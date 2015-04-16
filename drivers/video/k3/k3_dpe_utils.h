/* Copyright (c) 2013-2014, Hisilicon Tech. Co., Ltd. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
* GNU General Public License for more details.
*
*/

#ifndef K3_DPE_UTILS_H
#define K3_DPE_UTILS_H

#include "k3_fb.h"


struct dss_clk_rate * get_dss_clk_rate(struct k3_fb_data_type *k3fd);
int set_dss_clk_rate(struct k3_fb_data_type *k3fd);

void init_dpp_pdp(struct k3_fb_data_type *k3fd);
void init_sbl_pdp(struct k3_fb_data_type *k3fd);
void init_acm(struct k3_fb_data_type *k3fd);
void init_igm(struct k3_fb_data_type *k3fd);
void init_gmp(struct k3_fb_data_type *k3fd);
void init_xcc(struct k3_fb_data_type *k3fd);
void init_gama(struct k3_fb_data_type *k3fd);
void init_ifbc_pdp(struct k3_fb_data_type *k3fd);

void init_dfs_pdp(struct k3_fb_data_type *k3fd);
void init_dfs_sdp(struct k3_fb_data_type *k3fd);

void init_ldi_pdp(struct k3_fb_data_type *k3fd);
void init_ldi_sdp(struct k3_fb_data_type *k3fd);
void deinit_ldi(struct k3_fb_data_type *k3fd);
void enable_ldi(struct k3_fb_data_type *k3fd);
void disable_ldi(struct k3_fb_data_type *k3fd);

void enable_clk_pdp(struct k3_fb_data_type *k3fd);
void disable_clk_pdp(struct k3_fb_data_type *k3fd);
void enable_clk_sdp(struct k3_fb_data_type *k3fd);
void disable_clk_sdp(struct k3_fb_data_type *k3fd);
void enable_clk_adp(struct k3_fb_data_type *k3fd);
void disable_clk_adp(struct k3_fb_data_type *k3fd);
void enable_clk_common(struct k3_fb_data_type *k3fd);
void disable_clk_common(struct k3_fb_data_type *k3fd);
void enable_clk_panel(struct k3_fb_data_type *k3fd);
void disable_clk_panel(struct k3_fb_data_type *k3fd);
void enable_clk_dpe2(struct k3_fb_data_type *k3fd);
void disable_clk_dpe2(struct k3_fb_data_type *k3fd);
void enable_clk_rot1(struct k3_fb_data_type *k3fd);
void disable_clk_rot1(struct k3_fb_data_type *k3fd);
void enable_clk_axi(struct k3_fb_data_type *k3fd);
void disable_clk_axi(struct k3_fb_data_type *k3fd);

/* isr */
irqreturn_t dss_pdp_isr(int irq, void *ptr);
irqreturn_t dss_sdp_isr(int irq, void *ptr);
irqreturn_t dss_adp_isr(int irq, void *ptr);

int dpe_regulator_enable(struct k3_fb_data_type *k3fd);
int dpe_regulator_disable(struct k3_fb_data_type *k3fd);
int dpe_clk_enable(struct k3_fb_data_type *k3fd);
int dpe_clk_disable(struct k3_fb_data_type *k3fd);
void single_frame_update(struct k3_fb_data_type *k3fd);


#endif
