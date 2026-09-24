#ifndef ESP112_FRAME_DRAW_H
#define ESP112_FRAME_DRAW_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <stdint.h>
#include <stddef.h>
typedef struct {
    float client_x,client_y;
    uint64_t guid;
    unsigned kind,faction,health,max_health,distance_yards;
} Esp112FrameLabel;
/* A SINGLE state-isolated submission for all labels during game's
 * verified D3D9 Present. No cached D3DPOOL_DEFAULT resources, reset safe. */
unsigned esp112_frame_draw(IDirect3DDevice9 *device,
                           const Esp112FrameLabel *labels,size_t count);
unsigned esp112_frame_draw_success(void);
#endif
