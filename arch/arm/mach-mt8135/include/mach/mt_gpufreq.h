#ifndef _MT_GPUFREQ_H
#define _MT_GPUFREQ_H

#include <linux/module.h>

#define CONFIG_SUPPORT_MET_GPU 1
#if CONFIG_SUPPORT_MET_GPU
#include <linux/export.h>
#include <linux/met_drv.h>
#endif


/*********************
* Clock Mux Register
**********************/
#define CLK26CALI       (0xF00001C0)
#define CKSTA_REG       (0xF00001c4)
#define MBIST_CFG_0     (0xF0000208)
#define MBIST_CFG_1     (0xF000020C)
#define MBIST_CFG_2     (0xF0000210)
#define MBIST_CFG_3     (0xF0000214)
#define MBIST_CFG_4     (0xF0000218)
#define MBIST_CFG_5     (0xF000021C)
#define MBIST_CFG_6     (0xF0000220)
#define MBIST_CFG_7     (0xF0000224)

/*********************
* GPU Frequency List
**********************/

#define GPU_DVFS_OVER_F450     (450000)	/* (1350000) */
#define GPU_DVFS_OVER_F475     (475000)	/* (1425000) */
#define GPU_DVFS_OVER_F500     (500000)	/* (1500000) */
#define GPU_DVFS_OVER_F550     (550000)	/* (1650000) */


#define GPU_DVFS_F1     (395000)	/* KHz  GPU_MMPLL_D3 @1185 */
#define GPU_DVFS_F2     (296250)	/* KHz  GPU_MMPLL_D4 */
#define GPU_DVFS_F3     (257833)	/* KHz  GPU_MMPLL_D3 @773.5 */
#define GPU_DVFS_F4     (249600)	/* KHz  GPU_UNIVPLL_D5 */
#define GPU_DVFS_F5     (237000)	/* KHz  GPU_MMPLL_D5 @1185 */
#define GPU_DVFS_F6     (197500)	/* KHz  GPU_MMPLL_D6 @1185 */
#define GPU_DVFS_F7     (169286)	/* KHz  GPU_MMPLL_D7 @1185 */
#define GPU_DVFS_F8     (156000)	/* KHz  GPU_UNIVPLL1_D4 */


#define GPU_BONDING_000  GPU_DVFS_F1
#define GPU_BONDING_001  (286000)
#define GPU_BONDING_010  (357000)
#define GPU_BONDING_011  (400000)
#define GPU_BONDING_100  (455000)
#define GPU_BONDING_101  (476000)
#define GPU_BONDING_110  (476000)
#define GPU_BONDING_111  (476000)


/**************************
* MFG Clock Mux Selection
***************************/
/* mt8135 begin */
/* MTKsystem init setup begin */

/* MFG Clock Mux Selection */
#define GPU_CKSQ_MUX_CK    (0)
#define GPU_UNIVPLL1_D4    (1)
#define GPU_SYSPLL_D2      (2)
#define GPU_SYSPLL_D2P5    (3)
#define GPU_SYSPLL_D3      (4)
#define GPU_UNIVPLL_D5     (5)
#define GPU_UNIVPLL1_D2    (6)
#define GPU_MMPLL_D2       (7)
#define GPU_MMPLL_D3       (8)
#define GPU_MMPLL_D4       (9)
#define GPU_MMPLL_D5       (10)
#define GPU_MMPLL_D6       (11)
#define GPU_MMPLL_D7       (12)

/* MFG Mem clock selection */
#define GPU_MEM_CKSQ_MUX_CK  (0)
#define GPU_MEM_TOP_SMI      (1)
#define GPU_MEM_TOP_MFG      (2)
#define GPU_MEM_TOP_MEM      (3)


#define GPU_FMFG    (0)
#define GPU_FMEM    (1)		/* GPU internal mem clock */


#define MFG_CONFIG_REG_BASE       0xF0206000
#define MFG_PDN_STATUS           (MFG_CONFIG_REG_BASE + 0x0000)
#define MFG_PDN_SET              (MFG_CONFIG_REG_BASE + 0x0004)
#define MFG_PDN_CLR              (MFG_CONFIG_REG_BASE + 0x0008)
#define MFG_PDN_RESET            (MFG_CONFIG_REG_BASE + 0x000C)
#define MFG_DCM                  (MFG_CONFIG_REG_BASE + 0x0010)
#define MFG_DRAM_ACCESS_PATH     (MFG_CONFIG_REG_BASE + 0x0020)

/* mt8135 end */

#define GPU_MMPLL_1650_D3_CLOCK    (GPU_DVFS_OVER_F550)
#define GPU_MMPLL_1500_D3_CLOCK    (GPU_DVFS_OVER_F500)
#define GPU_MMPLL_1425_D3_CLOCK    (GPU_DVFS_OVER_F475)
#define GPU_MMPLL_1350_D3_CLOCK    (GPU_DVFS_OVER_F450)

#define GPU_MMPLL_D3_CLOCK       (GPU_DVFS_F1)
#define GPU_MMPLL_D4_CLOCK       (GPU_DVFS_F2)
#define GPU_MMPLL_773_D3_CLOCK   (GPU_DVFS_F3)
#define GPU_UNIVPLL_D5_CLOCK     (GPU_DVFS_F4)
#define GPU_MMPLL_D5_CLOCK       (GPU_DVFS_F5)
#define GPU_MMPLL_D6_CLOCK       (GPU_DVFS_F6)
#define GPU_MMPLL_D7_CLOCK       (GPU_DVFS_F7)
#define GPU_UNIVPLL1_D4_CLOCK    (GPU_DVFS_F8)


/******************************
* MFG Power Voltage Selection
*******************************/

#define GPU_POWER_VGPU_1_15V   (0x48)
#define GPU_POWER_VGPU_1_10V   (0x40)
#define GPU_POWER_VGPU_1_05V   (0x38)
#define GPU_POWER_VGPU_1_025V  (0x34)
#define GPU_POWER_VGPU_1_00V   (0x30)
#define GPU_POWER_VGPU_0_95V   (0x28)
#define GPU_POWER_VGPU_0_90V   (0x20)


#define GPU_POWER_VGPU_1_15V_VOL   (1150)
#define GPU_POWER_VGPU_1_10V_VOL   (1100)
#define GPU_POWER_VGPU_1_05V_VOL   (1050)
#define GPU_POWER_VGPU_1_025V_VOL  (1025)
#define GPU_POWER_VGPU_1_00V_VOL   (1000)
#define GPU_POWER_VGPU_0_95V_VOL   (950)
#define GPU_POWER_VGPU_0_90V_VOL   (900)


/*****************************************
* PMIC settle time, should not be changed
******************************************/
#define PMIC_SETTLE_TIME (40)	/* us */

/****************************************************************
* Default disable gpu dvfs.
*****************************************************************/
/* #define GPU_DVFS_DEFAULT_DISABLED */

/* #define BIT(_bit_)          (unsigned int)(1 << (_bit_)) */
#define BITS(_bits_, _val_) ((BIT(((1)?_bits_)+1)-BIT(((0)?_bits_))) & (_val_<<((0)?_bits_)))
#define BITMASK(_bits_)     (BIT(((1)?_bits_)+1)-BIT(((0)?_bits_)))


static volatile int sg_delayCount;
#define mt_gpufreq_udelay(x) {int i; for (i = 0; i < x * 1000; i++) sg_delayCount++; }


/********************************************
* enable this option to adjust buck voltage
*********************************************/
#ifndef CONFIG_MT8135_FPGA
#define MT_BUCK_ADJUST
#endif

struct mt_gpufreq_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_lower_bound;
	unsigned int gpufreq_upper_bound;
	unsigned int gpufreq_volt;
	unsigned int gpufreq_remap;
};

struct mt_gpufreq_power_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_power;
};

/*****************
* extern function
******************/
extern int mt_gpufreq_state_set(int enabled);
extern void mt_gpufreq_thermal_protect(unsigned int limited_power);
extern int mt_gpufreq_register(struct mt_gpufreq_info *freqs, int num);
extern bool mt_gpufreq_is_registered_get(void);
extern unsigned int mt_gpufreq_get_cur_freq(void);
extern unsigned int mt_gpufreq_get_cur_volt(void);
extern unsigned int mt_gpufreq_get_cur_load(void);
extern void mt_gpufreq_set_cur_load(unsigned int load);
extern int mt_gpufreq_non_register(void);
extern void mt_gpufreq_set_initial(void);
extern int mt_gpufreq_switch_handler(unsigned int load);
extern bool mt_gpufreq_dvfs_disabled(void);

#endif
