/*
 * mcp_port.h
 *
 *  Created on: Feb 21, 2026
 *      Author: arch7560
 */

#ifndef MCP_PORT_H
#define MCP_PORT_H

#include <stdint.h>
#include <stdbool.h>

// Timing
void mcp_port_delay_us(uint32_t us);
uint32_t mcp_port_get_tick_ms(void);

// Optional locking (no-op bare metal, mutex later for RTOS)
void mcp_port_lock(void);
void mcp_port_unlock(void);

// Chip-select
void mcp_port_cs_assert(void);
void mcp_port_cs_deassert(void);

// SPI full-duplex transfer
bool mcp_port_spi_xfer(const uint8_t *tx, uint8_t *rx, uint16_t len);

// IRQ notify/consume (ISR sets; main loop consumes)
void mcp_port_notify_irq(void);
bool mcp_port_poll_irq_notified(void);

#endif /* MCP_PORT_H */
