#include "plcode_internal.h"
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* DCS orbit canonicalization.
 * The Golay(23,12) code is cyclic — rotations of a codeword are valid
 * codewords for other codes. Each code pairs with an inverted alias
 * (53 orbits from 104 codes). When the decoder locks on an alias,
 * remap to the preferred (normal) label for the orbit.
 *
 * Table: preferred_code_index[i] = index of preferred code for code i's orbit.
 * Built at init time by checking codeword rotations. */
static int g_orbit_preferred[PLCODE_DCS_NUM_CODES];
static int g_orbit_initialized;

static void init_orbit_table(void)
{
    if (g_orbit_initialized) return;

    /* Default: each code is its own preferred */
    for (int i = 0; i < PLCODE_DCS_NUM_CODES; i++)
        g_orbit_preferred[i] = i;

    /* For each pair: check if any rotation of code i's codeword equals
     * code j's complement (inverted alias). If so, prefer the lower index. */
    for (int i = 0; i < PLCODE_DCS_NUM_CODES; i++) {
        uint32_t cw_i = plcode_dcs_codewords[i];
        for (int j = i + 1; j < PLCODE_DCS_NUM_CODES; j++) {
            if (g_orbit_preferred[j] != j) continue; /* already mapped */
            uint32_t cw_j = plcode_dcs_codewords[j];
            uint32_t comp_j = cw_j ^ 0x7FFFFF;

            uint32_t rot = cw_i;
            int found = 0;
            for (int r = 0; r < 23 && !found; r++) {
                if (rot == cw_j || rot == comp_j) found = 1;
                uint32_t lsb = rot & 1;
                rot = ((rot >> 1) | (lsb << 22)) & 0x7FFFFF;
            }
            if (!found) {
                rot = cw_i ^ 0x7FFFFF;
                for (int r = 0; r < 23 && !found; r++) {
                    if (rot == cw_j || rot == comp_j) found = 1;
                    uint32_t lsb = rot & 1;
                    rot = ((rot >> 1) | (lsb << 22)) & 0x7FFFFF;
                }
            }
            if (found) {
                /* i and j are in the same orbit — prefer lower index (normal) */
                g_orbit_preferred[j] = i;
            }
        }
    }
    g_orbit_initialized = 1;
}

/* Design 2nd-order Butterworth LPF coefficients (Q14) */
static void design_butterworth_lpf(int rate, double cutoff, int32_t b[3], int32_t a[2])
{
    double wc = tan(M_PI * cutoff / (double)rate);
    double wc2 = wc * wc;
    double sqrt2 = 1.4142135623730951;
    double norm = 1.0 / (1.0 + sqrt2 * wc + wc2);

    double b0 = wc2 * norm;
    double b1 = 2.0 * b0;
    double b2 = b0;
    double a1 = 2.0 * (wc2 - 1.0) * norm;
    double a2 = (1.0 - sqrt2 * wc + wc2) * norm;

    b[0] = (int32_t)(b0 * 16384.0 + 0.5);
    b[1] = (int32_t)(b1 * 16384.0 + 0.5);
    b[2] = (int32_t)(b2 * 16384.0 + 0.5);
    a[0] = (int32_t)(a1 * 16384.0 + (a1 >= 0 ? 0.5 : -0.5));
    a[1] = (int32_t)(a2 * 16384.0 + (a2 >= 0 ? 0.5 : -0.5));
}

int plcode_dcs_dec_create(plcode_dcs_dec_t **ctx, int rate)
{
    plcode_dcs_dec_t *c;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;

    plcode_tables_init();
    init_orbit_table();

    c = (plcode_dcs_dec_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    c->rate = rate;

    /* Design 300 Hz Butterworth LPF */
    design_butterworth_lpf(rate, 300.0, c->lpf_b, c->lpf_a);

    c->lpf_x[0] = c->lpf_x[1] = 0;
    c->lpf_y[0] = c->lpf_y[1] = 0;

    /* Comparator hysteresis thresholds */
    c->hyst_high = 500;
    c->hyst_low = -500;
    c->last_bit = 0;

    /* PLL: nominal bit rate 134.4 bps
     * pll_inc = (134.4 / rate) * 2^32 */
    c->pll_phase = 0;
    c->pll_inc = (uint32_t)(PLCODE_DCS_BITRATE / (double)rate * 4294967296.0 + 0.5);
    c->prev_input = 0;

    /* Shift register and detection */
    c->shift_reg = 0;
    c->total_bits = 0;
    c->match_code = -1;
    c->match_inv = 0;
    c->match_count = 0;
    c->bits_since_match = -1;
    c->confirmed_code = -1;
    c->confirmed_inv = 0;
    c->confirmed = 0;

    *ctx = c;
    return PLCODE_OK;
}

/* Apply 2nd-order IIR filter (Direct Form I, Q14 coefficients) */
static int16_t apply_lpf(plcode_dcs_dec_t *c, int16_t input)
{
    int32_t x = (int32_t)input;
    int32_t y;

    y = ((int32_t)c->lpf_b[0] * x
       + (int32_t)c->lpf_b[1] * c->lpf_x[0]
       + (int32_t)c->lpf_b[2] * c->lpf_x[1]
       - (int32_t)c->lpf_a[0] * c->lpf_y[0]
       - (int32_t)c->lpf_a[1] * c->lpf_y[1]) >> 14;

    c->lpf_x[1] = c->lpf_x[0];
    c->lpf_x[0] = x;
    c->lpf_y[1] = c->lpf_y[0];
    c->lpf_y[0] = y;

    return plcode_clamp16(y);
}

/* Comparator with hysteresis */
static int comparator(plcode_dcs_dec_t *c, int16_t input)
{
    if (input > c->hyst_high)
        c->last_bit = 1;
    else if (input < c->hyst_low)
        c->last_bit = 0;
    /* else hold last value */
    return c->last_bit;
}

/* Direct internal lookup — matches raw binary code value against the table.
 * Unlike the public plcode_dcs_code_index which accepts labels. */
static int dcs_code_index_internal(uint16_t raw_code)
{
    int i;
    for (i = 0; i < PLCODE_DCS_NUM_CODES; i++) {
        if (plcode_dcs_codes[i] == raw_code)
            return i;
    }
    return -1;
}

/* Try to extract a valid DCS code from a 23-bit shift register value.
 * Returns code table index or -1. Sets *inv to 0 (normal) or 1 (inverted). */
static int extract_dcs_code(uint32_t sr, int *inv)
{
    uint16_t data12, code9;

    /* Try normal: marker at bits 11:9 = 100 */
    data12 = (uint16_t)(sr >> 11);
    if ((data12 & 0xE00) == 0x800) {
        code9 = data12 & 0x1FF;
        int idx = dcs_code_index_internal(code9);
        if (idx >= 0) {
            *inv = 0;
            return idx;
        }
    }

    /* Try inverted: complement then check */
    uint32_t comp = sr ^ 0x7FFFFF;
    data12 = (uint16_t)(comp >> 11);
    if ((data12 & 0xE00) == 0x800) {
        code9 = data12 & 0x1FF;
        int idx = dcs_code_index_internal(code9);
        if (idx >= 0) {
            *inv = 1;
            return idx;
        }
    }

    return -1;
}

/* Check shift register for a valid DCS codeword.
 * Checks at aligned positions only (every 23 bits after first match).
 * Requires 3 consecutive aligned matches of the same code to confirm.
 * On aligned miss, falls back to free-running check to re-acquire. */
static void check_codeword(plcode_dcs_dec_t *c)
{
    if (c->total_bits < PLCODE_DCS_CODEWORD_BITS)
        return;

    if (c->bits_since_match >= 0) {
        c->bits_since_match++;
        if (c->bits_since_match < PLCODE_DCS_CODEWORD_BITS)
            return;
    }

    uint32_t sr = c->shift_reg & 0x7FFFFF;
    int found_inv = 0;
    int found_code = extract_dcs_code(sr, &found_inv);

    if (found_code >= 0) {
        if (c->bits_since_match == PLCODE_DCS_CODEWORD_BITS
            && found_code == c->match_code
            && found_inv == c->match_inv) {
            c->match_count++;
        } else {
            c->match_code = found_code;
            c->match_inv = found_inv;
            c->match_count = 1;
        }
        c->bits_since_match = 0;

        if (c->match_count >= 3) {
            c->confirmed = 1;
            c->confirmed_code = found_code;
            c->confirmed_inv = found_inv;
        }
    } else if (c->bits_since_match >= PLCODE_DCS_CODEWORD_BITS) {
        c->match_code = -1;
        c->match_count = 0;
        c->bits_since_match = -1;
    }
}

void plcode_dcs_dec_process(plcode_dcs_dec_t *ctx,
                            const int16_t *buf, size_t n,
                            plcode_dcs_result_t *result)
{
    size_t i;
    if (!ctx || !buf) return;

    for (i = 0; i < n; i++) {
        /* Low-pass filter */
        int16_t filtered = apply_lpf(ctx, buf[i]);

        /* Hard decision */
        int bit = comparator(ctx, filtered);

        /* Edge detection for PLL sync */
        if (bit != ctx->prev_input) {
            /* Transition detected — edges should occur at phase ~0.
             * Nudge PLL phase toward zero-crossing point.
             * Use proportional correction (~25% of error) for fast lock. */
            int32_t phase_err;
            if (ctx->pll_phase < 0x80000000u)
                phase_err = (int32_t)ctx->pll_phase;
            else
                phase_err = (int32_t)(ctx->pll_phase - 0xFFFFFFFFu - 1);
            ctx->pll_phase -= (uint32_t)(phase_err >> 2); /* 25% correction */
        }
        ctx->prev_input = bit;

        /* Check for PLL clock tick (phase crossing midpoint = 0x80000000) */
        uint32_t prev_phase = ctx->pll_phase;
        ctx->pll_phase += ctx->pll_inc;

        /* Sample bit at midpoint of bit period */
        if ((prev_phase < 0x80000000u) && (ctx->pll_phase >= 0x80000000u)) {
            /* Shift in new bit (right-shift, MSB insert — LSB first per TIA/EIA-603) */
            ctx->shift_reg = ((ctx->shift_reg >> 1) | ((uint32_t)bit << 22)) & 0x7FFFFF;
            ctx->total_bits++;

            /* Check against codeword table */
            check_codeword(ctx);
        }
    }

    /* Update result */
    if (result) {
        if (ctx->confirmed) {
            /* Remap through orbit table to preferred (canonical) code */
            int pref = g_orbit_preferred[ctx->confirmed_code];
            result->detected = 1;
            result->code_index = pref;
            result->code_number = (uint16_t)plcode_dcs_code_to_label(
                                      plcode_dcs_codes[pref]);
            result->inverted = (pref != ctx->confirmed_code)
                ? !ctx->confirmed_inv : ctx->confirmed_inv;
        } else {
            result->detected = 0;
            result->code_index = -1;
            result->code_number = 0;
            result->inverted = 0;
        }
    }
}

void plcode_dcs_dec_reset(plcode_dcs_dec_t *ctx)
{
    if (!ctx) return;
    ctx->lpf_x[0] = ctx->lpf_x[1] = 0;
    ctx->lpf_y[0] = ctx->lpf_y[1] = 0;
    ctx->last_bit = 0;
    ctx->pll_phase = 0;
    ctx->prev_input = 0;
    ctx->shift_reg = 0;
    ctx->total_bits = 0;
    ctx->match_code = -1;
    ctx->match_count = 0;
    ctx->bits_since_match = -1;
    ctx->confirmed = 0;
    ctx->confirmed_code = -1;
    ctx->confirmed_inv = 0;
}

void plcode_dcs_dec_destroy(plcode_dcs_dec_t *ctx)
{
    free(ctx);
}
