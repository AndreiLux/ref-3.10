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

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include <linux/module.h>
#include <sound/soc.h>

struct MT_SOC_Routing_Private_Data {
	uint32_t mApLoopbackType;
};

static int AP_Loopback_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct MT_SOC_Routing_Private_Data *priv =
	    (struct MT_SOC_Routing_Private_Data *)snd_soc_platform_get_drvdata(platform);

	ucontrol->value.integer.value[0] = priv->mApLoopbackType;
	return 0;
}

static int AP_Loopback_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_kcontrol_chip(kcontrol);
	struct MT_SOC_Routing_Private_Data *priv =
	    (struct MT_SOC_Routing_Private_Data *)snd_soc_platform_get_drvdata(platform);

	pr_info("%s %ld\n", __func__, ucontrol->value.integer.value[0]);

	if (priv->mApLoopbackType == ucontrol->value.integer.value[0]) {
		pr_notice("%s dummy operation for %u", __func__, priv->mApLoopbackType);
		return 0;
	}

	if (priv->mApLoopbackType != AP_LOOPBACK_NONE) {
		SetI2SDacEnable(false);
		SetI2SAdcEnable(false);
		SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I03,
			      Soc_Aud_InterConnectionOutput_O03);
		SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I03,
			      Soc_Aud_InterConnectionOutput_O04);
		EnableAfe(false);
		AudDrv_Clk_Off();
	}

	if (ucontrol->value.integer.value[0] == AP_LOOPBACK_AMIC_TO_SPK ||
	    ucontrol->value.integer.value[0] == AP_LOOPBACK_AMIC_TO_HP ||
	    ucontrol->value.integer.value[0] == AP_LOOPBACK_DMIC_TO_SPK ||
	    ucontrol->value.integer.value[0] == AP_LOOPBACK_DMIC_TO_HP ||
	    ucontrol->value.integer.value[0] == AP_LOOPBACK_HEADSET_MIC_TO_SPK ||
	    ucontrol->value.integer.value[0] == AP_LOOPBACK_HEADSET_MIC_TO_HP) {
		int sample_rate = 48000;
		struct AudioDigtalI2S digital_i2s;

		if (ucontrol->value.integer.value[0] == AP_LOOPBACK_DMIC_TO_SPK ||
		    ucontrol->value.integer.value[0] == AP_LOOPBACK_DMIC_TO_HP) {
			sample_rate = 32000;
		}

		AudDrv_Clk_On();

		SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I03,
			      Soc_Aud_InterConnectionOutput_O03);
		SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I03,
			      Soc_Aud_InterConnectionOutput_O04);

		/* configure uplink */
		memset(&digital_i2s, 0, sizeof(digital_i2s));
		digital_i2s.mLR_SWAP = Soc_Aud_LR_SWAP_NO_SWAP;
		digital_i2s.mBuffer_Update_word = 8;
		digital_i2s.mFpga_bit_test = 0;
		digital_i2s.mFpga_bit = 0;
		digital_i2s.mloopback = 0;
		digital_i2s.mINV_LRCK = Soc_Aud_INV_LRCK_NO_INVERSE;
		digital_i2s.mI2S_FMT = Soc_Aud_I2S_FORMAT_I2S;
		digital_i2s.mI2S_WLEN = Soc_Aud_I2S_WLEN_WLEN_16BITS;
		digital_i2s.mI2S_SAMPLERATE = sample_rate;
		SetI2SAdcIn(&digital_i2s);

		/* configure downlink */
		SetI2SDacOut(sample_rate);

		SetI2SAdcEnable(true);
		SetI2SDacEnable(true);

		EnableAfe(true);
	}

	priv->mApLoopbackType = ucontrol->value.integer.value[0];

	return 0;
}

static const char *const AP_Loopback_function[] = {
	ENUM_TO_STR(AP_LOOPBACK_NONE),
	ENUM_TO_STR(AP_LOOPBACK_AMIC_TO_SPK),
	ENUM_TO_STR(AP_LOOPBACK_AMIC_TO_HP),
	ENUM_TO_STR(AP_LOOPBACK_DMIC_TO_SPK),
	ENUM_TO_STR(AP_LOOPBACK_DMIC_TO_HP),
	ENUM_TO_STR(AP_LOOPBACK_HEADSET_MIC_TO_SPK),
	ENUM_TO_STR(AP_LOOPBACK_HEADSET_MIC_TO_HP),
};

static const struct soc_enum Routing_Control_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(AP_Loopback_function), AP_Loopback_function),
};

static const struct snd_kcontrol_new mt_soc_routing_controls[] = {
	SOC_ENUM_EXT("AP_Loopback_Select", Routing_Control_Enum[0], AP_Loopback_Get,
		     AP_Loopback_Set),
};

static int mtk_afe_routing_platform_probe(struct snd_soc_platform *platform)
{
	struct MT_SOC_Routing_Private_Data *priv;

	pr_notice("mtk_afe_routing_platform_probe\n");

	priv = kzalloc(sizeof(struct MT_SOC_Routing_Private_Data), GFP_KERNEL);

	if (!priv) {
		pr_err("%s failed to allocate private data\n", __func__);
		return -ENOMEM;
	}

	snd_soc_platform_set_drvdata(platform, priv);

	snd_soc_add_platform_controls(platform, mt_soc_routing_controls,
				      ARRAY_SIZE(mt_soc_routing_controls));
	return 0;
}

static int mtk_routing_pcm_open(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtk_routing_pcm_close(struct snd_pcm_substream *substream)
{
	return 0;
}

static struct snd_pcm_ops mtk_afe_routing_ops = {
	.open = mtk_routing_pcm_open,
	.close = mtk_routing_pcm_close,
};

static struct snd_soc_platform_driver mtk_soc_routing_platform = {
	.ops = &mtk_afe_routing_ops,
	.probe = mtk_afe_routing_platform_probe,
};

static int mtk_afe_routing_probe(struct platform_device *pdev)
{
	pr_notice("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev, &mtk_soc_routing_platform);
}

static int mtk_afe_routing_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver mtk_afe_routing_driver = {
	.driver = {
		   .name = MT_SOC_ROUTING_PCM,
		   .owner = THIS_MODULE,
		   },
	.probe = mtk_afe_routing_probe,
	.remove = mtk_afe_routing_remove,
};

module_platform_driver(mtk_afe_routing_driver);

MODULE_DESCRIPTION("MTK AFE Routing driver");
MODULE_LICENSE("GPL");
