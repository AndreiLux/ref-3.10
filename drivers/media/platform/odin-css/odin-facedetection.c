/*
 * Face Detection driver
 *
 * Copyright (C) 2013 Mtekvision
 * Author: Eonkyong Lee <tiny@mtekvision.com>
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

#include <linux/interrupt.h>
#include <linux/dma-mapping.h>

#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#if CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#endif

#include "odin-css.h"
#include "odin-css-system.h"
#include "odin-css-clk.h"
#include "odin-facedetection.h"
#include "odin-framegrabber.h"
#ifdef USE_FPD_HW_IP
#include "odin-facialpartsdetection.h"
#endif

#ifdef FD_USE_TEST_IMAGE
#include "./test/fd_image_long.h"
#endif

/* #ifdef FD_PROFILING */
#include "odin-css-utils.h"
/* #endif */

#ifdef FD_USE_TEST_IMAGE
void *fd_image_cpu_addr[FACE_BUF_MAX];
dma_addr_t fd_image_dma_addr[FACE_BUF_MAX];
int fd_image_buf_index;
#endif

#ifdef FD_PROFILING
CSS_TIME_DEFINE(fd_run_time);
static int time_count = 0;
#endif

static struct fd_hardware *g_fd_hw = NULL;

/* static FACE_DETECTION_REGISTERS fd_reg_data; */
struct face_info *p_fd_info;

static inline void set_fd_hw(struct fd_hardware *hw)
{
	g_fd_hw = hw;
}

static inline struct fd_hardware *get_fd_hw(void)
{
	BUG_ON(!g_fd_hw);
	return g_fd_hw;
}

static inline struct css_fd_device *get_fd_device(void)
{
	return hw_to_plat(get_fd_hw(), fd_hw)->fd_device;
}

static inline struct scaler_hardware *get_scl_hw(void)
{
	return &(hw_to_plat(get_fd_hw(), fd_hw)->scl_hw);
}

void fd_interrupt_enable(int enable)
{
	struct fd_hardware *fd_hw = get_fd_hw();

	if (enable)
		writel(0x1, fd_hw->iobase + REG_FD_INTR_ENABLE);
	else
		writel(0x0, fd_hw->iobase + REG_FD_INTR_ENABLE);
}

void fd_interrupt_clear(void)
{
	struct fd_hardware *fd_hw = get_fd_hw();

	writel(0x1, fd_hw->iobase + REG_FD_INTR_CLEAR);
}

void fd_get_face_info(struct css_face_detect_info *info, unsigned int count)
{
	struct fd_hardware *fd_hw = get_fd_hw();
	FD_FACE face;

	face.as32bits[0] = readl(fd_hw->iobase + REG_FD_FACE0_X + count * 16);
	face.as32bits[1] = readl(fd_hw->iobase + REG_FD_FACE0_Y + count * 16);
	face.as32bits[2] = readl(fd_hw->iobase + REG_FD_FACE0_SIZE + count * 16);
	face.as32bits[3] = readl(fd_hw->iobase + REG_FD_FACE0_ANGLE + count * 16);

	info->center_x = face.asfield.center_x;
	info->center_y = face.asfield.center_y;
	info->size = face.asfield.size;
	info->confidence = face.asfield.confidence;
	info->angle = face.asfield.angle;
	info->pose = face.asfield.pose;
}

int fd_get_control(void)
{
	struct fd_hardware *fd_hw = get_fd_hw();
	int control;

	control = readl(fd_hw->iobase + REG_FD_CONTROL);

	return control;
}

int fd_get_face_count(void)
{
	struct fd_hardware *fd_hw = get_fd_hw();
	int count;

	count = readl(fd_hw->iobase + REG_FD_FACE_COUNT);

	return count;
}

unsigned int fd_get_src_addr(void)
{
	struct fd_hardware *fd_hw = get_fd_hw();
	unsigned int offset;

	offset = readl(fd_hw->iobase + REG_FD_SRCBASE_ADDR);

	return offset;
}

unsigned int fd_get_intr_status(void)
{
	struct fd_hardware *fd_hw = get_fd_hw();
	FD_INTERRUPT interrupt;

	interrupt.as32bits = readl(fd_hw->iobase + REG_FD_INTR_STATUS);
	interrupt.as32bits = interrupt.as32bits & 0x1;

	return interrupt.as32bits;
}

void fd_set_control(unsigned int ctrl)
{
	struct fd_hardware *fd_hw = get_fd_hw();
	FD_CONTROL control;

	control.as32bits = ctrl;

	writel(control.as32bits, fd_hw->iobase + REG_FD_CONTROL);
}

void fd_set_direction(unsigned int direct)
{
	struct fd_hardware *fd_hw = get_fd_hw();
	FD_CONDITION dir;

	dir.as32bits = readl(fd_hw->iobase + REG_FD_DETECT_CONDITION);

	dir.asfield.direction = direct;

	writel(dir.as32bits, fd_hw->iobase + REG_FD_DETECT_CONDITION);
}

void fd_enable(unsigned int onoff)
{
	struct fd_hardware *fd_hw = get_fd_hw();
	FD_ENABLE enable;

	enable.asfield.onoff = onoff;

	writel(enable.as32bits, fd_hw->iobase + REG_FD_ENABLE);
}

void fd_set_src_addr(unsigned int addr)
{
	struct fd_hardware *fd_hw = get_fd_hw();
	writel(addr, fd_hw->iobase + REG_FD_SRCBASE_ADDR);
}

void fd_set_condition(unsigned int min_size, unsigned int direction)
{
	struct fd_hardware *fd_hw = get_fd_hw();
	FD_CONDITION condition;

	condition.as32bits = 0;
	condition.asfield.face_size_min = min_size;
	condition.asfield.direction = direction;
	writel(condition.as32bits, fd_hw->iobase + REG_FD_DETECT_CONDITION);
}

void fd_set_resolution(unsigned int res)
{
	struct fd_hardware *fd_hw = get_fd_hw();
	FD_SRC_RESOLUTION src_res;

	src_res.as32bits = 0;
	src_res.asfield.size = res;

	writel(src_res.as32bits, fd_hw->iobase + REG_FD_SRCIMAG_RESOLUTION);
}

void fd_set_offset(unsigned int xoffset, unsigned int yoffset)
{
	struct fd_hardware *fd_hw = get_fd_hw();
	FD_START_X start_x;
	FD_START_Y start_y;

	start_x.as32bits = 0;
	start_y.as32bits = 0;
	start_x.asfield.area = xoffset;
	start_y.asfield.area = yoffset;

	writel(start_x.as32bits, fd_hw->iobase + REG_FD_DETECT_START_X);
	writel(start_y.as32bits, fd_hw->iobase + REG_FD_DETECT_START_Y);
}

void fd_set_size(unsigned int xsize, unsigned int ysize)
{
	struct fd_hardware *fd_hw = get_fd_hw();
	FD_SIZE_X size_x;
	FD_SIZE_Y size_y;

	size_x.as32bits = 0;
	size_y.as32bits = 0;
	size_x.asfield.area = xsize;
	size_y.asfield.area = ysize;

	writel(size_x.as32bits, fd_hw->iobase + REG_FD_DETECT_SIZE_X);
	writel(size_y.as32bits, fd_hw->iobase + REG_FD_DETECT_SIZE_Y);
}

void fd_set_threshold(unsigned int threshold)
{
	struct fd_hardware *fd_hw = get_fd_hw();

	FD_THRESHOLD dhit;

	dhit.as32bits = 0;
	dhit.asfield.value = threshold;

	writel(dhit.as32bits, fd_hw->iobase + REG_FD_DETECT_THRESHOLD);
}

int odin_fd_get_face_count(void)
{
	struct css_fd_device *fd_dev = get_fd_device();
#ifdef FD_PAIR_GET_DATA
	return fd_dev->result_data[0].face_num;
#else
	return fd_dev->result_data.face_num;
#endif
}

void odin_fd_set_resolution(unsigned int value)
{
	struct css_fd_device *fd_dev = get_fd_device();

	if (value == FD_QVGA) {
		fd_dev->config_data.width = 320;
		fd_dev->config_data.height = 240;
		fd_dev->config_data.res = value;
		css_fd("fd resolution : %d\n", fd_dev->config_data.res);
	} else if (value == FD_VGA) {
		fd_dev->config_data.width = 640;
		fd_dev->config_data.height = 480;
		fd_dev->config_data.res = value;
		css_fd("fd resolution : %d\n", fd_dev->config_data.res);
	} else {
		/* return error or set default size */
		css_fd("fd unsupport resolution : %d\n", fd_dev->config_data.res);
		fd_dev->config_data.width = 640;
		fd_dev->config_data.height = 480;
		fd_dev->config_data.res = FD_VGA;
	}
}

void odin_fd_set_direction(unsigned int value)
{
	struct css_fd_device *fd_dev = get_fd_device();

	fd_dev->config_data.direct = value;
	css_fd("odin_fd_set_direction : %d\n", value);
}

void odin_fd_set_threshold(unsigned int value)
{
	struct css_fd_device *fd_dev = get_fd_device();

	fd_dev->config_data.threshold = value;
	css_fd("odin_fd_set_threshold : %d\n", value);
}

void odin_fd_reset(void)
{
	fd_set_control(FACE_DETECTION_RESET);
}

void odin_fd_start(void)
{
	struct css_fd_device *fd_dev = get_fd_device();

	if (fd_dev->init)
		fd_set_control(FACE_DETECTION_START);
}

void odin_fd_stop(void)
{
	struct css_fd_device *fd_dev = get_fd_device();

	if (fd_dev->init) {
		fd_set_control(FACE_DETECTION_STOP);
		fd_dev->init = 0;
	}
}

int odin_fd_config(void)
{
	struct css_fd_device *fd_dev = get_fd_device();
	struct scaler_hardware *scl_hw = get_scl_hw();
	unsigned int fd_dram_addr;
	unsigned int fd_direct = 0;

	if (!fd_dev || !scl_hw)
		return CSS_FAILURE;

	if (!fd_dev->init) {
		fd_enable(1);

		fd_dram_addr = readl(scl_hw->iobase + FD_DRAM_ADDR);
		css_fd("fd_dram_addr = 0x%x\n", fd_dram_addr);

#ifdef FD_USE_TEST_IMAGE
		/*fd_dram_addr = &fd_image; */ /*hw can't read kernel address direct */
		fd_dram_addr = fd_image_dma_addr[fd_image_buf_index];
		css_fd"test fd_image_dma_addr = 0x%x\n", fd_image_dma_addr);
#endif
		fd_set_src_addr(fd_dram_addr);

		fd_set_resolution(fd_dev->config_data.res); /* 0->QVGA */

		fd_set_offset(0, 0);

		fd_set_size(fd_dev->config_data.width, fd_dev->config_data.height);

		fd_set_threshold(fd_dev->config_data.threshold);

		fd_direct =	fd_dev->config_data.direct;

		/* detect 360 degree */
		if (fd_direct == FD_DIRECTION_RIGHT_45)
			fd_direct = FD_DIRECTION_LEFT_135;
		else
			fd_direct = FD_DIRECTION_RIGHT_45;

		fd_dev->config_data.direct = fd_direct;
		/*min_size = 0x0 -> 20pixel*/
		fd_set_condition(0, fd_dev->config_data.direct);

		fd_dev->init = 1;
	}

	return CSS_SUCCESS;
}

#ifdef FD_USE_TEST_IMAGE
static unsigned int test =1;
#endif

int odin_face_detection_run(unsigned int buf_index)
{
	struct css_fd_device *fd_dev = get_fd_device();
	int i = 0;
	int result = CSS_SUCCESS;

	fd_dev->fd_run = 1;

	if (fd_dev->fd_info[buf_index].state == FD_READY) {
		css_fd("fd S : I = %d\n", buf_index);
		p_fd_info = &fd_dev->fd_info[buf_index];
		p_fd_info->state = FD_RUN;
		/* fd_dev->fd_info[buf_index].state = FD_RUN; */
	} else {
		css_err("FD_INVALID_INFO_BUF : buf = %d, state = %d\n", buf_index,
				fd_dev->fd_info[buf_index].state);

		for (i = 0; i < FACE_BUF_MAX; i++) {
			css_err("ERR: buf = %d, state = %d\n", i, fd_dev->fd_info[i].state);
		}
		return CSS_ERROR_FD_INVALID_INFO_BUF;
	}

#ifdef FD_USE_TEST_IMAGE
	if ((test % 4) == 1) {
		switch (buf_index)
		{
		case 0:
			/* memcpy(fd_image_cpu_addr, fd_image, sizeof(fd_image)); */
			memcpy(fd_image_cpu_addr[buf_index], fd_s_640x480,
				sizeof(fd_s_640x480));	/* 35face */
			css_fd("fd_s_640x480 35 0xE9,0xED,0xA7,0xE5\n");
			break;
		case 1:
			/* memcpy(fd_image_cpu_addr, fd_image1, sizeof(fd_image1)); */
			memcpy(fd_image_cpu_addr[buf_index], fd_s1_640x480,
				sizeof(fd_s1_640x480));	/* 32face */
			css_fd("fd_s1_640x480 32 0x9F,0x9B,0xA6,0xA3\n");
			break;
		case 2:
		default:
			/* memcpy(fd_image_cpu_addr, fd_image5, sizeof(fd_image5)); */
			memcpy(fd_image_cpu_addr[buf_index], fd_people_640x480,
				sizeof(fd_people_640x480));/* 19face */
			css_fd("fd_people_640x480 19 0x9F,0x9B,0xA0,0x82,\n");
			break;
		}
	} else if ((test % 4) == 2) {
		switch (buf_index)
		{
		case 0:
			/* memcpy(fd_image_cpu_addr, fd_image, sizeof(fd_image));//6 face */
			memcpy(fd_image_cpu_addr[buf_index], fd_image_640x480,
				sizeof(fd_image_640x480));/* 6 face */
			css_fd("fd_image_640x480 6 0x9D,0x9D,0xAA,0x68,\n");
			break;
		case 1:
			/* memcpy(fd_image_cpu_addr, fd_image1, sizeof(fd_image1));//3face */
			memcpy(fd_image_cpu_addr[buf_index], fd_image1_640x480,
				sizeof(fd_image1_640x480));/* 3 face */
			css_fd("fd_image1_640x480 3 0xA7,0xA8,0x92,0xA2,\n");
			break;
		case 2:
		default:
			/* memcpy(fd_image_cpu_addr, fd_image5, sizeof(fd_image5)); */
			memcpy(fd_image_cpu_addr[buf_index], fd_image2_640x480,
				sizeof(fd_image2_640x480));/* 17face */
			css_fd("fd_image2_640x480 17 0xFD,0xFD,0xFD,0xFD,\n");
			break;
		}
	} else if ((test % 4) == 3) {
		switch (buf_index)
		{
		case 0:
			memcpy(fd_image_cpu_addr[buf_index], fd_image_640x480,
					sizeof(fd_image_640x480));
			css_fd("fd_image_640x480 6 0x9D,0x9D,0xAA,0x68,\n");
			break;
		case 1:
			memcpy(fd_image_cpu_addr[buf_index], fd_image1_640x480,
				sizeof(fd_image1_640x480));
			css_fd("fd_image1_640x480 3 0xA7,0xA8,0x92,0xA2,\n");
			break;
		case 2:
		default:
			memcpy(fd_image_cpu_addr[buf_index], fd_image2_640x480,
				sizeof(fd_image2_640x480));
			css_fd("fd_image2_640x480 17 0xFD,0xFD,0xFD,0xFD,\n");
			break;
		}
	} else {
		switch (buf_index)
		{
		case 0:
			memcpy(fd_image_cpu_addr[buf_index], fd_s_640x480,
				sizeof(fd_s_640x480)); /* 35face */
			css_fd("fd_s_640x480 35 0xE9,0xED,0xA7,0xE5\n");
			break;
		case 1:
			memcpy(fd_image_cpu_addr[buf_index], fd_s1_640x480,
				sizeof(fd_s1_640x480)); /* 32face */
			css_fd("fd_s1_640x480 32 0x9F,0x9B,0xA6,0xA3\n");
			break;
		case 2:
		default:
			memcpy(fd_image_cpu_addr[buf_index], fd_people_640x480,
				sizeof(fd_people_640x480));/* 19face */
			css_fd("fd_people_640x480 19 0x9F,0x9B,0xA0,0x82,\n");
			break;
		}
	}

	fd_image_buf_index = buf_index;

	css_fd("buf I = %d, test = %d\n", fd_dev->buf_index,test);
	test++;

#endif

#ifdef FD_PROFILING
	if (time_count == 0) {
		CSS_TIME_START(fd_run_time);
		time_count = 1;
	}
#endif

	result = odin_fd_config();
	if (result == CSS_SUCCESS)
		odin_fd_start();

	return result;
}

#ifndef USE_FPD_HW_IP
void odin_fd_get_result(void)
{
	struct css_fd_device *fd_dev = get_fd_device();
	int i,z;
	int face_num = 0;
	int index = 0;

	for (i = 0; i < FACE_BUF_MAX; i++) {
		if (fd_dev->fd_info[i].state == FD_DONE)
			break;
	}

	if (i == FACE_BUF_MAX)
		return;

	switch (fd_dev->fd_info[i].buf_newest) {
	case 1:
		for (z = 0; z < 4; z++) {
			if (fd_dev->fd_info[z].buf_newest == 0)
				fd_dev->fd_info[z].buf_newest = 1;
		}
		break;
	case 2:
		for (z = 0; z < 4; z++) {
			if ((z != i) && (fd_dev->fd_info[z].buf_newest != 3))
				fd_dev->fd_info[z].buf_newest++;
		}
		break;
	case 3:
		for (z = 0; z < 4; z++) {
			if (z != i)
				fd_dev->fd_info[z].buf_newest++;
		}
		break;
	case 0:
		break;
	}

	fd_dev->fd_info[i].buf_newest = 0;

	face_num = fd_dev->fd_info[i].face_num;

#ifdef FD_PAIR_GET_DATA
	/* detect 360 degree */
	if (fd_dev->config_data.direct == FD_DIRECTION_RIGHT_45)
		index = 0;
	else
		index = 1;

	memcpy(fd_dev->result_data[index].fd_face_info ,
		fd_dev->fd_info[i].fd_face_info,
		sizeof(struct css_face_detect_info) * face_num);
	fd_dev->result_data[index].crop.start_x = fd_dev->fd_info[i].crop.start_x;
	fd_dev->result_data[index].crop.start_y = fd_dev->fd_info[i].crop.start_y;
	fd_dev->result_data[index].crop.width = fd_dev->fd_info[i].crop.width;
	fd_dev->result_data[index].crop.height = fd_dev->fd_info[i].crop.height;
	fd_dev->result_data[index].direct = fd_dev->config_data.direct;

	fd_dev->result_data[index].face_num = face_num;
	fd_dev->result_data[index].result_buf_index = i;
#else
	memcpy(fd_dev->result_data.fd_face_info ,fd_dev->fd_info[i].fd_face_info,
		sizeof(struct css_face_detect_info) * face_num);
	fd_dev->result_data.crop.start_x = fd_dev->fd_info[i].crop.start_x;
	fd_dev->result_data.crop.start_y = fd_dev->fd_info[i].crop.start_y;
	fd_dev->result_data.crop.width = fd_dev->fd_info[i].crop.width;
	fd_dev->result_data.crop.height = fd_dev->fd_info[i].crop.height;
	fd_dev->result_data.direct = fd_dev->config_data.direct;

	fd_dev->result_data.face_num = face_num;
	fd_dev->result_data.result_buf_index = i;
#endif

#ifdef FD_PAIR_GET_DATA
	memset(fd_dev->fd_info[i].fd_face_info, 0,
		sizeof(struct css_face_detect_info) * 35 /*face max*/);
#endif

	fd_dev->fd_info[i].crop.start_x = 0;
	fd_dev->fd_info[i].crop.start_y = 0;
	fd_dev->fd_info[i].crop.width = 0;
	fd_dev->fd_info[i].crop.height =0;

	css_fd("fd E i = %d\n", i);

	/* fd_dev->fd_info[i].state = FD_READY; */
}

#ifdef FD_PAIR_GET_DATA
void odin_fd_get_two_direction_result(void)
{
	struct css_fd_device *fd_dev = get_fd_device();
	int i;

	for (i = 0 ; i < 2 ; i++) {
		memcpy(&fd_dev->last_result_data[i] ,
			&fd_dev->result_data[i],
			sizeof(struct css_fd_result_data));
	}

	for (i = 0 ; i < 2 ; i++) {
		memset(&fd_dev->result_data[i], 0,
			sizeof(struct css_fd_result_data));
	}
	css_fd("fd 2 E \n");
}
#endif
#endif

irqreturn_t odin_fd_irq(int irq, void *dev_id)
{
	struct css_fd_device *fd_dev = get_fd_device();
	unsigned int interrupt_status;
	int count;
	int control_status;
	int i;
#ifdef USE_FPD_HW_IP
	unsigned int fd_dram_addr;
	struct fd_hardware *fd_hw = get_fd_hw();
	struct scaler_hardware *scl_hw = get_scl_hw();
#endif
#ifdef FD_PAIR_GET_DATA
	unsigned int index;
#endif

	fd_interrupt_enable(0);

	interrupt_status = fd_get_intr_status();
	if (interrupt_status) {
		 control_status = fd_get_control();
		 if (control_status == 0x04) {

			count = fd_get_face_count();
			/* css_fd("face_count = %d\n", count); */

			p_fd_info->face_num = count;

			for (i = 0; i < count; i++)
				fd_get_face_info(&p_fd_info->fd_face_info[i], i);

			/* css_fd("fd_state before = %d\n", p_fd_info->state); */

#ifdef USE_FPD_HW_IP
			if (count && (fd_dev->fpd_run != 1)) {
				p_fd_info->state = FD_DONE;
				fd_dram_addr = readl(scl_hw->iobase + FD_DRAM_ADDR);
				odin_fpd_run(fd_dram_addr,count);
				/* odin_fpd_run(fd_image_dma_addr,count); */
			} else {
				p_fd_info->state = FD_READY;
				if (fd_dev->fpd_run == 1)
					css_fpd("FPD Already RUN\n");

				/* if fpd is running, result_data isn't renewed */
				if (count == 0)
					fd_dev->result_data.face_num = 0;
			}
#else
			/*
			for (i = 0; i < FACE_BUF_MAX; i++) {
				css_fpd("FD IRQ : buf = %d, state = %d\n", i,
						fd_dev->fd_info[i].state);
			}
			*/
			if (count) {
				for (i = 0; i < FACE_BUF_MAX/*fd_info_max*/; i++) {
					if (fd_dev->fd_info[i].state == FD_DONE) {
						fd_dev->fd_info[i].state = FD_READY;
					}
				}
				p_fd_info->state = FD_DONE;
				odin_fd_get_result();
			} else {
#ifdef FD_PAIR_GET_DATA
				if (fd_dev->config_data.direct == FD_DIRECTION_RIGHT_45)
					index = 0;
				else
					index = 1;
				fd_dev->result_data[index].face_num = 0;
				fd_dev->result_data[index].direct = fd_dev->config_data.direct;
#else
				fd_dev->result_data.face_num = 0;
                fd_dev->result_data.direct = fd_dev->config_data.direct;
#endif
				p_fd_info->state = FD_READY;
			}
#ifdef FD_PAIR_GET_DATA
			/* detect 360 degree */
			if (fd_dev->config_data.direct == FD_DIRECTION_LEFT_135) {
				odin_fd_get_two_direction_result();
			}
#endif

#endif
			/* css_fd("fd_state After = %d\n", p_fd_info->state); */
			odin_fd_stop();
			odin_fd_reset();
			/* fd_interrupt_clear(); */	/* interrupt clear */

			fd_dev->fd_run = 0;
		}
	}

	fd_interrupt_clear();
	fd_interrupt_enable(1);

#ifdef FD_PROFILING
	CSS_TIME_END(fd_run_time);
	CSS_TIME_RESULT(0, "fd_run_time total(%ld) us\n", CSS_TIME(fd_run_time));
	time_count = 0;
#endif

	return IRQ_HANDLED;
}

int odin_fd_hw_init(struct css_device *cssdev)
{
	int result = CSS_SUCCESS;
	css_clock_enable(CSS_CLK_FD);
	css_block_reset(BLK_RST_FD);

#ifdef FD_USE_TEST_IMAGE
	struct css_fd_device *fd_dev = NULL;
	int image_buf_size = 0;
	int i = 0;

	fd_dev = cssdev->fd_device;

#ifdef USE_FPD_HW_IP
	image_buf_size = fd_dev->config_data.width * fd_dev->config_data.height
						+ 512	/*desc area*/
						+ 4096;	/*result area*/
#else
	image_buf_size = fd_dev->config_data.width * fd_dev->config_data.height;
#endif

	for (i = 0; i < FACE_BUF_MAX; i++) {
		fd_image_cpu_addr[i] = dma_alloc_coherent(NULL, image_buf_size,
										&fd_image_dma_addr[i], GFP_KERNEL);
		memset(fd_image_cpu_addr[i], 0, image_buf_size);
		memcpy(fd_image_cpu_addr[i], fd_image, sizeof(fd_image));
	}
#endif

	fd_interrupt_enable(1);

	enable_irq(cssdev->fd_hw.irq);

	return result;
}

int odin_fd_hw_deinit(struct css_device *cssdev)
{
	int result = CSS_SUCCESS;

#ifdef FD_USE_TEST_IMAGE
	struct css_fd_device *fd_dev = NULL;
	int image_buf_size = 0;
	int i;

	fd_dev = cssdev->fd_device;
	if (!fd_dev) {
		css_err("fd dev is null\n");
		return CSS_ERROR_FD_DEV_IS_NULL;
	}

	image_buf_size = fd_dev->config_data.width * fd_dev->config_data.height
							+ 512 + 4096;
	for (i = 0; i < FACE_BUF_MAX; i++) {
		dma_free_coherent(NULL, image_buf_size, fd_image_cpu_addr[i],
					fd_image_dma_addr[i]);
	}
#endif

	disable_irq(cssdev->fd_hw.irq);

	/* interrupt disable */
	fd_interrupt_enable(0);

	odin_fd_stop();
	odin_fd_reset();

	css_clock_disable(CSS_CLK_FD);

	return result;
}

int odin_fd_init_resource(struct css_device *cssdev)
{
	int result = CSS_SUCCESS;
	int i;

	memset(cssdev->fd_device, 0, sizeof(struct css_fd_device));
	cssdev->fd_device->buf_index = 0;

	for (i = 0; i < FACE_BUF_MAX; i++)
		cssdev->fd_device->fd_info[i].buf_newest = i;

	cssdev->fd_device->config_data.direct = FD_DIRECTION_LEFT_135;

	return result;
}

int odin_fd_deinit_resource(struct css_device *cssdev)
{
	struct css_fd_device *fd_dev = NULL;
	int result = CSS_SUCCESS;
	int i;

	fd_dev = cssdev->fd_device;

	if (!fd_dev)
		return CSS_ERROR_FD_DEV_IS_NULL;

	for (i = 0; i < FACE_BUF_MAX; i++) {
#ifdef CONFIG_ODIN_ION_SMMU
			memset(&fd_dev->bufs[i], 0, sizeof(struct face_detect_buf));
#else
		if (fd_dev->bufs[i].cpu_addr)
			dma_free_coherent(NULL, fd_dev->bufs[i].size,
					fd_dev->bufs[i].cpu_addr, fd_dev->bufs[i].dma_addr);
#endif
		fd_dev->fd_info[i].buf_newest = i;
	}

	return result;
}

int odin_fd_probe(struct platform_device *pdev)
{
	struct css_device *cssdev = get_css_plat();
	struct fd_hardware *fd_hw = (struct fd_hardware *)&cssdev->fd_hw;
	struct resource *iores = NULL;
	int result = CSS_SUCCESS;

	if (!cssdev) {
		css_err("can't get cssdev!\n");
		return CSS_FAILURE;
	}

	if (!fd_hw) {
		css_err("can't get fd_hw!\n");
		return CSS_FAILURE;
	}

	fd_hw->pdev = pdev;

	platform_set_drvdata(pdev, fd_hw);

	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!iores) {
		css_err("fd failed to get resource!\n");
		return CSS_ERROR_GET_RESOURCE;
	}

	fd_hw->iores = request_mem_region(iores->start,
									iores->end - iores->start + 1,
									"face_detect");
	if (!fd_hw->iores) {
		css_err("failed to request mem region : face_detect!\n");
		return CSS_ERROR_GET_RESOURCE;
	}

	fd_hw->iobase = ioremap(iores->start, iores->end - iores->start + 1);
	if (!fd_hw->iobase) {
		css_err("failed to ioremap!\n");
		result = CSS_ERROR_GET_RESOURCE;
		goto error_ioremap;
	}

	/* get IRQ number of ODIN-CSS */
	fd_hw->irq = platform_get_irq(pdev, 0);

	/* requset IRQ number and register IRQ service routine */
	result = request_irq(fd_hw->irq, odin_fd_irq , 0, "face_detect", cssdev);
	if (result) {
		css_err("failed to request irq for fd %d\n", result);
		result = CSS_ERROR_REQUEST_IRQ;
		goto error_irq;
	}

	disable_irq(fd_hw->irq);

	cssdev->fd_device = kzalloc(sizeof(struct css_fd_device), GFP_KERNEL);
	if (!cssdev->fd_device) {
		result = -ENOMEM;
		goto error_fd_device_alloc;
	}

	set_fd_hw(fd_hw);
	if (result == CSS_SUCCESS) {
		css_info("face-detection probe ok!!\n");
	} else {
		css_err("face-detection probe err!!\n");
	}

	return result;
error_fd_device_alloc:
	if (fd_hw->irq)
		free_irq(fd_hw->irq, cssdev);

error_irq:
	if (fd_hw->iobase) {
		iounmap(fd_hw->iobase);
		fd_hw->iobase = NULL;
	}
error_ioremap:
	if (fd_hw->iores) {
		release_mem_region(iores->start, iores->end - iores->start + 1);
		fd_hw->iores = NULL;
	}

	return result;
}

int odin_fd_remove(struct platform_device *pdev)
{
	struct css_fd_device *fd_dev = NULL;
	struct fd_hardware *fd_hw = platform_get_drvdata(pdev);
	struct css_device *cssdev = hw_to_plat(fd_hw, fd_hw);
	int result = CSS_SUCCESS;

	if (fd_hw) {
		if (cssdev) {
			fd_dev = cssdev->fd_device;
			if (fd_dev) {
				kzfree(fd_dev);
				fd_dev = NULL;
			}
			if (fd_hw->irq)
				free_irq(fd_hw->irq, cssdev);
			if (fd_hw->iobase) {
				iounmap(fd_hw->iobase);
				fd_hw->iobase = NULL;
			}
			if (fd_hw->iores) {
				release_mem_region(fd_hw->iores->start,
								fd_hw->iores->end - fd_hw->iores->start + 1);
				fd_hw->iores = NULL;
			}
			set_fd_hw(NULL);
			result = CSS_SUCCESS;
		}
	}

	if (result == CSS_SUCCESS) {
		css_info("face-detection remove ok!!\n");
	} else {
		css_err("face-detection remove err!!\n");
	}

	return result;
}

int odin_fd_suspend(struct platform_device *pdev, pm_message_t state)
{
	int result = CSS_SUCCESS;
	return result;
}

int odin_fd_resume(struct platform_device *pdev)
{
	int result = CSS_SUCCESS;
	return result;
}

#ifdef CONFIG_OF
static struct of_device_id odin_fd_match[] = {
	{ .compatible = CSS_OF_FD_NAME},
	{},
};
#endif /* CONFIG_OF */

struct platform_driver odin_fd_driver =
{
	.probe		= odin_fd_probe,
	.remove 	= odin_fd_remove,
	.suspend	= odin_fd_suspend,
	.resume 	= odin_fd_resume,
	.driver 	= {
		.name	= CSS_PLATDRV_FD,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odin_fd_match),
#endif
	},
};

static int __init odin_fd_init(void)
{
	int result = 0;

	result = platform_driver_register(&odin_fd_driver);

	return result;
}

static void __exit odin_fd_exit(void)
{
	platform_driver_unregister(&odin_fd_driver);
	return;
}

MODULE_DESCRIPTION("odin face detection driver");
MODULE_AUTHOR("MTEKVISION<http://www.mtekvision.com>");

late_initcall(odin_fd_init);
module_exit(odin_fd_exit);

MODULE_LICENSE("GPL");

