#ifndef TELEMETRY_H
#define TELEMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "can_ctrl.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * How often to sample sensors and transmit a telemetry frame.
 * 100ms = 10Hz — adjust to suit your application.
 * --------------------------------------------------------------------- */
#define TELEMETRY_INTERVAL_MS   100u

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

/* Call once in USER CODE BEGIN 2 after all modules are initialized */
void telemetry_init(void);

/* Call every main loop iteration. Internally rate-limited by
   TELEMETRY_INTERVAL_MS — safe to call as fast as possible. */
void telemetry_process(void);

/* Returns a pointer to the last successfully assembled frame.
   Useful for local inspection or FRAM logging. NULL until first
   successful read. */
const can_telemetry_t *telemetry_get_last(void);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRY_H */
