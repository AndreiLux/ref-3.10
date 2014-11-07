/*
 * Copyright 2010 Calxeda, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PL320_MAILBOX_H
#define _PL320_MAILBOX_H

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/types.h>

#define MAX_MAILBOX_DATA	7

#define	PWR_ONOFF_SHIFT		20
#define	PG_SHIFT			24
#define	CPU_SHIFT			28
#define	SMS_SHIFT			31

/* Mialbox Command Type */
#define PM_CMD_TYPE			0
#define SENSOR_CMD_TYPE		1
#define I2C_CMD_TYPE		2
#define GPIO_CMD_TYPE		3

/* Category 1: SMS */
#define	SMS_SENSOR			0
#define	SMS_POWER			1

/* Category 2: CPU */
#define	CPU_CLK_GATE		1
#define	CPU_PWR_GATE		2
#define	CPU_CLOCK			3
#define	CPU_PMIC			4
#define	CPU_SUSPEND			7

/* Category 3: */
#define ODIN_CPU_IDLE		3

/* Category 2: SYSTEM */
#define	PWR_ON				1
#define	PWR_OFF				0
#define SYSTEM_POWEROFF		30

/* mailbox debug count */
#define MB_COUNT_MASK		0xFFFFF
#define MB_COUNT_OFFSET		8
#define MB_RETRUN_MASK		0xFF

/* definition of each mailbox sub irq */
#define MB_THERMAL_HANDLER	0
#define MB_DVS_HANDLER		1
#define MB_REGMAP_HANDLER	3
#define MB_SD_VOLTAGE_SWITCH_HANDLER	4
#define MB_COMPLETION_HANDLER			6
#define MB_SMS_GPIO_HANDLER_BASE	8	/* + 48 pin, 8~55 */
#define MB_THREADED_HANDLER_BASE	56
#define MB_THREADED_TOUCH_HANDLER	56

#define MB_ONKEY_HANDLER			56
#define MB_ADC_HANDLER       		57
/* total number of mailbox sub irq */
#define NUM_MB_SUB_IRQ		64

#define NR_MASK_REGS		(NUM_MB_SUB_IRQ-1)/32+1


#define SUB_IRQ_NUM_MASK		(0x0000FFFF)
#define SUB_COMPLETION_MASK		(0xFFFF0000)
#define SUB_COMPLETION_POS		16
#define FAST_IPC_CALL			0
#define SLOW_IPC_CALL			1
#define IN_ATOMIC_IPC_CALL		2
#define IN_ATOMIC_IPC_CALL_OPS	4

#define IN_ATOMIC_MAILBOX_BASE				(0x20060000)
#define IN_ATOMIC_MAILBOX_COMPLETION_OFFSET	(0)
#define IN_ATOMIC_MAILBOX_DATA_OFFSET		(16)

enum sub_completion_num {
	MB_COMPLETION_THERMAL = 0,
	MB_COMPLETION_PMIC,
	MB_COMPLETION_REGISTER,
	MB_COMPLETION_REGMAP,
	MB_COMPLETION_SD_VOLTAGE_SWITCH,
	MB_COMPLETION_GPIO,
	MB_COMPLETION_LPA,
	MB_COMPLETION_DSS,
	MB_COMPLETION_PD_VSP,
	MB_COMPLETION_PD_CSS,
	MB_COMPLETION_PD_DSS,
	MB_COMPLETION_PD_AUD,
	MB_COMPLETION_PD_ICS,
	MB_COMPLETION_PD_GPU,
	MB_COMPLETION_PD_CPU,
	MB_COMPLETION_PD_PERI,
	MB_COMPLETION_PD_SES,
	MB_COMPLETION_CLK,
	MB_COMPLETION_SD_PMIC,
	MB_COMPLETION_SD_REGMAP,
	MB_COMPLETION_EFUSE,
	MB_COMPLETION_MAX,
};

#define MB_COMPLETION_PD_BASE		MB_COMPLETION_PD_VSP

/* REGMAP IPC */
#define DATA_BASE			2

#define REGMAP_IPC		0x1
#define I2C_CMD			0x2
#define WRITE_CMD		0x3
#define READ_CMD		0x4
#define BULK_WRITE_CMD	0x6

struct pl320_mbox {
	struct device *dev;
	struct mutex irq_lock;
	struct irq_domain *domain;

	u32 irq_stat;
	u32 sub_irq_num;
	int irq_base;
	u32 data[NUM_MB_SUB_IRQ][7];
	u32 pl320_mbox_irq_mask_regs[NR_MASK_REGS];
};


/* GPIO command */
#define MB_NORMAL_CMD			(0x0)
#define MB_REG_WRITE			(0x1)
#define MB_REG_READ				(0x2)
#define MB_I2C_WRITE			(0x3)
#define MB_I2C_READ				(0x4)
#define MB_GPIO_SET				(0x5)
#define MB_GPIO_READ			(0x6)

#define GPIO_ID_MASK			(0x0000ff)
#define GPIO_CMD_MASK			(0x00ff00)
#define GPIO_VALUE_MASK			(0xff0000)

#define GPIO_ID_OFFSET			(0x0)
#define GPIO_CMD_OFFSET			(0x8)
#define GPIO_VALUE_OFFSET		(0x10)


#define GPIO_CMD_SET_DIRECTION		(0x0)
#define GPIO_CMD_SET_INT_PRIORITY	(0x1)
#define GPIO_CMD_SET_OUTPUT			(0x2)
#define GPIO_CMD_SET_IRQ			(0x3)
#define GPIO_CMD_SET_MASK			(0x4)
#define GPIO_CMD_SET_PEND			(0x5)
#define GPIO_CMD_SET_DEBOUNCE			(0x6)

#define GPIO_CMD_GET				(0x6)

extern int gic_disabled;
extern u32 saved_irqnr;
extern int ipc_irq;
#ifdef CONFIG_ODIN_PL320_SW_WORKAROUND_MEM_DFS_BUS_LOCK
u32 ipc_call_fast_mem(u32 *data);
#endif
int ipc_call_fast(u32 *data);
int arch_ipc_call_fast(u32 *data);

int mailbox_call(int ipc_type, int completion_sub_num, unsigned int *mbox_data,
		int timeout_ms);

extern int mailbox_request_irq(unsigned int sub_irq, irq_handler_t thread_fn,
		const char *name);
extern int pl320_gpio_irq_base(int subirq_base, int irq_base_offset);

#endif /* _PL320_MAILBOX_H */
