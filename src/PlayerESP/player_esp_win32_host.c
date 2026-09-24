/* PlayerESP in-game GUI + read-only scanner on WoW window thread. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <float.h>
#include <math.h>
#include "player_esp_scanner.h"
#include "player_esp_camera.h"
#include "player_esp_d3d9.h"
#pragma comment(lib, "Advapi32.lib")
#define MESSAGE_NAME "WoW335_PlayerESP_12340_GameThread_v1"
#define GET_POS_VA ((uintptr_t)0x006E6F10u)
#define WORLD_FRAME_PTR ((uintptr_t)0x00B7436Cu)
#define ACTIVE_CAMERA_OFFSET ((uintptr_t)0x7E20u)
#define MIN_PTR ((uintptr_t)0x10000u)
#define MAX_PTR ((uintptr_t)0x7FFE0000u)
static Esp335Scanner g_scanner;
static DWORD g_thread, g_tick_ms, g_log_ms;
static UINT g_message;
static unsigned g_initialised, g_enabled, g_driving, g_live;
static uint32_t g_last_manager, g_scans_ok, g_scans_failed;
static uint64_t g_last_player_guid, g_epoch;
static HWND g_game_window;
static DWORD g_install_try_ms;
static unsigned g_camera_ok, g_camera_bad, g_markers_rendered;
static unsigned g_install_attempts, g_install_failures, g_bind_failed;
static CRITICAL_SECTION g_frame_lock;
static volatile LONG g_shared_ready, g_gui_open=1;
static volatile LONG g_ui_flags=ESP335_GUI_DEFAULT;
static Esp335Core g_frame_snapshot;
static Esp335CameraAxes g_frame_axes;
static DWORD g_frame_tick;
static unsigned g_frame_camera_valid, g_frame_scan_valid;


static int on_thread(void) { return g_thread && GetCurrentThreadId() == g_thread; }
static int readable(uintptr_t ptr, size_t bytes) {
    MEMORY_BASIC_INFORMATION m;
    DWORD p;
    if (ptr < MIN_PTR || ptr >= MAX_PTR || bytes > MAX_PTR - ptr ||
        !VirtualQuery((void *)ptr, &m, sizeof(m)) || m.State != MEM_COMMIT)
        return 0;
    p = m.Protect & 0xffu;
    if (m.Protect & PAGE_GUARD || m.Protect & PAGE_NOACCESS ||
        p == PAGE_NOACCESS || p == PAGE_EXECUTE) return 0;
    return ptr + bytes <= (uintptr_t)m.BaseAddress + m.RegionSize;
}
static int read32(void *ctx, uintptr_t addr, uint32_t *out) {
    (void)ctx;
    if (!on_thread() || !out || !readable(addr, 4u)) return 0;
    __try { *out = *(volatile const uint32_t *)addr; return 1; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static int get_guid(uintptr_t addr, uint64_t *out) {
    uint32_t low, high;
    if (!out || !read32(NULL, addr, &low) ||
        !read32(NULL, addr + 4u, &high)) return 0;
    *out = ((uint64_t)high << 32) | low;
    return 1;
}
static int check_client(void *ctx, const char *expected) {
    HCRYPTPROV provider = 0;
    HCRYPTHASH digest = 0;
    HANDLE file = INVALID_HANDLE_VALUE;
    wchar_t path[MAX_PATH];
    BYTE buffer[65536], result[32];
    DWORD n, size = 32u;
    char calculated[65];
    static const char hex[] = "0123456789abcdef";
    unsigned i;
    int ok = 0;
    (void)ctx;
    if (!on_thread() ||
        strcmp(expected, ESP335_EXACT_EXE_SHA256) != 0 ||
        !GetModuleFileNameW(NULL, path, MAX_PATH)) return 0;
    file = CreateFileW(path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (file == INVALID_HANDLE_VALUE ||
        !CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_AES,
                              CRYPT_VERIFYCONTEXT) ||
        !CryptCreateHash(provider, CALG_SHA_256, 0, 0, &digest))
        goto done;
    for (;;) {
        if (!ReadFile(file, buffer, sizeof(buffer), &n, NULL)) goto done;
        if (!n) break;
        if (!CryptHashData(digest, buffer, n, 0)) goto done;
    }
    if (!CryptGetHashParam(digest, HP_HASHVAL, result, &size, 0) ||
        size != 32u) goto done;
    for (i = 0; i < 32u; ++i) {
        calculated[2u*i] = hex[result[i] >> 4];
        calculated[2u*i+1u] = hex[result[i] & 15u];
    }
    calculated[64] = '\0';
    ok = strcmp(calculated, expected) == 0;
done:
    if (digest) CryptDestroyHash(digest);
    if (provider) CryptReleaseContext(provider, 0);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    return ok;
}
static int check_layout(void *ctx, uintptr_t connection, uintptr_t offset) {
    static const BYTE prolog[] = {0x55, 0x8b, 0xec};
    (void)ctx;
    if (!on_thread() || connection != ESP335_CONNECTION_VA ||
        offset != ESP335_MANAGER_OFFSET ||
        !readable(GET_POS_VA, sizeof(prolog))) return 0;
    /* Prolog is a necessary gate, not an independent full ABI verification. */
    __try {
        return memcmp((const void *)GET_POS_VA, prolog, sizeof(prolog)) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static uint32_t thread_id(void *ctx) {
    (void)ctx;
    return (uint32_t)GetCurrentThreadId();
}
static int position(void *ctx, uintptr_t object, Esp335Vec3 *coords) {
    uintptr_t fn = GET_POS_VA;
    (void)ctx;
    if (!on_thread() || !coords || !readable(object, 0x40u)) return 0;
    __try {
        __asm {
            mov ecx, object
            push coords
            call fn
        }
        return _finite(coords->x) && _finite(coords->y) && _finite(coords->z);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static int metadata(void *ctx, uintptr_t object, Esp335Player *player) {
    uint32_t desc, hp, max_hp;
    (void)ctx;
    if (!on_thread() || !player ||
        !read32(NULL, object + 0x08u, &desc) ||
        !read32(NULL, (uintptr_t)desc + 24u * 4u, &hp) ||
        !read32(NULL, (uintptr_t)desc + 32u * 4u, &max_hp) ||
        !max_hp || hp > max_hp) return 0;
    player->health = hp;
    player->max_health = max_hp;
    /* Unsupported classification remains UNKNOWN, never guessed. */
    player->faction = player->relation = player->bg_team = 0u;
    player->level = player->class_id = 0u;
    return 1;
}
static uint64_t epoch(void *ctx) {
    uint32_t conn, manager;
    uint64_t guid;
    (void)ctx;
    if (!on_thread() ||
        !read32(NULL, ESP335_CONNECTION_VA, &conn) ||
        conn < MIN_PTR || conn >= MAX_PTR ||
        !read32(NULL, (uintptr_t)conn + ESP335_MANAGER_OFFSET, &manager) ||
        manager < MIN_PTR || manager >= MAX_PTR ||
        !get_guid((uintptr_t)manager + ESP335_MGR_LOCAL_GUID, &guid) ||
        !guid) {
        g_live = 0u;
        return 0u;
    }
    if (!g_live || manager != g_last_manager || guid != g_last_player_guid) {
        ++g_epoch;
        if (!g_epoch) ++g_epoch;
    }
    g_live = 1u;
    g_last_manager = manager;
    g_last_player_guid = guid;
    return g_epoch;
}

/* All foreign camera reads use the same guarded, game-thread-only read32.
 * Camera layout originates from ConsoleXP's 3.3.5 reference and remains an
 * in-game validation candidate. Invalid basis/viewport => draw NOTHING.
 */
static int read_float(uintptr_t address, float *value) {
    uint32_t bits;
    if (!value || !read32(NULL,address,&bits)) return 0;
    memcpy(value,&bits,sizeof(bits));
    return _finite(*value) != 0;
}
static int read_vec3(uintptr_t address, Esp335Vec3 *value) {
    return value &&
           read_float(address,&value->x) &&
           read_float(address+4u,&value->y) &&
           read_float(address+8u,&value->z);
}
static int read_camera_axes(Esp335CameraAxes *a) {
    uint32_t worldframe, active;
    Esp335Camera temporary;
    if (!on_thread() || !a ||
        !read32(NULL,WORLD_FRAME_PTR,&worldframe) ||
        worldframe < MIN_PTR || worldframe >= MAX_PTR ||
        !read32(NULL,(uintptr_t)worldframe+ACTIVE_CAMERA_OFFSET,&active) ||
        active < MIN_PTR || active >= MAX_PTR) return 0;
    memset(a,0,sizeof(*a));
    if (!read_vec3((uintptr_t)active+0x08u,&a->eye) ||
        !read_vec3((uintptr_t)active+0x14u,&a->forward) ||
        !read_vec3((uintptr_t)active+0x20u,&a->up) ||
        !read_vec3((uintptr_t)active+0x2Cu,&a->right) ||
        !read_float((uintptr_t)active+0x38u,&a->near_clip) ||
        !read_float((uintptr_t)active+0x3Cu,&a->far_clip) ||
        !read_float((uintptr_t)active+0x40u,&a->fov_y) ||
        !read_float((uintptr_t)active+0x44u,&a->aspect))
        return 0;
    a->viewport_width=1024.f;
    a->viewport_height=768.f;
    /* Pure validation of candidate camera axes on the game thread.
     * Renderer will substitute the live device viewport independently. */
    return esp335_camera_build(a,&temporary);
}
static void draw_frame(IDirect3DDevice9 *device, void *user) {
    Esp335Core frame;
    Esp335CameraAxes axes;
    Esp335Camera camera;
    Esp335Filter filter;
    Esp335Label labels[ESP335_MAX_PLAYERS];
    D3DVIEWPORT9 viewport;
    size_t count;
    DWORD last_frame=0;
    unsigned scan_ok=0,camera_ok=0,players=0,npcs=0;
    unsigned flags=(unsigned)InterlockedCompareExchange(&g_ui_flags,0,0);
    int show=InterlockedCompareExchange(&g_gui_open,0,0)!=0;
    (void)user;
    if (!device || InterlockedCompareExchange(&g_shared_ready,0,0)==0)
        return;
    if (!TryEnterCriticalSection(&g_frame_lock)) return;
    frame=g_frame_snapshot;
    axes=g_frame_axes;
    last_frame=g_frame_tick;
    scan_ok=g_frame_scan_valid;
    camera_ok=g_frame_camera_valid;
    players=g_scanner.accepted_players;
    npcs=g_scanner.accepted_npcs;
    LeaveCriticalSection(&g_frame_lock);
    if ((flags & ESP335_GUI_ESP) && scan_ok && camera_ok &&
        frame.world_epoch && !frame.frame_open &&
        (DWORD)(GetTickCount()-last_frame)<250u &&
        SUCCEEDED(IDirect3DDevice9_GetViewport(device,&viewport)) &&
        viewport.Width>=64u && viewport.Height>=64u) {
        axes.viewport_x=(float)viewport.X;
        axes.viewport_y=(float)viewport.Y;
        axes.viewport_width=(float)viewport.Width;
        axes.viewport_height=(float)viewport.Height;
        axes.aspect=(float)viewport.Width/(float)viewport.Height;
        if (esp335_camera_build(&axes,&camera)) {
            memset(&filter,0,sizeof(filter));
            filter.show_all=(flags & ESP335_GUI_PLAYERS)!=0u;
            filter.show_players=(flags & (ESP335_GUI_PLAYERS|
                ESP335_GUI_HORDE|ESP335_GUI_ALLIANCE|
                ESP335_GUI_HOSTILE|ESP335_GUI_BG_ENEMY))!=0u;
            filter.factions=0u;
            if (flags & ESP335_GUI_HORDE) filter.factions|=ESP335_HORDE;
            if (flags & ESP335_GUI_ALLIANCE) filter.factions|=ESP335_ALLIANCE;
            filter.show_hostile=(flags & ESP335_GUI_HOSTILE)!=0u;
            filter.show_bg_opponents=(flags & ESP335_GUI_BG_ENEMY)!=0u;
            filter.show_npc=(flags & ESP335_GUI_NPC)!=0u;
            filter.show_npc_hostile=(flags & ESP335_GUI_NPC_ENEMY)!=0u;
            filter.show_unknown=(flags & ESP335_GUI_UNKNOWN)!=0u;
            filter.max_distance=120.f;
            count=esp335_labels(&frame,&camera,&filter,1,
                                labels,ESP335_MAX_PLAYERS);
            if (count) g_markers_rendered+=(unsigned)esp335_d3d9_draw_labels(
                                                    device,labels,count);
        }
    }
    /* GUI is independent of camera/target visibility: always render it on
     * the actual EndScene thread even if scanner or projection failed. */
    if (show) esp335_d3d9_draw_panel(device,flags,scan_ok,players,npcs,
                                    camera_ok,g_markers_rendered,
                                    esp335_d3d9_dropped());
}
static void try_renderer(void) {
    DWORD now=GetTickCount();
    if (!g_enabled || !on_thread() || !g_scanner.bound ||
        !g_game_window || (DWORD)(now-g_install_try_ms)<2500u) return;
    g_install_try_ms=now;
    if (!esp335_d3d9_installed()) {
        ++g_install_attempts;
        if (!esp335_d3d9_install(g_game_window,draw_frame,NULL))
            ++g_install_failures;
    }
}
static void input(MSG *msg, WPARAM remove_mode) {
    int index;
    unsigned flags,bit;
    float x,y;
    if (!msg || !g_enabled || !on_thread() || remove_mode!=PM_REMOVE ||
        !g_game_window || msg->hwnd!=g_game_window) return;
    if (msg->message==WM_KEYUP && msg->wParam==VK_INSERT) {
        InterlockedExchange(&g_gui_open,
            InterlockedCompareExchange(&g_gui_open,0,0) ? 0 : 1);
        msg->message=WM_NULL;
        return;
    }
    if (msg->message!=WM_LBUTTONUP ||
        !InterlockedCompareExchange(&g_gui_open,0,0)) return;
    x=(float)(short)LOWORD(msg->lParam);
    y=(float)(short)HIWORD(msg->lParam);
    index=esp335_d3d9_panel_hit(x,y);
    if (index<0) return;
    flags=(unsigned)InterlockedCompareExchange(&g_ui_flags,0,0);
    bit=1u << (unsigned)index;
    flags^=bit;
    /* Specific player filters deactivate ALL so the setting is effective.
     * Faction and hostility remain separate on mixed-faction BGs. */
    if (index>=2 && index<=5 && (flags & bit))
        flags&=~ESP335_GUI_PLAYERS;
    if (index==1 && (flags & ESP335_GUI_PLAYERS))
        flags&=~(ESP335_GUI_HORDE|ESP335_GUI_ALLIANCE|
                 ESP335_GUI_HOSTILE|ESP335_GUI_BG_ENEMY);
    InterlockedExchange(&g_ui_flags,(LONG)flags);
    /* Native game must not click through our checkbox into the world. */
    msg->message=WM_NULL;
}
static void write_diag(int scan_ok) {
    wchar_t path[MAX_PATH], *slash;
    HANDLE file;
    char line[640];
    DWORD written, now = GetTickCount();
    int length;
    if (!on_thread() || (DWORD)(now - g_log_ms) < 5000u) return;
    g_log_ms = now;
    if (!GetModuleFileNameW(NULL, path, MAX_PATH)) return;
    slash = wcsrchr(path, L'\\');
    if (!slash) return;
    *slash = L'\0';
    if (wcslen(path) + 40u >= MAX_PATH) return;
    wcscat_s(path, MAX_PATH, L"\\.wow335_debug");
    CreateDirectoryW(path, NULL);
    wcscat_s(path, MAX_PATH, L"\\PlayerESP.jsonl");
    file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    length = _snprintf_s(line, sizeof(line), _TRUNCATE,
        "{\"component\":\"PlayerESP\",\"scan_ok\":%u,\"players\":%u,"
        "\"epoch\":%I64u,\"seen_players\":%u,\"seen_npcs\":%u,"
        "\"accepted_players\":%u,\"accepted_npcs\":%u,"
        "\"position_failures\":%u,\"metadata_failures\":%u,"
        "\"scan_failures\":%u,\"render_installed\":%u,"
        "\"render_attempts\":%u,\"render_failures\":%u,"
        "\"render_frames\":%u,\"render_dropped\":%u,"
        "\"camera_ok\":%u,\"camera_bad\":%u,"
        "\"markers\":%u,\"ui_flags\":%u,\"bind_failed\":%u}\n",
        scan_ok ? 1u : 0u,
        scan_ok ? (unsigned)g_scanner.snapshot.count : 0u,
        (unsigned __int64)g_scanner.snapshot.world_epoch,
        g_scanner.seen_players,g_scanner.seen_npcs,
        g_scanner.accepted_players,g_scanner.accepted_npcs,
        g_scanner.position_failures,g_scanner.metadata_failures,
        g_scanner.scan_failures,esp335_d3d9_installed()?1u:0u,
        g_install_attempts,g_install_failures,
        esp335_d3d9_frames(),esp335_d3d9_dropped(),
        g_camera_ok,g_camera_bad,g_markers_rendered,
        (unsigned)InterlockedCompareExchange(&g_ui_flags,0,0),g_bind_failed);
    if (length > 0) WriteFile(file, line, (DWORD)length, &written, NULL);
    CloseHandle(file);
}
static void drive(void) {
    int scan_ok,camera_ok;
    Esp335CameraAxes axes;
    DWORD now;
    if (!g_enabled || !g_scanner.bound || !on_thread() || g_driving)
        return;
    now=GetTickCount();
    if ((DWORD)(now-g_tick_ms)<40u) return;
    g_driving=1u;
    g_tick_ms=now;
    scan_ok=esp335_scanner_collect(&g_scanner);
    camera_ok=read_camera_axes(&axes);
    if (scan_ok) ++g_scans_ok; else ++g_scans_failed;
    if (camera_ok) ++g_camera_ok; else ++g_camera_bad;
    if (InterlockedCompareExchange(&g_shared_ready,0,0)) {
        EnterCriticalSection(&g_frame_lock);
        if (scan_ok) g_frame_snapshot=g_scanner.snapshot;
        else esp335_reset(&g_frame_snapshot);
        if (camera_ok) g_frame_axes=axes;
        g_frame_camera_valid=camera_ok?1u:0u;
        g_frame_scan_valid=scan_ok?1u:0u;
        g_frame_tick=now;
        LeaveCriticalSection(&g_frame_lock);
    }
    try_renderer();
    write_diag(scan_ok);
    g_driving=0u;
}
__declspec(dllexport) UINT WINAPI W335_MessageId(void) {
    return RegisterWindowMessageA(MESSAGE_NAME);
}
static void control(UINT message, WPARAM command, HWND hwnd) {
    Esp335ScannerHost host;
    if (!g_message) g_message = RegisterWindowMessageA(MESSAGE_NAME);
    if (message != g_message) return;
    if (command == 0u) {
        g_enabled = 0u;
        esp335_d3d9_uninstall();
        esp335_reset(&g_scanner.snapshot);
        if (InterlockedCompareExchange(&g_shared_ready,0,0)) {
            EnterCriticalSection(&g_frame_lock);
            esp335_reset(&g_frame_snapshot);
            g_frame_scan_valid=g_frame_camera_valid=0;
            LeaveCriticalSection(&g_frame_lock);
        }
        return;
    }
    if (command != 1u && command != 2u) return;
    if (!g_initialised) {
        g_initialised = 1u;
        g_thread = GetCurrentThreadId();
        memset(&host, 0, sizeof(host));
        host.verify_client_sha256 = check_client;
        host.verify_layout = check_layout;
        host.thread_id = thread_id;
        host.read_u32 = read32;
        host.position = position;
        host.player_metadata = metadata;
        host.world_epoch = epoch;
        if (!esp335_scanner_bind(&g_scanner, &host)) {
            ++g_bind_failed;
            g_log_ms=GetTickCount()-6000u;
            write_diag(0);
            g_thread = 0u;
            return;
        }
    }
    if (g_scanner.bound && on_thread() && command == 1u) {
        if (!InterlockedCompareExchange(&g_shared_ready,0,0)) {
            if (!InitializeCriticalSectionAndSpinCount(&g_frame_lock,4000u)) return;
            InterlockedExchange(&g_shared_ready,1);
        }
        g_enabled = 1u;
        if (hwnd && IsWindow(hwnd)) {
            HWND root=GetAncestor(hwnd,GA_ROOT);
            DWORD tid=GetWindowThreadProcessId(root,NULL);
            if (tid==g_thread) g_game_window=root;
        }
        try_renderer();
    }
}
__declspec(dllexport) LRESULT CALLBACK W335_HookProc(int code, WPARAM w, LPARAM l) {
    MSG *message;
    if (code < 0 || !l) return CallNextHookEx(NULL, code, w, l);
    message = (MSG *)l;
    if (message->message != WM_QUIT) {
        control(message->message, message->wParam, message->hwnd);
        input(message,w);
        drive();
    }
    return CallNextHookEx(NULL, code, w, l);
}
__declspec(dllexport) LRESULT CALLBACK W335_CallWndProc(int code, WPARAM w, LPARAM l) {
    const CWPSTRUCT *message;
    if (code < 0 || !l) return CallNextHookEx(NULL, code, w, l);
    message = (const CWPSTRUCT *)l;
    if (message->message != WM_QUIT) {
        control(message->message, message->wParam, message->hwnd);
        drive();
    }
    return CallNextHookEx(NULL, code, w, l);
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(instance);
    return TRUE;
}
