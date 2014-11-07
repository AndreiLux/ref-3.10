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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/of.h>
#include <linux/pm_qos.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/dmaengine_pcm.h>
#include <linux/odin_pd.h>
#include <linux/io.h>
#include <linux/odin_lpa.h>
#include "odin_pcm.h"

#define AUD_CTL_REG_BASE	0x34660000
#define AUD_CTL_REG_SIZE	0x70
#define AUD_CRG_REG_BASE	0x34670000
#define AUD_CRG_REG_SIZE	0x104

#define AUD_DSP_MM_SRAM_BASE	0x34200000
#define AUD_DSP_MM_DRAM00	0x34210000
#define AUD_DSP_MM_DRAM01	0x34214000
#define AUD_DSP_MM_DRAM02	0x34218000
#define AUD_DSP_MM_DRAM03	0x3421C000
#define AUD_DSP_MM_DRAM10	0x34220000
#define AUD_DSP_MM_DRAM11	0x34222000
#define AUD_DSP_MM_DRAM12	0x34224000
#define AUD_DSP_MM_DRAM13	0x34226000
#define AUD_DSP_MM_IRAM0	0x34228000
#define AUD_DSP_MM_IRAM1	0x34238000
#define AUD_DSP_MM_DCACHEA	0x34240000
#define AUD_DSP_MM_DCACHEB	0x34244000
#define AUD_DSP_MM_ICACHEA	0x34248000
#define AUD_DSP_MM_ICACHEB	0x34249000

/* AUD SRAM have 64KB size */
#define AUD_DSP_MM_SRAM_SIZE_1	16384 /* 16KB. Use DMA */
#define AUD_DSP_MM_SRAM_SIZE_2	49152 /* 48KB. Use PCM */

/* AUD SRAM+DRAM+IRAM+DCACHE+ICACHE have 296KB size */
#define AUD_DSP_MM_TOTAL_SIZE	303104

#define AUD_DSP_MM_USE_SIZE	AUD_DSP_MM_TOTAL_SIZE-AUD_DSP_MM_SRAM_SIZE_1
#define ADU_PHYSICAL_RAM	AUD_DSP_MM_SRAM_BASE+AUD_DSP_MM_SRAM_SIZE_1
#define ADU_PHYSICAL_RAM_SIZE	AUD_DSP_MM_USE_SIZE

#define ARM_LPA_CLOSE	0
#define ARM_LPA_OPEN	1
#define ARM_LPA_START	2
#define ARM_LPA_PAUSE	3

struct class *odin_pcm_lpa_class;
atomic_t arm_lpa_status = ATOMIC_INIT(ARM_LPA_CLOSE);

static ssize_t odin_pcm_lpa_get_state(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	int state;
	char lpa_state[4][16] = {
		{"ARM LPA CLOSE",},
		{"ARM LPA OPEN"},
		{"ARM LPA START"},
		{"ARM LPA PAUSE"},
	};

	state = atomic_read(&arm_lpa_status);
	return sprintf(buf,"%s\n",lpa_state[state]);
}
static CLASS_ATTR(odin_pcm_lpa_status, S_IWUSR|S_IRUGO,
		odin_pcm_lpa_get_state, NULL);

static ssize_t odin_pcm_lpa_decode_stset(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned long dst = simple_strtoul(buf, NULL, 10);

	if (dst == 0) {
		if (odin_sms_lpa_pcm_decoding_end(PM_SUB_BLOCK_LPA))
			printk(KERN_DEBUG "Faile to send decode end\n");
	} else if (dst == 1) {
		if (odin_sms_lpa_pcm_decoding_start(PM_SUB_BLOCK_LPA))
			printk(KERN_DEBUG "Faile to send decode start\n");
	} else {
		printk(KERN_DEBUG "Parameter is wrong\n");
	}

	return count;
}

static CLASS_ATTR(decode_stset, S_IRUGO | S_IWUGO, NULL,
				odin_pcm_lpa_decode_stset);

static int odin_pcm_alloccate_physical_buffer(struct snd_pcm *pcm,
							size_t size, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	void __iomem *memregion = NULL;
	void __iomem *aud_crg_base = NULL;
	void __iomem *aud_ctl_base = NULL;

	if (size != ADU_PHYSICAL_RAM_SIZE) {
		dev_err(pcm->card->dev, "Failed to alloc the AUD_DSP_RAM");
		return -ENOMEM;
	}

	aud_crg_base = devm_ioremap(pcm->card->dev,
					AUD_CRG_REG_BASE, AUD_CRG_REG_SIZE);
	if (!aud_crg_base) {
		dev_err(pcm->card->dev, "audio_crg ioremap failed\n");
		return -ENOMEM;
	}
	__raw_writel(0xffffff80, aud_crg_base + 0x100);
	odin_pd_on(PD_AUD, 0);
	__raw_writel(0xffffffff, aud_crg_base + 0x100); /* SRAM Reset Release */
	devm_iounmap(pcm->card->dev, aud_crg_base);
	aud_crg_base = NULL;

	aud_ctl_base = devm_ioremap(pcm->card->dev,
					AUD_CTL_REG_BASE, AUD_CTL_REG_SIZE);
	if (!aud_ctl_base) {
		dev_err(pcm->card->dev, "audio_ctl ioremap failed\n");
		return -ENOMEM;
	}
	__raw_writel(0x1, aud_ctl_base + 0x14); /* CacheMuxControl[0] */
	__raw_writel(0x3, aud_ctl_base + 0x18); /* IRAMMuxControl[1:0] */
	__raw_writel(0xff, aud_ctl_base + 0x1C); /* DRAMMuxControl[7:0] */
	devm_iounmap(pcm->card->dev, aud_ctl_base);
	aud_ctl_base = NULL;

	memregion= request_mem_region(ADU_PHYSICAL_RAM,
				ADU_PHYSICAL_RAM_SIZE, "odin-pcm-phyram");
	if (!memregion) {
		dev_err(pcm->card->dev, "Memory region already claimed\n");
		return -ENOMEM;
	}

	buf->area = (unsigned char *)devm_ioremap(pcm->card->dev,
				ADU_PHYSICAL_RAM, ADU_PHYSICAL_RAM_SIZE);
	if (!buf->area) {
		release_mem_region(ADU_PHYSICAL_RAM, ADU_PHYSICAL_RAM_SIZE);
		dev_err(pcm->card->dev, "ADU_PHYSICAL_RAM ioremap failed\n");
		return -ENOMEM;
	}
	buf->addr = ADU_PHYSICAL_RAM;
	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->bytes = size;

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	dev_info(pcm->card->dev, "va: 0x%pK, pa: 0x%llx, size: %zu\n",
						buf->area, buf->addr, size);
#else
	dev_info(pcm->card->dev, "va: 0x%pK, pa: 0x%x, size: %zu\n",
						buf->area, buf->addr, size);
#endif
	return 0;
}

static void odin_pcm_lpa_free_physical_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;

	substream = pcm->streams[stream].substream;
	if (!substream)
		return;

	buf = &substream->dma_buffer;
	if (!buf->area)
		return;

	release_mem_region(ADU_PHYSICAL_RAM, ADU_PHYSICAL_RAM_SIZE);
	devm_ioremap_release(pcm->card->dev, buf->area);
	buf->area = NULL;
}

static const struct snd_pcm_hardware odin_pcm_lpa_s_hardware = {
	.info			= SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_RESUME |
				  SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
				  SNDRV_PCM_INFO_INTERLEAVED,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_S24_LE |
				  SNDRV_PCM_FMTBIT_S32_LE,
	.channels_min		= 1,
	.channels_max		= 2,
	.period_bytes_min	= 1024,
	.period_bytes_max	= (ADU_PHYSICAL_RAM_SIZE/2),
	.periods_min		= 2,
	.periods_max		= (ADU_PHYSICAL_RAM_SIZE/1024),
	.buffer_bytes_max	= ADU_PHYSICAL_RAM_SIZE,
};

static bool odin_pcm_lpa_dma_filter(struct dma_chan *chan, void *param)
{
	struct odin_pcm_dma_params *dma_data;
	struct odin_pcm_platform_data *pdata;
	struct device *pcm_dev;
	struct device *dma_dev;
	struct device_node *pnode = NULL;

	dma_data = (struct odin_pcm_dma_params *)param;
	if (!dma_data)
		return false;

	pcm_dev = (struct device *)dma_data->pcm_device;
	dma_dev = chan->device->dev;

	if (!pcm_dev || !dma_dev)
		return false;

	if (dma_dev->of_node && pcm_dev->of_node) {
		pnode = of_parse_phandle(pcm_dev->of_node, "dma-device", 0);
		if (!pnode) {
			dev_warn(pcm_dev, "Faile to read propery 'dma-device' "
							"in device tree\n");
			return false;
		}

		if (pnode == dma_dev->of_node &&
			chan->chan_id == dma_data->chan_num)
			return true;
		else
			return false;
	}

	pdata = (struct odin_pcm_platform_data *)pcm_dev->platform_data;

	if (pdata) {
		if ((!strcmp(dev_name(dma_dev), pdata->dma_dev_name) ||
			pdata->dma_dev == dma_dev) &&
			(chan->chan_id == dma_data->chan_num))
			return true;
	}
	return false;
}

static int odin_pcm_lpa_open(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	struct odin_pcm_dma_params *dma_data = (struct odin_pcm_dma_params *)
		snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);

	if (dma_data->transfer_devtype == ODIN_PCM_FOR_SINGLE)
		snd_soc_set_runtime_hwparams(substream,
						&odin_pcm_lpa_s_hardware);
	else
		return -EINVAL;

	ret = snd_dmaengine_pcm_open_request_chan(substream,
					odin_pcm_lpa_dma_filter, dma_data);
	if (ret)
		return ret;


	atomic_set(&arm_lpa_status, ARM_LPA_OPEN);

	return ret;
}

static int odin_pcm_lpa_close(struct snd_pcm_substream *substream)
{
	int ret;

	atomic_set(&arm_lpa_status, ARM_LPA_CLOSE);

	ret = snd_dmaengine_pcm_close_release_chan(substream);
	if (ret)
		return ret;

	return ret;
}
static int odin_pcm_lpa_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct dma_chan *chan = snd_dmaengine_pcm_get_chan(substream);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct odin_pcm_dma_params *dma_data;
	struct dma_slave_config slv_config = {0, };
	enum dma_slave_buswidth buswidth;
	int ret;

	dma_data = (struct odin_pcm_dma_params *)
			snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	if (!dma_data)
		return -EINVAL;

	if (!dma_data->addr) {
		dev_err((struct device *)dma_data->pcm_device,
						"add of dma_data invalid\n");
		return -EINVAL;
	}

	if (!substream->dma_buffer.area || !substream->dma_buffer.addr) {
		dev_err((struct device *)dma_data->pcm_device,
						"memory not allocated\n");
		return -EINVAL;
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		buswidth = DMA_SLAVE_BUSWIDTH_1_BYTE;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		buswidth = DMA_SLAVE_BUSWIDTH_2_BYTES;
		break;
	case SNDRV_PCM_FORMAT_S18_3LE:
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S32_LE:
		buswidth = DMA_SLAVE_BUSWIDTH_4_BYTES;
		break;
	default:
		return -EINVAL;
	}

	slv_config.src_addr_width = buswidth;
	slv_config.dst_addr_width = buswidth;
	slv_config.slave_id = 1;

	ret = snd_hwparams_to_dma_slave_config(substream, params, &slv_config);
	if (ret)
		return ret;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		slv_config.dst_addr     = dma_data->addr;
		slv_config.dst_maxburst = dma_data->max_burst;
	} else {
		slv_config.src_addr	  = dma_data->addr;
		slv_config.src_maxburst = dma_data->max_burst;
	}

	ret = dmaengine_slave_config(chan, &slv_config);
	if (ret)
		return ret;

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	return 0;
}

static int odin_pcm_lpa_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}

static int odin_pcm_lpa_trigger(struct snd_pcm_substream *substream, int cmd)
{
	int ret;
	ret = snd_dmaengine_pcm_trigger(substream, cmd);
	if (ret)
		return ret;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		odin_sms_lpa_dma_start(PM_SUB_BLOCK_LPA);
		atomic_set(&arm_lpa_status, ARM_LPA_START);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		atomic_set(&arm_lpa_status, ARM_LPA_OPEN);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		atomic_set(&arm_lpa_status, ARM_LPA_PAUSE);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

int odin_pcm_lpa_copy(struct snd_pcm_substream *substream, int channel,
			    snd_pcm_uframes_t pos,
			    void __user *buf, snd_pcm_uframes_t count)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	char *hwbuf = runtime->dma_area + frames_to_bytes(runtime, pos);
	if (copy_from_user(hwbuf, buf, frames_to_bytes(runtime, count)))
		return -EFAULT;

	if (ARM_LPA_START == atomic_read(&arm_lpa_status)) {
		if (odin_sms_lpa_dma_start(PM_SUB_BLOCK_LPA))
			printk(KERN_EMERG "Faile to send dma start");
	}

	return 0;
}

static struct snd_pcm_ops odin_pcm_lpa_ops = {
	.open		= odin_pcm_lpa_open,
	.close		= odin_pcm_lpa_close,
	.hw_params	= odin_pcm_lpa_hw_params,
	.hw_free	= odin_pcm_lpa_hw_free,
	.trigger	= odin_pcm_lpa_trigger,
	.pointer	= snd_dmaengine_pcm_pointer,
	.copy		= odin_pcm_lpa_copy,
};

static u64 odin_lpa_dma_mask = DMA_BIT_MASK(64);

static int odin_pcm_lpa_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	struct odin_pcm_dma_params *dma_data;
	size_t alloc_size = 0;
	int ret = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &odin_lpa_dma_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(64);

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		dma_data = (struct odin_pcm_dma_params *)
			snd_soc_dai_get_dma_data(rtd->cpu_dai,
			pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream);
		if (!dma_data)
			return -EINVAL;

		if (dma_data)
			dma_data->pcm_device = rtd->platform->dev;

		if (dma_data->transfer_devtype == ODIN_PCM_FOR_SINGLE)
			alloc_size = odin_pcm_lpa_s_hardware.buffer_bytes_max;
		else
			return -EINVAL;

		ret = odin_pcm_alloccate_physical_buffer(pcm, alloc_size,
						SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto err;
	}

	return 0;
err:
	return ret;
}

static void odin_pcm_lpa_free(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream =
			pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;

	if (buf->addr == ADU_PHYSICAL_RAM)
		odin_pcm_lpa_free_physical_buffer(pcm,
						SNDRV_PCM_STREAM_PLAYBACK);

}

static struct snd_soc_platform_driver odin_pcm_lpa_platform = {
	.ops		= &odin_pcm_lpa_ops,
	.pcm_new	= odin_pcm_lpa_new,
	.pcm_free	= odin_pcm_lpa_free,
};

static int odin_pcm_lpa_platform_probe(struct platform_device *pdev)
{
	int ret;

	dev_set_name(&pdev->dev,"%s","odin-pcm-lpa");
	ret = snd_soc_register_platform(&pdev->dev, &odin_pcm_lpa_platform);
	if (ret)
		printk(KERN_EMERG "Failed to set the soc pcm platform");
	else
		printk(KERN_DEBUG "Probing success!");
#if 0
	odin_pcm_lpa_class = class_create(THIS_MODULE, "odin_pcm_lpa");
	if (IS_ERR(odin_pcm_lpa_class)) {
		printk(KERN_CRIT "Failed to create class(odin_pcm_lpa)\n");
	}

	if (!class_create_file(odin_pcm_lpa_class,
				&class_attr_odin_pcm_lpa_status))
		printk(KERN_CRIT "failed to create file for odin_pcm_lpa_state\n");

	if (!class_create_file(odin_pcm_lpa_class, &class_attr_decode_stset))
		printk(KERN_CRIT "failed to create file for class_attr_decode_stset\n");
#endif
	return ret;
}

static int odin_pcm_lpa_platform_remove(struct platform_device *pdev)
{
#if 0
	class_remove_file(odin_pcm_lpa_class,
			&class_attr_odin_pcm_lpa_status);

	class_remove_file(odin_pcm_lpa_class,
			&class_attr_decode_stset);
#endif
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id odin_pcm_lap_match[] = {
	{ .compatible = "lge,odin-pcm-lpa", },
	{},
};

static struct platform_driver odin_pcm_lpa_driver = {
	.driver = {
		.name = "odin-pcm-lpa",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(odin_pcm_lap_match),
	},
	.probe = odin_pcm_lpa_platform_probe,
	.remove = odin_pcm_lpa_platform_remove,
};
module_platform_driver(odin_pcm_lpa_driver);
