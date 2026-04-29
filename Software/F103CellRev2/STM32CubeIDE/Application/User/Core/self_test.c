#include "self_test.h"
#include "i2c.h"
#include "fram_port.h"
#include "tmp75b_port.h"
#include "ina3221_port.h"

/* -----------------------------------------------------------------------
 * FRAM address for self-test result byte.
 *
 * fram_port owns 0x0000–0x0007 (metadata) and the circular log starts
 * at 0x0008. The self-test result is a single byte stored at 0xFF00,
 * well above the log region (max log end = 0x0008 + 4095*16 = 0xFFF8).
 * 0xFF00 is the last clean page of the address space.
 * --------------------------------------------------------------------- */
#define SELF_TEST_FRAM_ADDR     0xFF00u

#define I2C_PING_TIMEOUT_MS     10u
#define I2C_PING_ATTEMPTS       2u

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

    if (!ping(INA3221_I2C_ADDR))  result |= SELF_TEST_INA3221;
    if (!ping(TMP75B_ADDR_TEMP0)) result |= SELF_TEST_TMP75B_0;
    if (!ping(TMP75B_ADDR_TEMP1)) result |= SELF_TEST_TMP75B_1;
    if (!ping(TMP75B_ADDR_TEMP2)) result |= SELF_TEST_TMP75B_2;
    if (!ping(FRAM_I2C_ADDR))     result |= SELF_TEST_FRAM;

    /* Log result to FRAM if FRAM itself passed — single raw byte at 0xFF00 */
    if (!(result & SELF_TEST_FRAM))
    {
        fram_drv_write_raw(SELF_TEST_FRAM_ADDR, &result, 1u);
    }

    return result;
}

uint8_t self_test_passed(self_test_result_t result)
{
    return (result == 0u) ? 1u : 0u;
}
