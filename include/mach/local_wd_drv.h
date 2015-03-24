#ifndef __L_WD_DRV_H
#define __L_WD_DRV_H
#include <mach/mt_typedefs.h>
#include "wd_api.h"


struct local_wd_driver {
	/* struct device_driver driver; */
	/* const struct platform_device_id *id_table; */
	void (*set_time_out_value) (unsigned int value);
	void (*mode_config) (BOOL dual_mode_en, BOOL irq, BOOL ext_en, BOOL ext_pol, BOOL wdt_en);
	int (*enable) (enum wk_wdt_en en);
	void (*restart) (enum wd_restart_type type);
	void (*dump_reg) (void);
	void (*arch_reset) (char mode);
	int (*swsysret_config) (int bit, int set_value);
};

struct local_wd_driver *get_local_wd_drv(void);


#endif
