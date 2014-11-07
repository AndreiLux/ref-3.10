/*
 * ALSA SoC Odin Digital Interface dummy driver
 *
 *  This driver is used by controllers which can operate in where
 *  no codec is needed.  This file provides stub codec that can be used
 *  in these configurations. TI DaVinci Audio controller uses this driver.
 *
 * Author:      Hyunhee Jeon,  <hyunhee.jeon@lge.com>
 * Copyright (c) 2012, LGE Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>

#define DRV_NAME "odin-dif"

#define STUB_RATES	SNDRV_PCM_RATE_8000_96000
#define STUB_FORMATS	SNDRV_PCM_FMTBIT_S16_LE


static struct snd_soc_codec_driver soc_codec_odin_dif;

static struct snd_soc_dai_driver dit_stub_dai = {
	.name		= "odin-dif",
	.playback 	= {
		.stream_name	= "Playback",
		.channels_min	= 1,
		.channels_max	= 2,
		.rates		= STUB_RATES,
		.formats	= STUB_FORMATS,
	},
	.capture	= {
		.stream_name	= "Capture",
		.channels_min	= 1,
		.channels_max	= 2,
		.rates		= STUB_RATES,
		.formats	= STUB_FORMATS,
	},
};

static int odin_dif_probe(struct platform_device *pdev)
{
	return snd_soc_register_codec(&pdev->dev, &soc_codec_odin_dif,
			&dit_stub_dai, 1);
}

static int odin_dif_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver odin_dif_driver = {
	.probe		= odin_dif_probe,
	.remove		= odin_dif_remove,
	.driver		= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
	},
};

module_platform_driver(odin_dif_driver);

MODULE_AUTHOR("Hyunhee Jeon <hyunhee.jeon@lge.com>");
MODULE_DESCRIPTION("Odin dummy cpu driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);

