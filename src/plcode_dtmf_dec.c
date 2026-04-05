#include "plcode_internal.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Default option values */
#define DEF_HITS_TO_BEGIN       2
#define DEF_MISSES_TO_END       3
#define DEF_MIN_OFF_FRAMES      2
#define DEF_NORMAL_TWIST_X     16   /* 12 dB */
#define DEF_REVERSE_TWIST_X     4   /*  6 dB */
#define DEF_HARMONIC_REJECT     1
#define DEF_HARMONIC_THRESH_PCT 50

/* Number of Goertzel filters: 8 fundamental + 8 harmonic */
#define NUM_GOERTZEL 16

/* Goertzel coefficient: 2*cos(2*pi*f/fs) stored in Q28 */
static int32_t goertzel_coeff(double freq_hz, int rate)
{
    double w = 2.0 * M_PI * freq_hz / (double)rate;
    double c = 2.0 * cos(w);
    return (int32_t)(c * 268435456.0 + (c >= 0 ? 0.5 : -0.5));
}

static void apply_defaults(plcode_dtmf_dec_t *c, const plcode_dtmf_dec_opts_t *opts)
{
    if (!opts) {
        c->hits_to_begin     = DEF_HITS_TO_BEGIN;
        c->misses_to_end     = DEF_MISSES_TO_END;
        c->min_off_frames    = DEF_MIN_OFF_FRAMES;
        c->normal_twist_x    = DEF_NORMAL_TWIST_X;
        c->reverse_twist_x   = DEF_REVERSE_TWIST_X;
        c->harmonic_reject   = DEF_HARMONIC_REJECT;
        c->harmonic_thresh_pct = DEF_HARMONIC_THRESH_PCT;
        return;
    }
    c->hits_to_begin     = opts->hits_to_begin     > 0 ? opts->hits_to_begin     : DEF_HITS_TO_BEGIN;
    c->misses_to_end     = opts->misses_to_end     > 0 ? opts->misses_to_end     : DEF_MISSES_TO_END;
    c->min_off_frames    = opts->min_off_frames    > 0 ? opts->min_off_frames    : DEF_MIN_OFF_FRAMES;
    c->normal_twist_x    = opts->normal_twist_x    > 0 ? opts->normal_twist_x    : DEF_NORMAL_TWIST_X;
    c->reverse_twist_x   = opts->reverse_twist_x   > 0 ? opts->reverse_twist_x   : DEF_REVERSE_TWIST_X;
    c->harmonic_reject   = opts->harmonic_reject   > 0 ? opts->harmonic_reject   : DEF_HARMONIC_REJECT;
    c->harmonic_thresh_pct = opts->harmonic_thresh_pct > 0 ? opts->harmonic_thresh_pct : DEF_HARMONIC_THRESH_PCT;
}

int plcode_dtmf_dec_create_ex(plcode_dtmf_dec_t **ctx, int rate,
                               const plcode_dtmf_dec_opts_t *opts)
{
    plcode_dtmf_dec_t *c;
    int i;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;

    plcode_tables_init();

    c = (plcode_dtmf_dec_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    c->rate = rate;
    c->block_size = rate / PLCODE_DTMF_BLOCK_DIV;  /* 20ms window */
    c->state = DTMF_ST_IDLE;
    c->current_digit = -1;
    c->cooldown_digit = -1;

    apply_defaults(c, opts);

    /* Precompute Goertzel coefficients:
     * 0..3  = row fundamentals
     * 4..7  = col fundamentals
     * 8..11 = row 2nd harmonics
     * 12..15 = col 2nd harmonics */
    for (i = 0; i < 4; i++) {
        c->coeff[i]      = goertzel_coeff((double)plcode_dtmf_row_freqs[i], rate);
        c->coeff[4 + i]  = goertzel_coeff((double)plcode_dtmf_col_freqs[i], rate);
        c->coeff[8 + i]  = goertzel_coeff((double)plcode_dtmf_row_harmonics[i], rate);
        c->coeff[12 + i] = goertzel_coeff((double)plcode_dtmf_col_harmonics[i], rate);
    }

    *ctx = c;
    return PLCODE_OK;
}

int plcode_dtmf_dec_create(plcode_dtmf_dec_t **ctx, int rate)
{
    return plcode_dtmf_dec_create_ex(ctx, rate, NULL);
}

/* Compute magnitude^2 for a Goertzel bin at end of block (coeff in Q28).
 * DTMF blocks are small (160-960 samples), so squared terms are kept
 * at full precision. Cross-term is split to avoid int64 overflow. */
static int64_t goertzel_mag2(int64_t s1, int64_t s2, int32_t coeff)
{
    int64_t a = s1 * s1;
    int64_t b = s2 * s2;
    int64_t cross = (s1 * (int64_t)coeff >> 14) * s2 >> 14;
    return a + b - cross;
}

/* Return detected digit index from raw Goertzel magnitudes, or -1.
 * Applies threshold, selectivity, asymmetric twist, and harmonic checks. */
static int detect_digit(plcode_dtmf_dec_t *c)
{
    int i;
    int64_t row_mag[4], col_mag[4];
    int max_row = -1, max_col = -1;
    int64_t max_row_mag = 0, max_col_mag = 0;
    int64_t second_row_mag = 0, second_col_mag = 0;

    /* Compute magnitude for each row frequency */
    for (i = 0; i < 4; i++) {
        row_mag[i] = goertzel_mag2(c->s1[i], c->s2[i], c->coeff[i]);
        if (row_mag[i] < 0) row_mag[i] = 0;

        if (row_mag[i] > max_row_mag) {
            second_row_mag = max_row_mag;
            max_row_mag = row_mag[i];
            max_row = i;
        } else if (row_mag[i] > second_row_mag) {
            second_row_mag = row_mag[i];
        }
    }

    /* Compute magnitude for each column frequency */
    for (i = 0; i < 4; i++) {
        col_mag[i] = goertzel_mag2(c->s1[4 + i], c->s2[4 + i], c->coeff[4 + i]);
        if (col_mag[i] < 0) col_mag[i] = 0;

        if (col_mag[i] > max_col_mag) {
            second_col_mag = max_col_mag;
            max_col_mag = col_mag[i];
            max_col = i;
        } else if (col_mag[i] > second_col_mag) {
            second_col_mag = col_mag[i];
        }
    }

    /* Detection criteria:
     * 1. Both strongest row and col above absolute threshold
     * 2. Relative energy: DTMF bins must dominate total block energy
     * 3. Each dominant tone > 6 dB (4x power) above second in its group
     * 4. Asymmetric twist check (FM pre-emphasis aware)
     * 5. 2nd harmonic rejection (talk-off protection) */

    int64_t threshold = (int64_t)c->block_size * c->block_size / 16;

    if (max_row < 0 || max_col < 0 ||
        max_row_mag <= threshold || max_col_mag <= threshold)
        return -1;

    /* Relative energy check (talk-off protection per Bellcore/ITU-T):
     * For a clean dual-tone DTMF signal, the two Goertzel bins capture
     * nearly all the signal energy: (row+col) / (energy * N) ≈ 1.0.
     * Require at least 25% (factor of 4) to reject speech/music where
     * energy is spread across many frequencies.
     * Guard against silence (energy_sum==0) with the absolute threshold above. */
    if (c->energy_sum > 0) {
        int64_t dtmf_energy = max_row_mag + max_col_mag;
        int64_t total_scaled = (c->energy_sum / 4) * (int64_t)c->block_size;
        if (dtmf_energy < total_scaled)
            return -1;
    }

    /* Selectivity: dominant > 4x second in each group */
    int row_ok = (second_row_mag == 0 || max_row_mag > second_row_mag * 4);
    int col_ok = (second_col_mag == 0 || max_col_mag > second_col_mag * 4);
    if (!row_ok || !col_ok)
        return -1;

    /* Asymmetric twist: FM pre-emphasis boosts high-group (column) freqs */
    int twist_ok;
    if (max_col_mag >= max_row_mag)
        twist_ok = (max_col_mag < max_row_mag * (int64_t)c->normal_twist_x);
    else
        twist_ok = (max_row_mag < max_col_mag * (int64_t)c->reverse_twist_x);
    if (!twist_ok)
        return -1;

    int detected_idx = max_row * 4 + max_col;

    /* 2nd harmonic rejection: voice formants have strong harmonics,
     * clean DTMF sinusoids do not */
    if (c->harmonic_reject) {
        int64_t row_harm = goertzel_mag2(c->s1[8 + max_row],
                                          c->s2[8 + max_row],
                                          c->coeff[8 + max_row]);
        int64_t col_harm = goertzel_mag2(c->s1[12 + max_col],
                                          c->s2[12 + max_col],
                                          c->coeff[12 + max_col]);
        if (row_harm < 0) row_harm = 0;
        if (col_harm < 0) col_harm = 0;

        if (row_harm > max_row_mag * c->harmonic_thresh_pct / 100 ||
            col_harm > max_col_mag * c->harmonic_thresh_pct / 100)
            return -1;  /* likely voice, not clean DTMF */
    }

    return detected_idx;
}

/* Fill result struct from digit index */
static void fill_result(plcode_dtmf_result_t *result, int idx)
{
    if (!result) return;
    if (idx >= 0) {
        result->detected = 1;
        result->digit_index = idx;
        result->digit = plcode_dtmf_digits[idx];
        result->row_freq = plcode_dtmf_row_freqs[idx / 4];
        result->col_freq = plcode_dtmf_col_freqs[idx % 4];
    } else {
        result->detected = 0;
        result->digit_index = -1;
        result->digit = '\0';
        result->row_freq = 0;
        result->col_freq = 0;
    }
}

static void process_block(plcode_dtmf_dec_t *c, plcode_dtmf_result_t *result)
{
    int i;
    int raw_idx = detect_digit(c);

    /* State machine:
     * IDLE     → raw hit starts PENDING
     * PENDING  → consecutive hits → ACTIVE; miss → IDLE
     * ACTIVE   → holds detected=1 through individual misses;
     *            consecutive misses → COOLDOWN
     * COOLDOWN → same-digit blocked for min_off_frames; different digit → PENDING */

    switch (c->state) {
    case DTMF_ST_IDLE:
        if (raw_idx >= 0) {
            c->state = DTMF_ST_PENDING;
            c->current_digit = raw_idx;
            c->hit_count = 1;
            if (c->hits_to_begin <= 1) {
                c->state = DTMF_ST_ACTIVE;
                c->miss_count = 0;
            }
        }
        fill_result(result, -1);
        break;

    case DTMF_ST_PENDING:
        if (raw_idx == c->current_digit) {
            c->hit_count++;
            if (c->hit_count >= c->hits_to_begin) {
                c->state = DTMF_ST_ACTIVE;
                c->miss_count = 0;
                fill_result(result, c->current_digit);
            } else {
                fill_result(result, -1);
            }
        } else {
            /* Miss or different digit — reset */
            if (raw_idx >= 0) {
                /* Different digit: restart pending for new digit */
                c->current_digit = raw_idx;
                c->hit_count = 1;
            } else {
                c->state = DTMF_ST_IDLE;
                c->current_digit = -1;
                c->hit_count = 0;
            }
            fill_result(result, -1);
        }
        break;

    case DTMF_ST_ACTIVE:
        if (raw_idx == c->current_digit) {
            /* Still detecting same digit — reset miss counter */
            c->miss_count = 0;
        } else {
            c->miss_count++;
            if (c->miss_count >= c->misses_to_end) {
                /* Digit truly ended */
                c->state = DTMF_ST_COOLDOWN;
                c->cooldown_count = c->min_off_frames;
                c->cooldown_digit = c->current_digit;
                c->current_digit = -1;
                fill_result(result, -1);
                break;
            }
        }
        /* Still ACTIVE — report detected */
        fill_result(result, c->current_digit);
        break;

    case DTMF_ST_COOLDOWN:
        c->cooldown_count--;
        if (c->cooldown_count <= 0) {
            c->state = DTMF_ST_IDLE;
            c->cooldown_digit = -1;
            /* If a different digit is already present, start tracking it */
            if (raw_idx >= 0) {
                c->state = DTMF_ST_PENDING;
                c->current_digit = raw_idx;
                c->hit_count = 1;
                if (c->hits_to_begin <= 1) {
                    c->state = DTMF_ST_ACTIVE;
                    c->miss_count = 0;
                }
            }
        } else if (raw_idx >= 0 && raw_idx != c->cooldown_digit) {
            /* Different digit during cooldown — allow immediately */
            c->state = DTMF_ST_PENDING;
            c->current_digit = raw_idx;
            c->hit_count = 1;
            c->cooldown_digit = -1;
            if (c->hits_to_begin <= 1) {
                c->state = DTMF_ST_ACTIVE;
                c->miss_count = 0;
            }
        }
        fill_result(result, -1);
        break;
    }

    /* Reset Goertzel accumulators and energy for next block */
    for (i = 0; i < NUM_GOERTZEL; i++) {
        c->s1[i] = 0;
        c->s2[i] = 0;
    }
    c->sample_count = 0;
    c->energy_sum = 0;
}

void plcode_dtmf_dec_process(plcode_dtmf_dec_t *ctx,
                              const int16_t *buf, size_t n,
                              plcode_dtmf_result_t *result)
{
    size_t i;
    int f, nf;

    if (!ctx || !buf) return;

    nf = ctx->harmonic_reject ? NUM_GOERTZEL : 8;

    for (i = 0; i < n; i++) {
        int32_t sample = (int32_t)buf[i];

        /* Accumulate block energy for relative threshold check */
        ctx->energy_sum += (int64_t)sample * sample;

        /* Update Goertzel filters (8 fundamental, optionally 8 harmonic) */
        for (f = 0; f < nf; f++) {
            int64_t s0 = (((int64_t)ctx->coeff[f] * ctx->s1[f]) >> 28)
                         - ctx->s2[f] + (int64_t)sample;
            ctx->s2[f] = ctx->s1[f];
            ctx->s1[f] = s0;
        }

        ctx->sample_count++;

        if (ctx->sample_count >= ctx->block_size) {
            process_block(ctx, result);
        }
    }
}

void plcode_dtmf_dec_reset(plcode_dtmf_dec_t *ctx)
{
    int i;
    if (!ctx) return;
    for (i = 0; i < NUM_GOERTZEL; i++) {
        ctx->s1[i] = 0;
        ctx->s2[i] = 0;
    }
    ctx->sample_count = 0;
    ctx->energy_sum = 0;
    ctx->state = DTMF_ST_IDLE;
    ctx->current_digit = -1;
    ctx->hit_count = 0;
    ctx->miss_count = 0;
    ctx->cooldown_count = 0;
    ctx->cooldown_digit = -1;
}

void plcode_dtmf_dec_destroy(plcode_dtmf_dec_t *ctx)
{
    free(ctx);
}
