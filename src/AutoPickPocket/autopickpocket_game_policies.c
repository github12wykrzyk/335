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
static int observer(void){
    char flag[8];
    return run("if not _G.W335PP_F then local f=CreateFrame('Frame');if f then f:RegisterEvent('COMBAT_LOG_EVENT_UNFILTERED');f:RegisterEvent('LOOT_OPENED');f:RegisterEvent('UI_ERROR_MESSAGE');f:SetScript('OnEvent',function(self,ev,...) local n=_G.W335PP_N;if not n or n=='0' then return end;if ev=='COMBAT_LOG_EVENT_UNFILTERED' then local _,kind,src,_,_,dst,_,_,id=...;if src and dst and UnitGUID('player') and string.upper(src)==string.upper(UnitGUID('player')) and id==921 and string.upper(dst)==_G.W335PP_G then if kind=='SPELL_CAST_SUCCESS' then _G.W335PP_S=n elseif kind=='SPELL_CAST_FAILED' then _G.W335PP_FAIL=n end end elseif ev=='LOOT_OPENED' and _G.W335PP_S==n then _G.W335PP_O=n elseif ev=='UI_ERROR_MESSAGE' and _G.W335PP_S==n and SPELL_FAILED_TARGET_NO_POCKETS and select(1,...)==SPELL_FAILED_TARGET_NO_POCKETS then _G.W335PP_E=n end end);_G.W335PP_F=f;_G.W335PP_INIT='1' end end") && value("W335PP_INIT",flag,sizeof(flag)) &&
           !strcmp(flag,"1");
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
    (void)ctx;
    if(!run("local id=UnitGUID and UnitGUID('player');local area=GetCurrentMapAreaID and GetCurrentMapAreaID() or 0;local inst=0;if GetInstanceInfo then local _,_,_,_,_,_,_,i=GetInstanceInfo();inst=i or 0 end;_G.W335PP_WORLD=id and (id..':'..tostring(area)..':'..tostring(inst)) or ''") || !value("W335PP_WORLD",word,sizeof(word)))return 0u;
    next=hash_world(word);
    if(next!=world_id){world_id=next;clear();}
    return next;
}
static int spell_usable(void *ctx,uint32_t spell_id){
    char flag[8];(void)ctx;
    if(spell_id!=921u || !observer() || !run("local ok=false;if UnitExists and UnitExists('player') and UnitClass and select(2,UnitClass('player'))=='ROGUE' and IsSpellKnown and IsSpellKnown(921) and IsStealthed and IsStealthed() and GetSpellInfo and IsUsableSpell then local name=GetSpellInfo(921);if name then local usable=IsUsableSpell(name);if usable then local s,d=GetSpellCooldown(921);ok=not(s and d and d>0 and s+d>GetTime()) end end end;_G.W335PP_READY=ok and '1' or '0'") ||
       !value("W335PP_READY",flag,sizeof(flag)))return 0;
    return !strcmp(flag,"1");
}
static int begin_attempt(void *ctx,PpGuid guid,uint32_t nonce){
    char script[448],armed[32],want[32];int n;(void)ctx;
    if(!is_owner() || !nonce || !(guid.lo|guid.hi) || !observer())return 0;
    if(active && (uint32_t)(GetTickCount()-started_ms)<1500u)return 0;
    clear();
    n=sprintf_s(script,sizeof(script),
      "_G.W335PP_N='%lu';_G.W335PP_G='0X%08lX%08lX';"
      "_G.W335PP_S='0';_G.W335PP_O='0';_G.W335PP_E='0';"
      "_G.W335PP_FAIL='0';_G.W335PP_ARM=_G.W335PP_N",
      (unsigned long)nonce,(unsigned long)guid.hi,(unsigned long)guid.lo);
    if(n<=0 || n>=(int)sizeof(script) || !run(script))return 0;
    (void)sprintf_s(want,sizeof(want),"%lu",(unsigned long)nonce);
    if(!value("W335PP_ARM",armed,sizeof(armed)) || strcmp(armed,want))return 0;
    current_target=guid;current_attempt=nonce;started_ms=GetTickCount();active=1;
    return 1; /* arming is NOT successful theft */
}
static int same(PpGuid a,PpGuid b){return a.lo==b.lo && a.hi==b.hi;}
static PpResult cast_result(void *ctx,PpGuid guid,uint32_t nonce){
    char sent[32],loot[32],empty[32],fail[32],want[32];
    PpGuid source={0u,0u};(void)ctx;
    if(!is_owner() || !active || nonce!=current_attempt ||
       !same(guid,current_target) ||
       (uint32_t)(GetTickCount()-started_ms)>1500u)return PP_RESULT_PENDING;
    (void)sprintf_s(want,sizeof(want),"%lu",(unsigned long)nonce);
    if(!value("W335PP_S",sent,sizeof(sent)) ||
       !value("W335PP_O",loot,sizeof(loot)) ||
       !value("W335PP_E",empty,sizeof(empty)) ||
       !value("W335PP_FAIL",fail,sizeof(fail)))
        return PP_RESULT_PENDING;
    /* A server SPELL_CAST_SUCCESS is NOT proof of stolen loot.
     * Require LOOT_OPENED AND the native loot-source GUID to match this NPC.
     * Native auto-loot may close too fast: then return PENDING, not fake success.
     */
    if(!strcmp(sent,want) && !strcmp(loot,want) &&
       read_u32(LOOT_SOURCE,&source.lo) &&
       read_u32(LOOT_SOURCE+4u,&source.hi) && same(source,guid)){
        clear();return PP_RESULT_SUCCESS;
    }
    if(!strcmp(sent,want) && !strcmp(empty,want)){
        clear();return PP_RESULT_EMPTY;
    }
    if(!strcmp(fail,want)){clear();return PP_RESULT_RETRYABLE;}
    return PP_RESULT_PENDING;
}
PP335_EXPORT const Pp335Policy *__stdcall PP335_VerifiedPolicyV1(void){
    if(!owner)owner=GetCurrentThreadId();
    memset(&policy,0,sizeof(policy));
    policy.spell_usable=spell_usable;
    policy.begin_attempt=begin_attempt;
    policy.cast_result=cast_result;
    policy.world_token=world_token;
    return &policy;
}
