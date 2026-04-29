/* =======================================================================
 * frame_codec.h — shared frame type definitions (CANESP ↔ NETESP UART)
 * Both sides include this header; no logic lives here.
 * ===================================================================== */
#ifndef FRAME_CODEC_H
#define FRAME_CODEC_H

#include <stdint.h>

/* Frame type byte — maps to CAN message ID category */
#define FRAME_TYPE_TELEMETRY    0x01u
#define FRAME_TYPE_FAULT        0x02u
#define FRAME_TYPE_CMD          0x03u

/* Mirror of STM32 can_ctrl.h can_telemetry_t — must stay in sync */
typedef struct __attribute__((packed)) {
    int16_t  temp_c[3];         /* TMP75B × 3, 0.01°C units    */
    uint16_t current_ma[3];     /* INA3221 channels, mA         */
    uint16_t voltage_mv[3];     /* INA3221 bus voltages, mV     */
    uint8_t  fault_flags;       /* latched fault bitmask        */
    uint8_t  reserved;
} frame_telemetry_t;            /* 16 bytes */

typedef struct __attribute__((packed)) {
    uint8_t  fault_flags;
    uint8_t  reserved[3];
} frame_fault_t;                /* 4 bytes */

typedef struct __attribute__((packed)) {
    uint8_t  cmd;               /* command byte from phone      */
    uint8_t  reserved[3];
} frame_cmd_t;                  /* 4 bytes */

#endif /* FRAME_CODEC_H */