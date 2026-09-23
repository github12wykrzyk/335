#ifndef WOW335_AUTOPICKPOCKET_CORE_H
#define WOW335_AUTOPICKPOCKET_CORE_H
/* Portable decision engine only; NO addresses, hooks, client ABI or loot calls.
 * A separately verified build-12340 adapter must supply all game interactions.
 */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PP_SCAN_CAP 64u
#define PP_HISTORY_CAP 256u
#define PP_SCAN_INTERVAL_MS 100u
#define PP_RESULT_TIMEOUT_MS 1500u
#define PP_RETRY_DELAY_MS 800u
#define PP_TIMEOUT_DELAY_MS 3000u

typedef struct { uint32_t lo, hi; } PpGuid;
typedef struct {
    PpGuid guid;
    float distance_sq;
    unsigned eligible; /* adapter verified: living, pickpocketable hostile NPC in reach */
} PpTarget;
typedef enum {
    PP_RESULT_PENDING = 0,
    PP_RESULT_SUCCESS = 1, /* positively confirmed Pick Pocket result; NOT loot completion */
    PP_RESULT_EMPTY = 2,   /* positively confirmed no pockets / already pickpocketed */
    PP_RESULT_RETRYABLE = 3, /* temporary range, LOS, stealth, or cast failure */
    PP_RESULT_PERMANENT = 4 /* positively confirmed ineligible for this session */
} PpResult;
typedef enum {
    PP_EVENT_CAST = 1,
    PP_EVENT_SUCCESS = 2,
    PP_EVENT_EMPTY = 3,
    PP_EVENT_RETRY = 4,
    PP_EVENT_TIMEOUT = 5,
    PP_EVENT_INELIGIBLE = 6
} PpEvent;
typedef struct {
    void *ctx;
    /* Game-thread only. Return only verified eligible NPCs, never player/pet/etc. */
    size_t (*scan)(void *ctx, PpTarget *out, size_t cap);
    /* Game-thread only: class, learned spell, usable state and stealth checks. */
    int (*can_cast)(void *ctx);
    /* Cast on EXACT GUID without stealing user's target. 1 = submitted only. */
    int (*cast_on_guid)(void *ctx, PpGuid target);
    /* Correlate exact GUID and attempt to an authoritative result. */
    PpResult (*result)(void *ctx, PpGuid target);
    /* Optional structured event sink; must not print to WoW chat. */
    void (*event)(void *ctx, PpEvent event, PpGuid target);
} PpAdapter;
typedef struct {
    PpGuid guid;
    uint32_t blocked_until_ms;
    unsigned terminal;
    unsigned present;
} PpHistory;
typedef struct {
    PpAdapter api;
    PpHistory history[PP_HISTORY_CAP];
    PpGuid active;
    uint32_t started_ms, last_scan_ms;
    unsigned history_next, active_valid, enabled, scan_started;
    uint32_t casts, successes, empty, retries, timeouts;
} PpEngine;
/* Starts DISABLED; no game interactions without an explicitly enabled adapter. */
int pp_init(PpEngine *engine, PpAdapter api);
void pp_enable(PpEngine *engine, int enable);
/* Clear when world/map/character changes; no implicit reset on movement. */
void pp_reset(PpEngine *engine);
void pp_tick(PpEngine *engine, uint32_t now_ms);
#ifdef __cplusplus
}
#endif
#endif
