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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/usb/odin_usb3.h>
#include <linux/odin_pd.h>
#ifdef CONFIG_USB_G_LGE_ANDROID
#include <linux/board_lge.h>
#endif
/* for Slimport Charging */
#if defined (CONFIG_SLIMPORT_ANX7808) || defined (CONFIG_SLIMPORT_ANX7812)
#include <linux/slimport.h>
#endif

#include "phy-lge-usb3.h"

static char *lge_usb3phy_cable_type[] = {
	[BC_CABLETYPE_UNDEFINED] = "NA",
	[BC_CABLETYPE_SDP] = "SDP(USB)",
	[BC_CABLETYPE_DCP] = "DCP(TA)",
	[BC_CABLETYPE_CDP] = "CDP",
	[BC_CABLETYPE_SLIMPORT] = "SLIMPORT",
};

static int dcd_retrycnt = 0;

static void lge_usb3phy_set_curpower(struct lge_usb3phy *lge_phy, unsigned mA, bool drd_flag);

int odin_otg_bc_detect(struct odin_usb3_otg *odin_otg)
{
	struct lge_usb3phy *lge_phy = dev_get_drvdata(odin_otg->usb3phy);

#if defined (CONFIG_SLIMPORT_ANX7808) || defined (CONFIG_SLIMPORT_ANX7812)
	if(slimport_is_connected()) {
		usb_dbg("slimport_is_connected\n");
		lge_phy->odin_otg->cabletype  = BC_CABLETYPE_SLIMPORT;
		lge_usb3phy_set_curpower(lge_phy, 100, true);
		return 0;
	}
#endif

	return lge_usb3phy_bc_detect(lge_phy);
}

void odin_otg_bc_cable_remove(struct odin_usb3_otg *odin_otg)
{
	struct lge_usb3phy *lge_phy = dev_get_drvdata(odin_otg->usb3phy);
	lge_phy->odin_otg->cabletype = BC_CABLETYPE_UNDEFINED;
	lge_usb3phy_set_curpower(lge_phy, 0, true);
}

void odin_otg_tune_phy(struct odin_usb3_otg *odin_otg)
{
	struct lge_usb3phy *lge_phy = dev_get_drvdata(odin_otg->usb3phy);

	/* DRD or OTG driver can overwrite tuning value.
	 * But when the tuning mode is configured as "setting", just leave it as it is.
	 */
	if (lge_phy->tuning_mode == PHY_TUNING_MODE_NORMAL)
		lge_usb3phy_tune_phy(lge_phy, lge_phy->tv_otg);
}

static int lge_usb3phy_cr_read(struct lge_usb3phy *lge_phy, u32 addr)
{
	u32 temp;
	u32 usec = 1000;
	u32 result = 0;

	/* Address set */
	writel(addr, lge_phy->regs + LGE_USB3PHY_CR_DATA_IN);
	temp = readl(lge_phy->regs + LGE_USB3PHY_CR_DATA_IN);

	writel(1, lge_phy->regs + LGE_USB3PHY_CR_CAP_ADDR);
	do {
		result = readl(lge_phy->regs + LGE_USB3PHY_CR_ACK);
		if (result & 1) {
			writel(0, lge_phy->regs + LGE_USB3PHY_CR_CAP_ADDR);
			break;
		}
		udelay(10);
		usec -= 1;
	} while (usec > 0);

	/* Data read */
	writel(1, lge_phy->regs + LGE_USB3PHY_CR_READ);
	usec = 1000;
	do {
		result = readl(lge_phy->regs + LGE_USB3PHY_CR_ACK);
		if (result & 1) {
			writel(0, lge_phy->regs + LGE_USB3PHY_CR_READ);
			break;
		}
		udelay(10);
		usec -= 1;
	} while (usec > 0);
	temp = readl(lge_phy->regs + LGE_USB3PHY_CR_DATA_OUT);
	ud_phy("[%s]Data = 0x%08x \n", __func__, temp);

	return temp;
}

static void lge_usb3phy_cr_write(struct lge_usb3phy *lge_phy, u32 addr, u32 data)
{
	u32 temp;
	u32 usec = 1000;
	u32 result = 0;
	ud_phy("[%s]Data = 0x%08x \n", __func__, data);

	/* Address set */
	writel(addr, lge_phy->regs + LGE_USB3PHY_CR_DATA_IN);
	temp = readl(lge_phy->regs + LGE_USB3PHY_CR_DATA_IN);

	writel(1, lge_phy->regs + LGE_USB3PHY_CR_CAP_ADDR);
	do {
		result = readl(lge_phy->regs + LGE_USB3PHY_CR_ACK);
		if (result & 1) {
			writel(0, lge_phy->regs + LGE_USB3PHY_CR_CAP_ADDR);
			break;
		}
		udelay(10);
		usec -= 1;
	} while (usec > 0);

	/* Data write */
	writel(data, lge_phy->regs + LGE_USB3PHY_CR_DATA_IN);

	writel(1, lge_phy->regs + LGE_USB3PHY_CR_CAP_DATA);
	usec = 1000;
	do {
		result = readl(lge_phy->regs + LGE_USB3PHY_CR_ACK);
		if (result & 1) {
			writel(0, lge_phy->regs + LGE_USB3PHY_CR_CAP_DATA);
			break;
		}
		udelay(10);
		usec -= 1;
	} while (usec > 0);

	writel(1, lge_phy->regs + LGE_USB3PHY_CR_WRITE);
	usec = 1000;
	do {
		result = readl(lge_phy->regs + LGE_USB3PHY_CR_ACK);
		if (result & 1) {
			writel(0, lge_phy->regs + LGE_USB3PHY_CR_WRITE);
			break;
		}
		udelay(10);
		usec -= 1;
	} while (usec > 0);
}

static int lge_usb3phy_regulator_get(struct lge_usb3phy *lge_phy)
{
	struct device *dev = lge_phy->dev;
	int ret;

	ud_phy("[%s] \n", __func__);

	lge_phy->usb30phy = regulator_get(dev,
				"+0.9V_AVDD_USBPHY");
	if (IS_ERR(lge_phy->usb30phy)) {
		ret = PTR_ERR(lge_phy->usb30phy);
		dev_err(dev,
			"unable to get regulator - usb30phy, error: %d\n", ret);
		return ret;
	}
	ret = regulator_enable(lge_phy->usb30phy);
	if (ret)
		return ret;

	lge_phy->usb30io = regulator_get(dev,
				"+3.3V_AVDD_USBPHYIO");
	if (IS_ERR(lge_phy->usb30io)) {
		ret = PTR_ERR(lge_phy->usb30io);
		dev_err(dev,
			"unable to get regulator - usb30io, error: %d\n", ret);
		return ret;
	}
	ret = regulator_enable(lge_phy->usb30io);
	if (ret)
		return ret;

	return 0;
}

static void lge_usb3phy_regulator_put(struct lge_usb3phy *lge_phy)
{
	struct device *dev = lge_phy->dev;
	int ret;

	ret = regulator_disable(lge_phy->usb30phy);
	if (ret)
		dev_err(dev,
			"unable to disable regulator - usb30phy (%d)\n", ret);
	regulator_put(lge_phy->usb30phy);

	ret = regulator_disable(lge_phy->usb30io);
	if (ret)
		dev_err(dev,
			"unable to disable regulator - usb30io (%d)\n", ret);
	regulator_put(lge_phy->usb30io);
}

static void lge_usb3phy_set_curpower(struct lge_usb3phy *lge_phy, unsigned mA, bool drd_flag)
{
	if (lge_phy->odin_otg->cur_power != mA) {
		usb_dbg("Cable Type = [%s] Max Current = [%u]\n",
					lge_usb3phy_cable_type[lge_phy->odin_otg->cabletype], mA);
		lge_phy->odin_otg->cur_power = mA;
	}

	if(drd_flag) {
			atomic_notifier_call_chain(&lge_phy->phy->notifier,
					ODIN_USB_CABLE_EVENT, (void*)lge_phy->odin_otg);
	}
}

static int lge_usb3phy_set_power(struct usb_phy *phy, unsigned mA)
{
	struct odin_usb3_otg * odin_otg = phy_to_usb3otg(phy);
	struct lge_usb3phy *lge_phy = dev_get_drvdata(odin_otg->usb3phy);

	if (lge_phy->odin_otg->cabletype == BC_CABLETYPE_SDP)
		lge_usb3phy_set_curpower(lge_phy, mA, false);
	return 0;
}

#ifdef CONFIG_USB_G_LGE_ANDROID
#define LGE_UDC_BC_DETECT_FACTORY_LOG_CNT	20
#define LGE_UDC_BC_DETECT_FACTORY_MAX_CNT	10
static int factory_retrycnt = 0;

static int lge_usb3phy_bc_detect(struct lge_usb3phy *lge_phy)
{
	u32 out;
	u32 timeout = 10;
	int factory_case = 0;
	enum bc_cabletype_odin_otg t_cabletype = BC_CABLETYPE_UNDEFINED;

	/* [Battery Charging Signals]
	 * IN : 0x02 - DCD(Data Contact Detect) enable
	 *							--> OUT : 0x00 - Non Data / 0x02 - Data
	 * IN : 0x0C - 1st Battery Charger Detection enable
	 *							--> OUT : 0x01 - DCP, CDP / 0x00 - SDP
	 * IN : 0x1C - 2nd Battery Charger Detection enable
	 *							--> OUT : 0x01 - DCP / 0x00 - CDP
	 * IN : 0x01 - Battery Charger Detection disable
	 */

	ud_phy("[%s] IN = 0x%08x, OUT = 0x%08x \n", __func__,
	 		readl(lge_phy->regs + LGE_USB3PHY_BC_IN),
	 		readl(lge_phy->regs + LGE_USB3PHY_BC_OUT));

	/* DCD */
	writel(0x02, lge_phy->regs + LGE_USB3PHY_BC_IN);
	do {
		out = readl(lge_phy->regs + LGE_USB3PHY_BC_OUT);
		if (out & LGE_USB3PHY_BC_OUT_FSVPLUS_MASK) {
			usb_dbg("[%s] DCD Negative, try again \n", __func__);
			dcd_retrycnt++;
			if (dcd_retrycnt >= LGE_USB3PHY_DCD_MAX_CNT) {
				usb_err("[%s] DCD Negative, but not try again(max retry[6])\n", __func__);
				break;
			}
			return -EAGAIN;
		}
		timeout --;
		mdelay(1);
	} while (timeout > 0);

	dcd_retrycnt = 0;

	/* 1st BC detection */
	writel(0x0C, lge_phy->regs + LGE_USB3PHY_BC_IN);
	mdelay(10);

	out = readl(lge_phy->regs + LGE_USB3PHY_BC_OUT);
	ud_phy("[%s] 1 OUT = 0x%08x \n", __func__,
	 		readl(lge_phy->regs + LGE_USB3PHY_BC_OUT));
	if (out & LGE_USB3PHY_BC_OUT_CHGDET0_MASK) {
		/* 2nd BC detection */
		writel(0x1C, lge_phy->regs + LGE_USB3PHY_BC_IN);
		udelay(200);

		out = readl(lge_phy->regs + LGE_USB3PHY_BC_OUT);
		ud_phy("[%s] 2 OUT = 0x%08x \n", __func__,
	 		readl(lge_phy->regs + LGE_USB3PHY_BC_OUT));
		if (out & LGE_USB3PHY_BC_OUT_CHGDET0_MASK) {
			ud_phy("CHGDET0 1, 1 ==> DCP (TA)\n");
			t_cabletype = BC_CABLETYPE_DCP;
		} else {
			ud_phy("CHGDET0 1, 0 ==> CDP\n");
			t_cabletype = BC_CABLETYPE_CDP;
		}
	} else {
		ud_phy("CHGDET0 0 ==> SDP (USB)\n");
		t_cabletype = BC_CABLETYPE_SDP;
	}

	if(t_cabletype != BC_CABLETYPE_SDP) {
		/* Factory cable & No battery */
		if(lge_phy->odin_otg->factory_cable_no_battery) {
			factory_case = 1;
		}
		/* Factory cable & No lcd */
		else if(lge_get_factory_boot()&&(lge_get_lcd_connect()== LGE_LCD_DISCONNECT)) {
			factory_case = 2;
		}

		if(factory_case) {
			factory_retrycnt++;
			if (factory_retrycnt >= LGE_UDC_BC_DETECT_FACTORY_MAX_CNT) {
				t_cabletype = BC_CABLETYPE_SDP;
				usb_err("Facotry%d: Forces to set cabletype %d to BC_CABLETYPE_SDP\n",
					factory_case, t_cabletype);
			}
			else {
				usb_err("Facotry%d: Retry lge_usb3phy_bc_detect count %d cabletype %d\n",
						factory_case, factory_retrycnt, t_cabletype);
				return -EAGAIN;
			}
		}
	}

	factory_retrycnt = 0;

	lge_phy->odin_otg->cabletype  = t_cabletype;
	/* Do not care about power in charger */
	lge_usb3phy_set_curpower(lge_phy, 0, true);

	/* Disable BC detection */
	writel(0x01, lge_phy->regs + LGE_USB3PHY_BC_IN);

	return 0;
}
#else
static int lge_usb3phy_bc_detect(struct lge_usb3phy *lge_phy)
{
	u32 out;
	u32 timeout = 10;

	/* [Battery Charging Signals]
	 * IN : 0x02 - DCD(Data Contact Detect) enable
	 *							--> OUT : 0x00 - Non Data / 0x02 - Data
	 * IN : 0x0C - 1st Battery Charger Detection enable
	 *							--> OUT : 0x01 - DCP, CDP / 0x00 - SDP
	 * IN : 0x1C - 2nd Battery Charger Detection enable
	 *							--> OUT : 0x01 - DCP / 0x00 - CDP
	 * IN : 0x01 - Battery Charger Detection disable
	 */

	ud_phy("[%s] IN = 0x%08x, OUT = 0x%08x \n", __func__,
	 		readl(lge_phy->regs + LGE_USB3PHY_BC_IN),
	 		readl(lge_phy->regs + LGE_USB3PHY_BC_OUT));

	/* DCD */
	writel(0x02, lge_phy->regs + LGE_USB3PHY_BC_IN);
	do {
		out = readl(lge_phy->regs + LGE_USB3PHY_BC_OUT);
		if (out & LGE_USB3PHY_BC_OUT_FSVPLUS_MASK) {
			usb_dbg("[%s] DCD Negative, try again \n", __func__);
			dcd_retrycnt++;
			if (dcd_retrycnt >= LGE_USB3PHY_DCD_MAX_CNT)
				break;
			return -EAGAIN;
		}
		timeout --;
		mdelay(1);
	} while (timeout > 0);

	dcd_retrycnt = 0;

	/* 1st BC detection */
	writel(0x0C, lge_phy->regs + LGE_USB3PHY_BC_IN);
	mdelay(10);

	out = readl(lge_phy->regs + LGE_USB3PHY_BC_OUT);
	ud_phy("[%s] 1 OUT = 0x%08x \n", __func__,
	 		readl(lge_phy->regs + LGE_USB3PHY_BC_OUT));
	if (out & LGE_USB3PHY_BC_OUT_CHGDET0_MASK) {
		/* 2nd BC detection */
		writel(0x1C, lge_phy->regs + LGE_USB3PHY_BC_IN);
		udelay(200);

		out = readl(lge_phy->regs + LGE_USB3PHY_BC_OUT);
		ud_phy("[%s] 2 OUT = 0x%08x \n", __func__,
	 		readl(lge_phy->regs + LGE_USB3PHY_BC_OUT));
		if (out & LGE_USB3PHY_BC_OUT_CHGDET0_MASK) {
			ud_phy("CHGDET0 1, 1 ==> DCP (TA)\n");
			lge_phy->odin_otg->cabletype = BC_CABLETYPE_DCP;
			lge_usb3phy_set_curpower(lge_phy, 1500, true);
		} else {
			ud_phy("CHGDET0 1, 0 ==> CDP\n");
			lge_phy->odin_otg->cabletype  = BC_CABLETYPE_CDP;
			lge_usb3phy_set_curpower(lge_phy, 1500, true);
		}
	} else {
		ud_phy("CHGDET0 0 ==> SDP (USB)\n");
		lge_phy->odin_otg->cabletype  = BC_CABLETYPE_SDP;
		lge_usb3phy_set_curpower(lge_phy, 100, true);
	}

	/* Disable BC detection */
	writel(0x01, lge_phy->regs + LGE_USB3PHY_BC_IN);

	return 0;
}
#endif

static void lge_usb3phy_tune_phy(struct lge_usb3phy *lge_phy, u32 tuning_value)
{
	ud_phy("[%s] tuning_value = 0x%08x \n", __func__, tuning_value);

	writel(LGE_USB3PHY_RX_SQRX_TUNE(tuning_value),
				lge_phy->regs + LGE_USB3PHY_RX_SQRX);

	writel(LGE_USB3PHY_TX_FSLS_TUNE(tuning_value),
				lge_phy->regs + LGE_USB3PHY_TX_FSLS);

	writel(LGE_USB3PHY_TX_HSXV_TUNE(tuning_value),
				lge_phy->regs + LGE_USB3PHY_TX_HSXV);

	writel(LGE_USB3PHY_TX_PREEMPAMP_TUNE(tuning_value),
				lge_phy->regs + LGE_USB3PHY_TX_PREEMPAMP);

	writel(LGE_USB3PHY_TX_PREEMPPULS_TUNE(tuning_value),
				lge_phy->regs + LGE_USB3PHY_TX_PREEMPPULS);

	writel(LGE_USB3PHY_TX_RES_TUNE(tuning_value),
				lge_phy->regs + LGE_USB3PHY_TX_RES);

	writel(LGE_USB3PHY_TX_RISE_TUNE(tuning_value),
				lge_phy->regs + LGE_USB3PHY_TX_RISE);

	writel(LGE_USB3PHY_TX_VREF_TUNE(tuning_value),
				lge_phy->regs + LGE_USB3PHY_TX_VREF);
}

static ssize_t tuning_value_show(struct device *dev,
	struct device_attribute *attr,
	const u32 *buf)
{
	struct lge_usb3phy *lge_phy = dev_get_drvdata(dev);
	return sprintf(buf, " Current tuning value : 0x%08x\n", lge_phy->tv_base);
}

static ssize_t tuning_value_store(struct device* dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct lge_usb3phy *lge_phy = dev_get_drvdata(dev);
	size_t rc;

	sscanf(buf, "%x", &lge_phy->tv_base);
	lge_usb3phy_tune_phy(lge_phy, lge_phy->tv_base);
	rc = strnlen(buf, count);

	return rc;
}

static DEVICE_ATTR(tuning_value, S_IRUGO | S_IWUSR,
				tuning_value_show, tuning_value_store);

static void lge_usb3phy_swreset(struct lge_usb3phy *lge_phy, bool reset)
{
	u32 temp;
	ud_phy("[%s] reset = %d \n", __func__, reset);

	if (reset) {
		temp = readl(lge_phy->crg_regs + LGE_USB3PHY_SWRESET);
		temp |= LGE_USB3PHY_SWRESET_USB_PHY;
		writel(temp, lge_phy->crg_regs + LGE_USB3PHY_SWRESET);

		temp = readl(lge_phy->crg_regs + LGE_USB3PHY_SWRESET);
		temp |= LGE_USB3PHY_SWRESET_USB_CORE;
		writel(temp, lge_phy->crg_regs + LGE_USB3PHY_SWRESET);
	} else {
		temp = readl(lge_phy->crg_regs + LGE_USB3PHY_SWRESET);
		temp &= ~LGE_USB3PHY_SWRESET_USB_CORE;
		writel(temp, lge_phy->crg_regs + LGE_USB3PHY_SWRESET);

		temp = readl(lge_phy->crg_regs + LGE_USB3PHY_SWRESET);
		temp &= ~LGE_USB3PHY_SWRESET_USB_PHY;
		writel(temp, lge_phy->crg_regs + LGE_USB3PHY_SWRESET);
	}
}

static void lge_usb3phy_powerdown(struct lge_usb3phy *lge_phy,
					enum phy_powerdown_mode mode, bool powerdown)
{
	u32 temp;
	ud_phy("[%s] mode %d -> %d \n", __func__, mode, powerdown);

	temp = readl(lge_phy->regs + LGE_USB3PHY_REG1);

	switch (mode) {
	case PD_MODE_HS:
		if (powerdown)
			temp |= LGE_USB3PHY_REG1_TEST_POWERDOWN_HSP;
		else
			temp &= ~LGE_USB3PHY_REG1_TEST_POWERDOWN_HSP;
		break;

	case PD_MODE_SS:
		if (powerdown)
			temp |= LGE_USB3PHY_REG1_TEST_POWERDOWN_SSP;
		else
			temp &= ~LGE_USB3PHY_REG1_TEST_POWERDOWN_SSP;
		break;

	default:
		usb_err("No such powerdown mode \n");
		break;
	}

	writel(temp, lge_phy->regs + LGE_USB3PHY_REG1);
}

static int lge_usb3phy_init(struct usb_phy *phy)
{
	struct odin_usb3_otg * odin_otg = phy_to_usb3otg(phy);
	struct lge_usb3phy *lge_phy = dev_get_drvdata(odin_otg->usb3phy);
	unsigned long flags;
	u32 temp;
	int ret = 0;
	ud_phy("[%s] \n", __func__);

	lge_usb3phy_swreset(lge_phy, 0);

	ret = regulator_enable(lge_phy->usb30io);
	if (ret)
		return ret;

	ret = regulator_enable(lge_phy->usb30phy);
	if (ret)
		return ret;

	clk_enable(lge_phy->clk);

	lge_usb3phy_powerdown(lge_phy, PD_MODE_HS, 0);
	lge_usb3phy_powerdown(lge_phy, PD_MODE_SS, 0);

	udelay(10);

	lge_usb3phy_swreset(lge_phy, 1);

	if (lge_phy->ss_redriver)
		gpio_set_value(lge_phy->rd_gpio, 1);

	spin_lock_irqsave(&lge_phy->lock, flags);

	writel(1, lge_phy->regs + LGE_USB3PHY_RETENABLEN);

	temp = readl(lge_phy->regs + LGE_USB3PHY_REG0);
	temp |= LGE_USB3PHY_REG0_OTGDISABLE0 |
		LGE_USB3PHY_REG0_COMMONONN;
	writel(temp, lge_phy->regs + LGE_USB3PHY_REG0);

	temp = readl(lge_phy->regs + LGE_USB3PHY_REG1);
	temp |= LGE_USB3PHY_REG1_SSC_EN |
		LGE_USB3PHY_REG1_REF_SSP_EN;
	writel(temp, lge_phy->regs + LGE_USB3PHY_REG1);

	lge_usb3phy_tune_phy(lge_phy, lge_phy->tv_base);

	spin_unlock_irqrestore(&lge_phy->lock, flags);
	return 0;
}

static void lge_usb3phy_shutdown(struct usb_phy *phy)
{
	struct odin_usb3_otg * odin_otg = phy_to_usb3otg(phy);
	struct lge_usb3phy *lge_phy = dev_get_drvdata(odin_otg->usb3phy);
	unsigned long flags;
	u32 temp;
	int ret = 0;
	ud_phy("[%s] \n", __func__);

	spin_lock_irqsave(&lge_phy->lock, flags);

	temp = readl(lge_phy->regs + LGE_USB3PHY_REG0);
	temp &= ~LGE_USB3PHY_REG0_COMMONONN;
	writel(temp, lge_phy->regs + LGE_USB3PHY_REG0);

	temp = readl(lge_phy->regs + LGE_USB3PHY_REG1);
	temp &= ~(LGE_USB3PHY_REG1_SSC_EN |
		LGE_USB3PHY_REG1_REF_SSP_EN);
	writel(temp, lge_phy->regs + LGE_USB3PHY_REG1);

	spin_unlock_irqrestore(&lge_phy->lock, flags);

	if (lge_phy->ss_redriver)
		gpio_set_value(lge_phy->rd_gpio, 0);

	lge_usb3phy_powerdown(lge_phy, PD_MODE_HS, 1);
	lge_usb3phy_powerdown(lge_phy, PD_MODE_SS, 1);

	clk_disable(lge_phy->clk);

	ret = regulator_disable(lge_phy->usb30phy);
	if (ret)
		return;
	ret = regulator_disable(lge_phy->usb30io);
	if (ret)
		return;
}

static int lge_usb3phy_redriver_gpio_init(struct platform_device *pdev)
{
	struct lge_usb3phy *lge_phy = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	int ret;

	if (of_property_read_u32(dev->parent->of_node,
				"redriver_gpio", &lge_phy->rd_gpio) != 0)
		return -EINVAL;
	ud_phy("%s: get irq_gpio %d\n", __func__, lge_phy->rd_gpio);

	ret = gpio_is_valid(lge_phy->rd_gpio);
	if (!ret) {
		dev_err(lge_phy->dev, "gpio %d is invalid\n",
			lge_phy->rd_gpio);
		return -EINVAL;
	}

	ret = devm_gpio_request(lge_phy->dev, lge_phy->rd_gpio, "redriver_pm");
	if (ret) {
		dev_err(lge_phy->dev, "Request gpio %d fail : %d\n",
			lge_phy->rd_gpio, ret);
		return -EINVAL;
	}

	gpio_direction_output(lge_phy->rd_gpio, 0);
	return 0;
}

static void lge_usb3phy_of_parser(struct lge_usb3phy *lge_phy)
{
	if (of_property_read_u32(lge_phy->dev->parent->of_node,
				"ss_redriver", &lge_phy->ss_redriver) != 0)
		dev_err(lge_phy->dev, "Can't read of property!\n");
	ud_phy("%s: ss_redriver = %d\n", __func__, lge_phy->ss_redriver);

	if (of_property_read_u32(lge_phy->dev->parent->of_node,
				"tuning_mode", &lge_phy->tuning_mode) != 0)
		dev_err(lge_phy->dev, "Can't read of property!\n");
	ud_phy("%s: tuning_mode = %d\n", __func__, lge_phy->tuning_mode);

	if (of_property_read_u32(lge_phy->dev->parent->of_node,
				"tv_base", &lge_phy->tv_base) != 0)
		dev_err(lge_phy->dev, "Can't read of property!\n");

	if (of_property_read_u32(lge_phy->dev->parent->of_node,
				"tv_otg", &lge_phy->tv_otg) != 0)
		dev_err(lge_phy->dev, "Can't read of property!\n");

	usb_dbg("%s: tuning_value base = 0x%08x, otg = 0x%08x\n",
				__func__, lge_phy->tv_base, lge_phy->tv_otg);
}

static int lge_usb3phy_probe(struct platform_device *pdev)
{
	struct lge_usb3phy *lge_phy;
	struct resource *res_mem;
	struct device *dev = &pdev->dev;
	struct clk *clk;
	volatile u8 __iomem *base;
	int ret = 0;

	lge_phy = devm_kzalloc(dev, sizeof(struct lge_usb3phy), GFP_KERNEL);
	if (!lge_phy){
		dev_err(&pdev->dev, "Memory alloc fail for Odin USB3 PHY\n");
		return -ENOMEM;
	}
	lge_phy->dev = dev;

	res_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_mem) {
		dev_err(&pdev->dev, "Get platform resource failed!\n");
		return -ENXIO;
	}

	base = devm_ioremap_resource(dev, res_mem);
	if (IS_ERR(base)) {
		dev_err(dev, "Get ioremap resource failed!\n");
		return PTR_ERR(base);
	}

	clk = devm_clk_get(dev, "usb30phy_ra_p");
	if (IS_ERR(clk)) {
		dev_err(dev, "Fail to get usb3phy clk\n");
		return PTR_ERR(clk);
	}
	lge_phy->clk = clk;

	lge_phy->odin_otg = odin_usb3_get_otg(pdev);
	lge_phy->odin_otg->usb3phy = lge_phy->dev;
	lge_phy->phy = &lge_phy->odin_otg->phy;

	lge_phy->phy->dev = lge_phy->dev;
	lge_phy->phy->label = "lge-usb3phy";
	lge_phy->phy->init = lge_usb3phy_init;
	lge_phy->phy->shutdown = lge_usb3phy_shutdown;
	lge_phy->phy->set_power = lge_usb3phy_set_power;
	lge_phy->regs = (volatile u32 __iomem *)(base - 0x100);

	spin_lock_init(&lge_phy->lock);

	ret = lge_usb3phy_regulator_get(lge_phy);
	if (ret) {
		dev_err(dev, "Can't get regulator!(%d)\n", ret);
		return ret;
	}

	ret = usb_add_phy(lge_phy->phy, USB_PHY_TYPE_USB3);
	if (ret) {
		dev_err(dev, "usb add phy, err: %d\n", ret);
		goto out;
	}

	ATOMIC_INIT_NOTIFIER_HEAD(&lge_phy->phy->notifier);

	platform_set_drvdata(pdev, lge_phy);

	lge_usb3phy_of_parser(lge_phy);

	if (lge_phy->tuning_mode == PHY_TUNING_MODE_SETTING)
		device_create_file(lge_phy->dev, &dev_attr_tuning_value);

	if (lge_phy->ss_redriver)
		lge_usb3phy_redriver_gpio_init(pdev);


	lge_phy->crg_regs = ioremap(LGE_USB3PHY_CRG_BASE, SZ_1K);

	ud_phy("[%s] Done! \n", __func__);
	return 0;

out:
	lge_usb3phy_regulator_put(lge_phy);
	return ret;
}

static int lge_usb3phy_remove(struct platform_device *pdev)
{
	struct lge_usb3phy *lge_phy = platform_get_drvdata(pdev);

	if (lge_phy->tuning_mode == PHY_TUNING_MODE_SETTING)
		device_remove_file(lge_phy->dev, &dev_attr_tuning_value);
	iounmap(lge_phy->crg_regs);
	lge_usb3phy_regulator_put(lge_phy);
	usb_remove_phy(lge_phy->phy);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

struct platform_driver lge_usb3phy_driver = {
	.probe		= lge_usb3phy_probe,
	.remove		= lge_usb3phy_remove,
	.driver	= {
		.name	= "lge-usb3phy",
		.owner	= THIS_MODULE,
	},
};
module_platform_driver(lge_usb3phy_driver);

MODULE_AUTHOR("Wonsuk Chang <wonsuk.chang@lge.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LGE USB3.0 PHY driver");
