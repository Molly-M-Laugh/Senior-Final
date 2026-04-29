#ifndef MCP_PORT_H
#define MCP_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "MCP251XFD.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * SPI clock speed — SPI1 at 72MHz / prescaler 8 = 9MHz
 * MCP2518FD max SPI clock = 0.85 * (FSYSCLK / 2).
 * With the default 40MHz oscillator this gives ~17MHz max.
 * 9MHz is well within spec.
 * --------------------------------------------------------------------- */
#define MCP_SPI_CLOCK_HZ    9000000u

/* -----------------------------------------------------------------------
 * Chip select index — we only have one MCP2518FD, use index 0.
 * The actual GPIO toggled is SPI_CS_Pin (PA4).
 * --------------------------------------------------------------------- */
#define MCP_CHIP_SELECT     0u

/* -----------------------------------------------------------------------
 * Externally visible device object — used by can_ctrl layer to call
 * into the Mailly driver (e.g. MCP251XFD_Init, MCP251XFD_TransmitMessage)
 * --------------------------------------------------------------------- */
extern MCP251XFD mcp251xfd_dev;

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

/* Populate the MCP251XFD device struct with port function pointers
   and hardware parameters. Call before MCP251XFD_Init(). */
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
