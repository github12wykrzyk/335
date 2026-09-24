#include "../src/PlayerESP112Port/esp112_motion.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    Esp112Motion m={0};
    int x,y;
    unsigned i;
    assert(esp112_motion_step(&m,11u,100.f,100.f,1000u,&x,&y));
    assert(x==100 && y==100);
    /* Alternating one-pixel target must produce less than one-pixel jitter. */
    for (i=0u;i<8u;++i) {
        assert(esp112_motion_step(&m,11u,(i&1u)?99.f:101.f,100.f,
                                  1016u+i*16u,&x,&y));
        assert(x>=99 && x<=101);
    }
    /* A rapid camera pan must remain close to current target. */
    assert(esp112_motion_step(&m,11u,160.f,120.f,1144u,&x,&y));
    assert(x>=149 && x<=160);
    /* A discontinuity must snap rather than interpolate through the screen. */
    assert(esp112_motion_step(&m,11u,1200.f,300.f,1160u,&x,&y));
    assert(x==1200 && y==300);
    /* Reusing a window for another GUID must never inherit old position. */
    assert(esp112_motion_step(&m,22u,10.f,20.f,1176u,&x,&y));
    assert(x==10 && y==20);
    /* Delayed game messages must not cause a long camera-following tail. */
    assert(esp112_motion_step(&m,22u,50.f,50.f,1376u,&x,&y));
    assert(x==50 && y==50);
    /* GetTickCount wrap must preserve short interval. */
    esp112_motion_reset(&m);
    assert(esp112_motion_step(&m,11u,0.f,0.f,0xfffffff0u,&x,&y));
    assert(esp112_motion_step(&m,11u,8.f,8.f,16u,&x,&y));
    assert(x>0 && x<=8);
    puts("ESP112_ADAPTIVE_MOTION: PASS");
    return 0;
}
