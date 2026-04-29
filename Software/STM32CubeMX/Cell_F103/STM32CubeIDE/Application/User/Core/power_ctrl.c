
#include "power_ctrl.h"
#include "gpio.h"

#define POWER_PG_POLL_TIMEOUT_MS  200u
#define POWER_PG_POLL_INTERVAL_MS   1u
#define POWER_1V8_SETTLE_MS        10u
#define POWER_OSC_SETTLE_MS         2u

#define FAULT_FLAG_5V0  (1u << 0)
#define FAULT_FLAG_3V3  (1u << 1)
#define FAULT_FLAG_1V8  (1u << 2)

static volatile uint8_t fault_flags = 0;

static void enable_5v0(void)  { HAL_GPIO_WritePin(EN5V0_GPIO_Port,  EN5V0_Pin,  GPIO_PIN_RESET); }
static void disable_5v0(void) { HAL_GPIO_WritePin(EN5V0_GPIO_Port,  EN5V0_Pin,  GPIO_PIN_SET);   }
static void enable_3v3(void)  { HAL_GPIO_WritePin(EN3V3_GPIO_Port,  EN3V3_Pin,  GPIO_PIN_RESET); }
static void disable_3v3(void) { HAL_GPIO_WritePin(EN3V3_GPIO_Port,  EN3V3_Pin,  GPIO_PIN_SET);   }
static void enable_1v8(void)  { HAL_GPIO_WritePin(ON1V8_GPIO_Port,  ON1V8_Pin,  GPIO_PIN_SET);   }
static void disable_1v8(void) { HAL_GPIO_WritePin(ON1V8_GPIO_Port,  ON1V8_Pin,  GPIO_PIN_RESET); }
static void enable_osc(void)  { HAL_GPIO_WritePin(OSC_EN_GPIO_Port, OSC_EN_Pin, GPIO_PIN_SET);   }
static void disable_osc(void) { HAL_GPIO_WritePin(OSC_EN_GPIO_Port, OSC_EN_Pin, GPIO_PIN_RESET); }

static uint8_t poll_pin_until(GPIO_TypeDef *port, uint16_t pin,
                               GPIO_PinState expected, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < timeout_ms)
    {
        if (HAL_GPIO_ReadPin(port, pin) == expected)
            return 1;
        HAL_Delay(POWER_PG_POLL_INTERVAL_MS);
    }
    return 0;
}

power_result_t power_reset_efuse(power_rail_t rail)
{
    /* Only TPS2421 rails support this — 1V8 uses TPS22950x */
    if (rail == POWER_RAIL_1V8) return POWER_ERR_FAULT;

    /* Step 1: drive EN high to deassert (already off, but be explicit) */
    switch (rail)
    {
        case POWER_RAIL_5V0: disable_5v0(); break;
        case POWER_RAIL_3V3: disable_3v3(); break;
        default: return POWER_ERR_FAULT;
    }

    /* Step 2: wait for internal capacitor to discharge.
       TPS2421 CT capacitor sets the retry delay — 10ms is conservative. */
    HAL_Delay(10u);

    /* Step 3: re-enable */
    switch (rail)
    {
        case POWER_RAIL_5V0: enable_5v0(); break;
        case POWER_RAIL_3V3: enable_3v3(); break;
        default: break;
    }

    /* Step 4: wait for PG to reassert */
    if (rail == POWER_RAIL_5V0)
    {
        if (!poll_pin_until(PG5V0_GPIO_Port, PG5V0_Pin,
                            GPIO_PIN_SET, POWER_PG_POLL_TIMEOUT_MS))
        {
            disable_5v0();
            return POWER_ERR_5V_TIMEOUT;
        }
    }
    else if (rail == POWER_RAIL_3V3)
    {
        if (!poll_pin_until(PG3V3_GPIO_Port, PG3V3_Pin,
                            GPIO_PIN_SET, POWER_PG_POLL_TIMEOUT_MS))
        {
            disable_3v3();
            return POWER_ERR_3V3_TIMEOUT;
        }
    }

    return POWER_OK;
}

power_result_t power_sequence_up(void)
{
    enable_5v0();
    if (!poll_pin_until(PG5V0_GPIO_Port, PG5V0_Pin, GPIO_PIN_SET, POWER_PG_POLL_TIMEOUT_MS))
    {
        disable_5v0();
        return POWER_ERR_5V_TIMEOUT;
    }

    enable_3v3();
    if (!poll_pin_until(PG3V3_GPIO_Port, PG3V3_Pin, GPIO_PIN_SET, POWER_PG_POLL_TIMEOUT_MS))
    {
        disable_3v3();
        disable_5v0();
        return POWER_ERR_3V3_TIMEOUT;
    }

    enable_1v8();
    HAL_Delay(POWER_1V8_SETTLE_MS);

    enable_osc();
    HAL_Delay(POWER_OSC_SETTLE_MS);

    return POWER_OK;
}

void power_sequence_down(void)
{
    disable_osc();
    disable_1v8();
    HAL_Delay(POWER_1V8_SETTLE_MS);
    disable_3v3();
    HAL_Delay(POWER_PG_POLL_INTERVAL_MS);
    disable_5v0();
}

void power_disable_rail(power_rail_t rail)
{
    switch (rail)
    {
        case POWER_RAIL_5V0: disable_5v0(); break;
        case POWER_RAIL_3V3: disable_3v3(); break;
        case POWER_RAIL_1V8: disable_1v8(); break;
        default: break;
    }
}

uint8_t power_rail_is_enabled(power_rail_t rail)
{
    GPIO_PinState state;
    switch (rail)
    {
        case POWER_RAIL_5V0:
            state = HAL_GPIO_ReadPin(EN5V0_GPIO_Port, EN5V0_Pin);
            return (state == GPIO_PIN_RESET) ? 1u : 0u;
        case POWER_RAIL_3V3:
            state = HAL_GPIO_ReadPin(EN3V3_GPIO_Port, EN3V3_Pin);
            return (state == GPIO_PIN_RESET) ? 1u : 0u;
        case POWER_RAIL_1V8:
            state = HAL_GPIO_ReadPin(ON1V8_GPIO_Port, ON1V8_Pin);
            return (state == GPIO_PIN_SET) ? 1u : 0u;
        default:
            return 0u;
    }
}

uint8_t power_pg_5v0(void)
{
    return (HAL_GPIO_ReadPin(PG5V0_GPIO_Port, PG5V0_Pin) == GPIO_PIN_SET) ? 1u : 0u;
}

uint8_t power_pg_3v3(void)
{
    return (HAL_GPIO_ReadPin(PG3V3_GPIO_Port, PG3V3_Pin) == GPIO_PIN_SET) ? 1u : 0u;
}

void power_fault_irq_handler(uint16_t gpio_pin)
{
    if      (gpio_pin == FLT_5V0_Pin) fault_flags |= FAULT_FLAG_5V0;
    else if (gpio_pin == FLT_3V3_Pin) fault_flags |= FAULT_FLAG_3V3;
    else if (gpio_pin == FLT_1V8_Pin) fault_flags |= FAULT_FLAG_1V8;
}

uint8_t power_get_fault_flags(void)
{
    uint8_t flags = fault_flags;
    fault_flags = 0;
    return flags;
}
