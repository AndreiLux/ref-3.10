#undef TRACE_SYSTEM
#define TRACE_SYSTEM cluster_stats

#if !defined(_MT_CLUSTER_STATS_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _MT_CLUSTER_STATS_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(cluster_state,
	TP_PROTO(unsigned int cluster, const char *state),
	TP_ARGS(cluster, state),
	TP_STRUCT__entry(
		__field(unsigned int, cluster)
		__field(const char* , state)
	),
	TP_fast_assign(
		__entry->cluster = cluster;
		__entry->state = state;
	),
	TP_printk("cluster=%d, state=%s", __entry->cluster, __entry->state)
);

#endif /* _MT_CLUSTER_STATS_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE mach/mt_cluster_stats_trace
#include <trace/define_trace.h>

extern int get_power_consumption_by_cpu(int cpu);
