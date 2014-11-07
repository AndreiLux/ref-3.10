/*
 * arch/arm/mach-odin/odin_sec_smmu.c
 *
 * Copyright:	(C) 2013 LG Electronics
 * Create based on drivers/iommu/odin_iommu_dev.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/delay.h>

#include <linux/platform_data/odin_tz.h>
#include "odin_sec_smmu.h"

/* It required to read dss3's iommu by SDMA */
static void read_dss3_sdma(u32 sdma_base, u32 smmu_wa_desc)
{
	u32 tmp = 0;
	u64 cmd = 0;

	tz_write(0x000000BC, smmu_wa_desc);
	tz_write(0x02BC31F0, smmu_wa_desc + 0x4);
	tz_write(SMMU_WA_VAL, smmu_wa_desc + 0x8);
	tz_write(0x410501BC, smmu_wa_desc + 0xC);
	tz_write(0x00200E41, smmu_wa_desc + 0x10);
	tz_write(0x38130804, smmu_wa_desc + 0x14);
	tz_write(0x00803403, smmu_wa_desc + 0x18);

	tmp = tz_read(sdma_base + 0x1000) & ~0xd;
	tz_write(tmp, sdma_base + 0x1000);
	tz_write(0x0, sdma_base + 0x1004);
	tz_write(0x0000ffff, sdma_base + 0x1008);
	tz_write((tmp|0x8), sdma_base + 0x1000);
	tz_write(0xff000000, sdma_base + 0x20);
	while (tz_read(sdma_base + 0xd00) & 0x1)
		;

	cmd = DMAGO(SMMU_WA_DESC, 0, 0);

	tz_write((((u32)cmd << 16) | (0 << 8) | 0), sdma_base + 0xd08);
	tz_write((u32)(cmd >> 16), sdma_base + 0xd0c);
	tz_write(0x0, sdma_base + 0xd04);
	udelay(1);
	tz_write(0x1 << 16, sdma_base + 0x2c);

	/* We should wait for about 10 useconds. */
	/* If we don'w wait, system will be hanged. */
	udelay(10);
}

static u32 get_iommu_base(int irq, u32 offset)
{
	u32 addr;

	switch (irq) {
		case IOMMU_DSS_0:
			addr = 0x31C00000;
			break;
		case IOMMU_DSS_1:
			addr = 0x31D00000;
			break;
		case IOMMU_DSS_2:
			addr = 0x31E00000;
			break;
		case IOMMU_DSS_3:
			addr = 0x31F00000;
			break;
		case IOMMU_CSS_0:
			addr = 0x33800000;
			break;
		case IOMMU_CSS_1:
			addr = 0x33900000;
			break;
		case IOMMU_VSP_0:
			addr = 0x320E0000;
			break;
		case IOMMU_VSP_1:
			addr = 0x321E0000;
			break;
		case IOMMU_ICS:
			addr = 0x3FF40000;
			break;
		case IOMMU_AUD:
			addr = 0x34800000;
			break;
		case IOMMU_PERI:
			addr = 0x20040000;
			break;
		case IOMMU_SES:
			addr = 0x360E0000;
			break;
		case IOMMU_SDMA:
			addr = 0x0D020000;
			break;
		default:
			return 0;
	}
	addr += offset;

	return addr;
}

int odin_niu_tz_init(int irq)
{
	u32 d_qos_reg, pt_qos_reg;
	u32 d_qos_val, pt_qos_val;

	d_qos_reg = get_iommu_base(irq, 0x10000);
	if (d_qos_reg == 0) {
		printk(KERN_ERR"Invalid number of request for NIU Init.\n");
		return -1;
	}
	pt_qos_reg = d_qos_reg + 0x40;

	switch (irq) {
		case IOMMU_DSS_0:
		case IOMMU_DSS_2:
		case IOMMU_DSS_3:
			d_qos_val = 0xFFFFF700 +
			(ADD_SW_DM12 << 6) + (QOS_PRI_F << 2) + NOC_URGENCY_3;
			pt_qos_val = (QOS_PRI_F << 2) + ADD_SW_DM12;
			break;
		case IOMMU_DSS_1:
			d_qos_val = 0xFFFFF700 +
			(ADD_SW_GM12 << 6) + (QOS_PRI_F << 2) + NOC_URGENCY_3;
			pt_qos_val = (QOS_PRI_F << 2) + ADD_SW_GM12;
			break;
		case IOMMU_CSS_0:
		case IOMMU_CSS_1:
			d_qos_val = 0xFFFFF700 +
			(ADD_SW_GM12 << 6) + (QOS_PRI_A << 2) + NOC_URGENCY_2;
			pt_qos_val = (QOS_PRI_A << 2) + ADD_SW_GM12;
			break;
		case IOMMU_VSP_0:
		case IOMMU_VSP_1:
			d_qos_val = 0xFFFFF700 +
			(ADD_SW_DM12 << 6) + (QOS_PRI_0 << 2) + NOC_URGENCY_0;
			pt_qos_val = (QOS_PRI_0 << 2) + ADD_SW_GM12;
			break;
		case IOMMU_ICS:
			d_qos_val = 0xFFFFF700 +
			(ADD_SW_AM12 << 6) + (QOS_PRI_0 << 2) + NOC_URGENCY_0;
			pt_qos_val = (QOS_PRI_0 << 2) + ADD_SW_AM12;
			break;
		case IOMMU_AUD:
			d_qos_val = 0xFFFFF700 +
			(ADD_SW_AM12 << 6) + (QOS_PRI_F << 2) + NOC_URGENCY_3;
			pt_qos_val = (QOS_PRI_F << 2) + ADD_SW_AM12;
			break;
		case IOMMU_PERI:
			d_qos_val = 0xFFFFF700 +
			(ADD_SW_AM12 << 6) + (QOS_PRI_0 << 2) + NOC_URGENCY_0;
			pt_qos_val = (QOS_PRI_0 << 2) + ADD_SW_AM12;
			pt_qos_reg = d_qos_reg + 0x80;
			break;
		case IOMMU_SDMA:
			/* SKIP */
			return 0;
		case IOMMU_SES:
			d_qos_val = 0xFFFFF700 +
			(ADD_SW_DM12 << 6) + (QOS_PRI_0 << 2) + NOC_URGENCY_0;
			pt_qos_val = (QOS_PRI_0 << 2) + ADD_SW_DM12;
			break;
		default:
			printk(KERN_ERR"Invalid number of request for NIU Init.\n");
			return -1;
	}
	tz_write(0, d_qos_reg);
	tz_write(d_qos_val, d_qos_reg + 0x4);
	tz_write(pt_qos_val, pt_qos_reg);

	return 0;
}

int odin_iommu_tz_init(int irq)
{
	u32 base = get_iommu_base(irq, 0x0);
	if (base == 0) {
		printk(KERN_ERR"Invalid number of request for IOMMU Init.\n");
		return -1;
	}
	if (irq == IOMMU_DSS_3) {
		read_dss3_sdma(SECURE_SDMA, SMMU_WA_DESC);
	}
	tz_write(GS0_CR0_NSCFG_BIT(0x0) | GS0_CR0_WACFG_BIT(0x0)
			 | GS0_CR0_RACFG_BIT(0x0) | GS0_CR0_SHCFG_BIT(0x0)
			 | GS0_CR0_MTCFG_BIT(0x0) | GS0_CR0_MEMATTR_BIT(0xF)
			 | GS0_CR0_BSU_BIT(0x0) | GS0_CR0_FB_BIT(0x0)
			 | GS0_CR0_PTM_BIT(0x1) | GS0_CR0_VMIDPNE_BIT(0x0)
			 | GS0_CR0_USFCFG_BIT(0x1) | GS0_CR0_GCFGFIE_BIT(0x1)
			 | GS0_CR0_GCFGFRE_BIT(0x1) | GS0_CR0_GFIE_BIT(0x1)
			 | GS0_CR0_GFRE_BIT(0x1) | GS0_CR0_CLIENTPD_BIT(0x1),
			 base + GS0_CR0);
	if (irq == IOMMU_PERI) {
		tz_write(GS0_SCR1_SPMEM_BIT(0x1)
				| GS0_SCR1_SIF_BIT(0x1)
				| GS0_SCR1_GEFRO_BIT(0x1)
				| GS0_SCR1_GASRAE_BIT(0x0)
				| GS0_SCR1_NSNUMIRPTO_BIT(0x1)
				| GS0_SCR1_NSNUMSMRGO_BIT(ODIN_IOMMU_PERI_CTX_NUM)
				| GS0_SCR1_NSNUMCBO_BIT(ODIN_IOMMU_PERI_CTX_NUM),
				base + GS0_SCR1);
	} else {
		tz_write(GS0_SCR1_SPMEM_BIT(0x1)
				| GS0_SCR1_SIF_BIT(0x1)
				| GS0_SCR1_GEFRO_BIT(0x1)
				| GS0_SCR1_GASRAE_BIT(0x0)
				| GS0_SCR1_NSNUMIRPTO_BIT(0x1)
				| GS0_SCR1_NSNUMSMRGO_BIT(0x1)
				| GS0_SCR1_NSNUMCBO_BIT(0x1),
				base + GS0_SCR1);
	}
	tz_write(GS0_ACR_S2CRB_TLBEN_BIT(0x1)
			 | GS0_ACR_MMUDISB_TLBEN_BIT(0x1)
			 | GS0_ACR_SMTNMB_TLBEN_BIT(0x1),
			 base + GS0_ACR);

	/* initialize fault status registers */
	tz_write(0xFFFFFFFF, base + GS0_GFSR);

	/* invalidate secure TLBs */
	tz_write(0xFFFFFFFF, base + GS0_STLBIALL);

	return 0;
}
