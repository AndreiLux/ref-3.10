/*
 * Copyright(c) 2014, Analogix Semiconductor. All rights reserved.
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

#include "slimport.h"
#include "slimport_tx_drv.h"
#ifdef CEC_ENABLE
#include "slimport_tx_cec.h"
#endif
//#define CHANGE_TX_P0_ADDR

#ifdef SP_REGISTER_SET_TEST
#define sp_tx_link_phy_initialization() do{ \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG0, val_SP_TX_LT_CTRL_REG0); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG4, 0x1b); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG7, 0x22); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG9, 0x23); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG14, 0x09); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG17, 0x16); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG19, 0x1F); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG1, val_SP_TX_LT_CTRL_REG1); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG5, val_SP_TX_LT_CTRL_REG5); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG8, val_SP_TX_LT_CTRL_REG8); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG15, val_SP_TX_LT_CTRL_REG15); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG18, val_SP_TX_LT_CTRL_REG18); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG2, val_SP_TX_LT_CTRL_REG2); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG6, val_SP_TX_LT_CTRL_REG6); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG16, val_SP_TX_LT_CTRL_REG16); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG3, 0x3F); \
	}while(0)
#else
#define sp_tx_link_phy_initialization() do{ \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG0, 0x19); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG4, 0x1b); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG7, 0x22); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG9, 0x23); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG14, 0x09); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG17, 0x16); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG19, 0x1F); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG1, 0x26); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG5, 0x28); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG8, 0x2F); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG15, 0x10); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG18, 0x1F); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG2, 0x36); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG6, 0x3c); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG16, 0x18); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG3, 0x3F); \
	}while(0)
#endif

#ifdef CHANGE_TX_P0_ADDR
#ifdef TX_P0
	#undef TX_P0
	#define TX_P0 0x78
#endif
#endif
