/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Jungmin Park <jungmin016.park@lge.com>
 * Youngki Lyu <youngki.lyu@lge.com>
 * 
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License 
 * version 2 as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
 * GNU General Public License for more details.
 */
#ifndef _ROT_RSC_DRV_H_
#define _ROT_RSC_DRV_H_

#include <linux/kernel.h>
#include <linux/types.h>
#include <video/odin-dss/dss_types.h>
#include "dss_comp_types.h"

#define ROT_SRC_NUM 1

bool rot_rsc_do(void* rot_rsc_id, struct dss_node_id *node_id, struct dss_node *node);

void* rot_rsc_alloc(void);
void rot_rsc_free(void* rot_rsc_id);

void rot_rsc_init(void (*cb_used_done)(struct dss_node_id *node_id, bool rsc_free, struct dss_image *input_image, struct dss_image *output_image), 
		void (*cb_ready_to_work)(struct dss_node_id *node_id, struct dss_node *node), 
		unsigned long (*cb_rot_paddr_get)(unsigned long paddr));
void rot_rsc_cleanup(void);

#endif /* #ifndef _ROT_RSC_DRV_H_ */



