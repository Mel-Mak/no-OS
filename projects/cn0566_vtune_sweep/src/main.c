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
#include "ad7291.h"
#include "adf4159.h"
#include "no_os_delay.h"
#include "no_os_i2c.h"
#include "no_os_spi.h"
#include "no_os_gpio.h"
#include "parameters.h"

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

/* Sweep parameters  matching vtune_sweep.py */
#define FREQ_START_GHZ	9500	/* 9.5 GHz in MHz */
#define FREQ_STOP_GHZ	12500	/* 12.5 GHz in MHz */
#define FREQ_STEP_GHZ	100	/* 100 MHz step */
#define RX_LO_MHZ	2000	/* 2.0 GHz rx_lo */
#define SETTLE_MS	500

#define VTUNE_CHANNEL	7

int main(void)
{
	struct ad7291_desc *adc_dev;
	struct adf4159_dev *pll_dev;
	int32_t millivolts;
	int32_t scaled_mv;
	uint8_t ch;
	int32_t freq_mhz;
	double pll_freq_mhz;
	double actual_freq;
	int ret;

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

	/* ---- ADF4159 init (SPI + GPIO) ---- */
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
	};

	ret = adf4159_init(&pll_dev, pll_init);
	if (ret) {
		printf("ADF4159 init failed: %d\n", ret);
		ad7291_remove(adc_dev);
		return ret;
	}
	printf("ADF4159 initialized on SPI-%d, LE=GPIO%d\n",
	       ADF4159_SPI_DEVICE, ADF4159_GPIO_LE);

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

	/* ---- VTune sweep ---- */
	printf("\nSweeping LO from %d.%d GHz to %d.%d GHz...\n",
	       FREQ_START_GHZ / 1000, (FREQ_START_GHZ % 1000) / 100,
	       FREQ_STOP_GHZ / 1000, (FREQ_STOP_GHZ % 1000) / 100);

	for (freq_mhz = FREQ_START_GHZ; freq_mhz <= FREQ_STOP_GHZ;
	     freq_mhz += FREQ_STEP_GHZ) {
		/*
		 * Match Python vtune_sweep.py: lo_freq = signal_freq + rx_lo,
		 * then SDR_LO_init divides by 4 for the external prescaler.
		 * ADF4159 output = (signal_freq + rx_lo) / 4
		 */
		pll_freq_mhz = (double)(freq_mhz + RX_LO_MHZ) / 4.0;

		actual_freq = adf4159_set_freq(pll_dev, pll_freq_mhz);
		if (actual_freq < 0) {
			printf("  %d.%02d GHz -> set_freq FAILED\n",
			       freq_mhz / 1000, (freq_mhz % 1000) / 10);
			continue;
		}

		no_os_mdelay(SETTLE_MS);

		ret = ad7291_read_channel_voltage(adc_dev, VTUNE_CHANNEL,
						  &millivolts);
		if (ret) {
			printf("  %d.%02d GHz -> VTune read error %d\n",
			       freq_mhz / 1000, (freq_mhz % 1000) / 10, ret);
			continue;
		}

		scaled_mv = millivolts * ch_scale_x1000[VTUNE_CHANNEL] / 1000;
		printf("  %d.%02d GHz -> VTune = %d.%03d V (PLL out = %.1f MHz)\n",
		       freq_mhz / 1000, (freq_mhz % 1000) / 10,
		       (int)(scaled_mv / 1000),
		       (int)(scaled_mv % 1000),
		       actual_freq);
	}

	printf("\nSweep complete.\n");

	adf4159_remove(pll_dev);
	ad7291_remove(adc_dev);

	return 0;
}
