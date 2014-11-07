#ifndef __PANEL_DEVICE_H
#define __PANEL_DEVICE_H

#include <video/odindss.h>

extern struct odin_dss_device hdmi_device;
extern struct odin_dss_device mem2mem_device;

#if defined (CONFIG_PANEL_LGLH500WF1)
extern struct odin_dss_device dsi_lglh500wf1_device;
#endif

#if defined (CONFIG_PANEL_LMS480)
extern struct odin_dss_device lms480_device;
#endif

#if defined (CONFIG_SLIMPORT_ANX7805)
extern struct odin_dss_device dsi_slimport_device;
#endif

#if defined (CONFIG_SLIMPORT_ANX7808)  || defined (CONFIG_SLIMPORT_ANX7812)
#if 0
extern struct odin_dss_device hdmi_slimport_device;
#else
extern struct odin_dss_device hdmi_panel_device;
#endif
#endif


#if defined (CONFIG_PANEL_LGLH520WF1)
extern struct odin_dss_device dsi_lglh520wf1_device;
#endif

/*
              
                                
                                 
*/
#if defined (CONFIG_PANEL_LGLH600WF2)
extern struct odin_dss_device dsi_lglh600wf2_device;
#endif
/*              */

#endif
