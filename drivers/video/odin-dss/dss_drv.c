/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Younghyun Jo <younghyun.jo@lge.com>
 * Cheolhyun park <cheolhyun.park@lge.com>
 * Youngki Lyu <youngki.lyu@lge.com>
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
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include <video/odin-dss/dss_types.h>
#include <video/odin-dss/dss_io.h>

#include "hal/du_hal.h"
#include "hal/mipi_dsi_common.h"
#include "hal/ovl_hal.h"

#include "mipi_dsi_drv.h"
#include "dip_mipi.h"
#include "dss_buf.h"
#include "dss_comp_types.h"
#include "dss_comp.h"
#include "dss_fb.h"
#include "dss_irq.h"
#include "du_ovl_drv.h"
#include "du_rsc.h"
#include "ovl_rsc.h"
#include "rot_rsc_drv.h"
#include "sync_src.h"
#include "vevent.h"

struct dss_desc
{
	int irq;

	struct {
		unsigned int du_rsc[DU_SRC_NUM];
		struct {
			unsigned int dip;
			unsigned int mipi;
		}
		lcd[DIP_NUM];
		unsigned int sync;
		unsigned int crg;
		unsigned int ovl;
		unsigned int mxd;
	}
	reg_base;

	struct {
		struct platform_device *du_rsc[DU_SRC_NUM];
		struct platform_device *ovl[OVL_CH_NUM];
		struct platform_device *lcd[DIP_NUM];
		struct platform_device *sync[SYNC_CH_NUM];
		struct platform_device *mxd;
	}
	pdev;

	struct miscdevice miscdev;
};
struct dss_desc *dss_desc;

struct dss_handle
{
	void *vevent_id;
	void *comp_id;
	struct mutex lock;
};

static void _dss_cb_buf_done(void *handle, const unsigned long  paddr)
{
	struct dss_handle *h = (struct dss_handle *)handle;
	int fd;

	fd = dss_buf_unmap(paddr);
	if (fd < 0) {
		pr_err("dss_buf_unamp failed. paddr:0x%08X\n", (int)paddr);
		return;
	}

	//pr_info("buf done. fd:%d paddr:0x%08x\n", fd, paddr);

	if (vevent_send((void*)h->vevent_id, (void*)&fd, sizeof(int)) < 0)
		pr_err("vevent_send failed\n");
}

static int _dss_io_open(struct dss_handle *h, unsigned long arg)
{
	struct dss_io_open a;
	int ret;

	ret = copy_from_user(&a, (void __user *)arg, sizeof(struct dss_io_open));
	if (ret != 0) {
		pr_err("copy_from_user failed size:%d\n", sizeof(struct dss_io_open));
		return -1;
	}

	h->vevent_id = a.vevent_id;

	return 0;
}

static int _dss_io_try_assign(struct dss_handle *h, unsigned long arg)
{
	int i;
	int ret;
	struct dss_io_mgr_try_assign a;
	struct dss_image input_image[DSS_NUM_OVERLAY_CH];
	struct dss_input_meta input_meta[DSS_NUM_OVERLAY_CH];
	enum dss_image_assign_res assign_req[DSS_NUM_OVERLAY_CH];
	unsigned int num_of_layer;
	struct dss_image output_image;
	struct dss_output_meta output_meta;
	enum sync_ch_id sync_ch;
	bool command_mode;

	mutex_lock(&h->lock);

	ret = copy_from_user(&a, (void __user *)arg, sizeof(struct dss_io_mgr_try_assign));
	if (ret != 0) {
		pr_err("copy_from_user failed size:%d\n", sizeof(struct dss_io_mgr_try_assign));
		mutex_unlock(&h->lock);
		return -1;
	}

	memset(input_image, 0, sizeof(input_image));
	memset(input_meta, 0, sizeof(input_meta));
	memset(assign_req, 0, sizeof(assign_req));
	memset(&output_image, 0, sizeof(output_image));
	memset(&output_meta, 0, sizeof(output_meta));
	sync_ch = SYNC_CH_INVALID;
	command_mode = false;

	num_of_layer = a.overlay_mixer.num_overlay_ch;

	for (i = 0; i < num_of_layer; i++) {
		input_image[i].image_size = a.overlay_ch[i].input.image_size;
		input_image[i].port = a.overlay_ch[i].input.port;
		input_image[i].pixel_format = a.overlay_ch[i].input.pixel_format;
		input_image[i].phy_addr = dss_buf_map(a.overlay_ch[i].input.ion_fd);
		input_image[i].ext_buf = true;

		input_meta[i].crop_rect = a.overlay_ch[i].crop_rect;
		input_meta[i].dst_rect = a.overlay_ch[i].dst_rect;
		input_meta[i].rotate_op = a.overlay_ch[i].rotate_op;
		input_meta[i].mxd_op = a.overlay_ch[i].mxd_enable;
		input_meta[i].blend_op = a.overlay_mixer.blend_ops[i];
		input_meta[i].flip_op = a.overlay_ch[i].flip_op;
		input_meta[i].bpp = a.overlay_ch[i].bpp;
		input_meta[i].global_alpha = a.overlay_ch[i].global_alpha;
		input_meta[i].premult_alpha = a.overlay_ch[i].premult_alpha;
		input_meta[i].zorder = a.overlay_ch[i].zorder;
		input_meta[i].chrom_enable = a.overlay_ch[i].chrom_enable;
		input_meta[i].chrom_data = a.overlay_ch[i].chrom_data;
		input_meta[i].chrom_mask = a.overlay_ch[i].chrom_mask;

		assign_req[i] = a.overlay_ch[i].assign_req;
	}

	output_image.image_size = a.output.image_size;
	output_image.port = a.output.port;
	output_image.pixel_format = a.output.pixel_format;

	if (output_image.port == DSS_DISPLAY_PORT_MEM) {
		output_image.phy_addr = dss_buf_map(a.output.ion_fd);
		output_image.ext_buf = true;
	}
	else {
		output_image.ext_buf = false;
		output_image.phy_addr = 0;
	}

	output_meta.pattern_data = a.overlay_mixer.pattern_data;

	ret = dssfb_get_ovl_sync(output_image.port, &output_meta.ovl_ch, &sync_ch, &command_mode);
	if (ret == false) {
		pr_err("dssfb_get_ovl_sync failed. port:%d\n", output_image.port);
		mutex_unlock(&h->lock);
		return -1;
	}

	dss_comp_update_layer_imgs(h->comp_id, input_image, input_meta, assign_req, num_of_layer, &output_image, &output_meta, sync_ch, command_mode);

	for (i = 0; i < num_of_layer; i++) {
		a.overlay_ch[i].assign_res = assign_req[i];
		if (assign_req[i] == DSS_IMAGE_ASSIGN_REJECTED)
			dss_buf_unmap(input_image[i].phy_addr);
	}

	ret = copy_to_user((void __user *)arg, &a, sizeof(struct dss_io_mgr_try_assign));
	if (ret != 0) {
		pr_err("copy_to_user failed size:%d\n", sizeof(struct dss_io_mgr_try_assign));
		mutex_unlock(&h->lock);
		return -1;
	}

	mutex_unlock(&h->lock);

	return 0;
}

static int (*_dss_io_fp[])(struct dss_handle *h, unsigned long arg) =
{
	_dss_io_open,
	_dss_io_try_assign,
};

static long _dss_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int errno = 0;
	unsigned long nr = _IOC_NR(cmd);

	if (nr < sizeof(_dss_io_fp)/sizeof(int *) && _dss_io_fp[nr] != NULL)
		errno = _dss_io_fp[nr]((struct dss_handle *)file->private_data, arg);
	else
		errno = -EPERM;

	if (errno < 0)
		pr_err("ioctl failed errno:%d cmd:%d\n", errno, (int)nr);

	return errno;
}

static int _dss_open(struct inode *inode, struct file *file)
{
	struct dss_handle *h;

	h = vzalloc(sizeof(struct dss_handle));
	if (h == NULL) {
		pr_err("vzalloc failed size:%d\n", sizeof(struct dss_handle));
		return -ENOMEM;
	}

	mutex_init(&h->lock);

	h->comp_id = dss_comp_open(h);
	if (h->comp_id == NULL) {
		pr_err("dss_comp_open failed\n");
		return -1;
	}

	file->private_data = (void *)h;

	return 0;
}

static int _dss_release(struct inode *inode, struct file *file)
{
	struct dss_handle *h;

	h = (struct dss_handle *)file->private_data;
	if (h == NULL) {
		pr_err("dss_handle is NULL\n");
		return -1;
	}

	mutex_lock(&h->lock);

	if (h->comp_id != NULL) {
		dss_comp_close(h->comp_id);
		h->comp_id = NULL;
	}

	mutex_unlock(&h->lock);

	vfree(h);

	file->private_data = NULL;

	dss_buf_list_print();

	return 0;
}

static struct platform_device *_dss_dt_parse(const struct device_node *parent, const unsigned int base, const char *name, unsigned int *vaddr)
{
	unsigned int size;
	unsigned int offset;
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;

	if (name == NULL) {
		pr_err("name is NULL\n");
		return NULL;
	}

	np = of_get_child_by_name(parent, name);
	if (np == NULL) {
		pr_info("Can`t find %s node\n", name);
		return NULL;
	}

	pdev = of_find_device_by_node(np);
	if (pdev == NULL) {
		pr_err("of_find_device_by_node failed(%s)\n", name);
		return NULL;
	}

	if (of_property_read_u32(np, "offset", &offset) != 0)
		pr_warn("Can`t find %s node offset\n", name);

	if (of_property_read_u32(np, "size", &size) != 0)
		pr_warn("Can`t find %s node size\n", name);

	if (vaddr) {
		*vaddr = (unsigned int)ioremap_nocache(base+offset, size);
		if (*vaddr == 0) {
			pr_err("ioremap_nocache failed(%s)\n", name);
			return NULL;
		}
	}

	return pdev;
}

static void _dss_dt_cleanup(struct dss_desc *d)
{
	if (d->reg_base.du_rsc[DU_SRC_VID0]) {
		iounmap((void __iomem *)d->reg_base.du_rsc[DU_SRC_VID0]);
		d->reg_base.du_rsc[DU_SRC_VID0] = 0;
	}

	if (d->reg_base.du_rsc[DU_SRC_VID1]) {
		iounmap((void __iomem *)d->reg_base.du_rsc[DU_SRC_VID1]);
		d->reg_base.du_rsc[DU_SRC_VID1] = 0;
	}

	if (d->reg_base.du_rsc[DU_SRC_VID2]) {
		iounmap((void __iomem *)d->reg_base.du_rsc[DU_SRC_VID2]);
		d->reg_base.du_rsc[DU_SRC_VID2] = 0;
	}

	if (d->reg_base.du_rsc[DU_SRC_GRA0]) {
		iounmap((void __iomem *)d->reg_base.du_rsc[DU_SRC_GRA0]);
		d->reg_base.du_rsc[DU_SRC_GRA0] = 0;
	}

	if (d->reg_base.du_rsc[DU_SRC_GRA1]) {
		iounmap((void __iomem *)d->reg_base.du_rsc[DU_SRC_GRA1]);
		d->reg_base.du_rsc[DU_SRC_GRA1] = 0;
	}

	if (d->reg_base.du_rsc[DU_SRC_GRA2]) {
		iounmap((void __iomem *)d->reg_base.du_rsc[DU_SRC_GRA2]);
		d->reg_base.du_rsc[DU_SRC_GRA2] = 0;
	}

	if (d->reg_base.du_rsc[DU_SRC_GSCL]) {
		iounmap((void __iomem *)d->reg_base.du_rsc[DU_SRC_GSCL]);
		d->reg_base.du_rsc[DU_SRC_GSCL] = 0;
	}

	if (d->reg_base.lcd[DIP0_LCD].dip) {
		iounmap((void __iomem *)d->reg_base.lcd[DIP0_LCD].dip);
		d->reg_base.lcd[DIP0_LCD].dip = 0;
	}

	if (d->reg_base.lcd[DIP0_LCD].mipi) {
		iounmap((void __iomem *)d->reg_base.lcd[DIP0_LCD].mipi);
		d->reg_base.lcd[DIP0_LCD].mipi = 0;
	}

	if (d->reg_base.lcd[DIP1_LCD].dip) {
		iounmap((void __iomem *)d->reg_base.lcd[DIP1_LCD].dip);
		d->reg_base.lcd[DIP1_LCD].dip = 0;
	}

	if (d->reg_base.lcd[DIP1_LCD].mipi) {
		iounmap((void __iomem *)d->reg_base.lcd[DIP1_LCD].mipi);
		d->reg_base.lcd[DIP1_LCD].mipi = 0;
	}

	if (d->reg_base.lcd[DIP_HDMI].dip) {
		iounmap((void __iomem *)d->reg_base.lcd[DIP_HDMI].dip);
		d->reg_base.lcd[DIP_HDMI].dip = 0;
	}

	if (d->reg_base.lcd[DIP_HDMI].mipi) {
		iounmap((void __iomem *)d->reg_base.lcd[DIP_HDMI].mipi);
		d->reg_base.lcd[DIP_HDMI].mipi = 0;
	}

	if (d->reg_base.crg) {
		iounmap((void __iomem *)d->reg_base.crg);
		d->reg_base.crg = 0;
	}

	if (d->reg_base.sync) {
		iounmap((void __iomem *)d->reg_base.sync);
		d->reg_base.sync = 0;
	}


	if (d->reg_base.ovl) {
		iounmap((void __iomem *)d->reg_base.ovl);
		d->reg_base.ovl = 0;
	}
}

static bool _dss_dt_init(struct dss_desc *d, struct platform_device *pdev)
{
	unsigned int base = 0;
	struct resource res;

	if (of_address_to_resource(pdev->dev.of_node, 0, &res) < 0) {
		pr_err("of_address_to_resource failed\n");
		return false;
	}

	base = (unsigned int)res.start;

	d->pdev.du_rsc[DU_SRC_VID0] = _dss_dt_parse(pdev->dev.of_node, base, "vid0", &d->reg_base.du_rsc[DU_SRC_VID0]);
	if (d->pdev.du_rsc[DU_SRC_VID0] == NULL) {
		pr_err("vid0 _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	d->pdev.du_rsc[DU_SRC_VID1] = _dss_dt_parse(pdev->dev.of_node, base, "vid1", &d->reg_base.du_rsc[DU_SRC_VID1]);
	if (d->pdev.du_rsc[DU_SRC_VID1] == NULL) {
		pr_err("vid1 _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	d->pdev.du_rsc[DU_SRC_VID2] = _dss_dt_parse(pdev->dev.of_node, base, "vid2", &d->reg_base.du_rsc[DU_SRC_VID2]);
	if (d->pdev.du_rsc[DU_SRC_VID2] == NULL) {
		pr_err("vid2 _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	d->pdev.du_rsc[DU_SRC_GRA0] = _dss_dt_parse(pdev->dev.of_node, base, "gra0", &d->reg_base.du_rsc[DU_SRC_GRA0]);
	if (d->pdev.du_rsc[DU_SRC_GRA0] == NULL) {
		pr_err("gra0 _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	d->pdev.du_rsc[DU_SRC_GRA1] = _dss_dt_parse(pdev->dev.of_node, base, "gra1", &d->reg_base.du_rsc[DU_SRC_GRA1]);
	if (d->pdev.du_rsc[DU_SRC_GRA1] == NULL) {
		pr_err("gra1 _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	d->pdev.du_rsc[DU_SRC_GRA2] = _dss_dt_parse(pdev->dev.of_node, base, "gra2", &d->reg_base.du_rsc[DU_SRC_GRA2]);
	if (d->pdev.du_rsc[DU_SRC_GRA2] == NULL) {
		pr_err("gra2 _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	d->pdev.du_rsc[DU_SRC_GSCL] = _dss_dt_parse(pdev->dev.of_node, base, "gscl", &d->reg_base.du_rsc[DU_SRC_GSCL]);
	if (d->pdev.du_rsc[DU_SRC_GSCL] == NULL) {
		pr_err("gscl _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	d->pdev.lcd[DIP0_LCD] = _dss_dt_parse(pdev->dev.of_node, base, "dip0_lcd", &d->reg_base.lcd[DIP0_LCD].dip);
	if (d->pdev.lcd[DIP0_LCD] == NULL) {
		pr_err("lcd0_dip _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	if (_dss_dt_parse(pdev->dev.of_node, base, "mipi0_lcd", &d->reg_base.lcd[DIP0_LCD].mipi) == NULL) {
		pr_err("lcd0_mipi _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	d->pdev.lcd[DIP1_LCD] = _dss_dt_parse(pdev->dev.of_node, base, "dip1_lcd", &d->reg_base.lcd[DIP1_LCD].dip);
	if (d->pdev.lcd[DIP1_LCD] == NULL) {
		pr_err("lcd1_dip _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	if (_dss_dt_parse(pdev->dev.of_node, base, "mipi1_lcd", &d->reg_base.lcd[DIP1_LCD].mipi) == NULL) {
		pr_err("lcd1_mipi _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	d->pdev.lcd[DIP_HDMI] = _dss_dt_parse(pdev->dev.of_node, base, "dip_hdmi", &d->reg_base.lcd[DIP_HDMI].dip);
	if (d->pdev.lcd[DIP_HDMI] == NULL) {
		pr_err("hdmi_dip _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	if (_dss_dt_parse(pdev->dev.of_node, base, "hdmi_tx", &d->reg_base.lcd[DIP_HDMI].mipi) == NULL) {
		pr_err("hdmi_tx _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	d->pdev.sync[SYNC_CH_ID0] = _dss_dt_parse(pdev->dev.of_node, base, "sync0", &d->reg_base.sync);
	if (d->pdev.sync == NULL) {
		pr_err("sync _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	d->pdev.sync[SYNC_CH_ID1] = _dss_dt_parse(pdev->dev.of_node, base, "sync1", NULL);
	if (d->pdev.sync == NULL) {
		pr_err("sync _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	d->pdev.sync[SYNC_CH_ID2] = _dss_dt_parse(pdev->dev.of_node, base, "sync2", NULL);
	if (d->pdev.sync == NULL) {
		pr_err("sync _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	if (_dss_dt_parse(pdev->dev.of_node, base, "crg", &d->reg_base.crg) == NULL) {
		pr_err("crg _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	d->pdev.ovl[OVL_CH_ID0] = _dss_dt_parse(pdev->dev.of_node, base, "ovl0", &d->reg_base.ovl);
	if (d->pdev.ovl[OVL_CH_ID0] == NULL) {
		pr_err("ovl0 _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	d->pdev.ovl[OVL_CH_ID1] = _dss_dt_parse(pdev->dev.of_node, base, "ovl1", NULL);
	if (d->pdev.ovl[OVL_CH_ID1] == NULL) {
		pr_err("ovl1 _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	d->pdev.ovl[OVL_CH_ID2] = _dss_dt_parse(pdev->dev.of_node, base, "ovl2", NULL);
	if (d->pdev.ovl[OVL_CH_ID2] == NULL) {
		pr_err("ovl2 _dss_dt_parse failed\n");
		_dss_dt_cleanup(d);
		return false;
	}

	d->irq = irq_of_parse_and_map(pdev->dev.of_node, 0);

	return true;
}

static const struct file_operations fops =
{
	.open = _dss_open,
	.release = _dss_release,
	.unlocked_ioctl = _dss_unlocked_ioctl,
};

static int dss_probe(struct platform_device *pdev)
{
	enum dip_data_path dip_index;
	int ret;

	dss_desc = vzalloc(sizeof(struct dss_desc));
	if (dss_desc == NULL) {
		pr_err("vzalloc failed size:%d\n", (sizeof(struct dss_desc)));
		return -ENOMEM;
	}

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, NULL);
	if (ret < 0) {
		pr_err("of_platform_populate failed(%d)\n", ret);
		return ret;
	}

	if (_dss_dt_init(dss_desc, pdev) == false) {
		pr_err("_dss_dt_init failed\n");
		return -1;
	}

	dss_buf_init();

	dss_crg_hal_init(dss_desc->reg_base.crg);
	sync_hal_init(dss_desc->reg_base.sync);
	for (dip_index = 0; dip_index < DIP_NUM; dip_index++)
		mipi_dsi_hal_init(dip_index, dss_desc->reg_base.lcd[dip_index].dip,
				dss_desc->reg_base.lcd[dip_index].mipi, false);
	ovl_hal_init(dss_desc->reg_base.ovl);
	du_hal_init(dss_desc->reg_base.du_rsc[DU_SRC_VID0],
			dss_desc->reg_base.du_rsc[DU_SRC_VID1],
			dss_desc->reg_base.du_rsc[DU_SRC_VID2],
			dss_desc->reg_base.du_rsc[DU_SRC_GRA0],
			dss_desc->reg_base.du_rsc[DU_SRC_GRA1],
			dss_desc->reg_base.du_rsc[DU_SRC_GRA2],
			dss_desc->reg_base.du_rsc[DU_SRC_GSCL]);

	dss_irq_init(dss_desc->irq);
	sync_init(dss_desc->pdev.sync);
	ovl_rsc_init(dss_desc->pdev.ovl);
	dssfb_init(dss_desc->pdev.lcd);
	du_rsc_init(dss_desc->pdev.du_rsc, dss_desc->pdev.mxd);
	dss_comp_init(dss_buf_alloc, dss_buf_free, _dss_cb_buf_done, dss_buf_rot_paddr_get);

	dss_desc->miscdev.minor = MISC_DYNAMIC_MINOR;
	dss_desc->miscdev.name = "odin-dss";
	dss_desc->miscdev.fops = &fops;
	dss_desc->miscdev.mode = 0777;

	ret = misc_register(&dss_desc->miscdev);
	if (ret < 0) {
		pr_err("misc_register failed(%d)\n", ret);
		return ret;
	}

	return 0;
}

static int dss_remove(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id dss_match[] =
{
	{
		.name = "dss",
		.compatible = "odin,dss",
	},
	{
	},
};

static struct platform_driver dss_pdrv =
{
	.driver = {
		.name = "dss",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(dss_match),
	},
	.probe = dss_probe,
	.remove = dss_remove,
};

static int __init dss_init(void)
{
	int ret;
	ret = platform_driver_register(&dss_pdrv);
	if (ret < 0) {
		pr_err("dss platform_driver_register failed\n");
		return ret;
	}

	return 0;
}

static void __exit dss_cleanup(void)
{
	platform_driver_unregister(&dss_pdrv);
}

device_initcall_sync(dss_init);
module_exit(dss_cleanup);
