/*
 * decode_dtmf_wav_debug.c — Deep DTMF analysis of a WAV file
 *
 * Runs Goertzel filters on every 20ms block and prints raw energy levels
 * for all 8 DTMF frequencies, plus tries relaxed decoder settings.
 */

#include "../include/plcode.h"
#include "../src/plcode_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

static int read_wav(const char *path, int16_t **out_samples,
                    uint32_t *out_num_samples, uint32_t *out_rate)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open %s\n", path); return -1; }

    char riff[4]; uint32_t file_size; char wave[4];
    fread(riff, 1, 4, f); fread(&file_size, 4, 1, f); fread(wave, 1, 4, f);

    if (memcmp(riff, "RIFF", 4) || memcmp(wave, "WAVE", 4)) {
        fprintf(stderr, "Not a WAV file\n"); fclose(f); return -1;
    }

    uint16_t audio_format = 0, num_channels = 0, bits_per_sample = 0;
    uint32_t sample_rate = 0, data_size = 0;
    int found_fmt = 0, found_data = 0;

    while (!found_data) {
        char chunk_id[4]; uint32_t chunk_size;
        if (fread(chunk_id, 1, 4, f) != 4) break;
        if (fread(&chunk_size, 4, 1, f) != 1) break;
        if (memcmp(chunk_id, "fmt ", 4) == 0) {
            fread(&audio_format, 2, 1, f); fread(&num_channels, 2, 1, f);
            fread(&sample_rate, 4, 1, f);
            uint32_t br; uint16_t ba;
            fread(&br, 4, 1, f); fread(&ba, 2, 1, f);
            fread(&bits_per_sample, 2, 1, f);
            if (chunk_size > 16) fseek(f, chunk_size - 16, SEEK_CUR);
            found_fmt = 1;
        } else if (memcmp(chunk_id, "data", 4) == 0) {
            data_size = chunk_size; found_data = 1;
        } else {
            fseek(f, chunk_size, SEEK_CUR);
        }
    }

    if (!found_fmt || !found_data || audio_format != 1 ||
        num_channels != 1 || bits_per_sample != 16) {
        fprintf(stderr, "Unsupported format\n"); fclose(f); return -1;
    }

    uint32_t n = data_size / 2;
    int16_t *samples = (int16_t *)malloc(data_size);
    if (!samples) { fclose(f); return -1; }
    fread(samples, 2, n, f); fclose(f);

    *out_samples = samples; *out_num_samples = n; *out_rate = sample_rate;
    return 0;
}

/* Simple Goertzel magnitude for a single frequency */
static double goertzel_mag(const int16_t *buf, size_t n, int freq, int rate)
{
    double k = 0.5 + ((double)n * freq / rate);
    double w = 2.0 * M_PI * k / n;
    double coeff = 2.0 * cos(w);
    double s0 = 0, s1 = 0, s2 = 0;

    for (size_t i = 0; i < n; i++) {
        s0 = (double)buf[i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    return s1*s1 + s2*s2 - coeff*s1*s2;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.wav>\n", argv[0]);
        return 1;
    }

    int16_t *samples = NULL;
    uint32_t num_samples = 0, rate = 0;
    if (read_wav(argv[1], &samples, &num_samples, &rate) != 0) return 1;

    printf("WAV: %u samples, %u Hz, %.3f sec\n\n",
           num_samples, rate, (double)num_samples / rate);

    /* DTMF frequencies */
    int row_freqs[] = {697, 770, 852, 941};
    int col_freqs[] = {1209, 1336, 1477, 1633};

    size_t block = rate / 50; /* 20ms */

    /* Pass 1: Scan for energy at DTMF frequencies */
    printf("=== Energy scan (20ms blocks, showing blocks with significant energy) ===\n");
    printf("Time(s)    | 697   770   852   941  | 1209  1336  1477  1633  | RMS   | Candidate\n");
    printf("-----------+-------------------------+--------------------------+-------+----------\n");

    for (uint32_t pos = 0; pos < num_samples; pos += block) {
        size_t n = block;
        if (pos + n > num_samples) n = num_samples - pos;
        if (n < block / 2) break;

        /* RMS of block */
        double sum_sq = 0;
        for (size_t i = 0; i < n; i++)
            sum_sq += (double)samples[pos+i] * samples[pos+i];
        double rms = sqrt(sum_sq / n);

        /* Skip very quiet blocks */
        if (rms < 50) continue;

        double row_mag[4], col_mag[4];
        for (int i = 0; i < 4; i++) {
            row_mag[i] = goertzel_mag(samples + pos, n, row_freqs[i], rate);
            col_mag[i] = goertzel_mag(samples + pos, n, col_freqs[i], rate);
        }

        /* Normalize to dB relative to strongest */
        double max_all = 1.0;
        for (int i = 0; i < 4; i++) {
            if (row_mag[i] > max_all) max_all = row_mag[i];
            if (col_mag[i] > max_all) max_all = col_mag[i];
        }

        /* Find best row and column */
        int best_row = 0, best_col = 0;
        for (int i = 1; i < 4; i++) {
            if (row_mag[i] > row_mag[best_row]) best_row = i;
            if (col_mag[i] > col_mag[best_col]) best_col = i;
        }

        /* Relative dB for each freq */
        double row_db[4], col_db[4];
        for (int i = 0; i < 4; i++) {
            row_db[i] = 10.0 * log10(row_mag[i] / max_all + 1e-20);
            col_db[i] = 10.0 * log10(col_mag[i] / max_all + 1e-20);
        }

        /* Check if it looks like DTMF: best row and best col both strong,
           and SNR vs second-best in each group > 6dB */
        double second_row = 0, second_col = 0;
        for (int i = 0; i < 4; i++) {
            if (i != best_row && row_mag[i] > second_row) second_row = row_mag[i];
            if (i != best_col && col_mag[i] > second_col) second_col = col_mag[i];
        }

        double row_snr = 10.0 * log10(row_mag[best_row] / (second_row + 1e-20));
        double col_snr = 10.0 * log10(col_mag[best_col] / (second_col + 1e-20));

        const char *digits[] = {
            "1","2","3","A",
            "4","5","6","B",
            "7","8","9","C",
            "*","0","#","D"
        };

        const char *candidate = "";
        if (row_snr > 4.0 && col_snr > 4.0 &&
            row_db[best_row] > -10.0 && col_db[best_col] > -10.0) {
            candidate = digits[best_row * 4 + best_col];
        }

        printf("%7.3f    | %5.1f %5.1f %5.1f %5.1f | %5.1f %5.1f %5.1f %5.1f | %5.0f | %s\n",
               (double)pos / rate,
               row_db[0], row_db[1], row_db[2], row_db[3],
               col_db[0], col_db[1], col_db[2], col_db[3],
               rms, candidate);
    }

    /* Pass 2: Run official decoder with default settings */
    printf("\n=== Default decoder ===\n");
    {
        plcode_dtmf_dec_t *dec = NULL;
        plcode_dtmf_dec_create(&dec, (int)rate);
        plcode_dtmf_result_t result;
        memset(&result, 0, sizeof(result));
        int prev = 0; char prev_d = 0;
        char digits_buf[256]; int dc = 0;

        for (uint32_t pos = 0; pos < num_samples; pos += block) {
            size_t n = block;
            if (pos + n > num_samples) n = num_samples - pos;
            plcode_dtmf_dec_process(dec, samples + pos, n, &result);
            if (result.detected && (!prev || result.digit != prev_d)) {
                printf("  %7.3f s  '%c' (row=%u, col=%u)\n",
                       (double)pos/rate, result.digit, result.row_freq, result.col_freq);
                if (dc < 255) digits_buf[dc++] = result.digit;
            }
            prev = result.detected; prev_d = result.digit;
        }
        digits_buf[dc] = 0;
        printf("  Result: \"%s\" (%d digits)\n", digits_buf, dc);
        plcode_dtmf_dec_destroy(dec);
    }

    /* Pass 3: Run with relaxed settings (1 hit to begin, 1 miss to end) */
    printf("\n=== Relaxed decoder (hits=1, misses=1) ===\n");
    {
        plcode_dtmf_dec_opts_t opts;
        memset(&opts, 0, sizeof(opts));
        opts.hits_to_begin = 1;
        opts.misses_to_end = 1;
        opts.min_off_frames = 1;
        opts.normal_twist_x = 32;   /* 15dB */
        opts.reverse_twist_x = 16;  /* 12dB */

        plcode_dtmf_dec_t *dec = NULL;
        plcode_dtmf_dec_create_ex(&dec, (int)rate, &opts);
        plcode_dtmf_result_t result;
        memset(&result, 0, sizeof(result));
        int prev = 0; char prev_d = 0;
        char digits_buf[256]; int dc = 0;

        for (uint32_t pos = 0; pos < num_samples; pos += block) {
            size_t n = block;
            if (pos + n > num_samples) n = num_samples - pos;
            plcode_dtmf_dec_process(dec, samples + pos, n, &result);
            if (result.detected && (!prev || result.digit != prev_d)) {
                printf("  %7.3f s  '%c' (row=%u, col=%u)\n",
                       (double)pos/rate, result.digit, result.row_freq, result.col_freq);
                if (dc < 255) digits_buf[dc++] = result.digit;
            }
            prev = result.detected; prev_d = result.digit;
        }
        digits_buf[dc] = 0;
        printf("  Result: \"%s\" (%d digits)\n", digits_buf, dc);
        plcode_dtmf_dec_destroy(dec);
    }

    free(samples);
    return 0;
}
