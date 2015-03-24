#undef TRACE_SYSTEM
#define TRACE_SYSTEM cpu_stats

#if !defined(_MT_CPU_STATS_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _MT_CPU_STATS_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(cpu_online,
	TP_PROTO(unsigned int cpu, int online),
	TP_ARGS(cpu, online),
	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(int, online)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->online = online;
	),
	TP_printk("cpu=%d, online=%d", __entry->cpu, __entry->online)
);

#endif /* _MT_CPU_STATS_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE mach/mt_cpu_stats_trace
#include <trace/define_trace.h>

extern int get_wfi_power(unsigned int cpu, unsigned int freq);
extern int get_nonidle_power(unsigned int cpu, unsigned int freq);


