/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TINNO_FLASH_H__
#define __TINNO_FLASH_H__

#include <media/nvc_torch.h>

struct tinno_flash_power_rail {
	/* to enable the module power */
	struct regulator *vin;
	/* to enable the host interface power */
	struct regulator *vdd;
};

struct tinno_flash_led_config {
	u16 flash_torch_ratio;	/* max flash to max torch ratio, in 1/1000 */
	u16 granularity;	/* 1, 10, 100, ... to carry float settings */
	u16 flash_levels;	/* calibrated flash levels < 32 */
	/* this table contains the calibrated flash level - luminance pair */
	struct nvc_torch_lumi_level_v1 *lumi_levels;
};

struct tinno_flash_config {
	struct tinno_flash_led_config led_config[1];
};

struct tinno_flash_platform_data {
	struct tinno_flash_config config;
	const char *dev_name; /* see implementation notes in driver */
	struct nvc_torch_pin_state pinstate; /* see notes in driver */
	struct edp_client edpc_config;
	int cfg;
	int num;
	int (*poweron_callback)(struct tinno_flash_power_rail *pw);
	int (*poweroff_callback)(struct tinno_flash_power_rail *pw);
	int gpio_en_torch;
	int gpio_en_flash;
	int edp_state_flash;
	int edp_state_torch;
};

#endif

