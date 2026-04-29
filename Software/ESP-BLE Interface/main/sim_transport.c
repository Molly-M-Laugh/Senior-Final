/* =======================================================================
 * sim_transport.c — fake transport for networking team testing
 *
 * Generates synthetic telemetry and fault frames on a timer so the
 * BLE stack can be exercised without CANESP or real CAN traffic.
 *
 * Compiled only when CONFIG_SIM_MODE=y in sdkconfig.
 * ===================================================================== */
#include "uart_port.h"
#include "frame_codec.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

#define TAG             "sim_transport"
#define SIM_QUEUE_DEPTH 8
#define TELEM_INTERVAL_MS   100
#define FAULT_INTERVAL_MS   5000

static QueueHandle_t s_rx_queue;

/* Generates incrementing fake telemetry every 100ms */
static void sim_task(void *arg)
{
    uint32_t tick = 0;
    uint8_t  fault_rotate = 0x01u;

    while (1) {
        uart_frame_t frame;
        memset(&frame, 0, sizeof(frame));

        if ((tick % (FAULT_INTERVAL_MS / TELEM_INTERVAL_MS)) == 0 && tick != 0) {
            /* Inject a rotating fault frame every 5s */
            frame_fault_t f = { .fault_flags = fault_rotate };
            fault_rotate = (uint8_t)(fault_rotate << 1) | (fault_rotate >> 7); /* rotate */
            frame.type = FRAME_TYPE_FAULT;
            frame.len  = sizeof(frame_fault_t);
            memcpy(frame.payload, &f, frame.len);
            ESP_LOGI(TAG, "sim fault frame: flags=0x%02X", f.fault_flags);
        } else {
            /* Inject a telemetry frame every 100ms */
            frame_telemetry_t t = {
                .temp_c      = { (int16_t)(2500 + tick % 500),   /* ~25.00°C + drift */
                                 (int16_t)(3100 + tick % 200),
                                 (int16_t)(2200 + tick % 300) },
                .current_ma  = { (uint16_t)(100 + tick % 50),
                                 (uint16_t)(200 + tick % 80),
                                 (uint16_t)(50  + tick % 20) },
                .voltage_mv  = { 5000, 3300, 1800 },             /* nominal rails    */
                .fault_flags = 0x00u,
                .reserved    = 0x00u,
            };
            frame.type = FRAME_TYPE_TELEMETRY;
            frame.len  = sizeof(frame_telemetry_t);
            memcpy(frame.payload, &t, frame.len);
        }

        /* Non-blocking push; drop if queue full (networking team's problem) */
        if (xQueueSend(s_rx_queue, &frame, 0) != pdTRUE) {
            ESP_LOGW(TAG, "sim RX queue full — frame dropped");
        }

        tick++;
        vTaskDelay(pdMS_TO_TICKS(TELEM_INTERVAL_MS));
    }
}

/* --- uart_port.h interface implementation ----------------------------- */

void uart_port_init(void)
{
    s_rx_queue = xQueueCreate(SIM_QUEUE_DEPTH, sizeof(uart_frame_t));
    configASSERT(s_rx_queue);

    /* Pin to Core 1 alongside ble_ctrl, same as the real uart_task would */
    xTaskCreatePinnedToCore(sim_task, "sim_task", 4096, NULL, 5, NULL, 1);
    ESP_LOGI(TAG, "sim transport active — BLE test mode");
}

bool uart_port_read(uart_frame_t *out, uint32_t timeout_ms)
{
    TickType_t ticks = (timeout_ms == portMAX_DELAY)
                       ? portMAX_DELAY
                       : pdMS_TO_TICKS(timeout_ms);
    return xQueueReceive(s_rx_queue, out, ticks) == pdTRUE;
}

bool uart_port_write(const uart_frame_t *frame)
{
    /* Commands going "toward CANESP" — just log them in sim mode */
    ESP_LOGI(TAG, "sim cmd TX: type=0x%02X len=%u payload[0]=0x%02X",
             frame->type, frame->len,
             frame->len > 0 ? frame->payload[0] : 0);
    return true;
}