/* =============================================================================
 * frame_codec.h — UART framing protocol shared by CANESP and NETESP
 *
 * Wire format (COBS-delimited):
 *   [0x00] [LEN_LO] [LEN_HI] [TYPE] [PAYLOAD: N bytes] [CRC16_LO] [CRC16_HI] [0x00]
 *
 * 0x00 is the frame boundary marker. COBS encoding ensures 0x00 never appears
 * inside the payload, so any 0x00 on the wire is unambiguously a frame edge.
 * CRC16-CCITT covers TYPE + PAYLOAD only (not the length or delimiter bytes).
 *
 * Maximum payload: FRAME_MAX_PAYLOAD_LEN bytes.
 * =========================================================================== */
#pragma once //I hate this

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------- */
/* Frame type bytes — map 1:1 to CAN ID categories                           */
/* -------------------------------------------------------------------------- */
#define FRAME_TYPE_TELEMETRY        0x10u   /* CAN 0x100 origin */
#define FRAME_TYPE_FAULT            0x11u   /* CAN 0x101 origin */
#define FRAME_TYPE_CMD_PING         0x20u   /* CAN 0x200        */
#define FRAME_TYPE_CMD_FORCE_LOG    0x22u   /* CAN 0x202        */
#define FRAME_TYPE_CMD_CLEAR_FAULTS 0x23u   /* CAN 0x203        */
#define FRAME_TYPE_CMD_RESET        0x24u   /* CAN 0x204        */

/* -------------------------------------------------------------------------- */
/* Payload structures — packed, matches STM32 can_ctrl.h exactly             */
/* -------------------------------------------------------------------------- */

/* Telemetry payload: 20 bytes */
typedef struct __attribute__((packed)) {
    int16_t  temp_c[3];       /* TMP75B ×3, units: 0.01 °C  */
    uint16_t current_ma[3];   /* INA3221 channels, mA        */
    uint16_t voltage_mv[3];   /* INA3221 bus voltages, mV    */
    uint8_t  fault_flags;     /* latched fault bitmask       */
    uint8_t  reserved;
} frame_telemetry_t;          /* 20 bytes                    */

/* Fault payload: 6 bytes */
typedef struct __attribute__((packed)) {
    uint32_t fault_sources;   /* FAULT_SRC_* bitmask         */
    uint8_t  tier;            /* 1=warning 2=critical 3=emergency */
    uint8_t  rail_states;     /* bit0=5V0 bit1=3V3 bit2=1V8  */
} frame_fault_t;              /* 6 bytes                     */

/* Command payload: 0 bytes (type byte alone is sufficient for all commands) */

/* -------------------------------------------------------------------------- */
/* Encoder / decoder API                                                       */
/* -------------------------------------------------------------------------- */

#define FRAME_MAX_PAYLOAD_LEN   64u
#define FRAME_OVERHEAD          6u    /* 2 delimiters + 2 len + 1 type + 1 crc16 extra = actually 7 */
#define FRAME_MAX_WIRE_LEN      (FRAME_MAX_PAYLOAD_LEN + 8u)

typedef enum {
    FRAME_OK             = 0,
    FRAME_ERR_TOO_LONG   = 1,   /* payload exceeds FRAME_MAX_PAYLOAD_LEN */
    FRAME_ERR_CRC        = 2,   /* CRC mismatch on decode                */
    FRAME_ERR_TRUNCATED  = 3,   /* wire buffer too short                 */
    FRAME_ERR_NO_FRAME   = 4,   /* no complete frame in buffer yet       */
} frame_status_t;

/*
 * frame_encode — encode type + payload into a wire buffer.
 * out_buf must be at least FRAME_MAX_WIRE_LEN bytes.
 * Returns FRAME_OK and sets *out_len on success.
 */
frame_status_t frame_encode(uint8_t type,
                             const void *payload, uint16_t payload_len,
                             uint8_t *out_buf, uint16_t *out_len);

/*
 * frame_decode — attempt to decode one frame from in_buf[0..in_len).
 * On FRAME_OK: *type_out and payload written, *consumed set to bytes used.
 * On FRAME_ERR_NO_FRAME: need more data, *consumed = 0.
 * On FRAME_ERR_CRC / FRAME_ERR_TRUNCATED: bad frame, *consumed = bytes to skip.
 * payload_buf must be at least FRAME_MAX_PAYLOAD_LEN bytes.
 */
frame_status_t frame_decode(const uint8_t *in_buf, uint16_t in_len,
                             uint8_t *type_out,
                             uint8_t *payload_buf, uint16_t *payload_len_out,
                             uint16_t *consumed);
