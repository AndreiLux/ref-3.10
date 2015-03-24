#ifndef MT_BATTERY_PROTECT_H
#define MT_BATTERY_PROTECT_H

#define BATTERY_MAX_BUDGET 4200
#define BATTERY_MIN_BUDGET 430

#define BATTERY_BUDGET "battery_budget"
#define BATTERY_NOTIFY "battery_notify"

typedef struct {
	int major, minor;
	int flag;
	dev_t devno;
	struct class *cls;
	struct cdev cdev;
	wait_queue_head_t queue;
} battery_throttle_dev_t;

extern void low_battery_protect_enable(void);

#endif
