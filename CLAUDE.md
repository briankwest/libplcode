# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
make            # Build libplcode.a static library
make test       # Build and run all tests (25 tests across 5 suites)
make clean      # Remove build artifacts
```

There is no single-test runner; all tests run together via `make test`. The test binary is `test_plcode`.

## Project Overview

libplcode is a C99 library for encoding/decoding CTCSS (PL) tones, DCS (DPL) codes, DTMF tones, and CW ID (Morse code) in signed 16-bit PCM audio — signaling for two-way radio. Zero external dependencies beyond libc/libm. All decoders can run simultaneously on the same audio stream.

## Architecture

**Create-Process-Destroy lifecycle**: All codecs follow `plcode_*_create()` → `plcode_*_process()` → `plcode_*_destroy()`. Memory is allocated only in `create()`; `process()` is allocation-free for streaming.

**Fixed-point DSP throughout** — no floating-point in audio paths. Uses Q-format coefficients (Q15/Q28/Q14) and a 1024-entry sine LUT.

### Codec pipeline:

- **CTCSS encode** (`plcode_ctcss_enc.c`): Phase accumulator → sine LUT → additive mix into PCM
- **CTCSS decode** (`plcode_ctcss_dec.c`): 50 parallel Goertzel filters, 1-second blocks, 6 dB SNR threshold, 2-window hysteresis
- **DCS encode** (`plcode_dcs_enc.c`): 9-bit code → 12-bit data → Golay(23,12) codeword → 134.4 bps NRZ FSK with IIR LPF smoothing
- **DCS decode** (`plcode_dcs_dec.c`): 2nd-order Butterworth LPF → comparator with hysteresis → PLL bit clock recovery → 23-bit shift register → Golay validation, 3 consecutive matches required
- **DTMF encode** (`plcode_dtmf_enc.c`): Dual phase accumulator → sine LUT → two-tone additive mix into PCM
- **DTMF decode** (`plcode_dtmf_dec.c`): 8 parallel Goertzel filters (4 row + 4 col), 20ms blocks, 6 dB SNR per group, twist check, 2-block hysteresis
- **CW ID encode** (`plcode_cwid_enc.c`): Message → Morse element sequence → phase accumulator tone generation with standard dot/dash/gap timing
- **CW ID decode** (`plcode_cwid_dec.c`): Single Goertzel filter, 10ms blocks, timing-based dot/dash classification, pattern-to-character Morse lookup, message accumulation

**Golay (23,12)** (`plcode_golay.c`): Generator polynomial 0xC75, used by DCS for error detection.

**Tables** (`plcode_tables.c`): Sine LUT, 50 CTCSS tone frequencies (67.0–254.1 Hz), 104 DCS octal codes (023–754), DTMF frequency/digit tables, 37-character Morse code table. Must be initialized via `plcode_init()`.

### Key files:

- `include/plcode.h` — public API (all structs, error codes, function signatures)
- `src/plcode_internal.h` — private structs, fixed-point helpers, LUT externs
- `tools/gen_wav.c` — WAV file generator for all tones/codes
- `tools/validate_wav.py` — independent Python validator (not using libplcode)

## Compiler Flags

`-std=c99 -Wall -Wextra -Wpedantic -O2` — the codebase compiles with zero warnings. Maintain this.

## Supported Configurations

- Sample rates: 8000, 16000, 32000, 48000 Hz
- Audio format: signed 16-bit linear PCM (int16_t)
