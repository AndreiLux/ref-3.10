#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "x_assert.h"
#include "x_os.h"


/* --------------------------------------------------------------------- */


#define MEMORY_ALIGNMENT        8

#define THREAD_NAME_LEN         16
#define THREAD_PRI_RANGE_LOW    (UINT8) 1
#define THREAD_PRI_RANGE_HIGH   (UINT8) 254
#define DEFAULT_TASK_SLICE      5


typedef struct os_thread_pvt_light {
	struct os_thread_pvt_light *previous;
	struct os_thread_pvt_light *next;

	UINT32 ui4_key;
	x_os_thread_pvt_del_fct pf_pvt_del;
	VOID *pv_pvt;
} OS_THREAD_PVT_LIGHT_T;


typedef struct os_thread_light {
	struct os_thread_light *previous;
	struct os_thread_light *next;
	struct task_struct *task;
	/* void *pv_stack; */
	/* SIZE_T z_stack_size; */
	CHAR s_name[THREAD_NAME_LEN + 1];
	UINT8 ui1_priority;
	x_os_thread_main_fct pf_main_rtn;
	OS_THREAD_PVT_LIGHT_T *pt_pvt;
	SIZE_T z_arg_size;
	UINT8 au1_arg_local[1];
} OS_THREAD_LIGHT_T;


/* static DEFINE_SPINLOCK(x_thread_lock); */
static DEFINE_RWLOCK(x_thread_lock);

/* --------------------------------------------------------------------- */


static UINT8 from_sched_priority(int sched_priority)
{
	return (UINT8) ((100 - sched_priority) * 256 / 100);
}


static int to_sched_priority(UINT8 ui1_priority)
{
	int sched_priority;
	sched_priority = 100 - (int)ui1_priority * 100 / 256;
	if (sched_priority < 1)
		sched_priority = 1;
	if (sched_priority > 99)
		sched_priority = 99;
	return sched_priority;
}


/* --------------------------------------------------------------------- */


static OS_THREAD_LIGHT_T *s_thread_list;


static void thread_list_add(OS_THREAD_LIGHT_T *pt_thread)
{
	if (s_thread_list != NULL) {
		pt_thread->previous = s_thread_list->previous;
		pt_thread->next = s_thread_list;
		s_thread_list->previous->next = pt_thread;
		s_thread_list->previous = pt_thread;
	} else {
		s_thread_list = pt_thread->next = pt_thread->previous = pt_thread;
	}
}


static void thread_list_remove(OS_THREAD_LIGHT_T *pt_thread)
{
	if (pt_thread->previous == pt_thread) {
		s_thread_list = NULL;
	} else {
		pt_thread->previous->next = pt_thread->next;
		pt_thread->next->previous = pt_thread->previous;
		if (s_thread_list == pt_thread) {
			s_thread_list = pt_thread->next;
		}
	}
}


static OS_THREAD_LIGHT_T *thread_find_handle(HANDLE_T h_th_hdl)
{
	OS_THREAD_LIGHT_T *pt_thread = s_thread_list;
	if (pt_thread == NULL) {
		return NULL;
	}
	do {
		if (pt_thread == (OS_THREAD_LIGHT_T *) (h_th_hdl)) {
			return pt_thread;
		}
		pt_thread = pt_thread->next;
	} while (pt_thread != s_thread_list);

	return NULL;
}


static OS_THREAD_LIGHT_T *thread_find_obj(const CHAR *ps_name)
{
	OS_THREAD_LIGHT_T *pt_thread = s_thread_list;
	if (pt_thread == NULL) {
		return NULL;
	}
	do {
		if (strncmp(pt_thread->s_name, ps_name, THREAD_NAME_LEN) == 0) {
			return pt_thread;
		}
		pt_thread = pt_thread->next;
	} while (pt_thread != s_thread_list);

	return NULL;
}


static void ThreadExit(void)
{
	unsigned long flags;
	OS_THREAD_LIGHT_T *pt_thread;
	struct task_struct *task;

	ASSERT(x_thread_self((HANDLE_T *) &pt_thread) == OSR_OK);

	/* local_irq_save(flags); */
	write_lock_irqsave(&x_thread_lock, flags);
	thread_list_remove(pt_thread);
	/* local_irq_restore(flags); */
	write_unlock_irqrestore(&x_thread_lock, flags);

	task = pt_thread->task;

	/* kfree(pt_thread->pv_stack); */
	kfree(pt_thread);

	complete_and_exit(NULL, 0);
}


static int ThreadProc(void *arg)
{
	OS_THREAD_LIGHT_T *pt_thread = (OS_THREAD_LIGHT_T *) arg;
	ASSERT(pt_thread != NULL);

	/* Invoke the original thread function */
	pt_thread->pf_main_rtn(pt_thread->z_arg_size != 0 ? pt_thread->au1_arg_local : NULL);

	/* Terminate thread */
	ThreadExit();

	return 0;
}


INT32 x_thread_create(HANDLE_T *ph_th_hdl,
		      const CHAR *ps_name,
		      SIZE_T z_stack_size,
		      UINT8 ui1_priority,
		      x_os_thread_main_fct pf_main_rtn, SIZE_T z_arg_size, VOID *pv_arg)
{
	OS_THREAD_LIGHT_T *pt_thread;
	/* void *pv_stack; */
	unsigned long flags;

	if (pv_arg == NULL) {
		z_arg_size = 0;
	}

	/* check arguments */
	if ((ps_name == NULL) || (ps_name[0] == '\0') || (ph_th_hdl == NULL) ||
	    /* (z_stack_size == 0) || */
	    (pf_main_rtn == NULL) ||
	    (ui1_priority < THREAD_PRI_RANGE_LOW) || (ui1_priority > THREAD_PRI_RANGE_HIGH) ||
	    ((pv_arg != NULL) && (z_arg_size == 0)) || ((pv_arg == NULL) && (z_arg_size != 0))) {
		return OSR_INV_ARG;
	}
	/* Make sure the stack size is aligned */
	/* z_stack_size = (z_stack_size + MEMORY_ALIGNMENT - 1) & (~(MEMORY_ALIGNMENT - 1)); */

	pt_thread = kcalloc(1, sizeof(OS_THREAD_LIGHT_T) - sizeof(UINT8) + z_arg_size, GFP_KERNEL);
	/* pv_stack = kcalloc(1, z_stack_size); */
	if (pt_thread == NULL /*|| pv_stack == NULL */) {
		/* kfree(pv_stack); */
		kfree(pt_thread);
		return OSR_NO_RESOURCE;
	}
	/* FILL_CALLER(pt_thread); */
	/* FILL_CALLER(pv_stack); */

	/* pt_thread->pv_stack = pv_stack; */
	strncpy(pt_thread->s_name, ps_name, THREAD_NAME_LEN);
	pt_thread->pf_main_rtn = pf_main_rtn;
	pt_thread->z_arg_size = z_arg_size;
	if (z_arg_size != 0) {
		memcpy(pt_thread->au1_arg_local, pv_arg, z_arg_size);
	}
	/* local_irq_save(flags); */
	write_lock_irqsave(&x_thread_lock, flags);
	if (thread_find_obj(ps_name) != NULL) {
		/* local_irq_restore(flags); */
		write_unlock_irqrestore(&x_thread_lock, flags);
		/* kfree(pv_stack); */
		kfree(pt_thread);
		return OSR_EXIST;
	}

	thread_list_add(pt_thread);
	/* local_irq_restore(flags); */
	write_unlock_irqrestore(&x_thread_lock, flags);

	pt_thread->task = kthread_create(&ThreadProc, pt_thread, ps_name);
	if (pt_thread->task == ERR_PTR(-ENOMEM)) {
		/* local_irq_save(flags); */
		write_lock_irqsave(&x_thread_lock, flags);
		thread_list_remove(pt_thread);
		/* local_irq_restore(flags); */
		write_unlock_irqrestore(&x_thread_lock, flags);
		/* kfree(pv_stack); */
		kfree(pt_thread);
		return OSR_NO_RESOURCE;
	} else if (IS_ERR(pt_thread->task)) {
		/* local_irq_save(flags); */
		write_lock_irqsave(&x_thread_lock, flags);
		thread_list_remove(pt_thread);
		/* local_irq_restore(flags); */
		write_unlock_irqrestore(&x_thread_lock, flags);
		/* kfree(pv_stack); */
		kfree(pt_thread);
		return OSR_FAIL;
	} else {
		struct sched_param param;
		int ret;

		param.sched_priority = to_sched_priority(ui1_priority);
		ret = sched_setscheduler_nocheck(pt_thread->task, SCHED_RR, &param);
		ASSERT(ret == 0);
		pt_thread->ui1_priority = from_sched_priority(param.sched_priority);
	}

	wake_up_process(pt_thread->task);

	*ph_th_hdl = (HANDLE_T) (pt_thread);
	return OSR_OK;
}
EXPORT_SYMBOL(x_thread_create);


VOID x_thread_exit(VOID)
{
	ThreadExit();
}
EXPORT_SYMBOL(x_thread_exit);


VOID x_thread_delay(UINT32 ui4_delay)
{
	ASSERT(!in_interrupt());

	if (ui4_delay == 0) {
		yield();
	} else {
		msleep(ui4_delay);
	}
}
EXPORT_SYMBOL(x_thread_delay);


INT32 x_thread_set_pri(HANDLE_T h_th_hdl, UINT8 ui1_new_pri)
{
	OS_THREAD_LIGHT_T *pt_thread;
	unsigned long flags;

	if ((ui1_new_pri < THREAD_PRI_RANGE_LOW) || (ui1_new_pri > THREAD_PRI_RANGE_HIGH)) {
		return OSR_INV_ARG;
	}
	/* local_irq_save(flags); */
	write_lock_irqsave(&x_thread_lock, flags);
	pt_thread = thread_find_handle(h_th_hdl);
	if (pt_thread == NULL) {
		/* local_irq_restore(flags); */
		write_unlock_irqrestore(&x_thread_lock, flags);
		return OSR_INV_HANDLE;
	}
	{
		struct sched_param param;
		int ret;

		param.sched_priority = to_sched_priority(ui1_new_pri);
		ret = sched_setscheduler_nocheck(pt_thread->task, SCHED_RR, &param);
		ASSERT(ret == 0);
		pt_thread->ui1_priority = from_sched_priority(param.sched_priority);
	}
	/* local_irq_restore(flags); */
	write_unlock_irqrestore(&x_thread_lock, flags);

	return OSR_OK;
}
EXPORT_SYMBOL(x_thread_set_pri);


INT32 x_thread_get_pri(HANDLE_T h_th_hdl, UINT8 *pui1_pri)
{
	OS_THREAD_LIGHT_T *pt_thread;
	unsigned long flags;

	if (pui1_pri == NULL) {
		return OSR_INV_ARG;
	}
	/* local_irq_save(flags); */
	read_lock_irqsave(&x_thread_lock, flags);
	pt_thread = thread_find_handle(h_th_hdl);
	if (pt_thread == NULL) {
		/* local_irq_restore(flags); */
		read_unlock_irqrestore(&x_thread_lock, flags);
		return OSR_INV_HANDLE;
	}
	*pui1_pri = pt_thread->ui1_priority;
	/* local_irq_restore(flags); */
	read_unlock_irqrestore(&x_thread_lock, flags);

	return OSR_OK;
}
EXPORT_SYMBOL(x_thread_get_pri);


INT32 x_thread_get_name(HANDLE_T h_th_hdl, UINT32 *s_name)
{
	OS_THREAD_LIGHT_T *pt_thread;
	unsigned long flags;

	/* local_irq_save(flags); */
	read_lock_irqsave(&x_thread_lock, flags);
	pt_thread = thread_find_handle(h_th_hdl);
	if (pt_thread == NULL) {
		/* local_irq_restore(flags); */
		read_unlock_irqrestore(&x_thread_lock, flags);
		return OSR_INV_HANDLE;
	}
	*s_name = (UINT32) &pt_thread->s_name[0];
	/* local_irq_restore(flags); */
	read_unlock_irqrestore(&x_thread_lock, flags);

	return OSR_OK;
}
EXPORT_SYMBOL(x_thread_get_name);


VOID x_thread_suspend(VOID)
{
	ASSERT(0);
}
EXPORT_SYMBOL(x_thread_suspend);


INT32 x_thread_resume(HANDLE_T h_th_hdl)
{
	return OSR_NOT_SUPPORT;
}
EXPORT_SYMBOL(x_thread_resume);


INT32 x_thread_self(HANDLE_T *ph_th_hdl)
{
	OS_THREAD_LIGHT_T *pt_thread;
	unsigned long flags;

	if (ph_th_hdl == NULL) {
		return OSR_INV_ARG;
	}
	/* local_irq_save(flags); */
	read_lock_irqsave(&x_thread_lock, flags);
	pt_thread = s_thread_list;
	if (pt_thread == NULL) {
		return OSR_NOT_EXIST;
	}
	do {
		if (pt_thread->task == current) {
			/* local_irq_restore(flags); */
			read_unlock_irqrestore(&x_thread_lock, flags);
			*ph_th_hdl = (HANDLE_T) (pt_thread);
			return OSR_OK;
		}
		pt_thread = pt_thread->next;
	} while (pt_thread != s_thread_list);
	/* local_irq_restore(flags); */
	read_unlock_irqrestore(&x_thread_lock, flags);

	return OSR_NOT_EXIST;
}
EXPORT_SYMBOL(x_thread_self);


INT32 x_thread_stack_stats(HANDLE_T h_th_hdl, SIZE_T *pz_alloc_stack, SIZE_T *pz_max_used_stack)
{
	return OSR_NOT_SUPPORT;
}
EXPORT_SYMBOL(x_thread_stack_stats);


INT32 x_thread_set_pvt(UINT32 ui4_key, x_os_thread_pvt_del_fct pf_pvt_del, VOID *pv_pvt)
{
	return OSR_NOT_SUPPORT;
}
EXPORT_SYMBOL(x_thread_set_pvt);


INT32 x_thread_get_pvt(UINT32 ui4_key, VOID **ppv_pvt)
{
	return OSR_NOT_SUPPORT;
}
EXPORT_SYMBOL(x_thread_get_pvt);


INT32 x_thread_del_pvt(UINT32 ui4_key)
{
	return OSR_NOT_SUPPORT;
}
EXPORT_SYMBOL(x_thread_del_pvt);


INT32 os_thread_init(VOID)
{
	return OSR_OK;
}


INT32 os_cli_show_thread_all(INT32 i4_argc, const CHAR **pps_argv)
{
	return OSR_OK;
}
EXPORT_SYMBOL(os_cli_show_thread_all);
