#ifndef PLCODE_INTERNAL_H
#define PLCODE_INTERNAL_H

#include "plcode.h"

/* ── Sine LUT ── */
#define PLCODE_SINE_LUT_SIZE 1024

extern int16_t plcode_sine_lut[PLCODE_SINE_LUT_SIZE];

/* ── CTCSS tone table (tenths of Hz) ── */
extern const uint16_t plcode_ctcss_tones[PLCODE_CTCSS_NUM_TONES];

/* ── DCS code table (octal numbers) ── */
extern const uint16_t plcode_dcs_codes[PLCODE_DCS_NUM_CODES];

/* ── DCS precomputed Golay codewords (filled at init) ── */
extern uint32_t plcode_dcs_codewords[PLCODE_DCS_NUM_CODES];

/* ── Table initialization (call before first use) ── */
void plcode_tables_init(void);

/* ── Fixed-point helpers ── */

static inline int16_t plcode_clamp16(int32_t x)
{
    if (x > 32767)  return 32767;
    if (x < -32768) return -32768;
    return (int16_t)x;
}

/* Lookup sine from phase accumulator (top 10 bits). */
static inline int16_t plcode_sine_lookup(uint32_t phase)
{
    return plcode_sine_lut[(phase >> 22) & (PLCODE_SINE_LUT_SIZE - 1)];
}

/* Scale Q15 sine value by amplitude. */
static inline int16_t plcode_scale(int16_t sine_val, int16_t amplitude)
{
    return (int16_t)(((int32_t)sine_val * amplitude) >> 15);
}

/* ── Validate sample rate ── */
static inline int plcode_valid_rate(int rate)
{
    return (rate == 8000 || rate == 16000 || rate == 32000 || rate == 48000);
}

/* ── Golay polynomial ── */
#define PLCODE_GOLAY_POLY 0xAE3u  /* x^11+x^9+x^7+x^6+x^5+x+1 (TIA/EIA-603) */

/* ── DCS constants ── */
#define PLCODE_DCS_BITRATE     134.4   /* bits per second */
#define PLCODE_DCS_CODEWORD_BITS 23

/* ── CTCSS Encoder context ── */
struct plcode_ctcss_enc {
    uint32_t phase;          /* Phase accumulator */
    uint32_t phase_inc;      /* Phase increment per sample */
    int16_t  amplitude;      /* Peak amplitude */
};

/* ── CTCSS Decoder context ── */
struct plcode_ctcss_dec {
    int      rate;
    int      block_size;     /* = sample_rate (1 second window) */
    int      sample_count;   /* Samples accumulated in current block */

    /* Goertzel state for each tone (int64 to avoid overflow at high rates) */
    int64_t  s1[PLCODE_CTCSS_NUM_TONES]; /* s[n-1] */
    int64_t  s2[PLCODE_CTCSS_NUM_TONES]; /* s[n-2] */
    int32_t  coeff[PLCODE_CTCSS_NUM_TONES]; /* 2*cos(2*pi*f/fs) in Q28 */

    /* Detection state */
    int      prev_tone;      /* Previous detected tone index (-1 = none) */
    int      confirm_count;  /* Consecutive detections of same tone */
};

/* ── DCS Encoder context ── */
struct plcode_dcs_enc {
    int      rate;
    uint32_t codeword;       /* 23-bit Golay codeword */
    int      inverted;       /* XOR flag */
    int16_t  amplitude;

    /* Bit timing */
    uint32_t bit_phase;      /* Fractional accumulator for bit clock */
    uint32_t bit_phase_inc;  /* Increment per sample */
    int      bit_index;      /* Current bit in codeword (0..22) */

    /* IIR LPF state */
    int32_t  lpf_state;      /* Single-pole IIR state, Q15 */
    int32_t  lpf_alpha;      /* Filter coefficient, Q15 */
};

/* ── DCS Decoder context ── */
struct plcode_dcs_dec {
    int      rate;

    /* 2nd-order Butterworth LPF (Q14 coefficients) */
    int32_t  lpf_b[3];
    int32_t  lpf_a[2];       /* a1, a2 (negated for direct form) */
    int32_t  lpf_x[2];       /* Input history */
    int32_t  lpf_y[2];       /* Output history */

    /* Comparator with hysteresis */
    int      last_bit;        /* Current hard decision */
    int16_t  hyst_high;       /* High threshold */
    int16_t  hyst_low;        /* Low threshold */

    /* PLL bit clock */
    uint32_t pll_phase;       /* Phase accumulator */
    uint32_t pll_inc;         /* Nominal increment per sample */
    int      prev_input;      /* Previous comparator output for edge detect */

    /* Shift register */
    uint32_t shift_reg;       /* 23-bit shift register */
    int      total_bits;      /* Total bits shifted in (for initial fill) */

    /* Detection state */
    int      match_code;      /* Matched code index, or -1 */
    int      match_inv;       /* Matched inverted flag */
    int      match_count;     /* Consecutive match count */
    int      bits_since_match;/* Bits since last match (for alignment) */
    int      confirmed_code;  /* Confirmed code index */
    int      confirmed_inv;   /* Confirmed inverted flag */
    int      confirmed;       /* Detection confirmed */
};

/* ── DTMF tables ── */
extern const uint16_t plcode_dtmf_row_freqs[4];  /* 697, 770, 852, 941 Hz */
extern const uint16_t plcode_dtmf_col_freqs[4];  /* 1209, 1336, 1477, 1633 Hz */
extern const uint16_t plcode_dtmf_row_harmonics[4]; /* 2nd harmonics of rows */
extern const uint16_t plcode_dtmf_col_harmonics[4]; /* 2nd harmonics of cols */
extern const char plcode_dtmf_digits[PLCODE_DTMF_NUM_DIGITS];

/* ── DTMF constants ── */
#define PLCODE_DTMF_BLOCK_DIV 50  /* block_size = rate / 50 → 20ms window */

/* ── DTMF Encoder context ── */
struct plcode_dtmf_enc {
    uint32_t row_phase;      /* Phase accumulator for row tone */
    uint32_t row_phase_inc;  /* Phase increment per sample */
    uint32_t col_phase;      /* Phase accumulator for column tone */
    uint32_t col_phase_inc;  /* Phase increment per sample */
    int16_t  amplitude;      /* Peak amplitude per tone */
};

/* ── DTMF Decoder state machine ── */
#define DTMF_ST_IDLE     0
#define DTMF_ST_PENDING  1
#define DTMF_ST_ACTIVE   2
#define DTMF_ST_COOLDOWN 3

/* ── DTMF Decoder context ── */
struct plcode_dtmf_dec {
    int      rate;
    int      block_size;     /* = rate / PLCODE_DTMF_BLOCK_DIV (20ms) */
    int      sample_count;   /* Samples accumulated in current block */

    /* Goertzel state: 0..3 row, 4..7 col, 8..15 harmonics */
    int64_t  s1[16];         /* s[n-1] */
    int64_t  s2[16];         /* s[n-2] */
    int32_t  coeff[16];      /* 2*cos(2*pi*f/fs) in Q28 */

    /* State machine (replaces prev_digit/confirm_count) */
    int      state;          /* DTMF_ST_IDLE/PENDING/ACTIVE/COOLDOWN */
    int      current_digit;  /* Digit index being tracked (-1 = none) */
    int      hit_count;      /* Consecutive hits in PENDING */
    int      miss_count;     /* Consecutive misses in ACTIVE */
    int      cooldown_count; /* Frames remaining in COOLDOWN */
    int      cooldown_digit; /* Digit that triggered cooldown */

    /* Options (copied from opts at create time) */
    int      hits_to_begin;
    int      misses_to_end;
    int      min_off_frames;
    int      normal_twist_x;
    int      reverse_twist_x;
    int      harmonic_reject;
    int      harmonic_thresh_pct;
};

/* ── CW ID (Morse) tables ── */
extern const char plcode_cwid_chars[PLCODE_CWID_NUM_CHARS];
extern const char * const plcode_cwid_patterns[PLCODE_CWID_NUM_CHARS];

/* ── CW ID constants ── */
#define PLCODE_CWID_BLOCK_DIV   100  /* block_size = rate / 100 → 10ms */
#define PLCODE_CWID_MSG_MAX     128
#define PLCODE_CWID_PATTERN_MAX   8

/* ── CW ID Encoder context ── */
struct plcode_cwid_enc {
    uint32_t phase;          /* Phase accumulator for tone */
    uint32_t phase_inc;      /* Phase increment per sample */
    int16_t  amplitude;
    int      dot_samples;    /* Samples per dot-length */

    int8_t  *elements;       /* Element sequence: +N=tone, -N=gap (dot-units), 0=end */
    int      num_elements;
    int      cur_element;    /* Current element index */
    int      cur_sample;     /* Sample position within current element */
    int      cur_duration;   /* Total samples for current element */
    int      complete;       /* 1 if message fully sent */
};

/* ── CW ID Decoder context ── */
struct plcode_cwid_dec {
    int      rate;
    int      block_size;     /* = rate / PLCODE_CWID_BLOCK_DIV (10ms) */
    int      sample_count;

    /* Single Goertzel filter */
    int64_t  s1;
    int64_t  s2;
    int32_t  coeff;          /* 2*cos(2*pi*f/fs) in Q28 */

    /* Timing */
    int      dot_samples;

    /* Tone detection state */
    int      tone_on;
    int      tone_samples;   /* Duration of current tone period */
    int      gap_samples;    /* Duration of current gap period */

    /* Pattern accumulation */
    char     pattern[PLCODE_CWID_PATTERN_MAX];
    int      pattern_len;

    /* Decoded message buffer */
    char     message[PLCODE_CWID_MSG_MAX];
    int      message_len;
    char     last_char;
};

/* ── Tone Generator context ── */
struct plcode_tone_enc {
    uint32_t phase;
    uint32_t phase_inc;
    int16_t  amplitude;
};

#endif /* PLCODE_INTERNAL_H */
