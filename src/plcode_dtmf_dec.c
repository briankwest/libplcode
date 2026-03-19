#include "plcode_internal.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Goertzel coefficient: 2*cos(2*pi*f/fs) stored in Q28 */
static int32_t goertzel_coeff(double freq_hz, int rate)
{
    double w = 2.0 * M_PI * freq_hz / (double)rate;
    double c = 2.0 * cos(w);
    return (int32_t)(c * 268435456.0 + (c >= 0 ? 0.5 : -0.5));
}

int plcode_dtmf_dec_create(plcode_dtmf_dec_t **ctx, int rate)
{
    plcode_dtmf_dec_t *c;
    int i;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;

    plcode_tables_init();

    c = (plcode_dtmf_dec_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    c->rate = rate;
    c->block_size = rate / PLCODE_DTMF_BLOCK_DIV;  /* 20ms window */
    c->sample_count = 0;
    c->prev_digit = -1;
    c->confirm_count = 0;

    /* Precompute Goertzel coefficients: 0..3 = row, 4..7 = col */
    for (i = 0; i < 4; i++) {
        c->coeff[i] = goertzel_coeff((double)plcode_dtmf_row_freqs[i], rate);
        c->coeff[4 + i] = goertzel_coeff((double)plcode_dtmf_col_freqs[i], rate);
    }

    *ctx = c;
    return PLCODE_OK;
}

/* Compute magnitude^2 for a Goertzel bin at end of block (coeff in Q28).
 * DTMF blocks are small (160-960 samples), so squared terms are kept
 * at full precision. Cross-term is split to avoid int64 overflow. */
static int64_t goertzel_mag2(int64_t s1, int64_t s2, int32_t coeff)
{
    int64_t a = s1 * s1;
    int64_t b = s2 * s2;
    int64_t c = (s1 * (int64_t)coeff >> 14) * s2 >> 14;
    return a + b - c;
}

static void process_block(plcode_dtmf_dec_t *c, plcode_dtmf_result_t *result)
{
    int i;
    int64_t row_mag[4], col_mag[4];
    int max_row = -1, max_col = -1;
    int64_t max_row_mag = 0, max_col_mag = 0;
    int64_t second_row_mag = 0, second_col_mag = 0;

    /* Compute magnitude for each row frequency */
    for (i = 0; i < 4; i++) {
        row_mag[i] = goertzel_mag2(c->s1[i], c->s2[i], c->coeff[i]);
        if (row_mag[i] < 0) row_mag[i] = 0;

        if (row_mag[i] > max_row_mag) {
            second_row_mag = max_row_mag;
            max_row_mag = row_mag[i];
            max_row = i;
        } else if (row_mag[i] > second_row_mag) {
            second_row_mag = row_mag[i];
        }
    }

    /* Compute magnitude for each column frequency */
    for (i = 0; i < 4; i++) {
        col_mag[i] = goertzel_mag2(c->s1[4 + i], c->s2[4 + i], c->coeff[4 + i]);
        if (col_mag[i] < 0) col_mag[i] = 0;

        if (col_mag[i] > max_col_mag) {
            second_col_mag = max_col_mag;
            max_col_mag = col_mag[i];
            max_col = i;
        } else if (col_mag[i] > second_col_mag) {
            second_col_mag = col_mag[i];
        }
    }

    /* Detection criteria:
     * 1. Both strongest row and col above absolute threshold
     * 2. Each dominant tone > 6 dB (4x power) above second in its group
     * 3. Twist: row/col energies within 10 dB (10x power) of each other
     * 4. 2-block hysteresis for confirmation */

    int64_t threshold = (int64_t)c->block_size * c->block_size / 16;

    int detected_idx = -1;

    if (max_row >= 0 && max_col >= 0 &&
        max_row_mag > threshold && max_col_mag > threshold) {

        int row_ok = (second_row_mag == 0 || max_row_mag > second_row_mag * 4);
        int col_ok = (second_col_mag == 0 || max_col_mag > second_col_mag * 4);

        int twist_ok = (max_row_mag < max_col_mag * 10) &&
                       (max_col_mag < max_row_mag * 10);

        if (row_ok && col_ok && twist_ok) {
            detected_idx = max_row * 4 + max_col;
        }
    }

    /* Hysteresis: require 2 consecutive detections */
    if (detected_idx >= 0 && detected_idx == c->prev_digit) {
        c->confirm_count++;
    } else {
        c->confirm_count = (detected_idx >= 0) ? 1 : 0;
    }
    c->prev_digit = detected_idx;

    /* Update result */
    if (result) {
        if (c->confirm_count >= 2 && detected_idx >= 0) {
            result->detected = 1;
            result->digit_index = detected_idx;
            result->digit = plcode_dtmf_digits[detected_idx];
            result->row_freq = plcode_dtmf_row_freqs[detected_idx / 4];
            result->col_freq = plcode_dtmf_col_freqs[detected_idx % 4];
        } else {
            result->detected = 0;
            result->digit_index = -1;
            result->digit = '\0';
            result->row_freq = 0;
            result->col_freq = 0;
        }
    }

    /* Reset Goertzel accumulators for next block */
    for (i = 0; i < 8; i++) {
        c->s1[i] = 0;
        c->s2[i] = 0;
    }
    c->sample_count = 0;
}

void plcode_dtmf_dec_process(plcode_dtmf_dec_t *ctx,
                              const int16_t *buf, size_t n,
                              plcode_dtmf_result_t *result)
{
    size_t i;
    int f;

    if (!ctx || !buf) return;

    for (i = 0; i < n; i++) {
        int32_t sample = (int32_t)buf[i];

        /* Update all 8 Goertzel filters */
        for (f = 0; f < 8; f++) {
            int64_t s0 = (((int64_t)ctx->coeff[f] * ctx->s1[f]) >> 28)
                         - ctx->s2[f] + (int64_t)sample;
            ctx->s2[f] = ctx->s1[f];
            ctx->s1[f] = s0;
        }

        ctx->sample_count++;

        if (ctx->sample_count >= ctx->block_size) {
            process_block(ctx, result);
        }
    }
}

void plcode_dtmf_dec_reset(plcode_dtmf_dec_t *ctx)
{
    int i;
    if (!ctx) return;
    for (i = 0; i < 8; i++) {
        ctx->s1[i] = 0;
        ctx->s2[i] = 0;
    }
    ctx->sample_count = 0;
    ctx->prev_digit = -1;
    ctx->confirm_count = 0;
}

void plcode_dtmf_dec_destroy(plcode_dtmf_dec_t *ctx)
{
    free(ctx);
}
