/***************************************************************************//**
 *   @file   adf4159_cfg.h
 *   @brief  Header file of ADF4159 Driver Configuration.
 *          Default config matches the CN0566 (ADALM-PHASER) Linux DT overlay.
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
