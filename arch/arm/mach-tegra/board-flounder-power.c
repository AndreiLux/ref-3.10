/*
 * arch/arm/mach-tegra/board-flounder-power.c
 *
 * Copyright (c) 2013 NVIDIA Corporation. All rights reserved.
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
#include <linux/generic_adc_thermal.h>
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

/* -40 to 125 degC */
static int flounder_batt_temperature_table[] = {
	259, 266, 272, 279, 286, 293, 301, 308,
	316, 324, 332, 340, 349, 358, 367, 376,
	386, 395, 405, 416, 426, 437, 448, 459,
	471, 483, 495, 508, 520, 533, 547, 561,
	575, 589, 604, 619, 634, 650, 666, 682,
	699, 716, 733, 751, 769, 787, 806, 825,
	845, 865, 885, 905, 926, 947, 969, 990,
	1013, 1035, 1058, 1081, 1104, 1127, 1151, 1175,
	1199, 1224, 1249, 1273, 1298, 1324, 1349, 1374,
	1400, 1426, 1451, 1477, 1503, 1529, 1554, 1580,
	1606, 1631, 1657, 1682, 1707, 1732, 1757, 1782,
	1807, 1831, 1855, 1878, 1902, 1925, 1948, 1970,
	1992, 2014, 2036, 2057, 2077, 2097, 2117, 2136,
	2155, 2174, 2192, 2209, 2227, 2243, 2259, 2275,
	2291, 2305, 2320, 2334, 2347, 2361, 2373, 2386,
	2397, 2409, 2420, 2430, 2441, 2450, 2460, 2469,
	2478, 2486, 2494, 2502, 2509, 2516, 2523, 2529,
	2535, 2541, 2547, 2552, 2557, 2562, 2567, 2571,
	2575, 2579, 2583, 2587, 2590, 2593, 2596, 2599,
	2602, 2605, 2607, 2609, 2611, 2614, 2615, 2617,
	2619, 2621, 2622, 2624, 2625, 2626,
};

static struct gadc_thermal_platform_data gadc_thermal_battery_pdata = {
	.iio_channel_name = "battery-temp-channel",
	.tz_name = "battery-temp",
	.temp_offset = 0,
	.adc_to_temp = NULL,
	.adc_temp_lookup = flounder_batt_temperature_table,
	.lookup_table_size = ARRAY_SIZE(flounder_batt_temperature_table),
	.first_index_temp = 125,
	.last_index_temp = -40,
};

static struct platform_device gadc_thermal_battery = {
	.name   = "generic-adc-thermal",
	.id     = 0,
	.dev    = {
		.platform_data = &gadc_thermal_battery_pdata,
	},
};

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

	platform_device_register(&gadc_thermal_battery);
	platform_device_register(&power_supply_extcon_device);

	flounder_cl_dvfs_init();
	return 0;
}

int __init flounder_edp_init(void)
{
	unsigned int regulator_mA;

	regulator_mA = get_maximum_cpu_current_supported();
	if (!regulator_mA)
		regulator_mA = 16000;

	pr_info("%s: CPU regulator %d mA\n", __func__, regulator_mA);
	tegra_init_cpu_edp_limits(regulator_mA);

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

/* This is really v2 rev of the flounder_soctherm_data structure */
static struct soctherm_platform_data flounder_soctherm_data = {
	.oc_irq_base = TEGRA_SOC_OC_IRQ_BASE,
	.num_oc_irqs = TEGRA_SOC_OC_NUM_IRQ,
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
					.trip_temp = 92000,
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
			.num_trips = 1,
			.trips = {
				{
					.cdev_type = "tegra-dram",
					.trip_temp = 78000,
					.trip_type = THERMAL_TRIP_ACTIVE,
					.upper = 1,
					.lower = 1,
				},
			},
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
					.throttling_depth = "heavy_throttling",
				},
				[THROTTLE_DEV_GPU] = {
					.enable = true,
					.throttling_depth = "heavy_throttling",
				},
			},
		},
	},
	.tshut_pmu_trip_data = &tpdata_palmas,
};

/* Only the diffs from flounder_soctherm_data structure */
static struct soctherm_platform_data flounder_v1_soctherm_data = {
	.therm = {
		[THERM_CPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 10000,
		},
		[THERM_PLL] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.num_trips = 4,
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
				{
				.cdev_type = "tegra-dram",
				.trip_temp = 78000,
				.trip_type = THERMAL_TRIP_ACTIVE,
				.upper = 1,
				.lower = 1,
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
	int cp_rev, ft_rev;
	struct board_info pmu_board_info;
	enum soctherm_therm_id therm_cpu = THERM_CPU;

	cp_rev = tegra_fuse_calib_base_get_cp(NULL, NULL);
	ft_rev = tegra_fuse_calib_base_get_ft(NULL, NULL);

	/* TODO: remove this line once hboot changes merged */
	tegra_gpio_disable(TEGRA_GPIO_PJ2);

	if (cp_rev) {
		/* ATE rev is Old or Mid - use PLLx sensor only */
		flounder_soctherm_data.therm[THERM_CPU] =
			flounder_v1_soctherm_data.therm[THERM_CPU];
		flounder_soctherm_data.therm[THERM_PLL] =
			flounder_v1_soctherm_data.therm[THERM_PLL];
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

	tegra_get_pmu_board_info(&pmu_board_info);
	/* Enable soc_therm OC throttling on selected platforms */
	memcpy(&flounder_soctherm_data.throttle[THROTTLE_OC4],
		       &battery_oc_throttle_t13x,
		       sizeof(battery_oc_throttle_t13x));
	return tegra11_soctherm_init(&flounder_soctherm_data);
}
