# libplcode

Portable C library for encoding and decoding **CTCSS (PL) tones**, **DCS (DPL) codes**, **DTMF tones**, and **CW ID (Morse code)** in signed 16-bit linear PCM audio. Used for signaling in two-way radio.

C99, no external dependencies beyond libc + libm, integer/fixed-point DSP. All decoders can run simultaneously on the same audio stream.

## Features

- **CTCSS Encoder** — Generates any of 50 standard tones (67.0–254.1 Hz) using a 1024-entry sine LUT and 32-bit phase accumulator
- **CTCSS Decoder** — 50 parallel Goertzel filters with 1-second block size, 6 dB SNR threshold, and 2-window hysteresis
- **DCS Encoder** — NRZ FSK baseband at 134.4 bps from Golay (23,12) codewords, with IIR low-pass smoothing
- **DCS Decoder** — 2nd-order Butterworth LPF, comparator with hysteresis, zero-crossing PLL for bit clock recovery, Golay-validated codeword extraction
- **DTMF Encoder** — Dual phase accumulator generating row + column tones for all 16 DTMF symbols
- **DTMF Decoder** — 8 parallel Goertzel filters (4 row + 4 col), 20 ms blocks, 6 dB SNR per group, twist check, 2-block hysteresis
- **CW ID Encoder** — Message string to Morse-keyed tone with standard dot/dash/gap timing (5–40 WPM)
- **CW ID Decoder** — Single Goertzel filter, 10 ms blocks, timing-based dot/dash classification, Morse table lookup, message accumulation
- **Tone Generator** — Arbitrary-frequency single-tone generator (1–4000 Hz)
- **Golay (23,12) Codec** — Generator polynomial 0xC75, used for DCS codeword generation and validation

### Supported Configurations

| Parameter | Values |
|-----------|--------|
| Sample rates | 8000, 16000, 32000, 48000 Hz |
| CTCSS tones | 50 standard (67.0–254.1 Hz) |
| DCS codes | 104 standard (octal 023–754), normal + inverted |
| DTMF digits | 16 symbols (0–9, *, #, A–D) |
| CW ID chars | 37 (A–Z, 0–9, /) |
| Audio format | Signed 16-bit linear PCM |

## Building

```bash
make        # builds libplcode.a (static library)
make test   # builds and runs the test suite
make clean  # removes build artifacts
```

Compiles with `-Wall -Wextra -Wpedantic` and zero warnings. Requires only a C99 compiler and libm.

## API

All subsystems follow a create/process/destroy lifecycle with opaque context structs. Memory is allocated only in `create()`; `process()` does no allocation.

### CTCSS Encoder

```c
#include "plcode.h"

plcode_ctcss_enc_t *enc;
int rc = plcode_ctcss_enc_create(&enc,
    8000,   /* sample rate */
    1000,   /* frequency in tenths of Hz (100.0 Hz) */
    3000);  /* amplitude (0–32767) */

/* Mix tone into existing PCM buffer (additive with clamping) */
int16_t buf[8000];
memset(buf, 0, sizeof(buf));
plcode_ctcss_enc_process(enc, buf, 8000);

plcode_ctcss_enc_destroy(enc);
```

### CTCSS Decoder

```c
plcode_ctcss_dec_t *dec;
plcode_ctcss_dec_create(&dec, 8000);

plcode_ctcss_result_t result;
plcode_ctcss_dec_process(dec, buf, 8000, &result);

if (result.detected) {
    printf("Tone: %.1f Hz (index %d)\n",
           result.tone_freq_x10 / 10.0,
           result.tone_index);
}

plcode_ctcss_dec_destroy(dec);
```

The decoder requires approximately 2 seconds of continuous tone for confirmed detection (1-second Goertzel blocks with 2-window hysteresis).

### DCS Encoder

```c
plcode_dcs_enc_t *enc;
int rc = plcode_dcs_enc_create(&enc,
    8000,   /* sample rate */
    023,    /* DCS code (octal) */
    0,      /* 0 = normal, 1 = inverted */
    3000);  /* amplitude */

int16_t buf[24000]; /* 3 seconds */
memset(buf, 0, sizeof(buf));
plcode_dcs_enc_process(enc, buf, 24000);

plcode_dcs_enc_destroy(enc);
```

### DCS Decoder

```c
plcode_dcs_dec_t *dec;
plcode_dcs_dec_create(&dec, 8000);

plcode_dcs_result_t result;
plcode_dcs_dec_process(dec, buf, 24000, &result);

if (result.detected) {
    printf("DCS %03o%s\n",
           result.code_number,
           result.inverted ? " inverted" : "");
}

plcode_dcs_dec_destroy(dec);
```

The decoder requires approximately 0.5 seconds for confirmed detection (3 consecutive codeword matches at 134.4 bps).

### DTMF Encoder

```c
plcode_dtmf_enc_t *enc;
int rc = plcode_dtmf_enc_create(&enc,
    8000,   /* sample rate */
    '5',    /* digit: 0-9, *, #, A-D */
    3000);  /* amplitude per tone (combined peak = 2x) */

int16_t buf[1600]; /* 200 ms */
memset(buf, 0, sizeof(buf));
plcode_dtmf_enc_process(enc, buf, 1600);

plcode_dtmf_enc_destroy(enc);
```

### DTMF Decoder

```c
plcode_dtmf_dec_t *dec;
plcode_dtmf_dec_create(&dec, 8000);

plcode_dtmf_result_t result;
plcode_dtmf_dec_process(dec, buf, 1600, &result);

if (result.detected) {
    printf("DTMF '%c' (row %d Hz, col %d Hz)\n",
           result.digit, result.row_freq, result.col_freq);
}

plcode_dtmf_dec_destroy(dec);
```

Detection requires approximately 40 ms (two 20 ms Goertzel blocks with hysteresis).

### CW ID Encoder

```c
plcode_cwid_enc_t *enc;
int rc = plcode_cwid_enc_create(&enc,
    8000,     /* sample rate */
    "W1AW",   /* message: A-Z, 0-9, /, space */
    800,      /* tone frequency in Hz */
    20,       /* speed in WPM */
    3000);    /* amplitude */

int16_t buf[40000]; /* 5 seconds */
memset(buf, 0, sizeof(buf));
plcode_cwid_enc_process(enc, buf, 40000);

if (plcode_cwid_enc_complete(enc)) {
    printf("Message fully encoded\n");
}

plcode_cwid_enc_destroy(enc);
```

### CW ID Decoder

```c
plcode_cwid_dec_t *dec;
plcode_cwid_dec_create(&dec,
    8000,   /* sample rate */
    800,    /* expected tone frequency */
    20);    /* expected WPM */

plcode_cwid_result_t result;
plcode_cwid_dec_process(dec, buf, 40000, &result);

printf("Decoded: %s\n", plcode_cwid_dec_message(dec));

plcode_cwid_dec_destroy(dec);
```

The decoder accumulates characters into an internal buffer accessible via `plcode_cwid_dec_message()`.

### Tone Generator

```c
plcode_tone_enc_t *enc;
int rc = plcode_tone_enc_create(&enc,
    8000,   /* sample rate */
    1000,   /* frequency in Hz (1-4000) */
    3000);  /* amplitude */

int16_t buf[8000];
memset(buf, 0, sizeof(buf));
plcode_tone_enc_process(enc, buf, 8000);

plcode_tone_enc_destroy(enc);
```

### Table Lookups

```c
/* CTCSS: iterate all 50 tones */
for (int i = 0; i < PLCODE_CTCSS_NUM_TONES; i++) {
    uint16_t freq = plcode_ctcss_tone_freq_x10(i);
    printf("Tone %d: %.1f Hz\n", i, freq / 10.0);
}

/* CTCSS: find index by frequency */
int idx = plcode_ctcss_tone_index(1000); /* 100.0 Hz -> index 12 */

/* DCS: iterate all 104 codes */
for (int i = 0; i < PLCODE_DCS_NUM_CODES; i++) {
    uint16_t code = plcode_dcs_code_number(i);
    if (code == 0) continue;
    printf("Code %d: %03o\n", i, code);
}

/* DCS: find index by code */
int idx = plcode_dcs_code_index(023); /* -> index 0 */

/* DTMF: iterate all 16 digits */
for (int i = 0; i < PLCODE_DTMF_NUM_DIGITS; i++) {
    printf("Digit %d: '%c'\n", i, plcode_dtmf_digit_char(i));
}

/* DTMF: find index by digit */
int idx = plcode_dtmf_digit_index('5'); /* -> index 5 */

/* CW ID: Morse lookup */
const char *pat = plcode_cwid_morse('A');     /* -> ".-" */
char ch = plcode_cwid_decode(".-");           /* -> 'A' */
```

### Golay (23,12)

```c
/* Encode 12-bit data word to 23-bit codeword */
uint32_t cw = plcode_golay_encode(0x813);

/* Validate a codeword */
if (plcode_golay_check(cw)) {
    printf("Valid Golay codeword\n");
}
```

### Error Codes

| Code | Constant | Meaning |
|------|----------|---------|
| 0 | `PLCODE_OK` | Success |
| -1 | `PLCODE_ERR_PARAM` | Invalid parameter |
| -2 | `PLCODE_ERR_ALLOC` | Memory allocation failed |
| -3 | `PLCODE_ERR_RATE` | Unsupported sample rate |

### Streaming Usage

The `_process()` functions are designed for streaming. Call them repeatedly with successive audio buffers of any size. Internal state is maintained across calls. All decoders operate on `const int16_t *` buffers and can run simultaneously on the same audio stream:

```c
plcode_ctcss_dec_t *ctcss_dec;
plcode_dcs_dec_t   *dcs_dec;
plcode_dtmf_dec_t  *dtmf_dec;
plcode_cwid_dec_t  *cwid_dec;

plcode_ctcss_dec_create(&ctcss_dec, 8000);
plcode_dcs_dec_create(&dcs_dec, 8000);
plcode_dtmf_dec_create(&dtmf_dec, 8000);
plcode_cwid_dec_create(&cwid_dec, 8000, 800, 20);

plcode_ctcss_result_t ctcss_result = {0};
plcode_dcs_result_t   dcs_result   = {0};
plcode_dtmf_result_t  dtmf_result  = {0};
plcode_cwid_result_t  cwid_result  = {0};

/* Feed audio in 160-sample frames (20 ms at 8 kHz) */
while (have_audio()) {
    int16_t frame[160];
    read_audio(frame, 160);

    plcode_ctcss_dec_process(ctcss_dec, frame, 160, &ctcss_result);
    plcode_dcs_dec_process(dcs_dec, frame, 160, &dcs_result);
    plcode_dtmf_dec_process(dtmf_dec, frame, 160, &dtmf_result);
    plcode_cwid_dec_process(cwid_dec, frame, 160, &cwid_result);

    if (ctcss_result.detected)
        printf("CTCSS: %.1f Hz\n", ctcss_result.tone_freq_x10 / 10.0);
    if (dcs_result.detected)
        printf("DCS: %03o%s\n", dcs_result.code_number,
               dcs_result.inverted ? " inv" : "");
    if (dtmf_result.detected)
        printf("DTMF: '%c'\n", dtmf_result.digit);
    if (cwid_result.new_character)
        printf("CW: '%c' (msg so far: %s)\n",
               cwid_result.character, plcode_cwid_dec_message(cwid_dec));
}

plcode_ctcss_dec_destroy(ctcss_dec);
plcode_dcs_dec_destroy(dcs_dec);
plcode_dtmf_dec_destroy(dtmf_dec);
plcode_cwid_dec_destroy(cwid_dec);
```

## Sample WAV Files

The `wav/` directory contains pre-generated sample files (16-bit mono PCM, 8000 Hz):

| Files | Count | Description |
|-------|-------|-------------|
| `ctcss_XXX_X.wav` | 50 | CTCSS tones, 3 sec (e.g., `ctcss_100_0.wav` = 100.0 Hz) |
| `dcs_NNN.wav` | 104 | DCS codes, normal, 3 sec (e.g., `dcs_023.wav`) |
| `dcs_NNN_inv.wav` | 104 | DCS codes, inverted, 3 sec (e.g., `dcs_023_inv.wav`) |
| `dtmf_X.wav` | 16 | DTMF digits, 500 ms (e.g., `dtmf_5.wav`, `dtmf_star.wav`) |
| `cwid_W1AW.wav` | 1 | CW ID sample, 5 sec |

### Regenerating WAV Files

```bash
make                                            # build library
cc -std=c99 -O2 -Iinclude tools/gen_wav.c \
   libplcode.a -lm -o gen_wav
./gen_wav                                       # writes to wav/
```

### Validating WAV Files

An independent Python validator verifies every WAV file without using the C library:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install numpy scipy
python3 tools/validate_wav.py
```

The validator performs:
- **CTCSS**: FFT peak frequency must match filename within 0.5 Hz
- **DCS**: Demodulates NRZ bits, extracts 23-bit codeword, verifies Golay validity, confirms code number and inversion state match filename

## Test Suite

```
$ make test
=== libplcode test suite ===

Golay (23,12) tests:
  encode produces valid codeword                     PASS
  all DCS codewords mod g(x) == 0                    PASS
  inverted codewords are not valid (expected)        PASS
  valid codeword has zero remainder                  PASS
  corrupted codeword fails check                     PASS
  Golay: 5/5 passed

CTCSS tests:
  encode/decode 100.0 Hz @ 8000 Hz                   PASS
  all tones x all rates (200 combos)                 PASS
  no false detection on silence                      PASS
  no false detection on white noise                  PASS
  adjacent tone rejection (67.0 vs 69.3 Hz)          PASS
  CTCSS: 5/5 passed

DCS tests:
  encode/decode DCS 023 @ 8000 Hz                    PASS
  encode/decode DCS 023 inverted @ 8000 Hz           PASS
  all DCS codes @ 8 kHz (104 codes)                  PASS
  DCS 145 x all sample rates (4)                     PASS
  no false detection on silence                      PASS
  DCS: 5/5 passed

DTMF tests:
  encode/decode DTMF '5' @ 8000 Hz                   PASS
  all digits x all rates (64 combos)                 PASS
  no false detection on silence                      PASS
  no false detection on white noise                  PASS
  single-tone rejection (697 Hz only)                PASS
  DTMF: 5/5 passed

CW ID tests:
  encode/decode single char 'A' @ 8000 Hz            PASS
  encode/decode callsign 'W1AW' @ 8000 Hz            PASS
  all CW chars individually (37 chars)               PASS
  no false detection on silence                      PASS
  'CQ' x all sample rates (4)                        PASS
  CWID: 5/5 passed

=== ALL TESTS PASSED ===
```

Tests cover:
- Golay encode/check round-trip for all 104 DCS codes
- CTCSS encode/decode round-trip for every tone at every sample rate (200 combinations)
- DCS encode/decode round-trip for every code (normal + inverted) at 8 kHz
- DCS encode/decode at all 4 sample rates
- DTMF encode/decode round-trip for every digit at every sample rate (64 combinations)
- DTMF single-tone rejection (row-only tone does not trigger detection)
- CW ID encode/decode round-trip for all 37 Morse characters
- CW ID callsign round-trip ("W1AW") and multi-rate verification
- Noise rejection (white noise and silence produce no false detections across all decoders)
- Adjacent tone rejection (CTCSS 67.0 Hz vs 69.3 Hz — minimum spacing of 2.3 Hz)

## Project Structure

```
libplcode/
  include/plcode.h          — Public API header
  src/plcode_internal.h     — Private shared definitions
  src/plcode_tables.c       — Sine LUT (1024 entries), tone/code/Morse tables
  src/plcode_golay.c        — Golay (23,12) codeword generation/validation
  src/plcode_ctcss_enc.c    — CTCSS tone encoder
  src/plcode_ctcss_dec.c    — CTCSS tone decoder (Goertzel)
  src/plcode_dcs_enc.c      — DCS code encoder
  src/plcode_dcs_dec.c      — DCS code decoder
  src/plcode_dtmf_enc.c     — DTMF dual-tone encoder
  src/plcode_dtmf_dec.c     — DTMF dual-tone decoder (Goertzel)
  src/plcode_cwid_enc.c     — CW ID (Morse) encoder
  src/plcode_cwid_dec.c     — CW ID (Morse) decoder (Goertzel)
  src/plcode_tone_enc.c     — Arbitrary frequency tone generator
  tests/test_main.c         — Test harness
  tests/test_golay.c        — Golay unit tests
  tests/test_ctcss.c        — CTCSS encode/decode round-trip
  tests/test_dcs.c          — DCS encode/decode round-trip
  tests/test_dtmf.c         — DTMF encode/decode round-trip
  tests/test_cwid.c         — CW ID encode/decode round-trip
  tools/gen_wav.c           — WAV file generator
  tools/validate_wav.py     — Independent Python WAV validator
  wav/                      — Pre-generated sample WAV files
  Makefile                  — C99, static lib, -lm only
```

## Technical Details

### CTCSS Encoder
- 1024-entry Q15 sine lookup table
- 32-bit fixed-point phase accumulator (top 10 bits index table)
- Phase increment = `(tone_freq / sample_rate) * 2^32`
- Additive mixing with int16 clamping

### CTCSS Decoder
- Goertzel algorithm: O(N) per frequency bin, 50 bins per block
- Q28 coefficients for sufficient precision at all sample rates
- int64 accumulators to prevent overflow with large block sizes
- Detection: absolute threshold + 6 dB SNR above second-highest + 2-window hysteresis

### DCS Encoder
- 9-bit octal code to 12-bit data word (prepend `100`) to 23-bit Golay codeword
- NRZ baseband: +amplitude for bit 1, -amplitude for bit 0 at 134.4 bps
- Single-pole IIR LPF at 300 Hz for transition smoothing
- Fixed-point fractional accumulator for bit timing

### DCS Decoder
- 2nd-order Butterworth LPF at 300 Hz (Q14 coefficients)
- Comparator with hysteresis for hard binary decisions
- Zero-crossing PLL for bit clock recovery (~6% bandwidth)
- 23-bit shift register with Golay validation and data word extraction
- Alignment tracking: skips initial shift register fill, then checks every 23 bits
- 3 consecutive matches required for confirmed detection

### DTMF Encoder
- Dual 32-bit phase accumulators (row + column frequencies)
- Row frequencies: 697, 770, 852, 941 Hz
- Column frequencies: 1209, 1336, 1477, 1633 Hz
- Both tones at specified amplitude, additive mixing with clamping

### DTMF Decoder
- 8 parallel Goertzel filters (4 row + 4 col), 20 ms block size
- Full-precision magnitude computation with split cross-term to avoid int64 overflow
- Detection: per-group 6 dB SNR, twist check (10 dB max row/col ratio), 2-block hysteresis
- Detects all 16 symbols: 0–9, *, #, A–D

### CW ID Encoder
- Pre-built int8 element sequence: positive = tone (dot-units), negative = gap (dot-units)
- Standard Morse timing: dot = 1 unit, dash = 3, element gap = 1, char gap = 3, word gap = 7
- Dot duration = 1200 / WPM ms (PARIS standard)
- Phase accumulator tone generation via shared sine LUT
- Finite message: `complete()` reports when the full message has been sent

### CW ID Decoder
- Single Goertzel filter tuned to expected CW tone frequency
- 10 ms block size for timing resolution down to 40 WPM
- Dot/dash classification at 2x dot-length threshold
- Dual gap thresholds: character boundary at 2x, word boundary at 5x dot-length
- Pattern-to-character lookup against 37-entry Morse table
- 128-character internal message buffer with `message()` accessor

### Tone Generator
- Same phase accumulator + sine LUT approach as CTCSS encoder
- Accepts any frequency from 1 to 4000 Hz (not limited to CTCSS tone table)
- Useful for test signals, courtesy tones, alert tones

### Golay (23,12) Code
- Generator polynomial: g(x) = x^11 + x^10 + x^6 + x^5 + x^4 + x^2 + 1 (0xC75)
- Cyclic code: all rotations of valid codewords are also valid
- Complement property: the bitwise complement of any codeword is also valid
- Inverted DCS codes: XOR all 23 bits of the codeword

## License

MIT — see [LICENSE](LICENSE).
