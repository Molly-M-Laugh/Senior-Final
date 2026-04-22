#include "telemetry.h"
#include "ina3221_port.h"
#include "tmp75b_port.h"
#include "fault_ctrl.h"
#include "can_ctrl.h"

static uint32_t        last_tx_tick = 0;
static can_telemetry_t last_frame   = {0};
static uint8_t         frame_valid  = 0;

/* -----------------------------------------------------------------------
 * Internal: read all sensors into a telemetry frame.
 * Partial failures are tolerated — zero is left in place for any
 * channel that fails so the frame is still transmitted with whatever
 * data is available.
 * --------------------------------------------------------------------- */
static void assemble_frame(can_telemetry_t *frame)
{
    /* --- INA3221 --- */
    ina3221_reading_t reading;
    for (int ch = INA3221_CH1_5V0; ch <= INA3221_CH3_1V8; ch++)
    {
        if (ina3221_read_channel((ina3221_channel_t)ch, &reading) == INA3221_OK)
        {
            frame->voltage_mv[ch] = (uint16_t)reading.voltage_mv;
            frame->current_ma[ch] = (uint16_t)reading.current_ma;
        }
        else
        {
            frame->voltage_mv[ch] = 0;
            frame->current_ma[ch] = 0;
        }
    }

    /* --- TMP75B --- */
    int16_t raw_temp;
    for (int s = TMP75B_SENSOR_0; s < TMP75B_SENSOR_COUNT; s++)
    {
        if (tmp75b_read_raw((tmp75b_sensor_t)s, &raw_temp) == TMP75B_OK)
        {
            /* Store in 0.01 degC units: mdegC / 10 */
            frame->temp_c[s] = (int16_t)(tmp75b_to_mdegc(raw_temp) / 10);
        }
        else
        {
            frame->temp_c[s] = 0;
        }
    }

    /* --- Fault state snapshot ---
       Active sources from last fault_ctrl_process() — not cleared here.
       Pack low 8 bits into fault_flags; tier into reserved byte. */
    uint32_t sources = fault_ctrl_get_active_sources();
    frame->fault_flags = (uint8_t)(sources & 0xFFu);
    frame->reserved    = (uint8_t)((uint8_t)fault_ctrl_get_active_tier());
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

void telemetry_init(void)
{
    last_tx_tick = HAL_GetTick();
    frame_valid  = 0;
}

void telemetry_process(void)
{
    uint32_t now = HAL_GetTick();

    /* Rate limit — only sample and transmit at TELEMETRY_INTERVAL_MS */
    if ((now - last_tx_tick) < TELEMETRY_INTERVAL_MS) return;
    last_tx_tick = now;

    assemble_frame(&last_frame);
    frame_valid = 1;

    /* Transmit — log but don't halt on TX failure */
    can_ctrl_transmit(&last_frame);
}

const can_telemetry_t *telemetry_get_last(void)
{
    return frame_valid ? &last_frame : NULL;
}
