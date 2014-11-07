/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*                          
                                  
                                                 
  
                                                       
                                                     
                   
                                                        
                          
                                  
  
                            
      
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/export.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/odin_pmic.h>
#ifndef CONFIG_OF
#ifdef CONFIG_GPIO_ODIN
#include <mach/gpio_names.h>
#endif
#endif /* !CONFIG_OF */

#ifndef CONFIG_OF
#include <mach/sdhci.h>
#include <mach/woden-wifi.h>
#else /* !CONFIG_OF */
#include "sdhci-odin.h"
#endif /* CONFIG_OF */

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

/* For HS200 */
#include <linux/clk-private.h>
#include <linux/clkdev.h>
#include <linux/mmc/card.h>
/* end HS200 */

/* RUNTIME PM */
#if defined(CONFIG_PM_RUNTIME)
#include <linux/pm_runtime.h>
#endif

#include "sdhci.h"
#include <linux/odin_pmic.h>
#include <linux/odin_iommu.h>

#define SDIO_3_0_UHS12_25CLK	25*1000*1000
#define SDIO_3_0_UHS25_50CLK	50*1000*1000
#define SDIO_3_0_UHS50_100CLK	100*1000*1000
#define SDIO_3_0_UHS104_200CLK	200*1000*1000

#define SDHCI_ODIN_EMMC_HS200_CLK 200*1000*1000
#define SDHCI_ODIN_uSD_CLK		  200*1000*1000

#ifdef CONFIG_WIFI_CONTROL_FUNC
struct sdhci_host *g_wifi_host = NULL;
#endif

struct sdhci_odin_drv_data {
	struct sdhci_odin_platform_data *pdata;
	struct sdhci_host *host;
	unsigned int dev_id;
	bool pm_resume_flag;	/* 0: count over 1 */
	struct clk *clk;
	struct clk *bus_clk;
#ifdef CONFIG_WODEN_FPGA
	void __iomem *ext_cfg_ioaddr;
	void __iomem *qos_ctrl_ioaddr;
#endif
};

/**
 *	sdhci_odin_mmc_of_parse() - parse host's Odin device-tree node
 *	@host: host whose node should be parsed.
 *
 *	Base structure and code taken from drivers/mmc/core/host.c mmc_of_parse()
 *
 */
void sdhci_odin_mmc_of_parse(struct mmc_host *host)
{
	struct device_node *np = NULL;
	struct sdhci_host *sdhci = NULL;
	struct sdhci_odin_drv_data *drv_data = NULL;
	struct sdhci_odin_platform_data *pdata = NULL;
	struct platform_device *pdev = to_platform_device(mmc_dev(host));
	u32 bus_width = 0;
	int len = 0, ret = 0;
	uint32_t val = 0;

	if (!host->parent || !host->parent->of_node)
		return;

	np = host->parent->of_node;
	sdhci = mmc_priv(host);
	drv_data = sdhci_priv(sdhci);
	pdata = drv_data->pdata;

	/* Odin specific device tree values */
	if (!of_property_read_u32(np, "sdio_port", &val))
		pdata->sdio_port = val;
	else
		pdata->sdio_port = 0;
	if (!of_property_read_u32(np, "sdclk", &val)) {
		pdata->sdio_clk = val;
		switch (pdata->sdio_clk)
		{
			case 25:
				sdhci->caps |= MMC_CAP_UHS_SDR12;
				break;
			case 50:
				sdhci->caps |= MMC_CAP_UHS_SDR25;
				break;
			case 100:
				sdhci->caps |= MMC_CAP_UHS_SDR50;
				break;
			case 200:
				sdhci->caps |= MMC_CAP_UHS_SDR104;
				break;
			default:
				sdhci->caps |= MMC_CAP_UHS_SDR50;
				break;
		}
		printk(KERN_DEBUG "%s sdio_clk[%d] %d\n",
				__func__, pdata->sdio_port, pdata->sdio_clk);
	}
#ifdef CONFIG_WIFI_CONTROL_FUNC
	if (of_find_property(np, "wifi", &len))
		g_wifi_host = sdhci;
#endif
	if (!of_property_read_u32(np, "tuning_mode", &val))
		sdhci->tuning_mode = val;
	if (!of_property_read_u32(np, "sdc_cd_gpio", &val)) {
		pdata->ext_cd_gpio = val;
		pr_debug("gpio num# : [%d]\n", pdata->ext_cd_gpio);

		if (gpio_is_valid(pdata->ext_cd_gpio)) {
			ret = mmc_gpio_request_cd(host, pdata->ext_cd_gpio);
			if (ret < 0) {
				dev_err(host->parent,
					"Failed to request CD GPIO #%d: %d!\n",
					pdata->ext_cd_gpio, ret);
			}
			else
				dev_info(host->parent, "Got CD GPIO #%d.\n",
					 pdata->ext_cd_gpio);
		}
	}
	if (!of_property_read_u32(np, "cmd_timeout", &val))
		sdhci->cmd_timeout = val;
	else
		sdhci->cmd_timeout = 10;

	/* "bus-width" is translated to MMC_CAP_*_BIT_DATA flags */
	if (of_property_read_u32(np, "bus-width", &bus_width) < 0) {
		dev_err(host->parent,
			"\"bus-width\" property is missing, assuming 1 bit.\n");
		bus_width = 1;
	}
	dev_err(host->parent,
			"\"bus-width\" is %d.\n", bus_width);

	switch (bus_width) {
	case 8:
		host->caps |= MMC_CAP_8_BIT_DATA;
		/* Hosts capable of 8-bit transfers can also do 4 bits */
	case 4:
		host->caps |= MMC_CAP_4_BIT_DATA;
		break;
	case 1:
		break;
	default:
		dev_err(host->parent,
			"Invalid \"bus-width\" value %ud!\n", bus_width);
	}

	/* f_max is obtained from the optional "max-frequency" property */
	of_property_read_u32(np, "max-frequency", &host->f_max);

	if (of_find_property(np, "non-removable", &len)) {
		host->caps |= MMC_CAP_NONREMOVABLE;
	}

	if (of_find_property(np, "cap-sd-highspeed", &len))
		host->caps |= MMC_CAP_SD_HIGHSPEED;
	if (of_find_property(np, "cap-mmc-highspeed", &len))
		host->caps |= MMC_CAP_MMC_HIGHSPEED;
	if (of_find_property(np, "cap-power-off-card", &len))
		host->caps |= MMC_CAP_POWER_OFF_CARD;
	if (of_find_property(np, "cap-sdio-irq", &len))
		host->caps |= MMC_CAP_SDIO_IRQ;
	if (of_find_property(np, "cap-1.8v-ddr", &len))
		host->caps |= MMC_CAP_1_8V_DDR;

	if (of_find_property(np, "cap2-hs200", &len))
		host->caps2 |= MMC_CAP2_HS200;
	if (of_find_property(np, "cap2-cache-ctrl", &len))
		host->caps2 |= MMC_CAP2_CACHE_CTRL;
	if (of_find_property(np, "cap2-packed-cmd", &len))
		host->caps2 |= MMC_CAP2_PACKED_CMD;
	if (of_find_property(np, "cap2-poweroff-notify", &len))
		host->caps2 |= MMC_CAP2_POWEROFF_NOTIFY;
	if (of_find_property(np, "cap2-enable-bkops", &len))
		host->caps2 |= MMC_CAP2_ENABLE_BKOPS;
	if (of_find_property(np, "cap2-no-rescan-start", &len))
		host->caps2 |= MMC_CAP2_NO_RESCAN_START;

	if (of_find_property(np, "keep-power-in-suspend", &len))
		host->pm_caps |= MMC_PM_KEEP_POWER;
	if (of_find_property(np, "enable-sdio-wakeup", &len))
		host->pm_caps |= MMC_PM_WAKE_SDIO_IRQ;

	if (of_find_property(np, "quirk-cap-clock-base-broken", &len))
		sdhci->quirks |= SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN;
	if (of_find_property(np, "quirk-broken-dma", &len))
		sdhci->quirks |= SDHCI_QUIRK_BROKEN_DMA;
	if (of_find_property(np, "quirk-broken-adma", &len))
		sdhci->quirks |= SDHCI_QUIRK_BROKEN_ADMA;

	if (of_find_property(np, "quirk2-preset-value-broken", &len))
		sdhci->quirks2 |= SDHCI_QUIRK2_PRESET_VALUE_BROKEN;
	if (of_find_property(np, "quirk2-host-off-card-on", &len))
		sdhci->quirks2 |= SDHCI_QUIRK2_HOST_OFF_CARD_ON;
	if (of_find_property(np, "quirk2-use-max-discard-size", &len))
		sdhci->quirks2 |= SDHCI_QUIRK2_USE_MAX_DISCARD_SIZE;

	return;
}

static int wifi_status = 0;
#ifdef CONFIG_WIFI_CONTROL_FUNC
void sdhci_odin_wifi_set_status(int status)
{
	wifi_status = status;
}
EXPORT_SYMBOL(sdhci_odin_wifi_set_status);

int sdhci_odin_wifi_carddetect(void)
{
	int wait = 100;

	if (!g_wifi_host) {
		pr_err("Can't get wifi host\n");
		return -ENOMEM;
	}

	/* We have to remove this flag when Wifi on/off triggers */
	g_wifi_host->mmc->caps &= ~MMC_CAP_NONREMOVABLE;

	if (!(sdhci_readl(g_wifi_host, SDHCI_PRESENT_STATE) &
		SDHCI_CARD_PRESENT)) {
		pr_err("%s: No wifi card in slot\n", mmc_hostname(g_wifi_host->mmc));
		return -ENODEV;
	}

	mmc_detect_change(g_wifi_host->mmc, msecs_to_jiffies(200));

	while (--wait) {
		if (g_wifi_host->mmc->card)
			break;

		msleep(100);
	}

	if (!wait) {
		pr_err("%s: Failed to detect wifi card\n",\
				mmc_hostname(g_wifi_host->mmc));
		return -ENODEV;
	}

	return 0;
}
EXPORT_SYMBOL(sdhci_odin_wifi_carddetect);
#endif /* CONFIG_WIFI_CONTROL_FUNC */

int sdhci_odin_switch_1_8v(void)
{
	int ret;

	unsigned int mbox_data[7] = {0, };

	mbox_data[0] = PM_CMD_TYPE;
	mbox_data[1] = 0x81000000;

	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_SD_PMIC, mbox_data, 1000);

	if (ret < 0) {
		ret = -ETIMEDOUT;
		pr_err("%s: Failed to switch voltage: errno(%d)\n",
				__func__, ret);
	}

	sdhci_odin_iopad_ctrl(SDHCI_IOPAD_1_8V);

	return ret;
}
EXPORT_SYMBOL(sdhci_odin_switch_1_8v);

int sdhci_odin_switch_3_3v(void)
{
	int ret;

	unsigned int mbox_data[7] = {0, };

	mbox_data[0] = PM_CMD_TYPE;
	mbox_data[1] = 0x81000001;

	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_SD_PMIC, mbox_data, 1000);

	if (ret < 0) {
		ret = -ETIMEDOUT;
		pr_err("%s: Failed to switch voltage: errno(%d)\n",
				__func__, ret);
	}

	sdhci_odin_iopad_ctrl(SDHCI_IOPAD_3_3V);

	return ret;
}
EXPORT_SYMBOL(sdhci_odin_switch_3_3v);

void sdhci_odin_iopad_ctrl(unsigned int iopad_set)
{
	void __iomem *iopadptr;
	unsigned int sdc_conf0_ret;

	iopadptr = ioremap(SDHCI_ODIN_SDC_CONF0, 0x1000);

	if (!iopadptr) {
		pr_err("%s : ioremap failed\n",__func__);
		goto err;
	}

	sdc_conf0_ret = readl(iopadptr + 0x200);

	switch (iopad_set) {
		case SDHCI_IOPAD_1_8V: {
				sdc_conf0_ret |= (0x1 << 24);
				writel(sdc_conf0_ret, iopadptr + 0x200);
				break;
			}
		case SDHCI_IOPAD_3_3V: {
				sdc_conf0_ret = sdc_conf0_ret & ~(0x1 << 24);
				writel(sdc_conf0_ret, iopadptr + 0x200);
				break;
			}
		default:
			goto err;
	}

err :
	iounmap(iopadptr);
}
EXPORT_SYMBOL(sdhci_odin_iopad_ctrl);

void sdhci_set_xinclk(unsigned int xinclk)
{
	void __iomem *ioaddr;
	u32 sdc_conf1;

	ioaddr = ioremap(SDHCI_ODIN_SDC_CONF0 + 0x204, 0x4);
	if (!ioaddr) {
		pr_err("%s : ioremap failed\n", __func__);
		goto err;
	}

	sdc_conf1 = readl(ioaddr);
	pr_info("sdc_conf1[0x%x]\n", sdc_conf1);

	sdc_conf1 &= ~0xff00;
	sdc_conf1 |= (xinclk << 8);

	writel(sdc_conf1, ioaddr);
	pr_info("sdc_conf1[0x%x]\n", sdc_conf1);

err:
	iounmap(ioaddr);
}
EXPORT_SYMBOL(sdhci_set_xinclk);

static unsigned int sdhci_odin_get_max_clk(struct sdhci_host *host)
{
#if defined (SDHCI_MAX_CLOCK)
	return SDHCI_MAX_CLOCK;
#else /* SDHCI_MAX_CLOCK */
	struct sdhci_odin_drv_data *drv_data = sdhci_priv(host);
	if ((host->mmc->caps2 & MMC_CAP2_HS200)) {
		clk_set_rate(drv_data->clk, SDHCI_ODIN_EMMC_HS200_CLK);
	}
	if (host->dev_id == SDHCI_ODIN_SD)
		clk_set_rate(drv_data->clk, SDHCI_ODIN_uSD_CLK);

	return clk_get_rate(drv_data->clk);
#endif /* !SDHCI_MAX_CLOCK */
}

static void sdhci_itapdly(void)
{
	void __iomem *ioaddr;
	u32 sdc_conf0;

	ioaddr = ioremap(SDHCI_ODIN_SDC_CONF0 + 0x200, 0x4);
	if (!ioaddr) {
		pr_err("%s : ioremap failed\n",__func__);
		goto err;
	}

	sdc_conf0 = readl(ioaddr);
	pr_info("sdc_conf0[0x%x]\n", sdc_conf0);

	writel((sdc_conf0 | 0x100), ioaddr);
	mdelay(1);

	sdc_conf0 = readl(ioaddr);
	pr_info("sdc_conf0[0x%x]\n", sdc_conf0);

	sdc_conf0 &= ~0x3E00;
	pr_info("sdc_conf0[0x%x]\n", sdc_conf0);

	writel((sdc_conf0 | 0x1400), ioaddr);

	sdc_conf0 = readl(ioaddr);
	writel((sdc_conf0 | 0x4000), ioaddr);

	mdelay(1);

	sdc_conf0 = readl(ioaddr);
	sdc_conf0 &= ~0x100;
	writel(sdc_conf0, ioaddr);

	sdc_conf0 = readl(ioaddr);
	pr_info("sdc_conf0[0x%x]\n", sdc_conf0);

err:
	iounmap(ioaddr);
}

static void sdhci_otapdly(unsigned char tapDelay, unsigned char io_strength)
{
	void __iomem *ioaddr;
	u32 sdc_conf0;

	ioaddr = ioremap(SDHCI_ODIN_SDC_CONF0 + 0x200, 0x4);
	if (!ioaddr) {
		pr_err("%s : ioremap failed\n",__func__);
		goto err;
	}

	sdc_conf0 = readl(ioaddr);
	pr_debug("sdc_conf0[0x%x]\n", sdc_conf0);

	writel((sdc_conf0 | 0x100), ioaddr);
	mdelay(1);

	sdc_conf0 = readl(ioaddr);
	pr_debug("sdc_conf0[0x%x]\n", sdc_conf0);

	sdc_conf0 &= ~0xF;
	writel(sdc_conf0, ioaddr);

	sdc_conf0 = readl(ioaddr);
	pr_debug("sdc_conf0[0x%x]\n", sdc_conf0);

	writel(sdc_conf0 | (tapDelay & 0xF), ioaddr);
	sdc_conf0 = readl(ioaddr);
	pr_debug("sdc_conf0[0x%x]\n", sdc_conf0);

	writel(sdc_conf0 | 0x10, ioaddr);
	sdc_conf0 = readl(ioaddr);
	pr_debug("sdc_conf0[0x%x]\n", sdc_conf0);

	mdelay(1);

	sdc_conf0 = readl(ioaddr);
	sdc_conf0 &= ~0x100;
	writel(sdc_conf0, ioaddr);

	sdc_conf0 = readl(ioaddr);
	pr_debug("sdc_conf0[0x%x]\n", sdc_conf0);

	sdc_conf0 = readl(ioaddr);
	sdc_conf0 &= ~(0x3 << 28);
	writel(sdc_conf0 | (io_strength & 0xF), ioaddr);
	pr_debug("sdc_conf0[0x%x]\n", sdc_conf0);

err:
	iounmap(ioaddr);
}

static ssize_t sdmmc_tap_delay_tuning(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	unsigned long tap_idx=0, io_idx=0, ulTemp;
	char *b = &buf[0], *token;

	token = strsep(&b, " ");
	if (token != NULL)
		if (kstrtoul(token, 0, &tap_idx) < 0)
			return count;

	token = strsep(&b, " ");
	if (token != NULL)
		if (kstrtoul(token, 0, &io_idx) < 0)
			return count;

	sdhci_otapdly(tap_idx, io_idx);

	return count;
}
static DEVICE_ATTR (tap_delay_tuning, 0200, NULL, sdmmc_tap_delay_tuning);

static struct sdhci_ops sdhci_odin_ops = {
	.get_max_clock = sdhci_odin_get_max_clk,
	.set_xinclk = sdhci_set_xinclk,
#if 0	/* Maybe reuse following functions because of changing PMIC */
	.voltage_switch_1_8v = sdhci_odin_switch_1_8V,
	.voltage_switch_3_3v = sdhci_odin_switch_3_3V,
#endif
};

static int sdhci_odin_hw_init(struct sdhci_odin_drv_data *drv_data)
{
	struct sdhci_host *host = drv_data->host;
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));

	if (drv_data->dev_id == SDHCI_ODIN_MMC) {
		drv_data->clk = clk_get(&pdev->dev, NULL);
		drv_data->bus_clk = clk_get(NULL, "emmc_aclk");
	}
	if (drv_data->dev_id == SDHCI_ODIN_SDIO) {
		drv_data->clk = clk_get(&pdev->dev, NULL);
		if (drv_data->pdata->sdio_port == 0x0)
			drv_data->bus_clk = clk_get(NULL, "sdio_aclk0");
		else if (drv_data->pdata->sdio_port == 0x1)
			drv_data->bus_clk = clk_get(NULL, "sdio_aclk1");
	}

	if (drv_data->dev_id == SDHCI_ODIN_SD) {
		drv_data->clk = clk_get(&pdev->dev, NULL);
		drv_data->bus_clk = clk_get(NULL, "sdc_aclk");
	}
	if (IS_ERR(drv_data->clk)) {
		printk(KERN_ERR "Can't get clock\n");
		return -EBUSY;
	}
	if (IS_ERR(drv_data->bus_clk)) {
		printk(KERN_ERR "Can't get bus clock\n");
		return -EBUSY;
	}

	if (drv_data->dev_id == SDHCI_ODIN_SDIO) {
		if (host->caps & MMC_CAP_UHS_SDR12)
			clk_set_rate(drv_data->clk, SDIO_3_0_UHS12_25CLK);
		else if (host->caps & MMC_CAP_UHS_SDR25)
			clk_set_rate(drv_data->clk, SDIO_3_0_UHS25_50CLK);
		else if (host->caps & MMC_CAP_UHS_SDR50)
			clk_set_rate(drv_data->clk, SDIO_3_0_UHS50_100CLK);
		else if (host->caps & MMC_CAP_UHS_SDR104)
			clk_set_rate(drv_data->clk, SDIO_3_0_UHS104_200CLK);
	}

	return 0;
}

static void sdhci_odin_hw_uninit(struct sdhci_odin_drv_data *drv_data)
{
	if (drv_data->clk) {
		clk_disable_unprepare(drv_data->clk);
		clk_put(drv_data->clk);
	}
}

#ifdef CONFIG_OF
static struct sdhci_odin_platform_data odin_sdhci0_pdata = {
	.dev_id		= SDHCI_ODIN_MMC,
};

static struct sdhci_odin_platform_data odin_sdhci1_pdata = {
	.dev_id		= SDHCI_ODIN_SDIO,
};

static struct sdhci_odin_platform_data odin_sdhci2_pdata = {
	.dev_id		= SDHCI_ODIN_SD,
};

static struct sdhci_odin_platform_data odin_sdhci3_pdata = {
	.dev_id		= SDHCI_ODIN_SDIO,
};

static struct of_device_id odin_sdhci_match[] = {
	{ .compatible = "woden,emmc", .data = &odin_sdhci0_pdata},
	{ .compatible = "odin,emmc", .data = &odin_sdhci0_pdata},
	{ .compatible = "woden,sdio", .data = &odin_sdhci1_pdata},
	{ .compatible = "odin,sdio", .data = &odin_sdhci1_pdata},
	{ .compatible = "odin,sd", .data = &odin_sdhci2_pdata},
	{ .compatible = "odin,sdio1", .data = &odin_sdhci3_pdata},
};
#endif /* CONFIG_OF */

static int __init sdhci_odin_probe(struct platform_device *pdev)
{
	struct sdhci_odin_drv_data *drv_data;
	struct sdhci_host *host;
	struct resource *iomem;
	int ret;

	uint32_t tuning_mode_tmp = 0;
	const struct of_device_id *of_id;
	of_id = of_match_device(odin_sdhci_match, &pdev->dev);
	pdev->dev.platform_data = of_id->data;

	iomem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!iomem) {
		ret = -ENOMEM;
		goto err;
	}

	if (resource_size(iomem) < 0x100)
		dev_err(&pdev->dev, "Invalid iomem size.\n");

	host = sdhci_alloc_host(&pdev->dev, sizeof(struct sdhci_odin_drv_data));

	if (IS_ERR(host)) {
		ret = PTR_ERR(host);
		goto err;
	}

	drv_data = sdhci_priv(host);
	drv_data->host = host;

	drv_data->pdata = pdev->dev.platform_data;

	if (drv_data->pdata == NULL) {
		dev_err(&pdev->dev, "Missing Odin sdhci platform data\n");
		ret = -ENXIO;
		goto free_host;
	}

	drv_data->dev_id = drv_data->pdata->dev_id;

	host->hw_name = dev_name(&pdev->dev);
	host->ops = &sdhci_odin_ops;
	host->irq = platform_get_irq(pdev, 0);

	if (!request_mem_region(iomem->start, resource_size(iomem),
		mmc_hostname(host->mmc))) {
		dev_err(&pdev->dev, "Cannot request resion\n");
		ret = -EBUSY;
		goto free_host;
	}

	host->ioaddr = ioremap(iomem->start, resource_size(iomem));
	if (!host->ioaddr) {
		dev_err(&pdev->dev, "Failed to remap registers\n");
		ret = -ENOMEM;
		goto release_mem;
	}

	sdhci_odin_mmc_of_parse(host->mmc);
	if (host->tuning_mode)
		tuning_mode_tmp = host->tuning_mode;

	ret = sdhci_odin_hw_init(drv_data);
	if (ret)
		goto unmap_io;

	if (drv_data->dev_id)
		host->dev_id = drv_data->dev_id;

	platform_set_drvdata(pdev, host);

#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_enable(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, ODIN_SDHCI_PM_TIMEOUT);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, true);
#endif

	/*
	 * dma_mask needs setting properly before calling sdhci_add_host() to
	 * avoid unnecessary buffer bouncing on block layer.
	 *
	 * REVISIT
	 *  - Confirm the limit of DMA-able address of SDMA and ADMA
	 *    respectively (dma descriptor format).
	 *  - sdhci.c driver handles physical address of data as if it's 32 bit
	 *    width. SO YOU LOOSE upper 32 bit even if you set dma_mask to
	 *    -1UL on 64 bit and your platform is capable of accessing 64 bit
	 *    physical addresses.
	 *  - The addressable limit extends to excess of 32 bit on LPAE enabled
	 *    32 bit ARM processor.
	 */
	if (!((host->quirks &
		(SDHCI_QUIRK_BROKEN_DMA | SDHCI_QUIRK_BROKEN_ADMA)) ==
		(SDHCI_QUIRK_BROKEN_DMA | SDHCI_QUIRK_BROKEN_ADMA))) {
#if 0
		if (odin_has_dram_3gbyte() == true) {
			if (host->dev_id != SDHCI_ODIN_SDIO)
				host->dma_mask = DMA_BIT_MASK(33);
			else
				host->dma_mask = DMA_BIT_MASK(32);
		} else
#endif
			host->dma_mask = DMA_BIT_MASK(32);
		mmc_dev(host->mmc)->dma_mask = &host->dma_mask;
	}

#if 0
	if (odin_has_dram_3gbyte() == true) {
		if (host->dev_id != SDHCI_ODIN_SDIO)
			odin_iommu_add_device(&pdev->dev);
	}
#endif

	ret = sdhci_add_host(host);

	if (host->dev_id == SDHCI_ODIN_SD) {
		sdhci_itapdly();
		sdhci_otapdly(0x6, 0x2);
		device_create_file(&pdev->dev, &dev_attr_tap_delay_tuning);
	}

	if (ret)
		goto uninit_hw;

	if (tuning_mode_tmp) {
		host->tuning_mode = tuning_mode_tmp;
		dev_dbg(&pdev->dev, "host->tuning_mode : %d\n", host->tuning_mode);
	}

	device_enable_async_suspend(&pdev->dev);

	return 0;

uninit_hw:
	sdhci_odin_hw_uninit(drv_data);

unmap_io:
	iounmap(host->ioaddr);

release_mem:
	release_mem_region(iomem->start, resource_size(iomem));

free_host:
	sdhci_free_host(host);
err:
#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
#endif
	dev_err(&pdev->dev, "Probing of sdhci-odin failed: %d\n", ret);
	return ret;
}

static int __exit sdhci_odin_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_odin_drv_data *drv_data = sdhci_priv(host);
	struct resource *iomem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	int dead;
	u32 scratch;

#if defined(CONFIG_PM_RUNTIME)
	if (pm_runtime_suspended(&pdev->dev))
		pm_runtime_resume(&pdev->dev);
#endif
	dead = 0;
	scratch = readl(host->ioaddr + SDHCI_INT_STATUS);
	if (scratch == (u32)-1)
		dead = 1;

	sdhci_remove_host(host, dead);
	sdhci_odin_hw_uninit(drv_data);
	iounmap(host->ioaddr);

	if (gpio_is_valid(drv_data->pdata->ext_cd_gpio))
	{
		mmc_gpio_free_cd(host->mmc);
	}

	release_mem_region(iomem->start, resource_size(iomem));

	/* For SD card */
	if (drv_data->dev_id == SDHCI_ODIN_SD)
	{
		sdhci_odin_switch_3_3v();
	}

	sdhci_free_host(host);

#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
#endif
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#if CONFIG_PM
static int sdhci_odin_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_odin_drv_data *drv_data = sdhci_priv(host);
	int ret = 0;

	if ((host->runtime_suspended)) {
		if (drv_data->dev_id == SDHCI_ODIN_MMC) {
			drv_data->clk = clk_get(dev, NULL);
			drv_data->bus_clk = clk_get(NULL, "emmc_aclk");
		}

		if (drv_data->dev_id == SDHCI_ODIN_SDIO) {
			drv_data->clk = clk_get(dev, NULL);
			if (drv_data->pdata->sdio_port == 0x0)
				drv_data->bus_clk = clk_get(NULL, "sdio_aclk0");
			else if (drv_data->pdata->sdio_port == 0x1)
				drv_data->bus_clk = clk_get(NULL, "sdio_aclk1");
		}

		if (drv_data->dev_id == SDHCI_ODIN_SD) {
			drv_data->clk = clk_get(dev, NULL);
			drv_data->bus_clk = clk_get(NULL, "sdc_aclk");
		}

		if (IS_ERR(drv_data->clk)) {
			printk(KERN_ERR "Can't get clock\n");
			return -EBUSY;
		}

		if (IS_ERR(drv_data->bus_clk)) {
			printk(KERN_ERR "Can't get bus clock\n");
			return -EBUSY;
		}

		if (drv_data->pm_resume_flag == true) {
			if (clk_prepare_enable(drv_data->clk)) {
				printk(KERN_ERR "Failed to prepare clock\n");
				return -EBUSY;
			}

			if (clk_prepare_enable(drv_data->bus_clk)) {
				printk(KERN_ERR "Failed to prepare clock\n");
				return -EBUSY;
			}
			drv_data->pm_resume_flag = false;
		}

		ret = sdhci_runtime_resume_host(host);
	}

	if (drv_data->dev_id == SDHCI_ODIN_SDIO) {
		if (drv_data->pdata->sdio_port == 0x0)
			host->mmc->caps |= MMC_CAP_NONREMOVABLE;
		host->mmc->pm_flags |= MMC_PM_KEEP_POWER;
	}

	if (drv_data->dev_id == SDHCI_ODIN_MMC) {
		host->mmc->caps |= MMC_CAP_NONREMOVABLE;
	}

	ret = sdhci_suspend_host(host);

	if ((ret != 0) && (drv_data->dev_id == SDHCI_ODIN_SDIO)) {
		printk(KERN_ERR "%s:sdhci_suspend_host(dev_id:%d) Failed\n",\
				__func__, drv_data->dev_id);
		return ret;
	}

	if (drv_data->clk) {
		clk_disable_unprepare(drv_data->clk);
		clk_put(drv_data->clk);
	}

	if (drv_data->bus_clk) {
		clk_disable_unprepare(drv_data->bus_clk);
		clk_put(drv_data->bus_clk);
	}

	return ret;
}

static int sdhci_odin_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_odin_drv_data *drv_data = sdhci_priv(host);
	int ret = 0;
	u32 present;

	if (drv_data->dev_id == SDHCI_ODIN_MMC) {
		drv_data->clk = clk_get(dev, NULL);
		drv_data->bus_clk = clk_get(NULL, "emmc_aclk");
	}
	if (drv_data->dev_id == SDHCI_ODIN_SDIO) {
		drv_data->clk = clk_get(dev, NULL);
		if (drv_data->pdata->sdio_port == 0x0)
			drv_data->bus_clk = clk_get(NULL, "sdio_aclk0");
		else if (drv_data->pdata->sdio_port == 0x1)
			drv_data->bus_clk = clk_get(NULL, "sdio_aclk1");
	}

	if (drv_data->dev_id == SDHCI_ODIN_SD) {
		drv_data->clk = clk_get(dev, NULL);
		drv_data->bus_clk = clk_get(NULL, "sdc_aclk");
	}

	if (IS_ERR(drv_data->clk)) {
		printk(KERN_ERR "Can't get clock\n");
		return -EBUSY;
	}

	if (IS_ERR(drv_data->bus_clk)) {
		printk(KERN_ERR "Can't get bus clock\n");
		return -EBUSY;
	}

	if (clk_prepare_enable(drv_data->clk)) {
		printk(KERN_ERR "Failed to prepare clock\n");
		return -EBUSY;
	}

	if (clk_prepare_enable(drv_data->bus_clk)) {
		printk(KERN_ERR "Failed to prepare clock\n");
		return -EBUSY;
	}

	if (drv_data->dev_id == SDHCI_ODIN_SDIO) {
		printk(KERN_DEBUG "SIC::JP::%s SDIO waiting for\
				CARD PRESENT - START\n", __func__);
		do {
			present = sdhci_readl(host, SDHCI_PRESENT_STATE)\
					  & SDHCI_CARD_PRESENT;
		} while (!present);
		printk(KERN_DEBUG "SIC::JP::%s SDIO waiting for\
				CARD PRESENT - SUCCESS\n", __func__);
	}

	if (host->dev_id == SDHCI_ODIN_SD)
		sdhci_otapdly(0x6, 0x2);

	ret = sdhci_resume_host(host);

	if (drv_data->dev_id == SDHCI_ODIN_MMC) {
		host->mmc->caps &= ~MMC_CAP_NONREMOVABLE;
	}

	return ret;
}
#else
#define sdhci_odin_suspend	NULL
#define sdhci_odin_resume	NULL
#endif /* CONFIG_PM */

#if defined(CONFIG_PM_RUNTIME)
static int sdhci_odin_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_odin_drv_data *drv_data = sdhci_priv(host);
	int ret = 0;

	if (host->suspended)
		return ret;

	if (drv_data->dev_id == SDHCI_ODIN_SDIO) {
		if (drv_data->pdata->sdio_port == 0x0)
			host->mmc->caps |= MMC_CAP_NONREMOVABLE;
		host->mmc->pm_flags |= MMC_PM_KEEP_POWER;
	}

	ret = sdhci_runtime_suspend_host(host);

	if (drv_data->pm_resume_flag == false){
		if (drv_data->clk) {
			clk_disable_unprepare(drv_data->clk);
			clk_put(drv_data->clk);
		}

		if (drv_data->bus_clk) {
			clk_disable_unprepare(drv_data->bus_clk);
			clk_put(drv_data->bus_clk);
		}

		drv_data->pm_resume_flag = true;
	}

	return ret;
}

static int sdhci_odin_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_odin_drv_data *drv_data = sdhci_priv(host);
	int ret = 0;

	if (host->suspended) {
		host->suspended = false;
		return ret;
	}

	if (drv_data->dev_id == SDHCI_ODIN_MMC) {
		drv_data->clk = clk_get(&pdev->dev, NULL);
		drv_data->bus_clk = clk_get(NULL, "emmc_aclk");
	}

	if (drv_data->dev_id == SDHCI_ODIN_SDIO) {
		drv_data->clk = clk_get(&pdev->dev, NULL);
		if (drv_data->pdata->sdio_port == 0x0)
			drv_data->bus_clk = clk_get(NULL, "sdio_aclk0");
		else if (drv_data->pdata->sdio_port == 0x1)
			drv_data->bus_clk = clk_get(NULL, "sdio_aclk1");
	}

	if (drv_data->dev_id == SDHCI_ODIN_SD) {
		drv_data->clk = clk_get(&pdev->dev, NULL);
		drv_data->bus_clk = clk_get(NULL, "sdc_aclk");
	}

	if (IS_ERR(drv_data->clk)) {
		printk(KERN_ERR "Can't get clock\n");
		return -EBUSY;
	}

	if (IS_ERR(drv_data->bus_clk)) {
		printk(KERN_ERR "Can't get bus clock\n");
		return -EBUSY;
	}

	if (drv_data->pm_resume_flag == true) {
		if (clk_prepare_enable(drv_data->clk)) {
			printk(KERN_ERR "Failed to prepare clock\n");
			return -EBUSY;
		}

		if (clk_prepare_enable(drv_data->bus_clk)) {
			printk(KERN_ERR "Failed to prepare clock\n");
			return -EBUSY;
		}
		drv_data->pm_resume_flag = false;
	}

	ret = sdhci_runtime_resume_host(host);

	return ret;
}
#else
#define sdhci_odin_runtime_suspend	NULL
#define sdhci_odin_runtime_resume	NULL
#endif /* CONFIG_PM */

static struct dev_pm_ops odin_sdhci_pm_ops = {
	.runtime_suspend	 = sdhci_odin_runtime_suspend,
	.runtime_resume		 = sdhci_odin_runtime_resume,
	.suspend			 = sdhci_odin_suspend,
	.resume				 = sdhci_odin_resume,
};

static struct platform_driver sdhci_odin_driver = {
	.driver = {
		.name	= "sdhci-odin",
		.owner	= THIS_MODULE,
		.pm = &odin_sdhci_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odin_sdhci_match),
#endif /* CONFIG_OF */
	},
	.probe		= sdhci_odin_probe,
	.remove		= __exit_p(sdhci_odin_remove),
};
module_platform_driver(sdhci_odin_driver);

MODULE_AUTHOR("Jongmyung Park <jongmyung.park@lge.com>");
MODULE_DESCRIPTION("Secure Digital Host Controller Interface driver for Odin");
MODULE_LICENSE("GPL v2");
