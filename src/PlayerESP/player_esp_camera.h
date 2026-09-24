/* WoW 3.3.5 camera axes -> tested D3D clip-space projection.
 * External camera reader must validate the exact client's live 3x3 basis.
 * No hard-coded FOV multiplier or pointers in this pure module.
 */
#ifndef PLAYER_ESP_335_CAMERA_H
#define PLAYER_ESP_335_CAMERA_H
#include "player_esp_core.h"
typedef struct {
    Esp335Vec3 eye, forward, up, right;
    float fov_y, near_clip, far_clip, aspect;
    float viewport_x, viewport_y, viewport_width, viewport_height;
} Esp335CameraAxes;
int esp335_camera_build(const Esp335CameraAxes *axes, Esp335Camera *out);
/* Native 12340 W2S writes into WorldFrame render rectangle (+0x330..0x33c).
 * Its +0x64..0x70 rectangle is a CLIP domain, not an output viewport.
 * out_u is left->right, out_v is top->bottom, each in [0,1]. */
int esp335_native_screen_to_ui(float raw_x,float raw_y,
                                const float render_lbrt[4],
                                float *out_u,float *out_v);
#endif
