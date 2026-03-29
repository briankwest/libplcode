# libplcode

Portable C library for encoding and decoding **CTCSS (PL) tones**, **DCS (DPL) codes**, **DTMF tones**, **CW ID (Morse code)**, **MCW (Modulated CW)**, **FSK CW**, **Two-Tone Sequential Paging**, **Five-Tone Selective Call (ZVEI1/CCIR/EIA)**, **MDC-1200 signaling**, **access tone burst**, and **courtesy tones** in signed 16-bit linear PCM audio. Used for signaling in two-way radio.

C99, no external dependencies beyond libc + libm, integer/fixed-point DSP. All decoders can run simultaneously on the same audio stream.

## Table of Contents

- [Features](#features)
  - [Supported Configurations](#supported-configurations)
- [Building](#building)
  - [Prerequisites](#prerequisites)
  - [Autotools Build (recommended)](#autotools-build-recommended)
  - [Legacy Makefile Build](#legacy-makefile-build)
  - [Cross-Compilation](#cross-compilation)
  - [Debian Packaging](#debian-packaging)
- [API](#api)
  - [CTCSS Encoder](#ctcss-encoder)
  - [CTCSS Decoder](#ctcss-decoder)
  - [DCS Encoder](#dcs-encoder)
  - [DCS Decoder](#dcs-decoder)
  - [DTMF Encoder](#dtmf-encoder)
  - [DTMF Decoder](#dtmf-decoder)
  - [CW ID Encoder](#cw-id-encoder)
  - [CW ID Decoder](#cw-id-decoder)
  - [MCW Encoder](#mcw-encoder)
  - [MCW Decoder](#mcw-decoder)
  - [FSK CW Encoder](#fsk-cw-encoder)
  - [FSK CW Decoder](#fsk-cw-decoder)
  - [Two-Tone Paging Encoder](#two-tone-paging-encoder)
  - [Two-Tone Paging Decoder](#two-tone-paging-decoder)
  - [Selcall Encoder](#selcall-encoder)
  - [Selcall Decoder](#selcall-decoder)
  - [Tone Burst Encoder](#tone-burst-encoder)
  - [Tone Burst Decoder](#tone-burst-decoder)
  - [MDC-1200 Encoder](#mdc-1200-encoder)
  - [MDC-1200 Decoder](#mdc-1200-decoder)
  - [Courtesy Tone Generator](#courtesy-tone-generator)
  - [Tone Generator](#tone-generator)
  - [Table Lookups](#table-lookups)
  - [Golay (23,12)](#golay-2312)
  - [Error Codes](#error-codes)
  - [Streaming Usage](#streaming-usage)
- [Sample WAV Files](#sample-wav-files)
  - [Regenerating WAV Files](#regenerating-wav-files)
  - [Validating WAV Files](#validating-wav-files)
- [Test Suite](#test-suite)
- [Project Structure](#project-structure)
- [Technical Details](#technical-details)
  - [CTCSS Encoder](#ctcss-encoder-1)
  - [CTCSS Decoder](#ctcss-decoder-1)
  - [DCS Encoder](#dcs-encoder-1)
  - [DCS Decoder](#dcs-decoder-1)
  - [DTMF Encoder](#dtmf-encoder-1)
  - [DTMF Decoder](#dtmf-decoder-1)
  - [CW ID Encoder](#cw-id-encoder-1)
  - [CW ID Decoder](#cw-id-decoder-1)
  - [MCW Encoder](#mcw-encoder-1)
  - [MCW Decoder](#mcw-decoder-1)
  - [FSK CW Encoder](#fsk-cw-encoder-1)
  - [FSK CW Decoder](#fsk-cw-decoder-1)
  - [Two-Tone Sequential Paging](#two-tone-sequential-paging)
  - [Five-Tone Selective Call](#five-tone-selective-call)
  - [Access Tone Burst](#access-tone-burst)
  - [MDC-1200 Signaling](#mdc-1200-signaling)
  - [Courtesy Tone Generator](#courtesy-tone-generator-1)
  - [Tone Generator](#tone-generator-1)
  - [Golay (23,12) Code](#golay-2312-code)
- [License](#license)

## Features

- **CTCSS Encoder** — Generates any of 50 standard tones (67.0–254.1 Hz) using a 1024-entry sine LUT and 32-bit phase accumulator
- **CTCSS Decoder** — 50 parallel Goertzel filters with 1-second block size, 6 dB SNR threshold, and 2-window hysteresis
- **DCS Encoder** — NRZ FSK baseband at 134.4 bps from Golay (23,12) codewords, with IIR low-pass smoothing
- **DCS Decoder** — 2nd-order Butterworth LPF, comparator with hysteresis, zero-crossing PLL for bit clock recovery, Golay-validated codeword extraction
- **DTMF Encoder** — Dual phase accumulator generating row + column tones for all 16 DTMF symbols
- **DTMF Decoder** — 8 parallel Goertzel filters (4 row + 4 col), 20 ms blocks, 6 dB SNR per group, twist check, 2-block hysteresis
- **CW ID Encoder** — Message string to Morse-keyed tone with standard dot/dash/gap timing (5–40 WPM)
- **CW ID Decoder** — Single Goertzel filter, 10 ms blocks, timing-based dot/dash classification, Morse table lookup, message accumulation
- **MCW Encoder** — Modulated CW with raised-cosine shaped keying for reduced bandwidth (5–40 WPM, 300–3000 Hz)
- **MCW Decoder** — Goertzel-based detection with envelope tracking, compatible with both hard-keyed CW and MCW signals
- **FSK CW Encoder** — Morse via frequency-shift keying between mark and space frequencies (5–40 WPM)
- **FSK CW Decoder** — Dual Goertzel filter (mark/space), differential detection, timing-based Morse decoding
- **Two-Tone Paging Encoder** — Two-tone sequential paging with 33 standard frequencies and configurable tone durations
- **Two-Tone Paging Decoder** — Goertzel-based sequential tone detection with frequency table lookup
- **Selcall Encoder** — Five-tone selective calling supporting ZVEI1, CCIR, and EIA standards (12 tone digits)
- **Selcall Decoder** — Sequential five-tone detection with standard-specific frequency tables
- **Tone Burst Encoder** — Access tone burst generator (configurable frequency and duration, typical 1750 Hz)
- **Tone Burst Decoder** — Single-frequency Goertzel detection with minimum duration validation
- **MDC-1200 Encoder** — Motorola MDC-1200 1200-baud FSK signaling (PTT ID, emergency, acknowledge)
- **MDC-1200 Decoder** — FSK demodulation with packet framing, CRC validation, and op/arg/unit-ID extraction
- **Courtesy Tone Generator** — Multi-segment tone sequences with configurable frequency, duration, and amplitude per segment
- **Tone Generator** — Arbitrary-frequency single-tone generator (1–4000 Hz)
- **Golay (23,12) Codec** — Generator polynomial 0xC75, used for DCS codeword generation and validation

### Supported Configurations

| Parameter | Values |
|-----------|--------|
| Sample rates | 8000, 16000, 32000, 48000 Hz |
| CTCSS tones | 50 standard (67.0–254.1 Hz) |
| DCS codes | 106 standard (octal 023–754), normal + inverted |
| DTMF digits | 16 symbols (0–9, *, #, A–D) |
| CW ID / MCW / FSK CW chars | 37 (A–Z, 0–9, /) |
| Two-Tone paging frequencies | 33 standard tones |
| Selcall standards | ZVEI1, CCIR, EIA (12 tone digits each) |
| MDC-1200 operations | PTT pre/post, emergency, acknowledge |
| Audio format | Signed 16-bit linear PCM |

## Building

### Prerequisites

A C99 compiler and libm are the only runtime dependencies. The build system uses GNU Autotools:

- **autoconf** (>= 2.69)
- **automake** (>= 1.14)
- **libtool** (>= 2.4)

On Debian/Ubuntu:

```bash
sudo apt install build-essential autoconf automake libtool
```

On macOS (Homebrew):

```bash
brew install autoconf automake libtool
```

### Autotools Build (recommended)

```bash
# 1. Bootstrap the build system (generates configure script)
autoreconf -fi

# 2. Configure (detects compiler, sets install prefix)
./configure

# 3. Build the shared and static libraries
make

# 4. Run the test suite (70 tests across 12 suites)
make check

# 5. Install (library, header, and pkg-config file)
sudo make install

# 6. Verify installation
pkg-config --cflags --libs plcode
# Expected output: -I/usr/local/include  -L/usr/local/lib -lplcode -lm
```

Common configure options:

```bash
./configure --prefix=/opt/libplcode     # Custom install prefix
./configure --enable-static --disable-shared   # Static library only
./configure --disable-static --enable-shared   # Shared library only
```

Use `make V=1` for verbose build output (silent rules are enabled by default).

### Legacy Makefile Build

The original plain Makefile is preserved as `Makefile.old` for environments without Autotools:

```bash
make -f Makefile.old          # builds libplcode.a (static library)
make -f Makefile.old test     # builds and runs the test suite
make -f Makefile.old clean    # removes build artifacts
```

Compiles with `-Wall -Wextra -Wpedantic` and zero warnings. Requires only a C99 compiler and libm.

### Cross-Compilation

The Autotools build supports standard cross-compilation via the `--host` flag:

```bash
autoreconf -fi
./configure --host=aarch64-linux-gnu
make
```

This requires a cross-compiler toolchain (e.g., `gcc-aarch64-linux-gnu`) installed on the build host. Common targets:

| Target | `--host` value |
|--------|----------------|
| ARM64 (aarch64) | `aarch64-linux-gnu` |
| ARMv7 hard-float | `arm-linux-gnueabihf` |
| RISC-V 64 | `riscv64-linux-gnu` |

### Debian Packaging

The `debian/` directory contains full Debian packaging support. To build a `.deb` package:

```bash
# Build the package (unsigned, binary-only)
dpkg-buildpackage -us -uc -b

# Install the resulting -dev package
sudo dpkg -i ../libplcode-dev_*.deb

# Verify
pkg-config --cflags --libs plcode
```

The package installs the static library, shared library, public header (`plcode.h`), and pkg-config file. Build dependencies are declared in `debian/control` and include `autoconf`, `automake`, and `libtool`.

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

### MCW Encoder

```c
plcode_mcw_enc_t *enc;
int rc = plcode_mcw_enc_create(&enc,
    8000,     /* sample rate */
    "W1AW",   /* message: A-Z, 0-9, /, space */
    800,      /* tone frequency in Hz (300-3000) */
    20,       /* speed in WPM (5-40) */
    3000);    /* amplitude */

int16_t buf[40000]; /* 5 seconds */
memset(buf, 0, sizeof(buf));
plcode_mcw_enc_process(enc, buf, 40000);

if (plcode_mcw_enc_complete(enc)) {
    printf("MCW message fully encoded\n");
}

plcode_mcw_enc_destroy(enc);
```

### MCW Decoder

```c
plcode_mcw_dec_t *dec;
plcode_mcw_dec_create(&dec,
    8000,   /* sample rate */
    800,    /* expected tone frequency (300-3000) */
    20);    /* expected WPM (5-40) */

plcode_cwid_result_t result;
plcode_mcw_dec_process(dec, buf, 40000, &result);

printf("Decoded: %s\n", plcode_mcw_dec_message(dec));

plcode_mcw_dec_destroy(dec);
```

The MCW decoder uses the same `plcode_cwid_result_t` result structure as the CW ID decoder, and MCW-encoded signals can also be decoded by the standard CW ID decoder.

### FSK CW Encoder

```c
plcode_fskcw_enc_t *enc;
int rc = plcode_fskcw_enc_create(&enc,
    8000,     /* sample rate */
    "W1AW",   /* message: A-Z, 0-9, /, space */
    1200,     /* mark frequency in Hz (300-3000) */
    1800,     /* space frequency in Hz (300-3000, must differ from mark) */
    20,       /* speed in WPM (5-40) */
    3000);    /* amplitude */

int16_t buf[40000]; /* 5 seconds */
memset(buf, 0, sizeof(buf));
plcode_fskcw_enc_process(enc, buf, 40000);

if (plcode_fskcw_enc_complete(enc)) {
    printf("FSK CW message fully encoded\n");
}

plcode_fskcw_enc_destroy(enc);
```

### FSK CW Decoder

```c
plcode_fskcw_dec_t *dec;
plcode_fskcw_dec_create(&dec,
    8000,     /* sample rate */
    1200,     /* mark frequency in Hz (300-3000) */
    1800,     /* space frequency in Hz (300-3000) */
    20);      /* expected WPM (5-40) */

plcode_cwid_result_t result;
plcode_fskcw_dec_process(dec, buf, 40000, &result);

printf("Decoded: %s\n", plcode_fskcw_dec_message(dec));

plcode_fskcw_dec_destroy(dec);
```

### Two-Tone Paging Encoder

```c
plcode_twotone_enc_t *enc;
int rc = plcode_twotone_enc_create(&enc,
    8000,      /* sample rate */
    3498,      /* tone A frequency in tenths of Hz (349.8 Hz) */
    5536,      /* tone B frequency in tenths of Hz (553.6 Hz) */
    1000,      /* tone A duration in ms (typical: 1000) */
    3000,      /* tone B duration in ms (typical: 3000) */
    3000);     /* amplitude */

int16_t buf[32000]; /* 4 seconds */
memset(buf, 0, sizeof(buf));
plcode_twotone_enc_process(enc, buf, 32000);

if (plcode_twotone_enc_complete(enc)) {
    printf("Two-tone page complete\n");
}

plcode_twotone_enc_destroy(enc);
```

### Two-Tone Paging Decoder

```c
plcode_twotone_dec_t *dec;
plcode_twotone_dec_create(&dec, 8000);

plcode_twotone_result_t result;
plcode_twotone_dec_process(dec, buf, 32000, &result);

if (result.detected) {
    printf("Two-tone page: %.1f Hz -> %.1f Hz\n",
           result.tone_a_freq_x10 / 10.0,
           result.tone_b_freq_x10 / 10.0);
}

plcode_twotone_dec_destroy(dec);
```

### Selcall Encoder

```c
plcode_selcall_enc_t *enc;
int rc = plcode_selcall_enc_create(&enc,
    8000,                    /* sample rate */
    PLCODE_SELCALL_ZVEI1,    /* standard: ZVEI1, CCIR, or EIA */
    "12345",                 /* 5-digit address ('0'-'9') */
    3000);                   /* amplitude */

int16_t buf[8000]; /* 1 second */
memset(buf, 0, sizeof(buf));
plcode_selcall_enc_process(enc, buf, 8000);

if (plcode_selcall_enc_complete(enc)) {
    printf("Selcall sequence complete\n");
}

plcode_selcall_enc_destroy(enc);
```

### Selcall Decoder

```c
plcode_selcall_dec_t *dec;
plcode_selcall_dec_create(&dec,
    8000,                    /* sample rate */
    PLCODE_SELCALL_ZVEI1);   /* standard to decode */

plcode_selcall_result_t result;
plcode_selcall_dec_process(dec, buf, 8000, &result);

if (result.detected) {
    printf("Selcall address: %s\n", result.address);
}

plcode_selcall_dec_destroy(dec);
```

### Tone Burst Encoder

```c
plcode_toneburst_enc_t *enc;
int rc = plcode_toneburst_enc_create(&enc,
    8000,     /* sample rate */
    1750,     /* frequency in Hz (typical: 1750, range: 300-3000) */
    500,      /* duration in ms (typical: 500-1000) */
    3000);    /* amplitude */

int16_t buf[8000]; /* 1 second */
memset(buf, 0, sizeof(buf));
plcode_toneburst_enc_process(enc, buf, 8000);

if (plcode_toneburst_enc_complete(enc)) {
    printf("Tone burst complete\n");
}

plcode_toneburst_enc_destroy(enc);
```

### Tone Burst Decoder

```c
plcode_toneburst_dec_t *dec;
plcode_toneburst_dec_create(&dec,
    8000,     /* sample rate */
    1750,     /* expected frequency in Hz (300-3000) */
    200);     /* minimum burst duration in ms (typical: 200-500) */

plcode_toneburst_result_t result;
plcode_toneburst_dec_process(dec, buf, 8000, &result);

if (result.detected) {
    printf("1750 Hz tone burst detected\n");
}

plcode_toneburst_dec_destroy(dec);
```

### MDC-1200 Encoder

```c
plcode_mdc1200_enc_t *enc;
int rc = plcode_mdc1200_enc_create(&enc,
    8000,                         /* sample rate */
    PLCODE_MDC1200_OP_PTT_PRE,    /* operation code */
    0x00,                         /* argument byte */
    0x1234,                       /* unit ID (0x0001-0xFFFF) */
    3000);                        /* amplitude */

int16_t buf[8000]; /* 1 second */
memset(buf, 0, sizeof(buf));
plcode_mdc1200_enc_process(enc, buf, 8000);

if (plcode_mdc1200_enc_complete(enc)) {
    printf("MDC-1200 packet sent\n");
}

plcode_mdc1200_enc_destroy(enc);
```

### MDC-1200 Decoder

```c
plcode_mdc1200_dec_t *dec;
plcode_mdc1200_dec_create(&dec, 8000);

plcode_mdc1200_result_t result;
plcode_mdc1200_dec_process(dec, buf, 8000, &result);

if (result.detected) {
    printf("MDC-1200: op=0x%02X arg=0x%02X unit=0x%04X\n",
           result.op, result.arg, result.unit_id);
}

plcode_mdc1200_dec_destroy(dec);
```

### Courtesy Tone Generator

```c
plcode_courtesy_tone_t tones[] = {
    { 880,  100, 3000 },   /* 880 Hz for 100 ms */
    {   0,   50,    0 },   /* 50 ms silence gap */
    { 880,  100, 3000 },   /* 880 Hz for 100 ms */
};

plcode_courtesy_enc_t *enc;
int rc = plcode_courtesy_enc_create(&enc,
    8000,               /* sample rate */
    tones,              /* array of tone descriptors */
    3);                 /* number of segments */

int16_t buf[4000]; /* 500 ms */
memset(buf, 0, sizeof(buf));
plcode_courtesy_enc_process(enc, buf, 4000);

if (plcode_courtesy_enc_complete(enc)) {
    printf("Courtesy tone complete\n");
}

plcode_courtesy_enc_destroy(enc);
```

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

/* DCS: iterate all 106 codes */
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

/* Two-Tone: iterate all 33 paging frequencies */
for (int i = 0; i < PLCODE_TWOTONE_NUM_FREQS; i++) {
    uint16_t freq = plcode_twotone_freq_x10(i);
    printf("Paging freq %d: %.1f Hz\n", i, freq / 10.0);
}

/* Selcall: get tone frequency for a standard and digit */
uint16_t freq = plcode_selcall_freq(PLCODE_SELCALL_ZVEI1, 5);
printf("ZVEI1 digit 5: %d Hz\n", freq);
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
$ make check
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
  fast detect 100.0 Hz (<= 350ms @ 8kHz)             PASS
  fast detect 131.8 Hz all rates (4 rates)           PASS
  CTCSS reverse burst stops encoder                  PASS
  CTCSS resume after reverse burst                   PASS
  CTCSS: 9/9 passed

DCS tests:
  encode/decode DCS 023 @ 8000 Hz                    PASS
  encode/decode DCS 023 inverted @ 8000 Hz           PASS
  all DCS codes @ 8 kHz (104 codes)                  PASS
  DCS 145 x all sample rates (4)                     PASS
  no false detection on silence                      PASS
  DCS turn-off stops encoder                         PASS
  DCS resume after turn-off                          PASS
  DCS: 7/7 passed

DTMF tests:
  encode/decode DTMF '5' @ 8000 Hz                   PASS
  all digits x all rates (64 combos)                 PASS
  no false detection on silence                      PASS
  no false detection on white noise                  PASS
  single-tone rejection (697 Hz only)                PASS
  dropout resilience (20ms gap mid-digit)            PASS
  same-digit short gap → single detection          PASS
  different digit immediate transition               PASS
  genuine gap → two detections of same digit       PASS
  create_ex custom opts (misses_to_end=1)            PASS
  asymmetric twist accept (col 3.5x row)             PASS
  asymmetric twist reject (col 5x row)               PASS
  harmonic rejection (strong 2nd harmonic)           PASS
  create_ex(NULL) matches create() defaults          PASS
  DTMF: 14/14 passed

CW ID tests:
  encode/decode single char 'A' @ 8000 Hz            PASS
  encode/decode callsign 'W1AW' @ 8000 Hz            PASS
  all CW chars individually (37 chars)               PASS
  no false detection on silence                      PASS
  'CQ' x all sample rates (4)                        PASS
  CWID: 5/5 passed

MCW tests:
  MCW encode/decode single char 'A' @ 8000 Hz        PASS
  MCW encode/decode callsign 'W1AW' @ 8000 Hz        PASS
  MCW all chars individually (37 chars)              PASS
  MCW no false detection on silence                  PASS
  MCW 'CQ' x all sample rates (4)                    PASS
  MCW encoder -> CW decoder cross-decode             PASS
  MCW: 6/6 passed

FSK CW tests:
  FSK CW encode/decode single char 'A' @ 8000 Hz     PASS
  FSK CW encode/decode callsign 'W1AW' @ 8000 Hz     PASS
  FSK CW all chars individually (37 chars)           PASS
  FSK CW no false detection on silence               PASS
  FSK CW 'CQ' x all sample rates (4)                 PASS
  FSK CW: 5/5 passed

Two-Tone Paging tests:
  two-tone encode/decode @ 8000 Hz                   PASS
  two-tone no false detection on silence             PASS
  two-tone x all sample rates (4)                    PASS
  Two-Tone: 3/3 passed

Selcall tests:
  ZVEI1 selcall '12345' @ 8000 Hz                    PASS
  CCIR selcall '67890' @ 8000 Hz                     PASS
  EIA selcall '54321' @ 16000 Hz                     PASS
  selcall no false detection on silence              PASS
  ZVEI1 x all sample rates (4)                       PASS
  Selcall: 5/5 passed

Tone Burst tests:
  1750 Hz burst encode/decode @ 8000 Hz              PASS
  tone burst no false detection on silence           PASS
  tone burst rejects short burst (50ms < 200ms min)  PASS
  tone burst x all sample rates (4)                  PASS
  Tone Burst: 4/4 passed

MDC-1200 tests:
  MDC-1200 PTT ID round-trip @ 8000 Hz               PASS
  MDC-1200 emergency round-trip @ 8000 Hz            PASS
  MDC-1200 no false detection on silence             PASS
  MDC-1200 x all sample rates (4)                    PASS
  MDC-1200: 4/4 passed

Courtesy Tone tests:
  courtesy tone basic generation                     PASS
  courtesy tone silence gap is silent                PASS
  courtesy tone complete flag                        PASS
  Courtesy: 3/3 passed

=== ALL TESTS PASSED ===
```

Tests cover:
- Golay encode/check round-trip for all DCS codes
- CTCSS encode/decode round-trip for every tone at every sample rate (200 combinations)
- CTCSS fast detection (confirmed within 350 ms)
- CTCSS reverse burst (encoder stop/resume)
- DCS encode/decode round-trip for every code (normal + inverted) at 8 kHz
- DCS encode/decode at all 4 sample rates
- DCS turn-off (encoder stop/resume)
- DTMF encode/decode round-trip for every digit at every sample rate (64 combinations)
- DTMF single-tone rejection, dropout resilience, twist checking, harmonic rejection
- CW ID encode/decode round-trip for all 37 Morse characters
- CW ID callsign round-trip ("W1AW") and multi-rate verification
- MCW encode/decode round-trip with cross-compatibility to CW ID decoder
- FSK CW encode/decode round-trip for all characters and sample rates
- Two-tone sequential paging encode/decode at all sample rates
- Five-tone selcall (ZVEI1/CCIR/EIA) encode/decode round-trip
- Tone burst detection with minimum duration validation and short-burst rejection
- MDC-1200 PTT ID and emergency packet round-trip at all sample rates
- Courtesy tone multi-segment generation with silence gaps
- Noise rejection (white noise and silence produce no false detections across all decoders)
- Adjacent tone rejection (CTCSS 67.0 Hz vs 69.3 Hz — minimum spacing of 2.3 Hz)

## Project Structure

```
libplcode/
  configure.ac              — Autoconf configuration
  Makefile.am               — Automake build rules
  Makefile.old              — Legacy plain Makefile (no Autotools)
  plcode.pc.in              — pkg-config template
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
  src/plcode_mcw_enc.c      — MCW (Modulated CW) encoder
  src/plcode_mcw_dec.c      — MCW decoder
  src/plcode_fskcw_enc.c    — FSK CW encoder
  src/plcode_fskcw_dec.c    — FSK CW decoder
  src/plcode_twotone_enc.c  — Two-tone sequential paging encoder
  src/plcode_twotone_dec.c  — Two-tone sequential paging decoder
  src/plcode_selcall_enc.c  — Five-tone selective call encoder
  src/plcode_selcall_dec.c  — Five-tone selective call decoder
  src/plcode_toneburst_enc.c — Access tone burst encoder
  src/plcode_toneburst_dec.c — Access tone burst decoder
  src/plcode_mdc1200_enc.c  — MDC-1200 signaling encoder
  src/plcode_mdc1200_dec.c  — MDC-1200 signaling decoder
  src/plcode_courtesy_enc.c — Courtesy tone generator
  src/plcode_tone_enc.c     — Arbitrary frequency tone generator
  tests/test_main.c         — Test harness
  tests/test_golay.c        — Golay unit tests
  tests/test_ctcss.c        — CTCSS encode/decode round-trip
  tests/test_dcs.c          — DCS encode/decode round-trip
  tests/test_dtmf.c         — DTMF encode/decode round-trip
  tests/test_cwid.c         — CW ID encode/decode round-trip
  tests/test_mcw.c          — MCW encode/decode round-trip
  tests/test_fskcw.c        — FSK CW encode/decode round-trip
  tests/test_twotone.c      — Two-tone paging round-trip
  tests/test_selcall.c      — Five-tone selcall round-trip
  tests/test_toneburst.c    — Tone burst round-trip
  tests/test_mdc1200.c      — MDC-1200 round-trip
  tests/test_courtesy.c     — Courtesy tone generation
  tools/gen_wav.c           — WAV file generator
  tools/validate_wav.py     — Independent Python WAV validator
  debian/                   — Debian packaging files
  wav/                      — Pre-generated sample WAV files
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

### MCW Encoder
- Raised-cosine envelope shaping on key-down/key-up transitions for reduced bandwidth
- Same Morse timing and phase accumulator approach as CW ID encoder
- Smoother spectral profile compared to hard-keyed CW
- Compatible with standard CW ID decoders (cross-decode verified in tests)

### MCW Decoder
- Goertzel-based tone detection with envelope amplitude tracking
- Same timing-based dot/dash classification as CW ID decoder
- Handles both raised-cosine shaped (MCW) and hard-keyed (CW) signals
- 128-character internal message buffer with `message()` accessor

### FSK CW Encoder
- Frequency-shift keying between mark (dits/dahs) and space (gaps) frequencies
- Continuous-phase switching between mark and space tones
- Standard Morse timing identical to CW ID encoder
- Both frequencies must be in 300–3000 Hz range and must differ

### FSK CW Decoder
- Dual Goertzel filter (mark frequency + space frequency)
- Differential power comparison for key state detection
- Timing-based Morse decoding identical to CW ID decoder
- 128-character internal message buffer with `message()` accessor

### Two-Tone Sequential Paging
- 33 standard paging frequencies with lookup table
- Encoder: sequential tone A then tone B with configurable durations (typical 1000 ms + 3000 ms)
- Decoder: Goertzel-based detection of two sequential tones with frequency identification

### Five-Tone Selective Call
- Three standard tone plans: ZVEI1, CCIR, EIA (12 tones each: digits 0–9, R for repeat, G for group)
- 5-digit address encoding/decoding
- Encoder: sequential tone generation at standard-specific frequencies
- Decoder: per-digit Goertzel detection with address accumulation

### Access Tone Burst
- Configurable single-tone burst (typical: 1750 Hz, 500–1000 ms)
- Encoder: fixed-duration tone generation with `complete()` flag
- Decoder: Goertzel detection with minimum duration threshold to reject transients

### MDC-1200 Signaling
- Motorola MDC-1200 protocol: 1200-baud FSK (1200 Hz / 1800 Hz)
- Packet structure: preamble, sync word, data (op, arg, unit ID), CRC
- Encoder: generates complete packet with CRC
- Decoder: FSK demodulation, bit clock recovery, sync detection, CRC validation
- Standard operation codes: PTT pre-key (0x01), PTT post-key (0x00), emergency (0x80), acknowledge (0x20)

### Courtesy Tone Generator
- Multi-segment tone sequences defined by an array of `{freq, duration_ms, amplitude}` descriptors
- Frequency of 0 produces a silence gap
- Phase accumulator tone generation via shared sine LUT
- `complete()` flag indicates when all segments have been played

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
