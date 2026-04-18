#ifndef MCP_PORT_H
#define MCP_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "MCP251XFD.h"
#include "driver/spi_master.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * SPI bus and pin configuration
 *
 * *** FLAG: confirm these against your ESP32 schematic before building ***
 *
 * SPI2_HOST = HSPI in older ESP-IDF naming. Use SPI2_HOST or SPI3_HOST
 * depending on which bus your MCP2518FD is wired to.
 *
 * MCP_CS_GPIO  — GPIO number connected to MCP2518FD !CS
 * MCP_SCK_GPIO — GPIO number connected to SCK
 * MCP_MOSI_GPIO — GPIO number connected to MOSI (SI on MCP side)
 * MCP_MISO_GPIO — GPIO number connected to MISO (SO on MCP side)
 * --------------------------------------------------------------------- */
#define MCP_SPI_HOST        SPI3_HOST   /* VSPI */
#define MCP_CS_GPIO         5
#define MCP_SCK_GPIO        18
#define MCP_MOSI_GPIO       23
#define MCP_MISO_GPIO       19
#define MCP_INT_GPIO        4
/* -----------------------------------------------------------------------
 * SPI clock speed.
 * MCP2518FD max SPI = ~0.85 * (FSYSCLK / 2). With 40MHz oscillator
 * that is ~17MHz max. 9MHz matches the STM32 side and is well within spec.
 * --------------------------------------------------------------------- */
#define MCP_SPI_CLOCK_HZ    9000000u

/* Chip select index — only one MCP2518FD on this bus */
#define MCP_CHIP_SELECT     0u

/* -----------------------------------------------------------------------
 * Externally visible device object — used by can_ctrl layer to call
 * into the Mailly driver (MCP251XFD_Init, MCP251XFD_TransmitMessage, etc.)
 * --------------------------------------------------------------------- */
extern MCP251XFD mcp251xfd_dev;

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

/* Initialise the SPI bus and register the MCP2518FD device.
   Must be called once at startup, before mcp_port_init_device_struct(). */
esp_err_t mcp_port_spi_bus_init(void);

/* Populate the MCP251XFD device struct with port function pointers and
   hardware parameters. Call after mcp_port_spi_bus_init(). */
void mcp_port_init_device_struct(void);

/* -----------------------------------------------------------------------
 * Port functions — passed as function pointers into the driver struct.
 * Not intended to be called directly by application code.
 * --------------------------------------------------------------------- */
eERRORRESULT mcp_spi_init(void *p_intf_dev, uint8_t chip_select,
                            const uint32_t sck_freq);

eERRORRESULT mcp_spi_transfer(void *p_intf_dev, uint8_t chip_select,
                                uint8_t *tx_data, uint8_t *rx_data,
                                size_t size);

uint32_t mcp_get_current_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* MCP_PORT_H */