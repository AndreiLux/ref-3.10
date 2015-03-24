#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
/* #include <linux/proc_fs.h> */
#include <linux/platform_device.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_pmic_wrap.h>
#include <mach/upmu_hw.h>
#include <mach/mt_spm_idle.h>
#include <mach/mt_clkmgr.h>
#include <mach/pmic_mt6320_sw.h>
#include <drivers/misc/mediatek/power/mt8135/upmu_common.h>
#include <mach/upmu_hw.h>

u32 MT8135_Suspend_Golden[] = {
/* mipi register ================== */
	0xF0012044, 0xffffffff, 0x88492480,
	0xF0012000, 0xffffffff, 0x00000000,
	0xF0012004, 0xffffffff, 0x00000820,
	0xF0012008, 0xffffffff, 0x00000400,
	0xF001200C, 0xffffffff, 0x00000100,
	0xF0012010, 0xffffffff, 0x00000100,
	0xF0012014, 0xffffffff, 0x00000100,
	0xF0012040, 0xffffffff, 0x00000080,
	0xF0012068, 0xffffffff, 0x00000000,
	0xF0012824, 0xffffffff, 0x24248800,
	0xF0012820, 0xffffffff, 0x00000000,
	0xF0012800, 0xffffffff, 0x00008000,
	0xF0012804, 0xffffffff, 0x00008000,
	0xF0012808, 0xffffffff, 0x00008000,
	0xF001280C, 0xffffffff, 0x00008000,
	0xF0012810, 0xffffffff, 0x00008000,
	0xF0012814, 0xffffffff, 0x00008000,
	0xF0012818, 0xffffffff, 0x00008000,
	0xF001281C, 0xffffffff, 0x00008000,
	0xF0012838, 0xffffffff, 0x00000000,
	0xF001283C, 0xffffffff, 0x00000000,
	0xF0012840, 0xffffffff, 0x00000000,
	0xF0012060, 0xffffffff, 0x00000200,
	0xF0012050, 0xffffffff, 0xBE510026,
};

u32 MT6397_E1_Suspend_Golden[] = {
	0x0102, 0xffff, 0xc787,
	0x0108, 0x7eff, 0x4882,
	0x01ce, 0x0001, 0x0000,
	0x0128, 0xe37f, 0x0f6a,	/* bit6:5 is on for E1 */
	0x012a, 0xfc07, 0x0001,	/* bit[11:10] = 2'b0 for reg_ck=24MHz */
	0x01d4, 0x001f, 0x0000,
/* 0x4014, 0x0004, 0x0004, */
	0x0714, 0x0c00, 0x0000,
	0x0718, 0x000a, 0x0002,
	0x071A, 0x7000, 0x0000,
	0x0746, 0x0001, 0x0001,
	0x0712, 0x0003, 0x0000,
	0x0710, 0x0101, 0x0000,
	0x070C, 0x1000, 0x1000,
	0x0702, 0x0007, 0x0000,
	0x0700, 0x000f, 0x0000,
	0x071C, 0x0003, 0x0000,
	0x071E, 0x0003, 0x0000,
	0x072C, 0x0100, 0x0000,
	0x0222, 0x0001, 0x0000,
	0x0248, 0x0001, 0x0000,
	0x0270, 0x0002, 0x0002,
	0x028a, 0x0100, 0x0100,
	0x0274, 0x0001, 0x0001,
	0x029a, 0x0001, 0x0000,
	0x0324, 0x0100, 0x0100,
	0x030e, 0x0001, 0x0001,
	0x0330, 0x0002, 0x0002,
	0x0334, 0x0001, 0x0001,
	0x034a, 0x0100, 0x0100,
	0x0356, 0x0002, 0x0002,
	0x035a, 0x0001, 0x0001,
	0x0370, 0x0100, 0x0100,
	0x0386, 0x0001, 0x0001,
	0x039C, 0x0100, 0x0100,
	0x0206, 0x0060, 0x0060,
/* 0x0400, 0x6c00, 0x2800, ignore this check for E1 */
	0x0402, 0x4001, 0x4001,
	0x0404, 0x1000, 0x0000,
/* 0x0446, 0x0100, 0x0100, only for E2 */
	0x0410, 0x4000, 0x4000,
	0x0412, 0x4000, 0x4000,
	0x0414, 0x1000, 0x0000,
	0x0416, 0x4000, 0x0000,
	0x0418, 0x4000, 0x0000,
	0x041a, 0x8000, 0x0000,
	0x041c, 0x8000, 0x0000,
	0x041e, 0x8000, 0x0000,
	0x0420, 0x8000, 0x0000,
	0x0422, 0x8000, 0x0000,
	0x0424, 0x8000, 0x0000,
	0x0428, 0x0100, 0x0100,
	0x0440, 0x8000, 0x0000,
/*0x0800, 0x3500, 0x0000,
0x0824, 0xa000, 0x0000,
0x0826, 0x0006, 0x0000,*/
	0x083a, 0x7000, 0x0000,
};

u32 MT6397_E2_Suspend_Golden[] = {
	0x0102, 0xffff, 0xc787,
	0x0108, 0x7eff, 0x4882,
	0x01ce, 0x0001, 0x0000,
	0x0128, 0xe37f, 0x0f0a,
	0x012a, 0xfc07, 0x0001,	/* bit[11:10] = 2'b0 for reg_ck=24MHz */
	0x01d4, 0x001f, 0x0000,
/* 0x4014, 0x0004, 0x0004, */
	0x0714, 0x0c00, 0x0000,
	0x0718, 0x000a, 0x0002,
	0x071A, 0x7000, 0x0000,
	0x0746, 0x0001, 0x0001,
	0x0712, 0x0003, 0x0000,
	0x0710, 0x0101, 0x0000,
	0x070C, 0x1000, 0x1000,
	0x0702, 0x0007, 0x0000,
	0x0700, 0x000f, 0x0000,
	0x071C, 0x0003, 0x0000,
	0x071E, 0x0003, 0x0000,
	0x072C, 0x0100, 0x0000,
	0x0222, 0x0001, 0x0000,
	0x0248, 0x0001, 0x0000,
	0x0270, 0x0002, 0x0002,
	0x028a, 0x0100, 0x0100,
	0x0274, 0x0001, 0x0001,
	0x029a, 0x0001, 0x0000,
	0x0324, 0x0100, 0x0100,
	0x030e, 0x0001, 0x0001,
	0x0330, 0x0002, 0x0002,
	0x0334, 0x0001, 0x0001,
	0x034a, 0x0100, 0x0100,
	0x0356, 0x0002, 0x0002,
	0x035a, 0x0001, 0x0001,
	0x0370, 0x0100, 0x0100,
	0x0386, 0x0001, 0x0001,
	0x039C, 0x0100, 0x0100,
	0x0206, 0x0060, 0x0060,
	0x0400, 0xffff, 0xc459,
	0x0402, 0x4001, 0x4001,
	0x0404, 0x1000, 0x0000,
	0x0446, 0x0100, 0x0100,
	0x0410, 0x4000, 0x4000,
	0x0412, 0x4000, 0x4000,
	0x0414, 0x1000, 0x0000,
	0x0416, 0x4000, 0x0000,
	0x0418, 0x4000, 0x0000,
	0x041a, 0x8000, 0x0000,
	0x041c, 0x8000, 0x0000,
	0x041e, 0x8000, 0x0000,
	0x0420, 0x8000, 0x0000,
	0x0422, 0x8000, 0x0000,
	0x0424, 0x8000, 0x0000,
	0x0428, 0x0100, 0x0100,
	0x0440, 0x8000, 0x0000,
/*0x0800, 0x3500, 0x0000,
0x0824, 0xa000, 0x0000,
0x0826, 0x0006, 0x0000,*/
	0x083a, 0x7000, 0x0000,
};

u32 MT6397_E3_Suspend_Golden[] = {
	0x0102, 0xffff, 0xc787,
	0x0108, 0x7eff, 0x4882,
	0x01ce, 0x0001, 0x0000,
	0x0128, 0xe37f, 0x0f0a,
	0x012a, 0xfc07, 0x0001,	/* bit[11:10] = 2'b0 for reg_ck=24MHz */
	0x01d4, 0x001f, 0x0000,
/* 0x4014, 0x0004, 0x0004, */
	0x0714, 0x0c00, 0x0000,
	0x0718, 0x000a, 0x0002,
	0x071A, 0x7000, 0x0000,
	0x0746, 0x0001, 0x0001,
	0x0712, 0x0003, 0x0000,
	0x0710, 0x0101, 0x0000,
	0x070C, 0x1000, 0x1000,
	0x0702, 0x0007, 0x0000,
	0x0700, 0x000f, 0x0000,
	0x071C, 0x0003, 0x0000,
	0x071E, 0x0003, 0x0000,
	0x072C, 0x0100, 0x0000,
	0x0222, 0x0001, 0x0000,
	0x0248, 0x0001, 0x0000,
	0x0270, 0x0002, 0x0002,
	0x028a, 0x0100, 0x0100,
	0x0274, 0x0001, 0x0001,
	0x029a, 0x0001, 0x0000,
	0x0324, 0x0100, 0x0100,
	0x030e, 0x0001, 0x0001,
	0x0330, 0x0002, 0x0002,
	0x0334, 0x0001, 0x0001,
	0x034a, 0x0100, 0x0100,
	0x0356, 0x0002, 0x0002,
	0x035a, 0x0001, 0x0001,
	0x0370, 0x0100, 0x0100,
	0x0386, 0x0001, 0x0001,
	0x039C, 0x0100, 0x0100,
	0x0206, 0x0060, 0x0060,
	0x0400, 0xffff, 0xcc58,	/* E3 VTCXO fix */
	0x0402, 0x4001, 0x4001,
	0x0404, 0x1000, 0x0000,
	0x0446, 0x0200, 0x0200,	/* E3 VTCXO fix */
	0x0410, 0x4000, 0x4000,
	0x0412, 0x4000, 0x4000,
	0x0414, 0x1000, 0x0000,
	0x0416, 0x4000, 0x0000,
	0x0418, 0x4000, 0x0000,
	0x041a, 0x8000, 0x0000,
	0x041c, 0x8000, 0x0000,
	0x041e, 0x8000, 0x0000,
	0x0420, 0x8000, 0x0000,
	0x0422, 0x8000, 0x0000,
	0x0424, 0x8000, 0x0000,
	0x0428, 0x0100, 0x0100,
	0x0440, 0x8000, 0x0000,
/*0x0800, 0x3500, 0x0000,
0x0824, 0xa000, 0x0000,
0x0826, 0x0006, 0x0000,*/
	0x083a, 0x7000, 0x0000,
};

char pmic_6397_reg_0x13e[][10] = {
	"Vproc15", "Vsram15", "Vcore", "Vgpu", "Vio18", "Vproc7", "Vsram7", "Vdram",
	"Vusb", "Vtcxo", "n.a ", "n.a ", "n.a ", "n.a", "n.a", "Vrtc"
};

char pmic_6397_reg_0x140[][10] = {
	"Vmch", "Vmc", "Vio28", "Vibr", "Vgp6", "Vgp5", "Vgp4", "Vcamaf",
	"Vcamio", "Vcamd", "Vemc_3v3", "n.a", "Vcama", "n.a", "n.a", "Va28"
};


#define gs_read(addr)		(*(volatile u32 *)(addr))

static U16 gs_pmic_read(U16 addr)
{
	U32 rdata = 0;
	pwrap_read((U32) addr, &rdata);
	return (U16) rdata;
}

static void Golden_Setting_Compare_PLL(void)
{

	if (pll_is_on(ARMPLL1)) {
		clc_notice("ARMPLL1: on\n");
		BUG();		/* this PLL should be turned off before suspend */
	}
	if (pll_is_on(MMPLL)) {
		clc_notice("MMPLL: on\n");
		BUG();		/* this PLL should be turned off before suspend */
	}
	if (pll_is_on(MSDCPLL)) {
		clc_notice("MSDCPLL: on\n");
		BUG();		/* this PLL should be turned off before suspend */
	}
	if (pll_is_on(TVDPLL)) {
		clc_notice("TVDPLL: on\n");
		BUG();		/* this PLL should be turned off before suspend */
	}
	if (pll_is_on(LVDSPLL)) {
		clc_notice("LVDSPLL: on\n");
		BUG();		/* this PLL should be turned off before suspend */
	}
	if (pll_is_on(AUDPLL)) {
		clc_notice("AUDPLL: on\n");
		BUG();		/* this PLL should be turned off before suspend */
	}
	if (pll_is_on(VDECPLL)) {
		clc_notice("VDECPLL: on\n");
		BUG();		/* this PLL should be turned off before suspend */
	}

	if (subsys_is_on(SYS_VDE)) {
		clc_notice("SYS_VDE: %s\n", subsys_is_on(SYS_VDE) ? "on" : "off");
	}
	if (subsys_is_on(SYS_MFG_2D)) {
		clc_notice("SYS_MFG_2D: %s\n", subsys_is_on(SYS_MFG_2D) ? "on" : "off");
	}
	if (subsys_is_on(SYS_MFG)) {
		clc_notice("SYS_MFG: %s\n", subsys_is_on(SYS_MFG) ? "on" : "off");
	}
	if (subsys_is_on(SYS_VEN)) {
		clc_notice("SYS_VEN: %s\n", subsys_is_on(SYS_VEN) ? "on" : "off");
	}
	if (subsys_is_on(SYS_ISP)) {
		clc_notice("SYS_ISP: %s\n", subsys_is_on(SYS_ISP) ? "on" : "off");
	}
	if (subsys_is_on(SYS_DIS)) {
		clc_notice("SYS_DIS: %s\n", subsys_is_on(SYS_DIS) ? "on" : "off");
	}

}

static void Golden_Setting_Compare_PMIC_LDO(void)
{
	u16 temp_value, temp_i;

	/* PMIC 0x13E ========================================== */
	temp_value = gs_pmic_read(0x13E);
	/* clc_notice("PMIC 0x13E : temp_value=%d\n", temp_value); */
	for (temp_i = 0; temp_i < 16; temp_i++) {
		if ((temp_value & (0x1 << temp_i)) == (0x1 << temp_i)) {
			clc_notice("PMIC %s : On.\n", pmic_6397_reg_0x13e[temp_i]);
		}
	}

	/* PMIC 0x140 ========================================== */
	temp_value = gs_pmic_read(0x140);
	/* clc_notice("PMIC 0x140 : temp_value=%d\n", temp_value); */
	for (temp_i = 0; temp_i < 16; temp_i++) {
		if ((temp_value & (0x1 << temp_i)) == (0x1 << temp_i)) {
			clc_notice("PMIC %s : On.\n", pmic_6397_reg_0x140[temp_i]);
		}
	}
}

void Golden_Setting_Compare_for_Suspend(void)
{
	u32 i, counter_8135, counter_6397;
	u32 MT_8135_Len, MT_6397_Len;
	u32 chip_version = 0;
	u32 *MT6397_Suspend_Golden_ptr;

	/* check MT6397_E1 or MT6397_E2 */
	chip_version = upmu_get_cid();

#if 1
	if (chip_version == PMIC6397_E1_CID_CODE) {
		MT_6397_Len = sizeof(MT6397_E1_Suspend_Golden) / sizeof(u32);
		MT6397_Suspend_Golden_ptr = (u32 *) MT6397_E1_Suspend_Golden;
	} else if (chip_version == PMIC6397_E2_CID_CODE) {
		MT_6397_Len = sizeof(MT6397_E2_Suspend_Golden) / sizeof(u32);
		MT6397_Suspend_Golden_ptr = (u32 *) MT6397_E2_Suspend_Golden;
	} else {
		MT_6397_Len = sizeof(MT6397_E3_Suspend_Golden) / sizeof(u32);
		MT6397_Suspend_Golden_ptr = (u32 *) MT6397_E3_Suspend_Golden;
	}
#endif


	MT_8135_Len = sizeof(MT8135_Suspend_Golden) / sizeof(u32);
	counter_8135 = 0;
	counter_6397 = 0;

	/* MT8135 ====================================================================================================== */
	/*
	   for( i=0 ; i<MT_8135_Len ; i+=3 )
	   {
	   if( (gs_read(MT8135_Suspend_Golden[i]) & MT8135_Suspend_Golden[i+1]) != (MT8135_Suspend_Golden[i+2] & MT8135_Suspend_Golden[i+1]))
	   {
	   counter_8135++;
	   clc_notice("MT8135 Suspend register[0x%x] = 0x%x (mask : 0x%x, value : 0x%x)\n", MT8135_Suspend_Golden[i], gs_read(MT8135_Suspend_Golden[i]), MT8135_Suspend_Golden[i+1], MT8135_Suspend_Golden[i+2] );
	   }
	   }

	   if(counter_8135 == 0)
	   {
	   clc_notice("MT8135 Suspend golden setting : pass.\n");
	   }
	 */
	Golden_Setting_Compare_PLL();

	/* MT6397 ====================================================================================================== */
	for (i = 0; i < MT_6397_Len; i += 3) {
		if ((gs_pmic_read(MT6397_Suspend_Golden_ptr[i]) & MT6397_Suspend_Golden_ptr[i + 1])
		    != (MT6397_Suspend_Golden_ptr[i + 2] & MT6397_Suspend_Golden_ptr[i + 1])) {
			counter_6397++;
			clc_notice
			    ("MT6397 Suspend register[0x%x] = 0x%x (mask : 0x%x, value : 0x%x)\n",
			     MT6397_Suspend_Golden_ptr[i],
			     gs_pmic_read(MT6397_Suspend_Golden_ptr[i]),
			     MT6397_Suspend_Golden_ptr[i + 1], MT6397_Suspend_Golden_ptr[i + 2]);
		}
	}

	if (counter_6397 == 0) {
		clc_notice("MT6397 Suspend golden setting : pass.\n");
	}

	Golden_Setting_Compare_PMIC_LDO();

	/* ============================================================================================================ */
}

static struct platform_driver mtk_golden_setting_driver = {
	.remove = NULL,
	.shutdown = NULL,
	.probe = NULL,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		   .name = "mtk-golden-setting",
		   },
};

#if 0
/***************************
* golden_setting_debug_read
****************************/
static int golden_setting_debug_read(char *buf, char **start, off_t off, int count, int *eof,
				     void *data)
{
	int len = 0;
	char *p = buf;

	p += sprintf(p, "golden setting\n");

	len = p - buf;
	return len;
}

/************************************
* golden_setting_debug_write
*************************************/
static ssize_t golden_setting_debug_write(struct file *file, const char *buffer,
					  unsigned long count, void *data)
{
	int enabled = 0;

	if (sscanf(buffer, "%d", &enabled) == 1) {
		if (enabled == 1) {
			Golden_Setting_Compare_for_Suspend();
		} else {
			clc_notice("bad argument_0!! argument should be \"1\"\n");
		}
	} else {
		clc_notice("bad argument_0!! argument should be \"1\"\n");
	}

	return count;
}
#endif

static int __init golden_setting_init(void)
{
	/* struct proc_dir_entry *mt_entry = NULL; */
	/* struct proc_dir_entry *mt_golden_setting_dir = NULL; */
	int golden_setting_err = 0;
#if 0
	mt_golden_setting_dir = proc_mkdir("golden_setting", NULL);
	if (!mt_golden_setting_dir) {
		clc_notice("[%s]: mkdir /proc/golden_setting failed\n", __func__);
	} else {
		mt_entry =
		    create_proc_entry("golden_setting_debug", S_IRUGO | S_IWUSR | S_IWGRP,
				      mt_golden_setting_dir);
		if (mt_entry) {
			mt_entry->read_proc = golden_setting_debug_read;
			mt_entry->write_proc = golden_setting_debug_write;
		}
	}
#endif
	golden_setting_err = platform_driver_register(&mtk_golden_setting_driver);

	if (golden_setting_err) {
		clc_notice("golden setting driver callback register failed..\n");
		return golden_setting_err;
	}

	return 0;
}


MODULE_DESCRIPTION("MT8135 golden setting compare v0.1");

late_initcall(golden_setting_init);
