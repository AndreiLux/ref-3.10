/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/clk-private.h>
#include <linux/clkdev.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#ifdef CONFIG_WODEN_SLEEP
#include <linux/syscore_ops.h>
#endif

#include "clk.h"

#define COM_CLK 1
#define CPU_CLK 2
#define AUD_CLK 3

#define CLK_DISABLE 1

static DEFINE_SPINLOCK(clk_out_lock);

#ifdef CONFIG_WODEN_SLEEP

static struct reg_save woden_clk_save[] = {

	//GPU
	SAVE_REG(GPU_PLL_CON0),
	SAVE_REG(GPU_PLL_CON1),
	SAVE_REG(GPU_CCLKSRC_SEL),
	SAVE_REG(GPU_CCLKSRC_MASK),
	SAVE_REG(GPU_CCLK_DIV),
	SAVE_REG(GPU_CCLK_GATE),

	//DSS
	SAVE_REG(DSS_PLL_CON0),
	SAVE_REG(DSS_PLL_CON1),
	SAVE_REG(DSS_FCLKSRC_SEL),
	SAVE_REG(DSS_FCLKSRC_MASK),
	SAVE_REG(DSS_FCLK_DIV),
	SAVE_REG(DSS_FCLK_GATE),
	SAVE_REG(DSS_ACLKSRC_SEL),
	SAVE_REG(DSS_ACLKSRC_MASK),
	SAVE_REG(DSS_ACLK_DIV),
	SAVE_REG(DSS_ACLK_GATE),
	SAVE_REG(DSS_HCLKSRC_SEL),
	SAVE_REG(DSS_HCLKSRC_MASK),
	SAVE_REG(DSS_HCLK_DIV),
	SAVE_REG(DSS_HCLK_GATE),
	SAVE_REG(DBUS_CLKSRC_SEL),
	SAVE_REG(DBUS_CLKSRC_MASK),
	SAVE_REG(DBUS_CLK_DIV),
	SAVE_REG(DBUS_CLK_GATE),

	//CORE
	SAVE_REG(CORE_PLL_CON0),
	SAVE_REG(CORE_PLL_CON1),
	SAVE_REG(ABUS_CLKSRC_SEL),
	SAVE_REG(ABUS_CLKSRC_MASK),
	SAVE_REG(ABUS_CLK_DIV),
	SAVE_REG(ABUS_CLK_GATE),
	SAVE_REG(GBUS_CLKSRC_SEL),
	SAVE_REG(GBUS_CLKSRC_MASK),
	SAVE_REG(GBUS_CLK_DIV),
	SAVE_REG(GBUS_CLK_GATE),
	SAVE_REG(PBUS_CLKSRC_SEL),
	SAVE_REG(PBUS_CLKSRC_MASK),
	SAVE_REG(PBUS_CLK_DIV),
	SAVE_REG(PBUS_CLK_GATE0),
	SAVE_REG(PBUS_CLK_GATE1),

	//ICS
	SAVE_REG(ICS_ACLKSRC_SEL),
	SAVE_REG(ICS_ACLKSRC_MASK),
	SAVE_REG(ICS_ACLK_DIV),
	SAVE_REG(ICS_ACLK_GATE),

	//MMC
	SAVE_REG(MMC0_SCLKSRC_SEL),
	SAVE_REG(MMC0_SCLKSRC_MASK),
	SAVE_REG(MMC0_SCLK_DIV),
	SAVE_REG(MMC0_SCLK_GATE),

	//SDIO
	SAVE_REG(SDIO0_SCLKSRC_SEL),
	SAVE_REG(SDIO0_SCLKSRC_MASK),
	SAVE_REG(SDIO0_SCLK_DIV),
	SAVE_REG(SDIO0_SCLK_GATE),

	//CPU
	SAVE_REG(CPU0_CLKSRC_SEL),
	SAVE_REG(CPU0_CLK_DIV0),
	SAVE_REG(CPU0_CLK_DIV1),
	SAVE_REG(CPU1_CLK_DIV0),
	SAVE_REG(CPU1_CLK_DIV_SEL),
	SAVE_REG(CPU1_CLK_CCI_GIC_CNT),
	SAVE_REG(CPU_PLL_CON),
	SAVE_REG(TRACE_CLK_REG),

};

#endif //CONFIG_WODEN_SLEEP end

static void __init woden_audio_clk_setup(void __iomem *base)
{
#if 1
	writel(0x1, base + 0x10);	/* dmc clodk select = 360Mhz*/
	writel(0x1, base + 0x10);	/* dmc clodk select = 360Mhz*/
	writel(0x1, base + 0x18);	/* div_CCLK 1 = 180Mhz */
	writel(0x1, base + 0x18);	/* div_CCLK 1 = 180Mhz */
	writel(0x1, base + 0x1C);	/* DSP enable */
	writel(0x1, base + 0x1C);	/* DSP enable */
	writel(0x1, base + 0x20);	/* div_ACLK 1 = 90Mhz*/
	writel(0x1, base + 0x20);	/* div_ACLK 1 = 90Mhz*/
	writel(0x3, base + 0x28);	/* div_PCLK 2 = 45Mhz*/
	writel(0x3, base + 0x28);	/* div_PCLK 2 = 45Mhz*/
#else
	writel(0x1, base + 0x10);	/* dmc clodk select = 360Mhz*/
	writel(0x1, base + 0x10);	/* dmc clodk select = 360Mhz*/
	writel(0xB, base + 0x18);	/* Divider > dspClkDiv (360 MHz / (11+1) = 30 MHz) */
	writel(0xB, base + 0x18);	/* Divider > dspClkDiv (360 MHz / (11+1) = 30 MHz) */
	writel(0x1, base + 0x1C);	/* DSP enable */
	writel(0x1, base + 0x1C);	/* DSP enable */
	writel(0x1, base + 0x20);	/* Divider > axiClkDiv (30 MHz / (1+1) = 15 MHz) */
	writel(0x1, base + 0x20);	/* Divider > axiClkDiv (30 MHz / (1+1) = 15 MHz)*/
	writel(0x1, base + 0x28);	/* Divider > axiClkDiv (30 MHz / (1+1) = 15 MHz)*/
	writel(0x1, base + 0x28);	/* Divider > axiClkDiv (30 MHz / (1+1) = 15 MHz)*/

#endif
	/* Audio Async PCLK setting */
	writel(0x0, base + 0x30);      /* Main clock source= OSC_CLK */
	writel(0x0, base + 0x30);      /* Main clock source= OSC_CLK */
	writel(0x0, base + 0x34);      /* div= 0 */
	writel(0x0, base + 0x34);      /* div= 0 */
	writel(0x1, base + 0x38);      /* gate = clock enable */
	writel(0x1, base + 0x38);      /* gate = clock enable */

	writel(0x01, base + 0x3c);      /* I2S0 AUD PLL base*/
	writel(0x01, base + 0x48);      /* I2S1 AUD PLL base*/
	writel(0x01, base + 0x54);      /* I2S2 AUD PLL base*/
	writel(0x01, base + 0x60);      /* I2S3 AUD PLL base*/
	writel(0x01, base + 0x6c);      /* I2S4 AUD PLL base*/
	writel(0x01, base + 0x78);      /* I2S5 AUD PLL base*/
}

static const char *dmc0_pll_sels_name[] = {"osc_clk", "dmc_pll_clk"};
static const char *dmc1_pll_sels_name[] = {"osc_clk", "dmc_pll_clk"};
static const char *sms_dmc_src_clk_sels_name[] = {"dmc0_pll_sel_clk", "dmc1_pll_sel_clk"};
static const char *cclk_gpu_pll_sels_name[] = {"osc_clk", "gpu_pll_clk"};
static const char *fclk_dss_pll_sels_name[] = {"osc_clk", "dss_pll_clk"};
static const char *aclk_dss_pll_sels_name[] = {"osc_clk", "core_pll_clk"};
static const char *hclk_dss_pll_sels_name[] = {"osc_clk", "core_pll_clk"};
static const char *abus_core_pll_sels_name[] = {"osc_clk", "core_pll_clk"};
static const char *dbus_core_pll_sels_name[] = {"osc_clk", "core_pll_clk"};
static const char *gbus_core_pll_sels_name[] = {"osc_clk", "core_pll_clk"};
static const char *pbus_core_pll_sels_name[] = {"osc_clk", "core_pll_clk"};
static const char *ics_aclk_core_pll_sels_name[] = {"osc_clk", "core_pll_clk"};
static const char *mmc0_core_pll_sels_name[] = {"osc_clk", "core_pll_clk"};
static const char *sdio0_core_pll_sels_name[] = {"osc_clk", "core_pll_clk"};

static const char *dmc0_alt_sels_name[] = {"sms_sms_dmc_src_clk",
	"osc_clk", "dbus_core_pll_sel_clk", "fclk_dss_pll_sel_clk"};
static const char *dmc1_alt_sels_name[] = {"sms_sms_dmc_src_clk",
	"osc_clk", "dbus_core_pll_sel_clk", "fclk_dss_pll_sel_clk"};
static const char *cclk_gpu_alt_sels_name[] = {"sms_sms_dmc_src_clk",
	"osc_clk", "dbus_core_pll_sel_clk", "fclk_dss_pll_sel_clk"};
static const char *fclk_dss_alt_sels_name[] = {"sms_sms_dmc_src_clk",
	"cclk_gpu_pll_sel_clk", "osc_clk", "fclk_dss_pll_sel_clk"};
static const char *aclk_dss_alt_sels_name[] = {"sms_sms_dmc_src_clk",
	"cclk_gpu_pll_sel_clk", "osc_clk", "fclk_dss_pll_sel_clk"};
static const char *hclk_dss_alt_sels_name[] = {"sms_sms_dmc_src_clk",
	"cclk_gpu_pll_sel_clk", "osc_clk", "fclk_dss_pll_sel_clk"};
static const char *abus_core_alt_sels_name[] = {"sms_sms_dmc_src_clk",
	"cclk_gpu_pll_sel_clk", "osc_clk", "fclk_dss_pll_sel_clk"};
static const char *dbus_core_alt_sels_name[] = {"sms_sms_dmc_src_clk",
	"cclk_gpu_pll_sel_clk", "osc_clk", "fclk_dss_pll_sel_clk"};
static const char *gbus_core_alt_sels_name[] = {"sms_sms_dmc_src_clk",
	"cclk_gpu_pll_sel_clk", "osc_clk", "fclk_dss_pll_sel_clk"};
static const char *pbus_core_alt_sels_name[] = {"sms_sms_dmc_src_clk",
	"cclk_gpu_pll_sel_clk", "osc_clk", "fclk_dss_pll_sel_clk"};
static const char *ics_aclk_core_alt_sels_name[] = {"sms_sms_dmc_src_clk",
	"cclk_gpu_pll_sel_clk", "osc_clk", "fclk_dss_pll_sel_clk"};
static const char *mmc0_core_alt_sels_name[] = {"sms_sms_dmc_src_clk",
	"cclk_gpu_pll_sel_clk", "osc_clk", "fclk_dss_pll_sel_clk"};
static const char *sdio0_core_alt_sels_name[] = {"sms_sms_dmc_src_clk",
	"cclk_gpu_pll_sel_clk", "osc_clk", "fclk_dss_pll_sel_clk"};

static const char *dmc0_clksrc_sels_name[] = {"dmc0_pll_sel_clk",
	"dmc0_alt_sel_clk"};
static const char *dmc1_clksrc_sels_name[] = {"dmc1_pll_sel_clk",
	"dmc1_alt_sel_clk"};
static const char *cclk_gpu_clksrc_sels_name[] = {"cclk_gpu_pll_sel_clk",
	"cclk_gpu_alt_sel_clk"};
static const char *fclk_dss_clksrc_sels_name[] = {"fclk_dss_pll_sel_clk",
	"fclk_dss_alt_sel_clk"};
static const char *aclk_dss_clksrc_sels_name[] = {"aclk_dss_pll_sel_clk",
	"aclk_dss_alt_sel_clk"};
static const char *hclk_dss_clksrc_sels_name[] = {"hclk_dss_pll_sel_clk",
	"hclk_dss_alt_sel_clk"};
static const char *abus_core_clksrc_sels_name[] = {"abus_core_pll_sel_clk",
	"abus_core_alt_sel_clk"};
static const char *dbus_core_clksrc_sels_name[] = {"dbus_core_pll_sel_clk",
	"dbus_core_alt_sel_clk"};
static const char *gbus_core_clksrc_sels_name[] = {"gbus_core_pll_sel_clk",
	"gbus_core_alt_sel_clk"};
static const char *pbus_core_clksrc_sels_name[] = {"pbus_core_pll_sel_clk",
	"pbus_core_alt_sel_clk"};
static const char *ics_aclk_core_clksrc_sels_name[] = {"ics_aclk_core_pll_sel_clk",
	"ics_aclk_core_alt_sel_clk"};
static const char *mmc0_core_clksrc_sels_name[] = {"mmc0_core_pll_sel_clk",
	"mmc0_core_alt_sel_clk"};
static const char *sdio0_core_clksrc_sels_name[] = {"sdio0_core_pll_sel_clk",
	"sdio0_core_alt_sel_clk"};

/*CPU*/
static const char *ca15_cpu_pll_sels_name[] = {"osc_clk", "cpu_pll_clk"};
static const char *ca7_cpu_pll_sels_name[] = {"osc_clk", "cpu_pll_clk"};
static const char *trace_at_sels_name[] = {"osc_clk", "ca7_atclken_div"};

static const char *ca15_cpu_alt_sels_name[] = {"sms_sms_dmc_src_clk",
	"cclk_gpu_pll_sel_clk", "osc_clk", "fclk_dss_pll_sel_clk"};
static const char *ca7_cpu_alt_sels_name[] = {"sms_sms_dmc_src_clk",
	"cclk_gpu_pll_sel_clk", "gbus_core_pll_sel_clk", "fclk_dss_pll_sel_clk"};
static const char *trace_alt_sels_name[] = {"sms_sms_dmc_src_clk",
	"cclk_gpu_pll_sel_clk", "dbus_core_pll_sel_clk", "fclk_dss_pll_sel_clk"};

static const char *ca15_cpu_clksrc_sels_name[] = {"ca15_cpu_pll_sel_clk",
	"ca15_cpu_alt_sel_clk"};
static const char *ca7_cpu_clksrc_sels_name[] = {"ca7_cpu_pll_sel_clk",
	"ca7_cpu_alt_sel_clk"};
static const char *cpucnt_clksrc_sels_name[] = {"ca7_cpu_alt_sel_clk",
	"osc_clk"};
static const char *trace_clksrc_sels_name[] = {"trace_at_sel_clk",
	"trace_alt_sel_clk"};

/*AUD*/

static const char *dsp_cclk_sels_name[] = {"osc_clk", "dmc_pll_clk"};
static const char *aud_apclk_sels0_name[] = {"osc_clk", "dmc_pll_clk"};
static const char *dsp_alt_sels_name[] = {"cclk_gpu_pll_sel_clk",
	"isp_pll", "gbus_core_pll_sel_clk", "fclk_dss_pll_sel_clk"};
static const char *apclk_saltclk_sels_name[] = {"cclk_gpu_pll_sel_clk",
	"osc_clk", "gbus_core_pll_sel_clk", "fclk_dss_pll_sel_clk"};
static const char *dsp_clksrc_sels_name[] = {"dsp_cclk_sel_clk",
	"dsp_alt_sel_clk"};
static const char *aud_apclk_sels1_name[] = {"aud_apclk_sel0_clk", "apclk_saltclk_sel_clk"};

static const char *i2s0_aud_pll_sels_name[] = {"osc_clk", "aud_pll_clk"};
static const char *i2s0_aud_alt_sels_name[] = {"mclk_i2s0", "mclk_i2s1",
	"mclk_i2s2", "mclk_i2s3", "mclk_i2s4", "mclk_i2s5"};
static const char *i2s0_aud_clksrc_sels_name[] = {"i2s0_aud_pll_sel_clk",
	"i2s0_aud_alt_sel_clk"};

static const char *i2s1_aud_pll_sels_name[] = {"osc_clk", "aud_pll_clk"};
static const char *i2s1_aud_alt_sels_name[] = {"mclk_i2s0", "mclk_i2s1",
	"mclk_i2s2", "mclk_i2s3", "mclk_i2s4", "mclk_i2s5"};
static const char *i2s1_aud_clksrc_sels_name[] = {"i2s1_aud_pll_sel_clk",
	"i2s1_aud_alt_sel_clk"};

static const char *i2s2_aud_pll_sels_name[] = {"osc_clk", "aud_pll_clk"};
static const char *i2s2_aud_alt_sels_name[] = {"mclk_i2s0", "mclk_i2s1",
	"mclk_i2s2", "mclk_i2s3", "mclk_i2s4", "mclk_i2s5"};
static const char *i2s2_aud_clksrc_sels_name[] = {"i2s2_aud_pll_sel_clk",
	"i2s2_aud_alt_sel_clk"};

static const char *i2s3_aud_pll_sels_name[] = {"osc_clk", "aud_pll_clk"};
static const char *i2s3_aud_alt_sels_name[] = {"mclk_i2s0", "mclk_i2s1",
	"mclk_i2s2", "mclk_i2s3", "mclk_i2s4", "mclk_i2s5"};
static const char *i2s3_aud_clksrc_sels_name[] = {"i2s3_aud_pll_sel_clk",
	"i2s3_aud_alt_sel_clk"};

static const char *i2s4_aud_pll_sels_name[] = {"osc_clk", "aud_pll_clk"};
static const char *i2s4_aud_alt_sels_name[] = {"mclk_i2s0", "mclk_i2s1",
	"mclk_i2s2", "mclk_i2s3", "mclk_i2s4", "mclk_i2s5"};
static const char *i2s4_aud_clksrc_sels_name[] = {"i2s4_aud_pll_sel_clk",
	"i2s4_aud_alt_sel_clk"};

static const char *i2s5_aud_pll_sels_name[] = {"osc_clk", "aud_pll_clk"};
static const char *i2s5_aud_alt_sels_name[] = {"mclk_i2s0", "mclk_i2s1",
	"mclk_i2s2", "mclk_i2s3", "mclk_i2s4", "mclk_i2s5"};
static const char *i2s5_aud_clksrc_sels_name[] = {"i2s5_aud_pll_sel_clk",
	"i2s5_aud_alt_sel_clk"};

enum woden_clocks {
	osc_clk, dmc_pll_clk, gpu_pll_clk, dss_pll_clk, core_pll_clk, dmc0_pll_sel_clk,

	dmc1_pll_sel_clk, sms_sms_dmc_src_clk, cclk_gpu_pll_sel_clk, fclk_dss_pll_sel_clk,
	aclk_dss_pll_sel_clk, hclk_dss_pll_sel_clk, abus_core_pll_sel_clk,
	dbus_core_pll_sel_clk, gbus_core_pll_sel_clk, pbus_core_pll_sel_clk,
	ics_aclk_core_pll_sel_clk, mmc0_core_pll_sel_clk, sdio0_core_pll_sel_clk,

	dmc0_alt_sel_clk, dmc1_alt_sel_clk, cclk_gpu_alt_sel_clk, fclk_dss_alt_sel_clk,
	aclk_dss_alt_sel_clk, hclk_dss_alt_sel_clk, abus_core_alt_sel_clk,
	dbus_core_alt_sel_clk, gbus_core_alt_sel_clk, pbus_core_alt_sel_clk,
	ics_aclk_core_alt_sel_clk, mmc0_core_alt_sel_clk, sdio0_core_alt_sel_clk,

	dmc0_clksrc_sel_clk, dmc1_clksrc_sel_clk, cclk_gpu_clksrc_sel_clk, fclk_dss_clksrc_sel_clk,
	aclk_dss_clksrc_sel_clk, hclk_dss_clksrc_sel_clk, abus_core_clksrc_sel_clk,
	dbus_core_clksrc_sel_clk, gbus_core_clksrc_sel_clk, pbus_core_clksrc_sel_clk,
	ics_aclk_core_clksrc_sel_clk, mmc0_core_clksrc_sel_clk, sdio0_core_clksrc_sel_clk,

	dmc0_clksrc_mask, dmc1_clksrc_mask, cclk_gpu_clksrc_mask, fclk_dss_clksrc_mask,
	aclk_dss_clksrc_mask, hclk_dss_clksrc_mask, abus_core_clksrc_mask, dbus_core_clksrc_mask,
	gbus_core_clksrc_mask, pbus_core_clksrc_mask, ics_aclk_core_clksrc_mask, mmc0_core_clksrc_mask,
	sdio0_core_clksrc_mask,

	mclk_dmc0_x0_div, mclk_dmc0_x2_div, mclk_dmc1_x0_div, mclk_dmc1_x2_div, cclk_gpu_div,
	fclk_dss_div, aclk_dss_div, hclk_dss_div, abus_core_div, dbus_core_div, gbus_core_div,
	pbus_core_div, pclk_peri_div, ics_aclk_core_div, mmc0_core_div, sdio0_core_div,

	woden_clk_cnt
};

char *woden_clk_names[] = {
	"osc_clk", "dmc_pll_clk", "gpu_pll_clk", "dss_pll_clk", "core_pll_clk",

	"dmc0_pll_sel_clk", "dmc1_pll_sel_clk", "sms_sms_dmc_src_clk", "cclk_gpu_pll_sel_clk",
	"fclk_dss_pll_sel_clk", "aclk_dss_pll_sel_clk", "hclk_dss_pll_sel_clk",
	"abus_core_pll_sel_clk", "dbus_core_pll_sel_clk", "gbus_core_pll_sel_clk",
	"pbus_core_pll_sel_clk", "ics_aclk_core_pll_sel_clk", "mmc0_core_pll_sel_clk",
	"sdio0_core_pll_sel_clk",

	"dmc0_alt_sel_clk", "dmc1_alt_sel_clk", "cclk_gpu_alt_sel_clk", "fclk_dss_alt_sel_clk",
	"aclk_dss_alt_sel_clk", "hclk_dss_alt_sel_clk", "abus_core_alt_sel_clk",
	"dbus_core_alt_sel_clk", "gbus_core_alt_sel_clk", "pbus_core_alt_sel_clk",
	"ics_aclk_core_alt_sel_clk", "mmc0_core_alt_sel_clk", "sdio0_core_alt_sel_clk",

	"dmc0_clksrc_sel_clk", "dmc1_clksrc_sel_clk", "cclk_gpu_clksrc_sel_clk",
	"fclk_dss_clksrc_sel_clk", "aclk_dss_clksrc_sel_clk", "hclk_dss_clksrc_sel_clk",
	"abus_core_clksrc_sel_clk", "dbus_core_clksrc_sel_clk", "gbus_core_clksrc_sel_clk",
	"pbus_core_clksrc_sel_clk", "ics_aclk_core_clksrc_sel_clk", "mmc0_core_clksrc_sel_clk",
	"sdio0_core_clksrc_sel_clk",

	"dmc0_clksrc_mask", "dmc1_clksrc_mask", "cclk_gpu_clksrc_mask", "fclk_dss_clksrc_mask",
	"aclk_dss_clksrc_mask", "hclk_dss_clksrc_mask", "abus_core_clksrc_mask",
	"dbus_core_clksrc_mask", "gbus_core_clksrc_mask", "pbus_core_clksrc_mask",
	"ics_aclk_core_clksrc_mask", "mmc0_core_clksrc_mask", "sdio0_core_clksrc_mask",

	"mclk_dmc0_x0_div", "mclk_dmc0_x2_div", "mclk_dmc1_x0_div", "mclk_dmc1_x2_div",
	"cclk_gpu_div", "fclk_dss_div", "aclk_dss_div", "hclk_dss_div", "abus_core_div",
	"dbus_core_div", "gbus_core_div", "pbus_core_div", "pclk_peri_div", "ics_aclk_core_div",
	"mmc0_core_div", "sdio0_core_div",

	"woden_clk_cnt"
};

enum woden_cpu_clocks {
	cpu_pll_clk, ca15_cpu_pll_sel_clk, ca7_cpu_pll_sel_clk, ca15_cpu_alt_sel_clk,
	ca7_cpu_alt_sel_clk, ca15_cpu_clksrc_sel_clk, ca7_cpu_clksrc_sel_clk, ca15_cpu_clksrc_mask,
	ca7_cpu_clksrc_mask, ca15_armclk_div, ca15_pclkdbg_div, ca15_aclkenm_div, ca15_aclkens_div,
	ca15_periphclken_div, ca15_atclken_div, ca15_pclkendbg_div, ca7_armclk_div, ca7_cci_400_div,
	ca7_gic_400_div, ca7_aclkenm_div, ca7_pclkendbg_div, ca7_atclken_div, trace_at_sel_clk,
	trace_alt_sel_clk, cpucnt_clksrc_sel_clk, trace_clksrc_sel_clk, cpucnt_div, trace_div,
	woden_cpu_clk_cnt
};

char *woden_cpu_clk_names[] = {
	"cpu_pll_clk", "ca15_cpu_pll_sel_clk", "ca7_cpu_pll_sel_clk", "ca15_cpu_alt_sel_clk",
	"ca7_cpu_alt_sel_clk", "ca15_cpu_clksrc_sel_clk", "ca7_cpu_clksrc_sel_clk",
	"ca15_cpu_clksrc_mask", "ca7_cpu_clksrc_mask", "ca15_armclk_div", "ca15_aclkenm_div",
	"ca15_aclkens_div", "ca15_periphclken_div", "ca15_atclken_div", "ca15_pclkdbg_div",
	"ca15_pclkendbg_div", "ca7_armclk_div", "ca7_cci_400_div", "ca7_gic_400_div",
	"ca7_aclkenm_div", "ca7_pclkendbg_div","ca7_atclken_div", "trace_at_sel_clk",
	"trace_alt_sel_clk", "cpucnt_clksrc_sel_clk", "trace_clksrc_sel_clk", "cpucnt_div",
	"trace_div",
	"woden_cpu_clk_cnt"
};

enum woden_aud_clocks {
	aud_pll_clk, dsp_cclk_sel_clk, aud_apclk_sel0_clk, dsp_alt_sel_clk,
	apclk_saltclk_sel_clk, dsp_clksrc_sel_clk, aud_apclk_sel1_clk, dsp_cclksrc_mask,
	div_cclk_dsp, div_aclk_aud, div_pclk_aud, div_pclk_async,
	i2s0_aud_pll_sel_clk, i2s0_aud_alt_sel_clk, i2s0_aud_clksrc_sel_clk, i2s0_aud_div,
	i2s1_aud_pll_sel_clk, i2s1_aud_alt_sel_clk, i2s1_aud_clksrc_sel_clk, i2s1_aud_div,
	i2s2_aud_pll_sel_clk, i2s2_aud_alt_sel_clk, i2s2_aud_clksrc_sel_clk, i2s2_aud_div,
	i2s3_aud_pll_sel_clk, i2s3_aud_alt_sel_clk, i2s3_aud_clksrc_sel_clk, i2s3_aud_div,
	i2s4_aud_pll_sel_clk, i2s4_aud_alt_sel_clk, i2s4_aud_clksrc_sel_clk, i2s4_aud_div,
	i2s5_aud_pll_sel_clk, i2s5_aud_alt_sel_clk, i2s5_aud_clksrc_sel_clk, i2s5_aud_div,
	woden_aud_clk_cnt
};

char *woden_aud_clk_names[] = {
	"aud_pll_clk", "dsp_cclk_sel_clk", "aud_apclk_sel0_clk", "dsp_alt_sel_clk",
	"apclk_saltclk_sel_clk", "dsp_clksrc_sel_clk", "aud_apclk_sel1_clk",
	"dsp_cclksrc_mask", "div_cclk_dsp", "div_aclk_aud", "div_pclk_aud", "div_pclk_async",
	"i2s0_aud_pll_sel_clk", "i2s0_aud_alt_sel_clk", "i2s0_aud_clksrc_sel_clk", "i2s0_aud_div",
	"i2s1_aud_pll_sel_clk", "i2s1_aud_alt_sel_clk", "i2s1_aud_clksrc_sel_clk", "i2s1_aud_div",
	"i2s2_aud_pll_sel_clk", "i2s2_aud_alt_sel_clk", "i2s2_aud_clksrc_sel_clk", "i2s2_aud_div",
	"i2s3_aud_pll_sel_clk", "i2s3_aud_alt_sel_clk", "i2s3_aud_clksrc_sel_clk", "i2s3_aud_div",
	"i2s4_aud_pll_sel_clk", "i2s4_aud_alt_sel_clk", "i2s4_aud_clksrc_sel_clk", "i2s4_aud_div",
	"i2s5_aud_pll_sel_clk", "i2s5_aud_alt_sel_clk", "i2s5_aud_clksrc_sel_clk", "i2s5_aud_div",

	"woden_aud_clk_cnt"
};

static struct clk *clk[woden_clk_cnt];
static struct clk *cpu_clk[woden_cpu_clk_cnt];
static struct clk *aud_clk[woden_aud_clk_cnt];

void woden_register_clk(void __iomem *base, u32 base_reg){

	clk[osc_clk] = clk_register_fixed_rate(NULL, "osc_clk", NULL, CLK_IS_ROOT, 24000000);

	clk[dmc_pll_clk] = odin_clk_register_ljpll(NULL, "dmc_pll_clk", "osc_clk", 0, base+0x70,
							base+0x70, base+0x78, base_reg+0x70, base_reg+0x70,
							base_reg+0x78, 1, 0, MACH_WODEN, COMM_PLL_CLK, NON_SECURE,
							&clk_out_lock);
	clk[gpu_pll_clk] = odin_clk_register_ljpll(NULL, "gpu_pll_clk", "osc_clk", 0, base+0x200,
							base+0x200, base+0x208, base_reg+0x200, base_reg+0x200,
							base_reg+0x208, 1, 0, MACH_WODEN, COMM_PLL_CLK, NON_SECURE,
							&clk_out_lock);
	clk[dss_pll_clk] = odin_clk_register_ljpll(NULL, "dss_pll_clk", "osc_clk", 0, base+0x190,
							base+0x190, base+0x198, base_reg+0x190, base_reg+0x190,
							base_reg+0x198, 1, 0, MACH_WODEN, COMM_PLL_CLK, NON_SECURE,
							&clk_out_lock);
	clk[core_pll_clk] = odin_clk_register_ljpll(NULL, "core_pll_clk", "osc_clk", 0, base+0x120,
							base+0x120, base+0x128, base_reg+0x120, base_reg+0x120,
							base_reg+0x128, 1, 0, MACH_WODEN, COMM_PLL_CLK, NON_SECURE,
							&clk_out_lock);

	clk[dmc0_pll_sel_clk] = odin_clk_register_mux(NULL, "dmc0_pll_sel_clk", dmc0_pll_sels_name,
							ARRAY_SIZE(dmc0_pll_sels_name), CLK_SET_RATE_PARENT, base+0x80,
							base_reg+0x80, 0, 1, 0, NON_SECURE, &clk_out_lock);
	clk[dmc1_pll_sel_clk] = odin_clk_register_mux(NULL, "dmc1_pll_sel_clk", dmc1_pll_sels_name,
							ARRAY_SIZE(dmc1_pll_sels_name), CLK_SET_RATE_PARENT, base+0xA0,
							base_reg+0xA0, 0, 1, 0, NON_SECURE, &clk_out_lock);
	clk[sms_sms_dmc_src_clk] = odin_clk_register_mux(NULL, "sms_sms_dmc_src_clk",
							sms_dmc_src_clk_sels_name, ARRAY_SIZE(sms_dmc_src_clk_sels_name),
							CLK_SET_RATE_PARENT, base+0xB0, base_reg+0xB0, 0, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[cclk_gpu_pll_sel_clk] = odin_clk_register_mux(NULL, "cclk_gpu_pll_sel_clk",
							cclk_gpu_pll_sels_name, ARRAY_SIZE(cclk_gpu_pll_sels_name),
							CLK_SET_RATE_PARENT, base+0x210, base_reg+0x210, 0, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[fclk_dss_pll_sel_clk] = odin_clk_register_mux(NULL, "fclk_dss_pll_sel_clk",
							fclk_dss_pll_sels_name, ARRAY_SIZE(fclk_dss_pll_sels_name),
							CLK_SET_RATE_PARENT, base+0x1A0, base_reg+0x1A0, 0, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[aclk_dss_pll_sel_clk] = odin_clk_register_mux(NULL, "aclk_dss_pll_sel_clk",
							aclk_dss_pll_sels_name, ARRAY_SIZE(aclk_dss_pll_sels_name),
							CLK_SET_RATE_PARENT, base+0x1B0, base_reg+0x1B0, 0, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[hclk_dss_pll_sel_clk] = odin_clk_register_mux(NULL, "hclk_dss_pll_sel_clk",
							hclk_dss_pll_sels_name, ARRAY_SIZE(hclk_dss_pll_sels_name),
							CLK_SET_RATE_PARENT, base+0x1C0, base_reg+0x1C0, 0, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[abus_core_pll_sel_clk] = odin_clk_register_mux(NULL, "abus_core_pll_sel_clk",
							abus_core_pll_sels_name, ARRAY_SIZE(abus_core_pll_sels_name),
							CLK_SET_RATE_PARENT, base+0x130, base_reg+0x130, 0, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[dbus_core_pll_sel_clk] = odin_clk_register_mux(NULL, "dbus_core_pll_sel_clk",
							dbus_core_pll_sels_name, ARRAY_SIZE(dbus_core_pll_sels_name),
							CLK_SET_RATE_PARENT, base+0x150, base_reg+0x150, 0, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[gbus_core_pll_sel_clk] = odin_clk_register_mux(NULL, "gbus_core_pll_sel_clk",
							gbus_core_pll_sels_name, ARRAY_SIZE(gbus_core_pll_sels_name),
							CLK_SET_RATE_PARENT, base+0x140, base_reg+0x140, 0, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[pbus_core_pll_sel_clk] = odin_clk_register_mux(NULL, "pbus_core_pll_sel_clk",
							pbus_core_pll_sels_name, ARRAY_SIZE(pbus_core_pll_sels_name),
							CLK_SET_RATE_PARENT, base+0x160, base_reg+0x160, 0, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[ics_aclk_core_pll_sel_clk] = odin_clk_register_mux(NULL, "ics_aclk_core_pll_sel_clk",
							ics_aclk_core_pll_sels_name, ARRAY_SIZE(ics_aclk_core_pll_sels_name),
							CLK_SET_RATE_PARENT, base+0x250, base_reg+0x250, 0, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[mmc0_core_pll_sel_clk] = odin_clk_register_mux(NULL, "mmc0_core_pll_sel_clk",
							mmc0_core_pll_sels_name, ARRAY_SIZE(mmc0_core_pll_sels_name),
							CLK_SET_RATE_PARENT, base+0x480, base_reg+0x480, 0, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[sdio0_core_pll_sel_clk] = odin_clk_register_mux(NULL, "sdio0_core_pll_sel_clk",
							sdio0_core_pll_sels_name, ARRAY_SIZE(sdio0_core_pll_sels_name),
							CLK_SET_RATE_PARENT, base+0x4A0, base_reg+0x4A0, 0, 1, 0,
							NON_SECURE, &clk_out_lock);


	clk[dmc0_alt_sel_clk] = odin_clk_register_mux(NULL, "dmc0_alt_sel_clk", dmc0_alt_sels_name,
							ARRAY_SIZE(dmc0_alt_sels_name), CLK_SET_RATE_PARENT, base+0x80,
							base_reg+0x80, 8, 2, 0, NON_SECURE, &clk_out_lock);
	clk[dmc1_alt_sel_clk] = odin_clk_register_mux(NULL, "dmc1_alt_sel_clk", dmc1_alt_sels_name,
							ARRAY_SIZE(dmc1_alt_sels_name), CLK_SET_RATE_PARENT, base+0xA0,
							base_reg+0xA0, 8, 2, 0, NON_SECURE, &clk_out_lock);
	clk[cclk_gpu_alt_sel_clk] = odin_clk_register_mux(NULL, "cclk_gpu_alt_sel_clk",
							cclk_gpu_alt_sels_name, ARRAY_SIZE(cclk_gpu_alt_sels_name),
							CLK_SET_RATE_PARENT, base+0x210, base_reg+210, 8, 2, 0,
							NON_SECURE, &clk_out_lock);
	clk[fclk_dss_alt_sel_clk] = odin_clk_register_mux(NULL, "fclk_dss_alt_sel_clk",
							fclk_dss_alt_sels_name, ARRAY_SIZE(fclk_dss_alt_sels_name),
							CLK_SET_RATE_PARENT, base+0x1A0, base_reg+0x1A0, 8, 2, 0,
							NON_SECURE, &clk_out_lock);
	clk[aclk_dss_alt_sel_clk] = odin_clk_register_mux(NULL, "aclk_dss_alt_sel_clk",
							aclk_dss_alt_sels_name, ARRAY_SIZE(aclk_dss_alt_sels_name),
							CLK_SET_RATE_PARENT, base+0x1B0, base_reg+0x1B0, 8, 2, 0,
							NON_SECURE,  &clk_out_lock);
	clk[hclk_dss_alt_sel_clk] = odin_clk_register_mux(NULL, "hclk_dss_alt_sel_clk",
							hclk_dss_alt_sels_name,	ARRAY_SIZE(hclk_dss_alt_sels_name),
							CLK_SET_RATE_PARENT, base+0x1C0, base_reg+0x1C0, 8, 2, 0,
							NON_SECURE, &clk_out_lock);
	clk[abus_core_alt_sel_clk] = odin_clk_register_mux(NULL, "abus_core_alt_sel_clk",
							abus_core_alt_sels_name, ARRAY_SIZE(abus_core_alt_sels_name),
							CLK_SET_RATE_PARENT, base+0x130, base_reg+0x130, 8, 2, 0,
							NON_SECURE, &clk_out_lock);
	clk[dbus_core_alt_sel_clk] = odin_clk_register_mux(NULL, "dbus_core_alt_sel_clk",
							dbus_core_alt_sels_name, ARRAY_SIZE(dbus_core_alt_sels_name),
							CLK_SET_RATE_PARENT, base+0x150, base_reg+0x150, 8, 2, 0,
							NON_SECURE, &clk_out_lock);
	clk[gbus_core_alt_sel_clk] = odin_clk_register_mux(NULL, "gbus_core_alt_sel_clk",
							gbus_core_alt_sels_name, ARRAY_SIZE(gbus_core_alt_sels_name),
							CLK_SET_RATE_PARENT, base+0x140, base_reg+0x140, 8, 2, 0,
							NON_SECURE, &clk_out_lock);
	clk[pbus_core_alt_sel_clk] = odin_clk_register_mux(NULL, "pbus_core_alt_sel_clk",
							pbus_core_alt_sels_name, ARRAY_SIZE(pbus_core_alt_sels_name),
							CLK_SET_RATE_PARENT, base+0x160, base_reg+0x160, 8, 2, 0,
							NON_SECURE, &clk_out_lock);
	clk[ics_aclk_core_alt_sel_clk] = odin_clk_register_mux(NULL, "ics_aclk_core_alt_sel_clk",
							ics_aclk_core_alt_sels_name, ARRAY_SIZE(ics_aclk_core_alt_sels_name),
							CLK_SET_RATE_PARENT, base+0x250, base_reg+0x250, 8, 2, 0,
							NON_SECURE, &clk_out_lock);
	clk[mmc0_core_alt_sel_clk] = odin_clk_register_mux(NULL, "mmc0_core_alt_sel_clk",
							mmc0_core_alt_sels_name, ARRAY_SIZE(mmc0_core_alt_sels_name),
							CLK_SET_RATE_PARENT, base+0x480, base_reg+0x480, 8, 2, 0,
							NON_SECURE, &clk_out_lock);
	clk[sdio0_core_alt_sel_clk] = odin_clk_register_mux(NULL, "sdio0_core_alt_sel_clk",
							sdio0_core_alt_sels_name, ARRAY_SIZE(sdio0_core_alt_sels_name),
							CLK_SET_RATE_PARENT, base+0x4A0, base_reg+0x4A0, 8, 2, 0,
							NON_SECURE, &clk_out_lock);


	clk[dmc0_clksrc_sel_clk] = odin_clk_register_mux(NULL, "dmc0_clksrc_sel_clk",
							dmc0_clksrc_sels_name, ARRAY_SIZE(dmc0_clksrc_sels_name),
							CLK_SET_RATE_PARENT, base+0x80, base_reg+0x80, 4, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[dmc1_clksrc_sel_clk] = odin_clk_register_mux(NULL, "dmc1_clksrc_sel_clk",
							dmc1_clksrc_sels_name, ARRAY_SIZE(dmc1_clksrc_sels_name),
							CLK_SET_RATE_PARENT, base+0xA0, base_reg+0xA0, 4, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[cclk_gpu_clksrc_sel_clk] = odin_clk_register_mux(NULL, "cclk_gpu_clksrc_sel_clk",
							cclk_gpu_clksrc_sels_name, ARRAY_SIZE(cclk_gpu_clksrc_sels_name),
							CLK_SET_RATE_PARENT, base+0x210, base_reg+0x210, 4, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[fclk_dss_clksrc_sel_clk] = odin_clk_register_mux(NULL, "fclk_dss_clksrc_sel_clk",
							fclk_dss_clksrc_sels_name, ARRAY_SIZE(fclk_dss_clksrc_sels_name),
							CLK_SET_RATE_PARENT, base+0x1A0, base_reg+0x1A0, 4, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[aclk_dss_clksrc_sel_clk] = odin_clk_register_mux(NULL, "aclk_dss_clksrc_sel_clk",
							aclk_dss_clksrc_sels_name, ARRAY_SIZE(aclk_dss_clksrc_sels_name),
							CLK_SET_RATE_PARENT, base+0x1B0, base_reg+0x1B0, 4, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[hclk_dss_clksrc_sel_clk] = odin_clk_register_mux(NULL, "hclk_dss_clksrc_sel_clk",
							hclk_dss_clksrc_sels_name, ARRAY_SIZE(hclk_dss_clksrc_sels_name),
							CLK_SET_RATE_PARENT, base+0x1C0, base_reg+0x1C0, 4, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[abus_core_clksrc_sel_clk] = odin_clk_register_mux(NULL, "abus_core_clksrc_sel_clk",
							abus_core_clksrc_sels_name, ARRAY_SIZE(abus_core_clksrc_sels_name),
							CLK_SET_RATE_PARENT, base+0x130, base_reg+0x130, 4, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[dbus_core_clksrc_sel_clk] = odin_clk_register_mux(NULL, "dbus_core_clksrc_sel_clk",
							dbus_core_clksrc_sels_name, ARRAY_SIZE(dbus_core_clksrc_sels_name),
							CLK_SET_RATE_PARENT, base+0x150, base_reg+0x150, 4, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[gbus_core_clksrc_sel_clk] = odin_clk_register_mux(NULL, "gbus_core_clksrc_sel_clk",
							gbus_core_clksrc_sels_name, ARRAY_SIZE(gbus_core_clksrc_sels_name),
							CLK_SET_RATE_PARENT, base+0x140, base_reg+0x140, 4, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[pbus_core_clksrc_sel_clk] = odin_clk_register_mux(NULL, "pbus_core_clksrc_sel_clk",
							pbus_core_clksrc_sels_name, ARRAY_SIZE(pbus_core_clksrc_sels_name),
							CLK_SET_RATE_PARENT, base+0x160, base_reg+0x160, 4, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[ics_aclk_core_clksrc_sel_clk] = odin_clk_register_mux(NULL, "ics_aclk_core_clksrc_sel_clk",
							ics_aclk_core_clksrc_sels_name,
							ARRAY_SIZE(ics_aclk_core_clksrc_sels_name), CLK_SET_RATE_PARENT,
							base+0x250,  base_reg+0x250, 4, 1, 0, NON_SECURE, &clk_out_lock);
	clk[mmc0_core_clksrc_sel_clk] = odin_clk_register_mux(NULL, "mmc0_core_clksrc_sel_clk",
							mmc0_core_clksrc_sels_name, ARRAY_SIZE(mmc0_core_clksrc_sels_name),
							CLK_SET_RATE_PARENT, base+0x480, base_reg+0x480, 4, 1, 0,
							NON_SECURE, &clk_out_lock);
	clk[sdio0_core_clksrc_sel_clk] = odin_clk_register_mux(NULL, "sdio0_core_clksrc_sel_clk",
							sdio0_core_clksrc_sels_name, ARRAY_SIZE(sdio0_core_clksrc_sels_name),
							CLK_SET_RATE_PARENT, base+0x4A0, base_reg+0x4A0, 4, 1, 0,
							NON_SECURE, &clk_out_lock);


	clk[dmc0_clksrc_mask] = odin_clk_register_gate(NULL, "dmc0_clksrc_mask", "dmc0_clksrc_sel_clk",
							CLK_SET_RATE_PARENT, base+0x84, base_reg+0x84, 0, 0,
							NON_SECURE, &clk_out_lock);
	clk[dmc1_clksrc_mask] = odin_clk_register_gate(NULL, "dmc1_clksrc_mask", "dmc1_clksrc_sel_clk",
							CLK_SET_RATE_PARENT, base+0xA4, base_reg+0xA4, 0, 0,
							NON_SECURE, &clk_out_lock);
	clk[cclk_gpu_clksrc_mask] = odin_clk_register_gate(NULL, "cclk_gpu_clksrc_mask",
							"cclk_gpu_clksrc_sel_clk", CLK_SET_RATE_PARENT, base+0x214,
							base_reg+0x214, 0, 0, NON_SECURE, &clk_out_lock);
	clk[fclk_dss_clksrc_mask] = odin_clk_register_gate(NULL, "fclk_dss_clksrc_mask",
							"fclk_dss_clksrc_sel_clk", CLK_SET_RATE_PARENT, base+0x1A4,
							base_reg+0x1A4, 0, 0, NON_SECURE, &clk_out_lock);
	clk[aclk_dss_clksrc_mask] = odin_clk_register_gate(NULL, "aclk_dss_clksrc_mask",
							"aclk_dss_clksrc_sel_clk", CLK_SET_RATE_PARENT, base+0x1B4,
							base_reg+0x1B4, 0, 0, NON_SECURE, &clk_out_lock);
	clk[hclk_dss_clksrc_mask] = odin_clk_register_gate(NULL, "hclk_dss_clksrc_mask",
							"hclk_dss_clksrc_sel_clk", CLK_SET_RATE_PARENT, base+0x1C4,
							base_reg+0x1C4, 0, 0, NON_SECURE, &clk_out_lock);
	clk[abus_core_clksrc_mask] = odin_clk_register_gate(NULL, "abus_core_clksrc_mask",
							"abus_core_clksrc_sel_clk", CLK_SET_RATE_PARENT, base+0x134,
							base_reg+0x134, 0, 0, NON_SECURE, &clk_out_lock);
	clk[dbus_core_clksrc_mask] = odin_clk_register_gate(NULL, "dbus_core_clksrc_mask",
							"dbus_core_clksrc_sel_clk", CLK_SET_RATE_PARENT, base+0x154,
							base_reg+0x154, 0, 0, NON_SECURE, &clk_out_lock);
	clk[gbus_core_clksrc_mask] = odin_clk_register_gate(NULL, "gbus_core_clksrc_mask",
							"gbus_core_clksrc_sel_clk", CLK_SET_RATE_PARENT, base+0x144,
							base_reg+0x144, 0, 0, NON_SECURE, &clk_out_lock);
	clk[pbus_core_clksrc_mask] = odin_clk_register_gate(NULL, "pbus_core_clksrc_mask",
							"pbus_core_clksrc_sel_clk", CLK_SET_RATE_PARENT, base+0x164,
							base_reg+0x164, 0, 0, NON_SECURE, &clk_out_lock);
	clk[ics_aclk_core_clksrc_mask] = odin_clk_register_gate(NULL, "ics_aclk_core_clksrc_mask",
							"ics_aclk_core_clksrc_sel_clk", CLK_SET_RATE_PARENT, base+0x254,
							base_reg+0x254, 0, 0, NON_SECURE, &clk_out_lock);
	clk[mmc0_core_clksrc_mask] = odin_clk_register_gate(NULL, "mmc0_core_clksrc_mask",
							"mmc0_core_clksrc_sel_clk", CLK_SET_RATE_PARENT, base+0x484,
							base_reg+0x484, 0, 0, NON_SECURE, &clk_out_lock);
	clk[sdio0_core_clksrc_mask] = odin_clk_register_gate(NULL, "sdio0_core_clksrc_mask",
							"sdio0_core_clksrc_sel_clk", CLK_SET_RATE_PARENT, base+0x4A4,
							base_reg+0x4A4, 0, 0, NON_SECURE, &clk_out_lock);


	clk[mclk_dmc0_x0_div] = odin_clk_register_div(NULL, "mclk_dmc0_x0_div", "dmc0_clksrc_mask", 0,
							base+0x88, NULL, base_reg+0x88, 0x0, 0, 8, 0, MACH_WODEN,
							COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
	clk[mclk_dmc0_x2_div] = odin_clk_register_div(NULL, "mclk_dmc0_x2_div", "dmc0_clksrc_mask", 0,
							base+0x88, NULL, base_reg+0x88, 0x0, 8, 8, 0, MACH_WODEN,
							COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
	clk[mclk_dmc1_x0_div] = odin_clk_register_div(NULL, "mclk_dmc1_x0_div", "dmc1_clksrc_mask", 0,
							base+0xA8, NULL, base_reg+0xA8, 0x0, 0, 8, 0, MACH_WODEN,
							COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
	clk[mclk_dmc1_x2_div] = odin_clk_register_div(NULL, "mclk_dmc1_x2_div", "dmc1_clksrc_mask", 0,
							base+0xA8, NULL, base_reg+0xA8, 0x0, 8, 8, 0, MACH_WODEN,
							COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
	clk[cclk_gpu_div] = odin_clk_register_div(NULL, "cclk_gpu_div", "cclk_gpu_clksrc_mask",
							CLK_SET_RATE_PARENT, base+0x218, NULL, base_reg+0x218, 0x0,
							0, 8, 0, MACH_WODEN, COMM_PLL_CLK, 0,NON_SECURE, &clk_out_lock);
	clk[fclk_dss_div] = odin_clk_register_div(NULL, "fclk_dss_div", "fclk_dss_clksrc_mask", 0,
							base+0x1A8, NULL, base_reg+0x1A8, 0x0, 0, 8, 0, MACH_WODEN,
							COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
	clk[aclk_dss_div] = odin_clk_register_div(NULL, "aclk_dss_div", "aclk_dss_clksrc_mask", 0,
							base+0x1B8, NULL, base_reg+0x1B8, 0x0, 0, 8, 0, MACH_WODEN,
							COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
	clk[hclk_dss_div] = odin_clk_register_div(NULL, "hclk_dss_div", "hclk_dss_clksrc_mask", 0,
							base+0x1C8, NULL, base_reg+0x1C8, 0x0, 0, 8, 0, MACH_WODEN,
							COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
	clk[abus_core_div] = odin_clk_register_div(NULL, "abus_core_div", "abus_core_clksrc_mask", 0,
							base+0x138, NULL, base_reg+0x138, 0x0, 0, 8, 0, MACH_WODEN,
							COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
	clk[dbus_core_div] = odin_clk_register_div(NULL, "dbus_core_div", "dbus_core_clksrc_mask", 0,
							base+0x158, NULL, base_reg+0x158, 0x0, 0, 8, 0, MACH_WODEN,
							COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
	clk[gbus_core_div] = odin_clk_register_div(NULL, "gbus_core_div", "gbus_core_clksrc_mask", 0,
							base+0x148, NULL, base_reg+0x148, 0x0, 0, 8, 0, MACH_WODEN,
							COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
	clk[pbus_core_div] = odin_clk_register_div(NULL, "pbus_core_div", "pbus_core_clksrc_mask", 0,
							base+0x168, NULL, base_reg+0x168, 0x0, 0, 8, 0,  MACH_WODEN,
							COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
	clk[pclk_peri_div] = odin_clk_register_div(NULL, "pclk_peri_div", "pbus_core_clksrc_mask", 0,
							base+0x168, NULL, base_reg+0x168, 0x0, 16, 8, 0, MACH_WODEN,
							COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
	clk[ics_aclk_core_div] = odin_clk_register_div(NULL, "ics_aclk_core_div",
							"ics_aclk_core_clksrc_mask", 0, base+0x258, NULL, base_reg+0x258, 0x0,
							0, 8, 0, MACH_WODEN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
	clk[mmc0_core_div] = odin_clk_register_div(NULL, "mmc0_core_div", "mmc0_core_clksrc_mask", 0,
							base+0x488, NULL, base_reg+0x488, 0x0, 0, 8, 0, MACH_WODEN,
							COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
	clk[sdio0_core_div] = odin_clk_register_div(NULL, "sdio0_core_div", "sdio0_core_clksrc_mask", 0,
							base+0x4A8, NULL, base_reg+0x4A8, 0x0, 0, 8, 0, MACH_WODEN,
							COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

}

void woden_register_cpu_clk(void __iomem *cpu_base, u32 base_reg){

	cpu_clk[cpu_pll_clk] = odin_clk_register_ljpll(NULL, "cpu_pll_clk", "osc_clk", 0,
							cpu_base+0x118, cpu_base+0x11C, NULL, base_reg+0x118, base_reg+0x11C,
							0x0, 17, 19, MACH_WODEN, CPU_PLL_CLK, NON_SECURE, &clk_out_lock);

	cpu_clk[ca15_cpu_pll_sel_clk] = odin_clk_register_mux(NULL, "ca15_cpu_pll_sel_clk",
							ca15_cpu_pll_sels_name, ARRAY_SIZE(ca15_cpu_pll_sels_name),
							CLK_SET_RATE_PARENT, cpu_base+0x100, base_reg+0x100, 0, 1, 0,
							NON_SECURE, &clk_out_lock);
	cpu_clk[ca7_cpu_pll_sel_clk] = odin_clk_register_mux(NULL, "ca7_cpu_pll_sel_clk",
							ca7_cpu_pll_sels_name, ARRAY_SIZE(ca7_cpu_pll_sels_name),
							CLK_SET_RATE_PARENT, cpu_base+0x110, base_reg+0x110, 16, 1, 0,
							NON_SECURE, &clk_out_lock);

	cpu_clk[ca15_cpu_alt_sel_clk] = odin_clk_register_mux(NULL, "ca15_cpu_alt_sel_clk",
							ca15_cpu_alt_sels_name, ARRAY_SIZE(ca15_cpu_alt_sels_name),
							CLK_SET_RATE_PARENT, cpu_base+0x100, base_reg+0x100, 1, 2, 0,
							NON_SECURE, &clk_out_lock);
	cpu_clk[ca7_cpu_alt_sel_clk] = odin_clk_register_mux(NULL, "ca7_cpu_alt_sel_clk",
							ca7_cpu_alt_sels_name, ARRAY_SIZE(ca7_cpu_alt_sels_name),
							CLK_SET_RATE_PARENT, cpu_base+0x110, base_reg+0x110, 17, 2, 0,
							NON_SECURE, &clk_out_lock);

	cpu_clk[ca15_cpu_clksrc_sel_clk] = odin_clk_register_mux(NULL, "ca15_cpu_clksrc_sel_clk",
							ca15_cpu_clksrc_sels_name, ARRAY_SIZE(ca15_cpu_clksrc_sels_name),
							CLK_SET_RATE_PARENT, cpu_base+0x100, base_reg+0x100, 3, 1, 0,
							NON_SECURE, &clk_out_lock);
	cpu_clk[ca7_cpu_clksrc_sel_clk] = odin_clk_register_mux(NULL, "ca7_cpu_clksrc_sel_clk",
							ca7_cpu_clksrc_sels_name, ARRAY_SIZE(ca7_cpu_clksrc_sels_name),
							CLK_SET_RATE_PARENT, cpu_base+0x110, base_reg+0x110, 19, 1, 0,
							NON_SECURE, &clk_out_lock);

	cpu_clk[ca15_cpu_clksrc_mask] = odin_clk_register_gate(NULL, "ca15_cpu_clksrc_mask",
							"ca15_cpu_clksrc_sel_clk", CLK_SET_RATE_PARENT, cpu_base+0x100,
							base_reg+0x100, 4, 0, NON_SECURE, &clk_out_lock);
	cpu_clk[ca7_cpu_clksrc_mask] = odin_clk_register_gate(NULL, "ca7_cpu_clksrc_mask",
							"ca7_cpu_clksrc_sel_clk", CLK_SET_RATE_PARENT, cpu_base+0x110,
							base_reg+0x110, 20, 0, NON_SECURE, &clk_out_lock);

	cpu_clk[ca15_armclk_div] = odin_clk_register_div(NULL, "ca15_armclk_div",
							"ca15_cpu_clksrc_mask", CLK_SET_RATE_PARENT, cpu_base+0x104,
							cpu_base+0x100, base_reg+0x104, base_reg+0x100, 0, 8, 12,
							MACH_WODEN, CPU_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
/*
	cpu_clk[ca15_aclkenm_div] = odin_clk_register_div(NULL, "ca15_aclkenm_div",
							"ca15_armclk_div", CLK_SET_RATE_PARENT, cpu_base+0x104, 8, 8, 0,
							&clk_out_lock);
	cpu_clk[ca15_aclkens_div] = odin_clk_register_div(NULL, "ca15_aclkens_div",
							"ca15_armclk_div", CLK_SET_RATE_PARENT, cpu_base+0x104, 16, 8, 0,
							&clk_out_lock);
	cpu_clk[ca15_periphclken_div] = odin_clk_register_div(NULL, "ca15_periphclken_div",
							"ca15_armclk_div", CLK_SET_RATE_PARENT, cpu_base+0x104, 24, 8, 0,
							&clk_out_lock);
	cpu_clk[ca15_atclken_div] = odin_clk_register_div(NULL, "ca15_atclken_div",
							"ca15_armclk_div", CLK_SET_RATE_PARENT, cpu_base+0x108, 0, 8, 0,
							&clk_out_lock);
	cpu_clk[ca15_pclkdbg_div] = odin_clk_register_div(NULL, "ca15_pclkdbg_div",
							"ca15_cpu_clksrc_sel_clk", CLK_SET_RATE_PARENT, cpu_base+0x108, 8, 8, 0,
							&clk_out_lock);
	cpu_clk[ca15_pclkendbg_div] = odin_clk_register_div(NULL, "ca15_pclkendbg_div",
							"ca15_pclkdbg_div", CLK_SET_RATE_PARENT, cpu_base+0x108, 16, 8, 0,
							&clk_out_lock);
*/
	cpu_clk[ca7_armclk_div] = odin_clk_register_div(NULL, "ca7_armclk_div",
							"ca7_cpu_clksrc_mask", CLK_SET_RATE_PARENT, cpu_base+0x10C, NULL,
							base_reg+0x10C, 0x0, 0, 8, 0, MACH_WODEN, CPU_PLL_CLK, 0, NON_SECURE,
							&clk_out_lock);
	cpu_clk[ca7_cci_400_div] = odin_clk_register_div(NULL, "ca7_cci_400_div",
							"ca7_cpu_clksrc_sel_clk", CLK_SET_RATE_PARENT, cpu_base+0x114, NULL,
							base_reg+0x114, 0x0, 16, 3, 0, MACH_WODEN, CPU_PLL_CLK, 0, NON_SECURE,
							&clk_out_lock);
	cpu_clk[ca7_gic_400_div] = odin_clk_register_div(NULL, "ca7_gic_400_div",
							"ca7_cpu_clksrc_sel_clk", CLK_SET_RATE_PARENT, cpu_base+0x114, NULL,
							base_reg+0x114, 0x0, 8, 3, 0, MACH_WODEN, CPU_PLL_CLK, 0, NON_SECURE,
							&clk_out_lock);
/*
	cpu_clk[ca7_aclkenm_div] = odin_clk_register_div(NULL, "ca7_aclkenm_div",
							"ca7_armclk_div", CLK_SET_RATE_PARENT, cpu_base+0x10C, 8, 8, 0,
							&clk_out_lock);
	cpu_clk[ca7_pclkendbg_div] = odin_clk_register_div(NULL, "ca7_pclkendbg_div",
							"ca7_armclk_div", CLK_SET_RATE_PARENT, cpu_base+0x110, 8, 8, 0,
							&clk_out_lock);
	cpu_clk[ca7_atclken_div] = odin_clk_register_div(NULL, "ca7_atclken_div",
							"ca7_armclk_div", CLK_SET_RATE_PARENT, cpu_base+0x110, 0, 8, 0,
							&clk_out_lock);
*/

	/*Create CNT_TRACE CLOCK*/
	cpu_clk[trace_at_sel_clk] = odin_clk_register_mux(NULL, "trace_at_sel_clk",
							trace_at_sels_name, ARRAY_SIZE(trace_at_sels_name),
							CLK_SET_RATE_PARENT, cpu_base+0x24, base_reg+0x24, 0, 1, 0,
							NON_SECURE, &clk_out_lock);

	cpu_clk[trace_alt_sel_clk] = odin_clk_register_mux(NULL, "trace_alt_sel_clk",
							trace_alt_sels_name, ARRAY_SIZE(trace_alt_sels_name),
							CLK_SET_RATE_PARENT, cpu_base+0x24, base_reg+0x24, 3, 2, 0,
							NON_SECURE, &clk_out_lock);

	cpu_clk[cpucnt_clksrc_sel_clk] = odin_clk_register_mux(NULL, "cpucnt_clksrc_sel_clk",
							cpucnt_clksrc_sels_name, ARRAY_SIZE(cpucnt_clksrc_sels_name),
							CLK_SET_RATE_PARENT, cpu_base+0x114, base_reg+0x114, 4, 1, 0,
							NON_SECURE, &clk_out_lock);
	cpu_clk[trace_clksrc_sel_clk] = odin_clk_register_mux(NULL, "trace_clksrc_sel_clk",
							trace_clksrc_sels_name, ARRAY_SIZE(trace_clksrc_sels_name),
							CLK_SET_RATE_PARENT, cpu_base+0x24, base_reg+0x24, 1, 1, 0,
							NON_SECURE, &clk_out_lock);


	cpu_clk[cpucnt_div] = odin_clk_register_div(NULL, "cpucnt_div", "cpucnt_clksrc_sel_clk", 0,
							cpu_base+0x114, NULL, base_reg+0x114, 0x0, 0, 4, 0, MACH_WODEN,
							CPU_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
	cpu_clk[trace_div] = odin_clk_register_div(NULL, "trace_div", "trace_clksrc_sel_clk", 0,
							cpu_base+0x24, NULL, base_reg+0x24, 0x0, 4, 4, 0, MACH_WODEN,
							CPU_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

}

void woden_register_aud_clk(void __iomem *aud_base, u32 base_reg){

	clk[aud_pll_clk] = odin_clk_register_apll(NULL, "aud_pll_clk", "osc_clk", 0, aud_base,
							aud_base+0x8, base_reg, base_reg+0x8, 0, MACH_WODEN, AUD_PLL_CLK,
							NON_SECURE, &clk_out_lock);

	aud_clk[dsp_cclk_sel_clk] = odin_clk_register_mux(NULL, "dsp_cclk_sel_clk",
							dsp_cclk_sels_name, ARRAY_SIZE(dsp_cclk_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x10, base_reg+0x10, 0, 1, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[aud_apclk_sel0_clk] = odin_clk_register_mux(NULL, "aud_apclk_sel0_clk",
							aud_apclk_sels0_name, ARRAY_SIZE(aud_apclk_sels0_name),
							CLK_SET_RATE_PARENT, aud_base+0x30, base_reg+0x30,  0, 1, 0,
							NON_SECURE, &clk_out_lock);

	aud_clk[dsp_alt_sel_clk] = odin_clk_register_mux(NULL, "dsp_alt_sel_clk",
							dsp_alt_sels_name, ARRAY_SIZE(dsp_alt_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x10, base_reg+0x10,  8, 3, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[apclk_saltclk_sel_clk] = odin_clk_register_mux(NULL, "apclk_saltclk_sel_clk",
							apclk_saltclk_sels_name, ARRAY_SIZE(apclk_saltclk_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x30, base_reg+0x30,  8, 3, 0,
							NON_SECURE, &clk_out_lock);

	aud_clk[dsp_clksrc_sel_clk] = odin_clk_register_mux(NULL, "dsp_clksrc_sel_clk",
							dsp_clksrc_sels_name, ARRAY_SIZE(dsp_clksrc_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x10, base_reg+0x10,  4, 1, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[aud_apclk_sel1_clk] = odin_clk_register_mux(NULL, "aud_apclk_sel1_clk",
							aud_apclk_sels1_name, ARRAY_SIZE(aud_apclk_sels1_name),
							CLK_SET_RATE_PARENT, aud_base+0x30, base_reg+0x30,  4, 1, 0,
							NON_SECURE, &clk_out_lock);

	aud_clk[dsp_cclksrc_mask] = odin_clk_register_gate(NULL, "dsp_cclksrc_mask",
							"dsp_clksrc_sel_clk", CLK_SET_RATE_PARENT, aud_base+0x14,
							 base_reg+0x14, 0, 0, NON_SECURE, &clk_out_lock);

	aud_clk[div_cclk_dsp] = odin_clk_register_div(NULL, "div_cclk_dsp", "dsp_cclksrc_mask",
							0, aud_base+0x18, NULL, base_reg+0x18, 0x0, 0, 8, 0, MACH_WODEN,
							COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
	aud_clk[div_aclk_aud] = odin_clk_register_div(NULL, "div_aclk_aud", "div_cclk_dsp", 0,
							aud_base+0x20, NULL, base_reg+0x20, 0x0, 0, 4, 0, MACH_WODEN,
							COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
	aud_clk[div_pclk_aud] = odin_clk_register_div(NULL, "div_pclk_aud", "div_cclk_dsp", 0,
							aud_base+0x28, NULL, base_reg+0x28, 0x0, 0, 8, 0, MACH_WODEN,
							COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);
	aud_clk[div_pclk_async] = odin_clk_register_div(NULL, "div_pclk_async", "aud_apclk_sel1_clk",
							CLK_SET_RATE_PARENT, aud_base+0x34, NULL, base_reg+0x34, 0x0, 0, 8, 0,
							MACH_WODEN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	aud_clk[i2s0_aud_pll_sel_clk] = odin_clk_register_mux(NULL, "i2s0_aud_pll_sel_clk",
							i2s0_aud_pll_sels_name, ARRAY_SIZE(i2s0_aud_pll_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x3C, base_reg+0x3C, 0, 1, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[i2s0_aud_alt_sel_clk] = odin_clk_register_mux(NULL, "i2s0_aud_alt_sel_clk",
							i2s0_aud_alt_sels_name, ARRAY_SIZE(i2s0_aud_alt_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x3C, base_reg+0x3C, 8, 2, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[i2s0_aud_clksrc_sel_clk] = odin_clk_register_mux(NULL, "i2s0_aud_clksrc_sel_clk",
							i2s0_aud_clksrc_sels_name, ARRAY_SIZE(i2s0_aud_clksrc_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x3C, base_reg+0x3C, 4, 1, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[i2s0_aud_div] = odin_clk_register_div(NULL, "i2s0_aud_div", "i2s0_aud_clksrc_sel_clk",
							CLK_SET_RATE_PARENT, aud_base+0x40, NULL, base_reg+0x40, 0x0, 0, 4, 0,
							MACH_WODEN, AUD_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	aud_clk[i2s1_aud_pll_sel_clk] = odin_clk_register_mux(NULL, "i2s1_aud_pll_sel_clk",
							i2s1_aud_pll_sels_name, ARRAY_SIZE(i2s1_aud_pll_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x48, base_reg+0x48, 0, 1, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[i2s1_aud_alt_sel_clk] = odin_clk_register_mux(NULL, "i2s1_aud_alt_sel_clk",
							i2s1_aud_alt_sels_name, ARRAY_SIZE(i2s1_aud_alt_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x48, base_reg+0x48, 8, 2, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[i2s1_aud_clksrc_sel_clk] = odin_clk_register_mux(NULL, "i2s1_aud_clksrc_sel_clk",
							i2s1_aud_clksrc_sels_name, ARRAY_SIZE(i2s1_aud_clksrc_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x48, base_reg+0x48, 4, 1, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[i2s1_aud_div] = odin_clk_register_div(NULL, "i2s1_aud_div",
							"i2s1_aud_clksrc_sel_clk", CLK_SET_RATE_PARENT, aud_base+0x4C, NULL,
							base_reg+0x4C, 0x0, 0, 4, 0, MACH_WODEN, AUD_PLL_CLK, 0, NON_SECURE,
							&clk_out_lock);

	aud_clk[i2s2_aud_pll_sel_clk] = odin_clk_register_mux(NULL, "i2s2_aud_pll_sel_clk",
							i2s2_aud_pll_sels_name, ARRAY_SIZE(i2s2_aud_pll_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x54, base_reg+0x54,  0, 1, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[i2s2_aud_alt_sel_clk] = odin_clk_register_mux(NULL, "i2s2_aud_alt_sel_clk",
							i2s2_aud_alt_sels_name, ARRAY_SIZE(i2s2_aud_alt_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x54, base_reg+0x54,  8, 2, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[i2s2_aud_clksrc_sel_clk] = odin_clk_register_mux(NULL, "i2s2_aud_clksrc_sel_clk",
							i2s2_aud_clksrc_sels_name, ARRAY_SIZE(i2s2_aud_clksrc_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x54, base_reg+0x54,  4, 1, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[i2s2_aud_div] = odin_clk_register_div(NULL, "i2s2_aud_div",
							"i2s2_aud_clksrc_sel_clk", CLK_SET_RATE_PARENT, aud_base+0x58, NULL,
							base_reg+0x58, 0x0, 0, 4, 0, MACH_WODEN, AUD_PLL_CLK, 0,NON_SECURE,
							&clk_out_lock);

	aud_clk[i2s3_aud_pll_sel_clk] = odin_clk_register_mux(NULL, "i2s3_aud_pll_sel_clk",
							i2s3_aud_pll_sels_name, ARRAY_SIZE(i2s3_aud_pll_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x60, base_reg+0x60,  0, 1, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[i2s3_aud_alt_sel_clk] = odin_clk_register_mux(NULL, "i2s3_aud_alt_sel_clk",
							i2s3_aud_alt_sels_name, ARRAY_SIZE(i2s3_aud_alt_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x60, base_reg+0x60,  8, 2, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[i2s3_aud_clksrc_sel_clk] = odin_clk_register_mux(NULL, "i2s3_aud_clksrc_sel_clk",
							i2s3_aud_clksrc_sels_name, ARRAY_SIZE(i2s3_aud_clksrc_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x60, base_reg+0x60,  4, 1, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[i2s3_aud_div] = odin_clk_register_div(NULL, "i2s3_aud_div",
							"i2s3_aud_clksrc_sel_clk", CLK_SET_RATE_PARENT, aud_base+0x64, NULL,
							base_reg+0x64, 0x0, 0, 4, 0, MACH_WODEN, AUD_PLL_CLK, 0,NON_SECURE,
							&clk_out_lock);

	aud_clk[i2s4_aud_pll_sel_clk] = odin_clk_register_mux(NULL, "i2s4_aud_pll_sel_clk",
							i2s4_aud_pll_sels_name, ARRAY_SIZE(i2s4_aud_pll_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x6C, base_reg+0x6C,  0, 1, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[i2s4_aud_alt_sel_clk] = odin_clk_register_mux(NULL, "i2s4_aud_alt_sel_clk",
							i2s4_aud_alt_sels_name, ARRAY_SIZE(i2s4_aud_alt_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x6C, base_reg+0x6C,  8, 2, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[i2s4_aud_clksrc_sel_clk] = odin_clk_register_mux(NULL, "i2s4_aud_clksrc_sel_clk",
							i2s4_aud_clksrc_sels_name, ARRAY_SIZE(i2s4_aud_clksrc_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x6C, base_reg+0x6C,  4, 1, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[i2s4_aud_div] = odin_clk_register_div(NULL, "i2s4_aud_div",
							"i2s4_aud_clksrc_sel_clk", CLK_SET_RATE_PARENT, aud_base+0x70, NULL,
							base_reg+0x70, 0x0, 0, 4, 0, MACH_WODEN, AUD_PLL_CLK, 0,NON_SECURE,
							&clk_out_lock);

	aud_clk[i2s5_aud_pll_sel_clk] = odin_clk_register_mux(NULL, "i2s5_aud_pll_sel_clk",
							i2s5_aud_pll_sels_name, ARRAY_SIZE(i2s5_aud_pll_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x78, base_reg+0x78,  0, 1, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[i2s5_aud_alt_sel_clk] = odin_clk_register_mux(NULL, "i2s5_aud_alt_sel_clk",
							i2s5_aud_alt_sels_name, ARRAY_SIZE(i2s5_aud_alt_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x78, base_reg+0x78,  8, 2, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[i2s5_aud_clksrc_sel_clk] = odin_clk_register_mux(NULL, "i2s5_aud_clksrc_sel_clk",
							i2s5_aud_clksrc_sels_name, ARRAY_SIZE(i2s5_aud_clksrc_sels_name),
							CLK_SET_RATE_PARENT, aud_base+0x78, base_reg+0x78,  4, 1, 0,
							NON_SECURE, &clk_out_lock);
	aud_clk[i2s5_aud_div] = odin_clk_register_div(NULL, "i2s5_aud_div",
							"i2s5_aud_clksrc_sel_clk", CLK_SET_RATE_PARENT, aud_base+0x7C, NULL,
							base_reg+0x7C, 0x0, 0, 4, 0, MACH_WODEN, AUD_PLL_CLK, 0,NON_SECURE,
							&clk_out_lock);

}

#ifdef CONFIG_WODEN_SLEEP

#if CLK_DISABLE
//[disable_clk]====================================

void woden_dbus_clk_disable(void)
{
	clk_disable_unprepare(clk_get(NULL, "fclk_dss"));
	clk_disable_unprepare(clk_get(NULL, "aclk_dss"));
	clk_disable_unprepare(clk_get(NULL, "hclk_dss"));

	if((clk_get(NULL, "fclk_dss")->enable_count==0) \
	&& (clk_get(NULL, "clk_dss")->enable_count==0) \
	&& (clk_get(NULL, "hclk_dss")->enable_count==0)){
		clk_disable_unprepare(clk_get(NULL, "dbus_core"));
	}
	else
		printk(KERN_ERR "can't disable %s\n", clk_get(NULL, "dbus_core")->name);
}

void woden_abus_clk_disable(void)
{
	//clk_disable_unprepare(clk_get(NULL, "aclk_aud"));
	clk_disable_unprepare(clk_get(NULL, "pclk_aud"));

	clk_disable_unprepare(clk_get(NULL, "abus_core"));
/*
	if((clk_get(NULL, "aclk_aud")->enable_count==0) \
	&& (clk_get(NULL, "pclk_aud")->enable_count==0))
	{
		clk_disable_unprepare(clk_get(NULL, "abus_core"));
	}
	else
		printk(KERN_ERR "can't disable %s\n", clk_get(NULL, "abus_core")->name);
*/
}

void woden_pbus_clk_disable(void)
{
	clk_disable_unprepare(clk_get(NULL, "pclk_peri_sysconf"));
	clk_disable_unprepare(clk_get(NULL, "pclk_peri_dmc0"));
	clk_disable_unprepare(clk_get(NULL, "pclk_peri_dmc1"));
	clk_disable_unprepare(clk_get(NULL, "pclk_peri_cpu_pclk_sys"));

	clk_disable_unprepare(clk_get(NULL, "pclk_peri_sdma"));
	//clk_disable_unprepare(clk_get(NULL, "pclk_peri_peri_top_ius"));
	//clk_disable_unprepare(clk_get(NULL, "pclk_peri_peri_top_twg"));

	clk_disable_unprepare(clk_get(NULL, "pclk_peri_dss_hclk"));

	//clk_disable_unprepare(clk_get(NULL, "pclk_peri_ics_ahb2ahb_clk"));
	//clk_disable_unprepare(clk_get(NULL, "pclk_peri_gpu_sys_clk"));
	//clk_disable_unprepare(clk_get(NULL, "pclk_peri_ics_axi2axi_slave_clk"));

	clk_disable_unprepare(clk_get(NULL, "pclk_peri_aud_pcfg_clk"));
	//clk_disable_unprepare(clk_get(NULL, "pclk_peri_gpio_0_7"));
	//clk_disable_unprepare(clk_get(NULL, "pclk_peri_gpio_8_11"));

	if((clk_get(NULL, "cclk_gpu")->enable_count==0)){
		clk_disable_unprepare(clk_get(NULL, "pclk_peri_gpu_sys_clk"));
	}
	else
		printk(KERN_ERR "can't disable %s\n", clk_get(NULL, "pclk_peri_gpu_sys_clk")->name);

	if((clk_get(NULL, "ics_core")->enable_count==0)){
		clk_disable_unprepare(clk_get(NULL, "pclk_peri_ics_ahb2ahb_clk"));
		clk_disable_unprepare(clk_get(NULL, "pclk_peri_ics_axi2axi_slave_clk"));
	}
	else
		printk(KERN_ERR "can't disable %s\n", clk_get(NULL, "pclk_peri_ics_ahb2ahb_clk")->name);

	if((clk_get(NULL, "pclk_peri_sysconf")->enable_count==0)\
	&& (clk_get(NULL, "pclk_peri_dmc0")->enable_count==0) \
	&& (clk_get(NULL, "pclk_peri_dmc1")->enable_count==0) \
	&& (clk_get(NULL, "pclk_peri_cpu_pclk_sys")->enable_count==0) \
	&& (clk_get(NULL, "pclk_peri_peri_top_ius")->enable_count==0) \
	&& (clk_get(NULL, "pclk_peri_peri_top_twg")->enable_count==0) \
	&& (clk_get(NULL, "pclk_peri_dss_hclk")->enable_count==0) \
	&& (clk_get(NULL, "pclk_peri_ics_ahb2ahb_clk")->enable_count==0) \
	&& (clk_get(NULL, "pclk_peri_gpu_sys_clk")->enable_count==0)\
	&& (clk_get(NULL, "pclk_peri_ics_axi2axi_slave_clk")->enable_count==0)\
	&& (clk_get(NULL, "pclk_peri_aud_pcfg_clk")->enable_count==0)\
	&& (clk_get(NULL, "pclk_peri_gpio_0_7")->enable_count==0)\
	&& (clk_get(NULL, "pclk_peri_gpio_8_11")->enable_count==0)){
		clk_disable_unprepare(clk_get(NULL, "pbus_core"));
	}
	else
		printk(KERN_ERR "can't disable %s\n", clk_get(NULL, "pbus_core")->name);
}

void woden_ics_clk_disable(void)
{
	if((clk_get(NULL, "sdio0_core")->enable_count==0)\
	&& (clk_get(NULL, "ics_core")->enable_count==1))
	{
		clk_disable_unprepare(clk_get(NULL, "ics_core"));
	}
	else
		printk(KERN_ERR "can't disable %s\n", clk_get(NULL, "ics_core")->name);
}

void woden_gbus_clk_disable(void)
{
	if((clk_get(NULL, "dbus_core")->enable_count==0)\
	&& (clk_get(NULL, "abus_core")->enable_count==0)\
	&& (clk_get(NULL, "ics_core")->enable_count==0))
	{
		clk_disable_unprepare(clk_get(NULL, "gbus_core"));
	}
	else
		printk(KERN_ERR "can't disable %s\n",(clk_get(NULL, "gbus_core")->name));
}

void woden_ca15_clk_disable(void)
{
	clk_disable_unprepare(clk_get(NULL, "cluster0"));
	clk_disable_unprepare(clk_get(NULL, "ca15_aclkenm"));
	clk_disable_unprepare(clk_get(NULL, "ca15_aclkens"));
	clk_disable_unprepare(clk_get(NULL, "ca15_pphclken"));
	clk_disable_unprepare(clk_get(NULL, "ca15_atclken"));
	clk_disable_unprepare(clk_get(NULL, "ca15_pclkdbg"));
	clk_disable_unprepare(clk_get(NULL, "ca15_pclkendbg"));
}

void woden_ca7_clk_disable(void)
{
	clk_disable_unprepare(clk_get(NULL, "cluster1"));
	clk_disable_unprepare(clk_get(NULL, "ca7_aclkenm"));
	clk_disable_unprepare(clk_get(NULL, "ca7_pclkdbg"));
	clk_disable_unprepare(clk_get(NULL, "ca7_gic_400"));
	clk_disable_unprepare(clk_get(NULL, "ca7_cci_400"));
	clk_disable_unprepare(clk_get(NULL, "cpucnt"));
	clk_disable_unprepare(clk_get(NULL, "trace"));
}

#endif //CLK_DISABLE end

int woden_clk_count(void)
{
	struct clk_lookup *c;

	for(c = woden_clks; c < woden_clks + ARRAY_SIZE(woden_clks); c++ )
	{
		printk("%s's enable_count:%d\n", __clk_get_name(c->clk), c->clk->enable_count);
	}
	return 0;
}
late_initcall(woden_clk_count);

void woden_pm_do_save(struct reg_save *ssp, int count)
{
	for(; count > 0; count--, ssp++){
		ssp->reg_val = __raw_readl(ssp->reg);
		//writel(0x0, ssp->reg); // this will be erased after suspend&resume function is merged!!
	}
}

void woden_pm_do_restore(struct reg_save *ssp, int count)
{
	for(; count > 0; count--, ssp++){
		printk("restore %p (restore %08lx, was %08x)\n",
		       ssp->reg, ssp->reg_val, __raw_readl(ssp->reg));
		__raw_writel(ssp->reg_val, ssp->reg);
	}
}

static int woden_clk_suspend(void)
{
#if CLK_DISABLE
	woden_ics_clk_disable();
	woden_dbus_clk_disable();
	woden_abus_clk_disable();
	woden_pbus_clk_disable();
	woden_gbus_clk_disable();
#endif

	woden_pm_do_save(woden_clk_save, ARRAY_SIZE(woden_clk_save));

	printk("%s : clk_enable_count=============================================\n",__func__);
	woden_clk_count();
	printk("%s : end==========================================================\n", __func__);

	return 0;
}


static void woden_clk_resume(void)
{
	struct clk_lookup *c;

	woden_pm_do_restore(woden_clk_save, ARRAY_SIZE(woden_clk_save));

	for(c = woden_clks; c < woden_clks + ARRAY_SIZE(woden_clks); c++ )
	{
		clkdev_add(c);
		__clk_init(NULL, c->clk);
		//printk("%s's rate:%lu\n", __clk_get_name(c->clk), __clk_get_rate(c->clk));
	}
	woden_gbus_clk_enable();
	woden_dbus_clk_enable();
	woden_abus_clk_enable();
	woden_pbus_clk_enable();
	woden_ics_clk_enable();
}

struct syscore_ops woden_clk_syscore_ops = {
	.suspend = woden_clk_suspend,
	.resume = woden_clk_resume,
};

#endif //CONFIG_WODEN_SLEEP END

int __init woden_default_clk_enable(void)
{

	struct device_node *np, *from;
	struct clk *clk;
	int ret = 1;

	from = of_find_node_by_name(NULL, "clocks");

	for(np = of_get_next_child(from, NULL); np; np=of_get_next_child(from, np)){

		u32 val[5];

		of_property_read_u32_array(np, "reg-flag", val, ARRAY_SIZE(val));

		if(val[0]==CPU_CLK){
			clk = clk_get(NULL, np->name);

			if(!clk){
				printk("cpu clock get error!!!\n");
				return -1;
			}
		}else if(val[0]==AUD_CLK){
			clk = clk_get(NULL, np->name);

			if(!clk){
				printk("audio clock get error!!!\n");
				return -1;
			}
		}else{
			clk = clk_get(NULL, np->name);

			if(!clk){
				printk("common clock get error!!!\n");
				return -1;
			}
		}

		ret = clk_prepare_enable(clk);
			if(ret)
			{
				printk(KERN_ERR "can't enable %s\n", clk->name);
				return ret;
			}
	}

	return ret;
}

int woden_clk_init(void)
{
	struct clk *clk = NULL;
	struct device_node *np, *from;
	const char *parent_name;
	void __iomem *base, *cpu_base, *aud_base;

	base = ioremap(WODEN_CRG_BASE, SZ_4K);
	woden_register_clk(base, WODEN_CRG_BASE);

	cpu_base = ioremap(WODEN_CPU_CFG_BASE, SZ_8K);
	woden_register_cpu_clk(cpu_base, WODEN_CPU_CFG_BASE);

	aud_base = ioremap(WODEN_AUD_CRG_BASE, SZ_4K);
	woden_audio_clk_setup(aud_base);
	woden_register_aud_clk(aud_base, WODEN_AUD_CRG_BASE);

	from = of_find_node_by_name(NULL, "clocks");

	for(np = of_get_next_child(from, NULL); np; np=of_get_next_child(from, np)){

			u32 val[5];

			of_property_read_u32_array(np, "reg-flag", val, ARRAY_SIZE(val));
			of_property_read_string(np, "clock-parent", &parent_name);

			if(val[0]==COM_CLK){
				clk = odin_clk_register_gate(NULL, np->name, parent_name,
				val[1], base + val[2], WODEN_CRG_BASE + val[2],
				val[3], val[4], NON_SECURE, &clk_out_lock);
			} else if(val[0]==CPU_CLK){
				clk = odin_clk_register_gate(NULL, np->name, parent_name,
				val[1], cpu_base + val[2], WODEN_CPU_CFG_BASE+val[2],
				val[3], val[4], NON_SECURE, &clk_out_lock);
			} else if(val[0]==AUD_CLK){
				clk = odin_clk_register_gate(NULL, np->name, parent_name,
				val[1], aud_base + val[2], WODEN_AUD_CRG_BASE+val[2],
				val[3], val[4], NON_SECURE, &clk_out_lock);
			} else{
				clk = clk_register_fixed_rate(NULL, np->name, NULL, CLK_IS_ROOT, 24000000);
			}

			of_clk_add_provider(np, of_clk_src_simple_get, clk);
			clk_register_clkdev(clk, np->name, NULL);

			printk("%s's rate:%lu  parent:%s\n", clk->name,
			clk_get_rate(clk), __clk_get_name(clk_get_parent(clk)));
	}

	woden_default_clk_enable();

#ifdef CONFIG_WODEN_SLEEP
	register_syscore_ops(&woden_clk_syscore_ops);
#endif


	return 0;
}

