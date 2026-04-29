#include "ina3221_port.h"
#include "i2c.h"

/* HAL I2C uses 8-bit address (7-bit << 1) */
#define INA3221_ADDR_8BIT   (INA3221_I2C_ADDR << 1)
#define I2C_TIMEOUT_MS      25u

/* -----------------------------------------------------------------------
 * Internal: write a 16-bit register (MSB first)
 * --------------------------------------------------------------------- */
static ina3221_status_t reg_write(uint8_t reg, uint16_t value)
{
    uint8_t buf[3];
    buf[0] = reg;
    buf[1] = (uint8_t)(value >> 8);
    buf[2] = (uint8_t)(value & 0xFFu);
    if (HAL_I2C_Master_Transmit(&hi2c1, INA3221_ADDR_8BIT,
                                 buf, 3, I2C_TIMEOUT_MS) != HAL_OK)
    {
        return INA3221_ERR_I2C;
    }
    return INA3221_OK;
}

/* -----------------------------------------------------------------------
 * Internal: read a 16-bit register (MSB first)
 * --------------------------------------------------------------------- */
static ina3221_status_t reg_read(uint8_t reg, uint16_t *out)
{
    uint8_t buf[2];

    /* Set register pointer */
    if (HAL_I2C_Master_Transmit(&hi2c1, INA3221_ADDR_8BIT,
                                 &reg, 1, I2C_TIMEOUT_MS) != HAL_OK)
    {
        return INA3221_ERR_I2C;
    }

    /* Read two bytes */
    if (HAL_I2C_Master_Receive(&hi2c1, INA3221_ADDR_8BIT,
                                buf, 2, I2C_TIMEOUT_MS) != HAL_OK)
    {
        return INA3221_ERR_I2C;
    }

    *out = ((uint16_t)buf[0] << 8) | buf[1];
    return INA3221_OK;
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

ina3221_status_t ina3221_init(void)
{
    ina3221_status_t st;

    /* Write configuration register */
    st = reg_write(INA3221_REG_CONFIG, INA3221_CONFIG_VALUE);
    if (st != INA3221_OK) return st;

    /* Set power-valid upper limit: ~5.5V -> raw = (5500 << 3) / 8 = 0x1578
       Power-valid lower limit: ~4.5V -> 0x1194
       These gate the PV (power-valid) output pin. */
    st = reg_write(INA3221_REG_PV_UPPER, 0x1578u);
    if (st != INA3221_OK) return st;

    st = reg_write(INA3221_REG_PV_LOWER, 0x1194u);
    return st;
}

ina3221_status_t ina3221_read_channel(ina3221_channel_t ch,
                                       ina3221_reading_t *out)
{
    /* Register layout: shunt reg = 0x01 + ch*2, bus reg = 0x02 + ch*2 */
    uint8_t shunt_reg = INA3221_REG_CH1_SHUNT + (uint8_t)(ch * 2u);
    uint8_t bus_reg   = INA3221_REG_CH1_BUS   + (uint8_t)(ch * 2u);
    uint16_t raw;
    ina3221_status_t st;

    /* --- Bus voltage ---
       Bits [15:3] are the value, bits [2:0] unused.
       LSB = 8mV. Value is always positive. */
    st = reg_read(bus_reg, &raw);
    if (st != INA3221_OK) return st;
    out->bus_mv = (int32_t)((int16_t)raw >> 3) * 8;

    /* --- Shunt voltage ---
       Bits [15:3] are signed two's complement, bits [2:0] unused.
       LSB = 40uV. */
    st = reg_read(shunt_reg, &raw);
    if (st != INA3221_OK) return st;
    out->shunt_uv = (int32_t)((int16_t)raw >> 3) * 40;

    /* --- Current (derived) ---
       I = Vshunt / Rshunt. shunt_uv in uV, shunt in mohm -> uA.
       uA = uV * 1000 / mohm */
    uint32_t shunt_mohm;
    switch (ch)
    {
        case INA3221_CH1_5V0: shunt_mohm = INA3221_SHUNT_CH1_MOHM; break;
        case INA3221_CH2_3V3: shunt_mohm = INA3221_SHUNT_CH2_MOHM; break;
        case INA3221_CH3_1V8: shunt_mohm = INA3221_SHUNT_CH3_MOHM; break;
        default: shunt_mohm = 100u; break;
    }
    out->current_ua = (out->shunt_uv * 1000) / (int32_t)shunt_mohm;

    return INA3221_OK;
}

ina3221_status_t ina3221_read_mask(uint16_t *mask_out)
{
    /* Reading mask/enable also clears latched alert flags */
    return reg_read(INA3221_REG_MASK_EN, mask_out);
}

ina3221_status_t ina3221_set_crit_limit(ina3221_channel_t ch, uint16_t raw)
{
    uint8_t reg = INA3221_REG_CH1_CRIT + (uint8_t)(ch * 2u);
    return reg_write(reg, raw);
}

ina3221_status_t ina3221_set_warn_limit(ina3221_channel_t ch, uint16_t raw)
{
    uint8_t reg = INA3221_REG_CH1_WARN + (uint8_t)(ch * 2u);
    return reg_write(reg, raw);
}

uint16_t ina3221_ma_to_limit(uint32_t current_ma, uint32_t shunt_mohm)
{
    /* Vshunt (uV) = I (mA) * R (mohm) = current_ma * shunt_mohm
       Raw value = Vshunt_uV / 40uV, shifted left 3 bits for register format */
    uint32_t shunt_uv = current_ma * shunt_mohm;
    uint32_t raw = (shunt_uv / 40u) << 3;
    return (uint16_t)(raw & 0xFFF8u);
}
