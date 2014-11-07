/*
 * linux/drivers/video/odin/dss/crg.c
 *
 * Copyright (C) 2012 LG Electronics
 * Author: Jaeky Oh <jaeky.oh@lge.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define DSS_SUBSYS_NAME	"CRG"
/* #define MAILBOX_USECASE */
#define ODIN_DSS_UNDERRUN_RESTORE

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
#include <linux/export.h>
#endif
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>

#include <video/odindss.h>
#include "dss.h"

#include "du.h"
#include "../display/panel_device.h"
#include "../hdmi_api_tx/api/api.h"

#include "crg_reg_def.h"
#include "crg_union_reg.h"
#include "../dss/mipi-dsi.h"
#include <linux/odin_register.h>
#include <linux/odin_mailbox.h>
#include <linux/odin_pd.h>
#include <linux/odin_dfs.h>
#include <linux/platform_data/odin_tz.h>
#include "linux/ktime.h"
#include <asm/mcpm.h>
#include <linux/cpufreq.h>
#include <linux/kernel_stat.h>


/******************** CRG REG control  *******************************/
#define CRG_REG_MASK(idx) \
	(((1 << ((CRG_##idx##_START) - (CRG_##idx##_END) + 1)) - 1) \
	<< (CRG_##idx##_END))
#define CRG_REG_VAL(val, idx) \
	((val) << (CRG_##idx##_END))
#define CRG_REG_GET(val, idx) \
	(((val) & CRG_REG_MASK(idx)) >> (CRG_##idx##_END))
#define CRG_REG_MOD(orig, val, idx) \
    (((orig) & ~CRG_REG_MASK(idx)) | CRG_REG_VAL(val, idx))


#define CRG_MAX_NR_ISRS		8

#define VID_SRC_COUNTING
struct crg_isr_data {
	odin_crg_isr_t	isr;
	void			*arg;
	u32			mask;
	u32			sub_mask;
};

struct crg_err_mask {
	u32			mask;
	u32			sub_mask[MAIN_MAX_POS];
};

struct crg_irq_stats {
	unsigned long last_reset;
	unsigned irq_count;
	unsigned irqs[32];
};

#ifdef DSS_UNDERRUN_LOG
struct pipe_info {
	u32 src_width;
	u32 src_height;
	u32 out_width;
	u32 out_height;
	u32 ovl_src_pos;
	u32 ovl_src_size;
	u32 read_status;
};

struct {
	u32 enabled_pipe;
	s64 time_stamp;
	u32 core_clk_status;
	int update_status;
	int dsi_state;
	u32 dip_fifo_cnt;
	u32 mipi_fifo_cnt;
	u32 ovl_src_sel;
	u32 ovl_src_en;
	u32 request_qos;
	struct pipe_info dma_pipe[MAX_OVERLAYS];
} underrun_info;
#endif

struct {
	struct platform_device	*pdev;
	u32 base;
	void __iomem	*iobase;
	int irq;
	struct completion completion;
	spinlock_t irq_lock;
	spinlock_t err_lock;
	spinlock_t clk_setting_lock;
	spinlock_t tz_lock;
	struct crg_isr_data registered_isr[CRG_MAX_NR_ISRS];
	struct work_struct error_work;
	struct workqueue_struct *error_work_queue;
	struct crg_err_mask err_enable;
	struct crg_err_mask err_status;
	u32 err_reset_mask[NUM_DIPC_BLOCK];
	u32 err_plane_mask[NUM_DIPC_BLOCK];
	u32 err_clk_mask[NUM_DIPC_BLOCK];
	u32 request_qos;
	enum odin_channel err_channel;
	int fifo_flush_cnt[NUM_DIPC_BLOCK];
	int fifo_flush_ok[NUM_DIPC_BLOCK];
	u32 scaler_on_mask;

	struct clk	*disp0_clk;
	struct clk	*disp1_clk;
	struct clk	*hdmi_disp_clk;

} crg_dev;

static inline void crg_write_tzreg(u32 addr, u32 val)
{
	u32 flags;
	spin_lock_irqsave(&crg_dev.tz_lock, flags);
#ifdef CONFIG_ODIN_TEE
	tz_write(val, crg_dev.base + addr);
#else
	__raw_writel(val, crg_dev.iobase + addr);
#endif
	spin_unlock_irqrestore(&crg_dev.tz_lock, flags);
}

static inline u32 crg_read_tzreg(u32 addr)
{
	u32 flags, r;
	spin_lock_irqsave(&crg_dev.tz_lock, flags);
#ifdef CONFIG_ODIN_TEE
	r =  tz_read(crg_dev.base + addr);
#else
	r = __raw_readl(crg_dev.iobase + addr);
#endif
	spin_unlock_irqrestore(&crg_dev.tz_lock, flags);

	return r;
}

static inline void crg_write_nsreg(u32 addr, u32 val)
{
	__raw_writel(val, crg_dev.iobase + addr);
}

static inline u32 crg_read_nsreg(u32 addr)
{
	return __raw_readl(crg_dev.iobase + addr);
}


static void _crg_set_interrupt_mask(u32 reg_val)
{
	crg_write_nsreg(ADDR_CRG_NON_SECURE_INTERRUPT_MASK_REGISTER, reg_val);
}

static u32 _crg_get_interrupt_mask(void)
{
	u32 val;
	val = crg_read_nsreg(ADDR_CRG_NON_SECURE_INTERRUPT_MASK_REGISTER);
	return val;
}

static u32 _crg_get_interrupt_status(void)
{
	u32 val;
	val = crg_read_nsreg(ADDR_CRG_NON_SECURE_INTERRUPT_PENDING_REGISTER);
	return val;
}

/* NON_SECURE_INTERRUPT_MASK_REGISTER */
static void _crg_set_interrupt_clear(u32 reg_val)
{
	crg_write_nsreg(ADDR_CRG_NON_SECURE_INTERRUPT_PENDING_REGISTER,
			reg_val);
}

int crg_runtime_get(void)
{
	return 0;
}

void crg_runtime_put(void)
{
	return;
}

void crg_set_irq_mask_all(u8 enable)
{
	if (enable)
		_crg_set_interrupt_mask(0xffffffff);
	else
		_crg_set_interrupt_mask(0x00000000);
}

/*when sub_mask is setted, mask is only one-bit  setted*/
int crg_irq_mask_valid_check(u32 mask, u32 sub_mask)
{
	int bit_pos;
	if (!(mask & CRG_IRQ_MAIN_ALL))
	{
		DSSERR("CRG_IRQ_MAIN_ALL err mask = %x\n", mask);
		return -EINVAL;
	}

	if (sub_mask)
	{
		DSSDBG("sub_mask is existed \n");
		if (!((mask & (CRG_IRQ_DIPS | CRG_IRQ_MIPIS))))
			return -EINVAL;

		bit_pos = find_first_bit((const unsigned long *)&mask, 32);
		clear_bit(bit_pos, (unsigned long *)&mask);

		bit_pos = find_first_bit((const unsigned long *)&mask, 32);

		if (bit_pos < 32)
			return -EINVAL;
	}

	return 0;
}

void crg_set_irq_enable(u32 mask, u32 sub_mask)
{
	u32 irqenable;
	u32 sub_irqenable = 0;
	u32 dip_num = 0;
	u32 dips_mask;
	u32 mipi_num = 0;
	u32 mipis_mask;
	u32 reg_val;

	reg_val = _crg_get_interrupt_mask();
	/* DSSDBG("crg_set_irq_enable read_val %x \n", reg_val); */

	irqenable = (CRG_IRQ_MAIN_ALL & _crg_get_interrupt_mask());
	/* DSSDBG("crg_set_irq_enable %x \n", irqenable) ;*/

	/* DIP Case */
	dips_mask = mask & CRG_IRQ_DIPS;
	if (dips_mask)
	{
		dip_num = (dips_mask >> (MAIN_DIP0_POS));
		switch (dip_num)
		{
			case 1:
				dip_num = 0;
				break;
			case 4:
				dip_num = 1;
				break;
			case 16:
				dip_num = 2;
				break;
			default:
				DSSERR("dip_num is invalid value\n");
				return;
		}
		sub_irqenable = CRG_IRQ_DIP_ALL & ~(dipc_get_irq_mask(dip_num));
	/* DSSDBG("crg_set_irq_enable: sub_irqenable = %x", sub_irqenable); */
	}

	/* MIPI Case */
	mipis_mask = mask & CRG_IRQ_MIPIS;
	if (mipis_mask)
	{
		mipi_num = (mipis_mask >> (MAIN_MIPI0_POS)) >> 2;
		sub_irqenable = CRG_IRQ_MIPI_ALL &
				~(odin_mipi_dsi_get_irq_mask(mipi_num));
		/* DSSDBG("crg_set_irq_enable: sub_irqenable = %x", sub_irqenable); */
	}

	/* clear the irqstatus for newly enabled irqs */
	if ((sub_mask) && (sub_mask & (sub_mask ^ sub_irqenable)))
	{
		if (dips_mask)
		{
			dipc_clear_irq(dip_num, sub_mask);
			dipc_mask_irq(dip_num, ~sub_irqenable & ~sub_mask);
			/* DSSDBG("crg_set_irq_enable: dipc_clear_irq : %x", sub_mask); */
		}

		if (mipis_mask)
		{
			odin_mipi_dsi_irq_clear(mipi_num, sub_mask);
			odin_mipi_dsi_irq_mask(mipi_num,
					       ~sub_irqenable & ~sub_mask);
			/* DSSDBG("crg_set_irq_enable: mipi_clear_irq : %x \n", sub_mask); */
		}
	}

	/* clear the irqstatus for newly enabled irqs */
	if (!(mask & irqenable))
	{
		_crg_set_interrupt_clear(mask);
	}

	_crg_set_interrupt_mask(irqenable | mask);

}

void crg_set_irq_disable(u32 mask, u32 sub_mask)
{
	int i;
	u32 irqenable, sub_irqenable;
	u32 dip_num = 0;
	u32 dips_mask;
	u32 mipi_num = 0;
	u32 mipis_mask;
	struct crg_isr_data *isr_data;
	bool mask_exist = false;
	bool sub_mask_exist = false;

	irqenable = CRG_IRQ_MAIN_ALL & _crg_get_interrupt_mask();

	/* DIP Case */
	dips_mask = mask & CRG_IRQ_DIPS;
	if (dips_mask)
	{

		switch (dips_mask)
		{
			case CRG_IRQ_DIP0:
				dip_num = 0;
				break;

			case CRG_IRQ_DIP1:
				dip_num = 1;
				break;

			case CRG_IRQ_DIP2:
				dip_num = 2;
				break;
			default:
				DSSERR("dips_mask is invalid value\n");
				return;
		}
		sub_irqenable = CRG_IRQ_DIP_ALL & ~(dipc_get_irq_mask(dip_num));
	}

	/* MIPI Case */
	mipis_mask = mask & CRG_IRQ_MIPIS;
	if (mipis_mask)
	{

		switch (mipis_mask)
		{
			case CRG_IRQ_MIPI0:
				mipi_num = 0;
				break;

			case CRG_IRQ_MIPI1:
				mipi_num = 1;
				break;
			default:
				DSSERR("mipis_mask is invalid value\n");
				return;
		}
		sub_irqenable = CRG_IRQ_MIPI_ALL &
				~(odin_mipi_dsi_get_irq_mask(mipi_num));
	}

	for (i = 0; i < CRG_MAX_NR_ISRS; i++) {
		isr_data = &crg_dev.registered_isr[i];

		if (isr_data->isr == NULL)
			continue;

		if (mask & isr_data->mask)
		{
			mask_exist = true;
			if (sub_mask & isr_data->sub_mask)
				sub_mask_exist = true;
		}
	}

	if (mask & crg_dev.err_enable.mask)
		mask_exist = true;

	if ((!mask_exist))
	{
		_crg_set_interrupt_mask(irqenable & ~mask);
	}

	if (sub_mask && (!sub_mask_exist))
	{
		/*DIP Case*/
		if (dips_mask)
		{
			dipc_mask_irq(dip_num, ~sub_irqenable | sub_mask);
			dipc_clear_irq(dip_num, sub_mask);
			/* DSSDBG("crg_set_irq_disable: dipc_clear_irq : %x", sub_mask); */
		}

		/*MIPI Case*/
		if (mipis_mask)
		{
			odin_mipi_dsi_irq_mask(mipi_num, ~sub_irqenable |
					       sub_mask);
			odin_mipi_dsi_irq_clear(mipi_num, sub_mask);
			/* DSSDBG("crg_set_irq_disable: dipc_clear_irq : %x", sub_mask); */
		}

	}

	if ((!mask_exist))
	{
		_crg_set_interrupt_clear(mask);
	}

}

void odin_crg_set_channel_int_mask(struct odin_dss_device *dssdev, bool enable)
{
	u32 mask = 0;
	bool manually_update_mode =	dssdev_manually_updated(dssdev);

	if (enable)
	{
		switch (dssdev->channel) {
		case ODIN_DSS_CHANNEL_LCD0:
			if (manually_update_mode)
				mask = (CRG_IRQ_MIPI0 | CRG_IRQ_DIP0 |
					CRG_IRQ_CABC0);
			else
				mask = (CRG_IRQ_VSYNC_MIPI0 | CRG_IRQ_MIPI0 |
					CRG_IRQ_DIP0 | CRG_IRQ_CABC0);
			break;

		case ODIN_DSS_CHANNEL_LCD1:
			if (manually_update_mode)
				mask = (CRG_IRQ_MIPI1 | CRG_IRQ_DIP1 |
					CRG_IRQ_CABC1);
			else
				mask = (CRG_IRQ_VSYNC_MIPI1 | CRG_IRQ_MIPI1 |
					CRG_IRQ_DIP1 | CRG_IRQ_CABC1);
			break;

		case ODIN_DSS_CHANNEL_HDMI:
				mask = (CRG_IRQ_VSYNC_HDMI | CRG_IRQ_DIP2 |
					CRG_IRQ_HDMI | CRG_IRQ_HDMI_WAKEUP);
		break;
		default:
			break;
		}
 	}
	else
	{
		switch (dssdev->channel) {
		case ODIN_DSS_CHANNEL_LCD0:
			if (manually_update_mode)
				mask = (CRG_IRQ_MIPI0 | CRG_IRQ_DIP0 |
					CRG_IRQ_CABC0);
			else
				mask = (CRG_IRQ_VSYNC_MIPI0 | CRG_IRQ_MIPI0 |
					CRG_IRQ_DIP0 | CRG_IRQ_CABC0);
			break;

		case ODIN_DSS_CHANNEL_LCD1:
			if (manually_update_mode)
				mask = (CRG_IRQ_MIPI1 | CRG_IRQ_DIP1 |
					CRG_IRQ_CABC1);
			else
				mask = (CRG_IRQ_VSYNC_MIPI1 | CRG_IRQ_MIPI1 |
					CRG_IRQ_DIP1 | CRG_IRQ_CABC1);
			break;

		case ODIN_DSS_CHANNEL_HDMI:
				mask = (CRG_IRQ_VSYNC_HDMI | CRG_IRQ_DIP2 |
					CRG_IRQ_HDMI | CRG_IRQ_HDMI_WAKEUP);
		break;
		default:
			break;
		}
  	}

	odin_crg_set_interrupt_mask(mask, enable);

	return;
}

void odin_crg_set_interrupt_mask(u32 mask, bool enable)
{
	u32 read_mask;
	unsigned long flags;

	spin_lock_irqsave(&crg_dev.irq_lock, flags);
	read_mask = _crg_get_interrupt_mask();
	if (enable)
		_crg_set_interrupt_mask(read_mask | mask);
	else
		_crg_set_interrupt_mask(read_mask & (~mask));
	spin_unlock_irqrestore(&crg_dev.irq_lock, flags);
}

void odin_crg_set_interrupt_clear(u32 mask)
{
	_crg_set_interrupt_clear(mask);
}

void odin_crg_set_clock_div(int ch, int val)
{
	u32 reg;
	reg = crg_read_tzreg(ADDR_CRG_CLOCK_DIVIDE_VALUE_0);
	reg &= ~(0xFF<<(8*ch));
	reg |= (val << (8*ch));
	crg_write_tzreg(ADDR_CRG_CLOCK_DIVIDE_VALUE_0, reg);
}

void odin_crg_clk_update(int i)
{
	int reg;
	reg = crg_read_tzreg(ADDR_CRG_CLOCK_DIVIDE_VALUE_UPDATE);
	reg &= ~(1 << i);
	crg_write_tzreg(ADDR_CRG_CLOCK_DIVIDE_VALUE_UPDATE, reg);
	mdelay(100);
	reg |= 1 << i;
	crg_write_tzreg(ADDR_CRG_CLOCK_DIVIDE_VALUE_UPDATE, reg);
	mdelay(100);
	reg &= ~(1 << i);
	crg_write_tzreg(ADDR_CRG_CLOCK_DIVIDE_VALUE_UPDATE, reg);
}


int odin_crg_register_isr(odin_crg_isr_t isr, void *arg,
	u32 mask, u32 sub_mask)
{
	int i;
	int ret;
	unsigned long flags;
	struct crg_isr_data *isr_data;

	if (isr == NULL)
	{
		DSSERR("isr is NULL \n");
		return -EINVAL;
	}

#if 0
	ret = crg_irq_mask_valid_check(mask, sub_mask);

	if (ret)
	{
		DSSERR("crg_irq_mask is not valid %d \n", ret);
		return -EINVAL;
	}
#endif

	spin_lock_irqsave(&crg_dev.irq_lock, flags);

	/* check for crgplicate entry */
	for (i = 0; i < CRG_MAX_NR_ISRS; i++) {
		isr_data = &crg_dev.registered_isr[i];
		if (isr_data->isr == isr && isr_data->arg == arg &&
				isr_data->mask == mask &&
				isr_data->sub_mask == sub_mask) {
			ret = -EINVAL;
			DSSINFO("register_isr is existed mask = %x, submask = %x\n",
				mask, sub_mask);
			goto err;
		}
	}

	isr_data = NULL;
	ret = -EBUSY;

	for (i = 0; i < CRG_MAX_NR_ISRS; i++) {
		isr_data = &crg_dev.registered_isr[i];

		if (isr_data->isr != NULL)
			continue;

		isr_data->isr = isr;
		isr_data->arg = arg;
		isr_data->mask = mask;
		isr_data->sub_mask = sub_mask;
		ret = 0;

		break;
	}

	if (ret)
		goto err;

	crg_set_irq_enable(mask, sub_mask);

	spin_unlock_irqrestore(&crg_dev.irq_lock, flags);

	return 0;
err:
	spin_unlock_irqrestore(&crg_dev.irq_lock, flags);
	DSSINFO("odin_crg_register_isr failed \n");
	return ret;
}
EXPORT_SYMBOL(odin_crg_register_isr);

int odin_crg_unregister_isr(odin_crg_isr_t isr, void *arg,
	u32 mask, u32 sub_mask)
{
	int i;
	unsigned long flags;
	int ret = -EINVAL;
	struct crg_isr_data *isr_data;

	spin_lock_irqsave(&crg_dev.irq_lock, flags);

	for (i = 0; i < CRG_MAX_NR_ISRS; i++) {
		isr_data = &crg_dev.registered_isr[i];
		if (isr_data->isr != isr || isr_data->arg != arg ||
				isr_data->mask != mask ||
				isr_data->sub_mask != sub_mask)
			continue;

		isr_data->isr = NULL;
		isr_data->arg = NULL;
		isr_data->mask = 0;
		isr_data->sub_mask = 0;

		ret = 0;
		break;
	}

	if (ret == 0)
		crg_set_irq_disable(mask, sub_mask);

	spin_unlock_irqrestore(&crg_dev.irq_lock, flags);

	return ret;
}
EXPORT_SYMBOL(odin_crg_unregister_isr);

int odin_crg_wait_for_irq_interruptible_timeout(u32 irqmask, u32 sub_irqmask,
		unsigned long timeout)
{
	void crg_irq_wait_handler(void *data, u32 mask, u32 sub_mask)
	{
		enum odin_channel channel;
		bool mipi_state;

		if ((mask & (CRG_IRQ_MIPI0 | CRG_IRQ_MIPI1)) &&
			(sub_mask & CRG_IRQ_MIPI_HS_DATA_COMPLETE))
		{
			if (mask & CRG_IRQ_MIPI1)
				channel = ODIN_DSS_CHANNEL_LCD1;
			else
				channel = ODIN_DSS_CHANNEL_LCD0;

			mipi_state = odin_mipi_cmd_loop_get(channel);

			if ((mipi_state) &&
				(sub_mask == CRG_IRQ_MIPI_HS_DATA_COMPLETE))
			{
				if (mipi_state)
				complete((struct completion *)data);

			}
		}
		else
		{
			complete((struct completion *)data);
		}
	}

	int r;
	DECLARE_COMPLETION_ONSTACK(completion);

	r = crg_runtime_get();
	if (r)
		return r;

	r = odin_crg_register_isr(crg_irq_wait_handler, &completion,
			irqmask, sub_irqmask);

	if (r)
		goto done;

	timeout = wait_for_completion_interruptible_timeout(&completion,
			timeout);

	odin_crg_unregister_isr(crg_irq_wait_handler, &completion,
		irqmask, sub_irqmask);

	if (timeout == 0)
		r = -ETIMEDOUT;
	else  if (timeout == -ERESTARTSYS)
		r = timeout;

done:
	crg_runtime_put();

	return r;
}


int crg_set_err_mask(u32 mask, u32 sub_mask, bool enable)
{
	int ret;
	unsigned long flags;

#if 0
	DSSDBG("crg_set_err_mask : mask = 0x%x, sub_mask = 0x%x, enable = 0x%x \n"
		, mask, sub_mask, enable);
#endif

	ret = crg_irq_mask_valid_check(mask, sub_mask);
	if (ret)
	{
		DSSERR("crg_irq_err_mask is not valid %d \n", ret);
		return -EINVAL;
	}

	spin_lock_irqsave(&crg_dev.irq_lock, flags);

	if (enable)
	{
		crg_dev.err_enable.mask |= mask;
		crg_dev.err_enable.sub_mask[fls(mask)-1] = sub_mask;
		crg_set_irq_enable(mask, sub_mask);
	}
	else
	{
		crg_dev.err_enable.mask &= ~mask;
		crg_dev.err_enable.sub_mask[fls(mask)-1] = 0;
		crg_set_irq_disable(mask, sub_mask);
	}

	spin_unlock_irqrestore(&crg_dev.irq_lock, flags);

	return 0;
}

int odin_crg_set_err_mask(struct odin_dss_device *dss_dev, bool enable)
{
	enum odin_channel channel;
	channel = dss_dev->channel;

	switch (channel) {
	case ODIN_DSS_CHANNEL_LCD0:
#if 0
	if (dss_dev->type == ODIN_DISPLAY_TYPE_RGB)
		crg_set_err_mask(CRG_IRQ_DIP0,
			CRG_IRQ_DIP_FIFO1_ACCESS_ERR, enable);
	else if (dss_dev->type == ODIN_DISPLAY_TYPE_DSI)
		crg_set_err_mask(CRG_IRQ_MIPI0,
			CRG_IRQ_MIPI_TXDATA_FIFO_URERR, enable);
#endif
	break;
	case ODIN_DSS_CHANNEL_LCD1:
		crg_set_err_mask(CRG_IRQ_MIPI1,
			CRG_IRQ_MIPI_TXDATA_FIFO_URERR, enable);
	break;
 	case ODIN_DSS_CHANNEL_HDMI:
		crg_set_err_mask(CRG_IRQ_DIP2,
			CRG_IRQ_DIP_FIFO1_ACCESS_ERR, enable);
	break;
 	default:
		break;
	}

	return 0;
}

#ifdef MAILBOX_USECASE
int odin_crg_dss_clk_ena(enum odin_crg_clk_gate clk_gate,
	enum odin_crg_core_clk clk_gate_val, bool enable)
{
	u32 base_addr = CRG_BASE_ADDR;
	u32 write_val = 0;
	u32 read_val = 0;
	unsigned long flags;

	if (clk_gate == CRG_CORE_CLK)
		base_addr += ADDR_CRG_CORE_CLOCK_ENABLE;
	else if (clk_gate == CRG_OTHER_CLK)
		base_addr += ADDR_CRG_OTHER_CLOCK_CONTROL;
	else
		return -EINVAL;

	spin_lock_irqsave(&crg_dev.clk_setting_lock, flags);
	read_val = odin_register_read(base_addr);

	write_val = read_val;

	if (enable)
		write_val |= clk_gate_val;
	else
		write_val &= ~(clk_gate_val);

	if (read_val == write_val)
		return 0;

#if 0
	DSSDBG("START: odin_crg_dss_clk_ena : write val = %x, write addr = %x \n",
		write_val, base_addr);
#endif
	odin_register_write(base_addr, write_val);
#if 0
	DSSDBG("END: odin_crg_dss_clk_ena : write val = %x, write addr = %x \n",
		write_val, base_addr);
#endif
	spin_unlock_irqrestore(&crg_dev.clk_setting_lock, flags);
	return 0;
}

int odin_crg_dss_mem_sleep_in(enum odin_crg_mem_deep_sleep mem_sleep,
	bool enable)
{
	u32 base_addr = CRG_BASE_ADDR;
	u32 write_val = 0;
	u32 read_val = 0;

	base_addr += ADDR_CRG_OTHER_CLOCK_CONTROL;

	/* DSSDBG("START: odin_crg_dss_mem_sleep_in : read val = %x \n ", read_val); */
	read_val = odin_register_read(base_addr);
	/* DSSDBG("END:  odin_crg_dss_mem_sleep_in : read val = %x \n ", read_val); */

	write_val = read_val;

	if (enable)
		write_val |= mem_sleep;
	else
		write_val &= ~(mem_sleep);

	if (read_val == write_val)
		return 0;

#if 0
	DSSDBG("START: odin_crg_dss_mem_sleep_in : \
		write val = %x, write addr = %x \n", write_val, base_addr);
#endif
	odin_register_write(base_addr, write_val);
#if 0
	DSSDBG("END: odin_crg_dss_mem_sleep_in : \
		write val = %x, write addr = %x \n", write_val, base_addr);
#endif

	return 0;
}
#else

#ifdef DSS_PD_CRG_DUMP_LOG
int odin_crg_get_register(u32 offset, bool secure)
{
	u32 base_addr = 0;
	u32 read_val = 0;
	unsigned long flags;

	spin_lock_irqsave(&crg_dev.clk_setting_lock, flags);
	if (secure)
		read_val = crg_read_tzreg(offset);
	else
		read_val =  __raw_readl(crg_dev.iobase + offset);
	spin_unlock_irqrestore(&crg_dev.clk_setting_lock, flags);

	return read_val;
}
#endif

int odin_crg_get_dss_clk_ena(enum odin_crg_clk_gate clk_gate)
{
	u32 base_addr = 0; /* CRG_BASE_ADDR; */
	u32 read_val = 0;
	unsigned long flags;

	if (clk_gate == CRG_CORE_CLK)
		base_addr = ADDR_CRG_CORE_CLOCK_ENABLE;
	else if (clk_gate == CRG_OTHER_CLK)
		base_addr = ADDR_CRG_OTHER_CLOCK_CONTROL;
	else
		return -EINVAL;

	spin_lock_irqsave(&crg_dev.clk_setting_lock, flags);
	read_val = crg_read_tzreg(base_addr);
	spin_unlock_irqrestore(&crg_dev.clk_setting_lock, flags);

	return read_val;
}

int odin_crg_dss_clk_ena(enum odin_crg_clk_gate clk_gate, u32 clk_gate_val,
	bool enable)
{
	u32 base_addr = 0; /* CRG_BASE_ADDR; */
	u32 write_val = 0;
	u32 read_val = 0;
	unsigned long flags;

	if (clk_gate == CRG_CORE_CLK)
		base_addr = ADDR_CRG_CORE_CLOCK_ENABLE;
	else if (clk_gate == CRG_OTHER_CLK)
		base_addr = ADDR_CRG_OTHER_CLOCK_CONTROL;
	else
		return -EINVAL;

	spin_lock_irqsave(&crg_dev.clk_setting_lock, flags);
	read_val = crg_read_tzreg(base_addr);
	write_val = read_val;

	if (enable)
			write_val |= clk_gate_val;
	else
			write_val &= ~(clk_gate_val);

	crg_write_tzreg(base_addr, write_val);
	spin_unlock_irqrestore(&crg_dev.clk_setting_lock, flags);
	return 0;
}
EXPORT_SYMBOL(odin_crg_dss_clk_ena);

int odin_crg_dss_pll_ena(struct clk *pll_clk, bool enable)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&crg_dev.clk_setting_lock, flags);

	if (enable)
	{
		ret = clk_enable(pll_clk);
		if (ret)
			DSSERR("clk_enable return:%d\n", ret);
	}
	else
	{
		if (pll_clk->enable_count)
			clk_disable(pll_clk);
	}
	spin_unlock_irqrestore(&crg_dev.clk_setting_lock, flags);

	return ret;
}

int odin_crg_dss_mem_sleep_in(enum odin_crg_mem_deep_sleep mem_sleep,
	bool enable)
{
	u32 base_addr = 0; /* CRG_BASE_ADDR;*/
	u32 write_val = 0;
	u32 read_val = 0;

	base_addr = ADDR_CRG_MEMORY_DEEP_SLEEP_MODE_CONTROL;
	read_val = crg_read_tzreg(base_addr);
	write_val = read_val;

	if (enable)
			write_val |= mem_sleep;
	else
			write_val &= ~(mem_sleep);

	crg_write_tzreg(base_addr, write_val);

	return 0;
}
#endif

int odin_crg_dss_channel_clk_enable(enum odin_channel channel, bool enable)
{
	int ret;

	if (enable)
	{
		switch (channel) {
		case ODIN_DSS_CHANNEL_LCD0:
			clk_prepare(crg_dev.disp0_clk);
			ret = odin_crg_dss_pll_ena(crg_dev.disp0_clk, true);
			if (ret)
				return ret;

			odin_crg_dss_clk_ena(CRG_CORE_CLK, (DIP0_CLK |
					     MIPI0_CLK), true);
			odin_crg_dss_clk_ena(CRG_OTHER_CLK,
					    (/*DISP0_CLK |*/ DPHY0_OSC_CLK |
					     TX_CLK_ESC0), true);
			odin_crg_dss_mem_sleep_in(DIP0_SLEEP ,false);
			break;
		case ODIN_DSS_CHANNEL_LCD1:
			clk_prepare(crg_dev.disp1_clk);
			ret = odin_crg_dss_pll_ena(crg_dev.disp1_clk, true);
			if (ret)
				return ret;

			odin_crg_dss_clk_ena(CRG_CORE_CLK, (DIP1_CLK |
					     MIPI1_CLK), true);
			odin_crg_dss_clk_ena(CRG_OTHER_CLK,
					    (/*DISP1_CLK | */DPHY1_OSC_CLK |
					     TX_CLK_ESC1), true);
			odin_crg_dss_mem_sleep_in(DIP1_SLEEP ,false);
		break;
	 	case ODIN_DSS_CHANNEL_HDMI:
			/* hdmi channel needs both disp0_clk & hdmi_disp_clk */
			clk_prepare(crg_dev.disp0_clk);
			ret = odin_crg_dss_pll_ena(crg_dev.disp0_clk, true);

			clk_prepare(crg_dev.hdmi_disp_clk);
			ret = odin_crg_dss_pll_ena(crg_dev.hdmi_disp_clk, true);
#if 0
			odin_crg_dss_clk_ena(CRG_CORE_CLK,
					    (DIP2_CLK | HDMI_CLK), true);
			/* odin_crg_dss_clk_ena(CRG_OTHER_CLK, HDMI_DISP_CLK , true); */
#else /* for hdmi */
			odin_crg_dss_clk_ena(CRG_CORE_CLK,
					    (MXD_CLK | DIP2_CLK | HDMI_CLK
				| OVERLAY_CLK | SYNCGEN_CLK), true);
			odin_crg_dss_clk_ena(CRG_OTHER_CLK, (SFR_CLK) , true);
#endif
			odin_crg_dss_mem_sleep_in(HDMI_SLEEP |
						  HDMI_SHUT_DOWN, false);
/* pll2 clock divide setting
* crg_register address 0xC
* 27MHz : 0x100000C
* 74.25MHz : 0x1000000
* 148.5MHz : 0xC
*/
#if 0
			reg = crg_read_tzreg(ADDR_CRG_CLOCK_DIVIDE_VALUE_0);
			reg &= ~0xFF;
			reg |= 0x0C;
			crg_write_tzreg(ADDR_CRG_CLOCK_DIVIDE_VALUE_0, reg);
/* need pll2 change start */
			crg_write_tzreg(0x14, 0x90);
#endif
		break;
	 	default:
			break;
		}
	}
	else
	{
		switch (channel) {
		case ODIN_DSS_CHANNEL_LCD0:
			odin_crg_dss_mem_sleep_in(DIP0_SLEEP ,true);
			odin_crg_dss_clk_ena(CRG_CORE_CLK,
					     (DIP0_CLK | MIPI0_CLK), false);
			odin_crg_dss_clk_ena(CRG_OTHER_CLK,
				(/*DISP0_CLK |*/ DPHY0_OSC_CLK | TX_CLK_ESC0),
					     false);

			odin_crg_dss_pll_ena(crg_dev.disp0_clk, false);
			clk_unprepare(crg_dev.disp0_clk);

			break;
		case ODIN_DSS_CHANNEL_LCD1:
			odin_crg_dss_mem_sleep_in(DIP1_SLEEP ,true);
			odin_crg_dss_clk_ena(CRG_CORE_CLK,
					     (DIP1_CLK | MIPI1_CLK), false);
			odin_crg_dss_clk_ena(CRG_OTHER_CLK,
				(/*DISP1_CLK | */DPHY1_OSC_CLK | TX_CLK_ESC1),
					     false);

			odin_crg_dss_pll_ena(crg_dev.disp1_clk, false);
			clk_unprepare(crg_dev.disp1_clk);
		break;
	 	case ODIN_DSS_CHANNEL_HDMI:
			odin_crg_dss_mem_sleep_in(HDMI_SLEEP | HDMI_SHUT_DOWN,
						  true);
			odin_crg_dss_clk_ena(CRG_CORE_CLK,
					     (MXD_CLK | DIP2_CLK | HDMI_CLK),
					     false);
			odin_crg_dss_clk_ena(CRG_OTHER_CLK, (SFR_CLK), false);
			/* odin_crg_dss_clk_ena(CRG_OTHER_CLK, HDMI_DISP_CLK , false); */

			/* hdmi channel needs both disp0_clk & hdmi_disp_clk */
			odin_crg_dss_pll_ena(crg_dev.hdmi_disp_clk, false);
			clk_unprepare(crg_dev.hdmi_disp_clk);

			odin_crg_dss_pll_ena(crg_dev.disp0_clk, false);
			clk_unprepare(crg_dev.disp0_clk);
		break;
	 	default:
			break;
		}
	}

	return 0;
}

u32 odin_crg_dss_plane_to_resetmask(enum odin_dma_plane plane)
{
	switch (plane) {
	case ODIN_DSS_VID0:
		return VSCL0_RESET;
	case ODIN_DSS_VID1:
		return VSCL1_RESET;
	case ODIN_DSS_VID2_FMT:
		return VDMA_RESET;
	case ODIN_DSS_GRA0:
		return GDMA0_RESET;
	case ODIN_DSS_GRA1:
		return GDMA1_RESET;
	case ODIN_DSS_GRA2:
		return GDMA2_RESET;
	case ODIN_DSS_mSCALER:
		return GSCL_RESET;
	default:
		return 0;
	}
}

int odin_crg_dss_reset(u32 reset_mask, bool enable)
{
	u32 base_addr = 0; /* CRG_BASE_ADDR; */
	u32 write_val = 0;
	u32 read_val = 0;
	/* DSSINFO("sw_reset for dss :%d plane %d enable\n", plane, enable); */
	base_addr = ADDR_CRG_SOFT_RESET;

	read_val = 0;
	read_val = crg_read_tzreg(base_addr);

	if (enable) {
		write_val = (read_val | reset_mask);
		crg_write_tzreg(base_addr, write_val);
	}
	else {
		write_val = (read_val & (~(reset_mask)));
		crg_write_tzreg(base_addr, write_val);
	}

	return 0;
}

int odin_crg_mxd_swreset(bool enable)
{
	u32 base_addr = 0; /* CRG_BASE_ADDR; */
	u32 write_val = 0;
	u32 read_val = 0;
	u32 reset_mask = 0;

	printk("sw_reset for mxd : %d enable\n", enable);
	base_addr = ADDR_CRG_SOFT_RESET;

	read_val = crg_read_tzreg(base_addr);
	reset_mask = MXD_RESET;

	if (enable) {
		write_val = (read_val | reset_mask);
		crg_write_tzreg(base_addr, write_val);
		udelay(100);
	}
	else {
		write_val = (read_val & (~(reset_mask)));
		crg_write_tzreg(base_addr, write_val);
		udelay(100);
	}

	return 0;
}

int odin_crg_gfx_swreset(bool enable)
{
	u32 base_addr = 0; /* CRG_BASE_ADDR; */
	u32 write_val = 0;
	u32 read_val = 0;
	u32 reset_mask = 0;

	printk("sw_reset for gfx : %d enable\n", enable);
	base_addr = ADDR_CRG_SOFT_RESET;

	read_val = crg_read_tzreg(base_addr);
	reset_mask = GFX0_RESET | GFX1_RESET;

	if (enable) {
		write_val = (read_val | reset_mask);
		crg_write_tzreg(base_addr, write_val);
		udelay(100);
	}
	else {
		write_val = (read_val & (~(reset_mask)));
		crg_write_tzreg(base_addr, write_val);
		udelay(100);
	}

	return 0;
}
#ifdef VID_SRC_COUNTING
/* vid_using_src_mask: vid src plane USING now. */
u32 vid_using_src_mask = 0;
static inline int __vid_src_masking_cnt (u32 mask)
{
	/* 0000101 (VID0/VID2) masks returns masking_cnt "2" */
	int i, masking_cnt = 0;
	for (i = 0; i < 3; i++)	/* vid source is three */
		if (mask & (1 << i))
			masking_cnt++;
	return masking_cnt;
}
#endif

u32 odin_crg_du_plane_to_clk(u32 plane, bool enable)
{
	u32 vid_mask, gra_mask, gscl_mask;
	u32 clk_mask = 0;
	u32 current_mask = 0;
	int vid_clk_count = 0;

	vid_mask = (plane & ((1 << ODIN_DSS_VID0) | (1 << ODIN_DSS_VID1) |
		   (1 << ODIN_DSS_VID2_FMT)));
	if (vid_mask)
	{
		if (!enable)
		{
#ifdef VID_SRC_COUNTING
			/*not used vid source */
			vid_using_src_mask &= ~vid_mask;
			vid_clk_count = __vid_src_masking_cnt
						(vid_using_src_mask);

			if (!vid_clk_count)
				clk_mask |= (vid_mask | VSCL0_CLK);
			else
				clk_mask |= (vid_mask & (~VSCL0_CLK));
#else
/*			current_mask = odin_crg_get_dss_clk_ena(CRG_CORE_CLK);
			get_vid_mask = (current_mask & ((1 << ODIN_DSS_VID0) | (1 << ODIN_DSS_VID1) | (1 << ODIN_DSS_VID2_FMT)));

			if (!(get_vid_mask & (~vid_mask)))
				clk_mask |= (vid_mask | VSCL0_CLK);
			else
				clk_mask |= (vid_mask & VSCL0_CLK)? (vid_mask & (~VSCL0_CLK)) : vid_mask;*/
#endif
#if 0 /* Dont's use debug because of spinlock check odin_dss_mgr_blank */
#if defined(VID_SRC_COUNTING) && defined(RUNTIME_CLK_GATING_LOG)
			printk("[-]vid_cnt:%d\n", vid_clk_count);
#endif
#endif
		}
		else {
			clk_mask |= (vid_mask | VSCL0_CLK);
#ifdef VID_SRC_COUNTING
			vid_using_src_mask |= vid_mask;
			vid_clk_count = __vid_src_masking_cnt(vid_using_src_mask);
#endif
#if 0 /* Dont's use debug because of spinlock check odin_dss_mgr_blank */
#if defined(VID_SRC_COUNTING) && defined(RUNTIME_CLK_GATING_LOG)
			printk("[+]vid_cnt:%d\n", vid_clk_count);
#endif
#endif
		}
	}

	gra_mask = (plane & ((1 << ODIN_DSS_GRA0) | (1 << ODIN_DSS_GRA1) |
						(1 << ODIN_DSS_GRA2)));
	if (gra_mask)
		clk_mask |= (gra_mask << 3);

	gscl_mask = (plane & (1 << ODIN_DSS_mSCALER));
	if (gscl_mask)
	{
		if (!enable)
		{
			/* check CABC clk on , hdmi clk */
			current_mask = odin_crg_get_dss_clk_ena(CRG_CORE_CLK);
			if (!(current_mask & CABC_CLK) && !(current_mask & HDMI_CLK))
				clk_mask |= (gscl_mask << 4);
		}
		else
			clk_mask |= (gscl_mask << 4);
	}

	return clk_mask;
}

int odin_crg_underrun_set_info(enum odin_channel channel, u32 ovl_mask,
	u32 request_qos)
{
	crg_dev.err_plane_mask[channel] =
		((odin_du_get_ovl_src_go() >> (8*channel)));/* | ovl_mask); */
	crg_dev.request_qos = request_qos;

	return 0;
}

int odin_crg_underrun_process_thread(enum odin_channel channel,
	bool reset_needed)
{
	/* u32 set_clk = 0; */
	int ret = 0;

	/* DSSINFO("MIPI DSI Underrun restore thread processing \n"); */

	odin_crg_dss_clk_ena(CRG_CORE_CLK, crg_dev.err_clk_mask[channel], true);

	if (reset_needed)
	{
		odin_crg_dss_reset(crg_dev.err_reset_mask[channel], true);
		udelay(1);
		odin_crg_dss_reset(crg_dev.err_reset_mask[channel], false);
		DSSERR("dma pipe reseting ch = %d, mask = %x \n", channel,
			crg_dev.err_reset_mask[channel]);
		ret = -EPIPE;
	}

	return ret;
}

void odin_crg_mipi_set_err_crg_mask(enum odin_channel channel)
{
	int i;
	crg_dev.err_reset_mask[channel] = 0;
	crg_dev.err_clk_mask[channel] = 0;

	DSSDBG("err_plane_mask = %x \n", crg_dev.err_plane_mask[channel]);

	for (i = 0; i < MAX_OVERLAYS; i++)
	{
		if (crg_dev.err_plane_mask[channel] & (1 << i))
		{
			crg_dev.err_reset_mask[channel] |=
				odin_crg_dss_plane_to_resetmask(i);
			crg_dev.err_clk_mask[channel] |=
				odin_crg_du_plane_to_clk((1 << i), true);
		}
	}
}

void crg_mipi_underrun_process_irq(u32 cur_err_status)
{
	enum odin_channel channel;
	int i;
	struct odin_overlay_manager * mgr;
#ifdef DSS_UNDERRUN_LOG
	s64 time_stamp;
	int  zorder = 0;
	u32 enable_need_mask;
#if 0 /*CPU IFNO */
	u64 total_stat;
	unsigned int load_val_l;
	struct cpufreq_policy *freq_policy;
#endif
#endif

	if (cur_err_status & CRG_IRQ_MIPI0)
		channel = ODIN_DSS_CHANNEL_LCD0;
	else if (cur_err_status & CRG_IRQ_MIPI1)
		channel = ODIN_DSS_CHANNEL_LCD1;
	else if (cur_err_status & CRG_IRQ_DIP2)
	{
		channel = ODIN_DSS_CHANNEL_HDMI;
		odin_crg_mipi_set_err_crg_mask(channel);
		odin_crg_underrun_process_thread(channel, true);
		return;
	}
	else
		return;

#ifdef DSS_UNDERRUN_LOG
#if 0
		for (i = 0; i < 8; i++)
		{
			DSSINFO("core%d state: %x \n", i, __mcpm_cpu_state(i%4, i/4));
			freq_policy = cpufreq_cpu_get(i);
			DSSINFO("core%d cpu freq: %d kHz \n", i, freq_policy->cur);
			total_stat = 0;
			for(j=0; j<NR_STATS; j++) {
			   total_stat += kcpustat_cpu(i).cpustat[j];
			}
			load_val_l = (((unsigned int)total_stat -
					   (unsigned int)
						kcpustat_cpu(i).cpustat[CPUTIME_IDLE])*100/
					   (unsigned int)total_stat);
			DSSINFO("core%d cpu load: %lu \n", i, load_val_l);
		}
#endif

	underrun_info.time_stamp = odin_mip_dsi_time_after_update();
	underrun_info.core_clk_status = odin_crg_get_dss_clk_ena(CRG_CORE_CLK);
	underrun_info.update_status= odin_mipi_dsi_get_update_status();
	underrun_info.dsi_state = odin_mipi_dsi_get_state();
	underrun_info.enabled_pipe = crg_dev.err_plane_mask[channel];

	if (channel == ODIN_DSS_CHANNEL_LCD0)
		enable_need_mask = (crg_dev.err_clk_mask[channel] |
			(OVERLAY_CLK|DIP0_CLK|MIPI0_CLK));
	else
		enable_need_mask = (crg_dev.err_clk_mask[channel] |
			(OVERLAY_CLK|DIP1_CLK|MIPI1_CLK));

	odin_crg_dss_clk_ena(CRG_CORE_CLK,
			enable_need_mask, true);

	for (i = 0; i < MAX_OVERLAYS; i++)
	{
		underrun_info.enabled_pipe = crg_dev.err_plane_mask[channel];
		if (crg_dev.err_plane_mask[channel] & (1 << i))
		{
			underrun_info.dma_pipe[i].src_width =
				odin_du_get_src_size_width(i);
			underrun_info.dma_pipe[i].src_height =
				odin_du_get_src_size_height(i);
			underrun_info.dma_pipe[i].out_width =
				odin_du_get_src_pipsize_width(i);
			underrun_info.dma_pipe[i].out_height =
				odin_du_get_src_pipsize_height(i);
			underrun_info.dma_pipe[i].read_status =
				odin_du_get_rd_staus(i);
			underrun_info.dma_pipe[i].ovl_src_pos =
				odin_du_get_ovl_src_pos(zorder);
			underrun_info.dma_pipe[i].ovl_src_size =
				odin_du_get_ovl_src_size(zorder);
			zorder++;
		}
	}

	underrun_info.dip_fifo_cnt = odin_dipc_get_lcd_cnt(channel);
	underrun_info.mipi_fifo_cnt = odin_mipi_dsi_get_tx_fifo(channel);
	underrun_info.ovl_src_sel = odin_du_get_ovl_src_sel(channel);
	underrun_info.ovl_src_en = odin_du_get_ovl_src_go();
	underrun_info.request_qos = crg_dev.request_qos;
#endif

	mgr = odin_mgr_dss_get_overlay_manager((int) channel);
	if (!(dssdev_manually_updated(mgr->device)))
		return;

#if RUNTIME_CLK_GATING
	enable_runtime_clk(mgr, false);
#endif
	odin_mipi_cmd_loop_set(channel, false, 0);

	crg_dev.err_channel = channel;

	odin_crg_mipi_set_err_crg_mask(channel);

#ifdef DSS_CACHE_BACKUP
	odin_dss_cache_log(channel);
#endif

	odin_crg_underrun_process_thread(channel, true);
}

static irqreturn_t dss_irq_handler(int irq, void *dev_id)
{
	int i;
	u32 irqstatus, irqenable;
	u32 sub_irqstatus[MAIN_MAX_POS] = {0,};
	u32 sub_irqclr;
	u32 sub_irqenable;
	u8  sub_hdmistatus;
	bool dip_status;
	bool mipi_status;
	bool hdmi_status;
	struct crg_isr_data *isr_data;
	struct crg_isr_data registered_isr[CRG_MAX_NR_ISRS];
	u32 cur_err_status;
#ifdef CONFIG_ARM_ODIN_BUS_DEVFREQ
	unsigned int clk_rate = 0;
#endif
	spin_lock(&crg_dev.irq_lock);

	/* dss_irq_handler needs to be MIPI0 / DIP0 clk enabled*/
	odin_crg_dss_clk_ena(CRG_CORE_CLK, MIPI0_CLK | DIP0_CLK , true);

	irqenable = (CRG_IRQ_MAIN_ALL & _crg_get_interrupt_mask());
	_crg_set_interrupt_mask(0x0);
	irqstatus = (CRG_IRQ_MAIN_ALL & _crg_get_interrupt_status());
	/* IRQ is not for us */
	if (!(irqstatus & irqenable)) {
		DSSINFO("unenabled intterupt is enabled enable=%x, status=%x\n",
			irqenable, irqstatus);
		_crg_set_interrupt_clear(irqstatus);
		_crg_set_interrupt_mask(irqenable);
		spin_unlock(&crg_dev.irq_lock);
		return IRQ_HANDLED;
	}

	for (i = 0; i < NUM_DIPC_BLOCK; i++)
	{
		dip_status = irqstatus & (CRG_IRQ_DIP0 << (i<<1));
		if (!dip_status)
			continue;

		sub_irqclr = sub_irqstatus[MAIN_DIP0_POS + (i<<1)]
			   = dipc_get_irq_status(i);
		sub_irqenable = ~(dipc_get_irq_mask(i));
		dipc_clear_irq(i, (sub_irqclr & sub_irqenable));
	}

	for (i = 0; i < NUM_MIPI_BLOCK; i++)
	{
		mipi_status = irqstatus & (CRG_IRQ_MIPI0 << (i<<1));
		if (!mipi_status)
			continue;

		sub_irqclr = sub_irqstatus[MAIN_MIPI0_POS + (i<<1)]	=
			odin_mipi_dsi_get_irq_status(i);
		sub_irqenable = ~(odin_mipi_dsi_get_irq_mask(i));
		odin_mipi_dsi_irq_clear(i, (sub_irqclr & sub_irqenable));
	}


	hdmi_status = irqstatus & (CRG_IRQ_HDMI);
	if(hdmi_status)
	{
		sub_hdmistatus = hdmi_get_irq_status();
		hdmi_clear_irq(sub_hdmistatus);
	}

	_crg_set_interrupt_clear(irqstatus);
	_crg_set_interrupt_mask(irqenable);
	if (_crg_get_interrupt_status() != 0x0) {
		DSSINFO(" not clear mask:%x, submask:%x, sub_clear:%x, irqstatus=%x\n",
		_crg_get_interrupt_status(),
		odin_mipi_dsi_get_irq_status(0), sub_irqclr, irqstatus);

		DSSINFO("core:0x%x\n", odin_crg_get_dss_clk_ena(CRG_CORE_CLK));
		check_clk_gating_status();
	}
	/* make a copy and unlock, so that isrs can unregister
	 * themselves */
	memcpy(registered_isr, crg_dev.registered_isr,
			sizeof(registered_isr));

	spin_unlock(&crg_dev.irq_lock);

	for (i = 0; i < CRG_MAX_NR_ISRS; i++) {
		isr_data = &registered_isr[i];

		if (!isr_data->isr)
			continue;

		if (isr_data->mask & irqstatus) {
			if (!isr_data->sub_mask)
				goto match_case;
			else if (isr_data->sub_mask &
				sub_irqstatus[fls(isr_data->mask)-1])
				goto match_case;

			continue;

			match_case:
				isr_data->isr(isr_data->arg, isr_data->mask,
					      isr_data->sub_mask);
		}
	}

/* DSSDBG("dss_irq_handler: main = %x sub = %x \n", irqstatus, sub_irqclr); */
/* DSSDBG( "dss_irq_handler called (%ld) \n", jiffies); */

/* For Error Check */
	spin_lock(&crg_dev.err_lock);

	cur_err_status = crg_dev.err_enable.mask & irqstatus;

	if (cur_err_status)
	{
		while (cur_err_status) {
			u32 pos = fls(cur_err_status) - 1, m = 1 << pos;
			cur_err_status &= ~m;

			if (m & crg_dev.err_status.mask )
				continue;

			crg_dev.err_status.sub_mask[pos] =
				crg_dev.err_enable.sub_mask[pos] &
				sub_irqstatus[pos];
			if (crg_dev.err_status.sub_mask[pos])
			{
				crg_dev.err_status.mask |= m;
				crg_set_err_mask
					(m, crg_dev.err_enable.sub_mask[pos],
					 false);
				crg_mipi_underrun_process_irq(m);
#ifdef CONFIG_ARM_ODIN_BUS_DEVFREQ
				clk_rate = odin_get_memclk();
				DSSINFO("current mem_clk rate = %d \n", clk_rate);
#endif
				schedule_work(&crg_dev.error_work);
			}
		}
	}

	spin_unlock(&crg_dev.err_lock);

	return IRQ_HANDLED;
}

void odin_dipc_fifo_flush_handler(void *data, u32 mask, u32 sub_mask)
{
	int dip_ch;
	int fifo_flush_cnt;
	if (mask & (CRG_IRQ_DIP0 | CRG_IRQ_MIPI0))
		dip_ch = 0;
	else if (mask & (CRG_IRQ_DIP1 | CRG_IRQ_MIPI1))
		dip_ch = 1;
	else
		dip_ch = 2;

	odin_dipc_fifo_flush(dip_ch);
	odin_dipc_resync(dip_ch);

	spin_lock(&crg_dev.err_lock);
    fifo_flush_cnt = crg_dev.fifo_flush_cnt[dip_ch]+1;
	spin_unlock(&crg_dev.err_lock);

	DSSINFO("crg_dev.fifo_flush_cnt = %d \n", fifo_flush_cnt);

	if (fifo_flush_cnt > 1)
	{
		odin_crg_unregister_isr(odin_dipc_fifo_flush_handler, NULL,
				mask, sub_mask);
	    fifo_flush_cnt = 0;

		spin_lock(&crg_dev.err_lock);
		crg_dev.fifo_flush_ok[dip_ch] = true;
		spin_unlock(&crg_dev.err_lock);
	}

	spin_lock(&crg_dev.err_lock);
	crg_dev.fifo_flush_cnt[dip_ch] = fifo_flush_cnt;
	spin_unlock(&crg_dev.err_lock);

}

bool odin_crg_get_error_handling_status(enum odin_channel channel)
{
	unsigned int err_handing_mask;
	if (channel == ODIN_DSS_CHANNEL_LCD0)
		return (bool)(crg_dev.err_status.mask & (CRG_IRQ_MIPI0 | CRG_IRQ_DIP0));
	else if(channel == ODIN_DSS_CHANNEL_LCD1)
		return (bool)(crg_dev.err_status.mask & (CRG_IRQ_MIPI1 | CRG_IRQ_DIP1));
	else if (channel == ODIN_DSS_CHANNEL_HDMI)
		return (bool)(crg_dev.err_status.mask & (CRG_IRQ_DIP2));
	else
		return false;
}

#ifdef DSS_UNDERRUN_LOG
static void crg_print_underrun_log(void)
{
	int i;

	DSSINFO("MIPI DSI Update start time = %lld ns\n",
		underrun_info.time_stamp);

	for (i = 0; i < MAX_OVERLAYS; i++)
	{
		if (underrun_info.enabled_pipe & (1 << i))
		{
			DSSINFO("sw=%d,sh=%d,ow=%d,oh=%d,rd_st=%x,ovlpos=%x,ovlsize=%x \n",
				underrun_info.dma_pipe[i].src_width,
				underrun_info.dma_pipe[i].src_height,
				underrun_info.dma_pipe[i].out_width,
				underrun_info.dma_pipe[i].out_height,
				underrun_info.dma_pipe[i].read_status,
				underrun_info.dma_pipe[i].ovl_src_pos,
				underrun_info.dma_pipe[i].ovl_src_size);
		}
	}

	DSSINFO("clk_status=%x,dip_fifo = %x,mipi fifo = %x \n",
		underrun_info.core_clk_status,
		underrun_info.dip_fifo_cnt,
		underrun_info.mipi_fifo_cnt);
}
#endif

static void crg_error_worker(struct work_struct *work)
{
	int i;
	u32 err_mask;
	unsigned long flags;
	int dip_mask;
	int dip_sub_mask;
	int mipi_mask;
	int mipi_sub_mask;
	struct odin_overlay_manager *manager = NULL;
	int cnt = 100;
	int ret_dip = 0;
	int ret_mipi = 0;
	unsigned int retry_cnt = 0;

	spin_lock_irqsave(&crg_dev.err_lock, flags);
	err_mask = crg_dev.err_status.mask;
	spin_unlock_irqrestore(&crg_dev.err_lock, flags);

	/*DIP Case*/
	if (err_mask & (CRG_IRQ_DIP0 | CRG_IRQ_DIP1 | CRG_IRQ_DIP2))
	{
		for (i = 0; i < NUM_DIPC_BLOCK; i++)
		{
			dip_mask = ( CRG_IRQ_DIP0 << (i << 1));
			if (!(err_mask & dip_mask))
				continue;

			DSSINFO("DIP%d workque processed\n", i);

#ifdef ODIN_DSS_UNDERRUN_RESTORE /* Underun Restore Code is Blcoking */
			manager = odin_mgr_dss_get_overlay_manager(i);

			do {
				odin_dipc_fifo_flush(i);
				usleep_range(10, 20);

				retry_cnt++;
				if (retry_cnt > 10000)
				{
					DSSERR("dip retry fail \n");
					break;
				}
			} while (odin_dipc_get_lcd_cnt(i) != 0x0);

			odin_dipc_resync(i);

			odin_du_set_ovl_cop_or_rop(ODIN_DSS_CHANNEL_LCD0,
				ODIN_DSS_OVL_ROP);

			odin_dipc_resync_hdmi(manager->device);

			odin_dipc_resync(i);

			if (i == 1)
				dip_sub_mask = CRG_IRQ_DIP_FIFO0_ACCESS_ERR;
			else
				dip_sub_mask = CRG_IRQ_DIP_FIFO1_ACCESS_ERR;


			crg_set_err_mask(dip_mask, dip_sub_mask, true);
#else
			if ((i == 1) || (i == 2))
				dip_sub_mask = CRG_IRQ_DIP_FIFO1_ACCESS_ERR;
			else
				dip_sub_mask = CRG_IRQ_DIP_FIFO1_ACCESS_ERR;

			crg_set_err_mask(dip_mask, dip_sub_mask, true);
			msleep(50);
#endif
		}
	}

	/*MIPI Case*/
	if (err_mask & CRG_IRQ_MIPIS)
	{
		for (i = 0; i < NUM_MIPI_BLOCK; i++)
		{
			mipi_mask = ( CRG_IRQ_MIPI0 << (i << 1));
			if (!(err_mask & mipi_mask))
				continue;

#ifdef ODIN_DSS_UNDERRUN_RESTORE
			manager = odin_mgr_dss_get_overlay_manager(i);
			if (dssdev_manually_updated(manager->device))
			{
				msleep(10);
				cnt = 0 ;
				do {
					if (cnt++ > 10000)
					{
						ret_dip = -EPIPE;
						break;
					}
					odin_dipc_fifo_flush(i);
					udelay(1);
				} while (odin_dipc_get_lcd_cnt(i) != 0x0);

				do {
					if (cnt++ > 10000)
					{
						ret_mipi = -EPIPE;
						break;
					}
					odin_mipi_dsi_fifo_flush_all(i);
					udelay(1);
				} while (odin_mipi_dsi_check_tx_fifo(i) != 0x0);

				mipi_sub_mask = CRG_IRQ_MIPI_TXDATA_FIFO_URERR;
				crg_set_err_mask(mipi_mask, mipi_sub_mask, true);
				manager->device->driver->flush_pending(
						(struct odin_dss_device *)manager->device);

#if RUNTIME_CLK_GATING
				disable_runtime_clk(manager->id);
#endif
#ifdef DSS_UNDERRUN_LOG
				crg_print_underrun_log();
#endif
				DSSERR("MIPI%d Access Error restored\n",i );
			}
			else
			{
				spin_lock_irqsave(&crg_dev.err_lock, flags);
				crg_dev.fifo_flush_cnt[i] = 0;
				spin_unlock_irqrestore(&crg_dev.err_lock, flags);

				dsscomp_complete_workqueue(manager->device, false, true);

				odin_du_set_ovl_cop_or_rop(ODIN_DSS_CHANNEL_LCD0,
					ODIN_DSS_OVL_COP);
				odin_set_rd_enable_plane(i, 0);
				manager->wait_for_vsync(manager);

				cnt = 0 ;
				do {
					if (cnt++ > 100) break;
					odin_dipc_fifo_flush(i);
					udelay(1);
				} while (odin_dipc_get_lcd_cnt(i) != 0x0);

				do {
					if (cnt++ > 100) break;
					odin_mipi_dsi_fifo_flush_all(i);
					udelay(1);
				} while (odin_mipi_dsi_check_tx_fifo(i) != 0x0);

				odin_set_rd_enable_plane(i, 1);
			}

			mipi_sub_mask = CRG_IRQ_MIPI_TXDATA_FIFO_URERR;
			crg_set_err_mask(mipi_mask, mipi_sub_mask, true);
		}
	}
#endif

	spin_lock_irqsave(&crg_dev.err_lock, flags);
	crg_dev.err_status.mask &= ~(err_mask);
	spin_unlock_irqrestore(&crg_dev.err_lock, flags);

}

#if RUNTIME_CLK_GATING
void odin_crg_runtime_clk_enable(enum odin_channel channel, bool enable)
{
	u32 core_clk_mask = 0;
	u32 other_clk_mask = 0;
	struct clk	*dp_pll_clk;

	switch (channel)
	{
	case ODIN_DSS_CHANNEL_LCD0:
		if (odin_dipc_ccr_gamma_config_status(ODIN_DSS_CHANNEL_LCD0))
			core_clk_mask = MIPI0_CLK;
		else
			core_clk_mask = DIP0_CLK | MIPI0_CLK;
#if DP_PLL_GATING
		dp_pll_clk = crg_dev.disp0_clk;
		other_clk_mask = DPHY0_OSC_CLK | TX_CLK_ESC0;
#else
		other_clk_mask = DISP0_CLK | DPHY0_OSC_CLK | TX_CLK_ESC0;
#endif
		break;
	case ODIN_DSS_CHANNEL_LCD1:
		core_clk_mask = DIP1_CLK | MIPI1_CLK;
#if DP_PLL_GATING
		dp_pll_clk = crg_dev.disp1_clk;
		other_clk_mask = DPHY1_OSC_CLK | TX_CLK_ESC1;
#else
		other_clk_mask = DISP1_CLK | DPHY1_OSC_CLK | TX_CLK_ESC1;
#endif
		break;
	default:
		DSSERR("invalid channel in runtime_clk_enable\n");
		return;
	}

	core_clk_mask |= (OVERLAY_CLK | SYNCGEN_CLK);

	if (!enable) {
		if (odin_crg_get_dss_clk_ena(CRG_CORE_CLK) & CABC_CLK)
			core_clk_mask &= ~OVERLAY_CLK;
		if (odin_crg_get_dss_clk_ena(CRG_CORE_CLK) & HDMI_CLK)
			core_clk_mask &= ~(OVERLAY_CLK | SYNCGEN_CLK);
		if ((channel == ODIN_DSS_CHANNEL_LCD0) &&
			(odin_crg_get_dss_clk_ena(CRG_OTHER_CLK) &
						  (DISP1_CLK | HDMI_DISP_CLK)))
			core_clk_mask &= ~SYNCGEN_CLK;
#if DP_PLL_GATING
		if (dp_pll_clk->prepare_count)
			odin_crg_dss_pll_ena(dp_pll_clk, false);
		else {
			DSSERR("dp_pll is not prepared\n");
			return;
		}
#endif
	}
	else {
#if DP_PLL_GATING
		if (dp_pll_clk->prepare_count)
			odin_crg_dss_pll_ena(dp_pll_clk, true);
		else {
			DSSERR("dp_pll is not prepared\n");
			return;
		}
#endif
	}

	odin_crg_dss_clk_ena(CRG_CORE_CLK, core_clk_mask, enable);
	odin_crg_dss_clk_ena(CRG_OTHER_CLK, other_clk_mask, enable);

	return;
}
#endif

#ifdef CONFIG_OF
extern struct device odin_device_parent;

static struct of_device_id odindss_crg_match[] = {
	{
		.compatible = "odindss-crg",
	},
	{},
};
#endif
/* CRG HW IP initialization */
static int odin_crghw_probe(struct platform_device *pdev)
{
	int r = 0;
#ifdef CONFIG_OF
	struct resource res;

	pdev->dev.parent = &odin_device_parent;
#else
	struct resource *res;
#endif
	DSSINFO("odin_crghw_probe\n");

	memset(&crg_dev ,0x0, sizeof(crg_dev));

	crg_dev.pdev = pdev;

#ifdef CONFIG_OF
	r = of_address_to_resource(pdev->dev.of_node, 0, &res);
	if (r < 0)
	{
		DSSERR("can't get IORESOURCE_MEM CRG\n");
		r = -EINVAL;
		goto err;
	}

	crg_dev.base = res.start;
	crg_dev.iobase = ioremap(res.start, resource_size(&res));
	if (crg_dev.iobase == NULL) {
		DSSERR("can't ioremap CRG\n");
		r = -ENOMEM;
		goto err;
	}

	crg_set_irq_mask_all(0);
	_crg_set_interrupt_clear(0xFFFFFFFF);
	crg_dev.irq = irq_of_parse_and_map(crg_dev.pdev->dev.of_node, 0);
	if (crg_dev.irq < 0) {
		DSSERR("no irq for device\n");
		r = -ENOENT;
		goto err;
	}
#else
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		DSSERR("can't get IORESOURCE_MEM CRG\n");
		r = -EINVAL;
		goto err;
	}

	crg_dev.base = res->start;
	crg_dev.iobase = ioremap(res->start, resource_size(res));
	if (crg_dev.base == NULL) {
		DSSERR("can't ioremap CRG\n");
		r = -ENOMEM;
		goto err;
	}

	crg_set_irq_mask_all(0);
	_crg_set_interrupt_clear(0xFFFFFFFF);

	crg_dev.irq = platform_get_irq(crg_dev.pdev, 0);
	if (crg_dev.irq < 0) {
		DSSERR("no irq for device\n");
		r = -ENOENT;
		goto err;
	}
#endif
	r = request_irq(crg_dev.irq, dss_irq_handler, IORESOURCE_IRQ_HIGHEDGE,
		pdev->name, &crg_dev);
	if (r < 0) {
		dev_err(&pdev->dev, "Can't request IRQ for dss_irq_handler(%d)\n",
			crg_dev.irq);
		goto err;
	}

	spin_lock_init(&crg_dev.irq_lock);
	spin_lock_init(&crg_dev.err_lock);
	spin_lock_init(&crg_dev.clk_setting_lock);
	spin_lock_init(&crg_dev.tz_lock);
	init_completion(&crg_dev.completion);

	/* crg_dev.error_work_queue = create_singlethread_workqueue("error_worker"); */

	INIT_WORK(&crg_dev.error_work, crg_error_worker);
	crg_dev.err_status.mask = 0;

#if DP_PLL_GATING
	crg_dev.disp0_clk = clk_get(NULL, "disp0_clk");
	crg_dev.disp1_clk = clk_get(NULL, "disp1_clk");
	crg_dev.hdmi_disp_clk = clk_get(NULL, "hdmi_disp_clk");
#endif
	platform_set_drvdata(pdev, &crg_dev);

	return 0;

err:
	if (crg_dev.iobase)
	{
		iounmap(crg_dev.iobase);
		crg_dev.base = (u32)NULL;
	}

	return r;
}

static int odin_crghw_remove(struct platform_device *pdev)
{
	DSSINFO("odin_crghw_remove\n");

	iounmap(crg_dev.iobase);
	crg_dev.base = (u32)NULL;

	return 0;
}

#if 0
static int crghw_runtime_suspend(struct device *dev)
{
	return 0;
}

static int crghw_runtime_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops	crg_pm_ops = {
	.runtime_suspend = crghw_runtime_suspend,
	.runtime_resume = crghw_runtime_resume,
};
#endif

static int odin_crghw_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int odin_crghw_resume(struct platform_device *pdev)
{
#if 0
	u32 sleep_mask, core_clkgate, other_clkgate;

	dev_dbg(&pdev->dev, "resume\n");
	crg_set_irq_mask_all(0);
	_crg_set_interrupt_clear(0xFFFFFFFF);

	/* DEEP sleep mask */
	sleep_mask = (HDMI_SHUT_DOWN | HDMI_SLEEP | DIP1_SLEEP);
	crg_write_tzreg(ADDR_CRG_MEMORY_DEEP_SLEEP_MODE_CONTROL, sleep_mask);

	/* core clk gating */
	core_clkgate = (MXD_CLK | CABC_CLK | GDMA0_CLK | OVERLAY_CLK |
					SYNCGEN_CLK | DIP0_CLK | MIPI0_CLK);
	crg_write_tzreg(ADDR_CRG_CORE_CLOCK_ENABLE, core_clkgate);

	/* other clk gating */
	other_clkgate = (crg_read_tzreg(ADDR_CRG_OTHER_CLOCK_CONTROL)) &
			 0x0000FFFF;
	other_clkgate |= (DISP0_CLK | DISP1_CLK | DPHY0_OSC_CLK | TX_CLK_ESC0);
	crg_write_tzreg(ADDR_CRG_OTHER_CLOCK_CONTROL, other_clkgate);
#endif
	return 0;
}

static struct platform_driver odin_crghw_driver = {
	.probe		= odin_crghw_probe,
	.remove		= odin_crghw_remove,
	.suspend	= odin_crghw_suspend,
	.resume		= odin_crghw_resume,
	.driver		= {
		.name	= "odindss-crg",
		.owner 	= THIS_MODULE,
	/*	.pm	= &crg_pm_ops, */
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odindss_crg_match),
#endif
	},
};

int odin_crg_init_platform_driver(void)
{
	return platform_driver_register(&odin_crghw_driver);
}

void odin_crg_uninit_platform_driver(void)
{
	return platform_driver_unregister(&odin_crghw_driver);
}

