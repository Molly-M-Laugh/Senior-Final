/*
 * can_ctrl.c
 *
 *  Created on: Feb 21, 2026
 *      Author: arch7560
 */


#include "can_ctrl.h"
#include "mcp2518.h"
#include "mcp_port.h"

void can_ctrl_init(void)
{
  // init MCP for 500 kbps classic CAN
  (void)mcp2518_init_classic(500000);
}

void can_ctrl_poll(void)
{
  // Only service MCP when interrupt says there is work
  if (mcp_port_poll_irq_notified()) {
    mcp2518_service();
  }

  // Later: you can also drain received frames here:
  // mcp_can_frame_t f;
  // while (mcp2518_recv(&f)) { ...process... }
}
