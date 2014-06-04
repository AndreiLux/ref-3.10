/*
 * arch/arm/mach-tegra/board-flounder-mdm9k.c
 *
 * Copyright (C) 2014 HTC Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "[MDM]: " fmt

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <mach/pinmux-t12.h>
#include <mach/pinmux.h>
#include <linux/platform_data/qcom_usb_modem_power.h>

#include "cpu-tegra.h"
#include "devices.h"
#include "board.h"
#include "board-common.h"
#include "board-flounder.h"
#include "tegra-board-id.h"
#include <mach/board_htc.h>

static struct gpio modem_gpios[] = { /* QCT 9K modem */
	{MDM2AP_ERRFATAL, GPIOF_IN, "MDM2AP_ERRFATAL"},
	{AP2MDM_ERRFATAL, GPIOF_OUT_INIT_LOW, "AP2MDM_ERRFATAL"},
	{MDM2AP_STATUS, GPIOF_IN, "MDM2AP_STATUS"},
	{AP2MDM_STATUS, GPIOF_OUT_INIT_LOW, "AP2MDM_STATUS"},
	{MDM2AP_WAKEUP, GPIOF_IN, "MDM2AP_WAKEUP"},
	{AP2MDM_WAKEUP, GPIOF_OUT_INIT_LOW, "AP2MDM_WAKEUP"},
	{MDM2AP_HSIC_READY, GPIOF_IN, "MDM2AP_HSIC_READY"},
	{AP2MDM_PMIC_RESET_N, GPIOF_OUT_INIT_LOW, "AP2MDM_PMIC_RESET_N"},
	{AP2MDM_IPC1, GPIOF_OUT_INIT_LOW, "AP2MDM_IPC1"},
	{MDM2AP_IPC3, GPIOF_IN, "MDM2AP_IPC3"},
	{AP2MDM_VDD_MIN, GPIOF_OUT_INIT_LOW, "AP2MDM_VDD_MIN"},
	{AP2MDM_IPC2, GPIOF_OUT_INIT_LOW, "AP2MDM_IPC2"},
	{MDM2AP_VDD_MIN, GPIOF_IN, "MDM2AP_VDD_MIN"},
	{AP2MDM_CHNL_RDY_CPU, GPIOF_OUT_INIT_LOW, "AP2MDM_CHNL_RDY_CPU"},
};

static void modem_dump_gpio_value(struct qcom_usb_modem *modem, int gpio_value, char *label);

static int modem_init(struct qcom_usb_modem *modem)
{
	int ret = 0;

	if (!modem) {
		pr_err("%s: mdm_drv = NULL\n", __func__);
		return -EINVAL;
	}

	ret = gpio_request_array(modem_gpios, ARRAY_SIZE(modem_gpios));
	if (ret) {
		pr_warn("%s:gpio request failed\n", __func__);
		return ret;
	}

	if (gpio_is_valid(modem->pdata->ap2mdm_status_gpio))
		gpio_direction_output(modem->pdata->ap2mdm_status_gpio, 0);
	else
		pr_err("gpio_request fail: ap2mdm_status_gpio\n");
	if (gpio_is_valid(modem->pdata->ap2mdm_errfatal_gpio))
		gpio_direction_output(modem->pdata->ap2mdm_errfatal_gpio, 0);
	else
		pr_err("gpio_request fail: ap2mdm_errfatal_gpio\n");
	if (gpio_is_valid(modem->pdata->ap2mdm_wakeup_gpio))
		gpio_direction_output(modem->pdata->ap2mdm_wakeup_gpio, 0);
	else
		pr_err("gpio_request fail: ap2mdm_wakeup_gpio\n");
	if (gpio_is_valid(modem->pdata->ap2mdm_ipc1_gpio))
		gpio_direction_output(modem->pdata->ap2mdm_ipc1_gpio, 0);
	else
		pr_err("gpio_request fail: ap2mdm_ipc1_gpio\n");

	if (gpio_is_valid(modem->pdata->mdm2ap_status_gpio))
		gpio_direction_input(modem->pdata->mdm2ap_status_gpio);
	else
		pr_err("gpio_request fail: mdm2ap_status_gpio\n");
	if (gpio_is_valid(modem->pdata->mdm2ap_errfatal_gpio))
		gpio_direction_input(modem->pdata->mdm2ap_errfatal_gpio);
	else
		pr_err("gpio_request fail: mdm2ap_errfatal_gpio\n");
	if (gpio_is_valid(modem->pdata->mdm2ap_wakeup_gpio))
		gpio_direction_input(modem->pdata->mdm2ap_wakeup_gpio);
	else
		pr_err("gpio_request fail: mdm2ap_wakeup_gpio\n");
	if (gpio_is_valid(modem->pdata->mdm2ap_hsic_ready_gpio))
		gpio_direction_input(modem->pdata->mdm2ap_hsic_ready_gpio);
	else
		pr_err("gpio_request fail: mdm2ap_hsic_ready_gpio\n");
	if (gpio_is_valid(modem->pdata->mdm2ap_ipc3_gpio ))
		gpio_direction_input(modem->pdata->mdm2ap_ipc3_gpio);
	else
		pr_err("gpio_request fail: mdm2ap_ipc3_gpio\n");

	/* export GPIO for user space access through sysfs */
	if (!modem->mdm_gpio_exported)
	{
		int idx;
		for (idx = 0; idx < ARRAY_SIZE(modem_gpios); idx++)
			gpio_export(modem_gpios[idx].gpio, true);
	}

	modem->mdm_gpio_exported = 1;

	return 0;
}

static void modem_power_on(struct qcom_usb_modem *modem)
{
	pr_info("+power_on_mdm\n");

	if (!modem) {
		pr_err("-%s: mdm_drv = NULL\n", __func__);
		return;
	}

	gpio_direction_output(modem->pdata->ap2mdm_wakeup_gpio, 0);

	/* Pull RESET gpio low and wait for it to settle. */
	pr_info("%s: Pulling RESET gpio low\n", __func__);

	gpio_direction_output(modem->pdata->ap2mdm_pmic_reset_n_gpio, 0);

	usleep_range(5000, 10000);

	/* Deassert RESET first and wait for it to settle. */
	pr_info("%s: Pulling RESET gpio high\n", __func__);

	gpio_direction_output(modem->pdata->ap2mdm_pmic_reset_n_gpio, 1);

	msleep(40);

	gpio_direction_output(modem->pdata->ap2mdm_status_gpio, 1);

	msleep(40);

	msleep(200);

	pr_info("-power_on_mdm\n");

	return;
}

#define MDM_HOLD_TIME		4000

static void modem_power_down(struct qcom_usb_modem *modem)
{
	pr_info("+power_down_mdm\n");

	if (!modem) {
		pr_err("%s-: mdm_drv = NULL\n", __func__);
		return;
	}

	/* APQ8064 only uses one pin to control pon and reset.
	 * If this pin is down over 300ms, memory data will loss.
	 * Currently sbl1 will help pull this pin up, and need 120~140ms.
	 * To decrease down time, we do not shut down MDM here and last until 8k PS_HOLD is pulled.
	 */
	pr_info("%s: Pulling RESET gpio LOW\n", __func__);

	if (modem->pdata->ap2mdm_pmic_reset_n_gpio >= 0)
		gpio_direction_output(modem->pdata->ap2mdm_pmic_reset_n_gpio, 0);

	msleep(MDM_HOLD_TIME);

	pr_info("-power_down_mdm\n");
}

static void modem_remove(struct qcom_usb_modem *modem)
{
	gpio_free_array(modem_gpios, ARRAY_SIZE(modem_gpios));

	modem->mdm_gpio_exported = 0;

	return;
}

static void modem_status_changed(struct qcom_usb_modem *modem, int value)
{
	if (!modem) {
		pr_err("%s-: mdm_drv = NULL\n", __func__);
		return;
	}

	if (modem->mdm_debug_on)
		pr_info("%s: value:%d\n", __func__, value);

	if (value && (modem->mdm_status & MDM_STATUS_BOOT_DONE)) {
		gpio_direction_output(modem->pdata->ap2mdm_wakeup_gpio, 1);
	}

	return;
}

static void modem_boot_done(struct qcom_usb_modem *modem)
{
	if (!modem) {
		pr_err("%s-: mdm_drv = NULL\n", __func__);
		return;
	}

	if (modem->mdm_status & (MDM_STATUS_BOOT_DONE | MDM_STATUS_STATUS_READY)) {
		gpio_direction_output(modem->pdata->ap2mdm_wakeup_gpio, 1);
	}

	return;
}

static void modem_nv_write_done(struct qcom_usb_modem *modem)
{
	gpio_direction_output(modem->pdata->ap2mdm_ipc1_gpio, 1);
	msleep(1);
	gpio_direction_output(modem->pdata->ap2mdm_ipc1_gpio, 0);

	return;
}

static void trigger_modem_errfatal(struct qcom_usb_modem *modem)
{
	pr_info("%s+\n", __func__);

	if (!modem) {
		pr_err("%s-: mdm_drv = NULL\n", __func__);
		return;
	}

	gpio_direction_output(modem->pdata->ap2mdm_errfatal_gpio, 0);
	msleep(1000);
	gpio_direction_output(modem->pdata->ap2mdm_errfatal_gpio, 1);
	msleep(1000);
	gpio_direction_output(modem->pdata->ap2mdm_errfatal_gpio, 0);

	pr_info("%s-\n", __func__);

	return;
}

static void modem_debug_state_changed(struct qcom_usb_modem *modem, int value)
{
	if (!modem) {
		pr_err("%s: mdm_drv = NULL\n", __func__);
		return;
	}

	modem->mdm_debug_on = value?true:false;

	return;
}

static void modem_dump_gpio_value(struct qcom_usb_modem *modem, int gpio_value, char *label)
{
	int ret;

	if (!modem) {
		pr_err("%s-: mdm_drv = NULL\n", __func__);
		return;
	}

	for (ret = 0; ret < ARRAY_SIZE(modem_gpios); ret++)
	{
		if (gpio_is_valid(gpio_value))
		{
			if (gpio_value == modem_gpios[ret].gpio)
			{
				pr_info("%s: %s = %d\n", label, modem_gpios[ret].label, gpio_get_value(gpio_value));
				break;
			}
		}
		else
			pr_info("%s: %s = %d\n", label, modem_gpios[ret].label, gpio_get_value(modem_gpios[ret].gpio));
	}

	return;
}

static const struct qcom_modem_operations mdm_operations = {
	.init = modem_init,
	.start = modem_power_on,
	.stop = modem_power_down,
	.stop2 = modem_power_down,
	.remove = modem_remove,
	.status_cb = modem_status_changed,
	.normal_boot_done_cb = modem_boot_done,
	.nv_write_done_cb = modem_nv_write_done,
	.fatal_trigger_cb = trigger_modem_errfatal,
	.debug_state_changed_cb = modem_debug_state_changed,
	.dump_mdm_gpio_cb = modem_dump_gpio_value,
};

struct tegra_usb_platform_data tegra_ehci2_hsic_modem_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.phy_intf = TEGRA_USB_PHY_INTF_HSIC,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.hot_plug = false,
		.remote_wakeup_supported = false,
		.power_off_on_suspend = true,
		.turn_off_vbus_on_lp0  = true,
	},
/* In flounder native design, the following are hard code
	.u_cfg.hsic = {
		.sync_start_delay = 9,
		.idle_wait_delay = 17,
		.term_range_adj = 0,
		.elastic_underrun_limit = 16,
		.elastic_overrun_limit = 16,
	},
*/
};

static struct qcom_usb_modem_power_platform_data mdm_pdata = {
	.mdm_version = "4.0",
	.ops = &mdm_operations,
	.mdm2ap_errfatal_gpio = MDM2AP_ERRFATAL,
	.ap2mdm_errfatal_gpio = AP2MDM_ERRFATAL,
	.mdm2ap_status_gpio = MDM2AP_STATUS,
	.ap2mdm_status_gpio = AP2MDM_STATUS,
	.mdm2ap_wakeup_gpio = MDM2AP_WAKEUP,
	.ap2mdm_wakeup_gpio = AP2MDM_WAKEUP,
	.mdm2ap_vdd_min_gpio = -1,
	.ap2mdm_vdd_min_gpio = -1,
	.mdm2ap_hsic_ready_gpio = MDM2AP_HSIC_READY,
	.ap2mdm_pmic_reset_n_gpio = AP2MDM_PMIC_RESET_N,
	.ap2mdm_ipc1_gpio = AP2MDM_IPC1,
	.ap2mdm_ipc2_gpio = -1,
	.mdm2ap_ipc3_gpio = MDM2AP_IPC3,
	.errfatal_irq_flags = IRQF_TRIGGER_RISING | IRQF_ONESHOT,
	.status_irq_flags = IRQF_TRIGGER_RISING |
				    IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
	.wake_irq_flags = IRQF_TRIGGER_RISING | IRQF_ONESHOT,
	.vdd_min_irq_flags = -1,
	.hsic_ready_irq_flags = IRQF_TRIGGER_RISING |
				    IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
	.ipc3_irq_flags = IRQF_TRIGGER_RISING |
				    IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
	.autosuspend_delay = 2000,
	.short_autosuspend_delay = 50,
	.ramdump_delay_ms = 2000,
	.tegra_ehci_device = &tegra_ehci2_device,
	.tegra_ehci_pdata = &tegra_ehci2_hsic_modem_pdata,
};

static struct platform_device qcom_mdm_device = {
	.name = "qcom_usb_modem_power",
	.id = -1,
	.dev = {
		.platform_data = &mdm_pdata,
	},
};

int __init flounder_mdm_9k_init(void)
{
/*
	We add echi2 platform data here. In native design, it may
	be added in flounder_usb_init().
*/
	if (is_mdm_modem()) {
		pr_info("%s: add mdm_devices\n", __func__);
		tegra_ehci2_device.dev.platform_data = &tegra_ehci2_hsic_modem_pdata;
		platform_device_register(&qcom_mdm_device);
	}

	return 0;
}
