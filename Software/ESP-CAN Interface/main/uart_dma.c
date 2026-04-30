/* =============================================================================
 * uart_dma.c — UART DMA transport for CANESP
 *
 * Same frame_codec protocol as NETESP's uart_dma.c. Symmetric link.
 *
 * GPIO —  CONFIRM against CANESP schematic:
 *   TX: GPIO_NUM_17    CONFIRM  (connects to NETESP RX)
 *   RX: GPIO_NUM_16    CONFIRM  (connects to NETESP TX)
 *
 * Baud: 921600 — matches NETESP.
 *
 * Queue model (same as NETESP uart_port):
 *   g_uart_rx_queue: frames arriving FROM NETESP (commands) → can_task
 *   g_uart_tx_queue: frames going TO NETESP (telemetry, fault) ← can_task
 * =========================================================================== */
#include "uart_port.h"
#include "frame_codec.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "uart_dma";

#define UART_PORT_NUM           UART_NUM_1
#define UART_TX_GPIO            17          /* CONFIRM */
#define UART_RX_GPIO            16          /* CONFIRM */
#define UART_BAUD_RATE          921600
#define UART_RX_BUF_SIZE        1024u
#define UART_TX_BUF_SIZE        1024u
#define UART_EVENT_QUEUE_DEPTH  20u
#define UART_TASK_STACK         4096u
#define UART_TASK_PRIORITY      5u

QueueHandle_t g_uart_rx_queue;
QueueHandle_t g_uart_tx_queue;

static QueueHandle_t s_uart_event_queue;
static uint8_t       s_rx_buf[UART_RX_BUF_SIZE];
static uint16_t      s_rx_head = 0;

/* -------------------------------------------------------------------------- */
/* UART task — Core 0                                                          */
/* -------------------------------------------------------------------------- */
static void uart_task(void *arg)
{
    uart_event_t    event;
    uint8_t         tmp[UART_RX_BUF_SIZE];
    uart_port_msg_t app_msg;

    ESP_LOGI(TAG, "UART task running on core %d", xPortGetCoreID());

    for (;;) {
        if (xQueueReceive(s_uart_event_queue, &event, pdMS_TO_TICKS(5)) == pdTRUE) {
            switch (event.type) {

            case UART_DATA: {
                int len = uart_read_bytes(UART_PORT_NUM, tmp,
                                          event.size, pdMS_TO_TICKS(10));
                if (len <= 0) break;

                for (int i = 0; i < len; i++) {
                    if (s_rx_head < sizeof(s_rx_buf))
                        s_rx_buf[s_rx_head++] = tmp[i];
                }

                uint16_t offset = 0;
                while (offset < s_rx_head) {
                    uint8_t  type;
                    uint8_t  payload[FRAME_MAX_PAYLOAD_LEN];
                    uint16_t payload_len;
                    uint16_t consumed;

                    frame_status_t st = frame_decode(
                        &s_rx_buf[offset], s_rx_head - offset,
                        &type, payload, &payload_len, &consumed);

                    if (st == FRAME_OK) {
                        app_msg.type        = type;
                        app_msg.payload_len = payload_len;
                        memcpy(app_msg.payload, payload, payload_len);
                        if (xQueueSend(g_uart_rx_queue, &app_msg, 0) != pdTRUE)
                            ESP_LOGW(TAG, "RX queue full — cmd frame dropped");
                        offset += consumed;
                    } else if (st == FRAME_ERR_NO_FRAME) {
                        break;
                    } else {
                        offset += (consumed > 0) ? consumed : 1u;
                        ESP_LOGD(TAG, "Frame decode error %d, resyncing", st);
                    }
                }

                if (offset > 0) {
                    s_rx_head -= offset;
                    memmove(s_rx_buf, &s_rx_buf[offset], s_rx_head);
                }
                break;
            }

            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "FIFO overflow");
                uart_flush_input(UART_PORT_NUM);
                xQueueReset(s_uart_event_queue);
                s_rx_head = 0;
                break;

            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "Ring buffer full");
                uart_flush_input(UART_PORT_NUM);
                s_rx_head = 0;
                break;

            default:
                break;
            }
        }

        /* Drain TX queue — encode and send to NETESP */
        uart_port_msg_t tx_msg;
        while (xQueueReceive(g_uart_tx_queue, &tx_msg, 0) == pdTRUE) {
            uint8_t  wire[FRAME_MAX_WIRE_LEN];
            uint16_t wire_len;
            frame_status_t st = frame_encode(tx_msg.type,
                                             tx_msg.payload, tx_msg.payload_len,
                                             wire, &wire_len);
            if (st == FRAME_OK)
                uart_write_bytes(UART_PORT_NUM, (const char *)wire, wire_len);
            else
                ESP_LOGE(TAG, "TX encode failed: %d", st);
        }
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

    uart_config_t cfg = {
        .baud_rate  = UART_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM,
                                 UART_TX_GPIO, UART_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM,
                                        UART_RX_BUF_SIZE, UART_TX_BUF_SIZE,
                                        UART_EVENT_QUEUE_DEPTH,
                                        &s_uart_event_queue, 0));

    xTaskCreatePinnedToCore(uart_task, "uart_dma", UART_TASK_STACK,
                            NULL, UART_TASK_PRIORITY, NULL, 0);
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
