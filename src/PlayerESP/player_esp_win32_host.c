/* Read-only PlayerESP game-thread host, diagnostic stage, not a visual ESP. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <float.h>
#include "player_esp_scanner.h"
#pragma comment(lib, "Advapi32.lib")
#define MESSAGE_NAME "WoW335_PlayerESP_12340_GameThread_v1"
#define GET_POS_VA ((uintptr_t)0x006E6F10u)
#define MIN_PTR ((uintptr_t)0x10000u)
#define MAX_PTR ((uintptr_t)0x7FFE0000u)
static Esp335Scanner g_scanner;
static DWORD g_thread, g_tick_ms, g_log_ms;
static UINT g_message;
static unsigned g_initialised, g_enabled, g_driving, g_live;
static uint32_t g_last_manager, g_scans_ok, g_scans_failed;
static uint64_t g_last_player_guid, g_epoch;

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
        !read32(NULL, (uintptr_t)desc + 25u * 4u, &max_hp) ||
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
static void write_diag(int scan_ok) {
    wchar_t path[MAX_PATH], *slash;
    HANDLE file;
    char line[256];
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
        "\"epoch\":%I64u,\"scans_ok\":%u,\"scans_failed\":%u}\n",
        scan_ok ? 1u : 0u,
        scan_ok ? (unsigned)g_scanner.snapshot.count : 0u,
        (unsigned __int64)g_scanner.snapshot.world_epoch,
        g_scans_ok, g_scans_failed);
    if (length > 0) WriteFile(file, line, (DWORD)length, &written, NULL);
    CloseHandle(file);
}
static void drive(void) {
    int ok;
    DWORD now;
    if (!g_enabled || !g_scanner.bound || !on_thread() || g_driving) return;
    now = GetTickCount();
    if ((DWORD)(now - g_tick_ms) < 40u) return;
    g_driving = 1u;
    g_tick_ms = now;
    ok = esp335_scanner_collect(&g_scanner);
    if (ok) ++g_scans_ok; else ++g_scans_failed;
    write_diag(ok);
    g_driving = 0u;
}
__declspec(dllexport) UINT WINAPI W335_MessageId(void) {
    return RegisterWindowMessageA(MESSAGE_NAME);
}
static void control(UINT message, WPARAM command) {
    Esp335ScannerHost host;
    if (!g_message) g_message = RegisterWindowMessageA(MESSAGE_NAME);
    if (message != g_message) return;
    if (command == 0u) {
        g_enabled = 0u;
        esp335_reset(&g_scanner.snapshot);
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
            g_thread = 0u;
            return;
        }
    }
    if (g_scanner.bound && on_thread() && command == 1u) g_enabled = 1u;
}
__declspec(dllexport) LRESULT CALLBACK W335_HookProc(int code, WPARAM w, LPARAM l) {
    MSG *message;
    if (code < 0 || !l) return CallNextHookEx(NULL, code, w, l);
    message = (MSG *)l;
    if (message->message != WM_QUIT) {
        control(message->message, message->wParam);
        drive();
    }
    return CallNextHookEx(NULL, code, w, l);
}
__declspec(dllexport) LRESULT CALLBACK W335_CallWndProc(int code, WPARAM w, LPARAM l) {
    const CWPSTRUCT *message;
    if (code < 0 || !l) return CallNextHookEx(NULL, code, w, l);
    message = (const CWPSTRUCT *)l;
    if (message->message != WM_QUIT) {
        control(message->message, message->wParam);
        drive();
    }
    return CallNextHookEx(NULL, code, w, l);
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(instance);
    return TRUE;
}
