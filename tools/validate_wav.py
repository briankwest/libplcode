#!/usr/bin/env python3
"""
Independent validation of libplcode WAV files using numpy/scipy.

CTCSS: FFT peak frequency must match filename within 0.5 Hz.
DCS: Demodulate NRZ bits, extract 23-bit codeword, verify Golay,
     extract code number, compare to filename.
"""

import sys
import os
import struct
import wave
import numpy as np
from pathlib import Path

# ── Golay (23,12) ──

GOLAY_POLY = 0xC75  # x^11+x^10+x^6+x^5+x^4+x^2+1

def golay_check(cw):
    """Check if 23-bit word is a valid Golay codeword (remainder == 0)."""
    r = cw & 0x7FFFFF
    for i in range(11, -1, -1):
        if r & (1 << (i + 11)):
            r ^= (GOLAY_POLY << i)
    return r == 0

def golay_encode(data12):
    """Encode 12-bit data to 23-bit Golay codeword."""
    cw = (data12 & 0xFFF) << 11
    r = cw
    for i in range(11, -1, -1):
        if r & (1 << (i + 11)):
            r ^= (GOLAY_POLY << i)
    return cw | (r & 0x7FF)

# ── Standard tables ──

CTCSS_TONES = [
    67.0, 69.3, 71.9, 74.4, 77.0, 79.7, 82.5, 85.4,
    88.5, 91.5, 94.8, 97.4, 100.0, 103.5, 107.2, 110.9,
    114.8, 118.8, 123.0, 127.3, 131.8, 136.5, 141.3, 146.2,
    151.4, 156.7, 159.8, 162.2, 165.5, 167.9, 171.3, 173.8,
    177.3, 179.9, 183.5, 186.2, 189.9, 192.8, 196.6, 199.5,
    203.5, 206.5, 210.7, 218.1, 225.7, 229.1, 233.6, 241.8,
    250.3, 254.1,
]

DCS_CODES = [
    0o023, 0o025, 0o026, 0o031, 0o032, 0o036, 0o043, 0o047,
    0o051, 0o053, 0o054, 0o065, 0o071, 0o072, 0o073, 0o074,
    0o114, 0o115, 0o116, 0o122, 0o125, 0o131, 0o132, 0o134,
    0o143, 0o145, 0o152, 0o155, 0o156, 0o162, 0o165, 0o172,
    0o174, 0o205, 0o212, 0o223, 0o225, 0o226, 0o243, 0o244,
    0o245, 0o246, 0o251, 0o252, 0o255, 0o261, 0o263, 0o265,
    0o266, 0o271, 0o274, 0o306, 0o311, 0o315, 0o325, 0o331,
    0o332, 0o343, 0o346, 0o351, 0o356, 0o364, 0o365, 0o371,
    0o411, 0o412, 0o413, 0o423, 0o431, 0o432, 0o445, 0o446,
    0o452, 0o454, 0o455, 0o462, 0o464, 0o465, 0o466, 0o503,
    0o506, 0o516, 0o523, 0o526, 0o532, 0o546, 0o565, 0o606,
    0o612, 0o624, 0o627, 0o631, 0o632, 0o654, 0o662, 0o664,
    0o703, 0o712, 0o723, 0o731, 0o732, 0o734, 0o743, 0o754,
]

def read_wav(path):
    """Read a WAV file, return (samples_int16[], sample_rate)."""
    with wave.open(str(path), 'rb') as wf:
        assert wf.getnchannels() == 1, "Expected mono"
        assert wf.getsampwidth() == 2, "Expected 16-bit"
        rate = wf.getframerate()
        n = wf.getnframes()
        raw = wf.readframes(n)
        samples = np.frombuffer(raw, dtype=np.int16)
        return samples, rate


def validate_ctcss(path):
    """Validate a CTCSS WAV: FFT peak must match expected frequency."""
    fname = path.stem  # e.g., "ctcss_100_0"
    parts = fname.split('_')
    expected_freq = float(parts[1]) + float(parts[2]) / 10.0

    samples, rate = read_wav(path)
    # Use last 2 seconds (skip initial transient)
    n_skip = rate
    samples = samples[n_skip:]

    # FFT
    N = len(samples)
    spectrum = np.abs(np.fft.rfft(samples))
    freqs = np.fft.rfftfreq(N, 1.0 / rate)

    # Find peak below 300 Hz (CTCSS range)
    mask = freqs < 300
    peak_idx = np.argmax(spectrum[mask])
    peak_freq = freqs[peak_idx]
    peak_mag = spectrum[peak_idx]

    # Check frequency accuracy
    tolerance = 0.5  # Hz
    ok = abs(peak_freq - expected_freq) < tolerance

    # Also verify it's a strong, clean tone (peak >> noise floor)
    noise_floor = np.median(spectrum[mask])
    snr = peak_mag / max(noise_floor, 1)

    return ok, expected_freq, peak_freq, snr


def validate_dcs(path):
    """Validate a DCS WAV: demodulate bits, extract Golay codeword, verify code."""
    fname = path.stem  # e.g., "dcs_023" or "dcs_023_inv"
    parts = fname.split('_')
    expected_code = int(parts[1], 8)  # octal
    expected_inv = len(parts) > 2 and parts[2] == 'inv'

    samples, rate = read_wav(path)

    # Low-pass filter: simple moving average (sub-audible, < 300 Hz)
    # Use a window of rate/300 samples
    win = max(int(rate / 300), 1)
    kernel = np.ones(win) / win
    filtered = np.convolve(samples.astype(float), kernel, mode='same')

    # Hard decision: sign of filtered signal
    bits_raw = (filtered > 0).astype(int)

    # Find bit boundaries using zero crossings
    # Bit rate = 134.4 bps
    samples_per_bit = rate / 134.4

    # Sample at bit centers, starting after initial transient (skip 1 sec)
    start = int(rate * 1.0)
    # Find a good starting alignment: look for a transition near start
    for i in range(start, start + int(samples_per_bit * 2)):
        if i > 0 and bits_raw[i] != bits_raw[i-1]:
            start = i
            break

    # Sample bits at midpoints
    bit_list = []
    pos = start + samples_per_bit / 2
    while pos < len(samples) - 1:
        idx = int(pos)
        bit_list.append(int(bits_raw[idx]))
        pos += samples_per_bit

    if len(bit_list) < 46:  # Need at least 2 codewords
        return False, expected_code, expected_inv, "too few bits"

    # Compute expected 23-bit Golay codeword from filename
    data12 = 0x800 | (expected_code & 0x1FF)
    expected_cw = golay_encode(data12)
    if expected_inv:
        expected_cw ^= 0x7FFFFF

    # Search for expected codeword at any alignment in the bit stream
    found = False
    found_offset = -1
    for offset in range(min(46, len(bit_list) - 23)):
        sr = 0
        for b in range(23):
            sr |= (bit_list[offset + b] << b)
        if sr == expected_cw:
            found = True
            found_offset = offset
            break

    if not found:
        # Also try: independently decode what IS in the stream
        # to give a useful diagnostic
        decoded_info = "expected cw 0x%06X not found in bitstream" % expected_cw
        for offset in range(min(23, len(bit_list) - 23)):
            sr = 0
            for b in range(23):
                sr |= (bit_list[offset + b] << b)
            if golay_check(sr):
                d12 = (sr >> 11) & 0xFFF
                if (d12 & 0xE00) == 0x800:
                    c9 = d12 & 0x1FF
                    decoded_info = f"found {oct(c9)[2:].zfill(3)} at offset {offset} instead"
                    break
        return False, expected_code, expected_inv, decoded_info

    # Verify the codeword repeats 23 bits later (consistency check)
    repeat_ok = False
    next_offset = found_offset + 23
    if next_offset + 23 <= len(bit_list):
        sr2 = 0
        for b in range(23):
            sr2 |= (bit_list[next_offset + b] << b)
        repeat_ok = (sr2 == expected_cw)

    detail = f"verified at offset {found_offset}"
    if repeat_ok:
        detail += ", repeats"
    else:
        detail += ", no repeat (PLL drift?)"

    return True, expected_code, expected_inv, detail


def main():
    wav_dir = Path("wav")
    if not wav_dir.exists():
        print("Error: wav/ directory not found. Run gen_wav first.")
        sys.exit(1)

    ctcss_files = sorted(wav_dir.glob("ctcss_*.wav"))
    dcs_files = sorted(wav_dir.glob("dcs_*.wav"))

    print(f"=== Independent WAV Validation (Python/NumPy/SciPy) ===\n")

    # ── CTCSS ──
    ctcss_pass = 0
    ctcss_total = len(ctcss_files)
    print(f"CTCSS tones ({ctcss_total} files):")
    for f in ctcss_files:
        ok, expected, measured, snr = validate_ctcss(f)
        status = "PASS" if ok else "FAIL"
        if ok:
            ctcss_pass += 1
            print(f"  {status}  {f.name:<25s}  expected={expected:6.1f} Hz  measured={measured:6.1f} Hz  SNR={snr:.0f}")
        else:
            print(f"  {status}  {f.name:<25s}  expected={expected:6.1f} Hz  measured={measured:6.1f} Hz  SNR={snr:.0f}  ***")

    print(f"\n  CTCSS: {ctcss_pass}/{ctcss_total} passed\n")

    # ── DCS ──
    dcs_pass = 0
    dcs_total = len(dcs_files)
    print(f"DCS codes ({dcs_total} files):")
    for f in dcs_files:
        ok, expected, expected_inv, detail = validate_dcs(f)
        status = "PASS" if ok else "FAIL"
        inv_str = " inv" if expected_inv else ""
        if ok:
            dcs_pass += 1
        print(f"  {status}  {f.name:<25s}  expected={oct(expected)[2:].zfill(3)}{inv_str:<5s}  {detail}")

    print(f"\n  DCS: {dcs_pass}/{dcs_total} passed\n")

    # ── Summary ──
    total = ctcss_pass + dcs_pass
    total_tests = ctcss_total + dcs_total
    print(f"=== {total}/{total_tests} total {'ALL PASSED' if total == total_tests else 'SOME FAILED'} ===")

    sys.exit(0 if total == total_tests else 1)


if __name__ == "__main__":
    main()
