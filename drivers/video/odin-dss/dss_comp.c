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
 * MERHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#include "hal/dss_hal_ch_id.h"
#include "rot_rsc_drv.h"
#include "du_rsc.h"
#include "du_ovl_drv.h"
#include "sync_src.h"

#define	DSS_COMP_MAX_RSC_NUM			(DU_SRC_NUM + ROT_SRC_NUM)
#define	DSS_COMP_INSTANCE_MAGIC_CODE		0xD1894170

#define DSS_COMP_CLOSING_WAIT_TIMEOUT_MS	3000

#define	DSS_MAX_PIPE_DEPTH			3

enum dss_comp_node_property
{
	// off-line
	DSS_COMP_NODE_VSP_WB = 0,	// rotate
	// on-line
	DSS_COMP_NODE_DU_WB,		// scale down
	DSS_COMP_NODE_DU,

	DSS_COMP_NODE_NUM,
	DSS_COMP_NODE_INVALID,
};

struct dss_comp_node
{
	enum dss_comp_node_property property;
	enum dss_image_format pixel_format;
	bool using;
	union
	{
		struct
		{
			enum du_src_ch_id rsc_id;
		} on;
		struct
		{
			void *rsc_id;
		} off;
	} rsc;
};

struct dss_comp_layer
{
	unsigned int sum_of_node_pic_size;

	struct dss_comp_node comp_node[DSS_MAX_PIPE_DEPTH];
	unsigned int num_of_node;
};

struct dss_comp_layers
{
	struct list_head list;

	unsigned int layers_id;
	unsigned int total_pic_size;

	struct dss_comp_layer comp_layer[DSS_COMP_MAX_RSC_NUM];
	unsigned int num_of_layer;

	enum sync_ch_id sync_ch;
	bool command_mode;
};

struct dss_comp_instance
{
	struct list_head list;

	void *handle;

	struct dss_comp_layers *working_layers;

	struct mutex worked_layers_list_lock;
	struct list_head worked_layers_list;

	struct workqueue_struct *buf_done_wq;

	unsigned int layers_id_gen;
	unsigned int magic_code;

	struct completion close_completion;

	struct
	{
		int nr_send_node;
		int nr_rcv_node;
	}debug;
};

struct dss_comp_db
{
	struct list_head instance_list;
	struct mutex instance_list_lock;
	unsigned long (*buf_alloc)( const size_t size );
	void (*buf_free)( const unsigned long paddr );
	void (*buf_done)(void *handle, const unsigned long paddr);
} comp_db;

struct dss_comp_buf_done_work
{
	struct dss_node_id node_id;
	unsigned long paddr;
	bool ext_in_buf;
	bool rsc_free;

	struct work_struct buf_done_work;
};

static inline void _dss_comp_node_print(struct dss_node_id *node_id, struct dss_node *node)
{
	pr_info("--node--\n");
	pr_info("\tlayer_id:%d node_id:%d\n", node_id->layer_id, node_id->node_id);
	pr_info("--input--\n");
	pr_info("\tiamge size :%d %d\n", node->input.image.image_size.w, node->input.image.image_size.h);
	pr_info("\tformat :%d zorder:%d global_alpha:%d\n", node->input.image.pixel_format, node->input.in_meta.zorder, node->input.in_meta.global_alpha);
	pr_info("\tcrop %d %d %d %d\n", node->input.in_meta.crop_rect.pos.x, node->input.in_meta.crop_rect.pos.x, node->input.in_meta.crop_rect.size.w, node->input.in_meta.crop_rect.size.h);
	pr_info("\tdst %d %d %d %d\n", node->input.in_meta.dst_rect.pos.x, node->input.in_meta.dst_rect.pos.x, node->input.in_meta.dst_rect.size.w, node->input.in_meta.dst_rect.size.h);
	pr_info("--output--\n");
	pr_info("\tiamge size :%d %d\n", node->output.image.image_size.w, node->output.image.image_size.h);
	pr_info("\tport :%d\n\n", node->output.image.port);
}

static void _dss_comp_worked_layers_free(struct dss_comp_instance *comp_instance, struct dss_comp_layers *layers)
{
	int layer_i = 0;
	struct dss_comp_layer *layer = NULL;

	mutex_lock(&comp_instance->worked_layers_list_lock);
	list_del(&layers->list);
	mutex_unlock( &comp_instance->worked_layers_list_lock );

	pr_info("worked layers free layers_id:%d)\n", layers->layers_id);

	for (layer_i = 0; layer_i < layers->num_of_layer; layer_i++ ) {
		int node_i = 0;

		layer = &layers->comp_layer[layer_i];
		for (node_i = 0; node_i < layer->num_of_node; node_i++) {
			switch(layer->comp_node[node_i].property) {
			case DSS_COMP_NODE_VSP_WB:
				rot_rsc_free(layer->comp_node[node_i].rsc.off.rsc_id);
				layer->comp_node[node_i].rsc.off.rsc_id = NULL;
				break;
			case DSS_COMP_NODE_DU_WB:
			case DSS_COMP_NODE_DU:
				du_rsc_ch_free(layer->comp_node[node_i].rsc.on.rsc_id);
				layer->comp_node[node_i].rsc.on.rsc_id = DU_SRC_INVALID;
				break;
			default:
				break;
			}
		}
	}

	vfree( layers );

	if (comp_instance->working_layers == NULL) {
		complete_all(&comp_instance->close_completion);
	}

	return;
}

static bool _dss_comp_layers_node_unused(struct dss_comp_layers *layers)
{
	int layer_i = 0;
	struct dss_comp_layer *layer = NULL;

	for (layer_i = 0; layer_i < layers->num_of_layer; layer_i++ ) {
		int node_i = 0;

		layer = &layers->comp_layer[layer_i];
		for (node_i = 0; node_i < layer->num_of_node; node_i++) {
			if (layer->comp_node[node_i].using == true) {
				return false;
			}
		}
	}

	return true;
}

static void _dss_comp_node_using_print(struct dss_comp_layers *layers)
{
	int layer_i = 0;
	struct dss_comp_layer *layer = NULL;

	for (layer_i = 0; layer_i < layers->num_of_layer; layer_i++ ) {
		int node_i = 0;

		layer = &layers->comp_layer[layer_i];
		for (node_i = 0; node_i < layer->num_of_node; node_i++) {
			if (layer->comp_node[node_i].using == true) {
				pr_info("node:%d rsc:%d is using\n", node_i, layer->comp_node[node_i].rsc.on.rsc_id);
			}
		}
	}
}

static void _dss_comp_buf_done_wq(struct work_struct *w)
{
	struct dss_comp_buf_done_work *work = NULL;
	struct dss_comp_instance *instance = NULL;
	struct dss_comp_layers *layers = NULL;
	struct dss_comp_layer *comp_layer = NULL;
	struct dss_comp_node *comp_node_curr = NULL;

	if (w == NULL) {
		pr_err("work_struct is NULL\n");
		return;
	}

	work = container_of(w, struct dss_comp_buf_done_work, buf_done_work);
	instance = (struct dss_comp_instance *)work->node_id.instance_id;
	layers = (struct dss_comp_layers *)work->node_id.layers_id;
	comp_layer = &layers->comp_layer[work->node_id.layer_id];
	comp_node_curr = &comp_layer->comp_node[work->node_id.node_id];

	if (work->paddr) {
		if (work->ext_in_buf == true)
			comp_db.buf_done(instance->handle, work->paddr);
		else
			comp_db.buf_free(work->paddr);
	}

	if (work->rsc_free) {
		comp_node_curr->using = false;
		if (_dss_comp_layers_node_unused(layers) == true)
			_dss_comp_worked_layers_free(instance, layers);
	}

	kfree(work);
}

static void _dss_com_buf_done_cb(struct dss_node_id *node_id, bool rsc_free, bool ext_in_buf, unsigned long paddr)
{
	struct dss_comp_buf_done_work *work = NULL;
	struct dss_comp_instance *instance = (struct dss_comp_instance *)node_id->instance_id;
	unsigned long flags = 0;

	work = kzalloc(sizeof(struct dss_comp_buf_done_work), GFP_ATOMIC);
	if (work == NULL) {
		pr_err("kzalloc failed size:%d\n", sizeof(struct dss_comp_buf_done_work));
		return ;
	}

	work->node_id = *node_id;
	work->ext_in_buf = ext_in_buf;
	work->paddr = paddr;
	work->rsc_free = rsc_free;
	INIT_WORK(&work->buf_done_work, _dss_comp_buf_done_wq);

	queue_work(instance->buf_done_wq, &work->buf_done_work);

}

static bool _dss_comp_do(struct dss_comp_node *comp_node, struct dss_node_id *node_id, struct dss_node *node, enum sync_ch_id sync_ch, bool command_mode, bool from_offline)
{
	bool ret = false;

	switch (comp_node->property) {
	case DSS_COMP_NODE_DU_WB:
	case DSS_COMP_NODE_DU:
		ret = du_ovl_do_img(comp_node->rsc.on.rsc_id, node_id, node, sync_ch, command_mode, !from_offline);
		break;
	case DSS_COMP_NODE_VSP_WB:
		ret = rot_rsc_do(comp_node->rsc.off.rsc_id, node_id, node);
		break;
	default :
		pr_err("comp_node->property:%d\n", comp_node->property);
		break;
	}

	return ret;
}

bool _dss_comp_is_same_layers_shape(struct dss_comp_layers *working_layers, struct dss_comp_layers *new_layers)
{
#define _dss_comp_pixel_format_rgb(pixel_format) ((pixel_format) <=DSS_IMAGE_FORMAT_RGB_565)
	unsigned int layer_i = 0, node_i = 0;
	struct dss_comp_layer *working_layer = NULL, *new_layer = NULL;

	if (working_layers == NULL) {
		//pr_info("first commit or closing\n");
		return false;
	}

	if (working_layers->num_of_layer != new_layers->num_of_layer) {
		pr_info( "num_of_layer - working_layers:%u --> working_layers:%u\n", working_layers->num_of_layer, new_layers->num_of_layer );
		return false;
	}

	for (layer_i = 0; layer_i < working_layers->num_of_layer; layer_i++ ) {
		working_layer = &working_layers->comp_layer[layer_i];
		new_layer = &new_layers->comp_layer[layer_i];

		if (working_layer->num_of_node != new_layer->num_of_node) {
			pr_info( "num_of_node - working_layer:%u --> working_layer:%u\n", working_layer->num_of_node, new_layer->num_of_node );
			return false;
		}

		for (node_i = 0; node_i < working_layer->num_of_node; node_i++ ) {
			if (working_layer->comp_node[node_i].property != new_layer->comp_node[node_i].property) {
				pr_info( "property - working_node:%u --> working_node:%u\n", working_layer->comp_node[node_i].property, new_layer->comp_node[node_i].property );
				return false;
			}
			if (working_layer->comp_node[node_i].pixel_format != new_layer->comp_node[node_i].pixel_format) {
				pr_info( "pixel_format - working_node:%u --> working_node:%u\n", working_layer->comp_node[node_i].pixel_format, new_layer->comp_node[node_i].pixel_format );
				return false;
			}

			if (_dss_comp_pixel_format_rgb(working_layer->comp_node[node_i].pixel_format) != 
					_dss_comp_pixel_format_rgb(new_layer->comp_node[node_i].pixel_format)) {
				pr_info( "pixel_format - working_node:%u --> working_node:%u\n", working_layer->comp_node[node_i].pixel_format, new_layer->comp_node[node_i].pixel_format );
				return false;
			}
		}
	}

	return true;
}

static void _dss_comp_used_done_cb(struct dss_node_id *node_id, bool rsc_free, struct dss_image *input, struct dss_image *output)
{
	struct dss_comp_instance *instance;
	struct dss_comp_layers *layers;
	struct dss_comp_layer *comp_layer;
	struct dss_comp_node *comp_node_curr;
	unsigned int node_id_next;

	if (node_id == NULL) {
		pr_err("invalid node_id\n");
		return;
	}

	instance = (struct dss_comp_instance *)node_id->instance_id;
	layers = (struct dss_comp_layers *)node_id->layers_id;
	comp_layer = &layers->comp_layer[node_id->layer_id];
	comp_node_curr = &comp_layer->comp_node[node_id->node_id];
	node_id_next = node_id->node_id + 1;

	//pr_info("used done cb %s instance:0x%X, layers:%u, layer:%u, node:%u rsc_free:%s\n", __FUNCTION__, (unsigned int)instance, layers->layers_id, node_id->layer_id, node_id->node_id, rsc_free ? "true" : "false");
	//pr_info("used done layer:%u, node:%u rsc_id:%d rsc_free:%s\n", node_id->layer_id, node_id->node_id, comp_node_curr->rsc.on.rsc_id, rsc_free ? "true" : "false");

	if (input)
		_dss_com_buf_done_cb(node_id, false, input->ext_buf, input->phy_addr);

	if (output && node_id_next == comp_layer->num_of_node) {
		if (output->port == DSS_DISPLAY_PORT_MEM)
			_dss_com_buf_done_cb(node_id, false, output->ext_buf, output->phy_addr);
	}

	if (rsc_free)
		_dss_com_buf_done_cb(node_id, true, false, 0);
}

static void _dss_comp_ready_to_work_cb(struct dss_node_id *node_id, struct dss_node *node, bool from_offline)
{
	struct dss_comp_instance *instance = (struct dss_comp_instance *)node_id->instance_id;
	struct dss_comp_layers *layers = (struct dss_comp_layers *)node_id->layers_id;
	struct dss_comp_layer *comp_layer = &layers->comp_layer[node_id->layer_id];
	unsigned int node_id_next = node_id->node_id + 1;

	//pr_info("ready_to_work_cb %s instance:0x%X, layers:%u, layer:%u, node:%u\n", __FUNCTION__, (unsigned int)instance, layers->layers_id, node_id->layer_id, node_id->node_id);
	//_dss_comp_node_print(node_id, node);

	if (node == NULL || node_id == NULL) {
		pr_err("Invalid node(0x%08X) or node_id(0x%08X)\n", (int)node, (int)node_id);
		return;
	}

	if (node_id_next < comp_layer->num_of_node) {
		struct dss_comp_node *comp_node_next = &comp_layer->comp_node[node_id_next];

		if (!node->undo && _dss_comp_is_same_layers_shape(instance->working_layers, layers) == false)
			node->undo = true;

		node->input.image.phy_addr = node->int_out_buf[node_id_next-1];
		node->int_out_buf[node_id_next-1]= 0;
		node->input.image.ext_buf = false;

		node_id->node_id = node_id_next;
		comp_node_next->using = true;

		_dss_comp_do(comp_node_next, node_id, node, layers->sync_ch, layers->command_mode, from_offline);
	}
}

static void dss_comp_ready_to_work_cb_offline(struct dss_node_id *_node_id, struct dss_node *_node)
{
	struct dss_node_id node_id = *_node_id;
	struct dss_node node = *_node;

	_dss_comp_ready_to_work_cb(&node_id, &node, true);
}

static void dss_comp_ready_to_work_cb_online(struct dss_node_id *_node_id, struct dss_node *_node)
{
	struct dss_node_id node_id = *_node_id;
	struct dss_node node = *_node;

	_dss_comp_ready_to_work_cb(&node_id, &node, false);
}


static bool __dss_comp_make_nodes_shape(struct dss_comp_layer *comp_layer, struct dss_image *input_image, struct dss_input_meta *input_meta)
{
	bool remaing_node = true;

	comp_layer->num_of_node = 0;
	comp_layer->sum_of_node_pic_size = 0;

	// color_format:
	comp_layer->comp_node[comp_layer->num_of_node].pixel_format = input_image->pixel_format;

	// case rotate:
	if (input_meta->rotate_op != DSS_IMAGE_OP_ROTATE_NONE) {

		if (input_meta->blend_op == DSS_IMAGE_OP_BLEND_NONE) {
			remaing_node = false;
		}

		comp_layer->comp_node[comp_layer->num_of_node].property = DSS_COMP_NODE_VSP_WB;
		comp_layer->comp_node[comp_layer->num_of_node].rsc.off.rsc_id = NULL;
		comp_layer->num_of_node++;
	}

	// case scale down:
	if (input_meta->crop_rect.size.w * input_meta->crop_rect.size.h >
			input_meta->dst_rect.size.w * input_meta->dst_rect.size.h) {

		if (input_meta->blend_op == DSS_IMAGE_OP_BLEND_NONE) {
			remaing_node = false;
		}

		comp_layer->comp_node[comp_layer->num_of_node].property = DSS_COMP_NODE_DU_WB;
		comp_layer->comp_node[comp_layer->num_of_node].rsc.on.rsc_id = DU_SRC_INVALID;
		comp_layer->num_of_node++;
	}

	// default:
	if (remaing_node == true) {
		comp_layer->comp_node[comp_layer->num_of_node].property = DSS_COMP_NODE_DU;
		comp_layer->comp_node[comp_layer->num_of_node].rsc.on.rsc_id = DU_SRC_INVALID;
		comp_layer->num_of_node++;
	}

	return true;
}

struct dss_comp_layers *_dss_comp_make_layers_shape( struct dss_image input_image[],
		struct dss_input_meta input_meta[],
		enum dss_image_assign_res assign_req[],
		unsigned int num_of_layer,
		struct dss_image *output_image,
		struct dss_output_meta *output_meta)
{
	struct dss_comp_layers *new_layers = NULL;
	unsigned int layer_i = 0;
	bool include_reject = false;

	if (num_of_layer >= DSS_COMP_MAX_RSC_NUM) {
		pr_warning("over num_of_layer:%d \n", num_of_layer);
		return 0;
	}

	new_layers = (struct dss_comp_layers *)vzalloc( sizeof(struct dss_comp_layers) );
	if (new_layers == NULL) {
		pr_err( "failed to alloc \n" );
		return (struct dss_comp_layers *)NULL;
	}

	new_layers->total_pic_size = 0;
	new_layers->num_of_layer = num_of_layer;
	new_layers->sync_ch = SYNC_CH_INVALID;

	for (layer_i = 0; layer_i < num_of_layer; layer_i++ ) {
		if (__dss_comp_make_nodes_shape(&new_layers->comp_layer[layer_i], &input_image[layer_i], &input_meta[layer_i]) == false) {
			pr_warning("reject layer(%d)\n", layer_i);
			assign_req[layer_i] = DSS_IMAGE_ASSIGN_REJECTED;
			include_reject = true;
		}
		else {
			assign_req[layer_i] = DSS_IMAGE_ASSIGN_RESERVED;
		}
	}

	if (include_reject == true) {
		for (layer_i = 0; layer_i < num_of_layer; layer_i++ ) {
			assign_req[layer_i] = DSS_IMAGE_ASSIGN_REJECTED;
		}

		vfree(new_layers);
		return NULL;
	}

	return new_layers;
}

static void _dss_comp_undo_node(struct dss_comp_instance *instance, void *layers_id, unsigned int layer_id, unsigned int node_id_id, struct dss_node_id *node_id, struct dss_node *node)
{
	memset(node_id, 0, sizeof(struct dss_node_id));
	memset(node, 0, sizeof(struct dss_node));

	node_id->instance_id = instance;
	node_id->layers_id = layers_id;
	node_id->layer_id = layer_id;
	node_id->node_id = node_id_id;

	node->undo = true;
}

static void _dss_comp_undo_first_node(struct dss_comp_instance *instance, struct dss_comp_layers *worked_layers)
{
	unsigned int layer_i = 0;
	unsigned int comp_i = 0;
	struct dss_comp_node *comp_node = NULL;
	struct dss_node_id node_id[DU_SRC_NUM];
	struct dss_node node[DU_SRC_NUM];

	memset(node_id, 0, sizeof(node_id));
	memset(node, 0, sizeof(node));

	for (layer_i=0; layer_i<worked_layers->num_of_layer; layer_i++ ) {
		for (comp_i=0; comp_i<worked_layers->comp_layer[layer_i].num_of_node; comp_i++) {
			comp_node = &worked_layers->comp_layer[layer_i].comp_node[comp_i];
			comp_node->using = true;

			if (comp_node->property == DSS_COMP_NODE_VSP_WB) {
				struct dss_node_id off_rsc_node_id;
				struct dss_node off_rsc_node;

				memset(&off_rsc_node_id, 0, sizeof(struct dss_node_id));
				memset(&off_rsc_node, 0, sizeof(struct dss_node));

				_dss_comp_undo_node(instance, (void*)worked_layers, layer_i, comp_i, &off_rsc_node_id, &off_rsc_node);
				rot_rsc_do(comp_node->rsc.off.rsc_id, &off_rsc_node_id, &off_rsc_node);
			}
			else {
				_dss_comp_undo_node(instance, (void*)worked_layers, layer_i, comp_i, &node_id[comp_node->rsc.on.rsc_id], &node[comp_node->rsc.on.rsc_id]);
			}
		}
	}

	du_ovl_do_imgs(node_id, node, worked_layers->sync_ch, worked_layers->command_mode);

	return;
}

bool _dss_comp_alloc_layers_nodes(struct dss_comp_layers *layers, unsigned int layers_id, struct dss_image input_image[], struct dss_input_meta	input_meta[], struct dss_image *output_image)
{
	unsigned int layer_i = 0, node_i = 0;
	struct dss_comp_layer *layer = NULL;
	struct dss_image input_image_temp;
	struct dss_input_meta input_meta_temp;
	struct dss_image output_image_temp;
	unsigned int temp = 0;

	memset(&input_image_temp, 0, sizeof(struct dss_image));
	memset(&input_meta_temp, 0, sizeof(struct dss_input_meta));
	memset(&output_image_temp, 0, sizeof(struct dss_image));

	layers->total_pic_size = 0;
	layers->layers_id = layers_id;

	for (layer_i = 0; layer_i < layers->num_of_layer; layer_i++ ) {
		layer = &layers->comp_layer[layer_i];

		input_image_temp = input_image[layer_i];
		input_meta_temp = input_meta[layer_i];
		output_image_temp = *output_image;

		for (node_i = 0; node_i < layer->num_of_node; node_i++ ) {
			switch (layer->comp_node[node_i].property) {
			case DSS_COMP_NODE_VSP_WB:
				if (input_image_temp.pixel_format != DSS_IMAGE_FORMAT_YUV420_2P && 
						input_image_temp.pixel_format != DSS_IMAGE_FORMAT_YUV420_3P) {
					pr_warning("pixel_format(%d) is out of off-line rsc capability", input_image_temp.pixel_format);
					goto _alloc_failed;
				}

				layer->comp_node[node_i].rsc.off.rsc_id = rot_rsc_alloc();
				if (layer->comp_node[node_i].rsc.off.rsc_id == NULL) {
					pr_warning("failed to alloc off-line rsc node\n");
					goto  _alloc_failed;
				}

				switch (input_meta_temp.rotate_op) {
				case DSS_IMAGE_OP_ROTATE_90:
					input_meta_temp.crop_rect.pos.x = input_image_temp.image_size.h - input_meta_temp.crop_rect.pos.y
							- input_meta_temp.crop_rect.size.h;
					input_meta_temp.crop_rect.pos.y = input_meta_temp.crop_rect.pos.x;
					break;
				case DSS_IMAGE_OP_ROTATE_180:
					input_meta_temp.crop_rect.pos.x = input_image_temp.image_size.w - input_meta_temp.crop_rect.pos.x
							- input_meta_temp.crop_rect.size.w;
					input_meta_temp.crop_rect.pos.y = input_image_temp.image_size.h - input_meta_temp.crop_rect.pos.y
							- input_meta_temp.crop_rect.size.h;
					break;
				case DSS_IMAGE_OP_ROTATE_270:
					input_meta_temp.crop_rect.pos.x = input_meta_temp.crop_rect.pos.y;
					input_meta_temp.crop_rect.pos.y = input_image_temp.image_size.w - input_meta_temp.crop_rect.pos.x
							- input_meta_temp.crop_rect.size.w;
					break;
				default:
					pr_err("rotate_op:%d\n", input_meta_temp.rotate_op);
					break;
				}

				switch (input_meta_temp.rotate_op) {
				case DSS_IMAGE_OP_ROTATE_90:
				case DSS_IMAGE_OP_ROTATE_270:
					temp = input_meta_temp.crop_rect.size.h;
					input_meta_temp.crop_rect.size.h = input_meta_temp.crop_rect.size.w;
					input_meta_temp.crop_rect.size.w = temp;
					break;
				case DSS_IMAGE_OP_ROTATE_180:
					break;
				default :
					pr_err("rotate_op:%d\n", input_meta_temp.rotate_op);
					break;
				}

				layers->total_pic_size += (input_meta_temp.crop_rect.size.w * input_meta_temp.crop_rect.size.h);
				layers->total_pic_size += (input_meta_temp.dst_rect.size.w * input_meta_temp.dst_rect.size.h);
				break;

			case DSS_COMP_NODE_DU_WB:
				input_meta_temp.dst_rect.pos.x = 0;
				input_meta_temp.dst_rect.pos.y = 0;

				output_image_temp.port = DSS_DISPLAY_PORT_MEM;
				output_image_temp.image_size = input_meta_temp.dst_rect.size;
				output_image_temp.ext_buf = false;
				output_image_temp.phy_addr = 0x0;

				switch (input_image_temp.pixel_format) {
				case DSS_IMAGE_FORMAT_RGBA_8888:
				case DSS_IMAGE_FORMAT_RGBX_8888:
				case DSS_IMAGE_FORMAT_BGRA_8888:
				case DSS_IMAGE_FORMAT_BGRX_8888:
				case DSS_IMAGE_FORMAT_ARGB_8888:
				case DSS_IMAGE_FORMAT_XRGB_8888:
				case DSS_IMAGE_FORMAT_ABGR_8888:
				case DSS_IMAGE_FORMAT_XBGR_8888:
				case DSS_IMAGE_FORMAT_RGBA_5551:
				case DSS_IMAGE_FORMAT_RGBX_5551:
				case DSS_IMAGE_FORMAT_BGRA_5551:
				case DSS_IMAGE_FORMAT_BGRX_5551:
				case DSS_IMAGE_FORMAT_ARGB_1555:
				case DSS_IMAGE_FORMAT_XRGB_1555:
				case DSS_IMAGE_FORMAT_ABGR_1555:
				case DSS_IMAGE_FORMAT_XBGR_1555:
				case DSS_IMAGE_FORMAT_RGBA_4444:
				case DSS_IMAGE_FORMAT_RGBX_4444:
				case DSS_IMAGE_FORMAT_BGRA_4444:
				case DSS_IMAGE_FORMAT_BGRX_4444:
				case DSS_IMAGE_FORMAT_ARGB_4444:
				case DSS_IMAGE_FORMAT_XRGB_4444:
				case DSS_IMAGE_FORMAT_ABGR_4444:
				case DSS_IMAGE_FORMAT_XBGR_4444:
					output_image_temp.pixel_format = input_image_temp.pixel_format;
					break;
				case DSS_IMAGE_FORMAT_YUV422_S:
				case DSS_IMAGE_FORMAT_YUV422_2P:
				case DSS_IMAGE_FORMAT_YUV224_2P:
				case DSS_IMAGE_FORMAT_YUV420_2P:
				case DSS_IMAGE_FORMAT_YUV444_2P:
				case DSS_IMAGE_FORMAT_YUV422_3P:
				case DSS_IMAGE_FORMAT_YUV224_3P:
				case DSS_IMAGE_FORMAT_YUV420_3P:
				case DSS_IMAGE_FORMAT_YUV444_3P:
				case DSS_IMAGE_FORMAT_YV_12:
					output_image_temp.pixel_format = DSS_IMAGE_FORMAT_RGBA_8888;
					break;
				case DSS_IMAGE_FORMAT_INVALID:
				default:
					pr_err("invalid input pixel format\n");
					break;
				}

				layer->comp_node[node_i].rsc.on.rsc_id = du_rsc_ch_alloc(&input_image_temp, &input_meta_temp, &output_image_temp);
				if (layer->comp_node[node_i].rsc.on.rsc_id == DU_SRC_INVALID) {
					pr_warning("failed to alloc on-line rsc node\n");
					goto  _alloc_failed;
				}

				input_image_temp = output_image_temp;
				input_meta_temp.crop_rect.pos.x = 0;
				input_meta_temp.crop_rect.pos.y = 0;
				input_meta_temp.crop_rect.size = input_meta_temp.dst_rect.size;
				output_image_temp = *output_image;

				layers->total_pic_size += (input_meta_temp.crop_rect.size.w * input_meta_temp.crop_rect.size.h);
				layers->total_pic_size += (input_meta_temp.dst_rect.size.w * input_meta_temp.dst_rect.size.h);
				break;

			case DSS_COMP_NODE_DU:
				layer->comp_node[node_i].rsc.on.rsc_id = du_rsc_ch_alloc(&input_image_temp, &input_meta_temp, &output_image_temp);
				if (layer->comp_node[node_i].rsc.on.rsc_id == DU_SRC_INVALID) {
					pr_warning("failed to alloc on-line rsc node\n");
					goto  _alloc_failed;
				}

				layers->total_pic_size += (input_meta_temp.crop_rect.size.w * input_meta_temp.crop_rect.size.h);
				layers->total_pic_size += (input_meta_temp.dst_rect.size.w * input_meta_temp.dst_rect.size.h);
				break;

			default:
				pr_info( "node property:%u\n", layer->comp_node[node_i].property );
				break;
			}
		}
	}

	return true;

_alloc_failed:

	for (layer_i = 0; layer_i < layers->num_of_layer; layer_i++ ) {
		layer = &layers->comp_layer[layer_i];

		for (node_i = 0; node_i < layer->num_of_node; node_i++ ) {
			switch (layer->comp_node[node_i].property) {
			case DSS_COMP_NODE_VSP_WB:
				if (layer->comp_node[node_i].rsc.off.rsc_id != NULL) {
					rot_rsc_free(layer->comp_node[node_i].rsc.off.rsc_id);
					layer->comp_node[node_i].rsc.off.rsc_id = NULL;
				}
				break;
			case DSS_COMP_NODE_DU_WB:
			case DSS_COMP_NODE_DU:
				if (layer->comp_node[node_i].rsc.on.rsc_id != DU_SRC_INVALID) {
					du_rsc_ch_free(layer->comp_node[node_i].rsc.on.rsc_id);
					layer->comp_node[node_i].rsc.on.rsc_id = DU_SRC_INVALID;
				}
				break;
			default:
				pr_info( "node property:%u\n", layer->comp_node[node_i].property );
				break;
			}
		}
	}

	return false;
}

static struct dss_comp_layers *_dss_comp_replace_working_layers(struct dss_comp_instance *instance, struct dss_comp_layers *new_layers)
{
	struct dss_comp_layers *worked_layers = instance->working_layers;

	pr_info("%s \n", __FUNCTION__);
	instance->working_layers = new_layers;

	if (worked_layers == NULL)
		return NULL;

	mutex_lock( &instance->worked_layers_list_lock );
	list_add_tail(&worked_layers->list, &instance->worked_layers_list);
	mutex_unlock( &instance->worked_layers_list_lock );

	return worked_layers;
}

static void _dss_comp_do_node(struct dss_comp_instance *instance, void *layers_id, unsigned int layer_id, struct dss_image *input_image, struct dss_input_meta *input_meta, struct dss_image *output_image, struct dss_output_meta *output_meta, struct dss_node_id *node_id, struct dss_node *node, int num_of_node)
{
	node_id->instance_id = instance;
	node_id->layers_id = (void*)layers_id;
	node_id->layer_id = layer_id;
	node_id->node_id = 0;

	node->input.image = *input_image;
	node->input.in_meta= *input_meta;
	node->output.image = *output_image;
	node->output.out_meta = *output_meta;

	if (num_of_node > 1) {
		int int_out_buf_i = 0;
		for (int_out_buf_i=0; int_out_buf_i<num_of_node-1; int_out_buf_i++)
			node->int_out_buf[int_out_buf_i] = comp_db.buf_alloc(input_meta->dst_rect.size.w * input_meta->dst_rect.size.h * 4);
	}
}

static void _dss_comp_do_first_node(struct dss_comp_instance *instance, struct dss_comp_layers *worked_layers, enum sync_ch_id sync_ch, bool command_mode, struct dss_image input_image[], struct dss_input_meta input_meta[], struct dss_image *output_image, struct dss_output_meta *output_meta)
{
	unsigned int layer_i = 0;
	unsigned int comp_i = 0;
	struct dss_comp_node *comp_node = NULL;
	struct dss_node_id node_id[DU_SRC_NUM];
	struct dss_node node[DU_SRC_NUM];

	memset(node_id, 0, sizeof(node_id));
	memset(node, 0, sizeof(node));

	if (worked_layers) {
		for (layer_i=0; layer_i<worked_layers->num_of_layer; layer_i++ ) {
			for (comp_i=0; comp_i<worked_layers->comp_layer[layer_i].num_of_node; comp_i++) {
				comp_node = &worked_layers->comp_layer[layer_i].comp_node[comp_i];
				comp_node->using = true;

				if (comp_node->property == DSS_COMP_NODE_VSP_WB) {
					struct dss_node_id off_rsc_node_id;
					struct dss_node off_rsc_node;

					memset(&off_rsc_node_id, 0, sizeof(struct dss_node_id));
					memset(&off_rsc_node, 0, sizeof(struct dss_node));
					_dss_comp_undo_node(instance, (void*)worked_layers, layer_i, comp_i, &off_rsc_node_id, &off_rsc_node);
					rot_rsc_do(comp_node->rsc.off.rsc_id, &off_rsc_node_id, &off_rsc_node);
				}
				else {
					_dss_comp_undo_node(instance, (void*)worked_layers, layer_i, comp_i, &node_id[comp_node->rsc.on.rsc_id], &node[comp_node->rsc.on.rsc_id]);
				}
			}
		}
	}

	if (instance->working_layers) {
		instance->working_layers->sync_ch = sync_ch;
		instance->working_layers->command_mode = command_mode;

		for (layer_i=0; layer_i<instance->working_layers->num_of_layer; layer_i++ ) {
			comp_node = &instance->working_layers->comp_layer[layer_i].comp_node[0];
			comp_node->using = true;

			if (comp_node->property == DSS_COMP_NODE_VSP_WB) {
				struct dss_node_id off_rsc_node_id;
				struct dss_node off_rsc_node;

				memset(&off_rsc_node_id, 0, sizeof(struct dss_node_id));
				memset(&off_rsc_node, 0, sizeof(struct dss_node));
				_dss_comp_do_node(instance, instance->working_layers, layer_i, &input_image[layer_i], &input_meta[layer_i], output_image, output_meta, 
						&off_rsc_node_id, &off_rsc_node, instance->working_layers->comp_layer[layer_i].num_of_node);
				rot_rsc_do(comp_node->rsc.off.rsc_id, &off_rsc_node_id, &off_rsc_node);

			}
			else{
				_dss_comp_do_node(instance, instance->working_layers, layer_i, &input_image[layer_i], &input_meta[layer_i], output_image, output_meta, 
						&node_id[comp_node->rsc.on.rsc_id], &node[comp_node->rsc.on.rsc_id], instance->working_layers->comp_layer[layer_i].num_of_node);
			}
		}
	}

	du_ovl_do_imgs(node_id, node, sync_ch, command_mode);
}

unsigned int dss_comp_update_layer_imgs( void *instance_id, struct dss_image input_image[], struct dss_input_meta input_meta[], enum dss_image_assign_res assign_req[], unsigned int num_of_layer, struct dss_image *output_image, struct dss_output_meta *output_meta, enum sync_ch_id sync_ch, bool command_mode )
{
	struct dss_comp_instance *instance = (struct dss_comp_instance *)instance_id;
	struct dss_comp_layers *new_layers = NULL;
	struct dss_comp_layers *worked_layers = NULL;
	unsigned int layer_i = 0;
	bool reject = false;

	if (instance->magic_code != DSS_COMP_INSTANCE_MAGIC_CODE) {
		pr_warning("not matched magic code(0x%X)\n", instance->magic_code);
		return 0;
	}

	new_layers = _dss_comp_make_layers_shape(input_image, input_meta, assign_req,  num_of_layer, output_image, output_meta);
	if (new_layers == NULL) {
		pr_warning("can not setup the shape (layer:%d)\n", num_of_layer);
		return 0;
	}

	if (instance->working_layers && _dss_comp_is_same_layers_shape(instance->working_layers, new_layers) == true) {
		vfree(new_layers);
	}
	else if (_dss_comp_alloc_layers_nodes(new_layers, instance->layers_id_gen,
				input_image, input_meta, output_image) == true) {
		instance->layers_id_gen++;
		worked_layers = _dss_comp_replace_working_layers(instance, new_layers);
	}
	else {
		vfree(new_layers);
		reject = true;
	}

	if (reject) {
		for (layer_i = 0; layer_i < num_of_layer; layer_i++ ) {
			assign_req[layer_i] = DSS_IMAGE_ASSIGN_REJECTED;
		}

		return 0;
	}
	else {
		_dss_comp_do_first_node(instance, worked_layers, sync_ch, command_mode, input_image, input_meta, output_image, output_meta);
		return instance->working_layers->total_pic_size;
	}
}

void * dss_comp_open(void *handle)
{
	struct dss_comp_instance *instance = NULL;

	pr_info("%s\n", __FUNCTION__);

	instance = (struct dss_comp_instance *)vzalloc( sizeof(struct dss_comp_instance) );
	if (instance == NULL) {
		pr_err( "memory alloc failed!\n" );
		return NULL;
	}

	instance->buf_done_wq = create_singlethread_workqueue("dss_com_buf_done");
	if (IS_ERR_OR_NULL(instance->buf_done_wq)) {
		pr_err("create_singlethread_workqueue failed errno:%d\n", (int)instance->buf_done_wq);
		vfree( instance );
		return NULL;
	}

	init_completion(&instance->close_completion);
	mutex_init( &instance->worked_layers_list_lock );
	INIT_LIST_HEAD( &instance->worked_layers_list );

	instance->handle = handle;
	instance->working_layers = NULL;
	instance->layers_id_gen = 0x0;
	instance->magic_code = DSS_COMP_INSTANCE_MAGIC_CODE;

	mutex_lock( &comp_db.instance_list_lock );

	list_add_tail( &instance->list, &comp_db.instance_list );

	mutex_unlock( &comp_db.instance_list_lock );

	return instance;
}

bool dss_comp_close( void *instance_id )
{
	struct dss_comp_instance *instance = (struct dss_comp_instance *)instance_id;

	pr_info("%s start\n", __FUNCTION__);

	mutex_lock(&comp_db.instance_list_lock );

	list_del( &instance->list );

	mutex_unlock( &comp_db.instance_list_lock );

	if (instance->working_layers) {
		struct dss_comp_layers *worked_layers = _dss_comp_replace_working_layers(instance, NULL);

		_dss_comp_undo_first_node(instance, worked_layers);
		drain_workqueue(instance->buf_done_wq);

		if (wait_for_completion_timeout(&instance->close_completion, msecs_to_jiffies(DSS_COMP_CLOSING_WAIT_TIMEOUT_MS)) <= 0) {
			pr_err("wait_for_completion_timeout err\n");
			_dss_comp_node_using_print(worked_layers);
		}
	}

	destroy_workqueue(instance->buf_done_wq);

	if (instance->debug.nr_send_node != instance->debug.nr_rcv_node)
		pr_info("send:%d rcv:%d\n", instance->debug.nr_send_node, instance->debug.nr_rcv_node);

	vfree( instance );

	pr_info("%s end\n", __FUNCTION__);

	return true;
}

void dss_comp_init(unsigned long (*buf_alloc)(const size_t size), void (*buf_free)(const unsigned long paddr), void (*buf_done)(void *handle, const unsigned long paddr), unsigned long (*rot_paddr_get)(const unsigned long paddr))
{
	mutex_init( &comp_db.instance_list_lock );
	INIT_LIST_HEAD( &comp_db.instance_list );

	comp_db.buf_alloc = buf_alloc;
	comp_db.buf_free = buf_free;
	comp_db.buf_done = buf_done;

	du_ovl_init(_dss_comp_used_done_cb, dss_comp_ready_to_work_cb_online);
	rot_rsc_init(_dss_comp_used_done_cb, dss_comp_ready_to_work_cb_offline, rot_paddr_get);
}

void dss_comp_cleanup( void )
{
	struct dss_comp_instance *instance_node, *tmp;

	list_for_each_entry_safe( instance_node, tmp, &comp_db.instance_list, list ) {
		dss_comp_close( instance_node );
	}
}
