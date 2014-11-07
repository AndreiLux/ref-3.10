/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Younghyun Jo <younghyun.jo@lge.com>
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
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#include "dss_comp_types.h"

#include "media/odin/vcodec/vcore/ppu.h"
#include "media/odin/vcodec/vppu_drv.h"

#define ROT_MAX_PIPE_DEPTH (15)
#define _rot_rsc_tick_inc(t)	(((t)+1) % ROT_MAX_PIPE_DEPTH)

struct rot_pipe
{
	struct dss_node_id node_id;
	struct dss_node node;
};

struct rot_node
{
	struct rot_pipe pipe[ROT_MAX_PIPE_DEPTH];
	unsigned int prepare_t;
	unsigned int working_t;
};

struct rot_rsc_ch
{	
	void *vppu_id;

	struct mutex node_lock;
	struct rot_node node;
};

static struct rot_desc
{
	struct mutex usage_count_lock;
	int usage_count;

	void (*cb_used_done)(struct dss_node_id *node_id, bool rsc_free, struct dss_image *input_image, struct dss_image *output_image);
	void (*cb_ready_to_work)(struct dss_node_id *node_id, struct dss_node *node);
	unsigned long (*cb_rot_paddr_get)(unsigned long paddr);
} rot_desc;

static void _rot_rsc_undo_mark(struct rot_node *node)
{
	int pipe_i = 0;

	if (node->prepare_t == node->working_t)
		return;
	else if (node->working_t < node->prepare_t) {
		for (pipe_i=node->working_t+1; pipe_i<node->prepare_t; pipe_i++)
			node->pipe[pipe_i].node.undo = true;
	}
	else {
		for (pipe_i=node->working_t+1; pipe_i<ROT_MAX_PIPE_DEPTH-1; pipe_i++)
			node->pipe[pipe_i].node.undo = true;

		for (pipe_i=0; pipe_i<node->prepare_t; pipe_i++)
			node->pipe[pipe_i].node.undo = true;
	}
}

static void _rot_rsc_swap(unsigned int *a, unsigned int *b)
{
	unsigned int temp;

	temp = *a;
	*a = *b;
	*b = temp;
}

static bool _rot_rsc_report(struct dss_node_id *node_id, struct dss_node *node)
{
	struct dss_node ready_to_work_node;
	struct dss_node_id ready_to_work_node_id;
	struct dss_node temp_node;

	memset(&ready_to_work_node, 0, sizeof(struct dss_node));
	memset(&ready_to_work_node_id, 0, sizeof(struct dss_node_id));
	temp_node = *node;

	switch (node->input.in_meta.rotate_op) {
	case DSS_IMAGE_OP_ROTATE_90:
		node->input.in_meta.crop_rect.pos.x = node->input.image.image_size.h - node->input.in_meta.crop_rect.pos.y
			- node->input.in_meta.crop_rect.size.h;
		node->input.in_meta.crop_rect.pos.y = temp_node.input.in_meta.crop_rect.pos.x;

		_rot_rsc_swap(&node->input.in_meta.crop_rect.size.w, &node->input.in_meta.crop_rect.size.h);
		_rot_rsc_swap(&node->input.image.image_size.w, &node->input.image.image_size.h);
		break;
	case DSS_IMAGE_OP_ROTATE_180:
		node->input.in_meta.crop_rect.pos.x = node->input.image.image_size.w - node->input.in_meta.crop_rect.pos.x
			- node->input.in_meta.crop_rect.size.w;
		node->input.in_meta.crop_rect.pos.y = node->input.image.image_size.h - temp_node.input.in_meta.crop_rect.pos.y
			- node->input.in_meta.crop_rect.size.h;
		break;
	case DSS_IMAGE_OP_ROTATE_270:
		node->input.in_meta.crop_rect.pos.x = node->input.in_meta.crop_rect.pos.y;
		node->input.in_meta.crop_rect.pos.y = node->input.image.image_size.w - temp_node.input.in_meta.crop_rect.pos.x
			- node->input.in_meta.crop_rect.size.w;

		_rot_rsc_swap(&node->input.in_meta.crop_rect.size.w, &node->input.in_meta.crop_rect.size.h);
		_rot_rsc_swap(&node->input.image.image_size.w, &node->input.image.image_size.h);
		break;
	case DSS_IMAGE_OP_ROTATE_NONE:
	default:
		if (!node->undo)
			pr_err("invalid rotate_op:%d\n", node->input.in_meta.rotate_op);
		break;
	}

	node->input.in_meta.rotate_op = DSS_IMAGE_OP_ROTATE_NONE;

	ready_to_work_node_id = *node_id;
	ready_to_work_node = *node;
	rot_desc.cb_ready_to_work(&ready_to_work_node_id, &ready_to_work_node);

	node->output.image.port = DSS_DISPLAY_PORT_MEM;
	node->output.image.phy_addr = node->int_out_buf[node_id->node_id];
	rot_desc.cb_used_done(node_id, true, &node->input.image, &node->output.image);

	return true;
}

static bool _rot_rsc_rotate_do(void *vppu_id, struct dss_node_id *node_id, struct dss_node *node)
{
	enum vppu_image_format vppu_image_format;
	int angle = -1;
	enum vppu_report_info ret;
	unsigned int rot_input_paddr = 0;
	unsigned int rot_output_paddr = 0;

	switch (node->input.image.pixel_format) {
	case DSS_IMAGE_FORMAT_YUV420_2P:
		vppu_image_format = VPPU_IMAGE_FORMAT_YUV420_2P;
		break;
	case DSS_IMAGE_FORMAT_YUV420_3P:
		vppu_image_format = VPPU_IMAGE_FORMAT_YUV420_3P;
		break;
	default:
		pr_err("invalid input format:%d\n", node->input.image.pixel_format);
		return false;
	}

	switch (node->input.in_meta.rotate_op) {
	case DSS_IMAGE_OP_ROTATE_90:
		angle = 90;
		break;
	case DSS_IMAGE_OP_ROTATE_180:
		angle = 180;
		break;
	case DSS_IMAGE_OP_ROTATE_270:
		angle = 270;
		break;
	case DSS_IMAGE_OP_ROTATE_NONE:
	default :
		pr_err("invalid rotate op %d\n", node->input.in_meta.rotate_op);
		return false;
	}

	rot_input_paddr = rot_desc.cb_rot_paddr_get(node->input.image.phy_addr);
	if (rot_input_paddr == 0) {
		pr_err("input paddr get failed\n");
		return false;
	}

	rot_output_paddr = rot_desc.cb_rot_paddr_get(node->int_out_buf[node_id->node_id]);
	if (rot_output_paddr == 0) {
		pr_err("output paddr get failed\n");
		return false;
	}

	ret = vppu_rotate(vppu_id, rot_input_paddr, rot_output_paddr, 
			node->input.image.image_size.w, node->input.image.image_size.h, angle, vppu_image_format);
	if (ret != VPPU_FAIL) {
		//pr_err("vppu_rotate failed %d\n", ret);
		return false;
	}

	return true;
}

static bool _rot_rsc_rotate(struct rot_rsc_ch *ch, struct dss_node_id *node_id, struct dss_node *node)
{
	bool ret =false;
	if (!node->undo)
		return _rot_rsc_rotate_do(ch->vppu_id, node_id, node);
	else {
		ch->node.working_t = _rot_rsc_tick_inc(ch->node.working_t);

		ret = _rot_rsc_report(node_id, node);

		return ret;
	}
}

static void _rot_rsc_cb_report(void *rot_rsc_id, struct vcore_ppu_report * report)
{
	struct rot_rsc_ch *ch = (struct rot_rsc_ch *)rot_rsc_id;
	struct dss_node_id *node_id = NULL;
	struct dss_node *node = NULL;

	mutex_lock(&ch->node_lock);

	node_id = &ch->node.pipe[ch->node.working_t].node_id;
	node = &ch->node.pipe[ch->node.working_t].node;

	switch (report->hdr) {
	case VCORE_PPU_DONE:
		if (report->info.done == VCORE_PPU_FAIL) {
			pr_err("vppu failed\n");
		}
		else {
			if (ch->node.working_t == ch->node.prepare_t) {
				pr_err("no worked node vppu error? tick:%d\n", ch->node.working_t);
				break;
			}
			ch->node.working_t = _rot_rsc_tick_inc(ch->node.working_t);
			_rot_rsc_report(node_id, node);
		}
		break;
	case VCORE_PPU_RESET:
		if (report->info.reset == VCORE_PPU_REPORT_RESET_END)
			_rot_rsc_rotate(ch, node_id, node);
		break;
	case VCORE_PPU_FEED:
		_rot_rsc_rotate(ch, node_id, node);
		break;
	}

	mutex_unlock(&ch->node_lock);
}

bool rot_rsc_do(void* rot_rsc_id, struct dss_node_id *node_id, struct dss_node *node)
{
	struct rot_rsc_ch *ch  = (struct rot_rsc_ch *)rot_rsc_id;
	bool running = false;
	bool ret = false;
	unsigned int prepare_t_next = 0;

	if (ch == NULL || node_id == NULL || node == NULL) {
		pr_err("Invalid argument ch:0x%08X node_id:0x%08X, node:0x%08X\n", (int)ch, (int)node_id, (int)node);
		return false;
	}

	mutex_lock(&ch->node_lock);

	prepare_t_next = _rot_rsc_tick_inc(ch->node.prepare_t);
	if (prepare_t_next == ch->node.working_t) {
		pr_err("rot_rsc too many work reserved, working:%u, prepare_t:%d\n", ch->node.working_t, ch->node.prepare_t);
		mutex_unlock(&ch->node_lock);
		return false;
	}

	running = (ch->node.prepare_t == ch->node.working_t) ? false : true;

	if (node->undo)
		_rot_rsc_undo_mark(&ch->node);

	ch->node.pipe[ch->node.prepare_t].node_id = *node_id;
	ch->node.pipe[ch->node.prepare_t].node = *node;
	ch->node.prepare_t = _rot_rsc_tick_inc(ch->node.prepare_t);


	if (running == false) 
		ret =  _rot_rsc_rotate(ch, node_id, node);
	else 
		ret = true;

	mutex_unlock(&ch->node_lock);

	return ret;
}

void* rot_rsc_alloc(void)
{
	struct rot_rsc_ch *ch = NULL;

	mutex_lock(&rot_desc.usage_count_lock);

	if (rot_desc.usage_count > 0) {
		mutex_unlock(&rot_desc.usage_count_lock);
		//pr_err("rot rsc is already occupied\n");
		return NULL;
	}
	else if (rot_desc.usage_count > 1) {
		pr_warn("usage_count(%d) must be 0 or 1\n", rot_desc.usage_count);
		mutex_unlock(&rot_desc.usage_count_lock);
		return NULL;
	}

	ch = vzalloc(sizeof(struct rot_rsc_ch));
	if (ch == NULL) {
		mutex_unlock(&rot_desc.usage_count_lock);
		pr_err("vzalloc failed failed\n");
		return (void*)NULL;
	}

	mutex_init(&ch->node_lock);

	ch->vppu_id = vppu_open(false , 0, 0, 0, 0, ch, _rot_rsc_cb_report);
	if(IS_ERR_OR_NULL(ch->vppu_id)) {
		vfree(ch);
		mutex_unlock(&rot_desc.usage_count_lock);
		pr_err("vppu open failed\n");
		return (void*)NULL;
	}

	rot_desc.usage_count++;

	mutex_unlock(&rot_desc.usage_count_lock);

	return (void*)ch;
}

void rot_rsc_free(void* rot_rsc_id)
{
	struct rot_rsc_ch *ch = (struct rot_rsc_ch *)rot_rsc_id;
	if (ch == NULL)
		return;

	mutex_lock(&rot_desc.usage_count_lock);

	if (rot_desc.usage_count == 0) {
		//pr_err("rot rsc isn`t allocated\n");
		mutex_unlock(&rot_desc.usage_count_lock);
		return;
	}

	if (ch->node.prepare_t != ch->node.working_t)
		pr_info("prepare_t(%d) working_t(%d) isn`t equal\n", ch->node.prepare_t, ch->node.working_t);

	vppu_close(ch->vppu_id);

	vfree(ch);

	rot_desc.usage_count = 0;

	mutex_unlock(&rot_desc.usage_count_lock);

	return;
}

void rot_rsc_init(void (*cb_used_done)(struct dss_node_id *node_id, bool rsc_free, struct dss_image *input_image, struct dss_image *output_image), 
		void (*cb_ready_to_work)(struct dss_node_id *node_id, struct dss_node *node), 
		unsigned long (*cb_rot_paddr_get)(unsigned long paddr))
{
	mutex_init(&rot_desc.usage_count_lock);

	rot_desc.usage_count = 0;
	rot_desc.cb_used_done = cb_used_done;
	rot_desc.cb_ready_to_work = cb_ready_to_work;
	rot_desc.cb_rot_paddr_get = cb_rot_paddr_get;
}

void rot_rsc_cleanup(void)
{
	rot_desc.cb_used_done = NULL;
	rot_desc.cb_ready_to_work = NULL;
}
