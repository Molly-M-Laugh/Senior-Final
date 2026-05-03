/* =============================================================================
 * frame_codec.c
 * =========================================================================== */
#include "frame_codec.h"
#include <string.h>

/* -------------------------------------------------------------------------- */
/* CRC16-CCITT (poly 0x1021, init 0xFFFF)                                    */
/* -------------------------------------------------------------------------- */
static uint16_t crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                                  : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

/* -------------------------------------------------------------------------- */
/* Minimal COBS encode/decode                                                  */
/* COBS replaces every 0x00 in the data so the wire stream is 0x00-free.     */
/* We use a simplified variant: escape each 0x00 as [0x01 0x01].             */
/* Full COBS is overkill at these payload sizes; this keeps decode trivial.  */
/*                                                                            */
/* Escape rule:  0x00 → 0x01 0x01                                            */
/*               0x01 → 0x01 0x02                                            */
/* -------------------------------------------------------------------------- */
static uint16_t cobs_encode(const uint8_t *in, uint16_t in_len,
                             uint8_t *out, uint16_t out_max)
{
    uint16_t o = 0;
    for (uint16_t i = 0; i < in_len; i++) {
        if (in[i] == 0x00u || in[i] == 0x01u) {
            if (o + 2 > out_max) return 0;
            out[o++] = 0x01u;
            out[o++] = in[i] + 1u;
        } else {
            if (o + 1 > out_max) return 0;
            out[o++] = in[i];
        }
    }
    return o;
}

static uint16_t cobs_decode(const uint8_t *in, uint16_t in_len,
                             uint8_t *out, uint16_t out_max)
{
    uint16_t o = 0;
    uint16_t i = 0;
    while (i < in_len) {
        if (in[i] == 0x01u) {
            if (i + 1 >= in_len) return 0;  /* truncated escape */
            if (o + 1 > out_max) return 0;
            out[o++] = in[i + 1] - 1u;
            i += 2;
        } else {
            if (o + 1 > out_max) return 0;
            out[o++] = in[i++];
        }
    }
    return o;
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */

frame_status_t frame_encode(uint8_t type,
                             const void *payload, uint16_t payload_len,
                             uint8_t *out_buf, uint16_t *out_len)
{
    if (payload_len > FRAME_MAX_PAYLOAD_LEN) return FRAME_ERR_TOO_LONG;

    /* Build raw inner: [TYPE][PAYLOAD][CRC16_LO][CRC16_HI] */
    uint8_t raw[FRAME_MAX_PAYLOAD_LEN + 4u];
    raw[0] = type;
    if (payload_len > 0) memcpy(&raw[1], payload, payload_len);
    uint16_t crc = crc16(raw, 1u + payload_len);
    raw[1 + payload_len]     = (uint8_t)(crc & 0xFFu);
    raw[2 + payload_len]     = (uint8_t)(crc >> 8);
    uint16_t raw_len = 1u + payload_len + 2u;

    /* COBS-encode into scratch space after the 3-byte header */
    uint8_t encoded[FRAME_MAX_PAYLOAD_LEN * 2u + 8u];
    uint16_t enc_len = cobs_encode(raw, raw_len, encoded, sizeof(encoded));
    if (enc_len == 0) return FRAME_ERR_TOO_LONG;

    /* Wire: 0x00 | LEN_LO | LEN_HI | <cobs body> | 0x00 */
    uint16_t wire_len = 1u + 2u + enc_len + 1u;
    if (wire_len > FRAME_MAX_WIRE_LEN) return FRAME_ERR_TOO_LONG;

    out_buf[0] = 0x00u;
    out_buf[1] = (uint8_t)(enc_len & 0xFFu);
    out_buf[2] = (uint8_t)(enc_len >> 8);
    memcpy(&out_buf[3], encoded, enc_len);
    out_buf[3 + enc_len] = 0x00u;
    *out_len = wire_len;

    return FRAME_OK;
}

frame_status_t frame_decode(const uint8_t *in_buf, uint16_t in_len,
                             uint8_t *type_out,
                             uint8_t *payload_buf, uint16_t *payload_len_out,
                             uint16_t *consumed)
{
    *consumed = 0;

    /* Need at least: 0x00 + 2 len bytes + 1 type + 2 crc + 0x00 = 7 bytes */
    if (in_len < 7u) return FRAME_ERR_NO_FRAME;

    /* Find opening delimiter */
    if (in_buf[0] != 0x00u) {
        /* Scan for sync — skip garbage bytes */
        uint16_t skip = 1;
        while (skip < in_len && in_buf[skip] != 0x00u) skip++;
        *consumed = skip;
        return FRAME_ERR_TRUNCATED;
    }

    uint16_t enc_len = (uint16_t)in_buf[1] | ((uint16_t)in_buf[2] << 8);
    uint16_t frame_total = 1u + 2u + enc_len + 1u;

    if (in_len < frame_total) return FRAME_ERR_NO_FRAME;  /* need more data */

    /* Verify closing delimiter */
    if (in_buf[3 + enc_len] != 0x00u) {
        *consumed = 1;   /* skip the bad opening delimiter, resync */
        return FRAME_ERR_TRUNCATED;
    }

    /* COBS-decode */
    uint8_t raw[FRAME_MAX_PAYLOAD_LEN + 4u];
    uint16_t raw_len = cobs_decode(&in_buf[3], enc_len, raw, sizeof(raw));
    if (raw_len < 3u) {  /* minimum: 1 type + 2 crc */
        *consumed = frame_total;
        return FRAME_ERR_TRUNCATED;
    }

    /* Verify CRC — covers type + payload (raw[0..raw_len-3]) */
    uint16_t payload_len = raw_len - 3u;
    uint16_t crc_calc = crc16(raw, 1u + payload_len);
    uint16_t crc_rx   = (uint16_t)raw[1 + payload_len]
                      | ((uint16_t)raw[2 + payload_len] << 8);
    if (crc_calc != crc_rx) {
        *consumed = frame_total;
        return FRAME_ERR_CRC;
    }

    *type_out        = raw[0];
    *payload_len_out = payload_len;
    if (payload_len > 0) memcpy(payload_buf, &raw[1], payload_len);
    *consumed = frame_total;

    return FRAME_OK;
}
