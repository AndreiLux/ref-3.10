/* Copyright (c) 2008-2014, Hisilicon Tech. Co., Ltd. All rights reserved.
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


#ifdef CONFIG_BUF_SYNC_USED
#define BUF_SYNC_TIMEOUT_MSEC	(10 * MSEC_PER_SEC)
#define BUF_SYNC_FENCE_NAME	"k3-dss-fence"
#define BUF_SYNC_TIMELINE_NAME	"k3-dss-timeline"

int k3fb_buf_sync_wait(int fence_fd)
{
	int ret = 0;
	struct sync_fence *fence = NULL;

	fence = sync_fence_fdget(fence_fd);
	if (fence == NULL){
		K3_FB_ERR("sync_fence_fdget failed!\n");
		return -EINVAL;
	}

	ret = sync_fence_wait(fence, BUF_SYNC_TIMEOUT_MSEC);
	if (ret == -ETIME)
		ret = sync_fence_wait(fence, BUF_SYNC_TIMEOUT_MSEC);

	if (ret < 0)
		K3_FB_WARNING("error waiting on fence: 0x%x\n", ret);

	sync_fence_put(fence);

	return ret;
}


void k3fb_buf_sync_signal(struct k3_fb_data_type *k3fd)
{
	//K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

	spin_lock(&k3fd->buf_sync_ctrl.refresh_lock);
	if (k3fd->buf_sync_ctrl.refresh) {
		sw_sync_timeline_inc(k3fd->buf_sync_ctrl.timeline, k3fd->buf_sync_ctrl.refresh);
		k3fd->buf_sync_ctrl.refresh = 0;
	}
	spin_unlock(&k3fd->buf_sync_ctrl.refresh_lock);

	//K3_FB_DEBUG("fb%d, -.\n", k3fd->index);
}

void k3fb_buf_sync_suspend(struct k3_fb_data_type *k3fd)
{
	unsigned long flags;

	spin_lock_irqsave(&k3fd->buf_sync_ctrl.refresh_lock,flags);
	sw_sync_timeline_inc(k3fd->buf_sync_ctrl.timeline, k3fd->buf_sync_ctrl.refresh + 1);
	k3fd->buf_sync_ctrl.refresh = 0;
	k3fd->buf_sync_ctrl.timeline_max++;
	spin_unlock_irqrestore(&k3fd->buf_sync_ctrl.refresh_lock,flags);
}

int k3fb_buf_sync_create_fence(struct k3_fb_data_type *k3fd, unsigned value)
{
	int fd = -1;
	struct sync_fence *fence = NULL;
	struct sync_pt *pt = NULL;

	BUG_ON(k3fd == NULL);

	fd = get_unused_fd();
	if (fd < 0) {
		K3_FB_ERR("get_unused_fd failed!\n");
		return fd;
	}

	pt = sw_sync_pt_create(k3fd->buf_sync_ctrl.timeline, value);
	if (pt == NULL) {
		return -ENOMEM;
	}

	fence = sync_fence_create(BUF_SYNC_FENCE_NAME, pt);
	if (fence == NULL) {
		sync_pt_free(pt);
		return -ENOMEM;
	}

	sync_fence_install(fence, fd);

	return fd;
}

void k3fb_buf_sync_register(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

	spin_lock_init(&k3fd->buf_sync_ctrl.refresh_lock);
	k3fd->buf_sync_ctrl.refresh = 0;
	k3fd->buf_sync_ctrl.timeline_max = 1;
	k3fd->buf_sync_ctrl.timeline =
		sw_sync_timeline_create(BUF_SYNC_TIMELINE_NAME);
	if (k3fd->buf_sync_ctrl.timeline == NULL) {
		K3_FB_ERR("cannot create time line!");
		return; /* -ENOMEM */
	}

	K3_FB_DEBUG("fb%d, -.\n", k3fd->index);
}

void k3fb_buf_sync_unregister(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	K3_FB_DEBUG("fb%d, +.\n", k3fd->index);

	if (k3fd->buf_sync_ctrl.timeline) {
		sync_timeline_destroy((struct sync_timeline *)k3fd->buf_sync_ctrl.timeline);
		k3fd->buf_sync_ctrl.timeline = NULL;
	}

	K3_FB_DEBUG("fb%d, -.\n", k3fd->index);
}
#else
int k3fb_buf_sync_wait(int fence_fd)
{ return 0; }
void k3fb_buf_sync_signal(struct k3_fb_data_type *k3fd)
{}
void k3fb_buf_sync_suspend(struct k3_fb_data_type *k3fd)
{}
int k3fb_buf_sync_create_fence(struct k3_fb_data_type *k3fd, unsigned value)
{ return 0; }
void k3fb_buf_sync_register(struct platform_device *pdev)
{}
void k3fb_buf_sync_unregister(struct platform_device *pdev)
{}
#endif
