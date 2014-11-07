/*             
                                                      
                                  
*/
/*
 * rt5671_ioctl.h  --  RT5671 ALSA SoC audio driver IO control
 *
 * Copyright 2012 Realtek Microelectronics
 * Author: Bard <bardliao@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/spi/spi.h>
#include <sound/soc.h>
#include "rt_codec_ioctl.h"
#include "rt5671_ioctl.h"
#include "rt5671.h"
#include "rt5671-dsp.h"

hweq_t hweq_param[] = {
	{/* NORMAL */
		{0},
		{0},
		0x0000,
	},
	{/* SPK */
#if 1 //                  
		{0},
		{0x1C10, 0x0000, 0x0276, 0x0868, 0x1EE5, 0xDC63, 0x1D51, 0x0F95,
		0x0161, 0xDE2E, 0x01D5, 0x95F2, 0xE6A4, 0x12B9, 0xFA19, 0xC0EA,
		0x1F2C, 0x0699, 0x0000, 0x1C10, 0x0000, 0x0436, 0x0000, 0x1D0B,
		0x02B4, 0x1D2B, 0x0800, 0x0800},
		0x000E,
#else
		{0},
		{0x1bbc, 0x0c73, 0x030b, 0xff24, 0x1ea6, 0xe4d9, 0x1c98, 0x589d,
		 0x01bd, 0xd344, 0x01ca, 0x2940, 0xd8cb, 0x1bbc, 0x0000, 0xef01,
		 0x1bbc, 0x0000, 0xef01, 0x1bbc, 0x0000, 0x0257, 0x0000, 0x1cc9,
		 0x02eb, 0x1cee, 0x0800, 0x0800},
		0x0000,
#endif
	},
	{/* HP */
#if 1 //                  
		{0},
		{0x1C10, 0x0000, 0x0276, 0x0868, 0x1EE5, 0xDC63, 0x1D51, 0x0F95,
		0x0161, 0xDE2E, 0x01D5, 0x95F2, 0xE6A4, 0x12B9, 0xFA19, 0xC0EA,
		0x1F2C, 0x0699, 0x0000, 0x1C10, 0x0000, 0x0436, 0x0000, 0x1D0B,
		0x02B4, 0x1D2B, 0x0800, 0x0800},
		0x000E,
#else
		{0},
		{0x1bbc, 0x0c73, 0x030b, 0xff24, 0x1ea6, 0xe4d9, 0x1c98, 0x589d,
		 0x01bd, 0xd344, 0x01ca, 0x2940, 0xd8cb, 0x1bbc, 0x0000, 0xef01,
		 0x1bbc, 0x0000, 0xef01, 0x1bbc, 0x0000, 0x0257, 0x0000, 0x1cc9,
		 0x02eb, 0x1cee, 0x0800, 0x0800},
		0x0000,
#endif
	},
	{/* ADC TYPE1*/
#if 1//                  
		{0},
		{0x0cf9, 0x09fd, 0xf405, 0xf596, 0xfb38, 0xf405, 0xf596, 0xfb38,
		 0xf405, 0x0436, 0x0000},
		0x001c,//mx-af
#else
		{0},
		{0x1c10, 0x01f4, 0xe904, 0x1c10, 0x01f4, 0xe904, 0x1c10, 0x01f4,
		 0xe904, 0x1c10, 0x01f4, 0xe904, 0x1c10, 0x01f4, 0x1c10, 0x01f4,
		 0x0800, 0x0800},
		0x0000,
#endif
	},
	{/* ADC TYPE2 */
		{0},
		{0x1c10, 0x01f4, 0xe904, 0x1c10, 0x01f4, 0xe904, 0x1c10, 0x01f4,
		 0xe904, 0x1c10, 0x01f4, 0xe904, 0x1c10, 0x01f4, 0x1c10, 0x01f4,
		 0x0800, 0x0800},
		0x0000,
	},
	{/* ADC TYPE3 */
		{0},
		{0x1c10, 0x01f4, 0xe904, 0x1c10, 0x01f4, 0xe904, 0x1c10, 0x01f4,
		 0xe904, 0x1c10, 0x01f4, 0xe904, 0x1c10, 0x01f4, 0x1c10, 0x01f4,
		 0x0800, 0x0800},
		0x0000,
	},
};
#define RT5671_HWEQ_LEN ARRAY_SIZE(hweq_param)

int eqreg[EQ_CH_NUM][EQ_REG_NUM] = {
	{0xa4, 0xa5, 0xeb, 0xec, 0xed, 0xee, 0xe7, 0xe8, 0xe9, 0xea, 0xe5,
	 0xe6, 0xae, 0xaf, 0xb0, 0xb4, 0xb5, 0xb6, 0xba, 0xbb, 0xbc, 0xc0,
	 0xc1, 0xc4, 0xc5, 0xc6, 0xca, 0xcc},// DAC L
	{0xa6, 0xa7, 0xf5, 0xf6, 0xf7, 0xf8, 0xf1, 0xf2, 0xf3, 0xf4, 0xef,
	 0xf0, 0xb1, 0xb2, 0xb3, 0xb7, 0xb8, 0xb9, 0xbd, 0xbe, 0xbf, 0xc2,
	 0xc3, 0xc7, 0xc8, 0xc9, 0xcb, 0xcd},// DAC R
#if 1//                  
    {0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd}, // ADC
#else
	{0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
	 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xe1, 0xe2}, // ADC
#endif
};

hwalc_t hwalc_param[] = {
	/* 0xb2, 0xb3, 0xb5, 0xb6, 0xb7, 0xb4 */
	{/* ALC_NORMAL */
		{0x0000, 0x001f, 0x1f00, 0x0000, 0x0000, 0x2206},
	},

/*
	{ RT5671_ALC_DRC_CTRL1	, 0x0000 }, //0xb2
	{ RT5671_ALC_DRC_CTRL2	, 0x001f }, //0xb3
	{ RT5671_ALC_CTRL_2	, 0x1f91 }, //0xb5
	{ RT5671_ALC_CTRL_3	, 0x4055 }, //0xb6
	{ RT5671_ALC_CTRL_4	, 0x0040 }, //0xb7
	{ RT5671_ALC_CTRL_1	, 0xe20c }, //0xb4
*/
	{/* ALC_TYPE1 */
		{0x0000, 0x001f, 0x1f91, 0x4055, 0x0040, 0xe20c},
	},
	{/* ALC_TYPE1 */
		{0x0000, 0x001f, 0x1f00, 0x0000, 0x0000, 0x2206},
	},
	{/* ALC_TYPE1 */
		{0x0000, 0x001f, 0x1f00, 0x0000, 0x0000, 0x2206},
	},
};
#define RT5671_HWALC_LEN ARRAY_SIZE(hwalc_param)

int alcreg[ALC_REG_NUM] = {0xb2, 0xb3, 0xb5, 0xb6, 0xb7, 0xb4};

int rt5671_update_eqmode(
	struct snd_soc_codec *codec, int channel, int mode)
{
	struct rt_codec_ops *ioctl_ops = rt_codec_get_ioctl_ops();
	int i, upd_reg, reg, mask;

	if (codec == NULL ||  mode >= RT5671_HWEQ_LEN)
		return -EINVAL;

	dev_dbg(codec->dev, "%s(): mode=%d\n", __func__, mode);
	if (mode != NORMAL) {
		for (i = 0; i < EQ_REG_NUM; i++) {
			hweq_param[mode].reg[i] = eqreg[channel][i];
		}

		for (i = 0; i < EQ_REG_NUM; i++) {
			if (hweq_param[mode].reg[i])
				ioctl_ops->index_write(codec, hweq_param[mode].reg[i],
						hweq_param[mode].value[i]);
			else
				break;
		}
	}
	switch (channel) {
	case EQ_CH_DACL:
		reg = RT5671_EQ_CTRL2;
		mask = 0x11fe;
		upd_reg = RT5671_EQ_CTRL1;
		break;
	case EQ_CH_DACR:
		reg = RT5671_EQ_CTRL2;
		mask = 0x22fe;
		upd_reg = RT5671_EQ_CTRL1;
		break;
	case EQ_CH_ADC:
		reg = RT5671_ADC_EQ_CTRL2;
		mask = 0x01bf;
		upd_reg = RT5671_ADC_EQ_CTRL1;
		break;
	default:
		printk(KERN_ERR "Invalid EQ channel\n");
		return -EINVAL;
	}
	snd_soc_update_bits(codec, reg, mask, hweq_param[mode].ctrl);
	snd_soc_update_bits(codec, upd_reg,
		RT5671_EQ_UPD, RT5671_EQ_UPD);
	snd_soc_update_bits(codec, upd_reg, RT5671_EQ_UPD, 0);

	return 0;
}

int rt5671_update_alcmode(struct snd_soc_codec *codec, int mode)
{
	int i;

	if (codec == NULL ||  mode >= RT5671_HWALC_LEN)
		return -EINVAL;

	dev_dbg(codec->dev, "%s(): mode=%d\n", __func__, mode);

	for (i = 0; i < ALC_REG_NUM - 1; i++)
		snd_soc_write(codec, alcreg[i], hwalc_param[mode].value[i]);

	snd_soc_write(codec, alcreg[i],
		hwalc_param[mode].value[i] | RT5671_DRC_AGC_UPD);

	return 0;
}

int rt5671_ioctl_common(struct snd_hwdep *hw, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	struct snd_soc_codec *codec = hw->private_data;
	struct rt_codec_cmd __user *_rt_codec = (struct rt_codec_cmd *)arg;
	struct rt_codec_cmd rt_codec;
	int *buf;
	static int eq_mode[EQ_CH_NUM];

	if (copy_from_user(&rt_codec, _rt_codec, sizeof(rt_codec))) {
		dev_err(codec->dev, "copy_from_user faild\n");
		return -EFAULT;
	}
	dev_dbg(codec->dev, "%s(): rt_codec.number=%d, cmd=%d\n",
			__func__, rt_codec.number, cmd);
	buf = kmalloc(sizeof(*buf) * rt_codec.number, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;
	if (copy_from_user(buf, rt_codec.buf,
		sizeof(*buf) * rt_codec.number))
		goto err;

	switch (cmd) {
	case RT_SET_CODEC_HWEQ_IOCTL:
		if (*buf >= EQ_CH_NUM)
			break;
		if (eq_mode[*buf] == *(buf + 1))
			break;
		eq_mode[*buf] = *(buf + 1);
		rt5671_update_eqmode(codec, eq_mode[*buf], *buf);
		break;

	case RT_GET_CODEC_ID:
		*buf = snd_soc_read(codec, RT5671_VENDOR_ID2);
		if (copy_to_user(rt_codec.buf, buf,
			sizeof(*buf) * rt_codec.number))
			goto err;
		break;
	case RT_READ_CODEC_DSP_IOCTL:
	case RT_WRITE_CODEC_DSP_IOCTL:
	case RT_GET_CODEC_DSP_MODE_IOCTL:
		return rt5671_dsp_ioctl_common(hw, file, cmd, arg);
		break;
	default:
		break;
	}

	kfree(buf);
	return 0;

err:
	kfree(buf);
	return -EFAULT;
}
EXPORT_SYMBOL_GPL(rt5671_ioctl_common);
/*              */
