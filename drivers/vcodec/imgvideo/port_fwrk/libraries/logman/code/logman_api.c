/*!
 *****************************************************************************
 *
 * @file	   logman_api.c
 *
 * This file contains the Device Manager API.
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

#ifndef _LOG_EVENTS_
#define _LOG_EVENTS_
#endif
#define CROSS_COPY

#include <img_errors.h>
#include <logman_api.h>
#include <log_api.h>
#include <sysos_api_km.h>

extern LOG_sApiInfo asApiInfo[];
extern IMG_INT32 gi32ApiEntries;

/*!
******************************************************************************

 @Function				LOGMAN_EnableLogging

******************************************************************************/
IMG_RESULT LOGMAN_EnableLogging(
	IMG_INT32 i32ApiId,
	IMG_INT32 i32LogLevel
	)
{
#ifdef WINCE
	IMG_ASSERT(0);
#else //Not WINCE
	LOG_Enable(i32ApiId, i32LogLevel, LOG_CYCLIC);
#endif

	/* Return success...*/
	return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function				LOGMAN_DisableLogging

******************************************************************************/
IMG_RESULT LOGMAN_DisableLogging(
	IMG_INT32 i32ApiId
	)
{
#ifdef WINCE
	IMG_ASSERT(0);
#else //Not WINCE
	LOG_Disable(i32ApiId);
#endif

	/* Return success...*/
	return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function				LOGMAN_GetApiSize

******************************************************************************/
IMG_RESULT LOGMAN_GetApiSize(
    IMG_INT32 __user * pi32ApiSize
	)
{
#ifdef WINCE
	IMG_ASSERT(0);
#else //Not WINCE
    IMG_INT32 i32ApiSize = gi32ApiEntries - 1;
    IMG_RESULT res = SYSOSKM_CopyToUser(pi32ApiSize, &i32ApiSize, sizeof(*pi32ApiSize));
    IMG_ASSERT(res == IMG_SUCCESS);
    return res;
#endif

	return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function				LOGMAN_GetApiList

******************************************************************************/
IMG_RESULT LOGMAN_GetApiList(
    LOG_sApiInfo __user * psApiInfo
	)
{
#ifdef WINCE
	IMG_ASSERT(0);
#else //Not WINCE
    IMG_INT32  i;
    IMG_UINT32 j;
    LOG_sApiInfo sTempApiInfo;
    LOG_sApiEventInfo sTempEventInfo;

	for(i = 0; i < gi32ApiEntries - 1; i++) {

        /* Copy API info from userspace to kernel (to get pointers to internal structures) */
        if(SYSOSKM_CopyFromUser(&sTempApiInfo, psApiInfo + i, sizeof(LOG_sApiInfo)))
            return IMG_ERROR_GENERIC_FAILURE;

		/* Copy API names from kernel to userspace.  */
        if(SYSOSKM_CopyToUser( (IMG_CHAR __user *) sTempApiInfo.pszApi, asApiInfo[i].pszApi, strlen(asApiInfo[i].pszApi)))
			return IMG_ERROR_GENERIC_FAILURE;

		for(j = 0; j < asApiInfo[i].ui32NoEvents; j++)
		{
            /* Copy API events from userspace to kernel (to get pointers to internal structures) */
            if( SYSOSKM_CopyFromUser(&sTempEventInfo, (LOG_sApiEventInfo __user *) sTempApiInfo.psApiEventInfo + j, sizeof(LOG_sApiEventInfo)))
            {
                return IMG_ERROR_GENERIC_FAILURE;
            }

			/* Copy API events from kernel to userspace.  */
            if(SYSOSKM_CopyToUser( (IMG_CHAR __user *) sTempEventInfo.pszFormatStrings, asApiInfo[i].psApiEventInfo[j].pszFormatStrings, strlen(asApiInfo[i].psApiEventInfo[j].pszFormatStrings)))
				return IMG_ERROR_GENERIC_FAILURE;

            /* Copy another event info (non-pointer) */
            sTempEventInfo.ui32EventId = asApiInfo[i].psApiEventInfo[j].ui32EventId;
            SYSOSKM_CopyToUser( (LOG_sApiEventInfo __user *) sTempApiInfo.psApiEventInfo + j, &sTempEventInfo, sizeof(LOG_sApiEventInfo));
		}

        /* Copy another api info (non-pointer) */
        sTempApiInfo.ui32ApiId = asApiInfo[i].ui32ApiId;
        sTempApiInfo.ui32NoEvents = asApiInfo[i].ui32NoEvents;
        SYSOSKM_CopyToUser(psApiInfo + i, &sTempApiInfo, sizeof(LOG_sApiInfo));
	}
#endif
	return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function				LOGMAN_GetApiEvents

******************************************************************************/
IMG_RESULT LOGMAN_GetApiEvents(
    IMG_INT32 __user * pi32ApiInfo
	)
{
#ifdef WINCE
	IMG_ASSERT(0);
#else //Not WINCE
	IMG_INT32 i;

	for(i = 0; i < gi32ApiEntries - 1; i++)
	{
        IMG_RESULT res = SYSOSKM_CopyToUser(pi32ApiInfo + i, &asApiInfo[i].ui32NoEvents, sizeof(pi32ApiInfo[i]));
        if(res != IMG_SUCCESS)
        {
            IMG_ASSERT(!"failed to copy to user");
            return res;
        }
	}
#endif
	return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function				LOGMAN_GetFormatStrings

******************************************************************************/
IMG_RESULT LOGMAN_GetFormatStrings(
    IMG_CHAR __user * pszString,
    IMG_INT32         i32ApiIndex,
    IMG_INT32         i32Event
	)
{
#ifdef WINCE
	IMG_ASSERT(0);
#else //Not WINCE
	if(SYSOSKM_CopyToUser(
		pszString,
		asApiInfo[i32ApiIndex].psApiEventInfo[i32Event].pszFormatStrings,
		strlen(asApiInfo[i32ApiIndex].psApiEventInfo[i32Event].pszFormatStrings
			)) != IMG_SUCCESS)
	{
		return IMG_ERROR_GENERIC_FAILURE;
	}
#endif
	return IMG_SUCCESS;
}

/*!
******************************************************************************

 @Function				LOGMAN_TestLogEnabled

******************************************************************************/
IMG_RESULT LOGMAN_TestLogEnabled(
    IMG_BOOL __user * pbLogEnabled
	)
{
    IMG_BOOL bLogEnabled = SYSOSVKM_GetBoolEnvVar("pipe_log");
    IMG_RESULT res = SYSOSKM_CopyToUser(pbLogEnabled, &bLogEnabled, sizeof(*pbLogEnabled));
    IMG_ASSERT(res == IMG_SUCCESS);

	return res;
}
