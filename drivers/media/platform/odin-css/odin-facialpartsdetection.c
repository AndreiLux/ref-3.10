/*
 * Facial Parts Detection driver
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
#include "odin-facialpartsdetection.h"
#include "odin-css-utils.h"

#ifdef USE_FPD_HW_IP

struct face_info *p_fpd_info; /* fpd ip가 현재 사용중인 fd_dev->fd_info */

#ifdef FPD_PROFILING
CSS_TIME_DEFINE(fpd_run_time);
static int fpd_time_count = 0;
#endif

static struct fpd_hardware *g_fpd_hw = NULL;

static inline void set_fpd_hw(struct fpd_hardware *hw)
{
	g_fpd_hw = hw;
}

static inline struct fpd_hardware *get_fpd_hw(void)
{
	BUG_ON(!g_fpd_hw);
	return g_fpd_hw;
}

static inline struct css_fd_device *get_fd_device(void)
{
	return hw_to_plat(get_fpd_hw(), fpd_hw)->fd_device;
}

void fpd_interrupt_enable(int enable)
{
	struct fpd_hardware *fpd_hw = get_fpd_hw();
	css_fpd("**fpd_interrupt_enable %d\n", enable);

	if (enable)
		writel(0x1, fpd_hw->iobase + REG_FPD_INTR_ENABLE);
	else
		writel(0x0, fpd_hw->iobase + REG_FPD_INTR_ENABLE);
}

void fpd_interrupt_clear(void)
{
	struct fpd_hardware *fpd_hw = get_fpd_hw();
	css_fpd("**fpd_interrupt_clear \n");

	writel(0x1, fpd_hw->iobase + REG_FPD_INTR_CLEAR);
}

unsigned int fpd_get_interrupt_status(void)
{
	struct fpd_hardware *fpd_hw = get_fpd_hw();
	FPD_INTERRUPT interrupt;

	interrupt.as32bits = readl(fpd_hw->iobase + REG_FPD_INTR_STATUS);
	interrupt.as32bits = interrupt.as32bits & 0x1;

	return interrupt.as32bits;
}

unsigned int fpd_get_desc_offset(void)
{
	struct fpd_hardware *fpd_hw = get_fpd_hw();
	unsigned int offset;

	offset = readl(fpd_hw->iobase + REG_FPD_DESC_ADDR);

	return offset;
}

unsigned int fpd_get_src_addr(void)
{
	struct fpd_hardware *fpd_hw = get_fpd_hw();
	unsigned int addr;

	addr = readl(fpd_hw->iobase + REG_FPD_SRCBASE_ADDR);

	return addr;
}

#ifdef CONFIG_ODIN_ION_SMMU
unsigned int fpd_get_src_cpu_addr(void)
{
	struct fpd_hardware *fpd_hw = get_fpd_hw();
	unsigned int addr;

	addr = (unsigned int)p_fpd_info->ion_cpu_addr;
	if (addr)
		css_fpd(" *****fpd_get_src_cpu_addr = %lx\n\n", addr);
	else
		css_err(" &&&&&&&& fpd_get_src_cpu_addr is NULL\n");

	return addr;
}
#endif

unsigned int fpd_get_result_offset(void)
{
	struct fpd_hardware *fpd_hw = get_fpd_hw();
	unsigned int offset;
	offset = readl(fpd_hw->iobase + REG_FPD_RESULT_ADDR);
	return offset;
}

int fpd_get_control(void)
{
	struct fpd_hardware *fpd_hw = get_fpd_hw();
	int control = 0;

	control = (readl(fpd_hw->iobase + REG_FPD_CONTROL)) & 0x00000007;

	return control;
}

int fpd_get_error(void)
{
	struct fpd_hardware *fpd_hw = get_fpd_hw();
	int control = 0;

	control = (readl(fpd_hw->iobase + REG_FPD_JOB_STATUS)) & 0x0000007F;

	return control;
}

/* #define FD_USE_TEST_IMAGE */
#ifdef FD_USE_TEST_IMAGE
extern void * fd_image_cpu_addr[FACE_BUF_MAX];
extern int fd_image_buf_index;
#endif

void fpd_get_info(struct css_facial_parts_info *info, unsigned int count)
{
	struct fpd_hardware *fpd_hw = get_fpd_hw();
	FPD_INFO face;
	unsigned int result_offset, result_base, result_addr;
	int i;

	result_offset = fpd_get_result_offset();

#ifdef FD_USE_TEST_IMAGE
	result_base = (result_offset << 5) + fd_image_cpu_addr[fd_image_buf_index];
#else

#ifdef CONFIG_ODIN_ION_SMMU
	result_base = (result_offset << 5) + fpd_get_src_cpu_addr();
#else
	result_base = (result_offset << 5) + fpd_get_src_addr();
#endif
#endif

#if 1
	for (i = 0; i < 16; i++) { 	/* use 16 reggister per 1 face */
		result_addr = result_base
					+ (i * 4)		/* write per 4byte reg */
					+ count * 64;	/* per 64byte */
		css_fpd("result_addr = 0x%x, i = %d , count = %d\n",
				result_addr, i, count);
		face.as32bits[i] = readl(result_addr);
		/* memcpy(&face.as32bits[i], (unsigned int *)a, sizeof(unsigned int));*/
	}
	/*
	result_addr = result_base + (count * sizeof(unsigned int) * 16);
	printk("result_addr = 0x%x, result_offset = 0x%x\n",
			result_addr, result_offset);
	memcpy(&face.as32bits[i],(unsigned int *)result_base,
			sizeof(unsigned int)*16);
	*/
#else
	for (i = 0; i < 16; i++) { /* use 16 reggister per 1 face */
		face.as32bits[i] = readl(result_addr
						+ (i * 4)		/* write per 4byte reg */
						+ count * 64	/* per 64byte */ );
	}
#endif
	info->execution_status	= face.asfield.execution_status;
	info->mouth_conf		= face.asfield.mouth_confidence;
	info->right_eye_conf	= face.asfield.right_eye_confidence;
	info->left_eye_conf		= face.asfield.left_eye_confidence;
	info->roll				= face.asfield.roll;
	info->pitch				= face.asfield.pitch;
	info->yaw				= face.asfield.yaw;
	info->left_eye.inner.y	= face.asfield.left_eye_inner_y;
	info->left_eye.inner.x	= face.asfield.left_eye_inner_x;
	info->left_eye.outer.y	= face.asfield.left_eye_outer_y;
	info->left_eye.outer.x	= face.asfield.left_eye_outer_x;
	info->left_eye.center.y = face.asfield.left_eye_center_y;
	info->left_eye.center.x = face.asfield.left_eye_center_x;
	info->right_eye.inner.y = face.asfield.right_eye_inner_y;
	info->right_eye.inner.x = face.asfield.right_eye_inner_x;
	info->right_eye.outer.y = face.asfield.right_eye_outer_y;
	info->right_eye.outer.x = face.asfield.right_eye_outer_x;
	info->right_eye.center.y = face.asfield.right_eye_center_y;
	info->right_eye.center.x = face.asfield.right_eye_center_x;
	info->nostril.left.y	= face.asfield.left_nostril_y;
	info->nostril.left.x	= face.asfield.left_nostril_x;
	info->nostril.right.y	= face.asfield.right_nostril_y;
	info->nostril.right.x	= face.asfield.right_nostril_x;
	info->mouth.left.y		= face.asfield.left_mouth_y;
	info->mouth.left.x		= face.asfield.left_mouth_x;
	info->mouth.right.y		= face.asfield.right_mouth_y;
	info->mouth.right.x		= face.asfield.right_mouth_x;
	info->mouth.upper.y		= face.asfield.upper_mouth_y;
	info->mouth.upper.x		= face.asfield.upper_mouth_x;
	info->mouth.center.y	= face.asfield.center_mouth_y;
	info->mouth.center.x	= face.asfield.center_mouth_x;
}

void fpd_set_control(unsigned int ctrl)
{
	struct fpd_hardware *fpd_hw = get_fpd_hw();
	FPD_CONTROL	control;
	css_fpd("**fpd_set_control %d\n",ctrl);

	control.as32bits = ctrl;
	writel(control.as32bits, fpd_hw->iobase + REG_FPD_CONTROL);
}

void fpd_set_face_index(int num)
{
	struct fpd_hardware *fpd_hw = get_fpd_hw();
	FPD_FACE_INDEX index;

	index.as32bits = 0;
	index.asfield.start_index = 0;
	index.asfield.end_index = num;
	writel(index.as32bits, fpd_hw->iobase + REG_FPD_FACE_INDEX);
}

void fpd_set_config(unsigned int image_res, unsigned int skip_confidence)
{
	struct fpd_hardware *fpd_hw = get_fpd_hw();
	FPD_CONFIG config;

	config.as32bits = 0;
	config.asfield.fpd_image_type = image_res;
	config.asfield.fpd_skip_confidence = skip_confidence;
	writel(config.as32bits, fpd_hw->iobase + REG_FPD_CONFIG);
}

void fpd_set_image_offset(unsigned int offset)
{
	struct fpd_hardware *fpd_hw = get_fpd_hw();

	writel(offset, fpd_hw->iobase + REG_FPD_IMAGE_ADDR);
}

void fpd_set_desc_offset(unsigned int offset)
{
	struct fpd_hardware *fpd_hw = get_fpd_hw();

	writel(offset, fpd_hw->iobase + REG_FPD_DESC_ADDR);
}

void fpd_set_work_offset(unsigned int offset)
{
	struct fpd_hardware *fpd_hw = get_fpd_hw();

	writel(offset, fpd_hw->iobase + REG_FPD_WORK_ADDR);
}

void fpd_set_result_offset(unsigned int offset)
{
	struct fpd_hardware *fpd_hw = get_fpd_hw();

	writel(offset, fpd_hw->iobase + REG_FPD_RESULT_ADDR);
}

void fpd_set_src_addr(unsigned int addr)
{
	struct fpd_hardware *fpd_hw = get_fpd_hw();

	writel(addr, fpd_hw->iobase + REG_FPD_SRCBASE_ADDR);
}

void fpd_set_descriptor(int i, unsigned int *value)
{
	struct fpd_hardware *fpd_hw = get_fpd_hw();
	unsigned int desc_addr, desc_offset;

	desc_offset = fpd_get_desc_offset();

#ifdef FD_USE_TEST_IMAGE
	desc_addr = (desc_offset << 5) + fd_image_cpu_addr[fd_image_buf_index];
#else

#ifdef CONFIG_ODIN_ION_SMMU
	css_fpd(" **************fpd_set_descriptor = %lx\n\n",
			p_fpd_info->ion_hdl.handle);

#if 0
	ion_cpu_addr
		= (unsigned long)odin_ion_map_kernel(p_fpd_info->ion_hdl.handle);
	ion_cpu_addr = p_fpd_info->ion_cpu_addr;
	if (ion_cpu_addr)
		printk(" **************odin_ion_map_kernel = %lx\n\n",ion_cpu_addr);
	else
		printk(" &&&&&&&&&& ion_cpu_addr is NULL\n");
#endif

	desc_addr = (desc_offset << 5) + fpd_get_src_cpu_addr();
#else
	desc_addr = (desc_offset << 5) + fpd_get_src_addr();
					/* (fpd_hw->iobase + REG_FPD_SRCBASE_ADDR); */
#endif
#endif
	/* css_fpd("desc_addr = 0x%x\n",desc_addr + i); */

	memcpy((void *)desc_addr + i, value, 4);
	/* writel(value, desc_addr); */
}

int odin_fpd_get_face_count(void)
{
	struct css_fd_device *fd_dev = get_fd_device();
	int i, count = 0;

#if 1
	for (i = 0; i < FACE_BUF_MAX; i++) {
		if (fd_dev->fd_info[i].state == FPD_RUN) {
			count = fd_dev->fd_info[i].face_num;
			css_fpd("get_face_count = %d\n", i);
			break;
		}
	}
#else
	p_fpd_info->fd_face_info.face_num;
#endif

	return count;
}

void odin_fpd_get_result(void)
{
	struct css_fd_device *fd_dev = get_fd_device();
	int i;

	for (i = 0; i < FACE_BUF_MAX; i++) {
		if (fd_dev->fd_info[i].state == FPD_RUN) {
			fd_dev->result_data.face_num = fd_dev->fd_info[i].face_num;
			memcpy(fd_dev->result_data.fd_face_info,
					fd_dev->fd_info[i].fd_face_info,
					sizeof(struct css_face_detect_info) * 35);
			memcpy(fd_dev->result_data.fpd_face_info ,
					fd_dev->fd_info[i].fpd_face_info,
					sizeof(struct css_facial_parts_info) * 35);
			css_fpd("fpd E i = %d\n", i);
			fd_dev->fd_info[i].state = FD_READY;
			break;
		}
	}
}

void odin_fpd_reset(void)
{
	fpd_set_control(FPD_CONTROL_RESET);
}

void odin_fpd_start(void)
{
	fpd_set_control(FPD_CONTROL_START);
}

void odin_fpd_stop(void)
{
	fpd_set_control(FPD_CONTROL_STOP);
}

void odin_fpd_config(unsigned int src_addr)
{
	struct css_fd_device *fd_dev = get_fd_device();
	int i;
	unsigned int img_res, skip_confidence;
	unsigned int offset = 0;

	offset = (fd_dev->config_data.width * fd_dev->config_data.height) >> 5;
	css_fpd("offset = %x\n", offset);

#ifdef CONFIG_ODIN_ION_SMMU
	src_addr = fd_get_src_addr();
#endif

	fpd_set_src_addr(src_addr);
	fpd_set_image_offset(0x00000000);

	fpd_set_desc_offset(offset);		/* QVGA = 0x12C0 */
	fpd_set_result_offset(offset + (0x200 >> 5)); /* 0x200 = 512 */

	fpd_set_work_offset(0xFCAC);
	/* 0xFCAC = H/W FIX - use SDRAM */

	img_res = fd_dev->config_data.res;	/* 0 = QVGA */
	skip_confidence = 0;
	fpd_set_config(img_res, skip_confidence);
}

void odin_fpd_run(unsigned int src_addr, int face_num)
{
	unsigned int desc_addr=0;
	unsigned int source_addr, desc_offset;
	int i;
	FPD_FORMAT format;
	struct css_fd_device *fd_dev = get_fd_device();

#ifdef FPD_PROFILING
	if (fpd_time_count ==0) {
		CSS_TIME_START(fpd_run_time);
		fpd_time_count =1;
	}
#endif

	fd_dev->fpd_run = 1;

	for (i = 0; i < FACE_BUF_MAX; i++) {
		if (fd_dev->fd_info[i].state == FD_DONE) {
			css_fpd("fpd I = %d\n", i);
			p_fpd_info = &fd_dev->fd_info[i];
			p_fpd_info->state = FPD_RUN;
			/* fd_dev->fd_info[i].state = FPD_RUN; */
			break;
		}
	}

	fpd_interrupt_enable(1); /* interrupt enable */

	odin_fpd_config(src_addr);

	fpd_set_face_index(face_num);

	desc_offset = fpd_get_desc_offset();
	source_addr = fpd_get_src_addr();

	/* css_fpd("desc_addr = 0x%x\n",desc_offset);	*/
	/* css_fpd("source_addr = 0x%x\n",source_addr);	*/
	/* desc_addr = (desc_addr << 5) + source_addr; */

	for (i = 0; i < face_num; i++) {
		format.as32bits[0] = 0;
		format.as32bits[1] = 0;

		format.asfield.center_x = p_fpd_info->fd_face_info[i].center_x;
		format.asfield.center_y = p_fpd_info->fd_face_info[i].center_y;
		format.asfield.angle = p_fpd_info->fd_face_info[i].angle;
		format.asfield.pose = p_fpd_info->fd_face_info[i].pose;
		format.asfield.size = p_fpd_info->fd_face_info[i].size;

		fpd_set_descriptor(i * 8, (unsigned int *)&format.as32bits[0]);
		fpd_set_descriptor((i * 8 + 4), (unsigned int *)&format.as32bits[1]);
	}

	odin_fpd_start();
}

void odin_fpd_hw_init(struct css_device *cssdev)
{
	css_clock_enable(CSS_CLK_FPD);
	css_block_reset(BLK_RST_FPD);

	fpd_interrupt_enable(1);
}

void odin_fpd_hw_deinit(struct css_device *cssdev)
{
	fpd_interrupt_enable(0);

	fpd_set_control(FPD_CONTROL_STOP);
	fpd_set_control(FPD_CONTROL_RESET);

	css_clock_disable(CSS_CLK_FPD);
}

irqreturn_t odin_fpd_irq(int irq, void *dev_id)
{
	struct css_fd_device *fd_dev = get_fd_device();
	unsigned int interrupt_status;
	int control_status;
	int error_status;
	int face_num, i;

	fpd_interrupt_enable(0);

	interrupt_status = fpd_get_interrupt_status();
	if (interrupt_status) {
		control_status = fpd_get_control();
		if (control_status == 0x04) {
			error_status = fpd_get_error();
			if (error_status)
				css_err("error_status = %d ", error_status);

			face_num = odin_fpd_get_face_count();
			for (i = 0; i < face_num; i++)
				fpd_get_info(&p_fpd_info->fpd_face_info[i], i);

			odin_fpd_get_result();

			/* p_fpd_info->state = FD_READY; */
			/* css_fpd("**\n"); */

			odin_fpd_stop();
			odin_fpd_reset();

			fd_dev->fpd_run = 0;
		}
	}

	fpd_interrupt_clear();
	fpd_interrupt_enable(1);

#ifdef FPD_PROFILING
	CSS_TIME_END(fpd_run_time);
	CSS_TIME_RESULT(fpd, "fpd_run_time total(%ld)us\n", CSS_TIME(fpd_run_time));
	fpd_time_count = 0;
#endif
	return IRQ_HANDLED;
}

static int odin_fpd_init_resource(struct platform_device *pdev)
{
	int result = CSS_FAILURE;
	struct resource *iores = NULL;
	struct css_device *cssdev = get_css_plat();
	struct fpd_hardware *fpd_hw = (struct fpd_hardware *)&cssdev->fpd_hw;

	if (!cssdev) {
		css_err("can't get cssdev!\n");
		return CSS_FAILURE;
	}

	if (!fpd_hw) {
		css_err("can't get fpd_hw!\n");
		return CSS_FAILURE;
	}

	fpd_hw->pdev = pdev;

	platform_set_drvdata(pdev, fpd_hw);

	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!iores) {
		css_err("fpd failed to get resource!\n");
		return CSS_ERROR_GET_RESOURCE;
	}

	fpd_hw->iores = request_mem_region(iores->start,
										iores->end - iores->start + 1, "fpd");

	if (!fpd_hw->iores) {
		css_err("failed to request mem region : FPD!\n");
		return CSS_ERROR_GET_RESOURCE;
	}

	fpd_hw->iobase = ioremap(iores->start, iores->end - iores->start + 1);
	if (!fpd_hw->iobase) {
		css_err("failed to fpd_ioremap!\n");
		result = CSS_ERROR_GET_RESOURCE;
		goto error_ioremap;
	}

	/* get IRQ number of ODIN-CSS */
	fpd_hw->irq = platform_get_irq(pdev, 0);

	/* request IRQ number and register IRQ service routine */
	result = request_irq(fpd_hw->irq, odin_fpd_irq, 0,
						"facial_parts_detect", cssdev);
	if (result) {
		css_err("failed to request irq for fpd %d\n", result);
		goto error_irq;
	}

	set_fpd_hw(fpd_hw);

	return CSS_SUCCESS;

error_irq:
	if (fpd_hw->iobase) {
		iounmap(fpd_hw->iobase);
		fpd_hw->iobase = NULL;
	}
error_ioremap:
	if (fpd_hw->iores) {
		release_mem_region(iores->start, iores->end - iores->start + 1);
		fpd_hw->iores = NULL;
	}

	return result;
}

int odin_fpd_deinit_resource(struct platform_device *pdev)
{
	int result = CSS_FAILURE;
	struct fpd_hardware *fpd_hw = platform_get_drvdata(pdev);

	if (fpd_hw) {
		if (fpd_hw->iobase) {
			iounmap(fpd_hw->iobase);
			fpd_hw->iobase = NULL;
		}
		if (fpd_hw->iores) {
			release_mem_region(fpd_hw->iores->start,
								fpd_hw->iores->end - fpd_hw->iores->start + 1);
			fpd_hw->iores = NULL;
		}

		set_fpd_hw(NULL);
		result = CSS_SUCCESS;
	}
	return result;
}

int odin_fpd_probe(struct platform_device *pdev)
{
	int result = CSS_SUCCESS;

	result = odin_fpd_init_resource(pdev);
	if (result == CSS_SUCCESS)
		css_info("fpd probe ok!!\n");
	else
		css_err("fpd probe err!!\n");

	return result;
}

int odin_fpd_remove(struct platform_device *pdev)
{
	int result = CSS_SUCCESS;

	result = odin_fpd_deinit_resource(pdev);

	if (result == CSS_SUCCESS)
		css_info("fpd remove ok!!\n");
	else
		css_err("fpd remove err!!\n");

	return result;
}

int odin_fpd_suspend(struct platform_device *pdev, pm_message_t state)
{
	int result = CSS_SUCCESS;
	return result;
}

int odin_fpd_resume(struct platform_device *pdev)
{
	int result = CSS_SUCCESS;
	return result;
}

#ifdef CONFIG_OF
static struct of_device_id odin_fpd_match[] = {
	{ .compatible = CSS_OF_FPD_NAME},
	{},
};
#endif /* CONFIG_OF */

struct platform_driver odin_fpd_driver =
{
	.probe		= odin_fpd_probe,
	.remove 	= odin_fpd_remove,
	.suspend	= odin_fpd_suspend,
	.resume 	= odin_fpd_resume,
	.driver 	= {
		.name	= CSS_PLATDRV_FPD,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odin_fpd_match),
#endif
	},
};

static int __init odin_fpd_init(void)
{
	int result = CSS_SUCCESS;

	result = platform_driver_register(&odin_fpd_driver);

	return result;
}

static void __exit odin_fpd_exit(void)
{
	platform_driver_unregister(&odin_fpd_driver);
	return;
}

MODULE_DESCRIPTION("odin facial parts detection driver");
MODULE_AUTHOR("MTEKVISION<http://www.mtekvision.com>");

late_initcall(odin_fpd_init);
module_exit(odin_fpd_exit);
MODULE_LICENSE("GPL");

#endif
