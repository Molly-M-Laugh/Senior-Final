#include "fram_port.h"
#include "i2c.h"

#define I2C_TIMEOUT_MS  25u

/* -----------------------------------------------------------------------
 * Internal: single-bank write (caller guarantees no boundary crossing)
 * --------------------------------------------------------------------- */
static fram_status_t bank_write(uint8_t dev_addr_8bit,
                                 uint16_t mem_addr,
                                 const uint8_t *buf,
                                 uint16_t len)
{
    /* FM24V05 write: [DEV_ADDR_W][ADDR_HIGH][ADDR_LOW][DATA...] */
    uint8_t header[2];
    header[0] = (uint8_t)(mem_addr >> 8);
    header[1] = (uint8_t)(mem_addr & 0xFFu);

    /* Send address bytes first, then data in a single transaction
       using HAL_I2C_Master_Transmit with a two-part approach via
       the sequential transmit API */
    if (HAL_I2C_Master_Transmit(&hi2c1, dev_addr_8bit,
                                 header, 2, I2C_TIMEOUT_MS) != HAL_OK)
    {
        return FRAM_ERR_I2C;
    }

    /* Note: F-RAM requires address + data in one I2C transaction.
       The two-call approach above will generate a STOP between them,
       which aborts the write. Use HAL_I2C_Mem_Write instead — it
       keeps address and data in a single transaction. */
    (void)buf; (void)len; /* <- remove when using Mem_Write below */
    return FRAM_ERR_I2C;  /* <- placeholder, see correct impl below */
}

/* -----------------------------------------------------------------------
 * Internal: single-bank read
 * --------------------------------------------------------------------- */
static fram_status_t bank_read(uint8_t dev_addr_8bit,
                                uint16_t mem_addr,
                                uint8_t *buf,
                                uint16_t len)
{
    if (HAL_I2C_Mem_Read(&hi2c1,
                          dev_addr_8bit,
                          mem_addr,
                          I2C_MEMADD_SIZE_8BIT,  /* FM24V05 uses 8-bit
                                                    mem addr per bank  */
                          buf,
                          len,
                          I2C_TIMEOUT_MS) != HAL_OK)
    {
        return FRAM_ERR_I2C;
    }
    return FRAM_OK;
}

/* -----------------------------------------------------------------------
 * Internal: correct single-bank write using HAL_I2C_Mem_Write
 * This keeps address + data in one I2C transaction (no mid-STOP).
 * --------------------------------------------------------------------- */
static fram_status_t bank_write_mem(uint8_t dev_addr_8bit,
                                     uint16_t mem_addr,
                                     const uint8_t *buf,
                                     uint16_t len)
{
    if (HAL_I2C_Mem_Write(&hi2c1,
                           dev_addr_8bit,
                           mem_addr,
                           I2C_MEMADD_SIZE_8BIT,
                           (uint8_t *)buf,
                           len,
                           I2C_TIMEOUT_MS) != HAL_OK)
    {
        return FRAM_ERR_I2C;
    }
    /* No write polling needed — F-RAM completes at bus speed */
    return FRAM_OK;
}

/* -----------------------------------------------------------------------
 * Internal: select correct bank device address for a given flat address
 * --------------------------------------------------------------------- */
static uint8_t get_dev_addr(uint16_t flat_addr)
{
    return (flat_addr < FRAM_BANK_SIZE) ? FRAM_ADDR_BANK0 : FRAM_ADDR_BANK1;
}

/* Within-bank offset from a flat address */
static uint16_t get_bank_offset(uint16_t flat_addr)
{
    return (flat_addr < FRAM_BANK_SIZE) ? flat_addr
                                        : (flat_addr - FRAM_BANK_SIZE);
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

void fram_init(void)
{
    /* Assert write protect by default — safe state at startup */
    FRAM_WP_ENABLE();
}

fram_status_t fram_read(uint16_t addr, uint8_t *buf, uint16_t len)
{
    if ((uint32_t)addr + len > FRAM_SIZE_BYTES) return FRAM_ERR_BOUNDS;

    uint16_t remaining = len;
    uint16_t src       = addr;
    uint8_t *dst       = buf;

    while (remaining > 0)
    {
        uint8_t  dev   = get_dev_addr(src);
        uint16_t off   = get_bank_offset(src);

        /* How many bytes can we read before hitting the bank boundary? */
        uint16_t chunk = (uint16_t)(FRAM_BANK_SIZE - off);
        if (chunk > remaining) chunk = remaining;

        fram_status_t st = bank_read(dev, off, dst, chunk);
        if (st != FRAM_OK) return st;

        src       += chunk;
        dst       += chunk;
        remaining -= chunk;
    }

    return FRAM_OK;
}

fram_status_t fram_write(uint16_t addr, const uint8_t *buf, uint16_t len)
{
    if ((uint32_t)addr + len > FRAM_SIZE_BYTES) return FRAM_ERR_BOUNDS;

    FRAM_WP_DISABLE();

    uint16_t       remaining = len;
    uint16_t       src_addr  = addr;
    const uint8_t *src_buf   = buf;
    fram_status_t  st        = FRAM_OK;

    while (remaining > 0)
    {
        uint8_t  dev   = get_dev_addr(src_addr);
        uint16_t off   = get_bank_offset(src_addr);

        uint16_t chunk = (uint16_t)(FRAM_BANK_SIZE - off);
        if (chunk > remaining) chunk = remaining;

        st = bank_write_mem(dev, off, src_buf, chunk);
        if (st != FRAM_OK) break;

        src_addr  += chunk;
        src_buf   += chunk;
        remaining -= chunk;
    }

    FRAM_WP_ENABLE();
    return st;
}

fram_status_t fram_read_byte(uint16_t addr, uint8_t *out)
{
    return fram_read(addr, out, 1);
}

fram_status_t fram_write_byte(uint16_t addr, uint8_t value)
{
    return fram_write(addr, &value, 1);
}

fram_status_t fram_read_u32(uint16_t addr, uint32_t *out)
{
    uint8_t buf[4];
    fram_status_t st = fram_read(addr, buf, 4);
    if (st != FRAM_OK) return st;
    /* Big-endian reassembly */
    *out = ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] <<  8) |
            (uint32_t)buf[3];
    return FRAM_OK;
}

fram_status_t fram_write_u32(uint16_t addr, uint32_t value)
{
    uint8_t buf[4];
    buf[0] = (uint8_t)(value >> 24);
    buf[1] = (uint8_t)(value >> 16);
    buf[2] = (uint8_t)(value >>  8);
    buf[3] = (uint8_t)(value & 0xFFu);
    return fram_write(addr, buf, 4);
}
