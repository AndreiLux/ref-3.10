/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
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

#if !defined(ODIN_DWC_HOST_H)
#define ODIN_DWC_HOST_H

#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/usb.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>

/*
 * DWC OTG dependent header
 */

typedef struct dwc_otg_device {
	/* Base address returned from ioremap() */
	void *base;

	/* Register offset for Diagnostic API */
	uint32_t reg_offset;

	/* Pointer to the core interface structure. */
	dwc_otg_core_if_t *core_if;

	/* Pointer to the HCD structure. */
	struct dwc_otg_hcd *hcd;

} dwc_otg_device_t;

/* Debug for DWC-OTG driver */
extern uint32_t g_dbg_lvl;

#define DBG_CIL			(0x01)
#define DBG_CILV		(0x02)
#define DBG_PCD			(0x04)
#define DBG_PCDV		(0x08)
#define DBG_HCD			(0x10)
#define DBG_HCDV		(0x20)
#define DBG_HCD_URB		(0x40)
#define DBG_ERR			(0x80)
#define DBG_ANY			(0xff)
#define DBG_OFF			0

#define USB_DWC "DWC-HSIC: "

#if defined(DEBUG) && !defined(CONFIG_IMC_LOG_OFF)
#define DWC_DEBUGPL(lvl, x...) \
	do { if ((lvl)&g_dbg_lvl) __DWC_DEBUG(USB_DWC x ); } while (0)

#define DWC_DEBUGP(x...)	DWC_DEBUGPL(DBG_ANY, x )

#define CHK_DEBUG_LEVEL(level) ((level) & g_dbg_lvl)
#else
#define DWC_DEBUGPL(lvl, x...) do {} while (0)
#define DWC_DEBUGP(x...)
#define CHK_DEBUG_LEVEL(level) (0)
#endif


/*
 * ODIN DWC Host dependent header
 */

/* Debug for ODIN-DWC driver */
#define ODIN_DWC_UERR	(0x01)
#define ODIN_DWC_UINFO	(0x02)
#define ODIN_DWC_UDBG	(0x04)

extern int odin_dwc_dbg_mask;

#define udbg(dbg_lvl_mask, args...) \
do { \
	if (odin_dwc_dbg_mask & ODIN_DWC_##dbg_lvl_mask) { \
		 pr_info("USB-HSIC: " args); \
	} \
} while (0)

/* Driver Init State */
enum init_state {
	HOST_CORE_INIT1,
	HOST_CORE_INIT2,
	HCD_INIT,
	INIT_DONE,
	INIT_FAILED,
};

/* USB-HSIC Bus State */
enum bus_state {
	DISCONNECT,
	SUSPENDED_DISCON,
	CHANGING1,
	CHANGING2,
	IDLE,
	RESET,
	ENUM_DONE,
	SUSPENDING,
	SUSPENDED,
	SUSPEND_ERR,
	RESUMING,
	RESUMED,
	RESUME_ERR,
};

struct odin_udev_id {
	int vid;
	int pid;
};

typedef struct odin_host_dev {
	/* IRQ number */
	unsigned int irq;

	/* Clocks for ODIN-DWC */
	struct clk *bus_clk;
	struct clk *refclkdig;
	struct clk *phy_clk;

	/* Regulator for ODIN-DWC */
	struct regulator *io_reg;

	/* CRG base address */
	void __iomem *crg_base;

	/* Offset of CRG registers */
	unsigned int reset_offset;
	unsigned int reset_mask;
	unsigned int phy_rst_val;
	unsigned int core_rst_val;
	unsigned int bus_rst_val;
} odin_host_dev_t;

typedef struct odin_dwc_drvdata {
	/* USB Host Controller Driver */
	struct usb_hcd *hcd;

	/* ODIN specific device data */
	dwc_otg_device_t dwc_dev;

	/* DWC specific device data */
	odin_host_dev_t odin_dev;

	/* ODIN-DWC Platform device */
	struct platform_device *pdev;

	/* ODIN HSIC bus state */
	enum bus_state b_state;

	/* Driver Init work queue and state */
	struct delayed_work init_work;
	enum init_state state;

	spinlock_t lock;

	/* Display USB devices info */
	struct notifier_block usb_nb;
} odin_dwc_ddata_t;

irqreturn_t odin_dwc_host_common_irq(int irq, void *dev);
int odin_dwc_host_cil_init(dwc_otg_core_if_t *core_if,
			const uint32_t *reg_base_addr);
void odin_dwc_host_cil_remove(dwc_otg_core_if_t *core_if);
void odin_dwc_host_reset(odin_dwc_ddata_t *ddata, bool dbg);
void odin_dwc_host_core_init1(dwc_otg_core_if_t *core_if);
void odin_dwc_host_core_init2(dwc_otg_core_if_t *core_if);
int odin_dwc_host_hcd_init(struct device *dev, odin_dwc_ddata_t *ddata);
void odin_dwc_host_set_host_mode(dwc_otg_core_if_t *core_if);
void odin_dwc_host_post_set_param(dwc_otg_core_if_t *core_if);
void odin_dwc_host_hcd_remove(struct dwc_otg_hcd *odin_hcd);
int odin_dwc_host_clk_enable(odin_dwc_ddata_t *ddata);
void odin_dwc_host_clk_disable(odin_dwc_ddata_t *ddata);
int odin_dwc_host_ldo_enable(odin_dwc_ddata_t *ddata);
int odin_dwc_host_ldo_disable(odin_dwc_ddata_t *ddata);
int odin_dwc_host_create_attr(struct platform_device *pdev);
void odin_dwc_host_remove_attr(struct platform_device *pdev);
int odin_dwc_host_usb_notify(struct notifier_block *nb,
		unsigned long action, void *dev);
void odin_dwc_host_controller_recovery(void);

#endif /* ODIN_DWC_HOST_H */
