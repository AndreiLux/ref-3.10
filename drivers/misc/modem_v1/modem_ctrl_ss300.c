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

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <linux/mipi-lli.h>
#include "modem_prj.h"
#include "modem_utils.h"

#define MIF_INIT_TIMEOUT	(30 * HZ)

static struct wake_lock mc_wake_lock;

static void mc_state_fsm(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->iod);
	int cp_on = gpio_get_value(mc->gpio_cp_on);
	int cp_reset  = gpio_get_value(mc->gpio_cp_reset);
	int cp_active = gpio_get_value(mc->gpio_phone_active);
	enum modem_state old_state = mc->phone_state;
	enum modem_state new_state = mc->phone_state;

	mif_err("old_state:%s cp_on:%d cp_reset:%d cp_active:%d\n",
		cp_state_str(old_state), cp_on, cp_reset, cp_active);

	if (cp_active) {
		if (!cp_on)
			new_state = STATE_OFFLINE;
		else if (old_state == STATE_ONLINE)
			new_state = STATE_CRASH_EXIT;
		else
			mif_err("don't care!!!\n");
	}

	if (new_state != old_state) {
		/* Change the modem state for RIL */
		mc->iod->modem_state_changed(mc->iod, new_state);

		if (new_state == STATE_CRASH_EXIT) {
			if (ld->close_tx)
				ld->close_tx(ld);
		}

		/* Change the modem state for CBD */
		mc->bootd->modem_state_changed(mc->bootd, new_state);
	}
}

static irqreturn_t phone_active_handler(int irq, void *arg)
{
	struct modem_ctl *mc = (struct modem_ctl *)arg;
	int cp_reset = gpio_get_value(mc->gpio_cp_reset);

	if (cp_reset)
		mc_state_fsm(mc);

	return IRQ_HANDLED;
}

static inline void make_gpio_floating(unsigned int gpio, bool floating)
{
	if (floating)
		gpio_direction_input(gpio);
	else
		gpio_direction_output(gpio, 0);
}

static int ss300_on(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->iod);
	int cp_on;
	int cp_reset;
	int cp_active;
	int cp_status;
	unsigned long flags;

	mif_err("+++\n");

	spin_lock_irqsave(&ld->lock, flags);

	cp_on = gpio_get_value(mc->gpio_cp_on);
	cp_reset  = gpio_get_value(mc->gpio_cp_reset);
	cp_active = gpio_get_value(mc->gpio_phone_active);
	cp_status = gpio_get_value(mc->gpio_cp_status);

	mif_err("state:%s cp_on:%d cp_reset:%d cp_active:%d cp_status:%d\n",
		mc_state(mc), cp_on, cp_reset, cp_active, cp_status);

	mc->phone_state = STATE_OFFLINE;

	gpio_set_value(mc->gpio_pda_active, 1);

	if (mc->wake_lock && !wake_lock_active(mc->wake_lock)) {
		wake_lock(mc->wake_lock);
		mif_err("%s->wake_lock locked\n", mc->name);
	}

	if (ld->ready)
		ld->ready(ld);

	spin_unlock_irqrestore(&ld->lock, flags);

	mif_disable_irq(&mc->irq_cp_active);

	gpio_set_value(mc->gpio_cp_on, 0);
	msleep(100);

	gpio_set_value(mc->gpio_cp_reset, 0);
	msleep(500);

	gpio_set_value(mc->gpio_cp_on, 1);
	msleep(100);

	if (ld->reset)
		ld->reset(ld);

	gpio_set_value(mc->gpio_cp_reset, 1);
	msleep(300);

	mif_err("---\n");
	return 0;
}

static int ss300_off(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->iod);
	int cp_on;
	int cp_reset;
	int cp_active;
	int cp_status;
	unsigned long flags;

	mif_err("+++\n");

	spin_lock_irqsave(&ld->lock, flags);

	cp_on = gpio_get_value(mc->gpio_cp_on);
	cp_reset  = gpio_get_value(mc->gpio_cp_reset);
	cp_active = gpio_get_value(mc->gpio_phone_active);
	cp_status = gpio_get_value(mc->gpio_cp_status);

	mif_err("state:%s cp_on:%d cp_reset:%d cp_active:%d cp_status:%d\n",
		mc_state(mc), cp_on, cp_reset, cp_active, cp_status);

	if (mc->phone_state == STATE_OFFLINE)
		goto exit;

	mc->phone_state = STATE_OFFLINE;

	if (ld->off)
		ld->off(ld);

	if (gpio_get_value(mc->gpio_cp_on)) {
		if (gpio_get_value(mc->gpio_cp_reset)) {
			mif_err("%s: cp_reset -> 0\n", mc->name);
			gpio_set_value(mc->gpio_cp_reset, 0);
		}
	} else {
		mif_err("%s: cp_on == 0\n", mc->name);
	}

exit:
	spin_unlock_irqrestore(&ld->lock, flags);

	mif_err("---\n");
	return 0;
}

static int ss300_reset(struct modem_ctl *mc)
{
	mif_err("+++\n");

	if (ss300_off(mc))
		return -EIO;

	msleep(100);

	if (ss300_on(mc))
		return -EIO;

	mif_err("---\n");
	return 0;
}

static int ss300_force_crash_exit(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->iod);
	mif_err("+++\n");

	if (mc->wake_lock && !wake_lock_active(mc->wake_lock)) {
		wake_lock(mc->wake_lock);
		mif_err("%s->wake_lock locked\n", mc->name);
	}

	/* Make DUMP start */
	ld->force_dump(ld, mc->iod);

	mif_err("---\n");
	return 0;
}

static int ss300_dump_reset(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->iod);
	unsigned int gpio_cp_reset = mc->gpio_cp_reset;
	unsigned long flags;

	spin_lock_irqsave(&ld->lock, flags);

	mif_err("+++\n");

	if (mc->wake_lock && !wake_lock_active(mc->wake_lock)) {
		wake_lock(mc->wake_lock);
		mif_err("%s->wake_lock locked\n", mc->name);
	}

	if (ld->ready)
		ld->ready(ld);

	spin_unlock_irqrestore(&ld->lock, flags);

	mif_disable_irq(&mc->irq_cp_active);

	gpio_set_value(gpio_cp_reset, 0);
	udelay(200);

	if (ld->reset)
		ld->reset(ld);

	gpio_set_value(gpio_cp_reset, 1);
	msleep(300);

	gpio_set_value(mc->gpio_ap_status, 1);

	mif_err("---\n");
	return 0;
}

static int ss300_boot_on(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->iod);
	mif_err("+++\n");

	if (ld->boot_on)
		ld->boot_on(ld, mc->bootd);

	gpio_set_value(mc->gpio_ap_status, 1);

	INIT_COMPLETION(ld->init_cmpl);

	mc->bootd->modem_state_changed(mc->bootd, STATE_BOOTING);
	mc->iod->modem_state_changed(mc->iod, STATE_BOOTING);

	mif_err("---\n");
	return 0;
}

static int ss300_boot_off(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->iod);
	unsigned long remain;
	mif_err("+++\n");

	remain = wait_for_completion_timeout(&ld->init_cmpl, MIF_INIT_TIMEOUT);
	if (remain == 0) {
		mif_err("T-I-M-E-O-U-T\n");
		mif_err("xxx\n");
		return -EAGAIN;
	}

	atomic_set(&mc->forced_cp_crash, 0);

	mif_err("---\n");
	return 0;
}

static int ss300_boot_done(struct modem_ctl *mc)
{
	mif_err("+++\n");

	if (mc->wake_lock && wake_lock_active(mc->wake_lock)) {
		wake_unlock(mc->wake_lock);
		mif_err("%s->wake_lock unlocked\n", mc->name);
	}

	mif_enable_irq(&mc->irq_cp_active);

	mif_err("---\n");
	return 0;
}

static void ss300_get_ops(struct modem_ctl *mc)
{
	mc->ops.modem_on = ss300_on;
	mc->ops.modem_off = ss300_off;
	mc->ops.modem_reset = ss300_reset;
	mc->ops.modem_boot_on = ss300_boot_on;
	mc->ops.modem_boot_off = ss300_boot_off;
	mc->ops.modem_boot_done = ss300_boot_done;
	mc->ops.modem_force_crash_exit = ss300_force_crash_exit;
	mc->ops.modem_dump_reset = ss300_dump_reset;
}

int ss300_init_modemctl_device(struct modem_ctl *mc, struct modem_data *pdata)
{
	int ret = 0;
	unsigned int irq = 0;
	unsigned long flag = 0;
	char name[MAX_NAME_LEN];
	mif_err("+++\n");

	if (!pdata->gpio_cp_on || !pdata->gpio_cp_reset
	    || !pdata->gpio_pda_active || !pdata->gpio_phone_active
	    || !pdata->gpio_ap_wakeup || !pdata->gpio_ap_status
	    || !pdata->gpio_cp_wakeup || !pdata->gpio_cp_status) {
		mif_err("ERR! no GPIO data\n");
		mif_err("xxx\n");
		return -ENXIO;
	}

	mc->gpio_cp_on = pdata->gpio_cp_on;
	mc->gpio_cp_reset = pdata->gpio_cp_reset;
	mc->gpio_pda_active = pdata->gpio_pda_active;
	mc->gpio_phone_active = pdata->gpio_phone_active;
	mc->gpio_ap_wakeup = pdata->gpio_ap_wakeup;
	mc->gpio_ap_status = pdata->gpio_ap_status;
	mc->gpio_cp_wakeup = pdata->gpio_cp_wakeup;
	mc->gpio_cp_status = pdata->gpio_cp_status;

	gpio_set_value(mc->gpio_cp_reset, 0);

	gpio_set_value(mc->gpio_cp_on, 0);

	ss300_get_ops(mc);
	dev_set_drvdata(mc->dev, mc);

	wake_lock_init(&mc_wake_lock, WAKE_LOCK_SUSPEND, "ss300_wake_lock");
	mc->wake_lock = &mc_wake_lock;

	irq = gpio_to_irq(mc->gpio_phone_active);
	if (!irq) {
		mif_err("ERR! no irq_cp_active\n");
		mif_err("xxx\n");
		return -EINVAL;
	}
	mif_err("PHONE_ACTIVE IRQ# = %d\n", irq);

	flag = IRQF_TRIGGER_RISING | IRQF_NO_THREAD | IRQF_NO_SUSPEND;
	snprintf(name, MAX_NAME_LEN, "%s_active", mc->name);
	mif_init_irq(&mc->irq_cp_active, irq, name, flag);

	ret = mif_request_irq(&mc->irq_cp_active, phone_active_handler, mc);
	if (ret) {
		mif_err("%s: ERR! request_irq(%s#%d) fail (%d)\n",
			mc->name, mc->irq_cp_active.name,
			mc->irq_cp_active.num, ret);
		mif_err("xxx\n");
		return ret;
	}

	mif_err("---\n");
	return 0;
}

