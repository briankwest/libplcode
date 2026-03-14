/*
 * plcode.h — PL/DPL (CTCSS/DCS) Codec Library
 *
 * Encode and decode CTCSS (PL) tones and DCS (DPL) codes
 * in signed 16-bit linear PCM audio.
 *
 * C99, no external dependencies beyond libc + libm.
 */

#ifndef PLCODE_H
#define PLCODE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Error codes */
#define PLCODE_OK            0
#define PLCODE_ERR_PARAM    -1
#define PLCODE_ERR_ALLOC    -2
#define PLCODE_ERR_RATE     -3

/* Number of standard tones/codes */
#define PLCODE_CTCSS_NUM_TONES  50
#define PLCODE_DCS_NUM_CODES    106

/* Supported sample rates */
#define PLCODE_RATE_8000   8000
#define PLCODE_RATE_16000 16000
#define PLCODE_RATE_32000 32000
#define PLCODE_RATE_48000 48000

/* ── CTCSS tone table ── */

/* Returns frequency in tenths of Hz (e.g., 670 = 67.0 Hz).
 * index: 0 .. PLCODE_CTCSS_NUM_TONES-1
 * Returns 0 on invalid index. */
uint16_t plcode_ctcss_tone_freq_x10(int index);

/* Returns the index for a frequency (tenths of Hz), or -1 if not found. */
int plcode_ctcss_tone_index(uint16_t freq_x10);

/* ── DCS code table ── */

/* Returns the octal code number (e.g., 23, 25, 26 ...).
 * index: 0 .. PLCODE_DCS_NUM_CODES-1
 * Returns 0 on invalid index. */
uint16_t plcode_dcs_code_number(int index);

/* Returns the index for an octal code number, or -1 if not found. */
int plcode_dcs_code_index(uint16_t octal_code);

/* ── CTCSS Encoder ── */

typedef struct plcode_ctcss_enc plcode_ctcss_enc_t;

/* Create a CTCSS tone encoder.
 *   ctx:       Receives allocated context pointer.
 *   rate:      Sample rate (8000/16000/32000/48000).
 *   freq_x10:  Tone frequency in tenths of Hz (e.g., 670 for 67.0 Hz).
 *   amplitude: Peak amplitude 0..32767 (typical: 1000-3000 for ~3-10% of full scale).
 */
int plcode_ctcss_enc_create(plcode_ctcss_enc_t **ctx,
                            int rate, uint16_t freq_x10, int16_t amplitude);

/* Mix CTCSS tone into PCM buffer (additive, with clamping).
 * buf: signed 16-bit PCM samples, modified in place.
 * n:   number of samples. */
void plcode_ctcss_enc_process(plcode_ctcss_enc_t *ctx, int16_t *buf, size_t n);

/* Destroy encoder, free all memory. */
void plcode_ctcss_enc_destroy(plcode_ctcss_enc_t *ctx);

/* ── CTCSS Decoder ── */

typedef struct plcode_ctcss_dec plcode_ctcss_dec_t;

typedef struct {
    int      detected;     /* 1 if a tone is detected, 0 otherwise */
    int      tone_index;   /* Index into tone table, or -1 */
    uint16_t tone_freq_x10;/* Detected frequency in tenths of Hz, or 0 */
    int32_t  magnitude;    /* Relative magnitude of detected tone */
} plcode_ctcss_result_t;

/* Create a CTCSS tone decoder.
 *   ctx:  Receives allocated context pointer.
 *   rate: Sample rate (8000/16000/32000/48000). */
int plcode_ctcss_dec_create(plcode_ctcss_dec_t **ctx, int rate);

/* Feed PCM samples to decoder.
 * buf:    signed 16-bit PCM samples (not modified).
 * n:      number of samples.
 * result: detection result (updated when a block completes). */
void plcode_ctcss_dec_process(plcode_ctcss_dec_t *ctx,
                              const int16_t *buf, size_t n,
                              plcode_ctcss_result_t *result);

/* Reset decoder state (e.g., on channel change). */
void plcode_ctcss_dec_reset(plcode_ctcss_dec_t *ctx);

/* Destroy decoder, free all memory. */
void plcode_ctcss_dec_destroy(plcode_ctcss_dec_t *ctx);

/* ── DCS Encoder ── */

typedef struct plcode_dcs_enc plcode_dcs_enc_t;

/* Create a DCS code encoder.
 *   ctx:       Receives allocated context pointer.
 *   rate:      Sample rate (8000/16000/32000/48000).
 *   code:      Octal code number (e.g., 23).
 *   inverted:  0 for normal, 1 for inverted (DPL).
 *   amplitude: Peak amplitude 0..32767. */
int plcode_dcs_enc_create(plcode_dcs_enc_t **ctx,
                          int rate, uint16_t code, int inverted,
                          int16_t amplitude);

/* Mix DCS baseband into PCM buffer (additive, with clamping).
 * buf: signed 16-bit PCM samples, modified in place.
 * n:   number of samples. */
void plcode_dcs_enc_process(plcode_dcs_enc_t *ctx, int16_t *buf, size_t n);

/* Destroy encoder, free all memory. */
void plcode_dcs_enc_destroy(plcode_dcs_enc_t *ctx);

/* ── DCS Decoder ── */

typedef struct plcode_dcs_dec plcode_dcs_dec_t;

typedef struct {
    int      detected;    /* 1 if a code is detected, 0 otherwise */
    int      code_index;  /* Index into code table, or -1 */
    uint16_t code_number; /* Octal code number, or 0 */
    int      inverted;    /* 1 if inverted (DPL), 0 if normal */
} plcode_dcs_result_t;

/* Create a DCS code decoder.
 *   ctx:  Receives allocated context pointer.
 *   rate: Sample rate (8000/16000/32000/48000). */
int plcode_dcs_dec_create(plcode_dcs_dec_t **ctx, int rate);

/* Feed PCM samples to decoder.
 * buf:    signed 16-bit PCM samples (not modified).
 * n:      number of samples.
 * result: detection result (updated on state change). */
void plcode_dcs_dec_process(plcode_dcs_dec_t *ctx,
                            const int16_t *buf, size_t n,
                            plcode_dcs_result_t *result);

/* Reset decoder state. */
void plcode_dcs_dec_reset(plcode_dcs_dec_t *ctx);

/* Destroy decoder, free all memory. */
void plcode_dcs_dec_destroy(plcode_dcs_dec_t *ctx);

/* ── Golay (23,12) — exposed for testing ── */

/* Encode 12-bit data word to 23-bit Golay codeword. */
uint32_t plcode_golay_encode(uint16_t data12);

/* Check if a 23-bit word is a valid Golay codeword (remainder == 0). */
int plcode_golay_check(uint32_t codeword23);

#ifdef __cplusplus
}
#endif

#endif /* PLCODE_H */
