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
    puts("PP335 packet codec: PASS");
    return 0;
}
