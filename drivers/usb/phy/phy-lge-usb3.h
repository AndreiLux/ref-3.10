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
 #ifndef __DRIVERS_USB_PHY_LGE_USB3_H
#define __DRIVERS_USB_PHY_LGE_USB3_H

#define LGE_USB3PHY_DCD_MAX_CNT	6

#define LGE_USB3PHY_CRG_BASE 0x3FF00000
#define LGE_USB3PHY_SWRESET 0x9C
#define LGE_USB3PHY_SWRESET_USB_CORE	(1 << 7)
#define LGE_USB3PHY_SWRESET_USB_PHY	(1 << 8)

/* RETENABLEN */
#define LGE_USB3PHY_RETENABLEN 0x210

/* CR INTERFACE */
#define LGE_USB3PHY_CR_ACK		0x240
#define LGE_USB3PHY_CR_CAP_ADDR	0x244
#define LGE_USB3PHY_CR_CAP_DATA	0x248
#define LGE_USB3PHY_CR_DATA_IN	0x24C
#define LGE_USB3PHY_CR_DATA_OUT	0x250
#define LGE_USB3PHY_CR_READ		0x254
#define LGE_USB3PHY_CR_WRITE		0x258

/* TX Tuning Register */
#define LGE_USB3PHY_TX_VREF		0x23C
#define LGE_USB3PHY_TX_RISE		0x238
#define LGE_USB3PHY_TX_RES			0x234
#define LGE_USB3PHY_TX_PREEMPPULS	0x230
#define LGE_USB3PHY_TX_PREEMPAMP	0x22C
#define LGE_USB3PHY_TX_HSXV		0x228
#define LGE_USB3PHY_TX_FSLS		0x224
#define LGE_USB3PHY_TX_DEEMPH_3P5DB		0x270
#define LGE_USB3PHY_TX_DEEMPH_6DB		0x274
#define LGE_USB3PHY_TX_SWING_FULL		0x278
#define LGE_USB3PHY_TX_BOOST_LVL		0x28C

/* RX Tuning Register */
#define LGE_USB3PHY_RX_SQRX		0x220

/* Tuning API */
#define LGE_USB3PHY_RX_SQRX_TUNE_MASK 0x7
#define LGE_USB3PHY_RX_SQRX_TUNE(n) ((n) & LGE_USB3PHY_RX_SQRX_TUNE_MASK)
#define LGE_USB3PHY_TX_FSLS_TUNE_MASK (0xf << 3)
#define LGE_USB3PHY_TX_FSLS_TUNE(n) (((n) & LGE_USB3PHY_TX_FSLS_TUNE_MASK) >> 3)
#define LGE_USB3PHY_TX_HSXV_TUNE_MASK (0x3 << 7)
#define LGE_USB3PHY_TX_HSXV_TUNE(n) (((n) & LGE_USB3PHY_TX_HSXV_TUNE_MASK) >> 7)
#define LGE_USB3PHY_TX_PREEMPAMPTUNE_MASK (0x3 << 9)
#define LGE_USB3PHY_TX_PREEMPAMP_TUNE(n) \
				(((n) & LGE_USB3PHY_TX_PREEMPAMPTUNE_MASK) >> 9)
#define LGE_USB3PHY_TX_PREEMPPULS_TUNE_MASK (0x1 << 11)
#define LGE_USB3PHY_TX_PREEMPPULS_TUNE(n) \
				(((n) & LGE_USB3PHY_TX_PREEMPPULS_TUNE_MASK) >> 11)
#define LGE_USB3PHY_TX_RES_TUNE_MASK (0x3 << 12)
#define LGE_USB3PHY_TX_RES_TUNE(n) (((n) & LGE_USB3PHY_TX_RES_TUNE_MASK) >> 12)
#define LGE_USB3PHY_TX_RISE_TUNE_MASK (0x3 << 14)
#define LGE_USB3PHY_TX_RISE_TUNE(n) (((n) &  LGE_USB3PHY_TX_RISE_TUNE_MASK) >> 14)
#define LGE_USB3PHY_TX_VREF_TUNE_MASK (0xf << 16)
#define LGE_USB3PHY_TX_VREF_TUNE(n) (((n) & LGE_USB3PHY_TX_VREF_TUNE_MASK) >> 16)

/* PHY REG 0 */
#define LGE_USB3PHY_REG0 0x30C
#define LGE_USB3PHY_REG0_OTGDISABLE0 (1 << 11)
#define LGE_USB3PHY_REG0_COMMONONN (1 << 16)

/* PHY REG 1 */
#define LGE_USB3PHY_REG1 0x310
#define LGE_USB3PHY_REG1_TEST_POWERDOWN_SSP (1 << 0)
#define LGE_USB3PHY_REG1_TEST_POWERDOWN_HSP (1 << 1)
#define LGE_USB3PHY_REG1_SSC_EN (1 << 12)
#define LGE_USB3PHY_REG1_REF_SSP_EN (1 << 13)

/* BC */
#define LGE_USB3PHY_BC_IN	0x300
#define LGE_USB3PHY_BC_OUT	0x304
#define LGE_USB3PHY_BC_OUT_CHGDET0_MASK	0x1
#define LGE_USB3PHY_BC_OUT_FSVPLUS_MASK (0x1 << 1)

enum phy_powerdown_mode {
	PD_MODE_HS = 1,
	PD_MODE_SS,
};

enum phy_tuning_mode {
	PHY_TUNING_MODE_NORMAL = 1,
	PHY_TUNING_MODE_SETTING,
};

struct lge_usb3phy {
	struct usb_phy	*phy;
	struct device	*dev;
	struct odin_usb3_otg	*odin_otg;

	struct clk	*clk;
	struct regulator *usb30phy;
	struct regulator *usb30io;

	void __iomem	*regs;
	void __iomem	*crg_regs;
	spinlock_t	lock;

	int rd_gpio;
	u32 ss_redriver;

	enum phy_tuning_mode tuning_mode;
	u32 tv_base;
	u32 tv_otg;
};

static int lge_usb3phy_bc_detect(struct lge_usb3phy *lge_phy);
static void lge_usb3phy_tune_phy(struct lge_usb3phy *lge_phy, u32 tuning_value);
#endif /*                             */