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

void test_twotone_roundtrip(void)
{
    int rate = 8000;
    /* Use two distinct paging tones: index 0 (330.5 Hz) and index 5 (433.7 Hz) */
    uint16_t tone_a = plcode_twotone_freq_x10(0);  /* 3305 */
    uint16_t tone_b = plcode_twotone_freq_x10(5);  /* 4337 */

    TEST("two-tone encode/decode @ 8000 Hz");

    plcode_twotone_enc_t *enc = NULL;
    plcode_twotone_dec_t *dec = NULL;

    if (plcode_twotone_enc_create(&enc, rate, tone_a, tone_b,
                                    1000, 3000, 3000) != PLCODE_OK) {
        FAIL("enc create"); return;
    }
    if (plcode_twotone_dec_create(&dec, rate) != PLCODE_OK) {
        FAIL("dec create"); plcode_twotone_enc_destroy(enc); return;
    }

    int total_samples = rate * 5;
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); goto cleanup; }

    plcode_twotone_enc_process(enc, buf, (size_t)total_samples);

    plcode_twotone_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_twotone_dec_process(dec, buf, (size_t)total_samples, &result);

    if (result.detected &&
        result.tone_a_freq_x10 == tone_a &&
        result.tone_b_freq_x10 == tone_b) {
        PASS();
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "det=%d a=%u(exp %u) b=%u(exp %u)",
                 result.detected,
                 result.tone_a_freq_x10, tone_a,
                 result.tone_b_freq_x10, tone_b);
        FAIL(msg);
    }

cleanup:
    free(buf);
    plcode_twotone_enc_destroy(enc);
    plcode_twotone_dec_destroy(dec);
}

void test_twotone_silence(void)
{
    int rate = 8000;
    TEST("two-tone no false detection on silence");

    plcode_twotone_dec_t *dec = NULL;
    if (plcode_twotone_dec_create(&dec, rate) != PLCODE_OK) {
        FAIL("create"); return;
    }

    int total_samples = rate * 5;
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_twotone_dec_destroy(dec); return; }

    plcode_twotone_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_twotone_dec_process(dec, buf, (size_t)total_samples, &result);

    if (!result.detected) { PASS(); } else { FAIL("false detect on silence"); }

    free(buf);
    plcode_twotone_dec_destroy(dec);
}

void test_twotone_multi_rate(void)
{
    static const int rates[] = { 8000, 16000, 32000, 48000 };
    int r, total = 0, passed_count = 0;
    char testname[80];
    uint16_t tone_a = plcode_twotone_freq_x10(10); /* 569.1 Hz */
    uint16_t tone_b = plcode_twotone_freq_x10(20); /* 979.9 Hz */

    for (r = 0; r < 4; r++) {
        int rate = rates[r];
        plcode_twotone_enc_t *enc = NULL;
        plcode_twotone_dec_t *dec = NULL;
        total++;

        if (plcode_twotone_enc_create(&enc, rate, tone_a, tone_b,
                                        1000, 3000, 3000) != PLCODE_OK) continue;
        if (plcode_twotone_dec_create(&dec, rate) != PLCODE_OK) {
            plcode_twotone_enc_destroy(enc); continue;
        }

        int total_samples = rate * 5;
        int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
        if (!buf) {
            plcode_twotone_enc_destroy(enc);
            plcode_twotone_dec_destroy(dec);
            continue;
        }

        plcode_twotone_enc_process(enc, buf, (size_t)total_samples);

        plcode_twotone_result_t result;
        memset(&result, 0, sizeof(result));
        plcode_twotone_dec_process(dec, buf, (size_t)total_samples, &result);

        if (result.detected &&
            result.tone_a_freq_x10 == tone_a &&
            result.tone_b_freq_x10 == tone_b) {
            passed_count++;
        } else {
            printf("\n    FAIL: @ %d Hz", rate);
        }

        free(buf);
        plcode_twotone_enc_destroy(enc);
        plcode_twotone_dec_destroy(dec);
    }

    snprintf(testname, sizeof(testname), "two-tone x all sample rates (%d)", total);
    TEST(testname);
    if (passed_count == total) { PASS(); }
    else {
        char msg[80];
        snprintf(msg, sizeof(msg), "%d/%d passed", passed_count, total);
        printf("\n"); FAIL(msg);
    }
}

int test_twotone(void)
{
    printf("Two-Tone Paging tests:\n");
    test_twotone_roundtrip();
    test_twotone_silence();
    test_twotone_multi_rate();
    printf("  Two-Tone: %d/%d passed\n\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
