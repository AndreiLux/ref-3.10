#ifndef __DISP_OVL_ENGINE_SW_H__
#define __DISP_OVL_ENGINE_SW_H__

#include "disp_ovl_engine_core.h"

/* Ovl_Engine SW */
/* #define DISP_OVL_ENGINE_SW_SUPPORT */

#ifdef DISP_OVL_ENGINE_SW_SUPPORT
void disp_ovl_engine_sw_init(void);
void disp_ovl_engine_sw_set_params(DISP_OVL_ENGINE_INSTANCE *params);
void disp_ovl_engine_trigger_sw_overlay(void);
void disp_ovl_engine_sw_register_irq(void (*irq_callback) (unsigned int param));
#endif

#endif
