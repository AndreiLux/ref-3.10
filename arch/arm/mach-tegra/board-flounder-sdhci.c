/*
 * arch/arm/mach-tegra/board-flounder-sdhci.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/resource.h>
#include <linux/platform_device.h>
#include <linux/wlan_plat.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/if.h>
#include <linux/mmc/host.h>
#include <linux/wl12xx.h>
#include <linux/platform_data/mmc-sdhci-tegra.h>
#include <linux/mfd/max77660/max77660-core.h>
#include <linux/tegra-fuse.h>
#include <linux/random.h>

#include <asm/mach-types.h>
#include <mach/irqs.h>
#include <mach/gpio-tegra.h>

#include "gpio-names.h"
#include "board.h"
#include "board-flounder.h"
#include "dvfs.h"
#include "iomap.h"
#include "tegra-board-id.h"

#define FLOUNDER_WLAN_PWR	TEGRA_GPIO_PX7
#define FLOUNDER_WLAN_WOW	TEGRA_GPIO_PU5
#if defined(CONFIG_BCMDHD_EDP_SUPPORT)
#define ON 1020 /* 1019.16mW */
#define OFF 0
static unsigned int wifi_states[] = {ON, OFF};
#endif

#define FUSE_SOC_SPEEDO_0	0x134
static void (*wifi_status_cb)(int card_present, void *dev_id);
static void *wifi_status_cb_devid;
static int flounder_wifi_status_register(void (*callback)(int , void *), void *);

static int flounder_wifi_reset(int on);
static int flounder_wifi_power(int on);
static int flounder_wifi_set_carddetect(int val);
static int flounder_wifi_get_mac_addr(unsigned char *buf);
static void* flounder_wifi_get_country_code(char *country_iso_code, u32 flags);

static struct wifi_platform_data flounder_wifi_control = {
	.set_power	= flounder_wifi_power,
	.set_reset	= flounder_wifi_reset,
	.set_carddetect	= flounder_wifi_set_carddetect,
	.get_mac_addr	= flounder_wifi_get_mac_addr,
	.get_country_code	= flounder_wifi_get_country_code,
#if defined (CONFIG_BCMDHD_EDP_SUPPORT)
	/* wifi edp client information */
	.client_info	= {
		.name		= "wifi_edp_client",
		.states		= wifi_states,
		.num_states	= ARRAY_SIZE(wifi_states),
		.e0_index	= 0,
		.priority	= EDP_MAX_PRIO,
	},
#endif
};

static struct resource wifi_resource[] = {
	[0] = {
		.name	= "bcm4329_wlan_irq",
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL
				| IORESOURCE_IRQ_SHAREABLE,
	},
};

static struct platform_device flounder_wifi_device = {
	.name		= "bcm4329_wlan",
	.id		= 1,
	.num_resources	= 1,
	.resource	= wifi_resource,
	.dev		= {
		.platform_data = &flounder_wifi_control,
	},
};

static struct resource mrvl_wifi_resource[] = {
	[0] = {
		.name   = "mrvl_wlan_irq",
		.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_LOWLEVEL | IORESOURCE_IRQ_SHAREABLE,
	},
};

static struct platform_device marvell_wifi_device = {
	.name           = "mrvl_wlan",
	.id             = 1,
	.num_resources  = 1,
	.resource       = mrvl_wifi_resource,
	.dev            = {
		.platform_data = &flounder_wifi_control,
	},
};

static struct resource sdhci_resource0[] = {
	[0] = {
		.start  = INT_SDMMC1,
		.end    = INT_SDMMC1,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC1_BASE,
		.end	= TEGRA_SDMMC1_BASE + TEGRA_SDMMC1_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource sdhci_resource3[] = {
	[0] = {
		.start  = INT_SDMMC4,
		.end    = INT_SDMMC4,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC4_BASE,
		.end	= TEGRA_SDMMC4_BASE + TEGRA_SDMMC4_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

#ifdef CONFIG_MMC_EMBEDDED_SDIO
static struct embedded_sdio_data embedded_sdio_data0 = {
	.cccr   = {
		.sdio_vsn       = 2,
		.multi_block    = 1,
		.low_speed      = 0,
		.wide_bus       = 0,
		.high_power     = 1,
		.high_speed     = 1,
	},
	.cis  = {
		.vendor	 = 0x02d0,
		.device	 = 0x4329,
	},
};
#endif

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data0 = {
	.mmc_data = {
		.register_status_notify	= flounder_wifi_status_register,
#ifdef CONFIG_MMC_EMBEDDED_SDIO
		.embedded_sdio = &embedded_sdio_data0,
#endif
		.built_in = 0,
		.ocr_mask = MMC_OCR_1V8_MASK,
	},
	.cd_gpio = -1,
	.wp_gpio = -1,
	.power_gpio = -1,
	.tap_delay = 0,
	.trim_delay = 0x2,
	.ddr_clk_limit = 41000000,
	.uhs_mask = MMC_UHS_MASK_DDR50,
	.calib_3v3_offsets = 0x7676,
	.calib_1v8_offsets = 0x7676,
	.default_drv_type = MMC_SET_DRIVER_TYPE_A,
};

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data3 = {
	.cd_gpio = -1,
	.wp_gpio = -1,
	.power_gpio = -1,
	.is_8bit = 1,
	.tap_delay = 0x4,
	.trim_delay = 0x3,
	.ddr_trim_delay = 0x0,
	.mmc_data = {
		.built_in = 1,
		.ocr_mask = MMC_OCR_1V8_MASK,
	},
	.ddr_clk_limit = 51000000,
	.max_clk_limit = 200000000,
	.calib_3v3_offsets = 0x0202,
	.calib_1v8_offsets = 0x0202,
};

static struct platform_device tegra_sdhci_device0 = {
	.name		= "sdhci-tegra",
	.id		= 0,
	.resource	= sdhci_resource0,
	.num_resources	= ARRAY_SIZE(sdhci_resource0),
	.dev = {
		.platform_data = &tegra_sdhci_platform_data0,
	},
};

static struct platform_device tegra_sdhci_device3 = {
	.name		= "sdhci-tegra",
	.id		= 3,
	.resource	= sdhci_resource3,
	.num_resources	= ARRAY_SIZE(sdhci_resource3),
	.dev = {
		.platform_data = &tegra_sdhci_platform_data3,
	},
};

static int flounder_wifi_status_register(
		void (*callback)(int card_present, void *dev_id),
		void *dev_id)
{
	if (wifi_status_cb)
		return -EAGAIN;
	wifi_status_cb = callback;
	wifi_status_cb_devid = dev_id;
	return 0;
}

static int flounder_wifi_set_carddetect(int val)
{
	pr_debug("%s: %d\n", __func__, val);
	if (wifi_status_cb)
		wifi_status_cb(val, wifi_status_cb_devid);
	else
		pr_warn("%s: Nobody to notify\n", __func__);
	return 0;
}

static int flounder_wifi_power(int on)
{
	pr_err("%s: %d\n", __func__, on);

	gpio_set_value(FLOUNDER_WLAN_PWR, on);
	msleep(100);

	return 0;
}

static int flounder_wifi_reset(int on)
{
	pr_debug("%s: do nothing\n", __func__);
	return 0;
}

static unsigned char flounder_mac_addr[IFHWADDRLEN] = { 0, 0x90, 0x4c, 0, 0, 0 };

static int __init flounder_mac_addr_setup(char *str)
{
	char macstr[IFHWADDRLEN*3];
	char *macptr = macstr;
	char *token;
	int i = 0;

	if (!str)
		return 0;
	pr_debug("wlan MAC = %s\n", str);
	if (strlen(str) >= sizeof(macstr))
		return 0;
	strcpy(macstr, str);

	while ((token = strsep(&macptr, ":")) != NULL) {
		unsigned long val;
		int res;

		if (i >= IFHWADDRLEN)
			break;
		res = kstrtoul(token, 0x10, &val);
		if (res < 0)
			return 0;
		flounder_mac_addr[i++] = (u8)val;
	}

	return 1;
}

__setup("flounder.wifimacaddr=", flounder_mac_addr_setup);

static int flounder_wifi_get_mac_addr(unsigned char *buf)
{
	uint rand_mac;

	if (!buf)
		return -EFAULT;

	if ((flounder_mac_addr[4] == 0) && (flounder_mac_addr[5] == 0)) {
		prandom_seed((uint)jiffies);
		rand_mac = prandom_u32();
		flounder_mac_addr[3] = (unsigned char)rand_mac;
		flounder_mac_addr[4] = (unsigned char)(rand_mac >> 8);
		flounder_mac_addr[5] = (unsigned char)(rand_mac >> 16);
	}
	memcpy(buf, flounder_mac_addr, IFHWADDRLEN);
	return 0;
}

#define WLC_CNTRY_BUF_SZ	4		/* Country string is 3 bytes + NUL */

static char flounder_country_code[WLC_CNTRY_BUF_SZ] = { 0 };

static int __init flounder_country_code_setup(char *str)
{
	if (!str)
		return 0;
	pr_debug("wlan country code = %s\n", str);
	if (strlen(str) >= sizeof(flounder_country_code))
		return 0;
	strcpy(flounder_country_code, str);
	return 1;
}
__setup("androidboot.wificountrycode=", flounder_country_code_setup);

struct cntry_locales_custom {
	char iso_abbrev[WLC_CNTRY_BUF_SZ];	/* ISO 3166-1 country abbreviation */
	char custom_locale[WLC_CNTRY_BUF_SZ];	/* Custom firmware locale */
	s32 custom_locale_rev;		/* Custom local revisin default -1 */
};

struct cntry_locales_custom country_code_custom_table[] = {
/* Table should be filled out based on custom platform regulatory requirement */
	{"",   "XZ", 11},  /* Universal if Country code is unknown or empty */
	{"US", "US", 0},
	{"AE", "AE", 1},
	{"AR", "AR", 21},
	{"AT", "AT", 4},
	{"AU", "AU", 6},
	{"BE", "BE", 4},
	{"BG", "BG", 4},
	{"BN", "BN", 4},
	{"BR", "BR", 4},
	{"CA", "US", 0},   /* Previousely was CA/31 */
	{"CH", "CH", 4},
	{"CY", "CY", 4},
	{"CZ", "CZ", 4},
	{"DE", "DE", 7},
	{"DK", "DK", 4},
	{"EE", "EE", 4},
	{"ES", "ES", 4},
	{"FI", "FI", 4},
	{"FR", "FR", 5},
	{"GB", "GB", 6},
	{"GR", "GR", 4},
	{"HK", "HK", 2},
	{"HR", "HR", 4},
	{"HU", "HU", 4},
	{"IE", "IE", 5},
	{"IN", "IN", 3},
	{"IS", "IS", 4},
	{"IT", "IT", 4},
	{"ID", "ID", 13},
	{"JP", "JP", 58},
	{"KR", "KR", 57},
	{"KW", "KW", 5},
	{"LI", "LI", 4},
	{"LT", "LT", 4},
	{"LU", "LU", 3},
	{"LV", "LV", 4},
	{"MA", "MA", 2},
	{"MT", "MT", 4},
	{"MX", "MX", 20},
	{"MY", "MY", 3},
	{"NL", "NL", 4},
	{"NO", "NO", 4},
	{"NZ", "NZ", 4},
	{"PL", "PL", 4},
	{"PT", "PT", 4},
	{"PY", "PY", 2},
	{"QA", "QA", 0},
	{"RO", "RO", 4},
	{"RU", "RU", 13},
	{"SA", "SA", 26},
	{"SE", "SE", 4},
	{"SG", "SG", 4},
	{"SI", "SI", 4},
	{"SK", "SK", 4},
	{"TH", "TH", 5},
	{"TR", "TR", 7},
	{"TW", "TW", 1},
	{"VN", "VN", 4},
	{"IR", "XZ", 11},	/* Universal if Country code is IRAN, (ISLAMIC REPUBLIC OF) */
	{"SD", "XZ", 11},	/* Universal if Country code is SUDAN */
	{"SY", "XZ", 11},	/* Universal if Country code is SYRIAN ARAB REPUBLIC */
	{"GL", "XZ", 11},	/* Universal if Country code is GREENLAND */
	{"PS", "XZ", 11},	/* Universal if Country code is PALESTINIAN TERRITORIES */
	{"TL", "XZ", 11},	/* Universal if Country code is TIMOR-LESTE (EAST TIMOR) */
	{"MH", "XZ", 11},	/* Universal if Country code is MARSHALL ISLANDS */
};

struct cntry_locales_custom country_code_nodfs_table[] = {
	{"",   "XZ", 40},  /* Universal if Country code is unknown or empty */
	{"US", "US", 172},
	{"AM", "E0", 26},
	{"AU", "AU", 37},
	{"BG", "E0", 26},
	{"BR", "BR", 18},
	{"CA", "US", 172},
	{"CH", "E0", 26},
	{"CY", "E0", 26},
	{"CZ", "E0", 26},
	{"DE", "E0", 26},
	{"DK", "E0", 26},
	{"DZ", "E0", 26},
	{"EE", "E0", 26},
	{"ES", "E0", 26},
	{"EU", "E0", 26},
	{"FI", "E0", 26},
	{"FR", "E0", 26},
	{"GB", "E0", 26},
	{"GR", "E0", 26},
	{"HK", "SG", 17},
	{"HR", "E0", 26},
	{"HU", "E0", 26},
	{"ID", "ID", 1},
	{"IE", "E0", 26},
	{"IL", "E0", 26},
	{"IN", "IN", 27},
	{"IQ", "E0", 26},
	{"IS", "E0", 26},
	{"IT", "E0", 26},
	{"JP", "JP", 83},
	{"KR", "KR", 79},
	{"KW", "E0", 26},
	{"KZ", "E0", 26},
	{"LI", "E0", 26},
	{"LT", "E0", 26},
	{"LU", "E0", 26},
	{"LV", "LV", 4},
	{"LY", "E0", 26},
	{"MA", "E0", 26},
	{"MT", "E0", 26},
	{"MY", "MY", 15},
	{"MX", "US", 172},
	{"NL", "E0", 26},
	{"NO", "E0", 26},
	{"OM", "E0", 26},
	{"PL", "E0", 26},
	{"PT", "E0", 26},
	{"QA", "QA", 0},
	{"RO", "E0", 26},
	{"RS", "E0", 26},
	{"SA", "SA", 26},
	{"SE", "E0", 26},
	{"SG", "SG", 17},
	{"SI", "E0", 26},
	{"SK", "E0", 26},
	{"SZ", "E0", 26},
	{"TH", "TH", 9},
	{"TN", "E0", 26},
	{"TR", "E0", 26},
	{"TW", "TW", 60},
	{"ZA", "E0", 26},
};

static void* flounder_wifi_get_country_code(char *country_iso_code, u32 flags)
{
	struct cntry_locales_custom *locales;
	int size, i;

	if (flags & WLAN_PLAT_NODFS_FLAG) {
		locales = country_code_nodfs_table;
		size = ARRAY_SIZE(country_code_nodfs_table);
	} else {
		locales = country_code_custom_table;
		size = ARRAY_SIZE(country_code_custom_table);
	}

	if (size == 0)
		 return NULL;

	if (!country_iso_code || country_iso_code[0] == 0)
		country_iso_code = flounder_country_code;

	for (i = 0; i < size; i++) {
		if (strcmp(country_iso_code, locales[i].iso_abbrev) == 0)
			return &locales[i];
	}
	/* if no country code matched return first universal code */
	return &locales[0];
}

static int __init flounder_wifi_init(void)
{
	int rc;

	rc = gpio_request(FLOUNDER_WLAN_PWR, "wlan_power");
	if (rc)
		pr_err("WLAN_PWR gpio request failed:%d\n", rc);
	rc = gpio_request(FLOUNDER_WLAN_WOW, "bcmsdh_sdmmc");
	if (rc)
		pr_err("WLAN_WOW gpio request failed:%d\n", rc);

	rc = gpio_direction_output(FLOUNDER_WLAN_PWR, 0);
	if (rc)
		pr_err("WLAN_PWR gpio direction configuration failed:%d\n", rc);
	rc = gpio_direction_input(FLOUNDER_WLAN_WOW);
	if (rc)
		pr_err("WLAN_WOW gpio direction configuration failed:%d\n", rc);

	wifi_resource[0].start = wifi_resource[0].end =
		gpio_to_irq(FLOUNDER_WLAN_WOW);

	platform_device_register(&flounder_wifi_device);

	mrvl_wifi_resource[0].start = mrvl_wifi_resource[0].end =
		gpio_to_irq(FLOUNDER_WLAN_WOW);
	platform_device_register(&marvell_wifi_device);

	return 0;
}

int __init flounder_sdhci_init(void)
{
	int nominal_core_mv;
	int min_vcore_override_mv;
	int boot_vcore_mv;
	u32 speedo;

	nominal_core_mv =
		tegra_dvfs_rail_get_nominal_millivolts(tegra_core_rail);
	if (nominal_core_mv) {
		tegra_sdhci_platform_data0.nominal_vcore_mv = nominal_core_mv;
		tegra_sdhci_platform_data3.nominal_vcore_mv = nominal_core_mv;
	}
	min_vcore_override_mv =
		tegra_dvfs_rail_get_override_floor(tegra_core_rail);
	if (min_vcore_override_mv) {
		tegra_sdhci_platform_data0.min_vcore_override_mv =
			min_vcore_override_mv;
		tegra_sdhci_platform_data3.min_vcore_override_mv =
			min_vcore_override_mv;
	}
	boot_vcore_mv = tegra_dvfs_rail_get_boot_level(tegra_core_rail);
	if (boot_vcore_mv) {
		tegra_sdhci_platform_data0.boot_vcore_mv = boot_vcore_mv;
		tegra_sdhci_platform_data3.boot_vcore_mv = boot_vcore_mv;
	}

	tegra_sdhci_platform_data0.max_clk_limit = 204000000;

	speedo = tegra_fuse_readl(FUSE_SOC_SPEEDO_0);
	tegra_sdhci_platform_data0.cpu_speedo = speedo;
	tegra_sdhci_platform_data3.cpu_speedo = speedo;

	tegra_sdhci_platform_data3.max_clk_limit = 200000000;

	platform_device_register(&tegra_sdhci_device3);
	platform_device_register(&tegra_sdhci_device0);
	flounder_wifi_init();

	return 0;
}
