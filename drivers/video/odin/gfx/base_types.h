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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See th
 * GNU General Public License for more details.
 */


/*                    
  
                                       
                                             
                
                     
  
                                            
                                           
                
                     
  
                           
 */

#ifndef	_BASE_TYPES_H_
#define	_BASE_TYPES_H_

/*----------------------------------------------------------------------
	Control Constants
-----------------------------------------------------------------------*/

/*---------------------------------------------------------------------
    File Inclusions
-----------------------------------------------------------------------*/

#ifdef	__cplusplus
extern "C"
{
#endif /* __cplusplus */

/*---------------------------------------------------------------------
	Base Type Definitions
----------------------------------------------------------------------*/
#ifndef UINT8
typedef	unsigned char			__UINT8;
#define UINT8 __UINT8
#endif

#ifndef UINT08
typedef	unsigned char			__UINT08;
#define UINT08 __UINT08
#endif

#ifndef SINT8
typedef	signed char				__SINT8;
#define SINT8 __SINT8
#endif

#ifndef SINT08
typedef	signed char				__SINT08;
#define SINT08 __SINT08
#endif

#ifndef CHAR
typedef	char					__CHAR;
#define CHAR __CHAR
#endif

#ifndef UINT16
typedef	unsigned short			__UINT16;
#define UINT16 __UINT16
#endif

#ifndef SINT16
typedef	signed short			__SINT16;
#define SINT16 __SINT16
#endif

#ifndef UINT32
typedef	unsigned int			__UINT32;
#define UINT32 __UINT32
#endif

#ifndef SINT32
typedef signed int				__SINT32;
#define SINT32 __SINT32
#endif

#ifndef BOOLEAN
typedef	unsigned int			__BOOLEAN;
#define BOOLEAN __BOOLEAN
#endif

#ifndef ULONG
typedef unsigned long			__ULONG;
#define ULONG __ULONG
#endif

#ifndef SLONG
typedef signed long				__SLONG;
#define SLONG __SLONG
#endif

#ifndef UINT64
typedef	unsigned long long		__UINT64;
#define UINT64 __UINT64
#endif

#ifndef SINT64
typedef	signed long long		__SINT64;
#define SINT64 __SINT64
#endif

#ifndef TRUE
#define TRUE					(1)
#endif

#ifndef FALSE
#define FALSE					(0)
#endif

#ifndef ON_STATE
#define ON_STATE				(1)
#endif

#ifndef OFF_STATE
#define OFF_STATE				(0)
#endif

#ifndef ON
#define ON						(1)
#endif

#ifndef OFF
#define OFF						(0)
#endif

#ifndef NULL
#define NULL					((void *)0)
#endif

#ifndef _STR
#define _STR(_x)	_VAL(_x)
#define _VAL(_x)	#_x
#endif

/** simplified form of __func__ or __FUNCTION__ */
#ifndef	__F__
#define	__F__		__func__
#endif
/** simplified form of __LINE__ */
#ifndef	__L__
#define	__L__		__LINE__
#endif
/** simplified form of __FILE__ */
#ifndef	__FL__
#define	__FL__		__FILE__
#endif

/*-----------------------------------------------------------------
	Driver Major Number
------------------------------------------------------------------*/
#define	KDRV_MAJOR_BASE		140		/* 260 */

#define	GFX_MAJOR			(KDRV_MAJOR_BASE+11)
#define	GFX_MINOR			0


/*-----------------------------------------------------------------
	Error Code Definitions
-------------------------------------------------------------------*/
#define RET_OK					0			/* success */
#define RET_ERROR				-EIO		/* general error */
#define RET_INVALID_PARAMS		-EINVAL		/* invalid paramter */
#define RET_INVALID_IOCTL		-ENOTTY		/* invalid ioctl request */
#define RET_OUT_OF_MEMORY		-ENOMEM		/* out ot memory */
#define RET_TIMEOUT				-ETIME		/* timeout */
#define RET_TRY_AGAIN			-EAGAIN		/* try again */
#define RET_INTR_CALL			-EINTR		/* interrupted system call */
#define RET_NOT_SUPPORTED		-0xffff		/* L8 extention */

/*-----------------------------------------------------------------
	Macro Function Definitions
------------------------------------------------------------------*/

#ifndef OFFSET
#define OFFSET(structure, member)	\
		((int) &(((structure *)0)->member))
		/* byte offset of member in structure*/
#endif


#ifndef NELEMENTS
#define NELEMENTS(array)	/* number of elements in an array */ \
		(sizeof (array) / sizeof ((array) [0]))
#endif

/**
 *	@def LX_CALC_ALIGNED_VALUE
 *
 *	very simple macro to get N order byte aligned integer
 *	( return 2**N byte aligned value )
 *
 *	order = 0 --> return value itself ( non aligned )
 *	order = 1 --> return  2 byte aligned value
 *	order = 2 --> return  4 byte aligned value
 *	order = 3 --> return  8 byte aligned value
 *	order = 4 --> return 16 byte aligned value
 *	order = 5 --> return 16 byte aligned value
 *	...
 */
/* #define LX_CALC_ALIGNED_VALUE(val,order)
	(((val)+(1<<(order))-1) & ~((1<<(order))-1)) */

/**
 *	@def PARAM_UNUSED(param)
 *
 * gcc produces warning message when it detects unused parameters or
 * variables.  The best workaround for the warning is to remove unused
 * parameters. But you can remove warning by marking the unused
 * parameters to "unused".  If you hesitate to remove
 * the unused parameters, use PARAM_UNUSED macro instead.
 */
#ifndef	PARAM_UNUSED
#define	PARAM_UNUSED(param)  ((void)(param))
#endif

/**
 * @def LX_UNLIKELY(c)
 *
 * linux kernel supports likely, unlikely macroes but application doesn't
 *
 */
#ifndef	__KERNEL__
	#define	likely(x)	__builtin_expect((x),1)
	#define	unlikely(x)	__builtin_expect((x),0)
#endif

/**
 *	@def __CHECK_IF_ERROR
 *
 *	General purpose error check routine.
 *	This macro will help you to write error check code with one line.
 */
#ifndef	__CHECK_IF_ERROR
#define	__CHECK_IF_ERROR(__checker,__if_output,__if_action,fmt,args...)	\
							do { 								\
								if (unlikely(__checker)) { 		\
									__if_output(fmt,##args);	\
									__if_action;				\
								}								\
							} while (0)
#endif


/**
 * chip revision information.
 *
 */
typedef	struct
{
	UINT32	version;	/* 4byte version data */
	UINT8	date[4];	/* data[0] : year, date[1] : month, date[2] : day */
}
CHIP_REV_INFO_T, LX_CHIP_REV_INFO_T;


/** general dimension
 *
 */
typedef struct
{
	UINT16	w;		/* width */
	UINT16	h;		/* height */
}
LX_DIMENSION_T;

/** general rectangle
 *
 */
typedef struct
{
	UINT16	x;	/* x ( offset ) */
	UINT16	y;	/* y ( offset ) */
	UINT16	w;	/* width */
	UINT16	h;	/* height */
}
LX_RECTANGLE_T /* long form */, LX_RECT_T /* short form */ ;


/**
 @def USER_PTR
 If you have structure parameter passed to IOCTRL and that structure
 has pointer value,
 pointer value should be marked as __user when it is used in kernel driver.
 "__user" indidcator means that pointer is from user space not kernel space.

 Let's define a structure contains pointer value:

 @code
 typedef struct
 {
    int   x, y, w, h;
	char USER_PTR *img;
 }
 IMAGE_DATA_T;
 @endcode

 IMAGE_DATA_T in user space will be expressed as follows:
 @code
 typedef struct
 {
    int   x, y, w, h;
	char *img;
 }
 IMAGE_DATA_T;
 @endcode

 IMAGE_DATA_T in kernel space will be expressed as follows:
 @code
 typedef struct
 {
   int   x, y, w, h;
   char* __user *img;
 }
 IMAGE_DATA_T;
 @endcode

*/
#ifndef	USER_PTR
#undef	USER_PTR
#endif

#ifdef  __KERNEL__	/* kernel space */
#define		USER_PTR	__user
#else				/* user space */
#define		USER_PTR
#endif

/*-----------------------------------------------------------------
    Type Definitions
-------------------------------------------------------------------*/

#ifdef	__cplusplus
}
#endif /* __cplusplus */

#endif /* _KDRV_TYPES_DRV_H_ */

