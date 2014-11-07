/*
 * linux/drivers/modem_control/mcd_cpu.c
 *
 * Version 1.0
 * This code permits to access the cpu specifics
 * of each supported platform.
 * among other things, it permits to configure and access gpios
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Contact: Ranquet Guillaume <guillaumex.ranquet@intel.com>
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
#include <linux/mdm_ctrl_board.h>

/**
 * mdm_ctrl_configure_gpio - Configure GPIOs
 * @gpio: GPIO to configure
 * @direction: GPIO direction - 0: IN | 1: OUT
 *
 */
static inline int mdm_ctrl_configure_gpio(int gpio,
					  int direction,
					  int value, const char *desc)
{
	int ret;

	ret = gpio_request(gpio, "ModemControl");

	if (direction)
		ret += gpio_direction_output(gpio, value);
	else
		ret += gpio_direction_input(gpio);

	if (ret) {
		pr_err(DRVNAME ": Unable to configure GPIO%d (%s)", gpio, desc);
		ret = -ENODEV;
	}

	return ret;
}

int cpu_init_gpio(void *data)
{
	struct mdm_ctrl_cpu_data *cpu_data = data;
	int ret;

	pr_info("%s : cpu_init\n", __func__);

#if defined(CONFIG_MIGRATION_CTL_DRV)
	/* Already initialized on mcd_pm */
	cpu_data->irq_reset = gpio_to_irq(cpu_data->gpio_rst_out);
	if (cpu_data->irq_reset < 0) {
		goto free_ctx3;
	}

	cpu_data->irq_cdump = gpio_to_irq(cpu_data->gpio_cdump);
	if (cpu_data->irq_cdump < 0) {
		goto free_ctx1;
	}

	cpu_data->irq_wakeup = gpio_to_irq(cpu_data->gpio_ap_wakeup);
	if (cpu_data->irq_wakeup < 0) {
		goto free_ctx6;
	}

	ret = gpio_direction_output(cpu_data->gpio_modem_pwr_down, 1);
	if (ret) {
		pr_err(DRVNAME ": Failed to configure GPIO%d\n",
			cpu_data->gpio_modem_pwr_down);
		goto out;
	}

	ret = gpio_direction_output(cpu_data->gpio_rst_bbn, 0);
	if (ret) {
		pr_err(DRVNAME ": Failed to configure GPIO%d\n",
			cpu_data->gpio_rst_out);
		goto out;
	}

	ret = gpio_direction_output(cpu_data->gpio_pwr_on, 0);
	if (ret) {
		pr_err(DRVNAME ": Failed to configure GPIO%d\n",
			cpu_data->gpio_pwr_on);
		goto out;
	}
#else
	/* Configure the RESET_BB gpio */
	ret = mdm_ctrl_configure_gpio(156, 1, 0, "RST_BB");
	if (ret)
		goto out;

	/* Configure the ON gpio */
	ret = mdm_ctrl_configure_gpio(148, 1, 0, "ON");
	if (ret)
		goto free_ctx5;

	/* Configure the RESET_OUT gpio & irq */
	ret = mdm_ctrl_configure_gpio(145, 0, 0, "RST_OUT");
	if (ret)
		goto free_ctx4;
	cpu_data->irq_reset = gpio_to_irq(cpu_data->gpio_rst_out);
	if (cpu_data->irq_reset < 0) {
		goto free_ctx3;
	}

	/* Configure the CORE_DUMP gpio & irq */
	ret = mdm_ctrl_configure_gpio(119, 0, 0, "CORE_DUMP");
	if (ret)
		goto free_ctx2;

	cpu_data->irq_cdump = gpio_to_irq(cpu_data->gpio_cdump);
	if (cpu_data->irq_cdump < 0) {
		goto free_ctx1;
	}
#endif

	pr_info(DRVNAME
		": GPIO (rst_bbn: %d, pwr_on: %d, rst_out: %d, fcdp_rb: %d)\n",
		cpu_data->gpio_rst_bbn, cpu_data->gpio_pwr_on,
		cpu_data->gpio_rst_out, cpu_data->gpio_cdump);

	return 0;

 free_ctx1:
	gpio_free(cpu_data->gpio_cdump);
 free_ctx2:
	if (cpu_data->irq_reset > 0)
		free_irq(cpu_data->irq_reset, cpu_data);
	cpu_data->irq_reset = 0;
 free_ctx3:
	gpio_free(cpu_data->gpio_rst_out);
 free_ctx4:
	gpio_free(cpu_data->gpio_pwr_on);
 free_ctx5:
	gpio_free(cpu_data->gpio_rst_bbn);
#if defined(CONFIG_MIGRATION_CTL_DRV)
 free_ctx6:
	gpio_free(cpu_data->gpio_ap_wakeup);
#endif
 out:
	return -ENODEV;
}

int cpu_cleanup_gpio(void *data)
{
	struct mdm_ctrl_cpu_data *cpu_data = data;

#if defined(CONFIG_MIGRATION_CTL_DRV)
	/* Already released on mcd_pm */
#else
	gpio_free(cpu_data->gpio_cdump);
	gpio_free(cpu_data->gpio_rst_out);
	gpio_free(cpu_data->gpio_pwr_on);
	gpio_free(cpu_data->gpio_rst_bbn);
#endif

	cpu_data->irq_cdump = 0;
	cpu_data->irq_reset = 0;
#if defined(CONFIG_MIGRATION_CTL_DRV)
	cpu_data->irq_wakeup = 0;
#endif
	return 0;
}

int get_gpio_irq_cdump(void *data)
{
	struct mdm_ctrl_cpu_data *cpu_data = data;
	return cpu_data->irq_cdump;
}

int get_gpio_irq_rst(void *data)
{
	struct mdm_ctrl_cpu_data *cpu_data = data;
	return cpu_data->irq_reset;
}

int get_gpio_mdm_state(void *data)
{
	struct mdm_ctrl_cpu_data *cpu_data = data;
	return gpio_get_value(cpu_data->gpio_rst_out);
}

int get_gpio_rst(void *data)
{
	struct mdm_ctrl_cpu_data *cpu_data = data;
	return cpu_data->gpio_rst_bbn;
}

int get_gpio_pwr(void *data)
{
	struct mdm_ctrl_cpu_data *cpu_data = data;
	return cpu_data->gpio_pwr_on;
}

#if defined(CONFIG_MIGRATION_CTL_DRV)
/* host wakeup gpio number */
int get_gpio_ap_wakeup(void *data)
{
	struct mdm_ctrl_cpu_data *cpu_data = data;
	return cpu_data->gpio_ap_wakeup;
}

/* host wakeup interrupt */
int get_irq_wakeup(void *data)
{
	struct mdm_ctrl_cpu_data *cpu_data = data;
	return cpu_data->irq_wakeup;
}

/* host active gpio number */
int get_gpio_host_active(void *data)
{
	struct mdm_ctrl_cpu_data *cpu_data = data;
	return cpu_data->gpio_host_active;
}

int get_gpio_pwr_down(void *data)
{
	struct mdm_ctrl_cpu_data *cpu_data = data;
	return cpu_data->gpio_modem_pwr_down;
}
#endif
