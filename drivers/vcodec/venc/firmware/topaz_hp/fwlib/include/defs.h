/*!
 *****************************************************************************
 *
 * @file	   defs.h
 *
 * Useful global definitions for PMX2 simulator.
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

#if !defined DEFS_H_SENTRY
#define DEFS_H_SENTRY

#include "img_types.h"

/******************************************************************************
 *	Include files
 *****************************************************************************/
#ifdef HAVE_ZLIB
#include "zlib.h"
#endif

/******************************************************************************
 *	Pragma definitions
 *****************************************************************************/
#ifdef WIN32
/*
	prevent warnings about inlining functions
*/
#pragma warning (disable: 4711)

/*
	Prevent warnings from the code coverage pragmas
*/
#pragma warning (disable: 4068)
#endif

/******************************************************************************
 *	Defines
 *****************************************************************************/

#define SIM_PATH_MAX  255

#ifdef HAVE_ZLIB
#define GZFILE gzFile

/*
	these make the zlib commands look like normal stdio commands
*/
#define gzfread(Buf,Size,Cnt,File) gzread(File,Buf,Cnt*Size)
#define gzfgets(Buf,Len,File) gzgets(File,Buf,Len)
#define gzfputs(Data, File) gzputs(File,Data)
#define gzfputc(Char, File) gzputc(File,Char)
#define gzfprintf(Params) gzprintf Params
#else
#define GZFILE FILE
#define gzopen fopen
#define gzclose fclose
#define gzfread fread
#define gzgetc getc
#define gzseek fseek
#define gzfgets fgets
#define gzfputs fputs
#define gzfputc fputc
#define gzfprintf(Params) fprintf Params
#endif

#if !defined(INLINE)
	#ifdef WIN32
		#define INLINE __forceinline
	#else
		#define INLINE __inline__
	#endif
#endif

/*
	A float to long conversion macro, which is useful for doing a hex dump of floats.
*/
#define F_TO_L(x) (* ((IMG_UINT32 *)( & x)))

/*
	A float to long conversion macro, which is useful for doing a hex dump of floats.
*/
#define L_TO_F(x) (* ((float *)( & x)))

/*
	MACROS to insert values into fields within a word. The basename of the
	field must have MASK_BASENAME and SHIFT_BASENAME constants.
*/
#define F_MASK(basename)  (MASK_##basename)
#define F_SHIFT(basename) (SHIFT_##basename)
#define F_MASK_MVEA(basename)  (MASK_MVEA_##basename)   /*	MVEA	*/
#define F_SHIFT_MVEA(basename) (SHIFT_MVEA_##basename)   /*	MVEA	*/
/*
	Extract a value from an instruction word.
*/
#define F_EXTRACT(val,basename) (((val)&(F_MASK(basename)))>>(F_SHIFT(basename)))

/*
	Mask and shift a value to the position of a particular field.
*/
#define F_ENCODE(val,basename)  (((val)<<(F_SHIFT(basename)))&(F_MASK(basename)))
#define F_DECODE(val,basename)  (((val)&(F_MASK(basename)))>>(F_SHIFT(basename)))
#define F_ENCODE_MVEA(val,basename)  (((val)<<(F_SHIFT_MVEA(basename)))&(F_MASK_MVEA(basename)))
/*obligee de definir F_ENCODE_MVEA car le nouveau ESB utilise des noms de registres differents*/

/*
	Insert a value into a word.
*/
#define F_INSERT(word,val,basename) (((word)&~(F_MASK(basename))) | (F_ENCODE((val),basename)))

/*
	Extract a 2s complement value from an word, and make it the correct sign
	Works by testing the top bit to see if the value is negative
*/
#define F_EXTRACT_2S_COMPLEMENT( Value, Field ) ((IMG_INT32)(((((((F_MASK( Field ) >> F_SHIFT( Field )) >> 1) + 1) & (Value >> F_SHIFT( Field ))) == 0) ? F_EXTRACT( Value, Field ) : (-(IMG_INT32)(((~Value & F_MASK( Field )) >> F_SHIFT( Field )) + 1)))))

/*
	B stands for 'bitfield', defines should be in the form
	#define FIELD_NAME 10:8		(i.e. upper : lower, both inclusive)
*/
#define B_EXTRACT( Data, Bits ) (((Data) & ((IMG_UINT32)0xffffffff >> (31 - (1 ? Bits)))) >> (0 ? Bits))
#define B_MASK( Bits ) ((((IMG_UINT32)0xffffffff >> (31 - (1 ? Bits))) >> (0 ? Bits)) << (0 ? Bits))
#define B_ENCODE( Data, Bits ) (((Data) << (0 ? Bits)) & (B_MASK( Bits )))
#define B_INSERT( Word, Data, Bits ) (((Word) & ~(B_MASK( Bits ))) | (B_ENCODE( Data, Bits )))

/*
	B_BIT returns boolean true if the corresponding bit is set
	defines must be in the form FIELD_NAME 10:8 as above, except
	that the bitfield must obviously be only 1 bit wide
*/
#define B_BIT( Word, Bits ) ((((Word) >> (0 ? Bits)) & 1) == 1)



#if !defined TRUE
#define TRUE 1
#endif

#if !defined FALSE
#define FALSE 0
#endif

#if !defined true
#define true 1
#endif

#if !defined false
#define false 0
#endif

/*
	A Nan that we take to mean an uninitialised float
*/
#define UNINITIALISED_FLT 0x7f80babe

#ifdef WIN32
#define ENTRY_POINT __declspec(dllexport)
#define CALL_CONV __cdecl
#else
#define ENTRY_POINT
#define CALL_CONV
#endif

#ifdef WIN32
#define DIRECTORY_SEPARATOR			"\\"
#define DIRECTORY_SEPARATOR_CHAR	'\\'
#else
#define DIRECTORY_SEPARATOR			"/"
#define DIRECTORY_SEPARATOR_CHAR	'/'
#endif

#define UNREF_PARAM(param)param;

/******************************************************************************
 *	Local Structure Definitions
 *****************************************************************************/
/*
** Fixed length integer types.
*/
typedef IMG_INT8	    INT8;
typedef IMG_BYTE		UINT8;
typedef IMG_INT16		INT16;
typedef IMG_UINT16		UINT16;

/*
	these need to be the same as the definitions in msvc6\include\basestd.h
	in order to build the primary core simulator
*/
typedef IMG_INT32		INT32;
typedef IMG_UINT32		UINT32;

typedef	IMG_UINT32		UINT;

/* In case you need to use UberNumbers */
typedef IMG_UINT64		UINT64;
typedef IMG_INT64		INT64;

/*
	Duff definition of 128 to get C++ SAVI Libraries to work.
*/
typedef void*			INT128;

typedef union {
	float f;
	IMG_UINT32 l;
} flong;

typedef union {
  double d;
  IMG_UINT32 l[2];
} dflong;


/*
	Definition of accumulation buffer pixel format.
*/
typedef	struct	_ACCUM_PIXEL_FORMAT_
{
	IMG_UINT8	Alpha;
	IMG_UINT8	Blue;
	IMG_UINT8	Green;
	IMG_UINT8	Red;
}ACCUM_PIXEL_FORMAT, *PACCUM_PIXEL_FORMAT;

typedef	struct	_ACCUM_PIXEL_FORMAT_FLONG_
{
	flong	Alpha;
	flong	Red;
	flong	Green;
	flong	Blue;
}ACCUM_PIXEL_FORMAT_FLONG, *PACCUM_PIXEL_FORMAT_FLONG;

/*
	Structure definition for doing blending. Define colours as INT32 so that
	arithmetic is easier.
*/
typedef	struct	_ACCUM_PIXEL_FORMAT_INT32_
{
	IMG_INT32	Alpha;
	IMG_INT32	Blue;
	IMG_INT32	Green;
	IMG_INT32	Red;
}ACCUM_PIXEL_FORMAT_INT32, *PACCUM_PIXEL_FORMAT_INT32;


/*
	Definition of accumulation buffer pixel format.
*/
typedef	struct	_RGB_
{
	IMG_UINT8	Blue;
	IMG_UINT8	Green;
	IMG_UINT8	Red;
}RGB, *PRGB;

typedef struct tagRGBA_COLOUR
{
	IMG_UINT32	uA;
	IMG_UINT32	uR, uG, uB;
} RGBA_COLOUR, *PRGBA_COLOUR;

typedef struct tagYUV_COLOUR
{
	IMG_UINT32	uA;
	IMG_INT32	nY, nU, nV;
} YUV_COLOUR, *PYUV_COLOUR;

typedef struct tagGENERIC_COLOUR
{
	IMG_UINT8	uTag;			/* used for carrying sideband information e.g. which plane this pixel came from */
	IMG_UINT32	uA;
	IMG_UINT32	auData[3];
} GENERIC_COLOUR, *PGENERIC_COLOUR;

/* Colour structures with tag for main/sub window. */
typedef struct tagRGBA_TAGGED_COLOUR
{
	IMG_UINT32	uR, uG, uB, uA;
	IMG_UINT32	uTag;
} RGBA_TAGGED_COLOUR, *PRGBA_TAGGED_COLOUR;

typedef struct tagYUV_TAGGED_COLOUR
{
	IMG_UINT32	uA;
	IMG_INT32	nY, nU, nV;
	IMG_UINT32	uTag;
} YUV_TAGGED_COLOUR, *PYUV_TAGGED_COLOUR;

/* Vector description.	*/
typedef struct _VECTOR_
{
	float fVX;
	float fVY;
	float fVZ;
}VECTOR, *PVECTOR;

/* A 4-vector, xyzw */
typedef struct tagVEC4
{
	flong data[4];
} VEC4, *PVEC4;

typedef struct _FIXED_POINT_
{
	flong	flValue;

	IMG_UINT32	uiFractionalBits;
	IMG_INT64	iValue;

} FIXED_POINT, *PFIXED_POINT;

/* Boolean type */
#ifndef __cplusplus
#if !defined (IMG_KERNEL_MODULE)
typedef unsigned int bool;
#endif
#endif

/* Byte type */
typedef unsigned char byte;

/* PVOID type */
typedef void * PVOID;

/* Set to 1 to print out multithread debug messages */
#define MTHREAD_DEBUG				0

#if MTHREAD_DEBUG
#define MTHREAD_DEBUG_LINE(A) A;
#else
#define MTHREAD_DEBUG_LINE(A)
#endif



#endif /* __DEFS_H_SENTRY */
