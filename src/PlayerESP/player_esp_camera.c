#include "player_esp_camera.h"
#include <math.h>
#include <string.h>

static float dot(Esp335Vec3 a, Esp335Vec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}
static int basis(Esp335Vec3 v) {
    float len = dot(v,v);
    return isfinite(len) && len > 0.8f && len < 1.2f;
}
int esp335_camera_build(const Esp335CameraAxes *a, Esp335Camera *out) {
    float hx, hy, q, xy, yz, zx;
    float *m;
    if (!a || !out || !basis(a->forward) || !basis(a->right) ||
        !basis(a->up) || !isfinite(a->eye.x) ||
        !isfinite(a->eye.y) || !isfinite(a->eye.z) ||
        !isfinite(a->fov_y) || a->fov_y < 0.3f || a->fov_y > 2.8f ||
        !isfinite(a->aspect) || a->aspect < 0.5f || a->aspect > 6.f ||
        !isfinite(a->near_clip) || a->near_clip <= 0.001f ||
        !isfinite(a->far_clip) || a->far_clip <= a->near_clip ||
        a->far_clip > 100000.f ||
        !isfinite(a->viewport_x) || !isfinite(a->viewport_y) ||
        !isfinite(a->viewport_width) || !isfinite(a->viewport_height) ||
        a->viewport_width < 64.f || a->viewport_height < 64.f ||
        a->viewport_width > 16384.f || a->viewport_height > 16384.f)
        return 0;
    xy = dot(a->forward,a->right);
    yz = dot(a->forward,a->up);
    zx = dot(a->right,a->up);
    if (fabsf(xy) > 0.15f || fabsf(yz) > 0.15f || fabsf(zx) > 0.15f)
        return 0;
    hy = 1.f / tanf(a->fov_y * 0.5f);
    hx = hy / a->aspect;
    q = a->far_clip / (a->far_clip - a->near_clip);
    if (!isfinite(hx) || !isfinite(hy) || !isfinite(q))
        return 0;
    memset(out,0,sizeof(*out));
    m=out->view_projection;
    m[0]=a->right.x*hx; m[1]=a->right.y*hx; m[2]=a->right.z*hx;
    m[3]=-dot(a->right,a->eye)*hx;
    m[4]=a->up.x*hy; m[5]=a->up.y*hy; m[6]=a->up.z*hy;
    m[7]=-dot(a->up,a->eye)*hy;
    m[8]=a->forward.x*q; m[9]=a->forward.y*q; m[10]=a->forward.z*q;
    m[11]=-(dot(a->forward,a->eye)+a->near_clip)*q;
    m[12]=a->forward.x; m[13]=a->forward.y; m[14]=a->forward.z;
    m[15]=-dot(a->forward,a->eye);
    out->viewport_x=a->viewport_x; out->viewport_y=a->viewport_y;
    out->viewport_w=a->viewport_width;
    out->viewport_h=a->viewport_height;
    out->local_position=a->eye;
    return 1;
}

/* Exact-client 12340 native WorldToScreen uses +0x330..0x33c to
 * transform to output coordinates. +0x64..0x70 are CLIP extents and
 * must never be used a second time to scale the output. */
int esp335_native_screen_to_ui(float raw_x,float raw_y,
                                const float render_lbrt[4],
                                float *out_u,float *out_v) {
    float left,bottom,right,top,x,y;
    if (!render_lbrt || !out_u || !out_v ||
        !isfinite(raw_x) || !isfinite(raw_y)) return 0;
    left=render_lbrt[0];bottom=render_lbrt[1];
    right=render_lbrt[2];top=render_lbrt[3];
    if (!isfinite(left) || !isfinite(bottom) ||
        !isfinite(right) || !isfinite(top) ||
        right-left<0.00001f || top-bottom<0.00001f) return 0;
    x=(raw_x-left)/(right-left);
    y=(raw_y-bottom)/(top-bottom);
    if (!isfinite(x) || !isfinite(y) || x<0.f || x>1.f ||
        y<0.f || y>1.f) return 0;
    *out_u=x;
    *out_v=1.f-y;
    return 1;
}
