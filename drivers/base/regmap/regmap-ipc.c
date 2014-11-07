/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/regmap.h>
#include <linux/regmap-ipc.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/odin_mailbox.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/notifier.h>

#include "internal.h"

#define STORE_BASE			1
#define DATA_BASE			2
#define BULK_BASE			3
#define RCVDATA_BASE		5

#define DATA_LENGTH			4

/* Debug Opetion */
#define REGMAP_DEBUG(d,f, ...) dev_dbg(d,f,##__VA_ARGS__)
/* #define REGMAPIPC_DEBUG */

unsigned int mbox_rcv_data[7];

static int mbox_size_check(size_t val_size)
{
	int length = 0;

	if ((val_size % 4) == 0)
		length = (val_size / 4);
	else
		length = (val_size / 4) + 1;

	return length;
}

static int loop_size_ckeck(size_t val_size)
{
	if (val_size > 4)
		val_size = 4;
	else
		val_size = (val_size % 4);

	return val_size;
}

static int regmap_ipc_write(void *context, const void *data, size_t count)
{
	struct ipc_client *ipc = context;
	unsigned int mbox_data[7], store_data = 0;
	int ret = 0;
	int i, j, shift = 0;
	int length = 1, recheck = 0;
	size_t val_of_count;

	store_data = ((WRITE_CMD << 24) | (ipc->slot << 16)
			| (ipc->addr << 8) | count);

	/* Initial mailbox data for i2c writing*/
	/* mailbox command structure */
    /* DATA[0] : The number of command
	 * DATA[1] : I2C write command / I2C Slot & Slave Address / count
	 * DATA[2] : DATA Store from DATA[2] to Data[5]
	 * DATA[6] : REGMAP-IPC irq
	*/
	mbox_data[0] = I2C_CMD;  /* i2c write command */
	mbox_data[1] = store_data;
	mbox_data[6] = REGMAP_IPC;

	val_of_count = count;

	if (count > 16) {
		printk(KERN_ERR "Count number is too long \n");
		return -EINVAL;
	}

	length = mbox_size_check(count);

	for (i = 0 ; i < length ; i++) {
			mbox_data[DATA_BASE + i] = (*((unsigned int *)data+i));
	}

	REGMAP_DEBUG(ipc->dev, "Slot Addr= 0x%x & Slave Addr= 0x%x & Count=%d\n",
		(mbox_data[STORE_BASE] >> 16) & 0xff, (mbox_data[STORE_BASE] >> 8) & 0xff,
		mbox_data[STORE_BASE] & 0xff);

#ifdef REGMAPIPC_DEBUG
	for (j = 0; j < length ; j++) {
		for (i = 0; i < count - 1 ; i++ ) {
			shift += 8;
			REGMAP_DEBUG(ipc->dev, "[(W):%d] Address=0x%x & Data[%d]=0x%x \n",
					i, mbox_data[DATA_BASE + j] & 0xff, i,
					(mbox_data[DATA_BASE + j] >> shift) & 0xff);
		}
		count -=3;
	}
#endif

	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_SD_REGMAP,
				mbox_data, 1000);
	if (ret < 0) {
		printk(KERN_ERR "REGMAP IPC write timed out for writing\n");
		ret = -ETIMEDOUT;
		goto done;
	}

	/* SMS I2C error check */
	if (mbox_data[0] != 0x0) {
		printk(KERN_ERR "SMS I2C(slot = %d, addr = 0x%x) timed out for writing\n",
			ipc->slot, ipc->addr);
		ret = mbox_data[0];
		goto done;
	}

	for ( i = 0 ; i < RCVDATA_BASE; i++) {
		mbox_rcv_data[i] = (*((unsigned int *)mbox_data + i + DATA_BASE));
	}

	for (j = 0; j < length ; j++) {
		for (i = 0 ; i < (val_of_count -1) ; i++) {
			mbox_data[i] = mbox_rcv_data[i];
			recheck += 8;
			REGMAP_DEBUG(ipc->dev,"[W] Confirm Addr[%d] = 0x%x & Data[%d]= 0x%x\n",
				j, mbox_rcv_data[j] & 0xff, i ,
				((mbox_rcv_data[j] >> recheck) & 0xff));
		}
	}

	return 0;

done:
	return ret;
}

static int regmap_ipc_read(void *context, const void *reg_buf, size_t reg_size,
							void *val, size_t val_size)
{
	struct ipc_client *ipc = context;
	unsigned int mbox_data[7], store_data = 0;
	unsigned int data_addr;
	int ret = 0, length;
	int i, init_size, addr = 0;

	store_data = ((READ_CMD << 24) | (ipc->slot << 16) |
			(ipc->addr << 8) | val_size);
	/*
	 * DATA[0] : I2C Task command
	 * DATA[1] : Reda CMD & I2C Slot & Slave Address & Size of I2C
	 * DATA[2] : Data
	 * DATA[6] : REGMAP-IPC IRQ
	*/

	mbox_data[0] = I2C_CMD;  /* i2c command */
	mbox_data[1] = store_data;
	mbox_data[2] = (*((unsigned int*)reg_buf) & 0xff);
	mbox_data[6] = REGMAP_IPC;

	/* Regmap ipc value set */
	length = mbox_size_check(val_size);
	init_size = val_size;
	data_addr = mbox_data[2];

	if (length > 3) {
		printk(KERN_ERR "Mailbox length is too long \n");
		return -EINVAL;
	}

	REGMAP_DEBUG(ipc->dev, "Slot Addr= 0x%x & Slave Addr=0x%x & Count=%d\n",
		(mbox_data[STORE_BASE] >> 16) & 0xff, (mbox_data[STORE_BASE] >> 8) & 0xff,
		mbox_data[STORE_BASE] & 0xff);

	for (i = 0; i < reg_size; i++ ) {
		REGMAP_DEBUG(ipc->dev, "[R:%d] Read Addr = 0x%x \n",
			i, mbox_data[DATA_BASE + i] & 0xff);
	}

	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_SD_REGMAP, mbox_data, 1000);
	if (ret < 0) {
		printk(KERN_ERR "REGMAP IPC read timed out for reading\n");
		ret = -ETIMEDOUT;
		goto done;
	}

	/* SMS I2C error check */
	if (mbox_data[0] != 0x0) {
		printk(KERN_ERR "SMS I2C(slot = %d, addr = 0x%x) timed out for reading\n",
			ipc->slot, ipc->addr);
		ret = mbox_data[0];
		goto done;
	}

	for ( i = 0 ; i < length ; i++) {
		mbox_rcv_data[i] = (*((unsigned int *)mbox_data + i + DATA_BASE));
	}

	for (i = 0 ; i < length; i++) {
		*((unsigned int *)val + i)  = mbox_rcv_data[i];
		REGMAP_DEBUG(ipc->dev, "[R] Addr[%d]= 0x%x DATA[%d]= 0x%x\n",
			i, data_addr + addr, i, mbox_rcv_data[i]);
		addr = i + DATA_LENGTH;
	}

	return 0;
done:
	return ret;
}

/* IPC gather_write */
static int regmap_ipc_gather_write(void *context,
					const void *reg, size_t reg_size,
					const void *val, size_t val_size)
{
	struct device *dev = context;
	struct ipc_client *ipc = context;
	unsigned int mbox_data[7] = {0, }, store_data = 0;
	unsigned int *bulk_val = (unsigned int *)val;
	int i, base = 0;
	int length = 1;
	int ret = 0, recheck = 8, init_size;
#ifdef REGMAPIPC_DEBUG
	int next_val, check_size = 0, j;
#endif

	if (val_size > 16)
		return -EINVAL;

	store_data = ((BULK_WRITE_CMD << 24) | (ipc->slot << 16)
			| (ipc->addr << 8) | val_size);

	/* Initial mailbox data for i2c writing*/
	/* mailbox command structure */
    /* DATA[0] : The number of command
	 * DATA[1] : I2C write command / I2C Slot & Slave Address / count
	 * DATA[2] : Start ADDRESS
	 * DATA[3] : Register value
	 * DATA[6] : REGMAP-IPC irq
	*/
	mbox_data[0] = I2C_CMD;  /* i2c write command */
	mbox_data[1] = store_data;
	mbox_data[2] = ((*(unsigned int *)reg) & 0xff);
	mbox_data[6] = REGMAP_IPC;

	init_size = val_size;
	length = mbox_size_check(val_size);

	REGMAP_DEBUG(ipc->dev, "Slot Addr= 0x%x & Slave Addr= 0x%x & Size = %d\n",
		(mbox_data[STORE_BASE] >> 16) & 0xff, (mbox_data[STORE_BASE] >> 8) & 0xff,
		mbox_data[STORE_BASE] & 0xff);

	REGMAP_DEBUG(ipc->dev, "Start Addr = 0x%x \n", mbox_data[2]);

	for (i = BULK_BASE; i < (length + BULK_BASE); i++) {
		mbox_data[i] = bulk_val[i - BULK_BASE];
		REGMAP_DEBUG(ipc->dev,"SEND DATA[%d] = 0x%x \n", i, mbox_data[i]);
	}

	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_SD_REGMAP,
				mbox_data, 1000);
	if (ret < 0) {
		printk(KERN_ERR "REGMAP IPC write timed out for writing\n");
		ret = -ETIMEDOUT;
		goto done;
	}

	/* SMS I2C error check */
	if (mbox_data[0] != 0x0) {
		printk(KERN_ERR "SMS I2C(slot = %d, addr = 0x%x) timed out for bulk writing\n",
			ipc->slot, ipc->addr);
		ret = mbox_data[0];
		goto done;
	}

	/* init value set */
	init_size = val_size;
	base = 0;
	recheck = 0;

	for (i = 0 ; i < length; i++) {
		mbox_rcv_data[i] = (*((unsigned int *)mbox_data + i + DATA_BASE));
#ifdef REGMAPIPC_DEBUG
		check_size = loop_size_ckeck(init_size);
		if (i > 0 || check_size < 4)
			next_val = 1;
		else
			next_val = 0;

		for (j = 0; j < check_size + next_val; j++) {
			REGMAP_DEBUG(ipc->dev, "[W] Confirm[%d] = 0x%x \n", base + j,
				(mbox_rcv_data[i] >> recheck) & 0xff);
			recheck += 8;
		}
		init_size -= check_size;
		recheck = 0;
		base += 4;
#endif
	}

	return 0;

done:
	return ret;
}

static struct regmap_bus regmap_ipc = {
	.write = regmap_ipc_write,
	.gather_write = regmap_ipc_gather_write,
	.read = regmap_ipc_read,
};

/**
 * regmap_init_ipc(): Initialise register map
 *
 * @ipc: Device that will be interacted with
 * @config: Configuration for register map
 *
 * The return value will be an ERR_PTR() on error or a valid pointer to
 * a struct regmap.
 */
struct regmap *regmap_init_ipc(struct ipc_client *ipc,
						const struct regmap_config *config)
{
	return regmap_init(ipc->dev, &regmap_ipc, ipc, config);
}
EXPORT_SYMBOL_GPL(regmap_init_ipc);

/**
 * devm_regmap_init_ipc(): Initialise managed register map
 *
 * @ipc: Device that will be interacted with
 * @config: Configuration for register map
 *
 * The return value will be an ERR_PTR() on error or a valid pointer
 * to a struct regmap.  The regmap will be automatically freed by the
 * device management code.
 */
struct regmap *devm_regmap_init_ipc(struct ipc_client *ipc,
						const struct regmap_config *config)
{
	return devm_regmap_init(ipc->dev, &regmap_ipc, ipc, config);
}
EXPORT_SYMBOL_GPL(devm_regmap_init_ipc);

MODULE_AUTHOR("In Hyuk Kang <hugh.kang@lge.com>");
MODULE_DESCRIPTION("Odin REGMAP IPC driver");
MODULE_LICENSE("GPL 2.0");
