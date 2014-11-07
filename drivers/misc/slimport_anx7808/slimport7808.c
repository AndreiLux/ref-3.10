/*
 * Copyright(c) 2012, LG Electronics Inc. All rights reserved.
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
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_data/slimport_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/async.h>
#include <linux/slimport.h>
#include <linux/platform_data/gpio-odin.h>

#include "slimport7808_tx_drv.h"
#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
#include "../msm/mdss/mdss_hdmi_slimport.h"
#endif
/*            
                                                          
                                   
 */
#include <linux/of_gpio.h>
#include <linux/of_platform.h>

/* Enable or Disable HDCP by default */
/* hdcp_enable = 1: Enable,  0: Disable */
static int hdcp_enable = 1;

/* HDCP switch for external block*/
/* external_block_en = 1: enable, 0: disable*/
int external_block_en = 0;


/* to access global platform data */
static struct anx7808_platform_data *g_pdata;

/*            
                                          
                                             
                                                                      
                                   
 */
/* #define USE_HDMI_SWITCH */

#ifdef USE_HDMI_SWITCH
static int hdmi_switch_gpio = 64;
#endif

/* Set ANX_PDWN_CTL GPIO to Input Mode When Slimport Power Down */
#define ANX_PDWN_CTL_GPIO_MODE
/* Handle Slimport Cable Detection ISR After I2C Resumed*/
#define ANX_I2C_SYNC_IRQ

#ifdef CONFIG_SLIMPORT_ADVDD_POWER
static int slimport7808_avdd_power(unsigned int onoff);
static int slimport7808_dvdd_power(unsigned int onoff);
#endif

struct i2c_client *anx7808_client;

struct anx7808_data {
	struct anx7808_platform_data    *pdata;
	struct delayed_work    work;
	struct delayed_work    isr_work;
	struct workqueue_struct    *workqueue;
	struct mutex    lock;
	struct wake_lock slimport_lock;
	struct delayed_work dwc3_ref_clk_work;
	bool slimport_connected;
#ifdef ANX_I2C_SYNC_IRQ
	atomic_t suspend_state;
#endif
};

ATOMIC_NOTIFIER_HEAD(hdmi_hpd_notifier_list);
EXPORT_SYMBOL(hdmi_hpd_notifier_list);

#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
struct msm_hdmi_slimport_ops {
	int (*set_upstream_hpd)(struct platform_device *pdev, uint8_t on);

};
struct msm_hdmi_slimport_ops *hdmi_slimport_ops = NULL;
#endif


#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
void slimport_set_hdmi_hpd(int on)
{
	int rc = 0;

	SLIMPORT_DBG("+\n");

	if (on) {
		rc = hdmi_slimport_ops->set_upstream_hpd(g_pdata->hdmi_pdev, 1);
		SLIMPORT_DBG("hpd on = %s\n", rc ? "failed" : "passed");
	}else{
		rc = hdmi_slimport_ops->set_upstream_hpd(g_pdata->hdmi_pdev, 0);
		SLIMPORT_DBG("hpd off = %s\n", rc ? "failed" : "passed");

	}
	SLIMPORT_DBG("-\n");

}
#endif

bool slimport_is_connected(void)
{
	struct anx7808_platform_data *pdata = NULL;
	bool result = false;

	if (!anx7808_client)
		return false;

#ifdef CONFIG_OF
	pdata = g_pdata;
#else
	pdata = anx7808_client->dev.platform_data;
#endif

	if (!pdata) {
		SLIMPORT_DBG("~~ Slimport Dongle is not detected \n");
		return false;
	}

	/* spin_lock(&pdata->lock); */

	if (gpio_get_value_cansleep(pdata->gpio_cbl_det)) {
		mdelay(10);
		if (gpio_get_value_cansleep(pdata->gpio_cbl_det)) {
			SLIMPORT_DBG("Slimport Dongle is detected\n");
			result = true;
		}
	}
	/* spin_unlock(&pdata->lock); */

	return result;
}
EXPORT_SYMBOL(slimport_is_connected);

/*            
                
                                   
 */
#ifdef CONFIG_SLIMPORT_ADVDD_POWER
static int slimport7808_avdd_power(unsigned int onoff)
{
#ifdef CONFIG_OF
	struct anx7808_platform_data *pdata = g_pdata;
#else
	struct anx7808_platform_data *pdata = anx7808_client->dev.platform_data;
#endif
	int rc = 0;

/* To do : regulator control after H/W change */
	return rc;
	if (onoff) {
		SLIMPORT_DBG("avdd power on\n");
		rc = regulator_enable(pdata->avdd_10);
		if (rc < 0) {
			SLIMPORT_ERR("failed to enable avdd regulator rc=%d\n", rc);
		}
	} else {
			SLIMPORT_DBG("avdd power off\n");
			rc = regulator_disable(pdata->avdd_10);
	}

	return rc;
}

static int slimport7808_dvdd_power(unsigned int onoff)
{
#ifdef CONFIG_OF
	struct anx7808_platform_data *pdata = g_pdata;
#else
	struct anx7808_platform_data *pdata = anx7808_client->dev.platform_data;
#endif
	int rc = 0;

/* To do : regulator control after H/W change */
	return rc;
	if (onoff) {
		SLIMPORT_DBG("dvdd power on\n");
		rc = regulator_enable(pdata->dvdd_10);
		if (rc < 0) {
			SLIMPORT_ERR("failed to enable dvdd regulator rc=%d\n", rc);
		}
	} else {
			SLIMPORT_DBG("dvdd power off\n");
			rc = regulator_disable(pdata->dvdd_10);
	}

	return rc;
}
#endif

#define SLIMPORT_ENABLE_IRQ_AT_BOOTTIME
#ifdef SLIMPORT_ENABLE_IRQ_AT_BOOTTIME
static struct anx7808_data *g_anx7808;
static bool irq_enabled = 0;
static irqreturn_t anx7808_cbl_det_isr(int irq, void *data);

static ssize_t anx7808_enable_irq_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "%d\n", irq_enabled);
}


static ssize_t anx7808_enable_irq_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	int ret;
	long val;

	if (!anx7808_client)
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
		anx7808_cbl_det_isr(anx7808_client->irq, g_anx7808);
		enable_irq(anx7808_client->irq);
	}
	else
		disable_irq(anx7808_client->irq);
	return count;
}
#endif

static ssize_t slimport7808_rev_check_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "%d\n", 0);
}

static ssize_t
slimport7808_rev_check_store(
	struct device *dev, struct device_attribute *attr,
	 const char *buf, size_t count)
{
	int cmd;

	sscanf(buf, "%d", &cmd);
	switch (cmd) {
	case 1:
		sp_tx_chip_located();
		break;
	}
	return count;
}
/*sysfs interface : Enable or Disable HDCP by default*/
static ssize_t sp_hdcp_feature_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "%d\n", hdcp_enable);
}

static ssize_t sp_hdcp_feature_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	int ret;
	long val;
	ret = strict_strtol(buf, 10, &val);
	if (ret)
		return ret;
	hdcp_enable = val;
	SLIMPORT_DBG(" hdcp_enable = %d\n",hdcp_enable);
	return count;
}

/*sysfs  interface : HDCP switch for VGA dongle*/
static ssize_t sp_external_block_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "%d\n", external_block_en);
}

static ssize_t sp_external_block_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
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
static ssize_t anx7730_write_reg_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "%d\n", 0);
}

static ssize_t anx7730_write_reg_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	int ret = 0;
	char op, i;
	char r[3];
	char v[3];
	unchar tmp;
	int id, reg, val = 0 ;

	if (sp_tx_system_state != STATE_PLAY_BACK){
		SLIMPORT_ERR("error!, Not STATE_PLAY_BACK\n");
		return -EINVAL;
	}

	if(sp_tx_rx_type != RX_HDMI){
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
			SLIMPORT_DBG("anx7730 read(%d,0x%x)= 0x%x \n", id, reg, tmp);
			break;

		case 0x31: /* "1" -> write */
			ret = snprintf(v,3,buf+4);
			val = simple_strtoul(v,NULL,16);

			i2c_master_write_reg(id,reg,val);
			i2c_master_read_reg(id,reg,&tmp);
			SLIMPORT_DBG("anx7730 write(%d,0x%x,0x%x)\n", id, reg, tmp);
			break;

		default:
			SLIMPORT_ERR("invalid operation code! (0:read, 1:write)\n");
			return -EINVAL;
	}

	return count;
}

/*sysfs  interface : sp_read_reg, sp_write_reg
anx7808 addr id:: HDMI_rx(0x7e:0, 0x80:1) DP_tx(0x72:5, 0x7a:6, 0x78:7)
ex:read ) 05df   = read:0  id:5 reg:0xdf
ex:write) 15df5f = write:1 id:5 reg:0xdf val:0x5f
*/
static int anx7808_id_change(int id){
	int chg_id = 0;

	switch(id){
		case 0: chg_id = 0x7e; //RX_P0
			break;
		case 1: chg_id = 0x80; //RX_P1
			break;
		case 5: chg_id = 0x72; //TX_P2
			break;
		case 6: chg_id = 0x7a; //TX_P1
			break;
		case 7: chg_id = 0x78; //TX_P0
			break;
	}
	return chg_id;
}

static ssize_t anx7808_write_reg_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "%d\n", 0);
}

static ssize_t anx7808_write_reg_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
{
	int ret = 0;
	char op, i;
	char r[3];
	char v[3];
	unchar tmp;
	int id, reg, val = 0 ;

	if (sp_tx_system_state != STATE_PLAY_BACK){
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

	id = anx7808_id_change(id); //ex) 5 -> 0x72

	switch(op){
		case 0x30: /* "0" -> read */
			sp_read_reg(id, reg, &tmp);
			SLIMPORT_DBG("anx7808 read(0x%x,0x%x)= 0x%x \n", id, reg, tmp);
			break;

		case 0x31: /* "1" -> write */
			ret = snprintf(v,3,buf+4);
			val = simple_strtoul(v,NULL,16);

			sp_write_reg(id, reg, val);
			sp_read_reg(id, reg, &tmp);
			SLIMPORT_DBG("anx7808 write(0x%x,0x%x,0x%x)\n", id, reg, tmp);
			break;

		default:
			SLIMPORT_ERR("invalid operation code! (0:read, 1:write)\n");
			return -EINVAL;
	}

	return count;
}

#ifdef SP_REGISTER_SET_TEST //Slimport test
/*sysfs read interface*/
static ssize_t ctrl_reg0_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG0);
}

/*sysfs write interface*/
static ssize_t ctrl_reg0_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
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
static ssize_t ctrl_reg10_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG10);
}

/*sysfs write interface*/
static ssize_t ctrl_reg10_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
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
static ssize_t ctrl_reg11_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG11);
}

/*sysfs write interface*/
static ssize_t ctrl_reg11_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
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
static ssize_t ctrl_reg2_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG2);
}

/*sysfs write interface*/
static ssize_t ctrl_reg2_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
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
static ssize_t ctrl_reg12_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG12);
}

/*sysfs write interface*/
static ssize_t ctrl_reg12_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
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
static ssize_t ctrl_reg1_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG1);
}

/*sysfs write interface*/
static ssize_t ctrl_reg1_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
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
static ssize_t ctrl_reg6_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG6);
}

/*sysfs write interface*/
static ssize_t ctrl_reg6_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
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
static ssize_t ctrl_reg16_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG16);
}

/*sysfs write interface*/
static ssize_t ctrl_reg16_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
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
static ssize_t ctrl_reg5_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG5);
}

/*sysfs write interface*/
static ssize_t ctrl_reg5_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
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
static ssize_t ctrl_reg8_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG8);
}

/*sysfs write interface*/
static ssize_t ctrl_reg8_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
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
static ssize_t ctrl_reg15_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG15);
}

/*sysfs write interface*/
static ssize_t ctrl_reg15_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
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
static ssize_t ctrl_reg18_show(struct device *dev, struct device_attribute *attr,
		 char *buf)
{
	return sprintf(buf, "0x%x\n", val_SP_TX_LT_CTRL_REG18);
}

/*sysfs write interface*/
static ssize_t ctrl_reg18_store(struct device *dev, struct device_attribute *attr,
		 const char *buf, size_t count)
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

static ssize_t slimport_sysfs_rda_hdmi_vga(struct device *dev, struct device_attribute *attr,
	       char *buf)
{
	int ret;
	ret = is_slimport_vga7808();
	return sprintf(buf, "%d", ret);
}

static struct device_attribute slimport_device_attrs[] = {
	/* __ATTR(rev_check, S_IRUGO | S_IWUSR, NULL, slimport_rev_check_store), */
	__ATTR(rev_check, S_IRUGO | S_IWUSR, slimport7808_rev_check_show, slimport7808_rev_check_store),
	__ATTR(hdcp, S_IRUGO | S_IWUSR, sp_hdcp_feature_show, sp_hdcp_feature_store),
	__ATTR(hdcp_switch, S_IRUGO | S_IWUSR, sp_external_block_show, sp_external_block_store),
	__ATTR(anx7730, S_IWUSR, NULL, anx7730_write_reg_store),
	__ATTR(hdmi_vga, S_IRUGO, slimport_sysfs_rda_hdmi_vga, NULL),
	/* __ATTR(anx7808, S_IRUGO | S_IWUSR, NULL, anx7808_write_reg_store),
	__ATTR(anx7730, S_IRUGO | S_IWUSR, anx7730_write_reg_show, anx7730_write_reg_store), */
	__ATTR(anx7808, S_IRUGO | S_IWUSR, anx7808_write_reg_show, anx7808_write_reg_store),
#ifdef SLIMPORT_ENABLE_IRQ_AT_BOOTTIME
	__ATTR(enable_irq, S_IRUGO | S_IWUSR, anx7808_enable_irq_show, anx7808_enable_irq_store),
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
	SLIMPORT_ERR("Unable to create interface\n");
	return -EINVAL;
}


int sp_read_reg(uint8_t slave_addr, uint8_t offset, uint8_t *buf)
{
	int ret = 0;

	anx7808_client->addr = (slave_addr >> 1);
	ret = i2c_smbus_read_byte_data(anx7808_client, offset);
	if (ret < 0) {
		SLIMPORT_ERR("failed to read i2c addr=0x%x, offset=0x%x\n", slave_addr, offset);
		return ret;
	}
	*buf = (uint8_t) ret;

	return 0;
}

int sp_write_reg(uint8_t slave_addr, uint8_t offset, uint8_t value)
{
	int ret = 0;

	anx7808_client->addr = (slave_addr >> 1);
	ret = i2c_smbus_write_byte_data(anx7808_client, offset, value);
	if (ret < 0) {
		SLIMPORT_ERR("failed to write i2c addr=%x offset=0x%x\n", slave_addr, offset);
	}
	return ret;
}

void sp_tx_hardware_poweron(void)
{
#ifdef CONFIG_OF
	struct anx7808_platform_data *pdata = g_pdata;
#else
	struct anx7808_platform_data *pdata = anx7808_client->dev.platform_data;
#endif
	int status;

	SLIMPORT_DBG_INFO("~~ sp_tx_hardware_poweron call\n");
	gpio_set_value(pdata->gpio_reset, 0);
	msleep(1);

	gpio_set_value(pdata->gpio_p_dwn, 0);
	msleep(2);
	if(pdata->gpio_v10_ctrl != 0) {
		/* Enable 1.0V LDO */
		gpio_set_value(pdata->gpio_v10_ctrl, 1);
		msleep(5);
	}
	gpio_set_value(pdata->gpio_reset, 1);

	sp_tx_pd_mode = 0;

	SLIMPORT_DBG("anx7808 power on\n");
}

void sp_tx_hardware_powerdown(void)
{
#ifdef CONFIG_OF
	struct anx7808_platform_data *pdata = g_pdata;
#else
	struct anx7808_platform_data *pdata = anx7808_client->dev.platform_data;
#endif
	sp_tx_pd_mode = 1;

	gpio_set_value(pdata->gpio_reset, 0);
	msleep(1);
	if (pdata->gpio_v10_ctrl != 0) {
		gpio_set_value(pdata->gpio_v10_ctrl, 0);
		msleep(2);
	}

	gpio_set_value(pdata->gpio_p_dwn, 1);

	msleep(1);

	SLIMPORT_DBG("anx7808 power down\n");
}

int slimport_read_edid_block(int block, uint8_t *edid_buf)
{
	if (block == 0) {
		memcpy(edid_buf, bedid_firstblock, sizeof(bedid_firstblock));
	} else if (block == 1) {
		memcpy(edid_buf, bedid_extblock, sizeof(bedid_extblock));
	} else {
		SLIMPORT_ERR("block number %d is invalid\n", block);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(slimport_read_edid_block);

static void slimport7808_cable_plug_proc(struct anx7808_data *anx7808)
{
	struct anx7808_platform_data *pdata = anx7808->pdata;
	int status =0;

	if (gpio_get_value_cansleep(pdata->gpio_cbl_det)) {
		mdelay(100);
		if (gpio_get_value_cansleep(pdata->gpio_cbl_det)) {
		SLIMPORT_DBG_INFO("~~ slimport_cable_plug_proc called sp_tx_pd_mode= %d\n", sp_tx_pd_mode);
			if (sp_tx_pd_mode) {
#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
				slimport_set_hdmi_hpd(1);
#endif
				sp_tx_hardware_poweron();
				status = sp_tx_power_on(SP_TX_PWR_REG);
				if (status < 0) {
					sp_tx_hardware_powerdown();
					return;
				}

				status = sp_tx_power_on(SP_TX_PWR_TOTAL);
				if (status < 0) {
					sp_tx_hardware_powerdown();
					return;
				}

				hdmi_rx_initialization();
				sp_tx_initialization();
				sp_tx_vbus_poweron();
				msleep(200);
				if (!sp_tx_get_cable_type()) {
					SLIMPORT_ERR("AUX ERR\n");
					sp_tx_vbus_powerdown();
					sp_tx_power_down(SP_TX_PWR_REG);
					sp_tx_power_down(SP_TX_PWR_TOTAL);
					sp_tx_hardware_powerdown();
					sp_tx_link_config_done = 0;
					sp_tx_hw_lt_enable = 0;
					sp_tx_hw_lt_done = 0;
					sp_tx_rx_type = RX_NULL;
					sp_tx_rx_type_backup = RX_NULL;
					sp_tx_set_sys_state(STATE_CABLE_PLUG);
		SLIMPORT_DBG_INFO("~~ slimport_cable_plug_proc called sp_tx_pd_mode \n");
					return;
				}
				sp_tx_rx_type_backup = sp_tx_rx_type;
			}
		SLIMPORT_DBG_INFO("~~ slimport_cable_plug_proc called sp_tx_rx_type=%d\n", sp_tx_rx_type);
			switch (sp_tx_rx_type) {
			case RX_HDMI:
				if (sp_tx_get_hdmi_connection())
					sp_tx_set_sys_state(STATE_PARSE_EDID);
				break;
			case RX_DP:
				if (sp_tx_get_dp_connection())
					sp_tx_set_sys_state(STATE_PARSE_EDID);
				break;
			case RX_VGA_GEN:
				if (sp_tx_get_vga_connection())
					sp_tx_set_sys_state(STATE_PARSE_EDID);
				break;
			case RX_VGA_9832:
				if (sp_tx_get_vga_connection()) {
					sp_tx_send_message(MSG_CLEAR_IRQ);
					sp_tx_set_sys_state(STATE_PARSE_EDID);
				}
				break;
			case RX_NULL:
			default:
				break;
			}
		}
	} else if (sp_tx_pd_mode == 0) {
		SLIMPORT_DBG_INFO("~~ slimport_cable_plug_proc called else if sp_tx_pd_mode == 0\n");
		sp_tx_vbus_powerdown();
		sp_tx_power_down(SP_TX_PWR_REG);
		sp_tx_power_down(SP_TX_PWR_TOTAL);
		sp_tx_hardware_powerdown();
		sp_tx_link_config_done = 0;
		sp_tx_hw_lt_enable = 0;
		sp_tx_hw_lt_done = 0;
		sp_tx_rx_type = RX_NULL;
		sp_tx_rx_type_backup = RX_NULL;
		sp_tx_set_sys_state(STATE_CABLE_PLUG);
	}
}

static void slimport7808_edid_proc(void)
{
	sp_tx_edid_read();

	if (bedid_break)
		SLIMPORT_ERR("EDID corruption!\n");
	atomic_notifier_call_chain(&hdmi_hpd_notifier_list, 1, NULL);
	hdmi_rx_set_hpd(1);
	hdmi_rx_set_termination(1);

	sp_tx_set_sys_state(STATE_CONFIG_HDMI);
}

static void slimport7808_config_output(void)
{
	SLIMPORT_DBG("slimport_config_output\n");
	sp_tx_clean_hdcp();
	sp_tx_set_colorspace();
	sp_tx_avi_setup();
	sp_tx_config_packets(AVI_PACKETS);
	sp_tx_enable_video_input(1);
	sp_tx_set_sys_state(STATE_HDCP_AUTH);
}

static void slimport7808_playback_proc(void)
{
	if ((sp_tx_rx_type == RX_VGA_9832)
		|| (sp_tx_rx_type == RX_VGA_GEN)) {
		if ((sp_tx_hw_hdcp_en == 0) && (external_block_en == 1)) {
			sp_tx_video_mute(1);
			sp_tx_set_sys_state(STATE_HDCP_AUTH);
		} else if ((sp_tx_hw_hdcp_en == 1) && (external_block_en == 0))
			sp_tx_disable_slimport_hdcp();
	}

	if (external_block_en == 1)
		sp_tx_video_mute(1);
	else
		sp_tx_video_mute(0);
}
/*  blcok this due to dongle issue
static void slimport_cable_monitor(struct anx7808_data *anx7808)
{
	if ((gpio_get_value_cansleep(anx7808->pdata->gpio_cbl_det))
		&& (!sp_tx_pd_mode)) {
		sp_tx_get_downstream_type();
		if (sp_tx_rx_type_backup != sp_tx_rx_type) {
			SLIMPORT_DBG("cable changed!\n");
			sp_tx_vbus_powerdown();
			sp_tx_power_down(SP_TX_PWR_REG);
			sp_tx_power_down(SP_TX_PWR_TOTAL);
			sp_tx_hardware_powerdown();
			sp_tx_link_config_done = 0;
			sp_tx_hw_lt_enable = 0;
			sp_tx_hw_lt_done = 0;
			sp_tx_rx_type = RX_NULL;
			sp_tx_rx_type_backup = RX_NULL;
			sp_tx_set_sys_state(STATE_CABLE_PLUG);
		}
	}
}
*/

static void slimport7808_main_proc(struct anx7808_data *anx7808)
{
	unchar check_value;
	int i;

	mutex_lock(&anx7808->lock);

	if (!sp_tx_pd_mode) {

		sp_tx_int_irq_handler();

		hdmi_rx_int_irq_handler();
	}

	if (sp_tx_system_state == STATE_CABLE_PLUG)
		slimport7808_cable_plug_proc(anx7808);

	if (sp_tx_system_state == STATE_PARSE_EDID)
		slimport7808_edid_proc();

	if (sp_tx_system_state == STATE_CONFIG_HDMI)
	{
		sp_tx_config_hdmi_input();
	}
	if (sp_tx_system_state == STATE_LINK_TRAINING) {

		if (!sp_tx_lt_pre_config())
		{
			sp_tx_hw_link_training();
		}
	}

	if (sp_tx_system_state == STATE_CONFIG_OUTPUT)
	{
		slimport7808_config_output();
	}

	if (sp_tx_system_state == STATE_HDCP_AUTH) {

		if (hdcp_enable) {
			sp_tx_hdcp_process();
		} else {
			sp_tx_power_down(SP_TX_PWR_HDCP);
			sp_tx_video_mute(0);
			sp_tx_show_infomation();
			sp_tx_set_sys_state(STATE_PLAY_BACK);
		}
	}

	if (sp_tx_system_state == STATE_PLAY_BACK)
		slimport7808_playback_proc();
	/*slimport_cable_monitor(anx7808);*/

	mutex_unlock(&anx7808->lock);
}

static uint8_t anx7808_chip_detect(void)
{
	return sp_tx_chip_located();
}

static void anx7808_chip_initial(void)
{
#ifdef EYE_TEST
	sp_tx_eye_diagram_test();
#else

	sp_tx_variable_init();
	sp_tx_vbus_powerdown();
	sp_tx_hardware_powerdown();
	sp_tx_set_sys_state(STATE_CABLE_PLUG);
#endif
}

static void anx7808_free_gpio(struct anx7808_data *anx7808)
{
	gpio_free(anx7808->pdata->gpio_cbl_det);
#if 0
	gpio_free(anx7808->pdata->gpio_int);
#endif
	gpio_free(anx7808->pdata->gpio_reset);
	gpio_free(anx7808->pdata->gpio_p_dwn);
	if(anx7808->pdata->gpio_v10_ctrl != 0) {
		gpio_free(anx7808->pdata->gpio_v10_ctrl);
	}
#ifdef CONFIG_SLIMPORT_ADVDD_POWER
	if (anx7808->pdata->external_ldo_control) {
		gpio_free(anx7808->pdata->gpio_v33_ctrl);
	}
#endif
}
static int anx7808_init_gpio(struct anx7808_data *anx7808)
{
	int ret = 0;

	SLIMPORT_DBG("anx7808 init gpio\n");

	ret = gpio_request(anx7808->pdata->gpio_p_dwn, "anx_p_dwn_ctl");
	if (ret) {
		SLIMPORT_ERR("failed to request gpio %d\n", anx7808->pdata->gpio_p_dwn);
		goto out;
	}
	gpio_direction_output(anx7808->pdata->gpio_p_dwn, 1);

	ret = gpio_request(anx7808->pdata->gpio_reset, "anx7808_reset_n");
		SLIMPORT_DBG("~~ gpio_request anx7808_reset_n ret=%d\n", ret);
	if (ret) {
		SLIMPORT_ERR("failed to request gpio %d\n", anx7808->pdata->gpio_reset);
		goto err0;
	}
	gpio_direction_output(anx7808->pdata->gpio_reset, 0);
#if 0
	ret = gpio_request(anx7808->pdata->gpio_int, "anx7808_int_n");
	if (ret) {
		SLIMPORT_ERR("failed to request gpio %d\n", anx7808->pdata->gpio_int);
		goto err1;
	}
	gpio_direction_input(anx7808->pdata->gpio_int);
#endif
	ret = gpio_request(anx7808->pdata->gpio_cbl_det, "anx7808_cbl_det");
	if (ret) {
		SLIMPORT_ERR("failed to request gpio %d\n", anx7808->pdata->gpio_cbl_det);
		goto err2;
	}
	gpio_direction_input(anx7808->pdata->gpio_cbl_det);

	if(anx7808->pdata->gpio_v10_ctrl != 0) {
		ret = gpio_request(anx7808->pdata->gpio_v10_ctrl,
							"anx7808_v10_ctrl");
			if (ret) {
				SLIMPORT_ERR("failed to request gpio %d\n",
								anx7808->pdata->gpio_v10_ctrl);
			goto err3;
		}
		gpio_direction_output(anx7808->pdata->gpio_v10_ctrl, 0);
		gpio_set_value(anx7808->pdata->gpio_v10_ctrl, 0);
	}

#ifdef CONFIG_SLIMPORT_ADVDD_POWER
	if (anx7808->pdata->external_ldo_control) {
		ret = gpio_request(anx7808->pdata->gpio_v33_ctrl,
							"anx7808_v33_ctrl");
			if (ret) {
				SLIMPORT_ERR("failed to request gpio %d\n",
								anx7808->pdata->gpio_v33_ctrl);
			goto err4;
		}
		gpio_direction_output(anx7808->pdata->gpio_v33_ctrl, 0);

		/* need to be check below */
		gpio_set_value(anx7808->pdata->gpio_v33_ctrl, 1);

	}
#endif
	gpio_set_value(anx7808->pdata->gpio_reset, 0);
	gpio_set_value(anx7808->pdata->gpio_p_dwn, 1);

#ifdef USE_HDMI_SWITCH
	ret = gpio_request(hdmi_switch_gpio, "anx7808_hdmi_switch_gpio");
	if (ret) {
		SLIMPORT_ERR("failed to request gpio %d\n", hdmi_switch_gpio);
		goto err5;
	}
	gpio_direction_output(hdmi_switch_gpio, 0);
	msleep(1);
	gpio_set_value(hdmi_switch_gpio, 1);
#endif

	goto out;

#ifdef USE_HDMI_SWITCH
err5:
#ifdef CONFIG_SLIMPORT_ADVDD_POWER
	gpio_free(anx7808->pdata->gpio_v33_ctrl);
#endif
#endif

#ifdef CONFIG_SLIMPORT_ADVDD_POWER
err4:
	gpio_free(anx7808->pdata->gpio_v10_ctrl);
#endif

err3:
	gpio_free(anx7808->pdata->gpio_cbl_det);

err2:
#if 0
	gpio_free(anx7808->pdata->gpio_int);
err1:
#endif
	gpio_free(anx7808->pdata->gpio_reset);

err0:
	gpio_free(anx7808->pdata->gpio_p_dwn);

out:
	return ret;
}

static int anx7808_system_init(void)
{
	int ret = 0;

	ret = anx7808_chip_detect();
	if (ret == 0) {
		SLIMPORT_ERR("failed to detect anx7808\n");
		return -ENODEV;
	}

	anx7808_chip_initial();
	return 0;
}

#ifdef CONFIG_SLIMPORT_USE_DWC3_REF_CLOCK
extern void dwc3_ref_clk_set(bool);
static void dwc3_ref_clk_work_func(struct work_struct *work)
{
	struct anx7808_data *td = container_of(work, struct anx7808_data,
						dwc3_ref_clk_work.work);
	if(td->slimport_connected)
		dwc3_ref_clk_set(true);
	else
		dwc3_ref_clk_set(false);
}
#endif

static irqreturn_t anx7808_cbl_det_isr(int irq, void *data)
{
	struct anx7808_data *anx7808 = data;

#ifdef ANX_I2C_SYNC_IRQ
	switch (atomic_read(&anx7808->suspend_state)) {
		case I2C_RESUME:
			queue_delayed_work(anx7808->workqueue, &anx7808->isr_work, 0);
			break;

		case I2C_SUSPEND:
		case I2C_SUSPEND_IRQ:
			atomic_set(&anx7808->suspend_state, I2C_SUSPEND_IRQ);
			break;

		default:
			break;
	}
#else
	queue_delayed_work(anx7808->workqueue, &anx7808->isr_work, 0);
#endif

	return IRQ_HANDLED;
}

static void anx7808_isr_work_func(struct work_struct *work)
{
	if (gpio_get_value(g_anx7808->pdata->gpio_cbl_det)) {
		wake_lock(&g_anx7808->slimport_lock);
		SLIMPORT_DBG("detect cable insertion\n");
		if (!g_anx7808->slimport_connected) {
			g_anx7808->slimport_connected = true;
		}
		queue_delayed_work(g_anx7808->workqueue, &g_anx7808->work, 0);
	}else {
		SLIMPORT_DBG("detect cable removal\n");
		if (g_anx7808->slimport_connected) {
			g_anx7808->slimport_connected = false;
		}
		cancel_delayed_work_sync(&g_anx7808->work);
		wake_unlock(&g_anx7808->slimport_lock);

		wake_lock_timeout(&g_anx7808->slimport_lock, 2*HZ);
	}
}

static void anx7808_work_func(struct work_struct *work)
{
#ifndef EYE_TEST
	struct anx7808_data *td = container_of(work, struct anx7808_data,
								work.work);

	slimport7808_main_proc(td);
	queue_delayed_work(td->workqueue, &td->work,
			msecs_to_jiffies(300));
#endif
}

/*            
                                    
                                   
 */
#ifdef CONFIG_OF
#ifdef CONFIG_SLIMPORT_ADVDD_POWER
int anx7808_regulator_configure(
	struct device *dev, struct anx7808_platform_data *pdata)
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

static int anx7808_parse_dt(
	struct device *dev, struct anx7808_platform_data *pdata)
{
	int rc = 0;
	struct device_node *np = dev->of_node;
#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
	struct platform_device *sp_pdev = NULL;
	struct device_node *sp_tx_node = NULL;
#endif

	rc = of_property_read_u32(np, "analogix,p-dwn-gpio", &pdata->gpio_p_dwn);
	rc = of_property_read_u32(np, "analogix,reset-gpio", &pdata->gpio_reset);
	rc = of_property_read_u32(np, "analogix,irq-gpio", &pdata->gpio_int);
	rc = of_property_read_u32(np, "analogix,cbl-det-gpio", &pdata->gpio_cbl_det);

	SLIMPORT_DBG_INFO("gpio p_dwn : %d, reset : %d, irq : %d, gpio_cbl_det : %d\n",
			pdata->gpio_p_dwn, pdata->gpio_reset, 	pdata->gpio_int, pdata->gpio_cbl_det);
	rc = of_property_read_u32(np, "analogix,v10-ctrl-gpio", &pdata->gpio_v10_ctrl);
	SLIMPORT_DBG_INFO("gpio gpio_v10_ctrl : %d\n", pdata->gpio_v10_ctrl);

	/* connects function nodes which are not provided with dts */
#ifdef CONFIG_SLIMPORT_ADVDD_POWER
	if (pdata->external_ldo_control) {
		rc = of_property_read_u32(np, "analogix,v33-ctrl-gpio", &pdata->gpio_v33_ctrl);
	SLIMPORT_DBG_INFO("gpio_v10_ctrl : %d, avdd33-en-gpio : %d\n",
			pdata->gpio_v10_ctrl, pdata->gpio_v33_ctrl);
	}

	pdata->avdd_power = slimport_avdd_power;
	pdata->dvdd_power = slimport_dvdd_power;
#endif

#ifdef USE_HDMI_SWITCH
	rc = of_property_read_u32(np, "analogix,hdmi-switch-gpio", &hdmi_switch_gpio);
	SLIMPORT_DBG_INFO("hdmi_switch_gpio : %d \n", hdmi_switch_gpio);
#endif
	return 0;
}
#else
static int anx7808_parse_dt(
	struct device *dev, struct anx7808_platform_data *pdata)
{
	return -ENODEV;
}
#endif


#ifdef CONFIG_SLIMPORT_ZERO_WAIT
static int zw_slimport_notifier_call(struct notifier_block *nb,
			unsigned long state, void *ptr)
{
	struct anx7808_data *anx7808 = (struct anx7808_data *)nb->ptr;

	switch (state) {
	case ZW_STATE_OFF:
		if (gpio_get_value(anx7808->pdata->gpio_cbl_det)) {
			wake_lock(&anx7808->slimport_lock);
			anx7808->slimport_connected = true;
#ifdef CONFIG_SLIMPORT_USE_DWC3_REF_CLOCK
			dwc3_ref_clk_set(true);
#endif
			queue_delayed_work(anx7808->workqueue,
						&anx7808->work, 0);
		}
		break;

	case ZW_STATE_ON_SYSTEM:
	case ZW_STATE_ON_USER:
		if (anx7808->slimport_connected) {
			/* we must go to suspend in the ZeroWait mdoe*/
#ifdef CONFIG_SLIMPORT_USE_DWC3_REF_CLOCK
			dwc3_ref_clk_set(false);
#endif
			cancel_delayed_work_sync(&anx7808->work);
			anx7808->slimport_connected = false;
			wake_unlock(&anx7808->slimport_lock);
		}
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block zw_slimport_nb = {
	.notifier_call = zw_slimport_notifier_call,
};
#endif /* CONFIG_SLIMPORT_ZERO_WAIT */

static int anx7808_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{

	struct anx7808_data *anx7808;
	struct anx7808_platform_data *pdata;
	int ret = 0;
//	int sbl_cable_type = 0;

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
		SLIMPORT_ERR("i2c bus does not support the anx7808\n");
		ret = -ENODEV;
		goto exit;
	}
	anx7808 = kzalloc(sizeof(struct anx7808_data), GFP_KERNEL);
	if (!anx7808) {
		SLIMPORT_ERR("failed to allocate driver data\n");
		ret = -ENOMEM;
		goto exit;
	}
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
							 sizeof(struct anx7808_platform_data),
							 GFP_KERNEL);
		if (!pdata) {
			SLIMPORT_ERR("Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err0;
		}
		client->dev.platform_data = pdata;
	/* device tree parsing function call */
		ret = anx7808_parse_dt(&client->dev, pdata);
		if (ret != 0) /* if occurs error */
			goto err0;

		anx7808->pdata = pdata;
	} else {
		anx7808->pdata = client->dev.platform_data;
	}
	/* to access global platform data */
	g_pdata = anx7808->pdata;

#ifdef SLIMPORT_ENABLE_IRQ_AT_BOOTTIME
	/*                    */
	g_anx7808 = anx7808;
#endif

	anx7808_client = client;

	mutex_init(&anx7808->lock);

	if (!anx7808->pdata) {
		ret = -EINVAL;
		goto err0;
	}

	ret = anx7808_init_gpio(anx7808);
	if (ret) {
		SLIMPORT_ERR("failed to initialize gpio\n");
		goto err0;
	}

	INIT_DELAYED_WORK(&anx7808->work, anx7808_work_func);
	INIT_DELAYED_WORK(&anx7808->isr_work, anx7808_isr_work_func);
#ifdef CONFIG_SLIMPORT_USE_DWC3_REF_CLOCK
	INIT_DELAYED_WORK(&anx7808->dwc3_ref_clk_work, dwc3_ref_clk_work_func);
#endif
	anx7808->workqueue = create_singlethread_workqueue("anx7808_work");
	if (anx7808->workqueue == NULL) {
		SLIMPORT_ERR("failed to create work queue\n");
		ret = -ENOMEM;
		goto err1;
	}

#ifdef CONFIG_SLIMPORT_ADVDD_POWER
	anx7808->pdata->avdd_power(1);
	anx7808->pdata->dvdd_power(1);
#endif
	ret = anx7808_system_init();
	if (ret) {
		SLIMPORT_ERR("failed to initialize anx7808\n");
		goto err2;
	}

	client->irq = gpio_to_irq(anx7808->pdata->gpio_cbl_det);
	if (client->irq < 0) {
		SLIMPORT_ERR("failed to get gpio irq\n");
		goto err2;
	}

	wake_lock_init(&anx7808->slimport_lock,
				WAKE_LOCK_SUSPEND,
				"slimport_wake_lock");

	/* sbl_cable_type = anx7808_get_sbl_cable_type(); */

	/*                                                
                                 
 */
		if(is_gic_direct_irq(client->irq) == 1) {
			ret = odin_gpio_sms_request_threaded_irq(client->irq, NULL, anx7808_cbl_det_isr,
						IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						"anx7808", anx7808);
			SLIMPORT_DBG_INFO("slimport cbl_det pin sms_request_threaded_irq!!!\n");
		}
		else {
			ret = request_threaded_irq(client->irq, NULL, anx7808_cbl_det_isr,
						IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						"anx7808", anx7808);
			SLIMPORT_DBG_INFO("slimport cbl_det pin request_threaded_irq!!!\n");
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
		atomic_set(&anx7808->suspend_state, I2C_RESUME);
#endif
#if 0
		ret = irq_set_irq_wake(client->irq, 1);
		if (ret  < 0) {
			SLIMPORT_ERR("Request irq for cable detect");
			pr_err("interrupt wake set fail\n");
			goto err3;
		}

		ret = enable_irq_wake(client->irq);
		if (ret  < 0) {
			SLIMPORT_ERR("Enable irq for cable detect");
			pr_err("interrupt wake enable fail\n");
			goto err3;
		}
#endif
	/* }else {
		SLIMPORT_ERR("%s, Disable cbl det irq!!\n",
			sbl_cable_type == CBL_910K ? "910K Cable Connected" : "Laf Mode");
	}
	*/


	ret = create_sysfs_interfaces(&client->dev);
	if (ret < 0) {
		SLIMPORT_ERR("sysfs register failed\n");
		goto err3;
	}
#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
	hdmi_slimport_ops = devm_kzalloc(&client->dev,
				    sizeof(struct msm_hdmi_slimport_ops),
				    GFP_KERNEL);
	if (!hdmi_slimport_ops) {
		SLIMPORT_ERR("alloc hdmi slimport ops failed\n");
		ret = -ENOMEM;
		goto err3;
	}

	if (anx7808->pdata->hdmi_pdev) {
		ret = msm_hdmi_register_slimport(anx7808->pdata->hdmi_pdev,
					   hdmi_slimport_ops, anx7808);
		if (ret) {
			SLIMPORT_ERR("register with hdmi failed\n");
			ret = -EPROBE_DEFER;
			goto err3;
		}
	}
#endif

#ifdef CONFIG_SLIMPORT_ZERO_WAIT
	zw_irqs_info_register(client->irq, 1);
	zw_notifier_chain_register(&zw_slimport_nb, anx7808);
#endif
	SLIMPORT_DBG_INFO("probe finished!!!\n");
	goto exit;

err3:
	free_irq(client->irq, anx7808);
err2:
	destroy_workqueue(anx7808->workqueue);
err1:
	anx7808_free_gpio(anx7808);
err0:
	anx7808_client = NULL;

#ifdef SLIMPORT_ENABLE_IRQ_AT_BOOTTIME
	/*                    */
	g_anx7808 = NULL;
#endif

	kfree(anx7808);
exit:
	return ret;
}

static int anx7808_i2c_remove(struct i2c_client *client)
{
	struct anx7808_data *anx7808 = i2c_get_clientdata(client);
	int i = 0;

#ifdef CONFIG_SLIMPORT_ZERO_WAIT
	zw_irqs_info_unregister(client->irq);
	zw_notifier_chain_unregister(&zw_slimport_nb);
#endif

	for (i = 0; i < ARRAY_SIZE(slimport_device_attrs); i++)
		device_remove_file(&client->dev, &slimport_device_attrs[i]);
	free_irq(client->irq, anx7808);
	anx7808_free_gpio(anx7808);
	destroy_workqueue(anx7808->workqueue);
	wake_lock_destroy(&anx7808->slimport_lock);

#ifdef SLIMPORT_ENABLE_IRQ_AT_BOOTTIME
	/*                    */
	g_anx7808 = NULL;
#endif

	kfree(anx7808);
	return 0;
}

bool is_slimport_vga(void)
{
	return ((sp_tx_rx_type == RX_VGA_9832)
		|| (sp_tx_rx_type == RX_VGA_GEN)) ? TRUE : FALSE;
}
/* 0x01: hdmi device is attached
    0x02: DP device is attached
    0x03: Old VGA device is attached // RX_VGA_9832
    0x04: new combo VGA device is attached // RX_VGA_GEN
    0x00: unknow device            */
EXPORT_SYMBOL(is_slimport_vga);
bool is_slimport_dp(void)
{
	return (sp_tx_rx_type == RX_DP) ? TRUE : FALSE;
}
EXPORT_SYMBOL(is_slimport_dp);
unchar sp_get_link_bw(void)
{
	return slimport_link_bw;
}
EXPORT_SYMBOL(sp_get_link_bw);
void sp_set_link_bw(unchar link_bw)
{
	slimport_link_bw = link_bw;
}

static int anx7808_i2c_suspend(struct i2c_client *client, pm_message_t state)
{
/*                    */
#ifdef ANX_PDWN_CTL_GPIO_MODE
	gpio_direction_input(g_anx7808->pdata->gpio_p_dwn);
	SLIMPORT_DBG("anx_pdwn gpio direction change to input\n");
#endif

#ifdef ANX_I2C_SYNC_IRQ
	atomic_set(&g_anx7808->suspend_state, I2C_SUSPEND);
#endif

	return 0;
}

static int anx7808_i2c_resume(struct i2c_client *client)
{
/*                    */
#ifdef ANX_PDWN_CTL_GPIO_MODE
	gpio_direction_output(g_anx7808->pdata->gpio_p_dwn, 1);
	SLIMPORT_DBG("anx_pdwn gpio direction change to output\n");
#endif

#ifdef ANX_I2C_SYNC_IRQ
	if (atomic_read(&g_anx7808->suspend_state) == I2C_SUSPEND_IRQ) {
		queue_delayed_work(g_anx7808->workqueue, &g_anx7808->isr_work, msecs_to_jiffies(700));
		SLIMPORT_DBG("i2c_resume isr workqueue");
	}
	atomic_set(&g_anx7808->suspend_state, I2C_RESUME);
#endif

	return 0;
}

static const struct i2c_device_id anx7808_id[] = {
	{ "anx7808", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, anx7808_id);

#ifdef CONFIG_OF
static struct of_device_id anx_match_table[] = {
    { .compatible = "analogix,anx7808",},
    { },
};
#endif

static struct i2c_driver anx7808_driver = {
	.driver  = {
		.name  = "anx7808",
		.owner  = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = anx_match_table,
#endif
	},
	.probe  = anx7808_i2c_probe,
	.remove  = anx7808_i2c_remove,
	.suspend = anx7808_i2c_suspend,
	.resume = anx7808_i2c_resume,
	.id_table  = anx7808_id,
};

static void anx7808_init_async(void *data, async_cookie_t cookie)
{
	int ret = 0;

	ret = i2c_add_driver(&anx7808_driver);
	if (ret < 0)
		pr_err("%s: failed to register anx7808 i2c drivern", __func__);
}

static int __init anx7808_init(void)
{
	async_schedule(anx7808_init_async, NULL);
	return 0;
}

static void __exit anx7808_exit(void)
{
	i2c_del_driver(&anx7808_driver);
}

late_initcall_sync(anx7808_init);
module_exit(anx7808_exit);

MODULE_DESCRIPTION("Slimport  transmitter ANX7808 driver");
MODULE_AUTHOR("ChoongRyeol Lee <choongryeol.lee@lge.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.4");

