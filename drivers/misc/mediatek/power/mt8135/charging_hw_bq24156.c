/*****************************************************************************
 *
 * Filename:
 * ---------
 *    charging_pmic.c
 *
 * Project:
 * --------
 *   ALPS_Software
 *
 * Description:
 * ------------
 *   This file implements the interface between BMT and ADC scheduler.
 *
 * Author:
 * -------
 *  Oscar Liu
 *
 *============================================================================
  * $Revision:   1.0  $
 * $Modtime:   11 Aug 2005 10:28:16  $
 * $Log:   //mtkvs01/vmdata/Maui_sw/archives/mcu/hal/peripheral/inc/bmt_chr_setting.h-arc  $
 *             HISTORY
 * Below this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include <mach/charging.h>
#include "bq24156.h"
#include <drivers/misc/mediatek/power/mt8135/upmu_common.h>
#include <mach/mt_gpio.h>
#include <mach/upmu_hw.h>
#include <linux/xlog.h>
#include <linux/delay.h>
#include <mach/mt_sleep.h>
#include <mach/mt_boot.h>
#include <mach/system.h>
#include <mach/battery_custom_data.h>

 /* ============================================================ // */
 /* define */
 /* ============================================================ // */
#define STATUS_OK	0
#define STATUS_UNSUPPORTED	-1
#define GETARRAYNUM(array) (sizeof(array)/sizeof(array[0]))


 /* ============================================================ // */
 /* global variable */
 /* ============================================================ // */
static mt_battery_charging_custom_data *p_bat_charging_data;

#ifndef MTK_BQ24156_NO_CD_GPIO
/* pull high to disable charging process, p.23, CD pin */
int gpio_number = 0;
int gpio_off_mode = GPIO_MODE_GPIO;
int gpio_off_dir = GPIO_DIR_OUT;
int gpio_off_out = GPIO_OUT_ONE;

int gpio_on_mode = GPIO_MODE_GPIO;
int gpio_on_dir = GPIO_DIR_OUT;
int gpio_on_out = GPIO_OUT_ZERO;
#endif


#if defined(CONFIG_HIGH_BATTERY_VOLTAGE_SUPPORT)
int g_enable_high_vbat_spec = 1;
int BQ24156_HIGH_VOLTAGE_THRESHOLD = 4415;	/* 4360*1.01 + 10 */
#else
int g_enable_high_vbat_spec = 0;
int BQ24156_HIGH_VOLTAGE_THRESHOLD = 4260;	/* 4200*1.01 + 10 */
#endif

kal_bool charging_type_det_done = KAL_TRUE;

const kal_uint32 VBAT_CV_VTH[] = {
	BATTERY_VOLT_03_500000_V, BATTERY_VOLT_03_520000_V, BATTERY_VOLT_03_540000_V,
	    BATTERY_VOLT_03_560000_V,
	BATTERY_VOLT_03_580000_V, BATTERY_VOLT_03_600000_V, BATTERY_VOLT_03_620000_V,
	    BATTERY_VOLT_03_640000_V,
	BATTERY_VOLT_03_660000_V, BATTERY_VOLT_03_680000_V, BATTERY_VOLT_03_700000_V,
	    BATTERY_VOLT_03_720000_V,
	BATTERY_VOLT_03_740000_V, BATTERY_VOLT_03_760000_V, BATTERY_VOLT_03_780000_V,
	    BATTERY_VOLT_03_800000_V,
	BATTERY_VOLT_03_820000_V, BATTERY_VOLT_03_840000_V, BATTERY_VOLT_03_860000_V,
	    BATTERY_VOLT_03_880000_V,
	BATTERY_VOLT_03_900000_V, BATTERY_VOLT_03_920000_V, BATTERY_VOLT_03_940000_V,
	    BATTERY_VOLT_03_960000_V,
	BATTERY_VOLT_03_980000_V, BATTERY_VOLT_04_000000_V, BATTERY_VOLT_04_020000_V,
	    BATTERY_VOLT_04_040000_V,
	BATTERY_VOLT_04_060000_V, BATTERY_VOLT_04_080000_V, BATTERY_VOLT_04_100000_V,
	    BATTERY_VOLT_04_120000_V,
	BATTERY_VOLT_04_140000_V, BATTERY_VOLT_04_160000_V, BATTERY_VOLT_04_180000_V,
	    BATTERY_VOLT_04_200000_V,
	BATTERY_VOLT_04_220000_V, BATTERY_VOLT_04_240000_V, BATTERY_VOLT_04_260000_V,
	    BATTERY_VOLT_04_280000_V,
	BATTERY_VOLT_04_300000_V, BATTERY_VOLT_04_320000_V, BATTERY_VOLT_04_340000_V,
	    BATTERY_VOLT_04_360000_V,
	BATTERY_VOLT_04_380000_V, BATTERY_VOLT_04_400000_V, BATTERY_VOLT_04_420000_V,
	    BATTERY_VOLT_04_440000_V
};

const kal_uint32 CS_VTH[] = {
	CHARGE_CURRENT_550_00_MA, CHARGE_CURRENT_650_00_MA, CHARGE_CURRENT_750_00_MA,
	    CHARGE_CURRENT_850_00_MA,
	CHARGE_CURRENT_950_00_MA, CHARGE_CURRENT_1050_00_MA, CHARGE_CURRENT_1150_00_MA,
	    CHARGE_CURRENT_1250_00_MA,
	135000, 145000, 155000
};


const kal_uint32 INPUT_CS_VTH[] = {
	CHARGE_CURRENT_100_00_MA, CHARGE_CURRENT_500_00_MA, CHARGE_CURRENT_800_00_MA,
	    CHARGE_CURRENT_MAX
};

const kal_uint32 VCDT_HV_VTH[] = {
	BATTERY_VOLT_04_200000_V, BATTERY_VOLT_04_250000_V, BATTERY_VOLT_04_300000_V,
	    BATTERY_VOLT_04_350000_V,
	BATTERY_VOLT_04_400000_V, BATTERY_VOLT_04_450000_V, BATTERY_VOLT_04_500000_V,
	    BATTERY_VOLT_04_550000_V,
	BATTERY_VOLT_04_600000_V, BATTERY_VOLT_06_000000_V, BATTERY_VOLT_06_500000_V,
	    BATTERY_VOLT_07_000000_V,
	BATTERY_VOLT_07_500000_V, BATTERY_VOLT_08_500000_V, BATTERY_VOLT_09_500000_V,
	    BATTERY_VOLT_10_500000_V
};

 /* ============================================================ // */
 /* function prototype */
 /* ============================================================ // */


 /* ============================================================ // */
 /* extern variable */
 /* ============================================================ // */

 /* ============================================================ // */
 /* extern function */
 /* ============================================================ // */
extern kal_uint32 upmu_get_reg_value(kal_uint32 reg);
extern bool mt_usb_is_device(void);
extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);

 /* ============================================================ // */
kal_uint32 charging_value_to_parameter(const kal_uint32 *parameter, const kal_uint32 array_size,
				       const kal_uint32 val)
{
	if (val < array_size) {
		return parameter[val];
	} else {
		battery_xlog_printk(BAT_LOG_CRTI, "Can't find the parameter \r\n");
		return parameter[0];
	}
}


kal_uint32 charging_parameter_to_value(const kal_uint32 *parameter, const kal_uint32 array_size,
				       const kal_uint32 val)
{
	kal_uint32 i;

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i)) {
			return i;
		}
	}

	battery_xlog_printk(BAT_LOG_FULL, "NO register value match. val=%d\r\n", val);
	/* TODO: ASSERT(0);      // not find the value */
	return 0;
}


static kal_uint32 bmt_find_closest_level(const kal_uint32 *pList, kal_uint32 number,
					 kal_uint32 level)
{
	kal_uint32 i;
	kal_uint32 max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = KAL_TRUE;
	else
		max_value_in_last_element = KAL_FALSE;

	if (max_value_in_last_element == KAL_TRUE) {
		for (i = (number - 1); i != 0; i--)	/* max value in the last element */
		{
			if (pList[i] <= level) {
				return pList[i];
			}
		}

		battery_xlog_printk(BAT_LOG_CRTI,
				    "Can't find closest level, small value first \r\n");
		return pList[0];
		/* return CHARGE_CURRENT_0_00_MA; */
	} else {
		for (i = 0; i < number; i++)	/* max value in the first element */
		{
			if (pList[i] <= level) {
				return pList[i];
			}
		}

		battery_xlog_printk(BAT_LOG_CRTI,
				    "Can't find closest level, large value first \r\n");
		return pList[number - 1];
		/* return CHARGE_CURRENT_0_00_MA; */
	}
}

static kal_uint32 charging_hw_init(void *data)
{
	kal_uint32 status = STATUS_OK;

	upmu_set_rg_bc11_bb_ctrl(1);	/* BC11_BB_CTRL */
	upmu_set_rg_bc11_rst(1);	/* BC11_RST */

	p_bat_charging_data = (mt_battery_charging_custom_data *)data;
	gpio_number = p_bat_charging_data->charger_enable_pin;

	/* GPIO related */
#ifndef MTK_BQ24156_NO_CD_GPIO
	/* enable charging process by GPIO, pull CD low, p.3 */
	mt_set_gpio_mode(gpio_number, gpio_on_mode);
	mt_set_gpio_dir(gpio_number, gpio_on_dir);
	mt_set_gpio_out(gpio_number, gpio_on_out);
#endif

	/* enable Safety limit register reset */
	mt_set_gpio_mode(p_bat_charging_data->charger_otg_pin, GPIO_MODE_GPIO);
	mt_set_gpio_dir(p_bat_charging_data->charger_otg_pin, GPIO_DIR_OUT);
	mt_set_gpio_out(p_bat_charging_data->charger_otg_pin, GPIO_OUT_ONE);


	/* BQ24156 related */
	/* reset to default state */
	bq24156_set_i_safe(0xF);	/* always set i safe register first */
	bq24156_set_v_safe(0x8);

	bq24156_set_tmr_rst(0x1);	/* kick watchdog, clear safety timer */

	bq24156_set_v_low(0x0);	/* weak battery is 3.4V */
	bq24156_set_te(0x1);	/* enable current termination */
	bq24156_set_ce(0x0);	/* enable charger */
	bq24156_set_hz_mode(0x0);	/* disable high impendance mode */

	if (g_enable_high_vbat_spec == 1) {
		bq24156_set_oreg(0x2B);	/* 4.36V */
	} else {
		bq24156_set_oreg(0x23);	/* 4.2V */
	}

	bq24156_set_iterm(0x5);	/* Termination current 300mA */

	return status;
}


static kal_uint32 charging_dump_register(void *data)
{
	kal_uint32 status = STATUS_OK;

	battery_xlog_printk(BAT_LOG_CRTI, "charging_dump_register\r\n");

	bq24156_dump_register();

	return status;
}


static kal_uint32 charging_enable(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint32 enable = *(kal_uint32 *) (data);

	if (KAL_TRUE == enable) {
		/* GPIO related */
#ifndef MTK_BQ24156_NO_CD_GPIO
		/* enable charging process by GPIO, pull CD low, p.3 */
		mt_set_gpio_mode(gpio_number, gpio_on_mode);
		mt_set_gpio_dir(gpio_number, gpio_on_dir);
		mt_set_gpio_out(gpio_number, gpio_on_out);
#endif

		/* enable Safety limit register reset */
		mt_set_gpio_mode(p_bat_charging_data->charger_otg_pin, GPIO_MODE_GPIO);
		mt_set_gpio_dir(p_bat_charging_data->charger_otg_pin, GPIO_DIR_OUT);
		mt_set_gpio_out(p_bat_charging_data->charger_otg_pin, GPIO_OUT_ONE);


		/* BQ24156 related */
		/* reset to default state */
		bq24156_set_i_safe(0xF);	/* always set i safe register first */
		bq24156_set_v_safe(0x8);

		bq24156_set_tmr_rst(0x1);	/* kick watchdog, clear safety timer */

		bq24156_set_ce(0x0);	/* enable charger */
		bq24156_set_hz_mode(0x0);	/* disable high impendance mode */
	} else {
#if defined(CONFIG_USB_MTK_HDRC_HCD)
		if (mt_usb_is_device())
#endif
		{
#ifndef MTK_BQ24156_NO_CD_GPIO
			/* disable charging process by GPIO */
			mt_set_gpio_mode(gpio_number, gpio_off_mode);
			mt_set_gpio_dir(gpio_number, gpio_off_dir);
			mt_set_gpio_out(gpio_number, gpio_off_out);
#endif

			/* enable Safety limit register reset */
			mt_set_gpio_mode(p_bat_charging_data->charger_otg_pin, GPIO_MODE_GPIO);
			mt_set_gpio_dir(p_bat_charging_data->charger_otg_pin, GPIO_DIR_OUT);
			mt_set_gpio_out(p_bat_charging_data->charger_otg_pin, GPIO_OUT_ONE);

			/* BQ24156 related */
			bq24156_set_i_safe(0xF);	/* always set i safe register first */
			bq24156_set_v_safe(0x8);

			/* disable charging process by BQ24156 register, p.28, table 4 */
			bq24156_set_ce(0x1);
			bq24156_set_tmr_rst(0x1);	/* kick watchdog, clear safety timer */
			bq24156_set_hz_mode(0x1);	/* enable high impendance mode */
		}
	}

	return status;
}


static kal_uint32 charging_set_cv_voltage(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint16 register_value;
	kal_uint32 cv_value = *(kal_uint32 *) (data);

	register_value =
	    charging_parameter_to_value(VBAT_CV_VTH, GETARRAYNUM(VBAT_CV_VTH), cv_value);
	bq24156_set_oreg(register_value);

	return status;
}


static kal_uint32 charging_get_current(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint32 array_size;
	kal_uint32 reg_value;

	/* Get current level */
	array_size = GETARRAYNUM(CS_VTH);
	reg_value = bq24156_get_input_charging_current();
	*(kal_uint32 *) data = charging_value_to_parameter(CS_VTH, array_size, reg_value);

	return status;
}



static kal_uint32 charging_set_current(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint32 set_chr_current;
	kal_uint32 array_size;
	kal_uint32 register_value;
	kal_uint32 current_value = *(kal_uint32 *) data;

	if (current_value <= CHARGE_CURRENT_350_00_MA) {
		bq24156_set_low_chg(1);
	} else {
		bq24156_set_low_chg(0);
		array_size = GETARRAYNUM(CS_VTH);
		set_chr_current = bmt_find_closest_level(CS_VTH, array_size, current_value);
		register_value = charging_parameter_to_value(CS_VTH, array_size, set_chr_current);
		bq24156_set_icharge(register_value);
	}

	return status;
}


static kal_uint32 charging_set_input_current(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint32 set_chr_current;
	kal_uint32 array_size;
	kal_uint32 register_value;

	if (*(kal_uint32 *) data > CHARGE_CURRENT_500_00_MA) {
		register_value = 0x3;
	} else {
		array_size = GETARRAYNUM(INPUT_CS_VTH);
		set_chr_current =
		    bmt_find_closest_level(INPUT_CS_VTH, array_size, *(kal_uint32 *) data);
		register_value =
		    charging_parameter_to_value(INPUT_CS_VTH, array_size, set_chr_current);
	}

	bq24156_set_input_charging_current(register_value);

	return status;
}


static kal_uint32 charging_get_charging_status(void *data)
{
	kal_uint32 status = STATUS_OK;
	kal_uint32 ret_val;

	ret_val = bq24156_get_chip_status();

	if (ret_val == 0x2)
		*(kal_uint32 *) data = KAL_TRUE;
	else
		*(kal_uint32 *) data = KAL_FALSE;

	return status;
}


static kal_uint32 charging_reset_watch_dog_timer(void *data)
{
	kal_uint32 status = STATUS_OK;

	battery_xlog_printk(BAT_LOG_CRTI, "charging_reset_watch_dog_timer\r\n");

	bq24156_set_tmr_rst(0x1);	/* Kick watchdog */

	return status;
}


static kal_uint32 charging_set_hv_threshold(void *data)
{
	kal_uint32 status = STATUS_OK;

	kal_uint32 set_hv_voltage;
	kal_uint32 array_size;
	kal_uint16 register_value;
	kal_uint32 voltage = *(kal_uint32 *) (data);

	array_size = GETARRAYNUM(VCDT_HV_VTH);
	set_hv_voltage = bmt_find_closest_level(VCDT_HV_VTH, array_size, voltage);
	register_value = charging_parameter_to_value(VCDT_HV_VTH, array_size, set_hv_voltage);
	upmu_set_rg_vcdt_hv_vth(register_value);

	return status;
}


static kal_uint32 charging_get_hv_status(void *data)
{
	kal_uint32 status = STATUS_OK;

	*(kal_bool *) (data) = upmu_get_rgs_vcdt_hv_det();

	return status;
}


static kal_uint32 charging_get_battery_status(void *data)
{
	kal_uint32 status = STATUS_OK;

	upmu_set_baton_tdet_en(1);
	upmu_set_rg_baton_en(1);
	*(kal_bool *) (data) = upmu_get_rgs_baton_undet();

	return status;
}


static kal_uint32 charging_get_charger_det_status(void *data)
{
	kal_uint32 status = STATUS_OK;

	*(kal_bool *) (data) = upmu_get_rgs_chrdet();

	return status;
}


kal_bool charging_type_detection_done(void)
{
	return charging_type_det_done;
}

extern CHARGER_TYPE hw_charger_type_detection(void);

static kal_uint32 charging_get_charger_type(void *data)
{
	kal_uint32 status = STATUS_OK;
	CHARGER_TYPE charger_type = CHARGER_UNKNOWN;
#if defined(CONFIG_POWER_EXT)
	*(CHARGER_TYPE *) (data) = STANDARD_HOST;
#else

	charging_type_det_done = KAL_FALSE;

	charger_type = hw_charger_type_detection();
	battery_xlog_printk(BAT_LOG_CRTI, "charging_get_charger_type = %d\r\n", charger_type);

	*(CHARGER_TYPE *) (data) = charger_type;

	charging_type_det_done = KAL_TRUE;
#endif
	return status;
}

static kal_uint32 charging_get_is_pcm_timer_trigger(void *data)
{
	kal_uint32 status = STATUS_OK;

	if (slp_get_wake_reason() == WR_PCM_TIMER)
		*(kal_bool *) (data) = KAL_TRUE;
	else
		*(kal_bool *) (data) = KAL_FALSE;

	battery_xlog_printk(BAT_LOG_CRTI, "slp_get_wake_reason=%d\n", slp_get_wake_reason());

	return status;
}

static kal_uint32 charging_set_platform_reset(void *data)
{
	kal_uint32 status = STATUS_OK;

	battery_xlog_printk(BAT_LOG_CRTI, "charging_set_platform_reset\n");

	arch_reset(0, NULL);

	return status;
}

static kal_uint32 charging_get_platfrom_boot_mode(void *data)
{
	kal_uint32 status = STATUS_OK;

	*(kal_uint32 *) (data) = get_boot_mode();

	battery_xlog_printk(BAT_LOG_CRTI, "get_boot_mode=%d\n", get_boot_mode());

	return status;
}

static kal_uint32 charging_enable_powerpath(void *data)
{
	return STATUS_OK;
}

static kal_uint32(*const charging_func[CHARGING_CMD_NUMBER]) (void *data) = {
charging_hw_init, charging_dump_register, charging_enable, charging_set_cv_voltage,
	    charging_get_current, charging_set_current, charging_set_input_current,
	    charging_get_charging_status, charging_reset_watch_dog_timer,
	    charging_set_hv_threshold, charging_get_hv_status, charging_get_battery_status,
	    charging_get_charger_det_status, charging_get_charger_type,
	    charging_get_is_pcm_timer_trigger, charging_set_platform_reset,
	    charging_get_platfrom_boot_mode, charging_enable_powerpath};


 /*
  * FUNCTION
  *             Internal_chr_control_handler
  *
  * DESCRIPTION
  *              This function is called to set the charger hw
  *
  * CALLS
  *
  * PARAMETERS
  *             None
  *
  * RETURNS
  *
  *
  * GLOBALS AFFECTED
  *        None
  */
kal_int32 chr_control_interface(CHARGING_CTRL_CMD cmd, void *data)
{
	kal_int32 status;
	if (cmd < CHARGING_CMD_NUMBER)
		status = charging_func[cmd] (data);
	else
		return STATUS_UNSUPPORTED;

	return status;
}
