#include "tmp75b_port.h"
#include "i2c.h"

#define I2C_TIMEOUT_MS  25u

#define ALERT_FLAG_0    (1u << 0)
#define ALERT_FLAG_1    (1u << 1)
#define ALERT_FLAG_2    (1u << 2)

static volatile uint8_t alert_flags = 0;

/* Lookup from sensor index to 8-bit I2C address */
static const uint8_t sensor_addr[TMP75B_SENSOR_COUNT] = {
    TMP75B_ADDR_TEMP0 << 1,
    TMP75B_ADDR_TEMP1 << 1,
    TMP75B_ADDR_TEMP2 << 1,
};

/* -----------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------- */

static tmp75b_status_t reg_write_u8(tmp75b_sensor_t sensor,
                                     uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = { reg, value };
    if (HAL_I2C_Master_Transmit(&hi2c1, sensor_addr[sensor],
                                 buf, 2, I2C_TIMEOUT_MS) != HAL_OK)
    {
        return TMP75B_ERR_I2C;
    }
    return TMP75B_OK;
}

static tmp75b_status_t reg_write_u16(tmp75b_sensor_t sensor,
                                      uint8_t reg, uint16_t value)
{
    uint8_t buf[3];
    buf[0] = reg;
    buf[1] = (uint8_t)(value >> 8);
    buf[2] = (uint8_t)(value & 0xFFu);
    if (HAL_I2C_Master_Transmit(&hi2c1, sensor_addr[sensor],
                                 buf, 3, I2C_TIMEOUT_MS) != HAL_OK)
    {
        return TMP75B_ERR_I2C;
    }
    return TMP75B_OK;
}

static tmp75b_status_t reg_read_u16(tmp75b_sensor_t sensor,
                                     uint8_t reg, uint16_t *out)
{
    uint8_t buf[2];
    if (HAL_I2C_Master_Transmit(&hi2c1, sensor_addr[sensor],
                                 &reg, 1, I2C_TIMEOUT_MS) != HAL_OK)
    {
        return TMP75B_ERR_I2C;
    }
    if (HAL_I2C_Master_Receive(&hi2c1, sensor_addr[sensor],
                                buf, 2, I2C_TIMEOUT_MS) != HAL_OK)
    {
        return TMP75B_ERR_I2C;
    }
    *out = ((uint16_t)buf[0] << 8) | buf[1];
    return TMP75B_OK;
}

/* -----------------------------------------------------------------------
 * Convert millidegrees C to the 12-bit TMP75B limit register format.
 * Register bits [15:4] are the signed temp in 1/16 degC steps, [3:0] = 0.
 * --------------------------------------------------------------------- */
static uint16_t mdegc_to_reg(int32_t mdegc)
{
    /* raw units = mdegc / 62.5 = mdegc * 16 / 1000 */
    int16_t raw = (int16_t)((mdegc * 16) / 1000);
    return (uint16_t)((uint16_t)raw << 4);
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

tmp75b_status_t tmp75b_init(tmp75b_sensor_t sensor)
{
    if (sensor >= TMP75B_SENSOR_COUNT) return TMP75B_ERR_INVALID;
    return reg_write_u8(sensor, TMP75B_REG_CONFIG, TMP75B_CONFIG_VALUE);
}

tmp75b_status_t tmp75b_init_all(void)
{
    tmp75b_status_t st;
    for (tmp75b_sensor_t s = TMP75B_SENSOR_0; s < TMP75B_SENSOR_COUNT; s++)
    {
        st = tmp75b_init(s);
        if (st != TMP75B_OK) return st;
    }
    return TMP75B_OK;
}

tmp75b_status_t tmp75b_read_raw(tmp75b_sensor_t sensor, int16_t *raw_out)
{
    if (sensor >= TMP75B_SENSOR_COUNT) return TMP75B_ERR_INVALID;

    uint16_t raw;
    tmp75b_status_t st = reg_read_u16(sensor, TMP75B_REG_TEMP, &raw);
    if (st != TMP75B_OK) return st;

    /* Bits [15:4] are signed temp in 1/16 degC; shift down to get raw value */
    *raw_out = (int16_t)raw >> 4;
    return TMP75B_OK;
}

int32_t tmp75b_to_mdegc(int16_t raw)
{
    /* Each LSB = 0.0625 degC = 62.5 mdegC
       Multiply by 1000 then divide by 16 to stay in integer math */
    return ((int32_t)raw * 1000) / 16;
}

tmp75b_status_t tmp75b_set_limits(tmp75b_sensor_t sensor,
                                   int32_t t_low_mdegc,
                                   int32_t t_high_mdegc)
{
    if (sensor >= TMP75B_SENSOR_COUNT) return TMP75B_ERR_INVALID;

    tmp75b_status_t st;
    st = reg_write_u16(sensor, TMP75B_REG_TLOW,  mdegc_to_reg(t_low_mdegc));
    if (st != TMP75B_OK) return st;
    st = reg_write_u16(sensor, TMP75B_REG_THIGH, mdegc_to_reg(t_high_mdegc));
    return st;
}

void tmp75b_alert_irq_handler(uint16_t gpio_pin)
{
    if      (gpio_pin == TEMP0_ALERT_Pin) alert_flags |= ALERT_FLAG_0;
    else if (gpio_pin == TEMP1_ALERT_Pin) alert_flags |= ALERT_FLAG_1;
    else if (gpio_pin == TEMP2_ALERT_Pin) alert_flags |= ALERT_FLAG_2;
}

uint8_t tmp75b_get_alert_flags(void)
{
    uint8_t flags = alert_flags;
    alert_flags = 0;
    return flags;
}
