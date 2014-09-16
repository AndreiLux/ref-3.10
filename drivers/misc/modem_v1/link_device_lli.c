/**
@file		link_device_lli.c
@brief		functions for a pseudo shared-memory based on a chip-to-chip
		(C2C) interface
@date		2014/02/05
@author		Hankook Jang (hankook.jang@samsung.com)
*/

/*
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/suspend.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio.h>
#include <mach/regs-gpio.h>
#include <linux/module.h>
#include <linux/debugfs.h>

#include <linux/mipi-lli.h>
#include "modem_prj.h"
#include "modem_utils.h"
#include "link_device_memory.h"

static int sleep_timeout = 100;
module_param(sleep_timeout, int, S_IRUGO);
MODULE_PARM_DESC(sleep_timeout, "LLI sleep timeout");

static int pm_enable = 1;
module_param(pm_enable, int, S_IRUGO);
MODULE_PARM_DESC(pm_enable, "LLI PM enable");

#ifdef DEBUG_CP_IRQSTRM
static int max_intr = 600;
module_param(max_intr, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(max_intr, "LLI maximum intr");

static int intr_cnt;
static unsigned long last_time, expired;

static inline void lli_mark_last_busy(void)
{
	intr_cnt = 0;
	ACCESS_ONCE(last_time) = jiffies;
	expired = ACCESS_ONCE(last_time) + msecs_to_jiffies(1000);
}

static inline bool lli_check_max_intr(void)
{
	if (++intr_cnt >= max_intr) {
		if (time_before(jiffies, expired)) {
			mif_info("cp_crash due to irq cnt %d\n", intr_cnt);
			intr_cnt = 0;
			modemctl_notify_event(MDM_EVENT_CP_FORCE_CRASH);
			return true;
		}
		lli_mark_last_busy();
	}
	return false;
}
#else
static inline void lli_mark_last_busy(void) {}
static inline bool lli_check_max_intr(void) { return false; }
#endif

static void send_ap2cp_irq(struct mem_link_device *mld, u16 mask)
{
#if 0/*def DEBUG_MODEM_IF*/
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;

	if (!cp_online(mc))
		mif_err("%s: mask 0x%04X (%s.state == %s)\n", ld->name, mask,
			mc->name, mc_state(mc));
#endif

	mipi_lli_send_interrupt(mask);
}

#ifdef CONFIG_LINK_POWER_MANAGEMENT
#ifdef CONFIG_LINK_POWER_MANAGEMENT_WITH_FSM

/**
@brief		forbid CP from going to sleep

Wakes up a CP if it can sleep and increases the "ref_cnt" counter in the
mem_link_device instance.

@param mld	the pointer to a mem_link_device instance

@remark		CAUTION!!! permit_cp_sleep() MUST be invoked after
		forbid_cp_sleep() success to decrease the "ref_cnt" counter.
*/
static void forbid_cp_sleep(struct mem_link_device *mld)
{
	struct modem_link_pm *pm = &mld->link_dev.pm;
	int ref_cnt;

	ref_cnt = atomic_inc_return(&mld->ref_cnt);
	mif_debug("ref_cnt %d\n", ref_cnt);

	if (ref_cnt > 1)
		return;

	if (pm->request_hold)
		pm->request_hold(pm);
}

/**
@brief	permit CP to go sleep

Decreases the "ref_cnt" counter in the mem_link_device instance if it can go
sleep and allows CP to go sleep only if the value of "ref_cnt" counter is less
than or equal to 0.

@param mld	the pointer to a mem_link_device instance

@remark		MUST be invoked after forbid_cp_sleep() success to decrease the
		"ref_cnt" counter.
*/
static void permit_cp_sleep(struct mem_link_device *mld)
{
	struct modem_link_pm *pm = &mld->link_dev.pm;
	int ref_cnt;

	ref_cnt = atomic_dec_return(&mld->ref_cnt);
	if (ref_cnt > 0)
		return;

	if (ref_cnt < 0) {
		mif_info("WARNING! ref_cnt %d < 0\n", ref_cnt);
		atomic_set(&mld->ref_cnt, 0);
		ref_cnt = 0;
	}

	if (pm->release_hold)
		pm->release_hold(pm);
}

static bool check_link_status(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	struct modem_link_pm *pm = &ld->pm;

	if (gpio_get_value(mld->gpio_cp_status) == 0)
		return false;

	if (mipi_lli_get_link_status() != LLI_MOUNTED)
		return false;

	if (cp_online(mc))
		return pm->link_active ? pm->link_active(pm) : true;

	return true;
}

static void pm_fail_cb(struct modem_link_pm *pm)
{
	struct link_device *ld = pm_to_link_device(pm);
	struct mem_link_device *mld = ld_to_mem_link_device(ld);
	mem_forced_cp_crash(mld);
}

static void pm_cp_fail_cb(struct modem_link_pm *pm)
{
	struct link_device *ld = pm_to_link_device(pm);
	struct mem_link_device *mld = ld_to_mem_link_device(ld);
	struct modem_ctl *mc = ld->mc;
	struct io_device *iod = mc->iod;
	unsigned long flags;

	spin_lock_irqsave(&ld->lock, flags);

	if (cp_online(mc)) {
		mem_handle_cp_crash(mld, STATE_CRASH_EXIT);
	} else if (cp_booting(mc)) {
		iod->modem_state_changed(iod, STATE_OFFLINE);
		ld->reload(ld);
	}

	spin_unlock_irqrestore(&ld->lock, flags);
}

static void start_pm(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_link_pm *pm = &ld->pm;

	if (!pm->start)
		return;

	if (pm_enable) {
		if (mld->attr & MODEM_ATTR(ATTR_IOSM_MESSAGE))
			pm->start(pm, PM_EVENT_NO_EVENT);
		else
			pm->start(pm, PM_EVENT_CP_BOOTING);
	} else {
		pm->start(pm, PM_EVENT_LOCK_ON);
	}
}

static void stop_pm(struct mem_link_device *mld)
{
	struct modem_link_pm *pm = &mld->link_dev.pm;

	if (pm->stop)
		pm->stop(pm);
}

static int init_pm(struct mem_link_device *mld)
{
	struct link_device *ld = &mld->link_dev;
	struct modem_link_pm *pm = &ld->pm;
	struct link_pm_svc *pm_svc;
	int ret;

	atomic_set(&mld->ref_cnt, 0);

	pm_svc = mipi_lli_get_pm_svc();

	ret = init_link_device_pm(ld, pm, pm_svc, pm_fail_cb, pm_cp_fail_cb);
	if (ret < 0)
		return ret;

	return 0;
}

#else

static inline void change_irq_type(unsigned int irq, unsigned int value)
{
	unsigned int type;
	type = value ? IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH;
	irq_set_irq_type(irq, type);
}

/**
@brief		finalize handling the PHONE_START command from CP

@param mld	the pointer to a mem_link_device instance
*/
static void finalize_cp_start(struct mem_link_device *mld)
{
	int ap_wakeup = gpio_get_value(mld->gpio_ap_wakeup);
	int cp_status = gpio_get_value(mld->gpio_cp_status);

	change_irq_type(mld->irq_ap_wakeup.num, ap_wakeup);
	change_irq_type(mld->irq_cp_status.num, cp_status);

	if (ap_wakeup) {
		if (wake_lock_active(&mld->ap_wlock))
			wake_lock(&mld->ap_wlock);
	} else {
		if (wake_lock_active(&mld->ap_wlock))
			wake_unlock(&mld->ap_wlock);
	}

	if (cp_status) {
		if (!wake_lock_active(&mld->ap_wlock))
			wake_lock(&mld->cp_wlock);
	} else {
		if (wake_lock_active(&mld->ap_wlock))
			wake_unlock(&mld->cp_wlock);
	}

	print_pm_status(mld);
}

static bool check_link_status(struct mem_link_device *mld)
{
	if (mipi_lli_get_link_status() != LLI_MOUNTED)
		return false;

	if (gpio_get_value(mld->gpio_cp_status) == 0)
		return false;

	return true;
}

static void release_cp_wakeup(struct work_struct *ws)
{
	struct mem_link_device *mld;
	int i;
	unsigned long flags;

	mld = container_of(ws, struct mem_link_device, cp_sleep_dwork.work);

	if (work_pending(&mld->cp_sleep_dwork.work))
		cancel_delayed_work(&mld->cp_sleep_dwork);

	spin_lock_irqsave(&mld->pm_lock, flags);
	i = atomic_read(&mld->ref_cnt);
	spin_unlock_irqrestore(&mld->pm_lock, flags);
	if (i > 0)
		goto reschedule;

	if (gpio_get_value(mld->gpio_ap_wakeup) == 0) {
		gpio_set_value(mld->gpio_cp_wakeup, 0);
#ifdef CONFIG_LTE_MODEM_XMM7260
		gpio_set_value(mld->gpio_ap_status, 0);
#endif
	}

#if 1
	print_pm_status(mld);
#endif

	return;

reschedule:
	queue_delayed_work(system_nrt_wq, &mld->cp_sleep_dwork,
			   msecs_to_jiffies(sleep_timeout));
}

/**
@brief		forbid CP from going to sleep

Wakes up a CP if it can sleep and increases the "ref_cnt" counter in the
mem_link_device instance.

@param mld	the pointer to a mem_link_device instance

@remark		CAUTION!!! permit_cp_sleep() MUST be invoked after
		forbid_cp_sleep() success to decrease the "ref_cnt" counter.
*/
static void forbid_cp_sleep(struct mem_link_device *mld)
{
	int ref_cnt;
	unsigned long flags;
	int cp_wakeup;

	spin_lock_irqsave(&mld->pm_lock, flags);

	if (work_pending(&mld->cp_sleep_dwork.work))
		cancel_delayed_work(&mld->cp_sleep_dwork);

	ref_cnt = atomic_inc_return(&mld->ref_cnt);
	mif_debug("ref_cnt %d\n", ref_cnt);

	cp_wakeup = gpio_get_value(mld->gpio_cp_wakeup);
	gpio_set_value(mld->gpio_cp_wakeup, 1);

	if (cp_wakeup == 0)
		print_pm_status(mld);

	spin_unlock_irqrestore(&mld->pm_lock, flags);
}

/**
@brief	permit CP to go sleep

Decreases the "ref_cnt" counter in the mem_link_device instance if it can go
sleep and allows CP to go sleep only if the value of "ref_cnt" counter is less
than or equal to 0.

@param mld	the pointer to a mem_link_device instance

@remark		MUST be invoked after forbid_cp_sleep() success to decrease the
		"ref_cnt" counter.
*/
static void permit_cp_sleep(struct mem_link_device *mld)
{
	int ref_cnt;
	unsigned long flags;

	spin_lock_irqsave(&mld->pm_lock, flags);

	ref_cnt = atomic_dec_return(&mld->ref_cnt);
	if (ref_cnt > 0)
		goto exit;

	if (ref_cnt < 0) {
		mif_info("WARNING! ref_cnt %d < 0\n", ref_cnt);
		atomic_set(&mld->ref_cnt, 0);
	}

exit:
	spin_unlock_irqrestore(&mld->pm_lock, flags);
}

/**
@brief	interrupt handler for a wakeup interrupt

1) Reads the interrupt value\n
2) Performs interrupt handling\n

@param irq	the IRQ number
@param data	the pointer to a data
*/
static irqreturn_t ap_wakeup_interrupt(int irq, void *data)
{
	struct mem_link_device *mld = (struct mem_link_device *)data;
	int ap_wakeup = gpio_get_value(mld->gpio_ap_wakeup);
	int cp_wakeup = gpio_get_value(mld->gpio_cp_wakeup);

	change_irq_type(irq, ap_wakeup);

	if (work_pending(&mld->cp_sleep_dwork.work))
		cancel_delayed_work(&mld->cp_sleep_dwork);

	if (ap_wakeup) {
		if (!cp_wakeup)
			gpio_set_value(mld->gpio_cp_wakeup, 1);

		if (!wake_lock_active(&mld->ap_wlock))
			wake_lock(&mld->ap_wlock);

		if (mipi_lli_get_link_status() == LLI_UNMOUNTED)
			mipi_lli_set_link_status(LLI_WAITFORMOUNT);

		if (!mipi_lli_suspended())
			gpio_set_value(mld->gpio_ap_status, 1);

	} else {
		if (wake_lock_active(&mld->ap_wlock))
			wake_unlock(&mld->ap_wlock);

		if (mipi_lli_get_link_status() & LLI_WAITFORMOUNT)
			mipi_lli_set_link_status(LLI_UNMOUNTED);
		queue_delayed_work(system_nrt_wq, &mld->cp_sleep_dwork,
				msecs_to_jiffies(sleep_timeout));
	}

	print_pm_status(mld);

	return IRQ_HANDLED;
}

static irqreturn_t cp_status_handler(int irq, void *data)
{
	struct mem_link_device *mld = (struct mem_link_device *)data;
	struct link_device *ld = &mld->link_dev;
	struct modem_ctl *mc = ld->mc;
	int cp_status = gpio_get_value(mld->gpio_cp_status);
	unsigned long flags;

	spin_lock_irqsave(&mld->pm_lock, flags);

	change_irq_type(irq, cp_status);

	if (!cp_online(mc))
		goto exit;

	if (cp_status) {
		if (!wake_lock_active(&mld->cp_wlock))
			wake_lock(&mld->cp_wlock);
	} else {
		gpio_set_value(mld->gpio_ap_status, 0);

		if (wake_lock_active(&mld->cp_wlock))
			wake_unlock(&mld->cp_wlock);
	}

exit:
	print_pm_status(mld);
	spin_unlock_irqrestore(&mld->pm_lock, flags);
	return IRQ_HANDLED;
}

static void start_pm(struct mem_link_device *mld)
{
	if (pm_enable) {
		int ap_wakeup = gpio_get_value(mld->gpio_ap_wakeup);
		int cp_status = gpio_get_value(mld->gpio_cp_status);

		print_pm_status(mld);

		change_irq_type(mld->irq_ap_wakeup.num, ap_wakeup);
		mif_enable_irq(&mld->irq_ap_wakeup);

		change_irq_type(mld->irq_cp_status.num, cp_status);
		mif_enable_irq(&mld->irq_cp_status);

		lli_mark_last_busy();
	} else {
		wake_lock(&mld->ap_wlock);
	}
}

static void stop_pm(struct mem_link_device *mld)
{
	print_pm_status(mld);

	mif_disable_irq(&mld->irq_ap_wakeup);
	mif_disable_irq(&mld->irq_cp_status);
}

static int init_pm(struct mem_link_device *mld)
{
	int err;
	unsigned int gpio;
	unsigned int irq_ap_wakeup;
	unsigned int irq_cp_status;
	unsigned long flags;

	gpio_set_value(mld->gpio_ap_status, 0);

	/*
	Retrieve GPIO#, IRQ#, and IRQ flags for PM
	*/
	gpio = mld->gpio_ap_wakeup;
	irq_ap_wakeup = gpio_to_irq(gpio);
	mif_err("CP2AP_WAKEUP GPIO:%d IRQ:%d\n", gpio, irq_ap_wakeup);

	gpio = mld->gpio_cp_wakeup;
	mif_err("AP2CP_WAKEUP GPIO:%d\n", gpio);

	gpio = mld->gpio_cp_status;
	irq_cp_status = gpio_to_irq(gpio);
	mif_err("CP2AP_STATUS GPIO:%d IRQ:%d\n", gpio, irq_cp_status);

	gpio = mld->gpio_ap_status;
	mif_err("AP2CP_STATUS GPIO:%d\n", gpio);

	/*
	Initialize locks, completions, bottom halves, etc.
	*/
	wake_lock_init(&mld->ap_wlock, WAKE_LOCK_SUSPEND, "lli_ap_wlock");

	wake_lock_init(&mld->cp_wlock, WAKE_LOCK_SUSPEND, "lli_cp_wlock");

	INIT_DELAYED_WORK(&mld->cp_sleep_dwork, release_cp_wakeup);

	spin_lock_init(&mld->pm_lock);
	atomic_set(&mld->ref_cnt, 0);

	/*
	Enable IRQs for PM
	*/
	print_pm_status(mld);

	flags = (IRQF_TRIGGER_HIGH | IRQF_ONESHOT);

	mif_init_irq(&mld->irq_ap_wakeup, irq_ap_wakeup,
		     "lli_cp2ap_wakeup", flags);
	err = mif_request_threaded_irq(&mld->irq_ap_wakeup,
			ap_wakeup_interrupt, NULL, mld);
	if (err)
		return err;
	mif_disable_irq(&mld->irq_ap_wakeup);

	mif_init_irq(&mld->irq_cp_status, irq_cp_status,
		     "lli_cp2ap_status", flags);
	err = mif_request_irq(&mld->irq_cp_status, cp_status_handler, mld);
	if (err)
		return err;
	mif_disable_irq(&mld->irq_cp_status);

	return 0;
}
#endif
#endif

static void lli_link_ready(struct link_device *ld)
{
	evt_log(0, "%s: PM %s <%pf>\n", ld->name, CALLEE, CALLER);
	stop_pm(ld_to_mem_link_device(ld));
}

static void lli_link_reset(struct link_device *ld)
{
	evt_log(0, "%s: PM %s <%pf>\n", ld->name, CALLEE, CALLER);
	mipi_lli_reset();
}

static void lli_link_reload(struct link_device *ld)
{
	evt_log(0, "%s: PM %s <%pf>\n", ld->name, CALLEE, CALLER);
	mipi_lli_reload();
}

static void lli_link_off(struct link_device *ld)
{
	evt_log(0, "%s: PM %s <%pf>\n", ld->name, CALLEE, CALLER);
	stop_pm(ld_to_mem_link_device(ld));
	mipi_lli_reload();
}

static bool lli_link_suspended(struct link_device *ld)
{
	return mipi_lli_suspended() ? true : false;
}

static void lli_disable_irq(struct link_device *ld)
{
	mipi_lli_disable_irq();
}

static void lli_enable_irq(struct link_device *ld)
{
	mipi_lli_enable_irq();
}

#ifdef CONFIG_LINK_POWER_MANAGEMENT_WITH_FSM
/**
@brief	interrupt handler for a MIPI-LLI IPC interrupt

1) Get a free mst buffer\n
2) Reads the RXQ status and saves the status to the mst buffer\n
3) Saves the interrupt value to the mst buffer\n
4) Invokes mem_irq_handler that is common to all memory-type interfaces\n

@param data	the pointer to a mem_link_device instance
@param intr	the interrupt value
*/
static void lli_irq_handler(void *data, u32 intr)
{
	struct mem_link_device *mld = (struct mem_link_device *)data;
	struct mst_buff *msb;

	/* Prohibit CP from going to sleep */
	if (gpio_get_value(mld->gpio_cp_status) == 0
	    || gpio_get_value(mld->gpio_ap_status) == 0)
		print_pm_status(mld);

	msb = mem_take_snapshot(mld, RX);
	if (!msb)
		return;
	msb->snapshot.int2ap = (u16)intr;

	mem_irq_handler(mld, msb);
}
#else
/**
@brief	interrupt handler for a MIPI-LLI IPC interrupt

1) Get a free mst buffer\n
2) Reads the RXQ status and saves the status to the mst buffer\n
3) Saves the interrupt value to the mst buffer\n
4) Invokes mem_irq_handler that is common to all memory-type interfaces\n

@param data	the pointer to a mem_link_device instance
@param intr	the interrupt value
*/
static void lli_irq_handler(void *data, u32 intr)
{
	struct mem_link_device *mld = (struct mem_link_device *)data;
	struct mst_buff *msb;

	if (unlikely(lli_check_max_intr()))
		return;

	/* Prohibit CP from going to sleep */
	if (gpio_get_value(mld->gpio_cp_status) == 0
	    || gpio_get_value(mld->gpio_ap_status) == 0)
		print_pm_status(mld);

	if (gpio_get_value(mld->gpio_cp_wakeup) == 0)
		gpio_set_value(mld->gpio_cp_wakeup, 1);

	msb = mem_take_snapshot(mld, RX);
	if (!msb)
		return;
	msb->snapshot.int2ap = (u16)intr;

	mem_irq_handler(mld, msb);
}
#endif

static struct mem_link_device *g_mld;

struct link_device *lli_create_link_device(struct platform_device *pdev)
{
	struct modem_data *modem;
	struct mem_link_device *mld;
	struct link_device *ld;
	int err;
	unsigned long start;
	unsigned long size;
#ifdef DEBUG_MODEM_IF
	struct dentry *debug_dir = debugfs_create_dir("svnet", NULL);
#endif
	mif_err("+++\n");

	/**
	 * Get the modem (platform) data
	 */
	modem = (struct modem_data *)pdev->dev.platform_data;
	if (!modem) {
		mif_err("ERR! modem == NULL\n");
		return NULL;
	}

	if (!modem->gpio_ap_wakeup) {
		mif_err("ERR! no gpio_ap_wakeup\n");
		return NULL;
	}

	if (!modem->gpio_cp_status) {
		mif_err("ERR! no gpio_cp_status\n");
		return NULL;
	}

	mif_err("MODEM:%s LINK:%s\n", modem->name, modem->link_name);

	/**
	 * Create a MEMORY link device instance
	 */
	mld = mem_create_link_device(MEM_LLI_SHMEM, modem);
	if (!mld) {
		mif_err("%s: ERR! create_link_device fail\n", modem->link_name);
		return NULL;
	}

	g_mld = mld;

	ld = &mld->link_dev;

	ld->ready = lli_link_ready;
	ld->reset = lli_link_reset;
	ld->reload = lli_link_reload;
	ld->off = lli_link_off;

	ld->suspended = lli_link_suspended;

	ld->enable_irq = lli_enable_irq;
	ld->disable_irq = lli_disable_irq;

	/**
	 * Link local functions to the corresponding function pointers that are
	 * mandatory for all memory-type link devices
	 */
	mld->send_ap2cp_irq = send_ap2cp_irq;

	/*
	** Link local functions to the corresponding function pointers
	*/
#ifndef CONFIG_LINK_POWER_MANAGEMENT_WITH_FSM
	mld->finalize_cp_start = finalize_cp_start;
#endif

#ifdef CONFIG_LINK_POWER_MANAGEMENT
	mld->start_pm = start_pm;
	mld->stop_pm = stop_pm;
	mld->forbid_cp_sleep = forbid_cp_sleep;
	mld->permit_cp_sleep = permit_cp_sleep;
	mld->link_active = check_link_status;
#endif

#ifdef DEBUG_MODEM_IF
	mld->debug_info = mipi_lli_debug_info;
#endif

	/**
	 * Initialize SHMEM maps for IPC (physical map -> logical map)
	 */
	start = mipi_lli_get_phys_base();
	size = mipi_lli_get_phys_size();
	err = mem_register_ipc_rgn(mld, start, size);
	if (err < 0) {
		mif_err("%s: ERR! register_ipc_rgn fail (%d)\n", ld->name, err);
		goto error;
	}
	err = mem_setup_ipc_map(mld);
	if (err < 0) {
		mif_err("%s: ERR! setup_ipc_map fail (%d)\n", ld->name, err);
		mem_unregister_ipc_rgn(mld);
		goto error;
	}

#ifdef CONFIG_LINK_DEVICE_WITH_SBD_ARCH
	if (ld->sbd_ipc) {
		struct sbd_link_device *sld = &mld->sbd_link_dev;

		err = create_sbd_link_device(ld, sld, mld->base, mld->size);
		if (err < 0)
			goto error;
	}
#endif

	/**
	 * Register interrupt handlers
	 */
	err = mipi_lli_register_handler(lli_irq_handler, mld);
	if (err) {
		mif_err("%s: ERR! register_handler fail (%d)\n", ld->name, err);
		goto error;
	}

	/*
	** Retrieve GPIO#, IRQ#, and IRQ flags for PM
	*/
	mld->gpio_ap_wakeup = modem->gpio_ap_wakeup;
	mld->gpio_cp_wakeup = modem->gpio_cp_wakeup;
	mld->gpio_cp_status = modem->gpio_cp_status;
	mld->gpio_ap_status = modem->gpio_ap_status;

	/* Retrieve modem specific attribute value */
	mld->attr = modem->attr;

#ifdef CONFIG_LINK_DEVICE_WITH_SBD_ARCH
	if (mld->attr & MODEM_ATTR(ATTR_IOSM_MESSAGE))
		mld->cmd_handler = iosm_event_bh;
	else
		mld->cmd_handler = mem_cmd_handler;
#else
	mld->cmd_handler = mem_cmd_handler;
#endif

#ifdef CONFIG_LINK_POWER_MANAGEMENT
	err = init_pm(mld);
	if (err)
		goto error;
#endif

#ifdef DEBUG_MODEM_IF
	mld->mem_dump_blob.data = mld->base;
	mld->mem_dump_blob.size = mld->size;
	debugfs_create_blob("mem_dump", S_IRUGO, debug_dir,
						&mld->mem_dump_blob);
#endif

	mif_err("---\n");
	return ld;

error:
	kfree(mld);
	mif_err("xxx\n");
	return NULL;
}

