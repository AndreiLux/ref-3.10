#ifndef _MT_SPM_MTCMOS_
#define _MT_SPM_MTCMOS_

#include <linux/kernel.h>

#define STA_POWER_DOWN  0
#define STA_POWER_ON    1

#define CA15_CA7_CONNECT    ((volatile uint32_t *)0xF0200018)
#define CA15_MISC_DBG       ((volatile uint32_t *)0xF020020C)
#define CA15_CCI400_DVM_EN  ((volatile uint32_t *)0xF0324000)
#define CA7_CCI400_DVM_EN   ((volatile uint32_t *)0xF0325000)
#define RGUCFG              ((volatile uint32_t *)0xF0200254)
#define CONFIG_RES          ((volatile uint32_t *)0xF0200268)
#define CA15_MISC_DBG       ((volatile uint32_t *)0xF020020C)
#define CA15_RST_CTL        ((volatile uint32_t *)0xF0200244)
#define CCI400_STATUS       ((volatile uint32_t *)0xF032000C)


/*
 * 1. for CPU MTCMOS: CPU0, CPU1, CPU2, CPU3, DBG, CPUSYS
 * 2. call spm_mtcmos_cpu_lock/unlock() before/after any operations
 */
extern void spm_mtcmos_cpu_lock(unsigned long *flags);
extern void spm_mtcmos_cpu_unlock(unsigned long *flags);

extern int spm_mtcmos_ctrl_cpu0(int state);
extern int spm_mtcmos_ctrl_cpu1(int state);
extern int spm_mtcmos_ctrl_cpu2(int state);
extern int spm_mtcmos_ctrl_cpu3(int state);
extern bool spm_cpusys_can_power_down(void);
extern int spm_mtcmos_ctrl_dbg(int state);
extern int spm_mtcmos_ctrl_cpusys(int state);
extern void spm_mtcmos_init(void);


#endif
