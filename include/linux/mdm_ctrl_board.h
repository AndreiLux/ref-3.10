/*
 * mdm_ctrl_board.h
 *
 * Header for the Modem control driver.
 *
 * Copyright (C) 2010, 2011 Intel Corporation. All rights reserved.
 *
 * Contact: Frederic BERAT <fredericx.berat@intel.com>
 *          Faouaz TENOUTIT <faouazx.tenoutit@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef __MDM_CTRL_BOARD_H__
#define __MDM_CTRL_BOARD_H__

//#include <asm/intel-mid.h>
#include <linux/module.h>
#include <linux/interrupt.h>

/* TODO : Need to change configuation. */
#define CONFIG_MIGRATION_CTL_DRV

#if 0 /* Need check platform device name */
#define DEVICE_NAME "modem_control"
#else
#define DEVICE_NAME "mdm_ctrl"
#endif
#define DRVNAME "mdm_ctrl"

/* Supported Modem IDs*/
enum {
	MODEM_UNSUP,
	MODEM_6260,
	MODEM_6268,
	MODEM_6360,
	MODEM_7160,
	MODEM_7260
};

/* Supported PMIC IDs*/
enum {
	PMIC_UNSUP,
	PMIC_MFLD,
	PMIC_CLVT,
	PMIC_MRFL,
	PMIC_BYT,
	PMIC_MOOR,
	PMIC_ODIN,
};

/* Supported CPU IDs*/
enum {
	CPU_UNSUP,
	CPU_PWELL,
	CPU_CLVIEW,
	CPU_TANGIER,
	CPU_VVIEW2,
	CPU_ANNIEDALE,
	CPU_ODIN
};

#if defined(CONFIG_MIGRATION_CTL_DRV)
enum {
	PM_UNSUP,
	PM_FLASHED,
	PM_FLASHELESS
};
#endif

struct mdm_ops {
	int	(*init) (void *data);
	int	(*cleanup) (void *data);
	int	(*get_cflash_delay) (void *data);
	int	(*get_wflash_delay) (void *data);
#if defined(CONFIG_MIGRATION_CTL_DRV)
	int (*power_on) (void *data, int gpio_rst, int gpio_pwr, int host_act, int pwr_dwn);
#else
	int	(*power_on) (void *data, int gpio_rst, int gpio_pwr);
#endif
	int	(*power_off) (void *data, int gpio_rst);
	int	(*warm_reset) (void *data, int gpio_rst);
};

struct cpu_ops {
	int	(*init) (void *data);
	int	(*cleanup) (void *data);
	int	(*get_mdm_state) (void *data);
	int	(*get_irq_cdump) (void *data);
	int	(*get_irq_rst) (void *data);
	int	(*get_gpio_rst) (void *data);
	int	(*get_gpio_pwr) (void *data);
#if defined(CONFIG_MIGRATION_CTL_DRV)
	int (*get_gpio_ap_wakeup) (void *data);
	int	(*get_irq_wakeup) (void *data);
	int	(*get_gpio_host_active) (void *data);
	int	(*get_gpio_pwr_down) (void *data);
#endif
};

struct pmic_ops {
	int	(*init) (void *data);
	int	(*cleanup) (void *data);
	int	(*power_on_mdm) (void *data);
	int	(*power_off_mdm) (void *data);
	int	(*get_early_pwr_on) (void *data);
	int	(*get_early_pwr_off) (void *data);
};

#if defined(CONFIG_MIGRATION_CTL_DRV)
struct pm_ops {
	int	(*init) (void *data);
	int	(*cleanup) (void *data);
	int (*remove) (void *device);
	int (*shutdown) (void *device);
	irqreturn_t	(*ap_wakeup_handler) (int irq, void *data);
};
#endif


struct mcd_base_info {
	/* modem infos */
	int		mdm_ver;
	struct	mdm_ops mdm;
	void	*modem_data;

	/* cpu infos */
	int		cpu_ver;
	struct	cpu_ops cpu;
	void	*cpu_data;

	/* pmic infos */
	int		pmic_ver;
	struct	pmic_ops pmic;
	void	*pmic_data;

#if defined(CONFIG_MIGRATION_CTL_DRV)
	/* pm infos */
	int 	pm_ver;
	struct	pm_ops pm;
	void	*pm_data;
#endif
};

#if defined(CONFIG_MIGRATION_CTL_DRV)
	/* Don't need SFI operations */
#else
struct sfi_to_mdm {
	char modem_name[SFI_NAME_LEN + 1];
	int modem_type;
};
#endif

/* GPIO names */
#define GPIO_RST_OUT	"ifx_mdm_rst_out"
#define GPIO_PWR_ON	"ifx_mdm_pwr_on"
#define GPIO_RST_BBN	"ifx_mdm_rst_pmu"
#define GPIO_CDUMP	"modem-gpio2"
#define GPIO_CDUMP_MRFL	"MODEM_CORE_DUMP"

/* For Odin gpio names in device tree. */
#if defined(CONFIG_MIGRATION_CTL_DRV)
#define PWR_DOWN_GPIO_NAME		"modem_pwr_down_gpio"
#define PWR_ON_GPIO_NAME		"pwr_on_gpio"
#define MODEM_RESET_GPIO_NAME	"modem_reset_gpio"
#define SLAVE_WAKEUP_GPIO_NAME	"slave_wakeup_gpio"
#define HOST_WAKEUP_GPIO_NAME	"host_wakeup_gpio"
#define HOST_ACTIVE_GPIO_NAME	"host_active_gpio"
#define FORCE_CRASH_GPIO_NAME	"force_crash_gpio"
#define RST_OUT_GPIO_NAME		"rst_out_gpio"
#define CORE_DUMP_GPIO_NAME		"core_dump_gpio"
#endif


/* Retrieve modem parameters on ACPI framework */
int retrieve_modem_platform_data(struct platform_device *pdev);

int mcd_register_mdm_info(struct mcd_base_info *info,
			  struct platform_device *pdev);

/* struct mcd_cpu_data
 * @gpio_rst_out: Reset out gpio (self reset indicator)
 * @gpio_pwr_on: Power on gpio (ON1 - Power up pin)
 * @gpio_rst_bbn: RST_BB_N gpio (Reset pin)
 * @gpio_cdump: CORE DUMP indicator
 * @irq_cdump: CORE DUMP irq
 * @irq_reset: RST_BB_N irq
 */
struct mdm_ctrl_cpu_data {
	/* GPIOs */
	char	*gpio_rst_out_name;
	int		gpio_rst_out;
	char	*gpio_pwr_on_name;
	int		gpio_pwr_on;
	char	*gpio_rst_bbn_name;
	int		gpio_rst_bbn;
	char	*gpio_cdump_name;
	int		 gpio_cdump;
#if defined(CONFIG_MIGRATION_CTL_DRV)
	char	*gpio_ap_wakeup_name;
	int		gpio_ap_wakeup;
	char	*gpio_slave_wakeup_name;
	int		gpio_slave_wakeup;
	char	*gpio_host_active_name;
	int		gpio_host_active;
	char	*gpio_modem_pwr_down_name;
	int		gpio_modem_pwr_down;
    char    *gpio_force_crash_name;
    int     gpio_force_crash;
#endif

	/* IRQs */
	int	irq_cdump;
	int	irq_reset;
#if defined(CONFIG_MIGRATION_CTL_DRV)
	int irq_wakeup;
#endif
};

/* struct mdm_ctrl_pmic_data
 * @chipctrl: PMIC base address
 * @chipctrlon: Modem power on PMIC value
 * @chipctrloff: Modem power off PMIC value
 * @early_pwr_on: call to power_on on probe indicator
 * @early_pwr_off: call to power_off on probe indicator
 * @pwr_down_duration:Powering down duration (us)
 */
struct mdm_ctrl_pmic_data {
	int		chipctrl;
	int		chipctrlon;
	int		chipctrloff;
	int		chipctrl_mask;
	bool	early_pwr_on;
	bool	early_pwr_off;
	int		pwr_down_duration;
};

/* struct mdm_ctrl_device_info - Board and modem infos
 *
 * @pre_on_delay:Delay before pulse on ON1 (us)
 * @on_duration:Pulse on ON1 duration (us)
 * @pre_wflash_delay:Delay before flashing window, after warm_reset (ms)
 * @pre_cflash_delay:Delay before flashing window, after cold_reset (ms)
 * @flash_duration:Flashing window durtion (ms)int  Not used ?
 * @warm_rst_duration:Warm reset duration (ms)
 */
struct mdm_ctrl_mdm_data {
	int	pre_on_delay;
	int	on_duration;
	int	pre_wflash_delay;
	int	pre_cflash_delay;
	int	flash_duration;
	int	warm_rst_duration;
	int	pre_pwr_down_delay;
};

#if defined(CONFIG_MIGRATION_CTL_DRV)
enum baseband_type {
	BASEBAND_XMM,
};

enum baseband_xmm_powerstate_t {
	BBXMM_PS_UNINIT	= 0,
	BBXMM_PS_INIT,

	BBXMM_PS_L0,
	BBXMM_PS_L0TOL2,
	BBXMM_PS_L0_AWAKE,

	BBXMM_PS_L2,
	BBXMM_PS_L2TOL0_FROM_AP,
	BBXMM_PS_L2_AWAKE,

	BBXMM_PS_L3,				/* BBXMM_PS_L3 = 8 */
	BBXMM_PS_L3TOL0_FROM_CP,
	BBXMM_PS_L3_WAKEREADY,		/* AP send wakeup signal to CP using SLAVE_WAKEUP */
	BBXMM_PS_L3_CP_WAKEUP,		/* CP is ready. It handles a HOST_WAKEUP gpio. */

	BBXMM_PS_LAST	= -1,
};

enum shutdown_state_t{
	SYSTEM_WAKEUP,
	SYSTEM_SHUTDOWN,
	CP_WAKEUP_IN_SHUTDOWN,
};

typedef struct baseband_power_platform_data {
	enum baseband_type baseband_type;
	enum baseband_xmm_powerstate_t pm_state;

	bool slave_wakeup;
	bool host_active;
	bool phy_ready;
	bool host_wakeup_low; /* CP is ready */

	struct workqueue_struct *pm_wq;
	struct work_struct pm_work;

	union {
		struct {
			int bb_pwr_down;
			int bb_rst;
			int bb_on;
			int ipc_bb_wake;
			int ipc_ap_wake;
			int ipc_hsic_active;
            int bb_force_crash;
			int ipc_cp_rst_out;
			int ipc_core_dump;
		} xmm;
	} modem;
} xmm_pm_drv_data_t;
#endif

#if defined(CONFIG_IMC_MODEM) || defined(CONFIG_IMC_DRV)
/* HSIC status notifier */
struct imc_hsic_ops {
	int	(*device_add) (void *data);
	int	(*device_remove) (void *data);
	int (*port_connect) (void *data);
	int (*port_disconnect) (void *data);
};
int register_hsic_notifier(struct imc_hsic_ops * ops);
#endif

#endif				/* __MDM_CTRL_BOARD_H__ */
