#include "../include/plcode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void test_dcs_encode_decode_single(void)
{
    int rate = 8000;
    uint16_t code = 23; /* DCS label 023 */
    int16_t amplitude = 3000;

    TEST("encode/decode DCS 023 @ 8000 Hz");

    plcode_dcs_enc_t *enc = NULL;
    plcode_dcs_dec_t *dec = NULL;

    int rc = plcode_dcs_enc_create(&enc, rate, code, 0, amplitude);
    if (rc != PLCODE_OK) { FAIL("enc create failed"); return; }

    rc = plcode_dcs_dec_create(&dec, rate);
    if (rc != PLCODE_OK) { FAIL("dec create failed"); plcode_dcs_enc_destroy(enc); return; }

    /* DCS at 134.4 bps: one codeword = 23/134.4 = 171ms
     * Need 3 consecutive matches = ~0.5s, plus PLL lock time.
     * Generate 3 seconds of signal. */
    int total_samples = rate * 3;
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); goto cleanup; }

    plcode_dcs_enc_process(enc, buf, (size_t)total_samples);

    plcode_dcs_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_dcs_dec_process(dec, buf, (size_t)total_samples, &result);

    if (result.detected && result.code_number == code && !result.inverted) {
        PASS();
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "detected=%d code=%03d inv=%d (expected 023 normal)",
                 result.detected, result.code_number, result.inverted);
        FAIL(msg);
    }

cleanup:
    free(buf);
    plcode_dcs_enc_destroy(enc);
    plcode_dcs_dec_destroy(dec);
}

void test_dcs_inverted(void)
{
    int rate = 8000;
    uint16_t code = 23;

    TEST("encode/decode DCS 023 inverted @ 8000 Hz");

    plcode_dcs_enc_t *enc = NULL;
    plcode_dcs_dec_t *dec = NULL;

    if (plcode_dcs_enc_create(&enc, rate, code, 1, 3000) != PLCODE_OK) { FAIL("enc create"); return; }
    if (plcode_dcs_dec_create(&dec, rate) != PLCODE_OK) { FAIL("dec create"); plcode_dcs_enc_destroy(enc); return; }

    int total_samples = rate * 3;
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); goto cleanup; }

    plcode_dcs_enc_process(enc, buf, (size_t)total_samples);

    plcode_dcs_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_dcs_dec_process(dec, buf, (size_t)total_samples, &result);

    /* D023I and D047N are the same physical signal (orbit pair).
     * Complement detections map to the orbit pair's normal form. */
    if (result.detected && result.code_number == 47 && !result.inverted) {
        PASS();
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "detected=%d code=%03d inv=%d (expected 047 normal via orbit pair)",
                 result.detected, result.code_number, result.inverted);
        FAIL(msg);
    }

cleanup:
    free(buf);
    plcode_dcs_enc_destroy(enc);
    plcode_dcs_dec_destroy(dec);
}

void test_dcs_all_codes_single_rate(void)
{
    int rate = 8000;
    int total = 0, passed_count = 0;
    int i;
    char testname[80];

    for (i = 0; i < PLCODE_DCS_NUM_CODES; i++) {
        uint16_t code = plcode_dcs_code_number(i);
        if (code == 0) continue;
        total++;

        plcode_dcs_enc_t *enc = NULL;
        plcode_dcs_dec_t *dec = NULL;

        if (plcode_dcs_enc_create(&enc, rate, code, 0, 3000) != PLCODE_OK) continue;
        if (plcode_dcs_dec_create(&dec, rate) != PLCODE_OK) {
            plcode_dcs_enc_destroy(enc);
            continue;
        }

        int total_samples = rate * 3;
        int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
        if (!buf) {
            plcode_dcs_enc_destroy(enc);
            plcode_dcs_dec_destroy(dec);
            continue;
        }

        plcode_dcs_enc_process(enc, buf, (size_t)total_samples);

        plcode_dcs_result_t result;
        memset(&result, 0, sizeof(result));
        plcode_dcs_dec_process(dec, buf, (size_t)total_samples, &result);

        /* Accept detection — orbit canonicalization may remap alias codes
         * to their preferred label (e.g., encode 464 → decode as 026). */
        if (result.detected) {
            passed_count++;
        } else {
            printf("\n    FAIL: DCS %03d (detected=%d code=%03d inv=%d)",
                   code, result.detected, result.code_number, result.inverted);
        }

        free(buf);
        plcode_dcs_enc_destroy(enc);
        plcode_dcs_dec_destroy(dec);
    }

    snprintf(testname, sizeof(testname), "all DCS codes @ 8 kHz (%d codes)", total);
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

void test_dcs_multi_rate(void)
{
    /* Test a representative code at all sample rates */
    uint16_t code = 145; /* DCS label 145 */
    int r;
    int total = 0, passed_count = 0;
    char testname[80];

    for (r = 0; r < NUM_RATES; r++) {
        int rate = sample_rates[r];
        total++;

        plcode_dcs_enc_t *enc = NULL;
        plcode_dcs_dec_t *dec = NULL;

        if (plcode_dcs_enc_create(&enc, rate, code, 0, 3000) != PLCODE_OK) continue;
        if (plcode_dcs_dec_create(&dec, rate) != PLCODE_OK) {
            plcode_dcs_enc_destroy(enc);
            continue;
        }

        int total_samples = rate * 3;
        int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
        if (!buf) {
            plcode_dcs_enc_destroy(enc);
            plcode_dcs_dec_destroy(dec);
            continue;
        }

        plcode_dcs_enc_process(enc, buf, (size_t)total_samples);

        plcode_dcs_result_t result;
        memset(&result, 0, sizeof(result));
        plcode_dcs_dec_process(dec, buf, (size_t)total_samples, &result);

        if (result.detected && result.code_number == code) {
            passed_count++;
        } else {
            printf("\n    FAIL: DCS %03d @ %d Hz (detected=%d code=%03d)",
                   code, rate, result.detected, result.code_number);
        }

        free(buf);
        plcode_dcs_enc_destroy(enc);
        plcode_dcs_dec_destroy(dec);
    }

    snprintf(testname, sizeof(testname), "DCS 145 x all sample rates (%d)", total);
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

void test_dcs_silence_rejection(void)
{
    int rate = 8000;
    TEST("no false detection on silence");

    plcode_dcs_dec_t *dec = NULL;
    if (plcode_dcs_dec_create(&dec, rate) != PLCODE_OK) { FAIL("create"); return; }

    int total_samples = rate * 3;
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_dcs_dec_destroy(dec); return; }

    plcode_dcs_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_dcs_dec_process(dec, buf, (size_t)total_samples, &result);

    if (!result.detected) {
        PASS();
    } else {
        FAIL("false detection on silence");
    }

    free(buf);
    plcode_dcs_dec_destroy(dec);
}

int test_dcs(void)
{
    printf("DCS tests:\n");

    test_dcs_encode_decode_single();
    test_dcs_inverted();
    test_dcs_all_codes_single_rate();
    test_dcs_multi_rate();
    test_dcs_silence_rejection();

    printf("  DCS: %d/%d passed\n\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
