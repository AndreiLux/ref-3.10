#ifndef _LINUX_FTS_TS_H_
#define _LINUX_FTS_TS_H_

#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/wakelock.h>
#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif


#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#define tsp_debug_dbg(mode, dev, fmt, ...)	\
({								\
	if (mode) {					\
		dev_dbg(dev, fmt, ## __VA_ARGS__);	\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);		\
	}				\
	else					\
		dev_dbg(dev, fmt, ## __VA_ARGS__);	\
})

#define tsp_debug_info(mode, dev, fmt, ...)	\
({								\
	if (mode) {							\
		dev_info(dev, fmt, ## __VA_ARGS__);		\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);		\
	}				\
	else					\
		dev_info(dev, fmt, ## __VA_ARGS__);	\
})

#define tsp_debug_err(mode, dev, fmt, ...)	\
({								\
	if (mode) {					\
		dev_err(dev, fmt, ## __VA_ARGS__);	\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);	\
	}				\
	else					\
		dev_err(dev, fmt, ## __VA_ARGS__); \
})
#else
#define tsp_debug_dbg(mode, dev, fmt, ...)	dev_dbg(dev, fmt, ## __VA_ARGS__)
#define tsp_debug_info(mode, dev, fmt, ...)	dev_info(dev, fmt, ## __VA_ARGS__)
#define tsp_debug_err(mode, dev, fmt, ...)	dev_err(dev, fmt, ## __VA_ARGS__)
#endif

#define USE_OPEN_CLOSE

#ifdef USE_OPEN_DWORK
#define TOUCH_OPEN_DWORK_TIME 10
#endif

#define FIRMWARE_IC					"fts_ic"
#define FTS_MAX_FW_PATH					64
#define FTS_TS_DRV_NAME					"fts_touch"
#define FTS_TS_DRV_VERSION				"0132"

#define FTS_DEVICE_NAME					"STM"

#define FTS_FIRMWARE_NAME_TRLTE				"tsp_stm/stm_tr.fw"
#define FTS_FIRMWARE_NAME_TBLTE				"tsp_stm/stm_tb.fw"
#define FTS_FIRMWARE_NAME_NULL	NULL

#define CONFIG_ID_D2_TR					0x2E
#define CONFIG_ID_D2_TB					0x30
#define CONFIG_OFFSET_BIN_D1				0xf822
#define CONFIG_OFFSET_BIN_D2				0x1E822

#define RX_OFFSET_BIN_D2				0x1E834
#define TX_OFFSET_BIN_D2				0x1E835

#define FTS_ID0						0x39
#define FTS_ID1						0x80
#define FTS_ID2						0x6C

#define FTS_SLAVE_ADDRESS				0x49
#define FTS_SLAVE_ADDRESS_SUB				0x48

#define FTS_DIGITAL_REV_1				0x01
#define FTS_DIGITAL_REV_2				0x02
#define FTS_FIFO_MAX					32
#define FTS_EVENT_SIZE					8

#define PRESSURE_MIN					0
#define PRESSURE_MAX					127
#define P70_PATCH_ADDR_START				0x00420000
#define FINGER_MAX					10
#define AREA_MIN					PRESSURE_MIN
#define AREA_MAX					PRESSURE_MAX

#define EVENTID_NO_EVENT				0x00
#define EVENTID_ENTER_POINTER				0x03
#define EVENTID_LEAVE_POINTER				0x04
#define EVENTID_MOTION_POINTER				0x05
#define EVENTID_HOVER_ENTER_POINTER			0x07
#define EVENTID_HOVER_LEAVE_POINTER			0x08
#define EVENTID_HOVER_MOTION_POINTER			0x09
#define EVENTID_PROXIMITY_IN				0x0B
#define EVENTID_PROXIMITY_OUT				0x0C
#define EVENTID_MSKEY					0x0E
#define EVENTID_ERROR					0x0F
#define EVENTID_CONTROLLER_READY			0x10
#define EVENTID_SLEEPOUT_CONTROLLER_READY		0x11
#define EVENTID_RESULT_READ_REGISTER			0x12
#define EVENTID_STATUS_EVENT				0x16
#define EVENTID_INTERNAL_RELEASE_INFO			0x19
#define EVENTID_EXTERNAL_RELEASE_INFO			0x1A

#define EVENTID_FROM_STRING				0x80
#define EVENTID_GESTURE					0x20

#define STATUS_EVENT_MUTUAL_AUTOTUNE_DONE		0x01
#define STATUS_EVENT_SELF_AUTOTUNE_DONE			0x42

#define INT_ENABLE					0x41
#define INT_DISABLE					0x00

#define READ_STATUS					0x84
#define READ_ONE_EVENT					0x85
#define READ_ALL_EVENT					0x86

#define SLEEPIN						0x90
#define SLEEPOUT					0x91
#define SENSEOFF					0x92
#define SENSEON						0x93
#define FTS_CMD_HOVER_OFF				0x94
#define FTS_CMD_HOVER_ON				0x95

#define FTS_CMD_MSKEY_AUTOTUNE				0x96

#define FTS_CMD_KEY_SENSE_ON				0x9B
#define SENSEON_SLOW					0x9C

#define FTS_CMD_SET_FAST_GLOVE_MODE			0x9D

#define FTS_CMD_MSHOVER_OFF				0x9E
#define FTS_CMD_MSHOVER_ON				0x9F
#define FTS_CMD_SET_NOR_GLOVE_MODE			0x9F

#define FLUSHBUFFER					0xA1
#define FORCECALIBRATION				0xA2
#define CX_TUNNING					0xA3
#define SELF_AUTO_TUNE					0xA4

#define FTS_CMD_CHARGER_PLUGGED				0xA8
#define FTS_CMD_CHARGER_UNPLUGGED			0xAB

#define FTS_CMD_RELEASEINFO				0xAA
#define FTS_CMD_STYLUS_OFF				0xAB
#define FTS_CMD_STYLUS_ON				0xAC
#define FTS_CMD_LOWPOWER_MODE				0xAD

#define FTS_CMS_ENABLE_FEATURE				0xC1
#define FTS_CMS_DISABLE_FEATURE				0xC2

#define FTS_CMD_WRITE_PRAM				0xF0
#define FTS_CMD_BURN_PROG_FLASH				0xF2
#define FTS_CMD_ERASE_PROG_FLASH			0xF3
#define FTS_CMD_READ_FLASH_STAT				0xF4
#define FTS_CMD_UNLOCK_FLASH				0xF7
#define FTS_CMD_SAVE_FWCONFIG				0xFB
#define FTS_CMD_SAVE_CX_TUNING				0xFC

#define FTS_CMD_CAM_QUICK_ACCESS			0xF800
#define FTS_CMD_NOTIFY					0xC0

#define FTS_RETRY_COUNT					10

#define TSP_BUF_SIZE 1024
#define CMD_STR_LEN 32
#define CMD_RESULT_STR_LEN 512
#define CMD_PARAM_NUM 8


#define FTS_SUPPORT_NOISE_PARAM

#ifdef CONFIG_GLOVE_TOUCH
enum TOUCH_MODE {
	FTS_TM_NORMAL = 0,
	FTS_TM_GLOVE,
};
#endif

enum fts_error_return {
	FTS_NOT_ERROR = 0,
	FTS_ERROR_INVALID_CHIP_ID,
	FTS_ERROR_INVALID_CHIP_VERSION_ID,
	FTS_ERROR_INVALID_SW_VERSION,
	FTS_ERROR_EVENT_ID,
	FTS_ERROR_TIMEOUT,
	FTS_ERROR_FW_UPDATE_FAIL,
};

#ifdef FTS_SUPPORT_NOISE_PARAM
#define MAX_NOISE_PARAM 5
struct fts_noise_param {
	unsigned short pAddr[MAX_NOISE_PARAM];
	unsigned char pData[MAX_NOISE_PARAM];
};
#endif

struct fts_i2c_platform_data {
	bool factory_flatform;
	bool recovery_mode;
	bool support_hover;
	bool support_mshover;
	int max_x;
	int max_y;
	int max_width;

	const char *firmware_name;
	const char *project_name;

	unsigned gpio;
	int irq_type;
};

#define SEC_TSP_FACTORY_TEST

#undef FTS_SUPPORT_TA_MODE

#ifdef SEC_TSP_FACTORY_TEST
extern struct class *sec_class;
#endif

struct fts_device_tree_data {
	int max_x;
	int max_y;
	int support_hover;
	int support_mshover;
	int extra_config[4];
	int external_ldo;
	int irq_gpio;
	const char *sub_pmic;
	const char *project;
	bool support_sidegesture;

	int num_of_supply;
	const char **name_of_supply;
};

#define RAW_MAX	3750
/**
 * struct fts_finger - Represents fingers.
 * @ state: finger status (Event ID).
 * @ mcount: moving counter for debug.
 */
struct fts_finger {
	unsigned char state;
	unsigned short mcount;
};

enum tsp_power_mode {
	TSP_POWERDOWN_MODE = 0,
	TSP_DEEPSLEEP_MODE,
	TSP_LOWPOWER_MODE,
};

enum fts_customer_feature {
	FTS_FEATURE_ORIENTATION_GESTURE = 1,
	FTS_FEATURE_STYLUS,
	FTS_FEATURE_QUICK_SHORT_CAMERA_ACCESS,
	FTS_FEATURE_SIDE_GUSTURE,
	FTS_FEATURE_COVER_GLASS,
	FTS_FEATURE_FAST_GLOVE_MODE,
};

struct fts_ts_info {
	struct device *dev;
	struct i2c_client *client;
	struct i2c_client *client_sub;
	struct input_dev *input_dev;
	struct hrtimer timer;
	struct timer_list timer_charger;
	struct timer_list timer_firmware;
	struct work_struct work;
	struct fts_device_tree_data *dt_data;
	struct regulator *vcc_en;
	struct regulator *sub_pmic;
	struct regulator_bulk_data *supplies;
#ifdef TSP_BOOSTER
	struct input_booster *tsp_booster;
#endif

	unsigned short slave_addr;

	const char *firmware_name;
	int irq;
	int irq_type;
	bool irq_enabled;

#ifdef FTS_SUPPORT_TA_MODE 	
	struct fts_callbacks callbacks;
#endif
	struct mutex lock;
	bool enabled;
#ifdef SEC_TSP_FACTORY_TEST
	struct device *fac_dev_ts;
	struct list_head cmd_list_head;
	u8 cmd_state;
	char cmd[CMD_STR_LEN];
	int cmd_param[CMD_PARAM_NUM];
	char cmd_result[CMD_RESULT_STR_LEN];
	struct mutex cmd_lock;
	bool cmd_is_running;
	int SenseChannelLength;
	int ForceChannelLength;
	short *pFrame;
#endif

	bool slow_report_rate;
	bool hover_ready;
	bool hover_enabled;
	bool mshover_enabled;
	bool fast_mshover_enabled;
	bool flip_enable;

	bool lowpower_mode;
	bool deepsleep_mode;
	int fts_power_mode;

#ifdef FTS_SUPPORT_TA_MODE
	bool TA_Pluged;
#endif

#ifdef FTS_SUPPORT_TOUCH_KEY
	unsigned char tsp_keystatus;
	int touchkey_threshold;
	struct device *fac_dev_tk;
#endif

	int digital_rev;
	int touch_count;
	struct fts_finger finger[FINGER_MAX];

	int touch_mode;
	int retry_hover_enable_after_wakeup;

	int ic_product_id;			/* product id of ic */
	int ic_revision_of_ic;		/* revision of reading from IC */
	int fw_version_of_ic;		/* firmware version of IC */
	int ic_revision_of_bin;		/* revision of reading from binary */
	int fw_version_of_bin;		/* firmware version of binary */
	int config_version_of_ic;		/* Config release data from IC */
	int config_version_of_bin;	/* Config release data from IC */
	unsigned short fw_main_version_of_ic;	/* firmware main version of IC */
	unsigned short fw_main_version_of_bin;	/* firmware main version of binary */
	int panel_revision;			/* Octa panel revision */

#ifdef USE_OPEN_DWORK
	struct delayed_work open_work;
#endif

#ifdef FTS_SUPPORT_NOISE_PARAM
	struct fts_noise_param *noise_param;
#endif

	struct mutex i2c_mutex;
	struct mutex device_mutex;
	bool touch_stopped;
	bool reinit_done;

	unsigned char data[FTS_EVENT_SIZE * FTS_FIFO_MAX];

	void (*fts_power_ctrl)(struct fts_ts_info *info, bool enable);

	int (*stop_device) (struct fts_ts_info * info);
	int (*start_device) (struct fts_ts_info * info);
	int (*fts_write_reg)(struct fts_ts_info *info, unsigned char *reg, unsigned short num_com);
	int (*fts_read_reg)(struct fts_ts_info *info, unsigned char *reg, int cnum, unsigned char *buf, int num);
	int (*fts_get_version_info)(struct fts_ts_info *info);
	int (*fts_wait_for_ready)(struct fts_ts_info *info);
	int (*fts_command)(struct fts_ts_info *info, unsigned char cmd);
	int (*fts_systemreset)(struct fts_ts_info *info);
	int (*fts_write_to_string)(struct fts_ts_info *info, unsigned short *reg, unsigned char *data, int length);
	int (*fts_read_from_string)(struct fts_ts_info *info, unsigned short *reg, unsigned char *data, int length);
#ifdef FTS_SUPPORT_NOISE_PARAM
	int (*fts_get_noise_param_address) (struct fts_ts_info *info);
#endif
};

int fts_fw_update_on_probe(struct fts_ts_info *info);
int fts_fw_update_on_hidden_menu(struct fts_ts_info *info, int update_type);
void fts_fw_init(struct fts_ts_info *info);

/*
 * If in DeviceTree structure, using below tree.
 * below tree is connected fts_parse_dt() in fts_ts.c file.
 */
/*
fts_i2c@49 {
	compatible = "stm,fts_ts";
	reg = <0x49>;
	interrupt-parent = <&msmgpio>;
	interrupts = <79 0>;
	vcc_en-supply = <&pma8084_lvs3>;
	stm,supply-num = <2>;		// the number of regulators
	stm,supply-name = "8084_l17", "8084_lvs3";	// the name of regulators, in power-on order
	stm,irq-gpio = <&msmgpio 79 0x1>;
	stm,tsp-coords = <1079 1919>;
	stm,support_func = <1 1>; // <support_hover, support_mshover>
	stm,i2c-pull-up = <1>;
	stm,tsp-project = "TR_LTE";
	stm,tsp-extra_config = <0 0 0 0>;// <"pwrctrl", "pwrctrl(sub)", "octa_id", "N">
};

*/
#endif				//_LINUX_FTS_TS_H_
