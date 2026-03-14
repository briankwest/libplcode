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

int plcode_ctcss_dec_create(plcode_ctcss_dec_t **ctx, int rate)
{
    plcode_ctcss_dec_t *c;
    int i;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;

    plcode_tables_init();

    c = (plcode_ctcss_dec_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    c->rate = rate;
    c->block_size = rate;  /* 1 second window */
    c->sample_count = 0;
    c->prev_tone = -1;
    c->confirm_count = 0;

    for (i = 0; i < PLCODE_CTCSS_NUM_TONES; i++) {
        double freq = (double)plcode_ctcss_tones[i] / 10.0;
        c->coeff[i] = goertzel_coeff(freq, rate);
        c->s1[i] = 0;
        c->s2[i] = 0;
    }

    *ctx = c;
    return PLCODE_OK;
}

/* Compute magnitude^2 for a Goertzel bin at end of block (coeff in Q28) */
static int64_t goertzel_mag2(int64_t s1, int64_t s2, int32_t coeff)
{
    /* Shift down to prevent overflow in squaring */
    int64_t s1s = s1 >> 8;
    int64_t s2s = s2 >> 8;
    int64_t a = s1s * s1s;
    int64_t b = s2s * s2s;
    int64_t c = (s1s * s2s >> 8) * (int64_t)coeff >> 20;
    return a + b - c;
}

static void process_block(plcode_ctcss_dec_t *c, plcode_ctcss_result_t *result)
{
    int i;
    int64_t max_mag = 0, second_mag = 0;
    int max_idx = -1;

    /* Compute magnitude for each tone */
    for (i = 0; i < PLCODE_CTCSS_NUM_TONES; i++) {
        int64_t mag = goertzel_mag2(c->s1[i], c->s2[i], c->coeff[i]);
        if (mag < 0) mag = 0; /* shouldn't happen but safety */

        if (mag > max_mag) {
            second_mag = max_mag;
            max_mag = mag;
            max_idx = i;
        } else if (mag > second_mag) {
            second_mag = mag;
        }
    }

    /* Detection criteria:
     * 1. Maximum magnitude above absolute threshold
     * 2. At least 6 dB (4x power) above second-highest
     * 3. 2-window hysteresis for confirmed detection */

    /* Threshold: scale with block size squared (energy accumulates) */
    int64_t threshold = (int64_t)c->block_size * c->block_size / 16;

    int detected_idx = -1;
    if (max_idx >= 0 && max_mag > threshold) {
        /* 6 dB = factor of 4 in power */
        if (second_mag == 0 || max_mag > second_mag * 4) {
            detected_idx = max_idx;
        }
    }

    /* Hysteresis: require 2 consecutive detections */
    if (detected_idx >= 0 && detected_idx == c->prev_tone) {
        c->confirm_count++;
    } else {
        c->confirm_count = (detected_idx >= 0) ? 1 : 0;
    }
    c->prev_tone = detected_idx;

    /* Update result */
    if (result) {
        if (c->confirm_count >= 2 && detected_idx >= 0) {
            result->detected = 1;
            result->tone_index = detected_idx;
            result->tone_freq_x10 = plcode_ctcss_tones[detected_idx];
            result->magnitude = (int32_t)(max_mag >> 20); /* scaled down */
        } else {
            result->detected = 0;
            result->tone_index = -1;
            result->tone_freq_x10 = 0;
            result->magnitude = 0;
        }
    }

    /* Reset Goertzel accumulators for next block */
    for (i = 0; i < PLCODE_CTCSS_NUM_TONES; i++) {
        c->s1[i] = 0;
        c->s2[i] = 0;
    }
    c->sample_count = 0;
}

void plcode_ctcss_dec_process(plcode_ctcss_dec_t *ctx,
                              const int16_t *buf, size_t n,
                              plcode_ctcss_result_t *result)
{
    size_t i;
    int t;

    if (!ctx || !buf) return;

    for (i = 0; i < n; i++) {
        int32_t sample = (int32_t)buf[i];

        /* Update all Goertzel filters (Q28 coeff, int64 accumulators) */
        for (t = 0; t < PLCODE_CTCSS_NUM_TONES; t++) {
            int64_t s0 = (((int64_t)ctx->coeff[t] * ctx->s1[t]) >> 28)
                         - ctx->s2[t] + (int64_t)sample;
            ctx->s2[t] = ctx->s1[t];
            ctx->s1[t] = s0;
        }

        ctx->sample_count++;

        if (ctx->sample_count >= ctx->block_size) {
            process_block(ctx, result);
        }
    }
}

void plcode_ctcss_dec_reset(plcode_ctcss_dec_t *ctx)
{
    int i;
    if (!ctx) return;
    for (i = 0; i < PLCODE_CTCSS_NUM_TONES; i++) {
        ctx->s1[i] = 0;
        ctx->s2[i] = 0;
    }
    ctx->sample_count = 0;
    ctx->prev_tone = -1;
    ctx->confirm_count = 0;
}

void plcode_ctcss_dec_destroy(plcode_ctcss_dec_t *ctx)
{
    free(ctx);
}
