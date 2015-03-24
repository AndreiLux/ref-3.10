/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _AUDDRV_AFE_H_
#define _AUDDRV_AFE_H_

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include <mach/mt_clkmgr.h>
/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

#ifndef MT8135_AUD_REG
#define MT8135_AUD_REG
#endif

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/

/*****************************************************************************
 *                          C O N S T A N T S
 *****************************************************************************/
#define AUDIO_HW_PHYSICAL_BASE  (0x12070000)
#define AUDIO_HW_VIRTUAL_BASE   (0xF2070000)
#ifdef AUDIO_MEM_IOREMAP
#define AFE_BASE                (0)
#else
#define AFE_BASE                (AUDIO_HW_VIRTUAL_BASE)
#endif

/* Internal sram: 0x12004000~0x12007FFF (16K) */
#define AFE_INTERNAL_SRAM_PHY_BASE  (AUDIO_HW_PHYSICAL_BASE-0x70000+0x8000)
#define AFE_INTERNAL_SRAM_VIR_BASE  (AUDIO_HW_VIRTUAL_BASE - 0x70000+0x8000)
#define AFE_INTERNAL_SRAM_SIZE  (0x4000)

/* Dram */
#define AFE_EXTERNAL_DRAM_SIZE  (0x4000)
/*****************************************************************************
 *                         M A C R O
 *****************************************************************************/

/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/
/* Fix build warning */
#undef AUDIO_TOP_CON0
/* End */
#define AUDIO_TOP_CON0          (AFE_BASE + 0x0000)
#ifdef MT8135_AUD_REG
#define AUDIO_TOP_CON1          (AFE_BASE + 0x0004)	/* 8135 new */
#endif
#define AUDIO_TOP_CON3          (AFE_BASE + 0x000C)
#define AFE_DAC_CON0            (AFE_BASE + 0x0010)
#define AFE_DAC_CON1            (AFE_BASE + 0x0014)
#define AFE_I2S_CON             (AFE_BASE + 0x0018)
#define AFE_DAIBT_CON0          (AFE_BASE + 0x001c)

#define AFE_CONN0       (AFE_BASE + 0x0020)
#define AFE_CONN1       (AFE_BASE + 0x0024)
#define AFE_CONN2       (AFE_BASE + 0x0028)
#define AFE_CONN3       (AFE_BASE + 0x002C)
#define AFE_CONN4       (AFE_BASE + 0x0030)

#define AFE_I2S_CON1    (AFE_BASE + 0x0034)
#define AFE_I2S_CON2    (AFE_BASE + 0x0038)
#define AFE_MRGIF_CON           (AFE_BASE + 0x003C)

/* Memory interface */
#define AFE_DL1_BASE     (AFE_BASE + 0x0040)
#define AFE_DL1_CUR       (AFE_BASE + 0x0044)
#define AFE_DL1_END       (AFE_BASE + 0x0048)
#define AFE_DL2_BASE     (AFE_BASE + 0x0050)
#define AFE_DL2_CUR       (AFE_BASE + 0x0054)
#define AFE_DL2_END       (AFE_BASE + 0x0058)
#define AFE_AWB_BASE   (AFE_BASE + 0x0070)
#define AFE_AWB_END     (AFE_BASE + 0x0078)
#define AFE_AWB_CUR     (AFE_BASE + 0x007C)
#define AFE_VUL_BASE     (AFE_BASE + 0x0080)
#define AFE_VUL_END       (AFE_BASE + 0x0088)
#define AFE_VUL_CUR       (AFE_BASE + 0x008C)
#define AFE_DAI_BASE            (AFE_BASE + 0x0090)
#define AFE_DAI_END             (AFE_BASE + 0x0098)
#define AFE_DAI_CUR             (AFE_BASE + 0x009C)

#define AFE_IRQ_CON             (AFE_BASE + 0x00A0)

/* Memory interface monitor */
#define AFE_MEMIF_MON0 (AFE_BASE + 0x00D0)
#define AFE_MEMIF_MON1 (AFE_BASE + 0x00D4)
#define AFE_MEMIF_MON2 (AFE_BASE + 0x00D8)
#define AFE_MEMIF_MON3          (AFE_BASE + 0x00DC)
#define AFE_MEMIF_MON4 (AFE_BASE + 0x00E0)

#ifdef MT8135_AUD_REG
#define AFE_ADDA_DL_SRC2_CON0   (AFE_BASE + 0x0108)	/* 8135 new */
#define AFE_ADDA_DL_SRC2_CON1   (AFE_BASE + 0x010C)	/* 8135 new */
#define AFE_ADDA_UL_SRC_CON0    (AFE_BASE + 0x0114)	/* 8135 new */
#define AFE_ADDA_UL_SRC_CON1    (AFE_BASE + 0x0118)	/* 8135 new */
#define AFE_ADDA_TOP_CON0       (AFE_BASE + 0x0120)	/* 8135 new */
#define AFE_ADDA_UL_DL_CON0     (AFE_BASE + 0x0124)	/* 8135 new */
#define AFE_ADDA_SRC_DEBUG      (AFE_BASE + 0x012C)	/* 8135 new */
#define AFE_ADDA_SRC_DEBUG_MON0 (AFE_BASE + 0x0130)	/* 8135 new */
#define AFE_ADDA_SRC_DEBUG_MON1 (AFE_BASE + 0x0134)	/* 8135 new */
#define AFE_ADDA_NEWIF_CFG0     (AFE_BASE + 0x0138)	/* 8135 new */
#define AFE_ADDA_NEWIF_CFG1     (AFE_BASE + 0x013C)	/* 8135 new */
#endif

#define AFE_FOC_CON             (AFE_BASE + 0x0170)
#define AFE_FOC_CON1            (AFE_BASE + 0x0174)
#define AFE_FOC_CON2            (AFE_BASE + 0x0178)
#define AFE_FOC_CON3            (AFE_BASE + 0x017C)
#define AFE_FOC_CON4            (AFE_BASE + 0x0180)
#define AFE_FOC_CON5            (AFE_BASE + 0x0184)
#define AFE_MON_STEP            (AFE_BASE + 0x0188)

#ifdef MT8135_AUD_REG
#define FOC_ROM_SIG             (AFE_BASE + 0x018C)	/* 8135 new, change address */
#endif

#define AFE_SIDETONE_DEBUG  (AFE_BASE + 0x01D0)
#define AFE_SIDETONE_MON    (AFE_BASE + 0x01D4)
#define AFE_SIDETONE_CON0   (AFE_BASE + 0x01E0)
#define AFE_SIDETONE_COEFF  (AFE_BASE + 0x01E4)
#define AFE_SIDETONE_CON1   (AFE_BASE + 0x01E8)
#define AFE_SIDETONE_GAIN   (AFE_BASE + 0x01EC)
#define AFE_SGEN_CON0         (AFE_BASE + 0x01F0)

#define AFE_TOP_CON0    (AFE_BASE + 0x0200)

#ifdef MT8135_AUD_REG
#define AFE_ADDA_PREDIS_CON0    (AFE_BASE + 0x0260)	/* 8135 new, change name */
#define AFE_ADDA_PREDIS_CON1    (AFE_BASE + 0x0264)	/* 8135 new, change name */
#else
#define AFE_PREDIS_CON0         (AFE_BASE + 0x0260)	/* 6589 old, change name */
#define AFE_PREDIS_CON1         (AFE_BASE + 0x0264)	/* 6589 old, change name */
#endif

#define AFE_MRGIF_MON0          (AFE_BASE + 0x0270)
#define AFE_MRGIF_MON1          (AFE_BASE + 0x0274)
#define AFE_MRGIF_MON2          (AFE_BASE + 0x0278)

#define AFE_MOD_PCM_BASE (AFE_BASE + 0x0330)
#define AFE_MOD_PCM_END  (AFE_BASE + 0x0338)
#define AFE_MOD_PCM_CUR  (AFE_BASE + 0x033C)

#ifdef MT8135_AUD_REG
#define AFE_HDMI_OUT_CON0       (AFE_BASE + 0x0370)	/* 8135 new */
#define AFE_HDMI_OUT_BASE       (AFE_BASE + 0x0374)	/* 8135 new */
#define AFE_HDMI_OUT_CUR        (AFE_BASE + 0x0378)	/* 8135 new */
#define AFE_HDMI_OUT_END        (AFE_BASE + 0x037C)	/* 8135 new */
#define AFE_SPDIF_OUT_CON0      (AFE_BASE + 0x0380)	/* 8135 new */
#define AFE_SPDIF_BASE          (AFE_BASE + 0x0384)	/* 8135 new */
#define AFE_SPDIF_CUR           (AFE_BASE + 0x0388)	/* 8135 new */
#define AFE_SPDIF_END           (AFE_BASE + 0x038C)	/* 8135 new */
#define AFE_HDMI_CONN0          (AFE_BASE + 0x0390)	/* 8135 new */
#define AFE_8CH_I2S_OUT_CON     (AFE_BASE + 0x0394)	/* 8135 new */
#endif

#define AFE_IRQ_MCU_CON             (AFE_BASE + 0x03A0)
#define AFE_IRQ_STATUS      (AFE_BASE + 0x03A4)
#define AFE_IRQ_CLR              (AFE_BASE + 0x03A8)
#define AFE_IRQ_CNT1        (AFE_BASE + 0x03AC)
#define AFE_IRQ_CNT2        (AFE_BASE + 0x03B0)
#define AFE_IRQ_MON2        (AFE_BASE + 0x03B8)
#define AFE_IRQ_CNT5        (AFE_BASE + 0x03BC)
#define AFE_IRQ1_CNT_MON    (AFE_BASE + 0x03C0)
#define AFE_IRQ2_CNT_MON    (AFE_BASE + 0x03C4)
#define AFE_IRQ1_EN_CNT_MON (AFE_BASE + 0x03C8)
#define AFE_IRQ5_EN_CNT_MON (AFE_BASE + 0x03cc)
#define AFE_MEMIF_MINLEN        (AFE_BASE + 0x03D0)
#define AFE_MEMIF_MAXLEN        (AFE_BASE + 0x03D4)
#define AFE_MEMIF_PBUF_SIZE   (AFE_BASE + 0x03D8)

/* AFE GAIN CONTROL REGISTER */
#define AFE_GAIN1_CON0         (AFE_BASE + 0x0410)
#define AFE_GAIN1_CON1         (AFE_BASE + 0x0414)
#define AFE_GAIN1_CON2         (AFE_BASE + 0x0418)
#define AFE_GAIN1_CON3         (AFE_BASE + 0x041C)
#define AFE_GAIN1_CONN         (AFE_BASE + 0x0420)
#define AFE_GAIN1_CUR          (AFE_BASE + 0x0424)
#define AFE_GAIN2_CON0         (AFE_BASE + 0x0428)
#define AFE_GAIN2_CON1         (AFE_BASE + 0x042C)
#define AFE_GAIN2_CON2         (AFE_BASE + 0x0430)
#define AFE_GAIN2_CON3         (AFE_BASE + 0x0434)
#define AFE_GAIN2_CONN         (AFE_BASE + 0x0438)
#define AFE_GAIN2_CUR          (AFE_BASE + 0x043C)
#define AFE_GAIN2_CONN2        (AFE_BASE + 0x0440)

#ifdef MT8135_AUD_REG
#define AFE_IEC_CFG             (AFE_BASE + 0x0480)	/* 8135 new */
#define AFE_IEC_NSNUM           (AFE_BASE + 0x0484)	/* 8135 new */
#define AFE_IEC_BURST_INFO      (AFE_BASE + 0x0488)	/* 8135 new */
#define AFE_IEC_BURST_LEN       (AFE_BASE + 0x048C)	/* 8135 new */
#define AFE_IEC_NSADR           (AFE_BASE + 0x0490)	/* 8135 new */
#define AFE_IEC_CHL_STAT0       (AFE_BASE + 0x04A0)	/* 8135 new */
#define AFE_IEC_CHL_STAT1       (AFE_BASE + 0x04A4)	/* 8135 new */
#define AFE_IEC_CHR_STAT0       (AFE_BASE + 0x04A8)	/* 8135 new */
#define AFE_IEC_CHR_STAT1       (AFE_BASE + 0x04AC)	/* 8135 new */
#endif

/* here is only fpga needed */
#ifdef MT8135_AUD_REG
#define FPGA_CFG2               (AFE_BASE + 0x04B8)	/* 8135 new */
#define FPGA_CFG3               (AFE_BASE + 0x04BC)	/* 8135 new */
#endif
#define FPGA_CFG0           (AFE_BASE + 0x04C0)
#define FPGA_CFG1           (AFE_BASE + 0x04C4)
#define FPGA_VERSION            (AFE_BASE + 0x04C8)
#define FPGA_STC                (AFE_BASE + 0x04CC)

#ifndef MT8135_AUD_REG
#define DBG_MON0                (AFE_BASE + 0x04D0)	/* 6589 old */
#define DBG_MON1                (AFE_BASE + 0x04D4)	/* 6589 old */
#define DBG_MON2                (AFE_BASE + 0x04D8)	/* 6589 old */
#define DBG_MON3                (AFE_BASE + 0x04DC)	/* 6589 old */
#define DBG_MON4                (AFE_BASE + 0x04E0)	/* 6589 old */
#define DBG_MON5                (AFE_BASE + 0x04E4)	/* 6589 old */
#define DBG_MON6                (AFE_BASE + 0x04E8)	/* 6589 old */
#endif

#define AFE_ASRC_CON0           (AFE_BASE + 0x0500)
#define AFE_ASRC_CON1           (AFE_BASE + 0x0504)
#define AFE_ASRC_CON2           (AFE_BASE + 0x0508)
#define AFE_ASRC_CON3           (AFE_BASE + 0x050C)
#define AFE_ASRC_CON4           (AFE_BASE + 0x0510)
#define AFE_ASRC_CON5           (AFE_BASE + 0x0514)
#define AFE_ASRC_CON6           (AFE_BASE + 0x0518)
#define AFE_ASRC_CON7           (AFE_BASE + 0x051C)
#define AFE_ASRC_CON8           (AFE_BASE + 0x0520)
#define AFE_ASRC_CON9           (AFE_BASE + 0x0524)
#define AFE_ASRC_CON10          (AFE_BASE + 0x0528)
#define AFE_ASRC_CON11          (AFE_BASE + 0x052C)

#define PCM_INTF_CON1           (AFE_BASE + 0x0530)
#define PCM_INTF_CON2           (AFE_BASE + 0x0538)
#define PCM2_INTF_CON           (AFE_BASE + 0x053C)

#ifndef MT8135_AUD_REG
#define FOC_ROM_SIG             (AFE_BASE + 0x0630)	/* 6589 old, change address */
#endif

/**********************************
 *  Detailed Definitions
 **********************************/

/* AFE_TOP_CON0 */
#define PDN_AFE  2
#define PDN_ADC  5
#define PDN_I2S  6
#define APB_W2T 12
#define APB_R2T 13
#define APB_SRC 14
#define PDN_APLL_TUNER 19
#define PDN_HDMI_CK    20
#define PDN_SPDF_CK    21

/* AUDIO_TOP_CON3 */
#define HDMI_SPEAKER_OUT_HDMI_POS 5
#define HDMI_SPEAKER_OUT_HDMI_LEN 1
#define HDMI_BCK_DIV_LEN      6
#define HDMI_BCK_DIV_POS      8
#define HDMI_2CH_SEL_POS      6
#define HDMI_2CH_SEL_LEN      2
#define HDMI_2CH_SEL_SDATA0 0
#define HDMI_2CH_SEL_SDATA1 1
#define HDMI_2CH_SEL_SDATA2 2
#define HDMI_2CH_SEL_SDATA3 3

/* AFE_DAC_CON0 */
#define AFE_ON      0
#define DL1_ON      1
#define DL2_ON      2
#define VUL_ON      3
#define DAI_ON      4
#define I2S_ON      5
#define AWB_ON      6
#define MOD_PCM_ON  7
#define AFE_ON_RETM  12
#define AFE_DL1_RETM 13
#define AFE_DL2_RETM 14
#define AFE_AWB_RETM 16

/* AFE_DAC_CON1 */
#define DL1_MODE_LEN    4
#define DL1_MODE_POS    0

#define DL2_MODE_LEN    4
#define DL2_MODE_POS    4

#define I2S_MODE_LEN    4
#define I2S_MODE_POS    8

#define AWB_MODE_LEN    4
#define AWB_MODE_POS    12

#define VUL_MODE_LEN    4
#define VUL_MODE_POS    16

#define DAI_MODE_LEN    1
#define DAI_MODE_POS    20

#define DL1_DATA_LEN    1
#define DL1_DATA_POS    21

#define DL2_DATA_LEN    1
#define DL2_DATA_POS    22

#define I2S_DATA_LEN    1
#define I2S_DATA_POS    23

#define AWB_DATA_LEN    1
#define AWB_DATA_POS    24

#define AWB_R_MONO_LEN  1
#define AWB_R_MONO_POS  25

#define VUL_DATA_LEN    1
#define VUL_DATA_POS    27

#define VUL_R_MONO_LEN  1
#define VUL_R_MONO_POS  28

#define DAI_DUP_WR_LEN  1
#define DAI_DUP_WR_POS  29

#define MOD_PCM_MODE_LEN    1
#define MOD_PCM_MODE_POS    30

#define MOD_PCM_DUP_WR_LEN  1
#define MOD_PCM_DUP_WR_POS  31

/* AFE_I2S_CON1 and AFE_I2S_CON2 */
#define AI2S_EN_POS             0
#define AI2S_EN_LEN             1
#define AI2S_WLEN_POS           1
#define AI2S_WLEN_LEN           1
#define AI2S_FMT_POS            3
#define AI2S_FMT_LEN            1
#define AI2S_OUT_MODE_POS       8
#define AI2S_OUT_MODE_LEN       4
#define AI2S_UPDATE_WORD_POS    24
#define AI2S_UPDATE_WORD_LEN    5
#define AI2S_LR_SWAP_POS        31
#define AI2S_LR_SWAP_LEN        1

#define I2S_EN_POS          0
#define I2S_EN_LEN          1
#define I2S_WLEN_POS        1
#define I2S_WLEN_LEN        1
#define I2S_SRC_POS         2
#define I2S_SRC_LEN         1
#define I2S_FMT_POS         3
#define I2S_FMT_LEN         1
#define I2S_DIR_POS         4
#define I2S_DIR_LEN         1
#define I2S_OUT_MODE_POS    8
#define I2S_OUT_MODE_LEN    4

#define FOC_EN_POS  0
#define FOC_EN_LEN  1

/* Modem PCM 1 */
#define PCM_EN_POS          0
#define PCM_EN_LEN          1

#define PCM_FMT_POS         1
#define PCM_FMT_LEN         2

#define PCM_MODE_POS        3
#define PCM_MODE_LEN        1

#define PCM_WLEN_POS        4
#define PCM_WLEN_LEN        1

#define PCM_SLAVE_POS       5
#define PCM_SLAVE_LEN       1

#define PCM_BYP_ASRC_POS    6
#define PCM_BYP_ASRC_LEN    1

#define PCM_BTMODE_POS      7
#define PCM_BTMODE_LEN      1

#define PCM_SYNC_TYPE_POS   8
#define PCM_SYNC_TYPE_LEN   1

#define PCM_SYNC_LEN_POS    9
#define PCM_SYNC_LEN_LEN    5

#define PCM_EXT_MODEM_POS   17
#define PCM_EXT_MODEM_LEN   1

#define PCM_VBT16K_MODE_POS 18
#define PCM_VBT16K_MODE_LEN 1

/* #define PCM_BCKINV_POS      6 */
/* #define PCM_BCKINV_LEN      1 */
/* #define PCM_SYNCINV_POS     7 */
/* #define PCM_SYNCINV_LEN     1 */

#define PCM_SERLOOPBK_POS   28
#define PCM_SERLOOPBK_LEN   1

#define PCM_PARLOOPBK_POS   29
#define PCM_PARLOOPBK_LEN   1

#define PCM_BUFLOOPBK_POS   30
#define PCM_BUFLOOPBK_LEN   1

#define PCM_FIX_VAL_SEL_POS 31
#define PCM_FIX_VAL_SEL_LEN 1

/* BT PCM */
#define DAIBT_EN_POS   0
#define DAIBT_EN_LEN   1
#define BTPCM_EN_POS   1
#define BTPCM_EN_LEN   1
#define BTPCM_SYNC_POS   2
#define BTPCM_SYNC_LEN   1
#define DAIBT_DATARDY_POS   3
#define DAIBT_DATARDY_LEN   1
#define BTPCM_LENGTH_POS   4
#define BTPCM_LENGTH_LEN   3
#define DAIBT_MODE_POS   9
#define DAIBT_MODE_LEN   1

/* AFE_IRQ_CON */
#define IRQ1_ON         0
#define IRQ2_ON         1
#define IRQ3_ON         2
#define IRQ4_ON         3
#define IRQ1_FS         4
#define IRQ2_FS         8
#define IRQ5_ON         12
#define IRQ6_ON         13
#define IRQ_SETTING_BIT 0x3007

/* AFE_IRQ_STATUS */
#define IRQ1_ON_BIT     (1<<0)
#define IRQ2_ON_BIT     (1<<1)
#define IRQ3_ON_BIT     (1<<2)
#define IRQ4_ON_BIT     (1<<3)
#define IRQ5_ON_BIT     (1<<4)
#define IRQ6_ON_BIT     (1<<5)
#define IRQ_STATUS_BIT  0x3F

/* AFE_IRQ_CLR */
#define IRQ1_CLR (1<<0)
#define IRQ2_CLR (1<<1)
#define IRQ3_CLR (1<<2)
#define IRQ4_CLR (1<<3)
#define IRQ_CLR  (1<<4)

#define IRQ1_MISS_CLR (1<<8)
#define IRQ2_MISS_CLR (1<<9)
#define IRQ3_MISS_CLR (1<<10)
#define IRQ4_MISS_CLR (1<<11)
#define IRQ5_MISS_CLR (1<<12)
#define IRQ6_MISS_CLR (1<<13)

/* AFE_IRQ_MCU_MON2 */
#define IRQ1_MISS_BIT       (1<<8)
#define IRQ2_MISS_BIT       (1<<9)
#define IRQ3_MISS_BIT       (1<<10)
#define IRQ4_MISS_BIT       (1<<11)
#define IRQ5_MISS_BIT       (1<<12)
#define IRQ6_MISS_BIT       (1<<13)
#define IRQ_MISS_STATUS_BIT 0x3F00

/* AUDIO_TOP_CON3 */
#define HDMI_OUT_SPEAKER_BIT    4
#define SPEAKER_OUT_HDMI        5
#define HDMI_2CH_SEL_POS        6
#define HDMI_2CH_SEL_LEN        2

/* AFE_SIDETONE_DEBUG */
#define STF_SRC_SEL     16
#define STF_I5I6_SEL    19

/* AFE_SIDETONE_CON0 */
#define STF_COEFF_VAL       0
#define STF_COEFF_ADDRESS   16
#define STF_CH_SEL          23
#define STF_COEFF_W_ENABLE  24
#define STF_W_ENABLE        25
#define STF_COEFF_BIT       0x0000FFFF

/* AFE_SIDETONE_CON1 */
#define STF_TAP_NUM 0
#define STF_ON      8
#define STF_BYPASS  31

/* AFE_SGEN_CON0 */
#define SINE_TONE_FREQ_DIV_CH1  0
#define SINE_TONE_AMP_DIV_CH1   5
#define SINE_TONE_MODE_CH1      8
#define SINE_TONE_FREQ_DIV_CH2  12
#define SINE_TONE_AMP_DIV_CH2   17
#define SINE_TONE_MODE_CH2      20
#define SINE_TONE_MUTE_CH1      24
#define SINE_TONE_MUTE_CH2      25
#define SINE_TONE_ENABLE        26
#define SINE_TONE_LOOPBACK_MOD  28

/* FPGA_CFG0 */
#define MCLK_MUX2_POS    26
#define MCLK_MUX2_LEN    1
#define MCLK_MUX1_POS    25
#define MCLK_MUX1_LEN    1
#define MCLK_MUX0_POS    24
#define MCLK_MUX0_LEN    1
#define SOFT_RST_POS   16
#define SOFT_RST_LEN   8
#define HOP26M_SEL_POS   12
#define HOP26M_SEL_LEN   2

/* FPGA_CFG1 */
#define CODEC_SEL_POS   0
#define DAC_SEL_POS     4
#define ADC_SEL_POS     8


/* #define AUDPLL_CON0   (APMIXED_BASE+0x02E8) */
#define AUDPLL_CON1   (APMIXED_BASE+0x02EC)
#define AUDPLL_CON4   (APMIXED_BASE+0x02F8)

/* apmixed sys AUDPLL_CON4 */
#define AUDPLL_SDM_PCW_98M       0x3C7EA932
#define AUDPLL_SDM_PCW_90M       0x37945EA6
#define AUDPLL_TUNER_N_98M       0x3C7EA933	/* 48k-based , 98.304M , sdm_pcw+1 */
#define AUDPLL_TUNER_N_90M       0x37945EA7	/* 44.1k-based , 90.3168M, sdm_pcw+1 */

/* AUDPLL_CON0 */
#define AUDPLL_EN_POS            0
#define AUDPLL_EN_LEN            1
#define AUDPLL_PREDIV_POS        4
#define AUDPLL_PREDIV_LEN        2
#define AUDPLL_POSDIV_POS        6
#define AUDPLL_POSDIV_LEN        3
/* AUDPLL_CON1 */
#define AUDPLL_SDM_PCW_POS       0
#define AUDPLL_SDM_PCW_LEN      31
#define AUDPLL_SDM_PCW_CHG_POS  31
#define AUDPLL_SDM_PCW_CHG_LEN   1
/* AUDPLL_CON4 */
#define AUDPLL_TUNER_N_INFO_POS  0
#define AUDPLL_TUNER_N_INFO_LEN 31
#define AUDPLL_TUNER_N_INFO_MASK 0x7FFFFFFF
#define AUDPLL_TUNER_EN_POS     31
#define AUDPLL_TUNER_EN_LEN      1
#define AUDPLL_TUNER_EN_MASK     0x80000000

/* #define CLK_CFG_9       (TOPCKEGN_BASE+0x0168) */
#define CLK_APLL_SEL_POS     16
#define CLK_APLL_SEL_LEN     3
#define CLKSQ_MUX_CK     0
#define AD_APLL_CK       1
#define APLL_D4          2
#define APLL_D8          3
#define APLL_D16         4
#define APLL_D24        5
#define PDN_APLL_POS     23
#define PDN_APLL_LEN      1

void Afe_Set_Reg(uint32_t offset, uint32_t value, uint32_t mask);
uint32_t Afe_Get_Reg(uint32_t offset);

/* for debug usage */
void Afe_Log_Print(void);

/* apmixed sys AUDPLL_CON4 */
#define AUDPLL_SDM_PCW_98M       0x3C7EA932
#define AUDPLL_SDM_PCW_90M       0x37945EA6
#define AUDPLL_TUNER_N_98M       0x3C7EA933	/* 48k-based , 98.304M , sdm_pcw+1 */
#define AUDPLL_TUNER_N_90M       0x37945EA7	/* 44.1k-based , 90.3168M, sdm_pcw+1 */

void AP_Set_Reg(uint32_t offset, uint32_t value, uint32_t mask);
/* nt32 AP_Get_Reg(uint32 offset); */

#endif
