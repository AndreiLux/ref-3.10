/*-----------------------------------------------------------------------------
 * $RCSfile: u_handle.h,v $
 * $Revision: #2 $
 * $Date: 2010/03/30 $
 * $Author: hc.yen $
 *
 * Description:
 *         This header file contains handle specific definitions, which are
 *         exported.
 *---------------------------------------------------------------------------*/

#ifndef _U_HANDLE_H_
#define _U_HANDLE_H_


/*-----------------------------------------------------------------------------
		    include files
-----------------------------------------------------------------------------*/

#include "u_common.h"


/*-----------------------------------------------------------------------------
		    macros, defines, typedefs, enums
 ----------------------------------------------------------------------------*/

/* Specify handle data types */
#if !defined(_NO_TYPEDEF_HANDLE_T_) && !defined(_TYPEDEF_HANDLE_T_)
typedef UINT32 HANDLE_T;

#define _TYPEDEF_HANDLE_T_
#endif

typedef UINT16 HANDLE_TYPE_T;

#if !defined(NULL_HANDLE)
#define NULL_HANDLE  ((HANDLE_T) 0)
#endif

#define INV_HANDLE_TYPE  ((HANDLE_TYPE_T) 0)

/* Handle API return values */
#define HR_OK                   ((INT32)   0)
#define HR_INV_ARG              ((INT32) - 1)
#define HR_INV_HANDLE           ((INT32) - 2)
#define HR_OUT_OF_HANDLES       ((INT32) - 3)
#define HR_NOT_ENOUGH_MEM       ((INT32) - 4)
#define HR_ALREADY_INIT         ((INT32) - 5)
#define HR_NOT_INIT             ((INT32) - 6)
#define HR_RECURSION_ERROR      ((INT32) - 7)
#define HR_NOT_ALLOWED          ((INT32) - 8)
#define HR_ALREADY_LINKED       ((INT32) - 9)
#define HR_NOT_LINKED           ((INT32) - 10)
#define HR_FREE_NOT_ALLOWED     ((INT32) - 11)
#define HR_INV_AUX_HEAD         ((INT32) - 12)
#define HR_INV_HANDLE_TYPE      ((INT32) - 13)
#define HR_CANNOT_REG_WITH_CLI  ((INT32) - 14)


#endif				/* _U_HANDLE_H_ */
