/* ODIN Key Function */

#ifndef _ODIN_KEYS_H
#define _ODIN_KEYS_H

struct odin_pwrkey_platform_data {
	int (*enable)(struct device *dev);
	void (*disable)(struct device *dev);
	const char *name;
};


#endif
