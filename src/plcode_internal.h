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
#define PLCODE_GOLAY_POLY 0xC75u  /* x^11+x^10+x^6+x^5+x^4+x^2+1 */

/* ── DCS constants ── */
#define PLCODE_DCS_BITRATE     134.4   /* bits per second */
#define PLCODE_DCS_CODEWORD_BITS 23

/* ── CTCSS Encoder context ── */
struct plcode_ctcss_enc {
    uint32_t phase;          /* Phase accumulator */
    uint32_t phase_inc;      /* Phase increment per sample */
    int16_t  amplitude;      /* Peak amplitude */
    int      rate;           /* Sample rate (needed for burst duration) */
    int      state;          /* 0=normal, 1=reverse_burst, 2=stopped */
    int      burst_remaining;/* Samples remaining in reverse burst */
};

/* ── CTCSS Decoder context ── */
struct plcode_ctcss_dec {
    int      rate;
    int      block_size;     /* = rate * 3/10 (300ms window) */
    int      half_block;     /* block_size / 2, for midpoint check */
    int      sample_count;   /* Samples accumulated in current block */

    /* Goertzel state for each tone (int64 to avoid overflow at high rates) */
    int64_t  s1[PLCODE_CTCSS_NUM_TONES]; /* s[n-1] */
    int64_t  s2[PLCODE_CTCSS_NUM_TONES]; /* s[n-2] */
    int32_t  coeff[PLCODE_CTCSS_NUM_TONES]; /* 2*cos(2*pi*f/fs) in Q28 */

    /* Detection state */
    int      prev_tone;      /* Previous detected tone index (-1 = none) */
    int      confirm_count;  /* Consecutive detections of same tone */
    int      mid_tone;       /* Best tone at midpoint (-1 = none) */
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

    /* Turn-off state */
    int      state;           /* 0=normal, 1=turn_off, 2=stopped */
    uint32_t turnoff_codeword;/* Golay codeword for complemented code */
    int      turnoff_bits;    /* Bit periods remaining in turn-off */
};

/* ── DCS Decoder context ── */
struct plcode_dcs_dec {
    int      rate;

    /* DC blocker (1st-order HP at ~5 Hz, removes FM demod offset) */
    int32_t  dc_x1;          /* Previous input sample */
    int32_t  dc_y1;          /* Previous output sample (Q15) */
    int32_t  dc_alpha;       /* Pole coefficient, Q15 (~0.996 at 8kHz) */

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
    int64_t  energy_sum;     /* Accumulated sum(sample^2) for current block */

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

/* Build Morse element sequence from message.
 * elements: output array, must have room for max_elements + 1 (0 terminator).
 * Returns number of elements (excluding terminator). */
int plcode_cwid_build_elements(const char *message, int8_t *elements, int max_elements);

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

/* ── MCW Encoder context ── */
struct plcode_mcw_enc {
    uint32_t phase;          /* Phase accumulator for tone */
    uint32_t phase_inc;      /* Phase increment per sample */
    int16_t  amplitude;
    int      dot_samples;    /* Samples per dot-length */
    int      ramp_len;       /* Samples in attack/decay ramp */
    int16_t *ramp;           /* Raised-cosine ramp table (Q15, 0..32767) */

    int8_t  *elements;       /* Element sequence: +N=tone, -N=gap, 0=end */
    int      num_elements;
    int      cur_element;
    int      cur_sample;
    int      cur_duration;
    int      complete;
};

/* ── MCW Decoder context ── */
struct plcode_mcw_dec {
    int      rate;
    int      block_size;     /* = rate / PLCODE_CWID_BLOCK_DIV (10ms) */
    int      sample_count;

    int64_t  s1;
    int64_t  s2;
    int32_t  coeff;          /* 2*cos(2*pi*f/fs) in Q28 */

    int      dot_samples;
    int      tone_on;
    int      tone_samples;
    int      gap_samples;

    char     pattern[PLCODE_CWID_PATTERN_MAX];
    int      pattern_len;
    char     message[PLCODE_CWID_MSG_MAX];
    int      message_len;
    char     last_char;
};

/* ── FSK CW Encoder context ── */
struct plcode_fskcw_enc {
    uint32_t phase;            /* Single phase accumulator (CPFSK) */
    uint32_t mark_phase_inc;   /* Phase increment for mark freq */
    uint32_t space_phase_inc;  /* Phase increment for space freq */
    int16_t  amplitude;
    int      dot_samples;

    int8_t  *elements;
    int      num_elements;
    int      cur_element;
    int      cur_sample;
    int      cur_duration;
    int      complete;
};

/* ── FSK CW Decoder context ── */
struct plcode_fskcw_dec {
    int      rate;
    int      block_size;
    int      sample_count;

    /* Goertzel for mark frequency */
    int64_t  mark_s1, mark_s2;
    int32_t  mark_coeff;

    /* Goertzel for space frequency */
    int64_t  space_s1, space_s2;
    int32_t  space_coeff;

    int      dot_samples;
    int      tone_on;        /* 1 = mark detected, 0 = space detected */
    int      tone_samples;
    int      gap_samples;

    char     pattern[PLCODE_CWID_PATTERN_MAX];
    int      pattern_len;
    char     message[PLCODE_CWID_MSG_MAX];
    int      message_len;
    char     last_char;
};

/* ── Two-Tone Paging tables ── */
#define PLCODE_TWOTONE_BLOCK_DIV 20  /* 50ms blocks = rate / 20 */

extern const uint16_t plcode_twotone_freqs[PLCODE_TWOTONE_NUM_FREQS];

/* ── Two-Tone Paging Encoder context ── */
struct plcode_twotone_enc {
    uint32_t phase;
    uint32_t phase_inc_a;    /* Phase increment for tone A */
    uint32_t phase_inc_b;    /* Phase increment for tone B */
    int16_t  amplitude;
    int      tone_a_samples; /* Total samples for tone A */
    int      tone_b_samples; /* Total samples for tone B */
    int      cur_sample;     /* Current sample position */
    int      complete;
};

/* ── Two-Tone Paging Decoder context ── */
struct plcode_twotone_dec {
    int      rate;
    int      block_size;     /* = rate / PLCODE_TWOTONE_BLOCK_DIV (50ms) */
    int      sample_count;

    /* Goertzel filters for all 33 paging frequencies */
    int64_t  s1[PLCODE_TWOTONE_NUM_FREQS];
    int64_t  s2[PLCODE_TWOTONE_NUM_FREQS];
    int32_t  coeff[PLCODE_TWOTONE_NUM_FREQS];

    /* Detection state */
    int      tone_a_idx;     /* Detected first tone index, or -1 */
    int      tone_a_count;   /* Consecutive blocks with tone A */
    int      tone_b_idx;     /* Detected second tone index, or -1 */
    int      tone_b_count;   /* Consecutive blocks with tone B */
    int      state;          /* 0=waiting_A, 1=in_A, 2=waiting_B, 3=in_B */
    int      detected;
    int      det_a_idx;      /* Confirmed tone A index (preserved after detect) */
    int      det_b_idx;      /* Confirmed tone B index (preserved after detect) */
};

/* ── Selcall tables ── */
#define PLCODE_SELCALL_BLOCK_DIV 100  /* 10ms blocks */
extern const uint16_t plcode_selcall_zvei1[PLCODE_SELCALL_NUM_TONES];
extern const uint16_t plcode_selcall_ccir[PLCODE_SELCALL_NUM_TONES];
extern const uint16_t plcode_selcall_eia[PLCODE_SELCALL_NUM_TONES];

/* Returns pointer to the frequency table for a given standard. */
const uint16_t *plcode_selcall_table(plcode_selcall_std_t std);

/* Returns tone duration in ms for a given standard. */
int plcode_selcall_tone_ms(plcode_selcall_std_t std);

/* ── Selcall Encoder context ── */
struct plcode_selcall_enc {
    uint32_t phase;
    uint32_t phase_inc[PLCODE_SELCALL_ADDR_LEN]; /* Phase inc for each tone */
    int16_t  amplitude;
    int      tone_samples;   /* Samples per tone */
    int      num_tones;      /* Always 5 */
    int      cur_tone;       /* Current tone index (0-4) */
    int      cur_sample;     /* Sample within current tone */
    int      complete;
};

/* ── Selcall Decoder context ── */
struct plcode_selcall_dec {
    int      rate;
    int      block_size;     /* = rate / PLCODE_SELCALL_BLOCK_DIV (10ms) */
    int      sample_count;

    /* Goertzel filters for 12 selcall frequencies */
    int64_t  s1[PLCODE_SELCALL_NUM_TONES];
    int64_t  s2[PLCODE_SELCALL_NUM_TONES];
    int32_t  coeff[PLCODE_SELCALL_NUM_TONES];

    int      tone_samples;   /* Expected samples per tone */
    int      cur_tone;       /* Current detected tone digit, or -1 */
    int      tone_count;     /* Consecutive blocks with same tone */
    int      num_received;   /* Digits received so far */
    char     address[PLCODE_SELCALL_ADDR_LEN + 1];
    int      detected;
};

/* ── Tone Burst Encoder context ── */
struct plcode_toneburst_enc {
    uint32_t phase;
    uint32_t phase_inc;
    int16_t  amplitude;
    int      total_samples;  /* Total samples for burst */
    int      cur_sample;
    int      complete;
};

/* ── Tone Burst Decoder context ── */
struct plcode_toneburst_dec {
    int      rate;
    int      block_size;     /* = rate / 100 (10ms blocks) */
    int      sample_count;

    int64_t  s1;
    int64_t  s2;
    int32_t  coeff;

    int      min_samples;    /* Minimum burst duration in samples */
    int      tone_active;    /* Current tone state */
    int      tone_samples;   /* Accumulated tone duration */
    int      detected;       /* 1 if minimum duration met */
};

/* ── MDC-1200 constants ── */
#define PLCODE_MDC1200_BAUD     1200
#define PLCODE_MDC1200_MARK_HZ  1200
#define PLCODE_MDC1200_SPACE_HZ 1800
#define PLCODE_MDC1200_SYNC     0x07FF
#define PLCODE_MDC1200_PREAMBLE_BITS 40
#define PLCODE_MDC1200_SYNC_BITS     16
#define PLCODE_MDC1200_DATA_BITS     56  /* op(8)+arg(8)+id(16)+crc(16)+checksum(8) */

/* ── MDC-1200 Encoder context ── */
struct plcode_mdc1200_enc {
    uint32_t phase;           /* CPFSK phase accumulator */
    uint32_t mark_phase_inc;
    uint32_t space_phase_inc;
    int16_t  amplitude;

    uint8_t  bits[16];        /* Packet bitstream (packed bytes) */
    int      total_bits;      /* Total bits in packet */
    int      cur_bit;         /* Current bit index */
    int      bit_samples;     /* Samples per bit (rate/1200) */
    int      cur_sample;      /* Sample within current bit */
    int      complete;
};

/* ── MDC-1200 Decoder context ── */
struct plcode_mdc1200_dec {
    int      rate;
    int      block_size;      /* Samples per bit-period Goertzel block */
    int      sample_count;

    /* Goertzel for mark and space */
    int64_t  mark_s1, mark_s2;
    int32_t  mark_coeff;
    int64_t  space_s1, space_s2;
    int32_t  space_coeff;

    /* Bit stream */
    uint32_t shift_reg;       /* 16-bit shift register for sync detection */
    int      synced;
    uint8_t  packet[8];       /* Received packet bytes */
    int      packet_bits;     /* Bits received after sync */

    /* Result */
    int      detected;
    uint8_t  op;
    uint8_t  arg;
    uint16_t unit_id;
};

/* ── Courtesy Tone Encoder context ── */
struct plcode_courtesy_enc {
    uint32_t phase;
    int      rate;
    int      num_tones;
    int      cur_tone;        /* Current tone index */
    int      cur_sample;      /* Sample within current tone */

    /* Per-tone parameters (copied from input) */
    uint32_t *phase_incs;     /* Phase increment per tone (0 for silence) */
    int      *tone_samples;   /* Duration in samples per tone */
    int16_t  *amplitudes;     /* Amplitude per tone */
    int       complete;
};

/* ── Tone Generator context ── */
struct plcode_tone_enc {
    uint32_t phase;
    uint32_t phase_inc;
    int16_t  amplitude;
};

#endif /* PLCODE_INTERNAL_H */
