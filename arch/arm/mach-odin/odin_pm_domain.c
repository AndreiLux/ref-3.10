/* arch/arm/mach-odin/odin_pm_domain.c
 *
 * ODIN PM domain controller
 *
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/list.h>
#include <linux/suspend.h>
#include <linux/pm_domain.h>
#include <linux/odin_pm_domain.h>
#include <linux/odin_pd.h>
#include <linux/odin_iommu.h>
#include <video/odindss.h>
#include <linux/odin_clk.h>
#include <linux/platform_data/odin_tz.h>
#include <linux/platform_data/gpio-odin.h>

#if defined(CONFIG_MACH_ODIN_LIGER)
#include <linux/odin_pmic.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>

static struct dentry *dbgfs_root, *dbgfs_pdinfo, *dbgfs_cpuinfo;
#endif

#if defined(CONFIG_DEBUG_AUD_RPM) || defined(CONFIG_DEBUG_ICS_RPM)
#define PDMSG	pr_err
#else
#define PDMSG	pr_debug
#endif

static int odin_gpd_power_off(struct generic_pm_domain *gpd);
static int odin_gpd_power_on(struct generic_pm_domain *gpd);

extern void phy_poweron_standby(u16 baseAddr);
extern int odin_crg_dss_clk_ena(enum odin_crg_clk_gate clk_gate,
		u32 clk_gate_val, bool enable);

#ifdef CONFIG_AUD_RPM
struct odin_pm_domain odin_pd_aud0 = {
	.gpd.name = "pd-aud0",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = false,
	.mbox_cmd_id = PD_AUD,
	.pdid = 0x0,
};

struct odin_pm_domain odin_pd_aud1 = {
	.gpd.name = "pd-aud1",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = true,
	.mbox_cmd_id = PD_AUD,
	.pdid = 0x1,
};

struct odin_pm_domain odin_pd_aud2 = {
	.gpd.name = "pd-aud2",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = false,
	.mbox_cmd_id = PD_AUD,
	.pdid = 0x2,
};

struct odin_pm_domain odin_pd_aud2_i2c = {
	.gpd.name = "pd-aud2-i2c",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = false,
	.mbox_cmd_id = 0xffffffff,
	.pdid = 0x40,
};

struct odin_pm_domain odin_pd_aud2_si2s0 = {
	.gpd.name = "pd-aud2-si2s0",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = true,
	.mbox_cmd_id = 0xffffffff,
	.pdid = 0x40,
};

struct odin_pm_domain odin_pd_aud2_si2s1 = {
	.gpd.name = "pd-aud2-si2s1",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = true,
	.mbox_cmd_id = 0xffffffff,
	.pdid = 0x40,
};

struct odin_pm_domain odin_pd_aud2_si2s2 = {
	.gpd.name = "pd-aud2-si2s2",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = true,
	.mbox_cmd_id = 0xffffffff,
	.pdid = false,
};

struct odin_pm_domain odin_pd_aud2_si2s3 = {
	.gpd.name = "pd-aud2-si2s3",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = true,
	.mbox_cmd_id = 0xffffffff,
	.pdid = false,
};

struct odin_pm_domain odin_pd_aud2_mi2s = {
	.gpd.name = "pd-aud2-mi2s",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = false,
	.mbox_cmd_id = 0xffffffff,
	.pdid = 0x40,
};
#endif

struct odin_pm_domain odin_pd_vsp0 = {
	.gpd.name = "pd-vsp0",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = false,
	.mbox_cmd_id = PD_VSP,
	.pdid = 0x0,
};

struct odin_pm_domain odin_pd_vsp1 = {
	.gpd.name = "pd-vsp1",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = true,
	.mbox_cmd_id = PD_VSP,
	.pdid = 0x1,
};

struct odin_pm_domain odin_pd_vsp2 = {
	.gpd.name = "pd-vsp2",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = true,
	.mbox_cmd_id = PD_VSP,
	.pdid = 0x2,
};

struct odin_pm_domain odin_pd_vsp3 = {
	.gpd.name = "pd-vsp3",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = true,
	.mbox_cmd_id = PD_VSP,
	.pdid = 0x3,
};

struct odin_pm_domain odin_pd_vsp4 = {
	.gpd.name = "pd-vsp4",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = true,
	.mbox_cmd_id = PD_VSP,
	.pdid = 0x4,
};

struct odin_pm_domain odin_pd_vsp4_jpeg = {
	.gpd.name = "pd-vsp4",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = true,
	.mbox_cmd_id = 0xffffffff,
	.pdid = 0x40,
};

struct odin_pm_domain odin_pd_vsp4_png = {
	.gpd.name = "pd-vsp4",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = true,
	.mbox_cmd_id = 0xffffffff,
	.pdid = 0x41,
};

struct odin_pm_domain odin_pd_dss0 = {
	.gpd.name = "pd-dss0",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = false,
	.mbox_cmd_id = PD_DSS,
	.pdid = 0x0,
};

struct odin_pm_domain odin_pd_dss1 = {
	.gpd.name = "pd-dss1",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = false,
	.mbox_cmd_id = PD_DSS,
	.pdid = 0x1,
};

struct odin_pm_domain odin_pd_dss2 = {
	.gpd.name = "pd-dss2",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = true,
	.mbox_cmd_id = PD_DSS,
	.pdid = 0x2,
};

struct odin_pm_domain odin_pd_dss3 = {
	.gpd.name = "pd-dss3",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = false,
	.mbox_cmd_id = PD_DSS,
	.pdid = 0x3,
};

struct odin_pm_domain odin_pd_dss4 = {
	.gpd.name = "pd-dss4",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
#ifdef  CONFIG_ODIN_DSS_NEW
	.is_off = false,
#else
	.is_off = true,
#endif
	.mbox_cmd_id = PD_DSS,
	.pdid = 0x4,
};

/* temp fix for HDMI begin */
struct odin_pm_domain odin_pd_dss3_lcd0_panel = {
	.gpd.name = "pd-dss3-lcd0-panel",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = false,
	.mbox_cmd_id = 0xffffffff,
	.pdid = 0x30,
};

struct odin_pm_domain odin_pd_dss3_lcd1_panel = {
	.gpd.name = "pd-dss3-lcd1-panel",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = false,
	.mbox_cmd_id = 0xffffffff,
	.pdid = 0x30,
};

struct odin_pm_domain odin_pd_dss3_hdmi_panel = {
	.gpd.name = "pd-dss3-hdmi-panel",
	.gpd.power_off = odin_gpd_power_off,
	.gpd.power_on = odin_gpd_power_on,
	.is_off = true,
	.mbox_cmd_id = 0xffffffff,
	.pdid = 0x31,
};
/* temp fix for HDMI end */

struct odin_pm_domain *odin_pm_domains[] = {
#ifdef CONFIG_AUD_RPM
	&odin_pd_aud0,
	&odin_pd_aud1,
	&odin_pd_aud2,
	&odin_pd_aud2_i2c,
	&odin_pd_aud2_si2s0,
	&odin_pd_aud2_si2s1,
	&odin_pd_aud2_si2s2,
	&odin_pd_aud2_si2s3,
	&odin_pd_aud2_mi2s,
#endif
	&odin_pd_vsp0,
	&odin_pd_vsp1,
	&odin_pd_vsp2,
	&odin_pd_vsp3,
	&odin_pd_vsp4,
	&odin_pd_vsp4_jpeg,
	&odin_pd_vsp4_png,
	&odin_pd_dss0,
	&odin_pd_dss1,
	&odin_pd_dss2,
	&odin_pd_dss3,
	&odin_pd_dss4,
/* temp fix for HDMI begin */
	&odin_pd_dss3_lcd0_panel,
	&odin_pd_dss3_lcd1_panel,
	&odin_pd_dss3_hdmi_panel,
/* temp fix for HDMI end */
};

#define CRG_OP_DELAY	1
#define CRG_OP_RDWR	2

struct pd_crg_reg {
	u32 op;
	u32 offset;
	u32 value;
};

#ifdef CONFIG_AUD_RPM
static struct pd_crg_reg aud_clk_save[] = {
	{ CRG_OP_RDWR, 0x0000, 0x0 },
	{ CRG_OP_RDWR, 0x0004, 0x0 },
	{ CRG_OP_RDWR, 0x0008, 0x0 },
	{ CRG_OP_RDWR, 0x000c, 0x0 },
	{ CRG_OP_RDWR, 0x0010, 0x0 },
	{ CRG_OP_RDWR, 0x0014, 0x0 },
	{ CRG_OP_RDWR, 0x0018, 0x0 },
	{ CRG_OP_RDWR, 0x001c, 0x0 },
	{ CRG_OP_RDWR, 0x0020, 0x0 },
	{ CRG_OP_RDWR, 0x0024, 0x0 },
	{ CRG_OP_RDWR, 0x0028, 0x0 },
	{ CRG_OP_RDWR, 0x002c, 0x0 },
	{ CRG_OP_RDWR, 0x0030, 0x0 },
	{ CRG_OP_RDWR, 0x0034, 0x0 },
	{ CRG_OP_RDWR, 0x0038, 0x0 },
	{ CRG_OP_RDWR, 0x003c, 0x0 },
	{ CRG_OP_RDWR, 0x0040, 0x0 },
	{ CRG_OP_RDWR, 0x0044, 0x0 },
	{ CRG_OP_RDWR, 0x0048, 0x0 },
	{ CRG_OP_RDWR, 0x004c, 0x0 },
	{ CRG_OP_RDWR, 0x0050, 0x0 },
	{ CRG_OP_RDWR, 0x0054, 0x0 },
	{ CRG_OP_RDWR, 0x0058, 0x0 },
	{ CRG_OP_RDWR, 0x005c, 0x0 },
	{ CRG_OP_RDWR, 0x0060, 0x0 },
	{ CRG_OP_RDWR, 0x0064, 0x0 },
	{ CRG_OP_RDWR, 0x0068, 0x0 },
	{ CRG_OP_RDWR, 0x006c, 0x0 },
	{ CRG_OP_RDWR, 0x0070, 0x0 },
	{ CRG_OP_RDWR, 0x0074, 0x0 },
	{ CRG_OP_RDWR, 0x0078, 0x0 },
	{ CRG_OP_RDWR, 0x007c, 0x0 },
	{ CRG_OP_RDWR, 0x0080, 0x0 },
	{ CRG_OP_RDWR, 0x0084, 0x0 },
	{ CRG_OP_RDWR, 0x0088, 0x0 },
	{ CRG_OP_RDWR, 0x008c, 0x0 },
	{ CRG_OP_RDWR, 0x0090, 0x0 },
	{ CRG_OP_RDWR, 0x0094, 0x0 },
	{ CRG_OP_RDWR, 0x0098, 0x0 },
	{ CRG_OP_RDWR, 0x009c, 0x0 },
	{ CRG_OP_RDWR, 0x00a0, 0x0 },
	{ CRG_OP_RDWR, 0x00a4, 0x0 },
	{ CRG_OP_RDWR, 0x00a8, 0x0 },
	{ CRG_OP_RDWR, 0x00ac, 0x0 },
};
#endif

static struct pd_crg_reg vsp0_clk_save[] = {
	{ CRG_OP_RDWR, 0x0200, 0x0 },
	{ CRG_OP_RDWR, 0x0204, 0x0 },
	{ CRG_OP_RDWR, 0x0208, 0x0 },
	{ CRG_OP_RDWR, 0x020c, 0x0 },
	{ CRG_OP_RDWR, 0x021c, 0x0 },
	{ CRG_OP_RDWR, 0x0240, 0x0 },
	{ CRG_OP_RDWR, 0x0244, 0x0 },
	{ CRG_OP_RDWR, 0x0248, 0x0 },
	{ CRG_OP_RDWR, 0x024c, 0x0 },
	{ CRG_OP_RDWR, 0x0250, 0x0 },
	{ CRG_OP_RDWR, 0x0254, 0x0 },
	{ CRG_OP_RDWR, 0x0258, 0x0 },
	{ CRG_OP_RDWR, 0x025c, 0x0 },
};

static struct pd_crg_reg vsp1_clk_save[] = {
	{ CRG_OP_RDWR, 0x0220, 0x0 },
	{ CRG_OP_RDWR, 0x0224, 0x0 },
	{ CRG_OP_RDWR, 0x0228, 0x0 },
	{ CRG_OP_RDWR, 0x022c, 0x0 },
	{ CRG_OP_RDWR, 0x0230, 0x0 },
	{ CRG_OP_RDWR, 0x0234, 0x0 },
	{ CRG_OP_RDWR, 0x0238, 0x0 },
	{ CRG_OP_RDWR, 0x023c, 0x0 },
	{ CRG_OP_RDWR, 0x0240, 0x0 },
	{ CRG_OP_RDWR, 0x0244, 0x0 },
	{ CRG_OP_RDWR, 0x0248, 0x0 },
	{ CRG_OP_RDWR, 0x024c, 0x0 },
	{ CRG_OP_RDWR, 0x0250, 0x0 },
	{ CRG_OP_RDWR, 0x0254, 0x0 },
	{ CRG_OP_RDWR, 0x0258, 0x0 },
	{ CRG_OP_RDWR, 0x025c, 0x0 },
};

static struct pd_crg_reg dss_clk_save[] = {
	{ CRG_OP_RDWR, 0x0000, 0x0 }, /* DSS_CORE_CLK_SEL */
	{ CRG_OP_RDWR, 0x0004, 0x0 }, /* DSS_CORE_CLK_ENABLE */
	{ CRG_OP_RDWR, 0x0008, 0x0 }, /* DSS_OTHER_CONTROL */
	{ CRG_OP_RDWR, 0x000c, 0x0 }, /* DSS_CLK_DIV_VAL_0 */
	{ CRG_OP_RDWR, 0x0010, 0x0 }, /* DSS_CLK_DIV_VAL_1 */
	{ CRG_OP_RDWR, 0x3000, 0x0 }, /* DSS_MEMORY_DEEP_SLEEP_MODE_CONTROL */
};

static struct pd_crg_reg dss_irq_save[] = {
	{ CRG_OP_RDWR, 0x0, 0x0 }, /* DSS_NONSECURE_INTERRUPT_MASK */
};
/**
 * access funcs for 'secure' CRG registers
 */

/* FIXME: 32bit vs 64bit */
static inline void scrg_writel(u32 base, u32 offset, u32 val)
{
#ifdef CONFIG_ODIN_TEE
	/* FIXME: check implementation of __raw_writel()
	 * normally, reg_write() don't check rc.
	*/
	(void)tz_write(val, base + offset);
#else
	void __iomem *va_base;

	/* FIXME: find out max size of CRG, and change SZ_64K to max. */
	va_base = ioremap(base, SZ_64K);
	BUG_ON(!va_base);
	__raw_writel(val, va_base + offset);

	iounmap(va_base);
#endif
}

/* FIXME: 32bit vs 64bit */
static inline u32 scrg_readl(u32 base, u32 offset)
{
	u32 r;

#ifdef CONFIG_ODIN_TEE
	r = tz_read(base + offset);
#else
	void __iomem *va_base;

	/* FIXME: find out max size of CRG, and change SZ_64K to max. */
	va_base = ioremap(base, SZ_64K);
	BUG_ON(!va_base);
	r = __raw_readl(va_base + offset);

	iounmap(va_base);
#endif

	return r;
}

static int store_crg_regs(u32 base_addr, struct pd_crg_reg *rs,
	int count, size_t size, u8 secure_flag)
{
	void __iomem *va_base = NULL;

	BUG_ON(!rs);

#ifdef CONFIG_ODIN_TEE
	if (secure_flag == NON_SECURE) {
		va_base = ioremap(base_addr, size);
		BUG_ON(!va_base);
	}

	for ( ; count > 0; count--, rs++) {
		if (rs->op == CRG_OP_DELAY) {
			udelay(rs->value);
			continue;
		}
		if (secure_flag)
			rs->value = tz_read(base_addr + rs->offset);
		else
			rs->value = __raw_readl(va_base + rs->offset);
	}

	if (secure_flag == NON_SECURE)
		iounmap(va_base);
#else /* !CONFIG_ODIN_TEE */
	va_base = ioremap(base_addr, size);
	BUG_ON(!va_base);

	for ( ; count > 0; count--, rs++) {
		if (rs->op == CRG_OP_DELAY) {
			udelay(rs->value);
			continue;
		}
		rs->value = __raw_readl(va_base + rs->offset);
	}

	iounmap(va_base);
#endif

	return 0;
}

static int restore_crg_regs(u32 base_addr, struct pd_crg_reg *rs,
	int count, size_t size, u8 secure_flag)
{
	void __iomem *va_base = NULL;

	BUG_ON(!rs);

#ifdef CONFIG_ODIN_TEE
	if (secure_flag == NON_SECURE) {
		va_base = ioremap(base_addr, size);
		BUG_ON(!va_base);
	}

	for ( ; count > 0; count--, rs++) {
		if (rs->op == CRG_OP_DELAY) {
			udelay(rs->value);
			continue;
		}
		if (secure_flag)
			tz_write(rs->value, base_addr + rs->offset);
		else
			__raw_writel(rs->value, va_base + rs->offset);
	}

	if (secure_flag == NON_SECURE)
		iounmap(va_base);
#else /* !CONFIG_ODIN_TEE */
	va_base = ioremap(base_addr, size);
	BUG_ON(!va_base);

	for ( ; count > 0; count--, rs++) {
		if (rs->op == CRG_OP_DELAY) {
			udelay(rs->value);
			continue;
		}
		__raw_writel(rs->value, va_base + rs->offset);
	}

	iounmap(va_base);
#endif

	return 0;
}

static void pd_clk_enable(char *name)
{
	struct clk *clk;
	/* FIXME, NULL -> meaningful word */
	clk = clk_get(NULL, name);
	if (clk)
		clk_prepare_enable(clk);
	else
		pr_err("%s: failed to get clk %s\n", __func__, name);
}

static void pd_clk_disable(char *name)
{
	struct clk *clk;
	/* FIXME, NULL -> meaningful word */
	clk = clk_get(NULL, name);
	if (clk)
		clk_disable_unprepare(clk);
	else
		pr_err("%s: failed to get clk %s\n", __func__, name);
}

int odin_pd_power(struct odin_pm_domain *pd, bool on)
{
	void __iomem *va_addr;
	u32 reset_rd, reset_wr, reset_mask;
	int rc = 0;

	PDMSG("PDMSG %s: power %s %s\n", __func__, on ? "on" : "off",
		pd->gpd.name);

	if (pd->mbox_cmd_id == 0xffffffff)
		return 0;

	/* FIXME: poll registers via SMS */
	if (on) {
		if (pd->mbox_cmd_id == PD_DSS && pd->pdid == 1) {
			reset_rd = scrg_readl(ODIN_DSS_CRG_BASE, 0x1000);
			reset_mask = VSCL0_RESET | VSCL1_RESET | VDMA_RESET | FORMATTER_RESET;
			reset_wr = reset_rd | reset_mask;
			scrg_writel(ODIN_DSS_CRG_BASE, 0x1000, reset_wr);
			mdelay(1);
		}
		if (pd->mbox_cmd_id == PD_DSS && pd->pdid == 2) {
			reset_rd = scrg_readl(ODIN_DSS_CRG_BASE, 0x1000);
			reset_mask = MXD_RESET;
			reset_wr = reset_rd | reset_mask;
			scrg_writel(ODIN_DSS_CRG_BASE, 0x1000, reset_wr);
			mdelay(1);
		}
		if (pd->mbox_cmd_id == PD_DSS && pd->pdid == 3) {
			reset_rd = scrg_readl(ODIN_DSS_CRG_BASE, 0x1000);
			reset_mask = CABC_RESET | GDMA0_RESET | GDMA1_RESET | GDMA2_RESET |
				OVERLAY_RESET | GSCL_RESET | ROTATOR_RESET |
				SYNCGEN_RESET | DIP0_RESET | MIPI0_RESET |
				DIP1_RESET | MIPI1_RESET | DIP2_RESET |
				HDMI_RESET;
			reset_wr = reset_rd | reset_mask;
			scrg_writel(ODIN_DSS_CRG_BASE, 0x1000, reset_wr);
			mdelay(1);
		}
		if (pd->mbox_cmd_id == PD_DSS && pd->pdid == 4) {
			reset_rd = scrg_readl(ODIN_DSS_CRG_BASE, 0x1000);
			reset_mask = GFX0_RESET | GFX1_RESET;
			reset_wr = reset_rd | reset_mask;
			scrg_writel(ODIN_DSS_CRG_BASE, 0x1000, reset_wr);
			mdelay(1);
		}

		rc = odin_pd_on(pd->mbox_cmd_id, pd->pdid);
		if (rc < 0) {
			/* FIXME: iounmap if DSS PDx */
			return rc;
		}

#ifdef CONFIG_AUD_RPM
		if (pd->mbox_cmd_id == PD_AUD && pd->pdid == 1) {
			reset_rd = scrg_readl(ODIN_AUD_CRG_BASE, SWRESET_AUD1);
			reset_mask = 0x0000000f;
			reset_wr = reset_rd | reset_mask;
			scrg_writel(ODIN_AUD_CRG_BASE, SWRESET_AUD1, reset_wr);
		}
		if (pd->mbox_cmd_id == PD_AUD && pd->pdid == 2) {
			reset_rd = scrg_readl(ODIN_AUD_CRG_BASE, SWRESET_AUD1);
			reset_mask = 0xfffffff0;
			reset_wr = reset_rd | reset_mask;
			scrg_writel(ODIN_AUD_CRG_BASE, SWRESET_AUD1, reset_wr);
		}
#endif

		if (pd->mbox_cmd_id == PD_DSS && pd->pdid == 1) {
			mdelay(1); /* FIXME: mbox latency??? */
			reset_wr = reset_rd & (~reset_mask);
			scrg_writel(ODIN_DSS_CRG_BASE, 0x1000, reset_wr);
		}
		if (pd->mbox_cmd_id == PD_DSS && pd->pdid == 2) {
			mdelay(1); /* FIXME: mbox latency??? */
			reset_wr = reset_rd & (~reset_mask);
			scrg_writel(ODIN_DSS_CRG_BASE, 0x1000, reset_wr);
		}
		if (pd->mbox_cmd_id == PD_DSS && pd->pdid == 3) {
			mdelay(1); /* FIXME: mbox latency??? */
			reset_wr = reset_rd & (~reset_mask);
			scrg_writel(ODIN_DSS_CRG_BASE, 0x1000, reset_wr);
#if 1
			pd_clk_enable("disp0_clk");
			pd_clk_enable("hdmi_disp_clk");

			odin_crg_dss_clk_ena(CRG_CORE_CLK,
					    (MXD_CLK | DIP2_CLK | HDMI_CLK
				| OVERLAY_CLK | SYNCGEN_CLK), true);
			odin_crg_dss_clk_ena(CRG_OTHER_CLK, (SFR_CLK) , true);

			phy_poweron_standby(0);

			odin_crg_dss_clk_ena(CRG_CORE_CLK,
					     (MXD_CLK | DIP2_CLK | HDMI_CLK),
					     false);
			odin_crg_dss_clk_ena(CRG_OTHER_CLK, (SFR_CLK), false);

			pd_clk_disable("hdmi_disp_clk");
			pd_clk_disable("disp0_clk");

#endif
		}
		if (pd->mbox_cmd_id == PD_DSS && pd->pdid == 4) {
			mdelay(1); /* FIXME: mbox latency??? */
			reset_wr = reset_rd & (~reset_mask);
			scrg_writel(ODIN_DSS_CRG_BASE, 0x1000, reset_wr);
		}

		/* enable VSP0 common/encoder part together */
		if (pd->mbox_cmd_id == PD_VSP && pd->pdid == 2) {
//			pd_clk_enable("vsp0_c_codec");

			scrg_writel(ODIN_VSP0_CRG_BASE, 0, 0);
			scrg_writel(ODIN_VSP0_CRG_BASE, 0x0004, 0);
			udelay(100);
			scrg_writel(ODIN_VSP0_CRG_BASE, 0, 1);
			scrg_writel(ODIN_VSP0_CRG_BASE, 0x0004, 1);
		}

		if (pd->mbox_cmd_id == PD_VSP && pd->pdid == 3) {
//			pd_clk_enable("vsp1_dec_clk");

			scrg_writel(ODIN_VSP1_CRG_BASE, 0x0008, 0);
			udelay(100);
			scrg_writel(ODIN_VSP1_CRG_BASE, 0x0008, 1);
		}

		if (pd->mbox_cmd_id == PD_VSP && pd->pdid == 4) {
			pd_clk_enable("vsp1_img_clk");

			scrg_writel(ODIN_VSP1_CRG_BASE, 0x000c, 0);
			udelay(100);
			scrg_writel(ODIN_VSP1_CRG_BASE, 0x000c, 1);
		}

		if (pd->pdid != 0)
			return 0;
		/* FIXME */
		switch(pd->mbox_cmd_id) {
#ifdef CONFIG_AUD_RPM
		case PD_AUD:
			restore_crg_regs(ODIN_AUD_CRG_BASE, aud_clk_save,
				ARRAY_SIZE(aud_clk_save), SZ_4K, NON_SECURE);
			odin_gpio_regs_resume(ODIN_GPIO_AUD_START,
				ODIN_GPIO_AUD_END, ODIN_GPIO_NR);
			odin_iommu_wakeup_from_idle(IOMMU_NS_IRQ_AUD);
			break;
#endif
		case PD_VSP:
			/* FIXME */
			restore_crg_regs(ODIN_VSP0_CRG_BASE, vsp0_clk_save,
				ARRAY_SIZE(vsp0_clk_save), SZ_4K, SECURE);
			/*clk_reg_update(VSP0_BLOCK, SECURE);*/
			/*pd_clk_enable("vsp0_c_codec");*/
			/*pd_clk_enable("vsp0_aclk_clk");*/

			restore_crg_regs(ODIN_VSP1_CRG_BASE, vsp1_clk_save,
				ARRAY_SIZE(vsp1_clk_save), SZ_4K, SECURE);
			/*clk_reg_update(VSP1_BLOCK, SECURE);*/
			/*pd_clk_enable("vsp1_dec_clk");*/
			/*pd_clk_enable("vsp1_img_clk");*/
			/*pd_clk_enable("vsp1_aclk_clk");*/
			/* FIXME: wakeup just after turning on PD0 */
			odin_iommu_wakeup_from_idle(IOMMU_NS_IRQ_VSP_0);
			odin_iommu_wakeup_from_idle(IOMMU_NS_IRQ_VSP_1);
			break;
		case PD_DSS:
#ifndef CONFIG_ODIN_DSS_NEW
			pd_clk_enable("overlay_clk");

			scrg_writel(ODIN_DSS_CRG_BASE, 0, 1);
#endif
			odin_iommu_wakeup_from_idle(IOMMU_NS_IRQ_DSS_0);
			odin_iommu_wakeup_from_idle(IOMMU_NS_IRQ_DSS_1);
			odin_iommu_wakeup_from_idle(IOMMU_NS_IRQ_DSS_2);
			odin_iommu_wakeup_from_idle(IOMMU_NS_IRQ_DSS_3);

			/*va_addr = ioremap(ODIN_DSS_CRG_BASE, SZ_16K);*/
			restore_crg_regs(ODIN_DSS_CRG_BASE, dss_clk_save,
				ARRAY_SIZE(dss_clk_save), SZ_16K, SECURE);

			restore_crg_regs(ODIN_DSS_CRG_BASE + 0xf804,
					dss_irq_save, ARRAY_SIZE(dss_irq_save),
						SZ_32, NON_SECURE);
#ifdef DSS_PD_CRG_DUMP_LOG
			printk("==> restore)mask:0x%x, core:0x%x, other:0x%x\n, ",
				dss_irq_save[0].value,
				dss_clk_save[1].value, dss_clk_save[2].value);
#endif

			scrg_writel(ODIN_DSS_CRG_BASE, 0x0014, 0x40);
			udelay(100);
			scrg_writel(ODIN_DSS_CRG_BASE, 0x0014, 0);

			break;
		default:
			/* FIXME: off PD, return err */
			break;
		}

	} else {
		if (pd->mbox_cmd_id == PD_DSS && pd->pdid == 1) {
			reset_rd = scrg_readl(ODIN_DSS_CRG_BASE, 0x1000);
			reset_mask = VSCL0_RESET | VSCL1_RESET | VDMA_RESET | FORMATTER_RESET;
			reset_wr = reset_rd | reset_mask;
			scrg_writel(ODIN_DSS_CRG_BASE, 0x1000, reset_wr);
		}

		if (pd->mbox_cmd_id == PD_DSS && pd->pdid == 2) {
			reset_rd = scrg_readl(ODIN_DSS_CRG_BASE, 0x1000);
			reset_mask = MXD_RESET;
			reset_wr = reset_rd | reset_mask;
			scrg_writel(ODIN_DSS_CRG_BASE, 0x1000, reset_wr);
		}

		if (pd->mbox_cmd_id == PD_DSS && pd->pdid == 3) {
			reset_rd = scrg_readl(ODIN_DSS_CRG_BASE, 0x1000);
			reset_mask = CABC_RESET | GDMA0_RESET | GDMA1_RESET | GDMA2_RESET |
				OVERLAY_RESET | GSCL_RESET | ROTATOR_RESET |
				SYNCGEN_RESET | DIP0_RESET | MIPI0_RESET |
				DIP1_RESET | MIPI1_RESET | DIP2_RESET |
				HDMI_RESET;
			reset_wr = reset_rd | reset_mask;
			scrg_writel(ODIN_DSS_CRG_BASE, 0x1000, reset_wr);
		}

		if (pd->pdid == 0) {
			switch(pd->mbox_cmd_id) {
#ifdef CONFIG_AUD_RPM
			case PD_AUD:
#ifndef	CONFIG_ODIN_ION_SMMU_4K
				odin_iommu_prohibit_tlb_flush(AUD_DOMAIN);
#else
				odin_iommu_prohibit_tlb_flush(AUD_POWER_DOMAIN);
#endif
				odin_gpio_regs_suspend(ODIN_GPIO_AUD_START,
					ODIN_GPIO_AUD_END, ODIN_GPIO_NR);
				store_crg_regs(ODIN_AUD_CRG_BASE,
					aud_clk_save,
					ARRAY_SIZE(aud_clk_save),
					SZ_4K, NON_SECURE);
				break;
#endif
			case PD_VSP:
#ifndef	CONFIG_ODIN_ION_SMMU_4K
				odin_iommu_prohibit_tlb_flush(VSP_DOMAIN);
#else
				odin_iommu_prohibit_tlb_flush(VSP_POWER_DOMAIN);
#endif

				/*pd_clk_disable("vsp0_c_codec");*/
				/*pd_clk_disable("vsp0_aclk_clk");*/
				store_crg_regs(ODIN_VSP0_CRG_BASE,
					vsp0_clk_save,
					ARRAY_SIZE(vsp0_clk_save),
					SZ_4K, SECURE);

				/*pd_clk_disable("vsp1_dec_clk");*/
				/*pd_clk_disable("vsp1_img_clk");*/
				/*pd_clk_disable("vsp1_aclk_clk");*/
				store_crg_regs(ODIN_VSP1_CRG_BASE,
					vsp1_clk_save,
					ARRAY_SIZE(vsp1_clk_save),
					SZ_4K, SECURE);
				break;
			case PD_DSS:
#ifndef	CONFIG_ODIN_ION_SMMU_4K
				odin_iommu_prohibit_tlb_flush(DSS_DOMAIN);
#else
				odin_iommu_prohibit_tlb_flush(DSS_POWER_DOMAIN);
#endif
				store_crg_regs(ODIN_DSS_CRG_BASE,
					dss_clk_save,
					ARRAY_SIZE(dss_clk_save),
					SZ_16K, SECURE);

				store_crg_regs(ODIN_DSS_CRG_BASE + 0xf804,
						dss_irq_save,
						ARRAY_SIZE(dss_irq_save),
						SZ_32, NON_SECURE);
#ifdef DSS_PD_CRG_DUMP_LOG
				printk("==> store) mask:0x%x, core:0x%x, other:0x%x\n",
					dss_irq_save[0].value,
					dss_clk_save[1].value, dss_clk_save[2].value);
#endif
#ifndef CONFIG_ODIN_DSS_NEW
				scrg_writel(ODIN_DSS_CRG_BASE, 0, 0);
				pd_clk_disable("overlay_clk");
#endif
				break;
			default:
				/* FIXME: return -EINVAL */
				break;
			}
		}

		rc = odin_pd_off(pd->mbox_cmd_id, pd->pdid);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static int odin_gpd_power_off(struct generic_pm_domain *domain)
{
	struct odin_pm_domain *pd;
	int rc = 0;

	PDMSG("PDMSG %s: power off %s\n", __func__, domain->name);
	pd = container_of(domain, struct odin_pm_domain, gpd);
#if 0
	/* FIXME: poll registers via SMS */
	rc = odin_pd_off(pd->mbox_cmd_id, 0);
	if (rc < 0) {
		pr_err("%s: failed to power off %s, rc=%d\n",
			__func__, domain->name, rc);
		return rc;
	}
#endif
	rc = odin_pd_power(pd, false);
	if (rc)
		pr_err("%s: failed to power off (%d)\n", __func__, rc);

	return 0;
}

static int odin_gpd_power_on(struct generic_pm_domain *domain)
{
	struct odin_pm_domain *pd;
	int rc = 0;

	PDMSG("PDMSG %s: power on %s\n", __func__, domain->name);
	pd = container_of(domain, struct odin_pm_domain, gpd);
#if 0
	/* FIXME: poll registers via SMS */
	rc = odin_pd_on(pd->mbox_cmd_id, 0);
	if (rc < 0) {
		pr_err("%s: failed to power on %s, rc=%d\n",
			__func__, domain->name, rc);
		return rc;
	}
#endif
	rc = odin_pd_power(pd, true);
	if (rc)
		pr_err("%s: failed to power on (%d)\n", __func__, rc);

	return 0;
}

int odin_pd_register_dev(struct device *dev,
	struct odin_pm_domain *pd)
{
	int rc = 0;

	if (!dev || !pd)
		return -EINVAL;

	rc = pm_genpd_add_device(&pd->gpd, dev);
	if (rc < 0) {
		pr_err("%s: failed to register %s to %s, rc=%d\n", __func__,
			dev_name(dev), pd->gpd.name, rc);
		return rc;
	}

	PDMSG("PDMSG %s: registered %s to %s\n", __func__,
		dev_name(dev), pd->gpd.name);
	return 0;
}

static int odin_pd_pm_event_handler(struct notifier_block *nb,
	unsigned long event, void *unused)
{
	PDMSG("PDMSG %s: received PM event=%lu\n", __func__, event);

	switch (event) {
	case PM_SUSPEND_PREPARE:
		break;
	case PM_POST_SUSPEND:
		break;
	}

	return NOTIFY_DONE;
}

#if defined(CONFIG_MACH_ODIN_LIGER)

struct dbg_pd_reg {
	char name[32];
	unsigned int nRegister;
};

static struct dbg_pd_reg pd_list[] = {
	{"CA15",	0x200e0018},
	{"CA7",		0x200e0038},
	{"GPU2",	0x200f1168},
	{"GPU1",	0x200f1164},
	{"GPU0",	0x200f1160},
	{"CBUS",	0x200f1000},
	{"CPU_PD",	0x200f1100},

	{"VSP4",	0x200f1190},
	{"VSP3",	0x200f118C},
	{"VSP2",	0x200f1188},
	{"VSP1",	0x200f1184},
	{"VSP0",	0x200f1180},

	{"AUD2",	0x200f1178},
	{"AUD1",	0x200f1174},
	{"AUD0",	0x200f1170},

	{"CSS3",	0x200f11CC},
	{"CSS2",	0x200f11C8},
	{"CSS1",	0x200f11C4},
	{"CSS0",	0x200f11C0},

	{"DSS4",	0x200f11b0},
	{"DSS3",	0x200f11ac},
	{"DSS2",	0x200f11a8},
	{"DSS1",	0x200f11a4},
	{"DSS0",	0x200f11a0},

	{"ICS2",	0x200f11e8},
	{"ICS1",	0x200f11e4},
	{"ICS0",	0x200f11e0},

	{"DDR0",	0x200f1130},
	{"DDR1",	0x200f1140},

	{"GBUS",	0x200f1014},
	{"ABUS",	0x200f1010},
	{"PERI",	0x200f1118},
	{"SES",		0x200f1120},
	{"CORE",	0x200f11F0},

	{"NONE",	0x00000000},
	{"EN_INT",	0x200f1200},
	{"INT_PD0", 0x200f1210},
	{"INT_PD1", 0x200f1214},
	{"INT_PD2", 0x200f1218},
	{"INT_BUS0",0x200f1220},
	{"INT_BUS1",0x200f1224},
	{"INT_MEM", 0x200f1228},
	{"PWR_STAT",0x200f1300}
};

static int dbg_show_pdinfo(struct seq_file *s, void *p)
{
	unsigned int nValue = 0x00000000;
	int pos = 0;
	int idx = 0;

	for(idx = 0; idx < ARRAY_SIZE(pd_list); idx++)
	{
		if(pd_list[idx].nRegister != 0x00000000)
		{
			nValue = odin_pmic_getRegister(pd_list[idx].nRegister);

			if(pd_list[idx].nRegister == 0x200e0018 || pd_list[idx].nRegister == 0x200e0038) //CA15, CA7
			{
				pos += seq_printf(s,"%-20s [%s][0x%08x][0x%08x]\n",pd_list[idx].name, ((nValue&(0x0000000f << (8))) == (0x0000000f << (8))) ? "Off" : "On ", pd_list[idx].nRegister, nValue);
			}
			else
			{
				pos += seq_printf(s,"%-20s [%s][0x%08x][0x%08x]\n",pd_list[idx].name, (nValue == 0x300) ? "Off" : "On ", pd_list[idx].nRegister, nValue);
			}
		}
		else
		{
			pos += seq_printf(s,"\n");
		}
	}

	pos += seq_printf(s,"\n");

	return pos;
}

static int dbg_show_cpuinfo(struct seq_file *s, void *p)
{
	int pos = 0;
	int idx = 0;
	unsigned int ca15_pwrup_reg = odin_pmic_getRegister(0x200e0014); //HW32_REG(0x200e0014);
	unsigned int ca7_pwrup_reg = odin_pmic_getRegister(0x200e0034); //HW32_REG(0x200e0034);

//	pos += seq_printf(s,"CA15 :");

	for(idx = 0 ; idx < 4 ; idx++)
	{
		if((ca15_pwrup_reg & 0xff) == 0xff)
		{
			pos += seq_printf(s,"off ");
		} else
		{
			pos += seq_printf(s,"on ");
		}
		ca15_pwrup_reg >>= 8;
	}

//	pos += seq_printf(s,"\nCA7 :");

	for(idx = 0 ; idx < 4 ; idx++)
	{
		if((ca7_pwrup_reg & 0xff) == 0xff)
		{
			pos += seq_printf(s,"off ");
		} else
		{
			pos += seq_printf(s,"on ");
		}
		ca7_pwrup_reg >>= 8;
	}

	pos += seq_printf(s,"\n");

	return pos;
}


#define DBGFS_FUNC_DECL(name) \
static int dbg_open_##name(struct inode *inode, struct file *file) \
{ \
	return single_open(file, dbg_show_##name, NULL); \
} \
static const struct file_operations dbg_fops_##name = { \
	.owner		= THIS_MODULE, \
	.open		= dbg_open_##name, \
	.read = seq_read, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

DBGFS_FUNC_DECL(pdinfo);
DBGFS_FUNC_DECL(cpuinfo);


odin_pd_init_debugfs(void)
{
	int i;
	struct dentry *chandir;

	dbgfs_root = debugfs_create_dir("odin_pm", NULL);
	if (IS_ERR(dbgfs_root) || !dbgfs_root)
		goto err_root;

	dbgfs_pdinfo = debugfs_create_file("pdinfo", S_IRUSR|S_IRGRP, dbgfs_root, NULL,
					  &dbg_fops_pdinfo);
	if (!dbgfs_pdinfo)
		goto err_state;

	dbgfs_cpuinfo = debugfs_create_file("cpuinfo", S_IRUSR|S_IRGRP, dbgfs_root, NULL,
						 &dbg_fops_cpuinfo);
	if (!dbgfs_cpuinfo)
		goto err_state;

	return;

err_state:
	debugfs_remove_recursive(dbgfs_root);
err_root:
	pr_err("odin_pm_pdinfo: debugfs is not available\n");

}
static void __exit odin_pd_cleanup_debugfs(void)
{
	debugfs_remove_recursive(dbgfs_root);
}
#endif


static int __init odin_pd_arch_init(void)
{
	int idx;

	pm_notifier(odin_pd_pm_event_handler, 0);

	for (idx = 0; idx < ARRAY_SIZE(odin_pm_domains); idx++) {
		pm_genpd_init(&odin_pm_domains[idx]->gpd, NULL,
			odin_pm_domains[idx]->is_off);
		PDMSG("PDMSG %s: registered %s (is_off=%d)\n", __func__,
			odin_pm_domains[idx]->gpd.name,
			odin_pm_domains[idx]->is_off);
	}

	/*
	 * FIXME: vsp clk was enabled at clk framework,
	 * clk shall be stored after disabling it
	 */
	pd_clk_disable("vsp0_c_codec");
	pd_clk_disable("vsp0_e_codec");
	pd_clk_disable("vsp1_dec_clk");
	pd_clk_disable("vsp1_img_clk");

	store_crg_regs(ODIN_VSP0_CRG_BASE, vsp0_clk_save,
		ARRAY_SIZE(vsp0_clk_save), SZ_4K, SECURE);

#if 0
	for (idx = 0; idx < ARRAY_SIZE(vsp0_clk_save); idx++)
		pr_debug("vsp0_clk_save off_set=%x, reg_val=%x\n",
			vsp0_clk_save[idx].off_set,
			vsp0_clk_save[idx].reg_val);
#endif

	store_crg_regs(ODIN_VSP1_CRG_BASE, vsp1_clk_save,
		ARRAY_SIZE(vsp1_clk_save), SZ_4K, SECURE);

#if 0
	for (idx = 0; idx < ARRAY_SIZE(vsp1_clk_save); idx++)
		pr_debug("vsp1_clk_save off_set=%x, reg_val=%x\n",
			vsp1_clk_save[idx].off_set,
			vsp1_clk_save[idx].reg_val);
#endif

	if (pm_genpd_add_subdomain(&odin_pd_vsp0.gpd, &odin_pd_vsp1.gpd))
		pr_err("%s: failed to add subdomain 1\n", __func__);
	if (pm_genpd_add_subdomain(&odin_pd_vsp1.gpd, &odin_pd_vsp2.gpd))
		pr_err("%s: failed to add subdomain 2\n", __func__);
	if (pm_genpd_add_subdomain(&odin_pd_vsp0.gpd, &odin_pd_vsp3.gpd))
		pr_err("%s: failed to add subdomain 3\n", __func__);
	if (pm_genpd_add_subdomain(&odin_pd_vsp0.gpd, &odin_pd_vsp4.gpd))
		pr_err("%s: failed to add subdomain 4\n", __func__);
	if (pm_genpd_add_subdomain(&odin_pd_vsp4.gpd, &odin_pd_vsp4_jpeg.gpd))
		pr_err("%s: failed to add subdomain 4\n", __func__);
	if (pm_genpd_add_subdomain(&odin_pd_vsp4.gpd, &odin_pd_vsp4_png.gpd))
		pr_err("%s: failed to add subdomain 4\n", __func__);

	if (pm_genpd_add_subdomain(&odin_pd_dss0.gpd, &odin_pd_dss1.gpd))
		pr_err("%s: failed to add subdomain %d\n", __func__, odin_pd_dss1.gpd);
	if (pm_genpd_add_subdomain(&odin_pd_dss1.gpd, &odin_pd_dss3.gpd))
		pr_err("%s: failed to add subdomain %d\n", __func__, odin_pd_dss3.gpd);
/* temp fix for HDMI begin */
	if (pm_genpd_add_subdomain(&odin_pd_dss3.gpd, &odin_pd_dss3_lcd0_panel.gpd))
		pr_err("%s: failed to add subdomain dss3_lcd0_panel\n", __func__);
	if (pm_genpd_add_subdomain(&odin_pd_dss3.gpd, &odin_pd_dss3_lcd1_panel.gpd))
		pr_err("%s: failed to add subdomain dss3_lcd1_panel\n", __func__);
	if (pm_genpd_add_subdomain(&odin_pd_dss3.gpd, &odin_pd_dss3_hdmi_panel.gpd))
		pr_err("%s: failed to add subdomain dss3_hdmi_panel\n", __func__);
/* temp fix for HDMI end */

	if (pm_genpd_add_subdomain(&odin_pd_dss0.gpd, &odin_pd_dss2.gpd))
		pr_err("%s: failed to add subdomain %d\n", __func__, odin_pd_dss2.gpd);
	if (pm_genpd_add_subdomain(&odin_pd_dss0.gpd, &odin_pd_dss4.gpd))
		pr_err("%s: failed to add subdomain %d\n", __func__);

#if defined(CONFIG_MACH_ODIN_LIGER)
	odin_pd_init_debugfs();
#endif

#ifdef CONFIG_AUD_RPM
	if (pm_genpd_add_subdomain(&odin_pd_aud0.gpd, &odin_pd_aud1.gpd))
		pr_err("%s: failed to add subdomain\n", __func__);
	if (pm_genpd_add_subdomain(&odin_pd_aud0.gpd, &odin_pd_aud2.gpd))
		pr_err("%s: failed to add subdomain\n", __func__);
	if (pm_genpd_add_subdomain(&odin_pd_aud2.gpd, &odin_pd_aud2_i2c.gpd))
		pr_err("%s: failed to add subdomain\n", __func__);
	if (pm_genpd_add_subdomain(&odin_pd_aud2.gpd, &odin_pd_aud2_si2s0.gpd))
		pr_err("%s: failed to add subdomain\n", __func__);
	if (pm_genpd_add_subdomain(&odin_pd_aud2.gpd, &odin_pd_aud2_si2s1.gpd))
		pr_err("%s: failed to add subdomain\n", __func__);
	if (pm_genpd_add_subdomain(&odin_pd_aud2.gpd, &odin_pd_aud2_si2s2.gpd))
		pr_err("%s: failed to add subdomain\n", __func__);
	if (pm_genpd_add_subdomain(&odin_pd_aud2.gpd, &odin_pd_aud2_si2s3.gpd))
		pr_err("%s: failed to add subdomain\n", __func__);
	if (pm_genpd_add_subdomain(&odin_pd_aud2.gpd, &odin_pd_aud2_mi2s.gpd))
		pr_err("%s: failed to add subdomain\n", __func__);
#endif

	return 0;
}
arch_initcall(odin_pd_arch_init);

static __init int odin_pd_late_init(void)
{
	PDMSG("PDMSG %s\n", __func__);
	pm_genpd_poweroff_unused();
	return 0;
}
late_initcall(odin_pd_late_init);

MODULE_DESCRIPTION("ODIN PM Domain Controller");
MODULE_AUTHOR("Changwon Lee <changwon.lee@lge.com>");
MODULE_LICENSE("GPL v2");
