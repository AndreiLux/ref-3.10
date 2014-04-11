#ifndef _LINUX_CYPRESS_TOUCHKEY_I2C_H
#define _LINUX_CYPRESS_TOUCHKEY_I2C_H

#include <linux/types.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

/* LDO Regulator */
#define	TK_REGULATOR_NAME	"vtouch_1.8v"

/* LDO Regulator */
#define	TK_LED_REGULATOR_NAME	"vtouch_3.3v"

/* LED LDO Type*/
#define LED_LDO_WITH_REGULATOR

struct touchkey_platform_data {
	int gpio_sda;
	int gpio_scl;
	int gpio_int;
	u32 irq_gpio_flags;
	u32 sda_gpio_flags;
	u32 scl_gpio_flags;
	bool i2c_gpio;
	void (*init_platform_hw)(void);
	int (*suspend) (void);
	int (*resume) (void);
	int (*power_on)(void *data, bool on);
	int (*led_power_on) (bool);
	int (*reset_platform_hw)(void);
	void (*register_cb)(void *);
};

#endif /* _LINUX_CYPRESS_TOUCHKEY_I2C_H */
