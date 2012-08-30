/*
 * arch/arm/mach-tegra/tegra11_speedo.c
 *
 * Copyright (C) 2012 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/bug.h>			/* For BUG_ON.  */

#include <mach/iomap.h>
#include <mach/tegra_fuse.h>
#include <mach/hardware.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include "fuse.h"


static int cpu_process_id;
static int core_process_id;
static int cpu_speedo_id;
static int cpu_speedo_value;
static int soc_speedo_id;
static int package_id;

static int enable_app_profiles;

void tegra_init_speedo_data(void)
{
	cpu_speedo_value = 1777;

	pr_info("Tegra11: CPU Speedo ID %d, Soc Speedo ID %d",
		 cpu_speedo_id, soc_speedo_id);
}

int tegra_cpu_process_id(void)
{
	return cpu_process_id;
}

int tegra_core_process_id(void)
{
	return core_process_id;
}

int tegra_cpu_speedo_id(void)
{
	return cpu_speedo_id;
}

int tegra_soc_speedo_id(void)
{
	return soc_speedo_id;
}

int tegra_package_id(void)
{
	return package_id;
}

int tegra_cpu_speedo_value(void)
{
	return cpu_speedo_value;
}

/*
 * CPU and core nominal voltage levels as determined by chip SKU and speedo
 * (not final - can be lowered by dvfs tables and rail dependencies; the
 * latter is resolved by the dvfs code)
 */
int tegra_cpu_speedo_mv(void)
{
	/* Not applicable on Tegar11 */
	return -ENOSYS;
}

int tegra_core_speedo_mv(void)
{
	switch (soc_speedo_id) {
	case 0:
		return 1100;
	default:
		BUG();
	}
}

static int get_enable_app_profiles(char *val, const struct kernel_param *kp)
{
	return param_get_uint(val, kp);
}

static struct kernel_param_ops tegra_profiles_ops = {
	.get = get_enable_app_profiles,
};

module_param_cb(tegra_enable_app_profiles,
	&tegra_profiles_ops, &enable_app_profiles, 0444);
