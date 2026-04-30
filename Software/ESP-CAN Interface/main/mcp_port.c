/* =============================================================================
 * mcp_port.c — ESP32 SPI HAL for MCP2518FD
 * =========================================================================== */
#include "mcp_port.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "mcp_port";

#define MCP_SPI_HOST        SPI2_HOST   /* HSPI — leaves SPI3 free if needed */
#define MCP_SPI_TIMEOUT_MS  25u
#define MCP_TRANS_BUF_SIZE  MCP251XFD_TRANS_BUF_SIZE

MCP251XFD mcp251xfd_dev;

static spi_device_handle_t s_spi_handle;

/* -------------------------------------------------------------------------- */
/* CS helpers — manual CS so we match STM32 mcp_port exactly                 */
/* -------------------------------------------------------------------------- */
static inline void cs_assert(void)
{
    gpio_set_level(MCP_PIN_CS, 0);
}

static inline void cs_deassert(void)
{
    gpio_set_level(MCP_PIN_CS, 1);
}

/* -------------------------------------------------------------------------- */
/* mcp_spi_init — called once by MCP251XFD_Init()                            */
/* SPI bus is already up by the time this is called (mcp_port_init ran first)*/
/* -------------------------------------------------------------------------- */
eERRORRESULT mcp_spi_init(void *p_intf_dev, uint8_t chip_select,
                            const uint32_t sck_freq)
{
    (void)p_intf_dev;
    (void)chip_select;

    /* Accept any frequency at or below our configured clock.
       Driver may request 1 MHz during safe reset — that's fine. */
    if (sck_freq > MCP_SPI_CLOCK_HZ) {
        ESP_LOGE(TAG, "Requested SPI freq %lu > configured %u", sck_freq, MCP_SPI_CLOCK_HZ);
        return ERR__SPI_FREQUENCY_ERROR;
    }
    return ERR_OK;
}

/* -------------------------------------------------------------------------- */
/* mcp_spi_transfer — full-duplex, mirrors STM32 mcp_port exactly           */
/* -------------------------------------------------------------------------- */
eERRORRESULT mcp_spi_transfer(void *p_intf_dev, uint8_t chip_select,
                                uint8_t *tx_data, uint8_t *rx_data,
                                size_t size)
{
    (void)p_intf_dev;
    (void)chip_select;

    static uint8_t dummy_tx[MCP_TRANS_BUF_SIZE];
    static uint8_t dummy_rx[MCP_TRANS_BUF_SIZE];

    if (size > MCP_TRANS_BUF_SIZE) return ERR__SPI_COMM_ERROR;
    if (size == 0)                  return ERR_OK;

    uint8_t *tx = (tx_data != NULL) ? tx_data : dummy_tx;
    uint8_t *rx = (rx_data != NULL) ? rx_data : dummy_rx;

    if (tx_data == NULL) memset(dummy_tx, 0x00, size);

    spi_transaction_t t = {
        .length    = size * 8,   /* bits */
        .tx_buffer = tx,
        .rx_buffer = rx,
    };

    cs_assert();
    esp_err_t err = spi_device_polling_transmit(s_spi_handle, &t);
    cs_deassert();

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI transfer failed: %s", esp_err_to_name(err));
        return ERR__SPI_COMM_ERROR;
    }
    return ERR_OK;
}

/* -------------------------------------------------------------------------- */
/* mcp_get_current_ms — FreeRTOS tick → ms                                   */
/* -------------------------------------------------------------------------- */
uint32_t mcp_get_current_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/* -------------------------------------------------------------------------- */
/* mcp_port_init                                                               */
/* -------------------------------------------------------------------------- */
void mcp_port_init(void)
{
    /* --- CS pin as output, deasserted --- */
    gpio_config_t cs_cfg = {
        .pin_bit_mask = (1ULL << MCP_PIN_CS),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cs_cfg));
    cs_deassert();

    /* --- SPI bus --- */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = MCP_PIN_MOSI,
        .miso_io_num     = MCP_PIN_MISO,
        .sclk_io_num     = MCP_PIN_SCK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = MCP_TRANS_BUF_SIZE,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(MCP_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    /* --- Device on bus (no hardware CS — we manage it manually) --- */
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = MCP_SPI_CLOCK_HZ,
        .mode           = 0,           /* CPOL=0 CPHA=0 — matches MCP2518FD */
        .spics_io_num   = -1,          /* manual CS */
        .queue_size     = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(MCP_SPI_HOST, &dev_cfg, &s_spi_handle));

    /* --- Populate MCP251XFD device struct --- */
    mcp251xfd_dev.UserDriverData  = NULL;
    mcp251xfd_dev.DriverConfig    = MCP251XFD_DRIVER_NORMAL_USE;
    mcp251xfd_dev.InterfaceDevice = &s_spi_handle;
    mcp251xfd_dev.SPI_ChipSelect  = MCP_CHIP_SELECT;
    mcp251xfd_dev.SPIClockSpeed   = MCP_SPI_CLOCK_HZ;
    mcp251xfd_dev.fnSPI_Init      = mcp_spi_init;
    mcp251xfd_dev.fnSPI_Transfer  = mcp_spi_transfer;
    mcp251xfd_dev.fnGetCurrentms  = mcp_get_current_ms;
    mcp251xfd_dev.fnComputeCRC16  = NULL;   /* CRC mode not used */

    ESP_LOGI(TAG, "MCP port init complete");
}
