/*
 * Copyright (C) 2010 MediaTek, Inc.
 *
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

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/rtc.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/reboot.h>

#include <asm/mach/time.h>

#include <mach/irqs.h>
#include <mach/mtk_rtc.h>
#include <mach/mtk_rtc_hal.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_pmic_wrap.h>
#if defined CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
#include <mach/system.h>
#include <mach/mt_boot.h>
#endif
#include <linux/rtc-mt.h>

#include "../misc/mediatek/power/mt8135/upmu_common.h"

#include "rtc-mt8135.h"

#define RTC_NAME	"mt-rtc"

#define RTC_RELPWR_WHEN_XRST	1	/* BBPU = 0 when xreset_rstb goes low */


/* we map HW YEA 0 (2000) to 1968 not 1970 because 2000 is the leap year */
#define RTC_MIN_YEAR		1968
#define RTC_NUM_YEARS		128
/* #define RTC_MAX_YEAR          (RTC_MIN_YEAR + RTC_NUM_YEARS - 1) */

#define RTC_MIN_YEAR_OFFSET	(RTC_MIN_YEAR - 1900)

static struct rtc_device *rtc;
static DEFINE_MUTEX(rtc_lock);

static int rtc_show_time;
static int rtc_show_alarm = 1;

#define rtc_get_lock() mutex_lock(&rtc_lock)
#define rtc_put_lock()  mutex_unlock(&rtc_lock)

/* elementary RTC IO primitives; need RTC clock enabled in order to work */

static inline u16 rtc_read(u16 addr)
{
	u32 rdata = 0;
	pwrap_read((u32) addr, &rdata);
	return (u16) rdata;
}

static inline void rtc_write(u16 addr, u16 data)
{
	if (RTC_PDN1 == addr) {
		if (data&(1<<4)) {
			dump_stack();
			/*BUG_ON(1);*/
		}
	}
	pwrap_write((u32) addr, (u32) data);
}

static inline void rtc_busy_wait(void)
{
	while (rtc_read(RTC_BBPU) & RTC_BBPU_CBUSY)
		continue;
}

static inline void rtc_write_trigger(void)
{
	rtc_write(RTC_WRTGR, 1);
	rtc_busy_wait();
}

/* complex RTC primitives: wrap RTC IO in clock enable/disable calls */

static void rtc_writeif_unlock(void)
{
	rtc_write(RTC_PROT, RTC_PROT_UNLOCK1);
	rtc_write_trigger();
	rtc_write(RTC_PROT, RTC_PROT_UNLOCK2);
	rtc_write_trigger();
}

static void _rtc_set_spare_fg_value(u16 val)
{
	/* RTC_AL_HOU bit8~14 */
	u16 temp;

	rtc_writeif_unlock();
	temp = rtc_read(RTC_AL_HOU);
	val = (val & (RTC_NEW_SPARE_FG_MASK >> RTC_NEW_SPARE_FG_SHIFT)) << RTC_NEW_SPARE_FG_SHIFT;
	temp = (temp & RTC_AL_HOU_MASK) | val;
	rtc_write(RTC_AL_HOU, temp);
	rtc_write_trigger();
}

static void _rtc_xosc_write(u16 val, bool reload)
{
	u16 bbpu;

	rtc_write(RTC_OSC32CON, RTC_OSC32CON_UNLOCK1);
	rtc_busy_wait();
	rtc_write(RTC_OSC32CON, RTC_OSC32CON_UNLOCK2);
	rtc_busy_wait();

	rtc_write(RTC_OSC32CON, val);
	rtc_busy_wait();

	if (reload) {
		bbpu = rtc_read(RTC_BBPU) | RTC_BBPU_KEY | RTC_BBPU_RELOAD;
		rtc_write(RTC_BBPU, bbpu);
		rtc_write_trigger();
	}
}

#if (defined(MTK_GPS_MT3332))
void rtc_force_set_gpio_32k(bool enable)
{
	u16 con;

	if (enable) {
		con = rtc_read(RTC_CON) & ~RTC_CON_F32KOB;
		rtc_write(RTC_CON, con);
		rtc_write_trigger();
	} else {
		con = rtc_read(RTC_CON) | RTC_CON_F32KOB;
		rtc_write(RTC_CON, con);
		rtc_write_trigger();
	}
}
#endif

static void _rtc_set_abb_32k(u16 enable)
{
	u16 con;

	if (enable)
		con = rtc_read(RTC_OSC32CON) | RTC_OSC32CON_LNBUFEN;
	else
		con = rtc_read(RTC_OSC32CON) & ~RTC_OSC32CON_LNBUFEN;

	_rtc_xosc_write(con, true);
	pr_debug("enable ABB 32k (0x%x)\n", con);
}

static u16 _rtc_get_register_status(const char *cmd)
{
	u16 spar0, al_hou, pdn1, con;
	int ret = 0;

	if (!strcmp(cmd, "XTAL")) {
		/*RTC_SPAR0 bit 6        : 32K less bit. True:with 32K, False:Without 32K */
		spar0 = rtc_read(RTC_SPAR0);
		if (spar0 & RTC_SPAR0_32K_LESS)
			ret = 1;
	} else if (!strcmp(cmd, "LPD")) {
		spar0 = rtc_read(RTC_SPAR0);
		if (spar0 & RTC_SPAR0_LP_DET)
			ret = 1;
	} else if (!strcmp(cmd, "FG")) {
		/* RTC_AL_HOU bit8~14 */
		al_hou = rtc_read(RTC_AL_HOU);
		al_hou = (al_hou & RTC_NEW_SPARE_FG_MASK) >> RTC_NEW_SPARE_FG_SHIFT;
		ret = al_hou;
	} else if (!strcmp(cmd, "GPIO")) {
		pdn1 = rtc_read(RTC_PDN1);
		con = rtc_read(RTC_CON);

		pr_debug("RTC_GPIO 32k status(RTC_PDN1=0x%x)(RTC_CON=0x%x)\n", pdn1, con);

		if (!(con & RTC_CON_F32KOB))
			ret = 1;
	} else if (!strcmp(cmd, "LPRST")) {
		spar0 = rtc_read(RTC_SPAR0);
		pr_debug("LPRST status(RTC_SPAR0=0x%x\n", spar0);

		if (spar0 & RTC_SPAR0_LONG_PRESS_RST)
			ret = 1;
	} else if (!strcmp(cmd, "ENTER_KPOC")) {
		spar0 = rtc_read(RTC_SPAR0);
		pr_debug("ENTER_KPOC status(RTC_SPAR0=0x%x\n", spar0);

		if (spar0 & RTC_SPAR0_ENTER_KPOC)
			ret = 1;
	} else if (!strcmp(cmd, "REBOOT_REASON")) {
		spar0 = rtc_read(RTC_SPAR0);
		pr_debug("REBOOT_REASON status(RTC_SPAR0=0x%x\n", spar0);

		ret = (spar0 >> RTC_SPAR0_REBOOT_REASON_SHIFT)
				& RTC_SPAR0_REBOOT_REASON_MASK;
	}

	return ret;
}

static void _rtc_set_register_status(const char *cmd, u16 val)
{
	if (!strcmp(cmd, "FG"))
		_rtc_set_spare_fg_value(val);
	else if (!strcmp(cmd, "ABB"))
		_rtc_set_abb_32k(val);
}

static void _rtc_set_gpio_32k_status(u16 user, bool enable)
{
	u16 con, pdn1, regval;
	if ((user == 0) && (true == enable)) {
		con = rtc_read(RTC_CON) & ~RTC_CON_F32KOB;
		rtc_write(RTC_CON, con);
		rtc_write_trigger();
		return;
	}

	if (enable) {
		pdn1 = rtc_read(RTC_PDN1);
	} else {
		pdn1 = rtc_read(RTC_PDN1) & ~(1U << user);
		rtc_write(RTC_PDN1, pdn1);
		rtc_write_trigger();
	}

	if (!(pdn1 & RTC_GPIO_USER_MASK)) {	/* no users */
		if (enable)
			con = rtc_read(RTC_CON) & ~RTC_CON_F32KOB;
		else
			con = rtc_read(RTC_CON) | RTC_CON_F32KOB;

		rtc_write(RTC_CON, con);
		rtc_write_trigger();
	}

	if (enable) {
		pdn1 |= (1U << user);
		rtc_write(RTC_PDN1, pdn1);
		rtc_write_trigger();
	}
	rtc_write(RTC_BBPU, rtc_read(RTC_BBPU) | RTC_BBPU_KEY | RTC_BBPU_RELOAD);
	regval = rtc_read(RTC_CON);
	pr_debug("RTC_GPIO user %d enable = %d 32k (0x%x) regval=0x%x, RTC_CON=0x%x\n", user,
		      enable, pdn1, regval, RTC_CON);
}

void rtc_set_writeif(bool enable)
{
	if (enable) {
		rtc_writeif_unlock();
	} else {
		rtc_write(RTC_PROT, 0);
		rtc_write_trigger();
	}
}

static void _rtc_mark_mode(const char *cmd)
{
	u16 pdn1, spar0;

	if (!strcmp(cmd, "recv")) {
		pr_err("[RTC]:enable the recovery mode!\n");
		spar0 = rtc_read(RTC_SPAR0) & (~RTC_SPAR0_SECURE_RECOVERY_BIT);
		rtc_write(RTC_SPAR0, spar0 | RTC_SPAR0_SECURE_RECOVERY_BIT);
		pdn1 = rtc_read(RTC_PDN1) & (~RTC_PDN1_RECOVERY_MASK);
		rtc_write(RTC_PDN1, pdn1 | RTC_PDN1_FAC_RESET);
	} else if (!strcmp(cmd, "kpoc")) {
		pdn1 = rtc_read(RTC_PDN1) & (~RTC_PDN1_KPOC);
		rtc_write(RTC_PDN1, pdn1 | RTC_PDN1_KPOC);
	} else if (!strcmp(cmd, "fast")) {
		pdn1 = rtc_read(RTC_PDN1) & (~RTC_PDN1_FAST_BOOT);
		rtc_write(RTC_PDN1, pdn1 | RTC_PDN1_FAST_BOOT);
	} else if (!strcmp(cmd, "enter_kpoc")) {
		spar0 = rtc_read(RTC_SPAR0) & (~RTC_SPAR0_ENTER_KPOC);
		rtc_write(RTC_SPAR0, spar0 | RTC_SPAR0_ENTER_KPOC);
	} else if (!strcmp(cmd, "emergency")) {
		spar0 = rtc_read(RTC_SPAR0) & (~RTC_SPAR0_EMERGENCY_DL_MODE);
		rtc_write(RTC_SPAR0, spar0 | RTC_SPAR0_EMERGENCY_DL_MODE);
	} else if (!strcmp(cmd, "clear_lprst")) {
		spar0 = rtc_read(RTC_SPAR0) & (~RTC_SPAR0_LONG_PRESS_RST);
		rtc_write(RTC_SPAR0, spar0);
	} else if (!strcmp(cmd, "enter_lprst")) {
		spar0 = rtc_read(RTC_SPAR0) & (~RTC_SPAR0_LONG_PRESS_RST);
		rtc_write(RTC_SPAR0, spar0 | RTC_SPAR0_LONG_PRESS_RST);
	} else if (!strcmp(cmd, "enter_sw_lprst")) {
		spar0 = rtc_read(RTC_SPAR0) & (~RTC_SPAR0_SW_LONG_PRESS_RST);
		rtc_write(RTC_SPAR0, spar0 | RTC_SPAR0_SW_LONG_PRESS_RST);
	} else if (!strncmp(cmd, "reboot", 6)) {
		u16 spar0, reason;
		reason = (cmd[6] - '0') & RTC_SPAR0_REBOOT_REASON_MASK;
		spar0 = rtc_read(RTC_SPAR0) & RTC_SPAR0_CLEAR_REBOOT_REASON;
		rtc_write(RTC_SPAR0, spar0
				| (reason << RTC_SPAR0_REBOOT_REASON_SHIFT));
	}

	rtc_write_trigger();
}

static u16 _rtc_rdwr_uart(u16 *val)
{
	u16 pdn2;

	if (val) {
		pdn2 = rtc_read(RTC_PDN2) & (~RTC_PDN2_UART_MASK);
		pdn2 |= (*val & (RTC_PDN2_UART_MASK >> RTC_PDN2_UART_SHIFT)) << RTC_PDN2_UART_SHIFT;
		rtc_write(RTC_PDN2, pdn2);
		rtc_write_trigger();
	}
	pdn2 = rtc_read(RTC_PDN2);

	return (pdn2 & RTC_PDN2_UART_MASK) >> RTC_PDN2_UART_SHIFT;
}

static void _rtc_bbpu_pwdn(void)
{
	u16 bbpu, con;

	rtc_writeif_unlock();
	/* disable 32K export if there are no RTC_GPIO users */
	if (!(rtc_read(RTC_PDN1) & RTC_GPIO_USER_MASK)) {
		con = rtc_read(RTC_CON) | RTC_CON_F32KOB;
		rtc_write(RTC_CON, con);
		rtc_write_trigger();
	}
#if (defined(MTK_GPS_MT3332))

	rtc_force_set_gpio_32k(false);
#endif

	/* pull PWRBB low */
	bbpu = RTC_BBPU_KEY | RTC_BBPU_AUTO | RTC_BBPU_PWREN;
	rtc_write(RTC_BBPU, bbpu);
	rtc_write_trigger();
}

static void _rtc_get_pwron_alarm(struct rtc_time *tm, struct rtc_wkalrm *alm)
{
	u16 pdn1, pdn2, spar0, spar1;

	pdn1 = rtc_read(RTC_PDN1);
	pdn2 = rtc_read(RTC_PDN2);
	spar0 = rtc_read(RTC_SPAR0);
	spar1 = rtc_read(RTC_SPAR1);

	alm->enabled = (pdn1 & RTC_PDN1_PWRON_TIME ? (pdn2 & RTC_PDN2_PWRON_LOGO ? 3 : 2) : 0);
	alm->pending = !!(pdn2 & RTC_PDN2_PWRON_ALARM);	/* return Power-On Alarm bit */
	tm->tm_year = ((pdn2 & RTC_PDN2_PWRON_YEA_MASK) >> RTC_PDN2_PWRON_YEA_SHIFT);
	tm->tm_mon = ((pdn2 & RTC_PDN2_PWRON_MTH_MASK) >> RTC_PDN2_PWRON_MTH_SHIFT);
	tm->tm_mday = ((spar1 & RTC_SPAR1_PWRON_DOM_MASK) >> RTC_SPAR1_PWRON_DOM_SHIFT);
	tm->tm_hour = ((spar1 & RTC_SPAR1_PWRON_HOU_MASK) >> RTC_SPAR1_PWRON_HOU_SHIFT);
	tm->tm_min = ((spar1 & RTC_SPAR1_PWRON_MIN_MASK) >> RTC_SPAR1_PWRON_MIN_SHIFT);
	tm->tm_sec = ((spar0 & RTC_SPAR0_PWRON_SEC_MASK) >> RTC_SPAR0_PWRON_SEC_SHIFT);
}

void _rtc_set_pwron_alarm(void)
{
	rtc_write(RTC_PDN1, rtc_read(RTC_PDN1) & (~RTC_PDN1_PWRON_TIME));
	rtc_write(RTC_PDN2, rtc_read(RTC_PDN2) | RTC_PDN2_PWRON_ALARM);
	rtc_write_trigger();
}

#ifndef USER_BUILD_KERNEL
static void _rtc_lp_exception(u16 irqsta)
{
	u16 bbpu, irqen, osc32;
	u16 pwrkey1, pwrkey2, prot, con, sec1, sec2;

	bbpu = rtc_read(RTC_BBPU);
	irqen = rtc_read(RTC_IRQ_EN);
	osc32 = rtc_read(RTC_OSC32CON);
	pwrkey1 = rtc_read(RTC_POWERKEY1);
	pwrkey2 = rtc_read(RTC_POWERKEY2);
	prot = rtc_read(RTC_PROT);
	con = rtc_read(RTC_CON);
	sec1 = rtc_read(RTC_TC_SEC);
	msleep(2000);
	sec2 = rtc_read(RTC_TC_SEC);

	pr_err("!!! 32K WAS STOPPED !!!\n"
		       "RTC_BBPU      = 0x%x\n"
		       "RTC_IRQ_STA   = 0x%x\n"
		       "RTC_IRQ_EN    = 0x%x\n"
		       "RTC_OSC32CON  = 0x%x\n"
		       "RTC_POWERKEY1 = 0x%x\n"
		       "RTC_POWERKEY2 = 0x%x\n"
		       "RTC_PROT      = 0x%x\n"
		       "RTC_CON       = 0x%x\n"
		       "RTC_TC_SEC    = %02d\n"
		       "RTC_TC_SEC    = %02d\n",
		       bbpu, irqsta, irqen, osc32, pwrkey1, pwrkey2, prot, con, sec1, sec2);
}
#endif

static bool _rtc_is_lp_irq(void)
{
	u16 irqsta;
	bool ret = false;

	irqsta = rtc_read(RTC_IRQ_STA);	/* read clear */
	if (unlikely(!(irqsta & RTC_IRQ_STA_AL))) {
#ifndef USER_BUILD_KERNEL
		if (irqsta & RTC_IRQ_STA_LP)
			_rtc_lp_exception(irqsta);
#endif
		ret = true;
	}

	return ret;
}

static void _rtc_reload_power(void)
{
	/* set AUTO bit because AUTO = 0 when PWREN = 1 and alarm occurs */
	u16 bbpu;

	bbpu = rtc_read(RTC_BBPU) | RTC_BBPU_KEY | RTC_BBPU_AUTO;
	rtc_write(RTC_BBPU, bbpu);
	rtc_write_trigger();
}

static void _rtc_get_tick(struct rtc_time *tm)
{
	tm->tm_sec = rtc_read(RTC_TC_SEC);
	tm->tm_min = rtc_read(RTC_TC_MIN);
	tm->tm_hour = rtc_read(RTC_TC_HOU);
	tm->tm_mday = rtc_read(RTC_TC_DOM);
	tm->tm_mon = rtc_read(RTC_TC_MTH);
	tm->tm_year = rtc_read(RTC_TC_YEA);
}

static void _rtc_get_tick_time(struct rtc_time *tm)
{
	_rtc_get_tick(tm);
	if (rtc_read(RTC_TC_SEC) < tm->tm_sec) {	/* SEC has carried */
		_rtc_get_tick(tm);
	}
}

static void _rtc_set_tick_time(struct rtc_time *tm)
{
	rtc_write(RTC_TC_YEA, tm->tm_year);
	rtc_write(RTC_TC_MTH, tm->tm_mon);
	rtc_write(RTC_TC_DOM, tm->tm_mday);
	rtc_write(RTC_TC_HOU, tm->tm_hour);
	rtc_write(RTC_TC_MIN, tm->tm_min);
	rtc_write(RTC_TC_SEC, tm->tm_sec);
	rtc_write_trigger();
}

static bool _rtc_check_pwron_alarm_rg(struct rtc_time *nowtm, struct rtc_time *tm)
{
	u16 pdn1, pdn2, spar0, spar1;
	bool ret = false;

	pdn1 = rtc_read(RTC_PDN1);
	pdn2 = rtc_read(RTC_PDN2);
	spar0 = rtc_read(RTC_SPAR0);
	spar1 = rtc_read(RTC_SPAR1);
	pr_debug("pdn1 = 0x%4x\n", pdn1);

	if (pdn1 & RTC_PDN1_PWRON_TIME) {	/* power-on time is available */

		pr_debug("pdn1 = 0x%4x\n", pdn1);
		_rtc_get_tick_time(nowtm);

		tm->tm_sec = ((spar0 & RTC_SPAR0_PWRON_SEC_MASK) >> RTC_SPAR0_PWRON_SEC_SHIFT);
		tm->tm_min = ((spar1 & RTC_SPAR1_PWRON_MIN_MASK) >> RTC_SPAR1_PWRON_MIN_SHIFT);
		tm->tm_hour = ((spar1 & RTC_SPAR1_PWRON_HOU_MASK) >> RTC_SPAR1_PWRON_HOU_SHIFT);
		tm->tm_mday = ((spar1 & RTC_SPAR1_PWRON_DOM_MASK) >> RTC_SPAR1_PWRON_DOM_SHIFT);
		tm->tm_mon = ((pdn2 & RTC_PDN2_PWRON_MTH_MASK) >> RTC_PDN2_PWRON_MTH_SHIFT);
		tm->tm_year = ((pdn2 & RTC_PDN2_PWRON_YEA_MASK) >> RTC_PDN2_PWRON_YEA_SHIFT);

		ret = true;
	}

	return ret;
}

static void _rtc_get_alarm_time(struct rtc_time *tm, struct rtc_wkalrm *alm)
{
	u16 irqen, pdn2;

	irqen = rtc_read(RTC_IRQ_EN);
	tm->tm_sec = rtc_read(RTC_AL_SEC);
	tm->tm_min = rtc_read(RTC_AL_MIN);
	tm->tm_hour = rtc_read(RTC_AL_HOU) & RTC_AL_HOU_MASK;
	tm->tm_mday = rtc_read(RTC_AL_DOM) & RTC_AL_DOM_MASK;
	tm->tm_mon = rtc_read(RTC_AL_MTH) & RTC_AL_MTH_MASK;
	tm->tm_year = rtc_read(RTC_AL_YEA);
	pdn2 = rtc_read(RTC_PDN2);
	alm->enabled = !!(irqen & RTC_IRQ_EN_AL);
	alm->pending = !!(pdn2 & RTC_PDN2_PWRON_ALARM);	/* return Power-On Alarm bit */
}

void _rtc_set_alarm_time(struct rtc_time *tm)
{
	u16 irqen;
	pr_debug("read tc time = %04d/%02d/%02d (%d) %02d:%02d:%02d\n",
		      tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		      tm->tm_wday, tm->tm_hour, tm->tm_min, tm->tm_sec);
	pr_debug("a = %d\n", (rtc_read(RTC_AL_MTH) & (RTC_NEW_SPARE3)) | tm->tm_mon);
	pr_debug("b = %d\n", (rtc_read(RTC_AL_DOM) & (RTC_NEW_SPARE1)) | tm->tm_mday);
	pr_debug("c = %d\n", (rtc_read(RTC_AL_HOU) & (RTC_NEW_SPARE_FG_MASK)) | tm->tm_hour);
	rtc_write(RTC_AL_YEA, tm->tm_year);
	rtc_write(RTC_AL_MTH, (rtc_read(RTC_AL_MTH) & (RTC_NEW_SPARE3)) | tm->tm_mon);
	rtc_write(RTC_AL_DOM, (rtc_read(RTC_AL_DOM) & (RTC_NEW_SPARE1)) | tm->tm_mday);
	rtc_write(RTC_AL_HOU, (rtc_read(RTC_AL_HOU) & (RTC_NEW_SPARE_FG_MASK)) | tm->tm_hour);
	rtc_write(RTC_AL_MIN, tm->tm_min);
	rtc_write(RTC_AL_SEC, tm->tm_sec);
	rtc_write(RTC_AL_MASK, RTC_AL_MASK_DOW);	/* mask DOW */
	rtc_write_trigger();
	irqen = rtc_read(RTC_IRQ_EN) | RTC_IRQ_EN_ONESHOT_AL;
	rtc_write(RTC_IRQ_EN, irqen);
	rtc_write_trigger();
}

static void _rtc_clear_alarm(void)
{
	u16 irqsta, irqen, pdn2;

	irqen = rtc_read(RTC_IRQ_EN) & ~RTC_IRQ_EN_AL;
	pdn2 = rtc_read(RTC_PDN2) & ~RTC_PDN2_PWRON_ALARM;
	rtc_write(RTC_IRQ_EN, irqen);
	rtc_write(RTC_PDN2, pdn2);
	rtc_write_trigger();
	irqsta = rtc_read(RTC_IRQ_STA);	/* read clear */
}

void _rtc_set_lp_irq(void)
{
	u16 irqen;

	rtc_writeif_unlock();
#ifndef USER_BUILD_KERNEL
	irqen = rtc_read(RTC_IRQ_EN) | RTC_IRQ_EN_LP;
#else
	irqen = rtc_read(RTC_IRQ_EN) & ~RTC_IRQ_EN_LP;
#endif
	rtc_write(RTC_IRQ_EN, irqen);
	rtc_write_trigger();
}

static void _rtc_read_rg(void)
{
	u16 irqen, pdn1;

	irqen = rtc_read(RTC_IRQ_EN);
	pdn1 = rtc_read(RTC_PDN1);

	pr_debug("RTC_IRQ_EN = 0x%x, RTC_PDN1 = 0x%x\n", irqen, pdn1);
}

static void _rtc_save_pwron_time(bool enable, struct rtc_time *tm, bool logo)
{
	u16 pdn1, pdn2, spar0, spar1;

	pdn2 =
	    rtc_read(RTC_PDN2) & ~(RTC_PDN2_PWRON_MTH_MASK | RTC_PDN2_PWRON_YEA_MASK |
				   RTC_PDN2_PWRON_LOGO);
	pdn2 |=
	    (tm->tm_year << RTC_PDN2_PWRON_YEA_SHIFT) | (tm->tm_mon << RTC_PDN2_PWRON_MTH_SHIFT);
	if (logo)
		pdn2 |= RTC_PDN2_PWRON_LOGO;

	spar1 =
	    (tm->tm_mday << RTC_SPAR1_PWRON_DOM_SHIFT) | (tm->
							  tm_hour << RTC_SPAR1_PWRON_HOU_SHIFT) |
	    (tm->tm_min << RTC_SPAR1_PWRON_MIN_SHIFT);
	spar0 = rtc_read(RTC_SPAR0) & ~RTC_SPAR0_PWRON_SEC_MASK;
	tm->tm_sec &= RTC_SPAR0_PWRON_SEC_MASK;
	spar0 |= tm->tm_sec << RTC_SPAR0_PWRON_SEC_SHIFT;

	rtc_write(RTC_PDN2, pdn2);
	rtc_write(RTC_SPAR1, spar1);
	rtc_write(RTC_SPAR0, spar0);
	if (enable) {
		pdn1 = rtc_read(RTC_PDN1) | RTC_PDN1_PWRON_TIME;
		rtc_write(RTC_PDN1, pdn1);
	} else {
		pdn1 = rtc_read(RTC_PDN1) & ~RTC_PDN1_PWRON_TIME;
		rtc_write(RTC_PDN1, pdn1);
	}
	rtc_write_trigger();
}

void rtc_write_dcxo_c2(u16 val, bool flag)
{
	u16 tmp, rg_dcxo_fsm_c2, value;
	u32 dcxo_trim1_c2, dcxo_trim2_c2, rgs_dcxo_c2;

	rtc_get_lock();
	pr_debug("val = 0x%4x,falg = %d\n", val, flag);
	tmp = rtc_read(RG_DCXO_CON1) & 0xff00;
	dcxo_trim1_c2 = upmu_get_rgs_dcxo_trim1_c2();
	dcxo_trim2_c2 = upmu_get_rgs_dcxo_trim2_c2();
	rgs_dcxo_c2 = ((rtc_read(RG_DCXO_TRIM_RO1) & 0x1) == 1) ? dcxo_trim1_c2 : dcxo_trim2_c2;
	if (flag)
		value = (u16) rgs_dcxo_c2 + val;
	else
		value = (u16) rgs_dcxo_c2 - val;

	pr_debug(" dcxo_trim1_c2 = 0x%4x,dcxo_trim2_c2 = 0x%4x\n", dcxo_trim1_c2,
		      dcxo_trim2_c2);
	pr_debug(" rgs_dcxo_c2 = 0x%4x,value = %x\n", rgs_dcxo_c2, value);
	rtc_write(RG_DCXO_CON1, tmp | value);

	upmu_set_rg_dcxo_manual_c2(0x1);
	upmu_set_rg_dcxo_manual_sync_en(0x1);
	upmu_set_rg_dcxo_manual_c1c2_sync_en(0x1);

	udelay(1);

	upmu_set_rg_dcxo_manual_c1c2_sync_en(0x0);
	upmu_set_rg_dcxo_manual_sync_en(0x0);
	upmu_set_rg_dcxo_manual_c2(0x0);
	upmu_set_rg_dcxo_c2_untrim(0x1);

	rg_dcxo_fsm_c2 = rtc_read(RG_DCXO_CON3) & 0xff00;
	rgs_dcxo_c2 = upmu_get_rgs_dcxo_c2();
	if (flag)
		value = (u16) rgs_dcxo_c2 + val;
	else
		value = (u16) rgs_dcxo_c2 - val;

	pr_debug(" rgs_dcxo_c2 = 0x%4x,value = %x\n", rgs_dcxo_c2, value);
	rtc_write(RG_DCXO_CON3, rg_dcxo_fsm_c2 | value);
	rtc_put_lock();
}
EXPORT_SYMBOL(rtc_write_dcxo_c2);

/*
 * RTC_PDN1:
 *     bit 0 - 3  : Android bits
 *     bit 4 - 5  : Recovery bits (0x10: factory data reset)
 *     bit 6      : Bypass PWRKEY bit
 *     bit 7      : Power-On Time bit
 *     bit 8      : RTC_GPIO_USER_WIFI bit
 *     bit 9      : RTC_GPIO_USER_GPS bit
 *     bit 10     : RTC_GPIO_USER_BT bit
 *     bit 11     : RTC_GPIO_USER_FM bit
 *     bit 12     : RTC_GPIO_USER_PMIC bit
 *     bit 13     : Fast Boot
 *     bit 14	  : Kernel Power Off Charging
 *     bit 15     : Debug bit
 */

/*
 * RTC_PDN2:
 *     bit 0 - 3 : MTH in power-on time
 *     bit 4     : Power-On Alarm bit
 *     bit 5 - 6 : UART bits
 *     bit 7     : reserved bit
 *     bit 8 - 14: YEA in power-on time
 *     bit 15    : Power-On Logo bit
 */

/*
 * RTC_SPAR0:
 *     bit 0 - 5 : SEC in power-on time
 *     bit 6	 : 32K less bit. True:with 32K, False:Without 32K
 *     bit 7 - 15: reserved bits
 */

/*
 * RTC_SPAR1:
 *     bit 0 - 5  : MIN in power-on time
 *     bit 6 - 10 : HOU in power-on time
 *     bit 11 - 15: DOM in power-on time
 */


/*
 * RTC_NEW_SPARE0: RTC_AL_HOU bit8~15
 *	   bit 8 ~ 14 : Fuel Gauge
 *     bit 15     : reserved bits
 */

/*
 * RTC_NEW_SPARE1: RTC_AL_DOM bit8~15
 *	   bit 8 ~ 15 : reserved bits
 */

/*
 * RTC_NEW_SPARE2: RTC_AL_DOW bit8~15
 *	   bit 8 ~ 15 : reserved bits
 */

/*
 * RTC_NEW_SPARE3: RTC_AL_MTH bit8~15
 *	   bit 8 ~ 15 : reserved bits
 */

static unsigned long _rtc_read_hw_time(void)
{
	unsigned long time;
	struct rtc_time tm;

	_rtc_get_tick_time(&tm);
	tm.tm_year += RTC_MIN_YEAR_OFFSET;
	tm.tm_mon--;
	rtc_tm_to_time(&tm, &time);
	tm.tm_wday = (time / 86400 + 4) % 7;	/* 1970/01/01 is Thursday */

	return time;
}

unsigned long rtc_read_hw_time(void)
{
	unsigned long time;

	rtc_get_lock();
	time = _rtc_read_hw_time();
	rtc_put_lock();

	return time;
}
EXPORT_SYMBOL(rtc_read_hw_time);

int get_rtc_spare_fg_value(void)
{
	/* RTC_AL_HOU bit8~14 */
	u16 temp;

	rtc_get_lock();
	temp = _rtc_get_register_status("FG");
	rtc_put_lock();

	return temp;
}
EXPORT_SYMBOL(get_rtc_spare_fg_value);

int set_rtc_spare_fg_value(int val)
{
	/* RTC_AL_HOU bit8~14 */
	if (val > 100)
		return 1;

	rtc_get_lock();
	_rtc_set_register_status("FG", val);
	rtc_put_lock();

	return 0;
}
EXPORT_SYMBOL(set_rtc_spare_fg_value);

bool rtc_crystal_exist_status(void)
{
	u16 ret;

	rtc_get_lock();
	ret = _rtc_get_register_status("XTAL");
	rtc_put_lock();

	return ret != 0;
}
EXPORT_SYMBOL(rtc_crystal_exist_status);

bool rtc_lprst_detected(void)
{
	u16 ret;

	rtc_get_lock();
	ret = _rtc_get_register_status("LPRST");
	rtc_put_lock();

	return ret;
}
EXPORT_SYMBOL(rtc_lprst_detected);

bool rtc_enter_kpoc_detected(void)
{
	u16 ret;

	rtc_get_lock();
	ret = _rtc_get_register_status("ENTER_KPOC");
	rtc_put_lock();

	return ret;
}
EXPORT_SYMBOL(rtc_enter_kpoc_detected);

int rtc_get_reboot_reason(void)
{
	u16 ret;

	rtc_get_lock();
	ret = _rtc_get_register_status("REBOOT_REASON");
	rtc_put_lock();

	return ret;
}
EXPORT_SYMBOL(rtc_get_reboot_reason);

/*
* Only for GPS to check the status.
* Others do not use this API
* This low power detected API is read clear.
*/
bool rtc_low_power_detected(void)
{
	u16 ret;

	rtc_get_lock();
	ret = _rtc_get_register_status("LPD");
	rtc_put_lock();

	return ret != 0;
}
EXPORT_SYMBOL(rtc_low_power_detected);

void rtc_gpio_enable_32k(enum rtc_gpio_user user)
{
	if (user < RTC_GPIO_USER_WIFI || user > RTC_GPIO_USER_PMIC)
		return;

	rtc_get_lock();
	_rtc_set_gpio_32k_status(user, true);
	rtc_put_lock();
}
EXPORT_SYMBOL(rtc_gpio_enable_32k);

void rtc_gpio_disable_32k(enum rtc_gpio_user user)
{
	if (user < RTC_GPIO_USER_WIFI || user > RTC_GPIO_USER_PMIC)
		return;

	rtc_get_lock();
	_rtc_set_gpio_32k_status(user, false);
	rtc_put_lock();
}
EXPORT_SYMBOL(rtc_gpio_disable_32k);

bool rtc_gpio_32k_status(void)
{
	u16 ret;

	rtc_get_lock();
	ret = _rtc_get_register_status("GPIO");
	rtc_put_lock();

	return ret != 0;
}
EXPORT_SYMBOL(rtc_gpio_32k_status);

void rtc_enable_abb_32k(void)
{
	rtc_get_lock();
	_rtc_set_register_status("ABB", 1);
	rtc_put_lock();
}
EXPORT_SYMBOL(rtc_enable_abb_32k);

void rtc_disable_abb_32k(void)
{
	rtc_get_lock();
	_rtc_set_register_status("ABB", 0);
	rtc_put_lock();
}
EXPORT_SYMBOL(rtc_disable_abb_32k);

void rtc_enable_writeif(void)
{
	rtc_get_lock();
	rtc_set_writeif(true);
	rtc_put_lock();
}
EXPORT_SYMBOL(rtc_enable_writeif);

void rtc_disable_writeif(void)
{
	rtc_get_lock();
	rtc_set_writeif(false);
	rtc_put_lock();
}
EXPORT_SYMBOL(rtc_disable_writeif);

void rtc_mark_recovery(void)
{
	rtc_get_lock();
	_rtc_mark_mode("recv");
	rtc_put_lock();
}
EXPORT_SYMBOL(rtc_mark_recovery);

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
void rtc_mark_kpoc(void)
{
	rtc_get_lock();
	_rtc_mark_mode("kpoc");
	rtc_put_lock();
}
EXPORT_SYMBOL(rtc_mark_kpoc);

void rtc_mark_enter_kpoc(void)
{
	rtc_get_lock();
	_rtc_mark_mode("enter_kpoc");
	rtc_put_lock();
}
EXPORT_SYMBOL(rtc_mark_enter_kpoc);
#endif
void rtc_mark_fast(void)
{
	rtc_get_lock();
	_rtc_mark_mode("fast");
	rtc_put_lock();
}
EXPORT_SYMBOL(rtc_mark_fast);

void rtc_mark_emergency(void)
{
	rtc_get_lock();
	_rtc_mark_mode("emergency");
	rtc_put_lock();
}
EXPORT_SYMBOL(rtc_mark_emergency);

void rtc_mark_clear_lprst(void)
{
	rtc_get_lock();
	_rtc_mark_mode("clear_lprst");
	rtc_put_lock();
}
EXPORT_SYMBOL(rtc_mark_clear_lprst);

void rtc_mark_enter_lprst(void)
{
	rtc_get_lock();
	_rtc_mark_mode("enter_lprst");
	rtc_put_lock();
}
EXPORT_SYMBOL(rtc_mark_enter_lprst);

void rtc_mark_enter_sw_lprst(void)
{
	rtc_get_lock();
	_rtc_mark_mode("enter_sw_lprst");
	rtc_put_lock();
}
EXPORT_SYMBOL(rtc_mark_enter_sw_lprst);

void rtc_mark_reboot_reason(int mode)
{
	char buf[8];
	strcpy(buf, "reboot?");
	buf[6] = (mode & RTC_SPAR0_REBOOT_REASON_MASK) + '0';

	rtc_get_lock();
	_rtc_mark_mode(buf);
	rtc_put_lock();
}
EXPORT_SYMBOL(rtc_mark_reboot_reason);

u16 rtc_rdwr_uart_bits(u16 *val)
{
	u16 ret;

	rtc_get_lock();
	ret = _rtc_rdwr_uart(val);
	rtc_put_lock();

	return ret;
}
EXPORT_SYMBOL(rtc_rdwr_uart_bits);

/* called at shutdown time to power off in a single core/single threaded environment;
 * no locking is necessary here */
void rtc_bbpu_power_down(void)
{
	_rtc_bbpu_pwdn();
}
EXPORT_SYMBOL(rtc_bbpu_power_down);

void rtc_read_pwron_alarm(struct rtc_wkalrm *alm)
{
	struct rtc_time *tm;

	if (alm == NULL)
		return;
	tm = &alm->time;
	rtc_get_lock();
	_rtc_get_pwron_alarm(tm, alm);
	rtc_put_lock();
	tm->tm_year += RTC_MIN_YEAR_OFFSET;
	tm->tm_mon -= 1;
	if (rtc_show_alarm) {
		pr_debug("power-on = %04d/%02d/%02d %02d:%02d:%02d (%d)(%d)\n",
			  tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			  tm->tm_hour, tm->tm_min, tm->tm_sec, alm->enabled, alm->pending);
	}
}
EXPORT_SYMBOL(rtc_read_pwron_alarm);

/* static void rtc_tasklet_handler(unsigned long data) */
static void rtc_handler(void)
{
	bool pwron_alm = false, isLowPowerIrq = false, pwron_alarm = false;
	struct rtc_time nowtm;
	struct rtc_time tm;
	pr_debug("rtc_tasklet_handler start\n");

	rtc_get_lock();
	isLowPowerIrq = _rtc_is_lp_irq();
	if (isLowPowerIrq) {
		rtc_put_lock();
		return;
	}
#if RTC_RELPWR_WHEN_XRST
	/* set AUTO bit because AUTO = 0 when PWREN = 1 and alarm occurs */
	_rtc_reload_power();
#endif
	pwron_alarm = _rtc_check_pwron_alarm_rg(&nowtm, &tm);
	nowtm.tm_year += RTC_MIN_YEAR;
	tm.tm_year += RTC_MIN_YEAR;
	if (pwron_alarm) {
		unsigned long now_time, time;

		now_time =
		    mktime(nowtm.tm_year, nowtm.tm_mon, nowtm.tm_mday, nowtm.tm_hour, nowtm.tm_min,
			   nowtm.tm_sec);
		time = mktime(tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

		if (now_time >= time - 1 && now_time <= time + 4) {	/* power on */
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
			if (g_boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
			    || g_boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
				tm.tm_year -= RTC_MIN_YEAR;
				tm.tm_sec += 1;
				_rtc_set_alarm_time(&tm);
				_rtc_mark_mode("kpoc");
				orderly_reboot(true);
			} else {
				_rtc_set_pwron_alarm();
				pwron_alm = true;
			}
#else
			hal_rtc_set_pwron_alarm();
			pwron_alm = true;
#endif
		} else if (now_time < time) {	/* set power-on alarm */
			tm.tm_sec -= 1;
			_rtc_set_alarm_time(&tm);
		}
	}
	rtc_put_lock();

	/* schedule work item for execution */
	rtc_update_irq(rtc, 1, RTC_IRQF | RTC_AF);
	/* wait for work item to finish running */
	flush_work(&rtc->irqwork);

	if (rtc_show_alarm)
		pr_debug("%s time is up\n", pwron_alm ? "power-on" : "alarm");

}

static irqreturn_t rtc_irq_handler_thread(int irq, void *data)
{
	rtc_handler();
	msleep(100);
	return IRQ_HANDLED;
}

#if RTC_OVER_TIME_RESET
static void rtc_reset_to_deftime(struct rtc_time *tm)
{
	struct rtc_time defaulttm;

	tm->tm_year = RTC_DEFAULT_YEA - 1900;
	tm->tm_mon = RTC_DEFAULT_MTH - 1;
	tm->tm_mday = RTC_DEFAULT_DOM;
	tm->tm_wday = 1;
	tm->tm_hour = 0;
	tm->tm_min = 0;
	tm->tm_sec = 0;

	/* set default alarm time */
	defaulttm.tm_year = RTC_DEFAULT_YEA - RTC_MIN_YEAR;
	defaulttm.tm_mon = RTC_DEFAULT_MTH;
	defaulttm.tm_mday = RTC_DEFAULT_DOM;
	defaulttm.tm_wday = 1;
	defaulttm.tm_hour = 0;
	defaulttm.tm_min = 0;
	defaulttm.tm_sec = 0;
	rtc_get_lock();
	_rtc_set_alarm_time(&defaulttm);
	rtc_put_lock();

	pr_err("reset to default date %04d/%02d/%02d\n",
		   RTC_DEFAULT_YEA, RTC_DEFAULT_MTH, RTC_DEFAULT_DOM);
}
#endif

static int rtc_ops_read_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long time;

	rtc_get_lock();
	_rtc_get_tick_time(tm);
	rtc_put_lock();

	tm->tm_year += RTC_MIN_YEAR_OFFSET;
	tm->tm_mon--;
	rtc_tm_to_time(tm, &time);
#if RTC_OVER_TIME_RESET
	if (unlikely(time > (unsigned long)LONG_MAX)) {
		rtc_reset_to_deftime(tm);
		rtc_tm_to_time(tm, &time);
	}
#endif
	tm->tm_wday = (time / 86400 + 4) % 7;	/* 1970/01/01 is Thursday */

	if (rtc_show_time) {
		pr_debug("read tc time = %04d/%02d/%02d (%d) %02d:%02d:%02d\n",
			  tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			  tm->tm_wday, tm->tm_hour, tm->tm_min, tm->tm_sec);
	}

	return 0;
}

static int rtc_ops_set_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long time;

	rtc_tm_to_time(tm, &time);
	if (time > (unsigned long)LONG_MAX)
		return -EINVAL;

	tm->tm_year -= RTC_MIN_YEAR_OFFSET;
	tm->tm_mon++;

	pr_debug("set tc time = %04d/%02d/%02d %02d:%02d:%02d\n",
		  tm->tm_year + RTC_MIN_YEAR, tm->tm_mon, tm->tm_mday,
		  tm->tm_hour, tm->tm_min, tm->tm_sec);

	rtc_get_lock();
	_rtc_set_tick_time(tm);
	rtc_put_lock();

	return 0;
}

static int rtc_ops_read_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	struct rtc_time *tm = &alm->time;

	rtc_get_lock();
	_rtc_get_alarm_time(tm, alm);
	rtc_put_lock();

	tm->tm_year += RTC_MIN_YEAR_OFFSET;
	tm->tm_mon--;

	pr_debug("read al time = %04d/%02d/%02d %02d:%02d:%02d (%d)\n",
		  tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		  tm->tm_hour, tm->tm_min, tm->tm_sec, alm->enabled);

	return 0;
}

static int rtc_ops_set_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	unsigned long time;
	struct rtc_time *tm = &alm->time;

	rtc_tm_to_time(tm, &time);
	if (time > (unsigned long)LONG_MAX)
		return -EINVAL;

	tm->tm_year -= RTC_MIN_YEAR_OFFSET;
	tm->tm_mon++;

	pr_debug("set al time = %04d/%02d/%02d %02d:%02d:%02d (%d)\n",
		  tm->tm_year + RTC_MIN_YEAR, tm->tm_mon, tm->tm_mday,
		  tm->tm_hour, tm->tm_min, tm->tm_sec, alm->enabled);

	rtc_get_lock();
	if (alm->enabled == 2) {	/* enable power-on alarm */
		_rtc_save_pwron_time(true, tm, false);
	} else if (alm->enabled == 3) {	/* enable power-on alarm with logo */
		_rtc_save_pwron_time(true, tm, true);
	} else if (alm->enabled == 4) {	/* disable power-on alarm */
		alm->enabled = 0;
		_rtc_save_pwron_time(false, tm, false);
	}

	/* disable alarm and clear Power-On Alarm bit */
	_rtc_clear_alarm();

	if (alm->enabled)
		_rtc_set_alarm_time(tm);

	rtc_put_lock();

	return 0;
}

static struct rtc_class_ops rtc_ops = {
	.read_time = rtc_ops_read_time,
	.set_time = rtc_ops_set_time,
	.read_alarm = rtc_ops_read_alarm,
	.set_alarm = rtc_ops_set_alarm,
};

static int rtc_pdrv_probe(struct platform_device *pdev)
{
	int ret;

	/* only enable LPD interrupt in engineering build */
	rtc_get_lock();
	_rtc_set_lp_irq();
	rtc_put_lock();

	/* register rtc device (/dev/rtc0) */
	rtc = rtc_device_register(RTC_NAME, &pdev->dev, &rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc)) {
		pr_err("register rtc device failed (%ld)\n", PTR_ERR(rtc));
		return PTR_ERR(rtc);
	}

	ret = request_threaded_irq(PMIC_IRQ(RG_INT_STATUS_RTC),	NULL,
		rtc_irq_handler_thread, IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "mtk-rtc", rtc);
	if (ret < 0) {
		pr_err("%s: failed to request RTC interrupt: err=%d\n", __func__, ret);
		rtc_device_unregister(rtc);
		rtc = 0;
		return ret;
	}

	irq_set_irq_wake(PMIC_IRQ(RG_INT_STATUS_RTC), 1);
	device_init_wakeup(&pdev->dev, 1);
	return 0;
}

/* should never be called */
static int rtc_pdrv_remove(struct platform_device *pdev)
{
	free_irq(PMIC_IRQ(RG_INT_STATUS_RTC), rtc);
	rtc_device_unregister(rtc);
	return 0;
}

static struct platform_driver rtc_pdrv = {
	.probe = rtc_pdrv_probe,
	.remove = rtc_pdrv_remove,
	.driver = {
		   .name = RTC_NAME,
		   .owner = THIS_MODULE,
		   },
};

static struct platform_device rtc_pdev = {
	.name = RTC_NAME,
	.id = -1,
};

static int __init rtc_subsys_init(void)
{
	int r;
	pr_debug("rtc_init");

	r = platform_device_register(&rtc_pdev);
	if (r) {
		pr_err("register device failed (%d)\n", r);
		return r;
	}

	r = platform_driver_register(&rtc_pdrv);
	if (r) {
		pr_err("register driver failed (%d)\n", r);
		platform_device_unregister(&rtc_pdev);
		return r;
	}
#if (defined(MTK_GPS_MT3332))
	_rtc_set_gpio_32k_status(0, true);
#endif

	return 0;
}

static int __init rtc_late_init(void)
{
	rtc_get_lock();
	_rtc_read_rg();
	rtc_put_lock();

	if (rtc_crystal_exist_status() == true)
		pr_debug("There is Crystal\n");
	else
		pr_debug("There is no Crystal\n");

#if (defined(MTK_GPS_MT3332))
	_rtc_set_gpio_32k_status(0, true);
#endif

	return 0;
}

late_initcall(rtc_late_init);
fs_initcall(rtc_subsys_init);

module_param(rtc_show_time, int, 0644);
module_param(rtc_show_alarm, int, 0644);

MODULE_LICENSE("GPL");
