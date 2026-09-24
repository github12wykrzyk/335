/* Experimental D3D9 renderer; the same vtable hook must never be installed
 * by a second module. A verified live device in-game is still required.
 * Install/remove on the WoW window thread, not inside DllMain.
 */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "player_esp_d3d9.h"
#pragma comment(lib, "d3d9.lib")
typedef HRESULT (STDMETHODCALLTYPE *EspEndScene)(IDirect3DDevice9 *);
typedef struct { float x,y,z,rhw; DWORD color; } EspVertex;
#define ESP335_BATCH_QUADS 512u
#define ESP335_FRAME_LABELS 96u
#define ESP335_FVF (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)
static void **g_slot;
static EspEndScene g_original;
static Esp335RenderCallback g_callback;
static void *g_user;
static DWORD g_owner_thread;
static volatile LONG g_rendering, g_enabled;
static volatile LONG g_frames, g_dropped;
static EspVertex g_vertices[ESP335_BATCH_QUADS * 6u];
static size_t g_used;
static IDirect3DDevice9 *g_device;
static int g_draw_failed;

static int append_quad(float x, float y, float w, float h, DWORD color) {
    EspVertex *v;
    float x1,y1,x2,y2;
    if (!g_device || g_draw_failed || !isfinite(x) || !isfinite(y) ||
        !isfinite(w) || !isfinite(h) || w<=0.f || h<=0.f) return 0;
    if (g_used+6u > ESP335_BATCH_QUADS*6u) {
        if (FAILED(IDirect3DDevice9_DrawPrimitiveUP(g_device,
                   D3DPT_TRIANGLELIST,(UINT)(g_used/3u),g_vertices,sizeof(EspVertex)))) {
            g_draw_failed=1;
            return 0;
        }
        g_used=0;
    }
    x1=x-0.5f; y1=y-0.5f; x2=x+w-0.5f; y2=y+h-0.5f;
    v=&g_vertices[g_used];
    v[0]=(EspVertex){x1,y1,0.f,1.f,color};
    v[1]=(EspVertex){x2,y1,0.f,1.f,color};
    v[2]=(EspVertex){x1,y2,0.f,1.f,color};
    v[3]=(EspVertex){x2,y1,0.f,1.f,color};
    v[4]=(EspVertex){x2,y2,0.f,1.f,color};
    v[5]=(EspVertex){x1,y2,0.f,1.f,color};
    g_used+=6u;
    return 1;
}
static void seven_segments(float x, float y, unsigned digit, DWORD col) {
    /* 7-segment minimal distance glyphs, avoids a font/text dependency. */
    static const unsigned char mask[10]={0x3f,0x06,0x5b,0x4f,0x66,
                                         0x6d,0x7d,0x07,0x7f,0x6f};
    unsigned m=mask[digit%10u];
    if (m&0x01u) append_quad(x+1.f,y,3.f,1.f,col);
    if (m&0x02u) append_quad(x+4.f,y+1.f,1.f,3.f,col);
    if (m&0x04u) append_quad(x+4.f,y+5.f,1.f,3.f,col);
    if (m&0x08u) append_quad(x+1.f,y+8.f,3.f,1.f,col);
    if (m&0x10u) append_quad(x,y+5.f,1.f,3.f,col);
    if (m&0x20u) append_quad(x,y+1.f,1.f,3.f,col);
    if (m&0x40u) append_quad(x+1.f,y+4.f,3.f,1.f,col);
}
size_t esp335_d3d9_draw_labels(IDirect3DDevice9 *device,
                              const Esp335Label *labels, size_t count) {
    IDirect3DStateBlock9 *block=NULL;
    D3DVIEWPORT9 vp;
    size_t i, drawn=0;
    HRESULT hr;
    if (!device || !labels || !count ||
        FAILED(IDirect3DDevice9_GetViewport(device,&vp)) ||
        vp.Width<64u || vp.Height<64u ||
        FAILED(IDirect3DDevice9_CreateStateBlock(device,D3DSBT_ALL,&block)) ||
        !block) return 0;
    if (FAILED(IDirect3DStateBlock9_Capture(block))) {
        IDirect3DStateBlock9_Release(block);
        return 0;
    }
    g_device=device;g_used=0;g_draw_failed=0;
    IDirect3DDevice9_SetTexture(device,0,NULL);
    IDirect3DDevice9_SetFVF(device,ESP335_FVF);
    IDirect3DDevice9_SetRenderState(device,D3DRS_ZENABLE,FALSE);
    IDirect3DDevice9_SetRenderState(device,D3DRS_ZWRITEENABLE,FALSE);
    IDirect3DDevice9_SetRenderState(device,D3DRS_ALPHABLENDENABLE,TRUE);
    IDirect3DDevice9_SetRenderState(device,D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(device,D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(device,D3DRS_LIGHTING,FALSE);
    IDirect3DDevice9_SetRenderState(device,D3DRS_CULLMODE,D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(device,D3DRS_SCISSORTESTENABLE,FALSE);
    IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_SELECTARG2);
    IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG2);
    IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
    if (count>ESP335_FRAME_LABELS) count=ESP335_FRAME_LABELS;
    for (i=0; i<count && !g_draw_failed; ++i) {
        const Esp335Label *p=&labels[i];
        float x=p->screen_x,y=p->screen_y,health_ratio;
        unsigned distance, hundreds,tens,ones;
        DWORD col=D3DCOLOR_ARGB(235,240,220,90);
        DWORD bg=D3DCOLOR_ARGB(170,9,9,9);
        DWORD hp=D3DCOLOR_ARGB(235,60,210,100);
        if (!p->guid || !isfinite(x) || !isfinite(y) ||
            x<vp.X+8.f || x>vp.X+vp.Width-8.f ||
            y<vp.Y+8.f || y>vp.Y+vp.Height-8.f ||
            !p->max_health || p->health>p->max_health) continue;
        health_ratio=(float)p->health/(float)p->max_health;
        append_quad(x-2.f,y-2.f,5.f,5.f,col);
        append_quad(x-18.f,y-13.f,36.f,5.f,bg);
        append_quad(x-17.f,y-12.f,34.f*health_ratio,3.f,hp);
        distance=(unsigned)fminf(p->distance,999.f);
        hundreds=distance/100u;
        tens=(distance/10u)%10u;
        ones=distance%10u;
        if (hundreds) seven_segments(x-9.f,y-27.f,hundreds,col);
        if (hundreds||tens) seven_segments(x-3.f,y-27.f,tens,col);
        seven_segments(x+3.f,y-27.f,ones,col);
        ++drawn;
    }
    if (g_used && !g_draw_failed) {
        hr=IDirect3DDevice9_DrawPrimitiveUP(device,D3DPT_TRIANGLELIST,
                   (UINT)(g_used/3u),g_vertices,sizeof(EspVertex));
        if (FAILED(hr)) drawn=0;
    }
    if (FAILED(IDirect3DStateBlock9_Apply(block))) drawn=0;
    IDirect3DStateBlock9_Release(block);
    g_device=NULL;g_used=0;
    if (g_draw_failed) return 0;
    return drawn;
}
static HRESULT STDMETHODCALLTYPE end_scene(IDirect3DDevice9 *device) {
    EspEndScene original=g_original;
    if (original && InterlockedCompareExchange(&g_enabled,0,0) &&
        GetCurrentThreadId()==g_owner_thread &&
        InterlockedCompareExchange(&g_rendering,1,0)==0) {
        InterlockedIncrement(&g_frames);
        if (g_callback) g_callback(device,g_user);
        InterlockedExchange(&g_rendering,0);
    } else {
        InterlockedIncrement(&g_dropped);
    }
    return original ? original(device) : D3DERR_INVALIDCALL;
}
static int swap_slot(void **slot, void *expect, void *replace) {
    DWORD old_protection,ignored;
    void *actual;
    if (!slot || !VirtualProtect(slot,sizeof(*slot),PAGE_EXECUTE_READWRITE,
                                 &old_protection)) return 0;
    actual=InterlockedCompareExchangePointer((PVOID volatile *)slot,
                                           replace,expect);
    VirtualProtect(slot,sizeof(*slot),old_protection,&ignored);
    return actual==expect;
}
int esp335_d3d9_install(HWND hwnd, Esp335RenderCallback cb, void *ctx) {
    IDirect3D9 *api;
    IDirect3DDevice9 *dummy=NULL;
    D3DPRESENT_PARAMETERS pp;
    DWORD pid=0;
    void **table;
    void *previous;
    if (!cb || !IsWindow(hwnd) || g_slot || !GetModuleHandleW(L"d3d9.dll"))
        return 0;
    if (GetWindowThreadProcessId(hwnd,&pid)!=GetCurrentThreadId() ||
        pid!=GetCurrentProcessId()) return 0;
    api=Direct3DCreate9(D3D_SDK_VERSION);
    if (!api) return 0;
    memset(&pp,0,sizeof(pp));
    pp.Windowed=TRUE;
    pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow=hwnd;
    pp.BackBufferWidth=1u; pp.BackBufferHeight=1u;
    pp.BackBufferFormat=D3DFMT_UNKNOWN;
    if (FAILED(IDirect3D9_CreateDevice(api,D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,
            &pp,&dummy)) || !dummy) {
        IDirect3D9_Release(api);
        return 0;
    }
    table=*(void ***)dummy;
    previous=table[42];
    if (!previous || previous==(void *)end_scene) {
        IDirect3DDevice9_Release(dummy);
        IDirect3D9_Release(api);
        return 0;
    }
    g_original=(EspEndScene)previous;
    g_owner_thread=GetCurrentThreadId();
    g_callback=cb;
    g_user=ctx;
    if (!swap_slot(&table[42],previous,(void *)end_scene)) {
        g_original=NULL;g_callback=NULL;g_user=NULL;
        IDirect3DDevice9_Release(dummy);
        IDirect3D9_Release(api);
        return 0;
    }
    g_slot=&table[42];
    InterlockedExchange(&g_enabled,1);
    IDirect3DDevice9_Release(dummy);
    IDirect3D9_Release(api);
    return 1;
}
void esp335_d3d9_uninstall(void) {
    if (!g_slot || GetCurrentThreadId()!=g_owner_thread) return;
    InterlockedExchange(&g_enabled,0);
    if (InterlockedCompareExchange(&g_rendering,0,0)!=0) return;
    if (swap_slot(g_slot,(void *)end_scene,(void *)g_original)) {
        g_slot=NULL;g_original=NULL;g_callback=NULL;g_user=NULL;
    }
}
unsigned esp335_d3d9_frames(void) {
    return (unsigned)InterlockedCompareExchange(&g_frames,0,0);
}
unsigned esp335_d3d9_dropped(void) {
    return (unsigned)InterlockedCompareExchange(&g_dropped,0,0);
}
