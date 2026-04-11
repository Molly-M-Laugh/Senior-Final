#include "can_ctrl.h"
#include "mcp_port.h"

#define FAULT_FLAG_ALRT1    (1u << 0)
#define FAULT_FLAG_ALRT2    (1u << 1)
#define FAULT_FLAG_FLT      (1u << 2)
#define CAN_OSC_FREQ_HZ    40000000u
static volatile uint8_t can_fault_flags = 0;

/* -----------------------------------------------------------------------
 * Static config structs — defined once, passed into driver at init
 * --------------------------------------------------------------------- */
static MCP251XFD_Config can_config = {
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

    .GPIO0PinMode  = MCP251XFD_PIN_AS_INT0_TX,
    .GPIO1PinMode  = MCP251XFD_PIN_AS_INT1_RX,
    .INTsOutMode   = MCP251XFD_PINS_OPENDRAIN_OUT,
    .TXCANOutMode  = MCP251XFD_PINS_PUSHPULL_OUT,

    .SysInterruptFlags = MCP251XFD_INT_BUS_ERROR_EVENT
                       | MCP251XFD_INT_RX_INVALID_MESSAGE_EVENT,
};

static MCP251XFD_FIFO fifo_list[2] = {
    {
        /* FIFO1 — TX */
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
        /* FIFO2 — RX */
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

/* Accept all messages into RX FIFO — tighten this once message IDs
   are finalized for the full network */
static MCP251XFD_Filter rx_filter = {
    .Filter        = MCP251XFD_FILTER0,
    .EnableFilter  = true,
    .Match         = MCP251XFD_MATCH_SID_EID,
    .PointTo       = CAN_RX_FIFO,
    .AcceptanceID  = MCP251XFD_ACCEPT_ALL_MESSAGES,
    .AcceptanceMask = MCP251XFD_ACCEPT_ALL_MESSAGES,
    .ExtendedID    = false,
};

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

can_ctrl_status_t can_ctrl_init(void)
{
    eERRORRESULT err;

    /* mcp_port_init_device_struct() must be called before this */
    err = Init_MCP251XFD(&mcp251xfd_dev, &can_config);
    if (err != ERR_OK) return CAN_CTRL_ERR_INIT;

    err = MCP251XFD_ConfigureFIFOList(&mcp251xfd_dev, fifo_list, 2);
    if (err != ERR_OK) return CAN_CTRL_ERR_FIFO;

    err = MCP251XFD_ConfigureFilter(&mcp251xfd_dev, &rx_filter);
    if (err != ERR_OK) return CAN_CTRL_ERR_FILTER;

    /* Init_MCP251XFD leaves the device in Normal mode — ready to go */
    return CAN_CTRL_OK;
}

can_ctrl_status_t can_ctrl_transmit(const can_telemetry_t *data)
{
    MCP251XFD_CANMessage msg = {
        .MessageID   = CAN_TELEMETRY_MSG_ID,
        .MessageSEQ  = 0,
        .ControlFlags = MCP251XFD_CANFD_FRAME
                      | MCP251XFD_SWITCH_BITRATE,   /* CAN FD with BRS */
        .DLC         = MCP251XFD_DLC_32BYTE,         /* 32 bytes of data */
        .PayloadData = (uint8_t *)data,
    };

    eERRORRESULT err = MCP251XFD_TransmitMessageToFIFO(&mcp251xfd_dev,
                                                         &msg,
                                                         CAN_TX_FIFO,
                                                         true); /* flush now */
    return (err == ERR_OK) ? CAN_CTRL_OK : CAN_CTRL_ERR_TX;
}

can_ctrl_status_t can_ctrl_receive(MCP251XFD_CANMessage *msg_out)
{
    /* Check if anything is waiting first */
    setMCP251XFD_FIFOstatus fifo_status;
    eERRORRESULT err = MCP251XFD_GetFIFOStatus(&mcp251xfd_dev,
                                                 CAN_RX_FIFO,
                                                 &fifo_status);
    if (err != ERR_OK)                              return CAN_CTRL_ERR_RX;
    if (!(fifo_status & MCP251XFD_RX_FIFO_NOT_EMPTY)) return CAN_CTRL_ERR_RX;

    static uint8_t rx_payload[64];
    msg_out->PayloadData = rx_payload;

    err = MCP251XFD_ReceiveMessageFromFIFO(&mcp251xfd_dev,
                                            msg_out,
                                            MCP251XFD_PAYLOAD_64BYTE,
                                            NULL,
                                            CAN_RX_FIFO);
    return (err == ERR_OK) ? CAN_CTRL_OK : CAN_CTRL_ERR_RX;
}

void can_ctrl_fault_irq_handler(uint16_t gpio_pin)
{
    if      (gpio_pin == CAN_ALRT1_Pin) can_fault_flags |= FAULT_FLAG_ALRT1;
    else if (gpio_pin == CAN_ALRT2_Pin) can_fault_flags |= FAULT_FLAG_ALRT2;
    else if (gpio_pin == CAN_FLT_Pin)   can_fault_flags |= FAULT_FLAG_FLT;
}

uint8_t can_ctrl_get_fault_flags(void)
{
    uint8_t flags = can_fault_flags;
    can_fault_flags = 0;
    return flags;
}
