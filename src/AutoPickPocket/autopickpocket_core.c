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
    if (e->api.event) e->api.event(e->api.ctx, ev, guid, e->active_attempt_id);
}
/* Sampling keeps idle diagnostics bounded and avoids per-scan disk writes. */
static void idle_event(PpEngine *e,PpEvent reason,uint32_t now) {
    PpGuid none={0u,0u};
    if (!e->diagnostic_started || (uint32_t)(now-e->last_diagnostic_ms)>=1000u) {
        e->diagnostic_started=1u;
        e->last_diagnostic_ms=now;
        emit(e,reason,none);
    }
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
/* A rejected cast or missing server result must never retry indefinitely. */
static void failure(PpEngine *e, PpGuid guid, uint32_t now, uint32_t delay, PpEvent reason) {
    PpHistory *h=entry(e,guid,1);
    int exhausted=h->attempts>=PP_MAX_ATTEMPTS_PER_GUID;
    block(e,guid,now,exhausted ? 0u : delay,exhausted);
    emit(e,reason,guid);
    if (exhausted) emit(e,PP_EVENT_GAVE_UP,guid);
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
    engine->active_attempt_id=0u;
    engine->scan_started=0u;
    engine->probe_mode=0u;
}
void pp_reset(PpEngine *engine) {
    if (!engine) return;
    memset(engine->history,0,sizeof(engine->history));
    engine->history_next=0u;
    engine->diagnostic_started=0u;
    engine->active_valid=0u;
    if (engine->probe_mode) engine->enabled=0u;
    engine->probe_mode=0u;
    engine->active_attempt_id=0u; /* keep next_attempt_id across world resets */
    engine->scan_started=0u;
}
/* Reject an unverified target, rather than substituting a nearby NPC.
 * Once submitted, pp_tick only observes the result or timeout.
 */
int pp_probe_once(PpEngine *e,PpGuid selected,uint32_t now) {
    PpTarget targets[PP_SCAN_CAP];
    PpHistory *h;
    size_t i,count;
    int found=0;
    if (!e || !nonzero(selected) || e->enabled || e->active_valid) return 0;
    e->active_attempt_id=0u;
    h=entry(e,selected,0);
    if (h && (h->attempts || h->terminal)) {
        emit(e,PP_EVENT_PROBE_REJECTED,selected);
        return 0;
    }
    if (e->api.can_cast(e->api.ctx)!=1) {
        emit(e,PP_EVENT_NOT_CASTABLE,selected);
        return 0;
    }
    count=e->api.scan(e->api.ctx,targets,PP_SCAN_CAP);
    if (count>PP_SCAN_CAP) count=PP_SCAN_CAP;
    for(i=0u;i<count;++i) {
        if (same(targets[i].guid,selected) && targets[i].eligible &&
            targets[i].distance_sq>=0.0f &&
            targets[i].distance_sq<FLT_MAX) {found=1;break;}
    }
    if (!found) {
        emit(e,PP_EVENT_PROBE_REJECTED,selected);
        return 0;
    }
    h=entry(e,selected,1);
    ++h->attempts;
    if (++e->next_attempt_id==0u) ++e->next_attempt_id;
    e->active_attempt_id=e->next_attempt_id;
    if (e->api.cast_on_guid(e->api.ctx,selected,e->active_attempt_id)!=1) {
        block(e,selected,now,0u,1);
        ++e->retries;
        emit(e,PP_EVENT_RETRY,selected);
        return 0;
    }
    e->active=selected;
    e->active_valid=1u;
    e->started_ms=now;
    e->enabled=1u;
    e->probe_mode=1u;
    ++e->casts;
    emit(e,PP_EVENT_CAST,selected);
    return 1;
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
        outcome=engine->api.result(engine->api.ctx,engine->active,engine->active_attempt_id);
        switch(outcome) {
        case PP_RESULT_SUCCESS:
            block(engine,engine->active,now,0u,1);
            ++engine->successes;
            emit(engine,PP_EVENT_SUCCESS,engine->active);
            engine->active_valid=0u;
            if (engine->probe_mode) {engine->enabled=0u;return;}
            /* A terminal GUID is excluded from the very next scan.
             * Reuse this pulse for a different nearby target. */
            engine->scan_started=0u;
            goto scan_next;
        case PP_RESULT_EMPTY:
            block(engine,engine->active,now,0u,1);
            ++engine->empty;
            emit(engine,PP_EVENT_EMPTY,engine->active);
            engine->active_valid=0u;
            if (engine->probe_mode) {engine->enabled=0u;return;}
            engine->scan_started=0u;
            goto scan_next;
        case PP_RESULT_PERMANENT:
            block(engine,engine->active,now,0u,1);
            emit(engine,PP_EVENT_INELIGIBLE,engine->active);
            engine->active_valid=0u;
            if (engine->probe_mode) engine->enabled=0u;
            return;
        case PP_RESULT_RETRYABLE:
            failure(engine,engine->active,now,PP_RETRY_DELAY_MS,PP_EVENT_RETRY);
            ++engine->retries;
            engine->active_valid=0u;
            if (engine->probe_mode) {engine->enabled=0u;return;}
            engine->scan_started=0u;
            goto scan_next;
        case PP_RESULT_PENDING: break;
        default: return; /* unknown result fails closed */
        }
        if ((uint32_t)(now-engine->started_ms) < PP_RESULT_TIMEOUT_MS) return;
        failure(engine,engine->active,now,PP_TIMEOUT_DELAY_MS,PP_EVENT_TIMEOUT);
        ++engine->timeouts;
        engine->active_valid=0u;
        if (engine->probe_mode) {engine->enabled=0u;return;}
        /* A timed-out NPC must not stall independent eligible GUIDs. */
        engine->scan_started=0u;
        goto scan_next;
    }
scan_next:
    if (engine->probe_mode) return;
    if (engine->scan_started && (uint32_t)(now-engine->last_scan_ms)<PP_SCAN_INTERVAL_MS)
        return;
    engine->scan_started=1u;
    engine->last_scan_ms=now;
    if (engine->api.can_cast(engine->api.ctx)!=1) {
        idle_event(engine,PP_EVENT_NOT_CASTABLE,now);
        return;
    }
    count=engine->api.scan(engine->api.ctx,targets,PP_SCAN_CAP);
    if (count>PP_SCAN_CAP) count=PP_SCAN_CAP;
    if (!count) {
        idle_event(engine,PP_EVENT_NO_CANDIDATES,now);
        return;
    }
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
    if (!found) {
        idle_event(engine,PP_EVENT_ALL_BLOCKED,now);
        return;
    }
    /* Record every submission attempt, including native rejection. */
    h=entry(engine,best.guid,1);
    if (h->attempts>=PP_MAX_ATTEMPTS_PER_GUID) {
        h->terminal=1u;
        emit(engine,PP_EVENT_GAVE_UP,best.guid);
        return;
    }
    ++h->attempts;
    if (++engine->next_attempt_id==0u) ++engine->next_attempt_id;
    engine->active_attempt_id=engine->next_attempt_id;
    if (engine->api.cast_on_guid(engine->api.ctx,best.guid,engine->active_attempt_id)==1) {
        engine->active=best.guid;
        engine->active_valid=1u;
        engine->started_ms=now;
        ++engine->casts;
        emit(engine,PP_EVENT_CAST,best.guid);
    } else {
        failure(engine,best.guid,now,PP_RETRY_DELAY_MS,PP_EVENT_RETRY);
        ++engine->retries;
        /* Do not override the native-cast-per-pulse bound after an
         * ambiguous rejected submission. Try another GUID next pulse. */
    }
}
