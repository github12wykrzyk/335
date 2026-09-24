#include "../src/PlayerESP112Port/esp112_slots.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    Esp112Slots slots={0};
    uint32_t used=0u;
    int a,b,c;
    esp112_slots_next_frame(&slots);
    a=esp112_slots_reserve(&slots,0xabcdu,used);
    used|=UINT32_C(1)<<a;
    b=esp112_slots_reserve(&slots,0xdef0u,used);
    assert(a==0 && b==1);
    assert(esp112_slots_reserve(&slots,0xabcdu,used)==-1);
    esp112_slots_next_frame(&slots);
    used=0u;
    assert(esp112_slots_reserve(&slots,0xdef0u,used)==b);
    used|=UINT32_C(1)<<b;
    assert(esp112_slots_reserve(&slots,0xabcdu,used)==a);
    used|=UINT32_C(1)<<a;
    c=esp112_slots_reserve(&slots,0x1234u,used);
    assert(c==2);
    esp112_slots_next_frame(&slots);
    used=0u;
    assert(esp112_slots_reserve(&slots,0xabcdu,used)==a);
    used|=UINT32_C(1)<<a;
    assert(esp112_slots_reserve(&slots,0x9999u,used)==b);
    assert(slots.guid[a]==0xabcdu);
    assert(slots.guid[b]==0x9999u);
    esp112_slots_reset(&slots);
    esp112_slots_next_frame(&slots);
    assert(esp112_slots_reserve(&slots,0xabcdu,0u)==0);
    puts("ESP112_STABLE_SLOTS: PASS");
    return 0;
}
