/**
 * linux/drivers/modem_control/mcd_mdm.c
 *
 * Version 1.0
 *
 * This code includes power sequences for IMC 7060 modems and its derivative.
 * That includes :
 *	- XMM6360
 *	- XMM7160
 *	- XMM7260
 * There is no guarantee for other modems.
 *
 * Intel Mobile driver for modem powering.
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Contact: Ranquet Guillaune <guillaumex.ranquet@intel.com>
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

#include <linux/mdm_ctrl.h>

/*****************************************************************************
 *
 * Modem Power/Reset functions
 *
 ****************************************************************************/

int mcd_mdm_init(void *data)
{
	return 0;
}

/**
 *  mcd_mdm_cold_boot - Perform a modem cold boot sequence
 *  @drv: Reference to the driver structure
 *
 *  - Set to HIGH the PWRDWN_N to switch ON the modem
 *  - Set to HIGH the RESET_BB_N
 *  - Do a pulse on ON1
 */
extern void mcd_pm_force_set_host_active(bool value);
#if defined(CONFIG_MIGRATION_CTL_DRV)
int mcd_mdm_cold_boot(void *data, int rst, int pwr_on, int host_act, int pwr_dwn)
#else
int mcd_mdm_cold_boot(void *data, int rst, int pwr_on)
#endif
{
	struct mdm_ctrl_mdm_data *mdm_data = data;

	/* Toggle RESET_PWRDWN_N */
	gpio_set_value(pwr_dwn, 0);
	mdelay(20);
	gpio_set_value(pwr_dwn, 1);

#if defined(CONFIG_ODIN_DWC_USB_HOST)
	odin_dwc_host_set_idle(1);
#endif

	/* Toggle the RESET_BB_N */
	gpio_set_value(rst, 1);

	usleep_range(mdm_data->pre_on_delay, mdm_data->pre_on_delay);

	/* Do a pulse on ON1 */
	gpio_set_value(pwr_on, 1);

	usleep_range(mdm_data->on_duration, mdm_data->on_duration);

	gpio_set_value(pwr_on, 0);

#if defined(CONFIG_MIGRATION_CTL_DRV)
	mcd_pm_force_set_host_active(1);
#endif

	return 0;
}

/**
 *  mdm_ctrl_silent_warm_reset_7x6x - Perform a silent modem warm reset
 *				      sequence
 *  @drv: Reference to the driver structure
 *
 *  - Do a pulse on the RESET_BB_N
 *  - No struct modification
 *  - debug purpose only
 */
int mcd_mdm_warm_reset(void *data, int rst)
{
	struct mdm_ctrl_mdm_data *mdm_data = data;

	gpio_set_value(rst, 0);
	usleep_range(mdm_data->warm_rst_duration, mdm_data->warm_rst_duration);
	gpio_set_value(rst, 1);

	return 0;
}

/**
 *  mcd_mdm_power_off - Perform the modem switch OFF sequence
 *  @drv: Reference to the driver structure
 *
 *  - Set to low the ON1
 *  - Write the PMIC reg
 */
int mcd_mdm_power_off(void *data, int rst)
{
	struct mdm_ctrl_mdm_data *mdm_data = data;

	/* Set the RESET_BB_N to 0 */
	gpio_set_value(rst, 0);

#if defined(CONFIG_ODIN_DWC_USB_HOST)
	odin_dwc_host_set_idle(0);
#endif

	/* Wait before doing the pulse on ON1 */
	usleep_range(mdm_data->pre_pwr_down_delay,
		     mdm_data->pre_pwr_down_delay);

	return 0;
}

int mcd_mdm_get_cflash_delay(void *data)
{
	struct mdm_ctrl_mdm_data *mdm_data;

#if defined(CONFIG_MIGRATION_CTL_DRV)
	mdm_data = data;
#endif
	return mdm_data->pre_cflash_delay;
}

int mcd_mdm_get_wflash_delay(void *data)
{
	struct mdm_ctrl_mdm_data *mdm_data = data;
	return mdm_data->pre_wflash_delay;
}

int mcd_mdm_cleanup(void *data)
{
	return 0;
}
