#include "esp112_geometry.h"
#include <math.h>
#include <limits.h>
int esp112_ndc_to_client(float nx,float ny,const Esp112Viewport *v,
                          int *out_x,int *out_y) {
    float x,y;
    if (!v || !out_x || !out_y || !isfinite(nx) || !isfinite(ny) ||
        v->width<64 || v->height<64 || v->width>16384 || v->height>16384 ||
        nx<0.f || nx>1.f || ny<0.f || ny>1.f) return 0;
    /* Identical order to 112's DdcToNdc + game client pixel projection:
     * x=NDC.x*width; y=height-NDC.y*height. No Lua effective scale. */
    x=nx*(float)v->width;
    y=(float)v->height-ny*(float)v->height;
    if (!isfinite(x) || !isfinite(y) || x<0.f || y<0.f ||
        x>(float)v->width || y>(float)v->height) return 0;
    *out_x=(int)(x+0.5f);
    *out_y=(int)(y+0.5f);
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
