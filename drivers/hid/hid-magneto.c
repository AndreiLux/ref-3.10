/*
 * HID driver for Lab126 Bluetooth Keyboard
 *
 * Copyright (C) 2014 Primax Electronics Ltd.
 *
 * Author:
 *	Brent Chang <brent.chang@primax.com.tw>
 *  Brothans Li <brothans.li@primax.com.tw>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/delay.h>
#include "hid-ids.h"

#include <linux/kernel.h>

/* Firmware version */
#define PMX_DRV_VERSION "2014-09-18_01.06"

/* Report id */
#define REPORT_ID_KEYBOARD		0x01
#define REPORT_ID_MOUSE			0x07
#define REPORT_ID_TOUCHPAD		0x08
/* Handle Lab126 Keyboard shop key */
#define REPORT_ID_CONSUMER		0x0c
#define REPORT_ID_LAB126		0x09

#define CAPS_OFF	0
#define CAPS_ON		1
int caps_status = CAPS_OFF;
/* CapsLock set feature report command */
unsigned char buf_on[] = { 0x64, 0x02 };
unsigned char buf_off[] = { 0x64, 0x00 };

/* Max multi-touch count */
#define MAX_MULTITOUCH_COUNT	2

/* Debug message */

/* #define PRIMAX_DEBUG */
#ifdef PRIMAX_DEBUG
	#define DEBUG_MSG(fmt, args...)	printk(fmt, ## args)
#else
	#define DEBUG_MSG(fmt, args...)
#endif

/* Global variables */
struct timer_list dpad_timer;
struct timer_list delay_timer;

/*
 * Device structure for matched devices
 * @quirks: Currently unused.
 * @input: Input device through which we report events.
 * @lastFingCount: Last finger count for multi-touch.
 * @DPadPressed: 4-direction D-pad status array
 * @lastDirection: Last direction of gesture (4-direction)
 * @DPadTransformed: Record if D-Pad long pressed action been transformed
 * @lastZoomStart: Record last received zoom event time
 */
struct pmx_device {
	unsigned long quirks;
	struct input_dev *input;
	int lastFingCount;
	char DPadPressed[4];
	int lastDirection;
	int DPadTransformed;
	unsigned long lastZoomStart;
	unsigned char initDone;
	struct timer_list dpad_timer;	/* brent */
	struct timer_list delay_timer;	/* brent */
};

/*
 * Timer callback function for first key event delay feature.
 * This is used to delay input events until evdev been created successfully.
 */
static void delay_func(unsigned long data)
{
	struct pmx_device *pmx_dev = (struct pmx_device *) data;

	DEBUG_MSG("%s: Ready to send input event\n", __func__);
	pmx_dev->initDone = 1;

	input_sync(pmx_dev->input);
}

/*
 *  Deal with input raw event
 *  REPORT_ID:		DESCRIPTION:
 *	0x01			Keyboard
 *	0x07			Mouse
 *	0x08			Touchpad
 *
 *	0x09			Lab126 Keyboard : handle Shop key
 */

static int pmx_raw_event(struct hid_device *hdev, struct hid_report *report,
	 u8 *data, int size)
{
	struct pmx_device *pmx_dev = hid_get_drvdata(hdev);
	int i;
	char keyCode;
	/* int keyStatus; */
	int delay_count = 0;
	int ret;

	DEBUG_MSG("%s: report type %d, report data:\n", __func__, report->id);
	for (i = 0; i < size; i++)
		DEBUG_MSG("0x%02x ", data[i]);
	DEBUG_MSG("\n");

	/* Add loop to delay input event while init not finished */
	while (!(hdev->claimed & HID_CLAIMED_INPUT) || !pmx_dev->initDone) {

		delay_count++;
		DEBUG_MSG("%s: wait for initialization\n", __func__);
		msleep(500);
	}
	DEBUG_MSG("%s: event delay time = %d ms\n", __func__, delay_count * 500);
	DEBUG_MSG("%s: hdev->claimed = 0x%x, ready to send input event\n", __func__, hdev->claimed);

	switch (report->id) {
	case REPORT_ID_KEYBOARD:	/* Keyboard event: 0x01 */
		DEBUG_MSG("%s: Enter:: received REPORT_ID_KEYBOARD event\n", __func__);
		keyCode = data[3];
		switch (keyCode) {
		case 0x39:	/* Handle CapsLock key */
			DEBUG_MSG("%s: handle Lab126 KEY_CAPSLOCK:  keyCode = 0x%02x\n", __func__, keyCode);
			if (caps_status) {
				/* Current Caps status: On ==> changed to Off */
				DEBUG_MSG("%s: CapsLock status: On ---> Off\n", __func__);
				caps_status = CAPS_OFF;
				ret = hdev->hid_output_raw_report(hdev, buf_off, sizeof(buf_off), HID_FEATURE_REPORT);
			} else {
				/* Current Caps status: Off ==> changed to On */
				DEBUG_MSG("%s: CapsLock status: Off ---> On\n", __func__);
				caps_status = CAPS_ON;
				ret = hdev->hid_output_raw_report(hdev, buf_on, sizeof(buf_on), HID_FEATURE_REPORT);
			}

			if (ret < 0) {
				DEBUG_MSG("%s: set HID_FEATURE_REPORT of CapsLock failed\n", __func__);
			}
			/* pass event to system */
			break;
		}
		break;
#if 0
	case REPORT_ID_CONSUMER:		/* Consumer event: 0x0c */
		DEBUG_MSG("%s: Enter:: received REPORT_ID_CONSUMER event\n", __func__);
		break;
	case REPORT_ID_MOUSE:		/* Mouse event: 0x07 */
		DEBUG_MSG("%s: Enter:: received REPORT_ID_MOUSE event\n", __func__);
		break;
	case REPORT_ID_TOUCHPAD:	/* Touchpad event: 0x08 */
		DEBUG_MSG("%s: Enter:: received REPORT_ID_TOUCHPAD event\n", __func__);
		break;
#endif
	case REPORT_ID_LAB126:		/* Lab126 Keyboard event: 0x09 */
		DEBUG_MSG("%s: Enter:: received REPORT_ID_LAB126 event\n", __func__);
		keyCode = data[1];
		switch (keyCode) {
		case 0x86:
			DEBUG_MSG("%s: handle Lab126 Keyboard special case:  keyCode = 0x%02x\n", __func__, keyCode);
			input_report_key(pmx_dev->input, KEY_SHOP, 1);
			input_sync(pmx_dev->input);
			input_report_key(pmx_dev->input, KEY_SHOP, 0);
			input_sync(pmx_dev->input);
			break;

		case 0x87:
			DEBUG_MSG("%s: handle Lab126 Keyboard special case:  keyCode = 0x%02x\n", __func__, keyCode);
			input_report_key(pmx_dev->input, KEY_BRIGHTNESSUP, 1);
			input_sync(pmx_dev->input);
/*
			input_report_key(pmx_dev->input, KEY_BRIGHTNESSUP, 0);
			input_sync(pmx_dev->input);
*/
			break;

		case 0x88:
			DEBUG_MSG("%s: handle Lab126 Keyboard special case:  keyCode = 0x%02x\n", __func__, keyCode);
			input_report_key(pmx_dev->input, KEY_BRIGHTNESSDOWN, 1);
			input_sync(pmx_dev->input);
/*
			input_report_key(pmx_dev->input, KEY_BRIGHTNESSDOWN, 0);
			input_sync(pmx_dev->input);
*/
			break;

		case 0x90:
			DEBUG_MSG("%s: handle Lab126 Keyboard special case:  keyCode = 0x%02x\n", __func__, keyCode);
			input_report_key(pmx_dev->input, KEY_F21, 1);
			input_sync(pmx_dev->input);
			input_report_key(pmx_dev->input, KEY_F21, 0);
			input_sync(pmx_dev->input);
			break;

		case 0x91:
			DEBUG_MSG("%s: handle Lab126 Keyboard special case:  keyCode = 0x%02x\n", __func__, keyCode);
			input_report_key(pmx_dev->input, KEY_F22, 1);
			input_sync(pmx_dev->input);
			input_report_key(pmx_dev->input, KEY_F22, 0);
			input_sync(pmx_dev->input);
			break;

		case 0x92:
			DEBUG_MSG("%s: handle Lab126 Keyboard special case:  keyCode = 0x%02x\n", __func__, keyCode);
			input_report_key(pmx_dev->input, KEY_F23, 1);
			input_sync(pmx_dev->input);
			input_report_key(pmx_dev->input, KEY_F23, 0);
			input_sync(pmx_dev->input);
			break;

		case 0x93:
			DEBUG_MSG("%s: handle Lab126 Keyboard special case:  keyCode = 0x%02x\n", __func__, keyCode);
			input_report_key(pmx_dev->input, KEY_F24, 1);
			input_sync(pmx_dev->input);
			input_report_key(pmx_dev->input, KEY_F24, 0);
			input_sync(pmx_dev->input);
			break;

		case 0x00:
			DEBUG_MSG("%s: handle Lab126 Keyboard special case:  keyCode = 0x%02x\n", __func__, keyCode);
			input_report_key(pmx_dev->input, KEY_BRIGHTNESSUP, 0);
			input_sync(pmx_dev->input);
			input_report_key(pmx_dev->input, KEY_BRIGHTNESSDOWN, 0);
			input_sync(pmx_dev->input);
			break;
		}
		return 1;
		break;
	default:	/* Unknown report id */
		DEBUG_MSG("%s: unhandled report id %d\n", __func__, report->id);
		break;
	}

	return 0;	/* Pass event to linux input subsystem */

}


/*
 * Probe function for matched devices
 */
static int pmx_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;
	struct pmx_device *pmx;

	DEBUG_MSG("%s\n", __func__);

	pmx = kmalloc(sizeof(*pmx), GFP_KERNEL | __GFP_ZERO);
	if (pmx == NULL) {
		DEBUG_MSG("%s: can't alloc primax descriptor\n", __func__);
		return -ENOMEM;
	}

	pmx->quirks = id->driver_data;
	hid_set_drvdata(hdev, pmx);

	ret = hid_parse(hdev);
	if (ret) {
		DEBUG_MSG("%s: parse failed\n", __func__);
		goto fail;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	/* ret = hid_hw_start(hdev, HID_CONNECT_HIDINPUT); */
	if (ret) {
		DEBUG_MSG("%s: hw start failed\n", __func__);
		goto fail;
	}

	/* Initialize CapsLock status: Off */
	caps_status = CAPS_OFF;

#if 0
	/* Send CapsLock set feature report command */
	if (caps_status) {
		/* CapsLock status: On */
		DEBUG_MSG("%s: Initialize CapsLock status: On\n", __func__);
		ret = hdev->hid_output_raw_report(hdev, buf_on, sizeof(buf_on), HID_FEATURE_REPORT);
	} else {
		/* CapsLock status: Off */
		DEBUG_MSG("%s: Initialize CapsLock status: Off\n", __func__);
	}

	if (ret < 0) {
		DEBUG_MSG("%s: set HID_FEATURE_REPORT of CapsLock failed\n", __func__);
		goto fail;
	}
#endif


	DEBUG_MSG("%s: Init ok, hdev->claimed = 0x%x\n", __func__, hdev->claimed);

	init_timer(&pmx->delay_timer);
	pmx->delay_timer.function = delay_func;
	pmx->delay_timer.expires = jiffies + 1*HZ;
	pmx->delay_timer.data = (unsigned long) pmx;
	add_timer(&pmx->delay_timer);

	return 0;

fail:
	kfree(pmx);
	return ret;
}


/*
 * Input settings and key mapping for matched devices
 *  USAGE_PAGE_ID:      DESCRIPTION:
 *		0x0001			Generic Desktop
 *		0x0006			Generic Device Control
 *		0x0007			Keyboard/Keypad
 *		0x0008			LED
 *		0x0009			BUTTON
 *		0x000c			Consumer
 *		0x000d			Digitizer
 */
static int pmx_input_mapping(struct hid_device *hdev,
		struct hid_input *hi, struct hid_field *field,
		struct hid_usage *usage, unsigned long **bit, int *max)
{
	struct input_dev *input = hi->input;
	struct pmx_device *pmx = hid_get_drvdata(hdev);
	/* DEBUG_MSG("%s\n", __func__); */

	if (!pmx->input)
		pmx->input = hi->input;

	/*
		DEBUG_MSG("%s: Usage page = 0x%x, Usage id = 0x%x\n", __func__,
				  (usage->hid & HID_USAGE_PAGE) >> 4, usage->hid & HID_USAGE);
	*/
	switch (usage->hid & HID_USAGE_PAGE) {
	case HID_UP_GENDESK:	/* 0x0001 */
		/* Define accepted event */
		set_bit(EV_REL, input->evbit);
		set_bit(EV_ABS, input->evbit);
		set_bit(EV_KEY, input->evbit);
		set_bit(EV_SYN, input->evbit);

		/* Handle Lab126 Keyboard shop key */
		set_bit(KEY_SHOP, input->keybit);
		set_bit(KEY_BRIGHTNESSUP, input->keybit);
		set_bit(KEY_BRIGHTNESSDOWN, input->keybit);
		set_bit(KEY_F21, input->keybit);
		set_bit(KEY_F22, input->keybit);
		set_bit(KEY_F23, input->keybit);
		set_bit(KEY_F24, input->keybit);
		break;

	case HID_UP_KEYBOARD:	/* 0x0007 */
		break;

	case HID_UP_LED:		/* 0x0008 */
		break;

	case HID_UP_BUTTON:		/* 0x0009 */
		break;

	case HID_UP_CONSUMER:	/* 0x000c */
		break;

	case HID_UP_DIGITIZER:	/* 0x000d */
		break;

	default:
		break;
	}


	/*
	 * Return a 1 means a matching mapping is found, otherwise need
     * HID driver to search mapping in hid-input.c
	 */
	return 0;
}


/*
 * Remove function
 */
static void pmx_remove(struct hid_device *hdev)
{
	struct pmx_device *pmx = hid_get_drvdata(hdev);

	DEBUG_MSG("%s\n", __func__);
	hid_hw_stop(hdev);

	if (NULL != pmx)
		kfree(pmx);
}


/*
 * Device list that matches this driver
 */
static const struct hid_device_id pmx_devices[] = {
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_LAB126_KB, USB_DEVICE_ID_LAB126_US_KB) },
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_LAB126_KB, USB_DEVICE_ID_LAB126_UK_KB) },
	{ }	/* Terminating entry */
};
MODULE_DEVICE_TABLE(hid, pmx_devices);


/*
 * Special driver function structure for matched devices
 */
static struct hid_driver pmx_driver = {
	.name = "primax_lab126_keyboard",
	.id_table = pmx_devices,
	.raw_event = pmx_raw_event,
	.probe = pmx_probe,
	.remove = pmx_remove,
	.input_mapping = pmx_input_mapping,
};


/*
 * Init function
 */
static int __init pmx_init(void)
{
	int ret = hid_register_driver(&pmx_driver);

	DEBUG_MSG("%s: pmx driver version = %s\n", __func__, PMX_DRV_VERSION);
	return ret;
}


/*
 * Exit function
 */
static void __exit pmx_exit(void)
{
	DEBUG_MSG("%s\n", __func__);
	hid_unregister_driver(&pmx_driver);
}


module_init(pmx_init);
module_exit(pmx_exit);

MODULE_AUTHOR("Brothans Li<brothans.li@primax.com.tw>");
MODULE_LICENSE("GPL");

/* End of file */
