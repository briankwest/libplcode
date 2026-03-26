#include "plcode_internal.h"
#include <stdlib.h>

int plcode_courtesy_enc_create(plcode_courtesy_enc_t **ctx,
                                int rate,
                                const plcode_courtesy_tone_t *tones,
                                int num_tones)
{
    plcode_courtesy_enc_t *c;
    int i;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;
    if (!tones || num_tones <= 0) return PLCODE_ERR_PARAM;

    plcode_tables_init();

    c = (plcode_courtesy_enc_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    c->phase_incs = (uint32_t *)calloc((size_t)num_tones, sizeof(uint32_t));
    c->tone_samples = (int *)calloc((size_t)num_tones, sizeof(int));
    c->amplitudes = (int16_t *)calloc((size_t)num_tones, sizeof(int16_t));

    if (!c->phase_incs || !c->tone_samples || !c->amplitudes) {
        free(c->phase_incs);
        free(c->tone_samples);
        free(c->amplitudes);
        free(c);
        return PLCODE_ERR_ALLOC;
    }

    c->rate = rate;
    c->num_tones = num_tones;

    for (i = 0; i < num_tones; i++) {
        if (tones[i].freq > 0) {
            c->phase_incs[i] = (uint32_t)(
                (double)tones[i].freq / (double)rate * 4294967296.0 + 0.5);
        } else {
            c->phase_incs[i] = 0; /* silence */
        }
        c->tone_samples[i] = (int)(
            (double)rate * tones[i].duration_ms / 1000.0 + 0.5);
        c->amplitudes[i] = tones[i].amplitude;
    }

    c->phase = 0;
    c->cur_tone = 0;
    c->cur_sample = 0;
    c->complete = 0;

    *ctx = c;
    return PLCODE_OK;
}

void plcode_courtesy_enc_process(plcode_courtesy_enc_t *ctx,
                                  int16_t *buf, size_t n)
{
    size_t i;
    if (!ctx || !buf) return;

    for (i = 0; i < n && !ctx->complete; i++) {
        if (ctx->cur_sample >= ctx->tone_samples[ctx->cur_tone]) {
            ctx->cur_tone++;
            ctx->cur_sample = 0;
            ctx->phase = 0;
            if (ctx->cur_tone >= ctx->num_tones) {
                ctx->complete = 1;
                break;
            }
        }

        if (ctx->phase_incs[ctx->cur_tone] != 0) {
            int16_t tone = plcode_scale(
                plcode_sine_lookup(ctx->phase),
                ctx->amplitudes[ctx->cur_tone]);
            buf[i] = plcode_clamp16((int32_t)buf[i] + (int32_t)tone);
            ctx->phase += ctx->phase_incs[ctx->cur_tone];
        }
        /* freq==0: silence — leave buffer unchanged */

        ctx->cur_sample++;
    }
}

int plcode_courtesy_enc_complete(plcode_courtesy_enc_t *ctx)
{
    if (!ctx) return 1;
    return ctx->complete;
}

void plcode_courtesy_enc_destroy(plcode_courtesy_enc_t *ctx)
{
    if (ctx) {
        free(ctx->phase_incs);
        free(ctx->tone_samples);
        free(ctx->amplitudes);
        free(ctx);
    }
}
