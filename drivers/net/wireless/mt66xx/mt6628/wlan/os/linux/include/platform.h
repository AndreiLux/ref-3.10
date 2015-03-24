/* add platform.h to avoid extern in  c files */

#ifndef _GL_PLATFORM_H
#define _GL_PLATFORM_H

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "gl_typedef.h"

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
extern BOOLEAN fgIsUnderEarlierSuspend;

extern struct semaphore g_halt_sem;
extern int g_u4HaltFlag;

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

extern int board_is_wifi_wakeup_event(void);

void wlanRegisterNotifier(void);
void wlanUnregisterNotifier(void);

#if defined(CONFIG_HAS_EARLYSUSPEND)
int glRegisterEarlySuspend(struct early_suspend *prDesc,
				  early_suspend_callback wlanSuspend,
				  late_resume_callback wlanResume);
int glUnregisterEarlySuspend(struct early_suspend *prDesc);
#endif

typedef int (*set_p2p_mode) (struct net_device *netdev, PARAM_CUSTOM_P2P_SET_STRUC_T p2pmode);
typedef void (*set_dbg_level) (unsigned char modules[DBG_MODULE_NUM]);

void register_set_p2p_mode_handler(set_p2p_mode handler);
void register_set_dbg_level_handler(set_dbg_level handler);
int mtk_wcn_wmt_func_off(ENUM_WMTDRV_TYPE_T type);


int glRegisterPlatformDev(void);
int glUnregisterPlatformDev(void);
int glWlanSetSuspendFlag(void);
int glWlanGetSuspendFlag(void);
int glWlanClearSuspendFlag(void);
int glIndicateWoWPacket(void *data);
int glWlanSetIndicateWoWFlag(void);
#endif					/* _GL_PLATFORM_H */
