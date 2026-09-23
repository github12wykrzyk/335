#include "../src/AutoPickPocket/autopickpocket_12340_adapter.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);return 1;} } while(0)
#define CONN 0x00100000u
#define MANAGER 0x00200000u
#define PLAYER 0x00300000u
#define NPC_A 0x00400000u
#define NPC_B 0x00500000u
#define DESC_A 0x00600000u
#define DESC_B 0x00700000u
typedef struct {
    uint32_t thread,hash_ok,abi_ok,usable,eligible_a,eligible_b;
    uint64_t world;
    PpResult result;
    unsigned casts,cast_spell,callbacks,events[16];
    uint32_t last_attempt,expected_result_attempt;
    unsigned player_pos_calls,move_player_after_scan;
    unsigned eligible_calls;
    uint32_t clock_value;
    float npc_b_distance;
    PpGuid last_guid;
} Mock;
static int digest(void *p,const char *expected){
    return ((Mock*)p)->hash_ok && !strcmp(expected,PP12340_CLIENT_SHA256);
}
static int abi(void *p,uintptr_t spell,uintptr_t pos) {
    return ((Mock*)p)->abi_ok && spell==PP12340_CAST_GUID_VA &&
           pos==PP12340_POSITION_VA;
}
static uint32_t tid(void *p){return ((Mock*)p)->thread;}
static uint32_t mock_clock(void *p){Mock *m=(Mock*)p;m->clock_value+=3u;return m->clock_value;}
static uint64_t world(void *p){return ((Mock*)p)->world;}
static int read32(void *p,uintptr_t addr,uint32_t *out){
    (void)p;
    switch(addr) {
    case PP12340_CONNECTION_VA: *out=CONN;break;
    case CONN+PP12340_MANAGER_OFFSET:*out=MANAGER;break;
    case MANAGER+0xC0u:*out=0xAABBCCDDu;break;
    case MANAGER+0xC4u:*out=0x01020304u;break;
    case MANAGER+0xACu:*out=PLAYER;break;
    case PLAYER+0x30u:*out=0xAABBCCDDu;break;
    case PLAYER+0x34u:*out=0x01020304u;break;
    case PLAYER+0x3Cu:*out=NPC_A;break;
    case NPC_A+0x30u:*out=111u;break;
    case NPC_A+0x34u:*out=10u;break;
    case NPC_A+0x3Cu:*out=NPC_B;break;
    case NPC_A+0x14u:*out=PP12340_UNIT_TYPE;break;
    case NPC_A+0x08u:*out=DESC_A;break;
    case DESC_A+0x18u*4u:*out=120u;break;
    case NPC_B+0x30u:*out=222u;break;
    case NPC_B+0x34u:*out=20u;break;
    case NPC_B+0x3Cu:*out=0u;break;
    case NPC_B+0x14u:*out=PP12340_UNIT_TYPE;break;
    case NPC_B+0x08u:*out=DESC_B;break;
    case DESC_B+0x18u*4u:*out=200u;break;
    default:return 0;
    }
    return 1;
}
static int pos(void *p,uintptr_t obj,float out[3]){
    Mock *m=(Mock*)p;out[1]=0;out[2]=0;
    if(obj==PLAYER) {
        ++m->player_pos_calls;
        out[0]=m->move_player_after_scan && m->player_pos_calls>1u ? 12.0f : 0.0f;
    }
    else if(obj==NPC_A)out[0]=3;
    else if(obj==NPC_B)out[0]=m->npc_b_distance;
    else return 0;
    return 1;
}
static int eligible(void *p,uintptr_t obj,PpGuid g){
    Mock *m=(Mock*)p;
    ++m->eligible_calls;
    if (obj==NPC_A)return m->eligible_a && g.lo==111 && g.hi==10;
    if (obj==NPC_B)return m->eligible_b && g.lo==222 && g.hi==20;
    return 0;
}
static int usable(void *p,uint32_t spell){
    Mock *m=(Mock*)p;
    return m->usable && spell==921u;
}
static int cast(void *p,uintptr_t addr,uint32_t spell,PpGuid target,uint32_t attempt){
    Mock *m=(Mock*)p;
    if(addr!=PP12340_CAST_GUID_VA || spell!=921u)return 0;
    ++m->casts;m->cast_spell=spell;m->last_guid=target;m->last_attempt=attempt;
    return 1;
}
static PpResult result(void *p,PpGuid g,uint32_t attempt){Mock *m=(Mock*)p;(void)g;return m->expected_result_attempt && m->expected_result_attempt!=attempt ? PP_RESULT_PENDING : m->result;}
static void event(void *p,PpEvent ev,PpGuid g,uint32_t attempt){Mock *m=(Mock*)p;(void)g;(void)attempt;++m->callbacks;++m->events[ev];}
static Pp12340Host host(Mock *m) {
    Pp12340Host h;memset(&h,0,sizeof(h));h.ctx=m;
    h.verify_exe_sha256=digest;h.verify_cast_abi=abi;h.thread_id=tid;
    h.clock_ms=mock_clock;
    h.read_u32=read32;h.position=pos;h.eligible_npc=eligible;
    h.spell_usable=usable;h.cast_guid=cast;h.cast_result=result;
    h.world_token=world;h.event=event;return h;
}
static void defaults(Mock *m) {
    memset(m,0,sizeof(*m));m->thread=1u;m->hash_ok=1u;m->abi_ok=1u;
    m->usable=1u;m->eligible_a=1u;m->eligible_b=1u;m->world=0x01010101u;
    m->npc_b_distance=2.0f;
}
static int test_bind_guard(void){
    Mock m;Pp12340Adapter a;Pp12340Host h;defaults(&m);h=host(&m);
    m.hash_ok=0;CHECK(!pp12340_bind(&a,&h));
    m.hash_ok=1;m.abi_ok=0;CHECK(!pp12340_bind(&a,&h));
    m.abi_ok=1;m.world=0;CHECK(!pp12340_bind(&a,&h));
    m.world=1u;CHECK(pp12340_bind(&a,&h));
    CHECK(!a.engine.enabled);return 0;
}
static int test_scan_cast_history_world(void){
    Mock m;Pp12340Adapter a;Pp12340Host h;defaults(&m);h=host(&m);
    CHECK(pp12340_bind(&a,&h));pp12340_enable(&a,1);
    pp12340_tick(&a,10);CHECK(m.casts==1 && m.last_guid.lo==222 && m.last_guid.hi==20);
    pp12340_tick(&a,20);CHECK(m.casts==1 && a.engine.successes==0);
    m.result=PP_RESULT_SUCCESS;pp12340_tick(&a,30);
    CHECK(a.engine.successes==1);
    m.result=PP_RESULT_PENDING;pp12340_tick(&a,110);
    CHECK(m.casts==2 && m.last_guid.lo==111);
    m.result=PP_RESULT_EMPTY;pp12340_tick(&a,111);CHECK(a.engine.empty==1);
    pp12340_tick(&a,400);CHECK(m.casts==2); /* per GUID memory */
    m.world=2u;pp12340_tick(&a,500);
    CHECK(m.casts==3 && m.last_guid.lo==222);
    m.thread=3u;pp12340_tick(&a,2000);CHECK(a.engine.active_valid);
    pp12340_enable(&a,0);CHECK(a.engine.enabled); /* foreign thread blocked */
    m.thread=1u;pp12340_enable(&a,0);CHECK(!a.engine.enabled);
    pp12340_enable(&a,1);
    m.world=0u;pp12340_tick(&a,2100);
    CHECK(a.engine.enabled && !a.engine.active_valid && m.casts==3);
    CHECK(m.events[PP_EVENT_WORLD_PAUSED]==1);
    m.world=3u;pp12340_tick(&a,2200);
    CHECK(a.engine.enabled && m.casts==4 && m.last_guid.lo==222);
    CHECK(m.events[PP_EVENT_WORLD_RESET]==2);
    return 0;
}
static int test_attempt_correlation(void){
    Mock m;Pp12340Adapter a;Pp12340Host h;uint32_t old_id;
    defaults(&m);h=host(&m);CHECK(pp12340_bind(&a,&h));pp12340_enable(&a,1);
    pp12340_tick(&a,0);old_id=m.last_attempt;CHECK(old_id!=0u);
    /* Timeout immediately scans the next unblocked NPC in the same pulse. */
    pp12340_tick(&a,1500);
    CHECK(m.casts==2 && m.last_attempt!=old_id);
    m.result=PP_RESULT_SUCCESS;m.expected_result_attempt=old_id;
    pp12340_tick(&a,1501);CHECK(a.engine.successes==0 && a.engine.active_valid);
    m.expected_result_attempt=m.last_attempt;
    pp12340_tick(&a,1502);CHECK(a.engine.successes==1);
    return 0;
}
static int test_silent_commands(void){
    Mock m;Pp12340Adapter a;Pp12340Host h;defaults(&m);h=host(&m);
    CHECK(pp12340_bind(&a,&h));
    CHECK(!pp12340_command(&a,"invalid") && !a.engine.enabled);
    CHECK(pp12340_command(&a,"  on\t") && a.engine.enabled);
    CHECK(m.events[PP_EVENT_ENABLED]==1);
    pp12340_tick(&a,0);CHECK(m.casts==1);
    CHECK(pp12340_command(&a,"reset") && !a.engine.active_valid);
    CHECK(m.events[PP_EVENT_RESET]==1);
    CHECK(pp12340_command(&a,"off") && !a.engine.enabled);
    CHECK(m.events[PP_EVENT_DISABLED]==1);
    m.thread=9u;CHECK(!pp12340_command(&a,"on") && !a.engine.enabled);
    m.thread=1u;CHECK(!pp12340_command(&a,"") && !a.engine.enabled);
    return 0;
}
static int test_moving_player_rechecked_before_cast(void){
    Mock m;Pp12340Adapter a;Pp12340Host h;defaults(&m);h=host(&m);
    CHECK(pp12340_bind(&a,&h));pp12340_enable(&a,1);
    m.move_player_after_scan=1u;
    pp12340_tick(&a,0);
    CHECK(m.player_pos_calls>=2u && m.casts==0u && a.engine.retries==1u);
    /* Rejected GUID is backoff-limited; another eligible GUID can proceed. */
    m.move_player_after_scan=0u;
    pp12340_tick(&a,100);
    CHECK(m.casts==1u && m.last_guid.lo==111u);
    return 0;
}
static int test_fail_closed_filter(void){
    Mock m;Pp12340Adapter a;Pp12340Host h;defaults(&m);h=host(&m);
    CHECK(pp12340_bind(&a,&h));pp12340_enable(&a,1);
    m.usable=0;pp12340_tick(&a,0);CHECK(m.casts==0);
    m.usable=1;m.eligible_a=0;m.eligible_b=0;
    pp12340_tick(&a,100);CHECK(m.casts==0);
    m.eligible_a=1;pp12340_tick(&a,200);CHECK(m.casts==1 && m.last_guid.lo==111);
    return 0;
}
static int test_spatial_gate_avoids_distant_eligibility_calls(void){
 Mock m;Pp12340Adapter a;Pp12340Host h;defaults(&m);h=host(&m);
 CHECK(pp12340_bind(&a,&h));pp12340_enable(&a,1);
 m.move_player_after_scan=1u; /* after player located: scan still near */
 pp12340_tick(&a,0u);
 CHECK(m.eligible_calls<=3u); /* only candidates within reach */
 return 0;
}
static int test_cached_player_revalidates_and_resets(void){
 Mock m;Pp12340Adapter a;Pp12340Host h;defaults(&m);h=host(&m);
 CHECK(pp12340_bind(&a,&h));pp12340_enable(&a,1);
 pp12340_tick(&a,0u);
 CHECK(a.cached_manager==MANAGER && a.cached_player_obj==PLAYER);
 CHECK(a.last_scan_candidates==2u && a.last_scan_duration_ms==3u);
 pp12340_reset(&a);
 CHECK(a.cached_manager==0u && a.cached_player_obj==0u);
 pp12340_tick(&a,20u);
 CHECK(a.cached_manager==MANAGER && a.cached_player_obj==PLAYER);
 return 0;
}
static int test_borderline_range_is_skipped_before_native_cast(void){
 Mock m;Pp12340Adapter a;Pp12340Host h;defaults(&m);h=host(&m);
 m.npc_b_distance=4.5f;
 CHECK(pp12340_bind(&a,&h));pp12340_enable(&a,1);
 pp12340_tick(&a,0u);
 CHECK(m.casts==1u && m.last_guid.lo==111u);
 return 0;
}
int main(void){
    if(test_cached_player_revalidates_and_resets()||test_borderline_range_is_skipped_before_native_cast()||test_spatial_gate_avoids_distant_eligibility_calls()||test_bind_guard()||test_scan_cast_history_world()||
       test_attempt_correlation()||test_silent_commands()||
       test_moving_player_rechecked_before_cast()||test_fail_closed_filter())return 1;
    puts("PP12340 native adapter mock: PASS");
    return 0;
}
