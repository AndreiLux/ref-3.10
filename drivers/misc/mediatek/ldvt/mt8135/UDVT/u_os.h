/*-----------------------------------------------------------------------------
 * $RCSfile: u_os.h,v $
 * $Revision: #3 $
 * $Date: 2011/11/29 $
 * $Author: guoping.li $
 *
 * Description:
 *         This header file contains exported OS data definitions.
 *
 *---------------------------------------------------------------------------*/

#ifndef _U_OS_H_
#define _U_OS_H_


/*-----------------------------------------------------------------------------
		    include files
-----------------------------------------------------------------------------*/

#include "u_common.h"
#include "u_handle.h"
#include "u_handle_grp.h"

/*-----------------------------------------------------------------------------
		    macros, defines, typedefs, enums
 ----------------------------------------------------------------------------*/

/* OS API return values */

#define OSR_THREAD_ACTIVE         ((INT32)   3)
#define OSR_MEM_NOT_FREE          ((INT32)   2)
#define OSR_WOULD_BLOCK           ((INT32)   1)
#define OSR_OK                    ((INT32)   0)
#define OSR_EXIST                 ((INT32) - 1)
#define OSR_INV_ARG               ((INT32) - 2)
#define OSR_TIMEOUT               ((INT32) - 3)
#define OSR_NO_RESOURCE           ((INT32) - 4)
#define OSR_NOT_EXIST             ((INT32) - 5)
#define OSR_NOT_FOUND             ((INT32) - 6)
#define OSR_INVALID               ((INT32) - 7)
#define OSR_NOT_INIT              ((INT32) - 8)
#define OSR_DELETED               ((INT32) - 9)
#define OSR_TOO_MANY              ((INT32) - 10)
#define OSR_TOO_BIG               ((INT32) - 11)
#define OSR_DUP_REG               ((INT32) - 12)
#define OSR_NO_MSG                ((INT32) - 13)
#define OSR_INV_HANDLE            ((INT32) - 14)
#define OSR_FAIL                  ((INT32) - 15)
#define OSR_OUT_BOUND             ((INT32) - 16)
#define OSR_NOT_SUPPORT           ((INT32) - 17)
#define OSR_BUSY                  ((INT32) - 18)
#define OSR_OUT_OF_HANDLES        ((INT32) - 19)
#define OSR_AEE_OUT_OF_RESOURCES  ((INT32) - 20)
#define OSR_AEE_NO_RIGHTS         ((INT32) - 21)
#define OSR_CANNOT_REG_WITH_CLI   ((INT32) - 22)
#define OSR_DUP_KEY               ((INT32) - 23)
#define OSR_KEY_NOT_FOUND         ((INT32) - 24)

#define OSR_SIGNAL_BREAK          ((INT32) - 99)


/* handle types for OS modules */
#define HT_GROUP_OS_MSGQ            ((HANDLE_TYPE_T) HT_GROUP_OS)
#define HT_GROUP_OS_BIN_SEMA        ((HANDLE_TYPE_T) HT_GROUP_OS + 1)
#define HT_GROUP_OS_MUTEX_SEMA      ((HANDLE_TYPE_T) HT_GROUP_OS + 2)
#define HT_GROUP_OS_COUNTING_SEMA   ((HANDLE_TYPE_T) HT_GROUP_OS + 3)
#define HT_GROUP_OS_THREAD          ((HANDLE_TYPE_T) HT_GROUP_OS + 4)
#define HT_GROUP_OS_TIMER           ((HANDLE_TYPE_T) HT_GROUP_OS + 5)
#define HT_GROUP_OS_MEMORY          ((HANDLE_TYPE_T) HT_GROUP_OS + 6)
#define HT_GROUP_OS_EVENT_GROUP     ((HANDLE_TYPE_T) HT_GROUP_OS + 7)


#define X_SEMA_STATE_LOCK   ((UINT32) 0)
#define X_SEMA_STATE_UNLOCK ((UINT32) 1)

typedef UINT32 CRIT_STATE_T;

typedef enum {
	X_SEMA_OPTION_WAIT = 1,
	X_SEMA_OPTION_NOWAIT
} SEMA_OPTION_T;

typedef enum {
	X_SEMA_TYPE_BINARY = 1,
	X_SEMA_TYPE_MUTEX,
	X_SEMA_TYPE_COUNTING
} SEMA_TYPE_T;


typedef enum {
	X_MSGQ_OPTION_WAIT = 1,
	X_MSGQ_OPTION_NOWAIT
} MSGQ_OPTION_T;

typedef enum {
	X_TIMER_FLAG_ONCE = 1,
	X_TIMER_FLAG_REPEAT
} TIMER_FLAG_T;


typedef UINT32 EV_GRP_EVENT_T;

typedef enum {
	X_EV_OP_CLEAR = 1,
	X_EV_OP_AND,		/* 0x2 - 0x5 */
	X_EV_OP_OR,
	X_EV_OP_AND_CONSUME,
	X_EV_OP_OR_CONSUME,
	X_EV_OP_AND_INTRPT = 0x12,	/* 0x12 - 0x15 *//*For easy to read */
	X_EV_OP_OR_INTRPT,
	X_EV_OP_AND_CONSUME_INTRPT,
	X_EV_OP_OR_CONSUME_INTRPT
} EV_GRP_OPERATION_T;

typedef enum {
	X_ISR_FLAG_SAMPLE_RANDOM = 0x40
} ISR_FLAG_T;


typedef VOID(*x_os_timer_cb_fct) (HANDLE_T pt_tm_handle, VOID * pv_tag);

typedef VOID(*x_os_thread_main_fct) (VOID * pv_arg);

typedef VOID(*x_os_thread_pvt_del_fct) (UINT32 ui4_key, VOID * pv_pvt);

typedef VOID(*x_os_isr_fct) (UINT16 ui2_vector_id);

#endif				/* _U_OS_H_ */
