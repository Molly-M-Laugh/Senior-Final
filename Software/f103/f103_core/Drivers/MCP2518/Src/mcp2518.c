/*
 * mcp2518.c
 *
 *  Created on: Feb 21, 2026
 *      Author: arch7560
 */


#include "mcp2518.h"
#include "mcp_port.h"

// ---------------- RX ring buffer ----------------
#define RXQ_SIZE  16

static mcp_can_frame_t s_rxq[RXQ_SIZE];
static volatile uint8_t s_rx_head = 0;
static volatile uint8_t s_rx_tail = 0;

static bool rxq_push(const mcp_can_frame_t *f)
{
  uint8_t next = (uint8_t)((s_rx_head + 1u) % RXQ_SIZE);
  if (next == s_rx_tail) return false; // full
  s_rxq[s_rx_head] = *f;
  s_rx_head = next;
  return true;
}

static bool rxq_pop(mcp_can_frame_t *f)
{
  if (s_rx_tail == s_rx_head) return false; // empty
  *f = s_rxq[s_rx_tail];
  s_rx_tail = (uint8_t)((s_rx_tail + 1u) % RXQ_SIZE);
  return true;
}

// ---------------- SPI / register helpers (stubs) ----------------
// You will fill these in with real MCP2518 command formats.
static bool mcp_reset_stub(void)
{
  // TODO: issue MCP2518 RESET SPI command
  return true;
}

mcp_status_t mcp2518_init_classic(uint32_t nominal_bitrate)
{
  (void)nominal_bitrate;

  mcp_port_lock();

  if (!mcp_reset_stub()) {
    mcp_port_unlock();
    return MCP_ERR_SPI;
  }

  // TODO:
  // - enter config mode
  // - set bit timing for 500 kbps
  // - setup RX/TX FIFOs
  // - enable interrupts
  // - enter normal mode

  mcp_port_unlock();
  return MCP_OK;
}

void mcp2518_service(void)
{
  // TODO: read/clear interrupt flags, drain RX FIFO, handle TX complete, errors
  // For now, nothing.

  // Example placeholder to prove ring buffer wiring:
  // (Remove this once you implement real RX.)
  // mcp_can_frame_t demo = {.id=0x123, .dlc=1, .data={0xAB}, .extended=false, .rtr=false};
  // rxq_push(&demo);
}

mcp_status_t mcp2518_send(const mcp_can_frame_t *frm)
{
  if (!frm) return MCP_ERR_PARAM;

  // TODO: write into TX FIFO and request transmit
  return MCP_OK;
}

bool mcp2518_recv(mcp_can_frame_t *out)
{
  if (!out) return false;
  return rxq_pop(out);
}

uint32_t mcp2518_get_error_flags(void)
{
  // TODO: read MCP2518 error/status registers
  return 0;
}
