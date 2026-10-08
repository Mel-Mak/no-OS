/***************************************************************************//**
 * @file   main.c
 * @brief  CN0566 ADAR1000/ADF4159 interactive validation server.
 * @author Melissa Makonga (melissa.makonga@analog.com)
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

/******************************************************************************/
/************************ GPIO helper functions *******************************/
/******************************************************************************/

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

#define GET_GPIO(member, number)					\
	do {								\
		ret = gpio_open(&g->member, number);			\
		if (ret)						\
			return ret;					\
	} while (0)

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

#define OUT(member, value)						\
	do {								\
		ret = no_os_gpio_direction_output(g->member, value);	\
		if (ret)						\
			return ret;					\
	} while (0)

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
	struct no_os_gpio_desc *all[] = {
		g->vctrl1,
		g->vctrl2,
		g->div_mr,
		g->div_s0,
		g->div_s1,
		g->div_s2,
		g->rx_load,
		g->tr,
		g->tx_sw,
		g->muxout,
		g->burst
	};
	uint32_t i;

	for (i = 0U; i < sizeof(all) / sizeof(all[0]); i++) {
		if (all[i])
			no_os_gpio_remove(all[i]);
	}
}

static int32_t show_gpio(const char *name,
			 uint32_t pin,
			 struct no_os_gpio_desc *desc,
			 int expected)
{
	uint8_t value = 0U;
	int32_t ret;

	ret = no_os_gpio_get_value(desc, &value);
	if (ret)
		return ret;

	printf("  %-8s GPIO%-2u = %u", name, pin, value);

	if (expected >= 0) {
		printf(" %s",
		       value == (uint8_t)expected ? "OK" : "MISMATCH");
	}

	printf("\n");

	return 0;
}

static int32_t board_gpio_verify(struct board_gpios *g)
{
	int32_t status = 0;

#define SHOW(name, pin, member, expected)				\
	do {								\
		if (show_gpio(name, pin, g->member, expected))		\
			status = -EIO;					\
	} while (0)

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

/******************************************************************************/
/********************** ADAR1000 phase helper functions ***********************/
/******************************************************************************/

/**
 * Set the same RX phase on one or more consecutive ADAR1000 channels.
 *
 * The requested phase values are written to the channel holding registers.
 * One RX latch is issued after all requested channel writes complete.
 *
 * Examples:
 *
 * Set channel 2 only:
 *     set_adar_rx_phase(dev, 2, 1, 180);
 *
 * Set channels 0 and 1:
 *     set_adar_rx_phase(dev, 0, 2, 180);
 *
 * Set all four channels:
 *     set_adar_rx_phase(dev, 0, ADAR1000_NUM_CHANNELS, 180);
 *
 * @param dev          ADAR1000 device descriptor.
 * @param first_ch     First channel to update.
 * @param num_channels Number of consecutive channels to update.
 * @param phase_deg    Phase in degrees from 0 through 359.
 *
 * @return 0 on success or a negative error code.
 */
static int32_t set_adar_rx_phase(struct adar1000_dev *dev,
				 uint8_t first_ch,
				 uint8_t num_channels,
				 uint16_t phase_deg)
{
	uint8_t ch;
	uint8_t end_ch;
	int32_t ret;

	if (!dev)
		return -EINVAL;

	if (phase_deg >= 360U)
		return -EINVAL;

	if (num_channels == 0U)
		return -EINVAL;

	if (first_ch >= ADAR1000_NUM_CHANNELS)
		return -EINVAL;

	/*
	 * Validate the requested range before calculating end_ch.
	 *
	 * This also guarantees that first_ch + num_channels remains
	 * within the valid channel range.
	 */
	if (num_channels > (ADAR1000_NUM_CHANNELS - first_ch))
		return -EINVAL;

	end_ch = first_ch + num_channels;

	for (ch = first_ch; ch < end_ch; ch++) {
		ret = adar1000_set_rx_phase(dev, ch, phase_deg);
		if (ret)
			return ret;
	}

	return adar1000_latch_rx(dev);
}

/**
 * Set the phase of one ADAR1000 RX channel.
 */
static int32_t set_adar_rx_channel_phase(struct adar1000_dev *dev,
					 uint8_t ch,
					 uint16_t phase_deg)
{
	return set_adar_rx_phase(dev, ch, 1U, phase_deg);
}

/**
 * Set all RX channels of one ADAR1000 to the same phase.
 */
static int32_t set_adar_all_rx_phases(struct adar1000_dev *dev,
				      uint16_t phase_deg)
{
	return set_adar_rx_phase(dev,
				 0U,
				 ADAR1000_NUM_CHANNELS,
				 phase_deg);
}

/**
 * Set all RX channels on all ADAR1000 devices to the same phase.
 *
 * Each ADAR1000 is written and latched independently.
 */
static int32_t set_all_adar_rx_phases(struct adar1000_dev *adar[],
				      uint8_t num_devices,
				      uint16_t phase_deg)
{
	uint8_t chip;
	int32_t ret;

	if (!adar || num_devices == 0U)
		return -EINVAL;

	for (chip = 0U; chip < num_devices; chip++) {
		if (!adar[chip])
			return -EINVAL;

		ret = set_adar_all_rx_phases(adar[chip], phase_deg);
		if (ret)
			return ret;
	}

	return 0;
}

/******************************************************************************/
/************************ ADAR1000 initialization *****************************/
/******************************************************************************/

static int32_t init_adar1000_rx(struct adar1000_dev **dev,
				uint8_t chip_select,
				uint8_t gain)
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

	printf("ADAR1000 CS%u product ID: 0x%04X\n",
	       chip_select,
	       product_id);

	ret = adar1000_setup_rx(*dev);
	if (ret)
		goto error;

	/*
	 * Gain and phase are holding-register values.
	 *
	 * Write all gain values first. set_adar_all_rx_phases() then
	 * writes all phase values and performs one RX latch. That latch
	 * activates both the gain and phase settings.
	 */
	for (ch = 0U; ch < ADAR1000_NUM_CHANNELS; ch++) {
		ret = adar1000_set_rx_gain(*dev, ch, gain);
		if (ret)
			goto error;
	}

	ret = set_adar_all_rx_phases(*dev, INITIAL_PHASE_DEG);
	if (ret)
		goto error;

	return adar1000_verify_rx(
		*dev,
		chip_select == ADAR1000_SPI_CS_0 ?
		"ADAR1000 #0" : "ADAR1000 #1"
	);

error:
	adar1000_remove(*dev);
	*dev = NULL;

	return ret;
}

/******************************************************************************/
/************************ ADAR1000 gain diagnostics ***************************/
/******************************************************************************/

static int32_t read_gain(struct adar1000_dev *dev,
			 unsigned chip,
			 unsigned ch)
{
	uint8_t raw;
	int32_t ret;

	ret = adar1000_get_rx_gain_raw(dev, (uint8_t)ch, &raw);
	if (ret)
		return ret;

	printf("Chip %u ch %u: raw=0x%02X VGA=%u ATTN=%s\n",
	       chip,
	       ch,
	       raw,
	       raw & ADAR1000_RX_GAIN_MASK,
	       (raw & ADAR1000_CH_ATTN_BYPASS) ?
	       "BYPASS" : "INSERT");

	return 0;
}

/******************************************************************************/
/******************************** Main ***************************************/
/******************************************************************************/

int main(void)
{
	struct board_gpios gpios = {0};
	struct adar1000_dev *adar[NUM_ADAR1000] = {NULL, NULL};
	struct adf4159_dev *pll = NULL;
	char cmd[MAX_CMD_LEN];
	int32_t ret = 0;
	uint8_t lock = 0U;
	uint32_t i;

	setlinebuf(stdout);

	ret = board_gpio_init(&gpios);
	if (ret) {
		printf("ERR: GPIO initialization failed: %ld\n",
		       (long)ret);
		goto cleanup;
	}

	ret = board_gpio_verify(&gpios);
	if (ret) {
		printf("ERR: GPIO verification failed: %ld\n",
		       (long)ret);
		goto cleanup;
	}

	ret = init_adar1000_rx(&adar[0],
			       ADAR1000_SPI_CS_0,
			       INITIAL_GAIN);
	if (ret) {
		printf("ERR: ADAR1000 #0 init failed: %ld\n",
		       (long)ret);
		goto cleanup;
	}

	ret = init_adar1000_rx(&adar[1],
			       ADAR1000_SPI_CS_1,
			       INITIAL_GAIN);
	if (ret) {
		printf("ERR: ADAR1000 #1 init failed: %ld\n",
		       (long)ret);
		goto cleanup;
	}

	{
		struct adf4159_init_param p = {
			.spi_init = {
				.device_id = ADF4159_SPI_DEVICE,
				.max_speed_hz = ADF4159_SPI_SPEED,
				.chip_select = ADF4159_SPI_CS,
				.mode = NO_OS_SPI_MODE_0,
				.platform_ops = SPI_OPS,
				.extra = NULL
			},
			.gpio_le = {
				.number = ADF4159_GPIO_LE,
				.platform_ops = GPIO_OPS,
				.extra = NULL
			},
			.gpio_ce = {
				.number = -1,
				.platform_ops = GPIO_OPS,
				.extra = NULL
			},
			.config = adf4159_default_cfg,
		};

		ret = adf4159_init(&pll, &p);
	}

	if (ret) {
		printf("ERR: ADF4159 init failed: %ld\n",
		       (long)ret);
		goto cleanup;
	}

	/*
	 * MUXOUT must be configured as digital lock detect before its
	 * GPIO level can be interpreted as the PLL lock status.
	 */
	ret = adf4159_set_muxout(pll, 6U);
	if (ret) {
		printf("ERR: setting PLL MUXOUT failed: %ld\n",
		       (long)ret);
		goto cleanup;
	}

	no_os_mdelay(10U);

	ret = no_os_gpio_get_value(gpios.muxout, &lock);
	if (ret) {
		printf("ERR: PLL lock GPIO read failed: %ld\n",
		       (long)ret);
		goto cleanup;
	}

	printf("PLL lock at startup: %u\n", lock);
	printf("READY\n");

	while (fgets(cmd, sizeof(cmd), stdin)) {
		unsigned chip;
		unsigned ch;
		unsigned count;
		unsigned val;
		unsigned long long freq;

		cmd[strcspn(cmd, "\r\n")] = '\0';

		/******************************************************************/
		/* Set gain on one ADAR1000 channel.                              */
		/******************************************************************/

		if (sscanf(cmd, "gain %u %u %u",
			   &chip, &ch, &val) == 3) {
			if (chip >= NUM_ADAR1000 ||
			    ch >= ADAR1000_NUM_CHANNELS ||
			    val > 127U) {
				printf(
					"ERR: gain <chip 0-1> "
					"<ch 0-3> <value 0-127>\n"
				);
				continue;
			}

			ret = adar1000_set_rx_gain(
				adar[chip],
				(uint8_t)ch,
				(uint8_t)val
			);

			if (!ret)
				ret = adar1000_latch_rx(adar[chip]);

			if (!ret)
				ret = read_gain(adar[chip], chip, ch);

			if (ret) {
				printf(
					"ERR: gain operation failed: %ld\n",
					(long)ret
				);
			} else {
				printf(
					"Chip %u ch %u gain=%u\n",
					chip,
					ch,
					val
				);
				printf("OK\n");
			}

		/******************************************************************/
		/* Read gain state from one ADAR1000 channel.                     */
		/******************************************************************/

		} else if (sscanf(cmd, "read_gain %u %u",
				  &chip, &ch) == 2) {
			if (chip >= NUM_ADAR1000 ||
			    ch >= ADAR1000_NUM_CHANNELS) {
				printf("ERR: invalid chip/channel\n");
				continue;
			}

			ret = read_gain(adar[chip], chip, ch);

			if (ret)
				printf("ERR: read failed: %ld\n", (long)ret);
			else
				printf("OK\n");

		/******************************************************************/
		/* Set phase on one ADAR1000 channel.                             */
		/******************************************************************/

		} else if (sscanf(cmd, "phase %u %u %u",
				  &chip, &ch, &val) == 3) {
			if (chip >= NUM_ADAR1000 ||
			    ch >= ADAR1000_NUM_CHANNELS ||
			    val >= 360U) {
				printf(
					"ERR: phase <chip 0-1> "
					"<ch 0-3> <deg 0-359>\n"
				);
				continue;
			}

			ret = set_adar_rx_channel_phase(
				adar[chip],
				(uint8_t)ch,
				(uint16_t)val
			);

			if (ret) {
				printf(
					"ERR: phase operation failed: %ld\n",
					(long)ret
				);
			} else {
				printf(
					"Chip %u ch %u phase=%u\n",
					chip,
					ch,
					val
				);
				printf("OK\n");
			}



		/******************************************************************/
		/* Set all four channels of one ADAR1000.                         */
		/******************************************************************/

		} else if (sscanf(cmd, "phase_chip %u %u",
				  &chip, &val) == 2) {
			if (chip >= NUM_ADAR1000 ||
			    val >= 360U) {
				printf(
					"ERR: phase_chip "
					"<chip 0-1> <deg 0-359>\n"
				);
				continue;
			}

			ret = set_adar_all_rx_phases(
				adar[chip],
				(uint16_t)val
			);

			if (ret) {
				printf(
					"ERR: phase_chip failed: %ld\n",
					(long)ret
				);
			} else {
				printf(
					"Chip %u all channels phase=%u\n",
					chip,
					val
				);
				printf("OK\n");
			}

		/******************************************************************/
		/* Set the same gain on all eight ADAR1000 channels.              */
		/******************************************************************/

		} else if (sscanf(cmd, "gain_all %u", &val) == 1) {
			uint8_t gains[ADAR1000_NUM_CHANNELS];

			if (val > 127U) {
				printf("ERR: gain_all <0-127>\n");
				continue;
			}

			for (i = 0U; i < ADAR1000_NUM_CHANNELS; i++)
				gains[i] = (uint8_t)val;

			ret = adar1000_set_all_rx_gains(adar[0], gains);

			if (!ret)
				ret = adar1000_set_all_rx_gains(
					adar[1],
					gains
				);

			if (ret) {
				printf(
					"ERR: gain_all failed: %ld\n",
					(long)ret
				);
			} else {
				printf(
					"All %u channels gain=%u\n",
					NUM_ADAR1000 *
					ADAR1000_NUM_CHANNELS,
					val
				);
				printf("OK\n");
			}

		/******************************************************************/
		/* Set the same phase on all eight ADAR1000 channels.             */
		/******************************************************************/

		} else if (sscanf(cmd, "phase_all %u", &val) == 1) {
			if (val >= 360U) {
				printf("ERR: phase_all <deg 0-359>\n");
				continue;
			}

			ret = set_all_adar_rx_phases(
				adar,
				NUM_ADAR1000,
				(uint16_t)val
			);

			if (ret) {
				printf(
					"ERR: phase_all failed: %ld\n",
					(long)ret
				);
			} else {
				printf(
					"All %u channels phase=%u\n",
					NUM_ADAR1000 *
					ADAR1000_NUM_CHANNELS,
					val
				);
				printf("OK\n");
			}

		/******************************************************************/
		/* Verify one ADAR1000.                                           */
		/******************************************************************/

		} else if (sscanf(cmd, "verify %u", &chip) == 1) {
			if (chip >= NUM_ADAR1000) {
				printf("ERR: verify <chip 0-1>\n");
				continue;
			}

			ret = adar1000_verify_rx(
				adar[chip],
				chip ?
				"ADAR1000 #1" :
				"ADAR1000 #0"
			);

			if (ret)
				printf(
					"ERR: verify failed: %ld\n",
					(long)ret
				);
			else
				printf("OK\n");

		/******************************************************************/
		/* Fully configure the ADF4159 output frequency.                  */
		/******************************************************************/

		} else if (sscanf(cmd, "pll_full %llu", &freq) == 1) {
			ret = adf4159_setup(pll, (uint64_t)freq);

			no_os_mdelay(SETTLE_MS);

			if (!ret)
				ret = no_os_gpio_get_value(
					gpios.muxout,
					&lock
				);

			if (ret) {
				printf(
					"ERR: pll_full failed: %ld\n",
					(long)ret
				);
			} else {
				printf(
					"PLL=%llu lock=%u\nOK\n",
					freq,
					lock
				);
			}

		/******************************************************************/
		/* Update the ADF4159 output frequency.                           */
		/******************************************************************/

		} else if (sscanf(cmd, "pll %llu", &freq) == 1) {
			ret = adf4159_set_freq(pll, (uint64_t)freq);

			no_os_mdelay(SETTLE_MS);

			if (!ret)
				ret = no_os_gpio_get_value(
					gpios.muxout,
					&lock
				);

			if (ret) {
				printf(
					"ERR: pll failed: %ld\n",
					(long)ret
				);
			} else {
				printf(
					"PLL=%llu lock=%u\nOK\n",
					freq,
					lock
				);
			}

		/******************************************************************/
		/* Read the ADF4159 digital lock-detect GPIO.                     */
		/******************************************************************/

		} else if (!strcmp(cmd, "lock")) {
			ret = no_os_gpio_get_value(
				gpios.muxout,
				&lock
			);

			if (ret) {
				printf(
					"ERR: lock read failed: %ld\n",
					(long)ret
				);
			} else {
				printf(
					"MUXOUT=%u %s\nOK\n",
					lock,
					lock ?
					"LOCKED" :
					"NOT_LOCKED"
				);
			}

		/******************************************************************/
		/* Verify the board GPIO states.                                  */
		/******************************************************************/

		} else if (!strcmp(cmd, "gpios")) {
			ret = board_gpio_verify(&gpios);

			if (ret)
				printf("ERR: GPIO verification failed\n");
			else
				printf("OK\n");

		/******************************************************************/
		/* Dump PLL register state.                                       */
		/******************************************************************/

		} else if (!strcmp(cmd, "dump")) {
			/*
			 * Keep the original command behavior: dump the ADF4159
			 * register values.
			 */
			adf4159_dump_regs(pll);
			printf("OK\n");

		/******************************************************************/
		/* Exit the command server.                                       */
		/******************************************************************/

		} else if (!strcmp(cmd, "quit")) {
			printf("OK\n");
			break;

		/******************************************************************/
		/* Unsupported command.                                          */
		/******************************************************************/

		} else {
			printf("ERR: unknown command\n");
		}

		no_os_mdelay(SETTLE_MS);
	}

cleanup:
	if (pll)
		adf4159_remove(pll);

	for (i = 0U; i < NUM_ADAR1000; i++) {
		if (adar[i])
			adar1000_remove(adar[i]);
	}

	board_gpio_remove(&gpios);

	return ret;
}
