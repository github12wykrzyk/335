#include "../src/PlayerESP/player_esp_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static Esp335Camera camera(void) {
    Esp335Camera c;
    memset(&c, 0, sizeof(c));
    c.view_projection[0] = c.view_projection[5] =
        c.view_projection[10] = c.view_projection[15] = 1.f;
    c.viewport_w = 800.f; c.viewport_h = 600.f;
    return c;
}
int main(void) {
    Esp335Core e;
    Esp335Camera c = camera();
    Esp335Filter filter = {0};
    filter.show_bg_opponents=1; filter.max_distance=50.f;
    Esp335Player p = {0};
    Esp335Label labels[ESP335_MAX_PLAYERS];
    uint64_t guid = 0;
    size_t count;
    float sx, sy, depth;
    esp335_reset(&e);
    assert(esp335_begin(&e, 1));
    p.guid=42; p.faction=ESP335_HORDE; p.relation=1;
    p.bg_team=ESP335_BG_OPPONENT; p.position.z=0.5f;
    p.health=50; p.max_health=100; p.level=80;
    assert(esp335_push(&e, &p));
    assert(esp335_push(&e, &p)); /* deduplicate same GUID */
    assert(e.count == 1);
    assert(esp335_labels(&e, &c, &filter, 1, labels, 256) == 0); /* incomplete frame */
    esp335_end(&e);
    count = esp335_labels(&e, &c, &filter, 1, labels, 256);
    assert(count == 1 && labels[0].guid == 42);
    assert(labels[0].screen_x == 400.f && labels[0].screen_y == 300.f);
    assert(esp335_labels(&e, &c, &filter, 0, labels, 256) == 0);
    assert(esp335_pick_guid(&e, labels, 1, 400.f, 290.f, 100.f, 30.f, &guid));
    assert(guid == 42);
    assert(!esp335_pick_guid(&e, labels, 1, 800.f, 290.f, 100.f, 30.f, &guid));
    assert(esp335_begin(&e, 2)); /* zone/instance transition */
    esp335_end(&e);
    assert(!esp335_pick_guid(&e, labels, 1, 400.f, 290.f, 100.f, 30.f, &guid));
    assert(e.count == 0);
    assert(!esp335_project(&c, (Esp335Vec3){0.f,0.f,-1.f}, &sx,&sy,&depth));
    assert(!esp335_project(&c, (Esp335Vec3){2.f,0.f,0.5f}, &sx,&sy,&depth));
    assert(esp335_project(&c, (Esp335Vec3){0.f,0.f,0.5f}, &sx,&sy,&depth));
    /* The ordinary faction switch works independently of BG team. */
    filter.show_bg_opponents=0; filter.factions=ESP335_HORDE;
    assert(esp335_eligible(&p, &filter, 0));
    filter.factions=0; filter.show_hostile=1;
    assert(!esp335_eligible(&p, &filter, 0));
    p.relation=ESP335_REL_HOSTILE;
    assert(esp335_eligible(&p, &filter, 0));
    p.kind=ESP335_KIND_NPC;
    p.relation=0u;
    filter.show_hostile=0u;filter.show_npc=1u;
    assert(esp335_eligible(&p,&filter,0));
    filter.show_npc=0u;filter.show_npc_hostile=1u;
    assert(!esp335_eligible(&p,&filter,0));
    p.relation=ESP335_REL_HOSTILE;
    assert(esp335_eligible(&p,&filter,0));
    puts("PLAYER_ESP_CORE: PASS");
    return 0;
}
