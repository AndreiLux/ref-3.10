/**
 * linux/modules/drivers/modem_control/mdm_ctrl.c
 *
 * Version 1.0
 *
 * This code allows to power and reset IMC modems.
 * There is a list of commands available in include/linux/mdm_ctrl.h
 * Current version supports the following modems :
 * - IMC6260
 * - IMC6360
 * - IMC7160
 * - IMC7260
 * There is no guarantee for other modems
 *
 *
 * Intel Mobile driver for modem powering.
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

#include <linux/platform_data/gpio-odin.h>
#include <linux/mdm_ctrl.h>
#include <linux/mdm_ctrl_board.h>
#include <linux/spinlock.h>

#include "mdm_util.h"
#include "mcd_mdm.h"
#include "mcd_pm.h"

// change for build
#define MDM_BOOT_DEVNAME	"mdm_ctrl"

#define MDM_MODEM_READY_DELAY	60	/* Modem readiness wait duration (sec) */


/*Ap can do core dump before it takes the reset out pin */
#define DELAY_HOST_ACTIVE_FOR_CP_TIMING

#define FEATURE_RECOVERY_CP_CRASH

#if defined(CONFIG_MIGRATION_CTL_DRV)
#define MDM_BOOT_CHECK_RETRY	10
static int retry_count = 0;
static bool check_2nd_boot = false;

/* Workaround : Soft USB Disconnect by MMGR */
extern int odin_dwc_host_set_idle(int set);
extern void odin_dwc_host_set_pre_reset(void);
#endif

#define DUMP_IPC_PACKET
#ifdef DUMP_IPC_PACKET
static struct delayed_work ipc_dump_work;
extern void print_ipc_dump_packet();
#endif

#define DUMP_ACM_PACKET
#ifdef DUMP_ACM_PACKET
static struct delayed_work acm_dump_work;
extern void print_acm_dump_packet();
#endif

#define DUMP_MUX_PACKET
#ifdef DUMP_MUX_PACKET
static struct delayed_work mux_dump_work;
extern void print_mux_dump_packet();
#endif

extern int mdm_reset;

#ifdef FEATURE_RECOVERY_CP_CRASH
extern int crash_by_gpio_status;
#endif

extern int mcd_pm_get_ap_shutdown_state(void);
extern void mcd_pm_set_ap_shutdown_state(int statue);
extern int mcd_pm_wake_lock(void);

#if defined(CONFIG_LGE_CRASH_HANDLER)
trap_info_t trapinfo;
unsigned char is_modem_crashed;

static spinlock_t mdm_lock;

trap_info_t* get_trap_info_data()
{
	return &trapinfo;
}
EXPORT_SYMBOL(get_trap_info_data);

unsigned char get_modem_crash_status()
{
	return is_modem_crashed;
}
EXPORT_SYMBOL(get_modem_crash_status);
#endif

#if defined(RECOVERY_BY_USB_DISCONNECTION)
struct delayed_work force_reset_work;
bool is_modem_coredump = false;
bool is_modem_reset = false;
bool get_modem_hangup_state()
{
    printk("[e2sh] is_modem_coredump = %d, is_modem_reset = %d\n",
                                    is_modem_coredump, is_modem_reset);
    return is_modem_coredump || is_modem_reset;
}
EXPORT_SYMBOL(get_modem_hangup_state);
#endif

#define ODIN_XMM_ADAPTATION
#ifdef ODIN_XMM_ADAPTATION
#define MAX_DELAYED_COLDRESET_RETRY_CNT       10
int is_cdc_being_attached = 0;
struct delayed_work delayed_cold_reset_work;
#endif

#if defined(CONFIG_MIGRATION_CTL_DRV)
const char *bus_ctrl_reason[] = {
	"CORE_DUMP_RESET",
	"CORE_DUMP_IDLE",
	"HW_RESET",
	"HOST_WAKEUP",
	"NONE",
};

static struct class *mdm_class;
struct device *mdm_sysfs;

static ssize_t as_enable_show(struct device *pdev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "L2_autosuspend_enable_show\n");
}

static ssize_t as_enable_store(struct device *pdev,
			struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int value;

	value = simple_strtoul(buf, NULL, 16);
	pr_info(DRVNAME ": L2_autosuspend_enable_store value=%d\n", value);

	return n;
}
static DEVICE_ATTR(as_enable, S_IRUGO | S_IWUSR,
			as_enable_show, as_enable_store);

static ssize_t hsic_enable_show(struct device *pdev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "hsic_enable_show\n");
}

static ssize_t hsic_enable_store(struct device *pdev,
			struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int value;

	value = simple_strtoul(buf, NULL, 16);
	pr_info(DRVNAME ": hsic_enable_store value=%d\n", value);

	return n;
}
static DEVICE_ATTR(hsic_enable, S_IRUGO | S_IWUSR,
			hsic_enable_show, hsic_enable_store);

static ssize_t soft_discon_store(struct device *pdev,
			struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int value;

	value = simple_strtoul(buf, NULL, 16);
	odin_dwc_host_set_idle(value);
	pr_info(DRVNAME ": soft_discon_store value=%d\n", value);

	return n;
}
static DEVICE_ATTR(soft_discon, S_IWUSR, NULL, soft_discon_store);

#if defined(CONFIG_LGE_CRASH_HANDLER)
static ssize_t force_panic_store(struct device *pdev,
			struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int value;

	value = simple_strtoul(buf, NULL, 16);
	pr_info(DRVNAME ": force_panic_store value=%d\n", value);

	if (value) {
		is_modem_crashed = 1;
		//panic("MODEM Crash: Force AP dump after CP dump\n");
		BUG();
	}

	return n;
}
static DEVICE_ATTR(force_panic, S_IWUSR, NULL, force_panic_store);
#endif

static void mdm_bus_ctrl_work(struct work_struct *work)
{
	struct mdm_ctrl *drv = mdm_drv;

	pr_info(DRVNAME ": %s , Reason is %s\n", __func__,
		bus_ctrl_reason[drv->bus_ctrl_reason]);

	if (drv->bus_ctrl_set_state > IDLE ) {
		pr_err(DRVNAME ": Invalid bus ctrl state (%d)\n", drv->bus_ctrl_set_state);
		return ;
	}

	switch(drv->bus_ctrl_reason) {
	case CORE_DUMP_RESET:
		if (drv->bus_ctrl_set_state != RESET) {
			pr_err(DRVNAME ": Invalid bus_ctrl_set_state (IDLE)\n");
		} else if (!mcd_pm_core_dump_set_bus(drv->bus_ctrl_set_state)) {
			drv->bus_ctrl_set_state = IDLE;
			drv->bus_ctrl_reason = CORE_DUMP_IDLE;
			queue_delayed_work(drv->bus_ctrl_wq, &drv->bus_ctrl_work,
				msecs_to_jiffies(1000));
		} else {
			goto retry;
		}
		break;
	case CORE_DUMP_IDLE:
		if (drv->bus_ctrl_set_state != IDLE) {
			pr_err(DRVNAME ": Invalid bus_ctrl_set_state (RESET)\n");
		} else if (!mcd_pm_core_dump_set_bus(drv->bus_ctrl_set_state)) {
			drv->bus_ctrl_set_state = DONE;
			drv->bus_ctrl_reason = NONE;
		} else {
			goto retry;
		}
		break;
	case HW_RESET:
		if (drv->bus_ctrl_set_state != RESET) {
			pr_err(DRVNAME ": Invalid bus_ctrl_set_state (IDLE)\n");
		} else if (!mcd_pm_reset_out_set_bus(drv->bus_ctrl_set_state)) {
			drv->bus_ctrl_set_state = DONE;
			drv->bus_ctrl_reason = NONE;
		} else {
			goto retry;
		}
		break;
	case HOST_WAKEUP:
		if (drv->bus_ctrl_set_state != IDLE) {
			pr_err(DRVNAME ": Invalid RESET BUS state\n");
		} else if (!mcd_pm_host_wakup_set_bus(drv->bus_ctrl_set_state)) {
			drv->bus_ctrl_set_state = DONE;
			drv->bus_ctrl_reason = NONE;
		} else {
			goto retry;
		}
		break;
	default:
		pr_err(DRVNAME ": Invalid bus_ctrl_set_state\n");
		break;
	}

	drv->bus_ctrl_retry_cnt = 0;
	return;

retry:
	if (++drv->bus_ctrl_retry_cnt > 1) {
		drv->bus_ctrl_retry_cnt = 0;
		drv->bus_ctrl_set_state = DONE;
		drv->bus_ctrl_reason = NONE;
		pr_err(DRVNAME ": Failed to control BUS\n");
	} else {
		pr_err(DRVNAME ": Failed to control BUS, Retry\n");
		queue_delayed_work(drv->bus_ctrl_wq, &drv->bus_ctrl_work,
			msecs_to_jiffies(500));
	}
}

static void mdm_second_enum_work(struct work_struct *work)
{
	struct mdm_ctrl *drv = mdm_drv;
	int mdm_state = mdm_ctrl_get_state(drv);
	int rst_ongoing = atomic_read(&drv->rst_ongoing);
	pr_info("%s: re-check. mdm_state = %d. rst_ongoing = %d.\n",
		__func__, mdm_state, rst_ongoing);
	if (mdm_state == MDM_CTRL_STATE_IPC_READY && rst_ongoing == 0) {
		pr_info("IPC is ready. second enum start.\n");
		drv->pdata->pm.ap_wakeup_handler(0, mdm_drv);
		check_2nd_boot = false;
		retry_count = 0;
	} else {
		retry_count++;
		pr_err("%s: Modem is not ready. retry_count = %d.\n",
			__func__, retry_count);
		if (retry_count < MDM_BOOT_CHECK_RETRY) {
			schedule_delayed_work(&drv->second_enum_work, msecs_to_jiffies(50));
		}
	}
}
#endif

/**
 *  mdm_ctrl_handle_hangup - This function handle the modem reset/coredump
 *  @work: a reference to work queue element
 *
 */
static void mdm_ctrl_handle_hangup(struct work_struct *work)
{
	struct mdm_ctrl *drv = mdm_drv;
	int modem_rst;

	/* Check the hangup reason */
	modem_rst = drv->hangup_causes;

	if (modem_rst & MDM_CTRL_HU_RESET)
		mdm_ctrl_set_state(drv, MDM_CTRL_STATE_WARM_BOOT);

	if (modem_rst & MDM_CTRL_HU_COREDUMP) {
		mdm_ctrl_set_state(drv, MDM_CTRL_STATE_COREDUMP);
	}

	pr_info(DRVNAME ": %s (reasons: 0x%X)\n", __func__, drv->hangup_causes);
}

/*****************************************************************************
 *
 * Local driver functions
 *
 ****************************************************************************/

static int mdm_ctrl_cold_boot(struct mdm_ctrl *drv)
{
	unsigned long flags;

	struct mdm_ops *mdm = &drv->pdata->mdm;
	struct cpu_ops *cpu = &drv->pdata->cpu;
	struct pmic_ops *pmic = &drv->pdata->pmic;

	void *mdm_data = drv->pdata->modem_data;
	void *cpu_data = drv->pdata->cpu_data;
	void *pmic_data = drv->pdata->pmic_data;

	int rst, pwr_on, cflash_delay;
#if defined(CONFIG_MIGRATION_CTL_DRV)
	int host_active, pwr_dwn;
#endif

	pr_warn(DRVNAME ": Cold boot requested\n");

	/* Set the current modem state */
	mdm_ctrl_set_state(drv, MDM_CTRL_STATE_COLD_BOOT);

	/* AP request => just ignore the modem reset */
	atomic_set(&drv->rst_ongoing, 1);

	rst = cpu->get_gpio_rst(cpu_data);
	pwr_on = cpu->get_gpio_pwr(cpu_data);
	cflash_delay = mdm->get_cflash_delay(mdm_data);
#if defined(CONFIG_MIGRATION_CTL_DRV)
	host_active = cpu->get_gpio_host_active(cpu_data);
	pwr_dwn = cpu->get_gpio_pwr_down(cpu_data);
	mdm->power_on(mdm_data, rst, pwr_on, host_active, pwr_dwn);
#else
	pmic->power_on_mdm(pmic_data);
	mdm->power_on(mdm_data, rst, pwr_on);
#endif

#if defined(CONFIG_MIGRATION_CTL_DRV)
	mdm_ctrl_launch_timer(&drv->flashing_timer, cflash_delay,
				MDM_TIMER_FLASH_ENABLE, drv);
#else
	mdm_ctrl_launch_timer(&drv->flashing_timer,
			      cflash_delay, MDM_TIMER_FLASH_ENABLE);
#endif
	return 0;
}

static int mdm_ctrl_normal_warm_reset(struct mdm_ctrl *drv)
{
	unsigned long flags;

	struct mdm_ops *mdm = &drv->pdata->mdm;
	struct cpu_ops *cpu = &drv->pdata->cpu;
	struct pmic_ops *pmic = &drv->pdata->pmic;

	void *mdm_data = drv->pdata->modem_data;
	void *cpu_data = drv->pdata->cpu_data;
	void *pmic_data = drv->pdata->pmic_data;

	int rst, wflash_delay;

	pr_info(DRVNAME ": Normal warm reset requested\r\n");

	/* AP requested reset => just ignore */
	atomic_set(&drv->rst_ongoing, 1);

	/* Set the current modem state */
	mdm_ctrl_set_state(drv, MDM_CTRL_STATE_WARM_BOOT);

	rst = cpu->get_gpio_rst(cpu_data);
	wflash_delay = mdm->get_wflash_delay(mdm_data);
	mdm->warm_reset(mdm_data, rst);

#if defined(CONFIG_MIGRATION_CTL_DRV)
	mdm_ctrl_launch_timer(&drv->flashing_timer,
				  wflash_delay, MDM_TIMER_FLASH_ENABLE, drv);
#else
	mdm_ctrl_launch_timer(&drv->flashing_timer,
			      wflash_delay, MDM_TIMER_FLASH_ENABLE);
#endif

	return 0;
}

static int mdm_ctrl_flashing_warm_reset(struct mdm_ctrl *drv)
{
	unsigned long flags;

	struct mdm_ops *mdm = &drv->pdata->mdm;
	struct cpu_ops *cpu = &drv->pdata->cpu;
	struct pmic_ops *pmic = &drv->pdata->pmic;

	void *mdm_data = drv->pdata->modem_data;
	void *cpu_data = drv->pdata->cpu_data;
	void *pmic_data = drv->pdata->pmic_data;

	int rst, wflash_delay;

	pr_info(DRVNAME ": Flashing warm reset requested\n");

	/* AP requested reset => just ignore */
	atomic_set(&drv->rst_ongoing, 1);

	/* Set the current modem state */
	mdm_ctrl_set_state(drv, MDM_CTRL_STATE_WARM_BOOT);

	rst = cpu->get_gpio_rst(cpu_data);
	wflash_delay = mdm->get_wflash_delay(mdm_data);
	mdm->warm_reset(mdm_data, rst);

	msleep(wflash_delay);

	return 0;
}

static int mdm_ctrl_power_off(struct mdm_ctrl *drv)
{
	unsigned long flags;

	struct mdm_ops *mdm = &drv->pdata->mdm;
	struct cpu_ops *cpu = &drv->pdata->cpu;
	struct pmic_ops *pmic = &drv->pdata->pmic;

	void *mdm_data = drv->pdata->modem_data;
	void *cpu_data = drv->pdata->cpu_data;
	void *pmic_data = drv->pdata->pmic_data;

	int rst;

	pr_info(DRVNAME ": Power OFF requested\n");

#if defined(CONFIG_MIGRATION_CTL_DRV)
	if ((drv->bus_ctrl_reason != NONE) || (drv->bus_ctrl_set_state != DONE)) {
		pr_err(DRVNAME ": Need check the bus ctrls (%d / %d)\n",
			drv->bus_ctrl_reason, drv->bus_ctrl_set_state);
		drv->bus_ctrl_reason = NONE;
		drv->bus_ctrl_set_state = DONE;
	}
#endif

	/* AP requested reset => just ignore */
	atomic_set(&drv->rst_ongoing, 1);

	/* Set the modem state to OFF */
	mdm_ctrl_set_state(drv, MDM_CTRL_STATE_OFF);

	rst = cpu->get_gpio_rst(cpu_data);
	mdm->power_off(mdm_data, rst);
#if defined(CONFIG_MIGRATION_CTL_DRV)
	/* Don't need pmic operation for Odin. */
#else
	pmic->power_off_mdm(pmic_data);
#endif
	return 0;
}

static int mdm_ctrl_cold_reset(struct mdm_ctrl *drv)
{
	pr_warn(DRVNAME ": Cold reset requested.\n");
#ifdef ODIN_XMM_ADAPTATION
	if(is_cdc_being_attached){
		printk("%s acm/ncm devices are being attached. Modem reset try in 100ms again!\n", __func__);
		schedule_delayed_work(&delayed_cold_reset_work, msecs_to_jiffies(100));
		return 0;
	} else {
		printk("%s Modem reset right now!\n", __func__);
	}
#endif
	mdm_ctrl_power_off(drv);
	mdm_ctrl_cold_boot(drv);

	return 0;
}
#ifdef ODIN_XMM_ADAPTATION
static int delayed_cold_reset_cnt = 0;
void delayed_cold_reset()
{
	if(!mdm_drv){
        pr_err(DRVNAME ": no mdm_drv instance\n");
        return -1;
    }
    if(is_cdc_being_attached && delayed_cold_reset_cnt < MAX_DELAYED_COLDRESET_RETRY_CNT){
        printk("%s acm/ncm devices are being attached. Modem reset try in 300ms again!(%d)\n", __func__, delayed_cold_reset_cnt++);
        schedule_delayed_work(&delayed_cold_reset_work, msecs_to_jiffies(300));
        return 0;
    }
    printk("[e2sh] Modem is being reset now.(retry count = %d)\n", delayed_cold_reset_cnt);
    delayed_cold_reset_cnt = 0;
    mdm_ctrl_power_off(mdm_drv);
    mdm_ctrl_cold_boot(mdm_drv);
}
#endif

#if defined(RECOVERY_BY_USB_DISCONNECTION)
int force_mdm_ctrl_cold_reset()
{
#if 0
    if(!mdm_reset){	//for blocking the hub disconnected reset
        pr_err(DRVNAME ": skip recovery - recovery disabled \n");
        return -1;
    }
#endif
    if(!mdm_drv){
        pr_err(DRVNAME ": no mdm_drv instance\n");
        return -1;
    }
    pr_debug(DRVNAME ": Modem is being reset now.\n");
    mdm_ctrl_power_off(mdm_drv);
    mdm_ctrl_cold_boot(mdm_drv);
}
void force_mdm_ctrl_cold_reset_request()
{
    pr_debug(DRVNAME ": Force modem cold reset requested. will be reset in 100ms\n");
    schedule_delayed_work(&force_reset_work, msecs_to_jiffies(100));
    return;
}
EXPORT_SYMBOL(force_mdm_ctrl_cold_reset_request);
#endif
#if defined(CONFIG_MIGRATION_CTL_DRV)
int host_wakeup_proc(int irq, void *data)
{
	struct mdm_ctrl *drv = data;
	struct baseband_power_platform_data * baseband_data = drv->pdata->pm_data;
	int value = 0;

	spin_lock(&mdm_lock);
	/* Ignoring event if we are in OFF state. */
	if (mdm_ctrl_get_state(drv) == MDM_CTRL_STATE_OFF) {
		spin_unlock(&mdm_lock);
		return -1;
	}

	/* Ignoring if Modem reset is ongoing. */
	if (atomic_read(&drv->rst_ongoing) == 1) {
		spin_unlock(&mdm_lock);
		value = gpio_get_value(baseband_data->modem.xmm.ipc_ap_wake);

		/* re-check. Host wakeup signal. */
		if (value == 0 && check_2nd_boot)
			schedule_delayed_work(&drv->second_enum_work, msecs_to_jiffies(50));

		return -2;
	}

	if (drv->bus_ctrl_reason <= CORE_DUMP_IDLE) {
		spin_unlock(&mdm_lock);
		return -3;
	}

	/* operate pm control. */
	if (!drv->pdata->pm.ap_wakeup_handler(irq, data)) {
		spin_unlock(&mdm_lock);
		return -4;
	}
	spin_unlock(&mdm_lock);

	return 0;
}
EXPORT_SYMBOL(host_wakeup_proc);

/**
 *  Host wakeup Handler.
 *  @irq: IRQ number
 *  @data: mdm_ctrl driver reference
 *
 */
static irqreturn_t mdm_ctrl_hostwakeup_it(int irq, void *data)
{
	struct mdm_ctrl *drv = data;
	int ret;

	if (mcd_pm_get_ap_shutdown_state() == SYSTEM_SHUTDOWN) {
		mcd_pm_set_ap_shutdown_state(CP_WAKEUP_IN_SHUTDOWN);
		mcd_pm_wake_lock();
		return IRQ_HANDLED;
	}

	ret = host_wakeup_proc(irq, data);
	if(ret)
		goto error;

	pr_info(DRVNAME ": HOST_WAKEUP it. state = %d.\n", mdm_ctrl_get_state(drv));
	return IRQ_HANDLED;

error:
	pr_err(DRVNAME ": HOST_WAKEUP error. NO = %d\r\n", ret);
	return IRQ_HANDLED;
}
#endif

/**
 *  mdm_ctrl_coredump_it - Modem has signaled a core dump
 *  @irq: IRQ number
 *  @data: mdm_ctrl driver reference
 *
 *  Schedule a work to handle CORE_DUMP depending on current modem state.
 */
static irqreturn_t mdm_ctrl_coredump_it(int irq, void *data)
{
	struct mdm_ctrl *drv = data;

	odin_dwc_host_set_pre_reset();

	pr_err(DRVNAME ": CORE_DUMP it.\n");
#if 0
	if(gpio_get_value(98)){
		gpio_set_value(98, 0);
		printk("%s] force cp crash = [%s] \n",__func__, gpio_get_value(98)? "HIGH": "LOW");
	}
#endif
	/* Ignoring event if we are in OFF state. */
	if (mdm_ctrl_get_state(drv) == MDM_CTRL_STATE_OFF) {
		pr_err(DRVNAME ": CORE_DUMP while OFF\r\n");
		goto out;
	}

	/* Ignoring if Modem reset is ongoing. */
	if (atomic_read(&drv->rst_ongoing) == 1) {
#ifdef LGE_DO_CORE_DUMP_BEFORE_RESET_OUT
		pr_err(DRVNAME ": CORE_DUMP while Modem Reset is ongoing DO it!\r\n");
		atomic_set(&drv->rst_ongoing, 0);
#else
		pr_err(DRVNAME ": CORE_DUMP while Modem Reset is ongoing\r\n");
		goto out;
#endif
	}

#if defined(CONFIG_MIGRATION_CTL_DRV)
	if (drv->bus_ctrl_reason == NONE) {
		drv->bus_ctrl_reason = CORE_DUMP_RESET;
		drv->bus_ctrl_set_state = RESET;
		queue_delayed_work(drv->bus_ctrl_wq, &drv->bus_ctrl_work, 0);
	} else {
		pr_err(DRVNAME ": Ignore CORE_DUMP BUS CTRL (%d / %d)\n",
			drv->bus_ctrl_reason, drv->bus_ctrl_set_state);
	}
#endif

#if defined(RECOVERY_BY_USB_DISCONNECTION)
	is_modem_coredump = true;
#endif

	/* Set the reason & launch the work to handle the hangup */
	drv->hangup_causes |= MDM_CTRL_HU_COREDUMP;
	queue_work(drv->hu_wq, &drv->hangup_work);

#ifdef DUMP_IPC_PACKET
    printk("%s IPC packet dump in 9 sec \n", __func__);
	schedule_delayed_work(&ipc_dump_work, msecs_to_jiffies(9000));
#endif

#ifdef DUMP_ACM_PACKET
	printk("%s ACM packet dump in 8 sec \n", __func__);
	schedule_delayed_work(&acm_dump_work, msecs_to_jiffies(8000));
#endif

#ifdef DUMP_MUX_PACKET
	printk("%s MUX packet dump in 7 sec \n", __func__);
	schedule_delayed_work(&mux_dump_work, msecs_to_jiffies(7000));
#endif
 out:
	return IRQ_HANDLED;
}

/**
 *  mdm_ctrl_reset_it - Modem has changed reset state. RST_OUT_N
 *  @irq: IRQ number
 *  @data: mdm_ctrl driver reference
 *
 *  Change current state and schedule work to handle unexpected resets.
 */
static irqreturn_t mdm_ctrl_reset_it(int irq, void *data)
{
	int value, reset_ongoing;
	struct mdm_ctrl *drv = data;
	unsigned long flags;

	odin_dwc_host_set_pre_reset();

	pr_info(DRVNAME "mdm_ctrl_reset_it : Modem reset completed\n");
	value = drv->pdata->cpu.get_mdm_state(drv->pdata->cpu_data);
	check_2nd_boot = true;

	/* Ignoring event if we are in OFF state. */
	if (mdm_ctrl_get_state(drv) == MDM_CTRL_STATE_OFF) {
		/* Logging event in order to minimise risk of hidding bug */
		pr_err(DRVNAME ": RESET_OUT 0x%x while OFF\r\n", value);
#if defined(RECOVERY_BY_USB_DISCONNECTION)
		is_modem_reset = true;
#endif
		goto out;
	}

	/* If reset is ongoing we expect falling if applicable and rising
	 * edge.
	 */
	reset_ongoing = atomic_read(&drv->rst_ongoing);
	if (reset_ongoing) {
		pr_err(DRVNAME ": RESET_OUT 0x%x\r\n", value);

#if defined(CONFIG_MIGRATION_CTL_DRV)
		if (drv->bus_ctrl_reason == NONE) {
			drv->bus_ctrl_reason = HW_RESET;
			drv->bus_ctrl_set_state = RESET;
			queue_delayed_work(drv->bus_ctrl_wq, &drv->bus_ctrl_work, 0);
		} else {
			pr_err(DRVNAME ": Ignore RST_OUT BUS CTRL (%d / %d)\n",
				drv->bus_ctrl_reason, drv->bus_ctrl_set_state);
		}
#endif

		/* Rising EDGE (IPC ready) */
		if (value) {
#if defined(RECOVERY_BY_USB_DISCONNECTION)
			is_modem_reset = false;
			is_modem_coredump = false;
#endif
			/* Reset the reset ongoing flag */
			atomic_set(&drv->rst_ongoing, 0);

			pr_err(DRVNAME ": IPC READY !\r\n");
			mdm_ctrl_set_state(drv, MDM_CTRL_STATE_IPC_READY);

#ifdef ODIN_XMM_ADAPTATION
            is_cdc_being_attached = 1;
#endif
		}
#ifdef FEATURE_RECOVERY_CP_CRASH
		crash_by_gpio_status = 0;
#endif
		goto out;
	}

	pr_err(DRVNAME ": Unexpected RESET_OUT 0x%x\r\n", value);
#if 0
	if(gpio_get_value(98)){
		gpio_set_value(98, 0);
		printk("%s] force cp crash = [%s] \n",__func__, gpio_get_value(98)? "HIGH": "LOW");
	}
#endif
	// This is debugging code requested from modem BSP for modem H/W reset case
	printk("%s] Unexpected RESET_OUT - mdm_pwrdown_value(144) = [%s] \n",
							__func__, gpio_get_value(144)? "HIGH": "LOW");
	printk("%s] Unexpected RESET_OUT - mdm_reset_value(156) = [%s] \n",
							__func__, gpio_get_value(156)? "HIGH": "LOW");

#if defined(RECOVERY_BY_USB_DISCONNECTION)
	is_modem_reset = true;
#endif
	/* Unexpected reset received */
	atomic_set(&drv->rst_ongoing, 1);


	/* Set the reason & launch the work to handle the hangup */
	drv->hangup_causes |= MDM_CTRL_HU_RESET;
	queue_work(drv->hu_wq, &drv->hangup_work);

 out:
	return IRQ_HANDLED;
}

/**
 *  clear_hangup_reasons - Clear the hangup reasons flag
 */
static void clear_hangup_reasons(void)
{
	mdm_drv->hangup_causes = MDM_CTRL_NO_HU;
}

/**
 *  get_hangup_reasons - Hangup reason flag accessor
 */
static int get_hangup_reasons(void)
{
	return mdm_drv->hangup_causes;
}

/*****************************************************************************
 *
 * Char device functions
 *
 ****************************************************************************/

/**
 *  mdm_ctrl_dev_open - Manage device access
 *  @inode: The node
 *  @filep: Reference to file
 *
 *  Called when a process tries to open the device file
 */
static int mdm_ctrl_dev_open(struct inode *inode, struct file *filep)
{
	mutex_lock(&mdm_drv->lock);
	/* Only ONE instance of this device can be opened */
	if (mdm_ctrl_get_opened(mdm_drv)) {
		mutex_unlock(&mdm_drv->lock);
		return -EBUSY;
	}

	/* Save private data for futur use */
	filep->private_data = mdm_drv;

	/* Set the open flag */
	mdm_ctrl_set_opened(mdm_drv, 1);
	mutex_unlock(&mdm_drv->lock);
	return 0;
}

/**
 *  mdm_ctrl_dev_close - Reset open state
 *  @inode: The node
 *  @filep: Reference to file
 *
 *  Called when a process closes the device file.
 */
static int mdm_ctrl_dev_close(struct inode *inode, struct file *filep)
{
	struct mdm_ctrl *drv = filep->private_data;

	/* Set the open flag */
	mutex_lock(&drv->lock);
	mdm_ctrl_set_opened(drv, 0);
	mutex_unlock(&drv->lock);
	return 0;
}

/**
 *  mdm_ctrl_dev_ioctl - Process ioctl requests
 *  @filep: Reference to file that stores private data.
 *  @cmd: Command that should be executed.
 *  @arg: Command's arguments.
 *
 */
long mdm_ctrl_dev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	struct mdm_ctrl *drv = filep->private_data;
	struct mdm_ctrl_cmd cmd_params;
	long ret = 0;
	unsigned int mdm_state;
	unsigned int param;

	pr_info(DRVNAME ": ioctl request 0x%x received \r\n", cmd);
	mdm_state = mdm_ctrl_get_state(drv);

	switch (cmd) {
	case MDM_CTRL_POWER_OFF:
		if (drv->bus_ctrl_reason < HW_RESET) {
			pr_err(DRVNAME ": Ignore POWER_OFF while core dump (%d)\n",
				drv->bus_ctrl_set_state);
		} else {
			/* Unconditionnal power off */
			mdm_ctrl_power_off(drv);
		}
		break;

	case MDM_CTRL_POWER_ON:
		/* Only allowed when modem is OFF or in unkown state */
		if ((mdm_state == MDM_CTRL_STATE_OFF) ||
				(mdm_state == MDM_CTRL_STATE_UNKNOWN))
			mdm_ctrl_cold_boot(drv);
		else
			/* Specific log in COREDUMP state */
			if (mdm_state == MDM_CTRL_STATE_COREDUMP)
				pr_err(DRVNAME ": Power ON not allowed (coredump)");
			else
				pr_info(DRVNAME ": Powering on while already on");
		break;

	case MDM_CTRL_WARM_RESET:
		/* Allowed in any state unless OFF */
		if (mdm_state != MDM_CTRL_STATE_OFF)
			mdm_ctrl_normal_warm_reset(drv);
		else
			pr_err(DRVNAME ": Warm reset not allowed (Modem OFF)");
		break;

	case MDM_CTRL_FLASHING_WARM_RESET:
		/* Allowed in any state unless OFF */
		if (mdm_state != MDM_CTRL_STATE_OFF)
			mdm_ctrl_flashing_warm_reset(drv);
		else
			pr_err(DRVNAME ": Warm reset not allowed (Modem OFF)");
		break;

	case MDM_CTRL_COLD_RESET:
		/* Allowed in any state unless OFF */
		if (mdm_state != MDM_CTRL_STATE_OFF)
			mdm_ctrl_cold_reset(drv);
		else
			pr_err(DRVNAME ": Cold reset not allowed (Modem OFF)");
		break;

	case MDM_CTRL_SET_STATE:
		/* Read the user command params */
		ret = copy_from_user(&param, (void *)arg, sizeof(param));
		if (ret < 0) {
			pr_info(DRVNAME ": copy from user failed ret = %ld\r\n",
				ret);
			goto out;
		}

		/* Filtering states. Allow any state ? */
		param &=
		    (MDM_CTRL_STATE_OFF |
		     MDM_CTRL_STATE_COLD_BOOT |
		     MDM_CTRL_STATE_WARM_BOOT |
		     MDM_CTRL_STATE_COREDUMP |
		     MDM_CTRL_STATE_IPC_READY |
		     MDM_CTRL_STATE_FW_DOWNLOAD_READY);

		mdm_ctrl_set_state(drv, param);
		break;

	case MDM_CTRL_GET_STATE:
		/* Return supposed current state.
		 * Real state can be different.
		 */
		param = mdm_state;

		ret = copy_to_user((void __user *)arg, &param, sizeof(param));
		if (ret < 0) {
			pr_info(DRVNAME ": copy to user failed ret = %ld\r\n",
				ret);
			return ret;
		}
		break;

	case MDM_CTRL_WAIT_FOR_STATE:
		/* Actively wait for state untill timeout */
		ret = copy_from_user(&cmd_params,
				     (void __user *)arg, sizeof(cmd_params));
		if (ret < 0) {
			pr_info(DRVNAME ": copy from user failed ret = %ld\r\n",
				ret);
			break;
		}
		pr_err(DRVNAME ": WAIT_FOR_STATE 0x%x ! \r\n",
		       cmd_params.param);

		ret = wait_event_interruptible_timeout(drv->event,
						       mdm_ctrl_get_state(drv)
						       == cmd_params.param,
						       msecs_to_jiffies
						       (cmd_params.timeout));
		if (!ret)
			pr_err(DRVNAME ": WAIT_FOR_STATE timed out ! \r\n");
		break;

	case MDM_CTRL_GET_HANGUP_REASONS:
		/* Return last hangup reason. Can be cumulative
		 * if they were not cleared since last hangup.
		 */
		param = get_hangup_reasons();

		ret = copy_to_user((void __user *)arg, &param, sizeof(param));
		if (ret < 0) {
			pr_info(DRVNAME ": copy to user failed ret = %ld\r\n",
				ret);
			return ret;
		}
		break;

	case MDM_CTRL_CLEAR_HANGUP_REASONS:
		clear_hangup_reasons();
		break;

	case MDM_CTRL_SET_POLLED_STATES:
		/* Set state to poll on. */
		/* Read the user command params */
		ret = copy_from_user(&param, (void *)arg, sizeof(param));
		if (ret < 0) {
			pr_info(DRVNAME ": copy from user failed ret = %ld\r\n",
				ret);
			return ret;
		}
		printk("%s : polled_states=%d\n", __func__, param);
		drv->polled_states = param;
		/* Poll is active ? */
		if (waitqueue_active(&drv->wait_wq)) {
			mdm_state = mdm_ctrl_get_state(drv);
			/* Check if current state is awaited */
			if (mdm_state)
				drv->polled_state_reached = ((mdm_state & param)
							     == mdm_state);

			/* Waking up the wait work queue to handle any
			 * polled state reached.
			 */
			wake_up(&drv->wait_wq);
		} else {
			/* Assume that mono threaded client are probably
			 * not polling yet and that they are not interested
			 * in the current state. This state may change until
			 * they start the poll. May be an issue for some cases.
			 */
			drv->polled_state_reached = false;
		}

		pr_info(DRVNAME ": states polled = 0x%x\r\n",
			drv->polled_states);
		break;

#if defined(CONFIG_LGE_CRASH_HANDLER)
	case MDM_CTRL_SET_TRAPINFO:
		ret = copy_from_user(&trapinfo, (void*)arg, sizeof(trapinfo));
		if (ret < 0) {
			pr_info(DRVNAME ": copy from user failed ret = %ld\r\n",
				ret);
			return ret;
		}
		break;
#endif

	default:
		pr_err(DRVNAME ": ioctl command %x unknown\r\n", cmd);
		ret = -ENOIOCTLCMD;
	}

 out:
	return ret;
}

/**
 *  mdm_ctrl_dev_read - Device read function
 *  @filep: Reference to file
 *  @data: User data
 *  @count: Bytes read.
 *  @ppos: Reference to position in file.
 *
 *  Called when a process, which already opened the dev file, attempts to
 *  read from it. Not allowed.
 */
static ssize_t mdm_ctrl_dev_read(struct file *filep,
				 char __user * data,
				 size_t count, loff_t * ppos)
{
	pr_err(DRVNAME ": Nothing to read\r\n");
	return -EINVAL;
}

/**
 *  mdm_ctrl_dev_write - Device write function
 *  @filep: Reference to file
 *  @data: User data
 *  @count: Bytes read.
 *  @ppos: Reference to position in file.
 *
 *  Called when a process writes to dev file.
 *  Not allowed.
 */
static ssize_t mdm_ctrl_dev_write(struct file *filep,
				  const char __user * data,
				  size_t count, loff_t * ppos)
{
	pr_err(DRVNAME ": Nothing to write to\r\n");
	return -EINVAL;
}

/**
 *  mdm_ctrl_dev_poll - Poll function
 *  @filep: Reference to file storing private data
 *  @pt: Reference to poll table structure
 *
 *  Flush the change state workqueue to ensure there is no new state pending.
 *  Relaunch the poll wait workqueue.
 *  Return POLLHUP|POLLRDNORM if any of the polled states was reached.
 */
static unsigned int mdm_ctrl_dev_poll(struct file *filep,
				      struct poll_table_struct *pt)
{
	struct mdm_ctrl *drv = filep->private_data;
	unsigned int ret = 0;

	/* Wait event change */
	poll_wait(filep, &drv->wait_wq, pt);

	/* State notify */
	if (drv->polled_state_reached ||
	    (mdm_ctrl_get_state(drv) & drv->polled_states)) {

		drv->polled_state_reached = false;
		ret |= POLLHUP | POLLRDNORM;
		pr_info(DRVNAME ": POLLHUP occured. Current state = 0x%x\r\n",
			mdm_ctrl_get_state(drv));
	}

	return ret;
}

/**
 * Device driver file operations
 */
static const struct file_operations mdm_ctrl_ops = {
	.open = mdm_ctrl_dev_open,
	.read = mdm_ctrl_dev_read,
	.write = mdm_ctrl_dev_write,
	.poll = mdm_ctrl_dev_poll,
	.release = mdm_ctrl_dev_close,
	.unlocked_ioctl = mdm_ctrl_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mdm_ctrl_dev_ioctl
#endif
};


/**
 *  mdm_ctrl_module_init - initialises the Modem Control driver
 *
 */
static int mdm_ctrl_module_probe(struct platform_device *pdev)
{
	int ret, irq;
	struct mdm_ctrl *new_drv;

#if defined(ODIN_DWC_HOST_SET_CPU_AFFINITY)
	cpumask_var_t cpumask;
#endif

	pr_err(DRVNAME ": probing mdm_ctrl\n");

	/* Allocate modem struct data */
	new_drv = kzalloc(sizeof(struct mdm_ctrl), GFP_KERNEL);
	if (!new_drv) {
		pr_err(DRVNAME ": Out of memory(new_drv)");
		ret = -ENOMEM;
		goto out;
	}

	pr_info(DRVNAME ": Getting device infos\n");
	/* Pre-initialisation: Retrieve platform device data */
	mdm_ctrl_get_device_info(new_drv, pdev);

	/* HERE new_drv variable must contain all modem specifics
	   (mdm name, cpu name, pmic, GPIO, early power on/off */

	if (new_drv->is_mdm_ctrl_disabled) {
		/* KW fix can't happen. */
		if (unlikely(new_drv->pdata))
			kfree(new_drv->pdata);
		ret = -ENODEV;
		goto free_drv;
	}

	/* Initialization */
	mutex_init(&new_drv->lock);
	init_waitqueue_head(&new_drv->event);
	init_waitqueue_head(&new_drv->wait_wq);

	/* init spin lock */
	spin_lock_init(&mdm_lock);

	/* Create a high priority ordered workqueue to change modem state */

	INIT_WORK(&new_drv->hangup_work, mdm_ctrl_handle_hangup);

	/* Create a workqueue to manage hangup */
	new_drv->hu_wq = create_singlethread_workqueue(DRVNAME "-hu_wq");
	if (!new_drv->hu_wq) {
		pr_err(DRVNAME ": Unable to create control workqueue");
		ret = -EIO;
		goto free_drv;
	}

#if defined(CONFIG_MIGRATION_CTL_DRV)
	INIT_DELAYED_WORK(&new_drv->bus_ctrl_work, mdm_bus_ctrl_work);

	new_drv->bus_ctrl_wq = create_singlethread_workqueue(DRVNAME "bus_ctrl_wq");
	if (!new_drv->bus_ctrl_wq) {
		pr_err(DRVNAME ": Unable to create control bus control workqueue");
		ret = -EIO;
		goto free_drv;
	}

	new_drv->bus_ctrl_reason = NONE;
	new_drv->bus_ctrl_set_state = DONE;
#endif

	/* Register the device */
	ret = alloc_chrdev_region(&new_drv->tdev, 0, 1, MDM_BOOT_DEVNAME);
	if (ret) {
		pr_err(DRVNAME ": alloc_chrdev_region failed (err: %d)", ret);
		goto free_hu_wq;
	}

	new_drv->major = MAJOR(new_drv->tdev);
	cdev_init(&new_drv->cdev, &mdm_ctrl_ops);
	new_drv->cdev.owner = THIS_MODULE;

	ret = cdev_add(&new_drv->cdev, new_drv->tdev, 1);
	if (ret) {
		pr_err(DRVNAME ": cdev_add failed (err: %d)", ret);
		goto unreg_reg;
	}

	new_drv->class = class_create(THIS_MODULE, DRVNAME);
	if (IS_ERR(new_drv->class)) {
		pr_err(DRVNAME ": class_create failed (err: %d)", ret);
		ret = -EIO;
		goto del_cdev;
	}

	new_drv->dev = device_create(new_drv->class,
				     NULL,
				     new_drv->tdev, NULL, MDM_BOOT_DEVNAME);

	if (IS_ERR(new_drv->dev)) {
		pr_err(DRVNAME ": device_create failed (err: %ld)",
		       PTR_ERR(new_drv->dev));
		ret = -EIO;
		goto del_class;
	}

	mdm_ctrl_set_state(new_drv, MDM_CTRL_STATE_OFF);

#if defined(CONFIG_MIGRATION_CTL_DRV)
	if (new_drv->pdata->pm.init(pdev))
		goto del_dev;
#endif
	if (new_drv->pdata->mdm.init(new_drv->pdata->modem_data))
		goto del_dev;

	if (new_drv->pdata->cpu.init(new_drv->pdata->cpu_data))
		goto del_dev;

	if (new_drv->pdata->pmic.init(new_drv->pdata->pmic_data))
		goto del_dev;

	printk("get_irq_rst=%d / get_irq_cdump=%d / get_irq_wakeup=%d \n",
		new_drv->pdata->cpu.get_irq_rst(new_drv->pdata->cpu_data),
		new_drv->pdata->cpu.get_irq_cdump(new_drv->pdata->cpu_data),
		new_drv->pdata->cpu.get_irq_wakeup(new_drv->pdata->cpu_data));

#if defined(CONFIG_MIGRATION_CTL_DRV)
	/* RST_OUT_N */
	ret = request_threaded_irq(
				new_drv->pdata->cpu.get_irq_rst(new_drv->pdata->cpu_data),
				NULL,
				mdm_ctrl_reset_it,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"IPC_CP_RST_OUT_IRQ",
				new_drv);
	if (ret) {
		pr_err(DRVNAME ": IRQ request failed for GPIO (RST_OUT)");
		ret = -ENODEV;
		goto del_dev;
	}

	/* CORE DUMP */
	ret =
	    odin_gpio_sms_request_irq(
	    	new_drv->pdata->cpu.get_irq_cdump(new_drv->pdata->cpu_data),
			mdm_ctrl_coredump_it,
			IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND, DRVNAME,
			new_drv);
	if (ret) {
		pr_err(DRVNAME ": IRQ request failed for GPIO (CORE DUMP)");
		ret = -ENODEV;
		goto free_all;
	}

	/* HOST WAKEUP */
	ret = odin_gpio_sms_request_irq(
			new_drv->pdata->cpu.get_irq_wakeup(new_drv->pdata->cpu_data),
			mdm_ctrl_hostwakeup_it,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND,
			"IPC_AP_WAKE_IRQ",
			new_drv);
	if (ret) {
		pr_err(DRVNAME ": IRQ request failed for GPIO (HOST_WAKEUP)");
		ret = -ENODEV;
		goto free_all;
	}
	ret = enable_irq_wake(new_drv->pdata->cpu.get_irq_wakeup(new_drv->pdata->cpu_data));
	if (ret < 0)
		pr_err("%s: HOST_WAKEUP enable_irq_wake error.\n", __func__);

#if defined(ODIN_DWC_HOST_SET_CPU_AFFINITY)
	cpumask_clear(cpumask);
	cpumask_set_cpu(MCD_HOST_WAKEUP_CPU_AFFINITY_NUM, cpumask);
	irq_set_affinity(new_drv->pdata->cpu.get_irq_wakeup(new_drv->pdata->cpu_data), cpumask);
	printk("%s : HOST_WAKEUP : %d \n", __func__,
			new_drv->pdata->cpu.get_irq_wakeup(new_drv->pdata->cpu_data));
#endif

#else
	ret =
	    request_irq(new_drv->pdata->cpu.
			get_irq_rst(new_drv->pdata->cpu_data),  // get_gpio_irq_rst
			mdm_ctrl_reset_it,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
			IRQF_NO_SUSPEND, DRVNAME, new_drv);
	if (ret) {
		pr_err(DRVNAME ": IRQ request failed for GPIO (RST_OUT)");
		ret = -ENODEV;
		goto del_dev;
	}

	ret =
	    request_irq(new_drv->pdata->cpu.
			get_irq_cdump(new_drv->pdata->cpu_data),    // get_gpio_rst
			mdm_ctrl_coredump_it,
			IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND, DRVNAME,
			new_drv);
	if (ret) {
		pr_err(DRVNAME ": IRQ request failed for GPIO (CORE DUMP)");
		ret = -ENODEV;
		goto free_all;
	}
#endif

	/* Inti CP_PWR_DOWN_N */
	gpio_set_value(144, 1);

	/* Everything is OK */
	mdm_drv = new_drv;
	/* Init driver */
	init_timer(&mdm_drv->flashing_timer);

#if defined(CONFIG_MIGRATION_CTL_DRV)
	ret = device_create_file(&pdev->dev, &dev_attr_as_enable);
	if (ret)
		goto free_all;
	ret = device_create_file(&pdev->dev, &dev_attr_hsic_enable);
	if (ret)
		goto free_all;
	/* Workaround : Soft USB Disconnect by MMGR */
	ret = device_create_file(&pdev->dev, &dev_attr_soft_discon);
	if (ret)
		goto free_all;

#if defined(CONFIG_LGE_CRASH_HANDLER)
	ret = device_create_file(&pdev->dev, &dev_attr_force_panic);
	if (ret)
		goto free_all;
#endif

	mdm_class = class_create(THIS_MODULE, "mdm");
	mdm_sysfs = device_create(mdm_class, NULL, 0, NULL, "mdm_sysfs");

	ret = sysfs_create_link(&mdm_sysfs->kobj, &pdev->dev.kobj, DRVNAME);
	if (ret)
		goto free_all;

	INIT_DELAYED_WORK(&mdm_drv->second_enum_work, mdm_second_enum_work);
#endif

#if defined(RECOVERY_BY_USB_DISCONNECTION)
    INIT_DELAYED_WORK(&force_reset_work, force_mdm_ctrl_cold_reset);
#endif

#ifdef ODIN_XMM_ADAPTATION
	INIT_DELAYED_WORK(&delayed_cold_reset_work, delayed_cold_reset);
#endif

	/* Modem power off sequence */
#if defined(CONFIG_MIGRATION_CTL_DRV)
	/* Don't need PMIC operations for Odin. */
	pr_info(DRVNAME ": Probe is done.\n");
#else
	if (mdm_drv->pdata->pmic.get_early_pwr_off(mdm_drv->pdata->pmic_data))
		mdm_ctrl_power_off(mdm_drv);

	/* Modem cold boot sequence */
	if (mdm_drv->pdata->pmic.get_early_pwr_on(mdm_drv->pdata->pmic_data))
		mdm_ctrl_cold_boot(mdm_drv);
#endif
	return 0;

 free_all:
	free_irq(new_drv->pdata->cpu.get_irq_rst(new_drv->pdata->cpu_data),
		 NULL);
 del_dev:
	device_destroy(new_drv->class, new_drv->tdev);

 del_class:
	class_destroy(new_drv->class);

 del_cdev:
	cdev_del(&new_drv->cdev);

 unreg_reg:
	unregister_chrdev_region(new_drv->tdev, 1);

 free_hu_wq:
	destroy_workqueue(new_drv->hu_wq);

 free_drv:
	kfree(new_drv);

 out:
	return ret;
}

/**
 *  mdm_ctrl_module_exit - Frees the resources taken by the control driver
 */
static int mdm_ctrl_module_remove(struct platform_device *pdev)
{
	struct mdm_ctrl *new_drv;

	if (!mdm_drv)
		return 0;

	if (mdm_drv->is_mdm_ctrl_disabled)
		goto out;

#if defined(CONFIG_MIGRATION_CTL_DRV)
	if (!mdm_drv->pdata->pm.remove(pdev))
		goto out;
#endif

	/* Delete the modem hangup worqueue */
	destroy_workqueue(mdm_drv->hu_wq);

	/* Unregister the device */
	device_destroy(mdm_drv->class, mdm_drv->tdev);
	class_destroy(mdm_drv->class);
	cdev_del(&mdm_drv->cdev);
	unregister_chrdev_region(mdm_drv->tdev, 1);

	if (mdm_drv->pdata->cpu.get_irq_cdump(mdm_drv->pdata->cpu_data) > 0)
		free_irq(mdm_drv->pdata->cpu.
			 get_irq_cdump(mdm_drv->pdata->cpu_data), NULL);
	if (mdm_drv->pdata->cpu.get_irq_rst(mdm_drv->pdata->cpu_data) > 0)
		free_irq(mdm_drv->pdata->cpu.
			 get_irq_rst(mdm_drv->pdata->cpu_data), NULL);

	mdm_drv->pdata->mdm.cleanup(mdm_drv->pdata->modem_data);
	mdm_drv->pdata->cpu.cleanup(mdm_drv->pdata->cpu_data);
	mdm_drv->pdata->pmic.cleanup(mdm_drv->pdata->pmic_data);;

	del_timer(&mdm_drv->flashing_timer);
	mutex_destroy(&mdm_drv->lock);

	/* Unregister the device */
	device_destroy(mdm_drv->class, mdm_drv->tdev);
	class_destroy(mdm_drv->class);
	cdev_del(&mdm_drv->cdev);
	unregister_chrdev_region(mdm_drv->tdev, 1);

 out:
	/* Free the driver context */
	kfree(mdm_drv->pdata);
	kfree(mdm_drv);
	mdm_drv = NULL;
	return 0;
}

#if defined(CONFIG_MIGRATION_CTL_DRV)
static int mdm_ctrl_module_shutdown(struct platform_device *pdev)
{
	if (!mdm_drv)
		return 0;

	if (!mdm_drv->pdata->pm.shutdown(pdev))
		return -1;

	return 0;
}
#endif


/* FOR ACPI HANDLING */
static struct acpi_device_id mdm_ctrl_acpi_ids[] = {
	/* ACPI IDs here */
	{"MCD0001", 0},
	{}
};
MODULE_DEVICE_TABLE(acpi, mdm_ctrl_acpi_ids);

static const struct platform_device_id mdm_ctrl_id_table[] = {
	{DEVICE_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(platform, mdm_ctrl_id_table);

#if defined(CONFIG_MIGRATION_CTL_DRV)
static struct of_device_id odin_mdm_ctrl_match[] = {
	{ .compatible = "imc,xmm7260-hsic"},
};
#endif

static struct platform_driver mcd_driver = {
	.probe = mdm_ctrl_module_probe,
	.remove = mdm_ctrl_module_remove,
	.shutdown = mdm_ctrl_module_shutdown,
	.driver = {
		.name = "xmm7260-hsic",
		.owner = THIS_MODULE,
		.of_match_table=of_match_ptr(odin_mdm_ctrl_match),
#ifdef CONFIG_PM
		.pm = &mcd_pm_dev_pm_ops,
#endif
	},
};

static int __init mdm_ctrl_module_init(void)
{
	printk("%s\n", __func__);
#ifdef DUMP_IPC_PACKET
	INIT_DELAYED_WORK(&ipc_dump_work, print_ipc_dump_packet);
#endif
#ifdef DUMP_ACM_PACKET
	INIT_DELAYED_WORK(&acm_dump_work, print_acm_dump_packet);
#endif
#ifdef DUMP_MUX_PACKET
	INIT_DELAYED_WORK(&mux_dump_work, print_mux_dump_packet);
#endif
	return platform_driver_register(&mcd_driver);
}

static void __exit mdm_ctrl_module_exit(void)
{
	printk("%s\n", __func__);
	platform_driver_unregister(&mcd_driver);
}

module_init(mdm_ctrl_module_init);
module_exit(mdm_ctrl_module_exit);

/**
 *  mdm_ctrl_modem_reset - Reset modem
 *
 *  Debug and integration purpose.
 */
static int mdm_ctrl_modem_reset(const char *val, struct kernel_param *kp)
{
	if (mdm_drv)
		mdm_ctrl_normal_warm_reset(mdm_drv);
	return 0;
}

module_param_call(modem_reset, mdm_ctrl_modem_reset, NULL, NULL, 0644);

MODULE_AUTHOR("Faouaz Tenoutit <faouazx.tenoutit@intel.com>");
MODULE_AUTHOR("Frederic Berat <fredericx.berat@intel.com>");
MODULE_DESCRIPTION("Intel Modem control driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DEVICE_NAME);
