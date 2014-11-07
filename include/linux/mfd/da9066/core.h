/*
 * core.h  --  Core Driver for Dialog Semiconductor DA9066 PMIC
 *
 * Copyright 2013 Dialog Semiconductor Ltd
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __DA9066_CORE_H_
#define __DA9066_CORE_H_

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/mfd/da9066/pmic.h>
#include <linux/mfd/da9066/rtc.h>


#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif /*CONFIG_HAS_EARLYSUSPEND*/

/*
 * Register values.
 */

#define __ODIN__		/* for ODIN project */

#define I2C								1

#ifdef CONFIG_SND_SOC_DA9066_AAD_MODULE
#define CONFIG_SND_SOC_DA9066_AAD
#endif

#define DA9066_IPC						"da9066"
#define DA9066_DRVNAME_CORE				"da9066-core"
#define DA9066_DRVNAME_REGULATORS		"da9066-regulators"


/* Module specific error codes */
#define INVALID_REGISTER				2
#define INVALID_READ					3
#define INVALID_PAGE					4

/* Total number of registers in DA9066 */
#define DA9066_MAX_REGISTER_CNT			0x0100

#define DA9066_PMIC_I2C_ADDR				(0x92 >> 1)

#define DA9066_IOCTL_READ_REG  0xc0025083
#define DA9066_IOCTL_WRITE_REG 0x40025084


/*
 * DA9066 Number of Interrupts
 */
enum da9066_models {
	PMIC_DA9066 = 0x80,
};

enum da9066_irq_number {
	/* EVENT_A register IRQ */
	DA9066_IRQ_EVF = 0,
	DA9066_IRQ_EADCIN1,
	DA9066_IRQ_ETBAT2,
	DA9066_IRQ_EVDD_LOW,
	DA9066_IRQ_EVDD_MON,
	DA9066_IRQ_EALARM,
	DA9066_IRQ_ESEQRDY,
	DA9066_IRQ_ETICK,

	/* EVENT_B register IRQ */
	DA9066_IRQ_ENONKEY_LO,
	DA9066_IRQ_ENONKEY_HI,
	DA9066_IRQ_ENONKEY_HOLDON,
	DA9066_IRQ_ENONKEY_HOLDOFF,
	DA9066_IRQ_ETBAT1,
	DA9066_IRQ_EADCEOM,

	/* EVENT_C register IRQ */
	DA9066_IRQ_ETA,
	DA9066_IRQ_ENJIGON,
	DA9066_IRQ_EACCDET,
	DA9066_IRQ_EJACKDET,

	DA9066_NUM_IRQ
};


#define DA9066_MCTL_MODE_INIT(_reg_id, _dsm_mode, _default_pm_mode) \
	[_reg_id] = { \
		.reg_id = _reg_id, \
		.dsm_opmode = _dsm_mode, \
		.default_pm_mode = _default_pm_mode, \
	}

/* for DEBUGGING and Troubleshooting */
#if 1 /* defined(DEBUG) */
#define dlg_crit(fmt, ...) printk(KERN_CRIT fmt, ##__VA_ARGS__)
#define dlg_err(fmt, ...) printk(KERN_ERR fmt, ##__VA_ARGS__)
#define dlg_warn(fmt, ...) printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define dlg_info(fmt, ...) printk(KERN_INFO fmt, ##__VA_ARGS__)
#else
#define dlg_crit(fmt, ...) 	do { } while (0);
#define dlg_err(fmt, ...)	do { } while (0);
#define dlg_warn(fmt, ...)	do { } while (0);
#define dlg_info(fmt, ...)	do { } while (0);
#endif

typedef u32 (*pmu_platform_callback)(int event, int param);

struct da9066_irq {
	irq_handler_t handler;
	void *data;
};

struct da9066_onkey {
	struct platform_device *pdev;
	struct input_dev *input;
};

struct da9066_regl_init_data {
	int regl_id;
	struct regulator_init_data   *initdata;
};


struct da9066_regl_map {
	u8 reg_id;
	u8 dsm_opmode;
	u8 default_pm_mode;
};

struct da9066_regulator_data {
	int				id;
	struct regulator_init_data	*initdata;
	unsigned long			flags;
	struct device_node *of_node;

	/* For DVC interface, base and max voltage. */
	int				dvc_base_uv;
	int				dvc_max_uv;
};

struct da9066_regulators_pdata {
	int 			id;
	unsigned			n_regulators;
	struct device_node *of_node;
	struct da9066_regulator_data	*regulator_data;
	struct da9066_regulator_info *rinfo;
};


/**
 * Data to be supplied by the platform to initialise the DA9066.
 *
 * @init: Function called during driver initialisation.  Should be
 *        used by the platform to configure GPIO functions and similar.
 * @irq_high: Set if DA9066 IRQ is active high.
 * @irq_base: Base IRQ for genirq (not currently used).
 */

#ifndef __ODIN__
struct temp2adc_map {
	int temp;
	int adc;
};
#endif

struct da9066_platform_data {
	int 	(*init)(struct da9066 *da9066);
	int 	(*irq_init)(struct da9066 *da9066);
	int		num_gpio;	/* GPIO number */
	int		irq_mode;	/* Clear interrupt by read/write(0/1) */
	int		irq_base;	/* IRQ base number of DA9066 */
	struct da9066_regl_init_data	*regulator_data;
};

struct da9066 {
	struct device *dev;
	struct mutex i2c_mutex;
	unsigned short  model;

	/* Control interface */
	struct mutex    io_mutex;
	struct regmap *regmap;

	int (*read_dev)(struct da9066 *const da9066, char reg, int size, void *dest);
	int (*write_dev)(struct da9066 *const da9066, char reg, int size, u8 *src);

	u8 *reg_cache;

	/* Interrupt handling */
    struct work_struct irq_work;
    struct task_struct *irq_task;
	struct mutex irq_mutex; /* IRQ table mutex */
	struct da9066_irq irq[DA9066_NUM_IRQ];
	int chip_irq;

	struct regmap_irq_chip_data *regmap_irq;

	struct da9066_pmic pmic;
	struct da9066_rtc rtc;
	struct da9066_onkey onkey;

	struct da9066_platform_data *pdata;
	struct mutex da9066_io_mutex;
};

/*
 * da9066 Core device initialisation and exit.
 */
int 	da9066_device_init(struct da9066 *da9066, int irq,
				struct da9066_platform_data *pdata);
void 	da9066_device_exit(struct da9066 *da9066);

/*
 * da9066 device IO
 */
int da9066_clear_bits(struct da9066 * const da9066,
			u8 const reg, u8 const mask);
int da9066_set_bits(struct da9066* const da9066, u8 const reg, u8 const mask);
int da9066_reg_read(struct da9066 * const da9066, u8 const reg, u8 *dest);
int da9066_reg_write(struct da9066 * const da9066, u8 const reg, u8 const val);
int da9066_block_read(struct da9066 * const da9066, u8 const start_reg,
			u8 const regs, u8 * const dest);
int da9066_block_write(struct da9066 * const da9066, u8 const start_reg,
			u8 const regs, u8 * const src);
int da9066_reg_update(struct da9066 * const da9066, u8 reg,
			u8 mask, u8 const val);

/*
 * da9066 internal interrupts
 */
int da9066_register_irq(struct da9066 *da9066, int irq, irq_handler_t handler,
			            unsigned long flags, const char *name, void *data);
int da9066_free_irq(struct da9066 *da9066, int irq);
int da9066_mask_irq(struct da9066 *da9066, int irq);
int da9066_unmask_irq(struct da9066 *da9066, int irq);
int da9066_irq_init(struct da9066 *da9066, int irq,
			struct da9066_platform_data *pdata);
int da9066_irq_exit(struct da9066 *da9066);

/* DLG new prototype */
void 	da9066_system_poweroff(void);
void 	da9066_set_mctl_enable(void);
extern struct da9066_platform_data da9066_pdata;

static inline unsigned da9066_model(struct da9066 *da9066)
{
	return da9066->model;
}

#endif /* __DA9066_CORE_H_ */
