#include <linux/mu3phy/mtk-phy.h>

#ifdef CONFIG_PROJECT_PHY
#include <mach/mt_pm_ldo.h>
#include <mach/mt_clkmgr.h>
#include <asm/io.h>
#include <linux/mu3phy/mtk-phy-asic.h>
#include <linux/mu3d/hal/mu3d_hal_osal.h>

/*Turn on/off ADA_SSUSB_XTAL_CK 26MHz*/
void enable_ssusb_xtal_clock(bool enable)
{
    if (enable) {
		/*
		* 1 *AP_PLL_CON0 =| 0x1 [0]=1: RG_LTECLKSQ_EN
		* 2 Wait PLL stable (100us)
		* 3 *AP_PLL_CON0 =| 0x2 [1]=1: RG_LTECLKSQ_LPF_EN
		* 4 *AP_PLL_CON2 =| 0x1 [0]=1: DA_REF2USB_TX_EN
		* 5 Wait PLL stable (100us)
		* 6 *AP_PLL_CON2 =| 0x2 [1]=1: DA_REF2USB_TX_LPF_EN
		* 7 *AP_PLL_CON2 =| 0x4 [2]=1: DA_REF2USB_TX_OUT_EN
		*/
		writel(readl((void __iomem *)AP_PLL_CON0)|(0x00000001),
		    (void __iomem *)AP_PLL_CON0);
		/*Wait 100 usec*/
		udelay(100);

		writel(readl((void __iomem *)AP_PLL_CON0)|(0x00000002),
		    (void __iomem *)AP_PLL_CON0);

		writel(readl((void __iomem *)AP_PLL_CON2)|(0x00000001),
		    (void __iomem *)AP_PLL_CON2);

		/*Wait 100 usec*/
		udelay(100);

		writel(readl((void __iomem *)AP_PLL_CON2)|(0x00000002),
		    (void __iomem *)AP_PLL_CON2);

		writel(readl((void __iomem *)AP_PLL_CON2)|(0x00000004),
		    (void __iomem *)AP_PLL_CON2);
    } else {
	    /*
		* AP_PLL_CON2 &= 0xFFFFFFF8	[2]=0: DA_REF2USB_TX_OUT_EN
		*					[1]=0: DA_REF2USB_TX_LPF_EN
		*					[0]=0: DA_REF2USB_TX_EN
		*/
		//writel(readl((void __iomem *)AP_PLL_CON2)&~(0x00000007),
		//	   (void __iomem *)AP_PLL_CON2);
    }
}

/*Turn on/off AD_LTEPLL_SSUSB26M_CK 26MHz*/
void enable_ssusb26m_ck(bool enable)
{
    if (enable) {
		/*
		* 1 *AP_PLL_CON0 =| 0x1 [0]=1: RG_LTECLKSQ_EN
		* 2 Wait PLL stable (100us)
		* 3 *AP_PLL_CON0 =| 0x2 [1]=1: RG_LTECLKSQ_LPF_EN
		*/
		writel(readl((void __iomem *)AP_PLL_CON0)|(0x00000001),
		    (void __iomem *)AP_PLL_CON0);
		/*Wait 100 usec*/
		udelay(100);

		writel(readl((void __iomem *)AP_PLL_CON0)|(0x00000002),
		(void __iomem *)AP_PLL_CON0);

    } else {
	    /*
		* AP_PLL_CON2 &= 0xFFFFFFF8	[2]=0: DA_REF2USB_TX_OUT_EN
		*					[1]=0: DA_REF2USB_TX_LPF_EN
		*					[0]=0: DA_REF2USB_TX_EN
		*/
		//writel(readl((void __iomem *)AP_PLL_CON2)&~(0x00000007),
		//	   (void __iomem *)AP_PLL_CON2);
    }
}

#define RG_SSUSB_VUSB10_ON (1<<5)
#define RG_SSUSB_VUSB10_ON_OFST (5)

/*This "power on/initial" sequence refer to "6593_USB_PORT0_PWR Sequence 20130729.xls"*/
PHY_INT32 phy_init_soc(struct u3phy_info *info)
{
	os_printk(K_INFO, "%s+\n", __func__);

	/*This power on sequence refers to Sheet .1 of "6593_USB_PORT0_PWR Sequence 20130729.xls"*/

	/*---POWER-----*/
	/*AVDD18_USB_P0 is always turned on. The driver does _NOT_ need to control it.*/
	hwPowerOn(MT6332_POWER_LDO_VUSB33, VOL_3300, "VDD33_USB_P0");

	/* Set RG_VUSB10_ON as 1 after VDD10 Ready */
	hwPowerOn(MT6331_POWER_LDO_VUSB10, VOL_1000, "VDD10_USB_P0");

	/*---CLOCK-----*/
	/* ADA_SSUSB_XTAL_CK:26MHz */
	enable_ssusb_xtal_clock(true);

	/* AD_LTEPLL_SSUSB26M_CK:26MHz always on */
	/* It seems that when turning on ADA_SSUSB_XTAL_CK, AD_LTEPLL_SSUSB26M_CK will also turn on.*/
	/* enable_ssusb26m_ck(true); */

	/* f_fusb30_ck:125MHz */
	enable_clock(MT_CG_PERI_USB0, "USB30");

	/* AD_SSUSB_48M_CK:48MHz */
	/* It seems that when turning on f_fusb30_ck, AD_SSUSB_48M_CK will also turn on.*/

	/*Wait 50 usec*/
	udelay(50);

	/* Set RG_SSUSB_VUSB10_ON as 1 after VUSB10 ready */
	U3PhyWriteField32(U3D_USB30_PHYA_REG0, RG_SSUSB_VUSB10_ON_OFST, RG_SSUSB_VUSB10_ON, 1);

	/*power domain iso disable*/
	U3PhyWriteField32(U3D_USBPHYACR6, E60802_RG_USB20_ISO_EN_OFST, E60802_RG_USB20_ISO_EN, 0);

	/*switch to USB function. (system register, force ip into usb mode)*/
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_FORCE_UART_EN_OFST, E60802_FORCE_UART_EN, 0);
	U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_UART_EN_OFST, E60802_RG_UART_EN, 0);
	U3PhyWriteField32(U3D_U2PHYACR4, E60802_RG_USB20_GPIO_CTL_OFST, E60802_RG_USB20_GPIO_CTL,0);
	U3PhyWriteField32(U3D_U2PHYACR4, E60802_USB20_GPIO_MODE_OFST, E60802_USB20_GPIO_MODE, 0);
	/*DP/DM BC1.1 path Disable*/
	U3PhyWriteField32(U3D_USBPHYACR6, E60802_RG_USB20_BC11_SW_EN_OFST, E60802_RG_USB20_BC11_SW_EN, 0);
	/*dp_100k disable*/
	U3PhyWriteField32(U3D_U2PHYACR4, E60802_RG_USB20_DP_100K_MODE_OFST, E60802_RG_USB20_DP_100K_MODE, 1);
	U3PhyWriteField32(U3D_U2PHYACR4, E60802_USB20_DP_100K_EN_OFST, E60802_USB20_DP_100K_EN, 0);
	/*dm_100k disable*/
	U3PhyWriteField32(U3D_U2PHYACR4, E60802_RG_USB20_DM_100K_EN_OFST, E60802_RG_USB20_DM_100K_EN, 0);
	/*Change 100uA current switch to SSUSB*/
	U3PhyWriteField32(U3D_USBPHYACR5, E60802_RG_USB20_HS_100U_U3_EN_OFST, E60802_RG_USB20_HS_100U_U3_EN, 1);
	/*OTG Enable*/
	U3PhyWriteField32(U3D_USBPHYACR6, E60802_RG_USB20_OTG_VBUSCMP_EN_OFST, E60802_RG_USB20_OTG_VBUSCMP_EN, 1);
	/*Release force suspendm. „³ (force_suspendm=0) (let suspendm=1, enable usb 480MHz pll)*/
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_FORCE_SUSPENDM_OFST, E60802_FORCE_SUSPENDM, 0);
	/*Wait 800 usec*/
	udelay(800);

	U3PhyWriteField32(U3D_U2PHYDTM1, E60802_FORCE_VBUSVALID_OFST, E60802_FORCE_VBUSVALID, 1);
	U3PhyWriteField32(U3D_U2PHYDTM1, E60802_FORCE_AVALID_OFST, E60802_FORCE_AVALID, 1);
	U3PhyWriteField32(U3D_U2PHYDTM1, E60802_FORCE_SESSEND_OFST, E60802_FORCE_SESSEND, 1);

	os_printk(K_INFO, "%s-\n", __func__);

	return PHY_TRUE;
}

PHY_INT32 u2_slew_rate_calibration(struct u3phy_info *info)
{
	PHY_INT32 i=0;
	PHY_INT32 fgRet = 0;
	PHY_INT32 u4FmOut = 0;
	PHY_INT32 u4Tmp = 0;

	os_printk(K_INFO, "%s\n", __func__);

	// => RG_USB20_HSTX_SRCAL_EN = 1
	// enable USB ring oscillator
	U3PhyWriteField32(U3D_USBPHYACR5, E60802_RG_USB20_HSTX_SRCAL_EN_OFST, E60802_RG_USB20_HSTX_SRCAL_EN, 1);

	//wait 1us
	udelay(1);

	// => USBPHY base address + 0xf11 = 1
	// Enable free run clock
	U3PhyWriteField32((USB3_SIF_BASE + 0xF11)
		, E60802_RG_FRCK_EN_OFST, E60802_RG_FRCK_EN, 0x1);

	// => USBPHY base address + 0xf01 = 0x04
	// Setting cyclecnt
	U3PhyWriteField32((USB3_SIF_BASE + 0xF01)
		, E60802_RG_CYCLECNT_OFST, E60802_RG_CYCLECNT, 0x04);

	// => USBPHY base address + 0xf03 = 0x01
	// Enable frequency meter
	U3PhyWriteField32((USB3_SIF_BASE + 0xF03)
		, E60802_RG_FREQDET_EN_OFST, E60802_RG_FREQDET_EN, 0x1);

	os_printk(K_INFO, "Freq_Valid=(0x%08X)\n", U3PhyReadReg32(USB3_SIF_BASE + 0xF10));

	// wait for FM detection done, set 10ms timeout
	for(i=0; i<10; i++){
		// => USBPHY base address + 0xf0c = FM_OUT
		// Read result
		u4FmOut = U3PhyReadReg32(USB3_SIF_BASE + 0xF0C);
		os_printk(K_INFO, "FM_OUT value: u4FmOut = %d(0x%08X)\n", u4FmOut, u4FmOut);

		// check if FM detection done
		if (u4FmOut != 0)
		{
			fgRet = 0;
			os_printk(K_INFO, "FM detection done! loop = %d\n", i);

			break;
		}

		fgRet = 1;
		udelay(1000);
	}
	// => USBPHY base address + 0xf03 = 0x00
	// Disable Frequency meter
	U3PhyWriteField32((USB3_SIF_BASE + 0xF03)
		, E60802_RG_FREQDET_EN_OFST, E60802_RG_FREQDET_EN, 0);

	// => USBPHY base address + 0xf11 = 0x00
	// Disable free run clock
	U3PhyWriteField32((USB3_SIF_BASE + 0xF11)
		, E60802_RG_FRCK_EN_OFST, E60802_RG_FRCK_EN, 0);

	//RG_USB20_HSTX_SRCTRL[2:0] = (1024/FM_OUT) * reference clock frequency * 0.028
	if(u4FmOut == 0){
		U3PhyWriteField32(U3D_USBPHYACR5, E60802_RG_USB20_HSTX_SRCTRL_OFST, E60802_RG_USB20_HSTX_SRCTRL, 0x4);
		fgRet = 1;
	}
	else{
		// set reg = (1024/FM_OUT) * REF_CK * U2_SR_COEF_E60802 / 1000 (round to the nearest digits)
		// u4Tmp = (((1024 * REF_CK * U2_SR_COEF_E60802) / u4FmOut) + 500) / 1000;
		u4Tmp = (1024 * REF_CK * U2_SR_COEF_E60802) / (u4FmOut * 1000);
		os_printk(K_INFO, "SR calibration value u1SrCalVal = %d\n", (PHY_UINT8)u4Tmp);
		U3PhyWriteField32(U3D_USBPHYACR5, E60802_RG_USB20_HSTX_SRCTRL_OFST, E60802_RG_USB20_HSTX_SRCTRL, u4Tmp);
	}

	// => RG_USB20_HSTX_SRCAL_EN = 0
	// disable USB ring oscillator
	U3PhyWriteField32(U3D_USBPHYACR5, E60802_RG_USB20_HSTX_SRCAL_EN_OFST, E60802_RG_USB20_HSTX_SRCAL_EN, 0);

	return fgRet;
}

/*This "save current" sequence refers to "6593_USB_PORT0_PWR Sequence 20130729.xls"*/
void usb_phy_savecurrent(unsigned int clk_on)
{
	os_printk(K_INFO, "%s clk_on=%d+\n", __func__, clk_on);

	/*switch to USB function. (system register, force ip into usb mode)*/
	//force_uart_en      1'b0
	//U3D_U2PHYDTM0 E60802_FORCE_UART_EN
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_FORCE_UART_EN_OFST, E60802_FORCE_UART_EN, 0);
	//RG_UART_EN         1'b0
	//U3D_U2PHYDTM1 E60802_RG_UART_EN
	U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_UART_EN_OFST, E60802_RG_UART_EN, 0);
	//rg_usb20_gpio_ctl  1'b0
	//U3D_U2PHYACR4 E60802_RG_USB20_GPIO_CTL
	U3PhyWriteField32(U3D_U2PHYACR4, E60802_RG_USB20_GPIO_CTL_OFST, E60802_RG_USB20_GPIO_CTL, 0);
	//usb20_gpio_mode	1'b0
	//U3D_U2PHYACR4 E60802_USB20_GPIO_MODE
	U3PhyWriteField32(U3D_U2PHYACR4, E60802_USB20_GPIO_MODE_OFST, E60802_USB20_GPIO_MODE, 0);

	/*let suspendm=1, enable usb 480MHz pll*/
	//RG_SUSPENDM 1'b1
	//U3D_U2PHYDTM0 E60802_RG_SUSPENDM
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_RG_SUSPENDM_OFST, E60802_RG_SUSPENDM, 1);

	/*force_suspendm=1*/
	//force_suspendm	1'b1
	//U3D_U2PHYDTM0 E60802_FORCE_SUSPENDM
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_FORCE_SUSPENDM_OFST, E60802_FORCE_SUSPENDM, 1);

	/*Wait USBPLL stable.*/
	//Wait 2 ms.
	udelay(2000);

	//RG_DPPULLDOWN	1'b1
	//U3D_U2PHYDTM0 E60802_RG_DPPULLDOWN
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_RG_DPPULLDOWN_OFST, E60802_RG_DPPULLDOWN, 1);

	//RG_DMPULLDOWN	1'b1
	//U3D_U2PHYDTM0 E60802_RG_DMPULLDOWN
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_RG_DMPULLDOWN_OFST, E60802_RG_DMPULLDOWN, 1);

	//RG_XCVRSEL[1:0] 2'b01
	//U3D_U2PHYDTM0 E60802_RG_XCVRSEL
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_RG_XCVRSEL_OFST, E60802_RG_XCVRSEL, 0x1);

	//RG_TERMSEL	1'b1
	//U3D_U2PHYDTM0 E60802_RG_TERMSEL
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_RG_TERMSEL_OFST, E60802_RG_TERMSEL, 1);

	//RG_DATAIN[3:0]	4'b0000
	//U3D_U2PHYDTM0 E60802_RG_DATAIN
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_RG_DATAIN_OFST, E60802_RG_DATAIN, 0);

	//force_dp_pulldown	1'b1
	//U3D_U2PHYDTM0 E60802_FORCE_DP_PULLDOWN
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_FORCE_DP_PULLDOWN_OFST, E60802_FORCE_DP_PULLDOWN, 1);

	//force_dm_pulldown	1'b1
	//U3D_U2PHYDTM0 E60802_FORCE_DM_PULLDOWN
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_FORCE_DM_PULLDOWN_OFST, E60802_FORCE_DM_PULLDOWN, 1);

	//force_xcversel	1'b1
	//U3D_U2PHYDTM0 E60802_FORCE_XCVRSEL
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_FORCE_XCVRSEL_OFST, E60802_FORCE_XCVRSEL, 1);

	//force_termsel	1'b1
	//U3D_U2PHYDTM0 E60802_FORCE_TERMSEL
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_FORCE_TERMSEL_OFST, E60802_FORCE_TERMSEL, 1);

	//force_datain	1'b1
	//U3D_U2PHYDTM0 E60802_FORCE_DATAIN
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_FORCE_DATAIN_OFST, E60802_FORCE_DATAIN, 1);

	/*DP/DM BC1.1 path Disable*/
	//RG_USB20_BC11_SW_EN 1'b0
	//U3D_USBPHYACR6 E60802_RG_USB20_BC11_SW_EN
	U3PhyWriteField32(U3D_USBPHYACR6, E60802_RG_USB20_BC11_SW_EN_OFST, E60802_RG_USB20_BC11_SW_EN, 0);

	/*OTG Disable*/
	//RG_USB20_OTG_VBUSCMP_EN 1¡¦b0
	//U3D_USBPHYACR6 E60802_RG_USB20_OTG_VBUSCMP_EN
	U3PhyWriteField32(U3D_USBPHYACR6, E60802_RG_USB20_OTG_VBUSCMP_EN_OFST, E60802_RG_USB20_OTG_VBUSCMP_EN, 0);

	/*Change 100uA current switch to USB2.0*/
	//RG_USB20_HS_100U_U3_EN	1'b0
	//U3D_USBPHYACR5 E60802_RG_USB20_HS_100U_U3_EN
	U3PhyWriteField32(U3D_USBPHYACR5, E60802_RG_USB20_HS_100U_U3_EN_OFST, E60802_RG_USB20_HS_100U_U3_EN, 0);

	//wait 800us
	udelay(800);

	/*let suspendm=0, set utmi into analog power down*/
	//RG_SUSPENDM 1'b0
	//U3D_U2PHYDTM0 E60802_RG_SUSPENDM
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_RG_SUSPENDM_OFST, E60802_RG_SUSPENDM, 0);

	//wait 1us
	udelay(1);

	U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_VBUSVALID_OFST, E60802_RG_VBUSVALID, 0);
	U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_AVALID_OFST, E60802_RG_AVALID, 0);
	U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_SESSEND_OFST, E60802_RG_SESSEND, 1);

	/* TODO:
	 * Turn off internal 48Mhz PLL if there is no other hardware module is
	 * using the 48Mhz clock -the control register is in clock document
	 * Turn off SSUSB reference clock (26MHz)
	 */
	if (clk_on) {
		/*---CLOCK-----*/
		/* Set RG_SSUSB_VUSB10_ON as 1 after VUSB10 ready */
		U3PhyWriteField32(U3D_USB30_PHYA_REG0, RG_SSUSB_VUSB10_ON_OFST, RG_SSUSB_VUSB10_ON, 0);

		//Wait 10 usec.
		udelay(10);

		/* f_fusb30_ck:125MHz */
		disable_clock(MT_CG_PERI_USB0, "USB30");

		/* ADA_SSUSB_XTAL_CK:26MHz */
		enable_ssusb_xtal_clock(false);

		/*---POWER-----*/
		/*AVDD18_USB_P0 is always turned on. The driver does _NOT_ need to control it.*/
		hwPowerDown(MT6332_POWER_LDO_VUSB33, "VDD33_USB_P0");

		/* Set RG_VUSB10_ON as 1 after VDD10 Ready */
		hwPowerDown(MT6331_POWER_LDO_VUSB10, "VDD10_USB_P0");
	}
 	//disable_clock(MT_CG_PERI_USB0, "USB30"); //f_fusb30_ck:125MHz

	os_printk(K_INFO, "%s-\n", __func__);
}

/*This "recovery" sequence refers to "6593_USB_PORT0_PWR Sequence 20130729.xls"*/
void usb_phy_recover(unsigned int clk_on)
{
	os_printk(K_INFO, "%s clk_on=%d+\n", __func__, clk_on);

	if (!clk_on) {
		/*---POWER-----*/
		/*AVDD18_USB_P0 is always turned on. The driver does _NOT_ need to control it.*/
		hwPowerOn(MT6332_POWER_LDO_VUSB33, VOL_3300, "VDD33_USB_P0");

		/* Set RG_VUSB10_ON as 1 after VDD10 Ready */
		hwPowerOn(MT6331_POWER_LDO_VUSB10, VOL_1000, "VDD10_USB_P0");

		/*---CLOCK-----*/
		/* ADA_SSUSB_XTAL_CK:26MHz */
		enable_ssusb_xtal_clock(true);

		/* AD_LTEPLL_SSUSB26M_CK:26MHz always on */
		/* It seems that when turning on ADA_SSUSB_XTAL_CK, AD_LTEPLL_SSUSB26M_CK will also turn on.*/
		/* enable_ssusb26m_ck(true); */

		/* f_fusb30_ck:125MHz */
		enable_clock(MT_CG_PERI_USB0, "USB30");

		/* AD_SSUSB_48M_CK:48MHz */
		/* It seems that when turning on f_fusb30_ck, AD_SSUSB_48M_CK will also turn on.*/

		//Wait 50 usec. (PHY 3.3v & 1.8v power stable time)
		udelay(50);

		/* Set RG_SSUSB_VUSB10_ON as 1 after VUSB10 ready */
		U3PhyWriteField32(U3D_USB30_PHYA_REG0, RG_SSUSB_VUSB10_ON_OFST, RG_SSUSB_VUSB10_ON, 1);
	}

	/*[MT6593 only]power domain iso disable*/
	//RG_USB20_ISO_EN	1'b0
	//U3D_USBPHYACR6 E60802_RG_USB20_ISO_EN
	U3PhyWriteField32(U3D_USBPHYACR6, E60802_RG_USB20_ISO_EN_OFST, E60802_RG_USB20_ISO_EN, 0);

	/*switch to USB function. (system register, force ip into usb mode)*/
	//force_uart_en      1'b0
	//U3D_U2PHYDTM0 E60802_FORCE_UART_EN
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_FORCE_UART_EN_OFST, E60802_FORCE_UART_EN, 0);
	//RG_UART_EN         1'b0
	//U3D_U2PHYDTM1 E60802_RG_UART_EN
	U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_UART_EN_OFST, E60802_RG_UART_EN, 0);
	//rg_usb20_gpio_ctl  1'b0
	//U3D_U2PHYACR4 E60802_RG_USB20_GPIO_CTL
	U3PhyWriteField32(U3D_U2PHYACR4, E60802_RG_USB20_GPIO_CTL_OFST, E60802_RG_USB20_GPIO_CTL, 0);
	//usb20_gpio_mode	1'b0
	//U3D_U2PHYACR4 E60802_USB20_GPIO_MODE
	U3PhyWriteField32(U3D_U2PHYACR4, E60802_USB20_GPIO_MODE_OFST, E60802_USB20_GPIO_MODE, 0);

	/*Release force suspendm. (force_suspendm=0) (let suspendm=1, enable usb 480MHz pll)*/
	/*force_suspendm	1'b0*/
	//U3D_U2PHYDTM0 E60802_FORCE_SUSPENDM
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_FORCE_SUSPENDM_OFST, E60802_FORCE_SUSPENDM, 0);

	//RG_DPPULLDOWN 1'b0
	//U3D_U2PHYDTM0 E60802_RG_DPPULLDOWN
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_RG_DPPULLDOWN_OFST, E60802_RG_DPPULLDOWN, 0);

	//RG_DMPULLDOWN 1'b0
	//U3D_U2PHYDTM0 E60802_RG_DMPULLDOWN
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_RG_DMPULLDOWN_OFST, E60802_RG_DMPULLDOWN, 0);

	//RG_XCVRSEL[1:0]	2'b00
	//U3D_U2PHYDTM0 E60802_RG_XCVRSEL
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_RG_XCVRSEL_OFST, E60802_RG_XCVRSEL, 0);

	//RG_TERMSEL	1'b0
	//U3D_U2PHYDTM0 E60802_RG_TERMSEL
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_RG_TERMSEL_OFST, E60802_RG_TERMSEL, 0);

	//RG_DATAIN[3:0]	4'b0000
	//U3D_U2PHYDTM0 E60802_RG_DATAIN
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_RG_DATAIN_OFST, E60802_RG_DATAIN, 0);

	//force_dp_pulldown	1'b0
	//U3D_U2PHYDTM0 E60802_FORCE_DP_PULLDOWN
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_FORCE_DP_PULLDOWN_OFST, E60802_FORCE_DP_PULLDOWN, 0);

	//force_dm_pulldown	1'b0
	//U3D_U2PHYDTM0 E60802_FORCE_DM_PULLDOWN
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_FORCE_DM_PULLDOWN_OFST, E60802_FORCE_DM_PULLDOWN, 0);

	//force_xcversel	1'b0
	//U3D_U2PHYDTM0 E60802_FORCE_XCVRSEL
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_FORCE_XCVRSEL_OFST, E60802_FORCE_XCVRSEL, 0);

	//force_termsel	1'b0
	//U3D_U2PHYDTM0 E60802_FORCE_TERMSEL
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_FORCE_TERMSEL_OFST, E60802_FORCE_TERMSEL, 0);

	//force_datain	1'b0
	//U3D_U2PHYDTM0 E60802_FORCE_DATAIN
	U3PhyWriteField32(U3D_U2PHYDTM0, E60802_FORCE_DATAIN_OFST, E60802_FORCE_DATAIN, 0);

	/*DP/DM BC1.1 path Disable*/
	//RG_USB20_BC11_SW_EN	1'b0
	//U3D_USBPHYACR6 E60802_RG_USB20_BC11_SW_EN
	U3PhyWriteField32(U3D_USBPHYACR6, E60802_RG_USB20_BC11_SW_EN_OFST, E60802_RG_USB20_BC11_SW_EN, 0);

	/*OTG Enable*/
	//RG_USB20_OTG_VBUSCMP_EN	1¡¦b1
	U3PhyWriteField32(U3D_USBPHYACR6, E60802_RG_USB20_OTG_VBUSCMP_EN_OFST, E60802_RG_USB20_OTG_VBUSCMP_EN, 1);

	/*Change 100uA current switch to SSUSB*/
	//RG_USB20_HS_100U_U3_EN	1'b1
	U3PhyWriteField32(U3D_USBPHYACR5, E60802_RG_USB20_HS_100U_U3_EN_OFST, E60802_RG_USB20_HS_100U_U3_EN, 1);

	//Wait 800 usec
	udelay(800);

	U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_VBUSVALID_OFST, E60802_RG_VBUSVALID, 1);
	U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_AVALID_OFST, E60802_RG_AVALID, 1);
	U3PhyWriteField32(U3D_U2PHYDTM1, E60802_RG_SESSEND_OFST, E60802_RG_SESSEND, 0);

	os_printk(K_INFO, "%s-\n", __func__);
}

// BC1.2
void Charger_Detect_Init(void)
{
	os_printk(K_INFO, "%s+\n", __func__);

	//turn on USB reference clock.
	enable_clock(MT_CG_PERI_USB0, "USB30");

	//wait 50 usec.
	udelay(50);

	/* RG_USB20_BC11_SW_EN = 1'b1 */
	U3PhyWriteField32(U3D_USBPHYACR6, E60802_RG_USB20_BC11_SW_EN_OFST, E60802_RG_USB20_BC11_SW_EN, 1);

	udelay(1);

	//4 14. turn off internal 48Mhz PLL.
	disable_clock(MT_CG_PERI_USB0, "USB30");

	os_printk(K_INFO, "%s-\n", __func__);
}

void Charger_Detect_Release(void)
{
	os_printk(K_INFO, "%s+\n", __func__);

	//turn on USB reference clock.
	enable_clock(MT_CG_PERI_USB0, "USB30");

	//wait 50 usec.
	udelay(50);

	/* RG_USB20_BC11_SW_EN = 1'b0 */
	U3PhyWriteField32(U3D_USBPHYACR6, E60802_RG_USB20_BC11_SW_EN_OFST, E60802_RG_USB20_BC11_SW_EN, 0);

	udelay(1);

	//4 14. turn off internal 48Mhz PLL.
	disable_clock(MT_CG_PERI_USB0, "USB30");

	os_printk(K_INFO, "%s-\n", __func__);
}


#endif
