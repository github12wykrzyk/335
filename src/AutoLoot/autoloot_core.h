#ifndef WOW335_AUTOLOOT_CORE_H
#define WOW335_AUTOLOOT_CORE_H

/* Pure decision engine. All game addresses, object traversal, packet calls,
   UI inspection and thread affinity belong to a separately verified 12340
   adapter. This file does NOT claim the existence of a working game DLL. */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AL_SCAN_CAP 32u
#define AL_HISTORY_CAP 32u

typedef struct { uint32_t lo, hi; } AlGuid;
typedef struct {
    AlGuid guid;
    float distance_sq;
    unsigned can_loot;
} AlCorpse;

typedef enum {
    AL_UI_CLOSED = 0,
    AL_UI_OPEN = 1,
    AL_UI_EMPTY = 2,
    AL_UI_BAG_FULL = 3,
    AL_UI_BLOCKED = 4
} AlUiState;

typedef enum {
    AL_IDLE = 0,
    AL_WAIT_OPEN = 1,
    AL_DRAIN = 2
} AlState;

typedef struct {
    void *ctx;
    /* Called on the verified WoW game thread, never by a worker thread.
       Only return corpses eligible for the current player to loot. */
    size_t (*scan)(void *ctx, AlCorpse *out, size_t capacity);
    /* Return 1 only when a loot interaction was actually submitted. */
    int (*interact)(void *ctx, AlGuid guid);
    /* Must describe ONLY the loot interaction owned by this engine.
       A manually opened loot window must not be drained or closed. */
    AlUiState (*window)(void *ctx, AlGuid owner);
    /* Return 1 only when a loot-all action was actually submitted. */
    int (*loot_all)(void *ctx);
    /* Return 1 iff movement/casting/other active modules permit this action.
       Combat is not implicitly prohibited. */
    int (*can_act)(void *ctx);
} AlAdapter;

typedef struct {
    AlGuid guid;
    uint32_t until_ms;
    unsigned valid;
} AlHistory;

typedef struct {
    AlAdapter api;
    AlHistory history[AL_HISTORY_CAP];
    AlGuid active;
    AlState state;
    uint32_t state_since_ms, last_scan_ms, last_drain_ms;
    unsigned history_next, drain_attempts, enabled, scan_started;
    unsigned requests, drained, deferred;
    float max_range_sq;
} AlEngine;

void al_init(AlEngine *engine, AlAdapter adapter, float max_range_sq);
void al_enable(AlEngine *engine, int enabled);
void al_tick(AlEngine *engine, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
#endif
