/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * Author: Wonsuk Chang <wonsuk.chang@lge.com>
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
 #ifndef __DRIVERS_ODIN_USB3__H
#define __DRIVERS_ODIN_USB3__H

#include <linux/usb/otg.h>

#define ODIN_USB	"odin_usb3: "
#define LGE_USB3PHY	"LGE-usb3phy: "

#define usb_err(x...)	printk(KERN_ERR ODIN_USB x )
#define usb_print(x...)	printk(ODIN_USB x )

#ifdef CONFIG_ODIN_USB3_DBG_SYSFS
extern u32 ud_loglevel;

enum odin_usb3_log {
	ODIN_USB3_LOG_NONE = 0,
	ODIN_USB3_LOG_CORE,
	ODIN_USB3_LOG_UDC,
	ODIN_USB3_LOG_OTG,
	ODIN_USB3_LOG_PHY,
	ODIN_USB3_LOG_ISO,
};

#define usb_dbg(x...)	printk(ODIN_USB x )

#define ud_core(x...)	\
	do {		\
		if (ODIN_USB3_LOG_NONE < ud_loglevel)	\
			printk(ODIN_USB x );	\
	} while (0)
#define ud_udc(x...)	\
	do {		\
		if (ODIN_USB3_LOG_UDC == ud_loglevel)	\
			printk(ODIN_USB x );	\
	} while (0)
#define ud_otg(x...)	\
	do {		\
		if (ODIN_USB3_LOG_OTG == ud_loglevel)	\
			printk(ODIN_USB x );	\
	} while (0)
#define ud_phy(x...)	\
	do {		\
		if (ODIN_USB3_LOG_PHY == ud_loglevel)	\
			printk(LGE_USB3PHY x );	\
	} while (0)
#define ud_iso(x...)	\
	do {		\
		if (ODIN_USB3_LOG_ISO == ud_loglevel)	\
			printk(ODIN_USB x );	\
	} while (0)

#else
#define usb_dbg(x...)	do {} while (0)
#define ud_core(x...)	usb_dbg(x...)
#define ud_udc(x...)	usb_dbg(x...)
#define ud_otg(x...)	usb_dbg(x...)
#define ud_phy(x...)	usb_dbg(x...)
#define ud_iso(x...)	do {} while (0)
#endif /* CONFIG_ODIN_USB3_DBG_SYSFS */

#define ODIN_USB3_EVENT_BUF_SIZE	256

enum bc_cabletype_odin_otg {
	BC_CABLETYPE_UNDEFINED = 0,
	BC_CABLETYPE_SDP,
	BC_CABLETYPE_DCP,
	BC_CABLETYPE_CDP,
	BC_CABLETYPE_SLIMPORT,
};

struct odin_usb3_otg {
	struct usb_otg otg;
	struct usb_phy phy;

	struct device *usb3phy;
	struct device *charger_dev;
	struct device *adc_dev;

	unsigned cur_power;
	enum bc_cabletype_odin_otg cabletype;
	bool factory_cable_no_battery;
};

struct odin_usb3_ic_ops {
	bool (* vbus_det) (struct odin_usb3_otg *odin_otg);
	void (* id_det) (struct odin_usb3_otg *odin_otg);
	void (* otg_vbus) (struct odin_usb3_otg *odin_otg, int onoff);
};

static inline struct odin_usb3_otg *phy_to_usb3otg(struct usb_phy *_phy)
{
	return container_of(_phy, struct odin_usb3_otg, phy);
}

extern struct odin_usb3_ic_ops usb3_ic_ops;

struct odin_usb3_otg *odin_usb3_get_otg(struct platform_device *child);
int odin_otg_vbus_event(struct odin_usb3_otg *odin_otg,
									int vbus, int delay_ms);
#ifdef CONFIG_USB_G_LGE_ANDROID
int odin_otg_get_vbus_state(struct odin_usb3_otg *odin_otg);
#endif
int odin_otg_id_event(struct odin_usb3_otg *odin_otg, int id);
int odin_drd_usb_id_state(struct usb_phy *phy);
int odin_otg_bc_detect(struct odin_usb3_otg *odin_otg);
void odin_otg_bc_cable_remove(struct odin_usb3_otg *odin_otg);
void odin_otg_tune_phy(struct odin_usb3_otg *odin_otg);
int odin_drd_get_state(struct platform_device *child);
int odin_usb3_otg_en_gpio_init(struct platform_device *pdev);

void odin_drd_dump_global_regs_reset(struct device *dev);
void odin_drd_usb2_phy_suspend(struct device *dev, int suspend);
void odin_drd_usb3_phy_suspend(struct device *dev, int suspend);
void odin_drd_ena_eventbuf(struct device *dev);
void odin_drd_dis_eventbuf(struct device *dev);
void odin_drd_flush_eventbuf(struct device *dev);
int odin_drd_get_eventbuf(struct device *dev);
void odin_drd_set_eventbuf(struct device *dev, int cnt);
void odin_drd_init_eventbuf(struct device *dev, dma_addr_t event_buf_dma);
int odin_drd_hw_init(struct device *dev);

#define ODIN_USB_CABLE_EVENT		0x0001

#endif /*__DRIVERS_ODIN_USB3__H */
