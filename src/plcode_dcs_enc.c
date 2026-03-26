#include "plcode_internal.h"
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int plcode_dcs_enc_create(plcode_dcs_enc_t **ctx,
                          int rate, uint16_t code, int inverted,
                          int16_t amplitude)
{
    plcode_dcs_enc_t *c;
    int code_idx;
    double cutoff_hz;

    if (!ctx) return PLCODE_ERR_PARAM;
    if (!plcode_valid_rate(rate)) return PLCODE_ERR_RATE;
    if (amplitude <= 0) return PLCODE_ERR_PARAM;

    code_idx = plcode_dcs_code_index(code);
    if (code_idx < 0) return PLCODE_ERR_PARAM;

    plcode_tables_init();

    c = (plcode_dcs_enc_t *)calloc(1, sizeof(*c));
    if (!c) return PLCODE_ERR_ALLOC;

    c->rate = rate;
    c->amplitude = amplitude;
    c->inverted = inverted;

    /* Get precomputed Golay codeword (TIA/EIA-603: marker at bit 9) */
    c->codeword = plcode_dcs_codewords[code_idx];
    if (inverted) {
        c->codeword ^= 0x7FFFFF; /* Invert all 23 bits */
    }

    /* Bit timing: fractional accumulator
     * bit_phase_inc = (134.4 / rate) * 2^32 */
    c->bit_phase_inc = (uint32_t)(PLCODE_DCS_BITRATE / (double)rate * 4294967296.0 + 0.5);
    c->bit_phase = 0;
    c->bit_index = 0;

    /* Single-pole IIR LPF at ~300 Hz
     * alpha = 1 - exp(-2*pi*fc/fs), in Q15 */
    cutoff_hz = 300.0;
    {
        double alpha = 1.0 - exp(-2.0 * M_PI * cutoff_hz / (double)rate);
        c->lpf_alpha = (int32_t)(alpha * 32768.0 + 0.5);
    }
    c->lpf_state = 0;

    /* Precompute turn-off codeword: complement of 9-bit code */
    {
        uint16_t code9 = plcode_dcs_codes[code_idx] & 0x1FF;
        uint16_t comp9 = (~code9) & 0x1FF;
        uint16_t data12 = (uint16_t)(0x800u | comp9);
        c->turnoff_codeword = plcode_golay_encode(data12);
        if (inverted)
            c->turnoff_codeword ^= 0x7FFFFF;
    }
    c->state = 0;

    *ctx = c;
    return PLCODE_OK;
}

void plcode_dcs_enc_process(plcode_dcs_enc_t *ctx, int16_t *buf, size_t n)
{
    size_t i;
    if (!ctx || !buf) return;

    for (i = 0; i < n; i++) {
        uint32_t cw;
        int bit;
        int16_t raw, filtered;
        uint32_t prev_phase;

        if (ctx->state == 2) break; /* stopped */

        /* Select active codeword */
        cw = (ctx->state == 1) ? ctx->turnoff_codeword : ctx->codeword;

        /* Get current bit value — LSB first per TIA/EIA-603 */
        bit = (cw >> ctx->bit_index) & 1;

        /* NRZ: +amplitude for 1, -amplitude for 0 */
        raw = bit ? ctx->amplitude : (int16_t)(-ctx->amplitude);

        /* IIR LPF: state += alpha * (input - state), Q15 */
        {
            int32_t input_q15 = (int32_t)raw << 15;
            int32_t diff = input_q15 - ctx->lpf_state;
            ctx->lpf_state += (int32_t)(((int64_t)ctx->lpf_alpha * diff) >> 15);
        }

        filtered = (int16_t)(ctx->lpf_state >> 15);

        /* Mix into buffer */
        buf[i] = plcode_clamp16((int32_t)buf[i] + (int32_t)filtered);

        /* Advance bit clock */
        prev_phase = ctx->bit_phase;
        ctx->bit_phase += ctx->bit_phase_inc;

        if (ctx->bit_phase < prev_phase) {
            ctx->bit_index = (ctx->bit_index + 1) % PLCODE_DCS_CODEWORD_BITS;

            /* Count down turn-off bits */
            if (ctx->state == 1) {
                ctx->turnoff_bits--;
                if (ctx->turnoff_bits <= 0)
                    ctx->state = 2; /* stopped */
            }
        }
    }
}

void plcode_dcs_enc_turn_off(plcode_dcs_enc_t *ctx)
{
    if (!ctx) return;
    ctx->state = 1;
    /* Send 3 complete codeword cycles (3 * 23 bits) */
    ctx->turnoff_bits = 3 * PLCODE_DCS_CODEWORD_BITS;
}

int plcode_dcs_enc_stopped(plcode_dcs_enc_t *ctx)
{
    if (!ctx) return 1;
    return (ctx->state == 2) ? 1 : 0;
}

void plcode_dcs_enc_resume(plcode_dcs_enc_t *ctx)
{
    if (!ctx) return;
    ctx->state = 0;
}

void plcode_dcs_enc_destroy(plcode_dcs_enc_t *ctx)
{
    free(ctx);
}
