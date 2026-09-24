#include "../src/AutoPickPocket/autopickpocket_packet_12340.h"
#include <stdio.h>
#include <string.h>
int main(void){
    uint8_t b[PP335_CAST_PACKET_CAP],s[PP335_CAST_PACKET_CAP];
    const uint8_t expected[]={
        0x2e,0x01,0x00,0x00, /* CMSG_CAST_SPELL: client DataStore opcode */
        0x12,               /* client cast count */
        0x99,0x03,0x00,0x00, /* spell 921 */
        0x00,               /* cast flags */
        0x02,0x00,0x00,0x00, /* unit target mask: uint32 in 3.3.5 */
        0x81,0x01,0x80      /* GUID mask + nonzero bytes */
    };
    memset(b,0xaa,sizeof(b));memset(s,0xaa,sizeof(s));
    if(pp335_build_cast_packet(b,sizeof(b),0x12u,1u,0x80000000u)!=sizeof(expected)
       ||memcmp(b,expected,sizeof(expected))!=0)return 1;
    if(pp335_build_cast_packet(s,sizeof(expected)-1u,0x12u,1u,0x80000000u)!=0u
       ||memcmp(s,b,sizeof(expected))==0)return 2;
    if(pp335_build_cast_packet(b,sizeof(b),0,0u,0u)!=0u)return 3;
    if(pp335_build_cast_packet(NULL,sizeof(b),0,1u,0u)!=0u)return 4;
    if(pp335_build_cast_packet(b,0u,0,1u,0u)!=0u)return 5;
    if(pp335_build_cast_packet(b,sizeof(b),0,0xffffffffu,0xffffffffu)!=23u)return 6;
    {
        uint8_t heartbeat[PP335_MOVE_PACKET_CAP];
        const float pos[3]={1.0f,2.0f,3.0f};
        const uint8_t header[]={0xee,0,0,0,1,1,0,0,0,0,0,0,0x44,0x33,0x22,0x11};
        if(pp335_build_heartbeat(heartbeat,sizeof(heartbeat),1u,0u,0x11223344u,pos,0.0f)!=36u ||
           memcmp(heartbeat,header,sizeof(header)))return 7;
        if(pp335_build_heartbeat(heartbeat,35u,1u,0u,0u,pos,0.0f)!=0u)return 8;
        if(pp335_build_heartbeat(heartbeat,sizeof(heartbeat),0u,0u,0u,pos,0.0f)!=0u)return 9;
        if(pp335_build_heartbeat(heartbeat,sizeof(heartbeat),1u,0u,0u,pos,7.0f)!=0u)return 10;
        if(pp335_build_heartbeat(heartbeat,sizeof(heartbeat),0xffffffffu,
                  0xffffffffu,0u,pos,0.0f)!=PP335_MOVE_PACKET_CAP)return 11;
    }
    puts("PP335 packet codec: PASS");
    return 0;
}
