/* Standalone in-process host for stock WoW 3.3.5a 12340 x86.
 * No third-party source. Does not install hooks or load itself: the updater's
 * authorized, game-thread window-message hook must load and invoke AL335_HookProc.
 * Runtime verifier and native ABI checks fail closed on any other game binary.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <math.h>
#include <string.h>
#include "autoloot_12340_adapter.h"

#pragma comment(lib, "Advapi32.lib")
#define AL_WINMSG_NAME "WoW335_AutoLoot_12340_GameThread_v1"
#define AL_GET_POS_VA 0x004D5EA0u
#define AL_LOOT_GUID_VA 0x00BFA8D8u
#define AL_FIRST_VALID_PTR 0x10000u
#define AL_MAX_VALID_PTR 0x7FFE0000u

static Al12340Adapter g_engine;
static DWORD g_game_thread;
static UINT g_message;
static unsigned g_was_owned;
static AlGuid g_last_owned;
static unsigned g_init_attempted;

typedef void (__thiscall *al_pos_fn)(void *, float *);
typedef void (__thiscall *al_right_click_fn)(void *, int);
typedef void (__cdecl *al_lua_fn)(const char *, const char *, int);

static int same(AlGuid a, AlGuid b) {
    return a.lo == b.lo && a.hi == b.hi;
}

/* SEH is required: objects may despawn between a scan and an interaction.
 * Never dereference foreign pointers outside the guarded read. */
static int readable(const void *p, size_t length) {
    MEMORY_BASIC_INFORMATION m;
    uintptr_t v = (uintptr_t)p;
    DWORD protection;
    if (!p || v < AL_FIRST_VALID_PTR || v >= AL_MAX_VALID_PTR ||
        length > AL_MAX_VALID_PTR - v || !VirtualQuery(p, &m, sizeof(m)) ||
        m.State != MEM_COMMIT) return 0;
    protection = m.Protect & 0xffu;
    if (m.Protect & PAGE_GUARD || m.Protect & PAGE_NOACCESS ||
        protection == PAGE_NOACCESS || protection == PAGE_EXECUTE)
        return 0;
    return v + length <= (uintptr_t)m.BaseAddress + m.RegionSize;
}
static int read32(void *ctx, uintptr_t at, uint32_t *out) {
    (void)ctx;
    if (!out || !readable((const void *)at, sizeof(*out))) return 0;
    __try { *out = *(volatile const uint32_t *)at; return 1; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static int is_game_thread(void) {
    return g_game_thread && GetCurrentThreadId() == g_game_thread;
}
static int exact_file_hash(const wchar_t *file, BYTE hash[32]) {
    HCRYPTPROV provider = 0;
    HCRYPTHASH digest = 0;
    HANDLE handle = INVALID_HANDLE_VALUE;
    BYTE buffer[65536];
    DWORD read_len, size = 32;
    int ok = 0;
    handle = CreateFileW(file, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE |
                         FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (handle == INVALID_HANDLE_VALUE ||
        !CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) ||
        !CryptCreateHash(provider, CALG_SHA_256, 0, 0, &digest)) goto cleanup;
    for (;;) {
        if (!ReadFile(handle, buffer, sizeof(buffer), &read_len, NULL)) goto cleanup;
        if (!read_len) break;
        if (!CryptHashData(digest, buffer, read_len, 0)) goto cleanup;
    }
    if (CryptGetHashParam(digest, HP_HASHVAL, hash, &size, 0) && size == 32)
        ok = 1;
cleanup:
    if (digest) CryptDestroyHash(digest);
    if (provider) CryptReleaseContext(provider, 0);
    if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
    return ok;
}
static int verify_exe(void *ctx, const char *expected) {
    wchar_t file[MAX_PATH];
    BYTE raw[32];
    char calculated[65];
    DWORD i;
    static const char hex[] = "0123456789abcdef";
    (void)ctx;
    if (strcmp(expected, AL12340_EXACT_EXE_SHA256) ||
        !GetModuleFileNameW(NULL, file, MAX_PATH) || !exact_file_hash(file, raw))
        return 0;
    for (i = 0; i < 32; ++i) {
        calculated[2*i] = hex[raw[i] >> 4];
        calculated[2*i+1] = hex[raw[i] & 15u];
    }
    calculated[64] = '\0';
    return strcmp(calculated, expected) == 0;
}
static int equal_bytes(uintptr_t at, const BYTE *expected, size_t n) {
    if (!readable((const void *)at, n)) return 0;
    __try { return memcmp((const void *)at, expected, n) == 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static int verify_abi(void *ctx, uintptr_t right_click, uintptr_t lua) {
    static const BYTE right_prefix[] = {0x55,0x8b,0xec,0x81,0xec,0xb4,0x02,0x00,0x00,0x56,0x57,0x8b,0xf9};
    static const BYTE lua_prefix[]   = {0x55,0x8b,0xec,0x51,0x83,0x05,0xa0,0x13,0xd4,0x00,0x01};
    static const BYTE pos_prefix[]   = {0x55,0x8b,0xec};
    (void)ctx;
    if (!is_game_thread() || right_click != AL12340_RIGHT_CLICK_VA ||
        lua != AL12340_LUA_EXECUTE_VA) return 0;
    /* Prologs were read from the SHA-pinned client by audit_autoloot_abi.py.
     * Prolog equality does NOT independently prove the full argument ABI. */
    return equal_bytes(right_click, right_prefix, sizeof(right_prefix)) &&
           equal_bytes(lua, lua_prefix, sizeof(lua_prefix)) &&
           equal_bytes(AL_GET_POS_VA, pos_prefix, sizeof(pos_prefix));
}
static uint32_t thread_id(void *ctx) {
    (void)ctx;
    return GetCurrentThreadId();
}
static int position(void *ctx, uintptr_t obj, float out[3]) {
    (void)ctx;
    if (!is_game_thread() || !out || !readable((void *)obj, 0x40))
        return 0;
    __try {
        ((al_pos_fn)AL_GET_POS_VA)((void *)obj, out);
        return _finite(out[0]) && _finite(out[1]) && _finite(out[2]);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}
static int right_click(void *ctx, uintptr_t va, uintptr_t obj, unsigned auto_loot) {
    (void)ctx;
    if (!is_game_thread() || va != AL12340_RIGHT_CLICK_VA ||
        !readable((void *)obj, 0x40) || auto_loot != 1u) return 0;
    /* Stock 12340 OnRightClick(this=unit in ECX, autoLoot=1).
     * The cast keeps x86 __thiscall and is never used on other clients. */
    __try {
        ((al_right_click_fn)va)((void *)obj, 1);
        g_was_owned = 0u;
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static AlUiState owned_loot(void *ctx, AlGuid active) {
    AlGuid window;
    (void)ctx;
    if (!is_game_thread() ||
        !read32(NULL, AL_LOOT_GUID_VA, &window.lo) ||
        !read32(NULL, AL_LOOT_GUID_VA + 4u, &window.hi))
        return AL_UI_BLOCKED;
    if (same(window, active)) {
        g_was_owned = 1u;
        g_last_owned = active;
        return AL_UI_OPEN;
    }
    if ((window.lo | window.hi) != 0u) return AL_UI_BLOCKED; /* manual / other owner */
    if (g_was_owned && same(g_last_owned, active)) {
        uint32_t conn, mgr, current, obj;
        g_was_owned = 0u;
        /* A closed window alone never certifies a completed loot. The core
         * defers and retries while the corpse is still visible/lootable. */
        if (!read32(NULL, AL12340_CLIENT_CONNECTION_VA, &conn) ||
            !read32(NULL, (uintptr_t)conn + AL12340_MANAGER_OFFSET, &mgr))
            return AL_UI_CLOSED;
        obj = 0u;
        if (!read32(NULL, (uintptr_t)mgr + 0xACu, &current))
            return AL_UI_CLOSED;
        {
            unsigned limit;
            for (limit=0; limit < 4096u && current >= AL_FIRST_VALID_PTR &&
                 current < AL_MAX_VALID_PTR; ++limit) {
                uint32_t low, high, next;
                if (!read32(NULL, (uintptr_t)current + 0x30u, &low) ||
                    !read32(NULL, (uintptr_t)current + 0x34u, &high)) break;
                if (low==active.lo && high==active.hi) { obj=current; break; }
                if (!read32(NULL, (uintptr_t)current + 0x3Cu, &next) ||
                    next==current) break;
                current=next;
            }
        }
        if (!obj) return AL_UI_EMPTY; /* despawned after owned loot session */
        {
            uint32_t desc=0u, flags=0u;
            if (!read32(NULL, (uintptr_t)obj + 8u, &desc) ||
                !read32(NULL, (uintptr_t)desc + 0x4Fu*4u, &flags))
                return AL_UI_CLOSED;
            return (flags & 1u) ? AL_UI_CLOSED : AL_UI_EMPTY;
        }
    }
    return AL_UI_CLOSED;
}
static int execute_lua(void *ctx, uintptr_t va, const char *script, const char *source) {
    (void)ctx;
    if (!is_game_thread() || va != AL12340_LUA_EXECUTE_VA || !script || !source)
        return 0;
    __try {
        ((al_lua_fn)va)(script, source, 0);
        return 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static int can_act(void *ctx) {
    AlGuid window={0u,0u};
    (void)ctx;
    return is_game_thread() &&
           read32(NULL, AL_LOOT_GUID_VA, &window.lo) &&
           read32(NULL, AL_LOOT_GUID_VA+4u, &window.hi) &&
           (((window.lo | window.hi) == 0u) ||
            (g_engine.engine.state != AL_IDLE &&
             same(window, g_engine.engine.active)));
}
static int bind_host(void) {
    Al12340Host host;
    memset(&host, 0, sizeof(host));
    host.verify_exe_sha256 = verify_exe;
    host.verify_abi = verify_abi;
    host.thread_id = thread_id;
    host.read_u32 = read32;
    host.position = position;
    host.right_click = right_click;
    host.owned_loot_window = owned_loot;
    host.execute_lua = execute_lua;
    host.can_act = can_act;
    return al12340_bind(&g_engine, &host, 5.0f);
}
/* Hook installation belongs to the explicitly authorized x86 launcher.
 * The target thread must be WoW's own window/game thread; WH_GETMESSAGE
 * naturally runs inside that thread, without patching Wow.exe opcodes. */
__declspec(dllexport) UINT WINAPI AL335_MessageId(void) {
    return RegisterWindowMessageA(AL_WINMSG_NAME);
}
__declspec(dllexport) LRESULT CALLBACK AL335_HookProc(int code, WPARAM wp, LPARAM lp) {
    MSG *msg;
    DWORD now;
    if (code < 0 || !lp) return CallNextHookEx(NULL, code, wp, lp);
    msg=(MSG *)lp;
    if (!g_message) g_message=RegisterWindowMessageA(AL_WINMSG_NAME);
    if (msg->message != g_message) return CallNextHookEx(NULL, code, wp, lp);
    if (!g_init_attempted) {
        g_init_attempted=1u;
        g_game_thread=GetCurrentThreadId();
        if (!bind_host()) {
            g_game_thread=0;
            return CallNextHookEx(NULL, code, wp, lp);
        }
    }
    if (!g_engine.bound || !is_game_thread()) return CallNextHookEx(NULL, code, wp, lp);
    if (msg->wParam==1u) al12340_enable(&g_engine, 1);
    else if (msg->wParam==0u) al12340_enable(&g_engine, 0);
    else if (msg->wParam==2u) {
        now=GetTickCount();
        al12340_tick(&g_engine, (uint32_t)now);
    }
    return CallNextHookEx(NULL, code, wp, lp);
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
