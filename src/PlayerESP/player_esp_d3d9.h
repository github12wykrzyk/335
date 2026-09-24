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
/* Shared layout: panel x=18..262, row y=62 + 24*i (9 controls).
 * UI bits are plain data; only game-thread message hook mutates them.
 */
#define ESP335_GUI_ESP       0x001u
#define ESP335_GUI_PLAYERS   0x002u
#define ESP335_GUI_HORDE     0x004u
#define ESP335_GUI_ALLIANCE  0x008u
#define ESP335_GUI_HOSTILE   0x010u
#define ESP335_GUI_BG_ENEMY  0x020u
#define ESP335_GUI_NPC       0x040u
#define ESP335_GUI_NPC_ENEMY 0x080u
#define ESP335_GUI_UNKNOWN   0x100u
#define ESP335_GUI_DEFAULT (ESP335_GUI_ESP|ESP335_GUI_PLAYERS|ESP335_GUI_UNKNOWN)
int esp335_d3d9_panel_hit(float x, float y);
void esp335_d3d9_draw_panel(IDirect3DDevice9 *device, unsigned flags,
                           unsigned scan_ok, unsigned players,
                           unsigned npcs, unsigned camera_ok,
                           unsigned markers, unsigned dropped);
unsigned esp335_d3d9_frames(void);
unsigned esp335_d3d9_dropped(void);
size_t esp335_d3d9_draw_labels(IDirect3DDevice9 *device,
                              const Esp335Label *labels, size_t count);
#endif
