#include <linux/kobject.h>
#include "trapz_device.h"

#ifndef _LINUX_TRAPZ_TRIGGER_H
#define _LINUX_TRAPZ_TRIGGER_H

#define MAX_TRIGGERS 3

struct trigger_list {
	struct list_head list;
	trapz_trigger_t trigger;
	struct timespec start_ts;
};

int add_trigger(const trapz_trigger_t *trigger, struct list_head *head,
	int *trigger_count);
int delete_trigger(const trapz_trigger_t *trigger, struct list_head *head,
	int *trigger_count);
void clear_triggers(struct list_head *head, int *trigger_count);
void send_trigger_uevent(const trapz_trigger_event_t *trigger_event,
	struct kobject *kobj);
void process_trigger(const int ctrl, trapz_trigger_event_t *trigger_event,
	struct list_head *head, int *trigger_count, struct timespec *ts);

#endif  /* _LINUX_TRAPZ_TRIGGER_H */
