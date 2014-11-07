/*
 * Copyright (C) 2013 Mtekvision
 * Author: Daeheon Kim <kimdh@mtekvision.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <asm/delay.h>

#if CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/odin_mailbox.h>
#include <linux/odin_pd.h>
#include <linux/odin_iommu.h>
#ifdef CONFIG_CPU_IDLE_ODIN_WORKAROUND
#include <linux/odin_cpuidle.h>
#endif

#include <asm/system_info.h>

#include "odin-css.h"
#include "odin-css-clk.h"
#include "odin-css-system.h"

#include "odin-capture.h"
#include "odin-isp.h"
#include "odin-isp-wrap.h"
#include "odin-framegrabber.h"
#include "odin-mipicsi.h"

#include "extern/proxy/odin_proxy.h"

#define CSS_SYS_RESET_CTRL		0x00
#define CSS_SYS_CLK_CTRL		0x04
#define CSS_SYS_MIPI_CLK_CTRL	0x08
#define CSS_SYS_AXI_CLK_CTRL	0x0C
#define CSS_SYS_AHB_CLK_CTRL	0x10
#define CSS_SYS_ISP_CLK_CTRL	0x14
#define CSS_SYS_VNR_CLK_CTRL	0x18
#define CSS_SYS_FPD_CLK_CTRL	0x20
#define CSS_SYS_FD_CLK_CTRL 	0x24
#define CSS_SYS_PIX_CLK_CTRL	0x28
#define CSS_SYS_MEM_DS_CTRL 	0x2C

struct css_power_domain {
	unsigned int pd_num;
	unsigned int bitmap;
};

struct css_power_domains {
	struct css_power_domain pd0;
	struct css_power_domain pd1;
	struct css_power_domain pd2;
	struct css_power_domain pd3;
};

struct css_power_domain_status {
	unsigned int pd_id;
	int 		 count;
};

struct css_crg_store {
	unsigned int crgregs;
	unsigned int value;
};

struct css_crg_store crg_store[] = {
	{CSS_SYS_CLK_CTRL, 		0},
	{CSS_SYS_MIPI_CLK_CTRL, 0},
	{CSS_SYS_AXI_CLK_CTRL, 	0},
	{CSS_SYS_AHB_CLK_CTRL, 	0},
	{CSS_SYS_ISP_CLK_CTRL, 	0},
	{CSS_SYS_VNR_CLK_CTRL, 	0},
	{CSS_SYS_FPD_CLK_CTRL, 	0},
	{CSS_SYS_FPD_CLK_CTRL, 	0},
	{CSS_SYS_FD_CLK_CTRL, 	0},
	{CSS_SYS_PIX_CLK_CTRL, 	0},
	{CSS_SYS_MEM_DS_CTRL, 	0},
};

const struct css_power_domains css_pds = {
	{ 0, 0xffffffff},
	{ 1, POWER_SCALER |	\
		 POWER_ISP0 |	\
		 POWER_ISP1 |	\
		 POWER_JPEG},
	{ 2, POWER_SCALER |	\
		 POWER_ISP0 |	\
		 POWER_ISP1 |	\
		 POWER_S3DC |	\
		 POWER_TUNER},
	{ 3, POWER_MDIS |	\
		 POWER_VNR}
};

struct css_power_domain_status css_pds_status[] = {
	{POWER_SCALER,	0},
	{POWER_MDIS,	0},
	{POWER_VNR,		0},
	{POWER_JPEG,	0},
	{POWER_ISP0,	0},
	{POWER_ISP1,	0},
	{POWER_S3DC,	0},
	{POWER_TUNER,	0},
};

static inline void css_update_pds_enable(int power)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(css_pds_status); i++) {
		if (css_pds_status[i].pd_id & power)
			css_pds_status[i].count++;
	}

	return;
}

static inline void css_update_pds_disable(int power)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(css_pds_status); i++) {
		if (css_pds_status[i].pd_id & power) {
			css_pds_status[i].count--;
			if (css_pds_status[i].count < 0)
				css_pds_status[i].count = 0;
		}
	}

	return;
}

struct css_crg_hardware crg_hardware;

static int css_crg_set_on(void);
static int css_crg_set_off(void);
static int css_pd_on(int power, int memclkctrl);
static int css_pd_off(int power, int memclkctrl);
static void css_crg_save(void);
static void css_crg_restore(void);

static inline struct css_crg_hardware *get_crg_hw(void)
{
	return &crg_hardware;
}

static void css_enable_cpuidle(int cpu)
{
#ifdef CONFIG_CPU_IDLE_ODIN_WORKAROUND
	odin_idle_disable(cpu, 0);
#endif
	return;
}

static void css_disable_cpuidle(int cpu)
{
#ifdef CONFIG_CPU_IDLE_ODIN_WORKAROUND
	odin_idle_disable(cpu, 1);
#endif
	return;
}

int css_set_isp_divider(int kHz)
{
	unsigned int clks;
	unsigned long flags = 0;
	unsigned int div = 0;
	struct css_crg_hardware *crg_hw = NULL;

	if (kHz == MINIMUM_ISP_PERFORMANCE/1000)
		div = 2;
	else if (kHz == DEFAULT_ISP_PERFORMANCE/1000)
		div = 1;
	else
		div = 0;

	crg_hw = get_crg_hw();

	spin_lock_irqsave(&crg_hw->slock, flags);
	clks = readl(crg_hw->iobase + CSS_SYS_ISP_CLK_CTRL);
	clks = clks & 0xFFFFFF0F;
	clks = clks | (div<<4);
	writel(clks, crg_hw->iobase + CSS_SYS_ISP_CLK_CTRL);
	spin_unlock_irqrestore(&crg_hw->slock, flags);

	return 0;
}

unsigned int css_get_isp_divider(void)
{
	unsigned int clks;
	unsigned long flags = 0;
	unsigned int div = 0;
	struct css_crg_hardware *crg_hw = NULL;

	crg_hw = get_crg_hw();

	spin_lock_irqsave(&crg_hw->slock, flags);
	clks = readl(crg_hw->iobase + CSS_SYS_ISP_CLK_CTRL);
	clks = clks & 0x000000F0;
	div = clks>>4;
	spin_unlock_irqrestore(&crg_hw->slock, flags);

	return div;
}


int css_block_reset_off(unsigned int mask)
{
	int ret = CSS_SUCCESS;
	/* unsigned long flags = 0; */
	struct css_crg_hardware *crg_hw = NULL;

	crg_hw = get_crg_hw();

	if (crg_hw && crg_hw->iobase != NULL) {
		/* spin_lock_irqsave(&crg_hw->slock, flags); */
		writel(mask, crg_hw->iobase);
		/* spin_unlock_irqrestore(&crg_hw->slock, flags); */
	} else {
		ret = CSS_FAILURE;
	}

	return ret;
}

int css_block_reset_on(unsigned int mask)
{
	int ret = CSS_SUCCESS;
	struct css_crg_hardware *crg_hw = NULL;

	crg_hw = get_crg_hw();

	if (crg_hw && crg_hw->iobase != NULL) {
		/* spin_lock_irqsave(&crg_hw->slock, flags); */
		writel(0x0, crg_hw->iobase);
		/* spin_unlock_irqrestore(&crg_hw->slock, flags); */
	} else {
		ret = CSS_FAILURE;
	}

	return ret;
}

int css_block_reset(unsigned int mask)
{
	int ret = CSS_SUCCESS;
	struct css_crg_hardware *crg_hw = NULL;

	crg_hw = get_crg_hw();

	if (crg_hw && crg_hw->iobase != NULL) {
		css_block_reset_off(mask);
		udelay(10);
		css_block_reset_on(mask);
		udelay(10);
	} else {
		ret = CSS_FAILURE;
	}

	return ret;
}

int css_power_domain_valid(int power)
{
	if (power & ~(POWER_ALL))
		return -1;
	else
		return 0;
}

int css_power_domain_init_on(int power)
{
	return css_pd_on(power, false);
}

int css_power_domain_init_off(int power)
{
	return css_pd_off(power, false);
}

int css_power_domain_on(int power)
{
	return css_pd_on(power, true);
}

int css_power_domain_off(int power)
{
	return css_pd_off(power, true);
}

void css_power_domain_suspend(void)
{
	struct css_crg_hardware *crg_hw = NULL;

	crg_hw = get_crg_hw();

	if (!crg_hw)
		return;

 	if (atomic_read(&crg_hw->pd2_user) > 0) {
		css_block_reset_off(BLK_RST_MIPI1);
 	}
	if (atomic_read(&crg_hw->pd1_user) > 0) {
		css_block_reset_off(BLK_RST_MIPI0);
		css_block_reset_off(BLK_RST_MIPI2);
 	}
	if (atomic_read(&crg_hw->pd0_user) > 0) {
		vl6180_proxy_stop_by_pd_off();
		capture_fps_log_deinit();
		css_crg_save();
#ifdef CONFIG_ODIN_ION_SMMU
#ifndef	CONFIG_ODIN_ION_SMMU_4K
		odin_iommu_prohibit_tlb_flush(CSS_DOMAIN);
#else
		odin_iommu_prohibit_tlb_flush(CSS_POWER_DOMAIN);
#endif
#endif
 	}
}

void css_power_domain_resume(void)
{
	struct css_crg_hardware *crg_hw = NULL;

	crg_hw = get_crg_hw();

	if (!crg_hw)
		return;

	if (atomic_read(&crg_hw->pd0_user) > 0) {
		css_bus_control(MEM_BUS,
					CSS_DEFAULT_MEM_BUS_FREQ_KHZ,
					CSS_BUS_TIME_SCHED_NONE);
		/*
		 * Don't modify below cci bus setting.
		 * Green Preview image was caused by 50MHz, 25Mhz cci bus clock.
		 * set the minimum cci bus clock to 100Mhz.
		*/
		css_bus_control(CCI_BUS,
					CSS_DEFAULT_CCI_BUS_FREQ_KHZ,
					CSS_BUS_TIME_SCHED_NONE);
		odin_pd_on(PD_CSS, 0);
		mdelay(3);
		css_block_reset_off(BLK_RESET_PD0|BLK_RESET_PD1|BLK_RESET_PD2|
							BLK_RESET_PD3);
		mdelay(1);
		css_crg_restore();
		mdelay(1);
		css_block_reset_on(BLK_RESET_PD0);
#ifdef CONFIG_ODIN_ION_SMMU
		odin_iommu_wakeup_from_idle(IOMMU_NS_IRQ_CSS_0);
		odin_iommu_wakeup_from_idle(IOMMU_NS_IRQ_CSS_1);
#endif
		capture_fps_log_init();
 	}
	if (atomic_read(&crg_hw->pd1_user) > 0) {
		css_block_reset_off(BLK_RESET_PD1);
		odin_pd_on(PD_CSS, 1);
		css_block_reset_on(BLK_RESET_PD1);
	}
	if (atomic_read(&crg_hw->pd2_user) > 0) {
		css_block_reset_off(BLK_RESET_PD2);
		odin_pd_on(PD_CSS, 2);
		css_block_reset_on(BLK_RESET_PD2);
	}
	if (atomic_read(&crg_hw->pd3_user) > 0) {
		css_block_reset_off(BLK_RESET_PD3);
		odin_pd_on(PD_CSS, 3);
		css_block_reset_on(BLK_RESET_PD3);
 	}
}


static int css_pd_on(int power, int memclkctrl)
{
	int ret = CSS_SUCCESS;
	struct css_crg_hardware *crg_hw = NULL;

	crg_hw = get_crg_hw();

	if (css_power_domain_valid(power)) {
		css_err("Err css power on: 0x%x\n", power);
		return CSS_FAILURE;
	}

	mutex_lock(&crg_hw->mutex);

	css_update_pds_enable(power);

	if (css_pds.pd0.bitmap & power) {
		atomic_inc(&crg_hw->pd0_user);
		if (atomic_read(&crg_hw->pd0_user) == 1) {
			if (memclkctrl) {
				css_bus_control(MEM_BUS,
								CSS_DEFAULT_MEM_BUS_FREQ_KHZ,
								CSS_BUS_TIME_SCHED_NONE);
				/*
				 * Don't modify below cci bus setting.
				 * Green Preview image was caused by 50MHz, 25Mhz cci bus clock.
				 * set the minimum cci bus clock to 100Mhz.
				*/
				css_bus_control(CCI_BUS,
								CSS_DEFAULT_CCI_BUS_FREQ_KHZ,
								CSS_BUS_TIME_SCHED_NONE);
			}

			odin_pd_on(PD_CSS, 0);
			mdelay(3);
			css_block_reset_off(BLK_RESET_PD0|BLK_RESET_PD1|BLK_RESET_PD2|
								BLK_RESET_PD3);
			mdelay(1);
			css_crg_set_on();
			mdelay(1);
			css_block_reset_on(BLK_RESET_PD0);
#ifdef CONFIG_ODIN_ION_SMMU
			odin_iommu_wakeup_from_idle(IOMMU_NS_IRQ_CSS_0);
			odin_iommu_wakeup_from_idle(IOMMU_NS_IRQ_CSS_1);
#endif
			css_init_isp_clock();
			css_set_isp_clock_khz(MAXIMUM_ISP_PERFORMANCE/1000);

			css_info("css pd0_on!!\n");

			css_disable_cpuidle(CSS_BLKS_INT_CPU);
			css_disable_cpuidle(CSS_ISP_INT_CPU);
		}
	}

	if (css_pds.pd1.bitmap & power) {
		atomic_inc(&crg_hw->pd1_user);
		if (atomic_read(&crg_hw->pd1_user) == 1) {
			css_block_reset_off(BLK_RESET_PD1);
			odin_pd_on(PD_CSS, 1);
			css_block_reset_on(BLK_RST_PD_STILL);
			css_info("css pd1_on!!\n");
		}
	}

	if (css_pds.pd2.bitmap & power) {
		atomic_inc(&crg_hw->pd2_user);
		if (atomic_read(&crg_hw->pd2_user) == 1) {
			css_block_reset_off(BLK_RESET_PD2);
			odin_pd_on(PD_CSS, 2);
			css_block_reset_on(BLK_RST_PD_STEREO);
			css_info("css pd2_on!!\n");
		}
	}

	if (css_pds.pd3.bitmap & power) {
		atomic_inc(&crg_hw->pd3_user);
		if (atomic_read(&crg_hw->pd3_user) == 1) {
			css_block_reset_off(BLK_RESET_PD3);
			odin_pd_on(PD_CSS, 3);
			css_info("css pd3_on!!\n");
		}
	}

	if (atomic_read(&crg_hw->pd0_user) == 1) {
		css_info("css pd0(%d) pd1(%d) pd2(%d) pd3(%d)\n",
				atomic_read(&crg_hw->pd0_user),
				atomic_read(&crg_hw->pd1_user),
				atomic_read(&crg_hw->pd2_user),
				atomic_read(&crg_hw->pd3_user));
	} else {
		css_sys("css pd0(%d) pd1(%d) pd2(%d) pd3(%d)\n",
				atomic_read(&crg_hw->pd0_user),
				atomic_read(&crg_hw->pd1_user),
				atomic_read(&crg_hw->pd2_user),
				atomic_read(&crg_hw->pd3_user));
	}

	mutex_unlock(&crg_hw->mutex);

	return ret;
}

static int css_pd_off(int power, int memclkctrl)
{
	int ret = CSS_SUCCESS;
	struct css_crg_hardware *crg_hw = NULL;

	crg_hw = get_crg_hw();

	if (css_power_domain_valid(power)) {
		css_info("Err css power off: 0x%x\n", power);
		return CSS_FAILURE;
	}

	mutex_lock(&crg_hw->mutex);

	if (css_pds.pd3.bitmap & power) {
		atomic_dec(&crg_hw->pd3_user);
		if (atomic_read(&crg_hw->pd3_user) <= 0) {
			atomic_set(&crg_hw->pd3_user, 0);
			css_block_reset_off(BLK_RESET_PD3);
			odin_pd_off(PD_CSS, 3);
			css_info("css pd3_off!!\n");
		}
	}

	if (css_pds.pd2.bitmap & power) {
		atomic_dec(&crg_hw->pd2_user);
		if (atomic_read(&crg_hw->pd2_user) <= 0) {
			atomic_set(&crg_hw->pd2_user, 0);
			css_block_reset_off(BLK_RESET_PD2);
			odin_pd_off(PD_CSS, 2);
			css_info("css pd2_off!!\n");
		}
	}

	if (css_pds.pd1.bitmap & power) {
		atomic_dec(&crg_hw->pd1_user);
		if (atomic_read(&crg_hw->pd1_user) <= 0) {
			atomic_set(&crg_hw->pd1_user, 0);
			css_block_reset_off(BLK_RESET_PD1);
			odin_pd_off(PD_CSS, 1);
			css_info("css pd1_off!!\n");
		}
	}

	if (css_pds.pd0.bitmap & power) {
		atomic_dec(&crg_hw->pd0_user);
		if (atomic_read(&crg_hw->pd0_user) <= 0) {
			atomic_set(&crg_hw->pd0_user, 0);
			vl6180_proxy_stop_by_pd_off();
			css_crg_set_off();
#ifdef CONFIG_ODIN_ION_SMMU
#ifndef	CONFIG_ODIN_ION_SMMU_4K
			odin_iommu_prohibit_tlb_flush(CSS_DOMAIN);
#else
			odin_iommu_prohibit_tlb_flush(CSS_POWER_DOMAIN);
#endif
#endif
			odin_pd_off(PD_CSS, 0);
			css_info("css pd0_off!!\n");

			if (memclkctrl) {
				css_bus_control(MEM_BUS,
							CSS_BUS_FREQ_CONTROL_OFF,
							CSS_BUS_TIME_SCHED_NONE);
				css_bus_control(CCI_BUS,
							CSS_BUS_FREQ_CONTROL_OFF,
							CSS_BUS_TIME_SCHED_NONE);
			}

			css_enable_cpuidle(CSS_BLKS_INT_CPU);
			css_enable_cpuidle(CSS_ISP_INT_CPU);
		}
	}

	css_update_pds_disable(power);

	if (atomic_read(&crg_hw->pd0_user) == 0 &&
		atomic_read(&crg_hw->pd1_user) == 0 &&
		atomic_read(&crg_hw->pd2_user) == 0 &&
		atomic_read(&crg_hw->pd3_user) == 0) {
		css_info("css pd0(%d) pd1(%d) pd2(%d) pd3(%d)\n",
				atomic_read(&crg_hw->pd0_user),
				atomic_read(&crg_hw->pd1_user),
				atomic_read(&crg_hw->pd2_user),
				atomic_read(&crg_hw->pd3_user));
	} else {
		css_sys("css pd0(%d) pd1(%d) pd2(%d) pd3(%d)\n",
				atomic_read(&crg_hw->pd0_user),
				atomic_read(&crg_hw->pd1_user),
				atomic_read(&crg_hw->pd2_user),
				atomic_read(&crg_hw->pd3_user));
	}

	mutex_unlock(&crg_hw->mutex);

	return ret;
}

void css_bus_control(bus_ctrl bus, unsigned int khz, unsigned int usec)
{
	struct css_crg_hardware *crg_hw = NULL;

	crg_hw = get_crg_hw();

	if (bus == MEM_BUS) {
		if (usec > 0) {
			pm_qos_update_request_timeout(&crg_hw->mem_qos, khz, usec);
			css_sys("css mem_bus freq(timeout:%d): %d\n", usec, khz);
		} else {
			pm_qos_update_request(&crg_hw->mem_qos, khz);
			css_sys("css mem_bus freq(static): %d\n", khz);
		}
	} else if (bus == CCI_BUS) {
		if (usec > 0) {
			pm_qos_update_request_timeout(&crg_hw->cci_qos, khz, usec);
			css_sys("css cci_bus freq(timeout:%d): %d\n", usec, khz);
		} else {
			pm_qos_update_request(&crg_hw->cci_qos, khz);
			css_sys("css cci_bus freq(static): %d\n", khz);
		}
	}
}

unsigned int css_crg_clk_info(void)
{
	unsigned long flags = 0;
	unsigned int clks = 0;
	struct css_crg_hardware *crg_hw = NULL;

	crg_hw = get_crg_hw();

	if (crg_hw == NULL || crg_hw->iobase == NULL) {
		return 0;
	}

	spin_lock_irqsave(&crg_hw->slock, flags);
	clks = readl(crg_hw->iobase + CSS_SYS_CLK_CTRL);
	spin_unlock_irqrestore(&crg_hw->slock, flags);

	return clks;
}

static void css_mem_deepsleep_on(void)
{
	unsigned long flags = 0;
	struct css_crg_hardware *crg_hw = NULL;

	crg_hw = get_crg_hw();

	if (crg_hw && crg_hw->iobase != NULL) {
		spin_lock_irqsave(&crg_hw->slock, flags);
		writel(0x000001FF, crg_hw->iobase + CSS_SYS_MEM_DS_CTRL);
		spin_unlock_irqrestore(&crg_hw->slock, flags);
	}
}

static void css_mem_deepsleep_off(void)
{
	unsigned long flags = 0;
	struct css_crg_hardware *crg_hw = NULL;

	crg_hw = get_crg_hw();

	if (crg_hw && crg_hw->iobase != NULL) {
		spin_lock_irqsave(&crg_hw->slock, flags);
		writel(0x00000000, crg_hw->iobase + CSS_SYS_MEM_DS_CTRL);
		spin_unlock_irqrestore(&crg_hw->slock, flags);
	}
}

static int css_crg_set_on(void)
{
	int ret = CSS_SUCCESS;
	unsigned long flags = 0;
	struct css_crg_hardware *crg_hw = NULL;

	crg_hw = get_crg_hw();

	if (crg_hw && crg_hw->iobase != NULL) {
		/* css_block_reset(BLK_RESET_ALL); */
		spin_lock_irqsave(&crg_hw->slock, flags);
		/* writel(0xffffffff, crg_hw->iobase + CSS_SYS_CLK_CTRL); */
		writel(0x00040000, crg_hw->iobase + CSS_SYS_MIPI_CLK_CTRL);
		writel(0x00000007, crg_hw->iobase + CSS_SYS_AXI_CLK_CTRL);
		writel(0x00000005, crg_hw->iobase + CSS_SYS_ISP_CLK_CTRL);
		writel(0x00000005, crg_hw->iobase + CSS_SYS_VNR_CLK_CTRL);
		/* change fpd clk source from ISP to AXI, and div 2 */
		writel(0x00000017, crg_hw->iobase + CSS_SYS_FPD_CLK_CTRL);
		/* change fd clk source from ISP to AXI, and div 2 */
		writel(0x00000017, crg_hw->iobase + CSS_SYS_FD_CLK_CTRL);
		writel(0x00020002, crg_hw->iobase + CSS_SYS_PIX_CLK_CTRL);
		spin_unlock_irqrestore(&crg_hw->slock, flags);

		css_mem_deepsleep_off();
	} else {
		ret = CSS_FAILURE;
	}

	return ret;
}

static int css_crg_set_off(void)
{
	int ret = CSS_SUCCESS;
	unsigned long flags = 0;
	struct css_crg_hardware *crg_hw = NULL;

	crg_hw = get_crg_hw();
	if (crg_hw && crg_hw->iobase != NULL) {
		spin_lock_irqsave(&crg_hw->slock, flags);
		writel(0x000000F8, crg_hw->iobase + CSS_SYS_CLK_CTRL);
		spin_unlock_irqrestore(&crg_hw->slock, flags);
		css_mem_deepsleep_on();
	} else {
		ret = CSS_FAILURE;
	}

	return ret;
}

static void css_crg_save(void)
{
	struct css_crg_hardware *crg_hw = NULL;
	int i;
	unsigned int flags = 0;

	crg_hw = get_crg_hw();

	if (crg_hw == NULL || crg_hw->iobase == NULL) {
		return;
	}
	spin_lock_irqsave(&crg_hw->slock, flags);
	for (i = 0; i < ARRAY_SIZE(crg_store); i++) {
		crg_store[i].value = readl(crg_hw->iobase + crg_store[i].crgregs);
	}
	spin_unlock_irqrestore(&crg_hw->slock, flags);
	css_mem_deepsleep_on();
}

static void css_crg_restore(void)
{
	struct css_crg_hardware *crg_hw = NULL;
	int i;
	unsigned int flags = 0;

	crg_hw = get_crg_hw();

	if (crg_hw == NULL || crg_hw->iobase == NULL) {
		return;
	}
	spin_lock_irqsave(&crg_hw->slock, flags);
	for (i = 0; i < ARRAY_SIZE(crg_store); i++) {
		writel(crg_store[i].value, crg_hw->iobase + crg_store[i].crgregs);
	}
	spin_unlock_irqrestore(&crg_hw->slock, flags);
	css_mem_deepsleep_off();
}


static void css_crg_reg_dump(void)
{
	int i;
	struct css_device *cssdev = get_css_plat();
	struct css_crg_hardware *crg_hw = get_crg_hw();

	if (cssdev == NULL)
		return;

	if (crg_hw == NULL)
		return;

	css_info("crg reg dump\n");
	for (i = 0; i < 4; i++) {
		printk("0x%08x ", readl(crg_hw->iobase + (i * 0x10)));
		printk("0x%08x ", readl(crg_hw->iobase + (i * 0x10) + 0x4));
		printk("0x%08x ", readl(crg_hw->iobase + (i * 0x10) + 0x8));
		printk("0x%08x\n", readl(crg_hw->iobase + (i * 0x10) + 0xC));
		schedule();
	}
}

int css_get_chip_revision(void)
{
	return (system_rev & 0xFF00) >> 8;
}

int odin_css_crg_probe(struct platform_device *pdev)
{
	int ret = CSS_SUCCESS;
	struct css_crg_hardware *crg_hw = NULL;
	struct resource *res_crg = NULL;
	unsigned long size_crg = 0;

	crg_hw = get_crg_hw();

	memset(crg_hw, 0, sizeof(struct css_crg_hardware));

	crg_hw->pdev = pdev;

	res_crg = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (NULL == res_crg) {
		printk("failed to get resource css crg!\n");
		return CSS_ERROR_GET_RESOURCE;
	}
	size_crg = res_crg->end - res_crg->start + 1;

	crg_hw->iores = request_mem_region(res_crg->start, size_crg, pdev->name);
	if (NULL == crg_hw->iores) {
		printk("failed to request mem region css crg : %s!\n",pdev->name);
		return CSS_ERROR_GET_RESOURCE;
	}

	crg_hw->iobase = ioremap_nocache(res_crg->start, size_crg);
	if (NULL == crg_hw->iobase) {
		printk("failed to ioremap css crg!\n");
		ret = CSS_ERROR_IOREMAP;
		goto error_ioremap;
	}

	spin_lock_init(&crg_hw->slock);
	atomic_set(&crg_hw->pd_on, 0);
	atomic_set(&crg_hw->pd0_user, 0);
	atomic_set(&crg_hw->pd1_user, 0);
	atomic_set(&crg_hw->pd2_user, 0);
	atomic_set(&crg_hw->pd3_user, 0);
	mutex_init(&crg_hw->mutex);

	pm_qos_add_request(&crg_hw->mem_qos, PM_QOS_ODIN_MEM_MIN_FREQ, 0);
	pm_qos_add_request(&crg_hw->cci_qos, PM_QOS_ODIN_CCI_MIN_FREQ, 0);

	printk("css crg probe ok!!\n");

	return CSS_SUCCESS;

error_ioremap:
	if (NULL != crg_hw->iores)
		release_mem_region(crg_hw->iores->start,
							crg_hw->iores->end -
							crg_hw->iores->start + 1);

	return ret;
}

int odin_css_crg_remove(struct platform_device *pdev)
{
	int ret = CSS_SUCCESS;
	struct css_crg_hardware *crg_hw = NULL;

	crg_hw = get_crg_hw();

	if (NULL != crg_hw->iobase) {
		iounmap(crg_hw->iobase);
		crg_hw->iobase = NULL;
	}

	if (NULL != crg_hw->iores) {
		release_mem_region(crg_hw->iores->start,
							crg_hw->iores->end -
							crg_hw->iores->start + 1);
		crg_hw->iores = NULL;
	}

	printk("css crg remove ok!!\n");

	return ret;
}

int odin_css_crg_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	return ret;
}

int odin_css_crg_resume(struct platform_device *pdev)
{
	int ret = 0;
	return ret;
}

void odin_css_reg_dump(void)
{
	capture_get_fps_info();
	capture_fps_log(NULL);

	css_crg_reg_dump();
	odin_isp_warp_reg_dump(0);
	odin_isp_warp_reg_dump(1);
	printk("ISP[0/1]: %d/%d\n", odin_isp_get_vsync_count(0),
			odin_isp_get_vsync_count(1));
	scaler_reg_dump();
	printk("ISP[0/1]: %d/%d\n", odin_isp_get_vsync_count(0),
			odin_isp_get_vsync_count(1));
	odin_mipicsi_reg_dump();

	capture_get_fps_info();
	capture_fps_log(NULL);
}

#ifdef CONFIG_OF
static struct of_device_id css_crg_match[] = {
	{ .compatible = CSS_OF_CRG_NAME},
	{},
};
#endif /* CONFIG_OF */

struct platform_driver odin_css_crg_driver =
{
	.probe		= odin_css_crg_probe,
	.remove		= odin_css_crg_remove,
	.suspend	= odin_css_crg_suspend,
	.resume		= odin_css_crg_resume,
	.driver		= {
		.name	= CSS_PLATDRV_CRG,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(css_crg_match),
#endif
	},
};

static int __init odin_css_crg_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&odin_css_crg_driver);

	return ret;
}

static void __exit odin_css_crg_exit(void)
{
	platform_driver_unregister(&odin_css_crg_driver);
	return;
}

#ifdef CONFIG_ARCH_MVVICTORIA
void odin_css_init(void)
{
	#define CSS_ISP0_BASE		 0x33000000
	#define CSS_ISP1_BASE		 0x33080000

	void __iomem *css_isp0_reg = NULL;
	void __iomem *css_isp1_reg = NULL;

	/*
	 * This is a temporary function.
	 * "reset block and set clk" will have to be replaced
	 *                           with odin's proper function.
	 * "config async-fifo" will be controlled by async-fifo driver.
	 * "config ISP" and "vsync fix bypass" will be controlled by ISP Lib.
	 * "out ISP test pattern" is for test
	 */

	unsigned int preview_width = 3280;
	unsigned int preview_height = 2464;
	unsigned int write_val = 0;

	css_isp0_reg = ioremap(CSS_ISP0_BASE, SZ_256K + SZ_128K);
	css_isp1_reg = ioremap(CSS_ISP1_BASE, SZ_256K + SZ_128K);

	/* config ISP */
	writel(0x00000011, css_isp0_reg + 0x00);
	writel(0x00000001, css_isp0_reg + 0x14);

	/* vsync fix bypass */
	writel(preview_height, css_isp0_reg + 0x38);
	writel(preview_height, css_isp0_reg + 0x10340);
	writel(preview_width, css_isp0_reg + 0x10344);
	writel(0x000, css_isp0_reg + 0x10348);
	writel(0x001, css_isp0_reg + 0x1034C);
	writel(preview_height, css_isp0_reg + 0x200e0);
	writel(preview_width, css_isp0_reg + 0x200e4);
	writel(0x000, css_isp0_reg + 0x200e8);
	writel(0x001, css_isp0_reg + 0x200eC);
	writel(preview_height, css_isp0_reg + 0x500f0);
	writel(preview_width, css_isp0_reg + 0x500f4);
	writel(0x001, css_isp0_reg + 0x500f8);
	writel(0x001, css_isp0_reg + 0x500fC);

	/* out ISP test pattern */
	write_val = 0;
	write_val = ((preview_width) << 16 | preview_height);
	writel(write_val, css_isp0_reg + 0x5C);
	/* writel(0x00000001, css_isp0_reg + 0x40); */		/*enable*/
	writel(0x00000000, css_isp0_reg + 0x40);			/*disable*/

	/* config ISP */
	writel(0x00000011, css_isp1_reg + 0x00);
	writel(0x00000001, css_isp1_reg + 0x14);

	/* vsync fix bypass */
	writel(preview_height, css_isp1_reg + 0x38);
	writel(preview_height, css_isp1_reg + 0x10340);
	writel(preview_width, css_isp1_reg + 0x10344);
	writel(0x000, css_isp1_reg + 0x10348);
	writel(0x001, css_isp1_reg + 0x1034C);
	writel(preview_height, css_isp1_reg + 0x200e0);
	writel(preview_width, css_isp1_reg + 0x200e4);
	writel(0x000, css_isp1_reg + 0x200e8);
	writel(0x001, css_isp1_reg + 0x200eC);
	writel(preview_height, css_isp1_reg + 0x500f0);
	writel(preview_width, css_isp1_reg + 0x500f4);
	writel(0x001, css_isp1_reg + 0x500f8);
	writel(0x001, css_isp1_reg + 0x500fC);

	/* out ISP test pattern */
	write_val = 0;
	write_val = ((preview_width) << 16 | preview_height);
	writel(write_val, css_isp1_reg + 0x5C);
	/* writel(0x00000001, css_isp1_reg + 0x40); */		/*enable*/
	writel(0x00000000, css_isp1_reg + 0x40);			/*disable*/
	/* writel(0x00000001, css_isp1_reg + 0x48); */
}
#endif


MODULE_DESCRIPTION("odin css crg driver");
MODULE_AUTHOR("MTEKVISION<http://www.mtekvision.com>");

late_initcall(odin_css_crg_init);
module_exit(odin_css_crg_exit);

MODULE_LICENSE("GPL");

