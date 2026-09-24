/* One process-wide D3D9 EndScene vtable owner for experimental PlayerESP.
 * No patch to Wow.exe, no input or targeting hooks. Dedicated stage only.
 */
#ifndef PLAYER_ESP_335_D3D9_H
#define PLAYER_ESP_335_D3D9_H
#include <windows.h>
#include <d3d9.h>
#include "player_esp_core.h"
typedef void (*Esp335RenderCallback)(IDirect3DDevice9 *, void *);
int esp335_d3d9_install(HWND hwnd, Esp335RenderCallback callback, void *user);
void esp335_d3d9_uninstall(void);
int esp335_d3d9_installed(void);
unsigned esp335_d3d9_frames(void);
unsigned esp335_d3d9_dropped(void);
size_t esp335_d3d9_draw_labels(IDirect3DDevice9 *device,
                              const Esp335Label *labels, size_t count);
#endif
