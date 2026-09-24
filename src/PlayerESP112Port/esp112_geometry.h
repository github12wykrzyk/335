/* 12340-specific port of 112's native W2S + per-label Windows overlay.
 * Unlike 112, native 12340 WorldToScreen CALLS DdcToNdc internally.
 * Its raw output is already in native UI coordinate units. Do NOT call
 * native DDC again. Undo the UI scale and convert TOP-LEFT to client pixels.
 */
#ifndef ESP112_GEOMETRY_335_H
#define ESP112_GEOMETRY_335_H
#define ESP112_LABEL_WIDTH 360
#define ESP112_LABEL_HEIGHT 58
#define ESP112_LABEL_TOP_PAD 36
#define ESP112_MAX_LABELS 32u
typedef struct {
    int client_x, client_y;
    int screen_left, screen_top;
    int width, height;
} Esp112Viewport;
int esp112_ui_to_client(float ui_x,float ui_y,float native_scale_x,
                         float native_scale_y,const Esp112Viewport *view,
                         int *out_client_x,int *out_client_y);
int esp112_label_rect(int client_x,int client_y,const Esp112Viewport *view,
                       int *left,int *top);
#endif
