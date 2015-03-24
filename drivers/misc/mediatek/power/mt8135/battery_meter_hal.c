#include <linux/xlog.h>
#include <linux/delay.h>

#include <mach/pmic_mt6397_sw.h>
#include <drivers/misc/mediatek/power/mt8135/upmu_common.h>
#include <mach/upmu_hw.h>

#include <mach/battery_meter_hal.h>

#define HWOCV_E1_WORKAROUND

#ifdef CONFIG_SOC_BY_EXT_HW_FG
#ifdef CONFIG_MTK_BQ27541_SUPPORT
#include "bq27541.h"
#endif
#endif

#include <mach/battery_custom_data.h>
#include "battery_meter_hal_custom_wrap.h"

/* ============================================================ // */
/* define */
/* ============================================================ // */
#define STATUS_OK    0
#define STATUS_UNSUPPORTED    -1
#define VOLTAGE_FULL_RANGE     1200
#define ADC_PRECISE           1024	/* 10 bits */

#define UNIT_FGCURRENT     (158122)	/* 158.122 uA */

/* ============================================================ // */
/* global variable */
/* ============================================================ // */
kal_int32 chip_diff_trim_value_4_0 = 0;
kal_int32 chip_diff_trim_value = 0;	/* unit = 0.1 */

kal_int32 g_hw_ocv_tune_value = 8;

kal_bool g_fg_is_charging = 0;

#ifdef CONFIG_SOC_BY_EXT_HW_FG
kal_bool ext_fg_init_done = KAL_FALSE;
#endif

static mt_battery_meter_custom_data *p_bat_meter_data;

/* ============================================================ // */
/* function prototype */
/* ============================================================ // */

/* ============================================================ // */
void get_hw_chip_diff_trim_value(void)
{
#if defined(CONFIG_POWER_EXT)
#else
#if 0
	kal_int32 reg_val_1 = 0;
	kal_int32 reg_val_2 = 0;

	reg_val_1 = upmu_get_reg_value(0x01E8);
	reg_val_1 = (reg_val_1 & 0xF000) >> 12;

	reg_val_2 = upmu_get_reg_value(0x01EA);
	reg_val_2 = (reg_val_2 & 0x0001);

	chip_diff_trim_value_4_0 = reg_val_1 | (reg_val_2 << 4);

	bm_print(BM_LOG_FULL,
		 "[Chip_Trim] Reg[0x%x]=0x%x, Reg[0x%x]=0x%x, chip_diff_trim_value_4_0=%d\n",
		 0x01E8, upmu_get_reg_value(0x01E8), 0x01EA, upmu_get_reg_value(0x01EA),
		 chip_diff_trim_value_4_0);

#else
	bm_print(BM_LOG_CRTI, "[Chip_Trim] skip chip trim value\n");
#endif

	switch (chip_diff_trim_value_4_0) {
	case 0:
		chip_diff_trim_value = 1000;
		bm_print(BM_LOG_FULL, "[Chip_Trim] chip_diff_trim_value = 1000\n");
		break;
	case 1:
		chip_diff_trim_value = 1005;
		break;
	case 2:
		chip_diff_trim_value = 1010;
		break;
	case 3:
		chip_diff_trim_value = 1015;
		break;
	case 4:
		chip_diff_trim_value = 1020;
		break;
	case 5:
		chip_diff_trim_value = 1025;
		break;
	case 6:
		chip_diff_trim_value = 1030;
		break;
	case 7:
		chip_diff_trim_value = 1035;
		break;
	case 8:
		chip_diff_trim_value = 1040;
		break;
	case 9:
		chip_diff_trim_value = 1045;
		break;
	case 10:
		chip_diff_trim_value = 1050;
		break;
	case 11:
		chip_diff_trim_value = 1055;
		break;
	case 12:
		chip_diff_trim_value = 1060;
		break;
	case 13:
		chip_diff_trim_value = 1065;
		break;
	case 14:
		chip_diff_trim_value = 1070;
		break;
	case 15:
		chip_diff_trim_value = 1075;
		break;
	case 31:
		chip_diff_trim_value = 995;
		break;
	case 30:
		chip_diff_trim_value = 990;
		break;
	case 29:
		chip_diff_trim_value = 985;
		break;
	case 28:
		chip_diff_trim_value = 980;
		break;
	case 27:
		chip_diff_trim_value = 975;
		break;
	case 26:
		chip_diff_trim_value = 970;
		break;
	case 25:
		chip_diff_trim_value = 965;
		break;
	case 24:
		chip_diff_trim_value = 960;
		break;
	case 23:
		chip_diff_trim_value = 955;
		break;
	case 22:
		chip_diff_trim_value = 950;
		break;
	case 21:
		chip_diff_trim_value = 945;
		break;
	case 20:
		chip_diff_trim_value = 940;
		break;
	case 19:
		chip_diff_trim_value = 935;
		break;
	case 18:
		chip_diff_trim_value = 930;
		break;
	case 17:
		chip_diff_trim_value = 925;
		break;
	default:
		bm_print(BM_LOG_FULL, "[Chip_Trim] Invalid value(%d)\n", chip_diff_trim_value_4_0);
		break;
	}

	bm_print(BM_LOG_FULL, "[Chip_Trim] %d,%d\n",
		 chip_diff_trim_value_4_0, chip_diff_trim_value);
#endif
}

kal_int32 use_chip_trim_value(kal_int32 not_trim_val)
{
#if defined(CONFIG_POWER_EXT)
	return not_trim_val;
#else
	kal_int32 ret_val = 0;

	ret_val = ((not_trim_val * chip_diff_trim_value) / 1000);

	bm_print(BM_LOG_FULL, "[use_chip_trim_value] %d -> %d\n", not_trim_val, ret_val);

	return ret_val;
#endif
}

int get_hw_ocv(void)
{
#if defined(CONFIG_POWER_EXT)
	return 4001;
#else
	kal_int32 adc_result_reg = 0;
	kal_int32 adc_result = 0;
	kal_int32 r_val_temp = 4;

#if defined(CONFIG_SWCHR_POWER_PATH)
	adc_result_reg = upmu_get_rg_adc_out_wakeup_swchr_trim();
	adc_result = (adc_result_reg * r_val_temp * VOLTAGE_FULL_RANGE) / ADC_PRECISE;
	bm_print(BM_LOG_CRTI, "[oam] get_hw_ocv (swchr) : adc_result_reg=%d, adc_result=%d\n",
		 adc_result_reg, adc_result);
#else
	adc_result_reg = upmu_get_rg_adc_out_wakeup_pchr();
	adc_result = (adc_result_reg * r_val_temp * VOLTAGE_FULL_RANGE) / ADC_PRECISE;
	bm_print(BM_LOG_CRTI, "[oam] get_hw_ocv (pchr) : adc_result_reg=%d, adc_result=%d\n",
		 adc_result_reg, adc_result);
#endif

	adc_result += g_hw_ocv_tune_value;
	return adc_result;
#endif
}


/* ============================================================// */

static void dump_nter(void)
{
	bm_print(BM_LOG_CRTI, "[dump_nter] upmu_get_fg_nter_29_16 = 0x%x\r\n",
		 upmu_get_fg_nter_29_16());
	bm_print(BM_LOG_CRTI, "[dump_nter] upmu_get_fg_nter_15_00 = 0x%x\r\n",
		 upmu_get_fg_nter_15_00());
}

static void dump_car(void)
{
	bm_print(BM_LOG_CRTI, "[dump_car] upmu_get_fg_car_35_32 = 0x%x\r\n",
		 upmu_get_fg_car_35_32());
	bm_print(BM_LOG_CRTI, "[dump_car] upmu_get_fg_car_31_16 = 0x%x\r\n",
		 upmu_get_fg_car_31_16());
	bm_print(BM_LOG_CRTI, "[dump_car] upmu_get_fg_car_15_00 = 0x%x\r\n",
		 upmu_get_fg_car_15_00());
}

static kal_uint32 fg_get_data_ready_status(void)
{
	kal_uint32 ret = 0;
	kal_uint32 temp_val = 0;

	ret = pmic_read_interface(FGADC_CON0, &temp_val, 0xFFFF, 0x0);

	bm_print(BM_LOG_FULL, "[fg_get_data_ready_status] Reg[0x%x]=0x%x\r\n", FGADC_CON0,
		 temp_val);

	temp_val =
	    (temp_val & (PMIC_FG_LATCHDATA_ST_MASK << PMIC_FG_LATCHDATA_ST_SHIFT)) >>
	    PMIC_FG_LATCHDATA_ST_SHIFT;

	return temp_val;
}

static kal_int32 fgauge_read_current(void *data);
static kal_int32 fgauge_initialization(void *data)
{
	kal_uint32 ret = 0;
	kal_int32 current_temp = 0;
	int m = 0;

	p_bat_meter_data = (mt_battery_meter_custom_data *) data;

#ifdef CONFIG_SOC_BY_EXT_HW_FG
	/* wait I2C init done */
	bm_print(BM_LOG_CRTI, "[fgauge_initialization] wait ext fg init done...\n");
	m = 0;
	while (1) {
		if (ext_fg_init_done == KAL_TRUE) {
			bm_print(BM_LOG_CRTI, "[fgauge_initialization] ext fg init done!\n");
			break;
		} else {
			m++;
			msleep(1);
			if (m > 1000) {
				bm_print(BM_LOG_CRTI, "[fgauge_initialization] timeout!\r\n");
				break;
			}
		}
	}

	return STATUS_OK;
#endif

	get_hw_chip_diff_trim_value();

/* 1. HW initialization */
	/* FGADC clock is 32768Hz from RTC */
	/* Enable FGADC in current mode at 32768Hz with auto-calibration */

	/* (1)    Enable VA2 */
	/* (2)    Enable FGADC clock for digital */
	upmu_set_rg_fgadc_ana_ck_pdn(0);
	upmu_set_rg_fgadc_ck_pdn(0);
	/* (3)    Set current mode, auto-calibration mode and 32KHz clock source */
	ret = pmic_config_interface(FGADC_CON0, 0x0028, 0x00FF, 0x0);
	/* (4)    Enable FGADC */
	ret = pmic_config_interface(FGADC_CON0, 0x0029, 0x00FF, 0x0);

	/* reset HW FG */
	ret = pmic_config_interface(FGADC_CON0, 0x7100, 0xFF00, 0x0);
	bm_print(BM_LOG_CRTI, "******** [fgauge_initialization] reset HW FG!\n");

	/* make sure init finish */
	m = 0;
	while (current_temp == 0) {
		fgauge_read_current(&current_temp);
		m++;
		if (m > 1000) {
			bm_print(BM_LOG_CRTI, "[fgauge_initialization] timeout!\r\n");
			break;
		}
	}

	bm_print(BM_LOG_CRTI, "******** [fgauge_initialization] Done!\n");

	return STATUS_OK;
}

static kal_int32 fgauge_read_current(void *data)
{
	kal_uint16 uvalue16 = 0;
	kal_int32 dvalue = 0;
	int m = 0;
	kal_int64 Temp_Value = 0;
	kal_int32 Current_Compensate_Value = 0;
	kal_uint32 ret = 0;

/* HW Init */
	/* (1)    i2c_write (0x60, 0xC8, 0x01); // Enable VA2 */
	/* (2)    i2c_write (0x61, 0x15, 0x00); // Enable FGADC clock for digital */
	/* (3)    i2c_write (0x61, 0x69, 0x28); // Set current mode, auto-calibration mode and 32KHz clock source */
	/* (4)    i2c_write (0x61, 0x69, 0x29); // Enable FGADC */

/* Read HW Raw Data */
	/* (1)    Set READ command */
	ret = pmic_config_interface(FGADC_CON0, 0x0200, 0xFF00, 0x0);
	/* (2)     Keep i2c read when status = 1 (0x06) */
	m = 0;
	while (fg_get_data_ready_status() == 0) {
		m++;
		if (m > 1000) {
			bm_print(BM_LOG_FULL,
				 "[fgauge_read_current] fg_get_data_ready_status timeout 1 !\r\n");
			break;
		}
	}
	/* (3)    Read FG_CURRENT_OUT[15:08] */
	/* (4)    Read FG_CURRENT_OUT[07:00] */
	uvalue16 = upmu_get_fg_current_out();
	bm_print(BM_LOG_FULL, "[fgauge_read_current] : FG_CURRENT = %x\r\n", uvalue16);
	/* (5)    (Read other data) */
	/* (6)    Clear status to 0 */
	ret = pmic_config_interface(FGADC_CON0, 0x0800, 0xFF00, 0x0);
	/* (7)    Keep i2c read when status = 0 (0x08) */
	/* while ( fg_get_sw_clear_status() != 0 ) */
	m = 0;
	while (fg_get_data_ready_status() != 0) {
		m++;
		if (m > 1000) {
			bm_print(BM_LOG_FULL,
				 "[fgauge_read_current] fg_get_data_ready_status timeout 2 !\r\n");
			break;
		}
	}
	/* (8)    Recover original settings */
	ret = pmic_config_interface(FGADC_CON0, 0x0000, 0xFF00, 0x0);

/* calculate the real world data */
	dvalue = (kal_uint32) uvalue16;
	if (dvalue == 0) {
		Temp_Value = (kal_int64) dvalue;
		g_fg_is_charging = KAL_FALSE;
	} else if (dvalue > 32767) {
		/* > 0x8000 */
		Temp_Value = (kal_int64) (dvalue - 65535);
		Temp_Value = Temp_Value - (Temp_Value * 2);
		g_fg_is_charging = KAL_FALSE;
	} else {
		Temp_Value = (kal_int64) dvalue;
		g_fg_is_charging = KAL_TRUE;
	}

	Temp_Value = Temp_Value * UNIT_FGCURRENT;
	do_div(Temp_Value, 100000);
	dvalue = (kal_uint32) Temp_Value;

	if (g_fg_is_charging == KAL_TRUE) {
		bm_print(BM_LOG_FULL, "[fgauge_read_current] current(charging) = %d mA\r\n",
			 dvalue);
	} else {
		bm_print(BM_LOG_FULL, "[fgauge_read_current] current(discharging) = %d mA\r\n",
			 dvalue);
	}

/* Auto adjust value */
	if (R_FG_VALUE != 20) {
		bm_print(BM_LOG_FULL,
			 "[fgauge_read_current] Auto adjust value due to the Rfg is %d\n Ori current=%d, ",
			 R_FG_VALUE, dvalue);

		dvalue = (dvalue * 20) / R_FG_VALUE;

		bm_print(BM_LOG_FULL, "[fgauge_read_current] new current=%d\n", dvalue);
	}
/* K current */
	if (R_FG_BOARD_SLOPE != R_FG_BOARD_BASE)
		dvalue = ((dvalue * R_FG_BOARD_BASE) + (R_FG_BOARD_SLOPE / 2)) / R_FG_BOARD_SLOPE;

/* current compensate */
	if (g_fg_is_charging == KAL_TRUE)
		dvalue = dvalue + Current_Compensate_Value;
	else
		dvalue = dvalue - Current_Compensate_Value;

	bm_print(BM_LOG_FULL, "[fgauge_read_current] ori current=%d\n", dvalue);

	dvalue = ((dvalue * CAR_TUNE_VALUE) / 100);

	dvalue = use_chip_trim_value(dvalue);

	bm_print(BM_LOG_FULL, "[fgauge_read_current] final current=%d (ratio=%d)\n", dvalue,
		 CAR_TUNE_VALUE);

	*(kal_int32 *) (data) = dvalue;

	return STATUS_OK;
}

static kal_int32 fgauge_read_current_sign(void *data)
{
	*(kal_bool *) (data) = g_fg_is_charging;

	return STATUS_OK;
}

static kal_int32 fgauge_read_columb_internal(void *data, int reset)
{
	kal_uint32 uvalue32_CAR = 0;
	kal_uint32 uvalue32_CAR_MSB = 0;
	kal_int32 dvalue_CAR = 0;
	int m = 0;
	int Temp_Value = 0;
	kal_uint32 ret = 0;

/* HW Init */
	/* (1)    i2c_write (0x60, 0xC8, 0x01); // Enable VA2 */
	/* (2)    i2c_write (0x61, 0x15, 0x00); // Enable FGADC clock for digital */
	/* (3)    i2c_write (0x61, 0x69, 0x28); // Set current mode, auto-calibration mode and 32KHz clock source */
	/* (4)    i2c_write (0x61, 0x69, 0x29); // Enable FGADC */

/* Read HW Raw Data */
	/* (1)    Set READ command */
	if (reset == 0)
		ret = pmic_config_interface(FGADC_CON0, 0x0200, 0xFF00, 0x0);
	else
		ret = pmic_config_interface(FGADC_CON0, 0x7300, 0xFF00, 0x0);

	/* (2)    Keep i2c read when status = 1 (0x06) */
	m = 0;
	while (fg_get_data_ready_status() == 0) {
		m++;
		if (m > 1000) {
			bm_print(BM_LOG_FULL,
				 "[fgauge_read_columb_internal] fg_get_data_ready_status timeout 1 !\r\n");
			break;
		}
	}
	/* (3)    Read FG_CURRENT_OUT[28:14] */
	/* (4)    Read FG_CURRENT_OUT[35] */
	uvalue32_CAR = (upmu_get_fg_car_15_00()) >> 14;
	uvalue32_CAR |= ((upmu_get_fg_car_31_16()) & 0x3FFF) << 2;

	uvalue32_CAR_MSB = (upmu_get_fg_car_35_32() & 0x0F) >> 3;

	bm_print(BM_LOG_FULL, "[fgauge_read_columb_internal] FG_CAR = 0x%x\r\n", uvalue32_CAR);
	bm_print(BM_LOG_FULL, "[fgauge_read_columb_internal] uvalue32_CAR_MSB = 0x%x\r\n",
		 uvalue32_CAR_MSB);

	/* (5)    (Read other data) */
	/* (6)    Clear status to 0 */
	ret = pmic_config_interface(FGADC_CON0, 0x0800, 0xFF00, 0x0);
	/* (7)    Keep i2c read when status = 0 (0x08) */
	/* while ( fg_get_sw_clear_status() != 0 ) */
	m = 0;
	while (fg_get_data_ready_status() != 0) {
		m++;
		if (m > 1000) {
			bm_print(BM_LOG_FULL,
				 "[fgauge_read_columb_internal] fg_get_data_ready_status timeout 2 !\r\n");
			break;
		}
	}
	/* (8)    Recover original settings */
	ret = pmic_config_interface(FGADC_CON0, 0x0000, 0xFF00, 0x0);

/* calculate the real world data */
	dvalue_CAR = (kal_int32) uvalue32_CAR;

	if (uvalue32_CAR == 0) {
		Temp_Value = 0;
	} else if (uvalue32_CAR == 65535) {
		/* 0xffff */
		Temp_Value = 0;
	} else if (uvalue32_CAR_MSB == 0x1) {
		/* dis-charging */
		Temp_Value = dvalue_CAR - 65535;	/* keep negative value */
	} else {
		/* charging */
		Temp_Value = (int)dvalue_CAR;
	}
	Temp_Value = (((Temp_Value * 35986) / 10) + (5)) / 10;	/* [28:14]'s LSB=359.86 uAh */
	dvalue_CAR = Temp_Value / 1000;	/* mAh */

	bm_print(BM_LOG_FULL, "[fgauge_read_columb_internal] dvalue_CAR = %d\r\n", dvalue_CAR);

	/* #if (OSR_SELECT_7 == 1) */
#if 0
	dvalue_CAR = dvalue_CAR * 8;
	if (Enable_FGADC_LOG == 1) {
		xlog_printk(ANDROID_LOG_DEBUG,
			    "[fgauge_read_columb_internal] : dvalue_CAR update to %d\r\n",
			    dvalue_CAR);
	}
#endif


/* Auto adjust value */
	if (R_FG_VALUE != 20) {
		bm_print(BM_LOG_FULL,
			 "[fgauge_read_columb_internal] Auto adjust value deu to the Rfg is %d\n Ori CAR=%d, ",
			 R_FG_VALUE, dvalue_CAR);

		dvalue_CAR = (dvalue_CAR * 20) / R_FG_VALUE;

		bm_print(BM_LOG_FULL, "[fgauge_read_columb_internal] new CAR=%d\n", dvalue_CAR);
	}

	dvalue_CAR = ((dvalue_CAR * CAR_TUNE_VALUE) / 100);

	dvalue_CAR = use_chip_trim_value(dvalue_CAR);

	bm_print(BM_LOG_FULL, "[fgauge_read_columb_internal] final dvalue_CAR = %d\r\n",
		 dvalue_CAR);

	if (Enable_FGADC_LOG >= BM_LOG_FULL) {
		dump_nter();
		dump_car();
	}

	*(kal_int32 *) (data) = dvalue_CAR;

	return STATUS_OK;
}

static kal_int32 fgauge_read_columb(void *data)
{
	return fgauge_read_columb_internal(data, 0);
}

static kal_int32 fgauge_hw_reset(void *data)
{
	volatile kal_uint32 val_car = 1;
	kal_uint32 val_car_temp = 1;
	kal_uint32 ret = 0;

	bm_print(BM_LOG_FULL, "[fgauge_hw_reset] : Start \r\n");

	while (val_car != 0x0) {
		ret = pmic_config_interface(FGADC_CON0, 0x7100, 0xFF00, 0x0);
		fgauge_read_columb_internal(&val_car_temp, 1);
		val_car = val_car_temp;
		bm_print(BM_LOG_FULL, "#");
	}

	bm_print(BM_LOG_FULL, "[fgauge_hw_reset] : End \r\n");

	return STATUS_OK;
}


static kal_int32 read_adc_v_bat_sense(void *data)
{
#if defined(CONFIG_POWER_EXT)
	*(kal_int32 *) (data) = 4201;
#else
	*(kal_int32 *) (data) =
	    PMIC_IMM_GetOneChannelValue(VBAT_CHANNEL_NUMBER, *(kal_int32 *) (data), 1);
#endif

	return STATUS_OK;
}



static kal_int32 read_adc_v_i_sense(void *data)
{
#if defined(CONFIG_POWER_EXT)
	*(kal_int32 *) (data) = 4202;
#else
	*(kal_int32 *) (data) =
	    PMIC_IMM_GetOneChannelValue(ISENSE_CHANNEL_NUMBER, *(kal_int32 *) (data), 1);
#endif

	return STATUS_OK;
}

static kal_int32 read_adc_v_bat_temp(void *data)
{
#if defined(CONFIG_POWER_EXT)
	*(kal_int32 *) (data) = 0;
#else
#if defined(MTK_PCB_TBAT_FEATURE)

	int ret = 0, data[4], i, ret_value = 0, ret_temp = 0;
	int Channel = 1;

	if (IMM_IsAdcInitReady() == 0) {
		bm_print(BM_LOG_CRTI, "[get_tbat_volt] AUXADC is not ready");
		return 0;
	}

	i = times;
	while (i--) {
		ret_value = IMM_GetOneChannelValue(Channel, data, &ret_temp);
		ret += ret_temp;
		bm_print(BM_LOG_FULL, "[get_tbat_volt] ret_temp=%d\n", ret_temp);
	}

	ret = ret * 1500 / 4096;
	ret = ret / times;
	bm_print(BM_LOG_CRTI, "[get_tbat_volt] Battery output mV = %d\n", ret);

	*(kal_int32 *) (data) = ret;

#else
	bm_print(BM_LOG_FULL,
		 "[read_adc_v_bat_temp] return PMIC_IMM_GetOneChannelValue(4,times,1);\n");
	*(kal_int32 *) (data) =
	    PMIC_IMM_GetOneChannelValue(VBATTEMP_CHANNEL_NUMBER, *(kal_int32 *) (data), 1);
#endif
#endif

	return STATUS_OK;
}

static kal_int32 read_adc_v_charger(void *data)
{
#if defined(CONFIG_POWER_EXT)
	*(kal_int32 *) (data) = 5001;
#else
	kal_int32 val;
	val = PMIC_IMM_GetOneChannelValue(VCHARGER_CHANNEL_NUMBER, *(kal_int32 *) (data), 1);
	val = val / 100;

	*(kal_int32 *) (data) = val;
#endif

	return STATUS_OK;
}

static kal_int32 read_hw_ocv(void *data)
{
#if defined(CONFIG_POWER_EXT)
	*(kal_int32 *) (data) = 3999;
#else
	if (upmu_get_cid() != PMIC6397_E1_CID_CODE) {
		*(kal_int32 *) (data) = get_hw_ocv();
	} else {
		/* HWOCV@E1 workaround */
		*(kal_int32 *) (data) = gFG_voltage;
	}
#endif

	return STATUS_OK;
}

static kal_int32 dump_register_fgadc(void *data)
{
	return STATUS_OK;
}

#ifdef CONFIG_EXT_FG_HAS_SOC
static kal_int32 ext_fg_get_soc(void *data)
{
#ifdef CONFIG_MTK_BQ27541_SUPPORT
	int ret = 0;
	int returnData = 0;

	ret = bq27541_set_cmd_read(BQ27541_CMD_StateOfCharge, &returnData);
	if (ret == 1) {
		bm_print(BM_LOG_CRTI, "[ext_fg_get_soc] bq27541 read raw data = %d\n", returnData);
		*(kal_int32 *) (data) = returnData;
		return STATUS_OK;
	} else {
		return STATUS_UNSUPPORTED;
	}
#endif

#ifdef MTK_BQ27741_SUPPORT
#endif

	return STATUS_OK;
}
#endif

#ifdef CONFIG_EXT_FG_HAS_TEMPERATURE
static kal_int32 ext_fg_get_temperature(void *data)
{
#ifdef CONFIG_MTK_BQ27541_SUPPORT
	int ret = 0;
	int returnData = 0;

	ret = bq27541_set_cmd_read(BQ27541_CMD_Temperature, &returnData);
	if (ret == 1) {
		bm_print(BM_LOG_CRTI, "[ext_fg_get_temperature] bq27541 read raw data = %d\n",
			 returnData);
		*(kal_int32 *) (data) = ((returnData / 10) - 273);
		return STATUS_OK;
	} else {
		return STATUS_UNSUPPORTED;
	}
#endif

#ifdef MTK_BQ27741_SUPPORT
#endif

	return STATUS_OK;
}
#endif

static kal_int32 ext_hw_fg(void *data)
{
#ifdef CONFIG_SOC_BY_EXT_HW_FG
	kal_int32 cmd = EXT_FG_CMD_NUMBER;

	/* make ext fg I2C init done */
	if (ext_fg_init_done == KAL_FALSE) {
		bm_print(BM_LOG_CRTI, "[ext_hw_fg] ext fg is not ready\n");
		return STATUS_UNSUPPORTED;
	}
	/* translate ext fg cmd */
	if (data) {
		cmd = *(kal_int32 *) (data);
	} else {
		bm_print(BM_LOG_CRTI, "[ext_hw_fg] NULL cmd!\n");
		return STATUS_UNSUPPORTED;
	}

	/* switch case to get battery information */
	switch (cmd) {
#ifdef CONFIG_EXT_FG_HAS_SOC
	case EXT_FG_CMD_SOC:
		return ext_fg_get_soc(data);
		break;
#endif

#ifdef CONFIG_EXT_FG_HAS_TEMPERATURE
	case EXT_FG_CMD_TEMPERATURE:
		return ext_fg_get_temperature(data);
		break;
#endif
	default:
		bm_print(BM_LOG_CRTI, "[ext_hw_fg] unsupported cmd(%d)!\n", cmd);
		return STATUS_UNSUPPORTED;
	}
#endif

	return STATUS_UNSUPPORTED;
}

static kal_int32(*const bm_func[BATTERY_METER_CMD_NUMBER]) (void *data) = {
	fgauge_initialization	/* hw fuel gague used only */
		, fgauge_read_current	/* hw fuel gague used only */
		, fgauge_read_current_sign	/* hw fuel gague used only */
		, fgauge_read_columb	/* hw fuel gague used only */
		, fgauge_hw_reset	/* hw fuel gague used only */
		, read_adc_v_bat_sense
		, read_adc_v_i_sense
		, read_adc_v_bat_temp
		, read_adc_v_charger
		, read_hw_ocv
		, dump_register_fgadc	/* hw fuel gague used only */
		, ext_hw_fg		/* ext hw fuel gauge used only */
};

kal_int32 bm_ctrl_cmd(BATTERY_METER_CTRL_CMD cmd, void *data)
{
	kal_int32 status;

	if ((cmd < BATTERY_METER_CMD_NUMBER) && (bm_func[cmd] != NULL))
		status = bm_func[cmd] (data);
	else
		return STATUS_UNSUPPORTED;

	return status;
}
