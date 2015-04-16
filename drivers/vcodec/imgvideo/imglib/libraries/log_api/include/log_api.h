/*!
 *****************************************************************************
 *
 * @file	   log_api.h
 *
 * Event logging header file.
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

#include <stddef.h>
#include <img_errors.h>
#include <img_types.h>
#include <img_defs.h>
#include <api_common.h>

#ifndef __THREADL__
#include <stdarg.h>
#endif

#if !defined WIN32 && !defined __linux__
	#include <metag/machine.inc>
	#include <metag/metagtbi.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined (__LOG_API_H__)
#define __LOG_API_H__

#define LOG_ALL_APIS			(0xFFFFFFFF)		/*!< Used with LOG_Enable() */
#define ASSERT_PIPE_LOG         (0xFFFFFFFF)
#define ASSERT_STRING_LENGTH    (512)

#define LOG_THREAD_MAX			4
////#define LOG_THREAD_MAX		1

/*!
******************************************************************************
 This type defines the level of logging mode for each of the APIs
******************************************************************************/
typedef struct
{
	IMG_UINT32		ui32ElapseTime;			//!< Latest elapse time
	IMG_UINT32		ui32MaxTime;			//!< Max elapse time
	IMG_UINT32		ui32MinTime;			//!< Min elapse time
	IMG_UINT32		ui32AveTime;			//!< Average elapse time	
	IMG_UINT32		ui32Count;				//!< Count of calls elapse time
	IMG_UINT32		ui32TotalElapseTime;	//!< Total elapse time
	IMG_UINT32		ui32TimeStamp;			//!< Current time stamp
	
	/* Private member */
	IMG_BOOL		bCounting;
	IMG_UINT32		ui32StartCount;

} LOG_sElapseTime;

/*!
******************************************************************************
 This structure contains the event id and format string for the event
******************************************************************************/
typedef struct
{
	IMG_UINT32			ui32EventId;
	IMG_CHAR * 			pszFormatStrings;

} LOG_sApiEventInfo;

/* 
******************************************************************************
 This structure contains the API information including a link to the
 event table for API events
*****************************************************************************/
typedef struct
{
	IMG_UINT32					ui32ApiId;
	IMG_CHAR *					pszApi;
	LOG_sApiEventInfo * 	    psApiEventInfo;
	IMG_UINT32					ui32NoEvents;

} LOG_sApiInfo;

#if defined _LOG_EVENTS_ || defined _POST_PROCESSOR_

#define LOG_ELEMENT_SIZE (5)

/*!
******************************************************************************
 This type defines the level of logging mode for each of the APIs
******************************************************************************/
typedef enum {
	LOG_LEVEL_OFF=0x0,			//!< Logging is off for the API
	LOG_LEVEL_HIGH,				//!< Only log high-level events for the API
	LOG_LEVEL_MEDIUM,			//!< Log high and medium level events for the API
	LOG_LEVEL_LOW,				//!< All events for the API

} LOG_eLevel;

/*!
******************************************************************************
 This type defines the log flags.

 Log flag are used to improve the usefulness of the processed log output.

 "Start" and "End" flags can be used to qualify an even and measure the time
 between the start and end.

 Arg1 qualifiers and/or Arg2 qualifier cna be used to qualifier output further
 using the values of Arg1 and/or Arg2.
******************************************************************************/
typedef enum {
	LOG_FLAG_NONE		=0x00000000,			//!< No log flags
	LOG_FLAG_START		=0x00000001,			//!< Flag "start" point
	LOG_FLAG_END		=0x00000002,			//!< Flag "end" point
	LOG_FLAG_QUAL_ARG1	=0x00000004,			//!< Use Arg1 as an event qualifier
	LOG_FLAG_QUAL_ARG2	=0x00000008,			//!< Use Arg2 as an event qualifier

} LOG_eFlags;

/*!
******************************************************************************
 This type defines the mode in which the buffer is to be used
******************************************************************************/
typedef enum {
	LOG_CYCLIC=0x0,				//!< Log is cyclic
	LOG_SINGLE_SHOT,			//!< Log is "single-shot" logging will stop when the log is full

} LOG_eLogMode;


/*!
******************************************************************************
 This structure contains the Deferred Write Cache Context
******************************************************************************/
typedef struct
{
	LOG_eLevel		eLevel;			//!< Logging level for this API
	LOG_eLogMode	eLogMode;		//!< Logging mode for this API

} LOG_sApiContext;

/*!
******************************************************************************
 Global variables
******************************************************************************/
#if !defined WIN32 && !defined __linux__
extern IMG_UINT32			gui32LogThreadId;

extern IMG_UINT32			gEventLogSize 				__attribute__ ((section ("LOG_GLOBAL_DATA")));
extern IMG_UINT32			gEventLog			[] 		__attribute__ ((section ("LOG_GLOBAL_DATA")));

extern IMG_BOOL				gbLogEnabled				__attribute__ ((section ("LOG_GLOBAL_DATA")));
extern LOG_sApiContext		gaeLogApi			[]		__attribute__ ((section ("LOG_GLOBAL_DATA")));

extern	IMG_UINT32 *		LOG_apui32Log		[] 		__attribute__ ((section ("LOG_GLOBAL_DATA")));
extern	IMG_UINT32 			LOG_aui32LogSize	[] 		__attribute__ ((section ("LOG_GLOBAL_DATA")));
extern	IMG_UINT32 			LOG_aui32LogIndex	[]	 	__attribute__ ((section ("LOG_GLOBAL_DATA")));
extern	LOG_eLevel			LOG_aeLevel			[]	 	__attribute__ ((section ("LOG_GLOBAL_DATA")));

extern IMG_UINT32			gui32NoActiveAPIs 			__attribute__ ((section ("LOG_GLOBAL_DATA")));
extern IMG_UINT32			gui32MaxNoActiveAPIs 		__attribute__ ((section ("LOG_GLOBAL_DATA")));
extern IMG_UINT32			gui32LogSize 				__attribute__ ((section ("LOG_GLOBAL_DATA")));
extern IMG_UINT32 *			gpui32Log 					__attribute__ ((section ("LOG_GLOBAL_DATA")));
extern TBISPIN				ThreadLock 					__attribute__ ((section ("LOG_GLOBAL_DATA")));
#else

extern LOG_sApiContext		gaeLogApi[];
extern IMG_BOOL				gbLogEnabled;

#endif

#ifndef __THREADL__


extern IMG_VOID LOG_Deinitialise(IMG_VOID);
/*!
******************************************************************************

 @Function				LOG_Initialise
 
 @Description 
 
 This function is used to initialise the loging system.  This will disable
 logging of all APIs.

 @Input		:			None.

 @Return	:			None.

******************************************************************************/
extern IMG_VOID LOG_Initialise(IMG_VOID);

/*!
******************************************************************************

 @Function				LOG_SetupLog
 
 @Description 
 
 This function is used to setup or change the logging for an API.

 NOTE: Not used in all implementations of th LOG API.

 @Input		ui32Api		: The API identifier.

 @Output	peLogLevel : A pointer to a local log level for this API.

 @Output	ppui32Log   : A pointer to the log buffer to be used.

 @Output	pui32LogSize : A pointer to the size of the log buffer.

 @Output	pui32LogIndex : A pointer to the index onto the log buffer.

 @Return	:			None.

******************************************************************************/
extern IMG_VOID LOG_SetupLog(
	IMG_UINT32		ui32Api,
	LOG_eLevel *	peLogLevel,
	IMG_UINT32 **	ppui32Log,
	IMG_UINT32 *	pui32LogSize, 
	IMG_UINT32 *	pui32LogIndex
);

/*!
******************************************************************************

 @Function				LOG_PerformLog
 
 @Description 
 
 This function is used to log an event.

 @Input		ui32Api		: The API identifier.
 
 @Input		ui32Event	: The event identifier.

 @Input		ui32Flags	: Event flags.

 @Output	ppui32Log   : A pointer to the log buffer to be used.

 @Output	pui32LogSize : A pointer to the size of the log buffer.

 @Output	pui32LogIndex : A pointer to the index onto the log buffer.

 @Input		ui32Arg1	: Argument 1.

 @Input		ui32Arg1	: Argument 2.

 @Return	:			None.

******************************************************************************/
extern IMG_VOID LOG_PerformLog(
	IMG_UINT32		ui32Api,
	IMG_UINT32		ui32Event,	
	IMG_UINT32		ui32Flags,	
	IMG_UINT32 **	ppui32Log,
	IMG_UINT32 *	pui32LogSize, 
	IMG_UINT32 *	pui32LogIndex,
	IMG_UINT32		ui32Arg1,
	IMG_UINT32		ui32Arg2
);

/*!
******************************************************************************

 @Function				LOG_ElapseTimeStart
 
 @Description 
 
 This function is used to set the current time in an elapse time structure.

 @Input		psElapseTime : A pointer to the elapse time structure

 @Return	:			None.

******************************************************************************/
extern IMG_VOID LOG_ElapseTimeStart(
	LOG_sElapseTime	*	psElapseTime
);


/*!
******************************************************************************

 @Function				LOG_ElapseTimeEnd
 
 @Description 
 
 This function is used to determine the elapse time and return this in the 
 elapse time structure.

 @Input		psElapseTime : A pointer to the elapse time structure

 @Return	:			None.

******************************************************************************/
extern IMG_VOID LOG_ElapseTimeEnd(
	LOG_sElapseTime	*	psElapseTime
);

/*!
******************************************************************************

 @Function				LOG_Enable
 
 @Description 
 
 This function is used to enable event logging for an API.

 @Input		ui32Api		: The API id.

 @Input		eLevel		: The level(s) to be enabled.

 @Input		eLogMode	: The log mode to be used.

 @Return	:			None.

******************************************************************************/
extern IMG_VOID LOG_Enable(
	IMG_UINT32		ui32Api,
	LOG_eLevel		eLevel,
	LOG_eLogMode	eLogMode
);

/*!
******************************************************************************

 @Function				LOG_Disable
 
 @Description 
 
 This function is used to disable event logging for an API.

 @Input		ui32Api		: The API id.

 @Return	:			None.

******************************************************************************/
extern IMG_VOID LOG_Disable(
	IMG_UINT32		ui32Api
);

/*!
******************************************************************************

 @Function				LOG_StopLogging
 
 @Description 
 
 This function is used to stop event logging for all APIs.

 @Input		:			None.

 @Return	:			None.

******************************************************************************/
extern IMG_VOID LOG_StopLogging(IMG_VOID);

/*!
******************************************************************************

 @Function				LOG_StopAllThreads
 
 @Description 
 
 This function is used to stop all threads.

 @Input		:			None.

 @Return	:			None.

******************************************************************************/
extern IMG_VOID LOG_StopAllThreads(IMG_VOID);

#ifdef _POST_PROCESSOR_

#else

/*!
******************************************************************************

 @Function				LOG_WriteBinaryData

 @Description

 This function writes binary data into the driver circular buffer.

 @Input ui32ApiIndex : API Index into the API Info array of structures

 @Input ui32Event    : Index into the API Event array of structures

 @Input ui32Arg1     : First argument

 @Input ui32Arg2     : Second argument

 @Input ui32Flags    : Flags

 @Output None

 @Return 
 
******************************************************************************/
extern IMG_RESULT LOG_WriteBinaryData(
	IMG_UINT32 ui32ApiIndex,
	IMG_UINT32 ui32Event,
	IMG_UINT32 ui32Arg1,
	IMG_UINT32 ui32Arg2,
	IMG_UINT32 ui32Flags
	);

/*!
******************************************************************************

 @Function				LOG_WriteString

 @Description

 This function writes a string into the driver circular buffer.

 @Input pszString : pointer to the string to be stored.

 @Output None

 @Return
 
******************************************************************************/
extern IMG_RESULT LOG_WriteString(
	IMG_CHAR *          pszString
	);

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

/*!
******************************************************************************

 @Function				LOG_Event
 
 @Description 
 
 This function is used to log an event.

 @Input		ui32Api		: The API identifier.
 
 @Input		ui32Event	: The event identifier.

 @Input		ui32Flags	: Event flags.

 @Input		eLevel		: The level of this event

 @Output	peLogLevel : A pointer to a local log level for this API.

 @Output	ppui32Log   : A pointer to the log buffer to be used.

 @Output	pui32LogSize : A pointer to the size of the log buffer.

 @Output	pui32LogIndex : A pointer to the index onto the log buffer.

 @Input		ui32Arg1	: Argument 1.

 @Input		ui32Arg1	: Argument 2.

 @Return	:			None.

******************************************************************************/
#if !defined WIN32 && !defined __linux__
IMG_INLINE static IMG_VOID LOG_Event(
	IMG_UINT32		ui32Api,
	IMG_UINT32		ui32Event,	
	IMG_UINT32		ui32Flags,	
	LOG_eLevel		eLevel,
	IMG_UINT32		ui32Arg1,
	IMG_UINT32		ui32Arg2
)
{
	IMG_UINT32		ui32Thread = (__TBIThreadId() >> TBID_THREAD_S);
	IMG_UINT32		ui32Slot = ui32Thread + (ui32Api * LOG_THREAD_MAX);

	LOG_eLevel *	peLogLevel		= &LOG_aeLevel [ui32Slot];
	IMG_UINT32 **	ppui32Log		= &LOG_apui32Log [ui32Slot];
	IMG_UINT32 *	pui32LogSize	= &LOG_aui32LogSize [ui32Slot];
	IMG_UINT32 *	pui32LogIndex	= &LOG_aui32LogIndex [ui32Slot];

	/* If logging is not enabled...*/
	if (!gbLogEnabled)
	{
		/* Nothing to do */
		return;
	}

	/* Ensure that the API is valid */ 
	IMG_ASSERT(ui32Api < API_ID_MAX);

	/* If the local flag is not the same as the global flag...*/
	if (*peLogLevel != gaeLogApi[ui32Api].eLevel)
	{

        //consolePrintf("Init log for API %d thread %d (slot %d)\n", ui32Api, ui32Thread, ui32Slot );
		
		/* Set up the log */
		LOG_SetupLog(ui32Api, peLogLevel, ppui32Log, pui32LogSize, pui32LogIndex);
	}

	/* If this level is being logged...*/
	if (*peLogLevel < eLevel)
	{
		/* Nothing to do */
		return;
	}
	/* We want to log the event...*/
	LOG_PerformLog(ui32Api, ui32Event, ui32Flags, ppui32Log, pui32LogSize, pui32LogIndex, ui32Arg1, ui32Arg2);
}
#else
IMG_INLINE static IMG_VOID LOG_Event(
	IMG_UINT32		ui32Api,
	IMG_UINT32		ui32Event,	
	IMG_UINT32		ui32Flags,	
	LOG_eLevel		eLevel,
	IMG_UINT32		ui32Arg1,
	IMG_UINT32		ui32Arg2
)
{
	/* If logging is not enabled...*/
	if (!gbLogEnabled)
	{
		/* Nothing to do */
		return;
	}

	/* Ensure that the API is valid */ 
	IMG_ASSERT(ui32Api < API_ID_MAX);

	/* If this level is being logged...*/
	if (gaeLogApi[ui32Api].eLevel < eLevel)
	{
		/* Nothing to do */
		return;
	}

	/* We want to log the event...*/
	LOG_PerformLog(ui32Api, ui32Event, ui32Flags, IMG_NULL, IMG_NULL, IMG_NULL, ui32Arg1, ui32Arg2);
}
#endif

/*!
******************************************************************************

 @Function				LOG_EVENT
 
 @Description 
 
 This macro is used to log events within an API.

 @Input		api			: The API identifier (e.g. SMDEC).

 @Input		event		: The event identifier, a short name identifying the 
						  event (e.g. SMDEC_Reset).

 @Input		flags		: Event flags.

 @Input		arg1		: Event argument 1.

 @Input		arg2		: Event argument 2.

******************************************************************************/
#define LOG_EVENT(api,event,flags,arg1,arg2)							\
	LOG_Event(API_ID_##api,												\
			  api##_##event##_EVENT_NUM,								\
			  flags,													\
			  api##_##event##_eLevel,									\
			  (IMG_UINT32) arg1, (IMG_UINT32) arg2)

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////


/*!
******************************************************************************

 @Function				LOG_ENABLE
 
 @Description 
 
 This macro is used to enable or change API events logging

 @Input		api			: The API identifier (e.g. SMDEC).

 @Input		level		: The level of log events to be enabled HIGH, MEDIUM or LOW.

 @Input		mode		: The buffer mode - CYCLIC or SINGLE_SHOT.

******************************************************************************/
#define	LOG_ENABLE(api,level,mode)								\
	LOG_Enable(API_ID_##api, LOG_LEVEL_##level, LOG_##mode);	\

/*!
******************************************************************************

 @Function				LOG_DISABLE
 
 @Description 
 
 This macro is used to disable API events logging.

 @Input		api			: The API identifier (e.g. SMDEC).

******************************************************************************/
#define	LOG_DISABLE(api)			\
	LOG_Disable(API_ID_##api);		\

/*!
******************************************************************************

 @Function				LOG_STOP_LOGGING
 
 @Description 
 
 This macro is used to stop all event logging.

 @Input		None

******************************************************************************/
#define	LOG_STOP_LOGGING()	LOG_StopLogging();

/*!
******************************************************************************

 @Function				LOG_STOP_ALL_THREADS
 
 @Description 
 
 This macro is used to stop all threads.

 @Input		None

******************************************************************************/
#define	LOG_STOP_ALL_THREADS()	LOG_StopAllThreads();

#endif

#endif

#else

/*** If not _LOG_EVENTS_ || _POST_PROCESSOR_ then provide dummy macros ***/
#define LOG_Initialise()
#define DECL_LOG_EVENT_START(api)
#define DECL_LOG_EVENT(api,event,level,formatstring)
#define DECL_LOG_EVENT_END()
#define LOG_EVENT(api,event,flags,arg1,arg2)
#define	LOG_ENABLE(api,level,mode)
#define	LOG_DISABLE(api)
#define	LOG_STOP_LOGGING()
#define	LOG_STOP_ALL_THREADS()

IMG_INLINE static IMG_VOID
LOG_ElapseTimeStart (
	LOG_sElapseTime	*	psElapseTime
)
{
	psElapseTime->ui32ElapseTime = 0;
}

#define LOG_ElapseTimeEnd(psElapseTime)

#endif 

#endif //__LOG_API_H__

#if defined(__cplusplus)
}
#endif

/*!
******************************************************************************

 @Function				DECL_LOG_EVENT
 
 @Description 
 
 This macros is use in the embedded system to .
 
 @Return	:			None.

******************************************************************************/
#ifdef _POST_PROCESSOR_
	#undef DECL_LOG_EVENT_START
	#undef DECL_LOG_EVENT
	#undef DECL_LOG_EVENT_END	

	#if defined LOG_SECOND_PASS
		#undef LOG_SECOND_PASS
	
		#define DECL_LOG_EVENT_START(api)									\
		static LOG_sApiEventInfo   api##sApiEventInfo[] = {
		
		#define DECL_LOG_EVENT(api,event,level,formatstring)				\
		{ api##_##event##_EVENT_NUM, formatstring},
		
		#define DECL_LOG_EVENT_END()										\
		};
	#else
		#define DECL_LOG_EVENT_START(api)									\
		enum {
		
		#define DECL_LOG_EVENT(api,event,level,formatstring)				\
		api##_##event##_EVENT_NUM,
		
		#define DECL_LOG_EVENT_END()	};	
		
		#define	LOG_SECOND_PASS	
	#endif

#else	//_POST_PROCESSOR_

	#if defined _LOG_EVENTS_ 
		#if defined LOG_SECOND_PASS		
			#undef LOG_SECOND_PASS	
			
			#undef DECL_LOG_EVENT_START
			#undef DECL_LOG_EVENT
			#undef DECL_LOG_EVENT_END
		
			#define DECL_LOG_EVENT_START(api)
			
			#define DECL_LOG_EVENT(api,event,level,formatstring)				\
			static LOG_eLevel		 api##_##event##_eLevel = LOG_LEVEL_##level;
			
			#define DECL_LOG_EVENT_END()
		#else		
			#define DECL_LOG_EVENT_START(api)									\
			enum {						
			
			#define DECL_LOG_EVENT(api,event,level,formatstring)				\
			api##_##event##_EVENT_NUM,
			
			#define DECL_LOG_EVENT_END()	};	
			
			#define	LOG_SECOND_PASS
		#endif
	#endif
	
#endif

