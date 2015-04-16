/*!
 *****************************************************************************
 *
 * @file	   img_types.h
 *
 * Typedefs based on the basic IMG types
 * ---------------------------------------------------------------------------
 *
 * Copyright (c) Imagination Technologies Ltd.
 * 
 * The contents of this file are subject to the MIT license as set out below.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN 
 * THE SOFTWARE.
 * 
 * Alternatively, the contents of this file may be used under the terms of the 
 * GNU General Public License Version 2 ("GPL")in which case the provisions of
 * GPL are applicable instead of those above. 
 * 
 * If you wish to allow use of your version of this file only under the terms 
 * of GPL, and not to allow others to use your version of this file under the 
 * terms of the MIT license, indicate your decision by deleting the provisions 
 * above and replace them with the notice and other provisions required by GPL 
 * as set out in the file called “GPLHEADER” included in this distribution. If 
 * you do not delete the provisions above, a recipient may use your version of 
 * this file under the terms of either the MIT license or GPL.
 * 
 * This License is also included in this distribution in the file called 
 * "MIT_COPYING".
 *
 *****************************************************************************/

#ifndef __IMG_TYPES__
#define __IMG_TYPES__

#include "img_systypes.h" // system specific type definitions

#ifdef __GNUC__
#define IS_NOT_USED __attribute__ ((unused))
#else
#define IS_NOT_USED
#endif
typedef          IMG_WCHAR    *IMG_PWCHAR;

typedef	          IMG_INT8	*IMG_PINT8;
typedef	         IMG_INT16	*IMG_PINT16;
typedef	         IMG_INT32	*IMG_PINT32;
typedef	         IMG_INT64	*IMG_PINT64;

//typedef	    unsigned int	IMG_UINT;
typedef	 IMG_UINT8	*IMG_PUINT8;
typedef	IMG_UINT16	*IMG_PUINT16;
typedef	IMG_UINT32	*IMG_PUINT32;
typedef	IMG_UINT64	*IMG_PUINT64;

/*
 * Typedefs of void are synonymous with the void keyword in C, 
 * but not in C++. In order to support the use of IMG_VOID
 * in place of the void keyword to specify that a function takes no 
 * arguments, it must be a macro rather than a typedef.
 */
#define IMG_VOID void
typedef            void*    IMG_PVOID;
typedef            void*    IMG_HANDLE;
typedef        IMG_INT32    IMG_RESULT;

/*
 * integral types that are not system specific
 */
typedef             char    IMG_CHAR;
typedef	             int	IMG_INT;
typedef	    unsigned int	IMG_UINT;
typedef              int    IMG_BOOL;

/*
 * boolean
 */
//#ifndef __cplusplus
#define IMG_NULL NULL
//#else
//#define IMG_NULL 0
//#endif

typedef IMG_UINT8 IMG_BOOL8;

#define	IMG_FALSE             	0	/* IMG_FALSE is known to be zero */
#define	IMG_TRUE            	1   /* 1 so it's defined and it is not 0 */

/*
 * floating point
 */
typedef float IMG_FLOAT;
typedef double IMG_DOUBLE;

typedef IMG_UINT64 IMG_SYS_PHYADDR;
#endif /* __IMG_TYPES__ */
