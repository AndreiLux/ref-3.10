/*
 * drivers/gud/odin/odin_drm.c
 *
 * Copyright:	(C) 2014 LG Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/string.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/vmalloc.h>
#include <linux/platform_data/odin_tz.h>
#include <linux/smp.h>
#include <asm/cputype.h>

#include <asm/uaccess.h>

#include "dev.h"
#include "drm_debug.h"

#define CA15_ROTATION

static struct lgdrm_private *drm_priv = NULL;
static int lgdrm_started = 0;

/* Utility function to initialize Driver */
static uint8_t drm_open_session(struct lgdrm_private *priv, uint32_t size)
{
	enum mc_result ret = MC_DRV_OK;

	do {
		/* Initialize session handle data */
		memset(&priv->session, 0, sizeof(struct mc_session_handle));

		/* Allocating WSM for DCI */
		ret = mc_malloc_wsm(priv->dev_id, 0, size,
				    (uint8_t**)&priv->pdci, 0);
		if (ret != MC_DRV_OK) {
			TZM_DBG_ERROR("mc_malloc_wsm returned: %d", ret);
			break;
		}

		/* Open session the trustlet */
		priv->session.device_id = drm_priv->dev_id;
		ret = mc_open_session(&priv->session,
				      &MC_UUID_DRM_RESERVED,
				      (uint8_t *)priv->pdci,
				      size);
		if (ret != MC_DRV_OK) {
			TZM_DBG_ERROR("mc_open_session returned: %d", ret);
			break;
		}
		TZM_DBG("mc_open_session returned: %d", ret);
	} while (0);

	return ret;
}

static uint8_t drm_notify_wait(struct mc_session_handle *session)
{
	enum mc_result ret = MC_DRV_OK;

	do {
		/* Notify the trusted application / driver */
		ret = mc_notify(session);
		if (ret != MC_DRV_OK) {
			TZM_DBG_ERROR("mc_notify returned: %d", ret);
			break;
		}
		TZM_DBG("mcNotify returned: %d", ret);

		/* Wait for response from the trusted application */
		ret = mc_wait_notification(session, MC_INFINITE_TIMEOUT);
		if (ret != MC_DRV_OK) {
			TZM_DBG_ERROR("mc_wait_notification returned: %d", ret);
			break;
		}

	} while (0);

	return ret;
}

static int drm_queue_event(struct lgdrm_private *priv, drm_evt_ptr event)
{
	int ret = 0;
	uint32_t buf_addr;
	buf_addr = (uint32_t)event;

	kfifo_in(&priv->drm_fifo, (uint8_t*)&buf_addr, sizeof(uint32_t));

	wake_up(&priv->waitq);

	if (down_timeout(&priv->resp_sem, priv->timeout)) {
		TZM_DBG_ERROR("drm event timeout");
		ret = -2;
	}
	if (event->ret != MC_DRV_OK)
		ret = -1;

	TZM_DBG("cmd: %d return: %d", event->command, event->ret);

	return ret;
}

static int drm_init_session(struct lgdrm_private *priv)
{
	enum mc_result ret;

	/* Open MobiCore device */
	ret = mc_open_device(priv->dev_id);
	if (ret != MC_DRV_OK) {
		TZM_DBG_ERROR("mc_open_device returned: %d", ret);
		return -1;
	}

	/* Allocate Shared Buffer and open Driver */
	ret = drm_open_session(priv, sizeof(void*));
	if (ret != MC_DRV_OK) {
		TZM_DBG_ERROR("Failed to init driver: %d", ret);
		return -1;
	}
	lgdrm_started = 1;
	return 0;
}

static int drm_event_process(struct lgdrm_private *priv, drm_evt_ptr evt)
{
	int ret;

	priv->pdci->cmdmap.header.commandId = evt->command;

	priv->pdci->cmdmap.pdata = (uint32_t)evt->map.secure_virt_addr;
	priv->pdci->cmdmap.data_len = evt->map.secure_virt_len;
	TZM_DBG("0x%p / %d", evt->map.secure_virt_addr, evt->map.secure_virt_len);
	ret = drm_notify_wait(&priv->session);
	evt->ret = priv->pdci->response.header.returnCode;

	return ret;
}

/*
 * This is our main function which will be called by the init function once the
 * module is loaded and our tplay thread is started.
 * For the example here we do everything, open, close, and call a function in the
 * driver. However you can implement this to wait for event from the driver, or
 * to do whatever is required.
 */
static int drm_thread(void *data) {
	enum mc_result ret;
	struct lgdrm_private *priv = data;
	wait_queue_t wait;

	TZM_DBG("DRM: main started");

	init_waitqueue_entry(&wait, current);

	while (1) {
		/* Kthread request and response loop */
		int kstate;

		add_wait_queue(&priv->waitq, &wait);
		set_current_state(TASK_INTERRUPTIBLE);

		if (kthread_should_stop())
			kstate = 0;
		else if (kfifo_len(&priv->drm_fifo))
			kstate = 0;	/* request arrived fifo */
		else
			kstate = 1;	/* No command */

		if (kstate) {
			schedule();
		}
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&priv->waitq, &wait);

		if (kthread_should_stop()) {
			TZM_DBG("tbase msg thread exited.");
			break;
		}

		while (kfifo_len(&priv->drm_fifo)) {
			drm_evt_ptr evt;
			uint32_t evt_addr;

			if (kfifo_out(&priv->drm_fifo,
				(uint8_t*)&evt_addr,
				sizeof(uint32_t)) != sizeof(uint32_t))
				break;

			evt = (drm_evt_ptr)evt_addr;
			drm_event_process(priv, evt);
			up(&priv->resp_sem);
			TZM_DBG("TZ returned semaphore up");
		}
	}
	mc_free_wsm(priv->dev_id, (uint8_t*)priv->pdci);

	/* Close driver session*/
	ret = mc_close_session(&priv->session);
	if (ret != MC_DRV_OK) {
		TZM_DBG_ERROR("mc_close_session returned: %d", ret);
		return -1;
	}

	/* Close MobiCore device */
	ret = mc_close_device(priv->dev_id);
	if (ret != MC_DRV_OK) {
		TZM_DBG_ERROR("mc_close_device returned: %d", ret);
	}

	return 0;
}

static int lgdrm_alloc_buf(struct lgdrm_private *priv, struct odin_drm_msg *msg)
{
	int ret;
	struct drm_event event;

	event.command = FID_DR_ALLOC_SEC_BUF;
	ret = mc_map(&priv->session, msg, sizeof(struct odin_drm_msg), &event.map);
	ret = drm_queue_event(priv, &event);
	mc_unmap(&priv->session, msg, &event.map);

	TZM_DBG("buffer: %x size: %d type: %d", msg->buffer, msg->len, msg->type);
	return ret;
}

static int lgdrm_free_buf(struct lgdrm_private *priv, struct odin_drm_msg *msg)
{
	int ret;
	struct drm_event event;

	event.command = FID_DR_FREE_SEC_BUF;
	ret = mc_map(&priv->session, msg, sizeof(struct odin_drm_msg), &event.map);
	ret = drm_queue_event(priv, &event);
	mc_unmap(&priv->session, msg, &event.map);

	return ret;
}

/* Testing function */
static int lgdrm_map_buf(void)
{
	struct odin_drm_ion *drmion;
	drmion = tzmem_init_list();
	tzmem_add_entry(drmion, 0xDE700000, 0x200000, DRM_CPB);
	tzmem_add_entry(drmion, 0xDE900000, 0x200000, DRM_CPB);
	tzmem_add_entry(drmion, 0xDEB00000, 0x200000, DRM_CPB);
	tzmem_add_entry(drmion, 0xDED00000, 0x200000, DRM_CPB);
	tzmem_map_buffer(drmion);
	tzmem_unmap_buffer(drmion);
	tzmem_delete_list(drmion);
	return 0;
}

static int lgdrm_rotate(struct neon_rotator_secure_arg_t *arg)
{
	struct fc_neon_rot data;

#ifdef CA15_ROTATION
	if (read_cpuid_part_number() == ARM_CPU_PART_CORTEX_A7) {
#else
	if (read_cpuid_part_number() == ARM_CPU_PART_CORTEX_A15) {
#endif
		TZM_DBG_ERROR("Invalid cpu affinity assigned");
		return -EFAULT;
	}

	data.src_first_offset = arg->src[0].offset;
	data.src_second_offset = arg->src[1].offset;

	data.dst_first_offset = arg->dst[0].offset;
	data.dst_second_offset = arg->dst[1].offset;

	/* Copy the argument for NEON rotator */
	data.width = arg->width;
	data.height = arg->height;
	data.whole_height = arg->whole_height;

	TZM_DBG("src=%x+%x dst=%x+%x width=%d height=%d",
		data.src_first_base, data.src_first_offset,
		data.dst_first_base, data.dst_first_offset,
		data.width, data.height);

	return tz_rotator(&data, arg->rot_degree);
}

static void ca7_rotator_handler(void *info)
{
	struct fc_neon_rot *cd = (struct fc_neon_rot *)info;

	rmb();
	cd->msg = tz_rotator(cd, cd->msg);
	wmb();
}

static int lgdrm_rot_map(struct neon_rotator_secure_arg_t *arg)
{
	int ret = 0;
	struct fc_neon_rot data;

	data.src_first_base = arg->src[0].pa;
	data.src_second_base = arg->src[1].pa;
	data.dst_first_base = arg->dst[0].pa;
	data.dst_second_base = arg->dst[1].pa;
	data.msg = ODIN_FC_NEON_MAP;

#ifdef CA15_ROTATION
	if ((read_cpuid_part_number() == ARM_CPU_PART_CORTEX_A7)
			|| (read_cpuid_mpidr() & 0xF) > 1) {
#else
	/* CA15 Cluster must migrate to CA7 */
	if ((read_cpuid_part_number() == ARM_CPU_PART_CORTEX_A15)
			|| !(read_cpuid_mpidr() & 0xF)) {
#endif
		TZM_DBG("[SecRot] ioctl came from %x\n", read_cpuid_mpidr());
		wmb();
#ifdef CA15_ROTATION
		smp_call_function_single(4, ca7_rotator_handler,
				(void*)&data, 1);
#else
		smp_call_function_single(1, ca7_rotator_handler,
				(void*)&data, 1);
#endif
		rmb();
		ret = data.msg;
	} else {
		ret = tz_rotator(&data, ODIN_FC_NEON_MAP);
	}
	return ret;
}

static int lgdrm_rot_unmap(struct neon_rotator_secure_arg_t *arg)
{
	int ret = 0;
	struct fc_neon_rot data;

	data.src_first_base = arg->src[0].pa;
	data.src_second_base = arg->src[1].pa;
	data.dst_first_base = arg->dst[0].pa;
	data.dst_second_base = arg->dst[1].pa;
	data.msg = ODIN_FC_NEON_UNMAP;

#ifdef CA15_ROTATION
	if ((read_cpuid_part_number() == ARM_CPU_PART_CORTEX_A7)
			|| (read_cpuid_mpidr() & 0xF) > 1) {
#else
	/* CA15 Cluster must migrate to CA7 */
	if ((read_cpuid_part_number() == ARM_CPU_PART_CORTEX_A15)
			|| !(read_cpuid_mpidr() & 0xF)) {
#endif
		TZM_DBG("[SecRot] ioctl came from %x\n", read_cpuid_mpidr());
		wmb();
#ifdef CA15_ROTATION
		smp_call_function_single(4, ca7_rotator_handler,
				(void*)&data, 1);
#else
		smp_call_function_single(1, ca7_rotator_handler,
				(void*)&data, 1);
#endif
		rmb();
		ret = data.msg;
	} else {
		ret = tz_rotator(&data, ODIN_FC_NEON_UNMAP);
	}
	return ret;
}

struct odin_drm_ion* tzmem_init_list(void)
{
	struct odin_drm_ion* ion = NULL;
	ion = kmalloc(sizeof(struct odin_drm_ion), GFP_KERNEL);
	if (!ion) {
		TZM_DBG_ERROR("Not enough memory");
		return NULL;
	}

	INIT_LIST_HEAD(&ion->tzlist.list);
	ion->ion_cnt = 0;
	if (!lgdrm_started) {
		drm_init_session(drm_priv);
	}

	return ion;
}
EXPORT_SYMBOL(tzmem_init_list);

void tzmem_add_entry(struct odin_drm_ion *ion,
		     phys_addr_t paddr,
		     unsigned int length,
		     unsigned int type)
{
	struct tz_ion_list *entry;
	entry = kmalloc(sizeof(struct tz_ion_list), GFP_KERNEL);
	if (!entry) {
		TZM_DBG_ERROR("Not enough memory");
		return;
	}
	if (paddr > 0xFFFFFFFF) {
		TZM_DBG_ERROR("Sorry, it can't support 64bit addressing");
		return;
	}
	entry->tzion.paddr = (uint32_t)paddr;
	entry->tzion.length = length;
	entry->tzion.type = type;

	list_add_tail(&entry->list, &ion->tzlist.list);
	ion->ion_cnt++;
}
EXPORT_SYMBOL(tzmem_add_entry);

void tzmem_delete_list(struct odin_drm_ion *ion)
{
	struct tz_ion_list *entry, *n;
	list_for_each_entry_safe(entry, n, &ion->tzlist.list, list) {
		if (entry) {
			list_del(&entry->list);
			kfree(entry);
		}
		ion->ion_cnt--;
	}
	if (ion->ion_cnt > 0) {
		TZM_DBG_ERROR("Not mathced counter and exissted list");
	}

	kfree(ion);
	ion = NULL;
}
EXPORT_SYMBOL(tzmem_delete_list);

int tzmem_map_buffer(struct odin_drm_ion *pion)
{
	int ret = 0;
	int i;
	int num = pion->ion_cnt;
	int len = num * sizeof(struct odin_tz_ion);
	struct drm_event event;
	struct odin_tz_ion *tzions = NULL;
	struct tz_ion_list *entry;

	tzions = vmalloc(len);
	if (!tzions) {
		TZM_DBG_ERROR("Not enough memory");
		return -ENOMEM;
	}
	i = 0;
	list_for_each_entry(entry, &pion->tzlist.list, list) {
		if (i > num) {
			TZM_DBG_ERROR("list entry overflow");
			break;
		}
		memcpy(&tzions[i], &entry->tzion, sizeof(struct odin_tz_ion));
		i++;
		TZM_DBG("pa=0x%x length=%x type=%d",
				entry->tzion.paddr,
				entry->tzion.length,
				entry->tzion.type);
	}
	if (i != num) {
		TZM_DBG_ERROR("list entry and number not matched");
		ret = -EFAULT;
		goto exit;
	}

	/* Info message for ION protection */
	TZM_DBG("vmalloc: %p, num: %d", tzions, num);

	event.command = FID_DR_MAP_ION_BUF;
	ret = mc_map(&drm_priv->session, tzions, len, &event.map);
	ret = drm_queue_event(drm_priv, &event);
	mc_unmap(&drm_priv->session, tzions, &event.map);
exit:
	vfree(tzions);
	tzions = NULL;

	return ret;
}
EXPORT_SYMBOL(tzmem_map_buffer);

int tzmem_unmap_buffer(struct odin_drm_ion *pion)
{
	int ret = 0;
	int i;
	int num = pion->ion_cnt;
	int len = num * sizeof(struct odin_tz_ion);
	struct drm_event event;
	struct odin_tz_ion *tzions = NULL;
	struct tz_ion_list *entry;

	tzions = vmalloc(len);
	if (!tzions) {
		TZM_DBG_ERROR("Not enough memory");
		return -ENOMEM;
	}
	i = 0;
	list_for_each_entry(entry, &pion->tzlist.list, list) {
		if (i > num) {
			TZM_DBG_ERROR("list entry overflow");
			break;
		}
		memcpy(&tzions[i], &entry->tzion, sizeof(struct odin_tz_ion));
		i++;
		TZM_DBG("pa=0x%x length=%x type=%d",
				entry->tzion.paddr,
				entry->tzion.length,
				entry->tzion.type);
	}
	if (i != num) {
		TZM_DBG_ERROR("list entry and number not matched");
		ret = -EFAULT;
		goto exit;
	}

	TZM_DBG("Free protected buffer");
	event.command = FID_DR_UNMAP_ION_BUF;
	ret = mc_map(&drm_priv->session, tzions, len, &event.map);
	ret = drm_queue_event(drm_priv, &event);
	mc_unmap(&drm_priv->session, tzions, &event.map);
exit:
	vfree(tzions);
	tzions = NULL;

	return 0;
}
EXPORT_SYMBOL(tzmem_unmap_buffer);

/* This is where we handle the IOCTL commands coming from user space */

static long odin_drm_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	int ret = -EINVAL;
	void __user *uarg = (void __user *)arg;
	struct lgdrm_private *priv = (struct lgdrm_private *)f->private_data;
	struct odin_drm_msg msg;
	struct neon_rotator_secure_arg_t neon;
	int elapsed;

	switch (cmd) {
	case DRM_BUFFER_ALLOC:{
		struct odin_drm_msg msg;
		if (copy_from_user(&msg, uarg, sizeof(msg)))
			return -EFAULT;
		ret = lgdrm_alloc_buf(priv, (struct odin_drm_msg*)uarg);
		if (!ret) {
			if (copy_to_user(uarg, &msg, sizeof(msg))) {
				ret = -EFAULT;
			}
		}
		}
		break;

	case DRM_BUFFER_FREE:
		if (copy_from_user(&msg, uarg, sizeof(msg)))
			return -EFAULT;
		ret = lgdrm_free_buf(priv, (struct odin_drm_msg*)uarg);
		break;

	case DRM_BUFFER_MAP:
		/* Test Purpose */
		ret = lgdrm_map_buf();
		break;

	case DRM_SECURE_ROTATE:
		TZM_DBG("Secure NEON rotator start");
		if (copy_from_user(&neon, uarg,
				sizeof(struct neon_rotator_secure_arg_t))) {
			TZM_DBG_ERROR("copy failed");
			ret = -EFAULT;
			break;
		}
		ret = lgdrm_rotate(&neon);
		TZM_DBG("Secure NEON rotator end");
		break;

	case DRM_ROTATOR_MAP:
		TZM_DBG("Secure NEON rotator map");
		if (copy_from_user(&neon, uarg,
				sizeof(struct neon_rotator_secure_arg_t))) {
			TZM_DBG_ERROR("copy failed");
			ret = -EFAULT;
			break;
		}
		ret = lgdrm_rot_map(&neon);
		break;

	case DRM_ROTATOR_UNMAP:
		TZM_DBG("Secure NEON rotator unmap");
		if (copy_from_user(&neon, uarg,
				sizeof(struct neon_rotator_secure_arg_t))) {
			TZM_DBG_ERROR("copy failed");
			ret = -EFAULT;
			break;
		}
		ret = lgdrm_rot_unmap(&neon);
		break;

	default:
		return -ENOTTY;
	}

	return ret;
}

static int odin_drm_open(struct inode *inode, struct file *file)
{
	TZM_DBG("DRM driver open");
	file->private_data = drm_priv;
	if (!lgdrm_started) {
		drm_init_session(drm_priv);
	}

	return 0;
}

static int odin_drm_release(struct inode *inode, struct file *file)
{
	TZM_DBG("DRM driver release");
	file->private_data = NULL;
	return 0;
}

static struct file_operations odin_drm_fops = {
	.owner		= THIS_MODULE,
	.open		= odin_drm_open,
	.release	= odin_drm_release,
	.unlocked_ioctl	= odin_drm_ioctl,
};

static int __init odin_drm_init(void)
{
	int err;

	drm_priv = kzalloc(sizeof(struct lgdrm_private), GFP_KERNEL);
	if (!drm_priv) {
		TZM_DBG_ERROR("odin_drm init failed");
		err = -ENOMEM;
		goto done;
	}

	err = alloc_chrdev_region(&drm_priv->devno, 0, 1, ODIN_DRM_DEV_NAME);
	if (err) {
		TZM_DBG_ERROR("Unable to allocate Odin DRM device number");
		goto err_devno;
	}

	cdev_init(&drm_priv->cdev, &odin_drm_fops);
	drm_priv->cdev.owner = THIS_MODULE;

	err = cdev_add(&drm_priv->cdev, drm_priv->devno, 1);
	if (err) {
		TZM_DBG_ERROR("Unable to add Odin DRM char device");
		goto err_cdev;
	}

	drm_priv->drmcls = class_create(THIS_MODULE, "odin_drm_cls");
	device_create(drm_priv->drmcls, NULL,
		      drm_priv->devno, NULL, ODIN_DRM_DEV_NAME);

	err = kfifo_alloc(&drm_priv->drm_fifo,
			  sizeof(uint32_t) * 8,
			  GFP_KERNEL);
	if (err) {
		TZM_DBG_ERROR("Out of memory allocating event FIFO buffer");
		goto err_class;
	}

	/* Fill the constant value for mobicore */
	drm_priv->dev_id = MC_DEVICE_ID_DEFAULT;
	drm_priv->timeout = (long)msecs_to_jiffies(30000);
	lgdrm_started = 0;

	init_waitqueue_head(&drm_priv->waitq);
	sema_init(&drm_priv->resp_sem, 0);

	/* Create the Odin DRM Main thread */
	drm_priv->drm_thd = kthread_run(drm_thread, drm_priv, "dci_thread");
	if (!drm_priv->drm_thd) {
		TZM_DBG_ERROR("Unable to start Odin DRM main thread");
		err = -EFAULT;
		goto err_class;
	}

	goto done;

err_class:
	device_destroy(drm_priv->drmcls, drm_priv->devno);
	class_destroy(drm_priv->drmcls);
err_cdev:
	unregister_chrdev_region(drm_priv->devno, 1);
err_devno:
	if (drm_priv) {
		kfree(drm_priv);
		drm_priv = NULL;
	}
done:
	return err;
}

static void __exit odin_drm_exit(void)
{
	kthread_stop(drm_priv->drm_thd);
	kfifo_free(&drm_priv->drm_fifo);

	lgdrm_started = 0;

	device_destroy(drm_priv->drmcls, drm_priv->devno);
	class_destroy(drm_priv->drmcls);
	unregister_chrdev_region(drm_priv->devno, ODIN_DRM_DEVMAX);

	if (drm_priv) {
		kfree(drm_priv);
		drm_priv = NULL;
	}
	TZM_DBG("Exit ODIN drm driver.");
}

late_initcall(odin_drm_init);
module_exit(odin_drm_exit);

MODULE_AUTHOR("LGE SIC");
MODULE_LICENSE("Dual BSD/GPL");
