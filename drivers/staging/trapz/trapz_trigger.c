/*
 * trapz_device.c
 *
 * TRAPZ (TRAcing and Profiling for Zpeed) Log Driver
 *
 * Copyright (C) Amazon Technologies Inc. All rights reserved.
 * Andy Prunicki (prunicki@lab126.com)
 * Martin Unsal (munsal@lab126.com)
 * TODO: Add additional contributor's names.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/trapz.h>

#include "trapz_trigger.h"

static void copy_timespec(const struct timespec *src, struct timespec *dest)
{
	dest->tv_sec = src->tv_sec;
	dest->tv_nsec = src->tv_nsec;
}

static void copy_trigger(const trapz_trigger_t *src, trapz_trigger_t *dest)
{
	dest->start_trace_point = src->start_trace_point;
	dest->end_trace_point = src->end_trace_point;
	dest->trigger_id = src->trigger_id;
	dest->single_shot = src->single_shot;
}

static int compare_triggers(const trapz_trigger_t *trig1,
	const trapz_trigger_t *trig2)
{
	int rc = 0;

	if (trig1->start_trace_point == trig2->start_trace_point &&
		trig1->end_trace_point == trig2->end_trace_point) {
		rc = 1;
	}

	return rc;
}

static int compare_trigger(const trapz_trigger_t *trig, const int trace_pt)
{
	int rc = 0;

	if (trig->start_trace_point == trace_pt)
		rc = 1;
	else if (trig->end_trace_point == trace_pt)
		rc = 2;

	return rc;
}

static struct trigger_list *find_trigger(const trapz_trigger_t *trigger,
	struct list_head *head)
{
	struct list_head *iter;
	struct trigger_list *curr_trig_list;

	list_for_each(iter, head) {
		curr_trig_list = list_entry(iter, struct trigger_list, list);
		if (compare_triggers(&curr_trig_list->trigger, trigger))
			return curr_trig_list;
	}

	return NULL;
}

static int find_trace_pt(const int trace_pt, struct trigger_list **trigger,
	struct list_head *head)
{
	struct list_head *iter;
	struct trigger_list *curr_trig_list;
	int rc = 0;

	list_for_each(iter, head) {
		curr_trig_list = list_entry(iter, struct trigger_list, list);
		rc = compare_trigger(&curr_trig_list->trigger, trace_pt);
		if (rc) {
			*trigger = curr_trig_list;
			break;
		}
	}

	return rc;
}

int add_trigger(const trapz_trigger_t *trigger, struct list_head *head,
	int *trigger_count)
{
	int rc = 0;
	struct trigger_list *trigger_list_ptr;

	if (*trigger_count < MAX_TRIGGERS) {
		/* Still more room for triggers */
		if (find_trigger(trigger, head) == NULL) {
			/* Trigger does not exist */
			trigger_list_ptr =
			(struct trigger_list *)
			kzalloc(sizeof(struct trigger_list), GFP_KERNEL);
			if (trigger_list_ptr == NULL) {
				rc = -ENOMEM;
			} else {
				copy_trigger(trigger,
					&trigger_list_ptr->trigger);
				INIT_LIST_HEAD(&trigger_list_ptr->list);
				list_add(&trigger_list_ptr->list, head);
				*trigger_count += 1;
			}
		} else {
			/* Trigger already exists */
			rc = -EEXIST;
		}
	} else {
		rc = -ENOMEM;
	}

	return rc;
}

int delete_trigger(const trapz_trigger_t *trigger, struct list_head *head,
	int *trigger_count)
{
	int rc = -EINVAL;
	struct trigger_list *trig_list = find_trigger(trigger, head);


	if (trig_list != NULL) {
		list_del(&trig_list->list);
		kfree(trig_list);
		*trigger_count -= 1;
		rc = 0;
	}

	return rc;
}

void clear_triggers(struct list_head *head, int *trigger_count)
{
	struct trigger_list *curr_trig_list;

	while (!list_empty(head)) {
		curr_trig_list = list_entry(head->next,
			struct trigger_list, list);
		list_del(head->next);
		kfree(curr_trig_list);
	}
	*trigger_count = 0;
}

void send_trigger_uevent(const trapz_trigger_event_t *trigger_event,
	struct kobject *kobj)
{
	char *envp[8];
	int i = 0;

	for (i = 1; i < 6; i++)
		envp[i] = kmalloc(sizeof(char) * 24, GFP_KERNEL);

	envp[0] = "EVENT=TRIGGER";
	snprintf(envp[1], 24, "ID=%d", trigger_event->trigger.trigger_id);
	snprintf(envp[2], 24, "START_S=%ld", trigger_event->start_ts.tv_sec);
	snprintf(envp[3], 24, "START_NS=%ld", trigger_event->start_ts.tv_nsec);
	snprintf(envp[4], 24, "END_S=%ld", trigger_event->end_ts.tv_sec);
	snprintf(envp[5], 24, "END_NS=%ld", trigger_event->end_ts.tv_nsec);
	if (trigger_event->trigger_active)
		envp[6] = "ACTIVE=TRUE";
	else
		envp[6] = "ACTIVE=FALSE";
	envp[7] = NULL;

	kobject_uevent_env(kobj, KOBJ_CHANGE, envp);
	for (i = 1; i < 6; i++)
		kfree(envp[i]);
}

void process_trigger(const int ctrl, trapz_trigger_event_t *trigger_event,
	struct list_head *head,
		int *trigger_count, struct timespec *ts)
{
	int trigger_rc;
	struct trigger_list *found_trig;

	trigger_rc = find_trace_pt(
		ctrl & TRAPZ_TRIGGER_MASK, &found_trig, head);
	if (trigger_rc == 1) {
		found_trig->start_ts.tv_sec = ts->tv_sec;
		found_trig->start_ts.tv_nsec = ts->tv_nsec;
	} else if (trigger_rc == 2 && found_trig->start_ts.tv_sec != 0) {
		/* TODO Load a local struct with the event info
			and send it out of crit. section. Also, delete
			the trigger if it is a one-shot. This is not
			a high-priority to fix, as we have not even
			utilized the trigger mechanism yet.
			Don't over-invest !! */
		copy_trigger(&found_trig->trigger, &trigger_event->trigger);
		copy_timespec(&found_trig->start_ts, &trigger_event->start_ts);
		copy_timespec(ts, &trigger_event->end_ts);
		if (found_trig->trigger.single_shot) {
			if (delete_trigger(
				&found_trig->trigger, head, trigger_count) != 0)
				printk(KERN_ERR "Trapz delete trigger failed.\n");
			trigger_event->trigger_active = 0;
		} else {
			trigger_event->trigger_active = 1;
		}
	}
}
