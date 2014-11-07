/*
 * clk-max77525.h - Clock Driver for the Maxim 77525
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

#ifndef __LINUX_CLK_MAX77525_H
#define __LINUX_CLK_MAX77525_H

struct max77525_clk_platform_data
{
	u8	osc_freq;
	u8	tcxo_pol;
	u8	tcxo_drv;
	u8	tcxo_dly;
};

#endif
