#ifndef ESP112_OVERLAY_335_H
#define ESP112_OVERLAY_335_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "esp112_geometry.h"
#define ESP112_DIAG_PAIRS 6u
typedef struct {
    HWND hwnd;
    HWND foot_hwnd;
    char caption[96];
    char foot_tag[12];
    int foot_left,foot_top;
    unsigned foot_visible;
    DWORD hp,max_hp;
    COLORREF color;
    int left,top;
    unsigned visible;
} Esp112OverlayLabel;
typedef struct {
    HINSTANCE instance;
    ATOM atom;
    ATOM foot_atom;
    Esp112OverlayLabel labels[ESP112_MAX_LABELS];
    unsigned visible;
} Esp112Overlay;
int esp112_overlay_init(Esp112Overlay *overlay,HINSTANCE instance);
int esp112_overlay_show(Esp112Overlay *overlay,unsigned slot,int left,int top,
                          const char *caption,DWORD hp,DWORD max_hp,COLORREF color);
int esp112_overlay_show_foot(Esp112Overlay *overlay,unsigned slot,int screen_x,
                              int screen_y,const char *short_id);
void esp112_overlay_hide_foot(Esp112Overlay *overlay,unsigned slot);
void esp112_overlay_hide_unused(Esp112Overlay *overlay,unsigned used);
/* Finish the frame by window identity, not the number of sorted rows. */
void esp112_overlay_finish_frame(Esp112Overlay *overlay,unsigned frame_mask);
void esp112_overlay_shutdown(Esp112Overlay *overlay);
#endif
