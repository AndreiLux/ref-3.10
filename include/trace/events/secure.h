#undef TRACE_SYSTEM
#define TRACE_SYSTEM secure

#if !defined(_TRACE_SECURE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SECURE_H

#include <linux/tracepoint.h>


TRACE_EVENT(secure_enter,

	TP_PROTO(u32 cmd),

	TP_ARGS(cmd),

	TP_STRUCT__entry(
		__field(    u32,  cmd )
	),

	TP_fast_assign(
		__entry->cmd = cmd;
	),

	TP_printk("sec_st cmd=%u",
	__entry->cmd)
);

TRACE_EVENT(secure_exit,

	TP_PROTO(u32 cmd),

	TP_ARGS(cmd),

	TP_STRUCT__entry(
		__field(    u32,  cmd )
	),

	TP_fast_assign(
		__entry->cmd = cmd;
	),

	TP_printk("sec_ed cmd=%u",
	__entry->cmd)
);

TRACE_EVENT(secure_elapsed,

	TP_PROTO(u32 cmd),

	TP_ARGS(cmd),

	TP_STRUCT__entry(
		__field(    u32,  cmd )
	),

	TP_fast_assign(
		__entry->cmd = cmd;
	),

	TP_printk("drm_tee_end=%u",
	__entry->cmd)
);

#endif /*  _TRACE_SECURE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
