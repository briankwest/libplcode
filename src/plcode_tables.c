#include "plcode_internal.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int plcode_tables_initialized = 0;

/* Sine LUT — filled at init time, read-only thereafter. */
int16_t plcode_sine_lut[PLCODE_SINE_LUT_SIZE];

static void plcode_init_sine_lut(void)
{
    int i;
    for (i = 0; i < PLCODE_SINE_LUT_SIZE; i++) {
        double angle = 2.0 * M_PI * (double)i / (double)PLCODE_SINE_LUT_SIZE;
        double val = sin(angle) * 32767.0;
        if (val > 32767.0) val = 32767.0;
        if (val < -32768.0) val = -32768.0;
        plcode_sine_lut[i] = (int16_t)(val + (val >= 0 ? 0.5 : -0.5));
    }
}

/* 50 standard CTCSS tones, frequency in tenths of Hz. */
const uint16_t plcode_ctcss_tones[PLCODE_CTCSS_NUM_TONES] = {
     670,  693,  719,  744,  770,  797,  825,  854,
     885,  915,  948,  974, 1000, 1035, 1072, 1109,
    1148, 1188, 1230, 1273, 1318, 1365, 1413, 1462,
    1514, 1567, 1598, 1622, 1655, 1679, 1713, 1738,
    1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995,
    2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418,
    2503, 2541
};

/* 106 standard DCS codes stored as C octal literals.
 * Last 2 entries are zero-padding (104 real codes). */
const uint16_t plcode_dcs_codes[PLCODE_DCS_NUM_CODES] = {
    023, 025, 026, 031, 032, 036, 043, 047,
    051, 053, 054, 065, 071, 072, 073, 074,
    0114, 0115, 0116, 0122, 0125, 0131, 0132, 0134,
    0143, 0145, 0152, 0155, 0156, 0162, 0165, 0172,
    0174, 0205, 0212, 0223, 0225, 0226, 0243, 0244,
    0245, 0246, 0251, 0252, 0255, 0261, 0263, 0265,
    0266, 0271, 0274, 0306, 0311, 0315, 0325, 0331,
    0332, 0343, 0346, 0351, 0356, 0364, 0365, 0371,
    0411, 0412, 0413, 0423, 0431, 0432, 0445, 0446,
    0452, 0454, 0455, 0462, 0464, 0465, 0466, 0503,
    0506, 0516, 0523, 0526, 0532, 0546, 0565, 0606,
    0612, 0624, 0627, 0631, 0632, 0654, 0662, 0664,
    0703, 0712, 0723, 0731, 0732, 0734, 0743, 0754,
};

/* 4 DTMF row frequencies in Hz. */
const uint16_t plcode_dtmf_row_freqs[4] = { 697, 770, 852, 941 };

/* 4 DTMF column frequencies in Hz. */
const uint16_t plcode_dtmf_col_freqs[4] = { 1209, 1336, 1477, 1633 };

/* 16 DTMF digit characters, row-major: rows 697/770/852/941 × cols 1209/1336/1477/1633. */
const char plcode_dtmf_digits[PLCODE_DTMF_NUM_DIGITS] = {
    '1', '2', '3', 'A',
    '4', '5', '6', 'B',
    '7', '8', '9', 'C',
    '*', '0', '#', 'D'
};

/* Precomputed Golay codewords for each DCS code — filled at init. */
uint32_t plcode_dcs_codewords[PLCODE_DCS_NUM_CODES];

static void plcode_init_dcs_codewords(void)
{
    int i;
    for (i = 0; i < PLCODE_DCS_NUM_CODES; i++) {
        if (plcode_dcs_codes[i] == 0) {
            plcode_dcs_codewords[i] = 0;
            continue;
        }
        uint16_t code9 = plcode_dcs_codes[i] & 0x1FF;
        uint16_t data12 = (uint16_t)(0x800u | code9);
        plcode_dcs_codewords[i] = plcode_golay_encode(data12);
    }
}

void plcode_tables_init(void)
{
    if (plcode_tables_initialized) return;
    plcode_init_sine_lut();
    plcode_init_dcs_codewords();
    plcode_tables_initialized = 1;
}

uint16_t plcode_ctcss_tone_freq_x10(int index)
{
    if (index < 0 || index >= PLCODE_CTCSS_NUM_TONES)
        return 0;
    return plcode_ctcss_tones[index];
}

int plcode_ctcss_tone_index(uint16_t freq_x10)
{
    int i;
    for (i = 0; i < PLCODE_CTCSS_NUM_TONES; i++) {
        if (plcode_ctcss_tones[i] == freq_x10)
            return i;
    }
    return -1;
}

uint16_t plcode_dcs_code_number(int index)
{
    if (index < 0 || index >= PLCODE_DCS_NUM_CODES)
        return 0;
    return plcode_dcs_codes[index];
}

int plcode_dcs_code_index(uint16_t octal_code)
{
    int i;
    for (i = 0; i < PLCODE_DCS_NUM_CODES; i++) {
        if (plcode_dcs_codes[i] == octal_code)
            return i;
    }
    return -1;
}

char plcode_dtmf_digit_char(int index)
{
    if (index < 0 || index >= PLCODE_DTMF_NUM_DIGITS)
        return '\0';
    return plcode_dtmf_digits[index];
}

int plcode_dtmf_digit_index(char digit)
{
    int i;
    for (i = 0; i < PLCODE_DTMF_NUM_DIGITS; i++) {
        if (plcode_dtmf_digits[i] == digit)
            return i;
    }
    return -1;
}

/* 37 CW ID characters: A-Z, 0-9, / */
const char plcode_cwid_chars[PLCODE_CWID_NUM_CHARS] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    '0','1','2','3','4','5','6','7','8','9','/'
};

const char * const plcode_cwid_patterns[PLCODE_CWID_NUM_CHARS] = {
    ".-",   "-...", "-.-.", "-..",  ".",    "..-.", "--.",  "....",
    "..",   ".---", "-.-",  ".-..", "--",   "-.",   "---",  ".--.",
    "--.-", ".-.",  "...",  "-",   "..-",  "...-", ".--",  "-..-",
    "-.--", "--..",
    "-----", ".----", "..---", "...--", "....-", ".....", "-....",
    "--...", "---..", "----.",
    "-..-."
};

const char *plcode_cwid_morse(char ch)
{
    int i;
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    for (i = 0; i < PLCODE_CWID_NUM_CHARS; i++) {
        if (plcode_cwid_chars[i] == ch)
            return plcode_cwid_patterns[i];
    }
    return NULL;
}

char plcode_cwid_decode(const char *pattern)
{
    int i;
    if (!pattern) return '\0';
    for (i = 0; i < PLCODE_CWID_NUM_CHARS; i++) {
        if (strcmp(plcode_cwid_patterns[i], pattern) == 0)
            return plcode_cwid_chars[i];
    }
    return '\0';
}
