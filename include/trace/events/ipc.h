#undef TRACE_SYSTEM
#define TRACE_SYSTEM ipc

#if !defined(_TRACE_IPC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_IPC_H

#include <linux/tracepoint.h>

TRACE_EVENT(ipc_handle,

	TP_PROTO(u32 sub_irq_num, unsigned int count),

	TP_ARGS(sub_irq_num, count),

	TP_STRUCT__entry(
		__field(	u32,  sub_irq_num )
		__field(	unsigned int,  count )
	),

	TP_fast_assign(
		__entry->sub_irq_num = sub_irq_num;
		__entry->count = count;
	),

	TP_printk("sub_irq_num=%u count=%u",
			__entry->sub_irq_num, __entry->count)
);

TRACE_EVENT(ipc_completion,

	TP_PROTO(int sub_completion_num, unsigned int count),

	TP_ARGS(sub_completion_num, count),

	TP_STRUCT__entry(
		__field(	int,	sub_completion_num )
		__field(	unsigned int,  count )
	),

	TP_fast_assign(
		__entry->sub_completion_num = sub_completion_num;
		__entry->count = count;
	),

	TP_printk("sub_completion_num=%d count=%u",
			__entry->sub_completion_num, __entry->count)
);

TRACE_EVENT(ipc_call_entry,

	TP_PROTO(int ipc_type, int completion_sub_num, unsigned int mbox_cmd, unsigned int *mbox_data, unsigned int count, int timeout_ms),

	TP_ARGS(ipc_type, completion_sub_num, mbox_cmd, mbox_data, count, timeout_ms),

	TP_STRUCT__entry(
		__field(	int			,	ipc_type )
		__field(	int			,	completion_sub_num )
		__field(	unsigned int,	mbox_cmd )
		__field(	unsigned int	*,	mbox_data )
		__field(	unsigned int,	count )
		__field(	int			,	timeout_ms )
	),

	TP_fast_assign(
		__entry->ipc_type = ipc_type;
		__entry->completion_sub_num = completion_sub_num;
		__entry->mbox_cmd = mbox_cmd;
		__entry->mbox_data = mbox_data;
		__entry->count = count;
		__entry->timeout_ms = timeout_ms;
	),

	TP_printk("ipc=%d comp_sub_num=%d mbox_cmd=%p mbox_addr=%p count=%u timeout_ms=%d",
			__entry->ipc_type, __entry->completion_sub_num, __entry->mbox_cmd, __entry->mbox_data, __entry->count, __entry->timeout_ms)
);

TRACE_EVENT(ipc_call_exit,

	TP_PROTO(int ipc_type, int completion_sub_num, unsigned int *mbox_data, unsigned int count, int timeout_ret),

	TP_ARGS(ipc_type, completion_sub_num, mbox_data, count, timeout_ret),

	TP_STRUCT__entry(
		__field(	int			,	ipc_type )
		__field(	int			,	completion_sub_num )
		__field(	unsigned int	*,	mbox_data )
		__field(	unsigned int,	count )
		__field(	int			,	timeout_ret )
	),

	TP_fast_assign(
		__entry->ipc_type = ipc_type;
		__entry->completion_sub_num = completion_sub_num;
		__entry->mbox_data = mbox_data;
		__entry->count = count;
		__entry->timeout_ret = timeout_ret;
	),

	TP_printk("ipc=%d comp_sub_num=%d mbox_addr=%p count=%u timeout_ret=%d",
			__entry->ipc_type, __entry->completion_sub_num, __entry->mbox_data, __entry->count, __entry->timeout_ret)
);

#endif /*  _TRACE_IPC_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

