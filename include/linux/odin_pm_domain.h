/* arch/arm/mach-odin/include/mach/odin_pm_domain.h
 *
 * ODIN PM domain controller
 *
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef __MACH_ODIN_PM_DOMAIN_H
#define __MACH_ODIN_PM_DOMAIN_H

/* FIXME: move to mach-odin/include/mach */

#include <linux/platform_device.h>
#include <linux/pm_domain.h>

struct odin_pm_domain {
	struct generic_pm_domain gpd;
	bool is_off;
	unsigned int mbox_cmd_id;
	u32 pdid;
};

#ifdef CONFIG_AUD_RPM
extern struct odin_pm_domain odin_pd_aud2_i2c;
extern struct odin_pm_domain odin_pd_aud2_si2s0;
extern struct odin_pm_domain odin_pd_aud2_si2s1;
extern struct odin_pm_domain odin_pd_aud2_si2s2;
extern struct odin_pm_domain odin_pd_aud2_si2s3;
extern struct odin_pm_domain odin_pd_aud2_mi2s;
#endif

extern struct odin_pm_domain odin_pd_vsp1;
extern struct odin_pm_domain odin_pd_vsp2;
extern struct odin_pm_domain odin_pd_vsp3;
extern struct odin_pm_domain odin_pd_vsp4_jpeg;
extern struct odin_pm_domain odin_pd_vsp4_png;

extern struct odin_pm_domain odin_pd_dss2;
extern struct odin_pm_domain odin_pd_dss3;
extern struct odin_pm_domain odin_pd_dss4;

/* temp fix for HDMI begin */
extern struct odin_pm_domain odin_pd_dss3_lcd0_panel;
extern struct odin_pm_domain odin_pd_dss3_lcd1_panel;
extern struct odin_pm_domain odin_pd_dss3_hdmi_panel;
/* temp fix for HDMI end */

extern int odin_pd_register_dev(struct device *dev,
	struct odin_pm_domain *pd);
extern int odin_pd_power(struct odin_pm_domain *pd, bool on);

/* FIXME: will be deprecated */
#if 0
extern int odin_pd_disable_dss(void);
extern int odin_pd_enable_dss(void);
#endif

#endif /* __MACH_ODIN_PM_DOMAIN_H */
