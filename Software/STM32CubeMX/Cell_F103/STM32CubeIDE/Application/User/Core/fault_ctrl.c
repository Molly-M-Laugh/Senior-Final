#include "fault_ctrl.h"
#include "power_ctrl.h"
#include "ina3221_port.h"
#include "tmp75b_port.h"
#include "can_ctrl.h"
#include "fram_port.h"

/* -----------------------------------------------------------------------
 * Module state
 * --------------------------------------------------------------------- */
static uint16_t      log_write_index  = 0;    /* Next FRAM record slot   */
static fault_tier_t  active_tier      = FAULT_TIER_NONE;
static uint32_t      active_sources   = FAULT_SRC_NONE;

/* -----------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------- */

static uint8_t get_rail_state_byte(void)
{
    return (uint8_t)( (power_rail_is_enabled(POWER_RAIL_5V0) << 0)
                    | (power_rail_is_enabled(POWER_RAIL_3V3) << 1)
                    | (power_rail_is_enabled(POWER_RAIL_1V8) << 2) );
}

static void log_to_fram(uint32_t sources, fault_tier_t tier)
{
    /* Ring buffer — wrap around when full, overwriting oldest record */
    if (log_write_index >= FAULT_LOG_MAX_RECORDS)
    {
        log_write_index = 0;
    }

    fault_log_record_t record = {
        .timestamp_ms  = HAL_GetTick(),
        .fault_sources = sources,
        .tier          = (uint8_t)tier,
        .rail_states   = get_rail_state_byte(),
        .reserved      = {0, 0},
    };

    uint16_t addr = FAULT_LOG_FRAM_BASE
                  + (log_write_index * FAULT_LOG_RECORD_SIZE);
    fram_write(addr, (uint8_t *)&record, sizeof(record));
    log_write_index++;
}

static void send_can_fault_frame(uint32_t sources, fault_tier_t tier)
{
    can_telemetry_t fault_frame = {0};

    /* Repurpose fault_flags and alert_flags fields for fault reporting.
       Full telemetry packing happens in the normal telemetry path. */
    fault_frame.fault_flags = (uint8_t)(sources & 0xFFu);
    fault_frame.alert_flags = (uint8_t)tier;

    /* Best-effort — don't block on TX failure during fault handling */
    can_ctrl_transmit(&fault_frame);
}

static void handle_tier1_warning(uint32_t sources)
{
    log_to_fram(sources, FAULT_TIER_WARNING);
    send_can_fault_frame(sources, FAULT_TIER_WARNING);
    /* Keep running — no rail action needed */
}

static void handle_tier2_critical(uint32_t sources)
{
    /* Disable only the faulted rail rather than full shutdown */
    if (sources & FAULT_SRC_5V0_FLT)
    {
        /* 5V eFuse latched off — disabling EN won't reset it, but it
           prevents re-enable attempts. Full latch-off recovery requires
           power cycle of the TPS2421 EN pin (toggle high->low). */
        power_disable_rail(POWER_RAIL_5V0);
    }
    if (sources & (FAULT_SRC_3V3_FLT))
    {
        power_disable_rail(POWER_RAIL_3V3);
    }
    if (sources & FAULT_SRC_1V8_FLT)
    {
        power_disable_rail(POWER_RAIL_1V8);
    }

    FAULT_ALERT_ASSERT();    /* Notify ESP32 */
    log_to_fram(sources, FAULT_TIER_CRITICAL);
    send_can_fault_frame(sources, FAULT_TIER_CRITICAL);
}

static void handle_tier3_emergency(uint32_t sources)
{
    /* Ordered rail shutdown — oscillator and all rails off */
    power_sequence_down();

    FAULT_ALERT_ASSERT();
    log_to_fram(sources, FAULT_TIER_EMERGENCY);
    /* CAN transmit will fail after rail shutdown — attempt anyway
       in case the CAN transceiver is on a separate supply */
    send_can_fault_frame(sources, FAULT_TIER_EMERGENCY);
}

/* -----------------------------------------------------------------------
 * Classify collected sources into the highest applicable tier.
 * --------------------------------------------------------------------- */
static fault_tier_t classify(uint32_t sources)
{
    if (sources == FAULT_SRC_NONE) return FAULT_TIER_NONE;

    /* Emergency: multiple rail faults simultaneously, or all three
       temperature sensors alerting at once */
    uint32_t rail_faults = sources & (FAULT_SRC_5V0_FLT
                                    | FAULT_SRC_3V3_FLT
                                    | FAULT_SRC_1V8_FLT);
    uint32_t temp_alerts = sources & (FAULT_SRC_TEMP0_ALERT
                                    | FAULT_SRC_TEMP1_ALERT
                                    | FAULT_SRC_TEMP2_ALERT);
    uint8_t rail_count = __builtin_popcount(rail_faults);
    uint8_t temp_count = __builtin_popcount(temp_alerts);

    if (rail_count >= 2 || temp_count >= 3)
    {
        return FAULT_TIER_EMERGENCY;
    }

    /* Critical: any single rail fault, INA3221 critical, PG lost */
    if (sources & (FAULT_SRC_5V0_FLT
                 | FAULT_SRC_3V3_FLT
                 | FAULT_SRC_1V8_FLT
                 | FAULT_SRC_5V0_PG_LOST
                 | FAULT_SRC_INA3221_CRIT))
    {
        return FAULT_TIER_CRITICAL;
    }

    /* Warning: soft threshold crossings, single temp alert, CAN fault */
    if (sources & (FAULT_SRC_INA3221_WARN
                 | FAULT_SRC_TEMP0_ALERT
                 | FAULT_SRC_TEMP1_ALERT
                 | FAULT_SRC_TEMP2_ALERT
                 | FAULT_SRC_CAN_FLT))
    {
        return FAULT_TIER_WARNING;
    }

    return FAULT_TIER_NONE;
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */
uint16_t fault_ctrl_read_log(fault_log_record_t *buf, uint16_t max_records)
{
    uint16_t count = (log_write_index < max_records)
                   ? log_write_index : max_records;

    for (uint16_t i = 0; i < count; i++)
    {
        uint16_t addr = FAULT_LOG_FRAM_BASE + (i * FAULT_LOG_RECORD_SIZE);
        fram_read(addr, (uint8_t *)&buf[i], FAULT_LOG_RECORD_SIZE);
    }

    return count;
}

void fault_ctrl_clear_log(void)
{
    uint8_t blank[FAULT_LOG_RECORD_SIZE] = {0};
    for (uint16_t i = 0; i < FAULT_LOG_MAX_RECORDS; i++)
    {
        uint16_t addr = FAULT_LOG_FRAM_BASE + (i * FAULT_LOG_RECORD_SIZE);
        fram_write(addr, blank, FAULT_LOG_RECORD_SIZE);
    }
    log_write_index = 0;
}


void fault_ctrl_init(void)
{
    log_write_index = 0;
    active_tier     = FAULT_TIER_NONE;
    active_sources  = FAULT_SRC_NONE;
    FAULT_ALERT_DEASSERT();
}
/* -----------------------------------------------------------------------
 * KNOWN LIMITATIONS / FUTURE WORK
 *
 * TPS2421 latch-off recovery:
 *   When the eFuse latches off after an overcurrent, toggling EN low
 *   again will not immediately re-enable it. The device requires EN to
 *   go high then low again after the fault clears. The current code
 *   disables the rail and logs it only — a retry/recovery strategy
 *   should be added once the application policy is decided.
 *
 * FRAM log full:
 *   Once 64 records are written the log silently stops recording.
 *   A production implementation should either wrap around as a ring
 *   buffer or overwrite the oldest record.
 *
 * INA3221 mask register:
 *   WF/CF bits are at positions 4 and 5 per the register map.
 *   Reading the mask register clears the latched alert flags —
 *   this is intentional and correct per the datasheet.
 * --------------------------------------------------------------------- */
void fault_ctrl_process(void)
{
    uint32_t sources = FAULT_SRC_NONE;

    /* --- Collect from power module --- */
    uint8_t pwr = power_get_fault_flags();
    if (pwr & (1u << 0)) sources |= FAULT_SRC_5V0_FLT;
    if (pwr & (1u << 1)) sources |= FAULT_SRC_3V3_FLT;
    if (pwr & (1u << 2)) sources |= FAULT_SRC_1V8_FLT;

    /* PG5V0 drop — polled here since EXTI only records FLT lines */
    if (!power_pg_5v0() && power_rail_is_enabled(POWER_RAIL_5V0))
    {
        sources |= FAULT_SRC_5V0_PG_LOST;
    }

    /* --- Collect from INA3221 --- */
    /* INA3221 alert flags are set by EXTI and read from the mask register.
       Here we read the mask register to get the current state. */
    uint16_t ina_mask = 0;
    if (ina3221_read_mask(&ina_mask) == INA3221_OK)
    {
        /* Bit 4 (WF) = any warning flag set */
        if (ina_mask & (1u << 4)) sources |= FAULT_SRC_INA3221_WARN;
        /* Bit 5 (CF) = any critical flag set */
        if (ina_mask & (1u << 5)) sources |= FAULT_SRC_INA3221_CRIT;
    }

    /* --- Collect from TMP75B --- */
    uint8_t tmp = tmp75b_get_alert_flags();
    if (tmp & (1u << 0)) sources |= FAULT_SRC_TEMP0_ALERT;
    if (tmp & (1u << 1)) sources |= FAULT_SRC_TEMP1_ALERT;
    if (tmp & (1u << 2)) sources |= FAULT_SRC_TEMP2_ALERT;

    /* --- Collect from CAN --- */
    uint8_t can = can_ctrl_get_fault_flags();
    if (can & (1u << 2)) sources |= FAULT_SRC_CAN_FLT;

    /* Nothing to do */
    if (sources == FAULT_SRC_NONE)
    {
        active_tier    = FAULT_TIER_NONE;
        active_sources = FAULT_SRC_NONE;
        return;
    }

    active_sources = sources;
    active_tier    = classify(sources);

    switch (active_tier)
    {
        case FAULT_TIER_WARNING:   handle_tier1_warning(sources);   break;
        case FAULT_TIER_CRITICAL:  handle_tier2_critical(sources);  break;
        case FAULT_TIER_EMERGENCY: handle_tier3_emergency(sources); break;
        default: break;
    }
}

fault_tier_t fault_ctrl_get_active_tier(void)
{
    return active_tier;
}

uint32_t fault_ctrl_get_active_sources(void)
{
    return active_sources;
}
