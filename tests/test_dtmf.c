#include "../include/plcode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  %-50s ", name); \
} while(0)

#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

static const int sample_rates[] = { 8000, 16000, 32000, 48000 };
#define NUM_RATES 4

void test_dtmf_encode_decode_single(void)
{
    int rate = 8000;
    char digit = '5';
    int16_t amplitude = 3000;

    TEST("encode/decode DTMF '5' @ 8000 Hz");

    plcode_dtmf_enc_t *enc = NULL;
    plcode_dtmf_dec_t *dec = NULL;

    int rc = plcode_dtmf_enc_create(&enc, rate, digit, amplitude);
    if (rc != PLCODE_OK) { FAIL("enc create failed"); return; }

    rc = plcode_dtmf_dec_create(&dec, rate);
    if (rc != PLCODE_OK) { FAIL("dec create failed"); plcode_dtmf_enc_destroy(enc); return; }

    /* Generate 200ms of tone (need >= 40ms for 2-block hysteresis at 20ms blocks) */
    int total_samples = rate / 5; /* 200ms */
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc failed"); goto cleanup; }

    plcode_dtmf_enc_process(enc, buf, (size_t)total_samples);

    plcode_dtmf_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_dtmf_dec_process(dec, buf, (size_t)total_samples, &result);

    if (result.detected && result.digit == digit) {
        PASS();
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "detected=%d digit='%c' (expected '%c')",
                 result.detected, result.digit ? result.digit : '?', digit);
        FAIL(msg);
    }

cleanup:
    free(buf);
    plcode_dtmf_enc_destroy(enc);
    plcode_dtmf_dec_destroy(dec);
}

void test_dtmf_all_digits_all_rates(void)
{
    int r, d;
    int total = 0, passed_count = 0;
    char testname[80];

    for (r = 0; r < NUM_RATES; r++) {
        int rate = sample_rates[r];

        for (d = 0; d < PLCODE_DTMF_NUM_DIGITS; d++) {
            char digit = plcode_dtmf_digit_char(d);
            if (digit == '\0') continue;

            total++;

            plcode_dtmf_enc_t *enc = NULL;
            plcode_dtmf_dec_t *dec = NULL;

            if (plcode_dtmf_enc_create(&enc, rate, digit, 3000) != PLCODE_OK) continue;
            if (plcode_dtmf_dec_create(&dec, rate) != PLCODE_OK) {
                plcode_dtmf_enc_destroy(enc);
                continue;
            }

            int total_samples = rate / 5; /* 200ms */
            int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
            if (!buf) {
                plcode_dtmf_enc_destroy(enc);
                plcode_dtmf_dec_destroy(dec);
                continue;
            }

            plcode_dtmf_enc_process(enc, buf, (size_t)total_samples);

            plcode_dtmf_result_t result;
            memset(&result, 0, sizeof(result));
            plcode_dtmf_dec_process(dec, buf, (size_t)total_samples, &result);

            if (result.detected && result.digit == digit) {
                passed_count++;
            } else {
                printf("\n    FAIL: digit '%c' @ %d Hz (detected=%d, got='%c')",
                       digit, rate, result.detected,
                       result.digit ? result.digit : '?');
            }

            free(buf);
            plcode_dtmf_enc_destroy(enc);
            plcode_dtmf_dec_destroy(dec);
        }
    }

    snprintf(testname, sizeof(testname), "all digits x all rates (%d combos)", total);
    TEST(testname);
    if (passed_count == total) {
        PASS();
    } else {
        char msg[80];
        snprintf(msg, sizeof(msg), "%d/%d passed", passed_count, total);
        printf("\n");
        FAIL(msg);
    }
}

void test_dtmf_silence_rejection(void)
{
    int rate = 8000;
    TEST("no false detection on silence");

    plcode_dtmf_dec_t *dec = NULL;
    if (plcode_dtmf_dec_create(&dec, rate) != PLCODE_OK) { FAIL("create failed"); return; }

    int total_samples = rate; /* 1 second */
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_dtmf_dec_destroy(dec); return; }

    plcode_dtmf_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_dtmf_dec_process(dec, buf, (size_t)total_samples, &result);

    if (!result.detected) {
        PASS();
    } else {
        FAIL("false detection on silence");
    }

    free(buf);
    plcode_dtmf_dec_destroy(dec);
}

void test_dtmf_noise_rejection(void)
{
    int rate = 8000;
    TEST("no false detection on white noise");

    plcode_dtmf_dec_t *dec = NULL;
    if (plcode_dtmf_dec_create(&dec, rate) != PLCODE_OK) { FAIL("create failed"); return; }

    int total_samples = rate; /* 1 second */
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_dtmf_dec_destroy(dec); return; }

    unsigned int seed = 54321;
    int i;
    for (i = 0; i < total_samples; i++) {
        seed = seed * 1103515245 + 12345;
        buf[i] = (int16_t)((seed >> 16) & 0x7FFF) - 16384;
    }

    plcode_dtmf_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_dtmf_dec_process(dec, buf, (size_t)total_samples, &result);

    if (!result.detected) {
        PASS();
    } else {
        FAIL("false detection on noise");
    }

    free(buf);
    plcode_dtmf_dec_destroy(dec);
}

void test_dtmf_single_tone_rejection(void)
{
    int rate = 8000;
    /* A single 697 Hz tone (row only, no column) should not trigger detection */
    TEST("single-tone rejection (697 Hz only)");

    plcode_dtmf_dec_t *dec = NULL;
    if (plcode_dtmf_dec_create(&dec, rate) != PLCODE_OK) { FAIL("create failed"); return; }

    int total_samples = rate / 5; /* 200ms */
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_dtmf_dec_destroy(dec); return; }

    /* Generate a single 697 Hz tone */
    int i;
    for (i = 0; i < total_samples; i++) {
        double angle = 2.0 * 3.14159265358979323846 * 697.0 * i / rate;
        buf[i] = (int16_t)(3000.0 * sin(angle));
    }

    plcode_dtmf_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_dtmf_dec_process(dec, buf, (size_t)total_samples, &result);

    if (!result.detected) {
        PASS();
    } else {
        FAIL("detected digit from single tone");
    }

    free(buf);
    plcode_dtmf_dec_destroy(dec);
}

/* Helper: generate DTMF tone for digit into buffer at given offset.
 * Generates row+col sinusoids additively. */
static void gen_dtmf_tone(int16_t *buf, int offset, int samples,
                           int rate, char digit, int16_t amplitude)
{
    int idx = plcode_dtmf_digit_index(digit);
    if (idx < 0) return;

    /* Row and col frequencies — need to look up by index.
     * Row = idx/4 maps to 697,770,852,941; Col = idx%4 maps to 1209,1336,1477,1633 */
    static const double row_f[] = { 697, 770, 852, 941 };
    static const double col_f[] = { 1209, 1336, 1477, 1633 };
    double rf = row_f[idx / 4];
    double cf = col_f[idx % 4];

    int i;
    for (i = 0; i < samples; i++) {
        double t = (double)(offset + i) / (double)rate;
        double v = amplitude * (sin(2.0 * M_PI * rf * t) +
                                sin(2.0 * M_PI * cf * t));
        if (v > 32767.0) v = 32767.0;
        if (v < -32768.0) v = -32768.0;
        buf[offset + i] = (int16_t)v;
    }
}

/* Test 1: dropout resilience — single 20ms gap mid-digit → single detection */
void test_dtmf_dropout_resilience(void)
{
    int rate = 8000;
    int block = rate / 50; /* 160 samples = 20ms */
    TEST("dropout resilience (20ms gap mid-digit)");

    plcode_dtmf_dec_t *dec = NULL;
    if (plcode_dtmf_dec_create(&dec, rate) != PLCODE_OK) { FAIL("create"); return; }

    /* 300ms total: 120ms tone + 20ms silence + 160ms tone */
    int tone1 = block * 6;   /* 120ms */
    int gap   = block * 1;   /* 20ms */
    int tone2 = block * 8;   /* 160ms */
    int total = tone1 + gap + tone2;
    int16_t *buf = (int16_t *)calloc((size_t)total, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_dtmf_dec_destroy(dec); return; }

    gen_dtmf_tone(buf, 0, tone1, rate, '5', 3000);
    /* gap is already zero from calloc */
    gen_dtmf_tone(buf, tone1 + gap, tone2, rate, '5', 3000);

    /* Feed block by block, count rising edges (0→1 transitions) */
    plcode_dtmf_result_t result;
    memset(&result, 0, sizeof(result));
    int prev_detected = 0;
    int onsets = 0;
    int pos;
    for (pos = 0; pos < total; pos += block) {
        int n = (pos + block <= total) ? block : total - pos;
        plcode_dtmf_dec_process(dec, buf + pos, (size_t)n, &result);
        if (result.detected && !prev_detected)
            onsets++;
        prev_detected = result.detected;
    }

    if (onsets == 1) {
        PASS();
    } else {
        char msg[80];
        snprintf(msg, sizeof(msg), "expected 1 onset, got %d", onsets);
        FAIL(msg);
    }

    free(buf);
    plcode_dtmf_dec_destroy(dec);
}

/* Test 2: same-digit gap rejected (gap < misses_to_end) */
void test_dtmf_same_digit_gap_rejected(void)
{
    int rate = 8000;
    int block = rate / 50;
    TEST("same-digit short gap → single detection");

    plcode_dtmf_dec_t *dec = NULL;
    if (plcode_dtmf_dec_create(&dec, rate) != PLCODE_OK) { FAIL("create"); return; }

    /* 100ms tone + 20ms gap + 100ms same tone (gap < 3 blocks = 60ms) */
    int tone1 = block * 5;
    int gap   = block * 1;
    int tone2 = block * 5;
    int total = tone1 + gap + tone2;
    int16_t *buf = (int16_t *)calloc((size_t)total, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_dtmf_dec_destroy(dec); return; }

    gen_dtmf_tone(buf, 0, tone1, rate, '5', 3000);
    gen_dtmf_tone(buf, tone1 + gap, tone2, rate, '5', 3000);

    plcode_dtmf_result_t result;
    memset(&result, 0, sizeof(result));
    int prev_detected = 0, onsets = 0, pos;
    for (pos = 0; pos < total; pos += block) {
        int n = (pos + block <= total) ? block : total - pos;
        plcode_dtmf_dec_process(dec, buf + pos, (size_t)n, &result);
        if (result.detected && !prev_detected)
            onsets++;
        prev_detected = result.detected;
    }

    if (onsets == 1) {
        PASS();
    } else {
        char msg[80];
        snprintf(msg, sizeof(msg), "expected 1, got %d", onsets);
        FAIL(msg);
    }

    free(buf);
    plcode_dtmf_dec_destroy(dec);
}

/* Test 3: different digit transitions detected without gap */
void test_dtmf_different_digit_no_gap(void)
{
    int rate = 8000;
    int block = rate / 50;
    TEST("different digit immediate transition");

    plcode_dtmf_dec_t *dec = NULL;
    if (plcode_dtmf_dec_create(&dec, rate) != PLCODE_OK) { FAIL("create"); return; }

    int tone1 = block * 6;  /* 120ms of '5' */
    int tone2 = block * 6;  /* 120ms of '8' */
    int total = tone1 + tone2;
    int16_t *buf = (int16_t *)calloc((size_t)total, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_dtmf_dec_destroy(dec); return; }

    gen_dtmf_tone(buf, 0, tone1, rate, '5', 3000);
    gen_dtmf_tone(buf, tone1, tone2, rate, '8', 3000);

    plcode_dtmf_result_t result;
    memset(&result, 0, sizeof(result));
    int got_5 = 0, got_8 = 0, pos;
    for (pos = 0; pos < total; pos += block) {
        int n = (pos + block <= total) ? block : total - pos;
        plcode_dtmf_dec_process(dec, buf + pos, (size_t)n, &result);
        if (result.detected) {
            if (result.digit == '5') got_5 = 1;
            if (result.digit == '8') got_8 = 1;
        }
    }

    if (got_5 && got_8) {
        PASS();
    } else {
        char msg[80];
        snprintf(msg, sizeof(msg), "got_5=%d got_8=%d", got_5, got_8);
        FAIL(msg);
    }

    free(buf);
    plcode_dtmf_dec_destroy(dec);
}

/* Test 4: genuine gap accepted — same digit with long gap → two detections */
void test_dtmf_genuine_gap_accepted(void)
{
    int rate = 8000;
    int block = rate / 50;
    TEST("genuine gap → two detections of same digit");

    plcode_dtmf_dec_t *dec = NULL;
    if (plcode_dtmf_dec_create(&dec, rate) != PLCODE_OK) { FAIL("create"); return; }

    /* 100ms tone + 120ms gap (>= misses_to_end(3)*20ms + min_off(2)*20ms) + 100ms tone */
    int tone1 = block * 5;
    int gap   = block * 6;   /* 120ms — exceeds 60ms miss + 40ms cooldown */
    int tone2 = block * 5;
    int total = tone1 + gap + tone2;
    int16_t *buf = (int16_t *)calloc((size_t)total, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_dtmf_dec_destroy(dec); return; }

    gen_dtmf_tone(buf, 0, tone1, rate, '5', 3000);
    gen_dtmf_tone(buf, tone1 + gap, tone2, rate, '5', 3000);

    plcode_dtmf_result_t result;
    memset(&result, 0, sizeof(result));
    int prev_detected = 0, onsets = 0, pos;
    for (pos = 0; pos < total; pos += block) {
        int n = (pos + block <= total) ? block : total - pos;
        plcode_dtmf_dec_process(dec, buf + pos, (size_t)n, &result);
        if (result.detected && !prev_detected)
            onsets++;
        prev_detected = result.detected;
    }

    if (onsets == 2) {
        PASS();
    } else {
        char msg[80];
        snprintf(msg, sizeof(msg), "expected 2, got %d", onsets);
        FAIL(msg);
    }

    free(buf);
    plcode_dtmf_dec_destroy(dec);
}

/* Test 5: create_ex with custom opts — misses_to_end=1 makes dropout cause 2 detections */
void test_dtmf_create_ex_custom_opts(void)
{
    int rate = 8000;
    int block = rate / 50;
    TEST("create_ex custom opts (misses_to_end=1)");

    plcode_dtmf_dec_opts_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.misses_to_end = 1;  /* Single miss ends digit */
    opts.min_off_frames = 1;

    plcode_dtmf_dec_t *dec = NULL;
    if (plcode_dtmf_dec_create_ex(&dec, rate, &opts) != PLCODE_OK) { FAIL("create"); return; }

    /* 100ms tone + 20ms gap + 100ms same tone
     * With misses_to_end=1, a single missed block ends the digit */
    int tone1 = block * 5;
    int gap   = block * 1;
    int tone2 = block * 5;
    int total = tone1 + gap + tone2;
    int16_t *buf = (int16_t *)calloc((size_t)total, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_dtmf_dec_destroy(dec); return; }

    gen_dtmf_tone(buf, 0, tone1, rate, '5', 3000);
    gen_dtmf_tone(buf, tone1 + gap, tone2, rate, '5', 3000);

    plcode_dtmf_result_t result;
    memset(&result, 0, sizeof(result));
    int prev_detected = 0, onsets = 0, pos;
    for (pos = 0; pos < total; pos += block) {
        int n = (pos + block <= total) ? block : total - pos;
        plcode_dtmf_dec_process(dec, buf + pos, (size_t)n, &result);
        if (result.detected && !prev_detected)
            onsets++;
        prev_detected = result.detected;
    }

    if (onsets == 2) {
        PASS();
    } else {
        char msg[80];
        snprintf(msg, sizeof(msg), "expected 2 with misses_to_end=1, got %d", onsets);
        FAIL(msg);
    }

    free(buf);
    plcode_dtmf_dec_destroy(dec);
}

/* Test 6: asymmetric twist accept — column 3.5x row amplitude (within 12dB normal) */
void test_dtmf_twist_accept(void)
{
    int rate = 8000;
    int block = rate / 50;
    TEST("asymmetric twist accept (col 3.5x row)");

    plcode_dtmf_dec_t *dec = NULL;
    if (plcode_dtmf_dec_create(&dec, rate) != PLCODE_OK) { FAIL("create"); return; }

    /* '5' = 770 Hz row + 1336 Hz col
     * Row amplitude 1000, col amplitude 3500 → power ratio ~12.25x (within 16x limit) */
    int total = block * 10;  /* 200ms */
    int16_t *buf = (int16_t *)calloc((size_t)total, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_dtmf_dec_destroy(dec); return; }

    int i;
    for (i = 0; i < total; i++) {
        double t = (double)i / (double)rate;
        double v = 1000.0 * sin(2.0 * M_PI * 770.0 * t) +
                   3500.0 * sin(2.0 * M_PI * 1336.0 * t);
        if (v > 32767.0) v = 32767.0;
        if (v < -32768.0) v = -32768.0;
        buf[i] = (int16_t)v;
    }

    plcode_dtmf_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_dtmf_dec_process(dec, buf, (size_t)total, &result);

    if (result.detected && result.digit == '5') {
        PASS();
    } else {
        char msg[80];
        snprintf(msg, sizeof(msg), "detected=%d digit='%c'",
                 result.detected, result.digit ? result.digit : '?');
        FAIL(msg);
    }

    free(buf);
    plcode_dtmf_dec_destroy(dec);
}

/* Test 7: asymmetric twist reject — column 5x row (exceeds 12dB = 16x power) */
void test_dtmf_twist_reject(void)
{
    int rate = 8000;
    int block = rate / 50;
    TEST("asymmetric twist reject (col 5x row)");

    plcode_dtmf_dec_t *dec = NULL;
    if (plcode_dtmf_dec_create(&dec, rate) != PLCODE_OK) { FAIL("create"); return; }

    /* Row amplitude 500, col amplitude 2500 → power ratio 25x (exceeds 16x) */
    int total = block * 10;
    int16_t *buf = (int16_t *)calloc((size_t)total, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_dtmf_dec_destroy(dec); return; }

    int i;
    for (i = 0; i < total; i++) {
        double t = (double)i / (double)rate;
        double v = 500.0 * sin(2.0 * M_PI * 770.0 * t) +
                   2500.0 * sin(2.0 * M_PI * 1336.0 * t);
        if (v > 32767.0) v = 32767.0;
        if (v < -32768.0) v = -32768.0;
        buf[i] = (int16_t)v;
    }

    plcode_dtmf_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_dtmf_dec_process(dec, buf, (size_t)total, &result);

    if (!result.detected) {
        PASS();
    } else {
        FAIL("should reject extreme twist");
    }

    free(buf);
    plcode_dtmf_dec_destroy(dec);
}

/* Test 8: harmonic rejection — fundamental + strong 2nd harmonic → reject */
void test_dtmf_harmonic_rejection(void)
{
    int rate = 8000;
    int block = rate / 50;
    TEST("harmonic rejection (strong 2nd harmonic)");

    plcode_dtmf_dec_t *dec = NULL;
    if (plcode_dtmf_dec_create(&dec, rate) != PLCODE_OK) { FAIL("create"); return; }

    /* '5' = 770 Hz + 1336 Hz, add strong 2nd harmonics at 1540 Hz and 2672 Hz */
    int total = block * 10;
    int16_t *buf = (int16_t *)calloc((size_t)total, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_dtmf_dec_destroy(dec); return; }

    int i;
    for (i = 0; i < total; i++) {
        double t = (double)i / (double)rate;
        double v = 3000.0 * sin(2.0 * M_PI * 770.0 * t) +
                   3000.0 * sin(2.0 * M_PI * 1336.0 * t) +
                   2500.0 * sin(2.0 * M_PI * 1540.0 * t) +   /* 83% of row fund */
                   2500.0 * sin(2.0 * M_PI * 2672.0 * t);     /* 83% of col fund */
        if (v > 32767.0) v = 32767.0;
        if (v < -32768.0) v = -32768.0;
        buf[i] = (int16_t)v;
    }

    plcode_dtmf_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_dtmf_dec_process(dec, buf, (size_t)total, &result);

    if (!result.detected) {
        PASS();
    } else {
        FAIL("should reject signal with strong harmonics");
    }

    free(buf);
    plcode_dtmf_dec_destroy(dec);
}

/* Test 9: create_ex NULL opts matches create defaults */
void test_dtmf_create_ex_defaults(void)
{
    int rate = 8000;
    TEST("create_ex(NULL) matches create() defaults");

    plcode_dtmf_dec_t *dec1 = NULL, *dec2 = NULL;
    if (plcode_dtmf_dec_create(&dec1, rate) != PLCODE_OK) { FAIL("create"); return; }
    if (plcode_dtmf_dec_create_ex(&dec2, rate, NULL) != PLCODE_OK) {
        FAIL("create_ex");
        plcode_dtmf_dec_destroy(dec1);
        return;
    }

    /* Feed identical signal, compare results */
    int total = rate / 5; /* 200ms */
    int16_t *buf = (int16_t *)calloc((size_t)total, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); goto cleanup; }

    gen_dtmf_tone(buf, 0, total, rate, '5', 3000);

    plcode_dtmf_result_t r1, r2;
    memset(&r1, 0, sizeof(r1));
    memset(&r2, 0, sizeof(r2));
    plcode_dtmf_dec_process(dec1, buf, (size_t)total, &r1);
    plcode_dtmf_dec_process(dec2, buf, (size_t)total, &r2);

    if (r1.detected == r2.detected && r1.digit == r2.digit) {
        PASS();
    } else {
        FAIL("results differ");
    }

    free(buf);
cleanup:
    plcode_dtmf_dec_destroy(dec1);
    plcode_dtmf_dec_destroy(dec2);
}

int test_dtmf(void)
{
    printf("DTMF tests:\n");

    test_dtmf_encode_decode_single();
    test_dtmf_all_digits_all_rates();
    test_dtmf_silence_rejection();
    test_dtmf_noise_rejection();
    test_dtmf_single_tone_rejection();
    test_dtmf_dropout_resilience();
    test_dtmf_same_digit_gap_rejected();
    test_dtmf_different_digit_no_gap();
    test_dtmf_genuine_gap_accepted();
    test_dtmf_create_ex_custom_opts();
    test_dtmf_twist_accept();
    test_dtmf_twist_reject();
    test_dtmf_harmonic_rejection();
    test_dtmf_create_ex_defaults();

    printf("  DTMF: %d/%d passed\n\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
