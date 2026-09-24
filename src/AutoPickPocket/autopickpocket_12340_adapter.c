#include "autopickpocket_12340_adapter.h"
#include <string.h>
#include <float.h>
#define PP_MGR_FIRST 0xACu
#define PP_MGR_LOCAL_GUID 0xC0u
#define PP_OBJ_DESCRIPTOR 0x08u
#define PP_OBJ_TYPE 0x14u
#define PP_OBJ_GUID 0x30u
#define PP_OBJ_NEXT 0x3Cu
#define PP_UNIT_HEALTH_FIELD 0x18u
#define PP_SCAN_LIMIT 4096u
#define PP_PTR_MIN 0x10000u
#define PP_PTR_MAX 0x7FFE0000u

static int ptr_ok(uint32_t addr) {
    return addr >= PP_PTR_MIN && addr < PP_PTR_MAX;
}
static int read32(Pp12340Adapter *a, uintptr_t addr, uint32_t *out) {
    return out && addr >= PP_PTR_MIN &&
        addr < PP_PTR_MAX && a->host.read_u32(a->host.ctx,addr,out)==1;
}
static int guid_at(Pp12340Adapter *a, uintptr_t obj, PpGuid *out) {
    return read32(a,obj+PP_OBJ_GUID,&out->lo) &&
           read32(a,obj+PP_OBJ_GUID+4u,&out->hi);
}
static int same(PpGuid a, PpGuid b) {
    return a.lo==b.lo && a.hi==b.hi;
}
static int mgr(Pp12340Adapter *a,uint32_t *out) {
    uint32_t conn=0u;
    if (!read32(a,PP12340_CONNECTION_VA,&conn) || !ptr_ok(conn) ||
        conn>PP_PTR_MAX-PP12340_MANAGER_OFFSET ||
        !read32(a,(uintptr_t)conn+PP12340_MANAGER_OFFSET,out))
        return 0;
    return ptr_ok(*out);
}
/* Avoid an O(N) player-search on every 40 ms loader pulse. A cached
 * address is usable only while the object manager AND current player's
 * exact GUID still match. Every dereference goes through guarded read32. */
static int locate_player(Pp12340Adapter *a,uint32_t manager,
                         PpGuid local_guid,uint32_t *player){
    uint32_t obj=0u,next=0u;
    PpGuid found;
    unsigned i;
    if(a->cached_manager==manager && ptr_ok(a->cached_player_obj) &&
       guid_at(a,a->cached_player_obj,&found) &&
       same(found,local_guid)){
        *player=a->cached_player_obj;
        return 1;
    }
    a->cached_manager=0u;
    a->cached_player_obj=0u;
    if(!read32(a,(uintptr_t)manager+PP_MGR_FIRST,&obj))return 0;
    for(i=0u;i<PP_SCAN_LIMIT && ptr_ok(obj);++i){
        if(!guid_at(a,obj,&found))return 0;
        if(same(found,local_guid)){
            a->cached_manager=manager;
            a->cached_player_obj=obj;
            *player=obj;
            return 1;
        }
        if(!read32(a,(uintptr_t)obj+PP_OBJ_NEXT,&next) || next==obj)
            return 0;
        obj=next;
    }
    return 0;
}
static size_t pp_scan(void *ctx,PpTarget *out,size_t cap) {
    Pp12340Adapter *a=(Pp12340Adapter *)ctx;
    uint32_t manager=0u,obj=0u,player_obj=0u,next=0u,desc=0u;
    PpGuid player_guid, guid;
    float me[3],pos[3],step_x=0.0f,step_y=0.0f;
    unsigned moving=0u;
    size_t count=0u;
    uint32_t scan_started=a->host.clock_ms ? a->host.clock_ms(a->host.ctx) : 0u;
    unsigned i;
    a->last_scan_duration_ms=0u;
    a->last_scan_candidates=0u;
    if (!out || !cap || !mgr(a,&manager) ||
        !read32(a,(uintptr_t)manager+PP_MGR_LOCAL_GUID,&player_guid.lo) ||
        !read32(a,(uintptr_t)manager+PP_MGR_LOCAL_GUID+4u,&player_guid.hi) ||
        !locate_player(a,manager,player_guid,&player_obj) ||
        a->host.position(a->host.ctx,player_obj,me)!=1 ||
        !read32(a,(uintptr_t)manager+PP_MGR_FIRST,&obj)) return 0u;
    /* Reject stationary samples, time gaps and teleport-sized deltas.
     * Movement is used to rank only, never to relax native cast range. */
    if(a->player_sample_valid && a->host.clock_ms){
        uint32_t elapsed=(uint32_t)(scan_started-a->last_player_sample_ms);
        float step_sq;
        step_x=me[0]-a->last_player_xyz[0];
        step_y=me[1]-a->last_player_xyz[1];
        step_sq=step_x*step_x+step_y*step_y;
        if(elapsed>=20u && elapsed<=250u && step_sq>=0.04f &&
           step_sq<=16.0f &&
           step_sq<=(0.025f*(float)elapsed)*(0.025f*(float)elapsed))
            moving=1u;
    }
    memcpy(a->last_player_xyz,me,sizeof(me));
    a->last_player_sample_ms=scan_started;
    a->player_sample_valid=a->host.clock_ms ? 1u : 0u;
    for(i=0u;i<PP_SCAN_LIMIT && ptr_ok(obj);++i) {
        uint32_t type=0u,health=0u;
        if (!guid_at(a,obj,&guid)) break;
        /* Spatial gate first: native creature-type ABI and policy checks
         * are expensive (VirtualQuery + code byte checks) and must not
         * run for the thousands of distant units in the object manager. */
        if (!same(player_guid,guid) &&
            read32(a,(uintptr_t)obj+PP_OBJ_TYPE,&type) &&
            type==PP12340_UNIT_TYPE &&
            read32(a,(uintptr_t)obj+PP_OBJ_DESCRIPTOR,&desc) &&
            ptr_ok(desc) &&
            read32(a,(uintptr_t)desc+4u*PP_UNIT_HEALTH_FIELD,&health) &&
            health>0u &&
            a->host.position(a->host.ctx,obj,pos)==1) {
            float dx=pos[0]-me[0],dy=pos[1]-me[1],dz=pos[2]-me[2];
            float d2=dx*dx+dy*dy+dz*dz;
            if (d2>=0.0f && d2<=PP12340_DETECT_REACH*PP12340_DETECT_REACH &&
                d2<FLT_MAX &&
                a->host.eligible_npc(a->host.ctx,obj,guid)==1) {
                size_t far=0u,n;
                if (count<cap) {
                    out[count].guid=guid;
                    out[count].distance_sq=d2;
                    out[count].eligible=(d2<=PP12340_REACH*PP12340_REACH || (a->host.cast_guid_spoof && d2<=PP12340_SPOOF_TOTAL_REACH*PP12340_SPOOF_TOTAL_REACH)) ? 1u : 2u;
                    out[count].forward=(moving && dx*step_x+dy*step_y>0.04f) ? 1u : 0u;
                    ++count;
                } else {
                    for(n=1u;n<count;++n)
                        if(out[n].distance_sq>out[far].distance_sq) far=n;
                    if(d2<out[far].distance_sq) {
                        out[far].guid=guid;
                        out[far].distance_sq=d2;
                        out[far].eligible=(d2<=PP12340_REACH*PP12340_REACH || (a->host.cast_guid_spoof && d2<=PP12340_SPOOF_TOTAL_REACH*PP12340_SPOOF_TOTAL_REACH)) ? 1u : 2u;
                        out[far].forward=(moving && dx*step_x+dy*step_y>0.04f) ? 1u : 0u;
                    }
                }
            }
        }
        if (!read32(a,(uintptr_t)obj+PP_OBJ_NEXT,&next) || next==obj) break;
        obj=next;
    }
    a->last_scan_candidates=(uint32_t)count;
    if(a->host.clock_ms)
        a->last_scan_duration_ms=(uint32_t)(a->host.clock_ms(a->host.ctx)-scan_started);
    return count;
}
static int pp_can_cast(void *ctx) {
    Pp12340Adapter *a=(Pp12340Adapter *)ctx;
    return a->host.spell_usable(a->host.ctx,PP12340_SPELL_ID)==1;
}
static int pp_cast(void *ctx,PpGuid guid,uint32_t attempt_id) {
    Pp12340Adapter *a=(Pp12340Adapter *)ctx;
    uint32_t manager=0u,obj=0u,next=0u,player_obj=0u,target_obj=0u;
    uint32_t type=0u,desc=0u,health=0u;
    PpGuid player_guid;
    float me[3],target[3],dx,dy,dz,d2;
    unsigned i;
    if ((guid.lo|guid.hi)==0u || !attempt_id || !mgr(a,&manager) ||
        !read32(a,(uintptr_t)manager+PP_MGR_LOCAL_GUID,&player_guid.lo) ||
        !read32(a,(uintptr_t)manager+PP_MGR_LOCAL_GUID+4u,&player_guid.hi) ||
        (player_guid.lo|player_guid.hi)==0u || same(player_guid,guid) ||
        !locate_player(a,manager,player_guid,&player_obj) ||
        !read32(a,(uintptr_t)manager+PP_MGR_FIRST,&obj)) return 0;
    /* Resolve the target's GUID afresh; re-read both positions just before
     * casting. Never act on a stale range sample from the earlier scan. */
    for(i=0u;i<PP_SCAN_LIMIT && ptr_ok(obj);++i) {
        PpGuid current;
        if (!guid_at(a,obj,&current)) return 0;
        if (same(current,guid)) {target_obj=obj;break;}
        if (!read32(a,(uintptr_t)obj+PP_OBJ_NEXT,&next) || next==obj)
            break;
        obj=next;
    }
    if (!player_obj || !target_obj ||
        !read32(a,(uintptr_t)target_obj+PP_OBJ_TYPE,&type) ||
        type!=PP12340_UNIT_TYPE ||
        !read32(a,(uintptr_t)target_obj+PP_OBJ_DESCRIPTOR,&desc) ||
        !ptr_ok(desc) ||
        !read32(a,(uintptr_t)desc+4u*PP_UNIT_HEALTH_FIELD,&health) ||
        health==0u ||
        a->host.eligible_npc(a->host.ctx,target_obj,guid)!=1 ||
        a->host.position(a->host.ctx,player_obj,me)!=1 ||
        a->host.position(a->host.ctx,target_obj,target)!=1) return 0;
    /* Movement can invalidate the earlier scan. Reject nonfinite range
     * as well as targets that have left the validated cast radius. */
    dx=target[0]-me[0];dy=target[1]-me[1];dz=target[2]-me[2];
    d2=dx*dx+dy*dy+dz*dz;
    if(!(d2>=0.0f && d2<FLT_MAX))return 0;
    if(d2>PP12340_SPOOF_TOTAL_REACH*PP12340_SPOOF_TOTAL_REACH)
        return PP_CAST_LOCAL_RANGE; /* real-position hard cap */
    if(pp_can_cast(a)!=1)return 0;
    if(d2>PP12340_REACH*PP12340_REACH){
        if(!a->host.cast_guid_spoof)return PP_CAST_LOCAL_RANGE;
        return a->host.cast_guid_spoof(a->host.ctx,PP12340_CAST_GUID_VA,
            PP12340_SPELL_ID,guid,attempt_id,player_guid,me,target)==1;
    }
    return a->host.cast_guid(a->host.ctx,PP12340_CAST_GUID_VA,
                              PP12340_SPELL_ID,guid,attempt_id)==1;
}
static PpResult pp_result(void *ctx,PpGuid guid,uint32_t attempt_id) {
    Pp12340Adapter *a=(Pp12340Adapter *)ctx;
    PpResult result=a->host.cast_result(a->host.ctx,guid,attempt_id);
    if (result==PP_RESULT_UI_RANGE_HINT) {
        /* UI_ERROR_MESSAGE has no GUID. A fresh native scan must also see
         * this exact in-flight GUID outside this candidate's cast radius.
         * A missing GUID, stale sample or in-range target is inconclusive. */
        size_t i;
        for(i=0u;i<a->engine.queue_count;++i)
            if(same(a->engine.queue[i].guid,guid) &&
               a->engine.queue[i].eligible==2u) {
                /* The observer is not a cast-result owner until this
                 * independent geometry gate passes. Release its exact
                 * nonce before attempting another NPC in this pulse. */
                if(a->host.end_attempt)
                    a->host.end_attempt(a->host.ctx,guid,attempt_id);
                return PP_RESULT_UI_RANGE_HINT;
            }
        return PP_RESULT_PENDING;
    }
    if (result<PP_RESULT_PENDING || result>PP_RESULT_CAST_REJECTED)
        return PP_RESULT_PENDING;
    return result;
}
static void pp_end_attempt(void *ctx,PpGuid guid,uint32_t attempt_id) {
    Pp12340Adapter *a=(Pp12340Adapter *)ctx;
    if(a->host.end_attempt)
        a->host.end_attempt(a->host.ctx,guid,attempt_id);
}
static void pp_event(void *ctx,PpEvent event,PpGuid guid,uint32_t attempt_id) {
    Pp12340Adapter *a=(Pp12340Adapter *)ctx;
    if (a->host.event) a->host.event(a->host.ctx,event,guid,attempt_id);
}
int pp12340_bind(Pp12340Adapter *a,const Pp12340Host *host) {
    PpAdapter api;
    uint32_t thread;
    uint64_t world;
    if (!a || !host || !host->verify_exe_sha256 ||
        !host->verify_cast_abi || !host->thread_id ||
        !host->read_u32 || !host->position || !host->eligible_npc ||
        !host->spell_usable || !host->cast_guid ||
        !host->cast_result || !host->world_token) return 0;
    thread=host->thread_id(host->ctx);
    world=host->world_token(host->ctx);
    if (!thread || !world ||
        host->verify_exe_sha256(host->ctx,PP12340_CLIENT_SHA256)!=1 ||
        host->verify_cast_abi(host->ctx,PP12340_CAST_GUID_VA,
                             PP12340_POSITION_VA)!=1) return 0;
    memset(a,0,sizeof(*a));
    a->host=*host;
    a->owner_thread=thread;
    a->current_world=world;
    memset(&api,0,sizeof(api));
    api.ctx=a;
    api.scan=pp_scan;
    api.can_cast=pp_can_cast;
    api.cast_on_guid=pp_cast;
    api.result=pp_result;
    api.end_attempt=pp_end_attempt;
    api.event=pp_event;
    if (!pp_init(&a->engine,api)) return 0;
    a->engine.burst_enabled=1u; /* packet-only macro-like nonblocking send */
    a->bound=1u;
    return 1;
}
void pp12340_enable(Pp12340Adapter *a,int enable) {
    if (a && a->bound && a->host.thread_id(a->host.ctx)==a->owner_thread)
        pp_enable(&a->engine,enable);
}
void pp12340_reset(Pp12340Adapter *a) {
    if (a && a->bound && a->host.thread_id(a->host.ctx)==a->owner_thread) {
        a->cached_manager=0u;a->cached_player_obj=0u;
        a->player_sample_valid=0u;
        pp_reset(&a->engine);
    }
}
static void pp_status(Pp12340Adapter *a,PpEvent status) {
    PpGuid none={0u,0u};
    if (a->host.event) a->host.event(a->host.ctx,status,none,0u);
}
int pp12340_command(Pp12340Adapter *a,const char *arguments) {
    const char *end;
    size_t length;
    if (!a || !a->bound || !arguments ||
        a->host.thread_id(a->host.ctx)!=a->owner_thread) return 0;
    while (*arguments==' ' || *arguments=='\t') ++arguments;
    end=arguments+strlen(arguments);
    while (end>arguments && (end[-1]==' ' || end[-1]=='\t')) --end;
    length=(size_t)(end-arguments);
    if (length==2u && !memcmp(arguments,"on",2u)) {
        pp_enable(&a->engine,1);
        pp_status(a,PP_EVENT_ENABLED);return 1;
    }
    if (length==3u && !memcmp(arguments,"off",3u)) {
        pp_enable(&a->engine,0);
        pp_status(a,PP_EVENT_DISABLED);return 1;
    }
    if (length==5u && !memcmp(arguments,"reset",5u)) {
        pp_reset(&a->engine);
        pp_status(a,PP_EVENT_RESET);return 1;
    }
    return 0; /* unknown/empty commands are silent and do not change state */
}
void pp12340_tick(Pp12340Adapter *a,uint32_t now_ms) {
    uint64_t world;
    if (!a || !a->bound ||
        a->host.thread_id(a->host.ctx)!=a->owner_thread) return;
    world=a->host.world_token(a->host.ctx);
    if (!world) {
        /* Loading screens are transient; pause and drop any ambiguous
         * in-flight attempt, but preserve the user's enabled setting. */
        if (a->current_world) {
            pp_reset(&a->engine);
            pp_status(a,PP_EVENT_WORLD_PAUSED);
        }
        a->current_world=0u;
        a->cached_manager=0u;a->cached_player_obj=0u;
        a->player_sample_valid=0u;
        return;
    }
    if (world!=a->current_world) {
        pp_reset(&a->engine);
        a->current_world=world;
        a->cached_manager=0u;a->cached_player_obj=0u;
        a->player_sample_valid=0u;
        pp_status(a,PP_EVENT_WORLD_RESET);
    }
    pp_tick(&a->engine,now_ms);
}
