/* linux/power/max77665-charger.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_MAX77665_CHARGER_H
#define __LINUX_MAX77665_CHARGER_H

enum max77665_charger_fchgtime
{
	MAX77665_FCHGTIME_DISABLE,
	MAX77665_FCHGTIME_4H,
	MAX77665_FCHGTIME_6H,
	MAX77665_FCHGTIME_8H,
	MAX77665_FCHGTIME_10H,
	MAX77665_FCHGTIME_12H,
	MAX77665_FCHGTIME_14H,
	MAX77665_FCHGTIME_16H,
};

enum max77665_charger_chg_rstrt
{
	MAX77665_CHG_RSTRT_100MV,
	MAX77665_CHG_RSTRT_150MV,
	MAX77665_CHG_RSTRT_200MV,
	MAX77665_CHG_RSTRT_DISABLE,
};

enum max77665_charger_to_ith
{
	MAX77665_CHG_TO_ITH_100MA,
	MAX77665_CHG_TO_ITH_125MA,
	MAX77665_CHG_TO_ITH_150MA,
	MAX77665_CHG_TO_ITH_175MA,
	MAX77665_CHG_TO_ITH_200MA,
	MAX77665_CHG_TO_ITH_250MA,
	MAX77665_CHG_TO_ITH_300MA,
	MAX77665_CHG_TO_ITH_350MA,
};

enum max77665_charger_top_off_timer
{
	MAX77665_CHG_TO_TIME_0MIN,
	MAX77665_CHG_TO_TIME_10MIN,
	MAX77665_CHG_TO_TIME_20MIN,
	MAX77665_CHG_TO_TIME_30MIN,
	MAX77665_CHG_TO_TIME_40MIN,
	MAX77665_CHG_TO_TIME_50MIN,
	MAX77665_CHG_TO_TIME_60MIN,
	MAX77665_CHG_TO_TIME_70MIN,
};

enum max77665_charger_chg_cv_prm
{
	MAX77665_CHG_CV_PRM_3650MV,
	MAX77665_CHG_CV_PRM_3675MV,
	MAX77665_CHG_CV_PRM_3700MV,
	MAX77665_CHG_CV_PRM_3725MV,
	MAX77665_CHG_CV_PRM_3750MV,
	MAX77665_CHG_CV_PRM_3775MV,
	MAX77665_CHG_CV_PRM_3800MV,
	MAX77665_CHG_CV_PRM_3825MV,
	MAX77665_CHG_CV_PRM_3850MV,
	MAX77665_CHG_CV_PRM_3875MV,
	MAX77665_CHG_CV_PRM_3900MV,
	MAX77665_CHG_CV_PRM_3925MV,
	MAX77665_CHG_CV_PRM_3950MV,
	MAX77665_CHG_CV_PRM_3975MV,
	MAX77665_CHG_CV_PRM_4000MV,
	MAX77665_CHG_CV_PRM_4025MV,
	MAX77665_CHG_CV_PRM_4050MV,
	MAX77665_CHG_CV_PRM_4075MV,
	MAX77665_CHG_CV_PRM_4100MV,
	MAX77665_CHG_CV_PRM_4125MV,
	MAX77665_CHG_CV_PRM_4150MV,
	MAX77665_CHG_CV_PRM_4175MV,
	MAX77665_CHG_CV_PRM_4200MV,
	MAX77665_CHG_CV_PRM_4225MV,
	MAX77665_CHG_CV_PRM_4250MV,
	MAX77665_CHG_CV_PRM_4275MV,
	MAX77665_CHG_CV_PRM_4300MV,
	MAX77665_CHG_CV_PRM_4325MV,
	MAX77665_CHG_CV_PRM_4340MV,
	MAX77665_CHG_CV_PRM_4350MV,
	MAX77665_CHG_CV_PRM_4375MV,
	MAX77665_CHG_CV_PRM_4400MV,
};

struct max77665_charger_pdata
{
	int irq;
	enum max77665_charger_fchgtime fast_charge_timer;
	enum max77665_charger_chg_rstrt charging_restart_thresold;
	int fast_charge_current_ac;		/* 0uA ~ 2100000uA */
	int fast_charge_current_usb;		/* 0uA ~ 2100000uA */
	enum max77665_charger_to_ith top_off_current_thresold;
	enum max77665_charger_top_off_timer top_off_timer;
	enum max77665_charger_chg_cv_prm charger_termination_voltage;
#ifdef CONFIG_MAX77665_JEITA
	bool enable_jeita;
	int jeita_temp_t1;
	int jeita_temp_t2;
	int jeita_temp_t3;
	int jeita_temp_t4;
	enum max77665_charger_chg_cv_prm charger_termination_voltage_jeita;
#endif
};

#ifdef CONFIG_MAX77665_JEITA
void max77665_charger_enable(struct power_supply *charger, bool enable);
void max77665_change_jeita_status(struct power_supply *charger,
			bool reduce_voltage, bool half_current);
#endif

#endif
