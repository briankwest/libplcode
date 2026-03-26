#include "plcode_internal.h"
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int32_t goertzel_coeff_calc(double freq_hz, int rate)
{
    double w = 2.0 * M_PI * freq_hz / (double)rate;
    double c = 2.0 * cos(w);
    return (int32_t)(c * 268435456.0 + (c >= 0 ? 0.5 : -0.5));
}

static void process_block(plcode_toneburst_dec_t *c)
{
    int64_t a = c->s1 * c->s1;
    int64_t b = c->s2 * c->s2;
    int64_t cross = (c->s1 * (int64_t)c->coeff >> 14) * c->s2 >> 14;
    int64_t mag = a + b - cross;
    int64_t threshold = (int64_t)c->block_size * c->block_size / 16;
    int tone_present = (mag > threshold);

    if (tone_present) {
        c->tone_active = 1;
        c->tone_samples += c->block_size;
        if (c->tone_samples >= c->min_samples)
            c->detected = 1;
    } else {
        c->tone_active = 0;
        c->tone_samples = 0;
    }

    c->s1 = 0;
    c->s2 = 0;
    c->sample_count = 0;
}

int plcode_toneburst_dec_create(plcode_toneburst_dec_t **ctx,
                                 int rate, int freq, int min_duration_ms)
{
    plcode_toneburst_dec_t *c;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;
    if (freq < 300 || freq > 3000) return PLCODE_ERR_PARAM;
    if (min_duration_ms <= 0) return PLCODE_ERR_PARAM;

    plcode_tables_init();

    c = (plcode_toneburst_dec_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    c->rate = rate;
    c->block_size = rate / 100; /* 10ms blocks */
    c->min_samples = (int)((double)rate * min_duration_ms / 1000.0 + 0.5);
    c->coeff = goertzel_coeff_calc((double)freq, rate);

    *ctx = c;
    return PLCODE_OK;
}

void plcode_toneburst_dec_process(plcode_toneburst_dec_t *ctx,
                                   const int16_t *buf, size_t n,
                                   plcode_toneburst_result_t *result)
{
    size_t i;
    if (!ctx || !buf) return;

    for (i = 0; i < n; i++) {
        int64_t s0 = (((int64_t)ctx->coeff * ctx->s1) >> 28)
                     - ctx->s2 + (int64_t)buf[i];
        ctx->s2 = ctx->s1;
        ctx->s1 = s0;
        ctx->sample_count++;
        if (ctx->sample_count >= ctx->block_size)
            process_block(ctx);
    }

    if (result) {
        result->detected = ctx->detected;
        result->tone_active = ctx->tone_active;
    }
}

void plcode_toneburst_dec_reset(plcode_toneburst_dec_t *ctx)
{
    if (!ctx) return;
    ctx->s1 = 0;
    ctx->s2 = 0;
    ctx->sample_count = 0;
    ctx->tone_active = 0;
    ctx->tone_samples = 0;
    ctx->detected = 0;
}

void plcode_toneburst_dec_destroy(plcode_toneburst_dec_t *ctx)
{
    free(ctx);
}
