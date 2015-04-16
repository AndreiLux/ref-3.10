/*
 * Copyright (C) huawei company
 *
 * This	program	is free	software; you can redistribute it and/or modify
 * it under	the	terms of the GNU General Public	License	version	2 as
 * published by	the	Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include "inputhub_route.h"
#include "protocol.h"
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/fs.h>
#include "sensor_info.h"
#include "sensor_sys_info.h"
#include <linux/mtd/hisi_nve_interface.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif

int hisi_nve_direct_access(struct hisi_nve_info_user *user_info);

#define SENSOR_CONFIG_DATA_OK 0
#define MAX_MCU_DATA_LENGTH  30
#define PHONE_COLOR_NV_NUM	231
#define MAG_CALIBRATE_DATA_NV_NUM 233
#define MAG_CALIBRATE_DATA_NV_SIZE (MAX_MAG_CALIBRATE_DATA_LENGTH)
#define NV_READ_TAG	1
#define NV_WRITE_TAG	0
#define OFFSET_BIG               127
#define OFFSET_SMALL             -128
#define Z_SCALE_BIG              255
#define Z_SCALE_SMALL            175
#define MAX_STR_SIZE	1024
#define MAX_FILE_ID	64

static char sensor_chip_info[SENSOR_MAX][20];
static int gsensor_offset[3]; //g-sensor校准参数
static RET_TYPE return_calibration = EXEC_FAIL;
static struct sensor_status sensor_status;
static struct sensor_status sensor_status_backup;
extern int first_start_flag;
static char buf[MAX_PKT_LENGTH]={0};
pkt_sys_dynload_req_t *dyn_req = (pkt_sys_dynload_req_t *)buf;

static struct gyro_platform_data gyro_data={
		.poll_interval=10,
		.axis_map_x=1,
		.axis_map_y=0,
		.axis_map_z=2,
		.negate_x=1,
		.negate_y=0,
		.negate_z=1,
		.gpio_int1=210,
	};
static struct g_sensor_platform_data gsensor_data={
	.poll_interval=10,
	.axis_map_x=1,
	.axis_map_y=0,
	.axis_map_z=2,
	.negate_x=0,
	.negate_y=1,
	.negate_z=0,
	.gpio_int1=208,
};
static struct compass_platform_data mag_data={
	.poll_interval=10,
	.outbit=0,
	.axis_map_x=0,
	.axis_map_y=1,
	.axis_map_z=2,
};
static struct als_platform_data als_data={
	.threshold_value = 1,
	.GA1 = 4166,
	.GA2 = 3000,
	.GA3 = 3750,
	.gpio_int1 = 206,
	.atime = 0xdb,
	.again = 1,
	.poll_interval = 1000,
	.init_time = 150,
};

static struct ps_platform_data ps_data={
	.min_proximity_value = 750,
	.pwindows_value = 100,
	.pwave_value = 60,
	.threshold_value = 70,
	.rdata_under_sun = 5500,
	.ps_pulse_count = 5,
	.gpio_int1 = 206,
	.persistent = 0x33,
	.ptime = 0xFF,
	.poll_interval = 250,
	.init_time = 100,
};

static bool should_be_processed_when_sr(int sensor_tag)
{
	bool ret = true;//can be closed default
	switch (sensor_tag) {
		case TAG_PS:
			ret = false;
			break;

		case TAG_STEP_COUNTER:
			ret = false;
			break;

		default:
			break;
	}

	return ret;
}

static DEFINE_MUTEX(mutex_update);
void update_sensor_info(const pkt_header_t *pkt)//To keep same with mcu, to activate sensor need open first and then setdelay
{
	if (TAG_SENSOR_BEGIN <= pkt->tag && pkt->tag < TAG_SENSOR_END) {
		mutex_lock(&mutex_update);
		if (CMD_CMN_OPEN_REQ == pkt->cmd) {
			sensor_status.opened[pkt->tag] = 1;
		} else if (CMD_CMN_CLOSE_REQ == pkt->cmd) {
			sensor_status.opened[pkt->tag] = 0;
			sensor_status.status[pkt->tag] = 0;
		} else if (CMD_CMN_INTERVAL_REQ == pkt->cmd) {
			if (sensor_status.opened[pkt->tag]) {
				sensor_status.delay[pkt->tag] = ((const pkt_cmn_interval_req_t *)pkt)->interval;
				sensor_status.status[pkt->tag] = 1;
			}
		}
		mutex_unlock(&mutex_update);
	}
}

void disable_sensors_when_suspend(void)
{
	int tag;

	memcpy(&sensor_status_backup, &sensor_status, sizeof(sensor_status_backup));
	for (tag = TAG_SENSOR_BEGIN; tag < TAG_SENSOR_END; ++tag) {
		if (sensor_status_backup.status[tag]) {
			if (should_be_processed_when_sr(tag)) {
				inputhub_sensor_enable(tag, false);
			}
		}
	}
}

void enable_sensors_when_resume(void)
{
	int tag;

	for (tag = TAG_SENSOR_BEGIN; tag < TAG_SENSOR_END; ++tag) {
		if (sensor_status_backup.status[tag] && (0 == sensor_status.status[tag])) {
			if (should_be_processed_when_sr(tag)) {
				inputhub_sensor_enable(tag, true);
				inputhub_sensor_setdelay(tag, sensor_status_backup.delay[tag]);
			}
		}
	}
}

/*******************************************************************************************
Function:	read_gsensor_offset_from_nv
Description:   读取NV项中的gsensor 校准数据，并发送给mcu 侧
Data Accessed:  无
Data Updated:   无
Input:         无
Output:         无
Return:         成功或者失败信息: 0->成功, -1->失败
*******************************************************************************************/
int read_gsensor_offset_from_nv(void)
{
	int ret = 0;
	struct hisi_nve_info_user user_info;
	write_info_t	pkg_ap;
	read_info_t	pkg_mcu;
	memset(&user_info, 0, sizeof(user_info));
	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	user_info.nv_operation = NV_READ_TAG;
	user_info.nv_number = PHONE_COLOR_NV_NUM;
	user_info.valid_size = 15;
	strncpy(user_info.nv_name, "gsensor", sizeof(user_info.nv_name));
	user_info.nv_name[sizeof(user_info.nv_name) - 1] = '\0';
	if ((ret = hisi_nve_direct_access(&user_info))!=0)
	{
		hwlog_err("nve_direct_access read error(%d)\n", ret);
		return -1;
	}
	first_start_flag=1;
	gsensor_offset[0] = (int8_t)user_info.nv_data[0];
	gsensor_offset[1] = (int8_t)user_info.nv_data[1];
	gsensor_offset[2] = user_info.nv_data[2];
	if (gsensor_offset[0] > 127)
		gsensor_offset[0] = gsensor_offset[0] - 256;
	if (gsensor_offset[1] > 127)
		gsensor_offset[1] = gsensor_offset[1] - 256;
	hwlog_info( "nve_direct_access read gsensor_offset (%d %d %d )\n",gsensor_offset[0],gsensor_offset[1],gsensor_offset[2]);
	if(gsensor_offset[0]>OFFSET_BIG||gsensor_offset[0]<OFFSET_SMALL
					||gsensor_offset[1]>OFFSET_BIG||gsensor_offset[1]<OFFSET_SMALL
						||gsensor_offset[2]>Z_SCALE_BIG||gsensor_offset[2]<Z_SCALE_SMALL)
	{
		hwlog_err("read gsensor offset data from nv suc, but value unormal! gsensor_offset (%d %d %d )\n",gsensor_offset[0],gsensor_offset[1],gsensor_offset[2]);
		return -1;
	}
	else
	{
		gsensor_data.offset_x=gsensor_offset[0];
		gsensor_data.offset_y=gsensor_offset[1];
		gsensor_data.offset_z=gsensor_offset[2];
		hwlog_err("read gsensor offset data from nv suc, value nomal! gsensor_offset (%d %d %d )\n",gsensor_offset[0],gsensor_offset[1],gsensor_offset[2]);
	}
	pkg_ap.tag=TAG_ACCEL;
	pkg_ap.cmd=CMD_ACCEL_OFFSET_REQ;
	pkg_ap.wr_buf=&gsensor_data.offset_x;
	pkg_ap.wr_len=sizeof(int)*3;
	ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
	if(ret)
	{
		hwlog_err("send gsensor offset data to mcu fail,ret=%d\n", ret);
		return -1;
	}
	if(pkg_mcu.errno!=0) hwlog_err("set gsensor offset fail,err=%d\n", pkg_mcu.errno);
	else
	{
		hwlog_info("send gsensor offset data to mcu success\n");
	}
	return 0;
}

/*******************************************************************************************
Function:	write_gsensor_offset_to_nv
Description:  将temp数据写入NV 项中
Data Accessed:  无
Data Updated:   无
Input:        g-sensor 校准值
Output:         无
Return:         成功或者失败信息: 0->成功, -1->失败
*******************************************************************************************/
int write_gsensor_offset_to_nv(char* temp)
{
	int ret = 0;
	struct hisi_nve_info_user user_info;

	if(temp==NULL)
	{
		hwlog_err("write_gsensor_offset_to_nv fail, invalid para!\n");
		return -1;
	}
	memset(&user_info, 0, sizeof(user_info));
	user_info.nv_operation = NV_WRITE_TAG;
	user_info.nv_number = PHONE_COLOR_NV_NUM;
	user_info.valid_size = 15;
	strncpy(user_info.nv_name, "gsensor", sizeof(user_info.nv_name));
	user_info.nv_name[sizeof(user_info.nv_name) - 1] = '\0';

	user_info.nv_data[0] = temp[0];
	user_info.nv_data[1] = temp[1];
	user_info.nv_data[2] = temp[2];
	if ((ret = hisi_nve_direct_access(&user_info))!=0)
	{
		hwlog_err("nve_direct_access write error(%d)\n", ret);
		return -1;
	}
	hwlog_info( "nve_direct_access write temp (%d %d %d )\n",user_info.nv_data[0],user_info.nv_data[1],user_info.nv_data[2]);

	return ret;
}

int read_mag_calibrate_data_from_nv(void)
{
	int ret = 0;
	struct hisi_nve_info_user user_info;
	write_info_t pkg_ap;

	memset(&user_info, 0, sizeof(user_info));
	memset(&pkg_ap, 0, sizeof(pkg_ap));

	//read from nv
	user_info.nv_operation = NV_READ_TAG;
	user_info.nv_number = MAG_CALIBRATE_DATA_NV_NUM;
	user_info.valid_size = MAG_CALIBRATE_DATA_NV_SIZE;
	strncpy(user_info.nv_name, "msensor", sizeof(user_info.nv_name));
	user_info.nv_name[sizeof(user_info.nv_name) - 1] = '\0';
	if ((ret = hisi_nve_direct_access(&user_info))!=0) {
		hwlog_err("nve_direct_access read error(%d)\n", ret);
		return -1;
	}

	//send to mcu
	pkg_ap.tag = TAG_MAG;
	pkg_ap.cmd = CMD_MAG_SET_CALIBRATE_TO_MCU_REQ;
	pkg_ap.wr_buf = &user_info.nv_data;
	pkg_ap.wr_len = MAG_CALIBRATE_DATA_NV_SIZE;
	if((ret = write_customize_cmd(&pkg_ap, NULL)) != 0) {
		hwlog_err("set mag_sensor data failed, ret = %d!\n", ret);
		return ret;
	} else {
		hwlog_info("send mag_sensor data to mcu success\n");
	}

	return 0;
}

static int write_magsensor_calibrate_data_to_nv(const char *src)
{
	int ret = 0;
	struct hisi_nve_info_user user_info;

	if(NULL == src) {
		hwlog_err("%s fail, invalid para!\n", __func__);
		return -1;
	}

	memset(&user_info, 0, sizeof(user_info));
	user_info.nv_operation = NV_WRITE_TAG;
	user_info.nv_number = MAG_CALIBRATE_DATA_NV_NUM;
	user_info.valid_size = MAG_CALIBRATE_DATA_NV_SIZE;
	strncpy(user_info.nv_name, "msensor", sizeof(user_info.nv_name));
	user_info.nv_name[sizeof(user_info.nv_name) - 1] = '\0';
	memcpy(user_info.nv_data, src, sizeof(user_info.nv_data));
	if ((ret = hisi_nve_direct_access(&user_info)) != 0) {
		hwlog_err("nve_direct_access write error(%d)\n", ret);
		return -1;
	}

	return ret;
}

static int mag_calibrate_data_from_mcu(const pkt_header_t *head)
{
	return write_magsensor_calibrate_data_to_nv(((const pkt_mag_calibrate_data_req_t *)head)->calibrate_data);
}

static int detect_i2c_device(struct device_node *dn, char *device_name)
{
	write_info_t	pkg_ap;
	read_info_t	pkg_mcu;
	pkt_i2c_read_req_t pkt_i2c_read;
	int i=0, ret=0, i2c_address=0, i2c_bus_num=0, register_add=0, len=0;
	u32 wia[10]={0};
	struct property *prop = NULL;

	memset(&pkt_i2c_read, 0, sizeof(pkt_i2c_read));
	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if(of_property_read_u32(dn, "bus_number", &i2c_bus_num) || of_property_read_u32(dn, "reg", &i2c_address)
				|| of_property_read_u32(dn, "chip_id_register", &register_add))
	{
		hwlog_err("%s:read i2c bus_number (%d)or bus address(%x) or chip_id_register(%x) from dts fail\n",
			device_name, i2c_bus_num, i2c_address, register_add);
		return -1;
	}

	prop = of_find_property(dn, "chip_id_value", NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;
	len = prop->length/4;
	hwlog_info("%s:chip_id_value len=%d \n", device_name, len);
	if(of_property_read_u32_array(dn, "chip_id_value", wia, len))
	{
		hwlog_err("%s:read chip_id_value (id0=%x) from dts fail len=%d\n", device_name,  wia[0], len);
		return -1;
	}
	hwlog_info("chipid value 0x%x  0x%x  0x%x 0x%x--\n", wia[0], wia[1], wia[2], wia[3]);
	pkt_i2c_read.busid = i2c_bus_num;
	pkt_i2c_read.addr = i2c_address;
	pkt_i2c_read.reg = register_add;
	pkt_i2c_read.length = 1;
	pkg_ap.tag = TAG_I2C;
	pkg_ap.cmd = CMD_I2C_READ_REQ;
	pkg_ap.wr_buf = &pkt_i2c_read.busid;
	pkg_ap.wr_len = sizeof(pkt_i2c_read) - sizeof(pkt_i2c_read.hd);
	ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
	if(ret)
	{
		hwlog_err("%s:detect_i2c_device:send i2c read cmd to mcu fail,ret=%d\n", device_name, ret);
		return -1;
	}
	if(pkg_mcu.errno != 0)
	{
		hwlog_err("%s:detect_i2c_device:i2c read fail\n", device_name);
		return -1;
	}else
	{
		hwlog_info("%s:i2c read success, data=0x%x\n", device_name, pkg_mcu.data[0]);
		for(i=0;i<len;i++)
			if(pkg_mcu.data[0] == (char)wia[i])
			{
				hwlog_info("%s:i2c detect  suc!\n", device_name);
				return 0;
			}
		hwlog_info("%s:i2c detect  fail!\n", device_name);
		return -1;
	}
}

/* delete the repeated file id by map table*/
static uint8_t check_file_list(uint8_t file_count, uint16_t *file_list)
{
	uint8_t map[MAX_FILE_ID];
	int i;
	uint8_t count = 0;

	memset(map, 0, sizeof(map));
	if ((file_count == 0) ||(NULL == file_list)) {
		hwlog_err("%s, val invalid\n", __func__);
		return count;
	}

	for (i = 0; i < file_count; i++) {
		if (file_list[i] < sizeof(map))
			map[file_list[i]]++;
	}

	for(i = 0; i < sizeof(map); i++) {
		if (map[i]) {
			file_list[count] = i;
			count ++;
		}
	}

	return count;
}

static int send_fileid_to_mcu(void)
{
	write_info_t	pkg_ap;
	read_info_t	pkg_mcu;
	int ret = 0;

	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	dyn_req->end = 1;
	pkg_ap.tag = TAG_SYS;
	pkg_ap.cmd = CMD_SYS_DYNLOAD_REQ;
	pkg_ap.wr_buf = &(dyn_req->end);
	pkg_ap.wr_len = dyn_req->file_count * sizeof(dyn_req->file_list[0]) 
					+ sizeof(dyn_req->end) + sizeof(dyn_req->file_count);

	ret = write_customize_cmd(&pkg_ap, &pkg_mcu);
	if (ret)
	{
		hwlog_err("send file id to mcu fail,ret=%d\n", ret);
		return -1;
	}
	if (pkg_mcu.errno != 0)
	{
		hwlog_err("file id set fail\n");
		return -1;
	}
	hwlog_info("set file id to mcu  success, data len=%d\n", pkg_mcu.data_length);
	return 0;
}

static int init_sensors_cfg_data_from_dts(void)
{
	int ret = 0, i=0;
	int poll_val=0, gpio_irq=0, ga=0, atime=0, again=0, temp=0;
	char *sensor_ty = NULL, *chip_info = NULL;
	int temp_pdc[27]={0};
	struct device_node *dn = NULL;
	int acc_flag=0, mag_flag=0, gyro_flag=0, als_flag=0, ps_flag=0;

	for_each_node_with_property(dn, "sensor_type")
	{
		ret=of_property_read_string(dn, "sensor_type", (const char **)&sensor_ty);
		if(ret)
		{
			hwlog_err("get sensor type fail ret=%d\n",ret);
			continue;
		}
		if(!strncmp(sensor_ty, "acc", sizeof("acc")))
		{
			hwlog_info("get acc dev from dts.\n");
			if(acc_flag || detect_i2c_device(dn, "acc")) continue;
			acc_flag = 1;

			ret=of_property_read_string(dn, "compatible", (const char **)&chip_info);
			if(ret) hwlog_err("%s:read acc poll_interval fail\n",__func__);
			else strncpy(sensor_chip_info[ACC], chip_info, strlen(chip_info));
			hwlog_info("detect acc dev ok.sensor name=%s\n", chip_info);

			ret=of_property_read_u32(dn, "poll_interval", &poll_val);
			if(ret) hwlog_err("%s:read acc poll_interval fail\n",__func__);
			else gsensor_data.poll_interval=(uint16_t)poll_val;

			ret=of_property_read_u32(dn, "axis_map_x", &temp);
			if(ret) hwlog_err("%s:read acc axis_map_x fail\n",__func__);
			else gsensor_data.axis_map_x=(uint8_t)temp;

			ret=of_property_read_u32(dn, "axis_map_y", &temp);
			if(ret) hwlog_err("%s:read acc axis_map_y fail\n",__func__);
			else gsensor_data.axis_map_y=(uint8_t)temp;

			ret=of_property_read_u32(dn, "axis_map_z", &temp);
			if(ret) hwlog_err("%s:read acc axis_map_z fail\n",__func__);
			else gsensor_data.axis_map_z=(uint8_t)temp;

			ret=of_property_read_u32(dn, "negate_x", &temp);
			if(ret) hwlog_err("%s:read acc negate_x fail\n",__func__);
			else gsensor_data.negate_x=(uint8_t)temp;

			ret=of_property_read_u32(dn, "negate_y", &temp);
			if(ret) hwlog_err("%s:read acc negate_y fail\n",__func__);
			else gsensor_data.negate_y=(uint8_t)temp;

			ret=of_property_read_u32(dn, "negate_z", &temp);
			if(ret) hwlog_err("%s:read acc negate_z fail\n",__func__);
			else gsensor_data.negate_z=(uint8_t)temp;

			ret = of_property_read_u32(dn, "file_id", &temp);
			if(ret) hwlog_err("%s:read acc file_id fail\n", __func__);
			else dyn_req->file_list[dyn_req->file_count] = (uint16_t)temp;
			dyn_req->file_count++;

			gpio_irq = of_get_named_gpio(dn, "gpio_int1", 0);
			if(gpio_irq) gsensor_data.gpio_int1=(uint8_t)gpio_irq;
			hwlog_info("acc:poll=%d, gpio=%d, file_id=%d\n", poll_val, gpio_irq,temp);
		}else if(!strncmp(sensor_ty, "mag", sizeof("mag")))
		{
			hwlog_info("get mag dev from dts.\n");
			if(mag_flag || detect_i2c_device(dn, "mag")) continue;
			mag_flag = 1;
			ret=of_property_read_string(dn, "compatible", (const char **)&chip_info);
			if(ret) hwlog_err("%s:read mag poll_interval fail\n",__func__);
			else strncpy(sensor_chip_info[MAG], chip_info, strlen(chip_info));
			hwlog_info("get mag dev from dts.sensor name=%s\n", chip_info);

			ret=of_property_read_u32(dn, "poll_interval", &poll_val);
			if(ret) hwlog_err("%s:read mag poll_interval fail\n",__func__);
			else mag_data.poll_interval=(uint16_t)poll_val;

			ret=of_property_read_u32(dn, "axis_map_x", &temp);
			if(ret) hwlog_err("%s:read mag axis_map_x fail\n",__func__);
			else mag_data.axis_map_x=(uint8_t)temp;

			ret=of_property_read_u32(dn, "axis_map_y", &temp);
			if(ret) hwlog_err("%s:read mag axis_map_y fail\n",__func__);
			else mag_data.axis_map_y=(uint8_t)temp;

			ret=of_property_read_u32(dn, "axis_map_z", &temp);
			if(ret) hwlog_err("%s:read mag axis_map_z fail\n",__func__);
			else mag_data.axis_map_z=(uint8_t)temp;

			ret=of_property_read_u32(dn, "negate_x", &temp);
			if(ret) hwlog_err("%s:read mag negate_x fail\n",__func__);
			else mag_data.negate_x=(uint8_t)temp;

			ret=of_property_read_u32(dn, "negate_y", &temp);
			if(ret) hwlog_err("%s:read mag negate_y fail\n",__func__);
			else mag_data.negate_y=(uint8_t)temp;

			ret=of_property_read_u32(dn, "negate_z", &temp);
			if(ret) hwlog_err("%s:read mag negate_z fail\n",__func__);
			else mag_data.negate_z=(uint8_t)temp;

			ret=of_property_read_u32(dn, "outbit", &temp);
			if(ret) hwlog_err("%s:read mag outbit fail\n",__func__);
			else mag_data.outbit=(uint8_t)temp;

			gpio_irq=of_get_named_gpio(dn, "gpio_reset", 0);
			if(gpio_irq) mag_data.gpio_rst=(uint8_t)gpio_irq;

			gpio_irq=of_get_named_gpio(dn, "gpio_drdy", 0);
			if(gpio_irq) mag_data.gpio_drdy=(uint8_t)gpio_irq;

			ret=of_property_read_u32_array(dn, "softiron_parameter", temp_pdc, 27);
			if(ret)
			{
				hwlog_err("%s:read mag pdc data fail\n",__func__);
			}
			for(i=0;i<27;i++)
			{
				mag_data.pdc_data[i]=(uint8_t)temp_pdc[i];
			}
			hwlog_info("mag:poll=%d, gpio=%d,pdc0=%d, pdc5=%d\n", poll_val, gpio_irq, mag_data.pdc_data[0], mag_data.pdc_data[5]);
		}else if(!strncmp(sensor_ty, "gyro", sizeof("gyro")))
		{
			if(gyro_flag || detect_i2c_device(dn, "gyro")) continue;
			gyro_flag = 1;

			ret = of_property_read_string(dn, "compatible", (const char **)&chip_info);
			if(ret) hwlog_err("%s:read gyro poll_interval fail\n", __func__);
			else strncpy(sensor_chip_info[GYRO], chip_info, strlen(chip_info));
			hwlog_info("get gyro dev from dts.sensor name=%s\n", chip_info);

			ret=of_property_read_u32(dn, "poll_interval", &poll_val);
			if(ret) hwlog_err("%s:read mag poll_interval fail\n",__func__);
			else gyro_data.poll_interval=(uint16_t)poll_val;

			ret=of_property_read_u32(dn, "axis_map_x", &temp);
			if(ret) hwlog_err("%s:read gyro axis_map_x fail\n",__func__);
			else gyro_data.axis_map_x=(uint8_t)temp;

			ret=of_property_read_u32(dn, "axis_map_y", &temp);
			if(ret) hwlog_err("%s:read gyro axis_map_y fail\n",__func__);
			else gyro_data.axis_map_y=(uint8_t)temp;

			ret=of_property_read_u32(dn, "axis_map_z", &temp);
			if(ret) hwlog_err("%s:read gyro axis_map_z fail\n",__func__);
			else gyro_data.axis_map_z=(uint8_t)temp;

			ret=of_property_read_u32(dn, "negate_x", &temp);
			if(ret) hwlog_err("%s:read gyro negate_x fail\n",__func__);
			else gyro_data.negate_x=(uint8_t)temp;

			ret=of_property_read_u32(dn, "negate_y", &temp);
			if(ret) hwlog_err("%s:read gyro negate_y fail\n",__func__);
			else gyro_data.negate_y=(uint8_t)temp;

			ret=of_property_read_u32(dn, "negate_z", &temp);
			if(ret) hwlog_err("%s:read gyro negate_z fail\n",__func__);
			else gyro_data.negate_z=(uint8_t)temp;

			ret = of_property_read_u32(dn, "file_id", &temp);
			if(ret) hwlog_err("%s:read acc file_id fail\n", __func__);
			else dyn_req->file_list[dyn_req->file_count] = (uint16_t)temp;
			dyn_req->file_count++;

			gpio_irq = of_get_named_gpio(dn, "gpio_int1", 0);
			if(gpio_irq) gyro_data.gpio_int1 = (uint8_t)gpio_irq;
			hwlog_info("gyro:poll=%d, gpio=%d\n", poll_val, gpio_irq);
		}
		else if(!strncmp(sensor_ty, "als", sizeof("als")))
		{
			if(als_flag || detect_i2c_device(dn, "als")) continue;
			als_flag = 1;

			ret = of_property_read_string(dn, "compatible", (const char **)&chip_info);
			if(ret) hwlog_err("%s:read als poll_interval fail\n", __func__);
			else strncpy(sensor_chip_info[ALS], chip_info, strlen(chip_info));
			hwlog_info("get als dev from dts.sensor name=%s\n", chip_info);

			ret=of_property_read_u32(dn, "poll_interval", &poll_val);
			if(ret) hwlog_err("%s:read als poll_interval fail\n",__func__);
			else als_data.poll_interval=(uint16_t)poll_val;

			ret=of_property_read_u32(dn, "init_time", &temp);
			if(ret) hwlog_err("%s:read als init time fail\n",__func__);
			else als_data.init_time=(uint16_t)temp;
			hwlog_info("%s:als  init time =%d\n",__func__, als_data.init_time);

			gpio_irq=of_get_named_gpio(dn, "gpio_int1", 0);
			if(gpio_irq) als_data.gpio_int1=(uint8_t)gpio_irq;

			ret=of_property_read_u32(dn, "GA1", &ga);
			if(ret) hwlog_err("%s:read als ga1 fail\n",__func__);
			else als_data.GA1=ga;

			ret=of_property_read_u32(dn, "GA2", &ga);
			if(ret) hwlog_err("%s:read als ga2 fail\n",__func__);
			else als_data.GA2=ga;

			ret=of_property_read_u32(dn, "GA3", &ga);
			if(ret) hwlog_err("%s:read als ga3 fail\n",__func__);
			else als_data.GA3=ga;

			ret=of_property_read_u32(dn, "atime", &atime);
			if(ret) hwlog_err("%s:read als atime fail\n",__func__);
			else als_data.atime=(uint8_t)atime;

			ret=of_property_read_u32(dn, "again", &again);
			if(ret) hwlog_err("%s:read als again fail\n",__func__);
			else als_data.again=(uint8_t)again;

			ret = of_property_read_u32(dn, "file_id", &temp);
			if(ret) hwlog_err("%s:read acc file_id fail\n", __func__);
			else dyn_req->file_list[dyn_req->file_count] = (uint16_t)temp;
			dyn_req->file_count++;

			hwlog_info("als:poll=%d, gpio=%d,ga1=%d\n", poll_val, gpio_irq, als_data.GA1);
		}
		else if(!strncmp(sensor_ty, "ps", sizeof("ps")))
		{
			if(ps_flag || detect_i2c_device(dn, "ps")) continue;
			ps_flag = 1;

			ret = of_property_read_string(dn, "compatible", (const char **)&chip_info);
			if(ret) hwlog_err("%s:read ps poll_interval fail\n", __func__);
			else strncpy(sensor_chip_info[PS], chip_info, strlen(chip_info));
			hwlog_info("get ps dev from dts.sensor name=%s\n", chip_info);

			ret=of_property_read_u32(dn, "min_proximity_value", &temp);
			if(ret) hwlog_err("%s:read mag min_proximity_value fail\n",__func__);
			else ps_data.min_proximity_value=temp;

			gpio_irq=of_get_named_gpio(dn, "gpio_int1", 0);
			if(gpio_irq) ps_data.gpio_int1=(uint8_t)gpio_irq;

			ret=of_property_read_u32(dn, "pwindows_value", &temp);
			if(ret) hwlog_err("%s:read pwindows_value fail\n",__func__);
			else ps_data.pwindows_value=temp;

			ret=of_property_read_u32(dn, "pwave_value", &temp);
			if(ret) hwlog_err("%s:read pwave_value fail\n",__func__);
			else ps_data.pwave_value=temp;

			ret=of_property_read_u32(dn, "threshold_value", &temp);
			if(ret) hwlog_err("%s:read threshold_value fail\n",__func__);
			else ps_data.threshold_value=temp;

			ret=of_property_read_u32(dn, "rdata_under_sun", &temp);
			if(ret) hwlog_err("%s:read rdata_under_sun fail\n",__func__);
			else ps_data.rdata_under_sun=temp;

			ret=of_property_read_u32(dn, "ps_pulse_count", &temp);
			if(ret) hwlog_err("%s:read ps_pulse_count fail\n",__func__);
			else ps_data.ps_pulse_count=(uint8_t)temp;

			ret=of_property_read_u32(dn, "persistent", &temp);
			if(ret) hwlog_err("%s:read persistent fail\n",__func__);
			else ps_data.persistent=(uint8_t)temp;

			ret=of_property_read_u32(dn, "ptime", &temp);
			if(ret) hwlog_err("%s:read ptime fail\n",__func__);
			else ps_data.ptime=(uint8_t)temp;

			ret=of_property_read_u32(dn, "p_on", &temp);
			if(ret) hwlog_err("%s:read p_on fail\n",__func__);
			else ps_data.p_on=(uint8_t)temp;
			ret=of_property_read_u32(dn, "poll_interval", &temp);
			if(ret) hwlog_err("%s:read poll_interval fail\n",__func__);
			else ps_data.poll_interval=(uint16_t)temp;
			ret=of_property_read_u32(dn, "init_time", &temp);
			if(ret) hwlog_err("%s:read init_time fail\n",__func__);
			else ps_data.init_time=(uint16_t)temp;
			hwlog_err("%s:p_on =%d, init_time=%d, poll=%d\n", __func__, ps_data.p_on, ps_data.init_time, ps_data.poll_interval);

			ret = of_property_read_u32(dn, "file_id", &temp);
			if(ret) hwlog_err("%s:read acc file_id fail\n", __func__);
			else dyn_req->file_list[dyn_req->file_count] = (uint16_t)temp;
			dyn_req->file_count++;

			hwlog_info("ps:poll=%d, gpio=%d,pwindows_value=%d,pwave_value=%d.\n", poll_val, gpio_irq, ps_data.pwindows_value, ps_data.pwave_value);
		}
	}

	hwlog_info("get file id number = %d , fileid=%d %d %d %d %d\n", 
		dyn_req->file_count, dyn_req->file_list[0], dyn_req->file_list[1], dyn_req->file_list[2], dyn_req->file_list[3], dyn_req->file_list[4]);

	dyn_req->file_count = check_file_list(dyn_req->file_count, dyn_req->file_list);
	if (dyn_req->file_count)
	{
		hwlog_info("after check, get file id number = %d , fileid=%d %d %d %d %d\n",
			dyn_req->file_count, dyn_req->file_list[0], dyn_req->file_list[1], dyn_req->file_list[2], dyn_req->file_list[3], dyn_req->file_list[4]);
		return send_fileid_to_mcu();
	}
	else
	{
		hwlog_err("%s file_count = 0, not send file_id to muc\n", __func__);
		return -EINVAL;
	}
}

/*******************************************************************************************
Function:	sensor_set_cfg_data
Description: 其他配置信息，例如是否需要根据电池电流大小校准指南针
Data Accessed:  无
Data Updated:   无
Input:        无
Output:         无
Return:         成功或者失败信息: 0->成功, -1->失败
*******************************************************************************************/
static int sensor_set_current_info(void)
{
	int ret = 0;
	u8 need_current = 0;
	write_info_t	pkg_ap;

	memset(&pkg_ap, 0, sizeof(pkg_ap));

	need_current = check_if_need_current();

	pkg_ap.tag=TAG_CURRENT;
	pkg_ap.cmd=CMD_CURRENT_CFG_REQ;
	pkg_ap.wr_buf=&need_current;
	pkg_ap.wr_len=sizeof(need_current);
	ret = write_customize_cmd(&pkg_ap,  NULL);
	if (ret)
	{
		hwlog_err("send current cfg data to mcu fail,ret=%d\n", ret);
		return -1;
	}
	else
	{
		hwlog_info( "set current cfg data to mcu success\n");
	}

	return 0;
}


/*******************************************************************************************
Function:	sensor_set_cfg_data
Description: 将配置参数发至mcu 侧
Data Accessed:  无
Data Updated:   无
Input:        无
Output:         无
Return:         成功或者失败信息: 0->成功, -1->失败
*******************************************************************************************/

static int sensor_set_cfg_data(void)
{
	int ret = 0;
	write_info_t	pkg_ap;
	read_info_t	pkg_mcu;

	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));

	ret = sensor_set_current_info();
	if (ret)
	{
		hwlog_err("sensor_set_current_info fail, ret = %d\n", ret);
	}

	//g-sensor
	pkg_ap.tag=TAG_ACCEL;
	pkg_ap.cmd=CMD_ACCEL_PARAMET_REQ;
	pkg_ap.wr_buf=&gsensor_data;
	pkg_ap.wr_len=sizeof(gsensor_data);
	ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
	if(ret)
	{
		hwlog_err("send gsensor cfg data to mcu fail,ret=%d\n", ret);
	}
	else
	{
		if(pkg_mcu.errno!=0) hwlog_err("set gsensor cfg fail\n");
		else
		{
			hwlog_info( "set gsensor cfg data to mcu success\n");
#ifdef CONFIG_HUAWEI_HW_DEV_DCT
			/* detect current device successful, set the flag as present */
			set_hw_dev_flag(DEV_I2C_G_SENSOR);
#endif
		}
	}
	//gyro
	pkg_ap.tag=TAG_GYRO;
	pkg_ap.cmd=CMD_GYRO_PARAMET_REQ;
	pkg_ap.wr_buf=&gyro_data;
	pkg_ap.wr_len=sizeof(gyro_data);
	ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
	if(ret)
	{
		hwlog_err("send gyro cfg data to mcu fail,ret=%d\n", ret);
	}
	else
	{
		if(pkg_mcu.errno!=0) hwlog_err("set gyro cfg fail\n");
		else
		{
			hwlog_info( "set gyro cfg data to mcu success\n");
#ifdef CONFIG_HUAWEI_HW_DEV_DCT
			/* detect current device successful, set the flag as present */
			set_hw_dev_flag(DEV_I2C_GYROSCOPE);
#endif
		}
	}
	//mag
	pkg_ap.tag=TAG_MAG;
	pkg_ap.cmd=CMD_MAG_PARAMET_REQ;
	pkg_ap.wr_buf=&mag_data;
	pkg_ap.wr_len=sizeof(mag_data);
	ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
	if(ret)
	{
		hwlog_err("send magg data to mcu fail,ret=%d\n", ret);
	}
	else
	{
		if(pkg_mcu.errno!=0) hwlog_err("set mag cfg fail\n");
		else
		{
			hwlog_info( "set mag cfg data to mcu success\n");
#ifdef CONFIG_HUAWEI_HW_DEV_DCT
			/* detect current device successful, set the flag as present */
			set_hw_dev_flag(DEV_I2C_COMPASS);
#endif
		}
	}
	register_mcu_event_notifier(TAG_MAG, CMD_MAG_SEND_CALIBRATE_TO_AP_REQ, mag_calibrate_data_from_mcu);

	//als
	pkg_ap.tag=TAG_ALS;
	pkg_ap.cmd=CMD_ALS_PARAMET_REQ;
	pkg_ap.wr_buf=&als_data;
	pkg_ap.wr_len=sizeof(als_data);
	ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
	if(ret)
	{
		hwlog_err("send als cfg data to mcu fail,ret=%d\n", ret);
	}
	else
	{
		if(pkg_mcu.errno!=0) hwlog_err("set als cfg fail\n");
		else
		{
			hwlog_info( "set als cfg data to mcu success\n");
		}
	}
	//ps
	pkg_ap.tag=TAG_PS;
	pkg_ap.cmd=CMD_PS_PARAMET_REQ;
	pkg_ap.wr_buf=&ps_data;
	pkg_ap.wr_len=sizeof(ps_data);
	ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
	if(ret)
	{
		hwlog_err("send ps cfg data to mcu fail,ret=%d\n", ret);
	}
	else
	{
		if(pkg_mcu.errno!=0) hwlog_err("set ps cfg fail\n");
		else
		{
			hwlog_info( "set ps cfg data to mcu success\n");
#ifdef CONFIG_HUAWEI_HW_DEV_DCT
			/* detect current device successful, set the flag as present */
			set_hw_dev_flag(DEV_I2C_L_SENSOR);
#endif
		}
	}

	return ret;
}

static ssize_t sensor_show_acc_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%s\n", sensor_chip_info[ACC]);
}
static DEVICE_ATTR(acc_info, S_IRUGO,
				   sensor_show_acc_info, NULL);
static ssize_t sensor_show_mag_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%s\n", sensor_chip_info[MAG]);
}
static DEVICE_ATTR(mag_info, S_IRUGO,
				   sensor_show_mag_info, NULL);
static ssize_t sensor_show_gyro_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%s\n", sensor_chip_info[GYRO]);
}
static DEVICE_ATTR(gyro_info, S_IRUGO,
				   sensor_show_gyro_info, NULL);
static ssize_t sensor_show_als_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%s\n", sensor_chip_info[ALS]);
}
static DEVICE_ATTR(als_info, S_IRUGO,
				   sensor_show_als_info, NULL);

static ssize_t sensor_show_ps_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%s\n", sensor_chip_info[PS]);
}
static DEVICE_ATTR(ps_info, S_IRUGO,
				   sensor_show_ps_info, NULL);

static ssize_t show_gyro_selfTest_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%s\n", sensor_status.gyro_selfTest_result);
}

static ssize_t attr_set_gyro_selftest(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int err = -1;
	write_info_t	pkg_ap;
	read_info_t	pkg_mcu;

	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if(1==val)
	{
		pkg_ap.tag=TAG_GYRO;
		pkg_ap.cmd=CMD_GYRO_SELFTEST_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		err=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(err)
		{
			hwlog_err("send gyro selftest cmd to mcu fail,ret=%d\n", err);
		}
		if(pkg_mcu.errno!=0)
		{
			hwlog_err("gyro selftest fail\n");
			memcpy(sensor_status.gyro_selfTest_result, "0", 2);
		}else
		{
			hwlog_info("gyro selftest  success, data len=%d\n", pkg_mcu.data_length);
			memcpy(sensor_status.gyro_selfTest_result, "1", 2);
		}
	}
	return size;
}

static DEVICE_ATTR(gyro_selfTest, 0664, show_gyro_selfTest_result, attr_set_gyro_selftest);
static ssize_t show_mag_selfTest_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%s\n", sensor_status.mag_selfTest_result);
}

static ssize_t write_mag_selfTest_result(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val = 0;
	int ret=0;
	write_info_t	pkg_ap;
	read_info_t	pkg_mcu;

	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if(1==val)
	{
		pkg_ap.tag=TAG_MAG;
		pkg_ap.cmd=CMD_MAG_SELFTEST_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send mag selftest cmd to mcu fail,ret=%d\n", ret);
		}
		if(pkg_mcu.errno!=0)
		{
			hwlog_err("mag selftest fail\n");
			memcpy(sensor_status.mag_selfTest_result, "0", 2);
		}else
		{
			hwlog_info("mag selftest  success, data len=%d\n", pkg_mcu.data_length);
			memcpy(sensor_status.mag_selfTest_result, "1", 2);
		}
		//hwlog_info("--mag_selfTest_result=%d---\n", mag_selfTest_result);
	}
	return count;
}
static DEVICE_ATTR(mag_selfTest, 0664,
				   show_mag_selfTest_result, write_mag_selfTest_result);

static ssize_t i2c_rw_pi(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val = 0;
	int ret=0;
	write_info_t	pkg_ap;
	read_info_t	pkg_mcu;
	pkt_i2c_read_req_t pkt_i2c_read;

	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	memset(&pkt_i2c_read, 0, sizeof(pkt_i2c_read));
	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;
	//##(bus_num)##(i2c_addr)##(reg_addr)##(len)
	pkt_i2c_read.busid=(val>>24)&0xff;
	pkt_i2c_read.addr=(val>>16)&0xff;
	pkt_i2c_read.reg=(val>>8)&0xff;
	pkt_i2c_read.reg_type=0;
	pkt_i2c_read.length=val&0xff;
	pkg_ap.tag=TAG_I2C;
	pkg_ap.cmd=CMD_I2C_READ_REQ;
	pkg_ap.wr_buf=&pkt_i2c_read.busid;
	pkg_ap.wr_len=sizeof(pkt_i2c_read)-sizeof(pkt_i2c_read.hd);
	hwlog_info("i2c send data id=%x, i2c_add=%x, reg_add=%x, len=%x\n", pkt_i2c_read.busid, pkt_i2c_read.addr, pkt_i2c_read.reg, pkt_i2c_read.length);
	ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
	if(ret)
	{
		hwlog_err("detect_i2c_device:send i2c read cmd to mcu fail,ret=%d\n", ret);
		return -1;
	}
	if(pkg_mcu.errno!=0)
	{
		hwlog_err("detect_i2c_device:i2c read fail\n");
		return -1;
	}else
	{
		hwlog_info("i2c read  success, data len=%d, data=%x\n", pkg_mcu.data_length, pkg_mcu.data[0]);
		return 0;
	}
}
static DEVICE_ATTR(i2c_rw, 0664, NULL, i2c_rw_pi);

static ssize_t attr_acc_calibrate_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int val = return_calibration;
	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static int acc_calibrate_save(const void *buf, int length)
{
	char temp_buf[10] = {0};
	int ret=0;
	if(buf == NULL||length <= 0)
	{
		hwlog_err("%s invalid argument.", __func__);
		return_calibration=EXEC_FAIL;
		return -1;
	}
	memcpy(temp_buf, buf, length);
	hwlog_info( "%s:gsensor calibrate ok, %d  %d  %d \n", __func__, temp_buf[0],temp_buf[1],temp_buf[2]);
	ret = write_gsensor_offset_to_nv(temp_buf);
	if(ret)
	{
		hwlog_err("nv write fail.\n");
		return_calibration=NV_FAIL;
		return -1;
	}
	return_calibration=SUC;
	return 0;
}

static ssize_t attr_acc_calibrate_write(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val = 0;
	int ret = 0;
	write_info_t	pkg_ap;
	read_info_t	pkg_mcu;

	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if(1==val)
	{
		pkg_ap.tag=TAG_ACCEL;
		pkg_ap.cmd=CMD_ACCEL_SELFCALI_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			return_calibration=COMMU_FAIL;
			hwlog_err("send acc calibrate cmd to mcu fail,ret=%d\n", ret);
			return count;
		}
		if(pkg_mcu.errno!=0)
		{
			hwlog_err("acc calibrate fail\n");
			return_calibration=EXEC_FAIL;
		}
		else
		{
			hwlog_info("acc calibrate  success, data len=%d\n", pkg_mcu.data_length);
			acc_calibrate_save(pkg_mcu.data, pkg_mcu.data_length);
		}
	}
	return count;
}

static DEVICE_ATTR(acc_calibrate, 0664, attr_acc_calibrate_show, attr_acc_calibrate_write);
//acc enable node
static ssize_t show_acc_enable_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.status[TAG_ACCEL]);
}

static ssize_t attr_set_acc_enable(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = -1;
	write_info_t	pkg_ap;
	read_info_t pkg_mcu;

	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if(1==val)
	{
		pkg_ap.tag=TAG_ACCEL;
		pkg_ap.cmd=CMD_CMN_OPEN_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send gsensor enable cmd to mcu fail,ret=%d\n", ret);
		}
		if(pkg_mcu.errno!=0) hwlog_err("set gsensor enable fail\n");
		else hwlog_info("gsensor enable success\n");
	}else
	{
		pkg_ap.tag=TAG_ACCEL;
		pkg_ap.cmd=CMD_CMN_CLOSE_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send gsensor disable cmd to mcu fail,ret=%d\n", ret);
		}
		if(pkg_mcu.errno!=0) hwlog_err("set gsensor disable fail\n");
		else hwlog_info("gsensor disable success\n");
	}
	return size;
}

static DEVICE_ATTR(acc_enable, 0664, show_acc_enable_result, attr_set_acc_enable);

static ssize_t show_acc_delay_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.delay[TAG_ACCEL]);
}

static ssize_t attr_set_acc_delay(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = -1;
	write_info_t	pkg_ap;
	read_info_t pkg_mcu;

	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if(val>=10&& val<1000)
	{
		pkg_ap.tag=TAG_ACCEL;
		pkg_ap.cmd=CMD_CMN_INTERVAL_REQ;
		pkg_ap.wr_buf=&val;
		pkg_ap.wr_len=sizeof(val);
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send gsensor delay cmd to mcu fail,ret=%d\n", ret);
		}
		if(pkg_mcu.errno!=0) hwlog_err("set gsensor delay fail\n");
		else
		{
			hwlog_info("set gsensor delay (%ld)success\n", val);
		}
	}
	return size;
}

static DEVICE_ATTR(acc_setdelay, 0664, show_acc_delay_result, attr_set_acc_delay);

static ssize_t show_gyro_enable_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.status[TAG_GYRO]);
}

static ssize_t attr_set_gyro_enable(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = -1;
	write_info_t	pkg_ap;
	read_info_t pkg_mcu;

	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if(1==val)
	{
		pkg_ap.tag=TAG_GYRO;
		pkg_ap.cmd=CMD_CMN_OPEN_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send gyro enable cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set gyro enable fail\n");
		else hwlog_info("gyro enable success\n");
	}else
	{
		pkg_ap.tag=TAG_GYRO;
		pkg_ap.cmd=CMD_CMN_CLOSE_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send gyro disable cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set gyro disable fail\n");
		else hwlog_info("gyro disable success\n");
	}
	return size;
}

static DEVICE_ATTR(gyro_enable, 0664, show_gyro_enable_result, attr_set_gyro_enable);

static ssize_t show_gyro_delay_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.delay[TAG_GYRO]);
}

static ssize_t attr_set_gyro_delay(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = -1;
	write_info_t	pkg_ap;
	read_info_t pkg_mcu;

	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if(val>=10&& val<1000)
	{
		pkg_ap.tag=TAG_GYRO;
		pkg_ap.cmd=CMD_CMN_INTERVAL_REQ;
		pkg_ap.wr_buf=&val;
		pkg_ap.wr_len=sizeof(val);
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send gyro delay cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set gsensor delay fail\n");
		else
		{
			hwlog_info("set gyro delay (%ld)success\n", val);
		}
	}
	return size;
}

static DEVICE_ATTR(gyro_setdelay, 0664, show_gyro_delay_result, attr_set_gyro_delay);

static ssize_t show_mag_enable_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.status[TAG_MAG]);
}

static ssize_t attr_set_mag_enable(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = -1;
	write_info_t	pkg_ap;
	read_info_t pkg_mcu;

	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if(1==val)
	{
		pkg_ap.tag=TAG_MAG;
		pkg_ap.cmd=CMD_CMN_OPEN_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send mag enable cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set mag enable fail\n");
		else
		{
			hwlog_info("set mag enable success\n");
		}
	}else
	{
		pkg_ap.tag=TAG_MAG;
		pkg_ap.cmd=CMD_CMN_CLOSE_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send mag disable cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set mag disable fail\n");
		else
		{
			hwlog_info("set mag disable success\n");
		}
	}
	return size;
}

static DEVICE_ATTR(mag_enable, 0664, show_mag_enable_result, attr_set_mag_enable);

static ssize_t show_mag_delay_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.delay[TAG_MAG]);
}

static ssize_t attr_set_mag_delay(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = -1;
	write_info_t	pkg_ap;
	read_info_t pkg_mcu;

	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if(val>=10&& val<1000)
	{
		pkg_ap.tag=TAG_MAG;
		pkg_ap.cmd=CMD_CMN_INTERVAL_REQ;
		pkg_ap.wr_buf=&val;
		pkg_ap.wr_len=sizeof(val);
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send mag delay cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set mag delay fail\n");
		else
		{
			hwlog_info("set mag delay (%ld)success\n", val);
		}
	}
	return size;
}

static DEVICE_ATTR(mag_setdelay, 0664, show_mag_delay_result, attr_set_mag_delay);
static ssize_t show_als_enable_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.status[TAG_ALS]);
}

static ssize_t attr_set_als_enable(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = -1;
	write_info_t	pkg_ap;
	read_info_t pkg_mcu;

	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if(1==val)
	{
		pkg_ap.tag=TAG_ALS;
		pkg_ap.cmd=CMD_CMN_OPEN_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send als enable cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set als enable fail\n");
		else
		{
			hwlog_info("set als enable success\n");
		}
	}else
	{
		pkg_ap.tag=TAG_ALS;
		pkg_ap.cmd=CMD_CMN_CLOSE_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send als disable cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set als disable fail\n");
		else
		{
			hwlog_info("set als disable success\n");
		}
	}
	return size;
}

static DEVICE_ATTR(als_enable, 0664, show_als_enable_result, attr_set_als_enable);

static ssize_t show_als_delay_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.delay[TAG_ALS]);
}

static ssize_t attr_set_als_delay(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = -1;
	write_info_t	pkg_ap;
	read_info_t pkg_mcu;

	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if(val>=10&& val<1000)
	{
		pkg_ap.tag=TAG_ALS;
		pkg_ap.cmd=CMD_CMN_INTERVAL_REQ;
		pkg_ap.wr_buf=&val;
		pkg_ap.wr_len=sizeof(val);
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send als delay cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set als delay fail\n");
		else
		{
			hwlog_info("set als delay (%ld)success\n", val);
		}
	}
	return size;
}
static DEVICE_ATTR(als_setdelay, 0664, show_als_delay_result, attr_set_als_delay);

static ssize_t show_ps_enable_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.status[TAG_PS]);
}

static ssize_t attr_set_ps_enable(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = -1;
	write_info_t	pkg_ap;
	read_info_t pkg_mcu;

	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if(1==val)
	{
		pkg_ap.tag=TAG_PS;
		pkg_ap.cmd=CMD_CMN_OPEN_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send ps enable cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set ps enable fail\n");
		else
		{
			hwlog_info("set ps enable success\n");
		}
	}else
	{
		pkg_ap.tag=TAG_PS;
		pkg_ap.cmd=CMD_CMN_CLOSE_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send ps disable cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set ps disable fail\n");
		else
		{
			hwlog_info("set ps disable success\n");
		}
	}
	return size;
}

static DEVICE_ATTR(ps_enable, 0664, show_ps_enable_result, attr_set_ps_enable);
static ssize_t show_ps_delay_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.delay[TAG_PS]);
}

static ssize_t attr_set_ps_delay(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = -1;
	write_info_t	pkg_ap;
	read_info_t pkg_mcu;

	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if(val>=10&& val<1000)
	{
		pkg_ap.tag=TAG_PS;
		pkg_ap.cmd=CMD_CMN_INTERVAL_REQ;
		pkg_ap.wr_buf=&val;
		pkg_ap.wr_len=sizeof(val);
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send ps delay cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set ps delay fail\n");
		else
		{
			hwlog_info("set ps delay (%ld)success\n", val);
		}
	}
	return size;
}
static DEVICE_ATTR(ps_setdelay, 0664, show_ps_delay_result, attr_set_ps_delay);

static ssize_t show_os_enable_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.status[TAG_ORIENTATION]);
}

static ssize_t attr_set_os_enable(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = -1;
	write_info_t	pkg_ap;
	read_info_t pkg_mcu;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if(1==val)
	{
		pkg_ap.tag=TAG_ORIENTATION;
		pkg_ap.cmd=CMD_CMN_OPEN_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send os enable cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set os enable fail\n");
		else
		{
			hwlog_info("set os enable success\n");
		}
	}else
	{
		pkg_ap.tag=TAG_ORIENTATION;
		pkg_ap.cmd=CMD_CMN_CLOSE_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send os disable cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set os disable fail\n");
		else
		{
			hwlog_info("set os disable success\n");
		}
	}
	return size;
}

static DEVICE_ATTR(os_enable, 0664, show_os_enable_result, attr_set_os_enable);
static ssize_t show_os_delay_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.delay[TAG_ORIENTATION]);
}

static ssize_t attr_set_os_delay(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = -1;
	write_info_t	pkg_ap;
	read_info_t pkg_mcu;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if(val>=10&& val<1000)
	{
		pkg_ap.tag=TAG_ORIENTATION;
		pkg_ap.cmd=CMD_CMN_INTERVAL_REQ;
		pkg_ap.wr_buf=&val;
		pkg_ap.wr_len=sizeof(val);
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send os delay cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set os delay fail\n");
		else
		{
			hwlog_info("set os delay (%ld)success\n", val);
		}
	}
	return size;
}
static DEVICE_ATTR(os_setdelay, 0664, show_os_delay_result, attr_set_os_delay);

//line
static ssize_t show_lines_enable_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.status[TAG_LINEAR_ACCEL]);
}

static ssize_t attr_set_lines_enable(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = -1;
	write_info_t	pkg_ap;
	read_info_t pkg_mcu;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if(1==val)
	{
		pkg_ap.tag=TAG_LINEAR_ACCEL;
		pkg_ap.cmd=CMD_CMN_OPEN_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send lines enable cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set lines enable fail\n");
		else
		{
			hwlog_info("set lines enable success\n");
		}
	}else
	{
		pkg_ap.tag=TAG_LINEAR_ACCEL;
		pkg_ap.cmd=CMD_CMN_CLOSE_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send lines disable cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set lines disable fail\n");
		else
		{
			hwlog_info("set lines disable success\n");
		}
	}
	return size;
}

static DEVICE_ATTR(lines_enable, 0664, show_lines_enable_result, attr_set_lines_enable);
static ssize_t show_lines_delay_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.delay[TAG_LINEAR_ACCEL]);
}

static ssize_t attr_set_lines_delay(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = -1;
	write_info_t	pkg_ap;
	read_info_t pkg_mcu;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if(val>=10&& val<1000)
	{
		pkg_ap.tag=TAG_LINEAR_ACCEL;
		pkg_ap.cmd=CMD_CMN_INTERVAL_REQ;
		pkg_ap.wr_buf=&val;
		pkg_ap.wr_len=sizeof(val);
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send lines delay cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set lines delay fail\n");
		else
		{
			hwlog_info("set lines delay (%ld)success\n", val);
		}
	}
	return size;
}
static DEVICE_ATTR(lines_setdelay, 0664, show_lines_delay_result, attr_set_lines_delay);
//gra
static ssize_t show_gras_enable_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.status[TAG_GRAVITY]);
}

static ssize_t attr_set_gras_enable(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = -1;
	write_info_t	pkg_ap;
	read_info_t pkg_mcu;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if(1==val)
	{
		pkg_ap.tag=TAG_GRAVITY;
		pkg_ap.cmd=CMD_CMN_OPEN_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send gras enable cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set gras enable fail\n");
		else
		{
			hwlog_info("set gras enable success\n");
		}
	}else
	{
		pkg_ap.tag=TAG_GRAVITY;
		pkg_ap.cmd=CMD_CMN_CLOSE_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send gras disable cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set gras disable fail\n");
		else
		{
			hwlog_info("set gras disable success\n");
		}
	}
	return size;
}

static DEVICE_ATTR(gras_enable, 0664, show_gras_enable_result, attr_set_gras_enable);
static ssize_t show_gras_delay_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.delay[TAG_GRAVITY]);
}

static ssize_t attr_set_gras_delay(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = -1;
	write_info_t	pkg_ap;
	read_info_t pkg_mcu;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if(val>=10&& val<1000)
	{
		pkg_ap.tag=TAG_GRAVITY;
		pkg_ap.cmd=CMD_CMN_INTERVAL_REQ;
		pkg_ap.wr_buf=&val;
		pkg_ap.wr_len=sizeof(val);
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send gras delay cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set gras delay fail\n");
		else
		{
			hwlog_info("set gras delay (%ld)success\n", val);
		}
	}
	return size;
}
static DEVICE_ATTR(gras_setdelay, 0664, show_gras_delay_result, attr_set_gras_delay);
//rv
static ssize_t show_rvs_enable_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.status[TAG_ROTATION_VECTORS]);
}

static ssize_t attr_set_rvs_enable(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = -1;
	write_info_t	pkg_ap;
	read_info_t pkg_mcu;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if(1==val)
	{
		pkg_ap.tag=TAG_ROTATION_VECTORS;
		pkg_ap.cmd=CMD_CMN_OPEN_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send rvs enable cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set rvs enable fail\n");
		else
		{
			hwlog_info("set rvs enable success\n");
		}
	}else
	{
		pkg_ap.tag=TAG_ROTATION_VECTORS;
		pkg_ap.cmd=CMD_CMN_CLOSE_REQ;
		pkg_ap.wr_buf=NULL;
		pkg_ap.wr_len=0;
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send rvs disable cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set rvs disable fail\n");
		else
		{
			hwlog_info("set rvs disable success\n");
		}
	}
	return size;
}

static DEVICE_ATTR(rvs_enable, 0664, show_rvs_enable_result, attr_set_rvs_enable);
static ssize_t show_rvs_delay_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_STR_SIZE, "%d\n", sensor_status.delay[TAG_ROTATION_VECTORS]);
}

static ssize_t attr_set_rvs_delay(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = -1;
	write_info_t	pkg_ap;
	read_info_t pkg_mcu;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	memset(&pkg_ap, 0, sizeof(pkg_ap));
	memset(&pkg_mcu, 0, sizeof(pkg_mcu));
	if(val>=10&& val<1000)
	{
		pkg_ap.tag=TAG_ROTATION_VECTORS;
		pkg_ap.cmd=CMD_CMN_INTERVAL_REQ;
		pkg_ap.wr_buf=&val;
		pkg_ap.wr_len=sizeof(val);
		ret=write_customize_cmd(&pkg_ap,  &pkg_mcu);
		if(ret)
		{
			hwlog_err("send rvs delay cmd to mcu fail,ret=%d\n", ret);
			return size;
		}
		if(pkg_mcu.errno!=0) hwlog_err("set rvs delay fail\n");
		else
		{
			hwlog_info("set rvs delay (%ld)success\n", val);
		}
	}
	return size;
}
static DEVICE_ATTR(rvs_setdelay, 0664, show_rvs_delay_result, attr_set_rvs_delay);

static struct attribute *sensor_attributes[] = {
	&dev_attr_acc_info.attr,
	&dev_attr_mag_info.attr,
	&dev_attr_gyro_info.attr,
	&dev_attr_ps_info.attr,
	&dev_attr_als_info.attr,
	&dev_attr_gyro_selfTest.attr,
	&dev_attr_mag_selfTest.attr,
	&dev_attr_i2c_rw.attr,
	&dev_attr_acc_calibrate.attr,
	&dev_attr_acc_enable.attr,
	&dev_attr_acc_setdelay.attr,
	&dev_attr_gyro_enable.attr,
	&dev_attr_gyro_setdelay.attr,
	&dev_attr_mag_enable.attr,
	&dev_attr_mag_setdelay.attr,
	&dev_attr_als_enable.attr,
	&dev_attr_als_setdelay.attr,
	&dev_attr_ps_enable.attr,
	&dev_attr_ps_setdelay.attr,
	&dev_attr_os_enable.attr,
	&dev_attr_os_setdelay.attr,
	&dev_attr_lines_enable.attr,
	&dev_attr_lines_setdelay.attr,
	&dev_attr_gras_enable.attr,
	&dev_attr_gras_setdelay.attr,
	&dev_attr_rvs_enable.attr,
	&dev_attr_rvs_setdelay.attr,
	NULL
};
static const struct attribute_group sensor_node = {
	.attrs = sensor_attributes,
};

static struct platform_device sensor_input_info = {
	.name = "huawei_sensor",
	.id = -1,
};

int mcu_sys_ready_callback(const pkt_header_t *head)
{
	int ret = 0;
	if (ST_MINSYSREADY == ((pkt_sys_statuschange_req_t *)head)->status)
	{
		hwlog_info("sys ready mini!\n");
		ret = init_sensors_cfg_data_from_dts();
		if(ret) hwlog_err("get sensors cfg data from dts fail,ret=%d, use default config data!\n", ret);
		else hwlog_info( "get sensors cfg data from dts success!\n");
	}else if (ST_MCUREADY == ((pkt_sys_statuschange_req_t *)head)->status)
	{
		hwlog_info("mcu all ready!\n");
		ret = sensor_set_cfg_data();
		if(ret<0) hwlog_err("sensor_chip_detect ret=%d\n", ret);
		unregister_mcu_event_notifier(TAG_SYS, CMD_SYS_STATUSCHANGE_REQ, mcu_sys_ready_callback);
	}
	return 0;
}
static int __init sensor_input_info_init(void)
{
	int ret = 0;
	hwlog_info("[%s] ++", __func__);

	ret = platform_device_register(&sensor_input_info);
	if (ret) {
		hwlog_err("%s: platform_device_register failed, ret:%d.\n",
				__func__, ret);
		goto REGISTER_ERR;
	}

	ret = sysfs_create_group(&sensor_input_info.dev.kobj, &sensor_node);
	if (ret) {
		hwlog_err("sensor_input_info_init sysfs_create_group error ret =%d", ret);
		goto SYSFS_CREATE_CGOUP_ERR;
	}
	hwlog_info("[%s] --", __func__);

	return 0;
SYSFS_CREATE_CGOUP_ERR:
	platform_device_unregister(&sensor_input_info);
REGISTER_ERR:
	return ret;

}

device_initcall(sensor_input_info_init);
MODULE_DESCRIPTION("sensor input info");
MODULE_AUTHOR("huawei driver group of K3V3");
MODULE_LICENSE("GPL");
