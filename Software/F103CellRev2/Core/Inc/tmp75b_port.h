#ifndef TMP75B_PORT_H
#define TMP75B_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * I2C addresses — base 0x48, set by A2:A1:A0 pins
 * U3 (TEMP0): A2=GND A1=GND A0=GND -> 0x48  (confirmed from schematic)
 * U4 (TEMP1): A2=GND A1=GND A0=VCC -> 0x49  (verify during bench scan)
 * U5 (TEMP2): A2=GND A1=VCC A0=VCC -> 0x4B  (verify during bench scan)
 * --------------------------------------------------------------------- */
#define TMP75B_ADDR_TEMP0       0x48u
#define TMP75B_ADDR_TEMP1       0x49u
#define TMP75B_ADDR_TEMP2       0x4Bu

/* -----------------------------------------------------------------------
 * Register addresses (TMP75B datasheet)
 * --------------------------------------------------------------------- */
#define TMP75B_REG_TEMP         0x00u   /* Temperature (read only)        */
#define TMP75B_REG_CONFIG       0x01u   /* Configuration                  */
#define TMP75B_REG_TLOW         0x02u   /* Alert low limit                */
#define TMP75B_REG_THIGH        0x03u   /* Alert high limit               */

/* -----------------------------------------------------------------------
 * Config register value used during init.
 *
 * Bit [7]   OS/ALERT  = 0  (comparator mode, not one-shot)
 * Bits[6:5] RES       = 11 (12-bit resolution, 0.0625 deg/LSB)
 * Bits[4:3] F-Queue   = 00 (alert after 1 fault)
 * Bit [2]   POL       = 0  (alert active low — matches EXTI falling edge)
 * Bit [1]   TM        = 0  (comparator mode)
 * Bit [0]   SD        = 0  (continuous conversion)
 *
 * = 0x60
 * --------------------------------------------------------------------- */
#define TMP75B_CONFIG_VALUE     0x60u

/* -----------------------------------------------------------------------
 * Sensor index — used to select device in API calls
 * --------------------------------------------------------------------- */
typedef enum {
    TMP75B_SENSOR_0 = 0,   /* U3 — TEMP0_ALERT PB12 */
    TMP75B_SENSOR_1 = 1,   /* U4 — TEMP1_ALERT PB13 */
    TMP75B_SENSOR_2 = 2,   /* U5 — TEMP2_ALERT PC14 */
    TMP75B_SENSOR_COUNT = 3,
} tmp75b_sensor_t;

/* -----------------------------------------------------------------------
 * Return type
 * --------------------------------------------------------------------- */
typedef enum {
    TMP75B_OK           = 0,
    TMP75B_ERR_I2C      = 1,
    TMP75B_ERR_INVALID  = 2,
} tmp75b_status_t;

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

/* Configure all three sensors: 12-bit resolution, comparator mode */
tmp75b_status_t tmp75b_init_all(void);

/* Init a single sensor — useful if one is unresponsive at startup */
tmp75b_status_t tmp75b_init(tmp75b_sensor_t sensor);

/* Read temperature. Returns signed value in units of 1/16 degC.
   To get degrees: temp_raw / 16.0f  (or use tmp75b_to_mdegc below) */
tmp75b_status_t tmp75b_read_raw(tmp75b_sensor_t sensor, int16_t *raw_out);

/* Convert raw reading to millidegrees C (avoids float in application) */
int32_t tmp75b_to_mdegc(int16_t raw);

/* Set alert thresholds in millidegrees C.
   ALERT pin asserts (low) when temp >= t_high_mdegc,
   deasserts when temp < t_low_mdegc (with hysteresis). */
tmp75b_status_t tmp75b_set_limits(tmp75b_sensor_t sensor,
                                   int32_t t_low_mdegc,
                                   int32_t t_high_mdegc);

/* Called from HAL_GPIO_EXTI_Callback — records which sensor fired.
   Call tmp75b_get_alert_flags() from main loop to respond. */
void    tmp75b_alert_irq_handler(uint16_t gpio_pin);
uint8_t tmp75b_get_alert_flags(void);   /* Bit N = sensor N alerted */

#ifdef __cplusplus
}
#endif

#endif /* TMP75B_PORT_H */
