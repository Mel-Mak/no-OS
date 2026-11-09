/***************************************************************************//**
 *   @file   main.c
 *   @brief  AD7291 voltage monitor example for Raspberry Pi (Linux platform).
 *           Reads and prints all 8 channel voltages in a loop.
 * 	 @author	 Melissa Makonga (melissa.makonga@analog.com)
********************************************************************************
* Copyright 2015(c) Analog Devices, Inc.
*
* SPDX-License-Identifier: BSD-3-Clause
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "cn0566_calibration.h"
#include "cn0566_control.h"
#include "genalyzer_analysis.h"
#include "iio_capture.h"
#include "no_os_delay.h"

#define MAX_CMD_LEN 512U
#define SETTLE_MS 50U
#define DEFAULT_URI "ip:localhost"
#define FALLBACK_URI "ip:192.168.2.1"
#define SAMPLE_RATE_HZ 30000000LL
#define RX_LO_HZ 2200000000LL
#define RF_BANDWIDTH_HZ 10000000LL
#define DEFAULT_SAMPLES 4096U
#define DEFAULT_CSV "captures/capture.csv"
#define DEFAULT_CAL "cal_data.json"
#define SIGNAL_FREQ_HZ 10425537000ULL
#define OFFSET_HZ 1000000LL

static void print_help(void)
{
    puts("gain <chip> <ch> <0-127>\ngain_all <0-127>\n"
         "phase <chip> <ch> <0-359>\nphase_all <0-359>\n"
         "rx_gain <dB>              set AD9361 RX gain (both channels)\n"
         "pll <hz> | pll_full <hz>\nlock | gpios | dump\n"
         "capture [csv] [samples]\nanalyze [samples]\n"
         "capture_analyze [csv] [samples]\nraw | diag\n"
         "calibrate                 re-run HB100 frequency calibration\n"
         "cal                       full gain/phase calibration\n"
         "help | quit");
}

static void channel_stats(const char *name, const struct iio_complex_sample *s,
                          size_t n)
{
    double sum_sq = 0.0;
    int32_t peak = 0;
    size_t i;
    for (i = 0; i < n; i++) {
        int32_t mag_sq = (int32_t)s[i].i * s[i].i + (int32_t)s[i].q * s[i].q;
        sum_sq += (double)mag_sq;
        if (mag_sq > peak) peak = mag_sq;
    }
    printf("  %s: peak=%.1f  RMS=%.1f\n", name,
           sqrt((double)peak), sqrt(sum_sq / n));
}

static double capture_rms(struct iio_capture *cap)
{
    struct iio_capture_data data = {0};
    double sum0 = 0.0, sum1 = 0.0;
    size_t i, n;
    if (iio_capture_read(cap, &data, DEFAULT_SAMPLES))
        return 0.0;
    n = data.sample_count;
    for (i = 0; i < n; i++) {
        sum0 += (double)data.rx0[i].i * data.rx0[i].i +
                (double)data.rx0[i].q * data.rx0[i].q;
        sum1 += (double)data.rx1[i].i * data.rx1[i].i +
                (double)data.rx1[i].q * data.rx1[i].q;
    }
    iio_capture_data_free(&data);
    double rms0 = sqrt(sum0 / n);
    double rms1 = sqrt(sum1 / n);
    return rms0 > rms1 ? rms0 : rms1;
}

/* Sweep PLL to find the actual HB100 frequency (like phaser_find_hb100.py).
 * Covers 10.28-10.72 GHz signal range (PLL 3120-3230 MHz). */
static int calibrate_hb100(struct adf4159_dev *pll, struct iio_capture *cap,
                           uint64_t *signal_hz)
{
    uint64_t pll_freq, best_pll = 0;
    double rms, best_rms = 0.0;
    int ret;

    printf("Calibrating HB100 (coarse sweep)...\n");
    for (pll_freq = 3120000000ULL; pll_freq <= 3230000000ULL;
         pll_freq += 2000000) {
        ret = adf4159_setup(pll, pll_freq);
        if (ret) return ret;
        no_os_mdelay(20);
        rms = capture_rms(cap);
        if (rms > best_rms) {
            best_rms = rms;
            best_pll = pll_freq;
        }
    }

    if (best_rms < 10.0) {
        printf("No HB100 signal found (max RMS=%.1f)\n", best_rms);
        return -1;
    }
    printf("  Coarse: PLL=%llu MHz, RMS=%.1f\n",
           (unsigned long long)(best_pll / 1000000), best_rms);

    printf("Refining (+/-4 MHz, 200 kHz steps)...\n");
    best_rms = 0.0;
    for (pll_freq = best_pll - 4000000; pll_freq <= best_pll + 4000000;
         pll_freq += 200000) {
        ret = adf4159_setup(pll, pll_freq);
        if (ret) return ret;
        no_os_mdelay(20);
        rms = capture_rms(cap);
        if (rms > best_rms) {
            best_rms = rms;
            best_pll = pll_freq;
        }
    }

    *signal_hz = 4 * best_pll - (uint64_t)RX_LO_HZ;
    printf("  HB100 = %.6f GHz (PLL = %llu Hz)\n",
           (double)*signal_hz / 1e9, (unsigned long long)best_pll);
    return 0;
}

static void print_capture_stats(const struct iio_capture_data *data)
{
    channel_stats("RX0", data->rx0, data->sample_count);
    channel_stats("RX1", data->rx1, data->sample_count);
}

static int acquire(struct iio_capture *capture, const char *csv, size_t samples,
                   int save, int analyze)
{
    struct iio_capture_data data = {0};
    int ret = iio_capture_read(capture, &data, samples);
    if (ret) return ret;
    print_capture_stats(&data);
    if (analyze) {
        struct genalyzer_config cfg = {SAMPLE_RATE_HZ, samples, 3U};
        struct genalyzer_result result;
        ret = genalyzer_analyze_iq(&data, &cfg, &result);
        if (!ret) genalyzer_print_result(&result);
    }
    if (!ret && save && csv) {
        int csv_ret = iio_capture_write_csv(&data, csv);
        if (csv_ret)
            printf("Warning: could not save CSV to %s\n", csv);
    }
    iio_capture_data_free(&data);
    return ret;
}

int main(int argc, char **argv)
{
    const char *uri = NULL;
    uint64_t signal_freq_hz = SIGNAL_FREQ_HZ;
    int have_freq_arg = 0;

    for (int a = 1; a < argc; a++) {
        if (strncmp(argv[a], "ip:", 3) == 0)
            uri = argv[a];
        else {
            signal_freq_hz = strtoull(argv[a], NULL, 10);
            have_freq_arg = 1;
        }
    }
    const char *fallback_uris[] = {DEFAULT_URI, FALLBACK_URI};
    struct cn0566_dev *board = NULL;
    struct iio_capture *capture = NULL;
    struct cn0566_init_param board_cfg = {127U, 0U};
    struct iio_capture_config cap_cfg = {
        NULL, SAMPLE_RATE_HZ, RX_LO_HZ, RF_BANDWIDTH_HZ,
        0.0, 0.0, DEFAULT_SAMPLES, 3U
    };
    struct cn0566_cal_data cal_data = {0};
    char cmd[MAX_CMD_LEN];
    int32_t ret = 0;
    uint8_t lock;
    unsigned chip, ch, val;
    unsigned long long freq;
    double rx_gain_val;

    setlinebuf(stdout);
    if ((ret = cn0566_init(&board, &board_cfg))) goto cleanup;
    cn0566_verify_gpios(board);

    if (uri) {
        cap_cfg.uri = uri;
        ret = iio_capture_open(&capture, &cap_cfg);
    } else {
        for (unsigned i = 0; i < 2; i++) {
            cap_cfg.uri = fallback_uris[i];
            printf("Trying Pluto at %s ...\n", fallback_uris[i]);
            ret = iio_capture_open(&capture, &cap_cfg);
            if (!ret) break;
        }
    }
    if (ret) goto cleanup;
    printf("Pluto connected: %s\n", cap_cfg.uri);

    /* Calibrate or use provided HB100 frequency, then set PLL */
    {
        struct adf4159_dev *pll_init = cn0566_get_adf4159(board);
        uint64_t pll_freq;

        if (have_freq_arg) {
            printf("Using provided signal freq: %.6f GHz\n",
                   (double)signal_freq_hz / 1e9);
        } else {
            ret = calibrate_hb100(pll_init, capture, &signal_freq_hz);
            if (ret) {
                printf("Calibration failed, using default %.6f GHz\n",
                       (double)SIGNAL_FREQ_HZ / 1e9);
                signal_freq_hz = SIGNAL_FREQ_HZ;
            }
        }

        pll_freq = (signal_freq_hz + RX_LO_HZ - OFFSET_HZ) / 4;
        printf("Setting PLL to %llu Hz ...\n", (unsigned long long)pll_freq);
        if ((ret = adf4159_setup(pll_init, pll_freq))) goto cleanup;
        no_os_mdelay(SETTLE_MS);
        if ((ret = cn0566_get_pll_lock(board, &lock))) goto cleanup;
        printf("PLL: %s\n", lock ? "LOCKED" : "NOT LOCKED");
        if (!lock)
            printf("WARNING: PLL not locked, signal may be absent\n");
    }

    if (!cn0566_load_calibration(&cal_data, DEFAULT_CAL)) {
        cn0566_apply_calibration(board, &cal_data);
        printf("Loaded calibration from %s\n", DEFAULT_CAL);
    } else {
        uint8_t i;
        for (i = 0; i < CN0566_NUM_ELEMENTS; i++)
            cal_data.gcal[i] = 1.0;
        printf("No calibration file found. Run 'cal' to calibrate.\n");
    }

    puts("READY");
    print_help();

    while (1) {
    char path[256] = DEFAULT_CSV;
    size_t samples = DEFAULT_SAMPLES;
    struct adar1000_dev *adar;
    struct adf4159_dev *pll = cn0566_get_adf4159(board);

    fputs("phaser> ", stdout);

    if (!fgets(cmd, sizeof(cmd), stdin))
        break;

    cmd[strcspn(cmd, "\r\n")] = '\0';

    /* General commands */
    if (!cmd[0])
        continue;

    if (!strcmp(cmd, "quit"))
        break;

    if (!strcmp(cmd, "help")) {
        print_help();
        continue;
    }

    /* ADAR1000 gain control */
    if (sscanf(cmd, "gain %u %u %u", &chip, &ch, &val) == 3) {
        adar = cn0566_get_adar1000(board, chip);

        if (!adar ||
            ch >= ADAR1000_NUM_CHANNELS ||
            val > 127U) {
            puts("ERR: invalid gain arguments");
            continue;
        }

        ret = adar1000_set_rx_gain(adar,
                                   (uint8_t)ch,
                                   (uint8_t)val);

        if (!ret)
            ret = adar1000_latch_rx(adar);
    }

    else if (sscanf(cmd, "gain_all %u", &val) == 1) {
        ret = cn0566_set_all_rx_gains(board,
                                      (uint8_t)val);
    }

    /* ADAR1000 phase control */
    else if (sscanf(cmd, "phase %u %u %u",
                    &chip, &ch, &val) == 3) {
        adar = cn0566_get_adar1000(board, chip);

        if (!adar ||
            ch >= ADAR1000_NUM_CHANNELS ||
            val >= 360U) {
            puts("ERR: invalid phase arguments");
            continue;
        }

        ret = adar1000_set_rx_phase(adar,
                                    (uint8_t)ch,
                                    (uint16_t)val);

        if (!ret)
            ret = adar1000_latch_rx(adar);
    }

    else if (sscanf(cmd, "phase_all %u", &val) == 1) {
        ret = cn0566_set_all_rx_phases(board,
                                       (uint16_t)val);
    }

    /* AD9361 RX gain */
    else if (sscanf(cmd, "rx_gain %lf", &rx_gain_val) == 1) {
        ret = iio_capture_set_rx_gain(capture,
                                      rx_gain_val,
                                      rx_gain_val);
    }

    /* ADF4159 control */
    else if (sscanf(cmd, "pll_full %llu", &freq) == 1) {
        ret = adf4159_setup(pll, (uint64_t)freq);
    }

    else if (sscanf(cmd, "pll %llu", &freq) == 1) {
        ret = adf4159_set_freq(pll, (uint64_t)freq);
    }

    else if (!strcmp(cmd, "lock")) {
        ret = cn0566_get_pll_lock(board, &lock);

        if (!ret)
            printf("PLL: %s\n",
                   lock ? "LOCKED" : "NOT LOCKED");
    }

    else if (!strcmp(cmd, "muxout_test")) {
        struct adf4159_dev *p = cn0566_get_adf4159(board);
        printf("Setting MUXOUT=DVDD (should read 1)...\n");
        adf4159_set_muxout(p, 1);
        no_os_mdelay(10);
        cn0566_get_pll_lock(board, &lock);
        printf("  MUXOUT GPIO = %u %s\n", lock,
               lock ? "OK" : "FAIL: pin/GPIO broken");
        printf("Restoring MUXOUT=DLOCK...\n");
        adf4159_set_muxout(p, 6);
        no_os_mdelay(50);
        cn0566_get_pll_lock(board, &lock);
        printf("  MUXOUT GPIO = %u (%s)\n", lock,
               lock ? "LOCKED" : "NOT LOCKED");
        ret = 0;
    }

    else if (!strcmp(cmd, "calibrate")) {
        cn0566_set_all_rx_gains(board, 127);
        cn0566_set_all_rx_phases(board, 0);
        no_os_mdelay(SETTLE_MS);
        ret = calibrate_hb100(pll, capture, &signal_freq_hz);
        if (!ret) {
            uint64_t pf = (signal_freq_hz + RX_LO_HZ - OFFSET_HZ) / 4;
            printf("Setting PLL to %llu Hz\n", (unsigned long long)pf);
            ret = adf4159_setup(pll, pf);
        }
        if (cal_data.gcal[0] > 0.0)
            cn0566_apply_calibration(board, &cal_data);
    }

    else if (!strcmp(cmd, "cal")) {
        ret = cn0566_run_full_calibration(board, capture,
                                          (double)SAMPLE_RATE_HZ,
                                          &cal_data);
        if (!ret)
            ret = cn0566_save_calibration(&cal_data, DEFAULT_CAL);
    }

    else if (!strcmp(cmd, "dump")) {
        adf4159_dump_regs(pll);
        ret = 0;
    }

    /* CN0566 board status */
    else if (!strcmp(cmd, "gpios")) {
        ret = cn0566_verify_gpios(board);
    }

    /* Pluto capture and analysis */
    else if (!strncmp(cmd, "capture_analyze", 15)) {
        sscanf(cmd,
               "capture_analyze %255s %zu",
               path,
               &samples);

        ret = acquire(capture,
                      path,
                      samples,
                      1,
                      1);
    }

    else if (!strncmp(cmd, "capture", 7)) {
        sscanf(cmd,
               "capture %255s %zu",
               path,
               &samples);

        ret = acquire(capture,
                      path,
                      samples,
                      1,
                      0);
    }

    else if (!strncmp(cmd, "analyze", 7)) {
        sscanf(cmd,
               "analyze %zu",
               &samples);

        ret = acquire(capture,
                      NULL,
                      samples,
                      0,
                      1);
    }

    else if (!strcmp(cmd, "diag")) {
        printf("-- PLL --\n");
        adf4159_dump_regs(cn0566_get_adf4159(board));
        no_os_mdelay(10);
        cn0566_get_pll_lock(board, &lock);
        printf("PLL lock: %u\n", lock);
        printf("\n-- GPIOs --\n");
        cn0566_verify_gpios(board);
        printf("\n");
        iio_capture_diag(capture);
        ret = 0;
    }

    else if (!strcmp(cmd, "raw")) {
        struct iio_capture_data data = {0};
        ret = iio_capture_read(capture, &data, DEFAULT_SAMPLES);
        if (!ret) {
            size_t show = data.sample_count < 10 ? data.sample_count : 10;
            printf("URI: %s  samples: %zu  showing first %zu\n",
                   cap_cfg.uri, data.sample_count, show);
            for (size_t j = 0; j < show; j++)
                printf("  [%zu] rx0=(%d,%d) rx1=(%d,%d)\n", j,
                       data.rx0[j].i, data.rx0[j].q,
                       data.rx1[j].i, data.rx1[j].q);
            print_capture_stats(&data);
        }
        iio_capture_data_free(&data);
    }

    else {
        puts("ERR: unknown command");
        continue;
    }

    no_os_mdelay(SETTLE_MS);
    if (ret)
        printf("ERR: %ld\n", (long)ret);
    else
        puts("OK");
}

cleanup:
    iio_capture_close(capture);
    cn0566_remove(board);
    if (ret) fprintf(stderr, "Failure: %ld\n", (long)ret);
    return ret ? EXIT_FAILURE : EXIT_SUCCESS;
}
