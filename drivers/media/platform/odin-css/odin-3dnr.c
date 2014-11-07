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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/ion.h>
#include <linux/fcntl.h>

#include <linux/time.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/wait.h>

#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#include "odin-css.h"
#include "odin-css-system.h"
#include "odin-css-clk.h"

#include "odin-3dnr.h"
#include "odin-css-utils.h"

#define OF_VNR_NAME				"mtekvision,3dnr"
#define ODIN_VNR_DEV_NAME		"odin-3dnr"
#define ODIN_VNR_DEV_MAJOR		257

#define FRAME_WIDTH_MAX			1920
#define FRAME_HEIGHT_MAX		1088
#define FRAME_HEIGHT_LIMIT		1080

#define ODIN_3DNR_TIMEOUT (msecs_to_jiffies(1800))

static DEFINE_MUTEX(vnr_mutex);
DECLARE_WAIT_QUEUE_HEAD(vnr_wait_queue);

/* spinlock_t  lock; */
unsigned long	g_vnr_status;
int 			g_error_num;
static vnr_ctx	*g_vnr_conf=NULL;


/* lut setting function */
static void odin_vnr_lut_set(vnr_ctx *vnr_conf, u32 offset, u32 value)
{
	BUG_ON(!vnr_conf);

	writel(value, vnr_conf->regs + REG_LUT_DATA_WPORT_ENTRY1 + (offset << 2));
}

static void odin_vnr_ycc(vnr_ctx *vnr_conf, u32 y, u32 cb, u32 cr)
{
	u32 reg = 0;
	BUG_ON(!vnr_conf);

	reg = (y | (cb << 1) | (cr << 2));
	writel(reg, vnr_conf->regs + REG_VNR_YCBCR_SEL);
	return;
}

static
void odin_vnr_lut_sel(vnr_ctx *vnr_conf, u32 temp, u32 spat_d2, u32 spat_d1)
{
	u32 reg = 0;
	BUG_ON(!vnr_conf);

	reg = ((spat_d1) | (spat_d2 << 1) | (temp << 2));
	writel(reg,  vnr_conf->regs + REG_VNR_LUT_SEL);
	return;
}

static void odin_vnr_lut_init(vnr_ctx *vnr_conf)
{
	int i;
	BUG_ON(!vnr_conf);

	odin_vnr_lut_sel(vnr_conf, 1, 1, 1);
	odin_vnr_ycc(vnr_conf, 1, 1, 1);

	for (i = 0; i < 256; i++)
		odin_vnr_lut_set(vnr_conf, i, 0);

	odin_vnr_lut_sel(vnr_conf, 0, 0, 0);
	odin_vnr_ycc(vnr_conf, 0, 0, 0);

	return;
}

static void odin_vnr_set(vnr_ctx *vnr_conf)
{

	BUG_ON(!vnr_conf);

	odin_vnr_lut_init(vnr_conf);
#if 0	/* tunning value */
	/* Spatial */
	odin_vnr_ycc(vnr_conf, 0, 0, 1); /* Y LUT */
	odin_vnr_lut_sel(vnr_conf, 0, 0, 1); /* Spatial dist 1 */

	odin_vnr_lut_set(vnr_conf, 0, 65536);
	odin_vnr_lut_set(vnr_conf, 1, 64632);
	odin_vnr_lut_set(vnr_conf, 2, 61994);
	odin_vnr_lut_set(vnr_conf, 3, 57835);
	odin_vnr_lut_set(vnr_conf, 4, 52477);
	odin_vnr_lut_set(vnr_conf, 5, 46311);

	odin_vnr_lut_sel(vnr_conf, 0, 1, 0); /* Spatial dist 2 */
	odin_vnr_lut_set(vnr_conf, 0, 65536);
	odin_vnr_lut_set(vnr_conf, 1, 64632);
	odin_vnr_lut_set(vnr_conf, 2, 61994);
	odin_vnr_lut_set(vnr_conf, 3, 57835);
	odin_vnr_lut_set(vnr_conf, 4, 52477);
	odin_vnr_lut_set(vnr_conf, 5, 46311);
	odin_vnr_ycc(vnr_conf, 0, 1, 0); /* Cb LUT */
	odin_vnr_lut_sel(vnr_conf, 0, 0, 1); /* Spatial dist 1 */
	odin_vnr_lut_set(vnr_conf, 0, 65536);
	odin_vnr_lut_set(vnr_conf, 1, 61994);
	odin_vnr_lut_set(vnr_conf, 2, 52477);
	odin_vnr_lut_sel(vnr_conf, 0, 1, 0); /* Spatial dist 2 */
	odin_vnr_lut_set(vnr_conf, 0, 65536);
	odin_vnr_lut_set(vnr_conf, 1, 61994);
	odin_vnr_lut_set(vnr_conf, 2, 52477);
	odin_vnr_ycc(vnr_conf, 1, 0, 0); /* Cr LUT */
	odin_vnr_lut_sel(vnr_conf, 0, 0, 1); /* Spatial dist 1 */
	odin_vnr_lut_set(vnr_conf, 0, 65536);
	odin_vnr_lut_set(vnr_conf, 1, 61994);
	odin_vnr_lut_set(vnr_conf, 2, 52477);
	odin_vnr_lut_sel(vnr_conf, 0, 1, 0); /* Spatial dist 2 */
	odin_vnr_lut_set(vnr_conf, 0, 65536);
	odin_vnr_lut_set(vnr_conf, 1, 61994);
	odin_vnr_lut_set(vnr_conf, 2, 52477);

	/* Temporal */
	odin_vnr_lut_sel(vnr_conf,  1, 0, 0); /* Spatial dist 1 */
	odin_vnr_ycc(vnr_conf,  0, 0, 1); /* Y LUT */
	odin_vnr_lut_set(vnr_conf,  0, 65536);
	odin_vnr_lut_set(vnr_conf,  1, 65454);
	odin_vnr_lut_set(vnr_conf,  2, 65209);
	odin_vnr_lut_set(vnr_conf,  3, 64803);
	odin_vnr_lut_set(vnr_conf,  4, 64238);
	odin_vnr_lut_set(vnr_conf,  5, 63520);
	odin_vnr_lut_set(vnr_conf,  6, 62652);
	odin_vnr_lut_set(vnr_conf,  7, 61642);
	odin_vnr_lut_set(vnr_conf,  8, 60497);
	odin_vnr_lut_set(vnr_conf,  9, 59225);
	odin_vnr_lut_set(vnr_conf, 10, 57835);
	odin_vnr_lut_set(vnr_conf, 11, 56337);
	odin_vnr_lut_set(vnr_conf, 12, 54740);
	odin_vnr_lut_set(vnr_conf, 13, 53056);
	odin_vnr_lut_set(vnr_conf, 14, 51295);
	odin_vnr_lut_set(vnr_conf, 15, 49469);
	odin_vnr_lut_set(vnr_conf, 16, 47589);
	odin_vnr_lut_set(vnr_conf, 17, 45666);
	odin_vnr_lut_set(vnr_conf, 18, 43711);
	odin_vnr_lut_set(vnr_conf, 19, 41735);

	odin_vnr_ycc(vnr_conf,  0, 1, 0); /* Cb LUT */
	odin_vnr_lut_set(vnr_conf,  0, 65536);
	odin_vnr_lut_set(vnr_conf,  1, 65209);
	odin_vnr_lut_set(vnr_conf,  2, 64238);
	odin_vnr_lut_set(vnr_conf,  3, 62652);
	odin_vnr_lut_set(vnr_conf,  4, 60497);
	odin_vnr_lut_set(vnr_conf,  5, 57835);
	odin_vnr_lut_set(vnr_conf,  6, 54740);
	odin_vnr_lut_set(vnr_conf,  7, 51295);
	odin_vnr_lut_set(vnr_conf,  8, 47589);
	odin_vnr_lut_set(vnr_conf,  9, 43711);
	odin_vnr_lut_set(vnr_conf, 10, 39750);
	odin_vnr_lut_set(vnr_conf, 11, 35788);
	odin_vnr_lut_set(vnr_conf, 12, 31900);
	odin_vnr_lut_set(vnr_conf, 13, 28151);
	odin_vnr_lut_set(vnr_conf, 14, 24596);

	odin_vnr_ycc(vnr_conf,  1, 0, 0); /* Cr LUT */
	odin_vnr_lut_set(vnr_conf,  0, 65536);
	odin_vnr_lut_set(vnr_conf,  1, 65209);
	odin_vnr_lut_set(vnr_conf,  2, 64238);
	odin_vnr_lut_set(vnr_conf,  3, 62652);
	odin_vnr_lut_set(vnr_conf,  4, 60497);
	odin_vnr_lut_set(vnr_conf,  5, 57835);
	odin_vnr_lut_set(vnr_conf,  6, 54740);
	odin_vnr_lut_set(vnr_conf,  7, 51295);
	odin_vnr_lut_set(vnr_conf,  8, 47589);
	odin_vnr_lut_set(vnr_conf,  9, 43711);
	odin_vnr_lut_set(vnr_conf, 10, 39750);
	odin_vnr_lut_set(vnr_conf, 11, 35788);
	odin_vnr_lut_set(vnr_conf, 12, 31900);
	odin_vnr_lut_set(vnr_conf, 13, 28151);
	odin_vnr_lut_set(vnr_conf, 14, 24596);
#else
	/* Spatial */
	odin_vnr_ycc(vnr_conf, 1, 1, 1); /* Y LUT */
	odin_vnr_lut_sel(vnr_conf, 0, 0, 1); /* Spatial dist 1 */
	odin_vnr_lut_set(vnr_conf, 0, 32768);
	odin_vnr_lut_set(vnr_conf, 1, 32768);
	odin_vnr_lut_set(vnr_conf, 2, 32768);
	odin_vnr_lut_set(vnr_conf, 3, 32768);
	odin_vnr_lut_set(vnr_conf, 4, 32768);
	odin_vnr_lut_set(vnr_conf, 5, 32768);
	odin_vnr_lut_sel(vnr_conf, 0, 1, 0); /* Spatial dist 2 */
	odin_vnr_lut_set(vnr_conf, 0, 32768);
	odin_vnr_lut_set(vnr_conf, 1, 32768);
	odin_vnr_lut_set(vnr_conf, 2, 32768);
	odin_vnr_lut_set(vnr_conf, 3, 32768);
	odin_vnr_lut_set(vnr_conf, 4, 32768);
	odin_vnr_lut_set(vnr_conf, 5, 32768);

	/* Temporal */
	odin_vnr_lut_sel(vnr_conf, 1, 0, 0);
	odin_vnr_ycc(vnr_conf, 1, 1, 1);	/* Y LUT */
	odin_vnr_lut_set(vnr_conf, 0, 65536);
	odin_vnr_lut_set(vnr_conf, 1, 65454);
	odin_vnr_lut_set(vnr_conf, 2, 65209);
	odin_vnr_lut_set(vnr_conf, 3, 64803);
	odin_vnr_lut_set(vnr_conf, 4, 64238);
	odin_vnr_lut_set(vnr_conf, 5, 63520);
	odin_vnr_lut_set(vnr_conf, 6, 62652);
	odin_vnr_lut_set(vnr_conf, 7, 61642);
	odin_vnr_lut_set(vnr_conf, 8, 60497);
	odin_vnr_lut_set(vnr_conf, 9, 59225);

#endif

	return;
}

void odin_vnr_start(vnr_ctx *vnr_conf)
{
	BUG_ON(!vnr_conf);

	writel(1, vnr_conf->regs + REG_VNR_CONTROL);

	return;
}

void odin_vnr_stop(vnr_ctx *vnr_conf)
{
	BUG_ON(!vnr_conf);

	writel(0, vnr_conf->regs + REG_VNR_CONTROL);

	set_bit(VNR_END, &g_vnr_status);

	return;
}

static void odin_vnr_cache_type(vnr_ctx *vnr_conf)
{
	u32 reg = 0;

	BUG_ON(!vnr_conf);

	reg = CACHE_ARCHCHE_TYPE | CACHE_AWCHCHE_TYPE;

	writel(reg, vnr_conf->regs + REG_VNR_CACHE_TYPE);

	return;
}

static void odin_vnr_ch_control(vnr_ctx *vnr_conf, int enable)
{
	u32 reg = 0;

	BUG_ON(!vnr_conf);

	if (enable == 1) {
		reg = CH_CTRL_R2EN | CH_CTRL_R1EN | CH_CTRL_R0EN | CH_CTRL_WEN;
		writel(reg, vnr_conf->regs + REG_VNR_CH_CTRL);
	} else {
		writel(0, vnr_conf->regs + REG_VNR_CH_CTRL);
	}

	return;
}

int odin_vnr_wait_done(vnr_ctx * vnr_conf)
{
	unsigned int ret = 0;
	unsigned long timeout;
	timeout = wait_for_completion_timeout(&vnr_conf->completion,
			ODIN_3DNR_TIMEOUT);
	if (timeout == 0) {
		pr_err("controller timed out\n");
		vnr_conf->status = VNR_ERROR;
		ret =  -ETIMEDOUT;
	}

	return ret;
}

int odin_vnr_set_config(vnr_ctx *vnr_conf)
{
	unsigned int hmask_value;
	unsigned int vmask_value;
	u32 reg = 0;
	unsigned int ret;
	unsigned long flags;
	unsigned long iova_ref2, iova_ref1, iova_cur, iova_result;
	vnr_param *conf_param;

	BUG_ON(!vnr_conf);

	vnr_conf->ion_client = odin_ion_client_create("css_3dnr_ion");
	if (vnr_conf->ion_client == NULL) {
		pr_err("3dnr ion client handle error\n");
		return -1;
	}
	conf_param = &vnr_conf->param;
	/* set des address */
	vnr_conf->ion_hld_ref2.handle = ion_import_dma_buf(
				vnr_conf->ion_client, conf_param->fd_ref2);
	vnr_conf->ion_hld_ref1.handle = ion_import_dma_buf(
				vnr_conf->ion_client, conf_param->fd_ref1);
	vnr_conf->ion_hld_cur.handle = ion_import_dma_buf(
				vnr_conf->ion_client, conf_param->fd_cur);
	vnr_conf->ion_hld_result.handle = ion_import_dma_buf(
				vnr_conf->ion_client, conf_param->fd_result);

	iova_ref2 = (unsigned long)odin_ion_get_iova_of_buffer(
				vnr_conf->ion_hld_ref2.handle, ODIN_SUBSYS_CSS);
	iova_ref1 = (unsigned long)odin_ion_get_iova_of_buffer(
				vnr_conf->ion_hld_ref1.handle, ODIN_SUBSYS_CSS);
	iova_cur = (unsigned long)odin_ion_get_iova_of_buffer(
				vnr_conf->ion_hld_cur.handle, ODIN_SUBSYS_CSS);
	iova_result = (unsigned long)odin_ion_get_iova_of_buffer(
				vnr_conf->ion_hld_result.handle, ODIN_SUBSYS_CSS);


	/* resolution check */
	spin_lock_irqsave(&vnr_conf->slock, flags);

	vnr_conf->status = VNR_READY;
	INIT_COMPLETION(vnr_conf->completion);

	hmask_value = conf_param->input_hmask;
	/* convert 420 format to 422 format  */
	vmask_value = conf_param->input_vmask * 3/4;

	if (hmask_value <= 0 || hmask_value > FRAME_WIDTH_MAX) {
		pr_err("hmask value error : %d\n", hmask_value);
		ret = -CSS_ERROR_VNR_MASK_SIZE;
		goto error;
	}

	if (vmask_value <= 0 || vmask_value > FRAME_HEIGHT_MAX) {
		pr_err("vmask value error : %d\n", hmask_value);
		ret = -CSS_ERROR_VNR_MASK_SIZE;
		goto error;
	}

	if (vmask_value > FRAME_HEIGHT_LIMIT) {
		vmask_value = 1080;
	}

	/* set transfer size */
	/* reg = (hmask_value * vmask_value * 3/4 *2) ==> *3/2) */
	reg = (hmask_value * vmask_value * 2);
	writel(reg, vnr_conf->regs + REG_VNR_DMA_TRANS_SIZE);

	/* set resolution size */
	reg = ((hmask_value - 1) << 12) | (vmask_value - 1);
	writel(reg, vnr_conf->regs + REG_VNR_RESOL_SIZE);

	odin_vnr_cache_type(vnr_conf);
	odin_vnr_ch_control(vnr_conf, 1); /* enable */

	writel(iova_result, vnr_conf->regs + REG_VNR_RESULT_BASE);
	writel(iova_ref2, vnr_conf->regs + REG_VNR_REF_BASE1);
	writel(iova_ref1, vnr_conf->regs + REG_VNR_REF_BASE0);
	writel(iova_cur, vnr_conf->regs + REG_VNR_CURR_BASE);

	odin_vnr_start(vnr_conf);
	spin_unlock_irqrestore(&vnr_conf->slock, flags);

	ret = odin_vnr_wait_done(vnr_conf);
	if (ret != 0) {
		vnr_conf->status = VNR_ERROR;
		g_error_num = -CSS_ERROR_VNR_TIMEOUT;
		wake_up_interruptible(&vnr_wait_queue);
	}

	odin_ion_client_destroy(vnr_conf->ion_client);
	return 0;

error:
	spin_unlock_irqrestore(&vnr_conf->slock, flags);
	vnr_conf->status = VNR_ERROR;
	odin_ion_client_destroy(vnr_conf->ion_client);
	return ret;
}

static irqreturn_t odin_vnr_irq_handler(int irq, void *dev_id)
{
	u32 reg = 0;
	vnr_ctx *vnr_conf = dev_id;

	BUG_ON(!vnr_conf);

	reg = readl(vnr_conf->regs + REG_VNR_STATUS);
	writel(0, vnr_conf->regs + REG_VNR_CONTROL);

	if ((reg & STAT_DONE) != 0x0) {
		vnr_conf->status = VNR_DONE;
		complete(&vnr_conf->completion);
		wake_up_interruptible(&vnr_wait_queue);

	} else if ((reg & STAT_BUSY) != 0) {
		set_bit(VNR_ERROR, &g_vnr_status);
		vnr_conf->status = VNR_BUSY;
		complete(&vnr_conf->completion);
		wake_up_interruptible(&vnr_wait_queue);
	} else {
		vnr_conf->status = VNR_ERROR;
		g_error_num = -(CSS_ERROR_VNR_IRQ);
		complete(&vnr_conf->completion);
		wake_up_interruptible(&vnr_wait_queue);
	}

	return IRQ_HANDLED;
}

unsigned int odin_vnr_poll(struct file *filp, poll_table * wait)
{
	int mask;
	vnr_ctx *ctrl;

	poll_wait(filp, &vnr_wait_queue, wait);

	ctrl  = (vnr_ctx *)filp->private_data;
	if (ctrl->status == VNR_DONE) {
		mask = POLLIN;
		ctrl->status = VNR_IDLE;
	} else if (ctrl->status == VNR_ERROR) {
		mask = POLLERR;
		ctrl->status = VNR_IDLE;
	} else {
		mask = 0;
	}

	return mask;
}

long odin_vnr_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	unsigned long size;
	int ret = 0;
	vnr_ctx *ctrl;

	ctrl = (vnr_ctx *)filp->private_data;

	if (_IOC_TYPE(cmd) != IOCTL_VNR_MAGIC)
		return -EINVAL;

	if (_IOC_NR(cmd) >= IOCTL_VNR_MAXNR)
		return -EINVAL;

	size = _IOC_SIZE(cmd);
	if (size) {
		if (_IOC_DIR(cmd) & _IOC_READ) {
			if (unlikely(!access_ok(VERIFY_WRITE, (void *)arg, size)))
				return -EFAULT;
		}
		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			if (unlikely(!access_ok(VERIFY_READ, (void *)arg, size)))
				return -EFAULT;
		}
	}

	mutex_lock(&vnr_mutex);

	switch (cmd)
	{
	case  IOCTL_VNR_PARAM:
		if (copy_from_user((vnr_param *)&ctrl->param, (void *)arg, size)) {
			ret = -EFAULT;
			break;
		}

		ret = odin_vnr_set_config(ctrl);
		if (ret != 0) {
			ctrl->status = VNR_ERROR;
			g_error_num = ret;
			wake_up_interruptible(&vnr_wait_queue);
			break;
		}
		break;

	case IOCTL_VNR_ERROR_GET:
		if (copy_to_user((void *)arg, (const void __user *)&g_error_num, size))
			ret = -EFAULT;
		break;

	default:
		pr_err("opps : ioctl wrong cmd = %x\n", cmd);
		break;
	}
	mutex_unlock(&vnr_mutex);

	return ret;
}

int odin_vnr_release(struct inode *inode, struct file *filp)
{
	printk("calling 3dnr driver release \n");

	mutex_lock(&g_vnr_conf->mutex);

	if (g_vnr_conf->open) {
		filp->private_data = NULL;

		disable_irq(g_vnr_conf->irq);

		css_clock_disable(CSS_CLK_VNR);
		css_power_domain_off(POWER_VNR);
		g_vnr_conf->open = 0;
	}

	mutex_unlock(&g_vnr_conf->mutex);

	return 0;
}

int odin_vnr_open(struct inode *inode, struct file *filp)
{
	printk("odin 3dnr open call\n");

	mutex_lock(&g_vnr_conf->mutex);

	if (g_vnr_conf->open) {
		css_warn("already opened 3dnr file\n");
		mutex_unlock(&g_vnr_conf->mutex);
		return -EBUSY;
	}

	filp->private_data = (vnr_ctx *)g_vnr_conf;

	css_power_domain_on(POWER_VNR);
	css_clock_enable(CSS_CLK_VNR);
	css_block_reset(BLK_RST_VNR);

	enable_irq(g_vnr_conf->irq);

	odin_vnr_set(g_vnr_conf);

	g_vnr_conf->open = 1;

	mutex_unlock(&g_vnr_conf->mutex);

	return 0;
}

struct file_operations odin_vnr_fops =
{
	.owner				= THIS_MODULE,
	.open				= odin_vnr_open,
	.poll				= odin_vnr_poll,
	.unlocked_ioctl		= odin_vnr_ioctl,
	.release			= odin_vnr_release,
};

static int odin_vnr_remove(struct platform_device *pdev)
{
	return 0;
}

static int odin_vnr_probe(struct platform_device *pdev)
{
	int ret = CSS_SUCCESS;
	struct device_node *dn = pdev->dev.of_node;
	struct resource res;
	ulong  regs_start, regs_size;

	printk("odin 3dnr probe called\n");
	g_vnr_conf = devm_kzalloc(&pdev->dev, sizeof(vnr_ctx), GFP_KERNEL);
	if (!g_vnr_conf)
		return -ENOMEM;

	spin_lock_init(&g_vnr_conf->slock);
	init_completion(&g_vnr_conf->completion);
	mutex_init(&g_vnr_conf->mutex);
	g_vnr_conf->dev = &pdev->dev;

#ifdef CONFIG_OF

	g_vnr_conf->irq  = irq_of_parse_and_map(dn, 0);
	if (g_vnr_conf->irq == NO_IRQ) {
		pr_err("Error mapping IRQ!\n");
		return -EINVAL;
	}

	ret = of_address_to_resource(dn, 0, &res);
	if (ret) {
		pr_err("Error parsing memory region! \n");
		return -EINVAL;
	}

	regs_start = res.start;
	regs_size = resource_size(&res);
	if (!devm_request_mem_region(g_vnr_conf->dev, regs_start, regs_size,
		OF_VNR_NAME)) {
		pr_err("Error requesting memory region!\n");
		return -ENOENT;
	}

	g_vnr_conf->regs = devm_ioremap(g_vnr_conf->dev, regs_start, regs_size);
	if (!g_vnr_conf->regs) {
		pr_err("Error mapping memory region!\n");
		return -ENOMEM;
	}

#endif

	ret = devm_request_irq(&pdev->dev, g_vnr_conf->irq,
			odin_vnr_irq_handler, 0, dev_name(&pdev->dev), g_vnr_conf);
	if (ret) {
		dev_err(&pdev->dev, "cannot claim IRQ %d\n", g_vnr_conf->irq);
		goto p_err;
	}

	disable_irq(g_vnr_conf->irq);

	platform_set_drvdata(pdev, g_vnr_conf);
	return 0;

p_err:
	iounmap(g_vnr_conf->regs);
	return ret;
}

#ifdef CONFIG_OF
static struct of_device_id odin_vnr_match[] = {
	{ .compatible = OF_VNR_NAME},
	{},
};
#endif /* CONFIG_OF */

static struct platform_driver odin_vnr_driver = {
	.probe		= odin_vnr_probe,
	.remove 	= odin_vnr_remove,
	.driver = {
		.name	= ODIN_VNR_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odin_vnr_match),
#endif
	}
};

static int __init odin_vnr_init(void)
{
	int ret;

	printk("calling odin 3dnr register driver\n");
	ret = register_chrdev(ODIN_VNR_DEV_MAJOR, ODIN_VNR_DEV_NAME,
							&odin_vnr_fops);
	if (ret < 0)
		goto err_3dnr;

	ret = platform_driver_register(&odin_vnr_driver);
	if (ret < 0)
		goto err_plat;

	return 0;

err_plat:
	unregister_chrdev(ODIN_VNR_DEV_MAJOR, ODIN_VNR_DEV_NAME);

err_3dnr:
	pr_err("registering 3dnr driver failed : err = %i", ret);
	return ret;
}

static void __exit odin_vnr_exit(void)
{
	pr_debug("calling odin 3dnr exit\n");
	platform_driver_unregister(&odin_vnr_driver);
	unregister_chrdev(ODIN_VNR_DEV_MAJOR, ODIN_VNR_DEV_NAME);
}

late_initcall(odin_vnr_init);
module_exit(odin_vnr_exit);

MODULE_LICENSE("Dual BSD/GPL");
