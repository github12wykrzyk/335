#include "../src/AutoPickPocket/autopickpocket_core.h"
#include <stdio.h>
#include <string.h>
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr); return 1; } } while (0)
typedef struct {
    PpTarget t[4]; size_t n;
    PpResult result;
    PpGuid casted[16]; unsigned cast_n, permitted, scans, events[20], cast_ok;
    uint32_t last_attempt, expected_result_attempt;
} Stub;
static size_t scan(void *p,PpTarget *out,size_t cap) {
    Stub *s=(Stub *)p;size_t n=s->n<cap?s->n:cap;
    memcpy(out,s->t,n*sizeof(*out));++s->scans;return n;
}
static int can_cast(void *p){return ((Stub *)p)->permitted;}
static int cast(void *p,PpGuid g,uint32_t attempt){Stub *s=(Stub *)p;s->last_attempt=attempt;if (!s->cast_ok)return 0;if(s->cast_n<16)s->casted[s->cast_n++]=g;return 1;}
static PpResult result(void *p,PpGuid g,uint32_t attempt){Stub *s=(Stub *)p;(void)g;return s->expected_result_attempt && s->expected_result_attempt!=attempt ? PP_RESULT_PENDING : s->result;}
static void event(void *p,PpEvent e,PpGuid g,uint32_t attempt){(void)g;(void)attempt; ++((Stub *)p)->events[e];}
static PpAdapter adapter(Stub *s){PpAdapter a;memset(&a,0,sizeof(a));a.ctx=s;a.scan=scan;a.can_cast=can_cast;a.cast_on_guid=cast;a.result=result;a.event=event;return a;}
static void init(Stub *s){memset(s,0,sizeof(*s));s->permitted=1;s->cast_ok=1;s->t[0].guid.lo=101;s->t[0].eligible=1;s->t[0].distance_sq=4;s->t[1].guid.lo=102;s->t[1].eligible=1;s->t[1].distance_sq=1;s->n=2;}
static int test_session(void){
 Stub s;PpEngine e;init(&s);CHECK(pp_init(&e,adapter(&s)));
 pp_tick(&e,0);CHECK(!s.scans);pp_enable(&e,1);
 pp_tick(&e,10);CHECK(s.cast_n==1 && s.casted[0].lo==102 && e.successes==0);
 pp_tick(&e,20);CHECK(s.cast_n==1); /* submission is NOT success */
 s.result=PP_RESULT_SUCCESS;pp_tick(&e,30);CHECK(e.successes==1 && s.events[PP_EVENT_SUCCESS]==1);
 s.result=PP_RESULT_PENDING;pp_tick(&e,110);CHECK(s.cast_n==2 && s.casted[1].lo==101);
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
 pp_tick(&e,4501);CHECK(e.successes==0 && e.active_valid);
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
 CHECK(s.events[PP_EVENT_NOT_CASTABLE]==2 && s.scans==0);
 s.permitted=1;pp_tick(&e,1100);pp_tick(&e,2000);
 CHECK(s.events[PP_EVENT_NO_CANDIDATES]==1);
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
int main(void){if(test_selected_probe_is_single_shot()||test_probe_rejects_without_substituting()||test_session()||test_retry_and_timeout()||test_unconfirmed_results_bounded()||test_late_result_cannot_complete_new_attempt()||test_idle_diagnostics_are_sampled()||test_filter_and_wrap())return 1;puts("AutoPickPocket portable core tests: PASS");return 0;}
