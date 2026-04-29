#ifndef FAULT_CTRL_H
#define FAULT_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "fram_port.h"   /* fram_log_entry_t, log_severity_t, log_event_t */
#include <stdint.h>

/* -----------------------------------------------------------------------
 * Fault severity tiers
 *
 * TIER 1 — WARNING
 *   Log to FRAM via fram_drv_log(), transmit CAN status frame, keep running.
 *   Examples: INA3221 warning threshold crossed, temp alert.
 *
 * TIER 2 — CRITICAL
 *   Disable the affected rail, assert ESP32 alert (PA8 / FreeIO_Pin),
 *   log to FRAM, transmit CAN fault frame.
 *   Examples: eFuse FLT line asserted, INA3221 critical threshold.
 *
 * TIER 3 — EMERGENCY
 *   Full power-down sequence, assert ESP32 alert, log to FRAM.
 *   Examples: multiple simultaneous rail faults, all temp sensors over limit.
 * --------------------------------------------------------------------- */
typedef enum {
    FAULT_TIER_NONE      = 0,
    FAULT_TIER_WARNING   = 1,
    FAULT_TIER_CRITICAL  = 2,
    FAULT_TIER_EMERGENCY = 3,
} fault_tier_t;

/* -----------------------------------------------------------------------
 * Fault source identifiers — one bit per source for logging.
 * Bits 0–7 are packed into the first payload byte of a fram_log_entry_t.
 * Bits 8–9 go into payload byte 1.
 * --------------------------------------------------------------------- */
#define FAULT_SRC_NONE          0x00000000u
#define FAULT_SRC_5V0_FLT       (1u << 0)
#define FAULT_SRC_3V3_FLT       (1u << 1)
#define FAULT_SRC_1V8_FLT       (1u << 2)
#define FAULT_SRC_5V0_PG_LOST   (1u << 3)
#define FAULT_SRC_INA3221_WARN  (1u << 4)
#define FAULT_SRC_INA3221_CRIT  (1u << 5)
#define FAULT_SRC_TEMP0_ALERT   (1u << 6)
#define FAULT_SRC_TEMP1_ALERT   (1u << 7)
#define FAULT_SRC_TEMP2_ALERT   (1u << 8)
#define FAULT_SRC_CAN_FLT       (1u << 9)

/* -----------------------------------------------------------------------
 * PA8 / FreeIO_Pin — ESP32 alert output.
 * Asserted HIGH to signal a tier 2+ fault to the ESP32.
 * --------------------------------------------------------------------- */
#define FAULT_ALERT_ASSERT()   \
    HAL_GPIO_WritePin(FreeIO_GPIO_Port, FreeIO_Pin, GPIO_PIN_SET)
#define FAULT_ALERT_DEASSERT() \
    HAL_GPIO_WritePin(FreeIO_GPIO_Port, FreeIO_Pin, GPIO_PIN_RESET)

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

/* Call once in USER CODE BEGIN 2 after all modules are initialized */
void fault_ctrl_init(void);

/* Call every main loop iteration. Collects flags from all modules,
   applies tier policy, takes action. */
void fault_ctrl_process(void);

/* Read the current highest active fault tier (cleared by process) */
fault_tier_t fault_ctrl_get_active_tier(void);

/* Read the combined fault source flags from the last process() call */
uint32_t fault_ctrl_get_active_sources(void);

/* Read up to max_entries fault log entries from FRAM via fram_drv_read_entry().
   Returns the number of entries actually read. */
uint16_t fault_ctrl_read_log(fram_log_entry_t *buf, uint16_t max_entries);

/* Erase all FRAM log entries via fram_drv_format(). Use with caution. */
void fault_ctrl_clear_log(void);

#ifdef __cplusplus
}
#endif

#endif /* FAULT_CTRL_H */
