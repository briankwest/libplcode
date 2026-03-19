#include "plcode_internal.h"
#include <stdlib.h>
#include <string.h>

int plcode_cwid_enc_create(plcode_cwid_enc_t **ctx,
                            int rate, const char *message,
                            int freq, int wpm, int16_t amplitude)
{
    plcode_cwid_enc_t *c;
    int i, msg_len, num, max_elements;
    int need_char_gap, need_word_gap;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!message) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;
    if (freq < 300 || freq > 3000) return PLCODE_ERR_PARAM;
    if (wpm < 5 || wpm > 40) return PLCODE_ERR_PARAM;
    if (amplitude <= 0) return PLCODE_ERR_PARAM;

    plcode_tables_init();

    msg_len = (int)strlen(message);
    max_elements = msg_len * 12 + 1;

    c = (plcode_cwid_enc_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    c->elements = (int8_t *)calloc((size_t)max_elements, sizeof(int8_t));
    if (!c->elements) { free(c); return PLCODE_ERR_ALLOC; }

    c->dot_samples = (int)(1.2 * (double)rate / (double)wpm + 0.5);
    c->phase_inc = (uint32_t)((double)freq / (double)rate * 4294967296.0 + 0.5);
    c->phase = 0;
    c->amplitude = amplitude;

    /* Build element sequence from message */
    num = 0;
    need_char_gap = 0;
    need_word_gap = 0;

    for (i = 0; i < msg_len; i++) {
        char ch = message[i];
        const char *pat;
        int j;

        if (ch == ' ') {
            if (need_char_gap) {
                need_word_gap = 1;
                need_char_gap = 0;
            }
            continue;
        }

        pat = plcode_cwid_morse(ch);
        if (!pat) continue;

        if (need_word_gap) {
            c->elements[num++] = -7;
            need_word_gap = 0;
        } else if (need_char_gap) {
            c->elements[num++] = -3;
        }
        need_char_gap = 1;

        for (j = 0; pat[j] != '\0'; j++) {
            if (j > 0)
                c->elements[num++] = -1;  /* inter-element gap */
            c->elements[num++] = (int8_t)((pat[j] == '.') ? 1 : 3);
        }
    }

    c->elements[num] = 0;
    c->num_elements = num;

    /* Initialize playback state */
    if (num > 0) {
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

void plcode_cwid_enc_process(plcode_cwid_enc_t *ctx, int16_t *buf, size_t n)
{
    size_t i;
    if (!ctx || !buf) return;

    for (i = 0; i < n && !ctx->complete; i++) {
        /* Advance to next element if current one is done */
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
            /* Tone element */
            int16_t tone = plcode_scale(plcode_sine_lookup(ctx->phase),
                                         ctx->amplitude);
            buf[i] = plcode_clamp16((int32_t)buf[i] + (int32_t)tone);
            ctx->phase += ctx->phase_inc;
        }
        /* Gap element: leave buffer unchanged (silence) */

        ctx->cur_sample++;
    }
}

int plcode_cwid_enc_complete(plcode_cwid_enc_t *ctx)
{
    if (!ctx) return 1;
    return ctx->complete;
}

void plcode_cwid_enc_destroy(plcode_cwid_enc_t *ctx)
{
    if (ctx) {
        free(ctx->elements);
        free(ctx);
    }
}
