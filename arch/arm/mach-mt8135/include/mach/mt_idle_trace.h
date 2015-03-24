#ifndef _TP_SAMPLES_TRACE_H
#define _TP_SAMPLES_TRACE_H

#include <linux/tracepoint.h>

DECLARE_TRACE(Cx_to_Cy_event, TP_PROTO(int cpu, int from, int to), TP_ARGS(cpu, from, to));


extern unsigned long localtimer_get_counter(void);
extern int localtimer_set_next_event(unsigned long evt);

extern u32 En_SPM_MCDI;
extern u32 ptp_data[6];
extern struct kobject *power_kobj;

#endif
