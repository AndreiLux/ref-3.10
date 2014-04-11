#ifndef _LINUX_INPUT_BOOSTER_H
#define _LINUX_INPUT_BOOSTER_H

#define INPUT_BOOSTER_NAME "input_booster"

/* We can use input handler to catch up specific events which are
 * used for the input booster.
 */
/*#define BOOSTER_USE_INPUT_HANDLER*/

#define BOOSTER_DEFAULT_ON_TIME		500	/* msec */
#define BOOSTER_DEFAULT_OFF_TIME	500	/* msec */
#define BOOSTER_DEFAULT_CHG_TIME	130	/* msec */

#define BOOSTER_DEBUG_CHG_TIME		500 /* msec */
#define BOOSTER_DEBUG_OFF_TIME		1000 /* msec */

/* Booster levels are used as below comment.
 * 0	: OFF
 * 1	: SIP (default SIP)
 * 2	: Default
 * 3	: Hangout SIP
 * 4	: Browsable App's
 * 5	: Light SIP, MMS, ..
 * 9	: Max for benchmark tool
 */
enum booster_level {
	BOOSTER_LEVEL0 = 0,
	BOOSTER_LEVEL1,
	BOOSTER_LEVEL2,
	BOOSTER_LEVEL3,
	BOOSTER_LEVEL4,
	BOOSTER_LEVEL5,
	BOOSTER_LEVEL9 = 9,
	BOOSTER_LEVEL9_CHG,
	BOOSTER_LEVEL_MAX,
};

enum booster_device_type {
	BOOSTER_DEVICE_KEY = 0,
	BOOSTER_DEVICE_TOUCHKEY,
	BOOSTER_DEVICE_TOUCH,
	BOOSTER_DEVICE_PEN,
	BOOSTER_DEVICE_MAX,
	BOOSTER_DEVICE_NOT_DEFINED,
};

enum booster_mode {
	BOOSTER_MODE_OFF = 0,
	BOOSTER_MODE_ON,
	BOOSTER_MODE_FORCE_OFF,
};

struct dvfs_time {
	int chg_time;
	int off_time;
	int pri_time;
};

struct dvfs_freq {
	s32 cpu_freq;
	s32 kfc_freq;
	s32 mif_freq;
	s32 int_freq;
};

struct booster_key {
	const char *desc;
	int code;
	const struct dvfs_time *time_table;
	const struct dvfs_freq *freq_table;
};

struct input_booster_platform_data {
	struct booster_key *keys;
	int nkeys;
	enum booster_device_type (*get_device_type) (int code);
};

#ifdef BOOSTER_USE_INPUT_HANDLER
#define INPUT_BOOSTER_SEND_EVENT(code, value)
#define INPUT_BOOSTER_REPORT_KEY_EVENT(dev, code, value)	\
	input_report_key(dev, code, value)
#else
extern void input_booster_send_event(unsigned int code, int value);

#define INPUT_BOOSTER_SEND_EVENT(code, value)	\
		input_booster_send_event(code, value)
#define INPUT_BOOSTER_REPORT_KEY_EVENT(dev, code, value)
#endif

/* Now we support sysfs file to change booster level under input_booster class.
 * But previous sysfs is scatterd into each driver, so we need to support it also.
 * Enable below define. if you want to use previous sysfs files.
 */
extern void change_booster_level_for_tsp(unsigned char level);
extern void change_booster_level_for_tsk(unsigned char level);

#endif /* _LINUX_INPUT_BOOSTER_H */
