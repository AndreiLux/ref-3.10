/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Cheolhyun park <cheolhyun.park@lge.com>
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

#ifndef _DU_OVL_DRV_H_
#define _DU_OVL_DRV_H_

#include <linux/types.h>
#include <video/odin-dss/dss_types.h>
#include "dss_comp_types.h"
#include "hal/dss_hal_ch_id.h"

bool du_ovl_do_imgs(struct dss_node_id node_id[], struct dss_node node[], enum sync_ch_id sync_ch, bool command_mode);
bool du_ovl_do_img(enum du_src_ch_id du_src_ch, struct dss_node_id *node_id, struct dss_node *node, enum sync_ch_id sync_ch, bool command_mode,
		bool locked);
bool du_ovl_available(enum du_src_ch_id du_src_ch, enum sync_ch_id sync_ch);
bool du_ovl_flush(enum du_src_ch_id du_src_ch, enum sync_ch_id sync_ch);

void du_ovl_init(void (*cb_used_done)(struct dss_node_id *node_id, bool rsc_free, struct dss_image *input_image, struct dss_image *output_image),
		void (*cb_ready_to_work)(struct dss_node_id *node_id, struct dss_node *node));
void du_ovl_cleanup(void);

#endif /* #ifndef _DU_OVL_DRV_H_ */


