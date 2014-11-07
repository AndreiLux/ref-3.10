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
 #ifndef __DRIVERS_USB_DWC3_ODIN_DRD__H
#define __DRIVERS_USB_DWC3_ODIN_DRD__H

#include <linux/wakelock.h>
#include <linux/usb/odin_usb3.h>

#define ODIN_USB3_XHCI_REG_START		0x0
#define ODIN_USB3_XHCI_REG_END		0x7FFF
#define ODIN_USB3_GLOBAL_REG_START	0xC100
#define ODIN_USB3_GLOBAL_REG_END		0xC6FF
#define ODIN_USB3_DEVICE_REG_START	0xC700
#define ODIN_USB3_DEVICE_REG_END		0xCBFF
#define ODIN_USB3_OTG_REG_START		0xCC00
#define ODIN_USB3_OTG_REG_END			0xCCFF

/* Global Common Register */
/** Global SoC Bus Configuration Register 0 (GSBUSCFG0) **/
#define ODIN_USB3_GSBUSCFG0	0x00
#define ODIN_USB3_GSBUSCFG0_INT_DMA_BURST_INCR		0x00000001
#define ODIN_USB3_GSBUSCFG0_INT_DMA_BURST_INCR4	0x00000002
#define ODIN_USB3_GSBUSCFG0_INT_DMA_BURST_INCR8	0x00000004
#define ODIN_USB3_GSBUSCFG0_INT_DMA_BURST_INCR16	0x00000008

/** Global SoC Bus Configuration Register 1 (GSBUSCFG1) **/
#define ODIN_USB3_GSBUSCFG1	0x04
#define ODIN_USB3_GSBUSCFG1_BRLIMIT(n)	((n) << 8)

/** Global Tx Threshold Control Register (GTXTHRCFG) **/
#define ODIN_USB3_GTXTHRCFG	0x08
#define ODIN_USB3_GTXTHRCFG_MAX_BURSTSIZE_MASK	(0x7f << 16)
#define ODIN_USB3_GTXTHRCFG_MAX_BURSTSIZE(x)	((x) << 16)
#define ODIN_USB3_GTXTHRCFG_PKTCNT_MASK	(0xf << 24)
#define ODIN_USB3_GTXTHRCFG_PKTCNT(x)	((x) << 24)
#define ODIN_USB3_GTXTHRCFG_PKTCNT_EN	(1 << 29)

/** Global Rx Threshold Control Register (GRXTHRCFG) **/
#define ODIN_USB3_GRXTHRCFG	0x0C
#define ODIN_USB3_GRXTHRCFG_MAX_BURSTSIZE_MASK	(0x1f << 19)
#define ODIN_USB3_GRXTHRCFG_MAX_BURSTSIZE(x)	((x) << 19)
#define ODIN_USB3_GRXTHRCFG_PKTCNT_MASK	(0xf << 24)
#define ODIN_USB3_GRXTHRCFG_PKTCNT(x)	((x) << 24)
#define ODIN_USB3_GRXTHRCFG_PKTCNT_EN	(1 << 29)

/** Global Core Control Register (GCTL) **/
#define ODIN_USB3_GCTL	0x10
#define ODIN_USB3_GCTL_SCALE_DOWN_MASK	(0x3 << 4)
#define ODIN_USB3_GCTL_CORE_SOFT_RST	(1 << 11)
#define ODIN_USB3_GCTL_PRT_CAP_DIR_MASK	(0x3 << 12)
#define ODIN_USB3_GCTL_PRT_CAP_DIR_SHIFT 12
#define ODIN_USB3_GCTL_PRT_CAP_DIR_HOST (1 << 12)
#define ODIN_USB3_GCTL_PRT_CAP_DIR_DEV (2 << 12)
#define ODIN_USB3_GCTL_PRT_CAP_DIR_OTG (3 << 12)
#define ODIN_USB3_GCTL_U2RSTECN	(1 << 16)
#define ODIN_USB3_GCTL_PWR_DN_SCALE_MASK	(0x1fff << 19)
#define ODIN_USB3_GCTL_PWR_DN_SCALE(n)	((n) << 19)
#define ODIN_USB3_GCTL_GBL_HIBERNATION_EN 0x2

#define ODIN_USB3_GEVENT	0x14
/** Global Status Register (GSTS) **/
#define ODIN_USB3_GSTS		0x18
/** Global User Control Register 1 (GUCTL1) **/
#define ODIN_USB3_GUCTL1	0x1C

#define ODIN_USB3_GSNPSID	0x20
#define ODIN_USB3_GGPIO	0x24
#define ODIN_USB3_GUID		0x28
#define ODIN_USB3_GUCTL	0x2C

/*Hardware Parameters 0 Register (GHWPARAMS0) */
#define ODIN_USB3_GHWPARAMS0		0x40
#define ODIN_USB3_GHWP0_MODE_MASK (0x7 << 0)
#define ODIN_USB3_GHWP0_MODE(n) 	\
			(((n) & ODIN_USB3_GHWP0_MODE_MASK) >> 0)

#define ODIN_USB3_GHWPARAMS1		0x44
#define ODIN_USB3_GHWPARAMS2		0x48

 /*Hardware Parameters 3 Register (GHWPARAMS3) */
#define ODIN_USB3_GHWPARAMS3		0x4C
#define ODIN_USB3_GHWP3_NUM_EPS_MASK (0x3f << 12)
#define ODIN_USB3_GHWP3_NUM_EPS(n)	\
			(((n) & ODIN_USB3_GHWP3_NUM_EPS_MASK) >> 12)
#define ODIN_USB3_GHWP3_NUM_IN_EPS_MASK (0x1f << 18)
#define ODIN_USB3_GHWP3_NUM_IN_EPS(n)	\
			(((n) & ODIN_USB3_GHWP3_NUM_IN_EPS_MASK) >> 18)

#define ODIN_USB3_GHWPARAMS4		0x50

 /*Hardware Parameters 5 Register (GHWPARAMS5) */
#define ODIN_USB3_GHWPARAMS5		0x54
#define ODIN_USB3_GHWP5_RXQ_FIFO_DEPTH_MASK (0x3f << 4)
#define ODIN_USB3_GHWP5_RXQ_FIFO_DEPTH(n)	\
			(((n) & ODIN_USB3_GHWP5_RXQ_FIFO_DEPTH_MASK) >> 4)
#define ODIN_USB3_GHWP5_TXQ_FIFO_DEPTH_MASK (0x3f << 10)
#define ODIN_USB3_GHWP5_TXQ_FIFO_DEPTH(n)	\
			(((n) & ODIN_USB3_GHWP5_TXQ_FIFO_DEPTH_MASK) >> 10)
#define ODIN_USB3_GHWP5_DWQ_FIFO_DEPTH_MASK (0x3f << 16)
#define ODIN_USB3_GHWP5_DWQ_FIFO_DEPTH(n)	\
			(((n) & ODIN_USB3_GHWP5_DWQ_FIFO_DEPTH_MASK) >> 16)
#define ODIN_USB3_GHWP5_DFQ_FIFO_DEPTH_MASK (0x3f << 22)
#define ODIN_USB3_GHWP5_DFQ_FIFO_DEPTH(n)	\
			(((n) & ODIN_USB3_GHWP5_DFQ_FIFO_DEPTH_MASK) >> 22)

#define ODIN_USB3_GHWPARAMS6		0x58
#define ODIN_USB3_GHWPARAMS7		0x5C

/* PHY Register */
#define ODIN_USB3_GUSB2PHYCFG(a)	(0x100 + ((a) * 0x04))
#define ODIN_USB3_GUSB2PHYCFG_16B_PHY_IF	(1 << 3)
#define ODIN_USB3_GUSB2PHYCFG_SUS_PHY	(1 << 6)
#define ODIN_USB3_GUSB2PHYCFG_ENBL_SLP_M	(1 << 8)
#define ODIN_USB3_GUSB2PHYCFG_USB_TRD_TIM_MASK (0xf << 10)
#define ODIN_USB3_GUSB2PHYCFG_USB_TRD_TIM(n)	((n) << 10)
#define ODIN_USB3_GUSB2PHYCFG_PHY_SOFT_RST	(1 << 31)

#define ODIN_USB3_GUSB2I2CCTL(a)	(0x140 + ((a) * 0x04))

#define ODIN_USB3_GUSB3PIPECTL(a)	(0x1C0 + ((a) * 0x04))
#define ODIN_USB3_GUSB3PIPECTL_SUS_PHY	(1 << 17)
#define ODIN_USB3_GUSB3PIPECTL_PHY_SOFT_RST	(1 << 31)

#define ODIN_USB3_GTXFIFOSIZ(a)	(0x200 + ((a) * 0x04))
#define ODIN_USB3_GRXFIFOSIZ(a)	(0x280 + ((a) * 0x04))

#define ODIN_USB3_GEVNTADRLO(n)	(0x300 + (n * 0x10))
#define ODIN_USB3_GEVNTADRHI(n)	(0x304 + (n * 0x10))
#define ODIN_USB3_GEVNTSIZ(n)		(0x308 + (n * 0x10))
#define ODIN_USB3_GEVNTSIZ_INT_MASK	(1 << 31)
#define ODIN_USB3_GEVNTCOUNT(n)	(0x30c + (n * 0x10))
#define ODIN_USB3_GEVNTCOUNT_MASK	0xffff

/* OTG Register */
#define ODIN_USB3_OCFG	0x00

#define ODIN_USB3_OCTL	0x04
#define ODIN_USB3_OCTL_PRT_PWR_CTL (1 << 5)
#define ODIN_USB3_OCTL_PERI_MODE	(1 << 6)

#define ODIN_USB3_OEVT	0x08
#define ODIN_USB3_OEVT_CONN_ID_STS_CHNG_EVNT	(1 << 24)

#define ODIN_USB3_OSTS	0x10
#define ODIN_USB3_OSTS_CONN_ID_STS     (1 << 0)
#define ODIN_USB3_OSTS_XHCI_PRT_PWR	(1 << 3)

#define ODIN_USB3_OEVTEN	0x0c

#define ODIN_USB3_ADPCFG	0x20

#define ODIN_USB3_ADPCTL	0x24
#define ODIN_USB3_ADPCTL_WB	(1 << 24)
#define ODIN_USB3_ADPCTL_ADP_EN	(1 << 26)
#define ODIN_USB3_ADPCTL_ENA_PRB	(1 << 28)

#define ODIN_USB3_ADPEVT	0x28

#define ODIN_USB3_ADPEVTEN	0x2c
#define ODIN_USB3_ADPEVTEN_ADP_TMOUT_EVNT_EN	(1 << 26)
#define ODIN_USB3_ADPEVTEN_ADP_PRB_EVNT_EN	(1 << 28)

enum odin_usb3_mode {
	USB3_MODE_DRD = 1,
	USB3_MODE_DEVICE_ONLY,
};

enum drd_mode {
	DRD_MODE_HOST = 1,
	DRD_MODE_DEVICE,
	DRD_MODE_OTG,
};

enum drd_phy_state {
	DRD_PHY_STATE_UNDEFINED,
	DRD_PHY_STATE_INIT,
	DRD_PHY_STATE_SHUTDOWN,
};

enum drd_pd_state {
	DRD_PD_STATE_UNDEFINED,
	DRD_PD_STATE_INIT,
	DRD_PD_STATE_ON,
	DRD_PD_STATE_OFF,
};

struct odin_drd{
	struct odin_usb3_otg odin_otg;
	struct usb_otg *otg;
	struct usb_phy *phy;
	struct device *dev;

	struct delayed_work work;
	struct delayed_work init_work;
	struct workqueue_struct *wq;

	spinlock_t lock;

	struct clk *hclk;
	struct clk *sus_clk;
	struct clk *ref;

	struct resource *mem_res;
	struct resource *pmem_res;
	struct resource *irq_res;
	volatile u8 __iomem *global_regs;
	volatile u8 __iomem *otg_regs;
	struct resource global_res;
	struct resource otg_res;
	struct resource xhci_res[2];
	struct resource udc_res[2];

	struct wake_lock host_wl;
	struct wake_lock udc_wl;

	int irq;
	u32 snpsid;

	enum drd_phy_state phy_state;
	enum drd_pd_state pd_state;

	unsigned int usb_id_state	: 1;
	unsigned int usb_vbus		: 1;
	unsigned int usb_init		: 1;
	unsigned int powered_otg	: 1;

	enum odin_usb3_mode usb_mode;

	struct odin_usb3_ic_ops *ic_ops;

	struct platform_device	*xhci;
};

#endif /*__DRIVERS_USB_DWC3_ODIN_DRD__H */
