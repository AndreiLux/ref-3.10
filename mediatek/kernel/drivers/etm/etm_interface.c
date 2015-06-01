#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/aee.h>
#include <linux/dma-mapping.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/of_address.h>
#include <asm/sizes.h>
#include <mach/mt_reg_base.h>
#include <mach/sync_write.h>
#include <mach/etm.h>
#include "etm_register.h"
#include "etb_register.h"

#define TRACE_RANGE_START 0xbf000000	/* default trace range */
#define TRACE_RANGE_END	0xd0000000	/* default trace range */
#define TIMEOUT 1000000
#define ETB_TIMESTAMP 1 
#define ETB_CYCLE_ACCURATE 0
#define CS_TP_PORTSIZE 16
/* T32 is 0x2001, we can apply 0x1 is fine */
#define CS_FORMATMODE 0x11	/* Enable Continuous formatter and FLUSHIN */
#define ETM_DEBUG 0




enum {
	TRACE_STATE_STOP = 0,		/* trace stopped */
	TRACE_STATE_TRACING ,		/* tracing */
	TRACE_STATE_UNFORMATTING,	/* unformatting frame */
	TRACE_STATE_UNFORMATTED,	/* frame unformatted */
	TRACE_STATE_SYNCING,		/* syncing to trace head */
	TRACE_STATE_PARSING,		/* decoding packet */
};

struct etm_info
{
	int enable;
	int is_ptm;
	const int *pwr_down;
	u32 etmtsr;
	u32 etmtcr;
    u32 trcidr0;
    u32 trcidr2;
};

struct etm_trace_context_t 
{
	int nr_etm_regs;
	void __iomem **etm_regs;
	void __iomem *etb_regs;
	void __iomem *funnel_regs;
	void __iomem *tpiu_regs;
	void __iomem *dem_regs;
	u32 etr_virt;
	u32 etr_phys;
	u32 etr_len;
	int use_etr;
	int etb_total_buf_size;
	int enable_data_trace;
	u32 trace_range_start, trace_range_end;
	struct etm_info *etm_info;
	int etm_idx;
	int state;
	struct mutex mutex;
};

static struct etm_trace_context_t tracer;

DEFINE_PER_CPU(int, trace_pwr_down);

#define DBGRST_ALL (tracer.dem_regs + 0x028)
#define DBGBUSCLK_EN (tracer.dem_regs + 0x02C)
#define DBGSYSCLK_EN (tracer.dem_regs + 0x030)
#define AHBAP_EN (tracer.dem_regs + 0x040)
#define DEM_UNLOCK (tracer.dem_regs + 0xFB0)
#define DEM_UNLOCK_MAGIC 0xC5ACCE55
#define AHB_EN (1 << 0)
#define POWER_ON_RESET (0 << 0)
#define SYSCLK_EN (1 << 0)
#define BUSCLK_EN (1 << 0)

/**
 * read from ETB register
 * @param ctx trace context
 * @param x register offset
 * @return value read from the register
 */
unsigned int etb_readl(const struct etm_trace_context_t *ctx, int x)
{
	return __raw_readl(ctx->etb_regs + x);
}

/**
 * write to ETB register
 * @param ctx trace context
 * @param v value to be written to the register
 * @param x register offset
 * @return value written to the register
 */
void etb_writel(const struct etm_trace_context_t *ctx, unsigned int v, int x)
{
	mt_reg_sync_writel(v, ctx->etb_regs + x);
}

/**
 * check whether ETB supports lock
 * @param ctx trace context
 * @return 1:supports lock, 0:doesn't
 */
int etb_supports_lock(const struct etm_trace_context_t *ctx)
{
        pr_info("[ETM LOG] %s\n",__func__);
        pr_info("[ETM LOG] ETBLS &0x%x=0x%x\n",(vmalloc_to_pfn(ctx->etb_regs)<<12) + ETBLS,etb_readl(ctx,ETBLS));
        pr_info("[ETM LOG] %s Done\n",__func__);
	return etb_readl(ctx, ETBLS) & 0x1;
}

/**
 * check whether ETB registers are locked
 * @param ctx trace context
 * @return 1:locked, 0:aren't
 */
int etb_is_locked(const struct etm_trace_context_t *ctx)
{
        pr_info("[ETM LOG] %s\n",__func__);
        pr_info("[ETM LOG] ETBLS &0x%x=0x%x\n",(vmalloc_to_pfn(ctx->etb_regs)<<12) + ETBLS,etb_readl(ctx,ETBLS));
        pr_info("[ETM LOG] %s Done\n",__func__);
	return etb_readl(ctx, ETBLS) & 0x2;
}

/**
 * disable further write access to ETB registers
 * @param ctx trace context
 */
void etb_lock(const struct etm_trace_context_t *ctx)
{
	if (etb_supports_lock(ctx)) {
		do {
			etb_writel(ctx, 0, ETBLA);
		} while (unlikely(!etb_is_locked(ctx)));
	} else {
		pr_warning("ETB does not support lock\n");
	}
        pr_info("[ETM LOG] %s\n",__func__);
        pr_info("[ETM LOG] ETBLA &0x%x=0x%x\n",(vmalloc_to_pfn(ctx->etb_regs)<<12) + ETBLA,etb_readl(ctx,ETBLA));
        pr_info("[ETM LOG] %s Done\n",__func__);
     }

/**
 * enable further write access to ETB registers
 * @param ctx trace context
 */
void etb_unlock(const struct etm_trace_context_t *ctx)
{
	if (etb_supports_lock(ctx)) {
		do {
			etb_writel(ctx, ETBLA_UNLOCK_MAGIC, ETBLA);
		} while (unlikely(etb_is_locked(ctx)));
	} else {
		pr_warning("ETB does not support lock\n");
	}
        pr_info("[ETM LOG] %s\n",__func__);
        pr_info("[ETM LOG] ETBLA &0x%x=0x%x\n",(vmalloc_to_pfn(ctx->etb_regs)<<12) + ETBLA,etb_readl(ctx,ETBLA));
        pr_info("[ETM LOG] %s Done\n",__func__);
}

int etb_get_data_length(const struct etm_trace_context_t *t)
{
	unsigned int v;
	int rp, wp;

	v = etb_readl(t, ETBSTS);
	rp = etb_readl(t, ETBRRP);
	wp = etb_readl(t, ETBRWP);
	
	pr_info("ETB status = 0x%x, rp = 0x%x, wp = 0x%x\n", v, rp, wp);
        pr_info("[ETM LOG] %s\n",__func__);
        pr_info("[ETM LOG] ETB status = 0x%x, rp = 0x%x, wp = 0x%x\n", v, rp, wp);
 	if (v & 1) { 
		/* full */
		return t->etb_total_buf_size;
	} else {
		if (t->use_etr) {
			if (wp == 0) {
				/* The trace is never started yet. Return 0 */
				return 0;
			} else {
				return (wp - tracer.etr_phys) / 4;
			}
		} else {
			return wp / 4;
		}
	}
        pr_info("[ETM LOG] %s Done\n",__func__);
}

static int etb_open(struct inode *inode, struct file *file)
{
	if (!tracer.etb_regs)
		return -ENODEV;

	file->private_data = &tracer;

	return nonseekable_open(inode, file);
}

static ssize_t etb_read(struct file *file, char __user *data,
					size_t len, loff_t *ppos)
{
	int total, i;
	long length;
	struct etm_trace_context_t *t = file->private_data;
	u32 first = 0, buffer_end = 0;
	u32 *buf;
	int wpos;
	int skip;
	long wlength;
	loff_t pos = *ppos;

	mutex_lock(&t->mutex);
	etb_unlock(t);

	if (t->state == TRACE_STATE_TRACING) {
		length = 0;
		pr_err("Need to stop trace\n");
		goto out;
	}

	total = etb_get_data_length(t);

    // we assume the following is always true
    // because ETM produce log so fast so that 
    // the buffer is always full in circular mode
	if (total == t->etb_total_buf_size) {
		first = etb_readl(t, ETBRWP);
		if (t->use_etr) {
			first = (first - t->etr_phys) / 4;
		}
        else {
            first /= 4;
        }
	}

	if (pos > total * 4) {
		//skip = 0;
		//wpos = total;
		goto out;
	} else {
		skip = (int)pos % 4;
		wpos = (int)pos / 4;
	}

    total -= wpos;
    first = (first + wpos) % t->etb_total_buf_size;
    if(!t->use_etr) {
        // if it's ETB, we set RRP properly to read data
        etb_writel(t, first * 4, ETBRRP);
    }

	wlength = min(total, DIV_ROUND_UP(skip + (int)len, 4));
	length = min(total * 4 - skip, (int)len);
	if (wlength == 0) {
		goto out;
	}

	buf = vmalloc(wlength * 4);

	pr_info("ETB read %ld bytes to %lld from %ld words at %d\n",
		length, pos, wlength, first);
	pr_info("ETB buffer length: %d\n", (total + wpos) * 4);
	pr_info("ETB status reg: 0x%x\n", etb_readl(t, ETBSTS));

	if (t->use_etr) {
		/* 
		 * XXX: ETBRRP cannot wrap around correctly on ETR. 
		 *	  The workaround is to read the buffer from WTBRWP directly.
		 */

		pr_info("ETR virt = 0x%x, phys = 0x%x\n", t->etr_virt, t->etr_phys);

		/* translate first and buffer_end from phys to virt */
		first *= 4;
		first += t->etr_virt;
		buffer_end = t->etr_virt + (t->etr_len * 4);
		pr_info("first(virt) = 0x%x\n\n", first);

		for (i = 0; i < wlength; i++) {
			buf[i] = *((unsigned int*)(first));
			first += 4;
			if (first >= buffer_end) {
				first = t->etr_virt;  
			}
		}
	} else {
		for (i = 0; i < wlength; i++) {
			buf[i] = etb_readl(t, ETBRRD);
		}
	}


	length -= copy_to_user(data, (u8 *)buf + skip, length);
	vfree(buf);
	*ppos = pos + length;

out:
	etb_lock(t);
	mutex_unlock(&t->mutex);

	return length;
}

static struct file_operations etb_file_ops = {
	.owner = THIS_MODULE,
	.read = etb_read,
	.open = etb_open,
};

static struct miscdevice etb_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "etb",
	.fops = &etb_file_ops
};

static struct miscdevice etm_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "etm",
};

static long etb_read_test(char *data,
					size_t len, loff_t pos)
{
	int total, i;
	long length;
	struct etm_trace_context_t *t = &tracer; 
	u32 first = 0, buffer_end = 0;
	u32 *buf;
	int wpos;
	int skip;
	long wlength;

	mutex_lock(&t->mutex);
	etb_unlock(t);

	if (t->state == TRACE_STATE_TRACING) {
		length = 0;
		pr_err("Need to stop trace\n");
		goto out;
	}

	total = etb_get_data_length(t);

    // we assume the following is always true
    // because ETM produce log so fast so that 
    // the buffer is always full in circular mode
	if (total == t->etb_total_buf_size) {
		first = etb_readl(t, ETBRWP);
		if (t->use_etr) {
			first = (first - t->etr_phys) / 4;
		}
        else {
            first /= 4;
        }
	}

	if (pos > total * 4) {
		//skip = 0;
		//wpos = total;
		goto out;
	} else {
		skip = (int)pos % 4;
		wpos = (int)pos / 4;
	}

    total -= wpos;
    first = (first + wpos) % t->etb_total_buf_size;
    if(!t->use_etr) {
        // if it's ETB, we set RRP properly to read data
        etb_writel(t, first * 4, ETBRRP);
    }

	wlength = min(total, DIV_ROUND_UP(skip + (int)len, 4));
	length = min(total * 4 - skip, (int)len);
	if (wlength == 0) {
		goto out;
	}

	buf = vmalloc(wlength * 4);

	pr_info("ETB read %ld bytes to %lld from %ld words at %d\n",
		length, pos, wlength, first);
	pr_info("ETB buffer length: %d\n", (total + wpos) * 4);
	pr_info("ETB status reg: 0x%x\n", etb_readl(t, ETBSTS));

	if (t->use_etr) {
		/* 
		 * XXX: ETBRRP cannot wrap around correctly on ETR. 
		 *	  The workaround is to read the buffer from WTBRWP directly.
		 */

		pr_info("ETR virt = 0x%x, phys = 0x%x\n", t->etr_virt, t->etr_phys);

		/* translate first and buffer_end from phys to virt */
		first *= 4;
		first += t->etr_virt;
		buffer_end = t->etr_virt + (t->etr_len * 4);
		pr_info("first(virt) = 0x%x\n\n", first);

		for (i = 0; i < wlength; i++) {
			buf[i] = *((unsigned int*)(first));
			first += 4;
			if (first >= buffer_end) {
				first = t->etr_virt;  
			}
		}
	} else {
		for (i = 0; i < wlength; i++) {
			buf[i] = etb_readl(t, ETBRRD);
		}
	}

	//length -= copy_to_user(data, (u8 *)buf + skip, length);
    for(i = 0; i < wlength; i++) {
        printk("0x%08x ", buf[i]);
        if(i && !(i % 6))
            printk("\n");
    }

    printk("read done\n");
	vfree(buf);

out:
	etb_lock(t);
	mutex_unlock(&t->mutex);

	return length;
}

/**
 * read from ETM register
 * @param ctx trace context
 * @param n ETM index
 * @param x register offset
 * @return value read from the register
 */
unsigned int etm_readl(const struct etm_trace_context_t *ctx, int n, int x)
{
	return __raw_readl(ctx->etm_regs[n] + x);
}

#if 0
/**
 * write to ETM register
 * @param ctx trace context
 * @param n ETM index
 * @param v value to be written to the register
 * @param x register offset
 * @return value written to the register
 */
unsigned int etm_writel(const struct etm_trace_context_t *ctx, int n, unsigned int v,
							int x)
{
	return __raw_writel(v, ctx->etm_regs[n] + x);
}
#endif 

void cs_cpu_write(void __iomem *addr_base, u32 offset, u32 wdata)
{
	/* TINFO="Write addr %h, with data %h", addr_base+offset, wdata */
	mt_reg_sync_writel(wdata, addr_base + offset);
}

u32 cs_cpu_read(const void __iomem *addr_base, u32 offset)
{
	u32 actual;
 
	/* TINFO="Read addr %h, with data %h", addr_base+offset, actual */
	actual = __raw_readl(addr_base + offset);

	return actual;
}

#define SW_LOCK_IMPLEMENTED 0x1
#define SW_LOCK_LOCKED      0x2
#define OS_LOCK_BIT3        (0x1 << 3)
#define OS_LOCK_BIT0        (0x1 << 0) 
#define OS_LOCK_LOCKED      0x2
#define OS_LOCK_LOCK        0x1

void cs_cpu_lock(void __iomem *addr_base)
{
	u32 result;

    result = cs_cpu_read(addr_base, ETMLSR) & 0x3;
    /* if software lock locked? */
    pr_info("[ETM LOG] %s\n",__func__);
    pr_info("[ETM LOG] ETMLSR  &0x%x=0x%x\n",(vmalloc_to_pfn(addr_base)<<12)+ETMLSR,cs_cpu_read(addr_base,ETMLSR));
    switch(result) {
    case SW_LOCK_IMPLEMENTED | SW_LOCK_LOCKED:
        printk("addr @ %p already locked\n", addr_base);
        break;
    case SW_LOCK_IMPLEMENTED:
        printk("addr @ %p implemented SW lock but not locked\n", addr_base);
        cs_cpu_write(addr_base, ETMLAR, 0x0);
        pr_info("[ETM LOG] ETMLAR  &0x%x=0x%x\n",(vmalloc_to_pfn(addr_base)<<12)+ETMLAR,cs_cpu_read(addr_base,ETMLAR));
        dsb();
        break;
    default:
        printk("addr @ %p doesn't have SW lock\n", addr_base);
        break;
    }
    pr_info("[ETM LOG] %s Done\n",__func__);
}

void cs_cpu_oslock(void __iomem *addr_base)
{
	u32 result, oslm;

    result = cs_cpu_read(addr_base, ETMOSLSR);
    oslm = ((result & OS_LOCK_BIT3) >> 2) | (result & OS_LOCK_BIT0);
    pr_info("[ETM LOG] %s\n",__func__);
    pr_info("[ETM LOG] ETMOSLSR  &0x%x=0x%x\n",(vmalloc_to_pfn(addr_base)<<12)+ETMOSLSR,cs_cpu_read(addr_base,ETMOSLSR));
    if(!oslm) {
        printk("addr @ %p doens't have OS lock\n", addr_base);
    }
    else if(result & OS_LOCK_LOCKED) {
        printk("addr @ %p already locked\n", addr_base);
    }
    else {
        printk("addr @ %p implemented OS lock but not locked\n", addr_base);
        cs_cpu_write(addr_base, ETMOSLAR, OS_LOCK_LOCK);
        pr_info("[ETM LOG] ETMOSLAR  &0x%x=0x%x\n",(vmalloc_to_pfn(addr_base)<<12)+ETMOSLAR,cs_cpu_read(addr_base,ETMOSLAR));
    }
   pr_info("[ETM LOG] %s Done\n",__func__);
}
void cs_cpu_unlock(void __iomem *addr_base) 
{
	u32 result;

    result = cs_cpu_read(addr_base, ETMLSR) & 0x3;
    pr_info("[ETM LOG] %s\n",__func__);
    pr_info("[ETM LOG] ETMLSR  &0x%x=0x%x\n",(vmalloc_to_pfn(addr_base)<<12)+ETMLSR,cs_cpu_read(addr_base,ETMLSR));
    /* if software lock locked? */
    switch(result) {
    case SW_LOCK_IMPLEMENTED | SW_LOCK_LOCKED:
        printk("addr @ %p locked\n", addr_base);
        cs_cpu_write(addr_base, ETMLAR, 0xC5ACCE55);
        pr_info("[ETM LOG] ETMLAR  &0x%x=0x%x\n",(vmalloc_to_pfn(addr_base)<<12)+ETMLAR,cs_cpu_read(addr_base,ETMLAR));
        dsb();
        break;
    case SW_LOCK_IMPLEMENTED:
        printk("addr @ %p implemented SW already unlocked\n", addr_base);
        break;
    default:
        printk("addr @ %p doesn't have SW lock\n", addr_base);
        break;
    }
    pr_info("[ETM LOG] %s Done\n",__func__);
}

void cs_cpu_osunlock(void __iomem *addr_base)
{
	u32 result, oslm;
    result = cs_cpu_read(addr_base, ETMOSLSR);
    oslm = ((result & OS_LOCK_BIT3) >> 2) | (result & OS_LOCK_BIT0);
    pr_info("[ETM LOG] %s\n",__func__);
    pr_info("[ETM LOG] ETMOSLSR  &0x%x=0x%x\n",(vmalloc_to_pfn(addr_base)<<12)+ETMOSLSR,cs_cpu_read(addr_base,ETMOSLSR));
    if(!oslm) {
        printk("addr @ %p doens't have OS lock\n", addr_base);
    }
    else if(result & OS_LOCK_LOCKED) {
        printk("addr @ %p OS locked\n", addr_base);
        cs_cpu_write(addr_base, ETMOSLAR, ~OS_LOCK_LOCK);
        pr_info("[ETM LOG] ETMOSLAR  &0x%x=0x%x\n",(vmalloc_to_pfn(addr_base)<<12)+ETMOSLAR,cs_cpu_read(addr_base,ETMOSLAR));
    }
    else {
        printk("addr @ %p implemented OS lock but not locked\n", addr_base);
    }
    pr_info("[ETM LOG] %s Done\n",__func__);
}

void cs_cpu_ptm_powerup(void __iomem *ptm_addr_base)
{
	u32 result;

	result = cs_cpu_read(ptm_addr_base, 0x000);
	result = result ^ 0x001;
	cs_cpu_write(ptm_addr_base, 0x000, result);
        pr_info("[ETM LOG] %s\n",__func__);
        pr_info("[ETM LOG] &0x%x=0x%x\n",(vmalloc_to_pfn(ptm_addr_base)<<12),cs_cpu_read(ptm_addr_base,0));
        pr_info("[ETM LOG] %s Done\n",__func__);
}

#define PCR_ENABLE      0x1
#define TSR_IDLE        0x1
#define TSR_PMSTABLE    0x2
void cs_cpu_etm_enable(void __iomem *ptm_addr_base)
{
	u32 result;
	u32 counter = 0;

	/* Set ETMPCR to enable */ 
    cs_cpu_write(ptm_addr_base, ETMPCR, PCR_ENABLE);

	/* Wait for TSR_IDLE / TSR_PMSTABLE to be set */
	do {
		result = cs_cpu_read(ptm_addr_base, ETMTSR);
		counter++;
	} while(counter < TIMEOUT && (result & (TSR_IDLE | TSR_PMSTABLE)));

    if (counter >= TIMEOUT) {
        printk("%s, %p timeout, result = 0x%x\n", __func__, ptm_addr_base, result);
    }
        pr_info("[ETM LOG] %s\n",__func__);
        pr_info("[ETM LOG] ETMPCR  &0x%x=0x%x\n",(vmalloc_to_pfn(ptm_addr_base)<<12)+ETMPCR,cs_cpu_read(ptm_addr_base,ETMPCR));
        pr_info("[ETM LOG] ETMTSR  &0x%x=0x%x\n",(vmalloc_to_pfn(ptm_addr_base)<<12)+ETMTSR,cs_cpu_read(ptm_addr_base,ETMTSR));

        pr_info("[ETM LOG] %s Done\n",__func__);
}

void cs_cpu_etm_disable(void __iomem *ptm_addr_base)
{
	u32 result;
	u32 counter = 0;

	/* Set ETMPCR to disable */ 
    cs_cpu_write(ptm_addr_base, ETMPCR, ~PCR_ENABLE);

	/* Wait for TSR_IDLE / TSR_PMSTABLE to be set */
	do {
		result = cs_cpu_read(ptm_addr_base, ETMTSR);
		counter++;
	} while(counter < TIMEOUT && !(result & (TSR_IDLE | TSR_PMSTABLE)));

    if (counter >= TIMEOUT) {
        printk("%s, %p timeout, result = 0x%x\n", __func__, ptm_addr_base, result);
    }
        pr_info("[ETM LOG] %s\n",__func__);
        pr_info("[ETM LOG] ETMPCR  &0x%x=0x%x\n",(vmalloc_to_pfn(ptm_addr_base)<<12)+ETMPCR,cs_cpu_read(ptm_addr_base,ETMPCR));
        pr_info("[ETM LOG] ETMTSR  &0x%x=0x%x\n",(vmalloc_to_pfn(ptm_addr_base)<<12)+ETMTSR,cs_cpu_read(ptm_addr_base,ETMTSR));        
        pr_info("[ETM LOG] %s Done\n",__func__);
}

void cs_cpu_ptm_clear_progbit(void __iomem *ptm_addr_base)
{
	u32 result;
	u32 counter = 0;

	/* Clear PTMControl.PTMProgramming ([10]) */
	result = cs_cpu_read(ptm_addr_base, 0x000);
	cs_cpu_write(ptm_addr_base, 0x000, result ^ (1 << 10));

	/* Wait for PTMStatus.PTMProgramming ([1]) to be clear */
	do{
		result = cs_cpu_read(ptm_addr_base, 0x010);
		counter++;
		if (counter >= TIMEOUT) {
			//aee_sram_fiq_log("ETM clear program bit timeout!\n");		  
			break;
		}
	} while ((result & 0x2));

        pr_info("[ETM LOG] %s\n",__func__);
        pr_info("[ETM LOG] &0x%x=0x%x\n",(vmalloc_to_pfn(ptm_addr_base)<<12)+0x000,cs_cpu_read(ptm_addr_base,0x000));
        pr_info("[ETM LOG] &0x%x=0x%x\n",(vmalloc_to_pfn(ptm_addr_base)<<12)+0x010,cs_cpu_read(ptm_addr_base,0x010));
        pr_info("[ETM LOG] %s Done\n",__func__);
}

void cs_cpu_tpiu_setup(void)
{
	u32 result;

	/* Current Port Size - Port Size 16 */
	cs_cpu_write(tracer.tpiu_regs, 0x004, 1 << (CS_TP_PORTSIZE - 1));

	result = cs_cpu_read(tracer.tpiu_regs, 0x000);
	if (!(result & ((CS_TP_PORTSIZE - 1))) ) {
		/* TERR="MAXPORTSIZE tie-off conflicts with port size" */
	}

	/* Formatter and Flush Control Register - Enable Continuous formatter and FLUSHIN */
	cs_cpu_write(tracer.tpiu_regs, 0x304, CS_FORMATMODE);
}

void cs_cpu_funnel_setup(void)
{
	u32 funnel_ports, i;

	funnel_ports = 0;
        pr_info("[ETM LOG] %s \n",__func__);
	for (i = 0; i < tracer.nr_etm_regs; i++) {
		if (tracer.etm_info[i].enable) {
			funnel_ports = funnel_ports | (1 << i); 
                        pr_info("[ETM LOG] funnel_ports=0x%x, tracer.nr_etm_regs=0x%x\n",funnel_ports,tracer.nr_etm_regs);
		}
	}

	cs_cpu_write(tracer.funnel_regs, 0x000, funnel_ports);
        pr_info("[ETM LOG] funnel_ports &0x%x=0x%x\n",(vmalloc_to_pfn(tracer.funnel_regs)<<12),cs_cpu_read(tracer.funnel_regs,0x000));
        pr_info("[ETM LOG] %s Done\n",__func__);
#if 0
	/* Adjust priorities so that ITM has highest */
	cs_cpu_write (tracer.funnel_regs, 0x004, 0x00FAC0D1);
#endif
}

void cs_cpu_flushandstop(void __iomem *device_addr_base)
{
	u32 result;
	u32 counter = 0;

	/* Configure the device to stop on flush completion */
	cs_cpu_write(device_addr_base, 0x304, CS_FORMATMODE | (1 << 12));
	/* Cause a manual flush */
	cs_cpu_write(device_addr_base, 0x304, CS_FORMATMODE | (1 << 6));

	/* Now wait for the device to stop */
	result = 0x02;
	while (result & 0x02) {
		result = cs_cpu_read(device_addr_base, 0x300);
		counter++;
		if (counter >= TIMEOUT) {
			//aee_sram_fiq_log("ETM flush and stop timeout!\n");  
			break;
		}
	}
}

void cs_cpu_etb_setup(void)
{
	/* Formatter and Flush Control Register - Enable Continuous formatter and FLUSHIN */
	cs_cpu_write(tracer.etb_regs, ETBFFCR, CS_FORMATMODE);
	/* Configure ETB control (set TraceCapture) */
	cs_cpu_write(tracer.etb_regs, ETBCTL, 0x01);
        pr_info("[ETM LOG] %s\n",__func__);
        pr_info("[ETM LOG] ETBFFCR &0x%x=0x%x\n",vmalloc_to_pfn(tracer.etb_regs)+ETBFFCR,cs_cpu_read(tracer.etb_regs,ETBFFCR));
        pr_info("[ETM LOG] ETBCTL  &0x%x=0x%x\n",vmalloc_to_pfn(tracer.etb_regs)+ETBCTL,cs_cpu_read(tracer.etb_regs,ETBCTL));
        pr_info("[ETM LOG] %s Done\n",__func__);
}

#define RS_ARC_GROUP    (0x5 << 16)
#define RS_SELECT(x)    (0x1 << x)
#define SS_STATUS_EN    (0x1 << 9)
#define EVENT_SELECT(x) (x)
#define IN_SELECT(x)    (0x1 << x)
#define EX_SELECT(x)    (0x1 << (x + 16))
#define CCCI_SUPPORT    (0x1 << 7)
#define TSSIZE          (0x1F << 24)
#define CONFIG_TS       (0x1 << 11)
#define CONFIG_CCI      (0x1 << 4)
#define SYNCPR          (0x1 << 25)
void cs_cpu_etm_setup(void __iomem *ptm_addr_base, int core)
{
	u32 result, config;
        pr_info("[ETM LOG] %s\n",__func__);
	/*
	 * Set up to trace memory range defined by ARC1
	 * SAC1&2 (ARC1)
	 */
	cs_cpu_write(ptm_addr_base, ETMACVR1, tracer.trace_range_start); 
	cs_cpu_write(ptm_addr_base, ETMACVR2, tracer.trace_range_end); 
	/* TATR1&2 */
    /* Don't have to set in ETMv4 */
    /* Select ARC1 on resource select register 2 (0 / 1 has special meaning) */
    cs_cpu_write(ptm_addr_base, ETMRSCTLR2, RS_ARC_GROUP | RS_SELECT(0));
    /* Set ARC1 as Include in ETMVIIECTLR */
    cs_cpu_write(ptm_addr_base, ETMVIIECTLR, IN_SELECT(0));
    /* Set ETMVICTLR */
    // set SSSTATUS = 1 and EVENT select 1 (always true)
    cs_cpu_write(ptm_addr_base, ETMVICTLR, SS_STATUS_EN | EVENT_SELECT(1));

    config = 0;
    result = cs_cpu_read(ptm_addr_base, ETMIDR0);
    if(result & TSSIZE) {
#if ETB_TIMESTAMP
        /* Enable timestamp */
        config |= CONFIG_TS;
#endif
    }
    else {
        printk("addr @ %p doesn't support global timestamp\n", ptm_addr_base);
    }

    if(result & CCCI_SUPPORT) { 
#if ETB_CYCLE_ACCURATE
        /* Enable cycle accurate */
        /* Currently we don't use Cycle count */
        //config |= CONFIG_CCI;
#endif
    }

    /* Write config */
    cs_cpu_write(ptm_addr_base, ETMCONFIG, config); 

    /* set TraceID for each core */
    /* start with cpu 0 = 2, cpu1 = 4, ... */
    cs_cpu_write(ptm_addr_base, ETMTRID, core * 2 + 2); 

	/* Set up synchronization frequency */
    result = cs_cpu_read(ptm_addr_base, ETMIDR3);
    if(!(result & SYNCPR)) {
        // TRCSYNCPR is RW
        // Trace synchronization request every 256 bytes
        cs_cpu_write(ptm_addr_base, ETMSYNCPR, 0x8); 
    }
        pr_info("[ETM LOG] ETMACVR1 &0x%x=0x%x\n",(vmalloc_to_pfn(ptm_addr_base)<<12)+ETMACVR1,cs_cpu_read(ptm_addr_base,ETMACVR1));
        pr_info("[ETM LOG] ETMACVR2  &0x%x=0x%x\n",(vmalloc_to_pfn(ptm_addr_base)<<12)+ETMACVR2,cs_cpu_read(ptm_addr_base,ETMACVR2));
        pr_info("[ETM LOG] ETMRSCTLR2  &0x%x=0x%x\n",(vmalloc_to_pfn(ptm_addr_base)<<12)+ETMRSCTLR2,cs_cpu_read(ptm_addr_base,ETMRSCTLR2));
        pr_info("[ETM LOG] ETMVIIECTLR  &0x%x=0x%x\n",(vmalloc_to_pfn(ptm_addr_base)<<12)+ETMVIIECTLR,cs_cpu_read(ptm_addr_base,ETMVIIECTLR));
        pr_info("[ETM LOG] ETMVICTLR  &0x%x=0x%x\n",(vmalloc_to_pfn(ptm_addr_base)<<12)+ETMVICTLR,cs_cpu_read(ptm_addr_base,ETMVICTLR));
        pr_info("[ETM LOG] ETMIDR0  &0x%x=0x%x\n",(vmalloc_to_pfn(ptm_addr_base)<<12)+ETMIDR0,cs_cpu_read(ptm_addr_base,ETMIDR0));
        pr_info("[ETM LOG] ETMCONFIG  &0x%x=0x%x\n",(vmalloc_to_pfn(ptm_addr_base)<<12)+ETMCONFIG,cs_cpu_read(ptm_addr_base,ETMCONFIG));
        pr_info("[ETM LOG] ETMTRID  &0x%x=0x%x\n",(vmalloc_to_pfn(ptm_addr_base)<<12)+ETMTRID,cs_cpu_read(ptm_addr_base,ETMTRID));
        pr_info("[ETM LOG] ETMIDR3  &0x%x=0x%x\n",(vmalloc_to_pfn(ptm_addr_base)<<12)+ETMIDR3,cs_cpu_read(ptm_addr_base,ETMIDR3));
        pr_info("[ETM LOG] ETMSYNCPR  &0x%x=0x%x\n",(vmalloc_to_pfn(ptm_addr_base)<<12)+ETMSYNCPR,cs_cpu_read(ptm_addr_base,ETMSYNCPR));
        pr_info("[ETM LOG] %s Done\n",__func__);
	/* Init ETM */
    //cs_cpu_etm_enable(ptm_addr_base);
}

static void trace_start(void)
{
	int i;
	int pwr_down;
        pr_info("[ETM LOG] %s\n",__func__);
	if (tracer.state == TRACE_STATE_TRACING) {
		pr_info("ETM trace is already running\n");
		return;
	}

	get_online_cpus();

	mutex_lock(&tracer.mutex);
  
	/* AHBAP_EN to enable master port, then ETR could write the trace to bus */
	__raw_writel(DEM_UNLOCK_MAGIC, DEM_UNLOCK);
	mt_reg_sync_writel(AHB_EN, AHBAP_EN);
  
	etb_unlock(&tracer);

	//cs_cpu_unlock(tracer.tpiu_regs);
	cs_cpu_unlock(tracer.funnel_regs);
	//cs_cpu_unlock(tracer.etb_regs);
  
	cs_cpu_funnel_setup();
	cs_cpu_etb_setup();  

	for (i = 0; i < tracer.nr_etm_regs; i++) {
		if (tracer.etm_info[i].pwr_down == NULL) {
			pwr_down = 0;
		} else {
			pwr_down = *(tracer.etm_info[i].pwr_down);
		}
		if (!pwr_down && tracer.etm_info[i].enable) {
			cs_cpu_unlock(tracer.etm_regs[i]);
            cs_cpu_osunlock(tracer.etm_regs[i]);
            /* Power-up TMs */
            // do we need this? ETM core unit power domain is the same as CPU
            //cs_cpu_ptm_powerup(tracer.etm_regs[i]);

            /* Disable TMs so that they can be set up safely */
			cs_cpu_etm_disable(tracer.etm_regs[i]);
            
            /* Set up TMs */
			cs_cpu_etm_setup(tracer.etm_regs[i], i);

            /* update the ETMTSR and ETMCONFIG*/
			tracer.etm_info[i].trcidr0 = etm_readl(&tracer, i, ETMIDR0);
			tracer.etm_info[i].trcidr2 = etm_readl(&tracer, i, ETMIDR2);

            /* Set up CoreSightTraceID */
            //cs_cpu_write(tracer.etm_regs[i], 0x200, i + 1);

            /* Enable TMs now everything has been set up */
            cs_cpu_etm_enable(tracer.etm_regs[i]);
          
            //cs_cpu_ptm_clear_progbit(tracer.etm_regs[i]);
		}
	}

	/* Avoid DBG_sys being reset */
	__raw_writel(DEM_UNLOCK_MAGIC, DEM_UNLOCK);
    // ETB is reset by power-on, not watch-dog
	__raw_writel(POWER_ON_RESET, DBGRST_ALL);
	__raw_writel(BUSCLK_EN, DBGBUSCLK_EN);

         mt_reg_sync_writel(SYSCLK_EN, DBGSYSCLK_EN);
        pr_info("[ETM LOG] DEM_UNLOCK &0x%x=0x%x\n",((vmalloc_to_pfn(tracer.dem_regs)<<12) + 0xFB0),__raw_readl(DEM_UNLOCK));
        pr_info("[ETM LOG] DBGRST_ALL &0x%x=0x%x\n",((vmalloc_to_pfn(tracer.dem_regs)<<12) + 0x028),__raw_readl(DBGRST_ALL));
        pr_info("[ETM LOG] DBGBUSCLK_EN &0x%x=0x%x\n",((vmalloc_to_pfn(tracer.dem_regs)<<12) + 0x02c),__raw_readl(DBGBUSCLK_EN));
        pr_info("[ETM LOG] DBGSYSCLK_EN &0x%x=0x%x\n",((vmalloc_to_pfn(tracer.dem_regs)<<12) + 0x030),__raw_readl(DBGSYSCLK_EN));
	tracer.state = TRACE_STATE_TRACING;
  
	etb_lock(&tracer);
        pr_info("[ETM LOG] %s Done\n",__func__);
	mutex_unlock(&tracer.mutex);

	put_online_cpus();
}
  
static void trace_stop(void)
{ 
	int i;	 
	int pwr_down;
        
        pr_info("[ETM LOG] %s\n",__func__);
	if (tracer.state == TRACE_STATE_STOP) {
		pr_info("ETM trace is already stop!\n");
		return;
	}
  
	get_online_cpus();

	mutex_lock(&tracer.mutex);
  
	etb_unlock(&tracer);
  
	for (i = 0; i < tracer.nr_etm_regs; i++) {
		if (tracer.etm_info[i].pwr_down == NULL) {
			pwr_down = 0;
		} else {
			pwr_down = *(tracer.etm_info[i].pwr_down);
		}
		if (!pwr_down && tracer.etm_info[i].enable) {
            /* "Trace program done" */
            /* "Disable trace components" */
			cs_cpu_etm_disable(tracer.etm_regs[i]);

            /* power down */
            //cs_cpu_write(tracer.etm_regs[i], 0x0, 0x1);
		}
	}

#if 0
	cs_cpu_flushandstop(tracer.tpiu_regs);
#endif
  
	/* Disable ETB capture (ETB_CTL bit0 = 0x0) */
	cs_cpu_write(tracer.etb_regs, ETBCTL, 0x0);
	/* Reset ETB RAM Read Data Pointer (ETB_RRP = 0x0) */
	/* no need to reset RRP */
#if 0
	cs_cpu_write (tracer.etb_regs, 0x14, 0x0);
#endif
        pr_info("[ETM LOG] ETBCTL  &0x%x=0x%x\n",vmalloc_to_pfn(tracer.etb_regs)+ETBCTL,cs_cpu_read(tracer.etb_regs,ETBCTL));
	dsb();
        
	tracer.state = TRACE_STATE_STOP;

	etb_lock(&tracer);
        pr_info("[ETM LOG] %s Done\n",__func__);
	mutex_unlock(&tracer.mutex);

	put_online_cpus();
}

/* 
 * trace_start_by_cpus: Restart traces of the given CPUs.
 * @mask: cpu mask
 * @init_etb: a flag to re-initialize ETB, funnel, ... etc
 */
void trace_start_by_cpus(const struct cpumask *mask, int init_etb)
{
	int i;

	if (!mask) {
		return ;
	}
        pr_info("[ETM LOG] %s\n",__func__);
	if (init_etb) {
		/* enable master port such that ETR could write the trace to bus */
		mt_reg_sync_writel(AHB_EN, (volatile u32 *)AHBAP_EN);
        //cs_cpu_unlock(tracer.tpiu_regs);
        cs_cpu_unlock(tracer.funnel_regs);
        //cs_cpu_unlock(tracer.etb_regs);
		etb_unlock(&tracer);

        cs_cpu_funnel_setup();

		/* Disable ETB capture (ETB_CTL bit0 = 0x0) */
		/* For wdt reset */
		cs_cpu_write (tracer.etb_regs, ETBCTL, 0x0);

		if (tracer.use_etr) {
			/* Set up ETR memory buffer address */  
			etb_writel(&tracer, tracer.etr_phys, TMCDBALR);
			/* Set up ETR memory buffer size */
			etb_writel(&tracer, tracer.etr_len, TMCRSZ);
		}

		cs_cpu_etb_setup();
	}

	for (i = 0; i < tracer.nr_etm_regs; i++) {
		if (cpumask_test_cpu(i, mask) && tracer.etm_info[i].enable) {
			cs_cpu_unlock(tracer.etm_regs[i]);
            cs_cpu_osunlock(tracer.etm_regs[i]);
            /* Power-up PTMs */
			//cs_cpu_ptm_powerup(tracer.etm_regs[i]);

            /* Disable PTMs so that they can be set up safely */
			cs_cpu_etm_disable(tracer.etm_regs[i]);

            /* Set up PTMs */
			cs_cpu_etm_setup(tracer.etm_regs[i], i);

            /* Set up CoreSightTraceID */
			//cs_cpu_write(tracer.etm_regs[i], 0x200, i + 1);

            /* Enable PTMs now everything has been set up */
            cs_cpu_etm_enable(tracer.etm_regs[i]);
			//cs_cpu_ptm_clear_progbit(tracer.etm_regs[i]);
		}
	}
  
	if (init_etb) { 
		/* Avoid DBG_sys being reset */
		mt_reg_sync_writel(DEM_UNLOCK_MAGIC, (volatile u32 *)DEM_UNLOCK);
		mt_reg_sync_writel(POWER_ON_RESET, (volatile u32 *)DBGRST_ALL);
		mt_reg_sync_writel(BUSCLK_EN, (volatile u32 *)DBGBUSCLK_EN);
		mt_reg_sync_writel(SYSCLK_EN, (volatile u32 *)DBGSYSCLK_EN);

		etb_lock(&tracer);
	}
        pr_info("[ETM LOG] DEM_UNLOCK &0x%x=0x%x\n",((vmalloc_to_pfn(tracer.dem_regs)<<12) + 0xFB0),__raw_readl(DEM_UNLOCK));
        pr_info("[ETM LOG] DBGRST_ALL &0x%x=0x%x\n",((vmalloc_to_pfn(tracer.dem_regs)<<12) + 0x028),__raw_readl(DBGRST_ALL));
        pr_info("[ETM LOG] DBGBUSCLK_EN &0x%x=0x%x\n",((vmalloc_to_pfn(tracer.dem_regs)<<12) + 0x02c),__raw_readl(DBGBUSCLK_EN));
        pr_info("[ETM LOG] DBGSYSCLK_EN &0x%x=0x%x\n",((vmalloc_to_pfn(tracer.dem_regs)<<12) + 0x030),__raw_readl(DBGSYSCLK_EN));
        pr_info("[ETM LOG] %s Done\n",__func__);
}

static inline ssize_t run_show(struct device *kobj,
			struct device_attribute *attr, char *buf)
{
        pr_info("[ETM DEBUG] run_show show tracer.state 0x%x\n",tracer.state);
	return snprintf(buf, PAGE_SIZE, "%x\n", tracer.state);
}

static ssize_t run_store(struct device *kobj, struct device_attribute *attr,
			const char *buf, size_t n)
{
	unsigned int value;
    //char *data;
	if (unlikely(sscanf(buf, "%u", &value) != 1))
		return -EINVAL;
        pr_info("[ETM DEBUG] run_show store value 0x%x\n",value);
        if(value == 1 ||value == 0)
        {
          pr_info ("[ETM DEBUG] Let AEE Skip run ETM/stop ETM\n");
          return n;
        }
	if (value == 2) {
		trace_start();
        pr_info("[ETM DEBUG] start() return\n");
	} else if(value == 3) {
		trace_stop();
        pr_info("[ETM DEBUG] stop() return\n");
        //data = vmalloc(8192);
        //if(data) {
        //    etb_read_test(data, 8192, 0);
        //    vfree(data);
        //}
        //else {
        //    pr_info("%s can't read ETB data\n", __func__);
        //}
	} else {
		return -EINVAL;
	}

	return n;
}

DEVICE_ATTR(run, 0644, run_show, run_store);

#define TMCReady    (0x1 << 2)
static inline ssize_t etb_length_show(struct device *kobj, 
			struct device_attribute *attr, char *buf)
{
	int v, etb_legnth;
    pr_info("[ETM DEBUG] etb_length_show\n");
    v = etb_readl(&tracer, ETBSTS);
    if(v & TMCReady) {
        etb_legnth = etb_get_data_length(&tracer);
        return sprintf(buf, "%08x\n", etb_legnth);
    }
    else {
        return sprintf(buf, "Need to stop trace before get length\n");
    }
}

static ssize_t etb_length_store(struct device *kobj, struct device_attribute *attr, 
			const char *buf, size_t n)
{
        pr_info("[ETM DEBUG] etb_length_store\n");
	/* do nothing */
	return n;
}

DEVICE_ATTR(etb_length, 0644, etb_length_show, etb_length_store);

static inline ssize_t trace_data_show(struct device *kobj, 
			struct device_attribute *attr, char *buf)
{
         pr_info("[ETM DEBUG] trace_data_show\n");
	return sprintf(buf, "%08x\n", tracer.enable_data_trace);
}

static ssize_t trace_data_store(struct device *kobj, struct device_attribute *attr, 
			const char *buf, size_t n)
{
	unsigned int value;
         pr_info("[ETM DEBUG] trace_data_store\n");

	if (unlikely(sscanf(buf, "%u", &value) != 1))
		return -EINVAL;  

	if (tracer.state == TRACE_STATE_TRACING) {
		pr_err("ETM trace is running. Stop first before changing setting\n");
		return n;
	}

	mutex_lock(&tracer.mutex);

	if(value == 1) {	
		tracer.enable_data_trace = 1;
	} else {
		tracer.enable_data_trace = 0;
	}

	mutex_unlock(&tracer.mutex);
  
	return n;
}

DEVICE_ATTR(trace_data, 0644, trace_data_show, trace_data_store);

static ssize_t trace_range_show(struct device *kobj, 
			struct device_attribute *attr, char *buf)
{
        pr_info("[ETM DEBUG] trace_range_show\n");
	return sprintf(buf, "%08x %08x\n", tracer.trace_range_start, tracer.trace_range_end);
}

static ssize_t trace_range_store(struct device *kobj, struct device_attribute *attr, 
			const char *buf, size_t n)
{
       pr_info("[ETM DEBUG] trace_range_store\n");  
	unsigned int range_start, range_end;
  
	if (sscanf(buf, "%x %x", &range_start, &range_end) != 2)
		return -EINVAL;

	if (tracer.state == TRACE_STATE_TRACING) {
		pr_err("ETM trace is running. Stop first before changing setting\n");
		return n;
	}

	mutex_lock(&tracer.mutex);

	tracer.trace_range_start = range_start;
	tracer.trace_range_end = range_end;

	mutex_unlock(&tracer.mutex);

	return n;
}

DEVICE_ATTR(trace_range, 0644, trace_range_show, trace_range_store);

static ssize_t etm_online_show(struct device *kobj, 
			struct device_attribute *attr, char *buf)
{
	unsigned int i;
        pr_info("[ETM DEBUG] etm_online_show\n"); 
	for (i = 0; i < tracer.nr_etm_regs; i++ ) {
		sprintf(buf, "%sETM_%d is %s\n", buf, i, 
			(tracer.etm_info[i].enable)? "Enabled": "Disabled");
	}

	return strlen(buf);
}

static ssize_t etm_online_store(struct device *kobj, struct device_attribute *attr, 
			const char *buf, size_t n)
{
	unsigned int ret;
	unsigned int num = 0;
	unsigned char str[10];
        pr_info("[ETM DEBUG] etm_online_store\n");
	ret = sscanf(buf, "%s %d",str ,&num) ;
		
	if (tracer.state == TRACE_STATE_TRACING) {
		pr_err("ETM trace is running. Stop first before changing setting\n");
		return n;
	}

	mutex_lock(&tracer.mutex);

	if (!strncmp(str, "ENABLE", strlen("ENABLE"))) {
		tracer.etm_info[num].enable = 1;
	} else if(!strncmp(str, "DISABLE", strlen("DISABLE"))) {
		tracer.etm_info[num].enable = 0;
	} else {
		pr_err("Input is not correct\n");
	}

	mutex_unlock(&tracer.mutex);

	return n;
}

DEVICE_ATTR(etm_online, 0644, etm_online_show, etm_online_store);

static ssize_t nr_etm_show(struct device *kobj, 
			struct device_attribute *attr, char *buf)
{
        pr_info("[ETM DEBUG] nr_etm_show\n");
	return sprintf(buf, "%d\n", tracer.nr_etm_regs);
}

static ssize_t nr_etm_store(struct device *kobj, struct device_attribute *attr, 
			const char *buf, size_t n)
{
        pr_info("[ETM DEBUG] nr_etm_store\n");
	return n;
}

DEVICE_ATTR(nr_etm, 0644, nr_etm_show, nr_etm_store);

static ssize_t etm_tcr_show(struct device *kobj, 
			struct device_attribute *attr, char *buf)
{
        pr_info("[ETM DEBUG] etm_tcr_show\n");
	if (tracer.state == TRACE_STATE_TRACING) {
		return sprintf(buf, "ETM trace is running. Stop first before changing setting\n");
	}
	return sprintf(buf, "0x%08x\n", etm_readl(&tracer, tracer.etm_idx , ETMCONFIG ));
}

static ssize_t etm_tcr_store(struct device *kobj, struct device_attribute *attr, 
			const char *buf, size_t n)
{
        pr_info("[ETM DEBUG] etm_tcr_store\n");
	return n;
}

DEVICE_ATTR(etm_tcr, 0644, etm_tcr_show, etm_tcr_store);

static ssize_t etm_idr0_show(struct device *kobj, 
			struct device_attribute *attr, char *buf)
{
        pr_info("[ETM DEBUG] etm_idr0_show\n");
	return sprintf(buf, "0x%08x\n", tracer.etm_info[tracer.etm_idx].trcidr0);
}

static ssize_t etm_idr0_store(struct device *kobj, struct device_attribute *attr, 
			const char *buf, size_t n)
{
        pr_info("[ETM DEBUG] etm_idr0_store\n");
	return n;
}

DEVICE_ATTR(etm_idr0, 0644, etm_idr0_show, etm_idr0_store);

static ssize_t etm_idr2_show(struct device *kobj, 
			struct device_attribute *attr, char *buf)
{
         pr_info("[ETM DEBUG] etm_idr2_show\n");
	return sprintf(buf, "0x%08x\n", tracer.etm_info[tracer.etm_idx].trcidr2);
}

static ssize_t etm_idr2_store(struct device *kobj, struct device_attribute *attr, 
			const char *buf, size_t n)
{
         pr_info("[ETM DEBUG] etm_idr2_store\n");
	return n;
}

DEVICE_ATTR(etm_idr2, 0644, etm_idr2_show, etm_idr2_store);

static ssize_t etm_lock_show(struct device *kobj, 
            struct device_attribute *attr, char *buf)
{
    pr_info("[ETM DEBUG] etm_lock_show\n");
    return 0;
}

static ssize_t etm_lock_store(struct device *kobj, struct device_attribute *attr, 
            const char *buf, size_t n)
{
	unsigned int value;
    pr_info("[ETM DEBUG] etm_lock_store\n");
    int i;

	if (unlikely(sscanf(buf, "%u", &value) != 1))
		return -EINVAL;  

	if (tracer.state == TRACE_STATE_TRACING) {
		pr_err("ETM trace is running. Stop first before changing setting\n");
		return n;
	}

    if(value == 1) { // lock
        for (i = 0; i < tracer.nr_etm_regs; i++) {
            if (cpumask_test_cpu(i, cpu_online_mask) && tracer.etm_info[i].enable) {
                cs_cpu_lock(tracer.etm_regs[i]);
                cs_cpu_oslock(tracer.etm_regs[i]);
            }
        }
    }
    else if(value == 0) { // unlock
        for (i = 0; i < tracer.nr_etm_regs; i++) {
            if (cpumask_test_cpu(i, cpu_online_mask) && tracer.etm_info[i].enable) {
                cs_cpu_unlock(tracer.etm_regs[i]);
                cs_cpu_osunlock(tracer.etm_regs[i]);
            }
        }
    }
    return n;
}

DEVICE_ATTR(etm_lock, 0644, etm_lock_show, etm_lock_store);

static ssize_t is_ptm_show(struct device *kobj, 
			struct device_attribute *attr, char *buf)
{
        pr_info("[ETM DEBUG] is_ptm_show\n");
	return sprintf(buf, "%d\n", 
			(tracer.etm_info[tracer.etm_idx].is_ptm)? 1: 0);
}

static ssize_t is_ptm_store(struct device *kobj, struct device_attribute *attr, 
			const char *buf, size_t n)
{
        pr_info("[ETM DEBUG] is_ptm_store\n");
	return n;
}

DEVICE_ATTR(is_ptm, 0644, is_ptm_show, is_ptm_store);

static ssize_t index_show(struct device *kobj, 
			struct device_attribute *attr, char *buf)
{
        pr_info("[ETM DEBUG] index_show\n");
	return sprintf(buf, "%d\n", tracer.etm_idx);
}

static ssize_t index_store(struct device *kobj, struct device_attribute *attr, 
			const char *buf, size_t n)
{
	unsigned int value;
	int ret;
        pr_info("[ETM DEBUG] index_store\n");
	if (unlikely(sscanf(buf, "%u", &value) != 1))
		return -EINVAL;  

	mutex_lock(&tracer.mutex);

	if (value >= tracer.nr_etm_regs) {
		ret = -EINVAL;
	} else {
		tracer.etm_idx = value;
		ret = n;
	}

	mutex_unlock(&tracer.mutex);

	return ret;
}

DEVICE_ATTR(index, 0644, index_show, index_store);

static int create_files(void)
{
	int ret = device_create_file(etm_device.this_device, &dev_attr_run);
	if (unlikely(ret != 0))
		return ret;	
		
	ret = device_create_file(etm_device.this_device, &dev_attr_etb_length);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(etm_device.this_device, &dev_attr_trace_data);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(etm_device.this_device, &dev_attr_trace_range);
	if (unlikely(ret != 0))
		return ret;
 
	ret = device_create_file(etm_device.this_device, &dev_attr_etm_online);
	if (unlikely(ret != 0))
		return ret;
 
	ret = device_create_file(etm_device.this_device, &dev_attr_nr_etm);
	if (unlikely(ret != 0))
		return ret;
 
	ret = device_create_file(etm_device.this_device, &dev_attr_etm_tcr);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(etm_device.this_device, &dev_attr_etm_idr0);
	if (unlikely(ret != 0))
		return ret;

    ret = device_create_file(etm_device.this_device, &dev_attr_etm_idr2);
    if (unlikely(ret != 0))
        return ret;

    ret = device_create_file(etm_device.this_device, &dev_attr_etm_lock);
    if (unlikely(ret != 0))
        return ret;

	ret = device_create_file(etm_device.this_device, &dev_attr_is_ptm);
	if (unlikely(ret != 0))
		return ret;
 
	ret = device_create_file(etm_device.this_device, &dev_attr_index);
	if (unlikely(ret != 0))
		return ret;

	return 0;
}

static void remove_files(void)
{
	device_remove_file(etm_device.this_device, &dev_attr_run);

	device_remove_file(etm_device.this_device, &dev_attr_etb_length);

	device_remove_file(etm_device.this_device, &dev_attr_trace_data);

	device_remove_file(etm_device.this_device, &dev_attr_trace_range);

	device_remove_file(etm_device.this_device, &dev_attr_etm_online);
    
	device_remove_file(etm_device.this_device, &dev_attr_nr_etm);

	device_remove_file(etm_device.this_device, &dev_attr_etm_tcr);

	device_remove_file(etm_device.this_device, &dev_attr_etm_idr0);
    
	device_remove_file(etm_device.this_device, &dev_attr_etm_idr2);

	device_remove_file(etm_device.this_device, &dev_attr_etm_lock);
}

static int etm_probe(struct platform_device *pdev)
{
	int ret = 0, i;

    pr_info("etm_probe\n");

	mutex_lock(&tracer.mutex);

    of_property_read_u32(pdev->dev.of_node, "num", &tracer.nr_etm_regs);
    printk("get num = %d\n", tracer.nr_etm_regs);

    tracer.etm_regs = kmalloc(sizeof(void *) * tracer.nr_etm_regs, GFP_KERNEL);
	if (!tracer.etm_regs) {
		pr_err("Failed to allocate ETM register array\n");
		ret = -ENOMEM;
		goto out;
	}

    for(i = 0; i < tracer.nr_etm_regs; i++) {
        tracer.etm_regs[i] = of_iomap(pdev->dev.of_node, i);
        printk("etm %d @ 0x%p\n", i+1, tracer.etm_regs[i]);
    }

    tracer.etm_info = kmalloc(sizeof(struct etm_info) * tracer.nr_etm_regs, GFP_KERNEL); 
	if (!tracer.etm_info) {
		pr_err("Failed to allocate ETM info array\n");
		ret = -ENOMEM;
		goto out;
	}

    for(i = 0; i < tracer.nr_etm_regs; i++) {
        memset(&(tracer.etm_info[i]), 0, sizeof(struct etm_info));
        tracer.etm_info[i].enable = 1;
        tracer.etm_info[i].is_ptm = 0;
        tracer.etm_info[i].pwr_down = &(per_cpu(trace_pwr_down, i));
    }

    if (unlikely((ret = misc_register(&etm_device)) != 0)) {
        pr_err("Fail to register etm device\n");
        goto out;
    }
   if (unlikely((ret = create_files()) != 0)) {
        pr_err("Fail to create device files\n");
        goto deregister;
    }

out:
	mutex_unlock(&tracer.mutex);
	return ret;

deregister:
	misc_deregister(&etm_device);
	mutex_unlock(&tracer.mutex);
	return ret;
}

static const struct of_device_id etm_of_ids[] = {
    {   .compatible = "mediatek,DBG_ETM", },
    {}
};

static struct platform_driver etm_driver =
{
	.probe = etm_probe,
	.driver = {
		.name = "etm",
        .of_match_table = etm_of_ids,
	},
};

/* use either ETR_SRAM, ETR_DRAM, or ETB, if undefine just by IC version */
//#define ETR_SRAM
#define ETR_BUFF_SIZE 0x200
#define ETR_SRAM_PHYS_BASE (0x00100000 + 0xF800)
#define ETR_SRAM_VIRT_BASE (INTER_SRAM + 0xF800)

int etb_probe(struct platform_device *pdev)
{
    void __iomem *etb_base, *etr_base;

    pr_info("[ETM LOG] %s\n",__func__);
	mutex_lock(&tracer.mutex);

    etb_base = of_iomap(pdev->dev.of_node, 0);
    if(!etb_base) {
        printk("can't of_iomap for etb!!\n");
        return -ENOMEM;
    }
    else {
        printk("of_iomap for etb @ 0x%p\n", etb_base);
    }

    etr_base = of_iomap(pdev->dev.of_node, 1);
    if(!etr_base) {
        printk("can't of_iomap for etr!!\n");
        return -ENOMEM;
    }
    else {
        printk("of_iomap for etr @ 0x%p\n", etr_base);
    }

    tracer.funnel_regs = of_iomap(pdev->dev.of_node, 2);
    if(!tracer.funnel_regs) {
        printk("can't of_iomap for funnel!!\n");
        return -ENOMEM;
    }
    else {
        printk("of_iomap for funnel @ 0x%p\n", tracer.funnel_regs);
    }
    
    tracer.dem_regs = of_iomap(pdev->dev.of_node, 3);
    if(!tracer.dem_regs) {
        printk("can't of_iomap for dem!!\n");
        return -ENOMEM;
    }
    else {
        printk("of_iomap for dem @ 0x%p\n", tracer.dem_regs);
    }

    /*
       In GNU C, addition and subtraction operations are supported on pointers to void and on pointers to functions. 
       This is done by treating the size of a void or of a function as 1.

       A consequence of this is that sizeof is also allowed on void and on function types, and returns 1.

       The option -Wpointer-arith requests a warning if these extensions are used.
     */
	//tracer.tpiu_regs = IOMEM(etb_base + 0x13000),
#if defined(ETR_DRAM)
	/* DRAM */
	void *buff;
	dma_addr_t dma_handle;

	buff = dma_alloc_coherent(NULL, ETR_BUFF_SIZE * sizeof(int), &dma_handle, GFP_KERNEL);
	if (!buff) {
		return -ENOMEM;
	}
	tracer.etr_virt = (u32)buff;
	tracer.etr_phys = dma_handle;
	tracer.etr_len = ETR_BUFF_SIZE;
	tracer.use_etr = 1;
	tracer.etb_regs = IOMEM(etr_base);
#elif defined(ETR_SRAM)
	/* SRAM */
    void *buff;
    buff = ioremap(0x0010F800, ETR_BUFF_SIZE * 4);
	tracer.etr_virt = (u32)buff;
	tracer.etr_phys = (dma_addr_t)ETR_SRAM_PHYS_BASE;
	tracer.etr_len = ETR_BUFF_SIZE;
	tracer.use_etr = 1;
	tracer.etb_regs = IOMEM(etr_base);
#else
	/* ETB */
	tracer.use_etr = 0;
	tracer.etb_regs = IOMEM(etb_base);
#endif

	if (unlikely(misc_register(&etb_device) != 0)) {
		pr_err("Fail to register etb device\n");
	}

	/* AHBAP_EN to enable master port, then ETR could write the trace to bus */
	__raw_writel(DEM_UNLOCK_MAGIC, DEM_UNLOCK);
	mt_reg_sync_writel(AHB_EN, AHBAP_EN);

	etb_unlock(&tracer);

	/* Disable ETB capture (ETB_CTL bit0 = 0x0) */
	/* For wdt reset */
	cs_cpu_write(tracer.etb_regs, ETBCTL, 0x0);

	if (tracer.use_etr) {
		pr_info("ETR virt = 0x%x, phys = 0x%x\n", tracer.etr_virt, tracer.etr_phys);
		/* Set up ETR memory buffer address */  
		etb_writel(&tracer, tracer.etr_phys, TMCDBALR);
		/* Set up ETR memory buffer size */
        // etr_len is word-count, 1 means 4 bytes 
		etb_writel(&tracer, (tracer.etr_len), TMCRSZ);
	}


    // size is in 32-bit words
	tracer.etb_total_buf_size = etb_readl(&tracer, TMCRSZ);
	tracer.state = TRACE_STATE_STOP;

	mutex_unlock(&tracer.mutex);
        printk("[ETM LOG] ETBCTL &0x%x=0x%x\n",vmalloc_to_pfn(tracer.etb_regs)+ETBCTL,cs_cpu_read(tracer.etb_regs+ETBCTL,0x0));
        printk("[ETM LOG] %s Done\n",__func__);
	return 0;
}

static const struct of_device_id etb_of_ids[] = {
    {   .compatible = "mediatek,DBG_ETB", },
    {}
};

static struct platform_driver etb_driver =
{
	.probe = etb_probe,
	.driver = {
		.name = "etb",
        .of_match_table = etb_of_ids,
	},
};

void trace_start_dormant(void)
{
	int cpu;

    if(tracer.state == TRACE_STATE_TRACING) {
        trace_start_by_cpus(cpumask_of(0), 1);
    }

	/*
	 * XXX: This function is called just before entering the suspend mode.
	 *	  The Linux kernel is already freeze.
	 *	  So it is safe to do the trick to access the per-cpu variable directly.
	 */
	for (cpu = 1; cpu < num_possible_cpus(); cpu++) {
		per_cpu(trace_pwr_down, cpu) = 1;
	}
}

static int
restart_trace(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	unsigned int cpu;
	volatile int *pwr_down;

	cpu = (unsigned int)hcpu;
    unsigned int cpu_ix = 0;

	switch (action) {
	case CPU_STARTING:
		switch (cpu) {
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			pwr_down = &get_cpu_var(trace_pwr_down);
			if (*pwr_down) {
                if(tracer.state == TRACE_STATE_TRACING) {
                    trace_start_by_cpus(cpumask_of(cpu), 0);
                }
				*pwr_down = 0;
			}
			put_cpu_var(trace_pwr_down);
			break;

		default:
			break;
		}

		break;

	case CPU_DYING:
		switch (cpu) {
		case 4:
		case 5:
		case 6:
		case 7:
            if (num_possible_cpus() > 4)
            {
                if ((!cpu_online(4)) && (!cpu_online(5)) && (!cpu_online(6)) && (!cpu_online(7))) {
                    // num_possible_cpus could be 4, 6, 8
                    for (cpu_ix = 4; cpu_ix < num_possible_cpus(); cpu_ix++) {
                        per_cpu(trace_pwr_down, cpu_ix) = 1;
                    }
                }
            }

			break;

		default:
			break;
		}

		break;

	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata pftracer_notifier = {
	.notifier_call = restart_trace,
};

/**
 * driver initialization entry point
 */
static int __init etm_init(void)
{
	int i, err;

	memset(&tracer, 0,sizeof(struct etm_trace_context_t));
	mutex_init(&tracer.mutex);
	tracer.trace_range_start = TRACE_RANGE_START;
	tracer.trace_range_end = TRACE_RANGE_END;

	for (i = 0; i < num_possible_cpus(); i++) {
		per_cpu(trace_pwr_down, i) = 0;
		//etm_driver_data[i].pwr_down = &(per_cpu(trace_pwr_down, i));
	}

	register_cpu_notifier(&pftracer_notifier);

	err = platform_driver_register(&etm_driver);
	if (err) {
		return err;
	}

	err = platform_driver_register(&etb_driver);
	if (err) {
		return err;
	}

	return 0;
}

/**
 * driver exit point
 */
static void __exit etm_exit(void)
{
	kfree(tracer.etm_info);
	kfree(tracer.etm_regs);

	remove_files();

	if (misc_deregister(&etm_device)) {
		pr_err("Fail to deregister dervice\n");
	}
}

module_init(etm_init);
module_exit(etm_exit);
