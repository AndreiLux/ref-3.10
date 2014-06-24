/*
 * Copyright (c) 2014, HTC CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define pr_fmt(fmt) "[MDM]: " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/slab.h>
#include <mach/gpio-tegra.h>
#include <mach/board_htc.h>
#include <linux/platform_data/qcom_usb_modem_power.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/debugfs.h>
#include <linux/completion.h>

#ifdef CONFIG_MSM_SUBSYSTEM_RESTART
#include "subsystem_restart.h"
#define EXTERNAL_MODEM "external_modem"
#endif

#ifdef CONFIG_MSM_SYSMON_COMM
#include "sysmon.h"
#endif

#define BOOST_CPU_FREQ_MIN	1200000
#define BOOST_CPU_FREQ_TIMEOUT	5000

#define WAKELOCK_TIMEOUT_FOR_USB_ENUM		(HZ * 10)
#define WAKELOCK_TIMEOUT_FOR_REMOTE_WAKE	(HZ)

/* MDM timeout value definition */
#define MDM_MODEM_TIMEOUT	6000
#define MDM_MODEM_DELTA	100
#define MDM_BOOT_TIMEOUT	60000L
#define MDM_RDUMP_TIMEOUT	180000L

/* MDM misc driver ioctl definition */
#define CHARM_CODE		0xCC
#define WAKE_CHARM		_IO(CHARM_CODE, 1)
#define RESET_CHARM		_IO(CHARM_CODE, 2)
#define CHECK_FOR_BOOT		_IOR(CHARM_CODE, 3, int)
#define WAIT_FOR_BOOT		_IO(CHARM_CODE, 4)
#define NORMAL_BOOT_DONE	_IOW(CHARM_CODE, 5, int)
#ifdef CONFIG_MSM_SUBSYSTEM_RESTART
#define RAM_DUMP_DONE		_IOW(CHARM_CODE, 6, int)
#define WAIT_FOR_RESTART	_IOR(CHARM_CODE, 7, int)
#endif
#define EFS_SYNC_TIMEOUT   _IO(CHARM_CODE, 92)
#ifdef CONFIG_MDM_FTRACE_DEBUG
#define GET_FTRACE_CMD   	_IO(CHARM_CODE, 93)
#endif
#define GET_MFG_MODE   	_IO(CHARM_CODE, 94)
#define GET_RADIO_FLAG   	_IO(CHARM_CODE, 95)
#ifdef CONFIG_MDM_ERRMSG
#define MODEM_ERRMSG_LEN	256
#define SET_MODEM_ERRMSG	_IOW(CHARM_CODE, 96, char[MODEM_ERRMSG_LEN])
#ifdef CONFIG_COMPAT
#define COMPAT_SET_MODEM_ERRMSG	_IOW(CHARM_CODE, 96, compat_uptr_t)
#endif
#endif
#define TRIGGER_MODEM_FATAL	_IO(CHARM_CODE, 97)
#define EFS_SYNC_DONE		_IO(CHARM_CODE, 99)
#define NV_WRITE_DONE		_IO(CHARM_CODE, 100)
#define POWER_OFF_CHARM _IOW(CHARM_CODE, 101, int)

#ifdef CONFIG_MSM_SUBSYSTEM_RESTART
extern int get_enable_ramdumps(void);
#else
static int get_enable_ramdumps(void)
{
	return 0;
}
#endif

static void mdm_enable_irqs(struct qcom_usb_modem *modem, bool is_wake_irq)
{
	if(is_wake_irq)
	{
		if(!modem->mdm_wake_irq_enabled)
		{
			if(modem->wake_irq > 0)
			{
				enable_irq(modem->wake_irq);
				if(modem->wake_irq_wakeable)
				{
					if(modem->mdm_debug_on)
						pr_info("%s: enable wake irq\n", __func__);
					enable_irq_wake(modem->wake_irq);
				}
			}
		}
		modem->mdm_wake_irq_enabled = true;
	}
	else
	{
		if(!modem->mdm_irq_enabled)
		{
			if(modem->mdm_debug_on)
				pr_info("%s: enable mdm irq\n", __func__);

			if(modem->errfatal_irq > 0)
			{
				enable_irq(modem->errfatal_irq);
				if(modem->errfatal_irq_wakeable)
					enable_irq_wake(modem->errfatal_irq);
			}
			if(modem->hsic_ready_irq > 0)
			{
				enable_irq(modem->hsic_ready_irq);
				if(modem->hsic_ready_irq_wakeable)
					enable_irq_wake(modem->hsic_ready_irq);
			}
			if(modem->status_irq > 0)
			{
				enable_irq(modem->status_irq);
				if(modem->status_irq_wakeable)
					enable_irq_wake(modem->status_irq);
			}
			if(modem->ipc3_irq > 0)
			{
				enable_irq(modem->ipc3_irq);
				if(modem->ipc3_irq_wakeable)
					enable_irq_wake(modem->ipc3_irq);
			}
			if(modem->vdd_min_irq > 0)
			{
				enable_irq(modem->vdd_min_irq);
				if(modem->vdd_min_irq_wakeable)
					enable_irq_wake(modem->vdd_min_irq);
			}
		}

		modem->mdm_irq_enabled = true;
	}

	return;
}

static void mdm_disable_irqs(struct qcom_usb_modem *modem, bool is_wake_irq)
{
	if(is_wake_irq)
	{
		if(modem->mdm_wake_irq_enabled)
		{
			if(modem->wake_irq > 0)
			{
				disable_irq_nosync(modem->wake_irq);
				if(modem->wake_irq_wakeable)
				{
					if(modem->mdm_debug_on)
						pr_info("%s: disable wake irq\n", __func__);
					disable_irq_wake(modem->wake_irq);
				}
			}
		}
		modem->mdm_wake_irq_enabled = false;
	}
	else
	{
		if(modem->mdm_irq_enabled)
		{
			if(modem->mdm_debug_on)
				pr_info("%s: disable mdm irq\n", __func__);

			if(modem->errfatal_irq > 0)
			{
				disable_irq_nosync(modem->errfatal_irq);
				if(modem->errfatal_irq_wakeable)
					disable_irq_wake(modem->errfatal_irq);
			}
			if(modem->hsic_ready_irq > 0)
			{
				disable_irq_nosync(modem->hsic_ready_irq);
				if(modem->hsic_ready_irq_wakeable)
					disable_irq_wake(modem->hsic_ready_irq);
			}
			if(modem->status_irq > 0)
			{
				disable_irq_nosync(modem->status_irq);
				if(modem->status_irq_wakeable)
					disable_irq_wake(modem->status_irq);
			}
			if(modem->ipc3_irq > 0)
			{
				disable_irq_nosync(modem->ipc3_irq);
				if(modem->ipc3_irq_wakeable)
					disable_irq_wake(modem->ipc3_irq);
			}
			if(modem->vdd_min_irq > 0)
			{
				disable_irq_nosync(modem->vdd_min_irq);
				if(modem->vdd_min_irq_wakeable)
					disable_irq_wake(modem->vdd_min_irq);
			}
		}

		modem->mdm_irq_enabled = false;
	}

	return;
}

static int mdm_panic_prep(struct notifier_block *this, unsigned long event, void *ptr)
{
	int i;
	struct device *dev;
	struct qcom_usb_modem *modem;

	pr_err("%s: AP CPU kernel panic!!!\n", __func__);

	dev = bus_find_device_by_name(&platform_bus_type, NULL,
					"MDM");
	if (!dev) {
		pr_warn("%s unable to find device name\n", __func__);
		goto done;
	}

	modem = dev_get_drvdata(dev);

	mutex_lock(&modem->lock);

	mdm_disable_irqs(modem, false);
	mdm_disable_irqs(modem, true);
	gpio_set_value(modem->pdata->ap2mdm_errfatal_gpio, 1);

	if (modem->pdata->ap2mdm_wakeup_gpio >= 0)
		gpio_set_value(modem->pdata->ap2mdm_wakeup_gpio, 1);

	mutex_unlock(&modem->lock);

	for (i = MDM_MODEM_TIMEOUT; i > 0; i -= MDM_MODEM_DELTA) {
		/* pet_watchdog(); */
		msleep(MDM_MODEM_DELTA);
		if (gpio_get_value(modem->pdata->mdm2ap_status_gpio) == 0)
			break;
	}

	if (i <= 0)
		pr_err("%s: MDM2AP_STATUS never went low\n", __func__);

done:
	return NOTIFY_DONE;
}

static struct notifier_block mdm_panic_blk = {
	.notifier_call = mdm_panic_prep,
};

/* supported modems */
static const struct usb_device_id modem_list[] = {
	{USB_DEVICE(0x1d6b, 0x0002),	/* USB HSIC Hub */
	 .driver_info = 0,
	 },
	{USB_DEVICE(0x05c6, 0x9048),	/* USB MDM HSIC */
	 .driver_info = 0,
	 },
	{}
};

static ssize_t load_unload_usb_host(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count);

static void cpu_freq_unboost(struct work_struct *ws)
{
	struct qcom_usb_modem *modem = container_of(ws, struct qcom_usb_modem,
						     cpu_unboost_work.work);

	pm_qos_update_request(&modem->cpu_boost_req, PM_QOS_DEFAULT_VALUE);
}

static void cpu_freq_boost(struct work_struct *ws)
{
	struct qcom_usb_modem *modem = container_of(ws, struct qcom_usb_modem,
						     cpu_boost_work);

	cancel_delayed_work_sync(&modem->cpu_unboost_work);
	pm_qos_update_request(&modem->cpu_boost_req, BOOST_CPU_FREQ_MIN);
	queue_delayed_work(modem->wq, &modem->cpu_unboost_work,
			      msecs_to_jiffies(BOOST_CPU_FREQ_TIMEOUT));
}

static irqreturn_t qcom_usb_modem_wake_thread(int irq, void *data)
{
	struct qcom_usb_modem *modem = (struct qcom_usb_modem *)data;
	unsigned long start_time = jiffies;

	if (modem->mdm_debug_on)
		pr_info("%s start\n", __func__);

	mutex_lock(&modem->lock);

	if (modem->udev && modem->udev->state != USB_STATE_NOTATTACHED) {
		wake_lock_timeout(&modem->wake_lock,
				  WAKELOCK_TIMEOUT_FOR_REMOTE_WAKE);

		dev_info(&modem->pdev->dev, "remote wake (%u)\n",
			 ++(modem->wake_cnt));

		if (!modem->system_suspend) {
			mutex_unlock(&modem->lock);
			usb_lock_device(modem->udev);
			if (usb_autopm_get_interface(modem->intf) == 0)
			{
				pr_info("%s(%d) usb_autopm_get_interface OK %u ms\n", __func__, __LINE__, jiffies_to_msecs(jiffies-start_time));
				usb_autopm_put_interface_async(modem->intf);
			}
			usb_unlock_device(modem->udev);
			mutex_lock(&modem->lock);
		}

#ifdef CONFIG_PM
		if (modem->short_autosuspend_enabled && modem->pdata->autosuspend_delay > 0) {
			pm_runtime_set_autosuspend_delay(&modem->udev->dev,
					modem->pdata->autosuspend_delay);
			modem->short_autosuspend_enabled = 0;
		}
#endif
	}

	mutex_unlock(&modem->lock);

	if (modem->mdm_debug_on)
		pr_info("%s end\n", __func__);

	return IRQ_HANDLED;
}

static void mdm_hsic_ready(struct work_struct *ws)
{
	struct qcom_usb_modem *modem = container_of(ws, struct qcom_usb_modem,
						     mdm_hsic_ready_work);
	int value;

	mutex_lock(&modem->lock);

	value = gpio_get_value(modem->pdata->mdm2ap_hsic_ready_gpio);
	if (value == 1) {
		modem->mdm_status |= MDM_STATUS_HSIC_READY;
		if(modem->mdm_debug_on)
			pr_info("%s: modem->mdm_status=0x%x\n", __func__, modem->mdm_status);
	}

	mutex_unlock(&modem->lock);

	return;
}
static irqreturn_t qcom_usb_modem_hsic_ready_thread(int irq, void *data)
{
	struct qcom_usb_modem *modem = (struct qcom_usb_modem *)data;


	if(modem->mdm_debug_on)
		pr_info("%s: mdm sent hsic_ready interrupt\n", __func__);

	queue_work(modem->wq, &modem->mdm_hsic_ready_work);

	return IRQ_HANDLED;
}

static void mdm_status_changed(struct work_struct *ws)
{
	struct qcom_usb_modem *modem = container_of(ws, struct qcom_usb_modem,
						     mdm_status_work);
	int value;

	mutex_lock(&modem->lock);
	value = gpio_get_value(modem->pdata->mdm2ap_status_gpio);

	if(modem->mdm_debug_on)
		pr_info("%s: status: %d, mdm_status: 0x%x\n", __func__, value, modem->mdm_status);

	if(((modem->mdm_status & MDM_STATUS_STATUS_READY)?1:0) != value)
	{
		if(value && (modem->mdm_status & MDM_STATUS_BOOT_DONE))
		{
			if (!work_pending(&modem->host_reset_work))
				queue_work(modem->usb_host_wq, &modem->host_reset_work);

			mutex_unlock (&modem->lock);
			wait_for_completion(&modem->usb_host_reset_done);
			mutex_lock (&modem->lock);
			INIT_COMPLETION(modem->usb_host_reset_done);

			if(modem->ops && modem->ops->status_cb)
				modem->ops->status_cb(modem, value);
			modem->mdm9k_status = 1;
		}
	}

	if ((value == 0) && (modem->mdm_status & MDM_STATUS_BOOT_DONE)) {
		if(modem->mdm_status & MDM_STATUS_RESET)
			pr_info("%s: mdm is already under reset! Skip reset.\n", __func__);
		else
		{
			modem->mdm_status = MDM_STATUS_RESET;
			if(modem->mdm_debug_on)
				pr_info("%s: modem->mdm_status=0x%x\n", __func__, modem->mdm_status);
			pr_err("%s: unexpected reset external modem\n", __func__);
			if(modem->ops && modem->ops->dump_mdm_gpio_cb)
				modem->ops->dump_mdm_gpio_cb(modem, -1, "mdm_status_changed");
#ifdef CONFIG_MSM_SUBSYSTEM_RESTART
			subsystem_restart(EXTERNAL_MODEM);
#endif
		}
	} else if (value == 1) {
		pr_info("%s: mdm status = 1: mdm is now ready\n", __func__);
		modem->mdm_status |= MDM_STATUS_STATUS_READY;
		if(modem->mdm_debug_on)
			pr_info("%s: modem->mdm_status=0x%x\n", __func__, modem->mdm_status);
	}
	else
	{
		modem->mdm_status &= ~MDM_STATUS_STATUS_READY;
		if(modem->mdm_debug_on)
			pr_info("%s: modem->mdm_status=0x%x\n", __func__, modem->mdm_status);
	}

	mutex_unlock(&modem->lock);

	return;
}

static irqreturn_t qcom_usb_modem_status_thread(int irq, void *data)
{
	struct qcom_usb_modem *modem = (struct qcom_usb_modem *)data;

	if(modem->mdm_debug_on)
		pr_info("%s: mdm sent status interrupt\n", __func__);

	queue_work(modem->wq, &modem->mdm_status_work);

	return IRQ_HANDLED;
}

static void mdm_fatal(struct work_struct *ws)
{
	struct qcom_usb_modem *modem = container_of(ws, struct qcom_usb_modem,
						     mdm_errfatal_work);

	if(modem->mdm_status & MDM_STATUS_RESETTING)
	{
		pr_info("%s: Already under reseting procedure. Skip this reset.\n", __func__);
		return ;
	}
	else
		pr_info("%s: Reseting the mdm due to an errfatal\n", __func__);

	mutex_lock(&modem->lock);

	modem->mdm_status = MDM_STATUS_RESET;

	mutex_unlock(&modem->lock);

	if(modem->mdm_debug_on)
		pr_info("%s: modem->mdm_status=0x%x\n", __func__, modem->mdm_status);

	if(modem->ops && modem->ops->dump_mdm_gpio_cb)
		modem->ops->dump_mdm_gpio_cb(modem, -1, "mdm_fatal");

#ifdef CONFIG_PM
	if (modem->udev) {
		usb_disable_autosuspend(modem->udev);
		pr_info("disable autosuspend for %s %s\n",
			modem->udev->manufacturer, modem->udev->product);
	}
#endif

	if (get_enable_ramdumps()) {
		msleep(modem->pdata->ramdump_delay_ms);
	}

#ifdef CONFIG_MSM_SUBSYSTEM_RESTART
	subsystem_restart(EXTERNAL_MODEM);
#endif

	return;
}

static irqreturn_t qcom_usb_modem_errfatal_thread(int irq, void *data)
{
	struct qcom_usb_modem *modem = (struct qcom_usb_modem *)data;

	if(!modem)
		goto done;

	if(modem->mdm_debug_on)
		pr_info("%s: mdm got errfatal interrupt\n", __func__);

#ifdef CONFIG_MDM_FTRACE_DEBUG
	if (modem->ftrace_enable) {
		trace_printk("%s: mdm got errfatal interrupt\n", __func__);
		tracing_off();
	}
#endif

	if (modem->mdm_status & (MDM_STATUS_BOOT_DONE | MDM_STATUS_STATUS_READY)) {
		if(modem->mdm_debug_on)
			pr_info("%s: scheduling errfatal work now\n", __func__);
		queue_work(modem->mdm_recovery_wq, &modem->mdm_errfatal_work);
	}

done:
	return IRQ_HANDLED;
}

static irqreturn_t qcom_usb_modem_ipc3_thread(int irq, void *data)
{
	struct qcom_usb_modem *modem = (struct qcom_usb_modem *)data;
	int value;
	unsigned int elapsed_ms;
	static unsigned long last_ipc3_high_jiffies = 0;

	mutex_lock(&modem->lock);

	value = gpio_get_value(modem->pdata->mdm2ap_ipc3_gpio);

	if (modem->mdm_status & MDM_STATUS_BOOT_DONE) {
		if (value == 1) {
			last_ipc3_high_jiffies = get_jiffies_64();
		} else 	if (last_ipc3_high_jiffies != 0) {
			elapsed_ms = jiffies_to_msecs(get_jiffies_64() - last_ipc3_high_jiffies);

			if ( elapsed_ms>=450 && elapsed_ms<=550 ) {
				pr_info("need trigger mdm reset by Kickstart : normal reset \n");
				modem->boot_type = CHARM_NORMAL_BOOT;
				complete(&modem->mdm_needs_reload);
			} else if ( elapsed_ms>=50 && elapsed_ms<=150 ) {
				pr_info("need trigger mdm reset by Kickstart : CNV reset \n");
				modem->boot_type = CHARM_CNV_RESET;
				complete(&modem->mdm_needs_reload);
			} else {
				pr_info("IPC3 interrupt is noise. interval is %d ms. \n", elapsed_ms);
			}
			last_ipc3_high_jiffies = 0;
		}
	} else {
		last_ipc3_high_jiffies = 0;
	}

	if (modem->mdm_debug_on && modem->ops && modem->ops->dump_mdm_gpio_cb)
		modem->ops->dump_mdm_gpio_cb(modem, modem->pdata->mdm2ap_ipc3_gpio, "qcom_usb_modem_ipc3_thread");

	modem->mdm2ap_ipc3_status = value;

#ifdef CONFIG_MDM_FTRACE_DEBUG
	if (modem->ftrace_enable)
		trace_printk("mdm2ap_ipc3_status=%d\n", modem->mdm2ap_ipc3_status);
#endif

	mutex_unlock(&modem->lock);

	return IRQ_HANDLED;
}

static irqreturn_t qcom_usb_modem_vdd_min_thread(int irq, void *data)
{
	/* Currently nothing to do */

	return IRQ_HANDLED;
}

static void tegra_usb_host_reset(struct work_struct *ws)
{
	struct qcom_usb_modem *modem = container_of(ws, struct qcom_usb_modem,
						     host_reset_work);
	load_unload_usb_host(&modem->pdev->dev, NULL, "0", 1);
	load_unload_usb_host(&modem->pdev->dev, NULL, "1", 1);

	mutex_lock(&modem->lock);
	complete(&modem->usb_host_reset_done);
	mutex_unlock(&modem->lock);
}

static void tegra_usb_host_load(struct work_struct *ws)
{
	struct qcom_usb_modem *modem = container_of(ws, struct qcom_usb_modem,
						     host_load_work);
	load_unload_usb_host(&modem->pdev->dev, NULL, "1", 1);
}

static void tegra_usb_host_unload(struct work_struct *ws)
{
	struct qcom_usb_modem *modem = container_of(ws, struct qcom_usb_modem,
						     host_unload_work);
	load_unload_usb_host(&modem->pdev->dev, NULL, "0", 1);
}

static void device_add_handler(struct qcom_usb_modem *modem,
			       struct usb_device *udev)
{
	const struct usb_device_descriptor *desc = &udev->descriptor;
	struct usb_interface *intf = usb_ifnum_to_if(udev, 0);
	const struct usb_device_id *id = NULL;

	if (intf) {
		/* only look for specific modems if modem_list is provided in
		   platform data. Otherwise, look for the modems in the default
		   supported modem list */
		if (modem->pdata->modem_list)
			id = usb_match_id(intf, modem->pdata->modem_list);
		else
			id = usb_match_id(intf, modem_list);
	}

	if (id) {
		/* hold wakelock to ensure ril has enough time to restart */
		wake_lock_timeout(&modem->wake_lock,
				  WAKELOCK_TIMEOUT_FOR_USB_ENUM);

		pr_info("Add device %d <%s %s>\n", udev->devnum,
			udev->manufacturer, udev->product);

		mutex_lock(&modem->lock);
		modem->udev = udev;
		modem->parent = udev->parent;
		modem->intf = intf;
		modem->vid = desc->idVendor;
		modem->pid = desc->idProduct;
		modem->wake_cnt = 0;
		mutex_unlock(&modem->lock);

		pr_info("persist_enabled: %u\n", udev->persist_enabled);

#ifdef CONFIG_PM
		if(modem->pdata->autosuspend_delay > 0)
		{
			pm_runtime_set_autosuspend_delay(&udev->dev,
					modem->pdata->autosuspend_delay);
			usb_enable_autosuspend(udev);
			pr_info("enable autosuspend for %s %s\n",
				udev->manufacturer, udev->product);
		}
		modem->short_autosuspend_enabled = 0;

		/* allow the device to wake up the system */
		if (udev->actconfig->desc.bmAttributes &
		    USB_CONFIG_ATT_WAKEUP)
			device_set_wakeup_enable(&udev->dev, true);
#endif
	}
}

static void device_remove_handler(struct qcom_usb_modem *modem,
				  struct usb_device *udev)
{
	const struct usb_device_descriptor *desc = &udev->descriptor;

	if (desc->idVendor == modem->vid && desc->idProduct == modem->pid) {
		pr_info("Remove device %d <%s %s>\n", udev->devnum,
			udev->manufacturer, udev->product);

		mutex_lock(&modem->lock);
		modem->udev = NULL;
		modem->intf = NULL;
		modem->vid = 0;
		mutex_unlock(&modem->lock);
	}
}

static int mdm_usb_notifier(struct notifier_block *notifier,
			    unsigned long usb_event, void *udev)
{
	struct qcom_usb_modem *modem =
	    container_of(notifier, struct qcom_usb_modem, usb_notifier);

	switch (usb_event) {
	case USB_DEVICE_ADD:
		device_add_handler(modem, udev);
		break;
	case USB_DEVICE_REMOVE:
		device_remove_handler(modem, udev);
		break;
	}
	return NOTIFY_OK;
}

static int mdm_pm_notifier(struct notifier_block *notifier,
			   unsigned long pm_event, void *unused)
{
	struct qcom_usb_modem *modem =
	    container_of(notifier, struct qcom_usb_modem, pm_notifier);

	mutex_lock(&modem->lock);
	if (!modem->udev) {
		mutex_unlock(&modem->lock);
		return NOTIFY_DONE;
	}

	pr_info("%s: event %ld\n", __func__, pm_event);
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		pr_info("%s : PM_SUSPEND_PREPARE\n", __func__);
		if (wake_lock_active(&modem->wake_lock)) {
			pr_warn("%s: wakelock was active, aborting suspend\n",
				__func__);
			mutex_unlock(&modem->lock);
			return NOTIFY_STOP;
		}

		modem->system_suspend = 1;
#ifdef CONFIG_PM
		if (modem->udev && modem->pdata->short_autosuspend_delay > 0 &&
		    modem->udev->state != USB_STATE_NOTATTACHED) {
			pm_runtime_set_autosuspend_delay(&modem->udev->dev,
					modem->pdata->short_autosuspend_delay);
			modem->short_autosuspend_enabled = 1;
			pr_info("%s: modem->short_autosuspend_enabled: %d (ms)\n", __func__, modem->pdata->short_autosuspend_delay);
		}
#endif
		mutex_unlock(&modem->lock);
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		pr_info("%s : PM_POST_SUSPEND\n", __func__);
		modem->system_suspend = 0;
		mutex_unlock(&modem->lock);
		return NOTIFY_OK;
	}

	mutex_unlock(&modem->lock);
	return NOTIFY_DONE;
}

static int mdm_request_irq(struct qcom_usb_modem *modem,
			   irq_handler_t thread_fn,
			   unsigned int irq_gpio,
			   unsigned long irq_flags,
			   const char *label,
			   unsigned int *irq,
			   bool *is_wakeable)
{
	int ret;

	/* gpio request is done in modem_init callback */
#if 0
	ret = gpio_request(irq_gpio, label);
	if (ret)
		return ret;
#endif

	/* enable IRQ for GPIO */
	*irq = gpio_to_irq(irq_gpio);

	ret = request_threaded_irq(*irq, NULL, thread_fn, irq_flags, label, modem);
	if (ret) {
		*irq = 0;
		return ret;
	}

	ret = enable_irq_wake(*irq);
	*is_wakeable = (ret) ? false : true;

	return 0;
}

static void mdm_hsic_phy_open(void)
{
	struct device *dev;
	struct qcom_usb_modem *modem;

	dev = bus_find_device_by_name(&platform_bus_type, NULL,
					"MDM");
	if (!dev) {
		pr_warn("%s unable to find device name\n", __func__);
		return;
	}

	modem = dev_get_drvdata(dev);

	return;
}

static void mdm_hsic_phy_init(void)
{
	struct device *dev;
	struct qcom_usb_modem *modem;

	dev = bus_find_device_by_name(&platform_bus_type, NULL,
					"MDM");
	if (!dev) {
		pr_warn("%s unable to find device name\n", __func__);
		return;
	}

	modem = dev_get_drvdata(dev);

	if(modem->mdm_debug_on)
		pr_info("%s\n", __func__);

	return;
}

static void mdm_hsic_print_interface_pm_info(struct usb_device *udev)
{
	struct usb_interface *intf;
	int i = 0, n = 0;

	if (udev == NULL)
		return;

	dev_info(&udev->dev, "%s:\n", __func__);

	if (udev->actconfig) {
		n = udev->actconfig->desc.bNumInterfaces;
		for (i = 0; i <= n - 1; i++) {
			intf = udev->actconfig->interface[i];
			pr_info("[HSIC_PM_DBG] intf:%d pm_usage_cnt:%d usage_count:%d\n", i,
			atomic_read(&intf->pm_usage_cnt), atomic_read(&intf->dev.power.usage_count));
		}
	}
}

static void mdm_hsic_phy_suspend(struct qcom_usb_modem *modem)
{
	unsigned long elapsed_ms = 0;
	static unsigned int suspend_cnt = 0;

	pr_info("%s\n", __func__);

#ifdef CONFIG_MDM_FTRACE_DEBUG
	if (modem->ftrace_enable)
		trace_printk("%s\n", __func__);
#endif

	mutex_lock(&modem->lock);

	if (modem->mdm_hsic_phy_resume_jiffies != 0) {
		elapsed_ms = jiffies_to_msecs(jiffies - modem->mdm_hsic_phy_resume_jiffies);
		modem->mdm_hsic_phy_active_total_ms += elapsed_ms;
	}

	suspend_cnt++;
	if (elapsed_ms > 30000 || suspend_cnt >= 10) {
		suspend_cnt = 0;
		if(modem->mdm_debug_on)
		{
			pr_info("%s: elapsed_ms: %lu ms\n", __func__, elapsed_ms);
			mdm_hsic_print_interface_pm_info(modem->udev);
		}
	}

	pr_info("%s: phy_active_total_ms: %lu ms\n", __func__, modem->mdm_hsic_phy_active_total_ms);

	mutex_unlock(&modem->lock);

	return;
}

static void mdm_hsic_phy_pre_suspend(void)
{
	struct device *dev;
	struct qcom_usb_modem *modem;

	dev = bus_find_device_by_name(&platform_bus_type, NULL,
					"MDM");
	if (!dev) {
		pr_warn("%s unable to find device name\n", __func__);
		return;
	}

	modem = dev_get_drvdata(dev);

	pr_info("%s\n", __func__);

#ifdef CONFIG_MDM_FTRACE_DEBUG
	if (modem->ftrace_enable)
		trace_printk("%s\n", __func__);
#endif

	return;
}

static void mdm_hsic_phy_post_suspend(void)
{
	struct device *dev;
	struct qcom_usb_modem *modem;

	dev = bus_find_device_by_name(&platform_bus_type, NULL,
					"MDM");
	if (!dev) {
		pr_warn("%s unable to find device name\n", __func__);
		return;
	}

	modem = dev_get_drvdata(dev);

	mdm_hsic_phy_suspend(modem);

	pr_info("%s\n", __func__);

#ifdef CONFIG_MDM_FTRACE_DEBUG
	if (modem->ftrace_enable)
		trace_printk("%s\n", __func__);
#endif

	return;
}

static void mdm_hsic_phy_resume(struct qcom_usb_modem *modem)
{
	pr_info("%s\n", __func__);

#ifdef CONFIG_MDM_FTRACE_DEBUG
	if (modem->ftrace_enable)
		trace_printk("%s\n", __func__);
#endif

	mutex_lock(&modem->lock);

	modem->mdm_hsic_phy_resume_jiffies = jiffies;

	mutex_unlock(&modem->lock);

	return;
}

static void mdm_hsic_phy_pre_resume(void)
{
	struct device *dev;
	struct qcom_usb_modem *modem;

	dev = bus_find_device_by_name(&platform_bus_type, NULL,
					"MDM");
	if (!dev) {
		pr_warn("%s unable to find device name\n", __func__);
		return;
	}

	modem = dev_get_drvdata(dev);

	pr_info("%s\n", __func__);

#ifdef CONFIG_MDM_FTRACE_DEBUG
	if (modem->ftrace_enable)
		trace_printk("%s\n", __func__);
#endif

	mdm_hsic_phy_resume(modem);

	return;
}

static void mdm_hsic_phy_post_resume(void)
{
	struct device *dev;
	struct qcom_usb_modem *modem;

	dev = bus_find_device_by_name(&platform_bus_type, NULL,
					"MDM");
	if (!dev) {
		pr_warn("%s unable to find device name\n", __func__);
		return;
	}

	modem = dev_get_drvdata(dev);

	pr_info("%s\n", __func__);

#ifdef CONFIG_MDM_FTRACE_DEBUG
	if (modem->ftrace_enable)
		trace_printk("%s\n", __func__);
#endif

	return;
}

static void mdm_post_remote_wakeup(void)
{
	struct device *dev;
	struct qcom_usb_modem *modem;

	dev = bus_find_device_by_name(&platform_bus_type, NULL,
					"MDM");
	if (!dev) {
		pr_warn("%s unable to find device name\n", __func__);
		return;
	}

	modem = dev_get_drvdata(dev);

	mutex_lock(&modem->lock);

#ifdef CONFIG_PM
	if (modem->udev &&
	    modem->udev->state != USB_STATE_NOTATTACHED &&
	    modem->short_autosuspend_enabled && modem->pdata->autosuspend_delay > 0) {
		pm_runtime_set_autosuspend_delay(&modem->udev->dev,
				modem->pdata->autosuspend_delay);
		modem->short_autosuspend_enabled = 0;
	}
#endif
	wake_lock_timeout(&modem->wake_lock, WAKELOCK_TIMEOUT_FOR_REMOTE_WAKE);

	mutex_unlock(&modem->lock);

	return;
}

void mdm_hsic_phy_close(void)
{
	struct device *dev;
	struct qcom_usb_modem *modem;

	dev = bus_find_device_by_name(&platform_bus_type, NULL,
					"MDM");
	if (!dev) {
		pr_warn("%s unable to find device name\n", __func__);
		return;
	}

	modem = dev_get_drvdata(dev);

	return;
}

/* load USB host controller */
static struct platform_device *tegra_usb_host_register(
				const struct qcom_usb_modem *modem)
{
	const struct platform_device *hc_device =
	    modem->pdata->tegra_ehci_device;
	struct platform_device *pdev;
	int val;

	pdev = platform_device_alloc(hc_device->name, hc_device->id);
	if (!pdev)
		return NULL;

	val = platform_device_add_resources(pdev, hc_device->resource,
					    hc_device->num_resources);
	if (val)
		goto error;

	pdev->dev.dma_mask = hc_device->dev.dma_mask;
	pdev->dev.coherent_dma_mask = hc_device->dev.coherent_dma_mask;

	val = platform_device_add_data(pdev, modem->pdata->tegra_ehci_pdata,
				       sizeof(struct tegra_usb_platform_data));
	if (val)
		goto error;

	val = platform_device_add(pdev);
	if (val)
		goto error;

	return pdev;

error:
	pr_err("%s: err %d\n", __func__, val);
	platform_device_put(pdev);
	return NULL;
}

/* unload USB host controller */
static void tegra_usb_host_unregister(struct platform_device *pdev)
{
	platform_device_unregister(pdev);
}

static ssize_t load_unload_usb_host(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct qcom_usb_modem *modem = dev_get_drvdata(dev);
	int host;

	if (sscanf(buf, "%d", &host) != 1 || host < 0 || host > 1)
		return -EINVAL;

	pr_info("%s USB host\n", (host) ? "load" : "unload");

	mutex_lock(&modem->hc_lock);
	if (host) {
		if (!modem->hc)
			modem->hc = tegra_usb_host_register(modem);
	} else {
		if (modem->hc) {
			tegra_usb_host_unregister(modem->hc);
			modem->hc = NULL;
		}
	}
	mutex_unlock(&modem->hc_lock);

	return count;
}

static struct tegra_usb_phy_platform_ops qcom_usb_modem_debug_remote_wakeup_ops = {
	.open = mdm_hsic_phy_open,
	.init = mdm_hsic_phy_init,
	.pre_suspend = mdm_hsic_phy_pre_suspend,
	.post_suspend = mdm_hsic_phy_post_suspend,
	.pre_resume = mdm_hsic_phy_pre_resume,
	.post_resume = mdm_hsic_phy_post_resume,
	.post_remote_wakeup = mdm_post_remote_wakeup,
	.close = mdm_hsic_phy_close,
};

static struct tegra_usb_phy_platform_ops qcom_usb_modem_remote_wakeup_ops = {
	.post_remote_wakeup = mdm_post_remote_wakeup,
};

static int proc_mdm9k_status(struct seq_file *s, void *unused)
{
	struct device *dev;
	struct qcom_usb_modem *modem;

	dev = bus_find_device_by_name(&platform_bus_type, NULL,
					"MDM");
	if (!dev) {
		pr_warn("%s unable to find device name\n", __func__);

		seq_printf(s, "0\n");
		return 0;
	}

	modem = dev_get_drvdata(dev);

	seq_printf(s, "%d\n", modem->mdm9k_status);
	return 0;
}

static int proc_mdm9k_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_mdm9k_status, PDE_DATA(inode));
}

int proc_mdm9k_release(struct inode *inode, struct file *file)
{
	const struct seq_operations *op = ((struct seq_file *)file->private_data)->op;
	int res = seq_release(inode, file);
	kfree(op);
	return res;
}

static const struct file_operations mdm9k_proc_ops = {
	.owner = THIS_MODULE,
	.open = proc_mdm9k_open,
	.read = seq_read,
	.release = proc_mdm9k_release,
};

static void mdm_loaded_info(struct qcom_usb_modem *modem)
{
	modem->mdm9k_status = 0;

	modem ->mdm9k_pde = proc_create_data("mdm9k_status", 0, NULL, &mdm9k_proc_ops, NULL);
}

static void mdm_unloaded_info(struct qcom_usb_modem *modem)
{
	if (modem ->mdm9k_pde)
		remove_proc_entry("mdm9k_status", NULL);
}

#ifdef CONFIG_MDM_FTRACE_DEBUG
static void execute_ftrace_cmd(char *cmd, struct qcom_usb_modem *modem)
{
	int ret;

	if (get_radio_flag() == RADIO_FLAG_NONE)
		return;

	/* wait until ftrace cmd can be executed */
	ret = wait_for_completion_interruptible(&modem->ftrace_cmd_can_be_executed);
	INIT_COMPLETION(modem->ftrace_cmd_can_be_executed);
	if (!ret) {
		/* copy cmd to ftrace_cmd buffer */
		mutex_lock(&modem->ftrace_cmd_lock);
		memset(modem->ftrace_cmd, 0, sizeof(modem->ftrace_cmd));
		strlcpy(modem->ftrace_cmd, cmd, sizeof(modem->ftrace_cmd));
		pr_info("%s: ftrace_cmd (%s)\n", __func__, modem->ftrace_cmd);
		mutex_unlock(&modem->ftrace_cmd_lock);

		/* signal the waiting thread there is pending cmd */
		complete(&modem->ftrace_cmd_pending);
	}
}

static void ftrace_enable_basic_log_fn(struct work_struct *ws)
{
	struct qcom_usb_modem *modem = container_of(ws, struct qcom_usb_modem,
						     ftrace_enable_log_work);

	pr_info("%s+\n", __func__);
	execute_ftrace_cmd("echo 8192 > /sys/kernel/debug/tracing/buffer_size_kb", modem);
	execute_ftrace_cmd("echo 1 > /sys/kernel/debug/tracing/tracing_on", modem);
	pr_info("%s-\n", __func__);
}
#endif

#ifdef CONFIG_MDM_ERRMSG
static int set_mdm_errmsg(void __user * msg, struct qcom_usb_modem *modem)
{
	if(modem == NULL)
		return -EFAULT;

	memset(modem->mdm_errmsg, 0, sizeof(modem->mdm_errmsg));
	if (unlikely(copy_from_user(modem->mdm_errmsg, msg, sizeof(modem->mdm_errmsg)))) {
		pr_err("%s: copy modem_errmsg failed\n", __func__);
		return -EFAULT;
	}

	modem->mdm_errmsg[sizeof(modem->mdm_errmsg) - 1] = '\0';
	pr_info("%s: set mdm errmsg: %s\n", __func__, modem->mdm_errmsg);

	return 0;
}

char *get_mdm_errmsg(struct qcom_usb_modem *modem)
{
	if(modem == NULL)
		return NULL;

	if (strlen(modem->mdm_errmsg) <= 0) {
		pr_err("%s: can not get mdm errmsg.\n", __func__);
		return NULL;
	}

	return modem->mdm_errmsg;
}

EXPORT_SYMBOL(get_mdm_errmsg);
#endif

static int mdm_modem_open(struct inode *inode, struct file *file)
{
	return 0;
}

long mdm_modem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int status, ret = 0;
	struct device *dev;
	struct qcom_usb_modem *modem;

	if (_IOC_TYPE(cmd) != CHARM_CODE) {
		pr_err("%s: invalid ioctl code\n", __func__);
		return -EINVAL;
	}

	dev = bus_find_device_by_name(&platform_bus_type, NULL,
					"MDM");
	if (!dev) {
		pr_warn("%s unable to find device name\n", __func__);
		return -EINVAL;
	}

	modem = dev_get_drvdata(dev);

	if(modem->mdm_debug_on)
		pr_info("%s: Entering ioctl cmd = %d\n", __func__, _IOC_NR(cmd));

	mutex_lock(&modem->lock);

	switch (cmd) {
	case WAKE_CHARM:
		pr_info("%s: Powering on mdm\n", __func__);
		if(!(modem->mdm_status & (MDM_STATUS_RAMDUMP | MDM_STATUS_RESET | MDM_STATUS_RESETTING)))
		{
			modem->mdm_status = MDM_STATUS_POWER_DOWN;
			if(modem->mdm_debug_on)
				pr_info("%s: modem->mdm_status=0x%x\n", __func__, modem->mdm_status);
		}

		if (!work_pending(&modem->host_reset_work))
			queue_work(modem->usb_host_wq, &modem->host_reset_work);

		/* hold wait lock to complete the enumeration */
		wake_lock_timeout(&modem->wake_lock, WAKELOCK_TIMEOUT_FOR_USB_ENUM);

		/* boost CPU freq */
		if (!work_pending(&modem->cpu_boost_work))
			queue_work(modem->wq, &modem->cpu_boost_work);

		/* Wait for usb host reset done */
		mutex_unlock(&modem->lock);
		wait_for_completion(&modem->usb_host_reset_done);
		mutex_lock(&modem->lock);
		INIT_COMPLETION(modem->usb_host_reset_done);

		if(modem->ops && modem->ops->dump_mdm_gpio_cb)
			modem->ops->dump_mdm_gpio_cb(modem, -1, "power_on_mdm (before)");

		/* Enable irq */
		mdm_enable_irqs(modem, false);
		mdm_enable_irqs(modem, true);

		/* start modem */
		if (modem->ops && modem->ops->start)
			modem->ops->start(modem);

		modem->mdm_status |= MDM_STATUS_POWER_ON;
		if(modem->mdm_debug_on)
			pr_info("%s: modem->mdm_status=0x%x\n", __func__, modem->mdm_status);
		break;

	case CHECK_FOR_BOOT:
		if (gpio_get_value(modem->pdata->mdm2ap_status_gpio) == 0)
			put_user(1, (unsigned long __user *)arg);
		else
			put_user(0, (unsigned long __user *)arg);
		break;

	case NORMAL_BOOT_DONE:
		{
			if(modem->mdm_debug_on)
				pr_info("%s: check if mdm is booted up\n", __func__);

			get_user(status, (unsigned long __user *)arg);
			if (status) {
				pr_err("%s: normal boot failed\n", __func__);
			} else {
				pr_info("%s: normal boot done\n", __func__);
				modem->mdm_status |= MDM_STATUS_BOOT_DONE;
				if(modem->mdm_debug_on)
					pr_info("%s: modem->mdm_status=0x%x\n", __func__, modem->mdm_status);
			}

			if((modem->mdm_status & MDM_STATUS_BOOT_DONE) && (modem->mdm_status & MDM_STATUS_STATUS_READY))
			{
				if (!work_pending(&modem->host_reset_work))
					queue_work(modem->usb_host_wq, &modem->host_reset_work);

				mutex_unlock(&modem->lock);
				wait_for_completion(&modem->usb_host_reset_done);
				mutex_lock(&modem->lock);
				INIT_COMPLETION(modem->usb_host_reset_done);

				if (modem->ops->normal_boot_done_cb != NULL) {
					pr_info("normal_boot_done_cb\n");
					modem->ops->normal_boot_done_cb(modem);
				}
				modem->mdm9k_status = 1;
			}

#ifdef CONFIG_MSM_SUBSYSTEM_RESTART
			if (modem->mdm_status & MDM_STATUS_RESET)
			{
				pr_info("%s: modem is under reset: complete mdm_boot\n", __func__);
				complete(&modem->mdm_boot);
				modem->mdm_status &= ~MDM_STATUS_RESET;
				if(modem->mdm_debug_on)
					pr_info("%s: modem->mdm_status=0x%x\n", __func__, modem->mdm_status);
			}
#endif

#ifdef CONFIG_MDM_FTRACE_DEBUG
			if (modem->ftrace_enable) {
				if (modem->ftrace_wq) {
					queue_work(modem->ftrace_wq, &modem->ftrace_enable_log_work);
				}
			}
#endif
		}
		break;

#ifdef CONFIG_MSM_SUBSYSTEM_RESTART
	case RAM_DUMP_DONE:
		if(modem->mdm_debug_on)
			pr_info("%s: mdm done collecting RAM dumps\n", __func__);

		get_user(status, (unsigned long __user *)arg);
		if (status)
			pr_err("%s: ramdump collection fail.\n", __func__);
		else
			pr_info("%s: ramdump collection completed\n", __func__);

		modem->mdm_status &= ~MDM_STATUS_RAMDUMP;
		if(modem->mdm_debug_on)
			pr_info("%s: modem->mdm_status=0x%x\n", __func__, modem->mdm_status);

		complete(&modem->mdm_ram_dumps);
		break;

	case WAIT_FOR_RESTART:
		if(modem->mdm_debug_on)
			pr_info("%s: wait for mdm to need images reloaded\n", __func__);

		mutex_unlock(&modem->lock);
		ret = wait_for_completion_interruptible(&modem->mdm_needs_reload);
		mutex_lock(&modem->lock);

		if ( modem->boot_type==CHARM_NORMAL_BOOT ) {
		        pr_info("%s: modem boot_type=Normal_boot\n", __func__);
		} else {
		        pr_info("%s: modem boot_type=%s\n", __func__, ((modem->boot_type==CHARM_RAM_DUMPS)?"Ram_dump":"CNV_Reset"));
		}

		if (!ret)
			put_user(modem->boot_type, (unsigned long __user *)arg);
		INIT_COMPLETION(modem->mdm_needs_reload);
		break;
#endif

#ifdef CONFIG_MDM_FTRACE_DEBUG
	case GET_FTRACE_CMD:
		{
			/* execute ftrace cmd only when radio flag is non-zero */
			if (get_radio_flag() != RADIO_FLAG_NONE) {
				complete(&modem->ftrace_cmd_can_be_executed);
				mutex_unlock(&modem->lock);
				ret = wait_for_completion_interruptible(&modem->ftrace_cmd_pending);
				mutex_lock(&modem->lock);
				if (!ret) {
					mutex_lock(&modem->ftrace_cmd_lock);
					pr_info("ioctl GET_FTRACE_CMD: %s\n", modem->ftrace_cmd);
					if (copy_to_user((void __user *)arg, modem->ftrace_cmd, sizeof(modem->ftrace_cmd))) {
						pr_err("GET_FTRACE_CMD read fail\n");
					}
					mutex_unlock(&modem->ftrace_cmd_lock);
				}
				INIT_COMPLETION(modem->ftrace_cmd_pending);
			}
			break;
		}
#endif

	case GET_MFG_MODE:
		pr_info("%s: board_mfg_mode() = %d\n", __func__, board_mfg_mode());
		put_user(board_mfg_mode(), (unsigned long __user *)arg);
		break;

	case GET_RADIO_FLAG:
		pr_info("%s: get_radio_flag() = %x\n", __func__, get_radio_flag());
		put_user(get_radio_flag(), (unsigned long __user *)arg);
		break;

#ifdef CONFIG_MDM_ERRMSG
	case SET_MODEM_ERRMSG:
		pr_info("%s: Set modem fatal errmsg\n", __func__);
		ret = set_mdm_errmsg((void __user *)arg, modem);
		break;
#endif

	case TRIGGER_MODEM_FATAL:
		pr_info("%s: Trigger modem fatal!!\n", __func__);
		if(modem->ops && modem->ops->fatal_trigger_cb)
			modem->ops->fatal_trigger_cb(modem);
		break;

	case EFS_SYNC_DONE:
		pr_info("%s:%s efs sync is done\n", __func__, (atomic_read(&modem->final_efs_wait) ? " FINAL" : ""));
		atomic_set(&modem->final_efs_wait, 0);
		break;

	case NV_WRITE_DONE:
		pr_info("%s: NV write done!\n", __func__);
		if (modem->ops && modem->ops->nv_write_done_cb) {
			modem->ops->nv_write_done_cb(modem);
		}
		break;

	case EFS_SYNC_TIMEOUT:
		break;

	case POWER_OFF_CHARM:
		pr_info("%s: (HTC_POWER_OFF_CHARM)Powering off mdm\n", __func__);
		if (modem->ops && modem->ops->stop2) {
			modem->mdm_status &= (MDM_STATUS_POWER_DOWN | MDM_STATUS_RESET);
			if(modem->mdm_debug_on)
				pr_info("%s: modem->mdm_status=0x%x\n", __func__, modem->mdm_status);
			modem->ops->stop2(modem);
		}
		break;

	default:
		pr_err("%s: invalid ioctl cmd = %d\n", __func__, _IOC_NR(cmd));
		ret = -EINVAL;
		break;
	}

	if(modem->mdm_debug_on)
		pr_info("%s: Entering ioctl cmd = %d done.\n", __func__, _IOC_NR(cmd));

	mutex_unlock(&modem->lock);

	return ret;
}

#ifdef CONFIG_COMPAT
long mdm_modem_ioctl_compat(struct file *filp, unsigned int cmd, unsigned long arg)
{
        switch (cmd) {
        /*Todo: Check which commands are needed to covert for compatibiltiy*/
        case COMPAT_SET_MODEM_ERRMSG:
                cmd = SET_MODEM_ERRMSG;
                break;

        default:
                break;
        }

        return mdm_modem_ioctl(filp, cmd, (unsigned long) compat_ptr(arg));
}
#endif

static const struct file_operations mdm_modem_fops = {
	.owner = THIS_MODULE,
	.open = mdm_modem_open,
	.unlocked_ioctl = mdm_modem_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mdm_modem_ioctl_compat,
#endif
};

static struct miscdevice mdm_modem_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mdm",
	.fops = &mdm_modem_fops
};

static int mdm_debug_on_set(void *data, u64 val)
{
	struct device *dev;
	struct qcom_usb_modem *modem;

	dev = bus_find_device_by_name(&platform_bus_type, NULL,
					"MDM");
	if (!dev) {
		pr_warn("%s unable to find device name\n", __func__);
		goto done;
	}

	modem = dev_get_drvdata(dev);

	mutex_lock(&modem->lock);

	if (modem->ops && modem->ops->debug_state_changed_cb)
		modem->ops->debug_state_changed_cb(modem, val);

	mutex_unlock(&modem->lock);

done:
	return 0;
}

static int mdm_debug_on_get(void *data, u64 * val)
{
	struct device *dev;
	struct qcom_usb_modem *modem;

	dev = bus_find_device_by_name(&platform_bus_type, NULL,
					"MDM");
	if (!dev) {
		pr_warn("%s unable to find device name\n", __func__);
		goto done;
	}

	modem = dev_get_drvdata(dev);

	*val = modem->mdm_debug_on;

done:
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(mdm_debug_on_fops, mdm_debug_on_get, mdm_debug_on_set, "%llu\n");

static int mdm_debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("mdm_dbg", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debugfs_create_file("debug_on", 0644, dent, NULL, &mdm_debug_on_fops);
	return 0;
}

#ifdef CONFIG_MSM_SUBSYSTEM_RESTART
static int mdm_subsys_shutdown(const struct subsys_data *crashed_subsys)
{
	struct qcom_usb_modem *modem = crashed_subsys->modem;

	if(modem->mdm_debug_on)
		pr_info("[%s] start\n", __func__);

	gpio_direction_output(modem->pdata->ap2mdm_errfatal_gpio, 1);
	if (modem->pdata->ramdump_delay_ms > 0) {
		/* Wait for the external modem to complete
		 * its preparation for ramdumps.
		 */
		msleep(modem->pdata->ramdump_delay_ms);
	}

	mutex_lock(&modem->lock);

	//Power down mdm
	mdm_disable_irqs(modem, false);

	if((modem->mdm_status & MDM_STATUS_RESETTING) && get_enable_ramdumps())
	{
		pr_info("%s: Need to capture MDM RAM Dump, don't Pulling RESET gpio LOW here to prevent MDM memory data loss\n", __func__);
	}
	else
	{
		pr_info("%s: Pulling RESET gpio LOW\n", __func__);
		if(modem->ops && modem->ops->stop)
			modem->ops->stop(modem);
	}

	mutex_unlock(&modem->lock);

	/* Workaournd for real-time MDM ramdump druing subsystem restart */
	/* ap2mdm_errfatal_gpio should be pulled low otherwise MDM will assume 8K fatal after bootup */
	gpio_direction_output(modem->pdata->ap2mdm_errfatal_gpio, 0);

	if(modem->ops && modem->ops->dump_mdm_gpio_cb)
		modem->ops->dump_mdm_gpio_cb(modem, -1, "mdm_subsys_shutdown");

	if(modem->mdm_debug_on)
		pr_info("[%s] end\n", __func__);

	return 0;
}

static int mdm_subsys_powerup(const struct subsys_data *crashed_subsys)
{
	struct qcom_usb_modem *modem = crashed_subsys->modem;
	int mdm_boot_status = 0;

	if(modem->mdm_debug_on)
		pr_info("[%s] start\n", __func__);

	gpio_direction_output(modem->pdata->ap2mdm_errfatal_gpio, 0);
	gpio_direction_output(modem->pdata->ap2mdm_status_gpio, 1);

	mutex_lock(&modem->lock);
	modem->boot_type = CHARM_NORMAL_BOOT;
	mutex_unlock(&modem->lock);
	complete(&modem->mdm_needs_reload);
	if (!wait_for_completion_timeout(&modem->mdm_boot, msecs_to_jiffies(MDM_BOOT_TIMEOUT))) {
		mdm_boot_status = -ETIMEDOUT;
		pr_err("%s: mdm modem restart timed out.\n", __func__);
	} else {
		pr_info("%s: mdm modem has been restarted\n", __func__);

#ifdef CONFIG_MSM_SYSMON_COMM
		/* Log the reason for the restart */
		if(modem->mdm_restart_wq)
			queue_work_on(0, modem->mdm_restart_wq, &modem->mdm_restart_reason_work);
#endif
	}
	INIT_COMPLETION(modem->mdm_boot);

	if(modem->mdm_debug_on)
		pr_info("[%s] end (mdm_boot_status=%d)\n", __func__, mdm_boot_status);

	return mdm_boot_status;
}

static int mdm_subsys_ramdumps(int want_dumps, const struct subsys_data *crashed_subsys)
{
	struct qcom_usb_modem *modem = crashed_subsys->modem;
	int mdm_ram_dump_status = 0;

	if(modem->mdm_debug_on)
		pr_info("%s: want_dumps is %d\n", __func__, want_dumps);

	if (want_dumps) {
		mutex_lock(&modem->lock);
		modem->mdm_status |= MDM_STATUS_RAMDUMP;
		modem->boot_type = CHARM_RAM_DUMPS;
		complete(&modem->mdm_needs_reload);
		mutex_unlock(&modem->lock);

		wait_for_completion(&modem->mdm_ram_dumps);
		INIT_COMPLETION(modem->mdm_ram_dumps);
		gpio_direction_output(modem->pdata->ap2mdm_errfatal_gpio, 1);

		mutex_lock(&modem->lock);
		mdm_disable_irqs(modem, false);

		if((modem->mdm_status & MDM_STATUS_RESETTING) && get_enable_ramdumps())
		{
			pr_info("%s: Need to capture MDM RAM Dump, don't Pulling RESET gpio LOW here to prevent MDM memory data loss\n", __func__);
		}
		else
		{
			pr_info("%s: Pulling RESET gpio LOW\n", __func__);
			if(modem->ops && modem->ops->stop)
				modem->ops->stop(modem);
		}
		mutex_unlock(&modem->lock);

		/* Workaournd for real-time MDM ramdump druing subsystem restart */
		/* ap2mdm_errfatal_gpio should be pulled low otherwise MDM will assume 8K fatal after bootup */
		gpio_direction_output(modem->pdata->ap2mdm_errfatal_gpio, 0);

		if(modem->ops && modem->ops->dump_mdm_gpio_cb)
			modem->ops->dump_mdm_gpio_cb(modem, -1, "mdm_subsys_ramdumps");
	}
	return mdm_ram_dump_status;
}

static struct subsys_data mdm_subsystem = {
	.shutdown = mdm_subsys_shutdown,
	.ramdump = mdm_subsys_ramdumps,
	.powerup = mdm_subsys_powerup,
	.name = EXTERNAL_MODEM,
};
#endif

#ifdef CONFIG_MSM_SYSMON_COMM
#define RD_BUF_SIZE	100
#define SFR_MAX_RETRIES 10
#define SFR_RETRY_INTERVAL 1000

static void mdm_restart_reason_fn(struct work_struct *ws)
{
	struct qcom_usb_modem *modem = container_of(ws, struct qcom_usb_modem,
						     mdm_restart_reason_work);

	int ret, ntries = 0;
	char sfr_buf[RD_BUF_SIZE];

	do {
		msleep(SFR_RETRY_INTERVAL);
		ret = sysmon_get_reason(SYSMON_SS_EXT_MODEM, sfr_buf, sizeof(sfr_buf));
		if (ret) {
			/*
			 * The sysmon device may not have been probed as yet
			 * after the restart.
			 */
			pr_err("%s: Error retrieving mdm restart reason, ret = %d, " "%d/%d tries\n", __func__, ret, ntries + 1, SFR_MAX_RETRIES);
		} else {
			pr_err("mdm restart reason: %s\n", sfr_buf);
			mutex_lock(&modem->lock);
			modem->msr_info_list[modem->mdm_msr_index].valid = 1;
			modem->msr_info_list[modem->mdm_msr_index].msr_time = current_kernel_time();
			snprintf(modem->msr_info_list[modem->mdm_msr_index].modem_errmsg, RD_BUF_SIZE, "%s", sfr_buf);
			if (++modem->mdm_msr_index >= MODEM_ERRMSG_LIST_LEN) {
				modem->mdm_msr_index = 0;
			}
			mutex_unlock(&modem->lock);
			break;
		}
	} while (++ntries < SFR_MAX_RETRIES);
}
#endif

static int mdm_init(struct qcom_usb_modem *modem, struct platform_device *pdev)
{
	struct qcom_usb_modem_power_platform_data *pdata =
	    pdev->dev.platform_data;
	int ret = 0;

	pr_info("%s\n", __func__);

	modem->pdata = pdata;
	modem->pdev = pdev;

	/* get modem operations from platform data */
	modem->ops = (const struct qcom_modem_operations *)pdata->ops;

	mutex_init(&(modem->lock));
	mutex_init(&modem->hc_lock);
#ifdef CONFIG_MDM_FTRACE_DEBUG
	mutex_init(&modem->ftrace_cmd_lock);
#endif
	wake_lock_init(&modem->wake_lock, WAKE_LOCK_SUSPEND, "mdm_lock");
	if (pdev->id >= 0)
		dev_set_name(&pdev->dev, "MDM%d", pdev->id);
	else
		dev_set_name(&pdev->dev, "MDM");

	INIT_WORK(&modem->host_reset_work, tegra_usb_host_reset);
	INIT_WORK(&modem->host_load_work, tegra_usb_host_load);
	INIT_WORK(&modem->host_unload_work, tegra_usb_host_unload);
	INIT_WORK(&modem->cpu_boost_work, cpu_freq_boost);
	INIT_DELAYED_WORK(&modem->cpu_unboost_work, cpu_freq_unboost);
	INIT_WORK(&modem->mdm_hsic_ready_work, mdm_hsic_ready);
	INIT_WORK(&modem->mdm_status_work, mdm_status_changed);
	INIT_WORK(&modem->mdm_errfatal_work, mdm_fatal);
#ifdef CONFIG_MDM_FTRACE_DEBUG
	INIT_WORK(&modem->ftrace_enable_log_work, ftrace_enable_basic_log_fn);
#endif
#ifdef CONFIG_MSM_SYSMON_COMM
	INIT_WORK(&modem->mdm_restart_reason_work, mdm_restart_reason_fn);
#endif
	pm_qos_add_request(&modem->cpu_boost_req, PM_QOS_CPU_FREQ_MIN,
			   PM_QOS_DEFAULT_VALUE);

	modem->pm_notifier.notifier_call = mdm_pm_notifier;
	modem->usb_notifier.notifier_call = mdm_usb_notifier;

	usb_register_notify(&modem->usb_notifier);
	register_pm_notifier(&modem->pm_notifier);

	mdm_loaded_info(modem);
	if (modem->ops && modem->ops->debug_state_changed_cb)
		modem->ops->debug_state_changed_cb(modem, (get_radio_flag() & RADIO_FLAG_MORE_LOG)?1:0);

#ifdef CONFIG_MDM_FTRACE_DEBUG
	if(get_radio_flag() & RADIO_FLAG_FTRACE_ENABLE)
		modem->ftrace_enable = true;
	else
		modem->ftrace_enable = false;

	/* Initialize completion */
	modem->ftrace_cmd_pending = COMPLETION_INITIALIZER_ONSTACK(modem->ftrace_cmd_pending);
	modem->ftrace_cmd_can_be_executed = COMPLETION_INITIALIZER_ONSTACK(modem->ftrace_cmd_can_be_executed);
#endif

	/* Register kernel panic notification */
	atomic_notifier_chain_register(&panic_notifier_list, &mdm_panic_blk);
	mdm_debugfs_init();

#ifdef CONFIG_MSM_SUBSYSTEM_RESTART
	/* Register subsystem handlers */
	mdm_subsystem.modem = modem;
	ssr_register_subsystem(&mdm_subsystem);

	/* Initialize completion */
	modem->mdm_needs_reload = COMPLETION_INITIALIZER_ONSTACK(modem->mdm_needs_reload);
	modem->mdm_boot = COMPLETION_INITIALIZER_ONSTACK(modem->mdm_boot);
	modem->mdm_ram_dumps = COMPLETION_INITIALIZER_ONSTACK(modem->mdm_ram_dumps);
#endif

	/* Initialize other variable */
	modem->boot_type = CHARM_NORMAL_BOOT;
	modem->mdm_status = MDM_STATUS_POWER_DOWN;
	modem->mdm_wake_irq_enabled = false;
	modem->mdm9k_status = 0;
	modem->usb_host_reset_done = COMPLETION_INITIALIZER_ONSTACK(modem->usb_host_reset_done);
	atomic_set(&modem->final_efs_wait, 0);
	modem->mdm2ap_ipc3_status = gpio_get_value(modem->pdata->mdm2ap_ipc3_gpio);

	/* hsic wakeup */
	modem->mdm_hsic_phy_resume_jiffies = 0;
	modem->mdm_hsic_phy_active_total_ms = 0;

#ifdef CONFIG_MDM_FTRACE_DEBUG
	memset(modem->ftrace_cmd, sizeof(modem->ftrace_cmd), 0);
#endif
#ifdef CONFIG_MSM_SYSMON_COMM
	memset(modem->msr_info_list, sizeof(modem->msr_info_list), 0);
	modem->mdm_msr_index = 0;
#endif

	/* create work queue platform_driver_register */
	modem->usb_host_wq = create_singlethread_workqueue("usb_host_queue");
	if(!modem->usb_host_wq)
		goto error;

	modem->wq = create_singlethread_workqueue("qcom_usb_mdm_queue");
	if(!modem->wq)
		goto error;

	modem->mdm_recovery_wq= create_singlethread_workqueue("mdm_recovery_queue");
	if(!modem->mdm_recovery_wq)
		goto error;

#ifdef CONFIG_MDM_FTRACE_DEBUG
	modem->ftrace_wq = create_singlethread_workqueue("qcom_usb_mdm_ftrace_queue");
	if(!modem->ftrace_wq)
		goto error;
#endif
#ifdef CONFIG_MSM_SYSMON_COMM
	modem->mdm_restart_wq = alloc_workqueue("qcom_usb_mdm_restart_queue", 0, 0);
	if(!modem->mdm_restart_wq)
		goto error;
#endif

	/* modem init to request gpio settings */
	if (modem->ops && modem->ops->init) {
		ret = modem->ops->init(modem);
		if (ret)
			goto error;
	}

	/* Request IRQ */
	/* if wake gpio is not specified we rely on native usb remote wake */
	if (gpio_is_valid(pdata->mdm2ap_wakeup_gpio)) {
		/* request remote wakeup irq from platform data */
		ret = mdm_request_irq(modem,
				      qcom_usb_modem_wake_thread,
				      pdata->mdm2ap_wakeup_gpio,
				      pdata->wake_irq_flags,
				      "MDM2AP_WAKEUP",
				      &modem->wake_irq,
				      &modem->wake_irq_wakeable);
		if (ret) {
			dev_err(&pdev->dev, "request wake irq error\n");
			goto error;
		}
	}

	/* Register hsic usb ops */
	if(modem->mdm_debug_on)
		modem->pdata->tegra_ehci_pdata->ops =
						&qcom_usb_modem_debug_remote_wakeup_ops;
	else
		modem->pdata->tegra_ehci_pdata->ops =
						&qcom_usb_modem_remote_wakeup_ops;

	if (gpio_is_valid(pdata->mdm2ap_hsic_ready_gpio)) {
		/* request hsic ready irq from platform data */
		ret = mdm_request_irq(modem,
				      qcom_usb_modem_hsic_ready_thread,
				      pdata->mdm2ap_hsic_ready_gpio,
				      pdata->hsic_ready_irq_flags,
				      "MDM2AP_HSIC_READY",
				      &modem->hsic_ready_irq,
				      &modem->hsic_ready_irq_wakeable);
		if (ret) {
			dev_err(&pdev->dev, "request hsic ready irq error\n");
			goto error;
		}
	}

	if (gpio_is_valid(pdata->mdm2ap_status_gpio)) {
		/* request status irq from platform data */
		ret = mdm_request_irq(modem,
				      qcom_usb_modem_status_thread,
				      pdata->mdm2ap_status_gpio,
				      pdata->status_irq_flags,
				      "MDM2AP_STATUS",
				      &modem->status_irq,
				      &modem->status_irq_wakeable);
		if (ret) {
			dev_err(&pdev->dev, "request status irq error\n");
			goto error;
		}
	}

	if (gpio_is_valid(pdata->mdm2ap_errfatal_gpio)) {
		/* request error fatal irq from platform data */
		ret = mdm_request_irq(modem,
				      qcom_usb_modem_errfatal_thread,
				      pdata->mdm2ap_errfatal_gpio,
				      pdata->errfatal_irq_flags,
				      "MDM2AP_ERRFATAL",
				      &modem->errfatal_irq,
				      &modem->errfatal_irq_wakeable);
		if (ret) {
			dev_err(&pdev->dev, "request errfatal irq error\n");
			goto error;
		}
	}

	if (gpio_is_valid(pdata->mdm2ap_ipc3_gpio)) {
		/* request ipc3 irq from platform data */
		ret = mdm_request_irq(modem,
				      qcom_usb_modem_ipc3_thread,
				      pdata->mdm2ap_ipc3_gpio,
				      pdata->ipc3_irq_flags,
				      "MDM2AP_IPC3",
				      &modem->ipc3_irq,
				      &modem->ipc3_irq_wakeable);
		if (ret) {
			dev_err(&pdev->dev, "request ipc3 irq error\n");
			goto error;
		}
	}

	if (gpio_is_valid(pdata->mdm2ap_vdd_min_gpio)) {
		/* request vdd min irq from platform data */
		ret = mdm_request_irq(modem,
				      qcom_usb_modem_vdd_min_thread,
				      pdata->mdm2ap_vdd_min_gpio,
				      pdata->vdd_min_irq_flags,
				      "mdm_vdd_min",
				      &modem->vdd_min_irq,
				      &modem->vdd_min_irq_wakeable);
		if (ret) {
			dev_err(&pdev->dev, "request vdd min irq error\n");
			goto error;
		}
	}

	/* Default force to disable irq */
	modem->mdm_wake_irq_enabled = true;
	mdm_disable_irqs(modem, true);
	modem->mdm_irq_enabled = true;
	mdm_disable_irqs(modem, false);

	/* Reigster misc mdm driver */
	pr_info("%s: Registering mdm modem\n", __func__);
	ret = misc_register(&mdm_modem_misc);
	if(ret) {
			dev_err(&pdev->dev, "register mdm_modem_misc driver fail.\n");
			goto error;
	}

	return ret;
error:

	mdm_unloaded_info(modem);

	unregister_pm_notifier(&modem->pm_notifier);
	usb_unregister_notify(&modem->usb_notifier);

	cancel_work_sync(&modem->host_reset_work);
	cancel_work_sync(&modem->host_load_work);
	cancel_work_sync(&modem->host_unload_work);
	cancel_work_sync(&modem->cpu_boost_work);
	cancel_delayed_work_sync(&modem->cpu_unboost_work);
	cancel_work_sync(&modem->mdm_hsic_ready_work);
	cancel_work_sync(&modem->mdm_status_work);
	cancel_work_sync(&modem->mdm_errfatal_work);
#ifdef CONFIG_MDM_FTRACE_DEBUG
	cancel_work_sync(&modem->ftrace_enable_log_work);
#endif
#ifdef CONFIG_MSM_SYSMON_COMM
	cancel_work_sync(&modem->mdm_restart_reason_work);
#endif

	if(modem->usb_host_wq)
		destroy_workqueue(modem->usb_host_wq);
	if(modem->wq)
		destroy_workqueue(modem->wq);
	if(modem->mdm_recovery_wq)
		destroy_workqueue(modem->mdm_recovery_wq);
#ifdef CONFIG_MDM_FTRACE_DEBUG
	if(modem->ftrace_wq)
		destroy_workqueue(modem->ftrace_wq);
#endif
#ifdef CONFIG_MSM_SYSMON_COMM
	if(modem->mdm_restart_wq)
		destroy_workqueue(modem->mdm_restart_wq);
#endif

	pm_qos_remove_request(&modem->cpu_boost_req);

	if (modem->wake_irq)
		free_irq(modem->wake_irq, modem);
	if (modem->hsic_ready_irq)
		free_irq(modem->hsic_ready_irq, modem);
	if (modem->status_irq)
		free_irq(modem->status_irq, modem);
	if (modem->errfatal_irq)
		free_irq(modem->errfatal_irq, modem);
	if (modem->ipc3_irq)
		free_irq(modem->ipc3_irq, modem);
	if (modem->vdd_min_irq)
		free_irq(modem->vdd_min_irq, modem);

	return ret;
}

static int qcom_usb_modem_probe(struct platform_device *pdev)
{
	struct qcom_usb_modem_power_platform_data *pdata =
	    pdev->dev.platform_data;
	struct qcom_usb_modem *modem;
	int ret = 0;

#ifdef CONFIG_MDM_POWEROFF_MODEM_IN_OFFMODE_CHARGING
	int mfg_mode = BOARD_MFG_MODE_NORMAL;

	/* Check offmode charging, don't load mdm2 driver if device in offmode charging */
	mfg_mode = board_mfg_mode();
	if (mfg_mode == BOARD_MFG_MODE_OFFMODE_CHARGING) {
			/* TODO: pull AP2MDM_PMIC_RESET_N to output low to save power */
			pr_info("%s: BOARD_MFG_MODE_OFFMODE_CHARGING\n", __func__);

			return 0;
	} else {
		pr_info("%s: mfg_mode=[%d]\n", __func__, mfg_mode);
	}
#else
	pr_info("%s: CONFIG_MDM_POWEROFF_MODEM_IN_OFFMODE_CHARGING not set\n", __func__);
#endif

	if (!pdata) {
		dev_dbg(&pdev->dev, "platform_data not available\n");
		return -EINVAL;
	}

	modem = kzalloc(sizeof(struct qcom_usb_modem), GFP_KERNEL);
	if (!modem) {
		dev_dbg(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	ret = mdm_init(modem, pdev);
	if (ret) {
		kfree(modem);
		return ret;
	}

	dev_set_drvdata(&pdev->dev, modem);

	return ret;
}

static int __exit qcom_usb_modem_remove(struct platform_device *pdev)
{
	struct qcom_usb_modem *modem = platform_get_drvdata(pdev);

#ifdef CONFIG_MDM_POWEROFF_MODEM_IN_OFFMODE_CHARGING
	/* Check offmode charging, don't load mdm2 driver if device in offmode charging */
	int mfg_mode = BOARD_MFG_MODE_NORMAL;
	mfg_mode = board_mfg_mode();
	if (mfg_mode == BOARD_MFG_MODE_OFFMODE_CHARGING) {
		/* TODO: To free AP2MDM_PMIC_RESET_N gpio */
		pr_info("%s: BOARD_MFG_MODE_OFFMODE_CHARGING\n", __func__);

		return 0;
	}
#endif

	misc_deregister(&mdm_modem_misc);

	mdm_unloaded_info(modem);

	unregister_pm_notifier(&modem->pm_notifier);
	usb_unregister_notify(&modem->usb_notifier);

	if (modem->wake_irq)
		free_irq(modem->wake_irq, modem);
	if (modem->errfatal_irq)
		free_irq(modem->errfatal_irq, modem);
	if (modem->hsic_ready_irq)
		free_irq(modem->hsic_ready_irq, modem);
	if (modem->status_irq)
		free_irq(modem->status_irq, modem);
	if (modem->ipc3_irq)
		free_irq(modem->ipc3_irq, modem);
	if (modem->vdd_min_irq)
		free_irq(modem->vdd_min_irq, modem);

	if(modem->ops && modem->ops->remove)
		modem->ops->remove(modem);

	cancel_work_sync(&modem->host_reset_work);
	cancel_work_sync(&modem->host_load_work);
	cancel_work_sync(&modem->host_unload_work);
	cancel_work_sync(&modem->cpu_boost_work);
	cancel_delayed_work_sync(&modem->cpu_unboost_work);
	cancel_work_sync(&modem->mdm_hsic_ready_work);
	cancel_work_sync(&modem->mdm_status_work);
	cancel_work_sync(&modem->mdm_errfatal_work);
#ifdef CONFIG_MDM_FTRACE_DEBUG
	cancel_work_sync(&modem->ftrace_enable_log_work);
#endif
#ifdef CONFIG_MSM_SYSMON_COMM
	cancel_work_sync(&modem->mdm_restart_reason_work);
#endif

	if(modem->usb_host_wq)
		destroy_workqueue(modem->usb_host_wq);
	if(modem->wq)
		destroy_workqueue(modem->wq);
	if(modem->mdm_recovery_wq)
		destroy_workqueue(modem->mdm_recovery_wq);
#ifdef CONFIG_MDM_FTRACE_DEBUG
	if(modem->ftrace_wq)
		destroy_workqueue(modem->ftrace_wq);
#endif
#ifdef CONFIG_MSM_SYSMON_COMM
	if(modem->mdm_restart_wq)
		destroy_workqueue(modem->mdm_restart_wq);
#endif
	pm_qos_remove_request(&modem->cpu_boost_req);

	kfree(modem);
	return 0;
}

static void qcom_usb_modem_shutdown(struct platform_device *pdev)
{
	struct qcom_usb_modem *modem = platform_get_drvdata(pdev);

	if(modem->mdm_debug_on)
		pr_info("%s: setting AP2MDM_STATUS low for a graceful restart\n", __func__);

	mutex_lock(&modem->lock);
	mdm_disable_irqs(modem, false);
	mdm_disable_irqs(modem, true);

	modem->mdm_status = MDM_STATUS_POWER_DOWN;
	if(modem->mdm_debug_on)
		pr_info("%s: modem->mdm_status=0x%x\n", __func__, modem->mdm_status);
	mutex_unlock(&modem->lock);

	atomic_set(&modem->final_efs_wait, 1);

	if(gpio_is_valid(modem->pdata->ap2mdm_status_gpio))
		gpio_set_value(modem->pdata->ap2mdm_status_gpio, 0);

	if (gpio_is_valid(modem->pdata->ap2mdm_wakeup_gpio))
		gpio_set_value(modem->pdata->ap2mdm_wakeup_gpio, 1);

	if (modem->ops && modem->ops->stop)
		modem->ops->stop(modem);

	if (gpio_is_valid(modem->pdata->ap2mdm_wakeup_gpio))
		gpio_set_value(modem->pdata->ap2mdm_wakeup_gpio, 0);

	return;
}

#ifdef CONFIG_PM
static int qcom_usb_modem_suspend(struct platform_device *pdev,
				   pm_message_t state)
{
	struct qcom_usb_modem *modem = platform_get_drvdata(pdev);

	if(modem->mdm_debug_on)
		pr_info("%s\n", __func__);

	/* send L3 hint to modem */
	if (modem->ops && modem->ops->suspend)
		modem->ops->suspend();

	return 0;
}

static int qcom_usb_modem_resume(struct platform_device *pdev)
{
	struct qcom_usb_modem *modem = platform_get_drvdata(pdev);

	if(modem->mdm_debug_on)
		pr_info("%s\n", __func__);

	/* send L3->L0 hint to modem */
	if (modem->ops && modem->ops->resume)
		modem->ops->resume();

	return 0;
}
#endif

static struct platform_driver qcom_usb_modem_power_driver = {
	.driver = {
		   .name = "qcom_usb_modem_power",
		   .owner = THIS_MODULE,
		   },
	.probe = qcom_usb_modem_probe,
	.remove = __exit_p(qcom_usb_modem_remove),
	.shutdown = qcom_usb_modem_shutdown,
#ifdef CONFIG_PM
	.suspend = qcom_usb_modem_suspend,
	.resume = qcom_usb_modem_resume,
#endif
};

static int __init qcom_usb_modem_power_init(void)
{
	return platform_driver_register(&qcom_usb_modem_power_driver);
}

module_init(qcom_usb_modem_power_init);

static void __exit qcom_usb_modem_power_exit(void)
{
	platform_driver_unregister(&qcom_usb_modem_power_driver);
}

module_exit(qcom_usb_modem_power_exit);

MODULE_DESCRIPTION("Qualcomm usb modem power management driver");
MODULE_LICENSE("GPL");
