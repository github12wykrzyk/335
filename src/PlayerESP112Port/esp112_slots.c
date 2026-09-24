#include "esp112_slots.h"
#include <string.h>
void esp112_slots_reset(Esp112Slots *s) {
    if (s) memset(s,0,sizeof(*s));
}
void esp112_slots_next_frame(Esp112Slots *s) {
    if (!s) return;
    ++s->frame;
    if (!s->frame) {
        unsigned i;
        s->frame=1u;
        for (i=0u;i<ESP112_SLOT_COUNT;++i) s->last_seen[i]=0u;
    }
}
int esp112_slots_reserve(Esp112Slots *s,uint64_t guid,uint32_t used) {
    unsigned i,choice=ESP112_SLOT_COUNT;
    uint32_t oldest=UINT32_MAX;
    if (!s || !guid || !s->frame) return -1;
    for (i=0u;i<ESP112_SLOT_COUNT;++i) {
        if (s->guid[i]==guid) {
            if (used & (UINT32_C(1)<<i)) return -1;
            s->last_seen[i]=s->frame;
            return (int)i;
        }
    }
    for (i=0u;i<ESP112_SLOT_COUNT;++i) {
        if (used & (UINT32_C(1)<<i)) continue;
        if (!s->guid[i]) {choice=i;break;}
        if (s->last_seen[i]<oldest) {oldest=s->last_seen[i];choice=i;}
    }
    if (choice==ESP112_SLOT_COUNT) return -1;
    s->guid[choice]=guid;
    s->last_seen[choice]=s->frame;
    return (int)choice;
}
