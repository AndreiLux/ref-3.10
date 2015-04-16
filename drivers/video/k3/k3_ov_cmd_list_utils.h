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
#ifndef _CMD_LIST_UTILS_H_
#define _CMD_LIST_UTILS_H_


typedef union {
	struct {
		u32 exec:1;
		u32 last:1;
		u32 nop:1;
		u32 interrupt:1;
		u32 pending:1;
		u32 id:10;
		u32 reserved:9;
		u32 valid_flag:8; //0xA5 is valid
	} bits;
	u32 ul32;

} cmd_flag_t;

typedef union {
	struct {
		u32 count:14;
		u32 reserved:18;
	} bits;
	u32 ul32;

} total_items_t;

typedef union {
	struct {
		u32 add0:18;
		u32 add1:6;
		u32 add2:6;
		u32 cnt:2;
	} bits;
	u32 ul32;

} reg_addr_t;

typedef struct cmd_item {
	reg_addr_t reg_addr;
	u32 data0;
	u32 data1;
	u32 data2;

} cmd_item_t;

typedef struct cmd_header {
	cmd_flag_t   flag;
	u32    next_list;
	total_items_t    total_items;
	u32    list_addr; //128bit align

} cmd_header_t;

enum dss_cmdlist_status {
	e_status_idle = 0x0,
	e_status_wait = 0x1,
	e_status_other,
};

enum dss_cmdlist_node_type {
	e_node_none = 0x0,
	e_node_first_nop = 0x1,
	e_node_nop = 0x2,
	e_node_frame = 0x3,
};

enum dss_cmdlist_list_valid {
	e_list_invalid = 0x0,
	e_list_valid = 0xA5,
};


/*
*for normal node,all variable should be filled.
*for NOP node, just the list_header,header_ion_handle,list_node, list_node_flag should be filled.
*list_node_flag must be e_node_nop when it is NOP node.
*And item_ion_handle in NOP node should be NULL.
*/
typedef struct dss_cmdlist_node {
	cmd_header_t * list_header;
	cmd_item_t *list_item;
	u32 item_index;
	int item_flag;

	int is_used;
	struct ion_handle* header_ion_handle;
	ion_phys_addr_t header_phys;
	struct ion_handle* item_ion_handle;
	ion_phys_addr_t item_phys;
	struct list_head list_node;

	u32 list_node_flag;
	u32 list_node_type;

} dss_cmdlist_node_t;


int cmdlist_alloc_one_list(dss_cmdlist_node_t ** cmdlist_node, struct ion_client *ion_client);
int cmdlist_free_one_list(dss_cmdlist_node_t * cmdlist_node, struct ion_client *ion_client);
dss_cmdlist_node_t * cmdlist_get_one_list(struct k3_fb_data_type *k3fd, int * id);
int cmdlist_throw_one_list(dss_cmdlist_node_t * cmdlist_node);

void ov_cmd_list_output_32(struct k3_fb_data_type *k3fd, char __iomem *base_addr, u32 value, u8 bw, u8 bs);

int cmdlist_add_new_list(struct k3_fb_data_type *k3fd, struct list_head * cmdlist_head, int is_last, u32 flag);
void cmdlist_frame_valid(struct list_head * cmdlist_head);
void cmdlist_flush_cmdlistmemory_cache(struct list_head * cmdlist_head, struct ion_client *ion_client);

int cmdlist_add_nop_list(struct k3_fb_data_type *k3fd, struct list_head *cmdlist_head, int is_first);
void cmdlist_start_frame(struct k3_fb_data_type *k3fd, u32 wbe_chn);
void cmdlist_free_nop(struct list_head * cmdlist_head, struct ion_client *ion_client);
void cmdlist_free_frame(struct list_head * cmdlist_head, struct ion_client *ion_client);

int cmdlist_prepare_next_frame(struct k3_fb_data_type *k3fd, u32 wbe_chn);

int cmdlist_config_start(struct k3_fb_data_type *k3fd, u32 wbe_chn);
int cmdlist_config_stop(struct k3_fb_data_type *k3fd, u32 wbe_chn);

void cmdlist_print_all_node(struct list_head * cmdlist_head);
void cmdlist_print_node_items(cmd_item_t * item, u32 count);


#endif
