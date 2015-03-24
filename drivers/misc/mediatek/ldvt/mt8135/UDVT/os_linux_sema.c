#include <linux/module.h>
#include <linux/param.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/thread_info.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif
#include "x_assert.h"
#include "x_os.h"


#define INVALID_OWNER ((struct thread_info *)(NULL))


typedef struct os_sema_light {
	struct os_sema_light *previous;
	struct os_sema_light *next;
	SEMA_TYPE_T e_type;
	struct semaphore sem;
	struct thread_info *owner;
	INT16 i2_selfcount;
} OS_SEMA_LIGHT_T;

static OS_SEMA_LIGHT_T *s_sema_list;
static DEFINE_SPINLOCK(x_sema_list_lock);
static void sema_list_add(OS_SEMA_LIGHT_T *pt_sema)
{
	if (s_sema_list != NULL) {
		pt_sema->previous = s_sema_list->previous;
		pt_sema->next = s_sema_list;
		s_sema_list->previous->next = pt_sema;
		s_sema_list->previous = pt_sema;
	} else {
		s_sema_list = pt_sema->next = pt_sema->previous = pt_sema;
	}
}


static void sema_list_remove(HANDLE_T h_sema_hdl)
{
	OS_SEMA_LIGHT_T *pt_sema = (OS_SEMA_LIGHT_T *) h_sema_hdl;
	if (pt_sema->previous == pt_sema) {
		s_sema_list = NULL;
	} else {
		pt_sema->previous->next = pt_sema->next;
		pt_sema->next->previous = pt_sema->previous;
		if (s_sema_list == pt_sema) {
			s_sema_list = pt_sema->next;
		}
	}
}


INT32 sema_find_obj(HANDLE_T h_sema_hdl)
{
	OS_SEMA_LIGHT_T *pt_sema = (OS_SEMA_LIGHT_T *) h_sema_hdl;
	OS_SEMA_LIGHT_T *pt_sema_lpointer = s_sema_list;
	if (pt_sema_lpointer == NULL) {
		return 0;
	}
	do {
		if (pt_sema_lpointer == pt_sema) {
			return 1;
		}
		pt_sema_lpointer = pt_sema_lpointer->next;
	} while (pt_sema_lpointer != s_sema_list);

	return 0;
}


INT32 x_sema_create(HANDLE_T *ph_sema_hdl, SEMA_TYPE_T e_type, UINT32 ui4_init_value)
{
	OS_SEMA_LIGHT_T *pt_sema;
	unsigned long flags;

	/* check arguments */
	if ((ph_sema_hdl == NULL) ||
	    ((e_type != X_SEMA_TYPE_BINARY) && (e_type != X_SEMA_TYPE_MUTEX) &&
	     (e_type != X_SEMA_TYPE_COUNTING))) {
		return OSR_INV_ARG;
	}

	if (((e_type == X_SEMA_TYPE_BINARY) || (e_type == X_SEMA_TYPE_MUTEX)) &&
	    (ui4_init_value != X_SEMA_STATE_LOCK) && (ui4_init_value != X_SEMA_STATE_UNLOCK)) {
		return OSR_INV_ARG;
	}

	if ((e_type == X_SEMA_TYPE_COUNTING) &&
	    (((INT32) ui4_init_value) < ((INT32) X_SEMA_STATE_LOCK))) {
		return OSR_INV_ARG;
	}

	pt_sema = kcalloc(1, sizeof(OS_SEMA_LIGHT_T), GFP_KERNEL);
	if (pt_sema == NULL) {
		return OSR_NO_RESOURCE;
	}
	/* FILL_CALLER(pt_sema); */

	pt_sema->e_type = e_type;
	sema_init(&pt_sema->sem, ui4_init_value);

	if (e_type == X_SEMA_TYPE_MUTEX) {
		if (ui4_init_value == X_SEMA_STATE_LOCK) {
			pt_sema->owner = current_thread_info();
			pt_sema->i2_selfcount++;
		} else {
			pt_sema->owner = INVALID_OWNER;
			pt_sema->i2_selfcount = 0;
		}
	}

	spin_lock_irqsave(&x_sema_list_lock, flags);
	sema_list_add(pt_sema);
	spin_unlock_irqrestore(&x_sema_list_lock, flags);

	*ph_sema_hdl = (HANDLE_T) (pt_sema);
	return OSR_OK;
}
EXPORT_SYMBOL(x_sema_create);


INT32 x_sema_delete(HANDLE_T h_sema_hdl)
{
	unsigned long flags;
	OS_SEMA_LIGHT_T *pt_sema = (OS_SEMA_LIGHT_T *) (h_sema_hdl);

	spin_lock_irqsave(&x_sema_list_lock, flags);
	if (!sema_find_obj(h_sema_hdl))	/* error handle */
	{
		spin_unlock_irqrestore(&x_sema_list_lock, flags);
		printk(KERN_ERR "[x_sema_delete]error: can not find this handle!\n");
		dump_stack();
		pt_sema = NULL;
		kfree(pt_sema);	/* added for Kloc work issue */
		return OSR_INV_HANDLE;
	}
	sema_list_remove(h_sema_hdl);
	spin_unlock_irqrestore(&x_sema_list_lock, flags);

	kfree(pt_sema);
	return OSR_OK;
}
EXPORT_SYMBOL(x_sema_delete);


INT32 x_sema_lock(HANDLE_T h_sema_hdl, SEMA_OPTION_T e_option)
{
	OS_SEMA_LIGHT_T *pt_sema = (OS_SEMA_LIGHT_T *) (h_sema_hdl);

	if (e_option != X_SEMA_OPTION_WAIT && e_option != X_SEMA_OPTION_NOWAIT) {
		return OSR_INV_ARG;
	}

	if (pt_sema->e_type == X_SEMA_TYPE_MUTEX) {
		if (pt_sema->owner != current_thread_info()) {
			if (e_option == X_SEMA_OPTION_NOWAIT) {
				if (down_trylock(&pt_sema->sem) != 0) {
					return OSR_WOULD_BLOCK;
				}
			} else {
				down(&pt_sema->sem);
			}
			pt_sema->owner = current_thread_info();
		}
		pt_sema->i2_selfcount++;
		return OSR_OK;
	} else {
		if (e_option == X_SEMA_OPTION_NOWAIT) {
			if (down_trylock(&pt_sema->sem) != 0) {
				return OSR_WOULD_BLOCK;
			}
		} else {
			down(&pt_sema->sem);
		}

		return OSR_OK;
	}
}
EXPORT_SYMBOL(x_sema_lock);


INT32 x_sema_lock_timeout(HANDLE_T h_sema_hdl, UINT32 ui4_time)
{
	static const int quantum_ms = 1000 / HZ;
	OS_SEMA_LIGHT_T *pt_sema = (OS_SEMA_LIGHT_T *) (h_sema_hdl);
	int ret;

	if (ui4_time == 0) {
		INT32 i4 = x_sema_lock(h_sema_hdl, X_SEMA_OPTION_NOWAIT);
		return i4 != OSR_WOULD_BLOCK ? i4 : OSR_TIMEOUT;
	}

	if (pt_sema->e_type == X_SEMA_TYPE_MUTEX) {
		if (pt_sema->owner != current_thread_info()) {
			ret = down_timeout(&pt_sema->sem, ui4_time / quantum_ms);
			if (ret != 0) {
				goto err;
			}
			pt_sema->owner = current_thread_info();
		}
		pt_sema->i2_selfcount++;
		return OSR_OK;
	} else {
		ret = down_timeout(&pt_sema->sem, ui4_time / quantum_ms);
		if (ret != 0) {
			goto err;
		}

		return OSR_OK;
	}

 err:
	switch (ret) {
	case -ETIME:
		return OSR_TIMEOUT;

	default:
		return OSR_FAIL;
	}
}
EXPORT_SYMBOL(x_sema_lock_timeout);


INT32 x_sema_unlock(HANDLE_T h_sema_hdl)
{
	OS_SEMA_LIGHT_T *pt_sema = (OS_SEMA_LIGHT_T *) (h_sema_hdl);

	if (pt_sema->e_type == X_SEMA_TYPE_MUTEX) {
		if (pt_sema->owner != current_thread_info()) {
			return OSR_FAIL;
		}
		pt_sema->i2_selfcount--;
		if (pt_sema->i2_selfcount != 0) {
			return OSR_OK;
		}
		pt_sema->owner = INVALID_OWNER;

		up(&pt_sema->sem);
		return OSR_OK;
	} else {
		up(&pt_sema->sem);
		return OSR_OK;
	}
}
EXPORT_SYMBOL(x_sema_unlock);


INT32 os_sema_init(VOID)
{
	return OSR_OK;
}


INT32 os_cli_show_sema_all(INT32 i4_argc, const CHAR **pps_argv)
{
	return OSR_OK;
}
EXPORT_SYMBOL(os_cli_show_sema_all);
