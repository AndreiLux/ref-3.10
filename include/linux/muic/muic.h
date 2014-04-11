/*
 * include/linux/muic/muic.h
 *
 * header file supporting MUIC common information
 *
 * Copyright (C) 2010 Samsung Electronics
 * Seoyoung Jeong <seo0.jeong@samsung.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef __MUIC_H__
#define __MUIC_H__

/* MUIC Interrupt */
enum {
	MUIC_INTR_DETACH	= 0,
	MUIC_INTR_ATTACH
};

/* MUIC Dock Observer Callback parameter */
enum {
	MUIC_DOCK_DETACHED	= 0,
	MUIC_DOCK_DESKDOCK	= 1,
	MUIC_DOCK_CARDOCK	= 2,
	MUIC_DOCK_AUDIODOCK	= 7,
	MUIC_DOCK_SMARTDOCK	= 8,

#if defined(CONFIG_MUIC_SUPPORT_FACTORY_BUTTON)
	MUIC_DOCK_FACTORY	= 11,
#endif /* CONFIG_MUIC_SUPPORT_FACTORY_BUTTON */

};

/* MUIC Path */
enum {
	MUIC_PATH_USB_AP	= 0,
	MUIC_PATH_USB_CP,
	MUIC_PATH_UART_AP,
	MUIC_PATH_UART_CP,
	MUIC_PATH_OPEN,
	MUIC_PATH_AUDIO,
};

/* bootparam SWITCH_SEL */
enum {
	SWITCH_SEL_USB_MASK	= 0x1,
	SWITCH_SEL_UART_MASK	= 0x2,
};

/* MUIC ADC table */
typedef enum {
	ADC_GND				= 0x00,
	ADC_SEND_END		= 0x01,
	ADC_REMOTE_S11		= 0x0c,
	ADC_SMARTDOCK		= 0x10, /* 0x10000 40.2K ohm */
	ADC_AUDIODOCK		= 0x12, /* 0x10010 64.9K ohm */
	ADC_CHARGING_CABLE	= 0x14,	/* 0x10100 102K ohm */
	ADC_CEA936ATYPE1_CHG	= 0x17,	/* 0x10111 200K ohm */
	ADC_JIG_USB_OFF		= 0x18, /* 0x11000 255K ohm */
	ADC_JIG_USB_ON		= 0x19, /* 0x11001 301K ohm */
	ADC_DESKDOCK		= 0x1a, /* 0x11010 365K ohm */
	ADC_CEA936ATYPE2_CHG	= 0x1b, /* 0x11011 442K ohm */
	ADC_JIG_UART_OFF	= 0x1c, /* 0x11100 523K ohm */
	ADC_JIG_UART_ON		= 0x1d, /* 0x11101 619K ohm */
	ADC_OPEN		= 0x1f,
	ADC_OPEN_219		= 0xfd, /* ADC open and 219.3K ohm */

	ADC_DONTCARE		= 0xfe, /* ADC don't care for MHL */
	ADC_ERROR		= 0xff, /* ADC value read error */
} muic_adc_t;


/* MUIC attached device type */
typedef enum {
	ATTACHED_DEV_NONE_MUIC = 0,
	ATTACHED_DEV_USB_MUIC,
	ATTACHED_DEV_CDP_MUIC,
	ATTACHED_DEV_OTG_MUIC,
	ATTACHED_DEV_TA_MUIC,
	ATTACHED_DEV_UNOFFICIAL_MUIC,
	ATTACHED_DEV_DESKDOCK_MUIC,
	ATTACHED_DEV_DESKDOCK_VB_MUIC,
	ATTACHED_DEV_CARDOCK_MUIC,
	ATTACHED_DEV_JIG_UART_OFF_MUIC,
	ATTACHED_DEV_JIG_UART_OFF_VB_MUIC,	/* VBUS enabled */
	ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC,	/* for otg test */
	ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC,	/* for fuelgauge test */
	ATTACHED_DEV_JIG_UART_ON_MUIC,
	ATTACHED_DEV_JIG_USB_OFF_MUIC,
	ATTACHED_DEV_JIG_USB_ON_MUIC,
	ATTACHED_DEV_SMARTDOCK_MUIC,
	ATTACHED_DEV_SMARTDOCK_VB_MUIC,
	ATTACHED_DEV_SMARTDOCK_TA_MUIC,
	ATTACHED_DEV_SMARTDOCK_USB_MUIC,
	ATTACHED_DEV_AUDIODOCK_MUIC,
	ATTACHED_DEV_MHL_MUIC,
	ATTACHED_DEV_CHARGING_CABLE_MUIC,
	ATTACHED_DEV_UNKNOWN_MUIC,

	ATTACHED_DEV_NUM,
} muic_attached_dev_t;

/* muic common callback driver internal data structure
 * that setted at muic-core.c file
 */
struct muic_platform_data {
	int irq_gpio;

	/* muic current USB/UART path */
	int usb_path;
	int uart_path;

	/* muic switch dev register function for DockObserver */
	void (*init_switch_dev_cb) (void);
	void (*cleanup_switch_dev_cb) (void);

	/* muic GPIO control function */
	int (*init_gpio_cb) (int switch_sel);
	int (*set_gpio_usb_sel) (int usb_path);
	int (*set_gpio_uart_sel) (int uart_path);
	int (*set_safeout) (int safeout_path);
};

#endif /* __MUIC_H__ */
