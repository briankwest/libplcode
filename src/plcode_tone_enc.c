#include "plcode_internal.h"
#include <stdlib.h>

int plcode_tone_enc_create(plcode_tone_enc_t **ctx,
                            int rate, int freq, int16_t amplitude)
{
    plcode_tone_enc_t *c;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;
    if (freq < 1 || freq > 4000) return PLCODE_ERR_PARAM;
    if (amplitude <= 0) return PLCODE_ERR_PARAM;

    plcode_tables_init();

    c = (plcode_tone_enc_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    c->phase_inc = (uint32_t)((double)freq / (double)rate * 4294967296.0 + 0.5);
    c->phase = 0;
    c->amplitude = amplitude;

    *ctx = c;
    return PLCODE_OK;
}

void plcode_tone_enc_process(plcode_tone_enc_t *ctx, int16_t *buf, size_t n)
{
    size_t i;
    if (!ctx || !buf) return;

    for (i = 0; i < n; i++) {
        int16_t tone = plcode_scale(plcode_sine_lookup(ctx->phase), ctx->amplitude);
        buf[i] = plcode_clamp16((int32_t)buf[i] + (int32_t)tone);
        ctx->phase += ctx->phase_inc;
    }
}

void plcode_tone_enc_destroy(plcode_tone_enc_t *ctx)
{
    free(ctx);
}
