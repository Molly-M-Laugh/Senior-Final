#include "mcp_port.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "mcp_port";

/* -----------------------------------------------------------------------
 * Device object — the Mailly driver operates on this struct.
 * Populated by mcp_port_init_device_struct() before MCP251XFD_Init().
 * --------------------------------------------------------------------- */
MCP251XFD mcp251xfd_dev;

/* SPI device handle — registered during mcp_port_spi_bus_init().
   Passed as InterfaceDevice and cast back in mcp_spi_transfer(). */
static spi_device_handle_t s_spi_handle;

/* -----------------------------------------------------------------------
 * CS helpers
 *
 * spics_io_num is set to -1 in the device config so ESP-IDF does not
 * drive CS automatically. We assert/deassert manually to match the same
 * pattern used on the STM32 side and give the Mailly driver full control
 * of the CS timing through mcp_spi_transfer().
 * --------------------------------------------------------------------- */
static inline void cs_assert(void)
{
    gpio_set_level(MCP_CS_GPIO, 0);
}

static inline void cs_deassert(void)
{
    gpio_set_level(MCP_CS_GPIO, 1);
}

/* -----------------------------------------------------------------------
 * mcp_port_spi_bus_init
 *
 * Configures the SPI bus and registers the MCP2518FD as a device.
 * Call once at startup before mcp_port_init_device_struct().
 *
 * The MCP2518FD uses SPI Mode 0,0 (CPOL=0, CPHA=0).
 * --------------------------------------------------------------------- */
esp_err_t mcp_port_spi_bus_init(void)
{
    /* Configure CS GPIO separately — we drive it manually */
    gpio_config_t cs_cfg = {
        .pin_bit_mask = (1ULL << MCP_CS_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&cs_cfg);
    if (err != ESP_OK) return err;
    cs_deassert(); /* idle high */

    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = MCP_MOSI_GPIO,
        .miso_io_num     = MCP_MISO_GPIO,
        .sclk_io_num     = MCP_SCK_GPIO,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        /* Max transfer size in bytes — one full CAN FD frame with CRC */
        .max_transfer_sz = MCP251XFD_TRANS_BUF_SIZE,
    };

    err = spi_bus_initialize(MCP_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(err));
        return err;
    }

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = MCP_SPI_CLOCK_HZ,
        .mode           = 0,        /* CPOL=0, CPHA=0 */
        .spics_io_num   = -1,       /* CS driven manually */
        .queue_size     = 1,
        .pre_cb         = NULL,
        .post_cb        = NULL,
    };

    err = spi_bus_add_device(MCP_SPI_HOST, &dev_cfg, &s_spi_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "SPI bus ready at %u Hz", MCP_SPI_CLOCK_HZ);
    return ESP_OK;
}

/* -----------------------------------------------------------------------
 * mcp_spi_init
 *
 * Called once by MCP251XFD_Init(). The bus is already up from
 * mcp_port_spi_bus_init() — nothing hardware to do here. We just validate
 * the requested clock isn't above what we configured.
 * --------------------------------------------------------------------- */
eERRORRESULT mcp_spi_init(void *p_intf_dev, uint8_t chip_select,
                            const uint32_t sck_freq)
{
    (void)p_intf_dev;
    (void)chip_select;

    /* Driver may request a reduced clock during safe reset (1MHz).
       ESP-IDF clock speed is fixed at device-add time, so we can't
       change it at runtime without removing and re-adding the device.
       Accept any requested frequency at or below our configured speed. */
    if (sck_freq > MCP_SPI_CLOCK_HZ)
    {
        return ERR__SPI_FREQUENCY_ERROR;
    }

    return ERR_OK;
}

/* -----------------------------------------------------------------------
 * mcp_spi_transfer
 *
 * Full-duplex SPI transfer. Mirrors the STM32 implementation exactly —
 * assert CS, clock bytes, deassert CS.
 *
 * p_intf_dev is &s_spi_handle, cast back to spi_device_handle_t *.
 *
 * ESP-IDF spi_transaction_t.length is in BITS, not bytes.
 *
 * tx_data or rx_data may be NULL (driver signals send-only or recv-only).
 * spi_device_polling_transmit requires both buffers, so we substitute
 * a zero-filled dummy buffer for the NULL side, same as STM32 port.
 * --------------------------------------------------------------------- */
eERRORRESULT mcp_spi_transfer(void *p_intf_dev, uint8_t chip_select,
                                uint8_t *tx_data, uint8_t *rx_data,
                                size_t size)
{
    (void)chip_select;

    if (size > MCP251XFD_TRANS_BUF_SIZE) return ERR__SPI_COMM_ERROR;

    /* Dummy buffers for one-directional transfers — static, same as STM32 */
    static uint8_t dummy_tx[MCP251XFD_TRANS_BUF_SIZE];
    static uint8_t dummy_rx[MCP251XFD_TRANS_BUF_SIZE];

    uint8_t *tx = (tx_data != NULL) ? tx_data : dummy_tx;
    uint8_t *rx = (rx_data != NULL) ? rx_data : dummy_rx;

    /* Zero dummy tx if receive-only so we clock out 0x00 bytes */
    if (tx_data == NULL)
    {
        memset(dummy_tx, 0x00, size);
    }

    spi_device_handle_t *dev = (spi_device_handle_t *)p_intf_dev;

    spi_transaction_t t = {
        .length    = size * 8,  /* length is in BITS in ESP-IDF */
        .tx_buffer = tx,
        .rx_buffer = rx,
    };

    cs_assert();
    esp_err_t err = spi_device_polling_transmit(*dev, &t);
    cs_deassert();

    return (err == ESP_OK) ? ERR_OK : ERR__SPI_COMM_ERROR;
}

/* -----------------------------------------------------------------------
 * mcp_get_current_ms
 *
 * esp_timer_get_time() returns microseconds since boot as int64_t.
 * Divide by 1000 and cast to uint32_t — matches HAL_GetTick() behaviour.
 * Rolls over after ~49 days, acceptable for this use case.
 * --------------------------------------------------------------------- */
uint32_t mcp_get_current_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

/* -----------------------------------------------------------------------
 * mcp_port_init_device_struct
 *
 * Populates the MCP251XFD device struct with port function pointers and
 * hardware parameters. Call after mcp_port_spi_bus_init() and before
 * MCP251XFD_Init().
 * --------------------------------------------------------------------- */
void mcp_port_init_device_struct(void)
{
    mcp251xfd_dev.UserDriverData  = NULL;
    mcp251xfd_dev.DriverConfig    = MCP251XFD_DRIVER_NORMAL_USE;
    mcp251xfd_dev.InterfaceDevice = &s_spi_handle;  /* cast back in transfer */
    mcp251xfd_dev.SPI_ChipSelect  = MCP_CHIP_SELECT;
    mcp251xfd_dev.SPIClockSpeed   = MCP_SPI_CLOCK_HZ;
    mcp251xfd_dev.fnSPI_Init      = mcp_spi_init;
    mcp251xfd_dev.fnSPI_Transfer  = mcp_spi_transfer;
    mcp251xfd_dev.fnGetCurrentms  = mcp_get_current_ms;
    mcp251xfd_dev.fnComputeCRC16  = NULL;           /* CRC mode not used */
}