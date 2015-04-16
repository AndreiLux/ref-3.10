/*!
 *****************************************************************************
 *
 * @file	   hostutils.c
 *
 * Topaz host code utility functions
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

#define TEST_DEBUGFS 0

#include "img_types.h"

#include "tal.h"
#include "tal_old.h"

#include <topaz.h>
#include <memmgr_km.h>
#include <hostutils.h>
#include <talmmu_api.h>
#include <rman_api.h>
#include <osa.h>

#include <sysos_api_km.h>
#include <dman_api_km.h>

#include "topaz_device_km.h"
#include "vxe_public_regdefs.h"

//#define DEBUG_REG_OUTPUT   //Enable this to dump registers and page table on page faults in kernel mode driver
//#define OUTPUT_COMM_STATS  // Enable this to output all firmware commands to debug log


#include "defs.h"
#ifdef FW_LOGGING
#include "fwtrace.h"
#endif

#ifdef WIN32
#define TRACK_FREE(ptr) 		IMG_FREE(ptr)
#define TRACK_MALLOC(ptr) 		IMG_MALLOC(ptr)
#define TRACK_DEVICE_MEMORY_INIT
#define TRACK_DEVICE_MEMORY_SHOW
#include <sys/timeb.h>
#define TIMER_INIT
#define TIMER_START(a,b)
#define TIMER_END(a)
#define TIMER_CAPTURE(a)
#define TIMER_CLOSE
#define __func__ __FUNCTION__
//#include "timer.h"
#else //WIN32
#define TRACK_FREE(ptr) 		IMG_FREE(ptr)
#define TRACK_MALLOC(ptr) 		IMG_MALLOC(ptr)
#define TRACK_DEVICE_MEMORY_INIT
#define TRACK_DEVICE_MEMORY_SHOW
#define TIMER_INIT
#define TIMER_START(...)
#define TIMER_END(...)
#define TIMER_CLOSE
#define TIMER_CAPTURE(...)
#if !defined (IMG_KERNEL_MODULE)
#include <sys/timeb.h>
#endif

#endif

#include <topaz_device.h>

#if defined (IMG_KERNEL_MODULE)
#include <sysdev_api_km.h>

#define PRINT printk
#define SPRINT sprintf
#define FPRINT(a, ...) printk(__VA_ARGS__)
#define FCLOSE(a)

#else
#define PRINT printf
#define SPRINT sprintf
#define FPRINT(...) fprintf(__VA_ARGS__)
#define FCLOSE fclose
#endif


#if defined (__PORT_FWRK__)
#include "dman_api.h"
#endif

#include "MTX_FwIF.h"
#if defined(FAKE_MTX)
#include "fake_mtx.h"
#endif


#if defined(IMG_KERNEL_MODULE)
#include <linux/kthread.h>
#include <linux/delay.h>

#endif

#define IMG_TIMEOUT (1)

static IMG_UINT32 ui32TopazTimeoutRetries = 0;

#if !defined(IMG_KERNEL_MODULE)
static IMG_CHAR *pszDriverVersion="Driver Version:Local Driver Build";
#endif

void DeInitTal(IMG_VOID);
static IMG_ERRORCODE comm_Send(IMG_COMM_SOCKET *psSocket, MTX_TOMTX_MSG *pMsg, IMG_UINT32 *pui32WritebackVal);
static IMG_ERRORCODE comm_Recv(IMG_COMM_SOCKET *psSocket, IMG_WRITEBACK_MSG *msg, IMG_BOOL bBlocking);
IMG_BOOL topazkm_pfnWaitOnSync(IMG_COMM_SOCKET *psSocket, IMG_UINT32 ui32WriteBackVal);

extern IMG_UINT32 	g_MMU_ControlVal;
extern IMG_BOOL		g_bUseSecureFwUpload;

#if defined(POLL_FOR_INTERRUPT)
struct ISRControl {
	IMG_BOOL exit;
	IMG_COMM_SOCKET **deviceSockets;
} isr_control = {
		IMG_TRUE,
		IMG_NULL
};

#if defined(IMG_KERNEL_MODULE)
struct task_struct *ISRThreadHandle;
#else
IMG_HANDLE ISRThreadHandle;
#endif

static IMG_VOID startISRThread();
static IMG_VOID stopISRThread();
static IMG_BOOL comm_AnyAreWaiting(IMG_COMM_SOCKET **sockets);
#endif


/*!
 ***********************************************************************************
 *
 * Description        : Time out values
 *
 ************************************************************************************/
#define TOPAZ_TIMEOUT_WAIT_FOR_SPACE      (500)
#define TOPAZ_TIMEOUT_WAIT_FOR_WRITEBACK  (4000)
#define TOPAZ_TIMEOUT_WAIT_FOR_SCRATCH    (4000)
#define TOPAZ_TIMEOUT_WAIT_FOR_FW_BOOT	  (4000)
#define TOPAZ_TIMEOUT_JPEG				  (50000)
#define TOPAZ_TIMEOUT_RETRIES             (ui32TopazTimeoutRetries)


#define 	MAX_TOPAZ_CMDS_QUEUED	256							/* max number of commands that can be queued up */
#define 	MAX_TOPAZ_CMD_COUNT		(0x1000)	/* max syncStatus value used (at least 4 * MAX_TOPAZ_CMDS_QUEUED) */


#define COMM_WB_DATA_BUF_SIZE				(64)

// Sempahore locks
#define COMM_LOCK_TX						0x01
#define COMM_LOCK_RX						0x02
#define COMM_LOCK_BOTH						(COMM_LOCK_TX | COMM_LOCK_RX)

#define RMAN_SOCKETS_ID 0x101
#define RMAN_DUMMY_ID 0x103

/*
 ***********************************************************************************
 *
 * Description        : Global Mem Space IDs
 *
 ************************************************************************************/
static IMG_HANDLE ui32TopazMulticoreRegId = (IMG_HANDLE)-1;
static IMG_HANDLE aui32HpCoreRegId[TOPAZHP_MAX_NUM_PIPES];
static IMG_HANDLE aui32VLCRegId[TOPAZHP_MAX_NUM_PIPES];
static IMG_HANDLE ui32SysMemId;
static IMG_HANDLE ui32DmacRegId;
// COMMS registers' region was not captured before
static IMG_HANDLE ui32CommsRegId;
// TESTBENCH registers' region was not captured before
static IMG_HANDLE ui32TestRegId;

// this should be set by FW in csim
IMG_UINT16	g_ui16LoadMethod=MTX_LOADMETHOD_DMA;	/* This is the load method used*/

#if defined(IMG_KERNEL_MODULE)
IMG_HANDLE g_SYSDevHandle;
#endif

IMG_UINT32	g_ui32CoreRev;
IMG_UINT32	g_ui32CoreDes1;

MEMORY_INFO **g_apsWBDataInfo;								//!< Data section

#if !defined(IMG_KERNEL_MODULE)
IMG_BOOL g_bDoingPdump = IMG_FALSE;
#endif

static IMG_UINT8 g_ui8PipeUsage[TOPAZHP_MAX_NUM_PIPES] = { 0 };

IMG_VOID comm_Deinitialize(IMG_VOID *param);

//#define OUTPUT_COMM_STATS



#ifdef SYSBRG_NO_BRIDGING
#undef COMM_PdumpComment
IMG_VOID
COMM_PdumpComment(IMG_CHAR * pszCommentText)
{
	TALPDUMP_Comment( ui32TopazMulticoreRegId, pszCommentText );
}
#endif

/**************************************************************************************************
* Function:		DeInitTal
* Description:	Deinitialise the TAL
*
***************************************************************************************************/
void DeInitTal(IMG_VOID)
{
	TIMER_START(hardwareduration,"");
	MMDeviceMemoryDeInitialise();
	TIMER_END("HW - TAL_Deinitialise in DeInitTal (topazapi.c)");
}

IMG_UINT8 COMM_GetPipeUsage(IMG_UINT8 ui8Pipe)
{
	return g_ui8PipeUsage[ui8Pipe];
}

IMG_VOID COMM_SetPipeUsage(IMG_UINT8 ui8Pipe, IMG_UINT8 ui8Val)
{
	g_ui8PipeUsage[ui8Pipe] = ui8Val;
}

IMG_UINT32
comm_GetConsumer(IMG_VOID)
{
	IMG_UINT32 ui32RegValue;

	TALREG_ReadWord32(
		ui32TopazMulticoreRegId,
		TOPAZHP_TOP_CR_FIRMWARE_REG_1 + (MTX_SCRATCHREG_TOMTX << 2),
		&ui32RegValue);

	return F_DECODE(ui32RegValue, WB_CONSUMER);
}

IMG_VOID
comm_SetConsumer(
	IMG_UINT32 ui32Consumer)
{
	IMG_UINT32 ui32RegValue;

	TALREG_ReadWord32(
		ui32TopazMulticoreRegId,
		TOPAZHP_TOP_CR_FIRMWARE_REG_1 + (MTX_SCRATCHREG_TOMTX << 2),
		&ui32RegValue);

	ui32RegValue = F_INSERT(ui32RegValue, ui32Consumer, WB_CONSUMER);

	TALREG_WriteWord32(
		ui32TopazMulticoreRegId,
		TOPAZHP_TOP_CR_FIRMWARE_REG_1 + (MTX_SCRATCHREG_TOMTX << 2),
		ui32RegValue);
}


IMG_UINT32
comm_GetProducer(IMG_VOID)
{
	IMG_UINT32 ui32RegValue;

	TALREG_ReadWord32(
		ui32TopazMulticoreRegId,
		TOPAZHP_TOP_CR_FIRMWARE_REG_1 + (MTX_SCRATCHREG_TOHOST << 2),
		&ui32RegValue);

	return F_DECODE(ui32RegValue, WB_PRODUCER);
}

IMG_VOID
comm_SetProducer(
	IMG_UINT32 ui32Producer)
{
	IMG_UINT32 ui32RegValue;

	TALREG_ReadWord32(
		ui32TopazMulticoreRegId,
		TOPAZHP_TOP_CR_FIRMWARE_REG_1 + (MTX_SCRATCHREG_TOHOST << 2),
		&ui32RegValue);

	ui32RegValue = F_INSERT(ui32RegValue, ui32Producer, WB_PRODUCER);

	TALREG_WriteWord32(
		ui32TopazMulticoreRegId,
		TOPAZHP_TOP_CR_FIRMWARE_REG_1 + (MTX_SCRATCHREG_TOHOST << 2),
		ui32RegValue);
}

/*
	`COMM_Serialize_Token` is an abstraction layer for searilisation to potentially use struct instead of primitive type. Please, do not rely on the
	type that `COMM_Serialize_Token` resolves to outside of COMM_Serialize* methods.
*/
typedef IMG_UINT32 COMM_Serialize_Token;

#if !defined(IMG_KERNEL_MODULE)
COMM_Serialize_Token comm_Serialize_Enter(IMG_VOID)
{
	IMG_UINT32 ui32RegValue;

	/* Poll for idle register to tell that both HW and FW are idle (`FW_IDLE_STATUS_IDLE` state) */
	FUNC_TAL_POLL(
		ui32TopazMulticoreRegId,
		MTX_SCRATCHREG_IDLE,
		TAL_CHECKFUNC_ISEQUAL,
		F_ENCODE(FW_IDLE_STATUS_IDLE, FW_IDLE_REG_STATUS),
		MASK_FW_IDLE_REG_STATUS,
		TOPAZ_TIMEOUT_RETRIES,
		TOPAZ_TIMEOUT_WAIT_FOR_SCRATCH
	);

	TALREG_ReadWord32(ui32TopazMulticoreRegId, MTX_SCRATCHREG_IDLE, &ui32RegValue);
	return F_EXTRACT(ui32RegValue, FW_IDLE_REG_RECEIVED_COMMANDS);
}

void comm_Serialize_Exit(COMM_Serialize_Token enterToken)
{
	const IMG_INT32 oldExecCommands = enterToken;

	FUNC_TAL_POLL(
		ui32TopazMulticoreRegId,
		MTX_SCRATCHREG_IDLE,
		TAL_CHECKFUNC_NOTEQUAL,
		F_ENCODE(oldExecCommands, FW_IDLE_REG_RECEIVED_COMMANDS),
		MASK_FW_IDLE_REG_RECEIVED_COMMANDS,
		TOPAZ_TIMEOUT_RETRIES,
		TOPAZ_TIMEOUT_WAIT_FOR_SCRATCH
	);
}
#else
#define comm_Serialize_Enter() (0)
#define comm_Serialize_Exit(x)
#endif

IMG_VOID
wbfifo_Clear(
	IMG_COMM_SOCKET * psSocket)
{
	psSocket->ui32IncomingFifo_Producer = 0;
	psSocket->ui32IncomingFifo_Consumer = 0;
}

IMG_BOOL
wbfifo_Add(
	IMG_COMM_SOCKET *psSocket,
	IMG_WRITEBACK_MSG *pNewMsg)
{
	IMG_UINT32 ui32NewProducer = psSocket->ui32IncomingFifo_Producer + 1;

	if (ui32NewProducer == COMM_INCOMING_FIFO_SIZE)
		ui32NewProducer = 0;

	if (ui32NewProducer == psSocket->ui32IncomingFifo_Consumer)
		return IMG_FALSE;

	IMG_MEMCPY(&psSocket->asIncomingFifo[psSocket->ui32IncomingFifo_Producer], pNewMsg, sizeof(IMG_WRITEBACK_MSG));

	psSocket->ui32IncomingFifo_Producer = ui32NewProducer;

	/* Now signal the user space thread about our new msg */
	SYSOSKM_SignalEventObject(psSocket->isrEvent);

	return IMG_TRUE;
}

IMG_BOOL
wbfifo_IsEmpty(
	IMG_COMM_SOCKET *psSocket)
{
	return (psSocket->ui32IncomingFifo_Producer == psSocket->ui32IncomingFifo_Consumer);
}

IMG_BOOL
wbfifo_Get(
	IMG_COMM_SOCKET *psSocket,
	IMG_WRITEBACK_MSG *pNewMsg)
{
	while(wbfifo_IsEmpty(psSocket)) {
		IMG_RESULT result;
		/* Wait for a new message to arrive */
		result = SYSOSKM_WaitEventObject(psSocket->isrEvent, IMG_FALSE);
		/* If we're interrupted */
		if (result != IMG_SUCCESS) return IMG_FALSE;
	}

	IMG_MEMCPY(pNewMsg, &psSocket->asIncomingFifo[psSocket->ui32IncomingFifo_Consumer], sizeof(IMG_WRITEBACK_MSG));

	psSocket->ui32IncomingFifo_Consumer++;

	if (psSocket->ui32IncomingFifo_Consumer == COMM_INCOMING_FIFO_SIZE)
		psSocket->ui32IncomingFifo_Consumer = 0;

	return IMG_TRUE;
}

static IMG_VOID Set_Auto_Clock_Gating(IMG_FW_CONTEXT *fwCtxt, IMG_CODEC eCodec, IMG_UINT8 ui8Gating)
{
	IMG_UINT32 ui32RegVal;

	TALPDUMP_Comment(fwCtxt->aui32TopazRegMemSpceId[0], "Enable access to all cores at once");
	ui32RegVal = F_ENCODE(1, TOPAZHP_TOP_CR_WRITES_CORE_ALL);
	TALREG_WriteWord32(
		fwCtxt->ui32TopazMulticoreRegId,
		TOPAZHP_TOP_CR_MULTICORE_CORE_SEL_0,
		ui32RegVal );

	TALPDUMP_Comment(fwCtxt->aui32TopazRegMemSpceId[0], "Setup automatic clock gating on all cores");
		ui32RegVal =	F_ENCODE(ui8Gating,TOPAZHP_CR_TOPAZHP_IPE0_AUTO_CLK_GATE) |
						F_ENCODE(ui8Gating,TOPAZHP_CR_TOPAZHP_IPE1_AUTO_CLK_GATE) |
						F_ENCODE(ui8Gating,TOPAZHP_CR_TOPAZHP_SPE0_AUTO_CLK_GATE) |
						F_ENCODE(ui8Gating,TOPAZHP_CR_TOPAZHP_SPE1_AUTO_CLK_GATE) |
						F_ENCODE(ui8Gating,TOPAZHP_CR_TOPAZHP_H264COMP4X4_AUTO_CLK_GATE) |
						F_ENCODE(ui8Gating,TOPAZHP_CR_TOPAZHP_H264COMP8X8_AUTO_CLK_GATE) |
						F_ENCODE(ui8Gating,TOPAZHP_CR_TOPAZHP_H264COMP16X16_AUTO_CLK_GATE) |
						F_ENCODE(ui8Gating,TOPAZHP_CR_TOPAZHP_JMCOMP_AUTO_CLK_GATE)|
						F_ENCODE(ui8Gating, TOPAZHP_CR_TOPAZHP_VLC_AUTO_CLK_GATE) |
						F_ENCODE(ui8Gating, TOPAZHP_CR_TOPAZHP_DEB_AUTO_CLK_GATE) |
						F_ENCODE(ui8Gating,TOPAZHP_CR_TOPAZHP_PC_DM_AUTO_CLK_GATE) |
						F_ENCODE(ui8Gating,TOPAZHP_CR_TOPAZHP_PC_DMS_AUTO_CLK_GATE) |
						F_ENCODE(ui8Gating,TOPAZHP_CR_TOPAZHP_CABAC_AUTO_CLK_GATE) |
						F_ENCODE(ui8Gating,TOPAZHP_CR_TOPAZHP_INPUT_SCALER_AUTO_CLK_GATE);

		TALREG_WriteWord32(
			fwCtxt->aui32TopazRegMemSpceId[0],
			TOPAZHP_CR_TOPAZHP_AUTO_CLOCK_GATING,
			ui32RegVal);


		if(eCodec!=IMG_CODEC_JPEG)
		{
			ui32RegVal = 0;
		TALREG_ReadWord32(fwCtxt->aui32TopazRegMemSpceId[0], TOPAZHP_CR_TOPAZHP_MAN_CLOCK_GATING, &ui32RegVal);

			// Disable LRITC clocks
			ui32RegVal = F_INSERT(ui32RegVal, 1, TOPAZHP_CR_TOPAZHP_LRITC_MAN_CLK_GATE);
			TALPDUMP_Comment(fwCtxt->aui32TopazRegMemSpceId[0], "Switch on LRITC gating on all cores (bypass LRITC clocks)");

#if  (defined (ENABLE_PREFETCH_GATING))
			// Disable PREFETCH clocks
			ui32RegVal = F_INSERT(ui32RegVal, 1, TOPAZHP_CR_TOPAZHP_PREFETCH_MAN_CLK_GATE);
			TALPDUMP_Comment( g_sFwCtxt.aui32TopazRegMemSpceId[0], "Switch on PREFETCH gating on all cores (bypass PREFETCH clock)");
#endif


		TALREG_WriteWord32(
			fwCtxt->aui32TopazRegMemSpceId[0],
			TOPAZHP_CR_TOPAZHP_MAN_CLOCK_GATING,
			ui32RegVal);
		}

	TALPDUMP_Comment(fwCtxt->aui32TopazRegMemSpceId[0], "disable access to all cores at once");
	ui32RegVal = F_ENCODE(0, TOPAZHP_TOP_CR_WRITES_CORE_ALL);
	TALREG_WriteWord32(
		fwCtxt->ui32TopazMulticoreRegId,
		TOPAZHP_TOP_CR_MULTICORE_CORE_SEL_0,
		ui32RegVal );
}

static IMG_VOID
comm_Lock(TOPAZKM_DevContext *devContext, IMG_UINT32 ui32Flags)
{
	if (ui32Flags & COMM_LOCK_TX)
	{
		SYSOSKM_LockMutex(devContext->commTxLock);
	}

	if (ui32Flags & COMM_LOCK_RX)
	{
		SYSOSKM_LockMutex(devContext->commRxLock);
	}
}


static IMG_VOID
comm_Unlock(TOPAZKM_DevContext *devContext, IMG_UINT32 ui32Flags)
{
	if (ui32Flags & COMM_LOCK_RX)
	{
		SYSOSKM_UnlockMutex(devContext->commRxLock);
	}

	if (ui32Flags & COMM_LOCK_TX)
	{
		SYSOSKM_UnlockMutex(devContext->commTxLock);
	}
}

IMG_ERRORCODE comm_PrepareFirmware(IMG_FW_CONTEXT *fwCtxt, IMG_CODEC eCodec)
{
	if(fwCtxt->bPopulated || fwCtxt->bInitialized)
	{
		return IMG_ERR_OK;
	}

	return MTX_PopulateFirmwareContext(eCodec, fwCtxt);
}

static IMG_UINT32 H264_RCCONFIG_TABLE_1[4] = { 0x00584aaf, 0x007f1410, 0x005030c6, 0x007f500c};
static IMG_UINT32 H264_RCCONFIG_TABLE_2[4] = { 0x00027dac, 0x000021f5, 0x000308d1, 0x0000057a};
static IMG_UINT32 H264_RCCONFIG_TABLE_3    = 0x0000075d;

static IMG_UINT32 MPEG_RCCONFIG_TABLES[3] = {0x0001ca00, 0x00000406, 0x0000003f};

static IMG_UINT32 H263_RCCONFIG_TABLES[3] = {0x00014d00, 0x000002cc, 0x0000003f};


/**************************************************************************************************
* Function:		COMM_LoadH264Tables
* Description:	Load the tables for H.264
*
***************************************************************************************************/
void
comm_LoadH264Tables(
	IMG_BIAS_TABLES	* psBiasTables
)
{
	IMG_INT32 n;
	IMG_UINT32 ui32Pipe, ui32PipeCount;

	ui32PipeCount = TOPAZKM_GetNumPipes();

	if (g_ui32CoreRev <= MAX_32_REV)
	{
		for(n=51;n>=0;n--)
		{
			TALREG_WriteWord32(ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_RC_CONFIG_TABLE0, psBiasTables->aui32RCConfigTable0[n]);
		}
	}
	else
	{
		//Load the RC config tables
		for(n = 0; n < 4; n++)
		{

			for (ui32Pipe = 0; ui32Pipe < ui32PipeCount; ui32Pipe++)
			{
				TALREG_WriteWord32(ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_RC_CONFIG_TABLE1_CORE(ui32Pipe), H264_RCCONFIG_TABLE_1[n]);
			}

			for (ui32Pipe = 0; ui32Pipe < ui32PipeCount; ui32Pipe++)
			{
				TALREG_WriteWord32(ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_RC_CONFIG_TABLE2_CORE(ui32Pipe), H264_RCCONFIG_TABLE_2[n]);
			}
		}

		for (ui32Pipe = 0; ui32Pipe < ui32PipeCount; ui32Pipe++)
		{
			TALREG_WriteWord32(ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_RC_CONFIG_TABLE3_CORE(ui32Pipe), H264_RCCONFIG_TABLE_3);
		}
	}

	for(n=52;n>=0;n-=2)
	{
		TALREG_WriteWord32(ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_RC_CONFIG_TABLE7, psBiasTables->aui32RCConfigTable7[n]);
		TALREG_WriteWord32(ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_RC_CONFIG_TABLE6, psBiasTables->aui32RCConfigTable6[n]);

		TALREG_WriteWord32(ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_RC_CONFIG_TABLE4, psBiasTables->aui32RCConfigTable4[n]);
		TALREG_WriteWord32(ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_RC_CONFIG_TABLE5, psBiasTables->aui32RCConfigTable5[n]);
	}

	for (ui32Pipe = 0; ui32Pipe < ui32PipeCount; ui32Pipe++) {
		TALREG_WriteWord32(aui32HpCoreRegId[ui32Pipe], TOPAZHP_CR_RC_CONFIG_REG8, psBiasTables->aui32RCConfig8);
		TALREG_WriteWord32(aui32HpCoreRegId[ui32Pipe], TOPAZHP_CR_RC_CONFIG_REG9, psBiasTables->aui32RCConfig9);
	}
}


/**************************************************************************************************
* Function:		COMM_LoadTables
* Description:	Load the tables for mpeg4
*
***************************************************************************************************/
void
comm_LoadTables(
	IMG_BIAS_TABLES * psBiasTables,
	IMG_UINT32 * pui32Coefficients)
{
	IMG_INT32 n;
	IMG_UINT32 ui32Pipe;

	if (g_ui32CoreRev <= MAX_32_REV)
	{
		for(n=31;n>=1;n--)
		{
			TALREG_WriteWord32(ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_RC_CONFIG_TABLE0, psBiasTables->aui32RCConfigTable0[n]);
		}
	}
	else
	{
		TALREG_WriteWord32(ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_RC_CONFIG_TABLE1_CORE(0), pui32Coefficients[0]);

		TALREG_WriteWord32(ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_RC_CONFIG_TABLE2_CORE(0), pui32Coefficients[1]);

		TALREG_WriteWord32(ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_RC_CONFIG_TABLE3_CORE(0), pui32Coefficients[2]);
	}

	for(n=31;n>=1;n-=2)
	{
		TALREG_WriteWord32(ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_RC_CONFIG_TABLE4, psBiasTables->aui32RCConfigTable4[n]);
		TALREG_WriteWord32(ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_RC_CONFIG_TABLE7, psBiasTables->aui32RCConfigTable7[n]);
		TALREG_WriteWord32(ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_RC_CONFIG_TABLE6, psBiasTables->aui32RCConfigTable6[n]);
	}

	for (ui32Pipe = 0; ui32Pipe < TOPAZKM_GetNumPipes(); ui32Pipe++) {
		TALREG_WriteWord32(aui32HpCoreRegId[ui32Pipe], TOPAZHP_CR_RC_CONFIG_REG8, psBiasTables->aui32RCConfig8);

		if (pui32Coefficients == MPEG_RCCONFIG_TABLES)
		{
			//VLC RSize is fcode - 1 and only done for mpeg4 AND mpeg2 not H263
			TALREG_WriteWord32(aui32VLCRegId[ui32Pipe], TOPAZ_VLC_CR_VLC_MPEG4_CFG, F_ENCODE(psBiasTables->ui32FCode - 1, TOPAZ_VLC_CR_RSIZE));
		}
	}
}

IMG_ERRORCODE
comm_LoadBias(
	IMG_BIAS_TABLES *psBiasTables,
	IMG_STANDARD eStandard)
{
	switch (eStandard)
	{
	case IMG_STANDARD_H264:
		comm_LoadH264Tables(psBiasTables);
		break;
	case IMG_STANDARD_H263:
		comm_LoadTables(psBiasTables, H263_RCCONFIG_TABLES);
		break;
	case IMG_STANDARD_MPEG4:
		comm_LoadTables(psBiasTables, MPEG_RCCONFIG_TABLES);
		break;
	case IMG_STANDARD_MPEG2:
		comm_LoadTables(psBiasTables, MPEG_RCCONFIG_TABLES);
		break;
	default:
		IMG_ASSERT(!"Unknown standard.\n");
		return IMG_ERR_UNDEFINED;
		break;
	}

	return IMG_ERR_OK;
}

IMG_ERRORCODE
comm_DispatchIncomingMsgs(IMG_COMM_SOCKET **sockets)
{
	IMG_UINT32 ui32HwFifoProducer;
	IMG_UINT32 ui32HwFifoConsumer;

	ui32HwFifoConsumer = comm_GetConsumer();
	ui32HwFifoProducer = comm_GetProducer();

#if !defined (IMG_KERNEL_MODULE)
	/* Read and compare consumer and producer */
	if (ui32HwFifoProducer == ui32HwFifoConsumer)
	{
		/* Wait for a command to process */
		FUNC_TAL_POLL(
			ui32TopazMulticoreRegId,
			TOPAZHP_TOP_CR_FIRMWARE_REG_1 + (MTX_SCRATCHREG_TOHOST << 2),
			TAL_CHECKFUNC_NOTEQUAL,
			ui32HwFifoConsumer,
			MASK_WB_PRODUCER,
			TOPAZ_TIMEOUT_RETRIES,
			TOPAZ_TIMEOUT_WAIT_FOR_WRITEBACK);
		ui32HwFifoProducer = comm_GetProducer();
	}
#endif

	while(ui32HwFifoConsumer != ui32HwFifoProducer)
	{
		IMG_WRITEBACK_MSG *psWritebackMsg;
		IMG_UINT8 ui8ConnId;

#if !defined (IMG_KERNEL_MODULE)
		// If interrupts aren't enabled we need to keep polling till we receive some results on our FIFO
		/* check we have at least one message to read (This test is only useful in PDUMP playback
		   as the other tests already cover things for normal driver operation) */
		FUNC_TAL_POLL(
			ui32TopazMulticoreRegId,
			TOPAZHP_TOP_CR_FIRMWARE_REG_1 + (MTX_SCRATCHREG_TOHOST << 2),
			TAL_CHECKFUNC_NOTEQUAL,
			ui32HwFifoConsumer,
			MASK_WB_PRODUCER,
			TOPAZ_TIMEOUT_RETRIES,
			TOPAZ_TIMEOUT_WAIT_FOR_WRITEBACK);
#endif

		/* Update corresponding memory region */
		updateHostMemory(g_apsWBDataInfo[ui32HwFifoConsumer]);
		psWritebackMsg = (IMG_WRITEBACK_MSG *)getKMAddress(g_apsWBDataInfo[ui32HwFifoConsumer]);

		/* Copy to the corresponding SW fifo */
		ui8ConnId = F_DECODE(psWritebackMsg->ui32CmdWord, MTX_MSG_CORE);

		/* Find corresponding Buffer Addr */


		/* If corresponding socket still exists, add to FIFO */
		if (sockets[ui8ConnId])
			wbfifo_Add(sockets[ui8ConnId], psWritebackMsg);

		// Activate corresponding FIFO;

		/* proceed to the next one */
		ui32HwFifoConsumer++;

		if (ui32HwFifoConsumer == WB_FIFO_SIZE)
			ui32HwFifoConsumer = 0;

		comm_SetConsumer(ui32HwFifoConsumer);

		/* We need to update the producer because we might have received a new message meanwhile.
		 * This new message won't trigger an interrupt and consequently will be lost till another message arrives
		 */
		ui32HwFifoProducer = comm_GetProducer();
	}

	return IMG_ERR_OK;
}

#if defined(POLL_FOR_INTERRUPT)
#if defined(IMG_KERNEL_MODULE)
int
#else
IMG_VOID
#endif
comm_DispatchIncomingMsgs_Wrapper(IMG_VOID *params)
{
	struct ISRControl *isr_control = (struct ISRControl *)params;
	IMG_ERRORCODE err_code;

	do {
		/* if all contexts are idle there is no need to do anything */
		while((!isr_control->exit) && !comm_AnyAreWaiting(isr_control->deviceSockets))
		{
#if !defined(IMG_KERNEL_MODULE)
			OSA_ThreadSleep(1);
#else
			msleep(SLEEP_DURING_POLL_IN_MS);
#endif
		}

		if(!isr_control->exit)
		{
			err_code = comm_DispatchIncomingMsgs(isr_control->deviceSockets);
		}

	} while ((!isr_control->exit) && (err_code == IMG_ERR_OK));

#if defined(IMG_KERNEL_MODULE)
	do_exit(0);
	return 0;
#endif
}

static IMG_VOID startISRThread(IMG_COMM_SOCKET **deviceSockets)
{
	isr_control.exit = IMG_FALSE;
	isr_control.deviceSockets = deviceSockets;

#if defined(IMG_KERNEL_MODULE)
	ISRThreadHandle = kthread_run(comm_DispatchIncomingMsgs_Wrapper,
								&isr_control,
								"ISRLooper");
#else
	OSA_ThreadCreateAndStart(comm_DispatchIncomingMsgs_Wrapper, &isr_control, "ISRLooper", OSA_THREAD_PRIORITY_ABOVE_NORMAL, &ISRThreadHandle);
#endif
}

static IMG_VOID stopISRThread()
{
	isr_control.exit = IMG_TRUE;
#if !defined(IMG_KERNEL_MODULE)
	OSA_ThreadWaitExitAndDestroy(ISRThreadHandle);
#endif
}
#endif

IMG_BOOL
comm_IsIdle(
		IMG_HCOMM_SOCKET *hSocket)
{
	IMG_COMM_SOCKET *psSocket = (IMG_COMM_SOCKET *)hSocket;

	return (psSocket->ui32MsgsSent == psSocket->ui32AckReceived);
}

#if defined(POLL_FOR_INTERRUPT)
static IMG_BOOL
comm_AnyAreWaiting(IMG_COMM_SOCKET **sockets)
{
	IMG_INT	iSocketNum = 0;
	IMG_COMM_SOCKET *pSocket;

	/* cycle through all sockets to see if any have outstanding activity */
	while(iSocketNum < TOPAZHP_MAX_POSSIBLE_STREAMS)
	{
		pSocket = sockets[iSocketNum++];
		if(pSocket && pSocket->bClientIsWaiting)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;		
}
#endif

IMG_BOOL
comm_IsHWIdle(IMG_VOID)
{
	IMG_UINT32 ui32RegValue;

	TALREG_ReadWord32(
	ui32TopazMulticoreRegId,
	MTX_SCRATCHREG_IDLE,
	&ui32RegValue);

	if (F_EXTRACT(ui32RegValue, FW_IDLE_REG_STATUS) == FW_IDLE_STATUS_IDLE)
		return IMG_TRUE;
	else
		return IMG_FALSE;
}

IMG_HANDLE topaz_GetMulticoreRegId(IMG_VOID)
{
	if((IMG_INT32)(ui32TopazMulticoreRegId) == -1)
	{
		ui32TopazMulticoreRegId = TAL_GetMemSpaceHandle("REG_TOPAZHP_MULTICORE");
		IMG_ASSERT((IMG_INT32)(ui32TopazMulticoreRegId) != -1);
	}
	return ui32TopazMulticoreRegId;
}

#ifdef FW_LOGGING
static MEMORY_INFO *trace_buffer = IMG_NULL;
struct fwtrace_tracer *tracer = IMG_NULL;
#endif

/*
 ************************************************************************************
 * Function Name      : TOPAZ_SetupFirmware
 * Inputs             :
 * Outputs            :
 * Returns            :
 * Description        : Loads MTX firmware
 ************************************************************************************/
IMG_VOID
topaz_SetupFirmware(
	TOPAZKM_DevContext *devContext,
	IMG_FW_CONTEXT *fwCtxt,
	MTX_eLoadMethod eLoadMethod,
	IMG_CODEC eCodec,
	IMG_UINT8 ui8NumPipes
)
{
#ifdef FW_LOGGING
	IMG_UINT32 devVirtAddress;
	IMG_UINT32 ui32Result;
#endif
	IMG_UINT32 ui32RegVal;

	fwCtxt->bInitialized = IMG_FALSE;

	// Reset the MTXs and Upload the code.
	/* start each MTX in turn MUST start with master to enable comms to other cores */

#ifdef SECURE_IO_PORTS
	if(!g_bUseSecureFwUpload)
	{
		// reset SECURE_CONFIG register to allow loading FW without security. Default option is secure. 
		IMG_UINT32 ui32SecureRegister = 0x000F0F0F;
		TAL_WriteWord( ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_SECURE_CONFIG, ui32SecureRegister);
	}
#endif 

	{
		IMG_ERRORCODE err;
		err = comm_PrepareFirmware(fwCtxt, eCodec);

		if(err != IMG_ERR_OK)
		{
			PRINT("Failed to populate firmware context. Error code: %i\n", err);
			return;
		}
	}
	/* initialise the MTX */
	MTX_Initialize(devContext, fwCtxt);
	
	/* clear TOHOST register now so that our ISR doesn't see any intermediate value before the FW has output anything */
	TALREG_WriteWord32( ui32TopazMulticoreRegId,	TOPAZHP_TOP_CR_FIRMWARE_REG_1 + (MTX_SCRATCHREG_TOHOST << 2), 0);

	/* clear BOOTSTATUS register.  Firmware will write to this to indicate firmware boot progress */
	TALREG_WriteWord32( ui32TopazMulticoreRegId,	TOPAZHP_TOP_CR_FIRMWARE_REG_1 + (MTX_SCRATCHREG_BOOTSTATUS << 2), 0);

	if(g_bUseSecureFwUpload)
	{
		TALPDUMP_Comment(ui32TopazMulticoreRegId, "----SecHost START-----------------------------------");
	}
	
	/* Soft reset of MTX*/
	ui32RegVal = 0;
	ui32RegVal = F_ENCODE( 1, TOPAZHP_TOP_CR_IMG_TOPAZ_MTX_SOFT_RESET )| F_ENCODE( 1,TOPAZHP_TOP_CR_IMG_TOPAZ_CORE_SOFT_RESET);
	TALREG_WriteWord32(
		ui32TopazMulticoreRegId,
		TOPAZHP_TOP_CR_MULTICORE_SRST,
		ui32RegVal);
	TALREG_WriteWord32(
		ui32TopazMulticoreRegId,
		TOPAZHP_TOP_CR_MULTICORE_SRST,
		0x0);

	if (fwCtxt->bInitialized == IMG_TRUE)
	{
		Set_Auto_Clock_Gating(fwCtxt, eCodec, 1);
		MTX_Load(devContext, fwCtxt, eLoadMethod);

		/* flush the command FIFO */
		ui32RegVal = 0;
		ui32RegVal = F_ENCODE(1, TOPAZHP_TOP_CR_CMD_FIFO_FLUSH );
		TALREG_WriteWord32(
			ui32TopazMulticoreRegId,
			TOPAZHP_TOP_CR_TOPAZ_CMD_FIFO_FLUSH,
			ui32RegVal);

		if(g_bUseSecureFwUpload)
		{
			TALREG_WriteWord32(	ui32TopazMulticoreRegId,	TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,g_MMU_ControlVal);
			TALPDUMP_Comment(ui32TopazMulticoreRegId, "----SecHost END -----------------------------------");
		}
		else
		{
			/* we do not want to run in secre FW mode so write a place holder to the FIFO that the firmware will know to ignore */
			TALREG_WriteWord32(	ui32TopazMulticoreRegId,	TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,TOPAZHP_NON_SECURE_FW_MARKER);
		}

#if defined(FAKE_MTX)
		if (gbFakeMtx)
		{
			TALREG_WriteWord32(
				ui32TopazMulticoreRegId,
				TOPAZHP_TOP_CR_MULTICORE_IDLE_PWR_MAN,
				F_ENCODE(1, TOPAZHP_TOP_CR_TOPAZ_IDLE_DISABLE));
		}
	#endif

		/* Clear FW_IDLE_STATUS register */
		TALREG_WriteWord32(
		ui32TopazMulticoreRegId,
		MTX_SCRATCHREG_IDLE,
		0);

#if defined(FAKE_MTX)
		if (gbFakeMtx)
		{
			/* Start the MTX firmware thread */
			FAKEMTX_StartMTX();
		}
#endif

#ifdef FW_LOGGING
		#define TRACE_SIZE 1024 * 1024 * 1
		allocMemory(IMG_NULL, TRACE_SIZE, 0, IMG_FALSE, &trace_buffer);
#if defined (IMG_KERNEL_MODULE)
		tracer = fwtrace_create(getKMAddress(trace_buffer), TRACE_SIZE);
#else
		tracer = fwtrace_create(getKMAddress(trace_buffer), TRACE_SIZE, trace_buffer);
#endif
		fwtrace_start(tracer);

		/* Tell the firmware about our tracer buffer location */
		ui32Result = TALMMU_GetDevVirtAddress(trace_buffer->hShadowMem, &devVirtAddress);
		IMG_ASSERT(ui32Result == IMG_SUCCESS);

		TALREG_WriteWord32(
			ui32TopazMulticoreRegId,
			MTX_SCRATCHREG_FWTRACE,
			devVirtAddress);
#endif

		/* turn on MTX */
		MTX_Start(fwCtxt);

		MTX_Kick(fwCtxt, 1);

		TALPDUMP_Comment(ui32TopazMulticoreRegId, "Poll for firmware to signal it is ready.  This POL can be removed safely.");
		/*
		 * We do not need to do this POLL here as it is safe to continue without it.  
		 * We do it because it serves to warn us that there is a problem if the firmware doesn't start for some reason
		 */
		FUNC_TAL_POLL(
		ui32TopazMulticoreRegId,
		TOPAZHP_TOP_CR_FIRMWARE_REG_1 + (MTX_SCRATCHREG_BOOTSTATUS << 2),
		TAL_CHECKFUNC_ISEQUAL,
		TOPAZHP_FW_BOOT_SIGNAL,
		0xffffffff,
		TOPAZ_TIMEOUT_RETRIES,
		TOPAZ_TIMEOUT_WAIT_FOR_FW_BOOT);
	}
}


/*
******************************************************************************
******************************************************************************
******************************************************************************
**************************    External API    ********************************
******************************************************************************
******************************************************************************
******************************************************************************/


IMG_UINT32
TOPAZKM_GetCoreRev()
{
	return g_ui32CoreRev;
}


IMG_UINT32
TOPAZKM_GetCoreDes1()
{
	return g_ui32CoreDes1;
}

IMG_VOID topazkm_pfnDevPowerPreS5(IMG_HANDLE hDevHandle, IMG_VOID *pvDevInstanceData){
	TOPAZKM_DevContext *devContext = pvDevInstanceData;
	MTX_TOMTX_MSG sMsg;
	IMG_INT iContext;
	IMG_BOOL bCmdSent=IMG_FALSE;
	IMG_UINT32 ui32WriteBackVal = 0;
	struct IMG_COMM_SOCKET_tag *psLastSocket = IMG_NULL;

	PRINT("\nSuspending MTX\n");

	sMsg.eCmdId = MTX_CMDID_GETVIDEO | MTX_CMDID_WB_INTERRUPT;
	sMsg.ui32Data = 0; /* Pipe selection for DMA */

	/* tell the firmware to save state of each open context then save the required register state */
	/* Send GetVideo commands to each active context and wait for them to finish */
	for(iContext = 0; iContext < TOPAZHP_MAX_POSSIBLE_STREAMS; iContext++)
	{
		if(devContext->deviceSockets[iContext] && (devContext->sFwCtxt.ui8ActiveContextMask & (1<<iContext)))
		{
			/* this context is active so we need to save the context */
			TALPDUMP_Comment(ui32TopazMulticoreRegId, "Tell FW to save context data");
			sMsg.pCommandDataBuf = devContext->sFwCtxt.psMTXContextDataCopy[iContext];
			psLastSocket = devContext->deviceSockets[iContext];
			comm_Send(
				psLastSocket,
				&sMsg,
				&ui32WriteBackVal);
			bCmdSent=IMG_TRUE;
		}
	}

	if(bCmdSent)
	{
		TALPDUMP_Comment(ui32TopazMulticoreRegId, "Wait for context save to complete");
		if(!topazkm_pfnWaitOnSync(psLastSocket,ui32WriteBackVal))
		{
			/* we have failed to save the context data so we should mark it as invalid*/
			PRINT("Unable to save firmware context so resume from low power will not be possible\n");
			devContext->sFwCtxt.ui8ActiveContextMask = 0;
			devContext->sFwCtxt.bInitialized = IMG_FALSE;
		}
#if !defined(IMG_KERNEL_MODULE)
		TALPDUMP_Comment(ui32TopazMulticoreRegId, "Save context 0 contents out to disk");
		updateHostMemory(devContext->sFwCtxt.psMTXContextDataCopy[0]);		
#endif
	}
	
		
	MTX_SaveState(&devContext->sFwCtxt);
	
}

IMG_VOID topazkm_pfnDevPowerPostS0(IMG_HANDLE hDevHandle, IMG_VOID *pvDevInstanceData) {
	TOPAZKM_DevContext *devContext = pvDevInstanceData;
	MTX_TOMTX_MSG sMsg;
	IMG_INT iContext;
	IMG_BOOL bCmdSent=IMG_FALSE;
	IMG_UINT32 ui32WriteBackVal = 0;
	struct IMG_COMM_SOCKET_tag *psLastSocket = IMG_NULL;

	/* Restore the MTX state */
	PRINT("\nResuming MTX\n");
	MTX_RestoreState(hDevHandle, &devContext->sFwCtxt);

	sMsg.eCmdId = MTX_CMDID_SETVIDEO | MTX_CMDID_WB_INTERRUPT;
	sMsg.ui32Data = 0; /* Pipe selection for DMA */

	/* tell the firmware to restore state of each open context*/
	/* Send SetVideo commands to each active context and wait for them to finish */
	for(iContext = 0; iContext < TOPAZHP_MAX_POSSIBLE_STREAMS; iContext++)
	{
		if(devContext->deviceSockets[iContext] && (devContext->sFwCtxt.ui8ActiveContextMask & (1<<iContext)))
		{
			TALPDUMP_Comment(ui32TopazMulticoreRegId, "Load context data into device memory");
			updateDeviceMemory(devContext->sFwCtxt.psMTXContextDataCopy[iContext]);		

			/* this context is active so we need to save the context */
			TALPDUMP_Comment(ui32TopazMulticoreRegId, "Tell FW to load context data");
			sMsg.pCommandDataBuf = devContext->sFwCtxt.psMTXContextDataCopy[iContext];
			psLastSocket = devContext->deviceSockets[iContext];
			comm_Send(
				psLastSocket,
				&sMsg,
				&ui32WriteBackVal);
			bCmdSent=IMG_TRUE;
		}
	}

	if(bCmdSent)
	{
		TALPDUMP_Comment(ui32TopazMulticoreRegId, "Wait for context restore to complete");
		if(!topazkm_pfnWaitOnSync(psLastSocket,ui32WriteBackVal))
		{
			/* we have failed to save the context data so we should mark it as invalid*/
			PRINT("Unable to restore firmware context so resume from low power will not be possible\n");
			devContext->sFwCtxt.ui8ActiveContextMask = 0;
			devContext->sFwCtxt.bInitialized = IMG_FALSE;
		}
	}
	
	TALPDUMP_Comment(ui32TopazMulticoreRegId, "---Firmware restored.");
}

/*
******************************************************************************

 @function              TOPAZ_Suspend

 @details

 Suspend the encoder and put into power saving mode.

 @param    psEncCtxt        : Pointer to Encoder Context

 @return   None

******************************************************************************/
IMG_VOID
TOPAZKM_Suspend(IMG_UINT32 ui32ConnId)
{
	IMG_RESULT result;
	IMG_HANDLE hConnHandle;
	IMG_HANDLE devHandle;

	result = DMANKM_GetConnHandleFromId(ui32ConnId, &hConnHandle);
	IMG_ASSERT(result == IMG_SUCCESS);
	if (result != IMG_SUCCESS)
	{
		PRINT("Couldn't suspend, wrong connection ID\n");
		return;
	}

	devHandle = DMANKM_GetDevHandleFromConn(hConnHandle);

	topazkm_pfnDevPowerPreS5(DMANKM_GetDevHandleFromConn(hConnHandle), DMANKM_GetDevInstanceData(devHandle));
}

/*
******************************************************************************

 @function              TOPAZ_Resume

 @details

 Restore the encoder and resume encoding.

 @param    psEncCtxt        : Pointer to Encoder Context

 @return   None

******************************************************************************/
IMG_VOID
TOPAZKM_Resume(IMG_UINT32 ui32ConnId)
{
	IMG_RESULT result;
	IMG_HANDLE hConnHandle;
	IMG_HANDLE devHandle;

	result = DMANKM_GetConnHandleFromId(ui32ConnId, &hConnHandle);
	IMG_ASSERT(result == IMG_SUCCESS);
	if (result != IMG_SUCCESS)
	{
		PRINT("Couldn't resume, wrong connection ID\n");
		return;
	}

	devHandle = DMANKM_GetDevHandleFromConn(hConnHandle);

	topazkm_pfnDevPowerPostS0(DMANKM_GetDevHandleFromConn(hConnHandle), DMANKM_GetDevInstanceData(devHandle));
}


IMG_BOOL
TOPAZKM_IsIdle(
		IMG_UINT32 ui32SockId)
{
	IMG_RESULT result;
	IMG_COMM_SOCKET *psSocket;

	result = RMAN_GetResource(ui32SockId, RMAN_SOCKETS_ID, (IMG_VOID **)&psSocket, IMG_NULL);
	IMG_ASSERT(result == IMG_SUCCESS);
	if (result != IMG_SUCCESS)
		return IMG_ERR_UNDEFINED;

	if (comm_IsIdle((IMG_HCOMM_SOCKET *)psSocket) && wbfifo_IsEmpty(psSocket))
		return IMG_TRUE;
	return IMG_FALSE;
}

/**************************************************************************************************
* Function:		COMM_LoadBias
* Description:	Load bias tables
*
***************************************************************************************************/
IMG_ERRORCODE
COMMKM_LoadBias(
	IMG_BIAS_TABLES * psBiasTables,
	IMG_STANDARD eStandard)
{
	return comm_LoadBias(psBiasTables, eStandard);
}

#if defined(IMG_KERNEL_MODULE)
#define WAIT_FOR_SYNC_RETREIS 100
#define WAIT_FOR_SYNC_TIMEOUT 1
#else
#define WAIT_FOR_SYNC_RETREIS 1000
#define WAIT_FOR_SYNC_TIMEOUT 100
#endif

IMG_BOOL topazkm_pfnWaitOnSync(IMG_COMM_SOCKET *psSocket, IMG_UINT32 ui32WriteBackVal)
{
	IMG_WRITEBACK_MSG sMtxMsg;
	IMG_UINT32 retries = 0;

	do {
		if(comm_Recv(
			psSocket,
			&sMtxMsg,
			IMG_FALSE) == IMG_ERR_UNDEFINED)
		{
			if(retries == WAIT_FOR_SYNC_RETREIS) {
				/* We shouldn't wait any longer than that!
					* If the hardware locked up, we will get stuck otherwise.
					*/
				PRINT("TIMEOUT: topazkm_pfnWaitOnSync timed out waiting for writeback 0x%08x.\n", ui32WriteBackVal);
				return IMG_FALSE;
			}

#ifndef IMG_KERNEL_MODULE
			OSA_ThreadSleep(WAIT_FOR_SYNC_TIMEOUT);
#else
			msleep(WAIT_FOR_SYNC_TIMEOUT);
#endif
			retries++;
			continue;
		}
	} while (sMtxMsg.ui32WritebackVal != ui32WriteBackVal /*|| ((sMtxMsg.ui32CmdWord & 0XF) != 0)*/);
	return IMG_TRUE;
}

IMG_VOID
_comm_CloseSocket(
	IMG_VOID *param)
{
	IMG_UINT32 nIndex;
	IMG_COMM_SOCKET *psSocket = (IMG_COMM_SOCKET *)param;

	for (nIndex = 0; nIndex < TOPAZHP_MAX_POSSIBLE_STREAMS; nIndex++)
	{
		if(psSocket->devContext->deviceSockets[nIndex] == psSocket)
		{
			psSocket->devContext->usedSockets--;
			break;
		}
	}


	/* if nIndex == TOPAZHP_MAX_POSSIBLE_STREAMS then OpenSocket succeeded and SetupSocket failed (maybe incompatible firmware) */
	if (nIndex != TOPAZHP_MAX_POSSIBLE_STREAMS)
	{
#if defined(IMG_KERNEL_MODULE)
	//if (psSocket->devContext->usedSockets != 0)
		/*
		 * Abort the stream first.
		 * This function can be called as a result of abnormal process exit, and since the hardware might be encoding some frame it means that
		 * the hardware still needs the context resources (buffers mapped to the hardware, etc), so we need to make sure that hardware encoding is aborted first
		 * before releasing the resources.
		 * This is important if you're doing several encodes simultaneously because releasing the resources too early will cause a page-fault
		 * that will halt all simultaneous encodes not just the one that caused the page-fault.
		 */
		MTX_TOMTX_MSG sMsg;
		IMG_UINT32 ui32WriteBackVal = 0;

		/* Clear all pending messages */
		wbfifo_Clear(psSocket);

		sMsg.eCmdId = MTX_CMDID_ABORT | MTX_CMDID_PRIORITY | MTX_CMDID_WB_INTERRUPT;
		sMsg.ui32Data = 0;
		sMsg.pCommandDataBuf = IMG_NULL;
		comm_Send(
			psSocket,
			&sMsg,
			&ui32WriteBackVal);

		topazkm_pfnWaitOnSync(psSocket, ui32WriteBackVal);
#endif
		/* Set it to NULL here -not any time sooner-, we need it in case we had to abort the stream. */
		psSocket->devContext->deviceSockets[nIndex] = IMG_NULL;
	}

	SYSOSKM_DestroyEventObject(psSocket->isrEvent);

	/* Free all the resources attached to this socket */
	RMAN_DestroyBucket(psSocket->hResBHandle);

	IMG_FREE(psSocket);
}

IMG_ERRORCODE
COMMKM_CloseSocket(
	IMG_UINT32			ui32SockId
)
{
	IMG_RESULT result;
	IMG_HANDLE resHandle;

	result = RMAN_GetResource(ui32SockId, RMAN_SOCKETS_ID, IMG_NULL, &resHandle);
	IMG_ASSERT(result == IMG_SUCCESS);
	if (result != IMG_SUCCESS)
		return IMG_ERR_UNDEFINED;

	RMAN_FreeResource(resHandle);

	return IMG_ERR_OK;
}

IMG_ERRORCODE
COMMKM_OpenSocket(
	IMG_UINT32			ui32ConnId,
	IMG_UINT32			*ui32SockId,
	IMG_CODEC			eCodec	
	)
{
	IMG_UINT32 resourceId;
	IMG_RESULT result;
	IMG_HANDLE devHandle;
	IMG_HANDLE hConnHandle;
	IMG_COMM_SOCKET *psSocket;
	TOPAZKM_ConnData *connData;
	TOPAZKM_DevContext *devContext;


	result = DMANKM_GetConnHandleFromId(ui32ConnId, &hConnHandle);
	IMG_ASSERT(result == IMG_SUCCESS);
	if (result != IMG_SUCCESS)
	{
		return IMG_ERR_UNDEFINED;
	}

	devHandle = DMANKM_GetDevHandleFromConn(hConnHandle);
	devContext = DMANKM_GetDevInstanceData(devHandle);
	connData = DMANKM_GetDevConnectionData(hConnHandle);

	psSocket = IMG_MALLOC(sizeof(IMG_COMM_SOCKET));
	IMG_ASSERT(psSocket != IMG_NULL);
	if(psSocket == IMG_NULL)
	{
		return IMG_ERR_UNDEFINED;
	}
	/* Create bucket to handle socket resources */
	RMAN_CreateBucket(&psSocket->hResBHandle);

	result = RMAN_RegisterResource(connData->hResBHandle, RMAN_SOCKETS_ID, _comm_CloseSocket, (IMG_VOID *)psSocket, IMG_NULL, &resourceId);
	IMG_ASSERT(result == IMG_SUCCESS);

	SYSOSKM_CopyToUser((IMG_VOID *)ui32SockId, (IMG_VOID *)&resourceId, sizeof(*ui32SockId));
	psSocket->ui32LowCmdCount = 0xa5a5a5a5 %  MAX_TOPAZ_CMD_COUNT;
	psSocket->ui32HighCmdCount = 0;
	psSocket->ui32MsgsSent = 0;
	psSocket->ui32AckReceived = 0;
	psSocket->ui32ResourceId = *ui32SockId;
	psSocket->bIsSerialized = IMG_FALSE;
	psSocket->eCodec = eCodec;
	psSocket->devContext = devContext;
	psSocket->connData = connData;

#if defined(POLL_FOR_INTERRUPT)
	psSocket->bClientIsWaiting = IMG_FALSE;
#endif

	SYSOSKM_CreateEventObject(&psSocket->isrEvent);
	wbfifo_Clear(psSocket);
	return IMG_ERR_OK;
}

#define IS_STD_H264(format) (((format) == IMG_CODEC_H264_NO_RC) || ((format) == IMG_CODEC_H264_VBR) || ((format) == IMG_CODEC_H264_CBR) || ((format) == IMG_CODEC_H264_VCM) || ((format) == IMG_CODEC_H264_ERC))

IMG_ERRORCODE
COMMKM_SetupSocket(
	IMG_UINT32			ui32SockId,
	IMG_BIAS_TABLES		*psBiasTables,
	IMG_UINT16			ui16FrameHeight,
	IMG_UINT16			ui16Width,
	IMG_BOOL			bDoSerializedComm,
	IMG_UINT8			*ui8CtxtNum,
	IMG_UINT32			*usedSocket
	)
{
	IMG_INT nIndex;
	IMG_RESULT result;
	IMG_FW_CONTEXT *fwCtxt;
	IMG_COMM_SOCKET *psSocket;
	IMG_BIAS_TABLES *tables;
	IMG_ERRORCODE eRes = IMG_ERR_UNDEFINED;

	result = RMAN_GetResource(ui32SockId, RMAN_SOCKETS_ID, (IMG_VOID **)&psSocket, IMG_NULL);
	IMG_ASSERT(result == IMG_SUCCESS);
	if (result != IMG_SUCCESS)
		return IMG_ERR_UNDEFINED;

	comm_Lock(psSocket->devContext, COMM_LOCK_BOTH);

	/* store serialised mode */
	psSocket->bIsSerialized = bDoSerializedComm;

	fwCtxt = &psSocket->devContext->sFwCtxt;

	if (
			(psSocket->devContext->firmwareUploaded != IMG_CODEC_NONE) && // Only do the following checks if some other firmware is loaded.
			((psSocket->devContext->firmwareUploaded != psSocket->eCodec) // Different firmware is uploaded
			|| (psSocket->devContext->firmwareUploaded == IMG_CODEC_JPEG && psSocket->devContext->usedSockets)) // We currently only support one JPEG context to be encoded at the same time
		)
	{
		if((psSocket->devContext->firmwareUploaded != IMG_CODEC_H264_ALL_RC) ||
			((psSocket->devContext->firmwareUploaded == IMG_CODEC_H264_ALL_RC) && !IS_STD_H264(psSocket->eCodec)))
		{
			comm_Unlock(psSocket->devContext, COMM_LOCK_BOTH);
			eRes = IMG_ERR_UNDEFINED;

			if(psSocket->devContext->firmwareUploaded == IMG_CODEC_JPEG)
				PRINT("\nERROR: Can't do multiple JPEG encodes at the same time.\n");
			else
				PRINT("\nERROR: Incompatible firmware context types!\n");
			return eRes;
		}
	}

	if(fwCtxt->bInitialized && (psSocket->devContext->usedSockets >= fwCtxt->ui32NumContexts))
	{
		/* the firmware can't support any more contexts */
		comm_Unlock(psSocket->devContext, COMM_LOCK_BOTH);
		PRINT("\nERROR: Firmware context limit reached!\n");
		return 	IMG_ERR_UNDEFINED;
	}

	/* Search for an Available socket. */
	for (nIndex = 0; nIndex < TOPAZHP_MAX_POSSIBLE_STREAMS; nIndex++)
	{
		if (psSocket->devContext->deviceSockets[nIndex] == IMG_NULL)
		{
			psSocket->ui8SocketId = nIndex;
			*ui8CtxtNum = psSocket->ui8SocketId = nIndex;
			psSocket->ui8SocketId = nIndex;
			*usedSocket = psSocket->ui8SocketId;
			psSocket->devContext->deviceSockets[nIndex] = psSocket;
			psSocket->devContext->usedSockets++;
			break;
		}
	}

	if(nIndex == TOPAZHP_MAX_POSSIBLE_STREAMS) {
		comm_Unlock(psSocket->devContext, COMM_LOCK_BOTH);
		return IMG_ERR_INVALID_SIZE;
	}

	if (psSocket->devContext->firmwareUploaded == IMG_CODEC_NONE)
	{
		PRINT("Loading a different firmware.\n");
		// Upload FW
#if defined(FAKE_MTX)
		if(gbFakeMtx)
		{
			if(g_bUseSecureFwUpload)
			{
				TALREG_WriteWord32(	ui32TopazMulticoreRegId,	TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,g_MMU_ControlVal);
			}

			/* Setup MTX Firmware BUT Do not load MTX firmware */
			fwCtxt->eLoadMethod = MTX_LOADMETHOD_NONE;
		}
		else
#endif
		{
			/* load and start MTX cores */
			fwCtxt->eLoadMethod = (MTX_eLoadMethod)g_ui16LoadMethod;
		}

		if (psSocket->eCodec == IMG_CODEC_JPEG)
			ui32TopazTimeoutRetries = TOPAZ_TIMEOUT_JPEG;
		else
		{
			IMG_UINT32 ui32MbsPerPic = (ui16FrameHeight * ui16Width) / 256;
			if (ui32TopazTimeoutRetries < (ui32MbsPerPic + 10) * 100)
				ui32TopazTimeoutRetries = (ui32MbsPerPic + 10) * 100;
		}

		topaz_SetupFirmware(psSocket->devContext, fwCtxt, fwCtxt->eLoadMethod, psSocket->eCodec, TOPAZKM_GetNumPipes());

		if(fwCtxt->bInitialized == IMG_FALSE)
		{
			comm_Unlock(psSocket->devContext, COMM_LOCK_BOTH);
			eRes = IMG_ERR_UNDEFINED;
			PRINT("\nERROR: Firmware cannot be loaded!\n");
			return eRes;
		}

		if(psSocket->eCodec != IMG_CODEC_JPEG) 
		{
			IMG_UINT8 ui8Pipe;
			IMG_STANDARD eStandard = IMG_STANDARD_NONE;
			tables = IMG_MALLOC(sizeof(*psBiasTables));
			IMG_ASSERT(tables != IMG_NULL);
			if(tables == IMG_NULL)
			{
				comm_Unlock(psSocket->devContext, COMM_LOCK_BOTH);
				return IMG_ERR_MEMORY;
			}
			SYSOSKM_CopyFromUser(tables, psBiasTables, sizeof(*psBiasTables));

			for (ui8Pipe = 0; ui8Pipe < TOPAZKM_GetNumPipes(); ui8Pipe++)
				TALREG_WriteWord32(
				aui32HpCoreRegId[ui8Pipe],
				TOPAZHP_CR_SEQUENCER_CONFIG,
				tables->ui32SeqConfigInit);

			switch(psSocket->eCodec)
			{
			case IMG_CODEC_H264_NO_RC:
			case IMG_CODEC_H264_VBR:
			case IMG_CODEC_H264_CBR:
			case IMG_CODEC_H264_VCM:
			case IMG_CODEC_H264_ERC:
				eStandard = IMG_STANDARD_H264;
				break;
			case IMG_CODEC_H263_NO_RC:
			case IMG_CODEC_H263_VBR:
			case IMG_CODEC_H263_CBR:
			case IMG_CODEC_H263_ERC:
				eStandard = IMG_STANDARD_H263;
				break;
			case IMG_CODEC_MPEG4_NO_RC:
			case IMG_CODEC_MPEG4_VBR:
			case IMG_CODEC_MPEG4_CBR:
			case IMG_CODEC_MPEG4_ERC:
				eStandard = IMG_STANDARD_MPEG4;
				break;
			case IMG_CODEC_MPEG2_NO_RC:
			case IMG_CODEC_MPEG2_VBR:
			case IMG_CODEC_MPEG2_CBR:
			case IMG_CODEC_MPEG2_ERC:
				eStandard = IMG_STANDARD_MPEG2;
				break;

			case IMG_CODEC_H264MVC_NO_RC:
			case IMG_CODEC_H264MVC_VBR:
			case IMG_CODEC_H264MVC_CBR:
			case IMG_CODEC_H264MVC_ERC:
				eStandard = IMG_STANDARD_H264;
				break;
			default:
				IMG_ASSERT(!"Impossible use case!\n");
				break;
			}
			comm_LoadBias(tables, eStandard);

			IMG_FREE(tables);

			/* initialise read offset of firmware output fifo */
			TALREG_WriteWord32(
				ui32TopazMulticoreRegId,
				TOPAZHP_TOP_CR_FIRMWARE_REG_1+(MTX_SCRATCHREG_TOMTX<<2),
				0);
		}

		psSocket->devContext->firmwareUploaded = psSocket->eCodec;

		if(IS_STD_H264(psSocket->eCodec))
		{
			psSocket->devContext->firmwareUploaded = IMG_CODEC_H264_ALL_RC;
		}

	}

	if (psSocket->eCodec == IMG_CODEC_JPEG)
		ui32TopazTimeoutRetries = TOPAZ_TIMEOUT_JPEG;
	else
	{
		IMG_UINT32 ui32MbsPerPic = (ui16FrameHeight * ui16Width) / 256;
		if (ui32TopazTimeoutRetries < (ui32MbsPerPic + 10) * 100)
			ui32TopazTimeoutRetries = (ui32MbsPerPic + 10) * 100;
	}

	eRes = IMG_ERR_OK;

#if defined(POLL_FOR_INTERRUPT)
	/* Only start the ISRThread when it's not running. */
	if (isr_control.exit)
		startISRThread(psSocket->devContext->deviceSockets);
#endif

	comm_Unlock(psSocket->devContext, COMM_LOCK_BOTH);

	return eRes;
}

static IMG_ERRORCODE
comm_Send(
	IMG_COMM_SOCKET *psSocket,
	MTX_TOMTX_MSG *pMsg,
	IMG_UINT32 *pui32WritebackVal)
{
	IMG_FW_CONTEXT *fwCtxt;
	IMG_UINT32 ui32SpaceAvailable;
	COMM_Serialize_Token serializeToken;

	fwCtxt = &psSocket->devContext->sFwCtxt;

	/* mark the context as active in case we need to save its state later */
	fwCtxt->ui8ActiveContextMask |= (1<<psSocket->ui8SocketId);

	//Mark COMM as Busy
	comm_Lock(psSocket->devContext, COMM_LOCK_TX);

	if(psSocket->bIsSerialized)
	{
		serializeToken = comm_Serialize_Enter();
	}

	TALREG_ReadWord32(
		ui32TopazMulticoreRegId,
		TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE_SPACE,
		&ui32SpaceAvailable);

	ui32SpaceAvailable = F_DECODE(ui32SpaceAvailable, TOPAZHP_TOP_CR_CMD_FIFO_SPACE);

	if (ui32SpaceAvailable < 4)
	{
		comm_Unlock(psSocket->devContext, COMM_LOCK_TX);
		return IMG_ERR_RETRY;
	}


	/* Write command to FIFO */
	{
		IMG_UINT32 ui32CmdWord;
		ui32CmdWord = F_ENCODE(psSocket->ui8SocketId, MTX_MSG_CORE) | pMsg->eCmdId;

		if (pMsg->eCmdId & MTX_CMDID_PRIORITY)
		{
			/* increment the command counter */
			psSocket->ui32HighCmdCount++;

			/* Prepare high priority command */
			ui32CmdWord |=
				F_ENCODE(1, MTX_MSG_PRIORITY) |
				F_ENCODE(((psSocket->ui32LowCmdCount - 1) & 0xff) | (psSocket->ui32HighCmdCount << 8), MTX_MSG_COUNT);
		}
		else
		{
			/* Prepare low priority command */
			ui32CmdWord |=
				F_ENCODE(psSocket->ui32LowCmdCount & 0xff, MTX_MSG_COUNT);
		}

		/* write command into FIFO */
		TALREG_WriteWord32(
			ui32TopazMulticoreRegId,
			TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
			ui32CmdWord);
	}

	/* Write data to FIFO */
	TALREG_WriteWord32(
		ui32TopazMulticoreRegId,
		TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
		pMsg->ui32Data);

	if ( pMsg->pCommandDataBuf )
	{
		/* Write address */
		writeMemoryRef(
			ui32TopazMulticoreRegId,
			TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
			pMsg->pCommandDataBuf,
			0);
	}
	else
	{
		/* Write nothing */
		TALREG_WriteWord32(
			ui32TopazMulticoreRegId,
			TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
			0);
	}

	/* Write writeback value to FIFO */
	{
		/* prepare Writeback value */
		IMG_UINT32 ui32WriteBackVal;

		// We don't actually use this value, but it may be useful to customers
		if (pMsg->eCmdId & MTX_CMDID_PRIORITY)
		{
			/* HIGH priority command */

			ui32WriteBackVal = psSocket->ui32HighCmdCount << 24;

			TALREG_WriteWord32(
				ui32TopazMulticoreRegId,
				TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
				ui32WriteBackVal);
		}
		else
		{
			/* LOW priority command */
			ui32WriteBackVal = psSocket->ui32LowCmdCount << 16;

			TALREG_WriteWord32(
				ui32TopazMulticoreRegId,
				TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
				ui32WriteBackVal);

			/* increment the command counter */
			psSocket->ui32LowCmdCount++;

		}

		if (pui32WritebackVal)
		{
			*pui32WritebackVal = ui32WriteBackVal;
		}

		psSocket->ui32LastSync = ui32WriteBackVal;
	}

	/* kick the master MTX */
	MTX_Kick(fwCtxt, 1);

	psSocket->ui32MsgsSent++;

	// Release COMM
	if(psSocket->bIsSerialized)
	{
		comm_Serialize_Exit(serializeToken);
	}
	comm_Unlock(psSocket->devContext, COMM_LOCK_TX);


	return IMG_ERR_OK;
}

IMG_ERRORCODE
COMMKM_Send(
	IMG_UINT32 ui32SockId,
	MTX_TOMTX_MSG *pMsg,
	IMG_UINT32 *pui32WritebackVal)
{
	MTX_TOMTX_MSG *msg;
	IMG_RESULT result;
	IMG_ERRORCODE err;
	IMG_COMM_SOCKET *psSocket;

	result = RMAN_GetResource(ui32SockId, RMAN_SOCKETS_ID, (IMG_VOID **)&psSocket, IMG_NULL);
	IMG_ASSERT(result == IMG_SUCCESS);
	if (result != IMG_SUCCESS)
		return IMG_ERR_UNDEFINED;

	msg = IMG_MALLOC(sizeof(*pMsg));
	IMG_ASSERT(msg != IMG_NULL);
	if(msg == IMG_NULL)
	{
		return IMG_ERR_UNDEFINED;			
	}
	SYSOSKM_CopyFromUser(msg, pMsg, sizeof(*pMsg));
	err = comm_Send(psSocket, msg, pui32WritebackVal);
	IMG_FREE(msg);
	return err;
}

static
IMG_ERRORCODE comm_Recv(IMG_COMM_SOCKET *psSocket, IMG_WRITEBACK_MSG *msg, IMG_BOOL bBlocking)
{
	IMG_BOOL bRes = IMG_FALSE;


#if defined(POLL_FOR_INTERRUPT)
	if (!bBlocking && comm_IsIdle((IMG_HCOMM_SOCKET *)psSocket))
		return IMG_ERR_UNDEFINED;
#else
	if (!bBlocking && wbfifo_IsEmpty(psSocket))
		return IMG_ERR_UNDEFINED;
#endif

#if defined(POLL_FOR_INTERRUPT)
	psSocket->bClientIsWaiting = IMG_TRUE;
#endif

	bRes = wbfifo_Get(psSocket, msg);

#if defined(POLL_FOR_INTERRUPT)
	psSocket->bClientIsWaiting = IMG_FALSE;
#endif

	if (bRes && (F_DECODE(msg->ui32CmdWord, MTX_MSG_MESSAGE_ID) == MTX_MESSAGE_ACK))
		psSocket->ui32AckReceived++;

	return (bRes ? IMG_ERR_OK : IMG_ERR_UNDEFINED);
}

IMG_ERRORCODE
COMMKM_Recv(
	IMG_UINT32	ui32SockId,
	IMG_WRITEBACK_MSG *pMsg,
	IMG_BOOL bBlocking)
{
	IMG_RESULT result;
	IMG_COMM_SOCKET *psSocket;

	IMG_ERRORCODE bRes = IMG_ERR_OK;
	IMG_WRITEBACK_MSG msg;

	result = RMAN_GetResource(ui32SockId, RMAN_SOCKETS_ID, (IMG_VOID **)&psSocket, IMG_NULL);
	IMG_ASSERT(result == IMG_SUCCESS);
	if (result != IMG_SUCCESS)
		return IMG_ERR_UNDEFINED;

	bRes = comm_Recv(psSocket, &msg, bBlocking);
	if(bRes == IMG_ERR_OK)
		SYSOSKM_CopyToUser(pMsg, &msg, sizeof(IMG_WRITEBACK_MSG));

	return bRes;
}

IMG_BOOL
COMMKM_IsIdle(
	IMG_UINT32 ui32SockId)
{
	IMG_RESULT result;
	IMG_HCOMM_SOCKET *hSocket;

	result = RMAN_GetResource(ui32SockId, RMAN_SOCKETS_ID, (IMG_VOID **)&hSocket, IMG_NULL);
	IMG_ASSERT(result == IMG_SUCCESS);
	if (result != IMG_SUCCESS)
		return IMG_FALSE;

	return comm_IsIdle(hSocket);
}

IMG_UINT32 COMMKM_GetFwConfigInt(IMG_UINT32 ui32ConnId, IMG_CHAR *name)
{
	IMG_HANDLE hConnHandle;
	IMG_RESULT result;
	IMG_FW_CONTEXT *fwCtxt;
	IMG_HANDLE devHandle;
	TOPAZKM_DevContext *devContext;

	result = DMANKM_GetConnHandleFromId(ui32ConnId, &hConnHandle);
	IMG_ASSERT(result == IMG_SUCCESS);
	if (result != IMG_SUCCESS)
		return 0xFFFFFFFF;

	devHandle = DMANKM_GetDevHandleFromConn(hConnHandle);
	devContext = DMANKM_GetDevInstanceData(devHandle);
	fwCtxt = &devContext->sFwCtxt;

	return MTX_GetFwConfigInt(fwCtxt, name);
}



IMG_ERRORCODE
COMMKM_Initialize(
	IMG_UINT32 ui32ConnId,
	MEMORY_INFO **apsWBDataInfo,
	IMG_UINT32 *pui32NumPipes,
	IMG_UINT32 ui32MmuFlags,
	IMG_UINT32 ui32MMUTileStride
)
{
	IMG_INT nIndex;
	IMG_RESULT result;
	IMG_UINT32 i;
#if !defined(IMG_KERNEL_MODULE)
	IMG_BOOL bPdumpState;
#endif
	IMG_UINT32 ui32RegVal;
	IMG_UINT32 num_cores;

	IMG_HANDLE devHandle;
	IMG_HANDLE hConnHandle;
	TOPAZKM_ConnData *connData;
	TOPAZKM_DevContext *devContext;

	result = DMANKM_GetConnHandleFromId(ui32ConnId, &hConnHandle);
	IMG_ASSERT(result == IMG_SUCCESS);
	if (result != IMG_SUCCESS)
		return IMG_ERR_UNDEFINED;

	devHandle = DMANKM_GetDevHandleFromConn(hConnHandle);
	IMG_ASSERT(devHandle != IMG_NULL);
	if (devHandle == IMG_NULL)
		return IMG_ERR_UNDEFINED;

	devContext = DMANKM_GetDevInstanceData(devHandle);
	IMG_ASSERT(devContext != IMG_NULL);
	if (devContext == IMG_NULL)
		return IMG_ERR_UNDEFINED;

	DMANKM_LockDeviceContext(devHandle);

	if(devContext->bInitialised) {
#if defined (IMG_KERNEL_MODULE)
		MEMORY_INFO **wbDataInfo;
		wbDataInfo = IMG_MALLOC(sizeof(MEMORY_INFO *) * WB_FIFO_SIZE);
		IMG_ASSERT(wbDataInfo != IMG_NULL);
		if(wbDataInfo == IMG_NULL)
		{
			DMANKM_UnlockDeviceContext(devHandle);
			return IMG_ERR_MEMORY;			
		}
		SYSOSKM_CopyFromUser(wbDataInfo, apsWBDataInfo, sizeof(MEMORY_INFO *) * WB_FIFO_SIZE);
		// Allocate WB buffers.
		for (nIndex = 0; nIndex < WB_FIFO_SIZE; nIndex++)
			SYSOSKM_CopyToUser(wbDataInfo[nIndex], g_apsWBDataInfo[nIndex], sizeof(MEMORY_INFO));
		IMG_FREE(wbDataInfo);
#else
		g_apsWBDataInfo = apsWBDataInfo;
#endif
		num_cores = TOPAZKM_GetNumPipes();
		SYSOSKM_CopyToUser((IMG_VOID *)pui32NumPipes, (IMG_VOID *)&num_cores, sizeof(*pui32NumPipes));
		DMANKM_UnlockDeviceContext(devHandle);
		return IMG_ERR_OK;
	}


	topazdd_Initialise(devContext);

	connData = DMANKM_GetDevConnectionData(hConnHandle);
	result = RMAN_RegisterResource(devContext->hResBHandle, RMAN_DUMMY_ID, comm_Deinitialize, (IMG_VOID *)devHandle, IMG_NULL, IMG_NULL);
	IMG_ASSERT(result == IMG_SUCCESS);

	// initialise the TAL
	TIMER_START(hardwareduration,"");

#if !defined (IMG_KERNEL_MODULE)
	{
		IMG_BOOL bPdump1,bPdump2;
		bPdump1=(getenv("DOPDUMP1")? IMG_TRUE:IMG_FALSE);
		bPdump2=(getenv("DOPDUMP2")? IMG_TRUE:IMG_FALSE);

		g_bDoingPdump = bPdump1 || bPdump2;

#ifdef FW_LOGGING
		if(g_bDoingPdump)
		{
			PRINT("\nERROR: pDump should be disabled if FW_LOGGING is enabled.!\n");
			IMG_ASSERT(0);
			return IMG_ERR_UNDEFINED;
		}
#endif


		g_ui16LoadMethod = getenv("BACKDOOR_LOAD") ? MTX_LOADMETHOD_BACKDOOR : g_ui16LoadMethod;
		// Enable Capture
		TALPDUMP_SetFlags((bPdump1 ? TAL_PDUMP_FLAGS_PDUMP1 : 0) |
						  (bPdump2 ? TAL_PDUMP_FLAGS_PDUMP2 : 0) |
						  ((bPdump1||bPdump2) ? (TAL_PDUMP_FLAGS_RES|TAL_PDUMP_FLAGS_PRM) : 0));
	}
#endif

	//These are still being used by drivers host code - NEED TO BE REMOVED
	ui32DmacRegId = TAL_GetMemSpaceHandle("REG_DMAC");
	ui32TopazMulticoreRegId = TAL_GetMemSpaceHandle("REG_TOPAZHP_MULTICORE");
	ui32SysMemId = TAL_GetMemSpaceHandle("MEMSYSMEM");
	
	ui32CommsRegId = TAL_GetMemSpaceHandle("REG_COMMS");
	ui32TestRegId = TAL_GetMemSpaceHandle("REG_TOPAZHP_TEST");

	num_cores = TOPAZKM_GetNumPipes();
	SYSOSKM_CopyToUser((IMG_VOID *)pui32NumPipes, (IMG_VOID *)&num_cores, sizeof(*pui32NumPipes));

	for (i = 0; i < * pui32NumPipes; i++) {
		IMG_CHAR szName[64];
		SPRINT(szName, "REG_TOPAZHP_CORE_%d", i);
		aui32HpCoreRegId[i] = TAL_GetMemSpaceHandle(szName);

		SPRINT(szName, "REG_TOPAZHP_VLC_CORE_%d", i);
		aui32VLCRegId[i] = TAL_GetMemSpaceHandle(szName);
#if !defined(IMG_KERNEL_MODULE)
		TALPDUMP_MemSpceCaptureEnable(aui32HpCoreRegId[i], IMG_TRUE, &bPdumpState);
		TALPDUMP_MemSpceCaptureEnable(aui32VLCRegId[i], IMG_TRUE, &bPdumpState);
#endif
	}

#if !defined(IMG_KERNEL_MODULE)
	TALPDUMP_MemSpceCaptureEnable(ui32TopazMulticoreRegId, IMG_TRUE, &bPdumpState);
	TALPDUMP_MemSpceCaptureEnable(ui32SysMemId, IMG_TRUE, &bPdumpState);
	TALPDUMP_MemSpceCaptureEnable(ui32DmacRegId, IMG_TRUE, &bPdumpState);
	// New regions have to be recorded by pdump
	TALPDUMP_MemSpceCaptureEnable(ui32CommsRegId, IMG_TRUE, &bPdumpState);
	TALPDUMP_MemSpceCaptureEnable(ui32TestRegId, IMG_TRUE, &bPdumpState);
	TALPDUMP_CaptureStart(".");
#endif

#if defined(IMG_KERNEL_MODULE)
		g_SYSDevHandle = devContext->hSysDevHandle;
#endif

	if (MMUDeviceMemoryInitialise(ui32MmuFlags, ui32MMUTileStride) == IMG_FALSE)
	{
		PRINT("\nERROR: Could not initialize MMU with selected parameters!\n");
		DMANKM_UnlockDeviceContext(devHandle);
		return IMG_ERR_MEMORY;
	}

#if !defined(IMG_KERNEL_MODULE)
	TALPDUMP_ConsoleMessage(TAL_MEMSPACE_ID_ANY, pszDriverVersion);
#endif

	if(ui32MmuFlags & MMU_SECURE_FW_UPLOAD)
	{
		TAL_PdumpComment(ui32TopazMulticoreRegId, "----SecHost START-----------------------------------");
	}

	//Start up MMU support for each core (if MMU is switched on)
	ui32RegVal = F_ENCODE( 1, TOPAZHP_TOP_CR_IMG_TOPAZ_MTX_SOFT_RESET )| F_ENCODE( 1,TOPAZHP_TOP_CR_IMG_TOPAZ_CORE_SOFT_RESET) | F_ENCODE(1,TOPAZHP_TOP_CR_IMG_TOPAZ_IO_SOFT_RESET);
	TALREG_WriteWord32(
		ui32TopazMulticoreRegId,
		TOPAZHP_TOP_CR_MULTICORE_SRST,
		ui32RegVal);
	TALREG_WriteWord32(
		ui32TopazMulticoreRegId,
		TOPAZHP_TOP_CR_MULTICORE_SRST,
		0x0);

#if defined(FAKE_MTX)
	if (gbFakeMtx)
	{
		TALREG_WriteWord32(
			ui32TopazMulticoreRegId,
			TOPAZHP_TOP_CR_MULTICORE_IDLE_PWR_MAN,
			F_ENCODE(1, TOPAZHP_TOP_CR_TOPAZ_IDLE_DISABLE));
	}
#endif


	for ( i = 0; i < * pui32NumPipes; i++ )
	{
		IMG_UINT32 uResetBits = F_ENCODE(1, TOPAZHP_CR_TOPAZHP_IPE_SOFT_RESET) |
			F_ENCODE(1, TOPAZHP_CR_TOPAZHP_SPE_SOFT_RESET) |
			F_ENCODE(1, TOPAZHP_CR_TOPAZHP_PC_SOFT_RESET) |
			F_ENCODE(1, TOPAZHP_CR_TOPAZHP_H264COMP_SOFT_RESET) |
			F_ENCODE(1, TOPAZHP_CR_TOPAZHP_JMCOMP_SOFT_RESET) |
			F_ENCODE(1, TOPAZHP_CR_TOPAZHP_PREFETCH_SOFT_RESET) |
			F_ENCODE(1, TOPAZHP_CR_TOPAZHP_VLC_SOFT_RESET ) |
			F_ENCODE(1, TOPAZHP_CR_TOPAZHP_DB_SOFT_RESET) |
			F_ENCODE(1, TOPAZHP_CR_TOPAZHP_LTRITC_SOFT_RESET) ;

#ifdef TOPAZHP
		uResetBits |= F_ENCODE(1, MVEA_CR_IMG_MVEA_SPE_SOFT_RESET(1)) |
			F_ENCODE(1, MVEA_CR_IMG_MVEA_IPE_SOFT_RESET(1));
#endif

		TALREG_WriteWord32(aui32HpCoreRegId[i], TOPAZHP_CR_TOPAZHP_SRST, uResetBits );

		TALPDUMP_MiscOutput(aui32HpCoreRegId[i], "IDL 10");

		TALREG_WriteWord32(aui32HpCoreRegId[i], TOPAZHP_CR_TOPAZHP_SRST, 0 );

	}

	if(ui32MmuFlags & MMU_SECURE_FW_UPLOAD)
	{
		TAL_PdumpComment(ui32TopazMulticoreRegId, "----SecHost END-----------------------------------");
	}

	MMUDeviceMemoryHWSetup(ui32TopazMulticoreRegId);

	devContext->firmwareUploaded = IMG_CODEC_NONE;


	TALREG_ReadWord32(ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_TOPAZHP_CORE_REV, &g_ui32CoreRev);
	TALREG_ReadWord32(ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_TOPAZHP_CORE_DES1, &g_ui32CoreDes1);

	if (g_ui32CoreRev < MIN_34_REV)
	{
		g_ui32CoreDes1 &= 0xFFFFF000;
	}

#if !defined (IMG_KERNEL_MODULE)
	// Pdump comments with core details
	{
		char szPdumpComment[80];
		SPRINT(szPdumpComment, "Core revision: 0x%08X", g_ui32CoreRev);
		TALPDUMP_Comment(ui32TopazMulticoreRegId, szPdumpComment);
		SPRINT(szPdumpComment, "Designer flags: 0x%08X", g_ui32CoreDes1);
		TALPDUMP_Comment(ui32TopazMulticoreRegId, szPdumpComment);
		SPRINT(szPdumpComment, "Number of cores: %d", TOPAZKM_GetNumPipes());
		TALPDUMP_Comment(ui32TopazMulticoreRegId, szPdumpComment);
	}

	// Poll on core revision register for pdump
	TALREG_Poll32(ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_TOPAZHP_CORE_REV, TAL_CHECKFUNC_ISEQUAL, g_ui32CoreRev, 0x00FF0000, 1, 1);
	TALREG_Poll32(ui32TopazMulticoreRegId, TOPAZHP_TOP_CR_TOPAZHP_CORE_REV, TAL_CHECKFUNC_GREATEREQ, g_ui32CoreRev, 0x0000FF00, 1, 1);
#endif

#ifdef EXIT_HANDLER
	SetupExitHandler(TALSETUP_Deinitialise);
#endif

#if defined (IMG_KERNEL_MODULE)
	g_apsWBDataInfo = IMG_MALLOC(sizeof(MEMORY_INFO *) * WB_FIFO_SIZE);
	IMG_ASSERT(g_apsWBDataInfo != IMG_NULL);
	if(g_apsWBDataInfo == IMG_NULL)
	{
		DMANKM_UnlockDeviceContext(devHandle);
		return IMG_ERR_MEMORY;			
	}
	SYSOSKM_CopyFromUser(g_apsWBDataInfo, apsWBDataInfo, sizeof(MEMORY_INFO *) * WB_FIFO_SIZE);
	// Allocate WB buffers.
	for (nIndex = 0; nIndex < WB_FIFO_SIZE; nIndex++)
	{
		MEMORY_INFO *mem_info;
		allocGenericMemory(devContext, COMM_WB_DATA_BUF_SIZE, &mem_info);
		SYSOSKM_CopyToUser(g_apsWBDataInfo[nIndex], mem_info, sizeof(MEMORY_INFO));
		g_apsWBDataInfo[nIndex] = mem_info;
	}
#else
	for (nIndex = 0; nIndex < WB_FIFO_SIZE; nIndex++)
		allocGenericMemory(devContext, COMM_WB_DATA_BUF_SIZE, &g_apsWBDataInfo[nIndex]);

	g_apsWBDataInfo = apsWBDataInfo;
#endif


	// Initialise the COMM registers
	comm_SetProducer(0);

	// Must reset the Consumer register too, otherwise the COMM stack may be initialised incorrectly
	comm_SetConsumer(0);

	for (nIndex = 0; nIndex < TOPAZHP_MAX_POSSIBLE_STREAMS; nIndex++)
	{
		devContext->deviceSockets[nIndex] = IMG_NULL;
	}
	devContext->usedSockets = 0;

	SYSOSKM_CreateMutex(&devContext->commTxLock);
	SYSOSKM_CreateMutex(&devContext->commRxLock);

	devContext->bInitialised = IMG_TRUE;
	DMANKM_UnlockDeviceContext(devHandle);


	return IMG_ERR_OK;
}

IMG_VOID
comm_Deinitialize(IMG_VOID *param)
{
	IMG_UINT32 nIndex;
	IMG_FW_CONTEXT *fwCtxt;
	IMG_HANDLE devHandle = (IMG_HANDLE *)param;
	TOPAZKM_DevContext *devContext;

	devContext = DMANKM_GetDevInstanceData(devHandle);


	fwCtxt = &devContext->sFwCtxt;



	if (fwCtxt && fwCtxt->bInitialized)
	{
#if defined(FAKE_MTX)
		if(gbFakeMtx)
		{
			// If there's no more connected sockets, just issue a command to shutdown the firmware.
			PRINT("Stopping the firmware.\n");

			TOPAZKM_MMUFlushMMUTableCache();

			{
				IMG_UINT32 ui32CmdWord;
				ui32CmdWord = F_ENCODE(0 /* SockId */, MTX_MSG_CORE) | MTX_CMDID_SHUTDOWN;
				ui32CmdWord |= F_ENCODE(0 /*psSocket->ui32LowCmdCount*/ & 0xff, MTX_MSG_COUNT);

				/* write command into FIFO */
				TALREG_WriteWord32(
					ui32TopazMulticoreRegId,
					TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
					ui32CmdWord);

				/* Write data to FIFO */
				TALREG_WriteWord32(
						ui32TopazMulticoreRegId,
						TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
						0);

				/* Write nothing */
				TALREG_WriteWord32(
					ui32TopazMulticoreRegId,
					TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
					0);

				/* LOW priority command */
				TALREG_WriteWord32(
					ui32TopazMulticoreRegId,
					TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
					0);
			}

			MTX_Kick(fwCtxt, 1);

			/* Stop the MTX firmware thread (deinit semaphores)*/
			FakeMTX_WaitForShutdown();
			FAKEMTX_StopMTX();
		}
		else
#endif
		{
			/* Stop the MTX */
			MTX_Stop(fwCtxt);
			MTX_WaitForCompletion(fwCtxt);
		}

#if defined(POLL_FOR_INTERRUPT)
		if(!isr_control.exit)
			stopISRThread();
#endif
	}

	if(g_apsWBDataInfo != IMG_NULL)
	{
		for (nIndex = 0; nIndex < WB_FIFO_SIZE; nIndex++)
			freeMemory(&g_apsWBDataInfo[nIndex]);

#if defined (IMG_KERNEL_MODULE)
		IMG_FREE(g_apsWBDataInfo);
#endif
	}

#ifdef FW_LOGGING
	fwtrace_stop(tracer);
	fwtrace_destroy(tracer);
	freeMemory(&trace_buffer);
#endif

	/* Close all of the opened sockets */
	for (nIndex = 0; nIndex < TOPAZHP_MAX_POSSIBLE_STREAMS; nIndex++)
	{
		if (devContext->deviceSockets[nIndex] != IMG_NULL)
			COMMKM_CloseSocket(devContext->deviceSockets[nIndex]->ui32ResourceId);
	}

	SYSOSKM_DestroyMutex(devContext->commTxLock);
	SYSOSKM_DestroyMutex(devContext->commRxLock);

	if (fwCtxt && fwCtxt->bInitialized)
		MTX_Deinitialize(fwCtxt);

	DeInitTal();

	devContext->firmwareUploaded = IMG_CODEC_NONE;
	devContext->bInitialised = IMG_FALSE;

}


/*
******************************************************************************

 @Function              COMM_GetNumPipes

 @details

 Get the number of pipes present

 @param   pui32NumPipes    : Number of pipes present

 @return   None

******************************************************************************/
IMG_UINT32
TOPAZKM_GetNumPipes()
{
	static IMG_UINT32 g_ui32PipesAvailable = 0;
	if(g_ui32PipesAvailable == 0)
	{
		/* get the actual number of cores */
		TALREG_ReadWord32(topaz_GetMulticoreRegId(), TOPAZHP_TOP_CR_MULTICORE_HW_CFG, &g_ui32PipesAvailable);
		g_ui32PipesAvailable = (g_ui32PipesAvailable & MASK_TOPAZHP_TOP_CR_NUM_CORES_SUPPORTED);
		IMG_ASSERT(g_ui32PipesAvailable != 0);
	}
	return g_ui32PipesAvailable;
}


/* EOF */

