/*!
 *****************************************************************************
 *
 * @file	   osa.h
 *
 * OS Abstraction Layer (OSA)
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

#ifndef OSA_H
#define OSA_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "img_defs.h"
#include "img_types.h"
#include "img_errors.h"

#define OSA_NO_TIMEOUT  (0xFFFFFFFF)  /*!< No timeout specified - indefinite wait.  */

/*!
******************************************************************************

 @Function              OSA_pfnThreadFunc

 @Description

 This is the prototype for thread function.

 @Input     pvTaskParam : A pointer client data provided to OSA_ThreadCreate().

 @Return    None.

******************************************************************************/
typedef IMG_VOID (* OSA_pfnThreadFunc) (
    IMG_VOID *  pvTaskParam
);


/*!
******************************************************************************
 This type defines the thread priority level.
 @brief  OSA thread priority level
******************************************************************************/
typedef enum
{
    OSA_THREAD_PRIORITY_LOWEST = 0,    /*!< Lowest priority.        */
    OSA_THREAD_PRIORITY_BELOW_NORMAL,  /*!< Below normal priority.  */
    OSA_THREAD_PRIORITY_NORMAL,        /*!< Normal priority.        */
    OSA_THREAD_PRIORITY_ABOVE_NORMAL,  /*!< Above normal priority.  */
    OSA_THREAD_PRIORITY_HIGHEST        /*!< Highest priority.       */

} OSA_eThreadPriority;


/*!
******************************************************************************
 This type defines the thread-safe init/de-init execution control variable.
 @brief  Thread-safe init/de-init control variable
******************************************************************************/
typedef volatile IMG_UINT32  OSA_ui32ThreadSafeInitControl;


/*!
******************************************************************************
 The initial value of the thread-safe init/de-init control variable.
******************************************************************************/
#define OSA_THREAD_SAFE_INIT_CONTROL_INIT_VAL  0


/*!
******************************************************************************

 @Function              OSA_pfnThreadSafeInitFunc

 @Description

 This is the prototype for the thread safe init function.

 @Input     pvTaskParam : A pointer to the parameter set passed on to the init
                          function.

 @Return    IMG_RESULT  : IMG_SUCCESS if successfully initialised; error otherwise

                          NOTE: In case error is returned by the function and
                          'bOnce' parameter of the calling OSA_ThreadSafeInit()
                          function was set to IMG_TRUE, an attempt to call
                          the corresponding de-init function through OSA_ThreadSafeDeInit()
                          will fail - the de-init function will not be called.

******************************************************************************/
typedef IMG_RESULT (*OSA_pfnThreadSafeInitFunc) (
    IMG_VOID *  pvInitParam
);


/*!
******************************************************************************

 @Function              OSA_pfnThreadSafeDeInitFunc

 @Description

 This is the prototype for the thread safe de-init function.

 @Input     pvTaskParam : A pointer to the parameter set passed on to the de-init
                          function.

 @Return    IMG_RESULT  : IMG_SUCCESS if successfully de-initialised; error otherwise

                          NOTE: In case error is returned by the function and
                          'bOnce' parameter of the calling OSA_ThreadSafeDeInit()
                          function was set to IMG_TRUE, an attempt to call
                          the corresponding init function through OSA_ThreadSafeInit()
                          will fail - the init function will not be called.

******************************************************************************/
typedef IMG_RESULT (*OSA_pfnThreadSafeDeInitFunc) (
    IMG_VOID *  pvDeInitParam
);


#ifdef IMG_MEM_LEAK_TRACK_ENABLED
/*!
******************************************************************************

 @Function              OSA_CritSectCreate_LeakTrack

 @Description

 Creates a critical section object.

 @Input     pszFileName : the source code file where this is called.

 @Input     ui32Line : line in <b. pzFileName </b> where this is called.

 @Output    phCritSect : A pointer used to return the critical section object handle.

 @Return    IMG_RESULT : This function returns either IMG_SUCCESS or an error code.

******************************************************************************/
IMG_RESULT OSA_CritSectCreate_LeakTrack(
    IMG_HANDLE * const  phCritSect,
    const IMG_CHAR * pszFileName,
    IMG_UINT32 ui32Line
);

#define OSA_CritSectCreate(A) OSA_CritSectCreate_LeakTrack(A,__FILE__,__LINE__)

#else
/*!
******************************************************************************

 @Function              OSA_CritSectCreate

 @Description

 Creates a critical section object.

 @Output    phCritSect : A pointer used to return the critical section object handle.

 @Return    IMG_RESULT : This function returns either IMG_SUCCESS or an error code.

******************************************************************************/
IMG_RESULT OSA_CritSectCreate(
    IMG_HANDLE * const  phCritSect
);
#endif

/*!
******************************************************************************

 @Function              OSA_CritSectDestroy

 @Description

 Destroys a critical section object.

 NOTE: The state of any thread waiting on the critical section when it is
 destroyed is undefined.

 @Input     hCritSect : Handle to critical section object.

 @Return    None.

******************************************************************************/
IMG_VOID OSA_CritSectDestroy(
    IMG_HANDLE  hCritSect
);


/*!
******************************************************************************

 @Function              OSA_CritSectUnlock

 @Description

 Releases ownership of the specified critical section object.

 @Input     hCritSect : Handle to critical section object.

 @Return    None.

******************************************************************************/
IMG_VOID OSA_CritSectUnlock(
    IMG_HANDLE  hCritSect
);


/*!
******************************************************************************

 @Function              OSA_CritSectLock

 @Description

 Waits until the critical section object can be acquired.

 NOTE: The state of any thread waiting on the critical section when it is
 destroyed is undefined.

 @Input     hCritSect : Handle to critical section object.

 @Return    None.

******************************************************************************/
IMG_VOID OSA_CritSectLock(
    IMG_HANDLE  hCritSect
);

#ifdef IMG_MEM_LEAK_TRACK_ENABLED
/*!
******************************************************************************

 @Function              OSA_ThreadSyncCreate_LeakTrack

 @Description

 Creates an inter-thread synchronisation object. Initial state is non-signalled.

 @Input     pszFileName : the source code file where this is called.

 @Input     ui32Line : line in <b. pzFileName </b> where this is called.

 @Output    phThreadSync : A pointer used to return the inter-thread synchronisation
                        object handle.

 @Return    IMG_RESULT : This function returns either IMG_SUCCESS or an error code.

******************************************************************************/
IMG_RESULT OSA_ThreadSyncCreate_LeakTrack(
    IMG_HANDLE * const  phThreadSync,
    const IMG_CHAR * pszFileName,
    IMG_UINT32 ui32Line
);
#define OSA_ThreadSyncCreate(a) OSA_ThreadSyncCreate_LeakTrack(a,__FILE__,__LINE__)
#else
/*!
******************************************************************************

 @Function              OSA_ThreadSyncCreate

 @Description

 Creates an inter-thread synchronisation object. Initial state is non-signalled.

 @Output    phThreadSync : A pointer used to return the inter-thread synchronisation
                        object handle.

 @Return    IMG_RESULT : This function returns either IMG_SUCCESS or an error code.

******************************************************************************/
IMG_RESULT OSA_ThreadSyncCreate(
    IMG_HANDLE * const  phThreadSync
);
#endif

/*!
******************************************************************************

 @Function              OSA_ThreadSyncDestroy

 @Description

 Destroys an inter-thread synchronisation object.

 NOTE: The state of any thread waiting on the inter-thread synchronisation object
 when it is destroyed is undefined.

 @Input     hThreadSync : Handle to inter-thread synchronisation object.

 @Return    None.

******************************************************************************/
IMG_VOID OSA_ThreadSyncDestroy(
    IMG_HANDLE  hThreadSync
);


/*!
******************************************************************************

 @Function              OSA_ThreadSyncSignal

 @Description

 Sets the specified inter-thread synchronisation object to the signalled state.
 Each OSA_ThreadSyncSignal() call is counted.

 The inter-thread synchronisation object remains signalled until a number
 of waiting threads, equal to OSA_ThreadSyncSignal() call count, is awaken. After
 that the inter-thread synchronisation object will become non-signalled
 automatically.

 If no threads are waiting, the object's state remains signalled.

 @Input     hThreadSync : Handle to inter-thread synchronisation object.

 @Return    None.

******************************************************************************/
IMG_VOID OSA_ThreadSyncSignal(
    IMG_HANDLE  hThreadSync
);


/*!
******************************************************************************

 @Function              OSA_ThreadSyncWait

 @Description

 Waits until the inter-thread synchronisation object is in the signalled state
 or the time-out interval elapses.

 If the signalled state of the inter-thread synchronisation object causes
 the thread to wake up and there were more than one OSA_ThreadSyncSignal() calls
 preceding the wake up, the inter-thread synchronisation object will remain signalled.
 The number of thread wake ups has to match the number of OSA_ThreadSyncSignal() calls.

 NOTE: The state of any thread waiting on the inter-thread synchronisation object
 when it is destroyed is undefined.

 @Input     hThreadSync      : Handle to inter-thread synchronisation object.

 @Input     ui32TimeoutMs : Timeout in milliseconds or #OSA_NO_TIMEOUT.

 @Return    IMG_RESULT : This function returns IMG_TIMEOUT if the timeout
                         interval is reached, IMG_SUCCESS or an error code.

******************************************************************************/
IMG_RESULT OSA_ThreadSyncWait(
    IMG_HANDLE  hThreadSync,
    IMG_UINT32  ui32TimeoutMs
);

#ifdef IMG_MEM_LEAK_TRACK_ENABLED
/*!
******************************************************************************

 @Function              OSA_ThreadConditionCreate_LeakTrack

 @Description

 Creates an inter-thread coniditional wait object. Initial state is non-signalled.

 @Input     pszFileName : the source code file where this is called.

 @Input     ui32Line : line in <b. pzFileName </b> where this is called.

 @Output    phThreadCondition : A pointer used to return the inter-thread conditional wait
                        object handle.

 @Return    IMG_RESULT : This function returns either IMG_SUCCESS or an error code.

******************************************************************************/
IMG_RESULT OSA_ThreadConditionCreate_LeakTrack(
    IMG_HANDLE * const  phThreadCondition,
    const IMG_CHAR * pszFileName,
    IMG_UINT32 ui32Line
);

#define OSA_ThreadConditionCreate(a) OSA_ThreadConditionCreate_LeakTrack(a,__FILE__,__LINE__)

#else

/*!
******************************************************************************

 @Function              OSA_ThreadConditionCreate

 @Description

 Creates an inter-thread coniditional wait object. Initial state is non-signalled.

 @Output    phThreadCondition : A pointer used to return the inter-thread conditional wait
                        object handle.

 @Return    IMG_RESULT : This function returns either IMG_SUCCESS or an error code.

******************************************************************************/
IMG_RESULT OSA_ThreadConditionCreate(
    IMG_HANDLE * const  phThreadCondition
);
#endif

/*!
******************************************************************************

 @Function              OSA_ThreadConditionDestroy

 @Description

 Destroys an inter-thread conditional wait object.

 NOTE: The state of any thread waiting on the inter-thread synchronisation object
 when it is destroyed is undefined.

 @Input     hThreadCondition : Handle to inter-thread synchronisation object.

 @Return    None.

******************************************************************************/
IMG_VOID OSA_ThreadConditionDestroy(
    IMG_HANDLE  hThreadCondition
);


/*!
******************************************************************************

 @Function              OSA_ThreadConditionSignal

 @Description

 Releases in the internal lock and sets the specified inter-thread synchronisation
 object to the signalled state.

 If no threads are waiting, the object's state remains signalled.

 @Input     hThreadCondition : Handle to inter-thread conditional wait object.

 @Return    None.

******************************************************************************/
IMG_VOID OSA_ThreadConditionSignal(
    IMG_HANDLE  hThreadCondition
);


/*!
******************************************************************************

 @Function              OSA_ThreadConditionWait

 @Description

 Releases the internal lock and waits until the inter-thread conditional
 wait object is in the signalled state or the time-out interval elapses.

 NOTE: The state of any thread waiting on the inter-thread conditional wait object
 when it is destroyed is undefined.

 @Input     hThreadCondition      : Handle to inter-thread conditional wait object.

 @Input     ui32TimeoutMs : Timeout in milliseconds or #OSA_NO_TIMEOUT.

 @Return    IMG_RESULT : This function returns IMG_TIMEOUT if the timeout
                         interval is reached, IMG_SUCCESS or an error code.

******************************************************************************/
IMG_RESULT OSA_ThreadConditionWait(
    IMG_HANDLE  hThreadCondition,
    IMG_UINT32  ui32TimeoutMs
);

/*!
******************************************************************************

 @Function              OSA_ThreadConditionUnlock

 @Description

 Releases ownership of the sync lock.

 @Input     hThreadCondition : Handle to inter-thread conditional wait object.

 @Return    None.

******************************************************************************/
IMG_VOID OSA_ThreadConditionUnlock(
    IMG_HANDLE  hThreadCondition
);


/*!
******************************************************************************

 @Function              OSA_ThreadConditionLock

 @Description

 Waits until the conditional wait critical section object can be acquired.

 @Input     hThreadCondition : Handle to inter-thread conditional wait object.

 @Return    None.

******************************************************************************/
IMG_VOID OSA_ThreadConditionLock(
    IMG_HANDLE  hThreadCondition
);

#ifdef IMG_MEM_LEAK_TRACK_ENABLED
/*!
******************************************************************************

 @Function              OSA_ThreadCreateAndStart_LeakTrack

 @Description

 Creates and starts a thread.

 @Input    pfnThreadFunc   : A pointer to a #OSA_pfnThreadFunc thread function.

 @Input    pvThreadParam   : A pointer to client data passed to the thread function.

 @Input    pszThreadName   : A text string giving the name of the thread.

 @Input    eThreadPriority : The thread priority.

 @Input     pszFileName : the source code file where this is called.

 @Input     ui32Line : line in <b. pzFileName </b> where this is called.

 @Output   phThread        : A pointer used to return the thread handle.

 @Return   IMG_RESULT : This function returns either IMG_SUCCESS or an error code.

******************************************************************************/
IMG_RESULT OSA_ThreadCreateAndStart_LeakTrack(
    OSA_pfnThreadFunc       pfnThreadFunc,
    IMG_VOID *              pvThreadParam,
    const IMG_CHAR * const  pszThreadName,
    OSA_eThreadPriority     eThreadPriority,
    IMG_HANDLE * const      phThread,
    const IMG_CHAR * pszFileName,
    IMG_UINT32 ui32Line
);
#define OSA_ThreadCreateAndStart(a,b,c,d,e) OSA_ThreadCreateAndStart_LeakTrack(a,b,c,d,e,__FILE__,__LINE__)
#else
/*!
******************************************************************************

 @Function              OSA_ThreadCreateAndStart

 @Description

 Creates and starts a thread.

 @Input    pfnThreadFunc   : A pointer to a #OSA_pfnThreadFunc thread function.

 @Input    pvThreadParam   : A pointer to client data passed to the thread function.

 @Input    pszThreadName   : A text string giving the name of the thread.

 @Input    eThreadPriority : The thread priority.

 @Output   phThread        : A pointer used to return the thread handle.

 @Return   IMG_RESULT : This function returns either IMG_SUCCESS or an error code.

******************************************************************************/
IMG_RESULT OSA_ThreadCreateAndStart(
    OSA_pfnThreadFunc       pfnThreadFunc,
    IMG_VOID *              pvThreadParam,
    const IMG_CHAR * const  pszThreadName,
    OSA_eThreadPriority     eThreadPriority,
    IMG_HANDLE * const      phThread
);
#endif

/*!
******************************************************************************

 @Function              OSA_ThreadWaitExitAndDestroy

 @Description

 Waits for a thread to exit and to destroy the thread object allocated
 by the OSA layer.

 @Input     hThread : Handle to thread.

 @Return    None.

******************************************************************************/
IMG_VOID OSA_ThreadWaitExitAndDestroy(
    IMG_HANDLE  hThread
);


/*!
******************************************************************************

 @Function              OSA_ThreadSleep

 @Description

 Cause the calling thread to sleep for a period of time.

 NOTE: A ui32SleepMs of 0 relinquish the time-slice of current thread.

 @Input     ui32SleepMs : Sleep period in milliseconds.

 @Return    None.

******************************************************************************/
IMG_VOID OSA_ThreadSleep(
    IMG_UINT32  ui32SleepMs
);


/*!
******************************************************************************

 @Function                OSA_ThreadLocalDataSet

 @Description

 Store data in Thread Local Storage (data specific to calling thread).

 @Input     pData : Pointer to data to be set.

 @Return    IMG_RESULT : This function returns either IMG_SUCCESS or an error code.

******************************************************************************/
IMG_RESULT OSA_ThreadLocalDataSet(
    IMG_PVOID  pData
);


/*!
******************************************************************************

 @Function                OSA_ThreadLocalDataGet

 @Description

 Retrieve data from Thread Local Storage (data specific to calling thread).

 @Input     ppData : Pointer to pointer that will store retrieved data or NULL
                     if function fails

 @Return    IMG_RESULT : This function returns either IMG_SUCCESS or an error code.

******************************************************************************/
IMG_RESULT OSA_ThreadLocalDataGet(
    IMG_PVOID *  ppData
);


/*!
******************************************************************************

 @Function                OSA_ThreadSafeInit

 @Description

 Allows for thread-safe execution of the provided init function.

 @Input     pui32InitControl : Pointer to the execution control variable.
                               The variable should be global and should be shared
                               with the corresponding OSA_ThreadSafeDeInit() call.

 @Input     pfnThreadSafeInitFunc : The pointer to the init function to be called.

 @Input     pvInitParam      : The parameter to be passed on to the init function.

 @Input     bOnce            : If set to IMG_TRUE, the init function will be called
                               only once. Any other calls OSA_ThreadSafeInit() calls
                               will not call the init function until the corresponding
                               de-init function will be called successfully by
                               OSA_ThreadSafeDeInit().
                               If set to IMG_FALSE, the init function will be called
                               every time OSA_ThreadSafeInit() is called.

                               NOTE: The value of this parameter must correspond
                               to the value used in corresponding OSA_ThreadSafeDeInit()
                               calls.

 @Return    IMG_RESULT : This function returns either IMG_SUCCESS or an error code.

******************************************************************************/
IMG_RESULT OSA_ThreadSafeInit(
    OSA_ui32ThreadSafeInitControl *  pui32InitControl,
    OSA_pfnThreadSafeInitFunc        pfnThreadSafeInitFunc,
    IMG_VOID *                       pvInitParam,
    IMG_BOOL                         bOnce
);


/*!
******************************************************************************

 @Function                OSA_ThreadSafeDeInit

 @Description

 Allows for thread-safe execution of the provided de-init function.

 @Input     pui32InitControl : Pointer to the execution control variable.
                               The variable should be global and should be shared
                               with the corresponding OSA_ThreadSafeInit() call.

 @Input     pfnThreadSafeInitFunc : The pointer to the de-init function to be called.

 @Input     pvInitParam      : The parameter to be passed on to the de-init function.

 @Input     bOnce            : If set to IMG_TRUE, the de-init function will be called
                               only once. Any other calls OSA_ThreadSafeDeInit() calls
                               will not call the de-init function until the corresponding
                               init function will be called successfully by
                               OSA_ThreadSafeInit().
                               If set to IMG_FALSE, the de-init function will be called
                               every time OSA_ThreadSafeDeInit() is called.

                               NOTE: The value of this parameter must correspond
                               to the value used in corresponding OSA_ThreadSafeInit()
                               calls.

 @Return    IMG_RESULT : This function returns either IMG_SUCCESS or an error code.

******************************************************************************/
IMG_RESULT OSA_ThreadSafeDeInit(
    OSA_ui32ThreadSafeInitControl *  pui32DeInitControl,
    OSA_pfnThreadSafeDeInitFunc      pfnThreadSafeDeInitFunc,
    IMG_VOID *                       pvDeInitParam,
    IMG_BOOL                         bOnce
);

#if defined(__cplusplus)
}
#endif


#endif /* OSA_H */




