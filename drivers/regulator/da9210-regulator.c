/*
 * da9210-regulator.c - Regulator device driver for DA9210
 * Copyright (C) 2013  Dialog Semiconductor Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/da9210-regulator.h>
#include <linux/regmap.h>
#include <linux/regmap-ipc.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>

/* For DebugFS */
#include <linux/debugfs.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/regulator/of_regulator.h>
#endif


struct da9210 {
	struct device *dev;
	struct regulator_dev *rdev;
	struct regmap *regmap;
	struct dentry *dent;
};

struct da9210_pdata {
	struct regulator_init_data da9210_constraints;
};

static const struct regmap_config da9210_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static struct da9210 *g_da9210 = NULL;

static int da9210_set_current_limit(struct regulator_dev *rdev, int min_uA,
				    int max_uA);
static int da9210_get_current_limit(struct regulator_dev *rdev);

static struct regulator_ops da9210_buck_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.list_voltage = regulator_list_voltage_linear,
	.set_current_limit = da9210_set_current_limit,
	.get_current_limit = da9210_get_current_limit,
};

#define DA9210_NAME	"da9210"

/* Default limits measured in millivolts and milliamps */
#define DA9210_MIN_MV		300
#define DA9210_MAX_MV		1570
#define DA9210_STEP_MV		10

/* Current limits for buck (uA) indices corresponds with register values */
static const int da9210_buck_limits[] = {
	1600000, 1800000, 2000000, 2200000, 2400000, 2600000, 2800000, 3000000,
	3200000, 3400000, 3600000, 3800000, 4000000, 4200000, 4400000, 4600000
};

static const struct regulator_desc da9210_reg = {
	.name = "DA9210",
	.id = 0,
	.ops = &da9210_buck_ops,
	.type = REGULATOR_VOLTAGE,
	.n_voltages = ((DA9210_MAX_MV - DA9210_MIN_MV) / DA9210_STEP_MV) + 1,
	.min_uV = (DA9210_MIN_MV * 1000),
	.uV_step = (DA9210_STEP_MV * 1000),
	.vsel_reg = DA9210_REG_VBUCK_A,
	.vsel_mask = DA9210_VBUCK_MASK,
	.enable_reg = DA9210_REG_BUCK_CONT,
	.enable_mask = DA9210_BUCK_EN,
	.owner = THIS_MODULE,
};

/* REGMAP IPC Register for DA9210 */
#define DA9210_I2C_SLOT	 	0
#define DA9210_SLAVE_ADDRESS 0xb0

struct debug_reg {
	char  *name;
	u8  reg;
};

#define DA9210_PM_DEBUG_REG(x) {#x, x##_REG}

static struct debug_reg da9210_pmic_debug_regs[] = {
	DA9210_PM_DEBUG_REG(DA9210_REG_STATUS_A),
	DA9210_PM_DEBUG_REG(DA9210_REG_STATUS_B),
	DA9210_PM_DEBUG_REG(DA9210_REG_EVENT_A),
	DA9210_PM_DEBUG_REG(DA9210_REG_EVENT_B),
	DA9210_PM_DEBUG_REG(DA9210_REG_MASK_A),
	DA9210_PM_DEBUG_REG(DA9210_REG_MASK_B),
	DA9210_PM_DEBUG_REG(DA9210_REG_CONTROL_A),
	DA9210_PM_DEBUG_REG(DA9210_REG_GPIO_0_1),
	DA9210_PM_DEBUG_REG(DA9210_REG_GPIO_2_3),
	DA9210_PM_DEBUG_REG(DA9210_REG_GPIO_4_5),
	DA9210_PM_DEBUG_REG(DA9210_REG_GPIO_6),
};

static int da9210_set_current_limit(struct regulator_dev *rdev, int min_uA,
				    int max_uA)
{
	struct da9210 *chip = rdev_get_drvdata(rdev);
	unsigned int sel;
	int i;

	/* search for closest to maximum */
	for (i = ARRAY_SIZE(da9210_buck_limits)-1; i >= 0; i--) {
		if (min_uA <= da9210_buck_limits[i] &&
		    max_uA >= da9210_buck_limits[i]) {
			sel = i;
			sel = sel << DA9210_BUCK_ILIM_SHIFT;
			return regmap_update_bits(chip->regmap,
						  DA9210_REG_BUCK_ILIM,
						  DA9210_BUCK_ILIM_MASK, sel);
		}
	}

	return -EINVAL;
}

static int da9210_get_current_limit(struct regulator_dev *rdev)
{
	struct da9210 *chip = rdev_get_drvdata(rdev);
	unsigned int data;
	unsigned int sel;
	int ret;

	printk("[WS][%s]\n Start\n", __func__);

	ret = regmap_read(chip->regmap, DA9210_REG_BUCK_ILIM, &data);
	if (ret < 0)
		return ret;

	printk("[WS][%s]\n DA9210_REG_BUCK_ILIM[0x%x]\n", __func__, data);

	/* select one of 16 values: 0000 (1600mA) to 1111 (4600mA) */
	sel = (data & DA9210_BUCK_ILIM_MASK) >> DA9210_BUCK_ILIM_SHIFT;

	return da9210_buck_limits[sel];
}

/* GPIO0 / GPIO1 Control For Indicator LED & Key LED */
int GPIO_0_1_CTRL_INIT(void)
{
	int ret = 0;

	if(g_da9210 == NULL) {
		printk("=== GPIO_0_1_CTRL_INIT g_da9210 is not initialized yet ===\n");
		return -1;
	}
	ret = regmap_write(g_da9210->regmap, DA9210_REG_GPIO_0_1, 0x77);
	return ret;
}
EXPORT_SYMBOL(GPIO_0_1_CTRL_INIT);

int GPIO1_INDICATOR_LED_CTRL(int value)
{
	if(g_da9210 == NULL) {
		printk("=== GPIO1_INDICATOR_LED_CTRL g_da9210 is not initialized yet ===\n");
		return -1;
	}
	value = value << 7;
	return regmap_update_bits(g_da9210->regmap, DA9210_REG_GPIO_0_1, DA9210_GPIO1_MODE, value);
}
EXPORT_SYMBOL(GPIO1_INDICATOR_LED_CTRL);

int GPIO0_KEY_LED_CTRL(int value)
{
	if(g_da9210 == NULL) {
		printk("=== GPIO0_KEY_LED_CTRL g_da9210 is not initialized yet ===\n");
		return -1;
	}
	value = value << 3;
	return regmap_update_bits(g_da9210->regmap, DA9210_REG_GPIO_0_1, DA9210_GPIO0_MODE, value);
}
EXPORT_SYMBOL(GPIO0_KEY_LED_CTRL);

static const struct i2c_device_id da9210_i2c_id[] = {
	{"da9210", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, da9210_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id da9210_dt_ids[] = {
        { .compatible = "dlg,da9210", .data = &da9210_i2c_id[0] },
        { /* sentinel */ }
};
#endif

static int set_reg(void *data, u64 val)
{
	struct da9210 *me;
	u32 addr = (u32) data;
	int ret;

	if (g_da9210) {
		me = g_da9210;
	}
	else{
		printk("==g_da9210 set_reg is not initialized yet==\n");
		return;
	}

	ret = regmap_write(me->regmap, addr, (u32) val);

	return ret;
}

static int get_reg(void *data, u64 *val)
{
	struct da9210 *me;
	u32 addr = (u32) data;
	u32 temp;
	int ret;

	if (g_da9210) {
		me = g_da9210;
	}
	else{
		printk("==g_da9210 get_reg is not initialized yet==\n");
		return;
	}

	ret = regmap_read(me->regmap, addr, &temp);

	if (ret < 0)
		return ret;

	*val = temp;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(reg_fops, get_reg, set_reg, "0x%02llx\n");

static int da9210_pmic_create_debugfs_entries(struct da9210 *chip)
{
	int i;

	chip->dent = debugfs_create_dir(DA9210_NAME, NULL);
	if (IS_ERR(chip->dent)) {
		pr_err("da9210_pmic driver couldn't create debugfs dir\n");
		return -EFAULT;
	}

	for (i = 0 ; i < ARRAY_SIZE(da9210_pmic_debug_regs) ; i++) {
		char *name = da9210_pmic_debug_regs[i].name;
		u32 reg = da9210_pmic_debug_regs[i].reg;
		struct dentry *file;

		file = debugfs_create_file(name, 0644, chip->dent,
					(void *) reg, &reg_fops);
		if (IS_ERR(file)) {
			pr_err("debugfs_create_file %s failed.\n", name);
			return -EFAULT;
		}
	}
	return 0;
}

/*
 * Platform driver interface functions
 */
static int da9210_i2c_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct ipc_client *ipc;
	struct device *dev = &pdev->dev;
	struct da9210 *chip;
	const struct of_device_id *deviceid;
	int ret = 0;
	int error;

	printk("[%s]_probe_start\n", __func__);

	ipc = kzalloc(sizeof(struct ipc_client), GFP_KERNEL);
	if (ipc == NULL) {
		dev_err(chip->dev, "Platform data not specified.\n");
		ret = -ENOMEM;
		goto free_mem_ipc;
	}
	ipc->slot = DA9210_I2C_SLOT;
	ipc->addr = DA9210_SLAVE_ADDRESS;
	ipc->dev = dev;

	chip = kzalloc(sizeof(struct da9210), GFP_KERNEL);
	if (chip == NULL) {
		ret = -ENOMEM;
		goto free_mem_ipc;
	}

	platform_set_drvdata(pdev, chip);
	chip->dev = &ipc->dev;

	chip->regmap = devm_regmap_init_ipc(ipc, &da9210_regmap_config);
	if (IS_ERR(chip->regmap)) {
		ret = PTR_ERR(chip->regmap);
		dev_err(chip->dev, "Failed to allocate register map: %d\n",
			ret);
		goto free_mem;
	}

	g_da9210 = chip;

	/* For DebugFS*/
	ret = da9210_pmic_create_debugfs_entries(chip);
	if (ret) {
		printk("da9210_pmic_create_debugfs_entries failed\n");
	}
	GPIO_0_1_CTRL_INIT();
	printk("[%s]_probe_end\n", __func__);

	return 0;

free_mem:
	kfree(chip);
free_mem_ipc:
	kfree(ipc);

	return ret;
}

static int da9210_i2c_remove(struct platform_device *pdev)
{
	struct da9210 *chip = platform_get_drvdata(pdev);

	if (chip->dent)
		debugfs_remove_recursive(chip->dent);
	return 0;
}

static struct platform_driver da9210_regulator_driver = {
	.driver = {
		.name = "da9210",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = da9210_dt_ids,
#endif
	},
	.probe = da9210_i2c_probe,
	.remove = da9210_i2c_remove,
	.id_table = da9210_i2c_id,
};

static int __init da9210_pm_init(void)
{
	int ret;

	ret = platform_driver_register(&da9210_regulator_driver);
	if (ret != 0)
		pr_err("Failed to register DA9210 PM driver\n");

	return ret;
}
fs_initcall(da9210_pm_init);

static void __exit da9210_pm_exit(void)
{
	platform_driver_unregister(&da9210_regulator_driver);
}
module_exit(da9210_pm_exit);

MODULE_AUTHOR("S Twiss <stwiss.opensource@diasemi.com>");
MODULE_DESCRIPTION("Regulator device driver for Dialog DA9210");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:da9210");
