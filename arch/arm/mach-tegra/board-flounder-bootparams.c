/*
 * linux/arch/arm/mach-tegra/board-flounder-bootparams.c
 *
 * Copyright (C) 2014 HTC Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/platform_device.h>
#include <mach/board_htc.h>
#include <linux/of.h>

static int mfg_mode;
int __init board_mfg_mode_initialize(char *s)
{
	if (!strcmp(s, "normal"))
		mfg_mode = BOARD_MFG_MODE_NORMAL;
	else if (!strcmp(s, "factory2"))
		mfg_mode = BOARD_MFG_MODE_FACTORY2;
	else if (!strcmp(s, "recovery"))
		mfg_mode = BOARD_MFG_MODE_RECOVERY;
	else if (!strcmp(s, "charge"))
		mfg_mode = BOARD_MFG_MODE_CHARGE;
	else if (!strcmp(s, "power_test"))
		mfg_mode = BOARD_MFG_MODE_POWERTEST;
	else if (!strcmp(s, "mfgkernel"))
		mfg_mode = BOARD_MFG_MODE_MFGKERNEL;
	else if (!strcmp(s, "modem_calibration"))
		mfg_mode = BOARD_MFG_MODE_MODEM_CALIBRATION;

	return 1;
}

int board_mfg_mode(void)
{
	return mfg_mode;
}
EXPORT_SYMBOL(board_mfg_mode);

__setup("androidboot.mode=", board_mfg_mode_initialize);

static unsigned long radio_flag = 0;
int __init radio_flag_init(char *s)
{
	if (strict_strtoul(s, 16, &radio_flag))
		return -EINVAL;
	else
		return 1;
}
__setup("radioflag=", radio_flag_init);

unsigned int get_radio_flag(void)
{
	return radio_flag;
}
EXPORT_SYMBOL(get_radio_flag);

static int mdm_sku;
static int get_modem_sku(void)
{
	int err, pid;

	err = of_property_read_u32(
		of_find_node_by_path("/chosen/board_info"),
		"pid", &pid);
	if (err) {
		pr_err("%s: can't read pid from DT, error = %d (set to default sku)\n", __func__, err);
		mdm_sku = MDM_SKU_WIFI_ONLY;
	}

	switch(pid)
	{
		case 302: //#F sku
			mdm_sku = MDM_SKU_WIFI_ONLY;
			break;

		case 303: //#UL sku
			mdm_sku = MDM_SKU_UL;
			break;

		case 304: //#WL sku
			mdm_sku = MDM_SKU_WL;
			break;

		default:
			mdm_sku = MDM_SKU_WIFI_ONLY;
			break;
	}

	return mdm_sku;
}

static unsigned int radio_image_status = 0;
static unsigned int __init set_radio_image_status(char *read_mdm_version)
{
	/* TODO: Try to read baseband version */
	if (strlen(read_mdm_version)) {
		if (strncmp(read_mdm_version,"T1.xx", 3) == 0)
			radio_image_status = RADIO_IMG_EXIST;
		else
			radio_image_status = RADIO_IMG_NON_EXIST;
	} else
		radio_image_status = RADIO_IMG_NON_EXIST;

	return 1;
}
__setup("androidboot.baseband=", set_radio_image_status);

int get_radio_image_status(void)
{
	return radio_image_status;
}
EXPORT_SYMBOL(get_radio_image_status);

bool is_mdm_modem()
{
	bool ret = false;
	int mdm_modem = get_modem_sku();

	if ((mdm_modem == MDM_SKU_UL) || (mdm_modem == MDM_SKU_WL))
		ret = true;

	return ret;
}
EXPORT_SYMBOL(is_mdm_modem);
