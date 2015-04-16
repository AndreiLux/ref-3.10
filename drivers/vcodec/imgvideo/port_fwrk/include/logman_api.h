/*!
 *****************************************************************************
 *
 * @file	   logman_api.h
 *
 * The Device Manager user mode API.
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

#if !defined (__LOGMAN_API_H__)
#define __LOGMAN_API_H__

#include <img_defs.h>
#include <log_api.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* Clears '__user' in user space but make it visible for doxygen */
#if !defined(IMG_KERNEL_MODULE) && !defined(__RPCCODEGEN__)
    #define __user
#endif

#ifdef  __RPCCODEGEN__
  #define rpc_prefix      LOGMAN
  #define rpc_filename    logman_api
#endif


/*!
******************************************************************************

 @Function				LOGMAN_EnableLogging

 @Description 

 This functions enables logging for the specified API using the specified
 Log level.

 @Input		i32ApiId    :   API id.

 @Input     i32LogLevel :   Log Level for the specified API.

 @Output    None

 @Return	IMG_RESULT  :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
IMG_RESULT LOGMAN_EnableLogging(
	IMG_INT32 i32ApiId,
	IMG_INT32 i32LogLevel
);

/*!
******************************************************************************

 @Function				LOGMAN_DisableLogging

 @Description 
 
 This Function disables logging for the specified API.

 @Input		i32ApiId :      API id.

 @Output    None

 @Return	IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
IMG_RESULT LOGMAN_DisableLogging(
	IMG_INT32 i32ApiId
);

/*!
******************************************************************************

 @Function				LOGMAN_GetApiSize

 @Description 
 
 This functions returns the number of API

 @Input     None 

 @Output	pi32ApiSize :   Pointer to integer containing the number of APIs. 

 @Return	IMG_RESULT  :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
IMG_RESULT LOGMAN_GetApiSize(
	IMG_INT32  __user * pi32ApiSize
	);

/*!
******************************************************************************

 @Function				LOGMAN_GetApiList

 @Description 
 
 This function is used to populate the structure containing informations
 about all the APIs.

 @Input		None

 @Output    psApiInfo  :    Pointer to structures containing info about APIs.

 @Return	IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
IMG_RESULT LOGMAN_GetApiList(
	LOG_sApiInfo __user * psApiInfo
	);

/*!
******************************************************************************

 @Function				LOGMAN_GetApiEvents

 @Description 
 
 This function returns an array of integers. Each element contains the number
 of events per API.

 @Input		None

 @Output    pi32ApiEvents : Pointer to array of integers containing the
                            number of events per API.

 @Return	IMG_RESULT    :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
IMG_RESULT LOGMAN_GetApiEvents(
	IMG_INT32 __user * pi32ApiEvents
	);

/*!
******************************************************************************

 @Function				LOGMAN_GetFormatStrings

 @Description 
 
 This function returns the format string for the specific event of the
 specific API.

 @Input		i32ApiIndex :   Api index (the value returned by LOGMAN_GetApiSize
                            should be the maximum value allowed).

 @Input     i32Event    :   Api event (the value returned by LOGMAN_GetApiEvents
                            should be the maximum value allowed per API.

 @Output    pszString   :   Pointer to the buffer which will contain the
                            format string.

 @Return	IMG_RESULT  :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
IMG_RESULT LOGMAN_GetFormatStrings(
	IMG_CHAR __user * pszString,
	IMG_INT32      i32ApiIndex,
	IMG_INT32      i32Event
	);

/*!
******************************************************************************

 @Function				LOGMAN_TestLogEnabled

 @Description 
 
 This functions returns true if the user mode logging system is enabled in the
 kernel driver, false otherwise.

 @Input		None
 
 @Output    pbLogEnabled :  Pointer to boolean which is true if logging system
                            is enabled. False otherwise.

 @Return	IMG_RESULT   :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT LOGMAN_TestLogEnabled(
	IMG_BOOL __user * pbLogEnabled
	);
	
#if defined (__cplusplus)
}
#endif
 
#endif /* __LOGMAN_API_H__	*/


