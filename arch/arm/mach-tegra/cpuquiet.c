/*
 * arch/arm/mach-tegra/cpuquiet.c
 *
 * Cpuquiet driver for Tegra3 CPUs
 *
 * Copyright (c) 2012 NVIDIA CORPORATION.  All rights reserved.
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
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/cpu.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/cpuquiet.h>
#include <linux/pm_qos.h>
#include <linux/debugfs.h>

#include "pm.h"
#include "cpu-tegra.h"
#include "clock.h"

#define INITIAL_STATE		TEGRA_CPQ_DISABLED
#define UP_DELAY_MS		70
#define DOWN_DELAY_MS		2000

static struct mutex *tegra3_cpu_lock;
static struct workqueue_struct *cpuquiet_wq;
static struct delayed_work cpuquiet_work;
static struct work_struct minmax_work;

static struct kobject *tegra_auto_sysfs_kobject;

static bool no_lp;
static bool enable;
static unsigned long up_delay;
static unsigned long down_delay;
static int mp_overhead = 10;
static unsigned int idle_top_freq;
static unsigned int idle_bottom_freq;

static struct clk *cpu_clk;
static struct clk *cpu_g_clk;
static struct clk *cpu_lp_clk;

static struct cpumask cr_online_requests;

enum {
	TEGRA_CPQ_DISABLED = 0,
	TEGRA_CPQ_IDLE,
	TEGRA_CPQ_SWITCH_TO_LP,
	TEGRA_CPQ_SWITCH_TO_G,
};

static int cpq_state;


static struct {
	cputime64_t time_up_total;
	u64 last_update;
	unsigned int up_down_count;
} hp_stats[CONFIG_NR_CPUS + 1];	/* Append LP CPU entry at the end */

static void hp_init_stats(void)
{
	int i;
	u64 cur_jiffies = get_jiffies_64();

	for (i = 0; i <= CONFIG_NR_CPUS; i++) {
		hp_stats[i].time_up_total = 0;
		hp_stats[i].last_update = cur_jiffies;

		hp_stats[i].up_down_count = 0;
		if (is_lp_cluster()) {
			if (i == CONFIG_NR_CPUS)
				hp_stats[i].up_down_count = 1;
		} else {
			if ((i < nr_cpu_ids) && cpu_online(i))
				hp_stats[i].up_down_count = 1;
		}
	}

}

static void hp_stats_update(unsigned int cpu, bool up)
{
	u64 cur_jiffies = get_jiffies_64();
	bool was_up = hp_stats[cpu].up_down_count & 0x1;

	if (was_up)
		hp_stats[cpu].time_up_total =
			hp_stats[cpu].time_up_total +
			(cur_jiffies - hp_stats[cpu].last_update);

	if (was_up != up) {
		hp_stats[cpu].up_down_count++;
		if ((hp_stats[cpu].up_down_count & 0x1) != up) {
			/* FIXME: sysfs user space CPU control breaks stats */
			pr_err("tegra hotplug stats out of sync with %s CPU%d",
			       (cpu < CONFIG_NR_CPUS) ? "G" : "LP",
			       (cpu < CONFIG_NR_CPUS) ?  cpu : 0);
			hp_stats[cpu].up_down_count ^=  0x1;
		}
	}
	hp_stats[cpu].last_update = cur_jiffies;
}




static int update_core_config(unsigned int cpunumber, bool up)
{
	int ret = -EINVAL;
	unsigned int nr_cpus = num_online_cpus();
	int max_cpus = pm_qos_request(PM_QOS_MAX_ONLINE_CPUS) ? : 4;
	int min_cpus = pm_qos_request(PM_QOS_MIN_ONLINE_CPUS);

	if (cpq_state == TEGRA_CPQ_DISABLED || cpunumber >= nr_cpu_ids)
		return ret;

	if (up) {
		if(is_lp_cluster()) {
			cpumask_set_cpu(cpunumber, &cr_online_requests);
			ret = -EBUSY;
		} else if (tegra_cpu_edp_favor_up(nr_cpus, mp_overhead) &&
			   nr_cpus < max_cpus) {
			hp_stats_update(cpunumber, true);
			ret = cpu_up(cpunumber);
		}
	} else {
		if (is_lp_cluster()) {
			ret = -EBUSY;
		} else if (nr_cpus > min_cpus) {
			hp_stats_update(cpunumber, false);
			ret = cpu_down(cpunumber);
		}
	}

	return ret;
}

static int tegra_quiesence_cpu(unsigned int cpunumber)
{
        return update_core_config(cpunumber, false);
}

static int tegra_wake_cpu(unsigned int cpunumber)
{
        return update_core_config(cpunumber, true);
}

static struct cpuquiet_driver tegra_cpuquiet_driver = {
        .name                   = "tegra",
        .quiesence_cpu          = tegra_quiesence_cpu,
        .wake_cpu               = tegra_wake_cpu,
};

static void apply_core_config(void)
{
	unsigned int cpu;

	if (is_lp_cluster() || cpq_state == TEGRA_CPQ_DISABLED)
		return;

	for_each_cpu_mask(cpu, cr_online_requests) {
		if (cpu < nr_cpu_ids && !cpu_online(cpu))
			if (!tegra_wake_cpu(cpu))
				cpumask_clear_cpu(cpu, &cr_online_requests);
	}
}

static void tegra_cpuquiet_work_func(struct work_struct *work)
{
	int device_busy = -1;

	mutex_lock(tegra3_cpu_lock);

	switch(cpq_state) {
		case TEGRA_CPQ_DISABLED:
		case TEGRA_CPQ_IDLE:
			break;
		case TEGRA_CPQ_SWITCH_TO_G:
			if (is_lp_cluster()) {
				if(!clk_set_parent(cpu_clk, cpu_g_clk)) {
					hp_stats_update(CONFIG_NR_CPUS, false);
					hp_stats_update(0, true);
					/*catch-up with governor target speed */
					tegra_cpu_set_speed_cap(NULL);
					/* process pending core requests*/
					device_busy = 0;
				}
			}
			break;
		case TEGRA_CPQ_SWITCH_TO_LP:
			if (!is_lp_cluster() && !no_lp &&
					num_online_cpus() == 1) {
				if (!clk_set_parent(cpu_clk, cpu_lp_clk)) {
					hp_stats_update(CONFIG_NR_CPUS, true);
					hp_stats_update(0, false);
					/*catch-up with governor target speed*/
					tegra_cpu_set_speed_cap(NULL);
					device_busy = 1;
				}
			}
			break;
		default:
			pr_err("%s: invalid tegra hotplug state %d\n",
		       __func__, cpq_state);
	}

	mutex_unlock(tegra3_cpu_lock);

	if (device_busy == 1) {
		cpuquiet_device_busy();
	} else if (!device_busy) {
		apply_core_config();
		cpuquiet_device_free();
	}
}

static void min_max_constraints_workfunc(struct work_struct *work)
{
	int count = -1;
	bool up = false;
	unsigned int cpu;

	int nr_cpus = num_online_cpus();
	int max_cpus = pm_qos_request(PM_QOS_MAX_ONLINE_CPUS) ? : 4;
	int min_cpus = pm_qos_request(PM_QOS_MIN_ONLINE_CPUS);

	if (cpq_state == TEGRA_CPQ_DISABLED)
		return;

	if (is_lp_cluster())
		return;

	if (nr_cpus < min_cpus) {
		up = true;
		count = min_cpus - nr_cpus;
	} else if (nr_cpus > max_cpus && max_cpus >= min_cpus) {
		count = nr_cpus - max_cpus;
	}

	for (;count > 0; count--) {
		if (up) {
			cpu = cpumask_next_zero(0, cpu_online_mask);
			if (cpu < nr_cpu_ids) {
				hp_stats_update(cpu, true);
				cpu_up(cpu);
			} else {
				break;
			}
		} else {
			cpu = cpumask_next(0, cpu_online_mask);
			if (cpu < nr_cpu_ids) {
				hp_stats_update(cpu, false);
				cpu_down(cpu);
			} else {
				break;
			}
		}
	}
}

static int min_cpus_notify(struct notifier_block *nb, unsigned long n, void *p)
{
	bool g_cluster = false;

	if (cpq_state == TEGRA_CPQ_DISABLED)
		return NOTIFY_OK;

	mutex_lock(tegra3_cpu_lock);

	if ((n >= 1) && is_lp_cluster()) {
		/* make sure cpu rate is within g-mode range before switching */
		unsigned long speed = max((unsigned long)tegra_getspeed(0),
					clk_get_min_rate(cpu_g_clk) / 1000);
		tegra_update_cpu_speed(speed);

		if (!clk_set_parent(cpu_clk, cpu_g_clk)) {
			hp_stats_update(CONFIG_NR_CPUS, false);
			hp_stats_update(0, true);
			g_cluster = true;
		}
	}

	tegra_cpu_set_speed_cap(NULL);
	mutex_unlock(tegra3_cpu_lock);

	schedule_work(&minmax_work);

	if (g_cluster)
		cpuquiet_device_free();

	return NOTIFY_OK;
}

static int max_cpus_notify(struct notifier_block *nb, unsigned long n, void *p)
{
	if (cpq_state == TEGRA_CPQ_DISABLED)
		return NOTIFY_OK;

	if (n < num_online_cpus())
		schedule_work(&minmax_work);

	return NOTIFY_OK;
}

void tegra_auto_hotplug_governor(unsigned int cpu_freq, bool suspend)
{
	if (!is_g_cluster_present())
		return;

	if (cpq_state == TEGRA_CPQ_DISABLED)
		return;

	if (suspend) {
		cpq_state = TEGRA_CPQ_IDLE;

		/* Switch to G-mode if suspend rate is high enough */
		if (is_lp_cluster() && (cpu_freq >= idle_bottom_freq)) {
			if (!clk_set_parent(cpu_clk, cpu_g_clk)) {
				hp_stats_update(CONFIG_NR_CPUS, false);
				hp_stats_update(0, true);
				cpuquiet_device_free();
			}
		}
		return;
	}

	if (is_lp_cluster() && pm_qos_request(PM_QOS_MIN_ONLINE_CPUS) >= 2) {
		if (cpq_state != TEGRA_CPQ_SWITCH_TO_G) {
			/* Force switch */
			cpq_state = TEGRA_CPQ_SWITCH_TO_G;
			queue_delayed_work(
				cpuquiet_wq, &cpuquiet_work, up_delay);
		}
		return;
	}

	if (is_lp_cluster() && (cpu_freq >= idle_top_freq || no_lp)) {
		cpq_state = TEGRA_CPQ_SWITCH_TO_G;
		queue_delayed_work(cpuquiet_wq, &cpuquiet_work, up_delay);
	} else if (!is_lp_cluster() && !no_lp &&
		   cpu_freq <= idle_bottom_freq) {
		cpq_state = TEGRA_CPQ_SWITCH_TO_LP;
		queue_delayed_work(cpuquiet_wq, &cpuquiet_work, down_delay);
	} else {
		cpq_state = TEGRA_CPQ_IDLE;
	}
}

static struct notifier_block min_cpus_notifier = {
	.notifier_call = min_cpus_notify,
};

static struct notifier_block max_cpus_notifier = {
	.notifier_call = max_cpus_notify,
};

static void delay_callback(struct cpuquiet_attribute *attr)
{
	unsigned long val;

	if (attr) {
		val = (*((unsigned long *)(attr->param)));
		(*((unsigned long *)(attr->param))) = msecs_to_jiffies(val);
	}
}

static void enable_callback(struct cpuquiet_attribute *attr)
{
	int disabled = -1;

	mutex_lock(tegra3_cpu_lock);

	if (!enable && cpq_state != TEGRA_CPQ_DISABLED) {
		disabled = 1;
		cpq_state = TEGRA_CPQ_DISABLED;
	} else if (enable && cpq_state == TEGRA_CPQ_DISABLED) {
		disabled = 0;
		cpq_state = TEGRA_CPQ_IDLE;
		tegra_cpu_set_speed_cap(NULL);
		hp_init_stats();
	}

	mutex_unlock(tegra3_cpu_lock);

	if (disabled == -1)
		return;

	if (disabled == 1) {
		cancel_delayed_work_sync(&cpuquiet_work);
		pr_info("Tegra cpuquiet clusterswitch disabled\n");
		cpuquiet_device_busy();
	} else if (!disabled) {
		pr_info("Tegra cpuquiet clusterswitch enabled\n");
		cpuquiet_device_free();
	}
}

CPQ_BASIC_ATTRIBUTE(no_lp, 0644, bool);
CPQ_BASIC_ATTRIBUTE(idle_top_freq, 0644, uint);
CPQ_BASIC_ATTRIBUTE(idle_bottom_freq, 0644, uint);
CPQ_BASIC_ATTRIBUTE(mp_overhead, 0644, int);
CPQ_ATTRIBUTE(up_delay, 0644, ulong, delay_callback);
CPQ_ATTRIBUTE(down_delay, 0644, ulong, delay_callback);
CPQ_ATTRIBUTE(enable, 0644, bool, enable_callback);

static struct attribute *tegra_auto_attributes[] = {
	&no_lp_attr.attr,
	&up_delay_attr.attr,
	&down_delay_attr.attr,
	&idle_top_freq_attr.attr,
	&idle_bottom_freq_attr.attr,
	&mp_overhead_attr.attr,
	&enable_attr.attr,
	NULL,
};

static const struct sysfs_ops tegra_auto_sysfs_ops = {
	.show = cpuquiet_auto_sysfs_show,
	.store = cpuquiet_auto_sysfs_store,
};

static struct kobj_type ktype_sysfs = {
	.sysfs_ops = &tegra_auto_sysfs_ops,
	.default_attrs = tegra_auto_attributes,
};

static int tegra_auto_sysfs(void)
{
	int err;

	tegra_auto_sysfs_kobject = kzalloc(sizeof(*tegra_auto_sysfs_kobject),
					GFP_KERNEL);

	if (!tegra_auto_sysfs_kobject)
		return -ENOMEM;

	err = cpuquiet_kobject_init(tegra_auto_sysfs_kobject, &ktype_sysfs,
				"tegra_cpuquiet");

	if (err)
		kfree(tegra_auto_sysfs_kobject);

	return err;
}

#ifdef CONFIG_DEBUG_FS

static struct dentry *hp_debugfs_root;

static int hp_stats_show(struct seq_file *s, void *data)
{
	int i;
	u64 cur_jiffies = get_jiffies_64();

	mutex_lock(tegra3_cpu_lock);
	if (cpq_state != TEGRA_CPQ_DISABLED) {
		for (i = 0; i <= CONFIG_NR_CPUS; i++) {
			bool was_up = (hp_stats[i].up_down_count & 0x1);
			hp_stats_update(i, was_up);
		}
	}
	mutex_unlock(tegra3_cpu_lock);

	seq_printf(s, "%-15s ", "cpu:");
	for (i = 0; i < CONFIG_NR_CPUS; i++)
		seq_printf(s, "G%-9d ", i);
	seq_printf(s, "LP\n");

	seq_printf(s, "%-15s ", "transitions:");
	for (i = 0; i <= CONFIG_NR_CPUS; i++)
		seq_printf(s, "%-10u ", hp_stats[i].up_down_count);
	seq_printf(s, "\n");

	seq_printf(s, "%-15s ", "time plugged:");
	for (i = 0; i <= CONFIG_NR_CPUS; i++) {
		seq_printf(s, "%-10llu ",
			   cputime64_to_clock_t(hp_stats[i].time_up_total));
	}
	seq_printf(s, "\n");

	seq_printf(s, "%-15s %llu\n", "time-stamp:",
		   cputime64_to_clock_t(cur_jiffies));

	return 0;
}


static int hp_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, hp_stats_show, inode->i_private);
}


static const struct file_operations hp_stats_fops = {
	.open		= hp_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


struct pm_qos_request min_cpu_req;
struct pm_qos_request max_cpu_req;

static int min_cpus_get(void *data, u64 *val)
{
	*val = pm_qos_request(PM_QOS_MIN_ONLINE_CPUS);
	return 0;
}
static int min_cpus_set(void *data, u64 val)
{
	pm_qos_update_request(&min_cpu_req, (s32)val);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(min_cpus_fops, min_cpus_get, min_cpus_set, "%llu\n");

static int max_cpus_get(void *data, u64 *val)
{
	*val = pm_qos_request(PM_QOS_MAX_ONLINE_CPUS);
	return 0;
}
static int max_cpus_set(void *data, u64 val)
{
	pm_qos_update_request(&max_cpu_req, (s32)val);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(max_cpus_fops, max_cpus_get, max_cpus_set, "%llu\n");

static int __init tegra_cpuquiet_debug_init(void)
{
	if (!tegra3_cpu_lock)
		return -ENOENT;

	hp_debugfs_root = debugfs_create_dir("tegra_hotplug", NULL);
	if (!hp_debugfs_root)
		return -ENOMEM;

	pm_qos_add_request(&min_cpu_req, PM_QOS_MIN_ONLINE_CPUS,
			   PM_QOS_DEFAULT_VALUE);
	pm_qos_add_request(&max_cpu_req, PM_QOS_MAX_ONLINE_CPUS,
			   PM_QOS_DEFAULT_VALUE);

	if (!debugfs_create_file(
		"min_cpus", S_IRUGO, hp_debugfs_root, NULL, &min_cpus_fops))
		goto err_out;

	if (!debugfs_create_file(
		"max_cpus", S_IRUGO, hp_debugfs_root, NULL, &max_cpus_fops))
		goto err_out;

	if (!debugfs_create_file(
		"stats", S_IRUGO, hp_debugfs_root, NULL, &hp_stats_fops))
		goto err_out;

	return 0;

err_out:
	debugfs_remove_recursive(hp_debugfs_root);
	pm_qos_remove_request(&min_cpu_req);
	pm_qos_remove_request(&max_cpu_req);
	return -ENOMEM;
}

late_initcall(tegra_cpuquiet_debug_init);

#endif /* CONFIG_DEBUG_FS */


int tegra_auto_hotplug_init(struct mutex *cpu_lock)
{
	int err;

	cpu_clk = clk_get_sys(NULL, "cpu");
	cpu_g_clk = clk_get_sys(NULL, "cpu_g");
	cpu_lp_clk = clk_get_sys(NULL, "cpu_lp");

	if (IS_ERR(cpu_clk) || IS_ERR(cpu_g_clk) || IS_ERR(cpu_lp_clk))
		return -ENOENT;

	/*
	 * Not bound to the issuer CPU (=> high-priority), has rescue worker
	 * task, single-threaded, freezable.
	 */
	cpuquiet_wq = alloc_workqueue(
		"cpuquiet", WQ_UNBOUND | WQ_RESCUER | WQ_FREEZABLE, 1);

	if (!cpuquiet_wq)
		return -ENOMEM;

	INIT_DELAYED_WORK(&cpuquiet_work, tegra_cpuquiet_work_func);
	INIT_WORK(&minmax_work, min_max_constraints_workfunc);

	idle_top_freq = clk_get_max_rate(cpu_lp_clk) / 1000;
	idle_bottom_freq = clk_get_min_rate(cpu_g_clk) / 1000;

	up_delay = msecs_to_jiffies(UP_DELAY_MS);
	down_delay = msecs_to_jiffies(DOWN_DELAY_MS);
	cpumask_clear(&cr_online_requests);
	tegra3_cpu_lock = cpu_lock;

	cpq_state = INITIAL_STATE;
	enable = cpq_state == TEGRA_CPQ_DISABLED ? false : true;
	hp_init_stats();

	pr_info("Tegra cpuquiet initialized: %s\n",
		(cpq_state == TEGRA_CPQ_DISABLED) ? "disabled" : "enabled");

	if (pm_qos_add_notifier(PM_QOS_MIN_ONLINE_CPUS, &min_cpus_notifier))
		pr_err("%s: Failed to register min cpus PM QoS notifier\n",
			__func__);
	if (pm_qos_add_notifier(PM_QOS_MAX_ONLINE_CPUS, &max_cpus_notifier))
		pr_err("%s: Failed to register max cpus PM QoS notifier\n",
			__func__);

	err = cpuquiet_register_driver(&tegra_cpuquiet_driver);
	if (err) {
		destroy_workqueue(cpuquiet_wq);
		return err;
	}

	err = tegra_auto_sysfs();
	if (err) {
		cpuquiet_unregister_driver(&tegra_cpuquiet_driver);
		destroy_workqueue(cpuquiet_wq);
	}

	return err;
}

void tegra_auto_hotplug_exit(void)
{
	destroy_workqueue(cpuquiet_wq);
        cpuquiet_unregister_driver(&tegra_cpuquiet_driver);
	kobject_put(tegra_auto_sysfs_kobject);

#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(hp_debugfs_root);
	pm_qos_remove_request(&min_cpu_req);
	pm_qos_remove_request(&max_cpu_req);
#endif
}
