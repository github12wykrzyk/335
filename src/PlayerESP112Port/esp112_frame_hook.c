/* One PlayerESP-owned D3D9 EndScene trampoline, no competing vtable patch.
 * MinHook vendor code/terms under src/ThirdParty/MinHook.
 * Fail closed for foreign entry-point jumps, nonmatching WoW HWND,
 * unexpected device/viewport, and non-game render thread (host).
 * Never hook in DllMain; never present or reset the device ourselves.
 */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <string.h>
#include "../ThirdParty/MinHook/include/MinHook.h"
#include "esp112_frame_hook.h"
#pragma comment(lib,"d3d9.lib")
typedef HRESULT (STDMETHODCALLTYPE *Esp112EndScene)(IDirect3DDevice9 *);
static Esp112EndScene g_original;
static void *g_target;
static HWND g_window;
static Esp112FrameCallback g_callback;
static void *g_callback_user;
static volatile LONG g_running,g_inside,g_callbacks,g_rejected;
static int matches_game_device(IDirect3DDevice9 *device) {
    D3DDEVICE_CREATION_PARAMETERS cp;
    D3DVIEWPORT9 vp;
    DWORD pid=0;
    HWND parent;
    if (!device || !g_window || !IsWindow(g_window) ||
        FAILED(IDirect3DDevice9_GetCreationParameters(device,&cp)) ||
        !cp.hFocusWindow || !IsWindow(cp.hFocusWindow) ||
        FAILED(IDirect3DDevice9_GetViewport(device,&vp)) ||
        vp.Width<64u || vp.Height<64u) return 0;
    parent=GetAncestor(cp.hFocusWindow,GA_ROOT);
    if (GetWindowThreadProcessId(parent,&pid)==0 ||
        pid!=GetCurrentProcessId()) return 0;
    return parent==g_window;
}
static HRESULT STDMETHODCALLTYPE hooked_end_scene(IDirect3DDevice9 *device) {
    Esp112EndScene original=g_original;
    if (!original) return D3DERR_INVALIDCALL;
    if (InterlockedCompareExchange(&g_running,0,0) &&
        InterlockedCompareExchange(&g_inside,1,0)==0) {
        if (matches_game_device(device)) {
            InterlockedIncrement(&g_callbacks);
            if (g_callback) g_callback(device,g_callback_user);
        } else InterlockedIncrement(&g_rejected);
        InterlockedExchange(&g_inside,0);
    }
    return original(device);
}
static int safe_entry(void *entry) {
    MEMORY_BASIC_INFORMATION memory;
    const unsigned char *p=(const unsigned char *)entry;
    if (!entry || !VirtualQuery(entry,&memory,sizeof(memory)) ||
        memory.State!=MEM_COMMIT || memory.Type!=MEM_IMAGE ||
        (memory.Protect & (PAGE_GUARD|PAGE_NOACCESS)) ||
        (uintptr_t)entry+8u>(uintptr_t)memory.BaseAddress+memory.RegionSize)
        return 0;
    __try {
        /* Existing third-party trampoline? Do not silently chain a second
         * D3D9 owner or overwrite a short/absolute jump. */
        if (p[0]==0xe9u || p[0]==0xebu ||
            (p[0]==0xffu && p[1]==0x25u) ||
            (p[0]==0x68u && p[5]==0xc3u)) return 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return 1;
}
int esp112_frame_install(HWND hwnd,Esp112FrameCallback callback,void *ctx) {
    IDirect3D9 *api=NULL;
    IDirect3DDevice9 *dummy=NULL;
    D3DPRESENT_PARAMETERS pp;
    void *target;
    DWORD pid=0;
    MH_STATUS status;
    if (g_target || !callback || !IsWindow(hwnd) ||
        GetWindowThreadProcessId(hwnd,&pid)!=GetCurrentThreadId() ||
        pid!=GetCurrentProcessId() || !GetModuleHandleW(L"d3d9.dll"))
        return 0;
    api=Direct3DCreate9(D3D_SDK_VERSION);
    if (!api) return 0;
    memset(&pp,0,sizeof(pp));
    pp.Windowed=TRUE;
    pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow=hwnd;
    pp.BackBufferWidth=1u;pp.BackBufferHeight=1u;
    pp.BackBufferFormat=D3DFMT_UNKNOWN;
    if (FAILED(IDirect3D9_CreateDevice(api,D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,
            &pp,&dummy)) || !dummy) {
        IDirect3D9_Release(api);
        return 0;
    }
    target=(*(void ***)dummy)[42]; /* IDirect3DDevice9::EndScene */
    IDirect3DDevice9_Release(dummy);
    IDirect3D9_Release(api);
    if (!safe_entry(target)) return 0;
    status=MH_Initialize();
    if (status!=MH_OK) return 0;
    g_window=hwnd;
    g_callback=callback;
    g_callback_user=ctx;
    status=MH_CreateHook(target,(void *)hooked_end_scene,
                         (void **)&g_original);
    if (status!=MH_OK) goto fail;
    g_target=target;
    status=MH_EnableHook(target);
    if (status!=MH_OK) goto fail;
    InterlockedExchange(&g_running,1);
    return 1;
fail:
    InterlockedExchange(&g_running,0);
    if (g_target) MH_RemoveHook(g_target);
    g_target=NULL;g_original=NULL;
    g_callback=NULL;g_callback_user=NULL;g_window=NULL;
    MH_Uninitialize();
    return 0;
}
void esp112_frame_uninstall(void) {
    if (!g_target || InterlockedCompareExchange(&g_inside,0,0))
        return;
    InterlockedExchange(&g_running,0);
    MH_DisableHook(g_target);
    MH_RemoveHook(g_target);
    MH_Uninitialize();
    g_target=NULL;g_original=NULL;g_callback=NULL;
    g_callback_user=NULL;g_window=NULL;
}
int esp112_frame_installed(void) {return g_target!=NULL;}
unsigned esp112_frame_callbacks(void) {
    return (unsigned)InterlockedCompareExchange(&g_callbacks,0,0);
}
unsigned esp112_frame_rejected(void) {
    return (unsigned)InterlockedCompareExchange(&g_rejected,0,0);
}
