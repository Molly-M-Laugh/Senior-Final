/* =======================================================================
 * main_netesp.c — NETESP app_main
 *
 * Core 0: NimBLE host task (pinned by nimble_port_freertos_init)
 * Core 1: ble_dispatch_task — reads uart_port, calls ble_ctrl_notify_*
 * ===================================================================== */
#include "uart_port.h"
#include "ble_ctrl.h"
#include "frame_codec.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

#define TAG "main_netesp"

/* -----------------------------------------------------------------------
 * Command callback — phone → CANESP (forwarded via uart_port_write)
 * --------------------------------------------------------------------- */
static void on_cmd_received(uint8_t cmd)
{
    uart_frame_t frame;
    frame_cmd_t  payload = { .cmd = cmd };
    frame.type = FRAME_TYPE_CMD;
    frame.len  = sizeof(frame_cmd_t);
    memcpy(frame.payload, &payload, frame.len);
    uart_port_write(&frame);
}

/* -----------------------------------------------------------------------
 * Dispatch task — reads from uart_port, routes to BLE notify calls
 * Pinned to Core 1.
 * --------------------------------------------------------------------- */
static void ble_dispatch_task(void *arg)
{
    uart_frame_t frame;
    while (1) {
        if (!uart_port_read(&frame, portMAX_DELAY)) continue;

        switch (frame.type) {

        case FRAME_TYPE_TELEMETRY:
            if (frame.len >= sizeof(frame_telemetry_t)) {
                frame_telemetry_t t;
                memcpy(&t, frame.payload, sizeof(t));
                ble_ctrl_notify_telemetry(&t);
            }
            break;

        case FRAME_TYPE_FAULT:
            if (frame.len >= sizeof(frame_fault_t)) {
                frame_fault_t f;
                memcpy(&f, frame.payload, sizeof(f));
                ble_ctrl_notify_fault(&f);
            }
            break;

        default:
            ESP_LOGW(TAG, "unknown frame type 0x%02X", frame.type);
            break;
        }
    }
}

/* -----------------------------------------------------------------------
 * app_main
 * --------------------------------------------------------------------- */
void app_main(void)
{
    /* Transport first (real or sim depending on build) */
    uart_port_init();

    /* Register command callback before BLE init */
    ble_ctrl_register_cmd_callback(on_cmd_received);

    /* BLE init — starts NimBLE host on Core 0, begins advertising */
    ble_ctrl_init();

    /* Dispatch task on Core 1 */
    xTaskCreatePinnedToCore(ble_dispatch_task, "ble_dispatch",
                            4096, NULL, 5, NULL, 1);
}