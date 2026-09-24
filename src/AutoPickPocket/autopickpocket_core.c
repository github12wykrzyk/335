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
    if (engine->active_valid && engine->api.end_attempt)
        engine->api.end_attempt(engine->api.ctx,engine->active,engine->active_attempt_id);
    {unsigned k;
     for(k=0u;k<PP_BURST_PENDING_CAP;++k){
         PpBurstPending *p=&engine->pending[k];
         if(p->valid) {
             block(engine,p->guid,0u,0u,1);
             if(engine->api.end_attempt)
                 engine->api.end_attempt(engine->api.ctx,p->guid,p->nonce);
         }
         p->valid=0u;
     }
     engine->pending_count=0u;
    }
    engine->enabled=enable ? 1u : 0u;
    /* Disable forgets pending cast, but not confirmed terminal history. */
    engine->active_valid=0u;
    engine->active_attempt_id=0u;
    engine->scan_started=0u;
    engine->queue_count=0u;
    engine->probe_mode=0u;
    engine->last_selection_forward=0u;
    engine->last_selection_ready=0u;
}
void pp_reset(PpEngine *engine) {
    if (!engine) return;
    if (engine->active_valid && engine->api.end_attempt)
        engine->api.end_attempt(engine->api.ctx,engine->active,engine->active_attempt_id);
    {unsigned k;
     for(k=0u;k<PP_BURST_PENDING_CAP;++k){
         PpBurstPending *p=&engine->pending[k];
         if(p->valid && engine->api.end_attempt)
             engine->api.end_attempt(engine->api.ctx,p->guid,p->nonce);
         p->valid=0u;
     }
     engine->pending_count=0u;
     engine->burst_sent=0u;
    }
    memset(engine->history,0,sizeof(engine->history));
    engine->history_next=0u;
    engine->diagnostic_started=0u;
    engine->active_valid=0u;
    if (engine->probe_mode) engine->enabled=0u;
    engine->probe_mode=0u;
    engine->active_attempt_id=0u; /* keep next_attempt_id across world resets */
    engine->scan_started=0u;
    engine->queue_count=0u;
    engine->last_selection_forward=0u;
    engine->last_selection_ready=0u;
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
        if (same(targets[i].guid,selected) && targets[i].eligible==1u &&
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
        if (e->api.end_attempt)
            e->api.end_attempt(e->api.ctx,selected,e->active_attempt_id);
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
/* Discovery runs independently of an outstanding spell result or cooldown.
 * A moving player must not be offered a 1.2-second-old GUID snapshot.
 * The native bridge validates the actual GUID/range again before casting. */
static void refresh_snapshot(PpEngine *e,uint32_t now) {
    size_t count;
    if (e->scan_started &&
        (uint32_t)(now-e->last_scan_ms)<PP_SCAN_INTERVAL_MS) {
        if ((uint32_t)(now-e->queue_built_ms)>=PP_QUEUE_TTL_MS)
            e->queue_count=0u;
        return;
    }
    e->scan_started=1u;
    e->last_scan_ms=now;
    e->queue_built_ms=now;
    count=e->api.scan(e->api.ctx,e->queue,PP_SCAN_CAP);
    e->queue_count=count>PP_SCAN_CAP ? PP_SCAN_CAP : count;
}
/* Burst mode mirrors repeated mouseover macro submission: never wait for
 * loot, animation or a result before choosing a different GUID. Bounded
 * per-GUID result observations continue independently, on the game thread.
 * Legacy single-target probe intentionally keeps its isolated state machine. */
static void burst_tick(PpEngine *e,uint32_t now) {
    unsigned i,local_rejects;
    size_t j;
    PpTarget best={0};
    PpHistory *h;
    int found,has_prefetch,submit;
    for(i=0u;i<PP_BURST_PENDING_CAP;++i){
        PpBurstPending *p=&e->pending[i];
        PpResult out=PP_RESULT_PENDING;
        if(!p->valid)continue;
        e->active_attempt_id=p->nonce; /* exact GUID/nonce in all event records */
        if(!p->last_polled_ms ||
           (uint32_t)(now-p->last_polled_ms)>=PP_BURST_RESULT_POLL_MS){
            p->last_polled_ms=now ? now : 1u;
            out=e->api.result(e->api.ctx,p->guid,p->nonce);
        }
        if(out==PP_RESULT_UI_RANGE_HINT)out=PP_RESULT_PENDING;
        if(out==PP_RESULT_CAST_ACK){
            if(!p->ack_seen){p->ack_seen=1u;emit(e,PP_EVENT_CAST_ACK,p->guid);}
            out=PP_RESULT_PENDING; /* Do not terminate this GUID or block the next NPC. */
        }
        if(out!=PP_RESULT_PENDING) {
            switch(out) {
            case PP_RESULT_SUCCESS:
            case PP_RESULT_MONEY_SUCCESS:
                block(e,p->guid,now,0u,1);++e->successes;
                emit(e,out==PP_RESULT_MONEY_SUCCESS ?
                     PP_EVENT_MONEY_SUCCESS : PP_EVENT_SUCCESS,p->guid);break;
            case PP_RESULT_EMPTY:
                block(e,p->guid,now,0u,1);++e->empty;
                emit(e,PP_EVENT_EMPTY,p->guid);break;
            case PP_RESULT_PERMANENT:
                block(e,p->guid,now,0u,1);
                emit(e,PP_EVENT_INELIGIBLE,p->guid);break;
            case PP_RESULT_OUT_OF_RANGE:
                h=entry(e,p->guid,1);
                if(h->attempts)--h->attempts; /* range is not a loot retry */
                block(e,p->guid,now,PP_RANGE_RETRY_DELAY_MS,0);
                ++e->retries;emit(e,PP_EVENT_OUT_OF_RANGE,p->guid);break;
            default:
                /* Server-scoped nonrange reject. Unscoped UI errors never
                 * arrive here as GUID-specific outcomes. */
                failure(e,p->guid,now,PP_RETRY_DELAY_MS,
                    out==PP_RESULT_LINE_OF_SIGHT ? PP_EVENT_LINE_OF_SIGHT :
                    out==PP_RESULT_NOT_STEALTHED ? PP_EVENT_NOT_STEALTHED :
                    out==PP_RESULT_NOT_READY ? PP_EVENT_NOT_READY :
                    PP_EVENT_CAST_REJECTED);
                ++e->retries;break;
            }
            if(e->api.end_attempt)
                e->api.end_attempt(e->api.ctx,p->guid,p->nonce);
            p->valid=0u;--e->pending_count;
            continue;
        }
        /* Current native geometry is per GUID; unlike UI_ERROR_MESSAGE it
         * can justify releasing an unanswered attempt that left 4yd.
         * Retain a short ACK grace, re-check the *current* snapshot and
         * release the exact nonce without declaring cast failure/success.
         * This never delays other GUIDs and has no 1.6+2.5s revisit lock. */
        for(j=0u;j<e->queue_count;++j)
            if(same(e->queue[j].guid,p->guid)){
                if(e->queue[j].eligible==2u && !p->ack_seen){
                    if(!p->outside_reported){
                        p->outside_reported=1u;
                        emit(e,PP_EVENT_BURST_RANGE_EXIT,p->guid);
                    }
                    if((uint32_t)(now-p->started_ms)>=PP_BURST_RANGE_RELEASE_MS &&
                       (uint32_t)(now-e->queue_built_ms)<=PP_SCAN_INTERVAL_MS){
                        block(e,p->guid,now,PP_RANGE_RETRY_DELAY_MS,0);
                        emit(e,PP_EVENT_BURST_RANGE_RELEASE,p->guid);
                        if(e->api.end_attempt)
                            e->api.end_attempt(e->api.ctx,p->guid,p->nonce);
                        p->valid=0u;--e->pending_count;
                    }
                }
                break;
            }
        if(!p->valid)continue;
        if((uint32_t)(now-p->started_ms)>=PP_BURST_OBSERVE_MS) {
            /* An ACK can mean the NPC was already picked: retrying after
             * an uncorrelated wallet event spams the same GUID. Quarantine
             * it for this world session without claiming any money/theft.
             * Truly unanswered sends retain their short per-GUID backoff. */
            if(p->ack_seen){
                block(e,p->guid,now,0u,1);
                ++e->timeouts;emit(e,PP_EVENT_ACK_LOOT_UNKNOWN,p->guid);
            }else{
                block(e,p->guid,now,PP_BURST_UNKNOWN_BACKOFF_MS,0);
                ++e->timeouts;emit(e,PP_EVENT_BURST_EXPIRE,p->guid);
            }
            if(e->api.end_attempt)
                e->api.end_attempt(e->api.ctx,p->guid,p->nonce);
            p->valid=0u;--e->pending_count;
        }
    }
    if(!e->queue_count || e->pending_count>=PP_BURST_PENDING_CAP)return;
    if(e->burst_sent &&
       (uint32_t)(now-e->last_burst_send_ms)<PP_BURST_MIN_SEND_MS)return;
    if(e->api.can_cast(e->api.ctx)!=1){
        idle_event(e,PP_EVENT_NOT_CASTABLE,now);return;
    }
    /* No range-error feedback loop: every local range refusal skips directly
     * to another fresh GUID, without submitting or consuming attempts. */
    for(local_rejects=0u;local_rejects<PP_LOCAL_FAILOVER_LIMIT;++local_rejects){
        found=0;has_prefetch=0;e->last_selection_ready=0u;
        e->last_selection_forward=0u;
        for(j=0u;j<e->queue_count;++j){
            PpTarget t=e->queue[j];
            unsigned k,inflight=0u;
            if(!nonzero(t.guid) ||
               !(t.distance_sq>=0.0f && t.distance_sq<FLT_MAX))continue;
            if(t.eligible==2u){has_prefetch=1;continue;}
            if(t.eligible!=1u)continue;
            for(k=0u;k<PP_BURST_PENDING_CAP;++k)
                if(e->pending[k].valid &&
                   same(e->pending[k].guid,t.guid)){inflight=1u;break;}
            if(inflight)continue;
            h=entry(e,t.guid,0);
            if(h && (h->terminal || h->attempts>=PP_MAX_ATTEMPTS_PER_GUID ||
                     !deadline_reached(now,h->blocked_until_ms)))continue;
            ++e->last_selection_ready;
            if(!found || (t.forward && !best.forward) ||
               (t.forward==best.forward && t.distance_sq<best.distance_sq)){
                best=t;found=1;
            }
        }
        if(!found){
            idle_event(e,has_prefetch ? PP_EVENT_PREFETCH_ONLY :
                       PP_EVENT_ALL_BLOCKED,now);
            return;
        }
        e->last_selection_forward=best.forward;
        h=entry(e,best.guid,1);
        ++h->attempts;
        if(++e->next_attempt_id==0u)++e->next_attempt_id;
        e->active_attempt_id=e->next_attempt_id;
        submit=e->api.cast_on_guid(e->api.ctx,best.guid,e->active_attempt_id);
        if(submit==1){
            for(i=0u;i<PP_BURST_PENDING_CAP;++i)
                if(!e->pending[i].valid){
                    PpBurstPending *p=&e->pending[i];
                    memset(p,0,sizeof(*p));
                    p->guid=best.guid;p->nonce=e->active_attempt_id;
                    p->started_ms=now;p->valid=1u;
                    ++e->pending_count;break;
                }
            e->last_burst_send_ms=now;e->burst_sent=1u;++e->casts;
            emit(e,PP_EVENT_CAST,best.guid);
            return; /* max one submitted packet per game-thread pulse */
        }
        if(e->api.end_attempt)
            e->api.end_attempt(e->api.ctx,best.guid,e->active_attempt_id);
        if(submit==PP_CAST_LOCAL_RANGE){
            --h->attempts;
            h->blocked_until_ms=now+PP_LOCAL_RANGE_BACKOFF_MS;
            emit(e,PP_EVENT_LOCAL_RANGE_REJECT,best.guid);
            continue;
        }
        /* No packet submitted. Do not consume server retry budget. */
        --h->attempts;
        h->blocked_until_ms=now+PP_RETRY_DELAY_MS;
        ++e->retries;emit(e,PP_EVENT_RETRY,best.guid);
        return;
    }
}
void pp_tick(PpEngine *engine, uint32_t now) {
    PpTarget best = {0}; /* MSVC /W4: initialized even on the no-candidate path. */
    size_t i;
    int found=0,has_prefetch=0;
    PpHistory *h;
    PpResult outcome;
    int submit_status;
    unsigned local_rejects;
    if (!engine || !engine->enabled) return;
    refresh_snapshot(engine,now); /* also while waiting for result */
    if(engine->burst_enabled && !engine->probe_mode){
        burst_tick(engine,now);
        return;
    }
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
            goto scan_next;
        case PP_RESULT_MONEY_SUCCESS:
            block(engine,engine->active,now,0u,1);
            ++engine->successes;
            emit(engine,PP_EVENT_MONEY_SUCCESS,engine->active);
            engine->active_valid=0u;
            if (engine->probe_mode) {engine->enabled=0u;return;}
            goto scan_next;
        case PP_RESULT_EMPTY:
            block(engine,engine->active,now,0u,1);
            ++engine->empty;
            emit(engine,PP_EVENT_EMPTY,engine->active);
            engine->active_valid=0u;
            if (engine->probe_mode) {engine->enabled=0u;return;}
            goto scan_next;
        case PP_RESULT_PERMANENT:
            block(engine,engine->active,now,0u,1);
            emit(engine,PP_EVENT_INELIGIBLE,engine->active);
            engine->active_valid=0u;
            if (engine->probe_mode) {engine->enabled=0u;return;}
            goto scan_next;
        case PP_RESULT_UI_RANGE_HINT:
            /* UI is not GUID scoped; the native adapter must also verify
             * this exact target has moved outside the live cast radius. */
            failure(engine,engine->active,now,PP_RANGE_RETRY_DELAY_MS,
                    PP_EVENT_UI_RANGE_RECHECKED);
            ++engine->retries;
            engine->active_valid=0u;
            if (engine->probe_mode) {engine->enabled=0u;return;}
            goto scan_next;
        case PP_RESULT_OUT_OF_RANGE:
        case PP_RESULT_LINE_OF_SIGHT:
        case PP_RESULT_NOT_STEALTHED:
        case PP_RESULT_NOT_READY:
        case PP_RESULT_CAST_REJECTED:
            failure(engine,engine->active,now,
                outcome==PP_RESULT_OUT_OF_RANGE ? PP_RANGE_RETRY_DELAY_MS : PP_RETRY_DELAY_MS,
                outcome==PP_RESULT_OUT_OF_RANGE ? PP_EVENT_OUT_OF_RANGE :
                outcome==PP_RESULT_LINE_OF_SIGHT ? PP_EVENT_LINE_OF_SIGHT :
                outcome==PP_RESULT_NOT_STEALTHED ? PP_EVENT_NOT_STEALTHED :
                outcome==PP_RESULT_NOT_READY ? PP_EVENT_NOT_READY : PP_EVENT_CAST_REJECTED);
            ++engine->retries;
            engine->active_valid=0u;
            if (engine->probe_mode) {engine->enabled=0u;return;}
            goto scan_next;
        case PP_RESULT_RETRYABLE:
            failure(engine,engine->active,now,PP_RETRY_DELAY_MS,PP_EVENT_RETRY);
            ++engine->retries;
            engine->active_valid=0u;
            if (engine->probe_mode) {engine->enabled=0u;return;}
            goto scan_next;
        case PP_RESULT_PENDING: break;
        default: return; /* unknown result fails closed */
        }
        if ((uint32_t)(now-engine->started_ms) < PP_RESULT_TIMEOUT_MS) return;
        /* Experimental 200ms unknown-result deadline. The old spell is
         * never counted as success and its exact nonce is cancelled. */
        if (engine->api.end_attempt)
            engine->api.end_attempt(engine->api.ctx,engine->active,engine->active_attempt_id);
        failure(engine,engine->active,now,PP_TIMEOUT_DELAY_MS,PP_EVENT_TIMEOUT);
        ++engine->timeouts;
        engine->active_valid=0u;
        if (engine->probe_mode) {engine->enabled=0u;return;}
        /* A timed-out NPC must not stall independent eligible GUIDs. */
        goto scan_next;
    }
scan_next:
    if (engine->probe_mode) return;
    if (!engine->queue_count) {
        idle_event(engine,PP_EVENT_NO_CANDIDATES,now);
        return;
    }
    if (engine->api.can_cast(engine->api.ctx)!=1) {
        idle_event(engine,PP_EVENT_NOT_CASTABLE,now);
        return;
    }
    /* A LOCAL range precheck did not submit a packet. Try another GUID in
     * this same game-thread pulse, without turning it into a server retry.
     * Bound the work: each rejection can require a guarded object lookup. */
    for(local_rejects=0u;local_rejects<PP_LOCAL_FAILOVER_LIMIT;++local_rejects) {
        found=0;has_prefetch=0;
        engine->last_selection_forward=0u;
        engine->last_selection_ready=0u;
        for(i=0u;i<engine->queue_count;++i) {
            PpTarget target=engine->queue[i];
            if (!nonzero(target.guid) ||
                !(target.distance_sq>=0.0f && target.distance_sq<FLT_MAX))
                continue;
            if (target.eligible==2u) {has_prefetch=1;continue;}
            if (target.eligible!=1u)continue;
            h=entry(engine,target.guid,0);
            if (h && (h->terminal || h->attempts>=PP_MAX_ATTEMPTS_PER_GUID ||
                      !deadline_reached(now,h->blocked_until_ms)))
                continue;
            ++engine->last_selection_ready;
            /* Only prefer forward targets if the native adapter validated
             * a recent movement vector. Stationary fallback stays nearest. */
            if (!found || (target.forward && !best.forward) ||
                (target.forward==best.forward &&
                 target.distance_sq<best.distance_sq)) {
                best=target;
                found=1;
            }
        }
        if (!found) {
            idle_event(engine,has_prefetch ? PP_EVENT_PREFETCH_ONLY :
                       PP_EVENT_ALL_BLOCKED,now);
            return;
        }
        engine->last_selection_forward=best.forward;
        h=entry(engine,best.guid,1);
        ++h->attempts;
        if (++engine->next_attempt_id==0u) ++engine->next_attempt_id;
        engine->active_attempt_id=engine->next_attempt_id;
        submit_status=engine->api.cast_on_guid(engine->api.ctx,best.guid,
                                               engine->active_attempt_id);
        if (submit_status==1) {
            engine->active=best.guid;
            engine->active_valid=1u;
            engine->started_ms=now;
            ++engine->casts;
            emit(engine,PP_EVENT_CAST,best.guid);
            return; /* never submit two casts in one loader pulse */
        }
        if (engine->api.end_attempt)
            engine->api.end_attempt(engine->api.ctx,best.guid,engine->active_attempt_id);
        if (submit_status==PP_CAST_LOCAL_RANGE) {
            /* No packet left the client: do not consume the GUID's server
             * retry budget or leave a pending result observer armed. */
            --h->attempts;
            h->blocked_until_ms=now+PP_LOCAL_RANGE_BACKOFF_MS;
            emit(engine,PP_EVENT_LOCAL_RANGE_REJECT,best.guid);
            continue;
        }
        failure(engine,best.guid,now,PP_RETRY_DELAY_MS,PP_EVENT_RETRY);
        ++engine->retries;
        return; /* other errors may signal server cooldown; do not burst */
    }
}
