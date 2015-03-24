#ifndef _BATTERY_CHARGING_CUSTOM_DATA_WRAP_H
#define _BATTERY_CHARGING_CUSTOM_DATA_WRAP_H

/* ============================================================ */
/* define */
/* ============================================================ */

#define TALKING_RECHARGE_VOLTAGE (p_bat_charging_data->talking_recharge_voltage)
#define TALKING_SYNC_TIME (p_bat_charging_data->talking_sync_time)

#define MAX_CHARGE_TEMPERATURE  (p_bat_charging_data->max_charge_temperature)
#define MIN_CHARGE_TEMPERATURE  (p_bat_charging_data->min_charge_temperature)
#define ERR_CHARGE_TEMPERATURE  (p_bat_charging_data->err_charge_temperature)
#define USE_AVG_TEMPERATURE  (p_bat_charging_data->use_avg_temperature)

#define V_PRE2CC_THRES			(p_bat_charging_data->v_pre2cc_thres)
#define V_CC2TOPOFF_THRES		(p_bat_charging_data->v_cc2topoff_thres)
#define RECHARGING_VOLTAGE      (p_bat_charging_data->recharging_voltage)
#define CHARGING_FULL_CURRENT    (p_bat_charging_data->charging_full_current)

#define USB_CHARGER_CURRENT_SUSPEND			(p_bat_charging_data->usb_charger_current_suspend)
#define USB_CHARGER_CURRENT_UNCONFIGURED	(p_bat_charging_data->usb_charger_current_unconfigured)
#define USB_CHARGER_CURRENT_CONFIGURED		(p_bat_charging_data->usb_charger_current_configured)

#define USB_CHARGER_CURRENT					(p_bat_charging_data->usb_charger_current)
#define AC_CHARGER_CURRENT					(p_bat_charging_data->ac_charger_current)
#define NON_STD_AC_CHARGER_CURRENT			(p_bat_charging_data->non_std_ac_charger_current)
#define CHARGING_HOST_CHARGER_CURRENT       (p_bat_charging_data->charging_host_charger_current)
#define APPLE_0_5A_CHARGER_CURRENT          (p_bat_charging_data->apple_0_5a_charger_current)
#define APPLE_1_0A_CHARGER_CURRENT          (p_bat_charging_data->apple_1_0a_charger_current)
#define APPLE_2_1A_CHARGER_CURRENT         (p_bat_charging_data->apple_2_1a_charger_current)

#define V_CHARGER_ENABLE (p_bat_charging_data->v_charger_enable)
#define V_CHARGER_MAX (p_bat_charging_data->v_charger_max)
#define V_CHARGER_MIN (p_bat_charging_data->v_charger_min)

#define ONEHUNDRED_PERCENT_TRACKING_TIME	10	/* (p_bat_charging_data->onehundred_percent_tracking_time) */
#define NPERCENT_TRACKING_TIME				20	/* (p_bat_charging_data->npercent_tracking_time) */
#define SYNC_TO_REAL_TRACKING_TIME		(p_bat_charging_data->sync_to_real_tracking_time)

#define CUST_SOC_JEITA_SYNC_TIME 30
#define JEITA_RECHARGE_VOLTAGE  (p_bat_charging_data->jeita_recharge_voltage)
#define JEITA_TEMP_ABOVE_POS_60_CV_VOLTAGE		(p_bat_charging_data->jeita_temp_above_pos_60_cv_voltage)
#define JEITA_TEMP_POS_45_TO_POS_60_CV_VOLTAGE	(p_bat_charging_data->jeita_temp_pos_45_to_pos_60_cv_voltage)
#define JEITA_TEMP_POS_10_TO_POS_45_CV_VOLTAGE	(p_bat_charging_data->jeita_temp_pos_10_to_pos_45_cv_voltage)
#define JEITA_TEMP_POS_0_TO_POS_10_CV_VOLTAGE	(p_bat_charging_data->jeita_temp_pos_0_to_pos_10_cv_voltage)
#define JEITA_TEMP_NEG_10_TO_POS_0_CV_VOLTAGE	(p_bat_charging_data->jeita_temp_neg_10_to_pos_0_cv_voltage)
#define JEITA_TEMP_BELOW_NEG_10_CV_VOLTAGE		(p_bat_charging_data->jeita_temp_below_neg_10_cv_voltage)

#define TEMP_POS_60_THRESHOLD  (p_bat_charging_data->temp_pos_60_threshold)
#define TEMP_POS_60_THRES_MINUS_X_DEGREE (p_bat_charging_data->temp_pos_60_thres_minus_x_degree)
#define TEMP_POS_45_THRESHOLD  (p_bat_charging_data->temp_pos_45_threshold)
#define TEMP_POS_45_THRES_MINUS_X_DEGREE (p_bat_charging_data->temp_pos_45_thres_minus_x_degree)
#define TEMP_POS_10_THRESHOLD  (p_bat_charging_data->temp_pos_10_threshold)
#define TEMP_POS_10_THRES_PLUS_X_DEGREE (p_bat_charging_data->temp_pos_10_thres_plus_x_degree)
#define TEMP_POS_0_THRESHOLD (p_bat_charging_data->temp_pos_0_threshold)
#define TEMP_POS_0_THRES_PLUS_X_DEGREE (p_bat_charging_data->temp_pos_0_thres_plus_x_degree)
#define TEMP_NEG_10_THRESHOLD (p_bat_charging_data->temp_neg_10_threshold)
#define TEMP_NEG_10_THRES_PLUS_X_DEGREE (p_bat_charging_data->temp_neg_10_thres_plus_x_degree)

#define JEITA_NEG_10_TO_POS_0_FULL_CURRENT              (p_bat_charging_data->talking_recharge_voltage)
#define JEITA_TEMP_POS_45_TO_POS_60_RECHARGE_VOLTAGE    (p_bat_charging_data->jeita_neg_10_to_pos_0_full_current)
#define JEITA_TEMP_POS_10_TO_POS_45_RECHARGE_VOLTAGE    (p_bat_charging_data->jeita_neg_10_to_pos_0_full_current)
#define JEITA_TEMP_POS_0_TO_POS_10_RECHARGE_VOLTAGE     (p_bat_charging_data->jeita_temp_pos_10_to_pos_45_recharge_voltage)
#define JEITA_TEMP_NEG_10_TO_POS_0_RECHARGE_VOLTAGE     (p_bat_charging_data->jeita_temp_pos_0_to_pos_10_recharge_voltage)
#define JEITA_TEMP_POS_45_TO_POS_60_CC2TOPOFF_THRESHOLD	(p_bat_charging_data->jeita_temp_pos_45_to_pos_60_cc2topoff_threshold)
#define JEITA_TEMP_POS_10_TO_POS_45_CC2TOPOFF_THRESHOLD	(p_bat_charging_data->jeita_temp_pos_10_to_pos_45_cc2topoff_threshold)
#define JEITA_TEMP_POS_0_TO_POS_10_CC2TOPOFF_THRESHOLD	(p_bat_charging_data->jeita_temp_pos_0_to_pos_10_cc2topoff_threshold)
#define JEITA_TEMP_NEG_10_TO_POS_0_CC2TOPOFF_THRESHOLD	(p_bat_charging_data->jeita_temp_neg_10_to_pos_0_cc2topoff_threshold)


/* ============================================================ */
/* ENUM */
/* ============================================================ */

/* ============================================================ */
/* structure */
/* ============================================================ */

/* ============================================================ */
/* typedef */
/* ============================================================ */

/* ============================================================ */
/* External Variables */
/* ============================================================ */

/* ============================================================ */
/* External function */
/* ============================================================ */

#endif				/* #ifndef _BATTERY_CHARGING_CUSTOM_DATA_WRAP_H */
