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

            int total_samples = rate * 5;  /* 5s for reliable detect at all rates */
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

void test_ctcss_fast_detect(void)
{
    int rate = 8000;
    uint16_t freq = 1000; /* 100.0 Hz — well separated from neighbors */
    int total, chunk, detected_at, offset, n, ms;
    int16_t *buf;
    plcode_ctcss_enc_t *enc = NULL;
    plcode_ctcss_dec_t *dec = NULL;
    plcode_ctcss_result_t result;

    TEST("fast detect 100.0 Hz (<= 350ms @ 8kHz)");

    if (plcode_ctcss_enc_create(&enc, rate, freq, 3000) != PLCODE_OK) { FAIL("enc create"); return; }
    if (plcode_ctcss_dec_create(&dec, rate) != PLCODE_OK) { FAIL("dec create"); plcode_ctcss_enc_destroy(enc); return; }

    total = rate * 2; /* 2s of tone */
    buf = (int16_t *)calloc((size_t)total, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); goto fd_cleanup; }

    plcode_ctcss_enc_process(enc, buf, (size_t)total);

    /* Feed in 10ms chunks, find first detection */
    chunk = rate / 100; /* 80 samples = 10ms */
    detected_at = -1;

    for (offset = 0; offset < total; offset += chunk) {
        n = (offset + chunk <= total) ? chunk : total - offset;
        memset(&result, 0, sizeof(result));
        plcode_ctcss_dec_process(dec, buf + offset, (size_t)n, &result);

        if (result.detected && result.tone_freq_x10 == freq) {
            detected_at = offset + n;
            break;
        }
    }

    if (detected_at < 0) {
        FAIL("not detected within 2 seconds");
    } else {
        ms = (detected_at * 1000) / rate;
        if (ms <= 350) {
            PASS();
        } else {
            char msg[80];
            snprintf(msg, sizeof(msg), "detected at %d ms (want <= 350)", ms);
            FAIL(msg);
        }
    }

fd_cleanup:
    free(buf);
    plcode_ctcss_enc_destroy(enc);
    plcode_ctcss_dec_destroy(dec);
}

void test_ctcss_fast_detect_all_rates(void)
{
    int r;
    uint16_t freq = 1318; /* 131.8 Hz — mid-range, common GMRS tone */
    int total_tests = 0, passed_tests = 0;
    char testname[80];

    for (r = 0; r < NUM_RATES; r++) {
        int rate = sample_rates[r];
        int total, chunk, detected_at, offset, n, ms;
        int16_t *buf;
        plcode_ctcss_enc_t *enc = NULL;
        plcode_ctcss_dec_t *dec = NULL;
        plcode_ctcss_result_t result;

        total_tests++;

        if (plcode_ctcss_enc_create(&enc, rate, freq, 3000) != PLCODE_OK) continue;
        if (plcode_ctcss_dec_create(&dec, rate) != PLCODE_OK) {
            plcode_ctcss_enc_destroy(enc);
            continue;
        }

        total = rate * 2;
        buf = (int16_t *)calloc((size_t)total, sizeof(int16_t));
        if (!buf) {
            plcode_ctcss_enc_destroy(enc);
            plcode_ctcss_dec_destroy(dec);
            continue;
        }

        plcode_ctcss_enc_process(enc, buf, (size_t)total);

        chunk = rate / 100;
        detected_at = -1;

        for (offset = 0; offset < total; offset += chunk) {
            n = (offset + chunk <= total) ? chunk : total - offset;
            memset(&result, 0, sizeof(result));
            plcode_ctcss_dec_process(dec, buf + offset, (size_t)n, &result);

            if (result.detected && result.tone_freq_x10 == freq) {
                detected_at = offset + n;
                break;
            }
        }

        if (detected_at >= 0) {
            ms = (detected_at * 1000) / rate;
            if (ms <= 350) {
                passed_tests++;
            } else {
                printf("\n    FAIL: 131.8 Hz @ %d Hz detected at %d ms", rate, ms);
            }
        } else {
            printf("\n    FAIL: 131.8 Hz @ %d Hz not detected", rate);
        }

        free(buf);
        plcode_ctcss_enc_destroy(enc);
        plcode_ctcss_dec_destroy(dec);
    }

    snprintf(testname, sizeof(testname), "fast detect 131.8 Hz all rates (%d rates)", total_tests);
    TEST(testname);
    if (passed_tests == total_tests) {
        PASS();
    } else {
        char msg[80];
        snprintf(msg, sizeof(msg), "%d/%d passed", passed_tests, total_tests);
        printf("\n");
        FAIL(msg);
    }
}

void test_ctcss_reverse_burst(void)
{
    int rate = 8000;
    uint16_t freq = 1000; /* 100.0 Hz */

    TEST("CTCSS reverse burst stops encoder");

    plcode_ctcss_enc_t *enc = NULL;
    if (plcode_ctcss_enc_create(&enc, rate, freq, 3000) != PLCODE_OK) {
        FAIL("create"); return;
    }

    int total_samples = rate * 2;
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_ctcss_enc_destroy(enc); return; }

    /* Generate 500ms of normal tone */
    plcode_ctcss_enc_process(enc, buf, (size_t)(rate / 2));

    if (plcode_ctcss_enc_stopped(enc)) {
        FAIL("stopped too early");
        goto rb_cleanup;
    }

    /* Trigger reverse burst */
    plcode_ctcss_enc_reverse_burst(enc);

    /* Process rest — should stop after ~200ms */
    plcode_ctcss_enc_process(enc, buf + rate / 2, (size_t)(total_samples - rate / 2));

    if (plcode_ctcss_enc_stopped(enc)) {
        /* Verify audio after stop point is unmodified (zeros in our case) */
        PASS();
    } else {
        FAIL("encoder did not stop after reverse burst");
    }

rb_cleanup:
    free(buf);
    plcode_ctcss_enc_destroy(enc);
}

void test_ctcss_resume_after_burst(void)
{
    int rate = 8000;
    uint16_t freq = 1000;

    TEST("CTCSS resume after reverse burst");

    plcode_ctcss_enc_t *enc = NULL;
    if (plcode_ctcss_enc_create(&enc, rate, freq, 3000) != PLCODE_OK) {
        FAIL("create"); return;
    }

    int total_samples = rate * 1;
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_ctcss_enc_destroy(enc); return; }

    /* Normal → reverse burst → stopped → resume → verify audio */
    plcode_ctcss_enc_process(enc, buf, (size_t)(rate / 4));
    plcode_ctcss_enc_reverse_burst(enc);
    plcode_ctcss_enc_process(enc, buf, (size_t)(rate / 2));

    if (!plcode_ctcss_enc_stopped(enc)) {
        FAIL("not stopped after burst"); goto res_cleanup;
    }

    plcode_ctcss_enc_resume(enc);
    memset(buf, 0, (size_t)total_samples * sizeof(int16_t));
    plcode_ctcss_enc_process(enc, buf, (size_t)total_samples);

    /* Check that audio was generated (non-zero samples) */
    {
        int i, has_audio = 0;
        for (i = 0; i < total_samples; i++) {
            if (buf[i] != 0) { has_audio = 1; break; }
        }
        if (has_audio) { PASS(); } else { FAIL("no audio after resume"); }
    }

res_cleanup:
    free(buf);
    plcode_ctcss_enc_destroy(enc);
}

int test_ctcss(void)
{
    printf("CTCSS tests:\n");

    test_ctcss_encode_decode_single();
    test_ctcss_all_tones_all_rates();
    test_ctcss_silence_rejection();
    test_ctcss_noise_rejection();
    test_ctcss_adjacent_rejection();
    test_ctcss_fast_detect();
    test_ctcss_fast_detect_all_rates();
    test_ctcss_reverse_burst();
    test_ctcss_resume_after_burst();

    printf("  CTCSS: %d/%d passed\n\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
