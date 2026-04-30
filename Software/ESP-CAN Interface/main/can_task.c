/* =============================================================================
 * can_task.c — CAN FD task for CANESP, Core 1
 *
 * Uses the Mailly MCP251XFD driver via mcp_port — same API as STM32 nodes.
 * Calls uart_port_send() to push decoded frames to NETESP.
 * Calls MCP251XFD_TransmitMessageToFIFO() to put commands onto the bus.
 * =========================================================================== */
#include "can_task.h"
#include "mcp_port.h"
#include "uart_port.h"
#include "frame_codec.h"
#include "MCP251XFD.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "can_task";

#define CAN_TASK_STACK      4096u
#define CAN_TASK_PRIORITY   6u      /* highest on Core 1 — CAN is time-sensitive */

#define CAN_TX_FIFO         MCP251XFD_TXQ
#define CAN_RX_FIFO         MCP251XFD_FIFO2
#define CAN_FILTER_0        MCP251XFD_FILTER0

/* INT1 signals RX FIFO not empty — used as a binary semaphore from ISR */
static SemaphoreHandle_t s_rx_sem;

/* -------------------------------------------------------------------------- */
/* MCP2518FD static configuration — mirrors STM32 can_ctrl.c exactly        */
/* -------------------------------------------------------------------------- */
static MCP251XFD_Config s_can_config = {
    .XtalFreq      = 0,
    .OscFreq       = CAN_OSC_FREQ_HZ,
    .SysclkConfig  = MCP251XFD_SYSCLK_IS_CLKIN,
    .ClkoPinConfig = MCP251XFD_CLKO_DivBy10,
    .SYSCLK_Result = NULL,

    .NominalBitrate = CAN_NOMINAL_BITRATE,
    .DataBitrate    = CAN_DATA_BITRATE,
    .BitTimeStats   = NULL,
    .Bandwidth      = MCP251XFD_DELAY_16BIT_TIMES,
    .ControlFlags   = MCP251XFD_CAN_UNLIMITED_RETRANS_ATTEMPTS
                    | MCP251XFD_CANFD_BITRATE_SWITCHING_ENABLE
                    | MCP251XFD_CANFD_USE_ISO_CRC,

    .GPIO0PinMode  = MCP251XFD_PIN_AS_INT0_TX,   /* INT0 → TX done/err    */
    .GPIO1PinMode  = MCP251XFD_PIN_AS_INT1_RX,   /* INT1 → RX not empty   */
    .INTsOutMode   = MCP251XFD_PINS_OPENDRAIN_OUT,
    .TXCANOutMode  = MCP251XFD_PINS_PUSHPULL_OUT,

    .SysInterruptFlags = MCP251XFD_INT_BUS_ERROR_EVENT
                       | MCP251XFD_INT_RX_INVALID_MESSAGE_EVENT,
};

static MCP251XFD_FIFO s_fifo_list[2] = {
    {
        .Name           = CAN_TX_FIFO,
        .Size           = MCP251XFD_FIFO_8_MESSAGE_DEEP,
        .Payload        = MCP251XFD_PAYLOAD_64BYTE,
        .Direction      = MCP251XFD_TRANSMIT_FIFO,
        .Attempts       = MCP251XFD_THREE_ATTEMPTS,
        .Priority       = MCP251XFD_MESSAGE_TX_PRIORITY16,
        .ControlFlags   = MCP251XFD_FIFO_NO_CONTROL_FLAGS,
        .InterruptFlags = MCP251XFD_FIFO_TX_ATTEMPTS_EXHAUSTED_INT,
        .RAMInfos       = NULL,
    },
    {
        .Name           = CAN_RX_FIFO,
        .Size           = MCP251XFD_FIFO_8_MESSAGE_DEEP,
        .Payload        = MCP251XFD_PAYLOAD_64BYTE,
        .Direction      = MCP251XFD_RECEIVE_FIFO,
        .Attempts       = MCP251XFD_THREE_ATTEMPTS,
        .Priority       = MCP251XFD_MESSAGE_TX_PRIORITY30,
        .ControlFlags   = MCP251XFD_FIFO_NO_CONTROL_FLAGS,
        .InterruptFlags = MCP251XFD_FIFO_RECEIVE_FIFO_NOT_EMPTY_INT,
        .RAMInfos       = NULL,
    },
};

/* Accept 0x100–0x1FF — all STM32 node telemetry and fault frames */
static MCP251XFD_Filter s_filter_list[1] = {
    {
        .Filter         = CAN_FILTER_0,
        .EnableFilter   = true,
        .Match          = MCP251XFD_MATCH_ONLY_SID,
        .AcceptanceID   = CAN_FILTER_ACCEPT_ID,
        .AcceptanceMask = CAN_FILTER_ACCEPT_MASK,  /*  tighten when IDs finalised */
        .PointTo        = CAN_RX_FIFO,
    },
};

/* -------------------------------------------------------------------------- */
/* INT1 ISR — signals RX semaphore so task wakes immediately on frame arrival */
/* -------------------------------------------------------------------------- */
static void IRAM_ATTR int1_isr_handler(void *arg)
{
    BaseType_t higher_prio_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_rx_sem, &higher_prio_woken);
    portYIELD_FROM_ISR(higher_prio_woken);
}

/* -------------------------------------------------------------------------- */
/* Frame dispatch — identify and forward to NETESP via uart_port             */
/* -------------------------------------------------------------------------- */
static void dispatch_rx_frame(const MCP251XFD_CANMessage *msg)
{
    uint32_t id       = msg->MessageID;
    uint8_t  low_byte = (uint8_t)(id & 0x00Fu);

    /* Identify frame type by lower nibble of ID:
       0x?00 family → telemetry, 0x?01 family → fault */
    if (low_byte == 0x00u) {
        /* Telemetry frame — verify payload size */
        if (msg->PayloadData == NULL) return;

        /* Node index from upper nibble offset */
        uint8_t node = (uint8_t)((id - CAN_BASE_TELEM_ID) / CAN_NODE_ID_STRIDE);
        ESP_LOGD(TAG, "Telem from node %u (ID=0x%03lX)", node, id);

        bool ok = uart_port_send(FRAME_TYPE_TELEMETRY,
                                 msg->PayloadData,
                                 sizeof(frame_telemetry_t));
        if (!ok) ESP_LOGW(TAG, "UART TX full — telemetry from node %u dropped", node);

    } else if (low_byte == 0x01u) {
        /* Fault frame */
        if (msg->PayloadData == NULL) return;

        uint8_t node = (uint8_t)((id - CAN_BASE_FAULT_ID) / CAN_NODE_ID_STRIDE);
        ESP_LOGW(TAG, "Fault from node %u (ID=0x%03lX)", node, id);

        bool ok = uart_port_send(FRAME_TYPE_FAULT,
                                 msg->PayloadData,
                                 sizeof(frame_fault_t));
        if (!ok) ESP_LOGW(TAG, "UART TX full — fault from node %u dropped", node);

    } else {
        ESP_LOGD(TAG, "Unhandled CAN ID 0x%03lX — ignoring", id);
    }
}

/* -------------------------------------------------------------------------- */
/* Drain RX FIFO                                                               */
/* -------------------------------------------------------------------------- */
static void drain_rx_fifo(void)
{
    uint8_t payload_buf[64];
    MCP251XFD_CANMessage msg;

    for (;;) {
        /* Check FIFO status */
        setMCP251XFD_FIFOstatus fifo_status = 0;
        eERRORRESULT res = MCP251XFD_GetFIFOStatus(&mcp251xfd_dev,
                                                    CAN_RX_FIFO,
                                                    &fifo_status);
        if (res != ERR_OK) break;
        if ((fifo_status & MCP251XFD_RX_FIFO_NOT_EMPTY) == 0u) break;

        memset(payload_buf, 0, sizeof(payload_buf));
        msg.PayloadData = payload_buf;

        res = MCP251XFD_ReceiveMessageFromFIFO(&mcp251xfd_dev,
                                               &msg,
                                               MCP251XFD_PAYLOAD_64BYTE,
                                               NULL,
                                               CAN_RX_FIFO);
        if (res != ERR_OK) break;

        dispatch_rx_frame(&msg);
    }
}

/* -------------------------------------------------------------------------- */
/* Transmit a command frame onto the CAN bus                                  */
/* -------------------------------------------------------------------------- */
static void transmit_cmd(uint8_t frame_type)
{
    /* Map FRAME_TYPE_CMD_* back to CAN message ID */
    uint32_t can_id;
    switch (frame_type) {
        case FRAME_TYPE_CMD_PING:         can_id = CAN_CMD_PING;         break;
        case FRAME_TYPE_CMD_FORCE_LOG:    can_id = CAN_CMD_FORCE_LOG;    break;
        case FRAME_TYPE_CMD_CLEAR_FAULTS: can_id = CAN_CMD_CLEAR_FAULTS; break;
        case FRAME_TYPE_CMD_RESET:        can_id = CAN_CMD_RESET;        break;
        default:
            ESP_LOGW(TAG, "Unknown cmd type 0x%02X — discarding", frame_type);
            return;
    }

    /* Commands carry no payload — DLC 0 */
    MCP251XFD_CANMessage msg = {
        .MessageID    = can_id,
        .ControlFlags = MCP251XFD_CANFD_FRAME
                      | MCP251XFD_SWITCH_BITRATE
                      | MCP251XFD_NO_FIFO,
        .DLC          = MCP251XFD_DLC_0BYTE,
        .PayloadData  = NULL,
    };

    eERRORRESULT res = MCP251XFD_TransmitMessageToFIFO(&mcp251xfd_dev,
                                                        &msg,
                                                        CAN_TX_FIFO,
                                                        true);
    if (res != ERR_OK) {
        ESP_LOGW(TAG, "CAN TX failed for cmd 0x%03lX: err=%d", can_id, res);
    } else {
        ESP_LOGI(TAG, "CMD 0x%03lX sent on CAN bus", can_id);
    }
}

/* -------------------------------------------------------------------------- */
/* Main task                                                                   */
/* -------------------------------------------------------------------------- */
static void can_task(void *arg)
{
    ESP_LOGI(TAG, "CAN task running on core %d", xPortGetCoreID());

    uart_port_msg_t uart_msg;

    for (;;) {
        /* --- RX: wait for INT1 semaphore (frame arrived) or timeout --- */
        if (xSemaphoreTake(s_rx_sem, pdMS_TO_TICKS(10)) == pdTRUE) {
            drain_rx_fifo();
        }

        /* --- TX: drain UART RX queue for commands from NETESP --- */
        while (uart_port_receive(&uart_msg, 0)) {
            transmit_cmd(uart_msg.type);
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */
void can_task_start(void)
{
    /* --- Create RX semaphore --- */
    s_rx_sem = xSemaphoreCreateBinary();
    configASSERT(s_rx_sem);

    /* --- Configure INT1 GPIO as input with falling-edge ISR --- */
    gpio_config_t int_cfg = {
        .pin_bit_mask = (1ULL << MCP_PIN_INT1),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,   /* MCP open-drain output */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&int_cfg));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(MCP_PIN_INT1, int1_isr_handler, NULL));

    /* --- Initialise MCP2518FD --- */
    eERRORRESULT res = Init_MCP251XFD(&mcp251xfd_dev, &s_can_config);
    if (res != ERR_OK) {
        ESP_LOGE(TAG, "MCP2518FD init failed: %d — halting CAN task", res);
        return;
    }

    res = MCP251XFD_ConfigureFIFOList(&mcp251xfd_dev, s_fifo_list, 2u);
    if (res != ERR_OK) {
        ESP_LOGE(TAG, "FIFO config failed: %d", res);
        return;
    }

    res = MCP251XFD_ConfigureFilterList(&mcp251xfd_dev,
                                        MCP251XFD_D_NET_FILTER_DISABLE,
                                        s_filter_list, 1u);
    if (res != ERR_OK) {
        ESP_LOGE(TAG, "Filter config failed: %d", res);
        return;
    }

    res = MCP251XFD_RequestOperationMode(&mcp251xfd_dev,
                                         MCP251XFD_NORMAL_CANFD_MODE,
                                         true);
    if (res != ERR_OK) {
        ESP_LOGE(TAG, "CAN FD mode request failed: %d", res);
        return;
    }

    ESP_LOGI(TAG, "MCP2518FD online — CAN FD 500kbps/2Mbps, filter 0x100-0x1FF");

    xTaskCreatePinnedToCore(can_task, "can_task", CAN_TASK_STACK,
                            NULL, CAN_TASK_PRIORITY, NULL, 1);
}
