/*
 * arch/arm/mach-tegra/tegra11_dvfs.c
 *
 * Copyright (C) 2012 NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/kobject.h>
#include <linux/err.h>

#include "clock.h"
#include "dvfs.h"
#include "fuse.h"
#include "board.h"
#include "tegra_cl_dvfs.h"

static bool tegra_dvfs_cpu_disabled;
static bool tegra_dvfs_core_disabled;

#define KHZ 1000
#define MHZ 1000000

/* FIXME: need tegra11 step */
#define VDD_SAFE_STEP			100

static struct dvfs_rail tegra11_dvfs_rail_vdd_cpu = {
	.reg_id = "vdd_cpu",
	.max_millivolts = 1400,
	.min_millivolts = 800,
	.step = VDD_SAFE_STEP,
	.jmp_to_zero = true,
};

static struct dvfs_rail tegra11_dvfs_rail_vdd_core = {
	.reg_id = "vdd_core",
	.max_millivolts = 1400,
	.min_millivolts = 800,
	.step = VDD_SAFE_STEP,
};

static struct dvfs_rail *tegra11_dvfs_rails[] = {
	&tegra11_dvfs_rail_vdd_cpu,
	&tegra11_dvfs_rail_vdd_core,
};

/* default cvb alignment on Tegra11 - 10mV */
int __attribute__((weak)) tegra_get_cvb_alignment_uV(void)
{
	return 10000;
}

/* CPU DVFS tables */
/* FIXME: real data */
static struct cpu_cvb_dvfs cpu_cvb_dvfs_table[] = {
	{
		.speedo_id = 0,
		.max_mv = 1250,
		.min_mv = 850,
		.margin = 112,
		.freqs_mult = MHZ,
		.speedo_scale = 100,
		.voltage_scale = 100,
		.cvb_table = {
			/*f      c0,    c1,    c2 */
			{ 306,  9251,  12837, -570},
			{ 408,  11737, 12957, -570},
			{ 510,  14282, 13076, -570},
			{ 612,  16887, 13196, -570},
			{ 714,  19552, 13315, -570},
			{ 816,  22277, 13435, -570},
			{ 918,  25061, 13554, -570},
			{1020,  27905, 13674, -570},
			{1122,  30809, 13793, -570},
			{1224,  33773, 13913, -570},
			{1326,  36797, 14032, -570},
			{1428,  39880, 14152, -570},
			{1530,  43023, 14271, -570},
			{1632,  46226, 14391, -570},
			{   0,      0,     0,    0},
		},
	}
};

static int cpu_millivolts[MAX_DVFS_FREQS];
static int cpu_dfll_millivolts[MAX_DVFS_FREQS];

static struct dvfs cpu_dvfs = {
	.clk_name	= "cpu_g",
	.process_id	= -1,
	.millivolts	= cpu_millivolts,
	.dfll_millivolts = cpu_dfll_millivolts,
	.auto_dvfs	= true,
	.dvfs_rail	= &tegra11_dvfs_rail_vdd_cpu,
};

static struct tegra_cl_dvfs_dfll_data cpu_dfll_data = {
		.dfll_clk_name	= "dfll_cpu",
		.tune0		= 0x000B0380,
		.tune1		= 0x000005e0,
		.droop_rate_min = 1000000,
};

/* Core DVFS tables */
/* FIXME: real data */
static const int core_millivolts[MAX_DVFS_FREQS] = {
	837,  900,  950, 1000, 1050, 1100, 1125};

#define CORE_DVFS(_clk_name, _speedo_id, _auto, _mult, _freqs...)	\
	{							\
		.clk_name	= _clk_name,			\
		.speedo_id	= _speedo_id,			\
		.process_id	= -1,				\
		.freqs		= {_freqs},			\
		.freqs_mult	= _mult,			\
		.millivolts	= core_millivolts,		\
		.auto_dvfs	= _auto,			\
		.dvfs_rail	= &tegra11_dvfs_rail_vdd_core,	\
	}

static struct dvfs core_dvfs_table[] = {
	/* Core voltages (mV):		    837,    900,    950,   1000,   1050,    1100,    1125, */
	/* Clock limits for internal blocks, PLLs */
#ifndef CONFIG_TEGRA_SIMULATION_PLATFORM
	CORE_DVFS("cpu_lp", -1, 1, KHZ,       1, 144000, 252000, 288000, 372000,  468000,  468000),
	CORE_DVFS("emc",   -1, 1, KHZ,        1, 264000, 348000, 384000, 528000,  666000,  666000),
	CORE_DVFS("sbus",  -1, 1, KHZ,        1,  81600, 102000, 136000, 204000,  204000,  204000),

	CORE_DVFS("vi",    -1, 1, KHZ,        1, 102000, 144000, 144000, 192000,  240000,  240000),

	CORE_DVFS("2d",    -1, 1, KHZ,        1, 132000, 180000, 204000, 264000,  336000,  336000),
	CORE_DVFS("3d",    -1, 1, KHZ,        1, 132000, 180000, 204000, 264000,  336000,  336000),

	CORE_DVFS("epp",   -1, 1, KHZ,        1, 120000, 144000, 168000, 216000,  276000,  276000),
	CORE_DVFS("msenc", -1, 1, KHZ,        1, 120000, 144000, 168000, 216000,  276000,  276000),
	CORE_DVFS("se",    -1, 1, KHZ,        1, 120000, 144000, 168000, 216000,  276000,  276000),
	CORE_DVFS("tsec",  -1, 1, KHZ,        1, 120000, 144000, 168000, 216000,  276000,  276000),
	CORE_DVFS("vde",   -1, 1, KHZ,        1, 120000, 144000, 168000, 216000,  276000,  276000),

	CORE_DVFS("host1x", -1, 1, KHZ,       1,  81600, 102000, 136000, 163200,  204000,  204000),

#ifdef CONFIG_TEGRA_DUAL_CBUS
	CORE_DVFS("c2bus", -1, 1, KHZ,        1, 132000, 180000, 204000, 264000,  336000,  336000),
	CORE_DVFS("c3bus", -1, 1, KHZ,        1, 120000, 144000, 168000, 216000,  276000,  276000),
#else
	CORE_DVFS("cbus",  -1, 1, KHZ,        1, 120000, 144000, 168000, 216000,  276000,  276000),
#endif

	CORE_DVFS("pll_m", -1, 1, KHZ,        1, 480000, 588000, 660000, 792000,  936000,  936000),
	CORE_DVFS("pll_c", -1, 1, KHZ,        1, 480000, 588000, 660000, 792000,  936000,  936000),
	CORE_DVFS("pll_c2", -1, 1, KHZ,       1, 480000, 588000, 660000, 792000,  936000,  936000),
	CORE_DVFS("pll_c3", -1, 1, KHZ,       1, 480000, 588000, 660000, 792000,  936000,  936000),
	CORE_DVFS("pll_d_out0", -1, 1, KHZ,   1, 480000, 588000, 660000, 792000,  936000,  936000),
	CORE_DVFS("pll_d2_out0", -1, 1, KHZ,  1, 480000, 588000, 660000, 792000,  936000,  936000),
	CORE_DVFS("pll_re_out", -1, 1, KHZ,   1, 480000, 588000, 660000, 792000,  936000,  936000),

	/* Core voltages (mV):		    837,    900,    950,   1000,   1050,    1100,    1125, */
	/* Clock limits for I/O peripherals */
	CORE_DVFS("i2c1", -1, 1, KHZ,         1,  58300,  68000,  81600, 102000,  136000,  136000),
	CORE_DVFS("i2c2", -1, 1, KHZ,         1,  58300,  68000,  81600, 102000,  136000,  136000),
	CORE_DVFS("i2c3", -1, 1, KHZ,         1,  58300,  68000,  81600, 102000,  136000,  136000),
	CORE_DVFS("i2c4", -1, 1, KHZ,         1,  58300,  68000,  81600, 102000,  136000,  136000),

	CORE_DVFS("sbc1", -1, 1, KHZ,         1,  24000,  24000,  48000,  48000,   48000,   48000),
	CORE_DVFS("sbc2", -1, 1, KHZ,         1,  24000,  24000,  48000,  48000,   48000,   48000),
	CORE_DVFS("sbc3", -1, 1, KHZ,         1,  24000,  24000,  48000,  48000,   48000,   48000),
	CORE_DVFS("sbc4", -1, 1, KHZ,         1,  24000,  24000,  48000,  48000,   48000,   48000),
	CORE_DVFS("sbc5", -1, 1, KHZ,         1,  24000,  24000,  48000,  48000,   48000,   48000),
	CORE_DVFS("sbc6", -1, 1, KHZ,         1,  24000,  24000,  48000,  48000,   48000,   48000),

	/* FIXME: re-enable after UART hs driver is updated */
#if 0
	CORE_DVFS("uarta", -1, 1, KHZ,        1,  58300,  58300, 102000, 102000,  102000,  102000),
	CORE_DVFS("uartb", -1, 1, KHZ,        1,  58300,  58300, 102000, 102000,  102000,  102000),
	CORE_DVFS("uartc", -1, 1, KHZ,        1,  58300,  58300, 102000, 102000,  102000,  102000),
	CORE_DVFS("uartd", -1, 1, KHZ,        1,  58300,  58300, 102000, 102000,  102000,  102000),
	CORE_DVFS("uarte", -1, 1, KHZ,        1,  58300,  58300, 102000, 102000,  102000,  102000),
#endif
	CORE_DVFS("sdmmc1", -1, 1, KHZ,       1, 102000, 102000, 163200, 163200,  163200,  163200),
	CORE_DVFS("sdmmc2", -1, 1, KHZ,       1, 102000, 102000, 163200, 163200,  163200,  163200),
	CORE_DVFS("sdmmc3", -1, 1, KHZ,       1, 102000, 102000, 163200, 163200,  163200,  163200),
	CORE_DVFS("sdmmc4", -1, 1, KHZ,       1, 102000, 102000, 163200, 163200,  163200,  163200),

	CORE_DVFS("pwm",  -1, 1, KHZ,         1,  40800,  48000,  48000,  48000,   48000,   48000),

	CORE_DVFS("csi",  -1, 1, KHZ,         1,      1,      1, 102000, 102000,  102000,  102000),
	CORE_DVFS("dsia", -1, 1, KHZ,         1, 100000, 125000, 125000, 125000,  125000,  125000),
	CORE_DVFS("dsib", -1, 1, KHZ,         1, 100000, 125000, 125000, 125000,  125000,  125000),
	CORE_DVFS("dsialp", -1, 1, KHZ,       1, 102000, 102000, 102000, 102000,  156000,  156000),
	CORE_DVFS("dsiblp", -1, 1, KHZ,       1, 102000, 102000, 102000, 102000,  156000,  156000),
	CORE_DVFS("hdmi", -1, 1, KHZ,         1,  99000, 118800, 148500, 198000,  198000,  198000),

	/*
	 * The clock rate for the display controllers that determines the
	 * necessary core voltage depends on a divider that is internal
	 * to the display block.  Disable auto-dvfs on the display clocks,
	 * and let the display driver call tegra_dvfs_set_rate manually
	 */
	CORE_DVFS("disp1", -1, 0, KHZ,         1, 108000, 120000, 144000, 192000,  240000,  240000),
	CORE_DVFS("disp2", -1, 0, KHZ,         1, 108000, 120000, 144000, 192000,  240000,  240000),

	CORE_DVFS("xusb_falcon_src", -1, 1, KHZ, 1, 204000, 204000, 204000, 336000,  336000,  336000),
	CORE_DVFS("xusb_host_src",   -1, 1, KHZ, 1,  51000,  51000,  51000, 112000,  112000,  112000),
	CORE_DVFS("xusb_dev_src",    -1, 1, KHZ, 1,  51000,  51000,  51000, 112000,  112000,  112000),
	CORE_DVFS("xusb_ss_src",     -1, 1, KHZ, 1,  60000,  60000,  60000, 120000,  120000,  120000),
	CORE_DVFS("xusb_fs_src",     -1, 1, KHZ, 1,      1,  48000,  48000,  48000,   48000,   48000),
	CORE_DVFS("xusb_hs_src",     -1, 1, KHZ, 1,      1,  60000,  60000,  60000,   60000,   60000),
#endif
};

int tegra_dvfs_disable_core_set(const char *arg, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_bool(arg, kp);
	if (ret)
		return ret;

	if (tegra_dvfs_core_disabled)
		tegra_dvfs_rail_disable(&tegra11_dvfs_rail_vdd_core);
	else
		tegra_dvfs_rail_enable(&tegra11_dvfs_rail_vdd_core);

	return 0;
}

int tegra_dvfs_disable_cpu_set(const char *arg, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_bool(arg, kp);
	if (ret)
		return ret;

	if (tegra_dvfs_cpu_disabled)
		tegra_dvfs_rail_disable(&tegra11_dvfs_rail_vdd_cpu);
	else
		tegra_dvfs_rail_enable(&tegra11_dvfs_rail_vdd_cpu);

	return 0;
}

int tegra_dvfs_disable_get(char *buffer, const struct kernel_param *kp)
{
	return param_get_bool(buffer, kp);
}

static struct kernel_param_ops tegra_dvfs_disable_core_ops = {
	.set = tegra_dvfs_disable_core_set,
	.get = tegra_dvfs_disable_get,
};

static struct kernel_param_ops tegra_dvfs_disable_cpu_ops = {
	.set = tegra_dvfs_disable_cpu_set,
	.get = tegra_dvfs_disable_get,
};

module_param_cb(disable_core, &tegra_dvfs_disable_core_ops,
	&tegra_dvfs_core_disabled, 0644);
module_param_cb(disable_cpu, &tegra_dvfs_disable_cpu_ops,
	&tegra_dvfs_cpu_disabled, 0644);


static bool __init can_update_max_rate(struct clk *c, struct dvfs *d)
{
	/* Don't update manual dvfs clocks */
	if (!d->auto_dvfs)
		return false;

	/*
	 * Don't update EMC shared bus, since EMC dvfs is board dependent: max
	 * rate and EMC scaling frequencies are determined by tegra BCT (flashed
	 * together with the image) and board specific EMC DFS table; we will
	 * check the scaling ladder against nominal core voltage when the table
	 * is loaded (and if on particular board the table is not loaded, EMC
	 * scaling is disabled).
	 */
	if (c->ops->shared_bus_update && (c->flags & PERIPH_EMC_ENB))
		return false;

	/*
	 * Don't update shared cbus, and don't propagate common cbus dvfs
	 * limit down to shared users, but set maximum rate for each user
	 * equal to the respective client limit.
	 */
	if (c->ops->shared_bus_update && (c->flags & PERIPH_ON_CBUS)) {
		struct clk *user;
		unsigned long rate;

		list_for_each_entry(
			user, &c->shared_bus_list, u.shared_bus_user.node) {
			if (user->u.shared_bus_user.client) {
				rate = user->u.shared_bus_user.client->max_rate;
				user->max_rate = rate;
				user->u.shared_bus_user.rate = rate;
			}
		}
		return false;
	}

	/* Other, than EMC and cbus, auto-dvfs clocks can be updated */
	return true;
}

static void __init init_dvfs_one(struct dvfs *d, int max_freq_index)
{
	int ret;
	struct clk *c = tegra_get_clock_by_name(d->clk_name);

	if (!c) {
		pr_debug("tegra11_dvfs: no clock found for %s\n",
			d->clk_name);
		return;
	}

	/* Update max rate for auto-dvfs clocks, with shared bus exceptions */
	if (can_update_max_rate(c, d)) {
		BUG_ON(!d->freqs[max_freq_index]);
		tegra_init_max_rate(
			c, d->freqs[max_freq_index] * d->freqs_mult);
	}
	d->max_millivolts = d->dvfs_rail->nominal_millivolts;

	ret = tegra_enable_dvfs_on_clk(c, d);
	if (ret)
		pr_err("tegra11_dvfs: failed to enable dvfs on %s\n", c->name);
}

static bool __init match_dvfs_one(struct dvfs *d, int speedo_id, int process_id)
{
	if ((d->process_id != -1 && d->process_id != process_id) ||
		(d->speedo_id != -1 && d->speedo_id != speedo_id)) {
		pr_debug("tegra11_dvfs: rejected %s speedo %d,"
			" process %d\n", d->clk_name, d->speedo_id,
			d->process_id);
		return false;
	}
	return true;
}


/* cvb_mv = ((c2 * speedo / s_scale + c1) * speedo / s_scale + c0) / v_scale */
static inline int get_cvb_voltage(int speedo, int s_scale,
				  struct cpu_cvb_dvfs_parameters *cvb)
{
	/* apply only speedo scale: output mv = cvb_mv * v_scale */
	int mv;
	mv = DIV_ROUND_CLOSEST(cvb->c2 * speedo, s_scale);
	mv = DIV_ROUND_CLOSEST((mv + cvb->c1) * speedo, s_scale) + cvb->c0;
	return mv;
}

static inline int round_cvb_voltage(int mv, int v_scale)
{
	/* combined: apply voltage scale and round to cvb alignment step */
	int cvb_align_step_uv = tegra_get_cvb_alignment_uV();

	return DIV_ROUND_UP(mv * 1000, v_scale * cvb_align_step_uv) *
		cvb_align_step_uv / 1000;
}

static int __init set_cpu_dvfs_data(int speedo_id, struct dvfs *cpu_dvfs,
	struct tegra_cl_dvfs_dfll_data *dfll_data, int *max_freq_index)
{
	int i, j, mv, dfll_mv;
	unsigned long fmax_at_vmin = 0;
	struct cpu_cvb_dvfs *d = NULL;
	struct cpu_cvb_dvfs_parameters *cvb = NULL;
	int speedo = tegra_cpu_speedo_value();

	/* Find matching cvb dvfs entry */
	for (i = 0; i < ARRAY_SIZE(cpu_cvb_dvfs_table); i++) {
		d = &cpu_cvb_dvfs_table[i];
		if (speedo_id == d->speedo_id)
			break;
	}

	if (!d) {
		pr_err("tegra11_dvfs: no cpu dvfs table for speedo_id %d\n",
		       speedo_id);
		return -ENOENT;
	}
	BUG_ON(d->min_mv < tegra11_dvfs_rail_vdd_cpu.min_millivolts);

	/*
	 * Use CVB table to fill in CPU dvfs frequencies and voltages. Each
	 * CVB entry specifies CPU frequency and CVB coefficients to calculate
	 * the respective voltage when DFLL is used as CPU clock source. Common
	 * margin is applied to determine voltage requirements for PLL source.
	 */
	for (i = 0, j = 0; i < MAX_DVFS_FREQS; i++) {
		cvb = &d->cvb_table[i];
		if (!cvb->freq)
			break;

		mv = get_cvb_voltage(speedo, d->speedo_scale, cvb);
		dfll_mv = round_cvb_voltage(mv, d->voltage_scale);
		dfll_mv = max(dfll_mv, d->min_mv);
		if (dfll_mv > d->max_mv)
			break;

		/* Check maximum frequency at minimum voltage */
		if (dfll_mv > d->min_mv) {
			if (!j)
				break;	/* 1st entry already above Vmin */
			if (!fmax_at_vmin)
				fmax_at_vmin = cpu_dvfs->freqs[j - 1];
		}

		/* dvfs tables with maximum frequency at any distinct voltage */
		if (!j || (dfll_mv > cpu_dfll_millivolts[j - 1])) {
			cpu_dvfs->freqs[j] = cvb->freq;
			cpu_dfll_millivolts[j] = dfll_mv;
			mv = mv * d->margin / 100;
			mv = round_cvb_voltage(mv, d->voltage_scale);
			cpu_millivolts[j] = max(mv, d->min_mv);
			j++;
		} else {
			cpu_dvfs->freqs[j - 1] = cvb->freq;
		}

	}
	/* Table must not be empty and must have and at least one entry below,
	   and one entry above Vmin */
	if (!i || !j || !fmax_at_vmin) {
		pr_err("tegra11_dvfs: invalid cpu dvfs table for speedo_id %d\n",
		       speedo_id);
		return -ENOENT;
	}

	/* dvfs tables are successfully populated - fill in the rest */
	cpu_dvfs->speedo_id = speedo_id;
	cpu_dvfs->freqs_mult = d->freqs_mult;
	cpu_dvfs->dvfs_rail->nominal_millivolts =
		min(cpu_millivolts[j - 1], d->max_mv);
	*max_freq_index = j - 1;

	dfll_data->out_rate_min = fmax_at_vmin * d->freqs_mult;
	dfll_data->millivolts_min = d->min_mv;
	return 0;
}

static int __init get_core_nominal_mv_index(int speedo_id)
{
	int i;
	int mv = tegra_core_speedo_mv();
	int core_edp_limit = get_core_edp();

	/*
	 * Start with nominal level for the chips with this speedo_id. Then,
	 * make sure core nominal voltage is below edp limit for the board
	 * (if edp limit is set).
	 */
	if (core_edp_limit)
		mv = min(mv, core_edp_limit);

	/* Round nominal level down to the nearest core scaling step */
	for (i = 0; i < MAX_DVFS_FREQS; i++) {
		if ((core_millivolts[i] == 0) || (mv < core_millivolts[i]))
			break;
	}

	if (i == 0) {
		pr_err("tegra11_dvfs: unable to adjust core dvfs table to"
		       " nominal voltage %d\n", mv);
		return -ENOSYS;
	}
	return i - 1;
}

void __init tegra11x_init_dvfs(void)
{
	int cpu_speedo_id = tegra_cpu_speedo_id();
	int soc_speedo_id = tegra_soc_speedo_id();
	int core_process_id = tegra_core_process_id();

	int i;
	int core_nominal_mv_index;
	int cpu_max_freq_index;

#ifndef CONFIG_TEGRA_CORE_DVFS
	tegra_dvfs_core_disabled = true;
#endif
#ifndef CONFIG_TEGRA_CPU_DVFS
	tegra_dvfs_cpu_disabled = true;
#endif

	/*
	 * Find nominal voltages for core (1st) and cpu rails before rail
	 * init. Nominal voltage index in core scaling ladder can also be
	 * used to determine max dvfs frequencies for all core clocks. In
	 * case of error disable core scaling and set index to 0, so that
	 * core clocks would not exceed rates allowed at minimum voltage.
	 */
	core_nominal_mv_index = get_core_nominal_mv_index(soc_speedo_id);
	if (core_nominal_mv_index < 0) {
		tegra11_dvfs_rail_vdd_core.disabled = true;
		tegra_dvfs_core_disabled = true;
		core_nominal_mv_index = 0;
	}
	tegra11_dvfs_rail_vdd_core.nominal_millivolts =
		core_millivolts[core_nominal_mv_index];

	/*
	 * Setup cpu dvfs and dfll tables from cvb data, determine nominal
	 * voltage for cpu rail, and cpu maximum frequency. Note that entire
	 * frequency range is guaranteed only when dfll is used as cpu clock
	 * source. Reaching maximum frequency with pll as cpu clock source
	 * may not be possible within nominal voltage range (dvfs mechanism
	 * would automatically fail frequency request in this case, so that
	 * voltage limit is not violated). Error when cpu dvfs table can not
	 * be constructed must never happen.
	 */
	if (set_cpu_dvfs_data(cpu_speedo_id, &cpu_dvfs,
			      &cpu_dfll_data, &cpu_max_freq_index))
		BUG();

	/* Init rail structures and dependencies */
	tegra_dvfs_init_rails(tegra11_dvfs_rails,
		ARRAY_SIZE(tegra11_dvfs_rails));

	/* Search core dvfs table for speedo/process matching entries and
	   initialize dvfs-ed clocks */
	for (i = 0; i <  ARRAY_SIZE(core_dvfs_table); i++) {
		struct dvfs *d = &core_dvfs_table[i];
		if (!match_dvfs_one(d, soc_speedo_id, core_process_id))
			continue;
		init_dvfs_one(d, core_nominal_mv_index);
	}

	/* Initialize matching cpu dvfs entry already found when nominal
	   voltage was determined */
	init_dvfs_one(&cpu_dvfs, cpu_max_freq_index);

	/* CL DVFS characterization data */
	tegra_cl_dvfs_set_dfll_data(&cpu_dfll_data);

	/* Finally disable dvfs on rails if necessary */
	if (tegra_dvfs_core_disabled)
		tegra_dvfs_rail_disable(&tegra11_dvfs_rail_vdd_core);
	if (tegra_dvfs_cpu_disabled)
		tegra_dvfs_rail_disable(&tegra11_dvfs_rail_vdd_cpu);

	pr_info("tegra dvfs: VDD_CPU nominal %dmV, scaling %s\n",
		tegra11_dvfs_rail_vdd_cpu.nominal_millivolts,
		tegra_dvfs_cpu_disabled ? "disabled" : "enabled");
	pr_info("tegra dvfs: VDD_CORE nominal %dmV, scaling %s\n",
		tegra11_dvfs_rail_vdd_core.nominal_millivolts,
		tegra_dvfs_core_disabled ? "disabled" : "enabled");
}

int tegra_dvfs_rail_disable_prepare(struct dvfs_rail *rail)
{
	return 0;
}

int tegra_dvfs_rail_post_enable(struct dvfs_rail *rail)
{
	return 0;
}

/*
 * sysfs and dvfs interfaces to cap tegra core domains frequencies
 */
static DEFINE_MUTEX(core_cap_lock);

struct core_cap {
	int refcnt;
	int level;
};
static struct core_cap core_buses_cap;
static struct core_cap kdvfs_core_cap;
static struct core_cap user_core_cap;

static struct kobject *cap_kobj;

/* Arranged in order required for enabling/lowering the cap */
static struct {
	const char *cap_name;
	struct clk *cap_clk;
	unsigned long freqs[MAX_DVFS_FREQS];
} core_cap_table[] = {
#ifdef CONFIG_TEGRA_DUAL_CBUS
	{ .cap_name = "cap.c2bus" },
	{ .cap_name = "cap.c3bus" },
#else
	{ .cap_name = "cap.cbus" },
#endif
	{ .cap_name = "cap.sclk" },
	{ .cap_name = "cap.emc" },
};


static void core_cap_level_set(int level)
{
	int i, j;

	for (j = 0; j < ARRAY_SIZE(core_millivolts); j++) {
		int v = core_millivolts[j];
		if ((v == 0) || (level < v))
			break;
	}
	j = (j == 0) ? 0 : j - 1;
	level = core_millivolts[j];

	if (level < core_buses_cap.level) {
		for (i = 0; i < ARRAY_SIZE(core_cap_table); i++)
			if (core_cap_table[i].cap_clk)
				clk_set_rate(core_cap_table[i].cap_clk,
					     core_cap_table[i].freqs[j]);
	} else if (level > core_buses_cap.level) {
		for (i = ARRAY_SIZE(core_cap_table) - 1; i >= 0; i--)
			if (core_cap_table[i].cap_clk)
				clk_set_rate(core_cap_table[i].cap_clk,
					     core_cap_table[i].freqs[j]);
	}
	core_buses_cap.level = level;
}

static void core_cap_update(void)
{
	int new_level = tegra11_dvfs_rail_vdd_core.max_millivolts;

	if (kdvfs_core_cap.refcnt)
		new_level = min(new_level, kdvfs_core_cap.level);
	if (user_core_cap.refcnt)
		new_level = min(new_level, user_core_cap.level);

	if (core_buses_cap.level != new_level)
		core_cap_level_set(new_level);
}

static void core_cap_enable(bool enable)
{
	if (enable)
		core_buses_cap.refcnt++;
	else if (core_buses_cap.refcnt)
		core_buses_cap.refcnt--;

	core_cap_update();
}

static ssize_t
core_cap_state_show(struct kobject *kobj, struct kobj_attribute *attr,
		    char *buf)
{
	return sprintf(buf, "%d (%d)\n", core_buses_cap.refcnt ? 1 : 0,
			user_core_cap.refcnt ? 1 : 0);
}
static ssize_t
core_cap_state_store(struct kobject *kobj, struct kobj_attribute *attr,
		     const char *buf, size_t count)
{
	int state;

	if (sscanf(buf, "%d", &state) != 1)
		return -1;

	mutex_lock(&core_cap_lock);

	if (state) {
		user_core_cap.refcnt++;
		if (user_core_cap.refcnt == 1)
			core_cap_enable(true);
	} else if (user_core_cap.refcnt) {
		user_core_cap.refcnt--;
		if (user_core_cap.refcnt == 0)
			core_cap_enable(false);
	}

	mutex_unlock(&core_cap_lock);
	return count;
}

static ssize_t
core_cap_level_show(struct kobject *kobj, struct kobj_attribute *attr,
		    char *buf)
{
	return sprintf(buf, "%d (%d)\n", core_buses_cap.level,
			user_core_cap.level);
}
static ssize_t
core_cap_level_store(struct kobject *kobj, struct kobj_attribute *attr,
		     const char *buf, size_t count)
{
	int level;

	if (sscanf(buf, "%d", &level) != 1)
		return -1;

	mutex_lock(&core_cap_lock);
	user_core_cap.level = level;
	core_cap_update();
	mutex_unlock(&core_cap_lock);
	return count;
}

static struct kobj_attribute cap_state_attribute =
	__ATTR(core_cap_state, 0644, core_cap_state_show, core_cap_state_store);
static struct kobj_attribute cap_level_attribute =
	__ATTR(core_cap_level, 0644, core_cap_level_show, core_cap_level_store);

const struct attribute *cap_attributes[] = {
	&cap_state_attribute.attr,
	&cap_level_attribute.attr,
	NULL,
};

void tegra_dvfs_core_cap_enable(bool enable)
{
	mutex_lock(&core_cap_lock);

	if (enable) {
		kdvfs_core_cap.refcnt++;
		if (kdvfs_core_cap.refcnt == 1)
			core_cap_enable(true);
	} else if (kdvfs_core_cap.refcnt) {
		kdvfs_core_cap.refcnt--;
		if (kdvfs_core_cap.refcnt == 0)
			core_cap_enable(false);
	}
	mutex_unlock(&core_cap_lock);
}

void tegra_dvfs_core_cap_level_set(int level)
{
	mutex_lock(&core_cap_lock);
	kdvfs_core_cap.level = level;
	core_cap_update();
	mutex_unlock(&core_cap_lock);
}

static int __init init_core_cap_one(struct clk *c, unsigned long *freqs)
{
	int i, v, next_v = 0;
	unsigned long rate, next_rate = 0;

	for (i = 0; i < ARRAY_SIZE(core_millivolts); i++) {
		v = core_millivolts[i];
		if (v == 0)
			break;

		for (;;) {
			rate = next_rate;
			next_rate = clk_round_rate(c->parent, rate + 1000);
			if (IS_ERR_VALUE(next_rate)) {
				pr_debug("tegra11_dvfs: failed to round %s rate %lu\n",
					 c->name, rate);
				return -EINVAL;
			}
			if (rate == next_rate)
				break;

			next_v = tegra_dvfs_predict_millivolts(
				c->parent, next_rate);
			if (IS_ERR_VALUE(next_v)) {
				pr_debug("tegra11_dvfs: failed to predict %s mV for rate %lu\n",
					 c->name, next_rate);
				return -EINVAL;
			}
			if (next_v > v)
				break;
		}

		if (rate == 0) {
			rate = next_rate;
			pr_warn("tegra11_dvfs: minimum %s rate %lu requires %d mV\n",
				c->name, rate, next_v);
		}
		freqs[i] = rate;
		next_rate = rate;
	}
	return 0;
}

static int __init tegra_dvfs_init_core_cap(void)
{
	int i;
	struct clk *c = NULL;

	core_buses_cap.level = kdvfs_core_cap.level = user_core_cap.level =
		tegra11_dvfs_rail_vdd_core.max_millivolts;

	for (i = 0; i < ARRAY_SIZE(core_cap_table); i++) {
		c = tegra_get_clock_by_name(core_cap_table[i].cap_name);
		if (!c || !c->parent ||
		    init_core_cap_one(c, core_cap_table[i].freqs)) {
			pr_err("tegra11_dvfs: failed to initialize %s table\n",
			       core_cap_table[i].cap_name);
			continue;
		}
		core_cap_table[i].cap_clk = c;
	}

	cap_kobj = kobject_create_and_add("tegra_cap", kernel_kobj);
	if (!cap_kobj) {
		pr_err("tegra11_dvfs: failed to create sysfs cap object\n");
		return 0;
	}

	if (sysfs_create_files(cap_kobj, cap_attributes)) {
		pr_err("tegra11_dvfs: failed to create sysfs cap interface\n");
		return 0;
	}
	pr_info("tegra dvfs: tegra sysfs cap interface is initialized\n");

	return 0;
}
late_initcall(tegra_dvfs_init_core_cap);
