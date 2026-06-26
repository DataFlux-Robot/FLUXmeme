/* Minimal MAVLink v2 framing for the FLUX_PARAM message.
 *
 * Frame layout (MAVLink v2):
 *   STX(0xFD) LEN INCOMPAT COMPAT SEQ SYSID COMPID MSGID(3 LE) PAYLOAD[LEN] CRC(2 LE)
 * CRC = CRC-16/X.25 over (STX..PAYLOAD) + a CRC_EXTRA seed byte.
 *
 * FLUX_PARAM (MSGID=42, CRC_EXTRA=42): char name[16]; float value;  (20 bytes)
 * One frame = one JOURNAL/signal record (name + float value). */
#ifndef FLUX_MAVLINK_H
#define FLUX_MAVLINK_H
#include <stddef.h>
#include <stdint.h>

#define FLUX_MAVLINK_STX 0xFDu
#define FLUX_MAVLINK_MSGID 42u
#define FLUX_MAVLINK_CRC_EXTRA 42u
#define FLUX_PARAM_PAYLOAD_LEN 20u  /* name[16] + value(4) */

/* CRC-16/X.25 accumulate. */
uint16_t flux_mav_crc_accumulate(uint16_t crc, uint8_t b);

/* Encode one frame into `out` (must hold >= 12 + payload_len + 2 bytes).
 * Returns frame length, or 0 on bad args. */
size_t flux_mav_encode(uint8_t sysid, uint8_t compid, uint16_t seq,
                       uint8_t msgid, uint8_t crc_extra,
                       const uint8_t* payload, uint8_t payload_len, uint8_t* out);

/* Decode: at `buf`, if a valid frame starts here, set *plen=total frame bytes,
 * fill outputs, return 1. Else return 0. */
int flux_mav_decode(const uint8_t* buf, size_t buf_len, size_t* plen,
                    uint8_t* sysid, uint8_t* compid, uint8_t* msgid,
                    uint8_t* payload, uint8_t* payload_len);

#endif
