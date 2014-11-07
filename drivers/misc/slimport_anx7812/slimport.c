/*
 * Copyright(c) 2014, Analogix Semiconductor. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "slimport_odin.h"

/* Enable or Disable HDCP by default */
/* hdcp_enable = 1: Enable,  0: Disable */
static int hdcp_enable = 1;

/* HDCP switch for external block*/
/* external_block_en = 1: enable, 0: disable*/
int external_block_en = 0;

/* to access global platform data */
static struct anx7812_platform_data *g_pdata;

//#define SP_REGISTER_SET_TEST

#ifdef SP_REGISTER_SET_TEST
//For Slimport swing&pre-emphasis test
unchar val_SP_TX_LT_CTRL_REG0 ;
unchar val_SP_TX_LT_CTRL_REG10 ;
unchar val_SP_TX_LT_CTRL_REG11 ;
unchar val_SP_TX_LT_CTRL_REG2 ;
unchar val_SP_TX_LT_CTRL_REG12;
unchar val_SP_TX_LT_CTRL_REG1;
unchar val_SP_TX_LT_CTRL_REG6;
unchar val_SP_TX_LT_CTRL_REG16;
unchar val_SP_TX_LT_CTRL_REG5;
unchar val_SP_TX_LT_CTRL_REG8;
unchar val_SP_TX_LT_CTRL_REG15;
unchar val_SP_TX_LT_CTRL_REG18;
#endif

#define TRUE 1
#define FALSE 0

#ifdef CONFIG_SLIMPORT_ADVDD_POWER
static int slimport7816_avdd_power(unsigned int onoff);
static int slimport7816_dvdd_power(unsigned int onoff);
#endif

struct i2c_client *anx7812_client;

struct anx7812_data {
	struct anx7812_platform_data    *pdata;
	struct delayed_work    work;
	struct delayed_work    isr_work;
	struct workqueue_struct    *workqueue;
	struct mutex    lock;
	struct wake_lock slimport_lock;
	bool slimport_connected;
#ifdef ANX_I2C_SYNC_IRQ
	atomic_t suspend_state;
#endif
};

bool slimport_is_connected(void)
{
	struct anx7812_platform_data *pdata = NULL;
	bool result = false;

	if (!anx7812_client)
		return false;

#ifdef CONFIG_OF
	pdata = g_pdata;
#else
	pdata = anx7812_client->dev.platform_data;
#endif

	if (!pdata)
		return false;

	if (gpio_get_value_cansleep(pdata->gpio_cbl_det)) {
		mdelay(10);
		if (gpio_get_value_cansleep(pdata->gpio_cbl_det)) {
			SLIMPORT_DBG_INFO("Slimport Dongle is detected\n");
			result = true;
		}
	}

	return result;
}
EXPORT_SYMBOL(slimport_is_connected);

/*            
                
                                   
 */
#ifdef CONFIG_SLIMPORT_ADVDD_POWER
static int slimport7816_avdd_power(unsigned int onoff)
{
#ifdef CONFIG_OF
	struct anx7812_platform_data *pdata = g_pdata;
#else
	struct anx7812_platform_data *pdata = anx7812_client->dev.platform_data;
#endif
	int rc = 0;

/* To do : regulator control after H/W change */
	return rc;
	if (onoff) {
		SLIMPORT_DBG_INFO("avdd power on\n");
		rc = regulator_enable(pdata->avdd_10);
		if (rc < 0) {
			SLIMPORT_ERR("failed to enable avdd regulator rc=%d\n", rc);
		}
	} else {
			SLIMPORT_DBG_INFO("avdd power off\n");
			rc = regulator_disable(pdata->avdd_10);
	}

	return rc;
}

static int slimport7816_dvdd_power(unsigned int onoff)
{
#ifdef CONFIG_OF
	struct anx7812_platform_data *pdata = g_pdata;
#else
	struct anx7812_platform_data *pdata = anx7812_client->dev.platform_data;
#endif
	int rc = 0;

/* To do : regulator control after H/W change */
	return rc;
	if (onoff) {
		SLIMPORT_DBG_INFO("dvdd power on\n");
		rc = regulator_enable(pdata->dvdd_10);
		if (rc < 0) {
			SLIMPORT_ERR("failed to enable dvdd regulator rc=%d\n", rc);
		}
	} else {
			SLIMPORT_DBG_INFO("dvdd power off\n");
			rc = regulator_disable(pdata->dvdd_10);
	}

	return rc;
}
#endif

#ifdef SLIMPORT_ENABLE_IRQ_AT_BOOTTIME
static struct anx7812_data *g_anx7812;
static bool irq_enabled = 0;
static irqreturn_t anx7812_cbl_det_isr(int irq, void *data);

static int anx7812_enable_irq_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "%d\n", irq_enabled);
}

static int anx7812_enable_irq_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	int ret;
	long val;

	if (!anx7812_client)
		return -ENODEV;

	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;

	SLIMPORT_DBG_INFO(" irq_enabled = %d, val = %d\n",irq_enabled, val);
	if (irq_enabled == val)
		return ret;
	irq_enabled = val;
	SLIMPORT_DBG_INFO(" irq_enabled = %d\n",irq_enabled);
	if (irq_enabled) {
//kyukyu
		anx7812_cbl_det_isr(anx7812_client->irq, g_anx7812);
		enable_irq(anx7812_client->irq);
	}
	else
		disable_irq(anx7812_client->irq);
	return count;
}
#endif
static int slimport7816_rev_check_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "%d\n", 0);
}
static int slimport7816_rev_check_store(
	struct device *dev, struct device_attribute *attr,
	 const char *buf, int count)
{
	int cmd;

	sscanf(buf, "%d", &cmd);
	switch (cmd) {
	case 1:
		//sp_tx_chip_located();
		break;
	}
	return count;
}
/*sysfs interface : Enable or Disable HDCP by default*/
static int sp_hdcp_feature_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "%d\n", hdcp_enable);
}

static int sp_hdcp_feature_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, int count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;
	hdcp_enable = val;
	SLIMPORT_DBG_INFO("hdcp_enable = %d\n",hdcp_enable);
	return count;
}

/*sysfs  interface : HDCP switch for VGA dongle*/
static int sp_external_block_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "%d\n", external_block_en);
}

static int sp_external_block_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, int count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;
	external_block_en = val;
	return count;
}

/*sysfs  interface : i2c_master_read_reg, i2c_master_write_reg
anx7730 addr id:: DP_rx(0x50:0, 0x8c:1) HDMI_tx(0x72:5, 0x7a:6, 0x70:7)
ex:read ) 05df   = read:0  id:5 reg:0xdf
ex:write) 15df5f = write:1 id:5 reg:0xdf val:0x5f
*/
static int anx7730_write_reg_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "%d\n", 0);
}
static int anx7730_write_reg_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, int count)
{
	int ret = 0;
	char op, i;
	char r[3];
	char v[3];
	unchar tmp;
	int id, reg, val = 0 ;

	if (sp_tx_cur_states() != STATE_PLAY_BACK){
		SLIMPORT_ERR("error!, Not STATE_PLAY_BACK\n");
		return -EINVAL;
	}

	if(sp_tx_cur_cable_type() != DWN_STRM_IS_HDMI_7730){
		SLIMPORT_ERR("error!, rx is not anx7730\n");
		return -EINVAL;
	}

	if(count != 7 && count != 5){
		SLIMPORT_ERR("cnt:%d, invalid input!\n", count-1);
		SLIMPORT_ERR("ex) 05df   -> op:0(read)  id:5 reg:0xdf \n");
		SLIMPORT_ERR("ex) 15df5f -> op:1(wirte) id:5 reg:0xdf val:0x5f\n");
		return -EINVAL;
	}

	ret = snprintf(&op,2,buf);
	ret = snprintf(&i,2,buf+1);
	ret = snprintf(r,3,buf+2);

	id = simple_strtoul(&i,NULL,10);
	reg = simple_strtoul(r,NULL,16);

	if ((id != 0 && id != 1 && id != 5 && id != 6 && id != 7 )){
		SLIMPORT_ERR("invalid addr id! (id:0,1,5,6,7)\n");
		return -EINVAL;
	}

	switch(op){
		case 0x30: /* "0" -> read */
			i2c_master_read_reg(id,reg,&tmp);
			SLIMPORT_DBG_INFO("anx7730 read(%d,0x%x)= 0x%x \n",id,reg,tmp);
			break;

		case 0x31: /* "1" -> write */
			ret = snprintf(v,3,buf+4);
			val = simple_strtoul(v,NULL,16);

			i2c_master_write_reg(id,reg,val);
			i2c_master_read_reg(id,reg,&tmp);
			SLIMPORT_DBG_INFO("anx7730 write(%d,0x%x,0x%x)\n",id,reg,tmp);
			break;

		default:
			SLIMPORT_ERR("invalid operation code! (0:read, 1:write)\n");
			return -EINVAL;
	}

	return count;
}

/*sysfs  interface : sp_read_reg, sp_write_reg
anx7812 addr id:: HDMI_rx(0x7e:0, 0x80:1) DP_tx(0x72:5, 0x7a:6, 0x70:7)
ex:read ) 05df   = read:0  id:5 reg:0xdf
ex:write) 15df5f = write:1 id:5 reg:0xdf val:0x5f
*/
static int anx7812_id_change(int id){
	int chg_id = 0;

	switch(id){
		case 0: chg_id =  RX_P0;
			break;
		case 1: chg_id =  RX_P1;
			break;
		case 5: chg_id = TX_P2;
			break;
		case 6: chg_id = TX_P1;
			break;
		case 7: chg_id = TX_P0;
			break;
	}
	return chg_id;
}

static int anx7812_write_reg_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "%d\n", 0);
}
static int anx7812_write_reg_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, int count)
{
	int ret = 0;
	char op, i;
	char r[3];
	char v[3];
	unchar tmp;
	int id, reg, val = 0 ;

	if (sp_tx_cur_states() != STATE_PLAY_BACK){
		SLIMPORT_ERR("error!, Not STATE_PLAY_BACK\n");
		return -EINVAL;
	}

	if(count != 7 && count != 5){
		SLIMPORT_ERR("cnt:%d, invalid input!\n", count-1);
		SLIMPORT_ERR("ex) 05df   -> op:0(read)  id:5 reg:0xdf \n");
		SLIMPORT_ERR("ex) 15df5f -> op:1(wirte) id:5 reg:0xdf val:0x5f\n");
		return -EINVAL;
	}

	ret = snprintf(&op,2,buf);
	ret = snprintf(&i,2,buf+1);
	ret = snprintf(r,3,buf+2);

	id = simple_strtoul(&i,NULL,10);
	reg = simple_strtoul(r,NULL,16);

	if ((id != 0 && id != 1 && id != 5 && id != 6 && id != 7 )){
		SLIMPORT_ERR("invalid addr id! (id:0,1,5,6,7)\n");
		return -EINVAL;
	}

	id = anx7812_id_change(id); //ex) 5 -> 0x72

	switch(op){
		case 0x30: /* "0" -> read */
			sp_read_reg(id, reg, &tmp);
			SLIMPORT_DBG_INFO("anx7812 read(0x%x,0x%x)= 0x%x \n",id,reg,tmp);
			break;

		case 0x31: /* "1" -> write */
			ret = snprintf(v,3,buf+4);
			val = simple_strtoul(v,NULL,16);

			sp_write_reg(id, reg, val);
			sp_read_reg(id, reg, &tmp);
			SLIMPORT_DBG_INFO("anx7812 write(0x%x,0x%x,0x%x)\n",id,reg,tmp);
			break;

		default:
			SLIMPORT_ERR("invalid operation code! (0:read, 1:write)\n");
			return -EINVAL;
	}

	return count;
}

static ssize_t slimport_sysfs_rda_hdmi_vga(struct device *dev, struct device_attribute *attr,
	       char *buf)
{
	int ret;
	ret = is_slimport_vga();
	return sprintf(buf, "%d", ret);
}

#ifdef SP_REGISTER_SET_TEST //Slimport test
/*sysfs read interface*/
static int ctrl_reg0_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG0);
}

/*sysfs write interface*/
static int ctrl_reg0_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, int count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG0 = val;
	return count;
}

/*sysfs read interface*/
static int ctrl_reg10_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG10);
}

/*sysfs write interface*/
static int ctrl_reg10_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, int count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG10 = val;
	return count;
}

/*sysfs read interface*/
static int ctrl_reg11_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG11);
}

/*sysfs write interface*/
static int ctrl_reg11_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, int count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG11 = val;
	return count;
}

/*sysfs read interface*/
static int ctrl_reg2_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG2);
}

/*sysfs write interface*/
static int ctrl_reg2_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, int count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG2 = val;
	return count;
}

/*sysfs read interface*/
static int ctrl_reg12_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG12);
}

/*sysfs write interface*/
static int ctrl_reg12_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, int count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG12 = val;
	return count;
}

/*sysfs read interface*/
static int ctrl_reg1_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG1);
}

/*sysfs write interface*/
static int ctrl_reg1_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, int count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG1 = val;
	return count;
}

/*sysfs read interface*/
static int ctrl_reg6_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG6);
}

/*sysfs write interface*/
static int ctrl_reg6_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, int count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG6 = val;
	return count;
}

/*sysfs read interface*/
static int ctrl_reg16_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG16);
}

/*sysfs write interface*/
static int ctrl_reg16_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, int count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG16 = val;
	return count;
}

/*sysfs read interface*/
static int ctrl_reg5_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG5);
}

/*sysfs write interface*/
static int ctrl_reg5_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, int count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG5 = val;
	return count;
}

/*sysfs read interface*/
static int ctrl_reg8_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG8);
}

/*sysfs write interface*/
static int ctrl_reg8_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, int count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG8 = val;
	return count;
}

/*sysfs read interface*/
static int ctrl_reg15_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG15);
}

/*sysfs write interface*/
static int ctrl_reg15_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, int count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG15 = val;
	return count;
}

/*sysfs read interface*/
static int ctrl_reg18_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG18);
}

/*sysfs write interface*/
static int ctrl_reg18_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, int count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG18 = val;
	return count;
}
#endif
/* for debugging */
static struct device_attribute slimport_device_attrs[] = {
	__ATTR(rev_check, S_IRUGO | S_IWUSR, slimport7816_rev_check_show, slimport7816_rev_check_store),
	__ATTR(hdcp, S_IRUGO | S_IWUSR, sp_hdcp_feature_show, sp_hdcp_feature_store),
	__ATTR(hdcp_switch, S_IRUGO | S_IWUSR, sp_external_block_show, sp_external_block_store),
	__ATTR(hdmi_vga, S_IRUGO/*| S_IWUSR*/, slimport_sysfs_rda_hdmi_vga, NULL),
	__ATTR(anx7730, S_IRUGO | S_IWUSR, anx7730_write_reg_show, anx7730_write_reg_store),
	__ATTR(anx7812, S_IRUGO | S_IWUSR, anx7812_write_reg_show, anx7812_write_reg_store),
#ifdef SLIMPORT_ENABLE_IRQ_AT_BOOTTIME
	__ATTR(enable_irq, S_IRUGO | S_IWUSR, anx7812_enable_irq_show, anx7812_enable_irq_store),
#endif
#ifdef SP_REGISTER_SET_TEST//slimport test
	__ATTR(ctrl_reg0, S_IRUGO | S_IWUSR, ctrl_reg0_show, ctrl_reg0_store),
	__ATTR(ctrl_reg10, S_IRUGO | S_IWUSR, ctrl_reg10_show, ctrl_reg10_store),
	__ATTR(ctrl_reg11, S_IRUGO | S_IWUSR, ctrl_reg11_show, ctrl_reg11_store),
	__ATTR(ctrl_reg2, S_IRUGO | S_IWUSR, ctrl_reg2_show, ctrl_reg2_store),
	__ATTR(ctrl_reg12, S_IRUGO | S_IWUSR, ctrl_reg12_show, ctrl_reg12_store),
	__ATTR(ctrl_reg1, S_IRUGO | S_IWUSR, ctrl_reg1_show, ctrl_reg1_store),
	__ATTR(ctrl_reg6, S_IRUGO | S_IWUSR, ctrl_reg6_show, ctrl_reg6_store),
	__ATTR(ctrl_reg16, S_IRUGO | S_IWUSR, ctrl_reg16_show, ctrl_reg16_store),
	__ATTR(ctrl_reg5, S_IRUGO | S_IWUSR, ctrl_reg5_show, ctrl_reg5_store),
	__ATTR(ctrl_reg8, S_IRUGO | S_IWUSR, ctrl_reg8_show, ctrl_reg8_store),
	__ATTR(ctrl_reg15, S_IRUGO | S_IWUSR, ctrl_reg15_show, ctrl_reg15_store),
	__ATTR(ctrl_reg18, S_IRUGO | S_IWUSR, ctrl_reg18_show, ctrl_reg18_store),
#endif
};

static int create_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(slimport_device_attrs); i++)
		if (device_create_file(dev, &slimport_device_attrs[i]))
			goto error;
	return 0;
error:
	for ( ; i >= 0; i--)
		device_remove_file(dev, &slimport_device_attrs[i]);
	SLIMPORT_ERR("Unable to create interface");
	return -EINVAL;
}

int sp_read_reg_byte(uint8_t slave_addr, uint8_t offset)
{
	int ret = 0;

	anx7812_client->addr = (slave_addr >> 1);
	ret = i2c_smbus_read_byte_data(anx7812_client, offset);
	if (ret < 0) {
		SLIMPORT_ERR("failed to read i2c addr=%x\n", slave_addr);
		return ret;
	}
	return 0;
}

int sp_read_reg(uint8_t slave_addr, uint8_t offset, uint8_t *buf)
{
	int ret = 0;

	if (sp_tx_pd_mode) {
		SLIMPORT_DBG("tried to read the sp reg after power down\n");
		return -EIO;
	}

	anx7812_client->addr = (slave_addr >> 1);
	ret = i2c_smbus_read_byte_data(anx7812_client, offset);
	if (ret < 0) {
		SLIMPORT_ERR("failed to read i2c addr=%x\n", slave_addr);
		return ret;
	}
	*buf = (uint8_t) ret;

	return 0;
}

int sp_write_reg(uint8_t slave_addr, uint8_t offset, uint8_t value)
{
	int ret = 0;

	if (sp_tx_pd_mode) {
		SLIMPORT_DBG("tried to write the sp reg after power down\n");
		return -EIO;
	}

	anx7812_client->addr = (slave_addr >> 1);
	ret = i2c_smbus_write_byte_data(anx7812_client, offset, value);
	if (ret < 0) {
		SLIMPORT_ERR("failed to write i2c addr=%x\n", slave_addr);
	}
	return ret;
}

void sp_tx_hardware_poweron(void)
{
#ifdef CONFIG_OF
	struct anx7812_platform_data *pdata = g_pdata;
#else
	struct anx7812_platform_data *pdata = anx7812_client->dev.platform_data;
#endif

	gpio_set_value(pdata->gpio_reset, 0);
	msleep(1);

	if(pdata->gpio_v10_ctrl != 0) {
		/* Enable 1.0V LDO */
		gpio_set_value(pdata->gpio_v10_ctrl, 1);
		msleep(1);
	}

	gpio_set_value(pdata->gpio_p_dwn, 0);
	msleep(1);
	gpio_set_value(pdata->gpio_reset, 1);

	sp_tx_pd_mode = 0;

	SLIMPORT_DBG_INFO("anx7812 power on\n");
}

void sp_tx_hardware_powerdown(void)
{
#ifdef CONFIG_OF
	struct anx7812_platform_data *pdata = g_pdata;
#else
	struct anx7812_platform_data *pdata = anx7812_client->dev.platform_data;
#endif

	sp_tx_pd_mode = 1;

	gpio_set_value(pdata->gpio_reset, 0);
	msleep(1);
	if (pdata->gpio_v10_ctrl != 0) {
		gpio_set_value(pdata->gpio_v10_ctrl, 0);
		msleep(1);
	}
	gpio_set_value(pdata->gpio_p_dwn, 1);
	msleep(1);

	SLIMPORT_DBG_INFO("anx7812 power down\n");
}

int slimport_read_edid_block(int block, uint8_t *edid_buf)
{
	if (block == 0) {
		memcpy(edid_buf, edid_blocks, 128 * sizeof(char));
	} else if (block == 1) {
		memcpy(edid_buf, (edid_blocks + 128), 128 * sizeof(char));
	} else {
		SLIMPORT_ERR("block number %d is invalid\n", block);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(slimport_read_edid_block);

static void anx7812_free_gpio(struct anx7812_data *anx7812)
{
	gpio_free(anx7812->pdata->gpio_cbl_det);
	gpio_free(anx7812->pdata->gpio_reset);
	gpio_free(anx7812->pdata->gpio_p_dwn);
	if(anx7812->pdata->gpio_v10_ctrl != 0) {
		gpio_free(anx7812->pdata->gpio_v10_ctrl);
	}
#ifdef CONFIG_SLIMPORT_ADVDD_POWER
	if (anx7812->pdata->external_ldo_control) {
		gpio_free(anx7812->pdata->gpio_v33_ctrl);
	}
#endif
}

static int anx7812_init_gpio(struct anx7812_data *anx7812)
{
	int ret = 0;

	SLIMPORT_DBG_INFO("anx7812 init gpio\n");
	/*  gpio for chip power down  */
	ret = gpio_request(anx7812->pdata->gpio_p_dwn, "anx7812_p_dwn_ctl");
	if (ret) {
		SLIMPORT_ERR("failed to request gpio %d\n",
				anx7812->pdata->gpio_p_dwn);
		goto err0;
	}
	gpio_direction_output(anx7812->pdata->gpio_p_dwn, 1);
	/*  gpio for chip reset  */
	ret = gpio_request(anx7812->pdata->gpio_reset, "anx7812_reset_n");
	if (ret) {
		SLIMPORT_ERR("failed to request gpio %d\n",
				anx7812->pdata->gpio_reset);
		goto err1;
	}
	gpio_direction_output(anx7812->pdata->gpio_reset, 0);
	/*  gpio for slimport cable detect  */
	ret = gpio_request(anx7812->pdata->gpio_cbl_det, "anx7812_cbl_det");
	if (ret) {
		SLIMPORT_ERR("failed to request gpio %d\n",
				anx7812->pdata->gpio_cbl_det);
		goto err2;
	}
	gpio_direction_input(anx7812->pdata->gpio_cbl_det);
	/*  gpios for power control */
	if(anx7812->pdata->gpio_v10_ctrl != 0) {
		/* V10 power control */
		ret = gpio_request(anx7812->pdata->gpio_v10_ctrl,
							"anx7812_v10_ctrl");
		if (ret) {
		SLIMPORT_ERR("failed to request gpio %d\n",
				anx7812->pdata->gpio_v10_ctrl);
			goto err3;
		}
		gpio_direction_output(anx7812->pdata->gpio_v10_ctrl, 0);
	}
#ifdef CONFIG_SLIMPORT_ADVDD_POWER
	/* V33 power control */
	if (anx7812->pdata->external_ldo_control) {
		ret = gpio_request(anx7812->pdata->gpio_v33_ctrl,
							"anx7812_v33_ctrl");
		if (ret) {
			SLIMPORT_ERR("failed to request gpio %d\n",
				anx7812->pdata->gpio_v33_ctrl);
			goto err4;
		}
		gpio_direction_output(anx7812->pdata->gpio_v33_ctrl, 0);
		/* need to be check below */
		gpio_set_value(anx7812->pdata->gpio_v33_ctrl, 1);
	}
#endif

	goto out;


#ifdef CONFIG_SLIMPORT_ADVDD_POWER
err4:
	gpio_free(anx7812->pdata->gpio_v33_ctrl);
#endif
err3:
	gpio_free(anx7812->pdata->gpio_v10_ctrl);
err2:
	gpio_free(anx7812->pdata->gpio_cbl_det);
err1:
	gpio_free(anx7812->pdata->gpio_reset);
err0:
	gpio_free(anx7812->pdata->gpio_p_dwn);
out:
	return ret;
}

static int anx7812_system_init(void)
{
	int ret = 0;

	ret = slimport_chip_detect();
	if (ret == 0) {
		sp_tx_hardware_powerdown();
		SLIMPORT_ERR("failed to detect anx7812\n");
		return -ENODEV;
	}

	slimport_chip_initial();
	return 0;
}

static irqreturn_t anx7812_cbl_det_isr(int irq, void *data)
{
	struct anx7812_data *anx7812 = data;

#ifdef ANX_I2C_SYNC_IRQ
	switch (atomic_read(&anx7812->suspend_state)) {
		case I2C_RESUME:
			queue_delayed_work(anx7812->workqueue, &anx7812->isr_work, 0);
			break;

		case I2C_SUSPEND:
		case I2C_SUSPEND_IRQ:
			atomic_set(&anx7812->suspend_state, I2C_SUSPEND_IRQ);
			break;

		default:
			break;
	}
#else

	queue_delayed_work(anx7812->workqueue, &anx7812->isr_work, 0);
#endif

	return IRQ_HANDLED;
}
static void anx7812_isr_work_func(struct work_struct *work)
{
	if (gpio_get_value(g_anx7812->pdata->gpio_cbl_det)) {
		if (!g_anx7812->slimport_connected) {
	                g_anx7812->slimport_connected = true;
			wake_lock(&g_anx7812->slimport_lock);
			SLIMPORT_DBG_INFO("detect cable insertion\n");
			queue_delayed_work(g_anx7812->workqueue, &g_anx7812->work, 0);
		}
	} else {
		if (g_anx7812->slimport_connected) {
			g_anx7812->slimport_connected = false;
			SLIMPORT_DBG_INFO("detect cable removal\n");
			cancel_delayed_work_sync(&g_anx7812->work);
//			flush_workqueue(g_anx7812->workqueue);
			sp_tx_hardware_powerdown();
			sp_tx_clean_state_machine();
			wake_unlock(&g_anx7812->slimport_lock);
			wake_lock_timeout(&g_anx7812->slimport_lock, 2*HZ);
		}
	}
}

static void anx7812_work_func(struct work_struct *work)
{
	struct anx7812_data *td = container_of(work, struct anx7812_data,
								work.work);
	int workqueu_timer = 0;
	if(sp_tx_cur_states() >= STATE_PLAY_BACK)
		workqueu_timer = 500;
	else
		workqueu_timer = 100;
	mutex_lock(&td->lock);
	slimport_main_process();
	mutex_unlock(&td->lock);
	queue_delayed_work(td->workqueue, &td->work,
			msecs_to_jiffies(workqueu_timer));
}

/*            
                                    
                                   
 */
#ifdef CONFIG_OF
#ifdef CONFIG_SLIMPORT_ADVDD_POWER
int anx7812_regulator_configure(
	struct device *dev, struct anx7812_platform_data *pdata)
{
	int rc = 0;
/* To do : regulator control after H/W change */
	return rc;

	pdata->avdd_10 = regulator_get(dev, "analogix,vdd_ana");

	if (IS_ERR(pdata->avdd_10)) {
		rc = PTR_ERR(pdata->avdd_10);
		SLIMPORT_ERR("Regulator get failed avdd_10 rc=%d\n", rc);
		return rc;
	}

	if (regulator_count_voltages(pdata->avdd_10) > 0) {
		rc = regulator_set_voltage(pdata->avdd_10, 1000000,
							1000000);
		if (rc) {
			SLIMPORT_ERR("Regulator set_vtg failed rc=%d\n", rc);
			goto error_set_vtg_avdd_10;
		}
	}

	pdata->dvdd_10 = regulator_get(dev, "analogix,vdd_dig");
	if (IS_ERR(pdata->dvdd_10)) {
		rc = PTR_ERR(pdata->dvdd_10);
		SLIMPORT_ERR("Regulator get failed dvdd_10 rc=%d\n", rc);
		return rc;
	}

	if (regulator_count_voltages(pdata->dvdd_10) > 0) {
		rc = regulator_set_voltage(pdata->dvdd_10, 1000000,
							1000000);
		if (rc) {
			SLIMPORT_ERR("Regulator set_vtg failed rc=%d\n", rc);
			goto error_set_vtg_dvdd_10;
		}
	}

	return 0;

error_set_vtg_dvdd_10:
	regulator_put(pdata->dvdd_10);
error_set_vtg_avdd_10:
	regulator_put(pdata->avdd_10);

	return rc;
}
#endif

static int anx7812_parse_dt(
	struct device *dev, struct anx7812_platform_data *pdata)
{
	int rc = 0;
	struct device_node *np = dev->of_node;

	rc = of_property_read_u32(np, "analogix,p-dwn-gpio", &pdata->gpio_p_dwn);
	rc = of_property_read_u32(np, "analogix,reset-gpio", &pdata->gpio_reset);
	rc = of_property_read_u32(np, "analogix,cbl-det-gpio", &pdata->gpio_cbl_det);

	SLIMPORT_DBG_INFO("gpio p_dwn : %d, reset : %d, gpio_cbl_det : %d\n",
			pdata->gpio_p_dwn, pdata->gpio_reset, pdata->gpio_cbl_det);
	rc = of_property_read_u32(np, "analogix,v10-ctrl-gpio", &pdata->gpio_v10_ctrl);
	SLIMPORT_DBG_INFO("gpio gpio_v10_ctrl : %d\n", pdata->gpio_v10_ctrl);
	/*
	 * if "external-ldo-control" property is not exist, we
	 * assume that it is used in board.
	 * if don't use external ldo control,
	 * please use "external-ldo-control=<0>" in dtsi
	 */
#ifdef CONFIG_SLIMPORT_ADVDD_POWER
	rc = of_property_read_u32(np, "analogix,external-ldo-control",
		&pdata->external_ldo_control);
	if (rc == -EINVAL)
		pdata->external_ldo_control = 0;

	if (pdata->external_ldo_control) {

		rc = of_property_read_u32(np, "analogix,v33-ctrl-gpio", &pdata->gpio_v33_ctrl);
		SLIMPORT_DBG_INFO("gpio_v10_ctrl : %d, avdd33-en-gpio : %d\n",
			pdata->gpio_v10_ctrl, pdata->gpio_v33_ctrl);
	}

	if (anx7812_regulator_configure(dev, pdata) < 0) {
		SLIMPORT_ERR("parsing dt for anx7812 is failed.\n");
		return rc;
	}

	/* connects function nodes which are not provided with dts */
	pdata->avdd_power = slimport7816_avdd_power;
	pdata->dvdd_power = slimport7816_dvdd_power;
#endif

	return 0;
}
#else
static int anx7812_parse_dt(
	struct device *dev, struct anx7812_platform_data *pdata)
{
	return -ENODEV;
}
#endif

/*
int anx7812_get_sbl_cable_type(void)
{
	int cable_type = 0;
	unsigned int *p_cable_type = (unsigned int *)
		(smem_get_entry(SMEM_ID_VENDOR1, &cable_smem_size));

	if (p_cable_type)
		cable_type = *p_cable_type;
	else
		cable_type = 0;

	return cable_type;
}
*/
static int anx7812_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{

	struct anx7812_data *anx7812;
	struct anx7812_platform_data *pdata;
	int ret = 0;
//	int sbl_cable_type = 0;

	SLIMPORT_DBG_INFO("start\n");

#ifdef SP_REGISTER_SET_TEST
	val_SP_TX_LT_CTRL_REG0 = 0x19;
	val_SP_TX_LT_CTRL_REG10 = 0x00;
	val_SP_TX_LT_CTRL_REG11 = 0x00;
	val_SP_TX_LT_CTRL_REG2 = 0x36;
	val_SP_TX_LT_CTRL_REG12 = 0x00;
	val_SP_TX_LT_CTRL_REG1 = 0x26;
	val_SP_TX_LT_CTRL_REG6 = 0x3c;
	val_SP_TX_LT_CTRL_REG16 = 0x18;
	val_SP_TX_LT_CTRL_REG5 = 0x28;
	val_SP_TX_LT_CTRL_REG8 = 0x2F;
	val_SP_TX_LT_CTRL_REG15 = 0x10;
	val_SP_TX_LT_CTRL_REG18 = 0x1F;
#endif
	if (!i2c_check_functionality(client->adapter,
		I2C_FUNC_SMBUS_I2C_BLOCK)) {
		SLIMPORT_ERR("i2c bus does not support the anx7812\n");
		ret = -ENODEV;
		goto exit;
	}

	anx7812 = kzalloc(sizeof(struct anx7812_data), GFP_KERNEL);
	if (!anx7812) {
		SLIMPORT_ERR("failed to allocate driver data\n");
		ret = -ENOMEM;
		goto exit;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
							 sizeof(struct anx7812_platform_data),
							 GFP_KERNEL);
		if (!pdata) {
			SLIMPORT_ERR("Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err0;
		}
		client->dev.platform_data = pdata;
	/* device tree parsing function call */
		ret = anx7812_parse_dt(&client->dev, pdata);
		if (ret != 0) /* if occurs error */
			goto err0;

		anx7812->pdata = pdata;
	} else {
		anx7812->pdata = client->dev.platform_data;
	}

	/* to access global platform data */
	g_pdata = anx7812->pdata;

#ifdef SLIMPORT_ENABLE_IRQ_AT_BOOTTIME
	/*                    */
	g_anx7812 = anx7812;
#endif
	anx7812_client = client;

	mutex_init(&anx7812->lock);

	if (!anx7812->pdata) {
		ret = -EINVAL;
		goto err0;
	}

	ret = anx7812_init_gpio(anx7812);
	if (ret) {
		SLIMPORT_ERR("failed to initialize gpio\n");
		goto err0;
	}

	INIT_DELAYED_WORK(&anx7812->work, anx7812_work_func);
	INIT_DELAYED_WORK(&anx7812->isr_work, anx7812_isr_work_func);

	anx7812->workqueue = create_singlethread_workqueue("anx7812_work");
	if (anx7812->workqueue == NULL) {
		SLIMPORT_ERR("failed to create work queue\n");
		ret = -ENOMEM;
		goto err1;
	}

#ifdef CONFIG_SLIMPORT_ADVDD_POWER
	anx7812->pdata->avdd_power(1);
	anx7812->pdata->dvdd_power(1);
#endif
	ret = anx7812_system_init();
	if (ret) {
		SLIMPORT_ERR("failed to initialize anx7812\n");
		goto err2;
	}

	client->irq = gpio_to_irq(anx7812->pdata->gpio_cbl_det);
	if (client->irq < 0) {
		SLIMPORT_ERR("failed to get gpio irq\n");
		goto err2;
	}

	wake_lock_init(&anx7812->slimport_lock,
				WAKE_LOCK_SUSPEND,
				"slimport_wake_lock");

	/* sbl_cable_type = anx7812_get_sbl_cable_type(); */

	/*                                                
                                 
 */

	if (lge_get_factory_boot() && d2260_get_battery_temp() < -300) {
		SLIMPORT_DBG_INFO("slimport cbl_det pin request_threaded_irq skipped\n");
	} else {
		if(is_gic_direct_irq(client->irq) == 1) {
			ret = odin_gpio_sms_request_threaded_irq(client->irq, NULL, anx7812_cbl_det_isr,
						IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
						"anx7812", anx7812);
			SLIMPORT_DBG_INFO("slimport cbl_det pin sms_request_threaded_irq!!!\n");
		}
		else {
			ret = request_threaded_irq(client->irq, NULL, anx7812_cbl_det_isr,
						IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						"anx7812", anx7812);
		}
		if (ret  < 0) {
			SLIMPORT_ERR("failed to request irq\n");
			goto err2;
		}

#ifdef SLIMPORT_ENABLE_IRQ_AT_BOOTTIME
		/*                    */
		disable_irq(client->irq);
#endif

#ifdef ANX_I2C_SYNC_IRQ
		atomic_set(&anx7812->suspend_state, I2C_RESUME);
#endif

#if 0
		ret = irq_set_irq_wake(client->irq, 1);
		if (ret  < 0) {
			SLIMPORT_ERR("Request irq for cable detect");
			SLIMPORT_ERR("interrupt wake set fail\n");
			goto err3;
		}

		ret = enable_irq_wake(client->irq);
		if (ret  < 0) {
			SLIMPORT_ERR("Enable irq for cable detect");
			SLIMPORT_ERR("interrupt wake enable fail\n");
			goto err3;
		}
#endif
	}
	/* }else {
		SLIMPORT_ERR("%s, Disable cbl det irq!!\n",
			sbl_cable_type == CBL_910K ? "910K Cable Connected" : "Laf Mode");
	}
	*/

	ret = create_sysfs_interfaces(&client->dev);
	if (ret < 0) {
		SLIMPORT_ERR("sysfs register failed");
		goto err3;
	}

	SLIMPORT_DBG_INFO("end\n");
	goto exit;

err3:
	free_irq(client->irq, anx7812);
err2:
	destroy_workqueue(anx7812->workqueue);
err1:
	anx7812_free_gpio(anx7812);
err0:
	anx7812_client = NULL;
#ifdef SLIMPORT_ENABLE_IRQ_AT_BOOTTIME
	/*                    */
	g_anx7812 = NULL;
#endif
	kfree(anx7812);
exit:
	return ret;
}

static int anx7812_i2c_remove(struct i2c_client *client)
{
	struct anx7812_data *anx7812 = i2c_get_clientdata(client);
	int i = 0;
	for (i = 0; i < ARRAY_SIZE(slimport_device_attrs); i++)
		device_remove_file(&client->dev, &slimport_device_attrs[i]);
	free_irq(client->irq, anx7812);
	anx7812_free_gpio(anx7812);
	destroy_workqueue(anx7812->workqueue);
	wake_lock_destroy(&anx7812->slimport_lock);
#ifdef SLIMPORT_ENABLE_IRQ_AT_BOOTTIME
	/*                    */
	g_anx7812 = NULL;
#endif
	kfree(anx7812);
	return 0;
}

bool is_slimport_vga(void)
{
	return ((sp_tx_cur_cable_type() == DWN_STRM_IS_VGA_9832)
		|| (sp_tx_cur_cable_type() == DWN_STRM_IS_ANALOG)) ? 1 : 0;
}
/* 0x01: hdmi device is attached
    0x02: DP device is attached
    0x03: Old VGA device is attached // RX_VGA_9832
    0x04: new combo VGA device is attached // RX_VGA_GEN
    0x00: unknow device            */
EXPORT_SYMBOL(is_slimport_vga);
bool is_slimport_dp(void)
{
	return (sp_tx_cur_cable_type() == DWN_STRM_IS_DIGITAL) ? TRUE : FALSE;
}
EXPORT_SYMBOL(is_slimport_dp);
unchar sp_get_link_bw(void)
{
	return sp_tx_cur_bw();
}
EXPORT_SYMBOL(sp_get_link_bw);
void sp_set_link_bw(unchar link_bw)
{
	sp_tx_set_bw(link_bw);
}

static int anx7812_i2c_suspend(struct i2c_client *client, pm_message_t state)
{
/*                    */
#ifdef ANX_PDWN_CTL_GPIO_MODE
	gpio_direction_input(g_anx7812->pdata->gpio_p_dwn);
	SLIMPORT_DBG("anx_pdwn gpio direction change to input\n");
#endif

#ifdef ANX_I2C_SYNC_IRQ
	atomic_set(&g_anx7812->suspend_state, I2C_SUSPEND);
#endif

	return 0;
}


static int anx7812_i2c_resume(struct i2c_client *client)
{
/*                    */
#ifdef ANX_PDWN_CTL_GPIO_MODE
	gpio_direction_output(g_anx7812->pdata->gpio_p_dwn, 1);
	SLIMPORT_DBG("anx_pdwn gpio direction change to output\n");
#endif

#ifdef ANX_I2C_SYNC_IRQ
	if (atomic_read(&g_anx7812->suspend_state) == I2C_SUSPEND_IRQ) {
	        atomic_set(&g_anx7812->suspend_state, I2C_RESUME);
		queue_delayed_work(g_anx7812->workqueue, &g_anx7812->isr_work, msecs_to_jiffies(700));
		SLIMPORT_DBG("i2c_resume isr workqueue");

		return 0;
	}
	atomic_set(&g_anx7812->suspend_state, I2C_RESUME);
#endif
	return 0;
}

static const struct i2c_device_id anx7812_id[] = {
	{ "anx7812", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, anx7812_id);

#ifdef CONFIG_OF
static struct of_device_id anx_match_table[] = {
    { .compatible = "analogix,anx7812",},
    { },
};
#endif

static struct i2c_driver anx7812_driver = {
	.driver  = {
		.name  = "anx7812",
		.owner  = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = anx_match_table,
#endif
	},
	.probe  = anx7812_i2c_probe,
	.remove  = anx7812_i2c_remove,
	.suspend = anx7812_i2c_suspend,
	.resume = anx7812_i2c_resume,
	.id_table  = anx7812_id,
};

static void anx7812_init_async(void *data, async_cookie_t cookie)
{
	int ret = 0;

	ret = i2c_add_driver(&anx7812_driver);
	if (ret < 0)
		SLIMPORT_ERR("failed to register anx7812 i2c drivern");
}

static int __init anx7812_init(void)
{
	async_schedule(anx7812_init_async, NULL);
	return 0;
}

static void __exit anx7812_exit(void)
{
	i2c_del_driver(&anx7812_driver);
}

late_initcall_sync(anx7812_init);
module_exit(anx7812_exit);

MODULE_DESCRIPTION("Slimport  transmitter anx7812 driver");
MODULE_AUTHOR("Junhua Xia <jxia@analogixsemi.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.1");
