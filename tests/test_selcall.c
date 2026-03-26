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

static int selcall_roundtrip(int rate, plcode_selcall_std_t std,
                              const char *address)
{
    plcode_selcall_enc_t *enc = NULL;
    plcode_selcall_dec_t *dec = NULL;
    int16_t *buf;
    int total_samples, ok;

    if (plcode_selcall_enc_create(&enc, rate, std, address, 3000) != PLCODE_OK)
        return 0;
    if (plcode_selcall_dec_create(&dec, rate, std) != PLCODE_OK) {
        plcode_selcall_enc_destroy(enc);
        return 0;
    }

    total_samples = rate * 3;
    buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) {
        plcode_selcall_enc_destroy(enc);
        plcode_selcall_dec_destroy(dec);
        return 0;
    }

    plcode_selcall_enc_process(enc, buf, (size_t)total_samples);

    plcode_selcall_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_selcall_dec_process(dec, buf, (size_t)total_samples, &result);

    ok = (result.detected && strcmp(result.address, address) == 0);

    free(buf);
    plcode_selcall_enc_destroy(enc);
    plcode_selcall_dec_destroy(dec);
    return ok;
}

void test_selcall_zvei1(void)
{
    TEST("ZVEI1 selcall '12345' @ 8000 Hz");
    if (selcall_roundtrip(8000, PLCODE_SELCALL_ZVEI1, "12345")) {
        PASS();
    } else {
        FAIL("decoded address did not match");
    }
}

void test_selcall_ccir(void)
{
    TEST("CCIR selcall '67890' @ 8000 Hz");
    if (selcall_roundtrip(8000, PLCODE_SELCALL_CCIR, "67890")) {
        PASS();
    } else {
        FAIL("decoded address did not match");
    }
}

void test_selcall_eia(void)
{
    TEST("EIA selcall '54321' @ 16000 Hz");
    if (selcall_roundtrip(16000, PLCODE_SELCALL_EIA, "54321")) {
        PASS();
    } else {
        FAIL("decoded address did not match");
    }
}

void test_selcall_silence(void)
{
    int rate = 8000;
    TEST("selcall no false detection on silence");

    plcode_selcall_dec_t *dec = NULL;
    if (plcode_selcall_dec_create(&dec, rate, PLCODE_SELCALL_ZVEI1) != PLCODE_OK) {
        FAIL("create"); return;
    }

    int total_samples = rate * 3;
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_selcall_dec_destroy(dec); return; }

    plcode_selcall_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_selcall_dec_process(dec, buf, (size_t)total_samples, &result);

    if (!result.detected) { PASS(); } else { FAIL("false detect"); }

    free(buf);
    plcode_selcall_dec_destroy(dec);
}

void test_selcall_multi_rate(void)
{
    static const int rates[] = { 8000, 16000, 32000, 48000 };
    int r, total = 0, passed_count = 0;
    char testname[80];

    for (r = 0; r < 4; r++) {
        total++;
        if (selcall_roundtrip(rates[r], PLCODE_SELCALL_ZVEI1, "12345")) {
            passed_count++;
        } else {
            printf("\n    FAIL: ZVEI1 @ %d Hz", rates[r]);
        }
    }

    snprintf(testname, sizeof(testname), "ZVEI1 x all sample rates (%d)", total);
    TEST(testname);
    if (passed_count == total) { PASS(); }
    else {
        char msg[80];
        snprintf(msg, sizeof(msg), "%d/%d passed", passed_count, total);
        printf("\n"); FAIL(msg);
    }
}

int test_selcall(void)
{
    printf("Selcall tests:\n");
    test_selcall_zvei1();
    test_selcall_ccir();
    test_selcall_eia();
    test_selcall_silence();
    test_selcall_multi_rate();
    printf("  Selcall: %d/%d passed\n\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
