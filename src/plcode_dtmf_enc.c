#include "plcode_internal.h"
#include <stdlib.h>

int plcode_dtmf_enc_create(plcode_dtmf_enc_t **ctx,
                            int rate, char digit, int16_t amplitude)
{
    plcode_dtmf_enc_t *c;
    int idx, row, col;
    double row_hz, col_hz;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;
    idx = plcode_dtmf_digit_index(digit);
    if (idx < 0) return PLCODE_ERR_PARAM;
    if (amplitude <= 0) return PLCODE_ERR_PARAM;

    plcode_tables_init();

    c = (plcode_dtmf_enc_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    row = idx / 4;
    col = idx % 4;
    row_hz = (double)plcode_dtmf_row_freqs[row];
    col_hz = (double)plcode_dtmf_col_freqs[col];

    c->row_phase_inc = (uint32_t)(row_hz / (double)rate * 4294967296.0 + 0.5);
    c->col_phase_inc = (uint32_t)(col_hz / (double)rate * 4294967296.0 + 0.5);
    c->row_phase = 0;
    c->col_phase = 0;
    c->amplitude = amplitude;

    *ctx = c;
    return PLCODE_OK;
}

void plcode_dtmf_enc_process(plcode_dtmf_enc_t *ctx, int16_t *buf, size_t n)
{
    size_t i;
    if (!ctx || !buf) return;

    for (i = 0; i < n; i++) {
        int16_t row_tone = plcode_scale(plcode_sine_lookup(ctx->row_phase),
                                         ctx->amplitude);
        int16_t col_tone = plcode_scale(plcode_sine_lookup(ctx->col_phase),
                                         ctx->amplitude);
        buf[i] = plcode_clamp16((int32_t)buf[i] + (int32_t)row_tone
                                + (int32_t)col_tone);
        ctx->row_phase += ctx->row_phase_inc;
        ctx->col_phase += ctx->col_phase_inc;
    }
}

void plcode_dtmf_enc_destroy(plcode_dtmf_enc_t *ctx)
{
    free(ctx);
}
