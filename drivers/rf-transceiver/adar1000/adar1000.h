/***************************************************************************//**
 * @file   adar1000.h
 * @brief  ADAR1000 4-channel X/Ku band beamformer no-OS driver.
 * @author Melissa Makonga
 ******************************************************************************
 * Copyright 2025(c) Analog Devices, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of Analog Devices, Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES, INC. "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES ARE DISCLAIMED. IN NO EVENT SHALL ANALOG DEVICES, INC. BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES.
 ******************************************************************************/

#ifndef ADAR1000_H_
#define ADAR1000_H_

#include <stdint.h>
#include "no_os_spi.h"
#include "no_os_util.h"

/* Register addresses */
#define ADAR1000_INTERFACE_CFG_A          0x000U
#define ADAR1000_INTERFACE_CFG_B          0x001U
#define ADAR1000_DEVICE_CFG               0x002U
#define ADAR1000_CHIP_TYPE                0x003U
#define ADAR1000_PRODUCT_ID_H             0x004U
#define ADAR1000_PRODUCT_ID_L             0x005U
#define ADAR1000_SCRATCH_PAD              0x00AU
#define ADAR1000_CH_RX_GAIN(ch)           (0x010U + (ch))
#define ADAR1000_CH_RX_PHASE_I(ch)        (0x014U + (2U * (ch)))
#define ADAR1000_CH_RX_PHASE_Q(ch)        (0x015U + (2U * (ch)))
#define ADAR1000_LD_WRK_REGS              0x028U
#define ADAR1000_RX_ENABLES               0x02EU
#define ADAR1000_MISC_ENABLES             0x030U
#define ADAR1000_SW_CTRL                  0x031U
#define ADAR1000_BIAS_CURRENT_RX_LNA      0x034U
#define ADAR1000_BIAS_CURRENT_RX          0x035U
#define ADAR1000_MEM_CTRL                 0x038U
#define ADAR1000_LDO_TRIM_CTL_0           0x400U
#define ADAR1000_LDO_TRIM_CTL_1           0x401U

/* INTERFACE_CFG_A: mirrored bits must be written identically. */
#define ADAR1000_SOFTRESET_MSB            NO_OS_BIT(7)
#define ADAR1000_SOFTRESET_LSB            NO_OS_BIT(0)
#define ADAR1000_SDO_ACTIVE_MSB           NO_OS_BIT(4)
#define ADAR1000_SDO_ACTIVE_LSB           NO_OS_BIT(3)
#define ADAR1000_SOFTRESET                \
	(ADAR1000_SOFTRESET_MSB | ADAR1000_SOFTRESET_LSB)
#define ADAR1000_4WIRE_SPI                \
	(ADAR1000_SDO_ACTIVE_MSB | ADAR1000_SDO_ACTIVE_LSB)

/* RX gain register: bit 7 = 1 bypasses the switched attenuator. */
#define ADAR1000_CH_ATTN_BYPASS           NO_OS_BIT(7)
#define ADAR1000_RX_GAIN_MASK             NO_OS_GENMASK(6, 0)

/* LD_WRK_REGS */
#define ADAR1000_LDRX_OVERRIDE            NO_OS_BIT(0)
#define ADAR1000_LDTX_OVERRIDE            NO_OS_BIT(1)

/* RX_ENABLES */
#define ADAR1000_RX_LNA_EN                NO_OS_BIT(2)
#define ADAR1000_VM_EN                    NO_OS_BIT(1)
#define ADAR1000_VGA_EN                   NO_OS_BIT(0)
/* Datasheet RX setup value: all channels and RX blocks enabled. */
#define ADAR1000_RX_EN_ALL                0x7FU

/* MISC_ENABLES */
#define ADAR1000_SW_DRV_TR_MODE_SEL       NO_OS_BIT(7)
#define ADAR1000_BIAS_CTRL                NO_OS_BIT(6)
#define ADAR1000_BIAS_EN                  NO_OS_BIT(5)
#define ADAR1000_LNA_BIAS_OUT_EN          NO_OS_BIT(4)
#define ADAR1000_CH_DET_EN(ch)            NO_OS_BIT(3U - (ch))

/* SW_CTRL */
#define ADAR1000_SW_DRV_TR_STATE          NO_OS_BIT(7)
#define ADAR1000_TX_EN                    NO_OS_BIT(6)
#define ADAR1000_RX_EN                    NO_OS_BIT(5)
#define ADAR1000_SW_DRV_EN_TR             NO_OS_BIT(4)
#define ADAR1000_SW_DRV_EN_POL            NO_OS_BIT(3)
#define ADAR1000_TR_SOURCE                NO_OS_BIT(2)
#define ADAR1000_TR_SPI                   NO_OS_BIT(1)
#define ADAR1000_POL                      NO_OS_BIT(0)

/* MEM_CTRL */
#define ADAR1000_BEAM_RAM_BYPASS          NO_OS_BIT(6)
#define ADAR1000_BIAS_RAM_BYPASS          NO_OS_BIT(5)
#define ADAR1000_MEM_CTRL_SPI_MODE        \
	(ADAR1000_BEAM_RAM_BYPASS | ADAR1000_BIAS_RAM_BYPASS)

/* 16-bit instruction word: R/W at bit 15, device address at bits 14:13. */
#define ADAR1000_SPI_ADDR(addr)           (((addr) & 0x3U) << 13)
#define ADAR1000_SPI_READ_CMD             NO_OS_BIT(15)
#define ADAR1000_SPI_REG_MASK             NO_OS_GENMASK(12, 0)

#define ADAR1000_NUM_CHANNELS             4U
#define ADAR1000_RX_GAIN_MIN              0U
#define ADAR1000_RX_GAIN_MAX              127U
#define ADAR1000_PHASE_TABLE_SIZE         128U

#define ADAR1000_SCRATCH_PAD_VAL_1        0xADU
#define ADAR1000_SCRATCH_PAD_VAL_2        0xEAU
#define ADAR1000_LDO_TRIM_SEL_2           0x02U
#define ADAR1000_LDO_TRIM_VAL             0x55U

/* CN0566-specific direct RX configuration, sourced from configure("rx"). */
#define ADAR1000_SW_CTRL_RX               \
	(ADAR1000_SW_DRV_TR_STATE | ADAR1000_RX_EN | ADAR1000_SW_DRV_EN_TR)
#define ADAR1000_RX_BIAS_LNA_MID          8U
#define ADAR1000_RX_BIAS_VGA_VM           22U

#define ADAR1000_CH_VALID(ch)             ((ch) < ADAR1000_NUM_CHANNELS)

struct adar1000_phase {
	uint16_t degrees;
	uint16_t frac;
	uint8_t vm_i;
	uint8_t vm_q;
};

struct adar1000_init_param {
	struct no_os_spi_init_param spi_init;
	uint8_t dev_addr;
};

struct adar1000_dev {
	struct no_os_spi_desc *spi_desc;
	uint8_t dev_addr;
};

int32_t adar1000_spi_write(struct adar1000_dev *dev, uint16_t reg_addr,
			   uint8_t data);
int32_t adar1000_spi_read(struct adar1000_dev *dev, uint16_t reg_addr,
			  uint8_t *data);
int32_t adar1000_init(struct adar1000_dev **device,
		      const struct adar1000_init_param *param);
int32_t adar1000_remove(struct adar1000_dev *dev);
int32_t adar1000_read_product_id(struct adar1000_dev *dev,
				 uint16_t *product_id);
int32_t adar1000_setup_rx(struct adar1000_dev *dev);
int32_t adar1000_set_rx_gain(struct adar1000_dev *dev, uint8_t channel,
			     uint8_t gain);
int32_t adar1000_get_rx_gain_raw(struct adar1000_dev *dev, uint8_t channel,
			         uint8_t *raw);
int32_t adar1000_get_rx_gain(struct adar1000_dev *dev, uint8_t channel,
			     uint8_t *gain);
int32_t adar1000_set_all_rx_gains(struct adar1000_dev *dev,
				  const uint8_t gains[ADAR1000_NUM_CHANNELS]);
int32_t adar1000_set_rx_phase(struct adar1000_dev *dev, uint8_t channel,
			      uint16_t degrees);
int32_t adar1000_latch_rx(struct adar1000_dev *dev);
int32_t adar1000_verify_rx(struct adar1000_dev *dev, const char *label);

#endif /* ADAR1000_H_ */
