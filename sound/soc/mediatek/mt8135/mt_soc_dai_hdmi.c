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
#include "mt_soc_hdmi_def.h"
#include <linux/module.h>
#include <sound/soc.h>

static struct snd_soc_dai_driver mtk_hdmi_cpu_dai[] = {
	{
	 .name = MT_SOC_HDMI_CPU_DAI_NAME,
	 .playback = {
		      .stream_name = MT_SOC_HDMI_PLAYBACK_STREAM_NAME,
		      .rates = HDMI_RATES,
		      .formats = HDMI_FORMATS,
		      .channels_min = HDMI_CHANNELS_MIN,
		      .channels_max = HDMI_CHANNELS_MAX,
		      .rate_min = HDMI_RATE_MIN,
		      .rate_max = HDMI_RATE_MAX,
		      },
	 },
};

static const struct snd_soc_component_driver mtk_hdmi_dai_component = {
	.name = MT_SOC_HDMI_CPU_DAI_NAME,
};

static int mtk_hdmi_dai_probe(struct platform_device *pdev)
{
	pr_notice("%s\n", __func__);
	return snd_soc_register_component(&pdev->dev, &mtk_hdmi_dai_component, mtk_hdmi_cpu_dai,
					  ARRAY_SIZE(mtk_hdmi_cpu_dai));
}

static int mtk_hdmi_dai_remove(struct platform_device *pdev)
{
	pr_notice("%s\n", __func__);
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static struct platform_driver mtk_hdmi_dai_driver = {
	.driver = {
		   .name = MT_SOC_HDMI_CPU_DAI_NAME,
		   .owner = THIS_MODULE,
		   },
	.probe = mtk_hdmi_dai_probe,
	.remove = mtk_hdmi_dai_remove,
};

module_platform_driver(mtk_hdmi_dai_driver);

MODULE_DESCRIPTION("ASoC MTK HDMI CPU DAI driver");
MODULE_LICENSE("GPL");
