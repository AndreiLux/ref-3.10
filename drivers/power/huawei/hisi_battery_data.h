#ifndef __HISI_BATTERY_DATA
#define __HISI_BATTERY_DATA

#define MAX_SINGLE_LUT_COLS	20

#define PC_CC_ROWS             29
#define PC_CC_COLS             13

#define PC_TEMP_ROWS		29
#define PC_TEMP_COLS		8
#define TEMP_SAMPLING_POINTS    6

struct single_row_lut {
	int x[MAX_SINGLE_LUT_COLS];
	int y[MAX_SINGLE_LUT_COLS];
	int cols;
};

/**
 * struct sf_lut -
 * @rows:	number of percent charge entries should be <= PC_CC_ROWS
 * @cols:	number of charge cycle entries should be <= PC_CC_COLS
 * @row_entries:	the charge cycles/temperature at which sf data
 *			is available in the table.
 *		The charge cycles must be in increasing order from 0 to rows.
 * @percent:	the percent charge at which sf data is available in the table
 *		The  percentcharge must be in decreasing order from 0 to cols.
 * @sf:		the scaling factor data
 */
struct sf_lut {
	int rows;
	int cols;
	int row_entries[PC_CC_COLS];
	int percent[PC_CC_ROWS];
	int sf[PC_CC_ROWS][PC_CC_COLS];
};

/**
 * struct pc_temp_ocv_lut -
 * @rows:	number of percent charge entries should be <= PC_TEMP_ROWS
 * @cols:	number of temperature entries should be <= PC_TEMP_COLS
 * @temp:	the temperatures at which ocv data is available in the table
 *		The temperatures must be in increasing order from 0 to rows.
 * @percent:	the percent charge at which ocv data is available in the table
 *		The  percentcharge must be in decreasing order from 0 to cols.
 * @ocv:	the open circuit voltage
 */
struct pc_temp_ocv_lut {
	int rows;
	int cols;
	int temp[PC_TEMP_COLS];
	int percent[PC_TEMP_ROWS];
	int ocv[PC_TEMP_ROWS][PC_TEMP_COLS];
};

/**
 * struct hisi_hi6421v300_coul_battery_data -
 * @id_voltage_min: the min voltage on ID pin of this battery (mV)
 * @id_voltage_max: the max voltage on ID pin of this battery (mV)
 * @fcc:		full charge capacity (mAmpHour)
 * @fcc_temp_lut:	table to get fcc at a given temp
 * @pc_temp_ocv_lut:	table to get percent charge given batt temp and cycles
 * @pc_sf_lut:		table to get percent charge scaling factor given cycles
 *			and percent charge
 * @rbatt_sf_lut:	table to get battery resistance scaling factor given
 *			temperature and percent charge
 * @default_rbatt_mohm:	the default value of battery resistance to use when
 *			readings from bms are not available.
 * @delta_rbatt_mohm:	the resistance to be added towards lower soc to
 *			compensate for battery capacitance.
 */
struct hisi_hi6421v300_coul_battery_data {
    unsigned int    id_voltage_min;
    unsigned int    id_voltage_max;
    unsigned int    fcc;
    struct single_row_lut   *fcc_temp_lut;
    struct single_row_lut   *fcc_sf_lut;
    struct pc_temp_ocv_lut  *pc_temp_ocv_lut;
    struct sf_lut   *pc_sf_lut;
    struct sf_lut   *rbatt_sf_lut;
    int default_rbatt_mohm;
    int delta_rbatt_mohm;
    unsigned int      max_currentmA;
    unsigned int      max_voltagemV;
    unsigned int      max_cin_currentmA;
    unsigned int      terminal_currentmA;
    unsigned int      charge_in_temp_5;
    unsigned int      charge_in_temp_10;
    char *batt_brand;
};
struct hisi_hi6421v300_coul_battery_data *get_battery_data(unsigned int id);
#endif
