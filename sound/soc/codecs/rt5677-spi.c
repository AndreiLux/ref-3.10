/*
 * rt5677-spi.c  --  RT5677 ALSA SoC audio codec driver
 *
 * Copyright 2013 Realtek Semiconductor Corp.
 * Author: Oder Chiou <oder_chiou@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_qos.h>
#include <linux/sysfs.h>
#include <linux/clk.h>
#include "rt5677-spi.h"

#define SPI_BURST_LEN		240
#define SPI_HEADER		5
#define RT5677_SPI_WRITE_BURST	0x5
#define RT5677_SPI_WRITE_32	0x3
#define RT5677_SPI_WRITE_16	0x1

static struct spi_device *g_spi;
int rt5677_spi_write(u8 *txbuf, size_t len)
{
	static DEFINE_MUTEX(lock);
	int status;

	mutex_lock(&lock);

	status = spi_write(g_spi, txbuf, len);

	mutex_unlock(&lock);

	if (status)
		dev_err(&g_spi->dev, "rt5677_spi_write error %d\n", status);

	return status;
}

int rt5677_spi_burst_write(u32 addr, u8 *txbuf, size_t len)
{
	unsigned int i, end, offset = 0;
	int status = 0;
	u8 write_buf[SPI_BURST_LEN + SPI_HEADER + 1];
	u8 spi_cmd;

	while (offset < len) {
		switch ((addr + offset) & 0x7) {
		case 4:
			spi_cmd = RT5677_SPI_WRITE_32;
			end = 4;
			break;
		case 2:
			spi_cmd = RT5677_SPI_WRITE_16;
			end = 2;
			break;
		case 0:
			spi_cmd = RT5677_SPI_WRITE_BURST;
			if (offset + SPI_BURST_LEN <= len)
				end = SPI_BURST_LEN;
			else {
				end = len % SPI_BURST_LEN;
				end = (((end - 1) >> 3) + 1) << 3;
			}
			break;
		default:
			pr_err("Bad section alignment\n");
			return -EACCES;
		}

		write_buf[0] = spi_cmd;
		write_buf[1] = ((addr + offset) & 0xff000000) >> 24;
		write_buf[2] = ((addr + offset) & 0x00ff0000) >> 16;
		write_buf[3] = ((addr + offset) & 0x0000ff00) >> 8;
		write_buf[4] = ((addr + offset) & 0x000000ff) >> 0;

		if (spi_cmd == RT5677_SPI_WRITE_BURST) {
			for (i = 0; i < end; i += 8) {
				write_buf[i + 12] = txbuf[offset + i + 0];
				write_buf[i + 11] = txbuf[offset + i + 1];
				write_buf[i + 10] = txbuf[offset + i + 2];
				write_buf[i +  9] = txbuf[offset + i + 3];
				write_buf[i +  8] = txbuf[offset + i + 4];
				write_buf[i +  7] = txbuf[offset + i + 5];
				write_buf[i +  6] = txbuf[offset + i + 6];
				write_buf[i +  5] = txbuf[offset + i + 7];
			}
		} else {
			unsigned int j = end + (SPI_HEADER - 1);
			for (i = 0; i < end; i++, j--) {
				if (offset + i < len)
					write_buf[j] = txbuf[offset + i];
				else
					write_buf[j] = 0;
			}
		}
		write_buf[end + SPI_HEADER] = spi_cmd;

		status |= rt5677_spi_write(write_buf, end + SPI_HEADER + 1);
		offset += end;
	}

	return status;
}

static int rt5677_spi_probe(struct spi_device *spi)
{
	g_spi = spi;
	return 0;
}

static int rt5677_spi_remove(struct spi_device *spi)
{
	return 0;
}

#ifdef CONFIG_PM
static int rt5677_suspend(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);
	struct rt5677_spi_platform_data *pdata;
	pr_info("%s\n", __func__);
	if (spi == NULL) {
		pr_debug("spi_device didn't exist");
		return 0;
	}
	pdata = spi->dev.platform_data;
	if (pdata && (pdata->spi_suspend))
		pdata->spi_suspend(1);
	return 0;
}

static int rt5677_resume(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);
	struct rt5677_spi_platform_data *pdata;
	pr_info("%s\n", __func__);
	if (spi == NULL) {
		pr_debug("spi_device didn't exist");
		return 0;
	}
	pdata = spi->dev.platform_data;
	if (pdata && (pdata->spi_suspend))
		pdata->spi_suspend(0);
	return 0;
}

static const struct dev_pm_ops rt5677_pm_ops = {
	.suspend = rt5677_suspend,
	.resume = rt5677_resume,
};
#endif /*CONFIG_PM */

static struct spi_driver rt5677_spi_driver = {
	.driver = {
			.name = "rt5677_spidev",
			.bus = &spi_bus_type,
			.owner = THIS_MODULE,
#if defined(CONFIG_PM)
			.pm = &rt5677_pm_ops,
#endif
	},
	.probe = rt5677_spi_probe,
	.remove = rt5677_spi_remove,
};

static int __init rt5677_spi_init(void)
{
	return spi_register_driver(&rt5677_spi_driver);
}

static void __exit rt5677_spi_exit(void)
{
	spi_unregister_driver(&rt5677_spi_driver);
}

module_init(rt5677_spi_init);
module_exit(rt5677_spi_exit);

MODULE_DESCRIPTION("ASoC RT5677 driver");
MODULE_AUTHOR("Oder Chiou <oder_chiou@realtek.com>");
MODULE_LICENSE("GPL");

