/***************************************************************************//**
 *   @file   main.c
 *   @brief  CN0566 VTune sweep sweeps ADF4159 LO across frequency and reads
 *           VTune voltage from AD7291 at each step.
 *           Replicates the Python vtune_sweep.py for verifying the No-OS port.
 *   @author Melissa Makonga
 *
 ********************************************************************************
 * Copyright 2025(c) Analog Devices, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of Analog Devices, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES, INC. "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL ANALOG DEVICES, INC. BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "ad7291.h"
#include "adf4159.h"
#include "adf4159_cfg.h"
#include "no_os_delay.h"
#include "no_os_i2c.h"
#include "no_os_spi.h"
#include "no_os_gpio.h"
#include "parameters.h"
#include <string.h>

/* CN0566 resistor-divider scale factors (x1000 for integer math).
 * VTune is channel 7: 1 + 69.8k/10k = 7.98 */
static const int32_t ch_scale_x1000[] = {
	2000,	/* CH0: VDD1V8   1 + 10k/10k = 2.0 */
	2000,	/* CH1: VDD3V0   1 + 10k/10k = 2.0 */
	2000,	/* CH2: VDD3V3   1 + 10k/10k = 2.0 */
	4010,	/* CH3: VDD4V5   1 + 30.1k/10k = 4.01 */
	7980,	/* CH4: VDD_AMP  1 + 69.8k/10k = 7.98 */
	4010,	/* CH5: VINPUT   1 + 30.1k/10k = 4.01 */
	1000,	/* CH6: IMON     1.0 */
	7980,	/* CH7: VTUNE    1 + 69.8k/10k = 7.98 */
};

static const char *ch_labels[] = {
	"VDD1V8 ",
	"VDD3V0 ",
	"VDD3V3 ",
	"VDD4V5 ",
	"VDD_AMP",
	"VINPUT ",
	"IMON   ",
	"VTUNE  ",
};

/* Sweep: ADF4159 output in Hz (LO = PLL freq * 4) */
#define SDR_RX_LO_HZ     2000000000ULL
#define SIGNAL_START_HZ   9500000000ULL
#define SIGNAL_STOP_HZ   12500000000ULL
#define SIGNAL_STEP_HZ    100000000ULL
#define SETTLE_MS	500

#define VTUNE_CHANNEL	7

int main(void)
{
	struct ad7291_desc *adc_dev;
	struct adf4159_dev *pll_dev;
	int32_t millivolts;
	int32_t scaled_mv;
	uint8_t ch;
	uint64_t pll_freq_hz;
	int ret;
	u_int64_t signal_freq;

	/* ---- AD7291 init (I2C) ---- */
	struct linux_i2c_init_param linux_i2c_extra = {
		.device_id = AD7291_I2C_BUS,
	};
	struct ad7291_init_param adc_init = {
		.i2c_init = {
			.device_id = AD7291_I2C_BUS,
			.max_speed_hz = 400000,
			.slave_address = AD7291_I2C_ADDR,
			.platform_ops = I2C_OPS,
			.extra = &linux_i2c_extra,
		},
		.vref_mv = 0,
	};

	ret = ad7291_init(&adc_dev, &adc_init);
	if (ret) {
		printf("AD7291 init failed: %d\n", ret);
		return ret;
	}
	printf("AD7291 initialized on I2C-%d at 0x%02X\n",
	       AD7291_I2C_BUS, AD7291_I2C_ADDR);

	/* ---- Board GPIOs FIRST (match Linux probe order: one-bit-adc-dac
	 * probes before ADF4159 IIO driver) ---- */
	struct no_os_gpio_desc *gpio_vctrl1, *gpio_vctrl2;
	struct no_os_gpio_desc *gpio_div_mr, *gpio_div_s0, *gpio_div_s1, *gpio_div_s2;
	struct no_os_gpio_desc *gpio_rx_load, *gpio_tr, *gpio_tx_sw, *gpio_muxout;
	struct no_os_gpio_desc *gpio_burst;

	struct no_os_gpio_init_param gpio_params[] = {
		{ .number = GPIO_VCTRL_1, .platform_ops = GPIO_OPS, .extra = NULL },
		{ .number = GPIO_VCTRL_2, .platform_ops = GPIO_OPS, .extra = NULL },
		{ .number = GPIO_DIV_MR,  .platform_ops = GPIO_OPS, .extra = NULL },
		{ .number = GPIO_DIV_S0,  .platform_ops = GPIO_OPS, .extra = NULL },
		{ .number = GPIO_DIV_S1,  .platform_ops = GPIO_OPS, .extra = NULL },
		{ .number = GPIO_DIV_S2,  .platform_ops = GPIO_OPS, .extra = NULL },
		{ .number = GPIO_RX_LOAD, .platform_ops = GPIO_OPS, .extra = NULL },
		{ .number = GPIO_TR,      .platform_ops = GPIO_OPS, .extra = NULL },
		{ .number = GPIO_TX_SW,   .platform_ops = GPIO_OPS, .extra = NULL },
		{ .number = GPIO_MUXOUT,  .platform_ops = GPIO_OPS, .extra = NULL },
		{ .number = GPIO_BURST,   .platform_ops = GPIO_OPS, .extra = NULL },
	};

	struct no_os_gpio_desc **gpio_descs[] = {
		&gpio_vctrl1, &gpio_vctrl2,
		&gpio_div_mr, &gpio_div_s0, &gpio_div_s1, &gpio_div_s2,
		&gpio_rx_load, &gpio_tr, &gpio_tx_sw, &gpio_muxout,
		&gpio_burst,
	};

	for (int i = 0; i < 11; i++) {
		ret = no_os_gpio_get(gpio_descs[i], &gpio_params[i]);
		if (ret) {
			printf("GPIO %d init failed: %d\n",
			       gpio_params[i].number, ret);
			goto cleanup_adc;
		}
	}

	no_os_gpio_direction_output(gpio_burst, 1);
	no_os_gpio_direction_output(gpio_vctrl1, 1);
	no_os_gpio_direction_output(gpio_vctrl2, 1);
	no_os_gpio_direction_output(gpio_div_mr, 0);
	no_os_gpio_direction_output(gpio_div_s0, 0);
	no_os_gpio_direction_output(gpio_div_s1, 0);
	no_os_gpio_direction_output(gpio_div_s2, 0);
	no_os_gpio_direction_output(gpio_rx_load, 0);
	no_os_gpio_direction_output(gpio_tr, 0);
	no_os_gpio_direction_output(gpio_tx_sw, 0);
	no_os_gpio_direction_input(gpio_muxout);

	printf("Board GPIOs set BEFORE ADF4159 init:\n");
	printf("  BURST=1, VCTRL_1=1, VCTRL_2=1, DIV_MR=0, DIV_S0=0, DIV_S1=0, DIV_S2=0\n");
	printf("  RX_LOAD=0, TR=0, TX_SW=0, MUXOUT=input\n");

	/* ---- ADF4159 init (SPI + GPIO) ----
	 * Board GPIOs are already configured (matching Linux probe order).
	 * adf4159_init() calls adf4159_setup() which writes all registers
	 * R7?R0 (including SEL1 variants) and programs the power-up frequency.
	 */
	struct adf4159_init_param pll_init = {
		.spi_init = {
			.device_id = ADF4159_SPI_DEVICE,
			.max_speed_hz = ADF4159_SPI_SPEED,
			.chip_select = ADF4159_SPI_CS,
			.mode = NO_OS_SPI_MODE_0,
			.platform_ops = SPI_OPS,
			.extra = NULL,
		},
		.gpio_le = {
			.number = ADF4159_GPIO_LE,
			.platform_ops = GPIO_OPS,
			.extra = NULL,
		},
		.gpio_ce = {
			.number = -1,
			.platform_ops = GPIO_OPS,
			.extra = NULL,
		},
		.config = adf4159_default_cfg,
	};

	ret = adf4159_init(&pll_dev, &pll_init);
	if (ret) {
		printf("ADF4159 init failed: %d\n", ret);
		goto cleanup_gpios;
	}
	if (ADF4159_GPIO_LE >= 0)
		printf("ADF4159 initialized on SPI-%d, LE=GPIO%d (manual toggle)\n",
		       ADF4159_SPI_DEVICE, ADF4159_GPIO_LE);
	else
		printf("ADF4159 initialized on SPI-%d, LE=CS%d (hardware-managed)\n",
		       ADF4159_SPI_DEVICE, ADF4159_SPI_CS);

	// uint8_t muxout_val;
	// no_os_gpio_get_value(gpio_muxout, &muxout_val);
	// printf("  MUXOUT (lock detect): %d (%s)\n",
	//        muxout_val, muxout_val ? "LOCKED" : "UNLOCKED");

	// /* ---- GPIO pin function check ---- */
	// printf("\n--- GPIO PIN CHECK ---\n");
	// printf("GPIO 27 (LE/CS2) and GPIO 25 (MUXOUT):\n");
	// system("raspi-gpio get 27");
	// system("raspi-gpio get 25");
	// if (ADF4159_GPIO_LE < 0)
	// 	printf("GPIO 27 should show ALT0/SPI0_CE2_N (hardware-managed LE via overlay).\n");
	// else
	// 	printf("GPIO 27 should show func=OUTPUT (manual LE toggle via GPIO).\n");
	// printf("--- END GPIO PIN CHECK ---\n");

	/* ---- Initial monitor readings ---- */
	printf("\nInitial monitor readings:\n");
	for (ch = 0; ch < AD7291_NUM_CHANNELS; ch++) {
		ret = ad7291_read_channel_voltage(adc_dev, ch, &millivolts);
		if (ret) {
			printf("  CH%d %s: read error %d\n",
			       ch, ch_labels[ch], ret);
			continue;
		}
		scaled_mv = millivolts * ch_scale_x1000[ch] / 1000;
		printf("  CH%d %s: %d.%03d V\n", ch, ch_labels[ch],
		       (int)(scaled_mv / 1000),
		       (int)(scaled_mv % 1000));
	}

	/* ---- MUXOUT latch test: proves ADF4159 is receiving data ---- */
	// printf("\n--- MUXOUT LATCH TEST ---\n");
	// {
	// 	uint32_t r0_save = pll_dev->st.regs[ADF4159_REG0];

	// 	/* Force MUXOUT = DVDD (should read 1) */
	// 	uint32_t r0_dvdd = (r0_save & ~ADF4159_REG0_MUXOUT_MASK) |
	// 			   ADF4159_REG0_MUXOUT(ADF4159_MUXOUT_DVDD);
	// 	adf4159_write(pll_dev, r0_dvdd | ADF4159_REG0);
	// 	no_os_mdelay(10);
	// 	no_os_gpio_get_value(gpio_muxout, &muxout_val);
	// 	printf("MUXOUT=DVDD -> libgpiod=%d, ", muxout_val);
	// 	system("raspi-gpio get 25");

	// 	/* Force MUXOUT = DGND (should read 0) */
	// 	uint32_t r0_dgnd = (r0_save & ~ADF4159_REG0_MUXOUT_MASK) |
	// 			   ADF4159_REG0_MUXOUT(ADF4159_MUXOUT_DGND);
	// 	adf4159_write(pll_dev, r0_dgnd | ADF4159_REG0);
	// 	no_os_mdelay(10);
	// 	no_os_gpio_get_value(gpio_muxout, &muxout_val);
	// 	printf("MUXOUT=DGND -> libgpiod=%d, ", muxout_val);
	// 	system("raspi-gpio get 25");

	// 	/* Restore original MUXOUT */
	// 	adf4159_write(pll_dev, r0_save | ADF4159_REG0);
	// 	no_os_mdelay(10);
	// 	no_os_gpio_get_value(gpio_muxout, &muxout_val);
	// 	printf("MUXOUT=restored -> libgpiod=%d, ", muxout_val);
	// 	system("raspi-gpio get 25");
	// }
	// printf("If DVDD=1 and DGND=0, ADF4159 IS latching register writes.\n");
	// printf("--- END MUXOUT LATCH TEST ---\n");

	/* ---- SPI link diagnostic: toggle PLL power-down ---- */
	// printf("\n--- SPI LINK TEST ---\n");
	// ret = adf4159_set_freq(pll_dev, 2625000000ULL);
	// printf("Set PLL to 2625 MHz (10.5 GHz LO), ret=%d\n", ret);
	// no_os_mdelay(500);
	// ret = ad7291_read_channel_voltage(adc_dev, VTUNE_CHANNEL, &millivolts);
	// scaled_mv = millivolts * ch_scale_x1000[VTUNE_CHANNEL] / 1000;
	// no_os_gpio_get_value(gpio_muxout, &muxout_val);
	// printf("VTune (PLL active):      %d.%03d V  MUXOUT=%d\n",
	//        (int)(scaled_mv / 1000), (int)(scaled_mv % 1000), muxout_val);

	// /* Power down the PLL: set PD bit in R3 */
	// pll_dev->st.regs[ADF4159_REG3] |= ADF4159_REG3_PD(1);
	// adf4159_write(pll_dev, pll_dev->st.regs[ADF4159_REG3] | ADF4159_REG3);
	// printf("Wrote R3 with PD=1 (power down)\n");
	// no_os_mdelay(500);
	// ret = ad7291_read_channel_voltage(adc_dev, VTUNE_CHANNEL, &millivolts);
	// scaled_mv = millivolts * ch_scale_x1000[VTUNE_CHANNEL] / 1000;
	// no_os_gpio_get_value(gpio_muxout, &muxout_val);
	// printf("VTune (PLL powered down): %d.%03d V  MUXOUT=%d\n",
	//        (int)(scaled_mv / 1000), (int)(scaled_mv % 1000), muxout_val);

	/* Power back up: clear PD bit */
	// pll_dev->st.regs[ADF4159_REG3] &= ~ADF4159_REG3_PD(1);
	// adf4159_write(pll_dev, pll_dev->st.regs[ADF4159_REG3] | ADF4159_REG3);
	// printf("Wrote R3 with PD=0 (power up)\n");
	// no_os_mdelay(500);
	// ret = ad7291_read_channel_voltage(adc_dev, VTUNE_CHANNEL, &millivolts);
	// scaled_mv = millivolts * ch_scale_x1000[VTUNE_CHANNEL] / 1000;
	// no_os_gpio_get_value(gpio_muxout, &muxout_val);
	// printf("VTune (PLL restored):    %d.%03d V  MUXOUT=%d\n",
	//        (int)(scaled_mv / 1000), (int)(scaled_mv % 1000), muxout_val);
	// printf("--- END SPI LINK TEST ---\n");
	// printf("If all three VTune readings are identical, SPI data is NOT reaching the ADF4159.\n\n");

	/* ---- VTune sweep ---- */
	printf("\nSweeping PLL from %llu to %llu Hz (LO %.1f to %.1f GHz)...\n",
	       (unsigned long long)SIGNAL_START_HZ,
	       (unsigned long long)SIGNAL_STOP_HZ, SIGNAL_START_HZ,
	       SIGNAL_STOP_HZ * 4.0 / 1e9);

	for (signal_freq = SIGNAL_START_HZ; signal_freq <= SIGNAL_STOP_HZ;
     signal_freq += SIGNAL_STEP_HZ) {
		pll_freq_hz = (signal_freq + SDR_RX_LO_HZ) / 4;
		ret = adf4159_set_freq(pll_dev, pll_freq_hz);
		if (ret) {
			printf("  %llu Hz -> FAILED (%d)\n",
			       (unsigned long long)pll_freq_hz, ret);
			continue;
		}

		no_os_mdelay(SETTLE_MS);

		ret = ad7291_read_channel_voltage(adc_dev, VTUNE_CHANNEL,
						  &millivolts);
		if (ret) {
			printf("  %llu Hz -> VTune read error %d\n",
			       (unsigned long long)pll_freq_hz, ret);
			continue;
		}

		scaled_mv = millivolts * ch_scale_x1000[VTUNE_CHANNEL] / 1000;
		// no_os_gpio_get_value(gpio_muxout, &muxout_val);
		printf("  %.2f GHz -> VTune = %d.%03d V  \n",
		       signal_freq / 1e9,
		       (int)(scaled_mv / 1000),
		       (int)(scaled_mv % 1000));
	}

	printf("\nSweep complete.\n");

	adf4159_remove(pll_dev);
	for (int i = 0; i < 11; i++)
		no_os_gpio_remove(*gpio_descs[i]);
	ad7291_remove(adc_dev);

	return 0;

cleanup_gpios:
	for (int i = 0; i < 11; i++)
		no_os_gpio_remove(*gpio_descs[i]);
cleanup_adc:
	ad7291_remove(adc_dev);
	return ret;
}
