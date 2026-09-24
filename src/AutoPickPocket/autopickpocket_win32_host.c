/* Exact-client native adapter, isolated TEST. The shared game-thread loader
 * must supply verified NPC, spell and cast-result policies before activation.
 * Does not install hooks, touch target, invoke AutoLoot, or chat.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <stdint.h>
#include "autopickpocket_win32_host.h"
#include "autopickpocket_packet_12340.h"
#pragma comment(lib,"Advapi32.lib")
#pragma comment(lib,"User32.lib")
#define PP_MIN_PTR 0x10000u
#define PP_MAX_PTR 0x7FFE0000u
#define PP_LOG_CAP 1048576u /* rotate complete JSONL records; no in-place erase */
#define PP_WINMSG_NAME "WoW335_AutoPickPocket_12340_GameThread_v1"
#define PP_CREATURE_TYPE_VA ((uintptr_t)0x0071F300u)
#define PP_CREATURE_UNDEAD 6u
#define PP_CREATURE_HUMANOID 7u
static Pp12340Adapter g_adapter;
static Pp335Policy g_policy;
static DWORD g_game_thread;
static int g_initialized;
static HMODULE g_self;
static UINT g_message;
static unsigned g_bind_attempted;
static DWORD g_last_bind_ms;
static unsigned g_desired_enable;
static unsigned g_pulse_running;
static uint32_t g_last_pulse_ms,g_pulse_delta_ms;
static uint32_t g_last_cast_ms,g_last_result_ms;
typedef struct {uint32_t nonce,started_ms;} PpHostFlight;
static PpHostFlight g_flights[PP_BURST_PENDING_CAP];
static uint32_t g_log_session;
static unsigned g_ui_category,g_ui_count;
static uint32_t g_wallet_delta_copper;
static uint8_t g_packet_cast_count;
/* No alternate transport: every accepted cast must use SendPacket. */
static uint32_t g_packet_nonce;
static PpGuid g_packet_guid;
static unsigned g_spoof_restore_failed;
static int valid_memory(const void *p,SIZE_T length) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t at=(uintptr_t)p;
    DWORD protect;
    if (!p || at<PP_MIN_PTR || at>=PP_MAX_PTR ||
        length>PP_MAX_PTR-at || !VirtualQuery(p,&m,sizeof(m)) ||
        m.State!=MEM_COMMIT || (m.Protect&PAGE_GUARD) ||
        (m.Protect&PAGE_NOACCESS)) return 0;
    protect=m.Protect&0xffu;
    if (protect==PAGE_EXECUTE || protect==PAGE_NOACCESS) return 0;
    return at+length<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static int read32(void *ctx,uintptr_t addr,uint32_t *out) {
    (void)ctx;
    if (!out || !valid_memory((const void *)addr,sizeof(*out))) return 0;
    __try { *out=*(volatile const uint32_t *)addr; return 1; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static int exe_digest(BYTE digest[32]) {
    wchar_t file[MAX_PATH];
    HCRYPTPROV provider=0;
    HCRYPTHASH hash=0;
    HANDLE h=INVALID_HANDLE_VALUE;
    BYTE buf[65536];
    DWORD len=0,digest_size=32;
    int good=0;
    if (!GetModuleFileNameW(NULL,file,MAX_PATH))return 0;
    h=CreateFileW(file,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|
        FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
    if(h==INVALID_HANDLE_VALUE ||
       !CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT) ||
       !CryptCreateHash(provider,CALG_SHA_256,0,0,&hash))goto done;
    for(;;) {
        if(!ReadFile(h,buf,sizeof(buf),&len,NULL))goto done;
        if(!len)break;
        if(!CryptHashData(hash,buf,len,0))goto done;
    }
    good=CryptGetHashParam(hash,HP_HASHVAL,digest,&digest_size,0) &&
         digest_size==32u;
done:
    if(hash)CryptDestroyHash(hash);
    if(provider)CryptReleaseContext(provider,0);
    if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);
    return good;
}
static int verify_hash(void *ctx,const char *expected) {
    BYTE digest[32];
    char hex[65];
    unsigned i;
    static const char chars[]="0123456789abcdef";
    (void)ctx;
    if (!expected || strcmp(expected,PP12340_CLIENT_SHA256) ||
        !exe_digest(digest))return 0;
    for(i=0u;i<32u;++i) {
        hex[i*2u]=chars[digest[i]>>4u];
        hex[i*2u+1u]=chars[digest[i]&15u];
    }
    hex[64]='\0';
    return strcmp(hex,expected)==0;
}
static int byte_match(uintptr_t at,const BYTE *expected,size_t length) {
    if(!valid_memory((const void *)at,length))return 0;
    __try {return memcmp((const void *)at,expected,length)==0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
typedef struct { DWORD pid, tid; int found; } GameWindowOwner;
static BOOL CALLBACK check_owner_window(HWND hwnd, LPARAM value) {
    GameWindowOwner *w=(GameWindowOwner *)value;
    DWORD pid=0u;
    DWORD thread;
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd,GW_OWNER)) return TRUE;
    thread=GetWindowThreadProcessId(hwnd,&pid);
    if (pid==w->pid && thread==w->tid) {
        w->found=1;
        return FALSE;
    }
    return TRUE;
}
static int current_thread_owns_game_window(void) {
    GameWindowOwner w;
    w.pid=GetCurrentProcessId();
    w.tid=GetCurrentThreadId();
    w.found=0;
    EnumWindows(check_owner_window,(LPARAM)&w);
    return w.found;
}
static int is_game_thread(void) {
    return g_initialized && g_game_thread==GetCurrentThreadId();
}

/* Static audit of exact pinned Wow.exe: 0x0071F300 has 26 direct E8 xrefs.
 * xref 0x004F7496 passes CGUnit_C in ECX, compares returned EAX with 12.
 * The method consumes no stack args and returns with C3 (thiscall).
 * No attempt is made to call other historical unverified NPC offsets.
 * Full-client SHA is checked at bind; exact code/xref bytes are checked
 * both at bind and before every native creature classification.
 */
static int verify_creature_type_abi(void) {
    static const BYTE body[]={
        0x80,0xB9,0xF4,0x09,0x00,0x00,0x00,0x74,0x04,0x33,0xC0,0xEB,0x0D,
        0x8B,0x81,0xD0,0x00,0x00,0x00,0x0F,0xB6,0x80,0xD3,0x01,0x00,0x00
    };
    static const BYTE caller[]={
        0x8B,0xCE,0xE8,0x65,0x7E,0x22,0x00,0x83,0xF8,0x0C
    };
    return byte_match(PP_CREATURE_TYPE_VA,body,sizeof(body)) &&
           byte_match((uintptr_t)0x004F7494u,caller,sizeof(caller));
}
static uint32_t native_creature_type(uintptr_t obj) {
    uintptr_t fn=PP_CREATURE_TYPE_VA;
    uint32_t creature_id=0u;
    /* A non-player CGUnit_C is already required by the adapter's object
     * type/GUID checks. Require the native method's first object field too. */
    if (!is_game_thread() || !valid_memory((const void *)obj,0x9F8u) ||
        !verify_creature_type_abi()) return 0u;
#if defined(_M_IX86)
    __try {
        __asm {
            mov ecx,obj
            call fn
            mov creature_id,eax
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { return 0u; }
#else
    return 0u;
#endif
    return creature_id;
}
/* The pinned 12340 client sends its own cast packet through
 * ClientServices::SendPacket(CDataStore*) at 006B0B50 (cdecl, 1 stack arg).
 * 0080B4E3 prepares the same one-argument call at 0080B4EE. Check the
 * caller and native callee on every bind and immediately before submission.
 * This is client packet dispatch, NOT raw socket send or 1.12 framing. */
#define PP335_SEND_VA ((uintptr_t)0x006B0B50u)
#define PP335_SESSION_VA ((uintptr_t)0x00C79CF4u)
#define PP335_CDATASTORE_VTABLE ((uint32_t)0x009E0E24u)
typedef struct {
    uint32_t vtable;
    uint8_t *buffer;
    uint32_t base;
    uint32_t alloc;
    uint32_t size;
    uint32_t read;
} Pp335OutboundDataStore;
typedef char Pp335_DataStore_x86_layout[(sizeof(Pp335OutboundDataStore)==24u)?1:-1];
static int packet_sender_abi(void) {
    static const BYTE send_head[]={
        0x55,0x8B,0xEC,0x8B,0x0D,0xF4,0x9C,0xC7,0x00,
        0x85,0xC9,0x74,0x0B,0x8B,0x45,0x08,0x50,0xE8
    };
    /* Exact 12340 PE: CMP dword ptr [ESI+0x534], imm8(5) uses
     * 83 BE ... 05, NOT 81 BE ... imm32. An incorrect signature silently
     * rejected PP335_BindOnGameThread before any Pick Pocket attempt. */
    static const BYTE native_head[]={
        0x55,0x8B,0xEC,0x56,0x8B,0xF1,0x83,0xBE,
        0x34,0x05,0x00,0x00,0x05
    };
    static const BYTE caller_head[]={
        0x8D,0x55,0xE4,0x52,0xC7,0x45,0xF8,0x00,
        0x00,0x00,0x00,0xE8
    };
    uint32_t displacement=0u;
    return byte_match(PP335_SEND_VA,send_head,sizeof(send_head)) &&
        byte_match((uintptr_t)0x00632B50u,native_head,sizeof(native_head)) &&
        byte_match((uintptr_t)0x0080B4E3u,caller_head,sizeof(caller_head)) &&
        read32(NULL,(uintptr_t)0x0080B4EFu,&displacement) &&
        (uint32_t)(0x0080B4F3u+displacement)==(uint32_t)PP335_SEND_VA;
}
static int packet_session_ready(void) {
    uint32_t session=0u,state=0u;
    return read32(NULL,PP335_SESSION_VA,&session) &&
        session>=PP_MIN_PTR && session<PP_MAX_PTR-0x534u &&
        read32(NULL,(uintptr_t)session+0x534u,&state) && state==5u;
}
static int verify_abi(void *ctx,uintptr_t spell,uintptr_t pos) {
    static const BYTE cast_prefix[]={
        0x55,0x8b,0xec,0xe8,0x48,0x5d,0xcc,0xff,
        0x68,0xa0,0x00,0x00,0x00,0x68,0x40,0x23,
        0x9f,0x00
    };
    static const BYTE pos_prefix[]={0x55,0x8b,0xec};
    /* Pinned-EXE audit: caller 0x0051041E calls 0x0080DA40 and
     * cleans five 32-bit parameters via 83 C4 14.
     * A binary prefix alone is never enough: verify the full EXE SHA first.
     */
    static const BYTE caller_postfix[]={0x83,0xc4,0x14};
    (void)ctx;
    return is_game_thread() && spell==PP12340_CAST_GUID_VA &&
        pos==PP12340_POSITION_VA &&
        byte_match(spell,cast_prefix,sizeof(cast_prefix)) &&
        byte_match(pos,pos_prefix,sizeof(pos_prefix)) &&
        byte_match((uintptr_t)0x00510423u,caller_postfix,sizeof(caller_postfix)) &&
        verify_creature_type_abi() && packet_sender_abi();
}
static uint32_t thread_id(void *ctx){(void)ctx;return GetCurrentThreadId();}
static uint32_t clock_ms(void *ctx){(void)ctx;return (uint32_t)GetTickCount();}
static int position(void *ctx,uintptr_t obj,float coords[3]) {
    uintptr_t fn=PP12340_POSITION_VA;
    (void)ctx;
    if(!is_game_thread() || !coords ||
       !valid_memory((const void *)obj,0x40u))return 0;
#if defined(_M_IX86)
    __try {
        __asm {
            mov ecx,obj
            push coords
            call fn
        }
        return _finite(coords[0]) && _finite(coords[1]) && _finite(coords[2]);
    }__except(EXCEPTION_EXECUTE_HANDLER){return 0;}
#else
    return 0;
#endif
}
static int eligible(void *ctx,uintptr_t obj,PpGuid guid) {
    uint32_t type;
    (void)ctx;
    if (!is_game_thread()) return 0;
    type=native_creature_type(obj);
    /* An eligible_NPC policy alone must never admit beasts, demons, players
     * or unknown creature types. Rechecked for every scan AND cast. */
    if (type!=PP_CREATURE_UNDEAD && type!=PP_CREATURE_HUMANOID)
        return 0;
    /* Optional independent provider may veto, but cannot bypass the native
     * type check. Unknown or conflicting reported values fail closed. */
    if (g_policy.creature_type &&
        g_policy.creature_type(g_policy.context,obj,guid)!=type) return 0;
    /* Only the user's chosen native undead/humanoid NPC filter is mandatory.
     * Additional hostility/eligibility policy, when present, may only veto. */
    return !g_policy.eligible_npc ||
           g_policy.eligible_npc(g_policy.context,obj,guid)==1;
}
static int usable(void *ctx,uint32_t spell_id) {
    (void)ctx;
    return is_game_thread() && spell_id==PP12340_SPELL_ID &&
        g_policy.spell_usable &&
        g_policy.spell_usable(g_policy.context,spell_id)==1;
}
static int cast_guid(void *ctx,uintptr_t va,uint32_t spell,PpGuid target,uint32_t attempt_id) {
    typedef void (__cdecl *send_fn)(Pp335OutboundDataStore *);
    Pp335OutboundDataStore packet;
    uint8_t bytes[PP335_CAST_PACKET_CAP];
    size_t count=0u;
    int submitted=0;
    (void)ctx;
    if(!is_game_thread() || va!=PP12340_CAST_GUID_VA ||
       spell!=PP335_PICK_POCKET_SPELL || (target.lo|target.hi)==0u ||
       !attempt_id || !g_policy.begin_attempt || !g_policy.end_attempt ||
       !g_policy.spell_usable ||
       g_policy.spell_usable(g_policy.context,spell)!=1)return 0;
    if(!packet_sender_abi() || !packet_session_ready())return 0;
    count=pp335_build_cast_packet(bytes,sizeof(bytes),
              ++g_packet_cast_count,target.lo,target.hi);
    if(count<15u || count>sizeof(bytes))return 0;
    memset(&packet,0,sizeof(packet));
    packet.vtable=PP335_CDATASTORE_VTABLE;
    packet.buffer=bytes;
    packet.alloc=(uint32_t)sizeof(bytes);
    packet.size=(uint32_t)count;
    if(g_policy.begin_attempt(g_policy.context,target,attempt_id)!=1)return 0;
    __try{
        ((send_fn)PP335_SEND_VA)(&packet);
        submitted=1;
    }__except(EXCEPTION_EXECUTE_HANDLER){submitted=0;}
    if(submitted){
        g_packet_nonce=attempt_id;
        g_packet_guid=target;
    }else g_policy.end_attempt(g_policy.context,target,attempt_id);
    return submitted;
}
static void event(void *ctx,PpEvent kind,PpGuid guid,uint32_t attempt_id);
/* Three ordered sends on the existing verified game-thread transport.
 * No client-memory movement; transient SERVER movement is not risk-free. */
static int send_heartbeat(PpGuid player,const float xyz[3],float facing,uint32_t when){
    typedef void (__cdecl *send_fn)(Pp335OutboundDataStore *);
    Pp335OutboundDataStore data;
    uint8_t raw[PP335_MOVE_PACKET_CAP];size_t count;
    if(!is_game_thread() || !packet_sender_abi() || !packet_session_ready())
        return 0;
    count=pp335_build_heartbeat(raw,sizeof(raw),player.lo,player.hi,when,xyz,facing);
    if(!count)return 0;
    memset(&data,0,sizeof(data));data.vtable=PP335_CDATASTORE_VTABLE;
    data.buffer=raw;data.alloc=(uint32_t)sizeof(raw);data.size=(uint32_t)count;
    __try{((send_fn)PP335_SEND_VA)(&data);return 1;}
    __except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
static int cast_guid_spoof(void *ctx,uintptr_t va,uint32_t spell,PpGuid guid,
    uint32_t nonce,PpGuid player,const float me[3],const float npc[3]){
    float dx,dy,dz,xy2,d2,xy,desired,ratio,spoof_xy[3],facing;
    uint32_t started;int cast_sent,restored;
    if(!is_game_thread() || g_spoof_restore_failed || !me || !npc ||
       !g_policy.movement_facing || va!=PP12340_CAST_GUID_VA ||
       spell!=PP12340_SPELL_ID || !nonce || !(player.lo|player.hi) ||
       !(guid.lo|guid.hi) ||
       !g_policy.movement_facing(g_policy.context,&facing) ||
       !_finite(facing) || facing<0.0f || facing>6.283186f)return 0;
    dx=npc[0]-me[0];dy=npc[1]-me[1];dz=npc[2]-me[2];
    xy2=dx*dx+dy*dy;d2=xy2+dz*dz;
    /* 10 yd TOTAL measured from unchanged REAL XYZ; Z is never spoofed. */
    if(!(d2>PP12340_REACH*PP12340_REACH &&
         d2<=PP12340_SPOOF_TOTAL_REACH*PP12340_SPOOF_TOTAL_REACH) ||
       !_finite(d2) || !_finite(xy2) || !_finite(dz) ||
       fabsf(dz)>0.75f)return 0;
    xy=sqrtf(xy2);desired=sqrtf(3.5f*3.5f-dz*dz);
    if(!(xy>desired) || !_finite(xy) || !_finite(desired))return 0;
    ratio=(xy-desired)/xy;
    spoof_xy[0]=me[0]+dx*ratio;spoof_xy[1]=me[1]+dy*ratio;spoof_xy[2]=me[2];
    if(!_finite(spoof_xy[0]) || !_finite(spoof_xy[1]))return 0;
    started=(uint32_t)GetTickCount();
    if(!send_heartbeat(player,spoof_xy,facing,started))return 0;
    /* Always attempt restoration, even if the spell send faults. */
    cast_sent=cast_guid(ctx,va,spell,guid,nonce);
    restored=send_heartbeat(player,me,facing,started+1u);
    if(!restored)g_spoof_restore_failed=1u; /* disable all future spoof */
    if(!cast_sent || !restored){
        if(cast_sent && g_policy.end_attempt)
            g_policy.end_attempt(g_policy.context,guid,nonce);
        return 0;
    }
    event(NULL,PP_EVENT_SPOOF_SEQUENCE,guid,nonce);
    return 1;
}
static PpResult result(void *ctx,PpGuid guid,uint32_t attempt_id) {
    PpResult outcome;
    (void)ctx;
    if(!is_game_thread() || !g_policy.cast_result)return PP_RESULT_PENDING;
    outcome=g_policy.cast_result(g_policy.context,guid,attempt_id);
    if(g_packet_nonce==attempt_id &&
       g_packet_guid.lo==guid.lo && g_packet_guid.hi==guid.hi &&
       outcome!=PP_RESULT_PENDING && outcome!=PP_RESULT_UI_RANGE_HINT){
        g_packet_nonce=0u;
    }
    return outcome;
}
static void end_attempt(void *ctx,PpGuid guid,uint32_t attempt_id) {
    unsigned i;
    (void)ctx;
    if(!is_game_thread())return;
    for(i=0u;i<PP_BURST_PENDING_CAP;++i)
        if(g_flights[i].nonce==attempt_id)g_flights[i].nonce=0u;
    /* Never use missing spell acknowledgement to change transport.
     * Release only this exact nonce; the next cast stays packet-only. */
    if(g_packet_nonce==attempt_id &&
       g_packet_guid.lo==guid.lo && g_packet_guid.hi==guid.hi)
        g_packet_nonce=0u;
    if(g_policy.end_attempt)
        g_policy.end_attempt(g_policy.context,guid,attempt_id);
}
static uint64_t world_token(void *ctx) {
    (void)ctx;
    if(!is_game_thread() || !g_policy.world_token)return 0u;
    return g_policy.world_token(g_policy.context);
}
static void event(void *ctx,PpEvent kind,PpGuid guid,uint32_t attempt_id) {
    wchar_t path[MAX_PATH],dir[MAX_PATH],older[MAX_PATH],newer[MAX_PATH],*slash;
    char line[768];
    const char *reason="attempt";
    DWORD ignored;
    HANDLE file;
    LARGE_INTEGER length;
    int n;
    unsigned i;
    uint32_t now,cast_gap=0u,result_wait=0u,next_wait=0u,queue_age=0u;
    (void)ctx;
    if(!is_game_thread())return;
    now=(uint32_t)GetTickCount();
    if(!g_log_session)g_log_session=now ^ (uint32_t)GetCurrentProcessId() ^
        (uint32_t)(uintptr_t)g_self;
    if(kind==PP_EVENT_CAST){
        unsigned k;
        if(g_last_cast_ms)cast_gap=(uint32_t)(now-g_last_cast_ms);
        if(g_last_result_ms)next_wait=(uint32_t)(now-g_last_result_ms);
        g_last_cast_ms=now;
        for(k=0u;k<PP_BURST_PENDING_CAP;++k)
            if(!g_flights[k].nonce || g_flights[k].nonce==attempt_id){
                g_flights[k].nonce=attempt_id;
                g_flights[k].started_ms=now;
                break;
            }
    } else if(kind==PP_EVENT_SUCCESS || kind==PP_EVENT_EMPTY ||
              kind==PP_EVENT_RETRY || kind==PP_EVENT_TIMEOUT ||
              kind==PP_EVENT_BURST_EXPIRE || kind==PP_EVENT_ACK_LOOT_UNKNOWN ||
              kind==PP_EVENT_BURST_RANGE_RELEASE || kind==PP_EVENT_INELIGIBLE ||
              kind==PP_EVENT_MONEY_SUCCESS || kind==PP_EVENT_OUT_OF_RANGE ||
              kind==PP_EVENT_LINE_OF_SIGHT || kind==PP_EVENT_NOT_STEALTHED ||
              kind==PP_EVENT_NOT_READY || kind==PP_EVENT_CAST_REJECTED ||
              kind==PP_EVENT_UI_RANGE_RECHECKED){
        unsigned k;
        for(k=0u;k<PP_BURST_PENDING_CAP;++k)
            if(g_flights[k].nonce && g_flights[k].nonce==attempt_id){
                result_wait=(uint32_t)(now-g_flights[k].started_ms);
                g_last_result_ms=now;
                g_flights[k].nonce=0u;
                break;
            }
    }
    if(g_adapter.engine.scan_started)
        queue_age=(uint32_t)(now-g_adapter.engine.queue_built_ms);
    if(!GetModuleFileNameW(NULL,dir,MAX_PATH))return;
    slash=wcsrchr(dir,L'\\');
    if(!slash)return;
    *slash=L'\0';
    if(swprintf_s(path,MAX_PATH,L"%ls\\.wow335_debug",dir)<0)return;
    if(!CreateDirectoryW(path,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS)
        return;
    if(swprintf_s(path,MAX_PATH,L"%ls\\.wow335_debug\\AutoPickPocket.packet.jsonl",dir)<0)
        return;
    file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|
          FILE_SHARE_DELETE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return;
    if(GetFileSizeEx(file,&length) && length.QuadPart>=PP_LOG_CAP) {
        CloseHandle(file);
        for(i=3u;i>0u;--i){
            if(i==1u){
                if(swprintf_s(older,MAX_PATH,
                    L"%ls\\.wow335_debug\\AutoPickPocket.packet.jsonl",dir)<0)return;
            }else if(swprintf_s(older,MAX_PATH,
                L"%ls\\.wow335_debug\\AutoPickPocket.packet.%u.jsonl",dir,i-1u)<0)
                return;
            if(swprintf_s(newer,MAX_PATH,
                L"%ls\\.wow335_debug\\AutoPickPocket.packet.%u.jsonl",dir,i)<0)
                return;
            (void)MoveFileExW(older,newer,
                MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
        }
        file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|
             FILE_SHARE_DELETE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
        if(file==INVALID_HANDLE_VALUE)return;
    }
    SetFilePointer(file,0,NULL,FILE_END);
    switch(kind) {
    case PP_EVENT_CAST: reason="cast_submitted";break;
    case PP_EVENT_SPOOF_SEQUENCE: reason="move_cast_restore_submitted_unverified";break;
    case PP_EVENT_SUCCESS: reason="result_claim_requires_loot_confirmation";break;
    case PP_EVENT_MONEY_SUCCESS: reason="wallet_loot_guid_correlated_signal";break;
    case PP_EVENT_CAST_ACK: reason="cast_ack_guid_correlated_not_theft";break;
    case PP_EVENT_ACK_LOOT_UNKNOWN: reason="cast_ack_loot_unconfirmed_guid_quarantined";break;
    case PP_EVENT_OUT_OF_RANGE: reason="out_of_range_guid_correlated";break;
    case PP_EVENT_UI_RANGE_RECHECKED: reason="ui_range_local_distance_confirmed_not_server_guid";break;
    case PP_EVENT_LINE_OF_SIGHT: reason="line_of_sight";break;
    case PP_EVENT_NOT_STEALTHED: reason="not_stealthed";break;
    case PP_EVENT_NOT_READY: reason="not_ready";break;
    case PP_EVENT_CAST_REJECTED: reason="cast_rejected_unknown";break;
    case PP_EVENT_PREFETCH_ONLY: reason="candidates_outside_cast_range";break;
    case PP_EVENT_LOCAL_RANGE_REJECT: reason="local_range_precheck_rejected";break;
    case PP_EVENT_EMPTY: reason="no_pockets";break;
    case PP_EVENT_RETRY: reason="temporary_failure";break;
    case PP_EVENT_TIMEOUT: reason="result_timeout";break;
    case PP_EVENT_BURST_EXPIRE: reason="burst_result_unknown_after_900ms";break;
    case PP_EVENT_BURST_RANGE_EXIT: reason="pending_guid_left_range_nonblocking";break;
    case PP_EVENT_BURST_RANGE_RELEASE: reason="range_exit_cancelled_pending_short_backoff";break;
    case PP_EVENT_WALLET_OBS: reason="wallet_delta_unattributed";break;
    case PP_EVENT_INELIGIBLE: reason="permanent_failure";break;
    case PP_EVENT_GAVE_UP: reason="retry_budget_exhausted";break;
    case PP_EVENT_NOT_CASTABLE: reason="not_castable";break;
    case PP_EVENT_NO_CANDIDATES: reason="no_candidates";break;
    case PP_EVENT_ALL_BLOCKED: reason="all_candidates_blocked";break;
    case PP_EVENT_WORLD_PAUSED: reason="world_unavailable";break;
    case PP_EVENT_WORLD_RESET: reason="world_changed";break;
    case PP_EVENT_ENABLED: reason="enabled";break;
    case PP_EVENT_DISABLED: reason="disabled";break;
    case PP_EVENT_RESET: reason="manual_reset";break;
    case PP_EVENT_LUA_EPOCH: reason="lua_observer_reinitialized";break;
    case PP_EVENT_UI_OBSERVATION:
        {static const char * const names[6]={
          "ui_out_of_range_unattributed","ui_line_of_sight_unattributed",
          "ui_not_stealthed_unattributed","ui_not_ready_unattributed",
          "ui_no_pockets_unattributed","ui_other_error_unattributed"};
         reason=names[g_ui_category<6u ? g_ui_category : 5u];}
        break;
    default:break;
    }
    n=sprintf_s(line,sizeof(line),
       "{\"module\":\"AutoPickPocket\",\"variant\":\"packet\",\"session_id\":\"%lu\",\"ms\":%lu,\"event\":%u,\"reason\":\"%s\",\"attempt\":%lu,\"guid_lo\":%lu,\"guid_hi\":%lu,\"scan_ms\":%lu,\"scan_candidates\":%lu,\"queue_depth\":%lu,\"queue_age_ms\":%lu,\"pulse_gap_ms\":%lu,\"cast_gap_ms\":%lu,\"result_wait_ms\":%lu,\"next_wait_ms\":%lu,\"selection_forward\":%u,\"candidates_ready\":%u,\"transport\":\"%s\",\"no_ack_count\":%u,\"count_since_poll\":%u,\"pending_results\":%u,\"wallet_delta_copper\":%lu}\n",
       (unsigned long)g_log_session,(unsigned long)now,(unsigned)kind,reason,
       (unsigned long)attempt_id,(unsigned long)guid.lo,(unsigned long)guid.hi,
       (unsigned long)g_adapter.last_scan_duration_ms,
       (unsigned long)g_adapter.last_scan_candidates,
       (unsigned long)g_adapter.engine.queue_count,
       (unsigned long)queue_age,(unsigned long)g_pulse_delta_ms,
       (unsigned long)cast_gap,(unsigned long)result_wait,
       (unsigned long)next_wait,
       g_adapter.engine.last_selection_forward,
       g_adapter.engine.last_selection_ready,
       "packet",0u,g_ui_count,
       g_adapter.engine.pending_count,(unsigned long)g_wallet_delta_copper);
    if(n>0)WriteFile(file,line,(DWORD)n,&ignored,NULL);
    CloseHandle(file);
    g_wallet_delta_copper=0u;
}
/* Called only from the verified game thread after the ordinary Lua frame
 * has been re-created. Uses the same session_id and rotated JSONL writer. */
void PP335_LogLuaObserverEpoch(void) {
    PpGuid none={0u,0u};
    event(NULL,PP_EVENT_LUA_EPOCH,none,0u);
}
/* The UI message has no target GUID. Preserve category/count on the
 * SAME rotated writer and native session as the cast lifecycle. */
void PP335_LogWalletObservation(unsigned count,uint32_t copper) {
    PpGuid none={0u,0u};
    if(!is_game_thread() || !count)return;
    g_ui_count=count;
    g_wallet_delta_copper=copper;
    event(NULL,PP_EVENT_WALLET_OBS,none,0u);
    g_ui_count=0u;
}
void PP335_LogUiObservation(unsigned category, unsigned count) {
    PpGuid none={0u,0u};
    if(!is_game_thread() || !count)return;
    g_ui_category=category;g_ui_count=count;
    event(NULL,PP_EVENT_UI_OBSERVATION,none,0u);
    g_ui_count=0u;
}
PP335_EXPORT int __stdcall PP335_BindOnGameThread(const Pp335Policy *policy) {
    Pp12340Host h;
    if(g_initialized || !current_thread_owns_game_window() ||
       !policy ||
       !policy->spell_usable || !policy->begin_attempt ||
       !policy->cast_result ||
       !policy->world_token)return 0;
    /* The authorized loader must call this from the actual WoW UI/window
     * thread. Never bind from DllMain or an updater worker.
     */
    g_game_thread=GetCurrentThreadId();
    g_initialized=1;
    /* Policy world_token executes Lua: check the pinned client and call ABI
     * first, not only inside pp12340_bind after world_token is invoked. */
    if (verify_hash(NULL,PP12340_CLIENT_SHA256)!=1 ||
        verify_abi(NULL,PP12340_CAST_GUID_VA,PP12340_POSITION_VA)!=1) {
        g_initialized=0;g_game_thread=0;return 0;
    }
    memset(&h,0,sizeof(h));
    h.verify_exe_sha256=verify_hash;
    h.verify_cast_abi=verify_abi;
    h.thread_id=thread_id;
    h.clock_ms=clock_ms;
    h.read_u32=read32;
    h.position=position;
    h.eligible_npc=eligible;
    h.spell_usable=usable;
    h.cast_guid=cast_guid;
    h.cast_guid_spoof=cast_guid_spoof;
    h.cast_result=result;
    h.end_attempt=end_attempt;
    h.world_token=world_token;
    h.event=event;
    g_policy=*policy;
    if(!pp12340_bind(&g_adapter,&h)) {
        memset(&g_policy,0,sizeof(g_policy));
        g_initialized=0;
        g_game_thread=0;
        return 0;
    }
    return 1;
}
PP335_EXPORT void __stdcall PP335_EnableOnGameThread(int enable) {
    if(is_game_thread())pp12340_enable(&g_adapter,enable);
}
PP335_EXPORT void __stdcall PP335_TickOnGameThread(uint32_t now_ms) {
    if(is_game_thread())pp12340_tick(&g_adapter,now_ms);
}
PP335_EXPORT void __stdcall PP335_ResetOnGameThread(void) {
    if(is_game_thread())pp12340_reset(&g_adapter);
}
PP335_EXPORT int __stdcall PP335_CommandOnGameThread(const char *arguments) {
    if (!is_game_thread()) return 0;
    return pp12340_command(&g_adapter,arguments);
}
/*
 * Work loader integration: W335_* is the ABI already used by its verified
 * dlls.txt chain. The trusted, concrete policy must be linked into THIS DLL
 * and export PP335_VerifiedPolicyV1; the old isolated adapter does not have
 * such a policy yet. Missing policy => W335_MessageId returns zero, so the
 * updater launcher refuses to start an accidentally registered incomplete
 * module, rather than running a silent no-op or guessing NPC types.
 */
typedef const Pp335Policy *(__stdcall *pp_verified_policy_fn)(void);
static pp_verified_policy_fn verified_policy(void) {
    if (!g_self) return NULL;
    { FARPROC proc=GetProcAddress(g_self,"PP335_VerifiedPolicyV1");
      if (!proc)proc=GetProcAddress(g_self,"_PP335_VerifiedPolicyV1@0");
      return (pp_verified_policy_fn)proc; }
}
PP335_EXPORT UINT WINAPI W335_MessageId(void) {
    if (!verified_policy()) return 0u;
    return RegisterWindowMessageA(PP_WINMSG_NAME);
}
static void pp_message(UINT message, WPARAM command) {
    pp_verified_policy_fn provider;
    const Pp335Policy *policy;
    if (!g_message) g_message=RegisterWindowMessageA(PP_WINMSG_NAME);
    if (!g_message || message!=g_message ||
        !current_thread_owns_game_window() || g_pulse_running) return;
    g_pulse_running=1u;
    if (command==0u) {
        g_desired_enable=0u;
        if (g_initialized) PP335_EnableOnGameThread(0);
    } else if (command==1u || command==2u) {
        if (command==1u) {
            g_desired_enable=1u;
            if (g_initialized) PP335_EnableOnGameThread(1);
        }
        /* The loader first hooks WoW at login; player GUID/world may not
         * exist yet. Retry slow, and enable ONCE after successful bind. */
        if (!g_initialized && (!g_bind_attempted ||
            (uint32_t)(GetTickCount()-g_last_bind_ms)>=1000u)) {
            g_bind_attempted=1u;
            g_last_bind_ms=GetTickCount();
            provider=verified_policy();
            policy=provider ? provider() : NULL;
            if (policy && policy->spell_usable && policy->begin_attempt &&
                policy->cast_result && policy->world_token &&
                PP335_BindOnGameThread(policy) && g_desired_enable)
                PP335_EnableOnGameThread(1);
        }
        /* Do not call PP335_EnableOnGameThread on each tick: pp_enable
         * intentionally clears active attempts and would lose all results. */
        if (g_initialized && command==2u) {
            uint32_t now=(uint32_t)GetTickCount();
            g_pulse_delta_ms=g_last_pulse_ms ?
                (uint32_t)(now-g_last_pulse_ms) : 0u;
            g_last_pulse_ms=now;
            PP335_TickOnGameThread(now);
        }
    }
    g_pulse_running=0u;
}
PP335_EXPORT LRESULT CALLBACK W335_HookProc(int code, WPARAM w, LPARAM l) {
    if (code>=0 && l) {
        const MSG *msg=(const MSG *)l;
        if (msg->message!=WM_QUIT) pp_message(msg->message,msg->wParam);
    }
    return CallNextHookEx(NULL,code,w,l);
}
PP335_EXPORT LRESULT CALLBACK W335_CallWndProc(int code, WPARAM w, LPARAM l) {
    if (code>=0 && l) {
        const CWPSTRUCT *msg=(const CWPSTRUCT *)l;
        if (msg->message!=WM_QUIT) pp_message(msg->message,msg->wParam);
    }
    return CallNextHookEx(NULL,code,w,l);
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved) {
    (void)reserved;
    if(reason==DLL_PROCESS_ATTACH) {
        g_self=module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
