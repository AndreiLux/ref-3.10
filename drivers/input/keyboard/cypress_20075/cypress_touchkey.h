#ifndef _CYPRESS_TOUCHKEY_H
#define _CYPRESS_TOUCHKEY_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/wakelock.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif

#include <linux/i2c/touchkey_i2c.h>

/* Touchkey Register */
#define KEYCODE_REG			0x03
#define BASE_REG			0x00
#define TK_STATUS_FLAG		0x07
#define TK_DIFF_DATA		0x16
#define TK_RAW_DATA			0x1E
#define TK_IDAC				0x0D
#define TK_COMP_IDAC		0x11
#define TK_BASELINE_DATA	0x26
#define TK_THRESHOLD		0x09
#define TK_FW_VER			0x04

#define TK_BIT_PRESS_EV		0x08
#define TK_BIT_KEYCODE		0x07

/* New command*/
#define TK_BIT_CMD_LEDCONTROL   0x40    // Owner for LED on/off control (0:host / 1:TouchIC)
#define TK_BIT_CMD_INSPECTION   0x20    // Inspection mode
#define TK_BIT_CMD_1mmSTYLUS    0x10    // 1mm stylus mode
#define TK_BIT_CMD_FLIP         0x08    // flip mode
#define TK_BIT_CMD_GLOVE        0x04    // glove mode
#define TK_BIT_CMD_TA_ON		0x02    // Ta mode
#define TK_BIT_CMD_REGULAR		0x01    // regular mode = normal mode

#define TK_BIT_WRITE_CONFIRM	0xAA

/* Status flag after sending command*/
#define TK_BIT_LEDCONTROL   0x40    // Owner for LED on/off control (0:host / 1:TouchIC)
#define TK_BIT_1mmSTYLUS    0x20    // 1mm stylus mode
#define TK_BIT_FLIP         0x10    // flip mode
#define TK_BIT_GLOVE        0x08    // glove mode
#define TK_BIT_TA_ON		0x04    // Ta mode
#define TK_BIT_REGULAR		0x02    // regular mode = normal mode
#define TK_BIT_LED_STATUS	0x01    // LED status

#define TK_CMD_LED_ON		0x10
#define TK_CMD_LED_OFF		0x20

#define TK_UPDATE_DOWN		1
#define TK_UPDATE_FAIL		-1
#define TK_UPDATE_PASS		0

/* KEY_TYPE*/
#define TK_USE_2KEY_TYPE

/* Flip cover*/
#define TKEY_FLIP_MODE
/* 1MM stylus */
#define TKEY_1MM_MODE

/* Boot-up Firmware Update */
#define TK_HAS_FIRMWARE_UPDATE

#define FW_PATH "cypress/cypress_20075_k_m06.fw"
#define TKEY_FW_PATH "/sdcard/cypress/fw.bin"

struct fw_image {
	u8 hdr_ver;
	u8 hdr_len;
	u16 first_fw_ver;
	u16 second_fw_ver;
	u16 third_ver;
	u32 fw_len;
	u16 checksum;
	u16 alignment_dummy;
	u8 data[0];
} __attribute__ ((packed));

#ifdef CONFIG_INPUT_BOOSTER
#define TOUCHKEY_BOOSTER
#endif

/* #define TK_INFORM_CHARGER	1 */
/* #define TK_USE_OPEN_DWORK */
#ifdef TK_USE_OPEN_DWORK
#define	TK_OPEN_DWORK_TIME	10
#endif

#ifdef CONFIG_GLOVE_TOUCH
#define	TK_GLOVE_DWORK_TIME	300
#endif

#if defined(TK_INFORM_CHARGER)
struct touchkey_callbacks {
	void (*inform_charger)(struct touchkey_callbacks *, bool);
};
#endif

enum {
	FW_NONE = 0,
	FW_BUILT_IN,
	FW_HEADER,
	FW_IN_SDCARD,
	FW_EX_SDCARD,
};

struct fw_update_info {
	u8 fw_path;
	const struct firmware *firm_data;
	struct fw_image	*fw_img;
};

/*Parameters for i2c driver*/
struct touchkey_i2c {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct completion init_done;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	struct mutex lock;
	struct wake_lock fw_wakelock;
	struct device	*dev;
	int irq;
	int md_ver_ic; /*module ver*/
	int fw_ver_ic;
	int device_ver;
	int firmware_id;
	struct touchkey_platform_data *pdata;
	char *name;
	int (*power)(int on);
	int update_status;
	bool enabled;
	int	src_fw_ver;
	int	src_md_ver;
#ifdef TK_USE_OPEN_DWORK
	struct delayed_work open_work;
#endif
#ifdef CONFIG_GLOVE_TOUCH
	struct delayed_work glove_change_work;
	bool tsk_glove_lock_status;
	bool tsk_glove_mode_status;
	struct mutex tsk_glove_lock;
#endif
#ifdef TK_INFORM_CHARGER
	struct touchkey_callbacks callbacks;
	bool charging_mode;
#endif
#ifdef TKEY_FLIP_MODE
	bool enabled_flip;
#endif
#ifdef TKEY_1MM_MODE
	bool enabled_1mm;
#endif
	bool status_update;
	struct work_struct update_work;
	struct workqueue_struct *fw_wq;
	struct fw_update_info update_info;
	bool do_checksum;
};

extern struct class *sec_class;
void touchkey_charger_infom(bool en);

extern unsigned int system_rev;
#endif /* _CYPRESS_TOUCHKEY_H */
