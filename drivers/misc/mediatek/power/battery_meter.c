#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/rtc.h>
#include <linux/xlog.h>
#include <linux/time.h>

#include <asm/uaccess.h>
#include <mach/mt_typedefs.h>
#include <mach/hardware.h>
#include <mach/mt_boot.h>

#include <mach/battery_custom_data.h>
#include <mach/battery_common.h>
#include <mach/battery_meter.h>
#include <mach/battery_meter_hal.h>
#include "battery_meter_custom_wrap.h"

/* ============================================================ // */
/* define */
/* ============================================================ // */
static DEFINE_MUTEX(FGADC_mutex);
static DEFINE_MUTEX(qmax_mutex);

#define CUST_CAPACITY_OCV2CV_TRANSFORM


#ifdef CUST_CAPACITY_OCV2CV_TRANSFORM
#define STEP_OF_QMAX 54  /* 54 mAh */
#define CV_CURRENT 6000 /* 600mA */
static kal_int32 g_currentfactor = 100;
#endif

int Enable_FGADC_LOG = BM_LOG_ERROR;

/* ============================================================ // */
/* global variable */
/* ============================================================ // */
BATTERY_METER_CONTROL battery_meter_ctrl = NULL;

/* static struct proc_dir_entry *proc_entry_fgadc; */
static char proc_fgadc_data[32];

kal_bool gFG_Is_Charging = KAL_FALSE;
kal_int32 g_auxadc_solution = 0;

static mt_battery_meter_custom_data *p_bat_meter_data;

/* Disable Battery check for HQA */
#ifdef CONFIG_MTK_DISABLE_POWER_ON_OFF_VOLTAGE_LIMITATION
#define FIXED_TBAT_25
#endif

#ifdef CONFIG_IDME
static kal_int32 battery_project = -1;
static kal_int32 battery_module = -1;
#endif

/* ///////////////////////////////////////////////////////////////////////////////////////// */
/* // PMIC AUXADC Related Variable */
/* ///////////////////////////////////////////////////////////////////////////////////////// */
int g_R_BAT_SENSE = 0;
int g_R_I_SENSE = 0;
int g_R_CHARGER_1 = 0;
int g_R_CHARGER_2 = 0;
int g_R_FG_offset = 0;

int fg_qmax_update_for_aging_flag = 1;

/* debug purpose */
kal_int32 g_hw_ocv_debug = 0;
kal_int32 g_hw_soc_debug = 0;
kal_int32 g_sw_soc_debug = 0;
kal_int32 g_rtc_soc_debug = 0;

/* HW FG */
kal_int32 gFG_DOD0 = 0;
kal_int32 gFG_DOD1 = 0;
kal_int32 gFG_columb = 0;
kal_int32 gFG_voltage = 0;
kal_int32 gFG_current = 0;
kal_int32 gFG_capacity = 0;
kal_int32 gFG_capacity_by_c = 0;
kal_int32 gFG_capacity_by_c_init = 0;
kal_int32 gFG_capacity_by_v = 0;
kal_int32 gFG_capacity_by_v_init = 0;
kal_int32 gFG_temp = 100;
kal_int32 gFG_resistance_bat = 0;
kal_int32 gFG_compensate_value = 0;
kal_int32 gFG_ori_voltage = 0;
kal_int32 gFG_BATT_CAPACITY = 0;
kal_int32 gFG_voltage_init = 0;
kal_int32 gFG_current_auto_detect_R_fg_total = 0;
kal_int32 gFG_current_auto_detect_R_fg_count = 0;
kal_int32 gFG_current_auto_detect_R_fg_result = 0;
kal_int32 gFG_15_vlot = 3700;
kal_int32 gFG_BATT_CAPACITY_init_high_current = 1200;
kal_int32 gFG_BATT_CAPACITY_aging = 1200;

/* voltage mode */
kal_int32 gfg_percent_check_point = 50;
kal_int32 volt_mode_update_timer = 0;
kal_int32 volt_mode_update_time_out = 6;	/* 1mins */

/* EM */
kal_int32 g_fg_dbg_bat_volt = 0;
kal_int32 g_fg_dbg_bat_current = 0;
kal_int32 g_fg_dbg_bat_zcv = 0;
kal_int32 g_fg_dbg_bat_temp = 0;
kal_int32 g_fg_dbg_bat_r = 0;
kal_int32 g_fg_dbg_bat_car = 0;
kal_int32 g_fg_dbg_bat_qmax = 0;
kal_int32 g_fg_dbg_d0 = 0;
kal_int32 g_fg_dbg_d1 = 0;
kal_int32 g_fg_dbg_percentage = 0;
kal_int32 g_fg_dbg_percentage_fg = 0;
kal_int32 g_fg_dbg_percentage_voltmode = 0;

kal_int32 g_update_qmax_flag = 1;
kal_int32 FGbatteryIndex = 0;
kal_int32 FGbatteryVoltageSum = 0;
kal_int32 gFG_voltage_AVG = 0;
kal_int32 gFG_vbat_offset = 0;
#if 0
kal_int32 FGvbatVoltageBuffer[FG_VBAT_AVERAGE_SIZE];
kal_int32 g_tracking_point = CUST_TRACKING_POINT;
#else
kal_int32 g_tracking_point = 0;
kal_int32 FGvbatVoltageBuffer[36];	/* tmp fix */
#endif

kal_int32 g_rtc_fg_soc = 0;
kal_int32 g_I_SENSE_offset = 0;

/* SW FG */
kal_int32 oam_v_ocv_init = 0;
kal_int32 oam_v_ocv_1 = 0;
kal_int32 oam_v_ocv_2 = 0;
kal_int32 oam_r_1 = 0;
kal_int32 oam_r_2 = 0;
kal_int32 oam_d0 = 0;
kal_int32 oam_i_ori = 0;
kal_int32 oam_i_1 = 0;
kal_int32 oam_i_2 = 0;
kal_int32 oam_car_1 = 0;
kal_int32 oam_car_2 = 0;
kal_int32 oam_d_1 = 1;
kal_int32 oam_d_2 = 1;
kal_int32 oam_d_3 = 1;
kal_int32 oam_d_3_pre = 0;
kal_int32 oam_d_4 = 0;
kal_int32 oam_d_4_pre = 0;
kal_int32 oam_d_5 = 0;
kal_int32 oam_init_i = 0;
kal_int32 oam_run_i = 0;
kal_int32 d5_count = 0;
kal_int32 d5_count_time = 60;
kal_int32 d5_count_time_rate = 1;
kal_int32 g_d_hw_ocv = 0;
kal_int32 g_vol_bat_hw_ocv = 0;
kal_int32 g_hw_ocv_before_sleep = 0;
kal_uint32 g_rtc_time_before_sleep = 0;
kal_int32 g_sw_vbat_temp = 0;


/* aging mechanism */
#ifdef CONFIG_MTK_ENABLE_AGING_ALGORITHM

static kal_int32 aging_ocv_1;
static kal_int32 aging_ocv_2;
static kal_int32 aging_car_1;
static kal_int32 aging_car_2;
static kal_int32 aging_dod_1;
static kal_int32 aging_dod_2;
static time_t aging_resume_time_1;
static time_t aging_resume_time_2;

#ifndef SELF_DISCHARGE_CHECK_THRESHOLD
#define SELF_DISCHARGE_CHECK_THRESHOLD 3
#endif

#ifndef OCV_RECOVER_TIME
#define OCV_RECOVER_TIME 1800
#endif

#ifndef DOD1_ABOVE_THRESHOLD
#define DOD1_ABOVE_THRESHOLD 30
#endif

#ifndef DOD2_BELOW_THRESHOLD
#define DOD2_BELOW_THRESHOLD 70
#endif

#ifndef MIN_DOD_DIFF_THRESHOLD
#define MIN_DOD_DIFF_THRESHOLD 60
#endif

#ifndef MIN_AGING_FACTOR
#define MIN_AGING_FACTOR 90
#endif

#endif				/* aging mechanism */

/* battery info */
#ifdef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT

kal_int32 gFG_battery_cycle = 0;
kal_int32 gFG_aging_factor = 100;
kal_int32 gFG_columb_sum = 0;
kal_int32 gFG_pre_columb_count = 0;

kal_int32 gFG_max_voltage = 0;
kal_int32 gFG_min_voltage = 10000;
kal_int32 gFG_max_current = 0;
kal_int32 gFG_min_current = 0;
kal_int32 gFG_max_temperature = -20;
kal_int32 gFG_min_temperature = 100;

#endif				/* battery info */

/* Temperature window size */
#define TEMP_AVERAGE_SIZE	12

/* ============================================================ // */
int get_r_fg_value(void)
{
	return R_FG_VALUE + g_R_FG_offset;
}

int BattThermistorConverTemp(int Res)
{
	int i = 0;
	int RES1 = 0, RES2 = 0;
	int TBatt_Value = -200, TMP1 = 0, TMP2 = 0;
	int saddles = p_bat_meter_data->battery_ntc_table_saddles;

	if (Res >= Batt_Temperature_Table[0].TemperatureR) {
		TBatt_Value = Batt_Temperature_Table[0].BatteryTemp;
	} else if (Res <= Batt_Temperature_Table[saddles-1].TemperatureR) {
		TBatt_Value = Batt_Temperature_Table[saddles-1].BatteryTemp;
	} else {
		RES1 = Batt_Temperature_Table[0].TemperatureR;
		TMP1 = Batt_Temperature_Table[0].BatteryTemp;

		for (i = 0; i <= saddles-1; i++) {
			if (Res >= Batt_Temperature_Table[i].TemperatureR) {
				RES2 = Batt_Temperature_Table[i].TemperatureR;
				TMP2 = Batt_Temperature_Table[i].BatteryTemp;
				break;
			} else {
				RES1 = Batt_Temperature_Table[i].TemperatureR;
				TMP1 = Batt_Temperature_Table[i].BatteryTemp;
			}
		}

		TBatt_Value = (((Res - RES2) * TMP1) + ((RES1 - Res) * TMP2)) / (RES1 - RES2);
	}

	return TBatt_Value;
}

kal_int32 fgauge_get_Q_max(kal_int16 temperature)
{
	kal_int32 ret_Q_max = 0;
	kal_int32 low_temperature = 0, high_temperature = 0;
	kal_int32 low_Q_max = 0, high_Q_max = 0;

	if (temperature <= TEMPERATURE_T1) {
		low_temperature = (-10);
		low_Q_max = Q_MAX_NEG_10;
		high_temperature = TEMPERATURE_T1;
		high_Q_max = Q_MAX_POS_0;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (battery_profile_t1_5 && temperature <= TEMPERATURE_T1_5) {
		low_temperature = TEMPERATURE_T1;
		low_Q_max = Q_MAX_POS_0;
		high_temperature = TEMPERATURE_T1_5;
		high_Q_max = Q_MAX_POS_10;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (temperature <= TEMPERATURE_T2) {
		low_temperature = TEMPERATURE_T1;
		low_Q_max = Q_MAX_POS_0;
		high_temperature = TEMPERATURE_T2;
		high_Q_max = Q_MAX_POS_25;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else {
		low_temperature = TEMPERATURE_T2;
		low_Q_max = Q_MAX_POS_25;
		high_temperature = TEMPERATURE_T3;
		high_Q_max = Q_MAX_POS_50;

		if (temperature > high_temperature)
			temperature = high_temperature;

	}

	ret_Q_max = low_Q_max + (((temperature - low_temperature) * (high_Q_max - low_Q_max)
				 ) / (high_temperature - low_temperature)
	    );

	bm_print(BM_LOG_FULL, "[fgauge_get_Q_max] Q_max = %d\r\n", ret_Q_max);

	return ret_Q_max;
}


kal_int32 fgauge_get_Q_max_high_current(kal_int16 temperature)
{
	kal_int32 ret_Q_max = 0;
	kal_int32 low_temperature = 0, high_temperature = 0;
	kal_int32 low_Q_max = 0, high_Q_max = 0;

	if (temperature <= TEMPERATURE_T1) {
		low_temperature = (-10);
		low_Q_max = Q_MAX_NEG_10_H_CURRENT;
		high_temperature = TEMPERATURE_T1;
		high_Q_max = Q_MAX_POS_0_H_CURRENT;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (battery_profile_t1_5 && temperature <= TEMPERATURE_T1_5) {
		low_temperature = TEMPERATURE_T1;
		low_Q_max = Q_MAX_POS_0_H_CURRENT;
		high_temperature = TEMPERATURE_T1_5;
		high_Q_max = Q_MAX_POS_10_H_CURRENT;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (temperature <= TEMPERATURE_T2) {
		low_temperature = TEMPERATURE_T1;
		low_Q_max = Q_MAX_POS_0_H_CURRENT;
		high_temperature = TEMPERATURE_T2;
		high_Q_max = Q_MAX_POS_25_H_CURRENT;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else {
		low_temperature = TEMPERATURE_T2;
		low_Q_max = Q_MAX_POS_25_H_CURRENT;
		high_temperature = TEMPERATURE_T3;
		high_Q_max = Q_MAX_POS_50_H_CURRENT;

		if (temperature > high_temperature)
			temperature = high_temperature;

	}

	ret_Q_max = low_Q_max + (((temperature - low_temperature) * (high_Q_max - low_Q_max)
				 ) / (high_temperature - low_temperature)
	    );

	bm_print(BM_LOG_FULL, "[fgauge_get_Q_max_high_current] Q_max = %d\r\n", ret_Q_max);

	return ret_Q_max;
}

int BattVoltToTemp(int dwVolt)
{
	kal_int64 TRes_temp;
	kal_int64 TRes;
	int sBaTTMP = -100;
	kal_int64 critical_low_v;

	/* TRes_temp = ((kal_int64)RBAT_PULL_UP_R*(kal_int64)dwVolt) / (RBAT_PULL_UP_VOLT-dwVolt); */
	/* TRes = (TRes_temp * (kal_int64)RBAT_PULL_DOWN_R)/((kal_int64)RBAT_PULL_DOWN_R - TRes_temp); */

	critical_low_v = (Batt_Temperature_Table[0].TemperatureR * (kal_int64) RBAT_PULL_UP_VOLT);
	do_div(critical_low_v, Batt_Temperature_Table[0].TemperatureR + RBAT_PULL_UP_VOLT);

	if (dwVolt >= critical_low_v)
		TRes_temp = Batt_Temperature_Table[0].TemperatureR;
	else {
		TRes_temp = (RBAT_PULL_UP_R * (kal_int64) dwVolt);
		do_div(TRes_temp, (RBAT_PULL_UP_VOLT - dwVolt));
	}

	TRes = (TRes_temp * RBAT_PULL_DOWN_R);
	do_div(TRes, abs(RBAT_PULL_DOWN_R - TRes_temp));

	/* convert register to temperature */
	sBaTTMP = BattThermistorConverTemp((int)TRes);

	return sBaTTMP;
}

int force_get_tbat(void)
{
#if defined(CONFIG_POWER_EXT) || defined(FIXED_TBAT_25)
	bm_print(BM_LOG_FULL, "[force_get_tbat] fixed TBAT=25 t\n");
	return 25;
#elif defined(CONFIG_SOC_BY_EXT_HW_FG) && defined(CONFIG_EXT_FG_HAS_TEMPERATURE)
	kal_int32 arg = EXT_FG_CMD_TEMPERATURE;

	if (battery_meter_ctrl(BATTERY_METER_CMD_EXT_HW_FG, &arg) == 0)
		return arg;
	else
		return 25;
#else
	int bat_temperature_volt = 0;
	int bat_temperature_val = 0;
	int fg_r_value = 0;
	kal_int32 fg_current_temp = 0;
	kal_bool fg_current_state = KAL_FALSE;
	int bat_temperature_volt_temp = 0;
	int ret = 0;

#if defined(CONFIG_SOC_BY_HW_FG)
	static int vbat_on_discharge = 0, current_discharge;
	static int vbat_on_charge = 0, current_charge;
	int resident_R = 0;
#endif
	static kal_bool battery_check_done = KAL_FALSE;
	static kal_bool battery_exist = KAL_FALSE;
	kal_uint32 baton_count = 0;
	kal_uint32 i;

	if (battery_check_done == KAL_FALSE) {
		for (i = 0; i < 3; i++)
			baton_count += bat_charger_get_battery_status();

		if (baton_count >= 3) {
			pr_warn("[BATTERY] No battery detected.\n");
			battery_exist = KAL_FALSE;
		} else {
			pr_info("[BATTERY] Battery detected!\n");
			battery_exist = KAL_TRUE;
		}
		battery_check_done = KAL_TRUE;
	}

#if defined(CONFIG_FIXED_T25_FOR_NO_BATTERY)
	if (battery_exist == KAL_FALSE)
		return 25;
#endif

	/* Get V_BAT_Temperature */
	bat_temperature_volt = 2;
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_TEMP, &bat_temperature_volt);

	if (bat_temperature_volt != 0) {
#if defined(CONFIG_SOC_BY_HW_FG)
		fg_r_value = get_r_fg_value();

		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &fg_current_temp);
		ret =
		    battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN, &fg_current_state);
		fg_current_temp = fg_current_temp / 10;

		if (fg_current_state == KAL_TRUE) {
			if (vbat_on_charge == 0) {
				vbat_on_charge = bat_temperature_volt;
				current_charge = fg_current_temp;
			}
			bat_temperature_volt_temp = bat_temperature_volt;
			bat_temperature_volt =
			    bat_temperature_volt - ((fg_current_temp * fg_r_value) / 1000);
		} else {
			if (vbat_on_discharge == 0) {
				vbat_on_discharge = bat_temperature_volt;
				current_discharge = fg_current_temp;
			}
			bat_temperature_volt_temp = bat_temperature_volt;
			bat_temperature_volt =
			    bat_temperature_volt + ((fg_current_temp * fg_r_value) / 1000);
		}

		if (vbat_on_charge != 0 && vbat_on_discharge != 0) {
			if (current_charge + current_discharge != 0) {
				resident_R =
				    (1000 * (vbat_on_charge - vbat_on_discharge)) /
				    (current_charge + current_discharge) - R_FG_VALUE;
				bm_print(BM_LOG_FULL, "[auto K for resident R] %d\n", resident_R);
			}
			vbat_on_charge = 0;
			vbat_on_discharge = 0;
		}
#endif

		bat_temperature_val = BattVoltToTemp(bat_temperature_volt);
	}

	bm_print(BM_LOG_FULL, "[force_get_tbat] %d,%d,%d,%d,%d,%d\n",
		 bat_temperature_volt_temp, bat_temperature_volt, fg_current_state, fg_current_temp,
		 fg_r_value, bat_temperature_val);

	return bat_temperature_val;
#endif
}
EXPORT_SYMBOL(force_get_tbat);

int fgauge_get_saddles(void)
{
	if (p_bat_meter_data)
		return p_bat_meter_data->battery_profile_saddles;
	else
		return 0;
}

int fgauge_get_saddles_r_table(void)
{
	if (p_bat_meter_data)
		return p_bat_meter_data->battery_r_profile_saddles;
	else
		return 0;
}

BATTERY_PROFILE_STRUC_P fgauge_get_profile(kal_uint32 temperature)
{
	switch (temperature) {
	case TEMPERATURE_T0:
		return &battery_profile_t0[0];
		break;
	case TEMPERATURE_T1:
		return &battery_profile_t1[0];
		break;
	case TEMPERATURE_T1_5:
		return &battery_profile_t1_5[0];
		break;
	case TEMPERATURE_T2:
		return &battery_profile_t2[0];
		break;
	case TEMPERATURE_T3:
		return &battery_profile_t3[0];
		break;
	case TEMPERATURE_T:
		return &battery_profile_temperature[0];
		break;
	default:
		return NULL;
		break;
	}
}

R_PROFILE_STRUC_P fgauge_get_profile_r_table(kal_uint32 temperature)
{
	switch (temperature) {
	case TEMPERATURE_T0:
		return &r_profile_t0[0];
		break;
	case TEMPERATURE_T1:
		return &r_profile_t1[0];
		break;
	case TEMPERATURE_T1_5:
		return &r_profile_t1_5[0];
		break;
	case TEMPERATURE_T2:
		return &r_profile_t2[0];
		break;
	case TEMPERATURE_T3:
		return &r_profile_t3[0];
		break;
	case TEMPERATURE_T:
		return &r_profile_temperature[0];
		break;
	default:
		return NULL;
		break;
	}
}

kal_int32 fgauge_read_capacity_by_v(kal_int32 voltage)
{
	int i = 0, saddles = 0;
	BATTERY_PROFILE_STRUC_P profile_p;
	kal_int32 ret_percent = 0;

	profile_p = fgauge_get_profile(TEMPERATURE_T);
	if (profile_p == NULL) {
		bm_print(BM_LOG_CRTI, "[FGADC] fgauge get ZCV profile : fail !\r\n");
		return 100;
	}

	saddles = fgauge_get_saddles();

	if (voltage > (profile_p + 0)->voltage)
		return 100;	/* battery capacity, not dod */

	if (voltage < (profile_p + saddles - 1)->voltage)
		return 0;	/* battery capacity, not dod */

	for (i = 0; i < saddles - 1; i++) {
		if ((voltage <= (profile_p + i)->voltage)
		    && (voltage >= (profile_p + i + 1)->voltage)) {
			ret_percent =
			    (profile_p + i)->percentage +
			    (((((profile_p + i)->voltage) -
			       voltage) * (((profile_p + i + 1)->percentage) -
					   ((profile_p + i)->percentage))
			     ) / (((profile_p + i)->voltage) - ((profile_p + i + 1)->voltage))
			    );

			break;
		}

	}
	ret_percent = 100 - ret_percent;

	return ret_percent;
}

kal_int32 fgauge_read_v_by_capacity(int bat_capacity)
{
	int i = 0, saddles = 0;
	BATTERY_PROFILE_STRUC_P profile_p;
	kal_int32 ret_volt = 0;

	profile_p = fgauge_get_profile(TEMPERATURE_T);
	if (profile_p == NULL) {
		bm_print(BM_LOG_CRTI,
			 "[fgauge_read_v_by_capacity] fgauge get ZCV profile : fail !\r\n");
		return 3700;
	}

	saddles = fgauge_get_saddles();

	if (bat_capacity < (profile_p + 0)->percentage)
		return 3700;

	if (bat_capacity > (profile_p + saddles - 1)->percentage)
		return 3700;

	for (i = 0; i < saddles - 1; i++) {
		if ((bat_capacity >= (profile_p + i)->percentage)
		    && (bat_capacity <= (profile_p + i + 1)->percentage)) {
			ret_volt =
			    (profile_p + i)->voltage -
			    (((bat_capacity -
			       ((profile_p + i)->percentage)) * (((profile_p + i)->voltage) -
								 ((profile_p + i + 1)->voltage))
			     ) / (((profile_p + i + 1)->percentage) - ((profile_p + i)->percentage))
			    );

			break;
		}
	}

	return ret_volt;
}

kal_int32 fgauge_read_d_by_v(kal_int32 volt_bat)
{
	int i = 0, saddles = 0;
	BATTERY_PROFILE_STRUC_P profile_p;
	kal_int32 ret_d = 0;

	profile_p = fgauge_get_profile(TEMPERATURE_T);
	if (profile_p == NULL) {
		bm_print(BM_LOG_CRTI, "[FGADC] fgauge get ZCV profile : fail !\r\n");
		return 100;
	}

	saddles = fgauge_get_saddles();

	if (volt_bat > (profile_p + 0)->voltage)
		return 0;

	if (volt_bat < (profile_p + saddles - 1)->voltage)
		return 100;

	for (i = 0; i < saddles - 1; i++) {
		if ((volt_bat <= (profile_p + i)->voltage)
		    && (volt_bat >= (profile_p + i + 1)->voltage)) {
			ret_d =
			    (profile_p + i)->percentage +
			    (((((profile_p + i)->voltage) -
			       volt_bat) * (((profile_p + i + 1)->percentage) -
					    ((profile_p + i)->percentage))
			     ) / (((profile_p + i)->voltage) - ((profile_p + i + 1)->voltage))
			    );

			break;
		}

	}

	return ret_d;
}

kal_int32 fgauge_read_v_by_d(int d_val)
{
	int i = 0, saddles = 0;
	BATTERY_PROFILE_STRUC_P profile_p;
	kal_int32 ret_volt = 0;

	profile_p = fgauge_get_profile(TEMPERATURE_T);
	if (profile_p == NULL) {
		bm_print(BM_LOG_CRTI,
			 "[fgauge_read_v_by_capacity] fgauge get ZCV profile : fail !\r\n");
		return 3700;
	}

	saddles = fgauge_get_saddles();

	if (d_val < (profile_p + 0)->percentage)
		return 3700;

	if (d_val > (profile_p + saddles - 1)->percentage)
		return 3700;

	for (i = 0; i < saddles - 1; i++) {
		if ((d_val >= (profile_p + i)->percentage)
		    && (d_val <= (profile_p + i + 1)->percentage)) {
			ret_volt =
			    (profile_p + i)->voltage -
			    (((d_val -
			       ((profile_p + i)->percentage)) * (((profile_p + i)->voltage) -
								 ((profile_p + i + 1)->voltage))
			     ) / (((profile_p + i + 1)->percentage) - ((profile_p + i)->percentage))
			    );

			break;
		}
	}

	return ret_volt;
}

kal_int32 fgauge_read_r_bat_by_v(kal_int32 voltage)
{
	int i = 0, saddles = 0;
	R_PROFILE_STRUC_P profile_p;
	kal_int32 ret_r = 0;

	profile_p = fgauge_get_profile_r_table(TEMPERATURE_T);
	if (profile_p == NULL) {
		bm_print(BM_LOG_CRTI, "[FGADC] fgauge get R-Table profile : fail !\r\n");
		return (profile_p + 0)->resistance;
	}

	saddles = fgauge_get_saddles_r_table();

	if (voltage > (profile_p + 0)->voltage)
		return (profile_p + 0)->resistance;

	if (voltage < (profile_p + saddles - 1)->voltage)
		return (profile_p + saddles - 1)->resistance;

	for (i = 0; i < saddles - 1; i++) {
		if ((voltage <= (profile_p + i)->voltage)
		    && (voltage >= (profile_p + i + 1)->voltage)) {
			ret_r =
			    (profile_p + i)->resistance +
			    (((((profile_p + i)->voltage) -
			       voltage) * (((profile_p + i + 1)->resistance) -
					   ((profile_p + i)->resistance))
			     ) / (((profile_p + i)->voltage) - ((profile_p + i + 1)->voltage))
			    );
			break;
		}
	}

	return ret_r;
}


void fgauge_construct_battery_profile(kal_int32 temperature, BATTERY_PROFILE_STRUC_P temp_profile_p)
{
	BATTERY_PROFILE_STRUC_P low_profile_p, high_profile_p;
	kal_int32 low_temperature, high_temperature;
	int i, saddles;
	kal_int32 temp_v_1 = 0, temp_v_2 = 0;

	if (temperature <= TEMPERATURE_T1) {
		low_profile_p = fgauge_get_profile(TEMPERATURE_T0);
		high_profile_p = fgauge_get_profile(TEMPERATURE_T1);
		low_temperature = (-10);
		high_temperature = TEMPERATURE_T1;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (battery_profile_t1_5 && temperature <= TEMPERATURE_T1_5) {
		low_profile_p = fgauge_get_profile(TEMPERATURE_T1);
		high_profile_p = fgauge_get_profile(TEMPERATURE_T1_5);
		low_temperature = TEMPERATURE_T1;
		high_temperature = TEMPERATURE_T1_5;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (temperature <= TEMPERATURE_T2) {
		low_profile_p = fgauge_get_profile(TEMPERATURE_T1);
		high_profile_p = fgauge_get_profile(TEMPERATURE_T2);
		low_temperature = TEMPERATURE_T1;
		high_temperature = TEMPERATURE_T2;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else {
		low_profile_p = fgauge_get_profile(TEMPERATURE_T2);
		high_profile_p = fgauge_get_profile(TEMPERATURE_T3);
		low_temperature = TEMPERATURE_T2;
		high_temperature = TEMPERATURE_T3;

		if (temperature > high_temperature)
			temperature = high_temperature;
	}

	saddles = fgauge_get_saddles();

	for (i = 0; i < saddles; i++) {
		if (((high_profile_p + i)->voltage) > ((low_profile_p + i)->voltage)) {
			temp_v_1 = (high_profile_p + i)->voltage;
			temp_v_2 = (low_profile_p + i)->voltage;

			(temp_profile_p + i)->voltage = temp_v_2 +
			    (((temperature - low_temperature) * (temp_v_1 - temp_v_2)
			     ) / (high_temperature - low_temperature)
			    );
		} else {
			temp_v_1 = (low_profile_p + i)->voltage;
			temp_v_2 = (high_profile_p + i)->voltage;

			(temp_profile_p + i)->voltage = temp_v_2 +
			    (((high_temperature - temperature) * (temp_v_1 - temp_v_2)
			     ) / (high_temperature - low_temperature)
			    );
		}

		(temp_profile_p + i)->percentage = (high_profile_p + i)->percentage;
#if 0
		(temp_profile_p + i)->voltage = temp_v_2 +
		    (((temperature - low_temperature) * (temp_v_1 - temp_v_2)
		     ) / (high_temperature - low_temperature)
		    );
#endif
	}


	/* Dumpt new battery profile */
	for (i = 0; i < saddles; i++) {
		bm_print(BM_LOG_CRTI, "<DOD,Voltage> at %d = <%d,%d>\r\n",
			 temperature, (temp_profile_p + i)->percentage,
			 (temp_profile_p + i)->voltage);
	}

}

void fgauge_construct_r_table_profile(kal_int32 temperature, R_PROFILE_STRUC_P temp_profile_p)
{
	R_PROFILE_STRUC_P low_profile_p, high_profile_p;
	kal_int32 low_temperature, high_temperature;
	int i, saddles;
	kal_int32 temp_v_1 = 0, temp_v_2 = 0;
	kal_int32 temp_r_1 = 0, temp_r_2 = 0;

	if (temperature <= TEMPERATURE_T1) {
		low_profile_p = fgauge_get_profile_r_table(TEMPERATURE_T0);
		high_profile_p = fgauge_get_profile_r_table(TEMPERATURE_T1);
		low_temperature = (-10);
		high_temperature = TEMPERATURE_T1;

		if (temperature < low_temperature)
			temperature = low_temperature;
	} else if (r_profile_t1_5 && temperature <= TEMPERATURE_T1_5) {
		low_profile_p = fgauge_get_profile_r_table(TEMPERATURE_T1);
		high_profile_p = fgauge_get_profile_r_table(TEMPERATURE_T1_5);
		low_temperature = TEMPERATURE_T1;
		high_temperature = TEMPERATURE_T1_5;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (temperature <= TEMPERATURE_T2) {
		low_profile_p = fgauge_get_profile_r_table(TEMPERATURE_T1);
		high_profile_p = fgauge_get_profile_r_table(TEMPERATURE_T2);
		low_temperature = TEMPERATURE_T1;
		high_temperature = TEMPERATURE_T2;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else {
		low_profile_p = fgauge_get_profile_r_table(TEMPERATURE_T2);
		high_profile_p = fgauge_get_profile_r_table(TEMPERATURE_T3);
		low_temperature = TEMPERATURE_T2;
		high_temperature = TEMPERATURE_T3;

		if (temperature > high_temperature)
			temperature = high_temperature;
	}

	saddles = fgauge_get_saddles_r_table();

	/* Interpolation for V_BAT */
	for (i = 0; i < saddles; i++) {
		if (((high_profile_p + i)->voltage) > ((low_profile_p + i)->voltage)) {
			temp_v_1 = (high_profile_p + i)->voltage;
			temp_v_2 = (low_profile_p + i)->voltage;

			(temp_profile_p + i)->voltage = temp_v_2 +
			    (((temperature - low_temperature) * (temp_v_1 - temp_v_2)
			     ) / (high_temperature - low_temperature)
			    );
		} else {
			temp_v_1 = (low_profile_p + i)->voltage;
			temp_v_2 = (high_profile_p + i)->voltage;

			(temp_profile_p + i)->voltage = temp_v_2 +
			    (((high_temperature - temperature) * (temp_v_1 - temp_v_2)
			     ) / (high_temperature - low_temperature)
			    );
		}

#if 0
		/* (temp_profile_p + i)->resistance = (high_profile_p + i)->resistance; */

		(temp_profile_p + i)->voltage = temp_v_2 +
		    (((temperature - low_temperature) * (temp_v_1 - temp_v_2)
		     ) / (high_temperature - low_temperature)
		    );
#endif
	}

	/* Interpolation for R_BAT */
	for (i = 0; i < saddles; i++) {
		if (((high_profile_p + i)->resistance) > ((low_profile_p + i)->resistance)) {
			temp_r_1 = (high_profile_p + i)->resistance;
			temp_r_2 = (low_profile_p + i)->resistance;

			(temp_profile_p + i)->resistance = temp_r_2 +
			    (((temperature - low_temperature) * (temp_r_1 - temp_r_2)
			     ) / (high_temperature - low_temperature)
			    );
		} else {
			temp_r_1 = (low_profile_p + i)->resistance;
			temp_r_2 = (high_profile_p + i)->resistance;

			(temp_profile_p + i)->resistance = temp_r_2 +
			    (((high_temperature - temperature) * (temp_r_1 - temp_r_2)
			     ) / (high_temperature - low_temperature)
			    );
		}

#if 0
		/* (temp_profile_p + i)->voltage = (high_profile_p + i)->voltage; */

		(temp_profile_p + i)->resistance = temp_r_2 +
		    (((temperature - low_temperature) * (temp_r_1 - temp_r_2)
		     ) / (high_temperature - low_temperature)
		    );
#endif
	}

	/* Dumpt new r-table profile */
	for (i = 0; i < saddles; i++) {
		bm_print(BM_LOG_CRTI, "<Rbat,VBAT> at %d = <%d,%d>\r\n",
			 temperature, (temp_profile_p + i)->resistance,
			 (temp_profile_p + i)->voltage);
	}

}

#ifdef CONFIG_CUSTOM_BATTERY_CYCLE_AGING_DATA

kal_int32 get_battery_aging_factor(kal_int32 cycle)
{
	kal_int32 i, f1, f2, c1, c2;
	kal_int32 saddles;
	kal_int32 factor;
	if (p_bat_meter_data)
		saddles = p_bat_meter_data->battery_aging_table_saddles;
	else
		return 100;

	for (i = 0; i < saddles; i++) {
		if (battery_aging_table[i].cycle == cycle)
			return battery_aging_table[i].aging_factor;

		if (battery_aging_table[i].cycle > cycle) {
			if (i == 0)
				return 100;

			if (battery_aging_table[i].aging_factor >
			    battery_aging_table[i - 1].aging_factor) {
				f1 = battery_aging_table[i].aging_factor;
				f2 = battery_aging_table[i - 1].aging_factor;
				c1 = battery_aging_table[i].cycle;
				c2 = battery_aging_table[i - 1].cycle;
				factor = (f2 + ((cycle - c2) * (f1 - f2)) / (c1 - c2));
				return factor;
			} else {
				f1 = battery_aging_table[i - 1].aging_factor;
				f2 = battery_aging_table[i].aging_factor;
				c1 = battery_aging_table[i].cycle;
				c2 = battery_aging_table[i - 1].cycle;
				factor = (f2 + ((cycle - c2) * (f1 - f2)) / (c1 - c2));
				return factor;
			}
		}
	}

	return battery_aging_table[saddles - 1].aging_factor;
}

#endif

void update_qmax_by_cycle(void)
{
#ifdef CONFIG_CUSTOM_BATTERY_CYCLE_AGING_DATA
	kal_int32 factor = 0;
	kal_int32 aging_capacity;

	factor = get_battery_aging_factor(gFG_battery_cycle);

	mutex_lock(&qmax_mutex);
	if (factor > 0 && factor < 100) {
		bm_print(BM_LOG_CRTI, "[FG] cycle count to aging factor %d\n", factor);
		aging_capacity = gFG_BATT_CAPACITY * factor / 100;
		if (aging_capacity < gFG_BATT_CAPACITY_aging) {
			bm_print(BM_LOG_CRTI, "[FG] update gFG_BATT_CAPACITY_aging to %d\n",
				 aging_capacity);
			gFG_BATT_CAPACITY_aging = aging_capacity;
		}
	}
	mutex_unlock(&qmax_mutex);
#endif
}

void update_qmax_by_aging_factor(void)
{
	kal_int32 aging_capacity;

	mutex_lock(&qmax_mutex);
	if (gFG_aging_factor < 100 && gFG_aging_factor > 0) {
		aging_capacity = gFG_BATT_CAPACITY * gFG_aging_factor / 100;
		if (aging_capacity < gFG_BATT_CAPACITY_aging) {
			bm_print(BM_LOG_CRTI,
				"[FG] update gFG_BATT_CAPACITY_aging to %d\n",
				aging_capacity);
			gFG_BATT_CAPACITY_aging = aging_capacity;
		}
	}
	mutex_unlock(&qmax_mutex);
}

void update_qmax_by_temp(void)
{
	mutex_lock(&qmax_mutex);
	gFG_BATT_CAPACITY = fgauge_get_Q_max(gFG_temp);
	gFG_BATT_CAPACITY_init_high_current = fgauge_get_Q_max_high_current(gFG_temp);
	gFG_BATT_CAPACITY_aging = gFG_BATT_CAPACITY;
	mutex_unlock(&qmax_mutex);

	update_qmax_by_cycle();
	update_qmax_by_aging_factor();

	bm_print(BM_LOG_CRTI,
		 "[fgauge_update_dod] gFG_BATT_CAPACITY=%d, gFG_BATT_CAPACITY_aging=%d, gFG_BATT_CAPACITY_init_high_current=%d\r\n",
		 gFG_BATT_CAPACITY, gFG_BATT_CAPACITY_aging,
		 gFG_BATT_CAPACITY_init_high_current);
}

void fgauge_construct_table_by_temp(void)
{
#if defined(CONFIG_POWER_EXT)
#else
	kal_uint32 i;
	static kal_int32 init_temp = KAL_TRUE;
	static kal_int32 curr_temp, last_temp, avg_temp;
	static kal_int32 battTempBuffer[TEMP_AVERAGE_SIZE];
	static kal_int32 temperature_sum;
	static kal_uint8 tempIndex;

	gFG_temp = battery_meter_get_battery_temperature();
	curr_temp = gFG_temp;

	/* Temperature window init */
	if (init_temp == KAL_TRUE) {
		for (i = 0; i < TEMP_AVERAGE_SIZE; i++)
			battTempBuffer[i] = curr_temp;

		last_temp = curr_temp;
		temperature_sum = curr_temp * TEMP_AVERAGE_SIZE;
		init_temp = KAL_FALSE;
	}
	/* Temperature sliding window */
	temperature_sum -= battTempBuffer[tempIndex];
	temperature_sum += curr_temp;
	battTempBuffer[tempIndex] = curr_temp;
	avg_temp = (temperature_sum) / TEMP_AVERAGE_SIZE;

	if (avg_temp != last_temp) {
		bm_print(BM_LOG_FULL,
			 "[fgauge_construct_table_by_temp] reconstruct table by temperature change from (%d) to (%d)\r\n",
			 last_temp, avg_temp);
#ifdef CONFIG_IDME
		if (battery_project != 2 && battery_module == 2) { /* handle non-linear R for Aston SDI cell */
			if (curr_temp >= 0 && curr_temp <= 1)
				curr_temp = -1;
		}
#endif
		fgauge_construct_r_table_profile(curr_temp,
						 fgauge_get_profile_r_table(TEMPERATURE_T));
		fgauge_construct_battery_profile(curr_temp, fgauge_get_profile(TEMPERATURE_T));
		last_temp = avg_temp;
		update_qmax_by_temp();
	}

	tempIndex = (tempIndex + 1) % TEMP_AVERAGE_SIZE;

#endif
}

#ifdef CUST_CAPACITY_OCV2CV_TRANSFORM
/*
	ZCV table is created by 600mA loading.
	Here we calculate average current and get a factor based on 600mA.
*/
void fgauge_get_current_factor(void)
{
#if defined(CONFIG_POWER_EXT)
#else
	kal_uint32 i;
	static kal_int32 init_current = KAL_TRUE;
	static kal_int32 inst_current, avg_current;
	static kal_int32 battCurrentBuffer[TEMP_AVERAGE_SIZE];
	static kal_int32 current_sum;
	static kal_uint8 tempcurrentIndex;

	if (KAL_TRUE == gFG_Is_Charging) {
		init_current = KAL_TRUE;
		g_currentfactor = 100;
		bm_print(BM_LOG_CRTI, "[fgauge_get_current_factor] Charging!!\r\n");
		return;
	}

	inst_current = gFG_current;

	if (init_current == KAL_TRUE) {
		for (i = 0; i < TEMP_AVERAGE_SIZE; i++)
			battCurrentBuffer[i] = inst_current;

		current_sum = inst_current * TEMP_AVERAGE_SIZE;
		init_current = KAL_FALSE;
	}

	/* current sliding window */
	current_sum -= battCurrentBuffer[tempcurrentIndex];
	current_sum += inst_current;
	battCurrentBuffer[tempcurrentIndex] = inst_current;
	avg_current = (current_sum)/TEMP_AVERAGE_SIZE;

	g_currentfactor = avg_current * 100 / CV_CURRENT;  /* calculate factor by 600ma */

	bm_print(BM_LOG_CRTI, "[fgauge_get_current_factor] %d,%d,%d,%d\r\n",
		inst_current, avg_current, g_currentfactor, gFG_Is_Charging);

	tempcurrentIndex = (tempcurrentIndex+1)%TEMP_AVERAGE_SIZE;
#endif
}

/*
	ZCV table has battery OCV-to-resistance information.
	Based on a given discharging current value, we can get a new estimated Qmax.
	Qmax is defined as OCV -I*R < power off voltage.
	Default power off voltage is 3400mV.
*/

kal_int32 fgauge_get_Q_max_high_current_by_current(kal_int32 i_current, kal_int16 val_temp)
{
	kal_int32 ret_Q_max = 0;
	kal_int32 iIndex = 0, saddles = 0;
	kal_int32 OCV_temp = 0, Rbat_temp = 0, V_drop = 0;
	R_PROFILE_STRUC_P p_profile_r;
	BATTERY_PROFILE_STRUC_P p_profile_battery;
	kal_int32 threshold = SYSTEM_OFF_VOLTAGE;
#ifdef CONFIG_IDME
	if (battery_project != 2 && battery_module == 2) /* for Aston SDI cell */
		threshold = 3380;
#endif

	/* for Qmax initialization */
	ret_Q_max = fgauge_get_Q_max_high_current(val_temp);

	/* get Rbat and OCV table of the current temperature */
	p_profile_r = fgauge_get_profile_r_table(TEMPERATURE_T);
	p_profile_battery = fgauge_get_profile(TEMPERATURE_T);
	if (p_profile_r == NULL || p_profile_battery == NULL) {
		bm_print(BM_LOG_ERROR, "get R-Table profile/OCV table profile : fail !\r\n");
		return ret_Q_max;
	}

	if (0 == p_profile_r->resistance || 0 == p_profile_battery->voltage) {
		bm_print(BM_LOG_ERROR, "get R-Table profile/OCV table profile : not ready !\r\n");
		return ret_Q_max;
	}

	saddles = fgauge_get_saddles();

	/* get Qmax in current temperature (>3.4) */
	for (iIndex = 0; iIndex < saddles - 1; iIndex++) {
		OCV_temp = (p_profile_battery+iIndex)->voltage;
		Rbat_temp = (p_profile_r+iIndex)->resistance;
		V_drop = (i_current * Rbat_temp)/10000;

		if (OCV_temp - V_drop < threshold) {
			if (iIndex <= 1)
				ret_Q_max = STEP_OF_QMAX;
			else
				ret_Q_max = (iIndex-1) * STEP_OF_QMAX;
			break;
		}
	}

	bm_print(BM_LOG_CRTI, "[fgauge_get_Q_max_by_current] %d,%d,%d,%d,%d\r\n",
			i_current, iIndex, OCV_temp, Rbat_temp, ret_Q_max);

	return ret_Q_max;
}

#endif


void fg_qmax_update_for_aging(void)
{
#if defined(CONFIG_POWER_EXT)
#else
	kal_bool hw_charging_done = bat_is_charging_full();

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	if (g_temp_status != TEMP_POS_10_TO_POS_45) {
		bm_print(BM_LOG_CRTI, "Skip qmax update due to not 4.2V full-charging.\n");
		return;
	}
#endif

	if (hw_charging_done == KAL_TRUE) {	/* charging full, g_HW_Charging_Done == 1 */

		if (gFG_DOD0 > 85) {
			if (gFG_columb < 0)
				gFG_columb = gFG_columb - gFG_columb * 2;	/* absolute value */

			gFG_BATT_CAPACITY_aging =
			    (((gFG_columb * 1000) + (5 * gFG_DOD0)) / gFG_DOD0) / 10;

			/* tuning */
			gFG_BATT_CAPACITY_aging =
			    (gFG_BATT_CAPACITY_aging * 100) / AGING_TUNING_VALUE;

			if (gFG_BATT_CAPACITY_aging == 0) {
				gFG_BATT_CAPACITY_aging =
				    fgauge_get_Q_max(battery_meter_get_battery_temperature());
				bm_print(BM_LOG_CRTI,
					 "[fg_qmax_update_for_aging] error, restore gFG_BATT_CAPACITY_aging (%d)\n",
					 gFG_BATT_CAPACITY_aging);
			}

			bm_print(BM_LOG_CRTI,
				 "[fg_qmax_update_for_aging] need update : gFG_columb=%d, gFG_DOD0=%d, new_qmax=%d\r\n",
				 gFG_columb, gFG_DOD0, gFG_BATT_CAPACITY_aging);
		} else {
			bm_print(BM_LOG_CRTI,
				 "[fg_qmax_update_for_aging] no update : gFG_columb=%d, gFG_DOD0=%d, new_qmax=%d\r\n",
				 gFG_columb, gFG_DOD0, gFG_BATT_CAPACITY_aging);
		}
	} else {
		bm_print(BM_LOG_CRTI, "[fg_qmax_update_for_aging] hw_charging_done=%d\r\n",
			 hw_charging_done);
	}
#endif
}


void dod_init(void)
{
#if defined(CONFIG_SOC_BY_HW_FG)
	int ret = 0;
	/* use get_hw_ocv----------------------------------------------------------------- */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_OCV, &gFG_voltage);
	gFG_capacity_by_v = fgauge_read_capacity_by_v(gFG_voltage);

	g_hw_ocv_debug = gFG_voltage;
	g_hw_soc_debug = gFG_capacity_by_v;
	g_sw_soc_debug = gFG_capacity_by_v_init;

	bm_print(BM_LOG_FULL, "[FGADC] get_hw_ocv=%d, HW_SOC=%d, SW_SOC = %d\n",
		 gFG_voltage, gFG_capacity_by_v, gFG_capacity_by_v_init);

	/* compare with hw_ocv & sw_ocv, check if less than or equal to 5% tolerance */
	if (abs(gFG_capacity_by_v_init - gFG_capacity_by_v) > 5)
		gFG_capacity_by_v = gFG_capacity_by_v_init;

	/* ------------------------------------------------------------------------------- */
#endif

#if defined(CONFIG_POWER_EXT)
	g_rtc_fg_soc = gFG_capacity_by_v;
#else
	g_rtc_fg_soc = get_rtc_spare_fg_value();
	g_rtc_soc_debug = g_rtc_fg_soc;
#endif

	pr_notice("%s: %d, %d, %d, %d\n", __func__, g_hw_ocv_debug, g_hw_soc_debug, g_sw_soc_debug, g_rtc_soc_debug);

#if defined(CONFIG_SOC_BY_HW_FG)
	if (g_rtc_fg_soc > 1 && g_rtc_fg_soc > gFG_capacity_by_v_init) {
		g_rtc_fg_soc -= 1;
		set_rtc_spare_fg_value(g_rtc_fg_soc);
	}
#endif

	if (((g_rtc_fg_soc != 0)
		&& ((abs(g_rtc_fg_soc - gFG_capacity_by_v) < CUST_POWERON_DELTA_CAPACITY_TOLRANCE) ||
		(abs(g_rtc_fg_soc - gFG_capacity_by_v_init) < CUST_POWERON_DELTA_CAPACITY_TOLRANCE))
		&& ((gFG_capacity_by_v > CUST_POWERON_LOW_CAPACITY_TOLRANCE
			|| bat_is_charger_exist() == KAL_TRUE)))
		|| ((g_rtc_fg_soc != 0)
		&& (g_boot_reason == BR_WDT_BY_PASS_PWK || g_boot_reason == BR_WDT
		    || g_boot_mode == RECOVERY_BOOT))) {

		gFG_capacity_by_v = g_rtc_fg_soc;
	}
	bm_print(BM_LOG_FULL, "[FGADC] g_rtc_fg_soc=%d, gFG_capacity_by_v=%d\n",
		 g_rtc_fg_soc, gFG_capacity_by_v);

	if (gFG_capacity_by_v == 0 && bat_is_charger_exist() == KAL_TRUE) {
		gFG_capacity_by_v = 1;

		bm_print(BM_LOG_FULL, "[FGADC] gFG_capacity_by_v=%d\n", gFG_capacity_by_v);
	}
	gFG_capacity = gFG_capacity_by_v;
	gFG_capacity_by_c_init = gFG_capacity;
	gFG_capacity_by_c = gFG_capacity;

	gFG_DOD0 = 100 - gFG_capacity;
	gFG_DOD1 = gFG_DOD0;

	gfg_percent_check_point = gFG_capacity;

#if 1				/* defined(CHANGE_TRACKING_POINT) */
	gFG_15_vlot = fgauge_read_v_by_capacity((100 - g_tracking_point));
	bm_print(BM_LOG_FULL, "[FGADC] gFG_15_vlot = %dmV\n", gFG_15_vlot);
#else
	/* gFG_15_vlot = fgauge_read_v_by_capacity(86); //14% */
	gFG_15_vlot = fgauge_read_v_by_capacity((100 - g_tracking_point));
	bm_print(BM_LOG_FULL, "[FGADC] gFG_15_vlot = %dmV\n", gFG_15_vlot);
	if ((gFG_15_vlot > 3800) || (gFG_15_vlot < 3600)) {
		bm_print(BM_LOG_CRTI, "[FGADC] gFG_15_vlot(%d) over range, reset to 3700\n",
			 gFG_15_vlot);
		gFG_15_vlot = 3700;
	}
#endif
}

/* ============================================================ // SW FG */
kal_int32 mtk_imp_tracking(kal_int32 ori_voltage, kal_int32 ori_current, kal_int32 recursion_time)
{
	kal_int32 ret_compensate_value = 0;
	kal_int32 temp_voltage_1 = ori_voltage;
	kal_int32 temp_voltage_2 = temp_voltage_1;
	int i = 0;

	for (i = 0; i < recursion_time; i++) {
		gFG_resistance_bat = fgauge_read_r_bat_by_v(temp_voltage_2);
		ret_compensate_value = ((ori_current) * (gFG_resistance_bat + R_FG_VALUE)) / 1000;
		ret_compensate_value = (ret_compensate_value + (10 / 2)) / 10;
		temp_voltage_2 = temp_voltage_1 + ret_compensate_value;

		bm_print(BM_LOG_FULL,
			 "[mtk_imp_tracking] temp_voltage_2=%d,temp_voltage_1=%d,ret_compensate_value=%d,gFG_resistance_bat=%d\n",
			 temp_voltage_2, temp_voltage_1, ret_compensate_value, gFG_resistance_bat);
	}

	gFG_resistance_bat = fgauge_read_r_bat_by_v(temp_voltage_2);
	ret_compensate_value =
	    ((ori_current) * (gFG_resistance_bat + R_FG_VALUE + FG_METER_RESISTANCE)) / 1000;
	ret_compensate_value = (ret_compensate_value + (10 / 2)) / 10;

	gFG_compensate_value = ret_compensate_value;

	bm_print(BM_LOG_FULL,
		 "[mtk_imp_tracking] temp_voltage_2=%d,temp_voltage_1=%d,ret_compensate_value=%d,gFG_resistance_bat=%d\n",
		 temp_voltage_2, temp_voltage_1, ret_compensate_value, gFG_resistance_bat);

	return ret_compensate_value;
}

void oam_init(void)
{
	int ret = 0;
	int vol_bat = 0;
	kal_int32 vbat_capacity = 0;

	vol_bat = 5;		/* set avg times */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE, &vol_bat);
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_OCV, &gFG_voltage);

	gFG_capacity_by_v = fgauge_read_capacity_by_v(gFG_voltage);
	vbat_capacity = fgauge_read_capacity_by_v(vol_bat);

	if (bat_is_charger_exist() == KAL_TRUE) {
		bm_print(BM_LOG_CRTI, "[oam_init_inf] gFG_capacity_by_v=%d, vbat_capacity=%d,\n",
			 gFG_capacity_by_v, vbat_capacity);

		/* to avoid plug in cable without battery, then plug in battery to make hw soc = 100% */
		/* if the difference bwtween ZCV and vbat is too large, using vbat instead ZCV */
		if (((gFG_capacity_by_v == 100) && (vbat_capacity < CUST_POWERON_MAX_VBAT_TOLRANCE))
		    || (abs(gFG_capacity_by_v - vbat_capacity) >
			CUST_POWERON_DELTA_VBAT_TOLRANCE)) {
			bm_print(BM_LOG_CRTI,
				 "[oam_init] fg_vbat=(%d), vbat=(%d), set fg_vat as vat\n",
				 gFG_voltage, vol_bat);

			gFG_voltage = vol_bat;
			gFG_capacity_by_v = vbat_capacity;
		}
	}

	gFG_capacity_by_v_init = gFG_capacity_by_v;

	dod_init();

	gFG_BATT_CAPACITY_aging = fgauge_get_Q_max(battery_meter_get_battery_temperature());

	/* oam_v_ocv_1 = gFG_voltage; */
	/* oam_v_ocv_2 = gFG_voltage; */


	oam_v_ocv_init = fgauge_read_v_by_d(gFG_DOD0);
	oam_v_ocv_2 = oam_v_ocv_1 = oam_v_ocv_init;
	g_vol_bat_hw_ocv = gFG_voltage;

	/* vbat = 5; //set avg times */
	/* ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE, &vbat); */
	/* oam_r_1 = fgauge_read_r_bat_by_v(vbat); */
	oam_r_1 = fgauge_read_r_bat_by_v(gFG_voltage);
	oam_r_2 = oam_r_1;

	oam_d0 = gFG_DOD0;
	oam_d_5 = oam_d0;
	oam_i_ori = gFG_current;
	g_d_hw_ocv = oam_d0;

	if (oam_init_i == 0) {
		bm_print(BM_LOG_CRTI,
			 "[oam_init] oam_v_ocv_1,oam_v_ocv_2,oam_r_1,oam_r_2,oam_d0,oam_i_ori\n");
		oam_init_i = 1;
	}

	bm_print(BM_LOG_CRTI, "[oam_init] %d,%d,%d,%d,%d,%d\n",
		 oam_v_ocv_1, oam_v_ocv_2, oam_r_1, oam_r_2, oam_d0, oam_i_ori);

	bm_print(BM_LOG_CRTI, "[oam_init_inf] hw_OCV, hw_D0, RTC, D0, oam_OCV_init, tbat\n");
	bm_print(BM_LOG_CRTI,
		 "[oam_run_inf] oam_OCV1, oam_OCV2, vbat, I1, I2, R1, R2, Car1, Car2,qmax, tbat\n");
	bm_print(BM_LOG_CRTI, "[oam_result_inf] D1, D2, D3, D4, D5, UI_SOC\n");


	bm_print(BM_LOG_CRTI, "[oam_init_inf] %d, %d, %d, %d, %d, %d\n",
		 gFG_voltage, (100 - fgauge_read_capacity_by_v(gFG_voltage)), g_rtc_fg_soc,
		 gFG_DOD0, oam_v_ocv_init, force_get_tbat());

}


void oam_run(void)
{
	int vol_bat = 0;
	/* int vol_bat_hw_ocv=0; */
	/* int d_hw_ocv=0; */
	int charging_current = 0;
	int ret = 0;

	/* Reconstruct table if temp changed; */
	fgauge_construct_table_by_temp();

	vol_bat = 15;		/* set avg times */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE, &vol_bat);

	/* ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_OCV, &vol_bat_hw_ocv); */
	/* d_hw_ocv = fgauge_read_d_by_v(vol_bat_hw_ocv); */

	oam_i_1 = (((oam_v_ocv_1 - vol_bat) * 1000) * 10) / oam_r_1;	/* 0.1mA */
	oam_i_2 = (((oam_v_ocv_2 - vol_bat) * 1000) * 10) / oam_r_2;	/* 0.1mA */

	oam_car_1 = (oam_i_1 * 10 / 3600) + oam_car_1;	/* 0.1mAh */
	oam_car_2 = (oam_i_2 * 10 / 3600) + oam_car_2;	/* 0.1mAh */

	oam_d_1 = oam_d0 + (oam_car_1 * 100 / 10) / gFG_BATT_CAPACITY_aging;
	if (oam_d_1 < 0)
		oam_d_1 = 0;
	if (oam_d_1 > 100)
		oam_d_1 = 100;

	oam_d_2 = oam_d0 + (oam_car_2 * 100 / 10) / gFG_BATT_CAPACITY_aging;
	if (oam_d_2 < 0)
		oam_d_2 = 0;
	if (oam_d_2 > 100)
		oam_d_2 = 100;

	oam_v_ocv_1 = vol_bat + mtk_imp_tracking(vol_bat, oam_i_2, 5);

	oam_d_3 = fgauge_read_d_by_v(oam_v_ocv_1);
	if (oam_d_3 < 0)
		oam_d_3 = 0;
	if (oam_d_3 > 100)
		oam_d_3 = 100;

	oam_r_1 = fgauge_read_r_bat_by_v(oam_v_ocv_1);

	oam_v_ocv_2 = fgauge_read_v_by_d(oam_d_2);
	oam_r_2 = fgauge_read_r_bat_by_v(oam_v_ocv_2);

#if 0
	oam_d_4 = (oam_d_2 + oam_d_3) / 2;
#else
	oam_d_4 = oam_d_3;
#endif

	gFG_columb = oam_car_2 / 10;	/* mAh */

	if ((oam_i_1 < 0) || (oam_i_2 < 0))
		gFG_Is_Charging = KAL_TRUE;
	else
		gFG_Is_Charging = KAL_FALSE;

#if 0
	if (gFG_Is_Charging == KAL_FALSE) {
		d5_count_time = 60;
	} else {
		charging_current = get_charging_setting_current();
		charging_current = charging_current / 100;
		d5_count_time_rate =
		    (((gFG_BATT_CAPACITY_aging * 60 * 60 / 100 / (charging_current - 50)) * 10) +
		     5) / 10;

		if (d5_count_time_rate < 1)
			d5_count_time_rate = 1;

		d5_count_time = d5_count_time_rate;
	}
#else
	d5_count_time = 60;
#endif

	if (d5_count >= d5_count_time) {
		if (gFG_Is_Charging == KAL_FALSE) {
			if (oam_d_3 > oam_d_5) {
				oam_d_5 = oam_d_5 + 1;
			} else {
				if (oam_d_4 > oam_d_5)
					oam_d_5 = oam_d_5 + 1;

			}
		} else {
			if (oam_d_5 > oam_d_3) {
				oam_d_5 = oam_d_5 - 1;
			} else {
				if (oam_d_4 < oam_d_5)
					oam_d_5 = oam_d_5 - 1;

			}
		}
		d5_count = 0;
		oam_d_3_pre = oam_d_3;
		oam_d_4_pre = oam_d_4;
	} else {
		d5_count = d5_count + 10;
	}

	bm_print(BM_LOG_CRTI, "[oam_run] %d,%d,%d,%d,%d,%d,%d,%d\n",
		 d5_count, d5_count_time, oam_d_3_pre, oam_d_3, oam_d_4_pre, oam_d_4, oam_d_5,
		 charging_current);

	if (oam_run_i == 0) {
		bm_print(BM_LOG_FULL,
			 "[oam_run] oam_i_1,oam_i_2,oam_car_1,oam_car_2,oam_d_1,oam_d_2,oam_v_ocv_1,oam_d_3,oam_r_1,oam_v_ocv_2,oam_r_2,vol_bat,g_vol_bat_hw_ocv,g_d_hw_ocv\n");
		oam_run_i = 1;
	}

	bm_print(BM_LOG_FULL, "[oam_run] %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		 oam_i_1, oam_i_2, oam_car_1, oam_car_2, oam_d_1, oam_d_2, oam_v_ocv_1, oam_d_3,
		 oam_r_1, oam_v_ocv_2, oam_r_2, vol_bat, g_vol_bat_hw_ocv, g_d_hw_ocv);

	bm_print(BM_LOG_FULL, "[oam_total] %d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		 gFG_capacity_by_c, gFG_capacity_by_v, gfg_percent_check_point,
		 oam_d_1, oam_d_2, oam_d_3, oam_d_4, oam_d_5, gFG_capacity_by_c_init, g_d_hw_ocv);

	bm_print(BM_LOG_CRTI, "[oam_total_s] %d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", gFG_capacity_by_c,	/* 1 */
		 gFG_capacity_by_v,	/* 2 */
		 gfg_percent_check_point,	/* 3 */
		 (100 - oam_d_1),	/* 4 */
		 (100 - oam_d_2),	/* 5 */
		 (100 - oam_d_3),	/* 6 */
		 (100 - oam_d_4),	/* 9 */
		 (100 - oam_d_5),	/* 10 */
		 gFG_capacity_by_c_init,	/* 7 */
		 (100 - g_d_hw_ocv)	/* 8 */
	    );

	bm_print(BM_LOG_FULL, "[oam_total_s_err] %d,%d,%d,%d,%d,%d,%d\n",
		 (gFG_capacity_by_c - gFG_capacity_by_v),
		 (gFG_capacity_by_c - gfg_percent_check_point),
		 (gFG_capacity_by_c - (100 - oam_d_1)),
		 (gFG_capacity_by_c - (100 - oam_d_2)),
		 (gFG_capacity_by_c - (100 - oam_d_3)),
		 (gFG_capacity_by_c - (100 - oam_d_4)), (gFG_capacity_by_c - (100 - oam_d_5))
	    );

	bm_print(BM_LOG_CRTI, "[oam_init_inf] %d, %d, %d, %d, %d, %d\n",
		 gFG_voltage, (100 - fgauge_read_capacity_by_v(gFG_voltage)), g_rtc_fg_soc,
		 gFG_DOD0, oam_v_ocv_init, force_get_tbat());

	bm_print(BM_LOG_CRTI, "[oam_run_inf] %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
		 oam_v_ocv_1, oam_v_ocv_2, vol_bat, oam_i_1, oam_i_2, oam_r_1, oam_r_2, oam_car_1,
		 oam_car_2, gFG_BATT_CAPACITY_aging, force_get_tbat(), oam_d0);

	bm_print(BM_LOG_CRTI, "[oam_result_inf] %d, %d, %d, %d, %d, %d\n",
		 oam_d_1, oam_d_2, oam_d_3, oam_d_4, oam_d_5, BMT_status.UI_SOC);
}

/* ============================================================ // */



void table_init(void)
{
	BATTERY_PROFILE_STRUC_P profile_p;
	R_PROFILE_STRUC_P profile_p_r_table;

	int temperature = force_get_tbat();

	/* Re-constructure r-table profile according to current temperature */
	profile_p_r_table = fgauge_get_profile_r_table(TEMPERATURE_T);
	if (profile_p_r_table == NULL) {
		bm_print(BM_LOG_CRTI,
			 "[FGADC] fgauge_get_profile_r_table : create table fail !\r\n");
	}
	fgauge_construct_r_table_profile(temperature, profile_p_r_table);

	/* Re-constructure battery profile according to current temperature */
	profile_p = fgauge_get_profile(TEMPERATURE_T);
	if (profile_p == NULL)
		bm_print(BM_LOG_CRTI, "[FGADC] fgauge_get_profile : create table fail !\r\n");

	fgauge_construct_battery_profile(temperature, profile_p);
}

kal_int32 auxadc_algo_run(void)
{
	kal_int32 val = 0;

	gFG_voltage = battery_meter_get_battery_voltage();
	val = fgauge_read_capacity_by_v(gFG_voltage);

	bm_print(BM_LOG_CRTI, "[auxadc_algo_run] %d,%d\n", gFG_voltage, val);

	return val;
}

#if defined(CONFIG_SOC_BY_HW_FG)
void update_fg_dbg_tool_value(void)
{
	g_fg_dbg_bat_volt = gFG_voltage_init;

	if (gFG_Is_Charging == KAL_TRUE)
		g_fg_dbg_bat_current = gFG_current;
	else
		g_fg_dbg_bat_current = -gFG_current;

	g_fg_dbg_bat_zcv = gFG_voltage;

	g_fg_dbg_bat_temp = gFG_temp;

	g_fg_dbg_bat_r = gFG_resistance_bat;

	g_fg_dbg_bat_car = gFG_columb;

	g_fg_dbg_bat_qmax = gFG_BATT_CAPACITY_aging;

	g_fg_dbg_d0 = gFG_DOD0;

	g_fg_dbg_d1 = gFG_DOD1;

	g_fg_dbg_percentage = bat_get_ui_percentage();

	g_fg_dbg_percentage_fg = gFG_capacity_by_c;

	g_fg_dbg_percentage_voltmode = gfg_percent_check_point;
}

kal_int32 fgauge_compensate_battery_voltage(kal_int32 ori_voltage)
{
	kal_int32 ret_compensate_value = 0;

	gFG_ori_voltage = ori_voltage;
	gFG_resistance_bat = fgauge_read_r_bat_by_v(ori_voltage);	/* Ohm */
	ret_compensate_value = (gFG_current * (gFG_resistance_bat + R_FG_VALUE)) / 1000;
	ret_compensate_value = (ret_compensate_value + (10 / 2)) / 10;

	if (gFG_Is_Charging == KAL_TRUE)
		ret_compensate_value = ret_compensate_value - (ret_compensate_value * 2);

	gFG_compensate_value = ret_compensate_value;

	bm_print(BM_LOG_FULL,
		 "[CompensateVoltage] Ori_voltage:%d, compensate_value:%d, gFG_resistance_bat:%d, gFG_current:%d\r\n",
		 ori_voltage, ret_compensate_value, gFG_resistance_bat, gFG_current);

	return ret_compensate_value;
}

kal_int32 fgauge_compensate_battery_voltage_recursion(kal_int32 ori_voltage,
						      kal_int32 recursion_time)
{
	kal_int32 ret_compensate_value = 0;
	kal_int32 temp_voltage_1 = ori_voltage;
	kal_int32 temp_voltage_2 = temp_voltage_1;
	int i = 0;

	for (i = 0; i < recursion_time; i++) {
		gFG_resistance_bat = fgauge_read_r_bat_by_v(temp_voltage_2);	/* Ohm */
		ret_compensate_value = (gFG_current * (gFG_resistance_bat + R_FG_VALUE)) / 1000;
		ret_compensate_value = (ret_compensate_value + (10 / 2)) / 10;

		if (gFG_Is_Charging == KAL_TRUE)
			ret_compensate_value = ret_compensate_value - (ret_compensate_value * 2);

		temp_voltage_2 = temp_voltage_1 + ret_compensate_value;

		bm_print(BM_LOG_FULL,
			 "[fgauge_compensate_battery_voltage_recursion] %d,%d,%d,%d\r\n",
			 temp_voltage_1, temp_voltage_2, gFG_resistance_bat, ret_compensate_value);
	}

	gFG_resistance_bat = fgauge_read_r_bat_by_v(temp_voltage_2);	/* Ohm */
	ret_compensate_value =
	    (gFG_current * (gFG_resistance_bat + R_FG_VALUE + FG_METER_RESISTANCE)) / 1000;
	ret_compensate_value = (ret_compensate_value + (10 / 2)) / 10;

	if (gFG_Is_Charging == KAL_TRUE)
		ret_compensate_value = ret_compensate_value - (ret_compensate_value * 2);

	gFG_compensate_value = ret_compensate_value;

	bm_print(BM_LOG_FULL, "[fgauge_compensate_battery_voltage_recursion] %d,%d,%d,%d\r\n",
		 temp_voltage_1, temp_voltage_2, gFG_resistance_bat, ret_compensate_value);

	return ret_compensate_value;
}


kal_int32 fgauge_get_dod0(kal_int32 voltage, kal_int32 temperature, kal_bool bOcv)
{
	kal_int32 dod0 = 0;
	int i = 0, saddles = 0, jj = 0;
	BATTERY_PROFILE_STRUC_P profile_p;
	R_PROFILE_STRUC_P profile_p_r_table;
	int ret = 0;

/* R-Table (First Time) */
	/* Re-constructure r-table profile according to current temperature */
	profile_p_r_table = fgauge_get_profile_r_table(TEMPERATURE_T);
	if (profile_p_r_table == NULL) {
		bm_print(BM_LOG_CRTI,
			 "[FGADC] fgauge_get_profile_r_table : create table fail !\r\n");
	}
	fgauge_construct_r_table_profile(temperature, profile_p_r_table);

	/* Re-constructure battery profile according to current temperature */
	profile_p = fgauge_get_profile(TEMPERATURE_T);
	if (profile_p == NULL) {
		bm_print(BM_LOG_CRTI, "[FGADC] fgauge_get_profile : create table fail !\r\n");
		return 100;
	}
	fgauge_construct_battery_profile(temperature, profile_p);

	/* Get total saddle points from the battery profile */
	saddles = fgauge_get_saddles();

	/* If the input voltage is not OCV, compensate to ZCV due to battery loading */
	/* Compasate battery voltage from current battery voltage */
	jj = 0;
	if (bOcv == KAL_FALSE) {
		while (gFG_current == 0) {
			ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &gFG_current);
			if (jj > 10)
				break;
			jj++;
		}
		/* voltage = voltage + fgauge_compensate_battery_voltage(voltage); //mV */
		voltage = voltage + fgauge_compensate_battery_voltage_recursion(voltage, 5);	/* mV */
		bm_print(BM_LOG_FULL, "[FGADC] compensate_battery_voltage, voltage=%d\r\n",
			 voltage);
	}
	/* If battery voltage is less then mimimum profile voltage, then return 100 */
	/* If battery voltage is greater then maximum profile voltage, then return 0 */
	if (voltage > (profile_p + 0)->voltage)
		return 0;

	if (voltage < (profile_p + saddles - 1)->voltage)
		return 100;

	/* get DOD0 according to current temperature */
	for (i = 0; i < saddles - 1; i++) {
		if ((voltage <= (profile_p + i)->voltage)
		    && (voltage >= (profile_p + i + 1)->voltage)) {
			dod0 =
			    (profile_p + i)->percentage +
			    (((((profile_p + i)->voltage) -
			       voltage) * (((profile_p + i + 1)->percentage) -
					   ((profile_p + i)->percentage))
			     ) / (((profile_p + i)->voltage) - ((profile_p + i + 1)->voltage))
			    );

			break;
		}
	}

	return dod0;
}


kal_int32 fgauge_update_dod(void)
{
	kal_int32 FG_dod_1 = 0;
	int adjust_coulomb_counter = CAR_TUNE_VALUE;

	if (gFG_DOD0 > 100) {
		gFG_DOD0 = 100;
		bm_print(BM_LOG_FULL, "[fgauge_update_dod] gFG_DOD0 set to 100, gFG_columb=%d\r\n",
			 gFG_columb);
	} else if (gFG_DOD0 < 0) {
		gFG_DOD0 = 0;
		bm_print(BM_LOG_FULL, "[fgauge_update_dod] gFG_DOD0 set to 0, gFG_columb=%d\r\n",
			 gFG_columb);
	} else {
	}

	FG_dod_1 = gFG_DOD0 - ((gFG_columb * 100) / gFG_BATT_CAPACITY_aging);

	bm_print(BM_LOG_FULL,
		 "[fgauge_update_dod] FG_dod_1=%d, adjust_coulomb_counter=%d, gFG_columb=%d, gFG_DOD0=%d, gFG_temp=%d, gFG_BATT_CAPACITY=%d\r\n",
		 FG_dod_1, adjust_coulomb_counter, gFG_columb, gFG_DOD0, gFG_temp,
		 gFG_BATT_CAPACITY);

	if (FG_dod_1 > 100) {
		FG_dod_1 = 100;
		bm_print(BM_LOG_FULL, "[fgauge_update_dod] FG_dod_1 set to 100, gFG_columb=%d\r\n",
			 gFG_columb);
	} else if (FG_dod_1 < 0) {
		FG_dod_1 = 0;
		bm_print(BM_LOG_FULL, "[fgauge_update_dod] FG_dod_1 set to 0, gFG_columb=%d\r\n",
			 gFG_columb);
	} else {
	}

	return FG_dod_1;
}


kal_int32 fgauge_read_capacity(kal_int32 type)
{
	kal_int32 voltage;
	kal_int32 temperature;
	kal_int32 dvalue = 0;

	kal_int32 temp_val = 0;

	if (type == 0) {	/* for initialization */
		/* Use voltage to calculate capacity */
		voltage = battery_meter_get_battery_voltage();	/* in unit of mV */
		temperature = battery_meter_get_battery_temperature();
		dvalue = fgauge_get_dod0(voltage, temperature, KAL_FALSE);	/* need compensate vbat */
	} else {
		/* Use DOD0 and columb counter to calculate capacity */
		dvalue = fgauge_update_dod();	/* DOD1 = DOD0 + (-CAR)/Qmax */
	}

	gFG_DOD1 = dvalue;

#if 0
	/* Battery Aging update ---------------------------------------------------------- */
	dvalue_new = dvalue;
	dvalue =
	    ((dvalue_new * gFG_BATT_CAPACITY_init_high_current * 100) / gFG_BATT_CAPACITY_aging) /
	    100;
	bm_print(BM_LOG_FULL,
		 "[fgauge_read_capacity] dvalue=%d, dvalue_new=%d, gFG_BATT_CAPACITY_init_high_current=%d, gFG_BATT_CAPACITY_aging=%d\r\n",
		 dvalue, dvalue_new, gFG_BATT_CAPACITY_init_high_current, gFG_BATT_CAPACITY_aging);
	/* ---------------------------------------------------------------------------- */
#endif

	temp_val = dvalue;
	dvalue = 100 - temp_val;

	if (dvalue <= 1) {
		dvalue = 1;
		bm_print(BM_LOG_FULL, "[fgauge_read_capacity] dvalue<=1 and set dvalue=1 !!\r\n");
	}

	return dvalue;
}


void fg_voltage_mode(void)
{
#if defined(CONFIG_POWER_EXT)
#else
	if (bat_is_charger_exist() == KAL_TRUE) {
		/* SOC only UP when charging */
		if (gFG_capacity_by_v > gfg_percent_check_point)
			gfg_percent_check_point++;

	} else {
		/* SOC only Done when dis-charging */
		if (gFG_capacity_by_v < gfg_percent_check_point)
			gfg_percent_check_point--;

	}

	bm_print(BM_LOG_FULL,
		 "[FGADC_VoltageMothod] gFG_capacity_by_v=%ld,gfg_percent_check_point=%ld\r\n",
		 gFG_capacity_by_v, gfg_percent_check_point);
#endif
}


void fgauge_algo_run(void)
{
	int i = 0;
	int ret = 0;
#ifdef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT
	int columb_delta = 0;
	int charge_current = 0;
#endif

	/* Reconstruct table if temp changed; */
	fgauge_construct_table_by_temp();

/* 1. Get Raw Data */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &gFG_current);
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN, &gFG_Is_Charging);

	gFG_voltage = battery_meter_get_battery_voltage();
	gFG_voltage_init = gFG_voltage;
	gFG_voltage = gFG_voltage + fgauge_compensate_battery_voltage_recursion(gFG_voltage, 5);	/* mV */
	gFG_voltage = gFG_voltage + OCV_BOARD_COMPESATE;

#ifdef CUST_CAPACITY_OCV2CV_TRANSFORM
	fgauge_get_current_factor();
#endif

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &gFG_columb);

#ifdef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT
	if (gFG_Is_Charging) {
		charge_current -= gFG_current;
		if (charge_current < gFG_min_current)
			gFG_min_current = charge_current;
	} else {
		if (gFG_current > gFG_max_current)
			gFG_max_current = gFG_current;
	}

	columb_delta = gFG_pre_columb_count - gFG_columb;

	if (columb_delta < 0)
		columb_delta = columb_delta - 2 * columb_delta;	/* absolute value */

	gFG_pre_columb_count = gFG_columb;
	gFG_columb_sum += columb_delta;

	/* should we use gFG_BATT_CAPACITY or gFG_BATT_CAPACITY_aging ?? */
	if (gFG_columb_sum >= 2 * gFG_BATT_CAPACITY_aging) {
		gFG_battery_cycle++;
		gFG_columb_sum -= 2 * gFG_BATT_CAPACITY_aging;
		bm_print(BM_LOG_CRTI, "Update battery cycle count to %d. \r\n", gFG_battery_cycle);
	}
	bm_print(BM_LOG_FULL, "@@@ bat cycle count %d, columb sum %d. \r\n", gFG_battery_cycle,
		 gFG_columb_sum);
#endif

/* 1.1 Average FG_voltage */
    /**************** Averaging : START ****************/
	if (gFG_voltage >= gFG_voltage_AVG)
		gFG_vbat_offset = (gFG_voltage - gFG_voltage_AVG);
	else
		gFG_vbat_offset = (gFG_voltage_AVG - gFG_voltage);

	if (gFG_vbat_offset <= MinErrorOffset) {
		FGbatteryVoltageSum -= FGvbatVoltageBuffer[FGbatteryIndex];
		FGbatteryVoltageSum += gFG_voltage;
		FGvbatVoltageBuffer[FGbatteryIndex] = gFG_voltage;

		gFG_voltage_AVG = FGbatteryVoltageSum / FG_VBAT_AVERAGE_SIZE;
		gFG_voltage = gFG_voltage_AVG;

		FGbatteryIndex++;
		if (FGbatteryIndex >= FG_VBAT_AVERAGE_SIZE)
			FGbatteryIndex = 0;

		bm_print(BM_LOG_FULL, "[FG_BUFFER] ");
		for (i = 0; i < FG_VBAT_AVERAGE_SIZE; i++)
			bm_print(BM_LOG_FULL, "%d,", FGvbatVoltageBuffer[i]);

		bm_print(BM_LOG_FULL, "\r\n");
	} else {
		bm_print(BM_LOG_FULL, "[FG] Over MinErrorOffset:V=%d,Avg_V=%d, ", gFG_voltage,
			 gFG_voltage_AVG);

		gFG_voltage = gFG_voltage_AVG;

		bm_print(BM_LOG_FULL, "Avg_V need write back to V : V=%d,Avg_V=%d.\r\n",
			 gFG_voltage, gFG_voltage_AVG);
	}

/* 2. Calculate battery capacity by VBAT */
	gFG_capacity_by_v = fgauge_read_capacity_by_v(gFG_voltage);

/* 3. Calculate battery capacity by Coulomb Counter */
	gFG_capacity_by_c = fgauge_read_capacity(1);

/* 4. voltage mode */
	if (volt_mode_update_timer >= volt_mode_update_time_out) {
		volt_mode_update_timer = 0;

		fg_voltage_mode();
	} else {
		volt_mode_update_timer++;
	}

/* 5. Logging */
	bm_print(BM_LOG_CRTI, "[FGADC] GG init cond. hw_ocv=%d, HW_SOC=%d, SW_SOC=%d, RTC_SOC=%d\n",
		 g_hw_ocv_debug, g_hw_soc_debug, g_sw_soc_debug, g_rtc_soc_debug);

	bm_print(BM_LOG_CRTI,
		 "[FGADC] %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
		 gFG_Is_Charging, gFG_current, gFG_columb, gFG_voltage, gFG_capacity_by_v,
		 gFG_capacity_by_c, gFG_capacity_by_c_init, gFG_BATT_CAPACITY,
		 gFG_BATT_CAPACITY_aging, gFG_compensate_value, gFG_ori_voltage,
		 OCV_BOARD_COMPESATE, R_FG_BOARD_SLOPE, gFG_voltage_init, MinErrorOffset, gFG_DOD0,
		 gFG_DOD1, CAR_TUNE_VALUE, AGING_TUNING_VALUE);
	update_fg_dbg_tool_value();
}

void fgauge_algo_run_init(void)
{
	int i = 0;
	int ret = 0;

/* 1. Get Raw Data */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &gFG_current);
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN, &gFG_Is_Charging);

	gFG_voltage = battery_meter_get_battery_voltage();
	gFG_voltage_init = gFG_voltage;
	gFG_voltage = gFG_voltage + fgauge_compensate_battery_voltage_recursion(gFG_voltage, 5);	/* mV */
	gFG_voltage = gFG_voltage + OCV_BOARD_COMPESATE;

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &gFG_columb);

/* 1.1 Average FG_voltage */
	for (i = 0; i < FG_VBAT_AVERAGE_SIZE; i++)
		FGvbatVoltageBuffer[i] = gFG_voltage;

	FGbatteryVoltageSum = gFG_voltage * FG_VBAT_AVERAGE_SIZE;
	gFG_voltage_AVG = gFG_voltage;

/* 2. Calculate battery capacity by VBAT */
	gFG_capacity_by_v = fgauge_read_capacity_by_v(gFG_voltage);
	gFG_capacity_by_v_init = gFG_capacity_by_v;

/* 3. Calculate battery capacity by Coulomb Counter */
	gFG_capacity_by_c = fgauge_read_capacity(1);

/* 4. update DOD0 */

	dod_init();

#ifdef CONFIG_MTK_HWFG_R_AUTO_DETECT
	gFG_current_auto_detect_R_fg_count = 0;

	for (i = 0; i < 10; i++) {
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &gFG_current);

		gFG_current_auto_detect_R_fg_total += gFG_current;
		gFG_current_auto_detect_R_fg_count++;
	}

	/* double check */
	if (gFG_current_auto_detect_R_fg_total <= 0) {
		bm_print(BM_LOG_CRTI, "gFG_current_auto_detect_R_fg_total=0, need double check\n");

		gFG_current_auto_detect_R_fg_count = 0;

		for (i = 0; i < 10; i++) {
			ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &gFG_current);

			gFG_current_auto_detect_R_fg_total += gFG_current;
			gFG_current_auto_detect_R_fg_count++;
		}
	}

	gFG_current_auto_detect_R_fg_result =
	    gFG_current_auto_detect_R_fg_total / gFG_current_auto_detect_R_fg_count;
	if (gFG_current_auto_detect_R_fg_result <= CURRENT_DETECT_R_FG) {
		g_auxadc_solution = 1;

		bm_print(BM_LOG_FULL,
			 "[FGADC] Detect NO Rfg, use AUXADC report. (%d=%d/%d)(%d)\r\n",
			 gFG_current_auto_detect_R_fg_result, gFG_current_auto_detect_R_fg_total,
			 gFG_current_auto_detect_R_fg_count, g_auxadc_solution);
	} else {
		if (g_auxadc_solution == 0) {
			g_auxadc_solution = 0;

			bm_print(BM_LOG_FULL,
				 "[FGADC] Detect Rfg, use FG report. (%d=%d/%d)(%d)\r\n",
				 gFG_current_auto_detect_R_fg_result,
				 gFG_current_auto_detect_R_fg_total,
				 gFG_current_auto_detect_R_fg_count, g_auxadc_solution);
		} else {
			bm_print(BM_LOG_FULL,
				 "[FGADC] Detect Rfg, but use AUXADC report. due to g_auxadc_solution=%d \r\n",
				 g_auxadc_solution);
		}
	}
#endif

/* 5. Logging */
	bm_print(BM_LOG_FULL,
		 "[FGADC] %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
		 gFG_Is_Charging, gFG_current, gFG_columb, gFG_voltage, gFG_capacity_by_v,
		 gFG_capacity_by_c, gFG_capacity_by_c_init, gFG_BATT_CAPACITY,
		 gFG_BATT_CAPACITY_aging, gFG_compensate_value, gFG_ori_voltage,
		 OCV_BOARD_COMPESATE, R_FG_BOARD_SLOPE, gFG_voltage_init, MinErrorOffset, gFG_DOD0,
		 gFG_DOD1, CAR_TUNE_VALUE, AGING_TUNING_VALUE);
	update_fg_dbg_tool_value();
}

void fgauge_initialization(void)
{
#if defined(CONFIG_POWER_EXT)
#else
	int i = 0;
	kal_uint32 ret = 0;

	/* gFG_BATT_CAPACITY_init_high_current = fgauge_get_Q_max_high_current(25); */
	/* gFG_BATT_CAPACITY_aging = fgauge_get_Q_max(25); */

	/* 1. HW initialization */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_HW_FG_INIT, p_bat_meter_data);

	/* 2. SW algorithm initialization */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_OCV, &gFG_voltage);

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &gFG_current);
	i = 0;
	while (gFG_current == 0) {
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &gFG_current);
		if (i > 10) {
			bm_print(BM_LOG_CRTI, "[fgauge_initialization] gFG_current == 0\n");
			break;
		}
		i++;
	}

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &gFG_columb);
	gFG_temp = battery_meter_get_battery_temperature();
	gFG_capacity = fgauge_read_capacity(0);

	gFG_capacity_by_c_init = gFG_capacity;
	gFG_capacity_by_c = gFG_capacity;
	gFG_capacity_by_v = gFG_capacity;

	gFG_DOD0 = 100 - gFG_capacity;

	gFG_BATT_CAPACITY = fgauge_get_Q_max(gFG_temp);

	gFG_BATT_CAPACITY_init_high_current = fgauge_get_Q_max_high_current(gFG_temp);
	gFG_BATT_CAPACITY_aging = fgauge_get_Q_max(gFG_temp);

	ret = battery_meter_ctrl(BATTERY_METER_CMD_DUMP_REGISTER, NULL);

	bm_print(BM_LOG_CRTI, "[fgauge_initialization] Done\n");
#endif
}
#endif

kal_int32 get_dynamic_period(int first_use, int first_wakeup_time, int battery_capacity_level)
{
#if defined(CONFIG_POWER_EXT)

	return first_wakeup_time;

#elif defined(CONFIG_SOC_BY_AUXADC) ||  defined(CONFIG_SOC_BY_SW_FG) || defined(CONFIG_SOC_BY_HW_FG)

	kal_int32 vbat_val = 0;
	kal_int32 ret_time = 600;

	vbat_val = g_sw_vbat_temp;

	/* change wake up period when system suspend. */
	if (vbat_val > VBAT_NORMAL_WAKEUP)	/* 3.6v */
		ret_time = NORMAL_WAKEUP_PERIOD;	/* 90 min */
	else if (vbat_val > VBAT_LOW_POWER_WAKEUP)	/* 3.5v */
		ret_time = LOW_POWER_WAKEUP_PERIOD;	/* 5 min */
	else
		ret_time = CLOSE_POWEROFF_WAKEUP_PERIOD;	/* 0.5 min */



	bm_print(BM_LOG_CRTI, "vbat_val=%d, ret_time=%d\n", vbat_val, ret_time);

	return ret_time;
#else

	kal_int32 car_instant = 0;
	kal_int32 current_instant = 0;
	static kal_int32 car_sleep;
	kal_int32 car_wakeup = 0;
	static kal_int32 last_time;

	kal_int32 ret_val = -1;
	int check_fglog = 0;
	kal_int32 I_sleep = 0;
	kal_int32 new_time = 0;

	int ret = 0;

	check_fglog = Enable_FGADC_LOG;
	/*
	if (check_fglog == 0)
		Enable_FGADC_LOG=1;
	*/
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &current_instant);

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &car_instant);

	/*
	if (check_fglog == 0)
		Enable_FGADC_LOG=0;
	*/
	if (car_instant < 0)
		car_instant = car_instant - (car_instant * 2);

	if (first_use == 1) {
		/* ret_val = 30*60; */ /* 30 mins */
		ret_val = first_wakeup_time;
		last_time = ret_val;
		car_sleep = car_instant;
	} else {
		car_wakeup = car_instant;

		if (last_time == 0)
			last_time = 1;

		if (car_sleep > car_wakeup) {
			car_sleep = car_wakeup;
			bm_print(BM_LOG_FULL, "[get_dynamic_period] reset car_sleep\n");
		}

		I_sleep = ((car_wakeup - car_sleep) * 3600) / last_time;	/* unit: second */

		if (I_sleep == 0) {
			/*
			if (check_fglog == 0)
				Enable_FGADC_LOG=1;
			*/

			ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &I_sleep);

			I_sleep = I_sleep / 10;
			/*
			if (check_fglog == 0)
				Enable_FGADC_LOG=0;
			*/
		}

		if (I_sleep == 0) {
			new_time = first_wakeup_time;
		} else {
			new_time =
			    ((gFG_BATT_CAPACITY * battery_capacity_level * 3600) / 100) / I_sleep;
		}
		ret_val = new_time;

		if (ret_val == 0)
			ret_val = first_wakeup_time;

		bm_print(BM_LOG_CRTI,
			 "[get_dynamic_period] car_instant=%d, car_wakeup=%d, car_sleep=%d, I_sleep=%d, gFG_BATT_CAPACITY=%d, last_time=%d, new_time=%d\r\n",
			 car_instant, car_wakeup, car_sleep, I_sleep, gFG_BATT_CAPACITY, last_time,
			 new_time);

		/* update parameter */
		car_sleep = car_wakeup;
		last_time = ret_val;
	}
	return ret_val;

#endif
}

/* ============================================================ // */
kal_int32 battery_meter_get_battery_voltage(void)
{
	int ret = 0;
	int val = 5;

	if (battery_meter_ctrl == NULL)
		return 0;

	val = 5;		/* set avg times */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE, &val);

	g_sw_vbat_temp = val;

#ifdef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT
	if (g_sw_vbat_temp > gFG_max_voltage)
		gFG_max_voltage = g_sw_vbat_temp;

	if (g_sw_vbat_temp < gFG_min_voltage)
		gFG_min_voltage = g_sw_vbat_temp;

#endif

	return val;
}

kal_int32 battery_meter_get_battery_voltage_cached(void)
{
	return gFG_voltage_init;
}

kal_int32 battery_meter_get_charging_current(void)
{
#if defined(CONFIG_SWCHR_POWER_PATH)
	return 0;
#else
	kal_int32 ADC_BAT_SENSE_tmp[20] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		};
	kal_int32 ADC_BAT_SENSE_sum = 0;
	kal_int32 ADC_BAT_SENSE = 0;
	kal_int32 ADC_I_SENSE_tmp[20] = {
		 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		 };
	kal_int32 ADC_I_SENSE_sum = 0;
	kal_int32 ADC_I_SENSE = 0;
	int repeat = 20;
	int i = 0;
	int j = 0;
	kal_int32 temp = 0;
	int ICharging = 0;
	int ret = 0;
	int val = 1;

	for (i = 0; i < repeat; i++) {
		val = 1;	/* set avg times */
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE, &val);
		ADC_BAT_SENSE_tmp[i] = val;

		val = 1;	/* set avg times */
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_I_SENSE, &val);
		ADC_I_SENSE_tmp[i] = val;

		ADC_BAT_SENSE_sum += ADC_BAT_SENSE_tmp[i];
		ADC_I_SENSE_sum += ADC_I_SENSE_tmp[i];
	}

	/* sorting    BAT_SENSE */
	for (i = 0; i < repeat; i++) {
		for (j = i; j < repeat; j++) {
			if (ADC_BAT_SENSE_tmp[j] < ADC_BAT_SENSE_tmp[i]) {
				temp = ADC_BAT_SENSE_tmp[j];
				ADC_BAT_SENSE_tmp[j] = ADC_BAT_SENSE_tmp[i];
				ADC_BAT_SENSE_tmp[i] = temp;
			}
		}
	}

	bm_print(BM_LOG_FULL, "[g_Get_I_Charging:BAT_SENSE]\r\n");
	for (i = 0; i < repeat; i++)
		bm_print(BM_LOG_FULL, "%d,", ADC_BAT_SENSE_tmp[i]);

	bm_print(BM_LOG_FULL, "\r\n");

	/* sorting    I_SENSE */
	for (i = 0; i < repeat; i++) {
		for (j = i; j < repeat; j++) {
			if (ADC_I_SENSE_tmp[j] < ADC_I_SENSE_tmp[i]) {
				temp = ADC_I_SENSE_tmp[j];
				ADC_I_SENSE_tmp[j] = ADC_I_SENSE_tmp[i];
				ADC_I_SENSE_tmp[i] = temp;
			}
		}
	}

	bm_print(BM_LOG_FULL, "[g_Get_I_Charging:I_SENSE]\r\n");
	for (i = 0; i < repeat; i++)
		bm_print(BM_LOG_FULL, "%d,", ADC_I_SENSE_tmp[i]);

	bm_print(BM_LOG_FULL, "\r\n");

	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[0];
	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[1];
	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[18];
	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[19];
	ADC_BAT_SENSE = ADC_BAT_SENSE_sum / (repeat - 4);

	bm_print(BM_LOG_FULL, "[g_Get_I_Charging] ADC_BAT_SENSE=%d\r\n", ADC_BAT_SENSE);

	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[0];
	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[1];
	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[18];
	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[19];
	ADC_I_SENSE = ADC_I_SENSE_sum / (repeat - 4);

	bm_print(BM_LOG_FULL, "[g_Get_I_Charging] ADC_I_SENSE(Before)=%d\r\n", ADC_I_SENSE);


	bm_print(BM_LOG_FULL, "[g_Get_I_Charging] ADC_I_SENSE(After)=%d\r\n", ADC_I_SENSE);

	if (ADC_I_SENSE > ADC_BAT_SENSE)
		ICharging = (ADC_I_SENSE - ADC_BAT_SENSE + g_I_SENSE_offset) * 1000 / CUST_R_SENSE;
	else
		ICharging = 0;

	return ICharging;
#endif
}

kal_int32 battery_meter_get_battery_current(void)
{
	int ret = 0;
	kal_int32 val = 0;

	if (g_auxadc_solution == 1)
		val = oam_i_2;
	else
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &val);

	return val;
}

kal_bool battery_meter_get_battery_current_sign(void)
{
	int ret = 0;
	kal_bool val = 0;

	if (g_auxadc_solution == 1)
		val = 0;	/* discharging */
	else
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN, &val);

	return val;
}

kal_int32 battery_meter_get_car(void)
{
	int ret = 0;
	kal_int32 val = 0;

	if (g_auxadc_solution == 1)
		val = oam_car_2;
	else
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &val);

	return val;
}

kal_int32 battery_meter_get_battery_temperature(void)
{
#ifdef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT
	kal_int32 batt_temp = force_get_tbat();

	if (batt_temp > gFG_max_temperature)
		gFG_max_temperature = batt_temp;
	if (batt_temp < gFG_min_temperature)
		gFG_min_temperature = batt_temp;

	return batt_temp;
#else
	return force_get_tbat();
#endif
}

kal_int32 battery_meter_get_charger_voltage(void)
{
	int ret = 0;
	int val = 0;

	if (battery_meter_ctrl == NULL)
		return 0;

	val = 5;		/* set avg times */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_CHARGER, &val);

	/* val = (((R_CHARGER_1+R_CHARGER_2)*100*val)/R_CHARGER_2)/100; */
	return val;
}

kal_int32 battery_meter_get_battery_soc(void)
{
#if defined(CONFIG_SOC_BY_HW_FG)
	return gFG_capacity_by_c;
#endif
}

#ifdef CUST_CAPACITY_OCV2CV_TRANSFORM
/* Here we compensate D1 by a factor from Qmax with loading. */
kal_int32 battery_meter_trans_battery_percentage(kal_int32 d_val)
{
	kal_int32 d_val_before = 0;
	kal_int32 temp_val = 0;
	kal_int32 C_0mA = 0;
	kal_int32 C_600mA = 0;
	kal_int32 C_current = 0;
	kal_int32 i_avg_current = 0;

	d_val_before = d_val;
	temp_val = battery_meter_get_battery_temperature();
	C_0mA = fgauge_get_Q_max(temp_val);

	/* discharging and current > 600ma */
	i_avg_current = g_currentfactor * CV_CURRENT/100;
	if (KAL_FALSE == gFG_Is_Charging && g_currentfactor > 100) {
		C_600mA = fgauge_get_Q_max_high_current(temp_val);
		C_current = fgauge_get_Q_max_high_current_by_current(i_avg_current, temp_val);
#ifdef CONFIG_IDME
		if (battery_project != 2 && battery_module == 2 && i_avg_current > 8000) /* only compensate up to 0.8A for Aston SDI cell */
			C_current = fgauge_get_Q_max_high_current_by_current(8000, temp_val);
#endif
		if (C_current < C_600mA)
			C_600mA = C_current;
	} else
		C_600mA = fgauge_get_Q_max_high_current(temp_val);

	if (C_0mA > C_600mA)
		d_val = d_val + (((C_0mA-C_600mA) * (d_val)) / C_600mA);

	if (d_val > 100)
		d_val = 100;

	bm_print(BM_LOG_CRTI, "[battery_meter_trans_battery_percentage] %d,%d,%d,%d,%d,%d\r\n",
			temp_val, C_0mA, C_600mA, d_val_before, d_val, g_currentfactor);

	return d_val;
}
#endif

kal_int32 battery_meter_get_battery_percentage(void)
{
#if defined(CONFIG_POWER_EXT)
	return 50;
#else

#if defined(CONFIG_SOC_BY_EXT_HW_FG) && defined(CONFIG_EXT_FG_HAS_SOC)
	kal_int32 arg = EXT_FG_CMD_SOC;
#endif

	if (bat_is_charger_exist() == KAL_FALSE)
		fg_qmax_update_for_aging_flag = 1;

#if defined(CONFIG_SOC_BY_AUXADC)
	return auxadc_algo_run();
#endif

#if defined(CONFIG_SOC_BY_HW_FG)
	if (g_auxadc_solution == 1) {
		return auxadc_algo_run();
	} else {
		fgauge_algo_run();

#ifdef CUST_CAPACITY_OCV2CV_TRANSFORM
		/* We keep gFG_capacity_by_c as capacity before compensation */
		/* Compensated capacity is returned for UI SOC tracking */
		return 100 - battery_meter_trans_battery_percentage(100-gFG_capacity_by_c);
#else
		return gFG_capacity_by_c;
#endif
	}
#endif

#if defined(CONFIG_SOC_BY_SW_FG)
	oam_run();
	if (OAM_D5 == 1)
		return 100 - oam_d_5;
	else
		return 100 - oam_d_2;
#endif

#if defined(CONFIG_SOC_BY_EXT_HW_FG) && defined(CONFIG_EXT_FG_HAS_SOC)
	if (battery_meter_ctrl(BATTERY_METER_CMD_EXT_HW_FG, &arg) == 0)
		return arg;
	else
		return 50;
#else
	return 50;
#endif

#endif
}


kal_int32 battery_meter_initial(void)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else

#if defined(CONFIG_SOC_BY_AUXADC)
	g_auxadc_solution = 1;
	table_init();
	bm_print(BM_LOG_CRTI, "[battery_meter_initial] CONFIG_SOC_BY_AUXADC done\n");
#endif

#if defined(CONFIG_SOC_BY_HW_FG)
	fgauge_initialization();
	fgauge_algo_run_init();
	bm_print(BM_LOG_CRTI, "[battery_meter_initial] CONFIG_SOC_BY_HW_FG done\n");
#endif

#if defined(CONFIG_SOC_BY_SW_FG)
	g_auxadc_solution = 1;
	table_init();
	oam_init();
	bm_print(BM_LOG_CRTI, "[battery_meter_initial] CONFIG_SOC_BY_SW_FG done\n");
#endif

#if defined(CONFIG_SOC_BY_EXT_HW_FG)
	/* 1. HW initialization */
	battery_meter_ctrl(BATTERY_METER_CMD_HW_FG_INIT, p_bat_meter_data);

	bm_print(BM_LOG_CRTI, "[battery_meter_initial] SOC_BY_EXT_HW_FG done\n");
#endif

	return 0;
#endif
}

void reset_parameter_car(void)
{
#if defined(CONFIG_SOC_BY_HW_FG)
	int ret = 0;
	ret = battery_meter_ctrl(BATTERY_METER_CMD_HW_RESET, NULL);
	gFG_columb = 0;

#ifdef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT
	gFG_pre_columb_count = 0;
#endif

#ifdef CONFIG_MTK_ENABLE_AGING_ALGORITHM
	aging_ocv_1 = 0;
	aging_ocv_2 = 0;
#endif

#endif

#if defined(CONFIG_SOC_BY_SW_FG)
	oam_car_1 = 0;
	oam_car_2 = 0;
	gFG_columb = 0;
#endif
}

void reset_parameter_dod_change(void)
{
#if defined(CONFIG_SOC_BY_HW_FG)
	bm_print(BM_LOG_FULL, "[FGADC] Update DOD0(%d) by %d \r\n", gFG_DOD0, gFG_DOD1);
	gFG_DOD0 = gFG_DOD1;
#endif

#if defined(CONFIG_SOC_BY_SW_FG)
	bm_print(BM_LOG_FULL, "[FGADC] Update oam_d0(%d) by %d \r\n", oam_d0, oam_d_5);
	oam_d0 = oam_d_5;
	gFG_DOD0 = oam_d0;
	oam_d_1 = oam_d_5;
	oam_d_2 = oam_d_5;
	oam_d_3 = oam_d_5;
	oam_d_4 = oam_d_5;
#endif
}

void reset_parameter_dod_full(kal_uint32 ui_percentage)
{
#if defined(CONFIG_SOC_BY_HW_FG)
	bm_print(BM_LOG_CRTI, "[battery_meter_reset]1 DOD0=%d,DOD1=%d,ui=%d\n", gFG_DOD0, gFG_DOD1,
		 ui_percentage);
	gFG_DOD0 = 100 - ui_percentage;
	gFG_DOD1 = gFG_DOD0;
	bm_print(BM_LOG_CRTI, "[battery_meter_reset]2 DOD0=%d,DOD1=%d,ui=%d\n", gFG_DOD0, gFG_DOD1,
		 ui_percentage);
#endif

#if defined(CONFIG_SOC_BY_SW_FG)
	bm_print(BM_LOG_CRTI, "[battery_meter_reset]1 oam_d0=%d,oam_d_5=%d,ui=%d\n", oam_d0,
		 oam_d_5, ui_percentage);
	oam_d0 = 100 - ui_percentage;
	gFG_DOD0 = oam_d0;
	gFG_DOD1 = oam_d0;
	oam_d_1 = oam_d0;
	oam_d_2 = oam_d0;
	oam_d_3 = oam_d0;
	oam_d_4 = oam_d0;
	oam_d_5 = oam_d0;
	bm_print(BM_LOG_CRTI, "[battery_meter_reset]2 oam_d0=%d,oam_d_5=%d,ui=%d\n", oam_d0,
		 oam_d_5, ui_percentage);
#endif
}

kal_int32 battery_meter_reset(kal_bool bUI_SOC)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	kal_uint32 ui_percentage = bat_get_ui_percentage();

#ifdef CUST_CAPACITY_OCV2CV_TRANSFORM
	if (KAL_FALSE == bUI_SOC) {
		ui_percentage = battery_meter_get_battery_soc();
		bm_print(BM_LOG_FULL, "[CUST_CAPACITY_OCV2CV_TRANSFORM]Use Battery SOC: %d\n", ui_percentage);
	}
#endif

#if defined(CONFIG_QMAX_UPDATE_BY_CHARGED_CAPACITY)
	if (bat_is_charging_full() == KAL_TRUE)	{ /* charge full */
		if (fg_qmax_update_for_aging_flag == 1) {
			fg_qmax_update_for_aging();
			fg_qmax_update_for_aging_flag = 0;
		}
	}
#endif

	reset_parameter_car();
	reset_parameter_dod_full(ui_percentage);

	return 0;
#endif
}

kal_int32 battery_meter_sync(kal_int32 bat_i_sense_offset)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	g_I_SENSE_offset = bat_i_sense_offset;
	return 0;
#endif
}

kal_int32 battery_meter_get_battery_zcv(void)
{
#if defined(CONFIG_POWER_EXT)
	return 3987;
#else
	return gFG_voltage;
#endif
}

kal_int32 battery_meter_get_battery_nPercent_zcv(void)
{
#if defined(CONFIG_POWER_EXT)
	return 3700;
#else
	return gFG_15_vlot;	/* 15% zcv,  15% can be customized by 100-g_tracking_point */
#endif
}

kal_int32 battery_meter_get_battery_nPercent_UI_SOC(void)
{
#if defined(CONFIG_POWER_EXT)
	return 15;
#else
	return g_tracking_point;	/* tracking point */
#endif
}

kal_int32 battery_meter_get_tempR(kal_int32 dwVolt)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	int TRes;

	TRes = (RBAT_PULL_UP_R * dwVolt) / (RBAT_PULL_UP_VOLT - dwVolt);

	return TRes;
#endif
}

kal_int32 battery_meter_get_tempV(void)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	int ret = 0;
	int val = 0;

	val = 1;		/* set avg times */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_TEMP, &val);
	return val;
#endif
}

kal_int32 battery_meter_get_VSense(void)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	int ret = 0;
	int val = 0;

	if (battery_meter_ctrl == NULL)
		return 0;

	val = 1;		/* set avg times */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_I_SENSE, &val);
	return val;
#endif
}

/* ============================================================ // */
static ssize_t fgadc_log_write(struct file *filp, const char __user *buff,
			       size_t len, loff_t *data)
{
	if (copy_from_user(&proc_fgadc_data, buff, len)) {
		bm_print(BM_LOG_CRTI, "fgadc_log_write error.\n");
		return -EFAULT;
	}

	if (proc_fgadc_data[0] == '1') {
		bm_print(BM_LOG_CRTI, "enable FGADC driver log system\n");
		Enable_FGADC_LOG = 1;
	} else if (proc_fgadc_data[0] == '2') {
		bm_print(BM_LOG_CRTI, "enable FGADC driver log system:2\n");
		Enable_FGADC_LOG = 2;
	} else {
		bm_print(BM_LOG_CRTI, "Disable FGADC driver log system\n");
		Enable_FGADC_LOG = 0;
	}

	return len;
}

static const struct file_operations fgadc_proc_fops = {
	.write = fgadc_log_write,
};

int init_proc_log_fg(void)
{
	int ret = 0;

#if 1
	proc_create("fgadc_log", 0644, NULL, &fgadc_proc_fops);
	bm_print(BM_LOG_CRTI, "proc_create fgadc_proc_fops\n");
#else
	proc_entry_fgadc = create_proc_entry("fgadc_log", 0644, NULL);

	if (proc_entry_fgadc == NULL) {
		ret = -ENOMEM;
		bm_print(BM_LOG_CRTI, "init_proc_log_fg: Couldn't create proc entry\n");
	} else {
		proc_entry_fgadc->write_proc = fgadc_log_write;
		bm_print(BM_LOG_CRTI, "init_proc_log_fg loaded.\n");
	}
#endif

	return ret;
}

#ifdef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT

/* ============================================================ // */

static ssize_t show_FG_Battery_Cycle(struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] gFG_battery_cycle  : %d\n", gFG_battery_cycle);
	return sprintf(buf, "%d\n", gFG_battery_cycle);
}

static ssize_t store_FG_Battery_Cycle(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t size)
{
	int cycle;
	if (1 == sscanf(buf, "%d", &cycle)) {
		bm_print(BM_LOG_CRTI, "[FG] update battery cycle count: %d\n", cycle);
		gFG_battery_cycle = cycle;

		update_qmax_by_cycle();
	} else {
		bm_print(BM_LOG_CRTI, "[FG] format error!\n");
	}
	return size;
}

static DEVICE_ATTR(FG_Battery_Cycle, 0664, show_FG_Battery_Cycle, store_FG_Battery_Cycle);

/* ------------------------------------------------------------------------------------------- */

static ssize_t show_FG_Max_Battery_Voltage(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] gFG_max_voltage  : %d\n", gFG_max_voltage);
	return sprintf(buf, "%d\n", gFG_max_voltage);
}

static ssize_t store_FG_Max_Battery_Voltage(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	int voltage;
	if (1 == sscanf(buf, "%d", &voltage)) {
		if (voltage > gFG_max_voltage) {
			bm_print(BM_LOG_CRTI, "[FG] update battery max voltage: %d\n", voltage);
			gFG_max_voltage = voltage;
		}
	} else {
		bm_print(BM_LOG_CRTI, "[FG] format error!\n");
	}
	return size;
}

static DEVICE_ATTR(FG_Max_Battery_Voltage, 0664, show_FG_Max_Battery_Voltage,
		   store_FG_Max_Battery_Voltage);

/* ------------------------------------------------------------------------------------------- */

static ssize_t show_FG_Min_Battery_Voltage(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] gFG_min_voltage  : %d\n", gFG_min_voltage);
	return sprintf(buf, "%d\n", gFG_min_voltage);
}

static ssize_t store_FG_Min_Battery_Voltage(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	int voltage;
	if (1 == sscanf(buf, "%d", &voltage)) {
		if (voltage < gFG_min_voltage) {
			bm_print(BM_LOG_CRTI, "[FG] update battery min voltage: %d\n", voltage);
			gFG_min_voltage = voltage;
		}
	} else {
		bm_print(BM_LOG_CRTI, "[FG] format error!\n");
	}
	return size;
}

static DEVICE_ATTR(FG_Min_Battery_Voltage, 0664, show_FG_Min_Battery_Voltage,
		   store_FG_Min_Battery_Voltage);

/* ------------------------------------------------------------------------------------------- */

static ssize_t show_FG_Max_Battery_Current(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] gFG_max_current  : %d\n", gFG_max_current);
	return sprintf(buf, "%d\n", gFG_max_current);
}

static ssize_t store_FG_Max_Battery_Current(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	int bat_current;
	if (1 == sscanf(buf, "%d", &bat_current)) {
		if (bat_current > gFG_max_current) {
			bm_print(BM_LOG_CRTI, "[FG] update battery max current: %d\n", bat_current);
			gFG_max_current = bat_current;
		}
	} else {
		bm_print(BM_LOG_CRTI, "[FG] format error!\n");
	}
	return size;
}

static DEVICE_ATTR(FG_Max_Battery_Current, 0664, show_FG_Max_Battery_Current,
		   store_FG_Max_Battery_Current);

/* ------------------------------------------------------------------------------------------- */

static ssize_t show_FG_Min_Battery_Current(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] gFG_min_current  : %d\n", gFG_min_current);
	return sprintf(buf, "%d\n", gFG_min_current);
}

static ssize_t store_FG_Min_Battery_Current(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	int bat_current;
	if (1 == sscanf(buf, "%d", &bat_current)) {
		if (bat_current < gFG_min_current) {
			bm_print(BM_LOG_CRTI, "[FG] update battery min current: %d\n", bat_current);
			gFG_min_current = bat_current;
		}
	} else {
		bm_print(BM_LOG_CRTI, "[FG] format error!\n");
	}
	return size;
}

static DEVICE_ATTR(FG_Min_Battery_Current, 0664, show_FG_Min_Battery_Current,
		   store_FG_Min_Battery_Current);

/* ------------------------------------------------------------------------------------------- */

static ssize_t show_FG_Max_Battery_Temperature(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] gFG_max_temperature  : %d\n", gFG_max_temperature);
	return sprintf(buf, "%d\n", gFG_max_temperature);
}

static ssize_t store_FG_Max_Battery_Temperature(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	int temp;
	if (1 == sscanf(buf, "%d", &temp)) {
		if (temp > gFG_max_temperature) {
			bm_print(BM_LOG_CRTI, "[FG] update battery max temp: %d\n", temp);
			gFG_max_temperature = temp;
		}
	} else {
		bm_print(BM_LOG_CRTI, "[FG] format error!\n");
	}
	return size;
}

static DEVICE_ATTR(FG_Max_Battery_Temperature, 0664, show_FG_Max_Battery_Temperature,
		   store_FG_Max_Battery_Temperature);

/* ------------------------------------------------------------------------------------------- */

static ssize_t show_FG_Min_Battery_Temperature(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] gFG_min_temperature  : %d\n", gFG_min_temperature);
	return sprintf(buf, "%d\n", gFG_min_temperature);
}

static ssize_t store_FG_Min_Battery_Temperature(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	int temp;
	if (1 == sscanf(buf, "%d", &temp)) {
		if (temp < gFG_min_temperature) {
			bm_print(BM_LOG_CRTI, "[FG] update battery min temp: %d\n", temp);
			gFG_min_temperature = temp;
		}
	} else {
		bm_print(BM_LOG_CRTI, "[FG] format error!\n");
	}
	return size;
}

static DEVICE_ATTR(FG_Min_Battery_Temperature, 0664, show_FG_Min_Battery_Temperature,
		   store_FG_Min_Battery_Temperature);

/* ------------------------------------------------------------------------------------------- */

static ssize_t show_FG_Aging_Factor(struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] gFG_aging_factor  : %d\n", gFG_aging_factor);
	return sprintf(buf, "%d\n", gFG_aging_factor);
}

static ssize_t store_FG_Aging_Factor(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t size)
{
	int factor;
	if (1 == sscanf(buf, "%d", &factor)) {
		if (factor < 100 && factor > 0) {
			pr_info("[FG] update battery aging factor: old(%d), new(%d)\n",
				 gFG_aging_factor, factor);

			gFG_aging_factor = factor;
			update_qmax_by_aging_factor();
		} else {
			pr_warn("[FG] try to set aging factor (%d) out of range!\n", factor);
		}
	} else {
		bm_print(BM_LOG_CRTI, "[FG] format error!\n");
	}

	return size;
}

static DEVICE_ATTR(FG_Aging_Factor, 0664, show_FG_Aging_Factor, store_FG_Aging_Factor);

/* ------------------------------------------------------------------------------------------- */

#endif

/* ============================================================ // */
static ssize_t show_FG_R_Offset(struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] gFG_R_Offset : %d\n", g_R_FG_offset);
	return sprintf(buf, "%d\n", g_R_FG_offset);
}

static ssize_t store_FG_R_Offset(struct device *dev, struct device_attribute *attr, const char *buf,
				 size_t size)
{
	int offset;
	if (1 == sscanf(buf, "%d", &offset)) {
		bm_print(BM_LOG_CRTI, "[FG] update g_R_FG_offset to %d\n", offset);
		g_R_FG_offset = offset;
	} else {
		bm_print(BM_LOG_CRTI, "[FG] format error!\n");
	}

	return size;
}

static DEVICE_ATTR(FG_R_Offset, 0664, show_FG_R_Offset, store_FG_R_Offset);

/* ============================================================ // */
static ssize_t show_FG_Current(struct device *dev, struct device_attribute *attr, char *buf)
{
	kal_int32 ret = 0;
	kal_int32 fg_current_inout_battery = 0;
	kal_int32 val = 0;
	kal_bool is_charging = 0;

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &val);
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN, &is_charging);

	if (is_charging == KAL_TRUE)
		fg_current_inout_battery = val;
	else
		fg_current_inout_battery = -val;

	bm_print(BM_LOG_FULL, "[FG] gFG_current_inout_battery : %d\n", fg_current_inout_battery);
	return sprintf(buf, "%d\n", fg_current_inout_battery);
}

static ssize_t store_FG_Current(struct device *dev, struct device_attribute *attr, const char *buf,
				size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_Current, 0664, show_FG_Current, store_FG_Current);

/* ============================================================ // */
static ssize_t show_FG_g_fg_dbg_bat_volt(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_bat_volt : %d\n", g_fg_dbg_bat_volt);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_volt);
}

static ssize_t store_FG_g_fg_dbg_bat_volt(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_volt, 0664, show_FG_g_fg_dbg_bat_volt,
		   store_FG_g_fg_dbg_bat_volt);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_bat_current(struct device *dev, struct device_attribute *attr,
					    char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_bat_current : %d\n", g_fg_dbg_bat_current);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_current);
}

static ssize_t store_FG_g_fg_dbg_bat_current(struct device *dev, struct device_attribute *attr,
					     const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_current, 0664, show_FG_g_fg_dbg_bat_current,
		   store_FG_g_fg_dbg_bat_current);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_bat_zcv(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_bat_zcv : %d\n", g_fg_dbg_bat_zcv);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_zcv);
}

static ssize_t store_FG_g_fg_dbg_bat_zcv(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_zcv, 0664, show_FG_g_fg_dbg_bat_zcv, store_FG_g_fg_dbg_bat_zcv);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_bat_temp(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_bat_temp : %d\n", g_fg_dbg_bat_temp);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_temp);
}

static ssize_t store_FG_g_fg_dbg_bat_temp(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_temp, 0664, show_FG_g_fg_dbg_bat_temp,
		   store_FG_g_fg_dbg_bat_temp);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_bat_r(struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_bat_r : %d\n", g_fg_dbg_bat_r);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_r);
}

static ssize_t store_FG_g_fg_dbg_bat_r(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_r, 0664, show_FG_g_fg_dbg_bat_r, store_FG_g_fg_dbg_bat_r);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_bat_car(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_bat_car : %d\n", g_fg_dbg_bat_car);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_car);
}

static ssize_t store_FG_g_fg_dbg_bat_car(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_car, 0664, show_FG_g_fg_dbg_bat_car, store_FG_g_fg_dbg_bat_car);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_bat_qmax(struct device *dev, struct device_attribute *attr,
					 char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_bat_qmax : %d\n", g_fg_dbg_bat_qmax);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_qmax);
}

static ssize_t store_FG_g_fg_dbg_bat_qmax(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_qmax, 0664, show_FG_g_fg_dbg_bat_qmax,
		   store_FG_g_fg_dbg_bat_qmax);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_d0(struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_d0 : %d\n", g_fg_dbg_d0);
	return sprintf(buf, "%d\n", g_fg_dbg_d0);
}

static ssize_t store_FG_g_fg_dbg_d0(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_d0, 0664, show_FG_g_fg_dbg_d0, store_FG_g_fg_dbg_d0);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_d1(struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_d1 : %d\n", g_fg_dbg_d1);
	return sprintf(buf, "%d\n", g_fg_dbg_d1);
}

static ssize_t store_FG_g_fg_dbg_d1(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_d1, 0664, show_FG_g_fg_dbg_d1, store_FG_g_fg_dbg_d1);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_percentage(struct device *dev, struct device_attribute *attr,
					   char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_percentage : %d\n", g_fg_dbg_percentage);
	return sprintf(buf, "%d\n", g_fg_dbg_percentage);
}

static ssize_t store_FG_g_fg_dbg_percentage(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_percentage, 0664, show_FG_g_fg_dbg_percentage,
		   store_FG_g_fg_dbg_percentage);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_percentage_fg(struct device *dev, struct device_attribute *attr,
					      char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_percentage_fg : %d\n", g_fg_dbg_percentage_fg);
	return sprintf(buf, "%d\n", g_fg_dbg_percentage_fg);
}

static ssize_t store_FG_g_fg_dbg_percentage_fg(struct device *dev, struct device_attribute *attr,
					       const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_percentage_fg, 0664, show_FG_g_fg_dbg_percentage_fg,
		   store_FG_g_fg_dbg_percentage_fg);
/* ------------------------------------------------------------------------------------------- */
static ssize_t show_FG_g_fg_dbg_percentage_voltmode(struct device *dev,
						    struct device_attribute *attr, char *buf)
{
	bm_print(BM_LOG_FULL, "[FG] g_fg_dbg_percentage_voltmode : %d\n",
		 g_fg_dbg_percentage_voltmode);
	return sprintf(buf, "%d\n", g_fg_dbg_percentage_voltmode);
}

static ssize_t store_FG_g_fg_dbg_percentage_voltmode(struct device *dev,
						     struct device_attribute *attr, const char *buf,
						     size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_percentage_voltmode, 0664, show_FG_g_fg_dbg_percentage_voltmode,
		   store_FG_g_fg_dbg_percentage_voltmode);

/* ============================================================ // */

static ssize_t show_car_tune_value(struct device *dev, struct device_attribute *attr, char *buf)
{
	bm_print(BM_LOG_FULL, "CAR_TUNE_VALUE: %d\n", CAR_TUNE_VALUE);
	return sprintf(buf, "%d\n", CAR_TUNE_VALUE);
}

static ssize_t store_car_tune_value(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int car_tune_value;
	if(1 == sscanf(buf, "%d", &car_tune_value)) {
		CAR_TUNE_VALUE = car_tune_value;
	} else {
		bm_print(BM_LOG_CRTI, "car_tune_value format error!\n");
	}
	return size;
}

static DEVICE_ATTR(car_tune_value, S_IRUSR | S_IWUSR, show_car_tune_value, store_car_tune_value);

static ssize_t show_charging_current_limit(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u mA\n", bat_charger_get_charging_current());
}

extern kal_uint32 set_bat_charging_current_limit(int limit);

static ssize_t store_charging_current_limit(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	int charging_current_limit;
	if (1 == sscanf(buf, "%d", &charging_current_limit))
		set_bat_charging_current_limit(charging_current_limit);
	else
		bm_print(BM_LOG_CRTI, "charging_current_limit format error!\n");
	return size;
}

static DEVICE_ATTR(charging_current_limit, S_IRUSR | S_IWUSR,
		show_charging_current_limit, store_charging_current_limit);

static void init_meter_global_data(struct platform_device *dev)
{
	mt_battery_meter_custom_data *data = dev->dev.platform_data;
	g_R_BAT_SENSE = data->r_bat_sense;
	g_R_I_SENSE = R_I_SENSE;
	g_R_CHARGER_1 = R_CHARGER_1;
	g_R_CHARGER_2 = R_CHARGER_2;
	g_R_FG_offset = CUST_R_FG_OFFSET;
	g_tracking_point = CUST_TRACKING_POINT;

#ifdef CONFIG_IDME
	if (battery_project == -1)
		battery_project = idme_get_battery_info(31, 2);
	if (battery_module == -1)
		battery_module = idme_get_battery_info(28, 1);
#endif
}

static int battery_meter_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	p_bat_meter_data = (mt_battery_meter_custom_data *) dev->dev.platform_data;
	init_meter_global_data(dev);

	bm_print(BM_LOG_CRTI, "[battery_meter_probe] probe\n");
	/* select battery meter control method */
	battery_meter_ctrl = bm_ctrl_cmd;
	/* LOG System Set */
	init_proc_log_fg();

	/* Create File For FG UI DEBUG */
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Current);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_volt);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_current);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_zcv);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_temp);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_r);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_car);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_qmax);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_d0);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_d1);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_percentage);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_percentage_fg);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_percentage_voltmode);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_R_Offset);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_car_tune_value);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_charging_current_limit);

#ifdef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Battery_Cycle);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Aging_Factor);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Max_Battery_Voltage);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Min_Battery_Voltage);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Max_Battery_Current);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Min_Battery_Current);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Max_Battery_Temperature);
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Min_Battery_Temperature);
#endif

	return 0;
}

static int battery_meter_remove(struct platform_device *dev)
{
	bm_print(BM_LOG_CRTI, "[battery_meter_remove]\n");
	return 0;
}

static void battery_meter_shutdown(struct platform_device *dev)
{
	bm_print(BM_LOG_CRTI, "[battery_meter_shutdown]\n");
}

static int battery_meter_suspend(struct platform_device *dev, pm_message_t state)
{
	/* -- hibernation path */
	if (state.event == PM_EVENT_FREEZE) {
		pr_warn("[%s] %p:%p\n", __func__, battery_meter_ctrl, &bm_ctrl_cmd);
		battery_meter_ctrl = bm_ctrl_cmd;
	}
	/* -- end of hibernation path */

#if defined(CONFIG_POWER_EXT)

#elif defined(CONFIG_SOC_BY_SW_FG) || defined(CONFIG_SOC_BY_HW_FG)
	g_rtc_time_before_sleep = rtc_read_hw_time();
	battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_OCV, &g_hw_ocv_before_sleep);
#endif

	bm_print(BM_LOG_CRTI, "[battery_meter_suspend]\n");
	return 0;
}

#ifdef CONFIG_MTK_ENABLE_AGING_ALGORITHM
static void battery_aging_check(void)
{
	kal_int32 hw_ocv_after_sleep;
	kal_uint32 time_after_sleep;
	kal_int32 vbat;
	kal_int32 DOD_hwocv;
	kal_int32 DOD_now;
	kal_int32 qmax_aging = 0;
	kal_int32 dod_gap = 10;

	battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_OCV, &hw_ocv_after_sleep);

	vbat = battery_meter_get_battery_voltage();

	bm_print(BM_LOG_CRTI, "@@@ HW_OCV=%d, VBAT=%d\n", hw_ocv_after_sleep, vbat);

	time_after_sleep = rtc_read_hw_time();
	/* getrawmonotonic(&time_after_sleep); */

	bm_print(BM_LOG_CRTI, "@@@ suspend_time %lu resume time %lu\n", g_rtc_time_before_sleep,
		 time_after_sleep);

	/* aging */
	if (time_after_sleep - g_rtc_time_before_sleep > OCV_RECOVER_TIME) {
		if (aging_ocv_1 == 0) {
			aging_ocv_1 = hw_ocv_after_sleep;
			battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &aging_car_1);
			aging_resume_time_1 = time_after_sleep;

			if (fgauge_read_d_by_v(aging_ocv_1) > DOD1_ABOVE_THRESHOLD) {
				aging_ocv_1 = 0;
				bm_print(BM_LOG_CRTI,
					 "[aging check] reset and find next aging_ocv1 for better precision\n");
			}
		} else if (aging_ocv_2 == 0) {
			aging_ocv_2 = hw_ocv_after_sleep;
			battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &aging_car_2);
			aging_resume_time_2 = time_after_sleep;

			if (fgauge_read_d_by_v(aging_ocv_2) < DOD2_BELOW_THRESHOLD) {
				aging_ocv_2 = 0;
				bm_print(BM_LOG_CRTI,
					 "[aging check] reset and find next aging_ocv2 for better precision\n");
			}
		} else {
			aging_ocv_1 = aging_ocv_2;
			aging_car_1 = aging_car_2;
			aging_resume_time_1 = aging_resume_time_2;

			aging_ocv_2 = hw_ocv_after_sleep;
			battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &aging_car_2);
			aging_resume_time_2 = time_after_sleep;
		}

		if (aging_ocv_2 > 0) {
			aging_dod_1 = fgauge_read_d_by_v(aging_ocv_1);
			aging_dod_2 = fgauge_read_d_by_v(aging_ocv_2);

			/* TODO: check if current too small to get accurate car */

			/* check dod region to avoid hwocv error margin */
			dod_gap = MIN_DOD_DIFF_THRESHOLD;

			/* check if DOD gap bigger than setting */
			if (aging_dod_2 > aging_dod_1 && (aging_dod_2 - aging_dod_1) >= dod_gap) {
				/* do aging calculation */
				qmax_aging =
				    (100 * (aging_car_1 - aging_car_2)) / (aging_dod_2 -
									   aging_dod_1);

				/* update if aging over 10%. */
				if (gFG_BATT_CAPACITY > qmax_aging
				    && ((gFG_BATT_CAPACITY - qmax_aging) >
					(gFG_BATT_CAPACITY / (100 - MIN_AGING_FACTOR)))) {
					bm_print(BM_LOG_CRTI,
						 "[aging check] before apply aging, qmax_aging(%d) qmax_now(%d) ocv1(%d) dod1(%d) car1(%d) ocv2(%d) dod2(%d) car2(%d)\n",
						 qmax_aging, gFG_BATT_CAPACITY, aging_ocv_1,
						 aging_dod_1, aging_car_1, aging_ocv_2, aging_dod_2,
						 aging_car_2);

#ifdef CONFIG_MTK_BATTERY_LIFETIME_DATA_SUPPORT
					gFG_aging_factor = 100 -
						((gFG_BATT_CAPACITY - qmax_aging) * 100) / gFG_BATT_CAPACITY;
#endif

					if (gFG_BATT_CAPACITY_aging > qmax_aging) {
						bm_print(BM_LOG_CRTI,
							 "[aging check] new qmax_aging %d old qmax_aging %d\n",
							 qmax_aging, gFG_BATT_CAPACITY_aging);
						gFG_BATT_CAPACITY_aging = qmax_aging;
						gFG_DOD0 = aging_dod_2;
						gFG_DOD1 = gFG_DOD0;
						reset_parameter_car();
					} else {
						bm_print(BM_LOG_CRTI,
							 "[aging check] current qmax_aging %d is smaller than calculated qmax_aging %d\n",
							 gFG_BATT_CAPACITY_aging, qmax_aging);
					}
					aging_ocv_1 = 0;
					aging_ocv_2 = 0;
				} else {
					aging_ocv_2 = 0;
					bm_print(BM_LOG_CRTI,
						 "[aging check] show no degrade, qmax_aging(%d) qmax_now(%d) ocv1(%d) dod1(%d) car1(%d) ocv2(%d) dod2(%d) car2(%d)\n",
						 qmax_aging, gFG_BATT_CAPACITY, aging_ocv_1,
						 aging_dod_1, aging_car_1, aging_ocv_2, aging_dod_2,
						 aging_car_2);
					bm_print(BM_LOG_CRTI,
						 "[aging check] reset and find next aging_ocv2\n");
				}
			} else {
				aging_ocv_2 = 0;
				bm_print(BM_LOG_CRTI,
					 "[aging check] reset and find next aging_ocv2\n");
			}
			bm_print(BM_LOG_CRTI,
				 "[aging check] qmax_aging(%d) qmax_now(%d) ocv1(%d) dod1(%d) car1(%d) ocv2(%d) dod2(%d) car2(%d)\n",
				 qmax_aging, gFG_BATT_CAPACITY, aging_ocv_1, aging_dod_1,
				 aging_car_1, aging_ocv_2, aging_dod_2, aging_car_2);
		}
	}
	/* self-discharging */
	if (time_after_sleep - g_rtc_time_before_sleep > OCV_RECOVER_TIME) {	/* 30 mins */

		DOD_hwocv = fgauge_read_d_by_v(hw_ocv_after_sleep);

		if (DOD_hwocv < DOD1_ABOVE_THRESHOLD || DOD_hwocv > DOD2_BELOW_THRESHOLD) {

			/* update columb counter to get DOD_now. */
			battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR, &gFG_columb);
			DOD_now = 100 - fgauge_read_capacity(1);

			if (DOD_hwocv > DOD_now
				&& (DOD_hwocv - DOD_now > SELF_DISCHARGE_CHECK_THRESHOLD)) {
				gFG_DOD0 = DOD_hwocv;
				gFG_DOD1 = gFG_DOD0;
				reset_parameter_car();
				pr_notice(
					"[self-discharge check] reset to HWOCV. dod_ocv(%d) dod_now(%d)\n",
					DOD_hwocv, DOD_now);
			}
			pr_notice("[self-discharge check] dod_ocv(%d) dod_now(%d)\n", DOD_hwocv, DOD_now);
		}
	}
	bm_print(BM_LOG_CRTI, "sleeptime=(%d)s, be_ocv=(%d), af_ocv=(%d), D0=(%d), car=(%d)\n",
		time_after_sleep - g_rtc_time_before_sleep, g_hw_ocv_before_sleep,
		hw_ocv_after_sleep, gFG_DOD0, gFG_columb);
}
#endif

static int battery_meter_resume(struct platform_device *dev)
{
#if defined(CONFIG_POWER_EXT)

#elif defined(CONFIG_SOC_BY_HW_FG)

#ifdef CONFIG_MTK_ENABLE_AGING_ALGORITHM
	if (bat_is_charger_exist() == KAL_FALSE)
		battery_aging_check();

#endif
#elif defined(CONFIG_SOC_BY_SW_FG)
	kal_int32 hw_ocv_after_sleep;
	kal_uint32 rtc_time_after_sleep;
	kal_int32 sw_vbat;
	kal_int32 vbat_diff = 200;

	battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_OCV, &hw_ocv_after_sleep);
#if 1
	sw_vbat = battery_meter_get_battery_voltage();

	bm_print(BM_LOG_CRTI, "HW_OCV=%d, SW_VBAT=%d\n", hw_ocv_after_sleep, sw_vbat);

	if (hw_ocv_after_sleep < sw_vbat) {
		bm_print(BM_LOG_CRTI, "Ignore HW_OCV : small than SW_VBAT\n");
	} else if ((hw_ocv_after_sleep - sw_vbat) > vbat_diff) {
		bm_print(BM_LOG_CRTI, "Ignore HW_OCV : diff > %d\n", vbat_diff);
	} else
#endif
	{
		rtc_time_after_sleep = rtc_read_hw_time();

		if (rtc_time_after_sleep - g_rtc_time_before_sleep > 3600) {	/* 1hr */

			if (hw_ocv_after_sleep != g_hw_ocv_before_sleep) {
				gFG_DOD0 = fgauge_read_d_by_v(hw_ocv_after_sleep);
				oam_v_ocv_2 = oam_v_ocv_1 = hw_ocv_after_sleep;
				oam_car_1 = 0;
				oam_car_2 = 0;
			} else {
				oam_car_1 = oam_car_1 + (40 * (rtc_time_after_sleep - g_rtc_time_before_sleep) / 3600);	/* 0.1mAh */
				oam_car_2 = oam_car_2 + (40 * (rtc_time_after_sleep - g_rtc_time_before_sleep) / 3600);	/* 0.1mAh */
			}
		} else {
			oam_car_1 = oam_car_1 + (20 * (rtc_time_after_sleep - g_rtc_time_before_sleep) / 3600);	/* 0.1mAh */
			oam_car_2 = oam_car_2 + (20 * (rtc_time_after_sleep - g_rtc_time_before_sleep) / 3600);	/* 0.1mAh */
		}
		bm_print(BM_LOG_CRTI,
			 "sleeptime=(%d)s, be_ocv=(%d), af_ocv=(%d), D0=(%d), car1=(%d), car2=(%d)\n",
			 rtc_time_after_sleep - g_rtc_time_before_sleep, g_hw_ocv_before_sleep,
			 hw_ocv_after_sleep, gFG_DOD0, oam_car_1, oam_car_2);
	}
#endif
	bm_print(BM_LOG_CRTI, "[battery_meter_resume]\n");
	return 0;
}

#if 0				/* move to board-common-battery.c */
struct platform_device battery_meter_device = {
	.name = "battery_meter",
	.id = -1,
};
#endif

static struct platform_driver battery_meter_driver = {
	.probe = battery_meter_probe,
	.remove = battery_meter_remove,
	.shutdown = battery_meter_shutdown,
	.suspend = battery_meter_suspend,
	.resume = battery_meter_resume,
	.driver = {
		   .name = "battery_meter",
		   },
};

static int __init battery_meter_init(void)
{
	int ret;

#if 0				/* move to board-common-battery.c */
	ret = platform_device_register(&battery_meter_device);
	if (ret) {
		bm_print(BM_LOG_CRTI, "[battery_meter_driver] Unable to device register(%d)\n",
			 ret);
		return ret;
	}
#endif

	ret = platform_driver_register(&battery_meter_driver);
	if (ret) {
		bm_print(BM_LOG_CRTI, "[battery_meter_driver] Unable to register driver (%d)\n",
			 ret);
		return ret;
	}

	bm_print(BM_LOG_CRTI, "[battery_meter_driver] Initialization : DONE\n");

	return 0;

}

static void __exit battery_meter_exit(void)
{
}
module_init(battery_meter_init);
module_exit(battery_meter_exit);

MODULE_AUTHOR("James Lo");
MODULE_DESCRIPTION("Battery Meter Device Driver");
MODULE_LICENSE("GPL");
