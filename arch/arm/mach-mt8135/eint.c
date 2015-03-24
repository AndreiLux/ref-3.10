#ifndef CONFIG_MT8135_FPGA
/* #define MD_EINT */
#endif
#define LDVT
#define EINT_TEST

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/irq.h>
#include <linux/irqchip/chained_irq.h>
#include <asm/mach/irq.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include <linux/sched.h>	/* need by sche_clock() */
#include <linux/syscore_ops.h>
#include <mach/mt_pmic_wrap.h>
#include <mach/mt_gpio_def.h>
#include <mach/mt_spm_sleep.h>
#include <mach/mt_reg_base.h>
#include <mach/eint.h>
#include <mach/irqs.h>
#include <mach/sync_write.h>
#include <mach/eint_drv.h>

#define EINT_DEBUG  0
#if (EINT_DEBUG == 1)
#define dbgmsg printk
#else
#define dbgmsg(...)
#endif

/* Check if NR_IRQS is enough */
#if (EINT_IRQ_BASE + EINT_MAX_CHANNEL) > (NR_IRQS)
#error NR_IRQS too small.
#endif

/*
 * Define internal data structures.
 */
typedef enum {
	SIM_HOT_PLUG_EINT_NUMBER,
	SIM_HOT_PLUG_EINT_DEBOUNCETIME,
	SIM_HOT_PLUG_EINT_POLARITY,
	SIM_HOT_PLUG_EINT_SENSITIVITY,
	SIM_HOT_PLUG_EINT_SOCKETTYPE,
} sim_hot_plug_eint_queryType;

typedef enum {
	ERR_SIM_HOT_PLUG_NULL_POINTER = -13,
	ERR_SIM_HOT_PLUG_QUERY_TYPE,
	ERR_SIM_HOT_PLUG_QUERY_STRING,
} sim_hot_plug_eint_queryErr;


typedef struct {
	void (*eint_func[EINT_MAX_CHANNEL]) (void);
	unsigned int eint_auto_umask[EINT_MAX_CHANNEL];
	/*is_deb_en: 1 means enable, 0 means disable */
	unsigned int is_deb_en[EINT_MAX_CHANNEL];
	unsigned int deb_time[EINT_MAX_CHANNEL];
#if defined(EINT_TEST)
	unsigned int softisr_called[EINT_MAX_CHANNEL];
#endif
	struct timer_list eint_sw_deb_timer[EINT_MAX_CHANNEL];
} eint_func;

unsigned long wake_mask[7];
unsigned long cur_mask[7];

struct mt_eint_chip mt8135_eint_chip = {
	.name = "mt8135_eint",
	.base_addr = EINT_BASE,
	.base_eint = 0,
	.base_irq  = EINT_IRQ_BASE,
	.nirqs     = EINT_AP_MAXNUMBER,
	.stat      = 0x000,
	.ack       = 0x040,
	.mask      = 0x080,
	.mask_set  = 0x0C0,
	.mask_clr  = 0x100,
	.sens      = 0x140,
	.sens_set  = 0x180,
	.sens_clr  = 0x1C0,
	.pol       = 0x300,
	.pol_set   = 0x340,
	.pol_clr   = 0x380,
	.port_mask = 7,
	.ports     = 6,
	.wake_mask = wake_mask,
	.cur_mask = cur_mask,
};

struct mt_eint_chip mt6397_eint_chip = {
	.name = "mt6397_eint",
	.base_addr = (EINT_BASE + 0xA00),
	.base_irq  = EINT_IRQ_BASE + EINT_AP_MAXNUMBER,
	.base_eint = EINT_AP_MAXNUMBER,
	.nirqs     = EINT_MAX_CHANNEL - EINT_AP_MAXNUMBER,
	.stat      = 0x000,
	.ack       = 0x008,
	.mask      = 0x018,
	.mask_set  = 0x020,
	.mask_clr  = 0x028,
	.sens      = 0x030,
	.sens_set  = 0x038,
	.sens_clr  = 0x040,
	.pol       = 0x048,
	.pol_set   = 0x050,
	.pol_clr   = 0x058,
	.port_mask = 1,
	.ports     = 1,
	.wake_mask = wake_mask + 6,
	.cur_mask = cur_mask + 6,
};

struct eint_domain {
	unsigned int __iomem *reg;
	u16 offset;
	u16 size;
};

struct eint_domain_group {
	struct eint_domain mt8135;
	struct eint_domain mt6397;
};

#define EINT_DOMAIN_NUM 3
static const struct eint_domain_group eint_domains[EINT_DOMAIN_NUM] = {
	{
		.mt8135 = {
			.reg = (unsigned int __iomem *)EINT_D0_EN_BASE,
			.offset = 0,
			.size = 192,
		},
		.mt6397 = {
			.reg = (unsigned int __iomem *)PMIC_EINT_D0_EN_BASE,
			.offset = 192,
			.size = 25,
		},
	},
	{
		.mt8135 = {
			.reg = (unsigned int __iomem *)EINT_D1_EN_BASE,
			.offset = 0,
			.size = 192,
		},
		.mt6397 = {
			.reg = (unsigned int __iomem *)PMIC_EINT_D1_EN_BASE,
			.offset = 192,
			.size = 25,
		},
	},
	{
		.mt8135 = {
			.reg = (unsigned int __iomem *)EINT_D2_EN_BASE,
			.offset = 0,
			.size = 192,
		},
		.mt6397 = {
			.reg = (unsigned int __iomem *)PMIC_EINT_D2_EN_BASE,
			.offset = 192,
			.size = 25,
		},
	},
};

static struct eint_domain_config default_eint_conf[EINT_DOMAIN_NUM] = {
	{ .start = 0, .size = 96 },
	{ .start = 96, .size = 96 },
	{ .start = 192, .size = 25 },
};

static struct eint_domain_config *eint_conf;

/*
 * Define global variables.
 */

#ifdef LDVT
static const unsigned int EINT_IRQ = MT_EINT_IRQ_ID;
static eint_func EINT_FUNC;
#else
static eint_func EINT_FUNC;
#endif


/*
 * mt_eint_get_mask: To get the eint mask
 * @eint_num: the EINT number to get
 */
static unsigned int mt_eint_get_mask(unsigned int eint_num)
{

	unsigned int base;
	unsigned int st;
	unsigned int bit = 1 << (eint_num % 32);

	if (eint_num < EINT_AP_MAXNUMBER) {
		base = (eint_num / 32) * 4 + EINT_MASK_BASE;
	} else {
		base = PMIC_EINT_MASK_BASE;
	}
	st = readl((volatile unsigned int *)base);

	if (st & bit)
		st = 1;		/* masked */
	else
		st = 0;		/* unmasked */

	return st;
}

#if 0
/*
 *  * mt_eint_mask_all: Mask all the specified EINT number.
 *   *
 *    */
static void mt_eint_mask_all(void)
{
	unsigned int base;
	unsigned int val = 0xFFFFFFFF, ap_cnt = (EINT_MAX_CHANNEL / 32), i;

	if (EINT_MAX_CHANNEL % 32)
		ap_cnt++;
	dbgmsg("[EINT] cnt:%d\n", ap_cnt);

	base = EINT_MASK_SET_BASE;
	for (i = 0; i < ap_cnt; i++) {
		writel_relaxed(val, (volatile unsigned int *)(base + (i * 4)));
		dbgmsg("[EINT] mask addr:%x = %x\n", EINT_MASK_BASE + (i * 4),
		       readl(EINT_MASK_BASE + (i * 4)));
	}

	val = 0x01FFFFFF;
	base = PMIC_EINT_MASK_SET_BASE;
	mt65xx_reg_sync_writel(val, base);
}

/*
 *  * mt_eint_unmask_all: Mask the specified EINT number.
 *   *
 *    */
static void mt_eint_unmask_all(void)
{
	unsigned int base;
	unsigned int val = 0xFFFFFFFF, ap_cnt = (EINT_MAX_CHANNEL / 32), i;

	if (EINT_MAX_CHANNEL % 32)
		ap_cnt++;
	dbgmsg("[EINT] cnt:%d\n", ap_cnt);

	base = EINT_MASK_CLR_BASE;
	for (i = 0; i < ap_cnt; i++) {
		writel_relaxed(val, base + (i * 4));
		dbgmsg("[EINT] unmask addr:%x = %x\n", EINT_MASK_BASE + (i * 4),
		       readl(EINT_MASK_BASE + (i * 4)));
	}
	val = 0x01FFFFFF;
	base = PMIC_EINT_MASK_CLR_BASE;
	mt65xx_reg_sync_writel(val, base);
}

/*
 * mt_eint_get_soft: To get the eint mask
 * @eint_num: the EINT number to get
 */
static unsigned int mt_eint_get_soft(unsigned int eint_num)
{
	unsigned int base;
	unsigned int st;

	if (eint_num < EINT_AP_MAXNUMBER) {
		base = (eint_num / 32) * 4 + EINT_SOFT_BASE;
	} else {
		base = PMIC_EINT_SOFT_BASE;
	}
	st = readl(base);

	return st;
}

/*
 * eint_send_pulse: Trigger the specified EINT number.
 * @eint_num: EINT number to send
 */
static inline void mt_eint_send_pulse(unsigned int eint_num)
{
	unsigned int base_set = (eint_num / 32) * 4 + EINT_SOFT_SET_BASE;
	unsigned int base_clr = (eint_num / 32) * 4 + EINT_SOFT_CLR_BASE;
	unsigned int bit = 1 << (eint_num % 32);
	if (eint_num < EINT_AP_MAXNUMBER) {
		base_set = (eint_num / 32) * 4 + EINT_SOFT_SET_BASE;
		base_clr = (eint_num / 32) * 4 + EINT_SOFT_CLR_BASE;
	} else {
		base_set = PMIC_EINT_SOFT_SET_BASE;
		base_clr = PMIC_EINT_SOFT_CLR_BASE;
	}

	mt65xx_reg_sync_writel(bit, base_set);
	mt65xx_reg_sync_writel(bit, base_clr);
}
#endif
#if defined(EINT_TEST)

#if   defined(CONFIG_MT6585_FPGA) || defined(CONFIG_MT8135_FPGA)
/*
 * mt_eint_emu_set: Trigger the specified EINT number.
 * @eint_num: EINT number to set
 */
static void mt_eint_emu_set(unsigned int eint_num)
{
	unsigned int base = 0;
	unsigned int bit = 1 << (eint_num % 32);
	unsigned int value = 0;

	if (eint_num < EINT_AP_MAXNUMBER) {
		base = (eint_num / 32) * 4 + EINT_EMUL_BASE;
	} else {
		base = PMIC_EINT_EMUL_BASE;
	}
	value = readl((volatile unsigned int *)base);
	value = bit | value;
	mt65xx_reg_sync_writel(value, (volatile unsigned int *)base);

	dbgmsg("[EINT] emul set addr:%x = %x\n", base, bit);


}

/*
 * mt_eint_emu_clr: Trigger the specified EINT number.
 * @eint_num: EINT number to clr
 */
static void mt_eint_emu_clr(unsigned int eint_num)
{
	unsigned int base = 0;
	unsigned int bit = 1 << (eint_num % 32);
	unsigned int value = 0;

	if (eint_num < EINT_AP_MAXNUMBER) {
		base = (eint_num / 32) * 4 + EINT_EMUL_BASE;
	} else {
		base = PMIC_EINT_EMUL_BASE;
	}
	value = readl((volatile unsigned int *)base);
	value = (~bit) & value;
	mt65xx_reg_sync_writel(value, (volatile unsigned int *)base);

	dbgmsg("[EINT] emul clr addr:%x = %x\n", base, bit);

}

#endif
/*
 * mt_eint_soft_set: Trigger the specified EINT number.
 * @eint_num: EINT number to set
 */
void mt_eint_soft_set(unsigned int eint_num)
{

	unsigned int base;
	unsigned int bit = 1 << (eint_num % 32);

	if (eint_num < EINT_AP_MAXNUMBER) {
		base = (eint_num / 32) * 4 + EINT_SOFT_SET_BASE;
	} else {
		mt65xx_reg_sync_writel(0xFFFFFFFF, PMIC_EINT_INPUT_MUX_SET_BASE);
		base = PMIC_EINT_SOFT_SET_BASE;
	}
	mt65xx_reg_sync_writel(bit, base);

	dbgmsg("[EINT] soft set addr:%x = %x\n", base, bit);

}
EXPORT_SYMBOL(mt_eint_soft_set);

/*
 * mt_eint_soft_clr: Unmask the specified EINT number.
 * @eint_num: EINT number to clear
 */
static void mt_eint_soft_clr(unsigned int eint_num)
{
	unsigned int base;
	unsigned int bit = 1 << (eint_num % 32);

	if (eint_num < EINT_AP_MAXNUMBER) {
		base = (eint_num / 32) * 4 + EINT_SOFT_CLR_BASE;
	} else {
		base = PMIC_EINT_SOFT_CLR_BASE;
	}
	mt65xx_reg_sync_writel(bit, base);

	dbgmsg("[EINT] soft clr addr:%x = %x\n", base, bit);

}
#endif
/*
 * mt_eint_mask: Mask the specified EINT number.
 * @eint_num: EINT number to mask
 */
void mt_eint_mask(unsigned int eint_num)
{
	unsigned int base;
	unsigned int bit = 1 << (eint_num % 32);

	if (eint_num < EINT_AP_MAXNUMBER) {
		base = (eint_num / 32) * 4 + EINT_MASK_SET_BASE;
	} else {
		base = PMIC_EINT_MASK_SET_BASE;
	}
	mt65xx_reg_sync_writel(bit, base);

	dbgmsg("[EINT] mask addr:%x = %x\n", base, bit);

}
EXPORT_SYMBOL(mt_eint_mask);

/*
 * mt_eint_unmask: Unmask the specified EINT number.
 * @eint_num: EINT number to unmask
 */
void mt_eint_unmask(unsigned int eint_num)
{
	unsigned int base;
	unsigned int bit = 1 << (eint_num % 32);

	if (eint_num < EINT_AP_MAXNUMBER) {
		base = (eint_num / 32) * 4 + EINT_MASK_CLR_BASE;
	} else {
		base = PMIC_EINT_MASK_CLR_BASE;
	}
	mt65xx_reg_sync_writel(bit, base);

	dbgmsg("[EINT] unmask addr:%x = %x\n", base, bit);
}
EXPORT_SYMBOL(mt_eint_unmask);

/*
 * mt_eint_set_polarity: Set the polarity for the EINT number.
 * @eint_num: EINT number to set
 * @pol: polarity to set
 */
void mt_eint_set_polarity(unsigned int eint_num, unsigned int pol)
{
	unsigned int count;
	unsigned int base;
	unsigned int bit = 1 << (eint_num % 32);

	if (pol == MT_EINT_POL_NEG) {
		if (eint_num < EINT_AP_MAXNUMBER) {
			base = (eint_num / 32) * 4 + EINT_POL_CLR_BASE;
		} else {
			base = PMIC_EINT_POL_CLR_BASE;
		}
	} else {
		if (eint_num < EINT_AP_MAXNUMBER) {
			base = (eint_num / 32) * 4 + EINT_POL_SET_BASE;
		} else {
			base = PMIC_EINT_POL_SET_BASE;
		}
	}
	mt65xx_reg_sync_writel(bit, base);

	for (count = 0; count < 250; count++)
		;

	if (eint_num < EINT_AP_MAXNUMBER) {
		base = (eint_num / 32) * 4 + EINT_INTACK_BASE;
	} else {
		base = PMIC_EINT_INTACK_BASE;
	}
	mt65xx_reg_sync_writel(bit, base);
	dbgmsg("[EINT] %s :%x, bit: %x\n", __func__, base, bit);

}
EXPORT_SYMBOL(mt_eint_set_polarity);

/*
 * mt_eint_get_polarity: Set the polarity for the EINT number.
 * @eint_num: EINT number to get
 * Return: polarity type
 */
unsigned int mt_eint_get_polarity(unsigned int eint_num)
{
	unsigned int val;
	unsigned int base;
	unsigned int bit = 1 << (eint_num % 32);
	unsigned int pol;

	if (eint_num < EINT_AP_MAXNUMBER) {
		base = (eint_num / 32) * 4 + EINT_POL_BASE;
	} else {
		base = PMIC_EINT_POL_BASE;
	}
	val = readl((volatile unsigned int *)base);

	dbgmsg("[EINT] %s :%x, bit:%x, val:%x\n", __func__, base, bit, val);
	if (val & bit) {
		pol = MT_EINT_POL_POS;
	} else {
		pol = MT_EINT_POL_NEG;
	}
	return pol;

}
EXPORT_SYMBOL(mt_eint_get_polarity);

/*
 * mt_eint_set_sens: Set the sensitivity for the EINT number.
 * @eint_num: EINT number to set
 * @sens: sensitivity to set
 * Always return 0.
 */
unsigned int mt_eint_set_sens(unsigned int eint_num, unsigned int sens)
{
	unsigned int base;
	unsigned int bit = 1 << (eint_num % 32);

	if (sens == MT_EDGE_SENSITIVE) {
		if (eint_num < EINT_AP_MAXNUMBER) {
			base = (eint_num / 32) * 4 + EINT_SENS_CLR_BASE;
		} else {
			base = PMIC_EINT_SENS_CLR_BASE;
		}
	} else if (sens == MT_LEVEL_SENSITIVE) {
		if (eint_num < EINT_AP_MAXNUMBER) {
			base = (eint_num / 32) * 4 + EINT_SENS_SET_BASE;
		} else {
			base = PMIC_EINT_SENS_SET_BASE;
		}
	} else {
		pr_crit("%s invalid sensitivity value\n", __func__);
		return 0;
	}
	mt65xx_reg_sync_writel(bit, base);
	dbgmsg("[EINT] %s :%x, bit: %x\n", __func__, base, bit);
	return 0;
}
EXPORT_SYMBOL(mt_eint_set_sens);

/*
 * mt_eint_get_sens: To get the eint sens
 * @eint_num: the EINT number to get
 */
static unsigned int mt_eint_get_sens(unsigned int eint_num)
{
	unsigned int base, sens;
	unsigned int bit = 1 << (eint_num % 32), st;

	if (eint_num < EINT_AP_MAXNUMBER) {
		base = (eint_num / 32) * 4 + EINT_SENS_BASE;
	} else {
		base = PMIC_EINT_SENS_BASE;
	}
	st = readl((volatile unsigned int *)base);
	if (st & bit) {
		sens = MT_LEVEL_SENSITIVE;
	} else {
		sens = MT_EDGE_SENSITIVE;
	}
	return sens;
}

/*
 * mt_eint_ack: To ack the interrupt
 * @eint_num: the EINT number to set
 */
static unsigned int mt_eint_ack(unsigned int eint_num)
{
	unsigned int base;
	unsigned int bit = 1 << (eint_num % 32);

	if (eint_num < EINT_AP_MAXNUMBER) {
		base = (eint_num / 32) * 4 + EINT_INTACK_BASE;
	} else {
		base = PMIC_EINT_INTACK_BASE;
	}
	mt65xx_reg_sync_writel(bit, base);

	dbgmsg("[EINT] %s :%x, bit: %x\n", __func__, base, bit);
	return 0;
}

/*
 * mt_eint_read_status: To read the interrupt status
 * @eint_num: the EINT number to set
 */
static unsigned int mt_eint_read_status(unsigned int eint_num)
{
	unsigned int base;
	unsigned int st;
	unsigned int bit = 1 << (eint_num % 32);

	if (eint_num < EINT_AP_MAXNUMBER) {
		base = (eint_num / 32) * 4 + EINT_STA_BASE;
	} else {
		base = PMIC_EINT_STA_BASE;
	}
	st = readl((volatile unsigned int *)base);

	return st & bit;
}

/*
 * mt_eint_get_status: To get the interrupt status
 * @eint_num: the EINT number to get
 */
static unsigned int mt_eint_get_status(unsigned int eint_num)
{
	unsigned int base;
	unsigned int st;

	if (eint_num < EINT_AP_MAXNUMBER) {
		base = (eint_num / 32) * 4 + EINT_STA_BASE;
	} else {
		base = PMIC_EINT_STA_BASE;
	}

	st = readl((volatile unsigned int *)base);
	return st;
}

/*
 * mt_eint_en_hw_debounce: To enable hw debounce
 * @eint_num: the EINT number to set
 */
static void mt_eint_en_hw_debounce(unsigned int eint_num)
{
	unsigned int base, bit;
	base = (eint_num / 4) * 4 + EINT_DBNC_SET_BASE;
	bit = (EINT_DBNC_SET_EN << EINT_DBNC_SET_EN_BITS) << ((eint_num % 4) * 8);
	mt65xx_reg_sync_writel(bit, base);
	EINT_FUNC.is_deb_en[eint_num] = 1;
}

/*
 * mt_eint_dis_hw_debounce: To disable hw debounce
 * @eint_num: the EINT number to set
 */
static void mt_eint_dis_hw_debounce(unsigned int eint_num)
{
	unsigned int clr_base, bit;
	clr_base = (eint_num / 4) * 4 + EINT_DBNC_CLR_BASE;
	bit = (EINT_DBNC_CLR_EN << EINT_DBNC_CLR_EN_BITS) << ((eint_num % 4) * 8);
	mt65xx_reg_sync_writel(bit, clr_base);
	EINT_FUNC.is_deb_en[eint_num] = 0;
}

/*
 * mt_eint_dis_sw_debounce: To set EINT_FUNC.is_deb_en[eint_num] disable
 * @eint_num: the EINT number to set
 */
static void mt_eint_dis_sw_debounce(unsigned int eint_num)
{
	EINT_FUNC.is_deb_en[eint_num] = 0;
}

/*
 * mt_eint_en_sw_debounce: To set EINT_FUNC.is_deb_en[eint_num] enable
 * @eint_num: the EINT number to set
 */
static void mt_eint_en_sw_debounce(unsigned int eint_num)
{
	EINT_FUNC.is_deb_en[eint_num] = 1;
}

/*
 * mt_can_en_debounce: Check the EINT number is able to enable debounce or not
 * @eint_num: the EINT number to set
 */
static unsigned int mt_can_en_debounce(unsigned int eint_num)
{
	unsigned int sens = mt_eint_get_sens(eint_num);
	/* debounce: debounce time is not 0 && it is not edge sensitive */
	if (EINT_FUNC.deb_time[eint_num] > 0 && sens != MT_EDGE_SENSITIVE)
		return 1;
	else {
		dbgmsg
		    (KERN_CRIT "Can't enable debounce of eint_num:%d, deb_time:%d, sens:%d\n",
		     eint_num, EINT_FUNC.deb_time[eint_num], sens);
		return 0;
	}
}

/*
 * mt_eint_set_hw_debounce: Set the de-bounce time for the specified EINT number.
 * @eint_num: EINT number to acknowledge
 * @ms: the de-bounce time to set (in miliseconds)
 */
void mt_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms)
{
	unsigned int dbnc, base, bit, clr_bit, clr_base, rst, unmask = 0;
	base = (eint_num / 4) * 4 + EINT_DBNC_SET_BASE;
	clr_base = (eint_num / 4) * 4 + EINT_DBNC_CLR_BASE;
	EINT_FUNC.deb_time[eint_num] = ms;

	/*
	 * Don't enable debounce once debounce time is 0 or
	 * its type is edge sensitive.
	 */
	if (!mt_can_en_debounce(eint_num)) {
		dbgmsg(KERN_CRIT "Can't enable debounce of eint_num:%d in %s\n", eint_num,
		       __func__);
		return;
	}

	if (ms == 0) {
		dbnc = 0;
		dbgmsg(KERN_CRIT "ms should not be 0. eint_num:%d in %s\n", eint_num, __func__);
	} else if (ms <= 1) {
		dbnc = 1;
	} else if (ms <= 16) {
		dbnc = 2;
	} else if (ms <= 32) {
		dbnc = 3;
	} else if (ms <= 64) {
		dbnc = 4;
	} else if (ms <= 128) {
		dbnc = 5;
	} else if (ms <= 256) {
		dbnc = 6;
	} else {
		dbnc = 7;
	}

	/* setp 1: mask the EINT */
	if (!mt_eint_get_mask(eint_num)) {
		mt_eint_mask(eint_num);
		unmask = 1;
	}

	/* step 2: Check hw debouce number to decide which type should be used */
	if (eint_num >= MAX_HW_DEBOUNCE_CNT)
		mt_eint_en_sw_debounce(eint_num);
	else {
		/* step 2.1: set hw debounce flag */
		EINT_FUNC.is_deb_en[eint_num] = 1;

		/* step 2.2: disable hw debounce */
		clr_bit = 0xFF << ((eint_num % 4) * 8);
		mt65xx_reg_sync_writel(clr_bit, clr_base);

		/* step 2.3: set new debounce value */
		bit =
		    ((dbnc << EINT_DBNC_SET_DBNC_BITS) |
		     (EINT_DBNC_SET_EN << EINT_DBNC_SET_EN_BITS)) << ((eint_num % 4) * 8);
		mt65xx_reg_sync_writel(bit, base);

		/* step 2.4: Delay a while (more than 2T) to wait for hw debounce enable work correctly */
		udelay(500);

		/* step 2.5: Reset hw debounce counter to avoid unexpected interrupt */
		rst = (EINT_DBNC_RST_BIT << EINT_DBNC_SET_RST_BITS) << ((eint_num % 4) * 8);
		mt65xx_reg_sync_writel(rst, base);

		/* step 2.6: Delay a while (more than 2T) to wait for hw debounce counter reset work correctly */
		udelay(500);
	}
	/* step 3: unmask the EINT */
	if (unmask == 1)
		mt_eint_unmask(eint_num);
}
EXPORT_SYMBOL(mt_eint_set_hw_debounce);

/*
 * mt_eint_timer_event_handler: EINT sw debounce handler
 * @eint_num: the EINT number and use unsigned long to prevent
 *            compile warning of timer usage.
 */
static void mt_eint_timer_event_handler(unsigned long eint_num)
{
	unsigned int status;
	unsigned long flags;

	/* disable interrupt for core 0 and it will run on core 0 only */
	local_irq_save(flags);
	mt_eint_unmask(eint_num);
	status = mt_eint_read_status(eint_num);
	dbgmsg("EINT Module - EINT_STA = 0x%x, in %s\n", status, __func__);
	if (status) {
		mt_eint_mask(eint_num);
		if (EINT_FUNC.eint_func[eint_num])
			EINT_FUNC.eint_func[eint_num] ();
		mt_eint_ack(eint_num);
	}
	local_irq_restore(flags);
	if (EINT_FUNC.eint_auto_umask[eint_num])
		mt_eint_unmask(eint_num);
}

/*
 * mt_eint_set_timer_event: To set a timer event for sw debounce.
 * @eint_num: the EINT number to set
 */
static void mt_eint_set_timer_event(unsigned int eint_num)
{
	struct timer_list *eint_timer = &EINT_FUNC.eint_sw_deb_timer[eint_num];
	/* assign this handler to execute on core 0 */
	int cpu = 0;

	/* register timer for this sw debounce eint */
	eint_timer->expires = jiffies + msecs_to_jiffies(EINT_FUNC.deb_time[eint_num]);
	dbgmsg("EINT Module - expires:%lu, jiffies:%lu, deb_in_jiffies:%lu, ", eint_timer->expires,
	       jiffies, msecs_to_jiffies(EINT_FUNC.deb_time[eint_num]));
	dbgmsg("deb:%d, in %s\n", EINT_FUNC.deb_time[eint_num], __func__);
	eint_timer->data = eint_num;
	eint_timer->function = &mt_eint_timer_event_handler;
	init_timer(eint_timer);
	add_timer_on(eint_timer, cpu);
}

static void mt_eint_demux(unsigned irq, struct irq_desc *desc)
{
	unsigned int rst, base, domain;
	unsigned int status, start, end;
	unsigned int reg_base;
	unsigned long long t1, t2;
	int last_index = -1;
	struct irq_chip *chip = irq_get_chip(irq);
	chained_irq_enter(chip, desc);

	domain = irq - EINT_IRQ;
	BUG_ON(domain >= EINT_DOMAIN_NUM);

	start = eint_conf[domain].start;
	end = eint_conf[domain].start + eint_conf[domain].size;

	/*
	 * NoteXXX: Need to get the wake up for 0.5 seconds when an EINT intr tirggers.
	 *          This is used to prevent system from suspend such that other drivers
	 *          or applications can have enough time to obtain their own wake lock.
	 *          (This information is gotten from the power management owner.)
	 */

	for (reg_base = start; reg_base < end; reg_base += 32) {
		/* read status register every 32 interrupts */
		status = mt_eint_get_status(reg_base);

		while (status) {
			int offset = __ffs(status);
			int index = reg_base + offset;
			struct mt_eint_def *eint = mt_get_eint_def(index);

			last_index = index;
			status &= ~(1 << offset);
			if (!eint)
				break;

			if (!EINT_FUNC.eint_func[index]) {
				generic_handle_irq(index + EINT_IRQ_BASE);
				continue;
			}

			/* Handle EINT registered by mt_eint_registration */
			mt_eint_mask(index);
			if ((EINT_FUNC.is_deb_en[index] == 1) && (index >= MAX_HW_DEBOUNCE_CNT)) {
				/* if its debounce is enable and it is a sw debounce */
				mt_eint_set_timer_event(index);
			} else {
				/* HW debounce or no use debounce */
				t1 = sched_clock();
				if (EINT_FUNC.eint_func[index]) {
					EINT_FUNC.eint_func[index] ();
				}
				t2 = sched_clock();
				mt_eint_ack(index);

				/* Don't need to use reset ? */
				/* reset debounce counter */
				base = (index / 4) * 4 + EINT_DBNC_SET_BASE;
				rst = (EINT_DBNC_RST_BIT << EINT_DBNC_SET_RST_BITS) <<
				    ((index % 4) * 8);
				mt65xx_reg_sync_writel(rst, base);

#if (EINT_DEBUG == 1)
				status = mt_eint_get_status(index);
				dbgmsg("EINT Module - EINT_STA after ack = 0x%x\n", status);
#endif
				if (EINT_FUNC.eint_auto_umask[index]) {
					mt_eint_unmask(index);
				}

				if ((t2 - t1) > 1000000)
					pr_info
					    ("[EINT]Warn!EINT:%d run too long,s:%llu,e:%llu,total:%llu\n",
					     index, t1, t2, (t2 - t1));
			}
		}
	}

	/* If we had processed any PMIC EINT, clear EEVT so it won't keep
	   system from sleeping.
	   If we didn't process any event, it is possible that level event
	   disasserted before we process it. We need to clear EEVT to prevent
	   unintended wakeup. */
	if (last_index >= EINT_AP_MAXNUMBER || last_index == -1)
		mt65xx_reg_sync_writel(0x1, PMIC_EINT_EEVT_CLR_BASE);
	chained_irq_exit(chip, desc);
}


static int mt_eint_max_channel(void)
{
	return EINT_MAX_CHANNEL;
}

static unsigned int mt_eint_get_debounce_cnt(unsigned int cur_eint_num)
{
	unsigned int dbnc, deb, base;
	base = (cur_eint_num / 4) * 4 + EINT_DBNC_BASE;

	if (cur_eint_num >= MAX_HW_DEBOUNCE_CNT)
		deb = EINT_FUNC.deb_time[cur_eint_num];
	else {
		dbnc = readl((volatile unsigned int *)base);
		dbnc = ((dbnc >> EINT_DBNC_SET_DBNC_BITS) >> ((cur_eint_num % 4) * 8) & EINT_DBNC);

		switch (dbnc) {
		case 0:
			deb = 0;	/* 0.5 actually, but we don't allow user to set. */
			dbgmsg(KERN_CRIT "ms should not be 0. eint_num:%d in %s\n",
			       cur_eint_num, __func__);
			break;
		case 1:
			deb = 1;
			break;
		case 2:
			deb = 16;
			break;
		case 3:
			deb = 32;
			break;
		case 4:
			deb = 64;
			break;
		case 5:
			deb = 128;
			break;
		case 6:
			deb = 256;
			break;
		case 7:
			deb = 512;
			break;
		default:
			deb = 0;
			pr_info("invalid deb time in the EIN_CON register, dbnc:%d, deb:%d\n", dbnc,
				deb);
			break;
		}
	}

	return deb;
}

static int mt_eint_is_debounce_en(unsigned int cur_eint_num)
{
	unsigned int base, val, en;
	if (cur_eint_num < MAX_HW_DEBOUNCE_CNT) {
		base = (cur_eint_num / 4) * 4 + EINT_DBNC_BASE;
		val = readl((volatile unsigned int *)base);
		val = val >> ((cur_eint_num % 4) * 8);
		if (val & EINT_DBNC_EN_BIT) {
			en = 1;
		} else {
			en = 0;
		}
	} else {
		en = EINT_FUNC.is_deb_en[cur_eint_num];
	}

	return en;
}

/*
 * mt_eint_registration: register a EINT.
 * @eint_num: the EINT number to register
 * @flag: the interrupt line behaviour to select
 * @EINT_FUNC_PTR: the ISR callback function
 * @is_auto_unmask: the indication flag of auto unmasking after ISR callback is processed
 */
void mt_eint_registration(unsigned int eint_num, unsigned int flag,
			  void (EINT_FUNC_PTR) (void), unsigned int is_auto_umask)
{
	if (eint_num < EINT_MAX_CHANNEL) {
		mt_eint_mask(eint_num);

		if (flag & (EINTF_TRIGGER_RISING | EINTF_TRIGGER_FALLING)) {
			mt_eint_set_polarity(eint_num,
					     (flag & EINTF_TRIGGER_FALLING) ? MT_EINT_POL_NEG :
					     MT_EINT_POL_POS);
			mt_eint_set_sens(eint_num, MT_EDGE_SENSITIVE);
		} else if (flag & (EINTF_TRIGGER_HIGH | EINTF_TRIGGER_LOW)) {
			mt_eint_set_polarity(eint_num,
					     (flag & EINTF_TRIGGER_LOW) ? MT_EINT_POL_NEG :
					     MT_EINT_POL_POS);
			mt_eint_set_sens(eint_num, MT_LEVEL_SENSITIVE);
		} else {
			pr_info("[EINT]: Wrong EINT Pol/Sens Setting 0x%x\n", flag);
			return;
		}

		EINT_FUNC.eint_func[eint_num] = EINT_FUNC_PTR;
		EINT_FUNC.eint_auto_umask[eint_num] = is_auto_umask;
		mt_eint_ack(eint_num);
		mt_eint_unmask(eint_num);
	} else {
		pr_info("[EINT]: Wrong EINT Number %d\n", eint_num);
	}
}
EXPORT_SYMBOL(mt_eint_registration);

#if 0
static void md_eint_test_fun(void)
{
	pr_info("in md_eint_test_fun");
#if 0
	char *result;
	unsigned int *len;
	int i;
	i = get_eint_attribute(" MD2_SIM1_HOT_PLUG_EINT ", strlen(" MD2_SIM1_HOT_PLUG_EINT "),
			       SIM_HOT_PLUG_EINT_NUMBER, &result, &len);
	pr_info("[STEN]MD EINT TEST:%d, %d, %d\n", i, len, (int)result);
	i = get_eint_attribute(" MD2_SIM2_HOT_PLUG_EINT ", strlen(" MD2_SIM2_HOT_PLUG_EINT "),
			       SIM_HOT_PLUG_EINT_POLARITY, &result, &len);
	pr_info("[STEN]MD EINT TEST:%d, %d, %d\n", i, len, (int)result);
#endif
}
#endif

static void mt_eint_enable_debounce(unsigned int cur_eint_num)
{
	mt_eint_mask(cur_eint_num);
	if (cur_eint_num < MAX_HW_DEBOUNCE_CNT) {
		/* HW debounce */
		mt_eint_en_hw_debounce(cur_eint_num);
	} else {
		/* SW debounce */
		mt_eint_en_sw_debounce(cur_eint_num);
	}
	mt_eint_unmask(cur_eint_num);
}

static void mt_eint_disable_debounce(unsigned int cur_eint_num)
{
	mt_eint_mask(cur_eint_num);
	if (cur_eint_num < MAX_HW_DEBOUNCE_CNT) {
		/* HW debounce */
		mt_eint_dis_hw_debounce(cur_eint_num);
	} else {
		/* SW debounce */
		mt_eint_dis_sw_debounce(cur_eint_num);
	}
	mt_eint_unmask(cur_eint_num);
}

#if defined(EINT_TEST)
static unsigned int cur_eint_num;
static ssize_t cur_eint_soft_set_show(struct device_driver *driver, char *buf)
{
	unsigned int ret = EINT_FUNC.softisr_called[cur_eint_num];
	/* reset to 0 for the next testing. */
	EINT_FUNC.softisr_called[cur_eint_num] = 0;
	return snprintf(buf, PAGE_SIZE, "%d\n", ret);
}

/*
 * set 1 to trigger and set 0 to clr this interrupt
 */
static ssize_t cur_eint_soft_set_store(struct device_driver *driver, const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int num;

	num = simple_strtoul(p, &p, 10);
	if (num == 1) {
#if   defined(CONFIG_MT6585_FPGA) || defined(CONFIG_MT8135_FPGA)
		mt_eint_emu_set(cur_eint_num);
#else
		mt_eint_soft_set(cur_eint_num);
#endif
	} else if (num == 0) {
#if   defined(CONFIG_MT6585_FPGA) || defined(CONFIG_MT8135_FPGA)
		mt_eint_emu_clr(cur_eint_num);
#else
		mt_eint_soft_clr(cur_eint_num);
#endif
	} else {
		pr_info("invalid number:%d it should be 1 to trigger interrupt or 0 to clr.\n",
			num);
	}

	return count;
}

DRIVER_ATTR(current_eint_soft_set, 0664, cur_eint_soft_set_show, cur_eint_soft_set_store);
static void mt_eint_soft_isr(void)
{
	EINT_FUNC.softisr_called[cur_eint_num] = 1;
	dbgmsg(KERN_DEBUG "in mt_eint_soft_isr\n");
#if defined(CONFIG_MT6585_FPGA) || defined(CONFIG_MT8135_FPGA)
	mt_eint_emu_clr(cur_eint_num);
#else
	mt_eint_soft_clr(cur_eint_num);
#endif
	return;
}

static ssize_t cur_eint_reg_isr_show(struct device_driver *driver, char *buf)
{
	/* if ISR has been registered return 1
	 * else return 0
	 */
	unsigned int sens, pol, deb, autounmask, base, dbnc;
	base = (cur_eint_num / 4) * 4 + EINT_DBNC_BASE;
	sens = mt_eint_get_sens(cur_eint_num);
	pol = mt_eint_get_polarity(cur_eint_num);
	autounmask = EINT_FUNC.eint_auto_umask[cur_eint_num];

	if (cur_eint_num >= MAX_HW_DEBOUNCE_CNT)
		deb = EINT_FUNC.deb_time[cur_eint_num];
	else {
		dbnc = readl((volatile unsigned int *)base);
		dbnc = ((dbnc >> EINT_DBNC_SET_DBNC_BITS) >> ((cur_eint_num % 4) * 8) & EINT_DBNC);

		switch (dbnc) {
		case 0:
			deb = 0;	/* 0.5 actually, but we don't allow user to set. */
			dbgmsg(KERN_DEBUG "ms should not be 0. eint_num:%d in %s\n",
			       cur_eint_num, __func__);
			break;
		case 1:
			deb = 1;
			break;
		case 2:
			deb = 16;
			break;
		case 3:
			deb = 32;
			break;
		case 4:
			deb = 64;
			break;
		case 5:
			deb = 128;
			break;
		case 6:
			deb = 256;
			break;
		case 7:
			deb = 512;
			break;
		default:
			deb = 0;
			pr_info("invalid deb time in the EIN_CON register, dbnc:%d, deb:%d\n", dbnc,
				deb);
			break;
		}
	}
	dbgmsg("sens:%d, pol:%d, deb:%d, isr:%p, autounmask:%d\n", sens, pol, deb,
	       EINT_FUNC.eint_func[cur_eint_num], autounmask);
	return snprintf(buf, PAGE_SIZE, "%d\n", deb);
}

static ssize_t cur_eint_reg_isr_store(struct device_driver *driver, const char *buf, size_t count)
{
	/* Get eint number */
	char *p = (char *)buf;
	unsigned int num;
	num = simple_strtoul(p, &p, 10);
	if (num != 1) {
		/* dbgmsg("Unregister soft isr\n"); */
		pr_info("Unregister soft isr\n");
		mt_eint_mask(cur_eint_num);
	} else {
		/* register its ISR: mt_eint_soft_isr */
		/* level, high, deb time: 64ms, not auto unmask */
		mt_eint_set_sens(cur_eint_num, MT_LEVEL_SENSITIVE);
		mt_eint_set_hw_debounce(cur_eint_num, EINT_FUNC.deb_time[cur_eint_num]);
		mt_eint_registration(cur_eint_num, MT_EINT_POL_POS, mt_eint_soft_isr, 1);
	}
	return count;
}

DRIVER_ATTR(current_eint_reg_isr, 0644, cur_eint_reg_isr_show, cur_eint_reg_isr_store);

static ssize_t cur_eint_mask_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "eint-%d is %s\n", cur_eint_num,
			mt_eint_get_mask(cur_eint_num) ? "mask" : "unmask");
}

static ssize_t cur_eint_mask_store(struct device_driver *driver, const char *buf, size_t count)
{
	/* Get eint mask */
	char *p = (char *)buf;
	unsigned int num;
	num = simple_strtoul(p, &p, 10);

	if (num != 1)
		mt_eint_mask(cur_eint_num);
	else
		mt_eint_unmask(cur_eint_num);
	return count;
}

DRIVER_ATTR(current_eint_mask, 0644, cur_eint_mask_show, cur_eint_mask_store);
#endif				/*!EINT_TEST */

static void mt_eint_clear_one_domain(const struct eint_domain *domain)
{
	int i, end;
	unsigned int __iomem *reg;

	reg = domain->reg;
	end = domain->size;

	for (i = 0; i < end; i += 32, ++reg)
		writel(0, reg);

	wmb(); /* flush the last write */
}

static inline bool is_matching_domain(
	const struct eint_domain *dom, const struct eint_domain_config *conf)
{
	return conf->start >= dom->offset &&
		(conf->start+conf->size) <= (dom->offset + dom->size);
}

static const struct eint_domain *get_matching_domain(
	const struct eint_domain_group *grp,
	const struct eint_domain_config *conf)
{
	const struct eint_domain *domain;
	if (is_matching_domain(&grp->mt8135, conf)) {
		pr_debug("%s: matching domain is mt8135: start=%d, size=%d\n",
				__func__, conf->start, conf->size);
		domain = &grp->mt8135;
	} else if (is_matching_domain(&grp->mt6397, conf)) {
		pr_debug("%s: matching domain is mt6397: start=%d, size=%d\n",
				__func__, conf->start, conf->size);
		domain = &grp->mt6397;
	} else {
		pr_err("%s: no matching domain found: start=%d, size=%d\n",
			__func__, conf->start, conf->size);
		domain = NULL;
	}
	return domain;
}

/*
 * mt_eint_setdomains: set all eint_num according to domains setting
 */
static void mt_eint_setdomains(const struct eint_domain_group *domains,
	struct eint_domain_config *conf, int ndom)
{
	unsigned int start, end, i, j;
	unsigned int __iomem *reg;

	eint_conf = conf;

	/* Clear all domains */
	for (i = 0; i < ndom; i++) {
		mt_eint_clear_one_domain(&domains[i].mt8135);
		mt_eint_clear_one_domain(&domains[i].mt6397);
	}

	/* Set according to domain_conf */
	for (i = 0; i < ndom; i++) {
		const struct eint_domain *dom =
			get_matching_domain(&domains[i], &conf[i]);
		/* domain may be absent, e.g. if configuration is
		 * only using 2 domains, and 3-d is unused */
		if (!dom)
			continue;
		BUG_ON(conf[i].start&31);

		conf[i].valid = true;

		start = conf[i].start - dom->offset;
		end = start + conf[i].size;
		reg = dom->reg;

		for (j = 0; j < end; j += 32, ++reg) {
			if (j < start)
				continue;
			writel(0xffffffff, reg);
		}

		wmb(); /* flush the last write */
	}
}

#ifdef MD_EINT
typedef struct {
	char name[24];
	int eint_num;
	int eint_deb;
	int eint_pol;
	int eint_sens;
	int socket_type;
} MD_SIM_HOTPLUG_INFO;

#define MD_SIM_MAX 16
MD_SIM_HOTPLUG_INFO md_sim_info[MD_SIM_MAX];
unsigned int md_sim_counter = 0;

static int get_eint_attribute(char *name, unsigned int name_len, unsigned int type, char *result,
			      unsigned int *len)
{
	int i;
	int ret = 0;
	int *sim_info = (int *)result;
	pr_info("in %s\n", __func__);
#ifdef MD_EINT
	pr_info("[EINT]CUST_EINT_MD1_CNT:%d,CUST_EINT_MD2_CNT:%d\n", CUST_EINT_MD1_CNT,
		CUST_EINT_MD2_CNT);
#endif
	pr_info("query info: name:%s, type:%d, len:%d\n", name, type, name_len);
	if (len == NULL || name == NULL || result == NULL)
		return ERR_SIM_HOT_PLUG_NULL_POINTER;

	for (i = 0; i < md_sim_counter; i++) {
		pr_info("compare string:%s\n", md_sim_info[i].name);
		if (!strncmp(name, md_sim_info[i].name, name_len)) {
			switch (type) {
			case SIM_HOT_PLUG_EINT_NUMBER:
				*len = sizeof(md_sim_info[i].eint_num);
				memcpy(sim_info, &md_sim_info[i].eint_num, *len);
				pr_info("[EINT]eint_num:%d\n", md_sim_info[i].eint_num);
				break;

			case SIM_HOT_PLUG_EINT_DEBOUNCETIME:
				*len = sizeof(md_sim_info[i].eint_deb);
				memcpy(sim_info, &md_sim_info[i].eint_deb, *len);
				pr_info("[EINT]eint_deb:%d\n", md_sim_info[i].eint_deb);
				break;

			case SIM_HOT_PLUG_EINT_POLARITY:
				*len = sizeof(md_sim_info[i].eint_pol);
				memcpy(sim_info, &md_sim_info[i].eint_pol, *len);
				pr_info("[EINT]eint_pol:%d\n", md_sim_info[i].eint_pol);
				break;

			case SIM_HOT_PLUG_EINT_SENSITIVITY:
				*len = sizeof(md_sim_info[i].eint_sens);
				memcpy(sim_info, &md_sim_info[i].eint_sens, *len);
				pr_info("[EINT]eint_sens:%d\n", md_sim_info[i].eint_sens);
				break;

			case SIM_HOT_PLUG_EINT_SOCKETTYPE:
				*len = sizeof(md_sim_info[i].socket_type);
				memcpy(sim_info, &md_sim_info[i].socket_type, *len);
				pr_info("[EINT]socket_type:%d\n", md_sim_info[i].socket_type);
				break;

			default:
				ret = ERR_SIM_HOT_PLUG_QUERY_TYPE;
				*len = sizeof(int);
				memset(sim_info, 0xff, *len);
				break;
			}
			return ret;
		}
	}

	*len = sizeof(int);
	memset(sim_info, 0xff, *len);

	return ERR_SIM_HOT_PLUG_QUERY_STRING;
}

static int get_type(char *name)
{

	int type1 = 0x0;
	int type2 = 0x0;
#if defined(CONFIG_MTK_SIM1_SOCKET_TYPE) || defined(CONFIG_MTK_SIM2_SOCKET_TYPE)
	char *p;
#endif

#ifdef CONFIG_MTK_SIM1_SOCKET_TYPE
	p = (char *)CONFIG_MTK_SIM1_SOCKET_TYPE;
	type1 = simple_strtoul(p, &p, 10);
#endif
#ifdef CONFIG_MTK_SIM2_SOCKET_TYPE
	p = (char *)CONFIG_MTK_SIM2_SOCKET_TYPE;
	type2 = simple_strtoul(p, &p, 10);
#endif
	if (!strncmp(name, "MD1_SIM1_HOT_PLUG_EINT", strlen("MD1_SIM1_HOT_PLUG_EINT")))
		return type1;
	else if (!strncmp(name, "MD1_SIM1_HOT_PLUG_EINT", strlen("MD1_SIM1_HOT_PLUG_EINT")))
		return type1;
	else if (!strncmp(name, "MD2_SIM2_HOT_PLUG_EINT", strlen("MD2_SIM2_HOT_PLUG_EINT")))
		return type2;
	else if (!strncmp(name, "MD2_SIM2_HOT_PLUG_EINT", strlen("MD2_SIM2_HOT_PLUG_EINT")))
		return type2;
	else
		return 0;
}
#endif
static void setup_MD_eint(void)
{
#ifdef MD_EINT

	pr_info("[EINT]CUST_EINT_MD1_CNT:%d,CUST_EINT_MD2_CNT:%d\n", CUST_EINT_MD1_CNT,
		CUST_EINT_MD2_CNT);

#if defined(CUST_EINT_MD1_0_NAME)
	sprintf(md_sim_info[md_sim_counter].name, CUST_EINT_MD1_0_NAME);
	md_sim_info[md_sim_counter].eint_num = CUST_EINT_MD1_0_NUM;
	md_sim_info[md_sim_counter].eint_pol = CUST_EINT_MD1_0_POLARITY;
	md_sim_info[md_sim_counter].eint_sens = CUST_EINT_MD1_0_SENSITIVE;
	md_sim_info[md_sim_counter].socket_type = get_type(md_sim_info[md_sim_counter].name);
	md_sim_info[md_sim_counter].eint_deb = CUST_EINT_MD1_0_DEBOUNCE_CN;
	pr_info("[EINT] MD1 name = %s\n", md_sim_info[md_sim_counter].name);
	pr_info("[EINT] MD1 type = %d\n", md_sim_info[md_sim_counter].socket_type);
	md_sim_counter++;
#endif
#if defined(CUST_EINT_MD1_1_NAME)
	sprintf(md_sim_info[md_sim_counter].name, CUST_EINT_MD1_1_NAME);
	md_sim_info[md_sim_counter].eint_num = CUST_EINT_MD1_1_NUM;
	md_sim_info[md_sim_counter].eint_pol = CUST_EINT_MD1_1_POLARITY;
	md_sim_info[md_sim_counter].eint_sens = CUST_EINT_MD1_1_SENSITIVE;
	md_sim_info[md_sim_counter].socket_type = get_type(md_sim_info[md_sim_counter].name);
	md_sim_info[md_sim_counter].eint_deb = CUST_EINT_MD1_1_DEBOUNCE_CN;
	pr_info("[EINT] MD1 name = %s\n", md_sim_info[md_sim_counter].name);
	pr_info("[EINT] MD1 type = %d\n", md_sim_info[md_sim_counter].socket_type);
	md_sim_counter++;
#endif
#if defined(CUST_EINT_MD1_2_NAME)
	sprintf(md_sim_info[md_sim_counter].name, CUST_EINT_MD1_2_NAME);
	md_sim_info[md_sim_counter].eint_num = CUST_EINT_MD1_2_NUM;
	md_sim_info[md_sim_counter].eint_pol = CUST_EINT_MD1_2_POLARITY;
	md_sim_info[md_sim_counter].eint_sens = CUST_EINT_MD1_2_SENSITIVE;
	md_sim_info[md_sim_counter].socket_type = get_type(md_sim_info[md_sim_counter].name);
	md_sim_info[md_sim_counter].eint_deb = CUST_EINT_MD1_2_DEBOUNCE_CN;
	pr_info("[EINT] MD1 name = %s\n", md_sim_info[md_sim_counter].name);
	pr_info("[EINT] MD1 type = %d\n", md_sim_info[md_sim_counter].socket_type);
	md_sim_counter++;
#endif
#if defined(CUST_EINT_MD1_3_NAME)
	sprintf(md_sim_info[md_sim_counter].name, CUST_EINT_MD1_3_NAME);
	md_sim_info[md_sim_counter].eint_num = CUST_EINT_MD1_3_NUM;
	md_sim_info[md_sim_counter].eint_pol = CUST_EINT_MD1_3_POLARITY;
	md_sim_info[md_sim_counter].eint_sens = CUST_EINT_MD1_3_SENSITIVE;
	md_sim_info[md_sim_counter].socket_type = get_type(md_sim_info[md_sim_counter].name);
	md_sim_info[md_sim_counter].eint_deb = CUST_EINT_MD1_3_DEBOUNCE_CN;
	pr_info("[EINT] MD1 name = %s\n", md_sim_info[md_sim_counter].name);
	pr_info("[EINT] MD1 type = %d\n", md_sim_info[md_sim_counter].socket_type);
	md_sim_counter++;
#endif
#if defined(CUST_EINT_MD1_4_NAME)
	sprintf(md_sim_info[md_sim_counter].name, CUST_EINT_MD1_4_NAME);
	md_sim_info[md_sim_counter].eint_num = CUST_EINT_MD1_4_NUM;
	md_sim_info[md_sim_counter].eint_pol = CUST_EINT_MD1_4_POLARITY;
	md_sim_info[md_sim_counter].eint_sens = CUST_EINT_MD1_4_SENSITIVE;
	md_sim_info[md_sim_counter].socket_type = get_type(md_sim_info[md_sim_counter].name);
	md_sim_info[md_sim_counter].eint_deb = CUST_EINT_MD1_4_DEBOUNCE_CN;
	pr_info("[EINT] MD1 name = %s\n", md_sim_info[md_sim_counter].name);
	pr_info("[EINT] MD1 type = %d\n", md_sim_info[md_sim_counter].socket_type);
	md_sim_counter++;
#endif

#if defined(CUST_EINT_MD2_0_NAME)
	sprintf(md_sim_info[md_sim_counter].name, CUST_EINT_MD2_0_NAME);
	md_sim_info[md_sim_counter].eint_num = CUST_EINT_MD2_0_NUM;
	md_sim_info[md_sim_counter].eint_pol = CUST_EINT_MD2_0_POLARITY;
	md_sim_info[md_sim_counter].eint_sens = CUST_EINT_MD2_0_SENSITIVE;
	md_sim_info[md_sim_counter].socket_type = get_type(md_sim_info[md_sim_counter].name);
	md_sim_info[md_sim_counter].eint_deb = CUST_EINT_MD2_0_DEBOUNCE_CN;
	pr_info("[EINT] MD2 name = %s\n", md_sim_info[md_sim_counter].name);
	pr_info("[EINT] MD2 type = %d\n", md_sim_info[md_sim_counter].socket_type);
	md_sim_counter++;
#endif
#if defined(CUST_EINT_MD2_1_NAME)
	sprintf(md_sim_info[md_sim_counter].name, CUST_EINT_MD2_1_NAME);
	md_sim_info[md_sim_counter].eint_num = CUST_EINT_MD2_1_NUM;
	md_sim_info[md_sim_counter].eint_pol = CUST_EINT_MD2_1_POLARITY;
	md_sim_info[md_sim_counter].eint_sens = CUST_EINT_MD2_1_SENSITIVE;
	md_sim_info[md_sim_counter].socket_type = get_type(md_sim_info[md_sim_counter].name);
	md_sim_info[md_sim_counter].eint_deb = CUST_EINT_MD2_1_DEBOUNCE_CN;
	pr_info("[EINT] MD2 name = %s\n", md_sim_info[md_sim_counter].name);
	pr_info("[EINT] MD2 type = %d\n", md_sim_info[md_sim_counter].socket_type);
	md_sim_counter++;
#endif
#if defined(CUST_EINT_MD2_2_NAME)
	sprintf(md_sim_info[md_sim_counter].name, CUST_EINT_MD2_2_NAME);
	md_sim_info[md_sim_counter].eint_num = CUST_EINT_MD2_2_NUM;
	md_sim_info[md_sim_counter].eint_pol = CUST_EINT_MD2_2_POLARITY;
	md_sim_info[md_sim_counter].eint_sens = CUST_EINT_MD2_2_SENSITIVE;
	md_sim_info[md_sim_counter].socket_type = get_type(md_sim_info[md_sim_counter].name);
	md_sim_info[md_sim_counter].eint_deb = CUST_EINT_MD2_2_DEBOUNCE_CN;
	dbgmsg("[EINT] MD2 name = %s\n", md_sim_info[md_sim_counter].name);
	dbgmsg("[EINT] MD2 type = %d\n", md_sim_info[md_sim_counter].socket_type);
	md_sim_counter++;
#endif
#if defined(CUST_EINT_MD2_3_NAME)
	sprintf(md_sim_info[md_sim_counter].name, CUST_EINT_MD2_3_NAME);
	md_sim_info[md_sim_counter].eint_num = CUST_EINT_MD2_3_NUM;
	md_sim_info[md_sim_counter].eint_pol = CUST_EINT_MD2_3_POLARITY;
	md_sim_info[md_sim_counter].eint_sens = CUST_EINT_MD2_3_SENSITIVE;
	md_sim_info[md_sim_counter].socket_type = get_type(md_sim_info[md_sim_counter].name);
	md_sim_info[md_sim_counter].eint_deb = CUST_EINT_MD2_3_DEBOUNCE_CN;
	pr_info("[EINT] MD2 name = %s\n", md_sim_info[md_sim_counter].name);
	pr_info("[EINT] MD2 type = %d\n", md_sim_info[md_sim_counter].socket_type);
	md_sim_counter++;
#endif
#if defined(CUST_EINT_MD2_4_NAME)
	sprintf(md_sim_info[md_sim_counter].name, CUST_EINT_MD2_4_NAME);
	md_sim_info[md_sim_counter].eint_num = CUST_EINT_MD2_4_NUM;
	md_sim_info[md_sim_counter].eint_pol = CUST_EINT_MD2_4_POLARITY;
	md_sim_info[md_sim_counter].eint_sens = CUST_EINT_MD2_4_SENSITIVE;
	md_sim_info[md_sim_counter].socket_type = get_type(md_sim_info[md_sim_counter].name);
	md_sim_info[md_sim_counter].eint_deb = CUST_EINT_MD2_4_DEBOUNCE_CN;
	pr_info("[EINT] MD2 name = %s\n", md_sim_info[md_sim_counter].name);
	pr_info("[EINT] MD2 type = %d\n", md_sim_info[md_sim_counter].socket_type);
	md_sim_counter++;
#endif
#endif				/* MD_EINT */
}

static int use_pmic_eint;
static void mt_eint_enable_pmic(void)
{
	unsigned int value;
	if (use_pmic_eint == 0) {
		pwrap_read(PMIC_WRP_CKPDN, &value);
		pwrap_write(PMIC_WRP_CKPDN, value & (~(0x1 << 5)));
		use_pmic_eint = 1;
	}
	dsb();
}

#if 0
static void mt_eint_disable_pmic(void)
{
	unsigned int value;
	if (use_pmic_eint == 1) {
		pwrap_read(PMIC_WRP_CKPDN, &value);
		pwrap_write(PMIC_WRP_CKPDN, value | (0x1 << 5));
		use_pmic_eint = 0;
	}
	dsb();
}
#endif

static void mt_eint_irq_mask(struct irq_data *d)
{
	struct mt_eint_chip *chip = d->chip_data;
	u32 mask = 1 << (d->hwirq & 0x1F);
	u32 port = (d->hwirq >> 5) & chip->port_mask;
	u32 reg = chip->base_addr + chip->mask_set + (port << 2);
	if (port >= chip->ports || chip->suspended)
		return;
	writel(mask, (void __iomem *)reg);
}

static void mt_eint_irq_unmask(struct irq_data *d)
{
	struct mt_eint_chip *chip = d->chip_data;
	u32 mask = 1 << (d->hwirq & 0x1F);
	u32 port = (d->hwirq >> 5) & chip->port_mask;
	u32 reg = chip->base_addr + chip->mask_clr + (port << 2);
	if (port >= chip->ports || chip->suspended)
		return;
	writel(mask, (void __iomem *)reg);
}

static void mt_eint_irq_ack(struct irq_data *d)
{
	struct mt_eint_chip *chip = d->chip_data;
	u32 mask = 1 << (d->hwirq & 0x1F);
	u32 port = (d->hwirq >> 5) & chip->port_mask;
	u32 reg = chip->base_addr + chip->ack + (port << 2);
	if (port >= chip->ports || chip->suspended)
		return;
	writel(mask, (void __iomem *)reg);
}

static void mt_eint_irq_set_polarity(struct irq_data *d, bool positive)
{
	struct mt_eint_chip *chip = d->chip_data;
	u32 mask = 1 << (d->hwirq & 0x1F);
	u32 port = (d->hwirq >> 5) & chip->port_mask;
	u32 reg = chip->base_addr + (port << 2);
	if (port >= chip->ports || chip->suspended)
		return;
	reg += positive ? chip->pol_set : chip->pol_clr;
	writel(mask, (void __iomem *)reg);
}

static void mt_eint_irq_set_sensitivity(struct irq_data *d, bool level_sensitive)
{
	struct mt_eint_chip *chip = d->chip_data;
	u32 mask = 1 << (d->hwirq & 0x1F);
	u32 port = (d->hwirq >> 5) & chip->port_mask;
	u32 reg = chip->base_addr + (port << 2);
	if (port >= chip->ports || chip->suspended)
		return;
	reg += level_sensitive ? chip->sens_set : chip->sens_clr;
	writel(mask, (void __iomem *)reg);
}

static int mt_eint_irq_set_type(struct irq_data *d, unsigned int type)
{
	/*
	 * Due to HW limitations, we only can configure 1 mode
	 * out of possible 4. No combinations are possible.
	 * 1. It is either edge sensitive, or level sensitive.
	 * 2. it is either positive or negative edge (not both).
	 * 3. it is either low  or high level (not both).
	 */
	if (((type & IRQ_TYPE_EDGE_BOTH) && (type & IRQ_TYPE_LEVEL_MASK)) ||
		((type & IRQ_TYPE_EDGE_BOTH) == IRQ_TYPE_EDGE_BOTH) ||
		((type & IRQ_TYPE_LEVEL_MASK) == IRQ_TYPE_LEVEL_MASK)) {
		pr_err("Can't configure IRQ%d (EINT%lu) for type 0x%X\n",
			d->irq, d->hwirq, type);
		return -EINVAL;
	}

	if (type & (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_EDGE_FALLING))
		mt_eint_irq_set_polarity(d, false);
	else
		mt_eint_irq_set_polarity(d, true);
	if (type & (IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING))
		mt_eint_irq_set_sensitivity(d, false);
	else
		mt_eint_irq_set_sensitivity(d, true);

	udelay(1);

	mt_eint_irq_ack(d);

	return 0;
}

static int mt_eint_irq_set_wake(struct irq_data *d, unsigned int on)
{
	struct mt_eint_chip *chip = irq_data_get_irq_chip_data(d);
	if (on)
		set_bit(d->hwirq, chip->wake_mask);
	else
		clear_bit(d->hwirq, chip->wake_mask);
	return 0;
}

static void mt_eint_chip_write_mask(struct mt_eint_chip *chip, u32 *buf)
{
	int port;
	for (port = 0; port < chip->ports; ++port) {
		u32 reg = chip->base_addr + (port << 2);
		writel(~buf[port], (void __iomem *)(reg + chip->mask_set));
		writel(buf[port], (void __iomem *)(reg + chip->mask_clr));
	}
	smp_wmb(); /* flush the last write */
}

static void mt_eint_chip_read_mask(struct mt_eint_chip *chip, u32 *buf)
{
	int port;
	smp_rmb(); /* ensure we're reading the current state */
	for (port = 0; port < chip->ports; ++port) {
		addr_t reg = chip->base_addr + chip->mask + (port << 2);
		buf[port] = ~readl((void __iomem *)reg);
	}
}

static int mt_eint_chip_read_status(
	struct mt_eint_chip *chip, u32 *buf, size_t size)
{
	int port;
	smp_rmb(); /* ensure we're reading the current state */
	if (size > chip->ports)
		size = chip->ports;
	for (port = 0; port < size; ++port) {
		u32 reg = chip->base_addr + chip->stat + (port << 2);
		buf[port] = readl((void __iomem *)reg);
	}
	return size;
}

void mt_eint_read_irq_status(u32 *buf, int nregs)
{
	int size = nregs;
	int max_size = mt8135_eint_chip.ports + mt6397_eint_chip.ports;

	if (size > max_size)
		size = max_size;

	if (size > 0)
		mt_eint_chip_read_status(&mt8135_eint_chip, buf, size);

	if (size > mt8135_eint_chip.ports)
		mt_eint_chip_read_status(&mt6397_eint_chip,
			buf  + mt8135_eint_chip.ports,
			size - mt8135_eint_chip.ports);
}
EXPORT_SYMBOL(mt_eint_read_irq_status);

static void mt_eint_chip_suspend(struct mt_eint_chip *chip)
{
	mt_eint_chip_read_mask(chip, (u32 *)chip->cur_mask);
	mt_eint_chip_write_mask(chip, (u32 *)chip->wake_mask);
}

struct mt_wake_event eint_event = {
	.domain = "EINT",
};

static inline u32 mt_eint_read_chip_irq_status_reg(struct mt_eint_chip *chip, u16 port)
{
	addr_t reg = chip->base_addr + chip->stat + (port << 2);
	return readl((void __iomem *)reg);
}

static void mt_eint_chip_resume(struct mt_eint_chip *chip)
{
	int port;
	struct mt_wake_event *we = spm_get_wakeup_event();

	if (we && we->domain && !strcmp(we->domain, "SPM") && we->code == 5) {
		for (port = 0; port < chip->ports; ++port) {
			u32 events = mt_eint_read_chip_irq_status_reg(chip, port);
			events &= chip->wake_mask[port];
			if (events) {
				int eint = __ffs(events) + (port << 5) + chip->base_eint;
				spm_report_wakeup_event(&eint_event, eint);
				break;
			}
		}
	}
	mt_eint_chip_write_mask(chip, (u32 *)chip->cur_mask);
}

static int mt_eint_suspend(void)
{
	mt_eint_chip_suspend(&mt8135_eint_chip);
	mt_eint_chip_suspend(&mt6397_eint_chip);
	return 0;
}

static void mt_eint_resume(void)
{
	mt_eint_chip_resume(&mt8135_eint_chip);
	mt_eint_chip_resume(&mt6397_eint_chip);
	mt65xx_reg_sync_writel(0x1, PMIC_EINT_EEVT_CLR_BASE);
}

static struct irq_chip mt_irq_eint = {
	.name = "mt-eint",
	.irq_set_wake  = mt_eint_irq_set_wake,
	.irq_mask = mt_eint_irq_mask,
	.irq_unmask = mt_eint_irq_unmask,
	.irq_ack = mt_eint_irq_ack,
	.irq_set_type = mt_eint_irq_set_type,
};

static struct syscore_ops mt_eint_syscore_ops = {
	.suspend = mt_eint_suspend,
	.resume = mt_eint_resume,
};

/*
 * mt_eint_init: initialize EINT driver.
 * Always return 0.
 */
int __init mt_eint_init(struct eint_domain_config *user_conf)
{
	unsigned int i, j;
	int irq_base;
	struct mt_eint_driver *eint_drv;
	struct eint_domain_config *conf = user_conf;

	/* assign EINTs to domains */
	if (!conf)
		conf = &default_eint_conf[0];
	else {
		pr_debug("User-defined EINT domains:\n");
		for (i = 0; i < EINT_DOMAIN_NUM; ++i)
			pr_debug("  domain=%d: start=%d, size=%d\n",
				i, conf[i].start, conf[i].size);
	}

	mt_eint_setdomains(eint_domains, conf, EINT_DOMAIN_NUM);
	setup_MD_eint();
	mt_eint_enable_pmic();
	for (i = 0; i < EINT_MAX_CHANNEL; i++) {
		struct mt_eint_def *eint = mt_get_eint_def(i);

		if (eint) {
			eint->chip = &mt8135_eint_chip;
			if (i >= eint->chip->nirqs)
				eint->chip = &mt6397_eint_chip;
		}

		EINT_FUNC.eint_func[i] = NULL;
		EINT_FUNC.is_deb_en[i] = 0;
		EINT_FUNC.deb_time[i] = 0;

		EINT_FUNC.eint_sw_deb_timer[i].expires = 0;
		EINT_FUNC.eint_sw_deb_timer[i].data = 0;
		EINT_FUNC.eint_sw_deb_timer[i].function = NULL;
#if defined(EINT_TEST)
		EINT_FUNC.softisr_called[i] = 0;
#endif

	}

	/* register EINT driver */
	eint_drv = get_mt_eint_drv();
	eint_drv->eint_max_channel = mt_eint_max_channel;
	eint_drv->enable = mt_eint_unmask;
	eint_drv->disable = mt_eint_mask;
	eint_drv->is_disable = mt_eint_get_mask;
	eint_drv->get_sens = mt_eint_get_sens;
	eint_drv->set_sens = mt_eint_set_sens;
	eint_drv->get_polarity = mt_eint_get_polarity;
	eint_drv->set_polarity = mt_eint_set_polarity;
	eint_drv->get_debounce_cnt = mt_eint_get_debounce_cnt;
	eint_drv->set_debounce_cnt = mt_eint_set_hw_debounce;
	eint_drv->is_debounce_en = mt_eint_is_debounce_en;
	eint_drv->enable_debounce = mt_eint_enable_debounce;
	eint_drv->disable_debounce = mt_eint_disable_debounce;

	/* Register Linux IRQ interface */
	irq_base = irq_alloc_descs(EINT_IRQ_BASE, EINT_IRQ_BASE, EINT_MAX_CHANNEL, numa_node_id());
	if (irq_base != EINT_IRQ_BASE)
		pr_err("EINT alloc desc error %d\n", irq_base);

	/* only enable vectors that are described in configuration */
	for (i = 0; i < EINT_DOMAIN_NUM; i++) {
		int start = eint_conf[i].start;
		int end = start + eint_conf[i].size;
		if (!eint_conf[i].valid)
			continue;
		for (j = start; j < end; ++j) {
			int idx = j + EINT_IRQ_BASE;
			struct mt_eint_def *eint = mt_get_eint_def(j);
			irq_set_chip_data(idx, eint->chip);
			irq_set_chip_and_handler(
				idx, &mt_irq_eint, handle_level_irq);
			set_irq_flags(idx, IRQF_VALID);
			pr_debug("Enable EINT vector for EINT%d (IRQ%d) in domain %d\n",
				j, idx, i);
		}
	}

	mt8135_eint_chip.domain = irq_domain_add_legacy(NULL,
		EINT_AP_MAXNUMBER,
		EINT_IRQ_BASE, 0,
		&irq_domain_simple_ops, &mt8135_eint_chip);

	mt6397_eint_chip.domain = irq_domain_add_legacy(NULL,
		EINT_MAX_CHANNEL - EINT_AP_MAXNUMBER,
		EINT_IRQ_BASE + EINT_AP_MAXNUMBER, 0,
		&irq_domain_simple_ops, &mt6397_eint_chip);

	if (!mt8135_eint_chip.domain || !mt6397_eint_chip.domain)
		pr_err("EINT domain add error\n");

	for (i = 0; i < EINT_DOMAIN_NUM; i++) {
		if (eint_conf[i].valid) {
			irq_set_chained_handler(EINT_IRQ + i, mt_eint_demux);
			pr_debug("Enable chained EINT IRQ handling for domain %d\n", i);
		}
	}

	register_syscore_ops(&mt_eint_syscore_ops);
	return 0;
}
