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
#include <linux/cpu.h>
#include <linux/cpufreq.h>
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
#include <linux/version.h>
#include <linux/seq_file.h>

#include <asm/system.h>
#include <asm/uaccess.h>

#include "mach/mt_typedefs.h"
#include "mach/mt_clkmgr.h"
#include "mach/mt_cpufreq.h"
#include "mach/sync_write.h"

#include <mach/pmic_mt6320_sw.h>
/* #include <mach/upmu_common.h> */
#include <mach/upmu_hw.h>
#include <mach/mt_boot.h>

#include <asm/topology.h>
#include <mach/mt_pmic_wrap.h>

/**************************************************
* enable for DVFS random test
***************************************************/
/* #define MT_DVFS_RANDOM_TEST */

/**************************************************
* Add voltage offset for CPU Stress
***************************************************/
/* #define MT_CPUSTRESS_TEST */
/* #define MT_CPUSTRESS_TEST_DEBUG */

/**************************************************
* enable this option to adjust buck voltage
***************************************************/
#define MT_BUCK_ADJUST

/**************************************************
* enable cpufreq/latency_table sysfs node support
***************************************************/
#define MT_CPUFREQ_LATENCY_TABLE_SUPPORT

/**************************************************
* enable WFI/MAX Power consumption API support
***************************************************/
#define MT_POWER_API_SUPPORT


/***************************
* debug message
****************************/
#define dprintk(fmt, args...)							\
do {										\
	if (mt_cpufreq_debug) {							\
		pr_info("[Power/DVFS] "fmt, ##args);	\
	}									\
} while (0)

#define DVFS_INFO(prio, module, fmt, args...)		\
do {							\
	if (mt_cpufreq_debug) {				\
		pr_info("[Power/DVFS] "fmt, ##args);	\
	}						\
} while (0)

#define ARRAY_AND_SIZE(x)	(x), ARRAY_SIZE(x)

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend mt_cpufreq_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 200,
	.suspend = NULL,
	.resume = NULL,
};
#endif


#define DVFS_V0     (1250)	/* mV */
#define DVFS_V1     (1200)	/* mV */
#define DVFS_V2     (1150)	/* mV */
#define DVFS_V3     (1100)	/* mV */
#define DVFS_V4     (1050)	/* mV */
#define DVFS_V5     (1000)	/* mV */
#define DVFS_V6     (975)	/* mV */
#define DVFS_V7     (950)	/* mV */

#define DVFS_V1250  (1250)	/* mV */
#define DVFS_V1225  (1225)	/* mV */
#define DVFS_V1200  (1200)	/* mV */
#define DVFS_V1150  (1150)	/* mV */
#define DVFS_V1100  (1100)	/* mV */
#define DVFS_V1050  (1050)	/* mV */
#define DVFS_V1000  (1000)	/* mV */
#define DVFS_V0975  (975)	/* mV */
#define DVFS_V0950  (950)	/* mV */


/*
 * Frequencies should be integral number of MHz,
 * because PLL configuration does not set kHz correctly,
 * and the "same frequency" check fails.
 * Before switching to non-integral MHz values,
 * PLL configuration code should be fixed
 * */

#define DVFS_CA7_F0_0   (1404000)	/* KHz */
#define DVFS_CA7_F0     (1209000)	/* KHz */
#define DVFS_CA7_F0_2   (1001000)	/* KHz */
#define DVFS_CA7_F1     (1092000)	/* KHz */
#define DVFS_CA7_F2     (988000)	/* KHz */
#define DVFS_CA7_F3     (884000)	/* KHz */
#define DVFS_CA7_F4     (780000)	/* KHz */
#define DVFS_CA7_F5     (585000)	/* KHz */
#define DVFS_CA7_F6     (442000)	/* KHz */
#define DVFS_CA7_F7     (364000)	/* KHz */

#define DVFS_CA15_F0_0  (1807000)	/* KHz */
#define DVFS_CA15_F0_1  (1508000)	/* KHz */
#define DVFS_CA15_F0    (1404000)	/* KHz */
#define DVFS_CA15_F0_2  (1352000)	/* KHz */
#define DVFS_CA15_F1    (1261000)	/* KHz */
#define DVFS_CA15_F2    (1118000)	/* KHz */
#define DVFS_CA15_F3    (975000)	/* KHz */
#define DVFS_CA15_F4    (806000)	/* KHz */
#define DVFS_CA15_F5    (637000)	/* KHz */
#define DVFS_CA15_F6    (507000)	/* KHz */
#define DVFS_CA15_F7    (390000)	/* KHz */


/*****************************************
* PMIC settle time, should not be changed
******************************************/
#define PMIC_SETTLE_TIME (40)	/* us */

/*****************************************
* MT6320 DVS DOWN settle time, should not be changed
******************************************/
#define DVS_DOWN_SETTLE_TIME (120)	/* us */

/***********************************************
* RMAP DOWN TIMES to postpone frequency degrade
************************************************/
#define RAMP_DOWN_TIMES (2)

/**********************************
* Available Clock Source for CPU
***********************************/
#define TOP_CKMUXSEL_CLKSQ   0x0
#define TOP_CKMUXSEL_ARMPLL  0x1
#define TOP_CKMUXSEL_ARMPLL2 0x1
#define TOP_CKMUXSEL_MAINPLL 0x2
#define TOP_CKMUXSEL_UNIVPLL 0x3

/*******************
* Cluster CA7/CA15
********************/
#define MAX_CLUSTERS	2
#define CA7_CLUSTER     0
#define CA15_CLUSTER    1

/*******************
* Register Define
********************/
#define CLK_MISC_CFG_2  (TOPRGU_BASE + 0x0160)


/**************************************************
* enable DVFS function
***************************************************/
static int g_dvfs_disable_count;

static unsigned int g_cur_freq[MAX_CLUSTERS];
static unsigned int g_cur_cpufreq_volt[MAX_CLUSTERS];
static unsigned int g_limited_max_ncpu[MAX_CLUSTERS + 1] = { 2, 2, 4 };

static unsigned int g_limited_max_freq[MAX_CLUSTERS];
static unsigned int g_limited_min_freq[MAX_CLUSTERS];
static unsigned int g_cpufreq_get_ptp_level[MAX_CLUSTERS + 1] = { 0, 0, 0 };

static unsigned int g_max_freq_by_ptp[MAX_CLUSTERS];
static unsigned int g_thermal_opp_num;
static unsigned int g_top_ckmuxsel;
static int g_top_pll;
static CHIP_SW_VER g_chip_sw_ver;
static U32 g_vcore;

/* static int g_ramp_down_count[MAX_CLUSTERS] = {0, 0}; */

static bool mt_cpufreq_debug;
static bool mt_cpufreq_ready;
static bool mt_cpufreq_pause;
static bool mt_cpufreq_ptpod_disable;
static bool mt_cpufreq_ptpod_voltage_down;
static bool mt_cpufreq_keep_max_freq_early_suspend = true;

/* pmic volt by PTP-OD */
static unsigned int mt_cpufreq_pmic_volt[MAX_CLUSTERS][8] = { {0}, {0} };

static DEFINE_SPINLOCK(mt_cpufreq_lock);

#ifdef PMIC_VOLTAGE_CHECK
static U32 g_vca7;
static U32 g_vca15;
#endif

#ifdef MT_CPUSTRESS_TEST
static int g_ca7_delta_v;
static int g_ca15_delta_v;
#endif

/* temp for experimet */
static int g_thermal_type = 1;	/* 0:default 1:budget 2: budget + rq */


/***************************
* Operate Point Definition
****************************/
#define OP(khz, volt)       \
{                           \
	.cpufreq_khz = khz,     \
	.cpufreq_volt = volt,   \
}

/***************************
* Power Info Definition
****************************/
struct mt_cpu_freq_info {
	unsigned int cpufreq_khz;
	unsigned int cpufreq_volt;
};

#ifdef MT_POWER_API_SUPPORT
struct mt_percpu_power_info {
	unsigned int cpufreq_khz;
	unsigned int cpufreq_power_wfi;
	unsigned int cpufreq_power_max;
};
#endif

/***************************
* MT8135 E1 DVFS Table
****************************/

static struct mt_cpu_freq_info mt8135_freqs_ca7_e1[] = {
	OP(DVFS_CA7_F0, DVFS_V1250),
	OP(DVFS_CA7_F1, DVFS_V1200),
	OP(DVFS_CA7_F2, DVFS_V1150),
	OP(DVFS_CA7_F3, DVFS_V1100),
	OP(DVFS_CA7_F4, DVFS_V1050),
	OP(DVFS_CA7_F5, DVFS_V1000),
	OP(DVFS_CA7_F6, DVFS_V0975),
	OP(DVFS_CA7_F7, DVFS_V0950),
};

static struct mt_cpu_freq_info mt8135_freqs_ca15_e1[] = {
	OP(DVFS_CA15_F0, DVFS_V1250),
	OP(DVFS_CA15_F1, DVFS_V1200),
	OP(DVFS_CA15_F2, DVFS_V1150),
	OP(DVFS_CA15_F3, DVFS_V1100),
	OP(DVFS_CA15_F4, DVFS_V1050),
	OP(DVFS_CA15_F5, DVFS_V1000),
	OP(DVFS_CA15_F6, DVFS_V0975),
	OP(DVFS_CA15_F7, DVFS_V0950),
};

static struct mt_cpu_freq_info *mt8135_freqs_e1[MAX_CLUSTERS] = {
	mt8135_freqs_ca7_e1,
	mt8135_freqs_ca15_e1,
};

static struct mt_cpu_freq_info mt8135_freqs_ca7_e1_2[] = {
	OP(DVFS_CA7_F0_2, DVFS_V1250),
	OP(DVFS_CA7_F2, DVFS_V1150),
	OP(DVFS_CA7_F3, DVFS_V1100),
	OP(DVFS_CA7_F4, DVFS_V1050),
	OP(DVFS_CA7_F5, DVFS_V1000),
	OP(DVFS_CA7_F6, DVFS_V0975),
	OP(DVFS_CA7_F7, DVFS_V0950),
};

static struct mt_cpu_freq_info mt8135_freqs_ca15_e1_2[] = {
	OP(DVFS_CA15_F0_2, DVFS_V1250),
	OP(DVFS_CA15_F2, DVFS_V1150),
	OP(DVFS_CA15_F3, DVFS_V1100),
	OP(DVFS_CA15_F4, DVFS_V1050),
	OP(DVFS_CA15_F5, DVFS_V1000),
	OP(DVFS_CA15_F6, DVFS_V0975),
	OP(DVFS_CA15_F7, DVFS_V0950),
};

static struct mt_cpu_freq_info *mt8135_freqs_e1_2[MAX_CLUSTERS] = {
	mt8135_freqs_ca7_e1_2,
	mt8135_freqs_ca15_e1_2,
};


static struct mt_cpu_freq_info mt8135_freqs_ca7_e1_1[] = {
	OP(DVFS_CA7_F0, DVFS_V1250),
	OP(DVFS_CA7_F1, DVFS_V1200),
	OP(DVFS_CA7_F2, DVFS_V1150),
	OP(DVFS_CA7_F3, DVFS_V1100),
	OP(DVFS_CA7_F4, DVFS_V1050),
	OP(DVFS_CA7_F5, DVFS_V1000),
	OP(DVFS_CA7_F6, DVFS_V0975),
	OP(DVFS_CA7_F7, DVFS_V0950),
};

static struct mt_cpu_freq_info mt8135_freqs_ca15_e1_1[] = {
	OP(DVFS_CA15_F0_1, DVFS_V1250),
	OP(DVFS_CA15_F1, DVFS_V1200),
	OP(DVFS_CA15_F2, DVFS_V1150),
	OP(DVFS_CA15_F3, DVFS_V1100),
	OP(DVFS_CA15_F4, DVFS_V1050),
	OP(DVFS_CA15_F5, DVFS_V1000),
	OP(DVFS_CA15_F6, DVFS_V0975),
	OP(DVFS_CA15_F7, DVFS_V0950),
};

static struct mt_cpu_freq_info *mt8135_freqs_e1_1[MAX_CLUSTERS] = {
	mt8135_freqs_ca7_e1_1,
	mt8135_freqs_ca15_e1_1,
};

static struct mt_cpu_freq_info mt8135_freqs_ca7_e1_0[] = {
	OP(DVFS_CA7_F0, DVFS_V1250),
	OP(DVFS_CA7_F1, DVFS_V1200),
	OP(DVFS_CA7_F2, DVFS_V1150),
	OP(DVFS_CA7_F3, DVFS_V1100),
	OP(DVFS_CA7_F4, DVFS_V1050),
	OP(DVFS_CA7_F5, DVFS_V1000),
	OP(DVFS_CA7_F6, DVFS_V0975),
	OP(DVFS_CA7_F7, DVFS_V0950),
};

static struct mt_cpu_freq_info mt8135_freqs_ca15_e1_0[] = {
	OP(DVFS_CA15_F0_0, DVFS_V1250),
	OP(DVFS_CA15_F0_1, DVFS_V1225),
	OP(DVFS_CA15_F1, DVFS_V1200),
	OP(DVFS_CA15_F2, DVFS_V1150),
	OP(DVFS_CA15_F3, DVFS_V1100),
	OP(DVFS_CA15_F4, DVFS_V1050),
	OP(DVFS_CA15_F5, DVFS_V1000),
	OP(DVFS_CA15_F7, DVFS_V0950),
};

static struct mt_cpu_freq_info *mt8135_freqs_e1_0[MAX_CLUSTERS] = {
	mt8135_freqs_ca7_e1_0,
	mt8135_freqs_ca15_e1_0,
};


static unsigned int mt_cpu_freqs_num[MAX_CLUSTERS];
static struct mt_cpu_freq_info *mt_cpu_freqs[MAX_CLUSTERS];
static struct cpufreq_frequency_table *mt_cpu_freqs_table[MAX_CLUSTERS];
static struct mt_cpu_power_info *mt_cpu_power;

#ifdef MT_POWER_API_SUPPORT
static struct mt_percpu_power_info *mt_percpu_power[MAX_CLUSTERS] = { NULL, NULL };
#endif

/******************************
* Internal Function Declaration
*******************************/
static void mt_cpufreq_volt_set(unsigned int target_volt, unsigned int cur_cluster);


/***********************************************
* MT8135 E1 CA7 Raw Data: 1.2Ghz @ 1.15V @ TT 125C
************************************************/
#define PERFROMANCE_RATIO (16)

#define T_105           (105)	/* Temperature 105C */
#define T_60            (60)	/* Temperature 60C */
#define T_25            (25)	/* Temperature 25C */

#define P_CA7_MCU_L     (98)	/* MCU Leakage Power */
#define P_CA7_MCU_T     (429)	/* MCU Total Power */
#define P_CA7_L         (38)	/* CA7 Leakage Power */
#define P_CA7_D         (140)
#define P_CA7_T         (178)	/* Single CA7 Core Power */

#define P_CA7_MCL99_105C_L  (75)	/* MCL99 Leakage Power @ 105C */
#define P_CA7_MCL99_25C_L   (4)	/* MCL99 Leakage Power @ 25C */
#define P_CA7_MCL50_105C_L  (37)	/* MCL50 Leakage Power @ 105C */
#define P_CA7_MCL50_25C_L   (2)	/* MCL50 Leakage Power @ 25C */

/* MCU dynamic power except of CA7 cores */
#define P_CA7_MCU_D ((P_CA7_MCU_T - P_CA7_MCU_L) - 2 * (P_CA7_T - P_CA7_L))

/* Total leakage at T_60 */
#define P_CA7_TOTAL_CORE_L	\
(P_CA7_MCL99_25C_L + (((((P_CA7_MCL99_105C_L - P_CA7_MCL99_25C_L) * 100) /	\
(T_105 - T_25)) * (T_60 - T_25)) / 100))

/* 1 core leakage at T_60 */
#define P_CA7_EACH_CORE_L	\
((P_CA7_TOTAL_CORE_L * ((P_CA7_L * 1000) / P_CA7_MCU_L)) / 1000)

#define P_CA7_D_1_CORE (P_CA7_D * 1)	/* CA7 dynamic power for 1 cores turned on */
#define P_CA7_D_2_CORE (P_CA7_D * 2)	/* CA7 dynamic power for 2 cores turned on */

#define A_CA7_1_CORE (P_CA7_MCU_D + P_CA7_D_1_CORE)	/* MCU dynamic power for 1 cores turned on */
#define A_CA7_2_CORE (P_CA7_MCU_D + P_CA7_D_2_CORE)	/* MCU dynamic power for 2 cores turned on */

#define P_CA7_WFI_L (35)
#define P_CA7_WFI_D (1)

/***********************************************
* MT8135 E1 CA15 Raw Data: 1.5Ghz @ 1.15V @ TT 125C
************************************************/
#define P_CA15_MCU_L     (632)	/* MCU Leakage Power */
#define P_CA15_MCU_T     (3157)	/* MCU Total Power */
#define P_CA15_L         (226)	/* CA15 Leakage Power */
#define P_CA15_D         (1220)
#define P_CA15_T         (1446)	/* Single CA7 Core Power */

#define P_CA15_MCL99_105C_L  (444)	/* MCL99 Leakage Power @ 105C */
#define P_CA15_MCL99_25C_L   (23)	/* MCL99 Leakage Power @ 25C */
#define P_CA15_MCL50_105C_L  (221)	/* MCL50 Leakage Power @ 105C */
#define P_CA15_MCL50_25C_L   (12)	/* MCL50 Leakage Power @ 25C */

/* MCU dynamic power except of CA7 cores */
#define P_CA15_MCU_D ((P_CA15_MCU_T - P_CA15_MCU_L) - 2 * (P_CA15_T - P_CA15_L))

/* Total leakage at T_60 */
#define P_CA15_TOTAL_CORE_L	\
(P_CA15_MCL99_25C_L + (((((P_CA15_MCL99_105C_L - P_CA15_MCL99_25C_L) * 100) /	\
(T_105 - T_25)) * (T_60 - T_25)) / 100))

/* 1 core leakage at T_60 */
#define P_CA15_EACH_CORE_L  ((P_CA15_TOTAL_CORE_L * ((P_CA15_L * 1000) / P_CA15_MCU_L)) / 1000)

#define P_CA15_D_1_CORE (P_CA15_D * 1)	/* CA7 dynamic power for 1 cores turned on */
#define P_CA15_D_2_CORE (P_CA15_D * 2)	/* CA7 dynamic power for 2 cores turned on */

#define A_CA15_1_CORE (P_CA15_MCU_D + P_CA15_D_1_CORE)	/* MCU dynamic power for 1 cores turned on */
#define A_CA15_2_CORE (P_CA15_MCU_D + P_CA15_D_2_CORE)	/* MCU dynamic power for 2 cores turned on */

#define P_CA15_WFI_L (226)
#define P_CA15_WFI_D (8)


static int cpu_to_cluster(int cpu)
{
	return topology_physical_package_id(cpu);
}

/* Check the mapping for DVFS voltage and pmic wrap voltage */
/* Need sync with mt_cpufreq_volt_set(), mt_cpufreq_pdrv_probe() */
static unsigned int mt_cpufreq_volt_to_pmic_wrap(unsigned int target_volt, unsigned int cur_cluster)
{
	unsigned int idx = 0;

	if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 1) {
		if (cur_cluster == CA7_CLUSTER) {	/* CA7 */
			switch (target_volt) {
			case DVFS_V1250:
				idx = 0;	/* spm_dvfs_ctrl_volt(0); */
				break;
			case DVFS_V1200:
				idx = 1;	/* spm_dvfs_ctrl_volt(1); */
				break;
			case DVFS_V1150:
				idx = 2;	/* spm_dvfs_ctrl_volt(2); */
				break;
			case DVFS_V1100:
				idx = 3;	/* spm_dvfs_ctrl_volt(3); */
				break;
			case DVFS_V1050:
				idx = 4;	/* spm_dvfs_ctrl_volt(4); */
				break;
			case DVFS_V1000:
				idx = 5;	/* spm_dvfs_ctrl_volt(5); */
				break;
			case DVFS_V0975:
				idx = 6;	/* spm_dvfs_ctrl_volt(6); */
				break;
			case DVFS_V0950:
				idx = 7;	/* spm_dvfs_ctrl_volt(7); */
				break;
			default:
				break;
			}
		} else {
			switch (target_volt) {
			case DVFS_V1250:
				idx = 0;	/* spm_dvfs_ctrl_volt(0); */
				break;
			case DVFS_V1225:
				idx = 1;	/* spm_dvfs_ctrl_volt(1); */
				break;
			case DVFS_V1200:
				idx = 2;	/* spm_dvfs_ctrl_volt(2); */
				break;
			case DVFS_V1150:
				idx = 3;	/* spm_dvfs_ctrl_volt(3); */
				break;
			case DVFS_V1100:
				idx = 4;	/* spm_dvfs_ctrl_volt(4); */
				break;
			case DVFS_V1050:
				idx = 5;	/* spm_dvfs_ctrl_volt(5); */
				break;
			case DVFS_V1000:
				idx = 6;	/* spm_dvfs_ctrl_volt(6); */
				break;
			case DVFS_V0950:
				idx = 7;	/* spm_dvfs_ctrl_volt(7); */
				break;
			default:
				break;
			}
		}
	} else if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 3) {
		if (cur_cluster == CA7_CLUSTER) {	/* CA7 */
			switch (target_volt) {
			case DVFS_V1250:
				idx = 0;	/* spm_dvfs_ctrl_volt(0); */
				break;
			case DVFS_V1150:
				idx = 1;	/* spm_dvfs_ctrl_volt(1); */
				break;
			case DVFS_V1100:
				idx = 2;	/* spm_dvfs_ctrl_volt(2); */
				break;
			case DVFS_V1050:
				idx = 3;	/* spm_dvfs_ctrl_volt(3); */
				break;
			case DVFS_V1000:
				idx = 4;	/* spm_dvfs_ctrl_volt(4); */
				break;
			case DVFS_V0975:
				idx = 5;	/* spm_dvfs_ctrl_volt(5); */
				break;
			case DVFS_V0950:
				idx = 6;	/* spm_dvfs_ctrl_volt(6); */
				break;
			default:
				break;
			}
		} else {
			switch (target_volt) {
			case DVFS_V1250:
				idx = 0;	/* spm_dvfs_ctrl_volt(0); */
				break;
			case DVFS_V1150:
				idx = 1;	/* spm_dvfs_ctrl_volt(1); */
				break;
			case DVFS_V1100:
				idx = 2;	/* spm_dvfs_ctrl_volt(2); */
				break;
			case DVFS_V1050:
				idx = 3;	/* spm_dvfs_ctrl_volt(3); */
				break;
			case DVFS_V1000:
				idx = 4;	/* spm_dvfs_ctrl_volt(4); */
				break;
			case DVFS_V0975:
				idx = 5;	/* spm_dvfs_ctrl_volt(5); */
				break;
			case DVFS_V0950:
				idx = 6;	/* spm_dvfs_ctrl_volt(6); */
				break;
			default:
				break;
			}
		}
	} else {
		if (cur_cluster == CA7_CLUSTER) {	/* CA7 */
			switch (target_volt) {
			case DVFS_V1250:
				idx = 0;	/* spm_dvfs_ctrl_volt(0); */
				break;
			case DVFS_V1200:
				idx = 1;	/* spm_dvfs_ctrl_volt(1); */
				break;
			case DVFS_V1150:
				idx = 2;	/* spm_dvfs_ctrl_volt(2); */
				break;
			case DVFS_V1100:
				idx = 3;	/* spm_dvfs_ctrl_volt(3); */
				break;
			case DVFS_V1050:
				idx = 4;	/* spm_dvfs_ctrl_volt(4); */
				break;
			case DVFS_V1000:
				idx = 5;	/* spm_dvfs_ctrl_volt(5); */
				break;
			case DVFS_V0975:
				idx = 6;	/* spm_dvfs_ctrl_volt(6); */
				break;
			case DVFS_V0950:
				idx = 7;	/* spm_dvfs_ctrl_volt(7); */
				break;
			default:
				break;
			}
		} else {
			switch (target_volt) {
			case DVFS_V1250:
				idx = 0;	/* spm_dvfs_ctrl_volt(0); */
				break;
			case DVFS_V1200:
				idx = 1;	/* spm_dvfs_ctrl_volt(1); */
				break;
			case DVFS_V1150:
				idx = 2;	/* spm_dvfs_ctrl_volt(2); */
				break;
			case DVFS_V1100:
				idx = 3;	/* spm_dvfs_ctrl_volt(3); */
				break;
			case DVFS_V1050:
				idx = 4;	/* spm_dvfs_ctrl_volt(4); */
				break;
			case DVFS_V1000:
				idx = 5;	/* spm_dvfs_ctrl_volt(5); */
				break;
			case DVFS_V0975:
				idx = 6;	/* spm_dvfs_ctrl_volt(6); */
				break;
			case DVFS_V0950:
				idx = 7;	/* spm_dvfs_ctrl_volt(7); */
				break;
			default:
				break;
			}
		}

	}

	dprintk("mt_cpufreq_volt_to_pmic_wrap: cluster = %d, current pmic wrap idx = %d\n",
		cur_cluster, idx);
	return idx;
}


/* Set voltage because PTP-OD modified voltage table by PMIC wrapper */
unsigned int mt_cpufreq_voltage_set_by_ptpod(unsigned int pmic_volt[], unsigned int array_size,
					     unsigned int cur_cluster)
{
	int i, idx;
	unsigned long flags;

	if (array_size > (sizeof(mt_cpufreq_pmic_volt[cur_cluster]) / 4)) {
		dprintk
		    ("cluster: %d, mt_cpufreq_voltage_set_by_ptpod: ERROR!array_size is invalide, array_size = %d\n",
		     cur_cluster, array_size);
	}

	spin_lock_irqsave(&mt_cpufreq_lock, flags);

	/* Check the mapping for DVFS voltage to pmic wrap voltage */
	idx = mt_cpufreq_volt_to_pmic_wrap(g_cur_cpufreq_volt[cur_cluster], cur_cluster);
	dprintk("Previous mt_cpufreq_pmic_volt[%d][%d] = %x\n", cur_cluster, idx,
		mt_cpufreq_pmic_volt[cur_cluster][idx]);

	/* Check PTP-OD modify voltage UP or DOWN, because PTPOD maybe not change voltage all UP or DOWN. */
	if (mt_cpufreq_pmic_volt[cur_cluster][idx] > pmic_volt[idx]) {
		mt_cpufreq_ptpod_voltage_down = true;
	} else if (mt_cpufreq_pmic_volt[cur_cluster][idx] < pmic_volt[idx]) {
		mt_cpufreq_ptpod_voltage_down = false;
	} else {
		for (i = 0; i < array_size; i++) {
			mt_cpufreq_pmic_volt[cur_cluster][i] = pmic_volt[i];
			dprintk("mt_cpufreq_pmic_volt[%d][%d] = %x\n", cur_cluster, i,
				mt_cpufreq_pmic_volt[cur_cluster][i]);
		}

		dprintk
		    ("mt_cpufreq_voltage_set_by_ptpod: pmic wrap voltage by PTP-OD was the same as previous!\n");
		spin_unlock_irqrestore(&mt_cpufreq_lock, flags);
		return 0;
	}

	for (i = 0; i < array_size; i++) {
		mt_cpufreq_pmic_volt[cur_cluster][i] = pmic_volt[i];
		dprintk("mt_cpufreq_pmic_volt[%d][%d] = %x\n", cur_cluster, i,
			mt_cpufreq_pmic_volt[cur_cluster][i]);
	}

	dprintk("mt_cpufreq_voltage_set_by_ptpod: Set voltage directly by PTP-OD request!");
	dprintk("cluster[%d]: mt_cpufreq_ptpod_voltage_down = %d\n", cur_cluster, mt_cpufreq_ptpod_voltage_down);

	/* If voltage down, need to consider DVS DOWN SW workaround. */
	if (mt_cpufreq_ptpod_voltage_down == true) {
		mt_cpufreq_volt_set(g_cur_cpufreq_volt[cur_cluster], cur_cluster);
	} else {
		mt_cpufreq_volt_set(g_cur_cpufreq_volt[cur_cluster], cur_cluster);
	}
	spin_unlock_irqrestore(&mt_cpufreq_lock, flags);

	return 0;
}
EXPORT_SYMBOL(mt_cpufreq_voltage_set_by_ptpod);


/* Look for MAX frequency in number of DVS. */
unsigned int mt_cpufreq_max_frequency_by_DVS(unsigned int num, unsigned int cur_cluster)
{
	int voltage_change_num = 0;
	int i = 0;

	/* Assume mt8135_freqs_e1 voltage will be put in order, and freq will be put from high to low. */
	if (num == voltage_change_num) {
		DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
			    "Cluster= %d, PTPOD0:num = %d, frequency= %d\n", cur_cluster, num,
			    mt_cpu_freqs[cur_cluster][0].cpufreq_khz);
		return mt_cpu_freqs[cur_cluster][0].cpufreq_khz;
	}

	for (i = 1; i < mt_cpu_freqs_num[cur_cluster]; i++) {
		if (mt_cpu_freqs[cur_cluster][i].cpufreq_volt !=
		    mt_cpu_freqs[cur_cluster][i - 1].cpufreq_volt)
			voltage_change_num++;

		if (num == voltage_change_num) {
			DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
				    "Cluster= %d, PTPOD1:num = %d, frequency= %d\n", cur_cluster,
				    num, mt_cpu_freqs[cur_cluster][i].cpufreq_khz);
			return mt_cpu_freqs[cur_cluster][i].cpufreq_khz;
		}
	}

	DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
		    "Cluster= %d, PTPOD2:num = %d, NOT found! return 0!\n", cur_cluster, num);

	return 0;
}
EXPORT_SYMBOL(mt_cpufreq_max_frequency_by_DVS);


static void mt_setup_power_table(void)
{
	int i = 0, j = 0, index = 0, multi[MAX_CLUSTERS] = { 0 }, p_dynamic[MAX_CLUSTERS] = {
	0}, p_leakage[MAX_CLUSTERS] = {
	0}, freq_ratio[MAX_CLUSTERS] = {
	0}, volt_square_ratio[MAX_CLUSTERS] = {
	0};
	struct mt_cpu_power_info temp_power_info;

	dprintk("P_CA7_MCU_D = %d, P_CA15_MCU_D = %d\n", P_CA7_MCU_D, P_CA15_MCU_D);

	dprintk
	    ("P_CA7_D_1_CORE = %d, P_CA7_D_2_CORE = %d, P_CA15_D_1_CORE = %d, P_CA15_D_2_CORE = %d\n",
	     P_CA7_D_1_CORE, P_CA7_D_2_CORE, P_CA15_D_1_CORE, P_CA15_D_2_CORE);

	dprintk
	    ("P_CA7_TOTAL_CORE_L = %d, P_CA7_EACH_CORE_L = %d, P_CA15_TOTAL_CORE_L = %d, P_CA15_EACH_CORE_L = %d\n",
	     P_CA7_TOTAL_CORE_L, P_CA7_EACH_CORE_L, P_CA15_TOTAL_CORE_L, P_CA15_EACH_CORE_L);

	dprintk("A_CA7_1_CORE = %d, A_CA7_2_CORE = %d, A_CA15_1_CORE = %d, A_CA15_2_CORE = %d\n",
		A_CA7_1_CORE, A_CA7_2_CORE, A_CA15_1_CORE, A_CA15_2_CORE);

	g_thermal_opp_num =
	    mt_cpu_freqs_num[CA7_CLUSTER] * mt_cpu_freqs_num[CA15_CLUSTER] * 4 +
	    mt_cpu_freqs_num[CA7_CLUSTER] * 2;

	mt_cpu_power = kzalloc(g_thermal_opp_num * sizeof(struct mt_cpu_power_info), GFP_KERNEL);

	for (i = 0; i < mt_cpu_freqs_num[CA7_CLUSTER]; i++) {

		volt_square_ratio[CA7_CLUSTER] =
		    (((mt_cpu_freqs[CA7_CLUSTER][i].cpufreq_volt * 100) / 1150) *
		     ((mt_cpu_freqs[CA7_CLUSTER][i].cpufreq_volt * 100) / 1150)) / 100;
		freq_ratio[CA7_CLUSTER] = (mt_cpu_freqs[CA7_CLUSTER][i].cpufreq_khz / 1200);
		dprintk("freq_ratio = %d, volt_square_ratio %d\n", freq_ratio[CA7_CLUSTER],
			volt_square_ratio[CA7_CLUSTER]);

		multi[CA7_CLUSTER] =
		    ((mt_cpu_freqs[CA7_CLUSTER][i].cpufreq_volt * 100) / 1150) *
		    ((mt_cpu_freqs[CA7_CLUSTER][i].cpufreq_volt * 100) / 1150) *
		    ((mt_cpu_freqs[CA7_CLUSTER][i].cpufreq_volt * 100) / 1150);

		/* 1 core */
		p_dynamic[CA7_CLUSTER] =
		    (((A_CA7_1_CORE * freq_ratio[CA7_CLUSTER]) / 1000) *
		     volt_square_ratio[CA7_CLUSTER]) / 100;
		p_leakage[CA7_CLUSTER] =
		    ((P_CA7_TOTAL_CORE_L * (multi[CA7_CLUSTER])) / (100 * 100 * 100)) -
		    1 * P_CA7_EACH_CORE_L;
		dprintk("p_dynamic[CA7_CLUSTER] = %d, p_leakage[CA7_CLUSTER] = %d\n",
			p_dynamic[CA7_CLUSTER], p_leakage[CA7_CLUSTER]);

		/* only CA7 1 core */
		mt_cpu_power[index].cpufreq_ncpu_ca7 = 1;
		mt_cpu_power[index].cpufreq_ncpu_ca15 = 0;
		mt_cpu_power[index].cpufreq_ncpu = 1;
		mt_cpu_power[index].cpufreq_khz_ca7 = mt_cpu_freqs[CA7_CLUSTER][i].cpufreq_khz;
		mt_cpu_power[index].cpufreq_khz_ca15 = 0;
		mt_cpu_power[index].cpufreq_khz =
		    mt_cpu_power[index].cpufreq_khz_ca7 * mt_cpu_power[index].cpufreq_ncpu_ca7;
		mt_cpu_power[index].cpufreq_power_ca7 =
		    p_dynamic[CA7_CLUSTER] + p_leakage[CA7_CLUSTER];
		mt_cpu_power[index].cpufreq_power_ca15 = 0;
		mt_cpu_power[index].cpufreq_power = mt_cpu_power[index].cpufreq_power_ca7;
		dprintk
		    ("CA7 mt_cpu_power[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
		     index, mt_cpu_power[index].cpufreq_ncpu_ca7,
		     mt_cpu_power[index].cpufreq_khz_ca7, mt_cpu_power[index].cpufreq_power_ca7);
		dprintk
		    ("CA15 mt_cpu_power[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
		     index, mt_cpu_power[index].cpufreq_ncpu_ca15,
		     mt_cpu_power[index].cpufreq_khz_ca15, mt_cpu_power[index].cpufreq_power_ca15);
		dprintk
		    ("mt_cpu_power[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
		     index, mt_cpu_power[index].cpufreq_ncpu, mt_cpu_power[index].cpufreq_khz,
		     mt_cpu_power[index].cpufreq_power);
		index++;

		for (j = 0; j < mt_cpu_freqs_num[CA15_CLUSTER]; j++) {
			/* CA7 1 core + CA15 */
			mt_cpu_power[index].cpufreq_ncpu_ca7 = 1;
			mt_cpu_power[index].cpufreq_khz_ca7 =
			    mt_cpu_freqs[CA7_CLUSTER][i].cpufreq_khz;
			mt_cpu_power[index].cpufreq_power_ca7 =
			    p_dynamic[CA7_CLUSTER] + p_leakage[CA7_CLUSTER];
			dprintk
			    ("CA7 mt_cpu_power[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
			     index, mt_cpu_power[index].cpufreq_ncpu_ca7,
			     mt_cpu_power[index].cpufreq_khz_ca7,
			     mt_cpu_power[index].cpufreq_power_ca7);


			volt_square_ratio[CA15_CLUSTER] =
			    (((mt_cpu_freqs[CA15_CLUSTER][j].cpufreq_volt * 100) / 1150) *
			     ((mt_cpu_freqs[CA15_CLUSTER][j].cpufreq_volt * 100) / 1150)) / 100;
			freq_ratio[CA15_CLUSTER] =
			    (mt_cpu_freqs[CA15_CLUSTER][j].cpufreq_khz / 1500);
			dprintk("freq_ratio = %d, volt_square_ratio %d\n", freq_ratio[CA15_CLUSTER],
				volt_square_ratio[CA15_CLUSTER]);

			multi[CA15_CLUSTER] =
			    ((mt_cpu_freqs[CA15_CLUSTER][j].cpufreq_volt * 100) / 1150) *
			    ((mt_cpu_freqs[CA15_CLUSTER][j].cpufreq_volt * 100) / 1150) *
			    ((mt_cpu_freqs[CA15_CLUSTER][j].cpufreq_volt * 100) / 1150);

			/* 1 core */
			p_dynamic[CA15_CLUSTER] =
			    (((A_CA15_1_CORE * freq_ratio[CA15_CLUSTER]) / 1000) *
			     volt_square_ratio[CA15_CLUSTER]) / 100;
			p_leakage[CA15_CLUSTER] =
			    ((P_CA15_TOTAL_CORE_L * (multi[CA15_CLUSTER])) / (100 * 100 * 100)) -
			    1 * P_CA15_EACH_CORE_L;
			dprintk("p_dynamic[CA15_CLUSTER] = %d, p_leakage[CA15_CLUSTER] = %d\n",
				p_dynamic[CA15_CLUSTER], p_leakage[CA15_CLUSTER]);

			mt_cpu_power[index].cpufreq_ncpu_ca15 = 1;
			mt_cpu_power[index].cpufreq_ncpu =
			    mt_cpu_power[index].cpufreq_ncpu_ca7 +
			    mt_cpu_power[index].cpufreq_ncpu_ca15;
			mt_cpu_power[index].cpufreq_khz_ca15 =
			    mt_cpu_freqs[CA15_CLUSTER][j].cpufreq_khz;
			mt_cpu_power[index].cpufreq_khz =
			    mt_cpu_power[index].cpufreq_khz_ca7 *
			    mt_cpu_power[index].cpufreq_ncpu_ca7 +
			    ((mt_cpu_power[index].cpufreq_khz_ca15 * PERFROMANCE_RATIO) *
			     mt_cpu_power[index].cpufreq_ncpu_ca15) / 10;
			mt_cpu_power[index].cpufreq_power_ca15 =
			    p_dynamic[CA15_CLUSTER] + p_leakage[CA15_CLUSTER];
			mt_cpu_power[index].cpufreq_power =
			    mt_cpu_power[index].cpufreq_power_ca7 +
			    mt_cpu_power[index].cpufreq_power_ca15;
			dprintk
			    ("CA15 mt_cpu_power[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
			     index, mt_cpu_power[index].cpufreq_ncpu_ca15,
			     mt_cpu_power[index].cpufreq_khz_ca15,
			     mt_cpu_power[index].cpufreq_power_ca15);
			dprintk
			    ("mt_cpu_power[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
			     index, mt_cpu_power[index].cpufreq_ncpu,
			     mt_cpu_power[index].cpufreq_khz, mt_cpu_power[index].cpufreq_power);
			index++;

			/* CA7 1 core + CA15 */
			mt_cpu_power[index].cpufreq_ncpu_ca7 = 1;
			mt_cpu_power[index].cpufreq_khz_ca7 =
			    mt_cpu_freqs[CA7_CLUSTER][i].cpufreq_khz;
			mt_cpu_power[index].cpufreq_power_ca7 =
			    p_dynamic[CA7_CLUSTER] + p_leakage[CA7_CLUSTER];
			dprintk
			    ("CA7 mt_cpu_power[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
			     index, mt_cpu_power[index].cpufreq_ncpu_ca7,
			     mt_cpu_power[index].cpufreq_khz_ca7,
			     mt_cpu_power[index].cpufreq_power_ca7);

			/* 2 core */
			p_dynamic[CA15_CLUSTER] =
			    (((A_CA15_2_CORE * freq_ratio[CA15_CLUSTER]) / 1000) *
			     volt_square_ratio[CA15_CLUSTER]) / 100;
			p_leakage[CA15_CLUSTER] =
			    (P_CA15_TOTAL_CORE_L * (multi[CA15_CLUSTER])) / (100 * 100 * 100);
			dprintk("p_dynamic[CA15_CLUSTER] = %d, p_leakage[CA15_CLUSTER] = %d\n",
				p_dynamic[CA15_CLUSTER], p_leakage[CA15_CLUSTER]);

			mt_cpu_power[index].cpufreq_ncpu_ca15 = 2;
			mt_cpu_power[index].cpufreq_ncpu =
			    mt_cpu_power[index].cpufreq_ncpu_ca7 +
			    mt_cpu_power[index].cpufreq_ncpu_ca15;
			mt_cpu_power[index].cpufreq_khz_ca15 =
			    mt_cpu_freqs[CA15_CLUSTER][j].cpufreq_khz;
			mt_cpu_power[index].cpufreq_khz =
			    mt_cpu_power[index].cpufreq_khz_ca7 *
			    mt_cpu_power[index].cpufreq_ncpu_ca7 +
			    ((mt_cpu_power[index].cpufreq_khz_ca15 * PERFROMANCE_RATIO) *
			     mt_cpu_power[index].cpufreq_ncpu_ca15) / 10;
			mt_cpu_power[index].cpufreq_power_ca15 =
			    p_dynamic[CA15_CLUSTER] + p_leakage[CA15_CLUSTER];
			mt_cpu_power[index].cpufreq_power =
			    mt_cpu_power[index].cpufreq_power_ca7 +
			    mt_cpu_power[index].cpufreq_power_ca15;
			dprintk
			    ("CA15 mt_cpu_power[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
			     index, mt_cpu_power[index].cpufreq_ncpu_ca15,
			     mt_cpu_power[index].cpufreq_khz_ca15,
			     mt_cpu_power[index].cpufreq_power_ca15);
			dprintk
			    ("mt_cpu_power[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
			     index, mt_cpu_power[index].cpufreq_ncpu,
			     mt_cpu_power[index].cpufreq_khz, mt_cpu_power[index].cpufreq_power);
			index++;

		}

		/* CA7 2 core only */
		p_dynamic[CA7_CLUSTER] =
		    (((A_CA7_2_CORE * freq_ratio[CA7_CLUSTER]) / 1000) *
		     volt_square_ratio[CA7_CLUSTER]) / 100;
		p_leakage[CA7_CLUSTER] =
		    ((P_CA7_TOTAL_CORE_L * (multi[CA7_CLUSTER])) / (100 * 100 * 100));
		dprintk("p_dynamic[CA7_CLUSTER] = %d, p_leakage[CA7_CLUSTER] = %d\n",
			p_dynamic[CA7_CLUSTER], p_leakage[CA7_CLUSTER]);

		mt_cpu_power[index].cpufreq_ncpu_ca7 = 2;
		mt_cpu_power[index].cpufreq_ncpu_ca15 = 0;
		mt_cpu_power[index].cpufreq_ncpu = 2;
		mt_cpu_power[index].cpufreq_khz_ca7 = mt_cpu_freqs[CA7_CLUSTER][i].cpufreq_khz;
		mt_cpu_power[index].cpufreq_khz_ca15 = 0;
		mt_cpu_power[index].cpufreq_khz =
		    mt_cpu_power[index].cpufreq_khz_ca7 * mt_cpu_power[index].cpufreq_ncpu_ca7;
		mt_cpu_power[index].cpufreq_power_ca7 =
		    p_dynamic[CA7_CLUSTER] + p_leakage[CA7_CLUSTER];
		mt_cpu_power[index].cpufreq_power_ca15 = 0;
		mt_cpu_power[index].cpufreq_power = mt_cpu_power[index].cpufreq_power_ca7;
		dprintk
		    ("CA7 mt_cpu_power[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
		     index, mt_cpu_power[index].cpufreq_ncpu_ca7,
		     mt_cpu_power[index].cpufreq_khz_ca7, mt_cpu_power[index].cpufreq_power_ca7);
		dprintk
		    ("CA15 mt_cpu_power[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
		     index, mt_cpu_power[index].cpufreq_ncpu_ca15,
		     mt_cpu_power[index].cpufreq_khz_ca15, mt_cpu_power[index].cpufreq_power_ca15);
		dprintk
		    ("mt_cpu_power[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
		     index, mt_cpu_power[index].cpufreq_ncpu, mt_cpu_power[index].cpufreq_khz,
		     mt_cpu_power[index].cpufreq_power);
		index++;

		for (j = 0; j < mt_cpu_freqs_num[CA15_CLUSTER]; j++) {
			/* CA7 2 core + CA15 */
			mt_cpu_power[index].cpufreq_ncpu_ca7 = 2;
			mt_cpu_power[index].cpufreq_khz_ca7 =
			    mt_cpu_freqs[CA7_CLUSTER][i].cpufreq_khz;
			mt_cpu_power[index].cpufreq_power_ca7 =
			    p_dynamic[CA7_CLUSTER] + p_leakage[CA7_CLUSTER];
			dprintk("CA7 mt_cpu_power[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
				index, mt_cpu_power[index].cpufreq_ncpu_ca7,
				mt_cpu_power[index].cpufreq_khz_ca7,
				mt_cpu_power[index].cpufreq_power_ca7);


			volt_square_ratio[CA15_CLUSTER] =
			    (((mt_cpu_freqs[CA15_CLUSTER][j].cpufreq_volt * 100) / 1150) *
			     ((mt_cpu_freqs[CA15_CLUSTER][j].cpufreq_volt * 100) / 1150)) / 100;
			freq_ratio[CA15_CLUSTER] =
			    (mt_cpu_freqs[CA15_CLUSTER][j].cpufreq_khz / 1500);
			dprintk("freq_ratio = %d, volt_square_ratio %d\n", freq_ratio[CA15_CLUSTER],
				volt_square_ratio[CA15_CLUSTER]);

			multi[CA15_CLUSTER] =
			    ((mt_cpu_freqs[CA15_CLUSTER][j].cpufreq_volt * 100) / 1150) *
			    ((mt_cpu_freqs[CA15_CLUSTER][j].cpufreq_volt * 100) / 1150) *
			    ((mt_cpu_freqs[CA15_CLUSTER][j].cpufreq_volt * 100) / 1150);

			/* 1 core */
			p_dynamic[CA15_CLUSTER] =
			    (((A_CA15_1_CORE * freq_ratio[CA15_CLUSTER]) / 1000) *
			     volt_square_ratio[CA15_CLUSTER]) / 100;
			p_leakage[CA15_CLUSTER] =
			    ((P_CA15_TOTAL_CORE_L * (multi[CA15_CLUSTER])) / (100 * 100 * 100)) -
			    1 * P_CA15_EACH_CORE_L;
			dprintk("p_dynamic[CA15_CLUSTER] = %d, p_leakage[CA15_CLUSTER] = %d\n",
				p_dynamic[CA15_CLUSTER], p_leakage[CA15_CLUSTER]);

			mt_cpu_power[index].cpufreq_ncpu_ca15 = 1;
			mt_cpu_power[index].cpufreq_ncpu =
			    mt_cpu_power[index].cpufreq_ncpu_ca7 +
			    mt_cpu_power[index].cpufreq_ncpu_ca15;
			mt_cpu_power[index].cpufreq_khz_ca15 =
			    mt_cpu_freqs[CA15_CLUSTER][j].cpufreq_khz;
			mt_cpu_power[index].cpufreq_khz =
			    mt_cpu_power[index].cpufreq_khz_ca7 *
			    mt_cpu_power[index].cpufreq_ncpu_ca7 +
			    ((mt_cpu_power[index].cpufreq_khz_ca15 * PERFROMANCE_RATIO) *
			     mt_cpu_power[index].cpufreq_ncpu_ca15) / 10;
			mt_cpu_power[index].cpufreq_power_ca15 =
			    p_dynamic[CA15_CLUSTER] + p_leakage[CA15_CLUSTER];
			mt_cpu_power[index].cpufreq_power =
			    mt_cpu_power[index].cpufreq_power_ca7 +
			    mt_cpu_power[index].cpufreq_power_ca15;
			dprintk
			    ("CA15 mt_cpu_power[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
			     index, mt_cpu_power[index].cpufreq_ncpu_ca15,
			     mt_cpu_power[index].cpufreq_khz_ca15,
			     mt_cpu_power[index].cpufreq_power_ca15);
			dprintk
			    ("mt_cpu_power[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
			     index, mt_cpu_power[index].cpufreq_ncpu,
			     mt_cpu_power[index].cpufreq_khz, mt_cpu_power[index].cpufreq_power);

			index++;

			/* CA7 2 core + CA15 */
			mt_cpu_power[index].cpufreq_ncpu_ca7 = 2;
			mt_cpu_power[index].cpufreq_khz_ca7 =
			    mt_cpu_freqs[CA7_CLUSTER][i].cpufreq_khz;
			mt_cpu_power[index].cpufreq_power_ca7 =
			    p_dynamic[CA7_CLUSTER] + p_leakage[CA7_CLUSTER];
			dprintk
			    ("CA7 mt_cpu_power[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
			     index, mt_cpu_power[index].cpufreq_ncpu_ca7,
			     mt_cpu_power[index].cpufreq_khz_ca7,
			     mt_cpu_power[index].cpufreq_power_ca7);



			/* 2 core */
			p_dynamic[CA15_CLUSTER] =
			    (((A_CA15_2_CORE * freq_ratio[CA15_CLUSTER]) / 1000) *
			     volt_square_ratio[CA15_CLUSTER]) / 100;
			p_leakage[CA15_CLUSTER] =
			    (P_CA15_TOTAL_CORE_L * (multi[CA15_CLUSTER])) / (100 * 100 * 100);
			dprintk("p_dynamic[CA15_CLUSTER] = %d, p_leakage[CA15_CLUSTER] = %d\n",
				p_dynamic[CA15_CLUSTER], p_leakage[CA15_CLUSTER]);

			mt_cpu_power[index].cpufreq_ncpu_ca15 = 2;
			mt_cpu_power[index].cpufreq_ncpu =
			    mt_cpu_power[index].cpufreq_ncpu_ca7 +
			    mt_cpu_power[index].cpufreq_ncpu_ca15;
			mt_cpu_power[index].cpufreq_khz_ca15 =
			    mt_cpu_freqs[CA15_CLUSTER][j].cpufreq_khz;
			mt_cpu_power[index].cpufreq_khz =
			    mt_cpu_power[index].cpufreq_khz_ca7 *
			    mt_cpu_power[index].cpufreq_ncpu_ca7 +
			    ((mt_cpu_power[index].cpufreq_khz_ca15 * PERFROMANCE_RATIO) *
			     mt_cpu_power[index].cpufreq_ncpu_ca15) / 10;
			mt_cpu_power[index].cpufreq_power_ca15 =
			    p_dynamic[CA15_CLUSTER] + p_leakage[CA15_CLUSTER];
			mt_cpu_power[index].cpufreq_power =
			    mt_cpu_power[index].cpufreq_power_ca7 +
			    mt_cpu_power[index].cpufreq_power_ca15;
			dprintk
			    ("CA15 mt_cpu_power[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
			     index, mt_cpu_power[index].cpufreq_ncpu_ca15,
			     mt_cpu_power[index].cpufreq_khz_ca15,
			     mt_cpu_power[index].cpufreq_power_ca15);
			dprintk
			    ("mt_cpu_power[%d]: cpufreq_ncpu = %d, cpufreq_khz = %d, cpufreq_power = %d\n",
			     index, mt_cpu_power[index].cpufreq_ncpu,
			     mt_cpu_power[index].cpufreq_khz, mt_cpu_power[index].cpufreq_power);
			index++;

		}


	}

	for (i = (index - 1); i > 0; i--) {
		for (j = 1; j <= i; j++) {
			if (mt_cpu_power[j - 1].cpufreq_power < mt_cpu_power[j].cpufreq_power) {
				temp_power_info.cpufreq_khz = mt_cpu_power[j - 1].cpufreq_khz;
				temp_power_info.cpufreq_ncpu = mt_cpu_power[j - 1].cpufreq_ncpu;
				temp_power_info.cpufreq_power = mt_cpu_power[j - 1].cpufreq_power;
				temp_power_info.cpufreq_khz_ca7 =
				    mt_cpu_power[j - 1].cpufreq_khz_ca7;
				temp_power_info.cpufreq_ncpu_ca7 =
				    mt_cpu_power[j - 1].cpufreq_ncpu_ca7;
				temp_power_info.cpufreq_power_ca7 =
				    mt_cpu_power[j - 1].cpufreq_power_ca7;
				temp_power_info.cpufreq_khz_ca15 =
				    mt_cpu_power[j - 1].cpufreq_khz_ca15;
				temp_power_info.cpufreq_ncpu_ca15 =
				    mt_cpu_power[j - 1].cpufreq_ncpu_ca15;
				temp_power_info.cpufreq_power_ca15 =
				    mt_cpu_power[j - 1].cpufreq_power_ca15;

				mt_cpu_power[j - 1].cpufreq_khz = mt_cpu_power[j].cpufreq_khz;
				mt_cpu_power[j - 1].cpufreq_ncpu = mt_cpu_power[j].cpufreq_ncpu;
				mt_cpu_power[j - 1].cpufreq_power = mt_cpu_power[j].cpufreq_power;
				mt_cpu_power[j - 1].cpufreq_khz_ca7 =
				    mt_cpu_power[j].cpufreq_khz_ca7;
				mt_cpu_power[j - 1].cpufreq_ncpu_ca7 =
				    mt_cpu_power[j].cpufreq_ncpu_ca7;
				mt_cpu_power[j - 1].cpufreq_power_ca7 =
				    mt_cpu_power[j].cpufreq_power_ca7;
				mt_cpu_power[j - 1].cpufreq_khz_ca15 =
				    mt_cpu_power[j].cpufreq_khz_ca15;
				mt_cpu_power[j - 1].cpufreq_ncpu_ca15 =
				    mt_cpu_power[j].cpufreq_ncpu_ca15;
				mt_cpu_power[j - 1].cpufreq_power_ca15 =
				    mt_cpu_power[j].cpufreq_power_ca15;

				mt_cpu_power[j].cpufreq_khz = temp_power_info.cpufreq_khz;
				mt_cpu_power[j].cpufreq_ncpu = temp_power_info.cpufreq_ncpu;
				mt_cpu_power[j].cpufreq_power = temp_power_info.cpufreq_power;
				mt_cpu_power[j].cpufreq_khz_ca7 = temp_power_info.cpufreq_khz_ca7;
				mt_cpu_power[j].cpufreq_ncpu_ca7 = temp_power_info.cpufreq_ncpu_ca7;
				mt_cpu_power[j].cpufreq_power_ca7 =
				    temp_power_info.cpufreq_power_ca7;
				mt_cpu_power[j].cpufreq_khz_ca15 = temp_power_info.cpufreq_khz_ca15;
				mt_cpu_power[j].cpufreq_ncpu_ca15 =
				    temp_power_info.cpufreq_ncpu_ca15;
				mt_cpu_power[j].cpufreq_power_ca15 =
				    temp_power_info.cpufreq_power_ca15;
			}
		}
	}

	for (i = 0; i < index; i++) {
		dprintk("mt_cpu_power[%d].cpufreq_khz_ca7 = %d, ", i,
			mt_cpu_power[i].cpufreq_khz_ca7);
		dprintk("mt_cpu_power[%d].cpufreq_khz_ca15 = %d, ", i,
			mt_cpu_power[i].cpufreq_khz_ca15);
		dprintk("mt_cpu_power[%d].cpufreq_khz = %d, ", i, mt_cpu_power[i].cpufreq_khz);
		dprintk("mt_cpu_power[%d].cpufreq_ncpu_ca7 = %d, ", i,
			mt_cpu_power[i].cpufreq_ncpu_ca7);
		dprintk("mt_cpu_power[%d].cpufreq_ncpu_ca15 = %d, ", i,
			mt_cpu_power[i].cpufreq_ncpu_ca15);
		dprintk("mt_cpu_power[%d].cpufreq_ncpu = %d, ", i, mt_cpu_power[i].cpufreq_ncpu);
		dprintk("mt_cpu_power[%d].cpufreq_power_ca7 = %d\n", i,
			mt_cpu_power[i].cpufreq_power_ca7);
		dprintk("mt_cpu_power[%d].cpufreq_power_ca15 = %d\n", i,
			mt_cpu_power[i].cpufreq_power_ca15);
		dprintk("mt_cpu_power[%d].cpufreq_power = %d\n", i, mt_cpu_power[i].cpufreq_power);
	}

#ifdef MT_POWER_API_SUPPORT
	mt_percpu_power[CA7_CLUSTER] =
	    kzalloc(mt_cpu_freqs_num[CA7_CLUSTER] * sizeof(struct mt_percpu_power_info),
		    GFP_KERNEL);
	mt_percpu_power[CA15_CLUSTER] =
	    kzalloc(mt_cpu_freqs_num[CA15_CLUSTER] * sizeof(struct mt_percpu_power_info),
		    GFP_KERNEL);

	for (i = 0; i < mt_cpu_freqs_num[CA7_CLUSTER]; i++) {
		mt_percpu_power[CA7_CLUSTER][i].cpufreq_khz =
		    mt_cpu_freqs[CA7_CLUSTER][i].cpufreq_khz;

		volt_square_ratio[CA7_CLUSTER] =
		    (((mt_cpu_freqs[CA7_CLUSTER][i].cpufreq_volt * 100) / 1150) *
		     ((mt_cpu_freqs[CA7_CLUSTER][i].cpufreq_volt * 100) / 1150)) / 100;
		freq_ratio[CA7_CLUSTER] = (mt_cpu_freqs[CA7_CLUSTER][i].cpufreq_khz / 1200);
		dprintk("freq_ratio = %d, volt_square_ratio %d\n", freq_ratio[CA7_CLUSTER],
			volt_square_ratio[CA7_CLUSTER]);
		multi[CA7_CLUSTER] =
		    ((mt_cpu_freqs[CA7_CLUSTER][i].cpufreq_volt * 100) / 1150) *
		    ((mt_cpu_freqs[CA7_CLUSTER][i].cpufreq_volt * 100) / 1150) *
		    ((mt_cpu_freqs[CA7_CLUSTER][i].cpufreq_volt * 100) / 1150);

		/* 1 core */
		p_dynamic[CA7_CLUSTER] =
		    (((P_CA7_WFI_D * freq_ratio[CA7_CLUSTER]) / 1000) *
		     volt_square_ratio[CA7_CLUSTER]) / 100;
		p_leakage[CA7_CLUSTER] = ((P_CA7_WFI_L * (multi[CA7_CLUSTER])) / (100 * 100 * 100));

		mt_percpu_power[CA7_CLUSTER][i].cpufreq_power_wfi =
		    p_dynamic[CA7_CLUSTER] + p_leakage[CA7_CLUSTER];

		for (j = 0; j < g_thermal_opp_num; j++) {
			if (mt_cpu_power[j].cpufreq_ncpu == 1 &&
			    mt_cpu_power[j].cpufreq_ncpu_ca7 == 1 &&
			    mt_cpu_power[j].cpufreq_khz_ca7 ==
			    mt_percpu_power[CA7_CLUSTER][i].cpufreq_khz) {
				mt_percpu_power[CA7_CLUSTER][i].cpufreq_power_max =
				    mt_cpu_power[j].cpufreq_power_ca7;
				break;
			}
		}

	}

	for (i = 0; i < mt_cpu_freqs_num[CA15_CLUSTER]; i++) {
		mt_percpu_power[CA15_CLUSTER][i].cpufreq_khz =
		    mt_cpu_freqs[CA15_CLUSTER][i].cpufreq_khz;

		volt_square_ratio[CA15_CLUSTER] =
		    (((mt_cpu_freqs[CA15_CLUSTER][i].cpufreq_volt * 100) / 1150) *
		     ((mt_cpu_freqs[CA15_CLUSTER][i].cpufreq_volt * 100) / 1150)) / 100;
		freq_ratio[CA15_CLUSTER] = (mt_cpu_freqs[CA15_CLUSTER][i].cpufreq_khz / 1500);
		multi[CA15_CLUSTER] =
		    ((mt_cpu_freqs[CA15_CLUSTER][i].cpufreq_volt * 100) / 1150) *
		    ((mt_cpu_freqs[CA15_CLUSTER][i].cpufreq_volt * 100) / 1150) *
		    ((mt_cpu_freqs[CA15_CLUSTER][i].cpufreq_volt * 100) / 1150);

		/* 1 core */
		p_dynamic[CA15_CLUSTER] =
		    (((P_CA15_WFI_D * freq_ratio[CA15_CLUSTER]) / 1000) *
		     volt_square_ratio[CA15_CLUSTER]) / 100;
		p_leakage[CA15_CLUSTER] =
		    ((P_CA15_WFI_L * (multi[CA15_CLUSTER])) / (100 * 100 * 100));

		mt_percpu_power[CA15_CLUSTER][i].cpufreq_power_wfi =
		    p_dynamic[CA15_CLUSTER] + p_leakage[CA15_CLUSTER];

		for (j = 0; j < g_thermal_opp_num; j++) {
			if (mt_cpu_power[j].cpufreq_ncpu == 2 &&
			    mt_cpu_power[j].cpufreq_ncpu_ca15 == 1 &&
			    mt_cpu_power[j].cpufreq_khz_ca15 ==
			    mt_percpu_power[CA15_CLUSTER][i].cpufreq_khz) {
				mt_percpu_power[CA15_CLUSTER][i].cpufreq_power_max =
				    mt_cpu_power[j].cpufreq_power_ca15;
				break;
			}
		}

	}
#endif

#ifdef CONFIG_THERMAL
	mtk_cpufreq_register(mt_cpu_power, g_thermal_opp_num);
#endif
}


/***********************************************
* register frequency table to cpufreq subsystem
************************************************/
static int mt_setup_freqs_table(struct cpufreq_policy *policy, struct mt_cpu_freq_info *freqs,
				int num)
{
	struct cpufreq_frequency_table *table;
	int i, ret;
	unsigned int cur_cluster;
	cur_cluster = cpu_to_cluster(policy->cpu);
	if (mt_cpu_freqs_table[cur_cluster] == NULL) {
		table = kzalloc((num + 1) * sizeof(*table), GFP_KERNEL);
		if (table == NULL)
			return -ENOMEM;

		for (i = 0; i < num; i++) {
			table[i].index = i;
			table[i].frequency = freqs[i].cpufreq_khz;
		}
		table[num].index = i;
		table[num].frequency = CPUFREQ_TABLE_END;

		mt_cpu_freqs[cur_cluster] = freqs;
		mt_cpu_freqs_num[cur_cluster] = num;
		mt_cpu_freqs_table[cur_cluster] = table;
		DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS", "Create cpufreq table\n");
	}


	ret = cpufreq_frequency_table_cpuinfo(policy, mt_cpu_freqs_table[cur_cluster]);
	if (!ret)
		cpufreq_frequency_table_get_attr(mt_cpu_freqs_table[cur_cluster], policy->cpu);

	policy->cpuinfo.transition_latency = 1000;	/* 1 us assumed */
	policy->cur = g_cur_freq[cur_cluster];

	cpumask_copy(policy->cpus, topology_core_cpumask(policy->cpu));
	cpumask_copy(policy->related_cpus, policy->cpus);

	if (mt_cpu_power == NULL && (mt_cpu_freqs[CA7_CLUSTER] != NULL)
	    && (mt_cpu_freqs[CA15_CLUSTER] != NULL))
		mt_setup_power_table();

	return 0;
}

/*****************************
* set CPU DVFS status
******************************/
int mt_cpufreq_state_set(int enabled)
{
	if (enabled) {
		if (!mt_cpufreq_pause) {
			DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS", "cpufreq already enabled\n");
			return 0;
		}

	/*************
	* enable DVFS
	**************/
		g_dvfs_disable_count--;
		DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
			    "enable DVFS: g_dvfs_disable_count = %d\n", g_dvfs_disable_count);

	/***********************************************
	* enable DVFS if no any module still disable it
	************************************************/
		if (g_dvfs_disable_count <= 0) {
			mt_cpufreq_pause = false;
		} else {
			DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
				    "someone still disable cpufreq, cannot enable it\n");
		}
	} else {
	/**************
	* disable DVFS
	***************/
		g_dvfs_disable_count++;
		DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
			    "disable DVFS: g_dvfs_disable_count = %d\n", g_dvfs_disable_count);

		if (mt_cpufreq_pause) {
			DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS", "cpufreq already disabled\n");
			return 0;
		}

		mt_cpufreq_pause = true;
	}

	return 0;
}
EXPORT_SYMBOL(mt_cpufreq_state_set);

static int mt_cpufreq_verify(struct cpufreq_policy *policy)
{
	unsigned int cur_cluster = cpu_to_cluster(policy->cpu);
	dprintk("call mt_cpufreq_verify cpu[%d]!\n", policy->cpu);
	return cpufreq_frequency_table_verify(policy, mt_cpu_freqs_table[cur_cluster]);
}

static unsigned int mt_cpufreq_get(unsigned int cpu)
{
	unsigned int cur_cluster = cpu_to_cluster(cpu);
	dprintk("call mt_cpufreq_get cpu[%d]: %d!\n", cpu, g_cur_freq[cur_cluster]);
	return g_cur_freq[cur_cluster];
}

static void mt_cpu_clock_switch(unsigned int sel, unsigned int cur_cluster)
{
	unsigned int ckmuxsel = 0;

	if (cur_cluster == CA7_CLUSTER) {	/* CA7 */
		ckmuxsel = DRV_Reg32(TOP_CKMUXSEL) & ~0xC;
		switch (sel) {
		case TOP_CKMUXSEL_CLKSQ:
			mt65xx_reg_sync_writel((ckmuxsel | 0x00), TOP_CKMUXSEL);
			break;
		case TOP_CKMUXSEL_ARMPLL2:
			mt65xx_reg_sync_writel((ckmuxsel | 0x04), TOP_CKMUXSEL);
			break;
		case TOP_CKMUXSEL_MAINPLL:
			mt65xx_reg_sync_writel((ckmuxsel | 0x08), TOP_CKMUXSEL);
			break;
		case TOP_CKMUXSEL_UNIVPLL:
			mt65xx_reg_sync_writel((ckmuxsel | 0x0C), TOP_CKMUXSEL);
			break;
		default:
			break;
		}
	} else {
		ckmuxsel = DRV_Reg32(TOP_CKMUXSEL) & ~0x30;
		switch (sel) {
		case TOP_CKMUXSEL_CLKSQ:
			mt65xx_reg_sync_writel((ckmuxsel | 0x00), TOP_CKMUXSEL);
			break;
		case TOP_CKMUXSEL_ARMPLL:
			mt65xx_reg_sync_writel((ckmuxsel | 0x10), TOP_CKMUXSEL);
			break;
		case TOP_CKMUXSEL_MAINPLL:
			mt65xx_reg_sync_writel((ckmuxsel | 0x20), TOP_CKMUXSEL);
			break;
		case TOP_CKMUXSEL_UNIVPLL:
			mt65xx_reg_sync_writel((ckmuxsel | 0x30), TOP_CKMUXSEL);
			break;
		default:
			break;
		}

	}

}

/* Need sync with mt_cpufreq_volt_to_pmic_wrap(), mt_cpufreq_pdrv_probe() */
static void mt_cpufreq_volt_set(unsigned int target_volt, unsigned int cur_cluster)
{
	unsigned int pmic_mapping_value = 0x58;

	if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 1) {
		if (cur_cluster == CA7_CLUSTER) {	/* CA7 */
			switch (target_volt) {
				dprintk("CA7 PTP DVS:\n");
			case DVFS_V1250:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][0];
				dprintk("switch to DVS0: %d mV\n", DVFS_V1250);
				break;
			case DVFS_V1200:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][1];
				dprintk("switch to DVS1: %d mV\n", DVFS_V1200);
				break;
			case DVFS_V1150:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][2];
				dprintk("switch to DVS2: %d mV\n", DVFS_V1150);
				break;
			case DVFS_V1100:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][3];
				dprintk("switch to DVS3: %d mV\n", DVFS_V1100);
				break;
			case DVFS_V1050:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][4];
				dprintk("switch to DVS4: %d mV\n", DVFS_V1050);
				break;
			case DVFS_V1000:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][5];
				dprintk("switch to DVS5: %d mV\n", DVFS_V1000);
				break;
			case DVFS_V0975:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][6];
				dprintk("switch to DVS6: %d mV\n", DVFS_V0975);
				break;
			case DVFS_V0950:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][7];
				dprintk("switch to DVS7: %d mV\n", DVFS_V0950);
				break;
			default:
				break;
			}

#ifdef MT_CPUSTRESS_TEST
			pmic_mapping_value += g_ca7_delta_v;
			if (pmic_mapping_value < 0x28)	/* keep 0.95V */
				pmic_mapping_value = 0x28;
#endif

			dprintk("CA7 pmic_mapping_value: 0x%x\n", pmic_mapping_value);
			mt65xx_reg_sync_writel(pmic_mapping_value, PMIC_WRAP_DVFS_WDATA0);	/* CA7 */
			spm_dvfs_ctrl_volt(0);	/* CA7 */

#ifdef PMIC_VOLTAGE_CHECK
			pwrap_read(0x033A, &g_vca7);
			dprintk("Current VCA7: 0x%x\n", g_vca7);
#endif
		} else {
			dprintk("CA15 PTP DVS:\n");
			switch (target_volt) {
			case DVFS_V1250:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][0];
				dprintk("switch to DVS0: %d mV\n", DVFS_V1250);
				break;
			case DVFS_V1225:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][1];
				dprintk("switch to DVS1: %d mV\n", DVFS_V1225);
				break;
			case DVFS_V1200:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][2];
				dprintk("switch to DVS2: %d mV\n", DVFS_V1200);
				break;
			case DVFS_V1150:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][3];
				dprintk("switch to DVS3: %d mV\n", DVFS_V1150);
				break;
			case DVFS_V1100:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][4];
				dprintk("switch to DVS4: %d mV\n", DVFS_V1100);
				break;
			case DVFS_V1050:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][5];
				dprintk("switch to DVS5: %d mV\n", DVFS_V1050);
				break;
			case DVFS_V1000:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][6];
				dprintk("switch to DVS6: %d mV\n", DVFS_V1000);
				break;
			case DVFS_V0950:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][7];
				dprintk("switch to DVS7: %d mV\n", DVFS_V0950);
				break;
			default:
				break;
			}

#ifdef MT_CPUSTRESS_TEST
			pmic_mapping_value += g_ca15_delta_v;
			if (pmic_mapping_value < 0x28)	/* keep 0.95V */
				pmic_mapping_value = 0x28;
#endif

			dprintk("CA15 pmic_mapping_value: 0x%x\n", pmic_mapping_value);
			mt65xx_reg_sync_writel(pmic_mapping_value, PMIC_WRAP_DVFS_WDATA1);	/* CA15 */
			spm_dvfs_ctrl_volt(1);	/* CA15 */

#ifdef PMIC_VOLTAGE_CHECK
			pwrap_read(0x0228, &g_vca15);
			dprintk("Current VCA15: 0x%x\n", g_vca15);
#endif
		}

	} else if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 3) {
		if (cur_cluster == CA7_CLUSTER) {	/* CA7 */
			switch (target_volt) {
				dprintk("CA7 PTP DVS:\n");
			case DVFS_V1250:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][0];
				dprintk("switch to DVS0: %d mV\n", DVFS_V1250);
				break;
			case DVFS_V1150:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][1];
				dprintk("switch to DVS1: %d mV\n", DVFS_V1150);
				break;
			case DVFS_V1100:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][2];
				dprintk("switch to DVS2: %d mV\n", DVFS_V1100);
				break;
			case DVFS_V1050:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][3];
				dprintk("switch to DVS3: %d mV\n", DVFS_V1050);
				break;
			case DVFS_V1000:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][4];
				dprintk("switch to DVS4: %d mV\n", DVFS_V1000);
				break;
			case DVFS_V0975:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][5];
				dprintk("switch to DVS5: %d mV\n", DVFS_V0975);
				break;
			case DVFS_V0950:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][6];
				dprintk("switch to DVS6: %d mV\n", DVFS_V0950);
				break;
			default:
				break;
			}

#ifdef MT_CPUSTRESS_TEST
			pmic_mapping_value += g_ca7_delta_v;
			if (pmic_mapping_value < 0x28)	/* keep 0.95V */
				pmic_mapping_value = 0x28;
#endif

			dprintk("CA7 pmic_mapping_value: 0x%x\n", pmic_mapping_value);
			mt65xx_reg_sync_writel(pmic_mapping_value, PMIC_WRAP_DVFS_WDATA0);	/* CA7 */
			spm_dvfs_ctrl_volt(0);	/* CA7 */

#ifdef PMIC_VOLTAGE_CHECK
			pwrap_read(0x033A, &g_vca7);
			dprintk("Current VCA7: 0x%x\n", g_vca7);
#endif
		} else {
			dprintk("CA15 PTP DVS:\n");
			switch (target_volt) {
			case DVFS_V1250:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][0];
				dprintk("switch to DVS0: %d mV\n", DVFS_V1250);
				break;
			case DVFS_V1150:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][1];
				dprintk("switch to DVS1: %d mV\n", DVFS_V1150);
				break;
			case DVFS_V1100:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][2];
				dprintk("switch to DVS2: %d mV\n", DVFS_V1100);
				break;
			case DVFS_V1050:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][3];
				dprintk("switch to DVS3: %d mV\n", DVFS_V1050);
				break;
			case DVFS_V1000:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][4];
				dprintk("switch to DVS4: %d mV\n", DVFS_V1000);
				break;
			case DVFS_V0975:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][5];
				dprintk("switch to DVS5: %d mV\n", DVFS_V0975);
				break;
			case DVFS_V0950:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][6];
				dprintk("switch to DVS6: %d mV\n", DVFS_V0950);
				break;
			default:
				break;
			}

#ifdef MT_CPUSTRESS_TEST
			pmic_mapping_value += g_ca15_delta_v;
			if (pmic_mapping_value < 0x28)	/* keep 0.95V */
				pmic_mapping_value = 0x28;
#endif

			dprintk("CA15 pmic_mapping_value: 0x%x\n", pmic_mapping_value);
			mt65xx_reg_sync_writel(pmic_mapping_value, PMIC_WRAP_DVFS_WDATA1);	/* CA15 */
			spm_dvfs_ctrl_volt(1);	/* CA15 */

#ifdef PMIC_VOLTAGE_CHECK
			pwrap_read(0x0228, &g_vca15);
			dprintk("Current VCA15: 0x%x\n", g_vca15);
#endif
		}
	} else {
		if (cur_cluster == CA7_CLUSTER) {	/* CA7 */
			switch (target_volt) {
				dprintk("CA7 PTP DVS:\n");
			case DVFS_V1250:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][0];
				dprintk("switch to DVS0: %d mV\n", DVFS_V1250);
				break;
			case DVFS_V1200:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][1];
				dprintk("switch to DVS1: %d mV\n", DVFS_V1200);
				break;
			case DVFS_V1150:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][2];
				dprintk("switch to DVS2: %d mV\n", DVFS_V1150);
				break;
			case DVFS_V1100:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][3];
				dprintk("switch to DVS3: %d mV\n", DVFS_V1100);
				break;
			case DVFS_V1050:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][4];
				dprintk("switch to DVS4: %d mV\n", DVFS_V1050);
				break;
			case DVFS_V1000:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][5];
				dprintk("switch to DVS5: %d mV\n", DVFS_V1000);
				break;
			case DVFS_V0975:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][6];
				dprintk("switch to DVS6: %d mV\n", DVFS_V0975);
				break;
			case DVFS_V0950:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][7];
				dprintk("switch to DVS7: %d mV\n", DVFS_V0950);
				break;
			default:
				break;
			}

#ifdef MT_CPUSTRESS_TEST
			pmic_mapping_value += g_ca7_delta_v;
			if (pmic_mapping_value < 0x28)	/* keep 0.95V */
				pmic_mapping_value = 0x28;
#endif

			dprintk("CA7 pmic_mapping_value: 0x%x\n", pmic_mapping_value);
			mt65xx_reg_sync_writel(pmic_mapping_value, PMIC_WRAP_DVFS_WDATA0);	/* CA7 */
			spm_dvfs_ctrl_volt(0);	/* CA7 */

#ifdef PMIC_VOLTAGE_CHECK
			pwrap_read(0x033A, &g_vca7);
			dprintk("Current VCA7: 0x%x\n", g_vca7);
#endif
		} else {
			dprintk("CA15 PTP DVS:\n");
			switch (target_volt) {
			case DVFS_V1250:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][0];
				dprintk("switch to DVS0: %d mV\n", DVFS_V1250);
				break;
			case DVFS_V1200:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][1];
				dprintk("switch to DVS1: %d mV\n", DVFS_V1200);
				break;
			case DVFS_V1150:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][2];
				dprintk("switch to DVS2: %d mV\n", DVFS_V1150);
				break;
			case DVFS_V1100:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][3];
				dprintk("switch to DVS3: %d mV\n", DVFS_V1100);
				break;
			case DVFS_V1050:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][4];
				dprintk("switch to DVS4: %d mV\n", DVFS_V1050);
				break;
			case DVFS_V1000:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][5];
				dprintk("switch to DVS5: %d mV\n", DVFS_V1000);
				break;
			case DVFS_V0975:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][6];
				dprintk("switch to DVS6: %d mV\n", DVFS_V0975);
				break;
			case DVFS_V0950:
				pmic_mapping_value = mt_cpufreq_pmic_volt[cur_cluster][7];
				dprintk("switch to DVS7: %d mV\n", DVFS_V0950);
				break;
			default:
				break;
			}

#ifdef MT_CPUSTRESS_TEST
			pmic_mapping_value += g_ca15_delta_v;
			if (pmic_mapping_value < 0x28)	/* keep 0.95V */
				pmic_mapping_value = 0x28;
#endif

			dprintk("CA15 pmic_mapping_value: 0x%x\n", pmic_mapping_value);
			mt65xx_reg_sync_writel(pmic_mapping_value, PMIC_WRAP_DVFS_WDATA1);	/* CA15 */
			spm_dvfs_ctrl_volt(1);	/* CA15 */

#ifdef PMIC_VOLTAGE_CHECK
			pwrap_read(0x0228, &g_vca15);
			dprintk("Current VCA15: 0x%x\n", g_vca15);
#endif
		}
	}

}

/*****************************************
* frequency ramp up and ramp down handler
******************************************/
/***********************************************************
* [note]
* 1. frequency ramp up need to wait voltage settle
* 2. frequency ramp down do not need to wait voltage settle
************************************************************/
static void mt_cpufreq_set(unsigned int freq_old, unsigned int freq_new, unsigned int target_volt,
			   unsigned int cur_cluster)
{
	bool ramp_up = freq_new > freq_old;
	int pll = cur_cluster == CA7_CLUSTER ? ARMPLL2 : ARMPLL1;
	int clk_src = cur_cluster == CA7_CLUSTER ?
		TOP_CKMUXSEL_ARMPLL2 : TOP_CKMUXSEL_ARMPLL;

#ifdef MT_BUCK_ADJUST
	if (ramp_up) {
		/* TODO: convert to hwPowerXXX call */
		mt_cpufreq_volt_set(target_volt, cur_cluster);
		udelay(PMIC_SETTLE_TIME);

#ifdef MT_CPUSTRESS_TEST_DEBUG
		mdelay(1);
#endif
	}
#endif

	if (g_top_pll >= 0)
		enable_pll(g_top_pll, "CPU_DVFS");
	enable_pll(pll, "CPU_DVFS");
	mt_cpu_clock_switch(g_top_ckmuxsel, cur_cluster);
	pll_set_freq(pll, freq_new);

#ifdef MT_CPUSTRESS_TEST_DEBUG
	mdelay(1);
#endif

	mt_cpu_clock_switch(clk_src, cur_cluster);
	disable_pll(pll, "CPU_DVFS");
	if (g_top_pll >= 0)
		disable_pll(g_top_pll, "CPU_DVFS");

#ifdef MT_BUCK_ADJUST
	if (!ramp_up)
		/* TODO: convert to hwPowerXXX call */
		mt_cpufreq_volt_set(target_volt, cur_cluster);
#endif

	g_cur_freq[cur_cluster] = freq_new;
	g_cur_cpufreq_volt[cur_cluster] = target_volt;

#ifdef MT_CPUSTRESS_TEST_DEBUG
	dprintk("mdelay\n");
#endif

	if (cur_cluster == CA7_CLUSTER) {
		dprintk("CA7: ARMPLL2_CON0 = 0x%x, ARMPLL2_CON1 = 0x%x, g_cur_freq = %d\n",
			DRV_Reg32(ARMPLL2_CON0), DRV_Reg32(ARMPLL2_CON1), g_cur_freq[CA7_CLUSTER]);
		dprintk("CA15: ARMPLL_CON0 = 0x%x, ARMPLL_CON1 = 0x%x, g_cur_freq = %d\n",
			DRV_Reg32(ARMPLL_CON0), DRV_Reg32(ARMPLL_CON1), g_cur_freq[CA15_CLUSTER]);
	} else {
		dprintk("CA15: ARMPLL_CON0 = 0x%x, ARMPLL_CON1 = 0x%x, g_cur_freq = %d\n",
			DRV_Reg32(ARMPLL_CON0), DRV_Reg32(ARMPLL_CON1), g_cur_freq[CA15_CLUSTER]);
		dprintk("CA7: ARMPLL2_CON0 = 0x%x, ARMPLL2_CON1 = 0x%x, g_cur_freq = %d\n",
			DRV_Reg32(ARMPLL2_CON0), DRV_Reg32(ARMPLL2_CON1), g_cur_freq[CA7_CLUSTER]);
	}
}


#ifdef MT_DVFS_RANDOM_TEST
static int mt_cpufreq_idx_get(int num)
{
	int random = 0, mult = 0, idx;
	random = jiffies & 0xF;

	while (1) {
		if ((mult * num) >= random) {
			idx = (mult * num) - random;
			break;
		}
		mult++;
	}
	return idx;
}
#endif

static unsigned int mt_thermal_limited_verify(unsigned int target_freq, unsigned int cur_cluster)
{
	if (target_freq > g_limited_max_freq[cur_cluster]) {
		target_freq = g_limited_max_freq[cur_cluster];
		dprintk("cur_cluster = %d, g_limited_max_freq = %d, ncpu = %d\n", cur_cluster,
			g_limited_max_freq[cur_cluster], g_limited_max_ncpu[CA7_CLUSTER]);
	}
	return target_freq;
}

/**********************************
* cpufreq target callback function
***********************************/
/*************************************************
* [note]
* 1. handle frequency change request
* 2. call mt_cpufreq_set to set target frequency
**************************************************/
static int mt_cpufreq_target(struct cpufreq_policy *policy, unsigned int target_freq,
			     unsigned int relation)
{
	int i, idx;
	unsigned int cpu, cur_cluster;
	unsigned long flags;

	struct mt_cpu_freq_info next;
	struct cpufreq_freqs freqs;

	cpu = policy->cpu;

	cur_cluster = cpu_to_cluster(policy->cpu);

	if (!mt_cpufreq_ready)
		return -ENOSYS;

	if (policy->cpu >= num_possible_cpus())
		return -EINVAL;

    /******************************
    * look up the target frequency
    *******************************/
	if (cpufreq_frequency_table_target
	    (policy, mt_cpu_freqs_table[cur_cluster], target_freq, relation, &idx)) {
		return -EINVAL;
	}
#ifdef MT_DVFS_RANDOM_TEST
	idx = mt_cpufreq_idx_get(8);
#endif

	if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 1) {
		next.cpufreq_khz = mt8135_freqs_e1_0[cur_cluster][idx].cpufreq_khz;
	} else if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 2) {
		next.cpufreq_khz = mt8135_freqs_e1_1[cur_cluster][idx].cpufreq_khz;
	} else if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 3) {
		next.cpufreq_khz = mt8135_freqs_e1_2[cur_cluster][idx].cpufreq_khz;
	} else if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 4) {
		next.cpufreq_khz = mt8135_freqs_e1[cur_cluster][idx].cpufreq_khz;
	} else {
		next.cpufreq_khz = mt8135_freqs_e1_1[cur_cluster][idx].cpufreq_khz;
	}

#ifdef MT_DVFS_RANDOM_TEST
	dprintk("cpu = %d, idx = %d, freqs.old = %d, freqs.new = %d\n", cpu, idx, policy->cur,
		next.cpufreq_khz);
#endif

	freqs.old = policy->cur;
	freqs.new = next.cpufreq_khz;
	freqs.cpu = policy->cpu;

#ifndef MT_DVFS_RANDOM_TEST
    /************************************************
    * DVFS keep CA7 at 1.2GHz in earlysuspend
    *************************************************/
	if (mt_cpufreq_pause == true && cur_cluster == CA7_CLUSTER) {
		if (policy->max >= DVFS_CA7_F0)
			freqs.new = DVFS_CA7_F0;
		else
			freqs.new = policy->max;

		dprintk("cpu = %d, pause DVFS: mt_cpufreq_limit_max_freq, freqs.new = %d\n", cpu,
			freqs.new);
	}
#endif				/* MT_DVFS_RANDOM_TEST */

	freqs.new = mt_thermal_limited_verify(freqs.new, cur_cluster);

	if (freqs.new < g_limited_min_freq[cur_cluster]) {
		dprintk
		    ("cpu = %d, cannot switch CPU frequency to %d Mhz due to voltage limitation\n",
		     cpu, g_limited_min_freq[cur_cluster] / 1000);
		freqs.new = g_limited_min_freq[cur_cluster];
	}

    /************************************************
    * DVFS keep at 1.15V when PTPOD initial
    *************************************************/
	if (mt_cpufreq_ptpod_disable) {
		if (cur_cluster == CA7_CLUSTER)
			freqs.new = DVFS_CA7_F2;
		else
			freqs.new = DVFS_CA15_F2;
		dprintk("PTPOD, freqs.new = %d\n", freqs.new);
	}

    /************************************************
    * target frequency == existing frequency, skip it
    *************************************************/
	if (freqs.old == freqs.new) {
		dprintk
		    ("cpu = %d, CPU frequency from %d MHz to %d MHz (skipped) due to same frequency\n",
		     cpu, freqs.old / 1000, freqs.new / 1000);
		return 0;
	}

    /**************************************
    * search for the corresponding voltage
    ***************************************/
	next.cpufreq_volt = 0;

	for (i = 0; i < mt_cpu_freqs_num[cur_cluster]; i++) {
		if (freqs.new == mt_cpu_freqs[cur_cluster][i].cpufreq_khz) {
			next.cpufreq_volt = mt_cpu_freqs[cur_cluster][i].cpufreq_volt;
			dprintk("cpu = %d, freqs.old = %d, freqs.new = %d, mt_cpu_freqs[%d].cpufreq_khz = %d\n", cpu,
				freqs.old, freqs.new, i, mt_cpu_freqs[cur_cluster][i].cpufreq_khz);
			dprintk
			    ("cpu = %d, next.cpufreq_volt = %d, mt_cpu_freqs[%d].cpufreq_volt = %d\n",
			     cpu, next.cpufreq_volt, i, mt_cpu_freqs[cur_cluster][i].cpufreq_volt);
			break;
		}
	}

	if (next.cpufreq_volt == 0) {
		dprintk("cpu = %d, Error!! Cannot find corresponding voltage at %d Mhz\n", cpu,
			freqs.new / 1000);
		return 0;
	}

	cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);

	spin_lock_irqsave(&mt_cpufreq_lock, flags);

    /******************************
    * set to the target frequency
    *******************************/

	dprintk("freqs.old = %d, freqs.new = %d in spinlock\n", freqs.old, freqs.new);

	freqs.old = pll_get_freq(cur_cluster == CA7_CLUSTER ? ARMPLL2 : ARMPLL1);

	dprintk("freqs.old = %d from PLL\n", freqs.old);

	if (freqs.old != freqs.new)
		mt_cpufreq_set(freqs.old, freqs.new, next.cpufreq_volt, cur_cluster);

	spin_unlock_irqrestore(&mt_cpufreq_lock, flags);

	cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);

	return 0;
}

/*********************************************************
* set up frequency table and register to cpufreq subsystem
**********************************************************/
static int mt_cpufreq_init(struct cpufreq_policy *policy)
{
	u32 cur_cluster = cpu_to_cluster(policy->cpu);
	int ret = -EINVAL;

	if (policy->cpu >= num_possible_cpus())
		return -EINVAL;

	cpumask_copy(policy->cpus, topology_core_cpumask(policy->cpu));
	cpumask_copy(policy->related_cpus, policy->cpus);
	policy->shared_type = CPUFREQ_SHARED_TYPE_ANY;

    /*******************************************************
    * 1 us, assumed, will be overwrited by min_sampling_rate
    ********************************************************/
	policy->cpuinfo.transition_latency = 1000;

    /*********************************************
    * set default policy and cpuinfo, unit : Khz
    **********************************************/
	if (cur_cluster == CA7_CLUSTER) {
		policy->cpuinfo.max_freq = g_max_freq_by_ptp[CA7_CLUSTER];
		policy->cpuinfo.min_freq = DVFS_CA7_F7;
		policy->cur = g_cur_freq[CA7_CLUSTER];
		policy->max = g_max_freq_by_ptp[CA7_CLUSTER];
		policy->min = DVFS_CA7_F7;
	} else {		/* CA15 */

		policy->cpuinfo.max_freq = g_max_freq_by_ptp[CA15_CLUSTER];
		policy->cpuinfo.min_freq = DVFS_CA15_F7;
		policy->cur = g_cur_freq[CA15_CLUSTER];
		policy->max = g_max_freq_by_ptp[CA15_CLUSTER];
		policy->min = DVFS_CA15_F7;
	}

	DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS", "CPU %d initialized\n", policy->cpu);


	if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 1) {
		if (cur_cluster == CA7_CLUSTER)
			ret = mt_setup_freqs_table(policy, ARRAY_AND_SIZE(mt8135_freqs_ca7_e1_0));
		else
			ret = mt_setup_freqs_table(policy, ARRAY_AND_SIZE(mt8135_freqs_ca15_e1_0));
	} else if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 2) {
		if (cur_cluster == CA7_CLUSTER)
			ret = mt_setup_freqs_table(policy, ARRAY_AND_SIZE(mt8135_freqs_ca7_e1_1));
		else
			ret = mt_setup_freqs_table(policy, ARRAY_AND_SIZE(mt8135_freqs_ca15_e1_1));
	} else if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 3) {
		if (cur_cluster == CA7_CLUSTER)
			ret = mt_setup_freqs_table(policy, ARRAY_AND_SIZE(mt8135_freqs_ca7_e1_2));
		else
			ret = mt_setup_freqs_table(policy, ARRAY_AND_SIZE(mt8135_freqs_ca15_e1_2));
	} else if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 4) {
		if (cur_cluster == CA7_CLUSTER)
			ret = mt_setup_freqs_table(policy, ARRAY_AND_SIZE(mt8135_freqs_ca7_e1));
		else
			ret = mt_setup_freqs_table(policy, ARRAY_AND_SIZE(mt8135_freqs_ca15_e1));
	} else {
		if (cur_cluster == CA7_CLUSTER)
			ret = mt_setup_freqs_table(policy, ARRAY_AND_SIZE(mt8135_freqs_ca7_e1_1));
		else
			ret = mt_setup_freqs_table(policy, ARRAY_AND_SIZE(mt8135_freqs_ca15_e1_1));
	}

	if (ret) {
		DVFS_INFO(ANDROID_LOG_ERROR, "Power/DVFS", "failed to setup frequency table\n");
		return ret;
	}

	return 0;
}


static int mt_cpufreq_exit(struct cpufreq_policy *policy)
{
	DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS", "%s: Exited, cpu: %d\n", __func__, policy->cpu);

	return 0;
}


#ifdef MT_CPUFREQ_LATENCY_TABLE_SUPPORT

static ssize_t show_latency_table(struct cpufreq_policy *policy, char *buf)
{
	const unsigned int FREQ_TRANS_LATENCY = 30;
	int i, j;
	int num_freq = 0;
	ssize_t n = 0;
	struct cpufreq_frequency_table *table = cpufreq_frequency_get_table(policy->cpu);

	if (!table)
		return -ENODEV;

	n += snprintf(buf + n, PAGE_SIZE - n, "   From  :    To\n");
	n += snprintf(buf + n, PAGE_SIZE - n, "         : ");
	for (i = 0; table[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (n >= PAGE_SIZE)
			break;
		if (table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;
		n += snprintf(buf + n, PAGE_SIZE - n, "%9u ", table[i].frequency);
		num_freq++;
	}
	if (n >= PAGE_SIZE)
		return PAGE_SIZE;

	n += snprintf(buf + n, PAGE_SIZE - n, "\n");

	for (i = 0; table[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (n >= PAGE_SIZE)
			break;
		if (table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;

		n += snprintf(buf + n, PAGE_SIZE - n, "%9u: ", table[i].frequency);

		for (j = 0; j < num_freq; j++) {
			if (n >= PAGE_SIZE)
				break;
			n += snprintf(buf + n, PAGE_SIZE - n, "%9u ", FREQ_TRANS_LATENCY);
		}
		if (n >= PAGE_SIZE)
			break;
		n += snprintf(buf + n, PAGE_SIZE - n, "\n");
	}
	if (n >= PAGE_SIZE)
		return PAGE_SIZE;

	return n;
}

struct freq_attr cpufreq_freq_attr_latency_table = {
	.attr = {.name = "latency_table",
		 .mode = 0444,
		 },
	.show = show_latency_table,
};

#endif				/* MT_CPUFREQ_LATENCY_TABLE_SUPPORT */
#ifdef CONFIG_PERFSTATS_PERTASK_PERFREQ
struct cpufreq_frequency_table *mt_cpufreq_gettable(unsigned int cpu)
{
	int cluster = cpu_to_cluster(cpu);
	return mt_cpu_freqs_table[cluster];
}
#endif

static struct freq_attr *mt_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
#ifdef MT_CPUFREQ_LATENCY_TABLE_SUPPORT
	&cpufreq_freq_attr_latency_table,
#endif				/* MT_CPUFREQ_LATENCY_TABLE_SUPPORT */
	NULL,
};

static struct cpufreq_driver mt_cpufreq_driver = {
	.verify = mt_cpufreq_verify,
	.target = mt_cpufreq_target,
	.init = mt_cpufreq_init,
	.exit = mt_cpufreq_exit,
	.get = mt_cpufreq_get,
	.name = "mt-cpufreq",
	.flags = CPUFREQ_STICKY,
	.have_governor_per_policy = true,
	.attr = mt_cpufreq_attr,
#ifdef CONFIG_PERFSTATS_PERTASK_PERFREQ
	.get_table = mt_cpufreq_gettable,
#endif
};

/*********************************
* early suspend callback function
**********************************/
void mt_cpufreq_early_suspend(struct early_suspend *h)
{
#ifndef MT_DVFS_RANDOM_TEST

	/* When early suspend, keep max freq. */
	if (mt_cpufreq_keep_max_freq_early_suspend == true)
		mt_cpufreq_state_set(0);

#endif

	return;
}

/*******************************
* late resume callback function
********************************/
void mt_cpufreq_late_resume(struct early_suspend *h)
{
#ifndef MT_DVFS_RANDOM_TEST

	/* When early suspend, keep max freq. */
	if (mt_cpufreq_keep_max_freq_early_suspend == true)
		mt_cpufreq_state_set(1);

#endif

	return;
}

/************************************************
* API to switch back default voltage setting for PTPOD disabled
*************************************************/
void mt_cpufreq_return_default_DVS_by_ptpod(unsigned int cur_cluster)
{

	if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 1) {
		/* For PTP-OD */
		if (cur_cluster == CA7_CLUSTER) {
			mt_cpufreq_pmic_volt[CA7_CLUSTER][0] = 0x58;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][1] = 0x50;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][2] = 0x48;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][3] = 0x40;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][4] = 0x38;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][5] = 0x30;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][6] = 0x2C;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][7] = 0x28;
		} else {
			mt_cpufreq_pmic_volt[CA15_CLUSTER][0] = 0x58;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][1] = 0x54;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][2] = 0x50;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][3] = 0x48;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][4] = 0x40;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][5] = 0x38;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][6] = 0x30;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][7] = 0x28;
		}
	} else if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 3) {
		/* For PTP-OD */
		if (cur_cluster == CA7_CLUSTER) {
			mt_cpufreq_pmic_volt[CA7_CLUSTER][0] = 0x58;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][1] = 0x48;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][2] = 0x40;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][3] = 0x38;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][4] = 0x30;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][5] = 0x2C;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][6] = 0x28;
		} else {
			mt_cpufreq_pmic_volt[CA15_CLUSTER][0] = 0x58;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][1] = 0x48;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][2] = 0x40;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][3] = 0x38;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][4] = 0x30;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][5] = 0x2C;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][6] = 0x28;
		}
	} else {
		/* For PTP-OD */
		if (cur_cluster == CA7_CLUSTER) {
			mt_cpufreq_pmic_volt[CA7_CLUSTER][0] = 0x58;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][1] = 0x50;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][2] = 0x48;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][3] = 0x40;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][4] = 0x38;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][5] = 0x30;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][6] = 0x2C;
			mt_cpufreq_pmic_volt[CA7_CLUSTER][7] = 0x28;
		} else {
			mt_cpufreq_pmic_volt[CA15_CLUSTER][0] = 0x58;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][1] = 0x50;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][2] = 0x48;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][3] = 0x40;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][4] = 0x38;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][5] = 0x30;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][6] = 0x2C;
			mt_cpufreq_pmic_volt[CA15_CLUSTER][7] = 0x28;
		}

	}
	DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS", "mt_cpufreq return default DVS by ptpod\n");
}
EXPORT_SYMBOL(mt_cpufreq_return_default_DVS_by_ptpod);

/************************************************
* DVFS enable API for PTPOD
*************************************************/
void mt_cpufreq_enable_by_ptpod(void)
{
	mt_cpufreq_ptpod_disable = false;
	DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS", "mt_cpufreq enabled by ptpod\n");
}
EXPORT_SYMBOL(mt_cpufreq_enable_by_ptpod);

/************************************************
* DVFS disable API for PTPOD
*************************************************/
unsigned int mt_cpufreq_disable_by_ptpod(void)
{
	struct cpufreq_policy *policy_ca7, *policy_ca15;

	policy_ca7 = cpufreq_cpu_get(0);
	policy_ca15 = cpufreq_cpu_get(2);
	if (!policy_ca7 || !policy_ca15)
		goto no_policy;

	mt_cpufreq_ptpod_disable = true;

	cpufreq_driver_target(policy_ca7, DVFS_CA7_F2, CPUFREQ_RELATION_L);

	DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
		    "CA7 mt_cpufreq disabled by ptpod, limited freq. at %d\n", DVFS_CA7_F2);

	cpufreq_cpu_put(policy_ca7);

	cpufreq_driver_target(policy_ca15, DVFS_CA15_F2, CPUFREQ_RELATION_L);

	DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
		    "CA15 mt_cpufreq disabled by ptpod, limited freq. at %d\n", DVFS_CA15_F2);

	cpufreq_cpu_put(policy_ca15);

	return 0;

no_policy:
	DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS", "Cannot get the policy\n");
	return -EINVAL;

}
EXPORT_SYMBOL(mt_cpufreq_disable_by_ptpod);

/************************************************
* frequency adjust interface for thermal protect
*************************************************/
/******************************************************
* parameter: target power
*******************************************************/
void __ref mt_cpufreq_thermal_protect(unsigned int limited_power)
{
	int i = 0, ncpu = 0, found = 0, found_index = -1, found_performance = 0;

	struct cpufreq_policy *policy_ca7, *policy_ca15;

	ncpu = num_possible_cpus();

	if (g_thermal_type == 1) {	/* throttle according to power budget table to find the best performance */
		if (limited_power == 0) {
			g_limited_max_ncpu[MAX_CLUSTERS] = 4;
			g_limited_max_ncpu[CA7_CLUSTER] = 2;
			g_limited_max_ncpu[CA15_CLUSTER] = 2;
			g_limited_max_freq[CA7_CLUSTER] = g_max_freq_by_ptp[CA7_CLUSTER];
			g_limited_max_freq[CA15_CLUSTER] = g_max_freq_by_ptp[CA15_CLUSTER];

			mt_hotplug_mechanism_thermal_protect(g_limited_max_ncpu[CA7_CLUSTER],
							     g_limited_max_ncpu[CA15_CLUSTER]);

			for (i = 1; i < num_possible_cpus(); i++) {
				if (!cpu_online(i))
					cpu_up(i);
			}

			if (cpu_online(0)) {
				policy_ca7 = cpufreq_cpu_get(0);
				if (!policy_ca7)
					goto no_policy;
				cpufreq_driver_target(policy_ca7, g_limited_max_freq[CA7_CLUSTER],
						      CPUFREQ_RELATION_L);
				cpufreq_cpu_put(policy_ca7);	/* ca7 */
			}
			if (cpu_online(2)) {
				policy_ca15 = cpufreq_cpu_get(2);
				if (!policy_ca15)
					goto no_policy;
				cpufreq_driver_target(policy_ca15, g_limited_max_freq[CA15_CLUSTER],
						      CPUFREQ_RELATION_L);
				cpufreq_cpu_put(policy_ca15);	/* ca15 */
			} else if (cpu_online(3)) {
				policy_ca15 = cpufreq_cpu_get(3);
				if (!policy_ca15)
					goto no_policy;
				cpufreq_driver_target(policy_ca15, g_limited_max_freq[CA15_CLUSTER],
						      CPUFREQ_RELATION_L);
				cpufreq_cpu_put(policy_ca15);	/* ca15 */
			}


			DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
				    "thermal limit g_limited_max_freq[CA7_CLUSTER] = %d, g_limited_max_ncpu[CA7_CLUSTER] = %d\n",
				    g_limited_max_freq[CA7_CLUSTER],
				    g_limited_max_ncpu[CA7_CLUSTER]);
			DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
				    "thermal limit g_limited_max_freq[CA15_CLUSTER] = %d, g_limited_max_ncpu[CA15_CLUSTER] = %d\n",
				    g_limited_max_freq[CA15_CLUSTER],
				    g_limited_max_ncpu[CA15_CLUSTER]);

		} else {
			for (i = 0;
			     i <
			     (mt_cpu_freqs_num[CA7_CLUSTER] * mt_cpu_freqs_num[CA15_CLUSTER] * 4 +
			      mt_cpu_freqs_num[CA7_CLUSTER] * 2); i++) {
				if (mt_cpu_power[i].cpufreq_power <= limited_power
				    && mt_cpu_power[i].cpufreq_khz > found_performance) {
					/* xlog_printk(ANDROID_LOG_INFO, "Power/DVFS", "Found %d, %d, %d, %d\n", mt_cpu_power[i].cpufreq_power, limited_power, mt_cpu_power[i].cpufreq_khz, found_performance); */
					found_performance = mt_cpu_power[i].cpufreq_khz;
					found = 1;
					found_index = i;
				}
			}

			if (!found) {
				DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
					    "Could not find suitable DVFS OPP, so choose the last one\n");
				DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
					    "thermal limit g_limited_max_freq[CA7_CLUSTER] = %d, g_limited_max_ncpu[CA7_CLUSTER] = %d\n",
					    g_limited_max_freq[CA7_CLUSTER],
					    g_limited_max_ncpu[CA7_CLUSTER]);
				DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
					    "thermal limit g_limited_max_freq[CA15_CLUSTER] = %d, g_limited_max_ncpu[CA15_CLUSTER] = %d\n",
					    g_limited_max_freq[CA15_CLUSTER],
					    g_limited_max_ncpu[CA15_CLUSTER]);
				found_index = mt_cpu_freqs_num[CA7_CLUSTER] * mt_cpu_freqs_num[CA15_CLUSTER] * 4 + mt_cpu_freqs_num[CA7_CLUSTER] * 2 - 1;	/* last index */
				found = 1;
			}

			if (!found) {
				DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
					    "thermal limit failed\n");
			} else {
				g_limited_max_ncpu[CA7_CLUSTER] =
				    mt_cpu_power[found_index].cpufreq_ncpu_ca7;
				g_limited_max_ncpu[CA15_CLUSTER] =
				    mt_cpu_power[found_index].cpufreq_ncpu_ca15;
				g_limited_max_freq[CA7_CLUSTER] =
				    mt_cpu_power[found_index].cpufreq_khz_ca7;
				g_limited_max_freq[CA15_CLUSTER] =
				    mt_cpu_power[found_index].cpufreq_khz_ca15;

				/* add info for user space hotplug. */
				mt_hotplug_mechanism_thermal_protect(g_limited_max_ncpu
								     [CA7_CLUSTER],
								     g_limited_max_ncpu
								     [CA15_CLUSTER]);

				if (g_limited_max_ncpu[CA15_CLUSTER] < 2) {
					for (i = 3; i >= 2 + g_limited_max_ncpu[CA15_CLUSTER]; i--) {
						DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
							    "turn off CPU%d due to thermal protection\n",
							    i);
						cpu_down(i);
					}
				}
				/* CA7 */
				if (g_limited_max_ncpu[CA7_CLUSTER] == 1) {
					DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
						    "turn off CPU1 due to thermal protection\n");
					cpu_down(1);
				}

				if (cpu_online(0)) {
					policy_ca7 = cpufreq_cpu_get(0);
					if (!policy_ca7)
						goto no_policy;
					cpufreq_driver_target(policy_ca7,
							      g_limited_max_freq[CA7_CLUSTER],
							      CPUFREQ_RELATION_L);
					cpufreq_cpu_put(policy_ca7);	/* ca7 */
				}
				if (cpu_online(2)) {
					policy_ca15 = cpufreq_cpu_get(2);
					if (!policy_ca15)
						goto no_policy;
					cpufreq_driver_target(policy_ca15,
							      g_limited_max_freq[CA15_CLUSTER],
							      CPUFREQ_RELATION_L);
					cpufreq_cpu_put(policy_ca15);	/* ca15 */
				} else if (cpu_online(3)) {
					policy_ca15 = cpufreq_cpu_get(3);
					if (!policy_ca15)
						goto no_policy;
					cpufreq_driver_target(policy_ca15,
							      g_limited_max_freq[CA15_CLUSTER],
							      CPUFREQ_RELATION_L);
					cpufreq_cpu_put(policy_ca15);	/* ca15 */
				}

				DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
					    "Suitable DVFS OPP: %d\n", found_index);
				DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
					    "thermal limit g_limited_max_freq[CA7_CLUSTER] = %d, g_limited_max_ncpu[CA7_CLUSTER] = %d\n",
					    g_limited_max_freq[CA7_CLUSTER],
					    g_limited_max_ncpu[CA7_CLUSTER]);
				DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
					    "thermal limit g_limited_max_freq[CA15_CLUSTER] = %d, g_limited_max_ncpu[CA15_CLUSTER] = %d\n",
					    g_limited_max_freq[CA15_CLUSTER],
					    g_limited_max_ncpu[CA15_CLUSTER]);

			}
		}
	} else {
		DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS", "No thermal throttle\n");
	}

	return;

no_policy:
	DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS", "Cannot get the policy\n");
	return;

}
EXPORT_SYMBOL(mt_cpufreq_thermal_protect);

/***************************
* show current DVFS stauts
****************************/
static int mt_cpufreq_state_show(struct seq_file *s, void *v)
{
	if (!mt_cpufreq_pause)
		seq_puts(s, "DVFS enabled\n");
	else
		seq_puts(s, "DVFS disabled\n");

	return 0;
}

static int mt_cpufreq_state_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_cpufreq_state_show, NULL);
}


/************************************
* set DVFS stauts by sysfs interface
*************************************/
static ssize_t mt_cpufreq_state_write(struct file *file, const char *buffer, size_t count,
				      loff_t *data)
{
	int enabled = 0;

	if (sscanf(buffer, "%d", &enabled) == 1) {
		if (enabled == 1) {
			mt_cpufreq_state_set(1);
		} else if (enabled == 0) {
			mt_cpufreq_state_set(0);
		} else {
			DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
				    "bad argument!! argument should be \"1\" or \"0\"\n");
		}
	} else {
		DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
			    "bad argument!! argument should be \"1\" or \"0\"\n");
	}

	return count;
}

/****************************
* show current limited freq
*****************************/
static int mt_cpufreq_limited_power_show(struct seq_file *s, void *v)
{
	seq_printf(s, "CA7: g_limited_max_freq = %d, g_limited_max_ncpu = %d\n",
		   g_limited_max_freq[CA7_CLUSTER], g_limited_max_ncpu[CA7_CLUSTER]);
	seq_printf(s, "CA15: g_limited_max_freq = %d, g_limited_max_ncpu = %d\n",
		   g_limited_max_freq[CA15_CLUSTER], g_limited_max_ncpu[CA15_CLUSTER]);

	return 0;
}

static int mt_cpufreq_limited_power_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_cpufreq_limited_power_show, NULL);
}


/**********************************
* limited power for thermal protect
***********************************/
static ssize_t mt_cpufreq_limited_power_write(struct file *file, const char *buffer, size_t count,
					      loff_t *data)
{
	unsigned int power = 0;

	if (sscanf(buffer, "%u", &power) == 1) {
		mt_cpufreq_thermal_protect(power);
		return count;
	} else {
		DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
			    "bad argument!! please provide the maximum limited power\n");
	}

	return -EINVAL;
}

/***************************
* show current debug status
****************************/
static int mt_cpufreq_debug_show(struct seq_file *s, void *v)
{
	if (mt_cpufreq_debug)
		seq_puts(s, "cpufreq debug enabled\n");
	else
		seq_puts(s, "cpufreq debug disabled\n");

	return 0;
}

static int mt_cpufreq_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_cpufreq_debug_show, NULL);
}


/***********************
* enable debug message
************************/
static ssize_t mt_cpufreq_debug_write(struct file *file, const char *buffer, size_t count,
				      loff_t *data)
{
	int debug = 0;

	if (sscanf(buffer, "%d", &debug) == 1) {
		if (debug == 0) {
			mt_cpufreq_debug = 0;
			return count;
		} else if (debug == 1) {
			mt_cpufreq_debug = 1;
			return count;
		} else {
			DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
				    "bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
		}
	} else {
		DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
			    "bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
	}

	return -EINVAL;
}

/****************************
* show current limited freq
*****************************/
static int mt_cpufreq_early_suspend_max_freq_show(struct seq_file *s, void *v)
{
	if (mt_cpufreq_keep_max_freq_early_suspend)
		seq_puts(s, "Keep CA7 1.2Ghz in early suspend\n");
	else
		seq_puts(s, "DVFS in early suspend\n");

	return 0;
}

static int mt_cpufreq_early_suspend_max_freq_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_cpufreq_early_suspend_max_freq_show, NULL);
}

/**********************************
* Set Fixed CPU frequency in early suspend
***********************************/
static ssize_t mt_cpufreq_early_suspend_max_freq_write(struct file *file, const char *buffer,
						       size_t count, loff_t *data)
{
	int keep_max_freq_early_suspend = 0;

	if (sscanf(buffer, "%d", &keep_max_freq_early_suspend) == 1) {
		if (keep_max_freq_early_suspend == 0) {
			mt_cpufreq_keep_max_freq_early_suspend = false;
			return count;
		} else if (keep_max_freq_early_suspend == 1) {
			mt_cpufreq_keep_max_freq_early_suspend = true;
			return count;
		} else {
			DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
				    "bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
		}
	} else {
		DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS",
			    "bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
	}

	return -EINVAL;

}


/***************************
* show cpufreq power info
****************************/
static int mt_cpufreq_power_dump_show(struct seq_file *s, void *v)
{
	loff_t *spos = (loff_t *) v;
	loff_t i = *spos;

	if (mt_cpu_power == NULL) {
		seq_puts(s, "Power table is unavailable\n");

		return 0;
	}

	if (i < g_thermal_opp_num) {
		seq_printf(s, "%8d, %8d, %8d, %2d, %2d, %2d, %5d, %5d, %5d\n",
			   mt_cpu_power[i].cpufreq_khz_ca7,
			   mt_cpu_power[i].cpufreq_khz_ca15,
			   mt_cpu_power[i].cpufreq_khz,
			   mt_cpu_power[i].cpufreq_ncpu_ca7,
			   mt_cpu_power[i].cpufreq_ncpu_ca15,
			   mt_cpu_power[i].cpufreq_ncpu,
			   mt_cpu_power[i].cpufreq_power_ca7,
			   mt_cpu_power[i].cpufreq_power_ca15, mt_cpu_power[i].cpufreq_power);
	}

	return 0;
}

static void *mt_cpufreq_power_dump_start(struct seq_file *s, loff_t *pos)
{
	loff_t *spos;

	if (*pos >= g_thermal_opp_num)
		return NULL;

	spos = kmalloc(sizeof(loff_t), GFP_KERNEL);
	if (!spos)
		return NULL;

	*spos = *pos;
	return spos;
}


static void *mt_cpufreq_power_dump_next(struct seq_file *s, void *v, loff_t *pos)
{
	loff_t *spos = (loff_t *) v;

	++(*spos);
	*pos = *spos;

	if (*pos >= g_thermal_opp_num)
		return NULL;

	return spos;
}


static void mt_cpufreq_power_dump_stop(struct seq_file *s, void *v)
{
	kfree(v);
}


static const struct seq_operations mt_cpufreq_power_dump_seq_ops = {
	.start = mt_cpufreq_power_dump_start,
	.next = mt_cpufreq_power_dump_next,
	.stop = mt_cpufreq_power_dump_stop,
	.show = mt_cpufreq_power_dump_show
};

static int mt_cpufreq_power_dump_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &mt_cpufreq_power_dump_seq_ops);
}

static const struct file_operations mt_cpufreq_debug_fops = {
	.owner = THIS_MODULE,
	.write = mt_cpufreq_debug_write,
	.open = mt_cpufreq_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations mt_cpufreq_limited_power_fops = {
	.owner = THIS_MODULE,
	.write = mt_cpufreq_limited_power_write,
	.open = mt_cpufreq_limited_power_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations mt_cpufreq_state_fops = {
	.owner = THIS_MODULE,
	.write = mt_cpufreq_state_write,
	.open = mt_cpufreq_state_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations mt_cpufreq_power_dump_fops = {
	.owner = THIS_MODULE,
	.open = mt_cpufreq_power_dump_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static const struct file_operations mt_cpufreq_early_suspend_max_freq_fops = {
	.owner = THIS_MODULE,
	.write = mt_cpufreq_early_suspend_max_freq_write,
	.open = mt_cpufreq_early_suspend_max_freq_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#ifdef MT_POWER_API_SUPPORT
int get_wfi_power(unsigned int cpu, unsigned int freq)
{
	int i = 0;
	if (cpu == 0 || cpu == 1) {
		/* CA7 */
		for (i = 0; i < mt_cpu_freqs_num[CA7_CLUSTER]; i++) {
			if (mt_percpu_power[CA7_CLUSTER][i].cpufreq_khz != freq)
				continue;

			return mt_percpu_power[CA7_CLUSTER][i].cpufreq_power_wfi;
		}
		return -1;	/* no matched power given the freq */
	} else {
		/* CA15 */
		for (i = 0; i < mt_cpu_freqs_num[CA7_CLUSTER]; i++) {
			if (mt_percpu_power[CA15_CLUSTER][i].cpufreq_khz != freq)
				continue;

			return mt_percpu_power[CA15_CLUSTER][i].cpufreq_power_wfi;
		}
		return -1;	/* no matched power given the freq */
	}
}
EXPORT_SYMBOL(get_wfi_power);

int get_nonidle_power(unsigned int cpu, unsigned int freq)
{
	int i = 0;
	if (cpu == 0 || cpu == 1) {
		/* CA7 */
		for (i = 0; i < mt_cpu_freqs_num[CA7_CLUSTER]; i++) {
			if (mt_percpu_power[CA7_CLUSTER][i].cpufreq_khz != freq)
				continue;

			return mt_percpu_power[CA7_CLUSTER][i].cpufreq_power_max;
		}
		return -1;	/* no matched power given the freq */
	} else {
		/* CA15 */
		for (i = 0; i < mt_cpu_freqs_num[CA7_CLUSTER]; i++) {
			if (mt_percpu_power[CA15_CLUSTER][i].cpufreq_khz != freq)
				continue;

			return mt_percpu_power[CA15_CLUSTER][i].cpufreq_power_max;
		}
		return -1;	/* no matched power given the freq */
	}
}
EXPORT_SYMBOL(get_nonidle_power);
#endif				/* MT_POWER_API_SUPPORT */

/*******************************************
* cpufrqe platform driver callback function
********************************************/
static int mt_cpufreq_pdrv_probe(struct platform_device *pdev)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	mt_cpufreq_early_suspend_handler.suspend = mt_cpufreq_early_suspend;
	mt_cpufreq_early_suspend_handler.resume = mt_cpufreq_late_resume;
	register_early_suspend(&mt_cpufreq_early_suspend_handler);
#endif

    /************************************************
    * Check PTP level to define default max freq
    *************************************************/
	g_cpufreq_get_ptp_level[CA7_CLUSTER] = PTP_get_ptp_ca7_level();
	g_cpufreq_get_ptp_level[CA15_CLUSTER] = PTP_get_ptp_ca15_level();
	g_chip_sw_ver = mt_get_chip_sw_ver();
	pmic_read_interface(VCORE_CON10, &g_vcore, 0x7F, 0);

	if (g_cpufreq_get_ptp_level[CA15_CLUSTER] == 3
		   && g_cpufreq_get_ptp_level[CA7_CLUSTER] <= 2) {
		g_cpufreq_get_ptp_level[MAX_CLUSTERS] = 1;
	} else if (g_cpufreq_get_ptp_level[CA15_CLUSTER] >= 0
		   && g_cpufreq_get_ptp_level[CA15_CLUSTER] <= 2) {
		g_cpufreq_get_ptp_level[MAX_CLUSTERS] = 2;
	} else if (g_cpufreq_get_ptp_level[CA15_CLUSTER] == 9
		   && g_cpufreq_get_ptp_level[CA7_CLUSTER] == 9) {
		g_cpufreq_get_ptp_level[MAX_CLUSTERS] = 3;
	} else {
		if (g_chip_sw_ver <= CHIP_SW_VER_01) {
			g_cpufreq_get_ptp_level[MAX_CLUSTERS] = 4;
		} else {
			g_cpufreq_get_ptp_level[MAX_CLUSTERS] = 2;
		}
	}


	if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 1) {
		g_max_freq_by_ptp[CA7_CLUSTER] = DVFS_CA7_F0;
		g_max_freq_by_ptp[CA15_CLUSTER] = DVFS_CA15_F0_0;
	} else if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 2) {
		g_max_freq_by_ptp[CA7_CLUSTER] = DVFS_CA7_F0;
		g_max_freq_by_ptp[CA15_CLUSTER] = DVFS_CA15_F0_1;
	} else if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 3) {
		g_max_freq_by_ptp[CA7_CLUSTER] = DVFS_CA7_F0_2;
		g_max_freq_by_ptp[CA15_CLUSTER] = DVFS_CA15_F0_2;
	} else if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 4) {
		g_max_freq_by_ptp[CA7_CLUSTER] = DVFS_CA7_F0;
		g_max_freq_by_ptp[CA15_CLUSTER] = DVFS_CA15_F0;
	} else {
		g_max_freq_by_ptp[CA7_CLUSTER] = DVFS_CA7_F0;
		g_max_freq_by_ptp[CA15_CLUSTER] = DVFS_CA15_F0_1;
	}

    /************************************************
    * Check chip version to define Clock Mux sel
    *************************************************/
	if (g_chip_sw_ver <= CHIP_SW_VER_01) {
		g_top_ckmuxsel = TOP_CKMUXSEL_CLKSQ;
		g_top_pll = -1;
	} else {
		mt65xx_reg_sync_writel((*(volatile u32 *)CLK_MISC_CFG_2) | (0xA << 25),
				       CLK_MISC_CFG_2);
		g_top_ckmuxsel = TOP_CKMUXSEL_MAINPLL;
		g_top_pll = MAINPLL;
	}


    /************************************************
    * voltage scaling need to wait PMIC driver ready
    *************************************************/
	mt_cpufreq_ready = true;

	/* CA7: Cluster0 */
	g_cur_freq[CA7_CLUSTER] = DVFS_CA7_F2;
	g_cur_cpufreq_volt[CA7_CLUSTER] = DVFS_V1150;
	g_limited_max_freq[CA7_CLUSTER] = g_max_freq_by_ptp[CA7_CLUSTER];
	g_limited_min_freq[CA7_CLUSTER] = DVFS_CA7_F7;

	/* CA15: Cluster1 */
	g_cur_freq[CA15_CLUSTER] = DVFS_CA15_F2;
	g_cur_cpufreq_volt[CA15_CLUSTER] = DVFS_V1150;
	g_limited_max_freq[CA15_CLUSTER] = g_max_freq_by_ptp[CA15_CLUSTER];
	g_limited_min_freq[CA15_CLUSTER] = DVFS_CA15_F7;

	DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS", "mediatek cpufreq initialized\n");
	DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS", "Current VCore: 0x%x\n", g_vcore);

/* Remove: init in preloader */
	pmic_config_interface(0x330, 0x1, 0x2, 0x1);	/* switch VCA7 to HW control */
	pmic_config_interface(0x330, 0x1, 0x1, 0x1);	/* switch VCA7 to HW control */
	pmic_config_interface(0x21E, 0x1, 0x2, 0x1);	/* switch VCA15 to HW control */
	pmic_config_interface(0x21E, 0x1, 0x1, 0x1);	/* switch VCA15 to HW control */


	mt65xx_reg_sync_writel(0x033A, PMIC_WRAP_DVFS_ADR0);
	mt65xx_reg_sync_writel(0x0228, PMIC_WRAP_DVFS_ADR1);
	mt65xx_reg_sync_writel(0x033A, PMIC_WRAP_DVFS_ADR2);
	mt65xx_reg_sync_writel(0x0228, PMIC_WRAP_DVFS_ADR3);
	mt65xx_reg_sync_writel(0x033A, PMIC_WRAP_DVFS_ADR4);
	mt65xx_reg_sync_writel(0x033A, PMIC_WRAP_DVFS_ADR5);
	mt65xx_reg_sync_writel(0x027A, PMIC_WRAP_DVFS_ADR6);
	mt65xx_reg_sync_writel(0x027A, PMIC_WRAP_DVFS_ADR7);

	mt65xx_reg_sync_writel(0x48, PMIC_WRAP_DVFS_WDATA0);	/* 1.15V VCA7  DVFS */
	mt65xx_reg_sync_writel(0x48, PMIC_WRAP_DVFS_WDATA1);	/* 1.15V VCA15 DVFS */
	mt65xx_reg_sync_writel(0x48, PMIC_WRAP_DVFS_WDATA2);	/* Reserved */
	mt65xx_reg_sync_writel(0x48, PMIC_WRAP_DVFS_WDATA3);	/* Reserved */
	mt65xx_reg_sync_writel(0x58, PMIC_WRAP_DVFS_WDATA4);	/* 1.25V VCA7  SPM */
	mt65xx_reg_sync_writel(0x18, PMIC_WRAP_DVFS_WDATA5);	/* 0.85V VCA7  SPM */
	mt65xx_reg_sync_writel(g_vcore, PMIC_WRAP_DVFS_WDATA6);	/* VCORE SPM reading from init vcore */
	mt65xx_reg_sync_writel(0x28, PMIC_WRAP_DVFS_WDATA7);	/* 0.95V VCORE SPM */
	mb();

	spm_dvfs_ctrl_volt(0);	/* default set CA7 to 1.15V */
	spm_dvfs_ctrl_volt(1);	/* default set CA15 to 1.15V */

	if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 1) {
		/* For PTP-OD */
		mt_cpufreq_pmic_volt[CA7_CLUSTER][0] = 0x58;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][1] = 0x50;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][2] = 0x48;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][3] = 0x40;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][4] = 0x38;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][5] = 0x30;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][6] = 0x2C;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][7] = 0x28;

		mt_cpufreq_pmic_volt[CA15_CLUSTER][0] = 0x58;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][1] = 0x54;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][2] = 0x50;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][3] = 0x48;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][4] = 0x40;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][5] = 0x38;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][6] = 0x30;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][7] = 0x28;
	} else if (g_cpufreq_get_ptp_level[MAX_CLUSTERS] == 3) {
		mt_cpufreq_pmic_volt[CA7_CLUSTER][0] = 0x58;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][1] = 0x48;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][2] = 0x40;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][3] = 0x38;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][4] = 0x30;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][5] = 0x2C;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][6] = 0x28;

		mt_cpufreq_pmic_volt[CA15_CLUSTER][0] = 0x58;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][1] = 0x48;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][2] = 0x40;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][3] = 0x38;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][4] = 0x30;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][5] = 0x2C;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][6] = 0x28;
	} else {
		mt_cpufreq_pmic_volt[CA7_CLUSTER][0] = 0x58;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][1] = 0x50;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][2] = 0x48;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][3] = 0x40;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][4] = 0x38;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][5] = 0x30;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][6] = 0x2C;
		mt_cpufreq_pmic_volt[CA7_CLUSTER][7] = 0x28;

		mt_cpufreq_pmic_volt[CA15_CLUSTER][0] = 0x58;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][1] = 0x50;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][2] = 0x48;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][3] = 0x40;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][4] = 0x38;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][5] = 0x30;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][6] = 0x2C;
		mt_cpufreq_pmic_volt[CA15_CLUSTER][7] = 0x28;
	}

	return cpufreq_register_driver(&mt_cpufreq_driver);
}

#if 0
static int mt_cpufreq_suspend(struct device *device)
{
	DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS", "mt_cpufreq_suspend\n");

	return 0;
}

static int mt_cpufreq_resume(struct device *device)
{
	DVFS_INFO(ANDROID_LOG_INFO, "Power/DVFS", "mt_cpufreq_resume\n");

	return 0;
}
#endif

/***************************************
* this function should never be called
****************************************/
static int mt_cpufreq_pdrv_remove(struct platform_device *pdev)
{
	return 0;
}

const struct dev_pm_ops mt_cpufreq_pdrv_pm_ops = {
#if 0
	.suspend = mt_cpufreq_suspend,
	.resume = mt_cpufreq_resume,
	.freeze = mt_cpufreq_suspend,
	.thaw = mt_cpufreq_resume,
	.poweroff = NULL,
	.restore = mt_cpufreq_resume,
	.restore_noirq = NULL,
#endif
};

static struct platform_driver mt_cpufreq_pdrv = {
	.probe = mt_cpufreq_pdrv_probe,
	.remove = mt_cpufreq_pdrv_remove,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
#if 0
#ifdef CONFIG_PM
		   .pm = &mt_cpufreq_pdrv_pm_ops,
#endif
#endif
		   .name = "mt-cpufreq",
		   .owner = THIS_MODULE,
		   },

};

/***********************************************************
* cpufreq initialization to register cpufreq platform driver
************************************************************/
static int __init mt_cpufreq_pdrv_init(void)
{
	int ret = 0;

	struct proc_dir_entry *mt_entry = NULL;
	struct proc_dir_entry *mt_cpufreq_dir = NULL;

	mt_cpufreq_dir = proc_mkdir("cpufreq", NULL);
	if (!mt_cpufreq_dir) {
		pr_err("[%s]: mkdir /proc/cpufreq failed\n", __func__);
	} else {
		mt_entry =
		    proc_create("cpufreq_debug", S_IRUGO | S_IWUSR | S_IWGRP, mt_cpufreq_dir,
				&mt_cpufreq_debug_fops);
		if (!mt_entry) {
			pr_err("[%s]: mkdir /proc/cpufreq/cpufreq_debug failed\n", __func__);
		}

		mt_entry =
		    proc_create("cpufreq_limited_power", S_IRUGO | S_IWUSR | S_IWGRP,
				mt_cpufreq_dir, &mt_cpufreq_limited_power_fops);
		if (!mt_entry) {
			pr_err("[%s]: mkdir /proc/cpufreq/cpufreq_limited_power failed\n",
			       __func__);
		}

		mt_entry =
		    proc_create("cpufreq_state", S_IRUGO | S_IWUSR | S_IWGRP, mt_cpufreq_dir,
				&mt_cpufreq_state_fops);
		if (!mt_entry) {
			pr_err("[%s]: mkdir /proc/cpufreq/cpufreq_state failed\n", __func__);
		}

		mt_entry =
		    proc_create("cpufreq_power_dump", S_IRUGO | S_IWUSR | S_IWGRP, mt_cpufreq_dir,
				&mt_cpufreq_power_dump_fops);
		if (!mt_entry) {
			pr_err("[%s]: mkdir /proc/cpufreq/cpufreq_power_dump failed\n", __func__);
		}

		mt_entry =
		    proc_create("cpufreq_early_suspend_max_freq", S_IRUGO | S_IWUSR | S_IWGRP,
				mt_cpufreq_dir, &mt_cpufreq_early_suspend_max_freq_fops);
		if (!mt_entry) {
			pr_err("[%s]: mkdir /proc/cpufreq/cpufreq_early_suspend_max_freq failed\n",
			       __func__);
		}
	}


	ret = platform_driver_register(&mt_cpufreq_pdrv);
	if (ret) {
		DVFS_INFO(ANDROID_LOG_ERROR, "Power/DVFS", "failed to register cpufreq driver\n");
		return ret;
	} else {
		DVFS_INFO(ANDROID_LOG_ERROR, "Power/DVFS", "cpufreq driver registration done\n");
		DVFS_INFO(ANDROID_LOG_ERROR, "Power/DVFS",
			    "g_cpufreq_get_ptp_level[CA7_CLUSTER] = %d\n",
			    g_cpufreq_get_ptp_level[CA7_CLUSTER]);
		DVFS_INFO(ANDROID_LOG_ERROR, "Power/DVFS",
			    "g_cpufreq_get_ptp_level[CA15_CLUSTER] = %d\n",
			    g_cpufreq_get_ptp_level[CA15_CLUSTER]);
		DVFS_INFO(ANDROID_LOG_ERROR, "Power/DVFS",
			    "g_cpufreq_get_ptp_level[MAX_CLUSTERS] = %d\n",
			    g_cpufreq_get_ptp_level[MAX_CLUSTERS]);
		return 0;
	}
}

static void __exit mt_cpufreq_pdrv_exit(void)
{
	cpufreq_unregister_driver(&mt_cpufreq_driver);
}
postcore_initcall(mt_cpufreq_pdrv_init);
#ifdef MT_CPUSTRESS_TEST
module_param(g_ca7_delta_v, int, 0644);
module_param(g_ca15_delta_v, int, 0644);
#endif
module_param(g_thermal_type, int, 0644);
module_exit(mt_cpufreq_pdrv_exit);

MODULE_DESCRIPTION("MediaTek CPU Frequency Scaling driver");
MODULE_LICENSE("GPL");
