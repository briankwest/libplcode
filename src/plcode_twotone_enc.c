#include "plcode_internal.h"
#include <stdlib.h>

int plcode_twotone_enc_create(plcode_twotone_enc_t **ctx,
                               int rate, uint16_t tone_a_freq,
                               uint16_t tone_b_freq,
                               int tone_a_ms, int tone_b_ms,
                               int16_t amplitude)
{
    plcode_twotone_enc_t *c;
    double fa, fb;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;
    if (tone_a_freq == 0 || tone_b_freq == 0) return PLCODE_ERR_PARAM;
    if (tone_a_ms <= 0 || tone_b_ms <= 0) return PLCODE_ERR_PARAM;
    if (amplitude <= 0) return PLCODE_ERR_PARAM;

    plcode_tables_init();

    c = (plcode_twotone_enc_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    fa = (double)tone_a_freq / 10.0;
    fb = (double)tone_b_freq / 10.0;

    c->phase_inc_a = (uint32_t)(fa / (double)rate * 4294967296.0 + 0.5);
    c->phase_inc_b = (uint32_t)(fb / (double)rate * 4294967296.0 + 0.5);
    c->amplitude = amplitude;
    c->tone_a_samples = (int)((double)rate * tone_a_ms / 1000.0 + 0.5);
    c->tone_b_samples = (int)((double)rate * tone_b_ms / 1000.0 + 0.5);
    c->phase = 0;
    c->cur_sample = 0;
    c->complete = 0;

    *ctx = c;
    return PLCODE_OK;
}

void plcode_twotone_enc_process(plcode_twotone_enc_t *ctx, int16_t *buf, size_t n)
{
    size_t i;
    int total;
    if (!ctx || !buf) return;

    total = ctx->tone_a_samples + ctx->tone_b_samples;

    for (i = 0; i < n && !ctx->complete; i++) {
        uint32_t inc = (ctx->cur_sample < ctx->tone_a_samples)
                       ? ctx->phase_inc_a : ctx->phase_inc_b;
        int16_t tone = plcode_scale(plcode_sine_lookup(ctx->phase), ctx->amplitude);
        buf[i] = plcode_clamp16((int32_t)buf[i] + (int32_t)tone);
        ctx->phase += inc;
        ctx->cur_sample++;
        if (ctx->cur_sample >= total)
            ctx->complete = 1;
    }
}

int plcode_twotone_enc_complete(plcode_twotone_enc_t *ctx)
{
    if (!ctx) return 1;
    return ctx->complete;
}

void plcode_twotone_enc_destroy(plcode_twotone_enc_t *ctx)
{
    free(ctx);
}
