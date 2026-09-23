#include "../src/AutoPickPocket/autopickpocket_core.h"
#include <stdio.h>
#include <string.h>
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr); return 1; } } while (0)
typedef struct {
    PpTarget t[4]; size_t n;
    PpResult result;
    PpGuid casted[16]; unsigned cast_n, permitted, scans, events[20], cast_ok;
    uint32_t last_attempt, expected_result_attempt;
    uint32_t end_attempt_id; PpGuid end_guid; unsigned end_count;
} Stub;
static size_t scan(void *p,PpTarget *out,size_t cap) {
    Stub *s=(Stub *)p;size_t n=s->n<cap?s->n:cap;
    memcpy(out,s->t,n*sizeof(*out));++s->scans;return n;
}
static int can_cast(void *p){return ((Stub *)p)->permitted;}
static int cast(void *p,PpGuid g,uint32_t attempt){Stub *s=(Stub *)p;s->last_attempt=attempt;if (!s->cast_ok)return 0;if(s->cast_n<16)s->casted[s->cast_n++]=g;return 1;}
static void end_attempt(void *p,PpGuid g,uint32_t attempt){
 Stub *s=(Stub*)p;s->end_guid=g;s->end_attempt_id=attempt;++s->end_count;
}
static PpResult result(void *p,PpGuid g,uint32_t attempt){Stub *s=(Stub *)p;(void)g;return s->expected_result_attempt && s->expected_result_attempt!=attempt ? PP_RESULT_PENDING : s->result;}
static void event(void *p,PpEvent e,PpGuid g,uint32_t attempt){(void)g;(void)attempt; ++((Stub *)p)->events[e];}
static PpAdapter adapter(Stub *s){PpAdapter a;memset(&a,0,sizeof(a));a.ctx=s;a.scan=scan;a.can_cast=can_cast;a.cast_on_guid=cast;a.result=result;a.end_attempt=end_attempt;a.event=event;return a;}
static void init(Stub *s){memset(s,0,sizeof(*s));s->permitted=1;s->cast_ok=1;s->t[0].guid.lo=101;s->t[0].eligible=1;s->t[0].distance_sq=4;s->t[1].guid.lo=102;s->t[1].eligible=1;s->t[1].distance_sq=1;s->n=2;}
static int test_session(void){
 Stub s;PpEngine e;init(&s);CHECK(pp_init(&e,adapter(&s)));
 pp_tick(&e,0);CHECK(!s.scans);pp_enable(&e,1);
 pp_tick(&e,10);CHECK(s.cast_n==1 && s.casted[0].lo==102 && e.successes==0);
 pp_tick(&e,20);CHECK(s.cast_n==1); /* submission is NOT success */
 s.result=PP_RESULT_SUCCESS;pp_tick(&e,30);
 CHECK(e.successes==1 && s.events[PP_EVENT_SUCCESS]==1);
 CHECK(s.cast_n==1); /* default burst enforces >=100 ms between submissions */
 s.result=PP_RESULT_PENDING;pp_tick(&e,110);
 CHECK(s.cast_n==2 && s.casted[1].lo==101);
 s.result=PP_RESULT_EMPTY;pp_tick(&e,111);CHECK(e.empty==1);
 pp_tick(&e,300);CHECK(s.cast_n==2); /* both GUIDs terminal */
 pp_reset(&e);pp_tick(&e,401);CHECK(s.cast_n==3 && s.casted[2].lo==102);
 pp_enable(&e,0);pp_tick(&e,1000);CHECK(s.cast_n==3);
 return 0;
}
static int test_retry_and_timeout(void){
 Stub s;PpEngine e;init(&s);s.n=1;CHECK(pp_init(&e,adapter(&s)));pp_enable(&e,1);
 s.cast_ok=0;pp_tick(&e,0);CHECK(s.cast_n==0 && e.retries==1);
 s.cast_ok=1;pp_tick(&e,100);CHECK(s.cast_n==0);pp_tick(&e,800);CHECK(s.cast_n==1);
 pp_tick(&e,2300);CHECK(e.timeouts==1 && e.successes==0);
 pp_tick(&e,2400);CHECK(s.cast_n==1);pp_tick(&e,5300);CHECK(s.cast_n==2);
 s.result=PP_RESULT_RETRYABLE;pp_tick(&e,5301);CHECK(e.retries==2);
 pp_tick(&e,6000);CHECK(s.cast_n==2);pp_tick(&e,6200);CHECK(s.cast_n==2);
 CHECK(s.events[PP_EVENT_GAVE_UP]==1); /* third attempted cast exhausted budget */
 pp_reset(&e);s.result=PP_RESULT_PENDING;pp_tick(&e,6300);
 CHECK(s.cast_n==3); /* explicit reset makes GUID eligible again */
 return 0;
}
static int test_unconfirmed_results_bounded(void){
 Stub s;PpEngine e;init(&s);s.n=1;CHECK(pp_init(&e,adapter(&s)));pp_enable(&e,1);
 pp_tick(&e,0);CHECK(s.cast_n==1);
 pp_tick(&e,1500);CHECK(e.timeouts==1);
 pp_tick(&e,4500);CHECK(s.cast_n==2);
 pp_tick(&e,6000);CHECK(e.timeouts==2);
 pp_tick(&e,9000);CHECK(s.cast_n==3);
 pp_tick(&e,10500);CHECK(e.timeouts==3 && s.events[PP_EVENT_GAVE_UP]==1);
 pp_tick(&e,60000);CHECK(s.cast_n==3); /* no unbounded retries on unknown result */
 return 0;
}
static int test_late_result_cannot_complete_new_attempt(void){
 Stub s;PpEngine e;uint32_t first,second;init(&s);s.n=1;
 CHECK(pp_init(&e,adapter(&s)));pp_enable(&e,1);
 pp_tick(&e,0);first=s.last_attempt;CHECK(first!=0u);
 pp_tick(&e,1500);pp_tick(&e,4500);second=s.last_attempt;
 CHECK(s.cast_n==2 && second!=first);
 s.result=PP_RESULT_SUCCESS;s.expected_result_attempt=first;
 pp_tick(&e,4501);CHECK(e.successes==0 && e.pending[0].valid);
 s.expected_result_attempt=second;pp_tick(&e,4502);
 CHECK(e.successes==1 && !e.active_valid);
 pp_reset(&e);s.result=PP_RESULT_PENDING;pp_tick(&e,4700);
 CHECK(s.last_attempt!=second && s.last_attempt!=first);
 return 0;
}
static int test_idle_diagnostics_are_sampled(void){
 Stub s;PpEngine e;init(&s);s.n=0;s.permitted=0;
 CHECK(pp_init(&e,adapter(&s)));pp_enable(&e,1);
 pp_tick(&e,0);pp_tick(&e,100);pp_tick(&e,1000);
 CHECK(s.scans==3u && s.cast_n==0u); /* scan runs during cooldown */
 s.permitted=1;pp_tick(&e,1100);pp_tick(&e,2000);
 CHECK(s.events[PP_EVENT_NO_CANDIDATES]>=1u);
 s.n=1;s.t[0].eligible=0;pp_tick(&e,3000);
 CHECK(s.events[PP_EVENT_ALL_BLOCKED]==1 && s.cast_n==0);
 return 0;
}
static int test_filter_and_wrap(void){
 Stub s;PpEngine e;init(&s);CHECK(pp_init(&e,adapter(&s)));pp_enable(&e,1);
 s.t[1].eligible=0;s.t[0].distance_sq=-1.0f;
 pp_tick(&e,0xfffffff0u);CHECK(s.cast_n==0);
 s.t[0].distance_sq=2.0f;s.permitted=0;pp_tick(&e,0x60u);CHECK(s.cast_n==0);
 s.permitted=1;pp_tick(&e,0xc5u);CHECK(s.cast_n==1 && s.casted[0].lo==101);
 return 0;
}
static int test_selected_probe_is_single_shot(void) {
 Stub s;PpEngine e;PpGuid chosen={101u,0u};init(&s);
 CHECK(pp_init(&e,adapter(&s)));
 CHECK(pp_probe_once(&e,chosen,10u));
 CHECK(s.cast_n==1 && s.casted[0].lo==101u);
 pp_tick(&e,20u);CHECK(s.cast_n==1 && e.successes==0u);
 s.result=PP_RESULT_SUCCESS;pp_tick(&e,30u);
 CHECK(e.successes==1 && !e.enabled);
 pp_tick(&e,500u);CHECK(s.cast_n==1);
 CHECK(!pp_probe_once(&e,chosen,600u));
 pp_reset(&e);s.result=PP_RESULT_PENDING;
 CHECK(pp_probe_once(&e,chosen,700u));
 pp_tick(&e,2200u);CHECK(e.timeouts==1 && !e.enabled);
 pp_tick(&e,10000u);CHECK(s.cast_n==2);
 return 0;
}
static int test_probe_rejects_without_substituting(void) {
 Stub s;PpEngine e;PpGuid chosen={101u,0u};init(&s);
 CHECK(pp_init(&e,adapter(&s)));
 s.t[0].eligible=0;
 CHECK(!pp_probe_once(&e,chosen,0u));
 CHECK(s.cast_n==0 && s.events[PP_EVENT_PROBE_REJECTED]==1);
 s.t[0].eligible=1;s.cast_ok=0;
 CHECK(!pp_probe_once(&e,chosen,10u));
 s.cast_ok=1;
 CHECK(!pp_probe_once(&e,chosen,20u) && s.cast_n==0);
 pp_reset(&e);
 CHECK(pp_probe_once(&e,chosen,30u));
 CHECK(s.cast_n==1 && s.casted[0].lo==101u);
 return 0;
}
static int test_next_target_rescan_no_extra_tick(void){
 Stub s;PpEngine e;init(&s);CHECK(pp_init(&e,adapter(&s)));pp_enable(&e,1);
 pp_tick(&e,10u);CHECK(s.cast_n==1 && s.casted[0].lo==102u);
 s.result=PP_RESULT_SUCCESS;
 pp_tick(&e,11u);CHECK(s.cast_n==1u && e.successes==1u);
 CHECK(s.scans==1u); /* second GUID from the initial bounded queue */
 CHECK(s.events[PP_EVENT_SUCCESS]==1);
 s.result=PP_RESULT_PENDING;
 pp_tick(&e,110u);CHECK(s.cast_n==2u && s.casted[1].lo==101u);
 s.result=PP_RESULT_EMPTY;
 pp_tick(&e,111u);CHECK(s.cast_n==2 && s.events[PP_EVENT_EMPTY]==1);
 return 0;
}
static int test_failed_or_timedout_npc_does_not_block_next_guid(void){
 Stub s;PpEngine e;init(&s);CHECK(pp_init(&e,adapter(&s)));pp_enable(&e,1);
 pp_tick(&e,10u);CHECK(s.cast_n==1 && s.casted[0].lo==102u);
 s.result=PP_RESULT_RETRYABLE;
 pp_tick(&e,11u);CHECK(e.retries==1u && s.cast_n==1u);
 pp_tick(&e,110u);CHECK(s.cast_n==2 && s.casted[1].lo==101u);
 pp_reset(&e);s.result=PP_RESULT_PENDING;
 pp_tick(&e,200u);CHECK(s.cast_n==3 && s.casted[2].lo==102u);
 pp_tick(&e,600u);CHECK(e.timeouts==1u && s.cast_n==4 &&
                         s.casted[3].lo==101u);
 return 0;
}
static int test_stale_queue_rebuilds_before_cast(void){
 Stub s;PpEngine e;init(&s);CHECK(pp_init(&e,adapter(&s)));pp_enable(&e,1);
 pp_tick(&e,10u);CHECK(s.cast_n==1 && s.casted[0].lo==102u);
 CHECK(s.scans==1u && e.queue_count==2u);
 /* NPC 101 disappeared, GUID 103 spawned while result was pending. */
 s.n=1u;s.t[0].guid.lo=103u;s.t[0].eligible=1u;
 s.t[0].distance_sq=1.0f;
 s.result=PP_RESULT_SUCCESS;
 pp_tick(&e,1300u); /* continuous refresh replaced old NPC */
 CHECK(s.cast_n==2 && s.casted[1].lo==103u && s.scans==2u);
 return 0;
}
static int test_queue_dropped_on_disable_and_world_reset(void){
 Stub s;PpEngine e;init(&s);CHECK(pp_init(&e,adapter(&s)));pp_enable(&e,1);
 pp_tick(&e,10u);CHECK(e.queue_count==2u);
 pp_enable(&e,0);CHECK(e.queue_count==0u);
 pp_enable(&e,1);pp_tick(&e,20u);CHECK(e.queue_count==2u);
 pp_reset(&e);CHECK(e.queue_count==0u);
 return 0;
}
static int test_read_ahead_while_cast_pending_and_gcd(void){
 Stub s;PpEngine e;init(&s);s.n=1u;
 CHECK(pp_init(&e,adapter(&s)));pp_enable(&e,1);
 pp_tick(&e,0u);CHECK(s.cast_n==1u);
 s.n=2u;s.t[1].guid.lo=103u;s.t[1].eligible=2u;
 s.t[1].distance_sq=49.0f;
 pp_tick(&e,80u);CHECK(s.scans==2u && s.cast_n==1u);
 s.t[1].eligible=1u;s.t[1].distance_sq=4.0f;
 s.permitted=0u;pp_tick(&e,160u);
 CHECK(s.scans==3u && s.cast_n==1u && e.queue_count==2u);
 s.result=PP_RESULT_SUCCESS;
 pp_tick(&e,161u);CHECK(e.successes==1u && s.cast_n==1u);
 s.permitted=1u;pp_tick(&e,162u);
 CHECK(s.cast_n==2u && s.casted[1].lo==103u && s.scans==3u);
 return 0;
}
static int test_prefetched_outside_range_never_submitted(void){
 Stub s;PpEngine e;init(&s);s.n=1u;s.t[0].eligible=2u;
 CHECK(pp_init(&e,adapter(&s)));pp_enable(&e,1);
 pp_tick(&e,0u);CHECK(s.cast_n==0u && e.queue_count==1u);
 pp_tick(&e,80u);CHECK(s.cast_n==0u);
 s.t[0].eligible=1u;pp_tick(&e,160u);
 CHECK(s.cast_n==1u && s.casted[0].lo==101u);
 return 0;
}
static int test_default_burst_two_npcs_without_result_ack(void){
 Stub s;PpEngine e;init(&s);
 CHECK(pp_init(&e,adapter(&s)));pp_enable(&e,1);
 pp_tick(&e,10u);
 CHECK(e.burst_mode==1u && s.cast_n==1u && e.pending[0].valid);
 CHECK(s.casted[0].lo==102u && e.successes==0u);
 pp_tick(&e,109u);CHECK(s.cast_n==1u);
 pp_tick(&e,110u);
 CHECK(s.cast_n==2u && s.casted[1].lo==101u && e.successes==0u);
 CHECK(e.pending[0].valid && e.pending[1].valid);
 pp_tick(&e,410u);
 CHECK(e.timeouts==1u && s.end_guid.lo==102u);
 pp_tick(&e,510u);
 CHECK(e.timeouts==2u && s.end_guid.lo==101u);
 return 0;
}
static int test_four_nearby_npcs_burst_100ms_without_waiting_for_money(void){
 Stub s;PpEngine e;uint32_t nonce[4];unsigned i;init(&s);
 s.n=4u;
 for(i=0u;i<4u;++i){
    s.t[i].guid.lo=101u+i;s.t[i].eligible=1u;
    s.t[i].distance_sq=1.0f+(float)i;
 }
 s.result=PP_RESULT_PENDING;
 CHECK(pp_init(&e,adapter(&s)));pp_enable(&e,1);
 pp_tick(&e,10u);nonce[0]=s.last_attempt;
 CHECK(s.cast_n==1u && s.casted[0].lo==101u && e.pending[0].valid);
 pp_tick(&e,109u);CHECK(s.cast_n==1u);
 pp_tick(&e,110u);nonce[1]=s.last_attempt;
 CHECK(s.cast_n==2u && s.casted[1].lo==102u);
 pp_tick(&e,210u);nonce[2]=s.last_attempt;
 CHECK(s.cast_n==3u && s.casted[2].lo==103u);
 pp_tick(&e,310u);nonce[3]=s.last_attempt;
 CHECK(s.cast_n==4u && s.casted[3].lo==104u);
 CHECK(s.scans>=4u && e.timeouts==0u && e.successes==0u);
 CHECK(e.pending[0].valid && e.pending[1].valid &&
       e.pending[2].valid && e.pending[3].valid);
 s.result=PP_RESULT_SUCCESS;s.expected_result_attempt=nonce[2];
 pp_tick(&e,320u);CHECK(e.successes==1u && e.timeouts==0u);
 CHECK(s.events[PP_EVENT_SUCCESS]==1u && s.cast_n==4u);
 s.expected_result_attempt=nonce[0];pp_tick(&e,321u);
 CHECK(e.successes==2u && s.cast_n==4u);
 s.expected_result_attempt=nonce[1];pp_tick(&e,322u);
 CHECK(e.successes==3u && s.cast_n==4u);
 s.expected_result_attempt=nonce[3];pp_tick(&e,323u);
 CHECK(e.successes==4u && s.cast_n==4u);
 pp_tick(&e,510u);CHECK(e.successes==4u && e.timeouts==0u);
 return 0;
}
static int test_burst_timeout_exact_guid_and_reset_release(void){
 Stub s;PpEngine e;unsigned i;init(&s);
 s.n=4u;
 for(i=0u;i<4u;++i){
    s.t[i].guid.lo=101u+i;s.t[i].eligible=1u;
    s.t[i].distance_sq=1.0f+(float)i;
 }
 CHECK(pp_init(&e,adapter(&s)));pp_enable(&e,1);
 pp_tick(&e,0u);pp_tick(&e,100u);pp_tick(&e,200u);pp_tick(&e,300u);
 CHECK(s.cast_n==4u && s.end_count==0u);
 pp_tick(&e,400u);CHECK(e.timeouts==1u && s.end_count==1u);
 CHECK(s.end_guid.lo==101u);
 pp_enable(&e,0);CHECK(s.end_count==4u && !e.pending[0].valid);
 pp_enable(&e,1);pp_tick(&e,500u);
 CHECK(s.cast_n==5u && s.casted[4].lo==102u); /* other GUIDs stay eligible */
 return 0;
}
static int test_money_loot_signal_finishes_attempt_before_timeout(void){
 Stub s;PpEngine e;init(&s);CHECK(pp_init(&e,adapter(&s)));pp_enable(&e,1);
 pp_tick(&e,10u);CHECK(s.cast_n==1u && s.casted[0].lo==102u);
 s.result=PP_RESULT_MONEY_SUCCESS;
 pp_tick(&e,110u);CHECK(e.successes==1u && e.timeouts==0u);
 CHECK(s.events[PP_EVENT_MONEY_SUCCESS]==1u && s.events[PP_EVENT_SUCCESS]==0u);
 CHECK(s.cast_n==2u && s.casted[1].lo==101u);
 s.result=PP_RESULT_PENDING;
 pp_tick(&e,509u);CHECK(e.timeouts==0u);
 pp_tick(&e,510u);CHECK(e.timeouts==1u);
 return 0;
}
static int test_timeout_releases_matching_attempt_before_next_guid(void){
 Stub s;PpEngine e;uint32_t first;init(&s);
 CHECK(pp_init(&e,adapter(&s)));pp_enable(&e,1);
 pp_tick(&e,10u);first=s.last_attempt;
 CHECK(s.cast_n==1u && s.casted[0].lo==102u && s.end_count==0u);
 pp_tick(&e,410u); /* exact 400 ms timeout: release before next GUID */
 CHECK(e.timeouts==1u && s.end_count==1u);
 CHECK(s.end_guid.lo==102u && s.end_attempt_id==first);
 CHECK(s.cast_n==2u && s.casted[1].lo==101u && s.last_attempt!=first);
 s.result=PP_RESULT_SUCCESS;s.expected_result_attempt=first;
 pp_tick(&e,411u);CHECK(e.successes==0u && e.pending[0].valid);
 s.expected_result_attempt=s.last_attempt;
 pp_tick(&e,412u);CHECK(e.successes==1u);
 return 0;
}
static int test_reset_and_refusal_release_only_own_nonce(void){
 Stub s;PpEngine e;init(&s);
 CHECK(pp_init(&e,adapter(&s)));pp_enable(&e,1);
 pp_tick(&e,10u);CHECK(s.cast_n==1u);
 pp_enable(&e,0);CHECK(s.end_count==1u && s.end_attempt_id==s.last_attempt);
 pp_enable(&e,1);s.result=PP_RESULT_PENDING;pp_tick(&e,80u);
 CHECK(s.cast_n==2u);
 pp_reset(&e);CHECK(s.end_count==2u && s.end_attempt_id==s.last_attempt);
 s.cast_ok=0;pp_tick(&e,160u);
 CHECK(s.end_count==3u && s.end_guid.lo==102u && !e.active_valid);
 return 0;
}
int main(void){if(test_default_burst_two_npcs_without_result_ack()||test_four_nearby_npcs_burst_100ms_without_waiting_for_money()||test_burst_timeout_exact_guid_and_reset_release()||test_money_loot_signal_finishes_attempt_before_timeout()||test_timeout_releases_matching_attempt_before_next_guid()||test_reset_and_refusal_release_only_own_nonce()||test_read_ahead_while_cast_pending_and_gcd()||test_prefetched_outside_range_never_submitted()||test_stale_queue_rebuilds_before_cast()||test_queue_dropped_on_disable_and_world_reset()||test_failed_or_timedout_npc_does_not_block_next_guid()||test_next_target_rescan_no_extra_tick()||test_selected_probe_is_single_shot()||test_probe_rejects_without_substituting()||test_session()||test_retry_and_timeout()||test_unconfirmed_results_bounded()||test_late_result_cannot_complete_new_attempt()||test_idle_diagnostics_are_sampled()||test_filter_and_wrap())return 1;puts("AutoPickPocket portable core tests: PASS");return 0;}
