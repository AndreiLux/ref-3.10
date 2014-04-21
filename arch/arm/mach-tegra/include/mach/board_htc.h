/*
 * linux/arch/arm/mach-tegra/include/mach/board_htc.h
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
#ifndef __ARCH_ARM_MACH_TEGRA_BOARD_HTC_H
#define __ARCH_ARM_MACH_TEGRA_BOARD_HTC_H

enum {
	BOARD_MFG_MODE_NORMAL = 0,
	BOARD_MFG_MODE_FACTORY2,
	BOARD_MFG_MODE_RECOVERY,
	BOARD_MFG_MODE_CHARGE,
	BOARD_MFG_MODE_POWERTEST,
	BOARD_MFG_MODE_OFFMODE_CHARGING,
	BOARD_MFG_MODE_MFGKERNEL,
	BOARD_MFG_MODE_MODEM_CALIBRATION,
};

enum {
	RADIO_FLAG_NONE = 0,
	RADIO_FLAG_MORE_LOG = BIT(0),
	RADIO_FLAG_FTRACE_ENABLE = BIT(1),
	RADIO_FLAG_USB_UPLOAD = BIT(3),
	RADIO_FLAG_DIAG_ENABLE = BIT(17),
};

enum {
	MDM_SKU_WIFI_ONLY = 0,
	MDM_SKU_UL,
	MDM_SKU_WL,
	NUM_MDM_SKU
};

enum {
	RADIO_IMG_NON_EXIST = 0,
	RADIO_IMG_EXIST
};

int board_mfg_mode(void);
unsigned int get_radio_flag(void);
int get_mdm_sku(void);
bool is_mdm_modem(void);
#endif
