/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include "sensor_common.h"
#include "../cci/hisi_cci.h"
#include "../io/hisi_isp_io.h"
#include "../camera/isp_ops.h"

void hisi_mclk_config(struct hisi_sensor_ctrl_t *s_ctrl,
	struct sensor_power_setting *power_setting, int state)
{
	int sensor_index;

	cam_debug("%s enter.state:%d", __func__, state);
	if (SENSOR_INDEX_INVALID != power_setting->sensor_index) {
		sensor_index = power_setting->sensor_index;
	} else {
		sensor_index = s_ctrl->sensor->sensor_info->sensor_index;
	}

	if (is_fpga_board())
		return;

	if (POWER_ON == state) {
		/* mclk */
		if (CAMERA_SENSOR_PRIMARY == sensor_index) {
			SETREG8(REG_ISP_CLK_DIVIDER, 0x44);
		} else {
			SETREG8(REG_ISP_CLK_DIVIDER, 0x44);
		}
	} else {
		//SETREG8(REG_ISP_CLK_DIVIDER, 0);
	}
	
	if (0 != power_setting->delay) {
		camdrv_msleep(power_setting->delay);
	}

	return;
}

int hisi_sensor_gpio_config(gpio_t pin_type, struct hisi_sensor_info *sensor_info,
	struct sensor_power_setting *power_setting, int state)
{
	int rc = -1;

	cam_debug("%s enter, pin_type: %d", __func__, pin_type);

	if (is_fpga_board())
		return 0;

	if(0 == sensor_info->gpios[pin_type].gpio) {
		cam_notice("gpio type[%d] is not actived", pin_type);
		return 0;
	}

	rc = gpio_request(sensor_info->gpios[pin_type].gpio, NULL);
	if(rc < 0) {
		cam_err("failed to request gpio[%d]", sensor_info->gpios[pin_type].gpio);
		return rc;
	}

	if(pin_type == FSIN) {
		cam_info("pin_level: %d", gpio_get_value(sensor_info->gpios[pin_type].gpio));
		rc = 0;
	} else {
		rc = gpio_direction_output(sensor_info->gpios[pin_type].gpio,
			state ? (power_setting->config_val + 1) % 2 : power_setting->config_val);
		if(rc < 0) {
			cam_err("failed to control gpio[%d]", sensor_info->gpios[pin_type].gpio);
		}
	}

	gpio_free(sensor_info->gpios[pin_type].gpio);

	if (0 != power_setting->delay) {
		camdrv_msleep(power_setting->delay);
	}

	return rc;

}

int hisi_sensor_ldo_config(ldo_index_t ldo, struct hisi_sensor_info *sensor_info,
	struct sensor_power_setting *power_setting, int state)
{
	int index;
	int rc = -1;
	const char *ldo_names[LDO_MAX]
		= {"dvdd", "dvdd2", "avdd", "avdd2", "vcm", "vcm2", "iopw"};

	cam_debug("%s enter, ldo: %d", __func__, ldo);

	if (is_fpga_board())
		return 0;

	for(index = 0; index < sensor_info->ldo_num; index++) {
		if(!strcmp(sensor_info->ldo[index].supply, ldo_names[ldo]))
			break;
	}

	if(index == sensor_info->ldo_num) {
		cam_notice("ldo [%s] is not actived", ldo_names[ldo]);
		return 0;
	}

	if (POWER_ON == state) {
		if(LDO_IOPW != ldo) {
			rc = regulator_set_voltage(sensor_info->ldo[index].consumer, power_setting->config_val, power_setting->config_val);
			if(rc < 0) {
				cam_err("failed to set ldo[%s] to %d V", ldo_names[ldo], power_setting->config_val);
				return rc;
			}
		}
		rc = regulator_bulk_enable(1, &sensor_info->ldo[index]);
		if (rc) {
			cam_err("failed to enable regulators %d\n", rc);
			return rc;
		}
		if (0 != power_setting->delay) {
			camdrv_msleep(power_setting->delay);
		}
	} else {
		regulator_bulk_disable(1, &sensor_info->ldo[index]);
		rc = 0;
	}

	return rc;

}

void hisi_sensor_i2c_config(struct hisi_sensor_ctrl_t *s_ctrl,
	struct sensor_power_setting *power_setting, int state)
{
	cam_debug("enter %s, state:%d", __func__, state);

	if (is_fpga_board())
		return;

	if (POWER_ON == state) {
		isp_config_i2c(&s_ctrl->sensor->sensor_info->i2c_config);

		if (0 != power_setting->delay) {
			camdrv_msleep(power_setting->delay);
		}
	}

	return;
}



int hisi_sensor_power_up(struct hisi_sensor_ctrl_t *s_ctrl)
{
	struct sensor_power_setting_array *power_setting_array
		= &s_ctrl->sensor->power_setting_array;
	struct sensor_power_setting *power_setting = NULL;
	int index = 0, rc = 0;

	cam_debug("%s enter.", __func__);

	/* fpga board compatibility */

	if (is_fpga_board()) {
		//hisi_sensor_power_up_fpga(s_ctrl);
		return 0;
	}

	for (index = 0; index < power_setting_array->size; index++) {
		power_setting = &power_setting_array->power_setting[index];
		switch(power_setting->seq_type) {
		case SENSOR_DVDD:
			rc = hisi_sensor_ldo_config(LDO_DVDD, s_ctrl->sensor->sensor_info,
				power_setting, POWER_ON);
			break;
		case SENSOR_DVDD2:
			rc = hisi_sensor_ldo_config(LDO_DVDD2, s_ctrl->sensor->sensor_info,
				power_setting, POWER_ON);
			break;
		case SENSOR_IOVDD:
			rc = hisi_sensor_ldo_config(LDO_IOPW, s_ctrl->sensor->sensor_info,
				power_setting, POWER_ON);
			break;
		case SENSOR_AVDD:
			rc = hisi_sensor_ldo_config(LDO_AVDD, s_ctrl->sensor->sensor_info,
				power_setting, POWER_ON);
			break;
		case SENSOR_AVDD2:
			rc = hisi_sensor_ldo_config(LDO_AVDD2, s_ctrl->sensor->sensor_info,
				power_setting, POWER_ON);
			break;
		case SENSOR_VCM_AVDD:
			rc = hisi_sensor_ldo_config(LDO_VCM, s_ctrl->sensor->sensor_info,
				power_setting, POWER_ON);
			break;
		case SENSOR_VCM_AVDD2:
			rc = hisi_sensor_ldo_config(LDO_VCM2, s_ctrl->sensor->sensor_info,
				power_setting, POWER_ON);
			break;

		case SENSOR_MCLK:
			hisi_mclk_config(s_ctrl, power_setting, POWER_ON);
			break;
		case SENSOR_I2C:
			hisi_sensor_i2c_config(s_ctrl, power_setting, POWER_ON);
			break;

		case SENSOR_CHECK_LEVEL:
			rc = hisi_sensor_gpio_config(FSIN, s_ctrl->sensor->sensor_info,
				power_setting, POWER_ON);
			break;
		case SENSOR_RST:
			rc = hisi_sensor_gpio_config(RESETB, s_ctrl->sensor->sensor_info,
				power_setting, POWER_ON);
			break;
		case SENSOR_PWDN:
			rc = hisi_sensor_gpio_config(PWDN, s_ctrl->sensor->sensor_info,
				power_setting, POWER_ON);
			break;
		case SENSOR_VCM_PWDN:
			rc = hisi_sensor_gpio_config(VCM, s_ctrl->sensor->sensor_info,
				power_setting, POWER_ON);
			break;
		case SENSOR_SUSPEND:
			rc = hisi_sensor_gpio_config(SUSPEND, s_ctrl->sensor->sensor_info,
				power_setting, POWER_ON);
			break;

		default:
			cam_err("%s invalid seq_type.", __func__);
			break;
		}

		if(rc) {
			cam_err("%s power up procedure error.", __func__);
			break;
		}
	}
	return rc;
}

int hisi_sensor_power_down(struct hisi_sensor_ctrl_t *s_ctrl)
{
	struct sensor_power_setting_array *power_setting_array
		= &s_ctrl->sensor->power_setting_array;
	struct sensor_power_setting *power_setting = NULL;
	int index = 0, rc = 0;

	cam_debug("%s enter.", __func__);

	for (index = (power_setting_array->size - 1); index >= 0; index--) {
		power_setting = &power_setting_array->power_setting[index];
		switch(power_setting->seq_type) {
		case SENSOR_DVDD:
			rc = hisi_sensor_ldo_config(LDO_DVDD, s_ctrl->sensor->sensor_info,
				power_setting, POWER_OFF);
			break;
		case SENSOR_DVDD2:
			rc = hisi_sensor_ldo_config(LDO_DVDD2, s_ctrl->sensor->sensor_info,
				power_setting, POWER_OFF);
			break;
		case SENSOR_IOVDD:
			rc = hisi_sensor_ldo_config(LDO_IOPW, s_ctrl->sensor->sensor_info,
				power_setting, POWER_OFF);
			break;
		case SENSOR_AVDD:
			rc = hisi_sensor_ldo_config(LDO_AVDD, s_ctrl->sensor->sensor_info,
				power_setting, POWER_OFF);
			break;
		case SENSOR_AVDD2:
			rc = hisi_sensor_ldo_config(LDO_AVDD2, s_ctrl->sensor->sensor_info,
				power_setting, POWER_OFF);
			break;
		case SENSOR_VCM_AVDD:
			rc = hisi_sensor_ldo_config(LDO_VCM, s_ctrl->sensor->sensor_info,
				power_setting, POWER_OFF);
			break;
		case SENSOR_VCM_AVDD2:
			rc = hisi_sensor_ldo_config(LDO_VCM2, s_ctrl->sensor->sensor_info,
				power_setting, POWER_OFF);
			break;

		case SENSOR_MCLK:
			hisi_mclk_config(s_ctrl, power_setting, POWER_OFF);
			break;
		case SENSOR_I2C:
			break;

		case SENSOR_CHECK_LEVEL:
			break;
		case SENSOR_PWDN:
			rc = hisi_sensor_gpio_config(PWDN, s_ctrl->sensor->sensor_info,
				power_setting, POWER_OFF);
			break;
		case SENSOR_RST:
			rc = hisi_sensor_gpio_config(RESETB, s_ctrl->sensor->sensor_info,
				power_setting, POWER_OFF);
			break;
		case SENSOR_VCM_PWDN:
			rc = hisi_sensor_gpio_config(VCM, s_ctrl->sensor->sensor_info,
				power_setting, POWER_OFF);
			break;
		case SENSOR_SUSPEND:
			break;

		default:
			cam_err("%s invalid seq_type.", __func__);
			break;
		}

	}
	return rc;
}

int hisi_sensor_i2c_read(struct hisi_sensor_ctrl_t *s_ctrl, void *data)
{
	struct sensor_cfg_data *cdata = (struct sensor_cfg_data *)data;
	long   rc = 0;
	unsigned int reg, *val;

	cam_debug("%s: address=0x%x\n", __func__, cdata->cfg.reg.subaddr);

	/* parse the I2C parameters */
	reg = cdata->cfg.reg.subaddr;
	val = &cdata->cfg.reg.value;

	rc = isp_read_sensor_byte(&s_ctrl->sensor->sensor_info->i2c_config, reg, (u16 *)val);

	return rc;
}

int hisi_sensor_i2c_write(struct hisi_sensor_ctrl_t *s_ctrl, void *data)
{
	struct sensor_cfg_data *cdata = (struct sensor_cfg_data *)data;
	long   rc = 0;
	unsigned int reg, val, mask;

	cam_debug("%s enter.\n", __func__);

	cam_debug("%s: address=0x%x, value=0x%x\n", __func__,
		cdata->cfg.reg.subaddr, cdata->cfg.reg.value);

	/* parse the I2C parameters */
	reg = cdata->cfg.reg.subaddr;
	val = cdata->cfg.reg.value;
	mask = cdata->cfg.reg.mask;

	rc = isp_write_sensor_byte(&s_ctrl->sensor->sensor_info->i2c_config, reg, val, mask, SCCB_BUS_MUTEX_WAIT);

	return rc;
}

int hisi_sensor_i2c_read_seq(struct hisi_sensor_ctrl_t *s_ctrl, void *data)
{
	struct sensor_cfg_data *cdata = (struct sensor_cfg_data *)data;
	struct sensor_i2c_setting setting;
	int size = sizeof(struct sensor_i2c_reg)*cdata->cfg.setting.size;
	long rc = 0;

	cam_debug("%s: enter.\n", __func__);

	setting.setting = (struct sensor_i2c_reg*)kzalloc(size, GFP_KERNEL);
	if (NULL == setting.setting) {
		cam_err("%s kmalloc error.\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(setting.setting,
		(void __user *)cdata->cfg.setting.setting, size)) {
		cam_err("%s copy_from_user error.\n", __func__);
		rc = -EFAULT;
		goto fail;
	}

	/* test */
	{
		int i=0;
		for(i=0; i<cdata->cfg.setting.size; i++) {
			cam_debug("%s subaddr=0x%x.\n",
				__func__,
				setting.setting[i].subaddr);
				setting.setting[i].value = i;
		}
	}

	if (copy_to_user((void __user *)cdata->cfg.setting.setting,
		setting.setting, size)) {
		cam_err("%s copy_to_user error.\n", __func__);
		rc = -EFAULT;
		goto fail;
	}

fail:
	kfree(setting.setting);
	return rc;
}

int hisi_sensor_i2c_write_seq(struct hisi_sensor_ctrl_t *s_ctrl, void *data)
{
	struct sensor_cfg_data *cdata = (struct sensor_cfg_data *)data;
	struct sensor_i2c_setting setting;
	int data_length = sizeof(struct sensor_i2c_reg)*cdata->cfg.setting.size;
	long rc = 0;

	cam_info("%s: enter setting=0x%x size=%d.\n", __func__,
			(unsigned int)cdata->cfg.setting.setting,
			(unsigned int)cdata->cfg.setting.size);

	setting.setting = (struct sensor_i2c_reg*)kzalloc(data_length, GFP_KERNEL);
	if (NULL == setting.setting) {
		cam_err("%s kmalloc error.\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(setting.setting,
		(void __user *)cdata->cfg.setting.setting, data_length)) {
		cam_err("%s copy_from_user error.\n", __func__);
		rc = -EFAULT;
		goto out;
	}

	rc = isp_write_sensor_seq(&s_ctrl->sensor->sensor_info->i2c_config, setting.setting, cdata->cfg.setting.size);
out:
	kfree(setting.setting);
	return rc;
}

int hisi_sensor_apply_expo_gain(struct hisi_sensor_ctrl_t *s_ctrl, void *data)
{
	struct sensor_cfg_data *cdata = (struct sensor_cfg_data *)data;
	struct expo_gain_seq host_ae_seq = cdata->cfg.host_ae_seq;
	struct hisi_sensor_t *sensor = s_ctrl->sensor;
	int i;

	if (sensor->sensor_info->sensor_type == 1) {/*ov sensor*/
		memmove(host_ae_seq.gain + 1, host_ae_seq.gain, sizeof(u32) * host_ae_seq.seq_size);
		host_ae_seq.expo[host_ae_seq.seq_size] = host_ae_seq.expo[host_ae_seq.seq_size - 1];
		host_ae_seq.seq_size++;
	}

	for (i = 0; i < host_ae_seq.seq_size; i++) {
		cam_info("expo[0x%04x], gain[0x%02x], hts[0x%02x], vts[0x%02x]",
			host_ae_seq.expo[i], host_ae_seq.gain[i], host_ae_seq.hts, host_ae_seq.vts);
	}
	cam_info("eof trigger[%d]", host_ae_seq.eof_trigger);

	return setup_eof_tasklet(sensor, &host_ae_seq);

}

int hisi_sensor_suspend_eg_task(struct hisi_sensor_ctrl_t *s_ctrl, void *data)
{
	struct sensor_cfg_data *cdata = (struct sensor_cfg_data *)data;
	struct expo_gain_seq ori_state = cdata->cfg.host_ae_seq;

	cam_notice("enter %s", __func__);
	cam_notice("expo[0x%04x], gain[0x%02x], hts[0x%02x], vts[0x%02x]",
		ori_state.expo[0], ori_state.gain[0], ori_state.hts, ori_state.vts);

	return teardown_eof_tasklet(s_ctrl->sensor, &ori_state);
}
