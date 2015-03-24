/*
 * Copyright (C) 2010 MediaTek, Inc.
 *
 * Author: Terry Chang <terry.chang@mediatek.com>
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

#ifndef _MTK_KPD_H_
#define _MTK_KPD_H_

#include <mach/mt_pm_ldo.h>
/* include PMIC header file */
#include <mach/mt_typedefs.h>
#include <mach/pmic_mt6329_hw_bank1.h>
#include <mach/pmic_mt6329_sw_bank1.h>
#include <mach/pmic_mt6329_hw.h>
#include <mach/pmic_mt6329_sw.h>
#include <mach/pmic_mt6320_sw.h>
/* #include <mach/upmu_common_sw.h> */
#include <mach/upmu_hw.h>

#define KPD_PWRKEY_USE_PMIC
#define KPD_BACKLIGHT_TIME	8	/* sec */
/* the keys can wake up the system and we should enable backlight */
#define KPD_BACKLIGHT_WAKE_KEY	\
{				\
	KEY_ENDCALL, KEY_POWER,	\
}

#define KPD_SLIDE_EINT		CUST_EINT_KPD_SLIDE_NUM
#define KPD_SLIDE_DEBOUNCE	CUST_EINT_KPD_SLIDE_DEBOUNCE_CN	/* ms */
#define KPD_SLIDE_POLARITY	CUST_EINT_KPD_SLIDE_POLARITY
#define KPD_SLIDE_SENSITIVE	CUST_EINT_KPD_SLIDE_SENSITIVE

#ifdef KPD_PWRKEY_USE_PMIC
void kpd_pwrkey_pmic_handler(unsigned long pressed);
#endif

void kpd_pmic_rstkey_handler(unsigned long pressed);

#endif
