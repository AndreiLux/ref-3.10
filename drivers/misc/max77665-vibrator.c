/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/max77665-vibrator.h>
#include <linux/mfd/max77665.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include "../staging/android/timed_output.h"


/* HAPTIC REGISTERS */
#define MAX77665_HAPTIC_STATUS		0x00
#define MAX77665_HBUSY_MASK		0x01

#define MAX77665_HAPTIC_CONFIG1		0x01
#define MAX77665_SCF_MASK		0x07
#define MAX77665_MSU_MASK		0x38
#define MAX77665_CONT_MASK		0x40
#define MAX77665_INV_MASK		0x80
#define MAX77665_SCF_SHIFT		0
#define MAX77665_MSU_SHIFT		3
#define MAX77665_CONT_SHIFT		6
#define MAX77665_INV_SHIFT		7

#define MAX77665_HAPTIC_CONFIG2		0x02
#define MAX77665_PDIV_MASK		0x03
#define MAX77665_HTYP_MASK		0x20
#define MAX77665_MEN_MASK		0x40
#define MAX77665_MODE_MASK		0x80
#define MAX77665_PDIV_SHIFT		0
#define MAX77665_HTYPE_SHIFT		5
#define MAX77665_MEM_SHIFT		6
#define MAX77665_MODE_SHIFT		7

#define MAX77665_HAPTIC_CONFIG_CHNL	0x03
#define MAX77665_PWMDCA_MASK		0x03
#define MAX77665_SIGDCA_MASK		0x0C
#define MAX77665_SIGPA_MASK		0x30
#define MAX77665_CYCA_MASK		0xC0

#define MAX77665_HAPTIC_CONFIG_CYC1	0x04
#define MAX77665_CYC1_MASK		0x0F
#define MAX77665_CYC0_MASK		0xF0
#define MAX77665_CYC1_SHIFT		0
#define MAX77665_CYC0_SHIFT		4

#define MAX77665_HAPTIC_CONFIG_CYC2	0x05
#define MAX77665_CYC3_MASK		0x0F
#define MAX77665_CYC2_MASK		0xF0
#define MAX77665_CYC3_SHIFT		0
#define MAX77665_CYC2_SHIFT		4

#define MAX77665_HAPTIC_CONFIG_PER0	0x06
#define MAX77665_HAPTIC_CONFIG_PER1	0x07
#define MAX77665_HAPTIC_CONFIG_PER2	0x08
#define MAX77665_HAPTIC_CONFIG_PER3	0x09

#define MAX77665_HAPTIC_CONFIG_DUTY1	0x0A
#define MAX77665_SIGDC1_MASK		0x0F
#define MAX77665_SIGDC0_MASK		0xF0
#define MAX77665_SIGDC1_SHIFT		0
#define MAX77665_SIGDC0_SHIFT		4

#define MAX77665_HAPTIC_CONFIG_DUTY2	0x0B
#define MAX77665_SIGDC3_MASK		0x0F
#define MAX77665_SIGDC2_MASK		0xF0
#define MAX77665_SIGDC3_SHIFT		0
#define MAX77665_SIGDC2_SHIFT		4

#define MAX77665_HAPTIC_CONFIG_PWMD0	0x0C
#define MAX77665_HAPTIC_CONFIG_PWMD1	0x0D
#define MAX77665_HAPTIC_CONFIG_PWMD2	0x0E
#define MAX77665_HAPTIC_CONFIG_PWMD3	0x0F

#define MAX77665_HAPTIC_REV		0x10


#define MAX77665_MOTOR_ON	MAX77665_HBUSY_MASK
#define MAX77665_MOTOR_OFF	0

#define MAX77665_HTYPE_MPWM	0
#define MAX77665_HTYPE_IPWM	MAX77665_HTYP_MASK
#define MAX77665_MODE_ERM	0
#define MAX77665_MODE_LRA	MAX77665_MODE_MASK

#define VIB_MAX_LEVEL_mV	3100
#define VIB_MIN_LEVEL_mV	1200

struct max77665_vib
{
	struct device *dev;
	struct regmap *regmap;
	struct hrtimer vib_timer;
	struct timed_output_dev timed_dev;
	spinlock_t lock;
	struct work_struct work;
	struct max77665_vib_platform_data *pdata;
	int state;
	int level;
	bool vreg_enabled;
	struct regulator *vreg_pwm;
};

static int max77665_vib_set(struct max77665_vib *vib, int on)
{
	int ret;

	if (on)
	{
		ret = pm_runtime_resume(vib->dev);
		if (IS_ERR_VALUE(ret))
			dev_warn(vib->dev, "pm_runtime_resume failed\n");

		// turn on LDO15 for internal PWM
		if (vib->vreg_enabled == false)
		{
			ret = regulator_enable(vib->vreg_pwm);
			if (IS_ERR_VALUE(ret))
			{
				pr_err("Failed to enable 77665_l15 regulator.\n");
				return ret;
			}
			vib->vreg_enabled = true;
		}

		// Set haptic enable
		ret = regmap_update_bits(vib->regmap, MAX77665_HAPTIC_CONFIG2, MAX77665_MEN_MASK, MAX77665_MEN_MASK);
		if (IS_ERR_VALUE(ret))
		{
			dev_err(vib->dev, "max77665_set_bits() returns failed : %d\n", ret);
			pm_runtime_suspend(vib->dev);
			return ret;
		}
	}
	else
	{
		ret = regmap_update_bits(vib->regmap, MAX77665_HAPTIC_CONFIG2,
				MAX77665_MEN_MASK, 0);
		if (IS_ERR_VALUE(ret))
		{
			dev_err(vib->dev, "max77665_set_bits() returns failed : %d\n", ret);
			return ret;
		}

	        // Turn off LDO15
	        if (vib->vreg_enabled)
	        {
	        	ret = regulator_disable(vib->vreg_pwm);
	        	if (IS_ERR_VALUE(ret))
	        	{
	        		pr_err("Failed to disable 77665_l15 regulator.\n");
	        		return ret;
	        	}
			vib->vreg_enabled = false;
	        }
		ret = pm_runtime_suspend(vib->dev);
		if (IS_ERR_VALUE(ret))
			dev_dbg(vib->dev, "pm_runtime_suspend failed\n");
	}

	return ret;
}

static void max77665_vib_enable(struct timed_output_dev *dev, int value)
{
	struct max77665_vib *vib = container_of(dev, struct max77665_vib,
					 timed_dev);
	unsigned long flags;

	int level;
	int timeoutms;

	spin_lock_irqsave(&vib->lock, flags);
	hrtimer_cancel(&vib->vib_timer);

	level = (value>>16) & 0xFFFF;
	timeoutms = value & 0xFFFF;

	if((level >= 800) && (level < 3300))
	{
	    	vib->level = level; // 800mV ~ 3300mV
	}
	else
	{
		vib->level = vib->pdata->level_mV;
	}

	if (value == 0)
		vib->state = 0;
	else
	{
		value = (value > vib->pdata->max_timeout_ms ?
				vib->pdata->max_timeout_ms : value);
		vib->state = 1;
		hrtimer_start(&vib->vib_timer,
				ktime_set(value / 1000, (value % 1000) * 1000000),
				HRTIMER_MODE_REL);
	}

	spin_unlock_irqrestore(&vib->lock, flags);
	schedule_work(&vib->work);
}

static void max77665_vib_update(struct work_struct *work)
{
	struct max77665_vib *vib = container_of(work, struct max77665_vib, work);

	max77665_vib_set(vib, vib->state);
}

static int max77665_vib_get_time(struct timed_output_dev *dev)
{
	struct max77665_vib *vib = container_of(dev, struct max77665_vib,
					 timed_dev);

	if (hrtimer_active(&vib->vib_timer))
	{
		ktime_t r = hrtimer_get_remaining(&vib->vib_timer);
		return ktime_to_ms(r);
	}
	else
		return 0;
}

static enum hrtimer_restart max77665_vib_timer_func(struct hrtimer *timer)
{
	struct max77665_vib *vib = container_of(timer, struct max77665_vib,
			vib_timer);
	vib->state = 0;
	schedule_work(&vib->work);
	return HRTIMER_NORESTART;
}

static int max77665_vib_hw_init(struct max77665_vib *max77665_vib)
{
	struct regmap *regmap = max77665_vib->regmap;
	struct max77665_vib_platform_data *pdata = max77665_vib->pdata;
	unsigned int val;
	int ret;

	/* set platform data */
	// Set SCF value
	ret = regmap_update_bits(regmap, MAX77665_HAPTIC_CONFIG1,
			MAX77665_SCF_MASK, (pdata->scf));
	if (IS_ERR_VALUE(ret))
		goto err;

	// Set Haptic type and motero tyep
	ret = regmap_update_bits(regmap, MAX77665_HAPTIC_CONFIG2,
			(MAX77665_HTYP_MASK | MAX77665_MODE_MASK),
			(pdata->input << MAX77665_HTYPE_SHIFT) | (pdata->motor << MAX77665_MODE_SHIFT));
	if (IS_ERR_VALUE(ret))
		goto err;

	// Write pattern registers
	// 1. cycle configuration
	val = (pdata->cycle[0]<<MAX77665_CYC0_SHIFT) | (pdata->cycle[1]<<MAX77665_CYC1_SHIFT);
	ret = regmap_write(regmap, MAX77665_HAPTIC_CONFIG_CYC1, val);
	if (IS_ERR_VALUE(ret))
		goto err;

	val = (pdata->cycle[2]<<MAX77665_CYC2_SHIFT) | (pdata->cycle[3]<<MAX77665_CYC3_SHIFT);
	ret = regmap_write(regmap, MAX77665_HAPTIC_CONFIG_CYC2, val);
	if (IS_ERR_VALUE(ret))
		goto err;

	// 2. signal configuration
	ret = regmap_bulk_write(regmap, MAX77665_HAPTIC_CONFIG_PER0, pdata->sigp, 4);
	if (IS_ERR_VALUE(ret))
		goto err;

	// 3. signal duty cycle configuration
	val = (pdata->sigdc[0]<<MAX77665_SIGDC0_SHIFT) | (pdata->sigdc[1]<<MAX77665_SIGDC1_SHIFT);
	ret = regmap_write(regmap, MAX77665_HAPTIC_CONFIG_DUTY1, val);
	if (IS_ERR_VALUE(ret))
		goto err;

	val = (pdata->sigdc[2]<<MAX77665_SIGDC2_SHIFT) | (pdata->sigdc[3]<<MAX77665_SIGDC3_SHIFT);
	ret = regmap_write(regmap, MAX77665_HAPTIC_CONFIG_DUTY2, val);
	if (IS_ERR_VALUE(ret))
		goto err;

	// 4. signal PWM duty communication
	ret = regmap_bulk_write(regmap, MAX77665_HAPTIC_CONFIG_PWMD0, pdata->pwmdc, 4);
	if (IS_ERR_VALUE(ret))
		goto err;

	// Choose default haptic configuration channel
	val = 0x00; // default channel pattern 0.
	ret = regmap_write(regmap, MAX77665_HAPTIC_CONFIG_CHNL, val);

	return ret;
err:
	dev_err(max77665_vib->dev, "%s : errors : %d\n", __func__, ret);
	return ret;
}

#ifdef CONFIG_PM
static int max77665_vib_suspend(struct device *dev)
{
	struct max77665_vib *vib = dev_get_drvdata(dev);

	hrtimer_cancel(&vib->vib_timer);
	cancel_work_sync(&vib->work);
	/* turn-off vibrator */
	max77665_vib_set(vib, 0);

	return 0;
}
#endif

const struct regmap_config max77665_vib_regmap_config =
{
	.reg_bits = 8,
	.val_bits = 8,
};

#ifdef CONFIG_OF
static struct max77665_vib_platform_data *max77665_vib_parse_dt(struct device *dev)
{
	struct max77665_vib_platform_data *pdata;
	struct device_node *np = dev->of_node;
	u32 val, arr[ARRAY_SIZE(pdata->pwmdc)];
	char *tmp;
	int ret;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

	ret = of_property_read_string(np, "input", &tmp);
	if (IS_ERR_VALUE(ret))
		return ret;
	if (!strcmp(tmp, "mpwm"))
		pdata->input = INPUT_MPWM;
	else if (!strcmp(tmp, "ipwm"))
		pdata->input = INPUT_IPWM;
	else
		return ERR_PTR(-EINVAL);

	ret = of_property_read_string(np, "motor", &tmp);
	if (IS_ERR_VALUE(ret))
		return ret;
	if (!strcmp(tmp, "ERM"))
		pdata->motor = MODE_ERM;
	else if (!strcmp(tmp, "LRA"))
		pdata->motor = MODE_LRA;
	else
		return ERR_PTR(-EINVAL);

	ret = of_property_read_u32(np, "scf", &val);
	if (IS_ERR_VALUE(ret))
		return ret;
	else if (unlikely(val < 1 || val > 8))
		return ERR_PTR(-EINVAL);
	pdata->scf = SCF_1CLOCK + val;

	ret = of_property_read_u32_array(np, "pwmdc", arr, ARRAY_SIZE(val));
	if (IS_ERR_VALUE(ret))
		return ret;
	for (i = 0; i < ARRAY_SIZE(val); i++)
	{
		if (val[i] < 1 || val[i] > 64)
			return ERR_PTR(-EINVAL);
		else
			pdata->pwmdc[i] = (u8)val[i];
	}

	ret = of_property_read_u32_array(np, "sigp", arr, ARRAY_SIZE(val));
	if (IS_ERR_VALUE(ret))
		return ret;
	for (i = 0; i < ARRAY_SIZE(val); i++)
	{
		if (val[i] > 255)
			return ERR_PTR(-EINVAL);
		else
			pdata->sigp[i] = (u8)val[i];
	}

	ret = of_property_read_u32_array(np, "sigdc", arr, ARRAY_SIZE(val));
	if (IS_ERR_VALUE(ret))
		return ret;
	for (i = 0; i < ARRAY_SIZE(val); i++)
	{
		if (val[i] > 15)
			return ERR_PTR(-EINVAL);
		else
			pdata->sigdc[i] = (u8)val[i];
	}

	ret = of_property_read_u32_array(np, "cycle", arr, ARRAY_SIZE(val));
	if (IS_ERR_VALUE(ret))
		return ret;
	for (i = 0; i < ARRAY_SIZE(val); i++)
	{
		if (val[i] < 1 || val[i] > 15)
			return ERR_PTR(-EINVAL);
		else
			pdata->cycle[i] = (u8)val[i];
	}

	ret = of_property_read_u32(np, "initial-vibrate-ms", &pdata->initial_vibrate_ms);
	if (IS_ERR_VALUE(ret))
		return ret;

	ret = of_property_read_u32(np, "max-timeout-ms", &pdata->max_timeout_ms);
	if (IS_ERR_VALUE(ret))
		return ret;

	ret = of_property_read_u32(np, "level-mV", &pdata->level_mV);
	if (IS_ERR_VALUE(ret))
		return ret;

	ret = of_property_read_string(np, "regulator-name", &pdata->regulator_name);
	if (IS_ERR_VALUE(ret))
		return ret;

	return pdata;
#endif

static int __devinit max77665_vib_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct max77665_vib_platform_data *pdata = dev_get_platdata(dev);
	struct max77665_vib *max77665_vib;
	struct regmap *regmap;
	int ret;

	if (unlikely(pdata->level_mV < VIB_MIN_LEVEL_mV ||
			pdata->level_mV > VIB_MAX_LEVEL_mV))
		return -EINVAL;

	max77665_vib = devm_kzalloc(dev, sizeof(struct max77665_vib), GFP_KERNEL);
	if (unlikely(!max77665_vib))
		return -ENOMEM;

	regmap = devm_regmap_init_i2c(client, &max77665_vib_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	max77665_vib->pdata = pdata;
	max77665_vib->level = pdata->level_mV;
	max77665_vib->dev = &client->dev;
	max77665_vib->regmap = regmap;
        max77665_vib->vreg_pwm = devm_regulator_get(dev, pdata->regulator_name);
	if (IS_ERR(max77665_vib->vreg_pwm))
		return PTR_ERR(max77665_vib->vreg_pwm);

       	regulator_set_voltage(max77665_vib->vreg_pwm, (int)max77665_vib->level*1000, (int)max77665_vib->level*1000);

	ret = max77665_vib_hw_init(max77665_vib);
	if (IS_ERR_VALUE(ret))
		return ret;

	/* Enable runtime PM ops, start in ACTIVE mode */
	ret = pm_runtime_set_active(&client->dev);
	if (IS_ERR_VALUE(ret))
		dev_warn(&client->dev, "unable to set runtime pm state\n");
	pm_runtime_enable(&client->dev);

	spin_lock_init(&max77665_vib->lock);
	INIT_WORK(&max77665_vib->work, max77665_vib_update);

	hrtimer_init(&max77665_vib->vib_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	max77665_vib->vib_timer.function = max77665_vib_timer_func;

	max77665_vib->timed_dev.name = "vibrator";
	max77665_vib->timed_dev.get_time = max77665_vib_get_time;
	max77665_vib->timed_dev.enable = max77665_vib_enable;

	ret = timed_output_dev_register(&max77665_vib->timed_dev);
	if (IS_ERR_VALUE(ret))
		goto err_read_vib;

	max77665_vib_enable(&max77665_vib->timed_dev, pdata->initial_vibrate_ms);

	i2c_set_clientdata(client, max77665_vib);

	pm_runtime_set_suspended(&client->dev);
	return 0;

err_read_vib:
	pm_runtime_set_suspended(&client->dev);
	pm_runtime_disable(&client->dev);
	return ret;
}

static int __devexit max77665_vib_remove(struct i2c_client *client)
{
	struct max77665_vib *vib = i2c_get_clientdata(client);

	pm_runtime_disable(&client->dev);
	hrtimer_cancel(&vib->vib_timer);
	cancel_work_sync(&vib->work);
	timed_output_dev_unregister(&vib->timed_dev);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id max77665_vib_of_match[] = {
        { .compatible = "maxim,max77665-vibrator" },
        { },
};
MODULE_DEVICE_TABLE(of, max77665_vib_of_match);
#endif

static const struct i2c_device_id max77665_vib_ids[] = {
    { "max77665-vibrator", 0 },
    { },
};
MODULE_DEVICE_TABLE(i2c, max77665_ids);

#ifdef CONFIG_PM
static struct dev_pm_ops max77665_vib_pm = {
    .suspend = max77665_vib_suspend,
};
#endif

static struct i2c_driver max77665_vib_driver = {
	.driver =
	{
		.name = "max77665-vibrator",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = max77665_vib_of_match,
#endif
#ifdef CONFIG_PM
		.driver.pm = &max77665_vib_pm,
#endif
	},
	.id_table = max77665_vib_ids,
	.probe = max77665_vib_probe,
	.remove = __devexit_p(max77665_vib_remove),
};

static int __init max77665_vib_init(void)
{
	return i2c_add_driver(&max77665_vib_driver);
}
module_init(max77665_vib_init);

static void __exit max77665_vib_exit(void)
{
	i2c_del_driver(&max77665_vib_driver);
}
module_exit(max77665_vib_exit);

MODULE_ALIAS("platform:max77665-vibrator");
MODULE_AUTHOR("Gyungoh Yoo <jack.yoo@maxim-ic.com>");
MODULE_DESCRIPTION("MAX77665 vibrator driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.2");
