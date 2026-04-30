/* =============================================================================
 * uart_port.h — UART transport seam for CANESP
 *
 * Identical interface to NETESP's uart_port.h.
 * On CANESP there is no sim mode — real hardware only.
 *
 * Queue model:
 *   g_uart_rx_queue: commands arriving FROM NETESP → can_task TX
 *   g_uart_tx_queue: telemetry/fault going TO NETESP ← can_task RX
 * =========================================================================== */
#pragma once

#include "frame_codec.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>

typedef struct {
    uint8_t  type;
    uint8_t  payload[FRAME_MAX_PAYLOAD_LEN];
    uint16_t payload_len;
} uart_port_msg_t;

extern QueueHandle_t g_uart_rx_queue;
extern QueueHandle_t g_uart_tx_queue;

#define UART_PORT_QUEUE_DEPTH   16u

void uart_port_init(void);
bool uart_port_receive(uart_port_msg_t *msg_out, uint32_t timeout_ms);
bool uart_port_send(uint8_t type, const void *payload, uint16_t payload_len);
