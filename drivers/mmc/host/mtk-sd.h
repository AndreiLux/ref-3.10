#ifndef __MTK_SD_H
#define __MTK_SD_H

#include <linux/bitops.h>
#include <linux/mmc/host.h>
#include <linux/tracepoint.h>

#include <mach/sync_write.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_clkmgr.h>

#define MAX_GPD_NUM         (1 + 1)	/* one null gpd */
#define MAX_BD_NUM          (1024)
#define MAX_BD_PER_GPD      (MAX_BD_NUM)
#define HOST_MAX_NUM        (5)
#define CLK_SRC_MAX_NUM		(1)
#define PERI_MSDC0_PDN      (MT_CG_PERI_MSDC20_1_PDN)
#define PERI_MSDC1_PDN      (MT_CG_PERI_MSDC20_2_PDN)
#define PERI_MSDC2_PDN      (MT_CG_PERI_MSDC30_1_PDN)
#define PERI_MSDC3_PDN      (MT_CG_PERI_MSDC30_2_PDN)
#define PERI_MSDC4_PDN      (MT_CG_PERI_MSDC30_3_PDN)

#define CUST_EINT_POLARITY_LOW              0
#define CUST_EINT_POLARITY_HIGH             1
#define CUST_EINT_DEBOUNCE_DISABLE          0
#define CUST_EINT_DEBOUNCE_ENABLE           1
#define CUST_EINT_EDGE_SENSITIVE            0
#define CUST_EINT_LEVEL_SENSITIVE           1
#define SDIO_ERROR_BYPASS
/* //////////////////////////////////////////////////////////////////////////// */


#define EINT_MSDC2_INS_NUM              14
#define EINT_MSDC2_INS_DEBOUNCE_CN      1
#define EINT_MSDC2_INS_POLARITY         CUST_EINT_POLARITY_LOW
#define EINT_MSDC2_INS_SENSITIVE        CUST_EINT_LEVEL_SENSITIVE
#define EINT_MSDC2_INS_DEBOUNCE_EN      CUST_EINT_DEBOUNCE_ENABLE

#define EINT_MSDC1_INS_NUM              15
#define EINT_MSDC1_INS_DEBOUNCE_CN      1
#define EINT_MSDC1_INS_POLARITY         CUST_EINT_POLARITY_LOW
#define EINT_MSDC1_INS_SENSITIVE        CUST_EINT_LEVEL_SENSITIVE
#define EINT_MSDC1_INS_DEBOUNCE_EN      CUST_EINT_DEBOUNCE_ENABLE

/* #define MSDC_CLKSRC_REG     (0xf100000C) */
#define MSDC_DESENSE_REG	(0xf0007070)

#ifdef CONFIG_MT8135_FPGA
#define FPGA_PLATFORM
#endif
#define MSDC_AUTOCMD12          (0x0001)
#define MSDC_AUTOCMD23          (0x0002)
#define MSDC_AUTOCMD19          (0x0003)

#ifdef CONFIG_ARCH_MT8135
#define MSDC_USE_DCT_TOOL       (1)
#else
#define MSDC_USE_DCT_TOOL       (0)
#endif

/*--------------------------------------------------------------------------*/
/* Common Macro                                                             */
/*--------------------------------------------------------------------------*/
#define REG_ADDR(x)                 (base + OFFSET_##x)

/*--------------------------------------------------------------------------*/
/* Common Definition                                                        */
/*--------------------------------------------------------------------------*/
#define MSDC_FIFO_SZ            (128)
#define MSDC_FIFO_THD           (64)	/* (128) */
#define MSDC_NUM                (4)

#define MSDC_MS                 (0)	/* No memory stick mode, 0 use to gate clock */
#define MSDC_SDMMC              (1)

#define MSDC_MODE_UNKNOWN       (0)
#define MSDC_MODE_PIO           (1)
#define MSDC_MODE_DMA_BASIC     (2)
#define MSDC_MODE_DMA_DESC      (3)
#define MSDC_MODE_DMA_ENHANCED  (4)
#define MSDC_MODE_MMC_STREAM    (5)

#define MSDC_BUS_1BITS          (0)
#define MSDC_BUS_4BITS          (1)
#define MSDC_BUS_8BITS          (2)

#define MSDC_BRUST_8B           (3)
#define MSDC_BRUST_16B          (4)
#define MSDC_BRUST_32B          (5)
#define MSDC_BRUST_64B          (6)

#define MSDC_PIN_PULL_NONE      MT_PIN_PULL_DISABLE
#define MSDC_PIN_PULL_DOWN      MT_PIN_PULL_ENABLE_DOWN
#define MSDC_PIN_PULL_UP        MT_PIN_PULL_ENABLE_UP

#define MSDC_AUTOCMD12          (0x0001)
#define MSDC_AUTOCMD23          (0x0002)
#define MSDC_AUTOCMD19          (0x0003)

#define MSDC_EMMC_BOOTMODE0     (0)	/* Pull low CMD mode */
#define MSDC_EMMC_BOOTMODE1     (1)	/* Reset CMD mode */

enum {
	RESP_NONE = 0,
	RESP_R1 = 1,
	RESP_R2 = 2,
	RESP_R3 = 3,
	RESP_R4 = 4,
	RESP_R5 = 1,
	RESP_R6 = 1,
	RESP_R7 = 1,
	RESP_R1B = 7
};

/*--------------------------------------------------------------------------*/
/* Register Offset                                                          */
/*--------------------------------------------------------------------------*/
#define OFFSET_MSDC_CFG         (0x0)
#define OFFSET_MSDC_IOCON       (0x04)
#define OFFSET_MSDC_PS          (0x08)
#define OFFSET_MSDC_INT         (0x0c)
#define OFFSET_MSDC_INTEN       (0x10)
#define OFFSET_MSDC_FIFOCS      (0x14)
#define OFFSET_MSDC_TXDATA      (0x18)
#define OFFSET_MSDC_RXDATA      (0x1c)
#define OFFSET_SDC_CFG          (0x30)
#define OFFSET_SDC_CMD          (0x34)
#define OFFSET_SDC_ARG          (0x38)
#define OFFSET_SDC_STS          (0x3c)
#define OFFSET_SDC_RESP0        (0x40)
#define OFFSET_SDC_RESP1        (0x44)
#define OFFSET_SDC_RESP2        (0x48)
#define OFFSET_SDC_RESP3        (0x4c)
#define OFFSET_SDC_BLK_NUM      (0x50)
#define OFFSET_SDC_CSTS         (0x58)
#define OFFSET_SDC_CSTS_EN      (0x5c)
#define OFFSET_SDC_DCRC_STS     (0x60)
#define OFFSET_EMMC_CFG0        (0x70)
#define OFFSET_EMMC_CFG1        (0x74)
#define OFFSET_EMMC_STS         (0x78)
#define OFFSET_EMMC_IOCON       (0x7c)
#define OFFSET_SDC_ACMD_RESP    (0x80)
#define OFFSET_SDC_ACMD19_TRG   (0x84)
#define OFFSET_SDC_ACMD19_STS   (0x88)
#define OFFSET_MSDC_DMA_SA      (0x90)
#define OFFSET_MSDC_DMA_CA      (0x94)
#define OFFSET_MSDC_DMA_CTRL    (0x98)
#define OFFSET_MSDC_DMA_CFG     (0x9c)
#define OFFSET_MSDC_DBG_SEL     (0xa0)
#define OFFSET_MSDC_DBG_OUT     (0xa4)
#ifdef CONFIG_ARCH_MT8135
#define OFFSET_MSDC_DMA_LEN     (0xa8)	/* add by jie.wu for register change */
#endif
#define OFFSET_MSDC_PATCH_BIT   (0xb0)
#define OFFSET_MSDC_PATCH_BIT1  (0xb4)
#define OFFSET_MSDC_PAD_CTL0    (0xe0)
#define OFFSET_MSDC_PAD_CTL1    (0xe4)
#define OFFSET_MSDC_PAD_CTL2    (0xe8)
#define OFFSET_MSDC_PAD_TUNE    (0xec)
#define OFFSET_MSDC_DAT_RDDLY0  (0xf0)
#define OFFSET_MSDC_DAT_RDDLY1  (0xf4)
#define OFFSET_MSDC_HW_DBG      (0xf8)
#define OFFSET_MSDC_VERSION     (0x100)
#define OFFSET_MSDC_ECO_VER     (0x104)

/*--------------------------------------------------------------------------*/
/* Register Address                                                         */
/*--------------------------------------------------------------------------*/

/* common register */
#define MSDC_CFG                REG_ADDR(MSDC_CFG)
#define MSDC_IOCON              REG_ADDR(MSDC_IOCON)
#define MSDC_PS                 REG_ADDR(MSDC_PS)
#define MSDC_INT                REG_ADDR(MSDC_INT)
#define MSDC_INTEN              REG_ADDR(MSDC_INTEN)
#define MSDC_FIFOCS             REG_ADDR(MSDC_FIFOCS)
#define MSDC_TXDATA             REG_ADDR(MSDC_TXDATA)
#define MSDC_RXDATA             REG_ADDR(MSDC_RXDATA)
#define MSDC_PATCH_BIT0         REG_ADDR(MSDC_PATCH_BIT)

/* sdmmc register */
#define SDC_CFG                 REG_ADDR(SDC_CFG)
#define SDC_CMD                 REG_ADDR(SDC_CMD)
#define SDC_ARG                 REG_ADDR(SDC_ARG)
#define SDC_STS                 REG_ADDR(SDC_STS)
#define SDC_RESP0               REG_ADDR(SDC_RESP0)
#define SDC_RESP1               REG_ADDR(SDC_RESP1)
#define SDC_RESP2               REG_ADDR(SDC_RESP2)
#define SDC_RESP3               REG_ADDR(SDC_RESP3)
#define SDC_BLK_NUM             REG_ADDR(SDC_BLK_NUM)
#define SDC_CSTS                REG_ADDR(SDC_CSTS)
#define SDC_CSTS_EN             REG_ADDR(SDC_CSTS_EN)
#define SDC_DCRC_STS            REG_ADDR(SDC_DCRC_STS)

/* emmc register*/
#define EMMC_CFG0               REG_ADDR(EMMC_CFG0)
#define EMMC_CFG1               REG_ADDR(EMMC_CFG1)
#define EMMC_STS                REG_ADDR(EMMC_STS)
#define EMMC_IOCON              REG_ADDR(EMMC_IOCON)

/* auto command register */
#define SDC_ACMD_RESP           REG_ADDR(SDC_ACMD_RESP)
#define SDC_ACMD19_TRG          REG_ADDR(SDC_ACMD19_TRG)
#define SDC_ACMD19_STS          REG_ADDR(SDC_ACMD19_STS)

/* dma register */
#define MSDC_DMA_SA             REG_ADDR(MSDC_DMA_SA)
#define MSDC_DMA_CA             REG_ADDR(MSDC_DMA_CA)
#define MSDC_DMA_CTRL           REG_ADDR(MSDC_DMA_CTRL)
#define MSDC_DMA_CFG            REG_ADDR(MSDC_DMA_CFG)
#ifdef CONFIG_ARCH_MT8135
#define MSDC_DMA_LEN            REG_ADDR(MSDC_DMA_LEN)
#endif

/* pad ctrl register */
#define MSDC_PAD_CTL0           REG_ADDR(MSDC_PAD_CTL0)
#define MSDC_PAD_CTL1           REG_ADDR(MSDC_PAD_CTL1)
#define MSDC_PAD_CTL2           REG_ADDR(MSDC_PAD_CTL2)

/* data read delay */
#define MSDC_DAT_RDDLY0         REG_ADDR(MSDC_DAT_RDDLY0)
#define MSDC_DAT_RDDLY1         REG_ADDR(MSDC_DAT_RDDLY1)

/* debug register */
#define MSDC_DBG_SEL            REG_ADDR(MSDC_DBG_SEL)
#define MSDC_DBG_OUT            REG_ADDR(MSDC_DBG_OUT)

/* misc register */
#define MSDC_PATCH_BIT          REG_ADDR(MSDC_PATCH_BIT)
#define MSDC_PATCH_BIT1         REG_ADDR(MSDC_PATCH_BIT1)
#define MSDC_PAD_TUNE           REG_ADDR(MSDC_PAD_TUNE)
#define MSDC_HW_DBG             REG_ADDR(MSDC_HW_DBG)
#define MSDC_VERSION            REG_ADDR(MSDC_VERSION)
#define MSDC_ECO_VER            REG_ADDR(MSDC_ECO_VER)	/* ECO Version */

/*--------------------------------------------------------------------------*/
/* Register Mask                                                            */
/*--------------------------------------------------------------------------*/

/* MSDC_CFG mask */
#define MSDC_CFG_MODE           (0x1  << 0)	/* RW */
#define MSDC_CFG_CKPDN          (0x1  << 1)	/* RW */
#define MSDC_CFG_RST            (0x1  << 2)	/* RW */
#define MSDC_CFG_PIO            (0x1  << 3)	/* RW */
#define MSDC_CFG_CKDRVEN        (0x1  << 4)	/* RW */
#define MSDC_CFG_BV18SDT        (0x1  << 5)	/* RW */
#define MSDC_CFG_BV18PSS        (0x1  << 6)	/* R  */
#define MSDC_CFG_CKSTB          (0x1  << 7)	/* R  */
#define MSDC_CFG_CKDIV          (0xff << 8)	/* RW */
#define MSDC_CFG_CKMOD          (0x3  << 16)	/* RW */

/* MSDC_IOCON mask */
#define MSDC_IOCON_SDR104CKS    (0x1  << 0)	/* RW */
#define MSDC_IOCON_RSPL         (0x1  << 1)	/* RW */
#define MSDC_IOCON_DSPL         (0x1  << 2)	/* RW */
#define MSDC_IOCON_DDLSEL       (0x1  << 3)	/* RW */
#define MSDC_IOCON_DDR50CKD     (0x1  << 4)	/* RW */
#define MSDC_IOCON_DSPLSEL      (0x1  << 5)	/* RW */
#define MSDC_IOCON_W_DSPL       (0x1  << 8)	/* RW */
#define MSDC_IOCON_D0SPL        (0x1  << 16)	/* RW */
#define MSDC_IOCON_D1SPL        (0x1  << 17)	/* RW */
#define MSDC_IOCON_D2SPL        (0x1  << 18)	/* RW */
#define MSDC_IOCON_D3SPL        (0x1  << 19)	/* RW */
#define MSDC_IOCON_D4SPL        (0x1  << 20)	/* RW */
#define MSDC_IOCON_D5SPL        (0x1  << 21)	/* RW */
#define MSDC_IOCON_D6SPL        (0x1  << 22)	/* RW */
#define MSDC_IOCON_D7SPL        (0x1  << 23)	/* RW */
#define MSDC_IOCON_RISCSZ       (0x3  << 24)	/* RW */

/* MSDC_PS mask */
#define MSDC_PS_CDEN            (0x1  << 0)	/* RW */
#define MSDC_PS_CDSTS           (0x1  << 1)	/* R  */
#define MSDC_PS_CDDEBOUNCE      (0xf  << 12)	/* RW */
#define MSDC_PS_DAT             (0xff << 16)	/* R  */
#define MSDC_PS_CMD             (0x1  << 24)	/* R  */
#define MSDC_PS_WP              (0x1UL << 31)	/* R  */

/* MSDC_INT mask */
#define MSDC_INT_MMCIRQ         (0x1  << 0)	/* W1C */
#define MSDC_INT_CDSC           (0x1  << 1)	/* W1C */
#define MSDC_INT_ACMDRDY        (0x1  << 3)	/* W1C */
#define MSDC_INT_ACMDTMO        (0x1  << 4)	/* W1C */
#define MSDC_INT_ACMDCRCERR     (0x1  << 5)	/* W1C */
#define MSDC_INT_DMAQ_EMPTY     (0x1  << 6)	/* W1C */
#define MSDC_INT_SDIOIRQ        (0x1  << 7)	/* W1C */
#define MSDC_INT_CMDRDY         (0x1  << 8)	/* W1C */
#define MSDC_INT_CMDTMO         (0x1  << 9)	/* W1C */
#define MSDC_INT_RSPCRCERR      (0x1  << 10)	/* W1C */
#define MSDC_INT_CSTA           (0x1  << 11)	/* R */
#define MSDC_INT_XFER_COMPL     (0x1  << 12)	/* W1C */
#define MSDC_INT_DXFER_DONE     (0x1  << 13)	/* W1C */
#define MSDC_INT_DATTMO         (0x1  << 14)	/* W1C */
#define MSDC_INT_DATCRCERR      (0x1  << 15)	/* W1C */
#define MSDC_INT_ACMD19_DONE    (0x1  << 16)	/* W1C */
#define MSDC_INT_DMA_BDCSERR    (0x1  << 17)	/* W1C */
#define MSDC_INT_DMA_GPDCSERR   (0x1  << 18)	/* W1C */
#define MSDC_INT_DMA_PROTECT    (0x1  << 19)	/* W1C */

/* MSDC_INTEN mask */
#define MSDC_INTEN_MMCIRQ       (0x1  << 0)	/* RW */
#define MSDC_INTEN_CDSC         (0x1  << 1)	/* RW */
#define MSDC_INTEN_ACMDRDY      (0x1  << 3)	/* RW */
#define MSDC_INTEN_ACMDTMO      (0x1  << 4)	/* RW */
#define MSDC_INTEN_ACMDCRCERR   (0x1  << 5)	/* RW */
#define MSDC_INTEN_DMAQ_EMPTY   (0x1  << 6)	/* RW */
#define MSDC_INTEN_SDIOIRQ      (0x1  << 7)	/* RW */
#define MSDC_INTEN_CMDRDY       (0x1  << 8)	/* RW */
#define MSDC_INTEN_CMDTMO       (0x1  << 9)	/* RW */
#define MSDC_INTEN_RSPCRCERR    (0x1  << 10)	/* RW */
#define MSDC_INTEN_CSTA         (0x1  << 11)	/* RW */
#define MSDC_INTEN_XFER_COMPL   (0x1  << 12)	/* RW */
#define MSDC_INTEN_DXFER_DONE   (0x1  << 13)	/* RW */
#define MSDC_INTEN_DATTMO       (0x1  << 14)	/* RW */
#define MSDC_INTEN_DATCRCERR    (0x1  << 15)	/* RW */
#define MSDC_INTEN_ACMD19_DONE  (0x1  << 16)	/* RW */
#define MSDC_INTEN_DMA_BDCSERR  (0x1  << 17)	/* RW */
#define MSDC_INTEN_DMA_GPDCSERR (0x1  << 18)	/* RW */
#define MSDC_INTEN_DMA_PROTECT  (0x1  << 19)	/* RW */

/* MSDC_FIFOCS mask */
#define MSDC_FIFOCS_RXCNT       (0xff << 0)	/* R */
#define MSDC_FIFOCS_TXCNT       (0xff << 16)	/* R */
#define MSDC_FIFOCS_CLR         (0x1UL << 31)	/* RW */

/* SDC_CFG mask */
#define SDC_CFG_SDIOINTWKUP     (0x1  << 0)	/* RW */
#define SDC_CFG_INSWKUP         (0x1  << 1)	/* RW */
#define SDC_CFG_BUSWIDTH        (0x3  << 16)	/* RW */
#define SDC_CFG_SDIO            (0x1  << 19)	/* RW */
#define SDC_CFG_SDIOIDE         (0x1  << 20)	/* RW */
#define SDC_CFG_INTATGAP        (0x1  << 21)	/* RW */
#define SDC_CFG_DTOC            (0xffUL << 24)	/* RW */

/* SDC_CMD mask */
#define SDC_CMD_OPC             (0x3f << 0)	/* RW */
#define SDC_CMD_BRK             (0x1  << 6)	/* RW */
#define SDC_CMD_RSPTYP          (0x7  << 7)	/* RW */
#define SDC_CMD_DTYP            (0x3  << 11)	/* RW */
#define SDC_CMD_RW              (0x1  << 13)	/* RW */
#define SDC_CMD_STOP            (0x1  << 14)	/* RW */
#define SDC_CMD_GOIRQ           (0x1  << 15)	/* RW */
#define SDC_CMD_BLKLEN          (0xfff << 16)	/* RW */
#define SDC_CMD_AUTOCMD         (0x3  << 28)	/* RW */
#define SDC_CMD_VOLSWTH         (0x1  << 30)	/* RW */

/* SDC_STS mask */
#define SDC_STS_SDCBUSY         (0x1  << 0)	/* RW */
#define SDC_STS_CMDBUSY         (0x1  << 1)	/* RW */
#define SDC_STS_SWR_COMPL       (0x1  << 31)	/* RW */

/* SDC_DCRC_STS mask */
#define SDC_DCRC_STS_NEG        (0xff << 8)	/* RO */
#define SDC_DCRC_STS_POS        (0xff << 0)	/* RO */

/* EMMC_CFG0 mask */
#define EMMC_CFG0_BOOTSTART     (0x1  << 0)	/* W */
#define EMMC_CFG0_BOOTSTOP      (0x1  << 1)	/* W */
#define EMMC_CFG0_BOOTMODE      (0x1  << 2)	/* RW */
#define EMMC_CFG0_BOOTACKDIS    (0x1  << 3)	/* RW */
#define EMMC_CFG0_BOOTWDLY      (0x7  << 12)	/* RW */
#define EMMC_CFG0_BOOTSUPP      (0x1  << 15)	/* RW */

/* EMMC_CFG1 mask */
#define EMMC_CFG1_BOOTDATTMC    (0xfffff << 0)	/* RW */
#define EMMC_CFG1_BOOTACKTMC    (0xfffUL << 20)	/* RW */

/* EMMC_STS mask */
#define EMMC_STS_BOOTCRCERR     (0x1  << 0)	/* W1C */
#define EMMC_STS_BOOTACKERR     (0x1  << 1)	/* W1C */
#define EMMC_STS_BOOTDATTMO     (0x1  << 2)	/* W1C */
#define EMMC_STS_BOOTACKTMO     (0x1  << 3)	/* W1C */
#define EMMC_STS_BOOTUPSTATE    (0x1  << 4)	/* R */
#define EMMC_STS_BOOTACKRCV     (0x1  << 5)	/* W1C */
#define EMMC_STS_BOOTDATRCV     (0x1  << 6)	/* R */

/* EMMC_IOCON mask */
#define EMMC_IOCON_BOOTRST      (0x1  << 0)	/* RW */

/* SDC_ACMD19_TRG mask */
#define SDC_ACMD19_TRG_TUNESEL  (0xf  << 0)	/* RW */

/* MSDC_DMA_CTRL mask */
#define MSDC_DMA_CTRL_START     (0x1  << 0)	/* W */
#define MSDC_DMA_CTRL_STOP      (0x1  << 1)	/* W */
#define MSDC_DMA_CTRL_RESUME    (0x1  << 2)	/* W */
#define MSDC_DMA_CTRL_MODE      (0x1  << 8)	/* RW */
#define MSDC_DMA_CTRL_LASTBUF   (0x1  << 10)	/* RW */
#define MSDC_DMA_CTRL_BRUSTSZ   (0x7  << 12)	/* RW */
#ifndef CONFIG_ARCH_MT8135
#define MSDC_DMA_CTRL_XFERSZ    (0xffffUL << 16)	/* RW */
#endif

/* MSDC_DMA_CFG mask */
#define MSDC_DMA_CFG_STS        (0x1  << 0)	/* R */
#define MSDC_DMA_CFG_DECSEN     (0x1  << 1)	/* RW */
#define MSDC_DMA_CFG_AHBHPROT2  (0x2  << 8)	/* RW */
#define MSDC_DMA_CFG_ACTIVEEN   (0x2  << 12)	/* RW */
#define MSDC_DMA_CFG_CS12B16B   (0x1  << 16)	/* RW */

/* MSDC_PATCH_BIT mask */
/* #define CKGEN_RX_SDClKO_SEL     (0x1  << 0) *//*This bit removed on MT6589/MT8585 */
#define MSDC_PATCH_BIT_ODDSUPP    (0x1  <<  1)	/* RW */
/* #define MSDC_PATCH_BIT_CKGEN_CK (0x1  << 6) *//* This bit removed on MT6589/MT8585 (Only use internel clock) */
#define MSDC_INT_DAT_LATCH_CK_SEL (0x7  <<  7)
#define MSDC_CKGEN_MSDC_DLY_SEL   (0x1F << 10)
#define MSDC_PATCH_BIT_IODSSEL    (0x1  << 16)	/* RW */
#define MSDC_PATCH_BIT_IOINTSEL   (0x1  << 17)	/* RW */
#define MSDC_PATCH_BIT_BUSYDLY    (0xf  << 18)	/* RW */
#define MSDC_PATCH_BIT_WDOD       (0xf  << 22)	/* RW */
#define MSDC_PATCH_BIT_IDRTSEL    (0x1  << 26)	/* RW */
#define MSDC_PATCH_BIT_CMDFSEL    (0x1  << 27)	/* RW */
#define MSDC_PATCH_BIT_INTDLSEL   (0x1  << 28)	/* RW */
#define MSDC_PATCH_BIT_SPCPUSH    (0x1  << 29)	/* RW */
#define MSDC_PATCH_BIT_DECRCTMO   (0x1  << 30)	/* RW */

/* MSDC_PATCH_BIT1 mask */
#define MSDC_PATCH_BIT1_WRDAT_CRCS  (0x7 << 0)
#define MSDC_PATCH_BIT1_CMD_RSP     (0x7 << 3)
#define MSDC_PATCH_BIT1_GET_CRC_MARGIN	(0x01 << 7) /* RW */

/* MSDC_PAD_CTL0 mask */
#define MSDC_PAD_CTL0_CLKDRVN   (0x7  << 0)	/* RW */
#define MSDC_PAD_CTL0_CLKDRVP   (0x7  << 4)	/* RW */
#define MSDC_PAD_CTL0_CLKSR     (0x1  << 8)	/* RW */
#define MSDC_PAD_CTL0_CLKPD     (0x1  << 16)	/* RW */
#define MSDC_PAD_CTL0_CLKPU     (0x1  << 17)	/* RW */
#define MSDC_PAD_CTL0_CLKSMT    (0x1  << 18)	/* RW */
#define MSDC_PAD_CTL0_CLKIES    (0x1  << 19)	/* RW */
#define MSDC_PAD_CTL0_CLKTDSEL  (0xf  << 20)	/* RW */
#define MSDC_PAD_CTL0_CLKRDSEL  (0xffUL << 24)	/* RW */

/* MSDC_PAD_CTL1 mask */
#define MSDC_PAD_CTL1_CMDDRVN   (0x7  << 0)	/* RW */
#define MSDC_PAD_CTL1_CMDDRVP   (0x7  << 4)	/* RW */
#define MSDC_PAD_CTL1_CMDSR     (0x1  << 8)	/* RW */
#define MSDC_PAD_CTL1_CMDPD     (0x1  << 16)	/* RW */
#define MSDC_PAD_CTL1_CMDPU     (0x1  << 17)	/* RW */
#define MSDC_PAD_CTL1_CMDSMT    (0x1  << 18)	/* RW */
#define MSDC_PAD_CTL1_CMDIES    (0x1  << 19)	/* RW */
#define MSDC_PAD_CTL1_CMDTDSEL  (0xf  << 20)	/* RW */
#define MSDC_PAD_CTL1_CMDRDSEL  (0xffUL << 24)	/* RW */

/* MSDC_PAD_CTL2 mask */
#define MSDC_PAD_CTL2_DATDRVN   (0x7  << 0)	/* RW */
#define MSDC_PAD_CTL2_DATDRVP   (0x7  << 4)	/* RW */
#define MSDC_PAD_CTL2_DATSR     (0x1  << 8)	/* RW */
#define MSDC_PAD_CTL2_DATPD     (0x1  << 16)	/* RW */
#define MSDC_PAD_CTL2_DATPU     (0x1  << 17)	/* RW */
#define MSDC_PAD_CTL2_DATIES    (0x1  << 19)	/* RW */
#define MSDC_PAD_CTL2_DATSMT    (0x1  << 18)	/* RW */
#define MSDC_PAD_CTL2_DATTDSEL  (0xf  << 20)	/* RW */
#define MSDC_PAD_CTL2_DATRDSEL  (0xffUL << 24)	/* RW */

/* MSDC_PAD_TUNE mask */
#define MSDC_PAD_TUNE_DATWRDLY  (0x1F << 0)	/* RW */
#define MSDC_PAD_TUNE_DATRRDLY  (0x1F << 8)	/* RW */
#define MSDC_PAD_TUNE_CMDRDLY   (0x1F << 16)	/* RW */
#define MSDC_PAD_TUNE_CMDRRDLY  (0x1FUL << 22)	/* RW */
#define MSDC_PAD_TUNE_CLKTXDLY  (0x1FUL << 27)	/* RW */

/* MSDC_DAT_RDDLY0/1 mask */
#define MSDC_DAT_RDDLY0_D3      (0x1F << 0)	/* RW */
#define MSDC_DAT_RDDLY0_D2      (0x1F << 8)	/* RW */
#define MSDC_DAT_RDDLY0_D1      (0x1F << 16)	/* RW */
#define MSDC_DAT_RDDLY0_D0      (0x1FUL << 24)	/* RW */

#define MSDC_DAT_RDDLY1_D7      (0x1F << 0)	/* RW */
#define MSDC_DAT_RDDLY1_D6      (0x1F << 8)	/* RW */
#define MSDC_DAT_RDDLY1_D5      (0x1F << 16)	/* RW */
#define MSDC_DAT_RDDLY1_D4      (0x1FUL << 24)	/* RW */

#define CARD_READY_FOR_DATA             (1<<8)
#define CARD_CURRENT_STATE(x)           ((x&0x00001E00)>>9)


/********************MSDC0*************************************************/
#define MSDC0_TDSEL_BASE		(GPIO_BASE+0x0700)
#define MSDC0_RDSEL_BASE		(GPIO_BASE+0x0700)
#define MSDC0_TDSEL				(0xF << 0)
#define MSDC0_RDSEL				(0x3FUL << 4)
#define MSDC0_DAT_DRVING_BASE	(GPIO_BASE+0x0500)
#define MSDC0_DAT_DRVING		(0x7 << 0)
#define MSDC0_CMD_DRVING_BASE	(GPIO_BASE+0x0500)
#define MSDC0_CMD_DRVING		(0x7 << 4)
#define MSDC0_CLK_DRVING_BASE	(GPIO_BASE+0x0500)
#define MSDC0_CLK_DRVING		(0x7 << 8)
#define MSDC0_DAT_SR_BASE		(GPIO_BASE+0x0500)
#define MSDC0_DAT_SR			(0x1 << 3)
#define MSDC0_CMD_SR_BASE		(GPIO_BASE+0x0500)
#define MSDC0_CMD_SR			(0x1 << 7)
#define MSDC0_CLK_SR_BASE		(GPIO_BASE+0x0500)
#define MSDC0_CLK_SR			(0x1 << 11)
#define MSDC0_IES_BASE			(GPIO_BASE+0x0100)
#define MSDC0_IES_DAT			(0x1 << 9)
#define MSDC0_IES_CMD			(0x1 << 4)
#define MSDC0_IES_CLK			(0x1 << 5)

#define MSDC0_SMT_BASE			(GPIO_BASE+0x0300)
#define MSDC0_SMT_DAT			(0x3CF << 0)
/* MASK:DAT0|DAT1|DAT2|DAT3|CLK|CMD|RSTB(Please use default value)|DAT4|DAT5|DAT6|DAT7 */
#define MSDC0_SMT_CMD			(0x1 << 4)
#define MSDC0_SMT_CLK			(0x1 << 5)

#define MSDC0_R0_BASE			(GPIO1_BASE+0x04D0)	/* 1.8v 10K resistor control */
#define MSDC0_R0_DAT			(0xFF << 2)	/* MASK:DAT7|DAT6|DAT5|DAT4|DAT3|DAT2|DAT1|DAT0|CMD|CLK */
#define MSDC0_R0_CMD			(0x1 << 1)
#define MSDC0_R0_CLK			(0x1 << 0)

#define MSDC0_R1_BASE			(GPIO_BASE+0x0200)
#define MSDC0_R1_DAT			(0x3CF << 0)
/* MASK:DAT0|DAT1|DAT2|DAT3|CLK|CMD|RSTB(Please use default value)|DAT4|DAT5|DAT6|DAT7 */
#define MSDC0_R1_CMD			(0x1 << 4)
#define MSDC0_R1_CLK			(0x1 << 5)


#define MSDC0_PUPD_BASE			(GPIO_BASE+0x0400)	/* '1' = pull up '0' =pull down */
#define MSDC0_PUPD_DAT			(0x3CF << 0)
/* MASK:DAT0|DAT1|DAT2|DAT3|CLK|CMD|RSTB(Please use default value)|DAT4|DAT5|DAT6|DAT7 */
#define MSDC0_PUPD_CMD			(0x1 << 4)
#define MSDC0_PUPD_CLK			(0x1 << 5)


/****************************MSDC1*************************************************/

#define MSDC1_TDSEL_BASE        (GPIO1_BASE+0x0730)
#define MSDC1_TDSEL				(0xFUL  << 4)

#define MSDC1_RDSEL_BASE        (GPIO1_BASE+0x0730)
#define MSDC1_RDSEL				(0x3FUL << 8)

#define MSDC1_DAT_DRVING_BASE   (GPIO1_BASE+0x0550)
#define MSDC1_DAT_DRVING		(0x7UL  << 8)

#define MSDC1_CMD_DRVING_BASE   (GPIO1_BASE+0x0550)
#define MSDC1_CMD_DRVING		(0x7UL  << 12)

#define MSDC1_CLK_DRVING_BASE   (GPIO1_BASE+0x0550)
#define MSDC1_CLK_DRVING		(0x7UL  << 16)

#define MSDC1_DAT_SR_BASE       (GPIO1_BASE+0x0550)
#define MSDC1_DAT_SR			(0x1 << 11)
#define MSDC1_CMD_SR_BASE       (GPIO1_BASE+0x0550)
#define MSDC1_CMD_SR			(0x1 << 15)
#define MSDC1_CLK_SR_BASE       (GPIO1_BASE+0x0550)
#define MSDC1_CLK_SR			(0x1 << 7)

#define MSDC1_IES_BASE          (GPIO1_BASE+0x0150)
#define MSDC1_IES_DAT			(0x1 << 3)
#define MSDC1_IES_CMD			(0x1 << 5)
#define MSDC1_IES_CLK			(0x1 << 6)

#define MSDC1_SMT_BASE          (GPIO1_BASE+0x0350)

#define MSDC1_SMT_DAT			(0x33 << 3)	/* DATA3|DATA2...DATA1|DATA0 */
#define MSDC1_SMT_CMD			(0x1 << 5)
#define MSDC1_SMT_CLK			(0x1 << 6)

#define MSDC1_DETECT_BASE       (GPIO_BASE+0x0CC0)
#define MSDC1_DETECT_SEL        (0x1 << 9)

#define MSDC1_PUPD_ENA_DET_BASE (GPIO1_BASE+0x230)
#define MSDC1_PUPD_ENA_DET      (0x1 << 15)

#define MSDC1_PUPD_DET_BASE     (GPIO1_BASE+0x430)
#define MSDC1_PUPD_DET          (0x1 << 15)

#define MSDC1_PUPD_ENABLE_BASE      (GPIO1_BASE+0x0250)	/* '1'= enable '0' = disable */
#define MSDC1_PUPD_POLARITY_BASE    (GPIO1_BASE+0x0450)	/* '1' = pull up '0' =pull down */

#define MSDC1_PUPD_DAT			(0x33 << 3)	/* DATA3|DATA2...DATA1|DATA0 */
#define MSDC1_PUPD_CMD			(0x1 << 5)
#define MSDC1_PUPD_CLK			(0x1 << 6)

/****************************MSDC2*************************************************/

#define MSDC2_TDSEL_BASE		(GPIO_BASE+0x0780)
#define MSDC2_RDSEL_BASE		(GPIO_BASE+0x0780)
#define MSDC2_TDSEL				(0xFUL  << 0)
#define MSDC2_RDSEL				(0x3FUL << 8)

#define MSDC2_DAT_DRVING_BASE	(GPIO_BASE+0x05A0)
#define MSDC2_DAT_DRVING		(0x7	<< 8)

#define MSDC2_CMD_DRVING_BASE	(GPIO_BASE+0x05A0)
#define MSDC2_CMD_DRVING		(0x7	<< 12)

#define MSDC2_CLK_DRVING_BASE	(GPIO_BASE+0x05C0)
#define MSDC2_CLK_DRVING		(0x7UL	<< 20)

#define MSDC2_DAT_SR_BASE		(GPIO_BASE+0x05A0)
#define MSDC2_DAT_SR			(0x1 << 11)
#define MSDC2_CMD_SR_BASE		(GPIO_BASE+0x05A0)
#define MSDC2_CMD_SR			(0x1 << 15)
#define MSDC2_CLK_SR_BASE		(GPIO_BASE+0x05C0)
#define MSDC2_CLK_SR			(0x1 << 23)

#define MSDC2_IES_BASE			(GPIO_BASE+0x01B0)
#define MSDC2_IES_DAT			(0x1 << 3)
#define MSDC2_IES_CMD			(0x1 << 0)
#define MSDC2_IES_CLK			(0x1 << 1)

#define MSDC2_SMT_BASE1			(GPIO_BASE+0x03B0)
#define MSDC2_SMT_DAT1_0		(0x3 << 2)	/* ...DAT1|DAT0... */
#define MSDC2_SMT_CMD			(0x1 << 0)
#define MSDC2_SMT_CLK			(0x1 << 1)
#define MSDC2_SMT_BASE2			(GPIO_BASE+0x03A0)
#define MSDC2_SMT_DAT2_3		(0x3 << 14)	/* ...DAT2|DAT3... */


#define MSDC2_PUPD_ENABLE_BASE1			(GPIO1_BASE+0x0250)	/* '1'= enable '0' = disable */
#define MSDC2_PUPD_POLARITY_BASE1		(GPIO1_BASE+0x0450)	/* '1' = pull up '0' =pull down */
#define MSDC2_PUPD_DAT1_0		(0x3 << 1)	/* ...DAT1|DAT0... */
#define MSDC2_PUPD_CLK			(0x1 << 0)

#define MSDC2_PUPD_ENABLE_BASE2			(GPIO_BASE+0x0240)	/* '1'= enable '0' = disable */
#define MSDC2_PUPD_POLARITY_BASE2		(GPIO_BASE+0x0440)	/* '1' = pull up '0' =pull down */
#define MSDC2_PUPD_DAT2_3		(0x3 << 13)
#define MSDC2_PUPD_CMD			(0x1 << 15)

/****************************MSDC3*************************************************/

#define MSDC3_TDSEL_BASE		(GPIO_BASE+0x0790)
#define MSDC3_RDSEL_BASE		(GPIO_BASE+0x0790)
#define MSDC3_TDSEL				(0xFUL  << 16)
#define MSDC3_RDSEL				(0x3FUL << 24)

#define MSDC3_DAT_DRVING_BASE	(GPIO_BASE+0x05C0)
#define MSDC3_DAT_DRVING		(0x7	<< 4)

#define MSDC3_CMD_DRVING_BASE	(GPIO_BASE+0x05C0)
#define MSDC3_CMD_DRVING		(0x7    << 8)

#define MSDC3_CLK_DRVING_BASE	(GPIO_BASE+0x05C0)
#define MSDC3_CLK_DRVING		(0x7UL  << 24)

#define MSDC3_DAT_SR_BASE		(GPIO_BASE+0x05C0)
#define MSDC3_DAT_SR			(0x1 << 7)
#define MSDC3_CMD_SR_BASE		(GPIO_BASE+0x05C0)
#define MSDC3_CMD_SR			(0x1 << 11)
#define MSDC3_CLK_SR_BASE		(GPIO_BASE+0x05C0)
#define MSDC3_CLK_SR			(0x1 << 27)

#define MSDC3_IES_BASE			(GPIO_BASE+0x01E0)
#define MSDC3_IES_DAT			(0x1 << 7)
#define MSDC3_IES_CMD			(0x1 << 4)
#define MSDC3_IES_CLK			(0x1 << 5)

#define MSDC3_SMT_BASE			(GPIO_BASE+0x03E0)

#define MSDC3_SMT_DAT			((0x3 << 6) | (0x3 << 2))	/* ..DAT0|DAT1..DAT3|DAT2 */
#define MSDC3_SMT_CMD			(0x1 << 4)
#define MSDC3_SMT_CLK			(0x1 << 5)

#define MSDC3_R0_BASE			(GPIO_BASE+0x04F0)	/* 1.8v 10K resistor control */

#define MSDC3_R0_DAT			(0xF << 12)	/* MASK:DAT3|DAT2|DAT1|DAT0 */
#define MSDC3_R0_CMD			(0x1 << 11)
#define MSDC3_R0_CLK			(0x1 << 10)


#define MSDC3_R1_BASE			(GPIO_BASE+0x02C0)
#define MSDC3_R1_DAT		    (0x33 << 5)
#define MSDC3_R1_CMD			(0x1 << 7)
#define MSDC3_R1_CLK			(0x1 << 8)


#define MSDC3_PUPD_BASE			(GPIO_BASE+0x04C0)	/* '1' = pull up '0' =pull down */
#define MSDC3_PUPD_DAT		    (0x33 << 5)
#define MSDC3_PUPD_CMD			(0x1 << 7)
#define MSDC3_PUPD_CLK			(0x1 << 8)

/********************MSDC4*************************************************/
#define	GPIO_BASE2				(0xF020C000)
#define MSDC4_TDSEL_BASE		(GPIO_BASE2+0x0760)
#define MSDC4_RDSEL_BASE		(GPIO_BASE2+0x0770)
#define MSDC4_TDSEL				(0xFUL << 24)
#define MSDC4_RDSEL				(0x3F  << 0)
#define MSDC4_DAT_DRVING_BASE	(GPIO_BASE2+0x0580)
#define MSDC4_DAT_DRVING		(0x7 << 20)
#define MSDC4_CMD_DRVING_BASE	(GPIO_BASE2+0x0570)
#define MSDC4_CMD_DRVING		(0x7 << 16)
#define MSDC4_CLK_DRVING_BASE	(GPIO_BASE2+0x0580)
#define MSDC4_CLK_DRVING		(0x7 << 0)
#define MSDC4_DAT_SR_BASE		(GPIO_BASE2+0x0580)
#define MSDC4_DAT_SR			(0x1 << 23)
#define MSDC4_CMD_SR_BASE		(GPIO_BASE2+0x0570)
#define MSDC4_CMD_SR			(0x1 << 19)
#define MSDC4_CLK_SR_BASE		(GPIO_BASE2+0x0580)
#define MSDC4_CLK_SR			(0x1 << 3)
#define MSDC4_IES_BASE			(GPIO_BASE2+0x0180)
#define MSDC4_IES_DAT			(0x1 << 2)
#define MSDC4_IES_CMD			(0x1 << 13)
#define MSDC4_IES_CLK			(0x1 << 11)

#define MSDC4_SMT_BASE			(GPIO_BASE2+0x0380)
#define MSDC4_SMT_DAT			((0x1 << 12) | (0x1F << 6) | (0x3 << 2))
#define MSDC4_SMT_CMD			(0x1 << 13)
#define MSDC4_SMT_CLK			(0x1 << 11)

#define MSDC4_R0_BASE			(GPIO_BASE+0x04F0)	/* 1.8v 10K resistor control */
#define MSDC4_R0_DAT			(0xFFUL << 18)	/* MASK:DAT7|DAT6|DAT5|DAT4|DAT3|DAT2|DAT1|DAT0|CMD|CLK */
#define MSDC4_R0_CMD			(0x1 << 17)
#define MSDC4_R0_CLK			(0x1 << 16)

#define MSDC4_R1_BASE1			(GPIO_BASE2+0x0250)
#define MSDC4_R1_DAT0_7			(0x7F << 9)	/* MASK:DAT2|DAT4|DAT7|DAT6|DAT5|DAT1|DAT0... */

#define MSDC4_R1_BASE2          (GPIO1_BASE+0x0260)
#define MSDC4_R1_DAT3           (0x1 << 1)	/* MASK:DAT3 */
#define MSDC4_R1_CMD			(0x1 << 2)
#define MSDC4_R1_CLK			(0x1 << 1)

#define MSDC4_PUPD_BASE1        (GPIO1_BASE+0x0450)	/* '1' = pull up '0' =pull down */
#define MSDC4_PUPD_DAT0_7_MASK  (0x7F)
#define MSDC4_PUPD_DAT0_7_SFT   (9)
#define MSDC4_PUPD_DAT0_7       (MSDC4_PUPD_DAT0_7_MASK << MSDC4_PUPD_DAT0_7_SFT)
/* MASK:DAT2|DAT4|DAT7|DAT6|DAT5|DAT1|DAT0... */

#define MSDC4_PUPD_BASE2        (GPIO1_BASE+0x0460)	/* '1' = pull up '0' =pull down */
#define MSDC4_PUPD_DAT3_MASK    (0x1)
#define MSDC4_PUPD_DAT3_SFT     (1)
#define MSDC4_PUPD_DAT3         (MSDC4_PUPD_DAT3_MASK << MSDC4_PUPD_DAT3_SFT)	/* MASK:DAT3 */
#define MSDC4_PUPD_CMD_MASK     (0x1)
#define MSDC4_PUPD_CMD_SFT      (2)
#define MSDC4_PUPD_CMD          (MSDC4_PUPD_CMD_MASK << MSDC4_PUPD_CMD_SFT)
#define MSDC4_PUPD_CLK_MASK     (0x1)
#define MSDC4_PUPD_CLK_SFT      (0)
#define MSDC4_PUPD_CLK          (MSDC4_PUPD_CLK_MASK << MSDC4_PUPD_CLK_SFT)

#define EN18IOKEY_BASE				(GPIO1_BASE+0x920)

#define MSDC_EN18IO_CMP_SEL_BASE	(GPIO1_BASE+0x910)
#define	MSDC1_EN18IO_CMP_EN			(0x1 << 3)
#define MSDC1_EN18IO_SEL1			(0x7 << 0)
#define	MSDC2_EN18IO_CMP_EN			(0x1 << 7)
#define MSDC2_EN18IO_SEL			(0x7 << 4)

#define MSDC_EN18IO_SW_BASE			(GPIO_BASE+0x900)
#define MSDC1_EN18IO_SW				(0x1 << 19)
#define MSDC2_EN18IO_SW				(0x1 << 25)
#define MSDC1_TUNE					(0xF << 20)
#define MSDC2_TUNE					(0xFUL << 26)

enum MSDC_POWER {

	MSDC_VIO18_MC1 = 0,
	MSDC_VIO18_MC2,
	MSDC_VIO28_MC1,
	MSDC_VIO28_MC2,
	MSDC_VMC,
	MSDC_VGP6,
	MSDC_VEMC_3V3,
};

#define MSDC_POWER_MC1     MSDC_VEMC_3V3
#define MSDC_POWER_MC2     MSDC_VMC



/*--------------------------------------------------------------------------*/
/* Descriptor Structure                                                     */
/*--------------------------------------------------------------------------*/
struct mt_gpdma_desc {
	u32 hwo:1;		/* could be changed by hw */
	u32 bdp:1;
	u32 rsv0:6;
	u32 chksum:8;
	u32 intr:1;
	u32 rsv1:15;
	void *next;
	void *ptr;
	u32 buflen:16;
	u32 extlen:8;
	u32 rsv2:8;
	u32 arg;
	u32 blknum;
	u32 cmd;
};

struct mt_bdma_desc {
	u32 eol:1;
	u32 rsv0:7;
	u32 chksum:8;
	u32 rsv1:1;
	u32 blkpad:1;
	u32 dwpad:1;
	u32 rsv2:13;
	void *next;
	void *ptr;
	u32 buflen:16;
	u32 rsv3:16;
};

/*--------------------------------------------------------------------------*/
/* Register Debugging Structure                                             */
/*--------------------------------------------------------------------------*/

struct msdc_cfg_reg {
	u32 msdc:1;
	u32 ckpwn:1;
	u32 rst:1;
	u32 pio:1;
	u32 ckdrven:1;
	u32 start18v:1;
	u32 pass18v:1;
	u32 ckstb:1;
	u32 ckdiv:8;
	u32 ckmod:2;
	u32 pad:14;
};

struct msdc_iocon_reg {
	u32 sdr104cksel:1;
	u32 rsmpl:1;
	u32 dsmpl:1;
	u32 ddlysel:1;
	u32 ddr50ckd:1;
	u32 dsplsel:1;
	u32 pad1:10;
	u32 d0spl:1;
	u32 d1spl:1;
	u32 d2spl:1;
	u32 d3spl:1;
	u32 d4spl:1;
	u32 d5spl:1;
	u32 d6spl:1;
	u32 d7spl:1;
	u32 riscsz:1;
	u32 pad2:7;
};

struct msdc_ps_reg {
	u32 cden:1;
	u32 cdsts:1;
	u32 pad1:10;
	u32 cddebounce:4;
	u32 dat:8;
	u32 cmd:1;
	u32 pad2:6;
	u32 wp:1;
};

struct msdc_int_reg {
	u32 mmcirq:1;
	u32 cdsc:1;
	u32 pad1:1;
	u32 atocmdrdy:1;
	u32 atocmdtmo:1;
	u32 atocmdcrc:1;
	u32 dmaqempty:1;
	u32 sdioirq:1;
	u32 cmdrdy:1;
	u32 cmdtmo:1;
	u32 rspcrc:1;
	u32 csta:1;
	u32 xfercomp:1;
	u32 dxferdone:1;
	u32 dattmo:1;
	u32 datcrc:1;
	u32 atocmd19done:1;
	u32 pad2:15;
};

struct msdc_inten_reg {
	u32 mmcirq:1;
	u32 cdsc:1;
	u32 pad1:1;
	u32 atocmdrdy:1;
	u32 atocmdtmo:1;
	u32 atocmdcrc:1;
	u32 dmaqempty:1;
	u32 sdioirq:1;
	u32 cmdrdy:1;
	u32 cmdtmo:1;
	u32 rspcrc:1;
	u32 csta:1;
	u32 xfercomp:1;
	u32 dxferdone:1;
	u32 dattmo:1;
	u32 datcrc:1;
	u32 atocmd19done:1;
	u32 pad2:15;
};

struct msdc_fifocs_reg {
	u32 rxcnt:8;
	u32 pad1:8;
	u32 txcnt:8;
	u32 pad2:7;
	u32 clr:1;
};

struct msdc_txdat_reg {
	u32 val;
};

struct msdc_rxdat_reg {
	u32 val;
};

struct sdc_cfg_reg {
	u32 sdiowkup:1;
	u32 inswkup:1;
	u32 pad1:14;
	u32 buswidth:2;
	u32 pad2:1;
	u32 sdio:1;
	u32 sdioide:1;
	u32 intblkgap:1;
	u32 pad4:2;
	u32 dtoc:8;
};

struct sdc_cmd_reg {
	u32 cmd:6;
	u32 brk:1;
	u32 rsptyp:3;
	u32 pad1:1;
	u32 dtype:2;
	u32 rw:1;
	u32 stop:1;
	u32 goirq:1;
	u32 blklen:12;
	u32 atocmd:2;
	u32 volswth:1;
	u32 pad2:1;
};

struct sdc_arg_reg {
	u32 arg;
};

struct sdc_sts_reg {
	u32 sdcbusy:1;
	u32 cmdbusy:1;
	u32 pad:29;
	u32 swrcmpl:1;
};

struct sdc_resp0_reg {
	u32 val;
};

struct sdc_resp1_reg {
	u32 val;
};

struct sdc_resp2_reg {
	u32 val;
};

struct sdc_resp3_reg {
	u32 val;
};

struct sdc_blknum_reg {
	u32 num;
};

struct sdc_csts_reg {
	u32 sts;
};

struct sdc_cstsen_reg {
	u32 sts;
};

struct sdc_datcrcsts_reg {
	u32 datcrcsts:8;
	u32 ddrcrcsts:4;
	u32 pad:20;
};

struct emmc_cfg0_reg {
	u32 bootstart:1;
	u32 bootstop:1;
	u32 bootmode:1;
	u32 pad1:9;
	u32 bootwaidly:3;
	u32 bootsupp:1;
	u32 pad2:16;
};

struct emmc_cfg1_reg {
	u32 bootcrctmc:16;
	u32 pad:4;
	u32 bootacktmc:12;
};

struct emmc_sts_reg {
	u32 bootcrcerr:1;
	u32 bootackerr:1;
	u32 bootdattmo:1;
	u32 bootacktmo:1;
	u32 bootupstate:1;
	u32 bootackrcv:1;
	u32 bootdatrcv:1;
	u32 pad:25;
};

struct emmc_iocon_reg {
	u32 bootrst:1;
	u32 pad:31;
};

struct msdc_acmd_resp_reg {
	u32 val;
};

struct msdc_acmd19_trg_reg {
	u32 tunesel:4;
	u32 pad:28;
};

struct msdc_acmd19_sts_reg {
	u32 val;
};

struct msdc_dma_sa_reg {
	u32 addr;
};

struct msdc_dma_ca_reg {
	u32 addr;
};

struct msdc_dma_ctrl_reg {
	u32 start:1;
	u32 stop:1;
	u32 resume:1;
	u32 pad1:5;
	u32 mode:1;
	u32 pad2:1;
	u32 lastbuf:1;
	u32 pad3:1;
	u32 brustsz:3;
	u32 pad4:1;
	u32 xfersz:16;
};

struct msdc_dma_cfg_reg {
	u32 status:1;
	u32 decsen:1;
	u32 pad1:2;
	u32 bdcsen:1;
	u32 gpdcsen:1;
	u32 pad2:26;
};

struct msdc_dbg_sel_reg {
	u32 sel:16;
	u32 pad2:16;
};

struct msdc_dbg_out_reg {
	u32 val;
};

struct msdc_pad_ctl0_reg {
	u32 clkdrvn:3;
	u32 rsv0:1;
	u32 clkdrvp:3;
	u32 rsv1:1;
	u32 clksr:1;
	u32 rsv2:7;
	u32 clkpd:1;
	u32 clkpu:1;
	u32 clksmt:1;
	u32 clkies:1;
	u32 clktdsel:4;
	u32 clkrdsel:8;
};

struct msdc_pad_ctl1_reg {
	u32 cmddrvn:3;
	u32 rsv0:1;
	u32 cmddrvp:3;
	u32 rsv1:1;
	u32 cmdsr:1;
	u32 rsv2:7;
	u32 cmdpd:1;
	u32 cmdpu:1;
	u32 cmdsmt:1;
	u32 cmdies:1;
	u32 cmdtdsel:4;
	u32 cmdrdsel:8;
};

struct msdc_pad_ctl2_reg {
	u32 datdrvn:3;
	u32 rsv0:1;
	u32 datdrvp:3;
	u32 rsv1:1;
	u32 datsr:1;
	u32 rsv2:7;
	u32 datpd:1;
	u32 datpu:1;
	u32 datsmt:1;
	u32 daties:1;
	u32 dattdsel:4;
	u32 datrdsel:8;
};

struct msdc_pad_tune_reg {
	u32 wrrxdly:3;
	u32 pad1:5;
	u32 rdrxdly:8;
	u32 pad2:16;
};

struct msdc_dat_rddly0 {
	u32 dat0:5;
	u32 rsv0:3;
	u32 dat1:5;
	u32 rsv1:3;
	u32 dat2:5;
	u32 rsv2:3;
	u32 dat3:5;
	u32 rsv3:3;
};

struct msdc_dat_rddly1 {
	u32 dat4:5;
	u32 rsv4:3;
	u32 dat5:5;
	u32 rsv5:3;
	u32 dat6:5;
	u32 rsv6:3;
	u32 dat7:5;
	u32 rsv7:3;
};

struct msdc_hw_dbg_reg {
	u32 dbg0sel:8;
	u32 dbg1sel:6;
	u32 pad1:2;
	u32 dbg2sel:6;
	u32 pad2:2;
	u32 dbg3sel:6;
	u32 pad3:2;
};

struct msdc_version_reg {
	u32 val;
};

struct msdc_eco_ver_reg {
	u32 val;
};

struct msdc_regs {
	struct msdc_cfg_reg msdc_cfg;	/* base+0x00h */
	struct msdc_iocon_reg msdc_iocon;	/* base+0x04h */
	struct msdc_ps_reg msdc_ps;	/* base+0x08h */
	struct msdc_int_reg msdc_int;	/* base+0x0ch */
	struct msdc_inten_reg msdc_inten;	/* base+0x10h */
	struct msdc_fifocs_reg msdc_fifocs;	/* base+0x14h */
	struct msdc_txdat_reg msdc_txdat;	/* base+0x18h */
	struct msdc_rxdat_reg msdc_rxdat;	/* base+0x1ch */
	u32 rsv1[4];
	struct sdc_cfg_reg sdc_cfg;	/* base+0x30h */
	struct sdc_cmd_reg sdc_cmd;	/* base+0x34h */
	struct sdc_arg_reg sdc_arg;	/* base+0x38h */
	struct sdc_sts_reg sdc_sts;	/* base+0x3ch */
	struct sdc_resp0_reg sdc_resp0;	/* base+0x40h */
	struct sdc_resp1_reg sdc_resp1;	/* base+0x44h */
	struct sdc_resp2_reg sdc_resp2;	/* base+0x48h */
	struct sdc_resp3_reg sdc_resp3;	/* base+0x4ch */
	struct sdc_blknum_reg sdc_blknum;	/* base+0x50h */
	u32 rsv2[1];
	struct sdc_csts_reg sdc_csts;	/* base+0x58h */
	struct sdc_cstsen_reg sdc_cstsen;	/* base+0x5ch */
	struct sdc_datcrcsts_reg sdc_dcrcsta;	/* base+0x60h */
	u32 rsv3[3];
	struct emmc_cfg0_reg emmc_cfg0;	/* base+0x70h */
	struct emmc_cfg1_reg emmc_cfg1;	/* base+0x74h */
	struct emmc_sts_reg emmc_sts;	/* base+0x78h */
	struct emmc_iocon_reg emmc_iocon;	/* base+0x7ch */
	struct msdc_acmd_resp_reg acmd_resp;	/* base+0x80h */
	struct msdc_acmd19_trg_reg acmd19_trg;	/* base+0x84h */
	struct msdc_acmd19_sts_reg acmd19_sts;	/* base+0x88h */
	u32 rsv4[1];
	struct msdc_dma_sa_reg dma_sa;	/* base+0x90h */
	struct msdc_dma_ca_reg dma_ca;	/* base+0x94h */
	struct msdc_dma_ctrl_reg dma_ctrl;	/* base+0x98h */
	struct msdc_dma_cfg_reg dma_cfg;	/* base+0x9ch */
	struct msdc_dbg_sel_reg dbg_sel;	/* base+0xa0h */
	struct msdc_dbg_out_reg dbg_out;	/* base+0xa4h */
	u32 rsv5[2];
	u32 patch0;		/* base+0xb0h */
	u32 patch1;		/* base+0xb4h */
	u32 rsv6[10];
	struct msdc_pad_ctl0_reg pad_ctl0;	/* base+0xe0h */
	struct msdc_pad_ctl1_reg pad_ctl1;	/* base+0xe4h */
	struct msdc_pad_ctl2_reg pad_ctl2;	/* base+0xe8h */
	struct msdc_pad_tune_reg pad_tune;	/* base+0xech */
	struct msdc_dat_rddly0 dat_rddly0;	/* base+0xf0h */
	struct msdc_dat_rddly1 dat_rddly1;	/* base+0xf4h */
	struct msdc_hw_dbg_reg hw_dbg;	/* base+0xf8h */
	u32 rsv7[1];
	struct msdc_version_reg version;	/* base+0x100h */
	struct msdc_eco_ver_reg eco_ver;	/* base+0x104h */
};

struct scatterlist_ex {
	u32 cmd;
	u32 arg;
	u32 sglen;
	struct scatterlist *sg;
};

#define DMA_FLAG_NONE       (0x00000000)
#define DMA_FLAG_EN_CHKSUM  (0x00000001)
#define DMA_FLAG_PAD_BLOCK  (0x00000002)
#define DMA_FLAG_PAD_DWORD  (0x00000004)

struct msdc_dma {
	u32 flags;		/* flags */
	u32 xfersz;		/* xfer size in bytes */
	u32 sglen;		/* size of scatter list */
	u32 blklen;		/* block size */
	struct scatterlist *sg;	/* I/O scatter list */
	struct scatterlist_ex *esg;	/* extended I/O scatter list */
	u8 mode;		/* dma mode        */
	u8 burstsz;		/* burst size      */
	u8 intr;		/* dma done interrupt */
	u8 padding;		/* padding */
	u32 cmd;		/* enhanced mode command */
	u32 arg;		/* enhanced mode arg */
	u32 rsp;		/* enhanced mode command response */
	u32 autorsp;		/* auto command response */

	struct mt_gpdma_desc *gpd;		/* pointer to gpd array */
	struct mt_bdma_desc *bd;		/* pointer to bd array */
	dma_addr_t gpd_addr;	/* the physical address of gpd array */
	dma_addr_t bd_addr;	/* the physical address of bd array */
	u32 used_gpd;		/* the number of used gpd elements */
	u32 used_bd;		/* the number of used bd elements */
};

struct tune_counter {
	u32 time_cmd;
	u32 time_read;
	u32 time_write;
};
struct msdc_saved_para {
	u32 pad_tune;
	u32 ddly0;
	u32 ddly1;
	u8 cmd_resp_ta_cntr;
	u8 wrdat_crc_ta_cntr;
	u8 suspend_flag;
	u32 msdc_cfg;
	u32 mode;
	u32 div;
	u32 sdc_cfg;
	u32 iocon;
	int ddr;
	u32 hz;
	u8 int_dat_latch_ck_sel;
	u8 ckgen_msdc_dly_sel;
};

struct last_cmd {
	u8	cmd;
	u8	type;
	u32	arg;
};

struct msdc_host {
	struct device *dev;
	struct msdc_hw *hw;

	struct mmc_host *mmc;	/* mmc structure */
	int cmd_rsp;
	struct last_cmd last_cmd;
	u32 sdio_tune_flag;

	struct mmcdev_info timing;

	spinlock_t lock;
	struct mmc_request *mrq;
	struct mmc_request *repeat_mrq;
	struct mmc_command *cmd;
	struct mmc_data *data;
	int error;
	int error_count;
	int max_error_count;
	int *error_stat;
	int timing_pos;
	bool disable_next;

	spinlock_t clk_gate_lock;
	spinlock_t remove_bad_card;	/*to solve removing bad card race condition with hot-plug enable */
	int clk_gate_count;
	unsigned long sdio_flags;

	u32 base;		/* host base address */
	int id;			/* host id */
	int pwr_ref;		/* core power reference count */

	struct msdc_dma dma;	/* dma channel */
	u32 dma_addr;		/* dma transfer address */
	u32 dma_left_size;	/* dma transfer left size */
	u32 dma_xfer_size;	/* dma transfer size in bytes */

	u32 timeout_ns;		/* data timeout ns */
	u32 timeout_clks;	/* data timeout clks */

	struct msdc_pinctrl *pin_ctl;
	struct delayed_work req_timeout;
	struct work_struct repeat_req;
	int irq;		/* host interrupt */

	struct tasklet_struct card_tasklet;

	u32 erase_start;
	u32 erase_end;

	u32 mclk;		/* mmc subsystem clock */
	u32 hclk;		/* host clock speed */
	u32 sclk;		/* SD/MS clock speed */
	u8 core_clkon;		/* Host core clock on ? */
	u8 card_clkon;		/* Card clock on ? */
	u8 core_power;		/* core power */
	u8 power_mode;		/* host power mode */
	u8 card_inserted;	/* card inserted ? */
	u8 suspend;		/* host suspended ? */
	u8 reserved;
	u8 app_cmd;		/* for app command */
	u32 app_cmd_arg;
	u64 starttime;
	struct timer_list timer;
	struct tune_counter t_counter;
	u32 rwcmd_time_tune;
	u32 read_time_tune;
	u32 write_time_tune;
	u8 autocmd;
	u32 sw_timeout;
	u32 power_cycle;	/* power cycle done in tuning flow */
	bool power_cycle_enable;	/*Enable power cycle */
	bool tune;
	bool shutdown:1;
	int power_domain;
	bool ddr;
	struct msdc_saved_para saved_para;
	int cd_pin;
	int cd_irq;
	int sd_cd_polarity;
	int sd_cd_insert_work;	/* to make sure insert mmc_rescan this work in start_host when boot up */
	/* driver will get a EINT(Level sensitive) when boot up phone with card insert */
	bool block_bad_card;
	struct dma_addr *latest_dma_address;
	int latest_operation_type;
	u8 clock_src;
	u32 host_mode;
	u32 io_power_state;
	u32 core_power_state;
	unsigned long request_ts;
	u8 ext_csd[512];
	int offset;
	bool offset_valid;
	char partition_access;

#ifdef MSDC_DMA_VIOLATION_DEBUG
	int dma_debug;
#endif

#ifdef SDIO_ERROR_BYPASS
	int sdio_error;		/* sdio error can't recovery */
#endif
	void (*power_control) (struct msdc_host *host, u32 on);
	void (*power_switch) (struct msdc_host *host, u32 on);
};

enum {
	OPER_TYPE_READ,
	OPER_TYPE_WRITE,
	OPER_TYPE_NUM
};

struct dma_addr {
	u32 start_address;
	u32 size;
	u8 end;
	struct dma_addr *next;
};

static inline unsigned int uffs(unsigned int x)
{
	unsigned int r = 1;

	if (!x)
		return 0;
	if (!(x & 0xffff)) {
		x >>= 16;
		r += 16;
	}
	if (!(x & 0xff)) {
		x >>= 8;
		r += 8;
	}
	if (!(x & 0xf)) {
		x >>= 4;
		r += 4;
	}
	if (!(x & 3)) {
		x >>= 2;
		r += 2;
	}
	if (!(x & 1)) {
		x >>= 1;
		r += 1;
	}
	return r;
}

#define sdr_read8(reg)           __raw_readb((const  void *)reg)
#define sdr_read16(reg)          __raw_readw((const  void *)reg)
#define sdr_read32(reg)          __raw_readl((const  void *)reg)
#if 0
#define sdr_write8(reg, val)      __raw_writeb(val, reg)
#define sdr_write16(reg, val)     __raw_writew(val, reg)
#define sdr_write32(reg, val)     __raw_writel(val, reg)
#else
#define sdr_write8(reg, val)      mt65xx_reg_sync_writeb(val, reg)
#define sdr_write16(reg, val)     mt65xx_reg_sync_writew(val, reg)
#define sdr_write32(reg, val)     mt65xx_reg_sync_writel(val, reg)

#define sdr_set_bits(reg, bs) \
	do {\
		unsigned int tv = sdr_read32(reg);\
		tv |= (u32)(bs); \
		sdr_write32(reg, tv); \
	} while (0)

#define sdr_clr_bits(reg, bs) \
	do {\
		unsigned int tv = sdr_read32(reg);\
		tv &= ~((u32)(bs)); \
		sdr_write32(reg, tv); \
	} while (0)

#endif

#define sdr_set_field(reg, field, val) \
	do {	\
		unsigned int tv = sdr_read32(reg);	\
		tv &= ~(field); \
		tv |= ((val) << (uffs((unsigned int)field) - 1)); \
		sdr_write32(reg, tv); \
	} while (0)

#define sdr_get_field(reg, field, val) \
	do {	\
		unsigned int tv = sdr_read32(reg);	\
		val = ((tv & (field)) >> (uffs((unsigned int)field) - 1)); \
	} while (0)

#define sdr_set_field_discrete(reg, field, val) \
	do {	\
		unsigned int tv = sdr_read32(reg); \
		tv = (val == 1) ? (tv|(field)):(tv & ~(field));\
		sdr_write32(reg, tv); \
	} while (0)

#define sdr_get_field_discrete(reg, field, val) \
	do {	\
		unsigned int tv = sdr_read32(reg); \
		val = tv & (field); \
		val = (val == field) ? 1 : 0;\
	} while (0)

static inline unsigned int sdr_clksrc(unsigned int id)
{
	switch (id) {
	case 0:
		return PERI_MSDC0_PDN;
	case 1:
		return PERI_MSDC1_PDN;
	case 2:
		return PERI_MSDC2_PDN;
	case 3:
		return PERI_MSDC3_PDN;
	case 4:
		return PERI_MSDC4_PDN;
	default:
		pr_info("[SD?] Invalid host id = %d\n", id);
		return PERI_MSDC0_PDN;
	}
}

#define REQ_CMD_EIO  (0x1 << 0)
#define REQ_CMD_TMO  (0x1 << 1)
#define REQ_DAT_ERR  (0x1 << 2)
#define REQ_STOP_EIO (0x1 << 3)
#define REQ_STOP_TMO (0x1 << 4)
#define REQ_CMD_BUSY (0x1 << 5)

#define MAX_CSD_CAPACITY (4096 * 512)

#define msdc_txfifocnt()   ((sdr_read32(MSDC_FIFOCS) & MSDC_FIFOCS_TXCNT) >> 16)
#define msdc_rxfifocnt()   ((sdr_read32(MSDC_FIFOCS) & MSDC_FIFOCS_RXCNT) >> 0)
#define msdc_fifo_write32(v)   sdr_write32(MSDC_TXDATA, (v))
#define msdc_fifo_write8(v)    sdr_write8(MSDC_TXDATA, (v))
#define msdc_fifo_read32()   sdr_read32(MSDC_RXDATA)
#define msdc_fifo_read8()    sdr_read8(MSDC_RXDATA)

#define msdc_dma_on()        sdr_clr_bits(MSDC_CFG, MSDC_CFG_PIO)
#define msdc_dma_off()       sdr_set_bits(MSDC_CFG, MSDC_CFG_PIO)

#define sdc_is_busy()          (sdr_read32(SDC_STS) & SDC_STS_SDCBUSY)
#define sdc_is_cmd_busy()      (sdr_read32(SDC_STS) & SDC_STS_CMDBUSY)

#define sdc_send_cmd(cmd, arg) \
	do { \
		sdr_write32(SDC_ARG, (arg)); \
		mb();/* ensure ARG register write is gone */ \
		sdr_write32(SDC_CMD, (cmd)); \
	} while (0)

#define is_card_present(h)     (((struct msdc_host *)(h))->card_inserted)
#define is_card_sdio(h)        (((struct msdc_host *)(h))->hw->sdio.irq.valid)

#ifdef CONFIG_MMC_MTK_EMMC
#define MTK_EMMC_ETT_TO_DRIVER	/* for eMMC off-line apply to driver */
#endif

#define MSDC_PREPARE_FLAG BIT(0)
#define MSDC_ASYNC_FLAG BIT(1)
#define MSDC_MMAP_FLAG BIT(2)

#define UNSTUFF_BITS(resp, start, size)					\
	({								\
		const int __size = size;				\
		const u32 __mask = (__size < 32 ? 1 << __size : 0) - 1;	\
		const int __off = 3 - ((start) / 32);			\
		const int __shft = (start) & 31;			\
		u32 __res;						\
									\
		__res = resp[__off] >> __shft;				\
		if (__size + __shft > 32)				\
			__res |= resp[__off-1] << ((32 - __shft) % 32);	\
		__res & __mask;						\
	})

#define msdc_retry(expr, retry, cnt, id) \
	do { \
		int backup = cnt; \
		while (retry) { \
			if (!(expr)) \
				break; \
			if (cnt-- == 0) { \
				retry--; \
				mdelay(1); \
				cnt = backup; \
			} \
		} \
		WARN_ON(retry == 0); \
	} while (0)

#define msdc_reset(id) \
	do { \
		int retry = 3, cnt = 1000; \
		sdr_set_bits(MSDC_CFG, MSDC_CFG_RST); \
		mb(); /* ensure reset is written */ \
		msdc_retry(sdr_read32(MSDC_CFG) & MSDC_CFG_RST, retry, cnt, id); \
	} while (0)

#define msdc_clr_int() \
	do { \
		u32 val = sdr_read32(MSDC_INT); \
		sdr_write32(MSDC_INT, val); \
	} while (0)

#define msdc_clr_fifo(id) \
	do { \
		int retry = 3, cnt = 1000; \
		sdr_set_bits(MSDC_FIFOCS, MSDC_FIFOCS_CLR); \
		msdc_retry(sdr_read32(MSDC_FIFOCS) & MSDC_FIFOCS_CLR, retry, cnt, id); \
	} while (0)

#define msdc_reset_hw(id) \
	do { \
		msdc_reset(id); \
		msdc_clr_fifo(id); \
		msdc_clr_int(); \
	} while (0)

#define msdc_irq_save(val) \
	do { \
		val = sdr_read32(MSDC_INTEN); \
		sdr_clr_bits(MSDC_INTEN, val); \
	} while (0)

#define msdc_irq_restore(val) \
	sdr_set_bits(MSDC_INTEN, val)

#define HOST_MAX_MCLK       (200000000)	/* (104000000) */
#define HOST_MIN_MCLK       (260000)

#define HOST_MAX_BLKSZ      (2048)

#define MSDC_OCR_AVAIL      (MMC_VDD_28_29 | MMC_VDD_29_30 | MMC_VDD_30_31 | MMC_VDD_31_32 | MMC_VDD_32_33)
/* #define MSDC_OCR_AVAIL      (MMC_VDD_32_33 | MMC_VDD_33_34) */

#define MSDC1_IRQ_SEL       (1 << 9)
#define PDN_REG             (0xF1000010)

#define DEFAULT_DEBOUNCE    (8)	/* 8 cycles */
#define DEFAULT_DTOC        (3)	/* data timeout counter. 65536x40(75/77) /1048576 * 3(83/85) sclk. */

#define CMD_TIMEOUT         (HZ/10 * 5)	/* 100ms x5 */
#define DAT_TIMEOUT         (HZ    * 5)	/* 1000ms x5 */
#define CLK_TIMEOUT         (HZ    * 5)	/* 5s    */
#define POLLING_BUSY		(HZ	   * 3)
#define MAX_DMA_CNT         (64 * 1024 - 512)	/* a single transaction for WIFI may be 50K */

#define MAX_HW_SGMTS        (MAX_BD_NUM)
#define MAX_PHY_SGMTS       (MAX_BD_NUM)
#define MAX_SGMT_SZ         (MAX_DMA_CNT)
#define MAX_REQ_SZ          (512*1024)

#define CMD_TUNE_UHS_MAX_TIME	(2*32*8*8)
#define CMD_TUNE_HS_MAX_TIME	(2*32)

#define READ_TUNE_UHS_CLKMOD1_MAX_TIME	(2*32*32*8)
#define READ_TUNE_UHS_MAX_TIME	(2*32*32)
#define READ_TUNE_HS_MAX_TIME	(2*32)

#define WRITE_TUNE_HS_MAX_TIME	(2*32*32)
#define WRITE_TUNE_UHS_MAX_TIME (2*32*32*8)

#define MSDC_LOWER_FREQ
#define MSDC_MAX_FREQ_DIV   (2)	/* 200 / (4 * 2) */
#define MSDC_MAX_TIMEOUT_RETRY	(1)
#define	MSDC_MAX_POWER_CYCLE	(3)

#ifdef CONFIG_MMC_MTK_EMMC
#define MTK_EMMC_ETT_TO_DRIVER	/* for eMMC off-line apply to driver */
#endif

#define MSDC_DEV_ATTR(name, fmt, val, fmt_type)					\
static ssize_t msdc_attr_##name##_show(struct device *dev, struct device_attribute *attr, char *buf)	\
{										\
	struct mmc_host *mmc = dev_get_drvdata(dev);				\
	struct msdc_host *host = mmc_priv(mmc);					\
	return sprintf(buf, fmt "\n", (fmt_type)val);				\
}										\
static ssize_t msdc_attr_##name##_store(struct device *dev,			\
	struct device_attribute *attr, const char *buf, size_t count)		\
{										\
	fmt_type tmp;								\
	struct mmc_host *mmc = dev_get_drvdata(dev);				\
	struct msdc_host *host = mmc_priv(mmc);					\
	int n = sscanf(buf, fmt, &tmp);						\
	val = (typeof(val))tmp;							\
	return n ? count : -EINVAL;						\
}										\
static DEVICE_ATTR(name, S_IRUGO | S_IWUSR | S_IWGRP, msdc_attr_##name##_show, msdc_attr_##name##_store)

#endif /* __MTK_SD_H */
