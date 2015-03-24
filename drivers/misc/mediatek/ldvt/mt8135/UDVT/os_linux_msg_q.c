#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/wait.h>
#include "x_assert.h"
#include "x_os.h"


#define MSGQ_NAME_LEN           16
#define MSGQ_MAX_DATA_SIZE      4095
#define MSGQ_MAX_MSGS           4095

typedef struct os_msg_q_light {
	struct os_msg_q_light *previous;
	struct os_msg_q_light *next;
	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	BYTE *read;
	BYTE *write;
	BYTE *end;
	SIZE_T message_size;
	SIZE_T z_maxsize;
	UINT16 ui2_msg_count;
	INT16 i2_refcount;
	CHAR s_name[MSGQ_NAME_LEN + 1];
	BYTE start[1];
} OS_MSGQ_LIGHT_T;


static OS_MSGQ_LIGHT_T *s_msg_q_list;


static DEFINE_SPINLOCK(x_msg_lock);

static void msg_q_list_add(OS_MSGQ_LIGHT_T *pt_msg_q)
{
	if (s_msg_q_list != NULL) {
		pt_msg_q->previous = s_msg_q_list->previous;
		pt_msg_q->next = s_msg_q_list;
		s_msg_q_list->previous->next = pt_msg_q;
		s_msg_q_list->previous = pt_msg_q;
	} else {
		s_msg_q_list = pt_msg_q->next = pt_msg_q->previous = pt_msg_q;
	}
}


static void msg_q_list_remove(OS_MSGQ_LIGHT_T *pt_msg_q)
{
	if (pt_msg_q->previous == pt_msg_q) {
		s_msg_q_list = NULL;
	} else {
		pt_msg_q->previous->next = pt_msg_q->next;
		pt_msg_q->next->previous = pt_msg_q->previous;
		if (s_msg_q_list == pt_msg_q) {
			s_msg_q_list = pt_msg_q->next;
		}
	}
}


static OS_MSGQ_LIGHT_T *msg_q_find_obj(const CHAR *ps_name)
{
	OS_MSGQ_LIGHT_T *pt_msg_q = s_msg_q_list;
	if (pt_msg_q == NULL) {
		return NULL;
	}
	do {
		if (strncmp(pt_msg_q->s_name, ps_name, MSGQ_NAME_LEN) == 0) {
			return pt_msg_q;
		}
		pt_msg_q = pt_msg_q->next;
	} while (pt_msg_q != s_msg_q_list);

	return NULL;
}


INT32
x_msg_q_create(HANDLE_T *ph_msg_hdl, const CHAR *ps_name, SIZE_T z_msg_size, UINT16 ui2_msg_count)
{
	OS_MSGQ_LIGHT_T *pt_msg_q;
	SIZE_T message_size;
	unsigned long flags;

	/* check arguments */
	if ((ps_name == NULL) || (ps_name[0] == '\0') || (ph_msg_hdl == NULL) ||
	    (z_msg_size > MSGQ_MAX_DATA_SIZE) || (ui2_msg_count > MSGQ_MAX_MSGS) ||
	    (z_msg_size == 0) || (ui2_msg_count == 0)) {
		return OSR_INV_ARG;
	}

	message_size = sizeof(SIZE_T) + ((z_msg_size + 3) & ~3);
	pt_msg_q =
	    kcalloc(1, sizeof(OS_MSGQ_LIGHT_T) - sizeof(BYTE) + message_size * (ui2_msg_count + 1),
		    GFP_KERNEL);
	if (pt_msg_q == NULL) {
		return OSR_NO_RESOURCE;
	}
	/* FILL_CALLER(pt_msg_q); */

	pt_msg_q->read = pt_msg_q->start;
	pt_msg_q->write = pt_msg_q->start;
	pt_msg_q->end = pt_msg_q->start + message_size * (ui2_msg_count + 1);
	pt_msg_q->message_size = message_size;
	pt_msg_q->z_maxsize = z_msg_size;
	pt_msg_q->ui2_msg_count = ui2_msg_count;
	pt_msg_q->i2_refcount = 1;
	strncpy(pt_msg_q->s_name, ps_name, MSGQ_NAME_LEN);

	init_waitqueue_head(&pt_msg_q->read_wq);
	init_waitqueue_head(&pt_msg_q->write_wq);

	/* local_irq_save(flags); */
	spin_lock_irqsave(&x_msg_lock, flags);
	if (msg_q_find_obj(ps_name) != NULL) {
		/* local_irq_restore(flags); */
		spin_unlock_irqrestore(&x_msg_lock, flags);
		kfree(pt_msg_q);
		return OSR_EXIST;
	}
	msg_q_list_add(pt_msg_q);
	/* local_irq_restore(flags); */
	spin_unlock_irqrestore(&x_msg_lock, flags);

	*ph_msg_hdl = (HANDLE_T) (pt_msg_q);
	return OSR_OK;
}
EXPORT_SYMBOL(x_msg_q_create);


INT32 x_msg_q_attach(HANDLE_T *ph_msg_hdl, const CHAR *ps_name)
{
	OS_MSGQ_LIGHT_T *pt_msg_q;
	unsigned long flags;

	/* arguments check */
	if ((ps_name == NULL) || (ps_name[0] == '\0') || (ph_msg_hdl == NULL_HANDLE)) {
		return OSR_INV_ARG;
	}
	/* local_irq_save(flags); */
	spin_lock_irqsave(&x_msg_lock, flags);
	pt_msg_q = msg_q_find_obj(ps_name);
	if (pt_msg_q == NULL) {
		/* local_irq_restore(flags); */
		spin_unlock_irqrestore(&x_msg_lock, flags);
		return OSR_NOT_EXIST;
	}

	pt_msg_q->i2_refcount++;
	/* local_irq_restore(flags); */
	spin_unlock_irqrestore(&x_msg_lock, flags);

	*ph_msg_hdl = (HANDLE_T) (pt_msg_q);

	return OSR_OK;
}
EXPORT_SYMBOL(x_msg_q_attach);


INT32 x_msg_q_delete(HANDLE_T h_msg_hdl)
{
	OS_MSGQ_LIGHT_T *pt_msg_q;
	INT16 i2_refcount;
	unsigned long flags;

	pt_msg_q = (OS_MSGQ_LIGHT_T *) (h_msg_hdl);

	/* local_irq_save(flags); */
	spin_lock_irqsave(&x_msg_lock, flags);
	i2_refcount = --pt_msg_q->i2_refcount;
	if (i2_refcount > 0) {
		/* local_irq_restore(flags); */
		spin_unlock_irqrestore(&x_msg_lock, flags);
		return OSR_OK;
	}
	msg_q_list_remove(pt_msg_q);
	/* local_irq_restore(flags); */
	spin_unlock_irqrestore(&x_msg_lock, flags);

	kfree(pt_msg_q);
	return OSR_OK;
}
EXPORT_SYMBOL(x_msg_q_delete);


INT32 x_msg_q_send(HANDLE_T h_msg_hdl, const VOID *pv_msg, SIZE_T z_size, UINT8 ui1_pri)
{
	OS_MSGQ_LIGHT_T *pt_msg_q;
	BYTE *write;
	unsigned long flags;

	if ((pv_msg == NULL) || (z_size == 0)) {
		return OSR_INV_ARG;
	}

	pt_msg_q = (OS_MSGQ_LIGHT_T *) (h_msg_hdl);

	if (z_size > pt_msg_q->z_maxsize) {
		return OSR_TOO_BIG;
	}
	/* local_irq_save(flags); */
	spin_lock_irqsave(&x_msg_lock, flags);
	write = pt_msg_q->write + pt_msg_q->message_size;
	if (write == pt_msg_q->end) {
		write = pt_msg_q->start;
	}
	if (write == pt_msg_q->read) {
		/* local_irq_restore(flags); */
		spin_unlock_irqrestore(&x_msg_lock, flags);
		return OSR_TOO_MANY;
	}
	*(SIZE_T *) (pt_msg_q->write) = z_size;
	memcpy(pt_msg_q->write + 4, pv_msg, z_size);
	pt_msg_q->write = write;
	wake_up_all(&pt_msg_q->write_wq);
	/* local_irq_restore(flags); */
	spin_unlock_irqrestore(&x_msg_lock, flags);

	return OSR_OK;
}
EXPORT_SYMBOL(x_msg_q_send);


INT32
x_msg_q_receive(UINT16 *pui2_index,
		VOID *pv_msg,
		SIZE_T *pz_size,
		HANDLE_T *ph_msgq_hdl_list, UINT16 ui2_msgq_hdl_count, MSGQ_OPTION_T e_option)
{
	OS_MSGQ_LIGHT_T *pt_msg_q;
	BYTE *read;
	SIZE_T z_size;

	/* check arguments */
	if (e_option != X_MSGQ_OPTION_WAIT && e_option != X_MSGQ_OPTION_NOWAIT) {
		return OSR_INV_ARG;
	}

	if ((pui2_index == NULL) || (pv_msg == NULL) ||
	    (pz_size == NULL) || (*pz_size == 0) ||
	    (ph_msgq_hdl_list == NULL) || (ui2_msgq_hdl_count == 0)) {
		return OSR_INV_ARG;
	}

	if (ui2_msgq_hdl_count != 1) {
		return OSR_NOT_SUPPORT;
	}

	pt_msg_q = (OS_MSGQ_LIGHT_T *) (ph_msgq_hdl_list[0]);
	if (pt_msg_q->read == pt_msg_q->write) {
		if (e_option == X_MSGQ_OPTION_NOWAIT) {
			return OSR_NO_MSG;
		}
		wait_event(pt_msg_q->write_wq, pt_msg_q->read != pt_msg_q->write);
	}
	read = pt_msg_q->read;
	z_size = *(SIZE_T *) (read);
	if (z_size > *pz_size) {
		z_size = *pz_size;
	}
	memcpy(pv_msg, read + 4, z_size);
	read += pt_msg_q->message_size;
	if (read == pt_msg_q->end) {
		read = pt_msg_q->start;
	}
	pt_msg_q->read = read;
	wake_up_all(&pt_msg_q->read_wq);

	*pui2_index = 0;
	*pz_size = z_size;

	return OSR_OK;
}
EXPORT_SYMBOL(x_msg_q_receive);


INT32
x_msg_q_receive_timeout(UINT16 *pui2_index,
			VOID *pv_msg,
			SIZE_T *pz_size,
			HANDLE_T *ph_msgq_hdl_list, UINT16 ui2_msgq_hdl_count, UINT32 ui4_time)
{
	static const int quantum_ms = 1000 / HZ;
	OS_MSGQ_LIGHT_T *pt_msg_q;
	BYTE *read;
	SIZE_T z_size;
	int ret;

	if ((pui2_index == NULL) || (pv_msg == NULL) ||
	    (pz_size == NULL) || (*pz_size == 0) ||
	    (ph_msgq_hdl_list == NULL) || (ui2_msgq_hdl_count == 0)) {
		return OSR_INV_ARG;
	}

	if (ui2_msgq_hdl_count != 1) {
		return OSR_NOT_SUPPORT;
	}

	pt_msg_q = (OS_MSGQ_LIGHT_T *) (ph_msgq_hdl_list[0]);
	if (pt_msg_q->read == pt_msg_q->write) {
		ret =
		    wait_event_timeout(pt_msg_q->write_wq, pt_msg_q->read != pt_msg_q->write,
				       ui4_time / quantum_ms);
		if (ret == 0) {
			return OSR_TIMEOUT;
		}
	}
	read = pt_msg_q->read;
	z_size = *(SIZE_T *) (read);
	if (z_size > *pz_size) {
		z_size = *pz_size;
	}
	memcpy(pv_msg, read + 4, z_size);
	read += pt_msg_q->message_size;
	if (read == pt_msg_q->end) {
		read = pt_msg_q->start;
	}
	pt_msg_q->read = read;
	wake_up_all(&pt_msg_q->read_wq);

	*pui2_index = 0;
	*pz_size = z_size;

	return OSR_OK;
}
EXPORT_SYMBOL(x_msg_q_receive_timeout);


INT32 x_msg_q_num_msgs(HANDLE_T h_msg_hdl, UINT16 *pui2_num_msgs)
{
	OS_MSGQ_LIGHT_T *pt_msg_q;
	SIZE_T messages;

	if (pui2_num_msgs == NULL) {
		return OSR_INV_ARG;
	}

	pt_msg_q = (OS_MSGQ_LIGHT_T *) (h_msg_hdl);

	messages = pt_msg_q->write - pt_msg_q->read;
	if (pt_msg_q->write < pt_msg_q->read) {
		messages += pt_msg_q->end - pt_msg_q->start;
	}
	messages /= pt_msg_q->message_size;
	*pui2_num_msgs = (UINT16) (messages);

	return OSR_OK;
}
EXPORT_SYMBOL(x_msg_q_num_msgs);


INT32 x_msg_q_get_max_msg_size(HANDLE_T h_msg_hdl, SIZE_T *pz_max_size)
{
	OS_MSGQ_LIGHT_T *pt_msg_q;

	if (pz_max_size == NULL) {
		return OSR_INV_ARG;
	}

	pt_msg_q = (OS_MSGQ_LIGHT_T *) (h_msg_hdl);
	*pz_max_size = pt_msg_q->z_maxsize;

	return OSR_OK;
}
EXPORT_SYMBOL(x_msg_q_get_max_msg_size);


INT32 msg_q_init(VOID)
{
	return OSR_OK;
}
