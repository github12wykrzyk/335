/* Portable state-machine regression tests: this is NOT a game/client test. */
#include "../src/AutoLoot/autoloot_core.h"
#include <assert.h>
#include <stdio.h>

typedef struct {
    AlCorpse corpses[2];
    AlUiState ui;
    unsigned scanned, interacted, collected;
    int permitted;
    AlGuid last;
} Mock;
static size_t scan(void *v, AlCorpse *out, size_t cap) {
    Mock *m = (Mock *)v;
    size_t i;
    ++m->scanned;
    for (i = 0; i < 2 && i < cap; ++i) out[i] = m->corpses[i];
    return 2;
}
static int interact(void *v, AlGuid guid) {
    Mock *m = (Mock *)v;
    ++m->interacted;
    m->last = guid;
    return 1;
}
static AlUiState window(void *v, AlGuid owner) {
    Mock *m = (Mock *)v;
    assert(owner.lo == m->last.lo && owner.hi == m->last.hi);
    return m->ui;
}
static int loot_all(void *v) {
    Mock *m = (Mock *)v;
    ++m->collected;
    return 1;
}
static int allowed(void *v) { return ((Mock *)v)->permitted; }
int main(void) {
    Mock m = {0};
    AlAdapter cb = {&m, scan, interact, window, loot_all, allowed};
    AlEngine e;
    m.permitted = 1;
    m.corpses[0].guid.lo = 100;
    m.corpses[0].distance_sq = 16.0f;
    m.corpses[0].can_loot = 1;
    m.corpses[1].guid.lo = 200;
    m.corpses[1].distance_sq = 4.0f;
    m.corpses[1].can_loot = 1;
    al_init(&e, cb, 25.0f);
    al_tick(&e, 100);
    assert(m.interacted == 0); /* disabled by default */
    al_enable(&e, 1);
    al_tick(&e, 100);
    assert(m.interacted == 1 && m.last.lo == 200 && e.state == AL_WAIT_OPEN);
    al_tick(&e, 101);
    assert(m.collected == 0);
    m.ui = AL_UI_OPEN;
    al_tick(&e, 102);
    assert(m.collected == 1 && e.state == AL_DRAIN);
    al_tick(&e, 120);
    assert(m.collected == 1); /* retry throttle */
    m.ui = AL_UI_EMPTY;
    al_tick(&e, 121);
    assert(e.drained == 1 && e.state == AL_IDLE);
    al_tick(&e, 200);
    assert(m.last.lo == 100); /* nearest completed GUID excluded */
    m.ui = AL_UI_BAG_FULL;
    al_tick(&e, 201);
    assert(e.deferred == 1 && e.state == AL_IDLE);
    al_tick(&e, 300);
    assert(m.interacted == 2); /* both GUIDs on cooldown */
    al_enable(&e, 0);
    al_tick(&e, 20000);
    assert(m.interacted == 2); /* OFF halts subsequent actions */

    al_init(&e, cb, 0.0f);
    al_enable(&e, 1);
    al_tick(&e, 20001);
    assert(m.interacted == 2); /* no verified range: fail closed */
    al_init(&e, cb, 25.0f);
    m.ui = AL_UI_CLOSED;
    al_enable(&e, 1);
    al_tick(&e, 30000);
    assert(e.state == AL_WAIT_OPEN);
    al_tick(&e, 30601);
    assert(e.state == AL_IDLE && e.deferred == 1); /* open timeout */
    al_tick(&e, 30799);
    assert(m.interacted == 3); /* nearest GUID is still on failure cooldown */
    al_tick(&e, 30801);
    assert(m.interacted == 4 && m.last.lo == 200); /* retry after 200 ms */
    puts("AUTOLOOT_CORE: PASS (portable logic only; no 12340 adapter)");
    return 0;
}
