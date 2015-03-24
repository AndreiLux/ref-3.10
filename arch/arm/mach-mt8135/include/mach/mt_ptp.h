#ifndef _MT_PTP_
#define _MT_PTP_

#include <linux/kernel.h>
#include <mach/sync_write.h>

typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef unsigned long long u64;

/* 8135 CA7 PTP parameters */
u32 PTP_CA7_VO_0, PTP_CA7_VO_1, PTP_CA7_VO_2, PTP_CA7_VO_3, PTP_CA7_VO_4, PTP_CA7_VO_5,
    PTP_CA7_VO_6, PTP_CA7_VO_7;
u32 PTP_CA7_init2_VO_0, PTP_CA7_init2_VO_1, PTP_CA7_init2_VO_2, PTP_CA7_init2_VO_3,
    PTP_CA7_init2_VO_4, PTP_CA7_init2_VO_5, PTP_CA7_init2_VO_6, PTP_CA7_init2_VO_7;
u32 PTP_CA7_INIT_FLAG = 1;
u32 PTP_CA7_DCVOFFSET = 0;
u32 PTP_CA7_AGEVOFFSET = 0;

/* 8135 CA15 PTP parameters */
u32 PTP_CA15_VO_0, PTP_CA15_VO_1, PTP_CA15_VO_2, PTP_CA15_VO_3, PTP_CA15_VO_4, PTP_CA15_VO_5,
    PTP_CA15_VO_6, PTP_CA15_VO_7;
u32 PTP_CA15_init2_VO_0, PTP_CA15_init2_VO_1, PTP_CA15_init2_VO_2, PTP_CA15_init2_VO_3,
    PTP_CA15_init2_VO_4, PTP_CA15_init2_VO_5, PTP_CA15_init2_VO_6, PTP_CA15_init2_VO_7;
u32 PTP_CA15_INIT_FLAG = 1;
u32 PTP_CA15_DCVOFFSET = 0;
u32 PTP_CA15_AGEVOFFSET = 0;

#define En_PTP_OD 1
#define PTP_Get_Real_Val 1
#define Set_PMIC_Volt 1
#define En_ISR_log 0

/* #define ptp_fsm_int_b_IRQ_ID 32 */

#define ptp_read(addr)		(*(volatile u32 *)(addr))
#define ptp_write(addr, val)	mt65xx_reg_sync_writel(val, addr)

/* 8135 CA7 PTP register =========================================== */
#define PTP_ca7_base_addr         (0xf100c000)
#define PTP_ca7_ctr_reg_addr      (PTP_ca7_base_addr+0x200)

#define PTP_CA7_DESCHAR           (PTP_ca7_ctr_reg_addr)
#define PTP_CA7_TEMPCHAR          (PTP_ca7_ctr_reg_addr+0x04)
#define PTP_CA7_DETCHAR           (PTP_ca7_ctr_reg_addr+0x08)
#define PTP_CA7_AGECHAR           (PTP_ca7_ctr_reg_addr+0x0c)

#define PTP_CA7_DCCONFIG          (PTP_ca7_ctr_reg_addr+0x10)
#define PTP_CA7_AGECONFIG         (PTP_ca7_ctr_reg_addr+0x14)
#define PTP_CA7_FREQPCT30         (PTP_ca7_ctr_reg_addr+0x18)
#define PTP_CA7_FREQPCT74         (PTP_ca7_ctr_reg_addr+0x1c)

#define PTP_CA7_LIMITVALS         (PTP_ca7_ctr_reg_addr+0x20)
#define PTP_CA7_VBOOT             (PTP_ca7_ctr_reg_addr+0x24)
#define PTP_CA7_DETWINDOW         (PTP_ca7_ctr_reg_addr+0x28)
#define PTP_CA7_PTPCONFIG         (PTP_ca7_ctr_reg_addr+0x2c)

#define PTP_CA7_TSCALCS           (PTP_ca7_ctr_reg_addr+0x30)
#define PTP_CA7_RUNCONFIG         (PTP_ca7_ctr_reg_addr+0x34)
#define PTP_CA7_PTPEN             (PTP_ca7_ctr_reg_addr+0x38)
#define PTP_CA7_INIT2VALS         (PTP_ca7_ctr_reg_addr+0x3c)

#define PTP_CA7_DCVALUES          (PTP_ca7_ctr_reg_addr+0x40)
#define PTP_CA7_AGEVALUES         (PTP_ca7_ctr_reg_addr+0x44)
#define PTP_CA7_VOP30             (PTP_ca7_ctr_reg_addr+0x48)
#define PTP_CA7_VOP74             (PTP_ca7_ctr_reg_addr+0x4c)

#define PTP_CA7_TEMP              (PTP_ca7_ctr_reg_addr+0x50)
#define PTP_CA7_PTPINTSTS         (PTP_ca7_ctr_reg_addr+0x54)
#define PTP_CA7_PTPINTSTSRAW      (PTP_ca7_ctr_reg_addr+0x58)
#define PTP_CA7_PTPINTEN          (PTP_ca7_ctr_reg_addr+0x5c)

#define PTP_CA7_SMSTATE0          (PTP_ca7_ctr_reg_addr+0x80)
#define PTP_CA7_SMSTATE1          (PTP_ca7_ctr_reg_addr+0x84)

/* 8135 CA7 Thermal Controller register =========================================== */
#define PTP_ca7_thermal_ctr_reg_addr     (PTP_ca7_base_addr)

#define PTP_CA7_TEMPMONCTL0              (PTP_ca7_base_addr)
#define PTP_CA7_TEMPMONCTL1              (PTP_ca7_base_addr+0x04)
#define PTP_CA7_TEMPMONCTL2              (PTP_ca7_base_addr+0x08)
#define PTP_CA7_TEMPMONINT               (PTP_ca7_base_addr+0x0c)

#define PTP_CA7_TEMPMONINTSTS            (PTP_ca7_base_addr+0x10)
#define PTP_CA7_TEMPMONIDET0             (PTP_ca7_base_addr+0x14)
#define PTP_CA7_TEMPMONIDET1             (PTP_ca7_base_addr+0x18)
#define PTP_CA7_TEMPMONIDET2             (PTP_ca7_base_addr+0x1c)

#define PTP_CA7_TEMPH2NTHRE              (PTP_ca7_base_addr+0x24)
#define PTP_CA7_TEMPHTHRE                (PTP_ca7_base_addr+0x28)
#define PTP_CA7_TEMPCTHRE                (PTP_ca7_base_addr+0x2c)

#define PTP_CA7_TEMPOFFSETH              (PTP_ca7_base_addr+0x30)
#define PTP_CA7_TEMPOFFSETL              (PTP_ca7_base_addr+0x34)
#define PTP_CA7_TEMPMSRCTL0              (PTP_ca7_base_addr+0x38)
#define PTP_CA7_TEMPMSRCTL1              (PTP_ca7_base_addr+0x3c)

#define PTP_CA7_TEMPAHBPOLL              (PTP_ca7_base_addr+0x40)
#define PTP_CA7_TEMPAHBTO                (PTP_ca7_base_addr+0x44)
#define PTP_CA7_TEMPADCPNP0              (PTP_ca7_base_addr+0x48)
#define PTP_CA7_TEMPADCPNP1              (PTP_ca7_base_addr+0x4c)

#define PTP_CA7_TEMPADCPNP2              (PTP_ca7_base_addr+0x50)
#define PTP_CA7_TEMPADCMUX               (PTP_ca7_base_addr+0x54)
#define PTP_CA7_TEMPADCEXT               (PTP_ca7_base_addr+0x58)
#define PTP_CA7_TEMPADCEXT1              (PTP_ca7_base_addr+0x5c)

#define PTP_CA7_TEMPADCEN                (PTP_ca7_base_addr+0x60)
#define PTP_CA7_TEMPPNPMUXADDR           (PTP_ca7_base_addr+0x64)
#define PTP_CA7_TEMPADCMUXADDR           (PTP_ca7_base_addr+0x68)
#define PTP_CA7_TEMPADCEXTADDR           (PTP_ca7_base_addr+0x6c)

#define PTP_CA7_TEMPADCEXT1ADDR          (PTP_ca7_base_addr+0x70)
#define PTP_CA7_TEMPADCENADDR            (PTP_ca7_base_addr+0x74)
#define PTP_CA7_TEMPADCVALIDADDR         (PTP_ca7_base_addr+0x78)
#define PTP_CA7_TEMPADCVOLTADDR          (PTP_ca7_base_addr+0x7c)

#define PTP_CA7_TEMPRDCTRL               (PTP_ca7_base_addr+0x80)
#define PTP_CA7_TEMPADCVALIDMASK         (PTP_ca7_base_addr+0x84)
#define PTP_CA7_TEMPADCVOLTAGESHIFT      (PTP_ca7_base_addr+0x88)
#define PTP_CA7_TEMPADCWRITECTRL         (PTP_ca7_base_addr+0x8c)

#define PTP_CA7_TEMPMSR0                 (PTP_ca7_base_addr+0x90)
#define PTP_CA7_TEMPMSR1                 (PTP_ca7_base_addr+0x94)
#define PTP_CA7_TEMPMSR2                 (PTP_ca7_base_addr+0x98)

#define PTP_CA7_TEMPIMMD0                (PTP_ca7_base_addr+0xa0)
#define PTP_CA7_TEMPIMMD1                (PTP_ca7_base_addr+0xa4)
#define PTP_CA7_TEMPIMMD2                (PTP_ca7_base_addr+0xa8)

#define PTP_CA7_TEMPMONIDET3             (PTP_ca7_base_addr+0xb0)
#define PTP_CA7_TEMPADCPNP3              (PTP_ca7_base_addr+0xb4)
#define PTP_CA7_TEMPMSR3                 (PTP_ca7_base_addr+0xb8)
#define PTP_CA7_TEMPIMMD3                (PTP_ca7_base_addr+0xbc)

#define PTP_CA7_TEMPPROTCTL              (PTP_ca7_base_addr+0xc0)
#define PTP_CA7_TEMPPROTTA               (PTP_ca7_base_addr+0xc4)
#define PTP_CA7_TEMPPROTTB               (PTP_ca7_base_addr+0xc8)
#define PTP_CA7_TEMPPROTTC               (PTP_ca7_base_addr+0xcc)

#define PTP_CA7_TEMPSPARE0               (PTP_ca7_base_addr+0xf0)
#define PTP_CA7_TEMPSPARE1               (PTP_ca7_base_addr+0xf4)

/* 8135 CA15 PTP register =========================================== */
#define PTP_ca15_base_addr           (0xf101a000)
#define PTP_ca15_ctr_reg_addr        (PTP_ca15_base_addr+0x200)

#define PTP_CA15_DESCHAR             (PTP_ca15_ctr_reg_addr)
#define PTP_CA15_TEMPCHAR            (PTP_ca15_ctr_reg_addr+0x04)
#define PTP_CA15_DETCHAR             (PTP_ca15_ctr_reg_addr+0x08)
#define PTP_CA15_AGECHAR             (PTP_ca15_ctr_reg_addr+0x0c)

#define PTP_CA15_DCCONFIG            (PTP_ca15_ctr_reg_addr+0x10)
#define PTP_CA15_AGECONFIG           (PTP_ca15_ctr_reg_addr+0x14)
#define PTP_CA15_FREQPCT30           (PTP_ca15_ctr_reg_addr+0x18)
#define PTP_CA15_FREQPCT74           (PTP_ca15_ctr_reg_addr+0x1c)

#define PTP_CA15_LIMITVALS           (PTP_ca15_ctr_reg_addr+0x20)
#define PTP_CA15_VBOOT               (PTP_ca15_ctr_reg_addr+0x24)
#define PTP_CA15_DETWINDOW           (PTP_ca15_ctr_reg_addr+0x28)
#define PTP_CA15_PTPCONFIG           (PTP_ca15_ctr_reg_addr+0x2c)

#define PTP_CA15_TSCALCS             (PTP_ca15_ctr_reg_addr+0x30)
#define PTP_CA15_RUNCONFIG           (PTP_ca15_ctr_reg_addr+0x34)
#define PTP_CA15_PTPEN               (PTP_ca15_ctr_reg_addr+0x38)
#define PTP_CA15_INIT2VALS           (PTP_ca15_ctr_reg_addr+0x3c)

#define PTP_CA15_DCVALUES            (PTP_ca15_ctr_reg_addr+0x40)
#define PTP_CA15_AGEVALUES           (PTP_ca15_ctr_reg_addr+0x44)
#define PTP_CA15_VOP30               (PTP_ca15_ctr_reg_addr+0x48)
#define PTP_CA15_VOP74               (PTP_ca15_ctr_reg_addr+0x4c)

#define PTP_CA15_TEMP                (PTP_ca15_ctr_reg_addr+0x50)
#define PTP_CA15_PTPINTSTS           (PTP_ca15_ctr_reg_addr+0x54)
#define PTP_CA15_PTPINTSTSRAW        (PTP_ca15_ctr_reg_addr+0x58)
#define PTP_CA15_PTPINTEN            (PTP_ca15_ctr_reg_addr+0x5c)

#define PTP_CA15_SMSTATE0            (PTP_ca15_ctr_reg_addr+0x80)
#define PTP_CA15_SMSTATE1            (PTP_ca15_ctr_reg_addr+0x84)

/* 8135 CA15 Thermal Controller register =========================================== */
#define PTP_ca15_thermal_ctr_reg_addr     (PTP_ca15_base_addr)

#define PTP_CA15_TEMPMONCTL0              (PTP_ca15_base_addr)
#define PTP_CA15_TEMPMONCTL1              (PTP_ca15_base_addr+0x04)
#define PTP_CA15_TEMPMONCTL2              (PTP_ca15_base_addr+0x08)
#define PTP_CA15_TEMPMONINT               (PTP_ca15_base_addr+0x0c)

#define PTP_CA15_TEMPMONINTSTS            (PTP_ca15_base_addr+0x10)
#define PTP_CA15_TEMPMONIDET0             (PTP_ca15_base_addr+0x14)
#define PTP_CA15_TEMPMONIDET1             (PTP_ca15_base_addr+0x18)
#define PTP_CA15_TEMPMONIDET2             (PTP_ca15_base_addr+0x1c)

#define PTP_CA15_TEMPH2NTHRE              (PTP_ca15_base_addr+0x24)
#define PTP_CA15_TEMPHTHRE                (PTP_ca15_base_addr+0x28)
#define PTP_CA15_TEMPCTHRE                (PTP_ca15_base_addr+0x2c)

#define PTP_CA15_TEMPOFFSETH              (PTP_ca15_base_addr+0x30)
#define PTP_CA15_TEMPOFFSETL              (PTP_ca15_base_addr+0x34)
#define PTP_CA15_TEMPMSRCTL0              (PTP_ca15_base_addr+0x38)
#define PTP_CA15_TEMPMSRCTL1              (PTP_ca15_base_addr+0x3c)

#define PTP_CA15_TEMPAHBPOLL              (PTP_ca15_base_addr+0x40)
#define PTP_CA15_TEMPAHBTO                (PTP_ca15_base_addr+0x44)
#define PTP_CA15_TEMPADCPNP0              (PTP_ca15_base_addr+0x48)
#define PTP_CA15_TEMPADCPNP1              (PTP_ca15_base_addr+0x4c)

#define PTP_CA15_TEMPADCPNP2              (PTP_ca15_base_addr+0x50)
#define PTP_CA15_TEMPADCMUX               (PTP_ca15_base_addr+0x54)
#define PTP_CA15_TEMPADCEXT               (PTP_ca15_base_addr+0x58)
#define PTP_CA15_TEMPADCEXT1              (PTP_ca15_base_addr+0x5c)

#define PTP_CA15_TEMPADCEN                (PTP_ca15_base_addr+0x60)
#define PTP_CA15_TEMPPNPMUXADDR           (PTP_ca15_base_addr+0x64)
#define PTP_CA15_TEMPADCMUXADDR           (PTP_ca15_base_addr+0x68)
#define PTP_CA15_TEMPADCEXTADDR           (PTP_ca15_base_addr+0x6c)

#define PTP_CA15_TEMPADCEXT1ADDR          (PTP_ca15_base_addr+0x70)
#define PTP_CA15_TEMPADCENADDR            (PTP_ca15_base_addr+0x74)
#define PTP_CA15_TEMPADCVALIDADDR         (PTP_ca15_base_addr+0x78)
#define PTP_CA15_TEMPADCVOLTADDR          (PTP_ca15_base_addr+0x7c)

#define PTP_CA15_TEMPRDCTRL               (PTP_ca15_base_addr+0x80)
#define PTP_CA15_TEMPADCVALIDMASK         (PTP_ca15_base_addr+0x84)
#define PTP_CA15_TEMPADCVOLTAGESHIFT      (PTP_ca15_base_addr+0x88)
#define PTP_CA15_TEMPADCWRITECTRL         (PTP_ca15_base_addr+0x8c)

#define PTP_CA15_TEMPMSR0                 (PTP_ca15_base_addr+0x90)
#define PTP_CA15_TEMPMSR1                 (PTP_ca15_base_addr+0x94)
#define PTP_CA15_TEMPMSR2                 (PTP_ca15_base_addr+0x98)

#define PTP_CA15_TEMPIMMD0                (PTP_ca15_base_addr+0xa0)
#define PTP_CA15_TEMPIMMD1                (PTP_ca15_base_addr+0xa4)
#define PTP_CA15_TEMPIMMD2                (PTP_ca15_base_addr+0xa8)

#define PTP_CA15_TEMPMONIDET3             (PTP_ca15_base_addr+0xb0)
#define PTP_CA15_TEMPADCPNP3              (PTP_ca15_base_addr+0xb4)
#define PTP_CA15_TEMPMSR3                 (PTP_ca15_base_addr+0xb8)
#define PTP_CA15_TEMPIMMD3                (PTP_ca15_base_addr+0xbc)

#define PTP_CA15_TEMPPROTCTL              (PTP_ca15_base_addr+0xc0)
#define PTP_CA15_TEMPPROTTA               (PTP_ca15_base_addr+0xc4)
#define PTP_CA15_TEMPPROTTB               (PTP_ca15_base_addr+0xc8)
#define PTP_CA15_TEMPPROTTC               (PTP_ca15_base_addr+0xcc)

#define PTP_CA15_TEMPSPARE0               (PTP_ca15_base_addr+0xf0)
#define PTP_CA15_TEMPSPARE1               (PTP_ca15_base_addr+0xf4)

#define PTP_PMIC_WRAP_BASE                (0xF000F000)
#define PTP_PMIC_WRAP_DVFS_ADR0           (PTP_PMIC_WRAP_BASE+0xF4)
#define PTP_PMIC_WRAP_DVFS_WDATA0         (PTP_PMIC_WRAP_BASE+0xF8)
#define PTP_PMIC_WRAP_DVFS_ADR1           (PTP_PMIC_WRAP_BASE+0xFC)
#define PTP_PMIC_WRAP_DVFS_WDATA1         (PTP_PMIC_WRAP_BASE+0x100)
#define PTP_PMIC_WRAP_DVFS_ADR2           (PTP_PMIC_WRAP_BASE+0x104)
#define PTP_PMIC_WRAP_DVFS_WDATA2         (PTP_PMIC_WRAP_BASE+0x108)
#define PTP_PMIC_WRAP_DVFS_ADR3           (PTP_PMIC_WRAP_BASE+0x10C)
#define PTP_PMIC_WRAP_DVFS_WDATA3         (PTP_PMIC_WRAP_BASE+0x110)
#define PTP_PMIC_WRAP_DVFS_ADR4           (PTP_PMIC_WRAP_BASE+0x114)
#define PTP_PMIC_WRAP_DVFS_WDATA4         (PTP_PMIC_WRAP_BASE+0x118)
#define PTP_PMIC_WRAP_DVFS_ADR5           (PTP_PMIC_WRAP_BASE+0x11C)
#define PTP_PMIC_WRAP_DVFS_WDATA5         (PTP_PMIC_WRAP_BASE+0x120)
#define PTP_PMIC_WRAP_DVFS_ADR6           (PTP_PMIC_WRAP_BASE+0x124)
#define PTP_PMIC_WRAP_DVFS_WDATA6         (PTP_PMIC_WRAP_BASE+0x128)
#define PTP_PMIC_WRAP_DVFS_ADR7           (PTP_PMIC_WRAP_BASE+0x12C)
#define PTP_PMIC_WRAP_DVFS_WDATA7         (PTP_PMIC_WRAP_BASE+0x130)

typedef struct {
	u32 ADC_CALI_EN;
	u32 PTPINITEN;
	u32 PTPMONEN;

	u32 MDES;
	u32 BDES;
	u32 DCCONFIG;
	u32 DCMDET;
	u32 DCBDET;
	u32 AGECONFIG;
	u32 AGEM;
	u32 AGEDELTA;
	u32 DVTFIXED;
	u32 VCO;
	u32 MTDES;
	u32 MTS;
	u32 BTS;

	u8 FREQPCT0;
	u8 FREQPCT1;
	u8 FREQPCT2;
	u8 FREQPCT3;
	u8 FREQPCT4;
	u8 FREQPCT5;
	u8 FREQPCT6;
	u8 FREQPCT7;

	u32 DETWINDOW;
	u32 VMAX;
	u32 VMIN;
	u32 DTHI;
	u32 DTLO;
	u32 VBOOT;
	u32 DETMAX;

	u32 DCVOFFSETIN;
	u32 AGEVOFFSETIN;
} PTP_Init_T;

u32 PTP_CA7_INIT_01(void);
u32 PTP_CA7_INIT_02(void);
u32 PTP_CA7_MON_MODE(void);
u32 PTP_CA15_INIT_01(void);
u32 PTP_CA15_INIT_02(void);
u32 PTP_CA15_MON_MODE(void);

u32 PTP_get_ptp_level(void);

extern u32 get_devinfo_with_index(u32 index);
/* extern int spm_dvfs_ctrl_volt(u32 value); */
/* extern unsigned int mt_cpufreq_max_frequency_by_DVS(unsigned int num); */
extern void mt_fh_popod_save(void);
extern void mt_fh_popod_restore(void);
/* extern void mt_cpufreq_return_default_DVS_by_ptpod(void); */
/* extern unsigned int mt_cpufreq_voltage_set_by_ptpod(unsigned int pmic_volt[], unsigned int array_size); */

#endif
