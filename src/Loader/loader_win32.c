/* WoW 3.3.5a 12340 x86: manifest-driven launcher for updater-managed modules.
 * Only names in the verified dlls.txt are loaded, in declared dependency order.
 * Every module implements the W335 hook ABI; AutoLoot335.dll has legacy AL335
 * exports so its existing verified runtime bytes remain unchanged.
 * Win32 CallNextHookEx provides the message-hook chain; game-level resource
 * ownership is validated separately by runtime/module_registry.json.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define MAX_MODULES 32
#define MAX_DLL_NAME 96
typedef UINT (WINAPI *message_id_fn)(void);
typedef LRESULT (CALLBACK *hook_fn)(int, WPARAM, LPARAM);
typedef struct {
    wchar_t name[MAX_DLL_NAME + 1];
    HMODULE library;
    HHOOK receive_hook;
    HHOOK dispatch_hook;
    UINT message;
    hook_fn on_receive;
    hook_fn on_dispatch;
} Module;
typedef struct { DWORD pid, tid; HWND hwnd; } Search;

static BOOL CALLBACK find_game_window(HWND hwnd, LPARAM context) {
    Search *s = (Search *)context;
    DWORD pid = 0;
    DWORD tid;
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER)) return TRUE;
    tid = GetWindowThreadProcessId(hwnd, &pid);
    if (tid && pid == s->pid) { s->tid = tid; s->hwnd = hwnd; return FALSE; }
    return TRUE;
}
static int pulse(HWND hwnd, UINT message, WPARAM command, UINT timeout_ms) {
    DWORD_PTR ignored = 0;
    return SendMessageTimeoutW(hwnd, message, command, 0,
        SMTO_ABORTIFHUNG | SMTO_BLOCK, timeout_ms, &ignored) != 0;
}
static FARPROC symbol(HMODULE library, const char *plain, const char *decorated) {
    FARPROC p = GetProcAddress(library, plain);
    return p ? p : GetProcAddress(library, decorated);
}
static int valid_name(const char *s) {
    size_t i, n = strlen(s);
    if (n < 5 || n > MAX_DLL_NAME || s[0] == '.' ||
        _stricmp(s + n - 4, ".dll") != 0) return 0;
    for (i = 0; i < n; ++i) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
            return 0;
        if (c == '.' && i + 1 < n && s[i + 1] == '.') return 0;
    }
    return 1;
}
static int read_manifest(const wchar_t *directory, Module *modules, int *count) {
    wchar_t list_path[MAX_PATH];
    char line[160];
    FILE *file = NULL;
    int i, length, n = 0;
    if (swprintf_s(list_path, MAX_PATH, L"%s\\dlls.txt", directory) < 0 ||
        _wfopen_s(&file, list_path, L"rb") != 0 || !file) {
        fputs("Missing dlls.txt in the selected game directory.\n", stderr);
        return 0;
    }
    while (fgets(line, sizeof(line), file)) {
        size_t size = strlen(line);
        if (size == sizeof(line) - 1 && line[size - 1] != '\n') goto invalid;
        while (size && (line[size - 1] == '\r' || line[size - 1] == '\n'))
            line[--size] = '\0';
        if (!size) continue;
        if (n == MAX_MODULES || !valid_name(line)) goto invalid;
        length = (int)size;
        for (i = 0; i < length; ++i) modules[n].name[i] = (wchar_t)(unsigned char)line[i];
        modules[n].name[length] = L'\0';
        for (i = 0; i < n; ++i)
            if (_wcsicmp(modules[i].name, modules[n].name) == 0) goto invalid;
        ++n;
    }
    if (ferror(file) || n == 0) goto invalid;
    fclose(file);
    *count = n;
    return 1;
invalid:
    fclose(file);
    fputs("Invalid, duplicate or empty DLL manifest.\n", stderr);
    return 0;
}
static int preload(const wchar_t *directory, Module *modules, int count) {
    int i;
    wchar_t path[MAX_PATH];
    message_id_fn get_message;
    for (i = 0; i < count; ++i) {
        if (swprintf_s(path, MAX_PATH, L"%s\\%s", directory, modules[i].name) < 0)
            return 0;
        modules[i].library = LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!modules[i].library) {
            fprintf(stderr, "Module %d LoadLibraryEx failed: %lu\n", i, GetLastError());
            return 0;
        }
        get_message = (message_id_fn)symbol(modules[i].library,
                                           "W335_MessageId", "_W335_MessageId@0");
        modules[i].on_receive = (hook_fn)symbol(modules[i].library,
                                                "W335_HookProc", "_W335_HookProc@12");
        modules[i].on_dispatch = (hook_fn)symbol(modules[i].library,
                                                 "W335_CallWndProc", "_W335_CallWndProc@12");
        /* Compatibility only for the existing manifest-pinned AutoLoot DLL. */
        if (_wcsicmp(modules[i].name, L"AutoLoot335.dll") == 0) {
            if (!get_message) get_message = (message_id_fn)symbol(
                modules[i].library, "AL335_MessageId", "_AL335_MessageId@0");
            if (!modules[i].on_receive) modules[i].on_receive = (hook_fn)symbol(
                modules[i].library, "AL335_HookProc", "_AL335_HookProc@12");
            if (!modules[i].on_dispatch) modules[i].on_dispatch = (hook_fn)symbol(
                modules[i].library, "AL335_CallWndProc", "_AL335_CallWndProc@12");
        }
        if (!get_message || !modules[i].on_receive || !modules[i].on_dispatch ||
            !(modules[i].message = get_message())) {
            fprintf(stderr, "Module %d does not implement the W335 hook ABI.\n", i);
            return 0;
        }
    }
    return 1;
}
static void unload(Module *modules, int count, HWND hwnd) {
    int i;
    if (IsWindow(hwnd))
        for (i = count - 1; i >= 0; --i)
            if (modules[i].receive_hook && modules[i].dispatch_hook)
                pulse(hwnd, modules[i].message, 0u, 200u);
    for (i = count - 1; i >= 0; --i) {
        if (modules[i].dispatch_hook) UnhookWindowsHookEx(modules[i].dispatch_hook);
        if (modules[i].receive_hook) UnhookWindowsHookEx(modules[i].receive_hook);
        if (modules[i].library) FreeLibrary(modules[i].library);
    }
}
int wmain(int argc, wchar_t **argv) {
    wchar_t exe[MAX_PATH], directory[MAX_PATH], command[2 * MAX_PATH];
    STARTUPINFOW si = {0};
    PROCESS_INFORMATION pi = {0};
    Search search = {0};
    Module modules[MAX_MODULES] = {{0}};
    DWORD started, exit_code = STILL_ACTIVE;
    int i, count = 0, result = 1, started_game = 0, attached = 0;
    if (argc != 3 || wcscmp(argv[1], L"--exe") || !argv[2][0]) return 2;
    if (!GetFullPathNameW(argv[2], MAX_PATH, exe, NULL) ||
        GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES ||
        !wcsrchr(exe, L'\\') || _wcsicmp(wcsrchr(exe, L'\\') + 1, L"Wow.exe"))
        return 3;
    wcscpy_s(directory, MAX_PATH, exe);
    *wcsrchr(directory, L'\\') = L'\0';
    if (!read_manifest(directory, modules, &count)) return 4;
    if (!preload(directory, modules, count)) goto cleanup;
    si.cb = sizeof(si);
    if (swprintf_s(command, 2 * MAX_PATH, L"\"%s\"", exe) < 0 ||
        !CreateProcessW(exe, command, NULL, NULL, FALSE, 0, NULL, directory, &si, &pi)) {
        fprintf(stderr, "Game CreateProcess failed: %lu\n", GetLastError());
        goto cleanup;
    }
    started_game = 1;
    search.pid = pi.dwProcessId;
    started = GetTickCount();
    while (GetTickCount() - started < 120000u && !search.tid) {
        if (!GetExitCodeProcess(pi.hProcess, &exit_code) || exit_code != STILL_ACTIVE)
            break;
        EnumWindows(find_game_window, (LPARAM)&search);
        if (!search.tid) Sleep(100);
    }
    if (!search.tid) goto cleanup;
    for (i = 0; i < count; ++i) {
        modules[i].receive_hook = SetWindowsHookExW(WH_GETMESSAGE,
            modules[i].on_receive, modules[i].library, search.tid);
        if (!modules[i].receive_hook) goto cleanup;
        modules[i].dispatch_hook = SetWindowsHookExW(WH_CALLWNDPROC,
            modules[i].on_dispatch, modules[i].library, search.tid);
        if (!modules[i].dispatch_hook) goto cleanup;
        if (!pulse(search.hwnd, modules[i].message, 1u, 2000u)) goto cleanup;
    }
    attached = 1;
    while (GetExitCodeProcess(pi.hProcess, &exit_code) && exit_code == STILL_ACTIVE) {
        if (!IsWindow(search.hwnd)) goto cleanup;
        for (i = 0; i < count; ++i)
            pulse(search.hwnd, modules[i].message, 2u, 200u);
        Sleep(40);
    }
    result = 0;
cleanup:
    /* Never leave a newly launched game running with an incomplete hook set. */
    if (started_game && !attached && pi.hProcess)
        TerminateProcess(pi.hProcess, 1u);
    unload(modules, count, search.hwnd);
    if (pi.hThread) CloseHandle(pi.hThread);
    if (pi.hProcess) CloseHandle(pi.hProcess);
    return result;
}
