/*
* da9066-regulator.c: Regulator driver for Dialog DA9066
*
* Copyright(c) 2013 Dialog Semiconductor Ltd.
*
* Author: Dialog Semiconductor Ltd. D. Chen
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/stringify.h>

#include <linux/mfd/da9066/pmic.h>
#include <linux/mfd/da9066/da9066_reg.h>
#include <linux/mfd/da9066/core.h>

#define DRIVER_NAME                 "da9066-regulator"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/regulator/of_regulator.h>
#endif

/* Definition for registering bit fields */
struct bfield {
	unsigned short	addr;
	unsigned char	mask;
};
#define BFIELD(_addr, _mask) \
	{ .addr = _addr, .mask = _mask }

struct field {
	unsigned short	addr;
	unsigned char	mask;
	unsigned char	shift;
	unsigned char	offset;
};
#define FIELD(_addr, _mask, _shift, _offset) \
	{ .addr = _addr, .mask = _mask, .shift = _shift, .offset = _offset }

struct regl_register_map {
	u8 v_reg;
	u8 v_bit;
	u8 v_len;
	u8 v_mask;
	u8 en_reg;
	u8 en_mask;
	u8 mctl_reg;
	u8 dsm_opmode;
	u8 ramp_reg;
	u8 ramp_bit;
};

struct da9066_reg_info {
	int id;
	char *name;
	struct regulator_ops	*ops;

	int				min_uv;
	int 			max_uv;
	unsigned 	step_uv;
	unsigned		n_steps;
	u32 min_uvolts;
	u32 uvolt_step;
	u32 max_uvolts;
};

extern struct da9066 *da9066_regl_info;

#define DA9066_NO_V_CONTROL_REG 0
#define DA9066_NO_V_CONTROL_BIT 0
#define DA9066_NO_V_CONTROL_LEN 0
#define DA9066_NO_V_CONTROL_MASK 0
#define DA9066_DEFINE_REGL(_name, _v_reg_, _vn_, _e_reg_, _en_,\
						 _m_reg_, _ramp_reg, _ramp_bit) \
	[DA9066_##_name] = \
		{ \
			.v_reg = DA9066_##_v_reg_##_REG, \
			.v_bit = DA9066_##_vn_##_BIT, \
			.v_len =  DA9066_##_vn_##_LEN, \
			.v_mask =  DA9066_##_vn_##_MASK, \
			.en_reg = DA9066_##_e_reg_##_REG, \
			.en_mask = DA9066_##_en_##_MASK, \
			.mctl_reg = DA9066_##_m_reg_##_REG, \
			.ramp_reg = _ramp_reg, \
			.ramp_bit = _ramp_bit, \
		}


struct da9066_regulator_info {
	int			id;
	char			*name;
	struct regulator_ops	*ops;

	/* Voltage adjust range */
	int		min_uv;
	int		max_uv;
	unsigned	step_uv;
	unsigned    n_steps;

	/* Current limiting */
	unsigned	n_current_limits;
	const int	*current_limits;

	/* DA9066 main register fields */
	struct bfield	enable;		/* bit used to enable regulator,
					   it returns actual state when read */
	struct field	mode;		/* buck mode of operation */
	struct bfield	suspend;
	struct bfield	sleep;
	struct bfield	suspend_sleep;
	struct field	voltage;
	struct field	suspend_voltage;
	struct field	ilimit;

	/* DA9066 event detection bit */
	struct bfield	oc_event;
};

/* Defines asignment of regulators info table to chip model */
struct da9066_dev_model {
	const struct da9066_regulator_info	*regulator_info;
	unsigned				n_regulators;
	unsigned				dev_model;
};

/* Single regulator settings */
struct da9066_regulator {
	struct regulator_desc			desc;
	struct regulator_dev			*rdev;
	struct da9066				*hw;
	const struct da9066_regulator_info	*info;

	unsigned long				 flags;
	unsigned					 mode;
	unsigned					 suspend_mode;
	struct da9066_regulator_data *da9066_regulator_info;
	struct da9066_regulators_pdata *regulator_data;
	int n_regulators;
};

static int da9066_regulator_set_voltage(struct regulator_dev *rdev,
						int min_uv, int max_uv,unsigned *selector);
static int da9066_regulator_get_voltage(struct regulator_dev *rdev);
static int da9066_regulator_enable(struct regulator_dev *rdev);
static int da9066_regulator_disable(struct regulator_dev *rdev);
static int da9066_regulator_is_enabled(struct regulator_dev *rdev);
static int da9066_regulator_list_voltage(struct regulator_dev *rdev,
						unsigned selector);

/* Encapsulates all information for the regulators driver */
struct da9066_regulators {
	int					irq_ldo_lim;
	int					irq_uvov;
	unsigned				n_regulators;
	/* Array size to be defined during init. Keep at end. */
	struct da9066_regulator			regulator[0];
};

#define DA9066_DEFINE_INFO(chip, regl_name, min_mV, step_mV, max_mV) \
	.id = chip##_ID_##regl_name, \
	.name = __stringify(chip##_##regl_name), \
	.ops = &da9066_ldo_ops, \
	.min_uv = (min_mV) * 1000, \
	.max_uv = (max_mV) * 1000, \
	.step_uv = (step_mV) * 1000, \
	.n_steps = (((max_mV) - (min_mV))/(step_mV) + 1)

#define DA9066_COMMON_FIELDS(regl_name) \
	.enable = BFIELD(DA9066_##regl_name##_REG, DA9066_##regl_name##_EN_MASK), \
	.voltage = FIELD(DA9066_##regl_name##_REG,  \
					DA9066_V##regl_name##_DP_MASK, \
					DA9066_V##regl_name##_DP_BIT, \
					DA9066_V##regl_name##_DP_BIAS)

static struct regulator_ops da9066_ldo_ops = {
	.set_voltage = da9066_regulator_set_voltage,
	.get_voltage_sel = da9066_regulator_get_voltage,
	.enable = da9066_regulator_enable,
	.disable = da9066_regulator_disable,
	.list_voltage = da9066_regulator_list_voltage,
	.is_enabled = da9066_regulator_is_enabled,
};

static const struct da9066_regulator_info da9066_regulator_info[] = {
	{
	 DA9066_DEFINE_INFO(DA9066, BUCK_1, 600, 6.25, 1393.75),
	 DA9066_COMMON_FIELDS(BUCK_1),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, BUCK_2, 500, 25, 2075),
	 DA9066_COMMON_FIELDS(BUCK_2),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, BUCK_3, 500, 25, 2075),
	 DA9066_COMMON_FIELDS(BUCK_3),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, BUCK_4, 500, 25, 2075),
	 DA9066_COMMON_FIELDS(BUCK_4),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, BUCK_5, 500, 25, 2075),
	 DA9066_COMMON_FIELDS(BUCK_5),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, BUCK_6, 160, 25, 2075),
	 DA9066_COMMON_FIELDS(BUCK_6),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_1, 1000, 50, 3100),
	 DA9066_COMMON_FIELDS(LDO_1),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_2, 1000, 50, 3100),
	 DA9066_COMMON_FIELDS(LDO_2),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_3, 1200, 50, 3300),
	 DA9066_COMMON_FIELDS(LDO_3),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_4, 1200, 50, 3300),
	 DA9066_COMMON_FIELDS(LDO_4),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_5, 1200, 50, 3300),
	 DA9066_COMMON_FIELDS(LDO_5),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_6, 1200, 50, 3300),
	 DA9066_COMMON_FIELDS(LDO_6),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_7, 1200, 50, 3300),
	 DA9066_COMMON_FIELDS(LDO_7),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_8, 1200, 50, 3300),
	 DA9066_COMMON_FIELDS(LDO_8),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_9, 1200, 50, 3300),
	 DA9066_COMMON_FIELDS(LDO_9),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_10, 1200, 50, 3300),
	 DA9066_COMMON_FIELDS(LDO_10),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_11, 1200, 50, 3300),
	 DA9066_COMMON_FIELDS(LDO_11),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_12, 1200, 50, 3300),
	 DA9066_COMMON_FIELDS(LDO_12),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_13, 1200, 50, 3300),
	 DA9066_COMMON_FIELDS(LDO_13),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_14, 1200, 50, 3300),
	 DA9066_COMMON_FIELDS(LDO_14),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_15, 1200, 50, 3300),
	 DA9066_COMMON_FIELDS(LDO_15),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_16, 1200, 50, 3300),
	 DA9066_COMMON_FIELDS(LDO_16),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_17, 1200, 50, 3300),
	 DA9066_COMMON_FIELDS(LDO_17),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_18, 1200, 50, 3300),
	 DA9066_COMMON_FIELDS(LDO_18),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_19, 1200, 50, 3300),
	 DA9066_COMMON_FIELDS(LDO_19),
	},
	{
	 DA9066_DEFINE_INFO(DA9066, LDO_20, 1200, 50, 3300),
	 DA9066_COMMON_FIELDS(LDO_20),
	},
};


static int da9066_register_regulator(struct da9066 *da9066, int reg,
					struct regulator_init_data *initdata);

/*
 * Regulator internal functions
 */
static int da9066_sel_to_vol(struct da9066_regulator *regl, unsigned selector)
{
	return regl->info->step_uv * selector + regl->info->min_uv;
}

static unsigned da9066_min_val_to_sel(int in, int base, int step)
{
	if (step == 0)
		return 0;

	return (in - base + step - 1) / step;
}


static int da9066_regulator_list_voltage(struct regulator_dev *rdev,
					unsigned selector)
{
	struct da9066_regulator *regl;
	unsigned int ret;
	regl = rdev_get_drvdata(rdev);

	ret = da9066_sel_to_vol(regl, selector);
	return ret;
}

static int da9066_regulator_set_voltage(struct regulator_dev *rdev,
				int min_uv, int max_uv, unsigned *selector)
{
	struct da9066_regulator *regl = rdev_get_drvdata(rdev);
	const struct field *fvol = &regl->info->voltage;
	int ret;
	unsigned val;
	unsigned sel = da9066_min_val_to_sel(min_uv, regl->info->min_uv,
							regl->info->step_uv);

	if (da9066_sel_to_vol(regl, sel) > max_uv)
		return -EINVAL;

	val = (sel + fvol->offset) << fvol->shift & fvol->mask;
	ret = da9066_reg_update(regl->hw, fvol->addr, fvol->mask, val);
	if (ret >= 0)
		*selector = sel;

	return ret;
}

static int da9066_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct da9066_regulator *regl = rdev_get_drvdata(rdev);
	const struct da9066_regulator_info *rinfo = regl->info;
	int sel;
	unsigned int data;

	sel = da9066_reg_read(regl->hw, rinfo->voltage.addr, &data);
	if (sel < 0)
		return sel;

	sel = (sel & rinfo->voltage.mask) >> rinfo->voltage.shift;
	sel -= rinfo->voltage.offset;
	if (sel < 0)
		sel = 0;
	if (sel >= rinfo->n_steps)
		sel = rinfo->n_steps - 1;

	return sel;
}

static int da9066_regulator_enable(struct regulator_dev *rdev)
{
	struct da9066_regulator *regl = rdev_get_drvdata(rdev);
	const struct da9066_regulator_info *rinfo = regl->info;

	return da9066_set_bits(regl->hw, rinfo->enable.addr,
				   rinfo->enable.mask);
}

static int da9066_regulator_disable(struct regulator_dev *rdev)
{
	struct da9066_regulator *regl = rdev_get_drvdata(rdev);
	const struct bfield *benable = &regl->info->enable;

	return da9066_clear_bits(regl->hw, benable->addr, benable->mask);
}

static int da9066_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct da9066_regulator *regl = rdev_get_drvdata(rdev);
	const struct bfield *benable = &regl->info->enable;
	unsigned int data;
	int val = da9066_reg_read(regl->hw, benable->addr, &data);

	if (val < 0)
		return val;

	return val & (benable->mask);
}

/* Link chip model with regulators info table */
static struct da9066_dev_model regulators_models[] = {
	{
		.regulator_info = da9066_regulator_info,
		.n_regulators = ARRAY_SIZE(da9066_regulator_info),
		.dev_model = PMIC_DA9066,
	},
	{NULL, 0, 0}	/* End of list */
};

static __init const struct da9066_regulator_info *da9066_get_regl_info(
		const struct da9066 *da9066, const struct da9066_dev_model *model, int id)
{
	int m;
#if 0
	for (m = 0; m < model->n_regulators; m++) {
		if (model->regulator_info[m].id == id) {
			return &model->regulator_info[m];
		}
	}
#endif
	for (m = model->n_regulators - 1;
	     model->regulator_info[m].id != id; m--) {
		if (m <= 0)
			return NULL;
	}

	return &model->regulator_info[m];
}

#ifdef CONFIG_OF
static struct da9066_regulators_pdata *da9066_reg_parse_dt(struct device *dev)
{
	struct device_node *nproot = dev->parent->of_node;
	struct device_node *regulators_np, *np;
	struct da9066_regulators_pdata *pdata;
	struct da9066_regulator_data *regulator_data;
	int i;

	if (unlikely(nproot == NULL))
		return ERR_PTR(-EINVAL);

	regulators_np = of_find_node_by_name(nproot, "regulators");
	if (!regulators_np) {
		dev_err(dev, "regulators node not found\n");
		return -ENODEV;
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

	pdata->n_regulators = (of_get_child_count(regulators_np) - 1);

	pdata->regulator_data = regulator_data = devm_kzalloc(dev,
			sizeof(*pdata->regulator_data) * pdata->n_regulators,
			GFP_KERNEL);
	if (unlikely(pdata->regulator_data == NULL)) {
		of_node_put(regulators_np);
		return ERR_PTR(-ENOMEM);
	}

	for_each_child_of_node(regulators_np, np) {
		for (i = 0; i < ARRAY_SIZE(da9066_regulator_info); i++) {
			if (!of_node_cmp(np->name,da9066_regulator_info[i].name)) {
				break;
			}
		}
		if (unlikely(i >= ARRAY_SIZE(da9066_regulator_info))) {
			dev_warn(dev, "Cannot find the regulator information for %s\n",
				np->name);
			continue;
		}

		regulator_data->id = i;
		regulator_data->initdata = of_get_regulator_init_data(dev, np);
		regulator_data->of_node = np;
		regulator_data++;
	}

	return pdata;
}
#endif

/*
 * da9066_regulator_is_enabled
 */
static int da9066_regulator_probe(struct platform_device *pdev)
{
	struct da9066 *da9066 = dev_get_drvdata(pdev->dev.parent);
	struct da9066_regulators_pdata *pdata;
	struct da9066_regulator_data *rdata;
	struct da9066_regulators *regulators;
	struct da9066_regulator *regl;
	struct regulator_config config;
	struct regulator_dev *rdev;
	const struct da9066_dev_model *model;
	size_t size;
	int n;
	int ret;


#ifdef CONFIG_OF
	pdata = da9066_reg_parse_dt(&pdev->dev);
#endif
	if (!pdata || pdata->n_regulators == 0) {
		dev_err(&pdev->dev,
			"No regulators defined for the platform\n");
		return -ENODEV;
	}

	/* Find regulators set for particular device model */
	for (model = regulators_models; model->regulator_info; model++) {
		if (model->dev_model == 0x80);
			break;

		if (model->regulator_info > 100)
			break;
	}
	if (!model->regulator_info) {
		dev_err(&pdev->dev, "Chip model not recognised (%u)\n",
			da9066_model(da9066));
		return -ENODEV;
	}

	/* Allocate memory required by usable regulators */
	size = sizeof(struct da9066_regulators) +
		pdata->n_regulators * sizeof(struct da9066_regulator);
	regulators = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
	if (!regulators) {
		dev_err(&pdev->dev, "No memory for regulators\n");
		return -ENOMEM;
	}

	regulators->n_regulators = pdata->n_regulators;
	platform_set_drvdata(pdev, regulators);

	n = 0;
	while ( n < regulators->n_regulators) {
		rdata = &pdata->regulator_data[n];

		/* Initialise regulator structure */
		regl = &regulators->regulator[n];
		regl->hw = da9066;
		regl->flags = rdata->flags;
		regl->info = da9066_get_regl_info(da9066, model, rdata->id);
		if (!regl->info) {
			dev_err(&pdev->dev,
				"Invalid regulator ID in platform data\n");
			ret = -EINVAL;
			goto err;
		}
		regl->desc.name = regl->info->name;
		regl->desc.id = rdata->id;
		regl->desc.ops = regl->info->ops;
		regl->desc.n_voltages = regl->info->n_steps;
		regl->desc.type = REGULATOR_VOLTAGE;
		regl->desc.owner = THIS_MODULE;

		/* Register regulator */
		config.dev = &pdev->dev;
		config.init_data = rdata->initdata;
		config.driver_data = regl;
		config.of_node = rdata->of_node;
		config.regmap = da9066->regmap;
		regl->rdev = regulator_register(&regl->desc, &config);
		if (IS_ERR_OR_NULL(regl->rdev)) {
			dev_err(&pdev->dev,
				"Failed to register %s regulator\n",
				regl->info->name);
			ret = PTR_ERR(regl->rdev);
			goto err;
		}
		n++;
	}
	dev_info(&pdev->dev, "Regulator started.\n");

	return 0;

err:
	/* Wind back regulators registeration */
	while (--n >= 0)
		regulator_unregister(regulators->regulator[n].rdev);

	return ret;
}

/*
 * da9066_regulator_remove
 */
static int da9066_regulator_remove(struct platform_device *pdev)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);
	struct da9066 *da9066 = rdev_get_drvdata(rdev);
	int i;

	for (i = 0; i < ARRAY_SIZE(da9066->pmic.pdev); i++)
		platform_device_unregister(da9066->pmic.pdev[i]);

	regulator_unregister(rdev);

	return 0;
}

/*
 * da9066_register_regulator
 */
static int da9066_register_regulator(struct da9066 *da9066, int reg,
					struct regulator_init_data *initdata)
{
	struct platform_device *pdev;
	int ret;
	if (reg < DA9066_ID_BUCK_1 || reg >= 25)
		return -EINVAL;

	if (da9066->pmic.pdev[reg])
		return -EBUSY;

	pdev = platform_device_alloc(DRIVER_NAME, reg);
	if (!pdev)
		return -ENOMEM;

	da9066->pmic.pdev[reg] = pdev;

	initdata->driver_data = da9066;

	pdev->dev.platform_data = initdata;
	pdev->dev.parent = da9066->dev;
	platform_set_drvdata(pdev, da9066);

	ret = platform_device_add(pdev);

	if (ret != 0) {
		dev_err(da9066->dev, "Failed to register regulator %d: %d\n", reg, ret);
		platform_device_del(pdev);
		da9066->pmic.pdev[reg] = NULL;
	}
	return ret;
}


static struct platform_driver da9066_regulator_driver = {
	.driver = {
		.name = DA9066_DRVNAME_REGULATORS,
		.owner = THIS_MODULE,
	},
	.probe  = da9066_regulator_probe,
	.remove = da9066_regulator_remove,
};

static int __init da9066_regulator_init(void)
{
	return platform_driver_register(&da9066_regulator_driver);
}
subsys_initcall(da9066_regulator_init);


static void __exit da9066_regulator_exit(void)
{
	platform_driver_unregister(&da9066_regulator_driver);
}
module_exit(da9066_regulator_exit);

/* Module information */
MODULE_AUTHOR("Dialog Semiconductor Ltd < william.seo@diasemi.com >");
MODULE_DESCRIPTION("DA9066 voltage and current regulator driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DA9066_DRVNAME_REGULATORS);
