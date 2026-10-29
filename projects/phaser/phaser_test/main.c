/***************************************************************************//**
 * @file   main.c
 * @brief  CN0566 ADAR1000/ADF4159 interactive validation server.
 * @author Melissa Makonga (melissa.makonga@analog.com)
 ******************************************************************************
 * Copyright 2024(c) Analog Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*******************************************************************************/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adar1000.h"
#include "adf4159.h"
#include "adf4159_cfg.h"
#include "iio_capture.h"
#include "no_os_delay.h"
#include "no_os_gpio.h"
#include "parameters.h"

#define MAX_CMD_LEN 512U
#define NUM_ADAR1000 2U
#define SETTLE_MS 50U
#define INITIAL_GAIN 127U
#define INITIAL_PHASE_DEG 0U
#define DEFAULT_URI "ip:192.168.2.1"
#define SAMPLE_RATE_HZ 30000000LL
#define RX_LO_HZ 2200000000LL
#define RF_BANDWIDTH_HZ 10000000LL
#define DEFAULT_SAMPLES 4096U
#define DEFAULT_CSV "captures/capture.csv"

struct board_gpios {
    struct no_os_gpio_desc *vctrl1, *vctrl2, *div_mr, *div_s0, *div_s1;
    struct no_os_gpio_desc *div_s2, *rx_load, *tr, *tx_sw, *muxout, *burst;
};

static int32_t gpio_open(struct no_os_gpio_desc **desc, uint32_t number)
{
    struct no_os_gpio_init_param p = {
        .number = number, .platform_ops = GPIO_OPS, .extra = NULL
    };
    return no_os_gpio_get(desc, &p);
}

static int32_t board_gpio_init(struct board_gpios *g)
{
    int32_t ret;
#define GET(member, number) do { ret = gpio_open(&g->member, number); if (ret) return ret; } while (0)
    GET(vctrl1, GPIO_VCTRL_1); GET(vctrl2, GPIO_VCTRL_2);
    GET(div_mr, GPIO_DIV_MR); GET(div_s0, GPIO_DIV_S0);
    GET(div_s1, GPIO_DIV_S1); GET(div_s2, GPIO_DIV_S2);
    GET(rx_load, GPIO_RX_LOAD); GET(tr, GPIO_TR); GET(tx_sw, GPIO_TX_SW);
    GET(muxout, GPIO_MUXOUT); GET(burst, GPIO_BURST);
#undef GET
#define OUT(member, value) do { ret = no_os_gpio_direction_output(g->member, value); if (ret) return ret; } while (0)
    OUT(burst, GPIO_LEVEL_BURST_RX); OUT(vctrl1, GPIO_LEVEL_VCTRL_1_RX);
    OUT(vctrl2, GPIO_LEVEL_VCTRL_2_RX); OUT(div_mr, GPIO_LEVEL_DIV_MR_RX);
    OUT(div_s0, GPIO_LEVEL_DIV_S0_RX); OUT(div_s1, GPIO_LEVEL_DIV_S1_RX);
    OUT(div_s2, GPIO_LEVEL_DIV_S2_RX); OUT(rx_load, GPIO_LEVEL_RX_LOAD_IDLE);
    OUT(tr, GPIO_LEVEL_TR_RX); OUT(tx_sw, GPIO_LEVEL_TX_SW_RX);
#undef OUT
    return no_os_gpio_direction_input(g->muxout);
}

static void board_gpio_remove(struct board_gpios *g)
{
    struct no_os_gpio_desc *all[] = {g->vctrl1, g->vctrl2, g->div_mr,
        g->div_s0, g->div_s1, g->div_s2, g->rx_load, g->tr, g->tx_sw,
        g->muxout, g->burst};
    size_t i;
    for (i = 0; i < sizeof(all) / sizeof(all[0]); i++)
        if (all[i]) no_os_gpio_remove(all[i]);
}

static int32_t show_gpio(const char *name, uint32_t pin,
                         struct no_os_gpio_desc *desc, int expected)
{
    uint8_t value = 0;
    int32_t ret = no_os_gpio_get_value(desc, &value);
    if (ret) return ret;
    printf("  %-8s GPIO%-2u = %u", name, pin, value);
    if (expected >= 0) printf(" %s", value == (uint8_t)expected ? "OK" : "MISMATCH");
    printf("\n");
    return 0;
}

static int32_t board_gpio_verify(struct board_gpios *g)
{
    int32_t status = 0;
#define SHOW(name,pin,member,expected) do { if (show_gpio(name,pin,g->member,expected)) status = -EIO; } while (0)
    SHOW("BURST", GPIO_BURST, burst, GPIO_LEVEL_BURST_RX);
    SHOW("VCTRL_1", GPIO_VCTRL_1, vctrl1, GPIO_LEVEL_VCTRL_1_RX);
    SHOW("VCTRL_2", GPIO_VCTRL_2, vctrl2, GPIO_LEVEL_VCTRL_2_RX);
    SHOW("DIV_MR", GPIO_DIV_MR, div_mr, GPIO_LEVEL_DIV_MR_RX);
    SHOW("DIV_S0", GPIO_DIV_S0, div_s0, GPIO_LEVEL_DIV_S0_RX);
    SHOW("DIV_S1", GPIO_DIV_S1, div_s1, GPIO_LEVEL_DIV_S1_RX);
    SHOW("DIV_S2", GPIO_DIV_S2, div_s2, GPIO_LEVEL_DIV_S2_RX);
    SHOW("RX_LOAD", GPIO_RX_LOAD, rx_load, GPIO_LEVEL_RX_LOAD_IDLE);
    SHOW("TR", GPIO_TR, tr, GPIO_LEVEL_TR_RX);
    SHOW("TX_SW", GPIO_TX_SW, tx_sw, GPIO_LEVEL_TX_SW_RX);
    SHOW("MUXOUT", GPIO_MUXOUT, muxout, -1);
#undef SHOW
    return status;
}

static int32_t init_adar1000_rx(struct adar1000_dev **dev,
                                uint8_t chip_select, uint8_t gain)
{
    struct adar1000_init_param p = {
        .spi_init = {.device_id = ADAR1000_SPI_DEVICE,
            .max_speed_hz = ADAR1000_SPI_SPEED, .chip_select = chip_select,
            .mode = NO_OS_SPI_MODE_0, .platform_ops = SPI_OPS, .extra = NULL},
        .dev_addr = ADAR1000_DEV_ADDR,
    };
    uint16_t product_id;
    uint8_t ch;
    int32_t ret = adar1000_init(dev, &p);
    if (ret) return ret;
    ret = adar1000_read_product_id(*dev, &product_id);
    if (ret) goto error;
    printf("ADAR1000 CS%u product ID: 0x%04X\n", chip_select, product_id);
    ret = adar1000_setup_rx(*dev);
    if (ret) goto error;
    for (ch = 0; ch < ADAR1000_NUM_CHANNELS; ch++) {
        ret = adar1000_set_rx_gain(*dev, ch, gain);
        if (ret) goto error;
        ret = adar1000_set_rx_phase(*dev, ch, INITIAL_PHASE_DEG);
        if (ret) goto error;
    }
    ret = adar1000_latch_rx(*dev);
    if (ret) goto error;
    return 0;
error:
    adar1000_remove(*dev); *dev = NULL; return ret;
}

static int capture_csv(const char *uri, const char *path, size_t samples)
{
    struct iio_capture_config cfg = {
        .uri = uri, .sample_rate_hz = SAMPLE_RATE_HZ, .rx_lo_hz = RX_LO_HZ,
        .rf_bandwidth_hz = RF_BANDWIDTH_HZ, .rx0_gain_db = 0.0,
        .rx1_gain_db = 0.0, .samples = samples, .discarded_buffers = 3U
    };
    struct iio_capture *capture = NULL;
    int ret = iio_capture_open(&capture, &cfg);
    if (!ret) ret = iio_capture_to_csv(capture, path, samples);
    iio_capture_close(capture);
    return ret;
}

static void print_help(void)
{
    puts("Commands:\n"
         "  gain <chip> <ch> <0-127>\n"
         "  gain_all <0-127>\n"
         "  phase <chip> <ch> <0-359>\n"
         "  phase_all <0-359>\n"
         "  pll <hz> | pll_full <hz>\n"
         "  lock | gpios | dump\n"
         "  capture [csv_path] [samples]\n"
         "  help | quit");
}

int main(int argc, char **argv)
{
    struct board_gpios gpios = {0};
    struct adar1000_dev *adar[NUM_ADAR1000] = {NULL, NULL};
    struct adf4159_dev *pll = NULL;
    const char *uri = argc > 1 ? argv[1] : DEFAULT_URI;
    char cmd[MAX_CMD_LEN];
    int32_t ret = 0;
    uint8_t lock = 0;
    uint32_t i;

    setlinebuf(stdout);
    ret = board_gpio_init(&gpios);
    if (ret) { printf("ERR: GPIO initialization failed: %ld\n", (long)ret); goto cleanup; }
    board_gpio_verify(&gpios);
    ret = init_adar1000_rx(&adar[0], ADAR1000_SPI_CS_0, INITIAL_GAIN);
    if (ret) { printf("ERR: ADAR1000 #0 init failed: %ld\n", (long)ret); goto cleanup; }
    ret = init_adar1000_rx(&adar[1], ADAR1000_SPI_CS_1, INITIAL_GAIN);
    if (ret) { printf("ERR: ADAR1000 #1 init failed: %ld\n", (long)ret); goto cleanup; }

    {
        struct adf4159_init_param p = {
            .spi_init = {.device_id = ADF4159_SPI_DEVICE,
                .max_speed_hz = ADF4159_SPI_SPEED,
                .chip_select = ADF4159_SPI_CS, .mode = NO_OS_SPI_MODE_0,
                .platform_ops = SPI_OPS, .extra = NULL},
            .gpio_le = {.number = ADF4159_GPIO_LE, .platform_ops = GPIO_OPS, .extra = NULL},
            .gpio_ce = {.number = -1, .platform_ops = GPIO_OPS, .extra = NULL},
            .config = adf4159_default_cfg,
        };
        ret = adf4159_init(&pll, &p);
    }
    if (ret) { printf("ERR: ADF4159 init failed: %ld\n", (long)ret); goto cleanup; }
    ret = adf4159_set_muxout(pll, 6U);
    if (ret) goto cleanup;

    printf("Pluto URI: %s\nREADY\n", uri);
    print_help();

    while (fputs("phaser> ", stdout), fgets(cmd, sizeof(cmd), stdin)) {
        unsigned chip, ch, val;
        unsigned long long freq;
        char path[256] = DEFAULT_CSV;
        size_t samples = DEFAULT_SAMPLES;
        cmd[strcspn(cmd, "\r\n")] = '\0';

        if (sscanf(cmd, "gain %u %u %u", &chip, &ch, &val) == 3) {
            if (chip >= NUM_ADAR1000 || ch >= ADAR1000_NUM_CHANNELS || val > 127U) {
                puts("ERR: invalid gain arguments"); continue;
            }
            ret = adar1000_set_rx_gain(adar[chip], (uint8_t)ch, (uint8_t)val);
            if (!ret) ret = adar1000_latch_rx(adar[chip]);
        } else if (sscanf(cmd, "gain_all %u", &val) == 1) {
            uint8_t gains[ADAR1000_NUM_CHANNELS];
            if (val > 127U) { puts("ERR: gain_all <0-127>"); continue; }
            for (i = 0; i < ADAR1000_NUM_CHANNELS; i++) gains[i] = (uint8_t)val;
            ret = adar1000_set_all_rx_gains(adar[0], gains);
            if (!ret) ret = adar1000_set_all_rx_gains(adar[1], gains);
        } else if (sscanf(cmd, "phase %u %u %u", &chip, &ch, &val) == 3) {
            if (chip >= NUM_ADAR1000 || ch >= ADAR1000_NUM_CHANNELS || val >= 360U) {
                puts("ERR: invalid phase arguments"); continue;
            }
            ret = adar1000_set_rx_phase(adar[chip], (uint8_t)ch, (uint16_t)val);
            if (!ret) ret = adar1000_latch_rx(adar[chip]);
        } else if (sscanf(cmd, "phase_all %u", &val) == 1) {
            if (val >= 360U) { puts("ERR: phase_all <0-359>"); continue; }
            ret = 0;
            for (chip = 0; chip < NUM_ADAR1000 && !ret; chip++) {
                for (ch = 0; ch < ADAR1000_NUM_CHANNELS && !ret; ch++)
                    ret = adar1000_set_rx_phase(adar[chip], (uint8_t)ch, (uint16_t)val);
                if (!ret) ret = adar1000_latch_rx(adar[chip]);
            }
        } else if (sscanf(cmd, "pll_full %llu", &freq) == 1) {
            ret = adf4159_setup(pll, (uint64_t)freq);
        } else if (sscanf(cmd, "pll %llu", &freq) == 1) {
            ret = adf4159_set_freq(pll, (uint64_t)freq);
        } else if (!strcmp(cmd, "lock")) {
            ret = no_os_gpio_get_value(gpios.muxout, &lock);
            if (!ret) printf("MUXOUT=%u %s\n", lock, lock ? "LOCKED" : "NOT_LOCKED");
        } else if (!strcmp(cmd, "gpios")) {
            ret = board_gpio_verify(&gpios);
        } else if (!strcmp(cmd, "dump")) {
            adf4159_dump_regs(pll); ret = 0;
        } else if (!strncmp(cmd, "capture", 7)) {
            int parsed = sscanf(cmd, "capture %255s %zu", path, &samples);
            if (parsed < 1) snprintf(path, sizeof(path), "%s", DEFAULT_CSV);
            if (parsed < 2) samples = DEFAULT_SAMPLES;
            ret = capture_csv(uri, path, samples);
        } else if (!strcmp(cmd, "help")) {
            print_help(); continue;
        } else if (!strcmp(cmd, "quit")) {
            break;
        } else if (!cmd[0]) {
            continue;
        } else {
            puts("ERR: unknown command"); continue;
        }

        no_os_mdelay(SETTLE_MS);
        puts(ret ? "ERR" : "OK");
    }

cleanup:
    if (pll) adf4159_remove(pll);
    for (i = 0; i < NUM_ADAR1000; i++) if (adar[i]) adar1000_remove(adar[i]);
    board_gpio_remove(&gpios);
    return ret ? EXIT_FAILURE : EXIT_SUCCESS;
}
