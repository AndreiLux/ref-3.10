/*
 * Huawei Touchscreen Driver
 *
 * Copyright (C) 2013 Huawei Device Co.Ltd
 * License terms: GNU General Public License (GPL) version 2
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/regulator/consumer.h>
#include <linux/string.h>
#include <linux/of_gpio.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/sched/rt.h>
#include <linux/fb.h>
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif
#include "huawei_touchscreen_chips.h"
#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif
#include <linux/huawei/dsm_pub.h>

#define TP_PHYS_BASE		(0XFFF34000)
#define LDO17_PHYS_ADDR 	(TP_PHYS_BASE +	(0X93 << 2))
#define LSW50_PHYS_ADDR	(TP_PHYS_BASE + (0xAC << 2))
#define DATA_SIZE	(4)

static struct dsm_dev dsm_tp = {
	.name = "dsm_tp",
	.fops = NULL,
	.buff_size = 1024,
};

static struct dsm_client *tp_dclient = NULL;

u8 *LDO17_status = NULL;
u8 *LSW50_status = NULL;

#define TS_BUS_TYPE "touchscreen/bus_type"
#define TS_BUS_ID "touchscreen/bus_id"
#define TS_PRODUCT "touchscreen/product"

u8 g_ts_log_cfg = 0;

struct ts_data g_ts_data;

static struct ts_cmd_node ping_cmd_buff;
static struct ts_cmd_node pang_cmd_buff;

/*external variable declare*/

/*global variable declare*/

/*external function declare*/
extern int i2c_check_addr_busy(struct i2c_adapter *adapter, int addr);

static struct ts_device_data g_ts_device_map[] =
{
	[0]	=   {
		.chip_name = "synaptics",
		.irq_gpio = TS_IO_UNDEFINE, //must be defined in dts/boardid
		.irq_config = TS_IRQ_CFG_UNDEFINE,//must be defined in dts/boardid
		.reset_gpio = TS_IO_UNDEFINE,//must be defined in dts/boardid
		.ops = &ts_synaptics_ops,
	},

	[1]	=	{
		.chip_name = "cypress",
		.irq_gpio = TS_IO_UNDEFINE,
		.irq_config = TS_IRQ_CFG_UNDEFINE,
		.reset_gpio = TS_IO_UNDEFINE,
		.ops = &ts_cypress_ops,
	},
};

int ts_i2c_write(u8 *buf, u16 length)
{
	int count = 0;
	int ret;

	do {
		ret = i2c_master_send(g_ts_data.client, buf, length);
		if (ret == length)
			return NO_ERR;
		msleep(I2C_WAIT_TIME);
	} while (++count < I2C_RW_TRIES);

	g_ts_data.dsm_info.constraints_LDO17_status = readl(LDO17_status);
	g_ts_data.dsm_info.constraints_LSW50_status = readl(LSW50_status);
	if (!dsm_client_ocuppy(tp_dclient)) {
		dsm_client_record(tp_dclient, "irq_gpio:%d\tvalue:%d\n", g_ts_data.chip_data->irq_gpio,
			gpio_get_value(g_ts_data.chip_data->irq_gpio));
		dsm_client_record(tp_dclient, "reset_gpio:%d\tvalue:%d\n", g_ts_data.chip_data->reset_gpio,
			gpio_get_value(g_ts_data.chip_data->reset_gpio));
		dsm_client_record(tp_dclient, "LDO17_status is 0x%x.\nLSW50_status is 0x%x\n", 
			g_ts_data.dsm_info.constraints_LDO17_status, g_ts_data.dsm_info.constraints_LSW50_status);
		dsm_client_notify(tp_dclient, DSM_TP_I2C_RW_ERROR_NO);
		dsm_client_notify(tp_dclient, DSM_TP_I2C_RW_ERROR_NO);
	}
	TS_LOG_ERR("ts_i2c_write failed\n");
	return -EIO;
}

int ts_spi_write(u8 *buf, u16 length)
{
	return NO_ERR;
}

int ts_i2c_read(u8 *reg_addr, u16 reg_len, u8 *buf, u16 len)
{
	int count = 0;
	int ret;
	struct i2c_msg xfer[2];

	/*register addr*/
	xfer[0].addr = g_ts_data.client->addr;
	xfer[0].flags = 0;
	xfer[0].len = reg_len;
	xfer[0].buf = reg_addr;

	/* Read data */
	xfer[1].addr = g_ts_data.client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = len;
	xfer[1].buf = buf;

	do {
		ret = i2c_transfer(g_ts_data.client->adapter, xfer, 2);
		if (ret == 2)
			return NO_ERR;
		msleep(I2C_WAIT_TIME);
	} while (++count < I2C_RW_TRIES);

	g_ts_data.dsm_info.constraints_LDO17_status = readl(LDO17_status);
	g_ts_data.dsm_info.constraints_LSW50_status = readl(LSW50_status);
	if (!dsm_client_ocuppy(tp_dclient)) {
		dsm_client_record(tp_dclient, "irq_gpio:%d\tvalue:%d\n", g_ts_data.chip_data->irq_gpio,
			gpio_get_value(g_ts_data.chip_data->irq_gpio));
		dsm_client_record(tp_dclient, "reset_gpio:%d\tvalue:%d\n", g_ts_data.chip_data->reset_gpio,
			gpio_get_value(g_ts_data.chip_data->reset_gpio));
		dsm_client_record(tp_dclient, "LDO17_status is 0x%x.\nLSW50_status is 0x%x\n", 
			g_ts_data.dsm_info.constraints_LDO17_status, g_ts_data.dsm_info.constraints_LSW50_status);
		dsm_client_notify(tp_dclient, DSM_TP_I2C_RW_ERROR_NO);
	}
	TS_LOG_ERR("ts_i2c_read failed\n");
	return -EIO;
}

int ts_spi_read(u8 *reg_addr, u16 reg_len, u8 *buf, u16 len)
{
	return NO_ERR;
}

static struct ts_bus_info ts_bus_i2c_info = {
	.btype		= TS_BUS_I2C,
	.bus_write	= ts_i2c_write,
	.bus_read	= ts_i2c_read,
};

static struct ts_bus_info ts_bus_spi_info = {
	.btype		= TS_BUS_SPI,
	.bus_write	= ts_spi_write,
	.bus_read	= ts_spi_read,
};

int register_algo_func(struct ts_device_data *chip_data, struct ts_algo_func *fn)
{
	int error = -EIO;

	if (!chip_data ||!fn)
		goto out;

	fn->algo_index = chip_data->algo_size;
	list_add_tail( &fn->node, &chip_data->algo_head);
	chip_data->algo_size++;
	smp_mb();
	error = NO_ERR;

out:
	return error;
}

int put_one_cmd(struct ts_cmd_node *cmd, int timeout)
{
	int error = -EIO;
	unsigned long flags;
	struct ts_cmd_queue *q;
	struct ts_cmd_sync *sync = NULL;

	if (!cmd) {
		TS_LOG_ERR("find null pointer\n");
		goto out;
	}

	if(TS_UNINIT == atomic_read(&g_ts_data.state)){
		TS_LOG_ERR("ts module not initialize\n");
		goto out;
	}

	if (timeout) {
		sync = (struct ts_cmd_sync *)kzalloc(sizeof(struct ts_cmd_sync), GFP_KERNEL);
		if (NULL == sync) {
			TS_LOG_ERR("failed to kzalloc completion\n");
			error = -ENOMEM;
			goto out;
		}
		init_completion(&sync->done);
		atomic_set(&sync->timeout_flag, TS_NOT_TIMEOUT);
		cmd->sync = sync;
	} else {
		cmd->sync = NULL;
	}

	q = &g_ts_data.queue;

	spin_lock_irqsave(&q->spin_lock, flags);
	smp_wmb();
	if (q->cmd_count == q->queue_size) {
		spin_unlock_irqrestore(&q->spin_lock, flags);
		TS_LOG_ERR("queue is full\n");
		BUG_ON(1);
		error = -EIO;
		goto free_sync;
	}
	memcpy(&q->ring_buff[q->wr_index], cmd, sizeof(struct ts_cmd_node));
	q->cmd_count++;
	q->wr_index++;
	q->wr_index %= q->queue_size;
	smp_mb();
	spin_unlock_irqrestore(&q->spin_lock, flags);
	TS_LOG_DEBUG("put one cmd :%d in ring buff\n", cmd->command);
	error = NO_ERR;
	wake_up_process(g_ts_data.ts_task); //wakeup process

	if (timeout && !(wait_for_completion_timeout(&sync->done, abs(timeout)*HZ))) {
		atomic_set(&sync->timeout_flag, TS_TIMEOUT);
		TS_LOG_ERR("wait for cmd respone timeout\n");
		error = -EBUSY;
		goto out;
	}
	smp_wmb();

free_sync:
	if (sync) {
		kfree(sync);
	}
out:
	return error;
}

static int get_one_cmd(struct ts_cmd_node *cmd)
{
	unsigned long flags;
	int error = -EIO;
	struct ts_cmd_queue *q;

	if (unlikely(!cmd)) {
		TS_LOG_ERR("find null pointer\n");
		goto out;
	}

	q = &g_ts_data.queue;

	spin_lock_irqsave(&q->spin_lock, flags);
	smp_wmb();
	if (!q->cmd_count) {
		TS_LOG_DEBUG("queue is empty\n");
		spin_unlock_irqrestore(&q->spin_lock, flags);
		goto out;
	}
	memcpy(cmd,&q->ring_buff[q->rd_index], sizeof(struct ts_cmd_node));
	q->cmd_count--;
	q->rd_index++;
	q->rd_index %= q->queue_size;
	smp_mb();
	spin_unlock_irqrestore(&q->spin_lock, flags);
	TS_LOG_DEBUG("get one cmd :%d from ring buff\n", cmd->command);
	error = NO_ERR;

out:
	return error;
}

static ssize_t ts_chip_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int error = NO_ERR;
	struct ts_cmd_node *cmd = NULL;
	struct ts_chip_info_param *info = NULL;

	TS_LOG_INFO("ts_chip_info_show called\n");

	if (dev == NULL) {
		TS_LOG_ERR("dev is null\n");
		error = -EINVAL;
		goto out;
	}

	cmd = (struct ts_cmd_node *)kzalloc(sizeof(struct ts_cmd_node), GFP_KERNEL);
	if (!cmd) {
		TS_LOG_ERR("malloc failed\n");
		error = -ENOMEM;
		goto out;
	}

	info = (struct ts_chip_info_param *)kzalloc(sizeof(struct ts_chip_info_param), GFP_KERNEL);
	if (!info) {
		TS_LOG_ERR("malloc failed\n");
		error = -ENOMEM;
		goto out;
	}

	cmd->command = TS_GET_CHIP_INFO;
	cmd->cmd_param.prv_params = (void *)info;

	error = put_one_cmd(cmd, SHORT_SYNC_TIMEOUT);
	if (error) {
		TS_LOG_ERR("put cmd error :%d\n", error);
		error = -EBUSY;
		goto out;
	}

	if (info->status != TS_ACTION_SUCCESS) {
		TS_LOG_ERR("read action failed\n");
		error = -EIO;
		goto out;
	}

	error = snprintf(buf, CHIP_INFO_LENGTH+CHIP_INFO_LENGTH*2+1,
			"%s-%s\n", info->ic_vendor, info->fw_vendor);

out:
	if (cmd)
		kfree(cmd);
	if (info)
		kfree(info);
	TS_LOG_DEBUG("ts_chip_info_show done\n");
	return error;
}

static ssize_t ts_calibrate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int error = NO_ERR;
	struct ts_cmd_node *cmd = NULL;
	struct ts_calibrate_info info;

	TS_LOG_INFO("ts_calibrate_show called\n");

	if (dev == NULL) {
		TS_LOG_ERR("dev is null\n");
		error = -EINVAL;
		goto out;
	}

	cmd = (struct ts_cmd_node *)kzalloc(sizeof(struct ts_cmd_node), GFP_KERNEL);
	if (!cmd) {
		TS_LOG_ERR("malloc failed\n");
		error = -ENOMEM;
		goto out;
	}

	cmd->command = TS_CALIBRATE_DEVICE;
	cmd->cmd_param.prv_params = (void *)&info;
	error = put_one_cmd(cmd, LONG_SYNC_TIMEOUT);
	if (error) {
		TS_LOG_ERR("put cmd error :%d\n", error);
		error = -EBUSY;
		goto out;
	}

	if (info.status == TS_ACTION_SUCCESS)
		error = snprintf(buf, MAX_STR_LEN, "%s\n", "success");
	else
		error = snprintf(buf, MAX_STR_LEN, "%s\n", "failed");

out:
	if (cmd)
		kfree(cmd);
	TS_LOG_DEBUG("ts_calibrate_show done\n");
	return error;
}

static ssize_t ts_glove_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int error = NO_ERR;
	struct ts_cmd_node *cmd = NULL;
	struct ts_glove_info *info = NULL;

	TS_LOG_INFO("ts_glove_mode_show called\n");

	if (dev == NULL) {
		TS_LOG_ERR("dev is null\n");
		error = -EINVAL;
		goto out;
	}

	cmd = (struct ts_cmd_node *)kzalloc(sizeof(struct ts_cmd_node), GFP_KERNEL);
	if (!cmd) {
		TS_LOG_ERR("malloc failed\n");
		error = -ENOMEM;
		goto out;
	}
	info = (struct ts_glove_info *)kzalloc(sizeof(struct ts_glove_info), GFP_KERNEL);
	if (!info) {
		TS_LOG_ERR("malloc failed\n");
		error = -ENOMEM;
		goto out;
	}

	info->op_action = TS_ACTION_READ;
	cmd->command = TS_GLOVE_SWITCH;
	cmd->cmd_param.prv_params = (void *)info;
	error = put_one_cmd(cmd, SHORT_SYNC_TIMEOUT);
	if (error) {
		TS_LOG_ERR("put cmd error :%d\n", error);
		error = -EBUSY;
		goto out;
	}

	if (info->status == TS_ACTION_SUCCESS)
		error = snprintf(buf, MAX_STR_LEN, "%d\n", info->glove_switch);
	else
		error = -EFAULT;

out:
	if (cmd)
		kfree(cmd);
	if (info)
		kfree(info);
	TS_LOG_DEBUG("ts_glove_mode_show done\n");
	return error;
}

static ssize_t ts_glove_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value;
	int error;
	struct ts_cmd_node *cmd = NULL;
	struct ts_glove_info *info = NULL;

	TS_LOG_INFO("ts_glove_mode_store called\n");

	if (dev == NULL) {
		TS_LOG_ERR("dev is null\n");
		error = -EINVAL;
		goto out;
	}

	error = sscanf(buf, "%u", &value);
	if (!error) {
		TS_LOG_ERR("sscanf return invaild :%d\n", error);
		error = -EINVAL;
		goto out;
	}
	TS_LOG_DEBUG("sscanf value is %u\n", value);

	cmd = (struct ts_cmd_node *)kzalloc(sizeof(struct ts_cmd_node), GFP_KERNEL);
	if (!cmd) {
		TS_LOG_ERR("malloc failed\n");
		error = -ENOMEM;
		goto out;
	}
	info = (struct ts_glove_info *)kzalloc(sizeof(struct ts_glove_info), GFP_KERNEL);
	if (!info) {
		TS_LOG_ERR("malloc failed\n");
		error = -ENOMEM;
		goto out;
	}

	info->op_action = TS_ACTION_WRITE;
	info->glove_switch = value;
	cmd->command = TS_GLOVE_SWITCH;
	cmd->cmd_param.prv_params = (void *)info;
	error = put_one_cmd(cmd, SHORT_SYNC_TIMEOUT);
	if (error) {
		TS_LOG_ERR("put cmd error :%d\n", error);
		error = -EBUSY;
		goto out;
	}

	if (info->status != TS_ACTION_SUCCESS) {
		TS_LOG_ERR("action failed\n");
		error = -EIO;
		goto out;
	}

	error = count;

out:
	if (cmd)
		kfree(cmd);
	if (info)
		kfree(info);
	TS_LOG_DEBUG("ts_glove_mode_store done\n");
	return error;
}

static int ts_send_holster_cmd(void)
{
	int error;
	struct ts_cmd_node cmd;

	cmd.command = TS_HOLSTER_SWITCH;
	cmd.cmd_param.prv_params = (void *)&g_ts_data.feature_info.holster_info;
	error = put_one_cmd(&cmd, SHORT_SYNC_TIMEOUT);
	if (error) {
		TS_LOG_ERR("put cmd error :%d\n", error);
		error = -EBUSY;
		goto out;
	}
	if (g_ts_data.feature_info.holster_info.status != TS_ACTION_SUCCESS) {
		TS_LOG_ERR("action failed\n");
		error = -EIO;
		goto out;
	}

out:
	return error;
}

static ssize_t ts_touch_window_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int window_enable;
	int x0 = 0;
	int y0 = 0;
	int x1 = 0;
	int y1 = 0;
	int error;
	struct ts_window_info info = {0};
	struct ts_cmd_node cmd;

	error = sscanf(buf, "%d %d %d %d %d",&window_enable, &x0, &y0, &x1, &y1);
	if (!error) {
		TS_LOG_ERR("sscanf return invaild :%d\n", error);
		error = -EINVAL;
		goto out;
	}
	TS_LOG_INFO("sscanf value is %d (%d,%d), (%d,%d)\n", window_enable, x0, y0, x1, y1);
	if (window_enable && ((x0 < 0) || (y0 < 0) || (x1 <= x0) || (y1 <= y0))) {
		TS_LOG_ERR("value is %d (%d,%d), (%d,%d)\n", window_enable, x0, y0, x1, y1);
		error = -EINVAL;
		goto out;
	}

	info.top_left_x0 = x0;
	info.top_left_y0 = y0;
	info.bottom_right_x1 = x1;
	info.bottom_right_y1 = y1;
	info.window_enable = window_enable;

	cmd.command = TS_TOUCH_WINDOW;
	cmd.cmd_param.prv_params = (void *)&info;
	error = put_one_cmd(&cmd, SHORT_SYNC_TIMEOUT);
	if (error) {
		TS_LOG_ERR("put cmd error :%d\n", error);
		error = -EBUSY;
		goto out;
	}
	if (info.status != TS_ACTION_SUCCESS) {
		TS_LOG_ERR("action failed\n");
		error = -EIO;
		goto out;
	}

	error = count;

out:
	TS_LOG_DEBUG("ts_touch_window_store done\n");
	return error;
}

static void ts_check_touch_window(struct ts_fingers *finger)
{
	int id;
	int flag = 0;
	int x0, y0, x1, y1;
	int window_enable;

	window_enable = g_ts_data.feature_info.window_info.window_enable;
	x0 = g_ts_data.feature_info.window_info.top_left_x0;
	y0 = g_ts_data.feature_info.window_info.top_left_y0;
	x1 = g_ts_data.feature_info.window_info.bottom_right_x1;
	y1 = g_ts_data.feature_info.window_info.bottom_right_y1;

	if (0 == window_enable) {
		TS_LOG_DEBUG("no need to part report\n");
		return;
	}

	if (finger->fingers[0].status != TS_FINGER_RELEASE) {
		for (id = 0; id < TS_MAX_FINGER; id++){
			if (finger->fingers[id].status != 0) {
				if ((finger->fingers[id].x >= x0) && (finger->fingers[id].x <= x1)
					&& (finger->fingers[id].y >= y0) && (finger->fingers[id].y <= y1)) {
					flag = 1;
				} else {
					finger->fingers[id].status = 0;
				}
			}
		}
		if (!flag)
			finger->fingers[0].status = TS_FINGER_RELEASE;
	}
}

static ssize_t ts_sensitivity_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value;
	int error;
	struct ts_holster_info *info = &g_ts_data.feature_info.holster_info;

	error = sscanf(buf, "%u", &value);
	if (!error) {
		TS_LOG_ERR("sscanf return invaild :%d\n", error);
		error = -EINVAL;
		goto out;
	}
	TS_LOG_DEBUG("sscanf value is %u\n", value);

	info->op_action = TS_ACTION_WRITE;
	info->holster_switch = value;

	error = ts_send_holster_cmd();
	if (error) {
		TS_LOG_ERR("ts_send_holster_cmd failed\n");
		error = -ENOMEM;
		goto out;
	}

	error = count;

out:
	TS_LOG_DEBUG("ts_sensitivity_store done\n");
	return error;
}

static ssize_t ts_hand_detect_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int error = NO_ERR;
	struct ts_cmd_node *cmd = NULL;
	struct ts_hand_info *info = NULL;

	TS_LOG_INFO("ts_hand_detect_show called\n");

	if (dev == NULL) {
		TS_LOG_ERR("dev is null\n");
		error = -EINVAL;
		goto out;
	}

	cmd = (struct ts_cmd_node *)kzalloc(sizeof(struct ts_cmd_node), GFP_KERNEL);
	if (!cmd) {
		TS_LOG_ERR("malloc failed\n");
		error = -ENOMEM;
		goto out;
	}
	info = (struct ts_hand_info *)kzalloc(sizeof(struct ts_hand_info), GFP_KERNEL);
	if (!info) {
		TS_LOG_ERR("malloc failed\n");
		error = -ENOMEM;
		goto out;
	}

	info->op_action = TS_ACTION_READ;
	cmd->command = TS_HAND_DETECT;
	cmd->cmd_param.prv_params = (void *)info;
	error = put_one_cmd(cmd, SHORT_SYNC_TIMEOUT);
	if (error) {
		TS_LOG_ERR("put cmd error :%d\n", error);
		error = -EBUSY;
		goto out;
	}

	if (info->status == TS_ACTION_SUCCESS)
		error = snprintf(buf, MAX_STR_LEN ,"%d\n", info->hand_value);
	 else
		error =  -EFAULT;

out:
	if (cmd)
		kfree(cmd);
	if (info)
		kfree(info);
	TS_LOG_DEBUG("done\n");
	return error;
}

static ssize_t ts_loglevel_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int error = NO_ERR;

	TS_LOG_INFO("ts_loglevel_show called\n");

	if (dev == NULL) {
		TS_LOG_ERR("dev is null\n");
		error = -EINVAL;
		goto out;
	}

	error = snprintf(buf, MAX_STR_LEN, "%d\n", g_ts_log_cfg);

out:
	TS_LOG_INFO("ts_loglevel_show done\n");
	return error;
}

static ssize_t ts_touch_window_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int error = NO_ERR;
	struct ts_window_info *info = &g_ts_data.feature_info.window_info;

	error = snprintf(buf, MAX_STR_LEN, "%d %d %d %d %d\n", info->window_enable, info->top_left_x0,
				info->top_left_y0, info->bottom_right_x1, info->bottom_right_y1);

	return error;
}

static ssize_t ts_loglevel_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value;
	int error;

	TS_LOG_INFO("ts_loglevel_store called\n");

	if (dev == NULL) {
		TS_LOG_ERR("dev is null\n");
		error = -EINVAL;
		goto out;
	}

	error = sscanf(buf, "%u", &value);
	if (!error) {
		TS_LOG_ERR("sscanf return invaild :%d\n", error);
		error = -EINVAL;
		goto out;
	}
	TS_LOG_DEBUG("sscanf value is %u\n", value);
	g_ts_log_cfg = value;
	error = count;

out:
	TS_LOG_INFO("ts_loglevel_store done\n");
	return error;
}

static ssize_t ts_fw_update_sd_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int error = NO_ERR;
	struct ts_cmd_node cmd;

	TS_LOG_INFO("ts_fw_update_sd_store called\n");

	memset(&cmd, 0, sizeof(struct ts_cmd_node));
	cmd.command = TS_FW_UPDATE_SD;
	error = put_one_cmd(&cmd, LONG_LONG_SYNC_TIMEOUT);
	if (error) {
		TS_LOG_ERR("ts_fw_update_sd_store failed:%d\n", error);
	}
	TS_LOG_INFO("ts_fw_update_sd_store done\n");
	return count;
}
static DEVICE_ATTR(touch_chip_info, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, ts_chip_info_show, NULL);
static DEVICE_ATTR(calibrate, S_IRUSR, ts_calibrate_show, NULL);
static DEVICE_ATTR(touch_glove, S_IRUSR|S_IWUSR, ts_glove_mode_show, ts_glove_mode_store);
static DEVICE_ATTR(touch_sensitivity, S_IWUSR, NULL, ts_sensitivity_store);
static DEVICE_ATTR(hand_detect, S_IRUSR, ts_hand_detect_show, NULL);
static DEVICE_ATTR(loglevel, S_IRUSR|S_IWUSR, ts_loglevel_show, ts_loglevel_store);
static DEVICE_ATTR(touch_window, S_IRUSR|S_IWUSR, ts_touch_window_show, ts_touch_window_store);
static DEVICE_ATTR(fw_update_sd, S_IWUSR, NULL, ts_fw_update_sd_store);

static struct attribute *ts_attributes[] = {
	&dev_attr_touch_chip_info.attr,
       &dev_attr_calibrate.attr,
       &dev_attr_touch_glove.attr,
       &dev_attr_touch_sensitivity.attr,
       &dev_attr_hand_detect.attr,
       &dev_attr_loglevel.attr,
       &dev_attr_touch_window.attr,
       &dev_attr_fw_update_sd.attr,
	NULL
};

static const struct attribute_group ts_attr_group = {
	.attrs = ts_attributes,
};

static int rawdata_proc_show(struct seq_file *m, void *v)
{
	int index;
	int index1;
	short row_size = 0;
	int range_size = 0;
	int error = NO_ERR;
	struct ts_cmd_node *cmd = NULL;
	struct ts_rawdata_info *info = NULL;

	TS_LOG_INFO("rawdata_proc_show, buffer size = %d\n", m->size);
	if(m->size <= RAW_DATA_SIZE) {
		m->count = m->size;
		return 0;
	}

	cmd = (struct ts_cmd_node *)kzalloc(sizeof(struct ts_cmd_node), GFP_KERNEL);
	if (!cmd) {
		TS_LOG_ERR("malloc failed\n");
		error = -ENOMEM;
		goto out;
	}

	info = (struct ts_rawdata_info *)kzalloc(sizeof(struct ts_rawdata_info), GFP_KERNEL);
	if (!info) {
		TS_LOG_ERR("malloc failed\n");
		error = -ENOMEM;
		goto out;
	}

	info->used_size = 0;
	cmd->command = TS_READ_RAW_DATA;
	cmd->cmd_param.prv_params = (void *)info;

	error = put_one_cmd(cmd, SHORT_SYNC_TIMEOUT);
	if (error) {
		TS_LOG_ERR("put cmd error :%d\n", error);
		error = -EBUSY;
		goto out;
	}

	if (info->status != TS_ACTION_SUCCESS) {
		TS_LOG_ERR("read action failed\n");
		error = -EIO;
		goto out;
	}
	seq_printf(m, "result:%s\n", info->result);
	seq_printf(m, "*************touch data*************\n");

	row_size = info->buff[0];
	range_size = info->buff[1];
	seq_printf(m, "rx: %d, tx : %d\n", row_size, range_size);

	for (index=0; row_size*index+2 < info->used_size; index++) {
		if (0 == index) {
			seq_printf(m, "rawdata begin\n");							/*print the title*/
		}
		for (index1=0; index1 < row_size; index1++) {
			seq_printf(m, "%d,", info->buff[2+row_size*index+index1]);		/*print oneline*/
		}
		index1 = 0;
		seq_printf(m, "\n ");

		if ((range_size -1) == index) {
			seq_printf(m, "rawdata end\n");
			seq_printf(m, "noisedata begin\n");
		}
	}
	seq_printf(m, "noisedata end\n");

	error = NO_ERR;

out:
	if (info)
		kfree(info);
	if (cmd)
		kfree(cmd);

	TS_LOG_INFO("rawdata_proc_show done\n");
	return error;
}

static int rawdata_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, rawdata_proc_show, NULL);
}

static const struct file_operations rawdata_proc_fops = {
	.open		= rawdata_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void procfs_create(void)
{
	if (!proc_mkdir("touchscreen", NULL))
		return;
	proc_create("touchscreen/rawdata", S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, NULL, &rawdata_proc_fops);
	return;
}

int ts_power_control_notify(enum ts_pm_type pm_type, int timeout)
{
	int error;
	struct ts_cmd_node cmd;

	cmd.command = TS_POWER_CONTROL;
	cmd.cmd_param.pub_params.pm_type = pm_type;
	error = put_one_cmd(&cmd, timeout);
	if (error) {
		TS_LOG_ERR("ts_power_control_notify, put cmd error :%d\n", error);
		error = -EBUSY;
	}

	return error;
}

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	int error = NO_ERR;
	int i;
	struct fb_event *fb_event = data;
	int *blank = fb_event->data;

	for (i = 0 ; i < FB_MAX; i++) {
		if (registered_fb[i] == fb_event->info) {
			if (i == 0) {
				TS_LOG_DEBUG("Ts-index:%d, go on !\n", i);
				break;
			} else {
				TS_LOG_INFO("Ts-index:%d, exit !\n", i);
				return error;
			}
		}
	}
	TS_LOG_INFO("fb_notifier_callback, blank: %d, state: %d, event:%lu\n", *blank, atomic_read(&g_ts_data.state), event);

	if (*blank == FB_BLANK_UNBLANK) {
		if (TS_SLEEP == atomic_read(&g_ts_data.state)) {
			/*resume device*/
			switch(event) {
			case FB_EARLY_EVENT_BLANK:
				error = ts_power_control_notify(TS_RESUME_DEVICE, SHORT_SYNC_TIMEOUT);	/*touch power on*/
				if (error)
					TS_LOG_ERR("ts resume device err\n");
				error = ts_power_control_notify(TS_AFTER_RESUME, NO_SYNC_TIMEOUT);	/*enable irq*/
				if (error)
					TS_LOG_ERR("ts after resume err\n");
				break;
			case FB_EVENT_BLANK:
				TS_LOG_DEBUG("resume: event = %lu, not care\n", event);
				break;
			default:
				TS_LOG_DEBUG("resume: event = %lu, not care\n", event);
				break;
			}
		}
	} else  if (*blank == FB_BLANK_POWERDOWN){
		if (TS_WORK == atomic_read(&g_ts_data.state)) {
			/*suspend device*/
			switch(event) {
			case FB_EARLY_EVENT_BLANK:
				error = ts_power_control_notify(TS_BEFORE_SUSPEND, SHORT_SYNC_TIMEOUT);	/*disable irq*/
				if (error)
					TS_LOG_ERR("ts suspend device err\n");
				break;
			case FB_EVENT_BLANK:
				error = ts_power_control_notify(TS_SUSPEND_DEVICE, NO_SYNC_TIMEOUT);	/*touch power off*/
				if (error)
					TS_LOG_ERR("ts before suspend err\n");
				break;
			default:
				TS_LOG_DEBUG("suspend: event = %lu, not care\n", event);
				break;
			}
		}
	} else {
		TS_LOG_INFO("blank: %d, not care\n", *blank);
	}

	return NO_ERR;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void ts_early_suspend(struct early_suspend *h)
{
	bool is_in_cell = g_ts_data.chip_data->is_in_cell;
	int error = NO_ERR;

	TS_LOG_INFO("ts early suspend, %s\n", (is_in_cell == false) ? "need suspend" : "no need suspend");

	/*for the in-cell, ts_suspend_notify called by lcd, not here*/
	if (false == is_in_cell) {
		error = ts_power_control_notify(TS_BEFORE_SUSPEND, SHORT_SYNC_TIMEOUT);
		if (error)
			TS_LOG_ERR("ts before suspend err\n");
		error = ts_power_control_notify(TS_SUSPEND_DEVICE, SHORT_SYNC_TIMEOUT);
		if (error)
			TS_LOG_ERR("ts suspend device err\n");
	}

	TS_LOG_INFO("ts_early_suspend done\n");
}

static void ts_late_resume(struct early_suspend *h)
{
	bool is_in_cell = g_ts_data.chip_data->is_in_cell;
	int error = NO_ERR;
	TS_LOG_INFO("ts late resume, %s\n", (is_in_cell == false) ? "need resume" : "no need resume");

	/*for the in-cell, ts_resume_notify called by lcd, not here*/
	if (false == is_in_cell) {
		error = ts_power_control_notify(TS_RESUME_DEVICE, SHORT_SYNC_TIMEOUT);
		if (error)
			TS_LOG_ERR("ts resume device err\n");
		error = ts_power_control_notify(TS_AFTER_RESUME, SHORT_SYNC_TIMEOUT);
		if (error)
			TS_LOG_ERR("ts after resume err\n");
	}
	ts_send_holster_cmd();

	TS_LOG_INFO("ts_late_resume done\n");
}
#endif

static irqreturn_t ts_irq_handler(int irq, void *dev_id)
{
	int error = NO_ERR;
	struct ts_cmd_node cmd;

	if (g_ts_data.chip_data->ops->chip_irq_top_half)
		error = g_ts_data.chip_data->ops->chip_irq_top_half(&cmd);

	if (error)//unexpected error happen, put err cmd to ts thread
		cmd.command = TS_INT_ERR_OCCUR;
	else
		cmd.command = TS_INT_PROCESS;

	disable_irq_nosync(g_ts_data.irq_id);

	if (put_one_cmd(&cmd, NO_SYNC_TIMEOUT))
		enable_irq(g_ts_data.irq_id);

	return IRQ_HANDLED;
}

static inline void ts_proc_bottom_half(struct ts_cmd_node *in_cmd, struct ts_cmd_node *out_cmd)
{
	struct ts_device_data *dev = g_ts_data.chip_data;

	TS_LOG_DEBUG("bottom half called\n");

	//related event need process, use out cmd to notify
	if (dev->ops->chip_irq_bottom_half)
		dev->ops->chip_irq_bottom_half(in_cmd, out_cmd);
}

static void ts_watchdog_work(struct work_struct *work)
{
	int error = NO_ERR;
	struct ts_cmd_node cmd;

	TS_LOG_DEBUG("ts_watchdog_work\n");
	cmd.command = TS_CHECK_STATUS;

	error=put_one_cmd(&cmd, NO_SYNC_TIMEOUT);
	if (error) {
		TS_LOG_ERR("put TS_CHECK_STATUS cmd error :%d\n", error);
	}
	return;
}

static void ts_watchdog_timer(unsigned long data)
{
	struct ts_data *cd = (struct ts_data *)data;

	TS_LOG_DEBUG("Timer triggered\n");

	if (!cd)
		return;

	if (!work_pending(&cd->watchdog_work))
		schedule_work(&cd->watchdog_work);

	return;
}

static void ts_start_wd_timer(struct ts_data *cd)
{
	if (!TS_WATCHDOG_TIMEOUT)
		return;

	TS_LOG_DEBUG("start wd\n");
	mod_timer(&cd->watchdog_timer, jiffies +
			msecs_to_jiffies(TS_WATCHDOG_TIMEOUT));
	return;
}

static void ts_stop_wd_timer(struct ts_data *cd)
{
	if (!TS_WATCHDOG_TIMEOUT)
		return;

	TS_LOG_DEBUG("stop wd\n");
	del_timer(&cd->watchdog_timer);
	cancel_work_sync(&cd->watchdog_work);
	del_timer(&cd->watchdog_timer);
	return;
}

static inline void ts_algo_calibrate(struct ts_cmd_node *in_cmd, struct ts_cmd_node *out_cmd)
{
	int id;
	int algo_size = g_ts_data.chip_data->algo_size;
	u32 order = in_cmd->cmd_param.pub_params.algo_param.algo_order;
	struct ts_fingers *in_finger = &in_cmd->cmd_param.pub_params.algo_param.info;
	struct ts_fingers *out_finger = &out_cmd->cmd_param.pub_params.algo_param.info;
	struct ts_algo_func *algo;

	if (!algo_size) {
		TS_LOG_INFO("no algo handler, direct report\n");
		goto out;
	}

	TS_LOG_DEBUG("algo order: %d, algo_size :%d\n", order, algo_size);

	for(id=0; id < algo_size; id++)
	{
		if (order&BIT_MASK(id)) {
			TS_LOG_DEBUG("algo id:%d is setted\n", id);
			list_for_each_entry(algo, &g_ts_data.chip_data->algo_head, node) {
				if (algo->algo_index == id) {//found the right algo func
					TS_LOG_DEBUG("algo :%s called\n", algo->algo_name);
					algo->chip_algo_func(g_ts_data.chip_data, in_finger, out_finger);
					memcpy(in_finger, out_finger, sizeof(struct ts_fingers));
					break;
				}
			}
		}
	}

out:
	memcpy(&out_cmd->cmd_param.pub_params.report_info, in_finger, sizeof(struct ts_fingers));
	out_cmd->command = TS_REPORT_INPUT;
	return;
}

static inline void ts_report_input(struct ts_cmd_node *in_cmd, struct ts_cmd_node *out_cmd)
{
	struct ts_fingers *finger = &in_cmd->cmd_param.pub_params.report_info;
	struct input_dev *input_dev = g_ts_data.input_dev;
	int finger_num = 0;
	int id;

	TS_LOG_DEBUG("ts_report_input\n");
	ts_check_touch_window(finger);

	for (id = 0; id < TS_MAX_FINGER; id++) {
		if (finger->fingers[id].status == 0) {
			TS_LOG_DEBUG("never touch before: id is %d\n", id);
			continue;
		}
		if (finger->fingers[id].status == TS_FINGER_PRESS) {
			TS_LOG_DEBUG("down: id is %d\n", id);
			finger_num++;
			input_report_abs(input_dev, ABS_MT_PRESSURE, finger->fingers[id].pressure);
			input_report_abs(input_dev, ABS_MT_POSITION_X, finger->fingers[id].x);
			input_report_abs(input_dev, ABS_MT_POSITION_Y, finger->fingers[id].y);
			input_report_abs(input_dev, ABS_MT_TRACKING_ID, id);
			input_mt_sync(input_dev);				//modfiy by mengkun
		} else if (finger->fingers[id].status == TS_FINGER_RELEASE) {
			TS_LOG_DEBUG("up: id is %d\n", id);
			input_mt_sync(input_dev);	//modfiy by mengkun
		}
	}

	input_report_key(input_dev, BTN_TOUCH, finger_num);
	input_sync(input_dev);

	TS_LOG_DEBUG("ts_report_input done\n");
	return;
}

static int ts_power_control(int irq_id,
	struct ts_cmd_node *in_cmd, struct ts_cmd_node *out_cmd)
{
	int error = NO_ERR;
	enum ts_pm_type pm_type = in_cmd->cmd_param.pub_params.pm_type;
	struct ts_device_data *dev = g_ts_data.chip_data;

	switch (pm_type) {
	case TS_BEFORE_SUSPEND: 	/*do something before suspend*/
		ts_stop_wd_timer(&g_ts_data);
		disable_irq(irq_id);
		if (dev->ops->chip_before_suspend)
			error = dev->ops->chip_before_suspend();
		break;
	case TS_SUSPEND_DEVICE:  	/*device power off or sleep*/
		atomic_set(&g_ts_data.state, TS_SLEEP);
		if (dev->ops->chip_suspend)
			error = dev->ops->chip_suspend();
		break;
	case TS_RESUME_DEVICE:	 /*device power on or wakeup*/
		if (dev->ops->chip_resume)
			error = dev->ops->chip_resume();
		break;
	case TS_AFTER_RESUME:		/*do something after resume*/
		if (dev->ops->chip_after_resume)
			error = dev->ops->chip_after_resume((void *)&g_ts_data.feature_info);
		atomic_set(&g_ts_data.state, TS_WORK);
		enable_irq(irq_id);
		ts_start_wd_timer(&g_ts_data);
		break;
	default:
		TS_LOG_ERR("pm_type = %d\n", pm_type);
		error = -EINVAL;
		break;
	}

	return error;
}

static inline int ts_fw_update_boot(struct ts_cmd_node *in_cmd, struct ts_cmd_node *out_cmd)
{
	char *fw_name = in_cmd->cmd_param.pub_params.firmware_info.fw_name;
	int error = NO_ERR;
	struct ts_device_data *dev = g_ts_data.chip_data;

	if (dev->ops->chip_fw_update_boot)
		error = dev->ops->chip_fw_update_boot(fw_name);

	if(error && !dsm_client_ocuppy(tp_dclient)){
		dsm_client_record(tp_dclient, "fw update result: failed.\n");
		dsm_client_notify(tp_dclient, DSM_TP_FWUPDATE_ERROR_NO);
		strncpy(g_ts_data.dsm_info.fw_update_result, "failed", strlen("failed"));
	}

	strncpy(g_ts_data.dsm_info.fw_update_result, "success", strlen("success"));
	TS_LOG_INFO("process firmware update boot, return value:%d\n", error);

	return error;
}

static inline int ts_fw_update_sd(struct ts_cmd_node *in_cmd, struct ts_cmd_node *out_cmd)
{
	int error = NO_ERR;
	struct ts_device_data *dev = g_ts_data.chip_data;

	TS_LOG_INFO("process firmware update sd\n");

	if (dev->ops->chip_fw_update_sd)
		error = dev->ops->chip_fw_update_sd();

	return error;
}

static inline int ts_calibrate(struct ts_cmd_node *in_cmd, struct ts_cmd_node *out_cmd)
{
	int error = NO_ERR;
	struct ts_device_data *dev = g_ts_data.chip_data;
	struct ts_calibrate_info *info = (struct ts_calibrate_info *)in_cmd->cmd_param.prv_params;

	TS_LOG_DEBUG("process firmware calibrate\n");

	if (dev->ops->chip_calibrate)
		error = dev->ops->chip_calibrate();

	if (error)
		info->status = TS_ACTION_FAILED;
	else
		info->status = TS_ACTION_SUCCESS;

	return error;
}

static int ts_get_chip_info(struct ts_cmd_node *in_cmd, struct ts_cmd_node *out_cmd)
{
	int error = NO_ERR;
	struct ts_device_data *dev = g_ts_data.chip_data;
	struct ts_chip_info_param *info = (struct ts_chip_info_param *)in_cmd->cmd_param.prv_params;

	TS_LOG_INFO("get chip info called\n");

	if (dev->ops->chip_get_info)
		error = dev->ops->chip_get_info(info);

	if (error)
		info->status = TS_ACTION_FAILED;
	else
		info->status = TS_ACTION_SUCCESS;

	return error;
}

static inline int ts_force_reset(struct ts_cmd_node *in_cmd, struct ts_cmd_node *out_cmd)
{
	int error = NO_ERR;
	struct ts_device_data *dev = g_ts_data.chip_data;

	TS_LOG_INFO("ts force reset called\n");

	if (dev->ops->chip_reset)
		error = dev->ops->chip_reset();

	if (error) {
		out_cmd->command = TS_ERR_OCCUR;
		goto out;
	}

out:
	return error;
}

static inline int ts_read_rawdata(struct ts_cmd_node *in_cmd, struct ts_cmd_node *out_cmd)
{
	int error = NO_ERR;
	struct ts_device_data *dev = g_ts_data.chip_data;
	struct ts_rawdata_info *info = (struct ts_rawdata_info *)in_cmd->cmd_param.prv_params;

	TS_LOG_INFO("ts read rawdata called\n");

	if (dev->ops->chip_get_rawdata)
		error = dev->ops->chip_get_rawdata(info, out_cmd);

	if (!error) {
		TS_LOG_INFO("read rawdata success\n");
		info->status = TS_ACTION_SUCCESS;
		info->time_stamp = ktime_get();
		goto out;
	}

	info->status = TS_ACTION_FAILED;
	TS_LOG_ERR("read rawdata failed :%d\n", error);

out:
	return error;
}

static inline int ts_glove_switch(struct ts_cmd_node *in_cmd, struct ts_cmd_node *out_cmd)
{
	int error = -EIO;
	struct ts_device_data *dev = g_ts_data.chip_data;
	struct ts_glove_info *info = (struct ts_glove_info *)in_cmd->cmd_param.prv_params;

	TS_LOG_DEBUG("glove action :%d, value:%d", info->op_action, info->glove_switch);

	if (dev->ops->chip_glove_switch)
		error = dev->ops->chip_glove_switch(info);

	if (error)
		info->status = TS_ACTION_FAILED;
	else
		info->status = TS_ACTION_SUCCESS;

	TS_LOG_DEBUG("glove switch process result: %d\n", error);

	return error;
}

static inline int ts_holster_switch(struct ts_cmd_node *in_cmd, struct ts_cmd_node *out_cmd)
{
	int error = -EIO;
	struct ts_device_data *dev = g_ts_data.chip_data;
	struct ts_holster_info *info = (struct ts_holster_info *)in_cmd->cmd_param.prv_params;

	if (dev->ops->chip_holster_switch)
		error = dev->ops->chip_holster_switch(info);

	if (error) {
		info->status = TS_ACTION_FAILED;
		TS_LOG_ERR("holster switch process error: %d\n", error);
	} else
		info->status = TS_ACTION_SUCCESS;

	return error;
}

static inline int ts_touch_window(struct ts_cmd_node *in_cmd, struct ts_cmd_node *out_cmd)
{
	struct ts_window_info *info = (struct ts_window_info *)in_cmd->cmd_param.prv_params;

	g_ts_data.feature_info.window_info.window_enable = info->window_enable;
	g_ts_data.feature_info.window_info.top_left_x0 = info->top_left_x0;
	g_ts_data.feature_info.window_info.top_left_y0 = info->top_left_y0;
	g_ts_data.feature_info.window_info.bottom_right_x1 = info->bottom_right_x1;
	g_ts_data.feature_info.window_info.bottom_right_y1 = info->bottom_right_y1;

	info->status = TS_ACTION_SUCCESS;

	return NO_ERR;
}

static inline int ts_hand_detect(struct ts_cmd_node *in_cmd, struct ts_cmd_node *out_cmd)
{
	int error = -EIO;
	struct ts_device_data *dev = g_ts_data.chip_data;
	struct ts_hand_info *info = (struct ts_hand_info *)in_cmd->cmd_param.prv_params;

	if (dev->ops->chip_hand_detect)
		error = dev->ops->chip_hand_detect(info);

	if (error)
		info->status = TS_ACTION_FAILED;
	else
		info->status = TS_ACTION_SUCCESS;

	return error;
}

static inline int ts_err_process(struct ts_cmd_node *in_cmd, struct ts_cmd_node *out_cmd)
{
	static int error_count = 0;
	int error = NO_ERR;
	struct ts_device_data *dev = g_ts_data.chip_data;

	TS_LOG_INFO("error process\n");

	if (dev->ops->chip_reset)
		error = dev->ops->chip_reset();
	if (error) {//error nest occurred, we define nest level
		out_cmd->command = TS_ERR_OCCUR;
		//BUG_ON(unlikely(++error_count == TS_ERR_NEST_LEVEL));
		goto out;
	}

	error_count = 0;
out:
	return error;
}

static inline int ts_int_err_process(struct ts_cmd_node *in_cmd, struct ts_cmd_node *out_cmd)
{
	int error = NO_ERR;
	struct ts_device_data *dev = g_ts_data.chip_data;

	if (dev->ops->chip_reset)
		error = dev->ops->chip_reset();

	if (error) {//error nest occurred, we define nest level
		out_cmd->command = TS_ERR_OCCUR;
		goto out;
	}

out:
	return error;
}

static inline int ts_test_cmd(struct ts_cmd_node *in_cmd, struct ts_cmd_node *out_cmd)
{
	int error = NO_ERR;
	struct ts_device_data *dev = g_ts_data.chip_data;

	if (dev->ops->chip_test)
		error = dev->ops->chip_test(in_cmd, out_cmd);

	if (error) {
		out_cmd->command = TS_ERR_OCCUR;
		goto out;
	}

out:
	return error;
}

static inline int ts_check_status(struct ts_cmd_node *in_cmd, struct ts_cmd_node *out_cmd)
{
	int error = NO_ERR;
	struct ts_device_data *dev = g_ts_data.chip_data;

	if (dev->ops->chip_check_status)
		error = dev->ops->chip_check_status();
	if (error) {
		out_cmd->command = TS_ERR_OCCUR;
	}

	ts_start_wd_timer(&g_ts_data);
	return error;
}

static inline bool ts_cmd_need_process(struct ts_cmd_node *cmd)
{
	bool is_need_process = true;
	struct ts_cmd_sync *sync = cmd->sync;
	enum ts_pm_type pm_type = cmd->cmd_param.pub_params.pm_type;

	if (unlikely(atomic_read(&g_ts_data.state) == TS_SLEEP)) {
		switch(cmd->command) {
		case TS_POWER_CONTROL:
			if ((pm_type != TS_RESUME_DEVICE) && (pm_type != TS_AFTER_RESUME))
				is_need_process = false;
			break;
		case TS_TOUCH_WINDOW:
			is_need_process = true;
			break;
		case TS_INT_PROCESS:
		case TS_INT_ERR_OCCUR:
			enable_irq(g_ts_data.irq_id);
			is_need_process = false;
			break;
		default:
			is_need_process = false;
			break;
		}
	}

	if(!is_need_process && sync) {
		if (atomic_read(&sync->timeout_flag) == TS_TIMEOUT) {
			kfree(sync);
		} else {
			complete(&sync->done);
		}
	}

	return is_need_process;
}
static int ts_proc_command(struct ts_cmd_node *cmd)
{
	int error = NO_ERR;
	struct ts_cmd_sync *sync = cmd->sync;
	struct ts_cmd_node *proc_cmd = cmd;
	struct ts_cmd_node *out_cmd = &pang_cmd_buff;

	if (!ts_cmd_need_process(proc_cmd)) {
		TS_LOG_INFO("no need to process cmd:%d", proc_cmd->command);
		goto out;
	}

related_proc:
	out_cmd->command = TS_INVAILD_CMD;

	switch(proc_cmd->command) {
		case TS_INT_PROCESS:
			ts_proc_bottom_half(proc_cmd, out_cmd);
			enable_irq(g_ts_data.irq_id);
			break;
		case TS_INPUT_ALGO:
			ts_algo_calibrate(proc_cmd, out_cmd);
			break;
		case TS_REPORT_INPUT:
			ts_report_input(proc_cmd, out_cmd);
			break;
		case TS_POWER_CONTROL:
			ts_power_control(g_ts_data.irq_id, proc_cmd, out_cmd);
			break;
		case TS_FW_UPDATE_BOOT:
			disable_irq(g_ts_data.irq_id);
			ts_fw_update_boot(proc_cmd, out_cmd);
			enable_irq(g_ts_data.irq_id);
			break;
		case TS_FW_UPDATE_SD:
			disable_irq(g_ts_data.irq_id);
			ts_fw_update_sd(proc_cmd, out_cmd);
			enable_irq(g_ts_data.irq_id);
			break;
		case TS_READ_RAW_DATA:
			disable_irq(g_ts_data.irq_id);
			ts_read_rawdata(proc_cmd, out_cmd);
			enable_irq(g_ts_data.irq_id);
			break;
		case TS_GET_CHIP_INFO:
			ts_get_chip_info(proc_cmd, out_cmd);
			break;
		case TS_CALIBRATE_DEVICE:
			ts_calibrate(proc_cmd, out_cmd);
			break;
		case TS_GLOVE_SWITCH:
			ts_glove_switch(proc_cmd, out_cmd);
			break;
		case TS_HAND_DETECT:
			ts_hand_detect(proc_cmd, out_cmd);
			break;
		case TS_FORCE_RESET:
			disable_irq(g_ts_data.irq_id);
			ts_stop_wd_timer(&g_ts_data);
			ts_force_reset(proc_cmd, out_cmd);
			ts_start_wd_timer(&g_ts_data);
			enable_irq(g_ts_data.irq_id);
			break;
		case TS_INT_ERR_OCCUR:
			ts_stop_wd_timer(&g_ts_data);
			ts_int_err_process(proc_cmd, out_cmd);
			enable_irq(g_ts_data.irq_id);
			ts_start_wd_timer(&g_ts_data);
			break;
		case TS_ERR_OCCUR:
			disable_irq(g_ts_data.irq_id);
			ts_stop_wd_timer(&g_ts_data);
			ts_err_process(proc_cmd, out_cmd);
			ts_start_wd_timer(&g_ts_data);
			enable_irq(g_ts_data.irq_id);
			break;
		case TS_CHECK_STATUS:
			ts_check_status(proc_cmd, out_cmd);
			break;
		case TS_HOLSTER_SWITCH:
			ts_holster_switch(proc_cmd, out_cmd);
			break;
		case TS_TOUCH_WINDOW:
			ts_touch_window(proc_cmd, out_cmd);
			break;
		case TS_TEST_CMD:
			ts_test_cmd(proc_cmd, out_cmd);
		default:
			break;
	}

	TS_LOG_DEBUG("command :%d process result:%d \n", proc_cmd->command, error);

	if (out_cmd->command != TS_INVAILD_CMD) {
		TS_LOG_DEBUG("related command :%d  need process\n", out_cmd->command);
		swap(proc_cmd, out_cmd);//ping - pang
		goto related_proc;
	}

	if (sync) {//notify wait threads by completion
		smp_mb();
		TS_LOG_DEBUG("wakeup threads in waitqueue\n");
		if (atomic_read(&sync->timeout_flag) == TS_TIMEOUT) {
			kfree(sync);
		} else {
			complete(&sync->done);
		}
	}

out:
	return error;
}

static bool ts_task_continue(void)
{
	bool task_continue = true;
	unsigned long flags;
	TS_LOG_DEBUG("prepare enter idle\n");

repeat:
	if (unlikely(kthread_should_stop())) {
		task_continue = false;
		goto out;
	}
	spin_lock_irqsave(&g_ts_data.queue.spin_lock, flags);
	smp_wmb();
	if (g_ts_data.queue.cmd_count) {
		set_current_state(TASK_RUNNING);
		TS_LOG_DEBUG("ts task state to  TASK_RUNNING\n");
		goto out_unlock;
	} else {
		set_current_state(TASK_INTERRUPTIBLE);
		TS_LOG_DEBUG("ts task state to  TASK_INTERRUPTIBLE\n");
		spin_unlock_irqrestore(&g_ts_data.queue.spin_lock, flags);
		schedule();
		goto repeat;
	}

out_unlock:
	spin_unlock_irqrestore(&g_ts_data.queue.spin_lock, flags);
out:
	return task_continue;
}

static int ts_input_open(struct input_dev *dev)
{

	TS_LOG_DEBUG("input_open called:%d\n", dev->users);
	return NO_ERR;
}

static void ts_input_close(struct input_dev *dev)
{
	TS_LOG_DEBUG("input_close called:%d\n", dev->users);
}

#ifdef CONFIG_OF //dts
static int chip_detect(const char *chip_name)
{
	bool found = false;
	int index;
	int error = -EIO;
	int len;
	const __be32 *addr;
	struct device_node *child= NULL;
	struct device_node *root = g_ts_data.node;
	struct ts_device_data *ptr = &g_ts_device_map[0];

	for_each_child_of_node(root, child) {//find the chip node
		if (of_device_is_compatible(child, chip_name)) {
			found = true;
			break;
		}
	}

	if (!found) {
		TS_LOG_ERR("not find chip :%s's node\n", chip_name);
		goto out;
	}

	if (g_ts_data.bops->btype == TS_BUS_I2C) {//i2c ts need get chip addr
		addr = of_get_property(child, "slave_address", &len);
		if (!addr || (len < sizeof(int))) {
			TS_LOG_ERR("invalid slave_address on %s, len:%d\n", chip_name, len);
			goto out;
		}
		if (g_ts_data.client->addr != be32_to_cpup(addr)) {
			error = i2c_check_addr_busy(g_ts_data.client->adapter, be32_to_cpup(addr));
			if (error) {
				TS_LOG_ERR("0x%x slave addr conflict\n", *addr);
				goto out;
			}
			TS_LOG_DEBUG("slave addr :0x%x not occupied\n", *addr);
			g_ts_data.client->addr = be32_to_cpup(addr);
		}
	}

	for(index = 0; ptr!= NULL && index< ARRAY_SIZE(g_ts_device_map); ptr++, index++)
	{
		if (!strcmp (chip_name, ptr->chip_name)) {
			if (g_ts_data.bops->btype == TS_BUS_I2C)
				ptr->slave_addr = g_ts_data.client->addr;

			ptr->bops = g_ts_data.bops;
			INIT_LIST_HEAD(&ptr->algo_head);
			g_ts_data.chip_data = ptr;

			//have parse dt func , execute it now
			if (ptr->ops->chip_parse_config && ptr->ops->chip_parse_config(child, ptr)) {
				TS_LOG_ERR("call %s's chip self parse config failed\n", chip_name);
				error = -EINVAL;
				goto out;
			}

			if (ptr->ops->chip_detect) {
				TS_LOG_DEBUG("call chip self init handle\n");
				error = ptr->ops->chip_detect(child, ptr, g_ts_data.ts_dev);
			}
			goto out;
		}
	}
	if (ARRAY_SIZE(g_ts_device_map) == index) {
		TS_LOG_ERR("we can not find %s's chip data in device map\n", chip_name);
		error = -EINVAL;
	}
out:
	return error;
}

static int get_support_chip(void)
{
	struct device_node *np = g_ts_data.node;
	const char *support_chip_name = NULL;
	//const char *bus_type;
	int rc = 0;
	int index = 0;
	int found = -ENXIO;
	int array_len = 0;

	if (np) {//support dts
		array_len = of_property_count_strings(np, "support_chip_name");
		if (array_len <= 0) {
			TS_LOG_ERR("chip name length invaild:%d\n", array_len);
			return -EINVAL;
		}

		for(index=0; index<array_len; index++)//try to detect active ts ic
		{
			rc = of_property_read_string_index(np,
				"support_chip_name", index,
				&support_chip_name);
			if (rc) {
				TS_LOG_ERR("read %d - chip name :%s, err:%d\n", index, support_chip_name, rc);
				continue;
			}

			if (!chip_detect(support_chip_name)) {
				found = NO_ERR;
				TS_LOG_INFO("chip: %s found success\n", support_chip_name);
				break;
			}
		}
	} else {//not support dts
		TS_LOG_ERR("dts feature not support: %d\n", rc);
	}
	return found;
}

static int ts_parse_config(void)
{
	int error = NO_ERR;
	int rc;
	int index;
	char *tmp_buff = NULL;

	if (g_ts_data.node) {
		rc = of_property_read_string(g_ts_data.node, "product", (const char **)&tmp_buff);
		if (rc) {
			TS_LOG_ERR("product read failed:%d\n", rc);
			error = -EINVAL;
			goto out;
		}
	}

	if (!tmp_buff) {
		TS_LOG_ERR("tmp_buff failed\n");
		error = -EINVAL;
		goto out;
	}

	for(index = 0; tmp_buff[index] && index < strlen(tmp_buff); index++)//exchange name to lower
		g_ts_data.product_name[index] = tolower(tmp_buff[index]);

	if (index == MAX_STR_LEN)
		g_ts_data.product_name[MAX_STR_LEN-1] = '\0';

	TS_LOG_INFO("parse product name :%s\n", g_ts_data.product_name);

out:
	return error;
}

static int get_ts_bus_info(void)
{
	const char *bus_type;

	int rc;
	int error = NO_ERR;
	u32 bus_id = 0;;

	g_ts_data.node = NULL;

	g_ts_data.node = of_find_compatible_node(NULL, NULL, TS_DEV_NAME);
	if (!g_ts_data.node) {
		TS_LOG_ERR("can't find ts module node\n");
		error = -EINVAL;
		goto out;
	}

	rc = of_property_read_string(g_ts_data.node, "bus_type", &bus_type);
	if (rc) {
		TS_LOG_ERR("bus type read failed:%d\n", rc);
		error = -EINVAL;
		goto out;
	}
	if (!strcmp (bus_type,"i2c")) {
		g_ts_data.bops = &ts_bus_i2c_info;
	} else if (!strcmp (bus_type,"spi")) {
		g_ts_data.bops = &ts_bus_spi_info;
	} else {
		TS_LOG_ERR("bus type invaild:%s\n", bus_type);
		error = -EINVAL;
	}

	rc = of_property_read_u32(g_ts_data.node, "bus_id", &bus_id);
	if (rc) {
		TS_LOG_ERR("bus id read failed\n");
		error = -EINVAL;
		goto out;
	}
	g_ts_data.bops->bus_id = bus_id;
	TS_LOG_DEBUG("bus id :%d\n", bus_id);

out:
	return error;
}

#else //board id

static int chip_detect(const char *chip_name)
{
	bool rc = false;
	int index;
	int addr;
	int error = -EINVAL;
	char *chip_slave_addr = NULL;
	char *chip_slave_ptr = NULL;
	struct ts_device_data *ptr = &g_ts_device_map[0];

	if (g_ts_data.bops->btype == TS_BUS_I2C) {
		chip_slave_addr = kzalloc(FULL_NAME_MAX_LEN, GFP_KERNEL);
		if (!chip_slave_addr) {
			TS_LOG_ERR("memory alloc failed\n");
			error = -ENOMEM;
			goto out;
		}

		strncpy(chip_slave_addr, chip_name, FULL_NAME_MAX_LEN);
		chip_slave_ptr = chip_slave_addr + strlen(chip_name);
		strncpy(chip_slave_ptr, CHIP_SLAVE_ADDR_NAME, FULL_NAME_MAX_LEN-strlen(chip_name));

		rc = get_hw_config_int(chip_slave_addr, &addr, NULL);
		if (!rc) {
			TS_LOG_ERR("%s slave addr read failed\n", chip_slave_addr);
			goto mem_out;
		}

		if (g_ts_data.client->addr != addr) {
			error = i2c_check_addr_busy(g_ts_data.client->adapter, addr);
			if (error)
			{
				TS_LOG_ERR("%s slave addr conflict\n", chip_slave_addr);
				goto mem_out;
			}
			g_ts_data.client->addr = (unsigned short)addr;
			TS_LOG_DEBUG("slave addr :0x%x not occupied\n", g_ts_data.client->addr);
		}
	}

	for(index = 0; ptr!= NULL && index< ARRAY_SIZE(g_ts_device_map); ptr++, index++)
	{
		if (!strcmp (chip_name, ptr->chip_name)) {
			if (g_ts_data.bops->btype == TS_BUS_I2C)
				ptr->slave_addr = g_ts_data.client->addr;

			ptr->bops = g_ts_data.bops;
			ptr->client = g_ts_data.client;	//client need to be used  modified by mengkun
			INIT_LIST_HEAD(&ptr->algo_head);
			g_ts_data.chip_data = ptr;

			if (ptr->ops->chip_parse_config && ptr->ops->chip_parse_config(NULL, ptr)) {
				TS_LOG_ERR("call %s's chip self parse failed\n", chip_name);
				error = -EINVAL;
				goto mem_out;
			}

			if (ptr->ops->chip_detect) {
				TS_LOG_DEBUG("call chip self init\n");
				error = ptr->ops->chip_detect(NULL, ptr, g_ts_data.ts_dev);
			}
			goto mem_out;
		}
	}
	if (ARRAY_SIZE(g_ts_device_map) == index) {
		TS_LOG_ERR("we can not find %s's chip data in device map\n", chip_name);
		error = -EINVAL;
	}

mem_out:
	kfree(chip_slave_addr);
out:
	return error;
}

static int get_support_chip(void)
{
	bool rc = false;
	int found = -ENXIO;
	int index;
	char *chip_id_ptr = NULL;
	char *chip_full_name = NULL;
	char tmp_buff[MAX_STR_LEN];

	chip_full_name = kzalloc(FULL_NAME_MAX_LEN, GFP_KERNEL);
	if (!chip_full_name) {
		TS_LOG_ERR("memory alloc failed\n");
		found = -ENOMEM;
		goto out;
	}

	strncpy(chip_full_name, CHIP_FULL_NAME, FULL_NAME_MAX_LEN);
	chip_id_ptr = chip_full_name + strlen(CHIP_FULL_NAME);

	for(index=0; index<SUPPORT_IC_NUM; index++)//try to detect active ts ic
	{
		*chip_id_ptr = index+INT_CONVERT_OFFSET;

		memset(tmp_buff, 0, MAX_STR_LEN);
		rc = get_hw_config_string(chip_full_name, tmp_buff, MAX_STR_LEN, NULL);
		if (!rc) {
			TS_LOG_DEBUG("get %s failed, try next device\n", chip_full_name);
			continue;
		}

		if (!strcmp(tmp_buff, IC_NAME_UNDEF)) {
			TS_LOG_DEBUG("skip device :null\n");
			continue;
		}

		if (!chip_detect(tmp_buff))
		{
			found = NO_ERR;
			TS_LOG_INFO("chip: %s found success\n", tmp_buff);
			break;
		}
	}

	kfree(chip_full_name);

out:
	return found;
}

static int ts_parse_config(void)
{
	bool rc = false;
	int index;
	int error = NO_ERR;
	char tmp_buff[MAX_STR_LEN];

	memset(tmp_buff, 0, MAX_STR_LEN);
	rc = get_hw_config_string(TS_PRODUCT, tmp_buff, MAX_STR_LEN, NULL);
	if (!rc) {
		TS_LOG_ERR("product name read failed\n");
		error = -EINVAL;
		goto out;
	}

	for(index = 0; tmp_buff[index] && index < MAX_STR_LEN; index++)//exchange name to lower
		g_ts_data.product_name[index] = tolower(tmp_buff[index]);

	if (index == MAX_STR_LEN)
		g_ts_data.product_name[MAX_STR_LEN-1] = '\0';

	TS_LOG_INFO("parse product name :%s", g_ts_data.product_name);

out:
	return error;
}

static int get_ts_bus_info(void)
{
	bool rc = false;
	int error = NO_ERR;
	int bus_id;
	char tmp_buff[MAX_STR_LEN];

	memset(tmp_buff, 0, MAX_STR_LEN);
	rc = get_hw_config_string(TS_BUS_TYPE, tmp_buff, MAX_STR_LEN, NULL);
	if (!rc) {
		TS_LOG_ERR("bus type read failed\n");
		error = -EINVAL;
		goto out;
	}
	if (!strcmp (tmp_buff, "i2c")) {
		g_ts_data.bops = &ts_bus_i2c_info;
	} else if (!strcmp (tmp_buff, "spi")) {
		g_ts_data.bops = &ts_bus_spi_info;
	} else {
		TS_LOG_ERR("bus_type :%s invaild\n", tmp_buff);
		error = -EINVAL;
		goto out;
	}

	rc = get_hw_config_int(TS_BUS_ID, &bus_id, NULL);
	if (!rc) {
		TS_LOG_ERR("bus id read failed\n");
		error = -EINVAL;
		goto out;
	}
	if (bus_id<0) {
		TS_LOG_ERR("bus id :%d invaild\n", bus_id);
		error = -EINVAL;
		goto out;
	} else {
		g_ts_data.bops->bus_id = bus_id;
		TS_LOG_DEBUG("bus id :%d stored\n", bus_id);
	}

out:
	return error;
}

#endif

static int ts_chip_init(void)
{
	int error = NO_ERR;
	struct ts_device_data *dev = g_ts_data.chip_data;

	TS_LOG_INFO("ts_chip_init called\n");

	if (dev->ops->chip_init)
		error = dev->ops->chip_init();
	if (error) {
		TS_LOG_ERR("chip init failed\n");
	}

	return error;
}

static int ts_register_algo(void)
{
	int error = NO_ERR;
	struct ts_device_data *dev = g_ts_data.chip_data;

	TS_LOG_INFO("register algo called\n");

	if (dev->ops->chip_register_algo)
		error =  dev->ops->chip_register_algo(dev);

	return error;
}

static void ts_ic_shutdown(void)
{
	struct ts_device_data *dev = g_ts_data.chip_data;
	if (dev->ops->chip_shutdown)
		dev->ops->chip_shutdown();
	return;
}

static int try_update_firmware(void)
{
	char joint_chr = '_';
	int error = NO_ERR;
	char *fw_name;
	struct ts_cmd_node cmd;

	memset(&cmd, 0, sizeof(struct ts_cmd_node));
	cmd.command = TS_FW_UPDATE_BOOT;
	fw_name = cmd.cmd_param.pub_params.firmware_info.fw_name;

	/*firmware name [product_name][ic_name][module][vendor]*/
	strncat(fw_name, g_ts_data.product_name, MAX_STR_LEN);
	strncat(fw_name, &joint_chr, 1);
	strncat(fw_name, g_ts_data.chip_data->chip_name, MAX_STR_LEN);
#if 0
	strncat(fw_name, &joint_chr, 1);
	strncat(fw_name, g_ts_data.chip_data->module_name, MAX_STR_LEN);
	strncat(fw_name, &joint_chr, 1);
	strncat(fw_name, g_ts_data.chip_data->version_name, MAX_STR_LEN);
#endif

	TS_LOG_INFO("fw file name is :%s\n", fw_name);

	error = put_one_cmd(&cmd, NO_SYNC_TIMEOUT);

	return error;
}

static int ts_init(void)
{
	int error = NO_ERR;
	unsigned long irq_flags;
	struct input_dev *input_dev;

	atomic_set(&g_ts_data.state, TS_UNINIT);

	g_ts_data.queue.rd_index = 0;
	g_ts_data.queue.wr_index = 0;
	g_ts_data.queue.cmd_count = 0;
	g_ts_data.queue.queue_size = TS_CMD_QUEUE_SIZE;
	spin_lock_init(&g_ts_data.queue.spin_lock);
	TS_LOG_DEBUG("ts init: cmd queue size : %d\n", TS_CMD_QUEUE_SIZE);

	error = ts_parse_config();
	if (error) {
		TS_LOG_ERR("ts parse config failed : %d\n", error);
		goto err_out;
	}

	g_ts_data.ts_dev = platform_device_alloc("huawei_touch", -1);
	if (!g_ts_data.ts_dev) {
		TS_LOG_ERR("platform device malloc failed\n");
		error = -ENOMEM;
		goto err_out;
	}

	error = platform_device_add(g_ts_data.ts_dev);
	if (error){
		TS_LOG_ERR("platform device add failed :%d\n", error);
		goto err_put_platform_dev;
	}

	error = get_support_chip();
	if (error) {
		TS_LOG_ERR("get support chip failed : %d\n", error);
		goto err_del_platform_dev;
	}

	error = ts_chip_init();
	if (error) {
		TS_LOG_ERR("chip init failed : %d,  try fw update again\n", error);
	}

	error = ts_register_algo();
	if (error) {
		TS_LOG_ERR("ts register algo failed : %d\n", error);
		goto err_del_platform_dev;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		TS_LOG_ERR("failed to allocate memory for input dev\n");
		error = -ENOMEM;
		goto err_del_platform_dev;
	}

	input_dev->name = TS_DEV_NAME;
	if (g_ts_data.bops->btype == TS_BUS_I2C)
		input_dev->id.bustype = BUS_I2C;
	else if (g_ts_data.bops->btype == TS_BUS_SPI)
		input_dev->id.bustype = BUS_SPI;
	input_dev->dev.parent = &g_ts_data.ts_dev->dev;
	input_dev->open = ts_input_open;
	input_dev->close = ts_input_close;
	g_ts_data.input_dev = input_dev;

	if (g_ts_data.chip_data->ops->chip_input_config) //config input for diff chip
		error = g_ts_data.chip_data->ops->chip_input_config(g_ts_data.input_dev);
	if (error)
		goto err_free_dev;

	input_set_drvdata(input_dev, &g_ts_data);

	error = input_register_device(input_dev);
	if (error) {
		TS_LOG_ERR("input dev register failed : %d\n", error);
		goto err_free_dev;
	}

#if defined(CONFIG_FB)
	g_ts_data.fb_notify.notifier_call = fb_notifier_callback;
	error = fb_register_client(&g_ts_data.fb_notify);
	if (error){
		TS_LOG_ERR("unable to register fb_notifier: %d\n", error);
		goto err_free_input_dev;
	}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	g_ts_data.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + TS_SUSPEND_LEVEL;
	g_ts_data.early_suspend.suspend = ts_early_suspend;
	g_ts_data.early_suspend.resume = ts_late_resume;
	register_early_suspend(&g_ts_data.early_suspend);
#endif

	error = sysfs_create_group(&g_ts_data.ts_dev->dev.kobj, &ts_attr_group);
	if (error) {
		TS_LOG_ERR("can't create ts's sysfs\n");
		goto err_unregister_suspend;
	}

	procfs_create();

	g_ts_data.irq_id= g_ts_data.client->irq = gpio_to_irq(g_ts_data.chip_data->irq_gpio);
	switch (g_ts_data.chip_data->irq_config) {
		case TS_IRQ_LOW_LEVEL:
			irq_flags = IRQF_TRIGGER_LOW;
			break;
		case TS_IRQ_HIGH_LEVEL:
			irq_flags = IRQF_TRIGGER_HIGH;
			break;
		case TS_IRQ_RAISE_EDGE:
			irq_flags = IRQF_TRIGGER_RISING;
			break;
		case TS_IRQ_FALL_EDGE:
			irq_flags = IRQF_TRIGGER_FALLING;
			break;
		default:
			TS_LOG_ERR("ts irq_config invaild\n");
			goto err_free_sysfs;
	}

	atomic_set(&g_ts_data.state, TS_WORK);//avoid 1st irq unable to handler

	error = request_irq(g_ts_data.irq_id, ts_irq_handler, irq_flags, "ts", &g_ts_data);
	if (error) {
		TS_LOG_ERR("ts request_irq failed\n");
		goto err_free_sysfs;
	}

	error = try_update_firmware();
	if (error) {
		TS_LOG_ERR("return fail : %d\n", error);
		goto err_firmware_update;
	}

	INIT_WORK(&(g_ts_data.watchdog_work), ts_watchdog_work);
	setup_timer(&(g_ts_data.watchdog_timer),ts_watchdog_timer,(unsigned long)(&g_ts_data));
	ts_start_wd_timer(&g_ts_data);

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
	/* detect current device successful, set the flag as present */
	set_hw_dev_flag(DEV_I2C_TOUCH_PANEL);
#endif
	TS_LOG_INFO("ts_init called out\n");
	goto out;

err_firmware_update:
	free_irq(g_ts_data.irq_id, &g_ts_data);
err_free_sysfs:
	sysfs_remove_group(&g_ts_data.ts_dev->dev.kobj, &ts_attr_group);
err_unregister_suspend:
#if defined(CONFIG_FB)
	if (fb_unregister_client(&g_ts_data.fb_notify))
		TS_LOG_ERR("error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&g_ts_data.early_suspend);
#endif
err_free_input_dev:
	input_unregister_device(input_dev);
err_free_dev:
	input_free_device(input_dev);
err_del_platform_dev:
	platform_device_del(g_ts_data.ts_dev);
err_put_platform_dev:
	platform_device_put(g_ts_data.ts_dev);
err_out:
	atomic_set(&g_ts_data.state, TS_UNINIT);
out:
	TS_LOG_INFO("ts_init, g_ts_data.state : %d\n", atomic_read(&g_ts_data.state));
	return error;
}

static int ts_creat_i2c_client(void)
{
	struct i2c_adapter *adapter = NULL;
	struct i2c_client *client = NULL;
	struct i2c_board_info board_info;

	memset(&board_info, 0, sizeof(struct i2c_board_info));
	strncpy(board_info.type, TS_DEV_NAME, I2C_NAME_SIZE);
	board_info.addr = I2C_DEFAULT_ADDR;
	board_info.flags = true;

	adapter = i2c_get_adapter(g_ts_data.bops->bus_id);
	if (!adapter) {
		TS_LOG_ERR("i2c_get_adapter failed\n");
		return -EIO;
	}

	client = i2c_new_device(adapter, &board_info);
	if (!client) {
		TS_LOG_ERR("i2c_new_device failed\n");
		return -EIO;
	}
	g_ts_data.client = client;
	i2c_set_clientdata(client, &g_ts_data);

	LDO17_status = (u8 *)ioremap(LDO17_PHYS_ADDR, DATA_SIZE);
	LSW50_status = (u8 *)ioremap(LSW50_PHYS_ADDR, DATA_SIZE);

	if (!tp_dclient) {
		tp_dclient = dsm_register_client(&dsm_tp);
	}

	return NO_ERR;
}

static int ts_destory_i2c_client(void)
{
	TS_LOG_ERR("destory i2c device\n");
	i2c_unregister_device(g_ts_data.client);
	return NO_ERR;
}

static int ts_create_client(void)
{
	int error = -EINVAL;

	switch(g_ts_data.bops->btype) {
		case TS_BUS_I2C:
			TS_LOG_DEBUG("create ts's i2c device\n");
			error = ts_creat_i2c_client();
			break;
		case TS_BUS_SPI:
			TS_LOG_DEBUG("create ts's spi device\n");
			break;
		default:
			TS_LOG_ERR("unknown ts's device\n");
			break;
	}

	return error;
}

static int ts_destory_client(void)
{
	TS_LOG_ERR("destory touchscreen device\n");

	switch(g_ts_data.bops->btype) {
		case TS_BUS_I2C:
			TS_LOG_DEBUG("destory ts's i2c device\n");
			ts_destory_i2c_client();
			break;
		case TS_BUS_SPI:
			TS_LOG_DEBUG("destory ts's spi device\n");
			break;
		default:
			TS_LOG_ERR("unknown ts's device\n");
			break;
	}
	return NO_ERR;
}

static int ts_thread(void *p)
{
	static const struct sched_param param = {
		.sched_priority = MAX_USER_RT_PRIO/2,
	};
	smp_wmb();
	if (ts_init()) {
		TS_LOG_ERR("ts_init  failed\n");
		goto out;
	}
	memset(&ping_cmd_buff, 0, sizeof(struct ts_cmd_node));
	memset(&pang_cmd_buff, 0, sizeof(struct ts_cmd_node));
	smp_mb();
	sched_setscheduler(current, SCHED_FIFO, &param);

	while(ts_task_continue())
	{
		while(!get_one_cmd(&ping_cmd_buff))//get one command
		{
			ts_proc_command(&ping_cmd_buff);
			memset(&ping_cmd_buff, 0, sizeof(struct ts_cmd_node));
			memset(&pang_cmd_buff, 0, sizeof(struct ts_cmd_node));
		}
	}

	TS_LOG_ERR("ts thread stop\n");
	atomic_set(&g_ts_data.state, TS_UNINIT);
	disable_irq(g_ts_data.irq_id);
	ts_ic_shutdown();
	free_irq(g_ts_data.irq_id, &g_ts_data);
	sysfs_remove_group(&g_ts_data.ts_dev->dev.kobj, &ts_attr_group);
#if defined(CONFIG_FB)
	if (fb_unregister_client(&g_ts_data.fb_notify))
		TS_LOG_ERR("error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&g_ts_data.early_suspend);
#endif
	input_unregister_device(g_ts_data.input_dev);
	input_free_device(g_ts_data.input_dev);
	platform_device_unregister(g_ts_data.ts_dev);
out:
	ts_destory_client();
	memset(&g_ts_data, 0, sizeof(struct ts_data));
	TS_LOG_ERR("ts_thread exited\n");
	return NO_ERR;
}

static int __init ts_module_init(void)
{
	int error = NO_ERR;

	memset(&g_ts_data, 0, sizeof(struct ts_data));

	g_ts_data.dev_id = 0;

	error = get_ts_bus_info();
	if (error) {
		TS_LOG_ERR("get bus info failed :%d\n", error);
		memset(&g_ts_data, 0, sizeof(struct ts_data));
		goto out;
	}

	error = ts_create_client();
	if (error) {
		TS_LOG_ERR("create device failed :%d\n", error);
		goto out;
	}

	g_ts_data.ts_task = kthread_create(ts_thread, &g_ts_data, "ts_thread:%d", g_ts_data.dev_id);
	if (IS_ERR(g_ts_data.ts_task)) {
		TS_LOG_ERR("create ts_thread failed\n");
		ts_destory_client();
		memset(&g_ts_data, 0, sizeof(struct ts_data));
		error = -EINVAL;
		goto out;
	}
	/* Attention about smp_mb/rmb/wmb
	     Add these driver to avoid  data consistency problem
	     ts_thread/ts_probe/irq_handler/put_one_cmd/get_one_cmd
	     may run in different cpus and L1/L2 cache data consistency need
	     to conside. We use barrier to make sure data consistently
	*/
	smp_mb();
	wake_up_process(g_ts_data.ts_task);

out:
	return error;
}

static void __exit ts_module_exit(void)
{
	TS_LOG_INFO("ts_module_exit called here\n");
	if (g_ts_data.ts_task)
		kthread_stop(g_ts_data.ts_task);
	return;
}

late_initcall(ts_module_init);
module_exit(ts_module_exit);

MODULE_AUTHOR("Huawei Device Company");
MODULE_DESCRIPTION("Huawei TouchScreen Driver");
MODULE_LICENSE("GPL");
