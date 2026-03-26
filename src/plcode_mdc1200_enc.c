#include "plcode_internal.h"
#include <stdlib.h>
#include <string.h>

/* CRC-CCITT (0x1021), init 0xFFFF */
static uint16_t crc_ccitt(const uint8_t *data, int len)
{
    uint16_t crc = 0xFFFF;
    int i, j;
    for (i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* Set a bit in a packed byte array (MSB first within bytes). */
static void set_bit(uint8_t *bits, int pos, int val)
{
    int byte_idx = pos / 8;
    int bit_idx = 7 - (pos % 8);
    if (val)
        bits[byte_idx] |= (uint8_t)(1 << bit_idx);
    else
        bits[byte_idx] &= (uint8_t)(~(1 << bit_idx));
}

int plcode_mdc1200_enc_create(plcode_mdc1200_enc_t **ctx,
                               int rate, uint8_t op, uint8_t arg,
                               uint16_t unit_id, int16_t amplitude)
{
    plcode_mdc1200_enc_t *c;
    uint8_t data[4];
    uint16_t crc;
    int i, bit_pos;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;
    if (amplitude <= 0) return PLCODE_ERR_PARAM;

    plcode_tables_init();

    c = (plcode_mdc1200_enc_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    c->mark_phase_inc = (uint32_t)(
        (double)PLCODE_MDC1200_MARK_HZ / (double)rate * 4294967296.0 + 0.5);
    c->space_phase_inc = (uint32_t)(
        (double)PLCODE_MDC1200_SPACE_HZ / (double)rate * 4294967296.0 + 0.5);
    c->amplitude = amplitude;
    c->bit_samples = (int)((double)rate / PLCODE_MDC1200_BAUD + 0.5);
    c->phase = 0;

    /* Build packet bitstream */
    memset(c->bits, 0, sizeof(c->bits));
    bit_pos = 0;

    /* Preamble: alternating 10101010... */
    for (i = 0; i < PLCODE_MDC1200_PREAMBLE_BITS; i++) {
        set_bit(c->bits, bit_pos++, (i & 1) == 0);
    }

    /* Sync word: 0x07FF (16 bits, MSB first) */
    for (i = 15; i >= 0; i--) {
        set_bit(c->bits, bit_pos++, (PLCODE_MDC1200_SYNC >> i) & 1);
    }

    /* Data: op, arg, unit_id (MSB first) */
    data[0] = op;
    data[1] = arg;
    data[2] = (uint8_t)(unit_id >> 8);
    data[3] = (uint8_t)(unit_id & 0xFF);

    for (i = 0; i < 4; i++) {
        int b;
        for (b = 7; b >= 0; b--)
            set_bit(c->bits, bit_pos++, (data[i] >> b) & 1);
    }

    /* CRC-CCITT over data bytes */
    crc = crc_ccitt(data, 4);
    for (i = 15; i >= 0; i--) {
        set_bit(c->bits, bit_pos++, (crc >> i) & 1);
    }

    c->total_bits = bit_pos;
    c->cur_bit = 0;
    c->cur_sample = 0;
    c->complete = 0;

    *ctx = c;
    return PLCODE_OK;
}

/* Get bit value from packed byte array. */
static int get_bit(const uint8_t *bits, int pos)
{
    int byte_idx = pos / 8;
    int bit_idx = 7 - (pos % 8);
    return (bits[byte_idx] >> bit_idx) & 1;
}

void plcode_mdc1200_enc_process(plcode_mdc1200_enc_t *ctx,
                                 int16_t *buf, size_t n)
{
    size_t i;
    if (!ctx || !buf) return;

    for (i = 0; i < n && !ctx->complete; i++) {
        int bit = get_bit(ctx->bits, ctx->cur_bit);
        uint32_t inc = bit ? ctx->mark_phase_inc : ctx->space_phase_inc;
        int16_t tone = plcode_scale(plcode_sine_lookup(ctx->phase),
                                     ctx->amplitude);
        buf[i] = plcode_clamp16((int32_t)buf[i] + (int32_t)tone);
        ctx->phase += inc;

        ctx->cur_sample++;
        if (ctx->cur_sample >= ctx->bit_samples) {
            ctx->cur_sample = 0;
            ctx->cur_bit++;
            if (ctx->cur_bit >= ctx->total_bits)
                ctx->complete = 1;
        }
    }
}

int plcode_mdc1200_enc_complete(plcode_mdc1200_enc_t *ctx)
{
    if (!ctx) return 1;
    return ctx->complete;
}

void plcode_mdc1200_enc_destroy(plcode_mdc1200_enc_t *ctx)
{
    free(ctx);
}
