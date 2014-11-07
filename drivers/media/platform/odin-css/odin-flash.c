/*
 * Flash driver
 *
 * Copyright (C) 2014 Mtekvision
 * Author: Eonkyong Yi <tiny@mtekvision.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#if CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif
#include <linux/leds.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include <linux/async.h>

#include "odin-css-system.h"
#include "odin-css-clk.h"
#include "odin-css.h"
#include "odin-css-utils.h"

#define FLASH_MODULE_NAME   "flash"
#define CSS_OF_FLASH_NAME   "lm3646,camera-led-flash-controller"

struct flash_platform {
	struct i2c_client *client;
	struct v4l2_subdev sd;
	struct platform_device	*pdev;

    uint32_t num_sources;
    const char *flash_trigger_name;
    struct led_trigger *flash_trigger;
	uint32_t flash_op_current; /* optimal */
    const char *torch_trigger_name;
	struct led_trigger *torch_trigger;
	uint32_t torch_max_current;
	uint32_t torch_op_current;
};

struct flash_platform *g_flash_plat;

enum flash_type {
	GPIO_FLASH = 0,
	TRIGGER_FLASH,
};

static enum flash_type flashtype;

extern int lm3646_gpio_enable(void);
extern int lm3646_gpio_disable(void);

static void  flash_set_platform(struct flash_platform *flash_plat);

void flash_set_platform(struct flash_platform *flash_plat)
{
	if (!g_flash_plat)
		g_flash_plat = flash_plat;
}

inline void *flash_get_platform(void)
{
	return g_flash_plat;
}

int flash_hw_init(void)
{
	int ret = CSS_SUCCESS;
	struct flash_platform *flash_plat = flash_get_platform();

	css_info("flash_hw_init!!\n");

	if(lm3646_gpio_enable()) {
		css_err("failed to enable gpio.\n");
		return -EFAULT;
	}
/*
	if (flash_plat->flash_trigger)
		led_trigger_event2(flash_plat->flash_trigger, 0, 0);
	if (flash_plat->torch_trigger)
		led_trigger_event2(flash_plat->torch_trigger, 0, 0);
*/

	return ret;
}

int flash_hw_deinit(void)
{
	int ret = CSS_SUCCESS;
	struct flash_platform *flash_plat = flash_get_platform();

	css_info("flash_hw_deinit!!\n");

/*	if (flash_plat->flash_trigger)
		led_trigger_event2(flash_plat->flash_trigger, 0, 0);
	if (flash_plat->torch_trigger)
		led_trigger_event2(flash_plat->torch_trigger, 0, 0);
*/
	if(lm3646_gpio_disable()) {
		css_err("failed to disable gpio.\n");
		ret = -EFAULT;
	}

	return ret;
}

int flash_control(struct css_flash_control_args *flash_args)
{
	int ret = CSS_SUCCESS;
	struct flash_platform *flash_plat = flash_get_platform();
	unsigned int i;
	uint32_t led_cur[2] = {0};
	uint32_t max_curr_l;

	css_info("flash_control %d!!\n", flash_args->mode);

	if (flash_args->mode == CSS_TORCH_ON) {
		if (flash_plat->torch_trigger) {
			for (i = 0; i < 2; i++) {
				led_cur[i] = flash_args->flash_current[i];
			}
			led_trigger_event2(flash_plat->torch_trigger,
				led_cur[0], led_cur[1]);
		}
	} else if (flash_args->mode == CSS_TORCH_OFF) {
		if (flash_plat->flash_trigger)
			led_trigger_event2(flash_plat->flash_trigger, 0, 0);
		if (flash_plat->torch_trigger)
			led_trigger_event2(flash_plat->torch_trigger, 0, 0);

	} else if(flash_args->mode == CSS_LED_HIGH) {
		if (flash_plat->torch_trigger)
			led_trigger_event2(flash_plat->torch_trigger, 0, 0);

		for (i = 0; i < 2; i++) {
			led_cur[i] = flash_args->flash_current[i];
		}
		led_trigger_event2(flash_plat->flash_trigger,
			led_cur[0], led_cur[1]);
	} else if(flash_args->mode == CSS_LED_LOW) {
		if (flash_plat->flash_trigger/*torch_trigger*/) {
			max_curr_l = flash_plat->torch_max_current;
			for(i=0; i<2; i++) {
				led_cur[i] = flash_args->flash_current[i];
			}
			/*led_trigger_event2(flash_plat->torch_trigger,
				led_cur[0], led_cur[1]);*/
			for (i = 0; i < 2; i++) {
				led_cur[i] = flash_args->flash_current[i];
			}
			led_trigger_event2(flash_plat->flash_trigger,
				led_cur[0], led_cur[1]);
		}
	} else if(flash_args->mode == CSS_LED_OFF) {
		if (flash_plat->flash_trigger)
			led_trigger_event2(flash_plat->flash_trigger, 0, 0);
		if (flash_plat->torch_trigger)
			led_trigger_event2(flash_plat->torch_trigger, 0, 0);
	}
	else { /* STROBE_ON */

	}

	return ret;
}

static int flash_open(struct inode *inode, struct file *file)
{
	int ret = CSS_SUCCESS;

	css_info("flash_open!!\n");
	/* file->private_data = ; */
	ret = flash_hw_init();

	return ret;
}

static int flash_release(struct inode *ignored, struct file *file)
{
	int ret = CSS_SUCCESS;
	css_info("flash_release!!\n");

	file->private_data = 0;
	ret = flash_hw_deinit();

	return ret;
}

static long flash_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = CSS_SUCCESS;
	struct css_flash_control_args *flash_args =
		(struct css_flash_control_args *)arg;

	css_info("flash_ioctl!!\n");

	switch (cmd) {
	case CSS_IOC_FLASH_LED_CONTROL:
		flash_control(flash_args);
		break;
	default:
		css_err("Invaild ioctl command\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct file_operations flash_fops = {
	.owner			= THIS_MODULE,
	.open			= flash_open,
	.release		= flash_release,
	.unlocked_ioctl = flash_ioctl,

};

static struct miscdevice flash_miscdevice = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = FLASH_MODULE_NAME,
	.fops  = &flash_fops,
};

static struct v4l2_subdev_core_ops flash_subdev_core_ops = {
	.ioctl = flash_ioctl,
};

static struct v4l2_subdev_ops flash_subdev_ops = {
	.core = &flash_subdev_core_ops,
};

static int flash_probe(struct platform_device *pdev)
{
	int ret = CSS_SUCCESS;
	struct flash_platform *flash_plat = NULL;

#if CONFIG_OF
	struct device_node *of_node = pdev->dev.of_node;
	struct device_node *flash_src_node = NULL;
	struct device_node *torch_src_node = NULL;
	struct led_trigger *temp = NULL;
#endif

	css_info("flash_probe!!\n");

	flash_plat = kzalloc(sizeof(struct flash_platform), GFP_KERNEL);
	if (!flash_plat) {
		css_err("flash_plat alloc fail");
		return -ENOMEM;
	}

#if CONFIG_OF
	if (!of_node) {
		css_err("of_node NULL\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(of_node, "cell-index", &pdev->id);
	if (ret < 0) {
		css_err("of_property_read_u32 failed\n");
		return -EINVAL;
	}
	css_info("pdev id %d\n", pdev->id);

	ret = of_property_read_u32(of_node, "lm3646,flash-type", &flashtype);
	if (ret < 0) {
		css_err("flash-type: read failed\n");
		return -EINVAL;
	}
	css_info("flash type %d\n", flashtype);

	css_info("%s: parse flash-source\n", __func__);
	flash_src_node = of_parse_phandle(of_node,"lm3646,flash-source", 0);
	if (!flash_src_node)
		css_err("flash_src_node(flash-source) NULL\n");

	/* Flash source */
	if(flash_src_node) {
		ret = of_property_read_string(flash_src_node,
			"lm3646,flash-trigger",
			&flash_plat->flash_trigger_name);
		if (ret < 0) {
			css_err("lm3646,flash-trigger: read failed\n");
		} else {
			css_info("flash trigger %s\n", flash_plat->flash_trigger_name);

			if (flashtype == GPIO_FLASH) {
				/* use fake current */
				flash_plat->flash_op_current = LED_FULL;
			} else {
				ret = of_property_read_u32(flash_src_node,
					"lm3646,flash-op-current",
					&flash_plat->flash_op_current);
				if (ret < 0)
					css_err("lm3646,flash-op-current read failed\n");
			}
		}

		of_node_put(flash_src_node);

		css_info("max_current = %d\n", flash_plat->flash_op_current);

		led_trigger_register_simple(flash_plat->flash_trigger_name,
			&flash_plat->flash_trigger);

		if (flashtype == GPIO_FLASH)
			if (flash_plat->flash_trigger)
				temp = flash_plat->flash_trigger;
	}

	/* Torch source */
	torch_src_node = of_parse_phandle(of_node, "lm3646,torch-source", 0);
	if (!torch_src_node)
		css_err("torch_src_node is NULL\n");

	if (torch_src_node) {
		ret = of_property_read_string(torch_src_node, "lm3646,torch-trigger",
			&flash_plat->torch_trigger_name);
		if (ret < 0) {
			css_err("lm3646,torch-trigger: read failed\n");
			goto torch_failed;
		}

		css_info("torch trigger %s\n", flash_plat->torch_trigger_name);

		if (flashtype == GPIO_FLASH) {
			/* use fake current */
			flash_plat->torch_op_current = LED_FULL;
			if (temp)
				flash_plat->torch_trigger = temp;
			else
				led_trigger_register_simple(
					flash_plat->torch_trigger_name,
					&flash_plat->torch_trigger);
		} else {
			ret = of_property_read_u32(torch_src_node,"lm3646,torch-op-current",
				&flash_plat->torch_op_current);
			if (ret < 0) {
				css_err("lm3646,torch-op-current read failed\n");
				goto torch_failed;
			}

			css_info("torch max_current %d\n", flash_plat->torch_op_current);

			led_trigger_register_simple(flash_plat->torch_trigger_name,
				&flash_plat->torch_trigger);
		}
	}
#endif

	flash_plat->pdev = pdev;

	platform_set_drvdata(pdev, flash_plat);

	ret = misc_register(&flash_miscdevice);
	if (ret < 0) {
		css_err("failed to register misc %d\n", ret);
		goto error_reg;
	}

	flash_set_platform(flash_plat);

	/* Initialize sub device */
	v4l2_subdev_init(&flash_plat->sd, &flash_subdev_ops);
	v4l2_set_subdevdata(&flash_plat->sd, flash_plat);
	snprintf(flash_plat->sd.name, ARRAY_SIZE(flash_plat->sd.name),
		"odin_flash");

	/* ret = v4l2_device_register_subdev(&css->v4l2_dev, &flash_plat->sd); */

	return CSS_SUCCESS;

error_reg:
	platform_set_drvdata(pdev, NULL);
torch_failed:
	of_node_put(torch_src_node);

	return ret;
}

static int flash_remove(struct platform_device *pdev)
{
	struct flash_platform *flash_plat = platform_get_drvdata(pdev);

	if(!flash_plat)
		return CSS_FAILURE;

	v4l2_device_unregister_subdev(&flash_plat->sd);
	kfree(flash_plat);
	flash_plat = NULL;

	platform_set_drvdata(pdev, NULL);

	return CSS_SUCCESS;
}

#ifdef CONFIG_OF
static struct of_device_id odin_flash_match[] = {
	{ .compatible = CSS_OF_FLASH_NAME},
	{},
};
#endif /* CONFIG_OF */

static struct platform_driver flash_driver = {
	.probe		= flash_probe,
	.remove 	= flash_remove,
	.driver = {
		.name	= FLASH_MODULE_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odin_flash_match),
#endif
	}
};

static void odin_async_flash_init(void *data, async_cookie_t cookie)
{
	platform_driver_register(&flash_driver);
}

static int __init odin_flash_init(void)
{
	async_schedule(odin_async_flash_init, NULL);
	return 0;
}

static void __exit odin_flash_exit(void)
{
	platform_driver_unregister(&flash_driver);
	return;
}

late_initcall(odin_flash_init);
module_exit(odin_flash_exit);

MODULE_DESCRIPTION("flash driver for ODIN CSS");
MODULE_AUTHOR("Mtekvision <http://www.mtekvision.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
