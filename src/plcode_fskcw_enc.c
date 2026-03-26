#include "plcode_internal.h"
#include <stdlib.h>
#include <string.h>

int plcode_fskcw_enc_create(plcode_fskcw_enc_t **ctx,
                              int rate, const char *message,
                              int mark_freq, int space_freq,
                              int wpm, int16_t amplitude)
{
    plcode_fskcw_enc_t *c;
    int msg_len, max_elements;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!message) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;
    if (mark_freq < 300 || mark_freq > 3000) return PLCODE_ERR_PARAM;
    if (space_freq < 300 || space_freq > 3000) return PLCODE_ERR_PARAM;
    if (mark_freq == space_freq) return PLCODE_ERR_PARAM;
    if (wpm < 5 || wpm > 40) return PLCODE_ERR_PARAM;
    if (amplitude <= 0) return PLCODE_ERR_PARAM;

    plcode_tables_init();

    msg_len = (int)strlen(message);
    max_elements = msg_len * 12 + 1;

    c = (plcode_fskcw_enc_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    c->elements = (int8_t *)calloc((size_t)max_elements, sizeof(int8_t));
    if (!c->elements) { free(c); return PLCODE_ERR_ALLOC; }

    c->dot_samples = (int)(1.2 * (double)rate / (double)wpm + 0.5);
    c->mark_phase_inc = (uint32_t)((double)mark_freq / (double)rate * 4294967296.0 + 0.5);
    c->space_phase_inc = (uint32_t)((double)space_freq / (double)rate * 4294967296.0 + 0.5);
    c->phase = 0;
    c->amplitude = amplitude;

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

void plcode_fskcw_enc_process(plcode_fskcw_enc_t *ctx, int16_t *buf, size_t n)
{
    size_t i;
    if (!ctx || !buf) return;

    for (i = 0; i < n && !ctx->complete; i++) {
        uint32_t phase_inc;

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

        /* CPFSK: mark frequency for tone elements, space for gaps */
        phase_inc = (ctx->elements[ctx->cur_element] > 0)
                    ? ctx->mark_phase_inc : ctx->space_phase_inc;

        {
            int16_t tone = plcode_scale(plcode_sine_lookup(ctx->phase),
                                         ctx->amplitude);
            buf[i] = plcode_clamp16((int32_t)buf[i] + (int32_t)tone);
        }

        ctx->phase += phase_inc;
        ctx->cur_sample++;
    }
}

int plcode_fskcw_enc_complete(plcode_fskcw_enc_t *ctx)
{
    if (!ctx) return 1;
    return ctx->complete;
}

void plcode_fskcw_enc_destroy(plcode_fskcw_enc_t *ctx)
{
    if (ctx) {
        free(ctx->elements);
        free(ctx);
    }
}
