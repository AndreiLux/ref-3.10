#ifndef __LINUX_TOUCH_NOTIFY_H
#define __LINUX_TOUCH_NOTIFY_H

#include <linux/notifier.h>


/* the dsv on */
#define LCD_EVENT_TOUCH_LPWG_ON		0x01

#define LCD_EVENT_TOUCH_LPWG_OFF	0x02

/* to let lcd-driver know touch-driver's status */
#define LCD_EVENT_TOUCH_DRIVER_REGISTERED	0x03

struct touch_event {
	void *data;
};


int touch_register_client(struct notifier_block *nb);
int touch_unregister_client(struct notifier_block *nb);
int touch_notifier_call_chain(unsigned long val, void *v);
#endif /* _LINUX_TOUCH_NOTIFY_H */
