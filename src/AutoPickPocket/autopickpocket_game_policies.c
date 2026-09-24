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
static void report_ui_observation(void){
    static unsigned long last_seq;
    char sequence[32],code[8],line[320];
    unsigned long seen,delta;
    const char *reason;
    wchar_t dir[MAX_PATH],path[MAX_PATH],*slash;
    HANDLE file;DWORD written;int n;
    if(!is_owner() || !value("W335PP_UI_SEQ",sequence,sizeof(sequence)))
        return;
    seen=strtoul(sequence,NULL,10);
    if(!seen || seen==last_seq)return;
    delta=seen>last_seq ? seen-last_seq : 1u;
    last_seq=seen;
    if(!value("W335PP_UI_KIND",code,sizeof(code)))return;
    switch(code[0]){
    case 'R': reason="ui_out_of_range_unattributed";break;
    case 'L': reason="ui_line_of_sight_unattributed";break;
    case 'S': reason="ui_not_stealthed_unattributed";break;
    case 'C': reason="ui_not_ready_unattributed";break;
    case 'E': reason="ui_no_pockets_unattributed";break;
    default: reason="ui_other_error_unattributed";break;
    }
    if(!GetModuleFileNameW(NULL,dir,MAX_PATH))return;
    slash=wcsrchr(dir,L'\\');
    if(!slash)return;
    *slash=L'\0';
    if(swprintf_s(path,MAX_PATH,L"%ls\\.wow335_debug",dir)<0)return;
    if(!CreateDirectoryW(path,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS)
        return;
    if(swprintf_s(path,MAX_PATH,L"%ls\\.wow335_debug\\AutoPickPocket.jsonl",dir)<0)
        return;
    n=sprintf_s(line,sizeof(line),
        "{\"module\":\"AutoPickPocket\",\"ms\":%lu,"
        "\"event\":25,\"reason\":\"%s\","
        "\"attempt\":0,\"guid_lo\":0,\"guid_hi\":0,"
        "\"guid_attribution\":\"none\","
        "\"count_since_poll\":%lu,\"last_code_only\":true}\n",
        (unsigned long)GetTickCount(),reason,delta);
    if(n<=0 || n>=(int)sizeof(line))return;
    file=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE|
         FILE_SHARE_DELETE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return;
    (void)WriteFile(file,line,(DWORD)n,&written,NULL);
    CloseHandle(file);
}
static int observer(void){
    char flag[8];
    /* A GUID-scoped combat-log outcome is different from a UI error without a GUID. */
    return run("if not _G.W335PP_F then\n local f=CreateFrame('Frame')\n if f then\n  local function category(m)\n   if m==SPELL_FAILED_TARGET_NO_POCKETS then return 'E' end\n   if m==SPELL_FAILED_OUT_OF_RANGE or m==ERR_OUT_OF_RANGE or m==SPELL_FAILED_TOO_CLOSE then return 'R' end\n   if m==SPELL_FAILED_LINE_OF_SIGHT or m==SPELL_FAILED_VISION_OBSCURED then return 'L' end\n   if m==SPELL_FAILED_ONLY_STEALTHED or m==SPELL_FAILED_NOT_STEALTHED then return 'S' end\n   if m==SPELL_FAILED_NOT_READY or m==SPELL_FAILED_SPELL_IN_PROGRESS then return 'C' end\n   return 'U'\n  end\n  f:RegisterEvent('COMBAT_LOG_EVENT_UNFILTERED')\n  f:RegisterEvent('LOOT_OPENED')\n  f:RegisterEvent('PLAYER_MONEY')\n  f:RegisterEvent('UI_ERROR_MESSAGE')\n  f:SetScript('OnEvent',function(self,ev,...)\n   local n=_G.W335PP_N\n   if not n or n=='0' then return end\n   local t=GetTime()\n   if ev=='COMBAT_LOG_EVENT_UNFILTERED' then\n    local _,kind,src,_,_,dst,_,_,id=...\n    if id~=921 or not src or not dst or not UnitGUID('player') or string.upper(src)~=string.upper(UnitGUID('player')) then return end\n    local dg=string.upper(dst)\n    local rec=_G.W335PP_BURST and _G.W335PP_BURST[dg]\n    if not rec or t-rec.t>1.5 then return end\n    if kind=='SPELL_CAST_SUCCESS' then\n     rec.s='1'\n     if rec.n==n and dg==_G.W335PP_G then\n      _G.W335PP_S=n\n     end\n    elseif kind=='SPELL_CAST_FAILED' then\n     local why=category(select(12,...))\n     rec.f=why=='U' and 'F' or why\n     if rec.n==n and dg==_G.W335PP_G then\n      _G.W335PP_FAIL=n\n      _G.W335PP_FAIL_CODE=rec.f\n     end\n    end\n   elseif ev=='LOOT_OPENED' then\n    if _G.W335PP_T and t-_G.W335PP_T<=1.5 then _G.W335PP_O=n end\n   elseif ev=='PLAYER_MONEY' then\n    if _G.W335PP_T and t-_G.W335PP_T<=0.4 and GetMoney and _G.W335PP_MB and _G.W335PP_MB>=0 then\n     local balance=GetMoney()\n     if balance>_G.W335PP_MB then\n      _G.W335PP_M=n\n      _G.W335PP_MD=tostring(balance-_G.W335PP_MB)\n     end\n    end\n   elseif ev=='UI_ERROR_MESSAGE' then\n    local why=category(select(1,...))\n    _G.W335PP_UI_SEQ=tostring(tonumber(_G.W335PP_UI_SEQ or '0')+1)\n    _G.W335PP_UI_KIND=why\n    _G.W335PP_UI_TIME=t\n    if _G.W335PP_T and t-_G.W335PP_T<=0.8 and _G.W335PP_INFLIGHT=='1' and why~='U' then\n     if why=='E' then _G.W335PP_E=n\n     else\n      _G.W335PP_FAIL=n\n      _G.W335PP_FAIL_CODE=why\n     end\n    end\n   end\n  end)\n  _G.W335PP_F=f\n  _G.W335PP_INIT='1'\n end\nend") && value("W335PP_INIT",flag,sizeof(flag)) &&
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
    if(next!=world_id){world_id=next;clear();(void)run("_G.W335PP_BURST={};_G.W335PP_N='0'");}
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
    /* Core owns the timeout and explicitly cancels this exact nonce.
     * Keep a shared-bound guard only against concurrent active submissions. */
    if(active && (uint32_t)(GetTickCount()-started_ms)<PP_RESULT_TIMEOUT_MS)return 0;
    clear();
    n=sprintf_s(script,sizeof(script),
      "_G.W335PP_N='%lu';_G.W335PP_G='0X%08lX%08lX';"
      "_G.W335PP_S='0';_G.W335PP_O='0';_G.W335PP_E='0';"
      "_G.W335PP_RANGE='0';_G.W335PP_M='0';"
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
    return 1; /* arming is NOT successful theft */
}
static PpResult cast_result(void *ctx,PpGuid guid,uint32_t nonce){
    char sent[32],loot[32],empty[32],fail[32],range[32],money[32],fail_code[8],want[32];
    uint32_t elapsed;
    PpGuid source={0u,0u};
    int source_read;
    (void)ctx;
    report_ui_observation();
    if(!is_owner() || !active || nonce!=current_attempt ||
       !same(guid,current_target))return PP_RESULT_PENDING;
    elapsed=(uint32_t)(GetTickCount()-started_ms);
    if(elapsed>PP_RESULT_TIMEOUT_MS)return PP_RESULT_PENDING;
    (void)sprintf_s(want,sizeof(want),"%lu",(unsigned long)nonce);
    if(!value("W335PP_S",sent,sizeof(sent)) ||
       !value("W335PP_O",loot,sizeof(loot)) ||
       !value("W335PP_E",empty,sizeof(empty)) ||
       !value("W335PP_FAIL",fail,sizeof(fail)) ||
       !value("W335PP_FAIL_CODE",fail_code,sizeof(fail_code)) ||
       !value("W335PP_RANGE",range,sizeof(range)) ||
       !value("W335PP_M",money,sizeof(money)))
        return PP_RESULT_PENDING;
    /* An explicit nonce-scoped no-pockets rejection can arrive without
     * a SPELL_CAST_SUCCESS event. Do not retry this GUID in that case. */
    if(!strcmp(empty,want)){
        clear();return PP_RESULT_EMPTY;
    }
    if(!strcmp(fail,want) || !strcmp(range,want)){
        PpResult why=classify_failure(!strcmp(range,want) ? 'R' : fail_code[0]);
        clear();return why;
    }
    /* LOOT_OPENED is recorded by a Lua frame, NOT a DLL Lua callback.
     * Native autoloot can close the window before the next loader pulse.
     * When the native loot GUID can still be read, require an exact match.
     * A mismatching source must not confirm another NPC's attempt.
     */
    source_read=read_u32(LOOT_SOURCE,&source.lo) &&
                read_u32(LOOT_SOURCE+4u,&source.hi);
    /* PLAYER_MONEY alone is not GUID-scoped. Require same-attempt LOOT_OPENED
     * within 400ms; reject an available nonmatching source GUID. */
    if(!strcmp(money,want) && !strcmp(loot,want) &&
       (!source_read || !(source.lo|source.hi) || same(source,guid))){
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
    policy.begin_attempt=begin_attempt;
    policy.cast_result=cast_result;
    policy.end_attempt=end_attempt;
    policy.world_token=world_token;
    return &policy;
}
