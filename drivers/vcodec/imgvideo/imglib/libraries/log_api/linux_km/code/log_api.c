/*!
 *****************************************************************************
 *
 * @file	   log_api.c
 *
 * This file contains the implementation of the debug logging functions#
 * for the Visual Framework.
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

#ifdef _LOG_EVENTS_

#include "img_defs.h"

#if !defined(IMG_KERNEL_MODULE)
  *** NOT YET IMPLEMENTED FOR USER MODE ***
#else
  #include <linux/kernel.h>
  #include <linux/slab.h>
#endif

/*!
******************************************************************************
 This causes enums for the event to be generated and the table containing the
 even number and format string
******************************************************************************/
#define _POST_PROCESSOR_
#include "dman_api_log.h"
#include "dman_api_km_log.h"
#include "vdecdd_log.h"
#ifdef USE_KERNEL_MODE_DMAC_DEVICE_DRIVER
#include "dmac_device_log.h"
#endif

#include "page_alloc_log.h"
#include "wrap_utils_log.h"
#include "sysbrg_api_log.h"
#include "ump_api_km.h"
#include "system.h"

extern IMG_INT32 gi32ApiEntries;
extern LOG_sApiInfo asApiInfo[];

#define DECL_API(api)																					\
{ API_ID_##api, #api, & api##sApiEventInfo[0], (sizeof( api##sApiEventInfo)/sizeof(LOG_sApiEventInfo)) },

/*
******************************************************************************
To include the logging for an API include the API using the DECL_API macro here...
*****************************************************************************/
LOG_sApiInfo asApiInfo[] = {
	DECL_API(DMAN)
	DECL_API(DMANKM)
	DECL_API(VDECDD)
#ifdef USE_KERNEL_MODE_DMAC_DEVICE_DRIVER
	DECL_API(DMACDD)
#endif
	DECL_API(PALLOC)
	DECL_API(WRAPU)
	DECL_API(SYSBRG)

	{ 0, IMG_NULL, IMG_NULL, 0 }			/*** Used to identify the end of the list ***/
};

IMG_INT32 gi32ApiEntries = sizeof(asApiInfo) / sizeof(asApiInfo[0]);

#if !(defined __RELEASE_RELEASE__ || defined NO_ASSERT)
#ifdef PIPE_ASSERT
static IMG_CHAR pszAssertString[256];
#endif
#endif

/*!
******************************************************************************
 Global variables
******************************************************************************/
LOG_sApiContext		gaeLogApi[API_ID_MAX];
IMG_BOOL			gbLogEnabled = IMG_FALSE;
IMGVIDEO_EXPORT_SYMBOL(gaeLogApi)
IMGVIDEO_EXPORT_SYMBOL(gbLogEnabled)

extern int pipe_log;
static IMG_BOOL		gInitialised = IMG_FALSE;
static IMG_HANDLE      ghLoggingPipeHandle;
static UMP_sConfigData gsConfigData;

/*!
******************************************************************************

 @Function				LOG_WriteBinaryData

******************************************************************************/
static IMG_RESULT LOG_WriteBinaryData(
	IMG_UINT32 ui32ApiIndex,
	IMG_UINT32 ui32Event,
	IMG_UINT32 ui32Arg1,
	IMG_UINT32 ui32Arg2,
	IMG_UINT32 ui32Flags
	)
{
	IMG_UINT32 pi32Data[5];

	pi32Data[0] = ui32ApiIndex;
	pi32Data[1] = ui32Event;
	pi32Data[2] = ui32Arg1;
	pi32Data[3] = ui32Arg2;
	pi32Data[4] = ui32Flags;

	UMPKM_WriteRecord(
		ghLoggingPipeHandle,
		5 * sizeof(IMG_INT32),
		(IMG_VOID *)pi32Data
		);

	/* Return success...*/
	return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function				LOG_WriteString

******************************************************************************/
static IS_NOT_USED IMG_RESULT LOG_WriteString(
	IMG_CHAR *          pszString
	)
{
	UMPKM_WriteRecord(
		ghLoggingPipeHandle,
		strlen(pszString),
		(IMG_VOID *)pszString
		);

	/* Return success...*/
	return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function				LOG_Initialise

******************************************************************************/
IMG_VOID LOG_Initialise(IMG_VOID)
{
	IMG_UINT32		ui32ApiIndex;
	IMG_UINT32		ui32EventId;
	IMG_RESULT      ui32Result;

	if (!gInitialised)
	{
		/* Disable logging...*/
		gbLogEnabled = IMG_FALSE;
		LOG_StopLogging();

		/* Quick xcheck that event nos are correct...*/
		ui32ApiIndex = 0;
		while (asApiInfo[ui32ApiIndex].pszApi != IMG_NULL)
		{
			for (ui32EventId=0; ui32EventId<asApiInfo[ui32ApiIndex].ui32NoEvents; ui32EventId++)
			{
				/**** Fails the the evnt IDs are incorrect and not numbered sequentially starting @ 0
					  Fix <api>_log.h event definitions ***/
				IMG_ASSERT(asApiInfo[ui32ApiIndex].psApiEventInfo[ui32EventId].ui32EventId == ui32EventId);
			}
			ui32ApiIndex++;
		}

		/* Initialise the User Mode Pipe...*/
		/* gsConfigData.eBufferMode = UMP_MODE_LOST_LESS; */
		gsConfigData.eBufferMode = UMP_MODE_CYCLIC;
		gsConfigData.ui32BufSize = SYS_LOG_BUFFER_SIZE;

		ui32Result = UMPKM_CreatePipe(
			"LoggingPipe",
			&gsConfigData,
			&ghLoggingPipeHandle
			);
		IMG_ASSERT(ui32Result == IMG_SUCCESS);
		if(ui32Result != IMG_SUCCESS)
		{
			return;
		}

		gInitialised = IMG_TRUE;
	}
}
IMG_VOID LOG_Deinitialise(IMG_VOID)
{
    DMANKM_UnRegisterDevice("LoggingPipe");
}
/*!
******************************************************************************

@Function				LOG_SetupLog

******************************************************************************/
IMG_VOID LOG_SetupLog(
	IMG_UINT32		ui32Api,
	LOG_eLevel *	peLogLevel,
	IMG_UINT32 **	ppui32Log,
	IMG_UINT32 *	pui32LogSize,
	IMG_UINT32 *	pui32LogIndex
	)
{
	/* Not used in this implementation...*/
	IMG_ASSERT(IMG_FALSE);
}

/*!
******************************************************************************

@Function				LOG_PerformLog

******************************************************************************/
IMG_VOID LOG_PerformLog(
	IMG_UINT32		ui32Api,
	IMG_UINT32		ui32Event,
	IMG_UINT32		ui32Flags,
	IMG_UINT32 **	ppui32Log,
	IMG_UINT32 *	pui32LogSize,
	IMG_UINT32 *	pui32LogIndex,
	IMG_UINT32		ui32Arg1,
	IMG_UINT32		ui32Arg2
)
{
	IMG_UINT32		ui32ApiIndex = 0;
	IMG_CHAR		aszMsg[500];		// keep this below 1000, to avoid "warning: frame size is larger than 1024 bytes "

	while (
			(asApiInfo[ui32ApiIndex].psApiEventInfo != IMG_NULL) &&
			(ui32Api != asApiInfo[ui32ApiIndex].ui32ApiId)
			)
	{
		ui32ApiIndex++;
	}
	/* API not found in list...*/
	IMG_ASSERT(asApiInfo[ui32ApiIndex].psApiEventInfo != IMG_NULL);

	/* If API found...*/
	if (asApiInfo[ui32ApiIndex].psApiEventInfo != IMG_NULL)
	{
		if(pipe_log) {
			if(gInitialised == IMG_TRUE)
				LOG_WriteBinaryData(ui32ApiIndex, ui32Event, ui32Arg1, ui32Arg2, ui32Flags);
		} else {
			char * p = aszMsg + sprintf(aszMsg, "%s","imgsysbrg: ");
			sprintf(p, asApiInfo[ui32ApiIndex].psApiEventInfo[ui32Event].pszFormatStrings, ui32Arg1, ui32Arg2);
			if ((ui32Flags & LOG_FLAG_START) != 0)
			{
				sprintf(aszMsg, "%s%s", aszMsg, " - START");
			}
			if ((ui32Flags & LOG_FLAG_END) != 0)
			{
				sprintf(aszMsg, "%s%s", aszMsg, " - END");
			}
			printk(KERN_NOTICE "%s\n", aszMsg);
		}
	}
}
IMGVIDEO_EXPORT_SYMBOL(LOG_PerformLog)

/*!
******************************************************************************

 @Function				LOG_Enable

******************************************************************************/
IMG_VOID LOG_Enable(
	IMG_UINT32		ui32Api,
	LOG_eLevel		eLevel,
	LOG_eLogMode	eLogMode
)
{
	if (ui32Api == LOG_ALL_APIS)
	{
		/* Loop over APIs */
		for (ui32Api=0; ui32Api<API_ID_MAX; ui32Api++)
		{
			LOG_Enable(ui32Api, eLevel, eLogMode);
		}
	}
	else
	{
		IMG_ASSERT(gInitialised);

		/* Ensure that the API is valid */
		IMG_ASSERT(ui32Api < API_ID_MAX);

		gbLogEnabled = IMG_TRUE;

		/* Set log level and mode */
		gaeLogApi[ui32Api].eLevel	= eLevel;
		gaeLogApi[ui32Api].eLogMode = eLogMode;
	}
}

/*!
******************************************************************************

 @Function				LOG_Disable

 @Description

 This function is used to disable event logging for an API.

 @Input		ui32Api		: The API id.

 @Return    IMG_VOID  : This function returns either IMG_SUCCESS or an
                          error code.

******************************************************************************/
IMG_VOID LOG_Disable(
	IMG_UINT32		ui32Api
)
{
	if(ui32Api == LOG_ALL_APIS)
	{
		gbLogEnabled = IMG_FALSE;
		printk(KERN_CRIT "%s: LOG DISABLED", __FUNCTION__);
		return;
	}

	/* Ensure that the API is valid */
	IMG_ASSERT(ui32Api < API_ID_MAX);

	/* Disable logging */
	gaeLogApi[ui32Api].eLevel = LOG_LEVEL_OFF;
}

/*!
******************************************************************************

 @Function				LOG_StopLogging

 @Description

 This function is used to stop event logging for all APIs.

 @Input		None.

 @Return    IMG_VOID  : This function returns either IMG_SUCCESS or an
                          error code.

******************************************************************************/
IMG_VOID LOG_StopLogging(IMG_VOID)
{
	IMG_UINT32		ui32Api;

	/* Loop over APIs */
	for (ui32Api=0; ui32Api<API_ID_MAX; ui32Api++)
	{
		LOG_Disable(ui32Api);
	}
}

/*!
******************************************************************************

 @Function				LOG_StopAllThreads

 @Description

 This function is used to stop all threads.

 @Input		None.

 @Return    IMG_VOID  : This function returns either IMG_SUCCESS or an
                          error code.

******************************************************************************/
IMG_VOID LOG_StopAllThreads(IMG_VOID)
{
	IMG_ASSERT(IMG_FALSE);
}


/*!
******************************************************************************

 @Function				LOG_ElapseTimeStart

******************************************************************************/
IMG_VOID LOG_ElapseTimeStart(
	LOG_sElapseTime	*	psElapseTime
)
{
	IMG_ASSERT(0);
#if 0
 	volatile IMG_UINT32 *       pTimer = LOG_TIMER_ADDR;
	IMG_UINT32					ui32Time = *pTimer;

	/* Cannot start when the previous count was not finished */
	IMG_ASSERT ( psElapseTime->bCounting == IMG_FALSE );

	/* If the "looks" like teh fisrt time...*/
	if (psElapseTime->ui32TimeStamp == 0)
	{
		/* Reset minimum */
		psElapseTime->ui32MinTime = 0xFFFFFFFF;
	}

	/* Set the time stamp */
	psElapseTime->ui32TimeStamp = ui32Time;

	psElapseTime->ui32StartCount ++;
	psElapseTime->bCounting = IMG_TRUE;
#endif
}


/*!
******************************************************************************

 @Function				LOG_ElapseTimeEnd

******************************************************************************/
IMG_VOID LOG_ElapseTimeEnd(
	LOG_sElapseTime	*	psElapseTime
)
{
 	IMG_ASSERT(0);
#if 0
	volatile IMG_UINT32 *       pTimer = LOG_TIMER_ADDR;
	IMG_UINT32					ui32Time = *pTimer;

	/* Check there's a count to end */
	IMG_ASSERT ( psElapseTime->bCounting == IMG_TRUE );

	/* Set the time stamp */
	if (ui32Time > psElapseTime->ui32TimeStamp)
	{
		/* Update the elapse time...*/
		psElapseTime->ui32ElapseTime = ui32Time - psElapseTime->ui32TimeStamp;
	}
	else
	{
		psElapseTime->ui32ElapseTime =  psElapseTime->ui32TimeStamp - ui32Time;
	}

	/* Check for > max...*/
	if (psElapseTime->ui32ElapseTime > psElapseTime->ui32MaxTime)
	{
		psElapseTime->ui32MaxTime = psElapseTime->ui32ElapseTime;
	}
	/* Check for <> mim...*/
	if (psElapseTime->ui32ElapseTime < psElapseTime->ui32MinTime)
	{
		psElapseTime->ui32MinTime = psElapseTime->ui32ElapseTime;
	}

	/* Calculate average...*/
	psElapseTime->ui32Count++;
	psElapseTime->ui32TotalElapseTime += psElapseTime->ui32ElapseTime;
	psElapseTime->ui32AveTime = psElapseTime->ui32TotalElapseTime / psElapseTime->ui32Count;

	psElapseTime->bCounting = IMG_FALSE;
#endif
}

#endif

