#include "autopickpocket_core.h"
#include <string.h>
#include <float.h>

static int same(PpGuid a, PpGuid b) { return a.lo == b.lo && a.hi == b.hi; }
static int nonzero(PpGuid a) { return a.lo != 0u || a.hi != 0u; }
/* Bounded delays < 2^31 allow wrap-safe GetTickCount-style arithmetic. */
static int deadline_reached(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
}
static void emit(PpEngine *e, PpEvent ev, PpGuid guid) {
    if (e->api.event) e->api.event(e->api.ctx, ev, guid);
}
static PpHistory *entry(PpEngine *e, PpGuid guid, int create) {
    unsigned i;
    PpHistory *slot;
    for (i=0u; i<PP_HISTORY_CAP; ++i)
        if (e->history[i].present && same(e->history[i].guid, guid))
            return &e->history[i];
    if (!create) return NULL;
    /* Reuse vacant entries before evicting oldest recorded entries. */
    for (i=0u; i<PP_HISTORY_CAP; ++i) {
        unsigned at=(e->history_next + i) % PP_HISTORY_CAP;
        if (!e->history[at].present) {
            e->history_next=(at+1u)%PP_HISTORY_CAP;
            slot=&e->history[at];
            memset(slot,0,sizeof(*slot));
            slot->guid=guid; slot->present=1u;
            return slot;
        }
    }
    slot=&e->history[e->history_next];
    e->history_next=(e->history_next+1u)%PP_HISTORY_CAP;
    memset(slot,0,sizeof(*slot));
    slot->guid=guid; slot->present=1u;
    return slot;
}
static void block(PpEngine *e, PpGuid guid, uint32_t now, uint32_t delay, int terminal) {
    PpHistory *h=entry(e,guid,1);
    h->terminal=terminal ? 1u : 0u;
    h->blocked_until_ms=now+delay;
}
int pp_init(PpEngine *engine, PpAdapter api) {
    if (!engine || !api.scan || !api.can_cast || !api.cast_on_guid || !api.result)
        return 0;
    memset(engine,0,sizeof(*engine));
    engine->api=api;
    return 1;
}
void pp_enable(PpEngine *engine, int enable) {
    if (!engine) return;
    engine->enabled=enable ? 1u : 0u;
    /* Disable forgets pending cast, but not confirmed terminal history. */
    engine->active_valid=0u;
    engine->scan_started=0u;
}
void pp_reset(PpEngine *engine) {
    if (!engine) return;
    memset(engine->history,0,sizeof(engine->history));
    engine->history_next=0u;
    engine->active_valid=0u;
    engine->scan_started=0u;
}
void pp_tick(PpEngine *engine, uint32_t now) {
    PpTarget targets[PP_SCAN_CAP];
    PpTarget best = {0}; /* MSVC /W4: initialized even on the no-candidate path. */
    size_t i, count;
    int found=0;
    PpHistory *h;
    PpResult outcome;
    if (!engine || !engine->enabled) return;
    if (engine->active_valid) {
        outcome=engine->api.result(engine->api.ctx,engine->active);
        switch(outcome) {
        case PP_RESULT_SUCCESS:
            block(engine,engine->active,now,0u,1);
            ++engine->successes;
            emit(engine,PP_EVENT_SUCCESS,engine->active);
            engine->active_valid=0u;
            return;
        case PP_RESULT_EMPTY:
            block(engine,engine->active,now,0u,1);
            ++engine->empty;
            emit(engine,PP_EVENT_EMPTY,engine->active);
            engine->active_valid=0u;
            return;
        case PP_RESULT_PERMANENT:
            block(engine,engine->active,now,0u,1);
            emit(engine,PP_EVENT_INELIGIBLE,engine->active);
            engine->active_valid=0u;
            return;
        case PP_RESULT_RETRYABLE:
            block(engine,engine->active,now,PP_RETRY_DELAY_MS,0);
            ++engine->retries;
            emit(engine,PP_EVENT_RETRY,engine->active);
            engine->active_valid=0u;
            return;
        case PP_RESULT_PENDING: break;
        default: return; /* unknown result fails closed */
        }
        if ((uint32_t)(now-engine->started_ms) < PP_RESULT_TIMEOUT_MS) return;
        block(engine,engine->active,now,PP_TIMEOUT_DELAY_MS,0);
        ++engine->timeouts;
        emit(engine,PP_EVENT_TIMEOUT,engine->active);
        engine->active_valid=0u;
        return;
    }
    if (engine->scan_started && (uint32_t)(now-engine->last_scan_ms)<PP_SCAN_INTERVAL_MS)
        return;
    engine->scan_started=1u;
    engine->last_scan_ms=now;
    if (engine->api.can_cast(engine->api.ctx)!=1) return;
    count=engine->api.scan(engine->api.ctx,targets,PP_SCAN_CAP);
    if (count>PP_SCAN_CAP) count=PP_SCAN_CAP;
    for(i=0u;i<count;++i) {
        PpTarget target=targets[i];
        if (!target.eligible || !nonzero(target.guid) ||
            !(target.distance_sq>=0.0f && target.distance_sq<FLT_MAX)) continue;
        h=entry(engine,target.guid,0);
        if (h && (h->terminal || !deadline_reached(now,h->blocked_until_ms))) continue;
        if (!found || target.distance_sq<best.distance_sq) {
            best=target;
            found=1;
        }
    }
    if (!found) return;
    if (engine->api.cast_on_guid(engine->api.ctx,best.guid)==1) {
        engine->active=best.guid;
        engine->active_valid=1u;
        engine->started_ms=now;
        ++engine->casts;
        emit(engine,PP_EVENT_CAST,best.guid);
    } else {
        block(engine,best.guid,now,PP_RETRY_DELAY_MS,0);
        ++engine->retries;
        emit(engine,PP_EVENT_RETRY,best.guid);
    }
}
