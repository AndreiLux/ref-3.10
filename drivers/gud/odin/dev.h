/*
 * drivers/gud/odin/dev.h
 *
 * Copyright:	(C) 2014 LG Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ODIN_DRM_DEV_H_
#define _ODIN_DRM_DEV_H_

#include <linux/kfifo.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/platform_data/odin_drm.h>

#include "mobicore_driver_api.h"
#include "tlOdinDci.h"

#define ODIN_DRM_DEV_NAME "odin_drm"
#define ODIN_DRM_DEVMAX 1

struct lgdrm_private {
	/* Basic device driver member */
	struct cdev cdev;
	struct class *drmcls;
	dev_t devno;

	/* mobicore session handles */
	struct mc_session_handle session;

	/* Shared buffer (mobicore<->kernel module) */
	dciMessage_t *pdci;

	uint32_t dev_id;
	long timeout;

	/* thread to service interrupts */
	struct task_struct *drm_thd;
	wait_queue_head_t waitq;
	struct semaphore resp_sem;

	/* Send message FIFO */
	struct kfifo drm_fifo;
};

struct drm_event {
	dciCommandId_t command;
	struct mc_bulk_map map;
	int ret;
};

typedef struct drm_event* drm_evt_ptr;

#define MC_UUID_DRM_DEFINED \
	{ 0x07, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	  0x00, 0x00, 0x00, 0x00, 0x00, 0xAA, 0xA8, 0x82 }

static const struct mc_uuid_t MC_UUID_DRM_RESERVED = {
	MC_UUID_DRM_DEFINED
};

#endif
