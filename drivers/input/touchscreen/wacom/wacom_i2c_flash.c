/*
 *  wacom_i2c_flash.c - Wacom G5 Digitizer Controller (I2C bus)
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "wacom_i2c.h"
#include "wacom_i2c_flash.h"

static int wacom_enter_flash_mode(struct wacom_i2c *wac_i2c)
{
	int ret, len = 0;
	char buf[8];

	dev_info(&wac_i2c->client->dev, "%s\n", __func__);

	memset(buf, 0, 8);
	strncpy(buf, "\rflash\r", 7);

	len = sizeof(buf) / sizeof(buf[0]);

	ret = wac_i2c->wacom_i2c_send(wac_i2c, buf, len, WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		dev_err(&wac_i2c->client->dev,
				"%s: Sending flash command failed, %d\n",
				__func__, ret);
		return ret;
	}

	msleep(500);

	return 0;
}

static int wacom_set_cmd_feature(struct wacom_i2c *wac_i2c)
{
	int ret, len = 0;
	u8 buf[4];

	buf[len++] = 4;
	buf[len++] = 0;
	buf[len++] = 0x37;
	buf[len++] = CMD_SET_FEATURE;

	ret = wac_i2c->wacom_i2c_send(wac_i2c, buf, len, WACOM_I2C_MODE_BOOT);
	if (ret < 0)
		dev_err(&wac_i2c->client->dev,
				"%s: failed send CMD_SET_FEATURE, %d\n",
				__func__, ret);

	return ret;
}

static int wacom_get_cmd_feature(struct wacom_i2c *wac_i2c)
{
	int ret, len = 0;
	u8 buf[6];

	buf[len++] = 4;
	buf[len++] = 0;
	buf[len++] = 0x38;
	buf[len++] = CMD_GET_FEATURE;
	buf[len++] = 5;
	buf[len++] = 0;

	ret = wac_i2c->wacom_i2c_send(wac_i2c, buf, len, WACOM_I2C_MODE_BOOT);
	if (ret < 0)
		dev_err(&wac_i2c->client->dev,
				"%s: failed send CMD_GET_FEATURE, ret: %d\n",
				__func__, ret);
	return ret;
}

/*
 * mode1. BOOT_QUERY: check enter boot mode for flash.
 * mode2. BOOT_BLVER : check bootloader version.
 * mode3. BOOT_MPU : check MPU version
 */
int wacom_check_flash_mode(struct wacom_i2c *wac_i2c, int mode)
{
	int ret, ECH;
	unsigned char response_cmd = 0;
	unsigned char command[CMD_SIZE];
	unsigned char response[RSP_SIZE];

	switch (mode) {
	case BOOT_QUERY:
		response_cmd = QUERY_CMD;
		break;
	case BOOT_BLVER:
		response_cmd = BOOT_CMD;
		break;
	case BOOT_MPU:
		response_cmd = MPU_CMD;
		break;
	default:
		break;
	}

	dev_info(&wac_i2c->client->dev, "%s, mode = %s\n", __func__,
			(mode == BOOT_QUERY) ? "BOOT_QUERY" :
			(mode == BOOT_BLVER) ? "BOOT_BLVER" :
			(mode == BOOT_MPU) ? "BOOT_MPU" : "Not Support");

	ret = wacom_set_cmd_feature(wac_i2c);
	if (ret < 0)
		return ret;

	command[0] = 5;
	command[1] = 0;
	command[2] = 5;
	command[3] = 0;
	command[4] = BOOT_CMD_REPORT_ID;
	command[5] = mode;
	command[6] = ECH = 7;

	ret = wac_i2c->wacom_i2c_send(wac_i2c, command, 7, WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		dev_err(&wac_i2c->client->dev,
				"%s: failed send REPORT_ID, %d\n",
				__func__, ret);
		return ret;
	}

	ret = wacom_get_cmd_feature(wac_i2c);
	if (ret < 0)
		return ret;

	usleep_range(10000, 11000);

	ret = wac_i2c->wacom_i2c_recv(wac_i2c, response, BOOT_RSP_SIZE,
			    WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		dev_err(&wac_i2c->client->dev,
				"%s: failed receive response, %d\n",
				__func__, ret);
		return ret;
	}

	if ((response[3] != response_cmd) || (response[4] != ECH)) {
		dev_err(&wac_i2c->client->dev,
				"%s: failed matching response3[%x], 4[%x]\n",
				__func__, response[3], response[4]);
		return -EIO;
	}

	return response[5];

}

int wacom_enter_bootloader(struct wacom_i2c *wac_i2c)
{
	int ret;
	int retry = 0;

	dev_info(&wac_i2c->client->dev, "%s\n", __func__);

	ret = wacom_enter_flash_mode(wac_i2c);
	if (ret < 0)
		msleep(500);

	do {
		msleep(100);
		ret = wacom_check_flash_mode(wac_i2c, BOOT_QUERY);
		retry++;
	} while (ret < 0 && retry < 10);

	if (ret < 0)
		return -EXIT_FAIL_GET_BOOT_LOADER_VERSION;

	ret = wacom_check_flash_mode(wac_i2c, BOOT_BLVER);
	wac_i2c->boot_ver = ret;
	if (ret < 0)
		return -EXIT_FAIL_GET_BOOT_LOADER_VERSION;

	return ret;
}

static int wacom_flash_end(struct wacom_i2c *wac_i2c)
{
	int ret, ECH;
	unsigned char command[CMD_SIZE];

	dev_info(&wac_i2c->client->dev, "%s\n", __func__);

	command[0] = 4;
	command[1] = 0;
	command[2] = 0x37;
	command[3] = CMD_SET_FEATURE;
	command[4] = 5;
	command[5] = 0;
	command[6] = 5;
	command[7] = 0;
	command[8] = BOOT_CMD_REPORT_ID;
	command[9] = BOOT_EXIT;
/*	command[10] = ECH = 7;*/
	command[10] = ECH = 0;

	ret = wac_i2c->wacom_i2c_send(wac_i2c, command, 11, WACOM_I2C_MODE_BOOT);
	if (ret < 0)
		dev_err(&wac_i2c->client->dev,
				"%s failed, %d\n",
				__func__, ret);
	return ret;
}

static int wacom_flash_erase(struct wacom_i2c *wac_i2c,
			int *eraseBlock, int num)
{
	int ret = 0, ECH;
	unsigned char sum;
	unsigned char cmd_chksum;
	int i, j;
	unsigned char command[CMD_SIZE];
	unsigned char response[RSP_SIZE];
	int len;

	dev_info(&wac_i2c->client->dev,
		"%s: total erase code-block number = %d\n", __func__, num);

	for (i = 0; i < num; i++) {
		/*msleep(500);*/
		ret = wacom_set_cmd_feature(wac_i2c);
		if (ret < 0) {
			dev_info(&wac_i2c->client->dev,
				"%s: set command failed, retry= %d\n",
				__func__, i);
			return ret;
		}
		len = 0;

		command[len++] = 5;						/* Data Register-LSB */
		command[len++] = 0;						/* Data-Register-MSB */
		command[len++] = 7;	               				/* Length Field-LSB */
		command[len++] = 0;						/* Length Field-MSB */
		command[len++] = BOOT_CMD_REPORT_ID;                 	/* Report:ReportID */
		command[len++] = BOOT_ERASE_FLASH;			        /* Report:erase command */
		command[len++] = ECH = i;					/* Report:echo */
		command[len++] = *eraseBlock;				/* Report:erased block No. */
		eraseBlock++;

		sum = 0;
		for (j = 0; j < 8; j++)
			sum += command[j];
		cmd_chksum = ~sum + 1;
		command[len++] = cmd_chksum;

		ret = wac_i2c->wacom_i2c_send(wac_i2c, command, len, WACOM_I2C_MODE_BOOT);
		if (ret < 0) {
			dev_err(&wac_i2c->client->dev,
					"%s: failed send erase command, %d\n",
					__func__, ret);
			return ret;
		}

		msleep(300);

		ret = wacom_get_cmd_feature(wac_i2c);
		if (ret < 0) {
			dev_info(&wac_i2c->client->dev,
				"%s: get command failed, retry= %d\n",
				__func__, i);
			return ret;
		}

		ret = wac_i2c->wacom_i2c_recv(wac_i2c, response, BOOT_RSP_SIZE,
				    WACOM_I2C_MODE_BOOT);
		if (ret < 0) {
			dev_err(&wac_i2c->client->dev,
					"%s: failed receive response, %d\n",
					__func__, ret);
			return ret;
		}

		if ((response[3] != 0x00) || (response[4] != ECH) 
			|| (response[5] != 0xff && response[5] != 0x00)) {
			dev_err(&wac_i2c->client->dev,
					"%s: failed matching response3[%x], 4[%x], 5[%x]\n",
					__func__, response[3], response[4], response[5]);
			return -EIO;
		}
	}
	return ret;
}

static int wacom_flash_erase_data(struct wacom_i2c *wac_i2c)
{
	int ret = 0, ECH;
	unsigned char sum;
	unsigned char cmd_chksum;
	int j;
	unsigned char command[CMD_SIZE];
	unsigned char response[RSP_SIZE];
	int len = 0;
	int ii;

	dev_info(&wac_i2c->client->dev, "%s\n", __func__);

	for (ii = 0; ii < SIZE_OF_DATA_FLASH_AREAD; ii++) {

		ret = wacom_set_cmd_feature(wac_i2c);
		if (ret < 0) {
			dev_info(&wac_i2c->client->dev,
				"%s: set command failed, l= %d\n",
				__func__, __LINE__);
			return ret;
		}

		command[len++] = 5;						/* Data Register-LSB */
		command[len++] = 0;						/* Data-Register-MSB */
		command[len++] = 7;	               				/* Length Field-LSB */
		command[len++] = 0;						/* Length Field-MSB */
		command[len++] = BOOT_CMD_REPORT_ID;                 	/* Report:ReportID */
		command[len++] = BOOT_ERASE_DATAMEM;			        /* Report:erase command */
		command[len++] = ECH = BOOT_ERASE_DATAMEM_ECH;					/* Report:echo */
		command[len++] = ii;				/* Report:erased block No. */

		sum = 0;
		for (j = 0; j < 8; j++)
			sum += command[j];
		cmd_chksum = ~sum + 1;
		command[len++] = cmd_chksum;

		ret = wac_i2c->wacom_i2c_send(wac_i2c, command, len, WACOM_I2C_MODE_BOOT);
		if (ret < 0) {
				dev_err(&wac_i2c->client->dev,
					"%s: failed send erase command, %d\n",
					__func__, ret);
			return ret;
			}

		msleep(300);

		ret = wacom_get_cmd_feature(wac_i2c);
		if (ret < 0) {
			dev_info(&wac_i2c->client->dev,
				"%s: get command failed, l= %d\n",
				__func__, __LINE__);
			return ret;
		}

		ret = wac_i2c->wacom_i2c_recv(wac_i2c, response, BOOT_RSP_SIZE,
				    WACOM_I2C_MODE_BOOT);
		if (ret < 0) {
				dev_err(&wac_i2c->client->dev,
					"%s: failed receive response, %d\n",
					__func__, ret);
			return ret;
			}

		if ((response[3] != BOOT_ERASE_DATAMEM) || (response[4] != ECH) 
			|| (response[5] != 0xff && response[5] != 0x00)) {
			dev_err(&wac_i2c->client->dev,
					"%s: failed matching response3[%x], 4[%x], 5[%x]\n",
					__func__, response[3], response[4], response[5]);
			return -EIO;
		}

	}
	return ret;
}

static int wacom_flash_write_block(struct wacom_i2c *wac_i2c, const unsigned char *flash_data,
			      unsigned long ulAddress, u8 *pcommand_id)
{
	const int MAX_COM_SIZE = (12 + FLASH_BLOCK_SIZE + 2);
	int ECH;
	int ret;
	unsigned char sum;
	unsigned char command[MAX_COM_SIZE];
	unsigned char response[RSP_SIZE];
	unsigned int i;

	ret = wacom_set_cmd_feature(wac_i2c);
	if (ret < 0)
		return ret;

	command[0] = 5;
	command[1] = 0;
	command[2] = 76;
	command[3] = 0;
	command[4] = BOOT_CMD_REPORT_ID;
	command[5] = BOOT_WRITE_FLASH;
	command[6] = ECH = ++(*pcommand_id);
	command[7] = ulAddress & 0x000000ff;
	command[8] = (ulAddress & 0x0000ff00) >> 8;
	command[9] = (ulAddress & 0x00ff0000) >> 16;
	command[10] = (ulAddress & 0xff000000) >> 24;
	command[11] = 8;
	sum = 0;
	for (i = 0; i < 12; i++)
		sum += command[i];
	command[MAX_COM_SIZE - 2] = ~sum + 1;

	sum = 0;
	for (i = 12; i < (FLASH_BLOCK_SIZE + 12); i++) {
		command[i] = flash_data[ulAddress + (i - 12)];
		sum += flash_data[ulAddress + (i - 12)];
	}
	command[MAX_COM_SIZE - 1] = ~sum + 1;

	ret = wac_i2c->wacom_i2c_send(wac_i2c, command, BOOT_CMD_SIZE,
			    WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		dev_dbg(&wac_i2c->client->dev,
				"%s: epen:1 rv:%d\n", __func__, ret);
		return ret;
	}

	usleep_range(10000, 11000);

	ret = wacom_get_cmd_feature(wac_i2c);
	if (ret < 0)
		return ret;

	ret = wac_i2c->wacom_i2c_recv(wac_i2c, response, BOOT_RSP_SIZE,
			    WACOM_I2C_MODE_BOOT);
	if (ret < 0) {
		dev_err(&wac_i2c->client->dev,
				"%s: failed receive response, %d\n",
				__func__, ret);
		return ret;
	}

	if ((response[3] != WRITE_CMD) ||
	    (response[4] != ECH) || response[5] != ACK)
		return -EIO;

	return 0;

}

static int wacom_flash_write(struct wacom_i2c *wac_i2c,
			const unsigned char *flash_data, size_t data_size,
			unsigned long start_address, unsigned long *max_address)
{
	unsigned long ulAddress;
	bool ret;
	int i;
	unsigned long pageNo = 0;
	u8 command_id = 0;

	dev_info(&wac_i2c->client->dev, "%s\n", __func__);

	for (ulAddress = start_address; ulAddress < *max_address;
	     ulAddress += FLASH_BLOCK_SIZE) {
		unsigned int j;
		bool bWrite = false;

		/* Wacom 2012/10/04: skip if all each data locating on
			from ulAddr to ulAddr+Block_SIZE_W are 0xff */
		for (i = 0; i < FLASH_BLOCK_SIZE; i++) {
			if (flash_data[ulAddress + i] != 0xFF)
				break;
		}
		if (i == (FLASH_BLOCK_SIZE)) {
			/*printk(KERN_DEBUG"epen:BLOCK PASSED\n"); */
			continue;
		}
		/* Wacom 2012/10/04 */

		for (j = 0; j < FLASH_BLOCK_SIZE; j++) {
			if (flash_data[ulAddress + j] == 0xFF)
				continue;
			else {
				bWrite = true;
				break;
			}
		}

		if (!bWrite) {
			pageNo++;
			continue;
		}

		ret = wacom_flash_write_block(wac_i2c, flash_data, ulAddress,
				       &command_id);
		if (ret < 0)
			return ret;

		pageNo++;
	}

	return 0;
}

int wacom_i2c_flash(struct wacom_i2c *wac_i2c)
{
	unsigned long max_address = 0;
	unsigned long start_address;
	int i, ret = 0;
	int eraseBlock[SIZE_OF_CODE_FLASH_AREA], eraseBlockNum;

	if (wac_i2c->ic_mpu_ver != MPU_W9007 && wac_i2c->ic_mpu_ver != MPU_W9010 && wac_i2c->ic_mpu_ver != MPU_W9012)
		return -EXIT_FAIL_GET_MPU_TYPE;

	wac_i2c->compulsory_flash_mode(wac_i2c, true);
	/*Reset*/
	wac_i2c->reset_platform_hw(wac_i2c);
	msleep(300);
	dev_err(&wac_i2c->client->dev, "%s: Set FWE\n", __func__);

	wacom_enter_bootloader(wac_i2c);
	dev_err(&wac_i2c->client->dev, "%s: enter bootloader mode\n", __func__);

	/*Set start and end address and block numbers*/
	eraseBlockNum = 0;
	start_address = START_ADDR_W9012;
	max_address = MAX_ADDR_W9012;

	for (i = BLOCK_NUM_W9012; i >= 8; i--) {
		eraseBlock[eraseBlockNum] = i;
		eraseBlockNum++;
	}

	/*Erase the old program code */
	ret = wacom_flash_erase(wac_i2c, eraseBlock, eraseBlockNum);
	if (ret < 0) {
		dev_err(&wac_i2c->client->dev,
				"%s: failed erase old firmware, %d\n",
				__func__, ret);
		goto flashing_fw_err;
	}

	/*Erase the old program data */
	ret = wacom_flash_erase_data(wac_i2c);
	if (ret < 0) {
		dev_err(&wac_i2c->client->dev,
				"%s: failed erase old firmware, %d\n",
				__func__, ret);
		goto flashing_fw_err;
	}


	/*Write the new program */
	ret = wacom_flash_write(wac_i2c, wac_i2c->fw_data,
	    			DATA_SIZE, start_address, &max_address);
	if (ret < 0) {
		dev_err(&wac_i2c->client->dev,
				"%s: failed writing new firmware, %d\n",
				__func__, ret);
		goto flashing_fw_err;
	}

	/*Enable */
	ret = wacom_flash_end(wac_i2c);
	if (ret < 0) {
		dev_err(&wac_i2c->client->dev,
				"%s: failed flash mode close, %d\n",
				__func__, ret);
		goto flashing_fw_err;
	}

	dev_err(&wac_i2c->client->dev,
				"%s: Successed new Firmware writing\n",
				__func__);

flashing_fw_err:
	wac_i2c->compulsory_flash_mode(wac_i2c, false);
	msleep(150);

	return ret;
}

int wacom_i2c_firm_update(struct wacom_i2c *wac_i2c)
{
	int ret = 0;
	int retry = 3;

	while (retry--) {
		ret = wacom_i2c_flash(wac_i2c);
		if (ret < 0)
			dev_err(&wac_i2c->client->dev,
				"%s: failed to write firmware(%d)\n",
				__func__, ret);
		else
			dev_err(&wac_i2c->client->dev,
				"%s: Successed to write firmware(%d)\n",
				__func__, ret);
		/* Reset IC */
		wac_i2c->reset_platform_hw(wac_i2c);
		if (ret >= 0)
			return 0;
	}

	return ret;
}

int wacom_fw_load_from_UMS(struct wacom_i2c *wac_i2c)
	{
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int ret = 0;
	const struct firmware *firm_data = NULL;

	dev_info(&wac_i2c->client->dev,
			"%s: Start firmware flashing (UMS).\n", __func__);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(WACOM_UMS_FW_PATH, O_RDONLY, S_IRUSR);

	if (IS_ERR(fp)) {
		dev_err(&wac_i2c->client->dev,
			"%s: failed to open %s.\n",
			__func__, WACOM_UMS_FW_PATH);
		ret = -ENOENT;
		goto open_err;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	dev_info(&wac_i2c->client->dev,
		"%s: start, file path %s, size %ld Bytes\n",
		__func__, WACOM_UMS_FW_PATH, fsize);

	firm_data = kzalloc(fsize, GFP_KERNEL);
	if (IS_ERR(firm_data)) {
		dev_err(&wac_i2c->client->dev,
			"%s, kmalloc failed\n", __func__);
			ret = -EFAULT;
		goto malloc_error;
	}

	nread = vfs_read(fp, (char __user *)firm_data,
		fsize, &fp->f_pos);

	dev_info(&wac_i2c->client->dev,
		"%s: nread %ld Bytes\n", __func__, nread);
	if (nread != fsize) {
		dev_err(&wac_i2c->client->dev,
			"%s: failed to read firmware file, nread %ld Bytes\n",
			__func__, nread);
		ret = -EIO;
		goto read_err;
	}

	filp_close(fp, current->files);
	set_fs(old_fs);
	/*start firm update*/
	wac_i2c->fw_data=(unsigned char *)firm_data;

	return 0;

read_err:
	kfree(firm_data);
malloc_error:
	filp_close(fp, current->files);
open_err:
	set_fs(old_fs);
	return ret;
}

int wacom_load_fw_from_req_fw(struct wacom_i2c *wac_i2c)
{
	int ret = 0;
	const struct firmware *firm_data = NULL;
	char fw_path[WACOM_MAX_FW_PATH];

	dev_info(&wac_i2c->client->dev, "%s\n", __func__);

	/*Obtain MPU type: this can be manually done in user space */
	dev_info(&wac_i2c->client->dev,
			"%s: MPU type: %x , BOOT ver: %x\n",
			__func__, wac_i2c->ic_mpu_ver, wac_i2c->boot_ver);

	memset(&fw_path, 0, WACOM_MAX_FW_PATH);
	if (wac_i2c->ic_mpu_ver == MPU_W9012) {
			snprintf(fw_path, WACOM_MAX_FW_PATH,
				"%s", WACOM_FW_NAME_W9012);
	} else {
		dev_info(&wac_i2c->client->dev,
			"%s: firmware name is NULL. return -1\n",
			__func__);
		ret = -ENOENT;
		goto firm_name_null_err;
	}

	ret = request_firmware(&firm_data, fw_path, &wac_i2c->client->dev);
	if (ret < 0) {
		dev_err(&wac_i2c->client->dev,
		       "%s: Unable to open firmware. ret %d\n",
				__func__, ret);
		goto request_firm_err;
	}

	dev_info(&wac_i2c->client->dev, "%s: firmware name: %s, size: %d\n",
			__func__, fw_path, firm_data->size);

	/* firmware version check */
	if (wac_i2c->ic_mpu_ver == MPU_W9010 || wac_i2c->ic_mpu_ver == MPU_W9007 || wac_i2c->ic_mpu_ver == MPU_W9012)
		wac_i2c->wac_query_data->fw_version_bin =
			(firm_data->data[FIRM_VER_LB_ADDR_W9012] |
			(firm_data->data[FIRM_VER_UB_ADDR_W9012] << 8));

	dev_info(&wac_i2c->client->dev, "%s: firmware version = %x\n",
			__func__, wac_i2c->wac_query_data->fw_version_bin);
	wac_i2c->fw_data=(unsigned char *)firm_data->data;
	release_firmware(firm_data);

firm_name_null_err:
request_firm_err:
	return ret;
}
