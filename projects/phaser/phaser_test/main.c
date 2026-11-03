#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cn0566_control.h"
#include "genalyzer_analysis.h"
#include "iio_capture.h"
#include "no_os_delay.h"

#define MAX_CMD_LEN 512U
#define SETTLE_MS 50U
#define DEFAULT_URI "ip:192.168.2.1"
#define FALLBACK_URI "ip:localhost"
#define SAMPLE_RATE_HZ 30000000LL
#define RX_LO_HZ 2200000000LL
#define RF_BANDWIDTH_HZ 10000000LL
#define DEFAULT_SAMPLES 4096U
#define DEFAULT_CSV "captures/capture.csv"

static void print_help(void)
{
    puts("gain <chip> <ch> <0-127>\ngain_all <0-127>\n"
         "phase <chip> <ch> <0-359>\nphase_all <0-359>\n"
         "pll <hz> | pll_full <hz>\nlock | gpios | dump\n"
         "capture [csv] [samples]\nanalyze [samples]\n"
         "capture_analyze [csv] [samples]\nhelp | quit");
}

static int acquire(struct iio_capture *capture, const char *csv, size_t samples,
                   int save, int analyze)
{
    struct iio_capture_data data = {0};
    int ret = iio_capture_read(capture, &data, samples);
    if (ret) return ret;
    if (analyze) {
        struct genalyzer_config cfg = {SAMPLE_RATE_HZ, samples, 3U};
        struct genalyzer_result result;
        ret = genalyzer_analyze_iq(&data, &cfg, &result);
        if (!ret) genalyzer_print_result(&result);
    }
    if (!ret && save) ret = iio_capture_write_csv(&data, csv);
    iio_capture_data_free(&data);
    return ret;
}

int main(int argc, char **argv)
{
    const char *uri = argc > 1 ? argv[1] : NULL;
    const char *fallback_uris[] = {DEFAULT_URI, FALLBACK_URI};
    struct cn0566_dev *board = NULL;
    struct iio_capture *capture = NULL;
    struct cn0566_init_param board_cfg = {127U, 0U};
    struct iio_capture_config cap_cfg = {
        NULL, SAMPLE_RATE_HZ, RX_LO_HZ, RF_BANDWIDTH_HZ,
        0.0, 0.0, DEFAULT_SAMPLES, 3U
    };
    char cmd[MAX_CMD_LEN];
    int32_t ret = 0;
    uint8_t lock;
    unsigned chip, ch, val;
    unsigned long long freq;

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
    printf("Pluto connected: %s\nREADY\n", cap_cfg.uri);
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

    else {
        puts("ERR: unknown command");
        continue;
    }

    no_os_mdelay(SETTLE_MS);
    puts(ret ? "ERR" : "OK");
}

cleanup:
    iio_capture_close(capture);
    cn0566_remove(board);
    if (ret) fprintf(stderr, "Failure: %ld\n", (long)ret);
    return ret ? EXIT_FAILURE : EXIT_SUCCESS;
}
