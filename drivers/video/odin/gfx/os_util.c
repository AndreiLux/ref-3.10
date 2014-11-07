/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See th
 * GNU General Public License for more details.
 */

/*       
  
                                
  
                                                  
                
                     
  
                           
     
 */

/*--------------------------------------------------------------------
	Control Constants
---------------------------------------------------------------------*/

/*--------------------------------------------------------------------
	File Inclusions
---------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/hardirq.h>
#include <linux/dma-mapping.h>
#include <linux/version.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <linux/version.h>

/* #include "tlsf.h" */
/*#include "debug_util.h" */
#include "os_util.h"
/*#include "debug_util.h" */
/*#include "base_device.h" */

/*--------------------------------------------------------------------
	Constant Definitions
---------------------------------------------------------------------*/


/*--------------------------------------------------------------------
	Macro Definitions
---------------------------------------------------------------------*/
static struct task_struct *g_csowner = NULL;
static struct class *kdrv_class = NULL;

#define SET_CRITICAL() 		do { g_csowner = current; } while (0)
#define CLEAR_CRITICAL() 	do { g_csowner = NULL; } while (0)
#define CHECK_CRITICAL() 	( g_csowner == current || in_interrupt() )

/*
 * critical resource lock/unlock
 */

#define OS_GLOBAL_LOCK()	\
	do { spin_lock_irqsave(&_g_res_lock, flags);} while (0)
#define OS_GLOBAL_UNLOCK()	\
	do { spin_unlock_irqrestore(&_g_res_lock, flags); } while (0)

#define	MSEC2TICKS(msec)	( ( (msec) * HZ) / 1000 )

/*
 * OS internal debug macroes
*/

/*--------------------------------------------------------------------
	Type Definitions
---------------------------------------------------------------------*/

/*--------------------------------------------------------------------
	External Function Prototype Declarations
---------------------------------------------------------------------*/

/*--------------------------------------------------------------------
	External Variables
---------------------------------------------------------------------*/

/*--------------------------------------------------------------------
	global Variables
---------------------------------------------------------------------*/

/*--------------------------------------------------------------------
	Static Function Prototypes Declarations
---------------------------------------------------------------------*/

/*--------------------------------------------------------------------
	Static Variables
---------------------------------------------------------------------*/

/**
 * spinlock for resource protection
 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
static	spinlock_t		_g_res_lock = SPIN_LOCK_UNLOCKED;
#else
static DEFINE_SPINLOCK(_g_res_lock);
#endif

/*--------------------------------------------------------------------
	Implementation Group
---------------------------------------------------------------------*/


/*--------------------------------------------------------------------
	DEVICE REGISTER
---------------------------------------------------------------------*/
void os_createdeviceclass    ( dev_t dev, const char* fmt, ... )
{
	va_list args;
	char dev_name[LX_MAX_DEVICE_NAME];

	va_start(args, fmt);
	vsnprintf( dev_name, LX_MAX_DEVICE_NAME, fmt, args );
	va_end(args);

	device_create( kdrv_class, NULL, dev, NULL, dev_name );
}
EXPORT_SYMBOL(os_createdeviceclass);

void os_destroydeviceclass   ( dev_t dev )
{
	device_destroy( kdrv_class, dev );
}

/*--------------------------------------------------------------------
	MUTEX OS API IMPLEMENTATION
---------------------------------------------------------------------*/
void os_initmutex_tag	( OS_SEM_T* pSem , UINT32 attr )
{
	PARAM_UNUSED(attr);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
	init_MUTEX(&(pSem->sem));
#else
	sema_init(&(pSem->sem),1);
#endif
	init_waitqueue_head(&(pSem->wq));
}

int os_lockmutex_tag	( OS_SEM_T* pSem, UINT32 timeout,
					const char* szFile, const char* szFunc, const int nLine )
{
	ULONG	flags;
	int		ret = RET_OK;

	/* interrupt handler should not lock mutex with wait option */
	if ( timeout !=0 && CHECK_CRITICAL() )
	{
		printk("Ooops. OS_LockMutex called at ISR. force timeout from \
			%d to 0.\n", timeout );

		timeout = 0;
	}

	/* normally, ISR should call OS_LockMutex with zero timeout */
	if ( timeout == 0x0 )
	{
		ret = down_trylock(&(pSem->sem));

		if ( RET_OK != ret )
		{
			printk("OS_LockMutex timeout. ret=%d\n", ret );
		}
	}
	/* normal process seems to call OS_LockMutex with timoeut option */
	else if ( timeout == OS_INFINITE_WAIT )
	{
		ret = down_interruptible(&(pSem->sem));

		if ( RET_OK != ret )
		{
			printk("OS_LockMutex failed. ret=%d\n", ret );
		}
	}
	else
	/* if timeout option is enabled, */
	{
		ULONG			ticks;
		wait_queue_t	wqe;

		ticks = MSEC2TICKS(timeout);
		pSem->wait_loop = TRUE;

		/* make wait_queue */
		init_waitqueue_entry(&wqe, current);
		add_wait_queue(&(pSem->wq), &wqe);

		for ( ; ; )
		{
			/* normally, current task goes to sleep before checking condition.
			 * when event is sent by another task or ISR, current task will
			 * escape from schedule_timeout(). this is what I want.
			 */
			set_current_state( TASK_INTERRUPTIBLE );

			ret = down_trylock(&(pSem->sem));

			/* good, I've got semaphore */
			if ( RET_OK == ret )
			{
				ret = RET_OK; break;
			}
			/* event not yet arrived, check timeout */
			else if (!ticks)
			{
				printk("OS_LockMutex timeout. ret=%d\n", ret );
				ret = RET_TIMEOUT; break;
			}
			else
			{
				ticks = schedule_timeout(ticks);

				printk("OS_LockMutex : timeout interrupt (ticks:%d)\n",
					(int)ticks );
				if ( signal_pending(current) )
				{
					printk("OS_LockMutex signal interrrupt.\n" );
					ret = RET_INTR_CALL; break;
				}
			}
		}

	   	set_current_state(TASK_RUNNING);
	    remove_wait_queue(&(pSem->wq), &wqe);
	}

	if ( RET_OK == ret )
	{
		OS_GLOBAL_LOCK();
		pSem->wait_loop = FALSE;
		OS_GLOBAL_UNLOCK();

/*		printk("OS_LockMutex (%p): done (tm:%d)\n", pSem, timeout); */
	}

	return ret;
}

/**
 * unlock all processes blocked by mutex
 *
 * @param pSem [IN] pointer to semaphore
 */
void os_unlockmutex_tag ( OS_SEM_T* pSem,
				const char* szFile, const char* szFunc, const int nLine )
{
	ULONG	flags;
	BOOLEAN	is_wait_loop = FALSE;

	OS_GLOBAL_LOCK();
	is_wait_loop 	= pSem->wait_loop;
	pSem->wait_loop = FALSE;
	OS_GLOBAL_UNLOCK();

	/* unlock mutex first and wake up the sleeping task */
/*	printk("OS_UnlockMutex (%p): done (loop:%d)\n", pSem, is_wait_loop); */
	up(&(pSem->sem));
	if (is_wait_loop) wake_up_interruptible(&(pSem->wq));
}

/*--------------------------------------------------------------------
	TIME/TICK OS API IMPLEMENTATION
---------------------------------------------------------------------*/
int os_msecsleep_tag ( UINT32 msec, const char* szFile,
	const char* szFunc, const int nLine )
{
	PARAM_UNUSED(szFile);
	PARAM_UNUSED(szFunc);
	PARAM_UNUSED(nLine);
	msleep(msec);

	return RET_OK;
}

void os_nsecdelay_tag 	( UINT32 nsec, const char* szFile,
	const char* szFunc, const int nLine )
{
	PARAM_UNUSED(szFile);
	PARAM_UNUSED(szFunc);
	PARAM_UNUSED(nLine);

	ndelay ( nsec );
}

void os_usecdelay_tag 	( UINT32 usec, const char* szFile,
	const char* szFunc, const int nLine )
{
	PARAM_UNUSED(szFile);
	PARAM_UNUSED(szFunc);
	PARAM_UNUSED(nLine);

	udelay ( usec );
}

/**
 * get mili seconds
 *
 */
UINT64 os_getmsecticks_tag ( void )
{
    struct timespec ts;
    ts = current_kernel_time();

    return (UINT64)(ts.tv_sec * 1000 + ts.tv_nsec/1000000);
}

/**
 * get micro seconds
 *
 * TOOD) get the real micro seconds
 */
UINT64 os_getusecticks_tag ( void )
{
    struct timespec ts;
    ts = current_kernel_time();

    return (UINT64)(ts.tv_sec * 1000000 + ts.tv_nsec/1000);
}

/**
 * get current (sec, msec, usec) information
 *
 *
 */
void os_getcurrentticks_tag ( UINT32* pSec, UINT32* pMSec, UINT32* pUSec )
{
    struct timespec ts;
    ts = current_kernel_time();

    if ( pSec )	*pSec = ts.tv_sec;
    if ( pMSec) *pMSec= ts.tv_nsec/1000000;
    if ( pUSec) *pUSec= ts.tv_nsec/1000;
}


/*--------------------------------------------------------------------
	MEMORY OS API IMPLEMENTATION
---------------------------------------------------------------------*/
void* os_kmalloc_tag ( size_t size, const char* szFile,
	const char* szFunc, const int nLine )
{
	void *ptr = NULL;
	PARAM_UNUSED(szFile);
	PARAM_UNUSED(szFunc);
	PARAM_UNUSED(nLine);

    if ( (ptr = kmalloc(size, GFP_KERNEL)) ) memset( ptr, 0x0, size );

	return ptr;
}

void os_kfree_tag ( void* ptr, const char* szFile, const char* szFunc,
	const int nLine )
{
	PARAM_UNUSED(szFile);
	PARAM_UNUSED(szFunc);
	PARAM_UNUSED(nLine);

	if (ptr)  kfree(ptr);
}



/** @} */

