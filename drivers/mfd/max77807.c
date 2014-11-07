/*
 * Maxim MAX77807 MFD Core
 *
 * Copyright (C) 2014 Maxim Integrated Product
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This driver is based on max77807.c
 */

//#define DEBUG
//#define VERBOSE_DEBUG
#define LOG_LEVEL  2


#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>

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
#include <linux/mfd/max77807/max77807.h>

#define DRIVER_DESC    "MAX77807 MFD Driver"
#define DRIVER_NAME    MAX77807_NAME
#define DRIVER_VERSION "1.0"
#define DRIVER_AUTHOR  "TaiEup Kim <clark.kim@maximintegrated.com>"

/* REGMAP IPC */
#define MAX77807_I2C_SLOT			4
#define MAX77807_PMIC_SLAVE_ADDR	0xCC
#define MAX77807_PERI_SLAVE_ADDR	0x90
#define MAX77807_MUIC_SLAVE_ADDR	0x4A
#define MAX77807_FUEL_I2C_SLOT		5
#define MAX77807_FUEL_SLAVE_ADDR	0x6C
enum {
    MAX77807_DEV_PMIC = 0,  /* PMIC (Charger, SFO) */
    MAX77807_DEV_PERIPH,    /* Motor */
	MAX77807_DEV_MUIC,
    MAX77807_DEV_FUELGAUGE, /* Fuel Gauge */
    /***/
    MAX77807_DEV_NUM_OF_DEVICES,
};

//struct max77807_dev;

struct max77807_core {
    struct mutex         lock;
    struct max77807_dev *dev[MAX77807_DEV_NUM_OF_DEVICES];
};
/*
struct max77807_dev {
    struct max77807_core        *core;
    int                          dev_id;
    void                        *pdata;
    struct mutex                 lock;
    struct device               *dev;
    struct max77807_io           io;
    struct kobject              *kobj;
    struct attribute_group      *attr_grp;
    struct regmap_irq_chip_data *irq_data;
	struct regmap               *regmap;
    int                          irq;
    int                          irq_gpio;
    struct irq_domain           *irq_domain;
};
*/
#define __lock(_me)    mutex_lock(&(_me)->lock)
#define __unlock(_me)  mutex_unlock(&(_me)->lock)

static struct max77807_core max77807;

static const struct regmap_config max77807_regmap_config = {
    .reg_bits   = 8,
    .val_bits   = 8,
    .cache_type = REGCACHE_NONE,
};

static int max77807_add_devices (struct max77807_dev *me,
    struct mfd_cell *cells, int n_devs)
{
    struct device *dev = me->dev;
    int rc;

    if (!dev->of_node) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0)
        rc = mfd_add_devices(dev, -1, cells, n_devs, NULL, 0);
#else /* LINUX_VERSION_CODE ... */
        rc = mfd_add_devices(dev, -1, cells, n_devs, NULL, 0, NULL);
#endif /* LINUX_VERSION_CODE ... */
        goto out;
    }

    rc = of_platform_populate(dev->of_node, NULL, NULL, dev);

out:
    return rc;
}

/*******************************************************************************
 *** MAX77807 PMIC
 ******************************************************************************/

/* Register map */
#define PMICID              0x20
#define PMICREV             0x21
#define INTSRC              0x22
#define INTSRC_MASK         0x23
#define TOPSYS_INT          0x24
//      RESERVED            0x25
#define TOPSYS_INT_MASK     0x26
//      RESERVED            0x27
#define TOPSYS_STAT         0x28
//      RESERVED            0x29
#define MAINCTRL1           0x2A
#define LSCONFIG            0x2B

/* Interrupt corresponding bit */
#define CHGR_INT            BIT (0)
#define TOP_INT             BIT (1)
#ifdef CONFIG_MAX77807_USBSW
#define USBSW_INT			BIT (3)
#endif

static void *max77807_pmic_get_platdata (struct max77807_dev *pmic)
{
#ifdef CONFIG_OF
    struct device *dev = pmic->dev;
    struct device_node *np = dev->of_node;
    struct max77807_pmic_platform_data *pdata;
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

static struct regmap_irq max77807_pmic_regmap_irqs[] = {
    #define REGMAP_IRQ_PMIC(_irq) \
            [MAX77807_IRQ_##_irq] = { .mask = _irq##_INT, }

    REGMAP_IRQ_PMIC(CHGR),
    REGMAP_IRQ_PMIC(TOP),
#ifdef CONFIG_MAX77807_USBSW
    REGMAP_IRQ_PMIC(USBSW),
#endif
};

static struct regmap_irq_chip max77807_pmic_regmap_irq_chip = {
    .name        = DRIVER_NAME,
    .irqs        = max77807_pmic_regmap_irqs,
    .num_irqs    = ARRAY_SIZE(max77807_pmic_regmap_irqs),
    .num_regs    = 1,
    .status_base = INTSRC,
    .mask_base   = INTSRC_MASK,
};

static int max77807_pmic_setup_irq (struct max77807_dev *pmic)
{
    struct device *dev = pmic->dev;
    struct max77807_pmic_platform_data *pdata = pmic->pdata;
    int rc = 0;

    /* disable all interrupts */
    max77807_write(&pmic->io, INTSRC_MASK, 0xFF);

    pmic->irq = pdata->irq;
    if (unlikely(pmic->irq <= 0)) {
        dev_err(dev, "interrupt disabled \n");
        goto out;
    }

    dev_info(dev, "<%s> requesting IRQ %d\n", __func__, pmic->irq);

    rc = regmap_add_irq_chip(pmic->io.regmap, pmic->irq,
        	IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_SHARED, -1,
						&max77807_pmic_regmap_irq_chip, &pmic->irq_data);
    if (unlikely(IS_ERR_VALUE(rc))) {
        dev_err("<%s> failed to add regmap irq chip [%d]\n", __func__,
            rc);
        pmic->irq = -1;
        goto out;
    }

out:
    return rc;
}

static struct mfd_cell max77807_pmic_devices[] = {
    { .name = MAX77807_CHARGER_NAME, },
    { .name = MAX77807_SFO_NAME,     },
};

static int max77807_pmic_setup (struct max77807_dev *pmic)
{
    struct device *dev = pmic->dev;
    int rc = 0;
    u8 chip_id = 0, chip_rev = 0;

    pmic->pdata = max77807_pmic_get_platdata(pmic);
    if (unlikely(IS_ERR(pmic->pdata))) {
        rc = PTR_ERR(pmic->pdata);
        pmic->pdata = NULL;
        dev_err(dev, "platform data is missing [%d]\n", rc);
        goto out;
    }

/*    rc = max77807_pmic_setup_irq(pmic);
    if (unlikely(rc)) {
        goto out;
    }
*/
    rc = max77807_add_devices(pmic, max77807_pmic_devices,
            ARRAY_SIZE(max77807_pmic_devices));
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

    chip_id = 0;
    chip_rev = 0;

    max77807_read(&pmic->io, PMICID,  &chip_id );
    max77807_read(&pmic->io, PMICREV, &chip_rev);

    printk("CHIP ID %Xh REV %Xh\n", chip_id, chip_rev);

out:
    return rc;
}

/*******************************************************************************
 *** MAX77807 Periph
 ******************************************************************************/

static struct mfd_cell max77807_periph_devices[] = {
    { .name = MAX77807_MOTOR_NAME, },
};

static int max77807_periph_setup (struct max77807_dev *periph)
{
    struct device *dev = periph->dev;
    int rc = 0;

    rc = max77807_add_devices(periph, max77807_periph_devices,
            ARRAY_SIZE(max77807_periph_devices));
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

#ifdef CONFIG_MAX77807_USBSW
/*******************************************************************************
 *** MAX77807 MUIC
 ******************************************************************************/

static struct mfd_cell max77807_muic_devices[] = {
    { .name = MAX77807_USBSW_NAME, },
};

static int max77807_muic_setup (struct max77807_dev *muic)
{
    struct device *dev = muic->dev;
    int rc = 0;

    rc = max77807_add_devices(muic, max77807_muic_devices,
            ARRAY_SIZE(max77807_muic_devices));
    if (unlikely(IS_ERR_VALUE(rc))) {
        log_err("<%s> failed to add sub-devices [%d]\n",__func__ , rc);
        goto out;
    }

    /* set device able to wake up system */
//  device_init_wakeup(dev, true);

    printk("<%s> driver core "DRIVER_VERSION" installed\n", __func__);

out:
    return rc;
}
#endif
/*******************************************************************************
 *** MAX77807 FuelGauge
 ******************************************************************************/

static int max77807_fuelgauge_setup (struct max77807_dev *fuelgauge)
{
    return -ENOTSUPP;
}

/*******************************************************************************
 *** MAX77807 MFD Core
 ******************************************************************************/

static __always_inline void max77807_destroy (struct max77807_dev *me)
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

static const struct platform_device_id max77807_ids[] = {
    { MAX77807_PMIC_NAME,      MAX77807_DEV_PMIC      },
    { MAX77807_PERIPH_NAME,    MAX77807_DEV_PERIPH    },
#ifdef CONFIG_MAX77807_USBSW
    { MAX77807_MUIC_NAME,		MAX77807_DEV_MUIC   },
#endif
    { MAX77807_FUELGAUGE_NAME, MAX77807_DEV_FUELGAUGE },
    { },
};
MODULE_DEVICE_TABLE(platform, max77807_ids);

#ifdef CONFIG_OF
static struct of_device_id max77807_of_ids[] = {
    { .compatible = "maxim,"MAX77807_PMIC_NAME,
      .data = &max77807_ids[MAX77807_DEV_PMIC],
    },
    { .compatible = "maxim,"MAX77807_PERIPH_NAME,
      .data = &max77807_ids[MAX77807_DEV_PERIPH],
    },
#ifdef CONFIG_MAX77807_USBSW
    { .compatible = "maxim,"MAX77807_MUIC_NAME,
      .data = &max77807_ids[MAX77807_DEV_MUIC],

	},
#endif
    { .compatible = "maxim,"MAX77807_FUELGAUGE_NAME,
      .data = &max77807_ids[MAX77807_DEV_FUELGAUGE],
    },
    { },
};
MODULE_DEVICE_TABLE(of, max77807_of_ids);
#endif /* CONFIG_OF */

static int max77807_ipc_probe (struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct ipc_client *regmap_ipc;
    struct max77807_core *core = &max77807;
    struct max77807_dev *me;
    const struct of_device_id *of_id;
    int rc;


    of_id = of_match_device(max77807_of_ids, &pdev->dev);
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
        case MAX77807_DEV_PMIC:
            pr_info("%s setup\n", MAX77807_PMIC_NAME);

            regmap_ipc->slot = MAX77807_I2C_SLOT;
            regmap_ipc->addr = MAX77807_PMIC_SLAVE_ADDR;
            regmap_ipc->dev  = dev;

            me->io.regmap = devm_regmap_init_ipc(regmap_ipc,
								&max77807_regmap_config);
            if (unlikely(IS_ERR(me->io.regmap))) {
                rc = PTR_ERR(me->io.regmap);
                me->io.regmap = NULL;
                pr_err("%s: failed to initialize regmap [%d]\n", __func__, rc);
                goto abort;
            }

            rc = max77807_pmic_setup(me);
            break;

        case MAX77807_DEV_PERIPH:
            pr_info("%s setup\n", MAX77807_PERIPH_NAME);

            regmap_ipc->slot = MAX77807_I2C_SLOT;
            regmap_ipc->addr = MAX77807_PERI_SLAVE_ADDR;
            regmap_ipc->dev  = dev;

            me->io.regmap = devm_regmap_init_ipc(regmap_ipc,
								&max77807_regmap_config);
            if (unlikely(IS_ERR(me->io.regmap))) {
                rc = PTR_ERR(me->io.regmap);
                me->io.regmap = NULL;
                pr_err("%s: failed to initialize regmap [%d]\n", __func__, rc);
                goto abort;
            }

            rc = max77807_periph_setup(me);
            break;
#ifdef CONFIG_MAX77807_USBSW
	case MAX77807_DEV_MUIC:
        regmap_ipc->slot = MAX77807_I2C_SLOT;
        regmap_ipc->addr = MAX77807_MUIC_SLAVE_ADDR;
        regmap_ipc->dev  = dev;

		me->io.regmap = devm_regmap_init_ipc(regmap_ipc,
								&max77807_regmap_config);
            if (unlikely(IS_ERR(me->io.regmap))) {
                rc = PTR_ERR(me->io.regmap);
                me->io.regmap = NULL;
                pr_err("%s: failed to initialize regmap [%d]\n", __func__, rc);
                goto abort;
            }
		rc = max77807_muic_setup(me);
		break;
#endif

/*        case MAX77807_DEV_FUELGAUGE:
            pr_info("%s setup\n", MAX77807_FUELGAUGE_NAME);

            regmap_ipc->slot = MAX77807_FUEL_I2C_SLOT;
            regmap_ipc->addr = MAX77807_FUEL_SLAVE_ADDR;
            regmap_ipc->dev  = pdev->dev;

            me->io.regmap = devm_regmap_init_ipc(regmap_ipc,
								&max77807_regmap_config);
            if (unlikely(IS_ERR(me->io.regmap))) {
                rc = PTR_ERR(me->io.regmap);
                me->io.regmap = NULL;
                pr_err("%s: failed to initialize regmap [%d]\n", __func__, rc);
                goto abort;
            }

            rc = max77807_fuelgauge_setup(me);
            break;
*/
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
    max77807_destroy(me);
    return rc;
}

static int max77807_ipc_remove (struct platform_device *pdev)
{
    struct max77807_dev *me = platform_get_drvdata(pdev);

    me->core->dev[me->dev_id] = NULL;

    max77807_destroy(me);

    return 0;
}

#ifdef CONFIG_PM_SLEEP
static int max77807_suspend (struct device *dev)
{
    struct max77807_dev *me = dev_get_drvdata(dev);
    struct i2c_client *client = to_i2c_client(dev);

    __lock(me);

    //log_vdbg("<%s> suspending\n", client->name);

    __unlock(me);
    return 0;
}

static int max77807_resume (struct device *dev)
{
    struct max77807_dev *me = dev_get_drvdata(dev);
    struct i2c_client *client = to_i2c_client(dev);

    __lock(me);

    //log_vdbg("<%s> resuming\n", client->name);

    __unlock(me);
    return 0;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(max77807_pm, max77807_suspend, max77807_resume);

static struct platform_driver max77807_ipc_driver = {
    .probe           = max77807_ipc_probe,
    .remove          = max77807_ipc_remove,
    .id_table        = max77807_ids,
    .driver = {
        .name            = DRIVER_NAME,
        .owner           = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = max77807_of_ids,
#endif /* CONFIG_OF */
    },
};

static __init int max77807_init (void)
{
    mutex_init(&max77807.lock);

	return platform_driver_register(&max77807_ipc_driver);
}
#if defined(CONFIG_MACH_ODIN_LIGER)
subsys_initcall(max77807_init);
#else
module_init(max77807_init);
#endif

static __exit void max77807_exit (void)
{
	platform_driver_unregister(&max77807_ipc_driver);
    mutex_destroy(&max77807.lock);
}
module_exit(max77807_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_VERSION(DRIVER_VERSION);

/*******************************************************************************
 * EXTERNAL SERVICES
 ******************************************************************************/

struct max77807_io *max77807_get_io (struct max77807_dev *chip)
{
    if (unlikely(!chip)) {
        log_err("not ready\n");
        return NULL;
    }

    return &chip->io;
}
