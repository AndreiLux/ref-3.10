/* hisi_rproc.h */

#ifndef HISI_RPROC_H
#define HISI_RPROC_H

#include <linux/huawei/mailbox.h>
#include <linux/errno.h>

#define HISI_RPROC_LPM3				"HISI_RPROC_LPM3"
#define HISI_RPROC_LPM3_FOR_REGULATOR		"HISI_RPROC_LPM3_FOR_REGULATOR"
#define HISI_RPROC_LPM3_FOR_DEBUG		"HISI_RPROC_LPM3_FOR_DEBUG"
#define HISI_RPROC_LPM3_FOR_RDR			"HISI_RPROC_LPM3_FOR_RDR"
#define HISI_RPROC_IOM3				"HISI_RPROC_IOM3"
#define HISI_RPROC_HIFI				"HISI_RPROC_HIFI"
/* #define HISI_RPROC_MCPU HI3630_MAILBOX_RP_MCPU */

#define RPROC_SYNC_SEND(rproc_name, msg, len,				\
		is_sync_msg, ack_buffer, ack_buffer_len)		\
hisi_rproc_xfer_sync(rproc_name, msg, len,				\
		is_sync_msg, ack_buffer, ack_buffer_len)

#define RPROC_ASYNC_SEND(rproc_name, msg, len,				\
		is_sync_msg, complete_fn, data)				\
hisi_rproc_xfer_async(rproc_name, msg, len,				\
		is_sync_msg, complete_fn, data)

#define RPROC_MONITOR_REGISTER(rproc_name, nb)				\
hisi_rproc_rx_register(rproc_name, nb)

#define RPROC_MONITOR_UNREGISTER(rproc_name, nb)			\
hisi_rproc_rx_unregister(rproc_name, nb)

typedef enum {
	ASYNC_MSG = 0,
	SYNC_MSG
} rproc_msg_type_t;

typedef mbox_msg_t rproc_msg_t;
typedef mbox_msg_len_t rproc_msg_len_t;
typedef void (*rproc_complete_t)(rproc_msg_t *ack_buffer,
				rproc_msg_len_t ack_buffer_len,
				int error,
				void *data);

#ifdef CONFIG_HISI_RPROC
extern int hisi_rproc_xfer_sync(const char *rproc_name,
				rproc_msg_t *msg,
				rproc_msg_len_t len,
				rproc_msg_type_t is_sync_msg,
				rproc_msg_t *ack_buffer,
				rproc_msg_len_t ack_buffer_len);
extern int hisi_rproc_xfer_async(const char *rproc_name,
				rproc_msg_t *msg,
				rproc_msg_len_t len,
				rproc_msg_type_t is_sync_msg,
				rproc_complete_t complete_fn,
				void *data);
extern int
hisi_rproc_rx_register(const char *rproc_name, struct notifier_block *nb);
extern int
hisi_rproc_rx_unregister(const char *rproc_name, struct notifier_block *nb);
#else
static inline int hisi_rproc_xfer_sync(const char *rproc_name,
				rproc_msg_t *msg,
				rproc_msg_len_t len,
				rproc_msg_type_t is_sync_msg,
				rproc_msg_t *ack_buffer,
				rproc_msg_len_t ack_buffer_len)
{
	return -ENOSYS;
}

static inline int hisi_rproc_xfer_async(const char *rproc_name,
				rproc_msg_t *msg,
				rproc_msg_len_t len,
				rproc_msg_type_t is_sync_msg,
				rproc_complete_t complete_fn,
				void *data)
{
	return -ENOSYS;
}

static inline int
hisi_rproc_rx_register(const char *rproc_name, struct notifier_block *nb)
{
	return -ENOSYS;
}

static inline int
hisi_rproc_rx_unregister(const char *rproc_name, struct notifier_block *nb)
{
	return -ENOSYS;
}
#endif /* CONFIG_HISI_RPROC */

#endif /* HISI_RPROC_H */
