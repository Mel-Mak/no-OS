/***************************************************************************//**
 *   @file   main.c
 *   @brief  ADXL355 example for Raspberry Pi (Linux platform).
 *           Reads and prints accelerometer data in a loop.
 *   @author MMakonga (melissa.makonga@analog.com)
********************************************************************************
 * Copyright 2026(c) Analog Devices, Inc.
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
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "adxl355.h"
#include "no_os_delay.h"
#include "no_os_spi.h"
#include "linux_spi.h"

#define SPI_DEVICE_ID	0
#define SPI_CS		0

int main(void)
{
	struct adxl355_dev *dev;
	struct no_os_spi_init_param spi_ip = {
		.device_id = SPI_DEVICE_ID,
		.max_speed_hz = 1000000,
		.bit_order = NO_OS_SPI_BIT_ORDER_MSB_FIRST,
		.mode = NO_OS_SPI_MODE_0,
		.platform_ops = &linux_spi_ops,
		.chip_select = SPI_CS,
		.extra = NULL,
	};
	struct adxl355_init_param init_param = {
		.comm_type = ADXL355_SPI_COMM,
		.dev_type = ID_ADXL355,
	};
	struct adxl355_frac_repr x, y, z, temp;
	int ret;

	init_param.comm_init.spi_init = spi_ip;

	ret = adxl355_init(&dev, init_param);
	if (ret) {
		printf("ADXL355 init failed: %d\n", ret);
		return ret;
	}

	printf("ADXL355 initialized on /dev/spidev%d.%d\n",
	       SPI_DEVICE_ID, SPI_CS);

	ret = adxl355_soft_reset(dev);
	if (ret)
		goto error;

	ret = adxl355_set_odr_lpf(dev, ADXL355_ODR_3_906HZ);
	if (ret)
		goto error;

	ret = adxl355_set_op_mode(dev, ADXL355_MEAS_TEMP_ON_DRDY_OFF);
	if (ret)
		goto error;

	while (1) {
		ret = adxl355_get_xyz(dev, &x, &y, &z);
		if (ret)
			goto error;

		printf("x=%d.%09u  y=%d.%09u  z=%d.%09u m/s^2\n",
		       (int)x.integer, (unsigned)abs(x.fractional),
		       (int)y.integer, (unsigned)abs(y.fractional),
		       (int)z.integer, (unsigned)abs(z.fractional));

		ret = adxl355_get_temp(dev, &temp);
		if (ret)
			goto error;

		printf("Temp=%d.%09u mC\n",
		       (int)temp.integer, (unsigned)abs(temp.fractional));

		printf("---\n");
		no_os_mdelay(1000);
	}

error:
	printf("Error: %d\n", ret);
	adxl355_remove(dev);
	return ret;
}
