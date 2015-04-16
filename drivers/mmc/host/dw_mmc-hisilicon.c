/*
 * Copyright (c) 2013 Linaro Ltd.
 * Copyright (c) 2013 Hisilicon Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/mmc/host.h>
#include <linux/mmc/dw_mmc.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <linux/clk-private.h>

#include "dw_mmc.h"
#include "dw_mmc-pltfm.h"

#define DRIVER_NAME "dwmmc_hs"
static void __iomem *pericrg_base = NULL;

#define PERI_CRG_RSTEN4             (0x90)
#define PERI_CRG_RSTDIS4            (0x94)
#define PERI_CRG_RSTSTAT4           (0x98)

#define BIT_RST_MMC2         (1<<19)
#define BIT_RST_SD           (1<<18)
#define BIT_RST_EMMC         (1<<17)


enum dw_mci_hisilicon_type {
	DW_MCI_TYPE_HI3620,
	DW_MCI_TYPE_HI3630,
};

static struct dw_mci_hisi_compatible {
	char				*compatible;
	enum dw_mci_hisilicon_type	type;
} hs_compat[] = {
	{
		.compatible	= "hisilicon,hi3620-dw-mshc",
		.type		= DW_MCI_TYPE_HI3620,
	}, {
		.compatible	= "hisilicon,hi3630-dw-mshc",
		.type		= DW_MCI_TYPE_HI3630,
	},
};

struct dw_mci_hs_priv_data {
	int				id;
	int				old_timing;
	int				gpio_cd;
	int 			old_signal_voltage;
	int 			old_power_mode;
	unsigned int 	priv_bus_hz;
	unsigned int    cd_vol;
	enum dw_mci_hisilicon_type	type;
};


static int hs_timing_config[][9][TUNING_INIT_CONFIG_NUM] = {
/* bus_clk, div, drv_sel, sam_dly, sam_phase_max, sam_phase_min, input_clk */
	{
		{360000000, 9,  1, 0, 0, 0, 36000000 },	/* 0: LEGACY 100k */
		{360000000, 6,  1, 0, 1, 1, 52000000 },	/* 1: MMC_HS*/
		{0},									/* 2: SD_HS */
		{0},									/* 3: SDR12 */
		{0}, 									/* 4: SDR25 */
		{0},									/* 5: SDR50 */
		{0},									/* 6: SDR104 */
		{720000000,  6, 0, 4, 6, 0, 102000000},	/* 7: DDR50 */
		{1440000000, 7, 0, 3, 15,0, 180000000}, /* 8: HS200 */
	}, {
		{360000000, 9, 1, 0, 0, 0, 36000000 },	/* 0: LEGACY 400k */
		{0},									/* 1: MMC_HS */
		{360000000, 6, 1, 0, 1, 1, 50000000 },	/* 2: SD_HS */
		{180000000, 6, 1, 2, 13,13,25000000 },	/* 3: SDR12 */
		{360000000, 6, 1, 0, 1, 1, 50000000 },	/* 4: SDR25 */
		{720000000, 6, 0, 3, 9, 0, 100000000},	/* 5: SDR50 */
		{1440000000,7, 0, 5, 15,0, 180000000},  /* 6: SDR104 */
		{0},									/* 7: DDR50 */
		{0},									/* 8: HS200 */
	}, {
		{360000000, 9, 1, 0, 0, 0, 36000000},	/* 0: LEGACY 400k */
		{0},									/* 1: MMC_HS */
		{360000000, 6, 1, 0, 4, 4, 50000000},									/* 2: SD_HS */
		{180000000, 6, 1, 2, 0, 0, 25000000},	/* 3: SDR12 */
		{360000000, 6, 1, 0, 4, 4, 50000000  },	/* 4: SDR25 */
		{720000000, 6, 0, 2, 9, 0, 100000000 },	/* 5: SDR50 */
		{1440000000, 7,0, 4, 15,0, 180000000 },	/* 6: SDR104 */
		{720000000, 15,0, 5, 11,1, 45000000  },	/* 7: DDR50 */
		{0},									/* 8: HS200 */
	}
};

static void dw_mci_hs_set_timing(struct dw_mci *host, int id, int timing, int sam_phase, int clk_div)
{
	int cclk_div;
	int drive_sel;
	int sam_dly;
	int sam_phase_max, sam_phase_min;
	int sam_phase_val;
	int reg_value;
	int enable_shift = 0;
	int use_sam_dly = 0;
	int d_value = 0;

	if (host->hw_mmc_id == DWMMC_SD_ID) {
		d_value = clk_div - hs_timing_config[id][timing][1];
	}

	cclk_div = clk_div;
	drive_sel = hs_timing_config[id][timing][2];
	sam_dly = hs_timing_config[id][timing][3] + d_value;
	sam_phase_max = hs_timing_config[id][timing][4];
	sam_phase_min = hs_timing_config[id][timing][5];

	if (sam_phase == -1)
		sam_phase_val  = (sam_phase_max + sam_phase_min) / 2;
	else
		sam_phase_val = sam_phase;

	if (timing == MMC_TIMING_LEGACY)
		enable_shift = 1;

	if ((id == 0) && (timing == MMC_TIMING_MMC_HS200)) {
		switch (sam_phase_val) {
			case 0:
			case 10:
			case 11:
			case 12:
			case 13:
			case 14:
			case 15:
				use_sam_dly = 1;
				break;
			default:
				use_sam_dly = 0;
		}
	}


	if ((id == 1) && (timing == MMC_TIMING_UHS_SDR104)) {
		if ((sam_phase_val == 0) || (sam_phase_val >= 10))
			use_sam_dly = 1;
	}

	if ((id == 2) && (timing == MMC_TIMING_UHS_SDR104)) {
		switch (sam_phase_val) {
			case 0:
			case 10:
			case 11:
			case 12:
			case 13:
			case 14:
			case 15:
				use_sam_dly = 1;
				break;
			default:
				use_sam_dly = 0;
		}
	}

	/* first disabl clk */
	mci_writel(host, GPIO, ~GPIO_CLK_ENABLE);
	reg_value = SDMMC_UHS_REG_EXT_VALUE(sam_phase_val, sam_dly);
	mci_writel(host, UHS_REG_EXT, reg_value);

	mci_writel(host, ENABLE_SHIFT, enable_shift);

	reg_value = SDMMC_GPIO_VALUE(cclk_div, use_sam_dly, drive_sel);
	mci_writel(host, GPIO, reg_value | GPIO_CLK_ENABLE);

	dev_info(host->dev,
		"id=%d,timing=%d,UHS_REG_EXT=0x%x,ENABLE_SHIFT=0x%x,GPIO=0x%x\n",
		id, timing,
		mci_readl(host, UHS_REG_EXT),
		mci_readl(host, ENABLE_SHIFT),
		mci_readl(host, GPIO));
}

static void dw_mci_hs_set_parent(struct dw_mci *host, int timing)
{
	struct dw_mci_hs_priv_data *priv = host->priv;
	int id = priv->id;
	struct clk *clk_parent = NULL;

	if (id != 0)
		return;
#if 0
	/* select 19.2M clk */
	if (timing == MMC_TIMING_LEGACY) {
		clk_parent = clk_get_parent_by_index(host->parent_clk, 0);
		if (IS_ERR_OR_NULL(clk_parent)) {
			dev_err(host->dev, " fail to get parent clk. \n");
		}

		clk_set_parent(host->parent_clk, clk_parent);
	} else {
		clk_parent = clk_get_parent_by_index(host->parent_clk, 4);
		if (IS_ERR_OR_NULL(clk_parent)) {
			dev_err(host->dev, " fail to get parent clk. \n");
		}

		clk_set_parent(host->parent_clk, clk_parent);
	}
#endif

	clk_parent = clk_get_parent_by_index(host->parent_clk, 4);
	if (IS_ERR_OR_NULL(clk_parent)) {
		dev_err(host->dev, " fail to get parent clk. \n");
	}

	clk_set_parent(host->parent_clk, clk_parent);
}


static void dw_mci_hs_set_ios(struct dw_mci *host, struct mmc_ios *ios)
{
	struct dw_mci_hs_priv_data *priv = host->priv;
	int id = priv->id;
	int ret;

	if (priv->old_power_mode != ios->power_mode) {
		switch (ios->power_mode) {
		case MMC_POWER_OFF:
			dev_info(host->dev, "set io to lowpower\n");

			/* set pin to idle */

			if ((host->pinctrl) && (host->pins_idle)) {
				ret = pinctrl_select_state(host->pinctrl, host->pins_idle);
				if (ret)
					dev_warn(host->dev, "could not set idle pins\n");
			}

			if (host->vqmmc)
				regulator_disable(host->vqmmc);

			if (host->vmmc)
				regulator_disable(host->vmmc);

			break;
		case MMC_POWER_UP:
			dev_info(host->dev, "set io to normal\n");
			if (host->vmmc) {
				ret = regulator_set_voltage(host->vmmc, 2950000, 2950000);
				if (ret)
					dev_err(host->dev, "regulator_set_voltage failed !\n");

				ret = regulator_enable(host->vmmc);
				if (ret)
					dev_err(host->dev, "regulator_enable failed !\n");
			}

			if (host->vqmmc) {
				ret = regulator_set_voltage(host->vqmmc, 2950000, 2950000);
				if (ret)
					dev_err(host->dev, "regulator_set_voltage failed !\n");

				ret = regulator_enable(host->vqmmc);
				if (ret)
					dev_err(host->dev, "regulator_enable failed !\n");
			}

			if ((host->pinctrl) && (host->pins_default)) {
				ret = pinctrl_select_state(host->pinctrl, host->pins_default);
				if (ret)
					dev_warn(host->dev, "could not set default pins\n");
			}

			break;
		case MMC_POWER_ON:
			break;
		default:
			dev_info(host->dev, "unknown power supply mode\n");
			break;
		}
		priv->old_power_mode = ios->power_mode;
	}

	if (priv->old_timing != ios->timing) {

		dw_mci_hs_set_parent(host, ios->timing);

		ret = clk_set_rate(host->ciu_clk, hs_timing_config[id][ios->timing][0]);
		if (ret)
			dev_err(host->dev, "clk_set_rate failed\n");

		host->tuning_init_sample = (hs_timing_config[id][ios->timing][4] +
									hs_timing_config[id][ios->timing][5]) / 2;

		if (host->sd_reinit == 0)
			host->current_div = hs_timing_config[id][ios->timing][1];

		dw_mci_hs_set_timing(host, id, ios->timing, host->tuning_init_sample, host->current_div);

		if (priv->priv_bus_hz == 0)
			host->bus_hz = hs_timing_config[id][ios->timing][6];
		else
			host->bus_hz = 2 * hs_timing_config[id][ios->timing][6];

		priv->old_timing = ios->timing;
	}
}

static void dw_mci_hs_prepare_command(struct dw_mci *host, u32 *cmdr)
{
	*cmdr |= SDMMC_CMD_USE_HOLD_REG;
}

static void dw_mci_hs_tuning_clear_flags(struct dw_mci *host)
{
	host->tuning_sample_flag = 0;
}

static void dw_mci_hs_tuning_set_flags(struct dw_mci *host, int sample, int ok)
{
	if (ok)
		host->tuning_sample_flag |= (1 << sample);
	else
		host->tuning_sample_flag &= ~(1 << sample);
}

/* By tuning, find the best timing condition
 *	1 -- tuning is not finished. And this function should be called again
 *	0 -- Tuning successfully.
 *		If this function be called again, another round of tuning would be start
 *	-1 -- Tuning failed. Maybe slow down the clock and call this function again
 */
static int dw_mci_hs_tuning_find_condition(struct dw_mci *host, int timing)
{
	struct dw_mci_hs_priv_data *priv = host->priv;
	int id = priv->id;
	int sample_min, sample_max;
	int i, j;
	int ret = 0;
	int mask, mask_lenth;
	int d_value = 0;

	if (host->hw_mmc_id == DWMMC_SD_ID) {
		d_value = host->current_div - hs_timing_config[id][timing][1];
		if (timing == MMC_TIMING_SD_HS) {
			sample_max = hs_timing_config[id][timing][4] + d_value;
			sample_min = hs_timing_config[id][timing][5] + d_value;
		} else if ((timing == MMC_TIMING_UHS_SDR50) || (timing == MMC_TIMING_UHS_SDR104)) {
			sample_max = hs_timing_config[id][timing][4] + 2*d_value;
			sample_min = hs_timing_config[id][timing][5];
		} else {
			sample_max = hs_timing_config[id][timing][4];
			sample_min = hs_timing_config[id][timing][5];
		}
	} else {
		sample_max = hs_timing_config[id][timing][4];
		sample_min = hs_timing_config[id][timing][5];
	}

	if (sample_max == sample_min) {
		host->tuning_init_sample = (sample_max + sample_min) / 2;
		dw_mci_hs_set_timing(host, id, timing, host->tuning_init_sample, host->current_div);
		dev_info(host->dev, "no need tuning: timing is %d, tuning sample = %d",
			timing, host->tuning_init_sample);
		return 0;
	}

	if (-1 == host->tuning_current_sample) {

		dw_mci_hs_tuning_clear_flags(host);

		/* set the first sam del as the min_sam_del */
		host->tuning_current_sample = sample_min;
		/* a trick for next "++" */
		host->tuning_current_sample--;
	}

	if (host->tuning_current_sample >= sample_max) {
		/* tuning finish, select the best sam_del */

		/* set sam del to -1, for next tuning */
		host->tuning_current_sample = -1;

		host->tuning_init_sample = -1;
		for (mask_lenth = (((sample_max - sample_min) >> 1) << 1) + 1;
			mask_lenth >= 1; mask_lenth -= 2) {

				mask = (1 << mask_lenth) - 1;
				for (i = (sample_min + sample_max - mask_lenth + 1) / 2, j = 1;
					(i <= sample_max - mask_lenth + 1) && (i >= sample_min);
					i = ((sample_min + sample_max - mask_lenth + 1) / 2) + ((j % 2) ? -1 : 1) * (j / 2)) {
					if ((host->tuning_sample_flag & (mask << i)) == (mask << i)) {
						host->tuning_init_sample = i + mask_lenth / 2 ;
						break;
					}

					j++;
				}

				if (host->tuning_init_sample != -1) {
					if ((host->hw_mmc_id == DWMMC_SD_ID) && (mask_lenth < 3)) {
						dev_info(host->dev, "sd card tuning need slow down clk, timing is %d, tuning_flag = 0x%x \n",
							timing, host->tuning_sample_flag);
						return -1;
					} else {
						dev_info(host->dev,
							"tuning OK: timing is %d, tuning sample = %d, tuning_flag = 0x%x",
							timing, host->tuning_init_sample, host->tuning_sample_flag);
						ret = 0;
						break;
					}
				}
		}

		if (-1 == host->tuning_init_sample) {
			host->tuning_init_sample = (sample_min + sample_max) / 2;
			dev_info(host->dev,
				"tuning err: no good sam_del, timing is %d, tuning_flag = 0x%x",
				timing, host->tuning_sample_flag);
			ret = -1;
		}

		dw_mci_hs_set_timing(host, id, timing, host->tuning_init_sample, host->current_div);
		return ret;
	} else {
		host->tuning_current_sample++;
		dw_mci_hs_set_timing(host, id, timing, host->tuning_current_sample, host->current_div);
		return 1;
	}

	return 0;
}

static void dw_mci_hs_tuning_set_current_state(struct dw_mci *host, int ok)
{
	dw_mci_hs_tuning_set_flags(host, host->tuning_current_sample, ok);
}

static int dw_mci_hs_slowdown_clk(struct dw_mci *host, int timing)
{
	struct dw_mci_hs_priv_data *priv = host->priv;
	int id = priv->id;

	host->current_div ++;
	dev_info(host->dev, "begin slowdown clk, current_div=%d\n", host->current_div);
	if (host->current_div > 15) {
		host->current_div = 15;
		return -1;
	} else
		dw_mci_hs_set_timing(host, id, timing, host->tuning_init_sample, host->current_div);

	return 0;
}

static int dw_mci_hs_tuning_move(struct dw_mci *host, int timing, int start)
{
	struct dw_mci_hs_priv_data *priv = host->priv;
	int id = priv->id;
	int sample_min, sample_max;
	int loop;

	/* sd card not need move ,just slow down clk */
	if (host->hw_mmc_id == DWMMC_SD_ID)
		return 1;

	sample_max = hs_timing_config[id][timing][4];
	sample_min = hs_timing_config[id][timing][5];
	if (start) {
		host->tuning_move_count = 0;
	}

	for (loop = 0; loop < 2; loop++) {
		host->tuning_move_count++;
		host->tuning_move_sample =
			host->tuning_init_sample +
			((host->tuning_move_count % 2) ? 1 : -1) * (host->tuning_move_count / 2);

		if ((host->tuning_move_sample > sample_max) ||
			(host->tuning_move_sample < sample_min)) {
			continue;
		} else {
			break;
		}
	}

	if ((host->tuning_move_sample > sample_max) ||
		(host->tuning_move_sample < sample_min)) {
			dw_mci_hs_set_timing(host, id, timing, host->tuning_init_sample, host->current_div);
			dev_info(host->dev, "id = %d, tuning move to init del_sel %d\n",
				id, host->tuning_init_sample);
			if (dw_mci_hs_slowdown_clk(host, timing) == -1) {
				dev_info(host->dev,"tuning move and slow down clk end \n");
				return 0;
			} else {
				dev_info(host->dev,"slow down clk in tuning move process\n");
				return 1;
			}
	} else {
		dw_mci_hs_set_timing(host, id, timing, host->tuning_move_sample, host->current_div);
		dev_info(host->dev, "id = %d, tuning move to current del_sel %d\n",
			id, host->tuning_move_sample);
		return 1;
	}
}

static int dw_mci_hs_get_resource(void)
{
	struct device_node *np = NULL;

	if (pericrg_base)
		return 0;

	np = of_find_compatible_node(NULL, NULL, "hisilicon,crgctrl");
	if (!np) {
		printk("can't find crgctrl!\n");
		return -1;
	}

	pericrg_base = of_iomap(np, 0);
	if (!pericrg_base) {
		printk("crgctrl iomap error!\n");
		return -1;
	}

	return 0;
}

static int dw_mci_hs_set_controller(struct dw_mci *host, bool set)
{
	struct dw_mci_hs_priv_data *priv = host->priv;
	int id = priv->id;

	if (pericrg_base == NULL) {
		dev_err(host->dev,"pericrg_base is null, can't reset mmc! \n");
		return -1;
	}

	if (set) {
		if (0 == id) {
			writel(BIT_RST_EMMC, pericrg_base + PERI_CRG_RSTEN4);
			dev_info(host->dev,"rest emmc \n");
			goto out;
		} else if (1 == id) {
			writel(BIT_RST_SD, pericrg_base + PERI_CRG_RSTEN4);
			dev_info(host->dev,"rest sd \n");
			goto out;
		} else if (2 == id) {
			writel(BIT_RST_MMC2, pericrg_base + PERI_CRG_RSTEN4);
			dev_info(host->dev,"rest sdio \n");
			goto out;
		}
	} else {
		if (0 == id) {
			writel(BIT_RST_EMMC, pericrg_base + PERI_CRG_RSTDIS4);
			dev_info(host->dev,"unrest emmc \n");
			goto out;
		} else if (1 == id) {
			writel(BIT_RST_SD, pericrg_base + PERI_CRG_RSTDIS4);
			dev_info(host->dev,"unrest sd \n");
			goto out;
		} else if (2 == id) {
			writel(BIT_RST_MMC2, pericrg_base + PERI_CRG_RSTDIS4);
			dev_info(host->dev,"unrest sdio \n");
			goto out;
		}
	}
out:
	return 0;
}

#ifdef CONFIG_BCMDHD
extern struct dw_mci* sdio_host;

void dw_mci_sdio_card_detect(struct dw_mci *host)
{
	if(host == NULL){
		printk(KERN_ERR "sdio detect, host is null,can not used to detect sdio\n");
		return;
	}

	dw_mci_set_cd(host);

	queue_work(host->card_workqueue, &host->card_work);
	return;
};
#endif
static int dw_mci_hs_priv_init(struct dw_mci *host)
{
	struct dw_mci_hs_priv_data *priv;
	int i;

	priv = devm_kzalloc(host->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(host->dev, "mem alloc failed for private data\n");
		return -ENOMEM;
	}
	priv->id = of_alias_get_id(host->dev->of_node, "mshc");
	priv->old_timing = -1;
	host->priv = priv;
	host->hw_mmc_id = priv->id;
	host->flags &= ~DWMMC_IN_TUNING;
	host->flags &= ~DWMMC_TUNING_DONE;
#ifdef CONFIG_BCMDHD
	if(priv->id == 2)
	{
		sdio_host = host;
	}
#endif
	for (i = 0; i < ARRAY_SIZE(hs_compat); i++) {
		if (of_device_is_compatible(host->dev->of_node,
					hs_compat[i].compatible))
			priv->type = hs_compat[i].type;
	}

	return 0;
}

static int dw_mci_hs_setup_clock(struct dw_mci *host)
{
	struct dw_mci_hs_priv_data *priv = host->priv;
	int timing = MMC_TIMING_LEGACY;
	int id = priv->id;
	int ret;

	dw_mci_hs_set_parent(host, timing);

	ret = clk_set_rate(host->ciu_clk, hs_timing_config[id][timing][0]);
	if (ret)
		dev_err(host->dev, "clk_set_rate failed\n");

	dw_mci_hs_set_controller(host, 0);

	host->tuning_current_sample = -1;
	host->sd_reinit = 0;
	host->current_div = hs_timing_config[id][timing][1];

	host->tuning_init_sample = (hs_timing_config[id][timing][4] +
								hs_timing_config[id][timing][5]) / 2;

	dw_mci_hs_set_timing(host, id, timing, host->tuning_init_sample, host->current_div);

	if (priv->priv_bus_hz == 0)
		host->bus_hz = hs_timing_config[id][timing][6];
	else
		host->bus_hz = priv->priv_bus_hz;

	priv->old_timing = timing;

	return 0;
}

static int dw_mci_hs_parse_dt(struct dw_mci *host)
{
	struct dw_mci_hs_priv_data *priv = host->priv;
	struct device_node *np = host->dev->of_node;
	int value = 0;

	if (of_property_read_u32(np, "hisi,bus_hz", &value)) {
		dev_info(host->dev, "bus_hz property not found, using "
				"value of 0 as default\n");
		value = 0;
	}

	priv->priv_bus_hz = value;

	value = 0;
	if (of_property_read_u32(np, "cd-vol",&value)) {
		dev_info(host->dev, "cd-vol property not found, using "
				"value of 0 as default\n");
		value = 0;
	}
	priv->cd_vol = value;

	dev_info(host->dev,"dts bus_hz = %d \n", priv->priv_bus_hz);
	dev_info(host->dev,"dts cd-vol = %d \n", priv->cd_vol);

	return 0;
}

static irqreturn_t dw_mci_hs_card_detect(int irq, void *data)
{
	struct dw_mci *host = (struct dw_mci *)data;
	host->sd_reinit = 0;
	host->flags &= ~DWMMC_IN_TUNING;
	host->flags &= ~DWMMC_TUNING_DONE;

	queue_work(host->card_workqueue, &host->card_work);
	return IRQ_HANDLED;
};

static int dw_mci_hs_get_cd(struct dw_mci *host, u32 slot_id)
{
	unsigned int status;
	struct dw_mci_hs_priv_data *priv = host->priv;

	if(priv->cd_vol)  /* cd_vol = 1 means sdcard gpio detect pin active-high */
		status = !gpio_get_value(priv->gpio_cd);
	else  /* cd_vol = 0 means sdcard gpio detect pin active-low */
		status = gpio_get_value(priv->gpio_cd);

	dev_info(host->dev," sd status = %d\n", status);
	return status;
}

static int dw_mci_hs_cd_detect_init(struct dw_mci *host)
{
	struct device_node *np = host->dev->of_node;
	int gpio;
	int err;

	if (host->pdata->quirks & DW_MCI_QUIRK_BROKEN_CARD_DETECTION)
		return 0;

	gpio = of_get_named_gpio(np, "cd-gpio", 0);
	if (gpio_is_valid(gpio)) {
		if (devm_gpio_request(host->dev, gpio, "dw-mci-cd")) {
			dev_err(host->dev, "gpio [%d] request failed\n", gpio);
		} else {
			struct dw_mci_hs_priv_data *priv = host->priv;
			priv->gpio_cd = gpio;
			host->pdata->get_cd = dw_mci_hs_get_cd;
			err = devm_request_irq(host->dev, gpio_to_irq(gpio),
				dw_mci_hs_card_detect,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				DRIVER_NAME, host);
			if (err)
				dev_warn(mmc_dev(host->dev), "request gpio irq error\n");
		}

	} else {
		dev_info(host->dev, "cd gpio not available");
	}

	return 0;
}

static int hs_dwmmc_card_busy(struct dw_mci *host)
{
	if ((mci_readl(host, STATUS) & SDMMC_DATA_BUSY)
		|| host->cmd || host->data || host->mrq || (host->state != STATE_IDLE)) {
		dev_vdbg(host->dev, " card is busy!");
		return 1;
	}

	return 0;
}

/* Common capabilities of hi3630 SoC */
static unsigned long hs_dwmmc_caps[3] = {
	/* emmc */
	MMC_CAP_NONREMOVABLE | MMC_CAP_8_BIT_DATA | MMC_CAP_MMC_HIGHSPEED |
	MMC_CAP_ERASE | MMC_CAP_CMD23,
	/* sd */
	MMC_CAP_DRIVER_TYPE_A | MMC_CAP_4_BIT_DATA | MMC_CAP_SD_HIGHSPEED
	| MMC_CAP_MMC_HIGHSPEED  | MMC_CAP_UHS_SDR12 |
	MMC_CAP_UHS_SDR25 | MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_SDR104,
	/* sdio */
	MMC_CAP_4_BIT_DATA | MMC_CAP_SDIO_IRQ | MMC_CAP_MMC_HIGHSPEED | MMC_CAP_NONREMOVABLE,
};

static const struct dw_mci_drv_data hs_drv_data = {
	.caps			= hs_dwmmc_caps,
	.init			= dw_mci_hs_priv_init,
	.set_ios		= dw_mci_hs_set_ios,
	.setup_clock	= dw_mci_hs_setup_clock,
	.prepare_command	= dw_mci_hs_prepare_command,
	.parse_dt		= dw_mci_hs_parse_dt,
	.cd_detect_init = dw_mci_hs_cd_detect_init,
	.tuning_find_condition
						= dw_mci_hs_tuning_find_condition,
	.tuning_set_current_state
						= dw_mci_hs_tuning_set_current_state,
	.tuning_move		= dw_mci_hs_tuning_move,
	.slowdown_clk		= dw_mci_hs_slowdown_clk,
};

static const struct of_device_id dw_mci_hs_match[] = {
	{ .compatible = "hisilicon,hi3620-dw-mshc",
			.data = &hs_drv_data, },
	{ .compatible = "hisilicon,hi3630-dw-mshc",
			.data = &hs_drv_data, },
	{},
};
MODULE_DEVICE_TABLE(of, dw_mci_hs_match);

int dw_mci_hs_probe(struct platform_device *pdev)
{
	const struct dw_mci_drv_data *drv_data = NULL;
	const struct of_device_id *match;
	int err;

	match = of_match_node(dw_mci_hs_match, pdev->dev.of_node);

	if (match)
		drv_data = match->data;

	err = dw_mci_hs_get_resource();
	if (err)
		return err;

	err = dw_mci_pltfm_register(pdev, drv_data);
	if (err)
		return err;

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, 50);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, 1);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int dw_mci_hs_suspend(struct device *dev)
{
	int ret;
	struct dw_mci *host = dev_get_drvdata(dev);
	struct dw_mci_hs_priv_data *priv = host->priv;
	dev_info(host->dev, " %s ++ \n", __func__);

	pm_runtime_get_sync(dev);

	if (priv->gpio_cd) {
		disable_irq_nosync(gpio_to_irq(priv->gpio_cd));
		dev_info(host->dev, " disable gpio detect \n");
	}

	ret = dw_mci_suspend(host);
	if (ret)
		return ret;
	priv->old_timing = -1;
	priv->old_power_mode = MMC_POWER_OFF;
	if (!IS_ERR(host->biu_clk))
		clk_disable_unprepare(host->biu_clk);

	if (!IS_ERR(host->ciu_clk))
		clk_disable_unprepare(host->ciu_clk);

	dw_mci_hs_set_controller(host, 1);

	host->current_speed = 0;

	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	dev_info(host->dev, " %s -- \n", __func__);
	return 0;
}

static int dw_mci_hs_resume(struct device *dev)
{
	int ret;
	struct dw_mci *host = dev_get_drvdata(dev);
	struct dw_mci_hs_priv_data *priv = host->priv;
	dev_info(host->dev, " %s ++ \n", __func__);

	pm_runtime_get_sync(dev);
	dw_mci_hs_set_controller(host, 0);

	if (!IS_ERR(host->biu_clk)) {
		if (clk_prepare_enable(host->biu_clk))
			dev_err(host->dev, "biu_clk clk_prepare_enable failed\n");
	}

	if (!IS_ERR(host->ciu_clk)) {
		if (clk_prepare_enable(host->ciu_clk))
			dev_err(host->dev, "ciu_clk clk_prepare_enable failed\n");
	}

	host->flags &= ~DWMMC_IN_TUNING;
	host->flags &= ~DWMMC_TUNING_DONE;

	ret = dw_mci_resume(host);
	if (ret)
		return ret;
	if (priv->gpio_cd) {
		enable_irq(gpio_to_irq(priv->gpio_cd));
		dev_info(host->dev, " enable gpio detect \n");
	}

	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	dev_info(host->dev, " %s -- \n", __func__);
	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int dw_mci_hs_runtime_suspend(struct device *dev)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	dev_vdbg(host->dev, " %s ++ \n", __func__);

	if (hs_dwmmc_card_busy(host))
		return 0;

	if (!IS_ERR(host->biu_clk))
		clk_disable_unprepare(host->biu_clk);

	if (!IS_ERR(host->ciu_clk))
		clk_disable_unprepare(host->ciu_clk);

	dev_vdbg(host->dev, " %s -- \n", __func__);

	return 0;
}

static int dw_mci_hs_runtime_resume(struct device *dev)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	dev_vdbg(host->dev, " %s ++ \n", __func__);

	if (!IS_ERR(host->biu_clk)) {
		if (clk_prepare_enable(host->biu_clk))
			dev_err(host->dev, "biu_clk clk_prepare_enable failed\n");
	}

	if (!IS_ERR(host->ciu_clk)) {
		if (clk_prepare_enable(host->ciu_clk))
			dev_err(host->dev, "ciu_clk clk_prepare_enable failed\n");
	}

	dev_vdbg(host->dev, " %s -- \n", __func__);

	return 0;
}
#endif

static const struct dev_pm_ops dw_mci_hs_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(dw_mci_hs_suspend, dw_mci_hs_resume)
	SET_RUNTIME_PM_OPS(dw_mci_hs_runtime_suspend,
		dw_mci_hs_runtime_resume, NULL)
};

static struct platform_driver dw_mci_hs_pltfm_driver = {
	.probe		= dw_mci_hs_probe,
	.remove		= dw_mci_pltfm_remove,
	.driver		= {
		.name		= DRIVER_NAME,
		.of_match_table	= of_match_ptr(dw_mci_hs_match),
		.pm		= &dw_mci_hs_pmops,
	},
};

module_platform_driver(dw_mci_hs_pltfm_driver);

MODULE_DESCRIPTION("Hisilicon Specific DW-MSHC Driver Extension");
MODULE_LICENSE("GPL v2");
