/*
 * decode_dtmf_wav.c — Read a 16-bit mono PCM WAV and decode DTMF digits
 *
 * Usage: decode_dtmf_wav <file.wav>
 */

#include "../include/plcode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Minimal WAV reader — 16-bit mono PCM only */
static int read_wav(const char *path, int16_t **out_samples,
                    uint32_t *out_num_samples, uint32_t *out_rate)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open %s\n", path);
        return -1;
    }

    /* RIFF header */
    char riff[4];
    uint32_t file_size;
    char wave[4];
    fread(riff, 1, 4, f);
    fread(&file_size, 4, 1, f);
    fread(wave, 1, 4, f);

    if (memcmp(riff, "RIFF", 4) || memcmp(wave, "WAVE", 4)) {
        fprintf(stderr, "Not a WAV file\n");
        fclose(f);
        return -1;
    }

    uint16_t audio_format = 0, num_channels = 0, bits_per_sample = 0;
    uint32_t sample_rate = 0, data_size = 0;
    int found_fmt = 0, found_data = 0;

    /* Parse chunks */
    while (!found_data) {
        char chunk_id[4];
        uint32_t chunk_size;
        if (fread(chunk_id, 1, 4, f) != 4) break;
        if (fread(&chunk_size, 4, 1, f) != 1) break;

        if (memcmp(chunk_id, "fmt ", 4) == 0) {
            fread(&audio_format, 2, 1, f);
            fread(&num_channels, 2, 1, f);
            fread(&sample_rate, 4, 1, f);
            uint32_t byte_rate;
            uint16_t block_align;
            fread(&byte_rate, 4, 1, f);
            fread(&block_align, 2, 1, f);
            fread(&bits_per_sample, 2, 1, f);
            /* skip any extra fmt bytes */
            if (chunk_size > 16)
                fseek(f, chunk_size - 16, SEEK_CUR);
            found_fmt = 1;
        } else if (memcmp(chunk_id, "data", 4) == 0) {
            data_size = chunk_size;
            found_data = 1;
        } else {
            fseek(f, chunk_size, SEEK_CUR);
        }
    }

    if (!found_fmt || !found_data) {
        fprintf(stderr, "Missing fmt or data chunk\n");
        fclose(f);
        return -1;
    }
    if (audio_format != 1) {
        fprintf(stderr, "Not PCM format (format=%u)\n", audio_format);
        fclose(f);
        return -1;
    }
    if (num_channels != 1) {
        fprintf(stderr, "Not mono (channels=%u)\n", num_channels);
        fclose(f);
        return -1;
    }
    if (bits_per_sample != 16) {
        fprintf(stderr, "Not 16-bit (bits=%u)\n", bits_per_sample);
        fclose(f);
        return -1;
    }

    uint32_t n = data_size / 2;
    int16_t *samples = (int16_t *)malloc(data_size);
    if (!samples) {
        fprintf(stderr, "Alloc failed\n");
        fclose(f);
        return -1;
    }
    fread(samples, 2, n, f);
    fclose(f);

    *out_samples = samples;
    *out_num_samples = n;
    *out_rate = sample_rate;
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.wav> [--verbose]\n", argv[0]);
        return 1;
    }

    int verbose = 0;
    if (argc >= 3 && strcmp(argv[2], "--verbose") == 0)
        verbose = 1;

    int16_t *samples = NULL;
    uint32_t num_samples = 0, rate = 0;
    if (read_wav(argv[1], &samples, &num_samples, &rate) != 0)
        return 1;

    printf("WAV: %u samples, %u Hz, %.3f sec\n",
           num_samples, rate, (double)num_samples / rate);

    /* Create DTMF decoder */
    plcode_dtmf_dec_t *dec = NULL;
    int err = plcode_dtmf_dec_create(&dec, (int)rate);
    if (err != PLCODE_OK) {
        fprintf(stderr, "plcode_dtmf_dec_create failed: %d\n", err);
        free(samples);
        return 1;
    }

    /* Process in 20ms chunks (matching the decoder's block size) */
    size_t chunk = rate / 50; /* 20ms */
    plcode_dtmf_result_t result;
    memset(&result, 0, sizeof(result));

    char detected_digits[4096];
    int digit_count = 0;
    int prev_detected = 0;
    char prev_digit = '\0';

    printf("\n--- DTMF Detection Log ---\n");

    for (uint32_t pos = 0; pos < num_samples; pos += chunk) {
        size_t n = chunk;
        if (pos + n > num_samples)
            n = num_samples - pos;

        plcode_dtmf_dec_process(dec, samples + pos, n, &result);

        double time_sec = (double)pos / rate;

        if (verbose) {
            if (result.detected)
                printf("  [%7.3f s] detected='%c' row=%u col=%u\n",
                       time_sec, result.digit, result.row_freq, result.col_freq);
            else if (prev_detected)
                printf("  [%7.3f s] (silence)\n", time_sec);
        }

        /* Record digit transitions */
        if (result.detected && !prev_detected) {
            printf("  %7.3f s  DIGIT '%c'  (row=%u Hz, col=%u Hz)\n",
                   time_sec, result.digit, result.row_freq, result.col_freq);
            if (digit_count < (int)sizeof(detected_digits) - 1)
                detected_digits[digit_count++] = result.digit;
        } else if (result.detected && result.digit != prev_digit && prev_detected) {
            /* Digit changed without gap */
            printf("  %7.3f s  DIGIT '%c'  (row=%u Hz, col=%u Hz) [no gap]\n",
                   time_sec, result.digit, result.row_freq, result.col_freq);
            if (digit_count < (int)sizeof(detected_digits) - 1)
                detected_digits[digit_count++] = result.digit;
        }

        prev_detected = result.detected;
        prev_digit = result.digit;
    }

    detected_digits[digit_count] = '\0';

    printf("\n--- Result ---\n");
    printf("Detected %d digit(s): \"%s\"\n", digit_count, detected_digits);

    plcode_dtmf_dec_destroy(dec);
    free(samples);
    return 0;
}
