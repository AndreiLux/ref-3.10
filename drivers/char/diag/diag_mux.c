/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/diagchar.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/ratelimit.h>
#include "diagchar.h"
#include "diag_mux.h"
#include "diag_usb.h"
#include "diag_memorydevice.h"

#ifdef CONFIG_LGE_DM_APP
#include "lg_dm_tty.h"
#include "diagfwd.h"
#include "diagmem.h"
#endif
#ifdef CONFIG_LGE_DIAG_BYPASS
#include "lg_diag_bypass.h"
#endif

struct diag_logger_t *logger;
static struct diag_logger_t usb_logger;
static struct diag_logger_t md_logger;

static struct diag_logger_ops usb_log_ops = {
	.open = diag_usb_connect_all,
	.close = diag_usb_disconnect_all,
	.queue_read = diag_usb_queue_read,
	.write = diag_usb_write,
};

static struct diag_logger_ops md_log_ops = {
	.open = diag_md_open_all,
	.close = diag_md_close_all,
	.queue_read = NULL,
	.write = diag_md_write,
};

int diag_mux_init()
{

#ifdef CONFIG_LGE_DM_APP
    int j = 0;
#endif

	logger = kzalloc(NUM_MUX_PROC * sizeof(struct diag_logger_t),
			 GFP_KERNEL);
	if (!logger)
		return -ENOMEM;
	kmemleak_not_leak(logger);

	usb_logger.mode = DIAG_USB_MODE;
	usb_logger.log_ops = &usb_log_ops;

	md_logger.mode = DIAG_MEMORY_DEVICE_MODE;
	md_logger.log_ops = &md_log_ops;
	diag_md_init();

#ifdef CONFIG_LGE_DM_APP
    lge_dm_tty = kzalloc(sizeof(struct dm_tty), GFP_KERNEL);
    if (lge_dm_tty == NULL)
    {
        printk(KERN_DEBUG "diag: diag_mux_init failed to allocate"
            "lge_dm_tty\n");
    }
    else
    {
        lge_dm_tty->num_tbl_entries = driver->poolsize;
        lge_dm_tty->tbl = kzalloc(lge_dm_tty->num_tbl_entries *
                  sizeof(struct diag_buf_tbl_t),
                  GFP_KERNEL);
        if (lge_dm_tty->tbl) {
            for (j = 0; j < lge_dm_tty->num_tbl_entries; j++) {
                lge_dm_tty->tbl[j].buf = NULL;
                lge_dm_tty->tbl[j].len = 0;
                lge_dm_tty->tbl[j].ctx = 0;
                spin_lock_init(&(lge_dm_tty->tbl[j].lock));
            }
        } else {
            kfree(lge_dm_tty->tbl);
            lge_dm_tty->num_tbl_entries = 0;
            lge_dm_tty->ops = NULL;
        }
    }
#endif

	/*
	 * Set USB logging as the default logger. This is the mode
	 * Diag should be in when it initializes.
	 */
	logger = &usb_logger;
	return 0;
}

void diag_mux_exit()
{
	kfree(logger);
}

int diag_mux_register(int proc, int ctx, struct diag_mux_ops *ops)
{
	int err = 0;
	if (!ops)
		return -EINVAL;

	if (proc < 0 || proc >= NUM_MUX_PROC)
		return 0;

	/* Register with USB logger */
	usb_logger.ops[proc] = ops;
	err = diag_usb_register(proc, ctx, ops);
	if (err) {
		pr_err("diag: MUX: unable to register usb operations for proc: %d, err: %d\n",
		       proc, err);
		return err;
	}

	md_logger.ops[proc] = ops;
	err = diag_md_register(proc, ctx, ops);
	if (err) {
		pr_err("diag: MUX: unable to register md operations for proc: %d, err: %d\n",
		       proc, err);
		return err;
	}

#ifdef CONFIG_LGE_DM_APP
#ifndef CONFIG_DIAGFWD_BRIDGE_CODE
    lge_dm_tty->id = DIAG_MUX_LOCAL;
#else
    lge_dm_tty->id = DIAG_MUX_MDM;
#endif
    lge_dm_tty->ctx = ctx;
    lge_dm_tty->ops = ops;
#endif

	return 0;
}

int diag_mux_queue_read(int proc)
{
	if (proc < 0 || proc >= NUM_MUX_PROC)
		return -EINVAL;
	if (!logger)
		return -EIO;
	if (logger->log_ops && logger->log_ops->queue_read)
		return logger->log_ops->queue_read(proc);
	return 0;
}

int diag_mux_write(int proc, unsigned char *buf, int len, int ctx)
{

#ifdef CONFIG_LGE_DM_APP
    int i;
    uint8_t found = 0;
    unsigned long flags;
    struct diag_usb_info *usb_info = NULL;
#endif

	if (proc < 0 || proc >= NUM_MUX_PROC)
		return -EINVAL;

#ifdef CONFIG_LGE_DM_APP
    if (driver->logging_mode == DM_APP_MODE) {
        /* only diag cmd #250 for supporting testmode tool */
        if ((GET_BUF_PERIPHERAL(ctx) == APPS_DATA) && (*((char *)buf) == 0xFA)) {
            usb_info = &diag_usb[lge_dm_tty->id];
            lge_dm_tty->dm_usb_req = diagmem_alloc(driver, sizeof(struct diag_request),
                usb_info->mempool);
            if (lge_dm_tty->dm_usb_req) {
                lge_dm_tty->dm_usb_req->buf = buf;
                lge_dm_tty->dm_usb_req->length = len;
                lge_dm_tty->dm_usb_req->context = (void *)(uintptr_t)ctx;

                queue_work(lge_dm_tty->dm_wq,
                    &(lge_dm_tty->dm_usb_work));
                flush_work(&(lge_dm_tty->dm_usb_work));

            }

            return 0;
        }

        for (i = 0; i < lge_dm_tty->num_tbl_entries && !found; i++) {
            spin_lock_irqsave(&lge_dm_tty->tbl[i].lock, flags);
            if (lge_dm_tty->tbl[i].len == 0) {
                lge_dm_tty->tbl[i].buf = buf;
                lge_dm_tty->tbl[i].len = len;
                lge_dm_tty->tbl[i].ctx = ctx;
                found = 1;
                diag_ws_on_read(DIAG_WS_MD, len);
            }
            spin_unlock_irqrestore(&lge_dm_tty->tbl[i].lock, flags);
        }

        lge_dm_tty->set_logging = 1;
        wake_up_interruptible(&lge_dm_tty->waitq);

        return 0;

    }
#endif

#ifdef CONFIG_LGE_DIAG_BYPASS
    if(diag_bypass_response(buf, len, proc, ctx, logger) > 0) {
        return 0;
    }
#endif

	if (logger && logger->log_ops && logger->log_ops->write)
		return logger->log_ops->write(proc, buf, len, ctx);
	return 0;
}

int diag_mux_switch_logging(int new_mode)
{
	struct diag_logger_t *new_logger = NULL;

	switch (new_mode) {
	case DIAG_USB_MODE:
		new_logger = &usb_logger;
		break;
	case DIAG_MEMORY_DEVICE_MODE:
		new_logger = &md_logger;
		break;
	default:
		pr_err("diag: Invalid mode %d in %s\n", new_mode, __func__);
		return -EINVAL;
	}

	if (logger) {
		logger->log_ops->close();
		logger = new_logger;
		logger->log_ops->open();
	}

	return 0;
}

