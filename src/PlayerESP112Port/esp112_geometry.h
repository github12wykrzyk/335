/* WoW 3.3.5a (12340) adaptation of the two-stage native 112 projection.
 * W2S returns device-domain floats. The separate native DDC -> NDC
 * conversion happens BEFORE this portable viewport-to-pixels step.
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
int esp112_ndc_to_client(float ndc_x,float ndc_y,const Esp112Viewport *view,
                          int *out_client_x,int *out_client_y);
int esp112_label_rect(int client_x,int client_y,const Esp112Viewport *view,
                       int *left,int *top);
#endif
