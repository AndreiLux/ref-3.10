/*****************************************************************************
 *
 * Filename:
 * ---------
 *    auxadc.h
 *
 * Project:
 * --------
 *   MT6573 DVT
 *
 * Description:
 * ------------
 *   This file is for Auxiliary ADC Unit.
 *
 * Author:
 * -------
 *  Myron Li
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#ifndef _DVT_ADC_TS_H
#define _DVT_ADC_TS_H

#include <mach/devs.h>
#include <mach/mt_irq.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_typedefs.h>
#include <mach/irqs.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_sleep.h>
#include <mach/mt_spm.h>

#define MT65XX_IRQ_LOWBAT_LINE    MT_LOWBATTERY_IRQ_ID
#define APCONFIG_BASE 0		/* only for build */
#define TDMA_TIMER_BASE 0
#define MDCONFIG_BASE 0
#define APMCU_CG_CLR0                   (APCONFIG_BASE + 0x0308)
#define APMCU_CG_SET0                   (APCONFIG_BASE + 0x0304)

#define MDMCU_CG_CON0                   (MDCONFIG_BASE + 0x0300)
#define MDMCU_CG_SET0                   (MDCONFIG_BASE + 0x0304)

#define TDMA_AUXEV0                     (TDMA_TIMER_BASE + 0x0400)
#define TDMA_AUXEV1                     (TDMA_TIMER_BASE + 0x0404)

#define AUXADC_CON0                     (AUXADC_BASE + 0x000)
#define AUXADC_CON1                     (AUXADC_BASE + 0x004)
#define AUXADC_CON1_SET                 (AUXADC_BASE + 0x008)
#define AUXADC_CON1_CLR                 (AUXADC_BASE + 0x00C)
#define AUXADC_CON2                     (AUXADC_BASE + 0x010)
/* efine AUXADC_CON3                     (AUXADC_BASE + 0x014) */

#define AUXADC_DAT0                     (AUXADC_BASE + 0x014)
#define AUXADC_DAT1                     (AUXADC_BASE + 0x018)
#define AUXADC_DAT2                     (AUXADC_BASE + 0x01C)
#define AUXADC_DAT3                     (AUXADC_BASE + 0x020)
#define AUXADC_DAT4                     (AUXADC_BASE + 0x024)
#define AUXADC_DAT5                     (AUXADC_BASE + 0x028)
#define AUXADC_DAT6                     (AUXADC_BASE + 0x02C)
#define AUXADC_DAT7                     (AUXADC_BASE + 0x030)
#define AUXADC_DAT8                     (AUXADC_BASE + 0x034)
#define AUXADC_DAT9                     (AUXADC_BASE + 0x038)
#define AUXADC_DAT10                   (AUXADC_BASE + 0x03C)
#define AUXADC_DAT11                   (AUXADC_BASE + 0x040)
#define AUXADC_DAT12                   (AUXADC_BASE + 0x044)
#define AUXADC_DAT13                   (AUXADC_BASE + 0x048)
#define AUXADC_DAT14                   (AUXADC_BASE + 0x04C)
#define AUXADC_DAT15                   (AUXADC_BASE + 0x050)

#define AUXADC_DET_VOLT                 (AUXADC_BASE + 0x084)
#define AUXADC_DET_SEL                  (AUXADC_BASE + 0x088)
#define AUXADC_DET_PERIOD               (AUXADC_BASE + 0x08C)
#define AUXADC_DET_DEBT                 (AUXADC_BASE + 0x090)
#define AUXADC_MISC                     (AUXADC_BASE + 0x094)
#define AUXADC_ECC                      (AUXADC_BASE + 0x098)
#define AUXADC_SAMPLE_LIST              (AUXADC_BASE + 0x09c)
#define AUXADC_ABIST_PERIOD             (AUXADC_BASE + 0x0A0)
#define AUXADC_TST                      (AUXADC_BASE + 0x0A4)
#define AUXADC_SPL_EN                   (AUXADC_BASE + 0x0B0)


#define BASE_VALUE   (100)
#define SET_AUXADC_CON0                 (BASE_VALUE + 1)
#define SET_AUXADC_CON1                 (BASE_VALUE + 2)
#define SET_AUXADC_CON2                 (BASE_VALUE + 3)
/* efine SET_AUXADC_CON3                 (BASE_VALUE + 4) */
#define SET_AUXADC_DAT0                 (BASE_VALUE + 5)
#define SET_AUXADC_DAT1                 (BASE_VALUE + 6)
#define SET_AUXADC_DAT2                 (BASE_VALUE + 7)
#define SET_AUXADC_DAT3                 (BASE_VALUE + 8)
#define SET_AUXADC_DAT4                 (BASE_VALUE + 9)
#define SET_AUXADC_DAT5                 (BASE_VALUE + 10)
#define SET_AUXADC_DAT6                 (BASE_VALUE + 11)
#define SET_AUXADC_DAT7                 (BASE_VALUE + 12)
#define SET_AUXADC_DAT8                 (BASE_VALUE + 13)
#define SET_AUXADC_DAT9                 (BASE_VALUE + 14)
#define SET_AUXADC_DAT10                (BASE_VALUE + 15)
#define SET_AUXADC_DAT11                (BASE_VALUE + 16)
#define SET_AUXADC_DAT12                (BASE_VALUE + 17)
#define SET_AUXADC_DAT13                (BASE_VALUE + 18)
#define SET_AUXADC_DET_VOLT             (BASE_VALUE + 19)
#define SET_AUXADC_DET_SEL              (BASE_VALUE + 20)
#define SET_AUXADC_DET_PERIOD           (BASE_VALUE + 21)
#define SET_AUXADC_DET_DEBT             (BASE_VALUE + 22)
#define SET_AUXADC_TXPWR_CH             (BASE_VALUE + 23)
#define SET_AUXADC_DAT14                (BASE_VALUE + 24)
#define SET_AUXADC_DAT15                (BASE_VALUE + 25)
#define SET_AUXADC_CON1_SET                 (BASE_VALUE + 26)
#define SET_AUXADC_CON1_CLR                 (BASE_VALUE + 27)
#if 0
#define SET_AUXADC_2GTX_CH              (BASE_VALUE + 24)
#define SET_AUXADC_2GTX_DAT0            (BASE_VALUE + 25)
#define SET_AUXADC_2GTX_DAT1            (BASE_VALUE + 26)
#define SET_AUXADC_2GTX_DAT2            (BASE_VALUE + 27)
#define SET_AUXADC_2GTX_DAT3            (BASE_VALUE + 28)
#define SET_AUXADC_2GTX_DAT4            (BASE_VALUE + 29)
#define SET_AUXADC_2GTX_DAT5            (BASE_VALUE + 30)
#endif
#define GET_AUXADC_CON0                 (BASE_VALUE + 31)
#define GET_AUXADC_CON1                 (BASE_VALUE + 32)
#define GET_AUXADC_CON2                 (BASE_VALUE + 33)
#define GET_AUXADC_CON3                 (BASE_VALUE + 34)
#define GET_AUXADC_DAT0                 (BASE_VALUE + 35)
#define GET_AUXADC_DAT1                 (BASE_VALUE + 36)
#define GET_AUXADC_DAT2                 (BASE_VALUE + 37)
#define GET_AUXADC_DAT3                 (BASE_VALUE + 38)
#define GET_AUXADC_DAT4                 (BASE_VALUE + 39)
#define GET_AUXADC_DAT5                 (BASE_VALUE + 40)
#define GET_AUXADC_DAT6                 (BASE_VALUE + 41)
#define GET_AUXADC_DAT7                 (BASE_VALUE + 42)
#define GET_AUXADC_DAT8                 (BASE_VALUE + 43)
#define GET_AUXADC_DAT9                 (BASE_VALUE + 44)
#define GET_AUXADC_DAT10                (BASE_VALUE + 45)
#define GET_AUXADC_DAT11                (BASE_VALUE + 46)
#define GET_AUXADC_DAT12                (BASE_VALUE + 47)
#define GET_AUXADC_DAT13                (BASE_VALUE + 48)
#define GET_AUXADC_DET_VOLT             (BASE_VALUE + 49)
#define GET_AUXADC_DET_SEL              (BASE_VALUE + 50)
#define GET_AUXADC_DET_PERIOD           (BASE_VALUE + 51)
#define GET_AUXADC_DET_DEBT             (BASE_VALUE + 52)
#define GET_AUXADC_TXPWR_CH             (BASE_VALUE + 53)
#define GET_AUXADC_DAT14                (BASE_VALUE + 54)
#define GET_AUXADC_DAT15                (BASE_VALUE + 55)
#if 0
#define GET_AUXADC_2GTX_CH              (BASE_VALUE + 54)
#define GET_AUXADC_2GTX_DAT0            (BASE_VALUE + 55)
#define GET_AUXADC_2GTX_DAT1            (BASE_VALUE + 56)
#define GET_AUXADC_2GTX_DAT2            (BASE_VALUE + 57)
#define GET_AUXADC_2GTX_DAT3            (BASE_VALUE + 58)
#define GET_AUXADC_2GTX_DAT4            (BASE_VALUE + 59)
#define GET_AUXADC_2GTX_DAT5            (BASE_VALUE + 60)
#endif
#define SET_ADC_WAKE_SRC	(BASE_VALUE + 61)
#define ENABLE_SYN_MODE                 (BASE_VALUE + 80)
#define DISABLE_SYN_MODE                (BASE_VALUE + 81)

#define ENABLE_ADC_RUN                  (BASE_VALUE + 84)
#define DISABLE_ADC_RUN                 (BASE_VALUE + 85)

#define ENABLE_BG_DETECT                (BASE_VALUE + 86)
#define DISABLE_BG_DETECT               (BASE_VALUE + 87)

#define ENABLE_ADC_CLOCK                (BASE_VALUE + 88)
#define DISABLE_ADC_CLOCK               (BASE_VALUE + 89)
#define ENABLE_ADC_DCM                  (BASE_VALUE + 90)

#if 0
#define ENABLE_3G_TX                    (BASE_VALUE + 88)
#define DISABLE_3G_TX                   (BASE_VALUE + 89)

#define ENABLE_2G_TX                    (BASE_VALUE + 90)
#define DISABLE_2G_TX                   (BASE_VALUE + 91)
#endif
#define SET_DET_VOLT                    (BASE_VALUE + 92)
#define GET_DET_VOLT                    (BASE_VALUE + 93)
#define SET_DET_PERIOD                  (BASE_VALUE + 94)
#define GET_DET_PERIOD                  (BASE_VALUE + 95)
#define SET_DET_DEBT                    (BASE_VALUE + 96)
#define GET_DET_DEBT                    (BASE_VALUE + 97)
#define ENABLE_ADC_AUTO_SET             (BASE_VALUE + 98)
#define DISABLE_ADC_AUTO_SET            (BASE_VALUE + 99)

#define GET_ALL_REG_VAL                 (BASE_VALUE + 100)

#define ENABLE_ADC_LOG                  (98)
#define DISABLE_ADC_LOG                 (99)

/* #define MT65XXIRQ_LOWBAT_CODE          (81) */

#endif
