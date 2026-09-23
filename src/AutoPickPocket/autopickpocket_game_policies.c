/* 12340 x86 game-thread policy. Isolated: NOT an installable game package.
 * The exact pinned EXE and native call ABIs are checked before any Lua access.
 * Game Lua only OBSERVES; never changes target, loot, movement or chat.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "autopickpocket_win32_host.h"
#define LUA_EXEC ((uintptr_t)0x00819210u)
#define LUA_STRING ((uintptr_t)0x00818010u)
#define LUA_STATE ((uintptr_t)0x00D3F78Cu)
#define LOOT_SOURCE ((uintptr_t)0x00BFA8D8u)
static DWORD owner;
static uint64_t world_id;
static PpGuid current_target;
static uint32_t current_attempt,started_ms;
static int active;
/* Fixed bounded native outstanding attempts; no shared-wallet attribution
 * while two GUIDs are in flight. All access occurs on the game/UI thread. */
typedef struct {PpGuid guid;uint32_t nonce,started;int valid;} NativePending;
static NativePending pending[PP_MAX_PENDING];
static int burst_overlap;
static Pp335Policy policy;
static int is_owner(void){return owner && GetCurrentThreadId()==owner;}
static int readable(uintptr_t at,size_t n){
    MEMORY_BASIC_INFORMATION m;
    if(at<0x10000u || at>=0x7FFE0000u || n>0x7FFE0000u-at ||
       !VirtualQuery((const void*)at,&m,sizeof(m)) ||
       m.State!=MEM_COMMIT || (m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))
        return 0;
    return at+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static int read_u32(uintptr_t addr,uint32_t *out){
    if(!out || !readable(addr,4u))return 0;
    __try{*out=*(volatile const uint32_t*)addr;return 1;}
    __except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
static int byte_match(uintptr_t addr,const BYTE *data,size_t n){
    if(!readable(addr,n))return 0;
    __try{return memcmp((const void*)addr,data,n)==0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
static int lua_abi(void){
    static const BYTE exec_head[]={0x55,0x8B,0xEC,0x51,0x83,0x05,0xA0,0x13,0xD4,0x00,0x01};
    static const BYTE get_head[]={0x55,0x8B,0xEC,0x8B,0x45,0x08,0x56,0x8B,0x35,0x8C,0xF7,0xD3,0x00,0x57,0x50,0x56};
    uint32_t L=0u;
    return is_owner() && read_u32(LUA_STATE,&L) && L>=0x10000u &&
           L<0x7FFE0000u && byte_match(LUA_EXEC,exec_head,sizeof(exec_head)) &&
           byte_match(LUA_STRING,get_head,sizeof(get_head));
}
static int run(const char *script){
    typedef void (__cdecl *fn)(const char*,const char*,int);
    if(!script || !lua_abi())return 0;
    __try{((fn)LUA_EXEC)(script,"AutoPickPocket335",0);return 1;}
    __except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
static int value(const char *key,char *out,size_t n){
    typedef int (__cdecl *fn)(const char*,const char**);
    const char *str=NULL;size_t i;int ok=0;
    if(!key || !out || n<2u || !lua_abi())return 0;
    out[0]='\0';
    __try{
        if(((fn)LUA_STRING)(key,&str) && str){
            for(i=0u;i+1u<n;++i){
                if(!readable((uintptr_t)(str+i),1u))break;
                out[i]=str[i];
                if(!str[i]){ok=1;break;}
            }
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){ok=0;}
    if(!ok)out[0]='\0';
    return ok;
}
static void clear(void);

/* Crash #5: WoW's Lua VM rejects an external DLL C function pointer.
 * Never register a native DLL callback in the client's Lua runtime.
 * Observe ordinary Lua events as nonce-scoped flags instead.
 */
static int same(PpGuid a,PpGuid b){
    return a.lo==b.lo && a.hi==b.hi;
}
static unsigned pending_size(void){unsigned i,n=0u;for(i=0u;i<PP_MAX_PENDING;++i)if(pending[i].valid)++n;return n;}
static int pending_index(PpGuid guid,uint32_t nonce){unsigned i;for(i=0u;i<PP_MAX_PENDING;++i)if(pending[i].valid && pending[i].nonce==nonce && same(pending[i].guid,guid))return (int)i;return -1;}
static void drop_pending(PpGuid guid,uint32_t nonce){
    int i=pending_index(guid,nonce),n;
    char script[270];
    if(i<0)return;
    pending[i].valid=0;
    /* Discard only the matching Lua GUID+nonce observer record, without
     * freeing a later attempt to the same NPC. Prevent unbounded Lua table
     * growth while retaining asynchronous results for other GUIDs. */
    n=sprintf_s(script,sizeof(script),
        "local p=_G.W335PP_BURST;local g='0X%08lX%08lX';"
        "if p and p[g] and p[g].n=='%lu' then p[g]=nil end",
        (unsigned long)guid.hi,(unsigned long)guid.lo,(unsigned long)nonce);
    if(n>0 && n<(int)sizeof(script))(void)run(script);
    if(!pending_size())burst_overlap=0;
}
static int observer(void){
    char flag[8];
    return run("if not _G.W335PP_F then local f=CreateFrame('Frame');if f then f:RegisterEvent('COMBAT_LOG_EVENT_UNFILTERED');f:RegisterEvent('LOOT_OPENED');f:RegisterEvent('PLAYER_MONEY');f:RegisterEvent('UI_ERROR_MESSAGE');f:SetScript('OnEvent',function(self,ev,...) local n=_G.W335PP_N;if not n or n=='0' then return end;if ev=='COMBAT_LOG_EVENT_UNFILTERED' then local _,kind,src,_,_,dst,_,_,id=...;local rec=_G.W335PP_BURST and dst and _G.W335PP_BURST[string.upper(dst)];if rec and GetTime()-rec.t<=0.4 and src and UnitGUID('player') and string.upper(src)==string.upper(UnitGUID('player')) and id==921 then if kind=='SPELL_CAST_SUCCESS' then rec.s='1' elseif kind=='SPELL_CAST_FAILED' then rec.f='1' end end;if _G.W335PP_T and GetTime()-_G.W335PP_T<=1.5 and src and dst and UnitGUID('player') and string.upper(src)==string.upper(UnitGUID('player')) and id==921 and string.upper(dst)==_G.W335PP_G then if kind=='SPELL_CAST_SUCCESS' then _G.W335PP_S=n;local t=UnitGUID and UnitGUID('target');if t and ClearTarget and string.upper(t)==_G.W335PP_G then ClearTarget() end elseif kind=='SPELL_CAST_FAILED' then _G.W335PP_FAIL=n end end elseif ev=='LOOT_OPENED' then if _G.W335PP_T and GetTime()-_G.W335PP_T<=1.5 then _G.W335PP_O=n end elseif ev=='PLAYER_MONEY' and _G.W335PP_T and GetMoney and _G.W335PP_MB and _G.W335PP_MB>=0 and GetTime()-_G.W335PP_T<=0.4 then local balance=GetMoney();if balance>_G.W335PP_MB then _G.W335PP_M=n;_G.W335PP_MD=tostring(balance-_G.W335PP_MB) end elseif ev=='UI_ERROR_MESSAGE' and _G.W335PP_T and GetTime()-_G.W335PP_T<=1.5 then local msg=select(1,...);if SPELL_FAILED_TARGET_NO_POCKETS and msg==SPELL_FAILED_TARGET_NO_POCKETS then _G.W335PP_E=n elseif (SPELL_FAILED_OUT_OF_RANGE and msg==SPELL_FAILED_OUT_OF_RANGE) or (ERR_OUT_OF_RANGE and msg==ERR_OUT_OF_RANGE) then _G.W335PP_RANGE=n end end end);_G.W335PP_F=f;_G.W335PP_INIT='1' end end") && value("W335PP_INIT",flag,sizeof(flag)) &&
           !strcmp(flag,"1");
}
static void clear(void){
    if(active)drop_pending(current_target,current_attempt);
    active=0;current_attempt=0u;started_ms=0u;
    memset(&current_target,0,sizeof(current_target));
}
static uint64_t hash_world(const char *s){
    uint64_t h=UINT64_C(14695981039346656037);unsigned i;
    if(!s || !s[0])return 0u;
    for(i=0u;s[i] && i<120u;++i){h^=(unsigned char)s[i];h*=UINT64_C(1099511628211);}
    return h;
}
static uint64_t world_token(void *ctx){
    char word[128];uint64_t next;
    (void)ctx;
    if(!run("local id=UnitGUID and UnitGUID('player');local area=GetCurrentMapAreaID and GetCurrentMapAreaID() or 0;local inst=0;if GetInstanceInfo then local _,_,_,_,_,_,_,i=GetInstanceInfo();inst=i or 0 end;_G.W335PP_WORLD=id and (id..':'..tostring(area)..':'..tostring(inst)) or ''") || !value("W335PP_WORLD",word,sizeof(word)))return 0u;
    next=hash_world(word);
    if(next!=world_id){world_id=next;clear();memset(pending,0,sizeof(pending));burst_overlap=0;(void)run("_G.W335PP_BURST={};_G.W335PP_N='0'");}
    return next;
}
static int spell_usable(void *ctx,uint32_t spell_id){
    char flag[8];(void)ctx;
    if(spell_id!=921u || !observer() || !run("local ok=false;if UnitExists and UnitExists('player') and UnitClass and select(2,UnitClass('player'))=='ROGUE' and IsSpellKnown and IsSpellKnown(921) and IsStealthed and IsStealthed() and GetSpellInfo and IsUsableSpell then local name=GetSpellInfo(921);if name then local usable=IsUsableSpell(name);if usable then local s,d=GetSpellCooldown(921);ok=not(s and d and d>0 and s+d>GetTime()) end end end;_G.W335PP_READY=ok and '1' or '0'") ||
       !value("W335PP_READY",flag,sizeof(flag)))return 0;
    return !strcmp(flag,"1");
}
static int begin_attempt(void *ctx,PpGuid guid,uint32_t nonce){
    char script[768],armed[32],want[32];int n;unsigned i;int free_slot=-1;(void)ctx;
    if(!is_owner() || !nonce || !(guid.lo|guid.hi) || !observer())return 0;
    /* Core owns the timeout and explicitly cancels this exact nonce.
     * Keep a shared-bound guard only against concurrent active submissions. */
    if(pending_size()>=PP_MAX_PENDING)return 0;
    for(i=0u;i<PP_MAX_PENDING;++i){
        if(pending[i].valid && same(pending[i].guid,guid))return 0;
        if(!pending[i].valid && free_slot<0)free_slot=(int)i;
    }
    if(free_slot<0)return 0;
    if(pending_size())burst_overlap=1;
    n=sprintf_s(script,sizeof(script),
      "_G.W335PP_N='%lu';_G.W335PP_G='0X%08lX%08lX';"
      "_G.W335PP_S='0';_G.W335PP_O='0';_G.W335PP_E='0';"
      "_G.W335PP_RANGE='0';_G.W335PP_M='0';_G.W335PP_MD='0';"
      "_G.W335PP_MB=GetMoney and GetMoney() or -1;"
      "_G.W335PP_FAIL='0';_G.W335PP_T=GetTime();"
      "_G.W335PP_BURST=_G.W335PP_BURST or {};"
      "_G.W335PP_BURST[_G.W335PP_G]={n=_G.W335PP_N,t=GetTime(),s='0',f='0'};"
      "_G.W335PP_ARM=_G.W335PP_N",
      (unsigned long)nonce,(unsigned long)guid.hi,(unsigned long)guid.lo);
    if(n<=0 || n>=(int)sizeof(script) || !run(script))return 0;
    (void)sprintf_s(want,sizeof(want),"%lu",(unsigned long)nonce);
    if(!value("W335PP_ARM",armed,sizeof(armed)) || strcmp(armed,want))return 0;
    current_target=guid;current_attempt=nonce;started_ms=GetTickCount();active=1;
    pending[free_slot].guid=guid;pending[free_slot].nonce=nonce;
    pending[free_slot].started=started_ms;pending[free_slot].valid=1;
    return 1; /* arming is NOT successful theft */
}
/* A spell event has an exact destination GUID in 12340 combat log.
 * Once attempts overlap, ONLY that GUID-tagged evidence can finish any
 * one of them. Unscoped PLAYER_MONEY/LOOT_OPENED/UI errors cannot identify
 * the recipient, and are intentionally ignored in burst mode. */
static PpResult burst_result(PpGuid guid,uint32_t nonce,uint32_t elapsed){
    char script[430],flag[8];
    int n;
    n=sprintf_s(script,sizeof(script),
       "_G.W335PP_BR='0';local r=_G.W335PP_BURST and "
       "_G.W335PP_BURST['0X%08lX%08lX'];"
       "if r and r.n=='%lu' and GetTime()-r.t<=0.4 then "
       "if r.f=='1' then _G.W335PP_BR='F' "
       "elseif r.s=='1' then _G.W335PP_BR='S' end end",
       (unsigned long)guid.hi,(unsigned long)guid.lo,(unsigned long)nonce);
    if(n<=0 || n>=(int)sizeof(script) ||
       !run(script) || !value("W335PP_BR",flag,sizeof(flag)))
        return PP_RESULT_PENDING;
    if(flag[0]=='F'){
        drop_pending(guid,nonce);
        if(active && nonce==current_attempt && same(guid,current_target))
            clear();
        return PP_RESULT_RETRYABLE;
    }
    if(flag[0]=='S' && elapsed>=80u){
        drop_pending(guid,nonce);
        if(active && nonce==current_attempt && same(guid,current_target))
            clear();
        return PP_RESULT_SUCCESS;
    }
    return PP_RESULT_PENDING;
}
static PpResult cast_result(void *ctx,PpGuid guid,uint32_t nonce){
    char sent[32],loot[32],empty[32],fail[32],range[32],money[32],want[32];
    uint32_t elapsed;
    PpGuid source={0u,0u};
    int source_read;
    (void)ctx;
    {int idx;
    if(!is_owner() || (idx=pending_index(guid,nonce))<0)return PP_RESULT_PENDING;
    elapsed=(uint32_t)(GetTickCount()-pending[idx].started);
    }
    /* The first solo cast may already have a wallet+loot signal when its
     * second neighbor becomes ready. Thereafter use GUID-tagged spell
     * events only; unscoped money/loot cannot identify one of 4 targets. */
    if(burst_overlap || !active || nonce!=current_attempt ||
       !same(guid,current_target))
        return burst_result(guid,nonce,elapsed);
    if(elapsed>PP_RESULT_TIMEOUT_MS)return PP_RESULT_PENDING;
    (void)sprintf_s(want,sizeof(want),"%lu",(unsigned long)nonce);
    if(!value("W335PP_S",sent,sizeof(sent)) ||
       !value("W335PP_O",loot,sizeof(loot)) ||
       !value("W335PP_E",empty,sizeof(empty)) ||
       !value("W335PP_FAIL",fail,sizeof(fail)) ||
       !value("W335PP_RANGE",range,sizeof(range)) ||
       !value("W335PP_M",money,sizeof(money)))
        return PP_RESULT_PENDING;
    /* An explicit nonce-scoped no-pockets rejection can arrive without
     * a SPELL_CAST_SUCCESS event. Do not retry this GUID in that case. */
    if(!strcmp(empty,want)){
        clear();return PP_RESULT_EMPTY;
    }
    if(!strcmp(fail,want) || !strcmp(range,want)){
        clear();return PP_RESULT_RETRYABLE;
    }
    /* LOOT_OPENED is recorded by a Lua frame, NOT a DLL Lua callback.
     * Native autoloot can close the window before the next loader pulse.
     * When the native loot GUID can still be read, require an exact match.
     * A mismatching source must not confirm another NPC's attempt.
     */
    source_read=read_u32(LOOT_SOURCE,&source.lo) &&
                read_u32(LOOT_SOURCE+4u,&source.hi);
    /* Experimental fast path: a strictly positive PLAYER_MONEY event after
     * the armed baseline AND LOOT_OPENED in the same 400 ms nonce window.
     * Explicit failures win. A readable different loot-source GUID vetoes
     * the money path: an unrelated wallet update is not proof of this NPC.
     * If the native loot source was already cleared, the combination is
     * indicative only; expose a distinct diagnostic event for game tests. */
    if(!strcmp(money,want) && !strcmp(loot,want) &&
       (!source_read || (source.lo|source.hi)==0u || same(source,guid))){
        clear();return PP_RESULT_MONEY_SUCCESS;
    }
    if(!strcmp(loot,want) && source_read && same(source,guid)){
        clear();return PP_RESULT_SUCCESS;
    }
    /* The server's spell-921 success event names the exact target GUID.
     * Give errors 80 ms after submission to arrive; this confirms the cast,
     * NOT that gold or items reached the inventory.
     */
    if(!strcmp(sent,want) && elapsed>=80u &&
       (!strcmp(loot,"0") || !source_read || same(source,guid))){
        clear();return PP_RESULT_SUCCESS;
    }
    return PP_RESULT_PENDING;
}
/* This callback runs synchronously on the same game/UI thread immediately
 * after native GUID casting returns; it cannot acknowledge spell success.
 * Guard BOTH nonce and current selected GUID. Failure to read Lua state
 * leaves target alone. Event fallback handles a later spell success. */
static void after_cast_submitted(void *ctx,PpGuid guid,uint32_t nonce){
    char script[320];
    int n;
    (void)ctx;
    if(!is_owner() || !active || !nonce || nonce!=current_attempt ||
       !same(guid,current_target))return;
    n=sprintf_s(script,sizeof(script),
      "if _G.W335PP_N=='%lu' and _G.W335PP_G=='0X%08lX%08lX' "
      "then local t=UnitGUID and UnitGUID('target');"
      "if t and ClearTarget and string.upper(t)==_G.W335PP_G "
      "then ClearTarget() end end",
      (unsigned long)nonce,(unsigned long)guid.hi,(unsigned long)guid.lo);
    if(n>0 && n<(int)sizeof(script)) (void)run(script);
}
/* Do not cancel a newer cast or another GUID on delayed callbacks. */
static void end_attempt(void *ctx,PpGuid guid,uint32_t nonce){
    (void)ctx;
    if(!is_owner())return;
    if(active && nonce==current_attempt && same(guid,current_target))clear();
    else drop_pending(guid,nonce);
}
PP335_EXPORT const Pp335Policy *__stdcall PP335_VerifiedPolicyV1(void){
    if(!owner)owner=GetCurrentThreadId();
    memset(&policy,0,sizeof(policy));
    policy.spell_usable=spell_usable;
    policy.begin_attempt=begin_attempt;
    policy.cast_result=cast_result;
    policy.after_cast_submitted=after_cast_submitted;
    policy.end_attempt=end_attempt;
    policy.world_token=world_token;
    return &policy;
}
