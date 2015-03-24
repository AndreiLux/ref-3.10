
/*****************************************************************************
 *
 * Filename:
 * ---------
 *   mtk_mfgdvfs.h
 *
 * Project:
 * --------
 *   MT8135
 *
 * Description:
 * ------------
 *   Mtk RGX GPU dvfs definition and declaration for mtk_mfgdvfs.c file
 *
 * Author:
 * -------
 *   Enzhu Wang
 *
 *============================================================================
 * History and modification
 *1. Initial @ July 9th 2013
 *============================================================================
 ****************************************************************************/
#include "servicesext.h"


#ifndef MTK_MFGDVFS_H
#define MTK_MFGDVFS_H


typedef struct _MTK_DVFS_T
{
	IMG_HANDLE	hMtkDVFSTimer;
	IMG_HANDLE	hMtkDVFSThread;
} MTK_DVFS_T;


/* be called by  mtk_mfgsys.c */
extern IMG_INT32 MTKSetFreqInfo(FREQ_TABLE_DVFS_TYPE TblType);
extern PVRSRV_ERROR MtkDVFSInit(IMG_VOID);
extern PVRSRV_ERROR MtkDVFSDeInit(IMG_VOID);

#endif // MTK_MFGDVFS_H

















