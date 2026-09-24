/* The same OS/window coordinate space as 112's pooled per-label windows.
 * Distinct popup windows avoid the inert dummy-device D3D9 EndScene hook and
 * UIParent/WorldFrame scale mismatch in the earlier 335 Lua renderer.
 * In-game rendering still MUST be verified in the exact client (windowed /
 * borderless required for desktop layered windows).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "esp112_overlay.h"
#define ESP112_CLASS "Wow335_112StyleESP_Label_12340"
#define ESP112_FOOT_CLASS "Wow335_112StyleESP_FootProbe_12340"
#define ESP112_FOOT_W 64
#define ESP112_FOOT_H 25
static LRESULT CALLBACK label_window(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    Esp112OverlayLabel *label=(Esp112OverlayLabel *)GetWindowLongPtrA(hwnd,GWLP_USERDATA);
    if (msg==WM_NCCREATE) {
        const CREATESTRUCTA *create=(const CREATESTRUCTA *)l;
        SetWindowLongPtrA(hwnd,GWLP_USERDATA,(LONG_PTR)create->lpCreateParams);
        return TRUE;
    }
    if (msg==WM_NCHITTEST) return HTTRANSPARENT;
    if (msg==WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (msg==WM_ERASEBKGND) return TRUE;
    if (msg==WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc=BeginPaint(hwnd,&ps);
        RECT all={0,0,ESP112_LABEL_WIDTH,ESP112_LABEL_HEIGHT},r;
        HBRUSH black=(HBRUSH)GetStockObject(BLACK_BRUSH);
        char health[72];
        HGDIOBJ prev;
        if (!dc) return 0;
        FillRect(dc,&all,black); /* RGB(0,0,0) is the transparency key. */
        if (label) {
            SetBkMode(dc,TRANSPARENT);
            prev=SelectObject(dc,GetStockObject(DEFAULT_GUI_FONT));
            SetTextColor(dc,RGB(24,24,24));
            r=all;r.top=5;r.bottom=24;
            OffsetRect(&r,1,1);
            DrawTextA(dc,label->caption,-1,&r,DT_CENTER|DT_SINGLELINE|DT_NOPREFIX);
            OffsetRect(&r,-1,-1);
            SetTextColor(dc,label->color);
            DrawTextA(dc,label->caption,-1,&r,DT_CENTER|DT_SINGLELINE|DT_NOPREFIX);
            if (label->max_hp && label->hp<=label->max_hp) {
                _snprintf_s(health,sizeof(health),_TRUNCATE,
                    "HP %lu/%lu", (unsigned long)label->hp,
                    (unsigned long)label->max_hp);
                r=all;r.top=24;r.bottom=44;
                SetTextColor(dc,RGB(245,225,93));
                DrawTextA(dc,health,-1,&r,DT_CENTER|DT_SINGLELINE|DT_NOPREFIX);
            }
            SelectObject(dc,prev);
        }
        EndPaint(hwnd,&ps);
        return 0;
    }
    if (msg==WM_DESTROY) return 0;
    return DefWindowProcA(hwnd,msg,w,l);
}

static LRESULT CALLBACK foot_window(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    Esp112OverlayLabel *label=(Esp112OverlayLabel *)GetWindowLongPtrA(hwnd,GWLP_USERDATA);
    (void)w;
    if (msg==WM_NCCREATE) {
        const CREATESTRUCTA *create=(const CREATESTRUCTA *)l;
        SetWindowLongPtrA(hwnd,GWLP_USERDATA,(LONG_PTR)create->lpCreateParams);
        return TRUE;
    }
    if (msg==WM_NCHITTEST) return HTTRANSPARENT;
    if (msg==WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (msg==WM_ERASEBKGND) return TRUE;
    if (msg==WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc=BeginPaint(hwnd,&ps);
        RECT all={0,0,ESP112_FOOT_W,ESP112_FOOT_H},r={21,0,63,25};
        HPEN pen;
        HGDIOBJ previous_pen;
        if (!dc) return 0;
        FillRect(dc,&all,(HBRUSH)GetStockObject(BLACK_BRUSH));
        pen=CreatePen(PS_SOLID,2,RGB(30,255,100));
        if (pen) {
            previous_pen=SelectObject(dc,pen);
            MoveToEx(dc,9,8,NULL);LineTo(dc,19,18);
            MoveToEx(dc,9,18,NULL);LineTo(dc,19,8);
            SelectObject(dc,previous_pen);
            DeleteObject(pen);
        }
        if (label) {
            SetBkMode(dc,TRANSPARENT);
            SetTextColor(dc,RGB(30,255,100));
            DrawTextA(dc,label->foot_tag,-1,&r,DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);
        }
        EndPaint(hwnd,&ps);
        return 0;
    }
    return DefWindowProcA(hwnd,msg,w,l);
}

int esp112_overlay_init(Esp112Overlay *o,HINSTANCE instance) {
    WNDCLASSA wc;
    if (!o || !instance) return 0;
    if (o->atom) return 1;
    memset(&wc,0,sizeof(wc));
    wc.lpfnWndProc=label_window;
    wc.hInstance=instance;
    wc.lpszClassName=ESP112_CLASS;
    o->atom=RegisterClassA(&wc);
    if (!o->atom) return 0; /* Do not subclass an unrelated owner. */
    wc.lpfnWndProc=foot_window;
    wc.lpszClassName=ESP112_FOOT_CLASS;
    o->foot_atom=RegisterClassA(&wc);
    if (!o->foot_atom) {
        UnregisterClassA(ESP112_CLASS,instance);
        o->atom=0;
        return 0;
    }
    o->instance=instance;
    return 1;
}
int esp112_overlay_show(Esp112Overlay *o,unsigned i,int x,int y,
                          const char *caption,DWORD hp,DWORD max_hp,
                          COLORREF color) {
    Esp112OverlayLabel *label;
    if (!o || !o->atom || i>=ESP112_MAX_LABELS || !caption) return 0;
    label=&o->labels[i];
    if (!label->hwnd) {
        label->hwnd=CreateWindowExA(
            WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_LAYERED|
            WS_EX_NOACTIVATE|WS_EX_TRANSPARENT,ESP112_CLASS,"",WS_POPUP,
            x,y,ESP112_LABEL_WIDTH,ESP112_LABEL_HEIGHT,NULL,NULL,
            o->instance,label);
        if (!label->hwnd) return 0;
        if (!SetLayeredWindowAttributes(label->hwnd,RGB(0,0,0),255u,
                                         LWA_COLORKEY)) {
            DestroyWindow(label->hwnd);
            label->hwnd=NULL;
            return 0;
        }
    }
    if (strcmp(label->caption,caption) || label->hp!=hp ||
        label->max_hp!=max_hp || label->color!=color) {
        strncpy_s(label->caption,sizeof(label->caption),caption,_TRUNCATE);
        label->hp=hp;label->max_hp=max_hp;label->color=color;
        InvalidateRect(label->hwnd,NULL,FALSE);
    }
    if (!label->visible || label->left!=x || label->top!=y) {
        if (!SetWindowPos(label->hwnd,NULL,x,y,0,0,
                          SWP_NOZORDER|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW))
            return 0;
        label->left=x;label->top=y;
        label->visible=1u;
    }
    if (i+1u>o->visible) o->visible=i+1u;
    return 1;
}

int esp112_overlay_show_foot(Esp112Overlay *o,unsigned i,int sx,int sy,
                              const char *short_id) {
    Esp112OverlayLabel *label;
    int left,top;
    if (!o || !o->foot_atom || i>=ESP112_DIAG_PAIRS || !short_id)
        return 0;
    label=&o->labels[i];
    left=sx-14;top=sy-12;
    if (!label->foot_hwnd) {
        label->foot_hwnd=CreateWindowExA(
            WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_LAYERED|
            WS_EX_NOACTIVATE|WS_EX_TRANSPARENT,ESP112_FOOT_CLASS,"",
            WS_POPUP,left,top,ESP112_FOOT_W,ESP112_FOOT_H,NULL,NULL,
            o->instance,label);
        if (!label->foot_hwnd) return 0;
        if (!SetLayeredWindowAttributes(label->foot_hwnd,RGB(0,0,0),255u,
                                         LWA_COLORKEY)) {
            DestroyWindow(label->foot_hwnd);
            label->foot_hwnd=NULL;
            return 0;
        }
    }
    if (strcmp(label->foot_tag,short_id)) {
        strncpy_s(label->foot_tag,sizeof(label->foot_tag),short_id,_TRUNCATE);
        InvalidateRect(label->foot_hwnd,NULL,FALSE);
    }
    if (!label->foot_visible || label->foot_left!=left || label->foot_top!=top) {
        if (!SetWindowPos(label->foot_hwnd,NULL,left,top,0,0,
                          SWP_NOZORDER|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW))
            return 0;
        label->foot_left=left;label->foot_top=top;
        label->foot_visible=1u;
    }
    return 1;
}
void esp112_overlay_hide_foot(Esp112Overlay *o,unsigned i) {
    Esp112OverlayLabel *label;
    if (!o || i>=ESP112_MAX_LABELS) return;
    label=&o->labels[i];
    if (label->foot_hwnd && label->foot_visible) {
        ShowWindow(label->foot_hwnd,SW_HIDE);
        label->foot_visible=0u;
    }
}

void esp112_overlay_hide_unused(Esp112Overlay *o,unsigned used) {
    unsigned i;
    if (!o) return;
    if (used>ESP112_MAX_LABELS) used=ESP112_MAX_LABELS;
    for (i=used;i<ESP112_MAX_LABELS;++i) {
        Esp112OverlayLabel *label=&o->labels[i];
        esp112_overlay_hide_foot(o,i);
        if (label->hwnd && label->visible) {
            ShowWindow(label->hwnd,SW_HIDE);
            label->visible=0u;
        }
    }
    o->visible=used;
}
void esp112_overlay_finish_frame(Esp112Overlay *o,unsigned frame_mask) {
    unsigned i,visible=0u;
    if (!o) return;
    for (i=0u;i<ESP112_MAX_LABELS;++i) {
        Esp112OverlayLabel *label=&o->labels[i];
        if (frame_mask & (1u<<i)) {
            ++visible;
            continue;
        }
        esp112_overlay_hide_foot(o,i);
        if (label->hwnd && label->visible) {
            ShowWindow(label->hwnd,SW_HIDE);
            label->visible=0u;
        }
    }
    o->visible=visible;
}
void esp112_overlay_shutdown(Esp112Overlay *o) {
    unsigned i;
    if (!o) return;
    for (i=0u;i<ESP112_MAX_LABELS;++i) {
        if (o->labels[i].foot_hwnd) {
            DestroyWindow(o->labels[i].foot_hwnd);
            o->labels[i].foot_hwnd=NULL;
        }
        if (o->labels[i].hwnd) {
            DestroyWindow(o->labels[i].hwnd);
            o->labels[i].hwnd=NULL;
        }
    }
    if (o->foot_atom) UnregisterClassA(ESP112_FOOT_CLASS,o->instance);
    if (o->atom) UnregisterClassA(ESP112_CLASS,o->instance);
    memset(o,0,sizeof(*o));
}
