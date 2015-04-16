/*
**Copyright (c) 2013-2014, Hisilicon Tech. Co., Ltd. All rights reserved.
**
**This program is free software; you can redistribute it and/or modify
**it under the terms of the GNU General Public License version 2 and
**only version 2 as published by the Free Software Foundation.
**
**This program is distributed in the hope that it will be useful,
**but WITHOUT ANY WARRANTY; without even the implied warranty of
**MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
**GNU General Public License for more details.
**
*/

#ifndef K3_RGB2MIPI_H
#define K3_RGB2MIPI_H

#include "k3_mipi_dsi.h"

struct rgb2mipi_spi_cmd_desc {
	unsigned short reg;
	unsigned char value;
	int delay;
	int delaytype;

};


int rgb2mipi_cmds_tx(struct spi_device *spi_dev, struct dsi_cmd_desc *cmds, int cnt);


#endif /* K3_RGB2MIPI_H */
