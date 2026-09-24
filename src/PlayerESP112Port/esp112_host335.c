/* Clean 112-style ESP backend for exactly WoW 3.3.5a/12340 x86.
 * Code/path owner: github12wykrzyk/335. Reference: 112 parallel
 * WoWPlayerESP_v1_2_range_sweep.c project_world + label overlay.
 *
 * Frame pipeline: game-thread 12340 object scan -> exact 12340 native
 * WorldToScreen -> exact 12340 native DdcToNdc -> game client pixels
 * -> ClientToScreen -> Windows per-label layered GDI HWNDs.
 * NO UIParent coordinate transform, guessed FOV, LUA, dummy D3D9 hook,
 * second loader or alteration of game bytes.
 * TEST: runtime geometry requires in-game verification; do not promote.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <wchar.h>
#include <stdlib.h>
#include "../PlayerESP/player_esp_scanner.h"
#include "esp112_geometry.h"
#include "esp112_overlay.h"
#include "esp112_slots.h"
#include "esp112_motion.h"
#include "esp112_frame_hook.h"
#include "esp112_frame_draw.h"
#define ESP112_PROJECTION_INTERVAL_MS 16u
#define ESP112_OBJECT_SCAN_INTERVAL_MS 50u
#pragma comment(lib,"Advapi32.lib")
#pragma comment(lib,"User32.lib")
#pragma comment(lib,"Gdi32.lib")

#define ESP112_MSG "WoW335_PlayerESP_12340_GameThread_v1"
#define GET_POSITION_VA ((uintptr_t)0x006E6F10u)
#define WORLD_FRAME_GLOBAL ((uintptr_t)0x00B7436Cu)
#define WORLD_TO_SCREEN_VA ((uintptr_t)0x004F6D20u)
#define DDC_TO_NDC_VA ((uintptr_t)0x0047BFF0u)
#define CAMERA_OFFSET ((uintptr_t)0x7E20u)
#define MAX_DISTANCE 40.f
#define MIN_MEM ((uintptr_t)0x10000u)
#define MAX_MEM ((uintptr_t)0x7FFE0000u)
#define PROBE_INTERVAL 5000u

typedef struct {
    int x,y;
    unsigned hp,max_hp,kind,faction;
    uint64_t guid;
    Esp335Vec3 world_base;
    float distance,raw_x,raw_y,ndc_x,ndc_y;
} Esp112Candidate;
typedef struct {
    unsigned short_id;
    Esp335Vec3 world_base;
    int head_x,head_y,foot_x,foot_y;
    float head_raw_x,head_raw_y,foot_raw_x,foot_raw_y;
    float head_ui_x,head_ui_y,foot_ui_x,foot_ui_y;
} Esp112PairProbe;

static HINSTANCE g_instance;
static DWORD g_game_tid,g_last_scan,g_last_object_scan,g_last_log,g_last_bind,g_last_probe;
static unsigned g_snapshot_valid,g_projection_ticks;
static uint64_t g_snapshot_epoch;
static Esp112Slots g_slots;
static Esp112Motion g_motion[ESP112_SLOT_COUNT];
static UINT_PTR g_refresh_timer;
static DWORD g_last_frame_ms,g_last_frame_install_ms;
static unsigned g_frame_mode,g_frame_candidates,g_frame_renders;
static unsigned g_frame_install_attempts,g_frame_install_errors,g_frame_foreign_thread;
static unsigned g_frame_fallbacks,g_frame_drawn_labels;
static unsigned g_frame_draw_failures,g_frame_draw_fail_streak,g_frame_disabled;
static DWORD g_frame_retry_after_ms;
static unsigned g_timer_wakeups,g_debug_pairs;
static DWORD g_worst_tick_gap,g_last_cadence_report;
static unsigned g_last_cadence_ticks,g_last_cadence_scans,g_last_cadence_timers;
static HWND g_game_hwnd;
static UINT g_message;
static unsigned g_enabled,g_visible=1u,g_driving,g_initialised;
static unsigned g_scans,g_scan_errors,g_proj_ok,g_proj_error,g_native_gate_fail;
static unsigned g_window_fail,g_probe_ready;
static unsigned g_hook_calls,g_world_live,g_overlay_init_fail;
static unsigned g_overlay_binds,g_hash_rejects;
static uint32_t g_manager;
static uint64_t g_local_guid,g_epoch;
static Esp335Scanner g_scanner;
static Esp112Overlay g_overlay;
static float g_probe[7];
static int g_probe_x,g_probe_y;
static Esp112Viewport g_probe_view;
static Esp112PairProbe g_pairs[ESP112_DIAG_PAIRS];
static unsigned g_pair_count;

static int game_thread(void) {
    return g_game_tid && GetCurrentThreadId()==g_game_tid;
}
static void game_frame(IDirect3DDevice9 *device,void *user);
/* Thread-owned WM_TIMER: no second client hook, no game-window timer ID,
 * no Win32 timer callback into a DLL after unhook. Timer messages are low
 * priority and therefore do not promise hard 60 FPS under game load. */
static void stop_refresh_timer(void) {
    if (game_thread() && g_refresh_timer) {
        KillTimer(NULL,g_refresh_timer);
        g_refresh_timer=0u;
    }
}
static void ensure_refresh_timer(void) {
    if (game_thread() && g_enabled && g_visible && !g_frame_mode &&
        g_game_hwnd && !g_refresh_timer)
        g_refresh_timer=SetTimer(NULL,0u,ESP112_PROJECTION_INTERVAL_MS,NULL);
}
static void reset_guid_motion(void) {
    unsigned i;
    esp112_slots_reset(&g_slots);
    for (i=0u;i<ESP112_SLOT_COUNT;++i) esp112_motion_reset(&g_motion[i]);
}
static int readable(uintptr_t p,size_t len) {
    MEMORY_BASIC_INFORMATION info;
    DWORD protect;
    if (p<MIN_MEM || p>=MAX_MEM || len==0u || len>MAX_MEM-p ||
        !VirtualQuery((const void *)p,&info,sizeof(info)) ||
        info.State!=MEM_COMMIT) return 0;
    protect=info.Protect&0xffu;
    if (info.Protect&PAGE_GUARD || info.Protect&PAGE_NOACCESS ||
        protect==PAGE_NOACCESS || protect==PAGE_EXECUTE) return 0;
    return p+len<=(uintptr_t)info.BaseAddress+info.RegionSize;
}
static int read32(void *context,uintptr_t at,uint32_t *out) {
    (void)context;
    if (!game_thread() || !out || !readable(at,4u)) return 0;
    __try { *out=*(volatile const uint32_t *)at;return 1; }
    __except(EXCEPTION_EXECUTE_HANDLER){return 0;}
}
static int read_float(uintptr_t at,float *out) {
    uint32_t bits;
    if (!out || !read32(NULL,at,&bits)) return 0;
    memcpy(out,&bits,sizeof(bits));
    return _finite(*out)!=0;
}
static int read_guid(uintptr_t at,uint64_t *guid) {
    uint32_t lo,hi;
    if (!guid || !read32(NULL,at,&lo) || !read32(NULL,at+4u,&hi))
        return 0;
    *guid=((uint64_t)hi<<32)|lo;
    return 1;
}
static int verify_client(void *context,const char *expected) {
    HCRYPTPROV provider=0;
    HCRYPTHASH digest=0;
    HANDLE file=INVALID_HANDLE_VALUE;
    wchar_t path[MAX_PATH];
    BYTE data[65536],result[32];
    DWORD received,length=32u;
    char actual[65];
    static const char hex[]="0123456789abcdef";
    unsigned i;
    int ok=0;
    (void)context;
    if (!game_thread() || strcmp(expected,ESP335_EXACT_EXE_SHA256) ||
        !GetModuleFileNameW(NULL,path,MAX_PATH)) goto fail;
    file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|
        FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,NULL);
    if (file==INVALID_HANDLE_VALUE ||
        !CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,
                               CRYPT_VERIFYCONTEXT) ||
        !CryptCreateHash(provider,CALG_SHA_256,0u,0u,&digest))
        goto finish;
    for (;;) {
        if (!ReadFile(file,data,sizeof(data),&received,NULL)) goto finish;
        if (!received) break;
        if (!CryptHashData(digest,data,received,0u)) goto finish;
    }
    if (!CryptGetHashParam(digest,HP_HASHVAL,result,&length,0u) ||
        length!=32u) goto finish;
    for (i=0u;i<32u;++i) {
        actual[i*2u]=hex[result[i]>>4];
        actual[i*2u+1u]=hex[result[i]&15u];
    }
    actual[64]='\0';
    ok=strcmp(expected,actual)==0;
finish:
    if (digest) CryptDestroyHash(digest);
    if (provider) CryptReleaseContext(provider,0u);
    if (file!=INVALID_HANDLE_VALUE) CloseHandle(file);
fail:
    if (!ok) ++g_hash_rejects;
    return ok;
}
static int verify_layout(void *context,uintptr_t connection,uintptr_t off) {
    static const BYTE pos[]={0x55,0x8b,0xec};
    static const BYTE w2s[]={
        0x55,0x8b,0xec,0x83,0xec,0x24,0x8b,0x45,
        0x08,0xd9,0x00,0x56,0xd9,0x55,0xdc,0x8b,0xf1
    };
    /* 0x47BFF0 (12340, not 112): cdecl, four stack parameters, ret 0. */
    static const BYTE ddc[]={0x55,0x8b,0xec,0x8b,0x45,0x10,0x85,0xc0};
    (void)context;
    if (!game_thread() || connection!=ESP335_CONNECTION_VA ||
        off!=ESP335_MANAGER_OFFSET ||
        !readable(GET_POSITION_VA,sizeof(pos)) ||
        !readable(WORLD_TO_SCREEN_VA,sizeof(w2s)) ||
        !readable(DDC_TO_NDC_VA,sizeof(ddc))) goto reject;
    __try {
        if (memcmp((const void *)GET_POSITION_VA,pos,sizeof(pos)) ||
            memcmp((const void *)WORLD_TO_SCREEN_VA,w2s,sizeof(w2s)) ||
            memcmp((const void *)DDC_TO_NDC_VA,ddc,sizeof(ddc)))
            goto reject;
        return 1;
    } __except(EXCEPTION_EXECUTE_HANDLER) { goto reject; }
reject:
    ++g_native_gate_fail;
    return 0;
}
static uint32_t thread_id(void *context) {
    (void)context;
    return (uint32_t)GetCurrentThreadId();
}
static int object_position(void *context,uintptr_t object,Esp335Vec3 *xyz) {
    uintptr_t fn=GET_POSITION_VA;
    (void)context;
    if (!game_thread() || !xyz || !readable(object,0x40u)) return 0;
    __try {
        __asm {
            mov ecx,object
            push xyz
            mov eax,fn
            call eax
        }
        return _finite(xyz->x)&&_finite(xyz->y)&&_finite(xyz->z);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static unsigned faction_from_race(unsigned race) {
    switch (race) {
        case 1u:case 3u:case 4u:case 7u:case 11u:
            return ESP335_ALLIANCE;
        case 2u:case 5u:case 6u:case 8u:case 10u:
            return ESP335_HORDE;
        default:return 0u;
    }
}
static int player_metadata(void *context,uintptr_t object,
                            Esp335Player *player) {
    uint32_t desc,hp,max_hp,bytes0;
    (void)context;
    if (!game_thread() || !player ||
        !read32(NULL,object+0x08u,&desc) ||
        !read32(NULL,(uintptr_t)desc+24u*4u,&hp) ||
        !read32(NULL,(uintptr_t)desc+32u*4u,&max_hp) ||
        !max_hp || hp>max_hp) return 0;
    player->health=hp;player->max_health=max_hp;
    if (player->kind==ESP335_KIND_PLAYER &&
        read32(NULL,(uintptr_t)desc+23u*4u,&bytes0))
        player->faction=faction_from_race(bytes0&0xffu);
    /* Do not invent hostility, names, model bounds or BG affiliation. */
    return 1;
}
static uint64_t world_epoch(void *context) {
    uint32_t conn,mgr;
    uint64_t guid;
    (void)context;
    if (!game_thread() ||
        !read32(NULL,ESP335_CONNECTION_VA,&conn) ||
        conn<MIN_MEM || conn>=MAX_MEM ||
        !read32(NULL,(uintptr_t)conn+ESP335_MANAGER_OFFSET,&mgr) ||
        mgr<MIN_MEM || mgr>=MAX_MEM ||
        !read_guid((uintptr_t)mgr+ESP335_MGR_LOCAL_GUID,&guid) ||
        !guid) {
        g_world_live=0u;
        return 0u;
    }
    if (!g_world_live || mgr!=g_manager || guid!=g_local_guid) {
        ++g_epoch;
        if (!g_epoch) ++g_epoch;
    }
    g_world_live=1u;
    g_manager=mgr;
    g_local_guid=guid;
    return g_epoch;
}
static int get_world_frame(uint32_t *frame,Esp335Vec3 *eye) {
    uint32_t wf,cam;
    if (!game_thread() || !frame || !eye ||
        !read32(NULL,WORLD_FRAME_GLOBAL,&wf) ||
        wf<MIN_MEM || wf>=MAX_MEM ||
        !read32(NULL,(uintptr_t)wf+CAMERA_OFFSET,&cam) ||
        cam<MIN_MEM || cam>=MAX_MEM ||
        !read_float((uintptr_t)cam+0x08u,&eye->x) ||
        !read_float((uintptr_t)cam+0x0cu,&eye->y) ||
        !read_float((uintptr_t)cam+0x10u,&eye->z))
        return 0;
    *frame=wf;
    return 1;
}
static int get_viewport(Esp112Viewport *v) {
    RECT rect;
    POINT origin={0,0};
    if (!game_thread() || !v || !g_game_hwnd ||
        !IsWindow(g_game_hwnd) || !GetClientRect(g_game_hwnd,&rect) ||
        !ClientToScreen(g_game_hwnd,&origin)) return 0;
    v->client_x=0;v->client_y=0;
    v->screen_left=origin.x;v->screen_top=origin.y;
    v->width=rect.right-rect.left;
    v->height=rect.bottom-rect.top;
    return v->width>=64 && v->height>=64 && v->width<=16384 &&
           v->height<=16384;
}
static int native_project(uint32_t world_frame,Esp335Vec3 world,
                           const Esp112Viewport *view,Esp112Candidate *c) {
    float world_xyz[3]={world.x,world.y,world.z},screen_xyz[3]={0.f,0.f,0.f};
    float nx=-1.f,ny=-1.f,scale_x=0.f,scale_y=0.f;
    uint32_t flags=0u,success=0u;
    uintptr_t fn=WORLD_TO_SCREEN_VA;
    if (!game_thread() || !view || !c || !_finite(world_xyz[0]) ||
        !_finite(world_xyz[1]) || !_finite(world_xyz[2]) ||
        !readable((uintptr_t)world_frame,0x340u)) return 0;
    __try {
        __asm {
            mov ecx,world_frame
            lea eax,flags
            push eax
            lea eax,screen_xyz
            push eax
            lea eax,world_xyz
            push eax
            mov eax,fn
            call eax
            mov success,eax
        }
        if (!(success&0xffu)) return 0;
        if (!_finite(screen_xyz[0]) || !_finite(screen_xyz[1]) || !_finite(screen_xyz[2]))
            return 0;
        /* Verified disassembly of 0x004F6D20: instruction 0x004F6E45
         * ALREADY calls 0x0047BFF0 before writing screen_xyz[0..1].
         * This is a material ABI difference from 112/5875, which needs
         * a second native DDC call. Calling it again double-multiplies
         * screen X/Y by 0xAC0CB4/0xAC0CB8 (observed in real logs).
         * The already-converted coordinates are BOTTOM-UP UI units.
         * The portable viewport helper flips Y ONCE when converting to
         * top-left Windows client pixels (verified from paired 0D70).
         */
    } __except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
    if (!read_float((uintptr_t)0x00AC0CB4u,&scale_x) ||
        !read_float((uintptr_t)0x00AC0CB8u,&scale_y) ||
        !esp112_ui_to_client(screen_xyz[0],screen_xyz[1],scale_x,scale_y,
                             view,&c->x,&c->y)) return 0;
    nx=screen_xyz[0]/scale_x;
    ny=screen_xyz[1]/scale_y;
    c->raw_x=screen_xyz[0];c->raw_y=screen_xyz[1];
    c->ndc_x=nx;c->ndc_y=ny;
    return 1;
}
static int candidate_cmp(const void *left,const void *right) {
    const Esp112Candidate *a=(const Esp112Candidate *)left;
    const Esp112Candidate *b=(const Esp112Candidate *)right;
    if (a->distance<b->distance) return -1;
    if (a->distance>b->distance) return 1;
    if (a->guid<b->guid) return -1;
    if (a->guid>b->guid) return 1;
    return 0;
}
static void diagnostic(void) {
    wchar_t path[MAX_PATH],*slash;
    HANDLE file;
    DWORD written,now=GetTickCount();
    char line[1100];
    int n;
    if (!game_thread() || (DWORD)(now-g_last_log)<PROBE_INTERVAL)
        return;
    g_last_log=now;
    if (!GetModuleFileNameW(NULL,path,MAX_PATH)) return;
    slash=wcsrchr(path,L'\\');
    if (!slash || wcslen(path)+46u>=MAX_PATH) return;
    *slash=L'\0';
    wcscat_s(path,MAX_PATH,L"\\.wow335_debug");
    CreateDirectoryW(path,NULL);
    wcscat_s(path,MAX_PATH,L"\\PlayerESP.jsonl");
    file=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,
                     NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if (file==INVALID_HANDLE_VALUE) return;
    n=_snprintf_s(line,sizeof(line),_TRUNCATE,
        "{\"component\":\"PlayerESP\",\"backend\":\"112-gdi\","
        "\"schema\":1,\"source\":\"335-native-ddc\","
        "\"scan_ok\":%u,\"npcs\":%u,\"players\":%u,"
        "\"scans\":%u,\"scan_errors\":%u,\"projection_ok\":%u,"
        "\"projection_failed\":%u,\"labels\":%u,"
        "\"overlay_created\":%u,\"overlay_create_errors\":%u,"
        "\"client_hash_rejects\":%u,\"layout_rejects\":%u,"
        "\"hook_calls\":%u,\"insert_visible\":%u,\"projection_ticks\":%u}\n",
        g_scanner.snapshot.world_epoch?1u:0u,g_scanner.accepted_npcs,
        g_scanner.accepted_players,g_scans,g_scan_errors,
        g_proj_ok,g_proj_error,g_overlay.visible,g_overlay_binds,
        g_overlay_init_fail+g_window_fail,g_hash_rejects,g_native_gate_fail,
        g_hook_calls,g_visible,g_projection_ticks);
    if (n>0) WriteFile(file,line,(DWORD)n,&written,NULL);
    {
        DWORD elapsed=g_last_cadence_report?
            (DWORD)(now-g_last_cadence_report):PROBE_INTERVAL;
        unsigned cadence_hz=elapsed?
            (unsigned)((g_projection_ticks-g_last_cadence_ticks)*1000u/elapsed):0u;
        unsigned scans_hz=elapsed?
            (unsigned)((g_scans-g_last_cadence_scans)*1000u/elapsed):0u;
        unsigned timer_delta=g_timer_wakeups-g_last_cadence_timers;
        n=_snprintf_s(line,sizeof(line),_TRUNCATE,
            "{\"component\":\"PlayerESP\",\"backend\":\"112-gdi\","
            "\"probe\":\"cadence\",\"projection_hz\":%u,\"scan_hz\":%u,"
            "\"timer_wakeups\":%u,\"max_tick_gap_ms\":%lu,"
            "\"elapsed_ms\":%lu,\"debug_pairs\":%u}\n",
            cadence_hz,scans_hz,timer_delta,(unsigned long)g_worst_tick_gap,
            (unsigned long)elapsed,g_debug_pairs);
        if (n>0) WriteFile(file,line,(DWORD)n,&written,NULL);
        g_last_cadence_report=now;
        g_last_cadence_ticks=g_projection_ticks;
        g_last_cadence_scans=g_scans;
        g_last_cadence_timers=g_timer_wakeups;
        g_worst_tick_gap=0u;
    }
    n=_snprintf_s(line,sizeof(line),_TRUNCATE,
        "{\"component\":\"PlayerESP\",\"backend\":\"112-gdi+d3d9\","
        "\"probe\":\"frame_backend\",\"frame_active\":%u,"
        "\"hook_installed\":%u,\"verified_callbacks\":%u,"
        "\"rejected_callbacks\":%u,\"render_frames\":%u,"
        "\"rendered_labels\":%u,\"foreign_thread\":%u,"
        "\"install_attempts\":%u,\"install_errors\":%u,"
        "\"fallbacks\":%u,\"scene_failures\":%u,"
        "\"scene_submitted\":%u,\"draw_failures\":%u,"
        "\"frame_disabled\":%u}\n",
        g_frame_mode,esp112_frame_installed(),
        esp112_frame_callbacks(),esp112_frame_rejected(),
        g_frame_renders,g_frame_drawn_labels,g_frame_foreign_thread,
        g_frame_install_attempts,g_frame_install_errors,g_frame_fallbacks,
        esp112_frame_scene_failures(),esp112_frame_submitted(),
        g_frame_draw_failures,g_frame_disabled);
    if (n>0) WriteFile(file,line,(DWORD)n,&written,NULL);
    if (g_probe_ready) {
        n=_snprintf_s(line,sizeof(line),_TRUNCATE,
            "{\"component\":\"PlayerESP\",\"backend\":\"112-gdi\","
            "\"probe\":\"native_12340_ui_once\","
            "\"raw_ui_xy\":[%.5f,%.5f],\"viewport_normalized_xy\":[%.5f,%.5f],"
            "\"ui_scale_xy\":[%.5f,%.5f],"
            "\"client_xy\":[%d,%d],\"client_wh\":[%d,%d],"
            "\"game_screen_xy\":[%d,%d]}\n",
            g_probe[0],g_probe[1],g_probe[2],g_probe[3],
            g_probe[4],g_probe[5],g_probe_x,g_probe_y,
            g_probe_view.width,g_probe_view.height,
            g_probe_view.screen_left,g_probe_view.screen_top);
        if (n>0) WriteFile(file,line,(DWORD)n,&written,NULL);
        g_probe_ready=0u;
    }
    if (g_pair_count) {
        unsigned i;
        for (i=0u;i<g_pair_count;++i) {
            const Esp112PairProbe *p=&g_pairs[i];
            n=_snprintf_s(line,sizeof(line),_TRUNCATE,
                "{\"component\":\"PlayerESP\",\"backend\":\"112-gdi\","
                "\"probe\":\"paired_head_and_feet\",\"id\":\"%04X\","
                "\"world_base\":[%.3f,%.3f,%.3f],"
                "\"head_client_xy\":[%d,%d],\"feet_client_xy\":[%d,%d],"
                "\"head_raw_ui\":[%.5f,%.5f],\"feet_raw_ui\":[%.5f,%.5f],"
                "\"head_normalized\":[%.5f,%.5f],"
                "\"feet_normalized\":[%.5f,%.5f]}\n",
                p->short_id,p->world_base.x,p->world_base.y,p->world_base.z,
                p->head_x,p->head_y,p->foot_x,p->foot_y,
                p->head_raw_x,p->head_raw_y,p->foot_raw_x,p->foot_raw_y,
                p->head_ui_x,p->head_ui_y,p->foot_ui_x,p->foot_ui_y);
            if (n>0) WriteFile(file,line,(DWORD)n,&written,NULL);
        }
    }
    CloseHandle(file);
}
static void drive(IDirect3DDevice9 *device) {
    Esp112Candidate candidates[ESP335_MAX_PLAYERS];
    Esp112FrameLabel frame_labels[ESP112_MAX_LABELS];
    unsigned frame_count=0u;
    Esp112Viewport view;
    Esp335Vec3 eye;
    uint32_t world_frame;
    unsigned n=0u,drawn=0u,i;
    unsigned visible_mask=0u;
    DWORD now;
    HWND foreground;
    int scan_ok;
    if (!game_thread() || !g_enabled || g_driving) return;
    now=GetTickCount();
    if (device) {
        /* Exact frame callback: never wait for WM_TIMER or apply old GDI
         * smoothing to camera motion within this same render frame. */
        g_last_frame_ms=now;
    } else {
        if (g_frame_mode && (DWORD)(now-g_last_frame_ms)<=2000u) return;
        if (g_frame_mode) {
            g_frame_mode=0u;
            g_frame_candidates=0u;
            g_frame_retry_after_ms=now;
            ++g_frame_fallbacks;
            g_last_scan=0u;
        }
        if ((DWORD)(now-g_last_scan)<ESP112_PROJECTION_INTERVAL_MS) return;
    }
    if (g_last_scan) {
        DWORD gap=(DWORD)(now-g_last_scan);
        if (gap>g_worst_tick_gap) g_worst_tick_gap=gap;
    }
    g_last_scan=now;
    ++g_projection_ticks;
    g_driving=1u;
    foreground=GetForegroundWindow();
    if (!g_visible || !g_game_hwnd || IsIconic(g_game_hwnd) ||
        GetAncestor(foreground,GA_ROOT)!=g_game_hwnd) {
        stop_refresh_timer();
        esp112_overlay_hide_unused(&g_overlay,0u);
        goto done;
    }
    if (!device) ensure_refresh_timer();
    if (!g_scanner.bound) {
        g_snapshot_valid=0u;
        if (!g_last_bind || (DWORD)(now-g_last_bind)>=5000u) {
            Esp335ScannerHost host;
            memset(&host,0,sizeof(host));
            g_last_bind=now;
            host.verify_client_sha256=verify_client;
            host.verify_layout=verify_layout;
            host.thread_id=thread_id;
            host.read_u32=read32;
            host.position=object_position;
            host.player_metadata=player_metadata;
            host.world_epoch=world_epoch;
            (void)esp335_scanner_bind(&g_scanner,&host);
        }
        if (!g_scanner.bound) goto clear;
    }
    /* Read object list at 20 Hz, but reproject the CURRENT world frame at
     * up to 60 Hz. 50 ms camera-only freezes were visibly jerking labels. */
    if (!g_snapshot_valid || !g_last_object_scan ||
        (DWORD)(now-g_last_object_scan)>=ESP112_OBJECT_SCAN_INTERVAL_MS) {
        g_last_object_scan=now;
        scan_ok=esp335_scanner_collect(&g_scanner);
        if (!scan_ok) {
            ++g_scan_errors;
            g_snapshot_valid=0u;
            reset_guid_motion();
            goto clear;
        }
        ++g_scans;
        g_snapshot_valid=1u;
    }
    /* No cached snapshot may escape a relog/map/instance change. */
    if (!g_scanner.snapshot.world_epoch ||
        world_epoch(NULL)!=g_scanner.snapshot.world_epoch) {
        g_snapshot_valid=0u;
        reset_guid_motion();
        goto clear;
    }
    if (g_snapshot_epoch!=g_scanner.snapshot.world_epoch) {
        reset_guid_motion();
        g_snapshot_epoch=g_scanner.snapshot.world_epoch;
    }
    esp112_slots_next_frame(&g_slots);
    g_pair_count=0u;
    if (!get_world_frame(&world_frame,&eye) || !get_viewport(&view))
        goto clear;
    for (i=0u;i<g_scanner.snapshot.count && n<ESP335_MAX_PLAYERS;++i) {
        const Esp335Player *p=&g_scanner.snapshot.players[i];
        Esp335Vec3 head=p->position;
        Esp112Candidate candidate;
        float dx,dy,dz,distance;
        if (!p->guid || !p->max_health) continue;
        dx=head.x-eye.x;dy=head.y-eye.y;dz=head.z-eye.z;
        distance=sqrtf(dx*dx+dy*dy+dz*dz);
        if (!_finite(distance) || distance>MAX_DISTANCE) continue;
        memset(&candidate,0,sizeof(candidate));
        /* A 112-style head anchor is a provisional visual baseline, NOT
         * precise model-height data for every 12340 creature template. */
        head.z+=2.30f;
        if (!native_project(world_frame,head,&view,&candidate)) {
            ++g_proj_error;continue;
        }
        ++g_proj_ok;
        candidate.hp=p->health;candidate.max_hp=p->max_health;
        candidate.guid=p->guid;candidate.kind=p->kind;
        candidate.world_base=p->position;
        candidate.faction=p->faction;candidate.distance=distance;
        candidates[n++]=candidate;
        if (!g_probe_ready && (DWORD)(now-g_last_probe)>=PROBE_INTERVAL) {
            g_probe_ready=1u;g_last_probe=now;
            g_probe[0]=candidate.raw_x;g_probe[1]=candidate.raw_y;
            g_probe[2]=candidate.ndc_x;g_probe[3]=candidate.ndc_y;
            (void)read_float((uintptr_t)0x00AC0CB4u,&g_probe[4]);
            (void)read_float((uintptr_t)0x00AC0CB8u,&g_probe[5]);
            g_probe_x=candidate.x;g_probe_y=candidate.y;
            g_probe_view=view;
        }
    }
    qsort(candidates,n,sizeof(candidates[0]),candidate_cmp);
    if (!device) {
        if (!g_overlay.atom && !esp112_overlay_init(&g_overlay,g_instance)) {
            ++g_overlay_init_fail;goto clear;
        }
        if (g_overlay.atom && !g_overlay_binds) ++g_overlay_binds;
    }
    for (i=0u;i<n && drawn<ESP112_MAX_LABELS;++i) {
        char title[96];
        int left,top,slot,smooth_left,smooth_top;
        COLORREF color;
        const Esp112Candidate *c=&candidates[i];
        if (device) {
            Esp112FrameLabel *label;
            if (frame_count>=ESP112_MAX_LABELS) break;
            label=&frame_labels[frame_count++];
            memset(label,0,sizeof(*label));
            label->client_x=(float)c->x;
            label->client_y=(float)c->y;
            label->guid=c->guid;label->kind=c->kind;
            label->faction=c->faction;
            label->health=c->hp;label->max_health=c->max_hp;
            label->distance_yards=(unsigned)(c->distance+0.5f);
            ++drawn;
            continue;
        }
        if (!esp112_label_rect(c->x,c->y,&view,&left,&top))
            continue;
        slot=esp112_slots_reserve(&g_slots,c->guid,visible_mask);
        if (slot<0 || !esp112_motion_step(&g_motion[slot],c->guid,
                                          (float)left,(float)top,now,
                                          &smooth_left,&smooth_top))
            continue;
        if (c->kind==ESP335_KIND_NPC) {
            _snprintf_s(title,sizeof(title),_TRUNCATE,
                "NPC %04X %u yd",(unsigned)(c->guid&0xFFFFu),
                (unsigned)(c->distance+0.5f));
            color=RGB(80,205,250);
        } else {
            _snprintf_s(title,sizeof(title),_TRUNCATE,
                "PLAYER %04X %u yd",(unsigned)(c->guid&0xFFFFu),
                (unsigned)(c->distance+0.5f));
            color=c->faction==ESP335_HORDE?RGB(240,65,65):
                  c->faction==ESP335_ALLIANCE?RGB(90,150,255):
                  RGB(238,230,145);
        }
        if (!esp112_overlay_show(&g_overlay,(unsigned)slot,smooth_left,smooth_top,title,
                                  c->hp,c->max_hp,color)) {
            ++g_window_fail;continue;
        }
        /* Diagnostic pair: same object GUID, head label vs native
         * projection of the actual base. No guessed head correction is
         * applied to the base marker. Green cross MUST touch NPC feet if
         * object position and the native viewport transform are correct. */
        if (g_debug_pairs && (unsigned)slot<ESP112_DIAG_PAIRS) {
            Esp112Candidate foot={0};
            if (native_project(world_frame,c->world_base,&view,&foot)) {
                char id[12];
                _snprintf_s(id,sizeof(id),_TRUNCATE,"%04X",
                            (unsigned)(c->guid&0xFFFFu));
                if (!esp112_overlay_show_foot(&g_overlay,(unsigned)slot,
                        foot.x+view.screen_left,foot.y+view.screen_top,id))
                    ++g_window_fail;
                if (g_pair_count<ESP112_DIAG_PAIRS) {
                    Esp112PairProbe *entry=&g_pairs[g_pair_count++];
                    memset(entry,0,sizeof(*entry));
                    entry->short_id=(unsigned)(c->guid&0xFFFFu);
                    entry->world_base=c->world_base;
                    entry->head_x=c->x;entry->head_y=c->y;
                    entry->foot_x=foot.x;entry->foot_y=foot.y;
                    entry->head_raw_x=c->raw_x;entry->head_raw_y=c->raw_y;
                    entry->foot_raw_x=foot.raw_x;entry->foot_raw_y=foot.raw_y;
                    entry->head_ui_x=c->ndc_x;entry->head_ui_y=c->ndc_y;
                    entry->foot_ui_x=foot.ndc_x;entry->foot_ui_y=foot.ndc_y;
                }
            } else esp112_overlay_hide_foot(&g_overlay,(unsigned)slot);
        } else esp112_overlay_hide_foot(&g_overlay,(unsigned)slot);
        visible_mask|=1u<<(unsigned)slot;
        ++drawn;
    }
    if (device) {
        unsigned painted=esp112_frame_draw(device,frame_labels,frame_count);
        g_frame_drawn_labels+=painted;
        ++g_frame_renders;
        if (frame_count && !painted) {
            ++g_frame_draw_failures;
            if (++g_frame_draw_fail_streak>=3u) {
                /* Never oscillate between a broken D3D renderer and GDI
                 * on alternate frames. One failure latch per game session. */
                g_frame_disabled=1u;
                g_frame_mode=0u;
                ++g_frame_fallbacks;
                ensure_refresh_timer();
            }
        } else g_frame_draw_fail_streak=0u;
    } else esp112_overlay_finish_frame(&g_overlay,visible_mask);
    goto done;
clear:
    esp112_overlay_hide_unused(&g_overlay,0u);
done:
    diagnostic();
    g_driving=0u;
}
__declspec(dllexport) UINT WINAPI W335_MessageId(void) {
    return RegisterWindowMessageA(ESP112_MSG);
}
static void control(UINT message,WPARAM command,HWND hwnd) {
    if (!g_message) g_message=RegisterWindowMessageA(ESP112_MSG);
    if (message!=g_message) return;
    if (command==0u) {
        stop_refresh_timer();
        g_enabled=0u;
        g_frame_mode=0u;
        g_frame_candidates=0u;
        g_frame_disabled=1u;
        esp112_frame_uninstall();
        g_snapshot_valid=0u;
        g_snapshot_epoch=0u;
        reset_guid_motion();
        esp112_overlay_shutdown(&g_overlay);
        esp335_scanner_unbind(&g_scanner);
        return;
    }
    if (command!=1u && command!=2u) return;
    if (!g_initialised) {
        g_game_tid=GetCurrentThreadId();
        g_initialised=1u;
    }
    if (!game_thread()) return;
    if (hwnd && IsWindow(hwnd)) {
        HWND root=GetAncestor(hwnd,GA_ROOT);
        if (root && GetWindowThreadProcessId(root,NULL)==g_game_tid)
            g_game_hwnd=root;
    }
    g_enabled=1u;
    ensure_refresh_timer();
}
static void try_frame_install(void) {
    DWORD now;
    if (!game_thread() || !g_enabled || !g_game_hwnd ||
        esp112_frame_installed() || !GetModuleHandleW(L"d3d9.dll"))
        return;
    now=GetTickCount();
    if (g_last_frame_install_ms &&
        (DWORD)(now-g_last_frame_install_ms)<5000u) return;
    g_last_frame_install_ms=now;
    ++g_frame_install_attempts;
    if (!esp112_frame_install(g_game_hwnd,game_frame,NULL))
        ++g_frame_install_errors;
}
/* ONLY callback of the single PlayerESP D3D9 owner. This code runs
 * between a successful BeginScene/EndScene pair immediately before Present.
 * Fail closed on a separate render thread (native object manager access).
 * For an unknown D3D device we retain the previous working GDI backend. */
static void game_frame(IDirect3DDevice9 *device,void *user) {
    D3DVIEWPORT9 vp;
    Esp112Viewport view;
    DWORD now;
    (void)user;
    if (!game_thread()) { ++g_frame_foreign_thread;return; }
    if (!g_enabled || !g_visible || !g_game_hwnd || g_frame_disabled ||
        GetAncestor(GetForegroundWindow(),GA_ROOT)!=g_game_hwnd ||
        FAILED(IDirect3DDevice9_GetViewport(device,&vp)) ||
        !get_viewport(&view) || vp.X!=0u || vp.Y!=0u ||
        vp.Width!=(UINT)view.width || vp.Height!=(UINT)view.height)
        return;
    now=GetTickCount();
    /* Do not switch backends based on the dummy device or one stray frame. */
    if (!g_frame_mode && g_frame_retry_after_ms &&
        (DWORD)(now-g_frame_retry_after_ms)<10000u) return;
    if (!g_frame_mode && ++g_frame_candidates>=3u) {
        g_frame_mode=1u;
        stop_refresh_timer();
        esp112_overlay_hide_unused(&g_overlay,0u);
        g_last_scan=0u;
        reset_guid_motion();
    }
    if (!g_frame_mode) return;
    if (g_last_frame_ms && (DWORD)(now-g_last_frame_ms)<2u) return;
    drive(device);
}
static void check_insert(const MSG *message,WPARAM mode) {
    if (!g_enabled || !g_game_hwnd || !message || mode!=PM_REMOVE ||
        message->message!=WM_KEYUP || message->wParam!=VK_INSERT ||
        GetAncestor(message->hwnd,GA_ROOT)!=g_game_hwnd) return;
    if ((GetKeyState(VK_CONTROL)&0x8000) &&
        (GetKeyState(VK_SHIFT)&0x8000)) {
        unsigned i;
        g_debug_pairs=!g_debug_pairs;
        if (!g_debug_pairs)
            for (i=0u;i<ESP112_DIAG_PAIRS;++i)
                esp112_overlay_hide_foot(&g_overlay,i);
        return;
    }
    g_visible=!g_visible;
    if (!g_visible) {
        stop_refresh_timer();
        esp112_overlay_hide_unused(&g_overlay,0u);
    } else ensure_refresh_timer();
}
__declspec(dllexport) LRESULT CALLBACK W335_HookProc(int code,WPARAM w,LPARAM l) {
    if (code>=0 && l) {
        MSG *msg=(MSG *)l;
        ++g_hook_calls;
        if (g_refresh_timer && msg->message==WM_TIMER &&
            !msg->hwnd && msg->wParam==g_refresh_timer && w==PM_REMOVE)
            ++g_timer_wakeups;
        if (msg->message!=WM_QUIT) {
            control(msg->message,msg->wParam,msg->hwnd);
            if (game_thread()) {
                check_insert(msg,w);
                try_frame_install();
            }
            drive(NULL);
        }
    }
    return CallNextHookEx(NULL,code,w,l);
}
__declspec(dllexport) LRESULT CALLBACK W335_CallWndProc(int code,WPARAM w,LPARAM l) {
    if (code>=0 && l) {
        const CWPSTRUCT *msg=(const CWPSTRUCT *)l;
        if (msg->message!=WM_QUIT) {
            control(msg->message,msg->wParam,msg->hwnd);
            try_frame_install();
            drive(NULL);
        }
    }
    return CallNextHookEx(NULL,code,w,l);
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved) {
    (void)reserved;
    if (reason==DLL_PROCESS_ATTACH) {
        g_instance=module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

