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

/* Returns the DCS code label (e.g., 23, 25, 26, 47, 754 ...).
 * Labels use octal digits written as a decimal integer:
 *   DCS 023 → 23, DCS 047 → 47, DCS 754 → 754.
 * index: 0 .. PLCODE_DCS_NUM_CODES-1
 * Returns 0 on invalid index. */
uint16_t plcode_dcs_code_number(int index);

/* Returns the index for a DCS code label, or -1 if not found. */
int plcode_dcs_code_index(uint16_t code_label);

/* Convert DCS label (23) to internal binary value (19).
 * Returns 0 on invalid label (contains non-octal digit). */
uint16_t plcode_dcs_label_to_code(int label);

/* Convert internal binary value (19) to DCS label (23). */
int plcode_dcs_code_to_label(uint16_t code);

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

/* Trigger reverse burst (180° phase inversion for ~200ms, then stop).
 * Call during normal tone generation to signal end of transmission. */
void plcode_ctcss_enc_reverse_burst(plcode_ctcss_enc_t *ctx);

/* Returns 1 if the encoder has stopped after completing a reverse burst. */
int plcode_ctcss_enc_stopped(plcode_ctcss_enc_t *ctx);

/* Resume normal tone generation after a reverse burst stop. */
void plcode_ctcss_enc_resume(plcode_ctcss_enc_t *ctx);

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
 *   code:      DCS code label (e.g., 23 for DCS 023, 47 for DCS 047).
 *              Labels use octal digits written as a decimal integer.
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

/* Trigger turn-off code (complemented codeword for ~3 cycles, then stop).
 * Call during normal code generation to signal end of transmission. */
void plcode_dcs_enc_turn_off(plcode_dcs_enc_t *ctx);

/* Returns 1 if the encoder has stopped after completing turn-off. */
int plcode_dcs_enc_stopped(plcode_dcs_enc_t *ctx);

/* Resume normal code generation after turn-off. */
void plcode_dcs_enc_resume(plcode_dcs_enc_t *ctx);

/* ── DCS Decoder ── */

typedef struct plcode_dcs_dec plcode_dcs_dec_t;

typedef struct {
    int      detected;    /* 1 if a code is detected, 0 otherwise */
    int      code_index;  /* Index into code table, or -1 */
    uint16_t code_number; /* DCS code label (e.g., 23 for DCS 023), or 0 */
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

/* ── DTMF digit table ── */

#define PLCODE_DTMF_NUM_DIGITS 16

/* Returns digit character ('0'-'9','*','#','A'-'D') for index 0..15.
 * Returns '\0' on invalid index. */
char plcode_dtmf_digit_char(int index);

/* Returns index 0..15 for a digit character, or -1 if invalid.
 * Accepts uppercase A-D only. */
int plcode_dtmf_digit_index(char digit);

/* ── DTMF Encoder ── */

typedef struct plcode_dtmf_enc plcode_dtmf_enc_t;

/* Create a DTMF tone encoder.
 *   ctx:       Receives allocated context pointer.
 *   rate:      Sample rate (8000/16000/32000/48000).
 *   digit:     DTMF digit character ('0'-'9','*','#','A'-'D').
 *   amplitude: Peak amplitude per tone 0..32767.
 *              Combined output peaks at 2x amplitude.
 */
int plcode_dtmf_enc_create(plcode_dtmf_enc_t **ctx,
                            int rate, char digit, int16_t amplitude);

/* Mix DTMF dual-tone into PCM buffer (additive, with clamping).
 * buf: signed 16-bit PCM samples, modified in place.
 * n:   number of samples. */
void plcode_dtmf_enc_process(plcode_dtmf_enc_t *ctx, int16_t *buf, size_t n);

/* Destroy encoder, free all memory. */
void plcode_dtmf_enc_destroy(plcode_dtmf_enc_t *ctx);

/* ── DTMF Decoder ── */

typedef struct plcode_dtmf_dec plcode_dtmf_dec_t;

typedef struct {
    int      detected;     /* 1 if a DTMF digit is detected, 0 otherwise */
    int      digit_index;  /* Index 0..15, or -1 */
    char     digit;        /* Digit character ('0'-'9','*','#','A'-'D'), or '\0' */
    uint16_t row_freq;     /* Row frequency in Hz, or 0 */
    uint16_t col_freq;     /* Column frequency in Hz, or 0 */
} plcode_dtmf_result_t;

/* DTMF decoder options (zero-initialized → all defaults).
 * Pass NULL to plcode_dtmf_dec_create_ex() for all defaults. */
typedef struct {
    int hits_to_begin;       /* Blocks to confirm onset (default: 2) */
    int misses_to_end;       /* Blocks to confirm offset (default: 3) */
    int min_off_frames;      /* Same-digit cooldown blocks (default: 2) */
    int normal_twist_x;      /* Normal twist power ratio (default: 16 = 12dB) */
    int reverse_twist_x;     /* Reverse twist power ratio (default: 4 = 6dB) */
    int harmonic_reject;     /* Enable 2nd harmonic check (default: 1) */
    int harmonic_thresh_pct; /* Harmonic limit as % of fundamental (default: 50) */
} plcode_dtmf_dec_opts_t;

/* Create a DTMF tone decoder.
 *   ctx:  Receives allocated context pointer.
 *   rate: Sample rate (8000/16000/32000/48000). */
int plcode_dtmf_dec_create(plcode_dtmf_dec_t **ctx, int rate);

/* Create a DTMF tone decoder with custom options.
 *   ctx:  Receives allocated context pointer.
 *   rate: Sample rate (8000/16000/32000/48000).
 *   opts: Options (NULL → all defaults, zero fields → default). */
int plcode_dtmf_dec_create_ex(plcode_dtmf_dec_t **ctx, int rate,
                               const plcode_dtmf_dec_opts_t *opts);

/* Feed PCM samples to decoder.
 * buf:    signed 16-bit PCM samples (not modified).
 * n:      number of samples.
 * result: detection result (updated when a block completes). */
void plcode_dtmf_dec_process(plcode_dtmf_dec_t *ctx,
                              const int16_t *buf, size_t n,
                              plcode_dtmf_result_t *result);

/* Reset decoder state (e.g., on channel change). */
void plcode_dtmf_dec_reset(plcode_dtmf_dec_t *ctx);

/* Destroy decoder, free all memory. */
void plcode_dtmf_dec_destroy(plcode_dtmf_dec_t *ctx);

/* ── CW ID (Morse Code) ── */

#define PLCODE_CWID_NUM_CHARS 37

/* Returns Morse pattern for a character (e.g., ".-" for 'A'), or NULL.
 * Accepts: A-Z (case insensitive), 0-9, /. */
const char *plcode_cwid_morse(char ch);

/* Returns the character for a Morse pattern, or '\0' if not found. */
char plcode_cwid_decode(const char *pattern);

/* ── CW ID Encoder ── */

typedef struct plcode_cwid_enc plcode_cwid_enc_t;

/* Create a CW ID encoder.
 *   ctx:       Receives allocated context pointer.
 *   rate:      Sample rate (8000/16000/32000/48000).
 *   message:   Null-terminated string (A-Z, 0-9, /, space).
 *   freq:      Tone frequency in Hz (typical: 800, range: 300-3000).
 *   wpm:       Speed in words per minute (range: 5-40).
 *   amplitude: Peak amplitude 0..32767.
 */
int plcode_cwid_enc_create(plcode_cwid_enc_t **ctx,
                            int rate, const char *message,
                            int freq, int wpm, int16_t amplitude);

/* Mix CW ID tone into PCM buffer (additive, with clamping).
 * After the message is complete, does not modify remaining samples. */
void plcode_cwid_enc_process(plcode_cwid_enc_t *ctx, int16_t *buf, size_t n);

/* Returns 1 if the entire message has been sent, 0 otherwise. */
int plcode_cwid_enc_complete(plcode_cwid_enc_t *ctx);

/* Destroy encoder, free all memory. */
void plcode_cwid_enc_destroy(plcode_cwid_enc_t *ctx);

/* ── CW ID Decoder ── */

typedef struct plcode_cwid_dec plcode_cwid_dec_t;

typedef struct {
    int  tone_active;    /* 1 if CW tone is currently detected */
    int  new_character;  /* 1 if a new character was decoded in this call */
    char character;      /* Most recently decoded character, or '\0' */
} plcode_cwid_result_t;

/* Create a CW ID decoder.
 *   ctx:  Receives allocated context pointer.
 *   rate: Sample rate (8000/16000/32000/48000).
 *   freq: Expected tone frequency in Hz (300-3000).
 *   wpm:  Expected speed in words per minute (5-40). */
int plcode_cwid_dec_create(plcode_cwid_dec_t **ctx,
                            int rate, int freq, int wpm);

/* Feed PCM samples to decoder.
 * buf:    signed 16-bit PCM samples (not modified).
 * n:      number of samples.
 * result: detection result (updated at end of call). */
void plcode_cwid_dec_process(plcode_cwid_dec_t *ctx,
                              const int16_t *buf, size_t n,
                              plcode_cwid_result_t *result);

/* Get the full decoded message accumulated so far.
 * Returns pointer to internal null-terminated buffer. */
const char *plcode_cwid_dec_message(plcode_cwid_dec_t *ctx);

/* Reset decoder state. */
void plcode_cwid_dec_reset(plcode_cwid_dec_t *ctx);

/* Destroy decoder, free all memory. */
void plcode_cwid_dec_destroy(plcode_cwid_dec_t *ctx);

/* ── MCW (Modulated CW) Encoder ── */

typedef struct plcode_mcw_enc plcode_mcw_enc_t;

/* Create an MCW encoder (CW with raised-cosine shaped keying).
 *   ctx:       Receives allocated context pointer.
 *   rate:      Sample rate (8000/16000/32000/48000).
 *   message:   Null-terminated string (A-Z, 0-9, /, space).
 *   freq:      Tone frequency in Hz (range: 300-3000).
 *   wpm:       Speed in words per minute (range: 5-40).
 *   amplitude: Peak amplitude 0..32767.
 */
int plcode_mcw_enc_create(plcode_mcw_enc_t **ctx,
                           int rate, const char *message,
                           int freq, int wpm, int16_t amplitude);

/* Mix MCW tone into PCM buffer (additive, with clamping).
 * After the message is complete, does not modify remaining samples. */
void plcode_mcw_enc_process(plcode_mcw_enc_t *ctx, int16_t *buf, size_t n);

/* Returns 1 if the entire message has been sent, 0 otherwise. */
int plcode_mcw_enc_complete(plcode_mcw_enc_t *ctx);

/* Destroy encoder, free all memory. */
void plcode_mcw_enc_destroy(plcode_mcw_enc_t *ctx);

/* ── MCW Decoder ── */

typedef struct plcode_mcw_dec plcode_mcw_dec_t;

/* Create an MCW decoder.
 *   ctx:  Receives allocated context pointer.
 *   rate: Sample rate (8000/16000/32000/48000).
 *   freq: Expected tone frequency in Hz (300-3000).
 *   wpm:  Expected speed in words per minute (5-40). */
int plcode_mcw_dec_create(plcode_mcw_dec_t **ctx,
                           int rate, int freq, int wpm);

/* Feed PCM samples to decoder.
 * buf:    signed 16-bit PCM samples (not modified).
 * n:      number of samples.
 * result: detection result (updated at end of call). */
void plcode_mcw_dec_process(plcode_mcw_dec_t *ctx,
                             const int16_t *buf, size_t n,
                             plcode_cwid_result_t *result);

/* Get the full decoded message accumulated so far. */
const char *plcode_mcw_dec_message(plcode_mcw_dec_t *ctx);

/* Reset decoder state. */
void plcode_mcw_dec_reset(plcode_mcw_dec_t *ctx);

/* Destroy decoder, free all memory. */
void plcode_mcw_dec_destroy(plcode_mcw_dec_t *ctx);

/* ── FSK CW Encoder ── */

typedef struct plcode_fskcw_enc plcode_fskcw_enc_t;

/* Create an FSK CW encoder (Morse via frequency-shift keying).
 *   ctx:        Receives allocated context pointer.
 *   rate:       Sample rate (8000/16000/32000/48000).
 *   message:    Null-terminated string (A-Z, 0-9, /, space).
 *   mark_freq:  Frequency for dits/dahs in Hz (300-3000).
 *   space_freq: Frequency for gaps in Hz (300-3000, must differ from mark).
 *   wpm:        Speed in words per minute (range: 5-40).
 *   amplitude:  Peak amplitude 0..32767.
 */
int plcode_fskcw_enc_create(plcode_fskcw_enc_t **ctx,
                              int rate, const char *message,
                              int mark_freq, int space_freq,
                              int wpm, int16_t amplitude);

/* Mix FSK CW tone into PCM buffer (additive, with clamping).
 * After the message is complete, does not modify remaining samples. */
void plcode_fskcw_enc_process(plcode_fskcw_enc_t *ctx, int16_t *buf, size_t n);

/* Returns 1 if the entire message has been sent, 0 otherwise. */
int plcode_fskcw_enc_complete(plcode_fskcw_enc_t *ctx);

/* Destroy encoder, free all memory. */
void plcode_fskcw_enc_destroy(plcode_fskcw_enc_t *ctx);

/* ── FSK CW Decoder ── */

typedef struct plcode_fskcw_dec plcode_fskcw_dec_t;

/* Create an FSK CW decoder.
 *   ctx:        Receives allocated context pointer.
 *   rate:       Sample rate (8000/16000/32000/48000).
 *   mark_freq:  Mark frequency in Hz (300-3000).
 *   space_freq: Space frequency in Hz (300-3000).
 *   wpm:        Expected speed in words per minute (5-40). */
int plcode_fskcw_dec_create(plcode_fskcw_dec_t **ctx,
                              int rate, int mark_freq, int space_freq,
                              int wpm);

/* Feed PCM samples to decoder.
 * buf:    signed 16-bit PCM samples (not modified).
 * n:      number of samples.
 * result: detection result (updated at end of call). */
void plcode_fskcw_dec_process(plcode_fskcw_dec_t *ctx,
                               const int16_t *buf, size_t n,
                               plcode_cwid_result_t *result);

/* Get the full decoded message accumulated so far. */
const char *plcode_fskcw_dec_message(plcode_fskcw_dec_t *ctx);

/* Reset decoder state. */
void plcode_fskcw_dec_reset(plcode_fskcw_dec_t *ctx);

/* Destroy decoder, free all memory. */
void plcode_fskcw_dec_destroy(plcode_fskcw_dec_t *ctx);

/* ── Two-Tone Sequential Paging ── */

#define PLCODE_TWOTONE_NUM_FREQS 33

/* Returns paging tone frequency in tenths of Hz for index 0..32.
 * Returns 0 on invalid index. */
uint16_t plcode_twotone_freq_x10(int index);

/* Returns index for a paging frequency (tenths Hz), or -1 if not found. */
int plcode_twotone_freq_index(uint16_t freq_x10);

/* ── Two-Tone Paging Encoder ── */

typedef struct plcode_twotone_enc plcode_twotone_enc_t;

/* Create a two-tone paging encoder.
 *   ctx:           Receives allocated context pointer.
 *   rate:          Sample rate (8000/16000/32000/48000).
 *   tone_a_freq:   First tone frequency in tenths of Hz.
 *   tone_b_freq:   Second tone frequency in tenths of Hz.
 *   tone_a_ms:     Duration of first tone in ms (typical: 1000).
 *   tone_b_ms:     Duration of second tone in ms (typical: 3000).
 *   amplitude:     Peak amplitude 0..32767.
 */
int plcode_twotone_enc_create(plcode_twotone_enc_t **ctx,
                               int rate, uint16_t tone_a_freq,
                               uint16_t tone_b_freq,
                               int tone_a_ms, int tone_b_ms,
                               int16_t amplitude);

void plcode_twotone_enc_process(plcode_twotone_enc_t *ctx, int16_t *buf, size_t n);
int plcode_twotone_enc_complete(plcode_twotone_enc_t *ctx);
void plcode_twotone_enc_destroy(plcode_twotone_enc_t *ctx);

/* ── Two-Tone Paging Decoder ── */

typedef struct plcode_twotone_dec plcode_twotone_dec_t;

typedef struct {
    int      detected;        /* 1 if both tones received in sequence */
    uint16_t tone_a_freq_x10; /* First tone in tenths of Hz, or 0 */
    uint16_t tone_b_freq_x10; /* Second tone in tenths of Hz, or 0 */
    int      tone_a_index;    /* Index into freq table, or -1 */
    int      tone_b_index;    /* Index into freq table, or -1 */
} plcode_twotone_result_t;

int plcode_twotone_dec_create(plcode_twotone_dec_t **ctx, int rate);

void plcode_twotone_dec_process(plcode_twotone_dec_t *ctx,
                                 const int16_t *buf, size_t n,
                                 plcode_twotone_result_t *result);

void plcode_twotone_dec_reset(plcode_twotone_dec_t *ctx);
void plcode_twotone_dec_destroy(plcode_twotone_dec_t *ctx);

/* ── Five-Tone Selective Call ── */

typedef enum {
    PLCODE_SELCALL_ZVEI1 = 0,
    PLCODE_SELCALL_CCIR  = 1,
    PLCODE_SELCALL_EIA   = 2
} plcode_selcall_std_t;

#define PLCODE_SELCALL_NUM_TONES 12  /* digits 0-9, R(repeat), G(group) */
#define PLCODE_SELCALL_ADDR_LEN  5

/* Returns tone frequency in Hz for a selcall standard and digit index (0-11).
 * Returns 0 on invalid input. */
uint16_t plcode_selcall_freq(plcode_selcall_std_t standard, int digit);

/* ── Selcall Encoder ── */

typedef struct plcode_selcall_enc plcode_selcall_enc_t;

/* Create a five-tone selcall encoder.
 *   ctx:       Receives allocated context pointer.
 *   rate:      Sample rate (8000/16000/32000/48000).
 *   standard:  PLCODE_SELCALL_ZVEI1, CCIR, or EIA.
 *   address:   5-digit string using chars '0'-'9' (e.g., "12345").
 *   amplitude: Peak amplitude 0..32767.
 */
int plcode_selcall_enc_create(plcode_selcall_enc_t **ctx,
                               int rate, plcode_selcall_std_t standard,
                               const char *address, int16_t amplitude);

void plcode_selcall_enc_process(plcode_selcall_enc_t *ctx, int16_t *buf, size_t n);
int plcode_selcall_enc_complete(plcode_selcall_enc_t *ctx);
void plcode_selcall_enc_destroy(plcode_selcall_enc_t *ctx);

/* ── Selcall Decoder ── */

typedef struct plcode_selcall_dec plcode_selcall_dec_t;

typedef struct {
    int  detected;                            /* 1 if 5-tone sequence detected */
    char address[PLCODE_SELCALL_ADDR_LEN+1];  /* Decoded address, null-terminated */
} plcode_selcall_result_t;

int plcode_selcall_dec_create(plcode_selcall_dec_t **ctx,
                               int rate, plcode_selcall_std_t standard);

void plcode_selcall_dec_process(plcode_selcall_dec_t *ctx,
                                 const int16_t *buf, size_t n,
                                 plcode_selcall_result_t *result);

void plcode_selcall_dec_reset(plcode_selcall_dec_t *ctx);
void plcode_selcall_dec_destroy(plcode_selcall_dec_t *ctx);

/* ── Access Tone Burst ── */

typedef struct plcode_toneburst_enc plcode_toneburst_enc_t;

/* Create an access tone burst encoder.
 *   ctx:         Receives allocated context pointer.
 *   rate:        Sample rate (8000/16000/32000/48000).
 *   freq:        Tone frequency in Hz (typical: 1750, range: 300-3000).
 *   duration_ms: Burst duration in ms (typical: 500-1000).
 *   amplitude:   Peak amplitude 0..32767.
 */
int plcode_toneburst_enc_create(plcode_toneburst_enc_t **ctx,
                                 int rate, int freq, int duration_ms,
                                 int16_t amplitude);

void plcode_toneburst_enc_process(plcode_toneburst_enc_t *ctx,
                                   int16_t *buf, size_t n);
int plcode_toneburst_enc_complete(plcode_toneburst_enc_t *ctx);
void plcode_toneburst_enc_destroy(plcode_toneburst_enc_t *ctx);

/* ── Tone Burst Decoder ── */

typedef struct plcode_toneburst_dec plcode_toneburst_dec_t;

typedef struct {
    int detected;     /* 1 when burst confirmed (met minimum duration) */
    int tone_active;  /* 1 while tone is present */
} plcode_toneburst_result_t;

/* Create a tone burst decoder.
 *   ctx:            Receives allocated context pointer.
 *   rate:           Sample rate (8000/16000/32000/48000).
 *   freq:           Expected frequency in Hz (300-3000).
 *   min_duration_ms: Minimum burst duration to confirm (typical: 200-500).
 */
int plcode_toneburst_dec_create(plcode_toneburst_dec_t **ctx,
                                 int rate, int freq, int min_duration_ms);

void plcode_toneburst_dec_process(plcode_toneburst_dec_t *ctx,
                                   const int16_t *buf, size_t n,
                                   plcode_toneburst_result_t *result);

void plcode_toneburst_dec_reset(plcode_toneburst_dec_t *ctx);
void plcode_toneburst_dec_destroy(plcode_toneburst_dec_t *ctx);

/* ── MDC-1200 Signaling ── */

#define PLCODE_MDC1200_OP_PTT_PRE  0x01  /* Pre-key PTT ID */
#define PLCODE_MDC1200_OP_PTT_POST 0x00  /* Post-key PTT ID */
#define PLCODE_MDC1200_OP_EMERG    0x80  /* Emergency */
#define PLCODE_MDC1200_OP_ACK      0x20  /* Acknowledge */

typedef struct plcode_mdc1200_enc plcode_mdc1200_enc_t;

/* Create an MDC-1200 encoder.
 *   ctx:       Receives allocated context pointer.
 *   rate:      Sample rate (8000/16000/32000/48000).
 *   op:        Operation code (e.g., PLCODE_MDC1200_OP_PTT_PRE).
 *   arg:       Argument byte.
 *   unit_id:   Unit ID (0x0001-0xFFFF).
 *   amplitude: Peak amplitude 0..32767.
 */
int plcode_mdc1200_enc_create(plcode_mdc1200_enc_t **ctx,
                               int rate, uint8_t op, uint8_t arg,
                               uint16_t unit_id, int16_t amplitude);

void plcode_mdc1200_enc_process(plcode_mdc1200_enc_t *ctx,
                                 int16_t *buf, size_t n);
int plcode_mdc1200_enc_complete(plcode_mdc1200_enc_t *ctx);
void plcode_mdc1200_enc_destroy(plcode_mdc1200_enc_t *ctx);

/* ── MDC-1200 Decoder ── */

typedef struct plcode_mdc1200_dec plcode_mdc1200_dec_t;

typedef struct {
    int      detected;   /* 1 when a valid packet is received */
    uint8_t  op;         /* Operation code */
    uint8_t  arg;        /* Argument byte */
    uint16_t unit_id;    /* Unit ID */
} plcode_mdc1200_result_t;

int plcode_mdc1200_dec_create(plcode_mdc1200_dec_t **ctx, int rate);

void plcode_mdc1200_dec_process(plcode_mdc1200_dec_t *ctx,
                                 const int16_t *buf, size_t n,
                                 plcode_mdc1200_result_t *result);

void plcode_mdc1200_dec_reset(plcode_mdc1200_dec_t *ctx);
void plcode_mdc1200_dec_destroy(plcode_mdc1200_dec_t *ctx);

/* ── Courtesy Tone Generator ── */

typedef struct plcode_courtesy_enc plcode_courtesy_enc_t;

typedef struct {
    int      freq;       /* Tone frequency in Hz (0 = silence/gap) */
    int      duration_ms;/* Duration in milliseconds */
    int16_t  amplitude;  /* Peak amplitude 0..32767 */
} plcode_courtesy_tone_t;

/* Create a courtesy tone generator.
 *   ctx:       Receives allocated context pointer.
 *   rate:      Sample rate (8000/16000/32000/48000).
 *   tones:     Array of tone descriptors.
 *   num_tones: Number of entries in the array.
 */
int plcode_courtesy_enc_create(plcode_courtesy_enc_t **ctx,
                                int rate,
                                const plcode_courtesy_tone_t *tones,
                                int num_tones);

void plcode_courtesy_enc_process(plcode_courtesy_enc_t *ctx,
                                  int16_t *buf, size_t n);
int plcode_courtesy_enc_complete(plcode_courtesy_enc_t *ctx);
void plcode_courtesy_enc_destroy(plcode_courtesy_enc_t *ctx);

/* ── Tone Generator (arbitrary frequency) ── */

typedef struct plcode_tone_enc plcode_tone_enc_t;

/* Create a single-frequency tone generator.
 *   ctx:       Receives allocated context pointer.
 *   rate:      Sample rate (8000/16000/32000/48000).
 *   freq:      Tone frequency in Hz (1-4000).
 *   amplitude: Peak amplitude 0..32767.
 */
int plcode_tone_enc_create(plcode_tone_enc_t **ctx,
                            int rate, int freq, int16_t amplitude);

/* Mix tone into PCM buffer (additive, with clamping).
 * buf: signed 16-bit PCM samples, modified in place.
 * n:   number of samples. */
void plcode_tone_enc_process(plcode_tone_enc_t *ctx, int16_t *buf, size_t n);

/* Destroy encoder, free all memory. */
void plcode_tone_enc_destroy(plcode_tone_enc_t *ctx);

/* ── Golay (23,12) — exposed for testing ── */

/* Encode 12-bit data word to 23-bit Golay codeword. */
uint32_t plcode_golay_encode(uint16_t data12);

/* Check if a 23-bit word is a valid Golay codeword (remainder == 0). */
int plcode_golay_check(uint32_t codeword23);

#ifdef __cplusplus
}
#endif

#endif /* PLCODE_H */
