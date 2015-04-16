/*
 * arch/arm/mach-hisi/usb_phy.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Erik Gilling <konkers@google.com>
 *	Benoit Goby <benoit@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/resource.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/usb/otg.h>
#include <linux/usb/ulpi.h>
#include <asm/mach-types.h>
#include <linux/usb_phy.h>

#define   USB_PORTSC1				0x184
#define   USB_PORTSC1_PTS(x)			(((x) & 0x3) << 30)
#define   USB_PORTSC1_PSPD(x)			(((x) & 0x3) << 26)
#define   USB_PORTSC1_PHCD			(1 << 23)
#define   USB_PORTSC1_WKOC			(1 << 22)
#define   USB_PORTSC1_WKDS			(1 << 21)
#define   USB_PORTSC1_WKCN			(1 << 20)
#define   USB_PORTSC1_PTC(x)			(((x) & 0xf) << 16)
#define   USB_PORTSC1_PP			(1 << 12)
#define   USB_PORTSC1_SUSP			(1 << 7)
#define   USB_PORTSC1_PE			(1 << 2)
#define   USB_PORTSC1_CCS			(1 << 0)

#define   USB_SUSP_CTRL				0x400
#define   USB_WAKE_ON_CNNT_EN_DEV		(1 << 3)
#define   USB_WAKE_ON_DISCON_EN_DEV		(1 << 4)
#define   USB_SUSP_CLR				(1 << 5)
#define   USB_PHY_CLK_VALID			(1 << 7)
#define   UTMIP_RESET				(1 << 11)
#define   UHSIC_RESET				(1 << 11)
#define   UTMIP_PHY_ENABLE			(1 << 12)
#define   ULPI_PHY_ENABLE			(1 << 13)
#define   USB_SUSP_SET				(1 << 14)
#define   USB_WAKEUP_DEBOUNCE_COUNT(x)		(((x) & 0x7) << 16)

#define   USB1_LEGACY_CTRL			0x410
#define   USB1_NO_LEGACY_MODE			(1 << 0)
#define   USB1_VBUS_SENSE_CTL_MASK		(3 << 1)
#define   USB1_VBUS_SENSE_CTL_VBUS_WAKEUP	(0 << 1)
#define   USB1_VBUS_SENSE_CTL_AB_SESS_VLD_OR_VBUS_WAKEUP \
						(1 << 1)
#define   USB1_VBUS_SENSE_CTL_AB_SESS_VLD	(2 << 1)
#define   USB1_VBUS_SENSE_CTL_A_SESS_VLD	(3 << 1)

#define   ULPI_TIMING_CTRL_0			0x424
#define   ULPI_OUTPUT_PINMUX_BYP		(1 << 10)
#define   ULPI_CLKOUT_PINMUX_BYP		(1 << 11)

#define   ULPI_TIMING_CTRL_1			0x428
#define   ULPI_DATA_TRIMMER_LOAD		(1 << 0)
#define   ULPI_DATA_TRIMMER_SEL(x)		(((x) & 0x7) << 1)
#define   ULPI_STPDIRNXT_TRIMMER_LOAD		(1 << 16)
#define   ULPI_STPDIRNXT_TRIMMER_SEL(x)		(((x) & 0x7) << 17)
#define   ULPI_DIR_TRIMMER_LOAD			(1 << 24)
#define   ULPI_DIR_TRIMMER_SEL(x)		(((x) & 0x7) << 25)

#define   UTMIP_PLL_CFG1			0x804
#define   UTMIP_XTAL_FREQ_COUNT(x)		(((x) & 0xfff) << 0)
#define   UTMIP_PLLU_ENABLE_DLY_COUNT(x)	(((x) & 0x1f) << 27)

#define   UTMIP_XCVR_CFG0			0x808
#define   UTMIP_XCVR_SETUP(x)			(((x) & 0xf) << 0)
#define   UTMIP_XCVR_LSRSLEW(x)			(((x) & 0x3) << 8)
#define   UTMIP_XCVR_LSFSLEW(x)			(((x) & 0x3) << 10)
#define   UTMIP_FORCE_PD_POWERDOWN		(1 << 14)
#define   UTMIP_FORCE_PD2_POWERDOWN		(1 << 16)
#define   UTMIP_FORCE_PDZI_POWERDOWN		(1 << 18)
#define   UTMIP_XCVR_HSSLEW_MSB(x)		(((x) & 0x7f) << 25)

#define   UTMIP_BIAS_CFG0			0x80c
#define   UTMIP_OTGPD				(1 << 11)
#define   UTMIP_BIASPD				(1 << 10)

#define   UTMIP_HSRX_CFG0			0x810
#define   UTMIP_ELASTIC_LIMIT(x)		(((x) & 0x1f) << 10)
#define   UTMIP_IDLE_WAIT(x)			(((x) & 0x1f) << 15)

#define   UTMIP_HSRX_CFG1			0x814
#define   UTMIP_HS_SYNC_START_DLY(x)		(((x) & 0x1f) << 1)

#define   UTMIP_TX_CFG0				0x820
#define   UTMIP_FS_PREABMLE_J			(1 << 19)
#define   UTMIP_HS_DISCON_DISABLE		(1 << 8)

#define   UTMIP_MISC_CFG0			0x824
#define   UTMIP_DPDM_OBSERVE			(1 << 26)
#define   UTMIP_DPDM_OBSERVE_SEL(x)		(((x) & 0xf) << 27)
#define   UTMIP_DPDM_OBSERVE_SEL_FS_J		(UTMIP_DPDM_OBSERVE_SEL(0xf))
#define   UTMIP_DPDM_OBSERVE_SEL_FS_K		(UTMIP_DPDM_OBSERVE_SEL(0xe))
#define   UTMIP_DPDM_OBSERVE_SEL_FS_SE1		(UTMIP_DPDM_OBSERVE_SEL(0xd))
#define   UTMIP_DPDM_OBSERVE_SEL_FS_SE0		(UTMIP_DPDM_OBSERVE_SEL(0xc))
#define   UTMIP_SUSPEND_EXIT_ON_EDGE		(1 << 22)

#define   UTMIP_MISC_CFG1			0x828
#define   UTMIP_PLL_ACTIVE_DLY_COUNT(x)		(((x) & 0x1f) << 18)
#define   UTMIP_PLLU_STABLE_COUNT(x)		(((x) & 0xfff) << 6)

#define   UTMIP_DEBOUNCE_CFG0			0x82c
#define   UTMIP_BIAS_DEBOUNCE_A(x)		(((x) & 0xffff) << 0)

#define   UTMIP_BAT_CHRG_CFG0			0x830
#define   UTMIP_PD_CHRG				(1 << 0)

#define   UTMIP_SPARE_CFG0			0x834
#define   FUSE_SETUP_SEL			(1 << 3)

#define   UTMIP_XCVR_CFG1			0x838
#define   UTMIP_FORCE_PDDISC_POWERDOWN		(1 << 0)
#define   UTMIP_FORCE_PDCHRP_POWERDOWN		(1 << 2)
#define   UTMIP_FORCE_PDDR_POWERDOWN		(1 << 4)
#define   UTMIP_XCVR_TERM_RANGE_ADJ(x)		(((x) & 0xf) << 18)

#define   UTMIP_BIAS_CFG1			0x83c
#define   UTMIP_BIAS_PDTRK_COUNT(x)		(((x) & 0x1f) << 3)

#define   PERI_CRG_BASE				0xFFF35000
#define   REG_PEREN4				0x40
#define   REG_PERDIS4				0x44

#define   PCTRL_BASE				0xe8a09000
#define   REG_PERI_CTRL18			0x4c

#define   REG_PERRSTEN4				0x90
#define   REG_PERRSTDIS4			0x94

#define   IP_RST_USB2HOST_PHY			(1 << 12)
#define   IP_RST_USB2HOST_UTMI			(1 << 13)

#define   REG_BASE_PERICRG			(0xFFF35000)
#define   PERICRG_REG(x)			(REG_BASE_PERICRG + x)
#define   PMC_NOC_DEBUG_IDLE                    (1 << 9)

#define   REG_BASE_PMCCTRL			(0xFFF31000)
#define   PMCCTRL_REG(x)			(REG_BASE_PMCCTRL + x)

#define   PMC_NOC_POWER_IDLEREQ			0x380
#define   PMC_NOC_POWER_IDLEACK			0x384
#define   PMC_NOC_POWER_IDLE			0x388

#define   TXRPDTUNE				(1 << 6)
#define   TXRPUTUNE				(1 << 8)

#define   USB2_HOST_CTRL0 0xC0
#define   USB2_HOST_CTRL1 0xC4

void __iomem *base_peri_crg;
void __iomem *base_pctrl;
void __iomem *base_pmc_ctrl;

#if 0
static void noc_dss_exit_idle_mode(unsigned int value)
{
	int ret;
	int value1, value2;

	base_pmc_ctrl = ioremap(REG_BASE_PMCCTRL, 0x10000);

	ret = readl(base_pmc_ctrl + PMC_NOC_POWER_IDLEREQ);
	ret &= (~value);
	writel(ret, base_pmc_ctrl + PMC_NOC_POWER_IDLEREQ);

	while (1) {
		value1 = readl(base_pmc_ctrl + PMC_NOC_POWER_IDLEACK);
		value2 = readl(base_pmc_ctrl + PMC_NOC_POWER_IDLE);
		if (((value1 & value) == 0) && ((value2 & value) == 0))
			break;
	}

	return;
}

static void debugsubsys_power_on(void)
{
	int ret;

	base_peri_crg = ioremap(REG_BASE_PERICRG, 0x10000);

	/*top cssys sotfreset*/
	ret = readl(base_peri_crg + 0x128);
	ret &=  (~(0x01 << 11));
	ret |= (0x01 << 11);

	writel(ret, base_peri_crg + 0x128);
	/*mtcmos enable*/
	writel(0x200, base_peri_crg + 0x150);
	udelay(100);
	/*top cssys sotfreset close*/
	ret = readl(base_peri_crg + 0x128);
	ret &=  (~(0x01 << 11));
	writel(ret, base_peri_crg + 0x128);
	/*iso disable*/
	writel(0x400, base_peri_crg + 0x148);
	/*open noc_debug reset*/
	writel(0x80000, base_peri_crg + 0x64);
	/*open clock*/
	writel(0x02, base_peri_crg + 0x50);
	writel(0x6000000, base_peri_crg+ 0x00);
	/*noc debug exit idle mode*/
	noc_dss_exit_idle_mode(PMC_NOC_DEBUG_IDLE);
	/*asp clock set*/
	return;
}
#endif

static void hsic_phy_clk_disable(struct hisik3_usb_phy *phy)
{
	/*void __iomem *base = phy->regs;*/

	printk(KERN_INFO "%s.\n", __func__);

	clk_disable_unprepare(phy->clk_hsic);
	clk_disable_unprepare(phy->clk_usb2host_ref);

	/*writel(0x600, base + REG_PERDIS4);*/
}

static int hsic_phy_clk_enable(struct hisik3_usb_phy *phy)
{
	/*void __iomem *base = phy->regs;*/
	int ret = 0;

	printk(KERN_INFO "%s.\n", __func__);

	ret = clk_prepare_enable(phy->clk_hsic);
	if (ret) {
		printk("phy->clk_hsic prepare clock enable error!\n");
		return ret;
	}

	ret = clk_prepare_enable(phy->clk_usb2host_ref);
	if (ret) {
		printk("phy->clk_usb2host_ref prepare clock enable error!\n");
		return ret;
	}

	return ret;

	/*Enable hisc phy cnf clk,12MHz and 480MHz.*/
	/*writel(0x600, base + REG_PEREN4);*/
}

static void hsic_phy_preresume(struct hisik3_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	val = readl(base + UTMIP_TX_CFG0);
	val |= UTMIP_HS_DISCON_DISABLE;
	writel(val, base + UTMIP_TX_CFG0);
}

static void hsic_phy_postresume(struct hisik3_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	val = readl(base + UTMIP_TX_CFG0);
	val &= ~UTMIP_HS_DISCON_DISABLE;
	writel(val, base + UTMIP_TX_CFG0);
}

static void hsic_phy_restore_start(struct hisik3_usb_phy *phy,
				   enum hisik3_usb_phy_port_speed port_speed)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	val = readl(base + UTMIP_MISC_CFG0);
	val &= ~UTMIP_DPDM_OBSERVE_SEL(~0);
	if (port_speed == HISIK3_USB_PHY_PORT_SPEED_LOW)
		val |= UTMIP_DPDM_OBSERVE_SEL_FS_K;
	else
		val |= UTMIP_DPDM_OBSERVE_SEL_FS_J;
	writel(val, base + UTMIP_MISC_CFG0);
	udelay(1);

	val = readl(base + UTMIP_MISC_CFG0);
	val |= UTMIP_DPDM_OBSERVE;
	writel(val, base + UTMIP_MISC_CFG0);
	udelay(10);
}

static void hsic_phy_restore_end(struct hisik3_usb_phy *phy)
{
	unsigned long val;
	void __iomem *base = phy->regs;

	return;

	val = readl(base + UTMIP_MISC_CFG0);
	val &= ~UTMIP_DPDM_OBSERVE;
	writel(val, base + UTMIP_MISC_CFG0);
	udelay(10);
}

static void hsic_phy_power_on(struct hisik3_usb_phy *phy)
{
	u32 uregv = 0;
	u32 temp = 0;
	u32 temp1 = 0;

	printk(KERN_INFO "%s.\n", __func__);

	uregv = readl(base_pctrl + REG_PERI_CTRL18);
	/* hsicphy exit siddq */
	uregv &= (~(HSICPHY_SIDDQ));
	/* hsicphy commonon_n */
	uregv |= (HSICPHY_COMMON_N);
	/*hsicphy txsrtune*/
	uregv |= (HSICPHY_TXSRTUNE);

	uregv |= (TXRPDTUNE);
	uregv |= (TXRPUTUNE);

	writel(uregv, base_pctrl + REG_PERI_CTRL18);

	temp = readl(base_pctrl + USB2_HOST_CTRL0);

	temp &= (~(0x1));

	writel(temp, base_pctrl + USB2_HOST_CTRL0);

	temp1 = readl(base_pctrl + USB2_HOST_CTRL1);

	temp1 &= (~(0x3));

	temp1 |= (0xC);

	temp1 &= (~(0x1));

	temp1 &= (~(0x2E0));

	temp1 |= (0x400);

	temp1 &= (~(0x1800));

	temp1 |= (0x2000);

	writel(temp1, base_pctrl + USB2_HOST_CTRL1);

	return;
}

static void hsic_phy_power_off(struct hisik3_usb_phy *phy)
{
	u32 uregv = 0;
	printk(KERN_INFO "%s.\n", __func__);

	uregv = readl(base_pctrl + REG_PERI_CTRL18);
	/* hsicphy enter siddq */
	uregv |= HSICPHY_SIDDQ;
	writel(uregv, base_pctrl + REG_PERI_CTRL18);

	return;
}

struct hisik3_usb_phy *hisik3_usb_phy_open(int instance, void __iomem *regs,
			void *config, enum hisik3_usb_phy_mode phy_mode, struct hisik3_ehci_hcd *hiusb_ehci)
{
	struct hisik3_usb_phy *phy;

	printk(KERN_INFO "%s.\n", __func__);

	phy = kmalloc(sizeof(struct hisik3_usb_phy), GFP_KERNEL);
	if (!phy)
		return ERR_PTR(-ENOMEM);

	phy->instance = instance;
	phy->regs = ioremap(PERI_CRG_BASE, 1024);
	phy->config = config;
	phy->mode = phy_mode;
	phy->clk_hsic = hiusb_ehci->clk_hsic;
	phy->clk_usb2host_ref = hiusb_ehci->clk_usb2host_ref;

	base_pctrl = ioremap(PCTRL_BASE, 1024);
#if 0
	debugsubsys_power_on();
#endif
	hsic_phy_clk_enable(phy);

	return phy;
}

void hisik3_usb_phy_power_on(struct hisik3_ehci_hcd *hiusb_ehci,  void __iomem *regs)
{
	struct hisik3_usb_phy *phy = hiusb_ehci->phy;
	void __iomem *base = phy->regs;

	printk(KERN_INFO "%s.\n", __func__);

	/* hsicphy port reset */
	writel(IP_RST_HSICPHY, base + REG_PERRSTEN4);
	/* hsicphy por reset */
	writel(IP_HSICPHY_POR, base + REG_PERRSTEN4);

	udelay(100);
	/* host phy port rstn */
	writel(IP_RST_USB2H_PHY, base + REG_PERRSTDIS4);
	/* host phy por rstn */
	writel(IP_RST_USB2H_POR_PHY, base + REG_PERRSTDIS4);
	udelay(100);

	writel(IP_RST_USB2HOST_PHY, base + REG_PERRSTDIS4);
	writel(IP_RST_USB2HOST_UTMI, base + REG_PERRSTDIS4);

	writel(IP_RST_USB2HOST, base + REG_PERRSTDIS4);

	/* confirure enable hsic, synopsys EHCI INSNREG08 */
	writel(1, regs+ 0xB0);

	hsic_phy_power_on(phy);
}

void hisik3_usb_phy_power_off(struct hisik3_ehci_hcd *hiusb_ehci)
{
	//u32 uregv = 0;
	struct hisik3_usb_phy *phy = hiusb_ehci->phy;
	void __iomem *base = phy->regs;

	printk(KERN_INFO "%s.\n", __func__);

	/* hsicphy port reset */
	writel(IP_RST_HSICPHY, base + REG_PERRSTEN4);
	/* hsicphy por reset */
	writel(IP_HSICPHY_POR, base + REG_PERRSTEN4);
	writel(GT_CLK_USB2HOST_REF, base + REG_PERRSTEN4);

	hsic_phy_power_off(phy);

	/* uregv = readl(base_pctrl + REG_PERI_CTRL18); */
	/* hsicphy enter siddq */
	/* uregv |= HSICPHY_SIDDQ; */
	/* writel(uregv, base_pctrl + REG_PERI_CTRL18); */

	return;
}

void hisik3_usb_phy_preresume(struct hisik3_usb_phy *phy)
{
	printk(KERN_INFO "%s.\n", __func__);

	hsic_phy_preresume(phy);
}

void hisik3_usb_phy_postresume(struct hisik3_usb_phy *phy)
{
	printk(KERN_INFO "%s.\n", __func__);
	hsic_phy_postresume(phy);
}

void hisik3_ehci_phy_restore_start(struct hisik3_usb_phy *phy,
				 enum hisik3_usb_phy_port_speed port_speed)
{
	/* Force the phy to keep data lines in suspend state */
	hsic_phy_restore_start(phy, port_speed);
}

void hisik3_ehci_phy_restore_end(struct hisik3_usb_phy *phy)
{
	/* Cancel the phy to keep data lines in suspend state */
	hsic_phy_restore_end(phy);
}

void hisik3_usb_phy_clk_disable(struct hisik3_usb_phy *phy)
{
	hsic_phy_clk_disable(phy);
}

void hisik3_usb_phy_clk_enable(struct hisik3_usb_phy *phy)
{
	hsic_phy_clk_enable(phy);
}

void hisik3_usb_phy_close(struct hisik3_usb_phy *phy)
{
	printk(KERN_INFO "%s.\n", __func__);
}
