#include "../src/PlayerESP112Port/esp112_geometry.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    Esp112Viewport view={0,0,120,240,2560,1440};
    const float sx=0.87158f,sy=0.49026f;
    int x,y,left,top;
    /* Pinned-client W2S raw values from uploaded PlayerESP(2).jsonl.
     * Native 335 W2S already calls DDC internally; a second DDC and
     * an additional DDC wrongly placed labels at the bottom of the screen;
     * with a SINGLE DDC, raw output is bottom-left and OS needs Y flip. */
    assert(esp112_ui_to_client(0.18139f,0.03538f,sx,sy,&view,&x,&y));
    assert(x>=532 && x<=534);
    assert(y>=1335 && y<=1337);
    assert(esp112_label_rect(x,y,&view,&left,&top));
    assert(left==120+x-ESP112_LABEL_WIDTH/2);
    assert(top==240+y-ESP112_LABEL_TOP_PAD);
    assert(esp112_ui_to_client(0.45379f,0.37938f,sx,sy,&view,&x,&y));
    assert(x>=1331 && x<=1334);
    assert(y>=324 && y<=327);
    assert(esp112_ui_to_client(0.f,0.f,sx,sy,&view,&x,&y));
    assert(x==0 && y==1440);
    assert(esp112_ui_to_client(sx,sy,sx,sy,&view,&x,&y));
    assert(x==2560 && y==0);
    assert(!esp112_ui_to_client(-0.01f,0.01f,sx,sy,&view,&x,&y));
    assert(!esp112_ui_to_client(sx+0.01f,0.01f,sx,sy,&view,&x,&y));
    assert(!esp112_ui_to_client(0.f,0.f,0.f,sy,&view,&x,&y));
    /* A camera displacement of less than a pixel must reach D3D9 without
     * premature round-to-int; legacy GDI remains exactly as before. */
    {
        float fx=0.f,fy=0.f,fx2=0.f,fy2=0.f;
        const float half_x=sx*(123.25f/2560.f);
        const float half_y=sy*(1.f-456.75f/1440.f);
        assert(esp112_ui_to_client_precise(half_x,half_y,sx,sy,
                                           &view,&fx,&fy));
        assert(fx>123.20f && fx<123.30f);
        assert(fy>456.70f && fy<456.80f);
        assert(esp112_ui_to_client(half_x,half_y,sx,sy,&view,&x,&y));
        assert(x==123 && y==457);
        assert(esp112_ui_to_client_precise(half_x+sx*(0.25f/2560.f),
                                           half_y, sx,sy,&view,&fx2,&fy2));
        assert(fx2>fx && fx2-fx>0.20f && fx2-fx<0.30f);
        assert(fy2>456.70f && fy2<456.80f);
        assert(!esp112_ui_to_client_precise(half_x,half_y,0.f,sy,
                                            &view,&fx2,&fy2));
    }
    /* Paired data from PlayerESP(4).jsonl, same NPC 0D70:
     * old top-left mapping: raised head Y=697, feet Y=335 (WRONG).
     * new Windows mapping must have raised head ABOVE feet. */
    {
        int head_y, feet_y;
        assert(esp112_ui_to_client(0.27460f,0.23727f,sx,sy,
                                   &view,&x,&head_y));
        assert(esp112_ui_to_client(0.29066f,0.11415f,sx,sy,
                                   &view,&x,&feet_y));
        assert(head_y<feet_y);
        assert(head_y>=742 && head_y<=745);
        assert(feet_y>=1103 && feet_y<=1106);
    }
    puts("ESP112_335_GEOMETRY: PASS native 12340 W2S once bottom-left UI to Windows top-left");
    return 0;
}
