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

static int mdc1200_roundtrip(int rate, uint8_t op, uint8_t arg, uint16_t unit_id)
{
    plcode_mdc1200_enc_t *enc = NULL;
    plcode_mdc1200_dec_t *dec = NULL;
    int16_t *buf;
    int total_samples, ok;

    if (plcode_mdc1200_enc_create(&enc, rate, op, arg, unit_id, 5000) != PLCODE_OK)
        return 0;
    if (plcode_mdc1200_dec_create(&dec, rate) != PLCODE_OK) {
        plcode_mdc1200_enc_destroy(enc);
        return 0;
    }

    total_samples = rate * 1; /* 1 second is plenty for a ~93ms packet */
    buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) {
        plcode_mdc1200_enc_destroy(enc);
        plcode_mdc1200_dec_destroy(dec);
        return 0;
    }

    plcode_mdc1200_enc_process(enc, buf, (size_t)total_samples);

    plcode_mdc1200_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_mdc1200_dec_process(dec, buf, (size_t)total_samples, &result);

    ok = (result.detected &&
          result.op == op &&
          result.arg == arg &&
          result.unit_id == unit_id);

    free(buf);
    plcode_mdc1200_enc_destroy(enc);
    plcode_mdc1200_dec_destroy(dec);
    return ok;
}

void test_mdc1200_ptt_id(void)
{
    TEST("MDC-1200 PTT ID round-trip @ 8000 Hz");
    if (mdc1200_roundtrip(8000, PLCODE_MDC1200_OP_PTT_PRE, 0x00, 0x1234)) {
        PASS();
    } else {
        FAIL("packet not decoded correctly");
    }
}

void test_mdc1200_emergency(void)
{
    TEST("MDC-1200 emergency round-trip @ 8000 Hz");
    if (mdc1200_roundtrip(8000, PLCODE_MDC1200_OP_EMERG, 0x42, 0xABCD)) {
        PASS();
    } else {
        FAIL("packet not decoded correctly");
    }
}

void test_mdc1200_silence(void)
{
    int rate = 8000;
    TEST("MDC-1200 no false detection on silence");

    plcode_mdc1200_dec_t *dec = NULL;
    if (plcode_mdc1200_dec_create(&dec, rate) != PLCODE_OK) {
        FAIL("create"); return;
    }

    int total_samples = rate * 1;
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_mdc1200_dec_destroy(dec); return; }

    plcode_mdc1200_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_mdc1200_dec_process(dec, buf, (size_t)total_samples, &result);

    if (!result.detected) { PASS(); } else { FAIL("false detect"); }

    free(buf);
    plcode_mdc1200_dec_destroy(dec);
}

void test_mdc1200_multi_rate(void)
{
    static const int rates[] = { 8000, 16000, 32000, 48000 };
    int r, total = 0, passed_count = 0;
    char testname[80];

    for (r = 0; r < 4; r++) {
        total++;
        if (mdc1200_roundtrip(rates[r], PLCODE_MDC1200_OP_PTT_POST, 0x00, 0x5678)) {
            passed_count++;
        } else {
            printf("\n    FAIL: @ %d Hz", rates[r]);
        }
    }

    snprintf(testname, sizeof(testname), "MDC-1200 x all sample rates (%d)", total);
    TEST(testname);
    if (passed_count == total) { PASS(); }
    else {
        char msg[80];
        snprintf(msg, sizeof(msg), "%d/%d passed", passed_count, total);
        printf("\n"); FAIL(msg);
    }
}

int test_mdc1200(void)
{
    printf("MDC-1200 tests:\n");
    test_mdc1200_ptt_id();
    test_mdc1200_emergency();
    test_mdc1200_silence();
    test_mdc1200_multi_rate();
    printf("  MDC-1200: %d/%d passed\n\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
