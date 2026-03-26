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

/* Find the strongest tone above threshold, return digit index or -1. */
static int find_best_digit(plcode_selcall_dec_t *c)
{
    int i, best = -1;
    int64_t best_mag = 0;
    int64_t threshold = (int64_t)c->block_size * c->block_size / 16;

    /* Only check digits 0-9 (not R/G for address decoding) */
    for (i = 0; i < 10; i++) {
        int64_t mag = goertzel_mag2(c->s1[i], c->s2[i], c->coeff[i]);
        if (mag > threshold && mag > best_mag) {
            best_mag = mag;
            best = i;
        }
    }

    /* Require 6 dB SNR over second-best */
    if (best >= 0) {
        int64_t second = 0;
        for (i = 0; i < PLCODE_SELCALL_NUM_TONES; i++) {
            if (i == best) continue;
            int64_t mag = goertzel_mag2(c->s1[i], c->s2[i], c->coeff[i]);
            if (mag > second) second = mag;
        }
        if (second > 0 && best_mag < second * 4)
            best = -1;
    }

    return best;
}

/* Minimum blocks for tone confirmation.
 * ZVEI1 70ms tone / 10ms block = 7 → need ~3 blocks for reliable detect.
 * EIA 33ms / 10ms = 3.3 → need ~2 blocks.
 * Use 2 blocks as minimum. */
#define MIN_SELCALL_BLOCKS 2

static void process_block(plcode_selcall_dec_t *c)
{
    int digit = find_best_digit(c);
    int i;

    if (digit >= 0) {
        if (digit == c->cur_tone) {
            c->tone_count++;
        } else {
            /* New tone detected */
            if (c->cur_tone >= 0 && c->tone_count >= MIN_SELCALL_BLOCKS) {
                /* Accept previous tone as a digit */
                if (c->num_received < PLCODE_SELCALL_ADDR_LEN) {
                    c->address[c->num_received++] = (char)('0' + c->cur_tone);
                    c->address[c->num_received] = '\0';
                    if (c->num_received == PLCODE_SELCALL_ADDR_LEN)
                        c->detected = 1;
                }
            }
            c->cur_tone = digit;
            c->tone_count = 1;
        }
    } else {
        /* No tone — accept pending if valid */
        if (c->cur_tone >= 0 && c->tone_count >= MIN_SELCALL_BLOCKS) {
            if (c->num_received < PLCODE_SELCALL_ADDR_LEN) {
                c->address[c->num_received++] = (char)('0' + c->cur_tone);
                c->address[c->num_received] = '\0';
                if (c->num_received == PLCODE_SELCALL_ADDR_LEN)
                    c->detected = 1;
            }
        }
        c->cur_tone = -1;
        c->tone_count = 0;
    }

    /* Reset Goertzel */
    for (i = 0; i < PLCODE_SELCALL_NUM_TONES; i++) {
        c->s1[i] = 0;
        c->s2[i] = 0;
    }
    c->sample_count = 0;
}

int plcode_selcall_dec_create(plcode_selcall_dec_t **ctx,
                               int rate, plcode_selcall_std_t standard)
{
    plcode_selcall_dec_t *c;
    const uint16_t *table;
    int tone_ms, i;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;

    table = plcode_selcall_table(standard);
    if (!table) return PLCODE_ERR_PARAM;

    tone_ms = plcode_selcall_tone_ms(standard);
    if (tone_ms <= 0) return PLCODE_ERR_PARAM;

    plcode_tables_init();

    c = (plcode_selcall_dec_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    c->rate = rate;
    c->block_size = rate / PLCODE_SELCALL_BLOCK_DIV;
    c->tone_samples = (int)((double)rate * tone_ms / 1000.0 + 0.5);
    c->cur_tone = -1;
    c->address[0] = '\0';

    for (i = 0; i < PLCODE_SELCALL_NUM_TONES; i++) {
        c->coeff[i] = goertzel_coeff((double)table[i], rate);
    }

    *ctx = c;
    return PLCODE_OK;
}

void plcode_selcall_dec_process(plcode_selcall_dec_t *ctx,
                                 const int16_t *buf, size_t n,
                                 plcode_selcall_result_t *result)
{
    size_t s;
    int i;

    if (!ctx || !buf) return;

    for (s = 0; s < n; s++) {
        for (i = 0; i < PLCODE_SELCALL_NUM_TONES; i++) {
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
            memcpy(result->address, ctx->address, PLCODE_SELCALL_ADDR_LEN + 1);
        } else {
            result->address[0] = '\0';
        }
    }
}

void plcode_selcall_dec_reset(plcode_selcall_dec_t *ctx)
{
    int i;
    if (!ctx) return;
    for (i = 0; i < PLCODE_SELCALL_NUM_TONES; i++) {
        ctx->s1[i] = 0;
        ctx->s2[i] = 0;
    }
    ctx->sample_count = 0;
    ctx->cur_tone = -1;
    ctx->tone_count = 0;
    ctx->num_received = 0;
    ctx->address[0] = '\0';
    ctx->detected = 0;
}

void plcode_selcall_dec_destroy(plcode_selcall_dec_t *ctx)
{
    free(ctx);
}
