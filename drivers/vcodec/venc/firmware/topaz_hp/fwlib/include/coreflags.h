/*!
 *****************************************************************************
 *
 * @file	   coreflags.h
 *
 * Core specific flags to turn features on/off on different builds
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

#ifndef _COREFLAGS_H_
#define _COREFLAGS_H_


#ifdef __cplusplus
extern "C" {
#endif

#define SERIALIZED_PIPES (0)
#define SERIALIZED_PIPES_NEW (1)

/*! The number of TOPAZ cores present in the system */
#define TOPAZHP_MAX_NUM_PIPES	(4)

#define TOPAZHP_MAX_POSSIBLE_STREAMS (8)

#define USE_VCM_HW_SUPPORT (1)

#define INPUT_SCALER_SUPPORTED (1)	/* controls the firmwares ability to support the optional hardware input scaler */

#define SECURE_MODE_POSSIBLE (1)		/* controls the firmwares ability to support secure mode firmware upload */

#define SECURE_IO_PORTS (1)		/* controls the firmwares ability to support secure input/output ports */

//#define FW_LOGGING

//#define FIRMWARE_BIAS (1)

#define TOPAZHP_MAX_BU_SUPPORT_HD	90
#define TOPAZHP_MAX_BU_SUPPORT_4K	128


#if defined(TOPAZ_MTX_HW)
#	if defined(ENABLE_FORCED_INLINE)
/*
	__attribute__((always_inline)) should be only used when all C code is compiled by
	GCC to a blob object with `-combine` swithch.
*/
#		define MTX_INLINE  inline __attribute__((always_inline))
#else
#		define MTX_INLINE  inline
#endif
#else
#	define MTX_INLINE
#endif

#ifdef __cplusplus
} /* extern "C"  */
#endif
#endif  // #ifndef _COREFLAGS_H_

