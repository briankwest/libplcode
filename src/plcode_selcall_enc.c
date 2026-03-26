#include "plcode_internal.h"
#include <stdlib.h>
#include <string.h>

int plcode_selcall_enc_create(plcode_selcall_enc_t **ctx,
                               int rate, plcode_selcall_std_t standard,
                               const char *address, int16_t amplitude)
{
    plcode_selcall_enc_t *c;
    const uint16_t *table;
    int tone_ms, i;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!address) return PLCODE_ERR_PARAM;
    if ((int)strlen(address) != PLCODE_SELCALL_ADDR_LEN) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;
    if (amplitude <= 0) return PLCODE_ERR_PARAM;

    table = plcode_selcall_table(standard);
    if (!table) return PLCODE_ERR_PARAM;

    tone_ms = plcode_selcall_tone_ms(standard);
    if (tone_ms <= 0) return PLCODE_ERR_PARAM;

    /* Validate address digits */
    for (i = 0; i < PLCODE_SELCALL_ADDR_LEN; i++) {
        if (address[i] < '0' || address[i] > '9')
            return PLCODE_ERR_PARAM;
    }

    plcode_tables_init();

    c = (plcode_selcall_enc_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    c->amplitude = amplitude;
    c->num_tones = PLCODE_SELCALL_ADDR_LEN;
    c->tone_samples = (int)((double)rate * tone_ms / 1000.0 + 0.5);

    for (i = 0; i < PLCODE_SELCALL_ADDR_LEN; i++) {
        int digit = address[i] - '0';
        double freq = (double)table[digit];
        c->phase_inc[i] = (uint32_t)(freq / (double)rate * 4294967296.0 + 0.5);
    }

    c->phase = 0;
    c->cur_tone = 0;
    c->cur_sample = 0;
    c->complete = 0;

    *ctx = c;
    return PLCODE_OK;
}

void plcode_selcall_enc_process(plcode_selcall_enc_t *ctx, int16_t *buf, size_t n)
{
    size_t i;
    if (!ctx || !buf) return;

    for (i = 0; i < n && !ctx->complete; i++) {
        if (ctx->cur_sample >= ctx->tone_samples) {
            ctx->cur_tone++;
            ctx->cur_sample = 0;
            if (ctx->cur_tone >= ctx->num_tones) {
                ctx->complete = 1;
                break;
            }
        }

        {
            int16_t tone = plcode_scale(
                plcode_sine_lookup(ctx->phase), ctx->amplitude);
            buf[i] = plcode_clamp16((int32_t)buf[i] + (int32_t)tone);
        }

        ctx->phase += ctx->phase_inc[ctx->cur_tone];
        ctx->cur_sample++;
    }
}

int plcode_selcall_enc_complete(plcode_selcall_enc_t *ctx)
{
    if (!ctx) return 1;
    return ctx->complete;
}

void plcode_selcall_enc_destroy(plcode_selcall_enc_t *ctx)
{
    free(ctx);
}
