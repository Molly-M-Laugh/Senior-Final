#ifndef FRAM_PORT_H
#define FRAM_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * I2C address — A0/A1/A2 all tied to GND (confirmed from schematic)
 * FM24V05 slave address format: 1010[A2][A1][A0] -> 0x50
 *
 * Note: FM24V05 is 512Kbit (64KB). The upper vs lower 32KB bank is
 * selected by bit 0 of the device address byte (treated as A16).
 * This driver handles that split transparently via bank selection.
 * --------------------------------------------------------------------- */
#define FRAM_BASE_ADDR          0x50u
#define FRAM_ADDR_BANK0         (FRAM_BASE_ADDR << 1)        /* 0x0000-0x7FFF */
#define FRAM_ADDR_BANK1         ((FRAM_BASE_ADDR | 0x01u) << 1) /* 0x8000-0xFFFF */

#define FRAM_SIZE_BYTES         0x10000u  /* 64KB total */
#define FRAM_BANK_SIZE          0x8000u   /* 32KB per bank */

/* -----------------------------------------------------------------------
 * WP pin — PA15, active high (write protected when HIGH)
 * Controlled here so callers don't touch GPIO directly.
 * --------------------------------------------------------------------- */
#define FRAM_WP_ENABLE()   HAL_GPIO_WritePin(WP_GPIO_Port, WP_Pin, GPIO_PIN_SET)
#define FRAM_WP_DISABLE()  HAL_GPIO_WritePin(WP_GPIO_Port, WP_Pin, GPIO_PIN_RESET)

/* -----------------------------------------------------------------------
 * Return type
 * --------------------------------------------------------------------- */
typedef enum {
    FRAM_OK             = 0,
    FRAM_ERR_I2C        = 1,
    FRAM_ERR_BOUNDS     = 2,  /* Address + length exceeds device size */
    FRAM_ERR_PROTECTED  = 3,  /* Write attempted while WP active */
} fram_status_t;

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

/* No hardware init needed — FM24V05 is ready at power-on.
   Ensures WP is asserted (safe default). */
void fram_init(void);

/* Read len bytes from addr into buf.
   Handles bank boundary crossing transparently. */
fram_status_t fram_read(uint16_t addr, uint8_t *buf, uint16_t len);

/* Write len bytes from buf to addr.
   Temporarily deasserts WP, writes, reasserts WP.
   No polling required — F-RAM writes complete at bus speed. */
fram_status_t fram_write(uint16_t addr, const uint8_t *buf, uint16_t len);

/* Convenience: read/write a single byte */
fram_status_t fram_read_byte(uint16_t addr, uint8_t *out);
fram_status_t fram_write_byte(uint16_t addr, uint8_t value);

/* Convenience: read/write a uint32_t (stored big-endian) */
fram_status_t fram_read_u32(uint16_t addr, uint32_t *out);
fram_status_t fram_write_u32(uint16_t addr, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif /* FRAM_PORT_H */
