/**
 * @file    power_ctrl.h
 * @brief   Power rail sequencing and fault control.
 *
 * Manages the three output rails in fixed order:
 *   5V0  -> TPS2421-1 eFuse  (EN5V0_Pin  PB10, active-low enable)
 *   3V3  -> TPS2421-1 eFuse  (EN3V3_Pin  PB15, active-low enable)
 *   1V8  -> TPS22950x switch (ON1V8_Pin  PB14, active-high enable)
 *
 * The DSC6111B oscillator (OSC_EN_Pin PB11, active-high) is enabled
 * last, after all rails are confirmed good.
 *
 * Fault inputs (active-low, EXTI falling):
 *   FLT_5V0_Pin  PB0  EXTI0
 *   FLT_3V3_Pin  PA1  EXTI1
 *   FLT_1V8_Pin  PA2  EXTI2
 *
 * Power-good inputs:
 *   PG5V0_Pin    PB4  (active-high, polled during sequence)
 *   PG3V3_Pin    PB1  (active-high, polled only — no EXTI)
 */

#ifndef POWER_CTRL_H
#define POWER_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

typedef enum {
    POWER_OK              = 0,
    POWER_ERR_5V_TIMEOUT  = 1,
    POWER_ERR_3V3_TIMEOUT = 2,
    POWER_ERR_1V8_TIMEOUT = 3,
	POWER_ERR_FAULT       = 4,
} power_result_t;

typedef enum {
    POWER_RAIL_5V0 = 0,
    POWER_RAIL_3V3 = 1,
    POWER_RAIL_1V8 = 2,
} power_rail_t;

power_result_t power_sequence_up(void);
void           power_sequence_down(void);
void           power_disable_rail(power_rail_t rail);
uint8_t        power_rail_is_enabled(power_rail_t rail);
uint8_t        power_pg_5v0(void);
uint8_t        power_pg_3v3(void);
power_result_t power_reset_efuse(power_rail_t rail);
void           power_fault_irq_handler(uint16_t gpio_pin);
uint8_t        power_get_fault_flags(void);

#ifdef __cplusplus
}
#endif

#endif /* POWER_CTRL_H */
