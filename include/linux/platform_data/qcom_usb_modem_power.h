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

#ifndef __MACH_QCOM_USB_MODEM_POWER_H
#define __MACH_QCOM_USB_MODEM_POWER_H

#include <linux/interrupt.h>
#include <linux/usb.h>
#include <linux/pm_qos.h>
#include <linux/wakelock.h>

/* modem private data structure */
#if defined(CONFIG_MDM_FTRACE_DEBUG) || defined(CONFIG_MDM_ERRMSG)
#define MDM_COM_BUF_SIZE 256
#endif

#ifdef CONFIG_MSM_SYSMON_COMM
#define RD_BUF_SIZE			100
#define MODEM_ERRMSG_LIST_LEN 10

struct mdm_msr_info {
	int valid;
	struct timespec msr_time;
	char modem_errmsg[RD_BUF_SIZE];
};
#endif

enum charm_boot_type {
	CHARM_NORMAL_BOOT = 0,
	CHARM_RAM_DUMPS,
	CHARM_CNV_RESET,
};

struct qcom_usb_modem {
	struct qcom_usb_modem_power_platform_data *pdata;
	struct platform_device *pdev;

	/* irq */
	unsigned int wake_cnt;	/* remote wakeup counter */
	unsigned int wake_irq;	/* remote wakeup irq */
	bool wake_irq_wakeable;
	unsigned int errfatal_irq;
	bool errfatal_irq_wakeable;
	unsigned int hsic_ready_irq;
	bool hsic_ready_irq_wakeable;
	unsigned int status_irq;
	bool status_irq_wakeable;
	unsigned int ipc3_irq;
	bool ipc3_irq_wakeable;
	unsigned int vdd_min_irq;
	bool vdd_min_irq_wakeable;
	bool mdm_irq_enabled;
	bool mdm_wake_irq_enabled;
	bool mdm_gpio_exported;

	/* mutex */
	struct mutex lock;
	struct mutex hc_lock;
	struct mutex hsic_phy_lock;
#ifdef CONFIG_MDM_FTRACE_DEBUG
	struct mutex ftrace_cmd_lock;
#endif

	/* wake lock */
	struct wake_lock wake_lock;	/* modem wake lock */

	/* usb */
	unsigned int vid;	/* modem vendor id */
	unsigned int pid;	/* modem product id */
	struct usb_device *udev;	/* modem usb device */
	struct usb_device *parent;	/* parent device */
	struct usb_interface *intf;	/* first modem usb interface */
	struct platform_device *hc;	/* USB host controller */
	struct notifier_block usb_notifier;	/* usb event notifier */
	struct notifier_block pm_notifier;	/* pm event notifier */
	int system_suspend;	/* system suspend flag */
	int short_autosuspend_enabled;

	/* workqueue */
	struct workqueue_struct *usb_host_wq;   /* Usb host workqueue */
	struct workqueue_struct *wq;	/* modem workqueue */
	struct workqueue_struct *mdm_recovery_wq; /* modem recovery workqueue */
#ifdef CONFIG_MDM_FTRACE_DEBUG
	struct workqueue_struct *ftrace_wq; /* ftrace workqueue */
#endif
#ifdef CONFIG_MSM_SYSMON_COMM
	struct workqueue_struct *mdm_restart_wq;
#endif

	/* work */
	struct work_struct host_reset_work;	/* usb host reset work */
	struct work_struct host_load_work;	/* usb host load work */
	struct work_struct host_unload_work;	/* usb host unload work */
	struct pm_qos_request cpu_boost_req; /* min CPU freq request */
	struct work_struct cpu_boost_work;	/* CPU freq boost work */
	struct delayed_work cpu_unboost_work;	/* CPU freq unboost work */
	struct work_struct mdm_hsic_ready_work; /* modem hsic ready work */
	struct work_struct mdm_status_work; /* modem status changed work */
	struct work_struct mdm_errfatal_work; /* modem errfatal work */
#ifdef CONFIG_MDM_FTRACE_DEBUG
	struct work_struct ftrace_enable_log_work; /* ftrace enable log work */
#endif
#ifdef CONFIG_MSM_SYSMON_COMM
	struct work_struct mdm_restart_reason_work; /* modem reset reason work */
#endif

	const struct qcom_modem_operations *ops;	/* modem operations */

	/* modem status */
	enum charm_boot_type boot_type;
	unsigned int mdm9k_status;
	struct proc_dir_entry *mdm9k_pde;
	unsigned short int mdm_status;
	unsigned int mdm2ap_ipc3_status;
	atomic_t final_efs_wait;
	struct completion usb_host_reset_done;

	/* modem debug */
	bool mdm_debug_on;
#ifdef CONFIG_MDM_FTRACE_DEBUG
	bool ftrace_enable;
	char ftrace_cmd[MDM_COM_BUF_SIZE];
	struct completion ftrace_cmd_pending;
	struct completion ftrace_cmd_can_be_executed;
#endif
#ifdef CONFIG_MSM_SYSMON_COMM
	struct mdm_msr_info msr_info_list[MODEM_ERRMSG_LIST_LEN];
	int mdm_msr_index;
#endif
#ifdef CONFIG_MDM_ERRMSG
	char mdm_errmsg[MDM_COM_BUF_SIZE];
#endif
#ifdef CONFIG_MSM_SUBSYSTEM_RESTART
	struct completion mdm_needs_reload;
	struct completion mdm_boot;
	struct completion mdm_ram_dumps;
#endif

	/* hsic wakeup */
	unsigned long mdm_hsic_phy_resume_jiffies;
	unsigned long mdm_hsic_phy_active_total_ms;
};

/* modem operations */
struct qcom_modem_operations {
	int (*init) (struct qcom_usb_modem *);	/* modem init */
	void (*start) (struct qcom_usb_modem *);	/* modem start */
	void (*stop) (struct qcom_usb_modem *);	/* modem stop */
	void (*stop2) (struct qcom_usb_modem *); /* modem stop 2 */
	void (*suspend) (void);	/* send L3 hint during system suspend */
	void (*resume) (void);	/* send L3->0 hint during system resume */
	void (*reset) (void);	/* modem reset */
	void (*remove) (struct qcom_usb_modem *); /* modem remove */
	void (*status_cb) (struct qcom_usb_modem *, int);
	void (*fatal_trigger_cb) (struct qcom_usb_modem *);
	void (*normal_boot_done_cb) (struct qcom_usb_modem *);
	void (*nv_write_done_cb) (struct qcom_usb_modem *);
	void (*debug_state_changed_cb) (struct qcom_usb_modem *, int);
	void (*dump_mdm_gpio_cb) (struct qcom_usb_modem *, int, char *); /* if gpio value is negtive, dump all gpio */
};

/* tegra usb modem power platform data */
struct qcom_usb_modem_power_platform_data {
	char *mdm_version;
	const struct qcom_modem_operations *ops;
	const struct usb_device_id *modem_list; /* supported modem list */
	unsigned mdm2ap_errfatal_gpio;
	unsigned ap2mdm_errfatal_gpio;
	unsigned mdm2ap_status_gpio;
	unsigned ap2mdm_status_gpio;
	unsigned mdm2ap_wakeup_gpio;
	unsigned ap2mdm_wakeup_gpio;
	unsigned mdm2ap_vdd_min_gpio;
	unsigned ap2mdm_vdd_min_gpio;
	unsigned mdm2ap_hsic_ready_gpio;
	unsigned ap2mdm_pmic_reset_n_gpio;
	unsigned ap2mdm_ipc1_gpio;
	unsigned ap2mdm_ipc2_gpio;
	unsigned mdm2ap_ipc3_gpio;
	unsigned long errfatal_irq_flags; /* modem error fatal irq flags */
	unsigned long status_irq_flags;  /* modem status irq flags */
	unsigned long wake_irq_flags;	/* remote wakeup irq flags */
	unsigned long vdd_min_irq_flags; /* vdd min irq flags */
	unsigned long hsic_ready_irq_flags; /* modem hsic ready irq flags */
	unsigned long ipc3_irq_flags; /* ipc3 irq flags */
	int autosuspend_delay;		/* autosuspend delay in milliseconds */
	int short_autosuspend_delay;	/* short autosuspend delay in ms */
	int ramdump_delay_ms;
	struct platform_device *tegra_ehci_device; /* USB host device */
	struct tegra_usb_platform_data *tegra_ehci_pdata;
};

/* MDM status bit mask definition */
#define MDM_STATUS_POWER_DOWN 0
#define MDM_STATUS_POWER_ON 1
#define MDM_STATUS_HSIC_READY (1 << 1)
#define MDM_STATUS_STATUS_READY (1 << 2)
#define MDM_STATUS_BOOT_DONE (1 << 3)
#define MDM_STATUS_RESET (1 << 4)
#define MDM_STATUS_RESETTING (1 << 5)
#define MDM_STATUS_RAMDUMP (1 << 6)
#define MDM_STATUS_EFFECTIVE_BIT 0x7f

#endif /* __MACH_QCOM_USB_MODEM_POWER_H */
