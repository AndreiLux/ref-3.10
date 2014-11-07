/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Youngki Lyu <youngki.lyu@lge.com>
 * Jungmin Park <jungmin016.park@lge.com>
 * Younghyun Jo <younghyun.jo@lge.com>
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
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>

#include "vevent_eq.h"

struct vevent_eq
{
	struct list_head list;
};

void (*vevent_eq_node_free)(void *);

void* vevent_eq_open(void)
{
	struct vevent_eq *eq = NULL;

	eq = vzalloc(sizeof(struct vevent_eq));
	if (eq == NULL) {
		pr_err("[vevent_eq] vzalloc failed\n");
		return ERR_PTR(-ENOMEM);
	}

	INIT_LIST_HEAD(&eq->list);

	return eq;
}

void vevent_eq_close(void *eq)
{
	struct vevent_eq *_eq = (struct vevent_eq *)eq;
	struct vevent_eq_node *node, *tmp;

	list_for_each_entry_safe(node, tmp, &_eq->list, list) {
		vevent_eq_node_free(node);
	}

	vfree(eq);
}

int vevent_eq_find_del_by_id(void *eq, void *vevent_id)
{
	struct vevent_eq *_eq = (struct vevent_eq *)eq;
	struct vevent_eq_node *node, *tmp;
	int deleted_num;
	deleted_num = 0;

	list_for_each_entry_safe(node, tmp, &_eq->list, list) {
		if (node->event.vevent_id == vevent_id){
			list_del(&node->list);
			vevent_eq_node_free(node);
			deleted_num++;
		}
	}

	return deleted_num;
}

int vevent_eq_push(void *eq, struct vevent_eq_node *node)
{
	struct vevent_eq *_eq = (struct vevent_eq *)eq;

	list_add_tail(&node->list, &_eq->list);

	return 0;
}

struct vevent_eq_node* vevent_eq_pop(void *eq)
{
	struct vevent_eq *_eq = (struct vevent_eq *)eq;
	struct vevent_eq_node *node = NULL;

	if (list_empty(&_eq->list)) {
		return NULL;
	}

	node = list_entry(_eq->list.next, struct vevent_eq_node, list);
	list_del(&node->list);

	return node;
}

struct vevent_eq_node* vevent_eq_peep(void *eq)
{
	struct vevent_eq *_eq = (struct vevent_eq *)eq;
	struct vevent_eq_node *node = NULL;

	if (list_empty(&_eq->list)) {
		return NULL;
	}

	node = list_entry(_eq->list.next, struct vevent_eq_node, list);

	return node;
}

void vevent_eq_init(void (*cb_node_free)(void*))
{
	vevent_eq_node_free = cb_node_free;
}
