#include "../src/PlayerESP/player_esp_scanner.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define CONN 0x20000u
#define MGR 0x30000u
#define SELF 0x40000u
#define ENEMY 0x50000u
typedef struct { uint64_t epoch; unsigned valid, healthy; } Fake;
static int verify(void *v, const char *sha) {
    Fake *f=(Fake *)v;
    return f->valid && strcmp(sha, ESP335_EXACT_EXE_SHA256)==0;
}
static int layout(void *v, uintptr_t addr, uintptr_t offset) {
    (void)v; return addr==ESP335_CONNECTION_VA && offset==ESP335_MANAGER_OFFSET;
}
static uint32_t thread(void *v) { (void)v; return 123u; }
static uint64_t epoch(void *v) { return ((Fake*)v)->epoch; }
static int read32(void *v, uintptr_t a, uint32_t *n) {
    Fake *f=(Fake *)v;
    if (!f->healthy) return 0;
    if (a==ESP335_CONNECTION_VA) *n=CONN;
    else if (a==CONN+ESP335_MANAGER_OFFSET) *n=MGR;
    else if (a==MGR+ESP335_MGR_LOCAL_GUID) *n=111u;
    else if (a==MGR+ESP335_MGR_LOCAL_GUID+4u) *n=0;
    else if (a==MGR+ESP335_MGR_FIRST) *n=SELF;
    else if (a==SELF+ESP335_OBJ_GUID) *n=111u;
    else if (a==SELF+ESP335_OBJ_GUID+4u) *n=0;
    else if (a==SELF+ESP335_OBJ_TYPE) *n=ESP335_OBJ_PLAYER;
    else if (a==SELF+ESP335_OBJ_NEXT) *n=ENEMY;
    else if (a==ENEMY+ESP335_OBJ_GUID) *n=222u;
    else if (a==ENEMY+ESP335_OBJ_GUID+4u) *n=0;
    else if (a==ENEMY+ESP335_OBJ_TYPE) *n=ESP335_OBJ_PLAYER;
    else if (a==ENEMY+ESP335_OBJ_NEXT) *n=0;
    else return 0;
    return 1;
}
static int position(void *v, uintptr_t addr, Esp335Vec3 *pos) {
    (void)v;
    if (addr != ENEMY) return 0;
    pos->x=0; pos->y=0; pos->z=0.5f;
    return 1;
}
static int metadata(void *v, uintptr_t addr, Esp335Player *p) {
    (void)v;
    if (addr != ENEMY || p->guid != 222u) return 0;
    p->faction=ESP335_HORDE; p->relation=ESP335_REL_HOSTILE;
    p->health=40; p->max_health=100;
    p->level=80; p->class_id=4;
    return 1;
}
int main(void) {
    Esp335Scanner s;
    Fake f={1u,0u,1u};
    Esp335ScannerHost h={0};
    memset(&s, 0, sizeof(s));
    h.context=&f; h.verify_client_sha256=verify; h.verify_layout=layout;
    h.thread_id=thread; h.read_u32=read32; h.position=position;
    h.player_metadata=metadata; h.world_epoch=epoch;
    assert(!esp335_scanner_bind(&s,&h)); /* SHA gate */
    f.valid=1;
    assert(esp335_scanner_bind(&s,&h));
    assert(esp335_scanner_collect(&s));
    assert(s.snapshot.count==1 && s.snapshot.players[0].guid==222u);
    assert(s.snapshot.world_epoch==1u);
    f.epoch=2u;
    assert(esp335_scanner_collect(&s));
    assert(s.snapshot.world_epoch==2u && s.snapshot.count==1u);
    f.healthy=0;
    assert(!esp335_scanner_collect(&s));
    assert(s.snapshot.count==0 && s.snapshot.world_epoch==0u);
    esp335_scanner_unbind(&s);
    puts("PLAYER_ESP_SCANNER: PASS");
    return 0;
}
