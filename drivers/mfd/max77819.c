/*
 * Maxim MAX77819 MFD Core
 *
 * Copyright (C) 2013 Maxim Integrated Product
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define LOG_LEVEL  1

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>

/* for Regmap */
#include <linux/regmap.h>
#include <linux/regmap-ipc.h>

/* for Device Tree */
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/irqdomain.h>

#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/mfd/core.h>
#include <linux/mfd/max77819.h>

#define DRIVER_DESC    "MAX77819 MFD Driver"
#define DRIVER_NAME    MAX77819_NAME
#define DRIVER_VERSION MAX77819_DRIVER_VERSION".1-rc"
#define DRIVER_AUTHOR  "Gyungoh Yoo <jack.yoo@maximintegrated.com>"

/* REGMAP IPC */
#define MAX77819_I2C_SLOT			4
#define MAX77819_PMIC_SLAVE_ADDR	0xCC
#define MAX77819_PERI_SLAVE_ADDR	0x90
#define MAX77819_FUEL_I2C_SLOT		5
#define MAX77819_FUEL_SLAVE_ADDR	0x6C

enum {
    MAX77819_DEV_PMIC = 0,  /* PMIC (Charger, Flash LED) */
    MAX77819_DEV_PERIPH,    /* WLED, Motor */
    MAX77819_DEV_FUELGAUGE, /* Fuel Gauge */
    /***/
    MAX77819_DEV_NUM_OF_DEVICES,
};

struct max77819_dev;

struct max77819_core {
    struct mutex         lock;
    struct max77819_dev *dev[MAX77819_DEV_NUM_OF_DEVICES];
};

#define __lock(_me)    mutex_lock(&(_me)->lock)
#define __unlock(_me)  mutex_unlock(&(_me)->lock)

static struct max77819_core max77819;

static const struct regmap_config max77819_regmap_config = {
    .reg_bits   = 8,
    .val_bits   = 8,
    .cache_type = REGCACHE_NONE,
};

static int max77819_add_devices (struct max77819_dev *me,
    struct mfd_cell *cells, int n_devs)
{
    struct device *dev = me->dev;
    int rc;

    if (!dev->of_node) {
        rc = mfd_add_devices(dev, -1, cells, n_devs, NULL, 0, NULL);
        goto out;
    }

    rc = of_platform_populate(dev->of_node, NULL, NULL, dev);

out:
    return rc;
}

/*******************************************************************************
 *** MAX77819 PMIC
 ******************************************************************************/

/* Register map */
#define PMICID              0x20
#define PMICREV             0x21
#define INTSRC              0x22
#define INTSRC_MASK         0x23
#define TOPSYS_INT          0x24
#define TOPSYS_INT_MASK     0x26
#define TOPSYS_STAT         0x28
#define MAINCTRL1           0x2A
#define LSCONFIG            0x2B

/* Interrupt corresponding bit */
#define CHGR_INT            BIT (0)
#define TOP_INT             BIT (1)
#define FLASH_INT           BIT (2)
#define WLED_INT            BIT (4)

static void *max77819_pmic_get_platdata (struct max77819_dev *pmic)
{
#ifdef CONFIG_OF
    struct device *dev = pmic->dev;
    struct device_node *np = dev->of_node;
    struct max77819_pmic_platform_data *pdata;
    int rc;

    pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
    if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

	rc = of_property_read_u32(np, "irq_gpio", &pmic->irq_gpio);

    if (pmic->irq_gpio < 0) {
        pdata->irq = irq_of_parse_and_map(np, 0);
    } else {
        unsigned gpio = (unsigned)pmic->irq_gpio;

        rc = gpio_request(gpio, DRIVER_NAME"-irq");
        if (unlikely(IS_ERR_VALUE(rc))) {
            dev_err(dev, "<%s> failed to request gpio %u [%d]\n", __func__,
				gpio, rc);
            pmic->irq_gpio = -1;
            pdata = ERR_PTR(rc);
            goto out;
        }

        gpio_direction_input(gpio);

        /* override pdata irq */
        pdata->irq = gpio_to_irq(gpio);

		/* Debounce time is set by 24MHz*/
		gpio_set_debounce(gpio, 1);
    }

out:
    return pdata;
#else /* CONFIG_OF */
    return dev_get_platdata(pmic->dev) ?
        dev_get_platdata(pmic->dev) : ERR_PTR(-EINVAL);
#endif /* CONFIG_OF */
}

static struct regmap_irq max77819_pmic_regmap_irqs[] = {
    #define REGMAP_IRQ_PMIC(_irq) \
            [MAX77819_IRQ_##_irq] = { .mask = _irq##_INT, }

    REGMAP_IRQ_PMIC(CHGR),
    REGMAP_IRQ_PMIC(TOP),
    REGMAP_IRQ_PMIC(FLASH),
    REGMAP_IRQ_PMIC(WLED),
};

static struct regmap_irq_chip max77819_pmic_regmap_irq_chip = {
    .name        = DRIVER_NAME,
    .irqs        = max77819_pmic_regmap_irqs,
    .num_irqs    = ARRAY_SIZE(max77819_pmic_regmap_irqs),
    .num_regs    = 1,
    .status_base = INTSRC,
    .mask_base   = INTSRC_MASK,
};

static int max77819_pmic_setup_irq (struct max77819_dev *pmic)
{
    struct device *dev = pmic->dev;
    struct max77819_pmic_platform_data *pdata = pmic->pdata;
    int rc = 0;

    /* disable all interrupts */
    max77819_write(&pmic->io, INTSRC_MASK, 0xFE);

    pmic->irq = pdata->irq;
    if (unlikely(pmic->irq <= 0)) {
        dev_err(dev, "interrupt disabled \n");
        goto out;
    }

    dev_info(dev, "<%s> requesting IRQ %d\n", __func__, pmic->irq);

    rc = regmap_add_irq_chip(pmic->io.regmap, pmic->irq,
        IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_SHARED, -1,
		&max77819_pmic_regmap_irq_chip, &pmic->irq_data);
    if (unlikely(IS_ERR_VALUE(rc))) {
        dev_err("<%s> failed to add regmap irq chip [%d]\n", __func__,
            rc);
        pmic->irq = -1;
        goto out;
    }

out:
    return rc;
}

static struct mfd_cell max77819_pmic_devices[] = {
    { .name = MAX77819_CHARGER_NAME, },
/*  { .name = MAX77819_SFO_NAME,     }, */
/*  { .name = MAX77819_FLASH_NAME,   }, */
};

static int max77819_pmic_setup (struct max77819_dev *pmic)
{
    struct device *dev = pmic->dev;
    int rc = 0;
    u8 chip_id = 0, chip_rev = 0;

    pmic->pdata = max77819_pmic_get_platdata(pmic);
    if (unlikely(IS_ERR(pmic->pdata))) {
        rc = PTR_ERR(pmic->pdata);
        pmic->pdata = NULL;
        dev_err(dev, "platform data is missing [%d]\n", rc);
        goto out;
    }

    rc = max77819_pmic_setup_irq(pmic);
    if (unlikely(rc)) {
        goto out;
    }

    rc = max77819_add_devices(pmic, max77819_pmic_devices,
            ARRAY_SIZE(max77819_pmic_devices));
    if (unlikely(IS_ERR_VALUE(rc))) {
        dev_err(dev, "<%s> failed to add sub-devices [%d]\n", __func__, rc);
        goto out;
	}

    /* set device able to wake up system */
    device_init_wakeup(dev, true);
    if (likely(pmic->irq > 0)) {
        enable_irq_wake((unsigned int)pmic->irq);
    }

    dev_info(dev, "<%s> driver core "DRIVER_VERSION" installed\n", __func__);

out:
    return rc;
}

/*******************************************************************************
 *** MAX77819 Periph
 ******************************************************************************/

static struct mfd_cell max77819_periph_devices[] = {
    { .name = MAX77819_WLED_NAME,  },
/*  { .name = MAX77819_MOTOR_NAME, }, */
};

static int max77819_periph_setup (struct max77819_dev *periph)
{
    struct device *dev = periph->dev;
    int rc = 0;

    rc = max77819_add_devices(periph, max77819_periph_devices,
            ARRAY_SIZE(max77819_periph_devices));
    if (unlikely(IS_ERR_VALUE(rc))) {
        dev_err(dev, "(%s) failed to add sub-devices [%d]\n", __func__, rc);
        goto out;
    }

    /* set device able to wake up system */
	device_init_wakeup(dev, true);

    dev_info(dev, "<%s> driver core "DRIVER_VERSION" installed\n", __func__);

out:
    return rc;
}

/*******************************************************************************
 *** MAX77819 FuelGauge
 ******************************************************************************/

static int max77819_fuelgauge_setup (struct max77819_dev *fuelgauge)
{
    return -ENOTSUPP;
}

/*******************************************************************************
 *** MAX77819 MFD Core
 ******************************************************************************/

static __always_inline void max77819_destroy (struct max77819_dev *me)
{
    struct device *dev = me->dev;

    if (likely(me->irq > 0)) {
        if (me->irq_data) {
            regmap_del_irq_chip(me->irq, me->irq_data);
        } else {
            devm_free_irq(dev, me->irq, me);
        }
    }

    if (likely(me->irq_gpio >= 0)) {
        gpio_free((unsigned)me->irq_gpio);
    }

    if (likely(me->attr_grp)) {
        sysfs_remove_group(me->kobj, me->attr_grp);
    }

    if (likely(me->io.regmap)) {
        regmap_exit(me->io.regmap);
    }

#ifdef CONFIG_OF
    if (likely(me->pdata)) {
        devm_kfree(dev, me->pdata);
    }
#endif /* CONFIG_OF */

    mutex_destroy(&me->lock);
    devm_kfree(dev, me);
}

static const struct platform_device_id max77819_ids[] = {
    { MAX77819_PMIC_NAME,      MAX77819_DEV_PMIC      },
    { MAX77819_PERIPH_NAME,    MAX77819_DEV_PERIPH    },
    { MAX77819_FUELGAUGE_NAME, MAX77819_DEV_FUELGAUGE },
    { },
};
MODULE_DEVICE_TABLE(platform, max77819_ids);

#ifdef CONFIG_OF
static struct of_device_id max77819_of_ids[] = {
    { .compatible = "maxim,"MAX77819_PMIC_NAME,
      .data = &max77819_ids[MAX77819_DEV_PMIC],
    },
    { .compatible = "maxim,"MAX77819_PERIPH_NAME,
      .data = &max77819_ids[MAX77819_DEV_PERIPH],
    },
    { .compatible = "maxim,"MAX77819_FUELGAUGE_NAME,
      .data = &max77819_ids[MAX77819_DEV_FUELGAUGE],
    },
    { },
};
MODULE_DEVICE_TABLE(of, max77819_of_ids);
#endif /* CONFIG_OF */

static int max77819_ipc_probe (struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct ipc_client *regmap_ipc;
    struct max77819_core *core = &max77819;
    struct max77819_dev *me;
    const struct of_device_id *of_id;
    int rc;

    of_id = of_match_device(max77819_of_ids, &pdev->dev);
    if (of_id)
        pdev->id_entry = of_id->data;

    me = devm_kzalloc(dev, sizeof(*me), GFP_KERNEL);
    if (unlikely(!me)) {
        dev_err(dev, "<%s> out of memory \n", __func__);
        return -ENOMEM;
    }

    platform_set_drvdata(pdev, me);

    mutex_init(&me->lock);
    me->core     = core;
    me->dev      = &pdev->dev;
    me->kobj     = &pdev->dev.kobj;
    me->irq      = -1;
    me->irq_gpio = -1;

    regmap_ipc = kzalloc(sizeof(struct ipc_client), GFP_KERNEL);
    if (regmap_ipc == NULL) {
        pr_err("%s : Platform data not specified.\n", __func__);
        return -ENOMEM;
    }

    /* detect device ID & post-probe */
    me->dev_id = (int)pdev->id_entry->driver_data;

    switch (me->dev_id) {
        case MAX77819_DEV_PMIC:
            pr_info("%s setup\n", MAX77819_PMIC_NAME);

            regmap_ipc->slot = MAX77819_I2C_SLOT;
            regmap_ipc->addr = MAX77819_PMIC_SLAVE_ADDR;
            regmap_ipc->dev  = dev;

            me->io.regmap = devm_regmap_init_ipc(regmap_ipc,
								&max77819_regmap_config);
            if (unlikely(IS_ERR(me->io.regmap))) {
                rc = PTR_ERR(me->io.regmap);
                me->io.regmap = NULL;
                pr_err("%s: failed to initialize regmap [%d]\n", __func__, rc);
                goto abort;
            }

            rc = max77819_pmic_setup(me);
            break;

        case MAX77819_DEV_PERIPH:
            pr_info("%s setup\n", MAX77819_PERIPH_NAME);

            regmap_ipc->slot = MAX77819_I2C_SLOT;
            regmap_ipc->addr = MAX77819_PERI_SLAVE_ADDR;
            regmap_ipc->dev  = dev;

            me->io.regmap = devm_regmap_init_ipc(regmap_ipc,
								&max77819_regmap_config);
            if (unlikely(IS_ERR(me->io.regmap))) {
                rc = PTR_ERR(me->io.regmap);
                me->io.regmap = NULL;
                pr_err("%s: failed to initialize regmap [%d]\n", __func__, rc);
                goto abort;
            }

            rc = max77819_periph_setup(me);
            break;

        case MAX77819_DEV_FUELGAUGE:
            pr_info("%s setup\n", MAX77819_FUELGAUGE_NAME);

            regmap_ipc->slot = MAX77819_FUEL_I2C_SLOT;
            regmap_ipc->addr = MAX77819_FUEL_SLAVE_ADDR;
            regmap_ipc->dev  = dev;

            me->io.regmap = devm_regmap_init_ipc(regmap_ipc,
								&max77819_regmap_config);
            if (unlikely(IS_ERR(me->io.regmap))) {
                rc = PTR_ERR(me->io.regmap);
                me->io.regmap = NULL;
                pr_err("%s: failed to initialize regmap [%d]\n", __func__, rc);
                goto abort;
            }

            rc = max77819_fuelgauge_setup(me);
            break;

        default:
            pr_err("%s: %s unknown device\n", __func__,pdev->name );
            BUG();
            rc = -ENOTSUPP;
            goto abort;
    }

    /* all done successfully */
    core->dev[me->dev_id] = me;

    return 0;

abort:
    platform_set_drvdata(pdev, NULL);
    max77819_destroy(me);
    return rc;
}

static int max77819_ipc_remove (struct platform_device *pdev)
{
    struct max77819_dev *me = platform_get_drvdata(pdev);

    me->core->dev[me->dev_id] = NULL;

    max77819_destroy(me);

    return 0;
}

static struct platform_driver max77819_ipc_driver = {
    .probe           = max77819_ipc_probe,
    .remove          = max77819_ipc_remove,
    .id_table        = max77819_ids,
    .driver = {
        .name            = DRIVER_NAME,
        .owner           = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = max77819_of_ids,
#endif /* CONFIG_OF */
    },
};

static int __init max77819_init(void)
{
	mutex_init(&max77819.lock);
	return platform_driver_register(&max77819_ipc_driver);
}
#if defined(CONFIG_MACH_ODIN_LIGER)
subsys_initcall(max77819_init);
#else
module_init(max77819_init);
#endif

static void __exit max77819_exit(void)
{
	platform_driver_unregister(&max77819_ipc_driver);
    mutex_destroy(&max77819.lock);
}
module_exit(max77819_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_VERSION(DRIVER_VERSION);

/*******************************************************************************
 * EXTERNAL SERVICES
 ******************************************************************************/

struct max77819_io *max77819_get_io (struct max77819_dev *chip)
{
    if (unlikely(!chip)) {
        log_err("not ready\n");
        return NULL;
    }

    return &chip->io;
}
