/*
 * flounder_battery.c
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
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/of.h>

struct flounder_battery_data {
	struct power_supply		battery;
	struct device			*dev;
	int present;
};
static struct flounder_battery_data *flounder_battery_data;

static enum power_supply_property flounder_battery_prop[] = {
	POWER_SUPPLY_PROP_PRESENT,
};

static int flounder_battery_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct flounder_battery_data *data = container_of(psy,
				struct flounder_battery_data, battery);

	if (psp == POWER_SUPPLY_PROP_PRESENT)
		val->intval = data->present;
	else
		return -EINVAL;
	return 0;
}

static int flounder_battery_probe(struct platform_device *pdev)
{
	struct flounder_battery_data *data;
	int ret;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, data);

	data->battery.name		= "flounder-battery";
	data->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	data->battery.get_property	= flounder_battery_get_property;
	data->battery.properties	= flounder_battery_prop;
	data->battery.num_properties	= ARRAY_SIZE(flounder_battery_prop);
	data->dev			= &pdev->dev;

	ret = power_supply_register(data->dev, &data->battery);
	if (ret) {
		dev_err(data->dev, "failed: power supply register\n");
		return ret;
	}

	data->present = 1;
	flounder_battery_data = data;

	return 0;
}

static int flounder_battery_remove(struct platform_device *pdev)
{
	struct flounder_battery_data *data = dev_get_drvdata(&pdev->dev);

	data->present = 0;
	power_supply_unregister(&data->battery);

	return 0;
}

static const struct of_device_id flounder_battery_dt_match[] = {
	{ .compatible = "htc,max17050_battery" },
	{ },
};
MODULE_DEVICE_TABLE(of, flounder_battery_dt_match);

static struct platform_driver flounder_battery_driver = {
	.driver	= {
		.name	= "flounder_battery",
		.of_match_table = of_match_ptr(flounder_battery_dt_match),
		.owner = THIS_MODULE,
	},
	.probe		= flounder_battery_probe,
	.remove		= flounder_battery_remove,
};

static int __init flounder_battery_init(void)
{
	return platform_driver_register(&flounder_battery_driver);
}
fs_initcall_sync(flounder_battery_init);

static void __exit flounder_battery_exit(void)
{
	platform_driver_unregister(&flounder_battery_driver);
}
module_exit(flounder_battery_exit);

MODULE_DESCRIPTION("Flounder battery driver");
MODULE_LICENSE("GPL");
