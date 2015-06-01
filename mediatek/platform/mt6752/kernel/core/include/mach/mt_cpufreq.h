/**
 * @file mt_cpufreq.h
 * @brief CPU DVFS driver interface
 */

#ifndef __MT_CPUFREQ_H__
#define __MT_CPUFREQ_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __MT_CPUFREQ_C__
#define CPUFREQ_EXTERN
#else
#define CPUFREQ_EXTERN extern
#endif

/*=============================================================*/
// Include files
/*=============================================================*/

// system includes

// project includes

// local includes

// forward references


/*=============================================================*/
// Macro definition
/*=============================================================*/


/*=============================================================*/
// Type definition
/*=============================================================*/

enum mt_cpu_dvfs_id {
    // K2 has little core only
    MT_CPU_DVFS_LITTLE,
    NR_MT_CPU_DVFS,
};

enum top_ckmuxsel {
    TOP_CKMUXSEL_CLKSQ   = 0, /* i.e. reg setting */
    TOP_CKMUXSEL_ARMPLL  = 1,
    TOP_CKMUXSEL_MAINPLL = 2,
    TOP_CKMUXSEL_UNIVPLL = 3,

    NR_TOP_CKMUXSEL,
} ;

/*
 * PMIC_WRAP
 */

/* Phase */
enum pmic_wrap_phase_id {
    PMIC_WRAP_PHASE_NORMAL,
    PMIC_WRAP_PHASE_SUSPEND,
    PMIC_WRAP_PHASE_DEEPIDLE,

    NR_PMIC_WRAP_PHASE,
};

/* IDX mapping */
enum {
    IDX_NM_VSRAM,		/* 0 */ /* PMIC_WRAP_PHASE_NORMAL */
    IDX_NM_VPROC,		/* 1 */
    IDX_NM_VGPU,		/* 2 */
    IDX_NM_VCORE,		/* 3 */
    IDX_NM_VLTE,		/* 4 */

    NR_IDX_NM,
};
enum {
    IDX_SP_VPROC_PWR_ON,        /* 0 */ /* PMIC_WRAP_PHASE_SUSPEND */
    IDX_SP_VPROC_SHUTDOWN,      /* 1 */
    IDX_SP_VSRAM_PWR_ON,        /* 2 */ 
    IDX_SP_VSRAM_SHUTDOWN,      /* 3 */
    IDX_SP_VCORE_NORMAL,        /* 4 */
    IDX_SP_VCORE_SLEEP,         /* 5 */
    IDX_SP_VCORE_TEMP1,         /* 6 */
    IDX_SP_VCORE_TEMP2,         /* 7 */

    NR_IDX_SP,
};
enum {
    IDX_DI_VCORE_TEMP1,		/* 0 */ /* PMIC_WRAP_PHASE_DEEPIDLE */
    IDX_DI_VCORE_TEMP2, 	/* 1 */
    
    IDX_DI_VSRAM_NORMAL,	/* 2 */
    IDX_DI_VSRAM_SLEEP, 	/* 3 */
    IDX_DI_VPROC_NORMAL = IDX_DI_VSRAM_NORMAL,  /* 2 */
    IDX_DI_VPROC_SLEEP = IDX_DI_VSRAM_SLEEP,    /* 3 */
    
    IDX_DI_VCORE_NORMAL,	/* 4 */
    IDX_DI_VCORE_SLEEP,		/* 5 */
    IDX_DI_VSRAM_FAST_TRSN_DIS, /* 6 */
    IDX_DI_VSRAM_FAST_TRSN_EN,  /* 7 */

    NR_IDX_DI,
};


typedef void (*cpuVoltsampler_func)(enum mt_cpu_dvfs_id , unsigned int mv);
/*=============================================================*/
// Global variable definition
/*=============================================================*/


/*=============================================================*/
// Global function definition
/*=============================================================*/

/* PMIC WRAP */
CPUFREQ_EXTERN void mt_cpufreq_set_pmic_phase(enum pmic_wrap_phase_id phase);
CPUFREQ_EXTERN void mt_cpufreq_set_pmic_cmd(enum pmic_wrap_phase_id phase, int idx, unsigned int cmd_wdata);
CPUFREQ_EXTERN void mt_cpufreq_apply_pmic_cmd(int idx);

/* PTP-OD */
CPUFREQ_EXTERN unsigned int mt_cpufreq_get_freq_by_idx(enum mt_cpu_dvfs_id id, int idx);
CPUFREQ_EXTERN int mt_cpufreq_update_volt(enum mt_cpu_dvfs_id id, unsigned int *volt_tbl, int nr_volt_tbl);
CPUFREQ_EXTERN void mt_cpufreq_restore_default_volt(enum mt_cpu_dvfs_id id);
CPUFREQ_EXTERN unsigned int mt_cpufreq_get_cur_volt(enum mt_cpu_dvfs_id id);
CPUFREQ_EXTERN void mt_cpufreq_enable_by_ptpod(enum mt_cpu_dvfs_id id);
CPUFREQ_EXTERN unsigned int mt_cpufreq_disable_by_ptpod(enum mt_cpu_dvfs_id id);

/* Thermal */
CPUFREQ_EXTERN void mt_cpufreq_thermal_protect(unsigned int limited_power);

/* SDIO */
CPUFREQ_EXTERN void mt_vcore_dvfs_disable_by_sdio(unsigned int type, bool disabled);
CPUFREQ_EXTERN void mt_vcore_dvfs_volt_set_by_sdio(unsigned int volt);
CPUFREQ_EXTERN unsigned int mt_vcore_dvfs_volt_get_by_sdio(void);

CPUFREQ_EXTERN unsigned int mt_get_cur_volt_vcore_ao(void);
//CPUFREQ_EXTERN unsigned int mt_get_cur_volt_vcore_pdn(void);

/* Generic */
CPUFREQ_EXTERN int mt_cpufreq_state_set(int enabled);
CPUFREQ_EXTERN int mt_cpufreq_clock_switch(enum mt_cpu_dvfs_id id, enum top_ckmuxsel sel);
CPUFREQ_EXTERN enum top_ckmuxsel mt_cpufreq_get_clock_switch(enum mt_cpu_dvfs_id id);
CPUFREQ_EXTERN void mt_cpufreq_setvolt_registerCB(cpuVoltsampler_func pCB);
CPUFREQ_EXTERN bool mt_cpufreq_earlysuspend_status_get(void);

CPUFREQ_EXTERN void mt_cpufreq_set_ramp_down_count_const(enum mt_cpu_dvfs_id id, int count);

#ifndef __KERNEL__
CPUFREQ_EXTERN int mt_cpufreq_pdrv_probe(void);
CPUFREQ_EXTERN int mt_cpufreq_set_opp_volt(enum mt_cpu_dvfs_id id, int idx);
CPUFREQ_EXTERN int mt_cpufreq_set_freq(enum mt_cpu_dvfs_id id, int idx);
CPUFREQ_EXTERN unsigned int dvfs_get_cpu_freq(enum mt_cpu_dvfs_id id);
CPUFREQ_EXTERN void dvfs_set_cpu_freq_FH(enum mt_cpu_dvfs_id id, int freq);
CPUFREQ_EXTERN unsigned int cpu_frequency_output_slt(enum mt_cpu_dvfs_id id);
CPUFREQ_EXTERN void dvfs_set_cpu_volt(enum mt_cpu_dvfs_id id, int volt);
CPUFREQ_EXTERN void dvfs_set_gpu_volt(int pmic_val);
CPUFREQ_EXTERN void dvfs_set_vcore_ao_volt(int pmic_val);
//CPUFREQ_EXTERN void dvfs_set_vcore_pdn_volt(int pmic_val);
CPUFREQ_EXTERN void dvfs_disable_by_ptpod(void);
CPUFREQ_EXTERN void dvfs_enable_by_ptpod(void);
#endif /* ! __KERNEL__ */

#undef CPUFREQ_EXTERN

#ifdef __cplusplus
}
#endif

#endif // __MT_CPUFREQ_H__
