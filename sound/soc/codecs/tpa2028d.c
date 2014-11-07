/* arch/arm/mach-msm/qdsp5v2/tpa2055-amp.c
 *
 * Copyright (C) 2009 LGE, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <asm/system.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <asm/ioctls.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <sound/tpa2028d.h>
#include <sound/soc.h>
#include "rt5671.h"

#define MODULE_NAME	"tpa2028d"

#define ODIN_SPK_OFF	0
#define ODIN_SPK_ON		1
#define DEBUG_AMP_CTL	1

#if DEBUG_AMP_CTL
#define D(fmt, args...) printk(fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

/* these values are tunning params */
#define AGC_COMPRESSION_RATE		0
#define AGC_OUTPUT_LIMITER_DISABLE	1
#define AGC_FIXED_GAIN				7

struct amp_data {
	struct i2c_client *client;
	struct audio_amp_platform_data *pdata;
};

static struct amp_data *amp_data = NULL;

static int amp_enable(int on_state)
{
	int err = 0;
	int aud_amp_en_gpio = amp_data->pdata->aud_amp_en_gpio;

	switch(on_state){
		case ODIN_SPK_OFF:
			gpio_set_value_cansleep(aud_amp_en_gpio, SPK_OFF);
			D(KERN_DEBUG "%s: AMP_EN is set to low\n", __func__);
			break;
		case ODIN_SPK_ON:
			gpio_set_value_cansleep(aud_amp_en_gpio, SPK_ON);
			D(KERN_DEBUG "%s: AMP_EN is set to high\n", __func__);
			break;
		case 2:
			D(KERN_DEBUG "%s: amp enable bypass(%d)\n", __func__, on_state);
			break;
		default:
			err = 1;
			break;
	}

	return err;
}


#ifdef CONFIG_OF
static struct audio_amp_platform_data audio_amp_pdata = {
	.enable = amp_enable,
	.agc_compression_rate = AGC_COMPRESSION_RATE,
	.agc_output_limiter_disable = AGC_OUTPUT_LIMITER_DISABLE,
	.agc_fixed_gain = AGC_FIXED_GAIN,
};

static struct of_device_id audio_amp_i2c_dt_ids[] = {
    {
        .compatible = "ti,tpa2028d",
        .data = &audio_amp_pdata,
    },
    { }
};

MODULE_DEVICE_TABLE(of, audio_amp_i2c_dt_ids);
#endif  //CONFIG_OF



int ReadI2C(char reg, char *ret)
{
	unsigned int err;
	unsigned char buf = reg;

	struct i2c_msg msg[2] = {
		{ amp_data->client->addr, 0, 1, &buf },
		{ amp_data->client->addr, I2C_M_RD, 1, ret}
	};
	err = i2c_transfer(amp_data->client->adapter, msg, 2);
	if (err < 0)
		D(KERN_ERR "%s():i2c read error:%x\n", __func__, reg);
	else
		D(KERN_INFO "%s():i2c read ok:%x\n", __func__, reg);
	return 0;

}

int WriteI2C(char reg, char val)
{

	int	err;
	unsigned char    buf[2];
	struct i2c_msg	msg = { amp_data->client->addr, 0, 2, buf };

	buf[0] = reg;
	buf[1] = val;
	err = i2c_transfer(amp_data->client->adapter, &msg, 1);

	if (err < 0) {
		D(KERN_ERR "%s():i2c write error:%x:%x\n", __func__, reg, val);
		return -EIO;
	} else {
		return 0;
	}
}

int tpa2028d_poweron(void)
{
	int fail = 0;
	unsigned int agc_compression_rate = amp_data->pdata->agc_compression_rate;
	unsigned int agc_output_limiter_disable = amp_data->pdata->agc_output_limiter_disable;
	unsigned int agc_fixed_gain = amp_data->pdata->agc_fixed_gain;

	agc_output_limiter_disable = (agc_output_limiter_disable<<7);

/*  fail |= WriteI2C(IC_CONTROL, 0xC2);	//Turn on */
	fail |= WriteI2C(IC_CONTROL, 0xE3); /*Tuen On*/
	fail |= WriteI2C(AGC_ATTACK_CONTROL, 0x05); /*Tuen On*/
	fail |= WriteI2C(AGC_RELEASE_CONTROL, 0x0B); /*Tuen On*/
	fail |= WriteI2C(AGC_HOLD_TIME_CONTROL, 0x00); /*Tuen On*/
	fail |= WriteI2C(AGC_FIXED_GAIN_CONTROL, agc_fixed_gain); /*Tuen On*/
	fail |= WriteI2C(AGC1_CONTROL, 0x3A|agc_output_limiter_disable); /*Tuen On*/
	fail |= WriteI2C(AGC2_CONTROL, 0xC0|agc_compression_rate); /*Tuen On*/
	fail |= WriteI2C(IC_CONTROL, 0xC3); /*Tuen On*/

	return fail;
}

int tpa2028d_powerdown(void)
{
	int fail = 0;
//                                                                                                                                                                
	return fail;
}

inline void set_amp_gain(int amp_state)
{
	int fail = 0;

	switch (amp_state) {
	case ODIN_SPK_ON:
		/* if the delay time is need for chip ready,
		 * should be added to each power or enable function.
		 */
		if(strcmp(amp_data->pdata->revision, "RevG")) {
			fail = amp_data->pdata->enable(SPK_ON);
			/*need 10 msec for chip ready*/
			msleep(10);
		}

		fail = tpa2028d_poweron();
		break;
	case ODIN_SPK_OFF:
		if(strcmp(amp_data->pdata->revision, "RevG"))
			amp_data->pdata->enable(2);

		fail = tpa2028d_powerdown();

		if(strcmp(amp_data->pdata->revision, "RevG"))
			amp_data->pdata->enable(SPK_OFF);
		break;
	default:
		D(KERN_DEBUG "Amp_state [%d] does not support \n", amp_state);
	}
}
EXPORT_SYMBOL(set_amp_gain);

#ifdef CONFIG_OF
static int tpa2028d_parse_dt(struct device *dev)
{
	int rc;
	int len;

	struct device_node *np = dev->of_node;
	const char *temp_string = np->name;
	struct audio_amp_platform_data *pdata =
			(struct audio_amp_platform_data *)dev->platform_data;

	rc = of_property_read_u32(np, "agc_compression_rate", &pdata->agc_compression_rate);
	rc = of_property_read_u32(np, "agc_output_limiter_disable", &pdata->agc_output_limiter_disable);
	rc = of_property_read_u32(np, "agc_fixed_gain", &pdata->agc_fixed_gain);
	rc = of_property_read_u32(np, "aud_amp_en_gpio", &pdata->aud_amp_en_gpio);
	rc = of_property_read_string(np, "revision", &temp_string);
	len = strlen(temp_string);
	memcpy(pdata->revision, temp_string, len);

	printk(KERN_DEBUG "agc_compression_rate : %2X\n", pdata->agc_compression_rate);
	printk(KERN_DEBUG "agc_output_limiter_disable : %2X\n", pdata->agc_output_limiter_disable);
	printk(KERN_DEBUG "agc_fixed_gain : %2X\n", pdata->agc_fixed_gain);
	printk(KERN_DEBUG "aud_amp_en_gpio : %d\n", pdata->aud_amp_en_gpio);
	printk(KERN_DEBUG "revision : %s\n", &pdata->revision);

	return rc;
}
#endif

static ssize_t
tpa2028d_comp_rate_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct audio_amp_platform_data *pdata = amp_data->pdata;
	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;
	if (val > 3 || val < 0)
		return -EINVAL;

	pdata->agc_compression_rate = val;
	return count;
}

static ssize_t
tpa2028d_comp_rate_show(struct device *dev, struct device_attribute *attr,   char *buf)
{
	struct audio_amp_platform_data *pdata = amp_data->pdata;
	char val = 0;

	ReadI2C(AGC2_CONTROL, &val);

	D("[tpa2028d_comp_rate_show] val : %x \n",val);

	return sprintf(buf, "AGC2_CONTROL : %x, pdata->agc_compression_rate : %d\n", val, pdata->agc_compression_rate);
}

static ssize_t
tpa2028d_out_lim_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct audio_amp_platform_data *pdata = amp_data->pdata;
	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;
	pdata->agc_output_limiter_disable = val&0x0001;
	return count;
}

static ssize_t
tpa2028d_out_lim_show(struct device *dev, struct device_attribute *attr,   char *buf)
{
	struct audio_amp_platform_data *pdata = amp_data->pdata;
	char val=0;

	ReadI2C(AGC1_CONTROL, &val);

	D("[tpa2028d_out_lim_show] val : %x \n",val);

	return sprintf(buf, "AGC1_CONTROL : %x, pdata->agc_output_limiter_disable : %d\n", val, pdata->agc_output_limiter_disable);
}

static ssize_t
tpa2028d_fixed_gain_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct audio_amp_platform_data *pdata = amp_data->pdata;
	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	pdata->agc_fixed_gain = val;
	return count;
}



static ssize_t
tpa2028d_fixed_gain_show(struct device *dev, struct device_attribute *attr,   char *buf)
{
	struct audio_amp_platform_data *pdata = amp_data->pdata;
	char val=0;

	ReadI2C(AGC_FIXED_GAIN_CONTROL, &val);

	D("[tpa2028d_fixed_gain_show] val : %x \n",val);

	return sprintf(buf, "fixed_gain : %x, pdata->agc_fixed_gain : %d\n", val, pdata->agc_fixed_gain);
}

static struct device_attribute tpa2028d_device_attrs[] = {
	__ATTR(comp_rate,  S_IRUGO | S_IWUSR, tpa2028d_comp_rate_show, tpa2028d_comp_rate_store),
	__ATTR(out_lim, S_IRUGO | S_IWUSR, tpa2028d_out_lim_show, tpa2028d_out_lim_store),
	__ATTR(fixed_gain, S_IRUGO | S_IWUSR, tpa2028d_fixed_gain_show, tpa2028d_fixed_gain_store),
};
static int tpa2028d_amp_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct amp_data *data;
	struct audio_amp_platform_data *pdata;
	struct i2c_adapter *adapter = client->adapter;
	int err;
	int ret, i;

	if (!i2c_check_functionality(adapter,I2C_FUNC_SMBUS_WORD_DATA)) {
		err = -EOPNOTSUPP;
		return err;
	}

#ifdef CONFIG_OF
	const struct of_device_id *of_id;
	of_id = of_match_device(audio_amp_i2c_dt_ids, &client->dev);
	client->dev.platform_data = of_id->data;
	tpa2028d_parse_dt(&client->dev);
#endif

	pdata = client->dev.platform_data;
	if (pdata == NULL) {
		D(KERN_DEBUG "%s platform data is null\n", __func__);
		return -ENODEV;
	}

	/* aud_amp_en_gpio initial setting */
	if(pdata->aud_amp_en_gpio != NULL){
		if (gpio_is_valid(pdata->aud_amp_en_gpio)){
			ret = gpio_request_one(pdata->aud_amp_en_gpio,
				GPIOF_DIR_OUT | GPIOF_INIT_LOW, "Audio Amp Enable");
			if (ret)
				return ret;
			if(!strcmp(pdata->revision, "RevG")){
				gpio_set_value(pdata->aud_amp_en_gpio, 1);
				D(KERN_INFO "amp_en_gpio initial value should be high in RevG\n");
			} else {
				gpio_set_value(pdata->aud_amp_en_gpio, 0);
			}
		}
	}

	data = kzalloc(sizeof(struct amp_data), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	amp_data = data;
	amp_data->pdata = pdata;
	data->client = client;
	i2c_set_clientdata(client, data);
	D(KERN_INFO "%s %d\n",__func__ , __LINE__);

	for (i = 0; i < ARRAY_SIZE(tpa2028d_device_attrs); i++) {
		err = device_create_file(&client->dev, &tpa2028d_device_attrs[i]);
		if (err)
			return err;
	}
	return 0;
}

static int tpa2028d_amp_remove(struct i2c_client *client)
{
	struct amp_data *data = i2c_get_clientdata(client);
	kfree(data);

	D(KERN_INFO "%s()\n", __func__);
	i2c_set_clientdata(client, NULL);
	return 0;
}


static struct i2c_device_id tpa2028d_amp_idtable[] = {
	{ "tpa2028d", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tpa2028d_amp_idtable);

static struct i2c_driver tpa2028d_amp_driver = {
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = audio_amp_i2c_dt_ids,
#endif
	},
	.probe = tpa2028d_amp_probe,
	.remove = tpa2028d_amp_remove,
	.id_table = tpa2028d_amp_idtable,
};

module_i2c_driver(tpa2028d_amp_driver);

MODULE_DESCRIPTION("TPA2028D Amp Control");
MODULE_AUTHOR("Kim EunHye <ehgrace.kim@lge.com>");
MODULE_LICENSE("GPL");
