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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <sound/hdmi_alink.h>

#undef VIRTUAL_HDMI_AUDIO_SWITH
#ifdef VIRTUAL_HDMI_AUDIO_SWITH
#include <linux/string.h>
#include <linux/switch.h>

struct switch_dev hdmi_aud_dev = {
	.name = "hdmi_audio"
};

static ssize_t hdmi_aud_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	return sprintf(buf, "%d\n", switch_get_state(&hdmi_aud_dev));
}

static ssize_t hdmi_aud_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	unsigned long val;
	int ret;

	ret = strict_strtoul(buf, 0, &val);
	if (ret) {
		dev_err(dev, "%s is not in hex or decimal form.\n", buf);
	} else {
		if (val)
			switch_set_state(&hdmi_aud_dev, 1);
		else
			switch_set_state(&hdmi_aud_dev, 0);
	}

	return strnlen(buf, count);
}
DEVICE_ATTR(hdmi_aud, (S_IRWXUGO), hdmi_aud_show, hdmi_aud_store);
#endif

struct hdmi_asample_device {
	struct device *dev;
	struct hdmi_audio_driver asample_driver;
};

struct hdmi_asample_device *asample = NULL;

static int hdmi_asample_open(void)
{
	dev_info(asample->dev, "hdmi_asample_open\n");
	return 0;
}

static int hdmi_asample_set_params(struct hdmi_audio_data_conf *dataconf)
{
	dev_info(asample->dev, "hdmi_asample_set_params\n");
	dev_info(asample->dev, "sample_rate %d\n", dataconf->sample_rate);
	dev_info(asample->dev, "channel %d\n", dataconf->channel);
	dev_info(asample->dev, "chn_width %d\n", dataconf->chn_width);
	dev_info(asample->dev, "spk_placement_idx 0x%x\n",
						dataconf->spk_placement_idx);
	return 0;
}

static int hdmi_asample_set_fmt(unsigned int fmt)
{
	dev_info(asample->dev, "hdmi_asample_set_fmt 0x%x\n", fmt);
	return 0;
}

static int hdmi_asample_set_clock(unsigned int mclk)
{
	dev_info(asample->dev, "hdmi_asample_set_clock %d\n", mclk);
	return 0;
}

static int hdmi_asample_operation(int cmd)
{
	switch(cmd) {
	case HDMI_AUDIO_CMD_START:
		dev_info(asample->dev, "command HDMI_AUDIO_CMD_START\n");
		break;
	case HDMI_AUDIO_CMD_STOP:
		dev_info(asample->dev, "command HDMI_AUDIO_CMD_STOP\n");
		break;
	default:
		dev_err(asample->dev, "No parse cmd 0x%x\n",cmd);
		break;
	}
	return 0;
}

static int hdmi_asample_close(void)
{
	dev_info(asample->dev, "hdmi_asample_close\n");
	return 0;
}

static int hdmi_asample_hw_switch(int onoff)
{
	dev_info(asample->dev, "hdmi_asample_hw_switch %d\n", onoff);
	return 0;
}

static int hdmi_asample_get_sad_byte_cnt(void)
{
	dev_info(asample->dev, "hdmi_asample_get_sad_cnt\n");
	return 3;
}

static int hdmi_asample_get_sad_bytes(unsigned char *sadbuf, int cnt)
{
/*
 *  http://en.wikipedia.org/wiki/Extended_display_identification_data
 *
 *  SAD Byte 1 (format and number of channels):
 *     bit 7: Reserved (0)
 *     bit 6..3: Audio format code
 *       1 = Linear Pulse Code Modulation (LPCM)
 *       2 = AC-3
 *       3 = MPEG1 (Layers 1 and 2)
 *       4 = MP3
 *       5 = MPEG2
 *       6 = AAC
 *       7 = DTS
 *       8 = ATRAC
 *       0, 15: Reserved
 *       9 = One-bit audio aka SACD
 *      10 = DD+
 *      11 = DTS-HD
 *      12 = MLP/Dolby TrueHD
 *      13 = DST Audio
 *      14 = Microsoft WMA Pro
 *     bit 2..0: number of channels minus 1
 *     (i.e. 000 = 1 channel; 001 = 2 channels; 111 = 8 channels)
 *
 *  SAD Byte 2 (sampling frequencies supported):
 *     bit 7: Reserved (0)
 *     bit 6: 192kHz
 *     bit 5: 176kHz
 *     bit 4: 96kHz
 *     bit 3: 88kHz
 *     bit 2: 48kHz
 *     bit 1: 44kHz
 *     bit 0: 32kHz
 *
 *  SAD Byte 3 (bitrate):
 *    For LPCM, bits 7:3 are reserved and the remaining bits define bit depth
 *     bit 2: 24 bit
 *     bit 1: 20 bit
 *     bit 0: 16 bit
 */
	dev_info(asample->dev, "hdmi_asample_get_sad_bytes %d\n", cnt);
	sadbuf[0] = 0x0F; /* 0000_1111b Format = LPCM, MAX_CH = 8 */
	sadbuf[1] = 0x7F; /* 0111_1111b Samplerate = 32K ~ 192K */
	sadbuf[2] = 0x05; /* 0000_0101b Bits 16,24 */
	return 0;
}

static int hdmi_asample_get_get_spk_allocation(unsigned char *spk_alloc)
{
/*
 * http://en.wikipedia.org/wiki/Extended_display_identification_data
 * bit 7: Reserved (0)
 * bit 6: Rear Left Center / Rear Right Center present for 1, absent for 0
 * bit 5: Front Left Center / Front Right Center present for 1, absent for 0
 * bit 4: Rear Center present for 1, absent for 0
 * bit 3: Rear Left / Rear Right present for 1, absent for 0
 * bit 2: Front Center present for 1, absent for 0
 * bit 1: LFE present for 1, absent for 0
 * bit 0: Front Left / Front Right present for 1, absent for 0
 */
	dev_info(asample->dev, "hdmi_asample_get_get_spk_allocation\n");
	*spk_alloc = 0x7F;
	return 0;
}


static int odin_hdmi_asample_probe(struct platform_device *pdev)
{
	int ret;

	asample = devm_kzalloc(&pdev->dev, sizeof(*asample), GFP_KERNEL);

	if (!asample) {
		dev_err(&pdev->dev, "Failed to memory alloc\n");
		return -ENOMEM;
	}

	asample->dev = &pdev->dev;
	asample->asample_driver.open = hdmi_asample_open;
	asample->asample_driver.set_params = hdmi_asample_set_params;
	asample->asample_driver.set_clock = hdmi_asample_set_clock;
	asample->asample_driver.set_fmt = hdmi_asample_set_fmt;
	asample->asample_driver.operation = hdmi_asample_operation;
	asample->asample_driver.close = hdmi_asample_close;
	asample->asample_driver.hw_switch = hdmi_asample_hw_switch;
	asample->asample_driver.get_sad_byte_cnt =
					hdmi_asample_get_sad_byte_cnt;
	asample->asample_driver.get_sad_bytes = hdmi_asample_get_sad_bytes;
	asample->asample_driver.get_spk_allocation =
					hdmi_asample_get_get_spk_allocation;

	ret = register_hdmi_alink_drv(&pdev->dev, &asample->asample_driver);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register alink drv %d\n", ret);
		return -EIO;
	}

#ifdef VIRTUAL_HDMI_AUDIO_SWITH
	if (switch_dev_register(&hdmi_aud_dev)) {
		dev_warn(&pdev->dev, "Failed to register hdmi_audio switch\n");
	} else {
		if(device_create_file(&pdev->dev, &dev_attr_hdmi_aud))
			dev_warn(&pdev->dev, "Failed to create sysfs\n");
	}
#endif

	dev_info(&pdev->dev, "Probing Success!\n");
	return 0;
}

static int odin_hdmi_asample_remove(struct platform_device *pdev)
{
	int ret;

	ret = deregister_hdmi_alink_drv(&pdev->dev);
	if (ret < 0)
		return ret;

	if (asample) {
		devm_kfree(&pdev->dev, asample);
		asample = NULL;
	}

#ifdef VIRTUAL_HDMI_AUDIO_SWITH
	switch_dev_unregister(&hdmi_aud_dev);
	device_remove_file(&pdev->dev, &dev_attr_hdmi_aud);
#endif
	return 0;
}

static const struct of_device_id odin_hdmi_asample[] = {
	{ .compatible = "lge,odin-hdmi-asample", },
	{},
};

static struct platform_driver odin_hdmi_asample_driver = {
	.driver = {
		.name = "odin-hdmi-asample",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(odin_hdmi_asample),
	},
	.probe = odin_hdmi_asample_probe,
	.remove = odin_hdmi_asample_remove,
};

module_platform_driver(odin_hdmi_asample_driver);

MODULE_AUTHOR("JongHo Kim <m4jongho.kim@lgepartner.com>");
MODULE_DESCRIPTION("ODIN HDMI Audio Driver Sample");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:odin-hdmi-asample");
