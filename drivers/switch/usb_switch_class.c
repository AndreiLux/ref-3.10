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
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/usb_switch.h>

struct class *usb_switch_class;
static atomic_t device_count;

#ifdef USB_SWITCH_STATE_INFO_CHARACTER
/* words written to sysfs files may, or may not, be \n terminated.
 * We want to accept with case. For this we use cmd_match.
 */
static int cmd_match(const char *cmd, const char *str)
{
	/* See if cmd, written into a sysfs file, matches
	 * str.  They must either be the same, or cmd can
	 * have a trailing newline
	 */
	while (*cmd && *str && *cmd == *str) {
		cmd++;
		str++;
	}
	if (*cmd == '\n')
		cmd++;
	if (*str || *cmd)
		return 0;
	return 1;
}
#endif

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct usb_switch_dev *sdev = (struct usb_switch_dev *)
								dev_get_drvdata(dev);
#ifdef USB_SWITCH_STATE_INFO_CHARACTER
	char *state_arr[2] = { "AP_USB", "CP_USB" };
	char *state_str = NULL;
#endif
	int state = AP_USB_ON;

	if (sdev->get_usb_state) {
		state = sdev->get_usb_state(sdev);
#ifdef USB_SWITCH_STATE_INFO_CHARACTER
		state_str = state_arr[state];
#else
		return sprintf(buf, "%d\n", state);
#endif
	}
	else { //DEFAULT STATE_AP_USB
#ifdef USB_SWITCH_STATE_INFO_CHARACTER
		state_str = state_arr[state];
#else
		return sprintf(buf, "%d\n", state);
#endif
	}

#ifdef USB_SWITCH_STATE_INFO_CHARACTER
	return sprintf(buf, "%s\n", state_str);
#endif
}

static ssize_t state_store(struct device *pdev, struct device_attribute *attr,
			    const char *buff, size_t size)
{
	struct usb_switch_dev *sdev = (struct usb_switch_dev *)
								dev_get_drvdata(pdev);
	int usb_switch = AP_USB_ON;

#ifdef USB_SWITCH_STATE_INFO_CHARACTER
	if (cmd_match(buff, "CP_USB")){
		usb_switch = CP_USB_ON;
	}
#endif

	sscanf(buff, "%d", &usb_switch);

	if (usb_switch > CP_USB_ON_RETAIN)
		usb_switch = AP_USB_ON;

	if (sdev->set_usb_state) {
		if(sdev->set_usb_state(sdev, usb_switch)){
			return -EINVAL;
		}
		else {
			return size;
		}
	}

	return -EINVAL;
}

static DEVICE_ATTR(state, S_IRUGO | S_IWUSR, state_show, state_store);

static int create_usb_switch_class(void)
{
	if (!usb_switch_class) {
		usb_switch_class = class_create(THIS_MODULE, "usb_switch");
		if (IS_ERR(usb_switch_class))
			return PTR_ERR(usb_switch_class);
		atomic_set(&device_count, 0);
	}

	return 0;
}

int usb_switch_dev_register(struct usb_switch_dev *sdev)
{
	int ret;

	if (!usb_switch_class) {
		ret = create_usb_switch_class();
		if (ret < 0)
			return ret;
	}

	sdev->index = atomic_inc_return(&device_count);
	sdev->dev = device_create(usb_switch_class, NULL,
		MKDEV(0, sdev->index), NULL, sdev->name);
	if (IS_ERR(sdev->dev))
		return PTR_ERR(sdev->dev);

	ret = device_create_file(sdev->dev, &dev_attr_state);
	if (ret < 0)
		goto err_create_file_1;

	dev_set_drvdata(sdev->dev, sdev);

	return 0;

err_create_file_1:
	device_destroy(usb_switch_class, MKDEV(0, sdev->index));
	printk(KERN_ERR "switch: Failed to register driver %s\n", sdev->name);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_switch_dev_register);

void usb_switch_dev_unregister(struct usb_switch_dev *sdev)
{
	device_remove_file(sdev->dev, &dev_attr_state);
	device_destroy(usb_switch_class, MKDEV(0, sdev->index));
	dev_set_drvdata(sdev->dev, NULL);
}
EXPORT_SYMBOL_GPL(usb_switch_dev_unregister);

static int __init usb_switch_class_init(void)
{
	return create_usb_switch_class();
}

static void __exit usb_switch_class_exit(void)
{
	class_destroy(usb_switch_class);
}

module_init(usb_switch_class_init);
module_exit(usb_switch_class_exit);

MODULE_AUTHOR("Jaesung Woo <jaesung.woo@lge.com>");
MODULE_DESCRIPTION("USB Switch Class driver");
MODULE_LICENSE("GPL");

