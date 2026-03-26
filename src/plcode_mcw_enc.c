#include "plcode_internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* 5 ms ramp for raised-cosine shaping */
#define MCW_RAMP_MS 5

int plcode_mcw_enc_create(plcode_mcw_enc_t **ctx,
                           int rate, const char *message,
                           int freq, int wpm, int16_t amplitude)
{
    plcode_mcw_enc_t *c;
    int msg_len, max_elements, i;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!message) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;
    if (freq < 300 || freq > 3000) return PLCODE_ERR_PARAM;
    if (wpm < 5 || wpm > 40) return PLCODE_ERR_PARAM;
    if (amplitude <= 0) return PLCODE_ERR_PARAM;

    plcode_tables_init();

    msg_len = (int)strlen(message);
    max_elements = msg_len * 12 + 1;

    c = (plcode_mcw_enc_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    c->elements = (int8_t *)calloc((size_t)max_elements, sizeof(int8_t));
    if (!c->elements) { free(c); return PLCODE_ERR_ALLOC; }

    c->dot_samples = (int)(1.2 * (double)rate / (double)wpm + 0.5);
    c->phase_inc = (uint32_t)((double)freq / (double)rate * 4294967296.0 + 0.5);
    c->phase = 0;
    c->amplitude = amplitude;

    /* Raised-cosine ramp table */
    c->ramp_len = rate * MCW_RAMP_MS / 1000;
    if (c->ramp_len * 2 > c->dot_samples)
        c->ramp_len = c->dot_samples / 2;
    if (c->ramp_len < 1)
        c->ramp_len = 1;

    c->ramp = (int16_t *)malloc((size_t)c->ramp_len * sizeof(int16_t));
    if (!c->ramp) { free(c->elements); free(c); return PLCODE_ERR_ALLOC; }

    for (i = 0; i < c->ramp_len; i++) {
        double t = (double)i / (double)c->ramp_len;
        double factor = 0.5 * (1.0 - cos(M_PI * t));
        c->ramp[i] = (int16_t)(factor * 32767.0 + 0.5);
    }

    /* Build Morse element sequence */
    c->num_elements = plcode_cwid_build_elements(message, c->elements, max_elements);

    if (c->num_elements > 0) {
        int dur = c->elements[0];
        if (dur < 0) dur = -dur;
        c->cur_duration = dur * c->dot_samples;
    } else {
        c->complete = 1;
    }
    c->cur_element = 0;
    c->cur_sample = 0;

    *ctx = c;
    return PLCODE_OK;
}

void plcode_mcw_enc_process(plcode_mcw_enc_t *ctx, int16_t *buf, size_t n)
{
    size_t i;
    if (!ctx || !buf) return;

    for (i = 0; i < n && !ctx->complete; i++) {
        while (ctx->cur_sample >= ctx->cur_duration) {
            int dur;
            ctx->cur_element++;
            if (ctx->elements[ctx->cur_element] == 0) {
                ctx->complete = 1;
                break;
            }
            ctx->cur_sample = 0;
            dur = ctx->elements[ctx->cur_element];
            if (dur < 0) dur = -dur;
            ctx->cur_duration = dur * ctx->dot_samples;
        }

        if (ctx->complete) break;

        if (ctx->elements[ctx->cur_element] > 0) {
            int16_t tone = plcode_scale(plcode_sine_lookup(ctx->phase),
                                         ctx->amplitude);

            /* Apply raised-cosine ramp at edges */
            if (ctx->cur_sample < ctx->ramp_len) {
                tone = (int16_t)(((int32_t)tone * ctx->ramp[ctx->cur_sample]) >> 15);
            } else if (ctx->cur_sample >= ctx->cur_duration - ctx->ramp_len) {
                int ri = ctx->cur_duration - 1 - ctx->cur_sample;
                tone = (int16_t)(((int32_t)tone * ctx->ramp[ri]) >> 15);
            }

            buf[i] = plcode_clamp16((int32_t)buf[i] + (int32_t)tone);
            ctx->phase += ctx->phase_inc;
        }

        ctx->cur_sample++;
    }
}

int plcode_mcw_enc_complete(plcode_mcw_enc_t *ctx)
{
    if (!ctx) return 1;
    return ctx->complete;
}

void plcode_mcw_enc_destroy(plcode_mcw_enc_t *ctx)
{
    if (ctx) {
        free(ctx->ramp);
        free(ctx->elements);
        free(ctx);
    }
}
