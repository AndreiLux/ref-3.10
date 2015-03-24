#ifndef _BATTERY_METER_HAL_H
#define _BATTERY_METER_HAL_H

#include <mach/mt_typedefs.h>

/* ============================================================ */
/* define */
/* ============================================================ */
#define BM_LOG_ERROR 0
#define BM_LOG_CRTI 1
#define BM_LOG_FULL 2

#define bm_print(num, fmt, args...)   \
do {									\
	if (Enable_FGADC_LOG >= (int)num) {				\
	if ((int)num == 0 || (Enable_FGADC_LOG == 1 && (int)num == 1) || Enable_FGADC_LOG == 2) \
	    xlog_printk(ANDROID_LOG_WARN, "Power/BatMeter", fmt, ##args); \
	else \
	    xlog_printk(ANDROID_LOG_INFO, "Power/BatMeter", fmt, ##args); \
	}								   \
} while (0)


/* ============================================================ */
/* ENUM */
/* ============================================================ */
typedef enum {
	EXT_FG_CMD_SOC,
	EXT_FG_CMD_TEMPERATURE,
	EXT_FG_CMD_NUMBER
} EXT_FG_CTRL_CMD;

typedef enum {
	BATTERY_METER_CMD_HW_FG_INIT,

	BATTERY_METER_CMD_GET_HW_FG_CURRENT,	/* fgauge_read_current */
	BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN,	/*  */
	BATTERY_METER_CMD_GET_HW_FG_CAR,	/* fgauge_read_columb */

	BATTERY_METER_CMD_HW_RESET,	/* FGADC_Reset_SW_Parameter */

	BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE,
	BATTERY_METER_CMD_GET_ADC_V_I_SENSE,
	BATTERY_METER_CMD_GET_ADC_V_BAT_TEMP,
	BATTERY_METER_CMD_GET_ADC_V_CHARGER,

	BATTERY_METER_CMD_GET_HW_OCV,

	BATTERY_METER_CMD_DUMP_REGISTER,
	BATTERY_METER_CMD_EXT_HW_FG,

	BATTERY_METER_CMD_NUMBER
} BATTERY_METER_CTRL_CMD;

/* ============================================================ */
/* structure */
/* ============================================================ */

/* ============================================================ */
/* typedef */
/* ============================================================ */
typedef kal_int32(*BATTERY_METER_CONTROL) (BATTERY_METER_CTRL_CMD cmd, void *data);

/* ============================================================ */
/* External Variables */
/* ============================================================ */
extern int Enable_FGADC_LOG;
extern kal_int32 gFG_voltage;	/* for HWOCV@E1 workaround */

/* ============================================================ */
/* External function */
/* ============================================================ */
extern int PMIC_IMM_GetOneChannelValue(int dwChannel, int deCount, int trimd);
extern kal_uint32 upmu_get_reg_value(kal_uint32 reg);
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
extern int IMM_IsAdcInitReady(void);
extern U32 pmic_config_interface(U32 RegNum, U32 val, U32 MASK, U32 SHIFT);
extern U32 pmic_read_interface(U32 RegNum, U32 *val, U32 MASK, U32 SHIFT);
extern kal_int32 bm_ctrl_cmd(BATTERY_METER_CTRL_CMD cmd, void *data);

#endif				/* #ifndef _BATTERY_METER_HAL_H */
