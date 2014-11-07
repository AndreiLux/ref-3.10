#ifndef __LCD_SLIMPORT_SAMPLE_H__
#define __LCD_SLIMPORT_SAMPLE_H__

#include <video/odindss.h>

#include "../dss/dss.h"
#include "../dss/mipi-dsi.h"

enum {
	PANEL_LGHDK,
};

extern int slimport_panel_on(enum odin_mipi_dsi_index resource_index,
			     struct odin_dss_device *dssdev);
extern int slimport_panel_off(enum odin_mipi_dsi_index resource_index,
			      struct odin_dss_device *dssdev);
extern void slimport_panel_config(struct odin_dss_device *dssdev);
extern int slimport_panel_init(void);

#endif /* !__LCD_SLIMPORT_SAMPLE_H__ */
