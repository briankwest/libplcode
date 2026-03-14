#include "plcode_internal.h"
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int plcode_ctcss_enc_create(plcode_ctcss_enc_t **ctx,
                            int rate, uint16_t freq_x10, int16_t amplitude)
{
    plcode_ctcss_enc_t *c;
    double freq_hz;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;
    if (plcode_ctcss_tone_index(freq_x10) < 0) return PLCODE_ERR_PARAM;
    if (amplitude <= 0) return PLCODE_ERR_PARAM;

    plcode_tables_init();

    c = (plcode_ctcss_enc_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    freq_hz = (double)freq_x10 / 10.0;

    /* Phase increment: (freq / rate) * 2^32 */
    c->phase_inc = (uint32_t)(freq_hz / (double)rate * 4294967296.0 + 0.5);
    c->phase = 0;
    c->amplitude = amplitude;

    *ctx = c;
    return PLCODE_OK;
}

void plcode_ctcss_enc_process(plcode_ctcss_enc_t *ctx, int16_t *buf, size_t n)
{
    size_t i;
    if (!ctx || !buf) return;

    for (i = 0; i < n; i++) {
        int16_t tone = plcode_scale(plcode_sine_lookup(ctx->phase), ctx->amplitude);
        buf[i] = plcode_clamp16((int32_t)buf[i] + (int32_t)tone);
        ctx->phase += ctx->phase_inc;
    }
}

void plcode_ctcss_enc_destroy(plcode_ctcss_enc_t *ctx)
{
    free(ctx);
}
