/*
 * LP5521 LED chip driver.
 *
 * Copyright (C) 2010 Nokia Corporation
 *
 * Contact: Samu Onkalo <samu.p.onkalo@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/leds.h>
#include <linux/leds-lp5521.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/da9210-regulator.h>
#include <linux/board_lge.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>

/* for Regmap */
#include <linux/regmap.h>
#include <linux/regmap-ipc.h>


#define LP5521_PROGRAM_LENGTH		32	/* in bytes */
#define LP5521_ENG_MASK_BASE		0x30	/* 00110000 */
#define LP5521_ENG_STATUS_MASK		0x07	/* 00000111 */

#define LP5521_CMD_LOAD			0x15	/* 00010101 */
#define LP5521_CMD_RUN			0x2a	/* 00101010 */
#define LP5521_CMD_DIRECT		0x3f	/* 00111111 */
#define LP5521_CMD_DISABLED		0x00	/* 00000000 */

/* Registers */
#define LP5521_REG_ENABLE		0x00
#define LP5521_REG_OP_MODE		0x01
#define LP5521_REG_R_PWM		0x02
#define LP5521_REG_G_PWM		0x03
#define LP5521_REG_B_PWM		0x04
#define LP5521_REG_R_CURRENT		0x05
#define LP5521_REG_G_CURRENT		0x06
#define LP5521_REG_B_CURRENT		0x07
#define LP5521_REG_CONFIG		0x08
#define LP5521_REG_R_CHANNEL_PC		0x09
#define LP5521_REG_G_CHANNEL_PC		0x0A
#define LP5521_REG_B_CHANNEL_PC		0x0B
#define LP5521_REG_STATUS		0x0C
#define LP5521_REG_RESET		0x0D
#define LP5521_REG_GPO			0x0E
#define LP5521_REG_R_PROG_MEM		0x10
#define LP5521_REG_G_PROG_MEM		0x30
#define LP5521_REG_B_PROG_MEM		0x50

#define LP5521_PROG_MEM_BASE		LP5521_REG_R_PROG_MEM
#define LP5521_PROG_MEM_SIZE		0x20

/* Base register to set LED current */
#define LP5521_REG_LED_CURRENT_BASE	LP5521_REG_R_CURRENT

/* Base register to set the brightness */
#define LP5521_REG_LED_PWM_BASE		LP5521_REG_R_PWM

/* Bits in ENABLE register */
#define LP5521_MASTER_ENABLE		0x40	/* chip_ind master enable */
#define LP5521_LOGARITHMIC_PWM		0x80	/* Logarithmic PWM adjustment */
#define LP5521_EXEC_RUN			0x2A
#define LP5521_ENABLE_DEFAULT	\
	(LP5521_MASTER_ENABLE | LP5521_LOGARITHMIC_PWM)
#define LP5521_ENABLE_RUN_PROGRAM	\
	(LP5521_ENABLE_DEFAULT | LP5521_EXEC_RUN)

/* Status */
#define LP5521_EXT_CLK_USED		0x08

/* default R channel current register value */
#define LP5521_REG_R_CURR_DEFAULT	0xAF

/* Current index max step */
#define PATTERN_CURRENT_INDEX_STEP_HAL 255

/* Pattern Mode */
#define PATTERN_OFF	0
#define PATTERN_BLINK_ON	-1

/*GV DCM Felica Pattern Mode*/
#define PATTERN_FELICA_ON 101
#define PATTERN_GPS_ON 102

/*GK Favorite MissedNoit Pattern Mode*/
#define PATTERN_FAVORITE_MISSED_NOTI 14

/*GK ATT Power off charged Mode (charge complete brightness 50%)*/
#define PATTERN_CHARGING_COMPLETE_50 15
#define PATTERN_CHARGING_50 16

/* Program Commands */
#define CMD_SET_PWM			0x40
#define CMD_WAIT_LSB			0x00

#define MAX_BLINK_TIME			60000	/* 60 sec */
#define lp5521_REG_MAX 			0xFF
#define LP5521_I2C_SLOT			2
#define LP5521_SLAVE_ADDR 		0x64

enum lp5521_wait_type {
	LP5521_CYCLE_INVALID,
	LP5521_CYCLE_50ms,
	LP5521_CYCLE_100ms,
	LP5521_CYCLE_200ms,
	LP5521_CYCLE_500ms,
	LP5521_CYCLE_700ms,
	LP5521_CYCLE_920ms,
	LP5521_CYCLE_982ms,
	LP5521_CYCLE_MAX,
};

struct lp5521_pattern_cmd {
	u8 r[LP5521_PROGRAM_LENGTH];
	u8 g[LP5521_PROGRAM_LENGTH];
	u8 b[LP5521_PROGRAM_LENGTH];
	unsigned pc_r;
	unsigned pc_g;
	unsigned pc_b;
};

struct lp5521_wait_param {
	unsigned cycle;
	unsigned limit;
	u8 cmd;
};
const struct regmap_config lp5521_ind_regmap_config =
{
	.reg_bits = 8,
	.val_bits = 8,
	.val_format_endian = lp5521_REG_MAX,


};

static const struct lp5521_wait_param lp5521_wait_params[LP5521_CYCLE_MAX] = {
	[LP5521_CYCLE_50ms] = {
		.cycle = 50,
		.limit = 3000,
		.cmd   = 0x43,
	},
	[LP5521_CYCLE_100ms] = {
		.cycle = 100,
		.limit = 6000,
		.cmd   = 0x46,
	},
	[LP5521_CYCLE_200ms] = {
		.cycle = 200,
		.limit = 10000,
		.cmd   = 0x4d,
	},
	[LP5521_CYCLE_500ms] = {
		.cycle = 500,
		.limit = 30000,
		.cmd   = 0x60,
	},
	[LP5521_CYCLE_700ms] = {
		.cycle = 700,
		.limit = 40000,
		.cmd   = 0x6d,
	},
	[LP5521_CYCLE_920ms] = {
		.cycle = 920,
		.limit = 50000,
		.cmd   = 0x7b,
	},
	[LP5521_CYCLE_982ms] = {
		.cycle = 982,
		.limit = 60000,
		.cmd   = 0x7f,
	},
};



/* for HW rev.C */
static struct lp5521_led_pattern board_ind_led_patterns[] = {
	{
		/* ID_POWER_ON = 1 */
		.r = mode1_red,
		.g = mode1_green,
		.b = mode1_blue,
		.size_r = ARRAY_SIZE(mode1_red),
		.size_g = ARRAY_SIZE(mode1_green),
		.size_b = ARRAY_SIZE(mode1_blue),
	},
	{
		/* ID_LCD_ON = 2 */
		.r = mode2_red,
		.g = mode2_green,
		.b = mode2_blue,
		.size_r = ARRAY_SIZE(mode2_red),
		.size_g = ARRAY_SIZE(mode2_green),
		.size_b = ARRAY_SIZE(mode2_blue),
		},
	{
		/* ID_CHARGING = 3 */
		.r = mode3_red,
		.size_r = ARRAY_SIZE(mode3_red),
	},
	{
		/* ID_CHARGING_FULL = 4 */
		.g = mode4_green,
		.size_g = ARRAY_SIZE(mode4_green),
	},
	{
		/* ID_CALENDAR_REMIND = 5 */
		.r = mode5_red,
		.g = mode5_green,
		.size_r = ARRAY_SIZE(mode5_red),
		.size_g = ARRAY_SIZE(mode5_green),
	},
	{
		/* ID_POWER_OFF = 6 */
		.r = mode6_red,
		.g = mode6_green,
		.b = mode6_blue,
		.size_r = ARRAY_SIZE(mode6_red),
		.size_g = ARRAY_SIZE(mode6_green),
		.size_b = ARRAY_SIZE(mode6_blue),
	},
	{
		/* ID_MISSED_NOTI = 7 */
		.r = mode7_red,
		.g = mode7_green,
		.b = mode7_blue,
		.size_r = ARRAY_SIZE(mode7_red),
		.size_g = ARRAY_SIZE(mode7_green),
		.size_b = ARRAY_SIZE(mode7_blue),
	},

	/* for dummy pattern IDs (defined LGLedRecord.java) */
	{
		/* ID_ALARM = 8 */
	},
	{
		/* ID_CALL_01 = 9 */
	},
	{
		/* ID_CALL_02 = 10 */
	},
	{
		/* ID_CALL_03 = 11 */
	},
	{
		/* ID_VOLUME_UP = 12 */
	},
	{
		/* ID_VOLUME_DOWN = 13 */
	},

	{
		/* ID_FAVORITE_MISSED_NOTI = 14 */
		.r = mode8_red,
		.g = mode8_green,
		.b = mode8_blue,
		.size_r = ARRAY_SIZE(mode8_red),
		.size_g = ARRAY_SIZE(mode8_green),
		.size_b = ARRAY_SIZE(mode8_blue),
	},
	{
		/* CHARGING_100_FOR_ATT = 15 (use chargerlogo, only AT&T) */
		.g = mode4_green_50,
		.size_g = ARRAY_SIZE(mode4_green_50),
	},
	{
		/* CHARGING_FOR_ATT = 16 (use chargerlogo, only AT&T) */
		.r = mode3_red_50,
		.size_r = ARRAY_SIZE(mode3_red_50),
	},
	{
		/* ID_MISSED_NOTI_PINK = 17 */
		.r = mode9_red,
		.g = mode9_green,
		.b = mode9_blue,
		.size_r = ARRAY_SIZE(mode9_red),
		.size_g = ARRAY_SIZE(mode9_green),
		.size_b = ARRAY_SIZE(mode9_blue),
	},
	{
		/* ID_MISSED_NOTI_BLUE = 18 */
		.r = mode10_red,
		.g = mode10_green,
		.b = mode10_blue,
		.size_r = ARRAY_SIZE(mode10_red),
		.size_g = ARRAY_SIZE(mode10_green),
		.size_b = ARRAY_SIZE(mode10_blue),
	},
	{
		/* ID_MISSED_NOTI_ORANGE = 19 */
		.r = mode11_red,
		.g = mode11_green,
		.b = mode11_blue,
		.size_r = ARRAY_SIZE(mode11_red),
		.size_g = ARRAY_SIZE(mode11_green),
		.size_b = ARRAY_SIZE(mode11_blue),
	},
	{
		/* ID_MISSED_NOTI_YELLOW = 20 */
		.r = mode12_red,
		.g = mode12_green,
		.b = mode12_blue,
		.size_r = ARRAY_SIZE(mode12_red),
		.size_g = ARRAY_SIZE(mode12_green),
		.size_b = ARRAY_SIZE(mode12_blue),
	},
	/* for dummy pattern IDs (defined LGLedRecord.java) */
	{
		/* ID_INCALL_PINK = 21 */
	},
	{
		/* ID_INCALL_BLUE = 22 */
	},
	{
		/* ID_INCALL_ORANGE = 23 */
	},
	{
		/* ID_INCALL_YELLOW = 24 */
	},
	{
		/* ID_INCALL_TURQUOISE = 25 */
	},
	{
		/* ID_INCALL_PURPLE = 26 */
	},
	{
		/* ID_INCALL_RED = 27 */
	},
	{
		/* ID_INCALL_LIME = 28 */
	},
	{
		/* ID_MISSED_NOTI_TURQUOISE = 29 */
		.r = mode13_red,
		.g = mode13_green,
		.b = mode13_blue,
		.size_r = ARRAY_SIZE(mode13_red),
		.size_g = ARRAY_SIZE(mode13_green),
		.size_b = ARRAY_SIZE(mode13_blue),
	},
	{
		/* ID_MISSED_NOTI_PURPLE = 30 */
		.r = mode14_red,
		.g = mode14_green,
		.b = mode14_blue,
		.size_r = ARRAY_SIZE(mode14_red),
		.size_g = ARRAY_SIZE(mode14_green),
		.size_b = ARRAY_SIZE(mode14_blue),
	},
	{
		/* ID_MISSED_NOTI_RED = 31 */
		.r = mode15_red,
		.g = mode15_green,
		.b = mode15_blue,
		.size_r = ARRAY_SIZE(mode15_red),
		.size_g = ARRAY_SIZE(mode15_green),
		.size_b = ARRAY_SIZE(mode15_blue),
	},
	{
		/* ID_MISSED_NOTI_LIME = 32 */
		.r = mode16_red,
		.g = mode16_green,
		.b = mode16_blue,
		.size_r = ARRAY_SIZE(mode16_red),
		.size_g = ARRAY_SIZE(mode16_green),
		.size_b = ARRAY_SIZE(mode16_blue),
	},
	{
		/* ID_NONE = 33 */
	},
	{
		/* ID_NONE = 34 */
	},
	{
		/* ID_INCALL = 35 */
		.r = mode17_red,
		.g = mode17_green,
		.b = mode17_blue,
		.size_r = ARRAY_SIZE(mode17_red),
		.size_g = ARRAY_SIZE(mode17_green),
		.size_b = ARRAY_SIZE(mode17_blue),
	},
	/* for dummy pattern IDs (defined LGLedRecord.java) */
	{
		/* ID_ = 36 */
	},
	{
		/* ID_INCALL = 37 */
		.r = mode18_red,
		.g = mode18_green,
		.b = mode18_blue,
		.size_r = ARRAY_SIZE(mode18_red),
		.size_g = ARRAY_SIZE(mode18_green),
		.size_b = ARRAY_SIZE(mode18_blue),
	},
};

static struct lp5521_led_config lp5521_ind_led_config[] = {
	{
		.name = "red",
		.chan_nr	= 0,
		.led_current	= 37,
		.max_current	= 37,
	},
	{
		.name = "green",
		.chan_nr	= 1,
		.led_current	= 180,
		.max_current	= 180,
	},
	{
		.name = "blue",
		.chan_nr	= 2,
		.led_current	= 90,
		.max_current	= 90,
	},
};

/* for HW rev.C */
static struct lp5521_led_config lp5521_ind_led_config_rev_c[] = {
	{
		.name = "red",
		.chan_nr	= 0,
		.led_current	= 25,
		.max_current	= 25,
	},
	{
		.name = "green",
		.chan_nr	= 1,
		.led_current	= 180,
		.max_current	= 180,
	},
	{
		.name = "blue",
		.chan_nr	= 2,
		.led_current	= 180,
		.max_current	= 180,
	},
};

/* for HW rev.B or lower */
static struct lp5521_led_config lp5521_ind_led_config_rev_b[] = {
	{
		.name = "red",
		.chan_nr	= 0,
		.led_current	= 37,
		.max_current	= 37,
	},
	{
		.name = "green",
		.chan_nr	= 1,
		.led_current	= 180,
		.max_current	= 180,
	},
	{
		.name = "blue",
		.chan_nr	= 2,
		.led_current	= 180,
		.max_current	= 180,
	},
};
#define LP5521_CONFIGS	(LP5521_PWM_HF | LP5521_PWRSAVE_EN | \
			LP5521_CP_MODE_AUTO | \
			LP5521_CLOCK_INT)

struct lp5521_chip      *chip_ind;


static void lp5521_enable(bool state)
{
	int ret = 0;
	LP5521_INFO_MSG("LP5521: [%s] state = %d\n", __func__, state);



    if(state){
		GPIO1_INDICATOR_LED_CTRL(1);
		printk("LP5521: [%s] GPIO1_INDICATOR_LED_CTRL(1) is called \n", __func__);
	//LP5521_INFO_MSG("LP5521: [%s] RGB_EN(gpio #%d) set to HIGH\n", __func__, chip_ind->rgb_led_en);
	}
	else {
		GPIO1_INDICATOR_LED_CTRL(0);
		printk("LP5521: [%s] GPIO1_INDICATOR_LED_CTRL(0) is called \n", __func__);
	//LP5521_INFO_MSG("LP5521: [%s] RGB_EN(gpio #%d) set to HIGH\n", __func__, chip_ind->rgb_led_en);
	}

	return;
}

static struct lp5521_platform_data lp5521_ind_pdata = {
	.led_config = lp5521_ind_led_config,
	.num_channels = ARRAY_SIZE(lp5521_ind_led_config),
	.clock_mode = LP5521_CLOCK_INT,
	.update_config = LP5521_CONFIGS,
	.patterns = board_ind_led_patterns,
	.num_patterns = ARRAY_SIZE(board_ind_led_patterns),
	.enable = lp5521_enable
};


static inline struct lp5521_led *cdev_to_led(struct led_classdev *cdev)
{
	return container_of(cdev, struct lp5521_led, cdev);
}

static inline struct lp5521_chip *engine_to_lp5521(struct lp5521_engine *engine)
{
	return container_of(engine, struct lp5521_chip,
			    engines[engine->id - 1]);
}

static inline struct lp5521_chip *led_to_lp5521(struct lp5521_led *led)
{
	return container_of(led, struct lp5521_chip,
			    leds[led->id]);
}

static void lp5521_led_brightness_work(struct work_struct *work);

static inline int lp5521_write(struct i2c_client *client, u8 reg, u8 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

static int lp5521_read(struct i2c_client *client, u8 reg, u8 *buf)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		return -EIO;

	*buf = ret;
	return 0;
}

static int lp5521_set_engine_mode(struct lp5521_engine *engine, u8 mode)
{
	struct lp5521_chip *chip_ind = engine_to_lp5521(engine);
	struct i2c_client *client = chip_ind->client;
	int ret;
	u8 engine_state;

	/* Only transition between RUN and DIRECT mode are handled here */
	if (mode == LP5521_CMD_LOAD)
		return 0;

	if (mode == LP5521_CMD_DISABLED)
		mode = LP5521_CMD_DIRECT;

	ret = lp5521_read(client, LP5521_REG_OP_MODE, &engine_state);
	if (ret < 0)
		return ret;

	/* set mode only for this engine */
	engine_state &= ~(engine->engine_mask);
	mode &= engine->engine_mask;
	engine_state |= mode;
	return regmap_write(chip_ind->regmap, LP5521_REG_OP_MODE, engine_state);
}

static int lp5521_load_program(struct lp5521_engine *end_ind, const u8 *pattern)
{
	printk("lp5521_load_program\n");
	struct lp5521_chip *chip_ind = engine_to_lp5521(end_ind);
	struct i2c_client *client = chip_ind->client;
	int ret;
	int addr;
	u8 mode = 0;

	printk("LP5521: [%s] is called \n", __func__);
	/* move current engine to direct mode and remember the state */
	ret = lp5521_set_engine_mode(end_ind, LP5521_CMD_DIRECT);
	/* Mode change requires min 500 us delay. 1 - 2 ms  with margin */
	usleep_range(1000, 2000);
	ret |= lp5521_read(client, LP5521_REG_OP_MODE, &mode);

	/* For loading, all the engines to load mode */
	regmap_write(chip_ind->regmap, LP5521_REG_OP_MODE, LP5521_CMD_DIRECT);
	/* Mode change requires min 500 us delay. 1 - 2 ms  with margin */
	usleep_range(1000, 2000);
	regmap_write(chip_ind->regmap, LP5521_REG_OP_MODE, LP5521_CMD_LOAD);
	/* Mode change requires min 500 us delay. 1 - 2 ms  with margin */
	usleep_range(1000, 2000);

	addr = LP5521_PROG_MEM_BASE + end_ind->prog_page * LP5521_PROG_MEM_SIZE;
	i2c_smbus_write_i2c_block_data(client,
				addr,
				LP5521_PROG_MEM_SIZE,
				pattern);

	ret |= regmap_write(chip_ind->regmap, LP5521_REG_OP_MODE, mode);
	return ret;
}

static int lp5521_set_led_current(struct lp5521_chip *chip_ind, int led, u8 curr)
{
	return regmap_write(chip_ind->regmap,
		    LP5521_REG_LED_CURRENT_BASE + chip_ind->leds[led].chan_nr,
		    curr);
}

static void lp5521_init_engine(struct lp5521_chip *chip_ind)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(chip_ind->engines); i++) {
		chip_ind->engines[i].id = i + 1;
		chip_ind->engines[i].engine_mask = LP5521_ENG_MASK_BASE >> (i * 2);
		chip_ind->engines[i].prog_page = i;
	}
}

static int lp5521_configure(struct i2c_client *client)
{
	struct lp5521_chip *chip_ind = i2c_get_clientdata(client);
	int ret;
	u8 cfg;

	lp5521_init_engine(chip_ind);

	/* Set all PWMs to direct control mode */
	ret = regmap_write(chip_ind->regmap, LP5521_REG_OP_MODE, LP5521_CMD_DIRECT);

	cfg = chip_ind->pdata->update_config ?
		: (LP5521_PWRSAVE_EN | LP5521_CP_MODE_AUTO | LP5521_R_TO_BATT);
	ret |= regmap_write(chip_ind->regmap, LP5521_REG_CONFIG, cfg);

	/* Initialize all channels PWM to zero -> leds off */
	ret |= regmap_write(chip_ind->regmap, LP5521_REG_R_PWM, 0);
	ret |= regmap_write(chip_ind->regmap, LP5521_REG_G_PWM, 0);
	ret |= regmap_write(chip_ind->regmap, LP5521_REG_B_PWM, 0);

	/* Set engines are set to run state when OP_MODE enables engines */
	ret |= regmap_write(chip_ind->regmap, LP5521_REG_ENABLE,
			LP5521_ENABLE_RUN_PROGRAM);
	/* enable takes 500us. 1 - 2 ms leaves some margin */
	usleep_range(1000, 2000);

	return ret;
}

static int lp5521_run_selftest(struct lp5521_chip *chip_ind, char *buf)
{
	int ret;
	u8 status;

	ret = lp5521_read(chip_ind->client, LP5521_REG_STATUS, &status);
	if (ret < 0)
		return ret;

	/* Check that ext clock is really in use if requested */
	if (chip_ind->pdata && chip_ind->pdata->clock_mode == LP5521_CLOCK_EXT)
		if  ((status & LP5521_EXT_CLK_USED) == 0)
			return -EIO;
	return 0;
}

static void lp5521_set_brightness(struct led_classdev *cdev,
			     enum led_brightness brightness)
{
	struct lp5521_led *led_ind = cdev_to_led(cdev);
	static unsigned long log_counter;
	led_ind->brightness = (u8)brightness;
	if (log_counter++ % 100 == 0) {
		printk("[%s] brightness : %d\n", __func__, brightness);
	}
	schedule_work(&led_ind->brightness_work);
}

static void lp5521_led_brightness_work(struct work_struct *work)
{
	struct lp5521_led *led_ind = container_of(work,
					      struct lp5521_led,
					      brightness_work);
	struct lp5521_chip *chip_ind = led_to_lp5521(led_ind);
	struct i2c_client *client = chip_ind->client;

	mutex_lock(&chip_ind->lock);
	regmap_write(chip_ind->regmap, LP5521_REG_LED_PWM_BASE + led_ind->chan_nr,
		led_ind->brightness);
	mutex_unlock(&chip_ind->lock);
}

/* Detect the chip_ind by setting its ENABLE register and reading it back. */
static int lp5521_detect(struct i2c_client *client)
{
	struct lp5521_chip *chip_ind = i2c_get_clientdata(client);
	int ret;
	u8 buf = 0;

	printk("LP5521: [%s] is called \n", __func__);
	ret = regmap_write(chip_ind->regmap, LP5521_REG_ENABLE, LP5521_ENABLE_DEFAULT);
	if (ret)
		return ret;
	/* enable takes 500us. 1 - 2 ms leaves some margin */
	usleep_range(1000, 2000);
	ret = regmap_read(chip_ind->regmap, LP5521_REG_ENABLE, &buf);
	if (ret)
		return ret;
	if (buf != LP5521_ENABLE_DEFAULT)
		return -ENODEV;

	return 0;
}

/* Set engine mode and create appropriate sysfs attributes, if required. */
static int lp5521_set_mode(struct lp5521_engine *engine, u8 mode)
{
	int ret = 0;

	/* if in that mode already do nothing, except for run */
	if (mode == engine->mode && mode != LP5521_CMD_RUN)
		return 0;

	if (mode == LP5521_CMD_RUN) {
		ret = lp5521_set_engine_mode(engine, LP5521_CMD_RUN);
	} else if (mode == LP5521_CMD_LOAD) {
		lp5521_set_engine_mode(engine, LP5521_CMD_DISABLED);
		lp5521_set_engine_mode(engine, LP5521_CMD_LOAD);
	} else if (mode == LP5521_CMD_DISABLED) {
		lp5521_set_engine_mode(engine, LP5521_CMD_DISABLED);
	}

	engine->mode = mode;

	return ret;
}

static int lp5521_do_store_load(struct lp5521_engine *engine,
				const char *buf, size_t len)
{
	struct lp5521_chip *chip_ind = engine_to_lp5521(engine);
	struct i2c_client *client = chip_ind->client;
	int  ret, nrchars, offset = 0, i = 0;
	char c[3];
	unsigned cmd;
	u8 pattern[LP5521_PROGRAM_LENGTH] = {0};

	while ((offset < len - 1) && (i < LP5521_PROGRAM_LENGTH)) {
		/* separate sscanfs because length is working only for %s */
		ret = sscanf(buf + offset, "%2s%n ", c, &nrchars);
		if (ret != 2)
			goto fail;
		ret = sscanf(c, "%2x", &cmd);
		if (ret != 1)
			goto fail;
		pattern[i] = (u8)cmd;

		offset += nrchars;
		i++;
	}

	/* Each instruction is 16bit long. Check that length is even */
	if (i % 2)
		goto fail;

	mutex_lock(&chip_ind->lock);
	if (engine->mode == LP5521_CMD_LOAD)
		ret = lp5521_load_program(engine, pattern);
	else
		ret = -EINVAL;
	mutex_unlock(&chip_ind->lock);

	if (ret) {
		dev_err(&client->dev, "failed loading pattern\n");
		return ret;
	}

	return len;
fail:
	dev_err(&client->dev, "wrong pattern format\n");
	return -EINVAL;
}

static ssize_t store_engine_load(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t len, int nr)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lp5521_chip *chip_ind = i2c_get_clientdata(client);
	return lp5521_do_store_load(&chip_ind->engines[nr - 1], buf, len);
}

#define store_load(nr)							\
static ssize_t store_engine##nr##_load(struct device *dev,		\
				     struct device_attribute *attr,	\
				     const char *buf, size_t len)	\
{									\
	return store_engine_load(dev, attr, buf, len, nr);		\
}
store_load(1)
store_load(2)
store_load(3)

static ssize_t show_engine_mode(struct device *dev,
				struct device_attribute *attr,
				char *buf, int nr)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lp5521_chip *chip_ind = i2c_get_clientdata(client);
	switch (chip_ind->engines[nr - 1].mode) {
	case LP5521_CMD_RUN:
		return sprintf(buf, "run\n");
	case LP5521_CMD_LOAD:
		return sprintf(buf, "load\n");
	case LP5521_CMD_DISABLED:
		return sprintf(buf, "disabled\n");
	default:
		return sprintf(buf, "disabled\n");
	}
}

#define show_mode(nr)							\
static ssize_t show_engine##nr##_mode(struct device *dev,		\
				    struct device_attribute *attr,	\
				    char *buf)				\
{									\
	return show_engine_mode(dev, attr, buf, nr);			\
}
show_mode(1)
show_mode(2)
show_mode(3)

static ssize_t store_engine_mode(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t len, int nr)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lp5521_chip *chip_ind = i2c_get_clientdata(client);
	struct lp5521_engine *engine = &chip_ind->engines[nr - 1];
	mutex_lock(&chip_ind->lock);

	if (!strncmp(buf, "run", 3))
		lp5521_set_mode(engine, LP5521_CMD_RUN);
	else if (!strncmp(buf, "load", 4))
		lp5521_set_mode(engine, LP5521_CMD_LOAD);
	else if (!strncmp(buf, "disabled", 8))
		lp5521_set_mode(engine, LP5521_CMD_DISABLED);

	mutex_unlock(&chip_ind->lock);
	return len;
}

#define store_mode(nr)							\
static ssize_t store_engine##nr##_mode(struct device *dev,		\
				     struct device_attribute *attr,	\
				     const char *buf, size_t len)	\
{									\
	return store_engine_mode(dev, attr, buf, len, nr);		\
}
store_mode(1)
store_mode(2)
store_mode(3)

static ssize_t show_max_current(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lp5521_led *led_ind = cdev_to_led(led_cdev);

	return sprintf(buf, "%d\n", led_ind->max_current);
}

static ssize_t show_current(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lp5521_led *led_ind = cdev_to_led(led_cdev);

	return sprintf(buf, "%d\n", led_ind->led_current);
}

static ssize_t store_current(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lp5521_led *led_ind = cdev_to_led(led_cdev);
	struct lp5521_chip *chip_ind = led_to_lp5521(led_ind);
	ssize_t ret;
	unsigned long curr;

	if (kstrtoul(buf, 0, &curr))
		return -EINVAL;

	if (curr > led_ind->max_current)
		return -EINVAL;

	mutex_lock(&chip_ind->lock);
	ret = lp5521_set_led_current(chip_ind, led_ind->id, curr);
	mutex_unlock(&chip_ind->lock);

	if (ret < 0)
		return ret;

	led_ind->led_current = (u8)curr;
	printk("[%s] brightness : %d", __func__, (u8)curr);

	return len;
}

static ssize_t lp5521_selftest(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lp5521_chip *chip_ind = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&chip_ind->lock);
	ret = lp5521_run_selftest(chip_ind, buf);
	mutex_unlock(&chip_ind->lock);
	return sprintf(buf, "%s\n", ret ? "FAIL" : "OK");
}

static void lp5521_clear_program_memory(struct lp5521_chip *chip_ind)
{
	int i;
	u8 rgb_mem[] = {
		LP5521_REG_R_PROG_MEM,
		LP5521_REG_G_PROG_MEM,
		LP5521_REG_B_PROG_MEM,
	};

	for (i = 0; i < ARRAY_SIZE(rgb_mem); i++) {
		regmap_write(chip_ind->regmap, rgb_mem[i], 0);
		regmap_write(chip_ind->regmap, rgb_mem[i] + 1, 0);
	}
}

static void lp5521_write_program_memory(struct lp5521_chip *chip_ind,
				u8 base, const u8 *rgb, int size)
{
	int i;

	if (!rgb || size <= 0)
		return;

	for (i = 0; i < size; i++)
		regmap_write(chip_ind->regmap, base + i, *(rgb + i));

	regmap_write(chip_ind->regmap, base + i, 0);
	regmap_write(chip_ind->regmap, base + i + 1, 0);
}

static inline struct lp5521_led_pattern *lp5521_get_pattern
					(struct lp5521_chip *chip_ind, u8 offset)
{
	struct lp5521_led_pattern *ptn;
	ptn = chip_ind->pdata->patterns + (offset - 1);
	return ptn;
}

static void _run_led_pattern(struct lp5521_chip *chip_ind,
			struct lp5521_led_pattern *ptn)
{
	struct i2c_client *cl = chip_ind->client;

	regmap_write(chip_ind->regmap, LP5521_REG_OP_MODE, LP5521_CMD_LOAD);
	usleep_range(1000, 2000);

	lp5521_clear_program_memory(chip_ind);

	lp5521_write_program_memory(chip_ind, LP5521_REG_R_PROG_MEM,
				ptn->r, ptn->size_r);
	lp5521_write_program_memory(chip_ind, LP5521_REG_G_PROG_MEM,
				ptn->g, ptn->size_g);
	lp5521_write_program_memory(chip_ind, LP5521_REG_B_PROG_MEM,
				ptn->b, ptn->size_b);

	regmap_write(chip_ind->regmap, LP5521_REG_OP_MODE, LP5521_CMD_RUN);
	usleep_range(1000, 2000);
	regmap_write(chip_ind->regmap, LP5521_REG_ENABLE, LP5521_ENABLE_RUN_PROGRAM);
}

static void lp5521_run_led_pattern(int mode, struct lp5521_chip *chip_ind)
{
	struct lp5521_led_pattern *ptn;
	struct i2c_client *cl = chip_ind->client;
	int num_patterns = chip_ind->pdata->num_patterns;

	if (mode >= 1000) {
		mode = mode - 1000;
	}

	chip_ind->id_pattern_play = mode;

	if (mode > num_patterns || !(chip_ind->pdata->patterns)) {
		chip_ind->id_pattern_play = PATTERN_OFF;
		printk("[%s] invalid pattern!", __func__);
		return;
	}

	if (mode == PATTERN_OFF) {
		regmap_write(chip_ind->regmap, LP5521_REG_ENABLE, LP5521_ENABLE_DEFAULT);
		usleep_range(1000, 2000);
		regmap_write(chip_ind->regmap, LP5521_REG_OP_MODE, LP5521_CMD_DIRECT);

	} else {
		ptn = lp5521_get_pattern(chip_ind, mode);
		if (!ptn)
			return;

		_run_led_pattern(chip_ind, ptn);

	}
}

static u8 get_led_current_value(u8 current_index)
{
	return current_index_mapped_value[current_index];
}

static ssize_t show_led_pattern(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct lp5521_chip *chip_ind = i2c_get_clientdata(to_i2c_client(dev));

	return sprintf(buf, "%d\n", chip_ind->id_pattern_play);
}

static ssize_t store_led_pattern(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct lp5521_chip *chip_ind = i2c_get_clientdata(to_i2c_client(dev));
	unsigned long val;
	int ret;
	printk("[%s] pattern id : %s", __func__, buf);

	ret = strict_strtoul(buf, 10, &val);
	if (ret)
		return ret;
	lp5521_run_led_pattern(val, chip_ind);

	return len;
}

static ssize_t show_led_current_index(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct lp5521_chip *chip_ind = i2c_get_clientdata(to_i2c_client(dev));
	if (!chip_ind)
		return 0;

	return sprintf(buf, "%d\n", chip_ind->current_index);
}

static ssize_t store_led_current_index(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct lp5521_chip *chip_ind = i2c_get_clientdata(to_i2c_client(dev));
	unsigned long val;
	int ret, i;
	u8 max_current, modify_current;

	printk("[%s] current index (0~255) : %s", __func__, buf);

	ret = strict_strtoul(buf, 10, &val);
	if (ret)
		return ret;

	if (val > PATTERN_CURRENT_INDEX_STEP_HAL || val < 0)
		return -EINVAL;

	if (!chip_ind)
		return 0;

	chip_ind->current_index = val;

	mutex_lock(&chip_ind->lock);
	for (i = 0; i < LP5521_MAX_LEDS ; i++) {
		max_current = chip_ind->leds[i].max_current;
		modify_current = get_led_current_value(val);
		if (modify_current > max_current)
			modify_current = max_current;
		printk("[%s] modify_current : %d", __func__,  modify_current);
		ret = lp5521_set_led_current(chip_ind, i, modify_current);
		if (ret)
			break;
		chip_ind->leds[i].led_current = modify_current;
	}
	mutex_unlock(&chip_ind->lock);

	if (ret)
		return ret;
	return len;
}

static void _set_pwm_cmd(struct lp5521_pattern_cmd *cmd, unsigned int color)
{
	u8 r = (color >> 16) & 0xFF;
	u8 g = (color >> 8) & 0xFF;
	u8 b = color & 0xFF;

	printk("LP5521: [%s] is called \n", __func__);
	cmd->r[cmd->pc_r++] = CMD_SET_PWM;
	cmd->r[cmd->pc_r++] = r;
	cmd->g[cmd->pc_g++] = CMD_SET_PWM;
	cmd->g[cmd->pc_g++] = g;
	cmd->b[cmd->pc_b++] = CMD_SET_PWM;
	cmd->b[cmd->pc_b++] = b;
}

static enum lp5521_wait_type _find_wait_cycle_type(unsigned int ms)
{
	int i;

	for (i = LP5521_CYCLE_50ms ; i < LP5521_CYCLE_MAX ; i++) {
		if (ms > lp5521_wait_params[i-1].limit &&
		    ms <= lp5521_wait_params[i].limit)
			return i;
	}

	return LP5521_CYCLE_INVALID;
}

static void _set_wait_cmd(struct lp5521_pattern_cmd *cmd,
			unsigned int ms, u8 jump, unsigned int off)
{
	enum lp5521_wait_type type = _find_wait_cycle_type(ms);
	unsigned int loop = ms / lp5521_wait_params[type].cycle;
	u8 cmd_msb = lp5521_wait_params[type].cmd;
	u8 msb;
	u8 lsb;
	u16 branch;

	WARN_ON(!cmd_msb);
	WARN_ON(loop > 64);

        if(off)
        {
            if(loop > 1)
            {
                if(loop > 128)
                    loop = 128;

                lsb = ((loop-1) & 0xff) | 0x80;
                /* wait command */
                cmd->r[cmd->pc_r++] = cmd_msb;
                cmd->r[cmd->pc_r++] = lsb;
                cmd->g[cmd->pc_g++] = cmd_msb;
                cmd->g[cmd->pc_g++] = lsb;
                cmd->b[cmd->pc_b++] = cmd_msb;
                cmd->b[cmd->pc_b++] = lsb;
            }
            else
            {
                /* wait command */
                cmd->r[cmd->pc_r++] = cmd_msb;
                cmd->r[cmd->pc_r++] = CMD_WAIT_LSB;
                cmd->g[cmd->pc_g++] = cmd_msb;
                cmd->g[cmd->pc_g++] = CMD_WAIT_LSB;
                cmd->b[cmd->pc_b++] = cmd_msb;
                cmd->b[cmd->pc_b++] = CMD_WAIT_LSB;
            }
        }
        else
        {
            /* wait command */
            cmd->r[cmd->pc_r++] = cmd_msb;
            cmd->r[cmd->pc_r++] = CMD_WAIT_LSB;
            cmd->g[cmd->pc_g++] = cmd_msb;
            cmd->g[cmd->pc_g++] = CMD_WAIT_LSB;
            cmd->b[cmd->pc_b++] = cmd_msb;
            cmd->b[cmd->pc_b++] = CMD_WAIT_LSB;

	/* branch command : if wait time is bigger than cycle msec,
			branch is used for command looping */
	if (loop > 1) {
		branch = (5 << 13) | ((loop - 1) << 7) | jump;
		msb = (branch >> 8) & 0xFF;
		lsb = branch & 0xFF;

		cmd->r[cmd->pc_r++] = msb;
		cmd->r[cmd->pc_r++] = lsb;
		cmd->g[cmd->pc_g++] = msb;
		cmd->g[cmd->pc_g++] = lsb;
		cmd->b[cmd->pc_b++] = msb;
		cmd->b[cmd->pc_b++] = lsb;
	}
}
}
inline bool ind_is_pc_overflow(struct lp5521_led_pattern *ptn_ind)
{
	return (ptn_ind->size_r >= LP5521_PROGRAM_LENGTH ||
		ptn_ind->size_g >= LP5521_PROGRAM_LENGTH ||
		ptn_ind->size_b >= LP5521_PROGRAM_LENGTH);
}

static ssize_t store_led_blink(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct lp5521_chip *chip_ind = i2c_get_clientdata(to_i2c_client(dev));
	unsigned int rgb = 0;
	unsigned int on = 0;
	unsigned int off = 0;
	struct lp5521_led_pattern ptn_ind = { };
	struct lp5521_pattern_cmd cmd = { };
	u8 jump_pc = 0;

	sscanf(buf, "0x%06x %d %d", &rgb, &on, &off);
	printk("[%s] rgb=0x%06x, on=%d, off=%d\n",__func__, rgb, on, off);
	lp5521_run_led_pattern(PATTERN_OFF, chip_ind);

	on = min_t(unsigned int, on, MAX_BLINK_TIME);
	off = min_t(unsigned int, off, MAX_BLINK_TIME);

	if (!rgb || !on || !off) {
		chip_ind->id_pattern_play = PATTERN_OFF;
		return len;
	} else {
		chip_ind->id_pattern_play = PATTERN_BLINK_ON;
	}

	/* on */
	_set_pwm_cmd(&cmd, rgb);
	_set_wait_cmd(&cmd, on, jump_pc, 0);
	jump_pc = cmd.pc_r / 2; /* 16bit size program counter */

	/* off */
	_set_pwm_cmd(&cmd, 0);
	_set_wait_cmd(&cmd, off, jump_pc, 1);

	ptn_ind.r = cmd.r;
	ptn_ind.size_r = cmd.pc_r;
	ptn_ind.g = cmd.g;
	ptn_ind.size_g = cmd.pc_g;
	ptn_ind.b = cmd.b;
	ptn_ind.size_b = cmd.pc_b;

	WARN_ON(ind_is_pc_overflow(&ptn_ind));

	_run_led_pattern(chip_ind, &ptn_ind);

	return len;
}

/* led_ind class device attributes */
static DEVICE_ATTR(led_current, S_IRUGO | S_IWUSR, show_current, store_current);
static DEVICE_ATTR(max_current, S_IRUGO , show_max_current, NULL);

static struct attribute *lp5521_led_attributes[] = {
	&dev_attr_led_current.attr,
	&dev_attr_max_current.attr,
	NULL,
};

static struct attribute_group lp5521_led_attribute_group = {
	.attrs = lp5521_led_attributes
};

/* device attributes */
static DEVICE_ATTR(engine1_mode, S_IRUGO | S_IWUSR,
		   show_engine1_mode, store_engine1_mode);
static DEVICE_ATTR(engine2_mode, S_IRUGO | S_IWUSR,
		   show_engine2_mode, store_engine2_mode);
static DEVICE_ATTR(engine3_mode, S_IRUGO | S_IWUSR,
		   show_engine3_mode, store_engine3_mode);
static DEVICE_ATTR(engine1_load, S_IWUSR, NULL, store_engine1_load);
static DEVICE_ATTR(engine2_load, S_IWUSR, NULL, store_engine2_load);
static DEVICE_ATTR(engine3_load, S_IWUSR, NULL, store_engine3_load);
static DEVICE_ATTR(selftest, S_IRUGO, lp5521_selftest, NULL);
static DEVICE_ATTR(led_pattern, S_IRUGO | S_IWUSR, show_led_pattern, store_led_pattern);
static DEVICE_ATTR(led_blink, S_IRUGO | S_IWUSR, NULL, store_led_blink);
static DEVICE_ATTR(led_current_index, S_IRUGO | S_IWUSR, show_led_current_index, store_led_current_index);

static struct attribute *lp5521_ind_attributes[] = {
	&dev_attr_engine1_mode.attr,
	&dev_attr_engine2_mode.attr,
	&dev_attr_engine3_mode.attr,
	&dev_attr_selftest.attr,
	&dev_attr_engine1_load.attr,
	&dev_attr_engine2_load.attr,
	&dev_attr_engine3_load.attr,
	&dev_attr_led_pattern.attr,
	&dev_attr_led_blink.attr,
	&dev_attr_led_current_index.attr,
	NULL
};

static const struct attribute_group lp5521_ind_group = {
	.attrs = lp5521_ind_attributes,
};

static int lp5521_register_sysfs(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	return sysfs_create_group(&dev->kobj, &lp5521_ind_group);
}

static void lp5521_unregister_sysfs(struct i2c_client *client)
{
	struct lp5521_chip *chip_ind = i2c_get_clientdata(client);
	struct device *dev = &client->dev;
	int i;

	sysfs_remove_group(&dev->kobj, &lp5521_ind_group);

	for (i = 0; i < chip_ind->num_leds; i++)
		sysfs_remove_group(&chip_ind->leds[i].cdev.dev->kobj,
				&lp5521_led_attribute_group);
}

static int lp5521_init_led(struct lp5521_led *led_ind,
				struct i2c_client *client,
				int chan, struct lp5521_platform_data pdata)
{
	struct device *dev = &client->dev;
	char name[32];
	int res;

	if (chan >= LP5521_MAX_LEDS)
		return -EINVAL;

	if (lp5521_ind_pdata.led_config[chan].led_current == 0)
		return 0;

	led_ind->led_current = lp5521_ind_pdata.led_config[chan].led_current;
	led_ind->max_current = lp5521_ind_pdata.led_config[chan].max_current;
	led_ind->chan_nr = lp5521_ind_pdata.led_config[chan].chan_nr;

	if (led_ind->chan_nr >= LP5521_MAX_LEDS) {
		dev_err(dev, "Use channel numbers between 0 and %d\n",
			LP5521_MAX_LEDS - 1);
		return -EINVAL;
	}

	led_ind->cdev.brightness_set = lp5521_set_brightness;
	if (lp5521_ind_pdata.led_config[chan].name) {
		led_ind->cdev.name = lp5521_ind_pdata.led_config[chan].name;
	} else {
		snprintf(name, sizeof(name), "%s:channel%d",
			lp5521_ind_pdata.label ?: client->name, chan);
		led_ind->cdev.name = name;
	}

	res = led_classdev_register(dev, &led_ind->cdev);
	if (res < 0) {
		dev_err(dev, "couldn't register led_ind on channel %d\n", chan);
		return res;
	}

	res = sysfs_create_group(&led_ind->cdev.dev->kobj,
			&lp5521_led_attribute_group);
	if (res < 0) {
		dev_err(dev, "couldn't register current attribute\n");
		led_classdev_unregister(&led_ind->cdev);
		return res;
	}
	return 0;
}


static int lp5521_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{

	int ret =0, i, led_ind;
	u8 buf = 0;
	u8 buf2 = 0;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct lp5521_chip *chip_ind;
	//struct lp5521_platform_data *pdata;
	int err = 0;
	int rc;
	unsigned int tmp;
	struct ipc_client *regmap_ipc;
	printk("[%s] deprecated start\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		return -ENOMEM;
	}
#ifdef CONFIG_OF
    if (&client->dev.of_node)
    {
        chip_ind = devm_kzalloc(&client->dev, sizeof(struct lp5521_chip), GFP_KERNEL);
        if (!chip_ind)
        {
            pr_err("%s: Failed to allocate memory\n", __func__);
            return -ENOMEM;
        }

		rc = of_property_read_u32(client->dev.of_node, "ti,led_en", &tmp);
		chip_ind->rgb_led_en = tmp;

		regmap_ipc = kzalloc(sizeof(struct ipc_client), GFP_KERNEL);

		if (regmap_ipc == NULL) {
			pr_err("%s : Platform data not specified.\n", __func__);
			return -ENOMEM;
		}

		regmap_ipc->slot = LP5521_I2C_SLOT;
		regmap_ipc->addr = LP5521_SLAVE_ADDR;
		regmap_ipc->dev  = &client->dev;

		chip_ind->regmap = devm_regmap_init_ipc(regmap_ipc, &lp5521_ind_regmap_config);

        }

#else
	chip_ind = devm_kzalloc(&client->dev, sizeof(*chip_ind), GFP_KERNEL);
	if (!chip_ind) {
		printk("[%s] Can not allocate memory!\n", __func__);
		return -ENOMEM;
	}
#endif

	i2c_set_clientdata(client, chip_ind);
	chip_ind->client = client;

	mutex_init(&chip_ind->lock);
	lp5521_ind_pdata.led_config = lp5521_ind_led_config_rev_c;

	chip_ind->pdata = &lp5521_ind_pdata;

    	GPIO1_INDICATOR_LED_CTRL(0);
	usleep_range(1000, 2000);

   	GPIO1_INDICATOR_LED_CTRL(1);
	usleep_range(1000, 2000); /* Keep enable down at least 1ms */
	ret = regmap_write(chip_ind->regmap, LP5521_REG_RESET, 0xff);
	if (ret) {
		printk("[lp5521_indicator probe] ret = regmap_write (fail) \n");
	}
	usleep_range(10000, 20000); /*
				     * Exact value is not available. 10 - 20ms
				     * appears to be enough for reset.
				     */

	/*
	 * Make sure that the chip_ind is reset by reading back the r channel
	 * current reg. This is dummy read is required on some platforms -
	 * otherwise further access to the R G B channels in the
	 * LP5521_REG_ENABLE register will not have any effect - strange!
	 */
	ret = regmap_read(chip_ind->regmap, LP5521_REG_R_CURRENT, &buf);

	if (ret || buf != LP5521_REG_R_CURR_DEFAULT) {
		dev_err(&client->dev, "error in resetting chip_ind\n");
		goto fail2;
	}
	usleep_range(10000, 20000);

	ret = lp5521_detect(client);

	if (ret) {
		dev_err(&client->dev, "chip_ind not found\n");
		goto fail2;
	}

	dev_info(&client->dev, "%s programmable led_ind chip_ind found\n", id->name);

	ret = lp5521_configure(client);
	if (ret < 0) {
		dev_err(&client->dev, "error configuring chip_ind\n");
		goto fail1;
	}

	/* Initialize leds */
	chip_ind->num_channels = lp5521_ind_pdata.num_channels;
	chip_ind->num_leds = 0;
	led_ind = 0;
	for (i = 0; i < lp5521_ind_pdata.num_channels; i++) {
		/* Do not initialize channels that are not connected */
		if (lp5521_ind_pdata.led_config[i].led_current == 0)
			continue;

		ret = lp5521_init_led(&chip_ind->leds[led_ind], client, i, lp5521_ind_pdata);
		if (ret) {
			dev_err(&client->dev, "error initializing leds\n");
			goto fail2;
		}
		chip_ind->num_leds++;

		chip_ind->leds[led_ind].id = led_ind;
		/* Set initial LED current */
		lp5521_set_led_current(chip_ind, led_ind,
				chip_ind->leds[led_ind].led_current);

		INIT_WORK(&(chip_ind->leds[led_ind].brightness_work),
			lp5521_led_brightness_work);

		led_ind++;
	}

	/* Initialize current index for auto brightness (max step) */
	chip_ind->current_index = PATTERN_CURRENT_INDEX_STEP_HAL;

	ret = lp5521_register_sysfs(client);
	if (ret) {
		dev_err(&client->dev, "registering sysfs failed\n");
		goto fail2;
	}

	/*tempolary delete start pattern off because system UI does not working */
#if defined(CONFIG_LGE_PM_FACTORY_TESTMODE) || defined(CONFIG_LAF_G_DRIVER)
	if(!lp5521_start_pattern_disable_condition())
#endif
	if (lge_get_boot_mode() == LGE_BOOT_MODE_NORMAL) {
	lp5521_run_led_pattern(1, chip_ind); //1: Power On pattern number
	}
	//LP5521_INFO_MSG("[%s] pattern id : 1(Power on)", __func__);
	LP5521_INFO_MSG("[%s] indicator complete\n", __func__);

	return ret;
fail2:
	for (i = 0; i < chip_ind->num_leds; i++) {
		led_classdev_unregister(&chip_ind->leds[i].cdev);
		cancel_work_sync(&chip_ind->leds[i].brightness_work);
	}
fail1:
    {
        if (lp5521_ind_pdata.enable)
	        lp5521_ind_pdata.enable(0);
    }
fail0:
    gpio_free(chip_ind->rgb_led_en);

	return ret;
}

static int lp5521_remove(struct i2c_client *client)
{
	struct lp5521_chip *chip_ind = i2c_get_clientdata(client);
	int i;

	printk("[%s] start\n", __func__);
	lp5521_run_led_pattern(PATTERN_OFF, chip_ind);
	lp5521_unregister_sysfs(client);

	for (i = 0; i < chip_ind->num_leds; i++) {
		led_classdev_unregister(&chip_ind->leds[i].cdev);
		cancel_work_sync(&chip_ind->leds[i].brightness_work);
	}

	if (chip_ind->pdata->enable)
		chip_ind->pdata->enable(0);


	if (chip_ind->pdata->release_resources)
		chip_ind->pdata->release_resources();

	devm_kfree(&client->dev, chip_ind);
	printk("[%s] complete\n", __func__);
	return 0;
}

static void lp5521_shutdown(struct i2c_client *client)
{
	struct lp5521_chip *chip_ind = i2c_get_clientdata(client);
	int i;

	if (!chip_ind) {
		printk("[%s] null pointer check!\n", __func__);
		return;
	}

	printk("[%s] start\n", __func__);
	lp5521_set_led_current(chip_ind, 0, 0);
	lp5521_set_led_current(chip_ind, 1, 0);
	lp5521_set_led_current(chip_ind, 2, 0);
	lp5521_run_led_pattern(PATTERN_OFF, chip_ind);
	lp5521_unregister_sysfs(client);

	for (i = 0; i < chip_ind->num_leds; i++) {
		led_classdev_unregister(&chip_ind->leds[i].cdev);
		cancel_work_sync(&chip_ind->leds[i].brightness_work);
	}

	if (chip_ind->pdata->enable)
		chip_ind->pdata->enable(0);


	if (chip_ind->pdata->release_resources)
		chip_ind->pdata->release_resources();

	devm_kfree(&client->dev, chip_ind);
	printk("[%s] complete\n", __func__);
}



static int lp5521_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct lp5521_chip *chip_ind = i2c_get_clientdata(client);

	if (!chip_ind || !chip_ind->pdata) {
		//printk("[%s] null pointer check!\n", __func__);
	return 0;
}

	//printk("[%s] id_pattern_play = %d\n", __func__, chip_ind->id_pattern_play);
   // {
	    if (chip_ind->pdata->enable && chip_ind->id_pattern_play == PATTERN_OFF) {
		   // printk("[%s] RGB_EN set to LOW\n", __func__);
		    chip_ind->pdata->enable(0);
	    }
  //  }
	return 0;
}

static int lp5521_resume(struct i2c_client *client)
{
	struct lp5521_chip *chip_ind = i2c_get_clientdata(client);
	int ret = 0;

	if (!chip_ind || !chip_ind->pdata) {
		//printk("[%s] null pointer check!\n", __func__);
		return 0;
	}

	//printk("[%s] id_pattern_play = %d\n", __func__, chip_ind->id_pattern_play);

	    if (chip_ind->pdata->enable && chip_ind->id_pattern_play == PATTERN_OFF) {
		   // printk("[%s] RGB_EN set to HIGH\n", __func__);
#if 0  // modify for I2c fail when resume
		    chip_ind->pdata->enable(0);
		    usleep_range(1000, 2000); /* Keep enable down at least 1ms */
#endif
		    chip_ind->pdata->enable(1);
		    usleep_range(1000, 2000); /* 500us abs min. */
#if 0  // modify for I2c fail when resume
		    regmap_write(chip_ind->regmap, LP5521_REG_RESET, 0xff);
		    usleep_range(10000, 20000);
		    ret = lp5521_configure(client);
		    if (ret < 0) {
			    dev_err(&client->dev, "error configuring chip_ind\n");
		    }
#endif
		}
    	else {
	    	if (chip_ind->id_pattern_play == PATTERN_OFF) {
			//printk("[%s] RGB_EN set to HIGH\n", __func__);
		    	regmap_write(chip_ind->regmap, LP5521_REG_RESET, 0xff);
		    	usleep_range(10000, 20000);
		    	ret = lp5521_configure(client);
		    	if (ret < 0) {
			    	dev_err(&client->dev, "error configuring chip_ind\n");
		    	}
        	}
    	}
		return ret;
}

static const struct i2c_device_id lp5521_id[] = {
	{ "lp5521_ind", 0 }, /* Three channel chip_ind */
	{ }
};

#ifdef CONFIG_OF
static struct of_device_id lp5521_match_table[] = {
	{ .compatible = "ti,lp5521_ind",},
	{ },
};
#endif

static struct i2c_driver lp5521_ind_driver = {
	.driver = {
        .owner  = THIS_MODULE,
		.name	= "lp5521_ind",
#ifdef CONFIG_OF
		.of_match_table = lp5521_match_table,
#endif
	},
	.probe		= lp5521_probe,
	.remove		= lp5521_remove,
	.shutdown	= lp5521_shutdown,
	.suspend	= lp5521_suspend,
	.resume		= lp5521_resume,
	.id_table	= lp5521_id,
};

/*
 * module load/unload record keeping
 */

static int __init lp5521_ind_dev_init(void)
{
	return i2c_add_driver(&lp5521_ind_driver);
}
module_init(lp5521_ind_dev_init);

static void __exit lp5521_ind_dev_exit(void)
{
	i2c_del_driver(&lp5521_ind_driver);
}
module_exit(lp5521_ind_dev_exit);

MODULE_DEVICE_TABLE(i2c, lp5521_id);

MODULE_AUTHOR("Mathias Nyman, Yuri Zaporozhets, Samu Onkalo");
MODULE_DESCRIPTION("LP5521 LED engine");
MODULE_LICENSE("GPL v2");
