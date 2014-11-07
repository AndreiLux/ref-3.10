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


#ifndef __LINUX_USBSWITCH_H__
#define __LINUX_USBSWITCH_H__

/* #define USB_SWITCH_NO_VBUS_SWITCH */
/* #define USB_SWITCH_STATE_INFO_CHARACTER */


#define AP_USB_ON					0
#define CP_USB_ON					1
#define CP_USB_ON_DOWNLOAD			2
#define CP_USB_ON_RETAIN			3

struct usb_switch_dev {
	const char	*name;
	struct device	*dev;
	int		index;

	int (*get_usb_state)(struct usb_switch_dev *sdev);
	int (*set_usb_state)(struct usb_switch_dev *sdev, int usb_switch);
};

extern int usb_switch_dev_register(struct usb_switch_dev *sdev);
extern void usb_switch_dev_unregister(struct usb_switch_dev *sdev);

#endif /* __LINUX_USBSWITCH_H__ */
