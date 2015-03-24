/*
 * MUSB OTG controller driver for Blackfin Processors
 *
 * Copyright 2006-2008 Analog Devices Inc.
 *
 * Enter bugs at http://blackfin.uclinux.org/
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <mach/irqs.h>
#include <mach/eint.h>
#include <mach/mt_gpio_def.h>
#include <mach/mt_usb.h>
#include <linux/musb/musb_core.h>
#include <linux/platform_device.h>
#include <linux/musb/musbhsdma.h>
#include <linux/switch.h>
#include "usb20.h"

#ifdef CONFIG_USB_MTK_OTG

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#endif


extern struct musb *mtk_musb;

static struct musb_fifo_cfg fifo_cfg_host[] = {
	{.hw_ep_num = 1, .style = MUSB_FIFO_TX, .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
	{.hw_ep_num = 1, .style = MUSB_FIFO_RX, .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
	{.hw_ep_num = 2, .style = MUSB_FIFO_TX, .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
	{.hw_ep_num = 2, .style = MUSB_FIFO_RX, .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
	{.hw_ep_num = 3, .style = MUSB_FIFO_TX, .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
	{.hw_ep_num = 3, .style = MUSB_FIFO_RX, .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
	{.hw_ep_num = 4, .style = MUSB_FIFO_TX, .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
	{.hw_ep_num = 4, .style = MUSB_FIFO_RX, .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
	{.hw_ep_num = 5, .style = MUSB_FIFO_TX, .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
	{.hw_ep_num = 5, .style = MUSB_FIFO_RX, .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
	{.hw_ep_num = 6, .style = MUSB_FIFO_TX, .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
	{.hw_ep_num = 6, .style = MUSB_FIFO_RX, .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
	{.hw_ep_num = 7, .style = MUSB_FIFO_TX, .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
	{.hw_ep_num = 7, .style = MUSB_FIFO_RX, .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
	{.hw_ep_num = 8, .style = MUSB_FIFO_TX, .maxpacket = 512, .mode = MUSB_BUF_SINGLE},
	{.hw_ep_num = 8, .style = MUSB_FIFO_RX, .maxpacket = 64, .mode = MUSB_BUF_SINGLE},
};

u32 delay_time = 15;
module_param(delay_time, int, 0644);
u32 delay_time1 = 55;
module_param(delay_time1, int, 0644);

void mt_usb_set_vbus(struct musb *musb, int is_on)
{
	struct musb_hdrc_platform_data *plat;
	struct mt_usb_board_data *bdata;
	plat = dev_get_platdata(mtk_musb->controller);
	bdata = plat->board_data;
	DBG(0, "mt65xx_usb20_vbus++,is_on=%d\r\n", is_on);
#ifndef FPGA_PLATFORM
	if (is_on) {
		/* power on VBUS, implement later... */
#ifdef CONFIG_MTK_FAN5405_SUPPORT
		fan5405_set_opa_mode(1);
		fan5405_set_otg_pl(1);
		fan5405_set_otg_en(1);
#elif defined(CONFIG_MTK_NCP1851_SUPPORT) || defined(CONFIG_MTK_BQ24196_SUPPORT) || defined(CONFIG_MTK_BQ24158_SUPPORT) || defined(CONFIG_MTK_BQ24297_SUPPORT)
		tbl_charger_otg_vbus((work_busy(&musb->id_pin_work.work) << 8) | 1);
#else
		if (bdata != NULL && bdata->vbus_gpio >= 0) {
			mt_pin_set_mode_gpio(bdata->vbus_gpio);
			gpio_direction_output(bdata->vbus_gpio, 1);
		}
#endif
	} else {
		/* power off VBUS, implement later... */
#ifdef CONFIG_MTK_FAN5405_SUPPORT
		fan5405_config_interface_liao(0x01, 0x30);
		fan5405_config_interface_liao(0x02, 0x8e);
#elif defined(CONFIG_MTK_NCP1851_SUPPORT) || defined(CONFIG_MTK_BQ24196_SUPPORT) || defined(CONFIG_MTK_BQ24158_SUPPORT) || defined(CONFIG_MTK_BQ24297_SUPPORT)
		tbl_charger_otg_vbus((work_busy(&musb->id_pin_work.work) << 8) | 0);
#else
		if (bdata != NULL && bdata->vbus_gpio >= 0) {
			mt_pin_set_mode_gpio(bdata->vbus_gpio);
			gpio_direction_output(bdata->vbus_gpio, 0);
		}
#endif
	}
#endif
}

int mt_usb_get_vbus_status(struct musb *musb)
{
	int ret = 0;

	if ((musb_readb(musb->mregs, MUSB_DEVCTL) & MUSB_DEVCTL_VBUS) != MUSB_DEVCTL_VBUS) {
		ret = 1;
	} else {
		DBG(0, "VBUS error, devctl=%x, power=%d\n", musb_readb(musb->mregs, MUSB_DEVCTL),
		    musb->power);
	}
	pr_info("vbus ready = %d\n", ret);
	return ret;
}

void mt_usb_init_drvvbus(void)
{
	struct musb_hdrc_platform_data *plat;
	struct mt_usb_board_data *bdata;
	plat = dev_get_platdata(mtk_musb->controller);
	bdata = plat->board_data;
#if !(defined(SWITCH_CHARGER) || defined(FPGA_PLATFORM))
	if (bdata != NULL && bdata->vbus_gpio >= 0) {
		mt_pin_set_mode_gpio(bdata->vbus_gpio);
		gpio_direction_output(bdata->vbus_gpio, 1);
		mt_pin_set_pull(bdata->vbus_gpio, MT_PIN_PULL_ENABLE_UP);
	}
#endif
}

u32 sw_deboun_time = 10;
module_param(sw_deboun_time, int, 0644);
struct switch_dev otg_state;
extern int ep_config_from_table_for_host(struct musb *musb);

static bool musb_is_host(void)
{
	u8 devctl = 0;
	DBG(0, "will mask PMIC charger detection\n");
	bat_detect_set_usb_host_mode(true);
	musb_platform_enable(mtk_musb);
	devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
	DBG(0, "devctl = %x before end session\n", devctl);
	devctl &= ~MUSB_DEVCTL_SESSION;	/* this will cause A-device change back to B-device after A-cable plug out */
	musb_writeb(mtk_musb->mregs, MUSB_DEVCTL, devctl);
	msleep(delay_time);

	devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
	DBG(0, "devctl = %x before set session\n", devctl);

	devctl |= MUSB_DEVCTL_SESSION;
	musb_writeb(mtk_musb->mregs, MUSB_DEVCTL, devctl);
	msleep(delay_time1);
	devctl = musb_readb(mtk_musb->mregs, MUSB_DEVCTL);
	DBG(0, "devclt = %x\n", devctl);

	if (devctl & MUSB_DEVCTL_BDEVICE) {
		DBG(0, "will unmask PMIC charger detection\n");
		bat_detect_set_usb_host_mode(false);
		return FALSE;
	} else {
		return TRUE;
	}
}

void musb_session_restart(struct musb *musb)
{
	void __iomem *mbase = musb->mregs;
	musb_writeb(mbase, MUSB_DEVCTL, (musb_readb(mbase, MUSB_DEVCTL) & (~MUSB_DEVCTL_SESSION)));
	DBG(0, "[MUSB] stopped session for VBUSERROR interrupt\n");
	USBPHY_SET8(0x6d, 0x3c);
	USBPHY_SET8(0x6c, 0x10);
	USBPHY_CLR8(0x6c, 0x2c);
	DBG(0, "[MUSB] force PHY to idle, 0x6d=%x, 0x6c=%x\n", USBPHY_READ8(0x6d),
	    USBPHY_READ8(0x6c));
	mdelay(5);
	USBPHY_CLR8(0x6d, 0x3c);
	USBPHY_CLR8(0x6c, 0x3c);
	DBG(0, "[MUSB] let PHY resample VBUS, 0x6d=%x, 0x6c=%x\n", USBPHY_READ8(0x6d),
	    USBPHY_READ8(0x6c));
	musb_writeb(mbase, MUSB_DEVCTL, (musb_readb(mbase, MUSB_DEVCTL) | MUSB_DEVCTL_SESSION));
	DBG(0, "[MUSB] restart session\n");
}

void switch_int_to_device(struct musb *musb)
{
	musb_writel(musb->mregs, USB_L1INTP, 0);
	musb_writel(musb->mregs, USB_L1INTM,
		    IDDIG_INT_STATUS | musb_readl(musb->mregs, USB_L1INTM));

	DBG(0, "switch_int_to_device is done\n");
}

void switch_int_to_host(struct musb *musb)
{
	musb_writel(musb->mregs, USB_L1INTP, IDDIG_INT_STATUS);
	musb_writel(musb->mregs, USB_L1INTM,
		    IDDIG_INT_STATUS | musb_readl(musb->mregs, USB_L1INTM));
	DBG(0, "switch_int_to_host is done\n");

}

void switch_int_to_host_and_mask(struct musb *musb)
{
	musb_writel(musb->mregs, USB_L1INTM, (~IDDIG_INT_STATUS) & musb_readl(musb->mregs, USB_L1INTM));	/* mask before change polarity */
	mb();
	musb_writel(musb->mregs, USB_L1INTP, IDDIG_INT_STATUS);
	DBG(0, "swtich_int_to_host_and_mask is done\n");
}

static void musb_id_pin_work(struct work_struct *data)
{

#ifdef CONFIG_AMAZON_METRICS_LOG
	char buf[128];
#endif
	down(&mtk_musb->musb_lock);
	DBG(0, "work start, is_host=%d\n", mtk_musb->is_host);
	if (mtk_musb->in_ipo_off) {
		DBG(0, "do nothing due to in_ipo_off\n");
		goto out;
	}

	mtk_musb->is_host = musb_is_host();
	DBG(0, "musb is as %s\n", mtk_musb->is_host ? "host" : "device");
	switch_set_state((struct switch_dev *)&otg_state, mtk_musb->is_host);

	if (mtk_musb->is_host) {
		/* setup fifo for host mode */
		ep_config_from_table_for_host(mtk_musb);
		wake_lock(&mtk_musb->usb_lock);
		musb_platform_set_vbus(mtk_musb, 1);
		musb_start(mtk_musb);
		MUSB_HST_MODE(mtk_musb);
		switch_int_to_device(mtk_musb);
#ifdef CONFIG_AMAZON_METRICS_LOG
		snprintf(buf, sizeof(buf),
			"%s:usb20:otg_gnd=1;CT;1,state=%d;DV;1:NR",
			__func__, mtk_musb->xceiv->state);
		log_to_metrics(ANDROID_LOG_INFO, "USBCableEvent", buf);
#endif
	} else {
		DBG(0, "devctl is %x\n", musb_readb(mtk_musb->mregs, MUSB_DEVCTL));
		musb_writeb(mtk_musb->mregs, MUSB_DEVCTL, 0);
		if (wake_lock_active(&mtk_musb->usb_lock))
			wake_unlock(&mtk_musb->usb_lock);
		musb_platform_set_vbus(mtk_musb, 0);
		musb_stop(mtk_musb);
		/* ALPS00849138 */
		mtk_musb->xceiv->state = OTG_STATE_B_IDLE;
		MUSB_DEV_MODE(mtk_musb);
		switch_int_to_host(mtk_musb);
#ifdef CONFIG_AMAZON_METRICS_LOG
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf),
			"%s:usb20:otg_off=1;CT;1,state=%d;DV;1:NR",
			__func__, mtk_musb->xceiv->state);
		log_to_metrics(ANDROID_LOG_INFO, "USBCableEvent", buf);
#endif
	}
 out:
	DBG(0, "work end, is_host=%d\n", mtk_musb->is_host);
	up(&mtk_musb->musb_lock);

}

void mt_usb_iddig_int(struct musb *musb)
{
	u32 usb_l1_ploy = musb_readl(musb->mregs, USB_L1INTP);

	DBG(0, "id pin interrupt assert,polarity=0x%x\n", usb_l1_ploy);
	if (usb_l1_ploy & IDDIG_INT_STATUS)
		usb_l1_ploy &= (~IDDIG_INT_STATUS);
	else
		usb_l1_ploy |= IDDIG_INT_STATUS;

	musb_writel(musb->mregs, USB_L1INTP, usb_l1_ploy);
	musb_writel(musb->mregs, USB_L1INTM,
		    (~IDDIG_INT_STATUS) & musb_readl(musb->mregs, USB_L1INTM));
	if (!mtk_musb->is_ready) {
		/* delay 5 sec if usb function is not ready */
		schedule_delayed_work(&mtk_musb->id_pin_work, 5000 * HZ / 1000);
		DBG(0, "id pin interrupt assert not ready, delay 5 sec\n");
	} else
		schedule_delayed_work(&musb->id_pin_work, sw_deboun_time * HZ / 1000);
	DBG(0, "id pin interrupt assert\n");
}

void static otg_int_init(void)
{
	u32 phy_id_pull = 0;
	phy_id_pull = __raw_readl((void __iomem *)U2PHYDTM1);
	phy_id_pull |= ID_PULL_UP;
	__raw_writel(phy_id_pull, (void __iomem *)U2PHYDTM1);

	musb_writel(mtk_musb->mregs, USB_L1INTM,
		    IDDIG_INT_STATUS | musb_readl(mtk_musb->mregs, USB_L1INTM));
}

void mt_usb_otg_init(struct musb *musb)
{
	int err;
	struct musb_hdrc_platform_data *plat;
	struct mt_usb_board_data *bdata;
	plat = dev_get_platdata(mtk_musb->controller);
	bdata = plat->board_data;

	/*init drrvbus */
	mt_usb_init_drvvbus();

	/* init idpin interrupt */
	otg_int_init();

	/* EP table */
	musb->fifo_cfg_host = fifo_cfg_host;
	musb->fifo_cfg_host_size = ARRAY_SIZE(fifo_cfg_host);

	otg_state.name = "otg_state";
	otg_state.index = 0;
	otg_state.state = 0;

	if (switch_dev_register(&otg_state))
		pr_info("switch_dev_register fail\n");

	if (bdata != NULL && bdata->vbus_gpio >= 0) {
		err = gpio_request(bdata->vbus_gpio, "mt_usb_otg_vbus");
		if (!err) {
			mt_pin_set_mode_gpio(bdata->vbus_gpio);
			gpio_direction_output(bdata->vbus_gpio, 0);
		} else
			pr_err("GPIO request failed for VBUS [GPIO%d]; err=%d\n", bdata->vbus_gpio, err);
	}
	INIT_DELAYED_WORK(&musb->id_pin_work, musb_id_pin_work);
}

void mt_usb_otg_exit(struct musb *musb)
{
	struct musb_hdrc_platform_data *plat;
	struct mt_usb_board_data *bdata;

	plat = dev_get_platdata(mtk_musb->controller);
	bdata = plat->board_data;

	if (bdata != NULL && bdata->vbus_gpio >= 0)
		gpio_free(bdata->vbus_gpio);
}
#else

/* for not define CONFIG_USB_MTK_OTG */
void mt_usb_otg_init(struct musb *musb)
{
}

void mt_usb_otg_exit(struct musb *musb)
{
}

void mt_usb_init_drvvbus(void)
{
}

void mt_usb_set_vbus(struct musb *musb, int is_on)
{
}

int mt_usb_get_vbus_status(struct musb *musb)
{
	return 1;
}

void mt_usb_iddig_int(struct musb *musb)
{
}

void switch_int_to_device(struct musb *musb)
{
}

void switch_int_to_host(struct musb *musb)
{
}

void switch_int_to_host_and_mask(struct musb *musb)
{
}

void musb_session_restart(struct musb *musb)
{
}

#endif
