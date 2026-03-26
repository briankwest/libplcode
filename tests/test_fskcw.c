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

#define MARK_FREQ  800
#define SPACE_FREQ 600

static int fskcw_roundtrip(int rate, const char *message,
                            int mark_freq, int space_freq, int wpm)
{
    plcode_fskcw_enc_t *enc = NULL;
    plcode_fskcw_dec_t *dec = NULL;
    int16_t *buf;
    const char *decoded;
    int total_samples, ok;

    if (plcode_fskcw_enc_create(&enc, rate, message, mark_freq, space_freq,
                                 wpm, 3000) != PLCODE_OK)
        return 0;
    if (plcode_fskcw_dec_create(&dec, rate, mark_freq, space_freq,
                                 wpm) != PLCODE_OK) {
        plcode_fskcw_enc_destroy(enc);
        return 0;
    }

    total_samples = rate * 5;
    buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) {
        plcode_fskcw_enc_destroy(enc);
        plcode_fskcw_dec_destroy(dec);
        return 0;
    }

    plcode_fskcw_enc_process(enc, buf, (size_t)total_samples);

    plcode_cwid_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_fskcw_dec_process(dec, buf, (size_t)total_samples, &result);

    decoded = plcode_fskcw_dec_message(dec);
    ok = (strcmp(decoded, message) == 0);

    free(buf);
    plcode_fskcw_enc_destroy(enc);
    plcode_fskcw_dec_destroy(dec);

    return ok;
}

void test_fskcw_single_char(void)
{
    TEST("FSK CW encode/decode single char 'A' @ 8000 Hz");
    if (fskcw_roundtrip(8000, "A", MARK_FREQ, SPACE_FREQ, 20)) {
        PASS();
    } else {
        FAIL("decoded message did not match");
    }
}

void test_fskcw_callsign(void)
{
    TEST("FSK CW encode/decode callsign 'W1AW' @ 8000 Hz");
    if (fskcw_roundtrip(8000, "W1AW", MARK_FREQ, SPACE_FREQ, 20)) {
        PASS();
    } else {
        FAIL("decoded message did not match");
    }
}

void test_fskcw_all_characters(void)
{
    const char *all_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/";
    int len = (int)strlen(all_chars);
    int i, total = 0, passed_count = 0;
    char testname[80];

    for (i = 0; i < len; i++) {
        char msg[2];
        msg[0] = all_chars[i];
        msg[1] = '\0';
        total++;
        if (fskcw_roundtrip(8000, msg, MARK_FREQ, SPACE_FREQ, 20)) {
            passed_count++;
        } else {
            printf("\n    FAIL: char '%c'", all_chars[i]);
        }
    }

    snprintf(testname, sizeof(testname), "FSK CW all chars individually (%d chars)", total);
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

void test_fskcw_silence_rejection(void)
{
    int rate = 8000;
    TEST("FSK CW no false detection on silence");

    plcode_fskcw_dec_t *dec = NULL;
    if (plcode_fskcw_dec_create(&dec, rate, MARK_FREQ, SPACE_FREQ, 20) != PLCODE_OK) {
        FAIL("create failed");
        return;
    }

    int total_samples = rate * 3;
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_fskcw_dec_destroy(dec); return; }

    plcode_cwid_result_t result;
    memset(&result, 0, sizeof(result));
    plcode_fskcw_dec_process(dec, buf, (size_t)total_samples, &result);

    const char *msg = plcode_fskcw_dec_message(dec);
    if (strlen(msg) == 0) {
        PASS();
    } else {
        FAIL("false detection on silence");
    }

    free(buf);
    plcode_fskcw_dec_destroy(dec);
}

void test_fskcw_multi_rate(void)
{
    int r;
    int total = 0, passed_count = 0;
    char testname[80];

    for (r = 0; r < NUM_RATES; r++) {
        total++;
        if (fskcw_roundtrip(sample_rates[r], "CQ", MARK_FREQ, SPACE_FREQ, 20)) {
            passed_count++;
        } else {
            printf("\n    FAIL: 'CQ' @ %d Hz", sample_rates[r]);
        }
    }

    snprintf(testname, sizeof(testname), "FSK CW 'CQ' x all sample rates (%d)", total);
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

int test_fskcw(void)
{
    printf("FSK CW tests:\n");

    test_fskcw_single_char();
    test_fskcw_callsign();
    test_fskcw_all_characters();
    test_fskcw_silence_rejection();
    test_fskcw_multi_rate();

    printf("  FSK CW: %d/%d passed\n\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
