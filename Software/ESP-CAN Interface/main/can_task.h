/* =============================================================================
 * can_task.h — CAN FD receive/transmit task for CANESP
 *
 * Runs on Core 1. Owns the MCP2518FD via the Mailly driver API.
 *
 * RX: Receives telemetry (0x100–0x1FF range) and fault (0x101 + offsets)
 *     frames from all STM32 nodes, serialises them via frame_codec, and
 *     pushes them to uart_port for transmission to NETESP.
 *
 * TX: Receives command frames from uart_port (posted by uart_task on Core 0)
 *     and transmits them onto the CAN FD bus for all STM32 nodes to receive.
 *
 * Node ID scheme:
 *   Up to 6 STM32 nodes, each with a 0x10 offset from the base:
 *     Node 0: telemetry=0x100, fault=0x101
 *     Node 1: telemetry=0x110, fault=0x111
 *     Node 2: telemetry=0x120, fault=0x121
 *     ... and so on.
 *   Hardware filter accepts the full 0x100–0x1FF range.
 *   Software dispatch identifies frame type by the lower nibble of the ID.
 *
 *      NODE_ID_STRIDE and the filter mask are the two values to adjust
 *      once we finalize the node ID scheme.
 * =========================================================================== */
#pragma once

#include "frame_codec.h"
#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------- */
/* Node ID scheme —  adjust when finalises                            */
/* -------------------------------------------------------------------------- */
#define CAN_NODE_COUNT          6u
#define CAN_NODE_ID_STRIDE      0x10u   /* ID offset between nodes            */
#define CAN_BASE_TELEM_ID       0x100u  /* Node 0 telemetry                   */
#define CAN_BASE_FAULT_ID       0x101u  /* Node 0 fault                       */

/* Filter accepts 0x100–0x1FF (mask bits 0-7 don't care) */
#define CAN_FILTER_ACCEPT_ID    0x100u
#define CAN_FILTER_ACCEPT_MASK  0x0FFu  /*  tighten once IDs finalised      */

/* Command IDs — broadcast to all nodes */
#define CAN_CMD_PING            0x200u
#define CAN_CMD_FORCE_LOG       0x202u
#define CAN_CMD_CLEAR_FAULTS    0x203u
#define CAN_CMD_RESET           0x204u

/* -------------------------------------------------------------------------- */
/* CAN FD configuration — matches STM32 nodes                                 */
/* -------------------------------------------------------------------------- */
#define CAN_OSC_FREQ_HZ         40000000u
#define CAN_NOMINAL_BITRATE     500000u
#define CAN_DATA_BITRATE        2000000u

/* -------------------------------------------------------------------------- */
/* API                                                                         */
/* -------------------------------------------------------------------------- */

/* Initialise MCP2518FD and start the CAN task on Core 1.
   Call after mcp_port_init() and uart_port_init(). */
void can_task_start(void);
