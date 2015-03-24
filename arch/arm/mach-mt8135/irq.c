#include <linux/io.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include <linux/cpu.h>
#include <linux/notifier.h>

#include <asm/mach/irq.h>
#include <asm/fiq.h>
#include <asm/fiq_glue.h>

#include <mach/mt_reg_base.h>
#include <mach/irqs.h>
#include <mach/sync_write.h>
#include <mach/mt_spm_idle.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
#include <asm/hardware/gic.h>
#else
#include <linux/irqchip/arm-gic.h>
#endif

#include <trustzone/kree/tz_irq.h>

#define GIC_ICDISR (GIC_DIST_BASE + 0x80)
#define GIC_ICDISER0 (GIC_DIST_BASE + 0x100)
#define GIC_ICDISER1 (GIC_DIST_BASE + 0x104)
#define GIC_ICDISER2 (GIC_DIST_BASE + 0x108)
#define GIC_ICDISER3 (GIC_DIST_BASE + 0x10C)
#define GIC_ICDISER4 (GIC_DIST_BASE + 0x110)
#define GIC_ICDISER5 (GIC_DIST_BASE + 0x114)
#define GIC_ICDISER6 (GIC_DIST_BASE + 0x118)
#define GIC_ICDISER7 (GIC_DIST_BASE + 0x11C)
#define GIC_ICDICER0 (GIC_DIST_BASE + 0x180)
#define GIC_ICDICER1 (GIC_DIST_BASE + 0x184)
#define GIC_ICDICER2 (GIC_DIST_BASE + 0x188)
#define GIC_ICDICER3 (GIC_DIST_BASE + 0x18C)
#define GIC_ICDICER4 (GIC_DIST_BASE + 0x190)
#define GIC_ICDICER5 (GIC_DIST_BASE + 0x194)
#define GIC_ICDICER6 (GIC_DIST_BASE + 0x198)
#define GIC_ICDICER7 (GIC_DIST_BASE + 0x19C)
#ifdef CONFIG_ARCH_MT8135
#define INT_POL_CTL0 (MCUSYS_CFGREG_BASE + 0x30)
#else
#define INT_POL_CTL0 (MCUSYS_CFGREG_BASE + 0x100)
#endif

#if defined(CONFIG_FIQ_GLUE)
static int irq_raise_softfiq(const struct cpumask *mask, unsigned int irq);
#else
#define irq_raise_softfiq(mask, irq)   0
#endif

static int (*org_gic_set_type) (struct irq_data *data, unsigned int flow_type);
static spinlock_t irq_lock;

/*
 * mt_irq_mask: disable an interrupt.
 * @data: irq_data
 */
void mt_irq_mask(struct irq_data *data)
{
	const unsigned int irq = data->irq;
	u32 mask = 1 << (irq % 32);

	if (irq < NR_GIC_SGI) {
		pr_crit("Fail to disable interrupt %d\n", irq);
		return;
	}

	*(volatile u32 *)(GIC_DIST_BASE + GIC_DIST_ENABLE_CLEAR + irq / 32 * 4) = mask;
	dsb();
}

/*
 * mt_irq_unmask: enable an interrupt.
 * @data: irq_data
 */
void mt_irq_unmask(struct irq_data *data)
{
	const unsigned int irq = data->irq;
	u32 mask = 1 << (irq % 32);

	if (irq < NR_GIC_SGI) {
		pr_crit("Fail to enable interrupt %d\n", irq);
		return;
	}

	*(volatile u32 *)(GIC_DIST_BASE + GIC_DIST_ENABLE_SET + irq / 32 * 4) = mask;
	dsb();
}

/*
 * mt_irq_set_sens: set the interrupt sensitivity
 * @irq: interrupt id
 * @sens: sensitivity
 */
void mt_irq_set_sens(unsigned int irq, unsigned int sens)
{
	unsigned long flags;
	u32 config;

	if (irq < (NR_GIC_SGI + NR_GIC_PPI)) {
		pr_crit("Fail to set sensitivity of interrupt %d\n", irq);
		return;
	}

	spin_lock_irqsave(&irq_lock, flags);

	if (sens == MT_EDGE_SENSITIVE) {
		config = readl((void *)(GIC_DIST_BASE + GIC_DIST_CONFIG + (irq / 16) * 4));
		config |= (0x2 << (irq % 16) * 2);
		writel(config, (void *)(GIC_DIST_BASE + GIC_DIST_CONFIG + (irq / 16) * 4));
	} else {
		config = readl((void *)(GIC_DIST_BASE + GIC_DIST_CONFIG + (irq / 16) * 4));
		config &= ~(0x2 << (irq % 16) * 2);
		writel(config, (void *)(GIC_DIST_BASE + GIC_DIST_CONFIG + (irq / 16) * 4));
	}

	spin_unlock_irqrestore(&irq_lock, flags);

	dsb();
}
EXPORT_SYMBOL(mt_irq_set_sens);

/*
 * mt_irq_set_polarity: set the interrupt polarity
 * @irq: interrupt id
 * @polarity: interrupt polarity
 */
void mt_irq_set_polarity(unsigned int irq, unsigned int polarity)
{
	unsigned long flags;
	u32 offset, reg_index, value;

	if (irq < (NR_GIC_SGI + NR_GIC_PPI)) {
		pr_crit("Fail to set polarity of interrupt %d\n", irq);
		return;
	}

	offset = (irq - GIC_PRIVATE_SIGNALS) & 0x1F;
	reg_index = (irq - GIC_PRIVATE_SIGNALS) >> 5;

	spin_lock_irqsave(&irq_lock, flags);

	if (polarity == 0) {
		/* active low */
		value = readl((void *)(INT_POL_CTL0 + (reg_index * 4)));
		value |= (1 << offset);
		mt65xx_reg_sync_writel(value, (INT_POL_CTL0 + (reg_index * 4)));
	} else {
		/* active high */
		value = readl((void *)(INT_POL_CTL0 + (reg_index * 4)));
		value &= ~(0x1 << offset);
		mt65xx_reg_sync_writel(value, INT_POL_CTL0 + (reg_index * 4));
	}

	spin_unlock_irqrestore(&irq_lock, flags);
}
EXPORT_SYMBOL(mt_irq_set_polarity);

#if defined(CONFIG_FIQ_GLUE)
/*
 * mt_irq_set_type: set interrupt type
 * @irq: interrupt id
 * @flow_type: interrupt type
 * Always return 0.
 */
static int mt_irq_set_type(struct irq_data *data, unsigned int flow_type)
{
	const unsigned int irq = data->irq;

	if (flow_type & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)) {
		mt_irq_set_sens(irq, MT_EDGE_SENSITIVE);
		mt_irq_set_polarity(irq, (flow_type & IRQF_TRIGGER_FALLING) ? 0 : 1);
		__irq_set_handler_locked(irq, handle_edge_irq);
	} else if (flow_type & (IRQF_TRIGGER_HIGH | IRQF_TRIGGER_LOW)) {
		mt_irq_set_sens(irq, MT_LEVEL_SENSITIVE);
		mt_irq_set_polarity(irq, (flow_type & IRQF_TRIGGER_LOW) ? 0 : 1);
		__irq_set_handler_locked(irq, handle_level_irq);
	}

	return 0;
}
#endif				/* CONFIG_FIQ_GLUE */

/*
 * mt_irq_polarity_set_type: set interrupt type wrapper for GIC
 * Check/set IRQ polarity for GIC interrupts
 */
static int mt_irq_polarity_set_type(struct irq_data *d, unsigned int type)
{
	unsigned int gicirq = d->hwirq;

	/* Set IRQ polarity */
	mt_irq_set_polarity(gicirq,
			    (type == IRQ_TYPE_LEVEL_HIGH || type == IRQ_TYPE_EDGE_RISING) ? 1 : 0);

	/* Adjust type for GIC. */
	if (type == IRQ_TYPE_LEVEL_LOW)
		type = IRQ_TYPE_LEVEL_HIGH;
	else if (type == IRQ_TYPE_EDGE_FALLING)
		type = IRQ_TYPE_EDGE_RISING;

	return org_gic_set_type(d, type);
}

void __cpuinit mt_gic_secondary_init(void)
{
	/* Dummy function, shouldn't be called. */
	BUG_ON(1);
}

void irq_raise_softirq(const struct cpumask *mask, unsigned int irq)
{
	if (irq_raise_softfiq(mask, irq))
		return;
	gic_raise_softirq(mask, irq);
}

int mt_irq_is_active(const unsigned int irq)
{
	const unsigned int iActive = readl((void *)(GIC_DIST_BASE + 0x200 + irq / 32 * 4));

	return iActive & (1 << (irq % 32)) ? 1 : 0;
}

/*
 * mt_enable_ppi: enable a private peripheral interrupt
 * @irq: interrupt id
 */
void mt_enable_ppi(int irq)
{
	u32 mask = 1 << (irq % 32);

	if (irq < NR_GIC_SGI) {
		pr_crit("Fail to enable PPI %d\n", irq);
		return;
	}
	if (irq >= (NR_GIC_SGI + NR_GIC_PPI)) {
		pr_crit("Fail to enable PPI %d\n", irq);
		return;
	}

	*(volatile u32 *)(GIC_DIST_BASE + GIC_DIST_ENABLE_SET + irq / 32 * 4) = mask;
	dsb();
}

/*
 * mt_PPI_mask_all: disable all PPI interrupts
 * @mask: pointer to struct mtk_irq_mask for storing the original mask value.
 * Return 0 for success; return negative values for failure.
 */
int mt_PPI_mask_all(struct mtk_irq_mask *mask)
{
	unsigned long flags;

	if (mask) {
#if defined(CONFIG_FIQ_GLUE)
		local_fiq_disable();
#endif
		spin_lock_irqsave(&irq_lock, flags);
		mask->mask0 = readl((void *)(GIC_ICDISER0));

		mt65xx_reg_sync_writel(0xFFFFFFFF, GIC_ICDICER0);

		spin_unlock_irqrestore(&irq_lock, flags);
#if defined(CONFIG_FIQ_GLUE)
		local_fiq_enable();
#endif

		mask->header = IRQ_MASK_HEADER;
		mask->footer = IRQ_MASK_FOOTER;

		return 0;
	} else {
		return -1;
	}
}

/*
 * mt_PPI_mask_restore: restore all PPI interrupts
 * @mask: pointer to struct mtk_irq_mask for storing the original mask value.
 * Return 0 for success; return negative values for failure.
 */
int mt_PPI_mask_restore(struct mtk_irq_mask *mask)
{
	unsigned long flags;

	if (!mask)
		return -1;

	if (mask->header != IRQ_MASK_HEADER)
		return -1;

	if (mask->footer != IRQ_MASK_FOOTER)
		return -1;

#if defined(CONFIG_FIQ_GLUE)
	local_fiq_disable();
#endif
	spin_lock_irqsave(&irq_lock, flags);

	mt65xx_reg_sync_writel(mask->mask0, GIC_ICDISER0);

	spin_unlock_irqrestore(&irq_lock, flags);
#if defined(CONFIG_FIQ_GLUE)
	local_fiq_enable();
#endif

	return 0;
}

/*
 * mt_SPI_mask_all: disable all SPI interrupts
 * @mask: pointer to struct mtk_irq_mask for storing the original mask value.
 * Return 0 for success; return negative values for failure.
 */
int mt_SPI_mask_all(struct mtk_irq_mask *mask)
{
	unsigned long flags;

	if (mask) {
#if defined(CONFIG_FIQ_GLUE)
		local_fiq_disable();
#endif
		spin_lock_irqsave(&irq_lock, flags);

		mask->mask1 = readl((void *)(GIC_ICDISER1));
		mask->mask2 = readl((void *)(GIC_ICDISER2));
		mask->mask3 = readl((void *)(GIC_ICDISER3));
		mask->mask4 = readl((void *)(GIC_ICDISER4));
		mask->mask5 = readl((void *)(GIC_ICDISER5));
		mask->mask6 = readl((void *)(GIC_ICDISER6));
		mask->mask7 = readl((void *)(GIC_ICDISER7));

		writel(0xFFFFFFFF, (void *)(GIC_ICDICER1));
		writel(0xFFFFFFFF, (void *)(GIC_ICDICER2));
		writel(0xFFFFFFFF, (void *)(GIC_ICDICER3));
		writel(0xFFFFFFFF, (void *)(GIC_ICDICER4));
		writel(0xFFFFFFFF, (void *)(GIC_ICDICER5));
		writel(0xFFFFFFFF, (void *)(GIC_ICDICER6));
		mt65xx_reg_sync_writel(0xFFFFFFFF, GIC_ICDICER7);

		spin_unlock_irqrestore(&irq_lock, flags);
#if defined(CONFIG_FIQ_GLUE)
		local_fiq_enable();
#endif

		mask->header = IRQ_MASK_HEADER;
		mask->footer = IRQ_MASK_FOOTER;

		return 0;
	} else {
		return -1;
	}
}

/*
 * mt_SPI_mask_restore: restore all SPI interrupts
 * @mask: pointer to struct mtk_irq_mask for storing the original mask value.
 * Return 0 for success; return negative values for failure.
 */
int mt_SPI_mask_restore(struct mtk_irq_mask *mask)
{
	unsigned long flags;

	if (!mask)
		return -1;

	if (mask->header != IRQ_MASK_HEADER)
		return -1;

	if (mask->footer != IRQ_MASK_FOOTER)
		return -1;

#if defined(CONFIG_FIQ_GLUE)
	local_fiq_disable();
#endif
	spin_lock_irqsave(&irq_lock, flags);

	writel(mask->mask1, (void *)(GIC_ICDISER1));
	writel(mask->mask2, (void *)(GIC_ICDISER2));
	writel(mask->mask3, (void *)(GIC_ICDISER3));
	writel(mask->mask4, (void *)(GIC_ICDISER4));
	writel(mask->mask5, (void *)(GIC_ICDISER5));
	writel(mask->mask6, (void *)(GIC_ICDISER6));
	mt65xx_reg_sync_writel(mask->mask7, GIC_ICDISER7);

	spin_unlock_irqrestore(&irq_lock, flags);
#if defined(CONFIG_FIQ_GLUE)
	local_fiq_enable();
#endif

	return 0;
}

/*
 * mt_irq_mask_all: disable all interrupts
 * @mask: pointer to struct mtk_irq_mask for storing the original mask value.
 * Return 0 for success; return negative values for failure.
 * (This is ONLY used for the idle current measurement by the factory mode.)
 */
int mt_irq_mask_all(struct mtk_irq_mask *mask)
{
	unsigned long flags;

	if (mask) {
#if defined(CONFIG_FIQ_GLUE)
		local_fiq_disable();
#endif
		spin_lock_irqsave(&irq_lock, flags);

#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
		kree_irq_mask_all(&mask->mask0, sizeof(struct mtk_irq_mask) -
				  sizeof(unsigned int) * 2);
#else
		mask->mask0 = readl((void *)(GIC_ICDISER0));
		mask->mask1 = readl((void *)(GIC_ICDISER1));
		mask->mask2 = readl((void *)(GIC_ICDISER2));
		mask->mask3 = readl((void *)(GIC_ICDISER3));
		mask->mask4 = readl((void *)(GIC_ICDISER4));
		mask->mask5 = readl((void *)(GIC_ICDISER5));
		mask->mask6 = readl((void *)(GIC_ICDISER6));
		mask->mask7 = readl((void *)(GIC_ICDISER7));

		writel(0xFFFFFFFF, (void *)(GIC_ICDICER0));
		writel(0xFFFFFFFF, (void *)(GIC_ICDICER1));
		writel(0xFFFFFFFF, (void *)(GIC_ICDICER2));
		writel(0xFFFFFFFF, (void *)(GIC_ICDICER3));
		writel(0xFFFFFFFF, (void *)(GIC_ICDICER4));
		writel(0xFFFFFFFF, (void *)(GIC_ICDICER5));
		writel(0xFFFFFFFF, (void *)(GIC_ICDICER6));
		mt65xx_reg_sync_writel(0xFFFFFFFF, GIC_ICDICER7);
#endif				/* CONFIG_MTK_IN_HOUSE_TEE_SUPPORT */

		spin_unlock_irqrestore(&irq_lock, flags);
#if defined(CONFIG_FIQ_GLUE)
		local_fiq_enable();
#endif

		mask->header = IRQ_MASK_HEADER;
		mask->footer = IRQ_MASK_FOOTER;

		return 0;
	} else {
		return -1;
	}
}

/*
 * mt_irq_mask_restore: restore all interrupts
 * @mask: pointer to struct mtk_irq_mask for storing the original mask value.
 * Return 0 for success; return negative values for failure.
 * (This is ONLY used for the idle current measurement by the factory mode.)
 */
int mt_irq_mask_restore(struct mtk_irq_mask *mask)
{
	unsigned long flags;

	if (!mask)
		return -1;

	if (mask->header != IRQ_MASK_HEADER)
		return -1;

	if (mask->footer != IRQ_MASK_FOOTER)
		return -1;

#if defined(CONFIG_FIQ_GLUE)
	local_fiq_disable();
#endif
	spin_lock_irqsave(&irq_lock, flags);

#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
	kree_irq_mask_restore(&mask->mask0, sizeof(struct mtk_irq_mask) - sizeof(unsigned int) * 2);
#else
	writel(mask->mask0, (void *)(GIC_ICDISER0));
	writel(mask->mask1, (void *)(GIC_ICDISER1));
	writel(mask->mask2, (void *)(GIC_ICDISER2));
	writel(mask->mask3, (void *)(GIC_ICDISER3));
	writel(mask->mask4, (void *)(GIC_ICDISER4));
	writel(mask->mask5, (void *)(GIC_ICDISER5));
	writel(mask->mask6, (void *)(GIC_ICDISER6));
	mt65xx_reg_sync_writel(mask->mask7, GIC_ICDISER7);
#endif				/* CONFIG_MTK_IN_HOUSE_TEE_SUPPORT */

	spin_unlock_irqrestore(&irq_lock, flags);
#if defined(CONFIG_FIQ_GLUE)
	local_fiq_enable();
#endif

	return 0;
}

/*
#define GIC_DIST_PENDING_SET            0x200
#define GIC_DIST_PENDING_CLEAR          0x280

 * mt_irq_set_pending_for_sleep: pending an interrupt for the sleep manager's use
 * @irq: interrupt id
 * (THIS IS ONLY FOR SLEEP FUNCTION USE. DO NOT USE IT YOURSELF!)
 */
void mt_irq_set_pending_for_sleep(unsigned int irq)
{
	u32 mask = 1 << (irq % 32);

	if (irq < NR_GIC_SGI) {
		pr_crit("Fail to set a pending on interrupt %d\n", irq);
		return;
	}

	*(volatile u32 *)(GIC_DIST_BASE + GIC_DIST_PENDING_SET + irq / 32 * 4) = mask;
	dsb();
}

/*
 * mt_irq_unmask_for_sleep: enable an interrupt for the sleep manager's use
 * @irq: interrupt id
 * (THIS IS ONLY FOR SLEEP FUNCTION USE. DO NOT USE IT YOURSELF!)
 *
 * !!!!NOTE!!!! Not support FIQ if TEE is enabled.
 */
void mt_irq_unmask_for_sleep(unsigned int irq)
{
	u32 mask = 1 << (irq % 32);

	if (irq < NR_GIC_SGI) {
		pr_crit("Fail to enable interrupt %d\n", irq);
		return;
	}

	*(volatile u32 *)(GIC_DIST_BASE + GIC_DIST_ENABLE_SET + irq / 32 * 4) = mask;
	dsb();
}

/*
 * mt_irq_mask_for_sleep: disable an interrupt for the sleep manager's use
 * @irq: interrupt id
 * (THIS IS ONLY FOR SLEEP FUNCTION USE. DO NOT USE IT YOURSELF!)
 *
 * !!!!NOTE!!!! Not support FIQ if TEE is enabled.
 */
void mt_irq_mask_for_sleep(unsigned int irq)
{
	u32 mask = 1 << (irq % 32);

	if (irq < NR_GIC_SGI) {
		pr_crit("Fail to enable interrupt %d\n", irq);
		return;
	}

	*(volatile u32 *)(GIC_DIST_BASE + GIC_DIST_ENABLE_CLEAR + irq / 32 * 4) = mask;
	dsb();
}

#if defined(CONFIG_FIQ_GLUE)
struct irq2fiq {
	int irq;
	int count;
	fiq_isr_handler handler;
	void *arg;
};

static struct irq2fiq irqs_to_fiq[] = {
	{.irq = MT_UART1_IRQ_ID,},
	{.irq = MT_UART2_IRQ_ID,},
	{.irq = MT_UART3_IRQ_ID,},
	{.irq = MT_UART4_IRQ_ID,},
	{.irq = MT_WDT_IRQ_ID,},
	{.irq = GIC_PPI_NS_PRIVATE_TIMER,},
	{.irq = FIQ_SMP_CALL_SGI,},
	{.irq = MT_SPM_IRQ1_ID,},	/* For 8135 SPM wfe issue. Don't need on other SoC. */
};

struct fiq_isr_log {
	int in_fiq_isr;
	int irq;
	int smp_call_cnt;
};

static struct fiq_isr_log fiq_isr_logs[NR_CPUS];
static unsigned int is_fiq_set[(NR_IRQS + 31) / 32];

static int fiq_glued;

#define line_is_fiq(no)    ((no) < NR_IRQS && (is_fiq_set[(no)/32] & (1<<((no)&31))))
#define line_set_fiq(no)   \
	do {               \
		if ((no) < NR_IRQS) \
			is_fiq_set[(no)/32] |= (1<<((no)&31)); \
	} while (0)


#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
static inline void __mt_enable_fiq(int irq)
{
	struct irq_data data;
	if (!line_is_fiq(irq)) {
		data.irq = irq;
		mt_irq_unmask(&data);
		return;
	}

	kree_enable_fiq(irq);
}

static inline void __mt_disable_fiq(int irq)
{
	struct irq_data data;
	if (!line_is_fiq(irq)) {
		data.irq = irq;
		mt_irq_mask(&data);
		return;
	}

	kree_disable_fiq(irq);
}

static int mt_irq_set_fiq(int irq, unsigned long irq_flags)
{
	struct irq_data data;
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&irq_lock, flags);
	ret = kree_set_fiq(irq, irq_flags);
	spin_unlock_irqrestore(&irq_lock, flags);
	if (!ret) {
		data.irq = irq;
		mt_irq_set_type(&data, irq_flags);
	}
	return ret;
}

static inline unsigned int mt_fiq_get_intack(void)
{
	return kree_fiq_get_intack();
}

static inline void mt_fiq_eoi(unsigned int iar)
{
	kree_fiq_eoi(iar);
}

static int irq_raise_softfiq(const struct cpumask *mask, unsigned int irq)
{
	if (!line_is_fiq(irq))
		return 0;

	kree_raise_softfiq(*cpus_addr(*mask), irq);
	return 1;
}

static int restore_for_fiq(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	/* Do nothing. TEE should recover GIC by itself */
	return NOTIFY_OK;
}
#else
static void __set_security(int irq);
static void __raise_priority(int irq);

static inline void __mt_enable_fiq(int irq)
{
	struct irq_data data;
	data.irq = irq;
	mt_irq_unmask(&data);
}

static inline void __mt_disable_fiq(int irq)
{
	struct irq_data data;
	data.irq = irq;
	mt_irq_mask(&data);
}

static int mt_irq_set_fiq(int irq, unsigned long irq_flags)
{
	struct irq_data data;

	__set_security(irq);
	__raise_priority(irq);
	data.irq = irq;
	mt_irq_set_type(&data, irq_flags);
	return 0;
}

static inline unsigned int mt_fiq_get_intack(void)
{
	return *((volatile unsigned int *)(GIC_CPU_BASE + GIC_CPU_INTACK));
}

static inline void mt_fiq_eoi(unsigned int iar)
{
	*(volatile unsigned int *)(GIC_CPU_BASE + GIC_CPU_EOI) = iar;

	dsb();
}

/* No TEE, just let kernel gic send it. */
static int irq_raise_softfiq(const struct cpumask *mask, unsigned int irq)
{
	return 0;
}

static void __set_security(int irq)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&irq_lock, flags);

	val = readl((void *)(GIC_ICDISR + 4 * (irq / 32)));
	val &= ~(1 << (irq % 32));
	writel(val, (void *)(GIC_ICDISR + 4 * (irq / 32)));

	spin_unlock_irqrestore(&irq_lock, flags);
}

static void __raise_priority(int irq)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&irq_lock, flags);

	val = readl((void *)(GIC_DIST_BASE + GIC_DIST_PRI + 4 * (irq / 4)));
	val &= ~(0xFF << ((irq % 4) * 8));
	val |= (0x50 << ((irq % 4) * 8));
	writel(val, (void *)(GIC_DIST_BASE + GIC_DIST_PRI + 4 * (irq / 4)));

	spin_unlock_irqrestore(&irq_lock, flags);
}

static int restore_for_fiq(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	int i;

	switch (action) {
	case CPU_STARTING:
		for (i = 0; i < ARRAY_SIZE(irqs_to_fiq); i++) {
			if ((irqs_to_fiq[i].irq < (NR_GIC_SGI + NR_GIC_PPI))
			    && (irqs_to_fiq[i].handler)) {
				__set_security(irqs_to_fiq[i].irq);
				__raise_priority(irqs_to_fiq[i].irq);
				dsb();
			}
		}
		break;

	default:
		break;
	}

	return NOTIFY_OK;
}
#endif				/* CONFIG_MTK_IN_HOUSE_TEE_SUPPORT */

/*
 * trigger_sw_irq: force to trigger a GIC SGI.
 * @irq: SGI number
 */
void trigger_sw_irq(int irq)
{
	if (irq < NR_GIC_SGI) {
		*((volatile unsigned int *)(GIC_DIST_BASE + GIC_DIST_SOFTINT)) = 0x18000 | irq;
	}
}

/*
 * mt_disable_fiq: disable an interrupt which is directed to FIQ.
 * @irq: interrupt id
 * Return error code.
 * NoteXXX: Not allow to suspend due to FIQ context.
 */
int mt_disable_fiq(int irq)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(irqs_to_fiq); i++) {
		if (irqs_to_fiq[i].irq == irq) {
			__mt_disable_fiq(irq);
			return 0;
		}
	}

	return -1;
}

/*
 * mt_enable_fiq: enable an interrupt which is directed to FIQ.
 * @irq: interrupt id
 * Return error code.
 * NoteXXX: Not allow to suspend due to FIQ context.
 */
int mt_enable_fiq(int irq)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(irqs_to_fiq); i++) {
		if (irqs_to_fiq[i].irq == irq) {
			__mt_enable_fiq(irq);
			return 0;
		}
	}

	return -1;
}

/*
 * For gic_arch_extn, check and mask if it is FIQ.
 */
static void mt_fiq_mask_check(struct irq_data *d)
{
	if (!line_is_fiq(d->irq))
		return;

	__mt_disable_fiq(d->irq);
}

/*
 * For gic_arch_extn, check and unmask if it is FIQ.
 */
static void mt_fiq_unmask_check(struct irq_data *d)
{
	if (!line_is_fiq(d->irq))
		return;

	__mt_enable_fiq(d->irq);
}

/*
 * fiq_isr: FIQ handler.
 */
static void fiq_isr(struct fiq_glue_handler *h, void *regs, void *svc_sp)
{
	unsigned int iar, irq;
	int i;

	iar = mt_fiq_get_intack();
	irq = iar & 0x3FF;

	if (irq >= NR_IRQS)
		return;

	for (i = 0; i < ARRAY_SIZE(irqs_to_fiq); i++) {
		if (irqs_to_fiq[i].irq == irq) {
			if (irqs_to_fiq[i].handler) {
				irqs_to_fiq[i].count++;
				(irqs_to_fiq[i].handler) (irqs_to_fiq[i].arg, regs, svc_sp);
			} else {
				pr_err("Interrupt %d triggers FIQ but no handler is registered\n",
				       irq);
			}
			break;
		}
	}
	if (i == ARRAY_SIZE(irqs_to_fiq)) {
		pr_err("Interrupt %d triggers FIQ but it is not registered\n", irq);
	}

	mt_fiq_eoi(iar);
}

/*
 * get_fiq_isr_log: get fiq_isr_log for debugging.
 * @cpu: processor id
 * @log: pointer to the address of fiq_isr_log
 * @len: length of fiq_isr_log
 * Return 0 for success or error code for failure.
 */
int get_fiq_isr_log(int cpu, unsigned int *log, unsigned int *len)
{
	if (log) {
		*log = (unsigned int)&(fiq_isr_logs[cpu]);
	}
	if (len) {
		*len = sizeof(struct fiq_isr_log);
	}

	return 0;
}

static struct notifier_block fiq_notifier = {
	.notifier_call = restore_for_fiq,
};

static struct fiq_glue_handler fiq_handler = {
	.fiq = fiq_isr,
};

static int __init_fiq(void)
{
	int ret;

	register_cpu_notifier(&fiq_notifier);

	/* Hook FIQ checker for mask/unmask. */
	/* So kernel enable_irq/disable_irq call will set FIQ correctly. */
	/* This is necessary for IRQ/FIQ suspend to work. */
	gic_arch_extn.irq_mask = mt_fiq_mask_check;
	gic_arch_extn.irq_unmask = mt_fiq_unmask_check;

	ret = fiq_glue_register_handler(&fiq_handler);
	if (ret) {
		pr_err("fail to register fiq_glue_handler\n");
	} else {
		fiq_glued = 1;
	}

	return ret;
}

/*
 * request_fiq: direct an interrupt to FIQ and register its handler.
 * @irq: interrupt id
 * @handler: FIQ handler
 * @irq_flags: IRQF_xxx
 * @arg: argument to the FIQ handler
 * Return error code.
 */
int request_fiq(int irq, fiq_isr_handler handler, unsigned long irq_flags, void *arg)
{
	int i;
	unsigned long flags;

	if (!fiq_glued) {
		__init_fiq();
	}

	for (i = 0; i < ARRAY_SIZE(irqs_to_fiq); i++) {
		spin_lock_irqsave(&irq_lock, flags);

		if (irqs_to_fiq[i].irq == irq) {

			irqs_to_fiq[i].handler = handler;
			irqs_to_fiq[i].arg = arg;

			line_set_fiq(irq);
			spin_unlock_irqrestore(&irq_lock, flags);

			if (mt_irq_set_fiq(irq, irq_flags))
				break;

			mb();

			__mt_enable_fiq(irq);

			return 0;
		}

		spin_unlock_irqrestore(&irq_lock, flags);
	}

	return -1;
}
#else
void trigger_sw_irq(int irq)
{
}

int mt_disable_fiq(int irq)
{
	return -ENODEV;
}

int mt_enable_fiq(int irq)
{
	return -ENODEV;
}

int get_fiq_isr_log(int cpu, unsigned int *log, unsigned int *len)
{
	return -ENODEV;
}

int request_fiq(int irq, fiq_isr_handler handler, unsigned long irq_flags, void *arg)
{
	return -ENODEV;
}
#endif				/* CONFIG_FIQ_GLUE */

static void __init mt_irq_polarity_init(void)
{
	struct irq_chip *gic_chip;

	gic_chip = irq_get_chip(MT_WDT_IRQ_ID);
	org_gic_set_type = gic_chip->irq_set_type;
	gic_chip->irq_set_type = mt_irq_polarity_set_type;
}

void __init mt_init_irq(void)
{
	struct irq_chip *gic_chip;

	spin_lock_init(&irq_lock);
	gic_chip = irq_get_chip(MT_WDT_IRQ_ID);
	/* Do hard-code init if irq is not initialized yet. */
	if (!gic_chip)
		gic_init_bases(0, -1, (void *)GIC_DIST_BASE, (void *)GIC_CPU_BASE, 0, NULL);
	gic_register_sgi(0, FIQ_DBG_SGI);
	mt_irq_polarity_init();
}

static unsigned int get_mask(int irq)
{
	unsigned int bit;
	bit = 1 << (irq % 32);
	return (readl(IOMEM(GIC_DIST_BASE + GIC_DIST_ENABLE_SET + irq / 32 * 4)) & bit) ? 1 : 0;
}

static unsigned int get_grp(int irq)
{
	unsigned int bit;
	bit = 1 << (irq % 32);
	/* 0x1:irq,0x0:fiq */
	return (readl(IOMEM(GIC_DIST_BASE + GIC_DIST_IGROUP + irq / 32 * 4)) & bit) ? 1 : 0;
}

static unsigned int get_pri(int irq)
{
	unsigned int bit;
	bit = 0xff << ((irq % 4) * 8);
	return (readl(IOMEM(GIC_DIST_BASE + GIC_DIST_PRI + irq / 4 * 4)) & bit) >>
		((irq % 4) * 8);
}

static unsigned int get_target(int irq)
{
	unsigned int bit;
	bit = 0xff << ((irq % 4) * 8);
	return (readl(IOMEM(GIC_DIST_BASE + GIC_DIST_TARGET + irq / 4 * 4)) & bit) >>
		((irq % 4) * 8);
}

static unsigned int get_sens(int irq)
{
	unsigned int bit;
	bit = 0x3 << ((irq % 16) * 2);
	/* edge:0x2, level:0x1 */
	return (readl(IOMEM(GIC_DIST_BASE + GIC_DIST_CONFIG + irq / 16 * 4)) & bit) >>
		((irq % 16) * 2);
}

static unsigned int get_pending_status(int irq)
{
	unsigned int bit;
	bit = 1 << (irq % 32);
	return (readl(IOMEM(GIC_DIST_BASE + GIC_DIST_PENDING_SET + irq / 32 * 4)) & bit) ? 1 : 0;
}

static unsigned int get_active_status(int irq)
{

	unsigned int bit;
	bit = 1 << (irq % 32);
	return (readl(IOMEM(GIC_DIST_BASE + GIC_DIST_ACTIVE_SET + irq / 32 * 4)) & bit) ? 1 : 0;
}



void mt_irq_dump_status(int irq)
{
	if (irq >= NR_IRQS)
		return;
	pr_info("[INT] irq:%d,enable:%x,active:%x,pending:%x,pri:%x,grp:%x,target:%x,sens:%x\n",
		irq, get_mask(irq), get_active_status(irq), get_pending_status(irq), get_pri(irq),
		get_grp(irq), get_target(irq), get_sens(irq));

}
EXPORT_SYMBOL(mt_irq_dump_status);
