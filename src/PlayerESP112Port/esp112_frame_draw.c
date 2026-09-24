/* Single frame-matched D3D9 label batch. No D3DX, GDI HWND or timers. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "esp112_frame_draw.h"
#pragma comment(lib,"d3d9.lib")
#define VCAP (8192u*6u)
typedef struct { float x,y,z,rhw; DWORD color; } EspVertex;
static EspVertex vertices[VCAP];
static size_t used;
static IDirect3DDevice9 *current;
static unsigned failed;
static int flush(void) {
    if (failed || !used) return failed?0:1;
    if (FAILED(IDirect3DDevice9_DrawPrimitiveUP(current,D3DPT_TRIANGLELIST,
              (UINT)(used/3u),vertices,sizeof(vertices[0])))) {
        failed=1;
        return 0;
    }
    used=0u;
    return 1;
}
static void quad(float x,float y,float w,float h,DWORD color) {
    EspVertex *v;
    const float x1=x-0.5f,y1=y-0.5f,x2=x+w-0.5f,y2=y+h-0.5f;
    if (failed || !current || !isfinite(x) || !isfinite(y) ||
        !isfinite(w) || !isfinite(h) || w<=0.f || h<=0.f) return;
    if (used+6u>VCAP && !flush()) return;
    v=&vertices[used];
    v[0]=(EspVertex){x1,y1,0.f,1.f,color};
    v[1]=(EspVertex){x2,y1,0.f,1.f,color};
    v[2]=(EspVertex){x1,y2,0.f,1.f,color};
    v[3]=(EspVertex){x2,y1,0.f,1.f,color};
    v[4]=(EspVertex){x2,y2,0.f,1.f,color};
    v[5]=(EspVertex){x1,y2,0.f,1.f,color};
    used+=6u;
}
typedef struct { char chr; unsigned char rows[7]; } GuiGlyph;
static const GuiGlyph g_glyphs[] = {
    {'0',{0x0e,0x13,0x15,0x15,0x15,0x19,0x0e}},
    {'1',{0x04,0x0c,0x04,0x04,0x04,0x04,0x0e}},
    {'2',{0x0e,0x11,0x01,0x02,0x04,0x08,0x1f}},
    {'3',{0x1e,0x01,0x01,0x0e,0x01,0x01,0x1e}},
    {'4',{0x02,0x06,0x0a,0x12,0x1f,0x02,0x02}},
    {'5',{0x1f,0x10,0x10,0x1e,0x01,0x01,0x1e}},
    {'6',{0x0f,0x10,0x10,0x1e,0x11,0x11,0x0e}},
    {'7',{0x1f,0x01,0x02,0x04,0x08,0x08,0x08}},
    {'8',{0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e}},
    {'9',{0x0e,0x11,0x11,0x0f,0x01,0x01,0x1e}},
    {'A',{0x0e,0x11,0x11,0x1f,0x11,0x11,0x11}},
    {'B',{0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e}},
    {'C',{0x0f,0x10,0x10,0x10,0x10,0x10,0x0f}},
    {'D',{0x1e,0x11,0x11,0x11,0x11,0x11,0x1e}},
    {'E',{0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f}},
    {'F',{0x1f,0x10,0x10,0x1e,0x10,0x10,0x10}},
    {'G',{0x0f,0x10,0x10,0x17,0x11,0x11,0x0f}},
    {'H',{0x11,0x11,0x11,0x1f,0x11,0x11,0x11}},
    {'I',{0x1f,0x04,0x04,0x04,0x04,0x04,0x1f}},
    {'J',{0x07,0x02,0x02,0x02,0x12,0x12,0x0c}},
    {'K',{0x11,0x12,0x14,0x18,0x14,0x12,0x11}},
    {'L',{0x10,0x10,0x10,0x10,0x10,0x10,0x1f}},
    {'M',{0x11,0x1b,0x15,0x15,0x11,0x11,0x11}},
    {'N',{0x11,0x19,0x15,0x13,0x11,0x11,0x11}},
    {'O',{0x0e,0x11,0x11,0x11,0x11,0x11,0x0e}},
    {'P',{0x1e,0x11,0x11,0x1e,0x10,0x10,0x10}},
    {'Q',{0x0e,0x11,0x11,0x11,0x15,0x12,0x0d}},
    {'R',{0x1e,0x11,0x11,0x1e,0x14,0x12,0x11}},
    {'S',{0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e}},
    {'T',{0x1f,0x04,0x04,0x04,0x04,0x04,0x04}},
    {'U',{0x11,0x11,0x11,0x11,0x11,0x11,0x0e}},
    {'V',{0x11,0x11,0x11,0x11,0x11,0x0a,0x04}},
    {'W',{0x11,0x11,0x11,0x15,0x15,0x15,0x0a}},
    {'X',{0x11,0x11,0x0a,0x04,0x0a,0x11,0x11}},
    {'Y',{0x11,0x11,0x0a,0x04,0x04,0x04,0x04}},
    {'Z',{0x1f,0x01,0x02,0x04,0x08,0x10,0x1f}},
    {':',{0x00,0x04,0x04,0x00,0x04,0x04,0x00}},
    {' ',{0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    {'-',{0x00,0x00,0x00,0x1f,0x00,0x00,0x00}},
    {'/',{0x01,0x01,0x02,0x04,0x08,0x10,0x10}},
    {'*',{0x00,0x15,0x0e,0x1f,0x0e,0x15,0x00}},
    {'?',{0x0e,0x11,0x01,0x02,0x04,0x00,0x04}}
};

static void glyph(float x,float y,char ch,DWORD color) {
    const unsigned char *bits=NULL;
    unsigned i,row,col;
    if (ch>='a' && ch<='z') ch=(char)(ch-32);
    for (i=0u;i<sizeof(g_glyphs)/sizeof(g_glyphs[0]);++i)
        if (g_glyphs[i].chr==ch) {bits=g_glyphs[i].rows;break;}
    if (!bits) return;
    /* Horizontal runs instead of pixel-by-pixel quads reduce D3D calls. */
    for (row=0u;row<7u;++row) {
        col=0u;
        while (col<5u) {
            unsigned start=col;
            while (col<5u && !(bits[row]&(1u<<(4u-col)))) ++col;
            start=col;
            while (col<5u && (bits[row]&(1u<<(4u-col)))) ++col;
            if (col>start)
                quad(x+(float)start*1.7f,y+(float)row*1.7f,
                     (float)(col-start)*1.7f,1.6f,color);
        }
    }
}
static void text(float x,float y,const char *msg,DWORD color) {
    unsigned n=0u;
    while (*msg && n++<56u) {
        glyph(x,y,*msg++,color);
        x+=10.f;
    }
}
unsigned esp112_frame_draw(IDirect3DDevice9 *device,
                           const Esp112FrameLabel *labels,size_t count) {
    IDirect3DStateBlock9 *state=NULL;
    D3DVIEWPORT9 vp;
    size_t i;
    unsigned drawn=0u;
    if (!device || (!labels && count) ||
        FAILED(IDirect3DDevice9_GetViewport(device,&vp)) ||
        vp.Width<64u || vp.Height<64u || vp.Width>16384u ||
        vp.Height>16384u ||
        FAILED(IDirect3DDevice9_CreateStateBlock(device,D3DSBT_ALL,&state)) ||
        !state) return 0u;
    if (FAILED(IDirect3DStateBlock9_Capture(state))) {
        IDirect3DStateBlock9_Release(state);
        return 0u;
    }
    current=device;used=0u;failed=0u;
    IDirect3DDevice9_SetVertexShader(device,NULL);
    IDirect3DDevice9_SetPixelShader(device,NULL);
    IDirect3DDevice9_SetTexture(device,0,NULL);
    IDirect3DDevice9_SetFVF(device,D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
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
    if (count>32u) count=32u;
    for (i=0u;i<count&&!failed;++i) {
        const Esp112FrameLabel *p=&labels[i];
        char name[48],health[36];
        DWORD col,bg=D3DCOLOR_ARGB(156,9,13,20);
        float x=p->client_x+(float)vp.X,y=p->client_y+(float)vp.Y;
        float percent;
        if (!p->guid || !isfinite(x)||!isfinite(y) ||
            x<(float)vp.X+75.f || x>(float)(vp.X+vp.Width)-75.f ||
            y<(float)vp.Y+24.f || y>(float)(vp.Y+vp.Height)-25.f ||
            !p->max_health || p->health>p->max_health) continue;
        col=p->kind==3u?D3DCOLOR_ARGB(245,80,205,250):
            p->faction==1u?D3DCOLOR_ARGB(245,235,73,73):
            p->faction==2u?D3DCOLOR_ARGB(245,96,160,252):
                D3DCOLOR_ARGB(245,237,232,150);
        _snprintf_s(name,sizeof(name),_TRUNCATE,"%s %04X %uY",
                    p->kind==3u?"NPC":"PLAYER",(unsigned)(p->guid&0xffffu),
                    p->distance_yards);
        _snprintf_s(health,sizeof(health),_TRUNCATE,"HP %u/%u",
                    p->health,p->max_health);
        percent=(float)p->health/(float)p->max_health;
        quad(x-79.f,y-36.f,158.f,34.f,bg);
        text(x-(float)strlen(name)*5.f,y-32.f,name,col);
        quad(x-43.f,y-15.f,86.f,5.f,D3DCOLOR_ARGB(240,20,20,20));
        quad(x-42.f,y-14.f,84.f*percent,3.f,D3DCOLOR_ARGB(240,78,211,91));
        text(x-(float)strlen(health)*5.f,y-8.f,health,
             D3DCOLOR_ARGB(240,242,230,110));
        ++drawn;
    }
    if (!flush()) drawn=0u;
    if (FAILED(IDirect3DStateBlock9_Apply(state))) drawn=0u;
    IDirect3DStateBlock9_Release(state);
    current=NULL;used=0u;
    return drawn;
}
