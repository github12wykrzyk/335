/* The ONLY PlayerESP D3D9 hook owner. Legacy GDI is fallback. */
#ifndef ESP112_FRAME_HOOK_H
#define ESP112_FRAME_HOOK_H
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
typedef void (*Esp112FrameCallback)(IDirect3DDevice9 *device,void *user);
int esp112_frame_install(HWND game_window,Esp112FrameCallback callback,
                         void *user);
void esp112_frame_uninstall(void);
int esp112_frame_installed(void);
unsigned esp112_frame_callbacks(void);
unsigned esp112_frame_rejected(void);
#endif
