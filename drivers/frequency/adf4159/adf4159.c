/***************************************************************************//**
 *   @file   adf4159.c
 *   @brief  Implementation of ADF4159 Driver.
 *          Based on the ADI Linux kernel IIO driver (GPLv2).
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
#include <string.h>
#include "adf4159.h"
#include "no_os_alloc.h"

/***************************************************************************//**
 * @brief Write a 32-bit register value to the ADF4159 via SPI.
 *
 * Sends 4 bytes MSB-first (big-endian). This matches the Linux kernel's
 * cpu_to_be32() + spi_write() sequence.
 *
 * @param dev - The device structure.
 * @param val - The 32-bit value to write (includes register address in bits 2:0).
 *
 * @return 0 on success, negative error code otherwise.
 ******************************************************************************/
int32_t adf4159_write(struct adf4159_dev *dev, uint32_t val)
{
	int32_t ret;
	uint8_t buf[4];

	buf[0] = (val >> 24) & 0xFF;
	buf[1] = (val >> 16) & 0xFF;
	buf[2] = (val >>  8) & 0xFF;
	buf[3] = (val >>  0) & 0xFF;

	// printf("  SPI R%d: 0x%08X\n", val & 0x7, val);

	if (dev->gpio_le)
		ADF4159_LE_LOW;

	ret = no_os_spi_write_and_read(dev->spi_desc, buf, 4);

	if (dev->gpio_le)
		ADF4159_LE_HIGH;

	if (ret)
		printf("  *** SPI WRITE FAILED: %d ***\n", ret);

	return ret;
}

/***************************************************************************//**
 * @brief Write all registers to the ADF4159.
 *
 * Writes R7, R6, R6_SEL1, R5, R5_SEL1, R4, R4_SEL1, R3, R2, R1, R0.
 * Register address (bits 2:0) is OR'd into each value at write time.
 * Matches the Linux kernel's adf4159_sync_config().
 *
 * @param dev - The device structure.
 *
 * @return 0 on success, negative error code otherwise.
 ******************************************************************************/
int32_t adf4159_sync_config(struct adf4159_dev *dev)
{
	static const int write_order[] = {
		ADF4159_REG7,
		ADF4159_REG6,  ADF4159_REG6_SEL1,
		ADF4159_REG5,  ADF4159_REG5_SEL1,
		ADF4159_REG4,  ADF4159_REG4_SEL1,
		ADF4159_REG3,
		ADF4159_REG2,
		ADF4159_REG1,
		ADF4159_REG0,
	};
	int32_t ret;
	uint32_t i, idx, addr, val;

	for (i = 0; i < sizeof(write_order) / sizeof(write_order[0]); i++) {
		idx = write_order[i];
		/* Convert array index to 3-bit register address */
		addr = (idx < 8) ? idx : (idx - 4);
		val = dev->st.regs[idx] | addr;

		ret = adf4159_write(dev, val);
		if (ret)
			return ret;
	}

	return 0;
}

/***************************************************************************//**
 * @brief Compute INT and FRAC from frequency and PFD.
 *
 * Matches the Linux kernel's adf4159_pll_fract_n_compute().
 * FRAC is a 25-bit value (modulus = 2^25 = 33554432).
 *
 * @param fpfd    - PFD frequency in Hz.
 * @param freq    - Desired output frequency in Hz.
 * @param integer - Pointer to store INT divider value.
 * @param fract   - Pointer to store 25-bit FRAC value.
 ******************************************************************************/
static void adf4159_pll_fract_n_compute(uint64_t fpfd, uint64_t freq,
					uint32_t *integer, uint32_t *fract)
{
	uint64_t tmp;

	tmp = freq * (uint64_t)ADF4159_MODULUS;
	tmp = tmp / fpfd;

	*fract = (uint32_t)(tmp % ADF4159_MODULUS);
	*integer = (uint32_t)(tmp / ADF4159_MODULUS);
}

/***************************************************************************//**
 * @brief Compute all register values and sync to hardware.
 *
 * Computes PFD frequency, INT/FRAC values, prescaler selection, and
 * populates all registers. Then calls adf4159_sync_config() to write
 * them to the device. Matches the Linux kernel's adf4159_setup().
 *
 * @param dev     - The device structure.
 * @param freq_hz - Desired output frequency in Hz.
 *
 * @return 0 on success, negative error code otherwise.
 ******************************************************************************/
int32_t adf4159_setup(struct adf4159_dev *dev, uint64_t freq_hz)
{
	struct adf4159_config *cfg = &dev->config;
	struct adf4159_state *st = &dev->st;
	uint32_t prescaler, min_int;
	uint32_t cp;
	uint32_t r_cnt;
	uint8_t ramp_en;

	if (freq_hz > ADF4159_MAX_OUT_FREQ || freq_hz < ADF4159_MIN_OUT_FREQ)
		return -1;

	/* Prescaler selection */
	if (freq_hz > ADF4159_MAX_FREQ_45_PRESC) {
		prescaler = 1;
		min_int = ADF4159_MIN_INT_89_PRESC;
	} else {
		prescaler = 0;
		min_int = ADF4159_MIN_INT_45_PRESC;
	}

	/* PFD frequency */
	r_cnt = cfg->ref_div_factor;
	if (r_cnt == 0)
		r_cnt = 1;

	st->fpfd = (uint64_t)cfg->clkin *
		   (cfg->ref_doubler_en ? 2 : 1);
	st->fpfd = st->fpfd /
		   (r_cnt * (cfg->ref_div2_en ? 2 : 1));

	/* Clamp PFD to max by increasing R if needed */
	while (st->fpfd > ADF4159_MAX_FREQ_PFD &&
	       r_cnt < ADF4159_MAX_R_CNT) {
		r_cnt++;
		st->fpfd = (uint64_t)cfg->clkin *
			   (cfg->ref_doubler_en ? 2 : 1);
		st->fpfd = st->fpfd /
			   (r_cnt * (cfg->ref_div2_en ? 2 : 1));
	}
	st->r_cnt = r_cnt;

	/* Compute INT and FRAC */
	adf4159_pll_fract_n_compute(st->fpfd, freq_hz,
				    &st->integer, &st->fract);

	if (st->integer < min_int) {
		printf("  ERROR: INT=%u < min_int=%u for freq %llu Hz\n",
		       st->integer, min_int,
		       (unsigned long long)freq_hz);
		return -1;
	}

	printf("  setup: freq=%llu Hz, fpfd=%llu Hz, INT=%u, FRAC=%u, R=%u\n",
	       (unsigned long long)freq_hz,
	       (unsigned long long)st->fpfd,
	       st->integer, st->fract, st->r_cnt);

	/* Charge pump current: steps of 315 uA from 315 to 5040 */
	cp = (cfg->cp_curr_uA > 315) ?
	     ((cfg->cp_curr_uA - 315 + 157) / 315) : 0;
	if (cp > 15)
		cp = 15;

	ramp_en = (cfg->ramp_mode != 0) ? 1 : 0;

	/* Populate all registers (without address bits � OR'd at write time) */
	memset(st->regs, 0, sizeof(st->regs));

	/* R0: FRAC MSB + INT + MUXOUT + RAMP_ON */
	st->regs[ADF4159_REG0] =
		ADF4159_REG0_INT(st->integer) |
		ADF4159_REG0_FRACT_MSB(st->fract >> 13) |
		ADF4159_REG0_MUXOUT(cfg->muxout) |
		ADF4159_REG0_RAMP_ON(ramp_en);

	/* R1: FRAC LSB + Phase */
	st->regs[ADF4159_REG1] =
		ADF4159_REG1_FRACT_LSB(st->fract & 0x1FFF) |
		ADF4159_REG1_PHASE(cfg->phase);

	/* R2: CP + Prescaler + R counter + CLK1 divider */
	st->regs[ADF4159_REG2] =
		ADF4159_REG2_CP_CURRENT(cp) |
		ADF4159_REG2_PRESCALER(prescaler) |
		ADF4159_REG2_RDIV2(cfg->ref_div2_en) |
		ADF4159_REG2_REF_DBL(cfg->ref_doubler_en) |
		ADF4159_REG2_R_CNT(st->r_cnt) |
		ADF4159_REG2_CLK1_DIV(cfg->clk1_div);

	/* R3: Function register */
	st->regs[ADF4159_REG3] =
		ADF4159_REG3_RAMP_MODE(cfg->ramp_mode) |
		ADF4159_REG3_PD_POL(cfg->pd_pol_pos) |
		ADF4159_REG3_NEG_BLEED_EN(cfg->neg_bleed_en) |
		ADF4159_REG3_NEG_BLEED_CURR(cfg->neg_bleed_curr);
	if (cfg->neg_bleed_en)
		st->regs[ADF4159_REG3] |= ADF4159_REG3_LOL_DIS(1);

	/* R4: CLK divider + Ramp status (SEL0) */
	st->regs[ADF4159_REG4] =
		ADF4159_REG4_RAMP_STATUS(cfg->ramp_status) |
		ADF4159_REG4_CLK_DIV_MODE(cfg->clk_div_mode) |
		ADF4159_REG4_CLK2_DIV(cfg->clk2_div[0]);

	/* R4 SEL1 */
	st->regs[ADF4159_REG4_SEL1] =
		ADF4159_REG4_CLK_DIV2_SEL(1) |
		ADF4159_REG4_RAMP_STATUS(cfg->ramp_status) |
		ADF4159_REG4_CLK_DIV_MODE(cfg->clk_div_mode) |
		ADF4159_REG4_CLK2_DIV(cfg->clk2_div[1]);

	/* R5: Deviation (SEL0) */
	st->regs[ADF4159_REG5] =
		ADF4159_REG5_DEVIATION(cfg->deviation[0]) |
		ADF4159_REG5_DEV_OFFSET(cfg->deviation_offs) |
		ADF4159_REG5_INTERRUPT(cfg->interrupt_mode);

	/* R5 SEL1 */
	st->regs[ADF4159_REG5_SEL1] =
		ADF4159_REG5_DEV_SEL(1) |
		ADF4159_REG5_DEVIATION(cfg->deviation[1]) |
		ADF4159_REG5_DEV_OFFSET(cfg->deviation_offs) |
		ADF4159_REG5_INTERRUPT(cfg->interrupt_mode);

	/* R6: Step (SEL0) */
	st->regs[ADF4159_REG6] =
		ADF4159_REG6_STEP(cfg->step_word[0]);

	/* R6 SEL1 */
	st->regs[ADF4159_REG6_SEL1] =
		ADF4159_REG6_STEP_SEL(1) |
		ADF4159_REG6_STEP(cfg->step_word[1]);

	/* R7: Delay */
	st->regs[ADF4159_REG7] =
		ADF4159_REG7_DEL_START_EN(cfg->delay_start_word ? 1 : 0) |
		ADF4159_REG7_DEL_START(cfg->delay_start_word);

	return adf4159_sync_config(dev);
}

/***************************************************************************//**
 * @brief Set the ADF4159 output frequency.
 *
 * Recomputes INT/FRAC for the new frequency and writes R3 (counter reset),
 * R2, R1, R0, R3 (clear counter reset). This is the fast frequency
 * update path � for initial setup, use adf4159_setup().
 *
 * @param dev     - The device structure.
 * @param freq_hz - Desired output frequency in Hz.
 *
 * @return 0 on success, negative error code otherwise.
 ******************************************************************************/
int32_t adf4159_set_freq(struct adf4159_dev *dev, uint64_t freq_hz)
{
	struct adf4159_state *st = &dev->st;
	uint32_t prescaler, min_int;
	int32_t ret;
	uint32_t addr;

	if (freq_hz > ADF4159_MAX_OUT_FREQ || freq_hz < ADF4159_MIN_OUT_FREQ)
		return -1;

	/* Prescaler selection */
	if (freq_hz > ADF4159_MAX_FREQ_45_PRESC) {
		prescaler = 1;
		min_int = ADF4159_MIN_INT_89_PRESC;
	} else {
		prescaler = 0;
		min_int = ADF4159_MIN_INT_45_PRESC;
	}

	/* Recompute INT and FRAC */
	adf4159_pll_fract_n_compute(st->fpfd, freq_hz,
				    &st->integer, &st->fract);

	if (st->integer < min_int)
		return -1;

	printf("  set_freq: target=%llu Hz, INT=%u, FRAC=%u\n",
	       (unsigned long long)freq_hz, st->integer, st->fract);

	/* Update R3: set counter reset */
	st->regs[ADF4159_REG3] |= ADF4159_REG3_CNT_RST(1);
	ret = adf4159_write(dev, st->regs[ADF4159_REG3] | ADF4159_REG3);
	if (ret)
		return ret;

	/* Update R2: prescaler (R counter / CLK1 div unchanged) */
	st->regs[ADF4159_REG2] &= ~(0x1 << 22);
	st->regs[ADF4159_REG2] |= ADF4159_REG2_PRESCALER(prescaler);
	ret = adf4159_write(dev, st->regs[ADF4159_REG2] | ADF4159_REG2);
	if (ret)
		return ret;

	/* Update R1: FRAC LSB */
	st->regs[ADF4159_REG1] &= ~(0x1FFF << 15);
	st->regs[ADF4159_REG1] |= ADF4159_REG1_FRACT_LSB(st->fract & 0x1FFF);
	ret = adf4159_write(dev, st->regs[ADF4159_REG1] | ADF4159_REG1);
	if (ret)
		return ret;

	/* Update R0: INT + FRAC MSB */
	st->regs[ADF4159_REG0] &= ~((0xFFF << 15) | (0xFFF << 3));
	st->regs[ADF4159_REG0] |= ADF4159_REG0_INT(st->integer) |
				   ADF4159_REG0_FRACT_MSB(st->fract >> 13);
	ret = adf4159_write(dev, st->regs[ADF4159_REG0] | ADF4159_REG0);
	if (ret)
		return ret;

	/* Clear counter reset */
	st->regs[ADF4159_REG3] &= ~ADF4159_REG3_CNT_RST(1);
	ret = adf4159_write(dev, st->regs[ADF4159_REG3] | ADF4159_REG3);

	return ret;
}

/***************************************************************************//**
 * @brief Initialize the ADF4159 device.
 *
 * Sets up SPI, optional GPIOs, and programs the PLL if a power-up
 * frequency is configured.
 *
 * @param device - Pointer to the device descriptor (allocated here).
 * @param param  - Initialization parameters.
 *
 * @return 0 on success, negative error code otherwise.
 ******************************************************************************/
int32_t adf4159_init(struct adf4159_dev **device,
		     const struct adf4159_init_param *param)
{
	struct adf4159_dev *dev;
	int32_t ret;

	dev = (struct adf4159_dev *)no_os_malloc(sizeof(*dev));
	if (!dev)
		return -1;
	memset(dev, 0, sizeof(*dev));

	/* Copy configuration */
	memcpy(&dev->config, &param->config, sizeof(dev->config));

	/* Setup GPIO LE � optional (skip if number < 0,
	 * meaning the SPI controller handles CS/LE natively) */
	if (param->gpio_le.number >= 0) {
		ret = no_os_gpio_get(&dev->gpio_le, &param->gpio_le);
		if (ret)
			goto err_free;
		ADF4159_LE_OUT;
		ADF4159_LE_LOW;
	}

	/* Setup GPIO CE � optional */
	if (param->gpio_ce.number >= 0) {
		ret = no_os_gpio_get(&dev->gpio_ce, &param->gpio_ce);
		if (ret)
			goto err_gpio_le;
		ADF4159_CE_OUT;
		ADF4159_CE_HIGH;
	}

	/* Setup SPI */
	ret = no_os_spi_init(&dev->spi_desc, &param->spi_init);
	if (ret)
		goto err_gpio_ce;

	printf("ADF4159: SPI init ok, clkin=%u Hz\n", dev->config.clkin);

	/* Program PLL if power-up frequency is set */
	if (dev->config.frequency > 0) {
		printf("ADF4159: programming power-up freq = %llu Hz\n",
		       (unsigned long long)dev->config.frequency);
		ret = adf4159_setup(dev, dev->config.frequency);
		if (ret) {
			printf("ADF4159: setup FAILED (%d)\n", ret);
			goto err_spi;
		}
		printf("ADF4159: init complete\n");
	}

	*device = dev;
	return 0;

err_spi:
	no_os_spi_remove(dev->spi_desc);
err_gpio_ce:
	if (dev->gpio_ce)
		no_os_gpio_remove(dev->gpio_ce);
err_gpio_le:
	if (dev->gpio_le)
		no_os_gpio_remove(dev->gpio_le);
err_free:
	no_os_free(dev);
	return ret;
}

/***************************************************************************//**
 * @brief Free the resources allocated by adf4159_init().
 *
 * @param dev - The device structure.
 *
 * @return 0 on success, negative error code otherwise.
 ******************************************************************************/
int32_t adf4159_remove(struct adf4159_dev *dev)
{
	int32_t ret;

	if (!dev)
		return -1;

	ret = no_os_spi_remove(dev->spi_desc);

	if (dev->gpio_le)
		ret |= no_os_gpio_remove(dev->gpio_le);
	if (dev->gpio_ce)
		ret |= no_os_gpio_remove(dev->gpio_ce);

	no_os_free(dev);

	return ret;
}
