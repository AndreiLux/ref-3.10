/*
 * arch/arm/mach-tegra/baseband-xmm-power.c
 *
 * Copyright (C) 2011 NVIDIA Corporation
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/spinlock.h>
#include <linux/usb.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/reboot.h>

#include <linux/platform_data/gpio-odin.h>
#include <linux/mdm_ctrl.h>
#include <linux/platform_data/odin_tz.h>

#include "mdm_util.h"
#include "mcd_pm.h"

MODULE_LICENSE("GPL");

#if defined(CONFIG_ODIN_DWC_USB_HOST)
extern int odin_dwc_host_set_idle(int set);
extern void odin_dwc_host_set_force_idle(int set);
#else
#define odin_dwc_host_set_idle(...) \
	do {} while (false);printk("do nothing. set IDLE\n");
#define odin_dwc_host_set_force_idle(int set) \
	do {} while (false);printk("do nothing. set Force IDLE\n");
#endif

#define WAKE_LOCK_TIMEOUT 500
#define IS_POWERSTATE_INIT(state) \
	(((state == BBXMM_PS_UNINIT) || (state == BBXMM_PS_INIT)) ? true : false)

static bool rt_state = false;
#define SET_RUNTIME_STATE(x)	(rt_state = x)
static bool need_autosuspend = false;
static bool system_suspend_state = false;

/* Is current flow executing in runtime pm or system pm?
 * true : Runtime PM flow.
 * false : System PM flow.
 */
#define RUNTIME_EXEC(udev)\
	(rt_state)

const char *pwrstate_cmt[] = {
	"BBXMM_PS_UNINIT",
	"BBXMM_PS_INIT",
	"BBXMM_PS_L0",
	"BBXMM_PS_L0TOL2",
	"BBXMM_PS_L0_AWAKE",
	"BBXMM_PS_L2",
	"BBXMM_PS_L2TOL0_FROM_AP",
	"BBXMM_PS_L2_AWAKE",
	"BBXMM_PS_L3",
	"BBXMM_PS_L3TOL0_FROM_CP",
	"BBXMM_PS_L3_WAKEREADY",
	"BBXMM_PS_L3_CP_WAKEUP",
};

/* To handle corner case of HOST_WAKEUP */
#define PENDING_HW

static struct usb_device_id xmm_pm_ids[] = {
	{ USB_DEVICE(VENDOR_ID, PRODUCT_ID),
	.driver_info = 0 },
	{}
};

static struct gpio tegra_baseband_gpios[] = {
	{ -1, GPIOF_OUT_INIT_HIGH,	"BB_PWR_DOWN"   },
	{ -1, GPIOF_OUT_INIT_LOW,	"BB_ON"   },
	{ -1, GPIOF_OUT_INIT_LOW,	"BB_RST" },
	{ -1, GPIOF_OUT_INIT_LOW,	"IPC_BB_WAKE" },
	{ -1, GPIOF_IN,				"IPC_AP_WAKE" },
	{ -1, GPIOF_OUT_INIT_HIGH,	"IPC_HSIC_ACTIVE" },
    { -1, GPIOF_OUT_INIT_LOW,   "BB_FORCE_CRASH" },
	{ -1, GPIOF_IN,				"IPC_CP_RST_OUT" },
	{ -1, GPIOF_IN,				"IPC_CP_CORE_DUMP" },
};

static enum {
	IPC_AP_WAKE_UNINIT,
	IPC_AP_WAKE_IRQ_READY,
	IPC_AP_WAKE_INIT1,
	IPC_AP_WAKE_INIT2,
	IPC_AP_WAKE_L,
	IPC_AP_WAKE_H,
} ipc_ap_wake_state;

enum baseband_xmm_powerstate_t baseband_xmm_powerstate;

static struct workqueue_struct *workqueue;
static struct workqueue_struct *workqueue_susp;

static struct work_struct L3_resume_work;
static struct work_struct host_wakeup_isr;

static struct baseband_power_platform_data *baseband_power_driver_data;
static struct usb_device *usbdev;

/* spin lock variables */
static spinlock_t xmm_lock;

/* Wake lock variables */
static int xmm_wake_lock_counter = 0;
static struct wake_lock wakelock;

/* Runtime suspend variables */
struct usb_interface *intf;

/* hsic state notifier */
struct imc_hsic_ops * hsic_notifier;
static bool l2_resume_work_done = false;

extern bool no_host_wakeup;
extern int host_wakeup_proc(int irq, void *data);
static enum shutdown_state_t mcd_pm_ap_shutdown = SYSTEM_WAKEUP;

#if defined(PENDING_HW)
#define PEND_REG_BASE (0x200f9080)
#define MASK_REG_BASE (0x200f9090)
void mcd_pm_disable_HW_irq(void) {
#if 0
	u32 read_conf_debounce;
	u32 read_mask_value;
#endif

	odin_gpio_smsgic_irq_mask(
		gpio_to_irq(baseband_power_driver_data->modem.xmm.ipc_ap_wake));

	/* Check actually to be disabled */
#if 0
	read_mask_value = tz_read(MASK_REG_BASE);
	if (read_mask_value & (0x1 << 6)) {
	    pr_info("EINT6 is masked (DISABLED)\n");
	} else {
		pr_info("EINT6 is unmasked (ENABLED)\n");
	}
#endif

	pr_info("mask HostW INT.\n");
}
EXPORT_SYMBOL(mcd_pm_disable_HW_irq);

void mcd_pm_enable_hw_irq(void) {
	pr_info("unmask HostW INT.\n");
	odin_gpio_smsgic_irq_unmask(
		gpio_to_irq(baseband_power_driver_data->modem.xmm.ipc_ap_wake));
}
EXPORT_SYMBOL(mcd_pm_enable_hw_irq);

bool check_pending_hw_int()
{
	u32 read_value;
	read_value = tz_read(PEND_REG_BASE);
	if (read_value & (0x1 << 6)) {
		pr_info("There was a HOST_WAKEUP interrupt during L2->L3.\n");
		return true;
	}
	return false;
}
EXPORT_SYMBOL(check_pending_hw_int);
#endif

#if defined(DEBUG_PM_TIME)
ktime_t starttime;
#define RESET_START_TIME() memset(&starttime, 0, sizeof(union ktime))

static void SET_START_TIME(void)
{
	if (starttime.tv64 == 0) {
		starttime = ktime_get();

		printk("PM_TIME: SET_START_TIME.(%llx)\n",
			(long long)ktime_to_ns(starttime));
	}
}

/* Estimate the execution for PM */
static void mcd_pm_show_time(ktime_t starttime, char *info)
{
	ktime_t calltime;
	u64 usecs64;
	int usecs;

	calltime = ktime_get();
	usecs64 = ktime_to_ns(ktime_sub(calltime, starttime));
	do_div(usecs64, NSEC_PER_USEC);
	usecs = usecs64;
	if (usecs == 0)
		usecs = 1;
	printk("PM_TIME: %s complete after %ld.%03ld msecs\n",
		info, usecs / USEC_PER_MSEC, usecs % USEC_PER_MSEC);
}
#else
#define SET_START_TIME(...) do {} while (false)
#define mcd_pm_show_time(...) do {} while (false)
#endif

int register_hsic_notifier(struct imc_hsic_ops * ops) {
	int rtn_val = 0;
	if (ops)
		hsic_notifier = ops;
	else
		rtn_val = -EINVAL;
	return rtn_val;
}

void mcd_pm_runtime_enable(bool enable) {
	if (usbdev) {
		if (enable) {
			usb_enable_autosuspend(usbdev);
			pr_debug("ENABLE autosuspend.\n");
		} else {
			usb_disable_autosuspend(usbdev);
			pr_debug("DISABLE autosuspend.\n");
		}
	}
}
EXPORT_SYMBOL(mcd_pm_runtime_enable);

static void mcd_pm_save_power_state(int state) {
	pr_info("transition: %s => %s.\n",
		pwrstate_cmt[baseband_xmm_powerstate], pwrstate_cmt[state]);

	spin_lock(&xmm_lock);
	baseband_xmm_powerstate = state;
	spin_unlock(&xmm_lock);
}

static void mcd_pm_set_host_active(
					struct baseband_power_platform_data *data,
					bool value)
{
	pr_info("GPIO HOST_ACTIVE set to %s.\n", value ? "HIGH" : "LOW");
	gpio_set_value(data->modem.xmm.ipc_hsic_active, value);
	data->host_active = value;
}


static void mcd_pm_set_slave_wakeup(
					struct baseband_power_platform_data *data,
					bool value)
{
	if (data->slave_wakeup == value) {
		return;
	}

	pr_info("GPIO SLAVE_WAKEUP set to %s.\n", value ? "HIGH" : "LOW");
	gpio_set_value(data->modem.xmm.ipc_bb_wake, value);
	data->slave_wakeup = value;
}

#define FEATURE_RECOVERY_SCHEME
#ifdef FEATURE_RECOVERY_SCHEME
extern int enable_recovery;
extern void force_mdm_ctrl_cold_reset_request();
#endif

void mcd_pm_set_force_crash(bool value)
{
	u32 read_value;
	int hw_value;

#if defined(PENDING_HW)
	/* Check a pending interrupt on GIC */
	read_value = tz_read(PEND_REG_BASE);
	if (read_value & (0x1 << 6)) {
		pr_err("%s: GIC detected the HOST_WAKEUP. \n", __func__);
	} else {
		pr_err("%s: GIC did not detect the HOST_WAKEUP. \n", __func__);
	}
#endif

#ifdef FEATURE_RECOVERY_SCHEME
	if(enable_recovery){
	    force_mdm_ctrl_cold_reset_request();
	}
	else{
	    /* Check current Host_wakeup level */
		hw_value = gpio_get_value(baseband_power_driver_data->modem.xmm.ipc_ap_wake);
		printk("%s: HOST_WAKEUP is %s.\n",
			__func__, hw_value ? "HIGH" : "LOW");
		if(baseband_power_driver_data == NULL){
		     pr_debug("Platform data is NULL. Modem crash by GPIO is failed!!\n");
		     return;
		}

		gpio_set_value(tegra_baseband_gpios[6].gpio, value);
		printk ("%s] tegra_baseband_gpios[6].gpio = %s (1st)\n",__func__, gpio_get_value(tegra_baseband_gpios[6].gpio)? "HIGH": "LOW");
		msleep(100);
		gpio_set_value(tegra_baseband_gpios[6].gpio, 0);
		printk ("%s] tegra_baseband_gpios[6].gpio = %s (1st)\n",__func__, gpio_get_value(tegra_baseband_gpios[6].gpio)? "HIGH": "LOW");
		msleep(100);
		gpio_set_value(tegra_baseband_gpios[6].gpio, value);
		printk ("%s] tegra_baseband_gpios[6].gpio = %s (2nd)\n",__func__, gpio_get_value(tegra_baseband_gpios[6].gpio)? "HIGH": "LOW");
		msleep(100);
		gpio_set_value(tegra_baseband_gpios[6].gpio, 0);
		printk ("%s] tegra_baseband_gpios[6].gpio = %s (2nd)\n",__func__, gpio_get_value(tegra_baseband_gpios[6].gpio)? "HIGH": "LOW");
    }
#else
	/* Check current Host_wakeup level */
	hw_value = gpio_get_value(baseband_power_driver_data->modem.xmm.ipc_ap_wake);
	printk("%s: HOST_WAKEUP is %s.\n",
		__func__, hw_value ? "HIGH" : "LOW");

    if(baseband_power_driver_data == NULL){
        pr_debug("Platform data is NULL. Modem crash by GPIO is failed!!\n");
        return;
    }

	gpio_set_value(tegra_baseband_gpios[6].gpio, value);
	printk ("%s] tegra_baseband_gpios[6].gpio = %s (1st)\n",__func__, gpio_get_value(tegra_baseband_gpios[6].gpio)? "HIGH": "LOW");
	msleep(100);
	gpio_set_value(tegra_baseband_gpios[6].gpio, 0);
	printk ("%s] tegra_baseband_gpios[6].gpio = %s (1st)\n",__func__, gpio_get_value(tegra_baseband_gpios[6].gpio)? "HIGH": "LOW");
	msleep(100);
	gpio_set_value(tegra_baseband_gpios[6].gpio, value);
	printk ("%s] tegra_baseband_gpios[6].gpio = %s (2nd)\n",__func__, gpio_get_value(tegra_baseband_gpios[6].gpio)? "HIGH": "LOW");
	msleep(100);
	gpio_set_value(tegra_baseband_gpios[6].gpio, 0);
	printk ("%s] tegra_baseband_gpios[6].gpio = %s (2nd)\n",__func__, gpio_get_value(tegra_baseband_gpios[6].gpio)? "HIGH": "LOW");
#endif
    return;
}
EXPORT_SYMBOL(mcd_pm_set_force_crash);

void mcd_pm_force_set_host_active(bool value)
{
	mcd_pm_set_host_active(baseband_power_driver_data, value);
}
EXPORT_SYMBOL(mcd_pm_force_set_host_active);

int mcd_pm_wake_lock(void)
{
	if (!xmm_wake_lock_counter) {
		xmm_wake_lock_counter++;
		wake_lock(&wakelock);
	}

	return xmm_wake_lock_counter;
}
EXPORT_SYMBOL(mcd_pm_wake_lock);

static int mcd_pm_wake_unlock(void)
{
	if (xmm_wake_lock_counter) {
		wake_unlock(&wakelock);
		xmm_wake_lock_counter--;
		wake_lock_timeout(&wakelock, msecs_to_jiffies(WAKE_LOCK_TIMEOUT));
	}

	return xmm_wake_lock_counter;
}

int mcd_pm_get_ap_shutdown_state(void)
{
	return mcd_pm_ap_shutdown;
}
EXPORT_SYMBOL(mcd_pm_get_ap_shutdown_state);

void mcd_pm_set_ap_shutdown_state(int statue)
{
	mcd_pm_ap_shutdown = statue;
}
EXPORT_SYMBOL(mcd_pm_set_ap_shutdown_state);

static void mcd_pm_reset_autosuspend(struct usb_device *udev)
{
	int rtn_val = 0;

	pr_debug("%s: \n", __func__);

	if (!udev) {
		pr_err("%s: udev is NULL !! \n", __func__);
		return;
	}

	if (intf == NULL) {
		pr_err("%s: udev's intf is NULL !! \n", __func__);
		return;
	}

	rtn_val = usb_autopm_get_interface_async(intf);		//for memory corruption issue
	if (rtn_val == 0) {
		usb_autopm_put_interface_async(intf);
	} else {
		pr_err("\n\n%s: do not exec get_interface.\n", __func__);
	}
}

int mcd_pm_ps_L0_proc(void) {
	int value = baseband_power_driver_data->slave_wakeup;

	pr_debug("%s: start.\n", __func__);

	if (IS_POWERSTATE_INIT(baseband_xmm_powerstate)) {
		pr_err("%s: Device is init state.\n",__func__);
		return 0;
	}

	if (value) {
		/* Clear the slave wakeup request */
		mcd_pm_set_slave_wakeup(baseband_power_driver_data, 0);
	}

	if ((usbdev == NULL) || (&usbdev->dev == NULL)) {
		pr_debug("%s: usbdev is NULL.\n\n", __func__);
	} else {
		if (need_autosuspend == true) {
			pr_debug("%s: Change default autosuspend.\n\n", __func__);

			pm_runtime_set_autosuspend_delay(&usbdev->dev,
				DEFAULT_AUTOSUSPEND_DELAY);
			need_autosuspend = false;
		}
	}

#if defined(DEBUG_PM_TIME)
	if ((baseband_xmm_powerstate == BBXMM_PS_L0) && !RUNTIME_EXEC(usbdev)) {
		mcd_pm_show_time(starttime, "L3->L0 resume time");
		RESET_START_TIME();
	}
#endif

	pr_debug("%s: end.\n", __func__);
	return 0;
}


int mcd_pm_ps_L0AWAKE_proc(struct baseband_power_platform_data * data) {
	pr_debug("%s:.\n", __func__);
	l2_resume_work_done = false;
	mcd_pm_reset_autosuspend(usbdev);
	l2_resume_work_done = true;
	return 0;
}

int mcd_pm_ps_L2AWAKE_proc(struct baseband_power_platform_data * data) {
	pr_debug("%s:.\n", __func__);
	l2_resume_work_done = false;
	mcd_pm_reset_autosuspend(usbdev);
	l2_resume_work_done = true;
	return 0;
}

int mcd_pm_ps_L2TOL0_from_ap_proc(struct baseband_power_platform_data * data) {
	pr_debug("%s:.\n", __func__);
	if (data->host_wakeup_low) {
		pr_debug("%s: Ignore slave wakeup handling.\n", __func__);
		return 0;
	}
	mcd_pm_set_slave_wakeup(data, 1);
	return 0;
}

int mcd_pm_ps_L3TOL0_from_cp_proc(struct baseband_power_platform_data * data) {
	pr_debug("%s:.\n", __func__);
	mcd_pm_reset_autosuspend(usbdev);
	return 0;
}

/* Handle resume -> AP initiate from L3. */
int mcd_pm_ps_L3_wake_ready_proc(struct baseband_power_platform_data * data) {
	pr_debug("%s:.\n", __func__);

	mcd_pm_set_slave_wakeup(data, 1);
	return 0;
}

int mcd_pm_ps_L3_proc(struct baseband_power_platform_data * data) {
	mcd_pm_set_host_active(data, 0);

	if(data->slave_wakeup){
		mcd_pm_set_slave_wakeup(data,0);
	}

	pr_debug("%s:\n", __func__);
#if defined(DEBUG_PM_TIME) && !defined(CONFIG_IMC_LOG_OFF)
	mcd_pm_show_time(starttime, "L0->L3. suspend time");
	RESET_START_TIME();
#endif

	return 0;
}

int mcd_pm_ps_L2_proc(struct baseband_power_platform_data * data) {
	if(data->slave_wakeup){
		mcd_pm_set_slave_wakeup(data,0);
	}

	return 0;
}
void mcd_pm_set_power_status(unsigned int status)
{
	struct baseband_power_platform_data *data = baseband_power_driver_data;

	if (status == BBXMM_PS_L0) {
		mcd_pm_wake_unlock();
	}

	if (baseband_xmm_powerstate == status)
		return;

	mcd_pm_save_power_state(status);

	switch (status) {
	case BBXMM_PS_L0:
		mcd_pm_ps_L0_proc();
		break;
	case BBXMM_PS_L0TOL2:
		break;
	case BBXMM_PS_L0_AWAKE:
		mcd_pm_ps_L0AWAKE_proc(data);
		break;
	case BBXMM_PS_L2:
		break;
	case BBXMM_PS_L2_AWAKE:
		mcd_pm_ps_L2AWAKE_proc(data);
		break;
	case BBXMM_PS_L2TOL0_FROM_AP:
		mcd_pm_ps_L2TOL0_from_ap_proc(data);
		break;

	case BBXMM_PS_L3:
		mcd_pm_ps_L3_proc(data);
		break;

	case BBXMM_PS_L3TOL0_FROM_CP:
		mcd_pm_ps_L3TOL0_from_cp_proc(data);
		break;
	case BBXMM_PS_L3_WAKEREADY:
		mcd_pm_ps_L3_wake_ready_proc(data);
		break;
	case BBXMM_PS_L3_CP_WAKEUP:
	default:
		break;
	}
}
EXPORT_SYMBOL_GPL(mcd_pm_set_power_status);

int mcd_pm_hostw_handshake(int timeout)
{
	int i = 0, ret = 0;
	pr_debug("%s. cur_state = %s.\n", __func__,
		pwrstate_cmt[baseband_xmm_powerstate]);

	if (IS_POWERSTATE_INIT(baseband_xmm_powerstate)) {
		pr_debug("%s. skip handshake on init_state.\n", __func__);
		return 0;
	}

	if ((baseband_xmm_powerstate == BBXMM_PS_L3_CP_WAKEUP) ||
		(baseband_xmm_powerstate == BBXMM_PS_L3TOL0_FROM_CP))
		return 0;

	if (mdm_drv == NULL)
		return 0;

	for (i = 0 ; i < timeout; i++) {
		if (mdm_drv->bus_ctrl_reason <= HW_RESET
			|| ipc_ap_wake_state == IPC_AP_WAKE_L)
			return 0;
		mdelay(1);
	}
	if (i >= timeout) {

#if defined(PENDING_HW)
		u32 read_value;
		read_value = tz_read(PEND_REG_BASE);
		if (read_value & (0x1 << 6)) {
			pr_err("GIC detected the HOST_WAKEUP\n");
			ret = -ETIMEDOUT;
		} else {
			pr_err("GIC did not detect the HOST_WAKEUP\n");
			ret = -ENODEV;
		}
#endif

		pr_err("HOST WAKEUP was NOT handled from CP (%d, %d msec)\n",
			no_host_wakeup, timeout);

		no_host_wakeup = true;
	}
	return ret;
}
EXPORT_SYMBOL(mcd_pm_hostw_handshake);

static void mcd_pm_host_wakeup_isr_work(struct work_struct *work)
{
	if (ipc_ap_wake_state == IPC_AP_WAKE_L) {
		pr_debug("%s: *LOW* state = %s.\n", __func__,
			pwrstate_cmt[baseband_xmm_powerstate]);

		switch(baseband_xmm_powerstate) {
			case BBXMM_PS_L3_WAKEREADY:
				mcd_pm_set_power_status(BBXMM_PS_L3_CP_WAKEUP);
				mcd_pm_wake_lock();
				break;

			case BBXMM_PS_L3:
				mcd_pm_set_power_status(BBXMM_PS_L3TOL0_FROM_CP);
				mcd_pm_wake_lock();
				 break;

			/* Go back to L0 state */
			case BBXMM_PS_L0:
			case BBXMM_PS_L2:
				mcd_pm_set_power_status(BBXMM_PS_L2_AWAKE);
				mcd_pm_wake_lock();
				break;

			case BBXMM_PS_L0TOL2:
				mcd_pm_set_power_status(BBXMM_PS_L0_AWAKE);
				mcd_pm_wake_lock();
				break;

			case BBXMM_PS_L2_AWAKE:
				if (baseband_power_driver_data->host_active == 0) {
					mcd_pm_set_power_status(BBXMM_PS_L3_CP_WAKEUP);
					pr_err("%s: L2_AWAKE occur. CP wakeup flow.\n");
					mcd_pm_wake_lock();
				}
				break;

			case BBXMM_PS_L2TOL0_FROM_AP:
				if(!system_suspend_state){
					mcd_pm_wake_lock();
				}
				break;

			default:
				/* Nothing to do */
				pr_err("%s: Unexcepted Interrupt. state = %d.\n",
					__func__, baseband_xmm_powerstate);
				break;
		}
	} else if (ipc_ap_wake_state == IPC_AP_WAKE_H) {
		pr_debug("mcd_pm_host_wakeup_isr_work *HIGH* cur_state = %s.\n",
			pwrstate_cmt[baseband_xmm_powerstate]);

		if (!baseband_power_driver_data->host_active) {
			pr_info("[%s] host active low: ignore request\n.", __func__);
			return;
		}

		mcd_pm_set_power_status(BBXMM_PS_L0);
	}
}

irqreturn_t mcd_pm_host_wakeup_irq(int irq, void *dev_id)
{
	int value;
	struct baseband_power_platform_data *data = baseband_power_driver_data;

	value = gpio_get_value(baseband_power_driver_data->modem.xmm.ipc_ap_wake);

	if (ipc_ap_wake_state < IPC_AP_WAKE_IRQ_READY) {
		pr_err("%s - spurious irq\n", __func__);
	} else if (ipc_ap_wake_state == IPC_AP_WAKE_IRQ_READY) {
		if (!value) {
			pr_debug("%s - IPC_AP_WAKE_INIT" " - got falling edge\n", __func__);

			if (mdm_drv->bus_ctrl_reason == NONE) {
				/* queue work */
				mdm_drv->bus_ctrl_reason = HOST_WAKEUP;
				mdm_drv->bus_ctrl_set_state = IDLE;
				queue_delayed_work(mdm_drv->bus_ctrl_wq,
					&mdm_drv->bus_ctrl_work, 0);

				/* go to IPC_AP_WAKE_INIT1 state */
				ipc_ap_wake_state = IPC_AP_WAKE_INIT1;
			} else {
				pr_err("%s :Ignore HOST WAKEUP BUS CTRL (%d / %d)\n", __func__,
					mdm_drv->bus_ctrl_reason, mdm_drv->bus_ctrl_set_state);
			}
		} else {
			pr_debug("%s - IPC_AP_WAKE_INIT1" " - wait for falling edge\n",	__func__);
		}
	} else if (ipc_ap_wake_state == IPC_AP_WAKE_INIT1) {
		if (!value) {
			pr_debug("%s - IPC_AP_WAKE_INIT2" " - wait for rising edge\n",__func__);
		} else {
			pr_debug("%s - IPC_AP_WAKE_INIT2" " - got rising edge\n",__func__);
			ipc_ap_wake_state = IPC_AP_WAKE_H;
		}
	} else {
		if (!value) {
			/* save gpio state */
			ipc_ap_wake_state = IPC_AP_WAKE_L;
			baseband_power_driver_data->host_wakeup_low = true;

			pr_info("HOST_WAKE_UP. LOW, state(%s), wake_st(%d).\n",
				pwrstate_cmt[baseband_xmm_powerstate],
				ipc_ap_wake_state);

			if(mcd_pm_ap_shutdown == CP_WAKEUP_IN_SHUTDOWN)
				mcd_pm_host_wakeup_isr_work(&host_wakeup_isr);
			else
				queue_work(workqueue_susp, &host_wakeup_isr);
		} else {
			/* save gpio state */
			ipc_ap_wake_state = IPC_AP_WAKE_H;
			baseband_power_driver_data->host_wakeup_low = false;

			pr_info("HOST_WAKE_UP. HIGH, state(%s), wake_st(%d).\n",
				pwrstate_cmt[baseband_xmm_powerstate],
				ipc_ap_wake_state);

			value = data->host_active;
			if (!value) {
				pr_info("host active low: ignore request\n");
				ipc_ap_wake_state = IPC_AP_WAKE_H;
				return IRQ_HANDLED;
			}

			if(mcd_pm_ap_shutdown == CP_WAKEUP_IN_SHUTDOWN)
				mcd_pm_host_wakeup_isr_work(&host_wakeup_isr);
			else
				queue_work(workqueue_susp, &host_wakeup_isr);
		}
	}

	return IRQ_HANDLED;
}

void mcd_pm_coredump_level()
{
	int value;
	u32 read_value;
	struct baseband_power_platform_data *data = baseband_power_driver_data;

#if defined(PENDING_HW)
	/* Check a pending interrupt on GIC */
	read_value = tz_read(PEND_REG_BASE);
	if (read_value & (0x1 << 7)) {
		pr_err("%s: CORE_DUMP is pending. \n", __func__);
	} else {
		pr_err("%s: CORE_DUMP is not pending. \n", __func__);
	}
#endif

	if(!data || (data->modem.xmm.ipc_ap_wake <= 0)){
		printk("%s CORE_DUMP is not available \n", __func__);
		return;
    }

	value = gpio_get_value(data->modem.xmm.ipc_core_dump);
	printk("%s CORE_DUMP is %s !!!! \n", __func__, value? "HIGH":"LOW");
}
EXPORT_SYMBOL(mcd_pm_coredump_level);

static bool mcd_pm_check_enum(void)
{
	bool ret = false;
	int timeout = 0;

	do
	{
		if (usbdev && usbdev->descriptor.idProduct == 0x0452) {
			ret = true;
			break;
		}

		msleep(100); /* Wait 3sec */
	} while (++timeout <= 30);

	pr_info("%s : time = %d msec\n", __func__, timeout * 100);

	return ret;
}

static void mcd_pm_init_done_work(struct work_struct *work)
{
	int value = baseband_power_driver_data->slave_wakeup;

	pr_debug("%s: STATE = L0.\n", __func__);

	if (value) {
		/* Clear the slave wakeup request */
		mcd_pm_set_slave_wakeup(baseband_power_driver_data, 0);
	}

	mcd_pm_save_power_state(BBXMM_PS_L0);
}

static int mcd_pm_driver_handle_resume(
				struct baseband_power_platform_data *data)
{
	pr_debug("%s: cur_state = %s.\n", __func__,
		pwrstate_cmt[baseband_xmm_powerstate]);

#if defined(DEBUG_PM_TIME)
	if (baseband_xmm_powerstate == BBXMM_PS_L3){
		SET_START_TIME();
	}
#endif
	if (IS_POWERSTATE_INIT(baseband_xmm_powerstate)) {
		pr_err("%s: Device is init state.\n",__func__);
		return 0;
	}

    if (!data) {
		pr_err("%s: platform data is NULL. \n",__func__);
		return 0;
    }

	if (baseband_xmm_powerstate == BBXMM_PS_L0_AWAKE) {
		pr_debug("%s : BBXMM_PS_L0_AWAKE.\n",__func__);
		mcd_pm_set_power_status(BBXMM_PS_L0);
		return 0;
	}

	if (baseband_xmm_powerstate == BBXMM_PS_L0){
		pr_debug("%s : BBXMM_PS_L0\n",__func__);
		return 0;
	}

	if (baseband_xmm_powerstate == BBXMM_PS_L3_CP_WAKEUP) {
		pr_debug("%s: CP already wake up. BBXMM_PS_L3_CP_WAKEUP\n",__func__);
		return 0;
	}

	/* In case of CP wakeup from L3, Don't need to handle SLAVE_WAKEUP. */
	if (baseband_xmm_powerstate == BBXMM_PS_L3TOL0_FROM_CP) {
		pr_debug("%s: Skip to handle slave_wakeup.\n",__func__);
		return 0;
	}

	/* We are waiting a response from CP. */
	if (baseband_xmm_powerstate == BBXMM_PS_L3_WAKEREADY) {
		pr_debug("%s: waiting a response HOST wakeup from CP.\n",__func__);
		return 0;
	}

	/* L3->L0, Les't go to trigger the SLAVE_WAKEUP to wake up CP.*/
	mcd_pm_set_power_status(BBXMM_PS_L3_WAKEREADY);

	return 0;
}

static void mcd_pm_L3_resume_work(struct work_struct *work)
{
	mcd_pm_driver_handle_resume(baseband_power_driver_data);
}


#if defined(DEBUG_DEVICE_DESCRIPTOR)
static void printDescriptorDump(struct usb_device *hdev)
{
	struct usb_device_descriptor * des = &(hdev->descriptor);


	printk("=========== Descript DUMP =========\n");
	printk("bLength = %04x, bDescriptorType = %04x, bcdUSB = %04x.\n",
		des->bLength, des->bDescriptorType, des->bcdUSB);
	printk("bDeviceClass = %04x, bDeviceSubClass = %04x, bDeviceProtocol = %04x.\n",
		des->bDeviceClass, des->bDeviceSubClass, des->bDeviceProtocol);
	printk("bMaxPacketSize0 = %04x, idVendor = %04x, idProduct = %04x.\n",
		des->bMaxPacketSize0, des->idVendor, des->idProduct);
	printk("bcdDevice = %04x, iManufacturer = %04x, iProduct = %04x.\n",
		des->bcdDevice, des->iManufacturer, des->iProduct);
	printk("iSerialNumber = %04x, bNumConfigurations = %04x\n",
		des->iSerialNumber, des->bNumConfigurations);
	printk("=========== DUMP END =========\n");
}
#endif

static void mcd_pm_device_add_handler(struct usb_device *udev)
{
	struct usb_interface *intface = NULL;
	const struct usb_device_id *id;
	const struct usb_device_descriptor *desc = NULL;

	pr_debug("%s: \n", __func__);

	if (udev == NULL)
		return;

	pr_debug("udev = 0x%pk\n", udev);

	intface = usb_ifnum_to_if(udev, 0);
	if (intface == NULL)
		return;

	id = usb_match_id(intface, xmm_pm_ids);

	if (id) {
		desc = &udev->descriptor;

		pr_debug("[%s] persist_enabled: %u\n", __func__, udev->persist_enabled);
		pr_info("Add device %d <%s %s>.\n",
				udev->devnum, udev->manufacturer, udev->product);
		pr_info("VID = 0x%.4x, PID = 0x%.4x. udev->do_remote_wakeup = %d.\n",
				desc->idVendor, desc->idProduct, udev->do_remote_wakeup);
#if defined(DEBUG_DEVICE_DESCRIPTOR)
		printDescriptorDump(udev);
#endif
		usbdev = udev;
		usbdev->force_set_port_susp = true;
		intf = intface;

		/* Enable Runtime PM */
		usb_enable_autosuspend(usbdev);

		/* System suspend state set to OFF. */
		baseband_power_driver_data->host_wakeup_low = false;

		/* Notify device add to MUX */
		if (hsic_notifier)
			hsic_notifier->device_add(udev);
	} else {
		pr_debug("[%s] Add error. \n", __func__);
	}
}

static void mcd_pm_device_remove_handler(struct usb_device *udev)
{
	if (usbdev == udev) {
		pr_info("Remove device %d <%s %s>\n",
			udev->devnum, udev->manufacturer, udev->product);

		pm_runtime_set_autosuspend_delay(&usbdev->dev,
			DISABLE_AUTOSUSPEND_DELAY);

		usbdev = NULL;
		if (hsic_notifier)
			hsic_notifier->device_remove(udev);

		mcd_pm_wake_unlock();

		/* System suspend state set to OFF. */
		baseband_power_driver_data->host_wakeup_low = false;

		/* In case HSIC is detached, internal state is changed to initial state
		   and host_active which means HSIC link set to LOW. */
		baseband_xmm_powerstate = BBXMM_PS_UNINIT;
		//mcd_pm_set_host_active(baseband_power_driver_data, 0);
	}
}

static int mcd_pm_usb_notify(struct notifier_block *self, unsigned long action,void *blob)
{
	switch (action) {
	case USB_DEVICE_ADD:
		mcd_pm_device_add_handler(blob);
		break;
	case USB_DEVICE_REMOVE:
		mcd_pm_device_remove_handler(blob);
		break;
	}

	return NOTIFY_OK;
}


static struct notifier_block mcd_pm_usb_nb = {
	.notifier_call = mcd_pm_usb_notify,
};

static int mcd_pm_power_pm_notifier_event(struct notifier_block *this,
					unsigned long event, void *ptr)
{
	struct baseband_power_platform_data *data = baseband_power_driver_data;
	int w;

	if (!data)
		return NOTIFY_DONE;

	switch (event) {
	case PM_SUSPEND_PREPARE:
		pr_debug("%s : PM_SUSPEND_PREPARE\n", __func__);

		if ((usbdev == NULL) || (&usbdev->dev == NULL)
			|| (&usbdev->parent->dev == NULL) || (usbdev->parent == NULL)) {
		    pr_err("%s: usbdev is NULL.\n", __func__);
		    return NOTIFY_DONE;
	    }

		w = device_may_wakeup(&usbdev->parent->dev);
		if(usbdev->parent->state == USB_STATE_SUSPENDED && !w){
			system_suspend_state = true;
			pm_runtime_resume(&usbdev->dev);
			pr_info("1-1 runtime resume\n");
			system_suspend_state = false;
		}

	case PM_POST_SUSPEND:
		pr_debug("%s : PM_POST_SUSPEND\n", __func__);
		break;
	}

	return NOTIFY_OK;
}


static int mcd_pm_shutdown_function(void)
{
	pr_debug("%s: \n", __func__);

	if ( l2_resume_work_done ) {
		flush_workqueue(workqueue_susp);
		flush_workqueue(workqueue);
	}


	return 0;
}


static int mcd_pm_setup_gpios_before_init(
			struct gpio * gpio_array,
			struct baseband_power_platform_data * pdata)
{
	int ret = 0;

	if (!gpio_array || !pdata) {
		ret = -EINVAL;
		goto invalid_param;
	}

	gpio_array[0].gpio = pdata->modem.xmm.bb_pwr_down;
	gpio_array[1].gpio = pdata->modem.xmm.bb_on;
	gpio_array[2].gpio = pdata->modem.xmm.bb_rst;
	gpio_array[3].gpio = pdata->modem.xmm.ipc_bb_wake;
	gpio_array[4].gpio = pdata->modem.xmm.ipc_ap_wake;
	gpio_array[5].gpio = pdata->modem.xmm.ipc_hsic_active;
    gpio_array[6].gpio = pdata->modem.xmm.bb_force_crash;
    gpio_array[7].gpio = pdata->modem.xmm.ipc_cp_rst_out;
    gpio_array[8].gpio = pdata->modem.xmm.ipc_core_dump;

	pr_info("GPIO setting is done.\n");

	return ret;

invalid_param:
	pr_err("%s: invalid parameter.\n", __func__);
	return ret;
}

int mcd_pm_core_dump_set_bus(mdm_ctrl_bus_state val)
{
	int ret;

	if (val == RESET)
		mcd_pm_set_slave_wakeup(baseband_power_driver_data, 0);

	ret = odin_dwc_host_set_idle(val);

	if (!ret && val == IDLE)
		baseband_xmm_powerstate = BBXMM_PS_UNINIT;

	return ret;
}

int mcd_pm_reset_out_set_bus(mdm_ctrl_bus_state val)
{
	int ret;

	ret = odin_dwc_host_set_idle(val);

	if (!ret) {
		udelay(100);
		mcd_pm_set_host_active(baseband_power_driver_data, 0);
		ipc_ap_wake_state = IPC_AP_WAKE_IRQ_READY;
	}

	return ret;
}

int mcd_pm_host_wakup_set_bus(mdm_ctrl_bus_state val)
{
	int ret;

	ret = odin_dwc_host_set_idle(val);

	if (!ret) {
		udelay(100);
		mcd_pm_set_host_active(baseband_power_driver_data, 1);
		mdelay(1);

		if (mcd_pm_check_enum()) {
			pr_debug("USB enumeration is succeed.\n");

			if (baseband_power_driver_data->slave_wakeup)
				mcd_pm_set_slave_wakeup(baseband_power_driver_data, 0);

			mcd_pm_save_power_state(BBXMM_PS_L0);
		} else {
			pr_debug("USB enumeration is failed\n");
		}
	}

	return ret;
}

/* L2.
 * Bus suspend is done.
 */
int mcd_pm_hsic_postsuspend(bool rt_call)
{
	pr_debug("%s. cur_state = %s.\n", __func__,
		pwrstate_cmt[baseband_xmm_powerstate]);

	if (IS_POWERSTATE_INIT(baseband_xmm_powerstate))
		return 0;

	SET_RUNTIME_STATE(rt_call);

	/* If device is going back to L0, we can't go to L2.
	 * It means there is interrupt between L0 and L2.
	 */
	if (baseband_xmm_powerstate == BBXMM_PS_L0_AWAKE)
		return -EPERM;

	return 0;
}
EXPORT_SYMBOL(mcd_pm_hsic_postsuspend);

int mcd_pm_hsic_preresume(bool rt_call)
{
	pr_debug("%s. cur_state = %s.\n", __func__,
		pwrstate_cmt[baseband_xmm_powerstate]);

	if (IS_POWERSTATE_INIT(baseband_xmm_powerstate))
		return -ESHUTDOWN;

	SET_RUNTIME_STATE(rt_call);

	if (!rt_call) {
		pr_debug("SYSTEM.resume. \n");
	}

	switch(baseband_xmm_powerstate) {
		case BBXMM_PS_L0_AWAKE:
		case BBXMM_PS_L2_AWAKE:
			break;

		case BBXMM_PS_L2:
			mcd_pm_set_power_status(BBXMM_PS_L2TOL0_FROM_AP);
			break;

		case BBXMM_PS_L3:
			queue_work(workqueue, &L3_resume_work);
			break;

		default:
			break;
	}

	return 0;
}
EXPORT_SYMBOL(mcd_pm_hsic_preresume);

int mcd_pm_hsic_phy_ready(bool rt_call)
{
	pr_debug("%s. cur_state = %s.\n", __func__,
		pwrstate_cmt[baseband_xmm_powerstate]);

	if (IS_POWERSTATE_INIT(baseband_xmm_powerstate))
		return 0;

	SET_RUNTIME_STATE(rt_call);

	/* Phy was turned ON. */
	baseband_power_driver_data->phy_ready = true;

	if (baseband_xmm_powerstate != BBXMM_PS_L2TOL0_FROM_AP
		&& baseband_xmm_powerstate != BBXMM_PS_L2_AWAKE) {
		mcd_pm_set_host_active(baseband_power_driver_data, 1);
		if (!RUNTIME_EXEC(usbdev))
			odin_dwc_host_set_force_idle(1);
	}

	return 0;
}
EXPORT_SYMBOL(mcd_pm_hsic_phy_ready);

/* Trying to turn off the PHY, LDO, PD_1 in host controller.
 * Runtime : L0 -> L0TOL2.
 * System  : L0 -> L2.
 */
int mcd_pm_hsic_phy_off(bool rt_call)
{
	pr_info("%s. state = %s. r = %d.\n",
		__func__, pwrstate_cmt[baseband_xmm_powerstate],
		rt_call);

#if defined(DEBUG_PM_TIME) && !defined(CONFIG_IMC_LOG_OFF)
	SET_START_TIME();
#endif
	if (IS_POWERSTATE_INIT(baseband_xmm_powerstate))
		return 0;

	SET_RUNTIME_STATE(rt_call);

	if (baseband_xmm_powerstate == BBXMM_PS_L0_AWAKE
		|| baseband_xmm_powerstate == BBXMM_PS_L2_AWAKE
		|| baseband_xmm_powerstate == BBXMM_PS_L3_WAKEREADY
		|| baseband_xmm_powerstate == BBXMM_PS_L3_CP_WAKEUP) {
		pr_err("%s. Current status = %s. Go back to L0.\n",
			__func__, pwrstate_cmt[baseband_xmm_powerstate]);
		return -EBUSY;
	}

	if (!RUNTIME_EXEC(usbdev)) {
		if ( (baseband_power_driver_data->host_wakeup_low == true)
			&& ( (baseband_xmm_powerstate == BBXMM_PS_L0)
				|| (baseband_xmm_powerstate == BBXMM_PS_L2)
				|| (baseband_xmm_powerstate == BBXMM_PS_L0TOL2) ) ) {
			pr_err("%s. State is AWAKE case, Go back to L0.\n",
				__func__);
			return -EBUSY;
		}
	}

#if defined(PENDING_HW)
	mcd_pm_disable_HW_irq();
#endif

	if (RUNTIME_EXEC(usbdev)) {
		mcd_pm_set_power_status(BBXMM_PS_L0TOL2);
	}else {
		mcd_pm_set_power_status(BBXMM_PS_L2);
	}

	return 0;
}
EXPORT_SYMBOL(mcd_pm_hsic_phy_off);

/* PHY,LDO,PD_1 is turned off.
 * Runtime : L0TOL2 -> L2.
 * System  : L2 -> L3.
 */
int mcd_pm_hsic_phy_off_done(bool rt_call)
{
	pr_debug("%s. state = %s. r = %d\n",
		__func__, pwrstate_cmt[baseband_xmm_powerstate], rt_call);

	if (IS_POWERSTATE_INIT(baseband_xmm_powerstate))
		return 0;

	SET_RUNTIME_STATE(rt_call);

	/* Phy was turned OFF. */
	baseband_power_driver_data->phy_ready = false;

	if (RUNTIME_EXEC(usbdev)) {
		mcd_pm_set_power_status(BBXMM_PS_L2);
	} else {
		mcd_pm_set_power_status(BBXMM_PS_L3);
	}

	return 0;
}
EXPORT_SYMBOL(mcd_pm_hsic_phy_off_done);

static struct notifier_block mcd_pm_power_pm_notifier = {
	.notifier_call = mcd_pm_power_pm_notifier_event,
};

static int mcd_pm_reboot_notify(struct notifier_block *nb,
                                unsigned long event, void *data)
{
	pr_debug("%s\n", __func__);

    switch (event) {
    case SYS_RESTART:
    case SYS_HALT:
    case SYS_POWER_OFF:
		mcd_pm_shutdown_function();
	    return NOTIFY_OK;
    }
    return NOTIFY_DONE;
}

static struct notifier_block mcd_pm_reboot_nb = {
	.notifier_call = mcd_pm_reboot_notify,
};

int mcd_pm_driver_init(struct platform_device *device)
{
	struct baseband_power_platform_data *data;
	struct mcd_base_info * mcd_data;
	int err = 0;

	pr_info("%s\n", __func__);

	mcd_data = device->dev.platform_data;
	data = mcd_data->pm_data;

	/* check for platform data */
	if (!data) {
		pr_debug("[XMM] data is NULL.\n");
		return -ENODEV;
	}

	/* check if supported modem */
	if (data->baseband_type != BASEBAND_XMM) {
		pr_err("unsuppported modem\n");
		return -ENODEV;
	}

	/* save platform data */
	baseband_power_driver_data = data;

	/* init wake lock */
	wake_lock_init(&wakelock, WAKE_LOCK_SUSPEND, "baseband_xmm_power");

	/* init spin lock */
	spin_lock_init(&xmm_lock);
	/* request baseband gpio(s) */

	err = mcd_pm_setup_gpios_before_init(tegra_baseband_gpios, data);
	if (err < 0) {
		pr_err("%s - gpio(s) setup is failed. err = %d.\n", __func__, err);
		return -ENODEV;
	}

	/* Print GPIO settings. */
	/*
	for (i = 0; i < 9; i++) {
		pr_info("gpio[%d] : %d.\n", i, tegra_baseband_gpios[i].gpio);
	}
	*/

	err = gpio_request_array(tegra_baseband_gpios,ARRAY_SIZE(tegra_baseband_gpios));
	if (err < 0) {
		pr_err("%s - request gpio(s) failed. err = %d.\n", __func__, err);
		return -ENODEV;
	}

    gpio_export(tegra_baseband_gpios[6].gpio, true);
    gpio_set_value(tegra_baseband_gpios[6].gpio, 0);

	/* Export gpio */
#if defined(EXPORT_GPIOS)
	for (i = 0; i < XMM_GPIO_MAX; i++) {
		if (xmm_gpio_resources[i].export) {
			gpio_export(xmm_gpio_resources[i].gpio_value, true);
		}
	}
#endif

	/* init work queue */
	workqueue = create_singlethread_workqueue("baseband_xmm_power_workqueue");
	if (!workqueue) {
		pr_err("cannot create workqueue\n");
		return -1;
	}
	workqueue_susp = alloc_workqueue("baseband_xmm_power_autosusp",
				WQ_HIGHPRI | WQ_NON_REENTRANT, 1);
	if (!workqueue_susp) {
		pr_err("cannot create workqueue_susp\n");
		return -1;
	}

	/* init work objects */
	INIT_WORK(&L3_resume_work, mcd_pm_L3_resume_work);
	INIT_WORK(&host_wakeup_isr, mcd_pm_host_wakeup_isr_work);

	/* init state variables */
	baseband_xmm_powerstate = BBXMM_PS_UNINIT;

	/* register notifier */
	usb_register_notify(&mcd_pm_usb_nb);
	register_pm_notifier(&mcd_pm_power_pm_notifier);
	register_reboot_notifier(&mcd_pm_reboot_nb);

	/* Runtime PM */
	pm_runtime_enable(&device->dev);

	pr_info("%s is DONE.\n", __func__);
	return 0;
}

int mcd_pm_driver_remove(struct platform_device *device)
{
	pr_debug("%s\n", __func__);

	/* check for platform data */
	if (!device) {
		pr_err("%s device is invalid.\n", __func__);
		return 0;
	}

	unregister_pm_notifier(&mcd_pm_power_pm_notifier);
	usb_unregister_notify(&mcd_pm_usb_nb);

	/* free baseband gpio(s) */
	gpio_free_array(tegra_baseband_gpios,ARRAY_SIZE(tegra_baseband_gpios));

	/* Runtime PM */
	pm_runtime_disable(&device->dev);

	/* destroy wake lock */
	wake_lock_destroy(&wakelock);

	/* destroy wake lock */
	destroy_workqueue(workqueue_susp);
	destroy_workqueue(workqueue);

	return 0;
}

int mcd_pm_driver_shutdown(struct platform_device *device)
{
	pr_debug("%s\n", __func__);

	if (IS_POWERSTATE_INIT(baseband_xmm_powerstate))
		return 0;

	if ( l2_resume_work_done ) {
		flush_workqueue(workqueue_susp);
		flush_workqueue(workqueue);
	} else {
		pr_err("%s:Called reboot_notify during resuming\n", __func__);
	}

	return 0;
}

#ifdef CONFIG_PM
static int mcd_pm_power_driver_suspend(struct device *dev)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int mcd_pm_power_driver_resume(struct device *dev)
{

#if defined(ODIN_DWC_HOST_SET_CPU_AFFINITY)
	cpumask_var_t cpumask;
#endif
	struct mcd_base_info *data
                = (struct mcd_base_info *)dev->platform_data;

#if defined(ODIN_DWC_HOST_SET_CPU_AFFINITY)
	cpumask_clear(cpumask);
	cpumask_set_cpu(MCD_HOST_WAKEUP_CPU_AFFINITY_NUM, cpumask);
	irq_set_affinity(data->cpu.get_irq_wakeup(data->cpu_data), cpumask);
	printk("%s : HOST_WAKEUP : %d \n", __func__,
		data->cpu.get_irq_wakeup(data->cpu_data));
#endif

	 if (IS_POWERSTATE_INIT(baseband_xmm_powerstate))
		 return 0;

     pr_debug("%s: cur_state = %s. \n", __func__,
	 	pwrstate_cmt[baseband_xmm_powerstate]);
     mcd_pm_driver_handle_resume(data->pm_data);

     return 0;
}

static int mcd_pm_power_suspend_noirq(struct device *dev)
{
    pr_debug("%s.\n", __func__);
	mcd_pm_ap_shutdown = SYSTEM_SHUTDOWN;
    return 0;
}

static int mcd_pm_power_resume_noirq(struct device *dev)
{
	int value;

	if(mcd_pm_ap_shutdown == CP_WAKEUP_IN_SHUTDOWN) {
		mcd_pm_disable_HW_irq();
		host_wakeup_proc(
			mdm_drv->pdata->cpu.get_irq_wakeup(mdm_drv->pdata->cpu_data), mdm_drv);
		mcd_pm_enable_hw_irq();
	}

    mcd_pm_ap_shutdown = SYSTEM_WAKEUP;
	value = gpio_get_value(baseband_power_driver_data->modem.xmm.ipc_ap_wake);
	pr_debug("%s.\n",__func__);
	return 0;
}

const struct dev_pm_ops mcd_pm_dev_pm_ops = {
	.suspend_noirq = mcd_pm_power_suspend_noirq,
	.resume_noirq = mcd_pm_power_resume_noirq,
	.suspend = mcd_pm_power_driver_suspend,
	.resume = mcd_pm_power_driver_resume,
};
#endif
