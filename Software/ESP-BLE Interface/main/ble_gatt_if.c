/* =======================================================================
 * ble_gatt_if.c — UART → BLE dispatch layer
 *
 * Sits between uart_port (queue) and ble_ctrl (NimBLE notify).
 * Owns nothing except the dispatch task.
 * ===================================================================== */
#include "ble_gatt_if.h"
#include "ble_ctrl.h"
#include "uart_port.h"
#include "frame_codec.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "ble_gatt_if";

#define DISPATCH_TASK_STACK     3072u
#define DISPATCH_TASK_PRIORITY  4u
#define DISPATCH_RX_TIMEOUT_MS  20u

/* -----------------------------------------------------------------------
 * Dispatch task — Core 0, same core as NimBLE stack.
 * Pulls frames from g_uart_rx_queue and pushes to phone via ble_ctrl.
 * --------------------------------------------------------------------- */
static void dispatch_task(void *arg)
{
    ESP_LOGI(TAG, "dispatch task running on core %d", xPortGetCoreID());

    uart_port_msg_t msg;

    for (;;) {
        if (!uart_port_receive(&msg, DISPATCH_RX_TIMEOUT_MS)) continue;

        switch (msg.type) {

        case FRAME_TYPE_TELEMETRY:
            if (msg.payload_len >= sizeof(frame_telemetry_t)) {
                ble_ctrl_notify_telemetry((const frame_telemetry_t *)msg.payload);
            } else {
                ESP_LOGW(TAG, "telemetry payload short: %u bytes", msg.payload_len);
            }
            break;

        case FRAME_TYPE_FAULT:
            if (msg.payload_len >= sizeof(frame_fault_t)) {
                ble_ctrl_notify_fault((const frame_fault_t *)msg.payload);
            } else {
                ESP_LOGW(TAG, "fault payload short: %u bytes", msg.payload_len);
            }
            break;

        default:
            ESP_LOGD(TAG, "unexpected frame type 0x%02x — discarding", msg.type);
            break;
        }
    }
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */
void ble_gatt_if_start(void)
{
    xTaskCreatePinnedToCore(dispatch_task, "ble_dispatch",
                            DISPATCH_TASK_STACK, NULL,
                            DISPATCH_TASK_PRIORITY, NULL, 0);
}