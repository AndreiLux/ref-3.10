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

#include <linux/errno.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/odin_iommu.h>
#include <linux/odin_mailbox.h>
#include <linux/odin_pm_domain.h>
#include <linux/string.h>

#include <media/odin/vcodec/vcore/device.h>
#include <media/odin/vcodec/vcore/decoder.h>
#include <media/odin/vcodec/vcore/encoder.h>
#include <media/odin/vcodec/vcore/ppu.h>
#include <media/odin/vcodec/vlog.h>

#define	VCORE_MAX_RUNNING_WEIGHT	(1920 * 1088 * 60)
//#define	VCORE_CLK_CONT_ON

static spinlock_t lock_vcore_list_head;
static LIST_HEAD(vcore_list_head);

struct clk_node{
	struct clk *clk;
	struct list_head list;
};

struct vcodec_vcore
{
	struct list_head list;
	struct list_head clk_list_head;
	struct platform_device *pdev;
	struct platform_driver *pdriver;
	int irq;
	int clk_enable_cnt;
	int clk_prepare_cnt;
	struct vcore_entry vcore_entry;
};

struct odin_pm_domain *odin_vsp_pm_domains[] = {
	&odin_pd_vsp1,
	&odin_pd_vsp2,
	&odin_pd_vsp3,
	&odin_pd_vsp4_jpeg,
	&odin_pd_vsp4_png,
};

void vcore_resume(struct device *dev)
{
	vlog_info("%s\n",dev->driver->name);
	pm_runtime_get_sync(dev);
}

void vcore_suspend(struct device *dev)
{
	vlog_info("%s\n",dev->driver->name);
	pm_runtime_put_sync(dev);
}

static void _vcodec_vcore_put(void *id)
{
	struct vcodec_vcore *vcore;
	vcore = (struct vcodec_vcore *)id;
	vcore_suspend(&vcore->pdev->dev);
}

void* vcodec_vcore_get_dec(enum vcore_dec_codec codec,
								struct vcore_dec_ops *ops,
								bool rendering_dpb)
{
	struct list_head *tmp;
	struct list_head *n;
	struct vcodec_vcore *vcore;
	struct vcodec_vcore *ret_vcore = NULL;

	unsigned long utilization_vcore_min = 0xFFFFFFFF;
	unsigned long utilization_vcore;
	struct vcore_entry *vcore_entry = NULL;

	spin_lock(&lock_vcore_list_head);
	list_for_each_safe(tmp, n, &vcore_list_head) {
		vcore = list_entry(tmp, struct vcodec_vcore, list);

		if (vcore->vcore_entry.dec.init == NULL ||
				(vcore->vcore_entry.dec.capability & (1<<codec)) == 0)
			continue;

		utilization_vcore = vcore->vcore_entry.get_utilization();
		/* vlog_info("%s running_weight :0x%x\n",
		    vcore->vcore_entry.name, running_weight); */

		if (utilization_vcore_min == 0xFFFFFFFF) {
			utilization_vcore_min = utilization_vcore;
			vcore_entry = &vcore->vcore_entry;
			ret_vcore = vcore;
		}
		else if ((utilization_vcore_min + 10) > utilization_vcore) {
			if (utilization_vcore_min > (utilization_vcore + 10)) {
				utilization_vcore_min = utilization_vcore;
				vcore_entry = &vcore->vcore_entry;
				ret_vcore = vcore;
			}
			else {
				if (vcore->vcore_entry.enc.init == NULL) { /* decoder only */
					utilization_vcore_min = utilization_vcore;
					vcore_entry = &vcore->vcore_entry;
					ret_vcore = vcore;
				}
			}
		}
	}
	spin_unlock(&lock_vcore_list_head);

	if (vcore_entry == NULL || ret_vcore == NULL) {
		vlog_error("failed to alloc vcore\n");
		return NULL;
	}


	if (rendering_dpb == true) {
		if (utilization_vcore_min > 60) {
			vlog_error("full utilization: %lu\n", utilization_vcore_min);
			return NULL;
		}
	}

	vcore_resume(&ret_vcore->pdev->dev);

	vcore_entry->dec.init(ops);

	return (void*)ret_vcore;
}

void vcodec_vcore_put_dec(void *id)
{
	return _vcodec_vcore_put(id);
}

void* vcodec_vcore_get_enc(enum vcore_enc_codec codec,
							struct vcore_enc_ops *ops,
							unsigned int width,
							unsigned int height,
							unsigned int fr_residual,
							unsigned int fr_divider)
{
	struct list_head *tmp;
	struct list_head *n;
	struct vcodec_vcore *vcore;
	struct vcodec_vcore *ret_vcore = NULL;
	struct vcore_entry *vcore_entry = NULL;

	switch (codec) {
	case VCORE_ENC_AVC :
	case VCORE_ENC_MPEG4 :
	case VCORE_ENC_H263 :
		if ((width == 0) || (height == 0) ||
			(fr_residual == 0) || (fr_divider == 0)) {
			vlog_error("input param: size:%u*%u, frame rate:%u/%u\n",
						width, height, fr_residual, fr_divider);
		}
		else {
			unsigned long utilization_vcore_max = 0x0;
			unsigned long estimated_util_vcore;

			spin_lock(&lock_vcore_list_head);
			list_for_each_safe(tmp, n, &vcore_list_head) {
				vcore = list_entry(tmp, struct vcodec_vcore, list);

				if (vcore->vcore_entry.enc.init == NULL ||
						(vcore->vcore_entry.enc.capability & (1<<codec)) == 0)
					continue;

				if (vcore->vcore_entry.get_util_estimation == NULL) {
					vlog_error("get_util_estimation is not defined");
					continue;
				}

				estimated_util_vcore = vcore->vcore_entry.get_util_estimation(
														width, height,
														fr_residual, fr_divider);
				vlog_trace("estimated utilization of vcore:%lu\n", estimated_util_vcore);

				if (utilization_vcore_max < estimated_util_vcore) {
					utilization_vcore_max = estimated_util_vcore;
					vcore_entry = &vcore->vcore_entry;
					ret_vcore = vcore;
				}
			}
			spin_unlock(&lock_vcore_list_head);
		}
		break;
	case VCORE_ENC_JPEG :
		{
			unsigned int utilization_vcore_min = 0xFFFFFFFF;
			unsigned int utilization_vcore;

			spin_lock(&lock_vcore_list_head);
			list_for_each_safe(tmp, n, &vcore_list_head) {
				vcore = list_entry(tmp, struct vcodec_vcore, list);

				if (vcore->vcore_entry.enc.init == NULL ||
						(vcore->vcore_entry.enc.capability & (1<<codec)) == 0)
					continue;

				utilization_vcore = vcore->vcore_entry.get_utilization();

				if (utilization_vcore_min > utilization_vcore) {
					utilization_vcore_min = utilization_vcore;
					vcore_entry = &vcore->vcore_entry;
					ret_vcore = vcore;
				}
			}
			spin_unlock(&lock_vcore_list_head);
		}
		break;
	default :
		vlog_error("invalid codec type: %u\n", codec);
	}

	if (vcore_entry == NULL || ret_vcore == NULL) {
		vlog_error("failed to alloc vcore\n");
		return NULL;
	}

	if (ret_vcore->pdev) {
		vcore_resume(&ret_vcore->pdev->dev);
	}

	vcore_entry->enc.init(ops);

	return (void*)ret_vcore;
}

void vcodec_vcore_put_enc(void *id)
{
	return _vcodec_vcore_put(id);
}

void* vcodec_vcore_get_ppu(struct vcore_ppu_ops *ops, bool without_fail,
							unsigned int width, unsigned int height,
							unsigned int fr_residual, unsigned int fr_divider)
{
	struct list_head *tmp;
	struct list_head *n;
	struct vcodec_vcore *vcore = NULL;
	struct vcore_entry *vcore_entry = NULL;
	struct vcodec_vcore *ret_vcore = NULL;

	if ((width == 0) ||
		(height == 0) ||
		(fr_residual == 0) ||
		(fr_divider == 0)) {
		vlog_error("input param: size:%u*%u, frame rate:%u/%u\n",
					width, height, fr_residual, fr_divider);
	}
	else {
		unsigned long utilization_vcore_max = 0x0;
		unsigned long estimated_util_vcore;

		spin_lock(&lock_vcore_list_head);
		list_for_each_safe(tmp, n, &vcore_list_head) {
			vcore = list_entry(tmp, struct vcodec_vcore, list);

			if (vcore->vcore_entry.ppu.init == NULL)
				continue;

			if (vcore->vcore_entry.get_util_estimation == NULL) {
				vlog_error("get_util_estimation is not defined");
				continue;
			}

			estimated_util_vcore = vcore->vcore_entry.get_util_estimation(
													width, height,
													fr_residual, fr_divider);
			vlog_trace("estimated utilization of vcore:%lu\n", estimated_util_vcore);

			if (utilization_vcore_max < estimated_util_vcore) {
				utilization_vcore_max = estimated_util_vcore;
				vcore_entry = &vcore->vcore_entry;
				ret_vcore = vcore;
			}
		}

		spin_unlock(&lock_vcore_list_head);
	}

	if (without_fail == true) {
		if (vcore_entry == NULL) {
			unsigned int utilization_vcore_min = 0xFFFFFFFF;
			unsigned int utilization_vcore;

			vlog_warning("not enough vcore capability, but try to alloc\n");
			spin_lock(&lock_vcore_list_head);
			list_for_each_safe(tmp, n, &vcore_list_head) {
				vcore = list_entry(tmp, struct vcodec_vcore, list);

				if (vcore->vcore_entry.ppu.init == NULL)
					continue;

				utilization_vcore = vcore->vcore_entry.get_utilization();

				if (utilization_vcore_min > utilization_vcore) {
					utilization_vcore_min = utilization_vcore;
					vcore_entry = &vcore->vcore_entry;
					ret_vcore = vcore;
				}
			}
			spin_unlock(&lock_vcore_list_head);
		}
	}

	if (vcore_entry == NULL || ret_vcore == NULL) {
		vlog_error("failed to alloc vcore\n");
		return NULL;
	}

	if (vcore && vcore->pdev && ret_vcore) {
		vcore_resume(&ret_vcore->pdev->dev);
	}

	vcore_entry->ppu.init(ops);

	return (void*)vcore;
}

void vcodec_vcore_put_ppu(void *id)
{
	return _vcodec_vcore_put(id);
}

static void _vcodec_vcore_init(void)
{
	spin_lock_init(&lock_vcore_list_head);
}

struct vcodec_vcore *_vcore_search_by_name(const char* vcore_name)
{
	struct list_head *head;
	struct vcodec_vcore *node;

	head = &vcore_list_head;
	list_for_each_entry(node,head,list) {
		if (!strcmp(node->vcore_entry.vcore_name, vcore_name)) {
			return node;
		}
	}
	vlog_error("[%s] vcore is not found\n", vcore_name);
	return NULL;
}

struct vcodec_vcore *_vcore_search_by_devptr(
	const struct platform_device* pdev)
{
	char *vcore_name;
	vcore_name = strrchr(pdev->name,'.')+sizeof(char);
	return _vcore_search_by_name(vcore_name);
}

void _vcore_clock_on(char* vcore_name)
{
	struct vcodec_vcore *vcore;
	struct list_head *head;
	struct clk_node *node;

	vcore = _vcore_search_by_name(vcore_name);
	if (IS_ERR_OR_NULL((const void*)vcore)) {
		vlog_error("vcore is ERR_OR_NULL\n");
		return;
	}

	head = &vcore->clk_list_head;
	vcore->clk_enable_cnt++;
	list_for_each_entry_reverse(node,head,list) {
#ifdef VCORE_CLK_CONT_ON
		clk_enable(node->clk);
#endif
	}
}

void _vcore_clock_off(char* vcore_name)
{
	struct vcodec_vcore *vcore;
	struct list_head *head;
	struct clk_node *node;

	vcore = _vcore_search_by_name(vcore_name);
	if (IS_ERR_OR_NULL((const void*)vcore)) {
		vlog_error("vcore is ERR_OR_NULL\n");
		return;
	}

	head = &vcore->clk_list_head;
	vcore->clk_enable_cnt--;
	list_for_each_entry(node,head,list) {
#ifdef VCORE_CLK_CONT_ON
		clk_disable(node->clk);
#endif
	}
}

int vcore_runtime_resume(struct device *dev)
{
	struct vcodec_vcore *vcore;
	struct list_head *head;
	struct clk_node *node;

	vcore = _vcore_search_by_name(dev->driver->name);
	if (IS_ERR_OR_NULL((const void*)vcore)) {
		vlog_error("vcore is ERR_OR_NULL\n");
		return -1;
	}
	vcore->clk_prepare_cnt++;
	vlog_info("[%s] power.usage_count: %d, prepare:%d, enable:%d\n",
		dev->driver->name ,vcore->pdev->dev.power.usage_count.counter,
		vcore->clk_prepare_cnt, vcore->clk_enable_cnt);

	head = &vcore->clk_list_head;
	list_for_each_entry(node,head,list) {
		clk_prepare(node->clk);
	}
	list_for_each_entry_reverse(node,head,list) {
		clk_enable(node->clk);
	}

	if (vcore->vcore_entry.runtime_resume()< 0) {
		vlog_error("vcore_entry.runtime_resume is failed\n");
		return -1;
	}

	list_for_each_entry(node,head,list) {
#ifdef VCORE_CLK_CONT_ON
		clk_disable(node->clk);
#endif
	}

	return 0;
}

int vcore_runtime_suspend(struct device *dev)
{
	struct vcodec_vcore *vcore;
	struct list_head *head;
	struct clk_node *node;

	vcore = _vcore_search_by_name(dev->driver->name);
	if (IS_ERR_OR_NULL((const void*)vcore)) {
		vlog_error("vcore is ERR_OR_NULL\n");
		return -1;
	}

	head = &vcore->clk_list_head;
	vcore->clk_prepare_cnt--;
	vlog_info("[%s] power.usage_count: %d, prepare:%d, enable:%d\n",
		vcore->vcore_entry.vcore_name ,vcore->pdev->dev.power.usage_count.counter,
		vcore->clk_prepare_cnt, vcore->clk_enable_cnt);
	list_for_each_entry(node,head,list) {
#ifndef VCORE_CLK_CONT_ON
		clk_disable(node->clk);
#endif
		clk_unprepare(node->clk);
	}

	if (vcore->vcore_entry.runtime_suspend() < 0) {
		vlog_error("vcore_entry.runtime_suspend is failed\n");
		return -1;
	}
	return 0;
}


struct odin_pm_domain *_pd_search_by_name(char* gpd_name)
{
	int idx;
	for (idx = 0; idx < ARRAY_SIZE(odin_vsp_pm_domains); idx++) {
		if (!strcmp(odin_vsp_pm_domains[idx]->gpd.name,gpd_name)) {
			return odin_vsp_pm_domains[idx];
		}
	}
	vlog_error("[%s] gpd is not found\n", gpd_name);
	return NULL;
}

int _vcore_clk_get(struct vcodec_vcore *vcore)
{
	int num_of_clk;
	int index_loop;
	struct list_head *clk_list_head;
	struct clk_node *clk_node;
	struct vcore_entry *vcore_entry;

	clk_list_head = &vcore->clk_list_head;
	vcore_entry = &vcore->vcore_entry;
	if (!IS_ERR_OR_NULL((const void*)vcore_entry)) {
		num_of_clk = vcore_entry->clk.num;
		for (index_loop=0; index_loop < num_of_clk; index_loop++) {
			clk_node = vzalloc(sizeof(struct clk_node));
			clk_node->clk = clk_get(NULL, vcore_entry->clk.name[index_loop]);
			list_add(&clk_node->list, clk_list_head);
			vlog_info("%s\n",clk_node->clk->name);
		}
		return 0;
	}
	vlog_error("is failed.\n");
	return -1;
}

int _vcore_clk_put(struct vcodec_vcore *vcore)
{
	int num_of_clk;
	int index_loop;
	struct list_head *clk_list_head;
	struct clk_node *clk_node;
	struct vcore_entry *vcore_entry;

	clk_list_head = &vcore->clk_list_head;
	vcore_entry = &vcore->vcore_entry;
	if (!IS_ERR_OR_NULL((const void*)vcore_entry)) {
		num_of_clk = vcore_entry->clk.num;
		for (index_loop=0; index_loop < num_of_clk; index_loop++) {
			clk_node = list_entry(clk_list_head->next,struct clk_node ,list);
			list_del(&clk_node->list);
			vlog_info("%s\n",clk_node->clk->name);
			clk_put(clk_node->clk);
			vfree(clk_node);
		}
		return 0;
	}
	vlog_error("is failed.\n");
	return -1;
}

static irqreturn_t _vcore_isr(int irq, void *isr)
{
	((void (*)(void))isr)();
	return IRQ_HANDLED;
}

static int _vcore_probe(struct platform_device* pdev)
{
	struct vcodec_vcore *vcore;
	struct vcore_entry *vcore_entry;
	struct odin_pm_domain *odin_pd_vsp;
	struct resource res;
	int irq;
	unsigned int reg_base;

	vcore = _vcore_search_by_devptr(pdev);
	if (IS_ERR_OR_NULL((const void*)vcore)) {
		vlog_error("[%s] vcore is not found\n",pdev->name);
		return -1;
	}
	vcore_entry = &vcore->vcore_entry;
	if (of_address_to_resource(pdev->dev.of_node, 0, &res) < 0)
		return -1;

	irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	vcore->irq = irq;
	reg_base = (unsigned int)res.start;
	if (request_irq(irq, _vcore_isr, 0,
		pdev->dev.of_node->name, vcore_entry->vcore_isr) < 0) {
		vlog_error("request_irq()failed. irq:%d,device_name:%s\n", irq, pdev->name);
		return -1;
	}

	odin_pd_vsp = _pd_search_by_name(vcore_entry->gpd_name);
	if (IS_ERR_OR_NULL((const void *)odin_pd_vsp)) {
		vlog_error("_pd_search_by_name failed\n");
		return -1;
	}

	if (odin_pd_register_dev(&pdev->dev, odin_pd_vsp) < 0) {
		vlog_warning("[%s] odin_pd_register is already registered\n",
			vcore_entry->gpd_name);
	}

	pm_runtime_enable(&pdev->dev);

	vcore->pdev = pdev;
	if (IS_ERR_OR_NULL((const void*)vcore_entry->vcore_register_done)) {
		vlog_error("[0x%08X]vcore_entry->vcore_register_done is NULL OR ERR\n",
			(unsigned int)vcore_entry->vcore_register_done);
		return -1;
	}

	if (_vcore_clk_get(vcore) < 0) {
		return -1;
	}

	if (vcore_entry->vcore_register_done(
		reg_base, _vcore_clock_on, _vcore_clock_off) < 0) {
		return-1;
	}

	vlog_info("[%s] vcore probed\n", pdev->name);

	return 0;
}

static int _vcore_remove(struct platform_device* pdev)
{
	pm_runtime_suspend(&pdev->dev);

	vlog_info("[%s] vcore removed\n",pdev->name);

	return 0;
}

static const struct dev_pm_ops vcore_pm_ops =
{
	.runtime_resume = vcore_runtime_resume,
	.runtime_suspend = vcore_runtime_suspend,
};


int vcore_register_request(struct vcore_entry* ops)
{
	struct vcodec_vcore *vcore;
	struct of_device_id *vcore_match;
	struct platform_driver *vcore_driver;
	int ret = 0;

	if (IS_ERR_OR_NULL((const void *)ops)) {
		vlog_error("vcore_entry is NULL OR ERR\n");
		return -EINVAL;
	}

	if (list_empty(&vcore_list_head)) {
		_vcodec_vcore_init();
	}

	vcore = vzalloc(sizeof(struct vcodec_vcore));
	if (IS_ERR_OR_NULL((const void *) vcore)) {
		vlog_error("vmalloc failed\n");
		ret = -ENOMEM;
		goto err_register_request_vcore;
	}

	vcore_driver = vzalloc(sizeof(struct platform_driver));
	if (IS_ERR_OR_NULL((const void *) vcore_driver)) {
		vlog_error("vcore_driver vmalloc failed\n");
		ret = -ENOMEM;
		goto err_register_request_vcore_driver;
	}

	vcore_match  = vzalloc(sizeof(struct of_device_id));
	if (IS_ERR_OR_NULL((const void *) vcore_match)) {
		vlog_error("vcore_driver vmalloc failed\n");
		ret = -ENOMEM;
		goto err_register_request_vcore_match;
	}

	INIT_LIST_HEAD(&vcore->clk_list_head);

	vcore->vcore_entry = *ops;

	vlog_info("name:%s dec_cap:0x%x enc_cap:0x%x\n",
		ops->vcore_name, ops->dec.capability, ops->enc.capability);

	if (strlen(ops->vcore_name) < sizeof(vcore_match->name)) {
		strcpy(vcore_match->name, ops->vcore_name);
	}
	strcpy(vcore_match->compatible, "odin,");
	if (strlen(vcore_match->compatible) + strlen(ops->vcore_name) < 128) {
		strcat(vcore_match->compatible, ops->vcore_name);
	}
	else {
		vlog_error("vcore_match->compatible can not be composed\n");
		ret = -EINVAL;
		goto err_register_request_compoing_compatible;
	}

	vcore_driver->driver.name = ops->vcore_name;
	vcore_driver->driver.owner = THIS_MODULE;
	vcore_driver->driver.of_match_table = of_match_ptr(vcore_match);
	vcore_driver->driver.pm = &vcore_pm_ops;
	vcore_driver->probe = _vcore_probe;
	vcore_driver->remove = _vcore_remove;
	vcore->pdriver = vcore_driver;

	spin_lock(&lock_vcore_list_head);
	list_add(&vcore->list, &vcore_list_head);
	spin_unlock(&lock_vcore_list_head);

	if (platform_driver_register(vcore_driver) < 0) {
		vlog_error("platform_driver_register failed\n");
		ret = -EINVAL;
		goto err_register_request_driver_reg;
	}

	return ret;

err_register_request_driver_reg :
	spin_lock(&lock_vcore_list_head);
	list_del(&vcore->list);
	spin_unlock(&lock_vcore_list_head);

err_register_request_compoing_compatible :
	vfree(vcore_match);

err_register_request_vcore_match :
	vfree(vcore_driver);

err_register_request_vcore_driver :
	vfree(vcore);

err_register_request_vcore :
	return ret;

}

int vcore_unregister_request(const char *vcore_name)
{
	struct vcodec_vcore *vcore;

	vcore = _vcore_search_by_name(vcore_name);
	if (IS_ERR_OR_NULL((const void *) vcore)) {
		return -1;
	}
	platform_driver_unregister(vcore->pdriver);

	free_irq(vcore->irq, vcore->vcore_entry.vcore_isr);

	vfree(vcore->pdriver);
	_vcore_clk_put(vcore);
	vcore->vcore_entry.vcore_remove();
	list_del(&vcore->list);
	vfree(vcore);
	vlog_info("done\n");
	return 0;
}


EXPORT_SYMBOL(vcore_register_request);
EXPORT_SYMBOL(vcore_unregister_request);
