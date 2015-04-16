/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012 Synaptics Incorporated
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/ctype.h>
#include <linux/hrtimer.h>
#include "synaptics.h"
#include <../../huawei_touchscreen_chips.h>

#define WATCHDOG_TIMEOUT_S 2
#define FORCE_TIMEOUT_100MS 10
#define STATUS_WORK_INTERVAL 20 /* ms */

#define NUMS_OF_TX 32
#define NUMS_OF_RX 18

#define RX_NUMBER 89  //f01 control_base_addr + 57
#define TX_NUMBER 90  //f01 control_base_addr + 58

#define V6_RX_OFFSET 12
#define V6_TX_OFFSET 13
#define V6_F12_CTL_OFFSET 5
#define V6_F12_CTL_LEN 14
#define BOOTLOADER_ID_OFFSET 0

#define STATUS_IDLE 0
#define STATUS_BUSY 1

#define DATA_REPORT_INDEX_OFFSET 1
#define DATA_REPORT_DATA_OFFSET 3

#define COMMAND_GET_REPORT 1
#define COMMAND_FORCE_CAL 2
#define COMMAND_FORCE_UPDATE 4

#define CONTROL_42_SIZE 2
#define CONTROL_43_54_SIZE 13
#define CONTROL_55_56_SIZE 2

#define HIGH_RESISTANCE_DATA_SIZE 6
#define FULL_RAW_CAP_MIN_MAX_DATA_SIZE 4
#define TREX_DATA_SIZE 7

#define NO_AUTO_CAL_MASK 0x01
#define F54_BUF_LEN 50

static u8 tx = 0;
static u8 rx = 0;

#define MMI_BUF_SIZE ((NUMS_OF_TX)*(NUMS_OF_RX)*2)
#define MMIDATA_SIZE ((NUMS_OF_TX)*(NUMS_OF_RX)*2+2)
#define MMI_BUF_SIZE_EXT (100)

static char buf_f54test_result[F54_BUF_LEN] = {0};//store mmi test result
static char mmi_buf[MMI_BUF_SIZE] = {0};
static int rawdatabuf[MMIDATA_SIZE] = {0};
static int mmidata_size = 0;


static int synaptics_rmi4_f54_attention(void);


enum f54_report_types {
	F54_8BIT_IMAGE = 1,
	F54_16BIT_IMAGE = 2,
	F54_RAW_16BIT_IMAGE = 3,
	F54_HIGH_RESISTANCE = 4,
	F54_TX_TO_TX_SHORT = 5,
	F54_RX_TO_RX1 = 7,
	F54_TRUE_BASELINE = 9,
	F54_FULL_RAW_CAP_MIN_MAX = 13,
	F54_RX_OPENS1 = 14,
	F54_TX_OPEN = 15,
	F54_TX_TO_GROUND = 16,
	F54_RX_TO_RX2 = 17,
	F54_RX_OPENS2 = 18,
	F54_FULL_RAW_CAP = 19,
	F54_FULL_RAW_CAP_RX_COUPLING_COMP = 20,
	F54_SENSOR_SPEED = 22,
	F54_ADC_RANGE = 23,
	F54_TREX_OPENS = 24,
	F54_TREX_TO_GND = 25,
	F54_TREX_SHORTS = 26,
	INVALID_REPORT_TYPE = -1,
};

enum f54_rawdata_limit {
	RAW_DATA_UP = 0,
	RAW_DATA_LOW = 1,
	DELT_DATA_UP = 2,
	DELT_DATA_LOW = 3,
	RX_TO_RX_UP = 4,
	RX_TO_RX_LOW = 5,
	TX_TO_TX_UP = 6,
	TX_TO_TX_LOW = 7,
};

struct f54_query {
	union {
		struct {
			/* query 0 */
			unsigned char num_of_rx_electrodes;

			/* query 1 */
			unsigned char num_of_tx_electrodes;

			/* query 2 */
			unsigned char f54_query2_b0__1:2;
			unsigned char has_baseline:1;
			unsigned char has_image8:1;
			unsigned char f54_query2_b4__5:2;
			unsigned char has_image16:1;
			unsigned char f54_query2_b7:1;

			/* queries 3.0 and 3.1 */
			unsigned short clock_rate;

			/* query 4 */
			unsigned char touch_controller_family;

			/* query 5 */
			unsigned char has_pixel_touch_threshold_adjustment:1;
			unsigned char f54_query5_b1__7:7;

			/* query 6 */
			unsigned char has_sensor_assignment:1;
			unsigned char has_interference_metric:1;
			unsigned char has_sense_frequency_control:1;
			unsigned char has_firmware_noise_mitigation:1;
			unsigned char has_ctrl11:1;
			unsigned char has_two_byte_report_rate:1;
			unsigned char has_one_byte_report_rate:1;
			unsigned char has_relaxation_control:1;

			/* query 7 */
			unsigned char curve_compensation_mode:2;
			unsigned char f54_query7_b2__7:6;

			/* query 8 */
			unsigned char f54_query8_b0:1;
			unsigned char has_iir_filter:1;
			unsigned char has_cmn_removal:1;
			unsigned char has_cmn_maximum:1;
			unsigned char has_touch_hysteresis:1;
			unsigned char has_edge_compensation:1;
			unsigned char has_per_frequency_noise_control:1;
			unsigned char has_enhanced_stretch:1;

			/* query 9 */
			unsigned char has_force_fast_relaxation:1;
			unsigned char has_multi_metric_state_machine:1;
			unsigned char has_signal_clarity:1;
			unsigned char has_variance_metric:1;
			unsigned char has_0d_relaxation_control:1;
			unsigned char has_0d_acquisition_control:1;
			unsigned char has_status:1;
			unsigned char has_slew_metric:1;

			/* queries 10 11 */
			unsigned char f54_query10;
			unsigned char f54_query11;

			/* query 12 */
			unsigned char number_of_sensing_frequencies:4;
			unsigned char f54_query12_b4__7:4;
		} __packed;
		unsigned char data[14];
	};
};

struct f54_control_0 {
	union {
		struct {
			unsigned char no_relax:1;
			unsigned char no_scan:1;
			unsigned char force_fast_relaxation:1;
			unsigned char startup_fast_relaxation:1;
			unsigned char gesture_cancels_sfr:1;
			unsigned char enable_energy_ratio_relaxation:1;
			unsigned char excessive_noise_attn_enable:1;
			unsigned char f54_control0_b7:1;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_1 {
	union {
		struct {
			unsigned char bursts_per_cluster:4;
			unsigned char f54_ctrl1_b4__7:4;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_2 {
	union {
		struct {
			unsigned short saturation_cap;
		} __packed;
		struct {
			unsigned char data[2];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_3 {
	union {
		struct {
			unsigned char pixel_touch_threshold;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_4__6 {
	union {
		struct {
			/* control 4 */
			unsigned char rx_feedback_cap:2;
			unsigned char bias_current:2;
			unsigned char f54_ctrl4_b4__7:4;

			/* control 5 */
			unsigned char low_ref_cap:2;
			unsigned char low_ref_feedback_cap:2;
			unsigned char low_ref_polarity:1;
			unsigned char f54_ctrl5_b5__7:3;

			/* control 6 */
			unsigned char high_ref_cap:2;
			unsigned char high_ref_feedback_cap:2;
			unsigned char high_ref_polarity:1;
			unsigned char f54_ctrl6_b5__7:3;
		} __packed;
		struct {
			unsigned char data[3];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_7 {
	union {
		struct {
			unsigned char cbc_cap:3;
			unsigned char cbc_polarity:1;
			unsigned char cbc_tx_carrier_selection:1;
			unsigned char f54_ctrl7_b5__7:3;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_8__9 {
	union {
		struct {
			/* control 8 */
			unsigned short integration_duration:10;
			unsigned short f54_ctrl8_b10__15:6;

			/* control 9 */
			unsigned char reset_duration;
		} __packed;
		struct {
			unsigned char data[3];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_10 {
	union {
		struct {
			unsigned char noise_sensing_bursts_per_image:4;
			unsigned char f54_ctrl10_b4__7:4;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_11 {
	union {
		struct {
			unsigned short f54_ctrl11;
		} __packed;
		struct {
			unsigned char data[2];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_12__13 {
	union {
		struct {
			/* control 12 */
			unsigned char slow_relaxation_rate;

			/* control 13 */
			unsigned char fast_relaxation_rate;
		} __packed;
		struct {
			unsigned char data[2];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_14 {
	union {
		struct {
				unsigned char rxs_on_xaxis:1;
				unsigned char curve_comp_on_txs:1;
				unsigned char f54_ctrl14_b2__7:6;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_15n {
	unsigned char sensor_rx_assignment;
};

struct f54_control_15 {
	struct f54_control_15n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_16n {
	unsigned char sensor_tx_assignment;
};

struct f54_control_16 {
	struct f54_control_16n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_17n {
	unsigned char burst_count_b8__10:3;
	unsigned char disable:1;
	unsigned char f54_ctrl17_b4:1;
	unsigned char filter_bandwidth:3;
};

struct f54_control_17 {
	struct f54_control_17n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_18n {
	unsigned char burst_count_b0__7;
};

struct f54_control_18 {
	struct f54_control_18n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_19n {
	unsigned char stretch_duration;
};

struct f54_control_19 {
	struct f54_control_19n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_20 {
	union {
		struct {
			unsigned char disable_noise_mitigation:1;
			unsigned char f54_ctrl20_b1__7:7;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_21 {
	union {
		struct {
			unsigned short freq_shift_noise_threshold;
		} __packed;
		struct {
			unsigned char data[2];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_22__26 {
	union {
		struct {
			/* control 22 */
			unsigned char f54_ctrl22;

			/* control 23 */
			unsigned short medium_noise_threshold;

			/* control 24 */
			unsigned short high_noise_threshold;

			/* control 25 */
			unsigned char noise_density;

			/* control 26 */
			unsigned char frame_count;
		} __packed;
		struct {
			unsigned char data[7];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_27 {
	union {
		struct {
			unsigned char iir_filter_coef;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_28 {
	union {
		struct {
			unsigned short quiet_threshold;
		} __packed;
		struct {
			unsigned char data[2];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_29 {
	union {
		struct {
			/* control 29 */
			unsigned char f54_ctrl29_b0__6:7;
			unsigned char cmn_filter_disable:1;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_30 {
	union {
		struct {
			unsigned char cmn_filter_max;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_31 {
	union {
		struct {
			unsigned char touch_hysteresis;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_32__35 {
	union {
		struct {
			/* control 32 */
			unsigned short rx_low_edge_comp;

			/* control 33 */
			unsigned short rx_high_edge_comp;

			/* control 34 */
			unsigned short tx_low_edge_comp;

			/* control 35 */
			unsigned short tx_high_edge_comp;
		} __packed;
		struct {
			unsigned char data[8];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_36n {
	unsigned char axis1_comp;
};

struct f54_control_36 {
	struct f54_control_36n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_37n {
	unsigned char axis2_comp;
};

struct f54_control_37 {
	struct f54_control_37n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_38n {
	unsigned char noise_control_1;
};

struct f54_control_38 {
	struct f54_control_38n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_39n {
	unsigned char noise_control_2;
};

struct f54_control_39 {
	struct f54_control_39n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_40n {
	unsigned char noise_control_3;
};

struct f54_control_40 {
	struct f54_control_40n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_41 {
	union {
		struct {
			unsigned char no_signal_clarity:1;
			unsigned char f54_ctrl41_b1__7:7;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_57 {
	union {
		struct {
			unsigned char cbc_cap_0d:3;
			unsigned char cbc_polarity_0d:1;
			unsigned char cbc_tx_carrier_selection_0d:1;
			unsigned char f54_ctrl57_b5__7:3;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control {
	struct f54_control_0 *reg_0;
	struct f54_control_1 *reg_1;
	struct f54_control_2 *reg_2;
	struct f54_control_3 *reg_3;
	struct f54_control_4__6 *reg_4__6;
	struct f54_control_7 *reg_7;
	struct f54_control_8__9 *reg_8__9;
	struct f54_control_10 *reg_10;
	struct f54_control_11 *reg_11;
	struct f54_control_12__13 *reg_12__13;
	struct f54_control_14 *reg_14;
	struct f54_control_15 *reg_15;
	struct f54_control_16 *reg_16;
	struct f54_control_17 *reg_17;
	struct f54_control_18 *reg_18;
	struct f54_control_19 *reg_19;
	struct f54_control_20 *reg_20;
	struct f54_control_21 *reg_21;
	struct f54_control_22__26 *reg_22__26;
	struct f54_control_27 *reg_27;
	struct f54_control_28 *reg_28;
	struct f54_control_29 *reg_29;
	struct f54_control_30 *reg_30;
	struct f54_control_31 *reg_31;
	struct f54_control_32__35 *reg_32__35;
	struct f54_control_36 *reg_36;
	struct f54_control_37 *reg_37;
	struct f54_control_38 *reg_38;
	struct f54_control_39 *reg_39;
	struct f54_control_40 *reg_40;
	struct f54_control_41 *reg_41;
	struct f54_control_57 *reg_57;
};

struct f55_query {
	union {
		struct {
			/* query 0 */
			unsigned char num_of_rx_electrodes;

			/* query 1 */
			unsigned char num_of_tx_electrodes;

			/* query 2 */
			unsigned char has_sensor_assignment:1;
			unsigned char has_edge_compensation:1;
			unsigned char curve_compensation_mode:2;
			unsigned char reserved:4;
		} __packed;
		unsigned char data[3];
	};
};


struct synaptics_rmi4_fn55_desc {
	unsigned short query_base_addr;
	unsigned short control_base_addr;
};
struct synaptics_fn54_statics_data {
	short RawimageAverage;
	short RawimageMaxNum;
	short RawimageMinNum;
	short RawimageNULL;
};
enum bl_version {
	V5 = 5,
	V6 = 6,
};

struct synaptics_rmi4_f54_handle {
	bool no_auto_cal;
	unsigned char status;
	unsigned char intr_mask;
	unsigned char intr_reg_num;
	unsigned char *report_data;
	unsigned char bootloader_id[2];
	unsigned short query_base_addr;
	unsigned short control_base_addr;
	unsigned short data_base_addr;
	unsigned short command_base_addr;
	unsigned short fifoindex;
	unsigned int report_size;
	unsigned int data_buffer_size;

	enum bl_version bl_version;
	enum f54_report_types report_type;
	
	struct f54_query query;
	struct f54_control control;
	struct kobject *attr_dir;
	struct synaptics_fn54_statics_data raw_statics_data;
	struct synaptics_fn54_statics_data delta_statics_data;
	struct synaptics_rmi4_exp_fn_ptr *fn_ptr;
	struct synaptics_rmi4_data *rmi4_data;
	struct synaptics_rmi4_fn55_desc *fn55;
	struct synaptics_rmi4_fn_desc f34_fd;
};


static struct synaptics_rmi4_f54_handle *f54;

DECLARE_COMPLETION(f54_remove_complete);

static void set_report_size(void)
{
	int rc = 0;
	switch (f54->report_type) {
	case F54_8BIT_IMAGE:
		f54->report_size = rx * tx;
		break;
	case F54_16BIT_IMAGE:
	case F54_RAW_16BIT_IMAGE:
	case F54_TRUE_BASELINE:
	case F54_FULL_RAW_CAP:
	case F54_FULL_RAW_CAP_RX_COUPLING_COMP:
	case F54_SENSOR_SPEED:
		f54->report_size = 2 * rx * tx;
		break;
	case F54_HIGH_RESISTANCE:
		f54->report_size = HIGH_RESISTANCE_DATA_SIZE;
		break;
	case F54_TX_TO_TX_SHORT:
	case F54_TX_OPEN:
		f54->report_size =  (tx + 7) / 8;
		break;
	case F54_TX_TO_GROUND:
		f54->report_size = 3;
		break;
	case F54_RX_TO_RX1:
	//edw
		if (rx < tx)
			f54->report_size = 2 * rx * rx;
		else
			f54->report_size = 2 * rx * tx;
		break;
	//edw
	case F54_RX_OPENS1:
		if (rx < tx)
			f54->report_size = 2 * rx * rx;
		else
			f54->report_size = 2 * rx * tx;
		break;
	case F54_FULL_RAW_CAP_MIN_MAX:
		f54->report_size = FULL_RAW_CAP_MIN_MAX_DATA_SIZE;
		break;
	case F54_RX_TO_RX2:
	case F54_RX_OPENS2:
		if (rx <= tx)
			f54->report_size = 0;
		else
			f54->report_size = 2 * rx * (rx - tx);
		break;
	case F54_ADC_RANGE:
		if (f54->query.has_signal_clarity) {
			rc = f54->fn_ptr->read(f54->rmi4_data,
					f54->control.reg_41->address,
					f54->control.reg_41->data,
					sizeof(f54->control.reg_41->data));
			if (rc < 0) {
				TS_LOG_INFO("Failed to read control reg_41\n");
				f54->report_size = 0;
				break;
			}
			if (!f54->control.reg_41->no_signal_clarity) {
				if (tx % 4)
					tx += 4 - (tx % 4);
			}
		}
		f54->report_size = 2 * rx * tx;
		break;
	case F54_TREX_OPENS:
	case F54_TREX_TO_GND:
	case F54_TREX_SHORTS:
		f54->report_size = TREX_DATA_SIZE;
		break;
	default:
		f54->report_size = 0;
	}

	return;
}


/* when the report type is  3 or 9, we call this function to  to find open
* transmitter electrodes, open receiver electrodes, transmitter-to-ground
* shorts, receiver-to-ground shorts, and transmitter-to-receiver shorts. */
static int f54_rawimage_report (void)
{
	short Rawimage;
	short Result = 0;

	int i;
	int raw_cap_uplimit = f54->rmi4_data->synaptics_chip_data->raw_limit_buf[RAW_DATA_UP];
	int raw_cap_lowlimit = f54->rmi4_data->synaptics_chip_data->raw_limit_buf[RAW_DATA_LOW];
	long int RawimageSum = 0;
	short RawimageAverage = 0;
	short RawimageMaxNum,RawimageMinNum;

	TS_LOG_INFO("f54_rawimage_report: rx = %d, tx = %d, mmidata_size =%d, raw_cap_uplimit = %d,raw_cap_lowlimit = %d\n", rx, tx, mmidata_size, raw_cap_uplimit, raw_cap_lowlimit);

	RawimageMaxNum = (mmi_buf[0]) | (mmi_buf[1] << 8);
	RawimageMinNum = (mmi_buf[0]) | (mmi_buf[1] << 8);
	for ( i = 0; i < mmidata_size; i+=2)
	{
		Rawimage = (mmi_buf[i]) | (mmi_buf[i+1] << 8);
		RawimageSum += Rawimage;
		if(RawimageMaxNum < Rawimage)
			RawimageMaxNum = Rawimage;
		if(RawimageMinNum > Rawimage)
			RawimageMinNum = Rawimage;
	}
	RawimageAverage = RawimageSum/(mmidata_size/2);
	f54->raw_statics_data.RawimageAverage = RawimageAverage;
	f54->raw_statics_data.RawimageMaxNum = RawimageMaxNum;
	f54->raw_statics_data.RawimageMinNum = RawimageMinNum;

	for ( i = 0; i < mmidata_size; i+=2)
	{
		Rawimage = (mmi_buf[i]) | (mmi_buf[i+1] << 8);
		if ((Rawimage > raw_cap_lowlimit)&& (Rawimage < raw_cap_uplimit))
		{
			Result++;
		}
	}
	if (Result == (mmidata_size/2))
		return 1;
	else
		return 0;
}

static int write_to_f54_register(unsigned char report_type)
{
	unsigned char command;
	int result;

	TS_LOG_INFO("report type = %d\n", report_type);

	command = report_type;
	/*set report type*/
	if (F54_RX_TO_RX1 != command)
	{
		result = f54->fn_ptr->write(f54->rmi4_data, f54->data_base_addr, &command, 1);
		if (result < 0){
		       TS_LOG_ERR("Could not write report type to 0x%x\n", f54->data_base_addr);
			return result;
		}
	}
	mdelay(5);

	/*set get_report to 1*/
	command = (unsigned char)COMMAND_GET_REPORT;
	result = f54->fn_ptr->write(f54->rmi4_data, f54->command_base_addr, &command, 1);
	msleep(50);

	return result;
}

static int mmi_add_static_data(void)
{
	int i;

	i = strlen(buf_f54test_result);
	if (i >= F54_BUF_LEN) {
		return -EINVAL;
	}
	snprintf((buf_f54test_result+i), F54_BUF_LEN-i,
		"[%4d,%4d,%4d]",
		f54->raw_statics_data.RawimageAverage,
		f54->raw_statics_data.RawimageMaxNum,
		f54->raw_statics_data.RawimageMinNum);

	i = strlen(buf_f54test_result);
	if (i >= F54_BUF_LEN) {
		return -EINVAL;
	}
	snprintf((buf_f54test_result+i), F54_BUF_LEN-i,
		"[%4d,%4d,%4d]",
		f54->delta_statics_data.RawimageAverage,
		f54->delta_statics_data.RawimageMaxNum,
		f54->delta_statics_data.RawimageMinNum);

        return 0;
}

static int f54_deltarawimage_report (void)
{
	short Rawimage;
	int i;
	int delt_cap_uplimit = f54->rmi4_data->synaptics_chip_data->raw_limit_buf[DELT_DATA_UP];
	int delt_cap_lowlimit = f54->rmi4_data->synaptics_chip_data->raw_limit_buf[DELT_DATA_LOW];
	long int DeltaRawimageSum = 0;
	short DeltaRawimageAverage = 0;
	short DeltaRawimageMaxNum,DeltaRawimageMinNum;
	short result = 0;
	
	TS_LOG_INFO("f54_deltarawimage_report enter, delt_cap_uplimit = %d, delt_cap_lowlimit = %d\n", delt_cap_uplimit, delt_cap_lowlimit);

	DeltaRawimageMaxNum = (mmi_buf[0]) | (mmi_buf[1] << 8);
	DeltaRawimageMinNum = (mmi_buf[0]) | (mmi_buf[1] << 8);
	for ( i = 0; i < mmidata_size; i+=2)
	{
		Rawimage = (mmi_buf[i]) | (mmi_buf[i+1] << 8);
		DeltaRawimageSum += Rawimage;
		if(DeltaRawimageMaxNum < Rawimage)
			DeltaRawimageMaxNum = Rawimage;
		if(DeltaRawimageMinNum > Rawimage)
			DeltaRawimageMinNum = Rawimage;
	}
	DeltaRawimageAverage = DeltaRawimageSum/(mmidata_size/2);
	f54->delta_statics_data.RawimageAverage = DeltaRawimageAverage;
	f54->delta_statics_data.RawimageMaxNum = DeltaRawimageMaxNum;
	f54->delta_statics_data.RawimageMinNum = DeltaRawimageMinNum;

	for ( i = 0; i < mmidata_size; i+=2)
	{
		Rawimage = (mmi_buf[i]) | (mmi_buf[i+1] << 8);
		if ((Rawimage > delt_cap_lowlimit) && (Rawimage < delt_cap_uplimit)) {
			result++;
		}
	}
	if (result == (mmidata_size/2)) {
		return 1;
	} else {
		return 0;
	}

}

static void mmi_deltacapacitance_test(void)
{
	unsigned char command;
	int result = 0;
	command = (unsigned char) F54_16BIT_IMAGE;
	TS_LOG_INFO("mmi_deltacapacitance_test called\n");
	write_to_f54_register(command);
	f54->report_type = command;
	result = synaptics_rmi4_f54_attention();
	if(result < 0){
		TS_LOG_ERR("Failed to get data\n");
		strncat(buf_f54test_result, "-3F", MAX_STR_LEN);
		return;
	}
	result = f54_deltarawimage_report();
	if(1 == result) {
		strncat(buf_f54test_result, "-3P", MAX_STR_LEN);
	} else {
		strncat(buf_f54test_result, "-3F", MAX_STR_LEN);
	}
	return;
}

static int f54_delta_rx_report (void)
{
	short Rawimage_rx;
	short Rawimage_rx1;
	short Rawimage_rx2;
	short Result = 0;
	int i = 0;
	int j = 0;
	int delt_cap_uplimit = f54->rmi4_data->synaptics_chip_data->raw_limit_buf[RX_TO_RX_UP];
	int delt_cap_lowlimit = f54->rmi4_data->synaptics_chip_data->raw_limit_buf[RX_TO_RX_LOW];

	TS_LOG_INFO("rx = %d, tx = %d, delt_cap_uplimit = %d, delt_cap_lowlimit = %d\n",rx, tx, delt_cap_uplimit, delt_cap_lowlimit);

	for ( i = 0; i < mmidata_size; i+=2)
	{
		Rawimage_rx1 = (mmi_buf[i]) | (mmi_buf[i+1] << 8);
		Rawimage_rx2 = (mmi_buf[i+2]) | (mmi_buf[i+3] << 8);
		Rawimage_rx = Rawimage_rx2 - Rawimage_rx1;
		j++;
		if (j == rx-1) {
			i+=2;
			j = 0;
		}
		if ((Rawimage_rx < delt_cap_uplimit) && (Rawimage_rx > delt_cap_lowlimit))
			Result++;
	}
	if(Result == (mmidata_size/2 - tx))
		return 1;
	else
		return 0;

}
static int f54_delta_tx_report (void)
{
	short *Rawimage_tx = NULL;
	short Rawimage_delta_tx;
	int i,j,tx_n,rx_n;
	int Result=0;
	int tx_to_tx_cap_uplimit = f54->rmi4_data->synaptics_chip_data->raw_limit_buf[TX_TO_TX_UP];
	int tx_to_tx_cap_lowlimit = f54->rmi4_data->synaptics_chip_data->raw_limit_buf[TX_TO_TX_LOW];

	TS_LOG_INFO("rx = %d, tx = %d, tx_to_tx_cap_uplimit = %d, tx_to_tx_cap_lowlimit = %d\n",rx, tx, tx_to_tx_cap_uplimit, tx_to_tx_cap_lowlimit);

	Rawimage_tx = (short *)kzalloc((MMI_BUF_SIZE + MMI_BUF_SIZE_EXT), GFP_KERNEL);
	if (!Rawimage_tx) {
		TS_LOG_ERR("Rawimage_tx kzalloc error\n");
		return 0;
	}

	for ( i = 0, j = 0; i < mmidata_size; i+=2, j++)
		Rawimage_tx[j] = (mmi_buf[i]) | (mmi_buf[i+1] << 8);
	for( tx_n = 0; tx_n < tx; tx_n++)
	{
		for(rx_n = 0; rx_n < rx; rx_n++)
		{
			Rawimage_delta_tx = Rawimage_tx[(tx_n+1)*rx+rx_n] - Rawimage_tx[tx_n*rx+rx_n];
			if((Rawimage_delta_tx < tx_to_tx_cap_uplimit)&&(Rawimage_delta_tx> tx_to_tx_cap_lowlimit))
				Result++;
		}
	}
	kfree(Rawimage_tx);

	if(Result == (mmidata_size/2 -rx))
		return 1;
	else
		return 0;
}

static void mmi_rawcapacitance_test(void)
{
	unsigned char command;
	int result = 0;

	command = (unsigned char) F54_RAW_16BIT_IMAGE;

	TS_LOG_INFO("mmi_rawcapacitance_test called\n");

	write_to_f54_register(command);
	f54->report_type = command;
	result = synaptics_rmi4_f54_attention();
	if(result < 0){
		TS_LOG_ERR("Failed to get data\n");
		strncat(buf_f54test_result, "1F", MAX_STR_LEN);
		return;
	}
	result = f54_rawimage_report();
	if (1 == result) {
		strncat(buf_f54test_result, "1P", MAX_STR_LEN);
	} else {
		strncat(buf_f54test_result, "1F", MAX_STR_LEN);
	}
	if (1 == (f54_delta_rx_report() && f54_delta_tx_report())) {
		strncat(buf_f54test_result, "-2P", MAX_STR_LEN);
	} else {
		strncat(buf_f54test_result, "-2F", MAX_STR_LEN);
	}
	return;
}

static int synaptics_f54_malloc(void)
{
	f54 = kzalloc(sizeof(struct synaptics_rmi4_f54_handle), GFP_KERNEL);
	if (!f54) {
		TS_LOG_ERR("Failed to alloc mem for f54\n");
		return -ENOMEM;
	}

	f54->fn_ptr = kzalloc(sizeof(struct synaptics_rmi4_exp_fn_ptr), GFP_KERNEL);
	if (!f54->fn_ptr) {
		TS_LOG_ERR("Failed to alloc mem for fn_ptr\n");
		return -ENOMEM;
	}

	f54->fn55 = kzalloc(sizeof(struct synaptics_rmi4_fn55_desc), GFP_KERNEL);
	if (!f54->fn55) {
		TS_LOG_ERR("Failed to alloc mem for fn55\n");
		return -ENOMEM;
	}

	return NO_ERR;
}

static void synaptics_f54_free(void)
{
	if (f54 && f54->fn_ptr)
		kfree(f54->fn_ptr);
	if (f54 && f54->fn55)
		kfree(f54->fn55);
	if (f54) {
		kfree(f54);
		f54 = NULL;
	}
}

static void put_capacitance_data (int index)
{
	int i , j;
	short temp;
	rawdatabuf[0] = rx;
	rawdatabuf[1] = tx;
	for(i = 0, j = index + 2; i < mmidata_size; i+=2, j++) {
		temp = (mmi_buf[i]) | (mmi_buf[i+1] << 8);
		rawdatabuf[j] = temp;
	}
}

int synaptics_get_cap_data(struct ts_rawdata_info *info)
{
	int rc = NO_ERR;
	unsigned char command;

	TS_LOG_INFO("synaptics_get_cap_data called\n");

	memset(buf_f54test_result, 0, sizeof(buf_f54test_result));
	memset(rawdatabuf, 0, sizeof(rawdatabuf));

	rc = f54->fn_ptr->read(f54->rmi4_data, f54->data_base_addr, &command, 1);
	if (rc < 0) {		/*i2c communication failed, mmi result is all failed*/
		memcpy(buf_f54test_result, "0F-1F-2F", (strlen("0F-1F-2F")+1));
	} else {
		memcpy(buf_f54test_result, "0P-", (strlen("0P-")+1));
		mmi_rawcapacitance_test();		/*report type == 3*/
		put_capacitance_data(0);
		mmi_deltacapacitance_test();		/*report type == 2*/
		put_capacitance_data(mmidata_size/2);
		mmi_add_static_data();
	}

	rc = f54->rmi4_data->reset_device(f54->rmi4_data);
	if (rc < 0) {
		TS_LOG_ERR("failed to write command to f01 reset!\n");
		goto exit;
	}
	memcpy(info->buff, rawdatabuf, sizeof(rawdatabuf));
	memcpy(info->result, buf_f54test_result, strlen(buf_f54test_result));
	info->used_size = sizeof(rawdatabuf)/sizeof(int);
	TS_LOG_INFO("info->used_size = %d\n",info->used_size);
	rc = NO_ERR;
exit:
	synaptics_f54_free();
	return rc;
}

static int synaptics_rmi4_f54_attention(void)
{
	int retval;
	int l;
	unsigned char report_index[2];
	unsigned char f12_2d_data[14]={0};

	if (V5 == f54->bl_version) {
		TS_LOG_INFO("version V5\n");
		/*get tx and rx value by read register from F11_2D_CTRL77 and F11_2D_CTRL78 */
		retval = f54->fn_ptr->read(f54->rmi4_data, f54->rmi4_data->f01_ctrl_base_addr+RX_NUMBER, &rx, 1);
		if (retval < 0){
			TS_LOG_ERR("Could not read RX value from 0x%04x\n", f54->rmi4_data->f01_ctrl_base_addr + RX_NUMBER);
			goto error_exit;
		}

		retval = f54->fn_ptr->read(f54->rmi4_data, f54->rmi4_data->f01_ctrl_base_addr + TX_NUMBER, &tx, 1);
		if (retval < 0){
			TS_LOG_ERR("Could not read TX value from 0x%04x\n", f54->rmi4_data->f01_ctrl_base_addr + TX_NUMBER);
			goto error_exit;
		}
	} else if (V6 == f54->bl_version) {
		TS_LOG_INFO("version V6\n");
		retval = f54->fn_ptr->read(f54->rmi4_data, f54->rmi4_data->f01_ctrl_base_addr+V6_F12_CTL_OFFSET, f12_2d_data, V6_F12_CTL_LEN);
		if (retval < 0){
			TS_LOG_ERR("Could not read RX value from 0x%04x\n", f54->rmi4_data->f01_ctrl_base_addr + V6_F12_CTL_OFFSET);
			goto error_exit;
		}
		rx=f12_2d_data[V6_RX_OFFSET];
		tx=f12_2d_data[V6_TX_OFFSET];
	} else {
		TS_LOG_ERR("unsuport version\n");
		retval = -EINVAL;
		goto error_exit;
	}

	set_report_size();

	if (f54->report_size == 0) {
		TS_LOG_ERR("Report data size = 0\n");
		retval = -EINVAL;
		goto error_exit;
	}

	if (f54->data_buffer_size < f54->report_size){
		if (f54->data_buffer_size)
			kfree(f54->report_data);
		f54->report_data = kzalloc(f54->report_size, GFP_KERNEL);
		if (!f54->report_data) {
			TS_LOG_ERR("Failed to alloc mem for data buffer\n");
			f54->data_buffer_size = 0;
			retval = -ENOMEM;
			goto error_exit;
		}
		f54->data_buffer_size = f54->report_size;
	}

	report_index[0] = 0;
	report_index[1] = 0;

	retval = f54->fn_ptr->write(f54->rmi4_data,
			f54->data_base_addr + DATA_REPORT_INDEX_OFFSET,
			report_index,
			sizeof(report_index));
	if (retval < 0) {
		TS_LOG_ERR("Failed to write report data index\n");
		retval = -EINVAL;
		goto error_exit;
	}

	retval = f54->fn_ptr->read(f54->rmi4_data,
			f54->data_base_addr + DATA_REPORT_DATA_OFFSET,
			f54->report_data,
			f54->report_size);
	if (retval < 0) {
		TS_LOG_ERR("Failed to read report data\n");
		retval = -EINVAL;
		goto error_exit;
	}

	/*get report data, one data contains two bytes*/
	mmidata_size = f54->report_size;

	for (l = 0; l < f54->report_size; l += 2)
	{
		mmi_buf[l] = f54->report_data[l];
		mmi_buf[l+1] = f54->report_data[l+1];
	}

	retval = NO_ERR;

error_exit:
	return retval;
}

static int synaptics_read_f34(void)
{
	int retval = NO_ERR;

	retval = f54->fn_ptr->read(f54->rmi4_data,
		f54->f34_fd.query_base_addr + BOOTLOADER_ID_OFFSET,
		f54->bootloader_id,
		sizeof(f54->bootloader_id));
	if (retval < 0) {
		TS_LOG_ERR("Failed to read bootloader ID\n");
		return retval;
	}

	TS_LOG_INFO("bootloader_id[1] = %c", f54->bootloader_id[1]);

	if (f54->bootloader_id[1] == '5') {
		f54->bl_version = V5;
	} else if (f54->bootloader_id[1] == '6') {
		f54->bl_version = V6;
	} else {
		TS_LOG_ERR("Unrecognized bootloader version\n");
		return -EINVAL;
	}

	TS_LOG_INFO("bl_version = %d\n", f54->bl_version);
	return NO_ERR;
}

int synaptics_rmi4_f54_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval = -EINVAL;
	bool hasF54 = false;
	bool hasF55 = false;
	bool hasF34 = false;
	unsigned short ii;
	unsigned char page;
	unsigned char intr_count = 0;
	struct synaptics_rmi4_fn_desc rmi_fd;

	if (synaptics_f54_malloc() != NO_ERR)
		goto exit_free_mem;

	f54->rmi4_data = rmi4_data;
	f54->fn_ptr->read = rmi4_data->i2c_read;
	f54->fn_ptr->write = rmi4_data->i2c_write;

	for (page = 0; page < PAGES_TO_SERVICE; page++) {
		for (ii = PDT_START; ii > PDT_END; ii -= PDT_ENTRY_SIZE) {
			ii |= (page << 8);

			retval = f54->fn_ptr->read(rmi4_data, ii, (unsigned char *)&rmi_fd, sizeof(rmi_fd));
			if (retval < 0) {
				TS_LOG_ERR("i2c read error, page = %d, ii = %d\n", page, ii);
				goto exit_free_mem;
			}

			if (!rmi_fd.fn_number) {
				TS_LOG_INFO("!rmi_fd.fn_number,page=%d,ii=%d\n",page,ii);
				retval = -EINVAL;
				break;
			}

			if (rmi_fd.fn_number == SYNAPTICS_RMI4_F54) {
				hasF54 = true;
				f54->query_base_addr =
					rmi_fd.query_base_addr | (page << 8);
				f54->control_base_addr =
					rmi_fd.ctrl_base_addr | (page << 8);
				f54->data_base_addr =
					rmi_fd.data_base_addr | (page << 8);
				f54->command_base_addr =
					rmi_fd.cmd_base_addr | (page << 8);
			} else if (rmi_fd.fn_number == SYNAPTICS_RMI4_F55) {
				hasF55 = true;
				f54->fn55->query_base_addr = rmi_fd.query_base_addr | (page << 8);
				f54->fn55->control_base_addr = rmi_fd.ctrl_base_addr | (page << 8);
			} else if (rmi_fd.fn_number == SYNAPTICS_RMI4_F34) {
				hasF34 = true;
				f54->f34_fd.query_base_addr = rmi_fd.query_base_addr;
				f54->f34_fd.ctrl_base_addr = rmi_fd.ctrl_base_addr;
				f54->f34_fd.data_base_addr = 	rmi_fd.data_base_addr;
			}

			if (hasF54 && hasF55 && hasF34)
				goto found;

			if (!hasF54)
				intr_count += (rmi_fd.intr_src_count & MASK_3BIT);
		}
	}

	TS_LOG_INFO("f54->control_base_addr = 0x%x, f54->data_base_addr = 0x%x\n", f54->control_base_addr, f54->data_base_addr);

	if (!hasF54 || !hasF34) {
		TS_LOG_ERR("Funtion  is not available, hasF54=%d, hasF34 = %d\n", hasF54, hasF34);
		retval = -EINVAL;
		goto exit_free_mem;
	}

found:
#if 0
	f54->intr_reg_num = (intr_count + 7) / 8;
	if (f54->intr_reg_num != 0)
		f54->intr_reg_num -= 1;

	f54->intr_mask = 0;
	intr_offset = intr_count % 8;
	for (ii = intr_offset; ii < ((rmi_fd.intr_src_count & MASK_3BIT) + intr_offset); ii++) {
		f54->intr_mask |= 1 << ii;
	}
#endif
	retval = f54->fn_ptr->read(rmi4_data, f54->query_base_addr, f54->query.data,
			sizeof(f54->query.data));
	if (retval < 0) {
		TS_LOG_ERR("Failed to read query registers\n");
		goto exit_free_mem;
	}

	retval = synaptics_read_f34();
	if (retval) {
		TS_LOG_ERR(" Read F34 failed, retval = %d\n", retval);
		goto exit_free_mem;
	}

	return NO_ERR;

exit_free_mem:
	synaptics_f54_free();
	return retval;
}

