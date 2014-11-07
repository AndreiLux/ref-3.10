#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/clk-private.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <asm/mach/map.h>
#include <linux/of.h>
#include <linux/odin_clk.h>
#ifdef CONFIG_PM_SLEEP
#include <linux/platform_device.h>
#endif

#ifdef CONFIG_PM_SLEEP_DEBUG
extern void clock_debug_print_enabled(void);
#endif
static DEFINE_SPINLOCK(clk_out_lock);

#ifdef CONFIG_PM_SLEEP
#if 0
static struct reg_save cpu_clk_save[] = {
       SAVE_REG(CPU_CFG_REG0),
       SAVE_REG(CPU_CFG_REG1),
       SAVE_REG(CPU_CFG_REG2),
       SAVE_REG(CPU_CFG_REG3),
       SAVE_REG(CPU_CFG_REG4),
       SAVE_REG(CPU_CFG_REG5),
       SAVE_REG(CPU_CFG_REG6),
       SAVE_REG(CPU_CFG_REG7),
       SAVE_REG(CPU_CFG_REG8),
       SAVE_REG(CPU_CFG_REG9),
       SAVE_REG(CPU_CFG_REG10),
       SAVE_REG(CPU_CFG_REG11),
};

static struct reg_save sms_clk_save[] = {
       SAVE_REG(ISP_PLL_CON0),
       SAVE_REG(NOC_PLL_CON0),
       SAVE_REG(DSS_PLL_CON0),
       SAVE_REG(GPU_PLL_CON0),
       SAVE_REG(DP1_PLL_CON0),
       SAVE_REG(DP2_PLL_CON0),
};

static struct reg_save core_clk_save[] = {
       SAVE_REG(SCFG_CLK_SRC_SEL),
       SAVE_REG(SCFG_CLK_SRC_MSK),
       SAVE_REG(SCFG_CLK_DIV_VAL),
       SAVE_REG(SCFG_CLK_OUT_ENA),
       SAVE_REG(SNOC_CLK_SRC_SEL),
       SAVE_REG(SNOC_CLK_SRC_MSK),
       SAVE_REG(SNOC_CLK_DIV_VAL),
       SAVE_REG(SNOC_CLK_OUT_ENA),
       SAVE_REG(ANOC_CLK_SRC_SEL),
	   SAVE_REG(ANOC_CLK_SRC_MSK),
	   SAVE_REG(ANOC_CLK_DIV_VAL),
	   SAVE_REG(ANOC_CLK_OUT_ENA),
	   SAVE_REG(GDNOC_CLK_SRC_SEL),
	   SAVE_REG(GDNOC_CLK_SRC_MSK),
	   SAVE_REG(GDNOC_CLK_DIV_VAL),
	   SAVE_REG(GDNOC_CLK_OUT_ENA),
	   SAVE_REG(MEM_PLL_CON0),
	   SAVE_REG(NPLL_CON),
	   SAVE_REG(NPLL_DSS_CON),
};
#endif

static struct reg_save aud_clk_save[] = {
		SAVE_REG(AUD_PLL_CON0),
		SAVE_REG(AUD_PLL_CON1),
		SAVE_REG(DSP_CCLKSRC_SEL),
		SAVE_REG(DSP_CCLKSRC_MASK),
		SAVE_REG(DSP_CCLK_DIV),
		SAVE_REG(DSP_CCLK_GATE0),
		SAVE_REG(AUD_ACLK_DIV),
		SAVE_REG(AUD_PCLK_DIV),
		SAVE_REG(ASYNC_PCLKSRC_SEL),
		SAVE_REG(ASYNC_PCLK_MASK),
		SAVE_REG(ASYNC_PCLK_DIV),
		SAVE_REG(ASYNC_PCLK_GATE),
		SAVE_REG(I2S0_SCLKSRC_SEL),
		SAVE_REG(I2S0_SCLK_MASK),
		SAVE_REG(I2S0_SCLK_DIV),
		SAVE_REG(I2S0_SCLK_GATE0),
		SAVE_REG(I2S1_SCLKSRC_SEL),
		SAVE_REG(I2S1_SCLK_MASK),
		SAVE_REG(I2S1_SCLK_DIV),
		SAVE_REG(I2S1_SCLK_GATE0),
		SAVE_REG(I2S2_SCLKSRC_SEL),
		SAVE_REG(I2S2_SCLK_MASK),
		SAVE_REG(I2S2_SCLK_DIV),
		SAVE_REG(I2S2_SCLK_GATE0),
		SAVE_REG(I2S3_SCLKSRC_SEL),
		SAVE_REG(I2S3_SCLK_MASK),
		SAVE_REG(I2S3_SCLK_DIV),
		SAVE_REG(I2S3_SCLK_GATE0),
		SAVE_REG(I2S4_SCLKSRC_SEL),
		SAVE_REG(I2S4_SCLK_MASK),
		SAVE_REG(I2S4_SCLK_DIV),
		SAVE_REG(I2S4_SCLK_GATE0),
		SAVE_REG(I2S5_SCLKSRC_SEL),
		SAVE_REG(I2S5_SCLK_MASK),
		SAVE_REG(I2S5_SCLK_DIV),
		SAVE_REG(I2S5_SCLK_GATE0),
};

static struct reg_save peri_clk_save[] = {
		SAVE_REG(CFG_PNOC_ACLK),
		SAVE_REG(CFG_CRG_PCLK),
		SAVE_REG(CFG_WT_CLK),
		SAVE_REG(CFG_PWM_PCLK),
		SAVE_REG(CFG_WDT_PCLK),
		SAVE_REG(CFG_TMR_PCLK0),
		SAVE_REG(CFG_TMR_PCLK1),
		SAVE_REG(CFG_G0_GPIO_PCLK),
		SAVE_REG(CFG_G1_GPIO_PCLK),
		SAVE_REG(CFG_SRAM_HCLK),
		SAVE_REG(CFG_PDMA_HCLK),
		SAVE_REG(CFG_SDC_ACLK),
		SAVE_REG(CFG_SDIO_ACLK0),
		SAVE_REG(CFG_SDIO_ACLK1),
		SAVE_REG(CFG_EMMC_ACLK),
		SAVE_REG(CFG_E2NAND_ACLK),
		SAVE_REG(CFG_I2C_PCLK0),
		SAVE_REG(CFG_I2C_PCLK1),
		SAVE_REG(CFG_I2C_PCLK2),
		SAVE_REG(CFG_I2C_PCLK3),
		SAVE_REG(CFG_SPI_PCLK0),
		SAVE_REG(CFG_UART_PCLK0),
		SAVE_REG(CFG_UART_PCLK1),
		SAVE_REG(CFG_UART_PCLK2),
		SAVE_REG(CFG_UART_PCLK3),
		SAVE_REG(CFG_UART_PCLK4),
		SAVE_REG(CFG_SDC_SCLK),
		SAVE_REG(CFG_SDIO_SCLK0),
		SAVE_REG(CFG_SDIO_SCLK1),
		SAVE_REG(CFG_EMMC_SCLK),
		SAVE_REG(CFG_E2NAND_SCLK),
		SAVE_REG(CFG_I2C_SCLK0),
		SAVE_REG(CFG_I2C_SCLK1),
		SAVE_REG(CFG_I2C_SCLK2),
		SAVE_REG(CFG_I2C_SCLK3),
		SAVE_REG(CFG_SPI_SCLK0),
		SAVE_REG(CFG_UART_SCLK0),
		SAVE_REG(CFG_UART_SCLK1),
		SAVE_REG(CFG_UART_SCLK2),
		SAVE_REG(CFG_UART_SCLK3),
		SAVE_REG(CFG_UART_SCLK4),
};
#endif

static void __init odin_audio_clk_setup(void __iomem *base)
{
	/* Audio reset release */
	writel(0xffffffff , base + 0x100);
	writel(0xffffffff , base + 0x104);

#if 0 /* audio subsystem source clock: dmc clock */
	/* testing for max performance */
	writel(0x1, base + 0x10);	/* dmc clodk select = 400Mhz*/
	writel(0x1, base + 0x18);	/* div_CCLK 1 = 200Mhz */
	writel(0x0, base + 0x1C);	/* DSP enable */
	writel(0x0, base + 0x20);	/* div_ACLK 1 = 100Mhz*/
	writel(0x0, base + 0x28);	/* div_PCLK 2 = 25Mhz*/
#else /* audio subsystem source clock: osillator */
	writel(0x0, base + 0x10);	/* osc clodk select = 24Mhz*/
	writel(0x0, base + 0x18);	/* div_CCLK = 24Mhz */
	writel(0x0, base + 0x1C);	/* DSP enable */
	writel(0x0, base + 0x20);	/* div_ACLK = 24Mhz*/
	writel(0x0, base + 0x28);	/* div_PCLK = 24Mhz*/
#endif

	/* Audio Async PCLK setting */
	writel(0x0, base + 0x30);      /* Main clock source= OSC_CLK */
	writel(0x0, base + 0x38);      /* div= 0 */
	writel(0x1, base + 0x3c);      /* gate = clock enable */

	writel(0x00, base + 0x50);		/* I2S0 Main Clock Source Selection to Osillator*/
	writel(0x00, base + 0x5c);      /* I2S0 clock enable */
	writel(0x00, base + 0x6c);      /* I2S1 clock enable */
	writel(0x00, base + 0x7c);      /* I2S2 clock enable */
	writel(0x00, base + 0x8c);      /* I2S3 clock enable */
	writel(0x00, base + 0x9c);      /* I2S4 clock enable */
	writel(0x00, base + 0xac);      /* I2S5 clock enable */

	writel(0x01, base + 0xa0);      /* I2S5 Main Clock Source Selection to AUD PLL*/
}

static void __init odin_vsp0_clk_setup( void __iomem *base)
{
#ifdef CONFIG_ODIN_TEE
	tz_write(0x1, base + 0x0200);
	tz_write(0x1, base + 0x0204);
	tz_write(0x0, base + 0x0208);
	tz_write(0x1, base + 0x020c);
/*
	tz_write(0x1, base + 0x0210);
	tz_write(0x1, base + 0x0214);
	tz_write(0x0, base + 0x0218);
*/
	tz_write(0x1, base + 0x021c);
	tz_write(0x1, base + 0x0240);
	tz_write(0x1, base + 0x0244);
	tz_write(0x0, base + 0x0248);
	tz_write(0x1, base + 0x024c);
	tz_write(0x1, base + 0x0250);
	tz_write(0x1, base + 0x0254);
	tz_write(0x3, base + 0x0258);
	tz_write(0x1, base + 0x025c);
#else
	writel(0x1, base + 0x0200);
	writel(0x1, base + 0x0204);
	writel(0x0, base + 0x0208);
	writel(0x1, base + 0x020c);
/*
	writel(0x1, base + 0x0210);
	writel(0x1, base + 0x0214);
	writel(0x0, base + 0x0218);
*/
	writel(0x1, base + 0x021c);
	writel(0x1, base + 0x0240);
	writel(0x1, base + 0x0244);
	writel(0x0, base + 0x0248);
	writel(0x1, base + 0x024c);
	writel(0x1, base + 0x0250);
	writel(0x1, base + 0x0254);
	writel(0x3, base + 0x0258);
	writel(0x1, base + 0x025c);
#endif
}

static void __init odin_vsp1_clk_setup( void __iomem *base)
{
#ifdef CONFIG_ODIN_TEE
	tz_write(0x1, base + 0x0220);
	tz_write(0x1, base + 0x0224);
	tz_write(0x0, base + 0x0228);
	tz_write(0x1, base + 0x022c);
	tz_write(0x2, base + 0x0230);
	tz_write(0x1, base + 0x0234);
	tz_write(0x1, base + 0x0238);
	tz_write(0x1, base + 0x023c);
	tz_write(0x1, base + 0x0240);
	tz_write(0x1, base + 0x0244);
	tz_write(0x0, base + 0x0248);
	tz_write(0x1, base + 0x024c);
	tz_write(0x1, base + 0x0250);
	tz_write(0x1, base + 0x0254);
	tz_write(0x3, base + 0x0258);
	tz_write(0x1, base + 0x025c);
#else
	writel(0x1, base + 0x0220);
	writel(0x1, base + 0x0224);
	writel(0x0, base + 0x0228);
	writel(0x1, base + 0x022c);
	writel(0x2, base + 0x0230);
	writel(0x1, base + 0x0234);
	writel(0x1, base + 0x0238);
	writel(0x1, base + 0x023c);
	writel(0x1, base + 0x0240);
	writel(0x1, base + 0x0244);
	writel(0x0, base + 0x0248);
	writel(0x1, base + 0x024c);
	writel(0x1, base + 0x0250);
	writel(0x1, base + 0x0254);
	writel(0x3, base + 0x0258);
	writel(0x1, base + 0x025c);
#endif
}

int odin_dt_clk_init(struct clk **clk, int count, void __iomem *base, u32 p_base, int block_type)
{
       struct device_node *np, *from;
       const char *parent_name;
	   int i = 0;

       from = of_find_node_by_name(NULL, "clocks");

       for(np = of_get_next_child(from, NULL); np; np=of_get_next_child(from, np)){

               u32 val[6];

               of_property_read_u32_array(np, "reg-flag", val, ARRAY_SIZE(val));

               if(val[0] == block_type){
                       of_property_read_string(np, "clock-parent", &parent_name);
                       clk[(count+1)+i] = odin_clk_register_gate(NULL, np->name, parent_name,
					   val[1], base+val[2], p_base+val[2], val[3], val[4], val[5], &clk_out_lock);

                       of_clk_add_provider(np, of_clk_src_simple_get, clk[(count+1)+i]);
                       clk_register_clkdev(clk[(count+1)+i], np->name, NULL);

                       //printk("%s's rate:%lu  parent:%s\n", clk[count+1+i]->name,
                       //clk_get_rate(clk[count+1+i]), __clk_get_name(clk_get_parent(clk[count+1+i])));
					   i++;
               }
       }

       return 0;
}

enum root_crgclk{
	osc_clk, rtc_clk,

	root_crgclk_cnt
};
char *root_crgclk_names[] = {
	"osc_clk", "rtc_clk",

	"root_crgclk_cnt"
};

static struct clk *root_crg_clk[root_crgclk_cnt];

void root_crg_reg(void){
	root_crg_clk[osc_clk] = clk_register_fixed_rate(NULL, "osc_clk", NULL, CLK_IS_ROOT, 24000000);
	root_crg_clk[rtc_clk] = clk_register_fixed_rate(NULL, "rtc_clk", NULL, CLK_IS_ROOT, 32000);
}

enum sms_crgclk{
	isp_pll_clk, noc_pll_clk, dss_pll_clk, gpu_pll_clk, dp1_pll_clk, dp2_pll_clk,

	sms_crgclk_cnt
};

char *sms_crgclk_names[] = {
	"isp_pll_clk", "noc_pll_clk", "dss_pll_clk", "gpu_pll_clk", "dp2_pll_clk",
	"dp2_pll_clk",

	"sms_crgclk_cnt"
};

static struct clk *sms_crg_clk[sms_crgclk_cnt];

void sms_crg_reg(void __iomem *sms_base, void __iomem *gpu_base, u32 p_sms_base, u32 p_gpu_base)
{
	sms_crg_clk[isp_pll_clk] = odin_clk_register_ljpll(NULL, "isp_pll_clk", "osc_clk", 0,
							sms_base+0x0E0, sms_base+0x0E0, sms_base+0x0E8, p_sms_base+0x0E0,
							p_sms_base+0x0E0, p_sms_base+0x0E8, 1, 0, MACH_ODIN, ISP_PLL_CLK,
							SECURE, &clk_out_lock);

	sms_crg_clk[noc_pll_clk] = odin_clk_register_ljpll(NULL, "noc_pll_clk", "osc_clk", 0,
							sms_base+0x120, sms_base+0x120, sms_base+0x128, p_sms_base+0x120,
							p_sms_base+0x120, p_sms_base+0x128, 1, 0, MACH_ODIN, COMM_PLL_CLK,
							SECURE, &clk_out_lock);

	sms_crg_clk[dss_pll_clk] = odin_clk_register_ljpll(NULL, "dss_pll_clk", "osc_clk", 0,
							sms_base+0x190, sms_base+0x190, sms_base+0x198, p_sms_base+0x190,
							p_sms_base+0x190, p_sms_base+0x198, 1, 0, MACH_ODIN, COMM_PLL_CLK,
							SECURE, &clk_out_lock);

	sms_crg_clk[gpu_pll_clk] = odin_clk_register_ljpll_dvfs(NULL, "gpu_pll_clk", "osc_clk", 0,
							sms_base+0x200, sms_base+0x200, sms_base+0x208, gpu_base+0x208, NULL,
							p_sms_base+0x200, p_sms_base+0x200, p_sms_base+0x208, p_gpu_base+0x208,
							0x0, 1, 0, 0, 4, 0, MACH_ODIN, GPU_PLL_CLK, SECURE, &clk_out_lock);

	sms_crg_clk[dp1_pll_clk] = odin_clk_register_ljpll(NULL, "dp1_pll_clk", "osc_clk", 0,
							sms_base+0x4B0, sms_base+0x4B0, sms_base+0x4B8, p_sms_base+0x4B0,
							p_sms_base+0x4B0, p_sms_base+0x4B8, 1, 0, MACH_ODIN, COMM_PLL_CLK,
							SECURE, &clk_out_lock);

	sms_crg_clk[dp2_pll_clk] = odin_clk_register_ljpll(NULL, "dp2_pll_clk", "osc_clk", 0,
							sms_base+0x4C0, sms_base+0x4C0, sms_base+0x4C8, p_sms_base+0x4C0,
							p_sms_base+0x4C0, p_sms_base+0x4C8, 1, 0, MACH_ODIN, DP2_PLL_CLK,
							SECURE, &clk_out_lock);

}

static const char *scfg_pll_sels_name[] = {"osc_clk", "noc_pll_clk"};
static const char *scfg_src_sels_name[] = {"scfg_pll_sel_clk", "mem_pll_clk"};
static const char *snoc_pll_sels_name[] = {"osc_clk", "noc_pll_clk"};
static const char *snoc_src_sels_name[] = {"snoc_pll_sel_clk", "mem_pll_clk"};
static const char *anoc_pll_sels_name[] = {"osc_clk", "noc_pll_clk"};
static const char *anoc_src_sels_name[] = {"anoc_pll_sel_clk", "mem_pll_clk"};
static const char *gdnoc_pll_sels_name[] = {"osc_clk", "noc_pll_clk"};
static const char *gdnoc_src_sels_name[] = {"gdnoc_pll_sel_clk", "mem_pll_clk"};
static const char *memc_src_sels_name[] = {"osc_clk", "mem_pll_clk"};
static const char *npll_src_sels_name[] = {"mem_pll_clk", "noc_pll_clk"};
static const char *npll_dss_pll_sels_name[] = {"mem_pll_clk", "noc_pll_clk"};
static const char *npll_dss_src_sels_name[] = {"npll_dss_pll_sel_clk", "isp_pll_clk"};

enum core_crgclk{
	mem_pll_clk, scfg_pll_sel_clk, scfg_src_sel_clk, scfg_mask_clk,	scfg_div_clk, snoc_pll_sel_clk,
	snoc_src_sel_clk, snoc_mask_clk, snoc_div_clk, anoc_pll_sel_clk, anoc_src_sel_clk,
	anoc_mask_clk, anoc_div_clk, gdnoc_pll_sel_clk, gdnoc_src_sel_clk, gdnoc_mask_clk,
	gdnoc_div_clk, memc_src_sel_clk, npll_src_sel_clk, npll_clk, npll_dss_pll_sel_clk,
	npll_dss_src_sel_clk, npll_clk_dss, scfg, snoc, anoc, gdnoc, memc_clk,

	core_crgclk_cnt
};

char *core_crgclk_names[] = {
	"mem_pll_clk", "scfg_pll_sel_clk", "scfg_src_sel_clk", "scfg_mask_clk", "scfg_div_clk",
	"snoc_pll_sel_clk",	"snoc_src_sel_clk",	"snoc_mask_clk", "snoc_div_clk", "anoc_pll_sel_clk",
	"anoc_src_sel_clk", "anoc_mask_clk", "anoc_div_clk", "gdnoc_pll_sel_clk", "gdnoc_src_sel_clk",
	"gdnoc_mask_clk", "gdnoc_div_clk", "memc_src_sel_clk", "npll_src_sel_clk", "npll_clk",
	"npll_dss_pll_sel_clk", "npll_dss_src_sel_clk", "npll_clk_dss", "scfg", "snoc", "anoc",
	"gdnoc", "memc_clk",

	"core_crgclk_cnt"
};

static struct clk *core_crg_clk[core_crgclk_cnt];

void core_crg_reg(void __iomem *core_base, u32 p_core_base)
{
	core_crg_clk[mem_pll_clk] = odin_clk_register_ljpll(NULL, "mem_pll_clk", "osc_clk", 0,
	core_base+0x400, core_base+0x400, NULL, p_core_base+0x400, p_core_base+0x400, 0x0, 1, 0,
	MACH_ODIN, MEM_PLL_CLK, SECURE, &clk_out_lock);

	core_crg_clk[scfg_pll_sel_clk] = odin_clk_register_mux(NULL, "scfg_pll_sel_clk",
	scfg_pll_sels_name, ARRAY_SIZE(scfg_pll_sels_name), CLK_SET_RATE_PARENT, core_base,
	p_core_base, 0, 1, 0, SECURE, &clk_out_lock);

	core_crg_clk[scfg_src_sel_clk] = odin_clk_register_mux(NULL, "scfg_src_sel_clk",
	scfg_src_sels_name, ARRAY_SIZE(scfg_src_sels_name), CLK_SET_RATE_PARENT, core_base,
	p_core_base, 4, 1, 0, SECURE, &clk_out_lock);

	core_crg_clk[scfg_mask_clk] = odin_clk_register_gate(NULL, "scfg_mask_clk", "scfg_src_sel_clk",
	CLK_SET_RATE_PARENT, core_base+0x4, p_core_base+0x4,
	0, 0, SECURE, &clk_out_lock);

	core_crg_clk[scfg_div_clk] = odin_clk_register_div(NULL, "scfg_div_clk", "scfg_mask_clk",
	0, core_base+0x8, NULL, p_core_base+0x8, 0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	SECURE, &clk_out_lock);

	core_crg_clk[snoc_pll_sel_clk] = odin_clk_register_mux(NULL, "snoc_pll_sel_clk",
	snoc_pll_sels_name, ARRAY_SIZE(snoc_pll_sels_name), CLK_SET_RATE_PARENT, core_base+0x100,
	p_core_base+0x100, 0, 1, 0, SECURE, &clk_out_lock);

	core_crg_clk[snoc_src_sel_clk] = odin_clk_register_mux(NULL, "snoc_src_sel_clk",
	snoc_src_sels_name, ARRAY_SIZE(snoc_src_sels_name), CLK_SET_RATE_PARENT, core_base+0x100,
	p_core_base+0x100, 4, 1, 0, SECURE, &clk_out_lock);

	core_crg_clk[snoc_mask_clk] = odin_clk_register_gate(NULL, "snoc_mask_clk", "snoc_src_sel_clk",
	CLK_SET_RATE_PARENT, core_base+0x104, p_core_base+0x104,
	0, 0, SECURE, &clk_out_lock);

	core_crg_clk[snoc_div_clk] = odin_clk_register_div(NULL, "snoc_div_clk", "snoc_mask_clk",
	0, core_base+0x108, NULL, p_core_base+0x108,
	0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0, SECURE, &clk_out_lock);

	core_crg_clk[anoc_pll_sel_clk] = odin_clk_register_mux(NULL, "anoc_pll_sel_clk",
	anoc_pll_sels_name, ARRAY_SIZE(anoc_pll_sels_name), CLK_SET_RATE_PARENT, core_base+0x200,
	p_core_base+0x200, 0, 1, 0, SECURE, &clk_out_lock);

	core_crg_clk[anoc_src_sel_clk] = odin_clk_register_mux(NULL, "anoc_src_sel_clk",
	anoc_src_sels_name, ARRAY_SIZE(anoc_src_sels_name), CLK_SET_RATE_PARENT, core_base+0x200,
	p_core_base+0x200, 4, 1, 0, SECURE, &clk_out_lock);

	core_crg_clk[anoc_mask_clk] = odin_clk_register_gate(NULL, "anoc_mask_clk", "anoc_src_sel_clk",
	CLK_SET_RATE_PARENT, core_base+0x204, p_core_base+0x204,
	0, 0, SECURE, &clk_out_lock);

	core_crg_clk[anoc_div_clk] = odin_clk_register_div(NULL, "anoc_div_clk", "anoc_mask_clk",
	0, core_base+0x208, NULL, p_core_base+0x208, 0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	SECURE, &clk_out_lock);

	core_crg_clk[gdnoc_pll_sel_clk] = odin_clk_register_mux(NULL, "gdnoc_pll_sel_clk",
	gdnoc_pll_sels_name, ARRAY_SIZE(gdnoc_pll_sels_name), CLK_SET_RATE_PARENT, core_base+0x300,
	p_core_base+0x300, 0, 1, 0, SECURE, &clk_out_lock);

	core_crg_clk[gdnoc_src_sel_clk] = odin_clk_register_mux(NULL, "gdnoc_src_sel_clk",
	gdnoc_src_sels_name, ARRAY_SIZE(gdnoc_src_sels_name), CLK_SET_RATE_PARENT, core_base+0x300,
	p_core_base+0x300, 4, 1, 0, SECURE, &clk_out_lock);

	core_crg_clk[gdnoc_mask_clk] = odin_clk_register_gate(NULL, "gdnoc_mask_clk",
	"gdnoc_src_sel_clk", CLK_SET_RATE_PARENT, core_base+0x304, p_core_base+0x304,
	0, 0, SECURE, &clk_out_lock);

	core_crg_clk[gdnoc_div_clk] = odin_clk_register_div(NULL, "gdnoc_div_clk", "gdnoc_mask_clk",
	0, core_base+0x308, NULL, p_core_base+0x308, 0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	SECURE, &clk_out_lock);

	core_crg_clk[memc_src_sel_clk] = odin_clk_register_mux(NULL, "memc_src_sel_clk"
	, memc_src_sels_name, ARRAY_SIZE(memc_src_sels_name), CLK_SET_RATE_PARENT, core_base+0x408,
	p_core_base+0x408, 0, 1, 0, SECURE, &clk_out_lock);

	core_crg_clk[npll_src_sel_clk] = odin_clk_register_mux(NULL, "npll_src_sel_clk",
	npll_src_sels_name, ARRAY_SIZE(npll_src_sels_name), CLK_SET_RATE_PARENT, core_base+0x500,
	p_core_base+0x500, 0, 1, 0, SECURE, &clk_out_lock);

	core_crg_clk[npll_clk] = odin_clk_register_gate(NULL, "npll_clk", "npll_src_sel_clk",
	CLK_SET_RATE_PARENT, core_base+0x500, p_core_base+0x500, 4, 0, SECURE, &clk_out_lock);

	core_crg_clk[npll_dss_pll_sel_clk] = odin_clk_register_mux(NULL, "npll_dss_pll_sel_clk",
	npll_dss_pll_sels_name, ARRAY_SIZE(npll_dss_pll_sels_name), CLK_SET_RATE_PARENT,
	core_base+0x504, p_core_base+0x504, 0, 1, 0, SECURE, &clk_out_lock);

	core_crg_clk[npll_dss_src_sel_clk] = odin_clk_register_mux(NULL, "npll_dss_src_sel_clk",
	npll_dss_src_sels_name, ARRAY_SIZE(npll_dss_src_sels_name), CLK_SET_RATE_PARENT,
	core_base+0x504, p_core_base+0x504, 1, 1, 0, SECURE, &clk_out_lock);

	core_crg_clk[npll_clk_dss] = odin_clk_register_gate(NULL, "npll_clk_dss",
	"npll_dss_src_sel_clk", CLK_SET_RATE_PARENT, core_base+0x504, p_core_base+0x504,
	4, 0, SECURE, &clk_out_lock);

	odin_dt_clk_init(core_crg_clk, npll_clk_dss, core_base, p_core_base, CORE_BLOCK);
}

static const char *ca15_pll_sels_name[] = {"osc_clk", "ca15_pll_clk"};
static const char *ca7_pll_sels_name[] = {"osc_clk", "ca7_pll_clk"};
static const char *cclk_src_sels_name[] = {"osc_clk", "noc_pll_clk", "osc_clk"};
static const char *hclk_src_sels_name[] = {"osc_clk", "npll_clk", "osc_clk"};
static const char *clk_cpucnt_src_sels_name[] = {"osc_clk", "npll_clk", "osc_clk"};
static const char *pclk_sapb_src_sels_name[] = {"osc_clk", "npll_clk", "osc_clk"};
static const char *gclk_src_sels_name[] = {"osc_clk", "npll_clk", "osc_clk"};
static const char *atclk_src_sels_name[] = {"osc_clk", "npll_clk", "osc_clk"};
static const char *trace_src_sels_name[] = {"osc_clk", "npll_clk", "osc_clk"};

enum cpu_crgclk{
	ca15_pll_clk,
	ca7_pll_clk,
	ca15_pll_sel_clk,
	ca15_div_clk,
	ca7_pll_sel_clk,
	ca7_div_clk,
	cclk_src_sel_clk,
	cclk_mask_clk,
	cclk_div_clk,
	hclk_src_sel_clk,
	hclk_mask_clk,
	hclk_div_clk,
	clk_cpucnt_src_sel_clk,
	clk_cpucnt_mask_clk,
	clk_cpucnt_div_clk,
	pclk_sapb_src_sel_clk,
	pclk_sapb_mask_clk,
	pclk_sapb_div_clk,
	gclk_src_sel_clk,
	gclk_mask_clk,
	gclk_div_clk,
	atclk_src_sel_clk,
	atclk_mask_clk,
	atclk_div_clk,
	trace_src_sel_clk,
	trace_mask_clk,
	trace_div_clk,
	cluster0,
	cluster1,
	cclk,
	hclk,
	clk_cpucnt,
	pclk_sapb,
	gclk,
	atclk,
	trace,
	cpu_crgclk_cnt
};

char *cpu_crgclk_names[] = {
	"ca15_pll_clk",
	"ca7_pll_clk",
	"ca15_pll_sel_clk",
	"ca15_div_clk",
	"ca7_pll_sel_clk",
	"ca7_div_clk",
	"cclk_src_sel_clk",
	"cclk_mask_clk",
	"cclk_div_clk",
	"hclk_src_sel_clk",
	"hclk_mask_clk",
	"hclk_div_clk",
	"clk_cpucnt_src_sel_clk",
	"clk_cpucnt_mask_clk",
	"clk_cpucnt_div_clk",
	"pclk_sapb_src_sel_clk",
	"pclk_sapb_mask_clk",
	"pclk_sapb_div_clk",
	"gclk_src_sel_clk",
	"gclk_mask_clk",
	"gclk_div_clk",
	"atclk_src_sel_clk",
	"atclk_mask_clk",
	"atclk_div_clk",
	"trace_src_sel_clk",
	"trace_mask_clk",
	"trace_div_clk",
	"cluster0",
	"cluster1",
	"cclk",
	"hclk",
	"clk_cpucnt",
	"pclk_sapb",
	"gclk",
	"atclk",
	"trace",
	"cpu_crgclk_cnt"
};

static struct clk *cpu_crg_clk[core_crgclk_cnt];

void cpu_crg_reg(void __iomem *cpu_base, u32 p_cpu_base)
{
	cpu_crg_clk[ca15_pll_clk] = odin_clk_register_ljpll_dvfs(NULL, "ca15_pll_clk", "osc_clk", 0,
	cpu_base + 0x011C, cpu_base + 0x0120, NULL, cpu_base + 0x0104, cpu_base + 0x0100,
	p_cpu_base + 0x011C, p_cpu_base + 0x0120, 0x0, p_cpu_base + 0x0104, p_cpu_base + 0x0100,
	17, 19, 0, 8, 12, MACH_ODIN, CPU_PLL_CLK, SECURE, &clk_out_lock);

	cpu_crg_clk[ca7_pll_clk] = odin_clk_register_ljpll_dvfs(NULL, "ca7_pll_clk", "osc_clk", 0,
	cpu_base + 0x0140, cpu_base + 0x0144, NULL, cpu_base + 0x010C, cpu_base + 0x0110,
	p_cpu_base + 0x0140, p_cpu_base + 0x0144, 0x0, p_cpu_base + 0x010C, p_cpu_base + 0x0110,
	17, 19, 0, 8, 27, MACH_ODIN, CPU_PLL_CLK, SECURE, &clk_out_lock);

	cpu_crg_clk[ca15_pll_sel_clk] = odin_clk_register_mux(NULL, "ca15_pll_sel_clk",
	ca15_pll_sels_name, ARRAY_SIZE(ca15_pll_sels_name), CLK_SET_RATE_PARENT, cpu_base+0x100,
	p_cpu_base+0x100, 0, 1, 0, SECURE, &clk_out_lock);

	cpu_crg_clk[ca15_div_clk] = odin_clk_register_div(NULL, "ca15_div_clk", "ca15_pll_sel_clk",
	CLK_SET_RATE_PARENT, cpu_base+0x104, cpu_base+0x100, p_cpu_base+0x104, p_cpu_base+0x100,
	0, 8, 12, MACH_ODIN, CPU_PLL_CLK, 0, SECURE, &clk_out_lock);

	cpu_crg_clk[ca7_pll_sel_clk] = odin_clk_register_mux(NULL, "ca7_pll_sel_clk", ca7_pll_sels_name,
	ARRAY_SIZE(ca7_pll_sels_name), CLK_SET_RATE_PARENT, cpu_base+0x110, p_cpu_base+0x110,
	16, 1, 0, SECURE, &clk_out_lock);

	cpu_crg_clk[ca7_div_clk] = odin_clk_register_div(NULL, "ca7_div_clk", "ca7_pll_sel_clk",
	CLK_SET_RATE_PARENT, cpu_base+0x10C, cpu_base+0x110, p_cpu_base+0x10C, p_cpu_base+0x110,
	0, 8, 27, MACH_ODIN, CPU_PLL_CLK, 0, SECURE, &clk_out_lock);

	cpu_crg_clk[cclk_src_sel_clk] = odin_clk_register_mux(NULL, "cclk_src_sel_clk",
	cclk_src_sels_name, ARRAY_SIZE(cclk_src_sels_name), CLK_SET_RATE_PARENT, cpu_base+0x114,
	p_cpu_base+0x114, 16, 2, 0, SECURE, &clk_out_lock);

	cpu_crg_clk[cclk_mask_clk] = odin_clk_register_gate(NULL, "cclk_mask_clk", "cclk_src_sel_clk",
	CLK_SET_RATE_PARENT, cpu_base+0x148, p_cpu_base+0x148, 0, 0, SECURE, &clk_out_lock);

	cpu_crg_clk[cclk_div_clk] = odin_clk_register_div(NULL, "cclk_div_clk", "cclk_mask_clk", 0,
	cpu_base+0x114, cpu_base+0x114, p_cpu_base+0x114, p_cpu_base+0x114, 0, 4, 24, MACH_ODIN,
	COMM_PLL_CLK, 0, SECURE, &clk_out_lock);

	cpu_crg_clk[hclk_src_sel_clk] = odin_clk_register_mux(NULL, "hclk_src_sel_clk",
	hclk_src_sels_name, ARRAY_SIZE(hclk_src_sels_name), CLK_SET_RATE_PARENT, cpu_base+0x114,
	p_cpu_base+0x114, 18, 2, 0, SECURE, &clk_out_lock);

	cpu_crg_clk[hclk_mask_clk] = odin_clk_register_gate(NULL, "hclk_mask_clk", "hclk_src_sel_clk",
	CLK_SET_RATE_PARENT, cpu_base+0x148, p_cpu_base+0x148, 1, 0, SECURE, &clk_out_lock);

	cpu_crg_clk[hclk_div_clk] = odin_clk_register_div(NULL, "hclk_div_clk", "hclk_mask_clk", 0,
	cpu_base+0x114, cpu_base+0x114, p_cpu_base+0x114, p_cpu_base+0x114, 4, 4, 25, MACH_ODIN,
	COMM_PLL_CLK, 0, SECURE, &clk_out_lock);

	cpu_crg_clk[clk_cpucnt_src_sel_clk] = odin_clk_register_mux(NULL, "clk_cpucnt_src_sel_clk",
	clk_cpucnt_src_sels_name, ARRAY_SIZE(clk_cpucnt_src_sels_name), CLK_SET_RATE_PARENT,
	cpu_base+0x114, p_cpu_base+0x114, 20, 2, 0, SECURE, &clk_out_lock);

	cpu_crg_clk[clk_cpucnt_mask_clk] = odin_clk_register_gate(NULL, "clk_cpucnt_mask_clk",
	"clk_cpucnt_src_sel_clk", CLK_SET_RATE_PARENT, cpu_base+0x148, p_cpu_base+0x148, 2, 0,
	SECURE, &clk_out_lock);

	cpu_crg_clk[clk_cpucnt_div_clk] = odin_clk_register_div(NULL, "clk_cpucnt_div_clk",
	"clk_cpucnt_mask_clk", 0, cpu_base+0x114, cpu_base+0x114, p_cpu_base+0x114, p_cpu_base+0x114,
	8, 4, 26, MACH_ODIN, COMM_PLL_CLK, 0, SECURE, &clk_out_lock);

	cpu_crg_clk[pclk_sapb_src_sel_clk] = odin_clk_register_mux(NULL, "pclk_sapb_src_sel_clk",
	pclk_sapb_src_sels_name, ARRAY_SIZE(pclk_sapb_src_sels_name), CLK_SET_RATE_PARENT,
	cpu_base+0x114, p_cpu_base+0x114, 22, 2, 0, SECURE, &clk_out_lock);

	cpu_crg_clk[pclk_sapb_mask_clk] = odin_clk_register_gate(NULL, "pclk_sapb_mask_clk",
	"pclk_sapb_src_sel_clk", CLK_SET_RATE_PARENT, cpu_base+0x148, p_cpu_base+0x148, 3, 0,
	SECURE, &clk_out_lock);

	cpu_crg_clk[pclk_sapb_div_clk] = odin_clk_register_div(NULL, "pclk_sapb_div_clk",
	"pclk_sapb_mask_clk", 0, cpu_base+0x114, cpu_base+0x114, p_cpu_base+0x114, p_cpu_base+0x114,
	12, 4, 27, MACH_ODIN, COMM_PLL_CLK, 0, SECURE, &clk_out_lock);

	cpu_crg_clk[gclk_src_sel_clk] = odin_clk_register_mux(NULL, "gclk_src_sel_clk",
	gclk_src_sels_name, ARRAY_SIZE(gclk_src_sels_name), CLK_SET_RATE_PARENT,
	cpu_base+0x118, p_cpu_base+0x118, 16, 2, 0, SECURE, &clk_out_lock);

	cpu_crg_clk[gclk_mask_clk] = odin_clk_register_gate(NULL, "gclk_mask_clk",
	"gclk_src_sel_clk", CLK_SET_RATE_PARENT, cpu_base+0x148, p_cpu_base+0x148, 4, 0,
	SECURE, &clk_out_lock);

	cpu_crg_clk[gclk_div_clk] = odin_clk_register_div(NULL, "gclk_div_clk",
	"gclk_mask_clk", 0, cpu_base+0x118, cpu_base+0x118, p_cpu_base+0x118, p_cpu_base+0x118,
	0, 4, 24, MACH_ODIN, COMM_PLL_CLK, 0, SECURE, &clk_out_lock);

	cpu_crg_clk[atclk_src_sel_clk] = odin_clk_register_mux(NULL, "atclk_src_sel_clk",
	atclk_src_sels_name, ARRAY_SIZE(atclk_src_sels_name), CLK_SET_RATE_PARENT,
	cpu_base+0x118, p_cpu_base+0x118, 18, 2, 0, SECURE, &clk_out_lock);

	cpu_crg_clk[atclk_mask_clk] = odin_clk_register_gate(NULL, "atclk_mask_clk",
	"atclk_src_sel_clk", CLK_SET_RATE_PARENT, cpu_base+0x148, p_cpu_base+0x148, 5, 0,
	SECURE, &clk_out_lock);

	cpu_crg_clk[atclk_div_clk] = odin_clk_register_div(NULL, "atclk_div_clk",
	"atclk_mask_clk", 0, cpu_base+0x118, cpu_base+0x118, p_cpu_base+0x118, p_cpu_base+0x118,
	4, 4, 25, MACH_ODIN, COMM_PLL_CLK, 0, SECURE, &clk_out_lock);

	cpu_crg_clk[trace_src_sel_clk] = odin_clk_register_mux(NULL, "trace_src_sel_clk",
	trace_src_sels_name, ARRAY_SIZE(trace_src_sels_name), CLK_SET_RATE_PARENT,
	cpu_base+0x118, p_cpu_base+0x118, 20, 2, 0, SECURE, &clk_out_lock);

	cpu_crg_clk[trace_mask_clk] = odin_clk_register_gate(NULL, "trace_mask_clk",
	"trace_src_sel_clk", CLK_SET_RATE_PARENT, cpu_base+0x148, p_cpu_base+0x148, 6, 0,
	SECURE, &clk_out_lock);

	cpu_crg_clk[trace_div_clk] = odin_clk_register_div(NULL, "trace_div_clk",
	"trace_mask_clk", 0, cpu_base+0x118, cpu_base+0x118, p_cpu_base+0x118, p_cpu_base+0x118,
	8, 4, 26, MACH_ODIN, COMM_PLL_CLK, 0, SECURE, &clk_out_lock);

	odin_dt_clk_init(cpu_crg_clk, atclk_div_clk, cpu_base, p_cpu_base, CPU_BLOCK);
}

static const char *ics_src_sels_name[] = {"osc_clk", "npll_clk", "mem_pll_clk"};
static const char *hsic_src_sels_name[] = {"osc_clk", "ics_hpll_clk", "mem_pll_clk"};

enum ics_crgclk{
	ics_hpll_clk,
	ics_src_sel_clk,
	ics_mask_clk,
	ics_div_clk,
	ics_gate_clk,
	ics_cclk_div_clk,
	usbhsic_pmu_div_clk,
	usb30_suspend_div_clk,
	usb30_ref_div_clk,
	hsi_clk_div_clk,
	lli_clksvc_mask_clk,
	lli_clksvc_div_clk,
	usbhsic_ref_mask_clk,
	usbhsic_ref_div_clk,
	hsic_src_sel_clk,
	hsic_mask_clk,
	hsic_div_clk,
	ics_clk,
	hsi_clk_ahb,
	usbhsic_bus_clk,
	cclk_usb30,
	c2c_ocp_clken,
	c2c_lli_noc_clk,
	lli_clk,
	ufs_clk,
	usbh_p_hclk,
	usb30_susp_clk,
	usb30_ref_hclk,
	hsi_tx_base_clk,
	c2c_func_clk,
	lli_clksvc,
	ufs_clksvc,
	lliphy_ref_clk,
	usb30phy_ra_p,
	usbhsic_refdig,
	usbhsic_phy,

	ics_crgclk_cnt
};

char *ics_crgclk_names[] = {
	"ics_hpll_clk",
	"ics_src_sel_clk",
	"ics_mask_clk",
	"ics_div_clk",
	"ics_gate_clk",
	"ics_cclk_div_clk",
	"usbhsic_pmu_div_clk",
	"usb30_suspend_div_clk",
	"usb30_ref_div_clk",
	"hsi_clk_div_clk",
	"lli_clksvc_mask_clk",
	"lli_clksvc_div_clk",
	"usbhsic_ref_mask_clk",
	"usbhsic_ref_div_clk",
	"hsic_src_sel_clk",
	"hsic_mask_clk",
	"hsic_div_clk",
	"ics_clk",
	"hsi_clk_ahb",
	"usbhsic_bus_clk",
	"cclk_usb30",
	"c2c_ocp_clken",
	"c2c_lli_noc_clk",
	"lli_clk",
	"ufs_clk",
	"usbh_p_hclk",
	"usb30_susp_clk",
	"usb30_ref_hclk",
	"hsi_tx_base_clk",
	"c2c_func_clk",
	"lli_clksvc",
	"ufs_clksvc",
	"lliphy_ref_clk",
	"usb30phy_ra_p",
	"usbhsic_refdig",
	"usbhsic_phy",

	"ics_crgclk_cnt"
};

static struct clk *ics_crg_clk[ics_crgclk_cnt];

void ics_crg_reg(void __iomem *ics_base, u32 p_ics_base)
{
	ics_crg_clk[ics_hpll_clk] = odin_clk_register_ljpll(NULL, "ics_hpll_clk", "osc_clk", 0,
	ics_base, ics_base, ics_base+0x8, p_ics_base, p_ics_base, p_ics_base+0x8, 20, 0,
	MACH_ODIN, ICS_PLL_CLK, NON_SECURE, &clk_out_lock);

	ics_crg_clk[ics_src_sel_clk] = odin_clk_register_mux(NULL, "ics_src_sel_clk", ics_src_sels_name,
	ARRAY_SIZE(ics_src_sels_name), CLK_SET_RATE_PARENT, ics_base+0x10, p_ics_base+0x10,
	0, 2, 0, NON_SECURE, &clk_out_lock);

	//may not be used
	ics_crg_clk[ics_mask_clk] = odin_clk_register_gate(NULL, "ics_mask_clk", "ics_src_sel_clk",
	CLK_SET_RATE_PARENT, ics_base+0x14, p_ics_base+0x14, 0, 0, NON_SECURE, &clk_out_lock);

	ics_crg_clk[ics_div_clk] = odin_clk_register_div(NULL, "ics_div_clk", "ics_mask_clk",
	0, ics_base+0x18, NULL, p_ics_base+0x18, 0x0, 0, 8, 0, MACH_ODIN,
	ICS_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	//may not be used
	ics_crg_clk[ics_gate_clk] = odin_clk_register_gate(NULL, "ics_gate_clk", "ics_div_clk",
	CLK_SET_RATE_PARENT, ics_base+0x1C, p_ics_base+0x1C, 0, 0, NON_SECURE, &clk_out_lock);

	ics_crg_clk[ics_cclk_div_clk] = odin_clk_register_div(NULL, "ics_cclk_div_clk", "ics_gate_clk",
	0, ics_base+0x30, NULL, p_ics_base+0x30, 0x0, 0, 8, 0, MACH_ODIN,
	ICS_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	ics_crg_clk[usbhsic_pmu_div_clk] = odin_clk_register_div(NULL, "usbhsic_pmu_div_clk",
	"ics_cclk_div_clk", 0, ics_base+0x54, NULL, p_ics_base+0x54, 0x0, 0, 8, 0,
	MACH_ODIN, ICS_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	ics_crg_clk[usb30_suspend_div_clk] = odin_clk_register_div(NULL, "usb30_suspend_div_clk",
	"ics_cclk_div_clk", 0, ics_base+0x5C, NULL, p_ics_base+0x5C, 0x0, 0, 8, 0,
	MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	ics_crg_clk[usb30_ref_div_clk] = odin_clk_register_div(NULL, "usb30_ref_div_clk",
	"ics_cclk_div_clk", 0, ics_base+0x64, NULL, p_ics_base+0x64, 0x0, 0, 8, 0,
	MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	ics_crg_clk[hsi_clk_div_clk] = odin_clk_register_div(NULL, "hsi_clk_div_clk", "ics_gate_clk",
	0, ics_base+0x6C, NULL, p_ics_base+0x6C, 0x0, 0, 8, 0, MACH_ODIN,
	COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	ics_crg_clk[lli_clksvc_mask_clk] = odin_clk_register_gate(NULL, "lli_clksvc_mask_clk",
	"osc_clk", CLK_SET_RATE_PARENT, ics_base+0x78, p_ics_base+0x78, 0, 0, NON_SECURE, &clk_out_lock);

	ics_crg_clk[lli_clksvc_div_clk] = odin_clk_register_div(NULL, "lli_clksvc_div_clk",
	"lli_clksvc_mask_clk", 0, ics_base+0x7C, NULL, p_ics_base+0x7C, 0x0,
	0, 8, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	ics_crg_clk[usbhsic_ref_mask_clk] = odin_clk_register_gate(NULL, "usbhsic_ref_mask_clk",
	"osc_clk", CLK_SET_RATE_PARENT, ics_base+0x90, p_ics_base+0x90, 0, 0, NON_SECURE,
	&clk_out_lock);

	ics_crg_clk[usbhsic_ref_div_clk] = odin_clk_register_div(NULL, "usbhsic_ref_div_clk",
	"usbhsic_ref_mask_clk", 0, ics_base+0x94, NULL, p_ics_base+0x94, 0x0,
	0, 8, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	ics_crg_clk[hsic_src_sel_clk] = odin_clk_register_mux(NULL, "hsic_src_sel_clk",
	hsic_src_sels_name, ARRAY_SIZE(hsic_src_sels_name), CLK_SET_RATE_PARENT, ics_base+0x20,
	p_ics_base+0x20, 0, 2, 0, NON_SECURE, &clk_out_lock);

	ics_crg_clk[hsic_mask_clk] = odin_clk_register_gate(NULL, "hsic_mask_clk", "hsic_src_sel_clk",
	CLK_SET_RATE_PARENT, ics_base+0x24, p_ics_base+0x24, 0, 0, NON_SECURE, &clk_out_lock);

	ics_crg_clk[hsic_div_clk] = odin_clk_register_div(NULL, "hsic_div_clk", "hsic_mask_clk",
	0, ics_base+0x28, NULL, p_ics_base+0x28, 0x0, 0, 8, 0, MACH_ODIN,
	COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	odin_dt_clk_init(ics_crg_clk, hsic_div_clk, ics_base, p_ics_base, ICS_BLOCK);
}

static const char *mipicfg_pll_sels_name[] = {"osc_clk", "isp_pll_clk"};
static const char *mipicfg_src_sels_name[] = {"mipicfg_pll_sel_clk", "npll_clk"};
static const char *axi_pll_sels_name[] = {"osc_clk", "isp_pll_clk"};
static const char *axi_src_sels_name[] = {"axi_pll_sel_clk", "npll_clk"};
static const char *isp_pll_sels_name[] = {"osc_clk", "isp_pll_clk"};
static const char *isp_src_sels_name[] = {"isp_pll_sel_clk", "npll_clk"};
static const char *pix0_isp_sels_name[] = {"isp0_clk", "isp0_clkx2"};
static const char *pix0_src_sels_name[] = {"pix0_isp_sel_clk", "fg_clk"};
static const char *pix1_isp_sels_name[] = {"isp1_clk", "isp0_clkx2"};
static const char *pix1_src_sels_name[] = {"pix1_isp_sel_clk", "fg_clk"};
static const char *vnr_pll_sels_name[] = {"osc_clk", "isp_pll_clk"};
static const char *vnr_src_sels_name[] = {"vnr_pll_sel_clk", "npll_clk"};
static const char *fd_pll_sels_name[] = {"osc_clk", "isp_pll_clk"};
static const char *fd_src_sels_name[] = {"fd_pll_sel_clk", "npll_clk"};

enum css_crgclk{
	mipicfg_pll_sel_clk,
	mipicfg_src_sel_clk,
	mipicfg_mask_clk,
	mipicfg_div_clk,
	axi_pll_sel_clk,
	axi_src_sel_clk,
	axi_mask_clk,
	axi_div_clk,
	fg_clk,
	isp_pll_sel_clk,
	isp_src_sel_clk,
	isp_mask_clk,
	isp_div_clk,
	isp0_clkx2,
	isp_div_clkx2,
	isp0_clk,
	isp1_clk,
	pix0_isp_sel_clk,
	pix0_src_sel_clk,
	pix1_isp_sel_clk,
	pix1_src_sel_clk,
	vnr_pll_sel_clk,
	vnr_src_sel_clk,
	vnr_mask_clk,
	vnr_div_clk,
	fd_pll_sel_clk,
	fd_src_sel_clk,
	fd_mask_clk,
	fd_div_clk,
	mipi_cfg0_clk,
	mipi_cfg1_clk,
	mipi_cfg2_clk,
	axi_clk,
	axi_clk_r,
	s3c_clk,
	s3c0_clk,
	s3c1_clk,
	v_fg_clk,
	ahb_clk,
	ahb_clk_r,
	apb_clk,
	tune_clkx2,
	v_isp0_clkx2,
	v_isp0_clk,
	v_isp1_clk,
	tune_clk,
	mdis_clk,
	pix0_clk,
	pix1_clk,
	vnr_clk,
	fd_clk,
	css_mipi0_clk,
	css_mipi1_clk,
	css_mipi2_clk,

	css_crgclk_cnt
};

char *css_crgclk_names[] = {
	"mipicfg_pll_sel_clk",
	"mipicfg_src_sel_clk",
	"mipicfg_mask_clk",
	"mipicfg_div_clk",
	"axi_pll_sel_clk",
	"axi_src_sel_clk",
	"axi_mask_clk",
	"axi_div_clk",
	"fg_clk",
	"isp_pll_sel_clk",
	"isp_src_sel_clk",
	"isp_mask_clk",
	"isp_div_clk",
	"isp0_clkx2",
	"isp_div_clkx2",
	"isp0_clk",
	"isp1_clk",
	"pix0_isp_sel_clk",
	"pix0_src_sel_clk",
	"pix1_isp_sel_clk",
	"pix1_src_sel_clk",
	"vnr_pll_sel_clk",
	"vnr_src_sel_clk",
	"vnr_mask_clk",
	"vnr_div_clk",
	"fd_pll_sel_clk",
	"fd_src_sel_clk",
	"fd_mask_clk",
	"fd_div_clk",
	"mipi_cfg0_clk",
	"mipi_cfg1_clk",
	"mipi_cfg2_clk",
	"axi_clk",
	"axi_clk_r",
	"s3c_clk",
	"s3c0_clk",
	"s3c1_clk",
	"v_fg_clk",
	"ahb_clk",
	"ahb_clk_r",
	"apb_clk",
	"tune_clkx2",
	"v_isp0_clkx2",
	"v_isp0_clk",
	"v_isp1_clk",
	"tune_clk",
	"mdis_clk",
	"pix0_clk",
	"pix1_clk",
	"vnr_clk",
	"fd_clk",
	"css_mipi0_clk",
	"css_mipi1_clk",
	"css_mipi2_clk",

	"css_crgclk_cnt"
};

static struct clk *css_crg_clk[css_crgclk_cnt];

void css_crg_reg(void __iomem *css_base, u32 p_css_base)
{
	css_crg_clk[mipicfg_pll_sel_clk] = odin_clk_register_mux(NULL, "mipicfg_pll_sel_clk",
	mipicfg_pll_sels_name, ARRAY_SIZE(mipicfg_pll_sels_name), CLK_SET_RATE_PARENT, css_base+0x8,
	p_css_base+0x8, 16, 1, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[mipicfg_src_sel_clk] = odin_clk_register_mux(NULL, "mipicfg_src_sel_clk",
	mipicfg_src_sels_name, ARRAY_SIZE(mipicfg_src_sels_name), CLK_SET_RATE_PARENT, css_base+0x8,
	p_css_base+0x8, 17, 1, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[mipicfg_mask_clk] = odin_clk_register_gate(NULL, "mipicfg_mask_clk",
	"mipicfg_src_sel_clk", CLK_SET_RATE_PARENT, css_base+0x8, p_css_base+0x8,
	18, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[mipicfg_div_clk] = odin_clk_register_div(NULL, "mipicfg_div_clk",
	"mipicfg_mask_clk", 0, css_base+0x8, NULL, p_css_base+0x8,
	0x0, 20, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[axi_pll_sel_clk] = odin_clk_register_mux(NULL, "axi_pll_sel_clk",
	axi_pll_sels_name, ARRAY_SIZE(axi_pll_sels_name), CLK_SET_RATE_PARENT, css_base+0xC,
	p_css_base+0xC, 0, 1, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[axi_src_sel_clk] = odin_clk_register_mux(NULL, "axi_src_sel_clk",
	axi_src_sels_name, ARRAY_SIZE(axi_src_sels_name), CLK_SET_RATE_PARENT, css_base+0xC,
	p_css_base+0xC, 1, 1, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[axi_mask_clk] = odin_clk_register_gate(NULL, "axi_mask_clk", "axi_src_sel_clk",
	CLK_SET_RATE_PARENT, css_base+0xC, p_css_base+0xC,
	2, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[axi_div_clk] = odin_clk_register_div(NULL, "axi_div_clk", "axi_mask_clk",
	0, css_base+0xC, NULL, p_css_base+0xC,
	0x0, 4, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[fg_clk] = odin_clk_register_gate(NULL, "fg_clk", "axi_div_clk",
	CLK_SET_RATE_PARENT, css_base+0x4, p_css_base+0x4,
	21, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[isp_pll_sel_clk] = odin_clk_register_mux(NULL, "isp_pll_sel_clk",
	isp_pll_sels_name, ARRAY_SIZE(isp_pll_sels_name), CLK_SET_RATE_PARENT, css_base+0x14,
	p_css_base+0x14, 0, 1, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[isp_src_sel_clk] = odin_clk_register_mux(NULL, "isp_src_sel_clk",
	isp_src_sels_name, ARRAY_SIZE(isp_src_sels_name), CLK_SET_RATE_PARENT, css_base+0x14,
	p_css_base+0x14, 1, 1, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[isp_mask_clk] = odin_clk_register_gate(NULL, "isp_mask_clk", "isp_src_sel_clk",
	CLK_SET_RATE_PARENT, css_base+0x14, p_css_base+0x14, 2, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[isp_div_clk] = odin_clk_register_div(NULL, "isp_div_clk", "isp_mask_clk",
	0, css_base+0x14, NULL, p_css_base+0x14, 0x0, 4, 4, 0, MACH_ODIN,
	COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[isp0_clkx2] = odin_clk_register_gate(NULL, "isp0_clkx2", "isp_div_clk",
	CLK_SET_RATE_PARENT, css_base+0x4, p_css_base+0x4, 22, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[isp_div_clkx2] = odin_clk_register_div(NULL, "isp_div_clkx2", "isp_div_clk",
	0, css_base+0x14, NULL, p_css_base+0x14,
	0x0, 8, 1, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[isp0_clk] = odin_clk_register_gate(NULL, "isp0_clk", "isp_div_clkx2",
	CLK_SET_RATE_PARENT, css_base+0x4, p_css_base+0x4,
	17, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[isp1_clk] = odin_clk_register_gate(NULL, "isp1_clk", "isp_div_clkx2",
	CLK_SET_RATE_PARENT, css_base+0x4, p_css_base+0x4,
	18, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[pix0_isp_sel_clk] = odin_clk_register_mux(NULL, "pix0_isp_sel_clk",
	pix0_isp_sels_name, ARRAY_SIZE(pix0_isp_sels_name), CLK_SET_RATE_PARENT, css_base+0x28,
	p_css_base+0x28, 0, 1, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[pix0_src_sel_clk] = odin_clk_register_mux(NULL, "pix0_src_sel_clk",
	pix0_src_sels_name, ARRAY_SIZE(pix0_src_sels_name), CLK_SET_RATE_PARENT, css_base+0x28,
	p_css_base+0x28, 1, 1, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[pix1_isp_sel_clk] = odin_clk_register_mux(NULL, "pix1_isp_sel_clk",
	pix1_isp_sels_name, ARRAY_SIZE(pix1_isp_sels_name), CLK_SET_RATE_PARENT, css_base+0x28,
	p_css_base+0x28, 16, 1, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[pix1_src_sel_clk] = odin_clk_register_mux(NULL, "pix1_src_sel_clk",
	pix1_src_sels_name, ARRAY_SIZE(pix1_src_sels_name), CLK_SET_RATE_PARENT, css_base+0x28,
	p_css_base+0x28, 17, 1, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[vnr_pll_sel_clk] = odin_clk_register_mux(NULL, "vnr_pll_sel_clk",
	vnr_pll_sels_name, ARRAY_SIZE(vnr_pll_sels_name), CLK_SET_RATE_PARENT, css_base+0x18,
	p_css_base+0x18, 0, 1, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[vnr_src_sel_clk] = odin_clk_register_mux(NULL, "vnr_src_sel_clk",
	vnr_src_sels_name, ARRAY_SIZE(vnr_src_sels_name), CLK_SET_RATE_PARENT, css_base+0x18,
	p_css_base+0x18, 1, 1, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[vnr_mask_clk] = odin_clk_register_gate(NULL, "vnr_mask_clk", "vnr_src_sel_clk",
	CLK_SET_RATE_PARENT, css_base+0x18, p_css_base+0x18, 2, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[vnr_div_clk] = odin_clk_register_div(NULL, "vnr_div_clk", "vnr_mask_clk",
	0, css_base+0x18, NULL, p_css_base+0x18,
	0x0, 4, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[fd_pll_sel_clk] = odin_clk_register_mux(NULL, "fd_pll_sel_clk", fd_pll_sels_name,
	ARRAY_SIZE(fd_pll_sels_name), CLK_SET_RATE_PARENT, css_base+0x24, p_css_base+0x24,
	0, 1, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[fd_src_sel_clk] = odin_clk_register_mux(NULL, "fd_src_sel_clk", fd_src_sels_name,
	ARRAY_SIZE(fd_src_sels_name), CLK_SET_RATE_PARENT, css_base+0x24, p_css_base+0x24, 1, 1, 0,
	NON_SECURE, &clk_out_lock);

	css_crg_clk[fd_mask_clk] = odin_clk_register_gate(NULL, "fd_mask_clk", "fd_src_sel_clk",
	CLK_SET_RATE_PARENT, css_base+0x24, p_css_base+0x24, 2, 0, NON_SECURE, &clk_out_lock);

	css_crg_clk[fd_div_clk] = odin_clk_register_div(NULL, "fd_div_clk", "fd_mask_clk",
	0, css_base+0x24, NULL, p_css_base+0x24, 0x0, 4, 4, 0, MACH_ODIN,
	COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	odin_dt_clk_init(css_crg_clk, fd_div_clk, css_base, p_css_base, CSS_BLOCK);
}


static const char *pnoc_a_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};
static const char *wt_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};
static const char *sdc_sclk_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};
static const char *sdio_sclk0_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};
static const char *sdio_sclk1_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};
static const char *emmc_sclk_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};
static const char *e2nand_sclk_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};
static const char *i2c_sclk0_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};
static const char *i2c_sclk1_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};
static const char *i2c_sclk2_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};
static const char *i2c_sclk3_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};
static const char *spi_sclk0_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};
static const char *uart_sclk0_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};
static const char *uart_sclk1_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};
static const char *uart_sclk2_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};
static const char *uart_sclk3_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};
static const char *uart_sclk4_src_sels_name[] = {"osc_clk", "npll_clk", "ics_hpll_clk"};

enum peri_crgclk{
	pnoc_a_src_sel_clk,
	pnoc_a_div_clk,
	pclk_div_clk,
	wt_src_sel_clk,
	wt_mask_clk,
	wt_div_clk,
	sdc_sclk_src_sel_clk,
	sdc_sclk_mask_clk,
	sdc_sclk_div_clk,
	sdio_sclk0_src_sel_clk,
	sdio_sclk0_mask_clk,
	sdio_sclk0_div_clk,
	sdio_sclk1_src_sel_clk,
	sdio_sclk1_mask_clk,
	sdio_sclk1_div_clk,
	emmc_sclk_src_sel_clk,
	emmc_sclk_mask_clk,
	emmc_sclk_div_clk,
	e2nand_sclk_src_sel_clk,
	e2nand_sclk_mask_clk,
	e2nand_sclk_div_clk,
	i2c_sclk0_src_sel_clk,
	i2c_sclk0_mask_clk,
	i2c_sclk0_div_clk,
	i2c_sclk1_src_sel_clk,
	i2c_sclk1_mask_clk,
	i2c_sclk1_div_clk,
	i2c_sclk2_src_sel_clk,
	i2c_sclk2_mask_clk,
	i2c_sclk2_div_clk,
	i2c_sclk3_src_sel_clk,
	i2c_sclk3_mask_clk,
	i2c_sclk3_div_clk,
	spi_sclk0_src_sel_clk,
	spi_sclk0_mask_clk,
	spi_sclk0_div_clk,
	uart_sclk0_src_sel_clk,
	uart_sclk0_mask_clk,
	uart_sclk0_div_clk,
	uart_sclk1_src_sel_clk,
	uart_sclk1_mask_clk,
	uart_sclk1_div_clk,
	uart_sclk2_src_sel_clk,
	uart_sclk2_mask_clk,
	uart_sclk2_div_clk,
	uart_sclk3_src_sel_clk,
	uart_sclk3_mask_clk,
	uart_sclk3_div_clk,
	uart_sclk4_src_sel_clk,
	uart_sclk4_mask_clk,
	uart_sclk4_div_clk,
	pwm_pclk,
	wdt_pclk,
	tmr_pclk0,
	tmr_pclk1,
	g0_gpio_pclk,
	g1_gpio_pclk,
	sram_hclk,
	pdma_hclk,
	sdc_aclk,
	sdio_aclk0,
	sdio_aclk1,
	emmc_aclk,
	e2nand_aclk,
	i2c_pclk0,
	i2c_pclk1,
	i2c_pclk2,
	i2c_pclk3,
	spi_pclk0,
	uart_pclk0,
	uart_pclk1,
	uart_pclk2,
	uart_pclk3,
	uart_pclk4,
	sdc_sclk,
	sdio_sclk0,
	sdio_sclk1,
	emmc_sclk,
	e2nand_sclk,
	i2c_sclk0,
	i2c_sclk1,
	i2c_sclk2,
	i2c_sclk3,
	spi_sclk0,
	uart_sclk0,
	uart_sclk1,
	uart_sclk2,
	uart_sclk3,
	uart_sclk4,

	peri_crgclk_cnt
};

char *peri_crgclk_names[] = {
	"pnoc_a_src_sel_clk",
	"pnoc_a_div_clk",
	"pclk_div_clk",
	"wt_src_sel_clk",
	"wt_mask_clk",
	"wt_div_clk",
	"sdc_sclk_src_sel_clk",
	"sdc_sclk_mask_clk",
	"sdc_sclk_div_clk",
	"sdio_sclk0_src_sel_clk",
	"sdio_sclk0_mask_clk",
	"sdio_sclk0_div_clk",
	"sdio_sclk1_src_sel_clk",
	"sdio_sclk1_mask_clk",
	"sdio_sclk1_div_clk",
	"emmc_sclk_src_sel_clk",
	"emmc_sclk_mask_clk",
	"emmc_sclk_div_clk",
	"e2nand_sclk_src_sel_clk",
	"e2nand_sclk_mask_clk",
	"e2nand_sclk_div_clk",
	"i2c_sclk0_src_sel_clk",
	"i2c_sclk0_mask_clk",
	"i2c_sclk0_div_clk",
	"i2c_sclk1_src_sel_clk",
	"i2c_sclk1_mask_clk",
	"i2c_sclk1_div_clk",
	"i2c_sclk2_src_sel_clk",
	"i2c_sclk2_mask_clk",
	"i2c_sclk2_div_clk",
	"i2c_sclk3_src_sel_clk",
	"i2c_sclk3_mask_clk",
	"i2c_sclk3_div_clk",
	"spi_sclk0_src_sel_clk",
	"spi_sclk0_mask_clk",
	"spi_sclk0_div_clk",
	"uart_sclk0_src_sel_clk",
	"uart_sclk0_mask_clk",
	"uart_sclk0_div_clk",
	"uart_sclk1_src_sel_clk",
	"uart_sclk1_mask_clk",
	"uart_sclk1_div_clk",
	"uart_sclk2_src_sel_clk",
	"uart_sclk2_mask_clk",
	"uart_sclk2_div_clk",
	"uart_sclk3_src_sel_clk",
	"uart_sclk3_mask_clk",
	"uart_sclk3_div_clk",
	"uart_sclk4_src_sel_clk",
	"uart_sclk4_mask_clk",
	"uart_sclk4_div_clk",
	"pwm_pclk",
	"wdt_pclk",
	"tmr_pclk0",
	"tmr_pclk1",
	"g0_gpio_pclk",
	"g1_gpio_pclk",
	"sram_hclk",
	"pdma_hclk",
	"sdc_aclk",
	"sdio_aclk0",
	"sdio_aclk1",
	"emmc_aclk",
	"e2nand_aclk",
	"i2c_pclk0",
	"i2c_pclk1",
	"i2c_pclk2",
	"i2c_pclk3",
	"spi_pclk0",
	"uart_pclk0",
	"uart_pclk1",
	"uart_pclk2",
	"uart_pclk3",
	"uart_pclk4",
	"sdc_sclk",
	"sdio_sclk0",
	"sdio_sclk1",
	"emmc_sclk",
	"e2nand_sclk",
	"i2c_sclk0",
	"i2c_sclk1",
	"i2c_sclk2",
	"i2c_sclk3",
	"spi_sclk0",
	"uart_sclk0",
	"uart_sclk1",
	"uart_sclk2",
	"uart_sclk3",
	"uart_sclk4",


	"peri_crgclk_cnt"
};

static struct clk *peri_crg_clk[peri_crgclk_cnt];

void peri_crg_reg(void __iomem *peri_base, u32 p_peri_base)
{
	peri_crg_clk[pnoc_a_src_sel_clk] = odin_clk_register_mux(NULL, "pnoc_a_src_sel_clk",
	pnoc_a_src_sels_name, ARRAY_SIZE(pnoc_a_src_sels_name), CLK_SET_RATE_PARENT, peri_base,
	p_peri_base, 4, 2, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[pnoc_a_div_clk] = odin_clk_register_div(NULL, "pnoc_a_div_clk",
	"pnoc_a_src_sel_clk", 0, peri_base, NULL, p_peri_base, 0x0, 8, 6, 0,
	MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[pclk_div_clk] = odin_clk_register_div(NULL, "pclk_div_clk", "pnoc_a_src_sel_clk",
	0, peri_base+0x4, NULL, p_peri_base+0x4, 0x0, 8, 6, 0, MACH_ODIN,
	COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[wt_src_sel_clk] = odin_clk_register_mux(NULL, "wt_src_sel_clk", wt_src_sels_name,
	ARRAY_SIZE(wt_src_sels_name), CLK_SET_RATE_PARENT, peri_base+0x8, p_peri_base+0x8, 4, 2, 0,
	NON_SECURE, &clk_out_lock);

	peri_crg_clk[wt_mask_clk] = odin_clk_register_gate(NULL, "wt_mask_clk", "wt_src_sel_clk",
	CLK_SET_RATE_PARENT, peri_base+0x8, p_peri_base+0x8, 1, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[wt_div_clk] = odin_clk_register_div(NULL, "wt_div_clk", "wt_mask_clk",
	0, peri_base+0x8, NULL, p_peri_base+0x8, 0x0, 8, 6, 0, MACH_ODIN,
	COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[sdc_sclk_src_sel_clk] = odin_clk_register_mux(NULL, "sdc_sclk_src_sel_clk",
	sdc_sclk_src_sels_name, ARRAY_SIZE(sdc_sclk_src_sels_name), CLK_SET_RATE_PARENT,
	peri_base+0x6C, p_peri_base+0x6C, 4, 2, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[sdc_sclk_mask_clk] = odin_clk_register_gate(NULL, "sdc_sclk_mask_clk",
	"sdc_sclk_src_sel_clk", CLK_SET_RATE_PARENT, peri_base+0x6C, p_peri_base+0x6C, 1, 0,
	NON_SECURE, &clk_out_lock);

	peri_crg_clk[sdc_sclk_div_clk] = odin_clk_register_div(NULL, "sdc_sclk_div_clk",
	"sdc_sclk_mask_clk", 0, peri_base+0x6C, NULL, p_peri_base+0x6C, 0x0,
	8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[sdio_sclk0_src_sel_clk] = odin_clk_register_mux(NULL, "sdio_sclk0_src_sel_clk",
	sdio_sclk0_src_sels_name, ARRAY_SIZE(sdio_sclk0_src_sels_name), CLK_SET_RATE_PARENT,
	peri_base+0x70, p_peri_base+0x70, 4, 2, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[sdio_sclk0_mask_clk] = odin_clk_register_gate(NULL, "sdio_sclk0_mask_clk",
	"sdio_sclk0_src_sel_clk", CLK_SET_RATE_PARENT, peri_base+0x70, p_peri_base+0x70, 1, 0,
	NON_SECURE, &clk_out_lock);

	peri_crg_clk[sdio_sclk0_div_clk] = odin_clk_register_div(NULL, "sdio_sclk0_div_clk",
	"sdio_sclk0_mask_clk", 0, peri_base+0x70, NULL, p_peri_base+0x70, 0x0,
	8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[sdio_sclk1_src_sel_clk] = odin_clk_register_mux(NULL, "sdio_sclk1_src_sel_clk",
	sdio_sclk1_src_sels_name, ARRAY_SIZE(sdio_sclk1_src_sels_name), CLK_SET_RATE_PARENT,
	peri_base+0x74, p_peri_base+0x74, 4, 2, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[sdio_sclk1_mask_clk] = odin_clk_register_gate(NULL, "sdio_sclk1_mask_clk",
	"sdio_sclk1_src_sel_clk", CLK_SET_RATE_PARENT, peri_base+0x74, p_peri_base+0x74,
	1, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[sdio_sclk1_div_clk] = odin_clk_register_div(NULL, "sdio_sclk1_div_clk",
	"sdio_sclk1_mask_clk", 0, peri_base+0x74, NULL, p_peri_base+0x74,
	0x0, 8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[emmc_sclk_src_sel_clk] = odin_clk_register_mux(NULL, "emmc_sclk_src_sel_clk",
	emmc_sclk_src_sels_name, ARRAY_SIZE(emmc_sclk_src_sels_name), CLK_SET_RATE_PARENT,
	peri_base+0x78, p_peri_base+0x78, 4, 2, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[emmc_sclk_mask_clk] = odin_clk_register_gate(NULL, "emmc_sclk_mask_clk",
	"emmc_sclk_src_sel_clk", CLK_SET_RATE_PARENT, peri_base+0x78, p_peri_base+0x78, 1, 0,
	NON_SECURE, &clk_out_lock);

	peri_crg_clk[emmc_sclk_div_clk] = odin_clk_register_div(NULL, "emmc_sclk_div_clk",
	"emmc_sclk_mask_clk", 0, peri_base+0x78, NULL, p_peri_base+0x78, 0x0,
	8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[e2nand_sclk_src_sel_clk] = odin_clk_register_mux(NULL, "e2nand_sclk_src_sel_clk",
	e2nand_sclk_src_sels_name, ARRAY_SIZE(e2nand_sclk_src_sels_name), CLK_SET_RATE_PARENT,
	peri_base+0x7C, p_peri_base+0x7C, 4, 2, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[e2nand_sclk_mask_clk] = odin_clk_register_gate(NULL, "e2nand_sclk_mask_clk",
	"e2nand_sclk_src_sel_clk", CLK_SET_RATE_PARENT, peri_base+0x7C, p_peri_base+0x7C, 1, 0,
	NON_SECURE, &clk_out_lock);

	peri_crg_clk[e2nand_sclk_div_clk] = odin_clk_register_div(NULL, "e2nand_sclk_div_clk",
	"e2nand_sclk_mask_clk", 0, peri_base+0x7C, NULL, p_peri_base+0x7C, 0x0,
	8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[i2c_sclk0_src_sel_clk] = odin_clk_register_mux(NULL, "i2c_sclk0_src_sel_clk",
	i2c_sclk0_src_sels_name, ARRAY_SIZE(i2c_sclk0_src_sels_name), CLK_SET_RATE_PARENT,
	peri_base+0x80, p_peri_base+0x80, 4, 2, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[i2c_sclk0_mask_clk] = odin_clk_register_gate(NULL, "i2c_sclk0_mask_clk",
	"i2c_sclk0_src_sel_clk", CLK_SET_RATE_PARENT, peri_base+0x80, p_peri_base+0x80, 1, 0,
	NON_SECURE, &clk_out_lock);

	peri_crg_clk[i2c_sclk0_div_clk] = odin_clk_register_div(NULL, "i2c_sclk0_div_clk",
	"i2c_sclk0_mask_clk", 0, peri_base+0x80, NULL, p_peri_base+0x80, 0x0,
	8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[i2c_sclk1_src_sel_clk] = odin_clk_register_mux(NULL, "i2c_sclk1_src_sel_clk",
	i2c_sclk1_src_sels_name, ARRAY_SIZE(i2c_sclk1_src_sels_name), CLK_SET_RATE_PARENT,
	peri_base+0x84, p_peri_base+0x84, 4, 2, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[i2c_sclk1_mask_clk] = odin_clk_register_gate(NULL, "i2c_sclk1_mask_clk",
	"i2c_sclk1_src_sel_clk", CLK_SET_RATE_PARENT, peri_base+0x84, p_peri_base+0x84, 1, 0,
	NON_SECURE, &clk_out_lock);

	peri_crg_clk[i2c_sclk1_div_clk] = odin_clk_register_div(NULL, "i2c_sclk1_div_clk",
	"i2c_sclk1_mask_clk", 0, peri_base+0x84, NULL, p_peri_base+0x84, 0x0,
	8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[i2c_sclk2_src_sel_clk] = odin_clk_register_mux(NULL, "i2c_sclk2_src_sel_clk",
	i2c_sclk2_src_sels_name, ARRAY_SIZE(i2c_sclk2_src_sels_name), CLK_SET_RATE_PARENT,
	peri_base+0x88, p_peri_base+0x88, 4, 2, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[i2c_sclk2_mask_clk] = odin_clk_register_gate(NULL, "i2c_sclk2_mask_clk",
	"i2c_sclk2_src_sel_clk", CLK_SET_RATE_PARENT, peri_base+0x88, p_peri_base+0x88, 1, 0,
	NON_SECURE, &clk_out_lock);

	peri_crg_clk[i2c_sclk2_div_clk] = odin_clk_register_div(NULL, "i2c_sclk2_div_clk",
	"i2c_sclk2_mask_clk", 0, peri_base+0x88, NULL, p_peri_base+0x88, 0x0,
	8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[i2c_sclk3_src_sel_clk] = odin_clk_register_mux(NULL, "i2c_sclk3_src_sel_clk",
	i2c_sclk3_src_sels_name, ARRAY_SIZE(i2c_sclk3_src_sels_name), CLK_SET_RATE_PARENT,
	peri_base+0x8C, p_peri_base+0x8C, 4, 2, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[i2c_sclk3_mask_clk] = odin_clk_register_gate(NULL, "i2c_sclk3_mask_clk",
	"i2c_sclk3_src_sel_clk", CLK_SET_RATE_PARENT, peri_base+0x8C, p_peri_base+0x8C, 1, 0,
	NON_SECURE, &clk_out_lock);

	peri_crg_clk[i2c_sclk3_div_clk] = odin_clk_register_div(NULL, "i2c_sclk3_div_clk",
	"i2c_sclk3_mask_clk", 0, peri_base+0x8C, NULL, p_peri_base+0x8C, 0x0,
	8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[spi_sclk0_src_sel_clk] = odin_clk_register_mux(NULL, "spi_sclk0_src_sel_clk",
	spi_sclk0_src_sels_name, ARRAY_SIZE(spi_sclk0_src_sels_name), CLK_SET_RATE_PARENT,
	peri_base+0x90, p_peri_base+0x90, 4, 2, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[spi_sclk0_mask_clk] = odin_clk_register_gate(NULL, "spi_sclk0_mask_clk",
	"spi_sclk0_src_sel_clk", CLK_SET_RATE_PARENT, peri_base+0x90, p_peri_base+0x90, 1, 0,
	NON_SECURE, &clk_out_lock);

	peri_crg_clk[spi_sclk0_div_clk] = odin_clk_register_div(NULL, "spi_sclk0_div_clk",
	"spi_sclk0_mask_clk", 0, peri_base+0x90, NULL, p_peri_base+0x90, 0x0,
	8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[uart_sclk0_src_sel_clk] = odin_clk_register_mux(NULL, "uart_sclk0_src_sel_clk",
	uart_sclk0_src_sels_name, ARRAY_SIZE(uart_sclk0_src_sels_name), CLK_SET_RATE_PARENT,
	peri_base+0x98, p_peri_base+0x98, 4, 2, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[uart_sclk0_mask_clk] = odin_clk_register_gate(NULL, "uart_sclk0_mask_clk",
	"uart_sclk0_src_sel_clk", CLK_SET_RATE_PARENT, peri_base+0x98, p_peri_base+0x98, 1, 0,
	NON_SECURE, &clk_out_lock);

	peri_crg_clk[uart_sclk0_div_clk] = odin_clk_register_div(NULL, "uart_sclk0_div_clk",
	"uart_sclk0_mask_clk", 0, peri_base+0x98, NULL, p_peri_base+0x98, 0x0,
	8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[uart_sclk1_src_sel_clk] = odin_clk_register_mux(NULL, "uart_sclk1_src_sel_clk",
	uart_sclk1_src_sels_name, ARRAY_SIZE(uart_sclk1_src_sels_name), CLK_SET_RATE_PARENT,
	peri_base+0x9C, p_peri_base+0x9C, 4, 2, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[uart_sclk1_mask_clk] = odin_clk_register_gate(NULL, "uart_sclk1_mask_clk",
	"uart_sclk1_src_sel_clk", CLK_SET_RATE_PARENT, peri_base+0x9C, p_peri_base+0x9C, 1, 0,
	NON_SECURE, &clk_out_lock);

	peri_crg_clk[uart_sclk1_div_clk] = odin_clk_register_div(NULL, "uart_sclk1_div_clk",
	"uart_sclk1_mask_clk", 0, peri_base+0x9C, NULL, p_peri_base+0x9C, 0x0,
	8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[uart_sclk2_src_sel_clk] = odin_clk_register_mux(NULL, "uart_sclk2_src_sel_clk",
	uart_sclk2_src_sels_name, ARRAY_SIZE(uart_sclk2_src_sels_name), CLK_SET_RATE_PARENT,
	peri_base+0xA0, p_peri_base+0xA0, 4, 2, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[uart_sclk2_mask_clk] = odin_clk_register_gate(NULL, "uart_sclk2_mask_clk",
	"uart_sclk2_src_sel_clk", CLK_SET_RATE_PARENT, peri_base+0xA0, p_peri_base+0xA0, 1, 0,
	NON_SECURE, &clk_out_lock);

	peri_crg_clk[uart_sclk2_div_clk] = odin_clk_register_div(NULL, "uart_sclk2_div_clk",
	"uart_sclk2_mask_clk", 0, peri_base+0xA0, NULL, p_peri_base+0xA0, 0x0,
	8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[uart_sclk3_src_sel_clk] = odin_clk_register_mux(NULL, "uart_sclk3_src_sel_clk",
	uart_sclk3_src_sels_name, ARRAY_SIZE(uart_sclk3_src_sels_name), CLK_SET_RATE_PARENT,
	peri_base+0xA4, p_peri_base+0xA4, 4, 2, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[uart_sclk3_mask_clk] = odin_clk_register_gate(NULL, "uart_sclk3_mask_clk",
	"uart_sclk3_src_sel_clk", CLK_SET_RATE_PARENT, peri_base+0xA4, p_peri_base+0xA4, 1, 0,
	NON_SECURE, &clk_out_lock);

	peri_crg_clk[uart_sclk3_div_clk] = odin_clk_register_div(NULL, "uart_sclk3_div_clk",
	"uart_sclk3_mask_clk", 0, peri_base+0xA4, NULL, p_peri_base+0xA4, 0x0,
	8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[uart_sclk4_src_sel_clk] = odin_clk_register_mux(NULL, "uart_sclk4_src_sel_clk",
	uart_sclk4_src_sels_name, ARRAY_SIZE(uart_sclk4_src_sels_name), CLK_SET_RATE_PARENT,
	peri_base+0xA8, p_peri_base+0xA8, 4, 2, 0, NON_SECURE, &clk_out_lock);

	peri_crg_clk[uart_sclk4_mask_clk] = odin_clk_register_gate(NULL, "uart_sclk4_mask_clk",
	"uart_sclk4_src_sel_clk", CLK_SET_RATE_PARENT, peri_base+0xA8, p_peri_base+0xA8, 1, 0,
	NON_SECURE, &clk_out_lock);

	peri_crg_clk[uart_sclk4_div_clk] = odin_clk_register_div(NULL, "uart_sclk4_div_clk",
	"uart_sclk4_mask_clk", 0, peri_base+0xA8, NULL, p_peri_base+0xA8, 0x0,
	8, 6, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	odin_dt_clk_init(peri_crg_clk, uart_sclk4_div_clk, peri_base, p_peri_base, PERI_BLOCK);
}

static const char *core_src_sels_name[] = {"osc_clk", "dss_pll_clk", "dss_pll_clk"};
static const char *gfxcore_src_sels_name[] = {"osc_clk", "npll_clk_dss", "npll_clk_dss"};
static const char *disp0_src_sels_name[] = {"osc_clk", "dp1_pll_clk", "dp1_pll_clk"};
static const char *disp1_src_sels_name[] = {"osc_clk", "dp2_pll_clk", "dp2_pll_clk"};
static const char *hdmi_src_sels_name[] = {"osc_clk", "dp1_pll_clk", "dp2_pll_clk"};
static const char *osc_src_sels_name[] = {"osc_clk", "osc_clk", "osc_clk"};
static const char *cec_src_sels_name[] = {"osc_clk", "rtc_clk", "rtc_clk"};
static const char *tx_esc_src_sels_name[] = {"osc_clk", "dss_pll_clk", "osc_clk"};

enum dss_crgclk{
	core_src_sel_clk,
	gfxcore_src_sel_clk,
	gfx_div_clk,
	disp0_src_sel_clk,
	disp0_div_clk,
	disp1_src_sel_clk,
	disp1_div_clk,
	hdmi_src_sel_clk,
	hdmi_div_clk,
/*
	osc_src_sel_clk,
	cec_src_sel_clk,
	tx_esc_src_sel_clk,
	tx_esc_div_clk,
	vacl0_clk,
	vacl1_clk,
	vdma_clk,
	mxd_clk,
	cabc_clk,
	formatter_clk,
	gdma0_clk,
	gdma1_clk,
	gdma2_clk,
*/
	overlay_clk,
/*
	gscl_clk,
	rotator_clk,
	sysncgen_clk,
	dip0_clk,
	dss_mipi0_clk,
	dip1_clk,
	dss_mipi1_clk,
	gfx0_clk,
	gfx1_clk,
	dip2_clk,
	hdmi_clk,
*/
	disp0_clk,
	disp1_clk,
	hdmi_disp_clk,
/*
	dphy0_osc_clk,
	dphy1_osc_clk,
	sfr_clk,
	cec_clk,
	tx_esc0_disp,
	tx_esc1_disp,
*/
	dss_crgclk_cnt
};

char *dss_crgclk_names[] = {
	"core_src_sel_clk",
	"gfxcore_src_sel_clk",
	"gfx_div_clk",
	"disp0_src_sel_clk",
	"disp0_div_clk",
	"disp1_src_sel_clk",
	"disp1_div_clk",
	"hdmi_src_sel_clk",
	"hdmi_div_clk",
/*
	"osc_src_sel_clk",
	"cec_src_sel_clk",
	"tx_esc_src_sel_clk",
	"tx_esc_div_clk",
	"vacl0_clk",
	"vacl1_clk",
	"vdma_clk",
	"mxd_clk",
	"cabc_clk",
	"formatter_clk",
	"gdma0_clk",
	"gdma1_clk",
	"gdma2_clk",
*/
	"overlay_clk",
/*
	"gscl_clk",
	"rotator_clk",
	"sysncgen_clk",
	"dip0_clk",
	"dss_mipi0_clk",
	"dip1_clk",
	"dss_mipi1_clk",
	"gfx0_clk",
	"gfx1_clk",
	"dip2_clk",
	"hdmi_clk",
*/
	"disp0_clk",
	"disp1_clk",
	"hdmi_disp_clk",
/*
	"dphy0_osc_clk",
	"dphy1_osc_clk",
	"sfr_clk",
	"cec_clk",
	"tx_esc0_disp",
	"tx_esc1_disp",
*/
	"dss_crgclk_cnt"
};

static struct clk *dss_crg_clk[dss_crgclk_cnt];

void dss_crg_reg(void __iomem *dss_base, u32 p_dss_base)
{

	dss_crg_clk[core_src_sel_clk] = odin_clk_register_mux(NULL, "core_src_sel_clk",
	core_src_sels_name, ARRAY_SIZE(core_src_sels_name), CLK_SET_RATE_PARENT, dss_base,
	p_dss_base, 0, 2, 0, SECURE, &clk_out_lock);

	dss_crg_clk[gfxcore_src_sel_clk] = odin_clk_register_mux(NULL, "gfxcore_src_sel_clk",
	gfxcore_src_sels_name, ARRAY_SIZE(gfxcore_src_sels_name), CLK_SET_RATE_PARENT, dss_base,
	p_dss_base, 4, 2, 0, SECURE, &clk_out_lock);

	dss_crg_clk[gfx_div_clk] = odin_clk_register_div(NULL, "gfx_div_clk", "gfxcore_src_sel_clk",
	0, dss_base+0x10, dss_base+0x14, p_dss_base+0x10, p_dss_base+0x14, 0, 8, 1,
	MACH_ODIN, COMM_PLL_CLK, 0, SECURE, &clk_out_lock);

	dss_crg_clk[disp0_src_sel_clk] = odin_clk_register_mux(NULL, "disp0_src_sel_clk",
	disp0_src_sels_name, ARRAY_SIZE(disp0_src_sels_name), CLK_SET_RATE_PARENT, dss_base+0x8,
	p_dss_base+0x8, 0, 2, 0, SECURE, &clk_out_lock);

	dss_crg_clk[disp0_div_clk] = odin_clk_register_div(NULL, "disp0_div_clk", "disp0_src_sel_clk",
	0, dss_base+0xC, dss_base+0x14, p_dss_base+0xC, p_dss_base+0x14, 8, 8, 2,
	MACH_ODIN, COMM_PLL_CLK, 0, SECURE, &clk_out_lock);

	dss_crg_clk[disp1_src_sel_clk] = odin_clk_register_mux(NULL, "disp1_src_sel_clk",
	disp1_src_sels_name, ARRAY_SIZE(disp1_src_sels_name), CLK_SET_RATE_PARENT, dss_base+0x8,
	p_dss_base+0x8, 2, 2, 0, SECURE, &clk_out_lock);

	dss_crg_clk[disp1_div_clk] = odin_clk_register_div(NULL, "disp1_div_clk", "disp1_src_sel_clk",
	0, dss_base+0xC, dss_base+0x14, p_dss_base+0xC, p_dss_base+0x14, 16, 8, 3,
	MACH_ODIN, COMM_PLL_CLK, 0, SECURE, &clk_out_lock);

	dss_crg_clk[hdmi_src_sel_clk] = odin_clk_register_mux(NULL, "hdmi_src_sel_clk",
	hdmi_src_sels_name, ARRAY_SIZE(hdmi_src_sels_name), CLK_SET_RATE_PARENT, dss_base+0x8,
	p_dss_base+0x8, 4, 2, 0, SECURE, &clk_out_lock);

	dss_crg_clk[hdmi_div_clk] = odin_clk_register_div(NULL, "hdmi_div_clk", "hdmi_src_sel_clk",
	CLK_SET_RATE_PARENT, dss_base+0xC, dss_base+0x14, p_dss_base+0xC, p_dss_base+0x14, 24, 8, 4,
	MACH_ODIN, DP2_PLL_CLK, 0, SECURE, &clk_out_lock);
/*
	dss_crg_clk[osc_src_sel_clk] = odin_clk_register_mux(NULL, "osc_src_sel_clk",
	osc_src_sels_name, ARRAY_SIZE(osc_src_sels_name), CLK_SET_RATE_PARENT, dss_base+0x8,
	p_dss_base+0x8, 6, 2, 0, SECURE, &clk_out_lock);

	dss_crg_clk[cec_src_sel_clk] = odin_clk_register_mux(NULL, "cec_src_sel_clk",
	cec_src_sels_name, ARRAY_SIZE(cec_src_sels_name), CLK_SET_RATE_PARENT, dss_base+0x8,
	p_dss_base+0x8, 8, 2, 0, SECURE, &clk_out_lock);

	dss_crg_clk[tx_esc_src_sel_clk] = odin_clk_register_mux(NULL, "tx_esc_src_sel_clk",
	tx_esc_src_sels_name, ARRAY_SIZE(tx_esc_src_sels_name), CLK_SET_RATE_PARENT, dss_base+0x8,
	p_dss_base+0x8, 10, 2, 0, SECURE, &clk_out_lock);

	dss_crg_clk[tx_esc_div_clk] = odin_clk_register_div(NULL, "tx_esc_div_clk",
	"tx_esc_src_sel_clk", CLK_SET_RATE_PARENT, dss_base+0xC, dss_base+0x14, p_dss_base+0xC,
	p_dss_base+0x14, 0, 8, 6, MACH_ODIN, COMM_PLL_CLK, 0, SECURE, &clk_out_lock);
*/
	odin_dt_clk_init(dss_crg_clk, hdmi_div_clk, dss_base, p_dss_base, DSS_BLOCK);
}

static const char *gpu_mem_src_sels_name[] = {"osc_clk", "gpu_pll_clk"};
static const char *gpu_mem_alt_sels_name[] = {"gpu_mem_src_sel_clk", "npll_clk"};
static const char *gpu_core_src_sels_name[] = {"osc_clk", "gpu_pll_clk"};
static const char *gpu_core_alt_sels_name[] = {"gpu_core_src_sel_clk", "npll_clk"};

enum gpu_crgclk{
	gpu_mem_src_sel_clk,
	gpu_mem_alt_sel_clk,
	gpu_mem_mask_clk,
	gpu_mem_div_clk,
	gpu_sys_div_clk,
	gpu_core_src_sel_clk,
	gpu_core_alt_sel_clk,
	gpu_core_mask_clk,
	gpu_core_div_clk,
	gpu_mem_clk,
	p_gpu_sys_clk,
	gpu_core_clk,

	gpu_crgclk_cnt
};

char *gpu_crgclk_names[] = {
	"gpu_mem_src_sel_clk",
	"gpu_mem_alt_sel_clk",
	"gpu_mem_mask_clk",
	"gpu_mem_div_clk",
	"gpu_sys_div_clk",
	"gpu_core_src_sel_clk",
	"gpu_core_alt_sel_clk",
	"gpu_core_mask_clk",
	"gpu_core_div_clk",
	"gpu_mem_clk",
	"p_gpu_sys_clk",
	"gpu_core_clk",

	"gpu_crgclk_cnt"
};

static struct clk *gpu_crg_clk[gpu_crgclk_cnt];

void gpu_crg_reg(void __iomem *gpu_base, u32 p_gpu_base)
{
	gpu_crg_clk[gpu_mem_src_sel_clk] = odin_clk_register_mux(NULL, "gpu_mem_src_sel_clk",
	gpu_mem_src_sels_name, ARRAY_SIZE(gpu_mem_src_sels_name), CLK_SET_RATE_PARENT, gpu_base,
	p_gpu_base, 0, 4, 0, SECURE, &clk_out_lock);

	gpu_crg_clk[gpu_mem_alt_sel_clk] = odin_clk_register_mux(NULL, "gpu_mem_alt_sel_clk",
	gpu_mem_alt_sels_name, ARRAY_SIZE(gpu_mem_alt_sels_name), CLK_SET_RATE_PARENT, gpu_base,
	p_gpu_base, 4, 4, 0, SECURE, &clk_out_lock);

	gpu_crg_clk[gpu_mem_mask_clk] = odin_clk_register_gate(NULL, "gpu_mem_mask_clk",
	"gpu_mem_alt_sel_clk", CLK_SET_RATE_PARENT, gpu_base+0x4, p_gpu_base+0x4, 0, 0, SECURE,
	&clk_out_lock);

	gpu_crg_clk[gpu_mem_div_clk] = odin_clk_register_div(NULL, "gpu_mem_div_clk",
	"gpu_mem_mask_clk", 0, gpu_base+0x8, NULL, p_gpu_base+0x8, 0x0, 0, 4, 0, MACH_ODIN,
	COMM_PLL_CLK, 0, SECURE, &clk_out_lock);

	//virtual_gpu_sys_div->Do not touch this divider!!!!
	gpu_crg_clk[gpu_sys_div_clk] = odin_clk_register_div(NULL, "gpu_sys_div_clk", "gpu_mem_div_clk",
	0, gpu_base+0x8, NULL, p_gpu_base+0x8, 0x0, 4, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0,
	SECURE, &clk_out_lock);

	gpu_crg_clk[gpu_core_src_sel_clk] = odin_clk_register_mux(NULL, "gpu_core_src_sel_clk",
	gpu_core_src_sels_name, ARRAY_SIZE(gpu_core_src_sels_name), CLK_SET_RATE_PARENT,
	gpu_base+0x200, p_gpu_base+0x200, 0, 4, 0, SECURE, &clk_out_lock);

	gpu_crg_clk[gpu_core_alt_sel_clk] = odin_clk_register_mux(NULL, "gpu_core_alt_sel_clk",
	gpu_core_alt_sels_name, ARRAY_SIZE(gpu_core_alt_sels_name), CLK_SET_RATE_PARENT,
	gpu_base+0x200, p_gpu_base+0x200, 4, 4, 0, SECURE, &clk_out_lock);

	gpu_crg_clk[gpu_core_mask_clk] = odin_clk_register_gate(NULL, "gpu_core_mask_clk",
	"gpu_core_alt_sel_clk", CLK_SET_RATE_PARENT, gpu_base+0x204, p_gpu_base+0x204, 0, 0,
	SECURE, &clk_out_lock);

	gpu_crg_clk[gpu_core_div_clk] = odin_clk_register_div(NULL, "gpu_core_div_clk",
	"gpu_core_mask_clk", CLK_SET_RATE_PARENT, gpu_base+0x208, NULL, p_gpu_base+0x208, 0x0,
	0, 4, 0, MACH_ODIN, GPU_PLL_CLK, 0, SECURE, &clk_out_lock);

	odin_dt_clk_init(gpu_crg_clk, gpu_core_div_clk, gpu_base, p_gpu_base, GPU_BLOCK);
}

static const char *vsp0_c_codec_src_sels_name[] = {"osc_clk", "dss_pll_clk", "npll_clk"};
//static const char *vsp0_e_codec_src_sels_name[] = {"osc_clk", "npll_clk", "dss_pll_clk"};
static const char *vsp0_aclk_src_sels_name[] = {"osc_clk", "npll_clk", "dss_pll_clk"};
static const char *vsp0_pclk_src_sels_name[] = {"osc_clk", "npll_clk", "dss_pll_clk"};

enum vsp0_crgclk{
	vsp0_c_codec_src_sel_clk,
	vsp0_c_codec_mask_clk,
	vsp0_c_codec_div_clk,
	vsp0_aclk_src_sel_clk,
	vsp0_aclk_mask_clk,
	vsp0_aclk_div_clk,
	vsp0_pclk_src_sel_clk,
	vsp0_pclk_mask_clk,
	vsp0_pclk_div_clk,
	vsp0_c_codec,
	vsp0_e_codec,
	vsp0_aclk_clk,
	vsp0_pclk_clk,

	vsp0_crgclk_cnt
};

char *vsp0_crgclk_names[] = {
	"vsp0_c_codec_src_sel_clk",
	"vsp0_c_codec_mask_clk",
	"vsp0_c_codec_div_clk",
	"vsp0_aclk_src_sel_clk",
	"vsp0_aclk_mask_clk",
	"vsp0_aclk_div_clk",
	"vsp0_pclk_src_sel_clk",
	"vsp0_pclk_mask_clk",
	"vsp0_pclk_div_clk",
	"vsp0_c_codec",
	"vsp0_e_codec",
	"vsp0_aclk_clk",
	"vsp0_pclk_clk",

	"vsp0_crgclk_cnt"
};

static struct clk *vsp0_crg_clk[vsp0_crgclk_cnt];

void vsp0_crg_reg(void __iomem *vsp0_base, u32 p_vsp0_base)
{

	vsp0_crg_clk[vsp0_c_codec_src_sel_clk] = odin_clk_register_mux(NULL, "vsp0_c_codec_src_sel_clk",
	vsp0_c_codec_src_sels_name, ARRAY_SIZE(vsp0_c_codec_src_sels_name), CLK_SET_RATE_PARENT,
	vsp0_base+0x200, p_vsp0_base+0x200, 0, 2, 0, SECURE, &clk_out_lock);

	vsp0_crg_clk[vsp0_c_codec_mask_clk] = odin_clk_register_gate(NULL, "vsp0_c_codec_mask_clk",
	"vsp0_c_codec_src_sel_clk", CLK_SET_RATE_PARENT, vsp0_base+0x204, p_vsp0_base+0x204, 0, 0,
	SECURE, &clk_out_lock);

	vsp0_crg_clk[vsp0_c_codec_div_clk] = odin_clk_register_div(NULL, "vsp0_c_codec_div_clk",
	"vsp0_c_codec_mask_clk", 0, vsp0_base+0x208, NULL, p_vsp0_base+0x208, 0x0, 0,
	4, 0, MACH_ODIN, COMM_PLL_CLK, 0, SECURE, &clk_out_lock);

	vsp0_crg_clk[vsp0_aclk_src_sel_clk] = odin_clk_register_mux(NULL, "vsp0_aclk_src_sel_clk",
	vsp0_aclk_src_sels_name, ARRAY_SIZE(vsp0_aclk_src_sels_name), CLK_SET_RATE_PARENT,
	vsp0_base+0x240, p_vsp0_base+0x240, 0, 2, 0, SECURE, &clk_out_lock);

	vsp0_crg_clk[vsp0_aclk_mask_clk] = odin_clk_register_gate(NULL, "vsp0_aclk_mask_clk",
	"vsp0_aclk_src_sel_clk", CLK_SET_RATE_PARENT, vsp0_base+0x244, p_vsp0_base+0x244, 0, 0,
	SECURE, &clk_out_lock);

	vsp0_crg_clk[vsp0_aclk_div_clk] = odin_clk_register_div(NULL, "vsp0_aclk_div_clk",
	"vsp0_aclk_mask_clk", 0, vsp0_base+0x248, NULL, p_vsp0_base+0x248,
	0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0, SECURE, &clk_out_lock);

	vsp0_crg_clk[vsp0_pclk_src_sel_clk] = odin_clk_register_mux(NULL, "vsp0_pclk_src_sel_clk",
	vsp0_pclk_src_sels_name, ARRAY_SIZE(vsp0_pclk_src_sels_name), CLK_SET_RATE_PARENT,
	vsp0_base+0x250, p_vsp0_base+0x250, 0, 2, 0, SECURE, &clk_out_lock);

	vsp0_crg_clk[vsp0_pclk_mask_clk] = odin_clk_register_gate(NULL, "vsp0_pclk_mask_clk",
	"vsp0_pclk_src_sel_clk", CLK_SET_RATE_PARENT, vsp0_base+0x254, p_vsp0_base+0x254, 0, 0,
	SECURE, &clk_out_lock);

	vsp0_crg_clk[vsp0_pclk_div_clk] = odin_clk_register_div(NULL, "vsp0_pclk_div_clk",
	"vsp0_pclk_mask_clk", 0, vsp0_base+0x258, NULL, p_vsp0_base+0x258, 0x0,
	0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0, SECURE, &clk_out_lock);

	odin_dt_clk_init(vsp0_crg_clk, vsp0_pclk_div_clk, vsp0_base, p_vsp0_base, VSP0_BLOCK);
}

static const char *vsp1_dec_src_sels_name[] = {"osc_clk", "dss_pll_clk","npll_clk"};
static const char *vsp1_img_src_sels_name[] = {"osc_clk","dss_pll_clk","npll_clk"};
static const char *vsp1_aclk_src_sels_name[] = {"osc_clk", "npll_clk", "dss_pll_clk"};
static const char *vsp1_pclk_src_sels_name[] = {"osc_clk", "npll_clk", "dss_pll_clk"};

enum vsp1_crgclk{
	vsp1_dec_src_sel_clk,
	vsp1_dec_mask_clk,
	vsp1_dec_div_clk,
	vsp1_img_src_sel_clk,
	vsp1_img_mask_clk,
	vsp1_img_div_clk,
	vsp1_aclk_src_sel_clk,
	vsp1_aclk_mask_clk,
	vsp1_aclk_div_clk,
	vsp1_pclk_src_sel_clk,
	vsp1_pclk_mask_clk,
	vsp1_pclk_div_clk,
	vsp1_dec_clk,
	vsp1_img_clk,
	vsp1_aclk_clk,
	vsp1_pclk_clk,

	vsp1_crgclk_cnt
};

char *vsp1_crgclk_names[] = {
vsp1_dec_src_sel_clk,
	"vsp1_dec_mask_clk",
	"vsp1_dec_div_clk",
	"vsp1_img_src_sel_clk",
	"vsp1_img_mask_clk",
	"vsp1_img_div_clk",
	"vsp1_aclk_src_sel_clk",
	"vsp1_aclk_mask_clk",
	"vsp1_aclk_div_clk",
	"vsp1_pclk_src_sel_clk",
	"vsp1_pclk_mask_clk",
	"vsp1_pclk_div_clk",
	"vsp1_dec_clk",
	"vsp1_img_clk",
	"vsp1_aclk_clk",
	"vsp1_pclk_clk",

	"vsp1_crgclk_cnt"
};

static struct clk *vsp1_crg_clk[vsp1_crgclk_cnt];

void vsp1_crg_reg(void __iomem *vsp1_base, u32 p_vsp1_base)
{
	vsp1_crg_clk[vsp1_dec_src_sel_clk] = odin_clk_register_mux(NULL, "vsp1_dec_src_sel_clk",
	vsp1_dec_src_sels_name, ARRAY_SIZE(vsp1_dec_src_sels_name), CLK_SET_RATE_PARENT,
	vsp1_base+0x220, p_vsp1_base+0x220, 0, 2, 0, SECURE, &clk_out_lock);

	vsp1_crg_clk[vsp1_dec_mask_clk] = odin_clk_register_gate(NULL, "vsp1_dec_mask_clk",
	"vsp1_dec_src_sel_clk", CLK_SET_RATE_PARENT, vsp1_base+0x224, p_vsp1_base+0x224, 0, 0,
	SECURE, &clk_out_lock);

	vsp1_crg_clk[vsp1_dec_div_clk] = odin_clk_register_div(NULL, "vsp1_dec_div_clk",
	"vsp1_dec_mask_clk", 0, vsp1_base+0x228, NULL, p_vsp1_base+0x228, 0x0,
	0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0, SECURE, &clk_out_lock);

	vsp1_crg_clk[vsp1_img_src_sel_clk] = odin_clk_register_mux(NULL, "vsp1_img_src_sel_clk",
	vsp1_img_src_sels_name, ARRAY_SIZE(vsp1_img_src_sels_name), CLK_SET_RATE_PARENT,
	vsp1_base+0x230, p_vsp1_base+0x230, 0, 2, 0, SECURE, &clk_out_lock);

	vsp1_crg_clk[vsp1_img_mask_clk] = odin_clk_register_gate(NULL, "vsp1_img_mask_clk",
	"vsp1_img_src_sel_clk", CLK_SET_RATE_PARENT, vsp1_base+0x234, p_vsp1_base+0x234, 0, 0,
	SECURE, &clk_out_lock);

	vsp1_crg_clk[vsp1_img_div_clk] = odin_clk_register_div(NULL, "vsp1_img_div_clk",
	"vsp1_img_mask_clk", 0, vsp1_base+0x238, NULL, p_vsp1_base+0x238, 0x0,
	0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0, SECURE, &clk_out_lock);

	vsp1_crg_clk[vsp1_aclk_src_sel_clk] = odin_clk_register_mux(NULL, "vsp1_aclk_src_sel_clk",
	vsp1_aclk_src_sels_name, ARRAY_SIZE(vsp1_aclk_src_sels_name), CLK_SET_RATE_PARENT,
	vsp1_base+0x240, p_vsp1_base+0x240, 0, 2, 0, SECURE, &clk_out_lock);

	vsp1_crg_clk[vsp1_aclk_mask_clk] = odin_clk_register_gate(NULL, "vsp1_aclk_mask_clk",
	"vsp1_aclk_src_sel_clk", CLK_SET_RATE_PARENT, vsp1_base+0x244, p_vsp1_base+0x244, 0, 0,
	SECURE, &clk_out_lock);

	vsp1_crg_clk[vsp1_aclk_div_clk] = odin_clk_register_div(NULL, "vsp1_aclk_div_clk",
	"vsp1_aclk_mask_clk", 0, vsp1_base+0x248, NULL, p_vsp1_base+0x248, 0x0,
	0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0, SECURE, &clk_out_lock);

	vsp1_crg_clk[vsp1_pclk_src_sel_clk] = odin_clk_register_mux(NULL, "vsp1_pclk_src_sel_clk",
	vsp1_pclk_src_sels_name, ARRAY_SIZE(vsp1_pclk_src_sels_name), CLK_SET_RATE_PARENT,
	vsp1_base+0x250, p_vsp1_base+0x250, 0, 2, 0, SECURE, &clk_out_lock);

	vsp1_crg_clk[vsp1_pclk_mask_clk] = odin_clk_register_gate(NULL, "vsp1_pclk_mask_clk",
	"vsp1_pclk_src_sel_clk", CLK_SET_RATE_PARENT, vsp1_base+0x254, p_vsp1_base+0x254, 0, 0,
	SECURE, &clk_out_lock);

	vsp1_crg_clk[vsp1_pclk_div_clk] = odin_clk_register_div(NULL, "vsp1_pclk_div_clk",
	"vsp1_pclk_mask_clk", 0, vsp1_base+0x258, NULL, p_vsp1_base+0x258,
	0x0, 0, 4, 0, MACH_ODIN, COMM_PLL_CLK, 0, SECURE, &clk_out_lock);

	odin_dt_clk_init(vsp1_crg_clk, vsp1_pclk_div_clk, vsp1_base, p_vsp1_base, VSP1_BLOCK);
}

static const char *dsp_cclk_src_sels_name[] = {"osc_clk", "npll_clk", "isp_pll_clk"};
static const char *p_async_src_sels_name[] = {"osc_clk", "npll_clk", "isp_pll_clk"};
static const char *slimbus_src_sels_name[] = {"osc_clk", "aud_pll_clk", "npll_clk"};
static const char *i2s0_pll_sels_name[] = {"osc_clk", "aud_pll_clk"};
static const char *i2s0_alt_sels_name[] = {"aud_pll_clk",};
static const char *i2s0_src_sels_name[] = {"i2s0_pll_sel_clk", "i2s0_alt_sel_clk"};
static const char *i2s1_pll_sels_name[] = {"osc_clk", "aud_pll_clk"};
static const char *i2s1_alt_sels_name[] = {"aud_pll_clk",};
static const char *i2s1_src_sels_name[] = {"i2s1_pll_sel_clk", "i2s1_alt_sel_clk"};
static const char *i2s2_pll_sels_name[] = {"osc_clk", "aud_pll_clk"};
static const char *i2s2_alt_sels_name[] = {"aud_pll_clk",};
static const char *i2s2_src_sels_name[] = {"i2s2_pll_sel_clk", "i2s2_alt_sel_clk"};
static const char *i2s3_pll_sels_name[] = {"osc_clk", "aud_pll_clk"};
static const char *i2s3_alt_sels_name[] = {"aud_pll_clk",};
static const char *i2s3_src_sels_name[] = {"i2s3_pll_sel_clk", "i2s3_alt_sel_clk"};
static const char *i2s4_pll_sels_name[] = {"osc_clk", "aud_pll_clk"};
static const char *i2s4_alt_sels_name[] = {"aud_pll_clk",};
static const char *i2s4_src_sels_name[] = {"i2s4_pll_sel_clk", "i2s4_alt_sel_clk"};
static const char *i2s5_pll_sels_name[] = {"osc_clk", "aud_pll_clk"};
static const char *i2s5_alt_sels_name[] = {"aud_pll_clk",};
static const char *i2s5_src_sels_name[] = {"i2s5_pll_sel_clk", "i2s5_alt_sel_clk"};

enum aud_crgclk{
	aud_pll_clk,
	dsp_cclk_src_sel_clk,
	dsp_cclk_mask_clk,
	dsp_cclk_div_clk,
	aclk_dsp_div_clk,
	p_async_src_sel_clk,
	p_async_mask_clk,
	p_async_div_clk,
	slimbus_src_sel_clk,
	slimbus_mask_clk,
	slimbus_div_clk,
	i2s0_pll_sel_clk,
	i2s0_alt_sel_clk,
	i2s0_src_sel_clk,
	i2s0_mask_clk,
	i2s0_div_clk,

	i2s1_pll_sel_clk,
	i2s1_alt_sel_clk,
	i2s1_src_sel_clk,
	i2s1_mask_clk,
	i2s1_div_clk,

	i2s2_pll_sel_clk,
	i2s2_alt_sel_clk,
	i2s2_src_sel_clk,
	i2s2_mask_clk,
	i2s2_div_clk,

	i2s3_pll_sel_clk,
	i2s3_alt_sel_clk,
	i2s3_src_sel_clk,
	i2s3_mask_clk,
	i2s3_div_clk,

	i2s4_pll_sel_clk,
	i2s4_alt_sel_clk,
	i2s4_src_sel_clk,
	i2s4_mask_clk,
	i2s4_div_clk,

	i2s5_pll_sel_clk,
	i2s5_alt_sel_clk,
	i2s5_src_sel_clk,
	i2s5_mask_clk,
	i2s5_div_clk,

	p_cclk_dsp,
	aclk_dsp,
	pclk_dsp,
	p_pclk_async,
	clk_slimbus,
	mclk_i2s0,
	mclk_i2s1,
	mclk_i2s2,
	mclk_i2s3,
	mclk_i2s4,
	mclk_i2s5,

	aud_crgclk_cnt
};

char *aud_crgclk_names[] = {
	"aud_pll_clk",
	"dsp_cclk_src_sel_clk",
	"dsp_cclk_mask_clk",
	"dsp_cclk_div_clk",
	"aclk_dsp_div_clk",
	"aclk_dsp_div_clk",
	"p_async_src_sel_clk",
	"p_async_mask_clk",
	"p_async_div_clk",
	"slimbus_src_sel_clk",
	"slimbus_mask_clk",
	"slimbus_div_clk",
	"i2s0_pll_sel_clk",
	"i2s0_alt_sel_clk",
	"i2s0_src_sel_clk",
	"i2s0_mask_clk",
	"i2s0_div_clk",

	"i2s1_pll_sel_clk",
	"i2s1_alt_sel_clk",
	"i2s1_src_sel_clk",
	"i2s1_mask_clk",
	"i2s1_div_clk",

	"i2s2_pll_sel_clk",
	"i2s2_alt_sel_clk",
	"i2s2_src_sel_clk",
	"i2s2_mask_clk",
	"i2s2_div_clk",

	"i2s3_pll_sel_clk",
	"i2s3_alt_sel_clk",
	"i2s3_src_sel_clk",
	"i2s3_mask_clk",
	"i2s3_div_clk",

	"i2s4_pll_sel_clk",
	"i2s4_alt_sel_clk",
	"i2s4_src_sel_clk",
	"i2s4_mask_clk",
	"i2s4_div_clk",

	"i2s5_pll_sel_clk",
	"i2s5_alt_sel_clk",
	"i2s5_src_sel_clk",
	"i2s5_mask_clk",
	"i2s5_div_clk",

	"p_cclk_dsp",
	"aclk_dsp",
	"pclk_dsp",
	"p_pclk_async",
	"clk_slimbus",
	"mclk_i2s0",
	"mclk_i2s1",
	"mclk_i2s2",
	"mclk_i2s3",
	"mclk_i2s4",
	"mclk_i2s5",

	"aud_crgclk_cnt"
};

static struct clk *aud_crg_clk[aud_crgclk_cnt];

void aud_crg_reg(void __iomem *aud_base, u32 p_aud_base)
{
	aud_crg_clk[aud_pll_clk] = odin_clk_register_apll(NULL, "aud_pll_clk", "osc_clk", 0, aud_base,
	aud_base+0x8, p_aud_base, p_aud_base+0x8, 0, MACH_ODIN, AUD_PLL_CLK, NON_SECURE, &clk_out_lock);
	aud_crg_clk[dsp_cclk_src_sel_clk] = odin_clk_register_mux(NULL, "dsp_cclk_src_sel_clk",
	dsp_cclk_src_sels_name, ARRAY_SIZE(dsp_cclk_src_sels_name), CLK_SET_RATE_PARENT, aud_base+0x010,
	p_aud_base+0x010, 0, 2, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[dsp_cclk_mask_clk] = odin_clk_register_gate(NULL, "dsp_cclk_mask_clk",
	"dsp_cclk_src_sel_clk", CLK_SET_RATE_PARENT, aud_base+0x014, p_aud_base+0x014, 0, 0,
	NON_SECURE, &clk_out_lock);

	aud_crg_clk[dsp_cclk_div_clk] = odin_clk_register_div(NULL, "dsp_cclk_div_clk",
	"dsp_cclk_mask_clk", 0, aud_base+0x018, NULL, p_aud_base+0x018, 0x0,
	0, 8, 0, MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[aclk_dsp_div_clk] = odin_clk_register_div(NULL, "aclk_dsp_div_clk",
	"dsp_cclk_div_clk", 0, aud_base+0x020, NULL, p_aud_base+0x020, 0x0, 0, 4, 0,
	MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[aclk_dsp_div_clk] = odin_clk_register_div(NULL, "pclk_dsp_div_clk",
	"dsp_cclk_div_clk", 0, aud_base+0x028, NULL, p_aud_base+0x028, 0x0, 0, 4, 0,
	MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[p_async_src_sel_clk] = odin_clk_register_mux(NULL, "p_async_src_sel_clk",
	p_async_src_sels_name, ARRAY_SIZE(p_async_src_sels_name), CLK_SET_RATE_PARENT, aud_base+0x030,
	p_aud_base+0x030, 0, 2, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[p_async_mask_clk] = odin_clk_register_gate(NULL, "p_async_mask_clk",
	"p_async_src_sel_clk", CLK_SET_RATE_PARENT, aud_base+0x034, p_aud_base+0x034, 0, 0,
	NON_SECURE, &clk_out_lock);

	aud_crg_clk[p_async_div_clk] = odin_clk_register_div(NULL, "p_async_div_clk",
	"p_async_mask_clk", 0, aud_base+0x038, NULL, p_aud_base+0x038, 0x0, 0, 8, 0,
	MACH_ODIN, COMM_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[slimbus_src_sel_clk] = odin_clk_register_mux(NULL, "slimbus_src_sel_clk",
	slimbus_src_sels_name, ARRAY_SIZE(slimbus_src_sels_name), CLK_SET_RATE_PARENT, aud_base+0x040,
	p_aud_base+0x040, 0, 2, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[slimbus_mask_clk] = odin_clk_register_gate(NULL, "slimbus_mask_clk",
	"slimbus_src_sel_clk", CLK_SET_RATE_PARENT, aud_base+0x044, p_aud_base+0x044, 0, 0,
	NON_SECURE, &clk_out_lock);

	aud_crg_clk[slimbus_div_clk] = odin_clk_register_div(NULL, "slimbus_div_clk",
	"slimbus_mask_clk", 0, aud_base+0x048, NULL, p_aud_base+0x048, 0x0, 0, 4, 0,
	MACH_ODIN, AUD_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	/*I2S0 CLK*/
	aud_crg_clk[i2s0_pll_sel_clk] = odin_clk_register_mux(NULL, "i2s0_pll_sel_clk",
	i2s0_pll_sels_name, ARRAY_SIZE(i2s0_pll_sels_name), CLK_SET_RATE_PARENT, aud_base+0x050,
	p_aud_base+0x050, 0, 1, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s0_alt_sel_clk] = odin_clk_register_mux(NULL, "i2s0_alt_sel_clk",
	i2s0_alt_sels_name, ARRAY_SIZE(i2s0_alt_sels_name), CLK_SET_RATE_PARENT, aud_base+0x050,
	p_aud_base+0x50, 8, 3, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s0_src_sel_clk] = odin_clk_register_mux(NULL, "i2s0_src_sel_clk",
	i2s0_src_sels_name, ARRAY_SIZE(i2s0_src_sels_name), CLK_SET_RATE_PARENT, aud_base+0x050,
	p_aud_base+0x050, 4, 1, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s0_mask_clk] = odin_clk_register_gate(NULL, "i2s0_mask_clk", "i2s0_src_sel_clk",
	CLK_SET_RATE_PARENT, aud_base+0x054,
	p_aud_base+0x054, 0, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s0_div_clk] = odin_clk_register_div(NULL, "i2s0_div_clk", "i2s0_mask_clk",
	CLK_SET_RATE_PARENT, aud_base+0x058, NULL,
	p_aud_base+0x058, 0x0, 0, 8, 0, MACH_ODIN, AUD_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

/*I2S1 CLK*/
	aud_crg_clk[i2s1_pll_sel_clk] = odin_clk_register_mux(NULL, "i2s1_pll_sel_clk",
	i2s1_pll_sels_name, ARRAY_SIZE(i2s1_pll_sels_name), CLK_SET_RATE_PARENT, aud_base+0x060,
	p_aud_base+0x060, 0, 1, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s1_alt_sel_clk] = odin_clk_register_mux(NULL, "i2s1_alt_sel_clk",
	i2s1_alt_sels_name, ARRAY_SIZE(i2s1_alt_sels_name), CLK_SET_RATE_PARENT, aud_base+0x060,
	p_aud_base+0x60, 8, 3, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s1_src_sel_clk] = odin_clk_register_mux(NULL, "i2s1_src_sel_clk",
	i2s1_src_sels_name, ARRAY_SIZE(i2s1_src_sels_name), CLK_SET_RATE_PARENT, aud_base+0x060,
	p_aud_base+0x060, 4, 1, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s1_mask_clk] = odin_clk_register_gate(NULL, "i2s1_mask_clk", "i2s1_src_sel_clk",
	CLK_SET_RATE_PARENT, aud_base+0x064,
	p_aud_base+0x064, 0, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s1_div_clk] = odin_clk_register_div(NULL, "i2s1_div_clk", "i2s1_mask_clk",
	CLK_SET_RATE_PARENT, aud_base+0x068, NULL,
	p_aud_base+0x068, 0x0, 0, 8, 0, MACH_ODIN, AUD_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

/*I2S2 CLK*/
	aud_crg_clk[i2s2_pll_sel_clk] = odin_clk_register_mux(NULL, "i2s2_pll_sel_clk",
	i2s2_pll_sels_name, ARRAY_SIZE(i2s2_pll_sels_name), CLK_SET_RATE_PARENT, aud_base+0x070,
	p_aud_base+0x070, 0, 1, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s2_alt_sel_clk] = odin_clk_register_mux(NULL, "i2s2_alt_sel_clk",
	i2s2_alt_sels_name, ARRAY_SIZE(i2s2_alt_sels_name), CLK_SET_RATE_PARENT, aud_base+0x070,
	p_aud_base+0x70, 8, 3, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s2_src_sel_clk] = odin_clk_register_mux(NULL, "i2s2_src_sel_clk",
	i2s2_src_sels_name, ARRAY_SIZE(i2s2_src_sels_name), CLK_SET_RATE_PARENT, aud_base+0x070,
	p_aud_base+0x070, 4, 1, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s2_mask_clk] = odin_clk_register_gate(NULL, "i2s2_mask_clk", "i2s2_src_sel_clk",
	CLK_SET_RATE_PARENT, aud_base+0x074,
	p_aud_base+0x074, 0, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s2_div_clk] = odin_clk_register_div(NULL, "i2s2_div_clk", "i2s2_mask_clk",
	CLK_SET_RATE_PARENT, aud_base+0x078, NULL,
	p_aud_base+0x078, 0x0, 0, 8, 0, MACH_ODIN, AUD_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

/*I2S3 CLK*/
	aud_crg_clk[i2s3_pll_sel_clk] = odin_clk_register_mux(NULL, "i2s3_pll_sel_clk",
	i2s3_pll_sels_name, ARRAY_SIZE(i2s3_pll_sels_name), CLK_SET_RATE_PARENT, aud_base+0x080,
	p_aud_base+0x080, 0, 1, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s3_alt_sel_clk] = odin_clk_register_mux(NULL, "i2s3_alt_sel_clk",
	i2s3_alt_sels_name, ARRAY_SIZE(i2s3_alt_sels_name), CLK_SET_RATE_PARENT, aud_base+0x080,
	p_aud_base+0x80, 8, 3, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s3_src_sel_clk] = odin_clk_register_mux(NULL, "i2s3_src_sel_clk",
	i2s3_src_sels_name, ARRAY_SIZE(i2s3_src_sels_name), CLK_SET_RATE_PARENT, aud_base+0x080,
	p_aud_base+0x080, 4, 1, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s3_mask_clk] = odin_clk_register_gate(NULL, "i2s3_mask_clk", "i2s3_src_sel_clk",
	CLK_SET_RATE_PARENT, aud_base+0x084,
	p_aud_base+0x084, 0, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s3_div_clk] = odin_clk_register_div(NULL, "i2s3_div_clk", "i2s3_mask_clk",
	CLK_SET_RATE_PARENT, aud_base+0x088, NULL,
	p_aud_base+0x088, 0x0, 0, 8, 0, MACH_ODIN, AUD_PLL_CLK, 0,
	NON_SECURE, &clk_out_lock);

	/*I2S4 CLK*/
	aud_crg_clk[i2s4_pll_sel_clk] = odin_clk_register_mux(NULL, "i2s4_pll_sel_clk",
	i2s4_pll_sels_name, ARRAY_SIZE(i2s4_pll_sels_name), CLK_SET_RATE_PARENT, aud_base+0x090,
	p_aud_base+0x090, 0, 1, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s4_alt_sel_clk] = odin_clk_register_mux(NULL, "i2s4_alt_sel_clk",
	i2s4_alt_sels_name, ARRAY_SIZE(i2s4_alt_sels_name), CLK_SET_RATE_PARENT, aud_base+0x090,
	p_aud_base+0x090, 8, 3, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s4_src_sel_clk] = odin_clk_register_mux(NULL, "i2s4_src_sel_clk",
	i2s4_src_sels_name, ARRAY_SIZE(i2s4_src_sels_name), CLK_SET_RATE_PARENT, aud_base+0x090,
	p_aud_base+0x090, 4, 1, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s4_mask_clk] = odin_clk_register_gate(NULL, "i2s4_mask_clk", "i2s4_src_sel_clk",
	CLK_SET_RATE_PARENT, aud_base+0x094, p_aud_base+0x094, 0, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s4_div_clk] = odin_clk_register_div(NULL, "i2s4_div_clk", "i2s4_mask_clk",
	CLK_SET_RATE_PARENT, aud_base+0x098, NULL, p_aud_base+0x098, 0x0, 0, 8, 0, MACH_ODIN,
	AUD_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	/*I2S5 CLK*/
	aud_crg_clk[i2s5_pll_sel_clk] = odin_clk_register_mux(NULL, "i2s5_pll_sel_clk",
	i2s5_pll_sels_name, ARRAY_SIZE(i2s5_pll_sels_name), CLK_SET_RATE_PARENT, aud_base+0x0A0,
	p_aud_base+0x0A0, 0, 1, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s5_alt_sel_clk] = odin_clk_register_mux(NULL, "i2s5_alt_sel_clk",
	i2s5_alt_sels_name, ARRAY_SIZE(i2s5_alt_sels_name), CLK_SET_RATE_PARENT, aud_base+0x0A0,
	p_aud_base+0x0A0, 8, 3, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s5_src_sel_clk] = odin_clk_register_mux(NULL, "i2s5_src_sel_clk",
	i2s5_src_sels_name, ARRAY_SIZE(i2s5_src_sels_name), CLK_SET_RATE_PARENT, aud_base+0x0A0,
	p_aud_base+0x0A0, 4, 1, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s5_mask_clk] = odin_clk_register_gate(NULL, "i2s5_mask_clk", "i2s5_src_sel_clk",
	CLK_SET_RATE_PARENT, aud_base+0x0A4, p_aud_base+0x0A4, 0, 0, NON_SECURE, &clk_out_lock);

	aud_crg_clk[i2s5_div_clk] = odin_clk_register_div(NULL, "i2s5_div_clk", "i2s5_mask_clk",
	CLK_SET_RATE_PARENT, aud_base+0x0A8, NULL, p_aud_base+0x0A8, 0x0, 0, 8, 0, MACH_ODIN,
	AUD_PLL_CLK, 0, NON_SECURE, &clk_out_lock);

	odin_dt_clk_init(aud_crg_clk, i2s5_div_clk, aud_base, p_aud_base, AUD_BLOCK);
}

int odin_clk_enable_check(u8 block_type)
{
	struct device_node *np, *from;
	struct clk *clk;
	int ret = 0;

	from = of_find_node_by_name(NULL, "clocks");

	for(np = of_get_next_child(from, NULL); np; np=of_get_next_child(from, np)){

		u32 val[5];

		of_property_read_u32_array(np, "reg-flag", val, ARRAY_SIZE(val));

		if(block_type == ALL_BLOCK || val[0] == block_type){
			clk = clk_get(NULL, np->name);

			if(__clk_is_enabled(clk)){
				ret = clk_prepare_enable(clk);
				if(ret){
					pr_err("can't enable %s\n", clk->name);
					return ret;
				}
			}
		}
	}

	return ret;
}

int clk_reg_update(u8 block_type, u8 secure_flag)
{
	struct clk **clk = NULL;
	int i, cnt = 0;

	switch(block_type){
		case CORE_BLOCK:
			clk = core_crg_clk;
			cnt = core_crgclk_cnt;
			break;
		case CPU_BLOCK:
			clk = cpu_crg_clk;
			cnt = cpu_crgclk_cnt;
			break;
		case ICS_BLOCK:
			clk = ics_crg_clk;
			cnt = ics_crgclk_cnt;
			break;
		case CSS_BLOCK:
			clk = css_crg_clk;
			cnt = css_crgclk_cnt;
			break;
		case PERI_BLOCK:
			clk = peri_crg_clk;
			cnt = peri_crgclk_cnt;
			break;
		case DSS_BLOCK:
			clk = dss_crg_clk;
			cnt = dss_crgclk_cnt;
			break;
		case GPU_BLOCK:
			clk = gpu_crg_clk;
			cnt = gpu_crgclk_cnt;
			break;
		case VSP0_BLOCK:
			clk = vsp0_crg_clk;
			cnt = vsp0_crgclk_cnt;
			break;
		case VSP1_BLOCK:
			clk = vsp1_crg_clk;
			cnt = vsp1_crgclk_cnt;
			break;
		case AUD_BLOCK:
			clk = aud_crg_clk;
			cnt = aud_crgclk_cnt;
			break;
		case SMS_BLOCK:
			clk = sms_crg_clk;
			cnt = sms_crgclk_cnt;
			break;
		default:
			pr_err("%s: could not find block\n", __func__);
			return -EINVAL;
			break;
	}

	for(i =0; i < cnt; i++){
		__clk_reg_update(NULL, clk[i]);
		clk[i]->enable_count = 0;
	}

	odin_clk_enable_check(block_type);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int odin_clk_probe(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	printk("%s complete\n", __func__);
	return 0;
}

static int odin_clk_suspend(struct device *dev)
{
	clk_reg_store(ODIN_PERI_CRG_BASE, peri_clk_save, ARRAY_SIZE(peri_clk_save), SZ_4K, NON_SECURE);
	clk_reg_store(ODIN_AUD_CRG_BASE, aud_clk_save, ARRAY_SIZE(aud_clk_save), SZ_4K, NON_SECURE);

	s_r_flag = 0;

#ifdef CONFIG_PM_SLEEP_DEBUG
	clock_debug_print_enabled();
#endif
	return 0;
}

static int odin_clk_resume(struct device *dev)
{
	clk_reg_restore(ODIN_PERI_CRG_BASE, peri_clk_save, ARRAY_SIZE(peri_clk_save), SZ_4K, NON_SECURE);
	clk_reg_restore(ODIN_AUD_CRG_BASE, aud_clk_save, ARRAY_SIZE(aud_clk_save), SZ_4K, NON_SECURE);

	s_r_flag = 1;
	return 0;
}

static struct of_device_id odin_clk_match[] = {
	{ .compatible = "odin_clock_driver"},
	{},
};

static const struct dev_pm_ops odin_clk_pm_ops = {
       .suspend_late           = odin_clk_suspend,
       .resume_early           = odin_clk_resume,
};

static struct platform_driver clk_driver = {
	.probe 		= odin_clk_probe,
	.driver		= {
		.name	= "odin_clk_driver",
		.of_match_table	= of_match_ptr(odin_clk_match),
		.pm	= &odin_clk_pm_ops,
	},
};
#endif /*CONFIG_PM_SLEEP*/

int odin_clk_init(void)
{
	void __iomem *sms_base, *core_base, *cpu_base, *ics_base, *css_base, *peri_base;
	void __iomem *dss_base, *gpu_base, *vsp0_base, *vsp1_base, *aud_base;

	mb_clk_probe();

	root_crg_reg();

	sms_base = ioremap(ODIN_SMS_CRG_BASE, SZ_4K);
	gpu_base = ioremap(ODIN_GPU_CRG_BASE, SZ_4K);
	sms_crg_reg(sms_base, gpu_base, ODIN_SMS_CRG_BASE, ODIN_GPU_CRG_BASE);

	core_base = ioremap(ODIN_CORE_CRG_BASE, SZ_4K);
	core_crg_reg(core_base, ODIN_CORE_CRG_BASE);

	cpu_base = ioremap(ODIN_CPU_CRG_BASE, SZ_64K);
	cpu_crg_reg(cpu_base, ODIN_CPU_CRG_BASE);

	ics_base = ioremap(ODIN_ICS_CRG_BASE, SZ_4K);
	ics_crg_reg(ics_base, ODIN_ICS_CRG_BASE);

	css_base = ioremap(ODIN_CSS_SYS_BASE, SZ_4K);
	css_crg_reg(css_base, ODIN_CSS_SYS_BASE);

	peri_base = ioremap(ODIN_PERI_CRG_BASE, SZ_4K);
	peri_crg_reg(peri_base, ODIN_PERI_CRG_BASE);

	dss_base = ioremap(ODIN_DSS_CRG_BASE, SZ_4K);
	dss_crg_reg(dss_base, ODIN_DSS_CRG_BASE);

	gpu_crg_reg(gpu_base, ODIN_GPU_CRG_BASE);

#ifdef CONFIG_ODIN_TEE
	vsp0_base = ODIN_VSP0_CRG_BASE;
	vsp1_base = ODIN_VSP1_CRG_BASE;
#else
	vsp0_base = ioremap(ODIN_VSP0_CRG_BASE, SZ_4K);
	vsp1_base = ioremap(ODIN_VSP1_CRG_BASE, SZ_4K);
#endif

	odin_vsp0_clk_setup(vsp0_base);
	vsp0_crg_reg(vsp0_base, ODIN_VSP0_CRG_BASE);

	odin_vsp1_clk_setup(vsp1_base);
	vsp1_crg_reg(vsp1_base, ODIN_VSP1_CRG_BASE);

	aud_base = ioremap(ODIN_AUD_CRG_BASE, SZ_4K);
	odin_audio_clk_setup(aud_base);
	aud_crg_reg(aud_base, ODIN_AUD_CRG_BASE);

	odin_clk_enable_check(ALL_BLOCK);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
int __init clk_driver_init(void)
{
	int ret;

	ret = platform_driver_register(&clk_driver);
	if (ret != 0)
		printk("can not register clk drvier!\n");

	return ret;
}
late_initcall(clk_driver_init);
#endif
