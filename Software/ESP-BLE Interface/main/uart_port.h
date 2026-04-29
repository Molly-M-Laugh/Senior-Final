/* =======================================================================
 * uart_port.h — transport seam between NETESP app and physical UART/sim
 *
 * Real build:  uart_dma.c is compiled in
 * Sim build:   sim_transport.c is compiled in (CONFIG_SIM_MODE=y)
 *
 * Application code calls only these functions — never the impl directly.
 * ===================================================================== */
#ifndef UART_PORT_H
#define UART_PORT_H

#include "frame_codec.h"
#include <stdbool.h>

/* Frame envelope delivered to the application */
typedef struct {
    uint8_t  type;          /* FRAME_TYPE_* */
    uint8_t  payload[64];   /* raw payload bytes */
    uint8_t  len;           /* payload length in bytes */
} uart_frame_t;

/* Initialize the transport (real or sim). Call once from app_main. */
void uart_port_init(void);

/* Block until a frame arrives (use portMAX_DELAY or a finite timeout).
   Returns true on success, false on timeout. */
bool uart_port_read(uart_frame_t *out, uint32_t timeout_ms);

/* Send a frame toward CANESP (commands from phone).
   Returns true if enqueued successfully. */
bool uart_port_write(const uart_frame_t *frame);

#endif /* UART_PORT_H */