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
#include <linux/types.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/amba/bus.h>

#include <linux/odin_mailbox.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/irqdomain.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/odin_pd.h>
#include <linux/odin_clk.h>
#include <linux/odin_panic.h>
#include <linux/reboot.h>
#include <linux/odin_wdt.h>

#define CREATE_TRACE_POINTS
#include <trace/events/ipc.h>

#include <linux/platform_data/gpio-odin.h>
#include <linux/platform_data/odin_tz.h>

#define IPCMxSOURCE(m)		((m) * 0x40)
#define IPCMxDSET(m)		(((m) * 0x40) + 0x004)
#define IPCMxDCLEAR(m)		(((m) * 0x40) + 0x008)
#define IPCMxDSTATUS(m)		(((m) * 0x40) + 0x00C)
#define IPCMxMODE(m)		(((m) * 0x40) + 0x010)
#define IPCMxMSET(m)		(((m) * 0x40) + 0x014)
#define IPCMxMCLEAR(m)		(((m) * 0x40) + 0x018)
#define IPCMxMSTATUS(m)		(((m) * 0x40) + 0x01C)
#define IPCMxSEND(m)		(((m) * 0x40) + 0x020)
#define IPCMxDR(m, dr)		(((m) * 0x40) + ((dr) * 4) + 0x024)

#define IPCMMIS(irq)		(((irq) * 8) + 0x800)
#define IPCMRIS(irq)		(((irq) * 8) + 0x804)

#define MBOX_MASK(n)		(1 << (n))
#define IPC_RX_MBOX			0
#define IPC_SLOW_MBOX		1
#define IPC_RX_1_MBOX		2
#define IPC_FAST_MBOX		3

#define CHAN_MASK(n)		(1 << (n))
#define DSP_SOURCE			3
#define CM3_2_SOURCE		2
#define CAX_SOURCE			1
#define CM3_SOURCE			0

#define MAILBOX_TIMEOUT_TIME	(1200)

struct sub_completion {
	spinlock_t spin_lock;
	struct mutex mutex;
	struct completion completion;
};

static struct sub_completion sub_completion_array[MB_COMPLETION_MAX];

struct pl320_mbox *pl320_mbox;
static void __iomem *ipc_base;
#ifndef CONFIG_ODIN_TEE
static void __iomem *memctl_base;
#else
static unsigned int memctl_base;
#endif
static void __iomem *in_atomic_mailbox_base;
static DEFINE_SPINLOCK(ipc_m0_lock);
static DEFINE_MUTEX(ipc_m1_lock);
static DECLARE_COMPLETION(ipc_completion);

static DECLARE_COMPLETION(mb_completion);
static unsigned int g_mb_data[MB_COMPLETION_MAX][7];
/*
 * These registers are modified under the irq bus lock and cached to avoid
 * unnecessary writes in bus_sync_unlock.
 */
static u32 pl320_mbox_irq_mask_regs[NR_MASK_REGS];
/* Mailbox H/W preparations */
static struct irq_chip pl320_mbox_irq_chip;
static struct irq_domain *pl320_mbox_irq_domain;

int ipc_irq;
extern odin_push_panic(void);

unsigned char is_mailbox_timeout=0;
unsigned char get_mailbox_status()
{
	return is_mailbox_timeout;
}
EXPORT_SYMBOL(get_mailbox_status);


static int mb_completion_func(unsigned int *data)
{
	int i;
	int sub_completion_num;
	unsigned int *mb_data = (unsigned int *)data;

	sub_completion_num = (mb_data[6] & SUB_COMPLETION_MASK) >> SUB_COMPLETION_POS;

	trace_ipc_completion(sub_completion_num, (mb_data[1] & (MB_COUNT_MASK << MB_COUNT_OFFSET)) >> MB_COUNT_OFFSET);

	for (i = 0; i < 7; i++)
		g_mb_data[sub_completion_num][i] = mb_data[i];

	complete(&sub_completion_array[sub_completion_num].completion);

	return 0;
}

/*  interrupt management */
static irqreturn_t mb_completion_handler(int irq, void *data)
{
	int i;
	int sub_completion_num;
	unsigned int *mb_data = (unsigned int *)data;

	sub_completion_num = (mb_data[6] & SUB_COMPLETION_MASK) >> SUB_COMPLETION_POS;

	for (i = 0; i < 7; i++)
		g_mb_data[sub_completion_num][i] = mb_data[i];

	complete(&sub_completion_array[sub_completion_num].completion);

	return IRQ_HANDLED;
}
/*  mask/unmask must be managed by SW */

static void mbox_irq_mask(struct irq_data *d)
{
	int offset = d->hwirq;
	int regoffset = offset / 32;
	u32 mask = 1 << (offset % 32);

	pl320_mbox_irq_mask_regs[regoffset] &= ~mask;
}

static void mbox_irq_unmask(struct irq_data *d)
{
	int offset = d->hwirq;
	int regoffset = offset / 32;
	u32 mask = 1 << (offset % 32);

	pl320_mbox_irq_mask_regs[regoffset] |= ~mask;
}

static void mbox_irq_ack(struct irq_data *d)
{
}

static struct irq_chip pl320_mbox_irq_chip = {
	.name		= "pl320_mbox",
	.irq_disable	= mbox_irq_unmask,
	.irq_ack	= mbox_irq_ack,
	.irq_mask	= mbox_irq_mask,
	.irq_unmask	= mbox_irq_unmask,
};

static int pl320_mbox_irq_map(struct irq_domain *d, unsigned int irq,
		irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(irq, &pl320_mbox_irq_chip,
			handle_simple_irq);
	irq_set_nested_thread(irq, 0);
	set_irq_flags(irq, IRQF_VALID);

	return 0;
}

static struct irq_domain_ops pl320_mbox_irq_ops = {
	.map    = pl320_mbox_irq_map,
	.xlate  = irq_domain_xlate_twocell,
};

static inline void set_destination(int source, int mbox)
{
	__raw_writel(CHAN_MASK(source), ipc_base + IPCMxDSET(mbox));
	__raw_writel(CHAN_MASK(source), ipc_base + IPCMxMSET(mbox));
}

static inline void clear_destination(int source, int mbox)
{
	__raw_writel(CHAN_MASK(source), ipc_base + IPCMxDCLEAR(mbox));
	__raw_writel(CHAN_MASK(source), ipc_base + IPCMxMCLEAR(mbox));
}

static void __ipc_send(int mbox, u32 *data)
{
	int i;
	for (i = 0; i < 7; i++)
		__raw_writel(data[i], ipc_base + IPCMxDR(mbox, i));
	__raw_writel(0x1, ipc_base + IPCMxSEND(mbox));
}

u32 __ipc_rcv(int mbox, u32 *data)
{
	int i;
	for (i = 0; i < 7; i++)
		data[i] = __raw_readl(ipc_base + IPCMxDR(mbox, i));
	return (data[1] & MB_RETRUN_MASK);
}

u32 __atomic_ipc_rcv(u32 *base, u32 *data)
{
	int i;
	for (i = 0; i < 7; i++) {
		data[i] = __raw_readl(base+i);
	}
	return data[1];
}

#ifdef CONFIG_ODIN_PL320_SW_WORKAROUND_MEM_DFS_BUS_LOCK
static DEFINE_SPINLOCK(mem_lock);

u32 ipc_call_fast_mem(u32 *data)
{
	static int prev;

	int timeout;
	u32 ret;
	unsigned long f0, f1;

	mutex_lock(&ipc_m1_lock);
	spin_lock_irqsave(&ipc_m0_lock, f0);
	spin_lock_irqsave(&mem_lock, f1);

#ifdef CONFIG_ODIN_TEE
	tz_write(0, memctl_base + 0x2C4);
#else
	__raw_writel(0, memctl_base + 0x2C4);
#endif
	__ipc_send(IPC_FAST_MBOX, data);

	for (timeout = 100000; timeout > 0; timeout--) {
#ifdef CONFIG_ODIN_TEE
		if (tz_read(memctl_base + 0x2C4) == 0x1)
#else
		if (__raw_readl(memctl_base + 0x2C4) == 0x1)
#endif
			break;

		if (prev == 24) /* in LPA, longer delay is needed */
			mdelay(1);
		else
			udelay(5);
	}
	if (timeout == 0) {
		ret = -1;
		goto out;
	}

	prev = data[5];

	ret = __ipc_rcv(IPC_FAST_MBOX, data);
	ret = data[2];
out:
	__raw_writel(0, ipc_base + IPCMxSEND(IPC_FAST_MBOX));

	spin_unlock_irqrestore(&mem_lock, f1);
	spin_unlock_irqrestore(&ipc_m0_lock, f0);
	mutex_unlock(&ipc_m1_lock);

	return ret;
}
EXPORT_SYMBOL(ipc_call_fast_mem);
#endif

/* non-blocking implementation from the A9 side, interrupt safe in theory */
int ipc_call_fast(u32 *data)
{
	int timeout, ret;
	unsigned long flags;

	spin_lock_irqsave(&ipc_m0_lock, flags);

	__ipc_send(IPC_FAST_MBOX, data);

	for (timeout = 100000; timeout > 0; timeout--) {
		if (__raw_readl(ipc_base + IPCMxSEND(IPC_FAST_MBOX)) == 0x2)
			break;
		udelay(5);
	}
	if (timeout == 0) {
		ret = -ETIMEDOUT;
		goto out;
	}

	ret = __ipc_rcv(IPC_FAST_MBOX, data);
out:
	__raw_writel(0, ipc_base + IPCMxSEND(IPC_FAST_MBOX));
	spin_unlock_irqrestore(&ipc_m0_lock, flags);

	return ret;
}
EXPORT_SYMBOL(ipc_call_fast);

/* blocking implmentation from the A9 side, not usuable in interrupts! */
int arch_ipc_call_fast(u32 *data)
{
	int timeout, ret;

	arch_spin_lock((arch_spinlock_t *)&ipc_m0_lock);

	__ipc_send(IPC_FAST_MBOX, data);

	for (timeout = 100000; timeout > 0; timeout--) {
		if (__raw_readl(ipc_base + IPCMxSEND(IPC_FAST_MBOX)) == 0x2)
			break;
		udelay(5);
	}
	if (timeout == 0) {
		ret = -ETIMEDOUT;
		goto out;
	}

	ret = __ipc_rcv(IPC_FAST_MBOX, data);
out:
	__raw_writel(0, ipc_base + IPCMxSEND(IPC_FAST_MBOX));
	arch_spin_unlock((arch_spinlock_t *)&ipc_m0_lock);

	return ret;
}
EXPORT_SYMBOL(arch_ipc_call_fast);

static int ipc_call_slow(u32 *data)
{
	int ret;


	mutex_lock(&ipc_m1_lock);

	init_completion(&ipc_completion);
	__ipc_send(IPC_SLOW_MBOX, data);
	ret = wait_for_completion_timeout(&ipc_completion,
					  msecs_to_jiffies(1000));
	if (ret == 0) {
		ret = -ETIMEDOUT;
		__raw_writel(0, ipc_base + IPCMxSEND(IPC_SLOW_MBOX));
		goto out;
	}

	ret = __ipc_rcv(IPC_SLOW_MBOX, data);
out:
	mutex_unlock(&ipc_m1_lock);

	return ret;
}

static u32 get_irq_stat(u32 irq_stat)
{
	if (irq_stat & MBOX_MASK(IPC_SLOW_MBOX))
		return IPC_SLOW_MBOX;
	if (irq_stat & MBOX_MASK(IPC_RX_MBOX))
		return IPC_RX_MBOX;
	if (irq_stat & MBOX_MASK(IPC_RX_1_MBOX))
		return IPC_RX_1_MBOX;

	return -EINVAL;
}

irqreturn_t ipc_handler(int irq, void *dev)
{
	struct pl320_mbox *pl320_mbox = dev;
	u32 data[7] = {0,};
	int i;

#ifdef CONFIG_ODIN_PL320_SW_WORKAROUND_MEM_DFS_BUS_LOCK
	unsigned long flags;
	spin_lock_irqsave(&mem_lock, flags);
#endif
	pl320_mbox->irq_stat = get_irq_stat(
			__raw_readl(ipc_base + IPCMMIS(1)));

	if (IPC_SLOW_MBOX == pl320_mbox->irq_stat) {
		__raw_writel(0, ipc_base + IPCMxSEND(IPC_SLOW_MBOX));
#ifdef CONFIG_ODIN_PL320_SW_WORKAROUND_MEM_DFS_BUS_LOCK
		spin_unlock_irqrestore(&mem_lock, flags);
#endif
		complete(&ipc_completion);
		return IRQ_HANDLED;
	}

	__ipc_rcv(pl320_mbox->irq_stat, data);
	pl320_mbox->sub_irq_num = data[6] & SUB_IRQ_NUM_MASK;


	for (i = 0; i < 7; i++)
		pl320_mbox->data[pl320_mbox->sub_irq_num][i] = data[i];

#ifdef CONFIG_ODIN_PL320_SW_WORKAROUND_MEM_DFS_BUS_LOCK
	spin_unlock_irqrestore(&mem_lock, flags);
#endif
	trace_ipc_handle(pl320_mbox->sub_irq_num, (data[1] & (MB_COUNT_MASK << MB_COUNT_OFFSET)) >> MB_COUNT_OFFSET);

	if (pl320_mbox->sub_irq_num < MB_SMS_GPIO_HANDLER_BASE) {
		if (pl320_mbox->sub_irq_num == MB_COMPLETION_HANDLER) {
			mb_completion_func(pl320_mbox->data[pl320_mbox->sub_irq_num]);
		}
	} else if (pl320_mbox->sub_irq_num >= MB_SMS_GPIO_HANDLER_BASE &&
			pl320_mbox->sub_irq_num < NUM_MB_SUB_IRQ) {
		generic_handle_irq(irq_create_mapping(pl320_mbox_irq_domain,
					pl320_mbox->sub_irq_num));
	}

	__raw_writel(2, ipc_base + IPCMxSEND(pl320_mbox->irq_stat));

	return IRQ_HANDLED;
}

int mailbox_request_irq(unsigned int sub_irq, irq_handler_t thread_fn,
		const char *name)
{
	int ret;

	if (sub_irq >= NUM_MB_SUB_IRQ) {
		ret = -EINVAL;
		pr_err("[pl320] Failed to add virture irq num: %s\n", name);
		goto err;
	}

	ret = request_threaded_irq(irq_create_mapping(pl320_mbox_irq_domain, sub_irq),
			thread_fn, NULL, IRQF_ONESHOT,
			name, pl320_mbox->data[sub_irq]);
	if (ret < 0) {
		pr_err("[pl320] Failed to request irq: %s\n", name);
		goto err;
	}

	return 0;
err:
	return ret;
}

int pl320_gpio_irq_base(int subirq_base, int irq_base_offset)
{
	if (irq_base_offset < 0)
		return -EINVAL;

	return irq_create_mapping(pl320_mbox_irq_domain, subirq_base)
			+ (irq_base_offset * ODIN_GPIO_NR);
}

static int __init pl320_early_init(void)
{
	int i;
	int ret;
	struct device_node *np;
	int irq_base;

	for (i = 0; i < MB_COMPLETION_MAX; i++) {
		spin_lock_init(&sub_completion_array[i].spin_lock);
		mutex_init(&sub_completion_array[i].mutex);
		init_completion(&sub_completion_array[i].completion);
	}

	pl320_mbox = kzalloc(sizeof(struct pl320_mbox), GFP_KERNEL);
	if (!pl320_mbox)
		return -ENOMEM;
	memset(pl320_mbox, 0, sizeof(struct pl320_mbox));

	np = of_find_compatible_node(NULL, NULL, "arm,pl320");
	ipc_base = of_iomap(np, 0);
	if (ipc_base == NULL)
		return -ENOMEM;

#ifdef CONFIG_ODIN_TEE
	memctl_base = 0x35000000;
#else
	memctl_base = ioremap(0x35000000, SZ_1K);
#endif
	__raw_writel(0, ipc_base + IPCMxSEND(IPC_FAST_MBOX));
	__raw_writel(0, ipc_base + IPCMxSEND(IPC_SLOW_MBOX));

	ipc_irq = irq_of_parse_and_map(np, 0);

	irq_base = irq_alloc_descs(-1, 0, NUM_MB_SUB_IRQ, 0);
	if (IS_ERR_VALUE(irq_base)) {
		pr_err("[pl320] Fail to allocate IRQ descs\n");
		ret = irq_base;
		goto err;
	}

	in_atomic_mailbox_base = ioremap(IN_ATOMIC_MAILBOX_BASE, SZ_64);
	__raw_writel(0, in_atomic_mailbox_base);

	pl320_mbox_irq_domain = irq_domain_add_simple(np,
			NUM_MB_SUB_IRQ, irq_base, &pl320_mbox_irq_ops,
			pl320_mbox);
	pl320_mbox->domain = pl320_mbox_irq_domain;

	if (!pl320_mbox_irq_domain) {
		pr_err("[PL320] Failed to create irqdomain\n");
		ret = -ENOSYS;
		goto err;
	}

	/* Init fast mailbox */
	__raw_writel(CHAN_MASK(CAX_SOURCE), ipc_base + IPCMxSOURCE(IPC_FAST_MBOX));
	set_destination(CM3_2_SOURCE, IPC_FAST_MBOX);

	/* Init slow mailbox */
	__raw_writel(CHAN_MASK(CAX_SOURCE), ipc_base + IPCMxSOURCE(IPC_SLOW_MBOX));
	__raw_writel(CHAN_MASK(CM3_SOURCE), ipc_base + IPCMxDSET(IPC_SLOW_MBOX));
	__raw_writel(CHAN_MASK(CM3_SOURCE) | CHAN_MASK(CAX_SOURCE),
		     ipc_base + IPCMxMSET(IPC_SLOW_MBOX));

	/* Init receive mailbox */
	__raw_writel(CHAN_MASK(CM3_SOURCE), ipc_base + IPCMxSOURCE(IPC_RX_MBOX));
	__raw_writel(CHAN_MASK(CAX_SOURCE), ipc_base + IPCMxDSET(IPC_RX_MBOX));
	__raw_writel(CHAN_MASK(CAX_SOURCE),
		     ipc_base + IPCMxMSET(IPC_RX_MBOX));

	/* Init receive mailbox */
	__raw_writel(CHAN_MASK(CM3_2_SOURCE), ipc_base + IPCMxSOURCE(IPC_RX_1_MBOX));
	__raw_writel(CHAN_MASK(CAX_SOURCE), ipc_base + IPCMxDSET(IPC_RX_1_MBOX));
	__raw_writel(CHAN_MASK(CM3_2_SOURCE) | CHAN_MASK(CAX_SOURCE),
		     ipc_base + IPCMxMSET(IPC_RX_1_MBOX));

	printk("[PL320] passed pl320_early_init\n");
	return 0;
err:
	kfree(pl320_mbox);
	iounmap(ipc_base);
	return ret;
}
early_initcall(pl320_early_init);

static int __init pl320_pure_init(void)
{
	int ret;
#if defined(CONFIG_ODIN_PL320_SW_WORKAROUND_IRQ_AFFINITY)
	cpumask_var_t new_value;
#endif

	ret = request_threaded_irq(ipc_irq, ipc_handler, NULL,
			IRQF_GIC_BALANCING | IRQF_NO_SUSPEND, "pl320", pl320_mbox);
	if (ret < 0) {
		printk("[pl320] Failed to request irq (ipc_threaded_handler)\n");
		goto err;
	}
	ret = mailbox_request_irq(MB_COMPLETION_HANDLER, mb_completion_handler,
					"MB_COMPLETION_HANDLER");
	if (ret != 0) {
		pr_err("failed to request irq (%d)\n", ret);
		goto err;
	}

#if defined(CONFIG_ODIN_PL320_SW_WORKAROUND_IRQ_AFFINITY)
	/* SW workaround for irq affinity */
	cpumask_clear(new_value);
	cpumask_set_cpu(2, new_value);
	cpumask_set_cpu(3, new_value);
	irq_set_affinity(ipc_irq, new_value);
	/* SW workaround for irq affinity */
#endif

	odin_get_all_pll_table();
	printk("[PL320] passed pl320_pure_init, ipc_irq: %d\n", ipc_irq);

err:
	return ret;
}
pure_initcall(pl320_pure_init);

#ifdef CONFIG_LGE_CRASH_HANDLER
#define LGE_CRASH_MSG_HDR "START LGE_CRASH_HANDLER"
#endif
static DEFINE_SPINLOCK(mb_count_lock);
static unsigned int count;
int mailbox_call(int ipc_type, int completion_sub_num, unsigned int *mbox_data,
		int timeout_ms)
{
	int ret;
	int timeout, i;
	unsigned int tmp_count;
	u32 irqnr_before, irqnr_after;

	timeout_ms = 2000;

	if (ipc_type == IN_ATOMIC_IPC_CALL) {
		spin_lock(&sub_completion_array[completion_sub_num].spin_lock);
	} else {
		mutex_lock(&sub_completion_array[completion_sub_num].mutex);
		init_completion(&sub_completion_array[completion_sub_num].completion);
	}

	spin_lock(&mb_count_lock);
	tmp_count = count;
	if (unlikely(count == MB_COUNT_MASK))
		count = 0;
	else
		count++;
	spin_unlock(&mb_count_lock);
	
	trace_ipc_call_entry(ipc_type, completion_sub_num, mbox_data[1], mbox_data, tmp_count, timeout_ms);
	mbox_data[0] |= ((tmp_count) & MB_COUNT_MASK) << MB_COUNT_OFFSET;

	if (ipc_type == FAST_IPC_CALL) {
		ret = ipc_call_fast(mbox_data);
	} else if (ipc_type == SLOW_IPC_CALL) {
		ret = ipc_call_slow(mbox_data);
	} else {
		mbox_data[0] |= IN_ATOMIC_IPC_CALL << IN_ATOMIC_IPC_CALL_OPS;
		ret = ipc_call_fast(mbox_data);
	}
	if (ret < 0) {
		pr_err("%s: failed to send a mail (%d)\n", __func__, ret);
		goto err;
	}

	irqnr_before = saved_irqnr;
	if (ipc_type == IN_ATOMIC_IPC_CALL) {

		for (timeout = timeout_ms*1000/20; timeout > 0; timeout--) {
			if (__raw_readl(in_atomic_mailbox_base +
						IN_ATOMIC_MAILBOX_COMPLETION_OFFSET)
					== (1 << completion_sub_num))
				break;
			udelay(20);
		}

		ret = timeout;
	} else {
		if (5000 > timeout_ms)
			timeout_ms = 5000;
		ret = wait_for_completion_timeout(
				&sub_completion_array[completion_sub_num].completion,
				msecs_to_jiffies(timeout_ms));
	}

	if (ret == 0) {
		irqnr_after = saved_irqnr;
		ret = -ETIMEDOUT;
#ifdef CONFIG_LGE_CRASH_HANDLER
		printk(KERN_ALERT "%s\n", LGE_CRASH_MSG_HDR);
#endif
		pr_err("%s: timeout while waiting for completion (%d)\n",
				__func__, ret);
		if ((irqnr_after > 15) && (irqnr_after < 1021)) {
			pr_err("%s: current irqnr: %d", __func__, irqnr_after);
			if (irqnr_before == irqnr_after) {
				pr_err("%s: irqnr[%d] handler may be running more than %d ms\n",
						__func__ ,irqnr_after, timeout_ms);
			}
		}
		goto err;
	}
	if (ipc_type == IN_ATOMIC_IPC_CALL) {
		__atomic_ipc_rcv(in_atomic_mailbox_base + IN_ATOMIC_MAILBOX_DATA_OFFSET,
				mbox_data);
	} else {
		for (i = 0; i < 7; i++)
			mbox_data[i] = g_mb_data[completion_sub_num][i];
	}

err:
	trace_ipc_call_exit(ipc_type, completion_sub_num, mbox_data, tmp_count, ret);

	if (ipc_type == IN_ATOMIC_IPC_CALL) {
		__raw_writel(0, in_atomic_mailbox_base +
				IN_ATOMIC_MAILBOX_COMPLETION_OFFSET);
		spin_unlock(&sub_completion_array[completion_sub_num].spin_lock);
	} else {
		mutex_unlock(&sub_completion_array[completion_sub_num].mutex);
	}

	if (ret == -ETIMEDOUT) {
		is_mailbox_timeout=1;
		dump_stack();
		force_reset_by_wdt();
		while(1)
			cpu_relax();
	}

	return ret;
}

#ifndef CONFIG_CRYPTO_DEV_ODIN_PE
static int __init ipc_test(void)
{
	tz_ses_pm(ODIN_SES_CC, 0x00000300);
	tz_ses_pm(ODIN_SES_PE, 0x00000300);
	printk("SES power domain off\n");
	return 0;
}

late_initcall(ipc_test);
#endif
