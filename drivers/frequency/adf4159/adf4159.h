/***************************************************************************//**
 *   @file   adf4159.h
 *   @brief  Header file of ADF4159 Driver.
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
#ifndef _ADF4159_H_
#define _ADF4159_H_

#include <stdint.h>
#include "no_os_gpio.h"
#include "no_os_spi.h"

/* Register indices (address is OR'd into low 3 bits at write time) */
#define ADF4159_REG0		0
#define ADF4159_REG1		1
#define ADF4159_REG2		2
#define ADF4159_REG3		3
#define ADF4159_REG4		4
#define ADF4159_REG5		5
#define ADF4159_REG6		6
#define ADF4159_REG7		7
#define ADF4159_REG4_SEL1	8
#define ADF4159_REG5_SEL1	9
#define ADF4159_REG6_SEL1	10
#define ADF4159_NUM_REGS	11

/* ---- REG0: FRAC/INT ---- */
#define ADF4159_REG0_RAMP_ON(x)	(((x) & 0x1) << 31)
#define ADF4159_REG0_MUXOUT(x)		(((x) & 0xF) << 27)
#define ADF4159_REG0_INT(x)		(((x) & 0xFFF) << 15)
#define ADF4159_REG0_FRACT_MSB(x)	(((x) & 0xFFF) << 3)
#define ADF4159_REG0_MUXOUT_MASK	(0xFU << 27)

/* ---- REG1: LSB FRAC ---- */
#define ADF4159_REG1_PHASE_ADJ(x)	(((x) & 0x1) << 28)
#define ADF4159_REG1_FRACT_LSB(x)	(((x) & 0x1FFF) << 15)
#define ADF4159_REG1_PHASE(x)		(((x) & 0xFFF) << 3)

/* ---- REG2: R / CLK1 / CP ---- */
#define ADF4159_REG2_CSR_EN(x)		(((x) & 0x1) << 28)
#define ADF4159_REG2_CP_CURRENT(x)	(((x) & 0xF) << 24)
#define ADF4159_REG2_PRESCALER(x)	(((x) & 0x1) << 22)
#define ADF4159_REG2_RDIV2(x)		(((x) & 0x1) << 21)
#define ADF4159_REG2_REF_DBL(x)	(((x) & 0x1) << 20)
#define ADF4159_REG2_R_CNT(x)		(((x) & 0x1F) << 15)
#define ADF4159_REG2_CLK1_DIV(x)	(((x) & 0xFFF) << 3)

/* ---- REG3: Function ---- */
#define ADF4159_REG3_NEG_BLEED_CURR(x)	(((x) & 0x7) << 22)
#define ADF4159_REG3_NEG_BLEED_EN(x)	(((x) & 0x1) << 21)
#define ADF4159_REG3_LOL_DIS(x)		(((x) & 0x1) << 16)
#define ADF4159_REG3_SD_RST(x)		(((x) & 0x1) << 14)
#define ADF4159_REG3_RAMP_MODE(x)	(((x) & 0x3) << 10)
#define ADF4159_REG3_PSK_EN(x)		(((x) & 0x1) << 9)
#define ADF4159_REG3_FSK_EN(x)		(((x) & 0x1) << 8)
#define ADF4159_REG3_LDP(x)		(((x) & 0x1) << 7)
#define ADF4159_REG3_PD_POL(x)		(((x) & 0x1) << 6)
#define ADF4159_REG3_PD(x)		(((x) & 0x1) << 5)
#define ADF4159_REG3_CP_Z(x)		(((x) & 0x1) << 4)
#define ADF4159_REG3_CNT_RST(x)	(((x) & 0x1) << 3)

/* ---- REG4: CLK DIV ---- */
#define ADF4159_REG4_LE_SEL(x)		(((x) & 0x1) << 31)
#define ADF4159_REG4_SD_MOD(x)		(((x) & 0x1F) << 26)
#define ADF4159_REG4_RAMP_STATUS(x)	(((x) & 0x1F) << 21)
#define ADF4159_REG4_CLK_DIV_MODE(x)	(((x) & 0x3) << 19)
#define ADF4159_REG4_CLK2_DIV(x)	(((x) & 0xFFF) << 7)
#define ADF4159_REG4_CLK_DIV2_SEL(x)	(((x) & 0x1) << 6)

/* ---- REG5: Deviation ---- */
#define ADF4159_REG5_TXDATA_INV(x)	(((x) & 0x1) << 30)
#define ADF4159_REG5_TXDATA_CLK(x)	(((x) & 0x1) << 29)
#define ADF4159_REG5_PARABOLIC(x)	(((x) & 0x1) << 28)
#define ADF4159_REG5_INTERRUPT(x)	(((x) & 0x3) << 26)
#define ADF4159_REG5_FSK_RAMP(x)	(((x) & 0x1) << 25)
#define ADF4159_REG5_DUAL_RAMP(x)	(((x) & 0x1) << 24)
#define ADF4159_REG5_DEV_SEL(x)		(((x) & 0x1) << 23)
#define ADF4159_REG5_DEV_OFFSET(x)	(((x) & 0xF) << 19)
#define ADF4159_REG5_DEVIATION(x)	(((x) & 0xFFFF) << 3)

/* ---- REG6: Step ---- */
#define ADF4159_REG6_STEP_SEL(x)	(((x) & 0x1) << 23)
#define ADF4159_REG6_STEP(x)		(((x) & 0xFFFFF) << 3)

/* ---- REG7: Delay ---- */
#define ADF4159_REG7_TXDATA_TRIG_DEL(x)	(((x) & 0x1) << 23)
#define ADF4159_REG7_TRI_DELAY(x)	(((x) & 0x1) << 22)
#define ADF4159_REG7_SING_FULL_TRI(x)	(((x) & 0x1) << 21)
#define ADF4159_REG7_TXDATA_TRIG(x)	(((x) & 0x1) << 20)
#define ADF4159_REG7_FAST_RAMP(x)	(((x) & 0x1) << 19)
#define ADF4159_REG7_RAMP_DEL_FL(x)	(((x) & 0x1) << 18)
#define ADF4159_REG7_RAMP_DEL(x)	(((x) & 0x1) << 17)
#define ADF4159_REG7_DEL_CLK_SEL(x)	(((x) & 0x1) << 16)
#define ADF4159_REG7_DEL_START_EN(x)	(((x) & 0x1) << 15)
#define ADF4159_REG7_DEL_START(x)	(((x) & 0xFFF) << 3)

/* MUXOUT values */
#define ADF4159_MUXOUT_THREE_STATE	0
#define ADF4159_MUXOUT_DVDD		1
#define ADF4159_MUXOUT_DGND		2
#define ADF4159_MUXOUT_RDIV		3
#define ADF4159_MUXOUT_NDIV		4
#define ADF4159_MUXOUT_ALOCK		5
#define ADF4159_MUXOUT_DLOCK		6
#define ADF4159_MUXOUT_SDO		7
#define ADF4159_MUXOUT_CLKDIV		10
#define ADF4159_MUXOUT_FLS		12
#define ADF4159_MUXOUT_RDIV2		13
#define ADF4159_MUXOUT_NDIV2		14

/* GPIO helpers */
#define ADF4159_LE_OUT	no_os_gpio_direction_output(dev->gpio_le, \
			NO_OS_GPIO_HIGH)
#define ADF4159_LE_LOW	no_os_gpio_set_value(dev->gpio_le, \
			NO_OS_GPIO_LOW)
#define ADF4159_LE_HIGH	no_os_gpio_set_value(dev->gpio_le, \
			NO_OS_GPIO_HIGH)
#define ADF4159_CE_OUT	no_os_gpio_direction_output(dev->gpio_ce, \
			NO_OS_GPIO_HIGH)
#define ADF4159_CE_LOW	no_os_gpio_set_value(dev->gpio_ce, \
			NO_OS_GPIO_LOW)
#define ADF4159_CE_HIGH	no_os_gpio_set_value(dev->gpio_ce, \
			NO_OS_GPIO_HIGH)

/* Specifications */
#define ADF4159_MAX_OUT_FREQ		13000000000ULL	/* Hz */
#define ADF4159_MIN_OUT_FREQ		500000000ULL	/* Hz */
#define ADF4159_MAX_FREQ_45_PRESC	8000000000ULL	/* Hz */
#define ADF4159_MIN_INT_45_PRESC	23
#define ADF4159_MIN_INT_89_PRESC	75
#define ADF4159_MAX_FREQ_PFD		110000000ULL	/* Hz */
#define ADF4159_MAX_FREQ_REFIN		260000000	/* Hz */
#define ADF4159_MIN_FREQ_REFIN		10000000	/* Hz */
#define ADF4159_MODULUS			33554432ULL	/* 2^25 */
#define ADF4159_MAX_R_CNT		32

/**
 * @struct adf4159_config
 * @brief  Configuration matching Linux kernel DT properties.
 */
struct adf4159_config {
	uint64_t	frequency;	/* Power-up frequency in Hz */
	uint32_t	clkin;		/* Reference clock in Hz */
	uint8_t		ref_doubler_en;
	uint8_t		ref_div2_en;
	uint32_t	ref_div_factor;	/* R counter (1�32) */
	uint32_t	cp_curr_uA;	/* Charge pump current in uA */
	uint8_t		pd_pol_pos;	/* Phase detector polarity: 1=positive */
	uint32_t	muxout;		/* MUXOUT select (0�15) */
	uint32_t	clk1_div;	/* CLK1 divider (0�4095) */
	uint32_t	clk2_div[2];	/* CLK2 dividers [SEL0, SEL1] */
	uint32_t	clk_div_mode;	/* CLK divider mode */
	uint32_t	ramp_mode;	/* 0=disabled */
	uint32_t	ramp_status;	/* Ramp status mode */
	int16_t		deviation[2];	/* Deviation words [SEL0, SEL1] */
	uint32_t	deviation_offs;	/* Deviation offset */
	uint32_t	step_word[2];	/* Step words [SEL0, SEL1] */
	uint32_t	delay_start_word;
	uint32_t	phase;		/* Phase value (0�4095) */
	uint32_t	interrupt_mode;
	uint8_t		neg_bleed_en;
	uint32_t	neg_bleed_curr;
};

/**
 * @struct adf4159_state
 * @brief  Runtime PLL state.
 */
struct adf4159_state {
	uint64_t	fpfd;		/* PFD frequency in Hz */
	uint32_t	integer;	/* INT divider value */
	uint32_t	fract;		/* 25-bit FRAC value */
	uint32_t	r_cnt;		/* R counter */
	uint32_t	regs[ADF4159_NUM_REGS];
};

/**
 * @struct adf4159_dev
 * @brief  ADF4159 device descriptor.
 */
struct adf4159_dev {
	struct no_os_spi_desc	*spi_desc;
	struct no_os_gpio_desc	*gpio_le;
	struct no_os_gpio_desc	*gpio_ce;
	struct adf4159_config	config;
	struct adf4159_state	st;
};

/**
 * @struct adf4159_init_param
 * @brief  ADF4159 initialization parameters.
 */
struct adf4159_init_param {
	struct no_os_spi_init_param	spi_init;
	struct no_os_gpio_init_param	gpio_le;
	struct no_os_gpio_init_param	gpio_ce;
	struct adf4159_config		config;
};

int32_t adf4159_init(struct adf4159_dev **device,
		     const struct adf4159_init_param *param);
int32_t adf4159_remove(struct adf4159_dev *dev);
int32_t adf4159_write(struct adf4159_dev *dev, uint32_t val);
int32_t adf4159_sync_config(struct adf4159_dev *dev);
int32_t adf4159_setup(struct adf4159_dev *dev, uint64_t freq_hz);
int32_t adf4159_set_freq(struct adf4159_dev *dev, uint64_t freq_hz);

#endif /* _ADF4159_H_ */
