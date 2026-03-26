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

void test_courtesy_basic(void)
{
    int rate = 8000;
    TEST("courtesy tone basic generation");

    plcode_courtesy_tone_t tones[] = {
        { 800,  100, 3000 },  /* 800 Hz for 100ms */
        {   0,   50,    0 },  /* 50ms silence */
        { 1000, 100, 3000 },  /* 1000 Hz for 100ms */
    };

    plcode_courtesy_enc_t *enc = NULL;
    if (plcode_courtesy_enc_create(&enc, rate, tones, 3) != PLCODE_OK) {
        FAIL("create"); return;
    }

    int total_samples = rate * 1;
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_courtesy_enc_destroy(enc); return; }

    plcode_courtesy_enc_process(enc, buf, (size_t)total_samples);

    /* Verify encoder completed */
    if (!plcode_courtesy_enc_complete(enc)) {
        FAIL("not complete"); goto cleanup;
    }

    /* Verify audio was generated (non-zero samples in first 100ms) */
    {
        int i, has_audio = 0;
        for (i = 0; i < rate / 10 && !has_audio; i++) {
            if (buf[i] != 0) has_audio = 1;
        }
        if (has_audio) { PASS(); } else { FAIL("no audio generated"); }
    }

cleanup:
    free(buf);
    plcode_courtesy_enc_destroy(enc);
}

void test_courtesy_silence_gap(void)
{
    int rate = 8000;
    TEST("courtesy tone silence gap is silent");

    plcode_courtesy_tone_t tones[] = {
        { 800,  50, 3000 },
        {   0, 200,    0 },  /* 200ms silence */
        { 800,  50, 3000 },
    };

    plcode_courtesy_enc_t *enc = NULL;
    if (plcode_courtesy_enc_create(&enc, rate, tones, 3) != PLCODE_OK) {
        FAIL("create"); return;
    }

    int total_samples = rate * 1;
    int16_t *buf = (int16_t *)calloc((size_t)total_samples, sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_courtesy_enc_destroy(enc); return; }

    plcode_courtesy_enc_process(enc, buf, (size_t)total_samples);

    /* Check that the silence region (50ms to 250ms) has zero samples */
    {
        int start = rate * 50 / 1000 + 10;  /* a few samples into the gap */
        int end = rate * 250 / 1000 - 10;
        int i, all_silent = 1;
        for (i = start; i < end; i++) {
            if (buf[i] != 0) { all_silent = 0; break; }
        }
        if (all_silent) { PASS(); } else { FAIL("gap not silent"); }
    }

    free(buf);
    plcode_courtesy_enc_destroy(enc);
}

void test_courtesy_complete_flag(void)
{
    int rate = 8000;
    TEST("courtesy tone complete flag");

    plcode_courtesy_tone_t tones[] = {
        { 1000, 100, 3000 },
    };

    plcode_courtesy_enc_t *enc = NULL;
    if (plcode_courtesy_enc_create(&enc, rate, tones, 1) != PLCODE_OK) {
        FAIL("create"); return;
    }

    /* Process only half the tone */
    int half = rate * 50 / 1000;
    int16_t *buf = (int16_t *)calloc((size_t)(rate), sizeof(int16_t));
    if (!buf) { FAIL("alloc"); plcode_courtesy_enc_destroy(enc); return; }

    plcode_courtesy_enc_process(enc, buf, (size_t)half);
    int mid = plcode_courtesy_enc_complete(enc);

    plcode_courtesy_enc_process(enc, buf + half, (size_t)(rate - half));
    int end = plcode_courtesy_enc_complete(enc);

    if (!mid && end) { PASS(); }
    else {
        char msg[80];
        snprintf(msg, sizeof(msg), "mid=%d end=%d (expected 0, 1)", mid, end);
        FAIL(msg);
    }

    free(buf);
    plcode_courtesy_enc_destroy(enc);
}

int test_courtesy(void)
{
    printf("Courtesy Tone tests:\n");
    test_courtesy_basic();
    test_courtesy_silence_gap();
    test_courtesy_complete_flag();
    printf("  Courtesy: %d/%d passed\n\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
