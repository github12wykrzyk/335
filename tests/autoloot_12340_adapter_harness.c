/* Fake-client test of the native adapter, NOT a game integration test. */
#include "../src/AutoLoot/autoloot_12340_adapter.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define CONN 0x10000u
#define MGR 0x20000u
#define PLAYER 0x30000u
#define CORPSE 0x40000u
#define CORPSE_DESC 0x60000u

typedef struct {
    uint32_t thread, verified, abi, right_clicks, lua_calls;
    AlUiState ui;
} Fake;
static uint32_t thread_id(void *c) {return ((Fake *)c)->thread;}
static int verify(void *c, const char *sha) {
    return ((Fake *)c)->verified && strcmp(sha, AL12340_EXACT_EXE_SHA256)==0;
}
static int abi(void *c, uintptr_t right, uintptr_t script) {
    return ((Fake *)c)->abi && right == AL12340_RIGHT_CLICK_VA &&
           script == AL12340_LUA_EXECUTE_VA;
}
static int read32(void *c, uintptr_t a, uint32_t *v) {
    (void)c;
    switch(a) {
    case AL12340_CLIENT_CONNECTION_VA: *v=CONN;return 1;
    case CONN+AL12340_MANAGER_OFFSET: *v=MGR;return 1;
    case MGR+0xACu: *v=PLAYER;return 1;
    case MGR+0xC0u: *v=11u;return 1;
    case MGR+0xC4u: *v=0u;return 1;
    case PLAYER+0x30u: *v=11u;return 1;
    case PLAYER+0x34u: *v=0u;return 1;
    case PLAYER+0x3Cu: *v=CORPSE;return 1;
    case CORPSE+0x30u: *v=22u;return 1;
    case CORPSE+0x34u: *v=0u;return 1;
    case CORPSE+0x3Cu: *v=0u;return 1;
    case CORPSE+0x14u: *v=3u;return 1;
    case CORPSE+0x08u: *v=CORPSE_DESC;return 1;
    case CORPSE_DESC+0x18u*4u: *v=0u;return 1;
    case CORPSE_DESC+0x4Fu*4u: *v=1u;return 1;
    default: return 0;
    }
}
static int position(void *c, uintptr_t obj, float p[3]) {
    (void)c;
    if(obj!=PLAYER && obj!=CORPSE) return 0;
    p[0]=obj==PLAYER?0.0f:2.0f;p[1]=0.0f;p[2]=0.0f;return 1;
}
static int click(void *c, uintptr_t va, uintptr_t obj, unsigned auto_loot) {
    Fake *f=(Fake *)c;
    if(va!=AL12340_RIGHT_CLICK_VA || obj!=CORPSE || auto_loot!=1u) return 0;
    ++f->right_clicks;return 1;
}
static AlUiState window(void *c, AlGuid g) {
    assert(g.lo==22u && g.hi==0u);
    return ((Fake *)c)->ui;
}
static int lua(void *c, uintptr_t va, const char *script, const char *source) {
    Fake *f=(Fake *)c;
    if(va!=AL12340_LUA_EXECUTE_VA || !strstr(script,"LootSlot(i)") ||
       strcmp(source,"WoW335AutoLoot")!=0) return 0;
    ++f->lua_calls;return 1;
}
static int can_act(void *c) {(void)c;return 1;}

int main(void) {
    Fake f={0};
    Al12340Host h={&f,verify,abi,thread_id,read32,position,click,
                   window,lua,can_act};
    Al12340Adapter a={0};
    f.thread=123u;f.verified=1u;
    assert(!al12340_bind(&a,&h,5.0f)); /* ABI not verified */
    f.abi=1u;
    assert(al12340_bind(&a,&h,5.0f));
    al12340_tick(&a,100u);
    assert(f.right_clicks==0u); /* disabled by default */
    al12340_enable(&a,1);
    f.thread=456u;al12340_tick(&a,100u);
    assert(f.right_clicks==0u); /* game-thread restriction */
    f.thread=123u;al12340_tick(&a,100u);
    assert(f.right_clicks==1u && a.engine.active.lo==22u);
    f.ui=AL_UI_OPEN;al12340_tick(&a,101u);
    assert(f.lua_calls==1u);
    f.ui=AL_UI_EMPTY;al12340_tick(&a,102u);
    assert(a.engine.drained==1u);
    al12340_tick(&a,300u);
    assert(f.right_clicks==1u); /* per-GUID cooldown */
    al12340_enable(&a,0);al12340_tick(&a,20000u);
    assert(f.right_clicks==1u);
    puts("AUTOLOOT_12340_ADAPTER: PASS (fake memory and callbacks only)");
    return 0;
}
