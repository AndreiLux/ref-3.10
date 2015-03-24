#ifndef __CHARGER_NOTIFIER_H__
#define __CHARGER_NOTIFIER_H__

#include <linux/notifier.h>

int charger_notifier_register(struct notifier_block *nb);
int charger_notifier_unregister(struct notifier_block *nb);

#endif
