#ifndef BATTERY_COMMON_H
#define BATTERY_COMMON_H

#include <linux/ioctl.h>
#include <mach/mt_typedefs.h>
#include "charging.h"
#include <linux/xlog.h>

/*****************************************************************************
 *  BATTERY VOLTAGE
 ****************************************************************************/
#define PRE_CHARGE_VOLTAGE                  3200
#define SYSTEM_OFF_VOLTAGE                  3400
#define CONSTANT_CURRENT_CHARGE_VOLTAGE     4100
#define CONSTANT_VOLTAGE_CHARGE_VOLTAGE     4200
#define CV_DROPDOWN_VOLTAGE                 4000
#define CHARGER_THRESH_HOLD                 4300
#define BATTERY_UVLO_VOLTAGE                2700

/*****************************************************************************
 *  BATTERY TIMER
 ****************************************************************************/
/* #define MAX_CHARGING_TIME             1*60*60         // 1hr */
/* #define MAX_CHARGING_TIME                   8*60*60   // 8hr */
#define MAX_CHARGING_TIME                   (12*60*60)  /* 12hr */
/* #define MAX_CHARGING_TIME                   24*60*60	// 24hr */

#define MAX_POSTFULL_SAFETY_TIME		1*30*60	/* 30mins */
#define MAX_PreCC_CHARGING_TIME		1*30*60	/* 0.5hr */

/* #define MAX_CV_CHARGING_TIME                  1*30*60         // 0.5hr */
#define MAX_CV_CHARGING_TIME			3*60*60	/* 3hr */


#define MUTEX_TIMEOUT                       5000
#define BAT_TASK_PERIOD                     10	/* 10sec */
#define g_free_bat_temp					1000	/* 1 s */

/*****************************************************************************
 *  BATTERY Protection
 ****************************************************************************/
#define Battery_Percent_100    100
#define charger_OVER_VOL	    1
#define BATTERY_UNDER_VOL		2
#define BATTERY_OVER_TEMP		3
#define ADC_SAMPLE_TIMES        5

/*****************************************************************************
 *  Pulse Charging State
 ****************************************************************************/
#define  CHR_PRE                        0x1000
#define  CHR_CC                         0x1001
#define  CHR_TOP_OFF                    0x1002
#define  CHR_POST_FULL                  0x1003
#define  CHR_BATFULL                    0x1004
#define  CHR_ERROR                      0x1005
#define  CHR_HOLD				        0x1006
#define  CHR_CMD_HOLD				        0x1007

/*****************************************************************************
 *  Call State
 ****************************************************************************/
#define CALL_IDLE 0
#define CALL_ACTIVE 1

/*****************************************************************************
 *  Enum
 ****************************************************************************/
typedef unsigned int WORD;


typedef enum {
	PMU_STATUS_OK = 0,
	PMU_STATUS_FAIL = 1,
} PMU_STATUS;


/*****************************************************************************
*   JEITA battery temperature standard
    charging info ,like temperatue, charging current, re-charging voltage, CV threshold would be reconfigurated.
    Temperature hysteresis default 6C.
    Reference table:
    degree    AC Current    USB current    CV threshold    Recharge Vol    hysteresis condition
    > 60       no charging current,             X                    X                     <54(Down)
    45~60     600mA         450mA             4.1V               4V                   <39(Down) >60(Up)
    10~45     600mA         450mA             4.2V               4.1V                <10(Down) >45(Up)
    0~10       600mA         450mA             4.1V               4V                   <0(Down)  >16(Up)
    -10~0     200mA         200mA             4V                  3.9V                <-10(Down) >6(Up)
    <-10      no charging current,              X                    X                    >-10(Up)
****************************************************************************/
typedef enum {
	TEMP_BELOW_NEG_10 = 0,
	TEMP_NEG_10_TO_POS_0,
	TEMP_POS_0_TO_POS_10,
	TEMP_POS_10_TO_POS_45,
	TEMP_POS_45_TO_POS_60,
	TEMP_ABOVE_POS_60
} temp_state_enum;

/*****************************************************************************
 *  structure
 ****************************************************************************/
typedef struct {
	kal_bool bat_exist;
	kal_bool bat_full;
	INT32 bat_charging_state;
	UINT32 bat_vol;
	kal_bool bat_in_recharging_state;
	kal_uint32 Vsense;
	kal_bool charger_exist;
	UINT32 charger_vol;
	INT32 charger_protect_status;
	INT32 ICharging;
	INT32 IBattery;
	INT32 temperature;
	INT32 temperatureR;
	INT32 temperatureV;
	UINT32 total_charging_time;
	UINT32 PRE_charging_time;
	UINT32 CC_charging_time;
	UINT32 TOPOFF_charging_time;
	UINT32 POSTFULL_charging_time;
	UINT32 charger_type;
	INT32 SOC;
	INT32 UI_SOC;
	UINT32 nPercent_ZCV;
	UINT32 nPrecent_UI_SOC_check_point;
	UINT32 ZCV;
} PMU_ChargerStruct;

struct mt_battery_charging_custom_data;

struct battery_common_data {
	int irq;
	bool common_init_done:1;
	bool init_done:1;
	bool down:1;
	bool usb_host_mode:1;
	CHARGING_CONTROL charger;
};

/*****************************************************************************
 *  Extern Variable
 ****************************************************************************/

extern struct battery_common_data g_bat;

extern PMU_ChargerStruct BMT_status;
extern kal_bool g_ftm_battery_flag;
extern int charging_level_data[1];
extern kal_bool g_call_state;
extern kal_bool g_cmd_hold_charging;
extern kal_bool g_charging_full_reset_bat_meter;
extern struct mt_battery_charging_custom_data *p_bat_charging_data;
extern int g_platform_boot_mode;
extern kal_int32 g_custom_charging_current;
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
extern int g_temp_status;
#endif
extern kal_bool g_bat_init_flag;
#ifdef MTK_BATTERY_NO_HAL
extern struct wake_lock battery_suspend_lock;
#endif

static inline int get_bat_average_voltage(void)
{
	return BMT_status.bat_vol;
}

static inline void bat_charger_init(struct mt_battery_charging_custom_data *pdata)
{
	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_INIT, pdata);
}

static inline void bat_charger_set_cv_voltage(BATTERY_VOLTAGE_ENUM cv_voltage)
{
	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_SET_CV_VOLTAGE, &cv_voltage);
}

static inline void bat_charger_enable(bool enable)
{
	u32 charging_enable = enable;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_ENABLE, &charging_enable);
}

static inline void bat_charger_enable_power_path(bool enable)
{
	u32 charging_enable = enable;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_ENABLE_POWERPATH, &charging_enable);
}

static inline bool bat_charger_is_pcm_timer_trigger(void)
{
	kal_bool is_pcm_timer_trigger = KAL_FALSE;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_GET_IS_PCM_TIMER_TRIGGER, &is_pcm_timer_trigger);

	return is_pcm_timer_trigger != 0;
}

static inline void bat_charger_reset_watchdog_timer(void)
{
	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_RESET_WATCH_DOG_TIMER, NULL);
}

static inline void bat_charger_dump_register(void)
{
	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_DUMP_REGISTER, NULL);
}

static inline int bat_charger_get_platform_boot_mode(void)
{
	int val = 0;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_GET_PLATFORM_BOOT_MODE, &val);

	return val;
}

static inline CHARGER_TYPE bat_charger_get_charger_type(void)
{
	CHARGER_TYPE chr_type = CHARGER_UNKNOWN;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_GET_CHARGER_TYPE, &chr_type);

	return chr_type;
}

static inline void bat_charger_set_platform_reset(void)
{
	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_SET_PLATFORM_RESET, NULL);
}

static inline int bat_charger_get_battery_status(void)
{
	int battery_status = 0;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_GET_BATTERY_STATUS, &battery_status);

	return battery_status;
}

static inline void bat_charger_set_hv_threshold(u32 hv_voltage)
{
	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_SET_HV_THRESHOLD, &hv_voltage);
}

static inline bool bat_charger_get_hv_status(void)
{
	kal_bool hv_status = 0;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_GET_HV_STATUS, &hv_status);

	return hv_status != 0;
}

static inline CHR_CURRENT_ENUM bat_charger_get_charging_current(void)
{
	CHR_CURRENT_ENUM charging_current = 0;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_GET_CURRENT, &charging_current);

	return charging_current;
}

static inline CHR_CURRENT_ENUM  bat_charger_get_input_current(void)
{
	CHR_CURRENT_ENUM input_current = 0;
	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_GET_INPUT_CURRENT, &input_current);
	return input_current;
}

static inline bool bat_charger_get_detect_status(void)
{
	kal_bool chr_status = false;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_GET_CHARGER_DET_STATUS, &chr_status);

	return chr_status != 0;
}

static inline bool bat_charger_get_charging_status(void)
{
	kal_uint32 status = 0;

	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_GET_CHARGING_STATUS, &status);

	return status != 0;
}

static inline void bat_charger_set_input_current(CHR_CURRENT_ENUM val)
{
	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_SET_INPUT_CURRENT, &val);
}

static inline void bat_charger_set_current(CHR_CURRENT_ENUM val)
{
	if (g_bat.charger)
		g_bat.charger(CHARGING_CMD_SET_CURRENT, &val);
}


/*****************************************************************************
 *  Extern Function
 ****************************************************************************/
extern int read_tbat_value(void);
extern kal_bool pmic_chrdet_status(void);
extern kal_bool bat_is_charger_exist(void);
extern kal_bool bat_is_charging_full(void);
extern kal_uint32 bat_get_ui_percentage(void);
extern kal_uint32 get_charging_setting_current(void);
extern kal_uint32 bat_is_recharging_phase(void);

extern void mt_power_off(void);
extern bool mt_usb_is_device(void);
extern void mt_usb_connect(void);
extern void mt_usb_disconnect(void);
extern int set_rtc_spare_fg_value(int val);
extern int get_rtc_spare_fg_value(void);

#ifdef CONFIG_MTK_SMART_BATTERY
extern void wake_up_bat(void);
extern unsigned long BAT_Get_Battery_Voltage(int polling_mode);
extern void mt_battery_charging_algorithm(void);
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
extern PMU_STATUS do_jeita_state_machine(void);
#endif

#else

#define wake_up_bat()			do {} while (0)
#define BAT_Get_Battery_Voltage(polling_mode)	({ 0; })

#endif

#endif				/* #ifndef BATTERY_COMMON_H */
