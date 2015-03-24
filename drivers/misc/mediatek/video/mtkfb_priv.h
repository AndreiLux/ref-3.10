#ifndef __MTKFB_PRIV_H
#define __MTKFB_PRIV_H

#if defined(MTK_OVERLAY_ENGINE_SUPPORT)
#include "disp_ovl_engine_api.h"
#endif


#if defined(MTK_OVERLAY_ENGINE_SUPPORT)
extern DISP_OVL_ENGINE_INSTANCE_HANDLE  mtkfb_instance;
#endif

extern unsigned int isAEEEnabled;


#endif				/* __MTKFB_PRIV_H */
