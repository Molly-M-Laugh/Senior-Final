#ifndef INA3221_PORT_H
#define INA3221_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * I2C address — A0 pin tied to GND (verify during bench scan)
 * --------------------------------------------------------------------- */
#define INA3221_I2C_ADDR        0x40u

/* -----------------------------------------------------------------------
 * Register addresses (INA3221 datasheet Table 7-4)
 * --------------------------------------------------------------------- */
#define INA3221_REG_CONFIG      0x00u
#define INA3221_REG_CH1_SHUNT   0x01u
#define INA3221_REG_CH1_BUS     0x02u
#define INA3221_REG_CH2_SHUNT   0x03u
#define INA3221_REG_CH2_BUS     0x04u
#define INA3221_REG_CH3_SHUNT   0x05u
#define INA3221_REG_CH3_BUS     0x06u
#define INA3221_REG_CH1_CRIT    0x07u
#define INA3221_REG_CH1_WARN    0x08u
#define INA3221_REG_CH2_CRIT    0x09u
#define INA3221_REG_CH2_WARN    0x0Au
#define INA3221_REG_CH3_CRIT    0x0Bu
#define INA3221_REG_CH3_WARN    0x0Cu
#define INA3221_REG_MASK_EN     0x0Fu
#define INA3221_REG_PV_UPPER    0x10u
#define INA3221_REG_PV_LOWER    0x11u

/* -----------------------------------------------------------------------
 * Config register value used during init.
 *
 * Bits [15]    RST  = 0  (no reset)
 * Bits [14:12] CH en = 111 (all three channels enabled)
 * Bits [11:9]  AVG  = 001 (4 samples average — reduces noise)
 * Bits [8:6]   VBUS CT = 010 (1.1ms bus voltage conversion)
 * Bits [5:3]   VSH CT  = 010 (1.1ms shunt voltage conversion)
 * Bits [2:0]   MODE = 111 (shunt and bus, continuous)
 *
 * = 0b0111_0010_1001_0111 = 0x7297
 *
 * Note: default config (0x7127) assumes higher voltage rails.
 * This value reconfigures averaging and conversion time for
 * accurate readings on sub-10V rails.
 * --------------------------------------------------------------------- */
#define INA3221_CONFIG_VALUE    0x7297u

/* -----------------------------------------------------------------------
 * Channel mapping (schematic: CH1=5V0, CH2=3V3, CH3=1V8)
 * --------------------------------------------------------------------- */
typedef enum {
    INA3221_CH1_5V0 = 0,
    INA3221_CH2_3V3 = 1,
    INA3221_CH3_1V8 = 2,
} ina3221_channel_t;

/* -----------------------------------------------------------------------
 * Shunt resistor values in milliohms (update to match BOB)
 * --------------------------------------------------------------------- */
#define INA3221_SHUNT_CH1_MOHM  100u
#define INA3221_SHUNT_CH2_MOHM  100u
#define INA3221_SHUNT_CH3_MOHM  100u

/* -----------------------------------------------------------------------
 * Result struct
 * --------------------------------------------------------------------- */
typedef struct {
    int32_t  voltage_mv;    /* Bus voltage in millivolts  */
    int32_t  shunt_uv;  /* Shunt voltage in microvolts */
    int32_t  current_ma;/* Derived current in MILLIamps */
} ina3221_reading_t;

/* -----------------------------------------------------------------------
 * Return type
 * --------------------------------------------------------------------- */
typedef enum {
    INA3221_OK          = 0,
    INA3221_ERR_I2C     = 1,
    INA3221_ERR_TIMEOUT = 2,
} ina3221_status_t;

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

/* Write config register and power-valid limits for sub-10V operation */
ina3221_status_t ina3221_init(void);

/* Read bus voltage and shunt voltage for one channel, derive current */
ina3221_status_t ina3221_read_channel(ina3221_channel_t ch,
                                       ina3221_reading_t *out);

/* Read the mask/enable register to check which alert flags are set.
   Caller inspects bits — see INA3221_REG_MASK_EN bit definitions below */
ina3221_status_t ina3221_read_mask(uint16_t *mask_out);

/* Set a critical or warning shunt limit for a channel (raw register value).
   Use ina3221_amps_to_limit() to convert from real current. */
ina3221_status_t ina3221_set_crit_limit(ina3221_channel_t ch, uint16_t raw);
ina3221_status_t ina3221_set_warn_limit(ina3221_channel_t ch, uint16_t raw);

/* Convert a current in milliamps and shunt in milliohms to a raw limit
   register value for use with ina3221_set_crit_limit / set_warn_limit */
uint16_t ina3221_ma_to_limit(uint32_t current_ma, uint32_t shunt_mohm);

#ifdef __cplusplus
}
#endif

#endif /* INA3221_PORT_H */
