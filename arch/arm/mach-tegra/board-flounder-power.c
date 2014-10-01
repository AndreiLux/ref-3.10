/*
 * arch/arm/mach-tegra/board-flounder-power.c
 *
 * Copyright (c) 2013-2014, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/io.h>
#include <mach/edp.h>
#include <mach/irqs.h>
#include <linux/edp.h>
#include <linux/platform_data/tegra_edp.h>
#include <linux/pid_thermal_gov.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/palmas.h>
#include <linux/power/power_supply_extcon.h>
#include <linux/regulator/machine.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/regulator/tegra-dfll-bypass-regulator.h>
#include <linux/tegra-fuse.h>

#include <asm/mach-types.h>
#include <mach/pinmux-t12.h>

#include "pm.h"
#include "dvfs.h"
#include "board.h"
#include "tegra-board-id.h"
#include "board-common.h"
#include "board-flounder.h"
#include "board-pmu-defines.h"
#include "devices.h"
#include "iomap.h"
#include "tegra_cl_dvfs.h"
#include "tegra11_soctherm.h"

#define PMC_CTRL                0x0
#define PMC_CTRL_INTR_LOW       (1 << 17)

static void flounder_board_suspend(int state, enum suspend_stage stage)
{
	static int request = 0;

	if (state == TEGRA_SUSPEND_LP0 && stage == TEGRA_SUSPEND_BEFORE_PERIPHERAL) {
		if (!request)
			gpio_request_one(TEGRA_GPIO_PB5, GPIOF_OUT_INIT_HIGH,
				"sdmmc3_dat2");
		else
			gpio_direction_output(TEGRA_GPIO_PB5,1);

		request = 1;
	}
}

static struct tegra_suspend_platform_data flounder_suspend_data = {
	.cpu_timer      = 500,
	.cpu_off_timer  = 300,
	.suspend_mode   = TEGRA_SUSPEND_LP0,
	.core_timer     = 0x157e,
	.core_off_timer = 2000,
	.corereq_high   = true,
	.sysclkreq_high = true,
	.cpu_lp2_min_residency = 1000,
	.min_residency_vmin_fmin = 1000,
	.min_residency_ncpu_fast = 8000,
	.min_residency_ncpu_slow = 5000,
	.min_residency_mclk_stop = 5000,
	.min_residency_crail = 20000,
	.board_suspend = flounder_board_suspend,
};

static struct power_supply_extcon_plat_data extcon_pdata = {
	.extcon_name = "tegra-udc",
};

static struct platform_device power_supply_extcon_device = {
	.name	= "power-supply-extcon",
	.id	= -1,
	.dev	= {
		.platform_data = &extcon_pdata,
	},
};

int __init flounder_suspend_init(void)
{
	tegra_init_suspend(&flounder_suspend_data);
	return 0;
}

/************************ FLOUNDER CL-DVFS DATA *********************/
#define FLOUNDER_DEFAULT_CVB_ALIGNMENT	10000

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
static struct tegra_cl_dvfs_cfg_param e1736_flounder_cl_dvfs_param = {
	.sample_rate = 12500, /* i2c freq */

	.force_mode = TEGRA_CL_DVFS_FORCE_FIXED,
	.cf = 10,
	.ci = 0,
	.cg = 2,

	.droop_cut_value = 0xF,
	.droop_restore_ramp = 0x0,
	.scale_out_ramp = 0x0,
};

/* E1736 volatge map. Fixed 10mv steps from 700mv to 1400mv */
#define E1736_CPU_VDD_MAP_SIZE ((1400000 - 700000) / 10000 + 1)
static struct voltage_reg_map e1736_cpu_vdd_map[E1736_CPU_VDD_MAP_SIZE];
static inline void e1736_fill_reg_map(void)
{
	int i;
	for (i = 0; i < E1736_CPU_VDD_MAP_SIZE; i++) {
		/* 0.7V corresponds to 0b0011010 = 26 */
		/* 1.4V corresponds to 0b1100000 = 96 */
		e1736_cpu_vdd_map[i].reg_value = i + 26;
		e1736_cpu_vdd_map[i].reg_uV = 700000 + 10000 * i;
	}
}

static struct tegra_cl_dvfs_platform_data e1736_cl_dvfs_data = {
	.dfll_clk_name = "dfll_cpu",
	.pmu_if = TEGRA_CL_DVFS_PMU_I2C,
	.u.pmu_i2c = {
		.fs_rate = 400000,
		.slave_addr = 0xb0, /* pmu i2c address */
		.reg = 0x23,        /* vdd_cpu rail reg address */
	},
	.vdd_map = e1736_cpu_vdd_map,
	.vdd_map_size = E1736_CPU_VDD_MAP_SIZE,

	.cfg_param = &e1736_flounder_cl_dvfs_param,
};
static int __init flounder_cl_dvfs_init(void)
{
	struct tegra_cl_dvfs_platform_data *data = NULL;

	e1736_fill_reg_map();
	data = &e1736_cl_dvfs_data;

	if (data) {
		data->flags = TEGRA_CL_DVFS_DYN_OUTPUT_CFG;
		tegra_cl_dvfs_device.dev.platform_data = data;
		platform_device_register(&tegra_cl_dvfs_device);
	}
	return 0;
}
#else
static inline int flounder_cl_dvfs_init(void)
{ return 0; }
#endif

int __init flounder_rail_alignment_init(void)
{
#ifdef CONFIG_ARCH_TEGRA_13x_SOC
#else
	tegra12x_vdd_cpu_align(FLOUNDER_DEFAULT_CVB_ALIGNMENT, 0);
#endif
	return 0;
}

int __init flounder_regulator_init(void)
{
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 pmc_ctrl;

	/* TPS65913: Normal state of INT request line is LOW.
	 * configure the power management controller to trigger PMU
	 * interrupts when HIGH.
	 */
	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);

	platform_device_register(&power_supply_extcon_device);

	return 0;
}

int __init flounder_edp_init(void)
{
	unsigned int regulator_mA;

	/* Both vdd_cpu and vdd_gpu uses 3-phase rail, so EDP current
	 * limit will be the same.
	 * */
	regulator_mA = 16800;

	pr_info("%s: CPU regulator %d mA\n", __func__, regulator_mA);
	tegra_init_cpu_edp_limits(regulator_mA);

	pr_info("%s: GPU regulator %d mA\n", __func__, regulator_mA);
	tegra_init_gpu_edp_limits(regulator_mA);

	return 0;
}


static struct pid_thermal_gov_params soctherm_pid_params = {
	.max_err_temp = 9000,
	.max_err_gain = 1000,

	.gain_p = 1000,
	.gain_d = 0,

	.up_compensation = 20,
	.down_compensation = 20,
};

static struct thermal_zone_params soctherm_tzp = {
	.governor_name = "pid_thermal_gov",
	.governor_params = &soctherm_pid_params,
};

static struct tegra_thermtrip_pmic_data tpdata_palmas = {
	.reset_tegra = 1,
	.pmu_16bit_ops = 0,
	.controller_type = 0,
	.pmu_i2c_addr = 0x58,
	.i2c_controller_id = 4,
	.poweroff_reg_addr = 0xa0,
	.poweroff_reg_data = 0x0,
};

/*
 * @PSKIP_CONFIG_NOTE: For T132, throttling config of PSKIP is no longer
 * done in soctherm registers. These settings are now done via registers in
 * denver:ccroc module which are at a different register offset. More
 * importantly, there are _only_ three levels of throttling: 'low',
 * 'medium' and 'heavy' and are selected via the 'throttling_depth' field
 * in the throttle->devs[] section of the soctherm config. Since the depth
 * specification is per device, it is necessary to manually make sure the
 * depths specified alongwith a given level are the same across all devs,
 * otherwise it will overwrite a previously set depth with a different
 * depth. We will refer to this comment at each relevant location in the
 * config sections below.
 */
static struct soctherm_platform_data flounder_soctherm_data = {
	.oc_irq_base = TEGRA_SOC_OC_IRQ_BASE,
	.num_oc_irqs = TEGRA_SOC_OC_NUM_IRQ,
	.therm = {
		[THERM_CPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 6000,
			.num_trips = 3,
			.trips = {
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 101000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 99000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "cpu-balanced",
					.trip_temp = 90000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
			.tzp = &soctherm_tzp,
		},
		[THERM_GPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 6000,
			.num_trips = 3,
			.trips = {
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 101000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 99000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "gpu-balanced",
					.trip_temp = 90000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
			.tzp = &soctherm_tzp,
		},
		[THERM_MEM] = {
			.zone_enable = true,
			.num_trips = 1,
			.trips = {
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 101000, /* = GPU shut */
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
			.tzp = &soctherm_tzp,
		},
		[THERM_PLL] = {
			.zone_enable = true,
			.tzp = &soctherm_tzp,
		},
	},
	.throttle = {
		[THROTTLE_HEAVY] = {
			.priority = 100,
			.devs = {
				[THROTTLE_DEV_CPU] = {
					.enable = true,
					.depth = 80,
					/* see @PSKIP_CONFIG_NOTE */
					.throttling_depth = "heavy_throttling",
				},
				[THROTTLE_DEV_GPU] = {
					.enable = true,
					.throttling_depth = "heavy_throttling",
				},
			},
		},
	},
};

/* Only the diffs from flounder_soctherm_data structure */
static struct soctherm_platform_data t132ref_v1_soctherm_data = {
	.therm = {
		[THERM_CPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 10000,
		},
		[THERM_PLL] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.num_trips = 3,
			.trips = {
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 97000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 94000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "cpu-balanced",
					.trip_temp = 84000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
			.tzp = &soctherm_tzp,
		},
	},
};

/* Only the diffs from ardbeg_soctherm_data structure */
static struct soctherm_platform_data t132ref_v2_soctherm_data = {
	.therm = {
		[THERM_CPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 10000,
			.num_trips = 3,
			.trips = {
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 105000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 102000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "cpu-balanced",
					.trip_temp = 85000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
			.tzp = &soctherm_tzp,
		},
		[THERM_GPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 5000,
			.num_trips = 3,
			.trips = {
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 101000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 99000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "gpu-balanced",
					.trip_temp = 89000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
			.tzp = &soctherm_tzp,
		},
	},
};

static struct soctherm_throttle battery_oc_throttle_t13x = {
	.throt_mode = BRIEF,
	.polarity = SOCTHERM_ACTIVE_LOW,
	.priority = 50,
	.intr = true,
	.alarm_cnt_threshold = 15,
	.alarm_filter = 5100000,
	.devs = {
		[THROTTLE_DEV_CPU] = {
			.enable = true,
			.depth = 50,
			/* see @PSKIP_CONFIG_NOTE */
			.throttling_depth = "low_throttling",
		},
		[THROTTLE_DEV_GPU] = {
			.enable = true,
			.throttling_depth = "medium_throttling",
		},
	},
};

int __init flounder_soctherm_init(void)
{
	const int t13x_cpu_edp_temp_margin = 5000,
		t13x_gpu_edp_temp_margin = 6000;
	int cpu_edp_temp_margin, gpu_edp_temp_margin;
	int cp_rev, ft_rev;
	enum soctherm_therm_id therm_cpu = THERM_CPU;

	cp_rev = tegra_fuse_calib_base_get_cp(NULL, NULL);
	ft_rev = tegra_fuse_calib_base_get_ft(NULL, NULL);

	/* TODO: remove this part once bootloader changes merged */
	tegra_gpio_disable(TEGRA_GPIO_PJ2);
	tegra_gpio_disable(TEGRA_GPIO_PS7);

	cpu_edp_temp_margin = t13x_cpu_edp_temp_margin;
	gpu_edp_temp_margin = t13x_gpu_edp_temp_margin;

	if (!cp_rev) {
		/* ATE rev is NEW: use v2 table */
		flounder_soctherm_data.therm[THERM_CPU] =
			t132ref_v2_soctherm_data.therm[THERM_CPU];
		flounder_soctherm_data.therm[THERM_GPU] =
			t132ref_v2_soctherm_data.therm[THERM_GPU];
	} else {
		/* ATE rev is Old or Mid: use PLLx sensor only */
		flounder_soctherm_data.therm[THERM_CPU] =
			t132ref_v1_soctherm_data.therm[THERM_CPU];
		flounder_soctherm_data.therm[THERM_PLL] =
			t132ref_v1_soctherm_data.therm[THERM_PLL];
		therm_cpu = THERM_PLL; /* override CPU with PLL zone */
	}

	/* do this only for supported CP,FT fuses */
	if ((cp_rev >= 0) && (ft_rev >= 0)) {
		tegra_platform_edp_init(
			flounder_soctherm_data.therm[therm_cpu].trips,
			&flounder_soctherm_data.therm[therm_cpu].num_trips,
			t13x_cpu_edp_temp_margin);
		tegra_platform_gpu_edp_init(
			flounder_soctherm_data.therm[THERM_GPU].trips,
			&flounder_soctherm_data.therm[THERM_GPU].num_trips,
			t13x_gpu_edp_temp_margin);
		tegra_add_cpu_vmax_trips(
			flounder_soctherm_data.therm[therm_cpu].trips,
			&flounder_soctherm_data.therm[therm_cpu].num_trips);
		tegra_add_tgpu_trips(
			flounder_soctherm_data.therm[THERM_GPU].trips,
			&flounder_soctherm_data.therm[THERM_GPU].num_trips);
		tegra_add_core_vmax_trips(
			flounder_soctherm_data.therm[THERM_PLL].trips,
			&flounder_soctherm_data.therm[THERM_PLL].num_trips);
	}

	tegra_add_cpu_vmin_trips(
		flounder_soctherm_data.therm[therm_cpu].trips,
		&flounder_soctherm_data.therm[therm_cpu].num_trips);
	tegra_add_gpu_vmin_trips(
		flounder_soctherm_data.therm[THERM_GPU].trips,
		&flounder_soctherm_data.therm[THERM_GPU].num_trips);
	tegra_add_core_vmin_trips(
		flounder_soctherm_data.therm[THERM_PLL].trips,
		&flounder_soctherm_data.therm[THERM_PLL].num_trips);

	flounder_soctherm_data.tshut_pmu_trip_data = &tpdata_palmas;
	/* Enable soc_therm OC throttling on selected platforms */
	memcpy(&flounder_soctherm_data.throttle[THROTTLE_OC4],
		       &battery_oc_throttle_t13x,
		       sizeof(battery_oc_throttle_t13x));
	return tegra11_soctherm_init(&flounder_soctherm_data);
}
