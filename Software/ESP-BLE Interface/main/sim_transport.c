#include "sdkconfig.h"

/* =============================================================================
 * sim_transport.c — simulated UART transport for NETESP
 *
 * Active when CONFIG_NETESP_SIM_MODE=1.
 * Runs a single FreeRTOS task on Core 1 that pushes synthetic frames into
 * g_uart_rx_queue at realistic intervals, mimicking what CANESP would deliver.
 * Any command sent via uart_port_send() is logged to console and discarded.
 *
 * The networking team does not need to modify this file.
 * To adjust sim cadence or fault injection frequency, change the #defines below.
 * =========================================================================== */

#ifdef CONFIG_SIM_MODE

#include "uart_port.h"
#include "frame_codec.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char *TAG = "sim_transport";

/* -------------------------------------------------------------------------- */
/* Sim cadence                                                                 */
/* -------------------------------------------------------------------------- */
#define SIM_TELEM_INTERVAL_MS   100u    /* 10 Hz — matches STM32 telemetry rate */
#define SIM_FAULT_INTERVAL_MS   5000u   /* inject a fault frame every 5 s       */
#define SIM_TASK_STACK          3072u
#define SIM_TASK_PRIORITY       4u

QueueHandle_t g_uart_rx_queue;
QueueHandle_t g_uart_tx_queue;

/* -------------------------------------------------------------------------- */
/* Synthetic data generators                                                   */
/* -------------------------------------------------------------------------- */

/* Produce a realistic-looking telemetry frame.
   Temperatures drift sinusoidally; currents and voltages are near-nominal. */
static void make_telem_frame(frame_telemetry_t *t, uint32_t tick_ms)
{
    float phase = (float)tick_ms / 3000.0f;  /* slow drift period ~3 s */

    /* Temps in 0.01 °C units, centred around 32 °C with ±4 °C swing */
    t->temp_c[0] = (int16_t)(3200 + (int16_t)(400.0f * sinf(phase)));
    t->temp_c[1] = (int16_t)(3200 + (int16_t)(400.0f * sinf(phase + 1.0f)));
    t->temp_c[2] = (int16_t)(3200 + (int16_t)(400.0f * sinf(phase + 2.0f)));

    /* Currents: CH1=5V0 ~800mA, CH2=3V3 ~400mA, CH3=1V8 ~200mA */
    t->current_ma[0] = (uint16_t)(800  + (uint16_t)(50.0f  * sinf(phase * 2.0f)));
    t->current_ma[1] = (uint16_t)(400  + (uint16_t)(30.0f  * sinf(phase * 2.0f)));
    t->current_ma[2] = (uint16_t)(200  + (uint16_t)(15.0f  * sinf(phase * 2.0f)));

    /* Voltages: near nominal */
    t->voltage_mv[0] = 5050u;   /* 5V rail */
    t->voltage_mv[1] = 3310u;   /* 3V3 rail */
    t->voltage_mv[2] = 1805u;   /* 1V8 rail */

    t->fault_flags = 0x00u;
    t->reserved    = 0x00u;
}

/* Rotate through fault scenarios so the networking team can test all paths */
static void make_fault_frame(frame_fault_t *f, uint32_t fault_count)
{
    static const uint32_t fault_scenarios[] = {
        0x00000010u,   /* FAULT_SRC_INA3221_WARN */
        0x00000040u,   /* FAULT_SRC_TEMP0_ALERT  */
        0x00000001u,   /* FAULT_SRC_5V0_FLT      */
        0x00000041u,   /* TEMP0 + 5V0 → critical */
        0x00000007u,   /* all three rails → emergency */
    };
    static const uint8_t tiers[] = { 1, 1, 2, 2, 3 };

    uint32_t idx = fault_count % (sizeof(fault_scenarios) / sizeof(fault_scenarios[0]));
    f->fault_sources = fault_scenarios[idx];
    f->tier          = tiers[idx];
    f->rail_states   = (f->tier == 3) ? 0x00u : 0x07u;  /* rails off on emergency */
}

/* -------------------------------------------------------------------------- */
/* Sim task                                                                    */
/* -------------------------------------------------------------------------- */
static void sim_task(void *arg)
{
    ESP_LOGI(TAG, "Sim transport running on core %d — SIM MODE ACTIVE", xPortGetCoreID());
    ESP_LOGW(TAG, "No real UART. All data is synthetic. Disable CONFIG_NETESP_SIM_MODE for hardware.");

    uint32_t last_telem_ms = 0;
    uint32_t last_fault_ms = 0;
    uint32_t fault_count   = 0;
    uart_port_msg_t msg;

    for (;;) {
        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        /* --- Push telemetry frame --- */
        if ((now_ms - last_telem_ms) >= SIM_TELEM_INTERVAL_MS) {
            last_telem_ms = now_ms;

            frame_telemetry_t t;
            make_telem_frame(&t, now_ms);

            msg.type        = FRAME_TYPE_TELEMETRY;
            msg.payload_len = sizeof(frame_telemetry_t);
            memcpy(msg.payload, &t, sizeof(frame_telemetry_t));

            if (xQueueSend(g_uart_rx_queue, &msg, 0) != pdTRUE) {
                ESP_LOGW(TAG, "RX queue full — telemetry frame dropped");
            }
        }

        /* --- Push fault frame --- */
        if ((now_ms - last_fault_ms) >= SIM_FAULT_INTERVAL_MS) {
            last_fault_ms = now_ms;

            frame_fault_t f;
            make_fault_frame(&f, fault_count++);

            msg.type        = FRAME_TYPE_FAULT;
            msg.payload_len = sizeof(frame_fault_t);
            memcpy(msg.payload, &f, sizeof(frame_fault_t));

            ESP_LOGI(TAG, "SIM fault injected: sources=0x%08lX tier=%u",
                     f.fault_sources, f.tier);

            if (xQueueSend(g_uart_rx_queue, &msg, 0) != pdTRUE) {
                ESP_LOGW(TAG, "RX queue full — fault frame dropped");
            }
        }

        /* --- Drain TX queue (log and discard) --- */
        while (xQueueReceive(g_uart_tx_queue, &msg, 0) == pdTRUE) {
            ESP_LOGI(TAG, "SIM TX: type=0x%02X len=%u (would send to CANESP)",
                     msg.type, msg.payload_len);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */

void uart_port_init(void)
{
    g_uart_rx_queue = xQueueCreate(UART_PORT_QUEUE_DEPTH, sizeof(uart_port_msg_t));
    g_uart_tx_queue = xQueueCreate(UART_PORT_QUEUE_DEPTH, sizeof(uart_port_msg_t));

    configASSERT(g_uart_rx_queue);
    configASSERT(g_uart_tx_queue);

    xTaskCreatePinnedToCore(sim_task, "sim_transport", SIM_TASK_STACK,
                            NULL, SIM_TASK_PRIORITY, NULL, 1);
}

bool uart_port_receive(uart_port_msg_t *msg_out, uint32_t timeout_ms)
{
    return xQueueReceive(g_uart_rx_queue, msg_out,
                         pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

bool uart_port_send(uint8_t type, const void *payload, uint16_t payload_len)
{
    uart_port_msg_t msg;
    msg.type        = type;
    msg.payload_len = payload_len;
    if (payload_len > 0) memcpy(msg.payload, payload, payload_len);
    return xQueueSend(g_uart_tx_queue, &msg, 0) == pdTRUE;
}

#endif /* CONFIG_SIM_MODE */
