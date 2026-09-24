#include "esp112_geometry.h"
#include <math.h>
#include <limits.h>
/* 0x004F6D20 internally calls 0x0047BFF0 and emits UI coordinate units.
 * At 2560x1440, globals 0xAC0CB4=0.87158, 0xAC0CB8=0.49026,
 * so raw (0.18139,0.03538) is viewport (0.2081,0.0722), NOT
 * native DDC again (0.1581,0.01734), NOR bottom-up Y (0.9278). */
int esp112_ui_to_client(float ux,float uy,float scalex,float scaley,
                         const Esp112Viewport *v,int *out_x,int *out_y) {
    float x,y;
    if (!v || !out_x || !out_y || !isfinite(ux) || !isfinite(uy) ||
        !isfinite(scalex) || !isfinite(scaley) ||
        scalex<=0.f || scaley<=0.f || scalex>10000.f || scaley>10000.f ||
        v->width<64 || v->height<64 || v->width>16384 || v->height>16384)
        return 0;
    x=ux/scalex;
    y=uy/scaley;
    if (!isfinite(x) || !isfinite(y) ||
        x<0.f || x>1.f || y<0.f || y>1.f) return 0;
    *out_x=(int)(x*(float)v->width+0.5f);
    *out_y=(int)(y*(float)v->height+0.5f);
    return 1;
}
int esp112_label_rect(int client_x,int client_y,const Esp112Viewport *v,
                       int *left,int *top) {
    if (!v || !left || !top || client_x<0 || client_y<0 ||
        client_x>v->width || client_y>v->height ||
        v->screen_left<INT_MIN+ESP112_LABEL_WIDTH ||
        v->screen_left>INT_MAX-ESP112_LABEL_WIDTH-v->width ||
        v->screen_top<INT_MIN+ESP112_LABEL_HEIGHT ||
        v->screen_top>INT_MAX-ESP112_LABEL_HEIGHT-v->height)
        return 0;
    *left=v->screen_left+client_x-ESP112_LABEL_WIDTH/2;
    *top=v->screen_top+client_y-ESP112_LABEL_TOP_PAD;
    return 1;
}
