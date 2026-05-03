/* =============================================================================
 * app_bridge.c
 * =========================================================================== */
#include "app_bridge.h"
#include "uart_port.h"
#include "frame_codec.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "app_bridge";

#define BRIDGE_TASK_STACK    3072u
#define BRIDGE_TASK_PRIORITY 4u
#define BRIDGE_RX_TIMEOUT_MS 20u

static publish_fn_t s_publish_fn = NULL;

/* -------------------------------------------------------------------------- */
/* Frame handlers                                                              */
/* -------------------------------------------------------------------------- */

static void handle_telemetry(const uart_port_msg_t *msg)
{
    if (msg->payload_len < sizeof(frame_telemetry_t)) {
        ESP_LOGW(TAG, "Telemetry payload short: %u bytes", msg->payload_len);
        return;
    }

    /* Log summary, useful to verify data flow */
    const frame_telemetry_t *t = (const frame_telemetry_t *)msg->payload;
    ESP_LOGD(TAG, "Telem: T=[%d,%d,%d] mA=[%u,%u,%u] mV=[%u,%u,%u] faults=0x%02X",
             t->temp_c[0], t->temp_c[1], t->temp_c[2],
             t->current_ma[0], t->current_ma[1], t->current_ma[2],
             t->voltage_mv[0], t->voltage_mv[1], t->voltage_mv[2],
             t->fault_flags);

    if (s_publish_fn) {
        s_publish_fn(BRIDGE_TOPIC_TELEMETRY, msg->payload, msg->payload_len);
    }
}

static void handle_fault(const uart_port_msg_t *msg)
{
    if (msg->payload_len < sizeof(frame_fault_t)) {
        ESP_LOGW(TAG, "Fault payload short: %u bytes", msg->payload_len);
        return;
    }

    const frame_fault_t *f = (const frame_fault_t *)msg->payload;
    ESP_LOGW(TAG, "FAULT: sources=0x%08lX tier=%u rail_states=0x%02X",
             f->fault_sources, f->tier, f->rail_states);

    if (s_publish_fn) {
        s_publish_fn(BRIDGE_TOPIC_FAULT, msg->payload, msg->payload_len);
    }
}

/* -------------------------------------------------------------------------- */
/* Bridge task — runs on Core 1                                               */
/* -------------------------------------------------------------------------- */
static void bridge_task(void *arg)
{
    ESP_LOGI(TAG, "Bridge task running on core %d", xPortGetCoreID());

    uart_port_msg_t msg;

    for (;;) {
        if (!uart_port_receive(&msg, BRIDGE_RX_TIMEOUT_MS)) continue;

        switch (msg.type) {
            case FRAME_TYPE_TELEMETRY:        handle_telemetry(&msg); break;
            case FRAME_TYPE_FAULT:            handle_fault(&msg);     break;
            default:
                ESP_LOGD(TAG, "Unexpected frame type 0x%02X from CANESP", msg.type);
                break;
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */

void app_bridge_register_publish_fn(publish_fn_t fn)
{
    s_publish_fn = fn;
}

void app_bridge_start(void)
{
    xTaskCreatePinnedToCore(bridge_task, "app_bridge", BRIDGE_TASK_STACK,
                            NULL, BRIDGE_TASK_PRIORITY, NULL, 1);
}

bool app_bridge_on_cmd_received(uint8_t cmd_type)
{
    /* Validate it's a known command type before forwarding */
    switch (cmd_type) {
        case FRAME_TYPE_CMD_PING:
        case FRAME_TYPE_CMD_FORCE_LOG:
        case FRAME_TYPE_CMD_CLEAR_FAULTS:
        case FRAME_TYPE_CMD_RESET:
            break;
        default:
            ESP_LOGW(TAG, "Unknown command type 0x%02X — discarding", cmd_type);
            return false;
    }

    /* No payload for any current command type */
    bool ok = uart_port_send(cmd_type, NULL, 0);
    if (!ok) ESP_LOGW(TAG, "TX queue full — command 0x%02X dropped", cmd_type);
    return ok;
}
