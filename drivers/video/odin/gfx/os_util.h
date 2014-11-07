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

#ifndef	_OS_UTIL_H_
#define	_OS_UTIL_H_

/*--------------------------------------------------------------------
	Control Constants
----------------------------------------------------------------------*/

/*--------------------------------------------------------------------
    File Inclusions
----------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include "base_types.h"


#ifdef	__cplusplus
extern "C"
{
#endif /* __cplusplus */

/*--------------------------------------------------------------------
	Constant Definitions
----------------------------------------------------------------------*/
#define		LX_MAX_DEVICE_NAME		12
#define		LX_MAX_DEVICE_NUM		64

/*--------------------------------------------------------------------
    Type Definitions
----------------------------------------------------------------------*/
/** @name Type Definition for OS Common
 *
 * @{
 *
 *	@def OS_NO_WAIT
 *	timeout option with no wait
 *
 *	function with this timeout never wait until it can acquire the mutex.
 *	try and fail if mutex is not available.
 *	this option is used in interrupt handler.
 *
 *	@see OS_LockMutexEx, OS_RecvEvent
 *
 *	@def OS_INFINITE_WAIT
 *	timeout option with infinite wait
 *
 *	function with this timeout wait forever until it can acquire the mutex.
 *	this option is used in normal task.
*/
#define OS_NO_WAIT				0
#define OS_INFINITE_WAIT		((u32)-1)

/**
 * @def OS_SEM_ATTR_DEFAULT
 * @brief default semaphore attribute ( not recursive )
 */
#define OS_SEM_ATTR_DEFAULT     0

/**
 * @def OS_EVENT_RECEIVE_ANY
 * option for OS_RecvEvent. wait for one of requested event.
 *
 *
 * @def OS_EVENT_RECEIVE_ALL
 * option for OS_RecvEvent. wait for all of requested event.
 *
*/
#define OS_EVENT_RECEIVE_ANY	0
#define OS_EVENT_RECEIVE_ALL	1

/**
 * @def OS_TIMER_TIMEOUT
 * timeout is issued onece (one short timer).
 *
 * @def OS_TIMER_TIMETICK
 * timetick is issued perodically.
 */
#define	OS_TIMER_TIMEOUT		(1<<0)
#define	OS_TIMER_TIMETICK		(1<<1)
/** @} */


/**
 *	type definition for semaphore structure
 */
typedef struct
{
	struct semaphore	sem; /* semaphore structure in Linux */
	wait_queue_head_t	wq;
		/*  wait_queue_head structure in Linux. wait for timed wait. */
	u32					wait_loop:1,
		/*  mutex lock with timeout requested */
						rsvd:31;
}OS_SEM_T;


typedef struct
{
	void *phys_mem_addr;	/*  start address of physical memory */
	u32	length;			/*  total length of physical memory */
	u32	block_size;		/*  mimimum allocation size */
	u32	mem_alloc_cnt;	/*  allocation count */
	u32	mem_alloc_size;	/*  allocated memory size */
	u32	mem_free_size;
		/*  free memory size ( real memory will be less than free size  ) */
}OS_RGN_INFO_T;

/**
 * type definition for the cached operation
 */
typedef struct
{
	void *phys_addr;
	void *virt_addr;
	u32	length;
}OS_CACHE_MAP_T;

/*--------------------------------------------------------------------
	MUTEX OS API
--------------------------------------------------------------------*/

/**
 * @name Function Definition for OS Mutex(Semaphore)
 * os functions for mutex(semaphore)
 *
 * @{
 *
 * @def OS_InitMutex
 * initialize mutex
 *
 * @param pSem [IN] pointer to OS_SEM_T
 * @param attr [IN] attribute of mutex
 * ( one of OS_SEM_ATTR_DEFAULT, OS_SEM_ATTR_RECURSIVE )
 */
#define OS_InitMutex(pSem,attr)		os_initmutex_tag(pSem,attr)
#ifdef CONFIG_ODIN_DUAL_GFX
#define OS_InitMutex1(pSem,attr)	os_initmutex_tag1(pSem,attr)
#endif

/**
 * @def OS_LockMutexEx
 * lock mutex.
 *
 * @param pSem [IN] pointer to OS_SEM_T
 * @param timeout [IN] timeout value (msec)
 * @return RET_OK if success, RET_TIMEOUT if timeout, none zero value for
 * otherwise
 *
 * when timeout is OS_INFINITE_WAIT, task may be blocked until it gets
 * the mutex.
 *
 * when timeout is OS_NO_WAIT
 *
 * As you know, You should use OS_TrylockMutex in interrupt service handler.
 * OS_TrylockMutex never sleeps (non-blocking mechanim);
 * If semaphore is available at the time it will return 0 (ok).
 * If the semaphore is not available at the time of the call, it returns
 * immediately with a nonzero return value.
 *
 * NOTE: As you know, done't call OS_MUTEX_LOCK in interrupt service handler.
 */

#define OS_LockMutex(pSem)	\
	os_lockmutex_tag(pSem,OS_INFINITE_WAIT, __FILE__, __FUNCTION__, __LINE__)
#ifdef CONFIG_ODIN_DUAL_GFX
#define OS_LockMutex1(pSem)	\
	os_lockmutex_tag(pSem,OS_INFINITE_WAIT, __FILE__, __FUNCTION__, __LINE__)
#endif

/**
 * @def OS_UnlockMutex
 * unlock mutex.
 *
 * @param pSem [IN] pointer to OS_SEM_T
 * @return NONE
 */
#define OS_UnlockMutex(pSem)	\
	os_unlockmutex_tag(pSem, __FILE__, __FUNCTION__, __LINE__)
#ifdef CONFIG_ODIN_DUAL_GFX
#define OS_UnlockMutex1(pSem)	\
	os_unlockmutex_tag1(pSem, __FILE__, __FUNCTION__, __LINE__)
#endif

/*--------------------------------------------------------------------
	TIME/TIMER OS API
--------------------------------------------------------------------*/
/**
 * @name Function Definition for OS Time/Timer
 * os functions for time/timer
 *
 * @{
*/


/**
 @def OS_MsecSleep
 general sleep (mili second).
 @param msec [IN] msec time
 @return RET_OK if success, RET_INTR_CALL if sleep is cancelled
 		by user interrupt.\n
        You should check return value if time is very crtical to operation.

 @def OS_NsecDelay
 busy wait delay (nano second).
 @param nsec [IN] nano second time
 @return NONE

 Remember that the these delay functions are busy-waiting.\n
 Pros : I think delay is very accurate than the normal sleep or
 schedule_timeout.\n
 Cons : Other tasks can't be run during the time interval.
 Thus, these functions should only
 be used when there is no practical alternative.\n

 @def OS_UsecDelay
 busy wait delay (micro second).
 @param usec [IN] micro second time
 @return NONE

 Remember that the these delay functions are busy-waiting.\n
 Pros : I think delay is very accurate than the normal sleep or
 schedule_timeout.\n
 Cons : Other tasks can't be run during the time interval.
 Thus, these functions should only
 be used when there is no practical alternative.\n

 @def OS_GETMSECTICKS
 @param NONE
 @return ticks in unit of mili second (UINT64)

 get current time ticks in unit of microsecond.
 this functions uses current_kernel_time() api.

 @def OS_GETUSECTICKS
 @param NONE
 @return ticks in unit of micro second (UINT64)

 current time ticks in unit of microsecond.
 function uses current_kernel_time() api and

 @def OS_GetCurrentTicks
 elapsed time tick.
 @param pSec [OUT]  seconds ( can be NULL )
 @param pMsec [OUT] mili seconds ( can be NULL )
 @param pUsec [OUT] micro seconds ( can be NULL )
 @return NONE

 functions uses current_kernel_time() api.
 function can returns elapsed tick time in unit of second
 or mili second or micro second.

 example, If you want to get elapsed mili second, use to following code:

	@verbatim
	u32 msecTime
	u32 usecTime;
	OS_GetCurrentTicks(NULL,&msecTime,NULL);
	printk("elapsed ticks in milisecond  = %d\n", msecTime );
	@endverbatim

*/
#define OS_MsecSleep(msec)	\
	os_msecsleep_tag(msec, __FILE__, __FUNCTION__, __LINE__)
#define OS_NsecDelay(nsec)	\
	os_nsecdelay_tag(nsec, __FILE__, __FUNCTION__, __LINE__)
#define OS_UsecDelay(usec)	\
	os_usecdelay_tag(usec, __FILE__, __FUNCTION__, __LINE__)
#define OS_GETMSECTICKS()	os_getmsecticks_tag()
#define OS_GETUSECTICKS()	os_getusecticks_tag()
#define OS_GetCurrentTicks(pSec,pMsec,pUsec)	\
	os_getcurrentticks_tag(pSec,pMsec,pUsec)

/*--------------------------------------------------------------------
	MEMORY OS API
--------------------------------------------------------------------*/

/**
 * @name Function Defnition for OS Memory
 * os functions for linux memory operation
 *
 * @{
 *
 * @def OS_KMalloc
 *	allocate kernel memory using kmalloc
 *	@param size [IN] byte size of requested memory
 *	@return pointer to the allocated memory
 *
 *
 * @def OS_KFree
 *	free kernel memory using kfree.
 *	@param ptr [IN] memory pointer to free
 *	@return none
 *
 * @def OS_Malloc
 *	alias for OS_KMalloc.
 *	@param size [IN] byte size of requested memory
 *
 * @def OS_Free
 *	alias for OS_KFree
 *	@param ptr [IN] memory pointer to free
 *	@return none
 *
 * @def OS_InitRegion
 *	initilize memory region.
 *	The device driver can allocate anonymous size of memory from the
 *	memry region.
 *	This simple region manager can allocate memory very fast.
 *	This function calls OS_InitRegionEx with the default block_size = 4KB.
 *
 *	@param pRgn [IN]		pointer to memory region manager
 *	@param phys_addr [IN]	physical start address of memory region
 *	@param size [IN]		total size of memory region
 *	@return RET_OK if success, RET_ERROR otherwise.
 *
 * @def OS_InitRegionEx
 *	initilize memory region.
 *	The device driver can allocate anonymous size of memory from the
 *	memry region.
 *	This simple region manager can allocate memory very fast.
 *
 *	@param pRgn [IN]		pointer to memory region manager
 *	@param phys_addr  [IN]	physical start address of memory region
 *	@param block_size [IN]	minimul allocation size
 *	@param block_num  [IN]	toal number of block ( total memory size
 *	= block_num * block_size )
 *	@return RET_OK if success, RET_ERROR otherwise.
 *
 * @def OS_CleanupRegion
 *	cleanup memory region.
 *	This function destroy the memory region.
 *	Note that this function will not free all the allocated memory.
 *	If you are not sure, DO NOT call this function.
 *
 *	@param pRgn [IN] pointer to memory region manager
 *	@return RET_OK if success, RET_ERROR otherwise.
 *
 * @def OS_MallocRegion
 *	allocate memory from the memory region.
 *	This function may be blocked by the memory condition.
 *	So DO NOT call this function from the ISR.
 *
 *	@param pRgn [IN]		pointer to memory region manager
 *	@return pointer to the allocated physical address.
 *
 * @def OS_FreeRegion
 *	free memory to the memory region.
 *	This function may be blocked by the memory condition.
 *	So DO NOT call this function from the ISR.
 *
 *	@param pRgn [IN]		pointer to memory region manager
 *	@param ptr [IN] memory pointer to free
 *	@return RET_OK if success, RET_ERROR otherwise.
 *
 * @def OS_GetRegionInfo
 *  get the current status os region
 *
 *	@param pRgn [IN] pointer to memory region manager
 *	@param pRegionInfo [OUT] pointer to memory information
 *	@return RET_OK if success, RET_ERROR otherwise.
 */
#define OS_KMalloc(size) os_kmalloc_tag(size, __FILE__, __FUNCTION__, __LINE__)
#define OS_KFree(ptr) os_kfree_tag(ptr, __FILE__, __FUNCTION__, __LINE__)
#define OS_Malloc(size)	OS_KMalloc(size)
#define OS_Free(ptr)	OS_KFree(ptr)

/** @} */



/*--------------------------------------------------------------------
	Extern Function Prototype Declaration
--------------------------------------------------------------------*/

/** helper function to making linux device class supporting udev
 *
 *	@param udev[IN] your device node allocated by cdev_add()
 *	@param fmt [IN] your device name
 */
extern void os_createdeviceclass	( dev_t dev, const char* fmt, ... );
extern void os_destroydeviceclass	( dev_t dev );
#ifdef CONFIG_ODIN_DUAL_GFX
extern void os_createdeviceclass1	( dev_t dev, const char* fmt, ... );
extern void os_destroydeviceclass1	( dev_t dev );
#endif
/*--------------------------------------------------------------------
  DO NOT use the below function. Use OS_XXX macroes instead of these functions.
--------------------------------------------------------------------*/
extern void os_initmutex_tag	( OS_SEM_T* pSem, u32 attr );
extern int os_lockmutex_tag 	( OS_SEM_T* pSem, u32 timeout,
			const char* szFile, const char* szFunc, const int nLine );
extern void os_unlockmutex_tag 		( OS_SEM_T* pSem, const char* szFile,
			const char* szFunc, const int nLine );

#ifdef CONFIG_ODIN_DUAL_GFX
extern void os_initmutex_tag1	( OS_SEM_T* pSem, u32 attr );
extern int os_lockmutex_tag1 	( OS_SEM_T* pSem, u32 timeout,
			const char* szFile, const char* szFunc, const int nLine );
extern void os_unlockmutex_tag1 		( OS_SEM_T* pSem, const char* szFile,
			const char* szFunc, const int nLine );
#endif

extern UINT64 os_getmsecticks_tag 	( void );
extern UINT64 os_getusecticks_tag 	( void );
extern void os_getcurrentticks_tag 	( u32* pSec, u32* pMsec, u32* pUsec );

extern int os_msecsleep_tag ( u32 msec, const char* szFile,
						const char* szFunc, const int nLine );
extern void os_nsecdelay_tag ( u32 nsec, const char* szFile,
						const char* szFunc, const int nLine );
extern void os_usecdelay_tag ( u32 usec, const char* szFile,
						const char* szFunc, const int nLine );
extern void* os_kmalloc_tag ( size_t size, const char* szFile,
						const char* szFunc, const int nLine );
extern void os_kfree_tag ( void* ptr, const char* szFile,
						const char* szFunc, const int nLine );

/*--------------------------------------------------------------------
	Extern Variables
--------------------------------------------------------------------*/

#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif /* _TIME_UTIL_H_ */

/** @} */

