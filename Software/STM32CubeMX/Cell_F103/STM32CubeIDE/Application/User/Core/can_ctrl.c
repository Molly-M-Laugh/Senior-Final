#include "can_ctrl.h"
#include "mcp_port.h"
#include "power_ctrl.h"
#include "gpio.h"
#include <string.h>

/* =======================================================================
 * Internal state
 * ===================================================================== */

static can_rx_cb_t  s_rx_callback  = NULL;
static bool         s_initialised  = false;

/* Per-message-type sequence counters. Indexed by the slot argument
   passed to can_ctrl_next_seq(). 8 slots covers all current types
   with room to grow — add more if needed without changing the API.    */
#define SEQ_SLOT_COUNT  8u
static uint8_t s_seq[SEQ_SLOT_COUNT];

/* Sequence slot assignments — keep these stable across versions.
   Adding a new message type: use the next free slot index.            */
#define SEQ_SLOT_SENSOR_DATA    0u
#define SEQ_SLOT_POWER_STATE    1u
#define SEQ_SLOT_FAULT_ALERT    2u
#define SEQ_SLOT_TEMP_ALERT     3u
#define SEQ_SLOT_FRAM_LOG       4u
#define SEQ_SLOT_ACK            5u
/* slots 6–7 reserved for future message types */

/* =======================================================================
 * MCP2518FD configuration objects
 *
 * Bit timing values for 40MHz oscillator:
 *
 * Arbitration 500 kbit/s:
 *   TQ = 1 / (40MHz / 4) = 100ns  → 10 TQ per bit
 *   SyncSeg=1, PropSeg+PhaseSeg1=6, PhaseSeg2=3, SJW=3
 *
 * Data 2 Mbit/s:
 *   TQ = 1 / (40MHz / 2) = 50ns  → 10 TQ per bit
 *   SyncSeg=1, PropSeg+PhaseSeg1=5, PhaseSeg2=4, SJW=4
 *
 * *** These values should be verified with a bit timing calculator
 *     (e.g. Microchip's MCP2518FD Excel tool) before production. ***
 * ===================================================================== */
static const MCP251XFD_BitTimeConfig s_arb_timing = {
    .NominalBitrate = CAN_CTRL_ARB_BITRATE_HZ,
    .NBRP  = 4u,    /* prescaler: 40MHz / 4 = 10MHz TQ clock */
    .NTSEG1 = 6u,   /* PropSeg + PhaseSeg1                   */
    .NTSEG2 = 3u,   /* PhaseSeg2                             */
    .NSJW   = 3u,   /* sync jump width                       */
};

static const MCP251XFD_BitTimeConfig s_data_timing = {
    .DataBitrate = CAN_CTRL_DATA_BITRATE_HZ,
    .DBRP  = 2u,    /* prescaler: 40MHz / 2 = 20MHz TQ clock */
    .DTSEG1 = 5u,
    .DTSEG2 = 4u,
    .DSJW   = 4u,
};

/* =======================================================================
 * FIFO configuration
 *
 * FIFO 1: TX — depth 4, CAN FD, high priority
 * FIFO 2: RX command frames (from ESP32, priority 2 band)
 * FIFO 3: RX emergency frames (faults/alerts, priority 0 band)
 *
 * Keeping RX FIFOs separate by priority means a burst of telemetry
 * ACKs can't crowd out an inbound emergency command.
 * ===================================================================== */
#define CAN_FIFO_TX         MCP251XFD_FIFO1
#define CAN_FIFO_RX_CMD     MCP251XFD_FIFO2
#define CAN_FIFO_RX_EMERG   MCP251XFD_FIFO3

static MCP251XFD_FIFO s_fifo_tx = {
    .Name       = CAN_FIFO_TX,
    .Size       = MCP251XFD_FIFO_4_MESSAGE_DEEP,
    .Payload    = MCP251XFD_PAYLOAD_64BYTE,
    .Direction  = MCP251XFD_TRANSMIT_FIFO,
    .Priority   = MCP251XFD_MESSAGE_TX_PRIORITY16,
};

static MCP251XFD_FIFO s_fifo_rx_cmd = {
    .Name       = CAN_FIFO_RX_CMD,
    .Size       = MCP251XFD_FIFO_8_MESSAGE_DEEP,
    .Payload    = MCP251XFD_PAYLOAD_8BYTE,    /* commands are small     */
    .Direction  = MCP251XFD_RECEIVE_FIFO,
    .InterruptEnable = MCP251XFD_FIFO_RX_INTERRUPT,
};

static MCP251XFD_FIFO s_fifo_rx_emerg = {
    .Name       = CAN_FIFO_RX_EMERG,
    .Size       = MCP251XFD_FIFO_4_MESSAGE_DEEP,
    .Payload    = MCP251XFD_PAYLOAD_8BYTE,
    .Direction  = MCP251XFD_RECEIVE_FIFO,
    .InterruptEnable = MCP251XFD_FIFO_RX_INTERRUPT,
};

/* =======================================================================
 * Filter configuration
 *
 * Filter 0: accept all CMD_* messages addressed to this node
 * Filter 1: accept FAULT_ALERT / TEMP_ALERT from any node (broadcast)
 *
 * To add a new message type that needs receiving:
 *   - Add a filter entry here and configure it in can_ctrl_init().
 *   - Route it to an appropriate FIFO.
 *   - Handle the msg_id in the rx dispatch in can_ctrl_irq_handler().
 * ===================================================================== */
#define CAN_FILTER_CMD      0u
#define CAN_FILTER_EMERG    1u

/* =======================================================================
 * Internal helpers
 * ===================================================================== */

/* Populate the common header for any outbound frame */
static void fill_header(can_frame_header_t *hdr, uint8_t seq_slot)
{
    hdr->proto_major = CAN_PROTO_VERSION_MAJOR;
    hdr->proto_minor = CAN_PROTO_VERSION_MINOR;
    hdr->src_node_id = (uint8_t)CAN_NODE_ID_STM32;
    hdr->seq         = can_ctrl_next_seq(seq_slot);
}

/* Thin wrapper: push a raw buffer into the TX FIFO */
static can_ctrl_err_t push_tx(uint32_t msg_id,
                               const uint8_t *payload,
                               uint8_t payload_len)
{
    MCP251XFD_CANMessage msg;
    memset(&msg, 0, sizeof(msg));

    msg.MessageID   = msg_id;
    msg.MessageSEQ  = 0u;
    msg.ControlFlags = MCP251XFD_CANFD_FRAME
                     | MCP251XFD_SWITCH_BITRATE    /* BRS: use data bitrate */
                     | MCP251XFD_EXT_MESSAGE_ID;   /* 29-bit extended ID    */
    msg.DLC         = MCP251XFD_DLCToByte(payload_len, true);  /* FD DLC   */
    msg.PayloadData = (uint8_t *)payload;           /* driver does not modify*/

    eERRORRESULT res = MCP251XFD_TransmitMessageToFIFO(&mcp251xfd_dev,
                                                        &msg,
                                                        CAN_FIFO_TX,
                                                        true);  /* flush now */
    if (res == ERR__TXFIFO_FULL) return CAN_CTRL_ERR_BUSY;
    return (res == ERR_OK) ? CAN_CTRL_OK : CAN_CTRL_ERR_TX;
}

/* =======================================================================
 * RX dispatch — called from can_ctrl_irq_handler()
 *
 * To handle a new inbound message type:
 *   1. Add its CAN_MSGID_xxx to can_ctrl.h
 *   2. Define its payload struct in can_ctrl.h
 *   3. Add a case here and call into the owning module
 * ===================================================================== */
static void dispatch_rx_frame(const MCP251XFD_CANMessage *msg)
{
    if (msg->PayloadData == NULL || msg->DLC == 0u) return;

    /* Notify the registered networking callback first — the integrator
       may want to observe all frames before module-level dispatch.     */
    if (s_rx_callback != NULL)
    {
        uint8_t byte_len = MCP251XFD_DLCToByte(msg->DLC, true);
        s_rx_callback(msg->MessageID, msg->PayloadData, byte_len);
    }

    /* Module-level dispatch */
    switch (msg->MessageID)
    {
        case CAN_MSGID_CMD_POWER:
        {
            if (msg->DLC < MCP251XFD_DLCToByte(sizeof(can_payload_cmd_power_t), true))
                break;   /* too short — malformed, ignore */

            const can_payload_cmd_power_t *cmd =
                (const can_payload_cmd_power_t *)msg->PayloadData;

            if (cmd->hdr.proto_major != CAN_PROTO_VERSION_MAJOR) break;

            /* Apply each bit in rail_mask */
            if (cmd->rail_mask & 0x01u)
            {
                if (cmd->enable) power_enable_5v0();
                else             power_disable_5v0();
            }
            if (cmd->rail_mask & 0x02u)
            {
                if (cmd->enable) power_enable_3v3();
                else             power_disable_3v3();
            }
            if (cmd->rail_mask & 0x04u)
            {
                if (cmd->enable) power_enable_1v8();
                else             power_disable_1v8();
            }

            can_ctrl_send_ack(msg->MessageID, cmd->hdr.seq);
            break;
        }

        case CAN_MSGID_CMD_TEMP_THRESH:
        {
            if (msg->DLC < MCP251XFD_DLCToByte(sizeof(can_payload_cmd_temp_thresh_t), true))
                break;

            const can_payload_cmd_temp_thresh_t *cmd =
                (const can_payload_cmd_temp_thresh_t *)msg->PayloadData;

            if (cmd->hdr.proto_major != CAN_PROTO_VERSION_MAJOR) break;

            /* TODO: route to tmp75b_set_threshold(cmd->sensor_idx,
                                                    cmd->limit_type,
                                                    cmd->threshold_raw)
               once tmp75b driver is written.                           */
            can_ctrl_send_ack(msg->MessageID, cmd->hdr.seq);
            break;
        }

        case CAN_MSGID_CMD_TEMP_REQ:
        {
            if (msg->DLC < MCP251XFD_DLCToByte(sizeof(can_payload_cmd_temp_req_t), true))
                break;

            const can_payload_cmd_temp_req_t *cmd =
                (const can_payload_cmd_temp_req_t *)msg->PayloadData;

            if (cmd->hdr.proto_major != CAN_PROTO_VERSION_MAJOR) break;

            /* TODO: trigger an immediate sensor read and call
               can_ctrl_send_sensor_data() once sensor drivers exist.  */
            can_ctrl_send_ack(msg->MessageID, cmd->hdr.seq);
            break;
        }

        case CAN_MSGID_CMD_ALERT_ACK:
        {
            if (msg->DLC < MCP251XFD_DLCToByte(sizeof(can_payload_cmd_alert_ack_t), true))
                break;

            const can_payload_cmd_alert_ack_t *cmd =
                (const can_payload_cmd_alert_ack_t *)msg->PayloadData;

            if (cmd->hdr.proto_major != CAN_PROTO_VERSION_MAJOR) break;

            /* TODO: route to tmp75b_clear_alert(cmd->sensor_idx)
               once tmp75b driver exposes alert clear.                  */
            can_ctrl_send_ack(msg->MessageID, cmd->hdr.seq);
            break;
        }

        default:
            /* Unknown or unhandled message ID — silently drop.
               The registered rx_callback has already seen it above.   */
            break;
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

    dev_cfg.NominalBitrate  = CAN_CTRL_ARB_BITRATE_HZ;
    dev_cfg.DataBitrate     = CAN_CTRL_DATA_BITRATE_HZ;
    dev_cfg.OscFreq         = 40000000u;    /* 40MHz XTAL                */
    dev_cfg.BitTimeConfig   = (MCP251XFD_BitTimeConfig *)&s_arb_timing;
    dev_cfg.DataBitTimeConfig = (MCP251XFD_BitTimeConfig *)&s_data_timing;
    dev_cfg.ControllerMode  = MCP251XFD_NORMAL_CANFD_MODE;
    /* Enable RX and TX interrupts at the controller level              */
    dev_cfg.Interrupts      = MCP251XFD_INT_RX_EVENT
                            | MCP251XFD_INT_TX_ATTEMPTS_EXHAUSTED_EVENT
                            | MCP251XFD_INT_BUS_ERROR_EVENT;

    eERRORRESULT res = MCP251XFD_Init(&mcp251xfd_dev, &dev_cfg);
    if (res != ERR_OK) return CAN_CTRL_ERR_INIT;

    /* --- FIFO configuration --- */
    res = MCP251XFD_ConfigureFIFO(&mcp251xfd_dev, &s_fifo_tx);
    if (res != ERR_OK) return CAN_CTRL_ERR_INIT;

    res = MCP251XFD_ConfigureFIFO(&mcp251xfd_dev, &s_fifo_rx_cmd);
    if (res != ERR_OK) return CAN_CTRL_ERR_INIT;

    res = MCP251XFD_ConfigureFIFO(&mcp251xfd_dev, &s_fifo_rx_emerg);
    if (res != ERR_OK) return CAN_CTRL_ERR_INIT;

    /* --- Filter 0: accept CMD_* frames addressed to this node ---
       Match on priority-2 + node-destination band.
       Mask allows any msg_type within that band.                       */
    MCP251XFD_Filter filt_cmd = {
        .Filter     = CAN_FILTER_CMD,
        .EnableFilter = true,
        .Match      = MCP251XFD_MATCH_ONLY_EID,
        .AcceptanceID   = CAN_MSGID_CMD_POWER,        /* representative   */
        .AcceptanceMask = 0x1000FF00u,                /* match prio+src band */
        .FIFO       = CAN_FIFO_RX_CMD,
    };
    res = MCP251XFD_ConfigureFilter(&mcp251xfd_dev, &filt_cmd);
    if (res != ERR_OK) return CAN_CTRL_ERR_FILTER;

    /* --- Filter 1: accept emergency broadcast frames (prio 0 band) -- */
    MCP251XFD_Filter filt_emerg = {
        .Filter     = CAN_FILTER_EMERG,
        .EnableFilter = true,
        .Match      = MCP251XFD_MATCH_ONLY_EID,
        .AcceptanceID   = CAN_MSGID_FAULT_ALERT,
        .AcceptanceMask = 0x00FF0000u,
        .FIFO       = CAN_FIFO_RX_EMERG,
    };
    res = MCP251XFD_ConfigureFilter(&mcp251xfd_dev, &filt_emerg);
    if (res != ERR_OK) return CAN_CTRL_ERR_FILTER;

    s_initialised = true;
    return CAN_CTRL_OK;
}

/* =======================================================================
 * can_ctrl_irq_handler
 *
 * Called from HAL_GPIO_EXTI_Callback for CAN_ALRT1, CAN_ALRT2, CAN_FLT.
 * Reads the MCP2518FD interrupt register to determine what fired, then
 * drains any waiting RX frames.
 * ===================================================================== */
void can_ctrl_irq_handler(uint16_t gpio_pin)
{
    (void)gpio_pin;   /* all three CAN pins route here — check INT reg  */

    if (!s_initialised) return;

    MCP251XFD_InterruptEvents int_flags;
    eERRORRESULT res = MCP251XFD_GetInterruptEvents(&mcp251xfd_dev, &int_flags);
    if (res != ERR_OK) return;

    /* --- RX: drain both FIFOs --- */
    if (int_flags & (MCP251XFD_INT_RX_EVENT))
    {
        uint8_t rx_buf[64];
        MCP251XFD_CANMessage rx_msg;
        rx_msg.PayloadData = rx_buf;

        /* Emergency FIFO first — higher priority */
        MCP251XFD_FIFOStatus fifo_st;
        MCP251XFD_FIFOx fifo_list[] = { CAN_FIFO_RX_EMERG, CAN_FIFO_RX_CMD };

        for (uint8_t f = 0; f < 2u; f++)
        {
            while (true)
            {
                res = MCP251XFD_GetFIFOStatus(&mcp251xfd_dev, fifo_list[f], &fifo_st);
                if (res != ERR_OK) break;
                if (!(fifo_st & MCP251XFD_RX_FIFO_NOT_EMPTY)) break;

                memset(rx_buf, 0, sizeof(rx_buf));
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

    /* --- Bus error: log and clear --- */
    if (int_flags & MCP251XFD_INT_BUS_ERROR_EVENT)
    {
        /* TODO: increment a bus error counter, log to FRAM, or
           escalate to a fault alert frame if error count threshold
           is exceeded.                                                 */
        MCP251XFD_ClearInterruptEvents(&mcp251xfd_dev,
                                        MCP251XFD_INT_BUS_ERROR_EVENT);
    }

    /* --- TX exhausted (all retries failed) --- */
    if (int_flags & MCP251XFD_INT_TX_ATTEMPTS_EXHAUSTED_EVENT)
    {
        /* TODO: flag to application layer — may indicate bus off.     */
        MCP251XFD_ClearInterruptEvents(&mcp251xfd_dev,
                                        MCP251XFD_INT_TX_ATTEMPTS_EXHAUSTED_EVENT);
    }
}

/* =======================================================================
 * can_ctrl_send  (raw)
 * ===================================================================== */
can_ctrl_err_t can_ctrl_send(uint32_t msg_id,
                              const uint8_t *payload,
                              uint8_t payload_len)
{
    if (!s_initialised)         return CAN_CTRL_ERR_INIT;
    if (payload == NULL)        return CAN_CTRL_ERR_PARAM;
    if (payload_len > 64u)      return CAN_CTRL_ERR_PARAM;

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
    return s_seq[slot]++;   /* wraps at 255 → 0 naturally */
}

/* =======================================================================
 * Convenience TX helpers
 * ===================================================================== */

can_ctrl_err_t can_ctrl_send_sensor_data(const can_payload_sensor_data_t *p)
{
    if (p == NULL) return CAN_CTRL_ERR_PARAM;
    /* Header is pre-filled by caller via fill_header() — see usage note
       in can_ctrl.h. The convenience helpers below show the pattern.   */
    return push_tx(CAN_MSGID_SENSOR_DATA,
                   (const uint8_t *)p,
                   sizeof(can_payload_sensor_data_t));
}

can_ctrl_err_t can_ctrl_send_power_state(const can_payload_power_state_t *p)
{
    if (p == NULL) return CAN_CTRL_ERR_PARAM;
    return push_tx(CAN_MSGID_POWER_STATE,
                   (const uint8_t *)p,
                   sizeof(can_payload_power_state_t));
}

can_ctrl_err_t can_ctrl_send_fault_alert(const can_payload_fault_alert_t *p)
{
    if (p == NULL) return CAN_CTRL_ERR_PARAM;
    return push_tx(CAN_MSGID_FAULT_ALERT,
                   (const uint8_t *)p,
                   sizeof(can_payload_fault_alert_t));
}

can_ctrl_err_t can_ctrl_send_temp_alert(const can_payload_temp_alert_t *p)
{
    if (p == NULL) return CAN_CTRL_ERR_PARAM;
    return push_tx(CAN_MSGID_TEMP_ALERT,
                   (const uint8_t *)p,
                   sizeof(can_payload_temp_alert_t));
}

can_ctrl_err_t can_ctrl_send_fram_chunk(const can_payload_fram_log_t *p)
{
    if (p == NULL) return CAN_CTRL_ERR_PARAM;
    return push_tx(CAN_MSGID_FRAM_LOG,
                   (const uint8_t *)p,
                   sizeof(can_payload_fram_log_t));
}

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
