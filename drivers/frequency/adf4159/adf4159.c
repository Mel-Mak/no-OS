/***************************************************************************//**
 *   @file   adf4159.c
 *   @brief  Implementation of ADF4159 Driver.
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

#include <stdlib.h>
#include "adf4159.h"
#include "adf4159_cfg.h"
#include "no_os_alloc.h"

/***************************************************************************//**
 * @brief Initialize the SPI communication with the device.
 *
 * @param device     - The device structure.
 * @param init_param - The structure that contains the device initial
 *                     parameters.
 *
 * @return status - Result of the initialization procedure.
 *                  Example:  0 - if initialization was successful;
 *                           -1 - if initialization was unsuccessful.
 ******************************************************************************/
int8_t adf4159_init(struct adf4159_dev **device,
		    struct adf4159_init_param init_param)
{
	struct adf4159_dev *dev;
	uint32_t cfg_value = 0;
	int8_t status = -1;

	dev = (struct adf4159_dev *)no_os_malloc(sizeof(*dev));
	if (!dev)
		return -1;

	dev->adf4159_st.pdata = &adf4159_pdata;

	/* Setup GPIO pads */
	status = no_os_gpio_get(&dev->gpio_le, &init_param.gpio_le);

	/* Setup Control GPIO Pins */
	ADF4159_LE_OUT;
	ADF4159_LE_LOW;

	/* CE is optional skip if not configured (number < 0) */
	if (init_param.gpio_ce.number >= 0) {
		status |= no_os_gpio_get(&dev->gpio_ce, &init_param.gpio_ce);
		ADF4159_CE_OUT;
		ADF4159_CE_HIGH;
	} else {
		dev->gpio_ce = NULL;
	}

	/* Setup SPI Interface */
	status |= no_os_spi_init(&dev->spi_desc, &init_param.spi_init);

	/* R7 - Delay register */
	cfg_value = adf4159_pdata.r7_user_settings |
		    ADF4159_R7_CTRL;
	adf4159_set(dev, cfg_value);
	dev->adf4159_st.reg_val[ADF4159_REG7] = cfg_value;

	/* R6 - Step register */
	cfg_value = adf4159_pdata.r6_user_settings |
		    ADF4159_R6_CTRL;
	adf4159_set(dev, cfg_value);
	dev->adf4159_st.reg_val[ADF4159_REG6] = cfg_value;

	/* R5 - Deviation register */
	cfg_value = adf4159_pdata.r5_user_settings |
		    ADF4159_R5_CTRL;
	adf4159_set(dev, cfg_value);
	dev->adf4159_st.reg_val[ADF4159_REG5] = cfg_value;

	/* R4 - CLK DIV register */
	cfg_value = adf4159_pdata.r4_user_settings |
		    ADF4159_R4_CTRL;
	adf4159_set(dev, cfg_value);
	dev->adf4159_st.reg_val[ADF4159_REG4] = cfg_value;

	/* R3 - Function register */
	cfg_value = adf4159_pdata.r3_user_settings |
		    ADF4159_R3_CTRL;
	adf4159_set(dev, cfg_value);
	dev->adf4159_st.reg_val[ADF4159_REG3] = cfg_value;

	/* R2 - MOD/R register */
	cfg_value = adf4159_pdata.r2_user_settings       |
		    ADF4159_PRESCALER(0)                 |
		    (adf4159_pdata.ref_div2_en ?
			ADF4159_RDIV2(1) : ADF4159_RDIV2(0))     |
		    (adf4159_pdata.ref_doubler_en ?
			ADF4159_REF_DBL(1) : ADF4159_REF_DBL(0)) |
		    ADF4159_R_CNT(1)                     |
		    ADF4159_R2_CTRL;
	adf4159_set(dev, cfg_value);
	dev->adf4159_st.reg_val[ADF4159_REG2] = cfg_value;

	/* R1 - LSB FRAC register */
	cfg_value = ADF4159_FRAC_VAL_LSB(1) |
		    ADF4159_R1_CTRL;
	adf4159_set(dev, cfg_value);
	dev->adf4159_st.reg_val[ADF4159_REG1] = cfg_value;

	/* R0 - FRAC/INT register (latches configuration) */
	cfg_value = adf4159_pdata.r0_user_settings |
		    ADF4159_INT_VAL(1)             |
		    ADF4159_FRAC_VAL_MSB(1)        |
		    ADF4159_R0_CTRL;
	adf4159_set(dev, cfg_value);
	dev->adf4159_st.reg_val[ADF4159_REG0] = cfg_value;

	*device = dev;

	return status;
}

/***************************************************************************//**
 * @brief Free the resources allocated by adf4159_init().
 *
 * @param dev - The device structure.
 *
 * @return 0 in case of success, negative error code otherwise.
 ******************************************************************************/
int32_t adf4159_remove(struct adf4159_dev *dev)
{
	int32_t ret;

	ret = no_os_spi_remove(dev->spi_desc);

	ret |= no_os_gpio_remove(dev->gpio_le);
	if (dev->gpio_ce)
		ret |= no_os_gpio_remove(dev->gpio_ce);

	no_os_free(dev);

	return ret;
}

/***************************************************************************//**
 * @brief Transmits 32 bits on SPI.
 *
 * @param dev   - The device structure.
 * @param value - Data which will be transmitted.
 *
 * @return status - Result of the operation.
 ******************************************************************************/
int8_t adf4159_set(struct adf4159_dev *dev,
		   uint32_t value)
{
	int8_t validation = 0;
	int8_t status = 0;
	uint8_t tx_buffer[4] = {0, 0, 0, 0};

	tx_buffer[0] = (uint8_t)((value & 0xFF000000) >> 24);
	tx_buffer[1] = (uint8_t)((value & 0x00FF0000) >> 16);
	tx_buffer[2] = (uint8_t)((value & 0x0000FF00) >> 8);
	tx_buffer[3] = (uint8_t)((value & 0x000000FF) >> 0);

	ADF4159_LE_LOW;
	validation = no_os_spi_write_and_read(dev->spi_desc,
					      tx_buffer,
					      4);
	if (validation != 4) {
		status = -1;
	}
	ADF4159_LE_HIGH;

	return status;
}

/***************************************************************************//**
 * @brief Increases the R counter value until the PFD frequency is
 *        smaller than ADF4159_MAX_FREQ_PFD.
 *
 * @param dev   - The device structure.
 * @param r_cnt - Initial r_cnt value.
 *
 * @return Final r_cnt value.
 ******************************************************************************/
int32_t adf4159_tune_r_cnt(struct adf4159_dev *dev,
			   int32_t r_cnt)
{
	struct adf4159_platform_data *pdata = dev->adf4159_st.pdata;

	do {
		r_cnt++;
		dev->adf4159_st.fpfd = (pdata->clkin *
					(pdata->ref_doubler_en ? 2 : 1)) /
				       (r_cnt *
					(pdata->ref_div2_en ? 2 : 1));
	} while (dev->adf4159_st.fpfd > ADF4159_MAX_FREQ_PFD);

	return r_cnt;
}

/***************************************************************************//**
 * @brief Sets the ADF4159 output frequency.
 *
 * @param dev  - The device structure.
 * @param freq - The desired frequency value in MHz.
 *
 * @return calculatedFrequency - The actual frequency value that was set (MHz).
 ******************************************************************************/
double adf4159_set_freq(struct adf4159_dev *dev,
			double freq)
{
	uint64_t tmp;
	uint32_t prescaler;
	uint16_t mdiv;
	uint16_t r_cnt = 0;
	double result;

	if ((freq > ADF4159_MAX_OUT_FREQ) || (freq < ADF4159_MIN_OUT_FREQ))
		return -1;

	if (freq > ADF4159_MAX_FREQ_45_PRESC) {
		prescaler = ADF4159_PRESCALER(1);
		mdiv = ADF4159_MIN_INT_89_PRESC;
	} else {
		prescaler = ADF4159_PRESCALER(0);
		mdiv = ADF4159_MIN_INT_45_PRESC;
	}

	freq *= 1000000;
	if ((dev->adf4159_st.pdata->clkin > ADF4159_MAX_FREQ_REFIN) ||
	    (dev->adf4159_st.pdata->clkin < ADF4159_MIN_FREQ_REFIN)) {
		return -1;
	}
	do {
		r_cnt = adf4159_tune_r_cnt(dev, r_cnt);
		dev->adf4159_st.r_cnt = r_cnt;
	} while (r_cnt == 0);

	tmp = freq * (uint64_t)ADF4159_FIXED_MODULUS +
	      (dev->adf4159_st.fpfd >> 1);
	tmp = (uint64_t)(tmp / dev->adf4159_st.fpfd);
	dev->adf4159_st.r0_fract = tmp % ADF4159_FIXED_MODULUS;
	tmp = tmp / ADF4159_FIXED_MODULUS;
	dev->adf4159_st.r0_int = (uint32_t)tmp;

	if (dev->adf4159_st.r0_int < mdiv)
		return -1;

	/* R3: set counter reset */
	dev->adf4159_st.reg_val[ADF4159_REG3] &= ~(ADF4159_CNT_RST(-1));
	dev->adf4159_st.reg_val[ADF4159_REG3] |= (ADF4159_CNT_RST(1));
	adf4159_set(dev, dev->adf4159_st.reg_val[ADF4159_REG3]);

	/* R2: update R counter and prescaler */
	dev->adf4159_st.reg_val[ADF4159_REG2] &= ~(ADF4159_R_CNT(-1) |
			ADF4159_PRESCALER(-1));
	dev->adf4159_st.reg_val[ADF4159_REG2] |= (ADF4159_R_CNT(
				dev->adf4159_st.r_cnt) |
			prescaler);
	adf4159_set(dev, dev->adf4159_st.reg_val[ADF4159_REG2]);

	/* R1: FRAC LSB (bottom 13 bits of 25-bit FRAC) */
	dev->adf4159_st.reg_val[ADF4159_REG1] &= ~ADF4159_FRAC_VAL_LSB(-1);
	dev->adf4159_st.reg_val[ADF4159_REG1] |= ADF4159_FRAC_VAL_LSB(
				dev->adf4159_st.r0_fract);
	adf4159_set(dev, dev->adf4159_st.reg_val[ADF4159_REG1]);

	/* R0: INT value + FRAC MSB (top 12 bits of 25-bit FRAC) */
	dev->adf4159_st.reg_val[ADF4159_REG0] &= ~(ADF4159_INT_VAL(-1) |
			ADF4159_FRAC_VAL_MSB(-1));
	dev->adf4159_st.reg_val[ADF4159_REG0] |= (ADF4159_INT_VAL(
				dev->adf4159_st.r0_int) |
			ADF4159_FRAC_VAL_MSB(dev->adf4159_st.r0_fract));
	adf4159_set(dev, dev->adf4159_st.reg_val[ADF4159_REG0]);

	/* R3: clear counter reset */
	dev->adf4159_st.reg_val[ADF4159_REG3] &= ~(ADF4159_CNT_RST(-1));
	adf4159_set(dev, dev->adf4159_st.reg_val[ADF4159_REG3]);

	result = dev->adf4159_st.r0_int *
		 (float)(dev->adf4159_st.fpfd / 1000000);
	result = result + ((float)dev->adf4159_st.r0_fract /
		 ADF4159_FIXED_MODULUS) *
		 (dev->adf4159_st.fpfd / 1000000);

	return result;
}
