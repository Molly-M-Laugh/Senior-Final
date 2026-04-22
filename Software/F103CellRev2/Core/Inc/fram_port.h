#ifndef FRAM_PORT_H
#define FRAM_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* =======================================================================
 * FM24V05 hardware constants
 *
 * A2=0, A1=1, A0=1 → slave address = 0b1010_011 = 0x53
 * Device ID address = 0b1111_101_1 (R) = 0xF7 / 0b1111_100_0 (W) = 0xF0
 * The JEDEC device ID read uses a special two-step sequence.
 *
 * Memory map: 0x0000–0xFFFF (64KB, 65536 bytes)
 * WP pin: PA15, active HIGH (write-protect when HIGH)
 * ===================================================================== */
#define FRAM_I2C_ADDR           0x53u   /* 7-bit address                 */
#define FRAM_DEV_ID_ADDR        0xF8u   /* reserved slave ID for Dev ID  */
#define FRAM_CAPACITY_BYTES     65536u  /* 64KB                          */
#define FRAM_I2C_TIMEOUT_MS     25u

/* =======================================================================
 * Write protect GPIO — PA15
 * *** CubeMX macro names — confirm against your ioc if renamed ***
 * ===================================================================== */
#define FRAM_WP_GPIO_PORT       GPIOA
#define FRAM_WP_PIN             GPIO_PIN_15

/* =======================================================================
 * Log entry layout
 *
 * 16 bytes per entry → 4096 entries in 64KB.
 * Circular buffer metadata occupies the first 8 bytes (0x0000–0x0007).
 * Log entries begin at FRAM_LOG_BASE (0x0008).
 *
 * CRC: simple XOR of bytes 0–14 (all fields except crc itself).
 * ===================================================================== */
#define FRAM_META_BASE          0x0000u /* metadata region start         */
#define FRAM_LOG_BASE           0x0008u /* first log entry address       */
#define FRAM_LOG_ENTRY_SIZE     16u     /* bytes per entry               */
#define FRAM_LOG_MAX_ENTRIES    ((FRAM_CAPACITY_BYTES - FRAM_LOG_BASE) \
                                 / FRAM_LOG_ENTRY_SIZE)  /* 4095 entries */

/* --- Severity levels ------------------------------------------------- */
typedef enum
{
    LOG_INFO  = 0x01,   /* routine event — boot, config, ack             */
    LOG_WARN  = 0x02,   /* threshold approached, minor anomaly           */
    LOG_FAULT = 0x03,   /* safety-critical — rail fault, thermal breach  */
} log_severity_t;

/* --- Event type codes ------------------------------------------------ */
typedef enum
{
    LOG_EVT_BOOT         = 0x01,  /* system started                      */
    LOG_EVT_POWER_FAULT  = 0x02,  /* power rail faulted                  */
    LOG_EVT_TEMP_ALERT   = 0x03,  /* thermal threshold breached          */
    LOG_EVT_TEMP_ACK     = 0x04,  /* thermal alert acknowledged          */
    LOG_EVT_CAN_ERROR    = 0x05,  /* CAN bus error detected              */
    LOG_EVT_OVERCURRENT  = 0x06,  /* INA3221 overcurrent event           */
    LOG_EVT_RESET        = 0x07,  /* unexpected reset / watchdog         */
    LOG_EVT_WP_FAIL      = 0x08,  /* FRAM write protect could not clear  */
    /* reserve 0x09–0xFF for future event types                          */
} log_event_t;

/* --- Log entry struct — 16 bytes packed ------------------------------ */
typedef struct __attribute__((packed))
{
    uint32_t timestamp_ms;   /* HAL_GetTick() at event time              */
    uint8_t  severity;       /* log_severity_t                           */
    uint8_t  event_type;     /* log_event_t                              */
    uint8_t  payload[9];     /* raw event data, zero-padded              */
    uint8_t  crc;            /* XOR of bytes 0–14                        */
} fram_log_entry_t;          /* must equal FRAM_LOG_ENTRY_SIZE (16)      */

/* --- Circular buffer metadata — 8 bytes at 0x0000 ------------------- */
typedef struct __attribute__((packed))
{
    uint16_t write_idx;      /* index of next entry to write (0–4094)   */
    uint16_t entry_count;    /* total entries written (saturates at max) */
    uint8_t  initialized;    /* 0xA5 if FRAM has been formatted          */
    uint8_t  reserved[3];
} fram_meta_t;               /* 8 bytes                                  */

#define FRAM_INIT_MAGIC     0xA5u

/* =======================================================================
 * Return codes
 * ===================================================================== */
typedef enum
{
    FRAM_OK           = 0,
    FRAM_ERR_I2C      = 1,   /* HAL I2C error                           */
    FRAM_ERR_ADDR     = 2,   /* address out of range                    */
    FRAM_ERR_WP       = 3,   /* write protect could not be cleared      */
    FRAM_ERR_CRC      = 4,   /* entry CRC mismatch on read              */
    FRAM_ERR_PARAM    = 5,   /* bad parameter                           */
    FRAM_ERR_NO_INIT  = 6,   /* FRAM not initialized                    */
} fram_err_t;

/* =======================================================================
 * Public API
 * ===================================================================== */

/* Verify device is present (Device ID read) and format if uninitialized.
   Call once at startup after power rails are stable.                    */
fram_err_t fram_drv_init(I2C_HandleTypeDef *hi2c);

/* Append a log entry to the circular buffer.
   Safe to call from ISR context — write protect toggle is the only
   side effect outside the FRAM itself.                                  */
fram_err_t fram_drv_log(log_severity_t severity,
                         log_event_t   event_type,
                         const uint8_t *payload,
                         uint8_t        payload_len);

/* Read a log entry by index (0 = oldest, entry_count-1 = newest).
   Returns FRAM_ERR_ADDR if index >= entry_count.                        */
fram_err_t fram_drv_read_entry(uint16_t       index,
                                fram_log_entry_t *entry_out);

/* Returns the total number of entries currently in the buffer.          */
uint16_t fram_drv_entry_count(void);

/* Erase all log entries and reset the circular buffer metadata.
   Use with caution — irreversible.                                      */
fram_err_t fram_drv_format(void);

/* Raw byte read/write — used by can_ctrl to transmit FRAM chunks.
   addr must be within 0x0000–0xFFFF. len must fit within the device.   */
fram_err_t fram_drv_read_raw (uint16_t addr, uint8_t *buf, uint16_t len);
fram_err_t fram_drv_write_raw(uint16_t addr, const uint8_t *buf,
                               uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* FRAM_PORT_H */