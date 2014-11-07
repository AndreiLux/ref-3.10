/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Sungwon Son <sungwon.son@lge.com>
 * Youngki Lyu <youngki.lyu@lge.com>
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

/*------------------------------------------------------------------------------
  File Incluseions
  ------------------------------------------------------------------------------*/

#include <linux/types.h>
#include <linux/kernel.h>

#include "ovl_hal.h"
#include "du_hal.h"
#include "sync_hal.h"

#include "reg_def/gra0_union_reg.h"
#include "reg_def/gra1_union_reg.h"
#include "reg_def/gra2_union_reg.h"
#include "reg_def/vid0_union_reg.h"
#include "reg_def/vid1_union_reg.h"
#include "reg_def/vid2_union_reg.h"
#include "reg_def/gscl_union_reg.h"

/*------------------------------------------------------------------------------
  Constant Definitions
  ------------------------------------------------------------------------------*/
#define DU_SIZE_MAX	0x1FFF
/*------------------------------------------------------------------------------
  Macro Definitions
  ------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------
  Type Definitions
  ------------------------------------------------------------------------------*/
enum du_res_vid
{
	DU_RES_VID0,
	DU_RES_VID1,
	DU_RES_VID2,
	DU_RES_VID_NUM
};

enum du_res_gra
{
	DU_RES_GRA0,
	DU_RES_GRA1,
	DU_RES_GRA2,
	DU_RES_GRA_NUM
};
/*------------------------------------------------------------------------------
  Static Function Prototypes Declarations
  ------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------
  Static Variables
  ------------------------------------------------------------------------------*/
volatile DSS_DU_VID0_REG_T *vidbase[DU_RES_VID_NUM];
volatile DSS_DU_GRA0_REG_T *grabase[DU_RES_GRA_NUM];
volatile DSS_DU_GSCL_REG_T *gsclbase;
/*------------------------------------------------------------------------------
  External Function Prototype Declarations
  ------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------
  global Functions
  ------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------
  global Variables
  ------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------
  Implementation Group
  ------------------------------------------------------------------------------*/
unsigned int du_hal_read_mem_full_command_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx != DU_SRC_VID0 && du_src_idx != DU_SRC_VID1 && 
			du_src_idx != DU_SRC_VID2 && du_src_idx != DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.rdmif_full_cmdfifo;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.rdmif_full_cmdfifo;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.rdmif_full_cmdfifo;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->rd_status.af.rdmif_full_cmdfifo;
		break;
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_read_mem_full_data_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx > DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.rdmif_full_datfifo;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.rdmif_full_datfifo;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.rdmif_full_datfifo;
		break;
	case DU_SRC_GRA0:
		ret = grabase[DU_RES_GRA0]->i0_rd_status.af.rdmif_full_datfifo;
		break;
	case DU_SRC_GRA1:
		ret = grabase[DU_RES_GRA1]->i0_rd_status.af.rdmif_full_datfifo;
		break;
	case DU_SRC_GRA2:
		ret = grabase[DU_RES_GRA2]->i0_rd_status.af.rdmif_full_datfifo;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->rd_status.af.rdmif_full_datfifo;
		break;
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_read_mem_empty_command_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx > DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.rdmif_empty_cmdfifo;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.rdmif_empty_cmdfifo;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.rdmif_empty_cmdfifo;
		break;
	case DU_SRC_GRA0:
		ret = grabase[DU_RES_GRA0]->i0_rd_status.af.rdmif_empty_cmdfifo;
		break;
	case DU_SRC_GRA1:
		ret = grabase[DU_RES_GRA1]->i0_rd_status.af.rdmif_empty_cmdfifo;
		break;
	case DU_SRC_GRA2:
		ret = grabase[DU_RES_GRA2]->i0_rd_status.af.rdmif_empty_cmdfifo;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->rd_status.af.rdmif_empty_cmdfifo;
		break;
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_read_mem_empty_data_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx > DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.rdmif_empty_datfifo;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.rdmif_empty_datfifo;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.rdmif_empty_datfifo;
		break;
	case DU_SRC_GRA0:
		ret = grabase[DU_RES_GRA0]->i0_rd_status.af.rdmif_empty_datfifo;
		break;
	case DU_SRC_GRA1:
		ret = grabase[DU_RES_GRA1]->i0_rd_status.af.rdmif_empty_datfifo;
		break;
	case DU_SRC_GRA2:
		ret = grabase[DU_RES_GRA2]->i0_rd_status.af.rdmif_empty_datfifo;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->rd_status.af.rdmif_empty_datfifo;
		break;
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_read_buffer_full_data_y_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx > DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.rdbif_full_datfifo_y;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.rdbif_full_datfifo_y;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.rdbif_full_datfifo_y;
		break;
	case DU_SRC_GRA0:
		ret = grabase[DU_RES_GRA0]->i0_rd_status.af.rdbif_full_datfifo_y;
		break;
	case DU_SRC_GRA1:
		ret = grabase[DU_RES_GRA1]->i0_rd_status.af.rdbif_full_datfifo_y;
		break;
	case DU_SRC_GRA2:
		ret = grabase[DU_RES_GRA2]->i0_rd_status.af.rdbif_full_datfifo_y;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->rd_status.af.rdbif_full_datfifo_y;
		break;
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_read_buffer_full_data_u_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx > DU_SRC_VID2) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.rdbif_full_datfifo_u;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.rdbif_full_datfifo_u;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.rdbif_full_datfifo_u;
		break;
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_GSCL:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_read_buffer_full_data_v_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx > DU_SRC_VID2) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.rdbif_full_datfifo_v;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.rdbif_full_datfifo_v;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.rdbif_full_datfifo_v;
		break;
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_GSCL:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_read_buffer_empty_data_y_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx >  DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.rdbif_empty_datfifo_y;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.rdbif_empty_datfifo_y;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.rdbif_empty_datfifo_y;
		break;
	case DU_SRC_GRA0:
		ret = grabase[DU_RES_GRA0]->i0_rd_status.af.rdbif_empty_datfifo_y;
		break;
	case DU_SRC_GRA1:
		ret = grabase[DU_RES_GRA1]->i0_rd_status.af.rdbif_empty_datfifo_y;
		break;
	case DU_SRC_GRA2:
		ret = grabase[DU_RES_GRA2]->i0_rd_status.af.rdbif_empty_datfifo_y;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->rd_status.af.rdbif_empty_datfifo_y;
		break;
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_read_buffer_empty_data_u_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx >  DU_SRC_VID2) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.rdbif_empty_datfifo_u;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.rdbif_empty_datfifo_u;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.rdbif_empty_datfifo_u;
		break;
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_GSCL:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_read_buffer_empty_data_v_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx >  DU_SRC_VID2) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.rdbif_empty_datfifo_v;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.rdbif_empty_datfifo_v;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.rdbif_empty_datfifo_v;
		break;
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_GSCL:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_read_buffer_con_y_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx >  DU_SRC_VID2) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.rdbif_con_state_y;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.rdbif_con_state_y;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.rdbif_con_state_y;
		break;
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_GSCL:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_read_buffer_con_u_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx >  DU_SRC_VID2) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.rdbif_con_state_u;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.rdbif_con_state_u;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.rdbif_con_state_u;
		break;
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_GSCL:
	case DU_SRC_NUM:
		return ret;
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_read_buffer_con_v_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx >  DU_SRC_VID2) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.rdbif_con_state_v;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.rdbif_con_state_v;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.rdbif_con_state_v;
		break;
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_GSCL:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_read_buffer_con_state(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx != DU_SRC_GRA0 && du_src_idx != DU_SRC_GRA1 &&
			du_src_idx != DU_SRC_GRA2 && du_src_idx != DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_GRA0:
		ret = grabase[DU_RES_GRA0]->i0_rd_status.af.rdbif_con_state;
		break;
	case DU_SRC_GRA1:
		ret = grabase[DU_RES_GRA1]->i0_rd_status.af.rdbif_con_state;
		break;
	case DU_SRC_GRA2:
		ret = grabase[DU_RES_GRA2]->i0_rd_status.af.rdbif_con_state;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->rd_status.af.rdbif_con_state;
		break;
	case DU_SRC_VID0:
	case DU_SRC_VID1:
	case DU_SRC_VID2:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_read_buffer_con_sync_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx > DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.rdbif_con_sync_state;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.rdbif_con_sync_state;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.rdbif_con_sync_state;
		break;
	case DU_SRC_GRA0:
		ret = grabase[DU_RES_GRA0]->i0_rd_status.af.rdbif_con_sync_state;
		break;
	case DU_SRC_GRA1:
		ret = grabase[DU_RES_GRA1]->i0_rd_status.af.rdbif_con_sync_state;
		break;
	case DU_SRC_GRA2:
		ret = grabase[DU_RES_GRA2]->i0_rd_status.af.rdbif_con_sync_state;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->rd_status.af.rdbif_con_sync_state;
		break;
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_read_buffer_y_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx >  DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.rdbif_fmc_y_state;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.rdbif_fmc_y_state;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.rdbif_fmc_y_state;
		break;
	case DU_SRC_GRA0:
		ret = grabase[DU_RES_GRA0]->i0_rd_status.af.rdbif_fmc_y_state;
		break;
	case DU_SRC_GRA1:
		ret = grabase[DU_RES_GRA1]->i0_rd_status.af.rdbif_fmc_y_state;
		break;
	case DU_SRC_GRA2:
		ret = grabase[DU_RES_GRA2]->i0_rd_status.af.rdbif_fmc_y_state;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->rd_status.af.rdbif_fmc_y_state;
		break;
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}
unsigned int du_hal_read_buffer_uv_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx >  DU_SRC_VID2) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.rdbif_fmc_uv_state;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.rdbif_fmc_uv_state;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.rdbif_fmc_uv_state;
		break;
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_GSCL:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_read_buffer_sync_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx >  DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.rdbif_fmc_sync_state;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.rdbif_fmc_sync_state;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.rdbif_fmc_sync_state;
		break;
	case DU_SRC_GRA0:
		ret = grabase[DU_RES_GRA0]->i0_rd_status.af.rdbif_fmc_sync_state;
		break;
	case DU_SRC_GRA1:
		ret = grabase[DU_RES_GRA1]->i0_rd_status.af.rdbif_fmc_sync_state;
		break;
	case DU_SRC_GRA2:
		ret = grabase[DU_RES_GRA2]->i0_rd_status.af.rdbif_fmc_sync_state;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->rd_status.af.rdbif_fmc_sync_state;
		break;
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_scaler_v_write_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx >  DU_SRC_VID1) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.scl_vsc_wr_state;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.scl_vsc_wr_state;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.scl_vsc_wr_state;
		break;
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_GSCL:
	case DU_SRC_NUM:
	default:
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);		
		return ret;
	}
	return ret;
}

unsigned int du_hal_scaler_v_read_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx >  DU_SRC_VID1) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.scl_vsc_rd_state;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.scl_vsc_rd_state;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.scl_vsc_rd_state;
		break;
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_GSCL:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_scaler_h_write_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx >  DU_SRC_VID1) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.scl_hsc_wr_state;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.scl_hsc_wr_state;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.scl_hsc_wr_state;
		break;
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_GSCL:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_scaler_h_read_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx >  DU_SRC_VID1) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_status.af.scl_hsc_rd_state;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_status.af.scl_hsc_rd_state;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_status.af.scl_hsc_rd_state;
		break;
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_GSCL:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

/* write back status function */
unsigned int du_hal_write_control_full_buffer_y_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx !=  DU_SRC_VID0 && du_src_idx != DU_SRC_VID1 && du_src_idx != DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_wb_status.af.wrctrl_full_inf_fifoy;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_wb_status.af.wrctrl_full_inf_fifoy;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->wb_status.af.wrctrl_full_inf_fifoy;
		break;
	case DU_SRC_VID2:
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_write_control_full_buffer_u_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx !=  DU_SRC_VID0 && du_src_idx != DU_SRC_VID1 && du_src_idx != DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_wb_status.af.wrctrl_full_inf_fifou;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_wb_status.af.wrctrl_full_inf_fifou;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->wb_status.af.wrctrl_full_inf_fifou;
		break;
	case DU_SRC_VID2:
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_write_control_full_buffer_v_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx !=  DU_SRC_VID0 && du_src_idx != DU_SRC_VID1 && du_src_idx != DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_wb_status.af.wrctrl_full_inf_fifov;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_wb_status.af.wrctrl_full_inf_fifov;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->wb_status.af.wrctrl_full_inf_fifov;
		break;
	case DU_SRC_VID2:
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_write_control_empty_buffer_y_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx !=  DU_SRC_VID0 && du_src_idx != DU_SRC_VID1 && du_src_idx != DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_wb_status.af.wrctrl_empty_inf_fifoy;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_wb_status.af.wrctrl_empty_inf_fifoy;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->wb_status.af.wrctrl_empty_inf_fifoy;
		break;
	case DU_SRC_VID2:
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_write_control_empty_buffer_u_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx !=  DU_SRC_VID0 && du_src_idx != DU_SRC_VID1 && du_src_idx != DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_wb_status.af.wrctrl_empty_inf_fifou;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_wb_status.af.wrctrl_empty_inf_fifou;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->wb_status.af.wrctrl_empty_inf_fifou;
		break;
	case DU_SRC_VID2:
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_write_control_empty_buffer_v_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx !=  DU_SRC_VID0 && du_src_idx != DU_SRC_VID1 && du_src_idx != DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_wb_status.af.wrctrl_empty_inf_fifov;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_wb_status.af.wrctrl_empty_inf_fifov;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->wb_status.af.wrctrl_empty_inf_fifov;
		break;
	case DU_SRC_VID2:
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_write_control_y_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx !=  DU_SRC_VID0 && du_src_idx != DU_SRC_VID1 && du_src_idx != DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_wb_status.af.wrctrl_con_y_state;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_wb_status.af.wrctrl_con_y_state;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->wb_status.af.wrctrl_con_y_state;
		break;
	case DU_SRC_VID2:
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_write_control_u_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx !=  DU_SRC_VID0 && du_src_idx != DU_SRC_VID1 && du_src_idx != DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_wb_status.af.wrctrl_con_u_state;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_wb_status.af.wrctrl_con_u_state;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->wb_status.af.wrctrl_con_u_state;
		break;
	case DU_SRC_VID2:
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_write_control_v_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx !=  DU_SRC_VID0 && du_src_idx != DU_SRC_VID1 && du_src_idx != DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_wb_status.af.wrctrl_con_v_state;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_wb_status.af.wrctrl_con_v_state;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->wb_status.af.wrctrl_con_v_state;
		break;
	case DU_SRC_VID2:
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_write_control_sync_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx !=  DU_SRC_VID0 && du_src_idx != DU_SRC_VID1 && du_src_idx != DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_wb_status.af.wrctrl_con_sync_state;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_wb_status.af.wrctrl_con_sync_state;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->wb_status.af.wrctrl_con_sync_state;
		break;
	case DU_SRC_VID2:
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}
	return ret;
}

unsigned int du_hal_write_mem_full_command_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx !=  DU_SRC_VID0 && du_src_idx != DU_SRC_VID1 && du_src_idx != DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_wb_status.af.wrmif_full_cmdfifo;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_wb_status.af.wrmif_full_cmdfifo;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->wb_status.af.wrmif_full_cmdfifo;
		break;
	case DU_SRC_VID2:
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_write_mem_full_data_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx !=  DU_SRC_VID0 && du_src_idx != DU_SRC_VID1 && du_src_idx != DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_wb_status.af.wrmif_full_datfifo;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_wb_status.af.wrmif_full_datfifo;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->wb_status.af.wrmif_full_datfifo;
		break;
	case DU_SRC_VID2:
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_wirte_mem_empty_command_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if(du_src_idx !=  DU_SRC_VID0 && du_src_idx != DU_SRC_VID1 && du_src_idx != DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_wb_status.af.wrmif_empty_cmdfifo;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_wb_status.af.wrmif_empty_cmdfifo;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->wb_status.af.wrmif_empty_cmdfifo;
		break;
	case DU_SRC_VID2:
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_wirte_mem_empty_data_fifo_status(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx !=  DU_SRC_VID0 && du_src_idx != DU_SRC_VID1 && du_src_idx != DU_SRC_GSCL) {
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_wb_status.af.wrmif_empty_datfifo;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_wb_status.af.wrmif_empty_datfifo;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->wb_status.af.wrmif_empty_datfifo;
		break;
	case DU_SRC_VID2:
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return ret;
	}

	return ret;
}

unsigned int du_hal_get_image_size(enum du_src_ch_id du_src_idx)
{
	unsigned int ret = 0xffffffff;

	if (du_src_idx >= DU_SRC_NUM) {
		pr_err("invalid du_src_idx(%d) of du_hal_get_image_size()\n", du_src_idx);
		return ret;
	}
		gra0_rd_sizexy gra_sizexy;
	switch (du_src_idx){
	case DU_SRC_VID0:
		ret = vidbase[DU_RES_VID0]->i0_rd_pip_sizexy.a32;
		break;
	case DU_SRC_VID1:
		ret = vidbase[DU_RES_VID1]->i0_rd_pip_sizexy.a32;
		break;
	case DU_SRC_VID2:
		ret = vidbase[DU_RES_VID2]->i0_rd_sizexy.a32;
		break;
	case DU_SRC_GRA0:
		ret = grabase[DU_RES_GRA0]->i0_rd_sizexy.a32;
		break;
	case DU_SRC_GRA1:
		ret = grabase[DU_RES_GRA1]->i0_rd_sizexy.a32;
		break;
	case DU_SRC_GRA2:
		ret = grabase[DU_RES_GRA2]->i0_rd_sizexy.a32;
		break;
	case DU_SRC_GSCL:
		ret = gsclbase->rd_sizexy.a32;
		break;
	case DU_SRC_NUM:
	default :
		pr_err("invalid du_src_idx(%d) of du_hal_get_image_size()\n", du_src_idx);
		return ret;
	}

	return ret;
}

bool du_hal_gra_enable(enum du_src_ch_id du_src_idx,
		enum sync_ch_id sync_sel,
		enum du_color_format color_format, enum du_byte_swap byte_swap,
		enum du_word_swap word_swap, enum du_alpha_swap alpha_swap,
		enum du_rgb_swap rgb_swap, enum du_lsb_padding_sel lsb_sel,
		enum du_flip_mode flip_mode,
		struct dss_image_size src_image, struct dss_image_rect crop_image,
		unsigned int addr_y)
{
	enum du_res_gra res_du_src_idx;

	volatile gra0_rd_cfg0 gra_rd_cfg0 = {.a32 = 0};
	volatile gra0_rd_cfg1 gra_rd_cfg1 = {.a32 = 0};
	volatile gscl_rd_cfg0 gscl_rd_cfg0_set = {.a32 = 0};
	volatile gscl_rd_cfg1 gscl_rd_cfg1_set = {.a32 = 0};

	if (du_src_idx < DU_SRC_GRA0 || du_src_idx > DU_SRC_GSCL || sync_sel >= SYNC_CH_NUM) {
		pr_err("invalid gra du_src_idx(%d)\n", du_src_idx);
		return false;
	}

	if (color_format > DU_COLOR_RGB888 || byte_swap > DU_BYTE_DCBA || word_swap > DU_WORD_BA ||
			alpha_swap > DU_ALPHA_RGBA || rgb_swap > DU_RGB_RBR) {
		pr_err("invalid gra color_format(%d), byte_swap(%d), word_swap(%d), alpha_swap(%d), rgb_swap(%d)\n", 
				color_format, byte_swap, word_swap, alpha_swap, rgb_swap);
		return false;
	}

	if (lsb_sel > DU_PAD_MSB || flip_mode > DU_FLIP_HV) {
		pr_err("invalid gra lsb_sel(%d), flip_mode(%d)\n", lsb_sel, flip_mode);
		return false;
	}

	if (src_image.w > DU_SIZE_MAX || src_image.h > DU_SIZE_MAX) {
		pr_err("invalid gra src_image(size:%d*%d)\n", src_image.w, src_image.h);
		return false;
	}

	if (crop_image.pos.x > DU_SIZE_MAX || crop_image.pos.y > DU_SIZE_MAX ||
			crop_image.size.w > DU_SIZE_MAX || crop_image.size.h > DU_SIZE_MAX) {
		pr_err("invalid gra crop_image(pos:%d,%d size:%d*%d)\n",
				crop_image.pos.x, crop_image.pos.y, crop_image.size.w, crop_image.size.h);
		return false;
	}

	if (addr_y == 0) {
		pr_err("invalid gra addr_y(0x%X)\n", addr_y);
		return false;
	}

	switch (du_src_idx) {
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
		switch (du_src_idx) {
		case DU_SRC_GRA0:
			res_du_src_idx = DU_RES_GRA0;
			break;
		case DU_SRC_GRA1:
			res_du_src_idx = DU_RES_GRA1;
			break;
		case DU_SRC_GRA2:
			res_du_src_idx = DU_RES_GRA2;
			break;
		case DU_SRC_VID0: /* to remove warning */
		case DU_SRC_VID1:
		case DU_SRC_VID2:
		case DU_SRC_GSCL:
		case DU_SRC_NUM:
		default:
			pr_err("invalid gra du_src_idx(%d)\n", du_src_idx);
			return false;
		}

		gra_rd_cfg0.af.src_en = 1;			/* cfg0 */
		gra_rd_cfg0.af.src_buf_sel = 0;
		gra_rd_cfg0.af.src_trans_num = 2;
		gra_rd_cfg0.af.src_sync_sel = sync_sel;
		gra_rd_cfg1.af.src_format = color_format;	/* cfg1 */
		gra_rd_cfg1.af.src_byte_swap = byte_swap;
		gra_rd_cfg1.af.src_word_swap = word_swap;
		gra_rd_cfg1.af.src_alpha_swap = alpha_swap;
		gra_rd_cfg1.af.src_rgb_swap = rgb_swap;
		gra_rd_cfg1.af.src_lsb_sel = lsb_sel;
		gra_rd_cfg1.af.src_flip_mode = flip_mode;	/* cfg1 end */
		grabase[res_du_src_idx]->i0_rd_cfg0 = gra_rd_cfg0;
		grabase[res_du_src_idx]->i0_rd_cfg1 = gra_rd_cfg1;
		grabase[res_du_src_idx]->i0_rd_width_height.a32 = (src_image.h << 16 | src_image.w);
		grabase[res_du_src_idx]->i0_rd_startxy.a32 = (crop_image.pos.y << 16 | crop_image.pos.x);
		grabase[res_du_src_idx]->i0_rd_sizexy.a32 = (crop_image.size.h << 16 | crop_image.size.w);
		grabase[res_du_src_idx]->i0_rd_dss_base_y_addr0.a32 = addr_y;
		break;
	case DU_SRC_GSCL:
		gscl_rd_cfg0_set.af.src_en = 1;				/* cfg0 */
		gscl_rd_cfg0_set.af.src_buf_sel = 0;
		gscl_rd_cfg0_set.af.src_trans_num = 2;
		gscl_rd_cfg0_set.af.src_sync_sel = sync_sel;
		gscl_rd_cfg1_set.af.src_format = color_format;		/* cfg1 */
		gscl_rd_cfg1_set.af.src_byte_swap = byte_swap;
		gscl_rd_cfg1_set.af.src_word_swap = word_swap;
		gscl_rd_cfg1_set.af.src_alpha_swap = alpha_swap;
		gscl_rd_cfg1_set.af.src_rgb_swap = rgb_swap;
		gscl_rd_cfg1_set.af.src_lsb_sel = lsb_sel;
		gscl_rd_cfg1_set.af.src_flip_op = flip_mode;		/* cfg1 end */
		gsclbase->rd_cfg0 = gscl_rd_cfg0_set;
		gsclbase->rd_cfg1 = gscl_rd_cfg1_set;
		gsclbase->rd_width_height.a32 = (src_image.h << 16 | src_image.w);
		gsclbase->rd_startxy.a32 = (crop_image.pos.y << 16 | crop_image.pos.x);
		gsclbase->rd_sizexy.a32 = (crop_image.size.h << 16 | crop_image.size.w);
		gsclbase->rd_dss_base_y_addr0.a32 = addr_y;
		break;
	case DU_SRC_VID0:	/* to remove warning */
	case DU_SRC_VID1:
	case DU_SRC_VID2:
	case DU_SRC_NUM:
	default:
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return false;
	}

	return true;
}

bool du_hal_gscl_wb_enable(
		/* cfg0 setting */
		bool alpha_reg_enable, enum sync_ch_id sync_sel,
		/* cfg1 setting */
		enum du_color_format color_format, enum du_byte_swap byte_swap,
		enum du_word_swap word_swap, enum du_alpha_swap alpha_swap,
		enum du_rgb_swap rgb_swap, enum du_yuv_swap yuv_swap,
		enum ovl_ch_id input_src_sel,
		/* alpha_value */
		unsigned char alpha_value, 
		struct dss_image_size src_image, struct dss_image_rect crop_image,
		/* addr setting */
		unsigned int addr_y, unsigned int addr_u)
{		
	volatile gscl_wb_cfg0 gscl_wb_cfg0_set = {.a32 = 0};
	volatile gscl_wb_cfg1 gscl_wb_cfg1_set = {.a32 = 0};

	if (sync_sel >= SYNC_CH_ID2) {
		pr_err("invalid gscl wb sync_sel(%d)\n", sync_sel);
		return false;
	}

	if (color_format != DU_COLOR_RGB565 && color_format != DU_COLOR_RGB555 && 
			color_format != DU_COLOR_RGB444 && color_format != DU_COLOR_RGB888 &&
			color_format != DU_COLOR_YUV422S && color_format != DU_COLOR_YUV422_2P &&
			color_format != DU_COLOR_YUV420_2P && color_format != DU_COLOR_YUV422_3P &&
			color_format != DU_COLOR_YUV420_3P) {
		pr_err("invalid gscl wb color_format(%d)\n", color_format);
		return false;
	}

	if (byte_swap > DU_BYTE_DCBA || word_swap > DU_WORD_BA || alpha_swap > DU_ALPHA_RGBA ||
			rgb_swap > DU_RGB_RBR || yuv_swap > DU_YUV_VU) {
		pr_err("invalid gscl wb byte_swap(%d) word_swap(%d) alpha_swap(%d) rgb_swap(%d) yuv_swap(%d)\n",
				byte_swap, word_swap, alpha_swap, rgb_swap, yuv_swap);
		return false;
	}

	if (input_src_sel >= OVL_CH_NUM) {
		pr_err("invalid gscl wb input_src_sel(%d)\n", input_src_sel);
		return false;
	}

	if (addr_y == 0 || addr_u == 0) {
		pr_err("invalid gscl wb addr_y(0x%X), addr_u(0x%X)\n", addr_y, addr_u);
		return false;
	}

	switch (color_format) {
	case DU_COLOR_RGB565:
		color_format = 0x0;
		break;
	case DU_COLOR_RGB555:
		color_format = 0x1;
		break;
	case DU_COLOR_RGB444:
		color_format = 0x2;
		break;
	case DU_COLOR_RGB888:
		color_format = 0x3;
		break;
	case DU_COLOR_YUV422S:
		color_format = 0x4;
		break;
	case DU_COLOR_YUV422_2P:
		color_format = 0x5;
		break;
	case DU_COLOR_YUV224_2P:
		return -1;
	case DU_COLOR_YUV420_2P:
		color_format = 0x6;
		break;
	case DU_COLOR_YUV444_2P:
		return -1;
	case DU_COLOR_YUV422_3P:
		color_format = 0x7;
		break;
	case DU_COLOR_YUV224_3P:
		return -1;
	case DU_COLOR_YUV420_3P:
		color_format = 0x8;
		break;
	case DU_COLOR_YUV444_3P:
		pr_err("invalid color_format(%d)\n", color_format);
		return false;
	default :
		pr_err("invalid color_format(%d)\n", color_format);
		return false;
	}
	
	gscl_wb_cfg0_set.af.wb_en = 1;
	gscl_wb_cfg0_set.af.wb_buf_sel = 0;
	gscl_wb_cfg0_set.af.wb_alpha_reg_en = alpha_reg_enable;
	gscl_wb_cfg0_set.af.wb_sync_sel = sync_sel;
	gscl_wb_cfg1_set.af.wb_format = color_format;
	gscl_wb_cfg1_set.af.wb_byte_swap = byte_swap;
	gscl_wb_cfg1_set.af.wb_word_swap = word_swap;
	gscl_wb_cfg1_set.af.wb_alpha_swap = alpha_swap;
	gscl_wb_cfg1_set.af.wb_rgb_swap = rgb_swap;
	gscl_wb_cfg1_set.af.wb_yuv_swap = yuv_swap;
	gscl_wb_cfg1_set.af.ovl_inf_src_sel = input_src_sel;
	gsclbase->wb_cfg0 = gscl_wb_cfg0_set;
	gsclbase->wb_cfg1 = gscl_wb_cfg1_set;
	gsclbase->wb_alpha_value.a32 = alpha_value;
	gsclbase->wb_width_height.a32 = (src_image.h << 16 | src_image.w);
	gsclbase->wb_startxy.a32 = (crop_image.pos.y << 16 | crop_image.pos.x);
	gsclbase->wb_sizexy.a32 = (crop_image.size.h << 16 | crop_image.size.w);
	gsclbase->wb_dss_base_y_addr0.a32 = addr_y;
	gsclbase->wb_dss_base_u_addr0.a32 = addr_u;

	return true;
}

bool du_hal_gra_disable(enum du_src_ch_id du_src_idx)
{
	enum du_res_gra base_du_src_idx;
	volatile gra0_rd_cfg0 gra_rd_cfg0_set = {.a32 = 0};
	volatile gra0_rd_cfg1 gra_rd_cfg1_set = {.a32 = 0};
	volatile gscl_rd_cfg0 gscl_rd_cfg0_set = {.a32 = 0};
	volatile gscl_rd_cfg1 gscl_rd_cfg1_set = {.a32 = 0};
	volatile gscl_wb_cfg0 gscl_wb_cfg0_set = {.a32 = 0};
	volatile gscl_wb_cfg1 gscl_wb_cfg1_set = {.a32 = 0};

	if (du_src_idx < DU_SRC_GRA0 || du_src_idx > DU_SRC_GSCL) {
		pr_err("invalid gra disable du_src_idx(%d)\n", du_src_idx);
		return false;
	}

	switch (du_src_idx) {
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
		switch (du_src_idx) {
		case DU_SRC_GRA0:
			base_du_src_idx = DU_RES_GRA0;
			break;
		case DU_SRC_GRA1:
			base_du_src_idx = DU_RES_GRA1;
			break;
		case DU_SRC_GRA2:
			base_du_src_idx = DU_RES_GRA2;
			break;
		case DU_SRC_VID0: /* remove warning */
		case DU_SRC_VID1:
		case DU_SRC_VID2:
		case DU_SRC_GSCL:
		case DU_SRC_NUM:
		default :
			pr_err("invalid gra disable du_src_idx(%d)\n", du_src_idx);
			return false;
		}

		gra_rd_cfg0_set.af.src_buf_sel = 0x3;
		gra_rd_cfg0_set.af.src_trans_num = 0x3;
		gra_rd_cfg0_set.af.src_sync_sel = 0x3;
		gra_rd_cfg1_set.af.src_rgb_swap = 0x6;
		grabase[base_du_src_idx]->i0_rd_cfg0 = gra_rd_cfg0_set;
		grabase[base_du_src_idx]->i0_rd_cfg1 = gra_rd_cfg1_set;
		grabase[base_du_src_idx]->i0_rd_width_height.a32 = 0;
		grabase[base_du_src_idx]->i0_rd_startxy.a32 = 0;
		grabase[base_du_src_idx]->i0_rd_sizexy.a32 = 0;
		grabase[base_du_src_idx]->i0_rd_dss_base_y_addr0.a32 = 0;
		break;
	case DU_SRC_GSCL:
		gscl_rd_cfg0_set.af.src_trans_num = 0x3;
		gscl_rd_cfg0_set.af.src_sync_sel = 0X3;
		gscl_rd_cfg1_set.af.src_rgb_swap = 0x6;
		gscl_wb_cfg0_set.af.wb_sync_sel = 0x3;
		gscl_wb_cfg1_set.af.wb_format = 0x9;
		gscl_wb_cfg1_set.af.wb_rgb_swap = 0x6;
		gscl_wb_cfg1_set.af.ovl_inf_src_sel = 0x3;
		gsclbase->rd_cfg0 = gscl_rd_cfg0_set;
		gsclbase->rd_cfg1 = gscl_rd_cfg1_set;
		gsclbase->rd_width_height.a32 = 0;
		gsclbase->rd_startxy.a32 = 0;
		gsclbase->rd_sizexy.a32 = 0;
		gsclbase->rd_dss_base_y_addr0.a32 = 0;	
		gsclbase->wb_cfg0 = gscl_wb_cfg0_set;
		gsclbase->wb_cfg1 = gscl_wb_cfg1_set;
		gsclbase->wb_alpha_value.a32 = 0;
		gsclbase->wb_width_height.a32 = 0;
		gsclbase->wb_startxy.a32 = 0;
		gsclbase->wb_sizexy.a32 = 0;
		gsclbase->wb_dss_base_y_addr0.a32 = 0;
		gsclbase->wb_dss_base_u_addr0.a32 = 0;
		break;
	case DU_SRC_VID0: /* remove warning */
	case DU_SRC_VID1:
	case DU_SRC_VID2:
	case DU_SRC_NUM:
	default:
		pr_err("invalid du_src_idx(%d)\n", du_src_idx);
		return false;
	}

	return true;
}

bool du_hal_vid_enable(enum du_src_ch_id du_src_idx,  //vid0, vid1, vid2
		/* cfg0 setting */
		bool dual_op_enable, enum sync_ch_id sync_sel,
		/* cfg1 setting */
		enum du_color_format color_format, enum du_byte_swap byte_swap,
		enum du_word_swap word_swap, enum du_alpha_swap alpha_swap,
		enum du_rgb_swap rgb_swap, enum du_yuv_swap yuv_swap,
		enum du_lsb_padding_sel lsb_sel, enum du_bnd_fill_mode bnd_fill_mode,
		enum du_flip_mode flip_mode,
		/* img setting */
		struct dss_image_size src_image, struct dss_image_rect crop_image,
		struct dss_image_size pip_image,
		/* addr setting */
		unsigned int addr_y, unsigned int addr_u, unsigned int addr_v)
{
	enum du_res_vid res_du_src_idx;
	volatile vid0_rd_cfg0 vid_rd_cfg0 = {.a32 = 0};
	volatile vid0_rd_cfg1 vid_rd_cfg1 = {.a32 = 0};
	volatile vid0_rd_scale_ratioxy vid_rd_scale_ratioxy = {.a32 = 0};

	if (du_src_idx > DU_SRC_VID2 || sync_sel >= SYNC_CH_NUM) {
		pr_err("invalid vid du_src_idx(%d)\n", du_src_idx);
		return false;
	}

	if (color_format > DU_COLOR_YUV444_3P || byte_swap > DU_BYTE_DCBA || word_swap > DU_WORD_BA ||
			alpha_swap > DU_ALPHA_RGBA || rgb_swap > DU_RGB_RBR || yuv_swap > DU_YUV_VU) {
		pr_err("invalid vid color_format(%d), byte_swap(%d), word_swap(%d), alpha_swap(%d), rgb_swap(%d), yuv_swap(%d)\n", 
				color_format, byte_swap, word_swap, alpha_swap, rgb_swap, yuv_swap);
		return false;
	}

	if (lsb_sel > DU_PAD_MSB || flip_mode > DU_FLIP_HV) {
		pr_err("invalid vid lsb_sel(%d), flip_mode(%d)\n", lsb_sel, flip_mode);

	}

	if (src_image.w > DU_SIZE_MAX || src_image.h > DU_SIZE_MAX) {
		pr_err("invalid vid src_image(size:%d*%d)\n", src_image.w, src_image.h);
		return false;
	}

	if (crop_image.pos.x > DU_SIZE_MAX || crop_image.pos.y > DU_SIZE_MAX ||
			crop_image.size.w > DU_SIZE_MAX || crop_image.size.h > DU_SIZE_MAX ||
			pip_image.w > DU_SIZE_MAX || pip_image.h > DU_SIZE_MAX) {
		pr_err("invalid vid crop_image(pos:%d,%d size:%d*%d), pip_image(size:%d*%d)\n", 
				crop_image.pos.x, crop_image.pos.y, crop_image.size.w, crop_image.size.h, pip_image.w, pip_image.h);
		return false;
	}

	if (color_format > DU_COLOR_RGB888) {
		if (addr_y == 0 || addr_u == 0 || addr_v == 0) {
			pr_err("invalid vid addr_y(0x%X), addr_u(0x%X), addr_v(0x%X)\n", addr_y, addr_u, addr_v);
			return false;
		}
	}
	else {
		if (addr_y == 0 || addr_u != 0 || addr_v != 0) {
			pr_err("invalid vid addr_y(0x%X), addr_u(0x%X), addr_v(0x%X)\n", addr_y, addr_u, addr_v);
			return false;
		}
	}

	switch (color_format) {
	case DU_COLOR_RGB565:
		color_format = 0x0;
		break;
	case DU_COLOR_RGB555:
		color_format = 0x1;
		break;
	case DU_COLOR_RGB444:
		color_format = 0x2;
		break;
	case DU_COLOR_RGB888:
		color_format = 0x3;
		break;
	case DU_COLOR_YUV422S:
		color_format = 0x5;
		break;
	case DU_COLOR_YUV422_2P:
		color_format = 0x8;
		break;
	case DU_COLOR_YUV224_2P:
		color_format = 0x9;
		break;
	case DU_COLOR_YUV420_2P:
		color_format = 0xA;
		break;
	case DU_COLOR_YUV444_2P:
		color_format = 0xB;
		break;
	case DU_COLOR_YUV422_3P:
		color_format = 0xC;
		break;
	case DU_COLOR_YUV224_3P:
		color_format = 0xD;
		break;
	case DU_COLOR_YUV420_3P:
		color_format = 0xE;
		break;
	case DU_COLOR_YUV444_3P:
		color_format = 0xF;
		break;
	default :
		pr_err("invalid color_format(%d)\n", color_format);
		return false;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		res_du_src_idx = DU_RES_VID0;
		break;
	case DU_SRC_VID1:
		res_du_src_idx = DU_RES_VID1;
		break;
	case DU_SRC_VID2:
		res_du_src_idx = DU_RES_VID2;
		break;
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_GSCL:
	case DU_SRC_NUM:
	default :
		pr_err("invalid color_format(%d)\n", color_format);
		return false;
	}

	vid_rd_cfg0.af.src_en = 1;
	vid_rd_cfg0.af.src_dst_sel = 0;
	vid_rd_cfg0.af.src_dual_op_en = dual_op_enable;
	vid_rd_cfg0.af.src_trans_num = 2;
	vid_rd_cfg0.af.src_sync_sel = sync_sel;
	vid_rd_cfg1.af.src_format = color_format;
	vid_rd_cfg1.af.src_byte_swap = byte_swap;
	vid_rd_cfg1.af.src_word_swap = word_swap;
	vid_rd_cfg1.af.src_alpha_swap = alpha_swap;
	vid_rd_cfg1.af.src_rgb_swap = rgb_swap;
	vid_rd_cfg1.af.src_yuv_swap = yuv_swap;
	vid_rd_cfg1.af.src_lsb_sel = lsb_sel;
	vid_rd_cfg1.af.src_bnd_fill_mode = bnd_fill_mode;

	if (du_src_idx == DU_SRC_VID0 || du_src_idx == DU_SRC_VID1) {
		if (crop_image.size.w != pip_image.w || crop_image.size.h != pip_image.h) {
			if (pip_image.w == 0 || pip_image.h == 0) {
				pr_err("invalid scale pip w(%d) h(%d)\n", pip_image.w, pip_image.h);
				return false;
			}
			vid_rd_scale_ratioxy.af.src_x_ratio = (crop_image.size.w * 4096)/pip_image.w;
			vid_rd_scale_ratioxy.af.src_y_ratio = (crop_image.size.h * 4096)/pip_image.h;
			vid_rd_cfg1.af.src_scl_bp_mode = 0;
			if (vid_rd_scale_ratioxy.af.src_x_ratio == 0 || vid_rd_scale_ratioxy.af.src_y_ratio == 0) {
				pr_err("invalid ratio value x_ratio(%d) y_ratio(%d)\n",
					vid_rd_scale_ratioxy.af.src_x_ratio, vid_rd_scale_ratioxy.af.src_y_ratio);
				return false;
			}
		}else {
			vid_rd_scale_ratioxy.af.src_x_ratio = 4096;
			vid_rd_scale_ratioxy.af.src_y_ratio = 4096;
			vid_rd_cfg1.af.src_scl_bp_mode = 1;
		}
	}

	vid_rd_cfg1.af.src_flip_mode = flip_mode;
	vidbase[res_du_src_idx]->i0_rd_cfg0 = vid_rd_cfg0;
	vidbase[res_du_src_idx]->i0_rd_cfg1 = vid_rd_cfg1;
	vidbase[res_du_src_idx]->i0_rd_width_height.a32 = (src_image.h << 16 | src_image.w);
	vidbase[res_du_src_idx]->i0_rd_startxy.a32 = (crop_image.pos.y << 16 | crop_image.pos.x);
	vidbase[res_du_src_idx]->i0_rd_sizexy.a32 = (crop_image.size.h << 16 | crop_image.size.w);
	vidbase[res_du_src_idx]->i0_rd_pip_sizexy.a32 = (pip_image.h << 16 | pip_image.w);
	vidbase[res_du_src_idx]->i0_rd_scale_ratioxy = vid_rd_scale_ratioxy;
	vidbase[res_du_src_idx]->i0_rd_dss_base_y_addr0.a32 = addr_y;
	vidbase[res_du_src_idx]->i0_rd_dss_base_u_addr0.a32 = addr_u;
	vidbase[res_du_src_idx]->i0_rd_dss_base_v_addr0.a32 = addr_v;

	return true;
}

bool du_hal_vid_wb_enable(enum du_src_ch_id du_src_idx, //vid0, vid1
		/* cfg0 setting */
		bool alpha_reg_enable, enum sync_ch_id sync_sel,
		/* cfg1 setting */
		enum du_color_format color_format, enum du_byte_swap byte_swap,
		enum du_word_swap word_swap, enum du_alpha_swap alpha_swap,
		enum du_rgb_swap rgb_swap, enum du_yuv_swap yuv_swap,
		enum du_src_ch_id  input_src_sel,
		/* alpha_value */
		unsigned char alpha_value, 
		struct dss_image_size src_image, struct dss_image_rect crop_image,
		/* addr setting */
		unsigned int addr_y, unsigned int addr_u)
{
	enum du_res_vid res_du_src_idx;
	volatile vid0_wb_cfg0 vid_wb_cfg0 = {.a32 = 0};
	volatile vid0_wb_cfg1 vid_wb_cfg1 = {.a32 = 0};
	volatile vid0_wb_alpha_value vid_wb_alpha_value = {.a32 = 0};

	if ((du_src_idx != DU_SRC_VID0 && du_src_idx != DU_SRC_VID1) || sync_sel >= SYNC_CH_NUM) {
		pr_err("invalid vid wb du_src_idx(%d), sync_sel(%d)\n", du_src_idx, sync_sel);
		return false;
	}

	if (color_format != DU_COLOR_RGB565 && color_format != DU_COLOR_RGB555 && 
			color_format != DU_COLOR_RGB444 && color_format != DU_COLOR_RGB888 &&
			color_format != DU_COLOR_YUV422S && color_format != DU_COLOR_YUV422_2P &&
			color_format != DU_COLOR_YUV420_2P && color_format != DU_COLOR_YUV422_3P &&
			color_format != DU_COLOR_YUV420_3P) {
		pr_err("invalid vid wb color_format(%d)\n", color_format);
		return false;
	}

	if (byte_swap > DU_BYTE_DCBA || word_swap > DU_WORD_BA || alpha_swap > DU_ALPHA_RGBA ||
			rgb_swap > DU_RGB_RBR || yuv_swap > DU_YUV_VU) {
		pr_err("invalid vid wb byte_swap(%d), word_swap(%d), alpha_swap(%d), rgb_swap(%d), yuv_swap(%d)\n",
				byte_swap, word_swap, alpha_swap, rgb_swap, yuv_swap);
		return false;
	}

	if (input_src_sel != DU_SRC_VID0 && input_src_sel != DU_SRC_VID1 && input_src_sel != DU_SRC_VID2) {
		pr_err("invalid vid wb input_src_sel(%d)\n",input_src_sel);
		return false;
	}

	switch (color_format) {
	case DU_COLOR_RGB565:
		color_format = 0x0;
		break;
	case DU_COLOR_RGB555:
		color_format = 0x1;
		break;
	case DU_COLOR_RGB444:
		color_format = 0x2;
		break;
	case DU_COLOR_RGB888:
		color_format = 0x3;
		break;
	case DU_COLOR_YUV422S:
		color_format = 0x4;
		break;
	case DU_COLOR_YUV422_2P:
		color_format = 0x5;
		break;
	case DU_COLOR_YUV224_2P:
		pr_err("invalid vid wb color_format(%d)\n", color_format);
		return false;
	case DU_COLOR_YUV420_2P:
		color_format = 0x6;
		break;
	case DU_COLOR_YUV444_2P:
		pr_err("invalid vid wb color_format(%d)\n", color_format);
		return false;
	case DU_COLOR_YUV422_3P:
		color_format = 0x7;
		break;
	case DU_COLOR_YUV224_3P:
		pr_err("invalid vid wb color_format(%d)\n", color_format);
		return false;
	case DU_COLOR_YUV420_3P:
		color_format = 0x8;
		break;
	case DU_COLOR_YUV444_3P:
		pr_err("invalid vid wb color_format(%d)\n", color_format);
		return false;
	default :
		pr_err("invalid vid wb color_format(%d)\n", color_format);
		return false;
	}		

	switch (du_src_idx) {
	case DU_SRC_VID0:
		res_du_src_idx = DU_RES_VID0;
		break;
	case DU_SRC_VID1:
		res_du_src_idx = DU_RES_VID1;
		break;
	case DU_SRC_VID2:
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_GSCL:
	case DU_SRC_NUM:
	default :
		pr_err("invalid vid wb color_format(%d)\n", color_format);
		return false;
	}

	vid_wb_cfg0.af.wb_en = 1;
	vid_wb_cfg0.af.wb_alpha_reg_en = alpha_reg_enable;
	vid_wb_cfg0.af.wb_sync_sel = sync_sel;
	vid_wb_cfg1.af.wb_format = color_format;
	vid_wb_cfg1.af.wb_byte_swap = byte_swap;
	vid_wb_cfg1.af.wb_word_swap = word_swap;
	vid_wb_cfg1.af.wb_alpha_swap = alpha_swap;
	vid_wb_cfg1.af.wb_rgb_swap = rgb_swap;
	vid_wb_cfg1.af.wb_yuv_swap = yuv_swap;
	vid_wb_cfg1.af.wb_src_sel = input_src_sel;
	vidbase[res_du_src_idx]->i0_wb_cfg0 = vid_wb_cfg0;
	vidbase[res_du_src_idx]->i0_wb_cfg1 = vid_wb_cfg1;
	vidbase[res_du_src_idx]->i0_wb_alpha_value.a32 = alpha_value;
	vidbase[res_du_src_idx]->i0_wb_width_height.a32 = (src_image.h << 16 | src_image.w);
	vidbase[res_du_src_idx]->i0_wb_startxy.a32 = (crop_image.pos.y << 16 | crop_image.pos.x);
	vidbase[res_du_src_idx]->i0_wb_sizexy.a32 = (crop_image.size.h << 16 | crop_image.size.w);
	vidbase[res_du_src_idx]->i0_wb_dss_base_y_addr0.a32 = addr_y;
	vidbase[res_du_src_idx]->i0_wb_dss_base_u_addr0.a32 = addr_u;
	vidbase[res_du_src_idx]->i0_rd_cfg0.af.src_dst_sel = 1;

	return true;
}

bool du_hal_vid_disable(enum du_src_ch_id du_src_idx)
{
	enum du_res_vid base_du_src_idx = 0;
	volatile vid0_rd_cfg0 vid_rd_cfg0_set = {.a32 = 0};
	volatile vid0_rd_cfg1 vid_rd_cfg1_set = {.a32 = 0};
	volatile vid0_wb_cfg0 vid_wb_cfg0_set = {.a32 = 0};
	volatile vid0_wb_cfg1 vid_wb_cfg1_set = {.a32 = 0};

	if (du_src_idx > DU_SRC_VID2) {
		pr_err("invalid vid disable du_src_idx(%d)\n", du_src_idx);
		return false;
	}

	switch (du_src_idx) {
	case DU_SRC_VID0:
		base_du_src_idx = DU_RES_VID0;
		break;
	case DU_SRC_VID1:
		base_du_src_idx = DU_RES_VID1;
		break;
	case DU_SRC_VID2:
		base_du_src_idx = DU_RES_VID2;
		break;
	case DU_SRC_GRA0:
	case DU_SRC_GRA1:
	case DU_SRC_GRA2:
	case DU_SRC_GSCL:
	case DU_SRC_NUM:
	default :
		pr_err("invalid vid disable du_src_idx(%d)\n", du_src_idx);
		return false;
	}

	vid_rd_cfg0_set.af.src_buf_sel = 0x3;
	vid_rd_cfg0_set.af.src_trans_num = 0x3;
	vid_rd_cfg0_set.af.src_sync_sel = 0x3;
	vid_rd_cfg1_set.af.src_rgb_swap = 0x6;
	vidbase[base_du_src_idx]->i0_rd_cfg0 = vid_rd_cfg0_set;
	vidbase[base_du_src_idx]->i0_rd_cfg1 = vid_rd_cfg1_set;
	vidbase[base_du_src_idx]->i0_rd_width_height.a32 = 0;
	vidbase[base_du_src_idx]->i0_rd_startxy.a32 = 0;
	vidbase[base_du_src_idx]->i0_rd_sizexy.a32 = 0;
	vidbase[base_du_src_idx]->i0_rd_pip_sizexy.a32 = 0;
	vidbase[base_du_src_idx]->i0_rd_scale_ratioxy.a32 = 0;
	vidbase[base_du_src_idx]->i0_rd_dss_base_y_addr0.a32 = 0;
	vidbase[base_du_src_idx]->i0_rd_dss_base_u_addr0.a32 = 0;
	vidbase[base_du_src_idx]->i0_rd_dss_base_v_addr0.a32 = 0;

	if (base_du_src_idx != DU_RES_VID2) {
		vid_wb_cfg0_set.af.wb_sync_sel = 0x3;
		vid_wb_cfg1_set.af.wb_format = 0x9;
		vid_wb_cfg1_set.af.wb_rgb_swap = 0x6;
		vidbase[base_du_src_idx]->i0_wb_cfg0 = vid_wb_cfg0_set;
		vidbase[base_du_src_idx]->i0_wb_cfg1 = vid_wb_cfg1_set;
		vidbase[base_du_src_idx]->i0_wb_alpha_value.a32 = 0;
		vidbase[base_du_src_idx]->i0_wb_width_height.a32 = 0;
		vidbase[base_du_src_idx]->i0_wb_startxy.a32 = 0;
		vidbase[base_du_src_idx]->i0_wb_sizexy.a32 = 0;
		vidbase[base_du_src_idx]->i0_wb_dss_base_y_addr0.a32 = 0;
		vidbase[base_du_src_idx]->i0_wb_dss_base_u_addr0.a32 = 0;
	}

	return true;
}

void du_hal_init(unsigned int vid0, unsigned int vid1, unsigned int vid2,
		unsigned int gra0, unsigned int gra1, unsigned int gra2,
		unsigned int gscl)
{
	vidbase[DU_RES_VID0] = (DSS_DU_VID0_REG_T *)vid0;
	vidbase[DU_RES_VID1] = (DSS_DU_VID0_REG_T *)vid1;
	vidbase[DU_RES_VID2] = (DSS_DU_VID0_REG_T *)vid2;

	grabase[DU_RES_GRA0] = (DSS_DU_GRA0_REG_T *)gra0;
	grabase[DU_RES_GRA1] = (DSS_DU_GRA0_REG_T *)gra1;
	grabase[DU_RES_GRA2] = (DSS_DU_GRA0_REG_T *)gra2;

	gsclbase = (DSS_DU_GSCL_REG_T *)gscl;
}

void  du_hal_cleanup(void)
{
}
