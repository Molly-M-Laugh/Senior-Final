/* =======================================================================
 * can_ctrl.c — CAN FD logic layer (MCP2518FD via mcp_port + Mailly driver)
 *
 * Layer stack:
 *   can_ctrl   ← this file  (message framing, FIFO mgmt, fault handling)
 *   mcp_port                (SPI HAL, CS toggling, device struct init)
 *   MCP251XFD driver        (Mailly — do NOT modify)
 * ===================================================================== */
#include "can_ctrl.h"
#include "mcp_port.h"

/* -----------------------------------------------------------------------
 * Internal state
 * --------------------------------------------------------------------- */
static volatile uint8_t  s_fault_flags  = 0u;
static can_rx_callback_t s_rx_callback  = NULL;

/* -----------------------------------------------------------------------
 * Static config — MCP251XFD_Config
 *
 * Key corrections vs. original broken version:
 *   .Bandwidth      = MCP251XFD_DELAY_16BIT_TIMES   (not _1BIT_)
 *   .INTsOutMode    = MCP251XFD_PINS_OPENDRAIN_OUT  (not _OPEN_DRAIN)
 *   .TXCANOutMode   = MCP251XFD_PINS_PUSHPULL_OUT   (not _PUSH_PULL)
 *   .SysInterruptFlags used (not .Interrupts)
 *   No .ControllerMode field — removed
 * --------------------------------------------------------------------- */
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

    /* INT0 → CAN_ALRT1 (TX done/err), INT1 → CAN_ALRT2 (RX not empty) */
    .GPIO0PinMode  = MCP251XFD_PIN_AS_INT0_TX,
    .GPIO1PinMode  = MCP251XFD_PIN_AS_INT1_RX,
    .INTsOutMode   = MCP251XFD_PINS_OPENDRAIN_OUT,
    .TXCANOutMode  = MCP251XFD_PINS_PUSHPULL_OUT,

    .SysInterruptFlags = MCP251XFD_INT_BUS_ERROR_EVENT
                       | MCP251XFD_INT_RX_INVALID_MESSAGE_EVENT,
};

/* -----------------------------------------------------------------------
 * Static FIFO list
 *
 * Key corrections:
 *   .Direction  = MCP251XFD_TRANSMIT_FIFO / MCP251XFD_RECEIVE_FIFO
 *                 (not MCP251XFD_TX_FIFO / _RX_FIFO)
 *   .InterruptFlags used (not .InterruptEnable)
 *   RX .Priority = PRIORITY30 (field present but ignored for RX FIFOs)
 * --------------------------------------------------------------------- */
static MCP251XFD_FIFO s_fifo_list[2] = {
    {
        /* TXQ — transmit queue */
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
        /* FIFO2 — receive */
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

/* -----------------------------------------------------------------------
 * Static filter — accept all standard IDs into FIFO2.
 * Tighten AcceptanceMask once node ID scheme is finalised.
 *
 * Key correction: .PointTo (not .FIFO) for filter→FIFO mapping.
 * --------------------------------------------------------------------- */
static MCP251XFD_Filter s_filter_list[1] = {
    {
        .Filter         = CAN_FILTER_0,
        .EnableFilter   = true,
        .Match          = MCP251XFD_MATCH_ONLY_SID,
        .AcceptanceID   = 0x000u,
        .AcceptanceMask = 0x000u,   /* mask=0 accepts everything */
        .PointTo        = CAN_RX_FIFO,
    },
};

/* =======================================================================
 * can_ctrl_init
 * ===================================================================== */
can_ctrl_status_t can_ctrl_init(void)
{
    eERRORRESULT res;

    /* Init_MCP251XFD — correct function name (not MCP251XFD_Init) */
    res = Init_MCP251XFD(&mcp251xfd_dev, &s_can_config);
    if (res != ERR_OK)
    {
        return CAN_CTRL_ERR_INIT;
    }

    /* Configure FIFOs */
    res = MCP251XFD_ConfigureFIFOList(&mcp251xfd_dev, s_fifo_list, 2u);
    if (res != ERR_OK)
    {
        return CAN_CTRL_ERR_INIT;
    }

    /* Configure filters */
    res = MCP251XFD_ConfigureFilterList(&mcp251xfd_dev,
                                        MCP251XFD_D_NET_FILTER_DISABLE,
                                        s_filter_list, 1u);
    if (res != ERR_OK)
    {
        return CAN_CTRL_ERR_INIT;
    }

    /* Enter Normal CAN FD mode */
    res = MCP251XFD_RequestOperationMode(&mcp251xfd_dev,
                                         MCP251XFD_NORMAL_CANFD_MODE,
                                         true);
    if (res != ERR_OK)
    {
        return CAN_CTRL_ERR_INIT;
    }

    return CAN_CTRL_OK;
}

/* =======================================================================
 * can_ctrl_transmit
 * ===================================================================== */
can_ctrl_status_t can_ctrl_transmit(const can_telemetry_t *data)
{
    if (data == NULL)
    {
        return CAN_CTRL_ERR_TX;
    }

    MCP251XFD_CANMessage msg = {
        .MessageID    = CAN_MSGID_TELEMETRY,
        .ControlFlags = MCP251XFD_CANFD_FRAME
                      | MCP251XFD_SWITCH_BITRATE
                      | MCP251XFD_NO_FIFO,        /* NO_RTR → NO_FIFO in this driver ver */
        .DLC          = MCP251XFD_DLC_20BYTE,
        .PayloadData  = (uint8_t *)data,
    };

    eERRORRESULT res = MCP251XFD_TransmitMessageToFIFO(&mcp251xfd_dev,
                                                        &msg,
                                                        CAN_TX_FIFO,
                                                        true);
    if (res == ERR__BUFFER_FULL)   /* ERR__TXFIFO_FULL → ERR__BUFFER_FULL */
    {
        return CAN_CTRL_ERR_TX;
    }
    if (res != ERR_OK)
    {
        return CAN_CTRL_ERR_TX;
    }

    return CAN_CTRL_OK;
}

/* =======================================================================
 * can_ctrl_receive
 * ===================================================================== */
can_ctrl_status_t can_ctrl_receive(MCP251XFD_CANMessage *msg_out)
{
    if (msg_out == NULL)
    {
        return CAN_CTRL_ERR_RX;
    }

    /* Check FIFO status before attempting read */
    setMCP251XFD_FIFOstatus fifo_status = 0;   /* was MCP251XFD_FIFOStatus */
    eERRORRESULT res = MCP251XFD_GetFIFOStatus(&mcp251xfd_dev,
                                                CAN_RX_FIFO,
                                                &fifo_status);
    if (res != ERR_OK)
    {
        return CAN_CTRL_ERR_RX;
    }

    if ((fifo_status & MCP251XFD_RX_FIFO_NOT_EMPTY) == 0u)
    {
        return CAN_CTRL_ERR_RX;   /* nothing waiting — caller polls */
    }

    uint8_t payload_buf[64] = {0};
    msg_out->PayloadData = payload_buf;

    res = MCP251XFD_ReceiveMessageFromFIFO(&mcp251xfd_dev,
                                            msg_out,
                                            MCP251XFD_PAYLOAD_64BYTE,
                                            NULL,
                                            CAN_RX_FIFO);
    if (res != ERR_OK)
    {
        return CAN_CTRL_ERR_RX;
    }

    /* Fire registered callback if present */
    if (s_rx_callback != NULL)
    {
        s_rx_callback(msg_out);
    }

    return CAN_CTRL_OK;
}

/* =======================================================================
 * can_ctrl_register_rx_callback
 * ===================================================================== */
void can_ctrl_register_rx_callback(can_rx_callback_t cb)
{
    s_rx_callback = cb;
}

/* =======================================================================
 * can_ctrl_irq_handler
 * Called from app_callbacks.c HAL_GPIO_EXTI_Callback for CAN_ALRT pins.
 * ===================================================================== */
void can_ctrl_irq_handler(uint16_t gpio_pin)
{
    /* CAN_ALRT2 = INT1 = RX FIFO not empty — drain it */
    if (gpio_pin == CAN_ALRT2_Pin)
    {
        MCP251XFD_CANMessage msg;
        /* Loop until FIFO empty; can_ctrl_receive returns ERR_RX when done */
        while (can_ctrl_receive(&msg) == CAN_CTRL_OK)
        {
            /* callback fired inside can_ctrl_receive */
        }
    }

    /* CAN_ALRT1 = INT0 = TX done / TX error / bus error */
    if (gpio_pin == CAN_ALRT1_Pin)
    {
        s_fault_flags |= (1u << 0);

        /* Read and clear interrupt flags */
        setMCP251XFD_InterruptEvents int_flags = 0; /* was MCP251XFD_InterruptEvents */
        MCP251XFD_GetInterruptEvents(&mcp251xfd_dev, &int_flags);
        MCP251XFD_ClearInterruptEvents(&mcp251xfd_dev, int_flags);
    }
}

/* =======================================================================
 * can_ctrl_get_fault_flags
 * ===================================================================== */
uint8_t can_ctrl_get_fault_flags(void)
{
    uint8_t flags = s_fault_flags;
    s_fault_flags = 0u;
    return flags;
}
