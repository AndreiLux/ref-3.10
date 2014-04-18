/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SUBSYS_RESTART_H
#define __SUBSYS_RESTART_H

#include <linux/spinlock.h>
#ifdef CONFIG_QCT_9K_MODEM
#include <linux/platform_data/qcom_usb_modem_power.h>
#endif

#define SUBSYS_NAME_MAX_LENGTH 40

enum {
	RESET_SOC = 1,
	RESET_SUBSYS_COUPLED,
	RESET_SUBSYS_INDEPENDENT,
	RESET_SUBSYS_MIXED = 25,
	RESET_LEVEL_MAX
};

struct subsys_data {
	const char *name;
	int (*shutdown) (const struct subsys_data *);
	int (*powerup) (const struct subsys_data *);
	void (*crash_shutdown) (const struct subsys_data *);
	int (*ramdump) (int, const struct subsys_data *);

	/* Internal use only */
	struct list_head list;
	void *notif_handle;

	struct mutex shutdown_lock;
	struct mutex powerup_lock;

	void *restart_order;
	struct subsys_data *single_restart_list[1];

	struct qcom_usb_modem *modem;
};

#if defined(CONFIG_MSM_SUBSYSTEM_RESTART)

int get_restart_level(void);
int subsystem_restart(const char *subsys_name);
int ssr_register_subsystem(struct subsys_data *subsys);

#ifdef CONFIG_QCT_9K_MODEM
#ifdef CONFIG_MDM_ERRMSG
char *get_mdm_errmsg(struct qcom_usb_modem *modem);
#endif
#endif

#else

static inline int get_restart_level(void)
{
	return 0;
}

static inline int subsystem_restart(const char *subsystem_name)
{
	return 0;
}

static inline int ssr_register_subsystem(struct subsys_data *subsys)
{
	return 0;
}

#endif /* CONFIG_MSM_SUBSYSTEM_RESTART */

#endif
