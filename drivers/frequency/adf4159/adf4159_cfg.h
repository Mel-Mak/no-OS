/***************************************************************************//**
 *   @file   adf4159_cfg.h
 *   @brief  Header file of ADF4159 Driver Configuration.
 *          Default config matches the CN0566 (ADALM-PHASER) Linux DT overlay.
 *   @author Melissa Makonga
 ********************************************************************************
 * Copyright 2020(c) Analog Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *******************************************************************************/

#ifndef __ADF4159_CFG_H__
#define __ADF4159_CFG_H__

#include "adf4159.h"

/* Default configuration matching the CN0566 Linux DT overlay properties:
 *   adi,clkin = 100 MHz
 *   adi,power-up-frequency = 3 GHz
 *   adi,charge-pump-current = 900 uA
 *   adi,muxout-select = 15
 *   adi,clk1-div = 100
 *   adi,pd-polarity-positive
 *   adi,ramp-mode = 0  (disabled)
 *   adi,ramp-status = 3
 *   adi,deviation = 1000
 *   adi,deviation-offset = 1
 */
static const struct adf4159_config adf4159_default_cfg = {
	.frequency      = 3000000000ULL,  /* 3 GHz power-up */
	.clkin          = 100000000,      /* 100 MHz reference */
	.ref_doubler_en = 0,
	.ref_div2_en    = 0,
	.ref_div_factor = 1,              /* R counter = 1 ? fpfd = 100 MHz */
	.cp_curr_uA     = 900,            /* 900 uA charge pump */
	.pd_pol_pos     = 0,              /* Positive PD polarity */
	.muxout         = 15,             /* From overlay */
	.clk1_div       = 100,
	.clk2_div       = {0, 0},
	.clk_div_mode   = 0,
	.ramp_mode      = 0,              /* Ramp disabled */
	.ramp_status    = 3,
	.deviation      = {1000, 0},
	.deviation_offs = 1,
	.step_word      = {0, 0},
	.delay_start_word = 0,
	.phase          = 0,
	.interrupt_mode = 0,
	.neg_bleed_en   = 0,
	.neg_bleed_curr = 0,
};

#endif /* __ADF4159_CFG_H__ */
