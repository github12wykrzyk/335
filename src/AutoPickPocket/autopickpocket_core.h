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
#define PP_SCAN_INTERVAL_MS 60u
#define PP_QUEUE_TTL_MS 160u
#define PP_RESULT_TIMEOUT_MS 200u /* retained for the single-target probe only */
#define PP_BURST_PENDING_CAP 32u
#define PP_BURST_MIN_SEND_MS 80u /* one packet per pulse and bounded rate */
#define PP_BURST_OBSERVE_MS 900u /* independent asynchronous result window */
#define PP_BURST_RESULT_POLL_MS 80u
#define PP_BURST_RANGE_RELEASE_MS 350u /* allow typical late ACK before range-based release */
#define PP_BURST_UNKNOWN_BACKOFF_MS 200u /* avoid multisecond UNKNOWN GUID lockout */
#define PP_RETRY_DELAY_MS 800u
#define PP_RANGE_RETRY_DELAY_MS 200u /* correlated GUID or independently checked UI range hint */
#define PP_LOCAL_RANGE_BACKOFF_MS 250u
#define PP_LOCAL_FAILOVER_LIMIT 4u
#define PP_CAST_LOCAL_RANGE (-1) /* no packet submitted: try another GUID */
#define PP_TIMEOUT_DELAY_MS 3000u
#define PP_MAX_ATTEMPTS_PER_GUID 3u

typedef struct { uint32_t lo, hi; } PpGuid;
typedef struct {
    PpGuid guid;
    float distance_sq;
    unsigned eligible; /* 1=in verified cast range, 2=detected nearby, not yet in cast range; 0=invalid */
    unsigned forward; /* fresh native player-motion direction; 0 if stationary/uncertain */
} PpTarget;
typedef enum {
    PP_RESULT_PENDING = 0,
    PP_RESULT_SUCCESS = 1, /* positively confirmed Pick Pocket result; NOT loot completion */
    PP_RESULT_EMPTY = 2,   /* positively confirmed no pockets / already pickpocketed */
    PP_RESULT_RETRYABLE = 3, /* temporary range, LOS, stealth, or cast failure */
    PP_RESULT_PERMANENT = 4, /* positively confirmed ineligible for this session */
    PP_RESULT_MONEY_SUCCESS = 5, /* wallet delta plus same-attempt loot event: indicative */
    PP_RESULT_OUT_OF_RANGE = 6, PP_RESULT_LINE_OF_SIGHT = 7,
    PP_RESULT_NOT_STEALTHED = 8, PP_RESULT_NOT_READY = 9,
    PP_RESULT_CAST_REJECTED = 10, /* explicit failure without classified reason */
    PP_RESULT_UI_RANGE_HINT = 11, /* global UI hint + native same-GUID distance recheck, not server GUID proof */
    PP_RESULT_CAST_ACK = 12 /* exact-GUID cast acknowledgement, NOT loot completion */
} PpResult;
typedef enum {
    PP_EVENT_CAST = 1,
    PP_EVENT_SUCCESS = 2,
    PP_EVENT_EMPTY = 3,
    PP_EVENT_RETRY = 4,
    PP_EVENT_TIMEOUT = 5,
    PP_EVENT_INELIGIBLE = 6,
    PP_EVENT_GAVE_UP = 7, /* bounded retry budget exhausted for GUID */
    PP_EVENT_NOT_CASTABLE = 8,
    PP_EVENT_NO_CANDIDATES = 9,
    PP_EVENT_ALL_BLOCKED = 10,
    PP_EVENT_WORLD_PAUSED = 11,
    PP_EVENT_WORLD_RESET = 12,
    PP_EVENT_ENABLED = 13,
    PP_EVENT_DISABLED = 14,
    PP_EVENT_RESET = 15,
    PP_EVENT_PROBE_REJECTED = 16,
    PP_EVENT_MONEY_SUCCESS = 17,
    PP_EVENT_PACKET_FALLBACK = 18,
    PP_EVENT_OUT_OF_RANGE = 19, PP_EVENT_LINE_OF_SIGHT = 20,
    PP_EVENT_NOT_STEALTHED = 21, PP_EVENT_NOT_READY = 22,
    PP_EVENT_CAST_REJECTED = 23,
    PP_EVENT_PREFETCH_ONLY = 24,
    PP_EVENT_LOCAL_RANGE_REJECT = 25,
    PP_EVENT_LUA_EPOCH = 26 /* Lua observer recreated, including /reload */,
    PP_EVENT_UI_OBSERVATION = 27, /* unscoped UI error, never a GUID result */
    PP_EVENT_UI_RANGE_RECHECKED = 28, /* old single-target UI hint */
    PP_EVENT_BURST_EXPIRE = 29, /* result unknown, not failed theft */
    PP_EVENT_BURST_RANGE_EXIT = 30, /* exact pending GUID out of reach; never blocks next GUID */
    PP_EVENT_WALLET_OBS = 31, /* global, unattributed positive wallet delta */
    PP_EVENT_BURST_RANGE_RELEASE = 32, /* geometry verified: cancel old pending nonce; short GUID backoff */
    PP_EVENT_SPOOF_SEQUENCE = 33, /* local packet submissions, not server ACK */
    PP_EVENT_CAST_ACK = 34, /* GUID-scoped cast acknowledged; no theft proven */
    PP_EVENT_ACK_LOOT_UNKNOWN = 35, /* ACK observed, loot not correlated; do not retry same NPC */
    PP_EVENT_SPOOF_RESTORE_LOOT = 36, /* local loot-closed or result; not proof of theft */
    PP_EVENT_SPOOF_RESTORE_TIMEOUT = 37, /* bounded remote position hold elapsed */
    PP_EVENT_SPOOF_RESTORE_ERROR = 38, /* restore send failed; future spoof disabled */
    PP_EVENT_SPOOF_WORLD_DROPPED = 39 /* world changed: do not send old character movement */
} PpEvent;
typedef struct {
    void *ctx;
    /* Game-thread only. Return only verified eligible NPCs, never player/pet/etc. */
    size_t (*scan)(void *ctx, PpTarget *out, size_t cap);
    /* Game-thread only: class, learned spell, usable state and stealth checks. */
    int (*can_cast)(void *ctx);
    /* 1=submitted, PP_CAST_LOCAL_RANGE=local preflight missed range,
     * 0=other refused submission. No return value implies spell success. */
    int (*cast_on_guid)(void *ctx, PpGuid target, uint32_t attempt_id);
    /* Correlate exact GUID and attempt to an authoritative result. */
    PpResult (*result)(void *ctx, PpGuid target, uint32_t attempt_id);
    /* Game-thread-only cancellation of an abandoned exact GUID+nonce.
     * The native policy must release its outstanding local arm before this
     * engine can submit another GUID in the same loader pulse. */
    void (*end_attempt)(void *ctx, PpGuid target, uint32_t attempt_id);
    /* Optional structured event sink; must not print to WoW chat. */
    void (*event)(void *ctx, PpEvent event, PpGuid target, uint32_t attempt_id);
} PpAdapter;
typedef struct {
    PpGuid guid;
    uint32_t blocked_until_ms;
    unsigned terminal;
    unsigned present;
    unsigned attempts; /* includes refused submissions and timed-out casts */
} PpHistory;
typedef struct {
    PpGuid guid;
    uint32_t nonce, started_ms, last_polled_ms;
    unsigned valid, outside_reported, ack_seen;
} PpBurstPending;
typedef struct {
    PpAdapter api;
    PpHistory history[PP_HISTORY_CAP];
    PpBurstPending pending[PP_BURST_PENDING_CAP];
    unsigned pending_count, burst_enabled, burst_sent;
    uint32_t last_burst_send_ms;
    PpGuid active;
    /* Monotonic nonce isolates delayed responses from old casts or worlds. */
    uint32_t active_attempt_id, next_attempt_id;
    uint32_t started_ms, last_scan_ms;
    /* Refreshed GUID snapshot; next pulse can discover new NPCs while an
     * earlier cast awaits its result. Only eligible==1 may be submitted. */
    PpTarget queue[PP_SCAN_CAP];
    size_t queue_count;
    uint32_t queue_built_ms;
    unsigned last_selection_forward, last_selection_ready;
    unsigned history_next, active_valid, enabled, scan_started;
    unsigned probe_mode;
    uint32_t last_diagnostic_ms;
    unsigned diagnostic_started;
    uint32_t casts, successes, empty, retries, timeouts;
} PpEngine;
/* Starts DISABLED; no game interactions without an explicitly enabled adapter. */
int pp_init(PpEngine *engine, PpAdapter api);
void pp_enable(PpEngine *engine, int enable);
/* Clear when world/map/character changes; no implicit reset on movement. */
void pp_reset(PpEngine *engine);
void pp_tick(PpEngine *engine, uint32_t now_ms);
/* One explicit attempt at selected GUID; 1 means submission, never success.
 * No autonomous scanning or retry; only explicit reset permits same GUID again. */
int pp_probe_once(PpEngine *engine, PpGuid selected, uint32_t now_ms);
#ifdef __cplusplus
}
#endif
#endif
