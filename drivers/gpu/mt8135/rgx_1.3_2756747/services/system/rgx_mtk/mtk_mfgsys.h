
/*****************************************************************************
 *
 * Filename:
 * ---------
 *   mtk_mfgsys.h
 *
 * Project:
 * --------
 *   MT8135
 *
 * Description:
 * ------------
 *   Mtk RGX GPU system definition and declaration for mtk_mfgsys.c and mtk_mfgdvfs.c file
 *
 * Author:
 * -------
 *   Enzhu Wang
 *
 *============================================================================
 * History and modification
 *1. Initial @ April 27th 2013
 *============================================================================
 ****************************************************************************/
#include "servicesext.h"
#include "rgxdevice.h"


#ifndef MTK_MFGSYS_H
#define MTK_MFGSYS_H

/* control APM is enabled or not  */
#define MTK_PM_SUPPORT 1  

/* control RD is enabled or not */
/* RD_POWER_ISLAND only enable with E2 IC, disalbe in E1 IC @2013/9/17 */
//#define RD_POWER_ISLAND 1 

/*  unit ms, timeout interval for DVFS detection */
#define MTK_DVFS_SWITCH_INTERVAL  300 

/*  need idle device before switching DVFS  */
#define MTK_DVFS_IDLE_DEVICE  0       

/* used created thread to handle DVFS switch or not */
#define MTK_DVFS_CREATE_THREAD  0       


#define ENABLE_MTK_MFG_DEBUG 0


#if ENABLE_MTK_MFG_DEBUG
#define mtk_mfg_debug(fmt, args...)     printk("[MFG]" fmt, ##args)
#else
#define mtk_mfg_debug(fmt, args...) 
#endif


typedef enum _FREQ_TABLE_TYPE_
{
    TBL_TYPE_DVFS_NOT_SUPPORT =0x0, /* means use init freq and volt always, not support DVFS */
    TBL_TYPE_DVFS_2_LEVEL =0x1,     /* two level to switch */
    TBL_TYPE_DVFS_3_LEVEL =0x2,     /* three level to switch */
    TBL_TYPE_DVFS_4_LEVEL =0x3,     /* four level to switch */
    TBL_TYPE_DVFS_MAX =0xf,
} FREQ_TABLE_DVFS_TYPE;



//extern to be used by PVRCore_Init in RGX DDK module.c 
extern int MTKMFGSystemInit(void);
extern int MTKMFGSystemDeInit(void);
extern bool MTKMFGIsE2andAboveVersion(void);


/* below register interface in RGX sysconfig.c */
extern PVRSRV_ERROR MTKSysDevPrePowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
                                         PVRSRV_DEV_POWER_STATE eCurrentPowerState,
									     IMG_BOOL bForced);
extern PVRSRV_ERROR MTKSysDevPostPowerState(PVRSRV_DEV_POWER_STATE eNewPowerState,
                                          PVRSRV_DEV_POWER_STATE eCurrentPowerState,
									      IMG_BOOL bForced);
extern PVRSRV_ERROR MTKSystemPrePowerState(PVRSRV_SYS_POWER_STATE eNewPowerState);

extern PVRSRV_ERROR MTKSystemPostPowerState(PVRSRV_SYS_POWER_STATE eNewPowerState);



#endif // MTK_MFGSYS_H

















