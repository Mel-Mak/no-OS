/***************************************************************************//**
 *   @file   adf4159.h
 *   @brief  Header file of ADF4159 Driver.
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
#ifndef _ADF4159_H_
#define _ADF4159_H_

#include <stdint.h>
#include "no_os_gpio.h"
#include "no_os_spi.h"

/* FRAC/INT Register R0 */
#define ADF4159_RAMP_ON(x)          ((x & 0x1) << 31)
/* MUXOUT Control */
#define ADF4159_MUXOUT_Z            (0x00 << 27)
#define ADF4159_MUXOUT_DVDD         (0x01 << 27)
#define ADF4159_MUXOUT_DGND         (0x02 << 27)
#define ADF4159_MUXOUT_RDIV         (0x03 << 27)
#define ADF4159_MUXOUT_NDIV         (0x04 << 27)
#define ADF4159_MUXOUT_ALOCK        (0x05 << 27)
#define ADF4159_MUXOUT_DLOCK        (0x06 << 27)
#define ADF4159_MUXOUT_SDO          (0x07 << 27)
#define ADF4159_MUXOUT_CLKDIV       (0x0A << 27)
#define ADF4159_MUXOUT_FLS          (0x0C << 27)
#define ADF4159_MUXOUT_RDIV2        (0x0D << 27)
#define ADF4159_MUXOUT_NDIV2        (0x0E << 27)
#define ADF4159_MUXOUT_RESERVED     (0x0F << 27)
/* 12-Bit Integer Value INT */
#define ADF4159_INT_VAL(x)          ((x & 0xFFF) << 15)
/* 12-Bit Fractional Value MSB (top 12 bits of 25-bit FRAC) */
#define ADF4159_FRAC_VAL_MSB(x)     ((x & 0x1FFE000) >> 10)
/* Control bits */
#define ADF4159_R0_CTRL             0x00

/* LSB FRAC Register R1 */
#define ADF4159_PHASE_ADJ(x)        ((x & 0x1) << 28)
/* 13-Bit Fractional Value LSB (bottom 13 bits of 25-bit FRAC) */
#define ADF4159_FRAC_VAL_LSB(x)     ((x & 0x1FFF) << 15)
/* 12-Bit Phase Value */
#define ADF4159_PHASE_VAL(x)        ((x & 0xFFF) << 3)
/* Control bits */
#define ADF4159_R1_CTRL             0x01

/* MOD/R Register R2 */
#define ADF4159_CSR_EN(x)           ((x & 0x1) << 28)
/* Current setting */
#define ADF4159_CURR_SET(x)         ((x & 0xF) << 24)
/* Prescaler */
#define ADF4159_PRESCALER(x)        ((x & 0x1) << 22)
/* RDIV2 */
#define ADF4159_RDIV2(x)            ((x & 0x1) << 21)
/* Reference Doubler */
#define ADF4159_REF_DBL(x)          ((x & 0x1) << 20)
/* 5-Bit R-Counter */
#define ADF4159_R_CNT(x)            ((x & 0x1F) << 15)
/* 12-Bit CLK1 Divider */
#define ADF4159_CLK1_DIV(x)         ((x & 0xFFF) << 3)
/* Control bits */
#define ADF4159_R2_CTRL             0x02

/* Function Register R3 */
#define ADF4159_NEG_BLEED_CURR(x)   ((x & 0x7) << 22)
#define ADF4159_NEG_BLEED_EN(x)     ((x & 0x1) << 21)
#define ADF4159_LOL_DIS(x)          ((x & 0x1) << 16)
#define ADF4159_SD_RST(x)           ((x & 0x1) << 14)
#define ADF4159_RAMP_MODE(x)        ((x & 0x3) << 10)
#define ADF4159_PSK_EN(x)           ((x & 0x1) << 9)
#define ADF4159_FSK_EN(x)           ((x & 0x1) << 8)
#define ADF4159_LDP(x)              ((x & 0x1) << 7)
#define ADF4159_PD_POL(x)           ((x & 0x1) << 6)
#define ADF4159_PD(x)               ((x & 0x1) << 5)
#define ADF4159_CP_Z(x)             ((x & 0x1) << 4)
#define ADF4159_CNT_RST(x)          ((x & 0x1) << 3)
/* Control bits */
#define ADF4159_R3_CTRL             0x03

/* Ramp mode values */
#define ADF4159_RAMP_MODE_DISABLED              0
#define ADF4159_RAMP_MODE_CONT_SAWTOOTH         1
#define ADF4159_RAMP_MODE_CONT_TRIANGULAR       2
#define ADF4159_RAMP_MODE_SING_SAWTOOTH_BURST   3

/* CLK DIV Register R4 */
#define ADF4159_LE_SEL(x)           ((x & 0x1) << 31)
#define ADF4159_SD_MOD_MODE(x)      ((x & 0x1F) << 26)
#define ADF4159_RAMP_STATUS(x)      ((x & 0x1F) << 21)
#define ADF4159_CLK_DIV_MODE(x)     ((x & 0x3) << 19)
#define ADF4159_CLK2_DIV(x)         ((x & 0xFFF) << 7)
#define ADF4159_CLK_DIV2_SEL(x)     ((x & 0x1) << 6)
/* Control bits */
#define ADF4159_R4_CTRL             0x04

/* Deviation Register R5 */
#define ADF4159_TXDATA_INVERT(x)    ((x & 0x1) << 30)
#define ADF4159_TXDATA_RAMP_CLK(x)  ((x & 0x1) << 29)
#define ADF4159_PARABOLIC_RAMP(x)   ((x & 0x1) << 28)
#define ADF4159_INTERRUPT_MODE(x)   ((x & 0x3) << 26)
#define ADF4159_FSK_RAMP(x)         ((x & 0x1) << 25)
#define ADF4159_DUAL_RAMP(x)        ((x & 0x1) << 24)
#define ADF4159_DEV_SEL(x)          ((x & 0x1) << 23)
#define ADF4159_DEV_OFFSET(x)       ((x & 0xF) << 19)
#define ADF4159_DEVIATION(x)        ((x & 0xFFFF) << 3)
/* Control bits */
#define ADF4159_R5_CTRL             0x05

/* Step Register R6 */
#define ADF4159_STEP_SEL(x)         ((x & 0x1) << 23)
#define ADF4159_STEP(x)             ((x & 0xFFFFF) << 3)
/* Control bits */
#define ADF4159_R6_CTRL             0x06

/* Delay Register R7 */
#define ADF4159_TXDATA_TRIG_DEL(x)  ((x & 0x1) << 23)
#define ADF4159_TRI_DELAY(x)        ((x & 0x1) << 22)
#define ADF4159_SING_FULL_TRI(x)    ((x & 0x1) << 21)
#define ADF4159_TXDATA_TRIG_EN(x)   ((x & 0x1) << 20)
#define ADF4159_FAST_RAMP(x)        ((x & 0x1) << 19)
#define ADF4159_RAMP_DEL_FL(x)      ((x & 0x1) << 18)
#define ADF4159_RAMP_DEL(x)         ((x & 0x1) << 17)
#define ADF4159_DEL_CLK_SEL(x)      ((x & 0x1) << 16)
#define ADF4159_DEL_START_EN(x)     ((x & 0x1) << 15)
#define ADF4159_DEL_START(x)        ((x & 0xFFF) << 3)
/* Control bits */
#define ADF4159_R7_CTRL             0x07

/* GPIO */
#define ADF4159_LE_OUT              no_os_gpio_direction_output(dev->gpio_le,  \
				    NO_OS_GPIO_HIGH);
#define ADF4159_LE_LOW              no_os_gpio_set_value(dev->gpio_le,         \
				    NO_OS_GPIO_LOW)
#define ADF4159_LE_HIGH             no_os_gpio_set_value(dev->gpio_le,         \
				    NO_OS_GPIO_HIGH)

#define ADF4159_CE_OUT              no_os_gpio_direction_output(dev->gpio_ce,  \
				    NO_OS_GPIO_HIGH);
#define ADF4159_CE_LOW              no_os_gpio_set_value(dev->gpio_ce,         \
				    NO_OS_GPIO_LOW)
#define ADF4159_CE_HIGH             no_os_gpio_set_value(dev->gpio_ce,         \
				    NO_OS_GPIO_HIGH)

/* Specifications */
#define ADF4159_MAX_OUT_FREQ        13000         /* MHz */
#define ADF4159_MIN_OUT_FREQ        500           /* MHz */
#define ADF4159_MAX_FREQ_45_PRESC   8000          /* MHz */
#define ADF4159_MIN_INT_45_PRESC    23
#define ADF4159_MIN_INT_89_PRESC    75
#define ADF4159_MAX_FREQ_PFD        110000000     /* Hz */
#define ADF4159_MAX_FREQ_REFIN      260000000     /* Hz */
#define ADF4159_MIN_FREQ_REFIN      10000000      /* Hz */
#define ADF4159_FIXED_MODULUS       33554432      /* 2^25 */
#define ADF4159_MAX_R_CNT           32

/* Registers */
#define ADF4159_REG0                0
#define ADF4159_REG1                1
#define ADF4159_REG2                2
#define ADF4159_REG3                3
#define ADF4159_REG4                4
#define ADF4159_REG5                5
#define ADF4159_REG6                6
#define ADF4159_REG7                7

struct adf4159_platform_data {
	uint32_t	clkin;
	uint8_t		ref_doubler_en;
	uint8_t		ref_div2_en;
	uint32_t	r0_user_settings;
	uint32_t	r2_user_settings;
	uint32_t	r3_user_settings;
	uint32_t	r4_user_settings;
	uint32_t	r5_user_settings;
	uint32_t	r6_user_settings;
	uint32_t	r7_user_settings;
};

struct adf4159_state {
	struct adf4159_platform_data	*pdata;
	uint32_t			fpfd;
	uint16_t			r_cnt;
	uint32_t			r0_fract;
	uint32_t			r0_int;
	uint32_t			reg_val[8];
};

struct adf4159_dev {
	/* SPI */
	struct no_os_spi_desc	*spi_desc;
	/* GPIO */
	struct no_os_gpio_desc	*gpio_le;
	struct no_os_gpio_desc	*gpio_ce;
	/* Device Settings */
	struct adf4159_state	adf4159_st;
};

struct adf4159_init_param {
	/* SPI */
	struct no_os_spi_init_param	spi_init;
	/* GPIO */
	struct no_os_gpio_init_param	gpio_le;
	struct no_os_gpio_init_param	gpio_ce;
};

/* Initialize the SPI communication with the device. */
int8_t adf4159_init(struct adf4159_dev **device,
		    struct adf4159_init_param init_param);

/* Free the resources allocated by adf4159_init(). */
int32_t adf4159_remove(struct adf4159_dev *dev);

/* Transmits 32 bits on SPI. */
int8_t adf4159_set(struct adf4159_dev *dev,
		   uint32_t value);

/* Increases the R counter value until the PFD frequency is
   smaller than ADF4159_MAX_FREQ_PFD. */
int32_t adf4159_tune_r_cnt(struct adf4159_dev *dev,
			   int32_t r_cnt);

/* Sets the ADF4159 output frequency. */
double adf4159_set_freq(struct adf4159_dev *dev,
			double freq);

#endif /* _ADF4159_H_ */
