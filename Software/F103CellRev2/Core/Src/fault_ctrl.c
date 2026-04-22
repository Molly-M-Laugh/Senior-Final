#include "fault_ctrl.h"
#include "power_ctrl.h"
#include "ina3221_port.h"
#include "tmp75b_port.h"
#include "can_ctrl.h"
#include "fram_port.h"
#include <string.h>

/* -----------------------------------------------------------------------
 * Module state
 * --------------------------------------------------------------------- */
static fault_tier_t  active_tier    = FAULT_TIER_NONE;
static uint32_t      active_sources = FAULT_SRC_NONE;

/* -----------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------- */

static uint8_t get_rail_state_byte(void)
{
    return (uint8_t)( (power_rail_is_enabled(POWER_RAIL_5V0) << 0)
                    | (power_rail_is_enabled(POWER_RAIL_3V3) << 1)
                    | (power_rail_is_enabled(POWER_RAIL_1V8) << 2) );
}

/* Pack fault sources and tier into the 9-byte payload field.
 * Layout:
 *   payload[0] = sources bits  [7:0]
 *   payload[1] = sources bits  [9:8]
 *   payload[2] = tier
 *   payload[3] = rail_states
 *   payload[4..8] = 0 (reserved)
 */
static void log_to_fram(uint32_t sources, fault_tier_t tier)
{
    uint8_t payload[9];
    memset(payload, 0, sizeof(payload));
    payload[0] = (uint8_t)(sources & 0xFFu);
    payload[1] = (uint8_t)((sources >> 8) & 0x03u);
    payload[2] = (uint8_t)tier;
    payload[3] = get_rail_state_byte();

    /* Map tier to log severity */
    log_severity_t sev;
    switch (tier)
    {
        case FAULT_TIER_EMERGENCY: sev = LOG_FAULT; break;
        case FAULT_TIER_CRITICAL:  sev = LOG_FAULT; break;
        case FAULT_TIER_WARNING:   sev = LOG_WARN;  break;
        default:                   sev = LOG_INFO;  break;
    }

    /* Map tier to log event type */
    log_event_t evt = LOG_EVT_POWER_FAULT; /* default — covers rail faults */
    if (sources & (FAULT_SRC_TEMP0_ALERT
                 | FAULT_SRC_TEMP1_ALERT
                 | FAULT_SRC_TEMP2_ALERT))
    {
        evt = LOG_EVT_TEMP_ALERT;
    }
    else if (sources & (FAULT_SRC_INA3221_WARN | FAULT_SRC_INA3221_CRIT))
    {
        evt = LOG_EVT_OVERCURRENT;
    }
    else if (sources & FAULT_SRC_CAN_FLT)
    {
        evt = LOG_EVT_CAN_ERROR;
    }

    fram_drv_log(sev, evt, payload, sizeof(payload));
}

static void send_can_fault_frame(uint32_t sources, fault_tier_t tier)
{
    can_telemetry_t fault_frame = {0};

    /* Pack sources bits [7:0] into fault_flags; tier into reserved byte */
    fault_frame.fault_flags = (uint8_t)(sources & 0xFFu);
    fault_frame.reserved    = (uint8_t)tier;

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
        /* TPS2421 latched off — disable EN to prevent re-enable attempts.
           Recovery requires a power cycle of the EN pin (high→low). */
        power_disable_rail(POWER_RAIL_5V0);
    }
    if (sources & FAULT_SRC_3V3_FLT)
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
    /* CAN transmit will likely fail after rail shutdown — attempt anyway
       in case the CAN transceiver is on a separate supply */
    send_can_fault_frame(sources, FAULT_TIER_EMERGENCY);
}

/* -----------------------------------------------------------------------
 * Classify collected sources into the highest applicable tier.
 * --------------------------------------------------------------------- */
static fault_tier_t classify(uint32_t sources)
{
    if (sources == FAULT_SRC_NONE) return FAULT_TIER_NONE;

    /* Emergency: two or more rail faults simultaneously, or all three
       temperature sensors alerting at once */
    uint32_t rail_faults = sources & (FAULT_SRC_5V0_FLT
                                    | FAULT_SRC_3V3_FLT
                                    | FAULT_SRC_1V8_FLT);
    uint32_t temp_alerts = sources & (FAULT_SRC_TEMP0_ALERT
                                    | FAULT_SRC_TEMP1_ALERT
                                    | FAULT_SRC_TEMP2_ALERT);
    uint8_t rail_count = (uint8_t)__builtin_popcount(rail_faults);
    uint8_t temp_count = (uint8_t)__builtin_popcount(temp_alerts);

    if (rail_count >= 2u || temp_count >= 3u)
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

void fault_ctrl_init(void)
{
    active_tier    = FAULT_TIER_NONE;
    active_sources = FAULT_SRC_NONE;
    FAULT_ALERT_DEASSERT();
}

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

    /* --- Collect from INA3221 ---
       Reading mask/enable register clears latched alert flags. */
    uint16_t ina_mask = 0;
    if (ina3221_read_mask(&ina_mask) == INA3221_OK)
    {
        if (ina_mask & (1u << 4)) sources |= FAULT_SRC_INA3221_WARN; /* WF bit */
        if (ina_mask & (1u << 5)) sources |= FAULT_SRC_INA3221_CRIT; /* CF bit */
    }

    /* --- Collect from TMP75B --- */
    uint8_t tmp = tmp75b_get_alert_flags();
    if (tmp & (1u << 0)) sources |= FAULT_SRC_TEMP0_ALERT;
    if (tmp & (1u << 1)) sources |= FAULT_SRC_TEMP1_ALERT;
    if (tmp & (1u << 2)) sources |= FAULT_SRC_TEMP2_ALERT;

    /* --- Collect from CAN --- */
    uint8_t can = can_ctrl_get_fault_flags();
    if (can & (1u << 0)) sources |= FAULT_SRC_CAN_FLT;

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

uint16_t fault_ctrl_read_log(fram_log_entry_t *buf, uint16_t max_entries)
{
    if (buf == NULL) return 0u;

    uint16_t total   = fram_drv_entry_count();
    uint16_t to_read = (total < max_entries) ? total : max_entries;

    for (uint16_t i = 0u; i < to_read; i++)
    {
        if (fram_drv_read_entry(i, &buf[i]) != FRAM_OK)
        {
            return i;   /* return count successfully read so far */
        }
    }
    return to_read;
}

void fault_ctrl_clear_log(void)
{
    fram_drv_format();
}

/* -----------------------------------------------------------------------
 * KNOWN LIMITATIONS / FUTURE WORK
 *
 * TPS2421 latch-off recovery:
 *   When the eFuse latches off after an overcurrent, the current code
 *   disables the rail and logs it only. A retry/recovery strategy
 *   should be added once the application policy is decided.
 *
 * INA3221 mask register:
 *   WF/CF bits are at positions 4 and 5 per the register map.
 *   Reading the mask register clears the latched alert flags —
 *   intentional and correct per the datasheet.
 *
 * CAN fault flag mapping:
 *   can_ctrl_get_fault_flags() bit 0 = ALRT1 (TX/bus error).
 *   Bit 1 (ALRT2) indicates RX not empty, not a fault — not collected here.
 * --------------------------------------------------------------------- */
