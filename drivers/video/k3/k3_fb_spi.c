/* Copyright (c) 2013-2014, Hisilicon Tech. Co., Ltd. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
* GNU General Public License for more details.
*
*/

#include "k3_fb.h"


struct spi_device *g_spi_dev = NULL;

static int k3_spi_probe (struct spi_device *spi_dev)
{
	K3_FB_DEBUG("+.\n");

	g_spi_dev = spi_dev;

	spi_dev->bits_per_word = 8;
	spi_dev->mode = SPI_MODE_3;
	spi_setup(spi_dev);

	k3_fb_device_set_status0(DTS_SPI_READY);

	K3_FB_DEBUG("-.\n");

	return 0;
}

static int k3_spi_remove(struct spi_device *spi_dev)
{
	K3_FB_DEBUG("+.\n");
	/*spi_unregister_driver(&spi_driver);*/
	K3_FB_DEBUG("-.\n");

	return 0;
}

static const struct of_device_id k3_spi_match_table[] = {
	{
		.compatible = DEV_NAME_SPI,
		.data = NULL,
	},
};

static struct spi_driver this_driver = {
	.probe = k3_spi_probe,
	.remove = k3_spi_remove,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.driver = {
		.name = DEV_NAME_SPI,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(k3_spi_match_table),
	},
};

static int __init k3_spi_init(void)
{
	int ret = 0;

	ret = spi_register_driver(&this_driver);
	if (ret) {
		K3_FB_ERR("spi_register_driver failed, error=%d!\n", ret);
		return ret;
	}

	return ret;
}

module_init(k3_spi_init);
