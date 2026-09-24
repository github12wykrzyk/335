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
#include "player_esp_lua.h"
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
static unsigned g_hook_calls, g_insert_events, g_gui_toggles, g_init_attempts;
static unsigned g_sha_rejects, g_layout_rejects;
static DWORD g_bind_try_ms;
static unsigned g_prev_insert_down, g_key_toggled, g_insert_polls;
static CRITICAL_SECTION g_frame_lock;
static volatile LONG g_shared_ready, g_gui_open=1;
static volatile LONG g_ui_flags=ESP335_GUI_DEFAULT;
static Esp335Core g_frame_snapshot;
static Esp335CameraAxes g_frame_axes;
static DWORD g_frame_tick;
static unsigned g_frame_camera_valid, g_frame_scan_valid;
static unsigned g_frame_players, g_frame_npcs;
static Esp335LuaGate g_lua_gate;
static DWORD g_lua_init_tick, g_lua_update_tick;
static unsigned g_lua_init_attempts,g_lua_init_ok,g_lua_updates,g_lua_update_errors,g_lua_gate_missing;
static unsigned g_lua_ready;
static int g_lua_last_visibility=-1;


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
        !GetModuleFileNameW(NULL, path, MAX_PATH)) { ++g_sha_rejects; return 0; }
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
    if (!ok) ++g_sha_rejects;
    return ok;
}
static int check_layout(void *ctx, uintptr_t connection, uintptr_t offset) {
    static const BYTE prolog[] = {0x55, 0x8b, 0xec};
    (void)ctx;
    if (!on_thread() || connection != ESP335_CONNECTION_VA ||
        offset != ESP335_MANAGER_OFFSET ||
        !readable(GET_POS_VA, sizeof(prolog))) {
        ++g_layout_rejects;
        return 0;
    }
    /* Prolog is a necessary gate, not an independent full ABI verification. */
    __try {
        int ok=memcmp((const void *)GET_POS_VA, prolog, sizeof(prolog)) == 0;
        if (!ok) ++g_layout_rejects;
        return ok;
    } __except (EXCEPTION_EXECUTE_HANDLER) { ++g_layout_rejects; return 0; }
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
static unsigned faction_from_race(unsigned race) {
    switch (race) {
        case 1u: case 3u: case 4u: case 7u: case 11u:
            return ESP335_ALLIANCE;
        case 2u: case 5u: case 6u: case 8u: case 10u:
            return ESP335_HORDE;
        default: return 0u; /* custom race: unknown, not guessed */
    }
}
static int metadata(void *ctx, uintptr_t object, Esp335Player *player) {
    uint32_t desc, hp, max_hp, bytes0=0u, level=0u, template_id=0u;
    (void)ctx;
    if (!on_thread() || !player ||
        !read32(NULL,object+0x08u,&desc) ||
        !read32(NULL,(uintptr_t)desc+24u*4u,&hp) ||
        !read32(NULL,(uintptr_t)desc+32u*4u,&max_hp) ||
        !max_hp || hp>max_hp) return 0;
    player->health=hp;
    player->max_health=max_hp;
    if (read32(NULL,(uintptr_t)desc+23u*4u,&bytes0)) {
        if (player->kind==ESP335_KIND_PLAYER)
            player->faction=faction_from_race(bytes0&0xffu);
        player->class_id=(bytes0>>8u)&0xffu;
    }
    if (read32(NULL,(uintptr_t)desc+54u*4u,&level) &&
        level>0u && level<=255u) player->level=level;
    /* Server-owned BG team/reaction MUST NOT be inferred from player race.
     * Player hostility remains UNKNOWN until runtime unit-reaction ABI is
     * verified. NPC hostility recognizes only documented monster templates,
     * never assumes all NPCs are hostile. */
    if (player->kind==ESP335_KIND_NPC &&
        read32(NULL,(uintptr_t)desc+55u*4u,&template_id) &&
        (template_id==14u || template_id==16u))
        player->relation=ESP335_REL_HOSTILE;
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
/* The 2026-09-24 game report showed hook installed=1 but EndScene frames=0.
 * Do not install dummy-device D3D9 hooks. WoW-native UIParent is rendered by
 * the actual client's own UI; AutoLoot remains the sole FrameScript ABI owner.
 */
static Esp335LuaGate get_lua_gate(void) {
    HMODULE host=GetModuleHandleW(L"AutoLoot335.dll");
    FARPROC symbol;
    if (!host) return NULL;
    symbol=GetProcAddress(host,"AL335_ExecuteUiScript");
    if (!symbol) symbol=GetProcAddress(host,"_AL335_ExecuteUiScript@8");
    return (Esp335LuaGate)symbol;
}
static void try_lua_gui(void) {
    DWORD now=GetTickCount();
    if (!g_enabled || !on_thread() || !g_game_window ||
        (g_lua_init_tick && (DWORD)(now-g_lua_init_tick)<5000u)) return;
    g_lua_init_tick=now;
    ++g_lua_init_attempts;
    g_lua_gate=get_lua_gate();
    if (!g_lua_gate) { ++g_lua_gate_missing;g_lua_ready=0u;return; }
    if (!esp335_lua_create(g_lua_gate)) {
        g_lua_ready=0u;
        return;
    }
    g_lua_ready=1u;
    ++g_lua_init_ok;
    g_lua_last_visibility=-1; /* restore after /reload or world UI teardown */
}
static void input(MSG *msg, WPARAM remove_mode) {
    int index;
    unsigned flags,bit;
    float x,y;
    if (!msg || !g_enabled || !on_thread() || remove_mode!=PM_REMOVE ||
        !g_game_window || !msg->hwnd ||
        GetAncestor(msg->hwnd,GA_ROOT)!=g_game_window) return;
    if (msg->message==WM_KEYUP && msg->wParam==VK_INSERT) {
        ++g_insert_events;
        /* Fallback for a short tap between 40ms polls; never toggle twice
         * if the same keypress was already sampled through DirectInput-
         * independent GetAsyncKeyState. */
        if (!g_key_toggled) {
            ++g_gui_toggles;
            InterlockedExchange(&g_gui_open,
                InterlockedCompareExchange(&g_gui_open,0,0) ? 0 : 1);
        }
        g_key_toggled=0u;
        g_prev_insert_down=0u;
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
static void poll_insert(void) {
    HWND foreground;
    unsigned pressed;
    if (!on_thread() || !g_enabled || !g_game_window) return;
    foreground=GetForegroundWindow();
    if (!foreground ||
        GetAncestor(foreground,GA_ROOT)!=g_game_window) {
        g_prev_insert_down=0u;
        return;
    }
    ++g_insert_polls;
    pressed=(GetAsyncKeyState(VK_INSERT) & 0x8000) ? 1u : 0u;
    if (pressed && !g_prev_insert_down) {
        ++g_gui_toggles;
        g_key_toggled=1u;
        InterlockedExchange(&g_gui_open,
            InterlockedCompareExchange(&g_gui_open,0,0) ? 0 : 1);
    }
    g_prev_insert_down=pressed;
}
static void write_diag(int scan_ok) {
    wchar_t path[MAX_PATH], *slash;
    HANDLE file;
    char line[1200];
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
        "\"markers\":%u,\"ui_flags\":%u,\"bind_failed\":%u,"
        "\"init_attempts\":%u,\"sha_rejects\":%u,\"layout_rejects\":%u,"
        "\"hook_calls\":%u,\"insert_events\":%u,"
        "\"insert_polls\":%u,\"gui_toggles\":%u,\"gui_open\":%u,"
        "\"lua_gate_missing\":%u,\"lua_init_attempts\":%u,"
        "\"lua_init_ok\":%u,\"lua_updates\":%u,"
        "\"lua_update_errors\":%u,\"lua_ready\":%u}\n",
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
        (unsigned)InterlockedCompareExchange(&g_ui_flags,0,0),g_bind_failed,
        g_init_attempts,g_sha_rejects,g_layout_rejects,
        g_hook_calls,g_insert_events,g_insert_polls,g_gui_toggles,
        (unsigned)InterlockedCompareExchange(&g_gui_open,0,0),
        g_lua_gate_missing,g_lua_init_attempts,g_lua_init_ok,
        g_lua_updates,g_lua_update_errors,g_lua_ready);
    if (length > 0) WriteFile(file, line, (DWORD)length, &written, NULL);
    CloseHandle(file);
}
static void drive(void) {
    int scan_ok,camera_ok;
    Esp335CameraAxes axes={0};
    Esp335Camera camera={0};
    RECT viewport={0};
    unsigned visibility;
    int valid_viewport;
    DWORD now;
    if (!g_enabled || !on_thread() || g_driving)
        return;
    now=GetTickCount();
    if ((DWORD)(now-g_tick_ms)<40u) return;
    g_driving=1u;
    g_tick_ms=now;
    poll_insert();
    try_lua_gui();
    if (g_lua_ready && g_lua_gate) {
        visibility=(unsigned)InterlockedCompareExchange(&g_gui_open,0,0);
        if (g_lua_last_visibility!=(int)visibility &&
            esp335_lua_visibility(g_lua_gate,visibility))
            g_lua_last_visibility=(int)visibility;
    }
    scan_ok=g_scanner.bound ? esp335_scanner_collect(&g_scanner) : 0;
    camera_ok=g_scanner.bound ? read_camera_axes(&axes) : 0;
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
        g_frame_players=g_scanner.accepted_players;
        g_frame_npcs=g_scanner.accepted_npcs;
        LeaveCriticalSection(&g_frame_lock);
    }
    valid_viewport=g_game_window && GetClientRect(g_game_window,&viewport) &&
        viewport.right-viewport.left>=64 && viewport.bottom-viewport.top>=64;
    memset(&camera,0,sizeof(camera));
    if (camera_ok && valid_viewport) {
        axes.viewport_x=0.f;
        axes.viewport_y=0.f;
        axes.viewport_width=(float)(viewport.right-viewport.left);
        axes.viewport_height=(float)(viewport.bottom-viewport.top);
        axes.aspect=axes.viewport_width/axes.viewport_height;
        camera_ok=esp335_camera_build(&axes,&camera);
    } else camera_ok=0;
    if (g_lua_ready && g_lua_gate &&
        (!g_lua_update_tick || (DWORD)(now-g_lua_update_tick)>=200u)) {
        g_lua_update_tick=now;
        if (esp335_lua_update(g_lua_gate,&g_scanner.snapshot,&camera,
            scan_ok?1u:0u,g_scanner.accepted_players,g_scanner.accepted_npcs,
            camera_ok?1u:0u)) ++g_lua_updates;
        else ++g_lua_update_errors;
    }
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
        if (g_lua_ready && g_lua_gate)
            esp335_lua_visibility(g_lua_gate,0u);
        g_lua_ready=0u;
        g_lua_last_visibility=-1;
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
        g_initialised=1u;
        g_thread=GetCurrentThreadId();
        if (!InitializeCriticalSectionAndSpinCount(&g_frame_lock,4000u))
            return;
        InterlockedExchange(&g_shared_ready,1);
    }
    if (!on_thread()) return;
    /* Renderer/UI must start independently of scanning ABI verification.
     * A failed scanner used to permanently suppress the whole GUI. */
    if (hwnd && IsWindow(hwnd)) {
        HWND root=GetAncestor(hwnd,GA_ROOT);
        DWORD tid=GetWindowThreadProcessId(root,NULL);
        if (tid==g_thread) g_game_window=root;
    }
    g_enabled=1u;
    try_lua_gui();
    if (!g_scanner.bound &&
        (g_bind_try_ms==0u ||
         (DWORD)(GetTickCount()-g_bind_try_ms)>=5000u)) {
        g_bind_try_ms=GetTickCount();
        ++g_init_attempts;
        memset(&host,0,sizeof(host));
        host.verify_client_sha256=check_client;
        host.verify_layout=check_layout;
        host.thread_id=thread_id;
        host.read_u32=read32;
        host.position=position;
        host.player_metadata=metadata;
        host.world_epoch=epoch;
        if (!esp335_scanner_bind(&g_scanner,&host))
            ++g_bind_failed; /* retry; do not disable UI or message hooks */
    }
}
__declspec(dllexport) LRESULT CALLBACK W335_HookProc(int code, WPARAM w, LPARAM l) {
    MSG *message;
    if (code < 0 || !l) return CallNextHookEx(NULL, code, w, l);
    message = (MSG *)l;
    ++g_hook_calls;
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
