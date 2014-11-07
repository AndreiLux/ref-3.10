#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/clk-private.h>
#include <linux/clkdev.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <asm/mach/map.h>
#include <linux/of.h>

#include "clk.h"

static DEFINE_SPINLOCK(clk_out_lock);

static struct map_desc odin_clk_io_desc[] __initdata = {
	{
		.virtual	= __MMIO_P2V(ODIN_SMS_CRG_BASE),
		.pfn		= __phys_to_pfn(ODIN_SMS_CRG_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= __MMIO_P2V(ODIN_CORE_CRG_BASE),
		.pfn		= __phys_to_pfn(ODIN_CORE_CRG_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= __MMIO_P2V(ODIN_VSP0_CRG_BASE),
		.pfn		= __phys_to_pfn(ODIN_VSP0_CRG_BASE),
		.length		= SZ_1K,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= __MMIO_P2V(ODIN_VSP1_CRG_BASE),
		.pfn		= __phys_to_pfn(ODIN_VSP1_CRG_BASE),
		.length		= SZ_1K,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= __MMIO_P2V(ODIN_AUD_CRG_BASE),
		.pfn		= __phys_to_pfn(ODIN_AUD_CRG_BASE),
		.length		= SZ_512,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= __MMIO_P2V(ODIN_ICS_CRG_BASE),
		.pfn		= __phys_to_pfn(ODIN_ICS_CRG_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= __MMIO_P2V(ODIN_DSS_CRG_BASE),
		.pfn		= __phys_to_pfn(ODIN_DSS_CRG_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= __MMIO_P2V(ODIN_CSS_SYS_BASE),
		.pfn		= __phys_to_pfn(ODIN_CSS_SYS_BASE),
		.length		= SZ_2K,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= __MMIO_P2V(ODIN_PERI_CRG_BASE),
		.pfn		= __phys_to_pfn(ODIN_PERI_CRG_BASE),
		.length		= SZ_4K,
		.type		= MT_DEVICE,
	},
	{
		.virtual	= __MMIO_P2V(ODIN_CPU_CRG_BASE),
		.pfn		= __phys_to_pfn(ODIN_CPU_CRG_BASE),
		.length		= SZ_64K,
		.type		= MT_DEVICE,
	},
};

DEFINE_CLK_FIXED_RATE(osc_clk, CLK_IS_ROOT, 24000000, 0x0);
DEFINE_CLK_FIXED_RATE(rtc_clk, CLK_IS_ROOT, 32000000, 0x0);

/*CPU PLL*/
DEFINE_LJ_PLL_DVFS(ca15_pll_clk,
	"osc_clk",
	&osc_clk,
	0,
	MMIO_P2V(ODIN_CPU_CRG_BASE + 0x011C),
	MMIO_P2V(ODIN_CPU_CRG_BASE + 0x0120),
	NULL,
	MMIO_P2V(ODIN_CPU_CRG_BASE + 0x0108),
	MMIO_P2V(ODIN_CPU_CRG_BASE + 0x0100),
	ODIN_CPU_CRG_BASE + 0x011C,
	ODIN_CPU_CRG_BASE + 0x0120,
	0x0,
	ODIN_CPU_CRG_BASE + 0x0108,
	ODIN_CPU_CRG_BASE + 0x0100,
	17, 19, 0, 8, 12,
	MACH_ODIN,
	CPU_PLL_CLK,
	NON_SECURE,
	&clk_out_lock);

DEFINE_LJ_PLL_DVFS(ca7_pll_clk,
	"osc_clk",
	&osc_clk,
	0,
	MMIO_P2V(ODIN_CPU_CRG_BASE + 0x0140),
	MMIO_P2V(ODIN_CPU_CRG_BASE + 0x0144),
	NULL,
	MMIO_P2V(ODIN_CPU_CRG_BASE + 0x010C),
	MMIO_P2V(ODIN_CPU_CRG_BASE + 0x0110),
	ODIN_CPU_CRG_BASE + 0x0140,
	ODIN_CPU_CRG_BASE + 0x0144,
	0x0,
	ODIN_CPU_CRG_BASE + 0x010C,
	ODIN_CPU_CRG_BASE + 0x0110,
	17, 19, 0, 8, 27,
	MACH_ODIN,
	CPU_PLL_CLK,
	NON_SECURE,
	&clk_out_lock);

/*MEM PLL*/
DEFINE_LJ_PLL(mem_pll_clk,
	"osc_clk",
	&osc_clk,
	0,
	MMIO_P2V(ODIN_CORE_CRG_BASE + 0x400),
	MMIO_P2V(ODIN_CORE_CRG_BASE + 0x400),
	NULL,
	ODIN_CORE_CRG_BASE + 0x400,
	ODIN_CORE_CRG_BASE + 0x400,
	0x0,
	1, 0,
	MACH_ODIN,
	COMM_PLL_CLK,
	NON_SECURE,
	&clk_out_lock);

/*ISP PLL*/
DEFINE_LJ_PLL(isp_pll_clk,
	"osc_clk",
	&osc_clk,
	0,
	MMIO_P2V(ODIN_SMS_CRG_BASE + 0x0E0),
	MMIO_P2V(ODIN_SMS_CRG_BASE + 0x0E0),
	MMIO_P2V(ODIN_SMS_CRG_BASE + 0x0E8),
	ODIN_SMS_CRG_BASE + 0x0E0,
	ODIN_SMS_CRG_BASE + 0x0E0,
	ODIN_SMS_CRG_BASE + 0x0E8,
	1, 0,
	MACH_ODIN,
	COMM_PLL_CLK,
	NON_SECURE,
	&clk_out_lock);

/*NOC PLL*/
DEFINE_LJ_PLL(noc_pll_clk,
	"osc_clk",
	&osc_clk,
	0,
	MMIO_P2V(ODIN_SMS_CRG_BASE + 0x120),
	MMIO_P2V(ODIN_SMS_CRG_BASE + 0x120),
	MMIO_P2V(ODIN_SMS_CRG_BASE + 0x128),
	ODIN_SMS_CRG_BASE + 0x120,
	ODIN_SMS_CRG_BASE + 0x120,
	ODIN_SMS_CRG_BASE + 0x128,
	1, 0,
	MACH_ODIN,
	COMM_PLL_CLK,
	NON_SECURE,
	&clk_out_lock);

/*DSS PLL*/
DEFINE_LJ_PLL(p_dss_pll_clk,
	"osc_clk",
	&osc_clk,
	0,
	MMIO_P2V(ODIN_SMS_CRG_BASE + 0x190),
	MMIO_P2V(ODIN_SMS_CRG_BASE + 0x190),
	MMIO_P2V(ODIN_SMS_CRG_BASE + 0x198),
	ODIN_SMS_CRG_BASE + 0x190,
	ODIN_SMS_CRG_BASE + 0x190,
	ODIN_SMS_CRG_BASE + 0x198,
	1, 0,
	MACH_ODIN,
	COMM_PLL_CLK,
	NON_SECURE,
	&clk_out_lock);

/*GPU PLL*/
DEFINE_LJ_PLL(p_gpu_pll_clk,
	"osc_clk",
	&osc_clk,
	0,
	MMIO_P2V(ODIN_SMS_CRG_BASE + 0x200),
	MMIO_P2V(ODIN_SMS_CRG_BASE + 0x200),
	MMIO_P2V(ODIN_SMS_CRG_BASE + 0x208),
	ODIN_SMS_CRG_BASE + 0x200,
	ODIN_SMS_CRG_BASE + 0x200,
	ODIN_SMS_CRG_BASE + 0x208,
	1, 0,
	MACH_ODIN,
	COMM_PLL_CLK,
	NON_SECURE,
	&clk_out_lock);

/*DP1 PLL*/
DEFINE_LJ_PLL(dp1_pll_clk,
	"osc_clk",
	&osc_clk,
	0,
	MMIO_P2V(ODIN_SMS_CRG_BASE + 0x4B0),
	MMIO_P2V(ODIN_SMS_CRG_BASE + 0x4B0),
	MMIO_P2V(ODIN_SMS_CRG_BASE + 0x4B8),
	ODIN_SMS_CRG_BASE + 0x4B0,
	ODIN_SMS_CRG_BASE + 0x4B0,
	ODIN_SMS_CRG_BASE + 0x4B8,
	1, 0,
	MACH_ODIN,
	COMM_PLL_CLK,
	NON_SECURE,
	&clk_out_lock);

/*DP2 PLL*/
DEFINE_LJ_PLL(dp2_pll_clk,
	"osc_clk",
	&osc_clk,
	0,
	MMIO_P2V(ODIN_SMS_CRG_BASE + 0x4C0),
	MMIO_P2V(ODIN_SMS_CRG_BASE + 0x4C0),
	MMIO_P2V(ODIN_SMS_CRG_BASE + 0x4C8),
	ODIN_SMS_CRG_BASE + 0x4C0,
	ODIN_SMS_CRG_BASE + 0x4C0,
	ODIN_SMS_CRG_BASE + 0x4C8,
	1, 0,
	MACH_ODIN,
	COMM_PLL_CLK,
	NON_SECURE,
	&clk_out_lock);

/*ICS HPLL*/
DEFINE_LJ_PLL(ics_hpll_clk,
	"osc_clk",
	&osc_clk,
	0,
	MMIO_P2V(ODIN_ICS_CRG_BASE),
	MMIO_P2V(ODIN_ICS_CRG_BASE),
	MMIO_P2V(ODIN_ICS_CRG_BASE + 0x08),
	ODIN_ICS_CRG_BASE,
	ODIN_ICS_CRG_BASE,
	ODIN_ICS_CRG_BASE + 0x08,
	20, 0,
	MACH_ODIN,
	ICS_PLL_CLK,
	NON_SECURE,
	&clk_out_lock);

/*AUD PLL*/
DEFINE_AUD_PLL(p_aud_pll_clk,
	"osc_clk",
	&osc_clk,
	0,
	MMIO_P2V(ODIN_AUD_CRG_BASE),
	MMIO_P2V(ODIN_AUD_CRG_BASE + 0x08),
	ODIN_AUD_CRG_BASE,
	ODIN_AUD_CRG_BASE + 0x08,
	0,
	MACH_ODIN,
	AUD_PLL_CLK,
	NON_SECURE,
	&clk_out_lock);

/***************************CORE*******************************/

static const char *scfg_pll_sels_name[] = {"osc_clk", "noc_pll_clk"};

static struct clk *scfg_pll_sels[] = {&osc_clk, &noc_pll_clk,};

DEFINE_ODIN_CLK_MUX(scfg_pll_sel_clk, scfg_pll_sels_name, scfg_pll_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CORE_CRG_BASE), ODIN_CORE_CRG_BASE,
	0, 1, 0, NON_SECURE, &clk_out_lock);

static const char *scfg_src_sels_name[] = {"scfg_pll_sel_clk", "mem_pll_clk"};

static struct clk *scfg_src_sels[] = {&scfg_pll_sel_clk, &mem_pll_clk,};

DEFINE_ODIN_CLK_MUX(scfg_src_sel_clk, scfg_src_sels_name, scfg_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CORE_CRG_BASE), ODIN_CORE_CRG_BASE,
	4, 1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(scfg_mask_clk, "scfg_src_sel_clk", &scfg_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CORE_CRG_BASE+0x4), ODIN_CORE_CRG_BASE+0x4,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(scfg_div_clk, "scfg_mask_clk", &scfg_mask_clk,
	0, MMIO_P2V(ODIN_CORE_CRG_BASE+0x8), NULL,
	ODIN_CORE_CRG_BASE+0x8, 0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

static const char *snoc_pll_sels_name[] = {"osc_clk", "noc_pll_clk"};

static struct clk *snoc_pll_sels[] = {&osc_clk, &noc_pll_clk,};

DEFINE_ODIN_CLK_MUX(snoc_pll_sel_clk, snoc_pll_sels_name, snoc_pll_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CORE_CRG_BASE+0x100), ODIN_CORE_CRG_BASE+0x100,
	0, 1, 0, NON_SECURE, &clk_out_lock);

static const char *snoc_src_sels_name[] = {"snoc_pll_sel_clk", "mem_pll_clk"};

static struct clk *snoc_src_sels[] = {&snoc_pll_sel_clk, &mem_pll_clk,};

DEFINE_ODIN_CLK_MUX(snoc_src_sel_clk, snoc_src_sels_name, snoc_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CORE_CRG_BASE+0x100), ODIN_CORE_CRG_BASE+0x100,
	4, 1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(snoc_mask_clk, "snoc_src_sel_clk", &snoc_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CORE_CRG_BASE+0x104), ODIN_CORE_CRG_BASE+0x104,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(snoc_div_clk, "snoc_mask_clk", &snoc_mask_clk,
	0, MMIO_P2V(ODIN_CORE_CRG_BASE+0x108), NULL, ODIN_CORE_CRG_BASE+0x108,
	0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *anoc_pll_sels_name[] = {"osc_clk", "noc_pll_clk"};

static struct clk *anoc_pll_sels[] = {&osc_clk, &noc_pll_clk,};

DEFINE_ODIN_CLK_MUX(anoc_pll_sel_clk, anoc_pll_sels_name, anoc_pll_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CORE_CRG_BASE+0x200), ODIN_CORE_CRG_BASE+0x200,
	0, 1, 0, NON_SECURE, &clk_out_lock);

static const char *anoc_src_sels_name[] = {"anoc_pll_sel_clk", "mem_pll_clk"};

static struct clk *anoc_src_sels[] = {&anoc_pll_sel_clk, &mem_pll_clk,};

DEFINE_ODIN_CLK_MUX(anoc_src_sel_clk, anoc_src_sels_name, anoc_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CORE_CRG_BASE+0x200), ODIN_CORE_CRG_BASE+0x200,
	4, 1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(anoc_mask_clk, "anoc_src_sel_clk", &anoc_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CORE_CRG_BASE+0x204), ODIN_CORE_CRG_BASE+0x204,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(anoc_div_clk, "anoc_mask_clk", &anoc_mask_clk,
	0, MMIO_P2V(ODIN_CORE_CRG_BASE+0x208), NULL, ODIN_CORE_CRG_BASE+0x208,
	0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *gdnoc_pll_sels_name[] = {"osc_clk", "noc_pll_clk"};

static struct clk *gdnoc_pll_sels[] = {&osc_clk, &noc_pll_clk,};

DEFINE_ODIN_CLK_MUX(gdnoc_pll_sel_clk, gdnoc_pll_sels_name, gdnoc_pll_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CORE_CRG_BASE+0x300), ODIN_CORE_CRG_BASE+0x300,
	0, 1, 0, NON_SECURE, &clk_out_lock);

static const char *gdnoc_src_sels_name[] = {"gdnoc_pll_sel_clk", "mem_pll_clk"};

static struct clk *gdnoc_src_sels[] = {&gdnoc_pll_sel_clk, &mem_pll_clk,};

DEFINE_ODIN_CLK_MUX(gdnoc_src_sel_clk, gdnoc_src_sels_name, gdnoc_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CORE_CRG_BASE+0x300), ODIN_CORE_CRG_BASE+0x300,
	4, 1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(gdnoc_mask_clk, "gdnoc_src_sel_clk", &gdnoc_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CORE_CRG_BASE+0x304), ODIN_CORE_CRG_BASE+0x304,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(gdnoc_div_clk, "gdnoc_mask_clk", &gdnoc_mask_clk,
	0, MMIO_P2V(ODIN_CORE_CRG_BASE+0x308), NULL, ODIN_CORE_CRG_BASE+0x308,
	0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *memc_src_sels_name[] = {"osc_clk", "mem_pll_clk"};

static struct clk *memc_src_sels[] = {&osc_clk, &mem_pll_clk,};

DEFINE_ODIN_CLK_MUX(memc_src_sel_clk, memc_src_sels_name, memc_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CORE_CRG_BASE+0x408), ODIN_CORE_CRG_BASE+0x408,
	0, 1, 0, NON_SECURE, &clk_out_lock);

static const char *npll_src_sels_name[] = {"mem_pll_clk", "noc_pll_clk"};

static struct clk *npll_src_sels[] = {&mem_pll_clk, &noc_pll_clk,};

DEFINE_ODIN_CLK_MUX(npll_src_sel_clk, npll_src_sels_name, npll_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CORE_CRG_BASE+0x500), ODIN_CORE_CRG_BASE+0x500,
	0, 1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(npll_clk, "npll_src_sel_clk", &npll_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CORE_CRG_BASE+0x500), ODIN_CORE_CRG_BASE+0x500,
	4, 0, NON_SECURE, &clk_out_lock);

static const char *npll_dss_pll_sels_name[] = {"mem_pll_clk", "noc_pll_clk"};

static struct clk *npll_dss_pll_sels[] = {&mem_pll_clk, &noc_pll_clk,};

DEFINE_ODIN_CLK_MUX(npll_dss_pll_sel_clk, npll_dss_pll_sels_name, npll_dss_pll_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CORE_CRG_BASE+0x504), ODIN_CORE_CRG_BASE+0x504,
	0, 1, 0, NON_SECURE, &clk_out_lock);

static const char *npll_dss_src_sels_name[] = {"npll_dss_pll_sel_clk", "isp_pll_clk"};

static struct clk *npll_dss_src_sels[] = {&npll_dss_pll_sel_clk, &noc_pll_clk,};

DEFINE_ODIN_CLK_MUX(npll_dss_src_sel_clk, npll_dss_src_sels_name, npll_dss_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CORE_CRG_BASE+0x504), ODIN_CORE_CRG_BASE+0x504,
	1, 1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(npll_clk_dss, "npll_dss_src_sel_clk", &npll_dss_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CORE_CRG_BASE+0x504), ODIN_CORE_CRG_BASE+0x504,
	4, 0, NON_SECURE, &clk_out_lock);


/***************************CPU*******************************/

static const char *ca15_pll_sels_name[] = {"osc_clk", "ca15_pll_clk"};

static struct clk *ca15_pll_sels[] = {&osc_clk, &ca15_pll_clk,};

DEFINE_ODIN_CLK_MUX(ca15_pll_sel_clk, ca15_pll_sels_name,
	ca15_pll_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CPU_CRG_BASE+0x100),
	ODIN_CPU_CRG_BASE+0x100, 0, 1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(ca15_div_clk, "ca15_pll_sel_clk",
	&ca15_pll_sel_clk, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CPU_CRG_BASE+0x104),
	MMIO_P2V(ODIN_CPU_CRG_BASE+0x100), ODIN_CPU_CRG_BASE+0x104, ODIN_CPU_CRG_BASE+0x100,
	0, 8, 12, MACH_ODIN, CPU_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

/*
DEFINE_ODIN_CLK_DIVIDER(pclkdbg_core_div_clk, "ca15_pll_sel_clk",
	&ca15_pll_sel_clk, 0, MMIO_P2V(ODIN_CPU_CRG_BASE+0x108),
	MMIO_P2V(ODIN_CPU_CRG_BASE+0x100), ODIN_CPU_CRG_BASE+0x108, ODIN_CPU_CRG_BASE+0x100,
	8, 8, 14, MACH_ODIN, CPU_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
*/
static const char *ca7_pll_sels_name[] = {"osc_clk", "ca7_pll_clk"};

static struct clk *ca7_pll_sels[] = {&osc_clk, &ca7_pll_clk,};

DEFINE_ODIN_CLK_MUX(ca7_pll_sel_clk, ca7_pll_sels_name,
	ca7_pll_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CPU_CRG_BASE+0x110),
	ODIN_CPU_CRG_BASE+0x110, 16, 1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(ca7_div_clk, "ca7_pll_sel_clk",
	&ca7_pll_sel_clk, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CPU_CRG_BASE+0x10C),
	MMIO_P2V(ODIN_CPU_CRG_BASE+0x110), ODIN_CPU_CRG_BASE+0x10C, ODIN_CPU_CRG_BASE+0x110,
	0, 8, 27, MACH_ODIN, CPU_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

/*
// ca15_div_2_clk is a dummy clock!!!!
DEFINE_ODIN_CLK_DIVIDER(ca15_div_2_clk, "ca15_pll_sel_clk", &ca15_pll_sel_clk,
	0, MMIO_P2V(ODIN_CPU_CRG_BASE+0x100), NULL,
	ODIN_CPU_CRG_BASE+0x100, 0x0, 28, 2, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

static const char *cpupll_divclk_sels_name[] = {"ca15_div_2_clk", "ca7_pll_sel_clk"};

static struct clk *cpupll_divclk_sels[] = {&ca15_div_2_clk, &ca7_pll_sel_clk,};

DEFINE_ODIN_CLK_MUX(cpupll_divclk_clk, cpupll_divclk_sels_name,
	cpupll_divclk_sels, 0, MMIO_P2V(ODIN_CPU_CRG_BASE+0x118),
	ODIN_CPU_CRG_BASE+0x118, 22, 1, 0, NON_SECURE, &clk_out_lock);
*/

static const char *cclk_src_sels_name[] = {"osc_clk", "noc_pll_clk", "osc_clk"};

static struct clk *cclk_src_sels[] = {&osc_clk, &noc_pll_clk, &osc_clk,};

DEFINE_ODIN_CLK_MUX(cclk_src_sel_clk, cclk_src_sels_name, cclk_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CPU_CRG_BASE+0x114),	ODIN_CPU_CRG_BASE+0x114,
	16, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(cclk_mask_clk, "cclk_src_sel_clk", &cclk_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CPU_CRG_BASE+0x148), ODIN_CPU_CRG_BASE+0x148,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(cclk_div_clk, "cclk_mask_clk", &cclk_mask_clk,
	0, MMIO_P2V(ODIN_CPU_CRG_BASE+0x114), MMIO_P2V(ODIN_CPU_CRG_BASE+0x114),
	ODIN_CPU_CRG_BASE+0x114, ODIN_CPU_CRG_BASE+0x114, 0, 4, 24, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

static const char *hclk_src_sels_name[] = {"osc_clk", "npll_clk", "osc_clk"};

static struct clk *hclk_src_sels[] = {&osc_clk, &npll_clk, &osc_clk,};

DEFINE_ODIN_CLK_MUX(hclk_src_sel_clk, hclk_src_sels_name, hclk_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CPU_CRG_BASE+0x114),	ODIN_CPU_CRG_BASE+0x114,
	18, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(hclk_mask_clk, "hclk_src_sel_clk", &hclk_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CPU_CRG_BASE+0x148), ODIN_CPU_CRG_BASE+0x148,
	1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(hclk_div_clk, "hclk_mask_clk", &hclk_mask_clk,
	0, MMIO_P2V(ODIN_CPU_CRG_BASE+0x114), MMIO_P2V(ODIN_CPU_CRG_BASE+0x114),
	ODIN_CPU_CRG_BASE+0x114, ODIN_CPU_CRG_BASE+0x114, 4, 4, 25, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

static const char *clk_cpucnt_src_sels_name[] = {"osc_clk", "npll_clk", "osc_clk"};

static struct clk *clk_cpucnt_src_sels[] = {&osc_clk, &npll_clk, &osc_clk,};

DEFINE_ODIN_CLK_MUX(clk_cpucnt_src_sel_clk, clk_cpucnt_src_sels_name, clk_cpucnt_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CPU_CRG_BASE+0x114),	ODIN_CPU_CRG_BASE+0x114,
	20, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(clk_cpucnt_mask_clk, "clk_cpucnt_src_sel_clk", &clk_cpucnt_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CPU_CRG_BASE+0x148), ODIN_CPU_CRG_BASE+0x148,
	2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(clk_cpucnt_div_clk, "clk_cpucnt_mask_clk", &clk_cpucnt_mask_clk,
	0, MMIO_P2V(ODIN_CPU_CRG_BASE+0x114), MMIO_P2V(ODIN_CPU_CRG_BASE+0x114),
	ODIN_CPU_CRG_BASE+0x114, ODIN_CPU_CRG_BASE+0x114, 8, 4, 26, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

static const char *pclk_sapb_src_sels_name[] = {"osc_clk", "npll_clk", "osc_clk"};

static struct clk *pclk_sapb_src_sels[] = {&osc_clk, &npll_clk, &osc_clk,};

DEFINE_ODIN_CLK_MUX(pclk_sapb_src_sel_clk, pclk_sapb_src_sels_name, pclk_sapb_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CPU_CRG_BASE+0x114),	ODIN_CPU_CRG_BASE+0x114,
	22, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(pclk_sapb_mask_clk, "pclk_sapb_src_sel_clk", &pclk_sapb_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CPU_CRG_BASE+0x148), ODIN_CPU_CRG_BASE+0x148,
	3, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(pclk_sapb_div_clk, "pclk_sapb_mask_clk", &pclk_sapb_mask_clk,
	0, MMIO_P2V(ODIN_CPU_CRG_BASE+0x114), MMIO_P2V(ODIN_CPU_CRG_BASE+0x114),
	ODIN_CPU_CRG_BASE+0x114, ODIN_CPU_CRG_BASE+0x114, 12, 4, 27, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

/*
static const char *gclk_src_sels_name[] = {"osc_clk", "npll_clk", "cpupll_divclk_clk"};

static struct clk *gclk_src_sels[] = {&osc_clk, &npll_clk, &cpupll_divclk_clk,};

DEFINE_ODIN_CLK_MUX(gclk_src_sel_clk, gclk_src_sels_name, gclk_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CPU_CRG_BASE+0x118),	ODIN_CPU_CRG_BASE+0x118,
	16, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(gclk_mask_clk, "gclk_src_sel_clk", &gclk_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CPU_CRG_BASE+0x148), ODIN_CPU_CRG_BASE+0x148,
	4, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(gclk_div_clk, "gclk_mask_clk", &gclk_mask_clk,
	0, MMIO_P2V(ODIN_CPU_CRG_BASE+0x118), MMIO_P2V(ODIN_CPU_CRG_BASE+0x118),
	ODIN_CPU_CRG_BASE+0x118, ODIN_CPU_CRG_BASE+0x118, 0, 4, 24, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

static const char *atclk_src_sels_name[] = {"osc_clk", "npll_clk", "cpupll_divclk_clk"};

static struct clk *atclk_src_sels[] = {&osc_clk, &npll_clk, &cpupll_divclk_clk,};

DEFINE_ODIN_CLK_MUX(atclk_src_sel_clk, atclk_src_sels_name, atclk_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CPU_CRG_BASE+0x118),	ODIN_CPU_CRG_BASE+0x118,
	18, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(atclk_mask_clk, "atclk_src_sel_clk", &atclk_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CPU_CRG_BASE+0x148), ODIN_CPU_CRG_BASE+0x148,
	5, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(atclk_div_clk, "atclk_mask_clk", &atclk_mask_clk,
	0, MMIO_P2V(ODIN_CPU_CRG_BASE+0x118), MMIO_P2V(ODIN_CPU_CRG_BASE+0x118),
	ODIN_CPU_CRG_BASE+0x118, ODIN_CPU_CRG_BASE+0x118, 4, 4, 25, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

static const char *trace_src_sels_name[] = {"osc_clk", "npll_clk", "cpupll_divclk_clk"};

static struct clk *trace_src_sels[] = {&osc_clk, &npll_clk, &cpupll_divclk_clk,};

DEFINE_ODIN_CLK_MUX(trace_src_sel_clk, trace_src_sels_name, trace_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CPU_CRG_BASE+0x118),	ODIN_CPU_CRG_BASE+0x118,
	20, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(trace_mask_clk, "trace_src_sel_clk", &trace_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CPU_CRG_BASE+0x148), ODIN_CPU_CRG_BASE+0x148,
	6, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(trace_div_clk, "trace_mask_clk", &trace_mask_clk,
	0, MMIO_P2V(ODIN_CPU_CRG_BASE+0x118), MMIO_P2V(ODIN_CPU_CRG_BASE+0x118),
	ODIN_CPU_CRG_BASE+0x118, ODIN_CPU_CRG_BASE+0x118, 8, 4, 26, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);
*/
/***************************ICS*******************************/

static const char *ics_src_sels_name[] = {"osc_clk", "npll_clk", "mem_pll_clk"};

static struct clk *ics_src_sels[] = {&osc_clk, &npll_clk, &mem_pll_clk,};

DEFINE_ODIN_CLK_MUX(ics_src_sel_clk, ics_src_sels_name, ics_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_ICS_CRG_BASE+0x10), ODIN_ICS_CRG_BASE+0x10,
	0, 2, 0, NON_SECURE, &clk_out_lock);

//may not be used
DEFINE_ODIN_CLK_GATE(ics_mask_clk, "ics_src_sel_clk", &ics_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_ICS_CRG_BASE+0x14), ODIN_ICS_CRG_BASE+0x14,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(ics_div_clk, "ics_mask_clk", &ics_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_ICS_CRG_BASE+0x18), NULL, ODIN_ICS_CRG_BASE+0x18,
	0x0, 0, 8, 0, MACH_ODIN, ICS_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

//may not be used
DEFINE_ODIN_CLK_GATE(ics_gate_clk, "ics_div_clk", &ics_div_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_ICS_CRG_BASE+0x1C), ODIN_ICS_CRG_BASE+0x1C,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(ics_cclk_div_clk, "ics_gate_clk", &ics_gate_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_ICS_CRG_BASE+0x30), NULL, ODIN_ICS_CRG_BASE+0x30,
	0x0, 0, 8, 0, MACH_ODIN, ICS_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(usbhsic_pmu_div_clk, "ics_cclk_div_clk", &ics_cclk_div_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_ICS_CRG_BASE+0x54), NULL, ODIN_ICS_CRG_BASE+0x54,
	0x0, 0, 8, 0, MACH_ODIN, ICS_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(usb30_suspend_div_clk, "ics_cclk_div_clk", &ics_cclk_div_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_ICS_CRG_BASE+0x5C), NULL, ODIN_ICS_CRG_BASE+0x5C,
	0x0, 0, 8, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(usb30_ref_div_clk, "ics_cclk_div_clk", &ics_cclk_div_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_ICS_CRG_BASE+0x64), NULL, ODIN_ICS_CRG_BASE+0x64,
	0x0, 0, 8, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(hsi_clk_div_clk, "ics_gate_clk", &ics_gate_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_ICS_CRG_BASE+0x6C), NULL, ODIN_ICS_CRG_BASE+0x6C,
	0x0, 0, 8, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(lli_clksvc_mask_clk, "osc_clk", &osc_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_ICS_CRG_BASE+0x78), ODIN_ICS_CRG_BASE+0x78,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(lli_clksvc_div_clk, "lli_clksvc_mask_clk", &lli_clksvc_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_ICS_CRG_BASE+0x7C), NULL, ODIN_ICS_CRG_BASE+0x7C,
	0x0, 0, 8, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(usbhsic_ref_mask_clk, "osc_clk", &osc_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_ICS_CRG_BASE+0x90), ODIN_ICS_CRG_BASE+0x90,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(usbhsic_ref_div_clk, "usbhsic_ref_mask_clk",
	&usbhsic_ref_mask_clk, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_ICS_CRG_BASE+0x94),
	NULL, ODIN_ICS_CRG_BASE+0x94, 0x0, 0, 8, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE,
	&clk_out_lock);

static const char *hsic_src_sels_name[] = {"osc_clk", "ics_hpll_clk", "mem_pll_clk"};

static struct clk *hsic_src_sels[] = {&osc_clk, &ics_hpll_clk, &mem_pll_clk,};

DEFINE_ODIN_CLK_MUX(hsic_src_sel_clk, hsic_src_sels_name, hsic_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_ICS_CRG_BASE+0x20), ODIN_ICS_CRG_BASE+0x20,
	0, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(hsic_mask_clk, "hsic_src_sel_clk", &hsic_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_ICS_CRG_BASE+0x24), ODIN_ICS_CRG_BASE+0x24,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(hsic_div_clk, "hsic_mask_clk", &hsic_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_ICS_CRG_BASE+0x28), NULL, ODIN_ICS_CRG_BASE+0x28,
	0x0, 0, 8, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

/***************************CSS*******************************/

static const char *mipicfg_pll_sels_name[] = {"osc_clk", "isp_pll_clk"};

static struct clk *mipicfg_pll_sels[] = {&osc_clk, &isp_pll_clk,};

DEFINE_ODIN_CLK_MUX(mipicfg_pll_sel_clk, mipicfg_pll_sels_name, mipicfg_pll_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x8), ODIN_CSS_SYS_BASE+0x8,
	16, 1, 0, NON_SECURE, &clk_out_lock);

static const char *mipicfg_src_sels_name[] = {"mipicfg_pll_sel_clk", "npll_clk"};

static struct clk *mipicfg_src_sels[] = {&mipicfg_pll_sel_clk, &npll_clk,};

DEFINE_ODIN_CLK_MUX(mipicfg_src_sel_clk, mipicfg_src_sels_name, mipicfg_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x8), ODIN_CSS_SYS_BASE+0x8,
	17, 1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(mipicfg_mask_clk, "mipicfg_src_sel_clk", &mipicfg_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x8), ODIN_CSS_SYS_BASE+0x8,
	18, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(mipicfg_div_clk, "mipicfg_mask_clk", &mipicfg_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x8), NULL, ODIN_CSS_SYS_BASE+0x8,
	0x0, 20, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *axi_pll_sels_name[] = {"osc_clk", "isp_pll_clk"};

static struct clk *axi_pll_sels[] = {&osc_clk, &isp_pll_clk,};

DEFINE_ODIN_CLK_MUX(axi_pll_sel_clk, axi_pll_sels_name, axi_pll_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0xC), ODIN_CSS_SYS_BASE+0xC,
	0, 1, 0, NON_SECURE, &clk_out_lock);

static const char *axi_src_sels_name[] = {"axi_pll_sel_clk", "npll_clk"};

static struct clk *axi_src_sels[] = {&axi_pll_sel_clk, &npll_clk,};

DEFINE_ODIN_CLK_MUX(axi_src_sel_clk, axi_src_sels_name, axi_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0xC), ODIN_CSS_SYS_BASE+0xC,
	1, 1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(axi_mask_clk, "axi_src_sel_clk", &axi_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0xC), ODIN_CSS_SYS_BASE+0xC,
	2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(axi_div_clk, "axi_mask_clk", &axi_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0xC), NULL, ODIN_CSS_SYS_BASE+0xC,
	0x0, 4, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(fg_clk, "axi_div_clk", &axi_div_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x4), ODIN_CSS_SYS_BASE+0x4,
	21, 0, NON_SECURE, &clk_out_lock);

static const char *isp_pll_sels_name[] = {"osc_clk", "isp_pll_clk"};

static struct clk *isp_pll_sels[] = {&osc_clk, &isp_pll_clk,};

DEFINE_ODIN_CLK_MUX(isp_pll_sel_clk, isp_pll_sels_name, isp_pll_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x14), ODIN_CSS_SYS_BASE+0x14,
	0, 1, 0, NON_SECURE, &clk_out_lock);

static const char *isp_src_sels_name[] = {"isp_pll_sel_clk", "npll_clk"};

static struct clk *isp_src_sels[] = {&isp_pll_sel_clk, &npll_clk,};

DEFINE_ODIN_CLK_MUX(isp_src_sel_clk, isp_src_sels_name, isp_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x14), ODIN_CSS_SYS_BASE+0x14,
	1, 1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(isp_mask_clk, "isp_src_sel_clk", &isp_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x14), ODIN_CSS_SYS_BASE+0x14,
	2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(isp_div_clk, "isp_mask_clk", &isp_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x14), NULL, ODIN_CSS_SYS_BASE+0x14,
	0x0, 4, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(isp0_clkx2, "isp_div_clk", &isp_div_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x4), ODIN_CSS_SYS_BASE+0x4,
	22, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(isp_div_clkx2, "isp_div_clk", &isp_div_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x14), NULL, ODIN_CSS_SYS_BASE+0x14,
	0x0, 8, 1, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(isp0_clk, "isp_div_clkx2", &isp_div_clkx2,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x4), ODIN_CSS_SYS_BASE+0x4,
	17, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(isp1_clk, "isp_div_clkx2", &isp_div_clkx2,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x4), ODIN_CSS_SYS_BASE+0x4,
	18, 0, NON_SECURE, &clk_out_lock);

static const char *pix0_isp_sels_name[] = {"isp0_clk", "isp0_clkx2"};

static struct clk *pix0_isp_sels[] = {&isp0_clk, &isp0_clkx2,};

DEFINE_ODIN_CLK_MUX(pix0_isp_sel_clk, pix0_isp_sels_name, pix0_isp_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x28), ODIN_CSS_SYS_BASE+0x28,
	0, 1, 0, NON_SECURE, &clk_out_lock);

static const char *pix0_src_sels_name[] = {"pix0_isp_sel_clk", "fg_clk"};

static struct clk *pix0_src_sels[] = {&pix0_isp_sel_clk, &fg_clk,};

DEFINE_ODIN_CLK_MUX(pix0_src_sel_clk, pix0_src_sels_name, pix0_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x28), ODIN_CSS_SYS_BASE+0x28,
	1, 1, 0, NON_SECURE, &clk_out_lock);

static const char *pix1_isp_sels_name[] = {"isp1_clk", "isp0_clkx2"};

static struct clk *pix1_isp_sels[] = {&isp1_clk, &isp0_clkx2,};

DEFINE_ODIN_CLK_MUX(pix1_isp_sel_clk, pix1_isp_sels_name, pix1_isp_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x28), ODIN_CSS_SYS_BASE+0x28,
	0, 1, 0, NON_SECURE, &clk_out_lock);

static const char *pix1_src_sels_name[] = {"pix1_isp_sel_clk", "fg_clk"};

static struct clk *pix1_src_sels[] = {&pix1_isp_sel_clk, &fg_clk,};

DEFINE_ODIN_CLK_MUX(pix1_src_sel_clk, pix1_src_sels_name, pix1_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x28), ODIN_CSS_SYS_BASE+0x28,
	1, 1, 0, NON_SECURE, &clk_out_lock);

static const char *vnr_pll_sels_name[] = {"osc_clk", "isp_pll_clk"};

static struct clk *vnr_pll_sels[] = {&osc_clk, &isp_pll_clk,};

DEFINE_ODIN_CLK_MUX(vnr_pll_sel_clk, vnr_pll_sels_name, vnr_pll_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x18), ODIN_CSS_SYS_BASE+0x18,
	0, 1, 0, NON_SECURE, &clk_out_lock);

static const char *vnr_src_sels_name[] = {"vnr_pll_sel_clk", "npll_clk"};

static struct clk *vnr_src_sels[] = {&vnr_pll_sel_clk, &npll_clk,};

DEFINE_ODIN_CLK_MUX(vnr_src_sel_clk, vnr_src_sels_name, vnr_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x18), ODIN_CSS_SYS_BASE+0x18,
	1, 1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(vnr_mask_clk, "vnr_src_sel_clk", &vnr_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x18), ODIN_CSS_SYS_BASE+0x18,
	2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(vnr_div_clk, "vnr_mask_clk", &vnr_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x18), NULL, ODIN_CSS_SYS_BASE+0x18,
	0x0, 4, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *fd_pll_sels_name[] = {"osc_clk", "isp_pll_clk"};

static struct clk *fd_pll_sels[] = {&osc_clk, &isp_pll_clk,};

DEFINE_ODIN_CLK_MUX(fd_pll_sel_clk, fd_pll_sels_name, fd_pll_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x24), ODIN_CSS_SYS_BASE+0x24,
	0, 1, 0, NON_SECURE, &clk_out_lock);

static const char *fd_src_sels_name[] = {"fd_pll_sel_clk", "npll_clk"};

static struct clk *fd_src_sels[] = {&fd_pll_sel_clk, &npll_clk,};

DEFINE_ODIN_CLK_MUX(fd_src_sel_clk, fd_src_sels_name, fd_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x24), ODIN_CSS_SYS_BASE+0x24,
	1, 1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(fd_mask_clk, "fd_src_sel_clk", &fd_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x24), ODIN_CSS_SYS_BASE+0x24,
	2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(fd_div_clk, "fd_mask_clk", &fd_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_CSS_SYS_BASE+0x24), NULL, ODIN_CSS_SYS_BASE+0x24,
	0x0, 4, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

/***************************PERI*******************************/

static const char *pnoc_a_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};

static struct clk *pnoc_a_src_sels[] = {&osc_clk, &npll_clk, &ics_hpll_clk,};

DEFINE_ODIN_CLK_MUX(pnoc_a_src_sel_clk, pnoc_a_src_sels_name, pnoc_a_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE), ODIN_PERI_CRG_BASE,
	4, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(pnoc_a_div_clk, "pnoc_a_src_sel_clk", &pnoc_a_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE), NULL, ODIN_PERI_CRG_BASE,
	0x0, 8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(pclk_div_clk, "pnoc_a_src_sel_clk", &pnoc_a_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x4), NULL, ODIN_PERI_CRG_BASE+0x4,
	0x0, 8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *wt_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};

static struct clk *wt_src_sels[] = {&osc_clk, &npll_clk, &ics_hpll_clk,};

DEFINE_ODIN_CLK_MUX(wt_src_sel_clk, wt_src_sels_name, wt_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x8), ODIN_PERI_CRG_BASE+0x8,
	4, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(wt_mask_clk, "wt_src_sel_clk", &wt_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x8), ODIN_PERI_CRG_BASE+0x8,
	1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(wt_div_clk, "wt_mask_clk", &wt_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x8), NULL, ODIN_PERI_CRG_BASE+0x8,
	0x0, 8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *sdc_sclk_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};

static struct clk *sdc_sclk_src_sels[] = {&osc_clk, &npll_clk, &ics_hpll_clk,};

DEFINE_ODIN_CLK_MUX(sdc_sclk_src_sel_clk, sdc_sclk_src_sels_name, sdc_sclk_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x6C), ODIN_PERI_CRG_BASE+0x6C,
	4, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(sdc_sclk_mask_clk, "sdc_sclk_src_sel_clk", &sdc_sclk_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x6C), ODIN_PERI_CRG_BASE+0x6C,
	1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(sdc_sclk_div_clk, "sdc_sclk_mask_clk", &sdc_sclk_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x6C), NULL, ODIN_PERI_CRG_BASE+0x6C,
	0x0, 8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *sdio_sclk0_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};

static struct clk *sdio_sclk0_src_sels[] = {&osc_clk, &npll_clk, &ics_hpll_clk,};

DEFINE_ODIN_CLK_MUX(sdio_sclk0_src_sel_clk, sdio_sclk0_src_sels_name, sdio_sclk0_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x70), ODIN_PERI_CRG_BASE+0x70,
	4, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(sdio_sclk0_mask_clk, "sdio_sclk0_src_sel_clk", &sdio_sclk0_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x70), ODIN_PERI_CRG_BASE+0x70,
	1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(sdio_sclk0_div_clk, "sdio_sclk0_mask_clk", &sdio_sclk0_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x70), NULL, ODIN_PERI_CRG_BASE+0x70,
	0x0, 8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *sdio_sclk1_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};

static struct clk *sdio_sclk1_src_sels[] = {&osc_clk, &npll_clk, &ics_hpll_clk,};

DEFINE_ODIN_CLK_MUX(sdio_sclk1_src_sel_clk, sdio_sclk1_src_sels_name, sdio_sclk1_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x74), ODIN_PERI_CRG_BASE+0x74,
	4, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(sdio_sclk1_mask_clk, "sdio_sclk1_src_sel_clk", &sdio_sclk1_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x74), ODIN_PERI_CRG_BASE+0x74,
	1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(sdio_sclk1_div_clk, "sdio_sclk1_mask_clk", &sdio_sclk1_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x74), NULL, ODIN_PERI_CRG_BASE+0x74,
	0x0, 8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *emmc_sclk_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};

static struct clk *emmc_sclk_src_sels[] = {&osc_clk, &npll_clk, &ics_hpll_clk,};

DEFINE_ODIN_CLK_MUX(emmc_sclk_src_sel_clk, emmc_sclk_src_sels_name, emmc_sclk_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x78), ODIN_PERI_CRG_BASE+0x78,
	4, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(emmc_sclk_mask_clk, "emmc_sclk_src_sel_clk", &emmc_sclk_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x78), ODIN_PERI_CRG_BASE+0x78,
	1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(emmc_sclk_div_clk, "emmc_sclk_mask_clk", &emmc_sclk_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x78), NULL, ODIN_PERI_CRG_BASE+0x78,
	0x0, 8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *e2nand_sclk_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};

static struct clk *e2nand_sclk_src_sels[] = {&osc_clk, &npll_clk, &ics_hpll_clk,};

DEFINE_ODIN_CLK_MUX(e2nand_sclk_src_sel_clk, e2nand_sclk_src_sels_name, e2nand_sclk_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x7C), ODIN_PERI_CRG_BASE+0x7C,
	4, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(e2nand_sclk_mask_clk, "e2nand_sclk_src_sel_clk", &e2nand_sclk_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x7C), ODIN_PERI_CRG_BASE+0x7C,
	1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(e2nand_sclk_div_clk, "e2nand_sclk_mask_clk", &e2nand_sclk_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x7C), NULL, ODIN_PERI_CRG_BASE+0x7C,
	0x0, 8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *i2c_sclk0_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};

static struct clk *i2c_sclk0_src_sels[] = {&osc_clk, &npll_clk, &ics_hpll_clk,};

DEFINE_ODIN_CLK_MUX(i2c_sclk0_src_sel_clk, i2c_sclk0_src_sels_name, i2c_sclk0_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x80), ODIN_PERI_CRG_BASE+0x80,
	4, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(i2c_sclk0_mask_clk, "i2c_sclk0_src_sel_clk", &i2c_sclk0_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x80), ODIN_PERI_CRG_BASE+0x80,
	1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(i2c_sclk0_div_clk, "i2c_sclk0_mask_clk", &i2c_sclk0_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x80), NULL, ODIN_PERI_CRG_BASE+0x80,
	0x0, 8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *i2c_sclk1_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};

static struct clk *i2c_sclk1_src_sels[] = {&osc_clk, &npll_clk, &ics_hpll_clk,};

DEFINE_ODIN_CLK_MUX(i2c_sclk1_src_sel_clk, i2c_sclk1_src_sels_name, i2c_sclk1_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x84), ODIN_PERI_CRG_BASE+0x84,
	4, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(i2c_sclk1_mask_clk, "i2c_sclk1_src_sel_clk", &i2c_sclk1_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x84), ODIN_PERI_CRG_BASE+0x84,
	1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(i2c_sclk1_div_clk, "i2c_sclk1_mask_clk", &i2c_sclk1_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x84), NULL, ODIN_PERI_CRG_BASE+0x84,
	0x0, 8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *i2c_sclk2_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};

static struct clk *i2c_sclk2_src_sels[] = {&osc_clk, &npll_clk, &ics_hpll_clk,};

DEFINE_ODIN_CLK_MUX(i2c_sclk2_src_sel_clk, i2c_sclk2_src_sels_name, i2c_sclk2_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x88), ODIN_PERI_CRG_BASE+0x88,
	4, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(i2c_sclk2_mask_clk, "i2c_sclk2_src_sel_clk", &i2c_sclk2_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x88), ODIN_PERI_CRG_BASE+0x88,
	1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(i2c_sclk2_div_clk, "i2c_sclk2_mask_clk", &i2c_sclk2_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x88), NULL, ODIN_PERI_CRG_BASE+0x88,
	0x0, 8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *i2c_sclk3_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};

static struct clk *i2c_sclk3_src_sels[] = {&osc_clk, &npll_clk, &ics_hpll_clk,};

DEFINE_ODIN_CLK_MUX(i2c_sclk3_src_sel_clk, i2c_sclk3_src_sels_name, i2c_sclk3_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x8C), ODIN_PERI_CRG_BASE+0x8C,
	4, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(i2c_sclk3_mask_clk, "i2c_sclk3_src_sel_clk", &i2c_sclk3_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x8C), ODIN_PERI_CRG_BASE+0x8C,
	1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(i2c_sclk3_div_clk, "i2c_sclk3_mask_clk", &i2c_sclk3_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x8C), NULL, ODIN_PERI_CRG_BASE+0x8C,
	0x0, 8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *spi_sclk0_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};

static struct clk *spi_sclk0_src_sels[] = {&osc_clk, &npll_clk, &ics_hpll_clk,};

DEFINE_ODIN_CLK_MUX(spi_sclk0_src_sel_clk, spi_sclk0_src_sels_name, spi_sclk0_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x90), ODIN_PERI_CRG_BASE+0x90,
	4, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(spi_sclk0_mask_clk, "spi_sclk0_src_sel_clk", &spi_sclk0_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x90), ODIN_PERI_CRG_BASE+0x90,
	1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(spi_sclk0_div_clk, "spi_sclk0_mask_clk", &spi_sclk0_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x90), NULL, ODIN_PERI_CRG_BASE+0x90,
	0x0, 8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *uart_sclk0_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};

static struct clk *uart_sclk0_src_sels[] = {&osc_clk, &npll_clk, &ics_hpll_clk,};

DEFINE_ODIN_CLK_MUX(uart_sclk0_src_sel_clk, uart_sclk0_src_sels_name, uart_sclk0_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x98), ODIN_PERI_CRG_BASE+0x98,
	4, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(uart_sclk0_mask_clk, "uart_sclk0_src_sel_clk", &uart_sclk0_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x98), ODIN_PERI_CRG_BASE+0x98,
	1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(uart_sclk0_div_clk, "uart_sclk0_mask_clk", &uart_sclk0_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x98), NULL, ODIN_PERI_CRG_BASE+0x98,
	0x0, 8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *uart_sclk1_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};

static struct clk *uart_sclk1_src_sels[] = {&osc_clk, &npll_clk, &ics_hpll_clk,};

DEFINE_ODIN_CLK_MUX(uart_sclk1_src_sel_clk, uart_sclk1_src_sels_name, uart_sclk1_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x9C), ODIN_PERI_CRG_BASE+0x9C,
	4, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(uart_sclk1_mask_clk, "uart_sclk1_src_sel_clk", &uart_sclk1_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x9C), ODIN_PERI_CRG_BASE+0x9C,
	1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(uart_sclk1_div_clk, "uart_sclk1_mask_clk", &uart_sclk1_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0x9C), NULL, ODIN_PERI_CRG_BASE+0x9C,
	0x0, 8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *uart_sclk2_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};

static struct clk *uart_sclk2_src_sels[] = {&osc_clk, &npll_clk, &ics_hpll_clk,};

DEFINE_ODIN_CLK_MUX(uart_sclk2_src_sel_clk, uart_sclk2_src_sels_name, uart_sclk2_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0xA0), ODIN_PERI_CRG_BASE+0xA0,
	4, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(uart_sclk2_mask_clk, "uart_sclk2_src_sel_clk", &uart_sclk2_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0xA0), ODIN_PERI_CRG_BASE+0xA0,
	1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(uart_sclk2_div_clk, "uart_sclk2_mask_clk", &uart_sclk2_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0xA0), NULL, ODIN_PERI_CRG_BASE+0xA0,
	0x0, 8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *uart_sclk3_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};

static struct clk *uart_sclk3_src_sels[] = {&osc_clk, &npll_clk, &ics_hpll_clk,};

DEFINE_ODIN_CLK_MUX(uart_sclk3_src_sel_clk, uart_sclk3_src_sels_name, uart_sclk3_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0xA4), ODIN_PERI_CRG_BASE+0xA4,
	4, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(uart_sclk3_mask_clk, "uart_sclk3_src_sel_clk", &uart_sclk3_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0xA4), ODIN_PERI_CRG_BASE+0xA4,
	1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(uart_sclk3_div_clk, "uart_sclk3_mask_clk", &uart_sclk3_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0xA4), NULL, ODIN_PERI_CRG_BASE+0xA4,
	0x0, 8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *uart_sclk4_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};

static struct clk *uart_sclk4_src_sels[] = {&osc_clk, &npll_clk, &ics_hpll_clk,};

DEFINE_ODIN_CLK_MUX(uart_sclk4_src_sel_clk, uart_sclk4_src_sels_name, uart_sclk4_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0xA8), ODIN_PERI_CRG_BASE+0xA8,
	4, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(uart_sclk4_mask_clk, "uart_sclk4_src_sel_clk", &uart_sclk4_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0xA8), ODIN_PERI_CRG_BASE+0xA8,
	1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(uart_sclk4_div_clk, "uart_sclk4_mask_clk", &uart_sclk4_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_PERI_CRG_BASE+0xA8), NULL, ODIN_PERI_CRG_BASE+0xA8,
	0x0, 8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

/***************************DSS*******************************/

static const char *core_src_sels_name[] = {"osc_clk", "p_dss_pll_clk", "p_dss_pll_clk"};

static struct clk *core_src_sels[] = {&osc_clk, &p_dss_pll_clk, &p_dss_pll_clk,};

DEFINE_ODIN_CLK_MUX(core_src_sel_clk, core_src_sels_name, core_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_DSS_CRG_BASE), ODIN_DSS_CRG_BASE,
	0, 2, 0, NON_SECURE, &clk_out_lock);

static const char *gfxcore_src_sels_name[] = {"osc_clk", "npll_clk_dss", "npll_clk_dss"};

static struct clk *gfxcore_src_sels[] = {&osc_clk, &npll_clk_dss, &npll_clk_dss,};

DEFINE_ODIN_CLK_MUX(gfxcore_src_sel_clk, gfxcore_src_sels_name, gfxcore_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_DSS_CRG_BASE), ODIN_DSS_CRG_BASE,
	4, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(gfx_div_clk, "gfxcore_src_sel_clk", &gfxcore_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_DSS_CRG_BASE+0x10), MMIO_P2V(ODIN_DSS_CRG_BASE+0x14),
	ODIN_DSS_CRG_BASE+0x10, ODIN_DSS_CRG_BASE+0x14, 0, 8, 1, MACH_ODIN,
	COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *disp0_src_sels_name[] = {"osc_clk", "dp1_pll_clk", "dp1_pll_clk"};

static struct clk *disp0_src_sels[] = {&osc_clk, &dp1_pll_clk, &dp1_pll_clk,};

DEFINE_ODIN_CLK_MUX(disp0_src_sel_clk, disp0_src_sels_name, disp0_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_DSS_CRG_BASE+0x8), ODIN_DSS_CRG_BASE+0x8,
	0, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(disp0_div_clk, "disp0_src_sel_clk", &disp0_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_DSS_CRG_BASE+0xC), MMIO_P2V(ODIN_DSS_CRG_BASE+0x14),
	ODIN_DSS_CRG_BASE+0xC, ODIN_DSS_CRG_BASE+0x14, 8, 8, 2, MACH_ODIN,
	COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *disp1_src_sels_name[] = {"osc_clk", "dp2_pll_clk", "dp2_pll_clk"};

static struct clk *disp1_src_sels[] = {&osc_clk, &dp2_pll_clk, &dp2_pll_clk,};

DEFINE_ODIN_CLK_MUX(disp1_src_sel_clk, disp1_src_sels_name, disp1_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_DSS_CRG_BASE+0x8), ODIN_DSS_CRG_BASE+0x8,
	2, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(disp1_div_clk, "disp1_src_sel_clk", &disp1_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_DSS_CRG_BASE+0xC), MMIO_P2V(ODIN_DSS_CRG_BASE+0x14),
	ODIN_DSS_CRG_BASE+0xC, ODIN_DSS_CRG_BASE+0x14, 16, 8, 3, MACH_ODIN,
	COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *hdmi_src_sels_name[] = {"osc_clk", "dp1_pll_clk", "dp2_pll_clk"};

static struct clk *hdmi_src_sels[] = {&osc_clk, &dp1_pll_clk, &dp2_pll_clk,};

DEFINE_ODIN_CLK_MUX(hdmi_src_sel_clk, hdmi_src_sels_name, hdmi_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_DSS_CRG_BASE+0x8), ODIN_DSS_CRG_BASE+0x8,
	4, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(hdmi_div_clk, "hdmi_src_sel_clk", &hdmi_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_DSS_CRG_BASE+0xC), MMIO_P2V(ODIN_DSS_CRG_BASE+0x14),
	ODIN_DSS_CRG_BASE+0xC, ODIN_DSS_CRG_BASE+0x14, 24, 8, 4, MACH_ODIN,
	COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

static const char *osc_src_sels_name[] = {"osc_clk", "osc_clk", "osc_clk"};

static struct clk *osc_src_sels[] = {&osc_clk, &osc_clk, &osc_clk,};

DEFINE_ODIN_CLK_MUX(osc_src_sel_clk, osc_src_sels_name, osc_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_DSS_CRG_BASE+0x8), ODIN_DSS_CRG_BASE+0x8,
	6, 2, 0, NON_SECURE, &clk_out_lock);

static const char *cec_src_sels_name[] = {"osc_clk", "rtc_clk", "rtc_clk"};

static struct clk *cec_src_sels[] = {&osc_clk, &rtc_clk, &rtc_clk,};

DEFINE_ODIN_CLK_MUX(cec_src_sel_clk, cec_src_sels_name, cec_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_DSS_CRG_BASE+0x8), ODIN_DSS_CRG_BASE+0x8,
	8, 2, 0, NON_SECURE, &clk_out_lock);

static const char *tx_esc_src_sels_name[] = {"osc_clk", "p_dss_pll_clk", "osc_clk"};

static struct clk *tx_esc_src_sels[] = {&osc_clk, &p_dss_pll_clk, &osc_clk,};

DEFINE_ODIN_CLK_MUX(tx_esc_src_sel_clk, tx_esc_src_sels_name, tx_esc_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_DSS_CRG_BASE+0x8), ODIN_DSS_CRG_BASE+0x8,
	10, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(tx_esc_div_clk, "tx_esc_src_sel_clk", &tx_esc_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_DSS_CRG_BASE+0xC), MMIO_P2V(ODIN_DSS_CRG_BASE+0x14),
	ODIN_DSS_CRG_BASE+0xC, ODIN_DSS_CRG_BASE+0x14, 0, 8, 6, MACH_ODIN,
	COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

/***************************GPU*******************************/

static const char *gpu_mem_src_sels_name[] = {"osc_clk", "p_gpu_pll_clk", "npll_clk"};

static struct clk *gpu_mem_src_sels[] = {&osc_clk, &p_gpu_pll_clk, &npll_clk,};

DEFINE_ODIN_CLK_MUX(gpu_mem_src_sel_clk, gpu_mem_src_sels_name, gpu_mem_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_GPU_CRG_BASE), ODIN_GPU_CRG_BASE,
	0, 4, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(gpu_mem_mask_clk, "gpu_mem_src_sel_clk", &gpu_mem_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_GPU_CRG_BASE+0x4), ODIN_GPU_CRG_BASE+0x4,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(gpu_mem_div_clk, "gpu_mem_mask_clk", &gpu_mem_mask_clk,
	0, MMIO_P2V(ODIN_GPU_CRG_BASE+0x8), NULL,
	ODIN_GPU_CRG_BASE+0x8, 0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

//virtual_gpu_sys_div->Do not touch this divider!!!!
DEFINE_ODIN_CLK_DIVIDER(gpu_sys_div_clk, "gpu_mem_div_clk", &gpu_mem_div_clk,
	0, MMIO_P2V(ODIN_GPU_CRG_BASE+0x8), NULL,
	ODIN_GPU_CRG_BASE+0x8, 0x0, 4, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

static const char *gpu_core_src_sels_name[] = {"osc_clk", "p_gpu_pll_clk", "npll_clk"};

static struct clk *gpu_core_src_sels[] = {&osc_clk, &p_gpu_pll_clk, &npll_clk,};

DEFINE_ODIN_CLK_MUX(gpu_core_src_sel_clk, gpu_core_src_sels_name, gpu_core_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_GPU_CRG_BASE+0x200), ODIN_GPU_CRG_BASE+0x200,
	0, 4, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(gpu_core_mask_clk, "gpu_core_src_sel_clk", &gpu_core_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_GPU_CRG_BASE+0x204), ODIN_GPU_CRG_BASE+0x204,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(gpu_core_div_clk, "gpu_core_mask_clk", &gpu_core_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_GPU_CRG_BASE+0x208), NULL,
	ODIN_GPU_CRG_BASE+0x208, 0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

/***************************VSP*******************************/

static const char *vsp0_c_codec_src_sels_name[] = {"osc_clk", "npll_clk", "p_dss_pll_clk"};

static struct clk *vsp0_c_codec_src_sels[] = {&osc_clk, &npll_clk, &p_dss_pll_clk,};

DEFINE_ODIN_CLK_MUX(vsp0_c_codec_src_sel_clk, vsp0_c_codec_src_sels_name, vsp0_c_codec_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP0_CRG_BASE+0x200), ODIN_VSP0_CRG_BASE+0x200,
	0, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(vsp0_c_codec_mask_clk, "vsp0_c_codec_src_sel_clk", &vsp0_c_codec_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP0_CRG_BASE+0x204), ODIN_VSP0_CRG_BASE+0x204,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(vsp0_c_codec_div_clk, "vsp0_c_codec_mask_clk", &vsp0_c_codec_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP0_CRG_BASE+0x208), NULL,
	ODIN_VSP0_CRG_BASE+0x208, 0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

static const char *vsp0_e_codec_src_sels_name[] = {"osc_clk", "npll_clk", "p_dss_pll_clk"};

static struct clk *vsp0_e_codec_src_sels[] = {&osc_clk, &npll_clk, &p_dss_pll_clk,};

DEFINE_ODIN_CLK_MUX(vsp0_e_codec_src_sel_clk, vsp0_e_codec_src_sels_name, vsp0_e_codec_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP0_CRG_BASE+0x220), ODIN_VSP0_CRG_BASE+0x220,
	0, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(vsp0_e_codec_mask_clk, "vsp0_e_codec_src_sel_clk", &vsp0_e_codec_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP0_CRG_BASE+0x224), ODIN_VSP0_CRG_BASE+0x224,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(vsp0_e_codec_div_clk, "vsp0_e_codec_mask_clk", &vsp0_e_codec_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP0_CRG_BASE+0x228), NULL,
	ODIN_VSP0_CRG_BASE+0x228, 0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

static const char *vsp0_aclk_src_sels_name[] = {"osc_clk", "npll_clk", "p_dss_pll_clk"};

static struct clk *vsp0_aclk_src_sels[] = {&osc_clk, &npll_clk, &p_dss_pll_clk,};

DEFINE_ODIN_CLK_MUX(vsp0_aclk_src_sel_clk, vsp0_aclk_src_sels_name, vsp0_aclk_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP0_CRG_BASE+0x240), ODIN_VSP0_CRG_BASE+0x240,
	0, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(vsp0_aclk_mask_clk, "vsp0_aclk_src_sel_clk", &vsp0_aclk_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP0_CRG_BASE+0x244), ODIN_VSP0_CRG_BASE+0x244,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(vsp0_aclk_div_clk, "vsp0_aclk_mask_clk", &vsp0_aclk_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP0_CRG_BASE+0x248), NULL,
	ODIN_VSP0_CRG_BASE+0x248, 0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

static const char *vsp0_pclk_src_sels_name[] = {"osc_clk", "npll_clk", "p_dss_pll_clk"};

static struct clk *vsp0_pclk_src_sels[] = {&osc_clk, &npll_clk, &p_dss_pll_clk,};

DEFINE_ODIN_CLK_MUX(vsp0_pclk_src_sel_clk, vsp0_pclk_src_sels_name, vsp0_pclk_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP0_CRG_BASE+0x250), ODIN_VSP0_CRG_BASE+0x250,
	0, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(vsp0_pclk_mask_clk, "vsp0_pclk_src_sel_clk", &vsp0_pclk_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP0_CRG_BASE+0x254), ODIN_VSP0_CRG_BASE+0x254,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(vsp0_pclk_div_clk, "vsp0_pclk_mask_clk", &vsp0_pclk_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP0_CRG_BASE+0x258), NULL,
	ODIN_VSP0_CRG_BASE+0x258, 0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

static const char *vsp1_dec_src_sels_name[] = {"osc_clk", "npll_clk", "p_dss_pll_clk"};

static struct clk *vsp1_dec_src_sels[] = {&osc_clk, &npll_clk, &p_dss_pll_clk,};

DEFINE_ODIN_CLK_MUX(vsp1_dec_src_sel_clk, vsp1_dec_src_sels_name, vsp1_dec_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP1_CRG_BASE+0x210), ODIN_VSP1_CRG_BASE+0x210,
	0, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(vsp1_dec_mask_clk, "vsp1_dec_src_sel_clk", &vsp1_dec_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP1_CRG_BASE+0x214), ODIN_VSP1_CRG_BASE+0x214,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(vsp1_dec_div_clk, "vsp1_dec_mask_clk", &vsp1_dec_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP1_CRG_BASE+0x218), NULL,
	ODIN_VSP1_CRG_BASE+0x218, 0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

static const char *vsp1_img_src_sels_name[] = {"osc_clk", "npll_clk", "p_dss_pll_clk"};

static struct clk *vsp1_img_src_sels[] = {&osc_clk, &npll_clk, &p_dss_pll_clk,};

DEFINE_ODIN_CLK_MUX(vsp1_img_src_sel_clk, vsp1_img_src_sels_name, vsp1_img_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP1_CRG_BASE+0x230), ODIN_VSP1_CRG_BASE+0x230,
	0, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(vsp1_img_mask_clk, "vsp1_img_src_sel_clk", &vsp1_img_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP1_CRG_BASE+0x234), ODIN_VSP1_CRG_BASE+0x234,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(vsp1_img_div_clk, "vsp1_img_mask_clk", &vsp1_img_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP1_CRG_BASE+0x238), NULL,
	ODIN_VSP1_CRG_BASE+0x238, 0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

static const char *vsp1_aclk_src_sels_name[] = {"osc_clk", "npll_clk", "p_dss_pll_clk"};

static struct clk *vsp1_aclk_src_sels[] = {&osc_clk, &npll_clk, &p_dss_pll_clk,};

DEFINE_ODIN_CLK_MUX(vsp1_aclk_src_sel_clk, vsp1_aclk_src_sels_name, vsp1_aclk_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP1_CRG_BASE+0x240), ODIN_VSP1_CRG_BASE+0x240,
	0, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(vsp1_aclk_mask_clk, "vsp1_aclk_src_sel_clk", &vsp1_aclk_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP1_CRG_BASE+0x244), ODIN_VSP1_CRG_BASE+0x244,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(vsp1_aclk_div_clk, "vsp1_aclk_mask_clk", &vsp1_aclk_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP1_CRG_BASE+0x248), NULL,
	ODIN_VSP1_CRG_BASE+0x248, 0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

static const char *vsp1_pclk_src_sels_name[] = {"osc_clk", "npll_clk", "p_dss_pll_clk"};

static struct clk *vsp1_pclk_src_sels[] = {&osc_clk, &npll_clk, &p_dss_pll_clk,};

DEFINE_ODIN_CLK_MUX(vsp1_pclk_src_sel_clk, vsp1_pclk_src_sels_name, vsp1_pclk_src_sels,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP1_CRG_BASE+0x250), ODIN_VSP1_CRG_BASE+0x250,
	0, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(vsp1_pclk_mask_clk, "vsp1_pclk_src_sel_clk", &vsp1_pclk_src_sel_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP1_CRG_BASE+0x254), ODIN_VSP1_CRG_BASE+0x254,
	0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(vsp1_pclk_div_clk, "vsp1_pclk_mask_clk", &vsp1_pclk_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_VSP1_CRG_BASE+0x258), NULL,
	ODIN_VSP1_CRG_BASE+0x258, 0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

/***************************AUD*******************************/

/*DSP Clock*/

static const char *dsp_cclk_src_sels_name[] = {"osc_clk", "npll_clk", "isp_pll_clk"};

static struct clk *dsp_cclk_src_sels[] = {&osc_clk, &npll_clk, &isp_pll_clk};

DEFINE_ODIN_CLK_MUX(dsp_cclk_src_sel_clk, dsp_cclk_src_sels_name,
	dsp_cclk_src_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x010),
	ODIN_AUD_CRG_BASE+0x010, 0, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(dsp_cclk_mask_clk, "dsp_cclk_src_sel_clk",
	&dsp_cclk_src_sel_clk, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x014),
	ODIN_AUD_CRG_BASE+0x014, 0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(dsp_cclk_div_clk, "dsp_cclk_mask_clk", &dsp_cclk_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x018), NULL,
	ODIN_AUD_CRG_BASE+0x018, 0x0, 0, 8, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(aclk_dsp_div_clk, "dsp_cclk_div_clk", &dsp_cclk_div_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x020), NULL,
	ODIN_AUD_CRG_BASE+0x020, 0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(pclk_dsp_div_clk, "dsp_cclk_div_clk", &dsp_cclk_div_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x028), NULL,
	ODIN_AUD_CRG_BASE+0x028, 0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

static const char *p_async_src_sels_name[] = {"osc_clk", "npll_clk", "isp_pll_clk"};

static struct clk *p_async_src_sels[] = {&osc_clk, &npll_clk, &isp_pll_clk};

DEFINE_ODIN_CLK_MUX(p_async_src_sel_clk, p_async_src_sels_name,
	p_async_src_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x030),
	ODIN_AUD_CRG_BASE+0x030, 0, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(p_async_mask_clk, "p_async_src_sel_clk",
	&p_async_src_sel_clk, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x034),
	ODIN_AUD_CRG_BASE+0x034, 0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(p_async_div_clk, "p_async_mask_clk", &p_async_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x038), NULL,
	ODIN_AUD_CRG_BASE+0x038, 0x0, 0, 8, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

static const char *slimbus_src_sels_name[] = {"osc_clk", "p_aud_pll_clk", "npll_clk"};

static struct clk *slimbus_src_sels[] = {&osc_clk, &p_aud_pll_clk, &npll_clk};

DEFINE_ODIN_CLK_MUX(slimbus_src_sel_clk, slimbus_src_sels_name,
	slimbus_src_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x040),
	ODIN_AUD_CRG_BASE+0x040, 0, 2, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(slimbus_mask_clk, "slimbus_src_sel_clk",
	&slimbus_src_sel_clk, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x044),
	ODIN_AUD_CRG_BASE+0x044, 0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(slimbus_div_clk, "slimbus_mask_clk", &slimbus_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x048), NULL,
	ODIN_AUD_CRG_BASE+0x048, 0x0, 0, 4, 0, MACH_ODIN, AUD_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

/*I2S0 CLK*/
static const char *i2s0_pll_sels_name[] = {"osc_clk", "p_aud_pll_clk"};

static struct clk *i2s0_pll_sels[] = {&osc_clk, &p_aud_pll_clk,};

DEFINE_ODIN_CLK_MUX(i2s0_pll_sel_clk, i2s0_pll_sels_name,
	i2s0_pll_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x050),
	ODIN_AUD_CRG_BASE+0x050, 0, 1, 0, NON_SECURE, &clk_out_lock);

static const char *i2s0_alt_sels_name[] = {"p_aud_pll_clk",};

static struct clk *i2s0_alt_sels[] = {&p_aud_pll_clk};

DEFINE_ODIN_CLK_MUX(i2s0_alt_sel_clk, i2s0_alt_sels_name,
	i2s0_alt_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x050),
	ODIN_AUD_CRG_BASE+0x50, 8, 3, 0, NON_SECURE, &clk_out_lock);

static const char *i2s0_src_sels_name[] = {"i2s0_pll_sel_clk", "i2s0_alt_sel_clk"};

static struct clk *i2s0_src_sels[] = {&i2s0_pll_sel_clk, &i2s0_alt_sel_clk,};

DEFINE_ODIN_CLK_MUX(i2s0_src_sel_clk, i2s0_src_sels_name,
	i2s0_src_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x050),
	ODIN_AUD_CRG_BASE+0x050, 4, 1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(i2s0_mask_clk, "i2s0_src_sel_clk",
	&i2s0_src_sel_clk, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x054),
	ODIN_AUD_CRG_BASE+0x054, 0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(i2s0_div_clk, "i2s0_mask_clk", &i2s0_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x058), NULL,
	ODIN_AUD_CRG_BASE+0x058, 0x0, 0, 8, 0, MACH_ODIN, AUD_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

/*I2S1 CLK*/
static const char *i2s1_pll_sels_name[] = {"osc_clk", "p_aud_pll_clk"};

static struct clk *i2s1_pll_sels[] = {&osc_clk, &p_aud_pll_clk,};

DEFINE_ODIN_CLK_MUX(i2s1_pll_sel_clk, i2s1_pll_sels_name,
	i2s1_pll_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x060),
	ODIN_AUD_CRG_BASE+0x060, 0, 1, 0, NON_SECURE, &clk_out_lock);

static const char *i2s1_alt_sels_name[] = {"p_aud_pll_clk",};

static struct clk *i2s1_alt_sels[] = {&p_aud_pll_clk};

DEFINE_ODIN_CLK_MUX(i2s1_alt_sel_clk, i2s1_alt_sels_name,
	i2s1_alt_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x060),
	ODIN_AUD_CRG_BASE+0x60, 8, 3, 0, NON_SECURE, &clk_out_lock);

static const char *i2s1_src_sels_name[] = {"i2s1_pll_sel_clk", "i2s1_alt_sel_clk"};

static struct clk *i2s1_src_sels[] = {&i2s1_pll_sel_clk, &i2s1_alt_sel_clk,};

DEFINE_ODIN_CLK_MUX(i2s1_src_sel_clk, i2s1_src_sels_name,
	i2s1_src_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x060),
	ODIN_AUD_CRG_BASE+0x060, 4, 1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(i2s1_mask_clk, "i2s1_src_sel_clk",
	&i2s1_src_sel_clk, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x064),
	ODIN_AUD_CRG_BASE+0x064, 0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(i2s1_div_clk, "i2s1_mask_clk", &i2s1_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x068), NULL,
	ODIN_AUD_CRG_BASE+0x068, 0x0, 0, 8, 0, MACH_ODIN, AUD_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

/*I2S2 CLK*/
static const char *i2s2_pll_sels_name[] = {"osc_clk", "p_aud_pll_clk"};

static struct clk *i2s2_pll_sels[] = {&osc_clk, &p_aud_pll_clk,};

DEFINE_ODIN_CLK_MUX(i2s2_pll_sel_clk, i2s2_pll_sels_name,
	i2s2_pll_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x070),
	ODIN_AUD_CRG_BASE+0x070, 0, 1, 0, NON_SECURE, &clk_out_lock);

static const char *i2s2_alt_sels_name[] = {"p_aud_pll_clk",};

static struct clk *i2s2_alt_sels[] = {&p_aud_pll_clk};

DEFINE_ODIN_CLK_MUX(i2s2_alt_sel_clk, i2s2_alt_sels_name,
	i2s2_alt_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x070),
	ODIN_AUD_CRG_BASE+0x70, 8, 3, 0, NON_SECURE, &clk_out_lock);

static const char *i2s2_src_sels_name[] = {"i2s2_pll_sel_clk", "i2s2_alt_sel_clk"};

static struct clk *i2s2_src_sels[] = {&i2s2_pll_sel_clk, &i2s2_alt_sel_clk,};

DEFINE_ODIN_CLK_MUX(i2s2_src_sel_clk, i2s2_src_sels_name,
	i2s2_src_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x070),
	ODIN_AUD_CRG_BASE+0x070, 4, 1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(i2s2_mask_clk, "i2s2_src_sel_clk",
	&i2s2_src_sel_clk, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x074),
	ODIN_AUD_CRG_BASE+0x074, 0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(i2s2_div_clk, "i2s2_mask_clk", &i2s2_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x078), NULL,
	ODIN_AUD_CRG_BASE+0x078, 0x0, 0, 8, 0, MACH_ODIN, AUD_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

/*I2S3 CLK*/
static const char *i2s3_pll_sels_name[] = {"osc_clk", "p_aud_pll_clk"};

static struct clk *i2s3_pll_sels[] = {&osc_clk, &p_aud_pll_clk,};

DEFINE_ODIN_CLK_MUX(i2s3_pll_sel_clk, i2s3_pll_sels_name,
	i2s3_pll_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x080),
	ODIN_AUD_CRG_BASE+0x080, 0, 1, 0, NON_SECURE, &clk_out_lock);

static const char *i2s3_alt_sels_name[] = {"p_aud_pll_clk",};

static struct clk *i2s3_alt_sels[] = {&p_aud_pll_clk};

DEFINE_ODIN_CLK_MUX(i2s3_alt_sel_clk, i2s3_alt_sels_name,
	i2s3_alt_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x080),
	ODIN_AUD_CRG_BASE+0x80, 8, 3, 0, NON_SECURE, &clk_out_lock);

static const char *i2s3_src_sels_name[] = {"i2s3_pll_sel_clk", "i2s3_alt_sel_clk"};

static struct clk *i2s3_src_sels[] = {&i2s3_pll_sel_clk, &i2s3_alt_sel_clk,};

DEFINE_ODIN_CLK_MUX(i2s3_src_sel_clk, i2s3_src_sels_name,
	i2s3_src_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x080),
	ODIN_AUD_CRG_BASE+0x080, 4, 1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(i2s3_mask_clk, "i2s3_src_sel_clk",
	&i2s3_src_sel_clk, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x084),
	ODIN_AUD_CRG_BASE+0x084, 0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(i2s3_div_clk, "i2s3_mask_clk", &i2s3_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x088), NULL,
	ODIN_AUD_CRG_BASE+0x088, 0x0, 0, 8, 0, MACH_ODIN, AUD_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

/*I2S4 CLK*/
static const char *i2s4_pll_sels_name[] = {"osc_clk", "p_aud_pll_clk"};

static struct clk *i2s4_pll_sels[] = {&osc_clk, &p_aud_pll_clk,};

DEFINE_ODIN_CLK_MUX(i2s4_pll_sel_clk, i2s4_pll_sels_name,
	i2s4_pll_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x090),
	ODIN_AUD_CRG_BASE+0x090, 0, 1, 0, NON_SECURE, &clk_out_lock);

static const char *i2s4_alt_sels_name[] = {"p_aud_pll_clk",};

static struct clk *i2s4_alt_sels[] = {&p_aud_pll_clk};

DEFINE_ODIN_CLK_MUX(i2s4_alt_sel_clk, i2s4_alt_sels_name,
	i2s4_alt_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x090),
	ODIN_AUD_CRG_BASE+0x090, 8, 3, 0, NON_SECURE, &clk_out_lock);

static const char *i2s4_src_sels_name[] = {"i2s4_pll_sel_clk", "i2s4_alt_sel_clk"};

static struct clk *i2s4_src_sels[] = {&i2s4_pll_sel_clk, &i2s4_alt_sel_clk,};

DEFINE_ODIN_CLK_MUX(i2s4_src_sel_clk, i2s4_src_sels_name,
	i2s4_src_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x090),
	ODIN_AUD_CRG_BASE+0x090, 4, 1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(i2s4_mask_clk, "i2s4_src_sel_clk",
	&i2s4_src_sel_clk, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x094),
	ODIN_AUD_CRG_BASE+0x094, 0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(i2s4_div_clk, "i2s4_mask_clk", &i2s4_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x098), NULL,
	ODIN_AUD_CRG_BASE+0x098, 0x0, 0, 8, 0, MACH_ODIN, AUD_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

/*I2S5 CLK*/
static const char *i2s5_pll_sels_name[] = {"osc_clk", "p_aud_pll_clk"};

static struct clk *i2s5_pll_sels[] = {&osc_clk, &p_aud_pll_clk,};

DEFINE_ODIN_CLK_MUX(i2s5_pll_sel_clk, i2s5_pll_sels_name,
	i2s5_pll_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x0A0),
	ODIN_AUD_CRG_BASE+0x0A0, 0, 1, 0, NON_SECURE, &clk_out_lock);

static const char *i2s5_alt_sels_name[] = {"p_aud_pll_clk",};

static struct clk *i2s5_alt_sels[] = {&p_aud_pll_clk};

DEFINE_ODIN_CLK_MUX(i2s5_alt_sel_clk, i2s5_alt_sels_name,
	i2s5_alt_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x0A0),
	ODIN_AUD_CRG_BASE+0x0A0, 8, 3, 0, NON_SECURE, &clk_out_lock);

static const char *i2s5_src_sels_name[] = {"i2s5_pll_sel_clk", "i2s5_alt_sel_clk"};

static struct clk *i2s5_src_sels[] = {&i2s5_pll_sel_clk, &i2s5_alt_sel_clk,};

DEFINE_ODIN_CLK_MUX(i2s5_src_sel_clk, i2s5_src_sels_name,
	i2s5_src_sels, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x0A0),
	ODIN_AUD_CRG_BASE+0x0A0, 4, 1, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_GATE(i2s5_mask_clk, "i2s5_src_sel_clk",
	&i2s5_src_sel_clk, CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x0A4),
	ODIN_AUD_CRG_BASE+0x0A4, 0, 0, NON_SECURE, &clk_out_lock);

DEFINE_ODIN_CLK_DIVIDER(i2s5_div_clk, "i2s5_mask_clk", &i2s5_mask_clk,
	CLK_SET_RATE_PARENT, MMIO_P2V(ODIN_AUD_CRG_BASE+0x0A8), NULL,
	ODIN_AUD_CRG_BASE+0x0A8, 0x0, 0, 8, 0, MACH_ODIN, AUD_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

static struct clk_lookup odin_clk_lookup[] = {
	CLKDEV_INIT(NULL, "osc_clk", &osc_clk),

	/*PLL*/
	CLKDEV_INIT(NULL, "rtc_clk", &rtc_clk),
	CLKDEV_INIT(NULL, "ca15_pll_clk", &ca15_pll_clk),
	CLKDEV_INIT(NULL, "ca7_pll_clk", &ca7_pll_clk),
	CLKDEV_INIT(NULL, "mem_pll_clk", &mem_pll_clk),
	CLKDEV_INIT(NULL, "isp_pll_clk", &isp_pll_clk),
	CLKDEV_INIT(NULL, "noc_pll_clk", &noc_pll_clk),
	CLKDEV_INIT(NULL, "p_dss_pll_clk", &p_dss_pll_clk),
	CLKDEV_INIT(NULL, "p_gpu_pll_clk", &p_gpu_pll_clk),
	CLKDEV_INIT(NULL, "dp1_pll_clk", &dp1_pll_clk),
	CLKDEV_INIT(NULL, "dp2_pll_clk", &dp2_pll_clk),
	CLKDEV_INIT(NULL, "ics_hpll_clk", &ics_hpll_clk),
	CLKDEV_INIT(NULL, "p_aud_pll_clk", &p_aud_pll_clk),

	/*CORE*/
	CLKDEV_INIT(NULL, "scfg_pll_sel_clk", &scfg_pll_sel_clk),
	CLKDEV_INIT(NULL, "scfg_src_sel_clk", &scfg_src_sel_clk),
	CLKDEV_INIT(NULL, "scfg_mask_clk", &scfg_mask_clk),
	CLKDEV_INIT(NULL, "scfg_div_clk", &scfg_div_clk),
	CLKDEV_INIT(NULL, "snoc_pll_sel_clk", &snoc_pll_sel_clk),
	CLKDEV_INIT(NULL, "snoc_src_sel_clk", &snoc_src_sel_clk),
	CLKDEV_INIT(NULL, "snoc_mask_clk", &snoc_mask_clk),
	CLKDEV_INIT(NULL, "snoc_div_clk", &snoc_div_clk),
	CLKDEV_INIT(NULL, "anoc_pll_sel_clk", &anoc_pll_sel_clk),
	CLKDEV_INIT(NULL, "anoc_src_sel_clk", &anoc_src_sel_clk),
	CLKDEV_INIT(NULL, "anoc_mask_clk", &anoc_mask_clk),
	CLKDEV_INIT(NULL, "anoc_div_clk", &anoc_div_clk),
	CLKDEV_INIT(NULL, "gdnoc_pll_sel_clk", &gdnoc_pll_sel_clk),
	CLKDEV_INIT(NULL, "gdnoc_src_sel_clk", &gdnoc_src_sel_clk),
	CLKDEV_INIT(NULL, "gdnoc_mask_clk", &gdnoc_mask_clk),
	CLKDEV_INIT(NULL, "gdnoc_div_clk", &gdnoc_div_clk),
	CLKDEV_INIT(NULL, "memc_src_sel_clk", &memc_src_sel_clk),
	CLKDEV_INIT(NULL, "npll_src_sel_clk", &npll_src_sel_clk),
	CLKDEV_INIT(NULL, "npll_clk", &npll_clk),
	CLKDEV_INIT(NULL, "npll_dss_pll_sel_clk", &npll_dss_pll_sel_clk),
	CLKDEV_INIT(NULL, "npll_dss_src_sel_clk", &npll_dss_src_sel_clk),
	CLKDEV_INIT(NULL, "npll_clk_dss", &npll_clk_dss),

	/*CPU*/
	CLKDEV_INIT(NULL, "ca15_pll_sel_clk", &ca15_pll_sel_clk),
	CLKDEV_INIT(NULL, "ca15_div_clk", &ca15_div_clk),
	CLKDEV_INIT(NULL, "ca7_pll_sel_clk", &ca7_pll_sel_clk),
	CLKDEV_INIT(NULL, "ca7_div_clk", &ca7_div_clk),
	CLKDEV_INIT(NULL, "cclk_src_sel_clk", &cclk_src_sel_clk),
	CLKDEV_INIT(NULL, "cclk_mask_clk", &cclk_mask_clk),
	CLKDEV_INIT(NULL, "cclk_div_clk", &cclk_div_clk),
	CLKDEV_INIT(NULL, "hclk_src_sel_clk", &hclk_src_sel_clk),
	CLKDEV_INIT(NULL, "hclk_mask_clk", &hclk_mask_clk),
	CLKDEV_INIT(NULL, "hclk_div_clk", &hclk_div_clk),
	CLKDEV_INIT(NULL, "clk_cpucnt_src_sel_clk", &clk_cpucnt_src_sel_clk),
	CLKDEV_INIT(NULL, "clk_cpucnt_mask_clk", &clk_cpucnt_mask_clk),
	CLKDEV_INIT(NULL, "clk_cpucnt_div_clk", &clk_cpucnt_div_clk),
	CLKDEV_INIT(NULL, "pclk_sapb_src_sel_clk", &pclk_sapb_src_sel_clk),
	CLKDEV_INIT(NULL, "pclk_sapb_mask_clk", &pclk_sapb_mask_clk),
	CLKDEV_INIT(NULL, "pclk_sapb_div_clk", &pclk_sapb_div_clk),

	/*ICS*/
	CLKDEV_INIT(NULL, "ics_src_sel_clk", &ics_src_sel_clk),
	CLKDEV_INIT(NULL, "ics_mask_clk", &ics_mask_clk),
	CLKDEV_INIT(NULL, "ics_div_clk", &ics_div_clk),
	CLKDEV_INIT(NULL, "ics_gate_clk", &ics_gate_clk),
	CLKDEV_INIT(NULL, "ics_cclk_div_clk", &ics_cclk_div_clk),
	CLKDEV_INIT(NULL, "usbhsic_pmu_div_clk", &usbhsic_pmu_div_clk),
	CLKDEV_INIT(NULL, "usb30_suspend_div_clk", &usb30_suspend_div_clk),
	CLKDEV_INIT(NULL, "usb30_ref_div_clk", &usb30_ref_div_clk),
	CLKDEV_INIT(NULL, "hsi_clk_div_clk", &hsi_clk_div_clk),
	CLKDEV_INIT(NULL, "lli_clksvc_mask_clk", &lli_clksvc_mask_clk),
	CLKDEV_INIT(NULL, "lli_clksvc_div_clk", &lli_clksvc_div_clk),
	CLKDEV_INIT(NULL, "usbhsic_ref_mask_clk", &usbhsic_ref_mask_clk),
	CLKDEV_INIT(NULL, "usbhsic_ref_div_clk", &usbhsic_ref_div_clk),
	CLKDEV_INIT(NULL, "hsic_src_sel_clk", &hsic_src_sel_clk),
	CLKDEV_INIT(NULL, "hsic_mask_clk", &hsic_mask_clk),
	CLKDEV_INIT(NULL, "hsic_div_clk", &hsic_div_clk),


	/*CSS*/
	CLKDEV_INIT(NULL, "mipicfg_pll_sel_clk", &mipicfg_pll_sel_clk),
	CLKDEV_INIT(NULL, "mipicfg_src_sel_clk", &mipicfg_src_sel_clk),
	CLKDEV_INIT(NULL, "mipicfg_mask_clk", &mipicfg_mask_clk),
	CLKDEV_INIT(NULL, "mipicfg_div_clk", &mipicfg_div_clk),
	CLKDEV_INIT(NULL, "axi_pll_sel_clk", &axi_pll_sel_clk),
	CLKDEV_INIT(NULL, "axi_src_sel_clk", &axi_src_sel_clk),
	CLKDEV_INIT(NULL, "axi_mask_clk", &axi_mask_clk),
	CLKDEV_INIT(NULL, "axi_div_clk", &axi_div_clk),
	CLKDEV_INIT(NULL, "fg_clk", &fg_clk),
	CLKDEV_INIT(NULL, "isp_pll_sel_clk", &isp_pll_sel_clk),
	CLKDEV_INIT(NULL, "isp_src_sel_clk", &isp_src_sel_clk),
	CLKDEV_INIT(NULL, "isp_mask_clk", &isp_mask_clk),
	CLKDEV_INIT(NULL, "isp_div_clk", &isp_div_clk),
	CLKDEV_INIT(NULL, "isp0_clkx2", &isp0_clkx2),
	CLKDEV_INIT(NULL, "isp_div_clkx2", &isp_div_clkx2),
	CLKDEV_INIT(NULL, "isp0_clk", &isp0_clk),
	CLKDEV_INIT(NULL, "isp1_clk", &isp1_clk),
	CLKDEV_INIT(NULL, "pix0_isp_sel_clk", &pix0_isp_sel_clk),
	CLKDEV_INIT(NULL, "pix0_src_sel_clk", &pix0_src_sel_clk),
	CLKDEV_INIT(NULL, "pix1_isp_sel_clk", &pix1_isp_sel_clk),
	CLKDEV_INIT(NULL, "pix1_src_sel_clk", &pix1_src_sel_clk),
	CLKDEV_INIT(NULL, "vnr_pll_sel_clk", &vnr_pll_sel_clk),
	CLKDEV_INIT(NULL, "vnr_src_sel_clk", &vnr_src_sel_clk),
	CLKDEV_INIT(NULL, "vnr_mask_clk", &vnr_mask_clk),
	CLKDEV_INIT(NULL, "vnr_div_clk", &vnr_div_clk),
	CLKDEV_INIT(NULL, "fd_pll_sel_clk", &fd_pll_sel_clk),
	CLKDEV_INIT(NULL, "fd_src_sel_clk", &fd_src_sel_clk),
	CLKDEV_INIT(NULL, "fd_mask_clk", &fd_mask_clk),
	CLKDEV_INIT(NULL, "fd_div_clk", &fd_div_clk),

	/*PERI*/
	CLKDEV_INIT(NULL, "pnoc_a_src_sel_clk", &pnoc_a_src_sel_clk),
	CLKDEV_INIT(NULL, "pnoc_a_div_clk", &pnoc_a_div_clk),
	CLKDEV_INIT(NULL, "pclk_div_clk", &pclk_div_clk),
	CLKDEV_INIT(NULL, "wt_src_sel_clk", &wt_src_sel_clk),
	CLKDEV_INIT(NULL, "wt_mask_clk", &wt_mask_clk),
	CLKDEV_INIT(NULL, "wt_div_clk", &wt_div_clk),
	CLKDEV_INIT(NULL, "sdc_sclk_src_sel_clk", &sdc_sclk_src_sel_clk),
	CLKDEV_INIT(NULL, "sdc_sclk_mask_clk", &sdc_sclk_mask_clk),
	CLKDEV_INIT(NULL, "sdc_sclk_div_clk", &sdc_sclk_div_clk),
	CLKDEV_INIT(NULL, "sdio_sclk0_src_sel_clk", &sdio_sclk0_src_sel_clk),
	CLKDEV_INIT(NULL, "sdio_sclk0_mask_clk", &sdio_sclk0_mask_clk),
	CLKDEV_INIT(NULL, "sdio_sclk0_div_clk", &sdio_sclk0_div_clk),
	CLKDEV_INIT(NULL, "sdio_sclk1_src_sel_clk", &sdio_sclk1_src_sel_clk),
	CLKDEV_INIT(NULL, "sdio_sclk1_mask_clk", &sdio_sclk1_mask_clk),
	CLKDEV_INIT(NULL, "sdio_sclk1_div_clk", &sdio_sclk1_div_clk),
	CLKDEV_INIT(NULL, "emmc_sclk_src_sel_clk", &emmc_sclk_src_sel_clk),
	CLKDEV_INIT(NULL, "emmc_sclk_mask_clk", &emmc_sclk_mask_clk),
	CLKDEV_INIT(NULL, "emmc_sclk_div_clk", &emmc_sclk_div_clk),
	CLKDEV_INIT(NULL, "e2nand_sclk_src_sel_clk", &e2nand_sclk_src_sel_clk),
	CLKDEV_INIT(NULL, "e2nand_sclk_mask_clk", &e2nand_sclk_mask_clk),
	CLKDEV_INIT(NULL, "e2nand_sclk_div_clk", &e2nand_sclk_div_clk),
	CLKDEV_INIT(NULL, "i2c_sclk0_src_sel_clk", &i2c_sclk0_src_sel_clk),
	CLKDEV_INIT(NULL, "i2c_sclk0_mask_clk", &i2c_sclk0_mask_clk),
	CLKDEV_INIT(NULL, "i2c_sclk0_div_clk", &i2c_sclk0_div_clk),
	CLKDEV_INIT(NULL, "i2c_sclk1_src_sel_clk", &i2c_sclk1_src_sel_clk),
	CLKDEV_INIT(NULL, "i2c_sclk1_mask_clk", &i2c_sclk1_mask_clk),
	CLKDEV_INIT(NULL, "i2c_sclk1_div_clk", &i2c_sclk1_div_clk),
	CLKDEV_INIT(NULL, "i2c_sclk2_src_sel_clk", &i2c_sclk2_src_sel_clk),
	CLKDEV_INIT(NULL, "i2c_sclk2_mask_clk", &i2c_sclk2_mask_clk),
	CLKDEV_INIT(NULL, "i2c_sclk2_div_clk", &i2c_sclk2_div_clk),
	CLKDEV_INIT(NULL, "i2c_sclk3_src_sel_clk", &i2c_sclk3_src_sel_clk),
	CLKDEV_INIT(NULL, "i2c_sclk3_mask_clk", &i2c_sclk3_mask_clk),
	CLKDEV_INIT(NULL, "i2c_sclk3_div_clk", &i2c_sclk3_div_clk),
	CLKDEV_INIT(NULL, "spi_sclk0_src_sel_clk", &spi_sclk0_src_sel_clk),
	CLKDEV_INIT(NULL, "spi_sclk0_mask_clk", &spi_sclk0_mask_clk),
	CLKDEV_INIT(NULL, "spi_sclk0_div_clk", &spi_sclk0_div_clk),
	CLKDEV_INIT(NULL, "uart_sclk0_src_sel_clk", &uart_sclk0_src_sel_clk),
	CLKDEV_INIT(NULL, "uart_sclk0_mask_clk", &uart_sclk0_mask_clk),
	CLKDEV_INIT(NULL, "uart_sclk0_div_clk", &uart_sclk0_div_clk),
	CLKDEV_INIT(NULL, "uart_sclk1_src_sel_clk", &uart_sclk1_src_sel_clk),
	CLKDEV_INIT(NULL, "uart_sclk1_mask_clk", &uart_sclk1_mask_clk),
	CLKDEV_INIT(NULL, "uart_sclk1_div_clk", &uart_sclk1_div_clk),
	CLKDEV_INIT(NULL, "uart_sclk2_src_sel_clk", &uart_sclk2_src_sel_clk),
	CLKDEV_INIT(NULL, "uart_sclk2_mask_clk", &uart_sclk2_mask_clk),
	CLKDEV_INIT(NULL, "uart_sclk2_div_clk", &uart_sclk2_div_clk),
	CLKDEV_INIT(NULL, "uart_sclk3_src_sel_clk", &uart_sclk3_src_sel_clk),
	CLKDEV_INIT(NULL, "uart_sclk3_mask_clk", &uart_sclk3_mask_clk),
	CLKDEV_INIT(NULL, "uart_sclk3_div_clk", &uart_sclk3_div_clk),
	CLKDEV_INIT(NULL, "uart_sclk4_src_sel_clk", &uart_sclk4_src_sel_clk),
	CLKDEV_INIT(NULL, "uart_sclk4_mask_clk", &uart_sclk4_mask_clk),
	CLKDEV_INIT(NULL, "uart_sclk4_div_clk", &uart_sclk4_div_clk),

	/*DSS*/
	CLKDEV_INIT(NULL, "core_src_sel_clk", &core_src_sel_clk),
	CLKDEV_INIT(NULL, "gfxcore_src_sel_clk", &gfxcore_src_sel_clk),
	CLKDEV_INIT(NULL, "gfx_div_clk", &gfx_div_clk),
	CLKDEV_INIT(NULL, "disp0_src_sel_clk", &disp0_src_sel_clk),
	CLKDEV_INIT(NULL, "disp0_div_clk", &disp0_div_clk),
	CLKDEV_INIT(NULL, "disp1_src_sel_clk", &disp1_src_sel_clk),
	CLKDEV_INIT(NULL, "disp1_div_clk", &disp1_div_clk),
	CLKDEV_INIT(NULL, "hdmi_src_sel_clk", &hdmi_src_sel_clk),
	CLKDEV_INIT(NULL, "hdmi_div_clk", &hdmi_div_clk),
	CLKDEV_INIT(NULL, "osc_src_sel_clk", &osc_src_sel_clk),
	CLKDEV_INIT(NULL, "cec_src_sel_clk", &cec_src_sel_clk),
	CLKDEV_INIT(NULL, "tx_esc_src_sel_clk", &tx_esc_src_sel_clk),
	CLKDEV_INIT(NULL, "tx_esc_div_clk", &tx_esc_div_clk),

	/*GPU*/
	CLKDEV_INIT(NULL, "gpu_mem_src_sel_clk", &gpu_mem_src_sel_clk),
	CLKDEV_INIT(NULL, "gpu_mem_mask_clk", &gpu_mem_mask_clk),
	CLKDEV_INIT(NULL, "gpu_mem_div_clk", &gpu_mem_div_clk),
	CLKDEV_INIT(NULL, "gpu_sys_div_clk", &gpu_sys_div_clk),
	CLKDEV_INIT(NULL, "gpu_core_src_sel_clk", &gpu_core_src_sel_clk),
	CLKDEV_INIT(NULL, "gpu_core_mask_clk", &gpu_core_mask_clk),
	CLKDEV_INIT(NULL, "gpu_core_div_clk", &gpu_core_div_clk),

	/*VSP*/
	CLKDEV_INIT(NULL, "vsp0_c_codec_src_sel_clk", &vsp0_c_codec_src_sel_clk),
	CLKDEV_INIT(NULL, "vsp0_c_codec_mask_clk", &vsp0_c_codec_mask_clk),
	CLKDEV_INIT(NULL, "vsp0_c_codec_div_clk", &vsp0_c_codec_div_clk),
	CLKDEV_INIT(NULL, "vsp0_e_codec_src_sel_clk", &vsp0_e_codec_src_sel_clk),
	CLKDEV_INIT(NULL, "vsp0_e_codec_mask_clk", &vsp0_e_codec_mask_clk),
	CLKDEV_INIT(NULL, "vsp0_e_codec_div_clk", &vsp0_e_codec_div_clk),
	CLKDEV_INIT(NULL, "vsp0_aclk_src_sel_clk", &vsp0_aclk_src_sel_clk),
	CLKDEV_INIT(NULL, "vsp0_aclk_mask_clk", &vsp0_aclk_mask_clk),
	CLKDEV_INIT(NULL, "vsp0_aclk_div_clk", &vsp0_aclk_div_clk),
	CLKDEV_INIT(NULL, "vsp0_pclk_src_sel_clk", &vsp0_pclk_src_sel_clk),
	CLKDEV_INIT(NULL, "vsp0_pclk_mask_clk", &vsp0_pclk_mask_clk),
	CLKDEV_INIT(NULL, "vsp0_pclk_div_clk", &vsp0_pclk_div_clk),
	CLKDEV_INIT(NULL, "vsp1_dec_src_sel_clk", &vsp1_dec_src_sel_clk),
	CLKDEV_INIT(NULL, "vsp1_dec_mask_clk", &vsp1_dec_mask_clk),
	CLKDEV_INIT(NULL, "vsp1_dec_div_clk", &vsp1_dec_div_clk),
	CLKDEV_INIT(NULL, "vsp1_img_src_sel_clk", &vsp1_img_src_sel_clk),
	CLKDEV_INIT(NULL, "vsp1_img_mask_clk", &vsp1_img_mask_clk),
	CLKDEV_INIT(NULL, "vsp1_img_div_clk", &vsp1_img_div_clk),
	CLKDEV_INIT(NULL, "vsp1_aclk_src_sel_clk", &vsp1_aclk_src_sel_clk),
	CLKDEV_INIT(NULL, "vsp1_aclk_mask_clk", &vsp1_aclk_mask_clk),
	CLKDEV_INIT(NULL, "vsp1_aclk_div_clk", &vsp1_aclk_div_clk),
	CLKDEV_INIT(NULL, "vsp1_pclk_src_sel_clk", &vsp1_pclk_src_sel_clk),
	CLKDEV_INIT(NULL, "vsp1_pclk_mask_clk", &vsp1_pclk_mask_clk),
	CLKDEV_INIT(NULL, "vsp1_pclk_div_clk", &vsp1_pclk_div_clk),

	/*AUD*/
	CLKDEV_INIT(NULL, "dsp_cclk_src_sel_clk", &dsp_cclk_src_sel_clk),
	CLKDEV_INIT(NULL, "dsp_cclk_mask_clk", &dsp_cclk_mask_clk),
	CLKDEV_INIT(NULL, "dsp_cclk_div_clk", &dsp_cclk_div_clk),
	CLKDEV_INIT(NULL, "aclk_dsp_div_clk", &aclk_dsp_div_clk),
	CLKDEV_INIT(NULL, "pclk_dsp_div_clk", &pclk_dsp_div_clk),
	CLKDEV_INIT(NULL, "p_async_src_sel_clk", &p_async_src_sel_clk),
	CLKDEV_INIT(NULL, "p_async_mask_clk", &p_async_mask_clk),
	CLKDEV_INIT(NULL, "p_async_div_clk", &p_async_div_clk),
	CLKDEV_INIT(NULL, "slimbus_src_sel_clk", &slimbus_src_sel_clk),
	CLKDEV_INIT(NULL, "slimbus_mask_clk", &slimbus_mask_clk),
	CLKDEV_INIT(NULL, "slimbus_div_clk", &slimbus_div_clk),
	CLKDEV_INIT(NULL, "i2s0_pll_sel_clk", &i2s0_pll_sel_clk),
	CLKDEV_INIT(NULL, "i2s0_alt_sel_clk", &i2s0_alt_sel_clk),
	CLKDEV_INIT(NULL, "i2s0_src_sel_clk", &i2s0_src_sel_clk),
	CLKDEV_INIT(NULL, "i2s0_mask_clk", &i2s0_mask_clk),
	CLKDEV_INIT(NULL, "i2s0_div_clk", &i2s0_div_clk),
	CLKDEV_INIT(NULL, "i2s1_pll_sel_clk", &i2s1_pll_sel_clk),
	CLKDEV_INIT(NULL, "i2s1_alt_sel_clk", &i2s1_alt_sel_clk),
	CLKDEV_INIT(NULL, "i2s1_src_sel_clk", &i2s1_src_sel_clk),
	CLKDEV_INIT(NULL, "i2s1_mask_clk", &i2s1_mask_clk),
	CLKDEV_INIT(NULL, "i2s1_div_clk", &i2s1_div_clk),
	CLKDEV_INIT(NULL, "i2s2_pll_sel_clk", &i2s2_pll_sel_clk),
	CLKDEV_INIT(NULL, "i2s2_alt_sel_clk", &i2s2_alt_sel_clk),
	CLKDEV_INIT(NULL, "i2s2_src_sel_clk", &i2s2_src_sel_clk),
	CLKDEV_INIT(NULL, "i2s2_mask_clk", &i2s2_mask_clk),
	CLKDEV_INIT(NULL, "i2s2_div_clk", &i2s2_div_clk),
	CLKDEV_INIT(NULL, "i2s3_pll_sel_clk", &i2s3_pll_sel_clk),
	CLKDEV_INIT(NULL, "i2s3_alt_sel_clk", &i2s3_alt_sel_clk),
	CLKDEV_INIT(NULL, "i2s3_src_sel_clk", &i2s3_src_sel_clk),
	CLKDEV_INIT(NULL, "i2s3_mask_clk", &i2s3_mask_clk),
	CLKDEV_INIT(NULL, "i2s3_div_clk", &i2s3_div_clk),
	CLKDEV_INIT(NULL, "i2s4_pll_sel_clk", &i2s4_pll_sel_clk),
	CLKDEV_INIT(NULL, "i2s4_alt_sel_clk", &i2s4_alt_sel_clk),
	CLKDEV_INIT(NULL, "i2s4_src_sel_clk", &i2s4_src_sel_clk),
	CLKDEV_INIT(NULL, "i2s4_mask_clk", &i2s4_mask_clk),
	CLKDEV_INIT(NULL, "i2s4_div_clk", &i2s4_div_clk),
	CLKDEV_INIT(NULL, "i2s5_pll_sel_clk", &i2s5_pll_sel_clk),
	CLKDEV_INIT(NULL, "i2s5_alt_sel_clk", &i2s5_alt_sel_clk),
	CLKDEV_INIT(NULL, "i2s5_src_sel_clk", &i2s5_src_sel_clk),
	CLKDEV_INIT(NULL, "i2s5_mask_clk", &i2s5_mask_clk),
	CLKDEV_INIT(NULL, "i2s5_div_clk", &i2s5_div_clk),
};

int __init odin_default_clk_enable(void)
{

	struct device_node *np, *from;
	struct clk *clk;
	int ret = 1;

	from = of_find_node_by_name(NULL, "clocks");

	for(np = of_get_next_child(from, NULL); np; np=of_get_next_child(from, np)){

		u32 val[5];

		of_property_read_u32_array(np, "reg-flag", val, ARRAY_SIZE(val));

		clk = clk_get(NULL, np->name);

		ret = clk_prepare_enable(clk);
		if(ret)
		{
			printk(KERN_ERR "can't enable %s\n", clk->name);
			return ret;
		}
	}

	return ret;
}

int odin_dt_clk_init(void)
{
	struct clk *clk = NULL;
	struct device_node *np, *from;
	const char *parent_name;

	from = of_find_node_by_name(NULL, "clocks");

	for(np = of_get_next_child(from, NULL); np; np=of_get_next_child(from, np)){

			u32 val[6];

			of_property_read_u32_array(np, "reg-flag", val, ARRAY_SIZE(val));
			of_property_read_string(np, "clock-parent", &parent_name);

			clk = odin_clk_register_gate(NULL, np->name, parent_name,
			val[1], MMIO_P2V(val[2]), val[2], val[3], val[4], val[5], &clk_out_lock);

			of_clk_add_provider(np, of_clk_src_simple_get, clk);
			clk_register_clkdev(clk, np->name, NULL);

			printk("%s's rate:%lu  parent:%s\n", clk->name,
			clk_get_rate(clk), __clk_get_name(clk_get_parent(clk)));
	}

	return 0;
}

#define to_lj_pll(_hw) container_of(_hw, struct lj_pll, hw)

int odin_clk_init(void)
{
	struct clk_lookup *c;
	void __iomem *init_clk = NULL;
	int ret;

	//mb_clk_probe();

	for(c = odin_clk_lookup; c < odin_clk_lookup + ARRAY_SIZE(odin_clk_lookup); c++ )
	{
		if(!(c->clk)->secure_flag)
		{
			init_clk = ioremap((c->clk)->reg_val, SZ_4K);
			(c->clk)->reg_val = readl(init_clk);
		}else{
#ifdef CONFIG_ODIN_TEE
			(c->clk)->reg_val = tz_read((c->clk)->reg_val);
#else
			init_clk = ioremap((c->clk)->reg_val, SZ_4K);
			(c->clk)->reg_val = readl(init_clk);
#endif
		}

		clkdev_add(c);
		__clk_init(NULL, c->clk);
		printk("%s's rate:%lu  reg_val: %lx\n", __clk_get_name(c->clk), __clk_get_rate(c->clk),
		(c->clk)->reg_val);

		if(!(c->clk)->secure_flag){
			iounmap(init_clk);
		}else{
#ifndef CONFIG_ODIN_TEE
			iounmap(init_clk);
#endif
		}
	}

	odin_dt_clk_init();
	odin_default_clk_enable();

	return 0;
}

int odin_clk_mem(void){

	iotable_init(odin_clk_io_desc, ARRAY_SIZE(odin_clk_io_desc));

	return 0;
}
