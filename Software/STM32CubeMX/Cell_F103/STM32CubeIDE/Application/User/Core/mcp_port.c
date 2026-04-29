#include "mcp_port.h"
#include "spi.h"
#include "gpio.h"

#define MCP_SPI_TIMEOUT_MS  25u

/* -----------------------------------------------------------------------
 * Device object — the Mailly driver operates on this struct.
 * Populated by mcp_port_init_device_struct() before MCP251XFD_Init().
 * --------------------------------------------------------------------- */
MCP251XFD mcp251xfd_dev;

/* -----------------------------------------------------------------------
 * CS helpers
 * --------------------------------------------------------------------- */
static inline void cs_assert(void)
{
    HAL_GPIO_WritePin(SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_RESET);
}

static inline void cs_deassert(void)
{
    HAL_GPIO_WritePin(SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_SET);
}

/* -----------------------------------------------------------------------
 * mcp_spi_init
 *
 * Called once by MCP251XFD_Init(). SPI1 peripheral is already configured
 * by CubeMX at 9MHz — nothing hardware to do here. We just validate the
 * requested clock isn't above what we configured.
 * --------------------------------------------------------------------- */
eERRORRESULT mcp_spi_init(void *p_intf_dev, uint8_t chip_select,
                            const uint32_t sck_freq)
{
    (void)p_intf_dev;
    (void)chip_select;

    /* Driver may request a reduced clock during safe reset (1MHz).
       STM32 SPI prescaler is fixed by CubeMX so we can't change it
       at runtime without reinitializing the peripheral. For now, accept
       any requested frequency at or below our configured speed.
       Flag if the driver requests something above our clock. */
    if (sck_freq > MCP_SPI_CLOCK_HZ)
    {
        return ERR__SPI_FREQUENCY_ERROR;
    }

    return ERR_OK;
}

/* -----------------------------------------------------------------------
 * mcp_spi_transfer
 *
 * Full-duplex SPI transfer. The Mailly driver handles framing — we just
 * assert CS, clock bytes, deassert CS.
 *
 * tx_data and rx_data may both be non-NULL (full duplex), or one may be
 * NULL if the driver only needs to send or receive. HAL_SPI_TransmitReceive
 * requires both buffers, so we use a small dummy buffer for the NULL case.
 * --------------------------------------------------------------------- */
eERRORRESULT mcp_spi_transfer(void *p_intf_dev, uint8_t chip_select,
                                uint8_t *tx_data, uint8_t *rx_data,
                                size_t size)
{
    (void)p_intf_dev;
    (void)chip_select;

    /* Dummy buffers for one-directional transfers */
    static uint8_t dummy_tx[MCP251XFD_TRANS_BUF_SIZE];
    static uint8_t dummy_rx[MCP251XFD_TRANS_BUF_SIZE];

    if (size > MCP251XFD_TRANS_BUF_SIZE) return ERR__SPI_COMM_ERROR;

    uint8_t *tx = (tx_data != NULL) ? tx_data : dummy_tx;
    uint8_t *rx = (rx_data != NULL) ? rx_data : dummy_rx;

    /* Zero dummy tx if we're receive-only so we clock out 0x00 bytes */
    if (tx_data == NULL)
    {
        for (size_t i = 0; i < size; i++) dummy_tx[i] = 0x00u;
    }

    cs_assert();
    HAL_StatusTypeDef hal_st = HAL_SPI_TransmitReceive(&hspi1,
                                                         tx, rx,
                                                         (uint16_t)size,
                                                         MCP_SPI_TIMEOUT_MS);
    cs_deassert();

    return (hal_st == HAL_OK) ? ERR_OK : ERR__SPI_COMM_ERROR;
}

/* -----------------------------------------------------------------------
 * mcp_get_current_ms
 *
 * HAL_GetTick() returns the SysTick millisecond counter directly.
 * --------------------------------------------------------------------- */
uint32_t mcp_get_current_ms(void)
{
    return HAL_GetTick();
}

/* -----------------------------------------------------------------------
 * mcp_port_init_device_struct
 *
 * Populates the MCP251XFD device struct with port function pointers and
 * hardware parameters. Call this before MCP251XFD_Init().
 * --------------------------------------------------------------------- */
void mcp_port_init_device_struct(void)
{
    mcp251xfd_dev.UserDriverData  = NULL;
    mcp251xfd_dev.DriverConfig    = MCP251XFD_DRIVER_NORMAL_USE;
    mcp251xfd_dev.InterfaceDevice = &hspi1;       /* passed as p_intf_dev */
    mcp251xfd_dev.SPI_ChipSelect  = MCP_CHIP_SELECT;
    mcp251xfd_dev.SPIClockSpeed   = MCP_SPI_CLOCK_HZ;
    mcp251xfd_dev.fnSPI_Init      = mcp_spi_init;
    mcp251xfd_dev.fnSPI_Transfer  = mcp_spi_transfer;
    mcp251xfd_dev.fnGetCurrentms  = mcp_get_current_ms;
    mcp251xfd_dev.fnComputeCRC16  = NULL;          /* CRC mode not used   */
}
