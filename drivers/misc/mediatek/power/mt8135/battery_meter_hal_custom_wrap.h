#ifndef _BATTERY_METER_HAL_CUSTOM_DATA_WRAP_H
#define _BATTERY_METER_HAL_CUSTOM_DATA_WRAP_H

/* ============================================================ */
/* define */
/* ============================================================ */
#define CUST_TABT_NUMBER (p_bat_meter_data->cust_tabt_number)
#define VBAT_CHANNEL_NUMBER (p_bat_meter_data->vbat_channel_number)
#define ISENSE_CHANNEL_NUMBER (p_bat_meter_data->isense_channel_number)
#define VCHARGER_CHANNEL_NUMBER (p_bat_meter_data->vcharger_channel_number)
#define VBATTEMP_CHANNEL_NUMBER (p_bat_meter_data->vbattemp_channel_number)

#define R_BAT_SENSE (p_bat_meter_data->r_bat_sense)
#define R_I_SENSE (p_bat_meter_data->r_i_sense)
#define R_CHARGER_1 (p_bat_meter_data->r_charger_1)
#define R_CHARGER_2 (p_bat_meter_data->r_charger_2)

#define TEMPERATURE_T0 110	/* (p_bat_meter_data->temperature_t0) */
#define TEMPERATURE_T1 0	/* (p_bat_meter_data->tempearture_t1) */
#define TEMPERATURE_T2 25	/* (p_bat_meter_data->temperature_t2) */
#define TEMPERATURE_T3 50	/* (p_bat_meter_data->temperature_t3) */
#define TEMPERATURE_T 255	/* (p_bat_meter_data->temperature_t) */

#define FG_METER_RESISTANCE (p_bat_meter_data->fg_meter_resistance)

#define Q_MAX_POS_50 (p_bat_meter_data->q_max_pos_50)
#define Q_MAX_POS_25 (p_bat_meter_data->q_max_pos_25)
#define Q_MAX_POS_0 (p_bat_meter_data->q_max_pos_0)
#define Q_MAX_NEG_10 (p_bat_meter_data->q_max_neg_10)

#define Q_MAX_POS_50_H_CURRENT (p_bat_meter_data->q_max_pos_50_h_current)
#define Q_MAX_POS_25_H_CURRENT (p_bat_meter_data->q_max_pos_25_h_current)
#define Q_MAX_POS_0_H_CURRENT (p_bat_meter_data->q_max_pos_0_h_current)
#define Q_MAX_NEG_10_H_CURRENT (p_bat_meter_data->q_max_neg_10_h_current)

/* Discharge Percentage */
#define OAM_D5 (p_bat_meter_data->oam_d5)

#define CUST_TRACKING_POINT (p_bat_meter_data->cust_tracking_point)
#define CUST_R_SENSE (p_bat_meter_data->cust_r_sense)
#define CUST_HW_CC (p_bat_meter_data->cust_hw_cc)
#define AGING_TUNING_VALUE (p_bat_meter_data->aging_tuning_value)
#define CUST_R_FG_OFFSET (p_bat_meter_data->cust_r_fg_offset)

#define OCV_BOARD_COMPESATE (p_bat_meter_data->ocv_board_compesate)	/* mV */
#define R_FG_BOARD_BASE (p_bat_meter_data->r_fg_board_base)
#define R_FG_BOARD_SLOPE (p_bat_meter_data->r_fg_board_slope)
#define CAR_TUNE_VALUE (p_bat_meter_data->car_tune_value)

/* HW Fuel gague  */
#define CURRENT_DETECT_R_FG (p_bat_meter_data->current_detect_r_fg)
#define MinErrorOffset (p_bat_meter_data->min_error_offset)
#define FG_VBAT_AVERAGE_SIZE (p_bat_meter_data->fg_vbat_average_size)
#define R_FG_VALUE (p_bat_meter_data->r_fg_value)

#define CUST_POWERON_DELTA_CAPACITY_TOLRANCE (p_bat_meter_data->poweron_delta_capacity_tolerance)
#define CUST_POWERON_LOW_CAPACITY_TOLRANCE (p_bat_meter_data->poweron_low_capacity_tolerance)
#define CUST_POWERON_MAX_VBAT_TOLRANCE (p_bat_meter_data->poweron_max_vbat_tolerance)
#define CUST_POWERON_DELTA_VBAT_TOLRANCE (p_bat_meter_data->poweron_delta_vbat_tolerance)

#define VBAT_NORMAL_WAKEUP (p_bat_meter_data->vbat_normal_wakeup)
#define VBAT_LOW_POWER_WAKEUP (p_bat_meter_data->vbat_low_power_wakeup)
#define NORMAL_WAKEUP_PERIOD (p_bat_meter_data->normal_wakeup_period)
#define LOW_POWER_WAKEUP_PERIOD (p_bat_meter_data->low_power_wakeup_period)
#define CLOSE_POWEROFF_WAKEUP_PERIOD (p_bat_meter_data->close_poweroff_wakeup_period)
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

#endif				/* #ifndef _BATTERY_METER_HAL_CUSTOM_DATA_WRAP_H */
