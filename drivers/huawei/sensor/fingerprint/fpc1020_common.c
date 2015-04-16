/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2013,2014 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */


#include "fpc1020_common.h"
#include "fpc1020_regs.h"
#include "fpc1020_capture.h"

#include <linux/sched.h>
struct chip_struct {
	fpc1020_chip_t type;
	u16 hwid;
	u8 revision;
};

static const char *chip_text[] = {
	"N/A",		/* FPC1020_CHIP_NONE */
	"fpc1020a"	/* FPC1020_CHIP_1020A */
};

static const struct chip_struct chip_data[] = {
	{FPC1020_CHIP_1020A,	0x020a,	0},
	{FPC1020_CHIP_NONE,	0,	0}
};

const fpc1020_setup_t fpc1020_setup_default_CT_a1a2 = {
	.adc_gain		= {0, 1, 1},	/*Dry, Wet, Normal */
	.adc_shift		= {5, 4, 2},
	.pxl_ctrl		= {0x1f0b,0x0f1f, 0x0f0f},
	.capture_settings_mux	= 0,
	.capture_count		= 1,
	.capture_mode		= FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start	= 18,
	.capture_row_count	= 173,
	.capture_col_start	= 1,
	.capture_col_groups	= 20,
};

const fpc1020_setup_t fpc1020_setup_default_CT_a3a4 = {
	.adc_gain		= {3, 0, 10},	/* Wet, Normal,Dry */
	.adc_shift		= {6, 8, 2},
	.pxl_ctrl		= {0x0f1b,0x0f1b, 0x0f0B},
	.capture_settings_mux	= 0,
	.capture_count		= 1,
	.capture_mode		= FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start	= 18,
	.capture_row_count	= 173,
	.capture_col_start	= 1,
	.capture_col_groups	= 20,
};

const fpc1020_setup_t fpc1020_setup_default_LO_a1a2 = {
	.adc_gain		= {0,1, 2},	/*Dry, Wet, Normal */
	.adc_shift		= {5,1, 4},
	.pxl_ctrl		= {0x1f0b,0x0f1f, 0x0f0b},
	.capture_settings_mux	= 0,
	.capture_count		= 1,
	.capture_mode		= FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start	= 19,
	.capture_row_count	= 171,
	.capture_col_start	= 2,
	.capture_col_groups	= 21,
};


const fpc1020_setup_t fpc1020_setup_default_LO_a3a4 = {
	.adc_gain		= {3, 0, 10},	/* Wet, Normal,Dry */
	.adc_shift		= {6, 8, 2},
	.pxl_ctrl		= {0x0f1b,0x0f1b, 0x0f0B},
	.capture_settings_mux	= 0,
	.capture_count		= 1,
	.capture_mode		= FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start	= 19,
	.capture_row_count	= 171,
	.capture_col_start	= 2,
	.capture_col_groups	= 21,
};
const fpc1020_diag_t fpc1020_diag_default = {
	.selftest     = 0,
	.spi_register = 0,
	.spi_regsize  = 0,
	.spi_data     = 0,
	.fingerdetect = 0,
	.navigation_enable = 0,
	.wakeup_enable = 0,
};

static int fpc1020_check_hw_id_extended(fpc1020_data_t *fpc1020);

static int fpc1020_hwid_1020a(fpc1020_data_t *fpc1020);

static int fpc1020_write_id_1020a_setup(fpc1020_data_t *fpc1020);

static int fpc1020_write_sensor_1020a_setup(fpc1020_data_t *fpc1020);


/* -------------------------------------------------------------------- */
/* function definitions							*/
/* -------------------------------------------------------------------- */
int fpc1020_setup_defaults(fpc1020_data_t *fpc1020)
{
	int error;
	const fpc1020_setup_t *ptr;

	memcpy((void *)&fpc1020->diag,
	       (void *)&fpc1020_diag_default,
	       sizeof(fpc1020_diag_t));

	error = (fpc1020->chip_type == FPC1020_CHIP_1020A) ? 0 : -EINVAL;
	if (error)
		goto out_err;

	if (gpio_is_valid(fpc1020->moduleID_gpio))
	{
		printk("fingerprint module ID = %d  chip_revision =%d \n",gpio_get_value(fpc1020->moduleID_gpio),fpc1020->chip_revision);
		if(gpio_get_value(fpc1020->moduleID_gpio))
		{
			printk("fingerprint module CT high \n");
			ptr = (fpc1020->chip_revision == 1) ? &fpc1020_setup_default_CT_a1a2 :
				(fpc1020->chip_revision == 2) ? &fpc1020_setup_default_CT_a1a2 :
				(fpc1020->chip_revision == 3) ? &fpc1020_setup_default_CT_a3a4 :
				(fpc1020->chip_revision == 4) ? &fpc1020_setup_default_CT_a3a4 :
				NULL;
		}
		else
		{
			printk("fingerprint module LO low \n");
			ptr = (fpc1020->chip_revision == 1) ? &fpc1020_setup_default_LO_a1a2 :
				(fpc1020->chip_revision == 2) ? &fpc1020_setup_default_LO_a1a2 :
				(fpc1020->chip_revision == 3) ? &fpc1020_setup_default_LO_a3a4 :
				(fpc1020->chip_revision == 4) ? &fpc1020_setup_default_LO_a3a4 :
				NULL;
		}
				
	}
	else
		ptr = (fpc1020->chip_revision == 1) ? &fpc1020_setup_default_LO_a1a2 :
			(fpc1020->chip_revision == 2) ? &fpc1020_setup_default_LO_a1a2 :
			(fpc1020->chip_revision == 3) ? &fpc1020_setup_default_LO_a3a4 :
			(fpc1020->chip_revision == 4) ? &fpc1020_setup_default_LO_a3a4 :
			NULL;

	error = (ptr == NULL) ? -EINVAL : 0;
	if (error)
		goto out_err;

	memcpy((void *)&fpc1020->setup,	ptr, sizeof(fpc1020_setup_t));

	dev_dbg(&fpc1020->spi->dev, "%s OK\n", __func__);

	return 0;

out_err:
	memset((void *)&fpc1020->setup,	0, sizeof(fpc1020_setup_t));
	dev_err(&fpc1020->spi->dev, "%s FAILED %d\n", __func__, error);

	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_gpio_reset(fpc1020_data_t *fpc1020)
{
	int error = 0;
	int counter = FPC1020_RESET_RETRIES;

	while (counter) {
		counter--;

		gpio_set_value(fpc1020->reset_gpio, 1);
		udelay(FPC1020_RESET_HIGH1_US);

		gpio_set_value(fpc1020->reset_gpio, 0);
		udelay(FPC1020_RESET_LOW_US);

		gpio_set_value(fpc1020->reset_gpio, 1);
		udelay(FPC1020_RESET_HIGH2_US);

		error = gpio_get_value(fpc1020->irq_gpio) ? 0 : -EIO;

		if (!error) {
			printk(KERN_INFO "%s OK !\n", __func__);
			counter = 0;
		} else {
			printk(KERN_INFO "%s timed out,retrying ...\n",
				__func__);

			udelay(1250);
		}
	}
	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_spi_reset(fpc1020_data_t *fpc1020)
{
	int error = 0;
	int counter = FPC1020_RESET_RETRIES;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	while (counter) {
		counter--;

		error = fpc1020_cmd(fpc1020,
				FPC1020_CMD_SOFT_RESET,
				false);

		if (error >= 0) {
			error = fpc1020_wait_for_irq(fpc1020,
					FPC1020_DEFAULT_IRQ_TIMEOUT_MS);
		}

		if (error >= 0) {
			error = gpio_get_value(fpc1020->irq_gpio) ? 0 : -EIO;

			if (!error) {
				dev_dbg(&fpc1020->spi->dev,
					"%s OK !\n", __func__);

				counter = 0;

			} else {
				dev_dbg(&fpc1020->spi->dev,
					"%s timed out,retrying ...\n",
					__func__);
			}
		}
	}
	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_reset(fpc1020_data_t *fpc1020)
{
	int error = 0;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	error = (fpc1020->soft_reset_enabled) ?
			fpc1020_spi_reset(fpc1020) :
			fpc1020_gpio_reset(fpc1020);

	if (error < 0)
		goto out;
	disable_irq(fpc1020->irq);
	fpc1020->interrupt_done = false;
	enable_irq(fpc1020->irq);

	error = fpc1020_read_irq(fpc1020, true);

	if (error < 0)
		goto out;

	if (error != FPC_1020_IRQ_REG_BITS_REBOOT) {
		dev_err(&fpc1020->spi->dev,
			"IRQ register, expected 0x%x, got 0x%x.\n",
			FPC_1020_IRQ_REG_BITS_REBOOT, (u8)error);

		error = -EIO;
		goto out;
	}

	error = (gpio_get_value(fpc1020->irq_gpio) != 0) ? -EIO : 0;

	if (error)
		dev_err(&fpc1020->spi->dev, "IRQ pin, not low after clear.\n");

	error = fpc1020_read_irq(fpc1020, true);

	if (error != 0) {
		dev_err(&fpc1020->spi->dev,
			"IRQ register, expected 0x%x, got 0x%x.\n",
			0,
			(u8)error);

		error = -EIO;
		goto out;
	}

	fpc1020->capture.available_bytes = 0;
	fpc1020->capture.read_offset = 0;
	fpc1020->capture.read_pending_eof = false;
out:
	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_check_hw_id(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u16 hardware_id;

	fpc1020_reg_access_t reg;
	int counter = 0;
	bool match = false;

	FPC1020_MK_REG_READ(reg, FPC1020_REG_HWID, &hardware_id);
	error = fpc1020_reg_access(fpc1020, &reg);

	if (error)
		return error;

	while (!match && chip_data[counter].type != FPC1020_CHIP_NONE) {
		if (chip_data[counter].hwid == hardware_id)
			match = true;
		else
			counter++;
	}

	if (match) {
		fpc1020->chip_type     = chip_data[counter].type;
		fpc1020->chip_revision = chip_data[counter].revision;

		if (fpc1020->chip_revision == 0) {
			error = fpc1020_check_hw_id_extended(fpc1020);

			if (error > 0) {
				fpc1020->chip_revision = error;
				error = 0;
			}
		}

		dev_info(&fpc1020->spi->dev,
				"Hardware id: 0x%x (%s, rev.%d) \n",
						hardware_id,
						chip_text[fpc1020->chip_type],
						fpc1020->chip_revision);
	} else {
		dev_err(&fpc1020->spi->dev,
			"Hardware id mismatch: got 0x%x\n", hardware_id);

		fpc1020->chip_type = FPC1020_CHIP_NONE;
		fpc1020->chip_revision = 0;

		return -EIO;
	}

	return error;
}


/* -------------------------------------------------------------------- */
const char *fpc1020_hw_id_text(fpc1020_data_t *fpc1020)
{
	return chip_text[fpc1020->chip_type];
}


/* -------------------------------------------------------------------- */
static int fpc1020_check_hw_id_extended(fpc1020_data_t *fpc1020)
{
	int error = 0;

	if (fpc1020->chip_revision != 0) {
		return fpc1020->chip_revision;
	}

	error = (fpc1020->chip_type == FPC1020_CHIP_1020A) ?
			fpc1020_hwid_1020a(fpc1020) : -EINVAL;

	if (error < 0) {
		dev_err(&fpc1020->spi->dev,
			"%s, Unable to check chip revision %d\n",
			__func__,error);
	}

	return (error < 0) ? error : fpc1020->chip_revision;
}


/* -------------------------------------------------------------------- */
static int fpc1020_hwid_1020a(fpc1020_data_t *fpc1020)
{
	int error = 0;
	int xpos, ypos, m1, m2, count;

	const int num_lines = 5;
	const int num_pixels = 32;
	const size_t image_size = num_lines *
			FPC1020_PIXEL_COL_GROUPS * FPC1020_ADC_GROUP;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	if (fpc1020->chip_type != FPC1020_CHIP_1020A)
		return -EINVAL;

	error = fpc1020_write_id_1020a_setup(fpc1020);
	if (error)
		goto out_err;

	error = fpc1020_capture_set_crop(fpc1020,
					0, FPC1020_PIXEL_COL_GROUPS,
					0, num_lines);
	if (error)
		goto out_err;

	error = fpc1020_capture_buffer(fpc1020,
					fpc1020->huge_buffer,
					0,
					image_size);
	if (error)
		goto out_err;

	m1 = m2 = count = 0;

	for (ypos = 1; ypos < num_lines; ypos++) {
		for (xpos = 0; xpos < num_pixels; xpos++) {
			m1 += fpc1020->huge_buffer
				[(ypos * FPC1020_PIXEL_COLUMNS) + xpos];

			m2 += fpc1020->huge_buffer
				[(ypos * FPC1020_PIXEL_COLUMNS) +
					(FPC1020_PIXEL_COLUMNS - 1 - xpos)];
			count++;
		}
	}

	m1 /= count;
	m2 /= count;

	if (fpc1020_check_in_range_u64(m1, 181, 219) &&
		fpc1020_check_in_range_u64(m2, 101, 179))
	{
		fpc1020->chip_revision = 1;
	}
	else if (fpc1020_check_in_range_u64(m1, 181, 219) &&
		fpc1020_check_in_range_u64(m2, 181, 219))
	{
		fpc1020->chip_revision = 2;
	}
	else if (fpc1020_check_in_range_u64(m1, 101, 179) &&
		fpc1020_check_in_range_u64(m2, 151, 179))
	{
		fpc1020->chip_revision = 3;
	}
	else if (fpc1020_check_in_range_u64(m1, 0, 99) &&
		fpc1020_check_in_range_u64(m2, 0, 99))
	{
		fpc1020->chip_revision = 4;
	}
	else
	{
		fpc1020->chip_revision = 0;
	}

	dev_dbg(&fpc1020->spi->dev, "%s m1,m2 = %d,%d %s rev=%d \n", __func__,
		m1, m2,
		(fpc1020->chip_revision == 0) ? "UNKNOWN!" : "detected",
		fpc1020->chip_revision);

out_err:
	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_write_id_1020a_setup(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	fpc1020_reg_access_t reg;

	temp_u16 = 15 << 8;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = 0xffff;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_TST_COL_PATTERN_EN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x04;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_write_sensor_setup(fpc1020_data_t *fpc1020)
{
	if (fpc1020->chip_type != FPC1020_CHIP_1020A)
		return -EINVAL;

	return fpc1020_write_sensor_1020a_setup(fpc1020);
}


/* -------------------------------------------------------------------- */
static int fpc1020_write_sensor_1020a_setup(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u32 temp_u32;
	u64 temp_u64;
	fpc1020_reg_access_t reg;
	int mux = 0;
	int rev = fpc1020->chip_revision;

	dev_dbg(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

	if (rev == 0)
		return -EINVAL;

	temp_u64 = (rev == 1) ?	0x363636363f3f3f3f : 0x141414141e1e1e1e;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_SAMPLE_PX_DLY, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = (rev == 1) ?	0x33 : 	0x0f;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_PXL_RST_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = (rev == 1) ? 0x37 : 0x15; 
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FINGER_DRIVE_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	if (rev < 3) {
		temp_u64 = 0x5540003f24;
		FPC1020_MK_REG_WRITE(reg, FPC1020_REG_ADC_SETUP, &temp_u64);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;

	temp_u32 = 0x00080000;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_ANA_TEST_MUX, &temp_u32);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x5540003f34;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_ADC_SETUP, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;
	}

	temp_u8 =  0x02; //32内部3.3V  02是外部供电VDDTX
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = fpc1020->setup.adc_shift[mux];
	temp_u16 <<= 8;
	temp_u16 |= fpc1020->setup.adc_gain[mux];

	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = fpc1020->setup.pxl_ctrl[mux];
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x03 | 0x08;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_IMAGE_SETUP, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x50;
	FPC1020_MK_REG_WRITE(reg,FPC1020_REG_FNGR_DET_THRES, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = 0x0010;
	FPC1020_MK_REG_WRITE(reg,FPC1020_REG_FNGR_DET_CNTR, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}

/* -------------------------------------------------------------------- */
int fpc1020_write_sensor_checkerboard_setup(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u32 temp_u32;
	u64 temp_u64;
	fpc1020_reg_access_t reg;
	int mux = 0;
	int rev = fpc1020->chip_revision;

	dev_dbg(&fpc1020->spi->dev, "%s %d\n", __func__, mux);
	printk("%s: chip_revision = %d\n", __func__, fpc1020->chip_revision);

	if (rev == 0)
		return -EINVAL;

	temp_u64 = (rev == 1 || rev == 2) ?	0x363636363f3f3f3f : 0x141414141e1e1e1e;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_SAMPLE_PX_DLY, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = (rev == 1 || rev == 2) ? 0x33 : 0x0f;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_PXL_RST_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = (rev == 1 || rev == 2) ? 0x37 : 0x15; 
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FINGER_DRIVE_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	if (rev < 3) {
		temp_u64 = 0x5540003f24;
		FPC1020_MK_REG_WRITE(reg, FPC1020_REG_ADC_SETUP, &temp_u64);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;

		temp_u32 = 0x00080000;
		FPC1020_MK_REG_WRITE(reg, FPC1020_REG_ANA_TEST_MUX, &temp_u32);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;

		temp_u64 = 0x5540003f34;
		FPC1020_MK_REG_WRITE(reg, FPC1020_REG_ADC_SETUP, &temp_u64);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;
	}

	temp_u8 =  0x02;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = (rev == 1 || rev == 2) ? 0x0001 : 0x1100;

	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = 0x0f1b;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x03 | 0x08;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_IMAGE_SETUP, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x50;
	FPC1020_MK_REG_WRITE(reg,FPC1020_REG_FNGR_DET_THRES, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = 0x0010;
	FPC1020_MK_REG_WRITE(reg,FPC1020_REG_FNGR_DET_CNTR, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}

/* -------------------------------------------------------------------- */
int fpc1020_wait_for_irq(fpc1020_data_t *fpc1020, int timeout)
{
	int result = 0;

	if (!timeout) {
		result = wait_event_interruptible(
				fpc1020->wq_irq_return,
				fpc1020->interrupt_done);
	} else {
		result = wait_event_interruptible_timeout(
				fpc1020->wq_irq_return,
				fpc1020->interrupt_done, timeout);
	}

	if (result < 0) {
		dev_err(&fpc1020->spi->dev,
			 "wait_event_interruptible interrupted by signal.\n");

		return result;
	}

	if (result || !timeout) {
		fpc1020->interrupt_done = false;
		return 0;
	}

	return -ETIMEDOUT;
}


/* -------------------------------------------------------------------- */
int fpc1020_read_irq(fpc1020_data_t *fpc1020, bool clear_irq)
{
	int error = 0;
	u8 irq_status;
	/* const */ fpc1020_reg_access_t reg_read = {
		.reg = FPC1020_REG_READ_INTERRUPT,
		.write = false,
		.reg_size = FPC1020_REG_SIZE(FPC1020_REG_READ_INTERRUPT),
		.dataptr = &irq_status
	};

	/* const */ fpc1020_reg_access_t reg_clear = {
		.reg = FPC1020_REG_READ_INTERRUPT_WITH_CLEAR,
		.write = false,
		.reg_size = FPC1020_REG_SIZE(
					FPC1020_REG_READ_INTERRUPT_WITH_CLEAR),
		.dataptr = &irq_status
	};

	error = fpc1020_reg_access(fpc1020,
				(clear_irq) ? &reg_clear : &reg_read);

	return (error < 0) ? error : irq_status;
}


/* -------------------------------------------------------------------- */
int fpc1020_read_status_reg(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u8 status;
	/* const */ fpc1020_reg_access_t reg_read = {
		.reg = FPC1020_REG_FPC_STATUS,
		.write = false,
		.reg_size = FPC1020_REG_SIZE(FPC1020_REG_FPC_STATUS),
		.dataptr = &status
	};

	error = fpc1020_reg_access(fpc1020, &reg_read);

	return (error < 0) ? error : status;
}


/* -------------------------------------------------------------------- */
int fpc1020_reg_access(fpc1020_data_t *fpc1020,
		      fpc1020_reg_access_t *reg_data)
{
	int error = 0;
	struct spi_message msg;

	struct spi_transfer cmd = {
		.cs_change = 0,
		.delay_usecs = 0,
		.speed_hz = FPC1020_SPI_CLOCK_SPEED,
		.tx_buf = &(reg_data->reg),
		.rx_buf = NULL,
		.len    = 1 + FPC1020_REG_ACCESS_DUMMY_BYTES(reg_data->reg),
		.tx_dma = 0,
		.rx_dma = 0,
		.bits_per_word = 0,
	};

	struct spi_transfer data = {
		.cs_change = 1,
		.delay_usecs = 0,
		.speed_hz = FPC1020_SPI_CLOCK_SPEED,
		.tx_buf = (reg_data->write) ? fpc1020->huge_buffer : NULL,
		.rx_buf = (!reg_data->write) ? fpc1020->huge_buffer : NULL,
		.len    = reg_data->reg_size,
		.tx_dma = 0,
		.rx_dma = 0,
		.bits_per_word = 0,
	};
#if CS_CONTROL
	gpio_set_value(fpc1020->cs_gpio, 0);
#endif
	if (reg_data->write) {
		#if (target_little_endian)
			int src = 0;
			int dst = reg_data->reg_size - 1;

			while (src < reg_data->reg_size) {
				fpc1020->huge_buffer[dst] =
							reg_data->dataptr[src];
				src++;
				dst--;
			}
		#else
			memcpy(fpc1020->huge_buffer,
				reg_data->dataptr,
				reg_data->reg_size);
		#endif
	}

	spi_message_init(&msg);
	spi_message_add_tail(&cmd,  &msg);
	spi_message_add_tail(&data, &msg);

	 error = spi_sync(fpc1020->spi, &msg);

	if (error)
		dev_err(&fpc1020->spi->dev, "spi_sync failed.\n");

	if (!reg_data->write) {
		#if (target_little_endian)
			int src = reg_data->reg_size - 1;
			int dst = 0;

			while (dst < reg_data->reg_size) {
				reg_data->dataptr[dst] =
						fpc1020->huge_buffer[src];
				src--;
				dst++;
			}
		#else
			memcpy(reg_data->dataptr,
				fpc1020->huge_buffer,
				reg_data->reg_size);
		#endif
	}
#if CS_CONTROL
	gpio_set_value(fpc1020->cs_gpio, 1);
#endif
/*
	dev_dbg(&fpc1020->spi->dev,
		"%s %s 0x%x/%dd (%d bytes) %x %x %x %x : %x %x %x %x\n",
		 __func__,
		(reg_data->write) ? "WRITE" : "READ",
		reg_data->reg,
		reg_data->reg,
		reg_data->reg_size,
		(reg_data->reg_size > 0) ? fpc1020->huge_buffer[0] : 0,
		(reg_data->reg_size > 1) ? fpc1020->huge_buffer[1] : 0,
		(reg_data->reg_size > 2) ? fpc1020->huge_buffer[2] : 0,
		(reg_data->reg_size > 3) ? fpc1020->huge_buffer[3] : 0,
		(reg_data->reg_size > 4) ? fpc1020->huge_buffer[4] : 0,
		(reg_data->reg_size > 5) ? fpc1020->huge_buffer[5] : 0,
		(reg_data->reg_size > 6) ? fpc1020->huge_buffer[6] : 0,
		(reg_data->reg_size > 7) ? fpc1020->huge_buffer[7] : 0);
*/
	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_cmd(fpc1020_data_t *fpc1020,
			fpc1020_cmd_t cmd,
			u8 wait_irq_mask)
{
	int error = 0;
	struct spi_message msg;

	struct spi_transfer t = {
		.cs_change = 1,
		.delay_usecs = 0,
		.speed_hz = FPC1020_SPI_CLOCK_SPEED,
		.tx_buf = &cmd,
		.rx_buf = NULL,
		.len    = 1,
		.tx_dma = 0,
		.rx_dma = 0,
		.bits_per_word = 0,
	};
#if CS_CONTROL
	gpio_set_value(fpc1020->cs_gpio, 0);
#endif
	spi_message_init(&msg);
	spi_message_add_tail(&t,  &msg);

	error = spi_sync(fpc1020->spi, &msg);

	if (error)
		dev_err(&fpc1020->spi->dev, "spi_sync failed.\n");

	if ((error >= 0) && wait_irq_mask) {
		error = fpc1020_wait_for_irq(fpc1020,
					FPC1020_DEFAULT_IRQ_TIMEOUT_MS);

		if (error >= 0)
			error = fpc1020_read_irq(fpc1020, true);
	}
#if CS_CONTROL
	gpio_set_value(fpc1020->cs_gpio, 1);
#endif
	// dev_dbg(&fpc1020->spi->dev, "%s 0x%x/%dd\n", __func__, cmd, cmd);

	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_wait_finger_present(fpc1020_data_t *fpc1020)
{
	int error = 0;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	error = fpc1020_read_irq(fpc1020, true);
	if (error < 0)
		return error;

	error = fpc1020_cmd(fpc1020,
			FPC1020_CMD_WAIT_FOR_FINGER_PRESENT, 0);

	if (error < 0)
		return error;

	while (1) {
		error = fpc1020_wait_for_irq(fpc1020,
						FPC1020_DEFAULT_IRQ_TIMEOUT_MS);

		if (error >= 0) {
			error = fpc1020_read_irq(fpc1020, true);
			if (error < 0)
				return error;

			if (error &
				(FPC_1020_IRQ_REG_BIT_FINGER_DOWN |
				FPC_1020_IRQ_REG_BIT_COMMAND_DONE)) {

				dev_dbg(&fpc1020->spi->dev, "%s Finger down\n", __func__);

				error = 0;
			} else {
				dev_dbg(&fpc1020->spi->dev,
					"%s Unexpected IRQ = %d\n", __func__,
					error);

				error = -EIO;
			}
			return error;
		}

		if (error < 0) {
			if (fpc1020->worker.stop_request){
				dev_dbg(&fpc1020->spi->dev,"%s stop Request \n", __func__);
				return -EINTR;
				}
			if (error != -ETIMEDOUT)
				return error;
		}
	}
}


/* -------------------------------------------------------------------- */
int fpc1020_check_finger_present_raw(fpc1020_data_t *fpc1020)
{
	fpc1020_reg_access_t reg;
	u16 temp_u16;
	int error = 0;

	error = fpc1020_read_irq(fpc1020, true);
	if (error < 0)
		return error;

	error = fpc1020_cmd(fpc1020,
			FPC1020_CMD_FINGER_PRESENT_QUERY,
			FPC_1020_IRQ_REG_BIT_COMMAND_DONE);

	if (error < 0)
		return error;

	FPC1020_MK_REG_READ(reg, FPC1020_REG_FINGER_PRESENT_STATUS, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
	{
		dev_dbg(&fpc1020->spi->dev, "%s error = 0x%x\n", __func__, error);
		return error;
	}

//	dev_dbg(&fpc1020->spi->dev, "%s zonedata = 0x%x\n", __func__, temp_u16);

	return temp_u16;
}


/* -------------------------------------------------------------------- */
int fpc1020_check_finger_present_sum(fpc1020_data_t *fpc1020)
{
	int zones = 0;
	u16 mask = FPC1020_FINGER_DETECT_ZONE_MASK;
	u8 count = 0;

	zones = fpc1020_check_finger_present_raw(fpc1020);

	if (zones < 0)
		return zones;
	else {
		zones &= mask;
		while (zones && mask) {
			count += (zones & 1) ? 1 : 0;
			zones >>= 1;
			mask >>= 1;
		}
		// dev_dbg(&fpc1020->spi->dev, "%s %d zones\n", __func__, count);
		return (int)count;
	}
}


/* -------------------------------------------------------------------- */
 int fpc1020_wake_up(fpc1020_data_t *fpc1020)
{
	const fpc1020_status_reg_t status_mask = FPC1020_STATUS_REG_MODE_MASK;

	int power  = 0;
	int reset  = 0;
	int status = 0;

	reset  = fpc1020_reset(fpc1020);
	status = fpc1020_read_status_reg(fpc1020);
	if (power == 0 && reset == 0 && status >= 0 &&
		(fpc1020_status_reg_t)(status & status_mask) ==
		FPC1020_STATUS_REG_IN_IDLE_MODE) {

		dev_dbg(&fpc1020->spi->dev, "%s OK\n", __func__);

		return 0;
	} else {

		dev_err(&fpc1020->spi->dev, "%s FAILED -%d\n", __func__,EIO);

		return -EIO;
	}
}
int fpc1020_sleep(fpc1020_data_t *fpc1020, bool deep_sleep)
{
	const char *str_deep = "deep";
	const char *str_regular = "regular";
	bool sleep_ok;
	int retries = FPC1020_SLEEP_RETRIES;
	const fpc1020_status_reg_t status_mask = FPC1020_STATUS_REG_MODE_MASK;

	int error =0;

	error = fpc1020_cmd(fpc1020,
				(deep_sleep) ? FPC1020_CMD_ACTIVATE_DEEP_SLEEP_MODE :
						FPC1020_CMD_ACTIVATE_SLEEP_MODE,
				0);


	if (error) {
		dev_dbg(&fpc1020->spi->dev,
			"%s %s command failed %d\n", __func__,
			(deep_sleep)? str_deep : str_regular,
			error);

		return error;
	}

	error = 0;
	sleep_ok = false;

	while (!sleep_ok && retries && (error >= 0)) {

		error = fpc1020_read_status_reg(fpc1020);

		if (error < 0) {
			dev_dbg(&fpc1020->spi->dev,
				"%s %s read status failed %d\n", __func__,
				(deep_sleep)? str_deep : str_regular,
				error);
		} else {
			error &= status_mask;
			sleep_ok = (deep_sleep) ?
				error == FPC1020_STATUS_REG_IN_DEEP_SLEEP_MODE :
				error == FPC1020_STATUS_REG_IN_SLEEP_MODE;
		}
		if (!sleep_ok) {
			udelay(FPC1020_SLEEP_RETRY_TIME_US);
			retries--;
		}
	}

	if (deep_sleep && sleep_ok && gpio_is_valid(fpc1020->reset_gpio))
		gpio_set_value(fpc1020->reset_gpio, 0);

#if CS_CONTROL
	if (deep_sleep && sleep_ok && gpio_is_valid(fpc1020->cs_gpio))
		gpio_set_value(fpc1020->cs_gpio, 0);
#endif
	/* Optional: Also disable power supplies in sleep */

	if (sleep_ok) {
		dev_dbg(&fpc1020->spi->dev,
			"%s %s OK\n", __func__,
			(deep_sleep)? str_deep : str_regular);
		return 0;
	} else {
		dev_err(&fpc1020->spi->dev,
			"%s %s FAILED\n", __func__,
			(deep_sleep)? str_deep : str_regular);
		return (deep_sleep) ? -EIO : -EAGAIN;
	}
}


/* -------------------------------------------------------------------- */
int fpc1020_fetch_image(fpc1020_data_t *fpc1020,
				u8 *buffer,
				int offset,
				size_t image_size_bytes,
				size_t buff_size)
{
	int error = 0;
	struct spi_message msg;
	const u8 tx_data[2] = {FPC1020_CMD_READ_IMAGE , 0};

	int buffer_order = get_order(FPC1020_FRAME_SIZE_MAX);
	u8* single_buff = (u8 *)__get_free_pages(GFP_KERNEL, buffer_order);

	dev_dbg(&fpc1020->spi->dev, "%s (+%d, buff_size=%d , image_byte_size =%d ldh memory)\n", __func__, offset, buff_size, image_size_bytes);

	if ((offset + (int)image_size_bytes) > buff_size) {
		dev_err(&fpc1020->spi->dev,
			"Image buffer too small for offset +%d (max %d bytes)",
			offset,
			buff_size);

		error = -ENOBUFS;
	}

	if (!error) {
		struct spi_transfer cmd = {
			.cs_change = 0,
			.delay_usecs = 0,
			.speed_hz = FPC1020_SPI_CLOCK_SPEED,
			.tx_buf = tx_data,
			.rx_buf = NULL,
			.len    = 2,
			.tx_dma = 0,
			.rx_dma = 0,
			.bits_per_word = 0,
		};

		struct spi_transfer data = {
			.cs_change = 1,
			.delay_usecs = 0,
			.speed_hz = FPC1020_SPI_CLOCK_SPEED,
			.tx_buf = NULL,
			.rx_buf = single_buff,
			.len    = (int)image_size_bytes,
			.tx_dma = 0,
			.rx_dma = 0,
			.bits_per_word = 0,
		};
#if CS_CONTROL
		gpio_set_value(fpc1020->cs_gpio, 0);
#endif
		spi_message_init(&msg);
		spi_message_add_tail(&cmd,  &msg);
		spi_message_add_tail(&data, &msg);

	 error = spi_sync(fpc1020->spi, &msg);

		if (error)
			dev_err(&fpc1020->spi->dev, "spi_sync failed.\n");

		memmove(buffer + offset, single_buff , image_size_bytes); 

#if CS_CONTROL
		gpio_set_value(fpc1020->cs_gpio, 1);
#endif
	}

	error = fpc1020_read_irq(fpc1020, true);

	if (error > 0)
		error = (error & FPC_1020_IRQ_REG_BIT_ERROR) ? -EIO : 0;
	if (single_buff) {
		free_pages((unsigned long)single_buff,
			   get_order(FPC1020_FRAME_SIZE_MAX));
	}
	return error;
}


/* -------------------------------------------------------------------- */
bool fpc1020_check_in_range_u64(u64 val, u64 min, u64 max)
{
	return (val >= min) && (val <= max);
}


/* -------------------------------------------------------------------- */
size_t fpc1020_calc_image_size(fpc1020_data_t *fpc1020)
{
	int image_byte_size = fpc1020->setup.capture_row_count * 
				fpc1020->setup.capture_col_groups *
				FPC1020_ADC_GROUP;

	dev_dbg(&fpc1020->spi->dev, "%s Rows %d->%d,Cols %d->%d (%d bytes)\n",
				__func__,
				fpc1020->setup.capture_row_start,
				fpc1020->setup.capture_row_start
				+ fpc1020->setup.capture_row_count - 1,
				fpc1020->setup.capture_col_start
					* FPC1020_ADC_GROUP,
				(fpc1020->setup.capture_col_start
					* FPC1020_ADC_GROUP)
				+ (fpc1020->setup.capture_col_groups *
					FPC1020_ADC_GROUP) - 1,
				image_byte_size
				);

	return image_byte_size;
}




/* -------------------------------------------------------------------- */

