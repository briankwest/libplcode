#include "../include/plcode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

int test_dtmf(void)
{
    printf("DTMF tests:\n");

    test_dtmf_encode_decode_single();
    test_dtmf_all_digits_all_rates();
    test_dtmf_silence_rejection();
    test_dtmf_noise_rejection();
    test_dtmf_single_tone_rejection();

    printf("  DTMF: %d/%d passed\n\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
