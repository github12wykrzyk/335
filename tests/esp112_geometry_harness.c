#include "../src/PlayerESP112Port/esp112_geometry.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    Esp112Viewport view={0,0,120,240,2560,1440};
    const float sx=0.87158f,sy=0.49026f;
    int x,y,left,top;
    /* Pinned-client W2S raw values from uploaded PlayerESP(2).jsonl.
     * Native 335 W2S already calls DDC internally; a second DDC and
     * inverted y wrongly placed labels at the bottom of the screen. */
    assert(esp112_ui_to_client(0.18139f,0.03538f,sx,sy,&view,&x,&y));
    assert(x>=532 && x<=534);
    assert(y>=103 && y<=105);
    assert(esp112_label_rect(x,y,&view,&left,&top));
    assert(left==120+x-ESP112_LABEL_WIDTH/2);
    assert(top==240+y-ESP112_LABEL_TOP_PAD);
    assert(esp112_ui_to_client(0.45379f,0.37938f,sx,sy,&view,&x,&y));
    assert(x>=1331 && x<=1334);
    assert(y>=1113 && y<=1116);
    assert(esp112_ui_to_client(0.f,0.f,sx,sy,&view,&x,&y));
    assert(x==0 && y==0);
    assert(esp112_ui_to_client(sx,sy,sx,sy,&view,&x,&y));
    assert(x==2560 && y==1440);
    assert(!esp112_ui_to_client(-0.01f,0.01f,sx,sy,&view,&x,&y));
    assert(!esp112_ui_to_client(sx+0.01f,0.01f,sx,sy,&view,&x,&y));
    assert(!esp112_ui_to_client(0.f,0.f,0.f,sy,&view,&x,&y));
    puts("ESP112_335_GEOMETRY: PASS native 12340 W2S once top-left UI");
    return 0;
}
