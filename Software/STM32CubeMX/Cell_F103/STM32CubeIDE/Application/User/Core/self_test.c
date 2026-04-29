#include "self_test.h"
#include "i2c.h"
#include "fram_port.h"
#include "tmp75b_port.h"
#include "ina3221_port.h"
#include "fault_ctrl.h"
/* FRAM address where the last self-test result is stored.
   Placed just above the fault log. */
#define SELF_TEST_FRAM_ADDR  (FAULT_LOG_FRAM_BASE               \
                             + FAULT_LOG_MAX_RECORDS             \
                             * FAULT_LOG_RECORD_SIZE)            /* = 0x0300 */

#define I2C_PING_TIMEOUT_MS  10u
#define I2C_PING_ATTEMPTS    2u

static uint8_t ping(uint8_t addr_7bit)
{
    return (HAL_I2C_IsDeviceReady(&hi2c1,
                                   (uint8_t)(addr_7bit << 1),
                                   I2C_PING_ATTEMPTS,
                                   I2C_PING_TIMEOUT_MS) == HAL_OK) ? 1u : 0u;
}

self_test_result_t self_test_run(void)
{
    self_test_result_t result = 0;

    if (!ping(INA3221_I2C_ADDR))      result |= SELF_TEST_INA3221;
    if (!ping(TMP75B_ADDR_TEMP0))     result |= SELF_TEST_TMP75B_0;
    if (!ping(TMP75B_ADDR_TEMP1))     result |= SELF_TEST_TMP75B_1;
    if (!ping(TMP75B_ADDR_TEMP2))     result |= SELF_TEST_TMP75B_2;
    if (!ping(FRAM_BASE_ADDR))        result |= SELF_TEST_FRAM;

    /* Log result to FRAM if FRAM itself passed */
    if (!(result & SELF_TEST_FRAM))
    {
        fram_write_byte(SELF_TEST_FRAM_ADDR, result);
    }

    return result;
}

uint8_t self_test_passed(self_test_result_t result)
{
    return (result == 0) ? 1u : 0u;
}
