/* linux/drivers/usb/gadget/u_lgeusb.c
 *
 * Copyright (C) 2011,2012 LG Electronics Inc.
 * Author : Hyeon H. Park <hyunhui.park@lge.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#include <linux/board_lge.h>
#include <linux/platform_data/lge_android_usb.h>

#include "u_lgeusb.h"

static struct mutex lgeusb_lock;

/* This length must be same as MAX_STR_LEN in android.c */
#define MAX_SERIAL_NO_LEN 256

#define LGE_VENDOR_ID 	0x1004
#define LGE_PRODUCT_ID 	0x618E
#define LGE_FACTORY_PID	0x6000

struct lgeusb_dev {
	struct device *dev;
	u16 vendor_id;
	u16 factory_pid;
	u8  iSerialNumber;
	const char *product;
	const char *manufacturer;
	const char *fcomposition;
	lgeusb_mode current_mode;

	int (*get_serial_number)(char *serial);
	int (*get_factory_cable)(void);
};

static char model_string[32];
static char swver_string[32];
static char subver_string[32];
static char phoneid_string[32];

#ifdef CONFIG_USB_G_LGE_MULTIPLE_CONFIGURATION
static bool is_mac_os;
#endif

static struct lgeusb_dev *_lgeusb_dev;

/* Belows are borrowed from android gadget's ATTR macros ;) */
#define LGE_ID_ATTR(field, format_string)               \
static ssize_t                              \
lgeusb_ ## field ## _show(struct device *dev, struct device_attribute *attr, \
		char *buf)                      \
{                                   \
	struct lgeusb_dev *usbdev = _lgeusb_dev;		\
	return sprintf(buf, format_string, usbdev->field);      \
}                                   \
static ssize_t                              \
lgeusb_ ## field ## _store(struct device *dev, struct device_attribute *attr, \
		const char *buf, size_t size)                   \
{                                   \
	int value;                              \
	struct lgeusb_dev *usbdev = _lgeusb_dev;	\
	if (sscanf(buf, format_string, &value) == 1) {          \
		usbdev->field = value;              \
		return size;                        \
	}                               \
	return -1;                          \
}                                   \
static DEVICE_ATTR(field, S_IRUGO | S_IWUSR, lgeusb_ ## field ## _show, \
		lgeusb_ ## field ## _store);

#define LGE_RDONLY_STRING_ATTR(field, string)               \
static ssize_t                              \
lgeusb_ ## field ## _show(struct device *dev, struct device_attribute *attr,   \
		char *buf)                      \
{                                   \
	struct lgeusb_dev *usbdev = _lgeusb_dev;		\
	return sprintf(buf, "%s", usbdev->string);              \
}                                   \
static DEVICE_ATTR(field, S_IRUGO, lgeusb_ ## field ## _show, NULL);

#define LGE_STRING_ATTR(field, buffer)               \
static ssize_t                              \
field ## _show(struct device *dev, struct device_attribute *attr,   \
		char *buf)                      \
{                                   \
	return snprintf(buf, PAGE_SIZE, "%s", buffer);          \
}                                   \
static ssize_t                              \
field ## _store(struct device *dev, struct device_attribute *attr,  \
		const char *buf, size_t size)                   \
{                                   \
	if (size >= sizeof(buffer)) \
		return -EINVAL;         \
	if (sscanf(buf, "%31s", buffer) == 1) {            \
		return size;                        \
	}                               \
	return -1;                          \
}                                   \
static DEVICE_ATTR(field, S_IRUGO | S_IWUSR, field ## _show, field ## _store);

LGE_ID_ATTR(vendor_id, "%04X\n")
LGE_ID_ATTR(factory_pid, "%04X\n")
LGE_ID_ATTR(iSerialNumber, "%d\n")
LGE_RDONLY_STRING_ATTR(product_name, product)
LGE_RDONLY_STRING_ATTR(manufacturer_name, manufacturer)
LGE_RDONLY_STRING_ATTR(fcomposition, fcomposition)
LGE_STRING_ATTR(model_name, model_string)
LGE_STRING_ATTR(sw_version, swver_string)
LGE_STRING_ATTR(sub_version, subver_string)
LGE_STRING_ATTR(phone_id, phoneid_string)

static ssize_t lgeusb_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lgeusb_dev *usbdev = _lgeusb_dev;
	int is_factory_cable = 0;
	int ret = 0;

	if (usbdev->get_factory_cable)
		is_factory_cable = usbdev->get_factory_cable();

	mutex_lock(&lgeusb_lock);
	if (is_factory_cable)
		usbdev->current_mode = LGEUSB_FACTORY_MODE;
	else
		usbdev->current_mode = LGEUSB_ANDROID_MODE;
	mutex_unlock(&lgeusb_lock);

	switch (is_factory_cable) {
		case LGEUSB_FACTORY_56K:
			ret = sprintf(buf, "%s\n", "factory_56k");
			break;
		case LGEUSB_FACTORY_130K:
			ret = sprintf(buf, "%s\n", "factory_130k");
			break;
		case LGEUSB_FACTORY_910K:
			ret = sprintf(buf, "%s\n", "factory_910k");
			break;
		default:
			ret = sprintf(buf, "%s\n", "normal");
			break;
	}

	return ret;
}
static DEVICE_ATTR(lge_usb_mode, S_IRUGO, lgeusb_mode_show, NULL);

#ifdef CONFIG_USB_G_LGE_ANDROID_AUTORUN
static u16 user_mode;

/* To set/get USB user mode to/from user space for autorun */
static int autorun_user_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;

	ret = sprintf(buf, "%d", user_mode);
	return ret;
}

static int autorun_user_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	unsigned long tmp;

	ret = strict_strtoul(buf, 10, &tmp);
	if (ret)
		return ret;

	mutex_lock(&lgeusb_lock);
	user_mode = (unsigned int)tmp;
	mutex_unlock(&lgeusb_lock);

	pr_info("autorun user mode : %d\n", user_mode);

	return ret;
}
static DEVICE_ATTR(autorun_user_mode, S_IRUGO | S_IWUSR, autorun_user_mode_show,
		autorun_user_mode_store);

int lgeusb_get_autorun_user_mode(void)
{
	return user_mode;
}

int lgeusb_get_model_name(char *model)
{
	if (!model || strlen(model) > 15)
		return -EINVAL;

	strlcpy(model, model_string, strlen(model) - 1);
	pr_info("lgeusb: model name %s\n", model);
	return 0;
}

int lgeusb_get_phone_id(char *phoneid)
{
	if (!phoneid || strlen(phoneid) > 15)
		return -EINVAL;

	strlcpy(phoneid, phoneid_string, strlen(phoneid) - 1);
	pr_info("lgeusb: phoneid %s\n", phoneid);
	return 0;
}

int lgeusb_get_sw_ver(char *sw_ver)
{
	if (!sw_ver || strlen(sw_ver) > 15)
		return -EINVAL;

	strlcpy(sw_ver, swver_string, strlen(sw_ver) - 1);
	pr_info("lgeusb: sw version %s\n", sw_ver);
	return 0;
}

int lgeusb_get_sub_ver(char *sub_ver)
{
	if (!sub_ver || strlen(sub_ver) > 15)
		return -EINVAL;

	strlcpy(sub_ver, subver_string, strlen(sub_ver) - 1);
	pr_info("lgeusb: sw sub version %s\n", sub_ver);
	return 0;
}
#endif

static struct device_attribute *lge_android_usb_attributes[] = {
	&dev_attr_vendor_id,
	&dev_attr_factory_pid,
	&dev_attr_product_name,
	&dev_attr_manufacturer_name,
	&dev_attr_fcomposition,
	&dev_attr_lge_usb_mode,
	&dev_attr_iSerialNumber,
	&dev_attr_model_name,
	&dev_attr_sw_version,
	&dev_attr_sub_version,
	&dev_attr_phone_id,
#ifdef CONFIG_USB_G_LGE_ANDROID_AUTORUN
	&dev_attr_autorun_user_mode,
#endif
	NULL
};

static int lgeusb_create_device_file(struct lgeusb_dev *dev)
{
	struct device_attribute **attrs = lge_android_usb_attributes;
	struct device_attribute *attr;
	int ret;

	while ((attr = *attrs++)) {
		ret = device_create_file(dev->dev, attr);
		if (ret) {
			pr_err("usb: lgeusb: error on creating device file %s\n",
					attr->attr.name);
			return ret;
		}
	}

	return 0;
}

static void lgusb_destroy_device_file(struct lgeusb_dev *dev)
{
	struct device_attribute **attrs = lge_android_usb_attributes;
	struct device_attribute *attr;

	while ((attr = *attrs++))
		device_remove_file(dev->dev, attr);
}

static int lgeusb_remove(struct platform_device *pdev)
{
	struct lgeusb_dev *dev = _lgeusb_dev;

	if (dev) {
		lgusb_destroy_device_file(dev);
		kfree(dev);
	}

	return 0;
}

int lgeusb_get_factory_cable(void)
{
	struct lgeusb_dev *usbdev = _lgeusb_dev;
	if (usbdev->get_factory_cable)
		return usbdev->get_factory_cable();
	return 0;
}

int lgeusb_get_vendor_id(void)
{
	struct lgeusb_dev *usbdev = _lgeusb_dev;
	return usbdev ? usbdev->vendor_id : -EINVAL;
}

int lgeusb_get_factory_pid(void)
{
	struct lgeusb_dev *usbdev = _lgeusb_dev;
	return usbdev ? usbdev->factory_pid : -EINVAL;
}

int lgeusb_get_serial_number(void)
{
	struct lgeusb_dev *usbdev = _lgeusb_dev;
	return usbdev ? usbdev->iSerialNumber : -EINVAL;
}

int lgeusb_get_manufacturer_name(char *manufact_name)
{
	struct lgeusb_dev *usbdev = _lgeusb_dev;
	char *manufact = manufact_name;

	if (!manufact || !usbdev || !usbdev->manufacturer)
		return -EINVAL;

	strlcpy(manufact, usbdev->manufacturer, MAX_SERIAL_NO_LEN - 1);
	pr_debug("lgeusb: manfacturer name %s\n", manufact);
	return 0;
}

int lgeusb_get_product_name(char *prod_name)
{
	struct lgeusb_dev *usbdev = _lgeusb_dev;
	char *prod = prod_name;

	if (!prod || !usbdev || !usbdev->product)
		return -EINVAL;

	strlcpy(prod, usbdev->product, MAX_SERIAL_NO_LEN - 1);
	pr_debug("lgeusb: product name %s\n", prod);
	return 0;
}

int lgeusb_get_factory_composition(char *fcomposition)
{
	struct lgeusb_dev *usbdev = _lgeusb_dev;
	char *fcompo = fcomposition;

	if (!fcomposition || !usbdev || !usbdev->fcomposition)
		return -EINVAL;

	strlcpy(fcompo, usbdev->fcomposition, MAX_SERIAL_NO_LEN - 1);
	pr_debug("lgeusb: factory composition %s\n", fcompo);
	return 0;
}

static int get_factory_cable(void)
{
	int res;
	lge_boot_mode_type boot_mode = lge_get_boot_mode();

	switch (boot_mode) {
		case LGE_BOOT_MODE_FACTORY:
		case LGE_BOOT_MODE_PIFBOOT:
			res = LGEUSB_FACTORY_130K;
			break;
		case LGE_BOOT_MODE_FACTORY2:
		case LGE_BOOT_MODE_PIFBOOT2:
			res = LGEUSB_FACTORY_56K;
			break;
		case LGE_BOOT_MODE_FACTORY3:
		case LGE_BOOT_MODE_PIFBOOT3:
			res = LGEUSB_FACTORY_910K;
			break;
		default:
			res = 0;
			break;
	}
	return res;
}
#ifdef CONFIG_USB_G_LGE_MULTIPLE_CONFIGURATION
void lgeusb_set_host_os(u16 w_length)
{
	switch (w_length) {
		case MAC_OS_TYPE:
			is_mac_os = true;
			break;
		case WIN_LINUX_TYPE:
			is_mac_os = false;
			break;
		default:
			break;
	}
}

bool lgeusb_get_host_os(void)
{
	return is_mac_os;
}
#endif

static int lgeusb_probe(struct platform_device *pdev)
{
	struct lge_android_usb_platform_data *pdata = pdev->dev.platform_data;
	struct lgeusb_dev *usbdev = _lgeusb_dev;

	dev_dbg(&pdev->dev, "%s: pdata: %p\n", __func__, pdata);

	usbdev->dev = &pdev->dev;

	if (pdata) {
		if (pdata->vendor_id)
			usbdev->vendor_id = pdata->vendor_id;

		if (pdata->factory_pid)
			usbdev->factory_pid = pdata->factory_pid;

		if (pdata->iSerialNumber)
			usbdev->iSerialNumber = pdata->iSerialNumber;

		if (pdata->product_name)
			usbdev->product = pdata->product_name;

		if (pdata->manufacturer_name)
			usbdev->manufacturer = pdata->manufacturer_name;

		if (pdata->factory_composition)
			usbdev->fcomposition = pdata->factory_composition;

		if (pdata->get_factory_cable)
			usbdev->get_factory_cable = pdata->get_factory_cable;
	}

	usbdev->current_mode = LGEUSB_DEFAULT_MODE;
	lgeusb_create_device_file(usbdev);

	return 0;
}

struct lge_android_usb_platform_data lge_android_usb_pdata = {
	.vendor_id = 0x1004,
	.factory_pid = 0x6000,
	.iSerialNumber = 0,
	.product_name = "LGE Android Phone",
	.manufacturer_name = "LG Electronics Inc.",
	.factory_composition = "acm,gser",
	.get_factory_cable = get_factory_cable,
};

struct platform_device lge_android_usb_device = {
	.name = "lge_android_usb",
	.id = -1,
	.dev = {
		.platform_data = &lge_android_usb_pdata,
	},
};

static struct platform_driver lge_android_usb_platform_driver = {
	.driver = {
		.name = "lge_android_usb",
	},
	.probe = lgeusb_probe,
	.remove = lgeusb_remove,
};

static int __init lgeusb_init(void)
{
	struct lgeusb_dev *dev;
	int ret;

	pr_info("u_lgeusb init\n");
	mutex_init(&lgeusb_lock);

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	_lgeusb_dev = dev;

	/* set default vid, pid and factory id. vid and pid will be overrided. */
	dev->vendor_id = LGE_VENDOR_ID;
	dev->factory_pid = LGE_FACTORY_PID;

	ret = platform_driver_register(&lge_android_usb_platform_driver);
	if (ret) {
		pr_err("%s(): Failed to register android"
				"platform driver\n", __func__);
		return ret;
	}

	ret = platform_device_register(&lge_android_usb_device);

	if (ret) {
		pr_err("%s(): Failed to register android"
				"platform driver\n", __func__);
		platform_driver_unregister(&lge_android_usb_platform_driver);
	}

	return ret;
}
module_init(lgeusb_init);

static void __exit lgeusb_cleanup(void)
{
	platform_device_unregister(&lge_android_usb_device);
	platform_driver_unregister(&lge_android_usb_platform_driver);
	_lgeusb_dev = NULL;
}
module_exit(lgeusb_cleanup);

