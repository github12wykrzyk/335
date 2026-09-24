/* Small adaptive screen-space low-pass for a GUID-owned overlay slot.
 * Zero allocations, stable across fluctuating Win32 message intervals.
 * Never predict past a camera teleport or reuse state for another GUID.
 */
#ifndef ESP112_MOTION_H
#define ESP112_MOTION_H
#include <stdint.h>
typedef struct {
    uint64_t guid;
    float x,y;
    uint32_t timestamp_ms;
    unsigned valid;
} Esp112Motion;
void esp112_motion_reset(Esp112Motion *motion);
int esp112_motion_step(Esp112Motion *motion,uint64_t guid,
                       float target_x,float target_y,uint32_t now_ms,
                       int *out_x,int *out_y);
#endif
