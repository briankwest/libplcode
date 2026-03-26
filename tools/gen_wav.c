/*
 * gen_wav.c — Generate WAV files for all CTCSS tones and DCS codes
 *
 * Outputs to wav/ directory:
 *   ctcss_XXX_X.wav  (e.g., ctcss_100_0.wav for 100.0 Hz)
 *   dcs_NNN.wav      (e.g., dcs_023.wav)
 *   dcs_NNN_inv.wav  (e.g., dcs_023_inv.wav)
 *
 * 16-bit mono PCM WAV, 8000 Hz, 3 seconds each.
 */

#include "../include/plcode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#define SAMPLE_RATE 8000
#define DURATION_SEC 3
#define NUM_SAMPLES (SAMPLE_RATE * DURATION_SEC)
#define AMPLITUDE 3000

/* Write a 16-bit mono PCM WAV file */
static int write_wav(const char *path, const int16_t *samples, int num_samples, int rate)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "Cannot open %s for writing\n", path);
        return -1;
    }

    uint32_t data_size = (uint32_t)(num_samples * 2);
    uint32_t file_size = 36 + data_size;
    uint16_t num_channels = 1;
    uint16_t bits_per_sample = 16;
    uint32_t byte_rate = (uint32_t)(rate * num_channels * bits_per_sample / 8);
    uint16_t block_align = (uint16_t)(num_channels * bits_per_sample / 8);
    uint32_t sample_rate = (uint32_t)rate;

    /* RIFF header */
    fwrite("RIFF", 1, 4, f);
    fwrite(&file_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);

    /* fmt chunk */
    fwrite("fmt ", 1, 4, f);
    uint32_t fmt_size = 16;
    fwrite(&fmt_size, 4, 1, f);
    uint16_t audio_format = 1; /* PCM */
    fwrite(&audio_format, 2, 1, f);
    fwrite(&num_channels, 2, 1, f);
    fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits_per_sample, 2, 1, f);

    /* data chunk */
    fwrite("data", 1, 4, f);
    fwrite(&data_size, 4, 1, f);
    fwrite(samples, 2, (size_t)num_samples, f);

    fclose(f);
    return 0;
}

static void generate_ctcss(void)
{
    int i;
    int16_t *buf = (int16_t *)calloc(NUM_SAMPLES, sizeof(int16_t));
    if (!buf) { fprintf(stderr, "alloc failed\n"); return; }

    printf("Generating CTCSS tones...\n");

    for (i = 0; i < PLCODE_CTCSS_NUM_TONES; i++) {
        uint16_t freq = plcode_ctcss_tone_freq_x10(i);
        if (freq == 0) continue;

        plcode_ctcss_enc_t *enc = NULL;
        if (plcode_ctcss_enc_create(&enc, SAMPLE_RATE, freq, AMPLITUDE) != PLCODE_OK) {
            fprintf(stderr, "  Failed to create encoder for %.1f Hz\n", freq / 10.0);
            continue;
        }

        memset(buf, 0, NUM_SAMPLES * sizeof(int16_t));
        plcode_ctcss_enc_process(enc, buf, NUM_SAMPLES);

        char filename[64];
        snprintf(filename, sizeof(filename), "wav/ctcss_%03d_%01d.wav",
                 freq / 10, freq % 10);

        if (write_wav(filename, buf, NUM_SAMPLES, SAMPLE_RATE) == 0) {
            printf("  %s  (%.1f Hz)\n", filename, freq / 10.0);
        }

        plcode_ctcss_enc_destroy(enc);
    }

    free(buf);
}

static void generate_dcs(void)
{
    int i;
    int16_t *buf = (int16_t *)calloc(NUM_SAMPLES, sizeof(int16_t));
    if (!buf) { fprintf(stderr, "alloc failed\n"); return; }

    printf("Generating DCS codes (normal)...\n");

    for (i = 0; i < PLCODE_DCS_NUM_CODES; i++) {
        uint16_t code = plcode_dcs_code_number(i);
        if (code == 0) continue;

        /* Normal */
        plcode_dcs_enc_t *enc = NULL;
        if (plcode_dcs_enc_create(&enc, SAMPLE_RATE, code, 0, AMPLITUDE) != PLCODE_OK) {
            fprintf(stderr, "  Failed to create encoder for DCS %03o\n", code);
            continue;
        }

        memset(buf, 0, NUM_SAMPLES * sizeof(int16_t));
        plcode_dcs_enc_process(enc, buf, NUM_SAMPLES);

        char filename[64];
        snprintf(filename, sizeof(filename), "wav/dcs_%03o.wav", code);

        if (write_wav(filename, buf, NUM_SAMPLES, SAMPLE_RATE) == 0) {
            printf("  %s  (DCS %03o)\n", filename, code);
        }

        plcode_dcs_enc_destroy(enc);

        /* Inverted */
        if (plcode_dcs_enc_create(&enc, SAMPLE_RATE, code, 1, AMPLITUDE) != PLCODE_OK) {
            continue;
        }

        memset(buf, 0, NUM_SAMPLES * sizeof(int16_t));
        plcode_dcs_enc_process(enc, buf, NUM_SAMPLES);

        snprintf(filename, sizeof(filename), "wav/dcs_%03o_inv.wav", code);

        if (write_wav(filename, buf, NUM_SAMPLES, SAMPLE_RATE) == 0) {
            printf("  %s  (DCS %03o inverted)\n", filename, code);
        }

        plcode_dcs_enc_destroy(enc);
    }

    free(buf);
}

static void generate_dtmf(void)
{
    int i;
    /* DTMF tones are short — generate 500ms */
    int dtmf_samples = SAMPLE_RATE / 2;
    int16_t *buf = (int16_t *)calloc((size_t)dtmf_samples, sizeof(int16_t));
    if (!buf) { fprintf(stderr, "alloc failed\n"); return; }

    printf("Generating DTMF tones...\n");

    for (i = 0; i < PLCODE_DTMF_NUM_DIGITS; i++) {
        char digit = plcode_dtmf_digit_char(i);
        if (digit == '\0') continue;

        plcode_dtmf_enc_t *enc = NULL;
        if (plcode_dtmf_enc_create(&enc, SAMPLE_RATE, digit, AMPLITUDE) != PLCODE_OK) {
            fprintf(stderr, "  Failed to create encoder for DTMF '%c'\n", digit);
            continue;
        }

        memset(buf, 0, (size_t)dtmf_samples * sizeof(int16_t));
        plcode_dtmf_enc_process(enc, buf, (size_t)dtmf_samples);

        char filename[64];
        /* Use 'star' and 'hash' for filesystem-safe names */
        if (digit == '*')
            snprintf(filename, sizeof(filename), "wav/dtmf_star.wav");
        else if (digit == '#')
            snprintf(filename, sizeof(filename), "wav/dtmf_hash.wav");
        else
            snprintf(filename, sizeof(filename), "wav/dtmf_%c.wav", digit);

        if (write_wav(filename, buf, dtmf_samples, SAMPLE_RATE) == 0) {
            printf("  %s  (DTMF '%c')\n", filename, digit);
        }

        plcode_dtmf_enc_destroy(enc);
    }

    free(buf);
}

int main(void)
{
    mkdir("wav", 0755);

    printf("=== libplcode WAV generator ===\n");
    printf("Rate: %d Hz, Duration: %d sec, Amplitude: %d\n\n",
           SAMPLE_RATE, DURATION_SEC, AMPLITUDE);

    generate_ctcss();
    printf("\n");
    generate_dcs();
    printf("\n");
    generate_dtmf();
    printf("\n");

    /* CW ID — 5 seconds at 800 Hz tone, 20 WPM */
    {
        int cwid_samples = SAMPLE_RATE * 5;
        int16_t *buf = (int16_t *)calloc((size_t)cwid_samples, sizeof(int16_t));
        if (buf) {
            plcode_cwid_enc_t *enc = NULL;
            printf("Generating CW ID...\n");
            if (plcode_cwid_enc_create(&enc, SAMPLE_RATE, "W1AW", 800, 20,
                                        AMPLITUDE) == PLCODE_OK) {
                plcode_cwid_enc_process(enc, buf, (size_t)cwid_samples);
                if (write_wav("wav/cwid_W1AW.wav", buf, cwid_samples,
                              SAMPLE_RATE) == 0) {
                    printf("  wav/cwid_W1AW.wav  (CW ID 'W1AW')\n");
                }
                plcode_cwid_enc_destroy(enc);
            }
            free(buf);
        }
    }

    /* MCW — 5 seconds at 800 Hz tone, 20 WPM */
    {
        int mcw_samples = SAMPLE_RATE * 5;
        int16_t *buf = (int16_t *)calloc((size_t)mcw_samples, sizeof(int16_t));
        if (buf) {
            plcode_mcw_enc_t *enc = NULL;
            printf("Generating MCW...\n");
            if (plcode_mcw_enc_create(&enc, SAMPLE_RATE, "W1AW", 800, 20,
                                       AMPLITUDE) == PLCODE_OK) {
                plcode_mcw_enc_process(enc, buf, (size_t)mcw_samples);
                if (write_wav("wav/mcw_W1AW.wav", buf, mcw_samples,
                              SAMPLE_RATE) == 0) {
                    printf("  wav/mcw_W1AW.wav  (MCW 'W1AW')\n");
                }
                plcode_mcw_enc_destroy(enc);
            }
            free(buf);
        }
    }

    /* FSK CW — 5 seconds, mark 800 Hz / space 600 Hz, 20 WPM */
    {
        int fsk_samples = SAMPLE_RATE * 5;
        int16_t *buf = (int16_t *)calloc((size_t)fsk_samples, sizeof(int16_t));
        if (buf) {
            plcode_fskcw_enc_t *enc = NULL;
            printf("Generating FSK CW...\n");
            if (plcode_fskcw_enc_create(&enc, SAMPLE_RATE, "W1AW", 800, 600, 20,
                                          AMPLITUDE) == PLCODE_OK) {
                plcode_fskcw_enc_process(enc, buf, (size_t)fsk_samples);
                if (write_wav("wav/fskcw_W1AW.wav", buf, fsk_samples,
                              SAMPLE_RATE) == 0) {
                    printf("  wav/fskcw_W1AW.wav  (FSK CW 'W1AW')\n");
                }
                plcode_fskcw_enc_destroy(enc);
            }
            free(buf);
        }
    }

    /* Two-Tone Paging — tone A 330.5 Hz 1s, tone B 433.7 Hz 3s */
    {
        int tt_samples = SAMPLE_RATE * 5;
        int16_t *buf = (int16_t *)calloc((size_t)tt_samples, sizeof(int16_t));
        if (buf) {
            plcode_twotone_enc_t *enc = NULL;
            printf("Generating Two-Tone Page...\n");
            if (plcode_twotone_enc_create(&enc, SAMPLE_RATE,
                    plcode_twotone_freq_x10(0), plcode_twotone_freq_x10(5),
                    1000, 3000, AMPLITUDE) == PLCODE_OK) {
                plcode_twotone_enc_process(enc, buf, (size_t)tt_samples);
                if (write_wav("wav/twotone_page.wav", buf, tt_samples,
                              SAMPLE_RATE) == 0) {
                    printf("  wav/twotone_page.wav\n");
                }
                plcode_twotone_enc_destroy(enc);
            }
            free(buf);
        }
    }

    /* Five-Tone Selcall ZVEI1 "12345" */
    {
        int sc_samples = SAMPLE_RATE * 2;
        int16_t *buf = (int16_t *)calloc((size_t)sc_samples, sizeof(int16_t));
        if (buf) {
            plcode_selcall_enc_t *enc = NULL;
            printf("Generating Selcall...\n");
            if (plcode_selcall_enc_create(&enc, SAMPLE_RATE,
                    PLCODE_SELCALL_ZVEI1, "12345", AMPLITUDE) == PLCODE_OK) {
                plcode_selcall_enc_process(enc, buf, (size_t)sc_samples);
                if (write_wav("wav/selcall_12345.wav", buf, sc_samples,
                              SAMPLE_RATE) == 0) {
                    printf("  wav/selcall_12345.wav  (ZVEI1 '12345')\n");
                }
                plcode_selcall_enc_destroy(enc);
            }
            free(buf);
        }
    }

    /* 1750 Hz Tone Burst — 500ms */
    {
        int tb_samples = SAMPLE_RATE * 1;
        int16_t *buf = (int16_t *)calloc((size_t)tb_samples, sizeof(int16_t));
        if (buf) {
            plcode_toneburst_enc_t *enc = NULL;
            printf("Generating Tone Burst...\n");
            if (plcode_toneburst_enc_create(&enc, SAMPLE_RATE, 1750, 500,
                                              AMPLITUDE) == PLCODE_OK) {
                plcode_toneburst_enc_process(enc, buf, (size_t)tb_samples);
                if (write_wav("wav/toneburst_1750.wav", buf, tb_samples,
                              SAMPLE_RATE) == 0) {
                    printf("  wav/toneburst_1750.wav  (1750 Hz, 500ms)\n");
                }
                plcode_toneburst_enc_destroy(enc);
            }
            free(buf);
        }
    }

    /* MDC-1200 PTT ID */
    {
        int mdc_samples = SAMPLE_RATE * 1;
        int16_t *buf = (int16_t *)calloc((size_t)mdc_samples, sizeof(int16_t));
        if (buf) {
            plcode_mdc1200_enc_t *enc = NULL;
            printf("Generating MDC-1200...\n");
            if (plcode_mdc1200_enc_create(&enc, SAMPLE_RATE,
                    PLCODE_MDC1200_OP_PTT_PRE, 0x00, 0x1234,
                    AMPLITUDE) == PLCODE_OK) {
                plcode_mdc1200_enc_process(enc, buf, (size_t)mdc_samples);
                if (write_wav("wav/mdc1200_ptt.wav", buf, mdc_samples,
                              SAMPLE_RATE) == 0) {
                    printf("  wav/mdc1200_ptt.wav  (PTT ID 0x1234)\n");
                }
                plcode_mdc1200_enc_destroy(enc);
            }
            free(buf);
        }
    }

    /* Courtesy Tone — 3-tone beep */
    {
        plcode_courtesy_tone_t ct[] = {
            { 800,  80, AMPLITUDE },
            {   0,  40,         0 },
            { 1000, 80, AMPLITUDE },
            {   0,  40,         0 },
            { 1200, 80, AMPLITUDE },
        };
        int ct_samples = SAMPLE_RATE * 1;
        int16_t *buf = (int16_t *)calloc((size_t)ct_samples, sizeof(int16_t));
        if (buf) {
            plcode_courtesy_enc_t *enc = NULL;
            printf("Generating Courtesy Tone...\n");
            if (plcode_courtesy_enc_create(&enc, SAMPLE_RATE, ct, 5) == PLCODE_OK) {
                plcode_courtesy_enc_process(enc, buf, (size_t)ct_samples);
                if (write_wav("wav/courtesy_tone.wav", buf, ct_samples,
                              SAMPLE_RATE) == 0) {
                    printf("  wav/courtesy_tone.wav  (3-tone beep)\n");
                }
                plcode_courtesy_enc_destroy(enc);
            }
            free(buf);
        }
    }

    printf("\nDone.\n");
    return 0;
}
