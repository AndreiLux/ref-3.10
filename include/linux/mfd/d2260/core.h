/*
 *  core definitions for Dialog D2260 PMIC
 *
 *  Copyright (C) 2013 Dialog Semiconductor Ltd.
 *  Author: Tony Olech <anthony.olech@diasemi.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 */

#ifndef __D2260_CORE_H_
#define __D2260_CORE_H_

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/mfd/d2260/pmic.h>
#include <linux/mfd/d2260/rtc.h>
#include <linux/power_supply.h>
#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif /*CONFIG_HAS_EARLYSUSPEND*/
#ifdef CONFIG_USB_ODIN_DRD
#include <linux/usb/odin_usb3.h>
#endif

#define MCTL_ENABLE_IN_SMS 1


enum d2260_chip_id {
	D2260,
	D2200_AA,
	D2260_AA,
};

enum d2260_irq {
	/* EVENT_A register IRQ */
	D2260_IRQ_EVF = 0,
	D2260_IRQ_ETBAT2,
	D2260_IRQ_EVDD_LOW,
	D2260_IRQ_EVDD_MON,
	D2260_IRQ_EALARM,
	D2260_IRQ_ESEQRDY,
	D2260_IRQ_ETICK,

	/* EVENT_B register IRQ */
	D2260_IRQ_ENONKEY_LO,
	D2260_IRQ_ENONKEY_HI,
	D2260_IRQ_ENONKEY_HOLDON,
	D2260_IRQ_ENONKEY_HOLDOFF,
	D2260_IRQ_ETBAT1,
	D2260_IRQ_EADCEOM,

	/* EVENT_C register IRQ */
	D2260_IRQ_GPI0,
	D2260_IRQ_GPI1,
	D2260_IRQ_ETA,
	D2260_IRQ_ENJIGON,
	D2260_IRQ_EACCDET,
	D2260_IRQ_EJACKDET,

	D2260_NUM_IRQ
};
/*
               
                               
                                     
 */
#define DIALOG_DEBUG_EN	0

#define DIALOG_DEBUG(d,f, ...) dev_dbg(d,f,##__VA_ARGS__)
#define DIALOG_INFO(d,f, ...) dev_info(d,f,##__VA_ARGS__)

typedef struct {
	unsigned long reg;
	unsigned short val;
} pmu_reg;



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


/**
 * Data to be supplied by the platform to initialise the D2260.
 *
 * @init: Function called during driver initialisation.  Should be
 *        used by the platform to configure GPIO functions and similar.
 * @irq_high: Set if D2260 IRQ is active high.
 * @irq_base: Base IRQ for genirq (not currently used).
 */

struct d2260_platform_data {
	int  (*init)(struct d2260 *d2260);
};

struct d2260 {
	struct device *dev;
	int mctl_status; /* 0 : mctl disable, 1 : mctl enable */
	struct regmap_irq_chip_data *irq_data;
	int chip_irq;
	u8 chip_id;
	struct regmap *regmap;
	struct d2260_platform_data *pdata;
	struct mutex lock;
};

/*
 * d2260 device IO
 */
int d2260_clear_bits(struct d2260 *const d2260, u16 const reg, u8 const mask);
int d2260_set_bits(struct d2260 *const d2260, u16 const reg, u8 const mask);
int d2260_update_bits(struct d2260 *const d2260, u16 const reg,
			u8 const mask, u8 const bits);
int d2260_reg_read(struct d2260 *const d2260, u16 const reg, u8 *dest);
int d2260_reg_write(struct d2260 *const d2260, u16 const reg, u8 const val);
int d2260_group_read(struct d2260 *const d2260, u16 const start_reg,
			u8 const regs, unsigned int *const dest);
int d2260_group_write(struct d2260 *const d2260, u16 const start_reg,
			u8 const regs, unsigned int *const src);

/*
 * d2260 internal interrupts
 */
int d2260_request_irq(struct d2260 *d2260, int irq, char *name,
                           irq_handler_t handler, void *data);
void d2260_free_irq(struct d2260 *d2260, int irq, void *data);

int d2260_irq_init(struct d2260 *d2260);
int d2260_irq_exit(struct d2260 *d2260);

int d2260_single_set(void);

#ifdef CONFIG_USB_ODIN_DRD
void d2260_drd_id_init_detect(struct odin_usb3_otg *odin_otg);
#endif

#endif /* __D2260_CORE_H_ */
