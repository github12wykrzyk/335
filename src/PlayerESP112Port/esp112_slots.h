/* Stable per-NPC window ownership, independent of distance-sort order. */
#ifndef ESP112_SLOTS_H
#define ESP112_SLOTS_H
#include <stdint.h>
#define ESP112_SLOT_COUNT 32u
typedef struct {
    uint64_t guid[ESP112_SLOT_COUNT];
    uint32_t last_seen[ESP112_SLOT_COUNT];
    uint32_t frame;
} Esp112Slots;
void esp112_slots_reset(Esp112Slots *slots);
void esp112_slots_next_frame(Esp112Slots *slots);
/* Returns reserved slot [0..31], or -1 if no safe slot is available. */
int esp112_slots_reserve(Esp112Slots *slots,uint64_t guid,uint32_t frame_mask);
#endif
