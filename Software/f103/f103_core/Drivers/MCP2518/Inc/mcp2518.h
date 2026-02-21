/*
 * mcp2518.h
 *
 *  Created on: Feb 21, 2026
 *      Author: Arch7560
 */
#ifndef MCP2518_H
#define MCP2518_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
  uint32_t id;      // 11-bit std or 29-bit ext
  uint8_t  dlc;     // 0..8 for classic CAN
  uint8_t  data[8];
  bool     extended;
  bool     rtr;
} mcp_can_frame_t;

typedef enum {
  MCP_OK = 0,
  MCP_ERR_SPI,
  MCP_ERR_TIMEOUT,
  MCP_ERR_BADSTATE,
  MCP_ERR_PARAM
} mcp_status_t;

// Classic CAN init at nominal bitrate (e.g., 500000)
mcp_status_t mcp2518_init_classic(uint32_t nominal_bitrate);

// Call when MCP2518 interrupt notified (or periodically)
void mcp2518_service(void);

// Send one CAN frame (queues or directly submits)
mcp_status_t mcp2518_send(const mcp_can_frame_t *frm);

// Receive one CAN frame if available
bool mcp2518_recv(mcp_can_frame_t *out);

// Optional: return driver/controller error flags (stub for now)
uint32_t mcp2518_get_error_flags(void);

#endif /* MCP2518_H */
