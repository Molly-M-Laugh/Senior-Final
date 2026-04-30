/* =============================================================================
 * mcp_port.h — ESP32 SPI port layer for MCP2518FD (CANESP)
 *
 * Mirrors the STM32 mcp_port exactly in structure and naming so the
 * MCP251XFD driver (Mailly) slots in identically. Only the HAL calls
 * underneath differ (ESP-IDF SPI master vs STM32 HAL SPI).
 *
 * SPI config — CONFIRM all GPIO against CANESP schematic:
 *   MOSI : GPIO_NUM_23    CONFIRM
 *   MISO : GPIO_NUM_19    CONFIRM
 *   SCK  : GPIO_NUM_18    CONFIRM
 *   CS   : GPIO_NUM_5     CONFIRM
 *   INT0 : GPIO_NUM_34    CONFIRM  (MCP2518FD TX done / bus error)
 *   INT1 : GPIO_NUM_35    CONFIRM  (MCP2518FD RX FIFO not empty)
 *
 * Clock: 9 MHz — matches STM32 node configuration.
 * MCP2518FD max SPI = 0.85 × (FSYSCLK / 2) = ~17 MHz with 40 MHz OSC.
 * 9 MHz is well within spec and keeps parity with STM32 nodes.
 * =========================================================================== */
#pragma once

#include "MCP251XFD.h"
#include <stdint.h>

/* -------------------------------------------------------------------------- */
/* SPI clock — keep in sync with STM32 nodes                                  */
/* -------------------------------------------------------------------------- */
#define MCP_SPI_CLOCK_HZ    9000000u

/* -------------------------------------------------------------------------- */
/* Single MCP2518FD on this board                                              */
/* -------------------------------------------------------------------------- */
#define MCP_CHIP_SELECT     0u

/* -------------------------------------------------------------------------- */
/* GPIO assignments —  CONFIRM against CANESP schematic                    */
/* -------------------------------------------------------------------------- */
#define MCP_PIN_MOSI        23   /* CONFIRM */
#define MCP_PIN_MISO        19   /* CONFIRM */
#define MCP_PIN_SCK         18   /* CONFIRM */
#define MCP_PIN_CS           5   /* CONFIRM */
#define MCP_PIN_INT0        34   /* CONFIRM — TX done / bus error  */
#define MCP_PIN_INT1        35   /* CONFIRM — RX FIFO not empty    */

/* -------------------------------------------------------------------------- */
/* Externally visible device object — used by can_task layer                  */
/* -------------------------------------------------------------------------- */
extern MCP251XFD mcp251xfd_dev;

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */

/* Initialise ESP-IDF SPI master bus and populate the MCP251XFD device struct.
   Call once from app_main() before can_task_start(). */
void mcp_port_init(void);

/* -------------------------------------------------------------------------- */
/* Port callbacks — passed as function pointers into the driver struct.       */
/* Not intended to be called directly by application code.                    */
/* -------------------------------------------------------------------------- */
eERRORRESULT mcp_spi_init(void *p_intf_dev, uint8_t chip_select,
                            const uint32_t sck_freq);

eERRORRESULT mcp_spi_transfer(void *p_intf_dev, uint8_t chip_select,
                                uint8_t *tx_data, uint8_t *rx_data,
                                size_t size);

uint32_t mcp_get_current_ms(void);
