/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <mach/exynos-fimc-is-sensor.h>

#include "fimc-is-core.h"
#include "fimc-is-interface.h"
#include "fimc-is-sec-define.h"
#include "fimc-is-device-ischain.h"
#include "fimc-is-dt.h"
#include "fimc-is-device-ois.h"

#define FIMC_IS_OIS_DEV_NAME		"exynos-fimc-is-ois"
#define FIMC_OIS_FW_NAME			"ois_fw.bin"
#define OIS_BOOT_FW_SIZE	(1024 * 4)
#define OIS_PROG_FW_SIZE	(1024 * 24)
#define FW_GYRO_SENSOR		0
#define FW_DRIVER_IC		1
#define FW_CORE_VERSION		2
#define FW_RELEASE_MONTH		3
#define FW_RELEASE_COUNT		4
#define FW_TRANS_SIZE		256
#define OIS_VER_OFFSET	14
static u8 bootCode[OIS_BOOT_FW_SIZE] = {0,};
static u8 progCode[OIS_PROG_FW_SIZE] = {0,};

static struct fimc_is_ois_info ois_minfo;
static struct fimc_is_ois_info ois_pinfo;

int fimc_is_ois_i2c_read(struct i2c_client *client, u16 addr, u8 *data)
{
	int err;
	u8 txbuf[2], rxbuf[1];
	struct i2c_msg msg[2];

	txbuf[0] = (addr & 0xff00) >> 8;
	txbuf[1] = (addr & 0xff);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = txbuf;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = rxbuf;

	err = i2c_transfer(client->adapter, msg, 2);
	if (unlikely(err != 2)) {
		err("%s: register read fail err = %d\n", __func__, err);
		return -EIO;
	}

	*data = rxbuf[0];
	return 0;
}

int fimc_is_ois_i2c_write(struct i2c_client *client ,u16 addr, u8 data)
{
        int retries = I2C_RETRY_COUNT;
        int ret = 0, err = 0;
        u8 buf[3] = {0,};
        struct i2c_msg msg = {
                .addr   = client->addr,
                .flags  = 0,
                .len    = 3,
                .buf    = buf,
        };

        buf[0] = (addr & 0xff00) >> 8;
        buf[1] = addr & 0xff;
        buf[2] = data;

#if 0
        pr_info("%s : W(0x%02X%02X %02X)\n",__func__, buf[0], buf[1], buf[2]);
#endif

        do {
                ret = i2c_transfer(client->adapter, &msg, 1);
                if (likely(ret == 1))
                        break;

                usleep_range(10000,11000);
                err = ret;
        } while (--retries > 0);

        /* Retry occured */
        if (unlikely(retries < I2C_RETRY_COUNT)) {
                err("i2c_write: error %d, write (%04X, %04X), retry %d\n",
                        err, addr, data, I2C_RETRY_COUNT - retries);
        }

        if (unlikely(ret != 1)) {
                err("I2C does not work\n\n");
                return -EIO;
        }

        return 0;
}

int fimc_is_ois_gpio_on(struct fimc_is_device_companion *device)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	pdata = device->pdata;

	if (!pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->gpio_cfg(device->pdev, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_ON);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_ois_gpio_off(struct fimc_is_device_companion *device)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;

	BUG_ON(!device);
	BUG_ON(!device->pdev);
	BUG_ON(!device->pdata);

	pdata = device->pdata;

	if (!pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->gpio_cfg(device->pdev, SENSOR_SCENARIO_OIS_FACTORY, GPIO_SCENARIO_OFF);
	if (ret) {
		err("gpio_cfg is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

void fimc_is_ois_enable(struct fimc_is_core *core)
{
	int ret = 0;

	ret = fimc_is_ois_i2c_write(core->client1, 0x02, 0x00);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x00, 0x01);
	if (ret) {
		err("i2c write fail\n");
	}
}

int fimc_is_ois_sine_mode(struct fimc_is_core *core, int mode)
{
	int ret = 0;

	if (mode == OPTICAL_STABILIZATION_MODE_SINE_X) {
		ret = fimc_is_ois_i2c_write(core->client1, 0x18, 0x01);
	} else if (mode == OPTICAL_STABILIZATION_MODE_SINE_Y) {
		ret = fimc_is_ois_i2c_write(core->client1, 0x18, 0x02);
	}
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x19, 0x01);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x1A, 0x30);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_write(core->client1, 0x02, 0x03);
	if (ret) {
		err("i2c write fail\n");
	}

	msleep(20);

	ret = fimc_is_ois_i2c_write(core->client1, 0x00, 0x01);
	if (ret) {
		err("i2c write fail\n");
	}

	return ret;
}
EXPORT_SYMBOL(fimc_is_ois_sine_mode);

void fimc_is_ois_version(struct fimc_is_core *core)
{
	int ret = 0;
	u8 val_c = 0, val_d = 0;
	u16 version = 0;

	ret = fimc_is_ois_i2c_read(core->client1, 0x00FC, &val_c);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x00FD, &val_d);
	if (ret) {
		err("i2c write fail\n");
	}

	version = (val_d << 8) | val_c;
	pr_info("OIS version = 0x%04x\n", version);
}

void fimc_is_ois_offset_test(struct fimc_is_core *core, long *raw_data_x, long *raw_data_y)
{
	int ret = 0, i = 0;
	u8 val = 0, x = 0, y = 0;
	u16 x_sum = 0, y_sum = 0, sum = 0;
	int retries = 0, avg_count = 20;

	ret = fimc_is_ois_i2c_write(core->client1, 0x0014, 0x01);
	if (ret) {
		err("i2c write fail\n");
	}

	retries = avg_count;
	do {
		fimc_is_ois_i2c_read(core->client1, 0x0014, &val);
		msleep(10);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val);

	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		fimc_is_ois_i2c_read(core->client1, 0x0248, &val);
		x = val;
		fimc_is_ois_i2c_read(core->client1, 0x0249, &val);
		x_sum = (val << 8) | x;
		sum += x_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_x = sum * 1000 / 175 / 10;

	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		fimc_is_ois_i2c_read(core->client1, 0x024A, &val);
		y = val;
		fimc_is_ois_i2c_read(core->client1, 0x024B, &val);
		y_sum = (val << 8) | y;
		sum += y_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_y = sum * 1000 / 175 / 10;

	fimc_is_ois_version(core);
	return;
}

void fimc_is_ois_get_offset_data(struct fimc_is_core *core, long *raw_data_x, long *raw_data_y)
{
	int i = 0;
	u8 val = 0, x = 0, y = 0;
	u16 x_sum = 0, y_sum = 0, sum = 0;
	int retries = 0, avg_count = 20;

	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		fimc_is_ois_i2c_read(core->client1, 0x0248, &val);
		x = val;
		fimc_is_ois_i2c_read(core->client1, 0x0249, &val);
		x_sum = (val << 8) | x;
		sum += x_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_x = sum * 1000 / 175 / 10;

	retries = avg_count;
	for (i = 0; i < retries; retries--) {
		fimc_is_ois_i2c_read(core->client1, 0x024A, &val);
		y = val;
		fimc_is_ois_i2c_read(core->client1, 0x024B, &val);
		y_sum = (val << 8) | y;
		sum += y_sum;
	}
	sum = sum * 10 / avg_count;
	*raw_data_y = sum * 1000 / 175 / 10;

	fimc_is_ois_version(core);
	return;
}

u8 fimc_is_ois_self_test(struct fimc_is_core *core)
{
	int ret = 0;
	u8 val = 0;
	int retries = 20;

	ret = fimc_is_ois_i2c_write(core->client1, 0x0014, 0x08);
	if (ret) {
		err("i2c write fail\n");
	}

	do {
		fimc_is_ois_i2c_read(core->client1, 0x0014, &val);
		msleep(10);
		if (--retries < 0) {
			err("Read register failed!!!!, data = 0x%04x\n", val);
			break;
		}
	} while (val);

	fimc_is_ois_i2c_read(core->client1, 0x0004, &val);

	return val;
}

u16 fimc_is_ois_calc_checksum(u8 *data, int size)
{
	int i = 0;
	u16 result = 0;

	for(i = 0; i < size; i += 2) {
		result = result + (0xFFFF & (((*(data + i + 1)) << 8) | (*(data + i))));
	}

	return result;
}

int fimc_is_ois_i2c_write_multi(struct i2c_client *client ,u16 addr, u8 *data, size_t size)
{
	int retries = I2C_RETRY_COUNT;
	int ret = 0, err = 0, i = 0;
	u8 buf[258] = {0,};
	struct i2c_msg msg = {
                .addr   = client->addr,
                .flags  = 0,
                .len    = size,
                .buf    = buf,
	};

	buf[0] = (addr & 0xFF00) >> 8;
	buf[1] = addr & 0xFF;

	for (i = 0; i < size - 2; i++) {
	        buf[i + 2] = *(data + i);
	}
#if 0
        pr_info("OISLOG %s : W(0x%02X%02X%02X)\n", __func__, buf[0], buf[1], buf[2]);
#endif
        do {
                ret = i2c_transfer(client->adapter, &msg, 1);
                if (likely(ret == 1))
                        break;

                usleep_range(10000,11000);
                err = ret;
        } while (--retries > 0);

        /* Retry occured */
        if (unlikely(retries < I2C_RETRY_COUNT)) {
                err("i2c_write: error %d, write (%04X, %04X), retry %d\n",
                        err, addr, *data, I2C_RETRY_COUNT - retries);
        }

        if (unlikely(ret != 1)) {
                err("I2C does not work\n\n");
                return -EIO;
	}

        return 0;
}

static int fimc_is_ois_i2c_read_multi(struct i2c_client *client, u16 addr, u16 *data)
{
	int err;
	u8 rxbuf[2], txbuf[2];
	struct i2c_msg msg[2];

	txbuf[0] = (addr & 0xff00) >> 8;
	txbuf[1] = (addr & 0xff);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = txbuf;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = rxbuf;

	err = i2c_transfer(client->adapter, msg, 2);
	if (unlikely(err != 2)) {
		pr_err("%s: register read fail\n", __func__);
		return -EIO;
	}

	*data = ((rxbuf[1] << 8) | rxbuf[0]);
	return 0;
}

void fimc_is_ois_fw_version(struct fimc_is_core *core)
{
	int ret = 0;
	char version[7] = {0, };

	ret = fimc_is_ois_i2c_read(core->client1, 0x00F9, &version[0]);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x00F8, &version[1]);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x007C, &version[2]);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x007D, &version[3]);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x007E, &version[4]);
	if (ret) {
		err("i2c write fail\n");
	}

	ret = fimc_is_ois_i2c_read(core->client1, 0x007F, &version[5]);
	if (ret) {
		err("i2c write fail\n");
	}

	memcpy(ois_minfo.header_ver, version, 6);

	return;
}

int fimc_is_ois_get_module_version(struct fimc_is_ois_info **minfo)
{
	*minfo = &ois_minfo;
	return 0;
}

int fimc_is_ois_get_phone_version(struct fimc_is_ois_info **pinfo)
{
	*pinfo = &ois_pinfo;
	return 0;
}

int fimc_is_ois_fw_revision(char *fw_ver)
{
	int revision = 0;
	revision = revision + ((int)fw_ver[FW_RELEASE_MONTH] - 64) * 100;
	revision = revision + ((int)fw_ver[FW_RELEASE_COUNT] - 48) * 10;
	revision = revision + (int)fw_ver[FW_RELEASE_COUNT + 1] - 48;

	return revision;
}

bool fimc_is_ois_version_compare(char *fw_ver1, char *fw_ver2)
{
	if (fw_ver1[FW_GYRO_SENSOR] != fw_ver2[FW_GYRO_SENSOR]
		|| fw_ver1[FW_DRIVER_IC] != fw_ver2[FW_DRIVER_IC]
		|| fw_ver1[FW_CORE_VERSION] != fw_ver2[FW_CORE_VERSION]) {
		return false;
	}

	return true;
}

int fimc_is_ois_open_fw(struct fimc_is_core *core, char *name, u8 **buf)
{
	int ret = 0;
	u32 size = 0;
	const struct firmware *fw_blob = NULL;
	static char fw_name[100];
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long nread;
	int fw_requested = 1;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	snprintf(fw_name, sizeof(fw_name), "%s%s",FIMC_IS_SETFILE_SDCARD_PATH, name);
	fp = filp_open(fw_name, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fp)) {
		err("failed to open SDCARD fw!!!");
		goto request_fw;
	}

	fw_requested = 0;
	size = fp->f_path.dentry->d_inode->i_size;
	pr_info("start read sdcard, file path %s, size %d Bytes\n", fw_name, size);

	*buf = vmalloc(size);
	if (!(*buf)) {
		err("failed to allocate memory");
		ret = -ENOMEM;
		goto p_err;
	}

	nread = vfs_read(fp, (char __user *)(*buf), size, &fp->f_pos);
	if (nread != size) {
		err("failed to read firmware file, %ld Bytes\n", nread);
		ret = -EIO;
		goto p_err;
	}

	memcpy(ois_pinfo.header_ver, *buf + nread - OIS_VER_OFFSET, 6);
	memcpy(bootCode, *buf, OIS_BOOT_FW_SIZE);
	memcpy(progCode, *buf + nread - OIS_PROG_FW_SIZE, OIS_PROG_FW_SIZE);

request_fw:
	if (fw_requested) {
		snprintf(fw_name, sizeof(fw_name), "%s", name);
		set_fs(old_fs);
		ret = request_firmware(&fw_blob, fw_name, &core->companion->pdev->dev);
		if (ret) {
			err("request_firmware is fail(ret:%d)", ret);
			ret = -EINVAL;
			goto p_err;
		}

		if (!fw_blob) {
			err("fw_blob is NULL");
			ret = -EINVAL;
			goto p_err;
		}

		if (!fw_blob->data) {
			err("fw_blob->data is NULL");
			ret = -EINVAL;
			goto p_err;
		}

		size = fw_blob->size;

		*buf = vmalloc(size);
		if (!(*buf)) {
			err("failed to allocate memory");
			ret = -ENOMEM;
			goto p_err;
		}
		memcpy((void *)(*buf), fw_blob->data, size);
		memcpy(ois_pinfo.header_ver, *buf + size - OIS_VER_OFFSET, 6);
		memcpy(bootCode, *buf, OIS_BOOT_FW_SIZE);
		memcpy(progCode, *buf + size - OIS_PROG_FW_SIZE, OIS_PROG_FW_SIZE);
	}

p_err:
	if (!fw_requested) {
		if (!IS_ERR_OR_NULL(fp)) {
			filp_close(fp, current->files);
		}
		set_fs(old_fs);
	} else {
		if (!IS_ERR_OR_NULL(fw_blob)) {
			release_firmware(fw_blob);
		}
	}

	return ret;
}

void fimc_is_ois_check_fw(struct fimc_is_core *core)
{
	u8 *buf = NULL;
	int ret = 0;

	ret = fimc_is_ois_open_fw(core, FIMC_OIS_FW_NAME, &buf);
	if (ret) {
		err("fimc_is_ois_open_fw failed(%d)\n", ret);
	}

	if (buf) {
		vfree(buf);
	}

	return;
}

void fimc_is_ois_fw_update(struct fimc_is_core *core)
{
	u8 SendData[256], RcvData;
	u16 RcvDataShort = 0;
	int block, i, position = 0;
	u16 checkSum;
	u8 *buf = NULL;
	int ret = 0, sum_size = 0;
	int module_ver = 0, binary_ver = 0;
	struct i2c_client *client = core->client1;

	/* OIS Status Check */
	ret = fimc_is_ois_open_fw(core, FIMC_OIS_FW_NAME, &buf);
	if (ret) {
		err("fimc_is_ois_open_fw failed(%d)\n", ret);
		goto p_err;
	}

	fimc_is_ois_fw_version(core);

	/*Update OIS FW when Gyro sensor/Driver IC/ Core version is same*/
	if (!fimc_is_ois_version_compare(ois_minfo.header_ver, ois_pinfo.header_ver)) {
		err("Does not update ois fw. OIS module ver = %s, binary ver = %s\n", ois_minfo.header_ver, ois_pinfo.header_ver );
		goto p_err;
	}

	module_ver = fimc_is_ois_fw_revision(ois_minfo.header_ver);
	binary_ver = fimc_is_ois_fw_revision(ois_pinfo.header_ver);
	if (binary_ver <= module_ver) {
		err("OIS module ver = %d, binary ver = %d\n", module_ver, binary_ver);
		goto p_err;
	}

	fimc_is_ois_i2c_read(client, 0x01, &RcvData); /* OISSTS Read */

	if (RcvData != 0x01) {/* OISSTS != IDLE */
		err("RCV data return!!\n");
		goto p_err;
	}

	/* Update a Boot Program */
	/* SET FWUP_CTRL REG */
	fimc_is_ois_i2c_write(client ,0x000C, 0x0B);

	position = 0;
	/* Write Boot Data */
	for (block = 4; block < 8; block++) {       /* 0x1000 - 0x1FFF */
		for (i = 0; i < 4; i++) {
			memcpy(SendData, bootCode + position, FW_TRANS_SIZE);
			fimc_is_ois_i2c_write_multi(client, 0x0100, SendData, FW_TRANS_SIZE + 2);
			position += FW_TRANS_SIZE;
			if (i == 0 ) {
				msleep(20);
			} else if ((i == 1) || (i == 2)) {	 /* Wait Erase and Write process */
				 msleep(10);		/* Wait Write process */
			} else {
				msleep(15); /* Wait Write and Verify process */
			}
		}
	}

	/* CHECKSUM (Boot) */
	sum_size = OIS_BOOT_FW_SIZE;
	checkSum = fimc_is_ois_calc_checksum(&bootCode[0], sum_size);
	SendData[0] = (checkSum & 0x00FF);
	SendData[1] = (checkSum & 0xFF00) >> 8;
	SendData[2] = 0x00;
	SendData[3] = 0x00;
	fimc_is_ois_i2c_write_multi(client, 0x08, SendData, 6);
	msleep(10);			 /* (RUMBA Self Reset) */

	fimc_is_ois_i2c_read_multi(client, 0x06, &RcvDataShort); /* Error Status read */
	pr_info("OISSTS Read = 0x%04x\n", RcvDataShort);

	if (RcvDataShort == 0x0001) {					/* Boot F/W Update Error check */
		/* Update a User Program */
		/* SET FWUP_CTRL REG */
		position = 0;
		SendData[0] = 0x09;		/* FWUP_DSIZE=256Byte, FWUP_MODE=PROG, FWUPEN=Start */
		fimc_is_ois_i2c_write(client, 0x0C, SendData[0]);		/* FWUPCTRL REG(0x000C) 1Byte Send */

		/* Write UserProgram Data */
		for (block = 4; block <= 27; block++) {		/* 0x1000 -0x 6FFF (RUMBA-SA)*/
			for (i = 0; i < 4; i++) {
				memcpy(SendData, &progCode[position], (size_t)FW_TRANS_SIZE);
				fimc_is_ois_i2c_write_multi(client, 0x100, SendData, FW_TRANS_SIZE + 2); /* FLS_DATA REG(0x0100) 256Byte Send */
				position += FW_TRANS_SIZE;
				if (i ==0 ) {
					msleep(20);		/* Wait Erase and Write process */
				} else if ((i==1) || (i==2)) {
					msleep(10);		/* Wait Write process */
				} else {
					 msleep(15);		/* Wait Write and Verify process */
				}
			}
		}

		/* CHECKSUM (Program) */
		sum_size = OIS_PROG_FW_SIZE;
		checkSum = fimc_is_ois_calc_checksum(&progCode[0], sum_size);
		SendData[0] = (checkSum & 0x00FF);
		SendData[1] = (checkSum & 0xFF00) >> 8;
		SendData[2] = 0;		/* Don't Care */
		SendData[3] = 0x80;		/* Self Reset Request */

		fimc_is_ois_i2c_write_multi(client, 0x08, SendData, 6);  /* FWUP_CHKSUM REG(0x0008) 4Byte Send */
		msleep(10);		/* (Wait RUMBA Self Reset) */
		fimc_is_ois_i2c_read(client, 0x6, (u8*)&RcvDataShort); /* Error Status read */

		if (RcvDataShort == 0x0000) {
			/* F/W Update Success Process */
			err("OISLOG OIS program update success\n");
		} else {
			/* F/W Update Error Process */
			err("OISLOG OIS program update fail\n");
		}
	} else {
		/* Error Process */
		err("CAN'T process OIS program update\n");
	}

p_err:
	pr_info("OIS module ver = %s, binary ver = %s\n", ois_minfo.header_ver, ois_pinfo.header_ver );

	if (buf) {
		vfree(buf);
	}

	return;
}

static int fimc_is_ois_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct fimc_is_device_ois *device;
	struct ois_i2c_platform_data *pdata;
	struct fimc_is_core *core;

	if (fimc_is_dev == NULL) {
		warn("fimc_is_dev is not yet probed");
		client->dev.init_name = FIMC_IS_OIS_DEV_NAME;
		return -EPROBE_DEFER;
	}

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core)
		return -EINVAL;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct ois_i2c_platform_data), GFP_KERNEL);
		if (!pdata) {
			err("Failed to allocate memory\n");
			return -ENOMEM;
		}
	} else {
		pdata = client->dev.platform_data;
	}

	if (pdata == NULL) {
		err("%s : ois probe fail\n", __func__);
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err("No I2C functionality found\n");
		devm_kfree(&client->dev, pdata);
		return -ENODEV;
	}

	device = kzalloc(sizeof(struct fimc_is_device_ois), GFP_KERNEL);
	if (!device) {
		err("fimc_is_device_companion is NULL");
		devm_kfree(&client->dev, pdata);
		return -ENOMEM;
	}

	core->client1 = client;
	i2c_set_clientdata(client, device);

	return 0;
}

static int fimc_is_ois_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id ois_id[] = {
	{FIMC_IS_OIS_DEV_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, ois_id);

#ifdef CONFIG_OF
static struct of_device_id ois_dt_ids[] = {
	{ .compatible = "rumba,ois",},
	{},
};
#endif

static struct i2c_driver ois_i2c_driver = {
	.driver = {
		   .name = FIMC_IS_OIS_DEV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = ois_dt_ids,
#endif
	},
	.probe = fimc_is_ois_probe,
	.remove = fimc_is_ois_remove,
	.id_table = ois_id,
};
module_i2c_driver(ois_i2c_driver);

MODULE_DESCRIPTION("OIS driver for Rumba");
MODULE_AUTHOR("kyoungho yun <kyoungho.yun@samsung.com>");
MODULE_LICENSE("GPL v2");
