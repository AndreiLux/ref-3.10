/*
 * MDIS driver
 *
 * Copyright (C) 2013 Mtekvision
 * Author: Jinyoung Park <parkjyb@mtekvision.com>
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
 */

/******************************************************************************
	Internal header files
******************************************************************************/

#include <linux/version.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/i2c.h>
#include <linux/videodev2.h>
#include <linux/poll.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/timer.h>

#if CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include "odin-css.h"
#include "odin-css-system.h"
#include "odin-css-clk.h"
#include "odin-framegrabber.h"
#include "odin-mdis.h"

#include <linux/gpio.h>

/******************************************************************************
	Macro definitions
******************************************************************************/
#define STAT_GET_VALID_IDX(a)	 ((a) ? 0 : 1)

/******************************************************************************
	Static variables
******************************************************************************/
static struct mdis_hardware *g_mdis_hw = NULL;
static struct css_mdis_ctrl *g_mdis_ctrl = NULL;
static u32 count = 0;

const struct css_image_size bayer_scaler_outres_table[] = {
	{ 768,	576},
	{1536,	864},
	{2104, 1560}, /* {2304, 1296}, */
};


/******************************************************************************
	Function definitions
******************************************************************************/
static inline void set_mdis_hw(struct mdis_hardware *hw)
{
	g_mdis_hw = hw;
}

static inline struct mdis_hardware *get_mdis_hw(void)
{
	BUG_ON(!g_mdis_hw);
	return g_mdis_hw;
}

static inline struct css_mdis_ctrl *odin_mdis_set_handler(void)
{
	return (struct css_mdis_ctrl *)kzalloc(
		sizeof(struct css_mdis_ctrl), GFP_KERNEL);
}

static inline struct css_mdis_ctrl *odin_mdis_get_handler(void)
{
	 return g_mdis_ctrl;
}

static inline struct css_mdis_vector_windows *odin_mdis_get_vector_windows(void)
{
	if (g_mdis_ctrl)
		return &g_mdis_ctrl->vec_wds[STAT_GET_VALID_IDX(mdis_fill_idx)];
	else
		return NULL;
}

static inline void odin_mdis_set_reg(u32 val, u32 offset)
{
	struct mdis_hardware *mdis_hw;

	mdis_hw = get_mdis_hw();

	writel(val, mdis_hw->iobase + offset);
}

static inline u32 odin_mdis_get_reg(u32 offset)
{
	struct mdis_hardware *mdis_hw;

	mdis_hw = get_mdis_hw();

	return readl(mdis_hw->iobase + offset);
}

static inline void odin_mdis_interrupt_enable(void)
{
	odin_mdis_set_reg(0x01, REG_MDIS_INTERRUPT_CLR);
	odin_mdis_set_reg(MDIS_INTERRUPT_ENABLE_BIT, REG_MDIS_INTERRUPT_EN);
}

static inline void odin_mdis_interrupt_disable(void)
{
	odin_mdis_set_reg(MDIS_INTERRUPT_DISABLE_BIT, REG_MDIS_INTERRUPT_EN);
}

struct css_mdis_vector_offset *odin_mdis_pop_vector(void)
{
	struct css_mdis_ctrl *ctrl = odin_mdis_get_handler();
	struct css_mdis_vector_offset *vector = NULL;

	if (list_empty(&ctrl->head)) {
		return NULL;
	} else {
		vector = list_first_entry(&ctrl->head, \
			struct css_mdis_vector_offset, entry);

		list_del(&vector->entry);
	}

	return vector;
}

static void odin_mdis_push_vector(s32 vector_x, s32 vector_y)
{
	struct css_mdis_ctrl *ctrl = odin_mdis_get_handler();
	struct css_mdis_vector_offset *vec_offset = &ctrl->vec_offset[count];
	struct css_mdis_vector_offset *tmp = NULL;
	struct list_head *pos = NULL;
	struct list_head *n = NULL;

	vec_offset->x = vector_x;
	vec_offset->y = vector_y;

	list_for_each_safe(pos, n, &ctrl->head) {
		tmp = list_entry(pos, struct css_mdis_vector_offset, entry);

		if (tmp->index == count) {
			css_warn("Exist id(%d) in queue\n", count);
			return;
		}
	}

	vec_offset->index = count;
	vec_offset->frame_num = scaler_get_frame_num(CSS_SCALER_0);

	list_add_tail(&vec_offset->entry, &ctrl->head);
}

static void odin_mdis_flush_vector(void)
{
	for (;;) {
		if (odin_mdis_pop_vector() == NULL) {
			break;
		}
	}
}

static void odin_mdis_find_vector(void)
{
	struct css_mdis_ctrl *ctrl = odin_mdis_get_handler();
	struct css_mdis_vector_windows *vec_wds = odin_mdis_get_vector_windows();
	s32 vector_h = 0;
	s32 vector_v = 0;
	s32 comp_cnt = 0;
	u32 i;	/* Window number */
	u32 j;	/* Vector number */
	u32 k, tmp;

	/* Find mdis middle value of vector */
	for (i = 0, tmp = 0; i < 5; i++) {
		/* Bubble Sort */
		for (k = 0; k < 10; k++) {
			for (j = 1; j < 10 - k; j++) {
				if (vec_wds->hvec[i][j - 1] > vec_wds->hvec[i][j]) {
					tmp = vec_wds->hvec[i][j - 1];
					vec_wds->hvec[i][j - 1] = vec_wds->hvec[i][j];
					vec_wds->hvec[i][j] = tmp;
				}

				if (vec_wds->vvec[i][j - 1] > vec_wds->vvec[i][j]) {
					tmp = vec_wds->vvec[i][j - 1];
					vec_wds->vvec[i][j - 1] = vec_wds->vvec[i][j];
					vec_wds->vvec[i][j] = tmp;
				}
			}
			/*
			vec_wds->hvec_re[i] = vec_wds->hvec[i][j];
			vec_wds->vvec_re[i] = vec_wds->vvec[i][j];
			*/
		}
	}

	for (k = 0, tmp = 0; k < 5; k++) {
		for (i = 1; i < 5 - k; i++) {
			if (vec_wds->hvec_re[i - 1] > vec_wds->hvec_re[i]) {
				tmp = vec_wds->hvec_re[i - 1];
				vec_wds->hvec_re[i - 1] = vec_wds->hvec_re[i];
				vec_wds->hvec_re[i] = tmp;
			}

			if (vec_wds->vvec_re[i - 1] > vec_wds->vvec_re[i]) {
				tmp = vec_wds->vvec_re[i - 1];
				vec_wds->vvec_re[i - 1] = vec_wds->vvec_re[i];
				vec_wds->vvec_re[i] = tmp;
			}
		}
	}

	comp_cnt = (ctrl->config.compare.h_comp_cnt >> 1);

	vector_h = (s32)(vec_wds->hvec_re[2] - comp_cnt);
	vector_v = (s32)(vec_wds->vvec_re[2] - comp_cnt);

	if (vector_h < ((-1) * comp_cnt))
		vector_h = ((-1) * comp_cnt);

	if (vector_v < ((-1) * comp_cnt))
		vector_v = ((-1) * comp_cnt);

	odin_mdis_push_vector(vector_h, vector_v);
}

static void odin_mdis_config_compare(struct css_mdiscfg_compare *p_comp_cnt)
{
	struct css_mdiscfg_compare *comp_cnt = p_comp_cnt;
	u32 val;

	val = ((comp_cnt->h_comp_cnt) << 24) | ((comp_cnt->v_comp_cnt) << 16) |
			((comp_cnt->ch_comp_cnt) << 8) | (comp_cnt->cv_comp_cnt);

	css_mdis("%s(): val(0x%08x)\n", __func__, val);

	odin_mdis_set_reg(val, REG_MDIS_COMPARE_RANGE);
}

static void odin_mdis_config_interval(struct css_mdiscfg_interval *p_intv)
{
	struct css_mdiscfg_interval *intv = p_intv;
	u32 val, c_val;

	val = ((intv->h_intv) << 16) | (intv->v_intv);
	c_val = ((intv->ch_intv) << 16) | (intv->cv_intv);

	css_mdis("%s(): val(0x%08x)\n", __func__, val);
	css_mdis("%s(): c_val(0x%08x)\n", __func__, c_val);

	odin_mdis_set_reg(val, REG_MDIS_SEARCH_INTERVAL_SIZE);
	odin_mdis_set_reg(c_val, REG_MDIS_SEARCH_INTERVAL_SIZE_CENTER);
}

static void odin_mdis_config_mask(struct css_mdiscfg_mask *p_mask)
{
	struct css_mdiscfg_mask *mask = p_mask;
	u32 h_val, v_val;
	u32 ch_val, cv_val;

	h_val = ((mask->h_hmask) << 16) | (mask->h_vmask);
	v_val = ((mask->v_hmask) << 16) | (mask->v_vmask);
	ch_val = ((mask->ch_hmask) << 16) | (mask->ch_vmask);
	cv_val = ((mask->cv_hmask) << 16) | (mask->cv_vmask);

	css_mdis("%s(): h_val(0x%08x)\n", __func__, h_val);
	css_mdis("%s(): v_val(0x%08x)\n", __func__, v_val);
	css_mdis("%s(): ch_val(0x%08x)\n", __func__, ch_val);
	css_mdis("%s(): cv_val(0x%08x)\n", __func__, cv_val);

	odin_mdis_set_reg(h_val, REG_MDIS_UPPER_LOWER_WINDOW_SIZE_H);
	odin_mdis_set_reg(v_val, REG_MDIS_UPPER_LOWER_WINDOW_SIZE_V);
	odin_mdis_set_reg(ch_val, REG_MDIS_CENTER_WINDOW_SIZE_H);
	odin_mdis_set_reg(cv_val, REG_MDIS_CENTER_WINDOW_SIZE_V);
}

static void odin_mdis_config_offset(struct css_mdiscfg_offset *p_offset)
{
	struct css_mdiscfg_offset *offset = p_offset;
	u32 win_cnt;
	u32 val;
	u32 i;

	win_cnt = MDIS_WINDOW_COUNT;

	for (i = 0; i < win_cnt; i++) {
		val = ((offset[i].h_offset) << 16) | (offset[i].v_offset);
		css_mdis("%s(): val[%d](0x%08x)\n", __func__, (i * 4), val);
		odin_mdis_set_reg(val, REG_MDIS_UPPER_LEFT_WINDOW_OFFSET_H + (i * 4));
	}
}

static void odin_mdis_config_trunc(struct css_mdiscfg_trunc *p_trunc_thr)
{
	struct css_mdiscfg_trunc *trunc_thr = p_trunc_thr;
	u32 val, c_val;

	val = ((trunc_thr->h_trunc_thr) << 16) | (trunc_thr->v_trunc_thr);
	c_val = ((trunc_thr->ch_trunc_thr) << 16) | (trunc_thr->cv_trunc_thr);

	css_mdis("%s(): val(0x%08x)\n", __func__, val);
	css_mdis("%s(): c_val(0x%08x)\n", __func__, c_val);

	odin_mdis_set_reg(val, REG_MDIS_TRUNCATION_THRESHOLD);
	odin_mdis_set_reg(c_val, REG_MDIS_TRUNCATION_THRESHOLD_CENTER);
}

static void odin_mdis_config_vsize(u32 v_size)
{
	odin_mdis_set_reg(v_size, REG_MDIS_VERTICAL_SIZE);
}

static void odin_mdis_config_pass_frame(u32 pass_frame)
{
	odin_mdis_set_reg(pass_frame, REG_MDIS_FRAME_PASS_NUMBER);
}

static void odin_mdis_set_trunc(u32 v_size, struct css_mdiscfg_trunc *p_trunc)
{
	struct css_mdiscfg_trunc *trunc = p_trunc;

	if (v_size == bayer_scaler_outres_table[0].height) {
		css_mdis("%s(): Vertical size: %d\n", __func__, v_size);
		trunc->h_trunc_thr = 7;		trunc->v_trunc_thr = 7;
		trunc->ch_trunc_thr = 15;	trunc->cv_trunc_thr = 17;
	} else if (v_size == bayer_scaler_outres_table[1].height) {
		css_mdis("%s(): Vertical size: %d\n", __func__, v_size);
		trunc->h_trunc_thr = 15;	trunc->v_trunc_thr = 15;
		trunc->ch_trunc_thr = 25;	trunc->cv_trunc_thr = 27;
	} else if (v_size == bayer_scaler_outres_table[2].height) {
		trunc->h_trunc_thr = 10;	trunc->v_trunc_thr = 10;
		trunc->ch_trunc_thr = 10;	trunc->cv_trunc_thr = 10;
	}
}

static void odin_mdis_set_mask(u32 v_size, struct css_mdiscfg_mask *p_mask)
{
	struct css_mdiscfg_mask *mask = p_mask;

	if (v_size == bayer_scaler_outres_table[0].height) {
		css_mdis("%s(): Vertical size: %d\n", __func__, v_size);
		mask->h_hmask = 200;	mask->h_vmask = 160;
		mask->v_hmask = 200;	mask->v_vmask = 160;
		mask->ch_hmask = 440;	mask->ch_vmask = 350;
		mask->cv_hmask = 440;	mask->cv_vmask = 350;
	} else if (v_size == bayer_scaler_outres_table[1].height) {
		css_mdis("%s(): Vertical size: %d\n", __func__, v_size);
		mask->h_hmask = 400;	mask->h_vmask = 280;
		mask->v_hmask = 400;	mask->v_vmask = 280;
		mask->ch_hmask = 630;	mask->ch_vmask = 380;
		mask->cv_hmask = 630;	mask->cv_vmask = 380;
	} else if (v_size == bayer_scaler_outres_table[2].height) {
		css_mdis("%s(): Vertical size: %d\n", __func__, v_size);
		mask->h_hmask = 640;	mask->h_vmask = 300;
		mask->v_hmask = 640;	mask->v_vmask = 300;
		mask->ch_hmask = 640;	mask->ch_vmask = 300;
		mask->cv_hmask = 640;	mask->cv_vmask = 300;
	}
}

static
void odin_mdis_set_interval(u32 v_size, struct css_mdiscfg_interval *p_intv)
{
	struct css_mdiscfg_interval *intv = p_intv;

	if (v_size == bayer_scaler_outres_table[0].height) {
		css_mdis("%s(): Vertical size: %d\n", __func__, v_size);
		intv->h_intv = 30;		intv->v_intv = 30;
		intv->ch_intv = 60;		intv->cv_intv = 70;
	} else if (v_size == bayer_scaler_outres_table[1].height) {
		css_mdis("%s(): Vertical size: %d\n", __func__, v_size);
		intv->h_intv = 60;		intv->v_intv = 80;
		intv->ch_intv = 100;	intv->cv_intv = 120;
	} else if (v_size == bayer_scaler_outres_table[2].height) {
		css_mdis("%s(): Vertical size: %d\n", __func__, v_size);
		intv->h_intv = 120;		intv->v_intv = 75;
		intv->ch_intv = 120;	intv->cv_intv = 75;
	}
}

static
void odin_mdis_set_compare(u32 v_size, struct css_mdiscfg_compare  *p_comp)
{
	struct css_mdiscfg_compare *comp = p_comp;

	if (v_size == bayer_scaler_outres_table[0].height) {
		css_mdis("%s(): Vertical size: %d\n", __func__, v_size);
		comp->h_comp_cnt = 64;	comp->v_comp_cnt = 64;
		comp->ch_comp_cnt = 64;	comp->cv_comp_cnt = 64;
	} else if (v_size == bayer_scaler_outres_table[1].height) {
		css_mdis("%s(): Vertical size: %d\n", __func__, v_size);
		comp->h_comp_cnt = 64;	comp->v_comp_cnt = 64;
		comp->ch_comp_cnt = 64;	comp->cv_comp_cnt = 64;
	} else if (v_size == bayer_scaler_outres_table[2].height) {
		css_mdis("%s(): Vertical size: %d\n", __func__, v_size);
		comp->h_comp_cnt = 64;	comp->v_comp_cnt = 64;
		comp->ch_comp_cnt = 64;	comp->cv_comp_cnt = 64;
	}
}

static
void odin_mdis_set_offset(u32 v_size, struct css_mdiscfg_offset *p_offset)
{
	struct css_mdiscfg_offset *offset = p_offset;

	if (v_size == bayer_scaler_outres_table[0].height) {
		css_mdis("%s(): Vertical size: %d\n", __func__, v_size);
		offset[0].h_offset = 1;		offset[0].v_offset = 1;
		offset[1].h_offset = 1;		offset[1].v_offset = 1;
		offset[2].h_offset = 320;	offset[2].v_offset = 1;
		offset[3].h_offset = 320;	offset[3].v_offset = 1;
		offset[4].h_offset = 100;	offset[4].v_offset = 50;
		offset[5].h_offset = 100;	offset[5].v_offset = 50;
		offset[6].h_offset = 1;		offset[6].v_offset = 241;
		offset[7].h_offset = 1;		offset[7].v_offset = 241;
		offset[8].h_offset = 320;	offset[8].v_offset = 241;
		offset[9].h_offset = 320;	offset[9].v_offset = 241;
	} else if (v_size == bayer_scaler_outres_table[1].height) {
		css_mdis("%s(): Vertical size: %d\n", __func__, v_size);
		offset[0].h_offset = 1;		offset[0].v_offset = 1;
		offset[1].h_offset = 1;		offset[1].v_offset = 1;
		offset[2].h_offset = 871;	offset[2].v_offset = 1;
		offset[3].h_offset = 871;	offset[3].v_offset = 1;
		offset[4].h_offset = 321;	offset[4].v_offset = 171;
		offset[5].h_offset = 321;	offset[5].v_offset = 171;
		offset[6].h_offset = 1;		offset[6].v_offset = 361;
		offset[7].h_offset = 1;		offset[7].v_offset = 361;
		offset[8].h_offset = 871;	offset[8].v_offset = 361;
		offset[9].h_offset = 871;	offset[9].v_offset = 361;
	} else if (v_size == bayer_scaler_outres_table[2].height) {
		css_mdis("%s(): Vertical size: %d\n", __func__, v_size);
		offset[0].h_offset = 1;		offset[0].v_offset = 17;
		offset[1].h_offset = 1;		offset[1].v_offset = 17;
		offset[2].h_offset = 1460;	offset[2].v_offset = 1;
		offset[3].h_offset = 1460;	offset[3].v_offset = 1;
		offset[4].h_offset = 720;	offset[4].v_offset = 456;	/* or 456 + 96*/
		offset[5].h_offset = 720;	offset[5].v_offset = 456;	/* or 456 + 96*/
		offset[6].h_offset = 1;		offset[6].v_offset = 780;	/* or 796 */
		offset[7].h_offset = 1;		offset[7].v_offset = 780;	/* or 796 */
		offset[8].h_offset = 1460;	offset[8].v_offset = 780;	/* or 796 */
		offset[9].h_offset = 1460;	offset[9].v_offset = 780;	/* or 796 */
	}
}

static
void odin_mdis_set_tuning_table(u32 v_size, struct css_mdis_configs *p_config)
{
	odin_mdis_set_trunc(v_size, &p_config->trunc);
	odin_mdis_set_offset(v_size, &p_config->offset[0]);
	odin_mdis_set_mask(v_size, &p_config->mask);
	odin_mdis_set_interval(v_size, &p_config->interval);
	odin_mdis_set_compare(v_size, &p_config->compare);
}

static
void odin_mdis_config_tunings(u32 v_size, u32 pass_frame,
			struct css_mdis_configs *p_config)
{
	odin_mdis_config_vsize(v_size);
	odin_mdis_config_pass_frame(pass_frame);
	odin_mdis_config_trunc(&p_config->trunc);
	odin_mdis_config_offset(&p_config->offset[0]);
	odin_mdis_config_mask(&p_config->mask);
	odin_mdis_config_interval(&p_config->interval);
	odin_mdis_config_compare(&p_config->compare);
}

static s32 odin_mdis_set_mode(u32 mode)
{
	u32 ctrl = 0x0;
	s32 ret = CSS_SUCCESS;

	if (mode & MDIS_ENABLE) {
		css_info("VDIS is enabled\n");
		ctrl |= STAT_MDIS_ENABLE;
		ctrl |= STAT_MDIS_VF_REVERSE_ON;
	} else {
		css_info("VDIS is disabled\n");
		ctrl &= MDIS_DISABLE;
	}

	css_mdis("%s(): --- MDIS_CTRL: 0x%08x\n", __func__, ctrl);
	odin_mdis_set_reg(ctrl, REG_MDIS_CTRL);

	return ret;
}

static s32 odin_mdis_configure(u32 v_size, u32 pass_frame)
{
	struct css_mdis_ctrl *ctrl = odin_mdis_get_handler();
	s32 ret = CSS_SUCCESS;

	css_info("VDIS Vertical size: %d\n", v_size);

	odin_mdis_set_tuning_table(v_size, &ctrl->config);
	odin_mdis_config_tunings(v_size, pass_frame, &ctrl->config);

	return ret;
}

s32 odin_mdis_control(struct file *p_file, struct css_mdis_control_args *arg)
{
	struct css_mdis_control_args *args = arg;
	s32 ret = CSS_SUCCESS;
	s32 i = -1;

	switch (args->cmd) {
	case ODIN_CMD_MDIS_ENABLE:
		odin_mdis_flush_vector();
		odin_mdis_interrupt_enable();
		odin_mdis_set_mode(1);
		css_mdis("\n");
		css_mdis("MDIS_VERTICAL_SIZE(0x%08x)\n",
				odin_mdis_get_reg(REG_MDIS_VERTICAL_SIZE));
		css_mdis("[info]MDIS_CTRL(0x%08x)\n",
				odin_mdis_get_reg(REG_MDIS_CTRL));
		css_mdis("[info]MDIS_TRUNCATION_THRESHOLD(0x%08x)\n",
				odin_mdis_get_reg(REG_MDIS_TRUNCATION_THRESHOLD));
		css_mdis("[info]MDIS_TRUNCATION_THRESHOLD_CENTER(0x%08x)\n",
				odin_mdis_get_reg(REG_MDIS_TRUNCATION_THRESHOLD_CENTER));
		css_mdis("[info]MDIS_UPPER_LOWER_WINDOW_SIZE_H(0x%08x)\n",
				odin_mdis_get_reg(REG_MDIS_UPPER_LOWER_WINDOW_SIZE_H));
		css_mdis("[info]MDIS_UPPER_LOWER_WINDOW_SIZE_V(0x%08x)\n",
				odin_mdis_get_reg(REG_MDIS_UPPER_LOWER_WINDOW_SIZE_V));
		css_mdis("[info]MDIS_CENTER_WINDOW_SIZE_H(0x%08x)\n",
				odin_mdis_get_reg(REG_MDIS_CENTER_WINDOW_SIZE_H));
		css_mdis("[info]MDIS_CENTER_WINDOW_SIZE_V(0x%08x)\n",
				odin_mdis_get_reg(REG_MDIS_CENTER_WINDOW_SIZE_V));
		css_mdis("[info]MDIS_SEARCH_INTERVAL_SIZE(0x%08x)\n",
				odin_mdis_get_reg(REG_MDIS_SEARCH_INTERVAL_SIZE));
		css_mdis("[info]MDIS_SEARCH_INTERVAL_SIZE_CENTER(0x%08x)\n",
				odin_mdis_get_reg(REG_MDIS_SEARCH_INTERVAL_SIZE_CENTER));
		css_mdis("[info]MDIS_COMPARE_RANGE(0x%08x)\n",
				odin_mdis_get_reg(REG_MDIS_COMPARE_RANGE));

		for (i = 0; i < MDIS_WINDOW_COUNT; i++) {
			css_mdis("UPPER_LEFT_WINDOW_OFFSET_H[%d](0x%08x)\n", (i * 4),
				odin_mdis_get_reg(REG_MDIS_UPPER_LEFT_WINDOW_OFFSET_H + \
				(i * 4)));
		}
		css_mdis("\n");
		break;

	case ODIN_CMD_MDIS_DISABLE:
		odin_mdis_flush_vector();
		odin_mdis_interrupt_disable();
		odin_mdis_set_mode(0);
		odin_mdis_deinit_device();
		break;

	case ODIN_CMD_MDIS_SET_VERTICAL_SIZE:
		odin_mdis_init_device();
		odin_mdis_configure(args->vsize, args->pass_frame);
		break;

	case ODIN_CMD_MDIS_GET_DATA:
	{
		struct css_mdis_vector_offset *vector = NULL;

		vector = odin_mdis_pop_vector();

		if (vector != NULL) {
			if (!access_ok(VERIFY_WRITE,
					(void *)args->offset_x, sizeof(u32))) {
				css_err("Failed to access user memory addr of offset_x\n");
			}
			if (!access_ok(VERIFY_WRITE,
					(void *)args->offset_x, sizeof(u32))) {
				css_err("Failed to access user memory addr of offset_x\n");
			}
			if (!access_ok(VERIFY_WRITE,
					(void *)args->frame_num, sizeof(u32))) {
				css_err("Failed to access user memory addr of offset_x\n");
			}
			if (put_user(vector->x, ((u32 *)args->offset_x)) < 0) {
				css_err("Failed to copy memory from kernel to user\n");
				ret = CSS_FAILURE;
			}
			if (put_user(vector->y, ((u32 *)args->offset_y)) < 0) {
				css_err("Failed to copy memory from kernel to user\n");
				ret = CSS_FAILURE;
			}
			if (put_user(vector->frame_num, ((u32 *)args->frame_num)) < 0) {
				css_err("Failed to copy memory from kernel to user\n");
				ret = CSS_FAILURE;
			}
		}

		break;
	}

	default:
		css_err("Invaild mdis control command\n");
		ret = CSS_FAILURE;
	}

	return ret;
}

s32 odin_mdis_init(void)
{
	struct mdis_hardware *mdis_hw = get_mdis_hw();
	s32 ret = CSS_SUCCESS;
	u32 i = -1;

	if (g_mdis_ctrl == NULL)
		g_mdis_ctrl = odin_mdis_set_handler();

	if (NULL == g_mdis_ctrl)
		return CSS_FAILURE;

	memset(g_mdis_ctrl, 0, sizeof(struct css_mdis_ctrl));

	for (i = 0; i < (CSS_MAX_FRAME_BUFFER_COUNT << 1); i++)
		g_mdis_ctrl->vec_offset[i].index = -1;

	spin_lock_init(&mdis_hw->slock);

	INIT_LIST_HEAD(&g_mdis_ctrl->head);

	css_power_domain_on(POWER_MDIS);
	css_clock_enable(CSS_CLK_MDIS);
	css_block_reset(BLK_RST_MDIS);

	enable_irq(mdis_hw->irq);

	return ret;
}

s32 odin_mdis_deinit(void)
{
	s32 ret = CSS_SUCCESS;
	struct mdis_hardware *mdis_hw = get_mdis_hw();

	count = 0;

	if (g_mdis_ctrl) {
		kfree(g_mdis_ctrl);
		g_mdis_ctrl = NULL;
	}

	atomic_set(&frame_cnt, 0);
	atomic_set(&mdis_cnt, 0);

	disable_irq(mdis_hw->irq);

	css_clock_disable(CSS_CLK_MDIS);

	css_power_domain_off(POWER_MDIS);

	return ret;
}

irqreturn_t odin_mdis_irq(s32 irq, void *dev_id)
{
	struct mdis_hardware *mdis_hw = get_mdis_hw();
	struct css_mdis_vector_windows *vec_wds = odin_mdis_get_vector_windows();
	u32 hdata, vdata, rhdata, rvdata, data5;
	u32 cnt = 0;
	u32 i = 0;
	u32 j = 0;

	cnt = atomic_add_return(1, &mdis_cnt);

	if (cnt & 1) { /* First interrupt - Upper left & right window area */
		for (i = 0; i < 2; i++) {
			hdata = readl(mdis_hw->iobase +
				(REG_MDIS_UPPER_LEFT_WINDOW_VECTOR_H + (i << 5)));
			vdata = readl(mdis_hw->iobase +
				(REG_MDIS_UPPER_LEFT_WINDOW_VECTOR_V + (i << 5)));
			rhdata = readl(mdis_hw->iobase +
				(REG_MDIS_UPPER_LEFT_WINDOW_VECTOR_H_REVERSE + (i << 5)));
			rvdata = readl(mdis_hw->iobase +
				(REG_MDIS_UPPER_LEFT_WINDOW_VECTOR_V_REVERSE + (i << 5)));
			data5 = readl(mdis_hw->iobase +
				(REG_MDIS_UPPER_LEFT_WINDOW_VECTOR_H_V_5TH + (i << 5)));

			for (j = 0; j < 4; j++) {
				vec_wds->hvec[i][j] = (hdata >> (j << 3)) & 0xFF;
				vec_wds->vvec[i][j] = (vdata >> (j << 3)) & 0xFF;
				vec_wds->hvec[i][j + 5] = (rhdata >> (j << 3)) & 0xFF;
				vec_wds->vvec[i][j + 5] = (rvdata >> (j << 3)) & 0xFF;
			}

			if (j == 4) {
				vec_wds->hvec[i][j] = (data5 >> 24) & 0xFF;
				vec_wds->vvec[i][j] = (data5 >> 16) & 0xFF;
				vec_wds->hvec[i][j + 5] = (data5 >> 8) & 0xFF;
				vec_wds->vvec[i][j + 5] = (data5) & 0xFF;
			}
		}
	} else {/* Second interrupt - Lower left & right and central window area */
		for (i = 2; i < 5; i++) {
			hdata = readl(mdis_hw->iobase +
				(REG_MDIS_UPPER_LEFT_WINDOW_VECTOR_H + (i << 5)));
			vdata = readl(mdis_hw->iobase +
				(REG_MDIS_UPPER_LEFT_WINDOW_VECTOR_V + (i << 5)));
			rhdata = readl(mdis_hw->iobase +
				(REG_MDIS_UPPER_LEFT_WINDOW_VECTOR_H_REVERSE + (i << 5)));
			rvdata = readl(mdis_hw->iobase +
				(REG_MDIS_UPPER_LEFT_WINDOW_VECTOR_V_REVERSE + (i << 5)));
			data5 = readl(mdis_hw->iobase +
				(REG_MDIS_UPPER_LEFT_WINDOW_VECTOR_H_V_5TH + (i << 5)));

			for (j = 0; j < 4; j++) {
				vec_wds->hvec[i][j] = (hdata >> (j << 3)) & 0xFF;
				vec_wds->vvec[i][j] = (vdata >> (j << 3)) & 0xFF;
				vec_wds->hvec[i][j + 5] = (rhdata >> (j << 3)) & 0xFF;
				vec_wds->vvec[i][j + 5] = (rvdata >> (j << 3)) & 0xFF;
			}

			if (j == 4) {
				vec_wds->hvec[i][j] = (data5 >> 24) & 0xFF;
				vec_wds->vvec[i][j] = (data5 >> 16) & 0xFF;
				vec_wds->hvec[i][j + 5] = (data5 >> 8) & 0xFF;
				vec_wds->vvec[i][j + 5] = (data5) & 0xFF;
			}
		}

		odin_mdis_find_vector();

		count++;

		if (count >= (CSS_MAX_FRAME_BUFFER_COUNT << 1)) {
			count = 0;
		}
	}

	odin_mdis_set_reg(0x01, REG_MDIS_INTERRUPT_CLR);

	return IRQ_HANDLED;
}

s32 odin_mdis_init_device(void)
{
	struct mdis_hardware *mdis_hw = NULL;
	s32 ret = CSS_SUCCESS;

	mdis_hw = get_mdis_hw();

	ret = odin_mdis_init();
	spin_lock_init(&mdis_hw->slock);

	return ret;
}

s32 odin_mdis_deinit_device(void)
{
	s32 ret = CSS_SUCCESS;

	odin_mdis_deinit();

	return ret;
}

s32 odin_mdis_init_resources(struct platform_device *pdev)
{
	struct css_device *cssdev = NULL;
	struct mdis_hardware *mdis_hw = NULL;
	struct resource *iores_mdis = NULL;
	u64 size_mdis = 0;
	s32 ret = CSS_SUCCESS;

	cssdev = get_css_plat();
	if (!cssdev) {
		css_err("can't get cssdev!\n");
		return CSS_FAILURE;
	}

	mdis_hw = (struct mdis_hardware *)&cssdev->mdis_hw;
	if (!mdis_hw) {
		css_err("can't get scl_hw!\n");
		return CSS_FAILURE;
	}

	mdis_hw->pdev = pdev;

	platform_set_drvdata(pdev, mdis_hw);

	iores_mdis = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (NULL == iores_mdis) {
		css_err("failed to get mdis resource!\n");
		return CSS_ERROR_GET_RESOURCE;
	}
	size_mdis = iores_mdis->end - iores_mdis->start + 1;

	mdis_hw->irq = platform_get_irq(pdev, 0);
	if (mdis_hw->irq < 0) {
		css_err("failed to get mdis irq number\n");
		return CSS_ERROR_GET_RESOURCE;
	}

	mdis_hw->iores = request_mem_region(iores_mdis->start,
										size_mdis, pdev->name);
	if (NULL == mdis_hw->iores) {
		css_err("failed to request mem region : %s!\n", pdev->name);
		return CSS_ERROR_GET_RESOURCE;
	}

	mdis_hw->iobase = ioremap_nocache(iores_mdis->start, size_mdis);
	if (NULL == mdis_hw->iobase) {
		css_err("failed to ioremap(isp0)!\n");
		ret = CSS_ERROR_IOREMAP;
		goto error_ioremap;
	}

	ret = request_irq(mdis_hw->irq, odin_mdis_irq, 0, pdev->name, g_mdis_ctrl);
	if (ret < 0) {
		css_err("failed to request mdis irq %d\n", ret);
		goto error_irq;
	}

	disable_irq(mdis_hw->irq);

	set_mdis_hw(mdis_hw);

	return ret;

error_irq:
	free_irq(mdis_hw->irq, mdis_hw);
	iounmap(mdis_hw->iobase);

error_ioremap:
	release_mem_region(iores_mdis->start, size_mdis);

	return ret;
}

s32 odin_mdis_deinit_resources(struct platform_device *pdev)
{
	struct mdis_hardware *mdis_hw = NULL;
	struct css_mdis_ctrl *ctrl = odin_mdis_get_handler();
	s32 ret = CSS_SUCCESS;

	mdis_hw = platform_get_drvdata(pdev);

	if (mdis_hw) {
		if (mdis_hw->irq > 0)
			free_irq(mdis_hw->irq, &g_mdis_ctrl);

		if (NULL != mdis_hw->iobase)
			iounmap(mdis_hw->iobase);

		if (NULL != mdis_hw->iores)
			release_mem_region(mdis_hw->iores->start,
								mdis_hw->iores->end -
								mdis_hw->iores->start + 1);

		set_mdis_hw(NULL);
		ctrl->mdis_base = NULL;
	}

	return ret;
}

int odin_mdis_probe(struct platform_device *pdev)
{
	int ret = CSS_SUCCESS;

	ret = odin_mdis_init_resources(pdev);

	if (ret == CSS_SUCCESS) {
		css_info("mdis probe ok!!\n");
	} else {
		css_err("mdis probe err!!\n");
	}

	return ret;
}

int odin_mdis_remove(struct platform_device *pdev)
{
	int ret = CSS_SUCCESS;

	ret = odin_mdis_deinit_resources(pdev);

	if (ret == CSS_SUCCESS) {
		css_info("mdis remove ok!!\n");
	} else {
		css_err("mdis remove err!!\n");
	}

	return ret;
}

int odin_mdis_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	return ret;
}

int odin_mdis_resume(struct platform_device *pdev)
{
	int ret = 0;
	return ret;
}

#ifdef CONFIG_OF
static struct of_device_id odin_mdis_match[] = {
	{ .compatible = CSS_OF_MDIS_NAME},
	{},
};
#endif /* CONFIG_OF */

struct platform_driver odin_mdis_driver =
{
	.probe		= odin_mdis_probe,
	.remove 	= odin_mdis_remove,
	.suspend	= odin_mdis_suspend,
	.resume 	= odin_mdis_resume,
	.driver 	= {
		.name	= CSS_PLATDRV_MDIS,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odin_mdis_match),
#endif
	},
};

static int __init odin_mdis_driver_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&odin_mdis_driver);

	return ret;
}

static void __exit odin_mdis_driver_exit(void)
{
	platform_driver_unregister(&odin_mdis_driver);
	return;
}

MODULE_DESCRIPTION("odin mdis driver");
MODULE_AUTHOR("MTEKVISION<http://www.mtekvision.com>");

late_initcall(odin_mdis_driver_init);
module_exit(odin_mdis_driver_exit);

MODULE_LICENSE("GPL");

