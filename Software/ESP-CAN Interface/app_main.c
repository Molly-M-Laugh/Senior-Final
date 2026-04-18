/**
 * @file    app_main.c
 * @brief   Cell_F103 ESP32 — application entry point
 *
 * Startup sequence:
 *   1. SPI bus init (mcp_port)
 *   2. MCP2518FD device struct init (mcp_port)
 *   3. CAN FD controller init (can_ctrl)
 *   4. BLE stack init  ←  implement ble_ctrl_init() here
 *   5. Register CAN RX callback → BLE forwarding
 *   6. FreeRTOS scheduler takes over
 *
 * Integration guide:
 *   - Implement ble_ctrl_init() in ble_ctrl.c / ble_ctrl.h
 *   - Implement ble_ctrl_notify() to push data to the connected mobile app
 *   - Implement ble_ctrl_send_cmd() to send commands received from the app
 *     down to the STM32 via can_ctrl_send_cmd_*() helpers
 *   - Register your command-from-app callback using
 *     ble_ctrl_register_cmd_callback(on_ble_cmd_received)
 *   - Do NOT call can_ctrl_send() from within the RX callback, it is
 *     called from can_rx_task context. Post to a queue instead.
 */

#include "mcp_port.h"
#include "can_ctrl.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/* -----------------------------------------------------------------------
 * @Colin, @Molly: uncomment and implement this header
 * #include "ble_ctrl.h"
 * --------------------------------------------------------------------- */

static const char *TAG = "app_main";

/* =======================================================================
 * CAN → BLE forward queue
 *
 * The CAN RX callback runs in can_rx_task context. Rather than calling
 * BLE notify directly from that context, we post frames to this queue
 * and drain it from a dedicated BLE forward task. This keeps the CAN
 * RX path fast and avoids re-entrancy issues.
 * ===================================================================== */
#define BLE_FWD_QUEUE_DEPTH     16u

typedef struct
{
    uint32_t msg_id;
    uint8_t  payload[64];
    uint8_t  dlc;
} can_frame_queued_t;

static QueueHandle_t s_ble_fwd_queue = NULL;

/* =======================================================================
 * CAN RX callback
 *
 * Called by can_ctrl when a frame arrives from the STM32. Posts to the
 * BLE forward queue, do not block or call BLE APIs directly here.
 *
 * msg_id  : CAN_MSGID_xxx constant from can_ctrl.h
 * payload : raw frame bytes, valid only for the duration of this call
 * dlc     : payload length in bytes
 * ===================================================================== */
static void on_can_frame_received(uint32_t msg_id,
                                   const uint8_t *payload,
                                   uint8_t dlc)
{
    if (s_ble_fwd_queue == NULL) return;

    can_frame_queued_t frame;
    frame.msg_id = msg_id;
    frame.dlc    = dlc < sizeof(frame.payload) ? dlc : sizeof(frame.payload);
    memcpy(frame.payload, payload, frame.dlc);

    /* Non-blocking post, drop frame if queue is full rather than
       stalling the CAN RX task.                                        */
    if (xQueueSendToBack(s_ble_fwd_queue, &frame, 0) != pdTRUE)
    {
        ESP_LOGW(TAG, "BLE forward queue full — frame 0x%08lX dropped",
                 msg_id);
    }
}

/* =======================================================================
 * BLE forward task
 *
 * Drains the forward queue and passes frames to the BLE notification
 * layer. Runs on Core 1 alongside can_rx_task.
 *
 * replace the ESP_LOGI stub with your ble_ctrl_notify()
 * call. The msg_id tells you which GATT characteristic to write to.
 * ===================================================================== */
static void ble_forward_task(void *arg)
{
    (void)arg;
    can_frame_queued_t frame;

    while (true)
    {
        if (xQueueReceive(s_ble_fwd_queue, &frame, portMAX_DELAY) == pdTRUE)
        {
            switch (frame.msg_id)
            {
                case CAN_MSGID_SENSOR_DATA:
                    /* ble_ctrl_notify(BLE_CHAR_SENSOR_DATA,
                                       frame.payload, frame.dlc); */
                    ESP_LOGI(TAG, "SENSOR_DATA  dlc=%d", frame.dlc);
                    break;

                case CAN_MSGID_POWER_STATE:
                    /* ble_ctrl_notify(BLE_CHAR_POWER_STATE,
                                       frame.payload, frame.dlc); */
                    ESP_LOGI(TAG, "POWER_STATE  dlc=%d", frame.dlc);
                    break;

                case CAN_MSGID_FAULT_ALERT:
                    /* fault alerts are high priority —
                       consider a dedicated characteristic or indication
                       (confirmed delivery) rather than notification.
                       ble_ctrl_indicate(BLE_CHAR_FAULT_ALERT,
                                         frame.payload, frame.dlc); */
                    ESP_LOGW(TAG, "FAULT_ALERT  dlc=%d", frame.dlc);
                    break;

                case CAN_MSGID_TEMP_ALERT:
                    /* 
                       ble_ctrl_indicate(BLE_CHAR_TEMP_ALERT,
                                         frame.payload, frame.dlc); */
                    ESP_LOGW(TAG, "TEMP_ALERT   dlc=%d", frame.dlc);
                    break;

                case CAN_MSGID_FRAM_LOG:
                    /* FRAM chunks arrive in sequence.
                       Reassemble using chunk_index / total_chunks from
                       can_payload_fram_log_t before forwarding.
                       ble_ctrl_notify(BLE_CHAR_FRAM_LOG,
                                       frame.payload, frame.dlc); */
                    ESP_LOGI(TAG, "FRAM_LOG     dlc=%d", frame.dlc);
                    break;

                default:
                    ESP_LOGD(TAG, "Unhandled msg_id 0x%08lX", frame.msg_id);
                    break;
            }
        }
    }
}

/* =======================================================================
 * app_main — runs before FreeRTOS scheduler starts
 * ===================================================================== */
void app_main(void)
{
    ESP_LOGI(TAG, "Cell_F103 ESP32 starting");

    /* ------------------------------------------------------------------
     * 1. SPI bus — must be first, mcp_port_init_device_struct depends on it
     * ---------------------------------------------------------------- */
    esp_err_t err = mcp_port_spi_bus_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(err));
        return;
    }

    /* ------------------------------------------------------------------
     * 2. MCP251XFD device struct — wire function pointers before CAN init
     * ---------------------------------------------------------------- */
    mcp_port_init_device_struct();

    /* ------------------------------------------------------------------
     * 3. CAN FD controller — configures MCP2518FD, starts RX task
     * ---------------------------------------------------------------- */
    can_ctrl_err_t can_err = can_ctrl_init();
    if (can_err != CAN_CTRL_OK)
    {
        ESP_LOGE(TAG, "CAN init failed: %d", can_err);
        return;
    }

    /* ------------------------------------------------------------------
     * 4. BLE forward queue
     * ---------------------------------------------------------------- */
    s_ble_fwd_queue = xQueueCreate(BLE_FWD_QUEUE_DEPTH,
                                    sizeof(can_frame_queued_t));
    if (s_ble_fwd_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create BLE forward queue");
        return;
    }

    /* ------------------------------------------------------------------
     *  initialise your BLE stack here
     *    Example:
     *        ble_ctrl_init();
     *        ble_ctrl_start_advertising();
     * ---------------------------------------------------------------- */

    /* ------------------------------------------------------------------
     * 6. Register CAN RX callback — frames now flow to BLE forward queue
     * ---------------------------------------------------------------- */
    can_ctrl_register_rx_callback(on_can_frame_received);

    /* ------------------------------------------------------------------
     * 7. BLE forward task — drains queue, calls BLE notify
     *    Pinned to Core 1 alongside can_rx_task.
     * ---------------------------------------------------------------- */
    xTaskCreatePinnedToCore(
        ble_forward_task,
        "ble_fwd",
        4096,
        NULL,
        9,           /* one below can_rx_task priority of 10 */
        NULL,
        1            /* Core 1 */
    );

    ESP_LOGI(TAG, "Startup complete — CAN FD active, awaiting BLE init");

    /* app_main returns here — FreeRTOS tasks continue running */
}
