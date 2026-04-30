/* =============================================================================
 * main_canesp.c — CANESP app_main
 *
 * Startup order:
 *   1. mcp_port_init()    — SPI bus + MCP251XFD device struct
 *   2. uart_port_init()   — UART DMA queues + task on Core 0
 *   3. can_task_start()   — MCP2518FD init + CAN task on Core 1
 *
 * Core assignment:
 *   Core 0: uart_task (UART DMA ↔ NETESP)
 *   Core 1: can_task  (MCP2518FD ↔ CAN FD bus)
 *
 * Data flow:
 *   CAN bus → MCP2518FD → can_task → uart_port_send() → UART DMA → NETESP
 *   NETESP  → UART DMA  → uart_port RX queue → can_task → CAN bus
 * =========================================================================== */
#include "mcp_port.h"
#include "uart_port.h"
#include "can_task.h"
#include "esp_log.h"

static const char *TAG = "canesp_main";

void app_main(void)
{
    ESP_LOGI(TAG, "CANESP starting");

    /* Step 1: SPI bus and MCP251XFD device struct */
    mcp_port_init();

    /* Step 2: UART DMA transport to NETESP (task pins to Core 0) */
    uart_port_init();

    /* Step 3: MCP2518FD init + CAN task (pins to Core 1) */
    can_task_start();

    ESP_LOGI(TAG, "CANESP init complete");
}
