/*
 * VNR driver
 *
 * Copyright (C) 2013 Mtekvision
 * Author: Sunki <kimska@mtekvision.com>
 *
 * Some parts borrowed from various video4linux drivers, especially
 * mvzenith-files, bttv-driver.c and zoran.c, see original files for credits.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __ODIN_3DNR_H__
#define __ODIN_3DNR_H__

/***************************************************************************/
/* Local Literals & definitions */
/***************************************************************************/
typedef struct {
	unsigned int	input_hmask;
	unsigned int	input_vmask;
	int				fd_ref2;
	int				fd_ref1;
	int				fd_cur;
	int				fd_result;
	unsigned int 	reserved;
}vnr_param;

typedef struct {
	vnr_param				param;
	struct ion_client		*ion_client;
	struct ion_handle_data	ion_hld_ref2;
	struct ion_handle_data	ion_hld_ref1;
	struct ion_handle_data	ion_hld_cur;
	struct ion_handle_data	ion_hld_result;

	int 					irq;
	spinlock_t				slock;
	struct completion		completion;

	struct device			*dev;
	void __iomem			*regs;

	int						status;
	int 					open;
	struct mutex			mutex;
}vnr_ctx;

/* VNR register's offsets */
#define REG_VNR_STATUS					0x0000
#define REG_VNR_CONTROL					0x0004
#define REG_VNR_CH_CTRL					0x0008
#define REG_VNR_DMA_TRANS_SIZE			0x000C
#define REG_VNR_RESOL_SIZE 				0x0010
#define REG_VNR_CACHE_TYPE 				0x0014
#define REG_VNR_LUT_SEL					0x0018
#define REG_VNR_YCBCR_SEL				0x001C
#define REG_VNR_RESULT_BASE				0x0040
#define REG_VNR_CURR_BASE				0x0044
#define REG_VNR_REF_BASE0				0x0048
#define REG_VNR_REF_BASE1				0x004C

#define REG_LUT_DATA_WPORT_ENTRY1		0x0400
#define REG_LUT_DATA_WPORT_ENTRY256		0x07FC

/* VNR register's bit-fields */

/* STATUS */
#define STAT_R2BUSY		(0x1 << 23)
#define STAT_R1BUSY		(0x1 << 22)
#define STAT_R0BUSY		(0x1 << 21)
#define STAT_WBUSY		(0x1 << 20)
#define STAT_R2DONE		(0x1 << 19)
#define STAT_R1DONE		(0x1 << 18)
#define STAT_R0DONE		(0x1 << 17)
#define STAT_WDDONE		(0x1 << 16)
#define STAT_BUSY		(0x2)
#define STAT_DONE		(0x1)


/* CONTROL */
#define CTRL_RUN		(0x1)

/* CHANNEL CONTROL */
#define CH_CTRL_R2EN	(0x8)
#define CH_CTRL_R1EN	(0x4)
#define CH_CTRL_R0EN	(0x2)
#define CH_CTRL_WEN		(0x1)

/* CACHE TYPE */
#define CACHE_ARCHCHE_TYPE	(0x3<<4)
#define CACHE_AWCHCHE_TYPE	0x3

enum odin_3dnr_is_status {
	VNR_IDLE = 0,
	VNR_READY,
	VNR_BUSY,
	VNR_DONE,
	VNR_ERROR,
	VNR_END,
};

enum odin_3dnr_error_status {
	CSS_ERROR_VNR_INIT = 10,
	CSS_ERROR_VNR_MASK_SIZE,
	CSS_ERROR_VNR_BUSY,
	CSS_ERROR_VNR_TIMEOUT,
	CSS_ERROR_VNR_IRQ,
};

#define IOCTL_VNR_MAGIC 		'R'
#define IOCTL_VNR_PARAM			_IOW(IOCTL_VNR_MAGIC, 0, vnr_param)
#define IOCTL_VNR_ERROR_GET		_IOR(IOCTL_VNR_MAGIC, 1, int)
#define IOCTL_VNR_MAXNR			2

#endif /* __ODIN_3DNR_H__ */
