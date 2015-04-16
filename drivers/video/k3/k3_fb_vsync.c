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

#define VSYNC_TIMEOUT_MSEC 100

#ifdef CONFIG_REPORT_VSYNC
extern void mali_kbase_pm_report_vsync(int);
#endif
extern int mipi_dsi_ulps_cfg(struct k3_fb_data_type *k3fd, int enable);
extern bool k3_dss_check_reg_reload_status(struct k3_fb_data_type *k3fd);

void k3fb_vsync_isr_handler(struct k3_fb_data_type *k3fd)
{
	struct k3fb_vsync *vsync_ctrl = NULL;
	int buffer_updated = 0;
	ktime_t pre_vsync_timestamp;

	BUG_ON(k3fd == NULL);
	vsync_ctrl = &(k3fd->vsync_ctrl);
	BUG_ON(vsync_ctrl == NULL);

	pre_vsync_timestamp = vsync_ctrl->vsync_timestamp;
	vsync_ctrl->vsync_timestamp = ktime_get();
	wake_up_interruptible_all(&(vsync_ctrl->vsync_wait));

	if (k3fd->panel_info.vsync_ctrl_type != VSYNC_CTRL_NONE) {
		spin_lock(&vsync_ctrl->spin_lock);
		if (vsync_ctrl->vsync_ctrl_expire_count) {
			vsync_ctrl->vsync_ctrl_expire_count--;
			if (vsync_ctrl->vsync_ctrl_expire_count == 0)
				schedule_work(&vsync_ctrl->vsync_ctrl_work);
		}
		spin_unlock(&vsync_ctrl->spin_lock);
	}

	if (vsync_ctrl->vsync_report_fnc) {
		if (k3fd->vsync_ctrl.vsync_enabled) {
			buffer_updated = atomic_dec_return(&(vsync_ctrl->buffer_updated));
		} else {
			buffer_updated = 1;
		}

		if (buffer_updated < 0) {
			atomic_cmpxchg(&(vsync_ctrl->buffer_updated), buffer_updated, 1);
		} else {
			vsync_ctrl->vsync_report_fnc(buffer_updated);
		}
	}

	if (g_debug_online_vsync) {
		K3_FB_INFO("fb%d, VSYNC=%llu, time_diff=%llu.\n", k3fd->index,
			ktime_to_ns(k3fd->vsync_ctrl.vsync_timestamp),
			(ktime_to_ns(k3fd->vsync_ctrl.vsync_timestamp) - ktime_to_ns(pre_vsync_timestamp)));
	}
}

static int vsync_timestamp_changed(struct k3_fb_data_type *k3fd,
	ktime_t prev_timestamp)
{
	BUG_ON(k3fd == NULL);
	return !ktime_equal(prev_timestamp, k3fd->vsync_ctrl.vsync_timestamp);
}

#if defined(CONFIG_K3_FB_VSYNC_THREAD)
static int wait_for_vsync_thread(void *data)
{
	struct k3_fb_data_type *k3fd = (struct k3_fb_data_type *)data;
	ktime_t prev_timestamp;
	int ret = 0;

	while (!kthread_should_stop()) {
		prev_timestamp = k3fd->vsync_ctrl.vsync_timestamp;
		ret = wait_event_interruptible_timeout(
			k3fd->vsync_ctrl.vsync_wait,
			vsync_timestamp_changed(k3fd, prev_timestamp) &&
			k3fd->vsync_ctrl.vsync_enabled,
			msecs_to_jiffies(VSYNC_TIMEOUT_MSEC));
		/*if (ret == 0) {
			K3_FB_ERR("wait vsync timeout!");
			return -ETIMEDOUT;
		}*/

		if (ret > 0) {
			char *envp[2];
			char buf[64];
			/* fb%d_VSYNC=%llu */
			snprintf(buf, sizeof(buf), "VSYNC=%llu",
				ktime_to_ns(k3fd->vsync_ctrl.vsync_timestamp));
			envp[0] = buf;
			envp[1] = NULL;
			kobject_uevent_env(&k3fd->pdev->dev.kobj, KOBJ_CHANGE, envp);
		}
	}

	return 0;
}
#else
static ssize_t vsync_show_event(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct fb_info *fbi = NULL;
	struct k3_fb_data_type *k3fd = NULL;
	ktime_t prev_timestamp;

	BUG_ON(dev == NULL);
	fbi = dev_get_drvdata(dev);
	BUG_ON(fbi == NULL);
	k3fd = (struct k3_fb_data_type *)fbi->par;
	BUG_ON(k3fd == NULL);

	prev_timestamp = k3fd->vsync_ctrl.vsync_timestamp;
	ret = wait_event_interruptible_timeout(
		k3fd->vsync_ctrl.vsync_wait,
		vsync_timestamp_changed(k3fd, prev_timestamp) &&
		k3fd->vsync_ctrl.vsync_enabled,
		msecs_to_jiffies(VSYNC_TIMEOUT_MSEC));
	/*if (ret == 0) {
		K3_FB_ERR("wait vsync timeout!");
		return -ETIMEDOUT;
	}*/

	if (ret > 0) {
		ret = snprintf(buf, PAGE_SIZE, "VSYNC=%llu",
			ktime_to_ns(k3fd->vsync_ctrl.vsync_timestamp));
		buf[strlen(buf) + 1] = '\0';
	}

	return ret;
}

static DEVICE_ATTR(vsync_event, S_IRUGO, vsync_show_event, NULL);
static struct attribute *vsync_fs_attrs[] = {
	&dev_attr_vsync_event.attr,
	NULL,
};

static struct attribute_group vsync_fs_attr_group = {
	.attrs = vsync_fs_attrs,
};

static int k3fb_create_vsync_sysfs(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	ret = sysfs_create_group(&k3fd->fbi->dev->kobj, &vsync_fs_attr_group);
	if (ret) {
		K3_FB_ERR("fb%d vsync sysfs group creation failed, error=%d!\n",
			k3fd->index, ret);
	}

	return ret;
}

static void k3fb_remove_vsync_sysfs(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	sysfs_remove_group(&k3fd->fbi->dev->kobj, &vsync_fs_attr_group);
}
#endif

#ifdef CONFIG_FAKE_VSYNC_USED
enum hrtimer_restart k3fb_fake_vsync(struct hrtimer *timer)
{
	struct k3_fb_data_type *k3fd = NULL;
	int fps = 60;

	k3fd = container_of(timer, struct k3_fb_data_type, fake_vsync_hrtimer);
	BUG_ON(k3fd == NULL);

	if (!k3fd->panel_power_on)
		goto error;

	if (k3fd->fake_vsync_used && k3fd->vsync_ctrl.vsync_enabled) {
		k3fd->vsync_ctrl.vsync_timestamp = ktime_get();
		wake_up_interruptible_all(&k3fd->vsync_ctrl.vsync_wait);
	}

error:
	hrtimer_start(&k3fd->fake_vsync_hrtimer,
		ktime_set(0, NSEC_PER_SEC / fps), HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}
#endif

static void k3fb_vsync_ctrl_workqueue_handler(struct work_struct *work)
{
	struct k3_fb_data_type *k3fd = NULL;
	struct k3fb_vsync *vsync_ctrl = NULL;
	struct k3_fb_panel_data *pdata = NULL;
	unsigned long flags = 0;

	vsync_ctrl = container_of(work, typeof(*vsync_ctrl), vsync_ctrl_work);
	BUG_ON(vsync_ctrl == NULL);
	k3fd = vsync_ctrl->k3fd;
	BUG_ON(k3fd == NULL);
	pdata = dev_get_platdata(&k3fd->pdev->dev);
	BUG_ON(pdata == NULL);

	down(&(k3fd->blank_sem));

	if (!k3fd->panel_power_on) {
		K3_FB_ERR("fb%d, panel is power off!", k3fd->index);
		up(&(k3fd->blank_sem));
		return;
	}

	mutex_lock(&(vsync_ctrl->vsync_lock));
	if (vsync_ctrl->vsync_ctrl_disabled_set &&
		(vsync_ctrl->vsync_ctrl_expire_count == 0) &&
		vsync_ctrl->vsync_ctrl_enabled &&
		!vsync_ctrl->vsync_enabled) {
		K3_FB_DEBUG("fb%d, dss clk off!\n", k3fd->index);

		spin_lock_irqsave(&(vsync_ctrl->spin_lock), flags);
		if (pdata->vsync_ctrl) {
			pdata->vsync_ctrl(k3fd->pdev, 0);
		} else {
			K3_FB_ERR("fb%d, vsync_ctrl not supported!\n", k3fd->index);
		}
		vsync_ctrl->vsync_ctrl_enabled = 0;
		vsync_ctrl->vsync_ctrl_disabled_set = 0;
		spin_unlock_irqrestore(&(vsync_ctrl->spin_lock), flags);

		if (k3fd->panel_info.vsync_ctrl_type & VSYNC_CTRL_MIPI_ULPS) {
			mipi_dsi_ulps_cfg(k3fd, 0);
		}

		if (k3fd->panel_info.vsync_ctrl_type & VSYNC_CTRL_CLK_OFF) {
			dpe_clk_disable(k3fd);
			mipi_dsi_clk_disable(k3fd);
		}

		if (k3fd->panel_info.vsync_ctrl_type & VSYNC_CTRL_VCC_OFF) {
			dpe_regulator_disable(k3fd);
		}
	}
	mutex_unlock(&(vsync_ctrl->vsync_lock));

	if (vsync_ctrl->vsync_report_fnc) {
		vsync_ctrl->vsync_report_fnc(1);
	}

	up(&(k3fd->blank_sem));
}

void k3fb_vsync_register(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;
	struct k3fb_vsync *vsync_ctrl = NULL;
#if defined(CONFIG_K3_FB_VSYNC_THREAD)
	char name[64] = {0};
#endif

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);
	vsync_ctrl = &(k3fd->vsync_ctrl);
	BUG_ON(vsync_ctrl == NULL);

	if (vsync_ctrl->vsync_created)
		return;

	vsync_ctrl->k3fd = k3fd;
	vsync_ctrl->vsync_enabled = 0;
	vsync_ctrl->vsync_timestamp = ktime_get();
	init_waitqueue_head(&(vsync_ctrl->vsync_wait));
	spin_lock_init(&(vsync_ctrl->spin_lock));
	INIT_WORK(&vsync_ctrl->vsync_ctrl_work, k3fb_vsync_ctrl_workqueue_handler);

	mutex_init(&(vsync_ctrl->vsync_lock));

	atomic_set(&(vsync_ctrl->buffer_updated), 1);
#ifdef CONFIG_REPORT_VSYNC
	vsync_ctrl->vsync_report_fnc = mali_kbase_pm_report_vsync;
#else
	vsync_ctrl->vsync_report_fnc = NULL;
#endif

#ifdef CONFIG_FAKE_VSYNC_USED
	/* hrtimer for fake vsync timing */
	k3fd->fake_vsync_used = false;
	hrtimer_init(&k3fd->fake_vsync_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	k3fd->fake_vsync_hrtimer.function = k3fb_fake_vsync;
	hrtimer_start(&k3fd->fake_vsync_hrtimer,
		ktime_set(0, NSEC_PER_SEC / 60), HRTIMER_MODE_REL);
#endif

#if defined(CONFIG_K3_FB_VSYNC_THREAD)
	snprintf(name, sizeof(name), "k3fb%d_vsync", k3fd->index);
	vsync_ctrl->vsync_thread = kthread_run(wait_for_vsync_thread, k3fd, name);
	if (IS_ERR(vsync_ctrl->vsync_thread)) {
		vsync_ctrl->vsync_thread = NULL;
		K3_FB_ERR("failed to run vsync thread!\n");
		return;
	}
#else
	k3fb_create_vsync_sysfs(pdev);
#endif

	vsync_ctrl->vsync_created = 1;
}

void k3fb_vsync_unregister(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;
	struct k3fb_vsync *vsync_ctrl = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);
	vsync_ctrl = &(k3fd->vsync_ctrl);
	BUG_ON(vsync_ctrl == NULL);

	if (!vsync_ctrl->vsync_created)
		return;

#ifdef CONFIG_FAKE_VSYNC_USED
	k3fd->fake_vsync_used = false;
	hrtimer_cancel(&k3fd->fake_vsync_hrtimer);
#endif

#if defined(CONFIG_K3_FB_VSYNC_THREAD)
	if (vsync_ctrl->vsync_thread)
		kthread_stop(vsync_ctrl->vsync_thread);
#else
	k3fb_remove_vsync_sysfs(pdev);
#endif

	vsync_ctrl->vsync_created = 0;
}

void k3fb_activate_vsync(struct k3_fb_data_type *k3fd)
{
	struct k3_fb_panel_data *pdata = NULL;
	struct k3fb_vsync *vsync_ctrl = NULL;
	unsigned long flags = 0;
	int clk_enabled = 0;

	BUG_ON(k3fd == NULL);
	pdata = dev_get_platdata(&k3fd->pdev->dev);
	BUG_ON(pdata == NULL);
	vsync_ctrl = &(k3fd->vsync_ctrl);
	BUG_ON(vsync_ctrl == NULL);

	if (k3fd->panel_info.vsync_ctrl_type == VSYNC_CTRL_NONE)
		return;

	mutex_lock(&(vsync_ctrl->vsync_lock));

	if (vsync_ctrl->vsync_ctrl_enabled == 0) {
		K3_FB_DEBUG("fb%d, dss clk on!\n", k3fd->index);
		if (k3fd->panel_info.vsync_ctrl_type & VSYNC_CTRL_VCC_OFF) {
			dpe_regulator_enable(k3fd);
		}

		if (k3fd->panel_info.vsync_ctrl_type & VSYNC_CTRL_CLK_OFF) {
			mipi_dsi_clk_enable(k3fd);
			dpe_clk_enable(k3fd);
		}

		if (k3fd->panel_info.vsync_ctrl_type & VSYNC_CTRL_MIPI_ULPS) {
			mipi_dsi_ulps_cfg(k3fd, 1);
		}

		vsync_ctrl->vsync_ctrl_enabled = 1;
		clk_enabled = 1;
	}

	spin_lock_irqsave(&(vsync_ctrl->spin_lock), flags);
	vsync_ctrl->vsync_ctrl_disabled_set = 0;
	vsync_ctrl->vsync_ctrl_expire_count = 0;

	if (clk_enabled) {
		if (pdata->vsync_ctrl) {
			pdata->vsync_ctrl(k3fd->pdev, 1);
		} else {
			K3_FB_ERR("fb%d, vsync_ctrl not supported!\n", k3fd->index);
		}
	}
	spin_unlock_irqrestore(&(vsync_ctrl->spin_lock), flags);

	mutex_unlock(&(vsync_ctrl->vsync_lock));
}

void k3fb_deactivate_vsync(struct k3_fb_data_type *k3fd)
{
	struct k3_fb_panel_data *pdata = NULL;
	struct k3fb_vsync *vsync_ctrl = NULL;
	unsigned long flags = 0;

	BUG_ON(k3fd == NULL);
	pdata = dev_get_platdata(&k3fd->pdev->dev);
	BUG_ON(pdata == NULL);
	vsync_ctrl = &(k3fd->vsync_ctrl);
	BUG_ON(vsync_ctrl == NULL);

	if (k3fd->panel_info.vsync_ctrl_type == VSYNC_CTRL_NONE)
		return;

	mutex_lock(&(vsync_ctrl->vsync_lock));

	spin_lock_irqsave(&(vsync_ctrl->spin_lock), flags);
	vsync_ctrl->vsync_ctrl_disabled_set = 1;
	if (vsync_ctrl->vsync_ctrl_enabled)
		vsync_ctrl->vsync_ctrl_expire_count = VSYNC_CTRL_EXPIRE_COUNT;
	spin_unlock_irqrestore(&(vsync_ctrl->spin_lock), flags);

	mutex_unlock(&(vsync_ctrl->vsync_lock));
}

int k3fb_vsync_ctrl(struct fb_info *info, void __user *argp)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;
	struct k3_fb_panel_data *pdata = NULL;
	struct k3fb_vsync *vsync_ctrl = NULL;
	int enable = 0;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);
	pdata = dev_get_platdata(&k3fd->pdev->dev);
	BUG_ON(pdata == NULL);
	vsync_ctrl = &(k3fd->vsync_ctrl);
	BUG_ON(vsync_ctrl == NULL);

	ret = copy_from_user(&enable, argp, sizeof(enable));
	if (ret) {
		K3_FB_ERR("k3fb_vsync_ctrl ioctl failed!\n");
		return ret;
	}

	enable = (enable) ? 1 : 0;

	mutex_lock(&(vsync_ctrl->vsync_lock));

	if (vsync_ctrl->vsync_enabled == enable) {
		mutex_unlock(&(vsync_ctrl->vsync_lock));
		return 0;
	}

	if (g_debug_online_vsync)
		K3_FB_INFO("fb%d, enable=%d!\n", k3fd->index, enable);

	vsync_ctrl->vsync_enabled = enable;

	mutex_unlock(&(vsync_ctrl->vsync_lock));

	if (!k3fd->panel_power_on) {
		K3_FB_ERR("fb%d, panel is power off!", k3fd->index);
		return 0;
	}

	if (enable) {
		k3fb_activate_vsync(k3fd);
	} else {
		k3fb_deactivate_vsync(k3fd);
	}

	return 0;
}

int k3fb_vsync_resume(struct k3_fb_data_type *k3fd)
{
	struct k3fb_vsync *vsync_ctrl = NULL;

	BUG_ON(k3fd == NULL);
	vsync_ctrl = &(k3fd->vsync_ctrl);
	BUG_ON(vsync_ctrl == NULL);

	vsync_ctrl->vsync_enabled = 0;
	vsync_ctrl->vsync_ctrl_expire_count = 0;
	vsync_ctrl->vsync_ctrl_enabled = 0;
	vsync_ctrl->vsync_ctrl_disabled_set = 0;

	atomic_set(&(vsync_ctrl->buffer_updated), 1);

	if (k3fd->panel_info.vsync_ctrl_type != VSYNC_CTRL_NONE) {
		if ((k3fd->panel_info.vsync_ctrl_type & VSYNC_CTRL_MIPI_ULPS) ||
			(k3fd->panel_info.vsync_ctrl_type & VSYNC_CTRL_CLK_OFF) ||
			(k3fd->panel_info.vsync_ctrl_type & VSYNC_CTRL_VCC_OFF)) {

			if (k3fd->panel_info.vsync_ctrl_type & VSYNC_CTRL_MIPI_ULPS) {
				mipi_dsi_ulps_cfg(k3fd, 0);
			}

			if (k3fd->panel_info.vsync_ctrl_type & VSYNC_CTRL_CLK_OFF) {
				dpe_clk_disable(k3fd);
				mipi_dsi_clk_disable(k3fd);
			}

			if (k3fd->panel_info.vsync_ctrl_type & VSYNC_CTRL_VCC_OFF) {
				dpe_regulator_disable(k3fd);
			}
		}
	}

	return 0;
}

int k3fb_vsync_suspend(struct k3_fb_data_type *k3fd)
{
	struct k3fb_vsync *vsync_ctrl = NULL;

	BUG_ON(k3fd == NULL);
	vsync_ctrl = &(k3fd->vsync_ctrl);
	BUG_ON(vsync_ctrl == NULL);

	if (k3fd->panel_info.vsync_ctrl_type != VSYNC_CTRL_NONE) {
		if (k3fd->vsync_ctrl.vsync_ctrl_enabled == 0) {
			if (k3fd->panel_info.vsync_ctrl_type & VSYNC_CTRL_VCC_OFF) {
				dpe_regulator_enable(k3fd);
				K3_FB_INFO("fb%d, need to enable dpe regulator! vsync_ctrl_enabled=%d.\n",
					k3fd->index, k3fd->vsync_ctrl.vsync_ctrl_enabled);
			}

			if (k3fd->panel_info.vsync_ctrl_type & VSYNC_CTRL_CLK_OFF) {
				mipi_dsi_clk_enable(k3fd);
				dpe_clk_enable(k3fd);
				K3_FB_INFO("fb%d, need to enable dpe clk! vsync_ctrl_enabled=%d.\n",
					k3fd->index, k3fd->vsync_ctrl.vsync_ctrl_enabled);
			}

			if (k3fd->panel_info.vsync_ctrl_type & VSYNC_CTRL_MIPI_ULPS) {
				mipi_dsi_ulps_cfg(k3fd, 1);
				K3_FB_INFO("fb%d, need to enable mipi clk, exit ulps!vsync_ctrl_enabled=%d.\n",
					k3fd->index, k3fd->vsync_ctrl.vsync_ctrl_enabled);
			}
		}
	}

	return 0;
}
