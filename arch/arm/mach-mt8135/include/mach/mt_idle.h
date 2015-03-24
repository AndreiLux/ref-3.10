#ifndef _MT_IDLE_H
#define _MT_IDLE_H

#include <mach/mt_spm_idle.h>

#ifdef SPM_MCDI_FUNC
extern void enable_mcidle_by_bit(int id);
extern void disable_mcidle_by_bit(int id);
#else
static inline void enable_mcidle_by_bit(int id)
{
}

static inline void disable_mcidle_by_bit(int id)
{
}
#endif

#if (defined CONFIG_MTK_LDVT || defined CONFIG_MT8135_FPGA)
static inline void enable_dpidle_by_bit(int id)
{
}

static inline void disable_dpidle_by_bit(int id)
{
}
#else
extern void enable_dpidle_by_bit(int id);
extern void disable_dpidle_by_bit(int id);
#endif


extern void mt_idle_init(void);


#ifdef __MT_IDLE_C__

/* idle task driver internal use only */

extern u32 ptp_data[6];
extern u32 En_SPM_MCDI;

extern unsigned long localtimer_get_counter(void);
extern int localtimer_set_next_event(unsigned long evt);

#endif /* __MT_IDLE_C__ */


#endif
