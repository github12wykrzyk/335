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
static size_t pp_scan(void *ctx,PpTarget *out,size_t cap) {
    Pp12340Adapter *a=(Pp12340Adapter *)ctx;
    uint32_t manager=0u,obj=0u,player_obj=0u,next=0u,desc=0u;
    PpGuid player_guid, guid;
    float me[3],pos[3];
    size_t count=0u;
    unsigned i;
    if (!out || !cap || !mgr(a,&manager) ||
        !read32(a,(uintptr_t)manager+PP_MGR_LOCAL_GUID,&player_guid.lo) ||
        !read32(a,(uintptr_t)manager+PP_MGR_LOCAL_GUID+4u,&player_guid.hi) ||
        !read32(a,(uintptr_t)manager+PP_MGR_FIRST,&obj)) return 0u;
    /* First pass locates local player, then second pass scans NPCs. The
     * manager's list can change; every address read must be host-guarded.
     */
    for(i=0u;i<PP_SCAN_LIMIT && ptr_ok(obj);++i) {
        if (!guid_at(a,obj,&guid)) return 0u;
        if (same(player_guid,guid)) { player_obj=obj;break; }
        if (!read32(a,(uintptr_t)obj+PP_OBJ_NEXT,&next) || next==obj) return 0u;
        obj=next;
    }
    if (!player_obj || a->host.position(a->host.ctx,player_obj,me)!=1 ||
        !read32(a,(uintptr_t)manager+PP_MGR_FIRST,&obj)) return 0u;
    for(i=0u;i<PP_SCAN_LIMIT && ptr_ok(obj);++i) {
        uint32_t type=0u,health=0u;
        if (!guid_at(a,obj,&guid)) break;
        if (!same(player_guid,guid) &&
            read32(a,(uintptr_t)obj+PP_OBJ_TYPE,&type) &&
            type==PP12340_UNIT_TYPE &&
            read32(a,(uintptr_t)obj+PP_OBJ_DESCRIPTOR,&desc) &&
            ptr_ok(desc) &&
            read32(a,(uintptr_t)desc+4u*PP_UNIT_HEALTH_FIELD,&health) &&
            health>0u &&
            a->host.eligible_npc(a->host.ctx,obj,guid)==1 &&
            a->host.position(a->host.ctx,obj,pos)==1) {
            float dx=pos[0]-me[0],dy=pos[1]-me[1],dz=pos[2]-me[2];
            float d2=dx*dx+dy*dy+dz*dz;
            if (d2>=0.0f && d2<=PP12340_REACH*PP12340_REACH &&
                d2<FLT_MAX) {
                size_t far=0u,n;
                if (count<cap) {
                    out[count].guid=guid;
                    out[count].distance_sq=d2;
                    out[count].eligible=1u;
                    ++count;
                } else {
                    for(n=1u;n<count;++n)
                        if(out[n].distance_sq>out[far].distance_sq) far=n;
                    if(d2<out[far].distance_sq) {
                        out[far].guid=guid;
                        out[far].distance_sq=d2;
                        out[far].eligible=1u;
                    }
                }
            }
        }
        if (!read32(a,(uintptr_t)obj+PP_OBJ_NEXT,&next) || next==obj) break;
        obj=next;
    }
    return count;
}
static int pp_can_cast(void *ctx) {
    Pp12340Adapter *a=(Pp12340Adapter *)ctx;
    return a->host.spell_usable(a->host.ctx,PP12340_SPELL_ID)==1;
}
static int pp_cast(void *ctx,PpGuid guid) {
    Pp12340Adapter *a=(Pp12340Adapter *)ctx;
    uint32_t manager=0u,obj=0u,next=0u;
    unsigned i;
    if ((guid.lo|guid.hi)==0u || !mgr(a,&manager) ||
        !read32(a,(uintptr_t)manager+PP_MGR_FIRST,&obj)) return 0;
    /* GUID may have despawned between scan and cast: require it to still be
     * a valid, living, eligible NPC. Never fall back to player's target.
     */
    for(i=0u;i<PP_SCAN_LIMIT && ptr_ok(obj);++i) {
        PpGuid current;
        uint32_t type=0u,desc=0u,health=0u;
        if (!guid_at(a,obj,&current)) return 0;
        if (same(current,guid)) {
            if (!read32(a,(uintptr_t)obj+PP_OBJ_TYPE,&type) ||
                type!=PP12340_UNIT_TYPE ||
                !read32(a,(uintptr_t)obj+PP_OBJ_DESCRIPTOR,&desc) ||
                !ptr_ok(desc) ||
                !read32(a,(uintptr_t)desc+4u*PP_UNIT_HEALTH_FIELD,&health) ||
                health==0u ||
                a->host.eligible_npc(a->host.ctx,obj,guid)!=1 ||
                pp_can_cast(a)!=1) return 0;
            return a->host.cast_guid(a->host.ctx,PP12340_CAST_GUID_VA,
                                      PP12340_SPELL_ID,guid)==1;
        }
        if (!read32(a,(uintptr_t)obj+PP_OBJ_NEXT,&next) || next==obj) break;
        obj=next;
    }
    return 0;
}
static PpResult pp_result(void *ctx,PpGuid guid) {
    Pp12340Adapter *a=(Pp12340Adapter *)ctx;
    PpResult result=a->host.cast_result(a->host.ctx,guid);
    if (result<PP_RESULT_PENDING || result>PP_RESULT_PERMANENT)
        return PP_RESULT_PENDING;
    return result;
}
static void pp_event(void *ctx,PpEvent event,PpGuid guid) {
    Pp12340Adapter *a=(Pp12340Adapter *)ctx;
    if (a->host.event) a->host.event(a->host.ctx,event,guid);
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
    api.event=pp_event;
    if (!pp_init(&a->engine,api)) return 0;
    a->bound=1u;
    return 1;
}
void pp12340_enable(Pp12340Adapter *a,int enable) {
    if (a && a->bound && a->host.thread_id(a->host.ctx)==a->owner_thread)
        pp_enable(&a->engine,enable);
}
void pp12340_reset(Pp12340Adapter *a) {
    if (a && a->bound && a->host.thread_id(a->host.ctx)==a->owner_thread)
        pp_reset(&a->engine);
}
void pp12340_tick(Pp12340Adapter *a,uint32_t now_ms) {
    uint64_t world;
    if (!a || !a->bound ||
        a->host.thread_id(a->host.ctx)!=a->owner_thread) return;
    world=a->host.world_token(a->host.ctx);
    if (!world) {
        /* Loading screens are transient; pause and drop any ambiguous
         * in-flight attempt, but preserve the user's enabled setting. */
        if (a->current_world) pp_reset(&a->engine);
        a->current_world=0u;
        return;
    }
    if (world!=a->current_world) {
        pp_reset(&a->engine);
        a->current_world=world;
    }
    pp_tick(&a->engine,now_ms);
}
