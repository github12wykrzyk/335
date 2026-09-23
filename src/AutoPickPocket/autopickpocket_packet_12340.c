#include "autopickpocket_packet_12340.h"
static void le32(uint8_t *p,uint32_t v){
    p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);
    p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);
}
size_t pp335_build_cast_packet(uint8_t *dst,size_t cap,
                             uint8_t cast_count,uint32_t guid_lo,uint32_t guid_hi){
    uint32_t parts[2],j;size_t n=14u;uint8_t mask=0u;unsigned i;
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
