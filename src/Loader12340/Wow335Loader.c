/* WoW 3.3.5a 12340 x86 STARTUP LOADER: isolated TEST experiment only.
 * This DLL is imported by a copy-only patched EpochConnection.dll at process startup.
 * It never writes to another process or changes game memory.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <stdint.h>
#include <wincrypt.h>
#include <string.h>
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Advapi32.lib")

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

/* modules.lock is written by the managed updater from the verified package.
 * It is a local consistency seal, NOT an authentication signature: updater
 * must still verify GitHub artifact provenance before creating it. */
static int local_sha256(const wchar_t *path, char result[65]) {
    DWORD attrs = GetFileAttributesW(path);
    HANDLE file = INVALID_HANDLE_VALUE;
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    BYTE buffer[16384], digest[32];
    DWORD got = 0, length = sizeof(digest);
    LARGE_INTEGER size;
    static const char hex[] = "0123456789abcdef";
    int ok = 0;
    if (attrs == INVALID_FILE_ATTRIBUTES ||
        (attrs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
        return 0;
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    if (!GetFileSizeEx(file, &size) || size.QuadPart > 128 * 1024 * 1024)
        goto done;
    if (!CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_AES,
                             CRYPT_VERIFYCONTEXT) ||
        !CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash))
        goto done;
    for (;;) {
        if (!ReadFile(file, buffer, sizeof(buffer), &got, NULL))
            goto done;
        if (!got) break;
        if (!CryptHashData(hash, buffer, got, 0)) goto done;
    }
    if (!CryptGetHashParam(hash, HP_HASHVAL, digest, &length, 0) ||
        length != sizeof(digest)) goto done;
    for (unsigned int i = 0; i < sizeof(digest); ++i) {
        result[i * 2] = hex[digest[i] >> 4];
        result[i * 2 + 1] = hex[digest[i] & 15];
    }
    result[64] = 0;
    ok = 1;
done:
    if (hash) CryptDestroyHash(hash);
    if (provider) CryptReleaseContext(provider, 0);
    CloseHandle(file);
    return ok;
}

static int lock_line(FILE *file, char *line, size_t capacity) {
    size_t n;
    if (!fgets(line, (int)capacity, file)) return 0;
    n = strlen(line);
    if (!n || (line[n - 1] != '\n' && !feof(file))) return 0;
    while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
        line[--n] = 0;
    return 1;
}
static int is_lower_sha(const char *value) {
    if (strlen(value) != 64) return 0;
    for (unsigned int i = 0; i < 64; ++i)
        if (!((value[i] >= '0' && value[i] <= '9') ||
              (value[i] >= 'a' && value[i] <= 'f'))) return 0;
    return 1;
}
static int preflight_module_lock(
    wchar_t names[WOW_MAX_MODULES][WOW_MAX_NAME + 1],
    unsigned int count) {
    wchar_t lock_path[MAX_PATH], dll_list[MAX_PATH], dll_path[MAX_PATH];
    char line[256], list_digest[65], module_digest[65];
    FILE *file = NULL;
    int ok = 0;
    if (!count) return 1; /* An empty list cannot load arbitrary modules. */
    if (swprintf_s(lock_path, MAX_PATH,
                   L"%ls\\.wow335_updater\\epoch_test\\modules.lock", g_root) < 0 ||
        swprintf_s(dll_list, MAX_PATH, L"%ls\\dlls.txt", g_root) < 0 ||
        !local_sha256(dll_list, list_digest))
        goto done;
    if (_wfopen_s(&file, lock_path, L"rb") != 0 || !file)
        goto done;
    if (!lock_line(file, line, sizeof(line)) ||
        strcmp(line, "WOW335-MODULES-V1") != 0 ||
        !lock_line(file, line, sizeof(line)) ||
        strcmp(line, list_digest) != 0)
        goto done;
    for (unsigned int i = 0; i < count; ++i) {
        char ascii_name[WOW_MAX_NAME + 1], *separator;
        size_t n = wcslen(names[i]);
        if (n > WOW_MAX_NAME) goto done;
        for (size_t k = 0; k < n; ++k) ascii_name[k] = (char)names[i][k];
        ascii_name[n] = 0;
        if (!lock_line(file, line, sizeof(line))) goto done;
        separator = strchr(line, ' ');
        if (!separator || strchr(separator + 1, ' ')) goto done;
        *separator++ = 0;
        if (strcmp(line, ascii_name) != 0 || !is_lower_sha(separator))
            goto done;
        if (swprintf_s(dll_path, MAX_PATH, L"%ls\\%ls",
                       g_root, names[i]) < 0 ||
            !local_sha256(dll_path, module_digest) ||
            strcmp(module_digest, separator) != 0)
            goto done;
    }
    if (fgetc(file) != EOF || ferror(file)) goto done;
    ok = 1;
done:
    if (file) fclose(file);
    if (!ok) log_event(NULL, L"MODULE_LOCK_SHA256_FAILED", ERROR_INVALID_DATA);
    return ok;
}

/* Preflight the COMPLETE module list before loading its first DLL. A stale,
 * truncated, substituted or x64 DLL cannot leave a half-loaded runtime.
 * SHA256/provenance remains the managed updater's separate prerequisite;
 * this local check also protects direct launches that bypass that GUI. */
static int preflight_module(const wchar_t *name) {
    wchar_t path[MAX_PATH];
    HANDLE file = INVALID_HANDLE_VALUE;
    DWORD got = 0, attrs;
    LARGE_INTEGER size, offset;
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS32 nt;
    int ok = 0;
    if (_wcsicmp(name, L"EpochConnection.dll") == 0 ||
        _wcsicmp(name, L"Wow335Loader.dll") == 0) {
        log_event(name, L"RESERVED", 5);
        return 0;
    }
    if (swprintf_s(path, MAX_PATH, L"%ls\\%ls", g_root, name) < 0)
        return 0;
    attrs = GetFileAttributesW(path);
    if (attrs == INVALID_FILE_ATTRIBUTES ||
        (attrs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
        log_event(name, L"PRECHECK_PATH", GetLastError());
        return 0;
    }
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        log_event(name, L"PRECHECK_OPEN", GetLastError());
        return 0;
    }
    if (!GetFileSizeEx(file, &size) ||
        size.QuadPart < (LONGLONG)sizeof(dos) ||
        !ReadFile(file, &dos, sizeof(dos), &got, NULL) ||
        got != sizeof(dos) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew < (LONG)sizeof(dos) ||
        (LONGLONG)dos.e_lfanew + (LONGLONG)sizeof(nt) > size.QuadPart)
        goto done;
    offset.QuadPart = dos.e_lfanew;
    if (!SetFilePointerEx(file, offset, NULL, FILE_BEGIN) ||
        !ReadFile(file, &nt, sizeof(nt), &got, NULL) || got != sizeof(nt))
        goto done;
    if (nt.Signature != IMAGE_NT_SIGNATURE ||
        nt.FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        !(nt.FileHeader.Characteristics & IMAGE_FILE_DLL) ||
        nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER32) ||
        nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        goto done;
    ok = 1;
done:
    CloseHandle(file);
    if (!ok) log_event(name, L"PRECHECK_PE32_X86_FAILED", ERROR_BAD_EXE_FORMAT);
    return ok;
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

    /* Verify the entire updater-managed lock BEFORE loading any member.
       The lock binds exact bytes and list order to the selected game folder. */
    if (!preflight_module_lock(names, count)) return 1;
    for (unsigned int i = 0; i < count; ++i) {
        if (!preflight_module(names[i])) return 1;
    }
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
