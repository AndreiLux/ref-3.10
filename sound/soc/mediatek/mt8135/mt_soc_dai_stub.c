/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "AudDrv_Def.h"
#include <linux/module.h>
#include <sound/soc.h>

static struct snd_soc_dai_driver mtk_dai_stub_dai[] = {
	{
	 .playback = {
		      .stream_name = MT_SOC_BTSCO_DL_STREAM_NAME,
		      .rates = SNDRV_PCM_RATE_8000,
		      .formats = SNDRV_PCM_FMTBIT_S16_LE,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rate_min = 8000,
		      .rate_max = 8000,
		      },
	 .capture = {
		     .stream_name = MT_SOC_BTSCO_UL_STREAM_NAME,
		     .rates = SNDRV_PCM_RATE_8000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     .channels_min = 1,
		     .channels_max = 1,
		     .rate_min = 8000,
		     .rate_max = 8000,
		     },
	 .name = MT_SOC_BTSCO_CPU_DAI_NAME,
	 },
	{
		.capture = {
		     .stream_name = MT_SOC_DL1_AWB_STREAM_NAME,
		     .rates = SNDRV_PCM_RATE_8000_48000,
		     .formats = SNDRV_PCM_FMTBIT_S16_LE,
		     .channels_min = 1,
		     .channels_max = 2,
		},
		.name = MT_SOC_DL1_AWB_CPU_DAI_NAME,
	},
};

static const struct snd_soc_component_driver mt_dai_component = {
	.name = MT_SOC_STUB_CPU_DAI
};

static int mtk_dai_stub_dev_probe(struct platform_device *pdev)
{
	int rc = 0;

	pr_notice("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	rc = snd_soc_register_component(&pdev->dev, &mt_dai_component, mtk_dai_stub_dai,
					ARRAY_SIZE(mtk_dai_stub_dai));

	return rc;
}

static int mtk_dai_stub_dev_remove(struct platform_device *pdev)
{
	pr_notice("%s:\n", __func__);

	snd_soc_unregister_component(&pdev->dev);

	return 0;
}

static struct platform_driver mtk_dai_stub_driver = {
	.probe = mtk_dai_stub_dev_probe,
	.remove = mtk_dai_stub_dev_remove,
	.driver = {
		   .name = MT_SOC_STUB_CPU_DAI,
		   .owner = THIS_MODULE,
		   },
};

module_platform_driver(mtk_dai_stub_driver);

/* Module information */
MODULE_DESCRIPTION("MTK SOC DAI driver");
MODULE_LICENSE("GPL v2");
