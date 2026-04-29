#include "fram_port.h"
#include "i2c.h"
#include "gpio.h"
#include <string.h>

/* =======================================================================
 * Module state
 * ===================================================================== */
static I2C_HandleTypeDef *s_hi2c     = NULL;
static fram_meta_t         s_meta;           /* cached in RAM            */
static bool                s_initialised    = false;

/* =======================================================================
 * Write protect helpers
 *
 * WP HIGH = write protected (safe default)
 * WP LOW  = write enabled (only during write operations)
 *
 * The FM24V05 WP pin is pulled down internally, so a floating pin or
 * GPIO reset state is write-enabled. We drive it explicitly.
 * ===================================================================== */
static inline void wp_assert(void)   /* protect — WP HIGH */
{
    HAL_GPIO_WritePin(FRAM_WP_GPIO_PORT, FRAM_WP_PIN, GPIO_PIN_SET);
}

static inline void wp_deassert(void) /* enable write — WP LOW */
{
    HAL_GPIO_WritePin(FRAM_WP_GPIO_PORT, FRAM_WP_PIN, GPIO_PIN_RESET);
}

/* =======================================================================
 * CRC — simple XOR of all bytes except the crc field itself
 * ===================================================================== */
static uint8_t compute_crc(const fram_log_entry_t *e)
{
    const uint8_t *b = (const uint8_t *)e;
    uint8_t crc = 0u;
    for (uint8_t i = 0u; i < (FRAM_LOG_ENTRY_SIZE - 1u); i++)
    {
        crc ^= b[i];
    }
    return crc;
}

/* =======================================================================
 * Low-level I2C transfers
 *
 * The FM24V05 uses a 16-bit memory address sent MSB first after the
 * slave address byte. HAL_I2C_Mem_Write/Read handle this natively with
 * I2C_MEMADD_SIZE_16BIT.
 *
 * Errata note (datasheet Rev H, page 18-19): the FM24V05 can generate
 * an unintended STOP condition. The workaround applied here is option 1:
 * the master ignores unintended STOPs by retrying once on I2C error.
 * ===================================================================== */
static HAL_StatusTypeDef i2c_read(uint16_t mem_addr,
                                   uint8_t *buf,
                                   uint16_t len)
{
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(s_hi2c,
                                              FRAM_I2C_ADDR << 1,
                                              mem_addr,
                                              I2C_MEMADD_SIZE_16BIT,
                                              buf, len,
                                              FRAM_I2C_TIMEOUT_MS);
    if (st != HAL_OK)
    {
        /* Retry once — handles the FM24V05 errata unintended STOP      */
        HAL_I2C_Init(s_hi2c);   /* recover bus                          */
        st = HAL_I2C_Mem_Read(s_hi2c,
                               FRAM_I2C_ADDR << 1,
                               mem_addr,
                               I2C_MEMADD_SIZE_16BIT,
                               buf, len,
                               FRAM_I2C_TIMEOUT_MS);
    }
    return st;
}

static HAL_StatusTypeDef i2c_write(uint16_t mem_addr,
                                    const uint8_t *buf,
                                    uint16_t len)
{
    /* FRAM writes require WP to be LOW                                  */
    wp_deassert();

    HAL_StatusTypeDef st = HAL_I2C_Mem_Write(s_hi2c,
                                               FRAM_I2C_ADDR << 1,
                                               mem_addr,
                                               I2C_MEMADD_SIZE_16BIT,
                                               (uint8_t *)buf, len,
                                               FRAM_I2C_TIMEOUT_MS);
    if (st != HAL_OK)
    {
        HAL_I2C_Init(s_hi2c);
        st = HAL_I2C_Mem_Write(s_hi2c,
                                FRAM_I2C_ADDR << 1,
                                mem_addr,
                                I2C_MEMADD_SIZE_16BIT,
                                (uint8_t *)buf, len,
                                FRAM_I2C_TIMEOUT_MS);
    }

    wp_assert(); /* re-protect immediately after write                   */
    return st;
}

/* =======================================================================
 * Metadata persistence
 * ===================================================================== */
static fram_err_t meta_read(void)
{
    HAL_StatusTypeDef st = i2c_read(FRAM_META_BASE,
                                     (uint8_t *)&s_meta,
                                     sizeof(s_meta));
    return (st == HAL_OK) ? FRAM_OK : FRAM_ERR_I2C;
}

static fram_err_t meta_write(void)
{
    HAL_StatusTypeDef st = i2c_write(FRAM_META_BASE,
                                      (const uint8_t *)&s_meta,
                                      sizeof(s_meta));
    return (st == HAL_OK) ? FRAM_OK : FRAM_ERR_I2C;
}

/* =======================================================================
 * Convert a logical entry index to a FRAM byte address.
 *
 * The circular buffer always writes to s_meta.write_idx and increments.
 * Reading by logical index (0 = oldest) requires computing the oldest
 * slot and offsetting from there.
 * ===================================================================== */
static uint16_t entry_addr(uint16_t logical_idx)
{
    uint16_t count = s_meta.entry_count < FRAM_LOG_MAX_ENTRIES
                     ? s_meta.entry_count : FRAM_LOG_MAX_ENTRIES;

    /* Oldest physical slot when buffer is full                          */
    uint16_t oldest_phys = (s_meta.entry_count >= FRAM_LOG_MAX_ENTRIES)
                           ? s_meta.write_idx   /* next write overwrites oldest */
                           : 0u;

    uint16_t phys = (oldest_phys + logical_idx) % FRAM_LOG_MAX_ENTRIES;
    (void)count;

    return (uint16_t)(FRAM_LOG_BASE + (phys * FRAM_LOG_ENTRY_SIZE));
}

/* =======================================================================
 * fram_drv_init
 * ===================================================================== */
fram_err_t fram_drv_init(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == NULL) return FRAM_ERR_PARAM;
    s_hi2c = hi2c;
    s_initialised = false;

    wp_assert(); /* safe default — write protected until needed          */

    /* --- Verify device is present using Device ID read sequence ---
     * Send reserved slave ID 0xF8, then this device's slave address,
     * then repeated START with 0xF9 (read). FM24V05 returns 3 bytes:
     * manufacturer ID (2 bytes) + product ID (1 byte).
     * We just verify the transaction succeeds — don't check the value
     * since we trust the schematic already.                             */
    uint8_t dev_id[3] = {0};
    uint8_t dev_addr_byte = (FRAM_I2C_ADDR << 1); /* write bit = 0      */

    /* Step 1: send reserved 0xF8 + device slave address                */
    HAL_StatusTypeDef st = HAL_I2C_Master_Transmit(s_hi2c,
                                                     FRAM_DEV_ID_ADDR,
                                                     &dev_addr_byte, 1u,
                                                     FRAM_I2C_TIMEOUT_MS);
    if (st != HAL_OK) return FRAM_ERR_I2C;

    /* Step 2: repeated START + read 3 bytes                            */
    st = HAL_I2C_Master_Receive(s_hi2c,
                                 FRAM_DEV_ID_ADDR | 0x01u,
                                 dev_id, 3u,
                                 FRAM_I2C_TIMEOUT_MS);
    if (st != HAL_OK) return FRAM_ERR_I2C;

    /* --- Check if FRAM has been formatted --- */
    fram_err_t err = meta_read();
    if (err != FRAM_OK) return err;

    if (s_meta.initialized != FRAM_INIT_MAGIC)
    {
        /* First boot — format the FRAM                                  */
        err = fram_drv_format();
        if (err != FRAM_OK) return err;
    }

    s_initialised = true;
    return FRAM_OK;
}

/* =======================================================================
 * fram_drv_format
 * ===================================================================== */
fram_err_t fram_drv_format(void)
{
    if (s_hi2c == NULL) return FRAM_ERR_PARAM;

    memset(&s_meta, 0, sizeof(s_meta));
    s_meta.initialized = FRAM_INIT_MAGIC;
    s_meta.write_idx   = 0u;
    s_meta.entry_count = 0u;

    return meta_write();
}

/* =======================================================================
 * fram_drv_log
 * ===================================================================== */
fram_err_t fram_drv_log(log_severity_t severity,
                         log_event_t   event_type,
                         const uint8_t *payload,
                         uint8_t        payload_len)
{
    if (!s_initialised)  return FRAM_ERR_NO_INIT;
    if (payload == NULL && payload_len > 0u) return FRAM_ERR_PARAM;

    fram_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    entry.timestamp_ms = HAL_GetTick();
    entry.severity     = (uint8_t)severity;
    entry.event_type   = (uint8_t)event_type;

    /* Copy payload, clamp to field size                                 */
    uint8_t copy_len = payload_len < sizeof(entry.payload)
                       ? payload_len : sizeof(entry.payload);
    if (payload != NULL)
    {
        memcpy(entry.payload, payload, copy_len);
    }

    entry.crc = compute_crc(&entry);

    /* Write to current write slot                                       */
    uint16_t addr = (uint16_t)(FRAM_LOG_BASE
                    + (s_meta.write_idx * FRAM_LOG_ENTRY_SIZE));

    HAL_StatusTypeDef st = i2c_write(addr,
                                      (const uint8_t *)&entry,
                                      sizeof(entry));
    if (st != HAL_OK) return FRAM_ERR_I2C;

    /* Advance write pointer — circular, wraps at max entries           */
    s_meta.write_idx = (s_meta.write_idx + 1u) % FRAM_LOG_MAX_ENTRIES;
    if (s_meta.entry_count < FRAM_LOG_MAX_ENTRIES)
    {
        s_meta.entry_count++;
    }

    return meta_write();
}

/* =======================================================================
 * fram_drv_read_entry
 * ===================================================================== */
fram_err_t fram_drv_read_entry(uint16_t index, fram_log_entry_t *entry_out)
{
    if (!s_initialised)   return FRAM_ERR_NO_INIT;
    if (entry_out == NULL) return FRAM_ERR_PARAM;
    if (index >= s_meta.entry_count) return FRAM_ERR_ADDR;

    uint16_t addr = entry_addr(index);

    HAL_StatusTypeDef st = i2c_read(addr,
                                     (uint8_t *)entry_out,
                                     sizeof(fram_log_entry_t));
    if (st != HAL_OK) return FRAM_ERR_I2C;

    /* Validate CRC                                                      */
    if (compute_crc(entry_out) != entry_out->crc)
    {
        return FRAM_ERR_CRC;
    }

    return FRAM_OK;
}

/* =======================================================================
 * fram_drv_entry_count
 * ===================================================================== */
uint16_t fram_drv_entry_count(void)
{
    return s_initialised ? s_meta.entry_count : 0u;
}

/* =======================================================================
 * fram_drv_read_raw / fram_drv_write_raw
 *
 * Used by can_ctrl to read and transmit FRAM chunks over CAN.
 * ===================================================================== */
fram_err_t fram_drv_read_raw(uint16_t addr, uint8_t *buf, uint16_t len)
{
    if (!s_initialised)  return FRAM_ERR_NO_INIT;
    if (buf == NULL)     return FRAM_ERR_PARAM;
    if ((uint32_t)addr + len > FRAM_CAPACITY_BYTES) return FRAM_ERR_ADDR;

    HAL_StatusTypeDef st = i2c_read(addr, buf, len);
    return (st == HAL_OK) ? FRAM_OK : FRAM_ERR_I2C;
}

fram_err_t fram_drv_write_raw(uint16_t addr, const uint8_t *buf, uint16_t len)
{
    if (!s_initialised)  return FRAM_ERR_NO_INIT;
    if (buf == NULL)     return FRAM_ERR_PARAM;
    if ((uint32_t)addr + len > FRAM_CAPACITY_BYTES) return FRAM_ERR_ADDR;

    HAL_StatusTypeDef st = i2c_write(addr, buf, len);
    return (st == HAL_OK) ? FRAM_OK : FRAM_ERR_I2C;
}