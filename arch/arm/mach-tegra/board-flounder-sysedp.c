/*
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

#include <linux/sysedp.h>
#include <linux/platform_device.h>
#include <linux/platform_data/tegra_edp.h>
#include <linux/power_supply.h>
#include <mach/edp.h>
#include <linux/interrupt.h>
#include "board-flounder.h"
#include "board.h"
#include "board-panel.h"
#include "common.h"
#include "tegra11_soctherm.h"

/* --- EDP consumers data --- */
static unsigned int imx219_states[] = { 0, 411 };
static unsigned int ov9760_states[] = { 0, 180 };
static unsigned int sdhci_states[] = { 0, 966 };
static unsigned int speaker_states[] = { 0, 641 };
static unsigned int wifi_states[] = { 0, 2318 };

/* default - 19x12 8" panel*/
static unsigned int backlight_default_states[] = {
	0, 110, 270, 430, 595, 765, 1055, 1340, 1655, 1970, 2380
};

static unsigned int tps61310_states[] = {
	0, 110 ,3350
};

static struct sysedp_consumer_data flounder_sysedp_consumer_data[] = {
	SYSEDP_CONSUMER_DATA("imx219", imx219_states),
	SYSEDP_CONSUMER_DATA("ov9760", ov9760_states),
	SYSEDP_CONSUMER_DATA("speaker", speaker_states),
	SYSEDP_CONSUMER_DATA("wifi", wifi_states),
	SYSEDP_CONSUMER_DATA("tegra-dsi-backlight.0", backlight_default_states),
	SYSEDP_CONSUMER_DATA("sdhci-tegra.0", sdhci_states),
	SYSEDP_CONSUMER_DATA("sdhci-tegra.3", sdhci_states),
	SYSEDP_CONSUMER_DATA("tps61310", tps61310_states),
};

static struct sysedp_platform_data flounder_sysedp_platform_data = {
	.consumer_data = flounder_sysedp_consumer_data,
	.consumer_data_size = ARRAY_SIZE(flounder_sysedp_consumer_data),
	.margin = 0,
	.min_budget = 4400,
};

static struct platform_device flounder_sysedp_device = {
	.name = "sysedp",
	.id = -1,
	.dev = { .platform_data = &flounder_sysedp_platform_data }
};

void __init flounder_new_sysedp_init(void)
{
	int r;

	r = platform_device_register(&flounder_sysedp_device);
	WARN_ON(r);
}

static struct tegra_sysedp_platform_data flounder_sysedp_dynamic_capping_platdata = {
	.core_gain = 100,
	.init_req_watts = 20000,
	.pthrot_ratio = 75,
	.cap_method = TEGRA_SYSEDP_CAP_METHOD_DIRECT,
};

static struct platform_device flounder_sysedp_dynamic_capping = {
	.name = "sysedp_dynamic_capping",
	.id = -1,
	.dev = { .platform_data = &flounder_sysedp_dynamic_capping_platdata }
};

struct sysedp_reactive_capping_platform_data flounder_battery_oc4_platdata = {
	.max_capping_mw = 15000,
	.step_alarm_mw = 1000,
	.step_relax_mw = 500,
	.relax_ms = 250,
	.sysedpc = {
		.name = "battery_oc4"
	},
	.irq = TEGRA_SOC_OC_IRQ_BASE + TEGRA_SOC_OC_IRQ_4,
	.irq_flags = IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
};

static struct platform_device flounder_sysedp_reactive_capping_oc4 = {
	.name = "sysedp_reactive_capping",
	.id = 1,
	.dev = { .platform_data = &flounder_battery_oc4_platdata }
};


void __init flounder_sysedp_dynamic_capping_init(void)
{
	int r;
	struct tegra_sysedp_corecap *corecap;
	unsigned int corecap_size;

	corecap = tegra_get_sysedp_corecap(&corecap_size);
	if (!corecap) {
		WARN_ON(1);
		return;
	}
	flounder_sysedp_dynamic_capping_platdata.corecap = corecap;
	flounder_sysedp_dynamic_capping_platdata.corecap_size = corecap_size;

	flounder_sysedp_dynamic_capping_platdata.cpufreq_lim = tegra_get_system_edp_entries(
		&flounder_sysedp_dynamic_capping_platdata.cpufreq_lim_size);
	if (!flounder_sysedp_dynamic_capping_platdata.cpufreq_lim) {
		WARN_ON(1);
		return;
	}

	r = platform_device_register(&flounder_sysedp_dynamic_capping);
	WARN_ON(r);

	r = platform_device_register(&flounder_sysedp_reactive_capping_oc4);
	WARN_ON(r);
}
