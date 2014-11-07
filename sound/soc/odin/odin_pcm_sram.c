/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2014 by LG Electronics Inc.
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

#include <asm/memory.h>

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/odin_pd.h>
#include <sound/ama.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/dmaengine_pcm.h>
#include <sound/soc.h>
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
/* #define AUD_DSP_MM_TOTAL_SIZE	65536 */
#define AUD_DSP_MM_TOTAL_SIZE	303104

#define AUD_DSP_MM_USE_SIZE	(AUD_DSP_MM_TOTAL_SIZE-AUD_DSP_MM_SRAM_SIZE_1)
#define AUD_PHYSICAL_RAM	(AUD_DSP_MM_SRAM_BASE+AUD_DSP_MM_SRAM_SIZE_1)
#define AUD_PHYSICAL_RAM_SIZE	AUD_DSP_MM_USE_SIZE

struct sram_mm_area {
	char *name;
	char *desc;
	dma_addr_t paddr;
	unsigned char *vaddr;
	size_t bytes;
};

/*
 *                  odin sram(dsp) memory map
 *
 * +----+----+--------+----------------------+-----------------+
 * |area|area|area    |area                  | area            |
 * |   0|   1|   2    |   3                  |    4            |
 * +----+----+--------+----------------------+-----------------+
 * 0x34200000 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 0x3424A000 <- address
 * 0KB ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 296KB <- size
 *
 * +-------+-------+------------+-------------+----------------+
 * |  name |  size | start addr | description | original area  |
 * +-------+-------+------------+------------------------------+
 * | area0 |  16KB | 0x34200000 | adma lli    | DSP SRAM       |
 * | area1 |  16KB | 0x34204000 | low buffer  | DSP SRAM       |
 * | area2 |  32KB | 0x34208000 | reserved1   | DSP SRAM       |
 * | area3 | 128KB | 0x34210000 | deep buffer | DSP D+IRAM     |
 * | area4 | 104KB | 0x34230000 | capture hifi| DSP IRAM+CACHE |
 * +-------+-------+------------+-------------+----------------+
 */

enum {
	SRAM_AREA0_ADAM_LLI = 0,
	SRAM_AREA1_LOW_BUFFER,
	SRAM_AREA2_RESERVED1,
	SRAM_AREA3_DEEP_BUFFER,
	SRAM_AREA4_CAPTURE_HIFI,
	SRAM_AREA_MAX,
};

static struct sram_mm_area odin_sram_memory_map[] = {
	[SRAM_AREA0_ADAM_LLI] = {
		.name = "area0",
		.desc = "adma_lli",
		.paddr = 0x34200000,
		.vaddr = NULL,
		.bytes = 16384
	},
	[SRAM_AREA1_LOW_BUFFER] = {
		.name = "area1",
		.desc = "low_buffer",
		.paddr = 0x34204000,
		.vaddr = NULL,
		.bytes = 16384
	},
	[SRAM_AREA2_RESERVED1] = {
		.name = "area2",
		.desc = "reserved1",
		.paddr = 0x34208000,
		.vaddr = NULL,
		.bytes = 32768
	},
	[SRAM_AREA3_DEEP_BUFFER] = {
		.name = "area3",
		.desc = "deep_buffer",
		.paddr = 0x34210000,
		.vaddr = NULL,
		.bytes = 131072
	},
	[SRAM_AREA4_CAPTURE_HIFI] = {
		.name = "area4",
		.desc = "capture_hifi",
		.paddr = 0x34230000,
		.vaddr = NULL,
		.bytes = 16384
	}
};

static const struct snd_pcm_hardware odin_pcm_sram_s_low_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_RESUME |
				  SNDRV_PCM_INFO_INTERLEAVED,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_S24_LE |
				  SNDRV_PCM_FMTBIT_S32_LE,
	.channels_min		= 1,
	.channels_max		= 2,
	.period_bytes_min	= 512,        /* 128 frm if 4bytes per frm */
	.period_bytes_max	= (16384/2),  /* 256 frm if 4bytes per frm */
	.periods_min		= 2,
	.periods_max		= (16384/512),
	.buffer_bytes_max	= 16384,
};

static const struct snd_pcm_hardware odin_pcm_sram_s_deep_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_RESUME |
				  SNDRV_PCM_INFO_INTERLEAVED,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_S24_LE |
				  SNDRV_PCM_FMTBIT_S32_LE,
	.channels_min		= 1,
	.channels_max		= 2,
	.period_bytes_min	= 1024,        /* 256 frm if 4bytes per frm */
	.period_bytes_max	= (131072/2),  /* 256 frm if 4bytes per frm */
	.periods_min		= 2,
	.periods_max		= (131072/1024),
	.buffer_bytes_max	= 131072,
};

static const struct snd_pcm_hardware odin_pcm_sram_s_hifi_capture_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
					SNDRV_PCM_INFO_MMAP_VALID |
					SNDRV_PCM_INFO_PAUSE |
					SNDRV_PCM_INFO_RESUME |
					SNDRV_PCM_INFO_INTERLEAVED,
	.formats		= SNDRV_PCM_FMTBIT_S16_LE |
					SNDRV_PCM_FMTBIT_S24_LE |
					SNDRV_PCM_FMTBIT_S32_LE,
	.rates 			= SNDRV_PCM_RATE_8000_192000,
	.channels_min		= 1,
	.channels_max		= 2,
	.period_bytes_min	= 512,       /* 128 frm if 4bytes per frm */
	.period_bytes_max	= (106496/2), /* 4096 frm if 4bytes per frm */
	.periods_min		= 2,
	.periods_max		= (106496/512),
	.buffer_bytes_max	= 106496,
};

static int odin_pcm_sram_hw_enable(struct device *dev, int onoff)
{
	void __iomem *aud_crg_base = NULL;
	void __iomem *aud_ctl_base = NULL;
	aud_crg_base = devm_ioremap(dev, AUD_CRG_REG_BASE, AUD_CRG_REG_SIZE);
	if (!aud_crg_base) {
		dev_err(dev, "audio_crg ioremap failed\n");
		return -ENOMEM;
	}

	if (!!onoff) {
		__raw_writel(0xffffff80, aud_crg_base + 0x100);
		odin_pd_on(PD_AUD, 1);
		/* SRAM Reset Release */
		__raw_writel(0xffffffff, aud_crg_base + 0x100);
	} else {
		odin_pd_off(PD_AUD, 1);
	}

	devm_iounmap(dev, aud_crg_base);
	aud_crg_base = NULL;

	aud_ctl_base = devm_ioremap(dev, AUD_CTL_REG_BASE, AUD_CTL_REG_SIZE);
	if (!aud_ctl_base) {
		dev_err(dev, "audio_ctl ioremap failed\n");
		return -ENOMEM;
	}

	if (!!onoff) {
		/* CacheMuxControl[0] */
		__raw_writel(0x1, aud_ctl_base + 0x14);
		/* IRAMMuxControl[1:0] */
		__raw_writel(0x3, aud_ctl_base + 0x18);
		/* DRAMMuxControl[7:0] */
		__raw_writel(0xff, aud_ctl_base + 0x1C);
	}
	devm_iounmap(dev, aud_ctl_base);
	aud_ctl_base = NULL;

	return 0;
}

static void *odin_pcm_sram_area_alloc(struct device *dev,
					struct sram_mm_area *area,
					dma_addr_t *paddr, size_t size)
{
#if 1
	void *addr = ama_alloc(size);
	if (addr == NULL) {
		dev_err(dev, "ama_alloc failed size:%d\n", size);
		return NULL;
	}

	area->vaddr = addr;
	area->paddr = ama_virt_to_phys(area->vaddr);
	*paddr = area->paddr;
	return area->vaddr;
#else
	void __iomem *memregion = NULL;

	if (!area) {
		dev_err(dev, "area array is NULL\n");
		return NULL;
	}

	if (area->vaddr) {
		dev_err(dev, "%s already allocated\n", area->desc);
		return NULL;
	}

	if (area->bytes != size ) {
		dev_err(dev, "%s size is wrong, need: %zu, capa:%zu\n",
				area->desc, size, area->bytes);
		return NULL;
	}

	memregion = request_mem_region(area->paddr, area->bytes, area->desc);
	if (!memregion) {
		dev_err(dev, "%s region already claimed\n", area->desc);
		return NULL;
	}

	area->vaddr = (unsigned char *)devm_ioremap(dev, area->paddr,
								area->bytes);
	if (!area->vaddr) {
		release_mem_region(area->paddr, area->bytes);
		dev_err(dev, "%s area ioremap failed\n", area->desc);
		return NULL;
	}

	*paddr = area->paddr;

	return area->vaddr;
#endif
}

static void odin_pcm_sram_area_free(struct device *dev,
					struct sram_mm_area *area)
{
	if (!area) {
		dev_err(dev, "area array is NULL\n");
		return NULL;
	}

	if (!area->vaddr)
		return;

#if 1
	ama_free(area->vaddr);

#else
	release_mem_region(area->paddr, area->bytes);
	devm_iounmap(dev, area->vaddr);
	area->vaddr = NULL;
#endif
	return;
}

static bool odin_pcm_sram_dma_filter(struct dma_chan *chan, void *param)
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
			dev_warn(pcm_dev,
				 "Faile to read propery dma-device in dt\n");
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

static int odin_pcm_sram_open(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	struct odin_pcm_dma_params *dma_data = (struct odin_pcm_dma_params *)
		snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);

	if (dma_data->transfer_devtype != ODIN_PCM_FOR_SINGLE)
		return -EINVAL;

	if (strstr(substream->pcm->id, "Low")) {
		snd_soc_set_runtime_hwparams(substream,
					&odin_pcm_sram_s_low_hardware);
		dev_dbg(rtd->platform->dev, "sram low playback open");
	} else if (strstr(substream->pcm->id, "Deep")) {
		snd_soc_set_runtime_hwparams(substream,
					&odin_pcm_sram_s_deep_hardware);
		dev_dbg(rtd->platform->dev, "sram deep playback open");
	} else if (strstr(substream->pcm->id, "aif1-1")) {
		snd_soc_set_runtime_hwparams(substream,
					&odin_pcm_sram_s_hifi_capture_hardware);
		dev_dbg(rtd->platform->dev, "sram deep playback open");
	} else {
		dev_err(rtd->platform->dev, "not support sram buffer");
		return -EINVAL;
	}

	return snd_dmaengine_pcm_open_request_chan(substream,
					odin_pcm_sram_dma_filter, dma_data);
}

static int odin_pcm_sram_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	dev_dbg(rtd->platform->dev, "%s\n", __func__);
	return snd_dmaengine_pcm_close_release_chan(substream);
}

static void odin_pcm_sram_hw_dump(struct snd_pcm_substream *substream)
{
	dmaengine_device_control(snd_dmaengine_pcm_get_chan(substream),
				 DMA_DEBUG, 0);
}

static int odin_pcm_sram_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct dma_chan *chan = snd_dmaengine_pcm_get_chan(substream);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct odin_pcm_dma_params *dma_data;
	struct dma_slave_config slv_config = {0, };
	enum dma_slave_buswidth buswidth;
	int ret;

	dev_dbg(rtd->platform->dev, "%s\n", __func__);

	dma_data = (struct odin_pcm_dma_params *)
			snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	if (!dma_data)
		return -EINVAL;

	if (!dma_data->addr) {
		dev_err(rtd->platform->dev, "add of dma_data invalid\n");
		return -EINVAL;
	}

	if (!substream->dma_buffer.area || !substream->dma_buffer.addr) {
		dev_err(rtd->platform->dev, "memory not allocated\n");
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

static int odin_pcm_sram_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}

static int odin_pcm_sram_mmap(struct snd_pcm_substream *substream, struct vm_area_struct *vma)
{
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	return remap_pfn_range(vma, vma->vm_start,
		       substream->dma_buffer.addr >> PAGE_SHIFT,
		       vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

static struct snd_pcm_ops odin_pcm_sram_ops = {
	.open		= odin_pcm_sram_open,
	.close		= odin_pcm_sram_close,
	.hw_params	= odin_pcm_sram_hw_params,
	.hw_free	= odin_pcm_sram_hw_free,
	.trigger	= snd_dmaengine_pcm_trigger,
	.pointer	= snd_dmaengine_pcm_pointer,
	.ack		= snd_dmaengine_pcm_dma_appl_ack,
	.hw_dump	= odin_pcm_sram_hw_dump,
	.mmap		= odin_pcm_sram_mmap,
};

static u64 odin_pcm_sram_dma_mask = DMA_BIT_MASK(64);

static int odin_pcm_sram_buffer_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_dma_buffer *buf = NULL;
	struct odin_pcm_dma_params *dma_data;
	struct sram_mm_area *sram_area = NULL;
	size_t size = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &odin_pcm_sram_dma_mask;

	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(64);

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		dma_data = (struct odin_pcm_dma_params *)
			snd_soc_dai_get_dma_data(rtd->cpu_dai,
					pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream);
	}
	else {
		dma_data = (struct odin_pcm_dma_params *)
			snd_soc_dai_get_dma_data(rtd->cpu_dai,
					pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream);
	}

	if (!dma_data)
		return -EINVAL;

	if (dma_data)
		dma_data->pcm_device = rtd->platform->dev;

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream)
		buf = &(pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream->dma_buffer);
	else
		buf = &(pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream->dma_buffer);

	if (dma_data->transfer_devtype != ODIN_PCM_FOR_SINGLE)
		return -EINVAL;

	if (strstr(pcm->id, "Low")) {
		size = odin_pcm_sram_s_low_hardware.buffer_bytes_max;
		sram_area = &(odin_sram_memory_map[SRAM_AREA1_LOW_BUFFER]);
		dev_dbg(rtd->platform->dev, "sram low playback alloc");
	} else 	if (strstr(pcm->id, "Deep")) {
		size = odin_pcm_sram_s_deep_hardware.buffer_bytes_max;
		sram_area = &(odin_sram_memory_map[SRAM_AREA3_DEEP_BUFFER]);
		dev_dbg(rtd->platform->dev, "sram deep playback alloc");
	} else 	if (strstr(pcm->id, "aif1-1")) {
		size = odin_pcm_sram_s_hifi_capture_hardware.buffer_bytes_max;
		sram_area = &(odin_sram_memory_map[SRAM_AREA4_CAPTURE_HIFI]);
		dev_dbg(rtd->platform->dev, "sram deep playback alloc");
	} else {
		dev_err(rtd->platform->dev, "not support sram buffer");
		return -EINVAL;
	}

	buf->area = odin_pcm_sram_area_alloc(rtd->platform->dev,
						sram_area, &(buf->addr), size);
	if (!buf->area) {
		dev_err(rtd->platform->dev,
			"odin_pcm_sram_area_alloc failed\n");
		return -ENOMEM;
	}

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->bytes = size;

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	dev_info(rtd->platform->dev, "va: 0x%pK, pa: 0x%llx, size: %zu\n",
						buf->area, buf->addr, size);
#else
	dev_info(rtd->platform->dev, "va: 0x%pK, pa: 0x%x, size: %zu\n",
						buf->area, buf->addr, size);
#endif
	return 0;
}

static void odin_pcm_sram_buffer_free(struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *rtd = NULL;
	struct snd_pcm_substream *substream = NULL;
	struct snd_dma_buffer *buf = NULL;
	struct sram_mm_area *sram_area = NULL;

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream)
		substream = pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	else
		substream = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
	if (!substream)
		return;

	rtd = substream->private_data;
	buf = &substream->dma_buffer;
	if (!buf->area)
		return;

	if (strstr(pcm->id, "Low")) {
		sram_area = &(odin_sram_memory_map[SRAM_AREA1_LOW_BUFFER]);
	} else if (strstr(pcm->id, "Deep")) {
		sram_area = &(odin_sram_memory_map[SRAM_AREA3_DEEP_BUFFER]);
	} else if (strstr(pcm->id, "aif1-1")) {
		sram_area = &(odin_sram_memory_map[SRAM_AREA4_CAPTURE_HIFI]);
	} else {
		return;
	}

	odin_pcm_sram_area_free(rtd->platform->dev, sram_area);
	buf->area = NULL;
}

static struct snd_soc_platform_driver odin_pcm_sram_platform = {
	.ops		= &odin_pcm_sram_ops,
	.pcm_new	= odin_pcm_sram_buffer_new,
	.pcm_free	= odin_pcm_sram_buffer_free,
};

static int odin_pcm_sram_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "%s power off\n", __func__);
	return odin_pcm_sram_hw_enable(dev, 0);
}

static int odin_pcm_sram_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s power on\n", __func__);
	return odin_pcm_sram_hw_enable(dev, 1);
}

static const struct dev_pm_ops odin_pcm_sram_pm_ops = {
	SET_RUNTIME_PM_OPS(odin_pcm_sram_runtime_suspend,
			   odin_pcm_sram_runtime_resume, NULL)
};

static int odin_pcm_sram_platform_probe(struct platform_device *pdev)
{
	int ret;

	pm_runtime_set_autosuspend_delay(&pdev->dev, 1000);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	ama_init(AUD_DSP_MM_SRAM_BASE, AUD_DSP_MM_TOTAL_SIZE);

	ret = snd_soc_register_platform(&pdev->dev, &odin_pcm_sram_platform);
	if (ret)
		dev_err(&pdev->dev, "Failed to set the soc pcm platform");
	else
		dev_dbg(&pdev->dev, "Probing success!");

	return ret;
}

static int odin_pcm_sram_platform_remove(struct platform_device *pdev)
{
	if (!pm_runtime_suspended(&pdev->dev))
		pm_runtime_put(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id odin_pcm_sram_match[] = {
	{ .compatible = "lge,odin-pcm-sram", },
	{},
};

static struct platform_driver odin_pcm_sram_driver = {
	.driver = {
		.name = "odin-pcm-sram",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(odin_pcm_sram_match),
		.pm = &odin_pcm_sram_pm_ops,
	},
	.probe = odin_pcm_sram_platform_probe,
	.remove = odin_pcm_sram_platform_remove,
};

module_platform_driver(odin_pcm_sram_driver);
MODULE_DESCRIPTION("Odin PCM(sram) ASoC driver");
MODULE_LICENSE("GPL");
