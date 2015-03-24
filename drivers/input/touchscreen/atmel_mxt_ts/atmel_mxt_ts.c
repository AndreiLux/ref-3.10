/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Copyright (C) 2011 Atmel Corporation
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#include <mach/mt_pm_ldo.h>

#include "atmel_mxt_ts.h"

/* Driver version */
#define MXT_DRIVER_VER_MAJOR    0x011
#define MXT_DRIVER_VER_MINOR    0x13

/* Family ID */
#define MXT224_ID		0x80
#define MXT768E_ID		0xA1
#define MXT1386_ID		0xA0

/* Version */
#define MXT_VER_20		20
#define MXT_VER_21		21
#define MXT_VER_22		22

/* Firmware files */
#define MXT_FW_NAME		"maxtouch.fw"
#define MXT_CFG_NAME		"maxtouch.cfg"
#define MXT_CFG_MAGIC		"OBP_RAW V1"

/* Registers */
#define MXT_FAMILY_ID		0x00
#define MXT_VARIANT_ID		0x01
#define MXT_VERSION		0x02
#define MXT_BUILD		0x03
#define MXT_MATRIX_X_SIZE	0x04
#define MXT_MATRIX_Y_SIZE	0x05
#define MXT_OBJECT_NUM		0x06
#define MXT_OBJECT_START	0x07

#define MXT_MAX_BLOCK_WRITE	768

/* Object types */
#define MXT_DEBUG_DIAGNOSTIC_T37	37
#define MXT_GEN_MESSAGE_T5		5
#define MXT_GEN_COMMAND_T6		6
#define MXT_GEN_POWER_T7		7
#define MXT_GEN_ACQUIRE_T8		8
#define MXT_GEN_DATASOURCE_T53		53
#define MXT_TOUCH_MULTI_T9		9
#define MXT_TOUCH_KEYARRAY_T15		15
#define MXT_TOUCH_PROXIMITY_T23		23
#define MXT_TOUCH_PROXKEY_T52		52
#define MXT_PROCI_GRIPFACE_T20		20
#define MXT_PROCG_NOISE_T22		22
#define MXT_PROCI_ONETOUCH_T24		24
#define MXT_PROCI_TWOTOUCH_T27		27
#define MXT_PROCI_GRIP_T40		40
#define MXT_PROCI_PALM_T41		41
#define MXT_PROCI_TOUCHSUPPRESSION_T42	42
#define MXT_PROCI_STYLUS_T47		47
#define MXT_PROCG_NOISESUPPRESSION_T48	48
#define MXT_SPT_COMMSCONFIG_T18		18
#define MXT_SPT_GPIOPWM_T19		19
#define MXT_SPT_SELFTEST_T25		25
#define MXT_SPT_CTECONFIG_T28		28
#define MXT_SPT_USERDATA_T38		38
#define MXT_SPT_DIGITIZER_T43		43
#define MXT_SPT_MESSAGECOUNT_T44	44
#define MXT_SPT_CTECONFIG_T46		46
#define MXT_SPT_NOISESUPPRESSION_T48    48

/* MXT_GEN_MESSAGE_T5 object */
#define MXT_RPTID_NOMSG		0xff

/* MXT_GEN_COMMAND_T6 field */
#define MXT_COMMAND_RESET	0
#define MXT_COMMAND_BACKUPNV	1
#define MXT_COMMAND_CALIBRATE	2
#define MXT_COMMAND_REPORTALL	3
#define MXT_COMMAND_DIAGNOSTIC	5

/* MXT_GEN_POWER_T7 field */
#define MXT_POWER_IDLEACQINT	0
#define MXT_POWER_ACTVACQINT	1
#define MXT_POWER_ACTV2IDLETO	2

#define MXT_POWER_CFG_RUN	0
#define MXT_POWER_CFG_DEEPSLEEP	1

/* MXT_GEN_ACQUIRE_T8 field */
#define MXT_ACQUIRE_CHRGTIME	0
#define MXT_ACQUIRE_TCHDRIFT	2
#define MXT_ACQUIRE_DRIFTST	3
#define MXT_ACQUIRE_TCHAUTOCAL	4
#define MXT_ACQUIRE_SYNC	5
#define MXT_ACQUIRE_ATCHCALST	6
#define MXT_ACQUIRE_ATCHCALSTHR	7
#define MXT_ACQUIRE_ATCHFRCCALTHR   8
#define MXT_ACQUIRE_ATCHFRCCALRATIO 9

/* MXT_TOUCH_MULTI_T9 field */
#define MXT_TOUCH_CTRL		0
#define MXT_TOUCH_XORIGIN	1
#define MXT_TOUCH_YORIGIN	2
#define MXT_TOUCH_XSIZE		3
#define MXT_TOUCH_YSIZE		4
#define MXT_TOUCH_BLEN		6
#define MXT_TOUCH_TCHTHR	7
#define MXT_TOUCH_TCHDI		8
#define MXT_TOUCH_ORIENT	9
#define MXT_TOUCH_MOVHYSTI	11
#define MXT_TOUCH_MOVHYSTN	12
#define MXT_TOUCH_NUMTOUCH	14
#define MXT_TOUCH_MRGHYST	15
#define MXT_TOUCH_MRGTHR	16
#define MXT_TOUCH_AMPHYST	17
#define MXT_TOUCH_XRANGE_LSB	18
#define MXT_TOUCH_XRANGE_MSB	19
#define MXT_TOUCH_YRANGE_LSB	20
#define MXT_TOUCH_YRANGE_MSB	21
#define MXT_TOUCH_XLOCLIP	22
#define MXT_TOUCH_XHICLIP	23
#define MXT_TOUCH_YLOCLIP	24
#define MXT_TOUCH_YHICLIP	25
#define MXT_TOUCH_XEDGECTRL	26
#define MXT_TOUCH_XEDGEDIST	27
#define MXT_TOUCH_YEDGECTRL	28
#define MXT_TOUCH_YEDGEDIST	29
#define MXT_TOUCH_JUMPLIMIT	30

/* MXT_PROCI_GRIPFACE_T20 field */
#define MXT_GRIPFACE_CTRL	0
#define MXT_GRIPFACE_XLOGRIP	1
#define MXT_GRIPFACE_XHIGRIP	2
#define MXT_GRIPFACE_YLOGRIP	3
#define MXT_GRIPFACE_YHIGRIP	4
#define MXT_GRIPFACE_MAXTCHS	5
#define MXT_GRIPFACE_SZTHR1	7
#define MXT_GRIPFACE_SZTHR2	8
#define MXT_GRIPFACE_SHPTHR1	9
#define MXT_GRIPFACE_SHPTHR2	10
#define MXT_GRIPFACE_SUPEXTTO	11

/* MXT_PROCI_NOISE field */
#define MXT_NOISE_CTRL		0
#define MXT_NOISE_OUTFLEN	1
#define MXT_NOISE_GCAFUL_LSB	3
#define MXT_NOISE_GCAFUL_MSB	4
#define MXT_NOISE_GCAFLL_LSB	5
#define MXT_NOISE_GCAFLL_MSB	6
#define MXT_NOISE_ACTVGCAFVALID	7
#define MXT_NOISE_NOISETHR	8
#define MXT_NOISE_FREQHOPSCALE	10
#define MXT_NOISE_FREQ0		11
#define MXT_NOISE_FREQ1		12
#define MXT_NOISE_FREQ2		13
#define MXT_NOISE_FREQ3		14
#define MXT_NOISE_FREQ4		15
#define MXT_NOISE_IDLEGCAFVALID	16

/* MXT_SPT_COMMSCONFIG_T18 */
#define MXT_COMMS_CTRL		0
#define MXT_COMMS_CMD		1
#define MXT_COMMS_RETRIGEN      (1 << 6)

/* MXT_SPT_CTECONFIG_T28 field */
#define MXT_CTE_CTRL		0
#define MXT_CTE_CMD		1
#define MXT_CTE_MODE		2
#define MXT_CTE_IDLEGCAFDEPTH	3
#define MXT_CTE_ACTVGCAFDEPTH	4
#define MXT_CTE_VOLTAGE		5

#define MXT_VOLTAGE_DEFAULT	2700000
#define MXT_VOLTAGE_STEP	10000

/* Define for MXT_GEN_COMMAND_T6 */
#define MXT_BOOT_VALUE		0xa5
#define MXT_RESET_VALUE		0x01
#define MXT_BACKUP_VALUE	0x55

#define MXT_T6_MSG_RESET	(1 << 7)
#define MXT_T6_MSG_CAL		(1 << 4)
#define MXT_T6_MSG_CFGERR	(1 << 3)

/* Define for MXT_PROCG_NOISESUPPRESSION_T42 */
#define MXT_T42_MSG_TCHSUP	(1 << 0)

/* Define for MXT_PROCG_NOISESUPPRESSION_T48 */
#define MXT_T48_CTRL            0
#define MXT_T48_CFG             1
#define MXT_T48_CALCFG          2
#define MXT_T48_NLTHR           11

#define MXT_T48_CTRL_ENABLE     (0x1 << 0)	/* T48 enable */
#define MXT_T48_CTRL_RPTEN      (0x1 << 1)	/* report enable */
#define MXT_T48_CTRL_RPTNLVL    (0x1 << 4)	/* noise level report enable */
#define MXT_T48_CALCFG_CHRGON   (0x1 << 5)	/* charger enable bit */
#define MXT_T48_CALCFG_DUALX    (0x1 << 4)	/* dualX enable bit */

#define MXT_T48_MSG_STATUS_FREQCHG  (0x1 << 0)	/* Frequency changed */
#define MXT_T48_MSG_STATUS_APXCHG   (0x1 << 1)	/* number of ADC changed */
#define MXT_T48_MSG_STATUS_ALGOERR  (0x1 << 2)	/* algorithm error */
#define MXT_T48_MSG_STATE_ERR       5	/* Noise suppression state: ERROR */

/* Delay times */
#define MXT_BACKUP_TIME		25	/* msec */
#define MXT224_RESET_TIME	65	/* msec */
#define MXT768E_RESET_TIME	250	/* msec */
#define MXT1386_RESET_TIME	200	/* msec */
#define MXT_RESET_TIME		200	/* msec */
#define MXT_RESET_NOCHGREAD	400	/* msec */
#define MXT_FWRESET_TIME	1000	/* msec */
#define MXT_POWERON_DELAY	400	/* msec */

/* Command to unlock bootloader */
#define MXT_UNLOCK_CMD_MSB	0xaa
#define MXT_UNLOCK_CMD_LSB	0xdc

/* Bootloader mode status */
#define MXT_WAITING_BOOTLOAD_CMD	0xc0	/* valid 7 6 bit only */
#define MXT_WAITING_FRAME_DATA	0x80	/* valid 7 6 bit only */
#define MXT_FRAME_CRC_CHECK	0x02
#define MXT_FRAME_CRC_FAIL	0x03
#define MXT_FRAME_CRC_PASS	0x04
#define MXT_APP_CRC_FAIL	0x40	/* valid 7 8 bit only */
#define MXT_BOOT_STATUS_MASK	0x3f
#define MXT_BOOT_EXTENDED_ID	(1 << 5)
#define MXT_BOOT_ID_MASK	0x1f

/* Command process status */
#define MXT_STATUS_CFGERROR	(1 << 3)

/* Touch status */
#define MXT_UNGRIP		(1 << 0)
#define MXT_SUPPRESS		(1 << 1)
#define MXT_AMP			(1 << 2)
#define MXT_VECTOR		(1 << 3)
#define MXT_MOVE		(1 << 4)
#define MXT_RELEASE		(1 << 5)
#define MXT_PRESS		(1 << 6)
#define MXT_DETECT		(1 << 7)

/* Touch orient bits */
#define MXT_XY_SWITCH		(1 << 0)
#define MXT_X_INVERT		(1 << 1)
#define MXT_Y_INVERT		(1 << 2)

/* Touchscreen absolute values */
#define MXT_MAX_AREA		0xff

#define MXT_MAX_FINGER		10

/* tuning parameters in driver */
/* the threshold to report the XY contained in the release message */
#define MXT_RELEASE_REPORT_THRESHOLD_X  35
#define MXT_RELEASE_REPORT_THRESHOLD_Y  35
#define MXT_EDGE_SIZE 40

/* defines related to antitouch handling */
#define MXT_AT_DELAY_MSEC      10000	/* 10 seconds */
#define MXT_AT_ACTION_INIT     0
#define MXT_AT_ACTION_PLUGIN   1
#define MXT_AT_ACTION_PLUGOUT  2
#define MXT_AT_ACTION_UNLOCK   3
#define MXT_AT_ACTION_SUSPEND  4

#define MXT_AVDD_REGULATOR_NAME  MT65XX_POWER_LDO_VGP4
#define MXT_AVDD_VOLTAGE  VOL_1800

static struct mxt_data *global_data;

struct mxt_info {
	u8 family_id;
	u8 variant_id;
	u8 version;
	u8 build;
	u8 matrix_xsize;
	u8 matrix_ysize;
	u8 object_num;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mxt_early_suspend(struct early_suspend *es);
static void mxt_late_resume(struct early_suspend *es);
#endif

struct mxt_raw_object {
	u8 type;
	u8 start_lsb;
	u8 start_msb;
	u8 size;
	u8 instances;
	u8 num_report_ids;
};

struct mxt_object {
	u8 type;
	u16 start_address;
	u16 size;
	u16 instances;
	u8 num_report_ids;

	/* to map object and message */
	u8 min_reportid;
	u8 max_reportid;
};

enum mxt_device_state { INIT, APPMODE, BOOTLOADER, FAILED };

/* Each client has this additional data */
struct mxt_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	const struct mxt_platform_data *pdata;
	enum mxt_device_state state;
	struct mxt_object *object_table;
	u16 mem_size;
	struct mxt_info info;
	unsigned int irq;
	unsigned int max_x;
	unsigned int max_y;
	struct bin_attribute mem_access_attr;
	bool debug_enabled;
	bool driver_paused;
	bool handle_noise;
	bool t48_charger_enabled;
	u8 noise_threshold;
	u8 noise_step;
	u8 bootloader_addr;
	u8 actv_cycle_time;
	u8 idle_cycle_time;
	u8 is_stopped;
	u8 max_reportid;
	u8 num_touchids;
	u8 chargertime;

	u8 atchcalst;
	u8 atchcalsthr;
	u8 atchfrccalthr;
	u8 atchfrccalratio;
	bool bUnlocked;
	struct delayed_work action_work;

	bool finger_states[MXT_MAX_FINGER];
	unsigned int prev_x[MXT_MAX_FINGER];
	unsigned int prev_y[MXT_MAX_FINGER];
	unsigned long config_crc;
	u8 *msg_buf;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif

	/* Cached parameters from object table */
	u8 T9_reportid_min;
	u8 T9_reportid_max;
	u8 T6_reportid;
	u8 T42_reportid;
	u8 T48_reportid;
	u16 T5_address;
	u8 T5_msg_size;
	u16 T7_address;
	u16 T44_address;
};

/* I2C slave address pairs */
struct mxt_i2c_address_pair {
	u8 bootloader;
	u8 application;
};

static const struct mxt_i2c_address_pair mxt_i2c_addresses[] = {
	{0x24, 0x4a},
	{0x25, 0x4b},
	{0x26, 0x4c},
	{0x27, 0x4d},
	{0x34, 0x5a},
	{0x35, 0x5b},
};

static int mxt_fw_read(struct mxt_data *data, u8 *val, unsigned int count)
{
	int ret;
	struct i2c_msg msg;

	msg.addr = data->bootloader_addr;
	msg.flags = data->client->flags & I2C_M_TEN;
	msg.flags |= I2C_M_RD;
	msg.len = count;
	msg.buf = val;

	ret = i2c_transfer(data->client->adapter, &msg, 1);

	return (ret == 1) ? 0 : ret;
}

static int mxt_fw_write(struct mxt_data *data, const u8 * const val, unsigned int count)
{
	int ret;
	struct i2c_msg msg;

	msg.addr = data->bootloader_addr;
	msg.flags = data->client->flags & I2C_M_TEN;
	msg.len = count;
	msg.buf = (u8 *)val;

	ret = i2c_transfer(data->client->adapter, &msg, 1);
	return (ret == 1) ? 0 : ret;
}

static int mxt_get_bootloader_address(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int i;

	for (i = 0; i < ARRAY_SIZE(mxt_i2c_addresses); i++) {
		if (mxt_i2c_addresses[i].application == client->addr) {
			data->bootloader_addr = mxt_i2c_addresses[i].bootloader;

			dev_info(&client->dev, "Bootloader i2c adress: 0x%02x\n",
				 data->bootloader_addr);

			return 0;
		}
	}

	dev_err(&client->dev, "Address 0x%02x not found in address table\n", client->addr);
	return -EINVAL;
}

static int mxt_probe_bootloader(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret;
	u8 val;

	ret = mxt_get_bootloader_address(data);
	if (ret)
		return ret;

	ret = mxt_fw_read(data, &val, 1);
	if (ret) {
		dev_err(dev, "%s: i2c recv failed, ret=%d\n", __func__, ret);
		return ret;
	}

	if (val & MXT_BOOT_STATUS_MASK)
		dev_err(dev, "Application CRC failure\n");
	else
		dev_err(dev, "Device in bootloader mode\n");

	return 0;
}

static int mxt_get_bootloader_version(struct mxt_data *data, u8 val)
{
	struct device *dev = &data->client->dev;
	u8 buf[3];

	if (val | MXT_BOOT_EXTENDED_ID) {
		dev_dbg(dev, "Getting extended mode ID information");

		if (mxt_fw_read(data, &buf[0], 3) != 0) {
			dev_err(dev, "%s: i2c failure\n", __func__);
			return -EIO;
		}

		dev_info(dev, "Bootloader ID:%d Version:%d\n", buf[1], buf[2]);

		return buf[0];
	} else {
		dev_info(dev, "Bootloader ID:%d\n", val & MXT_BOOT_ID_MASK);

		return val;
	}
}

static int mxt_check_bootloader(struct mxt_data *data, unsigned int state)
{
	struct device *dev = &data->client->dev;
	int ret;
	u8 val;

 recheck:
	if (state == MXT_WAITING_BOOTLOAD_CMD) {
		val = mxt_get_bootloader_version(data, val);
	} else {
		ret = mxt_fw_read(data, &val, 1);
		if (ret) {
			dev_err(dev, "%s: i2c recv failed, ret=%d\n", __func__, ret);
			return ret;
		}
	}

	switch (state) {
	case MXT_WAITING_BOOTLOAD_CMD:
		val &= ~MXT_BOOT_STATUS_MASK;
		break;
	case MXT_WAITING_FRAME_DATA:
	case MXT_APP_CRC_FAIL:
		val &= ~MXT_BOOT_STATUS_MASK;
		break;
	case MXT_FRAME_CRC_PASS:
		if (val == MXT_FRAME_CRC_CHECK)
			goto recheck;
		if (val == MXT_FRAME_CRC_FAIL) {
			dev_err(dev, "Bootloader CRC fail\n");
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	if (val != state) {
		dev_err(dev, "Invalid bootloader mode state 0x%X\n", val);
		return -EINVAL;
	}

	return 0;
}

static int mxt_unlock_bootloader(struct mxt_data *data)
{
	int ret;
	u8 buf[2];

	buf[0] = MXT_UNLOCK_CMD_LSB;
	buf[1] = MXT_UNLOCK_CMD_MSB;

	ret = mxt_fw_write(data, buf, 2);
	if (ret) {
		dev_err(&data->client->dev, "%s: i2c send failed, ret=%d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int mxt_read_reg(struct i2c_client *client, u16 reg, u16 len, void *val)
{
	struct i2c_msg xfer[2];
	u8 buf[2];
	int ret;
	int retry = 2;

	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 2;
	xfer[0].buf = buf;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = len;
	xfer[1].buf = val;

	do {
		ret = i2c_transfer(client->adapter, xfer, 2);
		if (ret == 2)
			break;
		dev_err(&client->dev, "%s: i2c retry\n", __func__);
	} while (--retry);

	return 0;
}

static int mxt_write_reg(struct i2c_client *client, u16 reg, u8 val)
{
	int retry = 2;
	int ret;
	u8 buf[3];

	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;
	buf[2] = val;

	do {
		ret = i2c_master_send(client, buf, sizeof(buf));
		if (ret == sizeof(buf))
			break;
		dev_err(&client->dev, "%s: i2c retry\n", __func__);
		udelay(100);
	} while (--retry);

	return 0;
}

int mxt_write_block(struct i2c_client *client, u16 addr, u16 length, u8 *value)
{
	int i;
	struct {
		__le16 le_addr;
		u8 data[MXT_MAX_BLOCK_WRITE];
	} i2c_block_transfer;

	if (length > MXT_MAX_BLOCK_WRITE)
		return -EINVAL;

	memcpy(i2c_block_transfer.data, value, length);

	i2c_block_transfer.le_addr = cpu_to_le16(addr);

	i = i2c_master_send(client, (u8 *) &i2c_block_transfer, length + 2);

	return i == (length + 2) ? 0 : -EIO;
}

static void mxt_output_object_table(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int i;

	for (i = 0; i < data->info.object_num; i++) {
		struct mxt_object *object = data->object_table + i;

		dev_info(dev, "T%u, start:%u size:%u instances:%u "
			 "min_reportid:%u max_reportid:%u\n",
			 object->type, object->start_address, object->size,
			 object->instances, object->min_reportid, object->max_reportid);
	}
}

static struct mxt_object *mxt_get_object(struct mxt_data *data, u8 type)
{
	struct mxt_object *object;
	int i;

	for (i = 0; i < data->info.object_num; i++) {
		object = data->object_table + i;
		if (object->type == type)
			return object;
	}

	dev_err(&data->client->dev, "Invalid object type T%u\n", type);
	mxt_output_object_table(data);

	return NULL;
}

static int mxt_read_object(struct mxt_data *data, u8 type, u8 offset, u8 *val)
{
	struct mxt_object *object;
	u16 reg;

	object = mxt_get_object(data, type);
	if (!object)
		return -EINVAL;

	reg = object->start_address;
	return mxt_read_reg(data->client, reg + offset, 1, val);
}

static int mxt_write_object(struct mxt_data *data, u8 type, u8 offset, u8 val)
{
	struct mxt_object *object;
	u16 reg;

	object = mxt_get_object(data, type);
	if (!object)
		return -EINVAL;

	if (offset >= object->size * object->instances) {
		dev_err(&data->client->dev, "Tried to write outside object T%d"
			" offset:%d, size:%d\n", type, offset, object->size);
		return -EINVAL;
	}

	reg = object->start_address;
	return mxt_write_reg(data->client, reg + offset, val);
}

static int mxt_set_t48_charger_bit(struct mxt_data *data, u8 val)
{
	int ret;
	u8 regval;
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;

	mutex_lock(&input_dev->mutex);

	ret = mxt_read_object(data, MXT_PROCG_NOISESUPPRESSION_T48, MXT_T48_CALCFG, &regval);
	if (ret)
		goto release;

	ret = mxt_write_object(data, MXT_PROCG_NOISESUPPRESSION_T48, MXT_T48_CALCFG,
			       val ? (regval | MXT_T48_CALCFG_CHRGON) : (regval &
									 ~MXT_T48_CALCFG_CHRGON));
	if (ret < 0)
		goto release;

	dev_info(dev, "atmel set charger bit %d, regval %d\n", val, regval);

	data->t48_charger_enabled = val;

 release:
	mutex_unlock(&input_dev->mutex);

	return ret;
}

static int mxt_proc_t48_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status, state;

	status = msg[1];
	state = msg[4];

/* dev_info(dev, "T48: %x %x %x %x %x %x %x %x\n", msg[0], msg[1], msg[2], msg[3], msg[4], msg[5], msg[6], msg[7]); */

	if ((state == MXT_T48_MSG_STATE_ERR) || (status & MXT_T48_MSG_STATUS_ALGOERR)) {
		dev_err(dev, "atmel got noise err when chgr bit turned on\n");
	}

	return 0;
}

static int mxt_read_message_count(struct mxt_data *data, u8 *count)
{
	/* TODO: Optimization: read first message along with message count */
	return mxt_read_reg(data->client, data->T44_address, 1, count);
}

static void mxt_input_sync(struct mxt_data *data)
{
	input_mt_report_pointer_emulation(data->input_dev, false);
	input_sync(data->input_dev);
}

static void mxt_reset_fingers(struct mxt_data *data)
{
	struct input_dev *input_dev = data->input_dev;
	int id;
	bool *finger_state;

	for (id = 0; id < data->num_touchids; id++) {
		finger_state = data->finger_states + id;

		if (*finger_state == 1) {
			input_mt_slot(input_dev, id);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
			*finger_state = 0;
		}
	}

	mxt_input_sync(data);
}

static void mxt_input_touch(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;
	u8 status;
	int x;
	int y;
	int area;
	int amplitude;
	int id;
	bool *finger_state;

	id = message[0] - data->T9_reportid_min;

	status = message[1];

	x = (message[2] << 4) | ((message[4] >> 4) & 0xf);
	y = (message[3] << 4) | ((message[4] & 0xf));
	if (data->max_x < 1024)
		x >>= 2;
	if (data->max_y < 1024)
		y >>= 2;
	area = message[5];
	amplitude = message[6];

	dev_dbg(dev,
		"[%d] %c%c%c%c%c%c%c%c x: %d y: %d area: %d amp: %d\n",
		id,
		(status & MXT_DETECT) ? 'D' : '.',
		(status & MXT_PRESS) ? 'P' : '.',
		(status & MXT_RELEASE) ? 'R' : '.',
		(status & MXT_MOVE) ? 'M' : '.',
		(status & MXT_VECTOR) ? 'V' : '.',
		(status & MXT_AMP) ? 'A' : '.',
		(status & MXT_SUPPRESS) ? 'S' : '.',
		(status & MXT_UNGRIP) ? 'U' : '.', x, y, area, amplitude);

	if ((status & MXT_RELEASE)) {
		/* use the previous XY if the release XY is close enough to previous XY, when the XY are not in edge area */
		if ((abs(data->prev_x[id] - x) < MXT_RELEASE_REPORT_THRESHOLD_X &&
		     abs(data->prev_y[id] - y) < MXT_RELEASE_REPORT_THRESHOLD_Y) &&
		    !((x < MXT_EDGE_SIZE) || (x > (data->max_x - MXT_EDGE_SIZE)) ||
		      (y < MXT_EDGE_SIZE) || (y > (data->max_y - MXT_EDGE_SIZE)))) {
			dev_dbg(dev, "atmel discard last xy. prev=%x, %x, cur=%x, %x\n",
				data->prev_x[id], data->prev_y[id], x, y);
			x = data->prev_x[id];
			y = data->prev_y[id];
		}
	}

	if (data->prev_x[id] != x || data->prev_y[id] != y) {
		data->prev_x[id] = x;
		data->prev_y[id] = y;
	}

	if (id < 0 || id > data->num_touchids) {
		dev_err(dev, "invalid touch id %d, total num touch is %d\n", id,
			data->num_touchids);
		return;
	}

	finger_state = &(data->finger_states[id]);
	input_mt_slot(input_dev, id);

	if ((status & MXT_DETECT) && (status & MXT_RELEASE)) {
		/* Touch in detect, just after being released, so
		 * get new touch tracking ID */
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
		*finger_state = 1;
		mxt_input_sync(data);
	}

	if ((status & MXT_DETECT) || ((*finger_state == 1) && (status & MXT_RELEASE))) {
		/* Touch in detect, or final release report, so
		 * report X/Y position */
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 1);
		*finger_state = 1;

		input_report_abs(input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(input_dev, ABS_MT_PRESSURE, amplitude);
		input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, area);
	}

	if (!(status & MXT_DETECT)) {
		/* Touch no longer in detect, so close out slot */
		mxt_input_sync(data);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
		*finger_state = 0;
	}
}

static void mxt_proc_t6_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	unsigned long crc;
	u8 status = msg[1];

	crc = msg[2] | (msg[3] << 8) | (msg[4] << 16);

	if (crc != data->config_crc) {
		data->config_crc = crc;
		dev_dbg(dev, "T6 config checksum: %06X\n", (unsigned int)crc);
	};

	dev_info(dev, "status = %x\n", status);

	/* too much output, so just use one line above
	   if (status | MXT_T6_MSG_RESET) {
	   dev_info(dev, "Resetting\n");
	   }

	   if (status | MXT_T6_MSG_CAL) {
	   dev_info(dev, "Calibrating\n");
	   }

	   if (status | MXT_T6_MSG_CFGERR) {
	   dev_err(dev, "Configuration error!\n");
	   }
	 */
}

static void mxt_proc_t42_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status = msg[1];

	if (status | MXT_T42_MSG_TCHSUP) {
		dev_info(dev, "T42 suppress\n");
	} else {
		dev_info(dev, "T42 normal\n");
	}
}

static int mxt_proc_messages(struct mxt_data *data, u8 *msg)
{
	u8 report_id;

	if (data->debug_enabled)
		print_hex_dump(KERN_DEBUG, "MXT MSG:", DUMP_PREFIX_NONE, 16, 1,
			       msg, data->T5_msg_size, false);

	report_id = msg[0];

	if (data->state == APPMODE
	    && report_id >= data->T9_reportid_min && report_id <= data->T9_reportid_max) {
		mxt_input_touch(data, msg);
	} else if (report_id == data->T6_reportid) {
		mxt_proc_t6_messages(data, msg);
	} else if (report_id == data->T48_reportid) {
		mxt_proc_t48_messages(data, msg);
	} else if (report_id == data->T42_reportid) {
		mxt_proc_t42_messages(data, msg);
	}

	return 0;
}

static void mxt_read_and_process_messages(struct mxt_data *data, u8 count)
{
	struct device *dev = &data->client->dev;
	int ret, i;

	ret = mxt_read_reg(data->client, data->T5_address,
			   data->T5_msg_size * count, data->msg_buf);
	if (ret) {
		dev_err(dev, "Failed to read %u messages (%d).\n", count, ret);
		return;
	}

	for (i = 0; i < count; i++) {
		ret = mxt_proc_messages(data, data->msg_buf + i * data->T5_msg_size);
	}

	mxt_input_sync(data);
}

static void mxt_handle_messages(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret;
	u8 count;

	ret = mxt_read_message_count(data, &count);
	if (ret) {
		dev_err(dev, "Failed to read message count (%d).\n", ret);
		return;
	}

	if (count > data->max_reportid) {
		dev_err(dev, "T44 count exceeded max report id\n");
		count = data->max_reportid;
	}

	if (count > 0) {
		mxt_read_and_process_messages(data, count);
	} else {
		/* dev_err(dev, "No messages\n"); */
	}
}

static int mxt_make_highchg(struct mxt_data *data)
{
	int ret;
	struct device *dev = &data->client->dev;
	int count;
	u8 reportid = MXT_RPTID_NOMSG;
	u8 *msg = data->msg_buf;

	/* If all objects report themselves then a number of messages equal to
	 * the number of report ids may be generated. Therefore a good safety
	 * heuristic is twice this number */
	count = data->max_reportid * 2;

	/* Read messages until invalid to make CHG pin high */
	do {
		ret = mxt_read_reg(data->client, data->T5_address, data->T5_msg_size, msg);
		if (ret) {
			dev_err(dev, "%s :Error in mxt read reg\n", __func__);
			return -EIO;
		}
		reportid = msg[0];

		/* mxt_proc_messages(data, msg); */
	} while (reportid != MXT_RPTID_NOMSG && --count);

	if (!count) {
		dev_err(dev, "CHG pin isn't cleared\n");
		return -EBUSY;
	}

	return 0;
}

static irqreturn_t mxt_interrupt(int irq, void *d)
{
	struct mxt_data *data = d;
	mxt_handle_messages(data);
	return IRQ_HANDLED;
}

static void mxt_read_current_crc(struct mxt_data *data)
{
	int try = 0;

	data->config_crc = 0;

	/* Try to read the config checksum of the existing cfg */
	while (++try < 4) {
		mxt_write_object(data, MXT_GEN_COMMAND_T6, MXT_COMMAND_REPORTALL, 1);

		msleep(30);

		mxt_handle_messages(data);

		if (data->config_crc > 0)
			return;
	}
}

static u32 mxt_calculate_crc32(u32 crc, u8 firstbyte, u8 secondbyte)
{
	static const unsigned int crcpoly = 0x80001B;
	u32 result;
	u16 data_word;

	data_word = (u16) ((u16) (secondbyte << 8) | firstbyte);
	result = ((crc << 1) ^ (u32) data_word);
	if (result & 0x1000000) {	/* if bit 25 is set */
		result ^= crcpoly;	/* XOR with crcpoly */
	}
	return result;
}

static u32 mxt_calculate_config_crc(u8 *base, off_t start_off, off_t end_off)
{
	u8 *i;
	u32 crc = 0;
	u8 *last_val = base + end_off - 1;

	if (end_off < start_off)
		return -EINVAL;

	for (i = base + start_off; i < last_val; i += 2) {
		crc = mxt_calculate_crc32(crc, *i, *(i + 1));
	}

	if (i == last_val)	/* if len is odd, fill the last byte with 0 */
		crc = mxt_calculate_crc32(crc, *i, 0);

	/* Mask only 24-bit */
	return crc & 0x00FFFFFF;
}

int mxt_download_config(struct mxt_data *data, const char *fn)
{
	struct device *dev = &data->client->dev;
	struct mxt_info cfg_info;
	struct mxt_object *object;
	const struct firmware *cfg = NULL;
	int ret;
	int offset;
	int pos;
	int i;
	unsigned long info_crc, config_crc;
	unsigned int type, instance, size;
	int config_start_offset;
	u8 *config_mem;		/* stores config items in memory, for crc cal */
	size_t config_mem_size;
	u32 crc_calculated;
	u8 val;
	u16 reg;

	ret = -1;
	if (ret < 0) {
		dev_err(dev, "Failure to request config file %s\n", fn);
		return 0;
	}

	mxt_read_current_crc(data);

	if (strncmp(cfg->data, MXT_CFG_MAGIC, strlen(MXT_CFG_MAGIC))) {
		dev_err(dev, "Unrecognised config file\n");
		ret = -EINVAL;
		goto release;
	}

	pos = strlen(MXT_CFG_MAGIC);

	/* Load information block and check */
	for (i = 0; i < sizeof(struct mxt_info); i++) {
		ret = sscanf(cfg->data + pos, "%hhx%n", (unsigned char *)&cfg_info + i, &offset);
		if (ret != 1) {
			dev_err(dev, "Bad format\n");
			ret = -EINVAL;
			goto release;
		}

		pos += offset;
	}

	if (cfg_info.family_id != data->info.family_id) {
		dev_err(dev, "Family ID mismatch!\n");
		ret = -EINVAL;
		goto release;
	}

	if (cfg_info.variant_id != data->info.variant_id) {
		dev_err(dev, "Variant ID mismatch!\n");
		ret = -EINVAL;
		goto release;
	}

	if (cfg_info.version != data->info.version)
		dev_err(dev, "Warning: version mismatch!\n");

	if (cfg_info.build != data->info.build)
		dev_err(dev, "Warning: build num mismatch!\n");

	ret = sscanf(cfg->data + pos, "%lx%n", &info_crc, &offset);
	if (ret != 1) {
		dev_err(dev, "Bad format\n");
		ret = -EINVAL;
		goto release;
	}
	pos += offset;

	/* Check config CRC */
	ret = sscanf(cfg->data + pos, "%lx%n", &config_crc, &offset);
	if (ret != 1) {
		dev_err(dev, "Bad format\n");
		ret = -EINVAL;
		goto release;
	}
	pos += offset;

	if (data->config_crc == config_crc) {
		dev_info(dev, "Config CRC 0x%X: OK\n", (unsigned int)data->config_crc);
		ret = 0;
		goto release;
	} else {
		dev_info(dev, "Config CRC 0x%X: does not match 0x%X\n",
			 (unsigned int)data->config_crc, (unsigned int)config_crc);
	}

	/* malloc memory to store configs, for crc cal */
	config_start_offset = MXT_OBJECT_START
	    + data->info.object_num * sizeof(struct mxt_raw_object);
	config_mem_size = data->mem_size - config_start_offset;
	config_mem = kzalloc(config_mem_size, GFP_KERNEL);

	while (pos < cfg->size) {
		/* Read type, instance, length */
		ret = sscanf(cfg->data + pos, "%x %x %x%n", &type, &instance, &size, &offset);
		if (ret == 0) {
			/* EOF */
			break;
		} else if (ret < 0) {
			dev_err(dev, "Bad format\n");
			ret = -EINVAL;
			goto release_mem;
		}
		pos += offset;

		object = mxt_get_object(data, type);
		if (!object) {
			dev_err(dev, "mxt_get_object wrong, type=%d\n", type);
			ret = -EINVAL;
			goto release_mem;
		}

		/* This error may be encountered on using a config from a later
		 * firmware version */
		if (size > object->size) {
			dev_err(dev, "Warning: Object length exceeded\n");
		}

		if (instance >= object->instances) {
			dev_err(dev, "Object instances exceeded!\n");
			ret = -EINVAL;
			goto release_mem;
		}

		reg = object->start_address + object->size * instance;

		for (i = 0; i < size; i++) {
			ret = sscanf(cfg->data + pos, "%hhx%n", &val, &offset);
			if (ret != 1) {
				dev_err(dev, "Bad format\n");
				ret = -EINVAL;
				goto release_mem;
			}

			if (i < object->size) {
				if ((reg + i >= config_start_offset) &&
				    (reg + i - config_start_offset <= config_mem_size))
					*(config_mem + reg + i - config_start_offset) = val;
				else {
					dev_err(dev, "Bad object: reg:%d, T%d\n", reg,
						object->type);
					ret = -EINVAL;
					goto release_mem;
				}
			}

			pos += offset;
		}

		/* If firmware is upgraded, new bytes may be added to end of
		 * objects. It is generally forward compatible to zero these
		 * bytes - previous behaviour will be retained. However
		 * this does invalidate the CRC and will force a config
		 * download every time until the configuration is updated */
		if (size < object->size) {
			dev_info(dev, "Warning: zeroing %d byte(s) in T%d\n",
				 object->size - size, type);

			/* no need to zero them here since the config mem
			 * is zeroized when we get it */
		}
	}

	/* calculate crc of the received configs (not the raw config file) */
	if (data->T7_address < config_start_offset) {
		dev_err(dev, "Bad T7 address, T7addr = %x, config offset %x\n",
			data->T7_address, config_start_offset);
		ret = 0;
		goto release_mem;
	}

	crc_calculated = mxt_calculate_config_crc(config_mem,
						  data->T7_address - config_start_offset,
						  config_mem_size);

	/* check the crc, calculated should same as what's in file */
	if (crc_calculated != config_crc) {
		dev_err(dev, "Config CRC mismatch, calculated crc = %x, config file has %lx\n",
			crc_calculated, config_crc);
		ret = 0;
		goto release_mem;
	}

	/* write the whole area as one shot, as long as the mem size is less than 64k it's fine */
	ret = mxt_write_block(data->client, config_start_offset, config_mem_size, config_mem);
	if (ret != 0)
		dev_err(dev, "Config write error, ret=%d\n", ret);
	else
		ret = 1;	/* tell the caller config has been sent */

 release_mem:
	kfree(config_mem);
 release:
	release_firmware(cfg);
	return ret;
}

static int mxt_soft_reset(struct mxt_data *data, u8 value)
{
	int timeout_counter = 0;
	struct device *dev = &data->client->dev;

	dev_info(dev, "Resetting chip\n");

	mxt_write_object(data, MXT_GEN_COMMAND_T6, MXT_COMMAND_RESET, value);

	if (data->pdata->read_chg == NULL) {
		msleep(MXT_RESET_NOCHGREAD);
	} else {
		switch (data->info.family_id) {
		case MXT224_ID:
			msleep(MXT224_RESET_TIME);
			break;
		case MXT768E_ID:
			msleep(MXT768E_RESET_TIME);
			break;
		case MXT1386_ID:
			msleep(MXT1386_RESET_TIME);
			break;
		default:
			msleep(MXT_RESET_TIME);
		}
		timeout_counter = 0;
		while ((timeout_counter++ <= 100) && data->pdata->read_chg())
			msleep(20);
		if (timeout_counter > 100) {
			dev_err(dev, "No response after reset!\n");
			return -EIO;
		}
	}

	return 0;
}

static int mxt_set_power_cfg(struct mxt_data *data, u8 mode)
{
	struct device *dev = &data->client->dev;
	int error;
	u8 actv_cycle_time;
	u8 idle_cycle_time;

	switch (mode) {
	case MXT_POWER_CFG_DEEPSLEEP:
		actv_cycle_time = 0;
		idle_cycle_time = 0;
		break;
	case MXT_POWER_CFG_RUN:
	default:
		actv_cycle_time = data->actv_cycle_time;
		idle_cycle_time = data->idle_cycle_time;
		break;
	}

	error = mxt_write_object(data, MXT_GEN_POWER_T7, MXT_POWER_ACTVACQINT, actv_cycle_time);
	if (error)
		goto i2c_error;

	error = mxt_write_object(data, MXT_GEN_POWER_T7, MXT_POWER_IDLEACQINT, idle_cycle_time);
	if (error)
		goto i2c_error;

	dev_dbg(dev, "Set ACTV %d, IDLE %d", actv_cycle_time, idle_cycle_time);

	return 0;

 i2c_error:
	dev_err(dev, "Failed to set power cfg");
	return error;
}

static int mxt_read_power_cfg(struct mxt_data *data, u8 *actv_cycle_time, u8 *idle_cycle_time)
{
	int error;

	error = mxt_read_object(data, MXT_GEN_POWER_T7, MXT_POWER_ACTVACQINT, actv_cycle_time);
	if (error)
		return error;

	error = mxt_read_object(data, MXT_GEN_POWER_T7, MXT_POWER_IDLEACQINT, idle_cycle_time);
	if (error)
		return error;

	return 0;
}

static int mxt_check_power_cfg_post_reset(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int error;

	error = mxt_read_power_cfg(data, &data->actv_cycle_time, &data->idle_cycle_time);
	if (error)
		return error;

	/* Power config is zero, select free run */
	if (data->actv_cycle_time == 0 || data->idle_cycle_time == 0) {
		dev_dbg(dev, "Overriding power cfg to free run\n");
		data->actv_cycle_time = 255;
		data->idle_cycle_time = 255;

		error = mxt_set_power_cfg(data, MXT_POWER_CFG_RUN);
		if (error)
			return error;
	}

	return 0;
}

static int mxt_probe_power_cfg(struct mxt_data *data)
{
	int error;

	error = mxt_read_power_cfg(data, &data->actv_cycle_time, &data->idle_cycle_time);
	if (error)
		return error;

	/* If in deep sleep mode, attempt reset */
	if (data->actv_cycle_time == 0 || data->idle_cycle_time == 0) {
		error = mxt_soft_reset(data, MXT_RESET_VALUE);
		if (error)
			return error;

		error = mxt_check_power_cfg_post_reset(data);
		if (error)
			return error;
	}

	return 0;
}

static int mxt_check_reg_init(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int timeout_counter = 0;
	int ret;
	u8 command_register;

	ret = mxt_download_config(data, MXT_CFG_NAME);
	if (ret > 0) {
		/* CRC does not match but the config file is loaded correctly */

		/* Backup to memory */
		mxt_write_object(data, MXT_GEN_COMMAND_T6, MXT_COMMAND_BACKUPNV, MXT_BACKUP_VALUE);
		msleep(MXT_BACKUP_TIME);
		do {
			ret = mxt_read_object(data, MXT_GEN_COMMAND_T6,
					      MXT_COMMAND_BACKUPNV, &command_register);
			if (ret)
				return ret;
			msleep(20);
		} while ((command_register != 0) && (timeout_counter++ <= 100));
		if (timeout_counter > 100) {
			dev_err(dev, "No response after backup!\n");
			return -EIO;
		}
	}

	/* for all cases do software reset,
	   even if error encountered during config file load, this will clear it up */
	ret = mxt_soft_reset(data, MXT_RESET_VALUE);
	if (ret)
		return ret;

	ret = mxt_check_power_cfg_post_reset(data);
	if (ret)
		return ret;

	return 0;
}

static int mxt_get_object_table(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	struct device *dev = &data->client->dev;
	int ret;
	int i;
	u16 end_address;
	struct mxt_raw_object *object_buf;
	struct mxt_raw_object *raw_object;
	u8 reportid = 0;
	data->mem_size = 0;

	object_buf = kcalloc(data->info.object_num, sizeof(struct mxt_raw_object), GFP_KERNEL);
	if (!object_buf) {
		dev_err(dev, "Failed to allocate space to read object table\n");
		return -ENOMEM;
	}

	data->object_table = kcalloc(data->info.object_num, sizeof(struct mxt_object), GFP_KERNEL);
	if (!data->object_table) {
		dev_err(dev, "Failed to allocate object table\n");
		ret = -ENOMEM;
		goto free_object_buf;
	}

	ret = mxt_read_reg(client, MXT_OBJECT_START,
			   data->info.object_num * sizeof(struct mxt_raw_object), object_buf);
	if (ret)
		goto free_object_table;

	for (i = 0; i < data->info.object_num; i++) {
		struct mxt_object *object = data->object_table + i;
		raw_object = object_buf + i;

		object->type = raw_object->type;
		object->start_address = (raw_object->start_msb << 8)
		    | raw_object->start_lsb;
		object->size = raw_object->size + 1;
		object->instances = raw_object->instances + 1;
		object->num_report_ids = raw_object->num_report_ids;

		if (object->num_report_ids) {
			reportid += object->num_report_ids * object->instances;
			object->max_reportid = reportid;
			object->min_reportid = object->max_reportid -
			    object->instances * object->num_report_ids + 1;
		}

		end_address = object->start_address + object->size * object->instances - 1;

		if (end_address >= data->mem_size)
			data->mem_size = end_address + 1;

		/* save data for objects used when processing interrupts */
		switch (object->type) {
		case MXT_TOUCH_MULTI_T9:
			data->T9_reportid_max = object->max_reportid;
			data->T9_reportid_min = object->min_reportid;
			data->num_touchids = object->num_report_ids * object->instances;
			if (data->num_touchids > MXT_MAX_FINGER) {
				dev_err(dev, "configured max finger (%d) is too large.\n",
					data->num_touchids);
				data->num_touchids = MXT_MAX_FINGER;
			}
			break;
		case MXT_GEN_POWER_T7:
			data->T7_address = object->start_address;
			break;
		case MXT_GEN_COMMAND_T6:
			data->T6_reportid = object->max_reportid;
			break;
		case MXT_GEN_MESSAGE_T5:
			/* CRC not enabled, therefore don't read last byte */
			data->T5_msg_size = object->size - 1;
			data->T5_address = object->start_address;
			break;
		case MXT_SPT_MESSAGECOUNT_T44:
			data->T44_address = object->start_address;
			break;
		case MXT_PROCI_TOUCHSUPPRESSION_T42:
			data->T42_reportid = object->max_reportid;
			break;
		case MXT_SPT_NOISESUPPRESSION_T48:
			data->T48_reportid = object->max_reportid;
			break;
		}
	}

	/* Store maximum reportid */
	data->max_reportid = reportid;

	data->msg_buf = kcalloc(data->max_reportid, data->T5_msg_size, GFP_KERNEL);
	if (!data->msg_buf) {
		dev_err(dev, "Failed to allocate message buffer\n");
		ret = -ENOMEM;
		goto free_object_table;
	}

	mxt_output_object_table(data);

	kfree(object_buf);

	return 0;

 free_object_table:
	kfree(data->object_table);
 free_object_buf:
	kfree(object_buf);
	return ret;
}

static int mxt_read_resolution(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	unsigned int x_range, y_range;
	unsigned char orient;
	unsigned char val;

	/* Update matrix size in info struct */
	error = mxt_read_reg(client, MXT_MATRIX_X_SIZE, 1, &val);
	if (error)
		return error;
	data->info.matrix_xsize = val;

	error = mxt_read_reg(client, MXT_MATRIX_Y_SIZE, 1, &val);
	if (error)
		return error;
	data->info.matrix_ysize = val;

	/* Read X/Y size of touchscreen */
	error = mxt_read_object(data, MXT_TOUCH_MULTI_T9, MXT_TOUCH_XRANGE_MSB, &val);
	if (error)
		return error;
	x_range = val << 8;

	error = mxt_read_object(data, MXT_TOUCH_MULTI_T9, MXT_TOUCH_XRANGE_LSB, &val);
	if (error)
		return error;
	x_range |= val;

	error = mxt_read_object(data, MXT_TOUCH_MULTI_T9, MXT_TOUCH_YRANGE_MSB, &val);
	if (error)
		return error;
	y_range = val << 8;

	error = mxt_read_object(data, MXT_TOUCH_MULTI_T9, MXT_TOUCH_YRANGE_LSB, &val);
	if (error)
		return error;
	y_range |= val;

	error = mxt_read_object(data, MXT_TOUCH_MULTI_T9, MXT_TOUCH_ORIENT, &orient);
	if (error)
		return error;

	/* Handle default values */
	if (x_range == 0)
		x_range = 1023;

	if (y_range == 0)
		y_range = 1023;

	if (orient & MXT_XY_SWITCH) {
		data->max_x = y_range;
		data->max_y = x_range;
	} else {
		data->max_x = x_range;
		data->max_y = y_range;
	}

	dev_info(&client->dev,
		 "Matrix Size X%uY%u Touchscreen size X%uY%u\n",
		 data->info.matrix_xsize, data->info.matrix_ysize, data->max_x, data->max_y);

	return 0;
}

static int mxt_enable_noise_report(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	unsigned char val;

	error = mxt_read_object(data, MXT_PROCG_NOISESUPPRESSION_T48, MXT_T48_NLTHR, &val);
	if (error)
		return error;

	if (val < data->noise_step) {
		dev_info(&client->dev, "Noise threshold read is %d, less than noise step %d\n",
			 data->noise_threshold, data->noise_step);
		data->noise_step = val / 2;
		dev_info(&client->dev, "Set noise step to %d\n", data->noise_step);
	}

	data->noise_threshold = val;

	dev_info(&client->dev, "Noise threshold is %d\n", data->noise_threshold);

	error = mxt_read_object(data, MXT_PROCG_NOISESUPPRESSION_T48, MXT_T48_CTRL, &val);
	if (error)
		return error;

	error = mxt_write_object(data, MXT_PROCG_NOISESUPPRESSION_T48, MXT_T48_CTRL,
				 (val | MXT_T48_CTRL_ENABLE | MXT_T48_CTRL_RPTEN));
	if (error < 0) {
		dev_err(&client->dev, "t48 write object error\n");
		return error;
	}

	dev_info(&client->dev, "Noise level report enabled\n");

	return 0;
}

static int mxt_initialize(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	struct mxt_info *info = &data->info;
	int error;
	u8 val;
	u8 count = 0;
	u8 bootloader_cmd[2];

	memset(data->finger_states, 0, sizeof(data->finger_states));
	memset(data->prev_x, 0, sizeof(data->prev_x));
	memset(data->prev_y, 0, sizeof(data->prev_y));

 retry_probe:
	error = mxt_read_reg(client, 0, sizeof(*info), info);
	if (error) {
		error = mxt_probe_bootloader(data);

		if (error) {
			/* chip is not there in bootloader mode either. we
			   have already had i2c retry */
			dev_err(&client->dev, "Error %d at mxt_probe_bootloader\n", error);
			return error;
		} else {
			count++;
			/* this may be any two bytes that are not bootloader
			   unlock cmd, should return chip to app mode */
			bootloader_cmd[0] = 0x01;
			bootloader_cmd[1] = 0x01;
			error = mxt_fw_write(data, bootloader_cmd, 2);
			if (error) {
				dev_err(&client->dev, "Error %d at mxt_fw_write, count=%d\n", error,
					count);
				return -EIO;
			}

			/* give up */
			if (count > 10) {
				data->state = BOOTLOADER;
				dev_err(&client->dev,
					"reached 10 counts for mxt_probe_bootloader, count=%d\n",
					count);
				return 0;
			}

			/* wait for device to reset */
			msleep(MXT_FWRESET_TIME);

			/* retry probe in app mode */
			goto retry_probe;
		}
	}

	dev_info(&client->dev,
		 "Family ID: %d Variant ID: %d Version: %d.%d.%02X "
		 "Object Num: %d\n",
		 info->family_id, info->variant_id,
		 info->version >> 4, info->version & 0xf, info->build, info->object_num);

	/* Get object table information */
	error = mxt_get_object_table(data);
	if (error) {
		dev_err(&client->dev, "Error %d reading object table\n", error);
		return error;
	}

	error = mxt_probe_power_cfg(data);
	if (error) {
		dev_err(&client->dev, "Failed to initialize power cfg\n");
		return error;
	}

	/* Check register init values */
	error = mxt_check_reg_init(data);
	if (error) {
		dev_err(&client->dev, "Failed to initialize config\n");
		/* Do not return error, to allow it to work with older fw */
	}

	error = mxt_read_resolution(data);
	if (error) {
		dev_err(&client->dev, "Failed to initialize screen size\n");
		return error;
	}

	error = mxt_enable_noise_report(data);
	if (error) {
		dev_err(&client->dev, "Failed to enable noise reporting\n");
		return error;
	}

	error = mxt_read_object(data, MXT_GEN_ACQUIRE_T8, MXT_ACQUIRE_CHRGTIME, &val);
	if (error)
		return error;
	data->chargertime = val;

	/* read the anti touch values */
	error = mxt_read_object(data, MXT_GEN_ACQUIRE_T8, MXT_ACQUIRE_ATCHCALST, &val);
	if (error)
		return error;
	data->atchcalst = val;

	error = mxt_read_object(data, MXT_GEN_ACQUIRE_T8, MXT_ACQUIRE_ATCHCALSTHR, &val);
	if (error)
		return error;
	data->atchcalsthr = val;

	error = mxt_read_object(data, MXT_GEN_ACQUIRE_T8, MXT_ACQUIRE_ATCHFRCCALTHR, &val);
	if (error)
		return error;
	data->atchfrccalthr = val;

	error = mxt_read_object(data, MXT_GEN_ACQUIRE_T8, MXT_ACQUIRE_ATCHFRCCALRATIO, &val);
	if (error)
		return error;
	data->atchfrccalratio = val;

	return 0;
}

static int mxt_set_antitouch(struct mxt_data *data, bool unlocked)
{
	struct device *dev = &data->client->dev;
	u8 atchcalst, atchcalsthr, atchfrccalthr, atchfrccalratio;
	int error;

	if (unlocked) {
		atchcalst = 20;
		atchcalsthr = 1;
		atchfrccalthr = 0;
		atchfrccalratio = 0;
	} else {
		atchcalst = data->atchcalst;
		atchcalsthr = data->atchcalsthr;
		atchfrccalthr = data->atchfrccalthr;
		atchfrccalratio = data->atchfrccalratio;
	}

	dev_info(dev, "set antitouch %d %d %d %d\n", atchcalst, atchcalsthr, atchfrccalthr,
		 atchfrccalratio);

	error = mxt_write_object(data, MXT_GEN_ACQUIRE_T8, MXT_ACQUIRE_ATCHCALST, atchcalst);
	if (error)
		return error;

	error = mxt_write_object(data, MXT_GEN_ACQUIRE_T8, MXT_ACQUIRE_ATCHCALSTHR, atchcalsthr);
	if (error)
		return error;

	error =
	    mxt_write_object(data, MXT_GEN_ACQUIRE_T8, MXT_ACQUIRE_ATCHFRCCALTHR, atchfrccalthr);
	if (error)
		return error;

	error =
	    mxt_write_object(data, MXT_GEN_ACQUIRE_T8, MXT_ACQUIRE_ATCHFRCCALRATIO,
			     atchfrccalratio);
	if (error)
		return error;

	return 0;
}

static void mxt_handle_antitouch(struct mxt_data *data, int state)
{
	struct input_dev *input_dev = data->input_dev;

	mutex_lock(&input_dev->mutex);

	switch (state) {
	case MXT_AT_ACTION_PLUGIN:
	case MXT_AT_ACTION_PLUGOUT:
		if (data->bUnlocked) {
			mxt_set_antitouch(data, 0);	/* this can be duplicate if within 10 sec of unlock */
			cancel_delayed_work_sync(&data->action_work);
			schedule_delayed_work(&data->action_work,
					      msecs_to_jiffies(MXT_AT_DELAY_MSEC));
		}
		break;

	case MXT_AT_ACTION_UNLOCK:
		/* no need to cancel here since there won't be work scheduled before unlock */
		data->bUnlocked = true;
		schedule_delayed_work(&data->action_work, msecs_to_jiffies(MXT_AT_DELAY_MSEC));
		break;

	case MXT_AT_ACTION_SUSPEND:
		mxt_set_antitouch(data, 0);
		cancel_delayed_work_sync(&data->action_work);
		data->bUnlocked = false;
		break;
	}

	mutex_unlock(&input_dev->mutex);
}


static void mxt_action_work_handler(struct work_struct *work)
{
	struct mxt_data *data = container_of(work, struct mxt_data, action_work.work);

	mxt_set_antitouch(data, 1);
}

static int mxt_load_fw(struct device *dev, const char *fn)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	const struct firmware *fw = NULL;
	unsigned int frame_size;
	unsigned int pos = 0;
	unsigned int retry = 0;
	unsigned int frame = 0;
	int ret;

	ret = request_firmware(&fw, fn, dev);
	if (ret < 0) {
		dev_err(dev, "Unable to open firmware %s\n", fn);
		return ret;
	}

	if (data->state != BOOTLOADER) {
		/* Change to the bootloader mode */
		ret = mxt_soft_reset(data, MXT_BOOT_VALUE);
		if (ret)
			goto release_firmware;

		ret = mxt_get_bootloader_address(data);
		if (ret)
			goto release_firmware;

		data->state = BOOTLOADER;
	}

	ret = mxt_check_bootloader(data, MXT_WAITING_BOOTLOAD_CMD);
	if (ret) {
		/* Bootloader may still be unlocked from previous update
		 * attempt */
		ret = mxt_check_bootloader(data, MXT_WAITING_FRAME_DATA);

		if (ret) {
			data->state = FAILED;
			goto release_firmware;
		}
	} else {
		dev_info(dev, "Unlocking bootloader\n");

		/* Unlock bootloader */
		ret = mxt_unlock_bootloader(data);
		if (ret) {
			data->state = FAILED;
			goto release_firmware;
		}
	}

	while (pos < fw->size) {
		ret = mxt_check_bootloader(data, MXT_WAITING_FRAME_DATA);
		if (ret) {
			data->state = FAILED;
			goto release_firmware;
		}

		frame_size = ((*(fw->data + pos) << 8) | *(fw->data + pos + 1));

		if (frame % 10 == 0)
			dev_info(dev, "Writing frame %d, %d/%zd bytes\n", frame, pos, fw->size);


		/* Take account of CRC bytes */
		frame_size += 2;

		/* Write one frame to device */
		ret = mxt_fw_write(data, fw->data + pos, frame_size);
		if (ret) {
			data->state = FAILED;
			goto release_firmware;
		}

		ret = mxt_check_bootloader(data, MXT_FRAME_CRC_PASS);
		if (ret) {
			retry++;

			/* Back off by 20ms per retry */
			msleep(retry * 20);

			if (retry > 20) {
				data->state = FAILED;
				goto release_firmware;
			}
		} else {
			retry = 0;
			pos += frame_size;
			dev_info(dev, "Updated %d/%zd bytes\n", pos, fw->size);
			frame++;
		}
	}

	data->state = APPMODE;

 release_firmware:
	release_firmware(fw);
	return ret;
}

static ssize_t mxt_update_fw_store(struct device *dev,
				   struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int error;
	int ret = count;

	disable_irq(data->client->irq);

	error = mxt_load_fw(dev, MXT_FW_NAME);
	if (error) {
		dev_err(dev, "The firmware update failed(%d)\n", error);
		ret = error;
	} else {
		dev_info(dev, "The firmware update succeeded\n");

		/* Wait for reset */
		msleep(MXT_FWRESET_TIME);

		data->state = INIT;
		kfree(data->object_table);
		data->object_table = NULL;
		kfree(data->msg_buf);
		data->msg_buf = NULL;

		error = mxt_initialize(data);
		if (error)
			ret = -EINVAL;
	}

	enable_irq(data->client->irq);
	return ret;
}

static ssize_t mxt_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int count = 0;

	count += sprintf(buf + count, "%d", data->info.version);
	count += sprintf(buf + count, "\n");

	return count;
}

static ssize_t mxt_build_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int count = 0;

	count += sprintf(buf + count, "%d", data->info.build);
	count += sprintf(buf + count, "\n");

	return count;
}

static ssize_t mxt_pause_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	ssize_t count;
	char c;

	c = data->driver_paused ? '1' : '0';
	count = sprintf(buf, "%c\n", c);

	return count;
}

static ssize_t mxt_pause_store(struct device *dev,
			       struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		data->driver_paused = (i == 1);
		dev_dbg(dev, "%s\n", i ? "paused" : "unpaused");
		return count;
	} else {
		dev_dbg(dev, "pause_driver write error\n");
		return -EINVAL;
	}
}

static ssize_t mxt_debug_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int count;
	char c;

	c = data->debug_enabled ? '1' : '0';
	count = sprintf(buf, "%c\n", c);

	return count;
}

static ssize_t mxt_debug_enable_store(struct device *dev,
				      struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		data->debug_enabled = (i == 1);

		dev_dbg(dev, "%s\n", i ? "debug enabled" : "debug disabled");
		return count;
	} else {
		dev_dbg(dev, "debug_enabled write error\n");
		return -EINVAL;
	}
}

static int mxt_check_mem_access_params(struct mxt_data *data, loff_t off, size_t *count)
{
	if (data->state != APPMODE) {
		dev_err(&data->client->dev, "Not in APPMODE\n");
		return -EINVAL;
	}

	if (off >= data->mem_size)
		return -EIO;

	if (off + *count > data->mem_size)
		*count = data->mem_size - off;

	if (*count > MXT_MAX_BLOCK_WRITE)
		*count = MXT_MAX_BLOCK_WRITE;

	return 0;
}

static ssize_t mxt_mem_access_read(struct file *filp, struct kobject *kobj,
				   struct bin_attribute *bin_attr, char *buf, loff_t off,
				   size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret = 0;

	ret = mxt_check_mem_access_params(data, off, &count);
	if (ret < 0)
		return ret;

	if (count > 0)
		ret = mxt_read_reg(data->client, off, count, buf);

	return ret == 0 ? count : ret;
}

static ssize_t mxt_mem_access_write(struct file *filp, struct kobject *kobj,
				    struct bin_attribute *bin_attr, char *buf, loff_t off,
				    size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret = 0;

	ret = mxt_check_mem_access_params(data, off, &count);
	if (ret < 0)
		return ret;

	if (count > 0)
		ret = mxt_write_block(data->client, off, count, buf);

	return ret == 0 ? count : 0;
}

static ssize_t mxt_t48_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	ssize_t count;
	char c;

	c = data->handle_noise ? '1' : '0';
	count = sprintf(buf, "%c\n", c);

	return count;
}

static ssize_t mxt_t48_ctrl_store(struct device *dev,
				  struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int error;
	u8 val;

	if (data->state != APPMODE) {
		dev_err(dev, "Not in APPMODE\n");
		return -EINVAL;
	}

	if (sscanf(buf, "%hhd", &val) == 1) {
		data->handle_noise = val;

		dev_info(dev, "atmel plug in value %d\n", val);	/* to be removed later */

		if (val) {
			if (data->t48_charger_enabled == 0) {
				/* when plug in, we enable charger bit */
				mxt_set_t48_charger_bit(data, 1);

				/* set charger time to 73 when connected, this effectively is no frequency hopping */
				error =
				    mxt_write_object(data, MXT_GEN_ACQUIRE_T8, MXT_ACQUIRE_CHRGTIME,
						     73);
				if (error)
					dev_err(dev, "charger time write failure\n");

				mxt_handle_antitouch(data, MXT_AT_ACTION_PLUGIN);
			}

		} else {
			if (data->t48_charger_enabled) {
				/* when plug out, we need to turn off charger bit */
				mxt_set_t48_charger_bit(data, 0);

				/* restore charger time when not connected */
				error =
				    mxt_write_object(data, MXT_GEN_ACQUIRE_T8, MXT_ACQUIRE_CHRGTIME,
						     data->chargertime);
				if (error)
					dev_err(dev, "charger time write failure\n");

				mxt_handle_antitouch(data, MXT_AT_ACTION_PLUGOUT);
			}
		}

		return count;
	} else {
		dev_err(dev, "t48 store error: can't read user input value\n");
		return -EINVAL;
	}
}

static ssize_t mxt_noise_step_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	ssize_t count;

	count = sprintf(buf, "%d\n", data->noise_step);

	return count;
}

static ssize_t mxt_noise_step_store(struct device *dev,
				    struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	u8 val;

	if (sscanf(buf, "%hhd", &val) == 1) {
		if (val > data->noise_threshold)
			dev_err(dev, "error: can't be bigger than the current noise threshold %d\n",
				data->noise_threshold);
		else
			data->noise_step = val;

		return count;
	} else {
		dev_err(dev, "error: can't read user input value\n");
		return -EINVAL;
	}
}

static ssize_t mxt_noise_threshold_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	ssize_t count;

	count = sprintf(buf, "%d\n", data->noise_threshold);

	return count;
}

static ssize_t mxt_noise_threshold_store(struct device *dev,
					 struct device_attribute *attr, const char *buf,
					 size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	u8 val;

	if (sscanf(buf, "%hhd", &val) == 1) {
		if (val < data->noise_step)
			dev_err(dev, "error: can't be smaller than the current noise step %d\n",
				data->noise_step);
		else
			data->noise_threshold = val;

		return count;
	} else {
		dev_err(dev, "error: can't read user input value\n");
		return -EINVAL;
	}
}

static ssize_t mxt_action_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	u8 val;

	if (sscanf(buf, "%hhd", &val) == 1) {
		dev_info(dev, "got action code %d\n", val);

		if (val == 1) {	/* User unlocked */
			mxt_handle_antitouch(data, MXT_AT_ACTION_UNLOCK);
		}
		return count;
	} else {
		dev_err(dev, "error: can't read user input value\n");
		return -EINVAL;
	}
}

static DEVICE_ATTR(update_fw, 0664, NULL, mxt_update_fw_store);
static DEVICE_ATTR(debug_enable, 0664, mxt_debug_enable_show, mxt_debug_enable_store);
static DEVICE_ATTR(pause_driver, 0664, mxt_pause_show, mxt_pause_store);
static DEVICE_ATTR(t48_ctrl, 0660, mxt_t48_ctrl_show, mxt_t48_ctrl_store);	/* t48 noise suppression charger bit on/off */
static DEVICE_ATTR(noise_step, 0660, mxt_noise_step_show, mxt_noise_step_store);
static DEVICE_ATTR(noise_threshold, 0660, mxt_noise_threshold_show, mxt_noise_threshold_store);
static DEVICE_ATTR(action, 0220, NULL, mxt_action_store);
static DEVICE_ATTR(version, 0444, mxt_version_show, NULL);
static DEVICE_ATTR(build, 0444, mxt_build_show, NULL);

static struct attribute *mxt_attrs[] = {
	&dev_attr_version.attr,
	&dev_attr_build.attr,
	&dev_attr_update_fw.attr,
	&dev_attr_debug_enable.attr,
	&dev_attr_pause_driver.attr,
	&dev_attr_t48_ctrl.attr,
	&dev_attr_noise_step.attr,
	&dev_attr_noise_threshold.attr,
	&dev_attr_action.attr,
	NULL
};

static const struct attribute_group mxt_attr_group = {
	.attrs = mxt_attrs,
};

static void mxt_start(struct mxt_data *data)
{
	int error;
	struct device *dev = &data->client->dev;

	if (data->is_stopped == 0)
		return;

	dev_info(dev, "mxt_start begin\n");

	error = mxt_set_power_cfg(data, MXT_POWER_CFG_RUN);
	if (error)
		return;

	error = mxt_write_object(data, MXT_GEN_COMMAND_T6, MXT_COMMAND_CALIBRATE, 1);
	if (error) {
		dev_err(dev, "Error %d doing calibrate\n", error);
		return;
	}

	data->is_stopped = 0;

	if (!error)
		dev_dbg(dev, "MXT started\n");

	dev_info(dev, "mxt_start end\n");
}

static void mxt_stop(struct mxt_data *data)
{
	int error;
	struct device *dev = &data->client->dev;

	if (data->is_stopped)
		return;

	dev_info(dev, "mxt_stop begin\n");

	error = mxt_set_power_cfg(data, MXT_POWER_CFG_DEEPSLEEP);

	mxt_reset_fingers(data);

	data->is_stopped = 1;

	if (!error)
		dev_dbg(dev, "MXT suspended\n");

	dev_info(dev, "mxt_stop end\n");
}

static int mxt_power_on(struct i2c_client *client, struct mxt_platform_data *pdata)
{
	/* Keep the touch chip in reset, until AVDD is up */
	gpio_direction_output(pdata->reset_gpio, 0);

	msleep(MXT_RESET_TIME);

	hwPowerOn(MXT_AVDD_REGULATOR_NAME, MXT_AVDD_VOLTAGE, "Touch VGP4");
	/* pmic_ldo_vol_sel(MXT_AVDD_REGULATOR_NAME, MXT_AVDD_VOLTAGE); */
	/* pmic_ldo_enable(MXT_AVDD_REGULATOR_NAME, KAL_TRUE); */

	/* Datasheet says 90ns is enough, this is much longer */
	msleep(MXT_RESET_TIME);

	gpio_set_value(pdata->reset_gpio, 1);

	msleep(MXT_POWERON_DELAY);

	return 0;
}

static int mxt_request_gpios(struct i2c_client *client, struct mxt_platform_data *pdata)
{
	/* Configure irq gpio line as input and set it to high */
	/*mt_pin_set_pull(pdata->irq_gpio, MT_PIN_PULL_UP);*/
	gpio_direction_input(pdata->irq_gpio);

	return 0;
}

static int mxt_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct mxt_platform_data *pdata;
	struct mxt_data *data;
	struct input_dev *input_dev;
	int error;

	dev_info(&client->dev, "atmel touch driver version: %d.%d\n", MXT_DRIVER_VER_MAJOR,
		 MXT_DRIVER_VER_MINOR);

	pdata = client->dev.platform_data;
	if (!pdata) {
		dev_err(&client->dev, "No platform data provided\n");
		return -ENODEV;
	}

	error = mxt_request_gpios(client, pdata);
	if (error) {
		dev_err(&client->dev, "Failed to request touch controller gpios"
			" (error %d)\n", error);
		return error;
	}

	error = mxt_power_on(client, pdata);
	if (error) {
		dev_err(&client->dev, "Failed to power on touch controller " "(error %d)\n", error);
		return error;
	}

	data = kzalloc(sizeof(struct mxt_data), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!data || !input_dev) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		error = -ENOMEM;
		goto err_free_mem;
	}

	global_data = data;

	data->state = INIT;

	data->client = client;
	data->input_dev = input_dev;
	data->pdata = pdata;
	data->irq = client->irq;

	/* Initialize i2c device */
	error = mxt_initialize(data);
	if (error)
		goto err_free_object;

	/* Initialize input device */
	input_dev->name = "AtmelTouch";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);

	/* For single touch */
	input_set_abs_params(input_dev, ABS_X, 0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, data->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE, 0, 255, 0, 0);

	/* For multi touch */
	input_mt_init_slots(input_dev, data->num_touchids, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, MXT_MAX_AREA, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, data->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);

	input_set_drvdata(input_dev, data);
	i2c_set_clientdata(client, data);

	data->handle_noise = 0;	/* this will be set by sysfs from upper layer */
	data->noise_step = 25;
	data->t48_charger_enabled = 0;	/* by default config file has t48 charger bit off */

	/* Set up COMMSCONFIG for edge-triggered interrupt */
	error = mxt_write_object(data, MXT_SPT_COMMSCONFIG_T18, MXT_COMMS_CTRL, MXT_COMMS_RETRIGEN);
	if (error) {
		dev_err(&client->dev, "Error %d configuring T18\n", error);
		goto err_free_object;
	}

	error = request_threaded_irq(client->irq, NULL, mxt_interrupt,
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "mxt_ts", data);

	if (error) {
		dev_err(&client->dev, "Error %d registering irq %d\n", error, client->irq);
		goto err_free_object;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = mxt_early_suspend;
	data->early_suspend.resume = mxt_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	if (data->state != BOOTLOADER) {
		error = mxt_make_highchg(data);
		if (error) {
			dev_err(&client->dev, "Failed to make high CHG\n");
			goto err_free_irq;
		}
	} /* else
		disable_irq(client->irq);*/

	error = input_register_device(input_dev);
	if (error) {
		dev_err(&client->dev, "Error %d registering input device\n", error);
		goto err_free_irq;
	}

	INIT_DELAYED_WORK(&data->action_work, mxt_action_work_handler);

	error = sysfs_create_group(&client->dev.kobj, &mxt_attr_group);
	if (error) {
		dev_err(&client->dev, "Failure %d creating sysfs group\n", error);
		goto err_unregister_device;
	}

	sysfs_bin_attr_init(&data->mem_access_attr);
	data->mem_access_attr.attr.name = "mem_access";
	data->mem_access_attr.attr.mode = S_IRUGO | S_IWUSR;
	data->mem_access_attr.read = mxt_mem_access_read;
	data->mem_access_attr.write = mxt_mem_access_write;
	data->mem_access_attr.size = data->mem_size;

	if (sysfs_create_bin_file(&client->dev.kobj, &data->mem_access_attr) < 0) {
		dev_err(&client->dev, "Failed to create %s\n", data->mem_access_attr.attr.name);
		goto err_remove_sysfs_group;
	}

	if (data->state != BOOTLOADER) {
		data->state = APPMODE;
	}

	data->is_stopped = 1;
	mxt_start(data);

	return 0;

 err_remove_sysfs_group:
	sysfs_remove_group(&client->dev.kobj, &mxt_attr_group);
 err_unregister_device:
	input_unregister_device(input_dev);
	input_dev = NULL;
 err_free_irq:
	 free_irq(client->irq, data);
 err_free_object:
	kfree(data->msg_buf);
	kfree(data->object_table);
 err_free_mem:
	input_free_device(input_dev);
	kfree(data);

	global_data = NULL;

	return error;
}

static int mxt_remove(struct i2c_client *client)
{
	struct mxt_data *data = i2c_get_clientdata(client);

	dev_info(&client->dev, "atmel remove\n");
	disable_irq(data->client->irq);
	sysfs_remove_bin_file(&client->dev.kobj, &data->mem_access_attr);
	sysfs_remove_group(&client->dev.kobj, &mxt_attr_group);
	cancel_delayed_work_sync(&data->action_work);
	input_unregister_device(data->input_dev);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif

	kfree(data->msg_buf);
	kfree(data->object_table);
	kfree(data);

	global_data = NULL;

	return 0;
}

#ifdef CONFIG_PM
static int mxt_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;

	disable_irq(client->irq);

	/* this can't be inside the mutex */
	mxt_handle_antitouch(data, MXT_AT_ACTION_SUSPEND);

	mutex_lock(&input_dev->mutex);

	mxt_stop(data);

	mutex_unlock(&input_dev->mutex);

	return 0;
}

static int mxt_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;

	mutex_lock(&input_dev->mutex);

	mxt_make_highchg(data);

	mxt_start(data);

	mutex_unlock(&input_dev->mutex);

	enable_irq(client->irq);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mxt_early_suspend(struct early_suspend *es)
{
	struct mxt_data *mxt;
	mxt = container_of(es, struct mxt_data, early_suspend);

	if (mxt_suspend(&mxt->client->dev) != 0)
		dev_err(&mxt->client->dev, "%s: failed\n", __func__);
}

static void mxt_late_resume(struct early_suspend *es)
{
	struct mxt_data *mxt;
	mxt = container_of(es, struct mxt_data, early_suspend);

	if (mxt_resume(&mxt->client->dev) != 0)
		dev_err(&mxt->client->dev, "%s: failed\n", __func__);
}

#else

static const struct dev_pm_ops mxt_pm_ops = {
	.suspend = mxt_suspend,
	.resume = mxt_resume,
};
#endif
#endif

static void mxt_shutdown(struct i2c_client *client)
{
	dev_info(&client->dev, "atmel shutdown\n");
	mxt_remove(client);
}


static const struct i2c_device_id mxt_id[] = {
	{"qt602240_ts", 0},
	{"atmel_mxt_ts", 0},
	{"mXT224", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, mxt_id);

static struct i2c_driver mxt_driver = {
	.driver = {
		   .name = "atmel_mxt_ts",
		   .owner = THIS_MODULE,
#if (defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND))
		   .pm = &mxt_pm_ops,
#endif
		   },
	.probe = mxt_probe,
	.remove = mxt_remove,
	.shutdown = mxt_shutdown,
	.id_table = mxt_id,
};

module_i2c_driver(mxt_driver);

/* Module information */
MODULE_AUTHOR("Joonyoung Shim <jy0922.shim@samsung.com>");
MODULE_DESCRIPTION("Atmel maXTouch Touchscreen driver");
MODULE_LICENSE("GPL");
