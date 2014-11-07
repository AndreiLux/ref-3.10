/*
 * arch/arm/mach-tegra/baseband-xmm-power.h
 *
 * Copyright (C) 2011 NVIDIA Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef BASEBAND_XMM_POWER_H
#define BASEBAND_XMM_POWER_H

#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/mdm_ctrl.h>

#if defined(ODIN_DWC_HOST_SET_CPU_AFFINITY)
#include <linux/odin_cpuidle.h>

#define MCD_HOST_WAKEUP_CPU_AFFINITY_NUM	(2)
#endif

#define VENDOR_ID         0x1519
#define PRODUCT_ID        0x0452

#define TEGRA_EHCI_DEVICE "/sys/devices/platform/tegra-ehci.1/ehci_power"
#define XMM_ONOFF_PATH "/sys/devices/platform/baseband_xmm_power/xmm_onoff"

#define XMM_MODEM_VER_1121	0x1121
#define XMM_MODEM_VER_1130	0x1130
#define XMM_MODEM_VER_1145	0x1145

#define ENUM_REPEAT_TRY_CNT 3
#define MODEM_ENUM_TIMEOUT_500MS 16 /* 8 sec */
#define MODEM_ENUM_TIMEOUT_200MS 25 /* 5 sec */
#define DEFAULT_AUTOSUSPEND_DELAY 	(2000)
#define DISABLE_AUTOSUSPEND_DELAY 	(-1)

/* In case resume from L3 state,
 * AP should wait a response of the host wakeup from CP after slave_wakeup set
 * to high */
#define WAIT_HOSTWAKEUP_TIMEOUT	500 /* 100ms */

/* shared between baseband-xmm-* modules so they can agree on same
 * modem configuration
 */
extern unsigned long modem_ver;
extern unsigned long modem_flash;
extern unsigned long modem_pm;

#if defined(CONFIG_MIGRATION_CTL_DRV)
/* Don't need power control in PM driver. It's controlled from mcd_ctrl. */
#else
/* TODO : Need to remove after discuss with RIL team */
void baseband_xmm_power_switch(bool power_on); //To_Ril-recovery Nvidia_Patch_20111226
#endif

#if defined(CONFIG_MIGRATION_CTL_DRV)
extern const struct dev_pm_ops mcd_pm_dev_pm_ops;

int mcd_pm_driver_init(struct platform_device *device);
int mcd_pm_driver_remove(struct platform_device *device);
int mcd_pm_driver_shutdown(struct platform_device *device);
irqreturn_t mcd_pm_host_wakeup_irq(int irq, void *dev_id);
#endif

#endif	//__BASEBAND_XMM_POWER_H__
