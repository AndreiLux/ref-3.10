/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Youngki Lyu <youngki.lyu@lge.com>
 * Jungmin Park <jungmin016.park@lge.com>
 * Younghyun Jo <younghyun.jo@lge.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/major.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/odin_iommu.h>
#include <linux/odin_mailbox.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>

#include <media/odin/vcodec/vcore/device.h>
#include <media/odin/vcodec/vdec_io.h>
#include <media/odin/vcodec/venc_io.h>
#include <media/odin/vcodec/vppu_io.h>
#include <media/odin/vcodec/vlog.h>
#include <media/odin/vcodec/vppu_drv.h>

#include "vcodec_io.h"
#include "vcodec_vcore.h"

#include "./common/vcodec_rate.h"
#include "./common/vcodec_timer.h"
#include "./common/vcodec_dpb.h"

#include "misc/vbuf/vbuf.h"

#include "vdec/vdec_drv.h"
#include "venc/venc_drv.h"

struct vcodec_desc
{
	struct miscdevice miscdev;
};
static struct vcodec_desc *vcodec_desc;

static struct file_operations fops =
{
	.open = vcodec_io_open,
	.release = vcodec_io_close,
	.unlocked_ioctl = vcodec_io_unlocked_ioctl,
};

static int _vcodec_probe(void)
{
	int errno = 0;

	vlog_info("vcodec_probe start\n");

	if (vbuf_init() < 0) {
		vlog_error("vbuf_init failed\n");
		return -ENOBUFS;
	}

	vcodec_desc = vzalloc(sizeof(struct vcodec_desc));
	if (vcodec_desc == NULL) {
		vlog_error("vzalloc failed\n");
		errno = -ENOMEM;
		goto err_vzalloc;
	}

	vcodec_rate_init();
	vcodec_timer_init();
	vcodec_dpb_init();

	vdec_init(vcodec_vcore_get_dec, vcodec_vcore_put_dec);
	venc_init(vcodec_vcore_get_enc, vcodec_vcore_put_enc);
	vppu_init(vcodec_vcore_get_ppu, vcodec_vcore_put_ppu);

	vcodec_desc->miscdev.minor = MISC_DYNAMIC_MINOR;
	vcodec_desc->miscdev.name = "vcodec";
	vcodec_desc->miscdev.mode = 0777;
	vcodec_desc->miscdev.fops = &fops;
	errno = misc_register(&vcodec_desc->miscdev);
	if (errno < 0) {
		vlog_error("misc_register failed\n");
		goto err_misc_register;
	}

	vlog_init(vcodec_desc->miscdev.this_device);

	vlog_info("vcodec_probe success\n");

	return 0;

err_misc_register:
	if (vcodec_desc) {
		vfree(vcodec_desc);
		vcodec_desc = NULL;
	}

err_vzalloc:
	vbuf_cleanup();

	return errno;
}

static int _vcodec_remove(void)
{
	misc_deregister(&vcodec_desc->miscdev);

	vdec_cleanup();
	venc_cleanup();
	vppu_cleanup();

	vcodec_timer_cleanup();
	vcodec_rate_cleanup();
	vcodec_dpb_cleanup();

	vfree(vcodec_desc);

	vbuf_cleanup();

	return 0;
}

static int __init vcodec_init(void)
{
	_vcodec_probe();
	return 0;
}

static void __exit vcodec_cleanup(void)
{
	_vcodec_remove();
}

late_initcall(vcodec_init);
module_exit(vcodec_cleanup);

MODULE_AUTHOR("LGE");
MODULE_DESCRIPTION("VIDEO CODEC DRIVER");
MODULE_LICENSE("GPL");

