 /*
  * LG ELECTRONICS INC., SEOUL, KOREA
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


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/platform_data/gpio-odin.h>
#include <linux/of_device.h>
#include <linux/usb_switch.h>
#ifdef USB_SWITCH_NO_VBUS_SWITCH
#include <linux/err.h>
#include <linux/usb/odin_usb3.h>
#endif

struct usb_switch_data {
	int usb_state;
	unsigned usb_sw_sel_gpio;
	struct device *dev;
	struct usb_switch_dev sdev;
	struct mutex mutex;
#ifdef USB_SWITCH_NO_VBUS_SWITCH
	struct odin_usb3_otg *odin_otg;
#endif
};

static void usb_switch_set_sw_sel_gpio(struct usb_switch_data *switch_data, int usb_switch)
{
	if(usb_switch == CP_USB_ON) {
		gpio_set_value(switch_data->usb_sw_sel_gpio, CP_USB_ON);
#ifdef USB_SWITCH_NO_VBUS_SWITCH
		if (!IS_ERR_OR_NULL(switch_data->odin_otg)) {
			odin_otg_vbus_event(switch_data->odin_otg, 0, 0);
		}
#endif
	}
	else { //AP_USB_ON
		gpio_set_value(switch_data->usb_sw_sel_gpio, AP_USB_ON);
#ifdef USB_SWITCH_NO_VBUS_SWITCH
		if (!IS_ERR_OR_NULL(switch_data->odin_otg)) {
			odin_otg_vbus_event(switch_data->odin_otg, 1, 350);
		}
#endif
	}
}

static int usb_switch_get_state(struct usb_switch_dev *sdev)
{
	struct usb_switch_data *switch_data =
		container_of(sdev, struct usb_switch_data, sdev);

	struct device *dev = switch_data->dev;
	ssize_t state;

	mutex_lock(&switch_data->mutex);

	dev_info(dev, "usb_switch_get_state %d\n", switch_data->usb_state);

	if(switch_data->usb_state > CP_USB_ON_RETAIN)
		return AP_USB_ON;
	else
		return switch_data->usb_state;

	mutex_unlock(&switch_data->mutex);

	return state;
}

#ifdef CONFIG_IMC_MODEM
extern void baseband_xmm_power_reset_on(void);
#endif

static int usb_switch_set_state(struct usb_switch_dev *sdev, int usb_switch)
{
	struct usb_switch_data *switch_data =
		container_of(sdev, struct usb_switch_data, sdev);
	struct device *dev = switch_data->dev;

	mutex_lock(&switch_data->mutex);

	dev_info(dev, "usb_switch_set_state: usb_state(%d) usb_switch(%d)\n",
		switch_data->usb_state, usb_switch);

	switch(usb_switch) {
		case CP_USB_ON :
		case CP_USB_ON_RETAIN :
		{
			if(switch_data->usb_state == AP_USB_ON) {
				usb_switch_set_sw_sel_gpio(switch_data, CP_USB_ON);
				switch_data->usb_state = usb_switch;
			}
			else if(usb_switch == CP_USB_ON_DOWNLOAD) {
				dev_info(dev, "Do nothing in Modem download mode\n");
			}
			else {
				switch_data->usb_state = usb_switch;
				dev_info(dev, "Do not need to switch CP USB\n");
			}
			break;
		}

		case CP_USB_ON_DOWNLOAD :
		{
			/* Switch to Modem USB */
			usb_switch_set_sw_sel_gpio(switch_data, CP_USB_ON);
			switch_data->usb_state = CP_USB_ON_DOWNLOAD;
			dev_info(dev, "Switch CP USB to download\n");

			/* Reset Modem */
			/* baseband_xmm_power_reset_on() */
#ifdef CONFIG_IMC_MODEM
			baseband_xmm_power_reset_on();
#endif
			dev_info(dev, "Reset Modem to download\n");
			break;
		}

		case AP_USB_ON :
		default :
		{
			if((switch_data->usb_state == CP_USB_ON) ||
				(switch_data->usb_state == CP_USB_ON_RETAIN) ) {
				usb_switch_set_sw_sel_gpio(switch_data, AP_USB_ON);
				switch_data->usb_state = AP_USB_ON;
			}
			else if(usb_switch == CP_USB_ON_DOWNLOAD) {
				dev_info(dev, "Do not switch AP USB in Modem download mode\n");
			}
			else {
				dev_info(dev, "Do not need to switch AP USB\n");
			}
			break;
		}
	}

	mutex_unlock(&switch_data->mutex);

	return 0;
}

bool cp_usb_retain = false;

int __init lge_cp_usb_retain_mode(char *s)
{
	if (!strcmp(s, "true")) {
		cp_usb_retain = true;
	}

	return 1;
}
__setup("lge.cpusb.retain=", lge_cp_usb_retain_mode);

static int usb_switch_usb_sw_sel_gpio_init(struct platform_device *pdev,
	struct usb_switch_data *switch_data)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	int usb_switch = AP_USB_ON;

	if(of_property_read_u32(pdev->dev.of_node,
			"usb_sw_sel", &(switch_data->usb_sw_sel_gpio))) {
		dev_err(dev, "Not found :usb_sw_sel\n");
		return -EINVAL;
	}

	if(!gpio_is_valid(switch_data->usb_sw_sel_gpio)){
		dev_err(dev, "usb_sw_sel(gpio %d) is invalid\n", switch_data->usb_sw_sel_gpio);
		return -EINVAL;
	}

	ret = gpio_request(switch_data->usb_sw_sel_gpio, pdev->name);
	if (ret < 0) {
		dev_err(dev, "gpio_request usb_sw_sel(gpio %d) fail\n", switch_data->usb_sw_sel_gpio);
		return -EINVAL;
	}

	if(cp_usb_retain) {
		usb_switch = CP_USB_ON;
		switch_data->usb_state = CP_USB_ON_RETAIN;
		cp_usb_retain = false;
	}

	ret = gpio_direction_output(switch_data->usb_sw_sel_gpio, usb_switch);
	if (ret < 0) {
		dev_err(dev, "gpio_direction_output usb_sw_sel(gpio %d) fail\n", switch_data->usb_sw_sel_gpio);
		gpio_free(switch_data->usb_sw_sel_gpio);
	}

	return ret;
}

#ifdef USB_SWITCH_NO_VBUS_SWITCH
static int usb_switch_usb_otg_init(struct usb_switch_data *switch_data)
{
	struct usb_phy *phy = NULL;
	struct lge_usb3phy *usb3phy;
	struct device *dev = switch_data->dev;

	switch_data->odin_otg = NULL;

	phy = usb_get_phy(USB_PHY_TYPE_USB3);
	if (IS_ERR_OR_NULL(phy)) {
		dev_err(dev, "Not found :USB_PHY_TYPE_USB3\n");
		return -EINVAL;
	}

	usb3phy = phy_to_usb3phy(phy);
	switch_data->odin_otg = usb3phy->odin_otg;

	return 0;
}
#endif

static int usb_switch_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct usb_switch_data *switch_data;
	int ret = 0;

	switch_data = kzalloc(sizeof(struct usb_switch_data), GFP_KERNEL);
	if (!switch_data) {
		dev_err(dev, "No Memory\n");
		return -ENOMEM;
	}

	switch_data->dev = dev;
	switch_data->usb_state = AP_USB_ON;

	/* initialize lock */
	mutex_init(&switch_data->mutex);

	dev_info(dev, "%s\n", dev_name(dev));

	if(of_property_read_string(pdev->dev.of_node,
			"lge,usb_switch,product_name", &(switch_data->sdev.name))) {
		dev_err(dev, "Not found :lge,usb_switch,product_name\n");
		return -EINVAL;
	}

#ifdef USB_SWITCH_NO_VBUS_SWITCH
	usb_switch_usb_otg_init(switch_data);
#endif

	ret = usb_switch_usb_sw_sel_gpio_init(pdev, switch_data);
	if (ret < 0) {
		goto err_free_data;
	}

	switch_data->sdev.get_usb_state = usb_switch_get_state;
	switch_data->sdev.set_usb_state = usb_switch_set_state;

	ret = usb_switch_dev_register(&switch_data->sdev);
	if (ret < 0)
		goto err_free_data;

	platform_set_drvdata(pdev, switch_data);

	return 0;

err_free_data:
	kfree(switch_data);

	return ret;
}

static int usb_switch_remove(struct platform_device *pdev)
{
	struct usb_switch_data *switch_data = platform_get_drvdata(pdev);

	gpio_free(switch_data->usb_sw_sel_gpio);

	usb_switch_dev_unregister(&switch_data->sdev);

	kfree(switch_data);

	return 0;
}

/*
usb_switch{
	compatible = "lg,usb-switch";
	lge,usb_switch,product_name = "FSUSB104UMX";
	usb_sw_sel = <121>;
};
*/
static const struct of_device_id usb_switch_of_match[] = {
	{ .compatible = "lg,usb-switch" },
	{ /* Sentinel */ }
};

static struct platform_driver usb_switch_driver = {
	.probe		= usb_switch_probe,
	.remove		= usb_switch_remove,
	.driver		= {
		.name	= "usb-switch",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(usb_switch_of_match),
	},
};

static int __init usb_switch_init(void)
{
	return platform_driver_register(&usb_switch_driver);
}

static void __exit usb_switch_exit(void)
{
	platform_driver_unregister(&usb_switch_driver);
}

module_init(usb_switch_init);
module_exit(usb_switch_exit);

MODULE_AUTHOR("Jaesung Woo <jaesung.woo@lge.com>");
MODULE_DESCRIPTION("USB Switch driver");
MODULE_LICENSE("GPL");
