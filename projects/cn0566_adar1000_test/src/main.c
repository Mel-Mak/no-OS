/***************************************************************************//**
 * @file   main.c
 * @brief  CN0566 ADAR1000/ADF4159 interactive validation server.
 * @author Melissa Makonga
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adar1000.h"
#include "adf4159.h"
#include "adf4159_cfg.h"
#include "no_os_delay.h"
#include "no_os_error.h"
#include "no_os_gpio.h"
#include "parameters.h"

#define MAX_CMD_LEN       256U
#define NUM_ADAR1000      2U
#define SETTLE_MS         50U
#define INITIAL_GAIN      127U
#define INITIAL_PHASE_DEG 0U

struct board_gpios {
	struct no_os_gpio_desc *vctrl1;
	struct no_os_gpio_desc *vctrl2;
	struct no_os_gpio_desc *div_mr;
	struct no_os_gpio_desc *div_s0;
	struct no_os_gpio_desc *div_s1;
	struct no_os_gpio_desc *div_s2;
	struct no_os_gpio_desc *rx_load;
	struct no_os_gpio_desc *tr;
	struct no_os_gpio_desc *tx_sw;
	struct no_os_gpio_desc *muxout;
	struct no_os_gpio_desc *burst;
};

static int32_t gpio_open(struct no_os_gpio_desc **desc, uint32_t number)
{
	struct no_os_gpio_init_param p = {
		.number = number,
		.platform_ops = GPIO_OPS,
		.extra = NULL,
	};
	return no_os_gpio_get(desc, &p);
}

static int32_t board_gpio_init(struct board_gpios *g)
{
	int32_t ret;
#define GET_GPIO(member, number) do { ret = gpio_open(&g->member, number); if (ret) return ret; } while (0)
	GET_GPIO(vctrl1, GPIO_VCTRL_1);
	GET_GPIO(vctrl2, GPIO_VCTRL_2);
	GET_GPIO(div_mr, GPIO_DIV_MR);
	GET_GPIO(div_s0, GPIO_DIV_S0);
	GET_GPIO(div_s1, GPIO_DIV_S1);
	GET_GPIO(div_s2, GPIO_DIV_S2);
	GET_GPIO(rx_load, GPIO_RX_LOAD);
	GET_GPIO(tr, GPIO_TR);
	GET_GPIO(tx_sw, GPIO_TX_SW);
	GET_GPIO(muxout, GPIO_MUXOUT);
	GET_GPIO(burst, GPIO_BURST);
#undef GET_GPIO

#define OUT(member, value) do { ret = no_os_gpio_direction_output(g->member, value); if (ret) return ret; } while (0)
	OUT(burst, GPIO_LEVEL_BURST_RX);
	OUT(vctrl1, GPIO_LEVEL_VCTRL_1_RX);
	OUT(vctrl2, GPIO_LEVEL_VCTRL_2_RX);
	OUT(div_mr, GPIO_LEVEL_DIV_MR_RX);
	OUT(div_s0, GPIO_LEVEL_DIV_S0_RX);
	OUT(div_s1, GPIO_LEVEL_DIV_S1_RX);
	OUT(div_s2, GPIO_LEVEL_DIV_S2_RX);
	OUT(rx_load, GPIO_LEVEL_RX_LOAD_IDLE);
	OUT(tr, GPIO_LEVEL_TR_RX);
	OUT(tx_sw, GPIO_LEVEL_TX_SW_RX);
#undef OUT
	return no_os_gpio_direction_input(g->muxout);
}

static void board_gpio_remove(struct board_gpios *g)
{
	struct no_os_gpio_desc *all[] = {g->vctrl1, g->vctrl2, g->div_mr,
		g->div_s0, g->div_s1, g->div_s2, g->rx_load, g->tr,
		g->tx_sw, g->muxout, g->burst};
	uint32_t i;
	for (i = 0; i < sizeof(all) / sizeof(all[0]); i++)
		if (all[i])
			no_os_gpio_remove(all[i]);
}

static int32_t show_gpio(const char *name, uint32_t pin,
			 struct no_os_gpio_desc *desc, int expected)
{
	uint8_t value = 0U;
	int32_t ret = no_os_gpio_get_value(desc, &value);
	if (ret)
		return ret;
	printf("  %-8s GPIO%-2u = %u", name, pin, value);
	if (expected >= 0)
		printf(" %s", value == (uint8_t)expected ? "OK" : "MISMATCH");
	printf("\n");
	return 0;
}

static int32_t board_gpio_verify(struct board_gpios *g)
{
	int32_t status = 0;
#define SHOW(name, pin, member, expected) do { if (show_gpio(name, pin, g->member, expected)) status = -EIO; } while (0)
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
		.spi_init = {
			.device_id = ADAR1000_SPI_DEVICE,
			.max_speed_hz = ADAR1000_SPI_SPEED,
			.chip_select = chip_select,
			.mode = NO_OS_SPI_MODE_0,
			.platform_ops = SPI_OPS,
			.extra = NULL,
		},
		.dev_addr = ADAR1000_DEV_ADDR,
	};
	uint16_t product_id;
	uint8_t ch;
	int32_t ret;

	ret = adar1000_init(dev, &p);
	if (ret)
		return ret;

	ret = adar1000_read_product_id(*dev, &product_id);
	if (ret)
		goto error;
	printf("ADAR1000 CS%u product ID: 0x%04X\n", chip_select, product_id);

	ret = adar1000_setup_rx(*dev);
	if (ret)
		goto error;

	/* Gain and phase are both holding registers. Initialize both before load. */
	for (ch = 0; ch < ADAR1000_NUM_CHANNELS; ch++) {
		ret = adar1000_set_rx_gain(*dev, ch, gain);
		if (ret)
			goto error;
		ret = adar1000_set_rx_phase(*dev, ch, INITIAL_PHASE_DEG);
		if (ret)
			goto error;
	}

	ret = adar1000_latch_rx(*dev);
	if (ret)
		goto error;

	return adar1000_verify_rx(*dev,
		chip_select == ADAR1000_SPI_CS_0 ? "ADAR1000 #0" : "ADAR1000 #1");
error:
	adar1000_remove(*dev);
	*dev = NULL;
	return ret;
}

static int32_t read_gain(struct adar1000_dev *dev, unsigned chip, unsigned ch)
{
	uint8_t raw;
	int32_t ret = adar1000_get_rx_gain_raw(dev, (uint8_t)ch, &raw);
	if (ret)
		return ret;
	printf("Chip %u ch %u: raw=0x%02X VGA=%u ATTN=%s\n", chip, ch, raw,
	       raw & ADAR1000_RX_GAIN_MASK,
	       (raw & ADAR1000_CH_ATTN_BYPASS) ? "BYPASS" : "INSERT");
	return 0;
}

int main(void)
{
	struct board_gpios gpios = {0};
	struct adar1000_dev *adar[NUM_ADAR1000] = {NULL, NULL};
	struct adf4159_dev *pll = NULL;
	char cmd[MAX_CMD_LEN];
	int32_t ret;
	uint8_t lock = 0U;
	uint32_t i;

	setlinebuf(stdout);
	ret = board_gpio_init(&gpios);
	if (ret) {
		printf("ERR: GPIO initialization failed: %ld\n", (long)ret);
		goto cleanup;
	}
	board_gpio_verify(&gpios);

	ret = init_adar1000_rx(&adar[0], ADAR1000_SPI_CS_0, INITIAL_GAIN);
	if (ret) { printf("ERR: ADAR1000 #0 init failed: %ld\n", (long)ret); goto cleanup; }
	ret = init_adar1000_rx(&adar[1], ADAR1000_SPI_CS_1, INITIAL_GAIN);
	if (ret) { printf("ERR: ADAR1000 #1 init failed: %ld\n", (long)ret); goto cleanup; }

	{
		struct adf4159_init_param p = {
			.spi_init = { .device_id = ADF4159_SPI_DEVICE,
				.max_speed_hz = ADF4159_SPI_SPEED,
				.chip_select = ADF4159_SPI_CS,
				.mode = NO_OS_SPI_MODE_0,
				.platform_ops = SPI_OPS, .extra = NULL },
			.gpio_le = { .number = ADF4159_GPIO_LE,
				.platform_ops = GPIO_OPS, .extra = NULL },
			.gpio_ce = { .number = -1, .platform_ops = GPIO_OPS,
				.extra = NULL },
			.config = adf4159_default_cfg,
		};
		ret = adf4159_init(&pll, &p);
	}
	if (ret) { printf("ERR: ADF4159 init failed: %ld\n", (long)ret); goto cleanup; }

	/* MUXOUT must be digital lock detect before lock status is meaningful. */
	ret = adf4159_set_muxout(pll, 6U);
	if (ret) { printf("ERR: setting PLL MUXOUT failed: %ld\n", (long)ret); goto cleanup; }
	no_os_mdelay(10U);
	ret = no_os_gpio_get_value(gpios.muxout, &lock);
	if (ret) { printf("ERR: PLL lock GPIO read failed: %ld\n", (long)ret); goto cleanup; }
	printf("PLL lock at startup: %u\n", lock);

	printf("READY\n");
	while (fgets(cmd, sizeof(cmd), stdin)) {
		unsigned chip, ch, val;
		unsigned long long freq;
		cmd[strcspn(cmd, "\r\n")] = '\0';

		if (sscanf(cmd, "gain %u %u %u", &chip, &ch, &val) == 3) {
			if (chip >= NUM_ADAR1000 || ch >= ADAR1000_NUM_CHANNELS || val > 127U) {
				printf("ERR: gain <chip 0-1> <ch 0-3> <value 0-127>\n"); continue;
			}
			ret = adar1000_set_rx_gain(adar[chip], (uint8_t)ch, (uint8_t)val);
			if (!ret) ret = adar1000_latch_rx(adar[chip]);
			if (!ret) ret = read_gain(adar[chip], chip, ch);
			if (ret) printf("ERR: gain operation failed: %ld\n", (long)ret);
			else printf("Chip %u ch %u gain=%u\nOK\n", chip, ch, val);
		} else if (sscanf(cmd, "read_gain %u %u", &chip, &ch) == 2) {
			if (chip >= NUM_ADAR1000 || ch >= ADAR1000_NUM_CHANNELS) { printf("ERR: invalid chip/channel\n"); continue; }
			ret = read_gain(adar[chip], chip, ch);
			printf(ret ? "ERR: read failed: %ld\n" : "OK\n", (long)ret);
		} else if (sscanf(cmd, "phase %u %u %u", &chip, &ch, &val) == 3) {
			if (chip >= NUM_ADAR1000 || ch >= ADAR1000_NUM_CHANNELS || val >= 360U) { printf("ERR: phase <chip 0-1> <ch 0-3> <deg 0-359>\n"); continue; }
			ret = adar1000_set_rx_phase(adar[chip], (uint8_t)ch, (uint16_t)val);
			if (!ret) ret = adar1000_latch_rx(adar[chip]);
			if (ret) printf("ERR: phase operation failed: %ld\n", (long)ret);
			else printf("Chip %u ch %u phase=%u\nOK\n", chip, ch, val);
		} else if (sscanf(cmd, "gain_all %u", &val) == 1) {
			uint8_t gains[ADAR1000_NUM_CHANNELS];
			if (val > 127U) { printf("ERR: gain_all <0-127>\n"); continue; }
			for (i = 0; i < ADAR1000_NUM_CHANNELS; i++) gains[i] = (uint8_t)val;
			ret = adar1000_set_all_rx_gains(adar[0], gains);
			if (!ret) ret = adar1000_set_all_rx_gains(adar[1], gains);
			if (ret) printf("ERR: gain_all failed: %ld\n", (long)ret);
			else printf("All 8 channels gain=%u\nOK\n", val);
		} else if (sscanf(cmd, "phase_all %u", &val) == 1) {
			if (val >= 360U) { printf("ERR: phase_all <0-359>\n"); continue; }
			ret = 0;
			for (chip = 0; chip < NUM_ADAR1000 && !ret; chip++) {
				for (ch = 0; ch < ADAR1000_NUM_CHANNELS && !ret; ch++)
					ret = adar1000_set_rx_phase(adar[chip], (uint8_t)ch, (uint16_t)val);
				if (!ret) ret = adar1000_latch_rx(adar[chip]);
			}
			if (ret) printf("ERR: phase_all failed: %ld\n", (long)ret);
			else printf("All 8 channels phase=%u\nOK\n", val);
		} else if (sscanf(cmd, "verify %u", &chip) == 1) {
			if (chip >= NUM_ADAR1000) { printf("ERR: verify <chip 0-1>\n"); continue; }
			ret = adar1000_verify_rx(adar[chip], chip ? "ADAR1000 #1" : "ADAR1000 #0");
			printf(ret ? "ERR: verify failed: %ld\n" : "OK\n", (long)ret);
		} else if (sscanf(cmd, "pll_full %llu", &freq) == 1) {
			ret = adf4159_setup(pll, (uint64_t)freq);
			no_os_mdelay(SETTLE_MS);
			if (!ret) ret = no_os_gpio_get_value(gpios.muxout, &lock);
			if (ret) printf("ERR: pll_full failed: %ld\n", (long)ret);
			else printf("PLL=%llu lock=%u\nOK\n", freq, lock);
		} else if (sscanf(cmd, "pll %llu", &freq) == 1) {
			ret = adf4159_set_freq(pll, (uint64_t)freq);
			no_os_mdelay(SETTLE_MS);
			if (!ret) ret = no_os_gpio_get_value(gpios.muxout, &lock);
			if (ret) printf("ERR: pll failed: %ld\n", (long)ret);
			else printf("PLL=%llu lock=%u\nOK\n", freq, lock);
		} else if (!strcmp(cmd, "lock")) {
			ret = no_os_gpio_get_value(gpios.muxout, &lock);
			if (ret) printf("ERR: lock read failed: %ld\n", (long)ret);
			else printf("MUXOUT=%u %s\nOK\n", lock, lock ? "LOCKED" : "NOT_LOCKED");
		} else if (!strcmp(cmd, "gpios")) {
			ret = board_gpio_verify(&gpios);
			printf(ret ? "ERR: GPIO verification failed\n" : "OK\n");
		} else if (!strcmp(cmd, "dump")) {
			adf4159_dump_regs(pll); printf("OK\n");
		} else if (!strcmp(cmd, "quit")) {
			printf("OK\n"); break;
		} else {
			printf("ERR: unknown command\n");
		}
		no_os_mdelay(SETTLE_MS);
	}

cleanup:
	if (pll) adf4159_remove(pll);
	for (i = 0; i < NUM_ADAR1000; i++) if (adar[i]) adar1000_remove(adar[i]);
	board_gpio_remove(&gpios);
	return ret;
}
