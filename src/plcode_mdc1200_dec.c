#include "plcode_internal.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int32_t goertzel_coeff(double freq_hz, int rate)
{
    double w = 2.0 * M_PI * freq_hz / (double)rate;
    double c = 2.0 * cos(w);
    return (int32_t)(c * 268435456.0 + (c >= 0 ? 0.5 : -0.5));
}

static int64_t goertzel_mag2(int64_t s1, int64_t s2, int32_t coeff)
{
    int64_t a = s1 * s1;
    int64_t b = s2 * s2;
    int64_t c = (s1 * (int64_t)coeff >> 14) * s2 >> 14;
    return a + b - c;
}

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

static void try_decode_packet(plcode_mdc1200_dec_t *c)
{
    uint8_t data[4];
    uint16_t crc_recv, crc_calc;

    data[0] = c->packet[0];
    data[1] = c->packet[1];
    data[2] = c->packet[2];
    data[3] = c->packet[3];

    crc_recv = (uint16_t)((c->packet[4] << 8) | c->packet[5]);
    crc_calc = crc_ccitt(data, 4);

    if (crc_recv == crc_calc) {
        c->detected = 1;
        c->op = data[0];
        c->arg = data[1];
        c->unit_id = (uint16_t)((data[2] << 8) | data[3]);
    }
}

static void process_block(plcode_mdc1200_dec_t *c)
{
    int64_t mark_mag = goertzel_mag2(c->mark_s1, c->mark_s2, c->mark_coeff);
    int64_t space_mag = goertzel_mag2(c->space_s1, c->space_s2, c->space_coeff);
    int bit = (mark_mag > space_mag) ? 1 : 0;

    /* Shift bit into sync detection register */
    c->shift_reg = ((c->shift_reg << 1) | (uint32_t)bit) & 0xFFFF;

    if (!c->synced) {
        if (c->shift_reg == PLCODE_MDC1200_SYNC) {
            c->synced = 1;
            c->packet_bits = 0;
            memset(c->packet, 0, sizeof(c->packet));
        }
    } else {
        /* Accumulate data bits into packet */
        int byte_idx = c->packet_bits / 8;
        int bit_idx = 7 - (c->packet_bits % 8);

        if (byte_idx < 8) {
            if (bit)
                c->packet[byte_idx] |= (uint8_t)(1 << bit_idx);
        }

        c->packet_bits++;

        /* 48 data bits = op(8) + arg(8) + id(16) + crc(16) */
        if (c->packet_bits >= 48) {
            try_decode_packet(c);
            c->synced = 0;
        }
    }

    /* Reset Goertzel */
    c->mark_s1 = 0;
    c->mark_s2 = 0;
    c->space_s1 = 0;
    c->space_s2 = 0;
    c->sample_count = 0;
}

int plcode_mdc1200_dec_create(plcode_mdc1200_dec_t **ctx, int rate)
{
    plcode_mdc1200_dec_t *c;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;

    plcode_tables_init();

    c = (plcode_mdc1200_dec_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    c->rate = rate;
    /* Block size = 1 bit period.  At 8kHz this is ~7 samples.
     * Marginal but functional since we compare mark vs space. */
    c->block_size = (int)((double)rate / PLCODE_MDC1200_BAUD + 0.5);

    c->mark_coeff = goertzel_coeff((double)PLCODE_MDC1200_MARK_HZ, rate);
    c->space_coeff = goertzel_coeff((double)PLCODE_MDC1200_SPACE_HZ, rate);

    *ctx = c;
    return PLCODE_OK;
}

void plcode_mdc1200_dec_process(plcode_mdc1200_dec_t *ctx,
                                 const int16_t *buf, size_t n,
                                 plcode_mdc1200_result_t *result)
{
    size_t i;
    if (!ctx || !buf) return;

    for (i = 0; i < n; i++) {
        int64_t ms0, ss0;

        ms0 = (((int64_t)ctx->mark_coeff * ctx->mark_s1) >> 28)
              - ctx->mark_s2 + (int64_t)buf[i];
        ctx->mark_s2 = ctx->mark_s1;
        ctx->mark_s1 = ms0;

        ss0 = (((int64_t)ctx->space_coeff * ctx->space_s1) >> 28)
              - ctx->space_s2 + (int64_t)buf[i];
        ctx->space_s2 = ctx->space_s1;
        ctx->space_s1 = ss0;

        ctx->sample_count++;
        if (ctx->sample_count >= ctx->block_size)
            process_block(ctx);
    }

    if (result) {
        result->detected = ctx->detected;
        result->op = ctx->op;
        result->arg = ctx->arg;
        result->unit_id = ctx->unit_id;
    }
}

void plcode_mdc1200_dec_reset(plcode_mdc1200_dec_t *ctx)
{
    if (!ctx) return;
    ctx->mark_s1 = 0;
    ctx->mark_s2 = 0;
    ctx->space_s1 = 0;
    ctx->space_s2 = 0;
    ctx->sample_count = 0;
    ctx->shift_reg = 0;
    ctx->synced = 0;
    ctx->packet_bits = 0;
    memset(ctx->packet, 0, sizeof(ctx->packet));
    ctx->detected = 0;
    ctx->op = 0;
    ctx->arg = 0;
    ctx->unit_id = 0;
}

void plcode_mdc1200_dec_destroy(plcode_mdc1200_dec_t *ctx)
{
    free(ctx);
}
