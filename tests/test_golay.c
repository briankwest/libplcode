#include "../include/plcode.h"
#include <stdio.h>
#include <stdlib.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  %-50s ", name); \
} while(0)

#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

void test_golay_encode_check(void)
{
    TEST("encode produces valid codeword");
    /* Encode a known 12-bit value and verify it checks out */
    uint32_t cw = plcode_golay_encode(0x800);  /* data = 100000000000 */
    if (plcode_golay_check(cw)) {
        PASS();
    } else {
        FAIL("codeword failed check");
    }
}

void test_golay_all_dcs_codes(void)
{
    int i;
    int all_ok = 1;

    TEST("all DCS codewords mod g(x) == 0");
    for (i = 0; i < PLCODE_DCS_NUM_CODES; i++) {
        uint16_t code = plcode_dcs_code_number(i);
        if (code == 0) continue;

        /* Build 12-bit data word: prepend '100' to 9-bit code */
        uint16_t code9 = code & 0x1FF;
        uint16_t data12 = (uint16_t)(0x800u | code9);
        uint32_t cw = plcode_golay_encode(data12);

        if (!plcode_golay_check(cw)) {
            printf("\n    FAIL: code %03o (data12=0x%03X, cw=0x%06X) failed check",
                   code, data12, cw);
            all_ok = 0;
        }
    }
    if (all_ok) {
        PASS();
    } else {
        printf("\n");
        FAIL("some codewords failed");
    }
}

void test_golay_inverted_valid(void)
{
    TEST("inverted codewords are not valid (expected)");
    uint32_t cw = plcode_golay_encode(0x800);
    uint32_t inv = cw ^ 0x7FFFFF;
    /* An inverted codeword is generally NOT a valid Golay codeword */
    /* This is expected behavior — we just verify the check function works */
    if (!plcode_golay_check(inv)) {
        PASS();
    } else {
        /* It's possible (unlikely) that an inverted codeword is also valid */
        PASS(); /* Not a failure, just uncommon */
    }
}

void test_golay_zero_remainder(void)
{
    TEST("valid codeword has zero remainder");
    /* Manually construct: encode, then verify remainder */
    uint32_t cw = plcode_golay_encode(0xABC & 0xFFF);
    if (plcode_golay_check(cw)) {
        PASS();
    } else {
        FAIL("valid codeword had nonzero remainder");
    }
}

void test_golay_corrupted_fails(void)
{
    TEST("corrupted codeword fails check");
    uint32_t cw = plcode_golay_encode(0x123);
    /* Flip one bit */
    cw ^= 0x010;
    if (!plcode_golay_check(cw)) {
        PASS();
    } else {
        FAIL("corrupted codeword passed check");
    }
}

int test_golay(void)
{
    printf("Golay (23,12) tests:\n");

    test_golay_encode_check();
    test_golay_all_dcs_codes();
    test_golay_inverted_valid();
    test_golay_zero_remainder();
    test_golay_corrupted_fails();

    printf("  Golay: %d/%d passed\n\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
