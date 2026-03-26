#include "plcode_internal.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int32_t goertzel_coeff(double freq_hz, int rate)
{
    double w = 2.0 * M_PI * freq_hz / (double)rate;
    double c = 2.0 * cos(w);
    return (int32_t)(c * 268435456.0 + (c >= 0 ? 0.5 : -0.5));
}

static int64_t goertzel_mag2(int64_t s1, int64_t s2, int32_t coeff)
{
    int64_t a = s1 * s1;
    int64_t b = s2 * s2;
    int64_t c = (s1 * (int64_t)coeff >> 14) * s2 >> 14;
    return a + b - c;
}

static void decode_pattern(plcode_fskcw_dec_t *c)
{
    char ch;
    c->pattern[c->pattern_len] = '\0';
    ch = plcode_cwid_decode(c->pattern);
    if (ch != '\0') {
        c->last_char = ch;
        if (c->message_len < PLCODE_CWID_MSG_MAX - 1) {
            c->message[c->message_len++] = ch;
            c->message[c->message_len] = '\0';
        }
    }
    c->pattern_len = 0;
}

static void process_block(plcode_fskcw_dec_t *c)
{
    int64_t mark_mag = goertzel_mag2(c->mark_s1, c->mark_s2, c->mark_coeff);
    int64_t space_mag = goertzel_mag2(c->space_s1, c->space_s2, c->space_coeff);
    int current_tone = (mark_mag > space_mag);

    if (current_tone && !c->tone_on) {
        /* Mark onset */
        if (c->pattern_len > 0) {
            if (c->gap_samples >= 5 * c->dot_samples) {
                decode_pattern(c);
                if (c->message_len < PLCODE_CWID_MSG_MAX - 1) {
                    c->message[c->message_len++] = ' ';
                    c->message[c->message_len] = '\0';
                }
            } else if (c->gap_samples >= 2 * c->dot_samples) {
                decode_pattern(c);
            }
        }
        c->tone_samples = 0;
    } else if (!current_tone && c->tone_on) {
        /* Mark offset — classify as dot or dash */
        if (c->pattern_len < PLCODE_CWID_PATTERN_MAX - 1) {
            if (c->tone_samples >= 2 * c->dot_samples) {
                c->pattern[c->pattern_len++] = '-';
            } else {
                c->pattern[c->pattern_len++] = '.';
            }
        }
        c->gap_samples = 0;
    }

    if (current_tone) {
        c->tone_samples += c->block_size;
    } else {
        c->gap_samples += c->block_size;

        if (c->gap_samples >= 2 * c->dot_samples && c->pattern_len > 0) {
            if (c->gap_samples >= 5 * c->dot_samples &&
                c->message_len > 0 &&
                c->message[c->message_len - 1] != ' ') {
                decode_pattern(c);
                if (c->message_len < PLCODE_CWID_MSG_MAX - 1) {
                    c->message[c->message_len++] = ' ';
                    c->message[c->message_len] = '\0';
                }
            } else if (c->gap_samples < 5 * c->dot_samples &&
                       c->gap_samples >= 2 * c->dot_samples &&
                       c->gap_samples - c->block_size < 2 * c->dot_samples) {
                decode_pattern(c);
            }
        }
    }

    c->tone_on = current_tone;

    /* Reset Goertzel states for next block */
    c->mark_s1 = 0;
    c->mark_s2 = 0;
    c->space_s1 = 0;
    c->space_s2 = 0;
    c->sample_count = 0;
}

int plcode_fskcw_dec_create(plcode_fskcw_dec_t **ctx,
                              int rate, int mark_freq, int space_freq,
                              int wpm)
{
    plcode_fskcw_dec_t *c;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;
    if (mark_freq < 300 || mark_freq > 3000) return PLCODE_ERR_PARAM;
    if (space_freq < 300 || space_freq > 3000) return PLCODE_ERR_PARAM;
    if (mark_freq == space_freq) return PLCODE_ERR_PARAM;
    if (wpm < 5 || wpm > 40) return PLCODE_ERR_PARAM;

    plcode_tables_init();

    c = (plcode_fskcw_dec_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    c->rate = rate;
    c->block_size = rate / PLCODE_CWID_BLOCK_DIV;
    c->dot_samples = (int)(1.2 * (double)rate / (double)wpm + 0.5);
    c->mark_coeff = goertzel_coeff((double)mark_freq, rate);
    c->space_coeff = goertzel_coeff((double)space_freq, rate);
    c->message[0] = '\0';

    *ctx = c;
    return PLCODE_OK;
}

void plcode_fskcw_dec_process(plcode_fskcw_dec_t *ctx,
                               const int16_t *buf, size_t n,
                               plcode_cwid_result_t *result)
{
    size_t i;
    int prev_msg_len;

    if (!ctx || !buf) return;

    prev_msg_len = ctx->message_len;

    for (i = 0; i < n; i++) {
        int64_t ms0, ss0;

        /* Update mark Goertzel */
        ms0 = (((int64_t)ctx->mark_coeff * ctx->mark_s1) >> 28)
              - ctx->mark_s2 + (int64_t)buf[i];
        ctx->mark_s2 = ctx->mark_s1;
        ctx->mark_s1 = ms0;

        /* Update space Goertzel */
        ss0 = (((int64_t)ctx->space_coeff * ctx->space_s1) >> 28)
              - ctx->space_s2 + (int64_t)buf[i];
        ctx->space_s2 = ctx->space_s1;
        ctx->space_s1 = ss0;

        ctx->sample_count++;

        if (ctx->sample_count >= ctx->block_size) {
            process_block(ctx);
        }
    }

    if (result) {
        result->tone_active = ctx->tone_on;
        result->character = ctx->last_char;
        result->new_character = (ctx->message_len > prev_msg_len) ? 1 : 0;
    }
}

const char *plcode_fskcw_dec_message(plcode_fskcw_dec_t *ctx)
{
    if (!ctx) return "";
    return ctx->message;
}

void plcode_fskcw_dec_reset(plcode_fskcw_dec_t *ctx)
{
    if (!ctx) return;
    ctx->mark_s1 = 0;
    ctx->mark_s2 = 0;
    ctx->space_s1 = 0;
    ctx->space_s2 = 0;
    ctx->sample_count = 0;
    ctx->tone_on = 0;
    ctx->tone_samples = 0;
    ctx->gap_samples = 0;
    ctx->pattern_len = 0;
    ctx->message_len = 0;
    ctx->message[0] = '\0';
    ctx->last_char = '\0';
}

void plcode_fskcw_dec_destroy(plcode_fskcw_dec_t *ctx)
{
    free(ctx);
}
