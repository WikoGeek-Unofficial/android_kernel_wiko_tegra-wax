/*
 * arch/arm/mach-tegra/cpuidle.c
 *
 * CPU idle driver for Tegra CPUs
 *
 * Copyright (c) 2010-2012, NVIDIA Corporation.
 * Copyright (c) 2011 Google, Inc.
 * Author: Colin Cross <ccross@android.com>
 *         Gary King <gking@nvidia.com>
 *
 * Rework for 3.3 by Peter De Schrijver <pdeschrijver@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/cpu.h>
#include <linux/cpuidle.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/suspend.h>
#include <linux/tick.h>
#include <linux/cpu_pm.h>
#include <linux/module.h>
#include <linux/hrtimer.h>

#include <mach/iomap.h>
#include <mach/irqs.h>

#include <trace/events/power.h>

#include "cpuidle.h"
#include "pm.h"
#include "sleep.h"
#include "timer.h"

int tegra_lp2_exit_latency;
static int tegra_lp2_power_off_time;
static unsigned int tegra_lp2_min_residency;

extern void tegra_cpu_wfi(void);

static int tegra_idle_enter_lp3(struct cpuidle_device *dev,
				int index);

struct cpuidle_driver tegra_idle_driver = {
	.name = "tegra_idle",
	.owner = THIS_MODULE,
};

static DEFINE_PER_CPU(struct cpuidle_device *, tegra_idle_device);

static int tegra_idle_enter_lp3(struct cpuidle_device *dev,
	int index)
{
	ktime_t enter, exit;
	s64 us;

	/* cpu_idle calls us with IRQs disabled */

	local_fiq_disable();

	enter = ktime_get();

	tegra_cpu_wfi();

	exit = ktime_sub(ktime_get(), enter);
	us = ktime_to_us(exit);

	local_fiq_enable();

	/* cpu_idle expects us to return with IRQs enabled */
	local_irq_enable();

	dev->last_residency = us;
	return index;
}

static bool lp2_in_idle __read_mostly = false;

#ifdef CONFIG_PM_SLEEP
static bool lp2_in_idle_modifiable __read_mostly = true;
static bool lp2_disabled_by_suspend;

void tegra_lp2_in_idle(bool enable)
{
	/* If LP2 in idle is permanently disabled it can't be re-enabled. */
	if (lp2_in_idle_modifiable) {
		lp2_in_idle = enable;
		lp2_in_idle_modifiable = enable;
		if (!enable)
			pr_warn("LP2 in idle disabled\n");
	}
}

void tegra_lp2_update_target_residency(struct cpuidle_state *state)
{
	state->target_residency = state->exit_latency +
		tegra_lp2_power_off_time;
	if (state->target_residency < tegra_lp2_min_residency)
		state->target_residency = tegra_lp2_min_residency;
}

static int tegra_idle_enter_lp2(struct cpuidle_device *dev,
	int index)
{
	ktime_t enter, exit;
	s64 us;
	struct cpuidle_state *state = &dev->states[index];
	bool entered_lp2;

	if (!lp2_in_idle || lp2_disabled_by_suspend ||
	    !tegra_lp2_is_allowed(dev, state)) {
		return dev->states[dev->safe_state_index].enter(dev,
					dev->safe_state_index);
	}

	/* cpu_idle calls us with IRQs disabled */
	trace_cpu_powergate_rcuidle((unsigned long)readl(
					    IO_ADDRESS(TEGRA_TMR1_BASE)
					    + TIMERUS_CNTR_1US),
				    POWER_CPU_POWERGATE_ENTRY);

	enter = ktime_get();

	tegra_cpu_idle_stats_lp2_ready(dev->cpu);
	entered_lp2 = tegra_idle_lp2(dev, state);

	trace_cpu_powergate_rcuidle((unsigned long)readl(
					    IO_ADDRESS(TEGRA_TMR1_BASE)
					    + TIMERUS_CNTR_1US),
				    POWER_CPU_POWERGATE_EXIT);

	exit = ktime_sub(ktime_get(), enter);
	us = ktime_to_us(exit);

	/* cpu_idle expects us to return with IRQs enabled */
	local_irq_enable();

	smp_rmb();

	/* Update LP2 latency provided no fall back to LP3 */
	if (entered_lp2) {
		tegra_lp2_set_global_latency(state);
		tegra_lp2_update_target_residency(state);
	}
	tegra_cpu_idle_stats_lp2_time(dev->cpu, us);

	dev->last_residency = (int)us;
	return (entered_lp2) ? index : 0;
}
#endif

#if defined(CONFIG_ARCH_TEGRA_HAS_SYMMETRIC_CPU_PWR_GATE)
#define	POWER_GATING_OPTION_LEN	8
static char power_gating_option[8] __read_mostly =
											{'c', 'r', 'a', 'i', 'l', '\0'};
static int power_gating_mode = TEGRA_POWER_CLUSTER_PART_CRAIL;
static struct kparam_string power_gating __read_mostly = {
	.maxlen = POWER_GATING_OPTION_LEN,
	.string = power_gating_option,
};

static int power_gating_set(const char *buffer, const struct kernel_param *kp)
{
	const struct kparam_string *kps = kp->str;
	int len = strlen(buffer);
	int mode = -1;

	if (len > kp->str->maxlen-1) {
		pr_err("%s: string %s does not fit in 6 chars.\n", kp->name, buffer);
		return -ENOSPC;
	}

	if (!strncmp(buffer, "noncpu", 6))
		mode = TEGRA_POWER_CLUSTER_PART_NONCPU;
	else if (!strncmp(buffer, "crail", 5))
		mode = TEGRA_POWER_CLUSTER_PART_CRAIL;
	else if (!strncmp(buffer, "emu", 3))
		mode = TEGRA_POWER_CLUSTER_PART_MASK;
	else if (!strncmp(buffer, "none", 4))
		mode = 0;

	if (mode >= 0) {
		strcpy(kps->string, buffer);
		kps->string[len - 1] = '\0';
		power_gating_mode = mode;
		return 0;
	}

	pr_err("%s: power gating option: noncpu, crail, emu, cpu.", kp->name);
	return -EINVAL;
}

static struct kernel_param_ops power_gating_ops = {
	.set = power_gating_set,
	.get = param_get_string,
};

module_param_cb(power_gating, &power_gating_ops, &power_gating, 0644);

int get_power_gating_partition(void)
{
	return power_gating_mode;
}
#endif

static int tegra_cpuidle_register_device(unsigned int cpu)
{
	struct cpuidle_device *dev;
	struct cpuidle_state *state;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->state_count = 0;
	dev->cpu = cpu;
	dev->power_specified = 1;

	state = &dev->states[0];
	snprintf(state->name, CPUIDLE_NAME_LEN, "LP3");
	snprintf(state->desc, CPUIDLE_DESC_LEN, "CPU flow-controlled");
	state->exit_latency = 10;
	state->target_residency = 10;
	state->power_usage = 600;
	state->flags = CPUIDLE_FLAG_TIME_VALID;
	state->enter = tegra_idle_enter_lp3;
	dev->safe_state_index = 0;
	dev->state_count++;

#ifdef CONFIG_PM_SLEEP
	state = &dev->states[1];
	snprintf(state->name, CPUIDLE_NAME_LEN, "LP2");
	snprintf(state->desc, CPUIDLE_DESC_LEN, "CPU power-gate");
	state->exit_latency = tegra_cpu_power_good_time();
	state->target_residency = tegra_cpu_power_off_time() +
		tegra_cpu_power_good_time();
	if (state->target_residency < tegra_lp2_min_residency)
		state->target_residency = tegra_lp2_min_residency;
	state->power_usage = 0;
	state->flags = CPUIDLE_FLAG_TIME_VALID;
	state->enter = tegra_idle_enter_lp2;
	dev->state_count++;
#endif

	if (cpuidle_register_device(dev)) {
		pr_err("CPU%u: failed to register idle device\n", cpu);
		kfree(dev);
		return -EIO;
	}
	per_cpu(tegra_idle_device, cpu) = dev;
	return 0;
}

static int tegra_cpuidle_pm_notify(struct notifier_block *nb,
	unsigned long event, void *dummy)
{
#ifdef CONFIG_PM_SLEEP
	if (event == PM_SUSPEND_PREPARE)
		lp2_disabled_by_suspend = true;
	else if (event == PM_POST_SUSPEND)
		lp2_disabled_by_suspend = false;
#endif

	return NOTIFY_OK;
}

static struct notifier_block tegra_cpuidle_pm_notifier = {
	.notifier_call = tegra_cpuidle_pm_notify,
};

static int __init tegra_cpuidle_init(void)
{
	unsigned int cpu;
	int ret;

	ret = cpuidle_register_driver(&tegra_idle_driver);
	if (ret) {
		pr_err("CPUidle driver registration failed\n");
		return ret;
	}

#ifdef CONFIG_PM_SLEEP
	tegra_lp2_min_residency = tegra_cpu_lp2_min_residency();
	tegra_lp2_exit_latency = tegra_cpu_power_good_time();
	tegra_lp2_power_off_time = tegra_cpu_power_off_time();

	ret = tegra_cpudile_init_soc();
	if (ret)
		return ret;
#endif

	for_each_possible_cpu(cpu) {
		ret = tegra_cpuidle_register_device(cpu);
		if (ret) {
			pr_err("CPU%u: CPUidle device registration failed\n",
				cpu);
			return ret;
		}
	}

	register_pm_notifier(&tegra_cpuidle_pm_notifier);
	return 0;
}
device_initcall(tegra_cpuidle_init);

static void __exit tegra_cpuidle_exit(void)
{
	unregister_pm_notifier(&tegra_cpuidle_pm_notifier);
	cpuidle_unregister_driver(&tegra_idle_driver);
}
module_exit(tegra_cpuidle_exit);

static int lp2_in_idle_set(const char *arg, const struct kernel_param *kp)
{
#ifdef CONFIG_PM_SLEEP
	int ret;

	/* If LP2 in idle is permanently disabled it can't be re-enabled. */
	if (lp2_in_idle_modifiable) {
		ret = param_set_bool(arg, kp);
		return ret;
	}
#endif
	return -ENODEV;
}

static int lp2_in_idle_get(char *buffer, const struct kernel_param *kp)
{
	return param_get_bool(buffer, kp);
}

static struct kernel_param_ops lp2_in_idle_ops = {
	.set = lp2_in_idle_set,
	.get = lp2_in_idle_get,
};
module_param_cb(lp2_in_idle, &lp2_in_idle_ops, &lp2_in_idle, 0644);

#if defined(CONFIG_DEBUG_FS) && defined(CONFIG_PM_SLEEP)
static int tegra_lp2_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, tegra_lp2_debug_show, inode->i_private);
}

static const struct file_operations tegra_lp2_debug_ops = {
	.open		= tegra_lp2_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init tegra_cpuidle_debug_init(void)
{
	struct dentry *dir;
	struct dentry *d;

	dir = debugfs_create_dir("cpuidle", NULL);
	if (!dir)
		return -ENOMEM;

	d = debugfs_create_file("lp2", S_IRUGO, dir, NULL,
		&tegra_lp2_debug_ops);
	if (!d)
		return -ENOMEM;

	return 0;
}

late_initcall(tegra_cpuidle_debug_init);
#endif
