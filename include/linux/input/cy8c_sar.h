#ifndef CY8C_SAR_I2C_H
#define CY8C_SAR_I2C_H

#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/notifier.h>
#include <linux/wakelock.h>
#include <linux/list.h>

#define CYPRESS_SAR_NAME 	"CYPRESS_SAR"
#define CYPRESS_SAR1_NAME	"CYPRESS_SAR1"
#define CYPRESS_SS_NAME 	"CY8C21x34B"

/* Bit 0 => Sensor Pad 1, ..., Bit 3 => Sendor Pad 4*/
#define CS_STATUS		(0x00)

/* F/W Reversion */
#define CS_FW_VERSION		(0x06)
#define CS_FW_CONFIG		(0xAA)		/*Smart Sense Supported*/

/* 8: high sensitivity, 255: low sensitivity */
#define CS_IDAC_BTN_BASE	(0x02)
#define CS_IDAC_BTN_PAD1	(0x02)
#define CS_IDAC_BTN_PAD2        (0x03)
#define CS_IDAC_BTN_PAD3        (0x04)
#define CS_IDAC_BTN_PAD4        (0x05)

#define CS_MODE			(0x07)
#define CS_DTIME		(0x07)
#define CS_SLEEPTIME		(0x08)
#define CS_FW_CHIPID		(0x0A)		/*Smart Sense Supported*/
#define CS_FW_KEYCFG		(0x0B)		/*Smart Sense Supported*/

#define CS_SELECT		(0x0C)
#define CS_BL_HB		(0x0D)
#define CS_BL_LB		(0x0E)
#define CS_RC_HB		(0x0F)
#define	CS_RC_LB		(0x10)
#define CS_DF_HB		(0x11)
#define CS_DF_LB		(0x12)
#define CS_INT_STATUS		(0x13)


#define CS_CMD_BASELINE		(0x55)
#define CS_CMD_DSLEEP		(0x02)
#define CS_CMD_BTN1		(0xA0)
#define CS_CMD_BTN2		(0xA1)
#define CS_CMD_BTN3		(0xA2)
#define CS_CMD_BTN4		(0xA3)

#define CS_CHIPID		(0x08)
#define CS_KEY_4		(0x04)
#define CS_KEY_3		(0x03)

#define CS_FUNC_PRINTRAW        (0x01)
#define ENABLE_CAP_ONLY_3KEY   1        /* 1:Enable 0:Disable. Let cap only support 3Key.*/

#define CY8C_I2C_RETRY_TIMES 	(5)
#define WAKEUP_DELAY (1*HZ)

enum mode {
	KEEP_AWAKE = 0,
	DEEP_SLEEP,
};

struct infor {
	uint8_t  config;
	uint16_t chipid;
	uint16_t version;
};

struct cy8c_i2c_sar_platform_data {
	struct 	infor id;
	uint16_t gpio_rst;
	uint16_t gpio_irq;
	uint16_t position_id;
	int 	(*power)(int on);
	int 	(*reset)(void);
	void 	(*gpio_init)(void);
};

struct cy8c_sar_data {
	struct list_head list;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct workqueue_struct *cy8c_wq;
	uint8_t use_irq;
	int radio_state;
	uint16_t position_id;
	enum mode sleep_mode;
	int pm_state;
	uint8_t is_activated;
	uint8_t polarity;
	int intr_irq;
	struct hrtimer timer;
	spinlock_t spin_lock;
	uint16_t version;
	struct infor id;
	uint16_t intr;
	int (*power)(int on);
	int (*reset)(void);
	unsigned long spinlock_flags;
	struct class *sar_class;
	struct device *sar_dev;
	struct delayed_work sleep_work;
};

/*For wifi call back*/
extern struct blocking_notifier_head sar_notifier_list;

extern int board_build_flag(void);

extern int register_notifier_by_sar(struct notifier_block *nb);
extern int unregister_notifier_by_sar(struct notifier_block *nb);
#endif
