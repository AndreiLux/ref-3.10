#ifndef __DONGLE_HDMI_H__
#define __DONGLE_HDMI_H__

#include <linux/notifier.h>

enum DONGLE_HDMI_STATUS {
	DONGLE_HDMI_POWER_ON = 1,
	DONGLE_HDMI_POWER_OFF,
	DONGLE_HDMI_MAx
};

/*Register to slimport driver*/
int hdmi_driver_notifier_register(struct notifier_block *nb);
int hdmi_driver_notifier_unregister(struct notifier_block *nb);

#endif /*__DONGLE_HDMI_H__*/

