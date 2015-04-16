/*!
 *****************************************************************************
 *
 * @file	   ump_api.h
 *
 * The User Mode Pipe user mode API.
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

#if !defined (__UMP_API_H__)
#define __UMP_API_H__

#include <img_defs.h>
#include <dman_api_km.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* Clears '__user' in user space but make it visible for doxygen */
#if !defined(IMG_KERNEL_MODULE) && !defined(__RPCCODEGEN__)
    #define __user
#endif

#ifdef  __RPCCODEGEN__
  #define rpc_prefix      UMP
  #define rpc_filename    ump_api
#endif

/*!
******************************************************************************
 This type defines the buffering mode.
******************************************************************************/
typedef enum
{
	UMP_MODE_CYCLIC,			//!< Uses a cyclic buffer in which data may be lost
	UMP_MODE_LOST_LESS,			//!< Continues to allocate buffers to prevent data loss

} UMP_eBufferMode;

/*!
******************************************************************************
 This structure contains the pipe config data.

 @brief		This structure contains the pipe config data.
******************************************************************************/
typedef struct
{
	UMP_eBufferMode			eBufferMode;	//!< Buffer mode
    IMG_UINT32				ui32BufSize;	//!< Size of buffer allocation(s) - in bytes.
											//   should be at least 1 word larger than the largest record
											//   written using UMPKM_WriteRecord()

} UMP_sConfigData;


/*!
******************************************************************************

 @Function				UMP_Initialise
 
 @Description 
 
 This function is used to initialises the user mode pipe component and should 
 be called at start-up.
 
 @Input		None.

 @Return	IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern __DO_NOT_BRIDGE__ IMG_RESULT UMP_Initialise(IMG_VOID);


/*!
******************************************************************************

 @Function				UMP_Deinitialise
 
 @Description 
 
 This function is used to deinitialises the user mode pipe component and 
 would normally be called at shutdown.
 
 @Input		None. 

 @Return	None.

******************************************************************************/
extern __DO_NOT_BRIDGE__ IMG_VOID UMP_Deinitialise(IMG_VOID);


/*!
******************************************************************************

 @Function				UMP_OpenPipe
 
 @Description 
 
 This function is used to open a pipe connect.
 
 @Input		pszPipeName :	The pipe name.
 
 @Output	phPipeHandle :	A pointer used to return the pipe handle.

 @Return    IMG_RESULT :    This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern __DO_NOT_BRIDGE__ IMG_RESULT UMP_OpenPipe(
	IMG_CHAR *					pszPipeName,
	IMG_HANDLE *				phPipeHandle
);


/*!
******************************************************************************

 @Function				UMP_ClosePipe
 
 @Description 
 
 This function is used to close a pipe.

 @Input		hPipeHandle :	The pipe handle returned by UMP_OpenPipe()

 @Return    IMG_RESULT :    This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern __DO_NOT_BRIDGE__ IMG_RESULT UMP_ClosePipe(
	IMG_HANDLE					hPipeHandle
);


/*!
******************************************************************************

 @Function				UMP_FlushPipe
 
 @Description 
 
 This function is used to flush a pipe and discard any in-flight data.

 @Input		hPipeHandle :	The pipe handle returned by UMP_OpenPipe()

 @Return    IMG_RESULT :    This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern __DO_NOT_BRIDGE__ IMG_RESULT UMP_FlushPipe(
	IMG_HANDLE					hPipeHandle
);


/*!
******************************************************************************

 @Function				UMP_PreemptRead
 
 @Description 
 
 This function is used to pre-empt any pending read and cause it to complete.

 @Input		hPipeHandle :	The pipe handle returned by UMP_OpenPipe()

 @Return    IMG_RESULT :    This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern __DO_NOT_BRIDGE__ IMG_RESULT UMP_PreemptRead(
	IMG_HANDLE					hPipeHandle
);


/*!
******************************************************************************

 @Function				UMP_GetConfigData
 
 @Description 
 
 This function is used to obtain the config data for the user mode pipe.

 @Input		hPipeHandle :	The pipe handle returned by UMP_OpenPipe()
 
 @Input		psConfigData :	A pointer to a #UMP_sConfigData structure in which
							the configuration information is returned. 

 @Return	IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern __DO_NOT_BRIDGE__ IMG_RESULT UMP_GetConfigData(
	IMG_HANDLE 					hPipeHandle,
	UMP_sConfigData *			psConfigData
);



/*!
******************************************************************************

 @Function				UMP_ReadBlock
 
 @Description 
 
 This function is used to "read" data from a pipe.

 NOTE: This is normally a synchronous read and will block till data becomes
 available.

 @Input		hPipeHandle :	The pipe handle returned by UMP_OpenPipe()

 @Input		ui32BufferSize : The size of the buffer into which the data is to be read
							NOTE: should be the same as the UMP_sConfigData#ui32BufSize. 
 
 @Output	pvData :		A pointer to a buffer in which the data is returned. 

 @Output	pui32DataRead :	A pointer used to return the amount of data read.

 @Output	pbDataLost :	A pointer used to return IMG_TRUE if data was lost, otherwise
							IMG_FALSE is returned.

 @Return	IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern __DO_NOT_BRIDGE__ IMG_RESULT UMP_ReadBlock(
	IMG_HANDLE  				hPipeHandle,
	IMG_UINT32					ui32BufferSize,
	IMG_VOID *					pvData,
	IMG_UINT32 *				pui32DataRead,
	IMG_BOOL *					pbDataLost
);


/*!
******************************************************************************

 @Function				UMP_ReadRecord
 
 @Description 
 
 This function is used to "read" a "record" from the pipe.

 @Input		hPipeHandle :	The pipe handle returned by UMP_OpenPipe().

 @Output	pui32Size :		A pointer used to return the size of the record.
						    Zero if no record was read.
 
 @Output	ppvData :		A pointer used to return the address of the 
							start of the record.

 @Output	pbDataLost :	A pointer used to return IMG_TRUE if data was lost, otherwise
							IMG_FALSE is returned.

 @Return	IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern __DO_NOT_BRIDGE__ IMG_RESULT UMP_ReadRecord(
	IMG_HANDLE					hPipeHandle,
	IMG_UINT32 *				pui32Size,
	IMG_VOID **					ppvData,
	IMG_BOOL *					pbDataLost
);

#if !defined (DOXYGEN_WILL_SEE_THIS)


/*!
******************************************************************************

 @Function				UMP_GetConfigData1
 
 @Description 
 
 This function is used to obtain the config data for the user mode pipe.

 @Input		ui32ConnId :	The connection Id.
 
 @Input		psConfigData :	A pointer to a #UMP_sConfigData structure in which
							the configuration information is returned. 

 @Return	IMG_RESULT :	This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT UMP_GetConfigData1(
	IMG_UINT32					ui32ConnId,
	UMP_sConfigData __user*		psConfigData
);

/*!
******************************************************************************

 @Function				UMP_ReadBlock1
 
 @Description 
 
 This function is used to "read" data from a pipe.

 NOTE: Because Linux may "interrupt" the wait and require a return to user
 mode (this happens when using gdb to debug use mdoe apps that are using the
 Protablity Framework) then IMG_INTERRUPTED may be return.  The calling app
 should detect this and re-initiate the command.

 @Input		ui32ConnId :	The connection Id returned by DMAN_OpenDevice()

 @Input		ui32BufferSize : The size of the buffer into which the data is to be read
							NOTE: should be the same as the UMP_sConfigData#ui32BufSize. 
 
 @Output	pvData :		A pointer to a buffer in which the data is returned. 

 @Output	pui32DataRead :	A pointer used to return the amount of data read.

 @Output	pbDataLost :	A pointer used to return IMG_TRUE if data was lost, otherwise
							IMG_FALSE is returned.

 @Return	IMG_RESULT:		This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT UMP_ReadBlock1(
	IMG_UINT32  				ui32ConnId,
	IMG_UINT32					ui32BufferSize,
    IMG_VOID __user*			pvData,
    IMG_UINT32 __user *			pui32DataRead,
    IMG_BOOL __user *			pbDataLost
);


/*!
******************************************************************************

 @Function				UMP_FlushPipe1
 
 @Description 
 
 This function is used to flush a pipe and discard any in-flight data.

 @Input		ui32ConnId :	The connection Id returned by DMAN_OpenDevice()

 @Return    IMG_RESULT :    This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT UMP_FlushPipe1(
	IMG_UINT32  				ui32ConnId
);


/*!
******************************************************************************

 @Function				UMP_PreemptRead1
 
 @Description 
 
 This function is used to pre-empt any pending read and cause it to complete.

 @Input		ui32ConnId :	The connection Id returned by DMAN_OpenDevice()

 @Return    IMG_RESULT :    This function returns either IMG_SUCCESS or an
							error code.

******************************************************************************/
extern IMG_RESULT UMP_PreemptRead1(
	IMG_UINT32  				ui32ConnId
);

#endif

#if defined(__cplusplus)
}
#endif
 
#endif /* __UMP_API_H__	*/

