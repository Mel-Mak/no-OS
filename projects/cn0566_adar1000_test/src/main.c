/***************************************************************************//**
 *   @file   main.c
 *   @brief  CN0566 ADAR1000 Test: Set phase and gain on ADAR1000
 *   @author Melissa Makonga
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
#include "adar1000.h"
#include "no_os_spi.h"
#include "parameters.h"
#include "adf4159.h"
#include "adf4159_cfg.h"

int main(int argc, char *argv[])
{
	int ret;
	uint8_t gain = 0;
	uint16_t phase = 0;

	if (argc < 3) {
		printf("Usage: %s <gain 0-127> <phase 0-359>\n", argv[0]);
		return -1;
	}

	gain = (uint8_t)atoi(argv[1]);
	phase = (uint16_t)atoi(argv[2]);

	if (gain > 127) {
		printf("Gain must be 0-127, got %d\n", gain);
		return -1;
	}
	if (phase >= 360) {
		printf("Phase must be 0-359, got %d\n", phase);
		return -1;
	}

	printf("Setting gain=%d, phase=%d degrees on all 4 channels\n", gain, phase);

	/* ---- ADF4159 init (SPI + GPIO) ----
	 * adf4159_init() calls adf4159_setup() which writes all registers
	 */
	struct adf4159_dev *pll_dev;
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
		return ret;
	}
	if (ADF4159_GPIO_LE >= 0)
		printf("ADF4159 initialized on SPI-%d, LE=GPIO%d (manual toggle)\n",
		       ADF4159_SPI_DEVICE, ADF4159_GPIO_LE);
	else
		printf("ADF4159 initialized on SPI-%d, LE=CS%d (hardware-managed)\n",
		       ADF4159_SPI_DEVICE, ADF4159_SPI_CS);

    struct adar1000_dev *dev;
    struct adar1000_init_param adar_init = {
        .spi_init = {
            .device_id = ADAR1000_SPI_DEVICE,
            .max_speed_hz = ADAR1000_SPI_SPEED,
            .chip_select = ADAR1000_SPI_CS_0,
            .mode = NO_OS_SPI_MODE_0,
            .platform_ops = SPI_OPS,
            .extra = NULL,
        },
        .dev_addr = ADAR1000_DEV_ADDR,
    };

    /* Step 1: Init the device (this sets up SPI + does reset + scratch pad test) */
	ret = adar1000_init(&dev, &adar_init);
	if (ret) {
		printf("adar1000_init() failed with error code %d\n", ret);
		return ret;
	}
    printf("SPI communication verified!\n");

	/* Verify the scratch pad */
	uint8_t readback;

	adar1000_spi_write(dev, ADAR1000_SCRATCH_PAD, 0x42);
	adar1000_spi_read(dev, ADAR1000_SCRATCH_PAD, &readback);
	printf("Scratch pad: wrote 0x42, read 0x%02X\n", readback);

	/*Set ut rx*/
	ret = adar1000_setup_rx(dev);
	if (ret) {
		printf("adar1000_setup_rx() failed with error code %d\n", ret);
		adar1000_remove(dev);
		return ret;
	}
	printf("Rx setup complete!\n");

	/*set RX gain for each channel*/
	for (uint8_t ch = 0; ch < 4; ch++) {
		ret = adar1000_set_rx_gain(dev, ch, gain);
		if (ret) {
			printf("adar1000_set_rx_gain() failed with error code %d\n", ret);
			adar1000_remove(dev);
			return ret;
		}
	}

	/* Set RX phase for each channel */
	for (uint8_t ch = 0; ch < 4; ch++) {
		ret = adar1000_set_rx_phase(dev, ch, phase);
		if (ret) {
			printf("adar1000_set_rx_phase() failed with error code %d\n", ret);
			adar1000_remove(dev);
			return ret;
		}
	}
	/* Latch RX phase */
	ret = adar1000_latch_rx(dev);
	if (ret) {
		printf("adar1000_latch_rx() failed with error code %d\n", ret);
		adar1000_remove(dev);
		return ret;
	}
	printf("Rx phase settings latched!\n");

	/* Cleanup */
	adar1000_remove(dev);
	adf4159_remove(pll_dev);

    return 0;
}
