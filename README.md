# libplcode

Portable C library for encoding and decoding **CTCSS (PL) tones** and **DCS (DPL) codes** in signed 16-bit linear PCM audio. Used for sub-audible squelch signaling in two-way radio.

C99, no external dependencies beyond libc + libm, integer/fixed-point DSP.

## Features

- **CTCSS Encoder** — Generates any of 50 standard tones (67.0–254.1 Hz) using a 1024-entry sine LUT and 32-bit phase accumulator
- **CTCSS Decoder** — 50 parallel Goertzel filters with 1-second block size, 6 dB SNR threshold, and 2-window hysteresis
- **DCS Encoder** — NRZ FSK baseband at 134.4 bps from Golay (23,12) codewords, with IIR low-pass smoothing
- **DCS Decoder** — 2nd-order Butterworth LPF, comparator with hysteresis, zero-crossing PLL for bit clock recovery, Golay-validated codeword extraction
- **Golay (23,12) Codec** — Generator polynomial 0xC75, used for DCS codeword generation and validation

### Supported Configurations

| Parameter | Values |
|-----------|--------|
| Sample rates | 8000, 16000, 32000, 48000 Hz |
| CTCSS tones | 50 standard (67.0–254.1 Hz) |
| DCS codes | 104 standard (octal 023–754), normal + inverted |
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

The `_process()` functions are designed for streaming. Call them repeatedly with successive audio buffers of any size. Internal state is maintained across calls:

```c
plcode_ctcss_dec_t *dec;
plcode_ctcss_dec_create(&dec, 8000);
plcode_ctcss_result_t result = {0};

/* Feed audio in 160-sample frames (20 ms at 8 kHz) */
while (have_audio()) {
    int16_t frame[160];
    read_audio(frame, 160);
    plcode_ctcss_dec_process(dec, frame, 160, &result);

    if (result.detected) {
        printf("Detected: %.1f Hz\n", result.tone_freq_x10 / 10.0);
    }
}

/* Reset state on channel change */
plcode_ctcss_dec_reset(dec);

plcode_ctcss_dec_destroy(dec);
```

## Sample WAV Files

The `wav/` directory contains 258 pre-generated sample files (16-bit mono PCM, 8000 Hz, 3 seconds each):

| Files | Count | Description |
|-------|-------|-------------|
| `ctcss_XXX_X.wav` | 50 | CTCSS tones (e.g., `ctcss_100_0.wav` = 100.0 Hz) |
| `dcs_NNN.wav` | 104 | DCS codes, normal (e.g., `dcs_023.wav`) |
| `dcs_NNN_inv.wav` | 104 | DCS codes, inverted (e.g., `dcs_023_inv.wav`) |

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

=== ALL TESTS PASSED ===
```

Tests cover:
- Golay encode/check round-trip for all 104 DCS codes
- CTCSS encode/decode round-trip for every tone at every sample rate (200 combinations)
- DCS encode/decode round-trip for every code (normal + inverted) at 8 kHz
- DCS encode/decode at all 4 sample rates
- Noise rejection (white noise and silence produce no false detections)
- Adjacent tone rejection (67.0 Hz vs 69.3 Hz — minimum spacing of 2.3 Hz)

## Project Structure

```
libplcode/
  include/plcode.h          — Public API header
  src/plcode_internal.h     — Private shared definitions
  src/plcode_tables.c       — Sine LUT (1024 entries), tone/code tables
  src/plcode_golay.c        — Golay (23,12) codeword generation/validation
  src/plcode_ctcss_enc.c    — CTCSS tone encoder
  src/plcode_ctcss_dec.c    — CTCSS tone decoder (Goertzel)
  src/plcode_dcs_enc.c      — DCS code encoder
  src/plcode_dcs_dec.c      — DCS code decoder
  tests/test_main.c         — Test harness
  tests/test_golay.c        — Golay unit tests
  tests/test_ctcss.c        — CTCSS encode/decode round-trip
  tests/test_dcs.c          — DCS encode/decode round-trip
  tools/gen_wav.c           — WAV file generator
  tools/validate_wav.py     — Independent Python WAV validator
  wav/                      — 258 pre-generated sample WAV files
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

### Golay (23,12) Code
- Generator polynomial: g(x) = x^11 + x^10 + x^6 + x^5 + x^4 + x^2 + 1 (0xC75)
- Cyclic code: all rotations of valid codewords are also valid
- Complement property: the bitwise complement of any codeword is also valid
- Inverted DCS codes: XOR all 23 bits of the codeword

## License

MIT — see [LICENSE](LICENSE).
