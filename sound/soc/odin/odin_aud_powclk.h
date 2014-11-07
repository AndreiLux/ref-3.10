/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2014 by LG Electronics Inc.
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

#ifndef __ODIN_AUD_POWCLK_H__
#define __ODIN_AUD_POWCLK_H__

enum {
	ODIN_AUD_PARENT_OSC,
	ODIN_AUD_PARENT_PLL,
	ODIN_AUD_PARENT_MAX,
};

enum {
	ODIN_AUD_CLK_SI2S0,
	ODIN_AUD_CLK_SI2S1,
	ODIN_AUD_CLK_SI2S2,
	ODIN_AUD_CLK_SI2S3,
	ODIN_AUD_CLK_SI2S4,
	ODIN_AUD_CLK_MI2S0,
	ODIN_AUD_CLK_MAX,
};

enum {
	ODIN_AUD_PW_DOMAIN_0,
	ODIN_AUD_PW_DOMAIN_1,
	ODIN_AUD_PW_DOMAIN_2,
	ODIN_AUD_PW_DOMAIN_MAX,
};

extern int odin_aud_powclk_get_clk_id(unsigned int phy_address);
extern int odin_aud_powclk_clk_unprepare_disable(int id);
extern int odin_aud_powclk_clk_prepare_enable(int id);
extern int odin_aud_powclk_set_clk_parent(int id, int parent_id, int freqeuncy);

#endif /* __ODIN_AUD_POWCLK_H__ */

