#ifndef WOW335_PP_12340_ADAPTER_H
#define WOW335_PP_12340_ADAPTER_H
#include "autopickpocket_core.h"
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PP12340_CLIENT_SHA256 "2236646eca33960431eb1c5331c0b8cce516f2f82e2885c17241b54e92c18c3d"
#define PP12340_CONNECTION_VA ((uintptr_t)0x00C79CE0u)
#define PP12340_MANAGER_OFFSET ((uintptr_t)0x2ED0u)
#define PP12340_CAST_GUID_VA ((uintptr_t)0x0080DA40u)
#define PP12340_POSITION_VA ((uintptr_t)0x006E6F10u)
#define PP12340_SPELL_ID 921u
#define PP12340_UNIT_TYPE 3u
#define PP12340_REACH 4.0f
/* Signed and guarded native callbacks run exclusively in the WoW game thread.
 * This interface does not install hooks or access the AutoLoot UI.
 * No callback may infer cast success from mere submission.
 */
typedef struct {
    void *ctx;
    int (*verify_exe_sha256)(void *, const char *expected);
    int (*verify_cast_abi)(void *, uintptr_t native_cast_va, uintptr_t position_va);
    uint32_t (*thread_id)(void *);
    /* Optional monotonic clock for diagnostic scan-duration measurement. */
    uint32_t (*clock_ms)(void *);
    int (*read_u32)(void *, uintptr_t addr, uint32_t *out);
    int (*position)(void *, uintptr_t unit_obj, float xyz[3]);
    /* Must prove allowed living non-player NPC with native creature type 6/7.
     * Independent hostile/pickpocketability policies may further veto. */
    int (*eligible_npc)(void *, uintptr_t unit_obj, PpGuid guid);
    /* Rogue, learned spell 921, usable, Stealth, no conflicting loot window. */
    int (*spell_usable)(void *, uint32_t spell_id);
    /* Submit native 5-argument cdecl spell request to EXACT GUID. */
    int (*cast_guid)(void *, uintptr_t native_cast_va, uint32_t spell_id, PpGuid guid, uint32_t attempt_id);
    /* Result must be correlated to THIS GUID and cast; unknown => pending. */
    PpResult (*cast_result)(void *, PpGuid guid, uint32_t attempt_id);
    /* Distinct world/character discriminator, 0 if world unavailable. */
    uint64_t (*world_token)(void *);
    void (*event)(void *, PpEvent event, PpGuid guid, uint32_t attempt_id);
} Pp12340Host;
typedef struct {
    PpEngine engine;
    Pp12340Host host;
    uint32_t owner_thread;
    uint64_t current_world;
    uint32_t cached_manager,cached_player_obj;
    uint32_t last_scan_duration_ms,last_scan_candidates;
    unsigned bound;
} Pp12340Adapter;
int pp12340_bind(Pp12340Adapter *a, const Pp12340Host *host);
void pp12340_enable(Pp12340Adapter *a, int enable);
void pp12340_tick(Pp12340Adapter *a, uint32_t now_ms);
void pp12340_reset(Pp12340Adapter *a);
/* Silent slash argument dispatcher; loader must register /appp with WoW. */
int pp12340_command(Pp12340Adapter *a, const char *arguments);
#ifdef __cplusplus
}
#endif
#endif
