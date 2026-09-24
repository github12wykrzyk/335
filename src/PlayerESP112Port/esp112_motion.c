#include "esp112_motion.h"
#include <math.h>
#include <string.h>
#include <limits.h>
void esp112_motion_reset(Esp112Motion *m) {
    if (m) memset(m,0,sizeof(*m));
}
int esp112_motion_step(Esp112Motion *m,uint64_t guid,
                       float tx,float ty,uint32_t now_ms,
                       int *out_x,int *out_y) {
    uint32_t elapsed;
    float dx,dy,dist,speed,tau,alpha;
    if (!m || !guid || !out_x || !out_y || !isfinite(tx) || !isfinite(ty) ||
        tx<-(float)INT_MAX/2.f || tx>(float)INT_MAX/2.f ||
        ty<-(float)INT_MAX/2.f || ty>(float)INT_MAX/2.f) return 0;
    elapsed=now_ms-m->timestamp_ms; /* unsigned counter wrap is intentional */
    if (!m->valid || m->guid!=guid || elapsed>120u || !elapsed) {
        m->x=tx;m->y=ty;m->guid=guid;m->valid=1u;
        m->timestamp_ms=now_ms;
    } else {
        dx=tx-m->x;dy=ty-m->y;
        dist=sqrtf(dx*dx+dy*dy);
        if (!isfinite(dist) || dist>300.f) {
            /* World/viewport jump: never drag a stale marker across map. */
            m->x=tx;m->y=ty;
        } else {
            speed=dist*1000.f/(float)elapsed;
            /* At rest suppress subpixel jitter, while fast pans use nearly
             * instantaneous updates to avoid visible camera-following lag.
             * alpha depends on actual elapsed time, not assumed 60 FPS. */
            tau=18.f/(1.f+speed/400.f);
            if (tau<1.5f) tau=1.5f;
            alpha=(float)elapsed/((float)elapsed+tau);
            m->x+=dx*alpha;
            m->y+=dy*alpha;
        }
        m->timestamp_ms=now_ms;
    }
    *out_x=(int)(m->x+(m->x>=0.f?0.5f:-0.5f));
    *out_y=(int)(m->y+(m->y>=0.f?0.5f:-0.5f));
    return 1;
}
