/* =======================================================================
 * can_ctrl.h — CAN FD logic layer for Cell_F103
 * Sits above mcp_port (HAL) and below application logic.
 * ===================================================================== */
#ifndef CAN_CTRL_H
#define CAN_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "MCP251XFD.h"
#include <stdint.h>
#include <stdbool.h>

/* -----------------------------------------------------------------------
 * OSC frequency — DSC6111B output fed into MCP2518FD CLKIN.
 * *** VERIFY AGAINST BOM BEFORE BENCH TEST ***
 * --------------------------------------------------------------------- */
#define CAN_OSC_FREQ_HZ         40000000u

/* CAN bus bitrates */
#define CAN_NOMINAL_BITRATE     500000u     /* 500 kbps arbitration */
#define CAN_DATA_BITRATE        2000000u    /* 2 Mbps data phase    */

/* FIFO assignments — TXQ + FIFO2 */
#define CAN_TX_FIFO             MCP251XFD_TXQ
#define CAN_RX_FIFO             MCP251XFD_FIFO2

/* Filter assignment */
#define CAN_FILTER_0            MCP251XFD_FILTER0

/* CAN message IDs — extend as needed for your node ID scheme */
#define CAN_MSGID_TELEMETRY     0x100u
#define CAN_MSGID_FAULT         0x101u
#define CAN_MSGID_CMD           0x200u

/* -----------------------------------------------------------------------
 * Telemetry payload — packed into CAN FD frame data field
 * --------------------------------------------------------------------- */
typedef struct __attribute__((packed)) {
    int16_t  temp_c[3];         /* TMP75B × 3 in 0.01°C units          */
    uint16_t current_ma[3];     /* INA3221 channels in mA               */
    uint16_t voltage_mv[3];     /* INA3221 bus voltages in mV           */
    uint8_t  fault_flags;       /* latched fault bitmask                */
    uint8_t  reserved;
} can_telemetry_t;

/* -----------------------------------------------------------------------
 * Status codes
 * --------------------------------------------------------------------- */
typedef enum {
    CAN_CTRL_OK          = 0,
    CAN_CTRL_ERR_INIT    = 1,
    CAN_CTRL_ERR_TX      = 2,   /* TX FIFO full or tx error             */
    CAN_CTRL_ERR_RX      = 3,   /* RX FIFO empty (not fatal — poll)     */
    CAN_CTRL_ERR_FAULT   = 4,
} can_ctrl_status_t;

/* -----------------------------------------------------------------------
 * RX callback type — registered via can_ctrl_register_rx_callback()
 * --------------------------------------------------------------------- */
typedef void (*can_rx_callback_t)(const MCP251XFD_CANMessage *msg);

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

/* Init MCP2518FD: clock, bitrate, FIFOs, filter, enter Normal mode.
   Call after mcp_port_init_device_struct(). */
can_ctrl_status_t can_ctrl_init(void);

/* Transmit telemetry frame. Returns CAN_CTRL_ERR_TX if FIFO full. */
can_ctrl_status_t can_ctrl_transmit(const can_telemetry_t *data);

/* Poll RX FIFO. Returns CAN_CTRL_ERR_RX if nothing waiting (not fatal). */
can_ctrl_status_t can_ctrl_receive(MCP251XFD_CANMessage *msg_out);

/* Register callback invoked from can_ctrl_irq_handler() on valid RX. */
void can_ctrl_register_rx_callback(can_rx_callback_t cb);

/* Called from app_callbacks.c EXTI handler for CAN_ALRT pins. */
void can_ctrl_irq_handler(uint16_t gpio_pin);

/* Return and clear latched fault flags (bit 0 = ALRT1, bit 1 = ALRT2). */
uint8_t can_ctrl_get_fault_flags(void);

#ifdef __cplusplus
}
#endif

#endif /* CAN_CTRL_H */
