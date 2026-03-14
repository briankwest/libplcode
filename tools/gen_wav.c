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

int main(void)
{
    mkdir("wav", 0755);

    printf("=== libplcode WAV generator ===\n");
    printf("Rate: %d Hz, Duration: %d sec, Amplitude: %d\n\n",
           SAMPLE_RATE, DURATION_SEC, AMPLITUDE);

    generate_ctcss();
    printf("\n");
    generate_dcs();

    printf("\nDone.\n");
    return 0;
}
