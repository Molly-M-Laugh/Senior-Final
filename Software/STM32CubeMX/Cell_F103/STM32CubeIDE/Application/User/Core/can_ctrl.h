#ifndef CAN_CTRL_H
#define CAN_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "MCP251XFD.h"
#include <stdint.h>
#include <stdbool.h>

/* =======================================================================
 * CAN FD bus parameters
 * Arbitration: 500 kbit/s  |  Data phase: 2 Mbit/s
 * Oscillator:  40 MHz (MCP2518FD XTAL)
 * ===================================================================== */
#define CAN_CTRL_ARB_BITRATE_HZ     500000u
#define CAN_CTRL_DATA_BITRATE_HZ    2000000u

/* =======================================================================
 * Node IDs  (11-bit standard CAN IDs)
 *
 *   0x010  — STM32 (Cell_F103)
 *   0x020  — ESP32 BLE gateway
 *   0x030–0x03F — buddy-line peer nodes (reserved, up to 16)
 *   0x7FF  — broadcast (all nodes)
 *
 * To add a node: assign the next free block and document it here.
 * ===================================================================== */
#define CAN_NODE_ID_STM32           0x010u
#define CAN_NODE_ID_ESP32           0x020u
#define CAN_NODE_ID_BUDDY_BASE      0x030u
#define CAN_NODE_ID_BROADCAST       0x7FFu

/* =======================================================================
 * Message ID allocation  (29-bit extended IDs)
 *
 * Format:  [ priority 3b ][ node_src 8b ][ msg_type 8b ][ reserved 10b ]
 *
 * Priority field (bits 28–26):
 *   0 = highest (emergency / fault)
 *   7 = lowest  (bulk / periodic)
 *
 * To add a new message type: assign the next free MSG_TYPE value in the
 * appropriate priority band and add a payload struct below.
 * ===================================================================== */

/* --- Priority 0: Emergency (fault, alert) --- */
#define CAN_MSGID_FAULT_ALERT       0x00001000u  /* STM32 → all           */
#define CAN_MSGID_TEMP_ALERT        0x00001001u  /* STM32 → all           */

/* --- Priority 2: Commands (ESP32 → STM32) --- */
#define CAN_MSGID_CMD_POWER         0x10002000u  /* set rail enable state  */
#define CAN_MSGID_CMD_TEMP_THRESH   0x10002001u  /* update alert threshold */
#define CAN_MSGID_CMD_TEMP_REQ      0x10002002u  /* request immediate read */
#define CAN_MSGID_CMD_ALERT_ACK     0x10002003u  /* acknowledge alert      */

/* --- Priority 6: Periodic telemetry (STM32 → ESP32) --- */
#define CAN_MSGID_SENSOR_DATA       0x30006000u  /* temp + current + volts */
#define CAN_MSGID_POWER_STATE       0x30006001u  /* rail states + faults   */
#define CAN_MSGID_FRAM_LOG          0x30006002u  /* FRAM log entry chunk   */

/* --- Priority 7: ACK / response (bidirectional) --- */
#define CAN_MSGID_ACK               0x38007000u
#define CAN_MSGID_NACK              0x38007001u

/* =======================================================================
 * Protocol version
 * Increment MINOR on backward-compatible additions (new msg types).
 * Increment MAJOR on breaking changes (payload layout changes).
 * ===================================================================== */
#define CAN_PROTO_VERSION_MAJOR     1u
#define CAN_PROTO_VERSION_MINOR     0u

/* =======================================================================
 * Common frame header  (first 4 bytes of every payload)
 *
 * Every message type begins with this header. Receivers must check
 * proto_major before parsing the rest of the payload.
 * Adding new fields: append to the payload struct — never move or
 * resize existing fields, as that breaks older nodes on the bus.
 * ===================================================================== */
typedef struct __attribute__((packed))
{
    uint8_t  proto_major;    /* CAN_PROTO_VERSION_MAJOR                  */
    uint8_t  proto_minor;    /* CAN_PROTO_VERSION_MINOR                  */
    uint8_t  src_node_id;    /* sending node (CAN_NODE_ID_xxx)           */
    uint8_t  seq;            /* rolling sequence 0–255, per-type counter */
} can_frame_header_t;        /* 4 bytes */

/* =======================================================================
 * Payload structures
 * All structs packed. Fixed-point only — no floats on the wire.
 * ===================================================================== */

/* --- SENSOR_DATA (STM32 → ESP32, periodic) --------------------------- */
/* Temperatures: raw TMP75B register (int16_t, LSB = 0.0625°C).
   Divide by 16 for °C.                                                  */
typedef struct __attribute__((packed))
{
    can_frame_header_t hdr;
    int16_t  temp_raw[3];          /* TMP75B sensors 0/1/2  [6 bytes]   */
    int16_t  current_ma[3];        /* INA3221 CH1/2/3 in mA [6 bytes]   */
    uint16_t voltage_mv[3];        /* INA3221 CH1/2/3 in mV [6 bytes]   */
    uint32_t timestamp_ms;
} can_payload_sensor_data_t;       /* 26 bytes */

/* --- POWER_STATE (STM32 → ESP32, periodic) --------------------------- */
typedef struct __attribute__((packed))
{
    can_frame_header_t hdr;
    uint8_t  rails_enabled;        /* bit0=5V, bit1=3V3, bit2=1V8       */
    uint8_t  rails_pg;             /* bit0=5V PG, bit1=3V3 PG           */
    uint8_t  fault_flags;          /* from power_get_fault_flags()       */
    uint8_t  reserved;
    uint32_t timestamp_ms;
} can_payload_power_state_t;       /* 12 bytes */

/* --- FAULT_ALERT (STM32 → all, event-driven) ------------------------- */
typedef struct __attribute__((packed))
{
    can_frame_header_t hdr;
    uint8_t  fault_flags;
    uint8_t  reserved[3];
    uint32_t timestamp_ms;
} can_payload_fault_alert_t;       /* 12 bytes */

/* --- TEMP_ALERT (STM32 → all, event-driven) -------------------------- */
typedef struct __attribute__((packed))
{
    can_frame_header_t hdr;
    uint8_t  sensor_idx;           /* 0, 1, or 2                        */
    uint8_t  alert_type;           /* 0=high limit, 1=low limit         */
    int16_t  temp_raw;             /* reading at alert time             */
    int16_t  threshold_raw;        /* threshold that was breached       */
    uint8_t  reserved[2];
    uint32_t timestamp_ms;
} can_payload_temp_alert_t;        /* 16 bytes */

/* --- FRAM_LOG (STM32 → ESP32, on request or periodic) ---------------- */
#define CAN_FRAM_LOG_CHUNK_BYTES    48u
typedef struct __attribute__((packed))
{
    can_frame_header_t hdr;
    uint16_t chunk_index;
    uint16_t total_chunks;
    uint8_t  data[CAN_FRAM_LOG_CHUNK_BYTES];
} can_payload_fram_log_t;          /* 56 bytes */

/* --- CMD_POWER (ESP32 → STM32) --------------------------------------- */
typedef struct __attribute__((packed))
{
    can_frame_header_t hdr;
    uint8_t  rail_mask;            /* bit0=5V, bit1=3V3, bit2=1V8       */
    uint8_t  enable;               /* 1 = enable, 0 = disable           */
    uint8_t  reserved[2];
} can_payload_cmd_power_t;         /* 8 bytes */

/* --- CMD_TEMP_THRESH (ESP32 → STM32) --------------------------------- */
typedef struct __attribute__((packed))
{
    can_frame_header_t hdr;
    uint8_t  sensor_idx;           /* 0, 1, or 2                        */
    uint8_t  limit_type;           /* 0=T_HIGH, 1=T_LOW                 */
    int16_t  threshold_raw;        /* TMP75B register format (×16 = °C) */
} can_payload_cmd_temp_thresh_t;   /* 8 bytes */

/* --- CMD_TEMP_REQ (ESP32 → STM32) ------------------------------------ */
typedef struct __attribute__((packed))
{
    can_frame_header_t hdr;
    uint8_t  sensor_mask;          /* bit field: which sensors to read  */
    uint8_t  reserved[3];
} can_payload_cmd_temp_req_t;      /* 8 bytes */

/* --- CMD_ALERT_ACK (ESP32 → STM32) ----------------------------------- */
typedef struct __attribute__((packed))
{
    can_frame_header_t hdr;
    uint8_t  sensor_idx;
    uint8_t  reserved[3];
} can_payload_cmd_alert_ack_t;     /* 8 bytes */

/* --- ACK / NACK (bidirectional) -------------------------------------- */
typedef struct __attribute__((packed))
{
    can_frame_header_t hdr;
    uint32_t ack_msg_id;           /* message ID being acknowledged     */
    uint8_t  ack_seq;              /* seq from the acked frame          */
    uint8_t  reason;               /* NACK only: error code, else 0     */
    uint8_t  reserved[2];
} can_payload_ack_t;               /* 12 bytes */

/* =======================================================================
 * Return codes
 * ===================================================================== */
typedef enum
{
    CAN_CTRL_OK           = 0,
    CAN_CTRL_ERR_INIT     = 1,
    CAN_CTRL_ERR_TX       = 2,
    CAN_CTRL_ERR_RX       = 3,
    CAN_CTRL_ERR_FILTER   = 4,
    CAN_CTRL_ERR_BUSY     = 5,
    CAN_CTRL_ERR_PARAM    = 6,
} can_ctrl_err_t;

/* =======================================================================
 * RX callback
 *
 * Registered by the networking / BLE layer. Called from the CAN RX task
 * when a frame is received and decoded. Do not call can_ctrl_send() from
 * within this callback — it is called from can_rx_task context.
 * ===================================================================== */
typedef void (*can_rx_cb_t)(uint32_t msg_id,
                             const uint8_t *payload,
                             uint8_t dlc);

/* =======================================================================
 * Public API — shared between STM32 and ESP32 targets
 * ===================================================================== */

can_ctrl_err_t can_ctrl_init(void);

can_ctrl_err_t can_ctrl_send(uint32_t msg_id,
                              const uint8_t *payload,
                              uint8_t payload_len);

void can_ctrl_register_rx_callback(can_rx_cb_t cb);

uint8_t can_ctrl_next_seq(uint8_t slot);

/* --- STM32 TX helpers (publish telemetry outbound) ------------------- */
can_ctrl_err_t can_ctrl_send_sensor_data(const can_payload_sensor_data_t *p);
can_ctrl_err_t can_ctrl_send_power_state(const can_payload_power_state_t *p);
can_ctrl_err_t can_ctrl_send_fault_alert(const can_payload_fault_alert_t *p);
can_ctrl_err_t can_ctrl_send_temp_alert (const can_payload_temp_alert_t  *p);
can_ctrl_err_t can_ctrl_send_fram_chunk (const can_payload_fram_log_t    *p);

/* --- ESP32 TX helpers (send commands to STM32) ----------------------- */
can_ctrl_err_t can_ctrl_send_cmd_power      (const can_payload_cmd_power_t       *p);
can_ctrl_err_t can_ctrl_send_cmd_temp_thresh(const can_payload_cmd_temp_thresh_t *p);
can_ctrl_err_t can_ctrl_send_cmd_temp_req   (const can_payload_cmd_temp_req_t    *p);
can_ctrl_err_t can_ctrl_send_cmd_alert_ack  (const can_payload_cmd_alert_ack_t   *p);

/* --- Bidirectional --------------------------------------------------- */
can_ctrl_err_t can_ctrl_send_ack (uint32_t acked_msg_id,  uint8_t acked_seq);
can_ctrl_err_t can_ctrl_send_nack(uint32_t nacked_msg_id, uint8_t nacked_seq,
                                   uint8_t reason);

/* STM32 only — called from HAL_GPIO_EXTI_Callback                      */
#ifndef ESP_PLATFORM
void can_ctrl_irq_handler(uint16_t gpio_pin);
#endif

#ifdef __cplusplus
}
#endif

#endif /* CAN_CTRL_H */
