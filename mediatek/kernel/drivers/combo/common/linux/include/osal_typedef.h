/*! \file
    \brief  Declaration of library functions

    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/


#ifndef _OSAL_TYPEDEF_H_
#define _OSAL_TYPEDEF_H_

#ifndef _TYPEDEFS_H	/*fix redifine*/
typedef void VOID;
typedef signed char INT8;
typedef signed short INT16;
typedef unsigned short UINT16;
typedef signed long LONG;
typedef signed int INT32;
typedef unsigned int UINT32;
#endif

typedef char CHAR;
typedef unsigned char UCHAR;
typedef unsigned long ULONG;
typedef void *PVOID;
typedef char *PCHAR;
typedef signed char *PINT8;
typedef unsigned char UINT8;
typedef unsigned char *PUINT8;
typedef unsigned char  *PUCHAR;
typedef signed short *PINT16;
typedef unsigned short *PUINT16;
typedef signed long *PLONG;
typedef signed int *PINT32;
typedef unsigned int *PUINT32;
typedef unsigned long  *PULONG;

typedef int MTK_WCN_BOOL;
#ifndef MTK_WCN_BOOL_TRUE
#define MTK_WCN_BOOL_FALSE               ((MTK_WCN_BOOL) 0)
#define MTK_WCN_BOOL_TRUE                ((MTK_WCN_BOOL) 1)
#endif

#endif /*_OSAL_TYPEDEF_H_*/

