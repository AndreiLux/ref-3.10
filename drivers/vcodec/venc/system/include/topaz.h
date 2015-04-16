/*!
 *****************************************************************************
 *
 * @file	   topaz.h
 *
 * This file contains the System Defintions.
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

#ifndef __TOPAZSYSTEM_H__
#define __TOPAZSYSTEM_H__


#ifdef __cplusplus
extern "C" {
#endif

//#define ISR_POLLING_ENABLED 1

#if defined(ISR_POLLING_ENABLED) || defined(SYSBRG_NO_BRIDGING)
#define POLL_FOR_INTERRUPT 1
#define SLEEP_DURING_POLL_IN_MS 10
#endif

/*!
******************************************************************************
 TOPAZ configuration values:
******************************************************************************/

#define REG_BASE_MTX                        0x04800000
#define REG_BASE_HOST                       0x00000000

/* Multicore Regs */
#define REG_OFFSET_TOPAZ_MULTICORE			0x00000000
#define REG_OFFSET_TOPAZ_DMAC				0x00000400
#define REG_OFFSET_TOPAZ_MTX				0x00000800

#define REG_SIZE_TOPAZ_MULTICORE			0x00000400
#define REG_SIZE_TOPAZ_DMAC					0x00000400
#define REG_SIZE_TOPAZ_MTX					0x00000800

/* Topaz core registers - Host view */
#define REG_OFFSET_TOPAZ_CORE_HOST			0x00001000
#define REG_SIZE_TOPAZ_CORE_HOST			0x00001000

#define REG_OFFSET_TOPAZ_TOPAZ_HOST			0x00000000
#define REG_OFFSET_TOPAZ_VLC_HOST			0x00000400
#define REG_OFFSET_TOPAZ_DEBLOCKER_HOST		0x00000800

#define REG_SIZE_TOPAZ_TOPAZ_HOST			0x00000400
#define REG_SIZE_TOPAZ_VLC_HOST				0x00000400
#define REG_SIZE_TOPAZ_DEBLOCKER_HOST		0x00000400

/* Register bank addresses - Host View */
#define REG_START_TOPAZ_MULTICORE_HOST		(REG_BASE_HOST + REG_OFFSET_TOPAZ_MULTICORE)
#define REG_END_TOPAZ_MULTICORE_HOST		(REG_START_TOPAZ_MULTICORE_HOST + REG_SIZE_TOPAZ_MULTICORE)

#define REG_START_TOPAZ_DMAC_HOST			(REG_BASE_HOST + REG_OFFSET_TOPAZ_DMAC)
#define REG_END_TOPAZ_DMAC_HOST				(REG_START_TOPAZ_DMAC_HOST + REG_SIZE_TOPAZ_DMAC)

#define REG_START_TOPAZ_MTX_HOST			(REG_BASE_HOST + REG_OFFSET_TOPAZ_MTX)
#define REG_END_TOPAZ_MTX_HOST(core)		(REG_START_TOPAZ_MTX_HOST + REG_SIZE_TOPAZ_MTX)

#define REG_START_TOPAZ_TOPAZ_HOST(core)	(REG_BASE_HOST + (REG_SIZE_TOPAZ_CORE_HOST*core) + REG_OFFSET_TOPAZ_CORE_HOST + REG_OFFSET_TOPAZ_TOPAZ_HOST)
#define REG_END_TOPAZ_TOPAZ_HOST(core)		(REG_START_TOPAZ_TOPAZ_HOST(core) + REG_SIZE_TOPAZ_TOPAZ_HOST)

/* Topaz core registers MTX view */
#define REG_OFFSET_TOPAZ_CORE_MTX			0x00010000
#define REG_SIZE_TOPAZ_CORE_MTX				0x00010000

#define REG_OFFSET_TOPAZ_MTX_MTX			0x00000000
#define REG_OFFSET_TOPAZ_TOPAZ_MTX			0x00000800
#define REG_OFFSET_TOPAZ_MVEA_MTX			0x00000C00
#define REG_OFFSET_TOPAZ_MVEACMD_MTX		0x00001000
#define REG_OFFSET_TOPAZ_VLC_MTX			0x00001400
#define REG_OFFSET_TOPAZ_DEBLOCKER_MTX		0x00001800
#define REG_OFFSET_TOPAZ_COMMS_MTX			0x00001C00
#define REG_OFFSET_TOPAZ_ESB_MTX			0x00002000

#define REG_SIZE_TOPAZ_MTX_MTX				0x00000800
#define REG_SIZE_TOPAZ_TOPAZ_MTX			0x00000400
#define REG_SIZE_TOPAZ_MVEA_MTX				0x00000400
#define REG_SIZE_TOPAZ_MVEACMD_MTX			0x00000400
#define REG_SIZE_TOPAZ_VLC_MTX				0x00000400
#define REG_SIZE_TOPAZ_DEBLOCKER_MTX		0x00000400
#define REG_SIZE_TOPAZ_COMMS_MTX			0x00000400
#define REG_SIZE_TOPAZ_ESB_MTX				0x00002000


/* Register bank addresses - MTX view */
#define REG_START_TOPAZ_MULTICORE_MTX		(REG_BASE_MTX + REG_OFFSET_TOPAZ_MULTICORE)
#define REG_END_TOPAZ_MULTICORE_MTX			(REG_START_TOPAZ_MULTICORE_MTX + REG_SIZE_TOPAZ_MULTICORE)

#define REG_START_TOPAZ_DMAC_MTX			(REG_BASE_MTX + REG_OFFSET_TOPAZ_DMAC)
#define REG_END_TOPAZ_DMAC_MTX				(REG_START_TOPAZ_DMAC_MTX + REG_SIZE_TOPAZ_DMAC)

#define REG_START_TOPAZ_MTX_MTX(core)		(REG_BASE_MTX + (REG_SIZE_TOPAZ_CORE_MTX*core) + REG_OFFSET_TOPAZ_CORE_MTX + REG_OFFSET_TOPAZ_MTX_MTX)
#define REG_END_TOPAZ_MTX_MTX(core)			(REG_START_TOPAZ_MTX_MTX(core) + REG_SIZE_TOPAZ_MTX_MTX)

#define REG_START_TOPAZ_TOPAZ_MTX(core)		(REG_BASE_MTX + (REG_SIZE_TOPAZ_CORE_MTX*core) + REG_OFFSET_TOPAZ_CORE_MTX + REG_OFFSET_TOPAZ_TOPAZ_MTX)
#define REG_END_TOPAZ_TOPAZ_MTX(core)		(REG_START_TOPAZ_TOPAZ_MTX(core) + REG_SIZE_TOPAZ_TOPAZ_MTX)

#define REG_START_TOPAZ_MVEA_MTX(core)		(REG_BASE_MTX + (REG_SIZE_TOPAZ_CORE_MTX*core) + REG_OFFSET_TOPAZ_CORE_MTX + REG_OFFSET_TOPAZ_MVEA_MTX)
#define REG_END_TOPAZ_MVEA_MTX(core)		(REG_START_TOPAZ_MVEA_MTX(core) + REG_SIZE_TOPAZ_MVEA_MTX)


/*!
******************************************************************************
 DMAC configuration values:
******************************************************************************/
/*! Defined for DMAC as first updated by the SoC Group      */
#define __DMAC_REV_002__
/*! Defined to force the DMAC API to use MSVDX memory spaces*/
#define __DMAC_MSVDX_MEMSPACE__
/*! The maximum number of channels in the SoC               */
#define DMAC_MAX_CHANNELS       (1)
#define DMA_MAX_CHANNELS        (3)
/*! The size of the DMAX HISR stack                         */
#define DMAC_HISR_STACKSIZE     (DEFAULT_HISR_STACK_SIZE)
/*! The priority of the DMAC HISR                           */
#define DMAC_HISR_PRIORITY      (KRN_HIGHEST_PRIORITY)
/*! The number of active DMAC channels supported by the API */
#define DMAC_NUM_ACTIVE_CHANNELS (1)
/*! The width of the DMA memory bus							*/
#define DMAC_MEMORY_BUSWIDTH	(128)


#define TOPAZ_DEV_NAME 			"TOPAZ"


#ifdef __cplusplus
}
#endif


#endif /* __SYSTEM_H__   */

