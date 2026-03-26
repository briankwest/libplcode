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

void test_toneburst_roundtrip(void)
{
    int rate = 8000;
    TEST("1750 Hz burst encode/decode @ 8000 Hz");

    plcode_toneburst_enc_t *enc = NULL;
    plcode_toneburst_dec_t *dec = NULL;

    if (plcode_toneburst_enc_create(&enc, rate, 1750, 500, 3000) != PLCODE_OK) {
        FAIL("enc create"); return;
    }
    if (plcode_toneburst_dec_create(&dec, rate, 1750, 200) != PLCODE_OK) {
        FAIL("dec create"); plcode_toneburst_enc_destroy(enc); return;
    }

    int total_samples = rate * 2;
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); goto cleanup; }

    plcode_toneburst_enc_process(enc, buf, (size_t)total_samples);

    plcode_toneburst_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_toneburst_dec_process(dec, buf, (size_t)total_samples, &result);

    if (result.detected) { PASS(); } else { FAIL("burst not detected"); }

cleanup:
    free(buf);
    plcode_toneburst_enc_destroy(enc);
    plcode_toneburst_dec_destroy(dec);
}

void test_toneburst_silence(void)
{
    int rate = 8000;
    TEST("tone burst no false detection on silence");

    plcode_toneburst_dec_t *dec = NULL;
    if (plcode_toneburst_dec_create(&dec, rate, 1750, 200) != PLCODE_OK) {
        FAIL("create"); return;
    }

    int total_samples = rate * 2;
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_toneburst_dec_destroy(dec); return; }

    plcode_toneburst_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_toneburst_dec_process(dec, buf, (size_t)total_samples, &result);

    if (!result.detected) { PASS(); } else { FAIL("false detect"); }

    free(buf);
    plcode_toneburst_dec_destroy(dec);
}

void test_toneburst_short_reject(void)
{
    int rate = 8000;
    TEST("tone burst rejects short burst (50ms < 200ms min)");

    plcode_toneburst_enc_t *enc = NULL;
    plcode_toneburst_dec_t *dec = NULL;

    /* Encode 50ms burst, require 200ms minimum */
    if (plcode_toneburst_enc_create(&enc, rate, 1750, 50, 3000) != PLCODE_OK) {
        FAIL("enc create"); return;
    }
    if (plcode_toneburst_dec_create(&dec, rate, 1750, 200) != PLCODE_OK) {
        FAIL("dec create"); plcode_toneburst_enc_destroy(enc); return;
    }

    int total_samples = rate * 1;
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); goto cleanup; }

    plcode_toneburst_enc_process(enc, buf, (size_t)total_samples);

    plcode_toneburst_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_toneburst_dec_process(dec, buf, (size_t)total_samples, &result);

    if (!result.detected) { PASS(); } else { FAIL("accepted short burst"); }

cleanup:
    free(buf);
    plcode_toneburst_enc_destroy(enc);
    plcode_toneburst_dec_destroy(dec);
}

void test_toneburst_multi_rate(void)
{
    static const int rates[] = { 8000, 16000, 32000, 48000 };
    int r, total = 0, passed_count = 0;
    char testname[80];

    for (r = 0; r < 4; r++) {
        int rate = rates[r];
        plcode_toneburst_enc_t *enc = NULL;
        plcode_toneburst_dec_t *dec = NULL;
        total++;

        if (plcode_toneburst_enc_create(&enc, rate, 1750, 500, 3000) != PLCODE_OK)
            continue;
        if (plcode_toneburst_dec_create(&dec, rate, 1750, 200) != PLCODE_OK) {
            plcode_toneburst_enc_destroy(enc); continue;
        }

        int total_samples = rate * 2;
        int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
        if (!buf) {
            plcode_toneburst_enc_destroy(enc);
            plcode_toneburst_dec_destroy(dec);
            continue;
        }

        plcode_toneburst_enc_process(enc, buf, (size_t)total_samples);

        plcode_toneburst_result_t result;
        memset(&result, 0, sizeof(result));
        plcode_toneburst_dec_process(dec, buf, (size_t)total_samples, &result);

        if (result.detected) passed_count++;
        else printf("\n    FAIL: @ %d Hz", rate);

        free(buf);
        plcode_toneburst_enc_destroy(enc);
        plcode_toneburst_dec_destroy(dec);
    }

    snprintf(testname, sizeof(testname), "tone burst x all sample rates (%d)", total);
    TEST(testname);
    if (passed_count == total) { PASS(); }
    else {
        char msg[80];
        snprintf(msg, sizeof(msg), "%d/%d passed", passed_count, total);
        printf("\n"); FAIL(msg);
    }
}

int test_toneburst(void)
{
    printf("Tone Burst tests:\n");
    test_toneburst_roundtrip();
    test_toneburst_silence();
    test_toneburst_short_reject();
    test_toneburst_multi_rate();
    printf("  Tone Burst: %d/%d passed\n\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
