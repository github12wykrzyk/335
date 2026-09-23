/* WoW 3.3.5a 12340 x86 STARTUP LOADER: isolated TEST experiment only.
 * This DLL is imported by a copy-only patched EpochConnection.dll at process startup.
 * It never writes to another process or changes game memory.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <stdint.h>
#pragma comment(lib, "User32.lib")

#define WOW_MAX_MODULES 64
#define WOW_MAX_NAME 120

static HMODULE g_self;
static wchar_t g_root[MAX_PATH];

static int valid_name(const wchar_t *p) {
    size_t n = wcslen(p);
    if (n < 5 || n > WOW_MAX_NAME || p[0] == L'.') return 0;
    if (_wcsicmp(p + n - 4, L".dll") != 0) return 0;
    for (size_t i = 0; i < n; ++i) {
        wchar_t ch = p[i];
        if (!((ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z') ||
              (ch >= L'0' && ch <= L'9') || ch == L'_' || ch == L'-' || ch == L'.'))
            return 0;
    }
    return 1;
}

static void log_event(const wchar_t *name, const wchar_t *event, DWORD code) {
    wchar_t path[MAX_PATH];
    FILE *f;
    if (swprintf_s(path, MAX_PATH, L"%ls\\Wow335Loader.log", g_root) < 0) return;
    if (_wfopen_s(&f, path, L"a, ccs=UTF-8") != 0 || !f) return;
    fwprintf(f, L"%ls\t%ls\t%lu\n", event, name ? name : L"-", (unsigned long)code);
    fclose(f);
}

/* AutoLoot335 1.0.1-test: a verified game-window-thread callback drives
 * both WH_GETMESSAGE and WH_CALLWNDPROC. The loader never invokes game
 * internals on its own worker thread. Fail closed when required exports are
 * absent instead of silently reverting to the old input-delayed protocol.
 */
typedef UINT (WINAPI *al_message_fn)(void);
typedef LRESULT (CALLBACK *al_hook_fn)(int, WPARAM, LPARAM);
typedef struct { DWORD pid; DWORD tid; HWND hwnd; } GameWindow;
static BOOL CALLBACK find_game_window(HWND hwnd, LPARAM value) {
    GameWindow *game=(GameWindow *)value;
    DWORD pid=0, tid;
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER)) return TRUE;
    tid=GetWindowThreadProcessId(hwnd,&pid);
    if (tid && pid==game->pid) {
        game->tid=tid; game->hwnd=hwnd; return FALSE;
    }
    return TRUE;
}
static int send_control(HWND hwnd, UINT message, WPARAM command, UINT wait_ms) {
    DWORD_PTR ignored=0;
    return SendMessageTimeoutW(hwnd,message,command,0,
        SMTO_ABORTIFHUNG | SMTO_BLOCK,wait_ms,&ignored)!=0;
}
static DWORD activate_autoloot(HMODULE host) {
    GameWindow game={0};
    HHOOK msg_hook=NULL, dispatch_hook=NULL;
    al_message_fn getmsg=(al_message_fn)GetProcAddress(host,"AL335_MessageId");
    al_hook_fn callback=(al_hook_fn)GetProcAddress(host,"AL335_HookProc");
    al_hook_fn dispatch=(al_hook_fn)GetProcAddress(host,"AL335_CallWndProc");
    UINT message;
    DWORD start=GetTickCount(), result=1u;
    if (!getmsg) getmsg=(al_message_fn)GetProcAddress(host,"_AL335_MessageId@0");
    if (!callback) callback=(al_hook_fn)GetProcAddress(host,"_AL335_HookProc@12");
    if (!dispatch) dispatch=(al_hook_fn)GetProcAddress(host,"_AL335_CallWndProc@12");
    if (!getmsg || !callback || !dispatch || !(message=getmsg())) {
        log_event(L"AutoLoot335.dll",L"HOOK_EXPORT_MISSING",GetLastError());
        return 1;
    }
    game.pid=GetCurrentProcessId();
    while ((DWORD)(GetTickCount()-start)<120000u && !game.tid) {
        EnumWindows(find_game_window,(LPARAM)&game);
        if (!game.tid) Sleep(100);
    }
    if (!game.tid) {
        log_event(L"AutoLoot335.dll",L"GAME_WINDOW_NOT_FOUND",0);
        return 1;
    }
    msg_hook=SetWindowsHookExW(WH_GETMESSAGE,callback,host,game.tid);
    if (!msg_hook) {
        log_event(L"AutoLoot335.dll",L"HOOK_FAILED",GetLastError());
        return 1;
    }
    dispatch_hook=SetWindowsHookExW(WH_CALLWNDPROC,dispatch,host,game.tid);
    if (!dispatch_hook) {
        log_event(L"AutoLoot335.dll",L"DISPATCH_HOOK_FAILED",GetLastError());
        goto cleanup;
    }
    if (!send_control(game.hwnd,message,1u,2000u)) {
        log_event(L"AutoLoot335.dll",L"ENABLE_FAILED",GetLastError());
        goto cleanup;
    }
    log_event(L"AutoLoot335.dll",L"HOOK_ENABLED_RESPONSIVE",0);
    {
        DWORD pulses=0u, delivered=0u, timeouts=0u;
        while (IsWindow(game.hwnd)) {
            DWORD owner=0;
            GetWindowThreadProcessId(game.hwnd,&owner);
            if (owner!=game.pid) break;
            /* Logging runs on the loader worker only, never in the game
             * callback. SendMessageTimeout success means dispatch was
             * acknowledged, NOT that a corpse interaction succeeded. */
            if (send_control(game.hwnd,message,2u,200u)) ++delivered;
            else ++timeouts;
            ++pulses;
            if (pulses>=250u) {
                log_event(L"AutoLoot335.dll",L"PULSE_DELIVERED",delivered);
                log_event(L"AutoLoot335.dll",L"PULSE_TIMEOUT",timeouts);
                pulses=delivered=timeouts=0u;
            }
            Sleep(40);
        }
        if (pulses) {
            log_event(L"AutoLoot335.dll",L"PULSE_DELIVERED",delivered);
            log_event(L"AutoLoot335.dll",L"PULSE_TIMEOUT",timeouts);
        }
    }
    result=0;
cleanup:
    if (IsWindow(game.hwnd)) send_control(game.hwnd,message,0u,200u);
    if (dispatch_hook) UnhookWindowsHookEx(dispatch_hook);
    if (msg_hook) UnhookWindowsHookEx(msg_hook);
    log_event(L"AutoLoot335.dll",L"HOOK_STOPPED",result);
    return result;
}

static DWORD WINAPI load_modules(LPVOID unused) {
    wchar_t path[MAX_PATH], line[256], names[WOW_MAX_MODULES][WOW_MAX_NAME + 1];
    unsigned int count = 0;
    HMODULE autoloot=NULL;
    FILE *f;
    (void)unused;
    if (swprintf_s(path, MAX_PATH, L"%ls\\dlls.txt", g_root) < 0) return 1;
    /* The updater owns the verified module order. If no manifest exists yet,
       bootstrap an EMPTY list, never enumerate arbitrary DLLs from the game folder. */
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) {
        DWORD last = GetLastError();
        if (last != ERROR_FILE_NOT_FOUND) {
            log_event(NULL, L"MANIFEST_STAT_FAILED", last); return 1;
        }
        HANDLE empty = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                   CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (empty == INVALID_HANDLE_VALUE) {
            log_event(NULL, L"MANIFEST_CREATE_FAILED", GetLastError()); return 1;
        }
        CloseHandle(empty);
        log_event(NULL, L"MANIFEST_CREATED_EMPTY", 0);
    }
    if (_wfopen_s(&f, path, L"rt, ccs=UTF-8") != 0 || !f) {
        log_event(NULL, L"MANIFEST_OPEN_FAILED", GetLastError());
        return 1;
    }
    while (fgetws(line, sizeof(line) / sizeof(line[0]), f)) {
        size_t n = wcslen(line);
        if (n && line[n-1] != L'\n' && !feof(f)) { log_event(NULL, L"BAD_LINE", 1); fclose(f); return 1; }
        while (n && (line[n-1] == L'\r' || line[n-1] == L'\n')) line[--n] = 0;
        if (!n) continue;
        if (!valid_name(line) || count >= WOW_MAX_MODULES) {
            log_event(line, L"BAD_NAME", 2); fclose(f); return 1;
        }
        for (unsigned int i = 0; i < count; ++i)
            if (_wcsicmp(line, names[i]) == 0) {
                log_event(line, L"DUPLICATE", 3); fclose(f); return 1;
            }
        wcscpy_s(names[count++], WOW_MAX_NAME + 1, line);
    }
    if (ferror(f)) { log_event(NULL, L"READ_ERROR", 4); fclose(f); return 1; }
    fclose(f);

    /* The managed updater must have verified exact SHA256 identities and dependencies
       before starting the client. A DLL name alone is never GH provenance. */
    for (unsigned int i = 0; i < count; ++i) {
        HMODULE loaded;
        if (_wcsicmp(names[i], L"EpochConnection.dll") == 0 ||
            _wcsicmp(names[i], L"Wow335Loader.dll") == 0) {
            log_event(names[i], L"RESERVED", 5); return 1;
        }
        if (swprintf_s(path, MAX_PATH, L"%ls\\%ls", g_root, names[i]) < 0) return 1;
        if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) {
            log_event(names[i], L"MISSING", GetLastError()); return 1;
        }
        loaded = LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!loaded) {
            log_event(names[i], L"LOAD_FAILED", GetLastError()); return 1;
        }
        log_event(names[i], L"LOADED", 0);
        if (_wcsicmp(names[i], L"AutoLoot335.dll") == 0) autoloot=loaded;
    }
    if (autoloot) return activate_autoloot(autoloot);
    return 0;
}

/* Export is also a simple explicit marker for import-table integration. */
__declspec(dllexport) void __stdcall Wow335LoaderAnchor(void) { }

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        HANDLE h;
        DWORD n = GetModuleFileNameW(NULL, g_root, MAX_PATH);
        if (!n || n >= MAX_PATH) return FALSE;
        wchar_t *slash = wcsrchr(g_root, L'\\');
        if (!slash) return FALSE;
        *slash = 0;
        g_self = instance;
        DisableThreadLibraryCalls(instance);
        h = CreateThread(NULL, 0, load_modules, NULL, 0, NULL);
        if (!h) return FALSE;
        CloseHandle(h); /* never wait for a worker thread under loader lock */
    }
    return TRUE;
}
