/* 12340 object-list observer; no new game hooks or client calls here.
 * Host callbacks must validate exact Wow.exe and run on the game thread.
 */
#ifndef PLAYER_ESP_335_SCANNER_H
#define PLAYER_ESP_335_SCANNER_H
#include "player_esp_core.h"
#include <stdint.h>
#include <stddef.h>
#define ESP335_EXACT_EXE_SHA256 "2236646eca33960431eb1c5331c0b8cce516f2f82e2885c17241b54e92c18c3d"
#define ESP335_CONNECTION_VA ((uintptr_t)0x00C79CE0u)
#define ESP335_MANAGER_OFFSET ((uintptr_t)0x2ED0u)
#define ESP335_MGR_FIRST ((uintptr_t)0xACu)
#define ESP335_MGR_LOCAL_GUID ((uintptr_t)0xC0u)
#define ESP335_OBJ_TYPE ((uintptr_t)0x14u)
#define ESP335_OBJ_GUID ((uintptr_t)0x30u)
#define ESP335_OBJ_NEXT ((uintptr_t)0x3Cu)
#define ESP335_OBJ_PLAYER 4u
#define ESP335_OBJ_NPC 3u

typedef struct {
    void *context;
    int (*verify_client_sha256)(void *, const char *);
    int (*verify_layout)(void *, uintptr_t, uintptr_t); /* manager / first-object ABI */
    uint32_t (*thread_id)(void *);
    int (*read_u32)(void *, uintptr_t, uint32_t *);
    int (*position)(void *, uintptr_t, Esp335Vec3 *);
    int (*player_metadata)(void *, uintptr_t, Esp335Player *);
    uint64_t (*world_epoch)(void *); /* nonzero and changes on relog, map/instance */
} Esp335ScannerHost;

typedef struct {
    Esp335Core snapshot;
    Esp335ScannerHost host;
    uint32_t game_thread;
    unsigned bound;
    unsigned seen_players, seen_npcs, accepted_players, accepted_npcs;
    unsigned position_failures, metadata_failures, scan_failures;
} Esp335Scanner;

int esp335_scanner_bind(Esp335Scanner *, const Esp335ScannerHost *);
int esp335_scanner_collect(Esp335Scanner *); /* 1: complete, 0: invalidated */
void esp335_scanner_unbind(Esp335Scanner *);

#endif
