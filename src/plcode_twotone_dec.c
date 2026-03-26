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

/* Find the strongest tone above threshold, return index or -1. */
static int find_best_tone(plcode_twotone_dec_t *c)
{
    int i, best = -1;
    int64_t best_mag = 0;
    int64_t threshold = (int64_t)c->block_size * c->block_size / 16;

    for (i = 0; i < PLCODE_TWOTONE_NUM_FREQS; i++) {
        int64_t mag = goertzel_mag2(c->s1[i], c->s2[i], c->coeff[i]);
        if (mag > threshold && mag > best_mag) {
            best_mag = mag;
            best = i;
        }
    }

    /* Require 6 dB SNR over second-best */
    if (best >= 0) {
        int64_t second = 0;
        for (i = 0; i < PLCODE_TWOTONE_NUM_FREQS; i++) {
            if (i == best) continue;
            int64_t mag = goertzel_mag2(c->s1[i], c->s2[i], c->coeff[i]);
            if (mag > second) second = mag;
        }
        if (second > 0 && best_mag < second * 4)
            best = -1;
    }

    return best;
}

/* Minimum consecutive blocks to confirm a tone:
 * Tone A: at least 400ms → 8 blocks at 50ms
 * Tone B: at least 400ms → 8 blocks at 50ms */
#define MIN_TONE_BLOCKS 8

static void process_block(plcode_twotone_dec_t *c)
{
    int tone_idx = find_best_tone(c);
    int i;

    switch (c->state) {
    case 0: /* waiting for tone A */
        if (tone_idx >= 0) {
            c->tone_a_idx = tone_idx;
            c->tone_a_count = 1;
            c->state = 1;
        }
        break;

    case 1: /* receiving tone A */
        if (tone_idx == c->tone_a_idx) {
            c->tone_a_count++;
        } else if (tone_idx >= 0 && tone_idx != c->tone_a_idx &&
                   c->tone_a_count >= MIN_TONE_BLOCKS) {
            /* Tone changed — this is tone B */
            c->tone_b_idx = tone_idx;
            c->tone_b_count = 1;
            c->state = 3;
        } else if (tone_idx < 0) {
            /* Brief gap between A and B is acceptable */
            if (c->tone_a_count >= MIN_TONE_BLOCKS) {
                c->state = 2;
            } else {
                /* Too short — reset */
                c->state = 0;
                c->tone_a_idx = -1;
            }
        } else {
            /* Different tone too early — restart */
            c->tone_a_idx = tone_idx;
            c->tone_a_count = 1;
        }
        break;

    case 2: /* gap between A and B */
        if (tone_idx >= 0 && tone_idx != c->tone_a_idx) {
            c->tone_b_idx = tone_idx;
            c->tone_b_count = 1;
            c->state = 3;
        } else if (tone_idx == c->tone_a_idx) {
            /* Tone A resumed — keep waiting */
            c->tone_a_count++;
            c->state = 1;
        } else {
            /* Still silence — give up after a while */
            c->state = 0;
            c->tone_a_idx = -1;
        }
        break;

    case 3: /* receiving tone B */
        if (tone_idx == c->tone_b_idx) {
            c->tone_b_count++;
            if (c->tone_b_count >= MIN_TONE_BLOCKS) {
                c->detected = 1;
                c->det_a_idx = c->tone_a_idx;
                c->det_b_idx = c->tone_b_idx;
            }
        } else {
            if (c->tone_b_count >= MIN_TONE_BLOCKS) {
                c->detected = 1;
                c->det_a_idx = c->tone_a_idx;
                c->det_b_idx = c->tone_b_idx;
            }
            /* Reset for next page */
            c->state = 0;
            c->tone_a_idx = -1;
            c->tone_b_idx = -1;
        }
        break;
    }

    /* Reset Goertzel state */
    for (i = 0; i < PLCODE_TWOTONE_NUM_FREQS; i++) {
        c->s1[i] = 0;
        c->s2[i] = 0;
    }
    c->sample_count = 0;
}

int plcode_twotone_dec_create(plcode_twotone_dec_t **ctx, int rate)
{
    plcode_twotone_dec_t *c;
    int i;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;

    plcode_tables_init();

    c = (plcode_twotone_dec_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    c->rate = rate;
    c->block_size = rate / PLCODE_TWOTONE_BLOCK_DIV;
    c->tone_a_idx = -1;
    c->tone_b_idx = -1;
    c->det_a_idx = -1;
    c->det_b_idx = -1;

    for (i = 0; i < PLCODE_TWOTONE_NUM_FREQS; i++) {
        double freq = (double)plcode_twotone_freqs[i] / 10.0;
        c->coeff[i] = goertzel_coeff(freq, rate);
    }

    *ctx = c;
    return PLCODE_OK;
}

void plcode_twotone_dec_process(plcode_twotone_dec_t *ctx,
                                 const int16_t *buf, size_t n,
                                 plcode_twotone_result_t *result)
{
    size_t s;
    int i;

    if (!ctx || !buf) return;

    for (s = 0; s < n; s++) {
        for (i = 0; i < PLCODE_TWOTONE_NUM_FREQS; i++) {
            int64_t s0 = (((int64_t)ctx->coeff[i] * ctx->s1[i]) >> 28)
                         - ctx->s2[i] + (int64_t)buf[s];
            ctx->s2[i] = ctx->s1[i];
            ctx->s1[i] = s0;
        }
        ctx->sample_count++;
        if (ctx->sample_count >= ctx->block_size)
            process_block(ctx);
    }

    if (result) {
        result->detected = ctx->detected;
        if (ctx->detected) {
            result->tone_a_index = ctx->det_a_idx;
            result->tone_b_index = ctx->det_b_idx;
            result->tone_a_freq_x10 = (ctx->det_a_idx >= 0)
                ? plcode_twotone_freqs[ctx->det_a_idx] : 0;
            result->tone_b_freq_x10 = (ctx->det_b_idx >= 0)
                ? plcode_twotone_freqs[ctx->det_b_idx] : 0;
        } else {
            result->tone_a_index = -1;
            result->tone_b_index = -1;
            result->tone_a_freq_x10 = 0;
            result->tone_b_freq_x10 = 0;
        }
    }
}

void plcode_twotone_dec_reset(plcode_twotone_dec_t *ctx)
{
    int i;
    if (!ctx) return;
    for (i = 0; i < PLCODE_TWOTONE_NUM_FREQS; i++) {
        ctx->s1[i] = 0;
        ctx->s2[i] = 0;
    }
    ctx->sample_count = 0;
    ctx->state = 0;
    ctx->tone_a_idx = -1;
    ctx->tone_b_idx = -1;
    ctx->det_a_idx = -1;
    ctx->det_b_idx = -1;
    ctx->tone_a_count = 0;
    ctx->tone_b_count = 0;
    ctx->detected = 0;
}

void plcode_twotone_dec_destroy(plcode_twotone_dec_t *ctx)
{
    free(ctx);
}
