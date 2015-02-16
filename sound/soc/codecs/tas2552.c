/*
 * ALSA SoC Texas Instruments TAS2552 Mono Audio Amplifier
 *
 * Copyright (C) 2014 Texas Instruments Inc.
 *
 * Author: Dan Murphy <dmurphy@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/tas2552-plat.h>

#include "tas2552.h"

/* Remove this below to disable register debugging */
#define TAS2552_DEBUG

static struct i2c_client *tas2552_clients[2];
static struct i2c_client *tas2552_client;
static int nClients = 0;

struct tas2552_data {
	struct mutex mutex;
	struct snd_soc_codec *codec;
	struct regmap *regmap;
	unsigned char regs[TAS2552_VBAT_DATA];
	int power_gpio;
	u8 power_state:1;
};

static int tas2552_i2c_read(int reg)
{
	struct tas2552_data *data;
	int val;

	if (WARN_ON(!tas2552_client))
		return -EINVAL;
	data = i2c_get_clientdata(tas2552_client);

	/* If powered off, return the cached value */
	mutex_lock(&data->mutex);
	if (data->power_state) {
		val = i2c_smbus_read_byte_data(tas2552_client, reg);
		if (val < 0)
			dev_err(&tas2552_client->dev, "Read failed %i\n", val);
		else
			data->regs[reg] = val;
	} else {
		val = data->regs[reg];
	}

	mutex_unlock(&data->mutex);
	return val;
}

static int tas2552_i2c_write(int reg, u8 value)
{
	struct tas2552_data *data;
	int val = 0;

	if (WARN_ON(!tas2552_client))
		return -EINVAL;
	data = i2c_get_clientdata(tas2552_client);

	mutex_lock(&data->mutex);
	if (data->power_state) {
		val = i2c_smbus_write_byte_data(tas2552_client, reg, value);
		if (val < 0) {
			dev_err(&tas2552_client->dev, "Write failed %i\n", val);
			goto write_err;
		}
	}

	/* Either powered on or off, we save the context */
	data->regs[reg] = value;

write_err:
	mutex_unlock(&data->mutex);
	return val;
}

#ifdef TAS2552_DEBUG
struct tas2552_reg {
	const char *name;
	uint8_t reg;
	int writeable;
} tas2552_regs[] = {
	{ "STATUS",		TAS2552_DEVICE_STATUS, 1 },
	{ "CFG1",		TAS2552_CFG_1, 1 },
	{ "CFG2",		TAS2552_CFG_2, 1 },
	{ "CFG3",		TAS2552_CFG_3, 1 },
	{ "DOUT",		TAS2552_DOUT, 1 },
	{ "SER_CTRL_1",	TAS2552_SER_CTRL_1, 1 },
	{ "SER_CTRL_2",	TAS2552_SER_CTRL_2, 1 },
	{ "OUTPUT_DATA", TAS2552_OUTPUT_DATA, 1 },
	{ "PLL_CTRL_1",	TAS2552_PLL_CTRL_1, 1 },
	{ "PLL_CTRL_2",	TAS2552_PLL_CTRL_2, 1 },
	{ "PLL_CTRL_3",	TAS2552_PLL_CTRL_3, 1 },
	{ "BTIP", TAS2552_BTIP, 1 },
	{ "BTS_CTRL", TAS2552_BTS_CTRL, 1 },
	{ "LIMIT_LVL_CTRL",	TAS2552_LIMIT_LVL_CTRL, 1 },
	{ "LIMIT_RATE_HYS",	TAS2552_LIMIT_RATE_HYS, 1 },
	{ "IMIT_RELEASE",	TAS2552_LIMIT_RELEASE, 1 },
	{ "LIMIT_INT_COUNT", TAS2552_LIMIT_INT_COUNT, 1 },
	{ "PDM_CFG",	TAS2552_PDM_CFG, 1 },
	{ "PGA_GAIN",	TAS2552_PGA_GAIN, 1 },
	{ "EDGE_CTRL",	TAS2552_EDGE_RATE_CTRL, 1 },
	{ "BOOST_CTRL",	TAS2552_BOOST_PT_CTRL, 1 },
	{ "VER_NUM", TAS2552_VER_NUM, 1 },
	{ "VBAT_DATA", TAS2552_VBAT_DATA, 1 },
};


void tas2552_get_client(struct device *dev)
{
	tas2552_client = tas2552_clients[0];
	if (nClients >= 2)
		if (&(tas2552_clients[1]->dev) == dev)
			tas2552_client = tas2552_clients[1];
}

static ssize_t tas2552_registers_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	unsigned i, n, reg_count;
	u8 read_buf;

	tas2552_get_client(dev);

	reg_count = sizeof(tas2552_regs) / sizeof(tas2552_regs[0]);
	for (i = 0, n = 0; i < reg_count; i++) {
		read_buf = tas2552_i2c_read(tas2552_regs[i].reg);
		n += scnprintf(buf + n, PAGE_SIZE - n,
			       "%-20s = 0x%02X\n",
			       tas2552_regs[i].name,
			       read_buf);
	}

	return n;
}

static ssize_t tas2552_registers_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	unsigned i, reg_count, value;
	int error = 0;
	char name[30];

	tas2552_get_client(dev);

	if (count >= 30) {
		pr_err("%s:input too long\n", __func__);
		return -1;
	}

	if (sscanf(buf, "%s %x", name, &value) != 2) {
		pr_err("%s:unable to parse input\n", __func__);
		return -1;
	}

	reg_count = sizeof(tas2552_regs) / sizeof(tas2552_regs[0]);
	for (i = 0; i < reg_count; i++) {
		if (!strcmp(name, tas2552_regs[i].name)) {
			if (tas2552_regs[i].writeable) {
				error = tas2552_i2c_write(tas2552_regs[i].reg, value);
				if (error) {
					pr_err("%s:Failed to write %s\n",
						__func__, name);
					return -1;
				}
			} else {
				pr_err("%s:Register %s is not writeable\n",
						__func__, name);
					return -1;
			}
			return count;
		}
	}

	pr_err("%s:no such register %s\n", __func__, name);
	return -1;
}

static DEVICE_ATTR(registers, S_IWUSR | S_IRUGO,
		tas2552_registers_show, tas2552_registers_store);

static struct attribute *tas2552_attrs[] = {
	&dev_attr_registers.attr,
	NULL
};

static const struct attribute_group tas2552_attr_group = {
	.attrs = tas2552_attrs,
};
#endif

static void tas2552_sw_shutdown(int sw_shutdown)
{
	u8 cfg1_reg;

	if (WARN_ON(!tas2552_client))
		return;

	dev_dbg(&tas2552_client->dev, "%s: %d\n", 
		__func__, sw_shutdown);

	cfg1_reg = tas2552_i2c_read(TAS2552_CFG_1);
	if (sw_shutdown)
		cfg1_reg |= (sw_shutdown << 1);
	else
		cfg1_reg &= TAS2552_SWS_MASK;

	tas2552_i2c_write(TAS2552_CFG_1, cfg1_reg);
}

static void tas2552_init(struct snd_soc_codec *codec)

{
	tas2552_get_client(codec->dev);

#if 0
	/* BCLK source, MUTE, SW Shutdown */
	tas2552_i2c_write(TAS2552_CFG_1, 0x12);

	tas2552_i2c_write(TAS2552_CFG_3, 0x5E);
	/* DOUT set to logic low when not transmitting data */
	tas2552_i2c_write(TAS2552_DOUT, 0x00);

	tas2552_i2c_write(TAS2552_OUTPUT_DATA, 0xC8);

	/* BLCK source for PDM*/
	tas2552_i2c_write(TAS2552_PDM_CFG, 0x02);
	tas2552_i2c_write(TAS2552_PGA_GAIN, 0x16);

	tas2552_i2c_write(TAS2552_BOOST_PT_CTRL, 0x00);
	tas2552_i2c_write(TAS2552_LIMIT_LVL_CTRL, 0x0C);
	tas2552_i2c_write(TAS2552_LIMIT_RATE_HYS, 0x20);
	tas2552_i2c_write(TAS2552_CFG_2, 0xEA);
#else

/* Sequence for DragonBoard -> mono/stereo TAS2552
     16 bit I2S, 32 bclk/frame         */
     tas2552_i2c_write(0x01, 0x12);
     tas2552_i2c_write(0x02, 0xE2);
     tas2552_i2c_write(0x08, 0x10);
     tas2552_i2c_write(0x09, 0x00);
     tas2552_i2c_write(0x0A, 0x00);
     tas2552_i2c_write(0x02, 0xEA);

     /* Below sets TAS to 'Mix' mode */
//     rc = tas2552_write_reg(client, 0x03, 0x5D);

     /* Below code sets tas with address 0x40 as left channel and 0x41 as right channel */
//     if(client->addr == 0x40) {
       tas2552_i2c_write(0x03, 0x4D);
//     } else {
//       rc = tas2552_write_reg(client, 0x03, 0x55);
//     }

     tas2552_i2c_write(0x04, 0x00);
     tas2552_i2c_write(0x05, 0x00);
     tas2552_i2c_write(0x06, 0x00);
     tas2552_i2c_write(0x07, 0xC8);
     tas2552_i2c_write(0x12, 0x16);
     tas2552_i2c_write(0x14, 0x0F);
     tas2552_i2c_write(0x01, 0x10);
#endif
}

static int tas2552_power(struct snd_soc_codec *codec, u8 power)
{
	struct tas2552_data *data;
	int	ret = 0;


	dev_dbg(codec->dev, "%s: %d\n", __func__, power);

	tas2552_get_client(codec->dev);

	BUG_ON(tas2552_client == NULL);
	data = i2c_get_clientdata(tas2552_client);

	mutex_lock(&data->mutex);
//	if (power == data->power_state)
//		goto exit;

	if (power) {
		if (data->power_gpio >= 0)
			gpio_set_value(data->power_gpio, 1);

		data->power_state = 1;
	} else {
//		if (data->power_gpio >= 0)
//			gpio_set_value(data->power_gpio, 0);

//		data->power_state = 0;
	}

//exit:
	mutex_unlock(&data->mutex);
	return ret;
}

static int tas2552_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	u8 wclk_reg;
	u8 cfg3_reg;
	int rate;

	rate = params_rate(params);

	dev_dbg(dai->dev, "%s: %d\n", __func__, rate);

	/* Setting DAC clock dividers based on substream sample rate. */
	switch (params_rate(params)) {
	case 8000:
		wclk_reg = TAS2552_8KHZ;
		break;
	case 11025:
		wclk_reg = TAS2552_11_12KHZ;
		break;
	case 16000:
		wclk_reg = TAS2552_16KHZ;
		break;
	case 32000:
		wclk_reg = TAS2552_32KHZ;
		break;
	case 22050:
	case 24000:
		wclk_reg = TAS2552_22_24KHZ;
		break;
	case 44100:
	case 48000:
		wclk_reg = TAS2552_44_48KHZ;
		break;
	case 96000:
		wclk_reg = TAS2552_88_96KHZ;
		break;
	default:
		return -EINVAL;
	}

	cfg3_reg = tas2552_i2c_read(TAS2552_CFG_3);
	cfg3_reg &= ~TAS2552_WCLK_MASK;
	cfg3_reg |= wclk_reg;

	tas2552_i2c_write(TAS2552_CFG_3, cfg3_reg);

	return 0;
}

static int tas2552_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	u8 serial_format;
	u8 serial_1_reg;

	dev_dbg(codec_dai->dev, "%s: fmt - 0x%4x\n", __func__, fmt);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		serial_format = 0x00;
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		serial_format = TAS2552_WORD_CLK_MASK;
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		serial_format = TAS2552_BIT_CLK_MASK;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		serial_format = (TAS2552_BIT_CLK_MASK | TAS2552_WORD_CLK_MASK);
		break;
	default:
		return -EINVAL;
	}

	serial_1_reg = tas2552_i2c_read(TAS2552_SER_CTRL_1);
	serial_1_reg &= ~(TAS2552_BIT_CLK_MASK | TAS2552_WORD_CLK_MASK);
	serial_1_reg |= serial_format;

	tas2552_i2c_write(TAS2552_SER_CTRL_1, serial_1_reg);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		serial_format = 0x0;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		serial_format = TAS2552_DAIFMT_DSP;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		serial_format = TAS2552_DAIFMT_RIGHT_J;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		serial_format = TAS2552_DAIFMT_LEFT_J;
		break;

	default:
		return -EINVAL;
	}

	serial_1_reg = tas2552_i2c_read(TAS2552_SER_CTRL_1);
	serial_1_reg &= ~TAS2552_DATA_FORMAT_MASK;
	serial_1_reg |= serial_format;

	tas2552_i2c_write(TAS2552_SER_CTRL_1, serial_1_reg);

	return 0;
}

static int tas2552_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
				  unsigned int freq, int dir)
{
	u8 cfg2_reg;

	switch (freq) {
	case 12288000:
	case 26000000:
	case 19200000:
	case 24576000:
		break;
	case 48000:
	case 32576:
		break;
	default:
		break;
	}

	dev_dbg(dai->dev, "%s:\n", __func__);

	tas2552_sw_shutdown(1);
	cfg2_reg = tas2552_i2c_read(TAS2552_CFG_2);
	tas2552_i2c_write(TAS2552_CFG_2, cfg2_reg & ~TAS2552_PLL_ENABLE);

	tas2552_i2c_write(TAS2552_PLL_CTRL_1, 0x10);
	tas2552_i2c_write(TAS2552_PLL_CTRL_2, 0x00);
	tas2552_i2c_write(TAS2552_PLL_CTRL_3, 0x00);

	cfg2_reg = tas2552_i2c_read(TAS2552_CFG_2);
	tas2552_i2c_write(TAS2552_CFG_2, cfg2_reg | TAS2552_PLL_ENABLE);

	tas2552_sw_shutdown(0);

	return 0;
}

static int tas2552_mute(struct snd_soc_dai *dai, int mute)
{
	u8 cfg1_reg;

	dev_dbg(dai->dev, "%s: %d\n", __func__, mute);

	cfg1_reg = tas2552_i2c_read(TAS2552_CFG_1);
	if (mute)
		cfg1_reg |= (mute << 2);
	else
		cfg1_reg &= TAS2552_MUTE_MASK;

	tas2552_i2c_write(TAS2552_CFG_1, cfg1_reg);

	return 0;
}

static int tas2552_startup(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	u8 cfg2_reg;

	dev_dbg(dai->dev, "%s:\n", __func__);

	tas2552_get_client(codec->dev);

	tas2552_sw_shutdown(1);
	tas2552_power(codec, 1);
	/* Turn on Class D amplifier */

	cfg2_reg = tas2552_i2c_read(TAS2552_CFG_2);
	cfg2_reg |= 0x80;

	tas2552_i2c_write(TAS2552_CFG_2, cfg2_reg);
	tas2552_sw_shutdown(0);

	return 0;
}

static void tas2552_shutdown(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(dai->dev, "%s:\n", __func__);

	tas2552_get_client(codec->dev);

	tas2552_sw_shutdown(1);
	tas2552_power(codec, 0);
}

static struct snd_soc_dai_ops tas2552_speaker_dai_ops = {
	.hw_params	= tas2552_hw_params,
	.set_sysclk	= tas2552_set_dai_sysclk,
	.set_fmt	= tas2552_set_dai_fmt,
	.startup	= tas2552_startup,
	.shutdown	= tas2552_shutdown,
	.digital_mute = tas2552_mute,
};

/* Formats supported by TAS2552 driver. */
#define TAS2552_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
			 SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

/* TAS2552 dai structure. */
static struct snd_soc_dai_driver tas2552_dai[] = {
	{
		.name = "tas2552-amplifier",
		.playback = {
			.stream_name = "Speaker",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S32_LE,
		},
		.capture = {
			.stream_name = "Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_48000,
			.formats = TAS2552_FORMATS,
		},
		.ops = &tas2552_speaker_dai_ops,
	},
};

/*
 * DAC digital volumes. From -7 to 24 dB in 1 dB steps
 */
static DECLARE_TLV_DB_SCALE(dac_tlv, -7, 100, 24);

static const struct snd_kcontrol_new tas2552_snd_controls[] = {
	SOC_SINGLE_TLV("Speaker Driver Playback Volume",
			 TAS2552_PGA_GAIN, 0, 0x1f, 1, dac_tlv),
};

static int tas2552_codec_probe(struct snd_soc_codec *codec)
{
	struct tas2552_data *tas2552 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s:\n", __func__);

	tas2552_get_client(codec->dev);

	tas2552->codec = codec;
	tas2552_power(codec, 1);
	tas2552_init(codec);

	return 0;
}


static int tas2552_codec_remove(struct snd_soc_codec *codec)
{
	tas2552_get_client(codec->dev);

	tas2552_power(codec, 0);

	return 0;
};

static int tas2552_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	int ret;

	BUG_ON(reg > TAS2552_MAX_REG);

	ret = tas2552_i2c_write(reg, (u8)value);

	return ret;

}

static unsigned int tas2552_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	int val;

	BUG_ON(reg > TAS2552_MAX_REG);

	val = tas2552_i2c_read(reg);

	return val;
}

static struct snd_soc_codec_driver soc_codec_dev_tas2552 = {
	.probe = tas2552_codec_probe,
	.remove = tas2552_codec_remove,
	.read = tas2552_read,
	.write = tas2552_write,
	.controls = tas2552_snd_controls,
	.num_controls = ARRAY_SIZE(tas2552_snd_controls),
};

static const struct regmap_config tas2552_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = TAS2552_MAX_REG,
	.reg_defaults = NULL,
	.num_reg_defaults = 0,
	.cache_type = REGCACHE_RBTREE,
};

static int tas2552_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct device *dev;
	struct tas2552_data *data;
	struct tas2552_platform_data *pdata = client->dev.platform_data;
	struct device_node *np = client->dev.of_node;
	int ret;

	dev_dbg(&client->dev, "%s:\n", __func__);

	if (nClients >= 2) return -1;

	dev = &client->dev;
	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		dev_err(dev, "Can not allocate memory\n");
		return -ENOMEM;
	}

	if (pdata) {
		data->power_gpio = pdata->power_gpio;
	} else if (np) {
		data->power_gpio = of_get_named_gpio(np, "ti,enable-gpio", 0);
	} else {
		dev_err(dev, "Platform data not set\n");
		return -ENODEV;
	}
	data->regmap = devm_regmap_init_i2c(client, &tas2552_regmap_config);
	if (IS_ERR(data->regmap)) {
		ret = PTR_ERR(data->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}
	
	tas2552_clients[nClients++] = client;
	tas2552_client = client;
	i2c_set_clientdata(tas2552_client, data);

	mutex_init(&data->mutex);

	if (data->power_gpio >= 0) {
		ret = devm_gpio_request(dev, data->power_gpio, "tas2552 enable");
		if (ret < 0) {
			dev_err(dev, "Failed to request power GPIO (%d)\n",
				data->power_gpio);
			goto err_gpio;
		}
		gpio_direction_output(data->power_gpio, 0);
	}

	ret = snd_soc_register_codec(&client->dev,
				      &soc_codec_dev_tas2552,
				      tas2552_dai, ARRAY_SIZE(tas2552_dai));
	if (ret < 0)
		dev_err(&client->dev, "Failed to register codec: %d\n", ret);

#ifdef TAS2552_DEBUG
	ret = sysfs_create_group(&client->dev.kobj, &tas2552_attr_group);
	if (ret) {
		pr_err("%s:Cannot create sysfs group\n", __func__);
		goto err_sysfs;
	}
#endif
	return 0;
#ifdef TAS2552_DEBUG
err_sysfs:
	snd_soc_unregister_codec(&client->dev);
#endif
err_gpio:
	tas2552_client = NULL;
	return ret;
}

static int tas2552_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static const struct i2c_device_id tas2552_id[] = {
	{ "tas2552-codec", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tas2552_id);

static const struct of_device_id tas2552_of_match[] = {
	{ .compatible = "ti,tas2552", },
	{},
};
MODULE_DEVICE_TABLE(of, tas2552_of_match);

static struct i2c_driver tas2552_i2c_driver = {
	.driver = {
		.name = "tas2552-codec",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tas2552_of_match),
	},
	.probe = tas2552_probe,
	.remove = tas2552_i2c_remove,
	.id_table = tas2552_id,
};

module_i2c_driver(tas2552_i2c_driver);

MODULE_AUTHOR("Dan Muprhy <dmurphy@ti.com>");
MODULE_DESCRIPTION("TAS2552 Audio amplifier driver");
MODULE_LICENSE("GPL");
