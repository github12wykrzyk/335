#include "autoloot_12340_adapter.h"
#include <string.h>

/* Research leads for the 12340 client. The verified host owns all memory
 * safety, pointer lifetime, thread, position ABI and loot-window ownership. */
#define MGR_FIRST 0xACu
#define MGR_LOCAL_GUID 0xC0u
#define OBJ_DESCRIPTOR 0x08u
#define OBJ_TYPE 0x14u
#define OBJ_GUID 0x30u
#define OBJ_NEXT 0x3Cu
#define UNIT_HEALTH_FIELD 0x18u
#define UNIT_TYPE 3u
#define SCAN_LIMIT 4096u

/* A callback must first confirm that the active window belongs to this GUID.
 * No synthetic success is generated when the loot window simply disappears. */
static const char LOOT_LUA[] =
    "pcall(function() local n=GetNumLootItems(); "
    "for i=n,1,-1 do LootSlot(i); ConfirmLootSlot(i) end end)";

static int equal_guid(AlGuid a, AlGuid b) {
    return a.lo == b.lo && a.hi == b.hi;
}
static int read32(Al12340Adapter *a, uintptr_t at, uint32_t *value) {
    if (!value || at < 0x10000u || at > UINT32_MAX - 4u) return 0;
    return a->host.read_u32(a->host.context, at, value) == 1;
}
static int guid_at(Al12340Adapter *a, uintptr_t at, AlGuid *guid) {
    return read32(a, at, &guid->lo) && read32(a, at + 4u, &guid->hi);
}
static int object_manager(Al12340Adapter *a, uint32_t *out) {
    uint32_t connection = 0u;
    if (!read32(a, AL12340_CLIENT_CONNECTION_VA, &connection) ||
        connection < 0x10000u ||
        connection > UINT32_MAX - AL12340_MANAGER_OFFSET ||
        !read32(a, (uintptr_t)connection + AL12340_MANAGER_OFFSET, out))
        return 0;
    return *out >= 0x10000u && *out < 0x7FFE0000u;
}
static uint32_t find_object(Al12340Adapter *a, uint32_t mgr, AlGuid guid) {
    uint32_t obj = 0u, next = 0u, i;
    if (!read32(a, (uintptr_t)mgr + MGR_FIRST, &obj)) return 0u;
    for (i = 0u; i < SCAN_LIMIT && obj >= 0x10000u &&
         obj < 0x7FFE0000u; ++i) {
        AlGuid g;
        if (!guid_at(a, (uintptr_t)obj + OBJ_GUID, &g)) break;
        if (equal_guid(g, guid)) return obj;
        if (!read32(a, (uintptr_t)obj + OBJ_NEXT, &next) || next == obj) break;
        obj = next;
    }
    return 0u;
}
static int corpse_eligible(Al12340Adapter *a, uint32_t obj) {
    uint32_t type = 0u, desc = 0u, health = ~0u, flags = 0u;
    if (!read32(a, (uintptr_t)obj + OBJ_TYPE, &type) || type != UNIT_TYPE ||
        !read32(a, (uintptr_t)obj + OBJ_DESCRIPTOR, &desc) ||
        desc < 0x10000u ||
        desc > 0x7FFE0000u - AL12340_LOOTABLE_FIELD * 4u ||
        !read32(a, (uintptr_t)desc + UNIT_HEALTH_FIELD * 4u, &health) ||
        !read32(a, (uintptr_t)desc + AL12340_LOOTABLE_FIELD * 4u, &flags))
        return 0;
    return health == 0u && (flags & AL12340_LOOTABLE_BIT) != 0u;
}
static size_t scan(void *ctx, AlCorpse *out, size_t cap) {
    Al12340Adapter *a = (Al12340Adapter *)ctx;
    AlGuid player_guid;
    uint32_t mgr, player, obj = 0u, next = 0u, i;
    float player_xyz[3], xyz[3];
    size_t count = 0u;
    if (!cap || !object_manager(a, &mgr) ||
        !guid_at(a, (uintptr_t)mgr + MGR_LOCAL_GUID, &player_guid))
        return 0u;
    player = find_object(a, mgr, player_guid);
    if (!player ||
        !a->host.position(a->host.context, player, player_xyz) ||
        !read32(a, (uintptr_t)mgr + MGR_FIRST, &obj))
        return 0u;
    for (i = 0u; i < SCAN_LIMIT && obj >= 0x10000u &&
         obj < 0x7FFE0000u; ++i) {
        AlGuid g;
        if (!guid_at(a, (uintptr_t)obj + OBJ_GUID, &g)) break;
        if (!equal_guid(g, player_guid) && corpse_eligible(a, obj) &&
            a->host.position(a->host.context, obj, xyz)) {
            float dx = xyz[0] - player_xyz[0];
            float dy = xyz[1] - player_xyz[1];
            float dz = xyz[2] - player_xyz[2];
            float d2 = dx * dx + dy * dy + dz * dz;
            if (d2 >= 0.0f && d2 <= a->engine.max_range_sq) {
                size_t n, farthest = 0u;
                if (count < cap) {
                    out[count].guid = g;
                    out[count].distance_sq = d2;
                    out[count].can_loot = 1u;
                    ++count;
                } else {
                    for (n = 1u; n < count; ++n)
                        if (out[n].distance_sq > out[farthest].distance_sq)
                            farthest = n;
                    if (d2 < out[farthest].distance_sq) {
                        out[farthest].guid = g;
                        out[farthest].distance_sq = d2;
                        out[farthest].can_loot = 1u;
                    }
                }
            }
        }
        if (!read32(a, (uintptr_t)obj + OBJ_NEXT, &next) || next == obj)
            break;
        obj = next;
    }
    return count;
}
static int interact(void *ctx, AlGuid guid) {
    Al12340Adapter *a = (Al12340Adapter *)ctx;
    uint32_t mgr, object;
    if (!object_manager(a, &mgr)) return 0;
    object = find_object(a, mgr, guid);
    if (!object || !corpse_eligible(a, object)) return 0;
    return a->host.right_click(a->host.context, AL12340_RIGHT_CLICK_VA,
                               object, 1u) == 1;
}
static AlUiState window_state(void *ctx, AlGuid guid) {
    Al12340Adapter *a = (Al12340Adapter *)ctx;
    return a->host.owned_loot_window(a->host.context, guid);
}
static int loot_all(void *ctx) {
    Al12340Adapter *a = (Al12340Adapter *)ctx;
    return a->host.execute_lua(a->host.context, AL12340_LUA_EXECUTE_VA,
                               LOOT_LUA, "WoW335AutoLoot") == 1;
}
static int can_act(void *ctx) {
    Al12340Adapter *a = (Al12340Adapter *)ctx;
    return a->host.can_act(a->host.context) == 1;
}
int al12340_bind(Al12340Adapter *a, const Al12340Host *h, float range) {
    AlAdapter api;
    uint32_t thread_id;
    if (!a || !h || !h->verify_exe_sha256 || !h->verify_abi ||
        !h->thread_id || !h->read_u32 || !h->position ||
        !h->right_click || !h->owned_loot_window ||
        !h->execute_lua || !h->can_act ||
        !(range > 0.0f && range <= 5.0f)) return 0;
    thread_id = h->thread_id(h->context);
    if (!thread_id ||
        h->verify_exe_sha256(h->context, AL12340_EXACT_EXE_SHA256) != 1 ||
        h->verify_abi(h->context, AL12340_RIGHT_CLICK_VA,
                      AL12340_LUA_EXECUTE_VA) != 1) return 0;
    memset(a, 0, sizeof(*a));
    a->host = *h;
    a->game_thread = thread_id;
    api.ctx = a;
    api.scan = scan;
    api.interact = interact;
    api.window = window_state;
    api.loot_all = loot_all;
    api.can_act = can_act;
    al_init(&a->engine, api, range * range);
    a->bound = 1u;
    return 1;
}
void al12340_enable(Al12340Adapter *a, int enabled) {
    if (a && a->bound &&
        a->host.thread_id(a->host.context) == a->game_thread)
        al_enable(&a->engine, enabled);
}
void al12340_tick(Al12340Adapter *a, uint32_t now) {
    if (a && a->bound &&
        a->host.thread_id(a->host.context) == a->game_thread)
        al_tick(&a->engine, now);
}
