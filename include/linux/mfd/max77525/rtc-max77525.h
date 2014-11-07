/*
 * rtc-max77525.h - RTC Driver for the Maxim 77525
 *
 * Copyright (C) 2013 Maxim Integrated
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __LINUX_RTC_MAX77525_H
#define __LINUX_RTC_MAX77525_H

enum max77525_smpl_threshold
{
	MAX77802A_SMPL_0_5S,
	MAX77802A_SMPL_1_0S,
	MAX77802A_SMPL_1_5S,
	MAX77802A_SMPL_2_0S,
	MAX77802A_SMPL_DISABLE,
};

struct max77525_rtc_platform_data
{
    enum max77525_smpl_threshold smpl_timer;
    bool smpl_en;
	bool rtc_write_enable;
	bool rtc_alarm_powerup;
};

#endif
