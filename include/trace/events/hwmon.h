#undef TRACE_SYSTEM
#define TRACE_SYSTEM hwmon

#if !defined(_TRACE_HWMON_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HWMON_H

#include <linux/tracepoint.h>

TRACE_EVENT(hwmon_sensor,

	TP_PROTO(unsigned int power, const char *cluster),

	TP_ARGS(power, cluster),

	TP_STRUCT__entry(
		__field(	unsigned int,    power )
		__array(	char,	cluster,    64)
	),

	TP_fast_assign(
		__entry->power	= power;
		strncpy(__entry->cluster, cluster, 64);
	),

	TP_printk("%u %s", __entry->power, __entry->cluster)
);
#endif /* _TRACE_HWMON_H */

#include <trace/define_trace.h>
