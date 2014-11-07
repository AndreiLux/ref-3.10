/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
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

#ifndef _DSS_COMP_H_
#define _DSS_COMP_H_

#include <video/odin-dss/dss_types.h>
#include "dss_comp_types.h"
#include "hal/dss_hal_ch_id.h"

unsigned int dss_comp_update_layer_imgs( void *instance_id,
											struct dss_image			input_image[],
											struct dss_input_meta		input_meta[],
											enum dss_image_assign_res   assign_req[],
											unsigned int				num_of_layer,
											struct dss_image			*output_image,
											struct dss_output_meta		*outpu_meta,
											enum sync_ch_id				sync_ch,
											bool							command_mode );

void *dss_comp_open( void *handle );
bool dss_comp_close( void *instance_id );

void dss_comp_init(unsigned long (*buf_alloc)(const size_t size), 
		void (*buf_free)(const unsigned long paddr), 
		void (*buf_done)(void *handle, const unsigned long paddr), 
		unsigned long (*rot_paddr_get)(const unsigned long paddr));
void dss_comp_cleanup( void );

#endif /* #ifndef _DSS_COMP_H_ */

