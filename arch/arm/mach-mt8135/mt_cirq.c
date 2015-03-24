#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <mach/mt_cirq.h>
#include <mach/mt_reg_base.h>
#include <asm/system.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/types.h>
#include <mach/mt_sleep.h>
#include "mach/sync_write.h"
#include "mach/irqs.h"
#include <asm/mach/irq.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
#include <asm/hardware/gic.h>
#else
#include <linux/irqchip/arm-gic.h>
#endif

static int mt_cirq_test(void);
#define CIRQ_DEBUG   0
#if (CIRQ_DEBUG == 1)
#ifdef CTP
#define dbgmsg dbg_print
#else
#define dbgmsg printk
#define print_func() pr_info("in %s\n", __func__);
#endif
#else
#define dbgmsg(...)
#define print_func() do { } while (0)
#endif

#define LDVT
#define INT_POL_CTL0 (MCUSYS_CFGREG_BASE + 0x100)

#define cirq_readl(addr)             readl((unsigned int *)(addr))

static struct platform_driver mt_cirq_drv = {
	.driver = {
		   .name = "cirq",
		   .bus = &platform_bus_type,
		   .owner = THIS_MODULE,
		   }
};

unsigned int mt_cirq_get_mask(unsigned int cirq_num)
{
	unsigned int base;
	unsigned int bit = 1 << (cirq_num % 32);
	phys_addr_t val;
	print_func();
	base = (cirq_num / 32) * 4 + CIRQ_MASK0;
	val = cirq_readl(base);
	if (val & bit)
		return 1;
	else
		return 0;
}

void mt_cirq_mask_all(void)
{
	int i;

	for (i = 0; i < 5; i++)
		mt65xx_reg_sync_writel(0xFFFFFFFF, CIRQ_MASK_SET0 + i * 4);
}

void mt_cirq_unmask_all(void)
{
	int i;

	for (i = 0; i < 5; i++)
		mt65xx_reg_sync_writel(0xFFFFFFFF, CIRQ_MASK_CLR0 + i * 4);
}

void mt_cirq_ack_all(void)
{
	int i;

	for (i = 0; i < 5; i++)
		mt65xx_reg_sync_writel(0xFFFFFFFF, CIRQ_ACK0 + i * 4);
}

void mt_cirq_mask(unsigned int cirq_num)
{
	unsigned int base;
	unsigned int bit = 1 << (cirq_num % 32);
	print_func();

	base = (cirq_num / 32) * 4 + CIRQ_ACK0;
	mt65xx_reg_sync_writel(bit, base);

	base = (cirq_num / 32) * 4 + CIRQ_MASK_SET0;
	mt65xx_reg_sync_writel(bit, base);

	dbgmsg(KERN_DEBUG "[CIRQ] mask addr:%x = %x\n", base, bit);

}

void mt_cirq_unmask(unsigned int cirq_num)
{
	unsigned int base;
	unsigned int bit = 1 << (cirq_num % 32);

	print_func();
	base = (cirq_num / 32) * 4 + CIRQ_ACK0;
	mt65xx_reg_sync_writel(bit, base);
	dbgmsg(KERN_DEBUG "[CIRQ] ack :%x, bit: %x\n", base, bit);

	base = (cirq_num / 32) * 4 + CIRQ_MASK_CLR0;
	mt65xx_reg_sync_writel(bit, base);

	dbgmsg(KERN_DEBUG "[CIRQ] unmask addr:%x = %x\n", base, bit);

}

static void mt_cirq_set_pol(unsigned int cirq_num, unsigned int pol)
{
	unsigned int base;
	unsigned int bit = 1 << (cirq_num % 32);

	print_func();
	if (pol == MT_CIRQ_POL_NEG)
		base = (cirq_num / 32) * 4 + CIRQ_POL_CLR0;
	else
		base = (cirq_num / 32) * 4 + CIRQ_POL_SET0;
	mt65xx_reg_sync_writel(bit, base);
	dbgmsg(KERN_DEBUG "[CIRQ] set pol,%d :%x, bit: %x\n", pol, base, bit);

}

unsigned int mt_cirq_set_sens(unsigned int cirq_num, unsigned int sens)
{
	unsigned int base;
	unsigned int bit = 1 << (cirq_num % 32);

	print_func();
	if (sens == MT_EDGE_SENSITIVE) {
		base = (cirq_num / 32) * 4 + CIRQ_SENS_CLR0;
	} else if (sens == MT_LEVEL_SENSITIVE) {
		base = (cirq_num / 32) * 4 + CIRQ_SENS_SET0;
	} else {
		dbgmsg(KERN_CRIT "%s invalid sensitivity value\n", __func__);
		return 0;
	}
	mt65xx_reg_sync_writel(bit, base);
	dbgmsg(KERN_DEBUG "[CIRQ] %s,sens:%d :%x, bit: %x\n", __func__, sens, base, bit);
	return 0;
}

unsigned int mt_cirq_get_sens(unsigned int cirq_num)
{

	unsigned int base;
	unsigned int bit = 1 << (cirq_num % 32);
	phys_addr_t val;
	print_func();
	base = (cirq_num / 32) * 4 + CIRQ_SENS0;
	val = cirq_readl(base);
	if (val & bit)
		return MT_LEVEL_SENSITIVE;
	else
		return MT_EDGE_SENSITIVE;

}

unsigned int mt_cirq_get_pol(unsigned int cirq_num)
{
	unsigned int base;
	unsigned int bit = 1 << (cirq_num % 32);
	phys_addr_t val;
	print_func();
	base = (cirq_num / 32) * 4 + CIRQ_POL0;
	val = cirq_readl(base);
	if (val & bit)
		return MT_CIRQ_POL_POS;
	else
		return MT_CIRQ_POL_NEG;
}

unsigned int mt_cirq_ack(unsigned int cirq_num)
{
	unsigned int base;
	unsigned int bit = 1 << (cirq_num % 32);
	print_func();

	base = (cirq_num / 32) * 4 + CIRQ_ACK0;
	mt65xx_reg_sync_writel(bit, base);


	dbgmsg(KERN_DEBUG "[CIRQ] ack addr:%x = %x\n", base, bit);

	return 0;
}

unsigned int mt_cirq_read_status(unsigned int cirq_num)
{
	unsigned int sta;
	unsigned int base;
	unsigned int bit = 1 << (cirq_num % 32);
	unsigned int irq_offset;
	unsigned int val;
	print_func();
	irq_offset = cirq_num / 32;

	base = (cirq_num / 32) * 4 + CIRQ_STA0;
	val = cirq_readl(base);
	sta = (val & bit);
	return sta;

}

void mt_cirq_enable(void)
{
	unsigned int base = CIRQ_CON;
	phys_addr_t val;
	val = cirq_readl(CIRQ_CON);
	val |= (0x3);		/* enable edge only mode */
	mt65xx_reg_sync_writel(val, base);
}
EXPORT_SYMBOL(mt_cirq_enable);

void mt_cirq_disable(void)
{
	unsigned int base = CIRQ_CON;
	phys_addr_t val;
	val = cirq_readl(CIRQ_CON);
	val &= (~0x1);
	mt65xx_reg_sync_writel(val, base);
}
EXPORT_SYMBOL(mt_cirq_disable);

void mt_cirq_flush(void)
{

	unsigned int irq;
	phys_addr_t val;
	print_func();

	/*make edge interrupt shows in the STA */
	mt_cirq_unmask_all();
	for (irq = 64; irq < (MT_NR_SPI + 32); irq += 32) {
		val = cirq_readl((((irq - 64) / 32) * 4 + CIRQ_STA0));
		/* pr_info("irq:%d,pending bit:%x\n", irq, val); */

		mt65xx_reg_sync_writel(val, (GIC_DIST_BASE + GIC_DIST_PENDING_SET + irq / 32 * 4));
		dsb();
		/* pr_info("irq:%d,pending bit:%x,%x\n", irq, val,
			readl(GIC_DIST_BASE + GIC_DIST_PENDING_SET + irq / 32 * 4)); */
	}
	mt_cirq_mask_all();
	dsb();
}
EXPORT_SYMBOL(mt_cirq_flush);

void mt_cirq_clone_pol(void)
{
	unsigned int irq, irq_bit, irq_offset;
	phys_addr_t value;
	phys_addr_t value_cirq;
	int ix;
	print_func();
	for (irq = 64; irq < (MT_NR_SPI + 32); irq += 32) {
		value = cirq_readl(INT_POL_CTL0 + ((irq - 32) / 32 * 4));
		irq_offset = (irq - 64) / 32;
		for (ix = 0; ix < 32; ix++) {
			dbgmsg(KERN_DEBUG "irq:%d,pol:%8x,value:0x%08x\n", irq,
			       INT_POL_CTL0 + (irq / 32 * 4), value);
			irq_bit = (irq_offset + ix) % 32;
			if (value & (0x1))	/* high trigger */
				mt_cirq_set_pol(irq + ix - 64, MT_CIRQ_POL_NEG);
			else	/* low trigger */
				mt_cirq_set_pol(irq + ix - 64, MT_CIRQ_POL_POS);

			value >>= 1;
		}
		value_cirq = cirq_readl(CIRQ_POL0 + irq_offset * 4);
		dbgmsg(KERN_DEBUG "irq:%d,cirq_value:0x%08x\n", irq, value_cirq);
	}
}

void mt_cirq_clone_sens(void)
{
	unsigned int irq, irq_bit, irq_offset;
	unsigned int value;
	unsigned int value_cirq;
	int ix;
	print_func();
	for (irq = 64; irq < (MT_NR_SPI + 16); irq += 16) {
		value = cirq_readl(GIC_DIST_BASE + GIC_DIST_CONFIG + (irq / 16) * 4);
		dbgmsg(KERN_DEBUG "irq:%d,sens:%08x,value:0x%08x\n", irq,
		       GIC_DIST_BASE + GIC_DIST_CONFIG + (irq / 16) * 4, value);
		irq_offset = (irq - 64) / 32;
		for (ix = 0; ix < 16; ix++) {
			irq_bit = (irq + ix) % 32;
			if (value & (0x2))	/* edge trigger */
				mt_cirq_set_sens(irq - 64 + ix, MT_EDGE_SENSITIVE);
			else	/* level trigger */
				mt_cirq_set_sens(irq - 64 + ix, MT_LEVEL_SENSITIVE);
			value >>= 2;
		}
		value_cirq = cirq_readl(CIRQ_SENS0 + irq_offset * 4);
		dbgmsg(KERN_DEBUG "irq:%d,cirq_value:0x%08x\n", irq, value_cirq);
	}
}

void mt_cirq_clone_mask(void)
{
	unsigned int irq, irq_bit, irq_offset;
	phys_addr_t value;
	phys_addr_t value_cirq;
	int ix;
	print_func();
	for (irq = 64; irq < (MT_NR_SPI + 32); irq += 32) {
		/* value = *(volatile u32 *)(GIC_DIST_BASE + GIC_DIST_ENABLE_SET + irq / 32 * 4) ; */
		value = cirq_readl(GIC_DIST_BASE + GIC_DIST_ENABLE_SET + irq / 32 * 4);
		dbgmsg(KERN_DEBUG "irq:%d,mask:%08x,value:0x%08x\n", irq,
		       (GIC_DIST_BASE + GIC_DIST_ENABLE_SET + irq / 32 * 4), value);
		irq_offset = (irq - 64) / 32;
		for (ix = 0; ix < 32; ix++) {
			irq_bit = (irq + ix) % 32;
			if (value & (0x1))	/* enable */
				mt_cirq_unmask(irq + ix - 64);
			else	/* disable */
				mt_cirq_mask(irq + ix - 64);

			value >>= 1;
		}
		value_cirq = cirq_readl(CIRQ_MASK0 + irq_offset * 4);
		dbgmsg(KERN_DEBUG "irq:%d,cirq_value:0x%08x\n", irq, value_cirq);
	}
}

void mt_cirq_clone_gic(void)
{
	mt_cirq_clone_pol();
	mt_cirq_clone_sens();
	mt_cirq_clone_mask();
}
EXPORT_SYMBOL(mt_cirq_clone_gic);

void mt_cirq_wfi_func(void)
{
	mt_cirq_mask_all();
	mt_cirq_ack_all();
}


#if defined(LDVT)
/*
 * cirq_dvt_show: To show usage.
 */
static ssize_t cirq_dvt_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CIRQ dvt test\n");
}

/*
 * mci_dvt_store: To select mci test case.
 */
static ssize_t cirq_dvt_store(struct device_driver *driver, const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int num;

	num = simple_strtoul(p, &p, 10);
	switch (num) {
	case 1:
		mt_cirq_clone_gic();
		break;
	case 2:
		mt_cirq_test();
		break;
	case 3:
		break;
	default:
		break;
	}

	return count;
}

DRIVER_ATTR(cirq_dvt, 0664, cirq_dvt_show, cirq_dvt_store);
#endif				/* !LDVT */

/*
 * CIRQ interrupt service routine.
 */
static irqreturn_t cirq_irq_handler(int irq, void *dev_id)
{
	pr_info("CIRQ_Handler\n");
	mt_cirq_ack_all();
	return IRQ_HANDLED;
}


void mt_cirq_dump_reg(void)
{
	int cirq_num;
	unsigned int pol, sens, mask;

	pr_info("IRQ:\tPOL\tSENS\tMASK\n");
	for (cirq_num = 0; cirq_num < MT_NR_SPI - 64; cirq_num++) {
		pol = mt_cirq_get_pol(cirq_num);
		sens = mt_cirq_get_sens(cirq_num);
		mask = mt_cirq_get_mask(cirq_num);
		pr_info("IRQ:%d\t%d\t%d\t%d\n", cirq_num + 64, pol, sens, mask);

	}
}

static int mt_cirq_test(void)
{
	int cirq_num = 100;
	mt_cirq_enable();

	/*test polarity */
	mt_cirq_set_pol(cirq_num, MT_CIRQ_POL_NEG);
	if (MT_CIRQ_POL_NEG != mt_cirq_get_pol(cirq_num))
		pr_info("mt_cirq_set_pol test failed!!\n");
	mt_cirq_set_pol(cirq_num, MT_CIRQ_POL_POS);
	if (MT_CIRQ_POL_POS != mt_cirq_get_pol(cirq_num))
		pr_info("mt_cirq_set_pol test failed!!\n");

	/*test sensitivity */
	mt_cirq_set_sens(cirq_num, MT_EDGE_SENSITIVE);
	if (MT_EDGE_SENSITIVE != mt_cirq_get_sens(cirq_num))
		pr_info("mt_cirq_set_sens test failed!!\n");
	mt_cirq_set_sens(cirq_num, MT_LEVEL_SENSITIVE);
	if (MT_LEVEL_SENSITIVE != mt_cirq_get_sens(cirq_num))
		pr_info("mt_cirq_set_sens test failed!!\n");

	/*test mask */
	mt_cirq_mask(cirq_num);
	if (1 != mt_cirq_get_mask(cirq_num))
		pr_info("mt_cirq_mask test failed!!\n");
	mt_cirq_unmask(cirq_num);
	mt_cirq_set_sens(cirq_num, MT_LEVEL_SENSITIVE);
	if (0 != mt_cirq_get_mask(cirq_num))
		pr_info("mt_cirq_unmask test failed!!\n");


	mt_cirq_clone_gic();
	mt_cirq_dump_reg();
	return 0;
}

/*
 * always return 0
 * */
static int mt_cirq_init(void)
{
	int ret;
	pr_info("CIRQ init...\n");
#if 1
	if (request_irq(MT_CIRQ_IRQ_ID, cirq_irq_handler, IRQF_TRIGGER_LOW, "CIRQ", NULL))
		pr_err("CIRQ IRQ LINE NOT AVAILABLE!!\n");
	else
		pr_info("CIRQ handler init success.");
#endif
	ret = driver_register(&mt_cirq_drv.driver);
#ifdef LDVT
	ret = driver_create_file(&mt_cirq_drv.driver, &driver_attr_cirq_dvt);
#endif
	if (ret == 0)
		pr_info("CIRQ init done...\n");
	return 0;

}
arch_initcall(mt_cirq_init);
