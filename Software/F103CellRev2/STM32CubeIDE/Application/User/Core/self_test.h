#ifndef SELF_TEST_H
#define SELF_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * One bit per device — set if that device failed to respond at startup
 * --------------------------------------------------------------------- */
#define SELF_TEST_INA3221   (1u << 0)
#define SELF_TEST_TMP75B_0  (1u << 1)
#define SELF_TEST_TMP75B_1  (1u << 2)
#define SELF_TEST_TMP75B_2  (1u << 3)
#define SELF_TEST_FRAM      (1u << 4)

typedef uint8_t self_test_result_t;   /* OR of SELF_TEST_* bits, 0 = all pass */

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

/* Ping all 5 I2C devices and return a bitmask of failures.
   Call after power_sequence_up() and before any driver init.
   Result is also written to FRAM address SELF_TEST_FRAM_ADDR. */
self_test_result_t self_test_run(void);

/* Human-readable: returns 1 if all devices passed */
uint8_t self_test_passed(self_test_result_t result);

#ifdef __cplusplus
}
#endif

#endif /* SELF_TEST_H */
