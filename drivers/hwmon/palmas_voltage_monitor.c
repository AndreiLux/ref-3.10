/*
 * palmas_voltage_monitor.c
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

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/mfd/palmas.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/battery_system_voltage_monitor.h>
#include <linux/power_supply.h>
#ifdef CONFIG_CABLE_VBUS_MONITOR
#include <linux/cable_vbus_monitor.h>
#endif

struct palmas_voltage_monitor_dev {
	int id;
	int irq;
	int reg;
	int reg_enable_bit;
	int monitor_volt_mv;
	int monitor_on;
	int (*notification)(unsigned int voltage);
};

struct palmas_voltage_monitor {
	struct palmas *palmas;
	struct device *dev;
	struct power_supply v_monitor;

	bool use_vbat_monitor;
	bool use_vsys_monitor;

	struct palmas_voltage_monitor_dev vbat_mon_dev;
	struct palmas_voltage_monitor_dev vsys_mon_dev;

	struct mutex mutex;

#ifdef CONFIG_CABLE_VBUS_MONITOR
	bool has_vbus_latched_cb;
#endif
};

enum {
	VBAT_MON_DEV,
	VSYS_MON_DEV,
};

#ifdef CONFIG_CABLE_VBUS_MONITOR
static int palmas_voltage_monitor_is_vbus_latched(void *data)
{
	struct palmas_voltage_monitor *monitor = data;
	int ret;
	unsigned int val;

	if (!monitor)
		return -EINVAL;

	ret = palmas_read(monitor->palmas, PALMAS_USB_OTG_BASE,
			PALMAS_USB_VBUS_INT_LATCH_SET, &val);
	if (ret < 0) {
		dev_err(monitor->dev,
			"USB_VBUS_INIT_LATCH_SET read fail, ret=%d\n", ret);
		return 0;
	}

	ret = palmas_write(monitor->palmas, PALMAS_USB_OTG_BASE,
			PALMAS_USB_VBUS_INT_LATCH_CLR,
			PALMAS_USB_VBUS_INT_LATCH_SET_VA_VBUS_VLD);
	if (ret < 0)
		dev_err(monitor->dev,
			"USB_VBUS_INIT_LATCH_CLR write fail, ret=%d\n", ret);

	return !!(val & PALMAS_USB_VBUS_INT_LATCH_SET_VA_VBUS_VLD);
}
#endif

static inline struct palmas_voltage_monitor_dev *get_monitor_dev_by_id(
	struct palmas_voltage_monitor *monitor,
	int monitor_id)
{

	if (monitor_id == VBAT_MON_DEV) {
		if (monitor->use_vbat_monitor)
			return &monitor->vbat_mon_dev;
	} else {
		if (monitor->use_vsys_monitor)
			return &monitor->vsys_mon_dev;
	}

	return NULL;
}

static int palmas_voltage_monitor_listener_register(
	struct palmas_voltage_monitor *monitor,	int monitor_id,
	int (*notification)(unsigned int voltage))
{
	struct palmas_voltage_monitor_dev *mon_dev = NULL;
	int ret = 0;

	if (!monitor || !notification)
		return -EINVAL;

	mon_dev = get_monitor_dev_by_id(monitor, monitor_id);

	if (!mon_dev)
		return -ENODEV;

	mutex_lock(&monitor->mutex);
	if (mon_dev->notification)
		ret = -EEXIST;
	else
		mon_dev->notification = notification;
	mutex_unlock(&monitor->mutex);

	return ret;
}

static void palmas_voltage_monitor_listener_unregister(
	struct palmas_voltage_monitor *monitor, int monitor_id)
{
	struct palmas_voltage_monitor_dev *mon_dev;
	int ret;

	if (!monitor)
		return;

	mon_dev = get_monitor_dev_by_id(monitor, monitor_id);

	if (!mon_dev)
		return;

	mutex_lock(&monitor->mutex);
	disable_irq(mon_dev->irq);
	mon_dev->monitor_volt_mv = 0;
	mon_dev->monitor_on = false;

	ret = palmas_write(monitor->palmas, PALMAS_PMU_CONTROL_BASE,
							mon_dev->reg, 0);
	if (ret < 0)
		dev_err(monitor->dev, "palmas write fail, ret=%d\n", ret);

	mon_dev->notification = NULL;
	mutex_unlock(&monitor->mutex);
}

#define PALMAS_MON_THRESHOLD_MV_MIN	(2300)
#define PALMAS_MON_THRESHOLD_MV_MAX	(4600)
#define PALMAS_MON_BITS_MIN		(0x06)
#define PALMAS_MON_BITS_MAX		(0x34)
#define PALMAS_MON_MV_STEP_PER_BIT	(50)
#define PALMAS_MON_ENABLE_BIT	(4600)
static int palmas_voltage_monitor_voltage_monitor_on_once(
	struct palmas_voltage_monitor *monitor, int monitor_id,
	unsigned int voltage)
{
	unsigned int bits;
	struct palmas_voltage_monitor_dev *mon_dev;

	if (!monitor)
		return -EINVAL;

	mon_dev = get_monitor_dev_by_id(monitor, monitor_id);

	if (!mon_dev)
		return 0;

	mutex_lock(&monitor->mutex);
	if (mon_dev->notification && !mon_dev->monitor_on) {
		mon_dev->monitor_volt_mv = voltage;

		if (voltage <= PALMAS_MON_THRESHOLD_MV_MIN)
			bits = PALMAS_MON_BITS_MIN;
		else if (voltage >= PALMAS_MON_THRESHOLD_MV_MAX)
			bits = PALMAS_MON_BITS_MAX;
		else {
			bits = PALMAS_MON_BITS_MIN +
				(voltage - PALMAS_MON_THRESHOLD_MV_MIN) /
				PALMAS_MON_MV_STEP_PER_BIT;
		}
		bits |= mon_dev->reg_enable_bit;

		palmas_write(monitor->palmas, PALMAS_PMU_CONTROL_BASE,
				mon_dev->reg, bits);

		mon_dev->monitor_on = true;
		enable_irq(mon_dev->irq);
	}
	mutex_unlock(&monitor->mutex);

	return 0;
}

static void palmas_voltage_monitor_voltage_monitor_off(
	struct palmas_voltage_monitor *monitor, int monitor_id)
{
	struct palmas_voltage_monitor_dev *mon_dev;

	if (!monitor)
		return;

	mon_dev = get_monitor_dev_by_id(monitor, monitor_id);

	if (!mon_dev)
		return;

	mutex_lock(&monitor->mutex);
	if (mon_dev->monitor_on) {
		disable_irq(mon_dev->irq);
		mon_dev->monitor_volt_mv = 0;
		mon_dev->monitor_on = false;

		palmas_write(monitor->palmas, PALMAS_PMU_CONTROL_BASE,
			mon_dev->reg, 0);
	}
	mutex_unlock(&monitor->mutex);
}

static irqreturn_t palmas_vbat_mon_irq_handler(int irq,
					void *_palmas_voltage_monitor)
{
	struct palmas_voltage_monitor *monitor = _palmas_voltage_monitor;
	unsigned int vbat_mon_line_state;
	int ret;

	ret = palmas_read(monitor->palmas, PALMAS_INTERRUPT_BASE,
		PALMAS_INT1_LINE_STATE, &vbat_mon_line_state);
	if (ret < 0)
		dev_err(monitor->dev, "INT1_LINE_STATE read fail, ret=%d\n",
					ret);
	else
		dev_dbg(monitor->dev, "vbat-mon-irq() INT1_LINE_STATE 0x%02x\n",
			vbat_mon_line_state);

	mutex_lock(&monitor->mutex);
	if (monitor->vbat_mon_dev.monitor_on) {
		disable_irq_nosync(monitor->vbat_mon_dev.irq);
		if (monitor->vbat_mon_dev.notification)
			monitor->vbat_mon_dev.notification(
				monitor->vbat_mon_dev.monitor_volt_mv);
		monitor->vbat_mon_dev.monitor_on = false;
	}
	power_supply_changed(&monitor->v_monitor);
	mutex_unlock(&monitor->mutex);

	return IRQ_HANDLED;
}

static irqreturn_t palmas_vsys_mon_irq_handler(int irq,
					void *_palmas_voltage_monitor)
{
	struct palmas_voltage_monitor *monitor = _palmas_voltage_monitor;
	unsigned int vsys_mon_line_state;
	int ret;

	ret = palmas_read(monitor->palmas, PALMAS_INTERRUPT_BASE,
		PALMAS_INT1_LINE_STATE, &vsys_mon_line_state);
	if (ret < 0)
		dev_err(monitor->dev, "INT1_LINE_STATE read fail, ret=%d\n",
					ret);
	else
		dev_dbg(monitor->dev, "vsys-mon-irq() INT1_LINE_STATE 0x%02x\n",
			vsys_mon_line_state);

	mutex_lock(&monitor->mutex);
	if (monitor->vsys_mon_dev.monitor_on) {
		disable_irq_nosync(monitor->vsys_mon_dev.irq);
		if (monitor->vsys_mon_dev.notification)
			monitor->vsys_mon_dev.notification(
				monitor->vsys_mon_dev.monitor_volt_mv);
		monitor->vsys_mon_dev.monitor_on = false;
	}
	mutex_unlock(&monitor->mutex);

	return IRQ_HANDLED;
}

static int palmas_voltage_monitor_vbat_listener_register(
	int (*notification)(unsigned int voltage), void *data)
{
	struct palmas_voltage_monitor *monitor = data;

	return palmas_voltage_monitor_listener_register(
		monitor, VBAT_MON_DEV, notification);
}

static void palmas_voltage_monitor_vbat_listener_unregister(void *data)
{
	struct palmas_voltage_monitor *monitor = data;

	palmas_voltage_monitor_listener_unregister(monitor, VBAT_MON_DEV);
}

static int palmas_voltage_monitor_vbat_monitor_on_once(
	unsigned int voltage, void *data)
{
	struct palmas_voltage_monitor *monitor = data;

	return palmas_voltage_monitor_voltage_monitor_on_once(
			monitor, VBAT_MON_DEV, voltage);
}

static void palmas_voltage_monitor_vbat_monitor_off(void *data)
{
	struct palmas_voltage_monitor *monitor = data;

	return palmas_voltage_monitor_voltage_monitor_off(monitor,
								VBAT_MON_DEV);
}

struct battery_system_voltage_monitor_worker_operations vbat_monitor_ops = {
	.monitor_on_once = palmas_voltage_monitor_vbat_monitor_on_once,
	.monitor_off = palmas_voltage_monitor_vbat_monitor_off,
	.listener_register = palmas_voltage_monitor_vbat_listener_register,
	.listener_unregister = palmas_voltage_monitor_vbat_listener_unregister,
};

struct battery_system_voltage_monitor_worker vbat_monitor_worker = {
	.ops = &vbat_monitor_ops,
	.data = NULL,
};

static ssize_t voltage_monitor_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int voltage;
	struct palmas_voltage_monitor *pvm;

	pvm = dev_get_drvdata(dev);
	sscanf(buf, "%u", &voltage);
	if (voltage == 0)
		palmas_voltage_monitor_vbat_monitor_off(pvm);
	else
		palmas_voltage_monitor_vbat_monitor_on_once(voltage, pvm);
	return count;
}

static DEVICE_ATTR(voltage_monitor, S_IWUSR | S_IWGRP, NULL,
							voltage_monitor_store);

static int palmas_voltage_monitor_probe(struct platform_device *pdev)
{
	struct palmas *palmas = dev_get_drvdata(pdev->dev.parent);
	struct palmas_platform_data *pdata;
	struct palmas_voltage_monitor_platform_data *vapdata = NULL;
	struct device_node *node = pdev->dev.of_node;
	struct palmas_voltage_monitor *palmas_voltage_monitor;
	int status, ret;

	palmas_voltage_monitor = devm_kzalloc(&pdev->dev,
				sizeof(*palmas_voltage_monitor), GFP_KERNEL);
	if (!palmas_voltage_monitor)
		return -ENOMEM;

	pdata = dev_get_platdata(pdev->dev.parent);
	if (pdata)
		vapdata = pdata->voltage_monitor_pdata;

	if (node && !vapdata) {
		palmas_voltage_monitor->use_vbat_monitor =
			of_property_read_bool(node, "ti,use-vbat-monitor");
		palmas_voltage_monitor->use_vsys_monitor =
			of_property_read_bool(node, "ti,use-vsys-monitor");
	} else {
		palmas_voltage_monitor->use_vbat_monitor = true;
		palmas_voltage_monitor->use_vsys_monitor = true;

		if (vapdata) {
			palmas_voltage_monitor->use_vbat_monitor =
					vapdata->use_vbat_monitor;
			palmas_voltage_monitor->use_vsys_monitor =
					vapdata->use_vsys_monitor;
		}
	}

	mutex_init(&palmas_voltage_monitor->mutex);
	dev_set_drvdata(&pdev->dev, palmas_voltage_monitor);

	palmas_voltage_monitor->palmas	= palmas;
	palmas_voltage_monitor->dev	= &pdev->dev;

	palmas_voltage_monitor->vbat_mon_dev.id = VBAT_MON_DEV;
	palmas_voltage_monitor->vbat_mon_dev.irq =
			palmas_irq_get_virq(palmas, PALMAS_VBAT_MON_IRQ);
	palmas_voltage_monitor->vbat_mon_dev.reg = PALMAS_VBAT_MON;
	palmas_voltage_monitor->vbat_mon_dev.reg_enable_bit =
			PALMAS_VBAT_MON_ENABLE;
	palmas_voltage_monitor->vbat_mon_dev.notification = NULL;

	palmas_voltage_monitor->vsys_mon_dev.id = VSYS_MON_DEV;
	palmas_voltage_monitor->vsys_mon_dev.irq =
			palmas_irq_get_virq(palmas, PALMAS_VSYS_MON_IRQ);
	palmas_voltage_monitor->vsys_mon_dev.reg = PALMAS_VSYS_MON;
	palmas_voltage_monitor->vsys_mon_dev.reg_enable_bit =
			PALMAS_VSYS_MON_ENABLE;
	palmas_voltage_monitor->vsys_mon_dev.notification = NULL;


	if (palmas_voltage_monitor->use_vbat_monitor) {
		status = devm_request_threaded_irq(palmas_voltage_monitor->dev,
				palmas_voltage_monitor->vbat_mon_dev.irq,
				NULL, palmas_vbat_mon_irq_handler,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
				IRQF_ONESHOT | IRQF_EARLY_RESUME,
				"palmas_vbat_mon", palmas_voltage_monitor);
		if (status < 0) {
			dev_err(&pdev->dev, "can't get IRQ %d, err %d\n",
				palmas_voltage_monitor->vbat_mon_dev.irq,
				status);
		} else {
			palmas_voltage_monitor->vbat_mon_dev.monitor_on = false;
			disable_irq(palmas_voltage_monitor->vbat_mon_dev.irq);
		}
		vbat_monitor_worker.data = palmas_voltage_monitor;
		battery_voltage_monitor_worker_register(&vbat_monitor_worker);
	}

	if (palmas_voltage_monitor->use_vsys_monitor) {
		status = devm_request_threaded_irq(palmas_voltage_monitor->dev,
				palmas_voltage_monitor->vsys_mon_dev.irq,
				NULL, palmas_vsys_mon_irq_handler,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
				IRQF_ONESHOT | IRQF_EARLY_RESUME,
				"palmas_vsys_mon", palmas_voltage_monitor);
		if (status < 0) {
			dev_err(&pdev->dev, "can't get IRQ %d, err %d\n",
				palmas_voltage_monitor->vsys_mon_dev.irq,
				status);
		} else {
			palmas_voltage_monitor->vsys_mon_dev.monitor_on = false;
			disable_irq(palmas_voltage_monitor->vsys_mon_dev.irq);
		}
	}

	palmas_voltage_monitor->v_monitor.name = "palmas_voltage_monitor";
	palmas_voltage_monitor->v_monitor.type = POWER_SUPPLY_TYPE_UNKNOWN;

	ret = power_supply_register(palmas_voltage_monitor->dev,
					&palmas_voltage_monitor->v_monitor);
	if (ret) {
		dev_err(palmas_voltage_monitor->dev,
					"Failed: power supply register\n");
		return ret;
	}

	ret = sysfs_create_file(&pdev->dev.kobj,
						&dev_attr_voltage_monitor.attr);
	if (ret < 0) {
		dev_err(&pdev->dev, "error creating sysfs file %d\n", ret);
		return ret;
	}

#ifdef CONFIG_CABLE_VBUS_MONITOR
	ret = palmas_write(palmas_voltage_monitor->palmas,
				PALMAS_USB_OTG_BASE,
				PALMAS_USB_VBUS_INT_EN_LO_SET,
				PALMAS_USB_VBUS_INT_EN_LO_SET_VA_VBUS_VLD);
	if (ret < 0) {
		dev_err(palmas_voltage_monitor->dev,
				"USB_VBUS_INT_EN_LO_SET update fail, ret=%d\n",
				ret);
		goto skip_vbus_latch_register;
	}

	cable_vbus_monitor_latch_cb_register(
			palmas_voltage_monitor_is_vbus_latched,
			palmas_voltage_monitor);
	palmas_voltage_monitor->has_vbus_latched_cb = true;

skip_vbus_latch_register:
#endif
	return 0;
}

static int palmas_voltage_monitor_remove(struct platform_device *pdev)
{
	struct palmas_voltage_monitor *monitor = dev_get_drvdata(&pdev->dev);
#ifdef CONFIG_CABLE_VBUS_MONITOR
	int ret;
#endif

	mutex_lock(&monitor->mutex);
	if (monitor->use_vbat_monitor) {
		disable_irq(monitor->vbat_mon_dev.irq);
		devm_free_irq(monitor->dev, monitor->vbat_mon_dev.irq,
					monitor);
	}

	if (monitor->use_vsys_monitor) {
		disable_irq(monitor->vsys_mon_dev.irq);
		devm_free_irq(monitor->dev, monitor->vbat_mon_dev.irq,
					monitor);
	}

#ifdef CONFIG_CABLE_VBUS_MONITOR
	if (monitor->has_vbus_latched_cb) {
		cable_vbus_monitor_latch_cb_unregister(monitor);
		monitor->has_vbus_latched_cb = false;
		ret = palmas_write(monitor->palmas,
				PALMAS_USB_OTG_BASE,
				PALMAS_USB_VBUS_INT_EN_LO_CLR,
				PALMAS_USB_VBUS_INT_EN_LO_CLR_VA_VBUS_VLD);
		if (ret < 0)
			dev_err(monitor->dev,
					"USB_VBUS_INT_EN_LO_SET update fail, ret=%d\n",
					ret);
	}
#endif
	mutex_unlock(&monitor->mutex);
	sysfs_remove_file(&monitor->v_monitor.dev->kobj,
						&dev_attr_voltage_monitor.attr);
	power_supply_unregister(&monitor->v_monitor);
	mutex_destroy(&monitor->mutex);
	devm_kfree(monitor->dev, monitor);

	return 0;
}

static void palmas_voltage_monitor_shutdown(struct platform_device *pdev)
{
	struct palmas_voltage_monitor *monitor = dev_get_drvdata(&pdev->dev);
	int ret;

	mutex_lock(&monitor->mutex);
	if (monitor->use_vbat_monitor) {
		disable_irq(monitor->vbat_mon_dev.irq);
		devm_free_irq(monitor->dev,
					monitor->vbat_mon_dev.irq,
					monitor);
		ret = palmas_write(monitor->palmas, PALMAS_PMU_CONTROL_BASE,
				PALMAS_VBAT_MON, 0);
		if (ret < 0)
			dev_err(monitor->dev,
					"PALMAS_VBAT_MON write fail, ret=%d\n",
					ret);
	}

	if (monitor->use_vsys_monitor) {
		disable_irq(monitor->vsys_mon_dev.irq);
		devm_free_irq(monitor->dev,
					monitor->vsys_mon_dev.irq,
					monitor);
		ret = palmas_write(monitor->palmas, PALMAS_PMU_CONTROL_BASE,
				PALMAS_VSYS_MON, 0);
		if (ret < 0)
			dev_err(monitor->dev,
					"PALMAS_VSYS_MON write fail, ret=%d\n",
					ret);
	}

#ifdef CONFIG_CABLE_VBUS_MONITOR
	if (monitor->has_vbus_latched_cb) {
		cable_vbus_monitor_latch_cb_unregister(monitor);
		monitor->has_vbus_latched_cb = false;
		ret = palmas_write(monitor->palmas,
				PALMAS_USB_OTG_BASE,
				PALMAS_USB_VBUS_INT_EN_LO_CLR,
				PALMAS_USB_VBUS_INT_EN_LO_CLR_VA_VBUS_VLD);
		if (ret < 0)
			dev_err(monitor->dev,
					"USB_VBUS_INT_EN_LO_SET update fail, ret=%d\n",
					ret);
	}
#endif
	mutex_unlock(&monitor->mutex);

}

static const struct of_device_id palmas_voltage_monitor_dt_match[] = {
	{ .compatible = "ti,palmas-voltage-monitor" },
	{ },
};
MODULE_DEVICE_TABLE(of, palmas_voltage_monitor_dt_match);

static struct platform_driver palmas_voltage_monitor_driver = {
	.driver	= {
		.name	= "palmas_voltage_monitor",
		.of_match_table = of_match_ptr(palmas_voltage_monitor_dt_match),
		.owner = THIS_MODULE,
	},
	.probe		= palmas_voltage_monitor_probe,
	.remove		= palmas_voltage_monitor_remove,
	.shutdown	= palmas_voltage_monitor_shutdown,
};

static int __init palmas_voltage_monitor_init(void)
{
	return platform_driver_register(&palmas_voltage_monitor_driver);
}
subsys_initcall(palmas_voltage_monitor_init);

static void __exit palmas_voltage_monitor_exit(void)
{
	platform_driver_unregister(&palmas_voltage_monitor_driver);
}
module_exit(palmas_voltage_monitor_exit);

MODULE_DESCRIPTION("TI Palmas voltage monitor driver");
MODULE_LICENSE("GPL");
