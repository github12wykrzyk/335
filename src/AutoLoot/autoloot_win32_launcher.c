/* Authorized 12340 x86 loader. For AL_NORMAL_RUNTIME it is bundled inside
 * the verified updater and loads ONLY the manifest-installed AutoLoot335.dll
 * from the selected game directory. It never patches Wow.exe. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#ifdef AL_NORMAL_RUNTIME
#define AL_HOST_FILENAME L"AutoLoot335.dll"
#else
#define AL_HOST_FILENAME L"AutoLoot335_Host_UNREGISTERED.dll"
#endif
typedef UINT (WINAPI *message_id_fn)(void);
typedef LRESULT (CALLBACK *hook_fn)(int, WPARAM, LPARAM);
typedef struct { DWORD pid; DWORD tid; HWND hwnd; } Search;
static BOOL CALLBACK find_window(HWND hwnd, LPARAM lparam) {
    Search *s = (Search *)lparam;
    DWORD pid = 0u;
    DWORD tid;
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER)) return TRUE;
    tid = GetWindowThreadProcessId(hwnd, &pid);
    if (tid && pid == s->pid) { s->tid=tid; s->hwnd=hwnd; return FALSE; }
    return TRUE;
}
/* Sent messages are processed even by many filtered nested input pumps;
 * unlike periodic PostMessage, this never accumulates an unbounded backlog.
 * A hung game thread times out; no WoW internals are called by this process.
 */
static int send_control(HWND hwnd, UINT msg, WPARAM command, UINT timeout_ms) {
    DWORD_PTR ignored = 0;
    return SendMessageTimeoutW(hwnd, msg, command, 0,
        SMTO_ABORTIFHUNG | SMTO_BLOCK, timeout_ms, &ignored) != 0;
}
int wmain(int argc, wchar_t **argv) {
    wchar_t path[MAX_PATH], gameDir[MAX_PATH], command[2*MAX_PATH];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi = {0};
    HMODULE host = NULL;
    HHOOK hook_msg = NULL;
    HHOOK hook_dispatch = NULL;
    Search s = {0};
    message_id_fn getmsg;
    hook_fn proc;
    hook_fn dispatch_proc;
    UINT msg = 0u;
    DWORD start, exitCode=STILL_ACTIVE;
    int result=1;
    if (argc != 3 || wcscmp(argv[1], L"--exe") || !argv[2][0]) {
        fputs("Usage: AutoLoot335_Launcher_PREVIEW.exe --exe <absolute path to Wow.exe>\n", stderr);
        return 2;
    }
    if (!GetFullPathNameW(argv[2], MAX_PATH, path, NULL) ||
        GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES ||
        _wcsicmp(wcsrchr(path, L'\\') ? wcsrchr(path,L'\\')+1 : path,L"Wow.exe")) {
        fputs("An exact WoW.exe path is required.\n", stderr); return 3;
    }
    wcscpy_s(gameDir, MAX_PATH, path);
    *wcsrchr(gameDir, L'\\') = L'\0';
#ifdef AL_NORMAL_RUNTIME
    /* The normal DLL is already part of the exact-SHA installed game stack.
     * No second download, independent preview, or guessed DLL path. */
    swprintf_s(command, 2*MAX_PATH, L"%s\\%s", gameDir, AL_HOST_FILENAME);
#else
    if (!GetModuleFileNameW(NULL, command, MAX_PATH)) return 4;
    *wcsrchr(command, L'\\') = L'\0';
    wcscat_s(command, 2*MAX_PATH, L"\\" AL_HOST_FILENAME);
#endif
    host=LoadLibraryW(command);
    if (!host) { fprintf(stderr,"Native module unavailable: %lu\n", GetLastError());return 5; }
    getmsg=(message_id_fn)GetProcAddress(host, "AL335_MessageId");
    if (!getmsg) getmsg=(message_id_fn)GetProcAddress(host, "_AL335_MessageId@0");
    proc=(hook_fn)GetProcAddress(host, "AL335_HookProc");
    if (!proc) proc=(hook_fn)GetProcAddress(host, "_AL335_HookProc@12");
    dispatch_proc=(hook_fn)GetProcAddress(host, "AL335_CallWndProc");
    if (!dispatch_proc) dispatch_proc=(hook_fn)GetProcAddress(host, "_AL335_CallWndProc@12");
    if (!getmsg || !proc || !dispatch_proc || !(msg=getmsg())) {
        fputs("Native hook exports unavailable.\n", stderr);goto cleanup;
    }
    memset(&si,0,sizeof(si));si.cb=sizeof(si);
    memset(&pi,0,sizeof(pi));
    swprintf_s(command, 2*MAX_PATH, L"\"%s\"", path);
    if (!CreateProcessW(path,command,NULL,NULL,FALSE,0,NULL,gameDir,&si,&pi)) {
        fprintf(stderr,"Wow.exe launch failed: %lu\n",GetLastError());goto cleanup;
    }
    s.pid=pi.dwProcessId;s.tid=0;s.hwnd=NULL;
    start=GetTickCount();
    while (GetTickCount()-start < 120000u && s.tid==0u) {
        if (!GetExitCodeProcess(pi.hProcess,&exitCode) || exitCode != STILL_ACTIVE) break;
        EnumWindows(find_window,(LPARAM)&s);
        if (!s.tid) Sleep(100);
    }
    if (!s.tid) { fputs("WoW game window thread not found; no hook installed.\n",stderr); goto cleanup; }
    hook_msg=SetWindowsHookExW(WH_GETMESSAGE,proc,host,s.tid);
    if (!hook_msg) {fprintf(stderr,"Game-thread message hook rejected: %lu\n",GetLastError());goto cleanup;}
    hook_dispatch=SetWindowsHookExW(WH_CALLWNDPROC,dispatch_proc,host,s.tid);
    if (!hook_dispatch) {fprintf(stderr,"Game-thread sent-message hook rejected: %lu\n",GetLastError());goto cleanup;}
    if (!send_control(s.hwnd,msg,1u,2000u)) {
        fprintf(stderr,"Unable to initialize game-thread AutoLoot: %lu\n",GetLastError());
        goto cleanup;
    }
    puts("Native AutoLoot requested on game thread; exact-client checks may keep it OFF.");
    while (GetExitCodeProcess(pi.hProcess,&exitCode) && exitCode==STILL_ACTIVE) {
        if (!IsWindow(s.hwnd)) {
            fputs("WoW window lost; stopping AutoLoot launcher.\n",stderr);
            goto cleanup;
        }
        /* Timeout is nonfatal: retry when the window thread resumes. */
        send_control(s.hwnd,msg,2u,200u);
        Sleep(40);
    }
    result=0;
cleanup:
    if (hook_msg || hook_dispatch) {
        if (IsWindow(s.hwnd)) send_control(s.hwnd,msg,0u,200u);
        if (hook_dispatch) UnhookWindowsHookEx(hook_dispatch);
        if (hook_msg) UnhookWindowsHookEx(hook_msg);
    }
    if (pi.hThread) CloseHandle(pi.hThread);
    if (pi.hProcess) CloseHandle(pi.hProcess);
    if (host) FreeLibrary(host);
    return result;
}
