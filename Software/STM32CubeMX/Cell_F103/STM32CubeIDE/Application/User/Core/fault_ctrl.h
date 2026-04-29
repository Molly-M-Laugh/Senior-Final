#ifndef FAULT_CTRL_H
#define FAULT_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * Fault severity tiers
 *
 * TIER 1 — WARNING
 *   Log to FRAM, transmit CAN status frame, keep running.
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
 * Fault source identifiers — one bit per source for logging
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
 * FRAM fault log — each record is 12 bytes, written sequentially.
 * Base address in FRAM: define layout here so all modules agree.
 * --------------------------------------------------------------------- */
#define FAULT_LOG_FRAM_BASE     0x0000u   /* Start of fault log in FRAM  */
#define FAULT_LOG_MAX_RECORDS   64u       /* 64 * 12 = 768 bytes         */
#define FAULT_LOG_RECORD_SIZE   12u

typedef struct __attribute__((packed)) {
    uint32_t timestamp_ms;   /* HAL_GetTick() at time of fault          */
    uint32_t fault_sources;  /* OR of FAULT_SRC_* bits                  */
    uint8_t  tier;           /* fault_tier_t value                      */
    uint8_t  rail_states;    /* Bit 0=5V0 en, 1=3V3 en, 2=1V8 en       */
    uint8_t  reserved[2];
} fault_log_record_t;        /* 12 bytes */

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

/* Read up to max_records fault log entries from FRAM into buf.
   Returns the number of records actually read. */
uint16_t fault_ctrl_read_log(fault_log_record_t *buf, uint16_t max_records);

/* Clear the entire fault log in FRAM and reset the write index */
void fault_ctrl_clear_log(void);

/* Call once in USER CODE BEGIN 2 after all modules are initialized */
void fault_ctrl_init(void);

/* Call every main loop iteration. Collects flags from all modules,
   applies tier policy, takes action. */
void fault_ctrl_process(void);

/* Read the current highest active fault tier (cleared by process) */
fault_tier_t fault_ctrl_get_active_tier(void);

/* Read the combined fault source flags from the last process() call */
uint32_t fault_ctrl_get_active_sources(void);

#ifdef __cplusplus
}
#endif

#endif /* FAULT_CTRL_H */
