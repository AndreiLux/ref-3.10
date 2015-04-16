/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2013,2014 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#define DEBUG

#include <linux/input.h>
#include <linux/delay.h>


#include "fpc1020_common.h"
#include "fpc1020_capture.h"

int fpc1020_init_capture(fpc1020_data_t *fpc1020)
{
	fpc1020->capture.state = FPC1020_CAPTURE_STATE_IDLE;
	fpc1020->capture.current_mode = FPC1020_MODE_IDLE;
	fpc1020->capture.available_bytes = 0;

	init_waitqueue_head(&fpc1020->capture.wq_data_avail);

	return 0;
}

/* -------------------------------------------------------------------- */
int fpc1020_write_capture_setup(fpc1020_data_t *fpc1020)
{
	return fpc1020_write_sensor_setup(fpc1020);
}


/* -------------------------------------------------------------------- */
int fpc1020_write_test_setup(fpc1020_data_t *fpc1020, u16 pattern)
{
	int error = 0;
	u8 config = 0x04;
	fpc1020_reg_access_t reg;

	dev_dbg(&fpc1020->spi->dev, "%s, pattern 0x%x\n", __func__, pattern);

	error = fpc1020_write_sensor_checkerboard_setup(fpc1020);
	if (error)
		goto out;

	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_TST_COL_PATTERN_EN, &pattern);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FINGER_DRIVE_CONF, &config);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}


/* -------------------------------------------------------------------- */
bool fpc1020_capture_check_ready(fpc1020_data_t *fpc1020)
{
	fpc1020_capture_state_t state = fpc1020->capture.state;

	return (state == FPC1020_CAPTURE_STATE_IDLE) ||
		(state == FPC1020_CAPTURE_STATE_COMPLETED) ||
		(state == FPC1020_CAPTURE_STATE_FAILED);
}


/* -------------------------------------------------------------------- */
int fpc1020_capture_task(fpc1020_data_t *fpc1020)
{
	int error = 0;
	bool expect_finger, adjust_settings;
	fpc1020_capture_mode_t mode = fpc1020->capture.current_mode;
	int current_capture, capture_count;
	int image_offset;

	size_t image_byte_size;

	expect_finger = (mode == FPC1020_MODE_WAIT_AND_CAPTURE || mode == FPC1020_MODE_WAIT_FINGER_UP_AND_CAPTURE);

	fpc1020->capture.state = FPC1020_CAPTURE_STATE_WRITE_SETTINGS;

	error = fpc1020_wake_up(fpc1020);
	if (error < 0)
		goto out_error;
	switch (mode) {
	case FPC1020_MODE_WAIT_AND_CAPTURE:
	case FPC1020_MODE_SINGLE_CAPTURE:
	case FPC1020_MODE_WAIT_FINGER_UP_AND_CAPTURE:
		capture_count = fpc1020->setup.capture_count;
		adjust_settings = true;
		error = fpc1020_write_capture_setup(fpc1020);
		break;

	case FPC1020_MODE_CHECKERBOARD_TEST_NORM:
		capture_count = 1;
		adjust_settings = false;
		error = fpc1020_write_test_setup(fpc1020, 0xaa55);
		break;

	case FPC1020_MODE_CHECKERBOARD_TEST_INV:
		capture_count = 1;
		adjust_settings = false;
		error = fpc1020_write_test_setup(fpc1020, 0x55aa);
		break;

	case FPC1020_MODE_BOARD_TEST_ONE:
		capture_count = 1;
		adjust_settings = false;
		error = fpc1020_write_test_setup(fpc1020, 0xffff);
		break;

	case FPC1020_MODE_BOARD_TEST_ZERO:
		capture_count = 1;
		adjust_settings = false;
		error = fpc1020_write_test_setup(fpc1020, 0x0000);
		break;

	case FPC1020_MODE_IDLE:
	default:
		capture_count = 0;
		adjust_settings = false;
		error = -EINVAL;
		break;
	}

	if (error < 0)
		goto out_error;

	if(mode == FPC1020_MODE_CHECKERBOARD_TEST_NORM ||
			mode == FPC1020_MODE_CHECKERBOARD_TEST_INV ||
			mode == FPC1020_MODE_BOARD_TEST_ONE ||
			mode == FPC1020_MODE_BOARD_TEST_ZERO){
		error = fpc1020_capture_set_crop(fpc1020, 0,24,0,192);
		image_byte_size = 36864;
	}
	else{
		error = fpc1020_capture_set_crop(fpc1020, 
					fpc1020->setup.capture_col_start,
					fpc1020->setup.capture_col_groups,
					fpc1020->setup.capture_row_start,
					fpc1020->setup.capture_row_count);
		image_byte_size = fpc1020_calc_image_size(fpc1020);
	}
	if (error < 0)
		goto out_error;

	dev_dbg(&fpc1020->spi->dev,
		"Start capture, mode %d, (%d frames)\n",
		mode,
		capture_count);

	if (expect_finger) {
		fpc1020->capture.state =
				FPC1020_CAPTURE_STATE_WAIT_FOR_FINGER_DOWN;

		error = fpc1020_capture_wait_finger_down(fpc1020);

		if (error < 0)
			goto out_error;

		dev_dbg(&fpc1020->spi->dev, "Finger down\n");
	}

	current_capture = 0;
	image_offset = 0;

	while (capture_count && (error >= 0))
	{
		fpc1020->capture.state = FPC1020_CAPTURE_STATE_ACQUIRE;

		dev_dbg(&fpc1020->spi->dev,"Capture, frame %d \n",current_capture + 1);

		error =	(!adjust_settings) ? 0 :
			fpc1020_capture_settings(fpc1020, current_capture);

		if (error < 0)
			goto out_error;

		error = fpc1020_cmd(fpc1020,
				FPC1020_CMD_CAPTURE_IMAGE,
				FPC_1020_IRQ_REG_BIT_FIFO_NEW_DATA);

		if (error < 0)
			goto out_error;

		fpc1020->capture.state = FPC1020_CAPTURE_STATE_FETCH;

		error = fpc1020_fetch_image(fpc1020,
						fpc1020->huge_buffer,
						image_offset,
						image_byte_size,
						(size_t)fpc1020->huge_buffer_size);
		if (error < 0)
			goto out_error;

		fpc1020->capture.available_bytes += (int)image_byte_size;
		fpc1020->capture.last_error = error;

		capture_count--;
		current_capture++;
		image_offset += (int)image_byte_size;
	}
	wake_up_interruptible(&fpc1020->capture.wq_data_avail);

	if (expect_finger) {
		fpc1020->capture.state =
				FPC1020_CAPTURE_STATE_WAIT_FOR_FINGER_UP;
		
		if(mode == FPC1020_MODE_WAIT_FINGER_UP_AND_CAPTURE){
			error = FPC1020_FINGER_DOWN_THRESHOLD + 1;
			while (error > FPC1020_FINGER_DOWN_THRESHOLD){
				msleep(FPC1020_POLLING_TIME_MS);
				error = fpc1020_check_finger_present_sum(fpc1020);
			}
		}

		if (error < 0)
			goto out_error;

		dev_dbg(&fpc1020->spi->dev, "Finger up\n");
	}

out_error:
	fpc1020->capture.last_error = error;

	if (error < 0) {
		fpc1020->capture.state = FPC1020_CAPTURE_STATE_FAILED;
		dev_err(&fpc1020->spi->dev, "%s FAILED %d\n", __func__, error);
	} else {
		fpc1020->capture.state = FPC1020_CAPTURE_STATE_COMPLETED;
		dev_err(&fpc1020->spi->dev, "%s OK\n", __func__);
	}
	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_capture_wait_finger_down(fpc1020_data_t *fpc1020)
{
	int error = 0;
	bool finger_down = false;

	error = fpc1020_wait_finger_present(fpc1020);

	while (!finger_down && (error >= 0))
	{
		if (fpc1020->worker.stop_request)
			error = -EINTR;
		else
			error = fpc1020_check_finger_present_sum(fpc1020);

		if (error > FPC1020_FINGER_DOWN_THRESHOLD)
			finger_down = true;
		else
			msleep(FPC1020_CAPTURE_WAIT_FINGER_DELAY_MS);
	}

	if(error < 0)
		return error;

	error = fpc1020_read_irq(fpc1020, true);

	return (finger_down) ? 0 : error;
}


/* -------------------------------------------------------------------- */
int fpc1020_capture_settings(fpc1020_data_t *fpc1020, int select)
{
	int error = 0;
	fpc1020_reg_access_t reg;

	u16 pxlCtrl;
	u16 adc_shift_gain;

	dev_dbg(&fpc1020->spi->dev, "%s #%d\n", __func__, select);

	if (select >= FPC1020_BUFFER_MAX_IMAGES) {
		error = -EINVAL;
		goto out_err;
	}

	pxlCtrl = fpc1020->setup.pxl_ctrl[select];

	adc_shift_gain = fpc1020->setup.adc_shift[select];
	adc_shift_gain <<= 8;
	adc_shift_gain |= fpc1020->setup.adc_gain[select];

	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_PXL_CTRL, &pxlCtrl);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out_err;

	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_ADC_SHIFT_GAIN, &adc_shift_gain);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out_err;

out_err:
	if (error)
		dev_err(&fpc1020->spi->dev, "%s Error %d\n", __func__, error);

	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_capture_set_crop(fpc1020_data_t *fpc1020,
					int first_column,
					int num_columns,
					int first_row,
					int num_rows)
{
	fpc1020_reg_access_t reg;
	u32 temp_u32;

	temp_u32 = first_row;
	temp_u32 <<= 8;
	temp_u32 |= num_rows;
	temp_u32 <<= 8;
	temp_u32 |= (first_column * FPC1020_ADC_GROUP);
	temp_u32 <<= 8;
	temp_u32 |= (num_columns * FPC1020_ADC_GROUP);

	FPC1020_MK_REG_WRITE(reg,FPC1020_REG_IMG_CAPT_SIZE, &temp_u32);
	return fpc1020_reg_access(fpc1020, &reg);
}

/* -------------------------------------------------------------------- */
int fpc1020_capture_buffer(fpc1020_data_t *fpc1020,
				u8 *data,
				size_t offset,
				size_t image_size_bytes)
{
	int error = 0;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	error = fpc1020_cmd(fpc1020,
			FPC1020_CMD_CAPTURE_IMAGE,
			FPC_1020_IRQ_REG_BIT_FIFO_NEW_DATA);

	if (error < 0)
		goto out_error;

	error = fpc1020_fetch_image(fpc1020,
					data,
					offset,
					image_size_bytes,
					(size_t)fpc1020->huge_buffer_size);
	if (error < 0)
		goto out_error;

	return 0;

out_error:
	dev_dbg(&fpc1020->spi->dev, "%s FAILED %d\n", __func__, error);

	return error;
}


/* -------------------------------------------------------------------- */

