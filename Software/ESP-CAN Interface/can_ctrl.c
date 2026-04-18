#include "can_ctrl.h"
#include "mcp_port.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "can_ctrl";

/* =======================================================================
 * Pin configuration
 *
 * *** FLAG: confirm MCP_INT_GPIO against your ESP32 schematic ***
 *
 * This is the GPIO connected to the MCP2518FD INT pin (active-low).
 * It must be an input-capable GPIO. Triggers on falling edge.
 * ===================================================================== */
#define MCP_INT_GPIO    4       /* *** confirm *** */

/* =======================================================================
 * FreeRTOS objects
 *
 * The ISR is kept minimal — it only gives the semaphore. All SPI work
 * (reading interrupt register, draining FIFOs) happens in can_rx_task,
 * which is safe to call the ESP-IDF SPI driver from.
 * ===================================================================== */
#define CAN_RX_TASK_STACK   4096u
#define CAN_RX_TASK_PRIO    10u     /* above BLE stack tasks             */
#define CAN_RX_TASK_CORE    1       /* Core 1 — BLE stack owns Core 0    */

static SemaphoreHandle_t    s_rx_sem      = NULL;
static TaskHandle_t         s_rx_task     = NULL;
static can_rx_cb_t          s_rx_callback = NULL;
static bool                 s_initialised = false;

/* =======================================================================
 * Sequence counters — same slot assignments as STM32 side.
 * Slots 0–4 unused on ESP32 (those are STM32 TX types), but kept in the
 * same array so the slot definitions in can_ctrl.h stay valid on both.
 * ===================================================================== */
#define SEQ_SLOT_COUNT          8u
#define SEQ_SLOT_CMD_POWER      0u
#define SEQ_SLOT_CMD_TEMP       1u
#define SEQ_SLOT_ACK            5u
/* slots 2–4, 6–7 reserved for future command types */

static uint8_t s_seq[SEQ_SLOT_COUNT];

/* =======================================================================
 * FIFO configuration
 *
 * FIFO 1: TX — commands to STM32
 * FIFO 2: RX telemetry (sensor data, power state, FRAM — priority 6)
 * FIFO 3: RX emergency (fault alerts, temp alerts — priority 0)
 *
 * Emergency FIFO is kept separate so a burst of telemetry frames
 * cannot delay processing of a fault alert.
 * ===================================================================== */
#define CAN_FIFO_TX             MCP251XFD_FIFO1
#define CAN_FIFO_RX_TELEM       MCP251XFD_FIFO2
#define CAN_FIFO_RX_EMERG       MCP251XFD_FIFO3

static MCP251XFD_FIFO s_fifo_tx = {
    .Name       = CAN_FIFO_TX,
    .Size       = MCP251XFD_FIFO_4_MESSAGE_DEEP,
    .Payload    = MCP251XFD_PAYLOAD_8BYTE,    /* commands are small      */
    .Direction  = MCP251XFD_TRANSMIT_FIFO,
    .Priority   = MCP251XFD_MESSAGE_TX_PRIORITY16,
};

static MCP251XFD_FIFO s_fifo_rx_telem = {
    .Name       = CAN_FIFO_RX_TELEM,
    .Size       = MCP251XFD_FIFO_8_MESSAGE_DEEP,
    .Payload    = MCP251XFD_PAYLOAD_64BYTE,   /* sensor data needs 26B   */
    .Direction  = MCP251XFD_RECEIVE_FIFO,
    .InterruptFlags = MCP251XFD_FIFO_RECEIVE_FIFO_NOT_EMPTY_INT,
};

static MCP251XFD_FIFO s_fifo_rx_emerg = {
    .Name           = CAN_FIFO_RX_EMERG,
    .Size           = MCP251XFD_FIFO_4_MESSAGE_DEEP,
    .Payload        = MCP251XFD_PAYLOAD_16BYTE,
    .Direction      = MCP251XFD_RECEIVE_FIFO,
    .InterruptFlags = MCP251XFD_FIFO_RECEIVE_FIFO_NOT_EMPTY_INT,
};

/* =======================================================================
 * Filter configuration
 *
 * Filter 0: accept telemetry from STM32 (priority 6 band, src=0x010)
 * Filter 1: accept emergency frames from any node (priority 0 band)
 *
 * To add a new inbound message type:
 *   1. Add CAN_MSGID_xxx to can_ctrl.h
 *   2. Add a payload struct to can_ctrl.h
 *   3. Add or extend a filter here
 *   4. Handle the msg_id in dispatch_rx_frame() below
 * ===================================================================== */
#define CAN_FILTER_TELEM        0u
#define CAN_FILTER_EMERG        1u

/* =======================================================================
 * Internal helpers
 * ===================================================================== */

static void fill_header(can_frame_header_t *hdr, uint8_t seq_slot)
{
    hdr->proto_major = CAN_PROTO_VERSION_MAJOR;
    hdr->proto_minor = CAN_PROTO_VERSION_MINOR;
    hdr->src_node_id = (uint8_t)CAN_NODE_ID_ESP32;
    hdr->seq         = can_ctrl_next_seq(seq_slot);
}

static can_ctrl_err_t push_tx(uint32_t msg_id,
                               const uint8_t *payload,
                               uint8_t payload_len)
{
    MCP251XFD_CANMessage msg;
    memset(&msg, 0, sizeof(msg));

    msg.MessageID    = msg_id;
    msg.MessageSEQ   = 0u;
    msg.ControlFlags = MCP251XFD_CANFD_FRAME
                     | MCP251XFD_SWITCH_BITRATE
                     | MCP251XFD_EXTENDED_MESSAGE_ID;
    msg.DLC          = MCP251XFD_DLCToByte(payload_len, true);
    msg.PayloadData  = (uint8_t *)payload;

    eERRORRESULT res = MCP251XFD_TransmitMessageToFIFO(&mcp251xfd_dev,
                                                        &msg,
                                                        CAN_FIFO_TX,
                                                        true);
    if (res == ERR__BUFFER_FULL) return CAN_CTRL_ERR_BUSY;
    return (res == ERR_OK) ? CAN_CTRL_OK : CAN_CTRL_ERR_TX;
}

/* =======================================================================
 * RX dispatch
 *
 * Routes inbound frames to the BLE layer via the registered callback.
 * The callback receives the raw msg_id + payload — the BLE layer decides
 * what to do with it (forward to mobile app, update internal state, etc.)
 *
 * ACK/NACK frames are handled locally — they do not go up to BLE.
 *
 * To handle a new inbound type: add a case, do any local bookkeeping,
 * then pass up to s_rx_callback if the BLE layer needs to see it.
 * ===================================================================== */
static void dispatch_rx_frame(const MCP251XFD_CANMessage *msg)
{
    if (msg->PayloadData == NULL || msg->DLC == 0u) return;

    uint8_t byte_len = MCP251XFD_DLCToByte(msg->DLC, true);

    /* Validate common header before doing anything */
    if (byte_len < sizeof(can_frame_header_t)) return;
    const can_frame_header_t *hdr =
        (const can_frame_header_t *)msg->PayloadData;

    if (hdr->proto_major != CAN_PROTO_VERSION_MAJOR)
    {
        ESP_LOGW(TAG, "Dropping frame 0x%08lX: proto major %d != %d",
                 msg->MessageID, hdr->proto_major, CAN_PROTO_VERSION_MAJOR);
        return;
    }

    switch (msg->MessageID)
    {
        /* Telemetry and alert frames — forward straight to BLE layer.
           The BLE layer serialises these for the mobile app.           */
        case CAN_MSGID_SENSOR_DATA:
        case CAN_MSGID_POWER_STATE:
        case CAN_MSGID_FRAM_LOG:
        case CAN_MSGID_FAULT_ALERT:
        case CAN_MSGID_TEMP_ALERT:
            if (s_rx_callback != NULL)
            {
                s_rx_callback(msg->MessageID, msg->PayloadData, byte_len);
            }
            break;

        /* ACK/NACK — local bookkeeping only, not forwarded to BLE.
           TODO: if retry logic is added, match ack_seq to a pending TX
           record here and clear the retry timer.                       */
        case CAN_MSGID_ACK:
        case CAN_MSGID_NACK:
        {
            if (byte_len < sizeof(can_payload_ack_t)) break;
            const can_payload_ack_t *ack =
                (const can_payload_ack_t *)msg->PayloadData;
            if (msg->MessageID == CAN_MSGID_NACK)
            {
                ESP_LOGW(TAG, "NACK received for msg_id 0x%08lX seq=%d reason=%d",
                         ack->ack_msg_id, ack->ack_seq, ack->reason);
            }
            break;
        }

        default:
            ESP_LOGD(TAG, "Unhandled msg_id 0x%08lX — ignored", msg->MessageID);
            break;
    }
}

/* =======================================================================
 * GPIO ISR  (IRAM-resident — must not call non-IRAM functions)
 *
 * Gives the binary semaphore so can_rx_task wakes and drains the FIFO.
 * All SPI work happens in task context, not here.
 * ===================================================================== */
static void IRAM_ATTR can_gpio_isr(void *arg)
{
    (void)arg;
    BaseType_t higher_prio_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_rx_sem, &higher_prio_woken);
    portYIELD_FROM_ISR(higher_prio_woken);
}

/* =======================================================================
 * CAN RX task  (Core 1)
 *
 * Blocks on the semaphore. When the ISR fires it wakes, reads the MCP2518FD
 * interrupt register, then drains both FIFOs before blocking again.
 * ===================================================================== */
static void can_rx_task(void *arg)
{
    (void)arg;
    uint8_t rx_buf[64];

    while (true)
    {
        /* Block until INT fires */
        xSemaphoreTake(s_rx_sem, portMAX_DELAY);

        if (!s_initialised) continue;

        setMCP251XFD_InterruptEvents int_flags;
        eERRORRESULT res = MCP251XFD_GetInterruptEvents(&mcp251xfd_dev,
                                                         &int_flags);
        if (res != ERR_OK) continue;

        /* --- Drain FIFOs: emergency first, then telemetry --- */
        if (int_flags & MCP251XFD_INT_RX_EVENT)
        {
            eMCP251XFD_FIFO fifo_list[] = { CAN_FIFO_RX_EMERG,
                                             CAN_FIFO_RX_TELEM };

            for (uint8_t f = 0; f < 2u; f++)
            {
                while (true)
                {
                    setMCP251XFD_FIFOstatus fifo_st;
                    res = MCP251XFD_GetFIFOStatus(&mcp251xfd_dev,
                                                   fifo_list[f], &fifo_st);
                    if (res != ERR_OK) break;
                    if (!(fifo_st & MCP251XFD_RX_FIFO_NOT_EMPTY)) break;

                    MCP251XFD_CANMessage rx_msg;
                    memset(rx_buf, 0, sizeof(rx_buf));
                    rx_msg.PayloadData = rx_buf;

                    res = MCP251XFD_ReceiveMessageFromFIFO(&mcp251xfd_dev,
                                                            &rx_msg,
                                                            MCP251XFD_PAYLOAD_64BYTE,
                                                            NULL,
                                                            fifo_list[f]);
                    if (res != ERR_OK) break;

                    dispatch_rx_frame(&rx_msg);
                }
            }
        }

        /* --- Bus error --- */
        if (int_flags & MCP251XFD_INT_BUS_ERROR_EVENT)
        {
            ESP_LOGE(TAG, "CAN bus error");
            /* TODO: increment error counter, escalate if threshold hit */
            MCP251XFD_ClearInterruptEvents(&mcp251xfd_dev,
                                            MCP251XFD_INT_BUS_ERROR_EVENT);
        }

        /* --- TX retry exhausted --- */
        if (int_flags & MCP251XFD_INT_TX_ATTEMPTS_EVENT)
        {
            ESP_LOGW(TAG, "TX attempts exhausted — possible bus-off");
            MCP251XFD_ClearInterruptEvents(&mcp251xfd_dev,
                                            MCP251XFD_INT_TX_ATTEMPTS_EVENT);
        }
    }
}

/* =======================================================================
 * can_ctrl_init
 * ===================================================================== */
can_ctrl_err_t can_ctrl_init(void)
{
    memset(s_seq, 0, sizeof(s_seq));
    s_rx_callback = NULL;
    s_initialised = false;

    /* --- MCP251XFD device init --- */
    MCP251XFD_Config dev_cfg;
    memset(&dev_cfg, 0, sizeof(dev_cfg));

    dev_cfg.XtalFreq          = 0u;           /* using oscillator, not XTAL */
    dev_cfg.OscFreq           = 40000000u;    /* 40MHz oscillator            */
    dev_cfg.SysclkConfig      = MCP251XFD_SYSCLK_IS_CLKIN;
    dev_cfg.ClkoPinConfig     = MCP251XFD_CLKO_SOF;
    dev_cfg.SYSCLK_Result     = NULL;
    dev_cfg.NominalBitrate    = CAN_CTRL_ARB_BITRATE_HZ;
    dev_cfg.DataBitrate       = CAN_CTRL_DATA_BITRATE_HZ;
    dev_cfg.BitTimeStats      = NULL;
    dev_cfg.Bandwidth         = MCP251XFD_DELAY_2BIT_TIMES;
    dev_cfg.ControlFlags      = MCP251XFD_CANFD_BITRATE_SWITCHING_ENABLE;
    dev_cfg.GPIO0PinMode      = MCP251XFD_PIN_AS_INT0_TX;
    dev_cfg.GPIO1PinMode      = MCP251XFD_PIN_AS_INT1_RX;
    dev_cfg.INTsOutMode       = MCP251XFD_PINS_PUSHPULL_OUT;
    dev_cfg.TXCANOutMode      = MCP251XFD_PINS_PUSHPULL_OUT;
    dev_cfg.SysInterruptFlags = MCP251XFD_INT_RX_EVENT
                              | MCP251XFD_INT_TX_ATTEMPTS_EVENT
                              | MCP251XFD_INT_BUS_ERROR_EVENT;

    eERRORRESULT res = Init_MCP251XFD(&mcp251xfd_dev, &dev_cfg);
    if (res != ERR_OK)
    {
        ESP_LOGE(TAG, "Init_MCP251XFD failed: %d", res);
        return CAN_CTRL_ERR_INIT;
    }

    /* --- FIFO configuration --- */
    res = MCP251XFD_ConfigureFIFO(&mcp251xfd_dev, &s_fifo_tx);
    if (res != ERR_OK) return CAN_CTRL_ERR_INIT;

    res = MCP251XFD_ConfigureFIFO(&mcp251xfd_dev, &s_fifo_rx_telem);
    if (res != ERR_OK) return CAN_CTRL_ERR_INIT;

    res = MCP251XFD_ConfigureFIFO(&mcp251xfd_dev, &s_fifo_rx_emerg);
    if (res != ERR_OK) return CAN_CTRL_ERR_INIT;

    /* --- Filter 0: telemetry from STM32 (priority 6 band) --- */
    MCP251XFD_Filter filt_telem = {
        .Filter         = CAN_FILTER_TELEM,
        .EnableFilter   = true,
        .Match          = MCP251XFD_MATCH_ONLY_EID,
        .AcceptanceID   = CAN_MSGID_SENSOR_DATA,
        .AcceptanceMask = 0x3000FF00u,
        .PointTo        = CAN_FIFO_RX_TELEM,
    };
    res = MCP251XFD_ConfigureFilter(&mcp251xfd_dev, &filt_telem);
    if (res != ERR_OK) return CAN_CTRL_ERR_FILTER;

    /* --- Filter 1: emergency frames from any node (priority 0) --- */
    MCP251XFD_Filter filt_emerg = {
        .Filter         = CAN_FILTER_EMERG,
        .EnableFilter   = true,
        .Match          = MCP251XFD_MATCH_ONLY_EID,
        .AcceptanceID   = CAN_MSGID_FAULT_ALERT,
        .AcceptanceMask = 0x00FF0000u,
        .FIFO           = CAN_FIFO_RX_EMERG,
    };
    res = MCP251XFD_ConfigureFilter(&mcp251xfd_dev, &filt_emerg);
    if (res != ERR_OK) return CAN_CTRL_ERR_FILTER;

    /* --- GPIO interrupt for MCP2518FD INT pin --- */
    gpio_config_t int_cfg = {
        .pin_bit_mask = (1ULL << MCP_INT_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,  /* INT is active-low o/d   */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,   /* fires on falling edge   */
    };
    if (gpio_config(&int_cfg) != ESP_OK)          return CAN_CTRL_ERR_INIT;
    if (gpio_install_isr_service(0) != ESP_OK)    return CAN_CTRL_ERR_INIT;
    if (gpio_isr_handler_add(MCP_INT_GPIO, can_gpio_isr, NULL) != ESP_OK)
        return CAN_CTRL_ERR_INIT;

    /* --- FreeRTOS semaphore and RX task --- */
    s_rx_sem = xSemaphoreCreateBinary();
    if (s_rx_sem == NULL) return CAN_CTRL_ERR_INIT;

    BaseType_t task_ok = xTaskCreatePinnedToCore(
        can_rx_task,
        "can_rx",
        CAN_RX_TASK_STACK,
        NULL,
        CAN_RX_TASK_PRIO,
        &s_rx_task,
        CAN_RX_TASK_CORE
    );
    if (task_ok != pdPASS) return CAN_CTRL_ERR_INIT;

    s_initialised = true;
    ESP_LOGI(TAG, "CAN FD ready — arb %u kbit/s, data %u kbit/s",
             CAN_CTRL_ARB_BITRATE_HZ / 1000u,
             CAN_CTRL_DATA_BITRATE_HZ / 1000u);

    return CAN_CTRL_OK;
}

/* =======================================================================
 * can_ctrl_send  (raw)
 * ===================================================================== */
can_ctrl_err_t can_ctrl_send(uint32_t msg_id,
                              const uint8_t *payload,
                              uint8_t payload_len)
{
    if (!s_initialised)     return CAN_CTRL_ERR_INIT;
    if (payload == NULL)    return CAN_CTRL_ERR_PARAM;
    if (payload_len > 64u)  return CAN_CTRL_ERR_PARAM;

    return push_tx(msg_id, payload, payload_len);
}

/* =======================================================================
 * can_ctrl_register_rx_callback
 * ===================================================================== */
void can_ctrl_register_rx_callback(can_rx_cb_t cb)
{
    s_rx_callback = cb;
}

/* =======================================================================
 * can_ctrl_next_seq
 * ===================================================================== */
uint8_t can_ctrl_next_seq(uint8_t slot)
{
    if (slot >= SEQ_SLOT_COUNT) return 0u;
    return s_seq[slot]++;
}

/* =======================================================================
 * ESP32 TX helpers — send commands to STM32
 * ===================================================================== */

can_ctrl_err_t can_ctrl_send_cmd_power(const can_payload_cmd_power_t *p)
{
    if (p == NULL) return CAN_CTRL_ERR_PARAM;
    return push_tx(CAN_MSGID_CMD_POWER,
                   (const uint8_t *)p,
                   sizeof(can_payload_cmd_power_t));
}

can_ctrl_err_t can_ctrl_send_cmd_temp_thresh(const can_payload_cmd_temp_thresh_t *p)
{
    if (p == NULL) return CAN_CTRL_ERR_PARAM;
    return push_tx(CAN_MSGID_CMD_TEMP_THRESH,
                   (const uint8_t *)p,
                   sizeof(can_payload_cmd_temp_thresh_t));
}

can_ctrl_err_t can_ctrl_send_cmd_temp_req(const can_payload_cmd_temp_req_t *p)
{
    if (p == NULL) return CAN_CTRL_ERR_PARAM;
    return push_tx(CAN_MSGID_CMD_TEMP_REQ,
                   (const uint8_t *)p,
                   sizeof(can_payload_cmd_temp_req_t));
}

can_ctrl_err_t can_ctrl_send_cmd_alert_ack(const can_payload_cmd_alert_ack_t *p)
{
    if (p == NULL) return CAN_CTRL_ERR_PARAM;
    return push_tx(CAN_MSGID_CMD_ALERT_ACK,
                   (const uint8_t *)p,
                   sizeof(can_payload_cmd_alert_ack_t));
}

/* =======================================================================
 * Bidirectional — ACK / NACK
 * ===================================================================== */

can_ctrl_err_t can_ctrl_send_ack(uint32_t acked_msg_id, uint8_t acked_seq)
{
    can_payload_ack_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    fill_header(&pkt.hdr, SEQ_SLOT_ACK);
    pkt.ack_msg_id = acked_msg_id;
    pkt.ack_seq    = acked_seq;
    pkt.reason     = 0u;
    return push_tx(CAN_MSGID_ACK,
                   (const uint8_t *)&pkt,
                   sizeof(pkt));
}

can_ctrl_err_t can_ctrl_send_nack(uint32_t nacked_msg_id,
                                   uint8_t  nacked_seq,
                                   uint8_t  reason)
{
    can_payload_ack_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    fill_header(&pkt.hdr, SEQ_SLOT_ACK);
    pkt.ack_msg_id = nacked_msg_id;
    pkt.ack_seq    = nacked_seq;
    pkt.reason     = reason;
    return push_tx(CAN_MSGID_NACK,
                   (const uint8_t *)&pkt,
                   sizeof(pkt));
}

/* =======================================================================
 * STM32 TX helpers — not used on ESP32, stubs satisfy the shared header.
 * If sharing a single compiled library becomes necessary these can be
 * removed with a target guard (#ifndef ESP_PLATFORM).
 * ===================================================================== */
can_ctrl_err_t can_ctrl_send_sensor_data(const can_payload_sensor_data_t *p)
    { (void)p; return CAN_CTRL_ERR_PARAM; }

can_ctrl_err_t can_ctrl_send_power_state(const can_payload_power_state_t *p)
    { (void)p; return CAN_CTRL_ERR_PARAM; }

can_ctrl_err_t can_ctrl_send_fault_alert(const can_payload_fault_alert_t *p)
    { (void)p; return CAN_CTRL_ERR_PARAM; }

can_ctrl_err_t can_ctrl_send_temp_alert(const can_payload_temp_alert_t *p)
    { (void)p; return CAN_CTRL_ERR_PARAM; }

can_ctrl_err_t can_ctrl_send_fram_chunk(const can_payload_fram_log_t *p)
    { (void)p; return CAN_CTRL_ERR_PARAM; }
