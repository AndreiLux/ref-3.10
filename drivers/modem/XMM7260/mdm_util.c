/*
 * linux/drivers/modem_control/mdm_util.c
 *
 * Version 1.0
 *
 * Utilities for modem control driver.
 *
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Contact: Faouaz Tenoutit <faouazx.tenoutit@intel.com>
 *          Frederic Berat <fredericx.berat@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include "mdm_util.h"
#include "mcd_mdm.h"
#include "mcd_cpu.h"
#include "mcd_pmic.h"
#include "mcd_pm.h"
#include <linux/mdm_ctrl_board.h>


struct mdm_ctrl *mdm_drv;


/**
 *  mdm_ctrl_enable_flashing - Set the modem state to FW_DOWNLOAD_READY
 *
 */
void mdm_ctrl_enable_flashing(unsigned long int param)
{
	struct mdm_ctrl *drv = (struct mdm_ctrl *)param;

	printk("[MDM] %s  <=====\n", __func__);
	del_timer(&drv->flashing_timer);
	if (mdm_ctrl_get_state(drv) != MDM_CTRL_STATE_IPC_READY) {
		printk("[MDM] MDM_CTRL_STATE_FW_DOWNLOAD_READY\n");
		// set host active gpio ......
		mdm_ctrl_set_state(drv, MDM_CTRL_STATE_FW_DOWNLOAD_READY);
	}
}

/**
 *  mdm_ctrl_launch_timer - Timer launcher helper
 *  @timer: Timer to activate
 *  @delay: Timer duration
 *  @timer_type: Timer type
 *
 *  Type can be MDM_TIMER_FLASH_ENABLE.
 *  Note: Type MDM_TIMER_FLASH_DISABLE is not used anymore.
 */
#if defined(CONFIG_MIGRATION_CTL_DRV)
void mdm_ctrl_launch_timer(struct timer_list *timer, int delay,
			   unsigned int timer_type, struct mdm_ctrl *mdm_drv)
#else
void mdm_ctrl_launch_timer(struct timer_list *timer, int delay,
			   unsigned int timer_type)
#endif
{
	timer->data = (unsigned long int)mdm_drv;
	switch (timer_type) {
	case MDM_TIMER_FLASH_ENABLE:
		timer->function = mdm_ctrl_enable_flashing;
		break;
	case MDM_TIMER_FLASH_DISABLE:
	default:
		pr_err(DRVNAME ": Unrecognized timer type %d", timer_type);
		del_timer(timer);
		return;
		break;
	}
	mod_timer(timer, jiffies + msecs_to_jiffies(delay));
}

/**
 *  mdm_ctrl_set_func - Set modem sequences functions to use
 *  @drv: Reference to the driver structure
 *
 */
void mdm_ctrl_set_func(struct mdm_ctrl *drv)
{
	int modem_type = 0;
	int cpu_type = 0;
	int pmic_type = 0;
#if defined(CONFIG_MIGRATION_CTL_DRV)
	int pm_type = 0;
#endif

	modem_type = drv->pdata->mdm_ver;
	cpu_type = drv->pdata->cpu_ver;
	pmic_type = drv->pdata->pmic_ver;
#if defined(CONFIG_MIGRATION_CTL_DRV)
	pm_type = drv->pdata->pm_ver;
#endif

	switch (modem_type) {
	case MODEM_6260:
	case MODEM_6268:
	case MODEM_6360:
	case MODEM_7160:
	case MODEM_7260:
		drv->pdata->mdm.init = mcd_mdm_init;
		drv->pdata->mdm.power_on = mcd_mdm_cold_boot;
		drv->pdata->mdm.warm_reset = mcd_mdm_warm_reset;
		drv->pdata->mdm.power_off = mcd_mdm_power_off;
		drv->pdata->mdm.cleanup = mcd_mdm_cleanup;
		drv->pdata->mdm.get_wflash_delay = mcd_mdm_get_wflash_delay;
		drv->pdata->mdm.get_cflash_delay = mcd_mdm_get_cflash_delay;
		break;
	default:
		pr_info(DRVNAME ": Can't retrieve modem specific functions");
		drv->is_mdm_ctrl_disabled = true;
		break;
	}

	switch (cpu_type) {
	case CPU_PWELL:
	case CPU_CLVIEW:
	case CPU_TANGIER:
	case CPU_VVIEW2:
	case CPU_ANNIEDALE:
		drv->pdata->cpu.init = cpu_init_gpio;
		drv->pdata->cpu.cleanup = cpu_cleanup_gpio;
		drv->pdata->cpu.get_mdm_state = get_gpio_mdm_state;
		drv->pdata->cpu.get_irq_cdump = get_gpio_irq_cdump;
		drv->pdata->cpu.get_irq_rst = get_gpio_irq_rst;
		drv->pdata->cpu.get_gpio_rst = get_gpio_rst;
		drv->pdata->cpu.get_gpio_pwr = get_gpio_pwr;
		break;

#if defined(CONFIG_MIGRATION_CTL_DRV)
	case CPU_ODIN:
		drv->pdata->cpu.init = cpu_init_gpio;
		drv->pdata->cpu.cleanup = cpu_cleanup_gpio;
		drv->pdata->cpu.get_mdm_state = get_gpio_mdm_state;
		drv->pdata->cpu.get_irq_cdump = get_gpio_irq_cdump;
		drv->pdata->cpu.get_irq_rst = get_gpio_irq_rst;
		drv->pdata->cpu.get_gpio_rst = get_gpio_rst;
		drv->pdata->cpu.get_gpio_pwr = get_gpio_pwr;
		drv->pdata->cpu.get_gpio_ap_wakeup = get_gpio_ap_wakeup;
		drv->pdata->cpu.get_irq_wakeup = get_irq_wakeup;
		drv->pdata->cpu.get_gpio_host_active = get_gpio_host_active;
		drv->pdata->cpu.get_gpio_pwr_down = get_gpio_pwr_down;
		break;
#endif

	default:
		pr_info(DRVNAME ": Can't retrieve cpu specific functions");
		drv->is_mdm_ctrl_disabled = true;
		break;
	}

	switch (pmic_type) {
	case PMIC_MFLD:
	case PMIC_MRFL:
	case PMIC_BYT:
	case PMIC_MOOR:
		drv->pdata->pmic.init = pmic_io_init;
		drv->pdata->pmic.power_on_mdm = pmic_io_power_on_mdm;
		drv->pdata->pmic.power_off_mdm = pmic_io_power_off_mdm;
		drv->pdata->pmic.cleanup = pmic_io_cleanup;
		drv->pdata->pmic.get_early_pwr_on = pmic_io_get_early_pwr_on;
		drv->pdata->pmic.get_early_pwr_off = pmic_io_get_early_pwr_off;
		break;
	case PMIC_CLVT:
		drv->pdata->pmic.init = pmic_io_init;
		drv->pdata->pmic.power_on_mdm = pmic_io_power_on_ctp_mdm;
		drv->pdata->pmic.power_off_mdm = pmic_io_power_off_mdm;
		drv->pdata->pmic.cleanup = pmic_io_cleanup;
		drv->pdata->pmic.get_early_pwr_on = pmic_io_get_early_pwr_on;
		drv->pdata->pmic.get_early_pwr_off = pmic_io_get_early_pwr_off;
		break;
#if defined(CONFIG_MIGRATION_CTL_DRV)
	case PMIC_ODIN:
		drv->pdata->pmic.init = pmic_io_init;
		drv->pdata->pmic.power_on_mdm = pmic_io_power_on_odin_mdm;
		drv->pdata->pmic.power_off_mdm = pmic_io_power_off_odin_mdm;
		drv->pdata->pmic.cleanup = pmic_io_cleanup;
		drv->pdata->pmic.get_early_pwr_on = pmic_io_get_early_pwr_on;
		drv->pdata->pmic.get_early_pwr_off = pmic_io_get_early_pwr_off;
		pr_info(DRVNAME ": %s for odin pmic\n", __func__);
		break;
#endif
	default:
		pr_info(DRVNAME ": Can't retrieve pmic specific functions");
		drv->is_mdm_ctrl_disabled = true;
		break;
	}

#if defined(CONFIG_MIGRATION_CTL_DRV)
	switch (pm_type) {
	case PM_FLASHED:
		/* TODO : Need to implement flashed case */
		break;

	case PM_FLASHELESS:
		pr_info(DRVNAME ": Set func for PM.\n");
		drv->pdata->pm.init = mcd_pm_driver_init;
		drv->pdata->pm.remove = mcd_pm_driver_remove;
		drv->pdata->pm.shutdown = mcd_pm_driver_shutdown;
		drv->pdata->pm.ap_wakeup_handler = mcd_pm_host_wakeup_irq;
		break;
	}
#endif
}


/**
 *  modem_ctrl_create_pdata - Create platform data
 *
 *  pdata is created base on information given by platform.
 *  Data used is the modem type, the cpu type and the pmic type.
 */
struct mcd_base_info *modem_ctrl_get_dev_data(struct platform_device *pdev)
{
	struct mcd_base_info *info = NULL;

	if (!pdev->dev.platform_data) {
		pr_info(DRVNAME
			"%s: No platform data available, checking ACPI...\n",
			__func__);

		if (retrieve_modem_platform_data(pdev)) {
			pr_err(DRVNAME
				   "%s: No registered info found. Disabling driver.",
				   __func__);
			return NULL;
		}
	}

	info = pdev->dev.platform_data;

	pr_err(DRVNAME ": cpu: %d mdm: %d pmic: %d.\n",
			info->cpu_ver, info->mdm_ver, info->pmic_ver);
	if ((info->mdm_ver == MODEM_UNSUP)
	    || (info->cpu_ver == CPU_UNSUP)
	    || (info->pmic_ver == PMIC_UNSUP)) {
		/* mdm_ctrl is disabled as some components */
		/* of the platform are not supported */
		pr_err(DRVNAME ": Don't support platform version.\n");
		kfree(info);
		return NULL;
	}

	return info;
}

/**
 *  mdm_ctrl_get_device_info - Create platform and modem data.
 *  @drv: Reference to the driver structure
 *
 *  Platform are build from SFI table data.
 */
void mdm_ctrl_get_device_info(struct mdm_ctrl *drv,
			      struct platform_device *pdev)
{
	drv->pdata = modem_ctrl_get_dev_data(pdev);

	if (!drv->pdata) {
		drv->is_mdm_ctrl_disabled = true;
		pr_info(DRVNAME ": Disabling driver. No known device.");
		goto out;
	}

	mdm_ctrl_set_func(drv);
 out:
	return;
}
