#ifndef CAN_CTRL_H
#define CAN_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "MCP251XFD.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * OSC frequency — DSC6111B output fed into MCP2518FD CLKIN.
 * *** VERIFY THIS AGAINST YOUR BOM BEFORE BENCH TESTING ***
 * Common DSC6111B frequencies: 4, 8, 20, 40 MHz.
 * --------------------------------------------------------------------- */
#define CAN_OSC_FREQ_HZ         40000000u   /* <-- UPDATE THIS */

/* -----------------------------------------------------------------------
 * CAN bitrates
 * Nominal (arbitration): 500 kbps
 * Data (CAN FD payload):   2 Mbps
 * Adjust to match network requirements.
 * --------------------------------------------------------------------- */
#define CAN_NOMINAL_BITRATE     500000u
#define CAN_DATA_BITRATE        2000000u

/* -----------------------------------------------------------------------
 * FIFO assignments
 * FIFO1 = TX  (8 messages deep, 64-byte payload)
 * FIFO2 = RX  (8 messages deep, 64-byte payload)
 * --------------------------------------------------------------------- */
#define CAN_TX_FIFO             MCP251XFD_FIFO1
#define CAN_RX_FIFO             MCP251XFD_FIFO2

/* -----------------------------------------------------------------------
 * Return type
 * --------------------------------------------------------------------- */
typedef enum {
    CAN_CTRL_OK         = 0,
    CAN_CTRL_ERR_INIT   = 1,  /* Init_MCP251XFD failed            */
    CAN_CTRL_ERR_FIFO   = 2,  /* FIFO configuration failed        */
    CAN_CTRL_ERR_FILTER = 3,  /* Filter configuration failed      */
    CAN_CTRL_ERR_TX     = 4,  /* Transmit failed                  */
    CAN_CTRL_ERR_RX     = 5,  /* Receive failed or FIFO empty     */
} can_ctrl_status_t;

/* -----------------------------------------------------------------------
 * Telemetry message structure — what this node puts on the bus.
 * All sensor readings packed into one 64-byte CAN FD frame.
 * --------------------------------------------------------------------- */
typedef struct {
    int32_t  temp_mdegc[3];    /* TMP75B readings, millidegrees C  */
    int32_t  bus_mv[3];        /* INA3221 bus voltages, mV         */
    int32_t  current_ua[3];    /* INA3221 derived currents, uA     */
    uint8_t  fault_flags;      /* Power fault flags                */
    uint8_t  alert_flags;      /* Temperature alert flags          */
    uint8_t  reserved[2];      /* Pad to 4-byte boundary           */
} can_telemetry_t;             /* Total: 32 bytes — fits in 64B payload */

/* -----------------------------------------------------------------------
 * CAN message ID for this node's telemetry frame.
 * *** Define your node ID scheme here ***
 * --------------------------------------------------------------------- */
#define CAN_TELEMETRY_MSG_ID    0x100u

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

/* Initialize MCP2518FD: clock, bitrate, FIFOs, filter, enter Normal mode */
can_ctrl_status_t can_ctrl_init(void);

/* Transmit a telemetry frame. Blocks until space in TX FIFO or returns
   CAN_CTRL_ERR_TX if the FIFO is full. */
can_ctrl_status_t can_ctrl_transmit(const can_telemetry_t *data);

/* Check RX FIFO and read one message if available.
   Returns CAN_CTRL_ERR_RX if FIFO is empty (not an error, just poll). */
can_ctrl_status_t can_ctrl_receive(MCP251XFD_CANMessage *msg_out);

/* Called from HAL_GPIO_EXTI_Callback for CAN_ALRT1, CAN_ALRT2, CAN_FLT */
void can_ctrl_fault_irq_handler(uint16_t gpio_pin);

/* Return and clear latched CAN fault flags.
   Bit 0 = ALRT1 (TX int), Bit 1 = ALRT2 (RX int), Bit 2 = FLT */
uint8_t can_ctrl_get_fault_flags(void);

#ifdef __cplusplus
}
#endif

#endif /* CAN_CTRL_H */
