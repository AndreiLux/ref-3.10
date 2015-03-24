/*
 * Copyright (C) 2011 MediaTek, Inc.
 *
 * Author: Holmes Chiou <holmes.chiou@mediatek.com>
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/sched_clock.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <board-custom.h>


#include "mach/mt_freqhopping.h"
#include "mach/mt_fhreg.h"
#include "mach/mt_clkmgr.h"
#include "mach/mt_typedefs.h"
#include "mach/mt_gpufreq.h"
#include "mach/mt_cpufreq.h"
#include "mach/emi_bwl.h"
#include "mach/sync_write.h"
#include "mach/mt_sleep.h"

#include <mach/mt_freqhopping_drv.h>

#include <linux/seq_file.h>

/* #define FH_MSG printk //TODO */
/* #define FH_BUG_ON(x) pr_info("BUGON %s:%d %s:%d",__FUNCTION__,__LINE__,current->comm,current->pid)//TODO */
/* #define FH_BUG_ON(...) //TODO */

#define MT_FH_CLK_GEN 0
#define MT_FH_SUSPEND 1
#define USER_DEFINE_SETTING_ID 1

static DEFINE_SPINLOCK(freqhopping_lock);


/* current DRAMC@mempll */
static unsigned int g_curr_dramc = 266;	/* default @266MHz ==> LPDDR2/DDR3 data rate 1066 */

#if MT_FH_CLK_GEN
static unsigned int g_curr_clkgen = MT658X_FH_PLL_TOTAL_NUM + 1;	/* default clkgen ==> no clkgen output */
#endif

static unsigned char g_mempll_fh_table[8];

static unsigned int g_initialize;

static unsigned int *g_fh_rank1_pa;
static unsigned int *g_fh_rank1_va;
static unsigned int *g_fh_rank0_pa;
static unsigned int *g_fh_rank0_va;
static unsigned int g_clk_en;

#ifndef PER_PROJECT_FH_SETTING

#define MT_FH_DUMMY_READ	0

#define LOW_DRAMC_DDS		0x0010C000
#define LOW_DRAMC_INT		67	/* 233.5 */
#define LOW_DRAMC_FRACTION	0	/* 233.5 */
#define LOW_DRAMC		233	/* 233.5 */
#define LOW_DRAMC_FREQ		233500

#define LVDS_PLL_IS_ON		0

/* TODO: fill in the default freq & corresponding setting_id */
static fh_pll_t g_fh_pll[MT658X_FH_PLL_TOTAL_NUM] = {	/* keep track the status of each PLL */
	{FH_FH_DISABLE, FH_PLL_DISABLE, 0, 0, 0},	/* ARMPLL   default SSC disable */
	{FH_FH_DISABLE, FH_PLL_DISABLE, 0, 1612000, 0},	/* MAINPLL  default SSC disable */
	{FH_FH_DISABLE, FH_PLL_DISABLE, 0, 266000, 0},	/* MEMPLL   default SSC disable */
	{FH_FH_DISABLE, FH_PLL_DISABLE, 0, 1599000, 0},	/* MSDCPLL  default SSC enable */
	{FH_FH_DISABLE, FH_PLL_DISABLE, 0, 1188000, 0},	/* TVDPLL   default SSC disable */
	{FH_FH_DISABLE, FH_PLL_DISABLE, 0, 1200000, 0},	/* LVDSPLL  default SSC disable */
	{FH_FH_DISABLE, FH_PLL_DISABLE, 0, 1200000, 0},	/* ARMPLL2  default SSC disable */
	{FH_FH_DISABLE, FH_PLL_DISABLE, 0, 1200000, 0},	/* MMPLL  default SSC disable */
	{FH_FH_DISABLE, FH_PLL_DISABLE, 0, 1200000, 0},	/* VDECPLL  default SSC disable */
};

static fh_pll_t g_fh_pll_sus[MT658X_FH_PLL_TOTAL_NUM] = {	/* keep track the status of each PLL */
	{FH_FH_DISABLE, FH_PLL_DISABLE, 0, 0, 0},	/* ARMPLL   default SSC disable */
	{FH_FH_ENABLE_SSC, FH_PLL_DISABLE, 0, 1612000, 0},	/* MAINPLL  default SSC disable */
	{FH_FH_DISABLE, FH_PLL_DISABLE, 0, 266000, 0},	/* MEMPLL   default SSC disable */
	{FH_FH_DISABLE, FH_PLL_DISABLE, 0, 1599000, 0},	/* MSDCPLL  default SSC enable */
	{FH_FH_DISABLE, FH_PLL_DISABLE, 0, 1188000, 0},	/* TVDPLL   default SSC disable */
	{FH_FH_DISABLE, FH_PLL_DISABLE, 0, 1200000, 0},	/* LVDSPLL  default SSC disable */
	{FH_FH_DISABLE, FH_PLL_DISABLE, 0, 1200000, 0},	/* ARMPLL2  default SSC disable */
	{FH_FH_DISABLE, FH_PLL_DISABLE, 0, 1200000, 0},	/* MMPLL  default SSC disable */
	{FH_FH_DISABLE, FH_PLL_DISABLE, 0, 1200000, 0},	/* VDECPLL  default SSC disable */
};


/* ARMPLL */
static const struct freqhopping_ssc mt_ssc_armpll_setting[MT_SSC_NR_PREDEFINE_SETTING] = {
	{0, 0, 0, 0, 0, 0},	/* Means disable */
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},	/* Means User-Define */
	{715000, 0, 9, 0, 4, 0xDC000},	/* 715MHz , 0.27us, 0.023437500, 0 ~ -8% */
	{419250, 0, 9, 0, 4, 0x102000},	/* 419.25MHz , 0.27us, 0.023437500, 0 ~ -8% */
	{1833000, 0, 9, 0, 4, 0x0011A000},	/* 1833MHz ,46.15384615 */
	{1391000, 0, 9, 0, 4, 0x000D6000},	/* 1391MHz ,46.15384615 */
	{1209000, 0, 9, 0, 4, 0x000BA000},	/* 1209MHz ,46.15384615 */
	{1183000, 0, 9, 0, 4, 0x000B6000},	/* 1183MHz ,46.15384615 */
	{1092000, 0, 9, 0, 4, 0x000A8000},	/* 1092MHz ,46.15384615 */
	{1001000, 0, 9, 0, 4, 0x0009A000},	/* 1001MHz ,46.15384615 */
	{916500, 0, 9, 0, 4, 0x0008D000},	/* 916.5MHz ,46.15384615 */
	{832000, 0, 9, 0, 4, 0x00080000},	/* 832MHz ,46.15384615 */
	{695500, 0, 9, 0, 4, 0x0006B000},	/* 695.5MHz ,46.15384615 */
	{578500, 0, 9, 0, 4, 0x00059000},	/* 578.5MHz ,46.15384615 */
	{347750, 0, 9, 0, 4, 0x00035800},	/* 347.75MHz ,46.15384615 */
	{1430000, 0, 9, 0, 4, 0x000DC000},	/* 1430MHz ,46.15384615 */
	{1677000, 0, 9, 0, 4, 0x00102000},	/* 1677MHz ,46.15384615 */
	{0, 0, 0, 0, 0, 0}	/* EOF */
};

static const struct freqhopping_ssc mt_ssc_mainpll_setting[MT_SSC_NR_PREDEFINE_SETTING] = {
	{0, 0, 0, 0, 0, 0},	/* Means disable */
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},	/* Means User-Define */
	{230000, 0, 9, 0, 4, 0x0006A276},	/* 230MHz , 0.27us, 0.023437500, +4% ~ -4% */
	{460000, 0, 9, 0, 4, 0x000D44EC},	/* 460MHz , 0.27us, 0.023437500, +4% ~ -4% */
	{537333, 0, 9, 0, 4, 0x000F8000},	/* 537.333MHz , 0.27us, 0.023437500, +4% ~ -4% */
	{1612000, 0, 9, 0, 4, 0xF8000},	/* 1.209GHz , 0.27us, 0.023437500, +4% ~ -4% */
	{0, 0, 0, 0, 0, 0}	/* EOF */
};

static const struct freqhopping_ssc mt_ssc_mempll_setting[MT_SSC_NR_PREDEFINE_SETTING] = {
	{0, 0, 0, 0, 0, 0},	/* Means disable */
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},	/* Means User-Define */
	{104000, 0, 9, 0, 4, 0x000EE8BA},	/* 104MHz , 0.27us, 0.023437500, 0 ~ -4% */
	{133250, 0, 9, 0, 4, 0x00131A2E},	/* 133.25MHz , 0.27us, 0.023437500, 0 ~ -4% */
	{266000, 0, 9, 0, 4, 0x00131A2D},	/* 266MHz , 0.27us, 0.023437500, 0 ~ -4% */
	/* {266000 ,0 ,9 ,0, 0xC39B, 0x00131A2D},//266MHz , 0.27us, 0.023437500, 0 ~ -8% */
	/* {200000 ,0 ,9 ,0, 0x9312, 0x000E5CCC},//200MHz , 0.27us, 0.023437500, 0 ~ -8% */
	{0, 0, 0, 0, 0, 0}	/* EOF */
};

static const struct freqhopping_ssc mt_ssc_msdcpll_setting[MT_SSC_NR_PREDEFINE_SETTING] = {
	{0, 0, 0, 0, 0, 0},	/* Means disable */
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},	/* Means User-Define */
	{104000, 0, 9, 0, 4, 0x00080000},	/* 104MHz, ..., 61.5 ..0 ~ -8% */
	{26000, 0, 9, 0, 4, 0x00020000},	/* 104MHz, ..., 61.5 ..0 ~ -8% */
	{208000, 0, 9, 0, 4, 0x00100000},	/* 104MHz, ..., 61.5 ..0 ~ -8% */
	{1599000, 0, 9, 0, 4, 0xF6000},	/* 1599MHz, ..., 61.5 ..0 ~ -8% */
	{1352000, 0, 9, 0, 4, 0xD0000},	/* 1352MHz, ..., 52 ..0 ~ -8% */
	{0, 0, 0, 0, 0, 0}	/* EOF */

};

static const struct freqhopping_ssc mt_ssc_tvdpll_setting[MT_SSC_NR_PREDEFINE_SETTING] = {
	{0, 0, 0, 0, 0, 0},	/* Means disable */
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},	/* Means User-Define */
	{143000, 0, 9, 0, 4, 0x000B0000},	/* 143MHz ,45.69230769 0 ~ -8% */
	{208000, 0, 9, 0, 4, 0x00100000},	/* 208MHz ,45.69230769 0 ~ -8% */
	{148500, 0, 9, 0, 4, 0x000B6C4E},	/* 148.5MHz ,45.69230769 0 ~ -8% */
	{1188000, 0, 9, 0, 4, 0xB6C4F},	/* 1188MHz ,45.69230769 0 ~ -8% */
	{1728000, 0, 9, 0, 4, 0x109D89},	/* 1728MHz, 66.46153846 0 ~ -8% */
	{0, 0, 0, 0, 0, 0}	/* EOF */
};

static const struct freqhopping_ssc mt_ssc_lvdspll_setting[MT_SSC_NR_PREDEFINE_SETTING] = {
	{0, 0, 0, 0, 0, 0},	/* Means disable */
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},	/* Means User-Define */
	{71500, 0, 9, 0, 4, 0x000B0000},	/* 71.5MHz ,46.15384615 */
	{72000, 0, 9, 0, 4, 0x000B13B1},	/* 72MHz ,46.15384615 */
	{75000, 0, 9, 0, 4, 0x000B89D8},	/* 75MHz ,46.15384615 */
	{87500, 0, 9, 0, 4, 0x000D7627},	/* 87.5MHz ,46.15384615 */
	{1200000, 0, 9, 0, 4, 0xB89D8},	/* 1200MHz ,46.15384615 */
	{1400000, 0, 9, 0, 4, 0xDD89D},	/* 1400MHz, 55.38461538 */
	{0, 0, 0, 0, 0, 0}	/* EOF */
};

static const struct freqhopping_ssc mt_ssc_armpll2_setting[MT_SSC_NR_PREDEFINE_SETTING] = {
	{0, 0, 0, 0, 0, 0},	/* Means disable */
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},	/* Means User-Define */
	{715000, 0, 9, 0, 4, 0xDC000},	/* 715MHz , 0.27us, 0.023437500, 0 ~ -8% */
	{419250, 0, 9, 0, 4, 0x102000},	/* 419.25MHz , 0.27us, 0.023437500, 0 ~ -8% */
	{1833000, 0, 9, 0, 4, 0x0011A000},	/* 1833MHz ,46.15384615 */
	{1391000, 0, 9, 0, 4, 0x000D6000},	/* 1391MHz ,46.15384615 */
	{1209000, 0, 9, 0, 4, 0x000BA000},	/* 1209MHz ,46.15384615 */
	{1183000, 0, 9, 0, 4, 0x000B6000},	/* 1183MHz ,46.15384615 */
	{1092000, 0, 9, 0, 4, 0x000A8000},	/* 1092MHz ,46.15384615 */
	{1001000, 0, 9, 0, 4, 0x0009A000},	/* 1001MHz ,46.15384615 */
	{916500, 0, 9, 0, 4, 0x0008D000},	/* 916.5MHz ,46.15384615 */
	{832000, 0, 9, 0, 4, 0x00080000},	/* 832MHz ,46.15384615 */
	{695500, 0, 9, 0, 4, 0x0006B000},	/* 695.5MHz ,46.15384615 */
	{578500, 0, 9, 0, 4, 0x00059000},	/* 578.5MHz ,46.15384615 */
	{347750, 0, 9, 0, 4, 0x00035800},	/* 347.75MHz ,46.15384615 */
	{1430000, 0, 9, 0, 4, 0x000DC000},	/* 1430MHz ,46.15384615 */
	{1677000, 0, 9, 0, 4, 0x00102000},	/* 1677MHz ,46.15384615 */
	{0, 0, 0, 0, 0, 0}	/* EOF */
};

static const struct freqhopping_ssc mt_ssc_mmpll_setting[MT_SSC_NR_PREDEFINE_SETTING] = {
	{0, 0, 0, 0, 0, 0},	/* Means disable */
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},	/* Means User-Define */
	{624000, 0, 9, 0, 4, 0x000C0000},	/* 624MHz ,46.15384615 */
	{591500, 0, 9, 0, 4, 0x000B6000},	/* 5915MHz ,46.15384615 */
	{592500, 0, 9, 0, 4, 0x000B64EC},	/* 592.5MHz ,46.15384615 */
	{309000, 0, 9, 0, 4, 0x00078000},	/* 309MHz ,46.15384615 */
	{0, 0, 0, 0, 0, 0}	/* EOF */
};

static const struct freqhopping_ssc mt_ssc_vdecpll_setting[MT_SSC_NR_PREDEFINE_SETTING] = {
	{0, 0, 0, 0, 0, 0},	/* Means disable */
	{0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},	/* Means User-Define */
	{399000, 0, 9, 0, 4, 0x0F589D},	/* 399MHz ,46.15384615 */
	{326000, 0, 9, 0, 4, 0x0C89D8},	/* 326MHz, 55.38461538 */
	{325000, 0, 9, 0, 4, 0x0C8000},	/* 326MHz, 55.38461538 */
	{156000, 0, 9, 0, 4, 0x060000},	/* 326MHz, 55.38461538 */
	{0, 0, 0, 0, 0, 0}	/* EOF */
};

static struct freqhopping_ssc mt_ssc_fhpll_userdefined[MT_FHPLL_MAX] = {
	{0, 1, 1, 2, 2, 0},	/* ARMPLL */
	{0, 1, 1, 2, 2, 0},	/* MAINPLL */
	{0, 1, 1, 2, 2, 0},	/* MEMPLL */
	{0, 1, 1, 2, 2, 0},	/* MSDCPLL */
	{0, 1, 1, 2, 2, 0},	/* TVDPLL */
	{0, 1, 1, 2, 2, 0},	/* LVDSPLL */
	{0, 1, 1, 2, 2, 0},	/* ARMPLL2 */
	{0, 1, 1, 2, 2, 0},	/* MMPLL */
	{0, 1, 1, 2, 2, 0},	/* VDECPLL */
};

#else				/* PER_PROJECT_FH_SETTING */

PER_PROJECT_FH_SETTING
#endif				/* PER_PROJECT_FH_SETTING */
#define MEMPLL_CON1 (DDRPHY_BASE + 0x000624)	/* (0x???????[31:10]) (Please Edwin help offer address) */
#define MMPLL_CON1 0xF0209258
#define VDECPLL_CON1 0xF0209308
static const unsigned long pll_con1_addr[] = {
	ARMPLL_CON1,
	MAINPLL_CON1,
	MEMPLL_CON1,
	MSDCPLL_CON1,
	TVDPLL_CON1,
	LVDSPLL_CON1,
	ARMPLL2_CON1,
	MMPLL_CON1,
	VDECPLL_CON1,
};




#define PLL_STATUS_ENABLE 1
#define PLL_STATUS_DISABLE 0
static void update_fhctl_status(const int pll_id, const int enable)
{
	int i = 0;
	int enabled_num = 0;
	static unsigned int pll_status[] = {
		PLL_STATUS_DISABLE,	/* ARMPLL */
		PLL_STATUS_DISABLE,	/* MAINPLL */
		PLL_STATUS_DISABLE,	/* MEMPLL */
		PLL_STATUS_DISABLE,	/* MSDCPLL */
		PLL_STATUS_DISABLE,	/* TVDPLL */
		PLL_STATUS_DISABLE,	/* LVDSPLL */
		PLL_STATUS_DISABLE,	/* ARMPLL2 */
		PLL_STATUS_DISABLE,	/* MMPLL */
		PLL_STATUS_DISABLE,	/* VDECPLL */
	};

	/* FH_MSG("PLL#%d ori status is %d, you hope to change to %d\n", pll_id, pll_status[pll_id], enable) ; */
	FH_MSG("PL%d:%d->%d", pll_id, pll_status[pll_id], enable);
	if (pll_status[pll_id] == enable) {
		FH_MSG("no ch");	/* no change */
		return;
	}

	pll_status[pll_id] = enable;

	for (i = MT658X_FH_MINIMUMM_PLL; i <= MT658X_FH_MAXIMUMM_PLL; i++) {

		if (pll_status[i] == PLL_STATUS_ENABLE) {
			/* FH_MSG("PLL#%d is enabled", i) ; */
			enabled_num++;
		}
		/*else {
		   FH_MSG("PLL#%d is disabled", i) ;
		   } */
	}

	FH_MSG("PLen#=%d", enabled_num);
#if 0
	if ((g_clk_en == 0) && (enabled_num >= 1)) {
		enable_clock(MT_CG_PERI_FHCTL_PDN, "FREQHOP");
		g_clk_en = 1;
	} else if ((g_clk_en == 1) && (enabled_num == 0)) {
		disable_clock(MT_CG_PERI_FHCTL_PDN, "FREQHOP");
		g_clk_en = 0;
	}
#endif
}

static int __mt_freqhop_isenable_peri_fhctlx(const unsigned int pll_id)
{
	unsigned long val = 0;
	switch (pll_id) {
	case MT658X_FH_ARM_PLL:
		val = 1 << pll_id;
		break;
	case MT658X_FH_MAIN_PLL:
		val = 1 << 2;
		break;
	case MT658X_FH_MEM_PLL:
		val = 1 << 10;
		break;
	case MT658X_FH_MSDC_PLL:
		val = 1 << 5;
		break;
	case MT658X_FH_TVD_PLL:
		val = 1 << 6;
		break;
	case MT658X_FH_LVDS_PLL:
		val = 1 << 7;
		break;
	case MT658X_FH_ARM_PLL2:
		val = 1 << 1;
		break;
	case MT658X_FH_MM_PLL:
		val = 1 << 4;
		break;
	case MT658X_FH_VDEC_PLL:
		val = 1 << 9;
		break;
	}
	if (fh_read32(PLL_HP_CON0) & val)
		return 1;
	else
		return 0;

}

static void __mt_freqhop_enable_peri_fhctlx(const unsigned int pll_id)
{
	switch (pll_id) {
	case MT658X_FH_ARM_PLL:
		fh_set_field(PLL_HP_CON0, 1 << pll_id, 1);
		break;
	case MT658X_FH_MAIN_PLL:
		fh_set_field(PLL_HP_CON0, 1 << 2, 1);
		break;
	case MT658X_FH_MEM_PLL:
		fh_set_field(PLL_HP_CON0, 1 << 10, 1);
		break;
	case MT658X_FH_MSDC_PLL:
		fh_set_field(PLL_HP_CON0, 1 << 5, 1);
		break;
	case MT658X_FH_TVD_PLL:
		fh_set_field(PLL_HP_CON0, 1 << 6, 1);
		break;
	case MT658X_FH_LVDS_PLL:
		fh_set_field(PLL_HP_CON0, 1 << 7, 1);
		break;
	case MT658X_FH_ARM_PLL2:
		fh_set_field(PLL_HP_CON0, 1 << 1, 1);
		break;
	case MT658X_FH_MM_PLL:
		fh_set_field(PLL_HP_CON0, 1 << 4, 1);
		break;
	case MT658X_FH_VDEC_PLL:
		fh_set_field(PLL_HP_CON0, 1 << 9, 1);
		break;
	}
}

static void __mt_freqhop_disable_peri_fhctlx(const unsigned int pll_id)
{
	switch (pll_id) {
	case MT658X_FH_ARM_PLL:
		fh_set_field(PLL_HP_CON0, 1 << pll_id, 0);
		break;
	case MT658X_FH_MAIN_PLL:
		fh_set_field(PLL_HP_CON0, 1 << 2, 0);
		break;
	case MT658X_FH_MEM_PLL:
		fh_set_field(PLL_HP_CON0, 1 << 10, 0);
		break;
	case MT658X_FH_MSDC_PLL:
		fh_set_field(PLL_HP_CON0, 1 << 5, 0);
		break;
	case MT658X_FH_TVD_PLL:
		fh_set_field(PLL_HP_CON0, 1 << 6, 0);
		break;
	case MT658X_FH_LVDS_PLL:
		fh_set_field(PLL_HP_CON0, 1 << 7, 0);
		break;
	case MT658X_FH_ARM_PLL2:
		fh_set_field(PLL_HP_CON0, 1 << 1, 0);
		break;
	case MT658X_FH_MM_PLL:
		fh_set_field(PLL_HP_CON0, 1 << 4, 0);
		break;
	case MT658X_FH_VDEC_PLL:
		fh_set_field(PLL_HP_CON0, 1 << 9, 0);
		break;
	}
}


static int __mt_enable_freqhopping(unsigned int pll_id, const struct freqhopping_ssc *ssc_setting)
{
	unsigned long flags;
	unsigned int target_dds = 0, cur_dds, timeout = 300, dds_en = 0;

	FH_MSG("EN: %s", __func__);
	FH_MSG("lbnd: %x upbnd: %x df: %x dt: %x dds:%x", ssc_setting->lowbnd, ssc_setting->upbnd,
	       ssc_setting->df, ssc_setting->dt, ssc_setting->dds);

	update_fhctl_status(pll_id, PLL_STATUS_ENABLE);
	mb(); /* memory barrier */


	/* lock @ __freqhopping_ctrl() */
	/* spin_lock_irqsave(&freqhopping_lock, flags); */

	g_fh_pll[pll_id].fh_status = FH_FH_ENABLE_SSC;


	/* TODO: should we check the following here ?? */
	/* if(unlikely(FH_PLL_STATUS_ENABLE == g_fh_pll[pll_id].fh_status)){ */
	/* Do nothing due to this not allowable flow */
	/* We shall DISABLE and then re-ENABLE for the new setting or another round */
	/* FH_MSG("ENABLE the same FH",pll_id); */
	/* WARN_ON(1); */
	/* spin_unlock_irqrestore(&freqhopping_lock, flags); */
	/* return 1; */
	/* }else { */

	local_irq_save(flags);

	if (!__mt_freqhop_isenable_peri_fhctlx(pll_id)) {
		fh_write32(FHCTLx_CFG(pll_id), 1);
		if (pll_id != MT658X_FH_MEM_PLL)
			target_dds = fh_read32(pll_con1_addr[pll_id]) & 0x1FFFFF;
		else
			target_dds = fh_read32(pll_con1_addr[pll_id]) >> 1 >> 10;

		FH_MSG("org dds: %x", target_dds);
		fh_write32(FHCTLx_DVFS(pll_id), target_dds);
		udelay(100);
		__mt_freqhop_enable_peri_fhctlx(pll_id);
		udelay(100);
		fh_write32(FHCTLx_DVFS(pll_id), 0x80000000 | target_dds);
	}
	udelay(50);
	fh_get_field(REG_FHCTL0_CFG + (0x14 * pll_id), FH_FRDDSX_EN, dds_en);
	if (dds_en)
		fh_set_field(REG_FHCTL0_CFG + (0x14 * pll_id), FH_FRDDSX_EN, 0);

	udelay(250);
	/* Set the relative parameter registers (dt/df/upbnd/downbnd) */
	/* Enable the fh bit */
	fh_set_field(REG_FHCTL0_CFG + (0x14 * pll_id), FH_FRDDSX_DYS, ssc_setting->df);
	fh_set_field(REG_FHCTL0_CFG + (0x14 * pll_id), FH_FRDDSX_DTS, ssc_setting->dt);
	fh_set_field(REG_FHCTL0_UPDNLMT + (0x14 * pll_id), FH_FRDDSX_DNLMT,
		     (ssc_setting->dds * ssc_setting->lowbnd) / 3200);
	fh_set_field(REG_FHCTL0_UPDNLMT + (0x14 * pll_id), FH_FRDDSX_UPLMT,
		     (ssc_setting->dds * ssc_setting->upbnd) / 3200);
/* fh_write32(REG_FHCTL0_DDS+(0x10*pll_id), (ssc_setting->dds)|(1U<<31)); */
	fh_write32(FHCTLx_DVFS(pll_id), (ssc_setting->dds));
	udelay(300);
	fh_write32(FHCTLx_DVFS(pll_id), (ssc_setting->dds) | 0x80000000);
	udelay(300);
	target_dds = ssc_setting->dds;
/* flags = fh_read32(PLL_HP_CON0) | (1 << pll_id); */
/* fh_write32( PLL_HP_CON0,  flags ); */

	mb(); /* memory barrier */

	while (timeout > 0) {
		timeout--;
		cur_dds = fh_read32(FHCTLx_MON(pll_id)) & 0x1fffff;
		if (cur_dds == target_dds)
			break;
	}
	if (timeout == 0)
		FH_MSG(" %s timeout %x %x\n", __func__, target_dds, cur_dds);

	fh_set_field(REG_FHCTL0_CFG + (0x14 * pll_id), FH_FRDDSX_EN, 1);
	/* fh_set_field(REG_FHCTL0_CFG+(0x10*pll_id),FH_FHCTLX_EN,1); */
	/* lock @ __freqhopping_ctrl() */
	/* spin_unlock_irqrestore(&freqhopping_lock, flags); */
	local_irq_restore(flags);

	FH_MSG("Exit");
	return 0;
}

static int __mt_disable_freqhopping(unsigned int pll_id, const struct freqhopping_ssc *ssc_setting)
{
	unsigned long flags;
	FH_MSG("EN: _dis_fh");

	/* lock @ __freqhopping_ctrl_lock() */
	/* spin_lock_irqsave(&freqhopping_lock, flags); */
	local_irq_save(flags);

	/* Set PLL_HP_CON0 as APMIXEDSYS/DDRPHY control */
	__mt_freqhop_disable_peri_fhctlx(pll_id);

	/* Set the relative registers */
	fh_set_field(REG_FHCTL0_CFG + (0x14 * pll_id), FH_FRDDSX_EN, 0);
	fh_set_field(REG_FHCTL0_CFG + (0x14 * pll_id), FH_FHCTLX_EN, 0);
	mb(); /* memory barrier */
	g_fh_pll[pll_id].fh_status = FH_FH_DISABLE;
	local_irq_restore(flags);

	/* lock @ __freqhopping_ctrl_lock() */
	/* spin_unlock_irqrestore(&freqhopping_lock, flags); */

	mb(); /* memory barrier */
	update_fhctl_status(pll_id, PLL_STATUS_DISABLE);


	/* FH_MSG("Exit"); */

	return 0;
}


/* freq is in KHz, return at which number of entry in mt_ssc_xxx_setting[] */
static noinline int __freq_to_index(enum FH_PLL_ID pll_id, int freq)
{
	unsigned int retVal = 0;
	unsigned int i = 2;	/* 0 is disable, 1 is user defines, so start from 2 */

	/* FH_MSG("EN: %s , pll_id: %d, freq: %d",__func__,pll_id,freq); */
	FH_MSG("EN: , id: %d, f: %d", pll_id, freq);

	switch (pll_id) {

		/* TODO: use Marco or something to make the code less redudant */
	case MT658X_FH_ARM_PLL:
		while (i < MT_SSC_NR_PREDEFINE_SETTING) {
			if (freq == mt_ssc_armpll_setting[i].freq) {
				retVal = i;
				break;
			}
			i++;
		}
		break;


	case MT658X_FH_MAIN_PLL:
		while (i < MT_SSC_NR_PREDEFINE_SETTING) {
			if (freq == mt_ssc_mainpll_setting[i].freq) {
				retVal = i;
				break;
			}
			i++;
		}
		break;

	case MT658X_FH_MEM_PLL:
		while (i < MT_SSC_NR_PREDEFINE_SETTING) {
			if (freq == mt_ssc_mempll_setting[i].freq) {
				retVal = i;
				break;
			}
			i++;
		}
		break;

	case MT658X_FH_MSDC_PLL:
		while (i < MT_SSC_NR_PREDEFINE_SETTING) {
			if (freq == mt_ssc_msdcpll_setting[i].freq) {
				retVal = i;
				break;
			}
			i++;
		}
		break;

	case MT658X_FH_TVD_PLL:
		FH_MSG("MT658X_FH_TVD_PLL");
		while (i < MT_SSC_NR_PREDEFINE_SETTING) {
			if (freq == mt_ssc_tvdpll_setting[i].freq) {
				retVal = i;
				break;
			}
			i++;
		}
		break;

	case MT658X_FH_LVDS_PLL:
		while (i < MT_SSC_NR_PREDEFINE_SETTING) {
			if (freq == mt_ssc_lvdspll_setting[i].freq) {
				retVal = i;
				break;
			}
			i++;
		}
		break;

	case MT658X_FH_ARM_PLL2:
		while (i < MT_SSC_NR_PREDEFINE_SETTING) {
			if (freq == mt_ssc_armpll2_setting[i].freq) {
				retVal = i;
				break;
			}
			i++;
		}
		break;

	case MT658X_FH_MM_PLL:
		while (i < MT_SSC_NR_PREDEFINE_SETTING) {
			if (freq == mt_ssc_mmpll_setting[i].freq) {
				retVal = i;
				break;
			}
			i++;
		}
		break;

	case MT658X_FH_VDEC_PLL:
		while (i < MT_SSC_NR_PREDEFINE_SETTING) {
			if (freq == mt_ssc_vdecpll_setting[i].freq) {
				retVal = i;
				break;
			}
			i++;
		}
		break;

	case MT658X_FH_PLL_TOTAL_NUM:
		FH_MSG("Error MT658X_FH_PLL_TOTAL_NUM!");
		break;


	};

	return retVal;
}

static const struct freqhopping_ssc *id_to_ssc(unsigned int pll_id, unsigned int ssc_setting_id)
{
	const struct freqhopping_ssc *pSSC_setting = NULL;
	switch (pll_id) {
	case MT658X_FH_MAIN_PLL:
		FH_BUG_ON(ssc_setting_id >
			  (sizeof(mt_ssc_mainpll_setting) / sizeof(struct freqhopping_ssc)));
		pSSC_setting = &mt_ssc_mainpll_setting[ssc_setting_id];
		break;
	case MT658X_FH_ARM_PLL:
		FH_BUG_ON(ssc_setting_id >
			  (sizeof(mt_ssc_armpll_setting) / sizeof(struct freqhopping_ssc)));
		pSSC_setting = &mt_ssc_armpll_setting[ssc_setting_id];
		break;
	case MT658X_FH_MSDC_PLL:
		FH_BUG_ON(ssc_setting_id >
			  (sizeof(mt_ssc_msdcpll_setting) / sizeof(struct freqhopping_ssc)));
		pSSC_setting = &mt_ssc_msdcpll_setting[ssc_setting_id];
		break;
	case MT658X_FH_TVD_PLL:
		FH_BUG_ON(ssc_setting_id >
			  (sizeof(mt_ssc_tvdpll_setting) / sizeof(struct freqhopping_ssc)));
		pSSC_setting = &mt_ssc_tvdpll_setting[ssc_setting_id];
		break;
	case MT658X_FH_LVDS_PLL:
		FH_BUG_ON(ssc_setting_id >
			  (sizeof(mt_ssc_lvdspll_setting) / sizeof(struct freqhopping_ssc)));
		pSSC_setting = &mt_ssc_lvdspll_setting[ssc_setting_id];
		break;
	case MT658X_FH_MEM_PLL:
		FH_BUG_ON(ssc_setting_id >
			  (sizeof(mt_ssc_mempll_setting) / sizeof(struct freqhopping_ssc)));
		pSSC_setting = &mt_ssc_mempll_setting[ssc_setting_id];
		break;
	case MT658X_FH_ARM_PLL2:
		FH_BUG_ON(ssc_setting_id >
			  (sizeof(mt_ssc_armpll2_setting) / sizeof(struct freqhopping_ssc)));
		pSSC_setting = &mt_ssc_armpll2_setting[ssc_setting_id];
		break;
	case MT658X_FH_MM_PLL:
		FH_BUG_ON(ssc_setting_id >
			  (sizeof(mt_ssc_lvdspll_setting) / sizeof(struct freqhopping_ssc)));
		pSSC_setting = &mt_ssc_mmpll_setting[ssc_setting_id];
		break;
	case MT658X_FH_VDEC_PLL:
		FH_BUG_ON(ssc_setting_id >
			  (sizeof(mt_ssc_lvdspll_setting) / sizeof(struct freqhopping_ssc)));
		pSSC_setting = &mt_ssc_vdecpll_setting[ssc_setting_id];
		break;
	}
	return pSSC_setting;
}

static int __freqhopping_ctrl(struct freqhopping_ioctl *fh_ctl, bool enable)
{
	const struct freqhopping_ssc *pSSC_setting = NULL;
/* unsigned long                   flags=0; */
	unsigned int ssc_setting_id = 0;
	int retVal = 1;

	FH_MSG("EN: _fh_ctrl %d:%d", fh_ctl->pll_id, enable);
	/* FH_MSG("%s fhpll_id: %d, enable: %d",(enable)?"enable":"disable",fh_ctl->pll_id,enable); */

	/* Check the out of range of frequency hopping PLL ID */
	FH_BUG_ON(fh_ctl->pll_id > MT658X_FH_MAXIMUMM_PLL);
	FH_BUG_ON(fh_ctl->pll_id < MT658X_FH_MINIMUMM_PLL);


	/* spin_lock_irqsave(&freqhopping_lock, flags); */

	if ((enable == true) && (g_fh_pll[fh_ctl->pll_id].fh_status == FH_FH_ENABLE_SSC)) {

		/* The FH already enabled @ this PLL */

		FH_MSG("re-en FH");

		/* disable FH first, will be enable later */
		/* __mt_disable_freqhopping(fh_ctl->pll_id,pSSC_setting); */
	} else if ((enable == false) && (g_fh_pll[fh_ctl->pll_id].fh_status == FH_FH_DISABLE)) {

		/* The FH already been disabled @ this PLL, do nothing & return */

		FH_MSG("re-dis FH");
		retVal = 0;
		goto Exit;
	}

	/* ccyeh fh_status set @ __mt_enable_freqhopping() __mt_disable_freqhopping() */
	/* g_fh_pll[fh_ctl->pll_id].fh_status = enable?FH_FH_ENABLE_SSC:FH_FH_DISABLE; */
	g_fh_pll[fh_ctl->pll_id].pll_status = enable;
	if (enable == true) {	/* enable freq. hopping @ fh_ctl->pll_id */

		if (g_fh_pll[fh_ctl->pll_id].pll_status == FH_PLL_DISABLE) {
			FH_MSG("pll is dis");

			/* update the fh_status & don't really enable the SSC */
			g_fh_pll[fh_ctl->pll_id].fh_status = FH_FH_ENABLE_SSC;
			retVal = 0;
			goto Exit;
		} else {
			FH_MSG("pll is en");
			if (g_fh_pll[fh_ctl->pll_id].user_defined == true) {
				FH_MSG("use u-def");

				pSSC_setting = &mt_ssc_fhpll_userdefined[fh_ctl->pll_id];
				g_fh_pll[fh_ctl->pll_id].setting_id = USER_DEFINE_SETTING_ID;
			} else {
				FH_MSG("n-user def");

				if (g_fh_pll[fh_ctl->pll_id].curr_freq != 0) {
					ssc_setting_id = g_fh_pll[fh_ctl->pll_id].setting_id =
					    __freq_to_index(fh_ctl->pll_id,
							    g_fh_pll[fh_ctl->pll_id].curr_freq);
				} else {
					ssc_setting_id = 0;
				}


				FH_MSG("sid %d", ssc_setting_id);
				if (ssc_setting_id == 0) {
					FH_MSG("No corresponding setting found !!!");
					FH_MSG("use debug mode");
					/* just disable FH & exit */
					if (0 ==
					    __mt_enable_freqhopping(fh_ctl->pll_id,
								    &fh_ctl->ssc_setting)) {
						retVal = 0;
						FH_MSG("en ok");
					} else {
						FH_MSG("__mt_enable_freqhopping fail.");
					}
					/* __mt_disable_freqhopping(fh_ctl->pll_id,pSSC_setting); */
					goto Exit;
				}
				pSSC_setting = id_to_ssc(fh_ctl->pll_id, ssc_setting_id);
			}	/* user defined */

			if (pSSC_setting == NULL) {
				FH_MSG("!!! pSSC_setting is NULL !!!");
				/* just disable FH & exit */
				__mt_disable_freqhopping(fh_ctl->pll_id, pSSC_setting);
				goto Exit;
			}

			if (0 == __mt_enable_freqhopping(fh_ctl->pll_id, pSSC_setting)) {
				retVal = 0;
				FH_MSG("en ok");
			} else {
				FH_MSG("__mt_enable_freqhopping fail.");
			}
		}

	} else {		/* disable req. hopping @ fh_ctl->pll_id */
		if (0 == __mt_disable_freqhopping(fh_ctl->pll_id, pSSC_setting)) {
			retVal = 0;
			FH_MSG("dis ok");
		} else {
			FH_MSG("__mt_disable_freqhopping fail.");
		}
	}

 Exit:
	/* release spinlock here */
	/* spin_unlock_irqrestore(&freqhopping_lock, flags); */

	/* FH_MSG("Exit"); */
	return retVal;
}

#if 0
/* mempll 266->293MHz using FHCTL */
static int mt_h2oc_mempll(void)
{

	return 0;
}

/* mempll 293->266MHz using FHCTL */
static int mt_oc2h_mempll(void)
{
	return 0;
}
#endif
/* mempll 200->266MHz using FHCTL */
static int mt_fh_hal_l2h_mempll(void)	/* mempll low to high (200->266MHz) */
{
	return 0;
}

/* mempll 266->200MHz using FHCTL */
static int mt_fh_hal_h2l_mempll(void)	/* mempll low to high (200->266MHz) */
{

	return 0;
}

static int mt_fh_hal_dram_overclock(int clk)
{
	return -1;
}

static int mt_fh_hal_get_dramc(void)
{
	return g_curr_dramc;
}

static void mt_fh_hal_popod_save(void)
{
	/* FH_MSG("EN: %s", __func__); */
}

static void mt_fh_hal_popod_restore(void)
{
	/* FH_MSG("EN: %s", __func__); */
}

/* static int freqhopping_dramc_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data) */
static int freqhopping_dramc_proc_read(struct seq_file *m, void *v)
{

	FH_MSG("EN: %s", __func__);

	seq_printf(m, "DRAMC: %dMHz\r\n", g_curr_dramc);
	seq_printf(m, "mt_get_emi_freq(): %dHz\r\n", mt_get_emi_freq());
	seq_printf(m, "get_ddr_type(): %d\r\n", get_ddr_type());
	seq_printf(m, "rank: 0x%x\r\n", (DRV_Reg32(EMI_CONA) & 0x20000));
	seq_printf(m, "infra: 0x%x\r\n", (slp_will_infra_pdn()));
	seq_printf(m, "g_fh_rank0_pa: %p\r\n", g_fh_rank0_pa);
	seq_printf(m, "g_fh_rank0_va: %p\r\n", g_fh_rank0_va);
	seq_printf(m, "g_fh_rank1_pa: %p\r\n", g_fh_rank1_pa);
	seq_printf(m, "g_fh_rank1_va: %p\r\n", g_fh_rank1_va);
	return 0;

#if 0
	char *p = page;
	int len = 0;

	FH_MSG("EN: %s", __func__);

	p += sprintf(p, "DRAMC: %dMHz\r\n", g_curr_dramc);
	p += sprintf(p, "mt_get_emi_freq(): %dHz\r\n", mt_get_emi_freq());
	p += sprintf(p, "get_ddr_type(): %d\r\n", get_ddr_type());
	p += sprintf(p, "rank: 0x%x\r\n", (DRV_Reg32(EMI_CONA) & 0x20000));
	p += sprintf(p, "infra: 0x%x\r\n", (slp_will_infra_pdn()));
	p += sprintf(p, "g_fh_rank0_pa: %p\r\n", g_fh_rank0_pa);
	p += sprintf(p, "g_fh_rank0_va: %p\r\n", g_fh_rank0_va);
	p += sprintf(p, "g_fh_rank1_pa: %p\r\n", g_fh_rank1_pa);
	p += sprintf(p, "g_fh_rank1_va: %p\r\n", g_fh_rank1_va);

	*start = page + off;

	len = p - page;

	if (len > off)
		len -= off;
	else
		len = 0;

	return len < count ? len : count;
#endif
}


static int freqhopping_dramc_proc_write(struct file *file, const char *buffer, unsigned long count,
					void *data)
{
	int len = 0, freq = 0;
	char dramc[32];

	FH_MSG("EN: proc");

	len = (count < (sizeof(dramc) - 1)) ? count : (sizeof(dramc) - 1);

	if (copy_from_user(dramc, buffer, len)) {
		FH_MSG("copy_from_user fail!");
		return 1;
	}

	dramc[len] = '\0';

	if (sscanf(dramc, "%d", &freq) == 1) {
		if ((freq == 266) || (freq == 200)) {
			FH_MSG("dramc:%d ", freq);
			(freq == 266) ? mt_fh_hal_l2h_mempll() : mt_fh_hal_h2l_mempll();
		} else if (freq == 293) {
			mt_fh_hal_dram_overclock(293);
		} else {
			FH_MSG("must be 200/266/293!");
		}

#if 0
		if (freq == 266)
			FH_MSG("==> %d", mt_fh_hal_dram_overclock(266));
		else if (freq == 293)
			FH_MSG("==> %d", mt_fh_hal_dram_overclock(293));
		else if (freq == LOW_DRAMC)
			FH_MSG("==> %d", mt_fh_hal_dram_overclock(208));

#endif

		return count;
	} else {
		FH_MSG("  bad argument!!");
	}

	return -EINVAL;
}

/* static int freqhopping_dvfs_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data) */
static int freqhopping_dvfs_proc_read(struct seq_file *m, void *v)
{
	FH_MSG("EN: %s", __func__);

	seq_puts(m, "DVFS:\r\n");
	seq_puts(m, "CFG: 0x3 is SSC mode;  0x5 is DVFS mode \r\n");
	/*for (i = 0; i < MT_FHPLL_MAX; i++) {
		seq_printf(m, "FHCTL%d:   CFG:0x%08x    DVFS:0x%08x\r\n"
		,i, DRV_Reg32(REG_FHCTL0_CFG+(i*0x14)), DRV_Reg32(REG_FHCTL0_DVFS+(i*0x14)));
	}*/

	return 0;

#if 0
	char *p = page;
	int len = 0;
	int i = 0;

	FH_MSG("EN: %s", __func__);

	p += sprintf(p, "DVFS:\r\n");
	p += sprintf(p, "CFG: 0x3 is SSC mode;  0x5 is DVFS mode \r\n");
	for (i = 0; i < MT_FHPLL_MAX; i++) {
		/* p += sprintf(p, "FHCTL%d:   CFG:0x%08x    DVFS:0x%08x\r\n"
		,i, DRV_Reg32(REG_FHCTL0_CFG+(i*0x14)), DRV_Reg32(REG_FHCTL0_DVFS+(i*0x14))); */
	}

	*start = page + off;

	len = p - page;

	if (len > off)
		len -= off;
	else
		len = 0;

	return len < count ? len : count;
#endif
}

static void mt_fh_hal_lock(unsigned long *flags);
static void mt_fh_hal_unlock(unsigned long *flags);
static int freqhopping_dvfs_proc_write(struct file *file, const char *buffer, unsigned long count,
				       void *data)
{
	int ret;
	char kbuf[256];
	unsigned long len = 0;
	unsigned int p1, p2, p3, p4, p5, p6, p7, p8;
	struct freqhopping_ioctl fh_ctl;

	unsigned long flags;
	p1 = 0;
	p2 = 0;
	p3 = 0;
	p4 = 0;
	p5 = 0;
	p6 = 0;
	p7 = 0;
	p8 = 0;

	FH_MSG("EN: %s", __func__);
	len = min(count, (unsigned long)(sizeof(kbuf) - 1));

	if (count == 0)
		return -1;
	if (count > 255)
		count = 255;

	ret = copy_from_user(kbuf, buffer, count);
	if (ret < 0)
		return -1;

	kbuf[count] = '\0';

	ret = sscanf(kbuf, "%x %x %x %x %x %x %x %x", &p1, &p2, &p3, &p4, &p5, &p6, &p7, &p8);

	fh_ctl.pll_id = p2;
	fh_ctl.ssc_setting.df = p3;
	fh_ctl.ssc_setting.dt = p4;
	fh_ctl.ssc_setting.upbnd = p5;
	fh_ctl.ssc_setting.lowbnd = p6;
	fh_ctl.ssc_setting.dds = p7;
	fh_ctl.ssc_setting.freq = p8;
	FH_BUG_ON(p2 > MT658X_FH_MAXIMUMM_PLL);
	memcpy(&g_fh_pll_sus, &g_fh_pll, sizeof(g_fh_pll));
	mt_fh_hal_lock(&flags);
	if (!p8) {
		g_fh_pll[p2].user_defined = true;
		memcpy(&mt_ssc_fhpll_userdefined[p2], &fh_ctl.ssc_setting,
		       sizeof(mt_ssc_fhpll_userdefined));
	}
	mt_fh_hal_unlock(&flags);
	freqhopping_config(p2, p8, p1);
	mt_fh_hal_lock(&flags);
	memcpy(&g_fh_pll, &g_fh_pll_sus, sizeof(g_fh_pll));
	mt_fh_hal_unlock(&flags);
	return count;
}



/* static int freqhopping_dumpregs_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data) */
static int freqhopping_dumpregs_proc_read(struct seq_file *m, void *v)
{
	int i = 0;

	FH_MSG("EN: %s", __func__);

	seq_puts(m, "FHDMA_CFG:\r\n");

	seq_printf(m, "0x%08x 0x%08x 0x%08x 0x%08x\r\n",
		   DRV_Reg32(REG_FHDMA_CFG + (i * 0x10)),
		   DRV_Reg32(REG_FHDMA_2G1BASE + (i * 0x10)),
		   DRV_Reg32(REG_FHDMA_2G2BASE + (i * 0x10)),
		   DRV_Reg32(REG_FHDMA_INTMDBASE + (i * 0x10)));

	seq_printf(m, "0x%08x 0x%08x 0x%08x 0x%08x\r\n",
		   DRV_Reg32(REG_FHDMA_EXTMDBASE + (i * 0x10)),
		   DRV_Reg32(REG_FHDMA_BTBASE + (i * 0x10)),
		   DRV_Reg32(REG_FHDMA_WFBASE + (i * 0x10)),
		   DRV_Reg32(REG_FHDMA_FMBASE + (i * 0x10)));

	seq_printf(m, "0x%08x 0x%08x 0x%08x 0x%08x\r\n",
		   DRV_Reg32(REG_FHSRAM_CON + (i * 0x10)),
		   DRV_Reg32(REG_FHSRAM_WR + (i * 0x10)),
		   DRV_Reg32(REG_FHSRAM_RD + (i * 0x10)), DRV_Reg32(REG_FHCTL_CFG + (i * 0x10)));

	seq_printf(m, "0x%08x 0x%08x 0x%08x 0x%08x\r\n",
		   DRV_Reg32(REG_FHCTL_2G1_CH + (i * 0x10)),
		   DRV_Reg32(REG_FHCTL_2G2_CH + (i * 0x10)),
		   DRV_Reg32(REG_FHCTL_INTMD_CH + (i * 0x10)),
		   DRV_Reg32(REG_FHCTL_EXTMD_CH + (i * 0x10)));

	seq_printf(m, "0x%08x 0x%08x 0x%08x \r\n\r\n",
		   DRV_Reg32(REG_FHCTL_BT_CH + (i * 0x10)),
		   DRV_Reg32(REG_FHCTL_WF_CH + (i * 0x10)),
		   DRV_Reg32(REG_FHCTL_FM_CH + (i * 0x10)));


	seq_puts(m, "FHCTL0_CFG:\r\n");

	for (i = 0; i < MT_FHPLL_MAX; i++) {
		seq_printf(m, "0x%08x 0x%08x 0x%08x 0x%08x\r\n",
			   DRV_Reg32(REG_FHCTL0_CFG + (i * 0x14)),
			   DRV_Reg32(REG_FHCTL0_UPDNLMT + (i * 0x14)),
			   DRV_Reg32(REG_FHCTL0_DDS + (i * 0x14)),
			   DRV_Reg32(REG_FHCTL0_MON + (i * 0x14)));
	}


	seq_printf(m, "\r\nPLL_HP_CON0:\r\n0x%08x\r\n", DRV_Reg32(PLL_HP_CON0));


	seq_printf(m,
		   "\r\nPLL_CON0 :\r\nARM:0x%08x MAIN:0x%08x MSDC:0x%08x TV:0x%08x LVDS:0x%08x UNIV:0x%08x\r\n",
		   DRV_Reg32(ARMPLL_CON0), DRV_Reg32(MAINPLL_CON0), DRV_Reg32(MSDCPLL_CON0),
		   DRV_Reg32(TVDPLL_CON0), DRV_Reg32(LVDSPLL_CON0), DRV_Reg32(UNIVPLL_CON0));

	seq_printf(m,
		   "\r\nPLL_CON1 :\r\nARM:0x%08x MAIN:0x%08x MSDC:0x%08x TV:0x%08x LVDS:0x%08x UNIV:0x%08x\r\n",
		   DRV_Reg32(ARMPLL_CON1), DRV_Reg32(MAINPLL_CON1), DRV_Reg32(MSDCPLL_CON1),
		   DRV_Reg32(TVDPLL_CON1), DRV_Reg32(LVDSPLL_CON1), DRV_Reg32(UNIVPLL_CON0 + 0x4));

	seq_printf(m,
		   "\r\nPLL_CON2 :\r\nARM:0x%08x MAIN:0x%08x MSDC:0x%08x TV:0x%08x LVDS:0x%08x UNIV:0x%08x\r\n",
		   DRV_Reg32(ARMPLL_CON2), DRV_Reg32(MAINPLL_CON2), DRV_Reg32(MSDCPLL_CON2),
		   DRV_Reg32(TVDPLL_CON2), DRV_Reg32(LVDSPLL_CON2), DRV_Reg32(UNIVPLL_CON0 + 0x8));


	seq_printf(m, "\r\nMEMPLL :\r\nMEMPLL9: 0x%08x MEMPLL10: 0x%08x MEMPLL11: 0x%08x MEMPLL12: 0x%08x\r\n"
		, DRV_Reg32(DDRPHY_BASE + 0x624), DRV_Reg32(DDRPHY_BASE + 0x628)
		, DRV_Reg32(DDRPHY_BASE + 0x62C), DRV_Reg32(DDRPHY_BASE + 0x630));	/* TODO: Hard code for now... */

	seq_printf(m, "\r\nCLK26CALI: 0x%08x\r\n", DRV_Reg32(CLK26CALI));

	return 0;

#if 0
	char *p = page;
	int len = 0;
	int i = 0;

	FH_MSG("EN: %s", __func__);

	p += sprintf(p, "FHDMA_CFG:\r\n");

	p += sprintf(p, "0x%08x 0x%08x 0x%08x 0x%08x\r\n",
		     DRV_Reg32(REG_FHDMA_CFG + (i * 0x10)),
		     DRV_Reg32(REG_FHDMA_2G1BASE + (i * 0x10)),
		     DRV_Reg32(REG_FHDMA_2G2BASE + (i * 0x10)),
		     DRV_Reg32(REG_FHDMA_INTMDBASE + (i * 0x10)));

	p += sprintf(p, "0x%08x 0x%08x 0x%08x 0x%08x\r\n",
		     DRV_Reg32(REG_FHDMA_EXTMDBASE + (i * 0x10)),
		     DRV_Reg32(REG_FHDMA_BTBASE + (i * 0x10)),
		     DRV_Reg32(REG_FHDMA_WFBASE + (i * 0x10)),
		     DRV_Reg32(REG_FHDMA_FMBASE + (i * 0x10)));

	p += sprintf(p, "0x%08x 0x%08x 0x%08x 0x%08x\r\n",
		     DRV_Reg32(REG_FHSRAM_CON + (i * 0x10)),
		     DRV_Reg32(REG_FHSRAM_WR + (i * 0x10)),
		     DRV_Reg32(REG_FHSRAM_RD + (i * 0x10)), DRV_Reg32(REG_FHCTL_CFG + (i * 0x10)));

	p += sprintf(p, "0x%08x 0x%08x 0x%08x 0x%08x\r\n",
		     DRV_Reg32(REG_FHCTL_2G1_CH + (i * 0x10)),
		     DRV_Reg32(REG_FHCTL_2G2_CH + (i * 0x10)),
		     DRV_Reg32(REG_FHCTL_INTMD_CH + (i * 0x10)),
		     DRV_Reg32(REG_FHCTL_EXTMD_CH + (i * 0x10)));

	p += sprintf(p, "0x%08x 0x%08x 0x%08x \r\n\r\n",
		     DRV_Reg32(REG_FHCTL_BT_CH + (i * 0x10)),
		     DRV_Reg32(REG_FHCTL_WF_CH + (i * 0x10)),
		     DRV_Reg32(REG_FHCTL_FM_CH + (i * 0x10)));


	p += sprintf(p, "FHCTL0_CFG:\r\n");

	for (i = 0; i < MT_FHPLL_MAX; i++) {
		p += sprintf(p, "0x%08x 0x%08x 0x%08x 0x%08x\r\n",
			     DRV_Reg32(REG_FHCTL0_CFG + (i * 0x14)),
			     DRV_Reg32(REG_FHCTL0_UPDNLMT + (i * 0x14)),
			     DRV_Reg32(REG_FHCTL0_DDS + (i * 0x14)),
			     DRV_Reg32(REG_FHCTL0_MON + (i * 0x14)));
	}


	p += sprintf(p, "\r\nPLL_HP_CON0:\r\n0x%08x\r\n", DRV_Reg32(PLL_HP_CON0));


	p += sprintf(p,
		     "\r\nPLL_CON0 :\r\nARM:0x%08x MAIN:0x%08x MSDC:0x%08x TV:0x%08x LVDS:0x%08x UNIV:0x%08x\r\n",
		     DRV_Reg32(ARMPLL_CON0), DRV_Reg32(MAINPLL_CON0), DRV_Reg32(MSDCPLL_CON0),
		     DRV_Reg32(TVDPLL_CON0), DRV_Reg32(LVDSPLL_CON0), DRV_Reg32(UNIVPLL_CON0));

	p += sprintf(p,
		     "\r\nPLL_CON1 :\r\nARM:0x%08x MAIN:0x%08x MSDC:0x%08x TV:0x%08x LVDS:0x%08x UNIV:0x%08x\r\n",
		     DRV_Reg32(ARMPLL_CON1), DRV_Reg32(MAINPLL_CON1), DRV_Reg32(MSDCPLL_CON1),
		     DRV_Reg32(TVDPLL_CON1), DRV_Reg32(LVDSPLL_CON1),
		     DRV_Reg32(UNIVPLL_CON0 + 0x4));

	p += sprintf(p,
		     "\r\nPLL_CON2 :\r\nARM:0x%08x MAIN:0x%08x MSDC:0x%08x TV:0x%08x LVDS:0x%08x UNIV:0x%08x\r\n",
		     DRV_Reg32(ARMPLL_CON2), DRV_Reg32(MAINPLL_CON2), DRV_Reg32(MSDCPLL_CON2),
		     DRV_Reg32(TVDPLL_CON2), DRV_Reg32(LVDSPLL_CON2),
		     DRV_Reg32(UNIVPLL_CON0 + 0x8));


	p += sprintf(p, "\r\nMEMPLL :\r\nMEMPLL9: 0x%08x MEMPLL10: 0x%08x MEMPLL11: 0x%08x MEMPLL12: 0x%08x\r\n"
		, DRV_Reg32(DDRPHY_BASE + 0x624), DRV_Reg32(DDRPHY_BASE + 0x628)
		, DRV_Reg32(DDRPHY_BASE + 0x62C), DRV_Reg32(DDRPHY_BASE + 0x630));
	/* TODO: Hard code for now... */

	p += sprintf(p, "\r\nCLK26CALI: 0x%08x\r\n", DRV_Reg32(CLK26CALI));

	*start = page + off;

	len = p - page;

	if (len > off)
		len -= off;
	else
		len = 0;

	return len < count ? len : count;
#endif

}


#if MT_FH_CLK_GEN

static int freqhopping_clkgen_proc_read(char *page, char **start, off_t off, int count, int *eof,
					void *data)
{
	char *p = page;
	int len = 0;

	FH_MSG("EN: %s", __func__);

	if (g_curr_clkgen > MT658X_FH_PLL_TOTAL_NUM)
		p += sprintf(p, "no clkgen output.\r\n");
	else
		p += sprintf(p, "clkgen:%d\r\n", g_curr_clkgen);


	p += sprintf(p,
		     "\r\nMBIST :\r\nMBIST_CFG_2: 0x%08x MBIST_CFG_6: 0x%08x MBIST_CFG_7: 0x%08x\r\n",
		     DRV_Reg32(MBIST_CFG_2), DRV_Reg32(MBIST_CFG_6), DRV_Reg32(MBIST_CFG_7));

	p += sprintf(p, "\r\nCLK_CFG_3: 0x%08x\r\n", DRV_Reg32(CLK_CFG_3));

	p += sprintf(p, "\r\nTOP_CKMUXSEL: 0x%08x\r\n", DRV_Reg32(TOP_CKMUXSEL));

	p += sprintf(p, "\r\nGPIO: 0x%08x 0x%08x 0x%08x 0x%08x\r\n",
		     DRV_Reg32(GPIO_BASE + 0xC60),
		     DRV_Reg32(GPIO_BASE + 0xC70),
		     DRV_Reg32(GPIO_BASE + 0xCD0), DRV_Reg32(GPIO_BASE + 0xD90));


	p += sprintf(p, "\r\nDDRPHY_BASE :\r\n0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\r\n",
		     DRV_Reg32(DDRPHY_BASE + 0x600),
		     DRV_Reg32(DDRPHY_BASE + 0x604),
		     DRV_Reg32(DDRPHY_BASE + 0x608),
		     DRV_Reg32(DDRPHY_BASE + 0x60C),
		     DRV_Reg32(DDRPHY_BASE + 0x614), DRV_Reg32(DDRPHY_BASE + 0x61C));

	*start = page + off;

	len = p - page;

	if (len > off)
		len -= off;
	else
		len = 0;

	return len < count ? len : count;
}


static int freqhopping_clkgen_proc_write(struct file *file, const char *buffer, unsigned long count,
					 void *data)
{
	int len = 0, pll_id = 0;
	char clkgen[32];

	FH_MSG("EN: %s", __func__);

	len = (count < (sizeof(clkgen) - 1)) ? count : (sizeof(clkgen) - 1);

	if (copy_from_user(clkgen, buffer, len)) {
		FH_MSG("copy_from_user fail!");
		return 1;
	}

	clkgen[len] = '\0';

	if (sscanf(clkgen, "%d", &pll_id) == 1) {
		if (pll_id == MT658X_FH_ARM_PLL) {
			fh_write32(MBIST_CFG_2, 0x00000009);	/* divide by 9+1 */
			fh_write32(CLK_CFG_3, 0x00000001);	/* enable it */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x00000001);	/* pll_pre_clk [don't care @ ARMPLL] */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x00000001);	/* pll_clk_sel [don't care @ ARMPLL] */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x00000401);	/* abist_clk_sel [0100: armpll_occ_mon] */
			udelay(1000);

			fh_write32(CLK26CALI, 0x00000001);
		} else if (pll_id == MT658X_FH_MAIN_PLL) {
			fh_write32(MBIST_CFG_2, 0x00000009);	/* divide by 9+1 */
			fh_write32(CLK_CFG_3, 0x00000001);	/* enable it */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x000F0001);	/* pll_pre_clk [1111: AD_MAIN_H230P3M] */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x000F0001);	/* pll_clk_sel [0000: pll_pre_clk] */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x000F0F01);	/* abist_clk_sel [1111: pll_clk_out] */
			udelay(1000);

			fh_write32(CLK26CALI, 0x00000001);
		} else if (pll_id == MT658X_FH_MEM_PLL) {

			fh_write32(DDRPHY_BASE + 0x600,
				   ((DRV_Reg32(DDRPHY_BASE + 0x600)) | 0x1 << 5));


			fh_write32(DDRPHY_BASE + 0x60C,
				   ((DRV_Reg32(DDRPHY_BASE + 0x60C)) | 0x1 << 21));
			fh_write32(DDRPHY_BASE + 0x614,
				   ((DRV_Reg32(DDRPHY_BASE + 0x614)) | 0x1 << 21));
			fh_write32(DDRPHY_BASE + 0x61C,
				   ((DRV_Reg32(DDRPHY_BASE + 0x61C)) | 0x1 << 21));

			fh_write32(DDRPHY_BASE + 0x60C, ((DRV_Reg32(DDRPHY_BASE + 0x60C)) & ~0x7));
			fh_write32(DDRPHY_BASE + 0x60C, ((DRV_Reg32(DDRPHY_BASE + 0x60C)) | 0x2));
			fh_write32(DDRPHY_BASE + 0x614, ((DRV_Reg32(DDRPHY_BASE + 0x614)) & ~0x7));
			fh_write32(DDRPHY_BASE + 0x614, ((DRV_Reg32(DDRPHY_BASE + 0x614)) | 0x2));
			fh_write32(DDRPHY_BASE + 0x61C, ((DRV_Reg32(DDRPHY_BASE + 0x61C)) & ~0x7));
			fh_write32(DDRPHY_BASE + 0x61C, ((DRV_Reg32(DDRPHY_BASE + 0x61C)) | 0x2));

			fh_write32(DDRPHY_BASE + 0x604,
				   ((DRV_Reg32(DDRPHY_BASE + 0x604)) | 0x1 << 3));
			fh_write32(DDRPHY_BASE + 0x604,
				   ((DRV_Reg32(DDRPHY_BASE + 0x604)) | 0x1 << 7));
			fh_write32(DDRPHY_BASE + 0x604,
				   ((DRV_Reg32(DDRPHY_BASE + 0x604)) | 0x1 << 4));
			fh_write32(DDRPHY_BASE + 0x604,
				   ((DRV_Reg32(DDRPHY_BASE + 0x604)) | 0x1 << 9));

#if 0
			fh_write32(DDRPHY_BASE + 0x608,
				   ((DRV_Reg32(DDRPHY_BASE + 0x608)) & ~0x000E0000));
#endif
			fh_write32(DDRPHY_BASE + 0x608,
				   ((DRV_Reg32(DDRPHY_BASE + 0x608)) | 0x00040000));

			fh_write32(DDRPHY_BASE + 0x608,
				   ((DRV_Reg32(DDRPHY_BASE + 0x608)) & ~0xF0000000));
			fh_write32(DDRPHY_BASE + 0x608,
				   ((DRV_Reg32(DDRPHY_BASE + 0x608)) | 0x80000000));

			/* fh_write32(MBIST_CFG_2, 0x00000001); //divide by 1+1 */
			fh_write32(CLK_CFG_3, 0x00000001);	/* enable it */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x00000001);	/* pll_pre_clk [don't care @ ARMPLL] */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x00000001);	/* pll_clk_sel [don't care @ ARMPLL] */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x00000501);	/* abist_clk_sel [0101: AD_MEMPLL_MONCLK] */
			udelay(1000);

			fh_write32(CLK26CALI, 0x00000000);
		} else if (pll_id == MT658X_FH_MSDC_PLL) {

			fh_write32(MBIST_CFG_2, 0x00000009);	/* divide by 9+1 */

			fh_write32(CLK_CFG_3, 0x00000001);	/* enable it */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x00080001);	/* pll_pre_clk [1000: AD_MSDCPLL_H208M] */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x00080001);	/* pll_clk_sel [0000: pll_pre_clk] */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x00080F01);	/* abist_clk_sel [1111: pll_clk_out] */
			udelay(1000);

			fh_write32(CLK26CALI, 0x00000001);

		} else if (pll_id == MT658X_FH_TVD_PLL) {
			fh_write32(MBIST_CFG_2, 0x00000009);	/* divide by 9+1 */

			fh_write32(CLK_CFG_3, 0x00000001);	/* enable it */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x00090001);	/* pll_pre_clk [1001: AD_TVHDMI_H_CK] */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x00090001);	/* pll_clk_sel [0000: pll_pre_clk] */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x00090F01);	/* abist_clk_sel [1111: pll_clk_out] */
			udelay(1000);

			fh_write32(CLK26CALI, 0x00000001);
		} else if (pll_id == MT658X_FH_LVDS_PLL) {
			fh_write32(MBIST_CFG_2, 0x00000009);	/* divide by 9+1 */

			fh_write32(CLK_CFG_3, 0x00000001);	/* enable it */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x000A0001);	/* pll_pre_clk [1010: AD_LVDS_H180M_CK] */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x000A0001);	/* pll_clk_sel [0000: pll_pre_clk] */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x000A0F01);	/* abist_clk_sel [1111: pll_clk_out] */
			udelay(1000);

			fh_write32(CLK26CALI, 0x00000001);
		} else if (pll_id == MT658X_FH_ARM_PLL2) {
			fh_write32(MBIST_CFG_2, 0x00000009);	/* divide by 9+1 */
			fh_write32(CLK_CFG_3, 0x00000001);	/* enable it */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x00000001);	/* pll_pre_clk [1010: AD_LVDS_H180M_CK] */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x00000001);	/* pll_clk_sel [0000: pll_pre_clk] */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x00000401);	/* abist_clk_sel [1111: pll_clk_out] */
			udelay(1000);
			fh_write32(CLK26CALI, 0x00000001);
		} else if (pll_id == MT658X_FH_MM_PLL) {
			fh_write32(MBIST_CFG_2, 0x00000009);	/* divide by 9+1 */
			fh_write32(CLK_CFG_3, 0x00000001);	/* enable it */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x000B0001);	/* pll_pre_clk [1010: AD_LVDS_H180M_CK] */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x000B0001);	/* pll_clk_sel [0000: pll_pre_clk] */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x000B0F01);	/* abist_clk_sel [1111: pll_clk_out] */
			udelay(1000);
			fh_write32(CLK26CALI, 0x00000001);
		} else if (pll_id == MT658X_FH_VDEC_PLL) {
			fh_write32(MBIST_CFG_2, 0x00000009);	/* divide by 9+1 */
			fh_write32(CLK_CFG_3, 0x00000001);	/* enable it */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x000A0001);	/* pll_pre_clk [1010: AD_LVDS_H180M_CK] */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x0F0A0001);	/* pll_clk_sel [0000: pll_pre_clk] */
			udelay(1000);
			fh_write32(CLK_CFG_3, 0x0F0A0F01);	/* abist_clk_sel [1111: pll_clk_out] */
			udelay(1000);
			fh_write32(CLK26CALI, 0x00000001);
		}

	} else {
		FH_MSG("  bad argument!!");
	}

	g_curr_clkgen = pll_id;

	return count;

	/* return -EINVAL; */
}

#endif				/* MT_FH_CLK_GEN */
#if MT_FH_SUSPEND
/*
static struct miscdevice mt_fh35_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mtfh35",
	//.fops = &mt_fh_fops, //TODO: Interface for UI maybe in the future...
};
*/
struct platform_device fh35_dev = {
	.name = "fh35",
	.id = -1,
};

static int mt_fh35_drv_probe(struct platform_device *dev)
{
	int err = 0;

	FH_MSG("EN: mt_fh35_probe()");

/* if ((err = misc_register(&mt_fh35_device))) */
/* FH_MSG("register fh driver error!%d", err); */
	return err;
}

static int mt_fh35_drv_remove(struct platform_device *dev)
{
	int err = 0;

/* if ((err = misc_deregister(&mt_fh35_device))) */
/* FH_MSG("deregister fh driver error!"); */

	return err;
}

static void mt_fh35_drv_shutdown(struct platform_device *dev)
{
	FH_MSG("mt_fh35_shutdown");
}

static int mt_fh35_drv_suspend(struct platform_device *dev, pm_message_t state)
{
	struct freqhopping_ioctl fh_ctl;
	int i, ret = 0;
	FH_MSG("35-supd-");
	memcpy((void *)&g_fh_pll_sus, (void *)&g_fh_pll, sizeof(g_fh_pll));
	for (i = 0; i < MT_FHPLL_MAX; i++) {
		if (g_fh_pll_sus[i].pll_status) {
			fh_ctl.pll_id = i;
			ret = __freqhopping_ctrl(&fh_ctl, false);
		}
	}
	disable_clock(MT_CG_PERI_FHCTL_PDN, "FREQHOP");
	g_clk_en = 0;
	return 0;
};

static int mt_fh35_drv_resume(struct platform_device *dev)
{
	struct freqhopping_ioctl fh_ctl;
	const struct freqhopping_ssc *pSSC_setting = NULL;
	int i, ret = 0;
	FH_MSG("35+resm+");
	enable_clock(MT_CG_PERI_FHCTL_PDN, "FREQHOP");
	g_clk_en = 1;
	for (i = 0; i < MT_FHPLL_MAX; i++) {
		if (g_fh_pll_sus[i].pll_status) {
			fh_ctl.pll_id = i;
			if (g_fh_pll_sus[i].setting_id) {
				pSSC_setting = id_to_ssc(i, g_fh_pll_sus[i].setting_id);
				if (pSSC_setting) {
					memcpy((void *)&fh_ctl.ssc_setting, (void *)pSSC_setting,
					       sizeof(struct freqhopping_ssc));
					ret = __freqhopping_ctrl(&fh_ctl, true);
				} else {
					FH_MSG("id %x setting null", i);
				}
			} else {
				FH_MSG("id %x setting %d", i, g_fh_pll_sus[i].setting_id);
			}
		}
	}
	return 0;
};

static struct platform_driver fh35_driver = {
	.probe = mt_fh35_drv_probe,
	.remove = mt_fh35_drv_remove,
	.shutdown = mt_fh35_drv_shutdown,
	.suspend = mt_fh35_drv_suspend,
	.resume = mt_fh35_drv_resume,
	.driver = {
		   .name = "fh35",
		   .owner = THIS_MODULE,
		   },
};

static int __init fh35_init(void)
{
	int err;
	err = platform_device_register(&fh35_dev);
	err = platform_driver_register(&fh35_driver);

	return err;
};
module_init(fh35_init);

static void __exit fh35_exit(void)
{
	platform_driver_unregister(&fh35_driver);
	platform_device_unregister(&fh35_dev);
};
module_exit(fh35_exit);

#endif
/* TODO: __init void mt_freqhopping_init(void) */
static void mt_fh_hal_init(void)
{
	int i;
/* int             ret = 0; */
	unsigned long flags;

	FH_MSG("EN: %s", __func__);

	if (g_initialize == 1) {
		FH_MSG("already init!");
		return;
	}

	/* init hopping table for mempll 200<->266 */
	memset(g_mempll_fh_table, 0, sizeof(g_mempll_fh_table));


	for (i = 0; i < MT_FHPLL_MAX; i++) {

		/* TODO: use the g_fh_pll[] to init the FHCTL */
		spin_lock_irqsave(&freqhopping_lock, flags);

		g_fh_pll[i].setting_id = 0;

		fh_write32(REG_FHCTL0_CFG + (i * 0x14), 0x00000000);	/* No SSC and FH enabled */
		fh_write32(REG_FHCTL0_UPDNLMT + (i * 0x14), 0x00000000);	/* clear all the settings */
		fh_write32(REG_FHCTL0_DDS + (i * 0x14), 0x00000000);	/* clear all the settings */

		/* TODO: do we need this */
		/* fh_write32(REG_FHCTL0_MON+(i*0x10), 0x00000000); //clear all the settings */

		spin_unlock_irqrestore(&freqhopping_lock, flags);
	}

	/* TODO: update each PLL status (go through g_fh_pll) */
	/* TODO: wait for sophie's table & query the EMI clock */
	/* TODO: ask sophie to call this init function during her init call (mt_clkmgr_init() ??) */
	/* TODO: call __freqhopping_ctrl() to init each pll */
#if (MT_FH_SUSPEND == 0)

	disable_clock(MT_CG_PERI_FHCTL_PDN, "FREQHOP");
	g_clk_en = 0;
#endif
	g_initialize = 1;

	/* freqhopping_config(MT658X_FH_MEM_PLL, 266000, true); //Enable MEMPLL SSC */
#if 0
	freqhopping_config(MT658X_FH_TVD_PLL, 1188000, true);	/* TODO: test only */
	freqhopping_config(MT658X_FH_LVDS_PLL, 1200000, true);	/* TODO: test only */
	freqhopping_config(MT658X_FH_MAIN_PLL, 1612000, true);	/* TODO: test only */
	freqhopping_config(MT658X_FH_MSDC_PLL, 1599000, true);	/* TODO: test only */
#endif
}

static void mt_fh_hal_lock(unsigned long *flags)
{
	spin_lock_irqsave(&freqhopping_lock, *flags);
}

static void mt_fh_hal_unlock(unsigned long *flags)
{
	spin_unlock_irqrestore(&freqhopping_lock, *flags);
}

static int mt_fh_hal_get_init(void)
{
	return g_initialize;
}

/* TODO: module_init(mt_freqhopping_init); */
/* TODO: module_exit(cpufreq_exit); */

static struct mt_fh_hal_driver g_fh_hal_drv;

struct mt_fh_hal_driver *mt_get_fh_hal_drv(void)
{
	memset(&g_fh_hal_drv, 0, sizeof(g_fh_hal_drv));

	g_fh_hal_drv.fh_pll = g_fh_pll;
	g_fh_hal_drv.fh_usrdef = mt_ssc_fhpll_userdefined;
	g_fh_hal_drv.pll_cnt = MT658X_FH_PLL_TOTAL_NUM;
	g_fh_hal_drv.mempll = MT658X_FH_MEM_PLL;
	g_fh_hal_drv.mainpll = MT658X_FH_MAIN_PLL;
	g_fh_hal_drv.msdcpll = MT658X_FH_MSDC_PLL;
	g_fh_hal_drv.mmpll = MT658X_FH_MM_PLL;
	g_fh_hal_drv.vencpll = MT658X_FH_VENC_PLL;
	g_fh_hal_drv.lvdspll = MT658X_FH_LVDS_PLL;


	g_fh_hal_drv.mt_fh_hal_init = mt_fh_hal_init;

#if MT_FH_CLK_GEN
	g_fh_hal_drv.proc.clk_gen_read = freqhopping_clkgen_proc_read;
	g_fh_hal_drv.proc.clk_gen_write = freqhopping_clkgen_proc_write;
#endif

	g_fh_hal_drv.proc.dramc_read = freqhopping_dramc_proc_read;
	g_fh_hal_drv.proc.dramc_write = freqhopping_dramc_proc_write;
	g_fh_hal_drv.proc.dumpregs_read = freqhopping_dumpregs_proc_read;

	g_fh_hal_drv.proc.dvfs_read = freqhopping_dvfs_proc_read;
	g_fh_hal_drv.proc.dvfs_write = freqhopping_dvfs_proc_write;

	g_fh_hal_drv.mt_fh_hal_ctrl = __freqhopping_ctrl;
	g_fh_hal_drv.mt_fh_lock = mt_fh_hal_lock;
	g_fh_hal_drv.mt_fh_unlock = mt_fh_hal_unlock;
	g_fh_hal_drv.mt_fh_get_init = mt_fh_hal_get_init;

	g_fh_hal_drv.mt_fh_popod_restore = mt_fh_hal_popod_restore;
	g_fh_hal_drv.mt_fh_popod_save = mt_fh_hal_popod_save;

	g_fh_hal_drv.mt_l2h_mempll = mt_fh_hal_l2h_mempll;
	g_fh_hal_drv.mt_h2l_mempll = mt_fh_hal_h2l_mempll;
	g_fh_hal_drv.mt_dram_overclock = mt_fh_hal_dram_overclock;
	g_fh_hal_drv.mt_get_dramc = mt_fh_hal_get_dramc;

	return &g_fh_hal_drv;
}
