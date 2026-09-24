#ifndef WOW335_PP_PACKET_12340_H
#define WOW335_PP_PACKET_12340_H
#include <stddef.h>
#include <stdint.h>
/* CMSG_CAST_SPELL 3.3.5a: opcode is the 32-bit client DataStore prefix,
 * NOT the 6-byte on-wire WorldSocket header. */
#define PP335_CAST_OPCODE 0x012Eu
#define PP335_PICK_POCKET_SPELL 921u
#define PP335_UNIT_TARGET_FLAG 0x00000002u
#define PP335_CAST_PACKET_CAP 23u
#define PP335_MOVE_HEARTBEAT 0x00EEu
#define PP335_MOVE_PACKET_CAP 43u
/* Serialize a unit-targeted, non-missile spell 921 into an owned buffer.
 * 0 = invalid argument/capacity. Does not transmit, spoof range or
 * manufacture a result; transport and lifecycle are separate. */
size_t pp335_build_cast_packet(uint8_t *dst,size_t cap,
                             uint8_t cast_count,uint32_t guid_lo,uint32_t guid_hi);
/* 3.3.5 stationary ground MSG_MOVE_HEARTBEAT: packed player GUID,
 * uint32 flags, uint16 flags2, uint32 time, XYZO, uint32 fallTime.
 * Caller must exclude transport, swimming, flying and falling. */
size_t pp335_build_heartbeat(uint8_t *dst,size_t cap,uint32_t guid_lo,
    uint32_t guid_hi,uint32_t time_ms,const float xyz[3],float facing);
#endif
