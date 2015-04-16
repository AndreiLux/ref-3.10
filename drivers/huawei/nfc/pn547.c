/*
 * Copyright (C) 2010 Trusted Logic S.A.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/wakelock.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include "pn547.h"
#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif
#include <hsad/config_mgr.h>
#define MAX_BUFFER_SIZE	512
static int firmware_update = 0;
static struct wake_lock wlock_read;
#define NFC_GPIO_LEVEL_HIGH 1

static bool ven_felica_status=false;

#define	MAX_CONFIG_NAME_SIZE	64
#define _read_sample_test_size			40
static char g_nfc_nxp_config_name[MAX_CONFIG_NAME_SIZE];

struct pn547_dev {
	wait_queue_head_t	read_wq;
	struct mutex	read_mutex;
	struct i2c_client	*client;
	struct miscdevice	pn547_device;
	struct pinctrl *pctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_idle;
	unsigned int 	firm_gpio;
	unsigned int	irq_gpio;
	int	sim_switch;
	int	sim_status;
	bool		irq_enabled;
	spinlock_t		irq_enabled_lock;
	struct regulator_bulk_data ven_felica;
	struct clk		*nfc_clk;
};

static  struct pn547_dev* nfcdev;

void set_nfc_nxp_config_name(void){
	int ret=-1;

	memset(g_nfc_nxp_config_name, 0, MAX_CONFIG_NAME_SIZE);
	ret = get_hw_config_string("nfc", "nfc_nxp_name", g_nfc_nxp_config_name, MAX_CONFIG_NAME_SIZE-1);
	if (ret != 0){
		memset(g_nfc_nxp_config_name, 0, MAX_CONFIG_NAME_SIZE);
		pr_err("%s: can't get nfc nxp config name\n", __func__);
		return ;
	}
	pr_info("%s: nfc nxp config name:%s\n", __func__, g_nfc_nxp_config_name);
	return ;
}

static int pn547_bulk_enable(struct  pn547_dev *pdev){
	int ret=0;

	if (false == ven_felica_status) {
		ret = regulator_bulk_enable(1, &(pdev->ven_felica));
		if (ret < 0)
			pr_err("%s: regulator_enable failed, ret:%d\n", __func__, ret);
		else
			ven_felica_status = true;
	} else {
		pr_err("%s: ven already enable\n", __func__);
	}

	return ret;
}

static int  pn547_bulk_disable(struct  pn547_dev *pdev){
	int ret=0;

	if (true == ven_felica_status){
		ret = regulator_bulk_disable(1, &(pdev->ven_felica));
		if (ret < 0)
			pr_err("%s: regulator_disable failed, ret:%d\n", __func__, ret);
		else
			ven_felica_status = false;
	} else {
		pr_err("%s: ven already disable\n", __func__);
	}

	return ret;
}

static int pn547_enable_nfc(struct  pn547_dev *pdev)
{
	int ret = 0;

	/* enable chip */
	ret = pn547_bulk_enable(pdev);
	msleep(20);
	if (ret < 0)
		return ret;

	ret = pn547_bulk_disable(pdev);
	msleep(60);
	if (ret < 0)
		return ret;

	ret = pn547_bulk_enable(pdev);
	msleep(20);

	return ret;
}

static void pn547_disable_irq(struct pn547_dev *pn547_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&pn547_dev->irq_enabled_lock, flags);
	if (pn547_dev->irq_enabled) {
		disable_irq_nosync(pn547_dev->client->irq);
		pn547_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&pn547_dev->irq_enabled_lock, flags);
}

static irqreturn_t pn547_dev_irq_handler(int irq, void *dev_id)
{
	struct pn547_dev *pn547_dev = dev_id;

	pn547_disable_irq(pn547_dev);

	/*set a wakelock to avoid entering into suspend */
	wake_lock_timeout(&wlock_read, 1 * HZ);

	/* Wake up waiting readers */
	wake_up(&pn547_dev->read_wq);

	return IRQ_HANDLED;
}

static ssize_t pn547_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn547_dev *pn547_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	char *tmpStr = NULL;
	int ret;
	int i;
	int retry;
	bool isSuccess = false;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	mutex_lock(&pn547_dev->read_mutex);

	if (!gpio_get_value(pn547_dev->irq_gpio)) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto fail;
		}

		pn547_disable_irq(pn547_dev);

		pn547_dev->irq_enabled = true;
		enable_irq(pn547_dev->client->irq);
		ret = wait_event_interruptible(pn547_dev->read_wq,
		gpio_get_value(pn547_dev->irq_gpio));
		if (ret)
			goto fail;
	}

	tmpStr = (char *)kzalloc(sizeof(tmp)*2, GFP_KERNEL);
	if (!tmpStr) {
		pr_info("%s:Cannot allocate memory for read tmpStr.\n", __func__);
		ret = -ENOMEM;
		goto fail;
	}

	/* Read data, we have 3 chances */
	for (retry = 0; retry < NFC_TRY_NUM; retry++)
	{
		ret = i2c_master_recv(pn547_dev->client, tmp, (int)count);

            for(i=0; i<count; i++)
            {
                snprintf(&tmpStr[i * 2], 3, "%02X", tmp[i]);
            }

            pr_info("%s : retry = %d, ret = %d, count = %3d > %s\n", __func__, retry, ret, count, tmpStr);

		if (ret == (int)count) {
			isSuccess = true;
			break;
		} else {
			pr_info("%s : read data try =%d returned %d\n", __func__,retry,ret);
			msleep(10);
			continue;
		}
	}

	kfree(tmpStr);
	if (false == isSuccess) {
		pr_err("%s : i2c_master_recv returned %d\n", __func__, ret);
		ret = -EIO;
	}

	mutex_unlock(&pn547_dev->read_mutex);

	if (ret < 0) {
		pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
		return ret;
	}
	if (ret > (int)count) {
		pr_err("%s: received too many bytes from i2c (%d)\n",
			__func__, ret);
		return -EIO;
	}
	if (copy_to_user(buf, tmp, ret)) {
		pr_err("%s : failed to copy to user space\n", __func__);
		return -EFAULT;
	}
	return (size_t)ret;

fail:
	mutex_unlock(&pn547_dev->read_mutex);
	return (size_t)ret;
}

static ssize_t pn547_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn547_dev  *pn547_dev;
	char tmp[MAX_BUFFER_SIZE];
	char *tmpStr = NULL;
	int ret;
	int retry;
	int i;
	bool isSuccess = false;

	pn547_dev = filp->private_data;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	if (copy_from_user(tmp, buf, count)) {
		pr_err("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}

	tmpStr = (char *)kzalloc(sizeof(tmp)*2, GFP_KERNEL);
	if (!tmpStr) {
		pr_info("%s:Cannot allocate memory for write tmpStr.\n", __func__);
		return -ENOMEM;
	}

	/* Write data, we have 3 chances */
	for (retry = 0; retry < NFC_TRY_NUM; retry++)
	{
		ret = i2c_master_send(pn547_dev->client, tmp, (int)count);

            for(i=0; i<count; i++)
            {
                snprintf(&tmpStr[i * 2], 3, "%02X", tmp[i]);
            }

            pr_info("%s : retry = %d, ret = %d, count = %3d > %s\n", __func__, retry, ret, count, tmpStr);

		if (ret == (int)count) {
			isSuccess = true;
			break;
		} else {
			if (retry > 0) {
				pr_info("%s : send data try =%d returned %d\n", __func__,retry,ret);
			}
			msleep(50);
			continue;
		}
	}
	kfree(tmpStr);

	if (false == isSuccess) {
		pr_err("%s : i2c_master_send returned %d\n", __func__, ret);
		ret = -EIO;
	}

	return (size_t)ret;
}

static int pn547_dev_open(struct inode *inode, struct file *filp)
{
	struct pn547_dev *pn547_dev = container_of(filp->private_data,
						struct pn547_dev,
						pn547_device);

	filp->private_data = pn547_dev;

	pr_info("%s : %d,%d\n", __func__, imajor(inode), iminor(inode));

	return 0;
}

static long pn547_dev_ioctl(struct file *filp,
			    unsigned int cmd, unsigned long arg)
{
	struct pn547_dev *pn547_dev = filp->private_data;
	int ret  = 0;

	switch(cmd){
	case PN547_SET_PWR:
		if (arg == 2) {
			/* power on with firmware download (requires hw reset)
			 */
			pr_info("%s power on with firmware\n", __func__);
			ret = pn547_bulk_enable(pn547_dev);
			if (ret < 0) {
				pr_err("%s: regulator_enable failed, ret:%d\n", __func__, ret);
				return ret;
			}
			msleep(1);
			gpio_set_value(pn547_dev->firm_gpio, 1);

			msleep(20);
			ret = pn547_bulk_disable(pn547_dev);
			if (ret < 0) {
				pr_err("%s: regulator_disable failed, ret:%d\n", __func__, ret);
				return ret;
			}

			msleep(60);
			ret = pn547_bulk_enable(pn547_dev);
			if (ret < 0) {
				pr_err("%s: regulator_enable failed, ret:%d\n", __func__, ret);
				return ret;
			}
			msleep(20);
		} else if (arg == 1) {
			/* power on */
			pr_info("%s power on\n", __func__);
			gpio_set_value(pn547_dev->firm_gpio, 0);
			ret = pn547_bulk_enable(pn547_dev);
			if (ret < 0) {
				pr_err("%s: regulator_enable failed, ret:%d\n", __func__, ret);
				return -EINVAL;
			}
			msleep(20);
		} else  if (arg == 0) {
			/* power off */
			pr_info("%s power off\n", __func__);
			gpio_set_value(pn547_dev->firm_gpio, 0);
			ret = pn547_bulk_disable(pn547_dev);
			if (ret < 0) {
				pr_err("%s: regulator_disable failed, ret:%d\n", __func__, ret);
				return -EINVAL;
			}
			msleep(60);
		} else {
			pr_err("%s bad arg %lu\n", __func__, arg);
			return -EINVAL;
		}
		break;
	default:
		pr_err("%s bad ioctl %u\n", __func__, cmd);
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations pn547_dev_fops = {
	.owner	= THIS_MODULE,
	.llseek	= no_llseek,
	.read	= pn547_dev_read,
	.write	= pn547_dev_write,
	.open	= pn547_dev_open,
	.unlocked_ioctl	= pn547_dev_ioctl,
};
static ssize_t pn547_i2c_write(struct  pn547_dev *pdev,const char *buf,int count)
{
	int ret;
	int retry;
	bool isSuccess = false;
	/* Write data, we have 3 chances */
	for (retry = 0; retry < NFC_TRY_NUM; retry++)
	{
		ret = i2c_master_send(pdev->client, buf, (int)count);
		if (ret == (int)count) {
			isSuccess = true;
			break;
		} else {
			if (retry > 0) {
				pr_info("%s : send data try =%d returned %d\n", __func__,retry,ret);
			}
			msleep(10);
			continue;
		}
	}
	if (false == isSuccess) {
		pr_err("%s : i2c_master_send returned %d\n", __func__, ret);
		ret = -EIO;
	}
	return (size_t)ret;
}
static ssize_t pn547_i2c_read(struct  pn547_dev *pdev,char *buf,int count)
{
	int ret;
	int retry;
	bool isSuccess = false;

	if (!gpio_get_value(pdev->irq_gpio)) {
		pn547_disable_irq(pdev);

		pdev->irq_enabled = true;
		enable_irq(pdev->client->irq);
		ret = wait_event_interruptible_timeout(pdev->read_wq,
		gpio_get_value(pdev->irq_gpio),msecs_to_jiffies(1000));
		if (ret <= 0)
		{
			ret  = -1;
			pr_err("%s : wait_event_interruptible_timeout error!\n", __func__);
			goto fail;
		}
	}
	/* Read data, we have 3 chances */
	for (retry = 0; retry < NFC_TRY_NUM; retry++)
	{
		ret = i2c_master_recv(pdev->client, buf, (int)count);
		if (ret == (int)count) {
			isSuccess = true;
			break;
		} else {
			pr_err("%s : read data try =%d returned %d\n", __func__,retry,ret);
			msleep(10);
			continue;
		}
	}
	if (false == isSuccess) {
		pr_err("%s : i2c_master_recv returned %d\n", __func__, ret);
		ret = -EIO;
	}
	return (size_t)ret;
fail:
	return (size_t)ret;
}
static int check_sim_status(struct i2c_client *client, struct  pn547_dev *pdev)
{
	int ret;

	unsigned char recvBuf[40];
	const  char send_reset[] = {0x20,0x00,0x01,0x01};
	const  char init_cmd[] = {0x20,0x01,0x00};

	const  char read_config[] = {0x2F,0x02,0x00};
	const  char read_config_UICC[] = {0x2F,0x3E,0x01,0x00};			//swp1
	const  char read_config_eSE[] = {0x2F,0x3E,0x01,0x01};			//eSE
	pdev->sim_status = 0;
	pr_info("%s: enter!\n", __func__);

	/*hardware reset*/
	/* power on */
	gpio_set_value(pdev->firm_gpio, 0);
	ret = pn547_bulk_enable(pdev);
	if (ret < 0)
	{
		pr_err("%s: regulator_enable failed, ret:%d\n", __func__, ret);
		goto failed;
	}
	msleep(20);


	/* power off */
	ret = pn547_bulk_disable(pdev);
	if (ret < 0) {
		pr_err("%s: regulator_disable failed, ret:%d\n", __func__, ret);
		goto failed;
	}
	msleep(60);

	/* power on */
	ret = pn547_bulk_enable(pdev);
	if (ret < 0)
	{
		pr_err("%s: regulator_enable failed, ret:%d\n", __func__, ret);
		goto failed;
	}
	msleep(20);

	/*write CORE_RESET_CMD*/
	ret=pn547_i2c_write(pdev,send_reset,sizeof(send_reset));
	if(ret < 0)
	{
		pr_err("%s: pn547_i2c_write failed, ret:%d\n", __func__, ret);
		goto failed;
	}
	/*read response*/
	ret=pn547_i2c_read(pdev,recvBuf,_read_sample_test_size);
	if(ret < 0)
	{
		pr_err("%s: pn547_i2c_read failed, ret:%d\n", __func__, ret);
		goto failed;
	}

	udelay(500);
	/*write CORE_INIT_CMD*/
	ret=pn547_i2c_write(pdev,init_cmd,sizeof(init_cmd));
	if(ret < 0)
	{
		pr_err("%s: pn547_i2c_write failed, ret:%d\n", __func__, ret);
		goto failed;
	}
	/*read response*/
	ret=pn547_i2c_read(pdev,recvBuf,_read_sample_test_size);
	if(ret < 0)
	{
		pr_err("%s: pn547_i2c_read failed, ret:%d\n", __func__, ret);
		goto failed;
	}

	udelay(500);
	/*write NCI_PROPRIETARY_ACT_CMD*/
	ret=pn547_i2c_write(pdev,read_config,sizeof(read_config));
	if(ret < 0)
	{
		pr_err("%s: pn547_i2c_write failed, ret:%d\n", __func__, ret);
		goto failed;
	}
	/*read response*/
	ret=pn547_i2c_read(pdev,recvBuf,_read_sample_test_size);
	if(ret < 0)
	{
		pr_err("%s: pn547_i2c_read failed, ret:%d\n", __func__, ret);
		goto failed;
	}

	udelay(500);

	/*write TEST_SWP_CMD UICC*/
	ret=pn547_i2c_write(pdev,read_config_UICC,sizeof(read_config_UICC));
	if(ret < 0)
	{
		pr_err("%s: pn547_i2c_write failed, ret:%d\n", __func__, ret);
		goto failed;
	}
	/*read response*/
	ret=pn547_i2c_read(pdev,recvBuf,_read_sample_test_size);
	if(ret < 0)
	{
		pr_err("%s: pn547_i2c_read failed, ret:%d\n", __func__, ret);
		goto failed;
	}

	mdelay(10);
	/*read notification*/
	ret=pn547_i2c_read(pdev,recvBuf,_read_sample_test_size);
	if(ret < 0)
	{
		pr_err("%s: pn547_i2c_read failed, ret:%d\n", __func__, ret);
		goto failed;
	}

	if(recvBuf[0] == 0x6F && recvBuf[1] == 0x3E && recvBuf[3] == 0x00)
	{
		pdev->sim_status |= UICC_SUPPORT_CARD_EMULATION;
	}

	/*write TEST_SWP_CMD eSE*/
	ret=pn547_i2c_write(pdev,read_config_eSE,sizeof(read_config_eSE));
	if(ret < 0)
	{
		pr_err("%s: pn547_i2c_write failed, ret:%d\n", __func__, ret);
		goto failed;
	}
	/*read response*/
	ret=pn547_i2c_read(pdev,recvBuf,_read_sample_test_size);
	if(ret < 0)
	{
		pr_err("%s: pn547_i2c_read failed, ret:%d\n", __func__, ret);
		goto failed;
	}

	mdelay(10);
	/*read notification*/
	ret=pn547_i2c_read(pdev,recvBuf,_read_sample_test_size);
	if(ret < 0)
	{
		pr_err("%s: pn547_i2c_read failed, ret:%d\n", __func__, ret);
		goto failed;
	}

	if(recvBuf[0] == 0x6F && recvBuf[1] == 0x3E && recvBuf[3] == 0x00)
	{
		pdev->sim_status |= eSE_SUPPORT_CARD_EMULATION;
	}

	return pdev->sim_status;
failed:
	pdev->sim_status = ret;
	return ret;

}

static ssize_t nfc_fwupdate_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{

	if ('1' == buf[0]) {
		firmware_update = 1;
		printk("%s:firmware update success\n", __func__);
	}

	return (ssize_t)count;
}

static ssize_t nfc_fwupdate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return (ssize_t)(snprintf(buf, sizeof(firmware_update)+1, "%d", firmware_update));
}

static ssize_t nxp_config_name_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	return (ssize_t)count;
}

static ssize_t nxp_config_name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return (ssize_t)(snprintf(buf, strlen(g_nfc_nxp_config_name)+1, "%s", g_nfc_nxp_config_name));
}
/*
	function: check which sim card support card Emulation.
	return value:
		eSE		UICC	value
		0		0		0	(not support)
		0		1		1	(swp1 support)
		1		0		2	(swp2 support)
		1		1		3	(all support)
		<0	:error
*/
static ssize_t nfc_sim_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int status=-1;
	struct i2c_client *i2c_client_dev = container_of(dev, struct i2c_client, dev);
	struct pn547_dev *pn547_dev;
	pn547_dev = i2c_get_clientdata(i2c_client_dev);
	if(pn547_dev == NULL)
	{
		pr_err("%s:  pn547_dev == NULL!\n", __func__);
		return status;
	}
	status = check_sim_status(i2c_client_dev,pn547_dev);
	if(status < 0)
	{
		pr_err("%s:  check_sim_status error!\n", __func__);
	}
	return (ssize_t)(sprintf(buf, "%d\n", status));
}

static ssize_t nfc_sim_switch_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct i2c_client *i2c_client_dev = container_of(dev, struct i2c_client, dev);
	struct pn547_dev *pn547_dev;
	int val = 0;

	pn547_dev = i2c_get_clientdata(i2c_client_dev);
	if(pn547_dev == NULL)
	{
		pr_err("%s:  pn547_dev == NULL!\n", __func__);
		return 0;
	}
	if (sscanf(buf, "%d", &val) == 1) {
		if (val < CARD1_SELECT || val > CARD2_SELECT)
		{
			pn547_dev->sim_switch = CARD1_SELECT;	//defaut UICC
			return -EINVAL;
		}
	}
	else
		return -EINVAL;
	switch(val){
		case CARD1_SELECT:
			pn547_dev->sim_switch = CARD1_SELECT;
			break;
		case CARD2_SELECT:
			pn547_dev->sim_switch = CARD2_SELECT;
			break;
		default:
			break;
	}
	return (ssize_t)count;
}

static ssize_t nfc_sim_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *i2c_client_dev = container_of(dev, struct i2c_client, dev);
	struct pn547_dev *pn547_dev;
	pn547_dev = i2c_get_clientdata(i2c_client_dev);
	if(pn547_dev == NULL)
	{
		pr_err("%s:  pn547_dev == NULL!\n", __func__);
		return 0;
	}
	return (ssize_t)(sprintf(buf, "%d\n", pn547_dev->sim_switch));
}
static ssize_t rd_nfc_sim_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int status=-1;
	struct i2c_client *i2c_client_dev = container_of(dev, struct i2c_client, dev);
	struct pn547_dev *pn547_dev;
	pn547_dev = i2c_get_clientdata(i2c_client_dev);
	if(pn547_dev == NULL)
	{
		pr_err("%s:  pn547_dev == NULL!\n", __func__);
		return status;
	}
	return (ssize_t)(sprintf(buf, "%d\n", pn547_dev->sim_status));
}

static struct device_attribute pn547_attr[] ={

	__ATTR(nfc_fwupdate, 0664, nfc_fwupdate_show, nfc_fwupdate_store),
	__ATTR(nxp_config_name, 0664, nxp_config_name_show, nxp_config_name_store),
	__ATTR(nfc_sim_switch, 0664, nfc_sim_switch_show, nfc_sim_switch_store),
	__ATTR(nfc_sim_status, 0444, nfc_sim_status_show, NULL),
	__ATTR(rd_nfc_sim_status, 0444, rd_nfc_sim_status_show, NULL),
};
static int create_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(pn547_attr); i++) {
		if (device_create_file(dev, pn547_attr + i)) {
			goto error;
		}
	}

	return 0;
error:
	for ( ; i >= 0; i--) {
		device_remove_file(dev, pn547_attr + i);
	}

	pr_err("%s:pn547 unable to create sysfs interface.\n", __func__ );
	return -1;
}

static int remove_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(pn547_attr); i++) {
		device_remove_file(dev, pn547_attr + i);
	}

	return 0;
}
static int nfc_ven_low_beforepwd(struct notifier_block *this, unsigned long code,
                                                                void *unused)
{
	int retval = 0;

	retval = pn547_bulk_enable(nfcdev);

	if (retval < 0) {
		pr_err("%s: regulator_disable failed, ret:%d\n", __func__, retval);
		return retval;
	}

	return retval;
}

static struct notifier_block nfc_ven_low_notifier = {
	.notifier_call = nfc_ven_low_beforepwd,
};

static int check_pn547(struct i2c_client *client, struct  pn547_dev *pdev)
{
	int ret = -1;
	int count = 0;
	const char host_to_pn547[1] = {0x20};
	const char firm_dload_cmd[8]={0x00, 0x04, 0xD0, 0x09, 0x00, 0x00, 0xB1, 0x84};

	ret = pn547_bulk_enable(pdev);
	if (ret < 0) {
		pr_err("%s: regulator_enable failed, ret:%d\n", __func__, ret);
		return ret;
	}

	msleep(20);
	ret = pn547_bulk_disable(pdev);
	if (ret < 0) {
		pr_err("%s: regulator_disable failed, ret:%d\n", __func__, ret);
		return ret;
	}

	msleep(60);
	ret = pn547_bulk_enable(pdev);
	if (ret < 0) {
		pr_err("%s: regulator_enable failed, ret:%d\n", __func__, ret);
		return ret;
	}
	msleep(20);

	do{
		ret = i2c_master_send(client,  host_to_pn547, sizeof(host_to_pn547));
		if (ret < 0) {
			pr_err("%s:pn547_i2c_write failed and ret = %d,at %d times\n", __func__,ret,count);
		} else {
			printk("%s:pn547_i2c_write success and ret = %d,at %d times\n",__func__,ret,count);
			msleep(10);
			ret = pn547_enable_nfc(pdev);
			if (ret < 0) {
				pr_err("%s: pn547_enable_nfc exit i2c check failed, ret:%d\n", __func__, ret);
				return ret;
			}
			return ret;
		}
		count++;
		msleep(10);
	}while(count <NFC_TRY_NUM);

	for (count = 0; count < NFC_TRY_NUM;count++)
	{
		gpio_set_value(pdev->firm_gpio, 1);
		ret = pn547_enable_nfc(pdev);
		if (ret < 0) {
			pr_err("%s: pn547_enable_nfc enter firmware failed, ret:%d\n", __func__, ret);
			return ret;
		}

		ret = i2c_master_send(client,  firm_dload_cmd, sizeof(firm_dload_cmd));
		if (ret < 0) {
			printk("%s:pn547_i2c_write download cmd failed:%d, ret = %d\n", __func__, count, ret);
			continue;
		}

		gpio_set_value(pdev->firm_gpio, 0);
		ret = pn547_enable_nfc(pdev);
		if (ret < 0) {
			pr_err("%s: pn547_enable_nfc exit firmware failed, ret:%d\n", __func__, ret);
			return ret;
		}
		break;
	}
	return ret;
}
static int pn547_get_resource(struct  pn547_dev *pdev,  struct i2c_client *client)
{
	int ret;
	pdev->irq_gpio = of_get_named_gpio(client->dev.of_node, "pn547,irq", 0);
	if (!gpio_is_valid(pdev->irq_gpio)) {
		pr_err("failed to get irq!\n");
		return -ENODEV;
	}

	pdev->firm_gpio = of_get_named_gpio(client->dev.of_node, "pn547,dload", 0);
	if (!gpio_is_valid(pdev->firm_gpio)) {
		pr_err("failed to get firm!\n");
		return -ENODEV;
	}

	pdev->pctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(pdev->pctrl)) {
        pdev->pctrl = NULL;
		pr_err("failed to get clk pin!\n");
		return -ENODEV;
	}
	pdev->pins_default = pinctrl_lookup_state(pdev->pctrl, "default");
	pdev->pins_idle = pinctrl_lookup_state(pdev->pctrl, "idle");
	ret = pinctrl_select_state(pdev->pctrl, pdev->pins_default);
	if (ret < 0){
		pr_err("%s: unapply new state!\n", __func__);
		return -ENODEV;
	}

	pdev->nfc_clk = devm_clk_get(&client->dev, NFC_CLK_PIN);
	if (IS_ERR(pdev->nfc_clk )) {
	    pr_err("failed to get clk out\n");
		return -ENODEV;
	} else {
		ret = clk_set_rate(pdev->nfc_clk , DEFAULT_NFC_CLK_RATE);
		if (ret < 0) {
		     return -EINVAL;
		}
	}
	pdev->ven_felica.supply =  "pn547ven";
	ret = devm_regulator_bulk_get(&client->dev, 1, &(pdev->ven_felica));
	if (ret) {
		pr_err("failed to get ven felica!\n");
		clk_put(pdev->nfc_clk);
		pdev->nfc_clk = NULL;
		return -ENODEV;
	}

	return 0;
}

static struct of_device_id pn547_i2c_dt_ids[] = {
	{.compatible = "hisilicon,pn547_nfc" },
	{}
};

static int pn547_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret;
	struct pn547_dev *pn547_dev;

	if (!of_match_device(pn547_i2c_dt_ids, &client->dev)) {
		pr_err("[%s,%d]: pn547 NFC match fail !\n", __FUNCTION__, __LINE__);
		return -ENODEV;
	}
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : need I2C_FUNC_I2C\n", __func__);
		return  -ENODEV;
	}

	ret = create_sysfs_interfaces(&client->dev);
	if (ret < 0) {
		pr_err("Failed to create_sysfs_interfaces\n");
		return -ENODEV;
	}

	pn547_dev = kzalloc(sizeof(*pn547_dev), GFP_KERNEL);
	if (pn547_dev == NULL) {
		pr_err("failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}
	/* get clk ,gpio,supply resource from dts*/
	ret = pn547_get_resource(pn547_dev, client);
	if (ret < 0) {
		pr_err("%s: pn547 probe get resource failed \n",__func__);
		goto err_pn547_get_resource;
	}

	ret = gpio_request(pn547_dev->firm_gpio,NULL);
	if (ret < 0) {
		pr_err("%s: gpio_request failed, ret:%d.\n", __func__,
			pn547_dev->firm_gpio);
		goto err_firm_gpio_request;
	}

	gpio_direction_output(pn547_dev->firm_gpio,0);


	/*using i2c to find nfc device */
	ret = check_pn547(client, pn547_dev);
	if(ret < 0){
		pr_err("%s: pn547 probe failed \n",__func__);
		goto err_check_pn547;
	}

	ret = pn547_bulk_disable(pn547_dev);
	if (ret < 0) {
		pr_err("%s:pn547_bulk_disable failed; ret:%d\n", __func__, ret);
	}

	pn547_dev->client   = client;
	/* init mutex and queues */
	init_waitqueue_head(&pn547_dev->read_wq);
	mutex_init(&pn547_dev->read_mutex);
	spin_lock_init(&pn547_dev->irq_enabled_lock);
	/* Initialize wakelock*/
	wake_lock_init(&wlock_read, WAKE_LOCK_SUSPEND, "nfc_read");
	/*register pn544 char device*/
	pn547_dev->pn547_device.minor = MISC_DYNAMIC_MINOR;
	pn547_dev->pn547_device.name = "pn544";
	pn547_dev->pn547_device.fops = &pn547_dev_fops;
	ret = misc_register(&pn547_dev->pn547_device);
	if (ret) {
		pr_err("%s : misc_register failed\n", __FILE__);
		goto err_misc_register;
	}

	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	pn547_dev->irq_enabled = true;
	ret = gpio_request(pn547_dev->irq_gpio,NULL);
	if (ret < 0) {
		pr_err("%s: gpio_request failed, ret:%d.\n", __func__,
			pn547_dev->irq_gpio);
		goto err_misc_register;
	}
	gpio_direction_input(pn547_dev->irq_gpio);
	client->irq = gpio_to_irq(pn547_dev->irq_gpio);
	ret = request_irq(client->irq, pn547_dev_irq_handler,
				IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND, client->name, pn547_dev);
	if (ret) {
		pr_err("request_irq client->irq=%d failed\n",client->irq);
		goto err_request_irq_failed;
	}
	pn547_disable_irq(pn547_dev);

	/*sim_select = 1,UICC select*/
	pn547_dev->sim_switch = CARD1_SELECT;
	pn547_dev->sim_status = CARD_UNKNOWN;
	/* set device data to client devices*/
	i2c_set_clientdata(client, pn547_dev);

	nfcdev = pn547_dev;
	/*notifier for supply shutdown*/
	ret = register_reboot_notifier(&nfc_ven_low_notifier);
	if (ret != 0) {
		pr_info(KERN_ERR "cannot register reboot notifier (err=%d)\n", ret);
		goto err_request_irq_failed;
	}

	/*enbale the 19.2M clk*/
	ret = clk_prepare_enable(pn547_dev->nfc_clk);
	if (ret) {
		pr_err("Enable NFC clk failed, ret=%d\n", ret);
		goto err_request_irq_failed;
	}

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
    /* detect current device successful, set the flag as present */
    set_hw_dev_flag(DEV_I2C_NFC);
#endif

	set_nfc_nxp_config_name();

	pr_err("NFC probe success\n");

	return 0;

err_request_irq_failed:
	gpio_free(pn547_dev->irq_gpio);
err_misc_register:
	misc_deregister(&pn547_dev->pn547_device);
	wake_lock_destroy(&wlock_read);
	mutex_destroy(&pn547_dev->read_mutex);
err_check_pn547:
	gpio_free(pn547_dev->firm_gpio);
err_firm_gpio_request:
	if(pn547_dev->nfc_clk)
	{
		clk_put(pn547_dev->nfc_clk);
		pn547_dev->nfc_clk = NULL;
		ret = pinctrl_select_state(pn547_dev->pctrl, pn547_dev->pins_idle);
		if (ret < 0){
			pr_err("%s: unapply new state!\n", __func__);
		}
	}
err_pn547_get_resource:
	kfree(pn547_dev);
err_exit:
	remove_sysfs_interfaces(&client->dev);
	return ret;
}

static int pn547_remove(struct i2c_client *client)
{
	struct pn547_dev *pn547_dev;
	int ret = 0;

	unregister_reboot_notifier(&nfc_ven_low_notifier);
	pn547_dev = i2c_get_clientdata(client);
	free_irq(client->irq, pn547_dev);
	misc_deregister(&pn547_dev->pn547_device);
	wake_lock_destroy(&wlock_read);
	mutex_destroy(&pn547_dev->read_mutex);
	gpio_free(pn547_dev->irq_gpio);
	gpio_free(pn547_dev->firm_gpio);
	remove_sysfs_interfaces(&client->dev);
	if(pn547_dev->nfc_clk)
	{
	   clk_put(pn547_dev->nfc_clk);
	   pn547_dev->nfc_clk = NULL;
	   ret = pinctrl_select_state(pn547_dev->pctrl, pn547_dev->pins_idle);
	   if (ret < 0){
		   pr_err("%s: unapply new state!\n", __func__);
	   }
	}
	kfree(pn547_dev);

	return 0;
}

static const struct i2c_device_id pn547_id[] = {
	{ "pn547", 0 },
	{ }
};

#ifdef CONFIG_PM
static int pn547_suspend(struct i2c_client *client, pm_message_t mesg)
{
//	struct pn547_dev *pn547_dev;
//	int ret = 0;

	printk("[%s] +\n", __func__);

#if 0
	pn547_dev = i2c_get_clientdata(client);

	clk_disable_unprepare(pn547_dev->nfc_clk);
	ret = pinctrl_select_state(pn547_dev->pctrl, pn547_dev->pins_idle);
	if (ret < 0){
		pr_err("%s: unapply new state!\n", __func__);
	}
#endif
	printk("[%s] -\n", __func__);

	return 0;
}
static int pn547_resume(struct i2c_client *client)
{
//	int ret = -1;
//	struct pn547_dev *pn547_dev;

	printk("[%s] +\n", __func__);

#if 0
	pn547_dev = i2c_get_clientdata(client);

	ret = pinctrl_select_state(pn547_dev->pctrl, pn547_dev->pins_default);
	if (ret < 0){
		pr_err("%s: unapply new state!\n", __func__);
	}

	ret = clk_prepare_enable(pn547_dev->nfc_clk);
	if (ret < 0) {
		printk("%s: clk_enable failed, ret:%d\n", __func__, ret);
	}
#endif

	printk("[%s] -\n", __func__);

	return 0;
}
#else
#define pn547_suspend      NULL
#define pn547_resume       NULL
#endif /* CONFIG_PM */

MODULE_DEVICE_TABLE(of, pn547_i2c_dt_ids);
static struct i2c_driver pn547_driver = {
	.id_table	= pn547_id,
	.probe		= pn547_probe,
	.remove		= pn547_remove,
	.suspend	= pn547_suspend,
	.resume 	= pn547_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "pn544",
		.of_match_table = of_match_ptr(pn547_i2c_dt_ids),
	},
};
/*
 * module load/unload record keeping
 */
module_i2c_driver(pn547_driver);
MODULE_AUTHOR("Sylvain Fonteneau");
MODULE_DESCRIPTION("NFC pn547 driver");
MODULE_LICENSE("GPL");
