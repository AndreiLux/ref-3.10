/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Jungmin Park <jungmin016.park@lge.com>
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
 #include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/odin_iommu.h> 
#include <linux/odin_mailbox.h> 
#include <linux/odin_pm_domain.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/clk-private.h>

#include "hal/du_hal.h"
#include "du_rsc.h"
#include "linux/clk.h"
#include "sync_src.h"
#include "hal/ovl_hal.h"
#include "hal/crg_hal.h"

struct du_rsc_ch_db
{
	struct
	{
		bool rotate;
		bool scale;
		bool mxd;
		bool wb;
		bool mixed_wb;
		struct
		{
			unsigned int rgb;
			unsigned int yuv;
		}input_format, output_format;
	}cap;

	bool use;
	struct odin_pm_domain *pm_domain;
	struct device *dev;
	struct clk *clk;
};

struct du_rsc_mxd_db
{
	struct odin_pm_domain *pm_domain;
	struct device *dev;
	struct clk *clk;
};

struct du_rsc_db
{
	struct du_rsc_ch_db ch[DU_SRC_NUM];
	struct du_rsc_mxd_db mxd;

	unsigned int candidate_ch;
	unsigned int final_ch;
};

static struct du_rsc_db du_rsc;
static struct mutex  du_rsc_lock;

static int _du_runtime_resume(struct device *dev)
{
	struct clk *clk = clk_get(dev, NULL);
	if (IS_ERR_OR_NULL(clk)) {
		pr_err("%s get clk is NULL\n", dev->of_node->name);
		return -1;
	}

	//pr_info("%s %d %d\n", __func__, clk->enable_count, clk->prepare_count);

	clk_enable(clk);
	dss_crg_hal_reset((enum dss_reset)dev->platform_data);
	
	return 0;
}

static int _du_runtime_suspend(struct device *dev)
{
	struct clk *clk = clk_get(dev, NULL);
	if (IS_ERR_OR_NULL(clk)) {
		pr_err("%s get clk is NULL\n", dev->of_node->name);
		return -1;
	}

	//pr_info("%s %d %d\n", __func__, clk->enable_count, clk->prepare_count);

	if (clk->enable_count == 0)
		pr_err("already clk disabled.\n");
	else
		clk_disable(clk);

	return 0;
}

static const struct dev_pm_ops _du_pm_ops = 
{
	.runtime_resume = _du_runtime_resume,
	.runtime_suspend = _du_runtime_suspend,
};

static struct of_device_id _du_vid0_match[] = 
{
	{.name = "vid0", .compatible = "odin,dss,vid0"},
	{},
};

static struct of_device_id _du_vid1_match[] = 
{
	{.name = "vid1", .compatible = "odin,dss,vid1"},
	{},
};

static struct of_device_id _du_vid2_match[] = 
{
	{.name = "vid2", .compatible = "odin,dss,vid2"},
	{},
};

static struct of_device_id _du_gra0_match[] = 
{
	{.name = "gra0", .compatible = "odin,dss,gra0"},
	{},
};

static struct of_device_id _du_gra1_match[] = 
{
	{.name = "gra1", .compatible = "odin,dss,gra1"},
	{},
};

static struct of_device_id _du_gra2_match[] = 
{
	{.name = "gra2", .compatible = "odin,dss,gra2"},
	{},
};

static struct of_device_id _du_gscl_match[] = 
{
	{.name = "gscl", .compatible = "odin,dss,gscl"},
	{},
};

static struct of_device_id _du_mxd_match[] = 
{
	{.name = "xd", .compatible = "odin,dss,xd"},
	{},
};

static struct platform_driver _du_vid0_drv = 
{
	.driver = {
		.name = "du_vid0",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr( _du_vid0_match ),
		.pm = &_du_pm_ops,
	},
};

static struct platform_driver _du_vid1_drv = 
{
	.driver = {
		.name = "du_vid1",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr( _du_vid1_match ),
		.pm = &_du_pm_ops,
	},
};

static struct platform_driver _du_vid2_drv = 
{
	.driver = {
		.name = "du_vid2",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr( _du_vid2_match ),
		.pm = &_du_pm_ops,
	},
};

static struct platform_driver _du_gra0_drv = 
{
	.driver = {
		.name = "du_gra0",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr( _du_gra0_match ),
		.pm = &_du_pm_ops,
	},
};

static struct platform_driver _du_gra1_drv = 
{
	.driver = {
		.name = "du_gra1",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr( _du_gra1_match ),
		.pm = &_du_pm_ops,
	},
};

static struct platform_driver _du_gra2_drv = 
{
	.driver = {
		.name = "du_gra2",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr( _du_gra2_match ),
		.pm = &_du_pm_ops,
	},
};

static struct platform_driver _du_gscl_drv = 
{
	.driver = {
		.name = "du_gscl",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr( _du_gscl_match ),
		.pm = &_du_pm_ops,
	},
};

static struct platform_driver _du_mxd_drv = 
{
	.driver = {
		.name = "du_mxd",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr( _du_mxd_match ),
		.pm = &_du_pm_ops,
	},
};

unsigned int __du_rsc_get_num_of_cap_bit(enum du_src_ch_id rsc_i)
{
	unsigned int pos;
	unsigned int count = 0;
	unsigned int weight_value = 10;
	
	for (pos=0 ; pos< 32; pos++) {
		if (du_rsc.ch[rsc_i].cap.input_format.rgb & (1<<pos))
			count++;
		if (du_rsc.ch[rsc_i].cap.input_format.yuv & (1<<pos))
			count++;
		if (du_rsc.ch[rsc_i].cap.output_format.rgb & (1<<pos))
			count++;
		if (du_rsc.ch[rsc_i].cap.output_format.yuv & (1<<pos))
			count++;
	}

	if (du_rsc.ch[rsc_i].cap.scale) // vid0, vid1
		count+=weight_value;
	if (du_rsc.ch[rsc_i].cap.mixed_wb)
		count+=weight_value*10;

	return count;
}

static bool __du_rsc_check_format(enum du_src_ch_id rsc_i, struct dss_image *input_img, struct dss_image *output_img)
{
	if (input_img->pixel_format <= DSS_IMAGE_FORMAT_RGB_565) {
		if((du_rsc.ch[rsc_i].cap.input_format.rgb & (1 << input_img->pixel_format)) == false)
			return false;
	}
	else 	{
		if ((du_rsc.ch[rsc_i].cap.input_format.yuv & (1<< (input_img->pixel_format -DSS_IMAGE_FORMAT_YUV422_S)))==false)
			return false;
	}

	if (output_img->port == DSS_DISPLAY_PORT_MEM) {
		if (output_img->pixel_format <= DSS_IMAGE_FORMAT_RGB_565) {
			if ((du_rsc.ch[rsc_i].cap.output_format.rgb & (1<< output_img->pixel_format)) == false)
				return false;
		}
		else 	{
			if ((du_rsc.ch[rsc_i].cap.output_format.yuv & (1<< (output_img->pixel_format -DSS_IMAGE_FORMAT_YUV422_S)))==false)
				return false;
		}
	}
	
	return true;
}

static bool __du_rsc_check_using(enum du_src_ch_id rsc_i)
{
	return du_rsc.ch[rsc_i].use;
}

static bool __du_rsc_img_is_scale(struct dss_input_meta *input_meta)
{
	if ((input_meta->crop_rect.size.h != input_meta->dst_rect.size.h) 
		||(input_meta->crop_rect.size.w!= input_meta->dst_rect.size.w)) {
		return true;				
	}

	return false;
}

static bool __du_rsc_img_is_wb(struct dss_image *input_img, struct dss_image *output_img)
{
	if ((input_img->port == DSS_DISPLAY_PORT_MEM)
		&& (output_img->port == DSS_DISPLAY_PORT_MEM))
		return true;

	return false;
}

static bool __du_rsc_img_is_mixed_wb(struct dss_image *input_img, struct dss_image *output_img)
{
	if ((input_img->port <= DSS_DISPLAY_PORT_FB1)
		&& (output_img->port == DSS_DISPLAY_PORT_MEM))
		return true;

	return false;
}

static bool __du_rsc_img_is_mxd(struct dss_input_meta *input_meta)
{
	return input_meta->mxd_op;
}

static bool __du_rsc_cap_is_scale(enum du_src_ch_id rsc_i)
{
	return du_rsc.ch[rsc_i].cap.scale;
}

static bool __du_rsc_cap_is_mxd(enum du_src_ch_id rsc_i)
{
	return du_rsc.ch[rsc_i].cap.mxd;
}

static bool __du_rsc_cap_is_wb(enum du_src_ch_id rsc_i)
{
	return du_rsc.ch[rsc_i].cap.wb;
}

static bool __du_rsc_cap_is_mixed_wb(enum du_src_ch_id rsc_i)
{
	return du_rsc.ch[rsc_i].cap.mixed_wb;
}

unsigned int _du_rsc_find_candidate(struct dss_image *input_img, struct dss_input_meta *input_meta, struct dss_image *output_img)
{
	unsigned int candidate_ch = 0x0; 
	unsigned int rsc_i;

    /* FIXME: temporarily disable GSCL due to page fault - daejong.seo
     * restore later by changing DU_SRC_NUM - 1 to DU_SRC_NUM */
	for (rsc_i = DU_SRC_VID0; rsc_i < DU_SRC_NUM - 1; rsc_i++) {
		if ((__du_rsc_check_using(rsc_i) == true) || (__du_rsc_check_format(rsc_i, input_img, output_img) == false))
			continue;

		if ( __du_rsc_img_is_mixed_wb(input_img, output_img)) { //gscl
			if( __du_rsc_cap_is_mixed_wb(rsc_i)) 
				candidate_ch |= (1<<rsc_i);
		}
		else if (__du_rsc_img_is_mxd(input_meta)) {
			if( __du_rsc_cap_is_mxd(rsc_i))
				candidate_ch |= (1<<rsc_i);
		}
		else if (__du_rsc_img_is_scale(input_meta)) { //vid0, vid1
			 if(__du_rsc_cap_is_scale(rsc_i))  
				candidate_ch |= (1<<rsc_i);
		}
		else if ( __du_rsc_img_is_wb(input_img, output_img)) { //vid0, vid1
			if( __du_rsc_cap_is_wb(rsc_i)) 
				candidate_ch |= (1<<rsc_i);
		}
		else {
			candidate_ch |= (1<<rsc_i);		
		}
	}

	du_rsc.candidate_ch = candidate_ch;
#if 0
	pr_info("%s >> format_i 0x%x, format_o 0x%x, mixed_wb %d, mxd %d, scale %d, wb %d , candidate 0x%x\n", 
			__func__, input_img->pixel_format, output_img->pixel_format,
			__du_rsc_img_is_mixed_wb(input_img, output_img),
			__du_rsc_img_is_mxd(input_meta), 
			__du_rsc_img_is_scale(input_meta),
			__du_rsc_img_is_wb(input_img, output_img),
			candidate_ch);
#endif

	return candidate_ch;
}

unsigned int _du_rsc_select_final(unsigned int candidate)
{
	unsigned int rsc_i;
	unsigned int *final_rsc_i = &du_rsc.final_ch;
	unsigned int num_of_bit=0;
	unsigned int min_num_of_bit=0xFFFFFFFF;

    /* FIXME: temporarily disable GSCL due to page fault - daejong.seo
     * restore later by changing DU_SRC_NUM - 1 to DU_SRC_NUM */
	for (rsc_i = DU_SRC_VID0; rsc_i < DU_SRC_NUM - 1; rsc_i++) {
		if((candidate & (1<<rsc_i)) == 0)
			continue;

		num_of_bit = __du_rsc_get_num_of_cap_bit(rsc_i);
		if (min_num_of_bit > num_of_bit) {
			min_num_of_bit = num_of_bit;
			*final_rsc_i = rsc_i;
		}
	}

	return *final_rsc_i;
}

static void _du_rsc_platform_data_init(struct device *dev, enum du_src_ch_id rsc_i)
{
	switch(rsc_i) {
	case DU_SRC_VID0:
		dev->platform_data = DSS_RESET_VSCL0;
		break;
	case DU_SRC_VID1:
		dev->platform_data = DSS_RESET_VSCL1;
		break;
	case DU_SRC_VID2:
		dev->platform_data = DSS_RESET_VDMA;
		break;
	case DU_SRC_GRA0:
		dev->platform_data = DSS_RESET_GDMA0;
		break;
	case DU_SRC_GRA1:
		dev->platform_data = DSS_RESET_GDMA1;
		break;
	case DU_SRC_GRA2:
		dev->platform_data = DSS_RESET_GDMA2;
		break;
	case DU_SRC_GSCL:
		dev->platform_data = DSS_RESET_GSCL;
		break;
	default:
		pr_warn("invalid rsc_i(%d)\n", rsc_i);
		break;
	}
}

static void _du_rsc_pm_init(struct platform_device *pdev_ch[], struct platform_device *pdev_mxd)
{
	unsigned int rsc_i;
	struct device *dev;
	struct clk *clk;

	for (rsc_i = DU_SRC_VID0; rsc_i < DU_SRC_NUM; rsc_i++) {
		dev = &pdev_ch[rsc_i]->dev;
	
		if (odin_pd_register_dev(dev, du_rsc.ch[rsc_i].pm_domain) < 0) 
			pr_err("%s pd_registe fail\n", dev->of_node->name);

		pm_runtime_enable(dev);

		clk = clk_get(dev, NULL);
		if (IS_ERR_OR_NULL(clk)) 
			pr_err("%s get clk fail", dev->of_node->name);
		else 	if ( clk->prepare_count == 0)
			clk_prepare(clk);

		_du_rsc_platform_data_init(dev, rsc_i);

		du_rsc.ch[rsc_i].dev = dev;
		du_rsc.ch[rsc_i].clk = clk;
	}

	if (pdev_mxd) {
		dev = &pdev_mxd->dev;
		if (odin_pd_register_dev(dev, du_rsc.mxd.pm_domain) < 0) 
				pr_err("%s pd_registe fail\n", dev->of_node->name);

		dev->platform_data = DSS_RESET_MXD;

		pm_runtime_enable(dev);

		clk = clk_get(dev, NULL);
		if (IS_ERR_OR_NULL(clk)) 
			pr_err("%s get clk fail", dev->of_node->name);
		else 	if ( clk->prepare_count == 0)
			clk_prepare(clk);

		du_rsc.mxd.dev = dev;
		du_rsc.mxd.clk = clk;
	}
}

static void _du_rsc_pm_cleanup(void)
{
	unsigned int rsc_i;
		
	for (rsc_i = DU_SRC_VID0; rsc_i < DU_SRC_NUM; rsc_i++) {
		pm_runtime_suspend(du_rsc.ch[rsc_i].dev);

		if (du_rsc.ch[rsc_i].clk->prepare_count == 1)
			clk_unprepare(du_rsc.ch[rsc_i].clk);
	}

	if (du_rsc.mxd.dev) {
		pm_runtime_suspend(du_rsc.mxd.dev);

		if (du_rsc.mxd.clk->prepare_count == 1)
			clk_unprepare(du_rsc.mxd.clk); 
	}			
}

void _du_rsc_pm_on(enum du_src_ch_id rsc_i)
{
	if (rsc_i == DU_SRC_VID1 || rsc_i == DU_SRC_VID2)
		pm_runtime_get_sync(du_rsc.ch[DU_SRC_VID0].dev);
	
	pm_runtime_get_sync(du_rsc.ch[rsc_i].dev);
}

void _du_rsc_pm_off(enum du_src_ch_id rsc_i)
{
	pm_runtime_put_sync(du_rsc.ch[rsc_i].dev);

	if (rsc_i == DU_SRC_VID1 || rsc_i == DU_SRC_VID2)
		pm_runtime_put_sync(du_rsc.ch[DU_SRC_VID0].dev);
}

enum du_src_ch_id du_rsc_ch_alloc(struct dss_image *input_img, struct dss_input_meta *input_meta, struct dss_image *output_img)
{
	unsigned int rsc_candidate_i;
	unsigned int rsc_final_i = DU_SRC_INVALID;

	mutex_lock(&du_rsc_lock );

	rsc_candidate_i = _du_rsc_find_candidate(input_img, input_meta, output_img);
	if (rsc_candidate_i == 0x0) {
		pr_err("No candidate ch\n");
		mutex_unlock( &du_rsc_lock );
		return (enum du_src_ch_id)rsc_final_i;
	}

	rsc_final_i =  _du_rsc_select_final(rsc_candidate_i);
	if (rsc_final_i != DU_SRC_INVALID) {
		du_rsc.ch[rsc_final_i].use = true;
	}
	else 
		pr_err("final rsc is error %d\n", rsc_final_i);
	
	mutex_unlock( &du_rsc_lock );

//	pr_info("%s >> candidate 0x%08X, final %d\n", __func__, du_rsc.candidate_ch, rsc_final_i);
	_du_rsc_pm_on(rsc_final_i);

	return rsc_final_i;
}

bool du_rsc_ch_free(enum du_src_ch_id rsc_i)
{
	bool ret = false;

	mutex_lock( &du_rsc_lock  );

	if (__du_rsc_check_using(rsc_i) == true) {
		du_rsc.ch[rsc_i].use = false;
		ret =  true;
	}
	else 
		pr_err("already free rsc %d\n", rsc_i);

	mutex_unlock(&du_rsc_lock );

	if (ret == true)
		_du_rsc_pm_off(rsc_i);
	
	return ret;
}

void du_rsc_init(struct platform_device *pdev_ch[], struct platform_device *pdev_mxd)
{
	int ret;
	unsigned int image_format_i;

	memset(&du_rsc, 0, sizeof(struct du_rsc_db));	

	//rgb
	for (image_format_i = DSS_IMAGE_FORMAT_RGBA_8888; image_format_i<=DSS_IMAGE_FORMAT_RGB_565; image_format_i++) {
		du_rsc.ch[DU_SRC_VID0].cap.input_format.rgb |= (1<<image_format_i);
		du_rsc.ch[DU_SRC_VID1].cap.input_format.rgb |= (1<<image_format_i);
		du_rsc.ch[DU_SRC_VID2].cap.input_format.rgb |= (1<<image_format_i);
		du_rsc.ch[DU_SRC_GRA0].cap.input_format.rgb |= (1<<image_format_i);
		du_rsc.ch[DU_SRC_GRA1].cap.input_format.rgb |= (1<<image_format_i);
		du_rsc.ch[DU_SRC_GRA2].cap.input_format.rgb |= (1<<image_format_i);
		du_rsc.ch[DU_SRC_GSCL].cap.input_format.rgb |= (1<<image_format_i);

		du_rsc.ch[DU_SRC_VID1].cap.output_format.rgb |= (1<<image_format_i);
		du_rsc.ch[DU_SRC_VID2].cap.output_format.rgb |= (1<<image_format_i);
		du_rsc.ch[DU_SRC_VID0].cap.output_format.rgb |= (1<<image_format_i);
		du_rsc.ch[DU_SRC_GSCL].cap.output_format.rgb |= (1<<image_format_i);
	}

	//yuv
	for (image_format_i = 0; image_format_i<=DSS_IMAGE_FORMAT_YV_12 - DSS_IMAGE_FORMAT_YUV422_S; image_format_i++) {
		du_rsc.ch[DU_SRC_VID0].cap.input_format.yuv |= (1<<image_format_i);
		du_rsc.ch[DU_SRC_VID1].cap.input_format.yuv |= (1<<image_format_i);
		du_rsc.ch[DU_SRC_VID2].cap.input_format.yuv |= (1<<image_format_i);
		
		du_rsc.ch[DU_SRC_VID0].cap.output_format.yuv |= (1<<image_format_i);
		du_rsc.ch[DU_SRC_VID1].cap.output_format.yuv |= (1<<image_format_i);
		du_rsc.ch[DU_SRC_VID2].cap.output_format.yuv |= (1<<image_format_i);
	}
	du_rsc.ch[DU_SRC_GRA0].cap.output_format.yuv = (1<<(DSS_IMAGE_FORMAT_YUV420_2P-DSS_IMAGE_FORMAT_YUV422_S));
	du_rsc.ch[DU_SRC_GRA1].cap.output_format.yuv = (1<<(DSS_IMAGE_FORMAT_YUV420_2P-DSS_IMAGE_FORMAT_YUV422_S));
	du_rsc.ch[DU_SRC_GRA2].cap.output_format.yuv = (1<<(DSS_IMAGE_FORMAT_YUV420_2P-DSS_IMAGE_FORMAT_YUV422_S));
	du_rsc.ch[DU_SRC_GSCL].cap.output_format.yuv = (1<<(DSS_IMAGE_FORMAT_YUV420_2P-DSS_IMAGE_FORMAT_YUV422_S));

	du_rsc.ch[DU_SRC_VID0].cap.scale = true;
	du_rsc.ch[DU_SRC_VID0].cap.mxd = true;
	du_rsc.ch[DU_SRC_VID0].cap.wb = true;
	du_rsc.ch[DU_SRC_VID1].cap.scale = true;
	du_rsc.ch[DU_SRC_VID1].cap.mxd = true;
	du_rsc.ch[DU_SRC_VID1].cap.wb = true;
	du_rsc.ch[DU_SRC_GSCL].cap.mixed_wb = true;

	du_rsc.ch[DU_SRC_VID0].pm_domain = &odin_pd_dss1_vid0;
	du_rsc.ch[DU_SRC_VID1].pm_domain = &odin_pd_dss1_vid1;
	du_rsc.ch[DU_SRC_VID2].pm_domain = &odin_pd_dss1_vid2;
	du_rsc.ch[DU_SRC_GRA0].pm_domain = &odin_pd_dss3_gra0;
	du_rsc.ch[DU_SRC_GRA1].pm_domain = &odin_pd_dss3_gra1;
	du_rsc.ch[DU_SRC_GRA2].pm_domain = &odin_pd_dss3_gra2;
	du_rsc.ch[DU_SRC_GSCL].pm_domain = &odin_pd_dss3_gscl;
	du_rsc.mxd.pm_domain = &odin_pd_dss2;

	mutex_init(&du_rsc_lock);

	ret = platform_driver_register(&_du_vid0_drv);
	if ( ret < 0 ) {
		pr_err("du_vid0 platform_driver_register failed\n");
	}
	ret = platform_driver_register(&_du_vid1_drv);
	if ( ret < 0 ) {
		pr_err("du_vid1 platform_driver_register failed\n");
		platform_driver_unregister(&_du_vid0_drv);
	}
	ret = platform_driver_register(&_du_vid2_drv);
	if ( ret < 0 ) {
		pr_err("du_vid2 platform_driver_register failed\n");
		platform_driver_unregister(&_du_vid0_drv);
		platform_driver_unregister(&_du_vid1_drv);
	}
	ret = platform_driver_register(&_du_gra0_drv);
	if ( ret < 0 ) {
		pr_err("du_gra0 platform_driver_register failed\n");
		platform_driver_unregister(&_du_vid0_drv);
		platform_driver_unregister(&_du_vid1_drv);
		platform_driver_unregister(&_du_vid2_drv);
	}
	ret = platform_driver_register(&_du_gra1_drv);
	if ( ret < 0 ) {
		pr_err("du_gra1 platform_driver_register failed\n");
		platform_driver_unregister(&_du_vid0_drv);
		platform_driver_unregister(&_du_vid1_drv);
		platform_driver_unregister(&_du_vid2_drv);
		platform_driver_unregister(&_du_gra0_drv);
	}
	ret = platform_driver_register(&_du_gra2_drv);
	if ( ret < 0 ) {
		pr_err("du_gra2 platform_driver_register failed\n");
		platform_driver_unregister(&_du_vid0_drv);
		platform_driver_unregister(&_du_vid1_drv);
		platform_driver_unregister(&_du_vid2_drv);
		platform_driver_unregister(&_du_gra0_drv);
		platform_driver_unregister(&_du_gra1_drv);
	}
	ret = platform_driver_register(&_du_gscl_drv);
	if ( ret < 0 ) {
		pr_err("du_gscl platform_driver_register failed\n");
		platform_driver_unregister(&_du_vid0_drv);
		platform_driver_unregister(&_du_vid1_drv);
		platform_driver_unregister(&_du_vid2_drv);
		platform_driver_unregister(&_du_gra0_drv);
		platform_driver_unregister(&_du_gra1_drv);
		platform_driver_unregister(&_du_gra2_drv);
	}
	ret = platform_driver_register(&_du_mxd_drv);
	if ( ret < 0 ) {
		pr_err("du_mxd platform_driver_register failed\n");
		platform_driver_unregister(&_du_vid0_drv);
		platform_driver_unregister(&_du_vid1_drv);
		platform_driver_unregister(&_du_vid2_drv);
		platform_driver_unregister(&_du_gra0_drv);
		platform_driver_unregister(&_du_gra1_drv);
		platform_driver_unregister(&_du_gra2_drv);
		platform_driver_unregister(&_du_gscl_drv);
	}

	_du_rsc_pm_init(pdev_ch, pdev_mxd);
}

void du_rsc_cleanup(void)
{
	platform_driver_unregister(&_du_vid0_drv);
	platform_driver_unregister(&_du_vid1_drv);
	platform_driver_unregister(&_du_vid2_drv);
	platform_driver_unregister(&_du_gra0_drv);
	platform_driver_unregister(&_du_gra1_drv);
	platform_driver_unregister(&_du_gra2_drv);
	platform_driver_unregister(&_du_gscl_drv);
	platform_driver_unregister(&_du_mxd_drv);

	_du_rsc_pm_cleanup();
}

