#include "plcode_internal.h"

/*
 * Golay (23,12) codec using generator polynomial g(x) = 0xC75.
 * g(x) = x^11 + x^10 + x^6 + x^5 + x^4 + x^2 + 1
 *
 * Encoding: 12 data bits in upper position, 11 parity bits appended.
 * data12 shifted left by 11, then divide by g(x) to get remainder.
 */

uint32_t plcode_golay_encode(uint16_t data12)
{
    /* Place data bits in upper 12 bits of a 23-bit word */
    uint32_t codeword = (uint32_t)(data12 & 0xFFF) << 11;
    uint32_t remainder = codeword;
    int i;

    /* Polynomial long division: divide 23-bit word by g(x) */
    for (i = 11; i >= 0; i--) {
        if (remainder & ((uint32_t)1 << (i + 11))) {
            remainder ^= ((uint32_t)PLCODE_GOLAY_POLY << i);
        }
    }

    /* Codeword = data bits | parity bits */
    return codeword | (remainder & 0x7FF);
}

int plcode_golay_check(uint32_t codeword23)
{
    uint32_t remainder = codeword23 & 0x7FFFFF; /* mask to 23 bits */
    int i;

    /* Divide by g(x) — if remainder is 0, it's a valid codeword */
    for (i = 11; i >= 0; i--) {
        if (remainder & ((uint32_t)1 << (i + 11))) {
            remainder ^= ((uint32_t)PLCODE_GOLAY_POLY << i);
        }
    }

    return (remainder == 0) ? 1 : 0;
}
