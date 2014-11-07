#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clk-private.h>
#include <linux/spinlock.h>
#include <linux/ioport.h>
#include <linux/platform_data/odin_tz.h>

/*SMS*/
#define ISP_PLL_CON0 			0x0E0
#define ISP_PLL_CON1			0x0E4
#define ISP_PLL_LOCKTIME		0x0E8
#define ISP_PLL_LOCK_COUNT		0x0EC
#define NOC_PLL_CON0			0x0120
#define NOC_PLL_CON1			0x0124
#define NOC_PLL_LOCKTIME		0x0128
#define NOC_PLL_LOCK_COUNT		0x012C
#define OSC_ENABLE				0x01D0
#define DSS_PLL_CON0			0x0190
#define DSS_PLL_CON1			0x0194
#define DSS_PLL_LOCKTIME		0x0198
#define DSS_PLL_LOCK_COUNT		0x019C
#define GPU_PLL_CON0			0x0200
#define GPU_PLL_CON1			0x0204
#define GPU_PLL_LOCKTIME		0x0208
#define GPU_PLL_LOCK_COUNT		0x020C
#define DP1_PLL_CON0	 		0x04B0
#define DP1_PLL_CON1	 		0x04B4
#define DP1_PLL_LOCKTIME		0x04B8
#define DP1_PLL_LOCK_COUNT		0x04BC
#define DP2_PLL_CON0	 		0x04C0
#define DP2_PLL_CON1	 		0x04C4
#define DP2_PLL_LOCKTIME		0x04C8
#define DP2_PLL_LOCK_COUNT		0x04CC

/*CPU*/
#define CPU_CFG_REG0			0x100
#define CPU_CFG_REG1			0x104
#define CPU_CFG_REG2			0x108
#define CPU_CFG_REG3			0x10C
#define CPU_CFG_REG4			0x110
#define CPU_CFG_REG5			0x114
#define CPU_CFG_REG6			0x118
#define CPU_CFG_REG7			0x11C
#define CPU_CFG_REG8			0x120
#define CPU_CFG_REG9			0x140
#define CPU_CFG_REG10			0x144
#define CPU_CFG_REG11			0x148

/*GPU*/
#define GPU_CRG_REG0			0x000
#define GPU_CRG_REG1			0x004
#define GPU_CRG_REG2			0x008
#define GPU_CRG_REG3			0x00C
#define GPU_CRG_REG4			0x10C
#define GPU_CRG_REG5			0x200
#define GPU_CRG_REG6			0x204
#define GPU_CRG_REG7			0x208
#define GPU_CRG_REG8			0x20C
#define GPU_CRG_REG9			0xF00

/*CCORE*/
#define SCFG_CLK_SRC_SEL		0x0000
#define SCFG_CLK_SRC_MSK		0x0004
#define SCFG_CLK_DIV_VAL		0x0008
#define SCFG_CLK_OUT_ENA		0x000C
#define SNOC_CLK_SRC_SEL		0x0100
#define SNOC_CLK_SRC_MSK		0x0104
#define SNOC_CLK_DIV_VAL		0x0108
#define SNOC_CLK_OUT_ENA		0x010C
#define ANOC_CLK_SRC_SEL		0x0200
#define ANOC_CLK_SRC_MSK		0x0204
#define ANOC_CLK_DIV_VAL		0x0208
#define ANOC_CLK_OUT_ENA		0x020C
#define GDNOC_CLK_SRC_SEL		0x0300
#define GDNOC_CLK_SRC_MSK		0x0304
#define GDNOC_CLK_DIV_VAL		0x0308
#define GDNOC_CLK_OUT_ENA		0x030C
#define MEM_PLL_CON0	 		0x0400
#define MEM_PLL_CON1	 		0x0404
#define MEM_PLL_CON2 			0x0408
#define NPLL_CON	 			0x0500
#define NPLL_DSS_CON	 		0x0504
#define CORE_SWRESETN	 		0x0F00

/*ICS*/
#define ICS_HPLL_CON0			0x000
#define ICS_HPLL_CON1			0x004
#define ICS_HPLL_LOCKTIME		0x008
#define ICS_HPLL_LOCK_COUNT		0x00C
#define ICS_CRG_HM_SEL     		0x010
#define ICS_CRG_HM_MASK       	0x014
#define ICS_CRG_HM_DIV        	0x018
#define ICS_CRG_HM_GATE       	0x01C
#define HSIC_CRG_HM_SEL       	0x020
#define HSIC_CRG_HM_MASK      	0x024
#define HSIC_CRG_HM_DIV       	0x028
#define HSIC_CRG_HM_GATE      	0x02C
#define ICS_CCLK_DIV          	0x030
#define ICS_CCLK_GATE         	0x034
#define HSI_HCLK_GATE         	0x038
#define USBHSIC_HCLK_GATE     	0x03C
#define USB30_HCLK_GATE       	0x040
#define C2C_ACLK_GATE         	0x044
#define C2C_LLI_NOC_CLK_GATE  	0x048
#define LLI_ACLK_GATE         	0x04C
#define UFS_ACLK_GATE         	0x050
#define USBHSIC_CLK_DIV       	0x054
#define USBHSIC_CLK_GATE      	0x058
#define USB30_CLK_DIV         	0x05C
#define USB30_CLK_GATE        	0x060
#define USB30_REF_DIV         	0x064
#define USB30_REF_GATE        	0x068
#define HSI_CLK_DIV           	0x06C
#define HSI_CLK_GATE          	0x070
#define C2C_CLK_GATE          	0x074
#define LLI_CLKSVC_MASK       	0x078
#define LLI_CLKSVC_DIV        	0x07C
#define LLI_CLKSVC_GATE       	0x080
#define UFS_CLKSVC_GATE       	0x084
#define LLI_PHY_CLK_GATE      	0x088
#define USB30_PHY_CLK_GATE    	0x08C
#define USBHSIC_REFCLKDIG_MASK	0x090
#define USBHSIC_REFCLKDIG_DIV 	0x094
#define USBHSIC_REFCLKDIG_GATE	0x098
#define ICS_SWRESET				0x09C

/*CSS*/
#define RESET_CTRL				0x000
#define CLK_CTRL				0x004
#define MIPI_CLK_CTRL			0x008
#define AXI_CLK_CTRL			0x00C
#define AHB_CLK_CTRL			0x010
#define ISP_CLK_CTRL			0x014
#define VNR_CLK_CTRL			0x018
#define FD_CLK_CTRL				0x024
#define PIX_CLK_CTRL			0x028
#define MEM_DS_CTRL				0x02C

/*PERI*/
#define CFG_PNOC_ACLK	 		0x000
#define CFG_CRG_PCLK	 		0x004
#define CFG_WT_CLK	 			0x008
#define CFG_PWM_PCLK	 		0x00C
#define CFG_WDT_PCLK	 		0x010
#define CFG_TMR_PCLK0	 		0x014
#define CFG_TMR_PCLK1	 		0x018
#define CFG_G0_GPIO_PCLK		0x01C
#define CFG_G1_GPIO_PCLK		0x020
#define CFG_SRAM_HCLK	 		0x024
#define CFG_PDMA_HCLK	 		0x028
#define CFG_SDC_ACLK	 		0x02C
#define CFG_SDIO_ACLK0		 	0x030
#define CFG_SDIO_ACLK1	 		0x034
#define CFG_EMMC_ACLK		 	0x038
#define CFG_E2NAND_ACLK		 	0x03C
#define CFG_I2C_PCLK0	 		0x040
#define CFG_I2C_PCLK1	 		0x044
#define CFG_I2C_PCLK2		 	0x048
#define CFG_I2C_PCLK3		 	0x04C
#define CFG_SPI_PCLK0		 	0x050
#define CFG_UART_PCLK0		 	0x058
#define CFG_UART_PCLK1		 	0x05C
#define CFG_UART_PCLK2		 	0x060
#define CFG_UART_PCLK3		 	0x064
#define CFG_UART_PCLK4		 	0x068
#define CFG_SDC_SCLK		 	0x06C
#define CFG_SDIO_SCLK0		 	0x070
#define CFG_SDIO_SCLK1		 	0x074
#define CFG_EMMC_SCLK		 	0x078
#define CFG_E2NAND_SCLK		 	0x07C
#define CFG_I2C_SCLK0		 	0x080
#define CFG_I2C_SCLK1		 	0x084
#define CFG_I2C_SCLK2		 	0x088
#define CFG_I2C_SCLK3		 	0x08C
#define CFG_SPI_SCLK0		 	0x090
#define CFG_UART_SCLK0	 		0x098
#define CFG_UART_SCLK1	 		0x09C
#define CFG_UART_SCLK2	 		0x0A0
#define CFG_UART_SCLK3	 		0x0A4
#define CFG_UART_SCLK4	 		0x0A8
#define PERI_SW_RST0	 		0x100
#define PERI_SW_RST1	 		0x104
#define PERI_SW_RST2	 		0x108

/*DSS*/
#define DSS_CORE_CLK_SEL		0x0000
#define DSS_CORE_CLK_ENABLE		0x0004
#define DSS_OTHER_CONTROL		0x0008
#define DSS_CLK_DIV_VAL_0		0x000c
#define DSS_CLK_DIV_VAL_1		0x0010
#define DSS_CLK_DIV_UPDATE		0x0014
#define DSS_SWRESET				0x1000

/*VSP*/
#define VSP_SW_RSTN_C_CCLK		0x0000
#define VSP_SW_RSTN_E_CCLK		0x0004
#define VSP_C_CCLKSRC_SEL		0x0200
#define VSP_C_CCLKSRC_MASK		0x0204
#define VSP_C_CCLK_DIV			0x0208
#define VSP_C_CCLK_GATE			0x020C
#define VSP_E_CCLKSRC_SEL		0x0220
#define VSP_E_CCLKSRC_MASK		0x0224
#define VSP_E_CCLK_DIV			0x0228
#define VSP_E_CCLK_GATE			0x022C
#define VSP_ACLKSRC_SEL			0x0240
#define VSP_ACLKSRC_MASK 		0x0244
#define VSP_ACLK_DIV			0x0248
#define VSP_ACLK_GATE			0x024C
#define VSP_PCLKSRC_SEL			0x0250
#define VSP_PCLKSRC_MASK		0x0254
#define VSP_PCLK_DIV			0x0258
#define VSP_PCLK_GATE			0x025C

#define VSP_SW_RSTN_D_CCLK		0x0008
#define VSP_SW_RSTN_I_CCLK		0x000C
#define VSP_D_CCLKSC_SEL		0x0210
#define VSP_D_CCLKSC_MASK		0x0214
#define VSP_D_CCLK_DIV			0x0218
#define VSP_D_CCLK_GATE			0x021C
#define VSP_I_CCLKSC_SEL		0x0230
#define VSP_I_CCLKSC_MASK		0x0234
#define VSP_I_CCLK_DIV			0x0238
#define VSP_I_CCLK_GATE			0x023C
#define VSP_ACLKSC_SEL			0x0240
#define VSP_ACLKSC_MASK			0x0244
#define VSP_ACLK_DIV			0x0248
#define VSP_ACLK_GATE			0x024C
#define VSP_PCLKSC_SEL			0x0250
#define VSP_PCLKSC_MASK			0x0254
#define VSP_PCLK_DIV			0x0258
#define VSP_PCLK_GATE			0x025C

/*AUD*/
#define AUD_PLL_CON0		 	0x000
#define AUD_PLL_CON1	 		0x004
#define AUD_PLL_LOCKTIME		0x008
#define AUD_PLL_LOCK_COUNT		0x00c
#define DSP_CCLKSRC_SEL	 		0x010
#define DSP_CCLKSRC_MASK		0x014
#define DSP_CCLK_DIV	 		0x018
#define DSP_CCLK_GATE0		 	0x01c
#define AUD_ACLK_DIV	 		0x020
#define AUD_ACLK_GATE	 		0x024
#define AUD_PCLK_DIV		 	0x028
#define AUD_PCLK_GATE	 		0x02c
#define ASYNC_PCLKSRC_SEL		0x030
#define ASYNC_PCLK_MASK	 		0x034
#define ASYNC_PCLK_DIV	 		0x038
#define ASYNC_PCLK_GATE	 		0x03c
#define SLIMBUS_SRC_SEL	 		0x040
#define SLIMBUS_MASK	 		0x044
#define SLIMBUS_DIV	 			0x048
#define SLIMBUS_GATE	 		0x04c
#define I2S0_SCLKSRC_SEL		0x050
#define I2S0_SCLK_MASK	 		0x054
#define I2S0_SCLK_DIV		 	0x058
#define I2S0_SCLK_GATE0	 		0x05c
#define I2S1_SCLKSRC_SEL		0x060
#define I2S1_SCLK_MASK	 		0x064
#define I2S1_SCLK_DIV	 		0x068
#define I2S1_SCLK_GATE0	 		0x06c
#define I2S2_SCLKSRC_SEL		0x070
#define I2S2_SCLK_MASK	 		0x074
#define I2S2_SCLK_DIV	 		0x078
#define I2S2_SCLK_GATE0	 		0x07c
#define I2S3_SCLKSRC_SEL		0x080
#define I2S3_SCLK_MASK		 	0x084
#define I2S3_SCLK_DIV	 		0x088
#define I2S3_SCLK_GATE0	 		0x08c
#define I2S4_SCLKSRC_SEL		0x090
#define I2S4_SCLK_MASK		 	0x094
#define I2S4_SCLK_DIV	 		0x098
#define I2S4_SCLK_GATE0	 		0x09c
#define I2S5_SCLKSRC_SEL		0x0a0
#define I2S5_SCLK_MASK	 		0x0a4
#define I2S5_SCLK_DIV	 		0x0a8
#define I2S5_SCLK_GATE0	 		0x0ac
#define SWRESET_AUD1	 		0x100
#define SWRESET_AUD2	 		0x104

#define ODIN_PLL_TABLE_BASE		0x0003F020

#define __MMIO_P2V(x)	(((x) & 0xFFFFFF) | (((x) & 0x10000000) >> 7) | 0xF8000000)
#define MMIO_P2V(x)	((void __iomem *)__MMIO_P2V(x))

#define WODEN_CRG_BASE 		0x200F2000
#define WODEN_CRG_CORE 		0x200F2120
#define WODEN_CPU_CFG_BASE 	0x200E0000
#define WODEN_AUD_CRG_BASE 	0x34670000

#define MACH_WODEN 0
#define MACH_ODIN  1

#define DVFS_DIV_ONLY 0
#define DVFS_PLL_DIV 1
#define DVFS_PLL_ONLY 2

/*PLL type*/
#define CPU_PLL_CLK 			0
#define GPU_PLL_CLK 			1
#define COMM_PLL_CLK 			2
#define AUD_PLL_CLK 			3
#define ICS_PLL_CLK 			4
#define MEM_PLL_CLK 			5
#define ISP_PLL_CLK 			6
#define DP2_PLL_CLK 			7

/*Block type*/
#define CORE_BLOCK 				0
#define CPU_BLOCK 				1
#define ICS_BLOCK 				2
#define CSS_BLOCK 				3
#define PERI_BLOCK 				4
#define DSS_BLOCK 				5
#define GPU_BLOCK 				6
#define VSP0_BLOCK 				7
#define VSP1_BLOCK 				8
#define AUD_BLOCK 				9
#define SMS_BLOCK 				10
#define ALL_BLOCK 				11

/*Second PLL type*/
#define CA15_PLL_CLK 			0
#define CA7_PLL_CLK 			1

/*ODIN CRG BASE*/
#define ODIN_PERI_CRG_BASE 	0x20020000
#define ODIN_GPU_CRG_BASE 	0x3A801000
#define ODIN_VSP0_CRG_BASE 	0x320A0000
#define ODIN_VSP1_CRG_BASE 	0x321A0000
#define ODIN_AUD_CRG_BASE 	0x34670000
#define ODIN_ICS_CRG_BASE 	0x3FF00000
#define ODIN_CSS_SYS_BASE 	0x330F0000
#define ODIN_DSS_CRG_BASE 	0x31400000
#define ODIN_SMS_CRG_BASE 	0x200F2000
#define ODIN_CPU_CRG_BASE 	0x200E0000
#define ODIN_CORE_CRG_BASE 	0x3C000000

#define AUD_CLK_RATE_12 		12288000
#define AUD_CLK_RATE_24 		24576000
#define AUD_CLK_RATE_49 		49152000
#define AUD_CLK_RATE_98 		98304000
#define AUD_CLK_RATE_8			8192000
#define AUD_CLK_RATE_16 		16384000
#define AUD_CLK_RATE_32 		32768000
#define AUD_CLK_RATE_11 		11289600
#define AUD_CLK_RATE_22 		22579200
#define AUD_CLK_RATE_45 		45158400
#define AUD_CLK_RATE_90 		90316800

#define NON_SECURE 0
#define SECURE 1

#define SAVE_REG(x) {.off_set = (x)}


extern unsigned long s_req_rate;
extern int s_r_flag;

struct lj_pll {
	struct clk_hw hw;
	unsigned long req_rate;
	void __iomem *reg;
	void __iomem *sel_reg;
	void __iomem *lock_time_reg;
	void __iomem *div_reg;
	void __iomem *div_ud_reg;
	u32		p_reg;
	u32		p_sel_reg;
	u32		p_lock_time_reg;
	u32		p_div_reg;
	u32		p_div_ud_reg;
	u8		byp_shift;
	u8		pdb_shift;
	u8		div_shift;
	u8		div_width;
	u8		div_ud_bit;
	u8		mach_type;
	u8		pll_type;
	u8		dvfs_flag;
	spinlock_t *lock;
};

struct aud_pll {
	struct clk_hw hw;
	unsigned long req_rate;
	void __iomem *reg;
	void __iomem *lock_time_reg;
	u32		p_reg;
	u32		p_lock_time_reg;
	u8		pdb_shift;
	u8		mach_type;
	u8		pll_type;
	spinlock_t *lock;
};

struct odin_clk_gate {
	struct clk_hw hw;
	void __iomem	*reg;
	u32		p_reg;
	u8		bit_idx;
	u8		flags;
	spinlock_t	*lock;
};

struct odin_clk_mux {
	struct clk_hw	hw;
	void __iomem	*reg;
	u32		p_reg;
	u8		shift;
	u8		width;
	u8		flags;
	spinlock_t	*lock;
};

struct odin_clk_divider {
	struct clk_hw	hw;
	void __iomem	*reg;
	void __iomem	*update_reg;
	u32		p_reg;
	u32		p_update_reg;
	u8		shift;
	u8		width;
	u8		update_bit;
	u8		flags;
	u8		mach_type;
	u8		pll_type;
	const struct clk_div_table	*table;
	spinlock_t	*lock;
};

struct clk_pll_table {
	unsigned long clk_rate;
	u32 reg_val;
	unsigned long aud_val;
};

struct reg_save{
	//void __iomem	*reg;
	u32 off_set;
	u32 reg_val;
};

extern struct clk_pll_table woden_cpu_clk_t[];
extern struct clk_pll_table woden_comm_clk_t[];

extern struct clk_pll_table *odin_ca15_clk_t;
extern struct clk_pll_table *odin_ca7_clk_t;
extern struct clk_pll_table *odin_mem_clk_t;
extern struct clk_pll_table *odin_gpu_clk_t;
extern struct clk_pll_table *odin_isp_clk_t;
extern struct clk_pll_table *odin_dp2_clk_t;
extern struct clk_pll_table *odin_comm_clk_t;

extern int clk_resume_test(void);

extern int woden_clk_count(void);

extern void print_clk_info(void);

extern int mb_volt_set(u32 addr, u32 val);

extern int mb_get_pll_table(u32 pll_type);

extern int mb_dvfs_item_set(u32 table_index, u32 freq, u32 voltage, u32 pll);

extern u32 mb_clk_set_pll(u32 pll_reg, u32 lock_byp_reg, u32 div_reg,
		u32 divud_req_rate, u32 scp_index);

extern u32 mb_clk_set_div(u32 div_reg, u32 update_reg, u32 sft_wid_ud_bit,
		u32 div_val, unsigned long req_rate);

extern int mb_reg_write(u32 val, u32 reg);

extern u32 mb_reg_read(u32 reg);

extern int mb_clk_probe(void);

extern int __clk_reg_update(struct device *dev, struct clk *clk);

extern struct clk *__odin_clk_register(struct device *dev, struct clk_hw *hw, void __iomem *reg,
		u32 p_reg, u8 secure_flag);

extern int get_table_size(struct clk_pll_table *table);

extern u64 divisor_64(u64 dividend, u64 divisor);

extern struct clk_pll_table *ljpll_search_table(struct clk_pll_table *table, int array_size,
		unsigned long rate);

extern int clk_reg_store(u32 base_addr, struct reg_save *rs, int count, size_t size, u8 secure_flag);

extern int clk_reg_restore(u32 base_addr, struct reg_save *rs, int count, size_t size,
							u8 secure_flag);

extern int clk_reg_update(u8 block_type, u8 secure_flag);

extern int clk_change_parent(struct clk *clk, char *parent_name);

extern int clk_set_par_rate(struct clk *clk, char *parent_name, unsigned long req_rate);

extern void odin_get_all_pll_table(void);

struct clk *odin_clk_register_apll(struct device *dev, const char *name, const char *parent_name,
		int flags, void __iomem *reg, void __iomem *lock_time_reg, u32 p_reg, u32 p_lock_time_reg,
		u8 pdb_shift, u8 mach_type, u8 pll_type, u8 secure_flag, spinlock_t *lock);

struct clk *odin_clk_register_ljpll(struct device *dev, const char *name,
		const char *parent_name, int flags, void __iomem *reg, void __iomem *sel_reg,
		void __iomem *lock_time_reg, u32 p_reg, u32 p_sel_reg, u32 p_lock_time_reg, u8 byp_shift,
		u8 pdb_shift, u8 mach_type, u8 pll_type, u8 secure_flag, spinlock_t *lock);

struct clk *odin_clk_register_ljpll_dvfs(struct device *dev, const char *name,
		const char *parent_name, int flags, void __iomem *reg, void __iomem *sel_reg,
		void __iomem *lock_time_reg, void __iomem *div_reg, void __iomem *div_ud_reg, u32 p_reg,
		u32 p_sel_reg, u32 p_lock_time_reg, u32 p_div_reg, u32 p_div_ud_reg,  u8 byp_shift,
		u8 pdb_shift, u8 div_shift, u8 div_width, u8 div_ud_bit, u8 mach_type, u8 pll_type,
		u8 secure_flag, spinlock_t *lock);

struct clk *odin_clk_register_div(struct device *dev, const char *name, const char *parent_name,
		unsigned long flags, void __iomem *reg, void __iomem *update_reg,  u32 p_reg, u32 p_update_reg,
		u8 shift, u8 width, u8 update_bit, u8 mach_type, u8 pll_type, u8 clk_divider_flags,
		u8 secure_flag, spinlock_t *lock);

struct clk *odin_clk_register_mux(struct device *dev, const char *name, const char **parent_names,
		u8 num_parents, unsigned long flags, void __iomem *reg, u32 p_reg, u8 shift, u8 width,
		u8 clk_mux_flags, u8 secure_flag, spinlock_t *lock);

struct clk *odin_clk_register_gate(struct device *dev, const char *name, const char *parent_name,
		unsigned long flags, void __iomem *reg, u32 p_reg, u8 bit_idx, u8 clk_gate_flags,
		u8 secure_flag, spinlock_t *lock);

#define DEFINE_ODIN_CLK(_name, _ops, _flags, _p_reg, _secure_flag, _parent_names,		\
		_parents)					\
	static struct clk _name = {				\
		.name = #_name,					\
		.ops = &_ops,					\
		.hw = &_name##_hw.hw,				\
		.parent_names = _parent_names,			\
		.num_parents = ARRAY_SIZE(_parent_names),	\
		.parents = _parents,				\
		.flags = _flags | CLK_IS_BASIC,			\
		.reg_val = _p_reg,			\
		.secure_flag = _secure_flag,			\
	}

extern struct clk_ops lj_pll_ops;

#define _DEFINE_LJ_PLL(_name, _parent_name, _parent_ptr, \
				_flags, _reg, _sel_reg, _lock_time_reg, _div_reg, _div_ud_reg, _p_reg, \
				_p_sel_reg, _p_lock_time_reg, _p_div_reg, _p_div_ud_reg, _byp_shift, \
				_pdb_shift, _div_shift, _div_width, _div_ud_bit, _mach_type, _pll_type, \
				_secure_flag, _lock) \
	static struct clk _name;				\
	static const char *_name##_parent_names[] = {			\
		_parent_name,					\
	};							\
	static struct clk *_name##_parents[] = {		\
		_parent_ptr,					\
	};							\
	static struct lj_pll _name##_hw = {			\
		.hw = {						\
			.clk = &_name,				\
		},						\
		.reg = _reg,					\
		.sel_reg = _sel_reg,					\
		.lock_time_reg = _lock_time_reg,					\
		.div_reg = _div_reg,					\
		.div_ud_reg = _div_ud_reg,					\
		.p_reg = _p_reg,					\
		.p_sel_reg = _p_sel_reg,					\
		.p_lock_time_reg = _p_lock_time_reg,					\
		.p_div_reg = _p_div_reg,					\
		.p_div_ud_reg = _p_div_ud_reg,					\
		.byp_shift = _byp_shift,				\
		.pdb_shift = _pdb_shift,				\
		.div_shift = _div_shift,				\
		.div_width = _div_width,				\
		.div_ud_bit = _div_ud_bit,				\
		.mach_type = _mach_type,				\
		.pll_type = _pll_type,				\
		.dvfs_flag = 0,				\
		.lock = _lock,					\
	};							\
	DEFINE_ODIN_CLK(_name, lj_pll_ops, _flags, _p_reg, _secure_flag,		\
			_name##_parent_names, _name##_parents);

#define DEFINE_LJ_PLL(_name, _parent_name, _parent_ptr, \
				_flags, _reg, _sel_reg, _lock_time_reg, _p_reg, \
				_p_sel_reg, _p_lock_time_reg, _byp_shift, \
				_pdb_shift, _mach_type, _pll_type, _secure_flag, _lock) \
		_DEFINE_LJ_PLL(_name, _parent_name, _parent_ptr, \
				_flags, _reg, _sel_reg, _lock_time_reg, NULL, NULL, _p_reg, \
				_p_sel_reg, _p_lock_time_reg, 0x0, 0x0, _byp_shift, \
				_pdb_shift, 0, 0, 0, _mach_type, _pll_type, \
				_secure_flag, _lock)

#define DEFINE_LJ_PLL_DVFS(_name, _parent_name, _parent_ptr, \
				_flags, _reg, _sel_reg, _lock_time_reg, _div_reg, _div_ud_reg, _p_reg, \
				_p_sel_reg, _p_lock_time_reg, _p_div_reg, _p_div_ud_reg, _byp_shift, \
				_pdb_shift, _div_shift, _div_width, div_ud_bit, _mach_type, _pll_type, \
				_secure_flag, _lock) \
		_DEFINE_LJ_PLL(_name, _parent_name, _parent_ptr, \
				_flags, _reg, _sel_reg, _lock_time_reg, _div_reg, _div_ud_reg, _p_reg, \
				_p_sel_reg, _p_lock_time_reg, _p_div_reg, _p_div_ud_reg, _byp_shift, \
				_pdb_shift, _div_shift, _div_width, div_ud_bit, _mach_type, _pll_type, \
				_secure_flag, _lock)


extern struct clk_ops aud_pll_ops;

#define DEFINE_AUD_PLL(_name, _parent_name, _parent_ptr, \
				_flags, _reg, _lock_time_reg, _p_reg, \
				_p_lock_time_reg, _pdb_shift, \
				_mach_type, _pll_type, _secure_flag, _lock) \
	static struct clk _name;				\
	static const char *_name##_parent_names[] = {			\
		_parent_name,					\
	};							\
	static struct clk *_name##_parents[] = {		\
		_parent_ptr,					\
	};							\
	static struct aud_pll _name##_hw = {			\
		.hw = {						\
			.clk = &_name,				\
		},						\
		.reg = _reg,					\
		.lock_time_reg = _lock_time_reg,					\
		.p_reg = _p_reg,					\
		.p_lock_time_reg = _p_lock_time_reg,					\
		.pdb_shift = _pdb_shift,				\
		.mach_type = _mach_type,				\
		.pll_type = _pll_type,				\
		.lock = _lock,					\
	};							\
	DEFINE_ODIN_CLK(_name, aud_pll_ops, _flags, _p_reg, _secure_flag,		\
			_name##_parent_names, _name##_parents);

extern struct clk_ops odin_clk_divider_ops;

#define _DEFINE_ODIN_CLK_DIVIDER(_name, _parent_name, _parent_ptr,	\
				_flags, _reg, _update_reg, _p_reg, _p_update_reg, _shift, _width, _update_bit,	\
				_mach_type, _pll_type, _divider_flags, _table, _secure_flag, _lock)	\
	static struct clk _name;				\
	static const char *_name##_parent_names[] = {		\
		_parent_name,					\
	};							\
	static struct clk *_name##_parents[] = {		\
		_parent_ptr,					\
	};							\
	static struct odin_clk_divider _name##_hw = {		\
		.hw = {						\
			.clk = &_name,				\
		},						\
		.reg = _reg,					\
		.update_reg = _update_reg,					\
		.p_reg = _p_reg,					\
		.p_update_reg = _p_update_reg,					\
		.shift = _shift,				\
		.width = _width,				\
		.update_bit = _update_bit,				\
		.flags = _divider_flags,			\
		.mach_type = _mach_type,				\
		.pll_type = _pll_type,				\
		.table = _table,				\
		.lock = _lock,					\
	};							\
	DEFINE_ODIN_CLK(_name, odin_clk_divider_ops, _flags, _p_reg, _secure_flag,		\
			_name##_parent_names, _name##_parents);

#define DEFINE_ODIN_CLK_DIVIDER(_name, _parent_name, _parent_ptr,	\
				_flags, _reg, _update_reg, _p_reg, _p_update_reg, _shift, _width,	\
				_update_bit, _mach_type, _pll_type, _divider_flags, _secure_flag, _lock)		\
	_DEFINE_ODIN_CLK_DIVIDER(_name, _parent_name, _parent_ptr,	\
				_flags, _reg, _update_reg, _p_reg, _p_update_reg, _shift, _width,	\
				_update_bit, _mach_type, _pll_type, _divider_flags, NULL, _secure_flag, _lock)

extern struct clk_ops odin_clk_mux_ops;

#define DEFINE_ODIN_CLK_MUX(_name, _parent_names, _parents, _flags,	\
				_reg, _p_reg, _shift, _width,		\
				_mux_flags, _secure_flag, _lock)		\
	static struct clk _name;				\
	static struct odin_clk_mux _name##_hw = {			\
		.hw = {						\
			.clk = &_name,				\
		},						\
		.reg = _reg,					\
		.p_reg = _p_reg,					\
		.shift = _shift,				\
		.width = _width,				\
		.flags = _mux_flags,				\
		.lock = _lock,					\
	};							\
	DEFINE_ODIN_CLK(_name, odin_clk_mux_ops, _flags, _p_reg, _secure_flag, \
			_parent_names, _parents);

extern struct clk_ops odin_clk_gate_ops;

#define DEFINE_ODIN_CLK_GATE(_name, _parent_name, _parent_ptr,	\
				_flags, _reg, _p_reg, _bit_idx,		\
				_gate_flags, _secure_flag, _lock)		\
	static struct clk _name;				\
	static const char *_name##_parent_names[] = {		\
		_parent_name,					\
	};							\
	static struct clk *_name##_parents[] = {		\
		_parent_ptr,					\
	};							\
	static struct odin_clk_gate _name##_hw = {			\
		.hw = {						\
			.clk = &_name,				\
		},						\
		.reg = _reg,					\
		.p_reg = _p_reg,					\
		.bit_idx = _bit_idx,				\
		.flags = _gate_flags,				\
		.lock = _lock,					\
	};							\
	DEFINE_ODIN_CLK(_name, odin_clk_gate_ops, _flags, _p_reg, _secure_flag,			\
			_name##_parent_names, _name##_parents);
