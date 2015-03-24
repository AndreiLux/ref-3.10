#undef TRACE_SYSTEM
#define TRACE_SYSTEM msdc

#if !defined(__MTK_SD_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)

#define __MTK_SD_TRACE_H

#include <linux/tracepoint.h>

struct msdc_host;

TRACE_EVENT(msdc_sdio_irq_enable,
	TP_PROTO(struct msdc_host *host, int enable),
	TP_ARGS(host, enable),
	TP_STRUCT__entry(
		__field(bool, enable)
		__field(int, depth)
	),
	TP_fast_assign(
		__entry->enable = enable;
		__entry->depth = irq_to_desc(gpio_to_irq(host->hw->sdio.irq.pin))->depth;
	),
	TP_printk("enable=%d depth=%d", __entry->enable, __entry->depth)
);

TRACE_EVENT(msdc_sdio_irq,
	TP_PROTO(struct msdc_host *host, int data),
	TP_ARGS(host, data),
	TP_STRUCT__entry(
		__field(int, data)
	),
	TP_fast_assign(
		__entry->data = data;
	),
	TP_printk("data=%08X", __entry->data)
);

TRACE_EVENT(msdc_data_xfer,
	TP_PROTO(struct msdc_host *host, u32 flags, u32 err, int bytes),
	TP_ARGS(host, flags, err, bytes),
	TP_STRUCT__entry(
		__field(u32, id)
		__field(u32, flags)
		__field(u32, err)
		__field(int, bytes)
	),
	TP_fast_assign(
		__entry->id = host->id;
		__entry->flags = flags;
		__entry->err = err;
		__entry->bytes = bytes;
	),
	TP_printk("id=%d: flags=%08X; err=%08X; bytes=%d", __entry->id, __entry->flags, __entry->err, __entry->bytes)
);

TRACE_EVENT(msdc_sdhc_irq,
	TP_PROTO(struct msdc_host *host, u32 tag, u32 data),
	TP_ARGS(host, tag, data),
	TP_STRUCT__entry(
		__field(u32, id)
		__field(u32, tag)
		__field(u32, data)
	),
	TP_fast_assign(
		__entry->id = host->id;
		__entry->tag = tag;
		__entry->data = data;
	),
	TP_printk("id=%d tag=%08X data=%08X", __entry->id, __entry->tag, __entry->data)
);

TRACE_EVENT(msdc_command_start,
	TP_PROTO(u8 id, u8 cmd, u32 arg, u8 tag),
	TP_ARGS(id, cmd, arg, tag),
	TP_STRUCT__entry(
		__field(u8, id)
		__field(u8, cmd)
		__field(u8, tag)
		__field(u32, arg)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->cmd = cmd;
		__entry->tag = tag;
		__entry->arg = arg;
	),
	TP_printk("id=%d cmd=%d arg=%08X tag=%d", __entry->id, __entry->cmd, __entry->arg, __entry->tag)
);

TRACE_EVENT(msdc_command_rsp,
	TP_PROTO(u8 id, u8 cmd, u32 err, u32 rsp),
	TP_ARGS(id, cmd, err, rsp),
	TP_STRUCT__entry(
		__field(u8, id)
		__field(u8, cmd)
		__field(u32, err)
		__field(u32, rsp)
	),
	TP_fast_assign(
		__entry->id = id;
		__entry->cmd = cmd;
		__entry->err = err;
		__entry->rsp = rsp;
	),
	TP_printk("id=%d cmd=%d err=%d rsp=%08X", __entry->id, __entry->cmd, __entry->err, __entry->rsp)
);

#endif /* __MTK_SD_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE mtk-sd-trace
#include <trace/define_trace.h>
