/*
 *  p9220_charger.c
 *  Samsung p9220 Charger Driver
 *
 *  Copyright (C) 2015 Samsung Electronics
 * Yeongmi Ha <yeongmi86.ha@samsung.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/battery/charger/p9220_charger.h>
#include <linux/errno.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/ctype.h>
#include <linux/firmware.h>

#define ENABLE 1
#define DISABLE 0

static enum power_supply_property sec_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
	POWER_SUPPLY_PROP_CHARGE_POWERED_OTG_CONTROL,
};

static struct i2c_client *p9220_i2c;
static const u8 OTPBootloader[] = {
0x00, 0x04, 0x00, 0x20, 0x57, 0x01, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0xFE, 0xE7, 0x00, 0x00, 0x80, 0x00, 0x00, 0xE0, 0x00, 0xBF, 0x40, 0x1E, 0xFC, 0xD2, 0x70, 0x47,
0x00, 0xB5, 0x6F, 0x4A, 0x6F, 0x4B, 0x01, 0x70, 0x01, 0x20, 0xFF, 0xF7, 0xF3, 0xFF, 0x52, 0x1E,
0x02, 0xD0, 0x18, 0x8B, 0x00, 0x06, 0xF7, 0xD4, 0x00, 0xBD, 0xF7, 0xB5, 0x05, 0x46, 0x6A, 0x48,
0x81, 0xB0, 0x00, 0x21, 0x94, 0x46, 0x81, 0x81, 0x66, 0x48, 0x31, 0x21, 0x01, 0x80, 0x04, 0x21,
0x81, 0x80, 0x06, 0x21, 0x01, 0x82, 0x28, 0x20, 0xFF, 0xF7, 0xDC, 0xFF, 0x00, 0x24, 0x15, 0xE0,
0x02, 0x99, 0x28, 0x5D, 0x09, 0x5D, 0x02, 0x46, 0x8A, 0x43, 0x01, 0xD0, 0x10, 0x20, 0x50, 0xE0,
0x81, 0x43, 0x0A, 0xD0, 0x5D, 0x4E, 0xB0, 0x89, 0x08, 0x27, 0x38, 0x43, 0xB0, 0x81, 0x28, 0x19,
0xFF, 0xF7, 0xCE, 0xFF, 0xB0, 0x89, 0xB8, 0x43, 0xB0, 0x81, 0x64, 0x1C, 0x64, 0x45, 0xE7, 0xD3,
0x54, 0x48, 0x36, 0x21, 0x01, 0x82, 0x00, 0x24, 0x38, 0xE0, 0x02, 0x98, 0x00, 0x27, 0x06, 0x5D,
0x52, 0x48, 0x82, 0x89, 0x08, 0x21, 0x0A, 0x43, 0x82, 0x81, 0x28, 0x19, 0x00, 0x90, 0x4D, 0x4A,
0x08, 0x20, 0x90, 0x80, 0x02, 0x20, 0xFF, 0xF7, 0xAD, 0xFF, 0x28, 0x5D, 0x33, 0x46, 0x83, 0x43,
0x15, 0xD0, 0x48, 0x49, 0x04, 0x20, 0x88, 0x80, 0x02, 0x20, 0xFF, 0xF7, 0xA3, 0xFF, 0x19, 0x46,
0x00, 0x98, 0xFF, 0xF7, 0xA5, 0xFF, 0x43, 0x49, 0x0F, 0x20, 0x88, 0x80, 0x02, 0x20, 0xFF, 0xF7,
0x99, 0xFF, 0x28, 0x5D, 0xB0, 0x42, 0x02, 0xD0, 0x7F, 0x1C, 0x0A, 0x2F, 0xDF, 0xD3, 0x3F, 0x48,
0x82, 0x89, 0x08, 0x21, 0x8A, 0x43, 0x82, 0x81, 0x0A, 0x2F, 0x06, 0xD3, 0x3C, 0x48, 0x29, 0x19,
0x41, 0x80, 0x29, 0x5D, 0xC1, 0x80, 0x04, 0x20, 0x03, 0xE0, 0x64, 0x1C, 0x64, 0x45, 0xC4, 0xD3,
0x02, 0x20, 0x34, 0x49, 0x11, 0x22, 0x0A, 0x80, 0x04, 0x22, 0x8A, 0x80, 0x32, 0x49, 0xFF, 0x22,
0x8A, 0x81, 0x04, 0xB0, 0xF0, 0xBD, 0x34, 0x49, 0x32, 0x48, 0x08, 0x60, 0x2F, 0x4D, 0x00, 0x22,
0xAA, 0x81, 0x2E, 0x4E, 0x20, 0x3E, 0xB2, 0x83, 0x2A, 0x80, 0x2B, 0x48, 0x5A, 0x21, 0x40, 0x38,
0x01, 0x80, 0x81, 0x15, 0x81, 0x80, 0x0B, 0x21, 0x01, 0x81, 0x2C, 0x49, 0x81, 0x81, 0x14, 0x20,
0xFF, 0xF7, 0x60, 0xFF, 0x2A, 0x4B, 0x01, 0x20, 0x18, 0x80, 0x02, 0x20, 0xFF, 0xF7, 0x5A, 0xFF,
0x8D, 0x20, 0x18, 0x80, 0x9A, 0x80, 0xFF, 0x20, 0x98, 0x82, 0x03, 0x20, 0x00, 0x02, 0x18, 0x82,
0xFC, 0x20, 0x98, 0x83, 0x22, 0x49, 0x95, 0x20, 0x20, 0x31, 0x08, 0x80, 0x1C, 0x4C, 0x0C, 0x20,
0x22, 0x80, 0xA8, 0x81, 0x20, 0x20, 0xB0, 0x83, 0x28, 0x80, 0xAA, 0x81, 0x04, 0x26, 0xA8, 0x89,
0x30, 0x43, 0xA8, 0x81, 0x20, 0x88, 0x01, 0x28, 0x1B, 0xD1, 0x61, 0x88, 0x80, 0x03, 0xA2, 0x88,
0x08, 0x18, 0x51, 0x18, 0x8B, 0xB2, 0x00, 0x21, 0x04, 0xE0, 0x0F, 0x19, 0x3F, 0x7A, 0xFB, 0x18,
0x9B, 0xB2, 0x49, 0x1C, 0x8A, 0x42, 0xF8, 0xD8, 0xE1, 0x88, 0x27, 0x46, 0x99, 0x42, 0x01, 0xD0,
0x08, 0x20, 0x0B, 0xE0, 0x00, 0x2A, 0x08, 0xD0, 0x09, 0x49, 0x08, 0x31, 0xFF, 0xF7, 0x35, 0xFF,
0x38, 0x80, 0xA8, 0x89, 0xB0, 0x43, 0xA8, 0x81, 0xD9, 0xE7, 0x02, 0x20, 0x20, 0x80, 0xD6, 0xE7,
0x10, 0x27, 0x00, 0x00, 0x00, 0x5C, 0x00, 0x40, 0x40, 0x30, 0x00, 0x40, 0x20, 0x6C, 0x00, 0x40,
0x00, 0x04, 0x00, 0x20, 0xFF, 0x0F, 0x00, 0x00, 0x80, 0xE1, 0x00, 0xE0, 0x04, 0x1D, 0x00, 0x00,
0x00, 0x64, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static int p9220_reg_read(struct i2c_client *client, u16 reg, u8 *val)
{
	int ret;
	struct i2c_msg msg[2];
	u8 wbuf[2];
	u8 rbuf[2];

	msg[0].addr = client->addr;
	msg[0].flags = client->flags & I2C_M_TEN;
	msg[0].len = 2;
	msg[0].buf = wbuf;

	wbuf[0] = (reg & 0xFF00) >> 8;
	wbuf[1] = (reg & 0xFF);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = rbuf;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0)
		pr_err("%s: i2c transfer fail", __func__);
	pr_err("%s: reg = 0x%x, val = 0x%x\n", __func__, reg, rbuf[0]);
	*val = rbuf[0];

	return ret;
}

static int p9220_reg_multi_read(struct i2c_client *client, u16 reg, u8 *val, int size)
{
	int ret;
	struct i2c_msg msg[2];
	u8 wbuf[2];

	pr_err("%s: reg = 0x%x, size = 0x%x\n", __func__, reg, size);
	msg[0].addr = client->addr;
	msg[0].flags = client->flags & I2C_M_TEN;
	msg[0].len = 2;
	msg[0].buf = wbuf;

	wbuf[0] = (reg & 0xFF00) >> 8;
	wbuf[1] = (reg & 0xFF);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = size;
	msg[1].buf = val;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0)
		pr_err("%s: i2c transfer fail", __func__);

	return ret;
}

static int p9220_reg_write(struct i2c_client *client, u16 reg, u8 val)
{
	int ret;
	unsigned char data[3] = { reg >> 8, reg & 0xff, val };

	ret = i2c_master_send(client, data, 3);
	if (ret < 3) {
		dev_err(&client->dev, "%s: i2c write error, reg: 0x%x, ret: %d\n",
				__func__, reg, ret);
		return ret < 0 ? ret : -EIO;
	}

	return 0;
}
#if 0
static int p9220_reg_multi_read(struct i2c_client *client, u16 reg, u8 *val, int size)
{
	int ret;
	/* We have 16-bit i2c addresses - care for endianess */
	unsigned char data[2] = { reg >> 8, reg & 0xff };

	ret = i2c_master_send(client, data, 2);
	if (ret < 2) {
		dev_err(&client->dev, "%s: i2c read error, reg: %x\n",
				__func__, reg);
		return ret < 0 ? ret : -EIO;
	}

	ret = i2c_master_recv(client, val, size);
	if (ret < size) {
		dev_err(&client->dev, "%s: i2c read error, reg: %x\n",
				__func__, reg);
		return ret < 0 ? ret : -EIO;
	}
	return 0;
}
#endif
static int p9220_reg_multi_write(struct i2c_client *client, u16 reg, const u8 * val, int size)
{
	int ret;
	const int sendsz = 16;
	unsigned char data[sendsz];
	int cnt = 0;

	dev_err(&client->dev, "%s: size: 0x%x\n",
				__func__, size);
	while(size > sendsz) {
		data[0] = (reg+cnt) >>8;
		data[1] = (reg+cnt) & 0xff;
		memcpy(data+2, val+cnt, sendsz);
		dev_err(&client->dev, "%s: addr: 0x%x, cnt: 0x%x\n", __func__, reg+cnt, cnt);
		ret = i2c_master_send(client, data, sendsz+2);
		if (ret < sendsz+2) {
			dev_err(&client->dev, "%s: i2c write error, reg: 0x%x\n",
					__func__, reg);
			return ret < 0 ? ret : -EIO;
		}
		cnt = cnt + sendsz;
		size = size - sendsz;
	}
	if (size > 0) {
		data[0] = (reg+cnt) >>8;
		data[1] = (reg+cnt) & 0xff;
		memcpy(data+2, val+cnt, size);
		dev_err(&client->dev, "%s: addr: 0x%x, cnt: 0x%x, size: 0x%x\n", __func__, reg+cnt, cnt, size);
		ret = i2c_master_send(client, data, size+2);
		if (ret < size+2) {
			dev_err(&client->dev, "%s: i2c write error, reg: 0x%x\n",
					__func__, reg);
			return ret < 0 ? ret : -EIO;
		}
	}

	return 0;
}
/*
static int p9220_reg_multi_write(struct i2c_client *client, u16 reg, const u8 * val, int size)
{
	int ret;
	unsigned char * data = kmalloc(size, GFP_KERNEL);

	if (data == NULL) {
		dev_err(&client->dev, "%s: kmalloc fail\n",
				__func__);
		return -1;
	}
	dev_err(&client->dev, "%s: size: 0x%x\n",
				__func__, size);
	data[0] = reg >>8;
	data[1] = reg & 0xff;
	memcpy(data+2, val, size);
	ret = i2c_master_send(client, data, size+2);
	kfree(data);
	if (ret < size+2) {
		dev_err(&client->dev, "%s: i2c write error, reg: 0x%x\n",
				__func__, reg);
		return ret < 0 ? ret : -EIO;
	}

	return 0;
}
*/

static int datacmp(const char *cs, const char *ct, int count)
{
	unsigned char c1, c2;

	while (count) {
		c1 = *cs++;
		c2 = *ct++;
		if (c1 != c2) {
			pr_err("%s, cnt %d", __func__, count);
			return c1 < c2 ? -1 : 1;
		}
		count--;
	}
	return 0;
}
static int LoadOTPLoaderInRAM(u16 addr)
{
	int i, size;
	u8 data[1024];
	if (p9220_reg_multi_write(p9220_i2c, addr, OTPBootloader, sizeof(OTPBootloader)) < 0) {
		pr_err("%s,fail", __func__);
	}
	size = sizeof(OTPBootloader);
	i = 0;
	while(size > 0) {
		pr_err("%s, 0x%x: , 0x%x", __func__, addr+i, size);
		if (p9220_reg_multi_read(p9220_i2c, addr+i, data+i, 16) < 0) {
			pr_err("%s, read failed(%d)", __func__, addr+i);
			return 0;
		}
		i += 16;
		size -= 16;
	}
	i = 0;
	for (i = 0 ; i < sizeof(OTPBootloader); i++) {
		if (i%16 == 0)
			pr_err("addr = 0x%x : ", addr+i);
		pr_err(" 0x%x,", data[i]);
		if (i%16 == 15)
			pr_err("\n");
	}

	if (datacmp(data, OTPBootloader, sizeof(OTPBootloader))) {
		pr_err("%s, data is not matched", __func__);
		return 0;
	}
	return 1;
}
static int PgmOTPwRAM(unsigned short OtpAddr, const u8 * srcData, int srcOffs, int size)
{
	//DateTime pgmSt = DateTime.Now;
	int i, j, cnt;

	pr_info("%s Start ---------------------------------------- \n",__func__);
	if (!p9220_i2c) {
		pr_err("%s: i2c is not probed\n", __func__);
		return false;		// write key
	}
	/*
	   if (!ConvertStr2Num(comboBoxDeviceI2cAddr.Text, out i2cDeviceAddr))
	   {
	   addText("ERROR: I2C Device Address Invalid");
	   return false;
	   }
	   addText("");
	 */
	//ignoreNAK = false;		// restore to default in case previous call did not finish
	// configure the system
	if (p9220_reg_write(p9220_i2c, 0x3000, 0x5a) < 0) {
		pr_err("%s: write key error\n", __func__);
		return false;		// write key
	}
	if (p9220_reg_write(p9220_i2c, 0x3040, 0x10) < 0) {
		pr_err("%s: halt M0 error\n", __func__);
		return false;		// halt M0
	}
	if (!LoadOTPLoaderInRAM(0x1c00)){
		pr_err("%s: LoadOTPLoaderInRAM error\n", __func__);
		return false;		// make sure load address and 1KB size are OK
	}

	if (p9220_reg_write(p9220_i2c, 0x3048, 0x80) < 0) {
		pr_err("%s: map RAM to OTP error\n", __func__);
		return false;		// map RAM to OTP
	}
	//if (!Write9220Byte(0x3008, 0x20)) {return false;		// ahb lower
	//ignoreNAK = true;
	//if (!Write9220Byte(0x3040, 0x80)) return false;		// run M0
	p9220_reg_write(p9220_i2c, 0x3040, 0x80);
	/*
	if (p9220_reg_write(p9220_i2c, 0x3040, 0x80) < 0) {
		pr_err("%s: run M0 error\n", __func__);
		return false;		// halt M0
	}
	*/
	//ignoreNAK = false;

	msleep(100);

	for (i = 0; i < size; i += 128)		// program pages of 128 bytes
	{
		u8 sBuf[136] = {0,};
		u16 StartAddr = (u16)i;
		u16 CheckSum = StartAddr;
		u16 CodeLength = 128;
		//Array.Copy(srcData, i + srcOffs, sBuf, 8, 128); //(src, srcoffset, dst, dstoffset, size)
		memcpy(sBuf + 8, srcData + i + srcOffs, 128);

		for (j = 127; j >= 0; j--)
		{
			if (sBuf[j + 8] != 0)
				break;
			else
				CodeLength--;
		}
		if (CodeLength == 0)
			continue;				// skip programming if nothing to program
		for (; j >= 0; j--)
			CheckSum += sBuf[j + 8];	// add the non zero values
		CheckSum += CodeLength;			// finish calculation of the check sum

		//Array.Copy(BitConverter.GetBytes(StartAddr), 0, sBuf, 2, 2);
		memcpy(sBuf+2, &StartAddr,2);
		//Array.Copy(BitConverter.GetBytes(CodeLength), 0, sBuf, 4, 2);
		memcpy(sBuf+4, &CodeLength,2);
		//Array.Copy(BitConverter.GetBytes(CheckSum), 0, sBuf, 6, 2);
		memcpy(sBuf+6, &CheckSum,2);

		//typedef struct {	// write to structure at address 0x400
		//		u16 Status;
		//		u16 StartAddr;
		//		u16 CodeLength;
		//		u16 DataChksum;
		//		u8  DataBuf[128];
		//}  P9220PgmStrType;

		// read status is guaranteed to be != 1 at this point

		//if (!Xfer9220i2c(p9220_i2c, 0x400, 0x100, sBuf, 0, CodeLength + 8,ReadWrite.WRITE))
		if (p9220_reg_multi_write(p9220_i2c, 0x400, sBuf, CodeLength + 8) < 0)
		{
			pr_err("ERROR: on writing to OTP buffer");
			return false;
		}
		sBuf[0] = 1;
		//if (!Xfer9220i2c(i2cDeviceAddr, 0x400, 0x100, sBuf, 0, 1, ReadWrite.WRITE))
		if (p9220_reg_write(p9220_i2c, 0x400, sBuf[0]) < 0)
		{
			pr_err("ERROR: on OTP buffer validation");
			return false;
		}

		//DateTime startT = DateTime.Now;
		cnt = 0;
		do
		{
			msleep(20);
			//if (!Xfer9220i2c(i2cDeviceAddr, 0x400, 0x100, sBuf, 0, 1, ReadWrite.READ))
			if (p9220_reg_read(p9220_i2c, 0x400, sBuf) < 0)
			{
				pr_err("ERROR: on readign OTP buffer status(%d)", cnt);
				return false;
			}
			/*
			   TimeSpan pas = DateTime.Now - startT;
			   if (pas.TotalMilliseconds > 5000)
			   {
			   pr_err("ERROR: time out on buffer program to OTP");
			   return false;
			   }
			 */
			 if (cnt > 1000) {
				 pr_err("ERROR: time out on buffer program to OTP");
				 break;
			 }
			 cnt++;
		} while (sBuf[0] == 1);

		// check status
		if (sBuf[0] != 2)		// not OK
		{
			pr_err("ERROR: buffer write to OTP returned status %d ",sBuf[0]);
			return false;
		}
	}

	// restore system
	if (p9220_reg_write(p9220_i2c, 0x3000, 0x5a) < 0) {
		pr_err("%s: write key error..\n", __func__);
		return false;		// write key
	}
	if (p9220_reg_write(p9220_i2c, 0x3048, 0x00) < 0) {
		pr_err("%s: remove code remapping error..\n", __func__);
		return false;		// remove code remapping
	}
	//if (!Write9220Byte(0x3040, 0x80)) return false;		// reset M0

	//	TimeSpan pgmSp = DateTime.Now - pgmSt;
	pr_err("OTP Programming finished in");
	pr_info("%s------------------------------------------------- \n", __func__);
	return true;
}

#define P9220S_FW_SDCARD_BIN_PATH "sdcard/p9220_otp.bin"
#define P9220S_FW_HEX_PATH "idt/p9220_otp.fw"
#define SDCARD					0
#define BUILT_IN				1

int p9220_otp_update = 0;

int p9220_firmware_update(struct p9220_charger_data *charger, int cmd)
{
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int ret = 0;
    const u8 *fw_img;

	pr_info("%s firmware update mode is = %d \n", __func__, cmd);

	switch(cmd) {
		case SDCARD:
		old_fs = get_fs();
		set_fs(KERNEL_DS);

		fp = filp_open(P9220S_FW_SDCARD_BIN_PATH, O_RDONLY, S_IRUSR);

		if (IS_ERR(fp)) {
			pr_err("%s: failed to open %s\n", __func__, P9220S_FW_SDCARD_BIN_PATH);
			ret = -ENOENT;
			set_fs(old_fs);
			return ret;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		pr_err("%s: start, file path %s, size %ld Bytes\n",
			__func__, P9220S_FW_SDCARD_BIN_PATH, fsize);

		fw_img = kmalloc(fsize, GFP_KERNEL);

		if (fw_img == NULL) {
			pr_err("%s, kmalloc failed\n", __func__);
			ret = -EFAULT;
			goto malloc_error;
		}

		nread = vfs_read(fp, (char __user *)fw_img,
					fsize, &fp->f_pos);
		pr_err("nread %ld Bytes\n", nread);
		if (nread != fsize) {
			pr_err("failed to read firmware file, nread %ld Bytes\n", nread);
			ret = -EIO;
			goto read_err;
		}

		filp_close(fp, current->files);
		set_fs(old_fs);
		p9220_otp_update = 1;
		PgmOTPwRAM(0 ,fw_img, 0, fsize);
		p9220_otp_update = 0;

		kfree(fw_img);
		break;
	case BUILT_IN:
		dev_err(&p9220_i2c->dev, "%s, request_firmware\n", __func__);
		ret = request_firmware(&charger->firm_data_bin, P9220S_FW_HEX_PATH,
			&p9220_i2c->dev);
		if ( ret < 0) {
			dev_err(&p9220_i2c->dev, "%s: failed to request firmware %s (%d) \n", __func__, P9220S_FW_HEX_PATH, ret);
			return -EINVAL;
		}
		pr_info("%s data size = %ld \n", __func__, charger->firm_data_bin->size);
		p9220_otp_update = 1;
		PgmOTPwRAM(0 ,charger->firm_data_bin->data, 0, charger->firm_data_bin->size);
		p9220_otp_update = 0;
		release_firmware(charger->firm_data_bin);
		break;
	default:
		return -1;
		break;
	}

	pr_info("%s --------------------------------------------------------------- \n", __func__);

	return 0;

read_err:
	kfree(fw_img);
malloc_error:
	filp_close(fp, current->files);
	set_fs(old_fs);
	return ret;
}

void p9220_wireless_chg_init(struct p9220_charger_data *charger)
{
	pr_info("%s ------------wireless chg init------------- \n", __func__);

}

static void p9220_detect_work(
		struct work_struct *work)
{
	struct p9220_charger_data *charger =
		container_of(work, struct p9220_charger_data, wpc_work.work);

	pr_info("%s\n", __func__);

	p9220_wireless_chg_init(charger);
}

static int p9220_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	//struct p9220_charger_data *charger =
	//	container_of(psy, struct p9220_charger_data, psy_chg);

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
		case POWER_SUPPLY_PROP_CHARGE_TYPE:
		case POWER_SUPPLY_PROP_HEALTH:
		case POWER_SUPPLY_PROP_ONLINE:
		case POWER_SUPPLY_PROP_CURRENT_NOW:
		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		case POWER_SUPPLY_PROP_CHARGE_POWERED_OTG_CONTROL:
			return -ENODATA;
		default:
			return -EINVAL;
	}
	return 0;
}

static int p9220_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct p9220_charger_data *charger =
		container_of(psy, struct p9220_charger_data, psy_chg);

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			break;
		case POWER_SUPPLY_PROP_ONLINE:
			if(val->intval == POWER_SUPPLY_TYPE_WIRELESS) {
				p9220_wireless_chg_init(charger);
			}
			break;
		case POWER_SUPPLY_PROP_CHARGE_POWERED_OTG_CONTROL:
			p9220_firmware_update(charger, val->intval);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

#if 0
static void p9220_wpc_isr_work(struct work_struct *work)
{
	//struct bq51221_charger_data *charger =
	//	container_of(work, struct bq51221_charger_data, isr_work.work);

	pr_info("%s \n",__func__);
}

static irqreturn_t p9220_wpc_irq_thread(int irq, void *irq_data)
{
	struct p9220_charger_data *charger = irq_data;

	pr_info("%s \n",__func__);
		schedule_delayed_work(&charger->isr_work, 0);

	return IRQ_HANDLED;
}
#endif

static int p9220_chg_parse_dt(struct device *dev,
		p9220_charger_platform_data_t *pdata)
{
	int ret = 0;
	struct device_node *np = dev->of_node;
//	enum of_gpio_flags irq_gpio_flags;

	if (!np) {
		pr_info("%s: np NULL\n", __func__);
		return 1;
	}

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s np NULL\n", __func__);
	} else {

		ret = of_property_read_string(np,
			"battery,wirelss_charger_name", (char const **)&pdata->wireless_charger_name);
		if (ret)
			pr_info("%s: Vendor is Empty\n", __func__);
	}

#if 0
	ret = pdata->irq_gpio = of_get_named_gpio_flags(np, "p9220-charger,irq-gpio",
			0, &irq_gpio_flags);
	if (ret < 0) {
		dev_err(dev, "%s : can't get irq-gpio\r\n", __FUNCTION__);
		return ret;
	}
	pr_info("%s irq_gpio = %d \n",__func__, pdata->irq_gpio);
	pdata->irq_base = gpio_to_irq(pdata->irq_gpio);

#endif

	return ret;
}

static int p9220_charger_probe(
						struct i2c_client *client,
						const struct i2c_device_id *id)
{
	struct device_node *of_node = client->dev.of_node;
	struct p9220_charger_data *charger;
	p9220_charger_platform_data_t *pdata = client->dev.platform_data;
	int ret = 0;

	dev_info(&client->dev,
		"%s: p9220 Charger Driver Loading\n", __func__);

	if (of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
		ret = p9220_chg_parse_dt(&client->dev, pdata);
		if (ret < 0)
			goto err_parse_dt;
	} else {
		pdata = client->dev.platform_data;
	}

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (charger == NULL) {
		dev_err(&client->dev, "Memory is not enough.\n");
		ret = -ENOMEM;
		goto err_wpc_nomem;
	}
	charger->dev = &client->dev;

	ret = i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA |
		I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_I2C_BLOCK);
	if (!ret) {
		ret = i2c_get_functionality(client->adapter);
		dev_err(charger->dev, "I2C functionality is not supported.\n");
		ret = -ENOSYS;
		goto err_i2cfunc_not_support;
	}

	charger->client = client;
	p9220_i2c = client;
	charger->pdata = pdata;

    pr_info("%s: %s\n", __func__, charger->pdata->wireless_charger_name );

#if 0
	/* if board-init had already assigned irq_base (>=0) ,
	no need to allocate it;
	assign -1 to let this driver allocate resource by itself*/

    if (pdata->irq_base < 0)
        pdata->irq_base = irq_alloc_descs(-1, 0, p9220_EVENT_IRQ, 0);
	if (pdata->irq_base < 0) {
		pr_err("%s: irq_alloc_descs Fail! ret(%d)\n",
				__func__, pdata->irq_base);
		ret = -EINVAL;
		goto irq_base_err;
	} else {
		charger->irq_base = pdata->irq_base;
		pr_info("%s: irq_base = %d\n",
			 __func__, charger->irq_base);

#if (LINUX_VERSION_CODE>=KERNEL_VERSION(3,4,0))
		irq_domain_add_legacy(of_node, p9220_EVENT_IRQ, charger->irq_base, 0,
				      &irq_domain_simple_ops, NULL);
#endif /*(LINUX_VERSION_CODE>=KERNEL_VERSION(3,4,0))*/
	}
#endif

	i2c_set_clientdata(client, charger);

	charger->psy_chg.name		= pdata->wireless_charger_name;
	charger->psy_chg.type		= POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property	= p9220_chg_get_property;
	charger->psy_chg.set_property	= p9220_chg_set_property;
	charger->psy_chg.properties	= sec_charger_props;
	charger->psy_chg.num_properties	= ARRAY_SIZE(sec_charger_props);

	mutex_init(&charger->io_lock);

#if 0
	if (charger->wpc_irq) {
		INIT_DELAYED_WORK(
			&charger->isr_work, p9220_wpc_isr_work);

		ret = request_threaded_irq(charger->wpc_irq,
				NULL, p9220_wpc_irq_thread,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"charger-irq", charger);
		if (ret) {
			dev_err(&client->dev,
				"%s: Failed to Reqeust IRQ\n", __func__);
			goto err_supply_unreg;
		}

		ret = enable_irq_wake(charger->wpc_irq);
		if (ret < 0)
			dev_err(&client->dev,
				"%s: Failed to Enable Wakeup Source(%d)\n",
				__func__, ret);
	}
#endif

	ret = power_supply_register(&client->dev, &charger->psy_chg);
	if (ret) {
		dev_err(&client->dev,
			"%s: Failed to Register psy_chg\n", __func__);
		goto err_supply_unreg;
	}

	charger->wqueue = create_workqueue("p9220_workqueue");
	if (!charger->wqueue) {
		pr_err("%s: Fail to Create Workqueue\n", __func__);
		goto err_pdata_free;
	}

	wake_lock_init(&(charger->wpc_wake_lock), WAKE_LOCK_SUSPEND,
			"wpc_wakelock");
	INIT_DELAYED_WORK(&charger->wpc_work, p9220_detect_work);

	dev_info(&client->dev,
		"%s: p9220 Charger Driver Loaded\n", __func__);

	return 0;

err_pdata_free:
	power_supply_unregister(&charger->psy_chg);
err_supply_unreg:
	mutex_destroy(&charger->io_lock);
err_i2cfunc_not_support:
//irq_base_err:
	kfree(charger);
err_wpc_nomem:
err_parse_dt:
	kfree(pdata);
	return ret;
}

static int p9220_charger_remove(struct i2c_client *client)
{
	return 0;
}

#if defined CONFIG_PM
static int p9220_charger_suspend(struct i2c_client *client,
				pm_message_t state)
{


	return 0;
}

static int p9220_charger_resume(struct i2c_client *client)
{


	return 0;
}
#else
#define p9220_charger_suspend NULL
#define p9220_charger_resume NULL
#endif

static void p9220_charger_shutdown(struct i2c_client *client)
{
	int dummy;
	u8 dummmmy;

	dummy = 0;
	if (dummy) {
			dummy = p9220_reg_write(client, 0x05, 0x05);
			dummy = p9220_reg_read(client, 0x05, &dummmmy);
	}

}

static const struct i2c_device_id p9220_charger_id_table[] = {
	{ "p9220-charger", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, p9220_id_table);

#ifdef CONFIG_OF
static struct of_device_id p9220_charger_match_table[] = {
	{ .compatible = "idt,p9220-charger",},
	{},
};
#else
#define p9220_charger_match_table NULL
#endif

static struct i2c_driver p9220_charger_driver = {
	.driver = {
		.name	= "p9220-charger",
		.owner	= THIS_MODULE,
		.of_match_table = p9220_charger_match_table,
	},
	.shutdown	= p9220_charger_shutdown,
	.suspend	= p9220_charger_suspend,
	.resume		= p9220_charger_resume,
	.probe	= p9220_charger_probe,
	.remove	= p9220_charger_remove,
	.id_table	= p9220_charger_id_table,
};

static int __init p9220_charger_init(void)
{
	pr_info("%s \n",__func__);
	return i2c_add_driver(&p9220_charger_driver);
}

static void __exit p9220_charger_exit(void)
{
	pr_info("%s \n",__func__);
	i2c_del_driver(&p9220_charger_driver);
}

module_init(p9220_charger_init);
module_exit(p9220_charger_exit);

MODULE_DESCRIPTION("Samsung p9220 Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
