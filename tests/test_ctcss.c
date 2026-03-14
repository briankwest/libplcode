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

void test_ctcss_encode_decode_single(void)
{
    int rate = 8000;
    uint16_t freq = 1000; /* 100.0 Hz */
    int16_t amplitude = 3000;

    TEST("encode/decode 100.0 Hz @ 8000 Hz");

    plcode_ctcss_enc_t *enc = NULL;
    plcode_ctcss_dec_t *dec = NULL;

    int rc = plcode_ctcss_enc_create(&enc, rate, freq, amplitude);
    if (rc != PLCODE_OK) { FAIL("enc create failed"); return; }

    rc = plcode_ctcss_dec_create(&dec, rate);
    if (rc != PLCODE_OK) { FAIL("dec create failed"); plcode_ctcss_enc_destroy(enc); return; }

    /* Generate 3 seconds of tone (need 2 blocks for hysteresis) */
    int total_samples = rate * 3;
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc failed"); goto cleanup; }

    plcode_ctcss_enc_process(enc, buf, (size_t)total_samples);

    plcode_ctcss_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_ctcss_dec_process(dec, buf, (size_t)total_samples, &result);

    if (result.detected && result.tone_freq_x10 == freq) {
        PASS();
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "detected=%d freq=%u (expected %u)",
                 result.detected, result.tone_freq_x10, freq);
        FAIL(msg);
    }

cleanup:
    free(buf);
    plcode_ctcss_enc_destroy(enc);
    plcode_ctcss_dec_destroy(dec);
}

void test_ctcss_all_tones_all_rates(void)
{
    int r, t;
    int total = 0, passed_count = 0;
    char testname[80];

    for (r = 0; r < NUM_RATES; r++) {
        int rate = sample_rates[r];

        for (t = 0; t < PLCODE_CTCSS_NUM_TONES; t++) {
            uint16_t freq = plcode_ctcss_tone_freq_x10(t);
            if (freq == 0) continue;

            total++;

            plcode_ctcss_enc_t *enc = NULL;
            plcode_ctcss_dec_t *dec = NULL;

            if (plcode_ctcss_enc_create(&enc, rate, freq, 3000) != PLCODE_OK) continue;
            if (plcode_ctcss_dec_create(&dec, rate) != PLCODE_OK) {
                plcode_ctcss_enc_destroy(enc);
                continue;
            }

            int total_samples = rate * 3;
            int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
            if (!buf) {
                plcode_ctcss_enc_destroy(enc);
                plcode_ctcss_dec_destroy(dec);
                continue;
            }

            plcode_ctcss_enc_process(enc, buf, (size_t)total_samples);

            plcode_ctcss_result_t result;
            memset(&result, 0, sizeof(result));
            plcode_ctcss_dec_process(dec, buf, (size_t)total_samples, &result);

            if (result.detected && result.tone_freq_x10 == freq) {
                passed_count++;
            } else {
                printf("\n    FAIL: tone %.1f Hz @ %d Hz (detected=%d, got=%u)",
                       (double)freq / 10.0, rate, result.detected, result.tone_freq_x10);
            }

            free(buf);
            plcode_ctcss_enc_destroy(enc);
            plcode_ctcss_dec_destroy(dec);
        }
    }

    snprintf(testname, sizeof(testname), "all tones x all rates (%d combos)", total);
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

void test_ctcss_silence_rejection(void)
{
    int rate = 8000;
    TEST("no false detection on silence");

    plcode_ctcss_dec_t *dec = NULL;
    if (plcode_ctcss_dec_create(&dec, rate) != PLCODE_OK) { FAIL("create failed"); return; }

    int total_samples = rate * 3;
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_ctcss_dec_destroy(dec); return; }

    /* Buffer is already zeroed (silence) */
    plcode_ctcss_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_ctcss_dec_process(dec, buf, (size_t)total_samples, &result);

    if (!result.detected) {
        PASS();
    } else {
        FAIL("false detection on silence");
    }

    free(buf);
    plcode_ctcss_dec_destroy(dec);
}

void test_ctcss_noise_rejection(void)
{
    int rate = 8000;
    TEST("no false detection on white noise");

    plcode_ctcss_dec_t *dec = NULL;
    if (plcode_ctcss_dec_create(&dec, rate) != PLCODE_OK) { FAIL("create failed"); return; }

    int total_samples = rate * 3;
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_ctcss_dec_destroy(dec); return; }

    /* Fill with pseudo-random noise */
    unsigned int seed = 12345;
    int i;
    for (i = 0; i < total_samples; i++) {
        seed = seed * 1103515245 + 12345;
        buf[i] = (int16_t)((seed >> 16) & 0x7FFF) - 16384;
    }

    plcode_ctcss_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_ctcss_dec_process(dec, buf, (size_t)total_samples, &result);

    if (!result.detected) {
        PASS();
    } else {
        FAIL("false detection on noise");
    }

    free(buf);
    plcode_ctcss_dec_destroy(dec);
}

void test_ctcss_adjacent_rejection(void)
{
    int rate = 8000;
    /* Encode 67.0 Hz, verify decoder does not report 69.3 Hz */
    TEST("adjacent tone rejection (67.0 vs 69.3 Hz)");

    plcode_ctcss_enc_t *enc = NULL;
    plcode_ctcss_dec_t *dec = NULL;

    if (plcode_ctcss_enc_create(&enc, rate, 670, 3000) != PLCODE_OK) { FAIL("enc create"); return; }
    if (plcode_ctcss_dec_create(&dec, rate) != PLCODE_OK) { FAIL("dec create"); plcode_ctcss_enc_destroy(enc); return; }

    int total_samples = rate * 3;
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); goto adj_cleanup; }

    plcode_ctcss_enc_process(enc, buf, (size_t)total_samples);

    plcode_ctcss_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_ctcss_dec_process(dec, buf, (size_t)total_samples, &result);

    if (result.detected && result.tone_freq_x10 == 670) {
        PASS();
    } else if (result.detected && result.tone_freq_x10 == 693) {
        FAIL("detected adjacent tone 69.3 instead of 67.0");
    } else {
        char msg[80];
        snprintf(msg, sizeof(msg), "unexpected: detected=%d freq=%u", result.detected, result.tone_freq_x10);
        FAIL(msg);
    }

adj_cleanup:
    free(buf);
    plcode_ctcss_enc_destroy(enc);
    plcode_ctcss_dec_destroy(dec);
}

int test_ctcss(void)
{
    printf("CTCSS tests:\n");

    test_ctcss_encode_decode_single();
    test_ctcss_all_tones_all_rates();
    test_ctcss_silence_rejection();
    test_ctcss_noise_rejection();
    test_ctcss_adjacent_rejection();

    printf("  CTCSS: %d/%d passed\n\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
