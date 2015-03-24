/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/xlog.h>
#include <linux/jiffies.h>
#include <linux/seq_file.h>


#include <asm/system.h>
#include <asm/uaccess.h>

#include "mach/mt_typedefs.h"
#include "mach/mt_clkmgr.h"
#include "mach/mt_gpufreq.h"
#include <drivers/misc/mediatek/power/mt8135/upmu_common.h>
#include "mach/sync_write.h"
#include "mach/mt_boot.h"

/***************************
* debug message
****************************/
#define dprintk(fmt, args...)                                           \
do {                                                                    \
    if (mt_gpufreq_debug) {                                             \
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", fmt, ##args);   \
    }                                                                   \
} while (0)

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend mt_gpufreq_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 200,
	.suspend = NULL,
	.resume = NULL,
};
#endif

/***************************
* MT8135 GPU Power Table
* updated new table @ 7/18
****************************/
static struct mt_gpufreq_power_info mt_gpufreqs_golden_power[] = {
	{.gpufreq_khz = GPU_DVFS_OVER_F550, .gpufreq_power = 1050},
	{.gpufreq_khz = GPU_DVFS_OVER_F500, .gpufreq_power = 1000},
	{.gpufreq_khz = GPU_DVFS_OVER_F475, .gpufreq_power = 950},
	{.gpufreq_khz = GPU_DVFS_OVER_F450, .gpufreq_power = 900},
	{.gpufreq_khz = GPU_DVFS_F1, .gpufreq_power = 800},
	{.gpufreq_khz = GPU_DVFS_F2, .gpufreq_power = 650},
	{.gpufreq_khz = GPU_DVFS_F3, .gpufreq_power = 500},
	{.gpufreq_khz = GPU_DVFS_F4, .gpufreq_power = 450},
	{.gpufreq_khz = GPU_DVFS_F5, .gpufreq_power = 400},
	{.gpufreq_khz = GPU_DVFS_F6, .gpufreq_power = 380},
	{.gpufreq_khz = GPU_DVFS_F7, .gpufreq_power = 360},
	{.gpufreq_khz = GPU_DVFS_F8, .gpufreq_power = 280},
};

/**************************
* enable GPU DVFS count
***************************/
static int g_gpufreq_dvfs_disable_count;

static unsigned int g_gpu_max_freq = GPU_DVFS_F1;
static unsigned int g_cur_gpu_freq = GPU_DVFS_F3;
static unsigned int g_cur_gpu_volt;
static unsigned int g_cur_gpu_load;

static unsigned int g_cur_freq_init_keep;
static unsigned int g_freq_new_init_keep;
static unsigned int g_volt_new_init_keep;

/* In default settiing, freq_table[0] is max frequency, freq_table[num-1] is min frequency,*/
static unsigned int g_gpufreq_max_id;

/* If not limited, it should be set to freq_table[0] (MAX frequency) */
static unsigned int g_limited_max_id;
static unsigned int g_limited_min_id;

static bool mt_gpufreq_max_checked;
static bool mt_gpufreq_debug;
static bool mt_gpufreq_pause;
static bool mt_gpufreq_keep_max_frequency;
static bool mt_gpufreq_keep_specific_frequency;
static unsigned int mt_gpufreq_fixed_frequency;
static unsigned int mt_gpufreq_fixed_voltage;

static DEFINE_SPINLOCK(mt_gpufreq_lock);

static unsigned int mt_gpufreqs_num;
static struct mt_gpufreq_info *mt_gpufreqs;
static struct mt_gpufreq_power_info *mt_gpufreqs_power;
static struct mt_gpufreq_power_info *mt_gpufreqs_default_power;

static bool mt_gpufreq_registered;
static bool mt_gpufreq_registered_statewrite;
static bool mt_gpufreq_already_non_registered;

static unsigned int mt_gpufreq_enable_univpll;
static unsigned int mt_gpufreq_enable_mmpll;

/******************************
* Extern Function Declaration
*******************************/
extern int mtk_gpufreq_register(struct mt_gpufreq_power_info *freqs, int num);


/******************************
* Extern Function Declaration
*******************************/
extern int pmic_get_gpu_status_bit_info(void);

/******************************
* Internal prototypes
*******************************/

/* for MET GPU info */
#if CONFIG_SUPPORT_MET_GPU
static char header[] =
    "met-info [000] 0.0: ms_ud_sys_header: GPU_Index," "GPU_freq,GPU_volt,GPU_load,d,d,d\n";
static char help[] = "  --met_gpu                             monitor mtkgpu\n";
unsigned int met_gpu_enable = 0;

void GPU_Index(void)
{
	if (met_gpu_enable) {
		trace_printk("%d,%d,%d\n", g_cur_gpu_freq / 1000, g_cur_gpu_volt / 10,
			     g_cur_gpu_load);
	}
}

void met_gpu_polling(unsigned long long stamp, int cpu)
{
	GPU_Index();
}

static int met_gpu_print_help(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, help);
}

static int met_gpu_print_header(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, header);
}

static void met_gpu_start(void)
{
	met_gpu_enable = 1;
	return;
}

static void met_gpu_stop(void)
{
	met_gpu_enable = 0;
	return;
}

struct metdevice met_gpu = {
	.name = "gpu",
	.owner = THIS_MODULE,
	.type = MET_TYPE_BUS,
	.start = met_gpu_start,
	.stop = met_gpu_stop,
	.timed_polling = met_gpu_polling,
	.tagged_polling = met_gpu_polling,
	.print_help = met_gpu_print_help,
	.print_header = met_gpu_print_header,
};
EXPORT_SYMBOL(met_gpu);
#endif



/*****************************************************************
* Check GPU current frequency and enable pll in initial and late resume.
*****************************************************************/
static void mt_gpufreq_check_freq_and_set_pll(void)
{
	dprintk("mt_gpufreq_check_freq_and_set_pll, g_cur_freq = %d\n", g_cur_gpu_freq);

	switch (g_cur_gpu_freq) {
	case GPU_MMPLL_D3_CLOCK:
	case GPU_MMPLL_D4_CLOCK:
	case GPU_MMPLL_D5_CLOCK:
	case GPU_MMPLL_D6_CLOCK:
	case GPU_MMPLL_D7_CLOCK:
	case GPU_MMPLL_773_D3_CLOCK:
	case GPU_MMPLL_1350_D3_CLOCK:
	case GPU_MMPLL_1425_D3_CLOCK:
	case GPU_MMPLL_1500_D3_CLOCK:
	case GPU_MMPLL_1650_D3_CLOCK:
		if (mt_gpufreq_enable_mmpll == 0) {
			enable_pll(MMPLL, "GPU_DVFS");
			mt_gpufreq_enable_mmpll = 1;
		}
		break;

	case GPU_UNIVPLL_D5_CLOCK:
		if (mt_gpufreq_enable_univpll == 0) {
			enable_pll(UNIVPLL, "GPU_DVFS");
			mt_gpufreq_enable_univpll = 1;
		}
		break;
	default:
		if (mt_gpufreq_enable_mmpll == 0) {
			enable_pll(MMPLL, "GPU_DVFS");
			mt_gpufreq_enable_mmpll = 1;
		}
		break;

	}
}


static void mt_gpufreq_read_current_clock_from_reg(void)
{
	unsigned int reg_value = 0;
	unsigned int freq = 0;
	DRV_WriteReg32(MBIST_CFG_3, 0x02000000);	/* hf_fmfg_ck */
	DRV_WriteReg32(MBIST_CFG_2, 0x00000000);
	DRV_WriteReg32(CLK26CALI, 0x00000010);
	mt_gpufreq_udelay(50);

	/* reg_value = (DRV_Reg32(CKSTA_REG) & BITMASK(31 : 16)) >> 16; */
	reg_value = (DRV_Reg32(CKSTA_REG) & 0xffff0000) >> 16;
	freq = reg_value * 26000 / 1024;	/* KHz */
	dprintk("mt_gpufreq_read_current_clock_from_reg: freq(%d)\n", freq);
}

static void mt_gpufreq_sel(unsigned int src, unsigned int sel)
{
	unsigned int clk_cfg = 0;
	CHIP_SW_VER ver = 0;

	if (src == GPU_FMFG) {
		clk_cfg = DRV_Reg32(CLK_CFG_0);

		switch (sel) {
		case GPU_CKSQ_MUX_CK:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			dprintk("mt_gpufreq_sel - GPU_CKSQ_MUX_CK\n");
			break;
		case GPU_UNIVPLL1_D4:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			dprintk("mt_gpufreq_sel - GPU_UNIVPLL1_D4\n");
			break;
		case GPU_SYSPLL_D2:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			dprintk("mt_gpufreq_sel - GPU_SYSPLL_D2\n");
			break;
		case GPU_SYSPLL_D2P5:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			dprintk("mt_gpufreq_sel - GPU_SYSPLL_D2P5\n");
			break;
		case GPU_SYSPLL_D3:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			dprintk("mt_gpufreq_sel - GPU_SYSPLL_D3\n");
			break;
		case GPU_UNIVPLL_D5:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			dprintk("mt_gpufreq_sel - GPU_UNIVPLL_D5\n");
			break;
		case GPU_UNIVPLL1_D2:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			dprintk("mt_gpufreq_sel - GPU_UNIVPLL1_D2\n");
			break;
		case GPU_MMPLL_D2:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			dprintk("mt_gpufreq_sel - GPU_MMPLL_D2\n");
			break;
		case GPU_MMPLL_D3:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			dprintk("mt_gpufreq_sel - GPU_MMPLL_D3\n");
			break;
		case GPU_MMPLL_D4:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			dprintk("mt_gpufreq_sel - GPU_MMPLL_D4\n");
			break;
		case GPU_MMPLL_D5:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			dprintk("mt_gpufreq_sel - GPU_MMPLL_D5\n");
			break;
		case GPU_MMPLL_D6:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			dprintk("mt_gpufreq_sel - GPU_MMPLL_D6\n");
			break;
		case GPU_MMPLL_D7:
			clk_cfg = (clk_cfg & 0xFFF0FFFF) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_0, clk_cfg);
			dprintk("mt_gpufreq_sel - GPU_MMPLL_D7\n");
			break;
		default:
			dprintk("mt_gpufreq_sel - NOT support PLL select\n");
			break;
		}
	}

	else {
		clk_cfg = DRV_Reg32(CLK_CFG_8);

		/* added 11th, Aug, 2013 */
		ver = mt_get_chip_sw_ver();
		if (CHIP_SW_VER_02 <= ver) {
			/* MT8135 SW V2 */
			if (GPU_MEM_TOP_MEM == sel) {
				sel = GPU_MEM_TOP_SMI;
			}
		}

		switch (sel) {
		case GPU_MEM_CKSQ_MUX_CK:
			clk_cfg = (clk_cfg & 0xfffcffff) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_8, clk_cfg);
			dprintk("mt_gpufreq_sel - GPU_MEM_CKSQ_MUX_CK\n");
			break;
		case GPU_MEM_TOP_SMI:
			clk_cfg = (clk_cfg & 0xfffcffff) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_8, clk_cfg);
			dprintk("mt_gpufreq_sel - GPU_MEM_TOP_SMI\n");
			break;
		case GPU_MEM_TOP_MFG:
			clk_cfg = (clk_cfg & 0xfffcffff) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_8, clk_cfg);
			dprintk("mt_gpufreq_sel - GPU_MEM_TOP_MFG IC ver %d\n", ver);
			break;
		case GPU_MEM_TOP_MEM:
			clk_cfg = (clk_cfg & 0xfffcffff) | (sel << 16);
			DRV_WriteReg32(CLK_CFG_8, clk_cfg);
			dprintk("mt_gpufreq_sel - GPU_MEM_TOP_MEM\n");
			break;
		default:
			dprintk("mt_gpufreq_sel - not support mem sel\n");
			break;
		}
	}
}


/**************************************
* check if maximum frequency is needed
***************************************/
static int mt_gpufreq_keep_max_freq(unsigned int freq_old, unsigned int freq_new)
{
	if (mt_gpufreq_keep_max_frequency == true)
		return 1;

	return 0;
}

/*****************************************************************
* Check if gpufreq registration is done
*****************************************************************/
bool mt_gpufreq_is_registered_get(void)
{
	if ((mt_gpufreq_registered == true) || (mt_gpufreq_already_non_registered == true))
		return true;
	else
		return false;
}
EXPORT_SYMBOL(mt_gpufreq_is_registered_get);


/* Default power table when mt_gpufreq_non_register() */
static void mt_setup_gpufreqs_default_power_table(int num)
{
	int j = 0;

	mt_gpufreqs_default_power = kzalloc((1) * sizeof(struct mt_gpufreq_power_info), GFP_KERNEL);
	if (mt_gpufreqs_default_power == NULL) {
		dprintk("GPU default power table memory allocation fail\n");
		return;
	}

	mt_gpufreqs_default_power[0].gpufreq_khz = g_cur_gpu_freq;

	for (j = 0; j < ARRAY_SIZE(mt_gpufreqs_golden_power); j++) {
		if (g_cur_gpu_freq == mt_gpufreqs_golden_power[j].gpufreq_khz) {
			mt_gpufreqs_default_power[0].gpufreq_power =
			    mt_gpufreqs_golden_power[j].gpufreq_power;
			break;
		}
	}

	dprintk("mt_gpufreqs_default_power[0].gpufreq_khz = %u\n",
		mt_gpufreqs_default_power[0].gpufreq_khz);
	dprintk("mt_gpufreqs_default_power[0].gpufreq_power = %u\n",
		mt_gpufreqs_default_power[0].gpufreq_power);

#ifdef CONFIG_THERMAL
	mtk_gpufreq_register(mt_gpufreqs_default_power, 1);
#endif
}

static void mt_setup_gpufreqs_power_table(int num)
{
	int i = 0, j = 0;

	mt_gpufreqs_power = kzalloc((num) * sizeof(struct mt_gpufreq_power_info), GFP_KERNEL);
	if (mt_gpufreqs_power == NULL) {
		dprintk("GPU power table memory allocation fail\n");
		return;
	}

	for (i = 0; i < num; i++) {
		mt_gpufreqs_power[i].gpufreq_khz = mt_gpufreqs[i].gpufreq_khz;

		for (j = 0; j < ARRAY_SIZE(mt_gpufreqs_golden_power); j++) {
			if (mt_gpufreqs[i].gpufreq_khz == mt_gpufreqs_golden_power[j].gpufreq_khz) {
				mt_gpufreqs_power[i].gpufreq_power =
				    mt_gpufreqs_golden_power[j].gpufreq_power;
				break;
			}
		}

		dprintk("mt_gpufreqs_power[%d].gpufreq_khz = %u\n", i,
			mt_gpufreqs_power[i].gpufreq_khz);
		dprintk("mt_gpufreqs_power[%d].gpufreq_power = %u\n", i,
			mt_gpufreqs_power[i].gpufreq_power);
	}

#ifdef CONFIG_THERMAL
	mtk_gpufreq_register(mt_gpufreqs_power, num);
#endif
}

/***********************************************
* register frequency table to gpufreq subsystem
************************************************/
static int mt_setup_gpufreqs_table(struct mt_gpufreq_info *freqs, int num)
{
	int i = 0;

	mt_gpufreqs = kzalloc((num) * sizeof(*freqs), GFP_KERNEL);
	if (mt_gpufreqs == NULL)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		mt_gpufreqs[i].gpufreq_khz = freqs[i].gpufreq_khz;
		mt_gpufreqs[i].gpufreq_lower_bound = freqs[i].gpufreq_lower_bound;
		mt_gpufreqs[i].gpufreq_upper_bound = freqs[i].gpufreq_upper_bound;
		mt_gpufreqs[i].gpufreq_volt = freqs[i].gpufreq_volt;
		mt_gpufreqs[i].gpufreq_remap = freqs[i].gpufreq_remap;

		dprintk("freqs[%d].gpufreq_khz = %u\n", i, freqs[i].gpufreq_khz);
		dprintk("freqs[%d].gpufreq_lower_bound = %u\n", i, freqs[i].gpufreq_lower_bound);
		dprintk("freqs[%d].gpufreq_upper_bound = %u\n", i, freqs[i].gpufreq_upper_bound);
		dprintk("freqs[%d].gpufreq_volt = %u\n", i, freqs[i].gpufreq_volt);
		dprintk("freqs[%d].gpufreq_remap = %u\n", i, freqs[i].gpufreq_remap);
	}

	mt_gpufreqs_num = num;

	/* Initial frequency and voltage was already set in mt_gpufreq_set_initial() */
#if 0				/* 1 */
	g_cur_gpu_freq = freqs[0].gpufreq_khz;
	g_cur_gpu_volt = freqs[0].gpufreq_volt;
#endif
	g_limited_max_id = 0;
	g_limited_min_id = mt_gpufreqs_num - 1;

	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
		    "mt_setup_gpufreqs_table, g_cur_freq = %d, g_cur_gpu_volt = %d\n",
		    g_cur_gpu_freq, g_cur_gpu_volt);

	mt_setup_gpufreqs_power_table(num);

	return 0;
}

/*****************************
* set GPU DVFS status
******************************/
int mt_gpufreq_state_set(int enabled)
{

	if (enabled) {
		if (!mt_gpufreq_pause) {
			xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
				    "gpufreq already enabled\n");
			return 0;
		}

		/*****************
		* enable GPU DVFS
		******************/
		g_gpufreq_dvfs_disable_count--;
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "enable GPU DVFS: g_gpufreq_dvfs_disable_count = %d\n",
			    g_gpufreq_dvfs_disable_count);

		/***********************************************
		* enable DVFS if no any module still disable it
		************************************************/
		if (g_gpufreq_dvfs_disable_count <= 0) {
			mt_gpufreq_pause = false;
		} else {
			xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
				    "someone still disable gpufreq, cannot enable it\n");
		}
	} else {
		/******************
		* disable GPU DVFS
		*******************/
		g_gpufreq_dvfs_disable_count++;
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "disable GPU DVFS: g_gpufreq_dvfs_disable_count = %d\n",
			    g_gpufreq_dvfs_disable_count);

		if (mt_gpufreq_pause) {
			xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
				    "gpufreq already disabled\n");
			return 0;
		}

		mt_gpufreq_pause = true;
	}

	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_state_set);

static void mt_gpu_clock_switch(unsigned int sel)
{
	switch (sel) {
	case GPU_MMPLL_D3_CLOCK:
	case GPU_MMPLL_D4_CLOCK:
	case GPU_MMPLL_D5_CLOCK:
	case GPU_MMPLL_D6_CLOCK:
	case GPU_MMPLL_D7_CLOCK:
		if (mt_gpufreq_enable_mmpll == 0) {
			enable_pll(MMPLL, "GPU_DVFS");
			mt_gpufreq_enable_mmpll = 1;
		}
		mt_gpufreq_sel(GPU_FMFG, GPU_SYSPLL_D3);
#if 0
		/* MMPLL_CON0[8:6] = 1, postdiv = /2, switch MMPLL to 1185 */
 DRV_WriteReg32(MMPLL_CON0, (DRV_Reg32(MMPLL_CON0) & ~BITMASK(8 : 6)) | BITS(8:6, 0));
		pll_fsel(MMPLL, 0xF00B64EC);
#else
		pll_set_freq(MMPLL, GPU_MMPLL_D3_CLOCK * 3);
#endif
		udelay(30);

		if (GPU_MMPLL_D3_CLOCK == sel) {
			mt_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D3);
		} else if (GPU_MMPLL_D4_CLOCK == sel) {
			mt_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D4);
		} else if (GPU_MMPLL_D5_CLOCK == sel) {
			mt_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D5);
		} else if (GPU_MMPLL_D6_CLOCK == sel) {
			mt_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D6);
		} else {
			mt_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D7);
		}

		/*
		   GPU internal mem clock:
		   E1, GPU core clock > 260M, sel TOP_MEM(333 if MEM clock is 1333)
		   else                 sel TOP_MFG(equal to GPU core clock)
		   E2, GPU core clock > 260M, sel TOP_SMI(322, DRAM will be 1600, can not use TOP_MEM)
		   else                 sel TOP_MFG(equal to GPU core clock)
		 */
		if (sel >= GPU_DVFS_F2) {
			mt_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MEM);
		} else {
			mt_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MFG);
		}

		break;

	case GPU_MMPLL_773_D3_CLOCK:
		if (mt_gpufreq_enable_mmpll == 0) {
			enable_pll(MMPLL, "GPU_DVFS");
			mt_gpufreq_enable_mmpll = 1;
		}
		mt_gpufreq_sel(GPU_FMFG, GPU_SYSPLL_D3);
#if 0
 DRV_WriteReg32(MMPLL_CON0, (DRV_Reg32(MMPLL_CON0) & ~BITMASK(8 : 6)) | BITS(8:6, 1));
		pll_fsel(MMPLL, 0x800EE000);
#else
		pll_set_freq(MMPLL, GPU_MMPLL_773_D3_CLOCK * 3);
#endif
		udelay(30);

		mt_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D3);
		mt_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MFG);
		break;

		/* for GPU overclocking */
	case GPU_MMPLL_1350_D3_CLOCK:
		if (mt_gpufreq_enable_mmpll == 0) {
			enable_pll(MMPLL, "GPU_DVFS");
			mt_gpufreq_enable_mmpll = 1;
		}

		pll_set_freq(MMPLL, GPU_MMPLL_1350_D3_CLOCK * 3);
		mt_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D3);
		mt_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MEM);

		break;

	case GPU_MMPLL_1425_D3_CLOCK:
		if (mt_gpufreq_enable_mmpll == 0) {
			enable_pll(MMPLL, "GPU_DVFS");
			mt_gpufreq_enable_mmpll = 1;
		}

		pll_set_freq(MMPLL, GPU_MMPLL_1425_D3_CLOCK * 3);
		mt_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D3);
		mt_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MEM);
		break;

	case GPU_MMPLL_1500_D3_CLOCK:
		if (mt_gpufreq_enable_mmpll == 0) {
			enable_pll(MMPLL, "GPU_DVFS");
			mt_gpufreq_enable_mmpll = 1;
		}

		pll_set_freq(MMPLL, GPU_MMPLL_1500_D3_CLOCK * 3);
		mt_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D3);
		mt_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MEM);
		break;

	case GPU_MMPLL_1650_D3_CLOCK:
		if (mt_gpufreq_enable_mmpll == 0) {
			enable_pll(MMPLL, "GPU_DVFS");
			mt_gpufreq_enable_mmpll = 1;
		}

		pll_set_freq(MMPLL, GPU_MMPLL_1650_D3_CLOCK * 3);
		mt_gpufreq_sel(GPU_FMFG, GPU_MMPLL_D3);
		mt_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MEM);
		break;


	case GPU_UNIVPLL_D5_CLOCK:
		if (mt_gpufreq_enable_univpll == 0) {
			enable_pll(UNIVPLL, "GPU_DVFS");
			mt_gpufreq_enable_univpll = 1;
		}
		mt_gpufreq_sel(GPU_FMFG, GPU_UNIVPLL_D5);
		mt_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MFG);
		break;

	default:
		if (mt_gpufreq_enable_mmpll == 0) {
			enable_pll(MMPLL, "GPU_DVFS");
			mt_gpufreq_enable_mmpll = 1;
		}
		pll_set_freq(MMPLL, sel * 3);

		if (sel >= GPU_DVFS_F2) {
			mt_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MEM);
		} else {
			mt_gpufreq_sel(GPU_FMEM, GPU_MEM_TOP_MFG);
		}
		break;
		/* not return for support any freq set by sysfs */

	}

	g_cur_gpu_freq = sel;

	mt_gpufreq_read_current_clock_from_reg();

}


void mt_gpu_vgpu_sel_1_15(void)
{
	dprintk("mt_gpu_vgpu_sel_1_15\n");

	upmu_set_vgpu_en_ctrl(0x0);	/* SW control */
	upmu_set_vgpu_en(0x1);	/* enable vgpu power */
	upmu_set_vgpu_vosel(GPU_POWER_VGPU_1_15V);
}
EXPORT_SYMBOL(mt_gpu_vgpu_sel_1_15);

void mt_gpu_disable_vgpu(void)
{
	dprintk("mt_gpu_disable_vgpu\n");
	upmu_set_vgpu_en_ctrl(0x0);	/* SW control */
	upmu_set_vgpu_en(0x0);	/* disable vgpu power */
}
EXPORT_SYMBOL(mt_gpu_disable_vgpu);

static void mt_gpu_vgpu_sel_1_10(void)
{

	upmu_set_vgpu_en_ctrl(0x0);	/* SW control */
	upmu_set_vgpu_en(0x1);	/* enable vgpu power */
	upmu_set_vgpu_vosel(GPU_POWER_VGPU_1_10V);
}

static void mt_gpu_vgpu_sel_1_05(void)
{

	upmu_set_vgpu_en_ctrl(0x0);	/* SW control */
	upmu_set_vgpu_en(0x1);	/* enable vgpu power */
	upmu_set_vgpu_vosel(GPU_POWER_VGPU_1_05V);
}


static void mt_gpu_vgpu_sel_1_025(void)
{

	upmu_set_vgpu_en_ctrl(0x0);	/* SW control */
	upmu_set_vgpu_en(0x1);	/* enable vgpu power */
	upmu_set_vgpu_vosel(GPU_POWER_VGPU_1_025V);
}

static void mt_gpu_vgpu_sel_1_00(void)
{

	upmu_set_vgpu_en_ctrl(0x0);	/* SW control */
	upmu_set_vgpu_en(0x1);	/* enable vgpu power */
	upmu_set_vgpu_vosel(GPU_POWER_VGPU_1_00V);
}

static void mt_gpu_vgpu_sel_0_95(void)
{

	upmu_set_vgpu_en_ctrl(0x0);	/* SW control */
	upmu_set_vgpu_en(0x1);	/* enable vgpu power */
	upmu_set_vgpu_vosel(GPU_POWER_VGPU_0_95V);
}

static void mt_gpu_volt_switch(unsigned int volt_old, unsigned int volt_new)
{

	if (volt_old < volt_new) {
		dprintk("mt_gpu_volt_switch: volt_old(%d) < volt_new(%d)\n", volt_old, volt_new);
	} else if (volt_old > volt_new) {
		dprintk("mt_gpu_volt_switch: volt_old(%d) > volt_new(%d)\n", volt_old, volt_new);
	} else {
		dprintk("mt_gpu_volt_switch: volt_old(%d) == volt_new(%d)\n", volt_old, volt_new);
		return;
	}

	if (volt_new == GPU_POWER_VGPU_1_15V_VOL) {
		mt_gpu_vgpu_sel_1_15();
		dprintk("mt_gpu_volt_switch: switch MFG power to GPU_POWER_VGPU_1_15V\n");
	} else if (volt_new == GPU_POWER_VGPU_1_10V_VOL) {
		mt_gpu_vgpu_sel_1_10();
		dprintk("mt_gpu_volt_switch: switch MFG power to GPU_POWER_VGPU_1_10V\n");
	} else if (volt_new == GPU_POWER_VGPU_1_05V_VOL) {
		mt_gpu_vgpu_sel_1_05();
		dprintk("mt_gpu_volt_switch: switch MFG power to GPU_POWER_VGPU_1_05V\n");
	} else if (volt_new == GPU_POWER_VGPU_1_025V_VOL) {
		mt_gpu_vgpu_sel_1_025();
		dprintk("mt_gpu_volt_switch: switch MFG power to GPU_POWER_VGPU_1_025V\n");
	}

	else if (volt_new == GPU_POWER_VGPU_1_00V_VOL) {
		mt_gpu_vgpu_sel_1_00();
		dprintk("mt_gpu_volt_switch: switch MFG power to GPU_POWER_VGPU_1_00V\n");

	} else if (volt_new == GPU_POWER_VGPU_0_95V_VOL) {
		mt_gpu_vgpu_sel_0_95();
		dprintk("mt_gpu_volt_switch: switch MFG power to GPU_POWER_VGPU_0_95V\n");

	} else {
		dprintk("mt_gpu_volt_switch: do not support specified volt (%d)\n", volt_new);
		return;
	}

	g_cur_gpu_volt = volt_new;

}

static void mt_gpu_volt_switch_initial(unsigned int volt_new)
{

	if (volt_new == GPU_POWER_VGPU_1_15V_VOL) {
		mt_gpu_vgpu_sel_1_15();
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "mt_gpu_volt_switch_initial: switch MFG power to GPU_POWER_VGPU_1_15V\n");
	} else if (volt_new == GPU_POWER_VGPU_1_05V_VOL) {
		mt_gpu_vgpu_sel_1_05();
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "mt_gpu_volt_switch_initial: switch MFG power to GPU_POWER_VGPU_1_05V\n");
	} else if (volt_new == GPU_POWER_VGPU_1_00V_VOL) {
		mt_gpu_vgpu_sel_1_00();
		dprintk("mt_gpu_volt_switch_initial: switch MFG power to GPU_POWER_VGPU_1_00V\n");
	} else if (volt_new == GPU_POWER_VGPU_0_95V_VOL) {
		mt_gpu_vgpu_sel_0_95();
		dprintk("mt_gpu_volt_switch_initial: switch MFG power to GPU_POWER_VGPU_0_95V\n");

	} else {
		dprintk("mt_gpu_volt_switch_initial: do not support specified volt (%d)\n",
			volt_new);
		return;
	}

	g_cur_gpu_volt = volt_new;



}

/* get bonding option for GPU max clock setting */
static unsigned int mt_gpufreq_get_max_speed(void)
{
	unsigned int m_hw_res4 = 0;
	unsigned int m_hw2_res4 = 0;

	m_hw_res4  = DRV_Reg32(0xf0009174);
	m_hw2_res4 = DRV_Reg32(0xf00091c4);

	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
		    "mt_gpufreq_get_max_speed m_hw_res4 is %d\n", m_hw_res4);
	m_hw_res4 = m_hw_res4 & (0x0000FFFF);

	/* Turbo with */
	if (0x1000 == m_hw_res4) {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_get_max_speed:Turbo\n");
		return GPU_DVFS_F1;/* GPU_DVFS_OVER_F450 */;
	}
	/* MT8131 */
	else if (0x1003 == m_hw_res4) {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "mt_gpufreq_get_max_speed:MT8131\n");
		return GPU_DVFS_F1;
	}
	/* non-turbo PTPOD_SW_BOND  */
	else if (0x1001 == m_hw_res4) {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_get_max_speed:non-turbo\n");
		/* update with new speed definition @ 2014/3/14 */
		return GPU_DVFS_F1;
		m_hw2_res4 = ((m_hw2_res4 & 0x00000700) >> 8);
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "mt_gpufreq_get_max_speed:m_hw2_res4 is %d\n", m_hw2_res4);
		switch (m_hw2_res4) {
		case 0x001:
			return GPU_BONDING_001;
		case 0x010:
			return GPU_BONDING_010;
		case 0x011:
			return GPU_BONDING_011;
		case 0x100:
			return GPU_BONDING_100;
		case 0x101:
			return GPU_BONDING_101;
		case 0x110:
			return GPU_BONDING_110;
		case 0x111:
			return GPU_BONDING_111;
		case 0x000:
		default:
			return GPU_BONDING_000;

		}
	}

	else {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "mt_gpufreq_get_max_speed:default\n");
		return GPU_DVFS_F1;

	}

}

/***********************************************************
* 1. 3D driver will call mt_gpufreq_set_initial to set initial frequency and voltage
* 2. When GPU idle in intial, voltage could be set directly.
************************************************************/
void mt_gpufreq_set_initial(void)
{
	unsigned int clk_sel = 0;
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
		    "g_cur_gpu_freq = %d, before mt_gpufreq_set_initial\n", g_cur_gpu_freq);

	mt_gpufreq_check_freq_and_set_pll();
	mt_gpu_volt_switch_initial(GPU_POWER_VGPU_1_15V_VOL);

	/* overclocking setting only for test
	   GPU_MMPLL_1650_D3_CLOCK
	   GPU_MMPLL_1500_D3_CLOCK
	   GPU_MMPLL_1425_D3_CLOCK
	   GPU_MMPLL_1350_D3_CLOCK */

	clk_sel = mt_gpufreq_get_max_speed();
	mt_gpu_clock_switch(clk_sel);
	g_gpu_max_freq = clk_sel;

	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
		    "mt_gpufreq_set_initial, g_cur_gpu_volt = %d, g_cur_gpu_freq = %d\n",
		    g_cur_gpu_volt, g_cur_gpu_freq);
}
EXPORT_SYMBOL(mt_gpufreq_set_initial);


/*****************************************
* frequency ramp up and ramp down handler
******************************************/
/***********************************************************
* [note]
* 1. frequency ramp up need to wait voltage settle
* 2. frequency ramp down do not need to wait voltage settle
************************************************************/
static void mt_gpufreq_set(unsigned int freq_old, unsigned int freq_new, unsigned int volt_old,
			   unsigned int volt_new)
{
	if (freq_new > freq_old) {
		mt_gpu_volt_switch(volt_old, volt_new);
		udelay(30);
		mt_gpu_clock_switch(freq_new);
	} else {
		mt_gpu_clock_switch(freq_new);
		mt_gpu_volt_switch(volt_old, volt_new);
		udelay(30);
	}
}

static int mt_gpufreq_look_up(unsigned int load)
{
	int i = 0, remap = 100;

  /**************************
    * look up the remap value
    ***************************/
	for (i = 0; i < mt_gpufreqs_num; i++) {
		if (mt_gpufreqs[i].gpufreq_khz == g_cur_gpu_freq) {
			remap = mt_gpufreqs[i].gpufreq_remap;
			break;
		}
	}

	load = (load * remap) / 100;
	g_cur_gpu_load = load;
	dprintk("GPU Loading = %d\n", load);

  /******************************
    * look up the target frequency
    *******************************/
	for (i = 0; i < mt_gpufreqs_num; i++) {
		if (load > mt_gpufreqs[i].gpufreq_lower_bound
		    && load <= mt_gpufreqs[i].gpufreq_upper_bound) {
			return i;
		}
	}

	return (mt_gpufreqs_num - 1);
}


static void mt_gpufreq_maxfreq_check(void)
{
	if (GPU_DVFS_F1 != g_gpu_max_freq) {
		/* keep max freq setting, DVFS will setting default GPU_DVFS_F1 as max freq */
		mt_gpufreqs[0].gpufreq_khz = g_gpu_max_freq;
		/* mt_gpufreqs_power[0].gpufreq_power keep as intial registed,as power consumption is estimated value */
	}
	return;
}


/**********************************
* gpufreq target callback function
***********************************/
/*************************************************
* [note]
* 1. handle frequency change request
* 2. call mt_gpufreq_set to set target frequency
**************************************************/
static int mt_gpufreq_target(int idx)
{
	unsigned long flags, target_freq, target_volt;

	/* max freq may be different controlled by efuse, DVFS need check max freq setting */
	if (!mt_gpufreq_max_checked) {
		mt_gpufreq_maxfreq_check();
		mt_gpufreq_max_checked = true;
	}

	spin_lock_irqsave(&mt_gpufreq_lock, flags);

    /**********************************
    * look up for the target GPU OPP
    ***********************************/
	target_freq = mt_gpufreqs[idx].gpufreq_khz;
	target_volt = mt_gpufreqs[idx].gpufreq_volt;

    /**********************************
    * Check if need to keep max frequency
    ***********************************/
	if (mt_gpufreq_keep_max_freq(g_cur_gpu_freq, target_freq)) {
		target_freq = mt_gpufreqs[g_gpufreq_max_id].gpufreq_khz;
		target_volt = mt_gpufreqs[g_gpufreq_max_id].gpufreq_volt;
		dprintk("Keep MAX frequency %d !\n", target_freq);
	}
#if 0				/* mark @ 6th Aug */
    /****************************************************
    * If need to raise frequency, raise to max frequency
    *****************************************************/
	if (target_freq > g_cur_gpu_freq) {
		target_freq = mt_gpufreqs[g_gpufreq_max_id].gpufreq_khz;
		target_volt = mt_gpufreqs[g_gpufreq_max_id].gpufreq_volt;
		dprintk("Need to raise frequency, raise to MAX frequency %d !\n", target_freq);
	}
#endif

	if (target_freq > mt_gpufreqs[g_limited_max_id].gpufreq_khz) {

	    /*********************************************
		* target_freq > limited_freq, need to adjust
		**********************************************/

		dprintk("Limit! Target freq %d > Thermal limit frequency %d! adjusting...\n",
			target_freq, mt_gpufreqs[g_limited_max_id].gpufreq_khz);
		target_freq = mt_gpufreqs[g_limited_max_id].gpufreq_khz;
		target_volt = mt_gpufreqs[g_limited_max_id].gpufreq_volt;
	}

	/************************************************
	* If /proc command fix the frequency.
	*************************************************/
	if (mt_gpufreq_keep_specific_frequency == true) {
		target_freq = mt_gpufreq_fixed_frequency;
		target_volt = mt_gpufreq_fixed_voltage;
		dprintk("Fixed! fixed frequency %d, fixed voltage %d\n", target_freq, target_volt);
	}

    /************************************************
    * target frequency == current frequency, skip it
    *************************************************/
	if (g_cur_gpu_freq == target_freq) {
		spin_unlock_irqrestore(&mt_gpufreq_lock, flags);
		dprintk("GPU frequency from %d MHz to %d MHz (skipped) due to same frequency\n",
			g_cur_gpu_freq / 1000, target_freq / 1000);
		return 0;
	}

	dprintk("GPU current frequency %d MHz, target frequency %d MHz\n", g_cur_gpu_freq / 1000,
		target_freq / 1000);

	spin_unlock_irqrestore(&mt_gpufreq_lock, flags);

  /******************************
    * set to the target freeuency
    *******************************/
	mt_gpufreq_set(g_cur_gpu_freq, target_freq, g_cur_gpu_volt, target_volt);


	return 0;
}


/*********************************
* early suspend callback function
**********************************/
void mt_gpufreq_early_suspend(struct early_suspend *h)
{
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_early_suspend\n");
	dprintk("g_cur_gpu_volt is %d, g_cur_gpu_freq is %d\n", g_cur_gpu_volt, g_cur_gpu_freq);

	/* mt_gpufreq_state_set(0); */

	if (mt_gpufreq_enable_univpll == 1) {
		disable_pll(UNIVPLL, "GPU_DVFS");
		mt_gpufreq_enable_univpll = 0;
	}

	if (mt_gpufreq_enable_mmpll == 1) {
		disable_pll(MMPLL, "GPU_DVFS");
		mt_gpufreq_enable_mmpll = 0;
	}

}

/*******************************
* late resume callback function
********************************/
void mt_gpufreq_late_resume(struct early_suspend *h)
{
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_late_resume\n");
	dprintk("g_cur_gpu_volt is %d, g_cur_gpu_freq is %d\n", g_cur_gpu_volt, g_cur_gpu_freq);

	mt_gpufreq_check_freq_and_set_pll();

	/* mt_gpufreq_state_set(1); */

	mt_gpu_clock_switch(g_cur_gpu_freq);
}

/************************************************
* frequency adjust interface for thermal protect
*************************************************/
/******************************************************
* parameter: target power
*******************************************************/
void mt_gpufreq_thermal_protect(unsigned int limited_power)
{
#if 1
	int i = 0;
	unsigned int limited_freq = 0;

	if (mt_gpufreqs_num == 0)
		return;

	if (limited_power == 0) {
		g_limited_max_id = 0;
	} else {
		g_limited_max_id = mt_gpufreqs_num - 1;

		for (i = 0; i < ARRAY_SIZE(mt_gpufreqs_golden_power); i++) {
			if (mt_gpufreqs_golden_power[i].gpufreq_power <= limited_power) {
				limited_freq = mt_gpufreqs_golden_power[i].gpufreq_khz;
				break;
			}
		}

		for (i = 0; i < mt_gpufreqs_num; i++) {
			if (mt_gpufreqs[i].gpufreq_khz <= limited_freq) {
				g_limited_max_id = i;
				break;
			}
		}
	}

	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
		    "limit frequency upper bound to id = %d, frequency = %d\n", g_limited_max_id,
		    mt_gpufreqs[g_limited_max_id].gpufreq_khz);
#endif
	return;
}
EXPORT_SYMBOL(mt_gpufreq_thermal_protect);

/************************************************
* return current GPU frequency
*************************************************/
unsigned int mt_gpufreq_get_cur_freq(void)
{
	dprintk("current GPU frequency is %d MHz\n", g_cur_gpu_freq / 1000);
	return g_cur_gpu_freq;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_freq);


/************************************************
* return current GPU voltage
*************************************************/
unsigned int mt_gpufreq_get_cur_volt(void)
{
	dprintk("current GPU volt is %d MHz\n", g_cur_gpu_volt);
	return g_cur_gpu_volt;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_volt);


/************************************************
* return current GPU loading
*************************************************/
unsigned int mt_gpufreq_get_cur_load(void)
{
	dprintk("current GPU load is %d\n", g_cur_gpu_load);
	return g_cur_gpu_load;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_load);


/************************************************
* set current GPU loading from pvrsrvkm.ko
*************************************************/
void mt_gpufreq_set_cur_load(unsigned int load)
{
	g_cur_gpu_load = load;
}
EXPORT_SYMBOL(mt_gpufreq_set_cur_load);



/************************************************
* return current GPU DVFS enabled/disabled
*************************************************/
bool mt_gpufreq_dvfs_disabled(void)
{
	dprintk("current GPU mt_gpufreq_pause is %d\n", mt_gpufreq_pause);
	return mt_gpufreq_pause;
}
EXPORT_SYMBOL(mt_gpufreq_dvfs_disabled);


#if 0
/******************************
* show current GPU DVFS stauts
*******************************/
static int mt_gpufreq_state_read(char *buf, char **start, off_t off, int count, int *eof,
				 void *data)
{
	int len = 0;
	char *p = buf;

	if (!mt_gpufreq_pause)
		p += sprintf(p, "GPU DVFS enabled\n");
	else
		p += sprintf(p, "GPU DVFS disabled\n");

	len = p - buf;
	return len;
}
#endif

/****************************************
* set GPU DVFS stauts by sysfs interface
*****************************************/
static ssize_t mt_gpufreq_state_write(struct file *file, const char *buffer, size_t count,
				      loff_t *data)
{
	int enabled = 0;

	/* If 3D not registered, it need to init timer and create kthread. */
	if ((mt_gpufreq_registered == false) && (mt_gpufreq_registered_statewrite == false)) {
		mt_gpufreq_pause = true;

		mt_gpufreqs = kzalloc((1) * sizeof(struct mt_gpufreq_info), GFP_KERNEL);
		mt_gpufreqs[0].gpufreq_khz = g_cur_gpu_freq;
		mt_gpufreqs[0].gpufreq_lower_bound = 0;
		mt_gpufreqs[0].gpufreq_upper_bound = 100;
		mt_gpufreqs[0].gpufreq_volt = 0;
		mt_gpufreqs[0].gpufreq_remap = 100;
		dprintk("freqs[0].gpufreq_khz = %u\n", mt_gpufreqs[0].gpufreq_khz);
		dprintk("freqs[0].gpufreq_lower_bound = %u\n", mt_gpufreqs[0].gpufreq_lower_bound);
		dprintk("freqs[0].gpufreq_upper_bound = %u\n", mt_gpufreqs[0].gpufreq_upper_bound);
		dprintk("freqs[0].gpufreq_volt = %u\n", mt_gpufreqs[0].gpufreq_volt);
		dprintk("freqs[0].gpufreq_remap = %u\n", mt_gpufreqs[0].gpufreq_remap);

		mt_gpufreq_registered_statewrite = true;
	}

	if (sscanf(buffer, "%d", &enabled) == 1) {
		if (enabled == 1) {
			mt_gpufreq_keep_max_frequency = false;
			mt_gpufreq_state_set(1);
		} else if (enabled == 0) {
			/* Keep MAX frequency when GPU DVFS disabled. */
			mt_gpufreq_keep_max_frequency = true;
			mt_gpufreq_target(g_gpufreq_max_id);
			mt_gpufreq_state_set(0);
		} else {
			xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
				    "bad argument!! argument should be \"1\" or \"0\"\n");
		}
	} else {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "bad argument!! argument should be \"1\" or \"0\"\n");
	}

	return count;
}

#if 0

/****************************
* show current limited power
*****************************/
static int mt_gpufreq_limited_power_read(char *buf, char **start, off_t off, int count, int *eof,
					 void *data)
{
	int len = 0;
	char *p = buf;

	p += sprintf(p, "g_limited_max_id = %d, frequency = %d\n", g_limited_max_id,
		     mt_gpufreqs[g_limited_max_id].gpufreq_khz);

	len = p - buf;
	return len;
}
#endif

/**********************************
* limited power for thermal protect
***********************************/
static ssize_t mt_gpufreq_limited_power_write(struct file *file, const char *buffer, size_t count,
					      loff_t *data)
{
	unsigned int power = 0;

	if (sscanf(buffer, "%u", &power) == 1) {
		mt_gpufreq_thermal_protect(power);
		return count;
	} else {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "bad argument!! please provide the maximum limited power\n");
	}

	return -EINVAL;
}

#if 0

/***************************
* show current debug status
****************************/
static int mt_gpufreq_debug_read(char *buf, char **start, off_t off, int count, int *eof,
				 void *data)
{
	int len = 0;
	char *p = buf;

	if (mt_gpufreq_debug)
		p += sprintf(p, "gpufreq debug enabled\n");
	else
		p += sprintf(p, "gpufreq debug disabled\n");

	len = p - buf;
	return len;
}
#endif

/***********************
* enable debug message
************************/
static ssize_t mt_gpufreq_debug_write(struct file *file, const char *buffer, size_t count,
				      loff_t *data)
{
	int debug = 0;

	if (sscanf(buffer, "%d", &debug) == 1) {
		if (debug == 0) {
			mt_gpufreq_debug = 0;
			return count;
		} else if (debug == 1) {
			mt_gpufreq_debug = 1;
			return count;
		} else {
			xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
				    "bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
		}
	} else {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
	}

	return -EINVAL;
}

#if 0
/********************
* show GPU OPP table
*********************/
static int mt_gpufreq_opp_dump_read(char *buf, char **start, off_t off, int count, int *eof,
				    void *data)
{
	int i = 0, j = 0, len = 0;
	char *p = buf;

	for (i = 0; i < mt_gpufreqs_num; i++) {
		p += sprintf(p, "[%d] ", i);
		p += sprintf(p, "freq = %d, ", mt_gpufreqs[i].gpufreq_khz);
		p += sprintf(p, "lower_bound = %d, ", mt_gpufreqs[i].gpufreq_lower_bound);
		p += sprintf(p, "upper_bound = %d, ", mt_gpufreqs[i].gpufreq_upper_bound);
		p += sprintf(p, "volt = %d, ", mt_gpufreqs[i].gpufreq_volt);
		p += sprintf(p, "remap = %d, ", mt_gpufreqs[i].gpufreq_remap);

		for (j = 0; j < ARRAY_SIZE(mt_gpufreqs_golden_power); j++) {
			if (mt_gpufreqs_golden_power[j].gpufreq_khz == mt_gpufreqs[i].gpufreq_khz) {
				p += sprintf(p, "power = %d\n",
					     mt_gpufreqs_golden_power[j].gpufreq_power);
				break;
			}
		}
	}

	len = p - buf;
	return len;
}
#endif

/***********************
* set specific frequency
************************/
static ssize_t mt_gpufreq_set_freq_write(struct file *file, const char *buffer, size_t count,
					 loff_t *data)
{
	int set_freq = 0;
	int set_volt = 0;

	if (sscanf(buffer, "%d %d", &set_freq, &set_volt) == 2) {
		mt_gpu_volt_switch(g_cur_gpu_volt, set_volt);
		/* set MMPLL speed as set_freq*3 */
		/* assume current PLL always as MMPLL */
		pll_set_freq(MMPLL, set_freq * 3);
		g_cur_gpu_freq = set_freq;

	} else {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "bad argument!! should be [set_freq set_volt] as [300000 1150]\n");
	}

	return -EINVAL;
}

static int mt_gpufreq_set_freq_show(struct seq_file* s, void* v)
{
	seq_printf(s, "g_cur_gpu_freq = %d\n", g_cur_gpu_freq);
	seq_printf(s, "g_cur_gpu_volt = %d\n", g_cur_gpu_volt);
	return 0;
}

static int mt_gpufreq_set_freq_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_set_freq_show, NULL);
}




#if 0
/***************************
* show current specific frequency status
****************************/
static int mt_gpufreq_fixed_frequency_read(char *buf, char **start, off_t off, int count, int *eof,
					   void *data)
{
	int len = 0;
	char *p = buf;

	if (mt_gpufreq_keep_specific_frequency)
		p += sprintf(p, "gpufreq fixed frequency enabled\n");
	else
		p += sprintf(p, "gpufreq fixed frequency disabled\n");

	len = p - buf;
	return len;
}
#endif

/***********************
* enable specific frequency
************************/
static ssize_t mt_gpufreq_fixed_frequency_write(struct file *file, const char *buffer, size_t count,
						loff_t *data)
{
	int enable = 0;
	int fixed_freq = 0;
	int fixed_volt = 0;

	if (sscanf(buffer, "%d %d %d", &enable, &fixed_freq, &fixed_volt) == 3) {
		if (enable == 0) {
			mt_gpufreq_keep_specific_frequency = false;
			return count;
		} else if (enable == 1) {
			mt_gpufreq_keep_specific_frequency = true;
			mt_gpufreq_fixed_frequency = fixed_freq;
			mt_gpufreq_fixed_voltage = fixed_volt;
			return count;
		} else {
			xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
				    "bad argument!! should be [enable fixed_freq fixed_volt]\n");
		}
	} else {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "bad argument!! should be [enable fixed_freq fixed_volt]\n");
	}

	return -EINVAL;
}

#if 0
/********************
* show variable dump
*********************/
static int mt_gpufreq_var_dump(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	char *p = buf;

	unsigned int clk_cfg_0 = 0;
	unsigned int clk_cfg_8 = 0;
	clk_cfg_0 = DRV_Reg32(CLK_CFG_0);
	clk_cfg_0 = (clk_cfg_0 & 0x000F0000) >> 16;
	clk_cfg_8 = DRV_Reg32(CLK_CFG_8);
	clk_cfg_8 = (clk_cfg_8 & 0x00000007);
	p += sprintf(p, "clk_cfg_0 = %d\n", clk_cfg_0);
	p += sprintf(p, "clk_cfg_8 = %d\n", clk_cfg_8);
	p += sprintf(p, "pmic_get_gpu_status_bit_info() = %d\n", pmic_get_gpu_status_bit_info());
	p += sprintf(p, "mt_gpufreq_enable_univpll = %d\n", mt_gpufreq_enable_univpll);
	p += sprintf(p, "mt_gpufreq_enable_mmpll = %d\n", mt_gpufreq_enable_mmpll);

	p += sprintf(p, "g_cur_freq_init_keep = %d\n", g_cur_freq_init_keep);
	p += sprintf(p, "g_cur_gpu_freq = %d\n", g_cur_gpu_freq);
	p += sprintf(p, "pll_get_freq(MMPLL) is %d\n", pll_get_freq(MMPLL));
	p += sprintf(p, "g_cur_gpu_volt = %d\n", g_cur_gpu_volt);

	p += sprintf(p, "g_freq_new_init_keep = %d, g_volt_new_init_keep = %d\n",
		     g_freq_new_init_keep, g_volt_new_init_keep);
	p += sprintf(p, "mt_gpufreq_registered = %d, mt_gpufreq_already_non_registered = %d\n",
		     mt_gpufreq_registered, mt_gpufreq_already_non_registered);

	len = p - buf;
	return len;
}

#endif

int mt_gpufreq_switch_handler(unsigned int load)
{
	int idx = 0;
	idx = mt_gpufreq_look_up(load);
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_switch_handler() idx is %d\n",
		    idx);

	mt_gpufreq_target(idx);

	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_switch_handler);


/*********************************
* mediatek gpufreq registration
**********************************/
int mt_gpufreq_register(struct mt_gpufreq_info *freqs, int num)
{
	if (mt_gpufreq_registered == false) {

		mt_gpufreq_registered = true;

#ifdef CONFIG_HAS_EARLYSUSPEND
		mt_gpufreq_early_suspend_handler.suspend = mt_gpufreq_early_suspend;
		mt_gpufreq_early_suspend_handler.resume = mt_gpufreq_late_resume;
		register_early_suspend(&mt_gpufreq_early_suspend_handler);
#endif

		/**********************
		* setup gpufreq table
		***********************/
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "setup gpufreqs table\n");

		mt_setup_gpufreqs_table(freqs, num);

		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "mediatek gpufreq registration done\n");
	} else {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "mediatek gpufreq already registered !\n");
	}
	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_register);

/*********************************
* mediatek gpufreq non registration
**********************************/
int mt_gpufreq_non_register(void)
{
	if (mt_gpufreq_already_non_registered == false) {
#ifdef CONFIG_HAS_EARLYSUSPEND
		mt_gpufreq_early_suspend_handler.suspend = mt_gpufreq_early_suspend;
		mt_gpufreq_early_suspend_handler.resume = mt_gpufreq_late_resume;
		register_early_suspend(&mt_gpufreq_early_suspend_handler);
#endif

		mt_gpufreq_already_non_registered = true;
		mt_setup_gpufreqs_default_power_table(1);

		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_non_register() done\n");
	} else {
		xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
			    "mt_gpufreq_non_register() already called !\n");
	}
	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_non_register);


/***************************
* show current GPU DVFS stauts
****************************/

static int mt_gpufreq_debug_show(struct seq_file *s, void *v)
{
	if (mt_gpufreq_debug)
		seq_puts(s, "gpufreq debug enabled\n");
	else
		seq_puts(s, "gpufreq debug disabled\n");

	return 0;
}

static int mt_gpufreq_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_debug_show, NULL);
}

/****************************
* show current limited GPU freq
*****************************/
static int mt_gpufreq_limited_power_show(struct seq_file *s, void *v)
{
	seq_printf(s, "g_limited_max_id = %d, frequency = %d\n", g_limited_max_id,
		   mt_gpufreqs[g_limited_max_id].gpufreq_khz);
	return 0;
}

static int mt_gpufreq_limited_power_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_limited_power_show, NULL);
}

static int mt_gpufreq_show(struct seq_file *s, void *v)
{
	if (mt_gpufreqs)
		seq_printf(s, "%d\n", mt_gpufreqs[g_limited_max_id].gpufreq_khz);
	return 0;
}

static int mt_gpufreq_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_show, NULL);
}

/******************************
* show current GPU DVFS stauts
*******************************/
static int mt_gpufreq_state_show(struct seq_file *s, void *v)
{
	if (!mt_gpufreq_pause)
		seq_puts(s, "GPU DVFS enabled\n");
	else
		seq_puts(s, "GPU DVFS disabled\n");

	return 0;
}

static int mt_gpufreq_state_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_state_show, NULL);
}


/********************
* show GPU OPP table
*********************/

static int mt_gpufreq_opp_dump_show(struct seq_file *s, void *v)
{
	int i = 0, j = 0;

	for (i = 0; i < mt_gpufreqs_num; i++) {
		seq_printf(s, "[%d] ", i);
		seq_printf(s, "freq = %d, ", mt_gpufreqs[i].gpufreq_khz);
		seq_printf(s, "lower_bound = %d, ", mt_gpufreqs[i].gpufreq_lower_bound);
		seq_printf(s, "upper_bound = %d, ", mt_gpufreqs[i].gpufreq_upper_bound);
		seq_printf(s, "volt = %d, ", mt_gpufreqs[i].gpufreq_volt);
		seq_printf(s, "remap = %d, ", mt_gpufreqs[i].gpufreq_remap);

		for (j = 0; j < ARRAY_SIZE(mt_gpufreqs_golden_power); j++) {
			if (mt_gpufreqs_golden_power[j].gpufreq_khz == mt_gpufreqs[i].gpufreq_khz) {
				seq_printf(s, "power = %d\n",
					   mt_gpufreqs_golden_power[j].gpufreq_power);
				break;
			}
		}
	}

	return 0;

}

static int mt_gpufreq_opp_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_opp_dump_show, NULL);
}



static int mt_gpufreq_fixed_frequency_show(struct seq_file *s, void *v)
{
	if (mt_gpufreq_keep_specific_frequency)
		seq_puts(s, "gpufreq fixed frequency enabled\n");
	else
		seq_puts(s, "gpufreq fixed frequency disabled\n");

	return 0;
}

static int mt_gpufreq_fixed_frequency_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_fixed_frequency_show, NULL);
}


/********************
* show variable dump
*********************/

static int mt_gpufreq_var_dump_show(struct seq_file *s, void *v)
{
	unsigned int clk_cfg_0 = 0;
	unsigned int clk_cfg_8 = 0;
	clk_cfg_0 = DRV_Reg32(CLK_CFG_0);
	clk_cfg_0 = (clk_cfg_0 & 0x000F0000) >> 16;
	clk_cfg_8 = DRV_Reg32(CLK_CFG_8);
	clk_cfg_8 = (clk_cfg_8 & 0x00000007);
	seq_printf(s, "clk_cfg_0 = %d\n", clk_cfg_0);
	seq_printf(s, "clk_cfg_8 = %d\n", clk_cfg_8);
	seq_printf(s, "pmic_get_gpu_status_bit_info() = %d\n", pmic_get_gpu_status_bit_info());
	seq_printf(s, "mt_gpufreq_enable_univpll = %d\n", mt_gpufreq_enable_univpll);
	seq_printf(s, "mt_gpufreq_enable_mmpll = %d\n", mt_gpufreq_enable_mmpll);

	seq_printf(s, "g_cur_freq_init_keep = %d\n", g_cur_freq_init_keep);
	seq_printf(s, "g_cur_gpu_freq = %d\n", g_cur_gpu_freq);
	seq_printf(s, "pll_get_freq(MMPLL) is %d\n", pll_get_freq(MMPLL));
	seq_printf(s, "g_cur_gpu_volt = %d\n", g_cur_gpu_volt);

	seq_printf(s, "g_freq_new_init_keep = %d, g_volt_new_init_keep = %d\n",
		   g_freq_new_init_keep, g_volt_new_init_keep);
	seq_printf(s, "mt_gpufreq_registered = %d, mt_gpufreq_already_non_registered = %d\n",
		   mt_gpufreq_registered, mt_gpufreq_already_non_registered);

	return 0;
}


static int mt_gpufreq_var_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_gpufreq_var_dump_show, NULL);
}


static const struct file_operations mt_gpufreq_debug_fops = {
	.owner = THIS_MODULE,
	.write = mt_gpufreq_debug_write,
	.open = mt_gpufreq_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations mt_gpufreq_limited_power_fops = {
	.owner = THIS_MODULE,
	.write = mt_gpufreq_limited_power_write,
	.open = mt_gpufreq_limited_power_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static const struct file_operations mt_gpufreq_state_fops = {
	.owner = THIS_MODULE,
	.write = mt_gpufreq_state_write,
	.open = mt_gpufreq_state_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static const struct file_operations mt_gpufreq_opp_dump_fops = {
	.owner = THIS_MODULE,
	.write = NULL,
	.open = mt_gpufreq_opp_dump_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static const struct file_operations mt_gpufreq_set_freq_fops = {
	.owner = THIS_MODULE,
	.write = mt_gpufreq_set_freq_write,
	.open = mt_gpufreq_set_freq_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static const struct file_operations mt_gpufreq_fixed_frequency_fops = {
	.owner = THIS_MODULE,
	.write = mt_gpufreq_fixed_frequency_write,
	.open = mt_gpufreq_fixed_frequency_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations mt_gpufreq_var_dump_fops = {
	.owner = THIS_MODULE,
	.write = NULL,
	.open = mt_gpufreq_var_dump_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations mt_gpufreq_fops = {
	.owner      = THIS_MODULE,
	.open       = mt_gpufreq_open,
	.read       = seq_read,
	.llseek     = seq_lseek,
	.release    = single_release,
};

/**********************************
* mediatek gpufreq initialization
***********************************/
static int __init mt_gpufreq_init(void)
{
	struct proc_dir_entry *mt_entry = NULL;
	struct proc_dir_entry *mt_gpufreq_dir = NULL;
	unsigned int clk_cfg_0 = 0;

	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_init\n");

	mt_gpufreq_dir = proc_mkdir("gpufreq", NULL);
	if (!mt_gpufreq_dir) {
		pr_err("[%s]: mkdir /proc/gpufreq failed\n", __func__);
	} else {
		mt_entry =
		    proc_create("gpufreq_debug", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
				&mt_gpufreq_debug_fops);
		if (!mt_entry) {
			pr_err("[%s]: mkdir /proc/gpufreq/gpufreq_debug failed\n", __func__);
		}

		mt_entry =
		    proc_create("gpufreq_limited_power", S_IRUGO | S_IWUSR | S_IWGRP,
				mt_gpufreq_dir, &mt_gpufreq_limited_power_fops);
		if (!mt_entry) {
			pr_err("[%s]: mkdir /proc/gpufreq/gpufreq_limited_power failed\n",
			       __func__);
		}

		mt_entry =
		    proc_create("gpufreq_state", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
				&mt_gpufreq_state_fops);
		if (!mt_entry) {
			pr_err("[%s]: mkdir /proc/gpufreq/gpufreq_state failed\n", __func__);
		}

		mt_entry =
		    proc_create("gpufreq_opp_dump", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
				&mt_gpufreq_opp_dump_fops);
		if (!mt_entry) {
			pr_err("[%s]: mkdir /proc/gpufreq/gpufreq_opp_dump failed\n", __func__);
		}

		mt_entry =
		    proc_create("gpufreq_set_freq", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
				&mt_gpufreq_set_freq_fops);
		if (!mt_entry) {
			pr_err("[%s]: mkdir /proc/gpufreq/gpufreq_set_freq failed\n", __func__);
		}

		mt_entry =
		    proc_create("gpufreq_fix_frequency", S_IRUGO | S_IWUSR | S_IWGRP,
				mt_gpufreq_dir, &mt_gpufreq_fixed_frequency_fops);
		if (!mt_entry) {
			pr_err("[%s]: mkdir /proc/gpufreq/gpufreq_fix_frequency failed\n",
			       __func__);
		}

		mt_entry =
		    proc_create("gpufreq_var_dump", S_IRUGO | S_IWUSR | S_IWGRP, mt_gpufreq_dir,
				&mt_gpufreq_var_dump_fops);
		if (!mt_entry) {
			pr_err("[%s]: mkdir /proc/gpufreq/gpufreq_var_dump failed\n", __func__);
		}

	mt_entry = proc_create("gpufreq",
				S_IRUGO | S_IWUSR | S_IWGRP,
				mt_gpufreq_dir,
				&mt_gpufreq_fops);
	if (!mt_entry)
		pr_err("[%s]: mkdir /proc/gpufreq/gpufreq failed\n", __func__);

	}

	clk_cfg_0 = DRV_Reg32(CLK_CFG_0);
	clk_cfg_0 = (clk_cfg_0 & 0x000F0000) >> 16;

	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS", "mt_gpufreq_init, clk_cfg_0 = %d\n",
		    clk_cfg_0);

	switch (clk_cfg_0) {
	case GPU_MMPLL_D3:
		g_cur_gpu_freq = GPU_MMPLL_D3_CLOCK;
		break;
	case GPU_MMPLL_D4:
		g_cur_gpu_freq = GPU_MMPLL_D4_CLOCK;
		break;
	case GPU_MMPLL_D5:
		g_cur_gpu_freq = GPU_MMPLL_D5_CLOCK;
		break;
	case GPU_MMPLL_D6:
		g_cur_gpu_freq = GPU_MMPLL_D6_CLOCK;
		break;
	case GPU_MMPLL_D7:
		g_cur_gpu_freq = GPU_MMPLL_D7_CLOCK;
		break;
	case GPU_UNIVPLL_D5:
		g_cur_gpu_freq = GPU_UNIVPLL_D5_CLOCK;
		break;
	case GPU_UNIVPLL1_D4:
		g_cur_gpu_freq = GPU_UNIVPLL1_D4_CLOCK;
		break;
	default:
		g_cur_gpu_freq = GPU_MMPLL_D3_CLOCK;
		break;
	}

	g_cur_freq_init_keep = g_cur_gpu_freq;
	xlog_printk(ANDROID_LOG_INFO, "Power/GPU_DVFS",
		    "mt_gpufreq_init, g_cur_freq_init_keep = %d\n", g_cur_freq_init_keep);
	mt_gpufreq_read_current_clock_from_reg();
	mt_gpu_vgpu_sel_1_15();

	return 0;
}

static void __exit mt_gpufreq_exit(void)
{

}
module_init(mt_gpufreq_init);
module_exit(mt_gpufreq_exit);

MODULE_DESCRIPTION("MediaTek GPU Frequency Scaling driver");
MODULE_LICENSE("GPL");
