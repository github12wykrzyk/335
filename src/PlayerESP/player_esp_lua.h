#ifndef PLAYER_ESP_335_LUA_H
#define PLAYER_ESP_335_LUA_H
#include "player_esp_core.h"
/* Shared AutoLoot game-thread Lua gate; no direct client FrameScript call. */
typedef int (__stdcall *Esp335LuaGate)(const char *script,const char *source);
int esp335_lua_create(Esp335LuaGate run);
int esp335_lua_visibility(Esp335LuaGate run,unsigned visible);
int esp335_lua_update(Esp335LuaGate run,const Esp335Core *core,
                       const Esp335Camera *camera,unsigned scan_ok,
                       unsigned player_count,unsigned npc_count,
                       unsigned camera_ok);
#endif
