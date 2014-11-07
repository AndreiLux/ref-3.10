/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 *  Author: Wonsuk Chang <wonsuk.chang@lge.com>
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

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/usb.h>
#include <linux/usb/gadget.h>
#include <linux/usb/hcd.h>
#include <linux/usb/otg.h>
#include <linux/usb/odin_usb3.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_data/gpio-odin.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/wakelock.h>
#include <linux/dma-mapping.h>
#include <linux/odin_pd.h>
/* for Slimport Charging */
#if defined (CONFIG_SLIMPORT_ANX7808) || defined (CONFIG_SLIMPORT_ANX7812)
#include <linux/slimport.h>
#endif
#ifdef CONFIG_USB_G_LGE_ANDROID
#include <linux/board_lge.h>
#endif

#include "odin_drd.h"

static u64 odin_drd_dma_mask = DMA_BIT_MASK(32);

static atomic_t rt_count = ATOMIC_INIT(0);

static void odin_drd_vbus_event_handler(struct odin_drd *drd,
											int vbus, int delay_ms);
static void odin_drd_id_event_handler(struct odin_drd *drd, int id);

void odin_drd_dbg_hwparam(struct odin_drd *drd)
{
	u32 hwparams0, hwparams3, hwparams5;

	hwparams0 = readl(drd->global_regs + ODIN_USB3_GHWPARAMS0);
	hwparams3 = readl(drd->global_regs + ODIN_USB3_GHWPARAMS3);
	hwparams5 = readl(drd->global_regs + ODIN_USB3_GHWPARAMS5);

	usb_print("mode=%0x\n", ODIN_USB3_GHWP0_MODE(hwparams0));
	usb_print("num of total eps=%d\n", ODIN_USB3_GHWP3_NUM_EPS(hwparams3));
	usb_print("num of in eps =%d\n",
			ODIN_USB3_GHWP3_NUM_IN_EPS(hwparams3) -1);
	usb_print("dfq_fifo_depth=%d\n",
			ODIN_USB3_GHWP5_DFQ_FIFO_DEPTH(hwparams5));
	usb_print("dwq_fifo_depth=%d\n",
			ODIN_USB3_GHWP5_DWQ_FIFO_DEPTH(hwparams5));
	usb_print("txq_fifo_depth=%d\n",
			ODIN_USB3_GHWP5_TXQ_FIFO_DEPTH(hwparams5));
	usb_print("rxq_fifo_depth=%d\n",
			ODIN_USB3_GHWP5_RXQ_FIFO_DEPTH(hwparams5));
}

void odin_drd_dump_global_regs(struct odin_drd *drd)
{
	usb_print("[[Dump Core Registers]]\n");
	usb_print("GSBUSCFG0 = 0x%08x\n",
			readl(drd->global_regs + ODIN_USB3_GSBUSCFG0));
	usb_print("GSBUSCFG1 = 0x%08x\n",
			readl(drd->global_regs + ODIN_USB3_GSBUSCFG1));
	usb_print("GTXTHRCFG = 0x%08x\n",
			readl(drd->global_regs + ODIN_USB3_GTXTHRCFG));
	usb_print("GRXTHRCFG = 0x%08x\n",
			readl(drd->global_regs + ODIN_USB3_GRXTHRCFG));
	usb_print("GCTL = 0x%08x\n",
			readl(drd->global_regs + ODIN_USB3_GCTL));
	usb_print("USB2PHYCFG0 = 0x%08x\n",
			readl(drd->global_regs +  ODIN_USB3_GUSB2PHYCFG(0)));
	usb_print("GEVTEN = 0x%08x\n",
			readl(drd->global_regs + ODIN_USB3_GEVENT));
	usb_print("GRXFIFOSIZ0 = 0x%08x\n",
			readl(drd->global_regs + ODIN_USB3_GRXFIFOSIZ(0)));
	usb_print("GTXFIFOSIZ0 = 0x%08x\n",
			readl(drd->global_regs + ODIN_USB3_GTXFIFOSIZ(0)));
	usb_print("GTXFIFOSIZ1 = 0x%08x\n",
			readl(drd->global_regs + ODIN_USB3_GTXFIFOSIZ(1)));
	usb_print("GTXFIFOSIZ2 = 0x%08x\n",
			readl(drd->global_regs + ODIN_USB3_GTXFIFOSIZ(2)));
	usb_print("GTXFIFOSIZ3 = 0x%08x\n",
			readl(drd->global_regs + ODIN_USB3_GTXFIFOSIZ(3)));
	usb_print("GUSB2I2CCTL0 = 0x%08x\n",
			readl(drd->global_regs + ODIN_USB3_GUSB2I2CCTL(0)));
	usb_print("GGPIO = 0x%08x\n",
			readl(drd->global_regs + ODIN_USB3_GGPIO));
	usb_print("GUID = 0x%08x\n",
			readl(drd->global_regs + ODIN_USB3_GUID));
	usb_print("GSNPSID = 0x%08x\n",
			readl(drd->global_regs + ODIN_USB3_GSNPSID));
}

void odin_drd_dump_global_regs_reset(struct device *dev)
{
	struct odin_drd *drd = dev_get_drvdata(dev);
	odin_drd_dump_global_regs(drd);

	/* Need to implement USB reset */
}

void odin_drd_dump_otg_regs(struct odin_drd *drd)
{
	usb_print("[[Dump OTG Registers]]\n");
	usb_print("OCFG = 0x%08x\n",
			readl(drd->otg_regs + ODIN_USB3_OCFG));
	usb_print("OCTL = 0x%08x\n",
			readl(drd->otg_regs + ODIN_USB3_OCTL));
	usb_print("OEVT = 0x%08x\n",
			readl(drd->otg_regs + ODIN_USB3_OEVT));
	usb_print("OEVTEN = 0x%08x\n",
			readl(drd->otg_regs + ODIN_USB3_OEVTEN));
	usb_print("OSTS = 0x%08x\n",
			readl(drd->otg_regs + ODIN_USB3_OSTS));
}

static inline struct odin_drd *usb3otg_to_drd(struct odin_usb3_otg *_usb3otg)
{
	return container_of(_usb3otg, struct odin_drd, odin_otg);
}

struct odin_usb3_otg *odin_usb3_get_otg(struct platform_device *child)
{
	struct odin_drd *drd = dev_get_drvdata(child->dev.parent);
	return &drd->odin_otg;
}

int odin_drd_get_state(struct platform_device *child)
{
	struct odin_drd *drd = dev_get_drvdata(child->dev.parent);

	if (drd->phy->state == OTG_STATE_B_PERIPHERAL) {
		if (drd->odin_otg.cabletype == BC_CABLETYPE_SDP ||
				drd->odin_otg.cabletype == BC_CABLETYPE_CDP) {
				return -EBUSY;
		}
	}

	return 0;
}

int odin_otg_vbus_event(struct odin_usb3_otg *odin_otg,
									int vbus, int delay_ms)
{
	struct odin_drd *drd = usb3otg_to_drd(odin_otg);
	odin_drd_vbus_event_handler(drd, vbus, delay_ms);
	return 0;
}

#ifdef CONFIG_USB_G_LGE_ANDROID
int odin_otg_get_vbus_state(struct odin_usb3_otg *odin_otg)
{
	struct odin_drd *drd = usb3otg_to_drd(odin_otg);
	return drd->usb_vbus;
}
#endif

int odin_otg_id_event(struct odin_usb3_otg *odin_otg, int id)
{
	struct odin_drd *drd = usb3otg_to_drd(odin_otg);
	odin_drd_id_event_handler(drd, id);
	return 0;
}

static int odin_drd_handshake(struct odin_drd *drd, u32 reg,
								u32 mask, u32 done, u32 msec)
{
	u32 ret;

	do {
		ret = readl((void *)reg);
		if ((ret & mask) == done)
			return 1;

		mdelay(1);
		msec -= 1;
	} while (msec > 0);

	return 0;
}

static int odin_drd_clk_get(struct platform_device *pdev)
{
	struct clk *clk;
	struct odin_drd *drd = platform_get_drvdata(pdev);

	/* "cclk_usb30" = 40 */
	clk = clk_get(NULL, "cclk_usb30");
	if (IS_ERR(clk))
		return PTR_ERR(clk);
	drd->hclk = clk;

	/* "usb30_susp_clk" = 60 */
	clk = clk_get(NULL, "usb30_susp_clk");
	if (IS_ERR(clk))
		return PTR_ERR(clk);
	drd->sus_clk = clk;

	/* "usb30_ref_hclk" = 68  */
	clk = clk_get(NULL, "usb30_ref_hclk");
	if (IS_ERR(clk))
		return PTR_ERR(clk);
	drd->ref = clk;

	return 0;
}

static int odin_drd_clk_put(struct platform_device *pdev)
{
	struct odin_drd *drd = platform_get_drvdata(pdev);
	struct clk *clk;

	clk = drd->hclk;
	clk_disable_unprepare(clk);
	clk_put(clk);
	drd->hclk = NULL;

	clk = drd->sus_clk;
	clk_disable_unprepare(clk);
	clk_put(clk);
	drd->sus_clk = NULL;

	clk = drd->ref;
	clk_disable_unprepare(clk);
	clk_put(clk);
	drd->ref = NULL;

	return 0;
}

static int odin_drd_clk_on(struct odin_drd *drd)
{
	clk_enable(drd->hclk);
	clk_enable(drd->sus_clk);
	clk_enable(drd->ref);
	return 0;
}

static int odin_drd_clk_off(struct odin_drd *drd)
{
	clk_disable(drd->hclk);
	clk_disable(drd->sus_clk);
	clk_disable(drd->ref);
	return 0;
}

static void odin_drd_power_control(struct odin_drd *drd, int onoff)
{
	switch (drd->pd_state) {
	case DRD_PD_STATE_UNDEFINED:
		ud_core("[%s] pd control is not ready (state = %d) \n",
					__func__, drd->pd_state);
		break;

	case DRD_PD_STATE_INIT:
		if (!onoff) {
			odin_drd_clk_off(drd);
			odin_pd_off(PD_ICS, 2);
			drd->pd_state = DRD_PD_STATE_OFF;
		}
#ifdef CONFIG_USB_G_LGE_ANDROID
		else {
			odin_pd_on(PD_ICS, 2);
			odin_drd_clk_on(drd);
			drd->pd_state = DRD_PD_STATE_ON;
		}
#endif
		break;

	case DRD_PD_STATE_OFF:
		if (onoff) {
			odin_pd_on(PD_ICS, 2);
			odin_drd_clk_on(drd);
			drd->pd_state = DRD_PD_STATE_ON;
		} else {
			ud_core("[%s] pd on fail (state = %d) \n", __func__, drd->pd_state);
		}
		break;

	case DRD_PD_STATE_ON:
		if (!onoff) {
			odin_drd_clk_off(drd);
			odin_pd_off(PD_ICS, 2);
			drd->pd_state = DRD_PD_STATE_OFF;
		} else {
			ud_core("[%s] pd off fail (state = %d) \n", __func__, drd->pd_state);
		}
		break;

	default:
		usb_err("[%s] pd state err(%d)\n", __func__, drd->pd_state);
		break;
	}
}

int odin_drd_hw_init(struct device *dev)
{
	struct odin_drd *drd = dev_get_drvdata(dev);
	u32 gctl = 0;
	u32 susp_clk_freq;
	int ret = 0;

	drd->snpsid = readl(drd->global_regs + ODIN_USB3_GSNPSID);
	ud_core("GSNPSID: 0x%08x \n", drd->snpsid);
	if ((drd->snpsid & 0xffff0000) != 0x55330000) {
		dev_err(drd->dev, "bad value for GSNPSID: 0x%08x!\n", drd->snpsid);
		ret  = -ENXIO;
		return ret;
	}

	gctl = readl(drd->global_regs + ODIN_USB3_GCTL);
	gctl &= ~(ODIN_USB3_GCTL_PRT_CAP_DIR_MASK |
			ODIN_USB3_GCTL_SCALE_DOWN_MASK |
			ODIN_USB3_GCTL_PWR_DN_SCALE_MASK |
			ODIN_USB3_GCTL_U2RSTECN);

	susp_clk_freq = clk_get_rate(drd->sus_clk);
	if (IS_ERR(susp_clk_freq)) {
		dev_err(drd->dev, "Failed to get suspend clock\n");
	} else {
		dev_dbg(drd->dev, "Suspend Clock is %d \n", susp_clk_freq);
		gctl |= ODIN_USB3_GCTL_PWR_DN_SCALE(susp_clk_freq/16000);
	}
	writel(gctl, drd->global_regs + ODIN_USB3_GCTL);
	return 0;
}

void odin_drd_usb2_phy_suspend(struct device *dev, int suspend)
{
	struct odin_drd *drd = dev_get_drvdata(dev);
	u32 usb2phycfg;

	usb2phycfg = readl(drd->global_regs + ODIN_USB3_GUSB2PHYCFG(0));
	if (suspend)
		usb2phycfg |= ODIN_USB3_GUSB2PHYCFG_SUS_PHY;
	else
		usb2phycfg &= ~ODIN_USB3_GUSB2PHYCFG_SUS_PHY;
	writel(usb2phycfg, drd->global_regs + ODIN_USB3_GUSB2PHYCFG(0));
}

void odin_drd_usb3_phy_suspend(struct device *dev, int suspend)
{
	struct odin_drd *drd = dev_get_drvdata(dev);
	u32 pipectl;

	pipectl = readl(drd->global_regs + ODIN_USB3_GUSB3PIPECTL(0));
	if (suspend)
		pipectl |= ODIN_USB3_GUSB3PIPECTL_SUS_PHY;
	else
		pipectl &= ~ODIN_USB3_GUSB3PIPECTL_SUS_PHY;
	writel(pipectl, drd->global_regs + ODIN_USB3_GUSB3PIPECTL(0));
}

void odin_drd_ena_eventbuf(struct device *dev)
{
	struct odin_drd *drd = dev_get_drvdata(dev);
	u32 eventsiz;

	eventsiz = readl(drd->global_regs + ODIN_USB3_GEVNTSIZ(0));
	eventsiz &= ~ODIN_USB3_GEVNTSIZ_INT_MASK;
	writel(eventsiz, drd->global_regs + ODIN_USB3_GEVNTSIZ(0));
}

void odin_drd_dis_eventbuf(struct device *dev)
{
	struct odin_drd *drd = dev_get_drvdata(dev);
	u32 eventsiz;

	eventsiz = readl(drd->global_regs + ODIN_USB3_GEVNTSIZ(0));
	eventsiz |= ODIN_USB3_GEVNTSIZ_INT_MASK;
	writel(eventsiz, drd->global_regs + ODIN_USB3_GEVNTSIZ(0));
}

void odin_drd_flush_eventbuf(struct device *dev)
{
	struct odin_drd *drd = dev_get_drvdata(dev);
	u32 cnt;

	cnt = readl(drd->global_regs + ODIN_USB3_GEVNTCOUNT(0));
	writel(cnt, drd->global_regs + ODIN_USB3_GEVNTCOUNT(0));
}

int odin_drd_get_eventbuf(struct device *dev)
{
	struct odin_drd *drd = dev_get_drvdata(dev);

	return (readl(drd->global_regs + ODIN_USB3_GEVNTCOUNT(0)) &
			ODIN_USB3_GEVNTCOUNT_MASK);
}

void odin_drd_set_eventbuf(struct device *dev, int cnt)
{
	struct odin_drd *drd = dev_get_drvdata(dev);

	writel(cnt, drd->global_regs + ODIN_USB3_GEVNTCOUNT(0));
}

void odin_drd_init_eventbuf(struct device *dev, dma_addr_t event_buf_dma)
{
	struct odin_drd *drd = dev_get_drvdata(dev);

	writel(event_buf_dma & 0xffffffffU,
				drd->global_regs + ODIN_USB3_GEVNTADRLO(0));
#ifdef CONFIG_ODIN_USB3_64_BIT_ARCH
	writel(event_buf_dma >> 32U & 0xffffffffU,
				drd->global_regs + ODIN_USB3_GEVNTADRHI(0));
#else
	writel(0, drd->global_regs + ODIN_USB3_GEVNTADRHI(0));
#endif
	writel(ODIN_USB3_EVENT_BUF_SIZE << 2,
				drd->global_regs + ODIN_USB3_GEVNTSIZ(0));
	writel(0, drd->global_regs + ODIN_USB3_GEVNTCOUNT(0));
}

void odin_drd_phy_sel_intf(struct device *dev, int intf)
{
	struct odin_drd *drd = dev_get_drvdata(dev);
	u32 gusb2phycfg;

	ud_core("[%s] set to %d \n", __func__, intf);

	gusb2phycfg = readl(drd->global_regs + ODIN_USB3_GUSB2PHYCFG(0));
	gusb2phycfg &= ~(	ODIN_USB3_GUSB2PHYCFG_USB_TRD_TIM_MASK);

	switch (intf) {
	case 1:	/* 16bit UTMI+ */
		gusb2phycfg |= ODIN_USB3_GUSB2PHYCFG_USB_TRD_TIM(5);
		gusb2phycfg |= ODIN_USB3_GUSB2PHYCFG_16B_PHY_IF;
		break;

	case 2:	/* 8bit UTMI+ */
		gusb2phycfg |= ODIN_USB3_GUSB2PHYCFG_USB_TRD_TIM(9);
		gusb2phycfg &= ~(ODIN_USB3_GUSB2PHYCFG_16B_PHY_IF);
		break;

	default:
		usb_err("[%s] wrong case(%d) \n", __func__, intf);
		gusb2phycfg |= ODIN_USB3_GUSB2PHYCFG_USB_TRD_TIM(5);
		gusb2phycfg |= ODIN_USB3_GUSB2PHYCFG_16B_PHY_IF;
		break;
	}

	writel(gusb2phycfg, drd->global_regs + ODIN_USB3_GUSB2PHYCFG(0));
}

static void odin_drd_softrst(struct odin_drd *drd, bool reset)
{
	u32 gctl;
	u32 gusb2phycfg;
	u32 gusb3pipectl;

	ud_core("[%s] reset = %d \n", __func__, reset);

	if (reset) {
		gctl = readl(drd->global_regs + ODIN_USB3_GCTL);
		gctl |= ODIN_USB3_GCTL_CORE_SOFT_RST;
		writel(gctl, drd->global_regs + ODIN_USB3_GCTL);

		gusb2phycfg = readl(drd->global_regs + ODIN_USB3_GUSB2PHYCFG(0));
		gusb2phycfg |= ODIN_USB3_GUSB2PHYCFG_PHY_SOFT_RST;
		writel(gusb2phycfg, drd->global_regs + ODIN_USB3_GUSB2PHYCFG(0));

		gusb3pipectl = readl(drd->global_regs + ODIN_USB3_GUSB3PIPECTL(0));
		gusb3pipectl |= ODIN_USB3_GUSB3PIPECTL_PHY_SOFT_RST;
		writel(gusb3pipectl, drd->global_regs + ODIN_USB3_GUSB3PIPECTL(0));
	} else {
		gusb2phycfg = readl(drd->global_regs + ODIN_USB3_GUSB2PHYCFG(0));
		gusb2phycfg &= ~ODIN_USB3_GUSB2PHYCFG_PHY_SOFT_RST;
		writel(gusb2phycfg, drd->global_regs + ODIN_USB3_GUSB2PHYCFG(0));

		gusb3pipectl = readl(drd->global_regs + ODIN_USB3_GUSB3PIPECTL(0));
		gusb3pipectl &= ~ODIN_USB3_GUSB3PIPECTL_PHY_SOFT_RST;
		writel(gusb3pipectl, drd->global_regs + ODIN_USB3_GUSB3PIPECTL(0));

		gctl = readl(drd->global_regs + ODIN_USB3_GCTL);
		gctl &= ~ODIN_USB3_GCTL_CORE_SOFT_RST;
		writel(gctl, drd->global_regs + ODIN_USB3_GCTL);
	}
}

void odin_drd_phy_init(struct odin_drd *drd)
{
	u32 gusb2phycfg;

	odin_drd_softrst(drd, 1);

	if (drd->phy_state == DRD_PHY_STATE_SHUTDOWN) {
		usb_phy_init(drd->phy);
		drd->phy_state = DRD_PHY_STATE_INIT;
	}

	odin_drd_softrst(drd, 0);

	gusb2phycfg = readl(drd->global_regs + ODIN_USB3_GUSB2PHYCFG(0));
	gusb2phycfg &= ~(ODIN_USB3_GUSB2PHYCFG_ENBL_SLP_M);
	writel(gusb2phycfg, drd->global_regs + ODIN_USB3_GUSB2PHYCFG(0));

	odin_drd_phy_sel_intf(drd->dev, 2);
}

void odin_drd_phy_exit(struct odin_drd *drd)
{
	u32 gusb2phycfg;

	gusb2phycfg = readl(drd->global_regs + ODIN_USB3_GUSB2PHYCFG(0));
	gusb2phycfg |= ODIN_USB3_GUSB2PHYCFG_ENBL_SLP_M;
	writel(gusb2phycfg, drd->global_regs + ODIN_USB3_GUSB2PHYCFG(0));

	ud_core("gusb2phycfg = 0x%08x | gusb3pipectl = 0x%08x \n",
			readl(drd->global_regs + ODIN_USB3_GUSB2PHYCFG(0)),
			readl(drd->global_regs + ODIN_USB3_GUSB3PIPECTL(0)));

	if (drd->phy_state == DRD_PHY_STATE_INIT) {
		usb_phy_shutdown(drd->phy);
		drd->phy_state = DRD_PHY_STATE_SHUTDOWN;
	}
}

int odin_drd_usb_id_state(struct usb_phy *phy)
{
	struct odin_usb3_otg *odin_otg =
			container_of(phy, struct odin_usb3_otg, phy);
	struct odin_drd *drd = usb3otg_to_drd(odin_otg);
	return drd->usb_id_state;
}

int odin_drd_bus_config(struct odin_drd *drd)
{
	writel(ODIN_USB3_GSBUSCFG0_INT_DMA_BURST_INCR |
			ODIN_USB3_GSBUSCFG0_INT_DMA_BURST_INCR16,
				drd->global_regs + ODIN_USB3_GSBUSCFG0);
	ud_core("ODIN_USB3_GSBUSCFG0 = 0x%08x\n",
			readl(drd->global_regs + ODIN_USB3_GSBUSCFG0));

	writel(ODIN_USB3_GSBUSCFG1_BRLIMIT(3),
				drd->global_regs + ODIN_USB3_GSBUSCFG1);
	ud_core("ODIN_USB3_GSBUSCFG1 = 0x%08x\n",
			readl(drd->global_regs + ODIN_USB3_GSBUSCFG1));
	return 0;
}

static void odin_drd_reset_adp(struct odin_drd *drd)
{
	u32 gctl = 0;

	writel(0, drd->otg_regs + ODIN_USB3_OEVTEN);
	writel(0, drd->otg_regs + ODIN_USB3_OCTL);

	gctl = readl(drd->global_regs + ODIN_USB3_GCTL);
	gctl |= ODIN_USB3_GCTL_PRT_CAP_DIR_OTG;
	writel(gctl, drd->global_regs + ODIN_USB3_GCTL);
}

static void odin_drd_set_mode(struct odin_drd *drd, int mode)
{
	u32 gctl = 0;
	u32 octl = 0;
	gctl = readl(drd->global_regs + ODIN_USB3_GCTL);

	switch (mode){
	case DRD_MODE_HOST:	/* Host */
		gctl |= ODIN_USB3_GCTL_PRT_CAP_DIR_HOST;

		octl = readl(drd->otg_regs + ODIN_USB3_OCTL);
		octl &= ~ODIN_USB3_OCTL_PERI_MODE;
		writel(octl, drd->otg_regs + ODIN_USB3_OCTL);
		break;
	case DRD_MODE_DEVICE: /* Device */
		gctl |= ODIN_USB3_GCTL_PRT_CAP_DIR_DEV;

		octl = readl(drd->otg_regs + ODIN_USB3_OCTL);
		octl |= ODIN_USB3_OCTL_PERI_MODE;
		writel(octl, drd->otg_regs + ODIN_USB3_OCTL);
		break;
	case DRD_MODE_OTG: /* OTG */
		gctl |= ODIN_USB3_GCTL_PRT_CAP_DIR_OTG;
		break;
	default:
		break;
	}
	writel(gctl, drd->global_regs + ODIN_USB3_GCTL);
}

#ifdef CONFIG_USB_G_LGE_ANDROID
#define ODIN_USB3_XHCI_START_DELAY		300
#endif

static int odin_drd_xhci_start(struct odin_drd *drd, int powered)
{
	struct device *dev = drd->dev;
	u32 octl = 0;
	u32 osts = 0;
	int flg;
	int ret = 0;

	ud_core("[%s] powered = %d \n", __func__, powered);

	if (!drd->xhci)
		return -EINVAL;

	wake_lock(&drd->host_wl);

	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		dev_err(drd->dev,
			"failed to runtime resume hcd, ret=%d\n", ret);
		return -EBUSY;
	}

	ret = odin_drd_hw_init(dev);
	if (ret) {
		dev_err(dev, "DRD hw init, err: %d\n", ret);
		goto err;
	}

	odin_otg_tune_phy(&drd->odin_otg);

	odin_drd_set_mode(drd, DRD_MODE_HOST);

	if (!powered) {
		if (drd->ic_ops->otg_vbus)
			drd->ic_ops->otg_vbus(&drd->odin_otg, 1);
	}

	ret = platform_device_add(drd->xhci);
	if (ret) {
		dev_err(dev, "Unable to add platform device for Odin DRD xHCI \n");
		goto err;
	}

	odin_drd_bus_config(drd);

#ifdef CONFIG_USB_G_LGE_ANDROID
	/*
	 * Though XHCI power would be set by now, but some delay is
	 * required for XHCI controller before setting OTG Port Power
	 * TODO: Tune this delay
	 */
	msleep(ODIN_USB3_XHCI_START_DELAY);
#else
	msleep(100);
#endif

	/* Power the port */
	osts = readl(drd->otg_regs + ODIN_USB3_OSTS);
	flg = odin_drd_handshake(drd, (u32)drd->otg_regs + ODIN_USB3_OSTS,
			ODIN_USB3_OSTS_XHCI_PRT_PWR,
			ODIN_USB3_OSTS_XHCI_PRT_PWR,
			200);
	if (flg) {
		ud_core("Port is powered by xhci-hcd\n");
		/* Set port power control bit */
		octl = readl(drd->otg_regs + ODIN_USB3_OCTL);
		octl |= ODIN_USB3_OCTL_PRT_PWR_CTL;
		writel(octl, drd->otg_regs + ODIN_USB3_OCTL);
	} else {
		ud_core("Port is not powered by xhci-hcd\n");
	}

	return 0;
err:

	if (!powered) {
		if (drd->ic_ops->otg_vbus)
			drd->ic_ops->otg_vbus(&drd->odin_otg, 0);
	}
	pm_runtime_put_sync(dev);
	return ret;
}

static int odin_drd_xhci_stop(struct odin_drd *drd, int powered)
{
	struct device *dev = drd->dev;
	int ret;

	if (!drd->xhci)
		return -EINVAL;

	platform_device_del(drd->xhci);

	ret = pm_runtime_put_sync(dev);
	if (ret < 0)
		dev_err(drd->dev, "pm_runtime_put_sync fail!\n");

	if (!powered) {
		if (drd->ic_ops->otg_vbus)
			drd->ic_ops->otg_vbus(&drd->odin_otg, 0);
	}

	wake_unlock(&drd->host_wl);
	return 0;
}

static int odin_drd_udc_start(struct odin_drd *drd)
{
	struct usb_gadget *gadget = drd->otg->gadget;
	struct device *dev = drd->dev;
	int ret = 0;
	int ret_r = 0;

	if (!gadget)
		return -ENODEV;

	wake_lock_timeout(&drd->udc_wl, 2*HZ);

#if defined (CONFIG_SLIMPORT_ANX7808) || defined (CONFIG_SLIMPORT_ANX7812)
	if(slimport_is_connected()) {
		ret = odin_otg_bc_detect(&drd->odin_otg);
		usb_dbg("SLIMPORT Conntected\n");
		return ret;
	}
#endif

	ret = pm_runtime_get_sync(dev);
	if (ret)
		ud_core("[%s]pm_runtime_get_sync - return %d\n", __func__, ret);

	if (drd->phy_state == DRD_PHY_STATE_INIT) {
		ret = odin_otg_bc_detect(&drd->odin_otg);
		if (ret) {
			/* Keep udc_wl wakelock to execute delayed workqueue */
			ret_r = -EAGAIN;
			goto out;
		}

		switch (drd->odin_otg.cabletype) {
		case BC_CABLETYPE_UNDEFINED:
			usb_dbg("Cable Type Undifined\n");
			ret_r = 0;
			goto out;

		case BC_CABLETYPE_SDP:
			usb_dbg("SDP Conntected\n");
			break;

		case BC_CABLETYPE_DCP:
			usb_dbg("DCP Conntected\n");
			ret_r = 0;
			goto out;

		case BC_CABLETYPE_CDP:
			usb_dbg("CDP Conntected\n");
			break;

		default:
			usb_dbg("Unknown cable type\n");
			ret_r = 0;
			goto out;
		}
	}

	ret = odin_drd_hw_init(dev);
	if (ret) {
		dev_err(dev, "DRD hw init, err: %d\n", ret);
		ret_r = ret;
		/* Keep udc_wl wakelock to execute delayed workqueue */
		goto out;
	}

	odin_drd_set_mode(drd, DRD_MODE_DEVICE);

	odin_drd_bus_config(drd);

	ret = usb_gadget_vbus_connect(gadget);
	if (ret)
		dev_err(drd->dev, "usb_gadget_vbus_connect fail!\n");

	return 0;

out:
	ret = pm_runtime_put_sync(dev);
	if (ret < 0)
		dev_err(drd->dev, "pm_runtime_put_sync fail!(%d)\n", ret);

	return ret_r;
}

static void odin_drd_udc_stop(struct odin_drd *drd)
{
	struct usb_gadget *gadget = drd->otg->gadget;
	struct device *dev = drd->dev;
	int ret = 0;

	if (!gadget)
		return;

	switch (drd->phy_state) {
	case DRD_PHY_STATE_INIT:
		if (drd->odin_otg.cabletype == BC_CABLETYPE_SDP ||
				drd->odin_otg.cabletype == BC_CABLETYPE_CDP) {
			ret = usb_gadget_vbus_disconnect(gadget);
			if (ret)
				dev_err(drd->dev, "usb_gadget_vbus_disconnect fail!\n");

			ret = pm_runtime_put_sync(dev);
			if (ret < 0)
				dev_err(drd->dev, "pm_runtime_put_sync fail!\n");
		}
		break;

	case DRD_PHY_STATE_SHUTDOWN:
		/* PHY state has already changed to shutdown(e.g., DCP connected).
		 * Do nothing here */
		break;

	default:
		ret = usb_gadget_vbus_disconnect(gadget);
		if (ret)
			dev_err(drd->dev, "usb_gadget_vbus_disconnect fail!\n");

		ret = pm_runtime_put_sync(dev);
		if (ret < 0)
			dev_err(drd->dev, "pm_runtime_put_sync fail!\n");
		break;
	}

	if (drd->phy)
		odin_otg_bc_cable_remove(&drd->odin_otg);

	wake_lock_timeout(&drd->udc_wl, HZ);
}

static void odin_drd_work(struct work_struct *work)
{
	struct odin_drd *drd = container_of(work,
					struct odin_drd, work.work);
	unsigned long flags;
	int vbus, usb_id, ret = 0;
	enum usb_otg_state state;
	enum usb_otg_state next_state = 0;

	spin_lock_irqsave(&drd->lock, flags);
	state = drd->phy->state;
	vbus = drd->usb_vbus;
	usb_id = drd->usb_id_state;
	spin_unlock_irqrestore(&drd->lock, flags);
	ud_otg("IN - OTG state = %d \n", drd->phy->state);

	switch (state){
	case OTG_STATE_UNDEFINED:
		if (usb_id)
			next_state = OTG_STATE_B_IDLE;
		else
			next_state = OTG_STATE_A_IDLE;

		queue_delayed_work(drd->wq, &drd->work, 0);
		break;

	case OTG_STATE_B_PERIPHERAL:
		if (!vbus) {
			odin_drd_udc_stop(drd);
			next_state = OTG_STATE_B_IDLE;
			queue_delayed_work(drd->wq, &drd->work, 0);
			ud_core("OTG_STATE_B_PERIPHERAL -> OTG_STATE_B_IDLE \n");
		} else {
			next_state = OTG_STATE_B_PERIPHERAL;
		}
		break;

	case OTG_STATE_A_HOST:
		if (usb_id) {
			ud_otg("drd->phy_state = %d \n", drd->phy_state);
			odin_drd_xhci_stop(drd, drd->powered_otg);
			next_state = OTG_STATE_A_IDLE;
			queue_delayed_work(drd->wq, &drd->work, 0);
			ud_core("OTG_STATE_A_HOST -> OTG_STATE_A_IDLE \n");
		} else {
			next_state = OTG_STATE_A_HOST;
		}
		break;

	case OTG_STATE_B_IDLE:
		if (usb_id) {
			if (vbus) {
				ret = odin_drd_udc_start(drd);
				if (!ret) {
					next_state = OTG_STATE_B_PERIPHERAL;
					queue_delayed_work(drd->wq, &drd->work, 0);
					ud_core("OTG_STATE_B_IDLE -> OTG_STATE_B_PERIPHERAL \n");
				} else if (ret == -EAGAIN) {
					queue_delayed_work(drd->wq, &drd->work,
											msecs_to_jiffies(200));
					ud_core("odin_drd_udc_start - RETRY \n");
				} else {
					usb_dbg("odin_drd_udc_start - fail return %d\n", ret);
				}
			} else {
				next_state = OTG_STATE_B_IDLE;
			}
		} else {
			next_state = OTG_STATE_A_IDLE;
			queue_delayed_work(drd->wq, &drd->work, 0);
			ud_core("OTG_STATE_B_IDLE -> OTG_STATE_A_IDLE \n");
		}
		break;

	case OTG_STATE_A_IDLE:
		if (vbus) {
			if (usb_id) {
				next_state = OTG_STATE_B_IDLE;
				queue_delayed_work(drd->wq, &drd->work, 0);
				ud_core("OTG_STATE_A_IDLE -> OTG_STATE_B_IDLE \n");
			} else {
				usb_dbg("Powered OTG \n");
				ret = odin_drd_xhci_start(drd, 1);
				if (!ret) {
					next_state = OTG_STATE_A_HOST;
					queue_delayed_work(drd->wq, &drd->work, 0);
					ud_core("OTG_STATE_A_IDLE -> OTG_STATE_A_HOST \n");
				} else if (ret == -EBUSY) {
					next_state = OTG_STATE_A_IDLE;
					queue_delayed_work(drd->wq, &drd->work,
									msecs_to_jiffies(200));
					ud_core("OTG_STATE_A_IDLE -> RETRY \n");
				} else {
					usb_dbg("odin_drd_xhci_start(powered) - fail return %d\n", ret);
				}
				drd->powered_otg = 1;
			}
		} else {
			if (!usb_id) {
				ret = odin_drd_xhci_start(drd, 0);
				if (!ret) {
					next_state = OTG_STATE_A_HOST;
					queue_delayed_work(drd->wq, &drd->work, 0);
					ud_core("OTG_STATE_A_IDLE -> OTG_STATE_A_HOST \n");
				} else if (ret == -EBUSY) {
					next_state = OTG_STATE_A_IDLE;
					queue_delayed_work(drd->wq, &drd->work,
									msecs_to_jiffies(200));
					ud_core("OTG_STATE_A_IDLE -> RETRY \n");
				} else {
					usb_dbg("odin_drd_xhci_start - fail return %d\n", ret);
				}
				drd->powered_otg = 0;
			} else {
				next_state = OTG_STATE_A_IDLE;
			}
		}
		break;

	default:
		dev_err(drd->dev, "OTG state = %d \n", drd->phy->state);
		break;

	}

	spin_lock_irqsave(&drd->lock, flags);
	drd->phy->state = next_state;
	spin_unlock_irqrestore(&drd->lock, flags);
	ud_otg("OUT - OTG state = %d \n", drd->phy->state);
}

static void odin_drd_vbus_event_handler(struct odin_drd *drd,
											int vbus, int delay_ms)
{
	unsigned long flags;

	if (drd->usb_mode == USB3_MODE_DRD) {
		bool do_work = false;

		usb_dbg("[%s]vbus = %d / delay(ms) = %d \n", __func__, vbus, delay_ms);
		spin_lock_irqsave(&drd->lock, flags);

		if (drd->usb_vbus != vbus) {
			drd->usb_vbus = vbus;
			do_work = true;
		}
		spin_unlock_irqrestore(&drd->lock, flags);

		if(do_work)
			queue_delayed_work(drd->wq, &drd->work, msecs_to_jiffies(delay_ms));
	}
}

static void odin_drd_id_event_handler(struct odin_drd *drd, int id)
{
	unsigned long flags;

	if (drd->usb_mode == USB3_MODE_DRD) {
		bool do_work = false;
		usb_dbg("[%s]id = %d \n", __func__, id);
		spin_lock_irqsave(&drd->lock, flags);

		if (drd->usb_id_state != id) {
			drd->usb_id_state = id;
			do_work = true;
		}
		spin_unlock_irqrestore(&drd->lock, flags);

		if(do_work)
			queue_delayed_work(drd->wq, &drd->work, 0);
	}
}

static int odin_drd_init_adp(struct odin_drd *drd)
{
	u32 adpevten = 0;
	int ret = 0;

	odin_drd_reset_adp(drd);

	adpevten = ODIN_USB3_ADPEVTEN_ADP_PRB_EVNT_EN |
		ODIN_USB3_ADPEVTEN_ADP_TMOUT_EVNT_EN;

	writel(adpevten, drd->otg_regs + ODIN_USB3_ADPEVTEN);
	writel(ODIN_USB3_ADPCTL_ENA_PRB | ODIN_USB3_ADPCTL_ADP_EN,
				drd->otg_regs + ODIN_USB3_ADPCTL);
	ret = odin_drd_handshake(drd,
				(u32)drd->otg_regs + ODIN_USB3_ADPCTL, ODIN_USB3_ADPCTL_WB, 0, 100);
	if (!ret)
		dev_err(drd->dev, "Init ADP handshake timeout \n");

	writel(0, drd->otg_regs + ODIN_USB3_OCFG);
	writel(0, drd->otg_regs + ODIN_USB3_OEVTEN);
	writel(0xffffffff, drd->otg_regs + ODIN_USB3_OEVT);
	writel(ODIN_USB3_OEVT_CONN_ID_STS_CHNG_EVNT, drd->otg_regs + ODIN_USB3_OEVTEN);
	writel(ODIN_USB3_OCTL_PERI_MODE, drd->otg_regs + ODIN_USB3_OCTL);

	return ret;
}

static int odin_drd_set_peripheral(struct usb_otg *otg,
				struct usb_gadget *gadget)
{
	struct odin_usb3_otg *odin_otg;
	struct odin_drd *drd;
	unsigned long flags;

	if (!otg)
		return -ENODEV;

	odin_otg = container_of(otg, struct odin_usb3_otg, otg);
	drd = usb3otg_to_drd(odin_otg);

	if (!gadget) {
		drd->otg->gadget = NULL;
		return -ENODEV;
	}

	spin_lock_irqsave(&drd->lock, flags);
	drd->otg->gadget = gadget;
	spin_unlock_irqrestore(&drd->lock, flags);
	return 0;
}

static int odin_drd_set_host(struct usb_otg *otg, struct usb_bus *host)
{
	struct odin_usb3_otg *odin_otg;
	struct odin_drd *drd;
	unsigned long flags;

	if (!otg)
		return -ENODEV;

	odin_otg = container_of(otg, struct odin_usb3_otg, otg);
	drd = usb3otg_to_drd(odin_otg);

	if (!host) {
		drd->otg->host = NULL;
		return -ENODEV;
	}

	spin_lock_irqsave(&drd->lock, flags);
	drd->otg->host = host;
	spin_unlock_irqrestore(&drd->lock, flags);
	return 0;
}

#ifdef CONFIG_USB_G_LGE_ANDROID
#define LGE_USB_NORMAL				0
#define LGE_USB_FACTORY_NO_BATTERY	1
#define LGE_USB_FACTORY_NO_LCD		2
#define LGE_USB_FACTORY_ONLY		3

static int odin_drd_factory_case(struct odin_drd *drd)
{
	int is_factory = 0;

	/* Factory cable & No battery */
	if(drd->odin_otg.factory_cable_no_battery) {
		is_factory = LGE_USB_FACTORY_NO_BATTERY;
	}
	/* Factory cable */
	else if(lge_get_factory_boot()) {
		/* Factory cable & No lcd */
		if(lge_get_lcd_connect() == LGE_LCD_DISCONNECT) {
			is_factory = LGE_USB_FACTORY_NO_LCD;
		}
		else {
			is_factory = LGE_USB_FACTORY_ONLY;
		}
	}
	else {
		is_factory = LGE_USB_NORMAL;
	}

	if(is_factory) {
		usb_print("[%s] is_factory(%d)\n", __func__, is_factory);
	}

	return is_factory;
}

static void odin_drd_usb3phy_register(struct odin_drd *drd, int factory_case)
{
	struct usb_phy *usb3phy = NULL;
	ud_core("[%s] \n", __func__);

	usb3phy = devm_usb_get_phy(drd->dev, USB_PHY_TYPE_USB3);
	if (!usb3phy) {
		usb_err("Phy is not ready! \n");
		return;
	}

	/* Do not disable usb phy in factory case */
	if(factory_case) {
		usb_phy_init(drd->phy);
		drd->phy_state = DRD_PHY_STATE_INIT;
	}
	else {
		usb_phy_shutdown(drd->phy);
		drd->phy_state = DRD_PHY_STATE_SHUTDOWN;
	}
}
#else
static void odin_drd_usb3phy_register(struct odin_drd *drd)
{
	struct usb_phy *usb3phy = NULL;
	ud_core("[%s] \n", __func__);

	usb3phy = devm_usb_get_phy(drd->dev, USB_PHY_TYPE_USB3);
	if (!usb3phy) {
		usb_err("Phy is not ready! \n");
		return;
	}

	usb_phy_shutdown(drd->phy);
	drd->phy_state = DRD_PHY_STATE_SHUTDOWN;
}
#endif

void odin_drd_usbinit(struct odin_drd *drd)
{
	if (drd->usb_mode == USB3_MODE_DRD) {

/* Do not disable usb phy & PD in factory case */
#ifdef CONFIG_USB_G_LGE_ANDROID
		bool is_vbus_det = false;

		int factory_case = odin_drd_factory_case(drd);

		odin_drd_usb3phy_register(drd, factory_case);
		drd->pd_state = DRD_PD_STATE_INIT;
		if(factory_case) {
			odin_drd_power_control(drd, 1);
		}
		else {
			odin_drd_power_control(drd, 0);
		}

		if (drd->odin_otg.charger_dev) {
			if (drd->ic_ops->vbus_det)
				is_vbus_det = drd->ic_ops->vbus_det(&drd->odin_otg);
		} else {
			usb_err("Charger driver is not registered \n");
		}

		if(factory_case == LGE_USB_FACTORY_ONLY) {
			if(!is_vbus_det) { /* No vbus event */
				usb_phy_shutdown(drd->phy);
				drd->phy_state = DRD_PHY_STATE_SHUTDOWN;

				odin_drd_power_control(drd, 0);

				mdelay(50);
			}
		}

		if (drd->odin_otg.adc_dev) {
			if (drd->ic_ops->id_det)
				drd->ic_ops->id_det(&drd->odin_otg);
		} else {
			usb_err("ADC driver is not registered \n");
		}
#else
		odin_drd_usb3phy_register(drd);
		drd->pd_state = DRD_PD_STATE_INIT;
		odin_drd_power_control(drd, 0);

		if (drd->odin_otg.charger_dev) {
			if (drd->ic_ops->vbus_det)
				drd->ic_ops->vbus_det(&drd->odin_otg);
		} else {
			usb_err("Charger driver is not registered \n");
		}

		if (drd->odin_otg.adc_dev) {
			if (drd->ic_ops->id_det)
				drd->ic_ops->id_det(&drd->odin_otg);
		} else {
			usb_err("ADC driver is not registered \n");
		}
#endif

	} else if (drd->usb_mode == USB3_MODE_DEVICE_ONLY) {
		if (!drd->otg->gadget) {
			usb_err("No gadget! \n");
			return;
		}
		odin_drd_set_mode(drd, DRD_MODE_DEVICE);
		odin_drd_bus_config(drd);
		usb_gadget_vbus_connect(drd->otg->gadget);
	}
}

#ifdef CONFIG_ODIN_USB3_DBG_SYSFS
u32 ud_loglevel;
static ssize_t odin_drd_show_loglevel(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ud_loglevel);
}

static ssize_t odin_drd_store_loglevel(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t rc;
	int temp = 0;
	sscanf(buf, "%d", &temp);
	rc = strnlen(buf, count);
	ud_loglevel = temp;
	return rc;
}

static DEVICE_ATTR(loglevel, S_IWUSR | S_IRUGO,
				odin_drd_show_loglevel, odin_drd_store_loglevel);

static ssize_t odin_drd_show_otgstate(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct odin_drd *drd = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", drd->phy->state);
}

static ssize_t odin_drd_store_otgstate(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t count)
{
	struct odin_drd *drd = dev_get_drvdata(dev);
	ssize_t rc;
	unsigned long flags;
	int temp = 0;

	spin_lock_irqsave(&drd->lock, flags);
	sscanf(buf, "%d", &temp);
	rc = strnlen(buf, count);
	drd->phy->state = temp;
	spin_unlock_irqrestore(&drd->lock, flags);

	queue_delayed_work(drd->wq, &drd->work, 0);
	return rc;
}

static DEVICE_ATTR(otgstate, S_IWUSR | S_IRUGO,
				odin_drd_show_otgstate, odin_drd_store_otgstate);

static ssize_t odin_drd_show_eventcnt(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t count)
{
	return sprintf(buf, "%d\n", odin_drd_get_eventbuf(dev));
}

static DEVICE_ATTR(eventcnt, S_IRUGO, odin_drd_show_eventcnt, NULL);

static ssize_t odin_drd_show_otgreg(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t count)
{
	struct odin_drd *drd = dev_get_drvdata(dev);
	odin_drd_dump_otg_regs(drd);
	return 0;
}

static DEVICE_ATTR(otgdump, S_IRUGO, odin_drd_show_otgreg, NULL);

static ssize_t odin_drd_show_globalreg(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t count)
{
	struct odin_drd *drd = dev_get_drvdata(dev);
	odin_drd_dump_global_regs(drd);
	return 0;
}

static DEVICE_ATTR(globaldump, S_IRUGO, odin_drd_show_globalreg, NULL);
#endif /* CONFIG_ODIN_USB3_DBG_SYSFS */

static ssize_t odin_drd_store_usbinit(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t count)
{
	struct odin_drd *drd = dev_get_drvdata(dev);
	ssize_t rc;
	int temp = 0;

	sscanf(buf, "%d", &temp);
	rc = strnlen(buf, count);
	if (!drd->usb_init && temp) {
		odin_drd_usbinit(drd);
		drd->usb_init = 1;
	}
	return rc;
}

static DEVICE_ATTR(usbinit, S_IWUSR, NULL, odin_drd_store_usbinit);

static struct attribute *odin_drd_attributes[] = {
#ifdef CONFIG_ODIN_USB3_DBG_SYSFS
	&dev_attr_loglevel.attr,
	&dev_attr_otgstate.attr,
	&dev_attr_eventcnt.attr,
	&dev_attr_otgdump.attr,
	&dev_attr_globaldump.attr,
#endif
	&dev_attr_usbinit.attr,
	NULL
};

static const struct attribute_group odin_drd_attr_group = {
	.name = NULL,
	.attrs = odin_drd_attributes,
};

static void odin_drd_init_check(struct work_struct *work)
{
	struct odin_drd *drd =
		container_of(work, struct odin_drd, init_work.work);
	ud_core("[%s] \n", __func__);

	/* This routin is to prepare for
	 * in case Android usb init does not call 'usbinit'
	 */
	if (!drd->usb_init) {
		usb_err("[%s] DRD forced to enable \n", __func__);
		odin_drd_usbinit(drd);
		drd->usb_init = 1;
	}
}

static int odin_drd_add_udc(struct platform_device *pdev)
{
	struct odin_drd *drd = platform_get_drvdata(pdev);
	struct platform_device	*udc;
	int ret;

	udc = platform_device_alloc("odin-udc", -1);
	if (!udc) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "Fail to memory alloc for Odin DRD UDC \n");
		return ret;
	}

	udc->dev.parent = &pdev->dev;
	dma_set_coherent_mask(&udc->dev, pdev->dev.coherent_dma_mask);
	udc->dev.dma_mask = pdev->dev.dma_mask;

	drd->udc_res[0].start = drd->mem_res->start + ODIN_USB3_DEVICE_REG_START;
	drd->udc_res[0].end = drd->mem_res->start + ODIN_USB3_DEVICE_REG_END;
	drd->udc_res[0].flags = drd->mem_res->flags;
	drd->udc_res[0].name = drd->mem_res->name;

	drd->udc_res[1].start = drd->irq_res->start;
	drd->udc_res[1].end = drd->irq_res->end;
	drd->udc_res[1].flags = drd->irq_res->flags;
	drd->udc_res[1].name = drd->irq_res->name;

	ret = platform_device_add_resources(udc, drd->udc_res,
								ARRAY_SIZE(drd->udc_res));
	if (ret) {
		dev_err(&pdev->dev, "couldn't add resources to UDC device\n");
		goto failed;
	}

	ret = platform_device_add(udc);
	if (ret) {
		dev_err(&pdev->dev, "Unable to add platform device for Odin DRD UDC \n");
		goto failed;
	}
	return 0;

failed:
	platform_device_put(udc);
	return ret;
}

static int odin_drd_add_xhci(struct platform_device *pdev)
{
	struct odin_drd *drd = platform_get_drvdata(pdev);
	struct platform_device	*xhci;
	int ret;

	xhci = platform_device_alloc("xhci-hcd", -1);
	if (!xhci) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "Fail to memory alloc for Odin DRD xHCI \n");
		return ret;
	}

	dma_set_coherent_mask(&xhci->dev, pdev->dev.coherent_dma_mask);
	xhci->dev.dma_mask = pdev->dev.dma_mask;

	drd->xhci_res[0].start = drd->mem_res->start + ODIN_USB3_XHCI_REG_START;
	drd->xhci_res[0].end = drd->mem_res->start + ODIN_USB3_XHCI_REG_END;
	drd->xhci_res[0].flags = drd->mem_res->flags;
	drd->xhci_res[0].name = drd->mem_res->name;

	drd->xhci_res[1].start = drd->irq_res->start;
	drd->xhci_res[1].end = drd->irq_res->end;
	drd->xhci_res[1].flags = drd->irq_res->flags;
	drd->xhci_res[1].name = drd->irq_res->name;

	drd->xhci = xhci;

	ret = platform_device_add_resources(xhci, drd->xhci_res,
								ARRAY_SIZE(drd->xhci_res));
	if (ret) {
		dev_err(&pdev->dev, "couldn't add resources to xHCI device\n");
		goto failed;
	}

	return 0;

failed:
	platform_device_put(xhci);
	return ret;
}

static int odin_drd_add_usb3phy(struct platform_device *pdev)
{
	struct odin_drd *drd = platform_get_drvdata(pdev);
	struct platform_device	*usb3phy;
	int ret;

	usb3phy = platform_device_alloc("lge-usb3phy", -1);
	if (!usb3phy) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "Fail to memory alloc for LGE USB3PHY \n");
		return ret;
	}

	usb3phy->dev.parent = &pdev->dev;
	dma_set_coherent_mask(&usb3phy->dev, pdev->dev.coherent_dma_mask);
	usb3phy->dev.dma_mask = pdev->dev.dma_mask;

	ret = platform_device_add_resources(usb3phy, drd->pmem_res, 1);
	if (ret) {
		dev_err(&pdev->dev, "couldn't add resources to LGE USB3PHY device\n");
		goto failed;
	}

	ret = platform_device_add(usb3phy);
	if (ret) {
		dev_err(&pdev->dev, "Unable to add platform device for LGE USB3PHY \n");
		goto failed;
	}
	return 0;

failed:
	platform_device_put(usb3phy);
	return ret;
}

static int odin_drd_probe(struct platform_device *pdev)
{
	struct odin_drd *drd;
	struct device *dev = &pdev->dev;
	int ret;

	drd = devm_kzalloc(dev, sizeof(struct odin_drd), GFP_KERNEL);
	if (!drd){
		dev_err(&pdev->dev, "Fail to memory alloc for Odin DRD \n");
		return -ENOMEM;
	}
	drd->dev = dev;
	platform_set_drvdata(pdev, drd);

	drd->irq =  platform_get_irq(pdev, 0);
	if (drd->irq < 0) {
		dev_err(dev, "unable to get irq\n");
		return drd->irq;
	}

	drd->mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!drd->mem_res) {
		dev_err(dev, "fail to get memory resource\n");
		return -ENXIO;
	}

	drd->irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!drd->irq_res) {
		dev_err(dev, "fail to get irq resource\n");
		return -ENXIO;
	}

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &odin_drd_dma_mask;
	if (!pdev->dev.coherent_dma_mask)
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	drd->global_res.start = drd->mem_res->start + ODIN_USB3_GLOBAL_REG_START;
	drd->global_res.end = drd->mem_res->start + ODIN_USB3_GLOBAL_REG_END;

	if (!devm_request_mem_region(dev, drd->global_res.start,
					resource_size(&drd->global_res), "odin-drd-global")) {
		dev_err(dev, "request_mem_region() failed!\n");
		return -ENOMEM;
	}

	drd->global_regs = devm_ioremap_nocache(dev, drd->global_res.start,
					 resource_size(&drd->global_res));
	if (!drd->global_regs) {
		dev_err(dev, "ioremap_nocache() failed!\n");
		return -ENOMEM;
	}

	drd->otg_res.start = drd->mem_res->start + ODIN_USB3_OTG_REG_START;
	drd->otg_res.end = drd->mem_res->start + ODIN_USB3_OTG_REG_END;

	if (!devm_request_mem_region(dev, drd->otg_res.start,
					resource_size(&drd->otg_res), "odin-drd-otg")) {
		dev_err(dev, "request_mem_region() failed!\n");
		return -ENOMEM;
	}

	drd->otg_regs = devm_ioremap_nocache(dev, drd->otg_res.start,
					 resource_size(&drd->otg_res));
	if (!drd->otg_regs) {
		dev_err(dev, "ioremap_nocache() failed!\n");
		return -ENOMEM;
	}

	drd->pmem_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!drd->pmem_res) {
		dev_err(dev, "fail to get memory resource\n");
		return -ENXIO;
	}

	drd->otg = &drd->odin_otg.otg;
	drd->phy = &drd->odin_otg.phy;
	drd->otg->set_peripheral = odin_drd_set_peripheral;
	drd->otg->set_host = odin_drd_set_host;
	drd->otg->phy = drd->phy;
	drd->phy->otg = drd->otg;
	drd->phy->state = OTG_STATE_UNDEFINED;
	drd->usb_id_state = 1; /* Initialize to B dev */
	drd->usb_init = 0;
	drd->phy_state = DRD_PHY_STATE_UNDEFINED;
	drd->pd_state = DRD_PD_STATE_UNDEFINED;

	ret = odin_drd_hw_init(dev);
	if (ret) {
		dev_err(dev, "usb hw init, err: %d\n", ret);
		return ret;
	}

	spin_lock_init(&drd->lock);
	wake_lock_init(&drd->host_wl, WAKE_LOCK_SUSPEND, "odin-otg");
	wake_lock_init(&drd->udc_wl, WAKE_LOCK_SUSPEND, "odin-udc");

	INIT_DELAYED_WORK(&drd->work, odin_drd_work);
	drd->wq = create_freezable_workqueue("odin_drd");
	if (!drd->wq) {
		dev_err(dev, "fail to create workqueue\n");
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&drd->init_work, odin_drd_init_check);
	schedule_delayed_work(&drd->init_work, msecs_to_jiffies(20000));

	ret = odin_usb3_otg_en_gpio_init(pdev);
	if (ret) {
		dev_err(dev, "usb3 init, err: %d\n",
			ret);
		goto out1;
	}

	ret = odin_drd_clk_get(pdev);
	if (ret){
		dev_err(dev, "Can't get clock!\n");
		goto out1;
	}

	odin_drd_dump_global_regs(drd);

#ifdef CONFIG_ODIN_USB3_DBG_SYSFS
	/* odin_drd_dump_global_regs(drd); */
	if (of_property_read_u32(pdev->dev.of_node,
				"log_level", &ud_loglevel) != 0)
		dev_err(dev, "Can't read of property! \n");
#endif

	ret = sysfs_create_group(&pdev->dev.kobj, &odin_drd_attr_group);
	if (ret) {
		dev_err(dev, "Can't register sysfs attributes!\n");
		goto out2;
	}

	if (of_property_read_u32(pdev->dev.of_node,
				"usb_mode", &drd->usb_mode) != 0)
		dev_err(dev, "Can't read of property! \n");

	odin_drd_init_adp(drd);
	odin_drd_add_usb3phy(pdev);
	odin_drd_add_udc(pdev);

	drd->ic_ops = &usb3_ic_ops;

	if (drd->usb_mode == USB3_MODE_DRD) {
		odin_drd_add_xhci(pdev);
		pm_runtime_set_active(&pdev->dev);
		pm_runtime_enable(&pdev->dev);
	}

	return 0;

out2:
	odin_drd_clk_put(pdev);

out1:
	cancel_delayed_work_sync(&drd->init_work);
	cancel_delayed_work_sync(&drd->work);
	destroy_workqueue(drd->wq);
	wake_lock_destroy(&drd->host_wl);
	wake_lock_destroy(&drd->udc_wl);
	platform_set_drvdata(pdev, NULL);
	return ret;
}

static int odin_drd_remove(struct platform_device *pdev)
{
	struct odin_drd *drd = platform_get_drvdata(pdev);

	sysfs_remove_group(&pdev->dev.kobj, &odin_drd_attr_group);

	cancel_delayed_work_sync(&drd->init_work);

	cancel_delayed_work_sync(&drd->work);

	destroy_workqueue(drd->wq);

	wake_lock_destroy(&drd->host_wl);

	wake_lock_destroy(&drd->udc_wl);

	odin_drd_clk_put(pdev);

	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM
 static int odin_drd_suspend(struct device *dev)
{
	struct odin_drd *drd = dev_get_drvdata(dev);
#ifdef CONFIG_PM_RUNTIME
	ud_core("%s: usage_count = %d, rt = %d\n",
		      __func__, atomic_read(&dev->power.usage_count),
		      atomic_read(&rt_count));

	if (pm_runtime_suspended(dev)) {
		dev_dbg(dev, "DRD is runtime suspended\n");
		return 0;
	}

	if (atomic_read(&rt_count) > 0) {
		ud_core("[%s] DRD already suspended \n", __func__);
		return 0;
	}
#endif

	disable_irq(drd->irq);
	odin_drd_phy_exit(drd);
	odin_drd_power_control(drd, 0);

	atomic_inc(&rt_count);
	return 0;
}

static int odin_drd_resume(struct device *dev)
{
	struct odin_drd *drd = dev_get_drvdata(dev);
	struct irq_desc *desc;
#ifdef CONFIG_PM_RUNTIME
	ud_core("%s: usage_count = %d, rt = %d\n",
		      __func__, atomic_read(&dev->power.usage_count),
		      atomic_read(&rt_count));
#endif

	odin_drd_power_control(drd, 1);
	odin_drd_phy_init(drd);

	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	desc = irq_to_desc(drd->irq);
	if (desc && desc->depth > 0)
		enable_irq(drd->irq);

	atomic_dec(&rt_count);
	return 0;
}
#else
#define odin_drd_suspend NULL
#define odin_drd_resume NULL
#endif /* CONFIG_PM */

#ifdef CONFIG_PM_RUNTIME
static int odin_drd_runtime_suspend(struct device *dev)
{
	struct odin_drd *drd = dev_get_drvdata(dev);
	ud_core("%s: usage_count = %d, rt = %d\n",
		      __func__, atomic_read(&dev->power.usage_count),
		      atomic_read(&rt_count));

	disable_irq(drd->irq);
	odin_drd_phy_exit(drd);
	odin_drd_power_control(drd, 0);

	atomic_inc(&rt_count);
	return 0;
}

static int odin_drd_runtime_resume(struct device *dev)
{
	struct odin_drd *drd = dev_get_drvdata(dev);
	struct irq_desc *desc;
	ud_core("%s: usage_count = %d, rt = %d\n",
		      __func__, atomic_read(&dev->power.usage_count),
		      atomic_read(&rt_count));

	odin_drd_power_control(drd, 1);
	odin_drd_phy_init(drd);
	desc = irq_to_desc(drd->irq);
	if (desc && desc->depth > 0)
		enable_irq(drd->irq);

	atomic_dec(&rt_count);
	return 0;
}
#else
#define odin_drd_runtime_suspend NULL
#define odin_drd_runtime_resume NULL
#endif /* CONFIG_PM_RUNTIME */

static const struct dev_pm_ops odin_drd_pm_ops = {
	.suspend_late		= odin_drd_suspend,
	.resume_early		= odin_drd_resume,
	.runtime_suspend = odin_drd_runtime_suspend,
	.runtime_resume = odin_drd_runtime_resume,
};

static struct of_device_id odin_usb_match[] = {
	{ .compatible = "odin_usb3_drd"},
};

struct platform_driver odin_drd_driver = {
	.probe		= odin_drd_probe,
	.remove		= odin_drd_remove,
	.driver	= {
		.name	= "odin-drd",
		.owner	= THIS_MODULE,
		.of_match_table=of_match_ptr(odin_usb_match),
		.pm = &odin_drd_pm_ops
	},
};
module_platform_driver(odin_drd_driver);

MODULE_AUTHOR("Wonsuk Chang <wonsuk.chang@lge.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LGE USB3.0 DRD driver");
