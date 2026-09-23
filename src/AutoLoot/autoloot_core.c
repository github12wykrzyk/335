#include "autoloot_core.h"
#include <string.h>

#define AL_SCAN_INTERVAL_MS 80u
#define AL_OPEN_TIMEOUT_MS 600u
#define AL_DRAIN_INTERVAL_MS 50u
#define AL_MAX_DRAIN 20u
#define AL_SUCCESS_COOLDOWN_MS 15000u
#define AL_FAILURE_COOLDOWN_MS 1200u
#define AL_FULL_BAG_COOLDOWN_MS 10000u

static int same(AlGuid a, AlGuid b) {
    return a.lo == b.lo && a.hi == b.hi;
}
static int zero(AlGuid g) { return (g.lo | g.hi) == 0u; }

/* Modulo-2^32 millisecond arithmetic remains valid across tick wrap if
   cooldowns are shorter than 2^31 ms. */
static int before(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) < 0;
}
static int recent(AlEngine *e, AlGuid g, uint32_t now) {
    size_t i;
    for (i = 0; i < AL_HISTORY_CAP; ++i)
        if (e->history[i].valid && same(e->history[i].guid, g) &&
            before(now, e->history[i].until_ms)) return 1;
    return 0;
}
static void defer(AlEngine *e, AlGuid g, uint32_t now, uint32_t delay) {
    size_t i;
    AlHistory *slot = NULL;
    for (i = 0; i < AL_HISTORY_CAP; ++i)
        if (e->history[i].valid && same(e->history[i].guid, g)) {
            slot = &e->history[i];
            break;
        }
    if (!slot) slot = &e->history[e->history_next++ % AL_HISTORY_CAP];
    slot->guid = g;
    slot->valid = 1u;
    slot->until_ms = now + delay;
}
static void release(AlEngine *e, uint32_t now, uint32_t delay, int completed) {
    if (!zero(e->active)) defer(e, e->active, now, delay);
    if (completed) ++e->drained;
    else ++e->deferred;
    memset(&e->active, 0, sizeof(e->active));
    e->state = AL_IDLE;
    e->drain_attempts = 0;
}
void al_init(AlEngine *e, AlAdapter adapter, float max_range_sq) {
    if (!e) return;
    memset(e, 0, sizeof(*e));
    e->api = adapter;
    /* 0 means no verified reach contract, so the engine must not scan. */
    if (max_range_sq > 0.0f && max_range_sq < 1000000.0f)
        e->max_range_sq = max_range_sq;
}
void al_enable(AlEngine *e, int enabled) {
    if (!e) return;
    e->enabled = enabled ? 1u : 0u;
    if (!e->enabled) {
        e->state = AL_IDLE;
        e->active.lo = e->active.hi = 0u;
        e->drain_attempts = 0;
    }
}
void al_tick(AlEngine *e, uint32_t now) {
    AlUiState ui;
    AlCorpse candidates[AL_SCAN_CAP];
    AlCorpse *best = NULL;
    float best_d2;
    size_t i, count;
    if (!e || !e->enabled || !e->max_range_sq || !e->api.scan ||
        !e->api.interact || !e->api.window || !e->api.loot_all ||
        !e->api.can_act) return;
    if (e->state != AL_IDLE) {
        ui = e->api.window(e->api.ctx, e->active);
        if (ui == AL_UI_EMPTY) {
            release(e, now, AL_SUCCESS_COOLDOWN_MS, 1);
            return;
        }
        if (ui == AL_UI_BAG_FULL) {
            release(e, now, AL_FULL_BAG_COOLDOWN_MS, 0);
            return;
        }
        if (ui == AL_UI_BLOCKED) {
            release(e, now, AL_FAILURE_COOLDOWN_MS, 0);
            return;
        }
        if (e->state == AL_WAIT_OPEN) {
            if (ui == AL_UI_OPEN) {
                e->state = AL_DRAIN;
                e->state_since_ms = now;
                e->last_drain_ms = now - AL_DRAIN_INTERVAL_MS;
            } else if ((uint32_t)(now - e->state_since_ms) >= AL_OPEN_TIMEOUT_MS) {
                release(e, now, AL_FAILURE_COOLDOWN_MS, 0);
                return;
            } else return;
        } else if (ui == AL_UI_CLOSED) {
            /* No server-confirmed empty state: do not invent success. */
            release(e, now, AL_FAILURE_COOLDOWN_MS, 0);
            return;
        }
        if (e->state == AL_DRAIN &&
            (uint32_t)(now - e->last_drain_ms) >= AL_DRAIN_INTERVAL_MS &&
            e->api.can_act(e->api.ctx)) {
            if (e->drain_attempts >= AL_MAX_DRAIN ||
                !e->api.loot_all(e->api.ctx)) {
                release(e, now, AL_FAILURE_COOLDOWN_MS, 0);
                return;
            }
            ++e->drain_attempts;
            e->last_drain_ms = now;
        }
        return;
    }
    if (e->scan_started &&
        (uint32_t)(now - e->last_scan_ms) < AL_SCAN_INTERVAL_MS) return;
    e->scan_started = 1u;
    e->last_scan_ms = now;
    if (!e->api.can_act(e->api.ctx)) return;
    count = e->api.scan(e->api.ctx, candidates, AL_SCAN_CAP);
    if (count > AL_SCAN_CAP) count = AL_SCAN_CAP;
    best_d2 = e->max_range_sq;
    for (i = 0; i < count; ++i) {
        AlCorpse *candidate = &candidates[i];
        if (!candidate->can_loot || zero(candidate->guid) ||
            recent(e, candidate->guid, now)) continue;
        /* Reject NaN, negative distances and out-of-contract candidates. */
        if (!(candidate->distance_sq >= 0.0f &&
              candidate->distance_sq <= best_d2)) continue;
        best = candidate;
        best_d2 = candidate->distance_sq;
    }
    if (best) {
        if (e->api.interact(e->api.ctx, best->guid) == 1) {
            e->active = best->guid;
            e->state = AL_WAIT_OPEN;
            e->state_since_ms = now;
            e->drain_attempts = 0;
            ++e->requests;
        } else defer(e, best->guid, now, AL_FAILURE_COOLDOWN_MS);
    }
}
