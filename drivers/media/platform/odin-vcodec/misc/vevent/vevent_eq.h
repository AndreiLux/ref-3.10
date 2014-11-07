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

#ifndef _VEVENT_EQ_H_
#define _VEVENT_EQ_H_

#include <media/odin/vcodec/vevent_io.h>

struct vevent_eq_node
{
	struct vevent event;
	struct list_head list;
};

extern void* vevent_eq_open(void);
extern void vevent_eq_close(void *eq);

extern int vevent_eq_find_del_by_id(void *eq, void *vevent_id);
extern int vevent_eq_push(void *eq, struct vevent_eq_node *node);
extern struct vevent_eq_node* vevent_eq_pop(void *eq);
extern struct vevent_eq_node* vevent_eq_peep(void *eq);

extern void vevent_eq_init(void (*cb_node_free)(void*));

#endif /* #ifndef _VEVENT_EQ_H_ */

