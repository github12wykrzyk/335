#include "../src/PlayerESP112Port/esp112_geometry.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    Esp112Viewport v={0,0,120,240,2560,1440};
    int x,y,left,top;
    assert(esp112_ndc_to_client(.5f,.5f,&v,&x,&y));
    assert(x==1280 && y==720);
    assert(esp112_label_rect(x,y,&v,&left,&top));
    assert(left==120+1280-ESP112_LABEL_WIDTH/2);
    assert(top==240+720-ESP112_LABEL_TOP_PAD);
    assert(esp112_ndc_to_client(0.f,1.f,&v,&x,&y));
    assert(x==0 && y==0);
    assert(esp112_ndc_to_client(1.f,0.f,&v,&x,&y));
    assert(x==2560 && y==1440);
    assert(!esp112_ndc_to_client(-.01f,.5f,&v,&x,&y));
    assert(!esp112_ndc_to_client(.5f,1.01f,&v,&x,&y));
    /* raw W2S 0.0908 is NOT assumed to be already NDC. Real 12340
     * DdcToNdc is the separate, tested native conversion step. */
    puts("ESP112_335_GEOMETRY: PASS");
    return 0;
}
