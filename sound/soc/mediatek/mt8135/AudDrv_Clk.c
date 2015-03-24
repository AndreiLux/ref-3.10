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

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/
#include <mach/mt_clkmgr.h>
#include <mach/mt_pm_ldo.h>
#include <mach/pmic_mt6397_sw.h>
#include <drivers/misc/mediatek/power/mt8135/upmu_common.h>
#include <mach/upmu_hw.h>

#include "AudDrv_Common.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include <linux/spinlock.h>
#include <linux/delay.h>

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/
int Aud_Core_Clk_cntr;
int Aud_AFE_Clk_cntr;
int Aud_ADC_Clk_cntr;
int Aud_I2S_Clk_cntr;
int Aud_ANA_Clk_cntr;
int Aud_LineIn_Clk_cntr;
int Aud_HDMI_Clk_cntr;
int Afe_Mem_Pwr_on;
int Aud_APLL_Tuner_Clk_cntr;
int Aud_SPDIF_Clk_cntr;
int Aud_HDMI_DVT_Clk_cntr;
int Aud_TOP_APLL_Clk_cntr;
static DEFINE_SPINLOCK(auddrv_Clk_lock);
/* amp mutex lock */
static DEFINE_MUTEX(auddrv_pmic_mutex);

/*****************************************************************************
 *                         INLINE FUNCTION
 *****************************************************************************/
inline void TURN_ON_AFE_CLOCK(void)
{
	if (enable_clock(MT_CG_INFRA_AUDIO_PDN, "AUDIO"))
		pr_err("%s enable_clock MT_CG_INFRA_AUDIO_PDN fail\n", __func__);

	if (enable_clock(MT_CG_AUDIO_PDN_AFE, "AUDIO"))
		pr_err("%s enable_clock MT_CG_AUDIO_PDN_AFE fail\n", __func__);
}

inline void TURN_OFF_AFE_CLOCK(void)
{
	if (disable_clock(MT_CG_AUDIO_PDN_AFE, "AUDIO"))
		pr_err("%s disable_clock MT_CG_AUDIO_PDN_AFE fail\n", __func__);

	if (disable_clock(MT_CG_INFRA_AUDIO_PDN, "AUDIO"))
		pr_err("%s disable_clock MT_CG_INFRA_AUDIO_PDN fail\n", __func__);
}

inline void TURN_ON_HDMI_CLOCK(void)
{
	if (enable_clock(MT_CG_INFRA_AUDIO_PDN, "AUDIO"))
		pr_err("%s enable_clock MT_CG_INFRA_AUDIO_PDN fail\n", __func__);

	if (enable_clock(MT_CG_AUDIO_PDN_HDMI_CK, "AUDIO"))
		pr_err("%s enable_clock MT_CG_AUDIO_PDN_HDMI_CK fail\n", __func__);
}

inline void TURN_OFF_HDMI_CLOCK(void)
{
	if (disable_clock(MT_CG_AUDIO_PDN_HDMI_CK, "AUDIO"))
		pr_err("%s disable_clock MT_CG_AUDIO_PDN_HDMI_CK fail\n", __func__);

	if (disable_clock(MT_CG_INFRA_AUDIO_PDN, "AUDIO"))
		pr_err("%s disable_clock MT_CG_INFRA_AUDIO_PDN fail\n", __func__);
}

inline void TURN_ON_APLL_TUNER_CLOCK(void)
{
	if (enable_clock(MT_CG_INFRA_AUDIO_PDN, "AUDIO"))
		pr_err("%s enable_clock MT_CG_INFRA_AUDIO_PDN fail\n", __func__);

	if (enable_clock(MT_CG_AUDIO_PDN_APLL_TUNER, "AUDIO"))
		pr_err("%s enable_clock MT_CG_AUDIO_PDN_APLL_TUNER fail\n", __func__);
}

inline void TURN_OFF_APLL_TUNER_CLOCK(void)
{
	if (disable_clock(MT_CG_AUDIO_PDN_APLL_TUNER, "AUDIO"))
		pr_err("%s disable_clock MT_CG_AUDIO_PDN_APLL_TUNER fail\n", __func__);

	if (disable_clock(MT_CG_INFRA_AUDIO_PDN, "AUDIO"))
		pr_err("%s disable_clock MT_CG_INFRA_AUDIO_PDN fail\n", __func__);
}

inline void TURN_ON_SPDIF_CLOCK(void)
{
	if (enable_clock(MT_CG_INFRA_AUDIO_PDN, "AUDIO"))
		pr_err("%s enable_clock MT_CG_INFRA_AUDIO_PDN fail\n", __func__);

	if (enable_clock(MT_CG_AUDIO_PDN_SPDF_CK, "AUDIO"))
		pr_err("%s enable_clock MT_CG_AUDIO_PDN_SPDF_CK fail\n", __func__);
}

inline void TURN_OFF_SPDIF_CLOCK(void)
{
	if (disable_clock(MT_CG_AUDIO_PDN_SPDF_CK, "AUDIO"))
		pr_err("%s disable_clock MT_CG_AUDIO_PDN_SPDF_CK fail\n", __func__);

	if (disable_clock(MT_CG_INFRA_AUDIO_PDN, "AUDIO"))
		pr_err("%s disable_clock MT_CG_INFRA_AUDIO_PDN fail\n", __func__);
}

/*****************************************************************************
 * FUNCTION
 *  AudDrv_Clk_On / AudDrv_Clk_Off
 *
 * DESCRIPTION
 *  Enable/Disable PLL(26M clock) \ AFE clock
 *
 *****************************************************************************
 */
void AudDrv_Clk_On(void)
{
	unsigned long flags;
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_AFE_Clk_cntr == 0) {
		pr_debug("+AudDrv_Clk_On, Aud_AFE_Clk_cntr:%d\n", Aud_AFE_Clk_cntr);
#ifdef PM_MANAGER_API
		TURN_ON_AFE_CLOCK();
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004000, 0x00004000);	/* bit2: afe power on */
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004000, 0xffffffff);	/* bit2: afe power on */
#endif
	}
	Aud_AFE_Clk_cntr++;
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	/* PRINTK_AUD_CLK("-AudDrv_Clk_On, Aud_AFE_Clk_cntr:%d\n",Aud_AFE_Clk_cntr); */
}
EXPORT_SYMBOL(AudDrv_Clk_On);

void AudDrv_Clk_Off(void)
{
	unsigned long flags;
	/* PRINTK_AUD_CLK("+!! AudDrv_Clk_Off, Aud_AFE_Clk_cntr:%d\n",Aud_AFE_Clk_cntr); */
	spin_lock_irqsave(&auddrv_Clk_lock, flags);

	Aud_AFE_Clk_cntr--;
	if (Aud_AFE_Clk_cntr == 0) {
		pr_debug("+ AudDrv_Clk_Off, Aud_AFE_Clk_cntr:%d\n", Aud_AFE_Clk_cntr);
		{
			/* Disable AFE clock */
#ifdef PM_MANAGER_API
			Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004004, 0x00004000);	/* bit2: power down afe */
			TURN_OFF_AFE_CLOCK();
#else
			Afe_Set_Reg(AUDIO_TOP_CON0, 0x00000000, 0x00004043);	/* bit2: power on */
#endif
		}
	} else if (Aud_AFE_Clk_cntr < 0) {
		PRINTK_AUD_ERROR("!! AudDrv_Clk_Off, Aud_AFE_Clk_cntr<0 (%d)\n", Aud_AFE_Clk_cntr);
		AUDIO_ASSERT(true);
		Aud_AFE_Clk_cntr = 0;
	}

	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	/* PRINTK_AUD_CLK("-!! AudDrv_Clk_Off, Aud_AFE_Clk_cntr:%d\n",Aud_AFE_Clk_cntr); */
}
EXPORT_SYMBOL(AudDrv_Clk_Off);

/*****************************************************************************
* FUNCTION
*  AudDrv_Suspend_Clk_Off / AudDrv_Suspend_Clk_On
*
* DESCRIPTION
*  Enable/Disable AFE clock for suspend
*
*****************************************************************************
*/
void AudDrv_Suspend_Clk_On(void)
{
	unsigned long flags;
	spin_lock_irqsave(&auddrv_Clk_lock, flags);

	if (Aud_AFE_Clk_cntr > 0) {
		PRINTK_AUD_CLK
		    ("AudDrv_Suspend_Clk_On Aud_AFE_Clk_cntr:%d ANA_Clk(%d) APLL_Tuner_Clk(%d) HDMI_Clk(%d)\n",
		     Aud_AFE_Clk_cntr, Aud_ANA_Clk_cntr, Aud_APLL_Tuner_Clk_cntr,
		     Aud_HDMI_Clk_cntr);
#ifdef PM_MANAGER_API
		/* bit2: afe power on, bit6: I2S power on */
		/* iru: bigfox suggest to set the register after clock enabled */
		/* Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004000, 0x00004000); */
		/* Enable AFE clock */
		TURN_ON_AFE_CLOCK();
		if (Aud_I2S_Clk_cntr > 0) {
			/* Enable I2S clock */
			if (enable_clock(MT_CG_AUDIO_PDN_I2S, "AUDIO"))
				pr_err("%s enable_clock MT_CG_AUDIO_PDN_I2S fail\n", __func__);
		}
		if (Aud_HDMI_Clk_cntr > 0)
			TURN_ON_HDMI_CLOCK();

		if (Aud_APLL_Tuner_Clk_cntr > 0)
			TURN_ON_APLL_TUNER_CLOCK();

		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004000, 0x00004044);	/* bit2: afe power on, bit6: I2S power on */
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004000, 0x00004044);	/* bit2: afe power on, bit6: I2S power on */
#endif
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	if (Aud_ANA_Clk_cntr > 0) {
		PRINTK_AUD_CLK("AudDrv_Suspend_Clk_On Aud_AFE_Clk_cntr:%d ANA_Clk(%d)\n",
			       Aud_AFE_Clk_cntr, Aud_ANA_Clk_cntr);
		upmu_set_rg_clksq_en(1);
	}
	/* PRINTK_AUD_CLK("-AudDrv_Suspend_Clk_On Aud_AFE_Clk_cntr:%d ANA_Clk(%d)\n",
	   Aud_AFE_Clk_cntr,Aud_ANA_Clk_cntr); */
}

void AudDrv_Suspend_Clk_Off(void)
{
	unsigned long flags;
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_AFE_Clk_cntr > 0) {
		PRINTK_AUD_CLK
		    ("AudDrv_Suspend_Clk_Off Aud_AFE_Clk_cntr:%d ANA_Clk(%d) APLL_Tuner_Clk(%d) HDMI_Clk(%d)\n",
		     Aud_AFE_Clk_cntr, Aud_ANA_Clk_cntr, Aud_APLL_Tuner_Clk_cntr,
		     Aud_HDMI_Clk_cntr);
#ifdef PM_MANAGER_API
		/* Disable AFE clock and I2S clock */
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004000, 0x00004000);	/* bit2: afe power off, bit6: I2S power off */

		if (Aud_APLL_Tuner_Clk_cntr > 0)
			TURN_OFF_APLL_TUNER_CLOCK();

		if (Aud_HDMI_Clk_cntr > 0)
			TURN_OFF_APLL_TUNER_CLOCK();

		TURN_OFF_AFE_CLOCK();

		if (Aud_I2S_Clk_cntr > 0) {
			if (disable_clock(MT_CG_AUDIO_PDN_I2S, "AUDIO"))
				pr_err("%s disable_clock MT_CG_AUDIO_PDN_I2S fail\n", __func__);
		}
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004044, 0x00004044);	/* bit2: afe power off, bit6: I2S power off */
#endif
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	if (Aud_ANA_Clk_cntr > 0) {
		PRINTK_AUD_CLK("AudDrv_Suspend_Clk_Off Aud_AFE_Clk_cntr:%d ANA_Clk(%d)\n",
			       Aud_AFE_Clk_cntr, Aud_ANA_Clk_cntr);
		upmu_set_rg_clksq_en(0);
	}
}

/*****************************************************************************
 * FUNCTION
 *  AudDrv_ANA_Top_On / AudDrv_ANA_Top_Off
 *
 * DESCRIPTION
 *  Enable/Disable analog part clock
 *
 *****************************************************************************/
void AudDrv_ANA_Top_On(void)
{
	Ana_Enable_Clk(0x0003, true);
}

void AudDrv_ANA_Top_Off(void)
{
	Ana_Enable_Clk(0x0003, false);
}

/*****************************************************************************
 * FUNCTION
 *  AudDrv_ANA_Clk_On / AudDrv_ANA_Clk_Off
 *
 * DESCRIPTION
 *  Enable/Disable analog part clock
 *
 *****************************************************************************/
void AudDrv_ANA_Clk_On(void)
{
	mutex_lock(&auddrv_pmic_mutex);
	if (Aud_ANA_Clk_cntr == 0) {
		pr_debug("+AudDrv_ANA_Clk_On, Aud_ANA_Clk_cntr:%d\n", Aud_ANA_Clk_cntr);
		upmu_set_rg_clksq_en(1);
		AudDrv_ANA_Top_On();
	}
	Aud_ANA_Clk_cntr++;
	mutex_unlock(&auddrv_pmic_mutex);
	pr_debug("-AudDrv_ANA_Clk_On, Aud_ANA_Clk_cntr:%d\n", Aud_ANA_Clk_cntr);
}
EXPORT_SYMBOL(AudDrv_ANA_Clk_On);

void AudDrv_ANA_Clk_Off(void)
{
	/* PRINTK_AUD_CLK("+AudDrv_ANA_Clk_Off, Aud_ADC_Clk_cntr:%d\n",  Aud_ANA_Clk_cntr); */
	mutex_lock(&auddrv_pmic_mutex);
	Aud_ANA_Clk_cntr--;
	if (Aud_ANA_Clk_cntr == 0) {
		pr_debug("+AudDrv_ANA_Clk_Off disable_clock Ana clk(%x)\n", Aud_ANA_Clk_cntr);
		/* Disable ADC clock */
#ifdef PM_MANAGER_API
		upmu_set_rg_clksq_en(0);
		AudDrv_ANA_Top_Off();
#else
		/* TODO:: open ADC clock.... */
#endif
	} else if (Aud_ANA_Clk_cntr < 0) {
		PRINTK_AUD_ERROR("!! AudDrv_ANA_Clk_Off, Aud_ADC_Clk_cntr<0 (%d)\n",
				 Aud_ANA_Clk_cntr);
		AUDIO_ASSERT(true);
		Aud_ANA_Clk_cntr = 0;
	}
	mutex_unlock(&auddrv_pmic_mutex);
	pr_debug("-AudDrv_ANA_Clk_Off, Aud_ADC_Clk_cntr:%d\n", Aud_ANA_Clk_cntr);
}
EXPORT_SYMBOL(AudDrv_ANA_Clk_Off);

/*****************************************************************************
 * FUNCTION
  *  AudDrv_ADC_Clk_On / AudDrv_ADC_Clk_Off
  *
  * DESCRIPTION
  *  Enable/Disable analog part clock
  *
  *****************************************************************************/
void AudDrv_ADC_Clk_On(void)
{
	/* PRINTK_AUDDRV("+AudDrv_ADC_Clk_On, Aud_ADC_Clk_cntr:%d\n", Aud_ADC_Clk_cntr); */
	mutex_lock(&auddrv_pmic_mutex);

	if (Aud_ADC_Clk_cntr == 0) {
		PRINTK_AUDDRV("+AudDrv_ADC_Clk_On enable_clock ADC clk(%x)\n", Aud_ADC_Clk_cntr);
#ifdef PM_MANAGER_API
		/* hwPowerOn(MT65XX_POWER_LDO_VA28,VOL_2800 , "AUDIO"); */
#endif
	}
	Aud_ADC_Clk_cntr++;
	mutex_unlock(&auddrv_pmic_mutex);
}

void AudDrv_ADC_Clk_Off(void)
{
	/* PRINTK_AUDDRV("+AudDrv_ADC_Clk_Off, Aud_ADC_Clk_cntr:%d\n", Aud_ADC_Clk_cntr); */
	mutex_lock(&auddrv_pmic_mutex);
	Aud_ADC_Clk_cntr--;
	if (Aud_ADC_Clk_cntr == 0) {
		PRINTK_AUDDRV("+AudDrv_ADC_Clk_On disable_clock ADC clk(%x)\n", Aud_ADC_Clk_cntr);
#ifdef PM_MANAGER_API
		/* hwPowerDown(MT65XX_POWER_LDO_VA28, "AUDIO"); */
#endif
	}
	if (Aud_ADC_Clk_cntr < 0) {
		PRINTK_AUDDRV("!! AudDrv_ADC_Clk_Off, Aud_ADC_Clk_cntr<0 (%d)\n", Aud_ADC_Clk_cntr);
		Aud_ADC_Clk_cntr = 0;
	}
	mutex_unlock(&auddrv_pmic_mutex);
	/* PRINTK_AUDDRV("-AudDrv_ADC_Clk_Off, Aud_ADC_Clk_cntr:%d\n", Aud_ADC_Clk_cntr); */
}

/*****************************************************************************
  * FUNCTION
  *  AudDrv_I2S_Clk_On / AudDrv_I2S_Clk_Off
  *
  * DESCRIPTION
  *  Enable/Disable analog part clock
  *
  *****************************************************************************/
void AudDrv_I2S_Clk_On(void)
{
	unsigned long flags;
	/* PRINTK_AUD_CLK("+AudDrv_I2S_Clk_On, Aud_I2S_Clk_cntr:%d\n", Aud_I2S_Clk_cntr); */
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_I2S_Clk_cntr == 0) {
#ifdef PM_MANAGER_API
		if (enable_clock(MT_CG_AUDIO_PDN_I2S, "AUDIO"))
			PRINTK_AUD_ERROR("Aud enable_clock MT65XX_PDN_AUDIO_I2S fail !!!\n");
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00000040, 0x00000040);	/* power on I2S clock */
#endif
	}
	Aud_I2S_Clk_cntr++;
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
}
EXPORT_SYMBOL(AudDrv_I2S_Clk_On);

void AudDrv_I2S_Clk_Off(void)
{
	unsigned long flags;
	/* PRINTK_AUD_CLK("+AudDrv_I2S_Clk_Off, Aud_I2S_Clk_cntr:%d\n", Aud_I2S_Clk_cntr); */
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	Aud_I2S_Clk_cntr--;
	if (Aud_I2S_Clk_cntr == 0) {
#ifdef PM_MANAGER_API
		if (disable_clock(MT_CG_AUDIO_PDN_I2S, "AUDIO"))
			PRINTK_AUD_ERROR("disable_clock MT_CG_AUDIO_PDN_I2S fail");
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00000000, 0x00000040);	/* power off I2S clock */
#endif
	} else if (Aud_I2S_Clk_cntr < 0) {
		PRINTK_AUD_ERROR("!! AudDrv_I2S_Clk_Off, Aud_I2S_Clk_cntr<0 (%d)\n",
				 Aud_I2S_Clk_cntr);
		AUDIO_ASSERT(true);
		Aud_I2S_Clk_cntr = 0;
	}
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	/* PRINTK_AUD_CLK("-AudDrv_I2S_Clk_Off, Aud_I2S_Clk_cntr:%d\n",Aud_I2S_Clk_cntr); */
}
EXPORT_SYMBOL(AudDrv_I2S_Clk_Off);

/*****************************************************************************
  * FUNCTION
  *  AudDrv_Core_Clk_On / AudDrv_Core_Clk_Off
  *
  * DESCRIPTION
  *  Enable/Disable analog part clock
  *
  *****************************************************************************/
void AudDrv_Core_Clk_On(void)
{
	/* PRINTK_AUD_CLK("+AudDrv_Core_Clk_On, Aud_Core_Clk_cntr:%d\n", Aud_Core_Clk_cntr); */
	unsigned long flags;
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_Core_Clk_cntr == 0) {
#ifdef PM_MANAGER_API
		if (enable_clock(MT_CG_INFRA_AUDIO_PDN, "AUDIO"))
			pr_err("%s enable_clock MT_CG_INFRA_AUDIO_PDN fail\n", __func__);

		if (enable_clock(MT_CG_AUDIO_PDN_AFE, "AUDIO"))
			pr_err("%s enable_clock MT_CG_AUDIO_PDN_AFE fail\n", __func__);
#endif
	}
	Aud_Core_Clk_cntr++;
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	/* PRINTK_AUD_CLK("-AudDrv_Core_Clk_On, Aud_Core_Clk_cntr:%d\n", Aud_Core_Clk_cntr); */
}

void AudDrv_Core_Clk_Off(void)
{
	/* PRINTK_AUD_CLK("+AudDrv_Core_Clk_On, Aud_Core_Clk_cntr:%d\n", Aud_Core_Clk_cntr); */
	unsigned long flags;
	spin_lock_irqsave(&auddrv_Clk_lock, flags);
	if (Aud_Core_Clk_cntr == 0) {
#ifdef PM_MANAGER_API
		if (disable_clock(MT_CG_AUDIO_PDN_AFE, "AUDIO"))
			pr_err("%s disable_clock MT_CG_AUDIO_PDN_AFE fail\n", __func__);

		if (disable_clock(MT_CG_INFRA_AUDIO_PDN, "AUDIO"))
			pr_err("%s disable_clock MT_CG_INFRA_AUDIO_PDN fail\n", __func__);
#endif
	}
	Aud_Core_Clk_cntr++;
	spin_unlock_irqrestore(&auddrv_Clk_lock, flags);
	/* PRINTK_AUD_CLK("-AudDrv_Core_Clk_On, Aud_Core_Clk_cntr:%d\n", Aud_Core_Clk_cntr); */
}

/*****************************************************************************
  * FUNCTION
  *  AudDrv_Linein_Clk_On / AudDrv_Linein_Clk_Off
  *
  * DESCRIPTION
  *  Enable/Disable analog part clock
  *
  *****************************************************************************/
void AudDrv_Linein_Clk_On(void)
{
	PRINTK_AUD_CLK("+AudDrv_Linein_Clk_On, Aud_I2S_Clk_cntr:%d\n", Aud_LineIn_Clk_cntr);
	if (Aud_LineIn_Clk_cntr == 0) {
#ifdef PM_MANAGER_API
		AudDrv_ANA_Clk_On();
		AudDrv_Clk_On();
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00000040, 0x00000040);	/* power on I2S clock */
#endif
	}
	Aud_LineIn_Clk_cntr++;
}

void AudDrv_Linein_Clk_Off(void)
{
	PRINTK_AUD_CLK("+AudDrv_Linein_Clk_Off, Aud_I2S_Clk_cntr:%d\n", Aud_LineIn_Clk_cntr);
	Aud_LineIn_Clk_cntr--;
	if (Aud_LineIn_Clk_cntr == 0) {
#ifdef PM_MANAGER_API
		AudDrv_ANA_Clk_On();
		AudDrv_Clk_On();
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00000000, 0x00000040);	/* power off I2S clock */
#endif
	} else if (Aud_LineIn_Clk_cntr < 0) {
		PRINTK_AUD_ERROR("!! AudDrv_Linein_Clk_Off, Aud_I2S_Clk_cntr<0 (%d)\n",
				 Aud_LineIn_Clk_cntr);
		AUDIO_ASSERT(true);
		Aud_LineIn_Clk_cntr = 0;
	}
	PRINTK_AUD_CLK("-AudDrv_I2S_Clk_Off, Aud_I2S_Clk_cntr:%d\n", Aud_LineIn_Clk_cntr);
}

/*****************************************************************************
  * FUNCTION
  *  AudDrv_HDMI_Clk_On / AudDrv_HDMI_Clk_Off
  *
  * DESCRIPTION
  *  Enable/Disable analog part clock
  *
  *****************************************************************************/
/*
void AudDrv_HDMI_Clk_On(void)
{
    PRINTK_AUD_CLK("+AudDrv_HDMI_Clk_On, Aud_I2S_Clk_cntr:%d\n", Aud_HDMI_Clk_cntr);
    if (Aud_HDMI_Clk_cntr == 0)
    {
	AudDrv_ANA_Clk_On();
	AudDrv_Clk_On();
    }
    Aud_HDMI_Clk_cntr++;
}

void AudDrv_HDMI_Clk_Off(void)
{
    PRINTK_AUD_CLK("+AudDrv_HDMI_Clk_Off, Aud_I2S_Clk_cntr:%d\n", Aud_HDMI_Clk_cntr);
    Aud_HDMI_Clk_cntr--;
    if (Aud_HDMI_Clk_cntr == 0)
    {
	AudDrv_ANA_Clk_Off();
	AudDrv_Clk_Off();
    }
    else if (Aud_HDMI_Clk_cntr < 0)
    {
	PRINTK_AUD_ERROR("!! AudDrv_Linein_Clk_Off, Aud_I2S_Clk_cntr<0 (%d)\n", Aud_HDMI_Clk_cntr);
	AUDIO_ASSERT(true);
	Aud_HDMI_Clk_cntr = 0;
    }
    PRINTK_AUD_CLK("-AudDrv_I2S_Clk_Off, Aud_I2S_Clk_cntr:%d\n", Aud_HDMI_Clk_cntr);
}
*/
/*****************************************************************************
  * FUNCTION
  *  AudDrv_SetHDMIClkSource
  *
  * DESCRIPTION
  *  Set HDMI Source Clock
  *
  *****************************************************************************/
void AudDrv_SetHDMIClkSource(UINT32 SampleRate, int apllclksel)
{
	/* TBD */
	UINT32 APLL_TUNER_N_INFO = AUDPLL_TUNER_N_98M;
	UINT32 APLL_SDM_PCW = AUDPLL_SDM_PCW_98M;	/* apll tuner always equal to sdm+1 */
	UINT32 ck_apll = 0;
	UINT32 u4HDMI_BCK_DIV;
	UINT32 BitWidth = 3;	/* default = 32 bits */
	u4HDMI_BCK_DIV = (128 / ((BitWidth + 1) * 8 * 2) / 2) - 1;
	if ((u4HDMI_BCK_DIV < 0) || (u4HDMI_BCK_DIV > 63))
		PRINTK_AUD_CLK("vClockSetting:u4HDMI_BCK_DIV is out of range.\n");

	ck_apll = apllclksel;

	if ((SampleRate == 44100) || (SampleRate == 88200) || (SampleRate == 176400)) {
		APLL_TUNER_N_INFO = AUDPLL_TUNER_N_90M;
		APLL_SDM_PCW = AUDPLL_SDM_PCW_90M;
	}

	switch (apllclksel) {
	case APLL_D4:
		clkmux_sel(MT_CLKMUX_APLL_SEL, MT_CG_V_APLL_D4, "AUDIO");
		break;
	case APLL_D8:
		clkmux_sel(MT_CLKMUX_APLL_SEL, MT_CG_V_APLL_D8, "AUDIO");
		break;
	case APLL_D24:
		clkmux_sel(MT_CLKMUX_APLL_SEL, MT_CG_V_APLL_D24, "AUDIO");
		break;
	case APLL_D16:
	default:		/* default 48k */
		/* APLL_DIV : 2048/128=16 */
		clkmux_sel(MT_CLKMUX_APLL_SEL, MT_CG_V_APLL_D16, "AUDIO");
		break;
	}
	/* Set APLL source clock SDM PCW info */
#ifdef PM_MANAGER_API
	pll_fsel(AUDPLL, APLL_SDM_PCW);
	/* Set HDMI BCK DIV */
	Afe_Set_Reg(AUDIO_TOP_CON3, u4HDMI_BCK_DIV << HDMI_BCK_DIV_POS,
		    ((0x1 << HDMI_BCK_DIV_LEN) - 1) << HDMI_BCK_DIV_POS);
#else
	Afe_Set_Reg(AUDPLL_CON1, APLL_SDM_PCW << AUDPLL_SDM_PCW_POS,
		    (0x1 << AUDPLL_SDM_PCW_LEN - 1) << AUDPLL_SDM_PCW_POS);
	/* Set APLL tuner clock N info */
	Afe_Set_Reg(AUDPLL_CON4, APLL_TUNER_N_INFO << AUDPLL_TUNER_N_INFO_POS,
		    (0x1 << AUDPLL_TUNER_N_INFO_LEN - 1) << AUDPLL_TUNER_N_INFO_POS);
	/* Set MCLK clock */
	Afe_Set_Reg(CLK_CFG_9, ck_apll << CLK_APLL_SEL_POS,
		    (0x1 << CLK_APLL_SEL_LEN - 1) << CLK_APLL_SEL_POS);
	/* Set HDMI BCK DIV */
	Afe_Set_Reg(AUDIO_TOP_CON3, u4HDMI_BCK_DIV << HDMI_BCK_DIV_POS,
		    (0x1 << HDMI_BCK_DIV_LEN - 1) << HDMI_BCK_DIV_POS);
	/* Turn on APLL source clock */
	Afe_Set_Reg(AUDPLL_CON4, 0x1 << AUDPLL_TUNER_EN_POS,
		    (0x1 << 0x1 - 1) << AUDPLL_TUNER_EN_POS);
	/* pdn_apll enable turn on */
	Afe_Set_Reg(CLK_CFG_9, 0x1 << PDN_APLL_POS, (0x1 << 0x1 - 1) << PDN_APLL_POS);
#endif

}

/*****************************************************************************
* FUNCTION
*  AudDrv_TOP_Apll_Clk_On / AudDrv_TOP_Apll_Clk_Off
*
* DESCRIPTION
*  Enable/Disable top apll clock
*
*****************************************************************************/

void AudDrv_TOP_Apll_Clk_On(void)
{
	spin_lock(&auddrv_Clk_lock);
	PRINTK_AUD_CLK("+AudDrv_TOP_Apll_Clk_On, Aud_APLL_Tuner_Clk_cntr:%d\n",
		       Aud_TOP_APLL_Clk_cntr);
	if (Aud_TOP_APLL_Clk_cntr == 0) {
#ifdef PM_MANAGER_API
		if (enable_clock(MT_CG_TOP_PDN_APLL, "AUDIO")) {
			xlog_printk(ANDROID_LOG_ERROR, "Sound",
				    "Aud enable_clock MT_CG_TOP_PDN_APLL fail !!!\n");
		}
#endif
	}
	Aud_TOP_APLL_Clk_cntr++;
	spin_unlock(&auddrv_Clk_lock);
}

void AudDrv_TOP_Apll_Clk_Off(void)
{
	spin_lock(&auddrv_Clk_lock);
	PRINTK_AUD_CLK("+AudDrv_TOP_Apll_Clk_Off, Aud_APLL_Tuner_Clk_cntr:%d\n",
		       Aud_TOP_APLL_Clk_cntr);
	Aud_TOP_APLL_Clk_cntr--;
	if (Aud_TOP_APLL_Clk_cntr == 0) {
		/* Disable apll tuner clock */
#ifdef PM_MANAGER_API
		if (disable_clock(MT_CG_TOP_PDN_APLL, "AUDIO")) {
			xlog_printk(ANDROID_LOG_ERROR, "Sound",
				    "disable_clock MT_CG_TOP_PDN_APLL fail");
		}
#endif
	} else if (Aud_TOP_APLL_Clk_cntr < 0) {
		PRINTK_AUD_ERROR("!! AudDrv_HDMI_Clk_Off, Aud_TOP_APLL_Clk_cntr<0 (%d)\n",
				 Aud_TOP_APLL_Clk_cntr);
		AUDIO_ASSERT(true);
		Aud_TOP_APLL_Clk_cntr = 0;
	}
	PRINTK_AUD_CLK("-AudDrv_TOP_Apll_Clk_Off, Aud_TOP_APLL_Clk_cntr:%d\n",
		       Aud_TOP_APLL_Clk_cntr);
	spin_unlock(&auddrv_Clk_lock);
}

/*****************************************************************************
  * FUNCTION
  *  AudDrv_HDMI_Clk_On / AudDrv_HDMI_Clk_Off
  *
  * DESCRIPTION
  *  Enable/Disable analog part clock
  *
  *****************************************************************************/

void AudDrv_HDMI_Clk_On(void)
{
	spin_lock(&auddrv_Clk_lock);
	PRINTK_AUD_CLK("+AudDrv_HDMI_Clk_On, Aud_HDMI_Clk_cntr:%d\n", Aud_HDMI_Clk_cntr);
	if (Aud_HDMI_Clk_cntr == 0) {
#ifdef PM_MANAGER_API
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004000, 0x00004000);
		TURN_ON_HDMI_CLOCK();
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004000, 0x00104000);
#endif
	}
	Aud_HDMI_Clk_cntr++;
	spin_unlock(&auddrv_Clk_lock);
}

void AudDrv_HDMI_Clk_Off(void)
{
	spin_lock(&auddrv_Clk_lock);
	PRINTK_AUD_CLK("+AudDrv_HDMI_Clk_Off, Aud_HDMI_Clk_cntr:%d\n", Aud_HDMI_Clk_cntr);
	Aud_HDMI_Clk_cntr--;
	if (Aud_HDMI_Clk_cntr == 0) {
		/* Disable HDMI clock */
#ifdef PM_MANAGER_API
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004000, 0x00004000);
		TURN_OFF_HDMI_CLOCK();
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00104000, 0x00104000);
#endif
	} else if (Aud_HDMI_Clk_cntr < 0) {
		PRINTK_AUD_ERROR("!! AudDrv_HDMI_Clk_Off, Aud_HDMI_Clk_cntr<0 (%d)\n",
				 Aud_HDMI_Clk_cntr);
		AUDIO_ASSERT(true);
		Aud_HDMI_Clk_cntr = 0;
	}
	PRINTK_AUD_CLK("-AudDrv_HDMI_Clk_Off, Aud_HDMI_Clk_cntr:%d\n", Aud_HDMI_Clk_cntr);
	spin_unlock(&auddrv_Clk_lock);
}

/*****************************************************************************
  * FUNCTION
  *  AudDrv_APLL_TUNER_Clk_On / AudDrv_APLL_TUNER_Clk_Off
  *
  * DESCRIPTION
  *  Enable/Disable apll tuner clock
  *
  *****************************************************************************/

void AudDrv_APLL_TUNER_Clk_On(void)
{
	spin_lock(&auddrv_Clk_lock);
	PRINTK_AUD_CLK("+AudDrv_APLL_TUNER_Clk_On, Aud_APLL_Tuner_Clk_cntr:%d\n",
		       Aud_APLL_Tuner_Clk_cntr);
	if (Aud_APLL_Tuner_Clk_cntr == 0) {
#ifdef PM_MANAGER_API
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004000, 0x00004000);
		TURN_ON_APLL_TUNER_CLOCK();
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004000, 0x00084000);
#endif
	}
	Aud_APLL_Tuner_Clk_cntr++;
	spin_unlock(&auddrv_Clk_lock);
}

void AudDrv_APLL_TUNER_Clk_Off(void)
{
	spin_lock(&auddrv_Clk_lock);
	PRINTK_AUD_CLK("+AudDrv_APLL_TUNER_Clk_Off, Aud_APLL_Tuner_Clk_cntr:%d\n",
		       Aud_APLL_Tuner_Clk_cntr);
	Aud_APLL_Tuner_Clk_cntr--;
	if (Aud_APLL_Tuner_Clk_cntr == 0) {
		/* Disable apll tuner clock */
#ifdef PM_MANAGER_API
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004000, 0x00004000);
		TURN_OFF_APLL_TUNER_CLOCK();
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00084000, 0x00084000);
#endif
	} else if (Aud_APLL_Tuner_Clk_cntr < 0) {
		PRINTK_AUD_ERROR("!! AudDrv_APLL_TUNER_Clk_Off, Aud_APLL_Tuner_Clk_cntr<0 (%d)\n",
				 Aud_APLL_Tuner_Clk_cntr);
		AUDIO_ASSERT(true);
		Aud_APLL_Tuner_Clk_cntr = 0;
	}
	PRINTK_AUD_CLK("-AudDrv_APLL_TUNER_Clk_Off, Aud_APLL_Tuner_Clk_cntr:%d\n",
		       Aud_APLL_Tuner_Clk_cntr);
	spin_unlock(&auddrv_Clk_lock);
}

/*****************************************************************************
  * FUNCTION
  *  AudDrv_SPDIF_Clk_On / AudDrv_SPDIF_Clk_Off
  *
  * DESCRIPTION
  *  Enable/Disable SPDIF clock
  *
  *****************************************************************************/

void AudDrv_SPDIF_Clk_On(void)
{
	spin_lock(&auddrv_Clk_lock);
	PRINTK_AUD_CLK("+AudDrv_SPDIF_Clk_On, Aud_SPDIF_Clk_cntr:%d\n", Aud_SPDIF_Clk_cntr);
	if (Aud_SPDIF_Clk_cntr == 0) {
#ifdef PM_MANAGER_API
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004000, 0x00004000);
		TURN_ON_SPDIF_CLOCK();
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004000, 0x00204000);
#endif
	}
	Aud_SPDIF_Clk_cntr++;
	spin_unlock(&auddrv_Clk_lock);
}

void AudDrv_SPDIF_Clk_Off(void)
{
	spin_lock(&auddrv_Clk_lock);
	PRINTK_AUD_CLK("+AudDrv_SPDIF_Clk_Off, Aud_SPDIF_Clk_cntr:%d\n", Aud_SPDIF_Clk_cntr);
	Aud_SPDIF_Clk_cntr--;
	if (Aud_SPDIF_Clk_cntr == 0) {
		/* Disable SPDIF clock */
#ifdef PM_MANAGER_API
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00004000, 0x00004000);
		TURN_OFF_SPDIF_CLOCK();
#else
		Afe_Set_Reg(AUDIO_TOP_CON0, 0x00204000, 0x00204000);
#endif
	} else if (Aud_SPDIF_Clk_cntr < 0) {
		PRINTK_AUD_ERROR("!! AudDrv_SPDIF_Clk_Off, Aud_SPDIF_Clk_cntr<0 (%d)\n",
				 Aud_SPDIF_Clk_cntr);
		AUDIO_ASSERT(true);
		Aud_SPDIF_Clk_cntr = 0;
	}
	PRINTK_AUD_CLK("-AudDrv_SPDIF_Clk_Off, Aud_SPDIF_Clk_cntr:%d\n", Aud_SPDIF_Clk_cntr);
	spin_unlock(&auddrv_Clk_lock);
}
