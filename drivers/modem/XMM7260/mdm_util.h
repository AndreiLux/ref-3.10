/*
 * linux/drivers/modem_control/mdm_util.h
 *
 * mdm_util.h
 * Utilities functions header.
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Contact: Faouaz Tenoutit <faouazx.tenoutit@intel.com>
 *          Frederic Berat <fredericx.berat@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/acpi_gpio.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/mdm_ctrl.h>
//#include <asm/intel-mid.h>
//#include <asm/intel_scu_pmic.h>
#include <linux/mdm_ctrl_board.h>

#ifndef _MDM_UTIL_H
#define _MDM_UTIL_H

#if defined(CONFIG_MIGRATION_CTL_DRV)
typedef enum {
	CORE_DUMP_RESET,
	CORE_DUMP_IDLE,
	HW_RESET,
	HOST_WAKEUP,
	NONE,
} mdm_bus_ctrl_reason;

typedef enum {
	RESET,
	IDLE,
	DONE,
} mdm_ctrl_bus_state;
#endif

#if defined(REMOVE_LOGS) || defined(CONFIG_IMC_LOG_OFF)

#if defined(pr_info)
#undef pr_info
#define pr_info(...)	printk("[MCD_I] " __VA_ARGS__)
#endif

#if defined(pr_err)
#undef pr_err
#define pr_err(...) 	printk("[MCD_E] " __VA_ARGS__)
#endif

#if defined(pr_warn)
#undef pr_warn
#define pr_warn(...) 	printk("[MCD_W] " __VA_ARGS__)
#endif

#if defined(pr_debug)
#undef pr_debug
#define pr_debug(...)	do {} while (false)
#endif

#elif defined(DEBUG)

#if defined(pr_debug)
#undef pr_debug
#define pr_debug(...) 	printk("[MCD_D] " __VA_ARGS__)
#endif

#endif

#if !defined(REMOVE_LOGS) && !defined(CONFIG_IMC_LOG_OFF)

#if defined(pr_err)
#undef pr_err
#define pr_err(...) 	printk("[MCD_E] " __VA_ARGS__)
#endif

#if defined(pr_info)
#undef pr_info
#define pr_info(...) 	printk("[MCD_I] " __VA_ARGS__)
#endif

#if defined(pr_warn)
#undef pr_warn
#define pr_warn(...) 	printk("[MCD_W] " __VA_ARGS__)
#endif

#endif

/**
 * struct mdm_ctrl - Modem boot driver
 *
 * @lock: spinlock to serialise access to the driver information
 * @major: char device major number
 * @tdev: char device type dev
 * @dev: char device
 * @cdev: char device
 * @class: char device class
 * @opened: This flag is used to allow only ONE instance of this driver
 * @wait_wq: Read/Poll/Select wait event
 * @gpio_rst_out: Modem RESET_OUT GPIO
 * @gpio_pwr_on: Modem PWR_ON GPIO
 * @gpio_rst_bbn: Modem RESET_BBN GPIO
 * @gpio_cdump: Modem coredump GPIO
 * @irq_cdump: the modem core dump interrupt line
 * @irq_reset: the modem reset interrupt line
 * @rst_ongoing: Stating that a reset is ongoing
 * @wq: Modem Reset/Coredump worqueue
 * @hangup_work: Modem Reset/Coredump work
 */
struct mdm_ctrl {
	/* Char device registration */
	int major;
	dev_t tdev;
	struct device *dev;
	struct cdev cdev;
	struct class *class;

	/* Device infos */
	struct mcd_base_info *pdata;

	/* Used to prevent multiple access to device */
	unsigned int opened;

	/* A waitqueue for poll/read operations */
	wait_queue_head_t wait_wq;
	unsigned int polled_states;
	bool polled_state_reached;

	/* modem status */
	atomic_t rst_ongoing;
	int hangup_causes;

	struct mutex lock;
	atomic_t modem_state;

	struct work_struct change_state_work;

	struct workqueue_struct *hu_wq;
	struct work_struct hangup_work;

	struct timer_list flashing_timer;

	/* Wait queue for WAIT_FOR_STATE ioctl */
	wait_queue_head_t event;

	bool is_mdm_ctrl_disabled;

#if defined(CONFIG_MIGRATION_CTL_DRV)
	struct delayed_work second_enum_work;

	int bus_ctrl_retry_cnt;
	mdm_bus_ctrl_reason bus_ctrl_reason;
	mdm_ctrl_bus_state bus_ctrl_set_state;
	struct delayed_work bus_ctrl_work;
	struct workqueue_struct *bus_ctrl_wq;
#endif
};

/* List of states */
struct next_state {
	struct list_head link;
	int state;
};

/* Modem control driver instance */
extern struct mdm_ctrl *mdm_drv;

void mdm_ctrl_enable_flashing(unsigned long int param);
void mdm_ctrl_disable_flashing(unsigned long int param);

#if defined(CONFIG_MIGRATION_CTL_DRV)
int mcd_pm_core_dump_set_bus(mdm_ctrl_bus_state val);
int mcd_pm_reset_out_set_bus(mdm_ctrl_bus_state val);
int mcd_pm_host_wakup_set_bus(mdm_ctrl_bus_state val);
void mdm_ctrl_launch_timer(struct timer_list *timer, int delay,
			   unsigned int timer_type, struct mdm_ctrl *mdm_drv);

#else
void mdm_ctrl_launch_timer(struct timer_list *timer, int delay,
			   unsigned int timer_type);
#endif

#if 0 // not called in the modem control module
inline void mdm_ctrl_set_reset_ongoing(struct mdm_ctrl *drv, int ongoing);
inline int mdm_ctrl_get_reset_ongoing(struct mdm_ctrl *drv);
#endif

void mdm_ctrl_get_device_info(struct mdm_ctrl *drv,
			      struct platform_device *pdev);

/**
 *  mdm_ctrl_set_opened - Set the open device state
 *  @drv: Reference to the driver structure
 *  @value: Value to set
 *
 */
extern inline void mdm_ctrl_set_opened(struct mdm_ctrl *drv, int value)
{
	/* Set the open flag */
	drv->opened = value;
}


/**
 *  mdm_ctrl_get_opened - Return the device open state
 *  @drv: Reference to the driver structure
 *
 */
extern inline int mdm_ctrl_get_opened(struct mdm_ctrl *drv)
{
	int opened;

	/* Set the open flag */
	opened = drv->opened;
	return opened;
}

/**
 *  mdm_ctrl_set_state -  Effectively sets the modem state on work execution
 *  @work: Reference to the work structure
 *
 */
extern inline void mdm_ctrl_set_state(struct mdm_ctrl *drv, int state)
{
	/* Set the current modem state */
	atomic_set(&drv->modem_state, state);
	if (likely(state != MDM_CTRL_STATE_UNKNOWN) &&
	    (state & drv->polled_states)) {

		drv->polled_state_reached = true;
		/* Waking up the poll work queue */
		wake_up(&drv->wait_wq);
		pr_info(DRVNAME ": Waking up polling 0x%x\r\n", state);

	}
	/* Waking up the wait_for_state work queue */
	wake_up(&drv->event);
}


/**
 *  mdm_ctrl_get_state - Return the local current modem state
 *  @drv: Reference to the driver structure
 *
 *  Note: Real current state may be different in case of self-reset
 *	  or if user has manually changed the state.
 */
extern inline int mdm_ctrl_get_state(struct mdm_ctrl *drv)
{
	return atomic_read(&drv->modem_state);
}


#endif				/* _MDM_UTIL_H */
