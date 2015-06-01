#include <linux/init.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
#include <asm/bug.h>
#include <mach/mt_typedefs.h>
#include <mach/sync_write.h>
#include <mach/mt_dcm.h>
#include <mach/mt_secure_api.h>

//#define DCM_DEFAULT_ALL_OFF

#if defined(__KERNEL__)
#undef INFRACFG_AO_BASE
#undef MCUCFG_BASE
#undef TOPCKGEN_BASE
#define INFRACFG_AO_BASE        0xF0001000
#define MCUCFG_BASE             0xF0200000
#define TOPCKGEN_BASE           0xF0000000
#define DRAMC_BASE              0xF0004000
#else
#define INFRACFG_AO_BASE        0x10001000
#define MCUCFG_BASE             0x10200000
#define TOPCKGEN_BASE           0x10000000
#define DRAMC_BASE              0x10004000
#endif


#define DRAMC_PD_CTRL                   (DRAMC_BASE + 0x1DC)
#define DFS_DRAMC_PD_CTRL               (DRAMC_BASE + 0x828)
#define INFRACFG_TOP_CKMUXSEL           (INFRACFG_AO_BASE +  0x0)
#define INFRACFG_TOP_CKDIV1             (INFRACFG_AO_BASE +  0x8)
#define INFRACFG_TOP_DCMCTL             (INFRACFG_AO_BASE +  0x10)
#define INFRACFG_TOP_DCMDBC             (INFRACFG_AO_BASE +  0x14)
#define INFRACFG_DCMCTL                 (INFRACFG_AO_BASE +  0x50)
#define INFRACFG_INFRA_BUS_DCM_CTRL           (INFRACFG_AO_BASE +  0x70)
#define INFRACFG_PERI_BUS_DCM_CTRL      (INFRACFG_AO_BASE +  0x74)
#define INFRACFG_MEM_DCM_CTRL           (INFRACFG_AO_BASE +  0x78)
#define INFRACFG_DFS_MEM_DCM_CTRL       (INFRACFG_AO_BASE +  0x7C)

#define	TOPCKGEN_CLK_MISC_CFG_0         (TOPCKGEN_BASE +  0x0104)

#define MCUCFG_L2C_SRAM_CTRL            (MCUCFG_BASE + 0x648)
#define MCUCFG_CCI_CLK_CTRL             (MCUCFG_BASE + 0x660)
#define MCUCFG_BUS_FABRIC_DCM_CTRL      (MCUCFG_BASE + 0x668)

#define K2_MCUCFG_BASE (0xf0200000)      //0x1020_0000
#define MP0_AXI_CONFIG          (K2_MCUCFG_BASE + 0x2C)
#define MP1_AXI_CONFIG          (K2_MCUCFG_BASE + 0x22C)
#define ACINACTM                (1<<4)



#if !defined(__KERNEL__)
#define KERN_NOTICE
#define KERN_CONT
#define printk(fmt, args...) dbg_print(fmt, ##args)
#define late_initcall(a)

#define DEFINE_MUTEX(a)  a
#define mutex_lock(a)
#define mutex_unlock(a)

#define IOMEM(a) (a)
#define __raw_readl(a)  (*(volatile unsigned long*)(a))
#define mt_reg_sync_writel(val, addr)  ({ *(unsigned long *)(addr) = val; dsb(); })
#define unlikely(a)  (a)
#define BUG() do { __asm__ __volatile__ ("dsb \n\t"); } while(0)
#else
#define USING_XLOG
#endif // for CTP




#ifdef USING_XLOG

#include <linux/xlog.h>
#define TAG     "Power/dcm"

#define MT6592_DCM_SETTING (1)

//#define DCM_ENABLE_DCM_CFG

#define dcm_err(fmt, args...)                                   \
        xlog_printk(ANDROID_LOG_ERROR, TAG, fmt, ##args)
#define dcm_warn(fmt, args...)                          \
        xlog_printk(ANDROID_LOG_WARN, TAG, fmt, ##args)
#define dcm_info(fmt, args...)                          \
        xlog_printk(ANDROID_LOG_INFO, TAG, fmt, ##args)
#define dcm_dbg(fmt, args...)                                   \
        xlog_printk(ANDROID_LOG_DEBUG, TAG, fmt, ##args)
#define dcm_ver(fmt, args...)                                   \
        xlog_printk(ANDROID_LOG_VERBOSE, TAG, fmt, ##args)

#else /* !USING_XLOG */

#define TAG     "[Power/dcm] "

#define dcm_err(fmt, args...)                   \
        printk(KERN_ERR TAG);                   \
        printk(KERN_CONT fmt, ##args)
#define dcm_warn(fmt, args...)                  \
        printk(KERN_WARNING TAG);               \
        printk(KERN_CONT fmt, ##args)
#define dcm_info(fmt, args...)                  \
        printk(KERN_NOTICE TAG);                \
        printk(KERN_CONT fmt, ##args)
#define dcm_dbg(fmt, args...)                   \
        printk(KERN_INFO TAG);                  \
        printk(KERN_CONT fmt, ##args)
#define dcm_ver(fmt, args...)                   \
        printk(KERN_DEBUG TAG);                 \
        printk(KERN_CONT fmt, ##args)

#endif



/** macro **/
#define and(v, a) ((v) & (a))
#define or(v, o) ((v) | (o))
#define aor(v, a, o) (((v) & (a)) | (o))

#define reg_read(addr)         __raw_readl(IOMEM(addr))
#define reg_write(addr, val)   mt_reg_sync_writel((val), ((void *)addr))

#define DCM_OFF (0)
#define DCM_ON (1)


#define DCM_DEBUG_MON
void debug_mon(int major, int minor, int fqmtr_div);


/** global **/
static DEFINE_MUTEX(dcm_lock);
static unsigned int dcm_initiated = 0;
static int armcore_dcm_fix_en =0;




/*****************************************
 * following is implementation per DCM module.
 * 1. per-DCM function is 1-argu with ON/OFF/MODE option.
 *****************************************/
typedef int (*DCM_FUNC)(int);


/** TOP_DCMCTL
 * 0	0	infra_dcm_enable
 * 4	4	ca7_dcm_enable (ARMPLL_DIV DCM Mode,  0: 26M@IDLE 1: 1/4 or 1/5 or 1/6 @idle depends on clkdiv1_sel")
 * 5	5	ca7_dcm_wfi_enable
 * 6	6	ca7_dcm_wfe_enable
 * 8	8	ca15_dcm_enable (X)
 * 9	9	ca15_dcm_wfi_enable (X)
 * 10	10	ca15_dcm_wfe_enable (X)
 **/
//// FIXME, wait for MARK.LIN to validate register field.
typedef enum {
        ARMCORE_DCM_OFF = DCM_OFF,
        ARMCORE_DCM_MODE1 = DCM_ON,
        ARMCORE_DCM_MODE2 = DCM_ON+1,
} ENUM_ARMCORE_DCM;
int dcm_armcore(ENUM_ARMCORE_DCM mode)
{

        if (mode == ARMCORE_DCM_OFF) {
                //swithc to mode 2, and turn wfi/wfe-enable off
                reg_write(INFRACFG_TOP_DCMCTL,
                          aor(reg_read(INFRACFG_TOP_DCMCTL), ~(0x7<<4), (0<<4)));
#if 0
                reg_write(INFRACFG_TOP_CKDIV1,
                          aor(reg_read(INFRACFG_TOP_CKDIV1), ~(0x1f<<0), 0)); // div by 1/1
#endif
                return 0;
        }

        if (mode == ARMCORE_DCM_MODE2) {
                //switch to mode 2
                reg_write(INFRACFG_TOP_DCMCTL,
                          aor(reg_read(INFRACFG_TOP_DCMCTL), ~(7<<4), (6<<4)));
#if 0
                reg_write(INFRACFG_TOP_CKDIV1,
                          aor(reg_read(INFRACFG_TOP_CKDIV1), ~(0x1f<<0), 0x01d)); // div by 1/1
#endif
        }

        else if (mode == ARMCORE_DCM_MODE1) {
                //switch to mode 1, and mode 2 off
                reg_write(INFRACFG_TOP_DCMCTL,
                          aor(reg_read(INFRACFG_TOP_DCMCTL), ~(0x7<<4), (7<<4)));
        }

        return 0;
}

int dcm_armcore_pll_clkdiv(int pll, int div)
{
        int mux;

        if (pll==2 || pll==3)
                mux = 1<<(pll-2);
        else
                mux = 0;

        reg_write(INFRACFG_TOP_CKMUXSEL, aor(reg_read(INFRACFG_TOP_CKMUXSEL),
                                             ~(3<<0), 0<<0)); //26Mhz

        reg_write(TOPCKGEN_CLK_MISC_CFG_0, aor(reg_read(TOPCKGEN_CLK_MISC_CFG_0), ~(0x03<<4), mux << 4));

        reg_write(INFRACFG_TOP_CKMUXSEL, aor(reg_read(INFRACFG_TOP_CKMUXSEL),
                                             ~(3<<0), pll<<0));
        reg_write(INFRACFG_TOP_CKDIV1,
                  aor(reg_read(INFRACFG_TOP_CKDIV1), ~(0x1f<<0), div << 0));
}



typedef enum {
        MCUSYS_DCM_OFF = DCM_OFF,
        MCUSYS_DCM_ON = DCM_ON,
} ENUM_MCUSYS_DCM;
int dcm_mcusys(ENUM_MCUSYS_DCM on)
{
        if (on == MCUSYS_DCM_OFF) {
                //MCUSYS bus DCM
                mcusys_smc_write(MCUCFG_CCI_CLK_CTRL,
                          aor(reg_read(MCUCFG_CCI_CLK_CTRL), ~(0x1<<8), 0));
                //L2C SRAM DCM
                mcusys_smc_write(MCUCFG_L2C_SRAM_CTRL, aor(reg_read(MCUCFG_L2C_SRAM_CTRL), ~(1<<0), 0));
                //bus_fabric_dcm_ctrl
                mcusys_smc_write(MCUCFG_BUS_FABRIC_DCM_CTRL,
                          aor(reg_read(MCUCFG_BUS_FABRIC_DCM_CTRL), ~(0x0f0f), 0));
        }
        else {
                //MCUSYS bus DCM
                mcusys_smc_write(MCUCFG_CCI_CLK_CTRL, aor(reg_read(MCUCFG_CCI_CLK_CTRL), ~(0x1<<8), (1<<8)));
                //L2C SRAM DCM
                mcusys_smc_write(MCUCFG_L2C_SRAM_CTRL, aor(reg_read(MCUCFG_L2C_SRAM_CTRL), ~(1<<0), (1<<0)));
                //bus_fabric_dcm_ctrl
                mcusys_smc_write(MCUCFG_BUS_FABRIC_DCM_CTRL,
                          aor(reg_read(MCUCFG_BUS_FABRIC_DCM_CTRL), ~(0x0f0f), (0x0f0f)));
        }

        return 0;

}


/** INFRACFG_INFRA_BUS_DCM_CTRL
 * 0	0	infra_dcm_rg_clkoff_en
 * 1	1	infra_dcm_rg_clkslow_en
 * 4	4	infra_dcm_rg_force_on
 * 9	5	infra_dcm_rg_fsel
 * 14	10	infra_dcm_rg_sfsel
 * 19	15	infra_dcm_dbc_rg_dbc_num
 * 20	20	infra_dcm_dbc_rg_dbc_en
 **/
#define INFRA_DCM_DEFAULT_MASK  ((1<<20) | (0x1f<<15) | (0x1f<<10) | (0x1f<<5) | (3<<0))
#define INFRA_DCM_DEFAULT (0x1f0603)

typedef enum {
        INFRA_DCM_OFF = DCM_OFF,
        INFRA_DCM_ON = DCM_ON,
} ENUM_INFRA_DCM;

int dcm_infra(ENUM_INFRA_DCM on)
{
        unsigned int val;

        val = (on == INFRA_DCM_ON) ? 3: 0;
        reg_write(INFRACFG_INFRA_BUS_DCM_CTRL, aor(reg_read(INFRACFG_INFRA_BUS_DCM_CTRL),
                                                   ~(0x03<<0),
                                                   val));

        return 0;
}

int dcm_infra_dbc(int cnt)
{
        reg_write(INFRACFG_INFRA_BUS_DCM_CTRL, aor(reg_read(INFRACFG_INFRA_BUS_DCM_CTRL),
                                                  ~(0x1f << 15),
                                                  (cnt << 15)));

        return 0;
}

/** order is 0~5, respectly 1/1 ~ 1/32 **/
int dcm_infra_rate(unsigned int fsel, unsigned int sfsel)
{
        int val;
        if (fsel > 5 || sfsel > fsel) {
                BUG();
        }

        val = (sfsel == 5) ? 0 : (1 << (14-sfsel));
        val |= (fsel == 5) ? 0 : (1 << (9-fsel));
        reg_write(INFRACFG_INFRA_BUS_DCM_CTRL, aor(reg_read(INFRACFG_INFRA_BUS_DCM_CTRL),
                                                  ~((0x1f << 10) | (0x1f<<5)), val));

        return 0;
}



/** INFRACFG_PERI_BUS_DCM_CTRL
 * 0	0	peri_dcm_rg_clkoff_en
 * 1	1	peri_dcm_rg_clkslow_en
 * 2	2	peri_dcm_rg_force_clkoff
 * 3	3	peri_dcm_rg_force_clkslow
 * 4	4	peri_dcm_rg_force_on
 * 9	5	peri_dcm_rg_fsel
 * 14	10	peri_dcm_rg_sfsel
 * 19	15	peri_dcm_dbc_rg_dbc_num
 * 20	20	peri_dcm_dbc_rg_dbc_en
 * 21	21	re_usb_dcm_en
 * 22	22	re_pmic_dcm_en
 * 27	23	pmic_cnt_mst_rg_sfsel
 * 28	28	re_icusb_dcm_en
 * 29	29	rg_audio_dcm_en
 **/
#define PERI_DCM_DEFAULT_MASK  ((1<<20) | (0x1f<<15) | (0x1f<<10) | (0x1f<<5) | (3<<0))
#define PERI_DCM_DEFAULT (0x001f0603)

#define MISC_DCM_DEFAULT_MASK  ((1<<29) | (1<<28) | (1<<22) | (1<<21))
#define MISC_DCM_DEFAULT (0xb0000000)

#define PMIC_DCM_DEFAULT (1<<22)
#define USB_DCM_DEFAULT (1<<21)
#define ICUSB_DCM_DEFAULT (1<<28)
#define AUDIO_DCM_DEFAULT (1<<29)

typedef enum {
        MISC_DCM_OFF = DCM_OFF,
        PMIC_DCM_OFF = DCM_OFF,
        USB_DCM_OFF = DCM_OFF,
        ICUSB_DCM_OFF = DCM_OFF,
        AUDIO_DCM_OFF = DCM_OFF,

        MISC_DCM_ON = DCM_ON,
        PMIC_DCM_ON = DCM_ON,
        USB_DCM_ON = DCM_ON,
        ICUSB_DCM_ON = DCM_ON,
        AUDIO_DCM_ON = DCM_ON,
} ENUM_MISC_DCM;


/** argu REG, is 1-bit hot value **/
int _dcm_peri_misc(unsigned int reg, int on)
{
        reg_write(INFRACFG_PERI_BUS_DCM_CTRL, aor(reg_read(INFRACFG_PERI_BUS_DCM_CTRL),
                                                  ~reg, (on) ? reg : 0));

        return 0;
}

int dcm_pmic(ENUM_MISC_DCM on)
{
        _dcm_peri_misc(PMIC_DCM_DEFAULT, on);

        return 0;
}
int dcm_usb(ENUM_MISC_DCM on)
{
        _dcm_peri_misc(USB_DCM_DEFAULT, on);

        return 0;
}
int dcm_icusb(ENUM_MISC_DCM on)
{
        _dcm_peri_misc(ICUSB_DCM_DEFAULT, on);

        return 0;
}
int dcm_audio(ENUM_MISC_DCM on)
{
        _dcm_peri_misc(AUDIO_DCM_DEFAULT, on);

        return 0;
}



typedef enum {
        PERI_DCM_OFF = DCM_OFF,
        PERI_DCM_ON = DCM_ON,
} ENUM_PERI_DCM;

int dcm_peri(ENUM_PERI_DCM on)
{
        unsigned int val = (on == PERI_DCM_ON) ? 3 : 0;

        reg_write(INFRACFG_PERI_BUS_DCM_CTRL, aor(reg_read(INFRACFG_PERI_BUS_DCM_CTRL),
                                                  ~(3<<0),
                                                  val));

        return 0;
}


int dcm_peri_dbc(int cnt)
{
        reg_write(INFRACFG_PERI_BUS_DCM_CTRL, aor(reg_read(INFRACFG_PERI_BUS_DCM_CTRL),
                                                  ~(0x1f << 15),
                                                  (cnt << 15)));

        return 0;
}

/** order is 0~5, respectly 1/1 ~ 1/32 **/
int dcm_peri_rate(unsigned int fsel, unsigned int sfsel)
{
        int val;
        if (fsel > 5 || sfsel > fsel) {
                BUG();
        }

        val = (sfsel == 5) ? 0 : (1 << (14-sfsel));
        val |= (fsel == 5) ? 0 : (1 << (9-fsel));
        reg_write(INFRACFG_PERI_BUS_DCM_CTRL, aor(reg_read(INFRACFG_PERI_BUS_DCM_CTRL),
                                                  ~((0x1f << 10) | (0x1f<<5)), val));

        return 0;
}



/** MEM_DCM_CTRL
 *    0	        0	mem_dcm_apb_toggle
 *    5	        1	mem_dcm_apb_sel
 *    6	        6	mem_dcm_force_on
 *    7	        7	mem_dcm_dcm_en
 *    8	        8	mem_dcm_dbc_en
 *    15	9	mem_dcm_dbc_cnt
 *    20	16	mem_dcm_fsel
 *    25	21	mem_dcm_idle_fsel
 *    26	26	mem_dcm_force_off
 *    28	28	phy_cg_off_disable
 *    29	29	pipe_0_cg_off_disable
 *    31	31	mem_dcm_hwcg_off_disable
 **/
#define EMI_3PLL_MODE_DEFAULT_MASK ((0x1f<21) | (0x1f<<16) | (0x3f<<9) | \
                                    (1<<8) | (1<<7) | (1<<6) | (0x1f<<1))
#define EMI_3PLL_MODE_DEFAULT ((0x1f<<21) | (0<<16) | (1<<9) | (1<<8) | (1<<7) | (0<<6) | (0x1f<<1))
#define EMI_3PLL_MODE_DCM_OFF()                                         \
        do {                                                            \
                reg_write(INFRACFG_MEM_DCM_CTRL,                        \
                          aor(reg_read(INFRACFG_MEM_DCM_CTRL),          \
                              ~(EMI_3PLL_MODE_DEFAULT_MASK),            \
                              (EMI_3PLL_MODE_DEFAULT & ~(1<<7))));      \
                                                                        \
                /* toggle */                                            \
                reg_write(INFRACFG_MEM_DCM_CTRL,                        \
                          or(reg_read(INFRACFG_MEM_DCM_CTRL), 1));      \
                reg_write(INFRACFG_MEM_DCM_CTRL,                        \
                          and(reg_read(INFRACFG_MEM_DCM_CTRL), ~1));    \
        } while(0)

#define EMI_3PLL_MODE_DCM_ON()                                          \
        do {                                                            \
                reg_write(INFRACFG_MEM_DCM_CTRL,                        \
                          aor(reg_read(INFRACFG_MEM_DCM_CTRL),          \
                              ~(EMI_3PLL_MODE_DEFAULT_MASK),            \
                              (EMI_3PLL_MODE_DEFAULT)));                \
                                                                        \
                /* toggle */                                            \
                reg_write(INFRACFG_MEM_DCM_CTRL,                        \
                          or(reg_read(INFRACFG_MEM_DCM_CTRL), 1));      \
                reg_write(INFRACFG_MEM_DCM_CTRL,                        \
                          and(reg_read(INFRACFG_MEM_DCM_CTRL), ~1));    \
        } while(0)


#if 0
#define CLK_CFG_0		(0xF0000040)
/*
 * Return value definitions
 *  0: 26M,
 *  1: 3 PLL,
 *  2: 1 PLL
 */
static inline unsigned int mt_get_clk_mem_sel(void)
{
	unsigned int val;

    //CLK_CFG_0(0x10000040)[9:8]
    //clk_mem_sel
    //2'b00:clk26m
    //2'b01:dmpll_ck->3PLL
    //2'b10:ddr_x1_ck->1PLL
    val = (*(volatile unsigned int *)(CLK_CFG_0));
	val = (val>>8) & 0x3;

	return val;
}
#else
unsigned int mt_get_clk_mem_sel(void);
#endif

#define IS_EMI_1PLL_MODE() (mt_get_clk_mem_sel() == 0x2)

/** DFS_MEM_DCM_CTRL
 * 0	0	mem_dcm_apb_toggle
 * 5	1	mem_dcm_apb_sel
 * 6	6	mem_dcm_force_on
 * 7	7	mem_dcm_dcm_en
 * 8	8	mem_dcm_dbc_en
 * 15	9	mem_dcm_dbc_cnt
 * 20	16	mem_dcm_fsel
 * 25	21	mem_dcm_idle_fsel
 * 26	26	mem_dcm_force_off
 * 28	28	phy_cg_off_disable
 * 29	29	pipe_0_cg_off_disable
 * 31	31	mem_dcm_hwcg_off_disable
 **/
#define EMI_1PLL_MODE_DEFAULT_MASK ((1<<28) | (0x1f<21) | (0x1f<<16) | (0x3f<<9) | \
                                    (1<<8) | (1<<7) | (1<<6) | (0x1f<<1))
#define EMI_1PLL_MODE_DEFAULT ((1<<28) | (0x7<<21) | (0<<16) | (1<<9) | \
                               (1<<8) | (1<<7) | (0<<6) | (0x1f<<1))

#define EMI_1PLL_MODE_DCM_OFF()                                         \
        do {                                                            \
                reg_write(INFRACFG_DFS_MEM_DCM_CTRL,                    \
                          aor(reg_read(INFRACFG_DFS_MEM_DCM_CTRL),      \
                              ~(EMI_1PLL_MODE_DEFAULT_MASK),            \
                              (EMI_1PLL_MODE_DEFAULT & ~(1<<7))));      \
                                                                        \
                /* toggle */                                            \
                reg_write(INFRACFG_DFS_MEM_DCM_CTRL,                    \
                          or(reg_read(INFRACFG_DFS_MEM_DCM_CTRL), 1));  \
                reg_write(INFRACFG_DFS_MEM_DCM_CTRL,                    \
                          and(reg_read(INFRACFG_DFS_MEM_DCM_CTRL), ~1)); \
                                                                        \
                /* gate off the 1pll mode clock before mux sel */       \
                if (!IS_EMI_1PLL_MODE())                                \
                        reg_write(INFRACFG_DFS_MEM_DCM_CTRL,            \
                                  or(reg_read(INFRACFG_DFS_MEM_DCM_CTRL), (1<<31))); \
        } while(0)

#define EMI_1PLL_MODE_DCM_ON()                                          \
        do {                                                            \
                /* gate off the 1pll mode clock before mux sel */       \
                if (!IS_EMI_1PLL_MODE())                                \
                        reg_write(INFRACFG_DFS_MEM_DCM_CTRL,            \
                                  or(reg_read(INFRACFG_DFS_MEM_DCM_CTRL), (1<<31))); \
                                                                        \
                reg_write(INFRACFG_DFS_MEM_DCM_CTRL,                    \
                          aor(reg_read(INFRACFG_DFS_MEM_DCM_CTRL),      \
                              ~(EMI_1PLL_MODE_DEFAULT_MASK),            \
                              (EMI_1PLL_MODE_DEFAULT)));                \
                /* ungate the 1pll mode clock  */                       \
                reg_write(INFRACFG_DFS_MEM_DCM_CTRL,                    \
                          and(reg_read(INFRACFG_DFS_MEM_DCM_CTRL), ~(1<<31))); \
                                                                        \
                /* toggle */                                            \
                reg_write(INFRACFG_DFS_MEM_DCM_CTRL,                    \
                          or(reg_read(INFRACFG_DFS_MEM_DCM_CTRL), 1));  \
                reg_write(INFRACFG_DFS_MEM_DCM_CTRL,                    \
                          and(reg_read(INFRACFG_DFS_MEM_DCM_CTRL), ~1)); \
        } while(0)



typedef enum {
        EMI_DCM_OFF = DCM_OFF,
        EMI_DCM_3PLL_MODE = DCM_ON,
        EMI_DCM_1PLL_MODE = (DCM_ON+1),
} ENUM_EMI_DCM;

int dcm_emi(ENUM_EMI_DCM mode)
{
        if (mode == EMI_DCM_OFF) {
                EMI_3PLL_MODE_DCM_OFF();
                EMI_1PLL_MODE_DCM_OFF();

                return 0;
        }

        /* 1PLL mode */
        if (mode == EMI_DCM_1PLL_MODE) {
                EMI_3PLL_MODE_DCM_OFF();
                EMI_1PLL_MODE_DCM_ON();
        }
        else if (mode == EMI_DCM_3PLL_MODE) {
                EMI_3PLL_MODE_DCM_ON();
                EMI_1PLL_MODE_DCM_OFF();
        }

        return 0;
}

int dcm_emi_dbc(ENUM_EMI_DCM mode, int cnt)
{
        if (mode == EMI_DCM_1PLL_MODE) {
                reg_write(INFRACFG_DFS_MEM_DCM_CTRL,
                          aor(reg_read(INFRACFG_DFS_MEM_DCM_CTRL),
                              ~(0x1f<<9), cnt));
        }
        else {
                reg_write(INFRACFG_MEM_DCM_CTRL,
                          aor(reg_read(INFRACFG_MEM_DCM_CTRL),
                              ~(0x1f<<9), cnt));
        }

        return 0;
}



enum {
        ARMCORE_DCM_TYPE  = (1U << 0),
        MCUSYS_DCM_TYPE  = (1U << 1),
        INFRA_DCM_TYPE  = (1U << 2),
        PERI_DCM_TYPE  = (1U << 3),
        EMI_DCM_TYPE  = (1U << 4),
        PMIC_DCM_TYPE  = (1U << 5),
        USB_DCM_TYPE  = (1U << 6),
        ICUSB_DCM_TYPE  = (1U << 7),
        AUDIO_DCM_TYPE  = (1U << 8),

        NR_DCM_TYPE = 9,
};

#define ALL_DCM_TYPE  (ARMCORE_DCM_TYPE | MCUSYS_DCM_TYPE | INFRA_DCM_TYPE | PERI_DCM_TYPE |  \
                       EMI_DCM_TYPE | PMIC_DCM_TYPE | USB_DCM_TYPE | ICUSB_DCM_TYPE | \
                       AUDIO_DCM_TYPE )


typedef struct _dcm {
        int current_state;
        int saved_state;
        int disable_refcnt;
        int default_state;
        DCM_FUNC func;
        int typeid;
        char *name;
} DCM;

static DCM dcm_array[NR_DCM_TYPE] = {
        {
                .typeid = ARMCORE_DCM_TYPE,
                .name = "ARMCORE_DCM",
                .func = (DCM_FUNC)dcm_armcore,
                .current_state = ARMCORE_DCM_MODE1,
                .default_state = ARMCORE_DCM_MODE1,
                .disable_refcnt = 0,
        },
        {
                .typeid = MCUSYS_DCM_TYPE,
                .name = "MCUSYS_DCM",
                .func = (DCM_FUNC)dcm_mcusys,
                .current_state = MCUSYS_DCM_ON,
                .default_state = MCUSYS_DCM_ON,
                .disable_refcnt = 0,
        },
        {
                .typeid = INFRA_DCM_TYPE,
                .name = "INFRA_DCM",
                .func = (DCM_FUNC)dcm_infra,
                .current_state = INFRA_DCM_ON,
                .default_state = INFRA_DCM_ON,
                .disable_refcnt = 0,
        },
        {
                .typeid = PERI_DCM_TYPE,
                .name = "PERI_DCM",
                .func = (DCM_FUNC)dcm_peri,
                .current_state = PERI_DCM_ON,
                .default_state = PERI_DCM_ON,
                .disable_refcnt = 0,
        },
        {
                .typeid = EMI_DCM_TYPE,
                .name = "EMI_DCM",
                .func = (DCM_FUNC)dcm_emi,
                .current_state = EMI_DCM_3PLL_MODE,
                .default_state = EMI_DCM_3PLL_MODE,
                .disable_refcnt = 0,
        },
        {
                .typeid = PMIC_DCM_TYPE,
                .name = "PMIC_DCM",
                .func = (DCM_FUNC)dcm_pmic,
                .current_state = PMIC_DCM_ON,
                .default_state = PMIC_DCM_ON,
                .disable_refcnt = 0,
        },
        {
                .typeid = USB_DCM_TYPE,
                .name = "USB_DCM",
                .func = (DCM_FUNC)dcm_usb,
                .current_state = USB_DCM_ON,
                .default_state = USB_DCM_ON,
                .disable_refcnt = 0,
        },
        {
                .typeid = ICUSB_DCM_TYPE,
                .name = "ICUSB_DCM",
                .func = (DCM_FUNC)dcm_icusb,
                .current_state = ICUSB_DCM_ON,
                .default_state = ICUSB_DCM_ON,
                .disable_refcnt = 0,
        },
        {
                .typeid = AUDIO_DCM_TYPE,
                .name = "AUDIO_DCM",
                .func = (DCM_FUNC)dcm_audio,
                .current_state = AUDIO_DCM_ON,
                .default_state = AUDIO_DCM_ON,
                .disable_refcnt = 0,
        },
};



/*****************************************
 * DCM driver will provide regular APIs :
 * 1. dcm_restore(type) to recovery CURRENT_STATE before any power-off reset.
 * 2. dcm_set_default(type) to reset as cold-power-on init state.
 * 3. dcm_disable(type) to disable all dcm.
 * 4. dcm_set_state(type) to set dcm state.
 * 5. dcm_dump_state(type) to show CURRENT_STATE.
 * 6. /sys/power/dcm_state interface:  'restore', 'disable', 'dump', 'set'. 4 commands.
 *
 * spsecified APIs for workaround:
 * 1. (definitely no workaround now)
 *****************************************/

void dcm_set_default(unsigned int type)
{
        int i;
        DCM *dcm;

        dcm_info("[%s]type:0x%08x\n", __func__, type);

        mutex_lock(&dcm_lock);

        for (i = 0, dcm=&dcm_array[0]; i < NR_DCM_TYPE; i++, dcm++) {
                if (type & dcm->typeid) {
                        /* fixme, hack for while is 1PLL mode. */
                        if (dcm->typeid == EMI_DCM_TYPE) {
                                if (IS_EMI_1PLL_MODE())
                                        dcm->default_state = EMI_DCM_1PLL_MODE;
                        }
                        dcm->saved_state = dcm->current_state = dcm->default_state;
                        dcm->disable_refcnt = 0;
                        dcm->func(dcm->current_state);

                        dcm_info("[%16s 0x%08x] current state:%d (%d)\n",
                                 dcm->name, dcm->typeid, dcm->current_state, dcm->disable_refcnt);

                }
        }

        mutex_unlock(&dcm_lock);
}

void dcm_set_state(unsigned int type, int state)
{
        int i;
        DCM *dcm;

        dcm_info("[%s]type:0x%08x, set:%d\n", __func__, type, state);

        mutex_lock(&dcm_lock);

        for (i = 0, dcm=&dcm_array[0]; type && (i < NR_DCM_TYPE); i++, dcm++) {
                if (type & dcm->typeid) {
                        type &= ~(dcm->typeid);

                        dcm->saved_state = state;
                        if (dcm->disable_refcnt == 0) {
                                dcm->current_state = state;
                                dcm->func(dcm->current_state);
                        }

                        dcm_info("[%16s 0x%08x] current state:%d (%d)\n",
                                 dcm->name, dcm->typeid, dcm->current_state, dcm->disable_refcnt);

                }
        }

        mutex_unlock(&dcm_lock);
}


void dcm_disable(unsigned int type)
{
        int i;
        DCM *dcm;

        dcm_info("[%s]type:0x%08x\n", __func__, type);

        mutex_lock(&dcm_lock);

        for (i = 0, dcm=&dcm_array[0]; type && (i < NR_DCM_TYPE); i++, dcm++) {
                if (type & dcm->typeid) {
                        type &= ~(dcm->typeid);

                        dcm->current_state = DCM_OFF;
                        dcm->disable_refcnt++;
                        dcm->func(dcm->current_state);

                        dcm_info("[%16s 0x%08x] current state:%d (%d)\n",
                                 dcm->name, dcm->typeid, dcm->current_state, dcm->disable_refcnt);

                }
        }

        mutex_unlock(&dcm_lock);

}

void dcm_restore(unsigned int type)
{
        int i;
        DCM *dcm;

        dcm_info("[%s]type:0x%08x\n", __func__, type);

        mutex_lock(&dcm_lock);

        for (i = 0, dcm=&dcm_array[0]; type && (i < NR_DCM_TYPE); i++, dcm++) {
                if (type & dcm->typeid) {
                        type &= ~(dcm->typeid);

                        if (dcm->disable_refcnt > 0)
                                dcm->disable_refcnt--;
                        if (dcm->disable_refcnt == 0) {
                                dcm->current_state = dcm->saved_state;
                                dcm->func(dcm->current_state);
                        }

                        dcm_info("[%16s 0x%08x] current state:%d (%d)\n",
                                 dcm->name, dcm->typeid, dcm->current_state, dcm->disable_refcnt);

                }
        }

        mutex_unlock(&dcm_lock);
}


void dcm_dump_state(int type)
{
        int i;
        DCM *dcm;

        dcm_info("\n******** dcm dump state ********* \n");
        for (i = 0, dcm=&dcm_array[0]; i < NR_DCM_TYPE; i++, dcm++) {
                if (type & dcm->typeid) {
                        dcm_info("[%-16s 0x%08x] current state:%d (%d)\n",
                                 dcm->name, dcm->typeid, dcm->current_state, dcm->disable_refcnt);
                }
        }

        return;
}

#define REG_DUMP(addr) do { dcm_info("%-30s(0x%08lx): 0x%08lx \n", #addr, addr, reg_read(addr)); } while(0)


void dcm_dump_regs(void)
{


        dcm_info("\n******** dcm dump register ********* \n");
        REG_DUMP(INFRACFG_TOP_CKMUXSEL);
        REG_DUMP(INFRACFG_TOP_CKDIV1);
        REG_DUMP(INFRACFG_TOP_DCMCTL);
        REG_DUMP(INFRACFG_TOP_DCMDBC);
        REG_DUMP(INFRACFG_DCMCTL);
        REG_DUMP(INFRACFG_INFRA_BUS_DCM_CTRL);
        REG_DUMP(INFRACFG_PERI_BUS_DCM_CTRL);
        REG_DUMP(INFRACFG_MEM_DCM_CTRL);
        REG_DUMP(INFRACFG_DFS_MEM_DCM_CTRL);

        REG_DUMP(MCUCFG_L2C_SRAM_CTRL);
        REG_DUMP(MCUCFG_CCI_CLK_CTRL);
        REG_DUMP(MCUCFG_BUS_FABRIC_DCM_CTRL);

        return;
}


#if defined (CONFIG_PM)
static ssize_t dcm_state_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        int len = 0;
        char *p = buf;
        int i;
        DCM *dcm;

//        dcm_dump_state(ALL_DCM_TYPE);
        p += sprintf(p, "\n******** dcm dump state ********* \n");
        for (i = 0, dcm=&dcm_array[0]; i < NR_DCM_TYPE; i++, dcm++) {
                p += sprintf(p, "[%-16s 0x%08x] current state:%d (%d)\n",
                             dcm->name, dcm->typeid, dcm->current_state, dcm->disable_refcnt);
        }

        p += sprintf(p, "\n********** dcm_state help *********\n");
        p += sprintf(p, "set:           echo set [mask] [mode] > /sys/power/dcm_state\n");
        p += sprintf(p, "disable:       echo disable [mask] > /sys/power/dcm_state\n");
        p += sprintf(p, "restore:       echo restore [mask] > /sys/power/dcm_state\n");
        p += sprintf(p, "dump:          echo dump [mask] > /sys/power/dcm_state\n");
        p += sprintf(p, "***** [mask] is hexl bit mask of dcm;   \n");
        p += sprintf(p, "***** [mode] is type of DCM to set and retained \n");


        len = p - buf;
        return len;
}

static ssize_t dcm_state_store(struct kobject *kobj, struct kobj_attribute *attr,const char *buf, size_t n)
{
        char cmd[10];
        unsigned int mask;

        if (sscanf(buf, "%s %x", cmd, &mask) == 2) {
                mask &= ALL_DCM_TYPE;

                if (!strcmp(cmd, "restore")) {
                        //dcm_dump_regs();
                        dcm_restore(mask);
                        //dcm_dump_regs();
                } else if (!strcmp(cmd, "disable")) {
                        //dcm_dump_regs();
                        dcm_disable(mask);
                        //dcm_dump_regs();
                } else if (!strcmp(cmd, "dump")) {
                        dcm_dump_state(mask);
                        dcm_dump_regs();
                } else if (!strcmp(cmd, "armcore_fix")) {
                        if (mask == 0){
                                unsigned int val;
                                val = reg_read(MP0_AXI_CONFIG);
                                val = and(val, ~ACINACTM);
                                mcusys_smc_write(MP0_AXI_CONFIG,  val);
                        }
                        armcore_dcm_fix_en = mask;
                } else if (!strcmp(cmd, "set")) {
                        int mode;
                        if (sscanf(buf, "%s %x %d", cmd, &mask, &mode) == 3) {
                                mask &= ALL_DCM_TYPE;

                                dcm_set_state(mask, mode);
                        }
                } else {
                        dcm_info ("SORRY, do not support your command: %s \n", cmd);
                }
                return n;
        }
        else {
                dcm_info ("SORRY, do not support your command. \n");
        }

        return -EINVAL;
}

static struct kobj_attribute dcm_state_attr = {
        .attr = {
                .name = "dcm_state",
                .mode = 0644,
        },
        .show = dcm_state_show,
        .store = dcm_state_store,
};

static int dcm_cpu_notify(struct notifier_block *self,
                          unsigned long action, void *hcpu)
{
        cpumask_t mp1_cpus = {.bits = 0x0f0 };
        cpumask_t mp1_cpu_online;
	int scpu = (long)hcpu;
        unsigned int val;

        if (armcore_dcm_fix_en == 0)
                return NOTIFY_OK;

        if (scpu < 4)
                return NOTIFY_OK;

        cpumask_andnot(&mp1_cpu_online, cpu_online_mask, cpumask_of(scpu));
        cpumask_shift_right(&mp1_cpu_online, &mp1_cpu_online, 4);
        if (!cpumask_empty(&mp1_cpu_online))
                return NOTIFY_OK;


	switch (action) {
        case CPU_DOWN_FAILED:
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		val = reg_read(MP0_AXI_CONFIG);
		val = and(val, ~ACINACTM);
		mcusys_smc_write(MP0_AXI_CONFIG,  val);

                break;


	case CPU_ONLINE:

                if (and(reg_read(MP0_AXI_CONFIG), ACINACTM)) {
                        dcm_info("axi_config:0x%0x \n", reg_read(MP0_AXI_CONFIG));
                        BUG();
                }
		break;

        //case CPU_UP_FAILED:
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		val = reg_read(MP0_AXI_CONFIG);
		val = or(val, ACINACTM);
		mcusys_smc_write(MP0_AXI_CONFIG,  val);

		break;

	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block dcm_cpu_nb = {
	.notifier_call = dcm_cpu_notify,
};





#if defined (DCM_DEBUG_MON)
static ssize_t dcm_debug_mon_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t dcm_debug_mon_store(struct kobject *kobj, struct kobj_attribute *attr,const char *buf, size_t n);
static struct kobj_attribute dcm_debug_mon_attr = {
        .attr = {
                .name = "dcm_debug_mon",
                .mode = 0644,
        },
        .show = dcm_debug_mon_show,
        .store = dcm_debug_mon_store,
};


#endif //#if defined (DCM_DEBUG_MON)

#endif //#if defined (CONFIG_PM)



int mt_dcm_init(void)
{
        if (dcm_initiated)
                return 0;

        /** workaround **/


        dcm_initiated = 1;

        /** fixme, should we set this dramc register for each 3pll/1pll switch?
         * to enable DRAMC_PD_CTRL[25]: DCMEN, otherwise EMI DCM will not work.
         */
        reg_write(DRAMC_PD_CTRL, or(reg_read(DRAMC_PD_CTRL), 1<<25));
        reg_write(DFS_DRAMC_PD_CTRL, or(reg_read(DFS_DRAMC_PD_CTRL), 1<<25));



#if !defined (DCM_DEFAULT_ALL_OFF)
        /** enable all dcm **/
        dcm_set_default(ALL_DCM_TYPE);
#else //#if !defined (DCM_DEFAULT_ALL_OFF)
        dcm_set_state(ALL_DCM_TYPE, DCM_OFF);
#endif //#if !defined (DCM_DEFAULT_ALL_OFF)


#if defined (CONFIG_PM)
        {
                int err = 0;

                err = sysfs_create_file(power_kobj, &dcm_state_attr.attr);

                if (err) {
                        dcm_err("[%s]: fail to create sysfs\n", __func__);
                }
        }

#if defined (DCM_DEBUG_MON)
        {
                int err = 0;

                err = sysfs_create_file(power_kobj, &dcm_debug_mon_attr.attr);

                if (err) {
                        dcm_err("[%s]: fail to create sysfs\n", __func__);
                }
        }
#endif  //#if defined (DCM_DEBUG_MON)
#endif //#if defined (CONFIG_PM)



        // K2 workdaround,
	register_cpu_notifier(&dcm_cpu_nb);


        return 0;
}

late_initcall(mt_dcm_init);


/**** public APIs *****/

void mt_dcm_emi_1pll_mode(void)
{
        mt_dcm_init();
        dcm_set_state(EMI_DCM_TYPE, EMI_DCM_1PLL_MODE);
}

void mt_dcm_emi_off(void)
{
        mt_dcm_init();

        //NOTIC, using dcm_set_state to keep 'off' state, instead of dcm_disable(EMI);
        dcm_set_state(EMI_DCM_TYPE, EMI_DCM_OFF);
}

void mt_dcm_emi_3pll_mode(void)
{
        mt_dcm_init();
        dcm_set_state(EMI_DCM_TYPE, EMI_DCM_3PLL_MODE);
}

void mt_dcm_disable(void)
{
        mt_dcm_init();
        dcm_disable(ALL_DCM_TYPE);
}

void mt_dcm_restore(void)
{
        mt_dcm_init();
        dcm_restore(ALL_DCM_TYPE);
}


EXPORT_SYMBOLE(mt_dcm_emi_1pll_mode);
EXPORT_SYMBOLE(mt_dcm_emi_3pll_mode);
EXPORT_SYMBOLE(mt_dcm_emi_off_mode);
EXPORT_SYMBOLE(mt_dcm_emi_disable);
EXPORT_SYMBOLE(mt_dcm_emi_restore);


#if defined(DCM_DEBUG_MON)
#include "mach/mt_gpio_core.h"

enum _ckgen_ck {
        hd_faxi_ck = 1,
        hf_fddrphycfg_ck,
        f_fpwm_ck,
        hf_fvdec_ck,
        hf_fmm_ck = 5,
        hf_fcamtg_ck,
        f_fuart_ck,
        hf_fspi_ck,
        hf_fmsdc50_0_hclk_ck,
        hf_fmsdc50_0_ck = 10,
        hf_fmsdc30_1_ck,
        hf_fmsdc30_2_ck,
        hf_fmsdc30_3_ck,
        hf_faudio_ck,
        hf_faud_intbus_ck = 15,
        hf_fpmicspi_ck,
        hf_fscp_ck,
        hf_fatb_ck,
        hf_fmjc_ck,
        hf_fdpi0_ck = 20,
        hf_faud_1_ck,
        hf_faud_2_ck,
        hf_fscam_ck,
        hf_fmfg_ck,
        mem_clkmux_ck = 25,
        mem_dcm_ck = 26,
};

enum _abist_clk {
        AD_ARMCA7PLL_754M_CORE_CK = 2,
        AD_OSC_CK,
        AD_MAIN_H546M_CK,
        AD_MAIN_H364M_CK = 5,
        AD_MAIN_H218P4M_CK,
        AD_MAIN_H156M_CK,
        AD_UNIV_178P3M_CK,
        AD_UNIV_48M_CK,
        AD_UNIV_624M_CK =10,
        AD_UNIV_416M_CK,
        AD_UNIV_249P6M_CK,
        AD_APLL1_180P6336M_CK,
        AD_APLL2_196P608M_CK,
        AD_LTEPLL_FS26M_CK = 15,
        rtc32k_ck_i,
        AD_MMPLL_700M_CK,
        AD_VENCPLL_410M_CK,
        AD_TVDPLL_594M_CK,
        AD_MPLL_208M_CK =20,
        AD_MSDCPLL_806M_CK,
        AD_USB_48M_CK,
        AD_MEMPLL_MONCLK,
        MIPI_DSI_TST_CK,
        AD_PLLGP_TST_CK =25,
};


enum {
        ABIST_CLK = 1<<0, //mem_dcm_ck(27), hd_faxi_ck(1), hf_fddrphycfg_ck(2)
        CKGEN_CK = 1<<1,
        INFRA_PERI_AXI = 1<<2,
        INFRA_MEM = 1<<3,

};



#define	CLK_MISC_CFG_0  (0xf0000000 + 0x0104) //ckgen base
#define CLK_DBG_CFG (0xf000010c)
#define CLK26CALI_0 (0xf0000220)
#define CLK26CALI_1 (0xf0000224)
#define TOP_MISC_TEST_MODE_CFG  (0xf0011000)

#define INFRA_AO_DBG_CON0 (0xf0001500)
#define INFRA_AO_DBG_CON1 (0xf0001504)
#define INFRA_AO_DBG_CON2 (0xf0001508)
#define INFRA_AO_DBG_CON3 (0xf000150C)


#define GPIO_DIR_OUT 1
#define GPIO_DIR_OUT 0

void debug_mon(int major, int minor, int fqmtr_div)
{
        dcm_info("[%s ] %d %d %d \n", __func__, major, minor, fqmtr_div);

        if (major & INFRA_PERI_AXI) {
                reg_write(INFRA_AO_DBG_CON0, 1); //(select infra_ao debug mon port 0 input1
                reg_write(INFRA_AO_DBG_CON1, 1); //(select infra_ao debug mon port 1 input1
                reg_write(INFRA_AO_DBG_CON2, 1); //(select infra_ao debug mon port 2 input1
                reg_write(INFRA_AO_DBG_CON3, 1); //(select infra_ao debug mon port 3 input1

                //*TOP_MISC_TEST_MODE_CFG &= 0xffe0ffff; *TOP_MISC_TEST_MODE_CFG |= 0x00030000;
                reg_write(TOP_MISC_TEST_MODE_CFG,
                          aor(reg_read(TOP_MISC_TEST_MODE_CFG), 0xffe0ffff, 0x00030000));

                ////hd_66m_infra_bus_mclk_ck_div2 , PAD_CMPCLK
                mt_set_gpio_dir_base(35, GPIO_DIR_OUT);
                mt_set_gpio_mode_base(35, 7);

                ////hd_66m_peri_bus_mclk_ck_div2, PAD_WB_RSTB
                mt_set_gpio_dir_base(109, GPIO_DIR_OUT);
                mt_set_gpio_mode_base(109, 7);

                ////infra_dcmck_slow_down, PAD_WB_SEN
                mt_set_gpio_dir_base(108, GPIO_DIR_OUT);
                mt_set_gpio_mode_base(108, 7);

                ////peri_dcmck_slow_down, PAD_WB_SDATA
                mt_set_gpio_dir_base(107, GPIO_DIR_OUT);
                mt_set_gpio_mode_base(107, 7);

        }

        if (major & INFRA_MEM) {
                reg_write(INFRA_AO_DBG_CON0, 0xf);

                //*TOP_MISC_TEST_MODE_CFG &= 0xffe0ffff; *TOP_MISC_TEST_MODE_CFG |= 0x00020000;
                reg_write(TOP_MISC_TEST_MODE_CFG,
                          aor(reg_read(TOP_MISC_TEST_MODE_CFG), 0xffe0ffff, 0x00020000));

                //hg_fdramc_hclk_ck_div8, PAD_WB_CTRL0
                mt_set_gpio_dir_base(98, GPIO_DIR_OUT);
                mt_set_gpio_mode_base(98, 7);
        }


        if (major & CKGEN_CK) {
                // gpio setting (WB_CTRL0)
                mt_set_gpio_dir_base(98, GPIO_DIR_OUT);
                mt_set_gpio_mode_base(98, 7);

                //2. *TOP_MISC_TEST_MODE_CFG = ( (*TOP_MISC_TEST_MODE_CFG) & (0xfc00ffff) ) | 0x18c0000;
                reg_write(TOP_MISC_TEST_MODE_CFG, aor(reg_read(TOP_MISC_TEST_MODE_CFG), 0xfc00ffff, 0x18c0000));

                //mux
                reg_write(CLK_DBG_CFG, aor(reg_read(CLK_DBG_CFG), ~0x03, 1)); //CKGEN_CK

                // clock source
                reg_write(CLK_DBG_CFG, aor(reg_read(CLK_DBG_CFG), ~(0x1f<<8), minor<<8));

                //fqmtr divider
                fqmtr_div--;
                reg_write(CLK_MISC_CFG_0, aor(reg_read(CLK_MISC_CFG_0), ~((0x0ff<<24)), fqmtr_div << 24));


                // enable fqmtr
                reg_write(CLK26CALI_0, aor(reg_read(CLK26CALI_0), ~(1<<12), 1<<12));

        }


        if (major & ABIST_CLK) {
                // gpio setting (I2S0_MCK)
                mt_set_gpio_dir_base(130, GPIO_DIR_OUT);
                mt_set_gpio_mode_base(130, 7);

                reg_write(TOP_MISC_TEST_MODE_CFG, aor(reg_read(TOP_MISC_TEST_MODE_CFG), 0xfc00ffff, 0x18c0000));

                //mux
                reg_write(CLK_DBG_CFG, aor(reg_read(CLK_DBG_CFG), ~0x03, 0)); //ABIST_CLK

                // clock source
                reg_write(CLK_DBG_CFG, aor(reg_read(CLK_DBG_CFG), ~(0x1f<<16), minor<<16));

                //fqmtr divider
                fqmtr_div--;
                if (AD_ARMCA7PLL_754M_CORE_CK) {
                        int arm_k1 = 0x0f;   //arm_k1 is pre-dividor.
                        fqmtr_div >>= 4;
                        reg_write(CLK_MISC_CFG_0, aor(reg_read(CLK_MISC_CFG_0), ~((0x0ff<<16)), (arm_k1 << 16)));
                }
                reg_write(CLK_MISC_CFG_0, aor(reg_read(CLK_MISC_CFG_0), ~((0x0ff<<24)), (fqmtr_div << 24)));


                // enable fqmtr
                reg_write(CLK26CALI_0, aor(reg_read(CLK26CALI_0), ~(1<<12), 1<<12));
        }
}


static ssize_t dcm_debug_mon_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{

        int len = 0;
        char *p = buf;

        p += sprintf(p, "\n********** dcm_debug_mon help *********\n");
        p += sprintf(p, "emi/armclk/bus:  echo [which] > /sys/power/dcm_debug_mon\n");
        p += sprintf(p, "select:  echo [major] [minor] [divider] > /sys/power/dcm_debug_mon\n");


        dcm_info("\n"
                 "MAJOR: \n"
                 "        ABIST_CLK = 1<<0, \n"
                 "        CKGEN_CK = 1<<1,\n"
                 "        INFRA_PERI_AXI = 1<<2,\n"
                 "        INFRA_MEM = 1<<3,\n");

        dcm_info("\n"
                 "enum _ckgen_ck { \n"
                 "        hd_faxi_ck = 1, \n"
                 "        hf_fddrphycfg_ck, \n"
                 "        f_fpwm_ck, \n"
                 "        hf_fvdec_ck, \n"
                 "        hf_fmm_ck = 5, \n"
                 "        hf_fcamtg_ck, \n"
                 "        f_fuart_ck, \n"
                 "        hf_fspi_ck, \n"
                 "        hf_fmsdc50_0_hclk_ck, \n"
                 "        hf_fmsdc50_0_ck = 10, \n"
                 "        hf_fmsdc30_1_ck, \n"
                 "        hf_fmsdc30_2_ck, \n"
                 "        hf_fmsdc30_3_ck, \n"
                 "        hf_faudio_ck, \n"
                 "        hf_faud_intbus_ck = 15, \n"
                 "        hf_fpmicspi_ck, \n"
                 "        hf_fscp_ck, \n"
                 "        hf_fatb_ck, \n"
                 "        hf_fmjc_ck, \n"
                 "        hf_fdpi0_ck = 20, \n"
                 "        hf_faud_1_ck, \n"
                 "        hf_faud_2_ck, \n"
                 "        hf_fscam_ck, \n"
                 "        hf_fmfg_ck, \n"
                 "        mem_clkmux_ck = 25, \n"
                 "        mem_dcm_ck = 26, \n"
                 "}; \n");

        dcm_info("\n"
                 "enum _abist_clk { \n"
                 "        AD_ARMCA7PLL_754M_CORE_CK = 2, \n"
                 "        AD_OSC_CK, \n"
                 "        AD_MAIN_H546M_CK, \n"
                 "        AD_MAIN_H364M_CK = 5, \n"
                 "        AD_MAIN_H218P4M_CK, \n"
                 "        AD_MAIN_H156M_CK, \n"
                 "        AD_UNIV_178P3M_CK, \n"
                 "        AD_UNIV_48M_CK, \n"
                 "        AD_UNIV_624M_CK =10, \n"
                 "        AD_UNIV_416M_CK, \n"
                 "        AD_UNIV_249P6M_CK, \n"
                 "        AD_APLL1_180P6336M_CK, \n"
                 "        AD_APLL2_196P608M_CK, \n"
                 "        AD_LTEPLL_FS26M_CK = 15, \n"
                 "        rtc32k_ck_i, \n"
                 "        AD_MMPLL_700M_CK, \n"
                 "        AD_VENCPLL_410M_CK, \n"
                 "        AD_TVDPLL_594M_CK, \n"
                 "        AD_MPLL_208M_CK =20, \n"
                 "        AD_MSDCPLL_806M_CK, \n"
                 "        AD_USB_48M_CK, \n"
                 "        AD_MEMPLL_MONCLK, \n"
                 "        MIPI_DSI_TST_CK, \n"
                 "        AD_PLLGP_TST_CK =25, \n"
                 "}; \n");



        len = p - buf;
        return len;

}


static ssize_t dcm_debug_mon_store(struct kobject *kobj, struct kobj_attribute *attr,const char *buf, size_t n)
{
        char cmd[10];
        unsigned int mask;

        if (sscanf(buf, "%s", cmd, &mask) == 1) {
                if (!strcmp(cmd, "emi")) {
                        int div = 16;
                        debug_mon(CKGEN_CK, mem_dcm_ck, div);
                        dcm_info("dbg_mon_A0 (WB_CTRL0, 98): mem_dcm_ck/%d\n", div);

                } else if (!strcmp(cmd, "armclk")) {
                        int div = 160;
                        debug_mon(ABIST_CLK, AD_ARMCA7PLL_754M_CORE_CK, div);
                        dcm_info("dbg_mon_B0 (I2S_MCK, 130): arm_clk/%d \n", div);

                } else if (!strcmp(cmd, "bus")) {
                        int div = 2;
                        debug_mon(INFRA_PERI_AXI, 0, div);
                        dcm_info("hd_66m_infra_bus_mclk_ck_div2 PAD_CMPCLK, GPIO35 \n");
                        dcm_info("hd_66m_peri_bus_mclk_ck_div2	PAD_WB_RSTB, GPIO109 \n");
                        dcm_info("infra_dcmck_slow_down	PAD_WB_SEN, GPIO108 \n");
                        dcm_info("peri_dcmck_slow_down	PAD_WB_SDATA, GPIO107 \n");
                        dcm_info("divider(%d) \n", div);

                } else if (!strcmp(cmd, "set")) {
                        int major, minor, div;
                        if (sscanf(buf, "%d %d %d", &major, &minor, &div) == 3) {
                                debug_mon(major, minor, div);
                        }
                }
                return n;
        }

        return -EINVAL;
}



#endif //#if defined (DCM_DEBUG_MON)

