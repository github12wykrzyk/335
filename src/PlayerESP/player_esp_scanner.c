#include "player_esp_scanner.h"
#include <string.h>

#define ESP335_MAX_SCAN 4096u
#define ESP335_LOW_PTR ((uintptr_t)0x10000u)
#define ESP335_HIGH_PTR ((uintptr_t)0x7FFE0000u)

static int valid_pointer(uintptr_t p) {
    return p >= ESP335_LOW_PTR && p < ESP335_HIGH_PTR;
}
static int read32(const Esp335Scanner *s, uintptr_t at, uint32_t *out) {
    if (!out || !valid_pointer(at) || at > UINT32_MAX - 4u) return 0;
    return s->host.read_u32(s->host.context, at, out) == 1;
}
static int read_guid(const Esp335Scanner *s, uintptr_t at, uint64_t *guid) {
    uint32_t lo, hi;
    if (!guid || !read32(s, at, &lo) || !read32(s, at + 4u, &hi)) return 0;
    *guid = ((uint64_t)hi << 32) | lo;
    return 1;
}
int esp335_scanner_bind(Esp335Scanner *s, const Esp335ScannerHost *h) {
    uint32_t thread;
    if (!s || !h || !h->verify_client_sha256 || !h->verify_layout ||
        !h->thread_id || !h->read_u32 || !h->position ||
        !h->player_metadata || !h->world_epoch) return 0;
    thread = h->thread_id(h->context);
    if (!thread || h->verify_client_sha256(h->context, ESP335_EXACT_EXE_SHA256) != 1 ||
        h->verify_layout(h->context, ESP335_CONNECTION_VA,
                         ESP335_MANAGER_OFFSET) != 1) return 0;
    memset(s, 0, sizeof(*s));
    s->host = *h;
    s->game_thread = thread;
    s->bound = 1;
    return 1;
}
void esp335_scanner_unbind(Esp335Scanner *s) {
    if (s) memset(s, 0, sizeof(*s));
}
int esp335_scanner_collect(Esp335Scanner *s) {
    Esp335Core next;
    uint32_t conn, mgr, obj, type, step;
    uint64_t local_guid, epoch;
    uintptr_t prev = 0;
    if (!s || !s->bound) return 0;
    if (s->host.thread_id(s->host.context) != s->game_thread) goto invalid;
    epoch = s->host.world_epoch(s->host.context);
    if (!epoch || !read32(s, ESP335_CONNECTION_VA, &conn) ||
        !valid_pointer(conn) ||
        !read32(s, (uintptr_t)conn + ESP335_MANAGER_OFFSET, &mgr) ||
        !valid_pointer(mgr) ||
        !read_guid(s, (uintptr_t)mgr + ESP335_MGR_LOCAL_GUID, &local_guid) ||
        !local_guid ||
        !read32(s, (uintptr_t)mgr + ESP335_MGR_FIRST, &obj)) goto invalid;
    s->seen_players=s->seen_npcs=s->accepted_players=s->accepted_npcs=0;
    s->position_failures=s->metadata_failures=0;
    esp335_reset(&next);
    if (!esp335_begin(&next, epoch)) goto invalid;
    /* 12340 object-list termination may use a tagged odd sentinel (1),
     * as well as NULL; never dereference the terminal node. */
    for (step = 0; obj && !(obj & 1u); ++step) {
        uint32_t next_obj;
        uint64_t guid;
        if (step >= ESP335_MAX_SCAN ||
            !valid_pointer(obj) || obj == prev ||
            !read_guid(s, (uintptr_t)obj + ESP335_OBJ_GUID, &guid) ||
            !read32(s, (uintptr_t)obj + ESP335_OBJ_TYPE, &type))
            goto invalid;
        if (guid && guid != local_guid && (type == ESP335_OBJ_PLAYER || type == ESP335_OBJ_NPC)) {
            Esp335Player p;
            if (type == ESP335_OBJ_PLAYER) ++s->seen_players;
            else ++s->seen_npcs;
            Esp335Vec3 pos;
            memset(&p, 0, sizeof(p));
            if (s->host.position(s->host.context, obj, &pos) == 1) {
                p.position = pos;
                p.guid = guid;
                p.kind = type;
                /* Metadata callback must classify from the exact-client
                 * runtime, never guess faction from a cached appearance. */
                if (s->host.player_metadata(s->host.context, obj, &p) == 1) {
                    p.guid = guid; p.position = pos; p.kind = type;
                    p.object_address=(uintptr_t)obj;
                    if (!esp335_push(&next, &p) &&
                        next.count < ESP335_MAX_PLAYERS) goto invalid;
                    if (type == ESP335_OBJ_PLAYER) ++s->accepted_players;
                    else ++s->accepted_npcs;
                } else ++s->metadata_failures;
            } else ++s->position_failures;
        }
        prev = obj;
        if (!read32(s, (uintptr_t)obj + ESP335_OBJ_NEXT, &next_obj))
            goto invalid;
        obj = next_obj;
    }
    esp335_end(&next);
    if (s->host.world_epoch(s->host.context) != epoch ||
        s->host.thread_id(s->host.context) != s->game_thread) goto invalid;
    s->snapshot = next; /* Publish only complete and coherent snapshots. */
    return 1;
invalid:
    ++s->scan_failures;
    esp335_reset(&s->snapshot); /* Never show a stale target after read failure. */
    return 0;
}

int esp335_scanner_live_position(Esp335Scanner *s,
                                 const Esp335Player *p,Esp335Vec3 *out) {
    uint64_t actual_guid;
    uint32_t actual_type;
    Esp335Vec3 fresh;
    if (!s || !s->bound || !p || !out) return 0;
    if (!s->snapshot.world_epoch || !p->guid ||
        !valid_pointer(p->object_address) ||
        p->object_address>UINT32_MAX-ESP335_OBJ_GUID-8u ||
        s->host.thread_id(s->host.context)!=s->game_thread ||
        s->host.world_epoch(s->host.context)!=s->snapshot.world_epoch ||
        !read_guid(s,p->object_address+ESP335_OBJ_GUID,&actual_guid) ||
        actual_guid!=p->guid ||
        !read32(s,p->object_address+ESP335_OBJ_TYPE,&actual_type) ||
        actual_type!=p->kind ||
        s->host.position(s->host.context,p->object_address,&fresh)!=1) {
        ++s->live_position_rejected;
        return 0;
    }
    *out=fresh;
    ++s->live_position_ok;
    return 1;
}
