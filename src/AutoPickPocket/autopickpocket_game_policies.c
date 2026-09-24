/* 12340 x86 game-thread policy. Isolated: NOT an installable game package.
 * The exact pinned EXE and native call ABIs are checked before any Lua access.
 * Game Lua only OBSERVES; never changes target, loot, movement or chat.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
static int observer(void);

/* Crash #5: WoW's Lua VM rejects an external DLL C function pointer.
 * Never register a native DLL callback in the client's Lua runtime.
 * Observe ordinary Lua events as nonce-scoped flags instead.
 */
static int same(PpGuid a,PpGuid b){
    return a.lo==b.lo && a.hi==b.hi;
}
/* UI_ERROR_MESSAGE has no destination GUID. It MUST remain an independent
 * observation in burst mode, not an invented failure of the newest NPC.
 * The Lua frame only writes flags; this game-thread poll writes bounded JSONL. */
/* Independent per-category counters: never treat a global UI event as the
 * server's response for an exact GUID. Preserve every observed error between
 * loader ticks even if several different UI messages arrive in one pulse. */
static void report_ui_observation(void){
    static unsigned long previous[6];
    static char previous_boot[48];
    static unsigned long wallet_previous,wallet_copper_previous;
    static const char *keys[6]={
        "W335PP_UI_R","W335PP_UI_L","W335PP_UI_S",
        "W335PP_UI_C","W335PP_UI_E","W335PP_UI_U"
    };
    char sequence[32],boot[48],wallet_seq[32],wallet_copper[32];
    unsigned long seen,delta;
    unsigned i;
    if(!is_owner())return;
    /* Reinitialize the Lua observer only if /reload removed its global. */
    if((!value("W335PP_BOOT",boot,sizeof(boot)) || !boot[0]) &&
       (!observer() || !value("W335PP_BOOT",boot,sizeof(boot))))return;
    if(boot[0] && strcmp(previous_boot,boot)!=0){
        memset(previous,0,sizeof(previous));
        wallet_previous=0u;wallet_copper_previous=0u;
        strcpy_s(previous_boot,sizeof(previous_boot),boot);
        PP335_LogLuaObserverEpoch();
    }
    if(value("W335PP_WALLET_COUNT",wallet_seq,sizeof(wallet_seq)) &&
       value("W335PP_WALLET_DELTA",wallet_copper,sizeof(wallet_copper))){
        unsigned long count=strtoul(wallet_seq,NULL,10);
        unsigned long copper=strtoul(wallet_copper,NULL,10);
        if(count!=wallet_previous){
            PP335_LogWalletObservation(
                (unsigned)(count>wallet_previous ?
                           count-wallet_previous : count),
                (uint32_t)(copper-wallet_copper_previous));
            wallet_previous=count;
            wallet_copper_previous=copper;
        }
    }
    for(i=0u;i<6u;++i){
        if(!value(keys[i],sequence,sizeof(sequence)))continue;
        seen=strtoul(sequence,NULL,10);
        if(!seen || seen==previous[i])continue;
        delta=seen>previous[i] ? seen-previous[i] : seen;
        previous[i]=seen;
        PP335_LogUiObservation(i,(unsigned)delta);
    }
}
static int observer(void){
    char flag[8];
    /* A GUID-scoped combat-log outcome is different from a UI error without a GUID. */
    return run("if not _G.W335PP_F then\n local f=CreateFrame('Frame')\n if f then\n  local function category(m)\n   if not m then return 'U' end\n   if SPELL_FAILED_TARGET_NO_POCKETS and m==SPELL_FAILED_TARGET_NO_POCKETS then return 'E' end\n   if SPELL_FAILED_OUT_OF_RANGE and m==SPELL_FAILED_OUT_OF_RANGE or ERR_OUT_OF_RANGE and m==ERR_OUT_OF_RANGE or SPELL_FAILED_TOO_CLOSE and m==SPELL_FAILED_TOO_CLOSE then return 'R' end\n   if SPELL_FAILED_LINE_OF_SIGHT and m==SPELL_FAILED_LINE_OF_SIGHT or SPELL_FAILED_VISION_OBSCURED and m==SPELL_FAILED_VISION_OBSCURED then return 'L' end\n   if SPELL_FAILED_ONLY_STEALTHED and m==SPELL_FAILED_ONLY_STEALTHED or SPELL_FAILED_NOT_STEALTHED and m==SPELL_FAILED_NOT_STEALTHED then return 'S' end\n   if SPELL_FAILED_NOT_READY and m==SPELL_FAILED_NOT_READY or SPELL_FAILED_SPELL_IN_PROGRESS and m==SPELL_FAILED_SPELL_IN_PROGRESS then return 'C' end\n   return 'U'\n  end\n  f:RegisterEvent('COMBAT_LOG_EVENT_UNFILTERED')\n  f:RegisterEvent('LOOT_OPENED')\n  f:RegisterEvent('PLAYER_MONEY')\n  f:RegisterEvent('UI_ERROR_MESSAGE')\n  f:SetScript('OnEvent',function(self,ev,...)\n   local n=_G.W335PP_N\n   if not n or n=='0' then return end\n   local t=GetTime()\n   if ev=='COMBAT_LOG_EVENT_UNFILTERED' then\n    local _,kind,src,_,_,dst,_,_,id=...\n    if id~=921 or not src or not dst or not UnitGUID('player') or string.upper(src)~=string.upper(UnitGUID('player')) then return end\n    local dg=string.upper(dst)\n    local rec=_G.W335PP_BURST and _G.W335PP_BURST[dg]\n    if not rec or t-rec.t>1.5 then return end\n    if kind=='SPELL_CAST_SUCCESS' then\n     rec.s='1'\n     if rec.n==n and dg==_G.W335PP_G then\n      _G.W335PP_S=n\n     end\n    elseif kind=='SPELL_CAST_FAILED' then\n     local why=category(select(12,...))\n     rec.f=why=='U' and 'F' or why\n     if rec.n==n and dg==_G.W335PP_G then\n      _G.W335PP_FAIL=n\n      _G.W335PP_FAIL_CODE=rec.f\n     end\n    end\n   elseif ev=='LOOT_OPENED' then\n    if _G.W335PP_T and t-_G.W335PP_T<=1.5 then _G.W335PP_O=n end\n   elseif ev=='PLAYER_MONEY' then\n    if GetMoney then\n     local balance=GetMoney()\n     local previous=_G.W335PP_WALLET_LAST or balance\n     if balance>previous then\n      _G.W335PP_WALLET_COUNT=tostring(tonumber(_G.W335PP_WALLET_COUNT or '0')+1)\n      _G.W335PP_WALLET_DELTA=tostring(tonumber(_G.W335PP_WALLET_DELTA or '0')+balance-previous)\n     end\n     _G.W335PP_WALLET_LAST=balance\n    end\n    if _G.W335PP_T and t-_G.W335PP_T<=0.4 and GetMoney and _G.W335PP_MB and _G.W335PP_MB>=0 then\n     local balance=GetMoney()\n     if balance>_G.W335PP_MB then\n      _G.W335PP_M=n\n      _G.W335PP_MD=tostring(balance-_G.W335PP_MB)\n     end\n    end\n   elseif ev=='UI_ERROR_MESSAGE' then\n    local why=category(select(1,...))\n    _G.W335PP_UI_SEQ=tostring(tonumber(_G.W335PP_UI_SEQ or '0')+1)\n    _G.W335PP_UI_KIND=why\n    local key='W335PP_UI_'..why\n    _G[key]=tostring(tonumber(_G[key] or '0')+1)\n    _G.W335PP_UI_TIME=t\n    if why=='R' and _G.W335PP_T and t-_G.W335PP_T<=0.8 and _G.W335PP_INFLIGHT=='1' then\n     _G.W335PP_UI_ACTIVE_RANGE=n\n    end\n   end\n  end)\n  _G.W335PP_BOOT=tostring(GetTime())\n  _G.W335PP_WALLET_LAST=GetMoney and GetMoney() or 0\n  _G.W335PP_F=f\n  _G.W335PP_INIT='1'\n end\nend") && value("W335PP_INIT",flag,sizeof(flag)) &&
           !strcmp(flag,"1");
}
static PpResult classify_failure(char code){
    switch(code){
    case 'E': return PP_RESULT_EMPTY;
    case 'R': return PP_RESULT_OUT_OF_RANGE;
    case 'L': return PP_RESULT_LINE_OF_SIGHT;
    case 'S': return PP_RESULT_NOT_STEALTHED;
    case 'C': return PP_RESULT_NOT_READY;
    default: return PP_RESULT_CAST_REJECTED;
    }
}
static void clear(void){
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
    report_ui_observation();
    (void)ctx;
    if(!run("local id=UnitGUID and UnitGUID('player');local area=GetCurrentMapAreaID and GetCurrentMapAreaID() or 0;local inst=0;if GetInstanceInfo then local _,_,_,_,_,_,_,i=GetInstanceInfo();inst=i or 0 end;_G.W335PP_WORLD=id and (id..':'..tostring(area)..':'..tostring(inst)) or ''") || !value("W335PP_WORLD",word,sizeof(word)))return 0u;
    next=hash_world(word);
    if(next!=world_id){world_id=next;clear();(void)run("_G.W335PP_BURST={};_G.W335PP_N='0';_G.W335PP_WALLET_LAST=GetMoney and GetMoney() or 0");}
    return next;
}
static int spell_usable(void *ctx,uint32_t spell_id){
    char flag[8];(void)ctx;
    if(spell_id!=921u || !observer() || !run("local ok=false;if UnitExists and UnitExists('player') and UnitClass and select(2,UnitClass('player'))=='ROGUE' and IsSpellKnown and IsSpellKnown(921) and IsStealthed and IsStealthed() and GetSpellInfo and IsUsableSpell then local name=GetSpellInfo(921);if name then local usable=IsUsableSpell(name);if usable then local s,d=GetSpellCooldown(921);ok=not(s and d and d>0 and s+d>GetTime()) end end end;_G.W335PP_READY=ok and '1' or '0'") ||
       !value("W335PP_READY",flag,sizeof(flag)))return 0;
    return !strcmp(flag,"1");
}
static int begin_attempt(void *ctx,PpGuid guid,uint32_t nonce){
    static unsigned burst_arms;
    char script[448],armed[32],want[32];int n;(void)ctx;
    if(!is_owner() || !nonce || !(guid.lo|guid.hi) || !observer())return 0;
    /* The previous GUID's combat-log record stays in BURST, independently
     * queryable by GUID and nonce. Arming the next packet never waits on loot. */
    clear();
    n=sprintf_s(script,sizeof(script),
      "_G.W335PP_N='%lu';_G.W335PP_G='0X%08lX%08lX';"
      "_G.W335PP_S='0';_G.W335PP_O='0';_G.W335PP_E='0';"
      "_G.W335PP_RANGE='0';_G.W335PP_M='0';_G.W335PP_UI_ACTIVE_RANGE='0';"
      "_G.W335PP_MB=GetMoney and GetMoney() or -1;"
      "_G.W335PP_FAIL='0';_G.W335PP_FAIL_CODE='0';_G.W335PP_T=GetTime();"
      "_G.W335PP_INFLIGHT='1';_G.W335PP_BURST=_G.W335PP_BURST or {};"
      "_G.W335PP_BURST[_G.W335PP_G]={n=_G.W335PP_N,t=GetTime(),s='0',f='0'};"
      "_G.W335PP_ARM=_G.W335PP_N",
      (unsigned long)nonce,(unsigned long)guid.hi,(unsigned long)guid.lo);
    if(n<=0 || n>=(int)sizeof(script) || !run(script))return 0;
    (void)sprintf_s(want,sizeof(want),"%lu",(unsigned long)nonce);
    if(!value("W335PP_ARM",armed,sizeof(armed)) || strcmp(armed,want))return 0;
    current_target=guid;current_attempt=nonce;started_ms=GetTickCount();active=1;
    /* Bounded pruning prevents an unbounded Lua GUID ledger on long routes. */
    if((++burst_arms%32u)==0u)
        (void)run("local b=_G.W335PP_BURST;if b then local t=GetTime();for k,v in pairs(b) do if t-v.t>2 then b[k]=nil end end end");
    return 1; /* arming is NOT successful theft */
}
static int movement_facing(void *ctx,float *facing){
    char out[48],*end=NULL;double angle;
    (void)ctx;
    if(!is_owner() || !facing ||
       !run("local p=UnitExists and UnitExists('player');local ok=p and not(UnitIsDeadOrGhost and UnitIsDeadOrGhost('player')) and not(UnitInVehicle and UnitInVehicle('player')) and not(UnitOnTaxi and UnitOnTaxi('player')) and not(IsFalling and IsFalling()) and not(IsSwimming and IsSwimming()) and not(IsFlying and IsFlying());local a=GetPlayerFacing and GetPlayerFacing();_G.W335PP_FACING=ok and type(a)=='number' and tostring(a) or ''") ||
       !value("W335PP_FACING",out,sizeof(out)) || !out[0])return 0;
    angle=strtod(out,&end);
    if(!end || *end || !(angle>=0.0 && angle<=6.283186))return 0;
    *facing=(float)angle;return 1;
}
static PpResult cast_result(void *ctx,PpGuid guid,uint32_t nonce){
    char query[380],q_nonce[32],q_success[8],q_failure[8],want[32];
    char loot[32],money[32];
    PpGuid source={0u,0u};
    int source_read,n,latest;
    uint32_t elapsed=0u;
    (void)ctx;
    if(!is_owner() || !nonce || !(guid.lo|guid.hi))
        return PP_RESULT_PENDING;
    report_ui_observation();
    latest=active && nonce==current_attempt && same(guid,current_target);
    if(latest)elapsed=(uint32_t)(GetTickCount()-started_ms);
    (void)sprintf_s(want,sizeof(want),"%lu",(unsigned long)nonce);
    /* The Lua COMBAT_LOG observer records each spell result under its exact
     * server destination GUID and cast nonce, even after newer packets send.
     * No global UI_ERROR_MESSAGE is ever attributed to a specific GUID. */
    n=sprintf_s(query,sizeof(query),
      "local r=_G.W335PP_BURST and _G.W335PP_BURST['0X%08lX%08lX'];"
      "_G.W335PP_QUERY_N=r and r.n or '0';"
      "_G.W335PP_QUERY_S=r and r.s or '0';"
      "_G.W335PP_QUERY_F=r and r.f or '0'",
      (unsigned long)guid.hi,(unsigned long)guid.lo);
    if(n<=0 || n>=(int)sizeof(query) || !run(query) ||
       !value("W335PP_QUERY_N",q_nonce,sizeof(q_nonce)) ||
       !value("W335PP_QUERY_S",q_success,sizeof(q_success)) ||
       !value("W335PP_QUERY_F",q_failure,sizeof(q_failure)) ||
       strcmp(q_nonce,want))return PP_RESULT_PENDING;
    if(q_failure[0]!='0' && q_failure[0])
        return classify_failure(q_failure[0]);
    /* A spell acknowledgement proves neither completed loot nor money. */
    /* Unscoped wallet/loot flags may belong to another pending GUID.
     * Only an exact native LOOT_SOURCE match can confirm the current GUID. */
    if(latest &&
       value("W335PP_O",loot,sizeof(loot)) &&
       value("W335PP_M",money,sizeof(money))){
        source_read=read_u32(LOOT_SOURCE,&source.lo) &&
                    read_u32(LOOT_SOURCE+4u,&source.hi);
        if(source_read && (source.lo|source.hi) && same(source,guid)){
            if(!strcmp(money,want) && !strcmp(loot,want))
                return PP_RESULT_MONEY_SUCCESS;
            if(!strcmp(loot,want))return PP_RESULT_CAST_ACK;
        }
    }
    if(q_success[0]=='1' && (!latest || elapsed>=80u))
        return PP_RESULT_CAST_ACK;
    return PP_RESULT_PENDING;
}
/* Do not cancel a newer cast or another GUID on delayed callbacks. */
static void end_attempt(void *ctx,PpGuid guid,uint32_t nonce){
    (void)ctx;
    if(is_owner() && active && nonce==current_attempt &&
       same(guid,current_target))clear();
}
PP335_EXPORT const Pp335Policy *__stdcall PP335_VerifiedPolicyV1(void){
    if(!owner)owner=GetCurrentThreadId();
    memset(&policy,0,sizeof(policy));
    policy.spell_usable=spell_usable;
    policy.movement_facing=movement_facing;
    policy.begin_attempt=begin_attempt;
    policy.cast_result=cast_result;
    policy.end_attempt=end_attempt;
    policy.world_token=world_token;
    return &policy;
}
