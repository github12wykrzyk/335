#include "autopickpocket_packet_12340.h"
#include <float.h>
#include <string.h>
static void le32(uint8_t *p,uint32_t v){
    p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);
    p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);
}
size_t pp335_build_cast_packet(uint8_t *dst,size_t cap,
                             uint8_t cast_count,uint32_t guid_lo,uint32_t guid_hi){
    uint32_t parts[2],j;size_t n=15u;uint8_t mask=0u;unsigned i;
    if(!dst || cap<15u || (guid_lo|guid_hi)==0u)return 0u;
    parts[0]=guid_lo;parts[1]=guid_hi;
    for(i=0u;i<8u;++i)
        if(((parts[i/4u]>>((i%4u)*8u))&255u)!=0u){
            mask=(uint8_t)(mask|(uint8_t)(1u<<i));++n;
        }
    if(n>cap)return 0u;
    le32(dst,PP335_CAST_OPCODE);
    dst[4]=cast_count;
    le32(dst+5,PP335_PICK_POCKET_SPELL);
    dst[9]=0u; /* no missile trajectory/movement payload */
    le32(dst+10,PP335_UNIT_TARGET_FLAG);
    dst[14]=mask;
    n=15u;
    for(i=0u;i<8u;++i){
        j=(parts[i/4u]>>((i%4u)*8u))&255u;
        if(j)dst[n++]=(uint8_t)j;
    }
    return n;
}

static int pp335_finite(float x){return x==x && x>-FLT_MAX && x<FLT_MAX;}
size_t pp335_build_heartbeat(uint8_t *dst,size_t cap,uint32_t guid_lo,
    uint32_t guid_hi,uint32_t time_ms,const float xyz[3],float facing){
    uint32_t parts[2],bits;uint8_t mask=0u;size_t n=5u;unsigned i;
    if(!dst || !xyz || !(guid_lo|guid_hi) || !pp335_finite(xyz[0]) ||
       !pp335_finite(xyz[1]) || !pp335_finite(xyz[2]) ||
       !pp335_finite(facing) || facing<0.0f || facing>6.283186f)return 0u;
    parts[0]=guid_lo;parts[1]=guid_hi;
    for(i=0u;i<8u;++i)
        if((parts[i/4u]>>((i%4u)*8u))&255u){
            mask=(uint8_t)(mask|(uint8_t)(1u<<i));++n;
        }
    if(n+4u+2u+4u+16u+4u>cap)return 0u;
    le32(dst,PP335_MOVE_HEARTBEAT);dst[4]=mask;n=5u;
    for(i=0u;i<8u;++i){
        bits=(parts[i/4u]>>((i%4u)*8u))&255u;
        if(bits)dst[n++]=(uint8_t)bits;
    }
    le32(dst+n,0u);n+=4u; /* normal ground mover, no movement flags */
    dst[n++]=0u;dst[n++]=0u; /* movement flags2 (16-bit) */
    le32(dst+n,time_ms);n+=4u;
    for(i=0u;i<4u;++i){
        float x=i<3u ? xyz[i] : facing;
        memcpy(&bits,&x,sizeof(bits));le32(dst+n,bits);n+=4u;
    }
    le32(dst+n,0u);n+=4u; /* fall time */
    return n;
}
