/* Copyright (c) 2009-2013,	Code Aurora	Forum. All rights reserved.
 *
 * This	program	is free	software; you can redistribute it and/or modify
 * it under	the	terms of the GNU General Public	License	version	2 and
 * only	version	2 as published by the Free Software	Foundation.
 *
 * This	program	is distributed in the hope that	it will	be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A	PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received	a copy of the GNU General Public License
 * along with this program;	if not,	write to the Free Software
 * Foundation, Inc., 51	Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
/*****************************************************************************
File name: bluetooth_power.c
Description: Bluetooth Power Switch Module, Controls power to external 
             bluetooth device with interface to power management device.
Author: s00194326
Version: 1.0
Date: 2013-10-24
History: NA
*****************************************************************************/
 
/*
 * Bluetooth Power Switch Module
 * controls	power to external Bluetooth	device
 * with	interface to power management device
 */


#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>

#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/rfkill.h>
#include <linux/delay.h>

#define DTS_COMP_BLUETOOTH_POWER_NAME "huawei,bluetooth_power"
#define BT_VDDIO_LDO_18V 1800000

typedef struct bluetooth_power_private_data {
	struct gpio	gpio_enable;		// gpio BT_enable
	struct gpio	gpio_bt_wake_host;		// gpio BT_wake_host
	struct gpio	gpio_host_wake_bt;		// gpio host_wake_BT
	struct pinctrl *bt_uart;			// bt uart
	struct pinctrl_state *bt_uart_default;
	struct pinctrl_state *bt_uart_idle;
	struct clk *clk;                    // bt 32k clock
	struct regulator *vdd;              // bt regulator
	struct rfkill *rfkill;
	bool   previous;
	unsigned pwr_count;                 // only if count equals 0, then open bluetooth
}bluetooth_device_data;

/*************************************************
Function:       bluetooth_power_handle
Description:    power on / power off  Bluetooth
Calls:              bluetooth_power_handle()
Input:          platform_device *pdev
                bluetooth_power_private_data *p_dev_data
Output:         
Return:         int ret
Others:         NA
*************************************************/
static int bluetooth_power_handle(struct bluetooth_power_private_data *p_dev_data, int on){
	int ret = 0;
	printk("%s,%d.\n", __func__, on);

	/* power on	the	bluetooth */
	if(on)
	{
		if(p_dev_data->pwr_count == 0)
		{
			printk("%s, bluetooth power on in\n", __func__);
			/*SET NORMAL*/

			/*enable bt	sleep clk*/
			printk("enable bluetooth 32 clk======>\n");
			ret	= clk_prepare_enable(p_dev_data->clk);
			if (unlikely(ret < 0)){
				pr_err("%s : enable	clk	failed %d",__func__, ret);
				goto clk_err;
			}
            msleep(5);
			printk("power on gpio BT_EN \n");
			gpio_set_value(p_dev_data->gpio_enable.gpio, 1);
			msleep(5);
			p_dev_data->pwr_count = 1;
			printk("%s, bluetooth power on out. bt enable gpio is %d\n", __func__, p_dev_data->gpio_enable.gpio);

		}/* end if pwr_count == 0 */
	}
	else
	{
		/* power off the bluetooth */
		if( p_dev_data->pwr_count == 1)
		{
			printk("%s, bluetooth power off in \n", __func__);

			gpio_set_value(p_dev_data->gpio_enable.gpio, 0);
			msleep(5); 

			/*disable sleep clk*/
			clk_disable(p_dev_data->clk);

			p_dev_data->pwr_count = 0;
			printk("%s, bluetooth power off out \n", __func__);

		}/* end if pwr_count == 1 */
	}/* end else on == 0 */
	
	return ret;

clk_err:
	//pinctrl_select_state(bluetooth_dev_data.bt_uart, bluetooth_dev_data.bt_uart_idle); 
	return ret;
}

/*************************************************
Function:       bluetooth_toggle_radio
Description:    be called by system when rfkill is triggered
Calls:              bluetooth_power_handle()
Input:          platform_device *pdev
                bluetooth_power_private_data *p_dev_data
Output:         
Return:         int ret
Others:         NA
*************************************************/
static int bluetooth_toggle_radio(void *data, bool blocked)
{
	int	ret	= 0;
	struct bluetooth_power_private_data *pdev_bluetooth_power_data = NULL;

	printk(KERN_INFO "bluetooth_toggle_radio in \n");
	
       if( data == NULL )
               return ret;

	pdev_bluetooth_power_data = platform_get_drvdata(data);
	if (NULL == pdev_bluetooth_power_data) {
		pr_err("%s:	pdev_bluetooth_power_data is NULL \n", __func__);
		return ret;
	}

	printk(KERN_INFO "bluetooth_toggle_radio blocked: %d , previous: %d \n", blocked, pdev_bluetooth_power_data->previous );
	
       if (pdev_bluetooth_power_data->previous != blocked) {
		ret = bluetooth_power_handle(pdev_bluetooth_power_data, (!blocked));
		pdev_bluetooth_power_data->previous =	blocked;
	}

	printk(KERN_INFO "bluetooth_toggle_radio out \n");
	   
	return ret;
}

static const struct	rfkill_ops bluetooth_power_rfkill_ops = {
	.set_block = bluetooth_toggle_radio,
};

/*************************************************
Function:       bluetooth_power_rfkill_probe
Description:    rfkill init function
Calls:              rfkill_alloc()
                    rfkill_init_sw_state()
                    rfkill_register()
                    rfkill_destroy()
Input:          platform_device *pdev
                bluetooth_power_private_data *p_dev_data
Output:         
Return:         int ret
Others:         NA
*************************************************/
static int bluetooth_power_rfkill_probe(struct platform_device *pdev, 
                struct bluetooth_power_private_data *p_dev_data)
{
	int ret = 0;

	printk(KERN_INFO "bluetooth_power_rfkill_probe in \n");

	/* alloc memery for rfkill */
	p_dev_data->rfkill = rfkill_alloc("bt_power", 
	&pdev->dev, RFKILL_TYPE_BLUETOOTH, &bluetooth_power_rfkill_ops, pdev);
	if(!p_dev_data->rfkill){
		dev_err(&pdev->dev,	"bluetooth rfkill allocate failed\n");
		return -ENOMEM;
	}

	/* force Bluetooth off during init to allow	for	user control */
	rfkill_init_sw_state( p_dev_data->rfkill, 1);
	p_dev_data->previous =	1;

	ret	= rfkill_register( p_dev_data->rfkill );
	if (ret) {
		dev_err(&pdev->dev,	"rfkill	register failed=%d\n", ret);
		goto rfkill_failed;
	}

	printk(KERN_INFO "bluetooth_power_rfkill_probe out \n");

	return ret;

rfkill_failed:
	rfkill_destroy( p_dev_data->rfkill );
	return ret;
}

/*************************************************
Function:       bluetooth_power_probe
Description:    bluetooth power driver probe function
Calls:          bluetooth_power_rfkill_probe
Input:          platform_device *pdev
Output:         
Return:         int ret
Others:         NA
*************************************************/
static int bluetooth_power_probe(struct platform_device *pdev){
	struct device *bluetooth_power_dev = &pdev->dev;
	struct device_node *np = bluetooth_power_dev->of_node;
	struct bluetooth_power_private_data *pdev_bluetooth_power_data = NULL;
	int ret;

	pdev_bluetooth_power_data = devm_kzalloc(bluetooth_power_dev, sizeof(struct bluetooth_power_private_data), GFP_KERNEL);
	if ( pdev_bluetooth_power_data == NULL ) {
		dev_err(bluetooth_power_dev, "failed to allocate bluetooth_power_private_data\n");
		return -ENOMEM;
	}

	printk(KERN_INFO "bluetooth_power probe in \n");

	of_property_read_u32(np, "huawei,bt_en", &(pdev_bluetooth_power_data->gpio_enable.gpio));

	of_property_read_u32(np, "huawei,bt_wake_host", &(pdev_bluetooth_power_data->gpio_bt_wake_host.gpio));

	of_property_read_u32(np, "huawei,host_wake_bt", &(pdev_bluetooth_power_data->gpio_host_wake_bt.gpio));

	printk(KERN_INFO "devm clk get \n");

	pdev_bluetooth_power_data->clk = devm_clk_get(bluetooth_power_dev, NULL);
	printk(KERN_INFO "clk is 0x%x\n", (int)pdev_bluetooth_power_data->clk);
	if (IS_ERR( pdev_bluetooth_power_data->clk)) {
		printk(KERN_INFO "Get bt 32k clk failed\n");
		ret = PTR_ERR( pdev_bluetooth_power_data->clk );
		goto err_free_clk_ret;
	}

	/*printk(KERN_INFO "regulator_get in\n");
	pdev_bluetooth_power_data->vdd = regulator_get(bluetooth_power_dev,"huawei,bt_power");   // regulator is ldo14
	if (IS_ERR( pdev_bluetooth_power_data->vdd )) {
		printk(KERN_INFO "bt regulator_get failed\n");
		pdev_bluetooth_power_data->vdd = NULL;
		ret	= -ENOMEM;
		goto err_free_clk_ret; // free memory
	}
	printk(KERN_INFO "regulator_get out\n");*/

	ret	= gpio_request( pdev_bluetooth_power_data->gpio_enable.gpio , "bt_gpio_enable" );
	if (ret < 0) {
		pr_err("%s: gpio_request %d  failed, ret:%d .\n", __func__, pdev_bluetooth_power_data->gpio_enable.gpio ,ret);
		goto err_free_clk_ret;
	}

	ret	= gpio_request( pdev_bluetooth_power_data->gpio_host_wake_bt.gpio , "host_wake_bt" );
	if (ret < 0) {
		pr_err("%s: gpio_request %d  failed, ret:%d .\n", __func__, pdev_bluetooth_power_data->gpio_host_wake_bt.gpio ,ret);
		goto err_free_clk_ret;
	}

	ret	= gpio_request( pdev_bluetooth_power_data->gpio_bt_wake_host.gpio , "bt_wake_host" );
	if (ret < 0) {
		pr_err("%s: gpio_request %d  failed, ret:%d .\n", __func__, pdev_bluetooth_power_data->gpio_bt_wake_host.gpio ,ret);
		goto err_free_clk_ret;
	}

	ret	= gpio_direction_output( pdev_bluetooth_power_data->gpio_enable.gpio , 0);
	if (ret < 0) {
		pr_err("%s: gpio_direction_output %d  failed, ret:%d .\n", __func__, pdev_bluetooth_power_data->gpio_enable.gpio ,ret);
		goto err_free_clk_ret;
	}

	ret	= gpio_direction_output( pdev_bluetooth_power_data->gpio_host_wake_bt.gpio , 0);
	if (ret < 0) {
		pr_err("%s: gpio_direction_output %d  failed, ret:%d .\n", __func__, pdev_bluetooth_power_data->gpio_host_wake_bt.gpio ,ret);
		goto err_free_clk_ret;
	}

	ret	= gpio_direction_input( pdev_bluetooth_power_data->gpio_bt_wake_host.gpio);
	if (ret < 0) {
		pr_err("%s: gpio_direction_output %d  failed, ret:%d .\n", __func__, pdev_bluetooth_power_data->gpio_bt_wake_host.gpio ,ret);
		goto err_free_clk_ret;
	}

	/* set bluetooth vddio voltage to 1.8V */
	/*printk(KERN_INFO "regulator_set_voltage in\n");
	ret	= regulator_set_voltage( pdev_bluetooth_power_data->vdd, BT_VDDIO_LDO_18V, BT_VDDIO_LDO_18V );
	printk(KERN_INFO "regulator_set_voltage out\n");
	if (ret) {
		dev_err(&pdev->dev,	"%s: bluetooth regulator_set_voltage err %d\n", __func__, ret);
		goto err_free_clk_ret;
	}*/

	ret	= bluetooth_power_rfkill_probe(pdev, pdev_bluetooth_power_data);
	if (ret) {
		dev_err(&pdev->dev,	"bluetooth_power_rfkill_probe failed, ret:%d\n", ret);
		goto err_free_clk_ret;
	}

	// other code
	platform_set_drvdata(pdev, pdev_bluetooth_power_data);
	
	printk(KERN_INFO "bluetooth_power probe out \n");

	return ret;
	
err_free_clk_ret:
	devm_clk_put(&pdev->dev, pdev_bluetooth_power_data->clk);

	return ret;
}
            
static int bluetooth_power_remove(struct platform_device *pdev)
{
	//devm_kfree();  TODO
	return 0;
}

static int bluetooth_power_suspend(struct platform_device *pdev, pm_message_t state)
{
    printk("%s+.\n",__func__);
	// pinctrl_select_state(bluetooth_dev_data.bt_uart, bluetooth_dev_data.bt_uart_idle);
	// other code
	printk("%s-.\n",__func__);
	return 0;
}

static int bluetooth_power_resume(struct platform_device *pdev)
{
	printk("%s+.\n",__func__);
	// pinctrl_select_state(bluetooth_dev_data.bt_uart, bluetooth_dev_data.bt_uart_default);
	// other code
	printk("%s-.\n",__func__);
	return 0;
}

static const struct of_device_id bluetooth_power_match_table[] = {
	{
		.compatible = DTS_COMP_BLUETOOTH_POWER_NAME,   // compatible must match with which defined in dts
		.data = NULL,
	},
	{
	},
};

MODULE_DEVICE_TABLE(of, bluetooth_power_match_table);

static struct platform_driver bluetooth_power_driver = {
	.probe = bluetooth_power_probe,
	.remove = bluetooth_power_remove,
	.suspend = bluetooth_power_suspend,
	.resume = bluetooth_power_resume,   // TODO no shutdown ?
	
	.driver = {
		.name = "huawei,bluetooth_power",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(bluetooth_power_match_table), // dts required code
	},

};

static int __init bluetooth_power_init(void)
{
    int ret ;
	ret = platform_driver_register(&bluetooth_power_driver);
	return ret;
}

static void __exit bluetooth_power_exit(void)
{

	platform_driver_unregister(&bluetooth_power_driver);
}

module_init(bluetooth_power_init);
module_exit(bluetooth_power_exit);

MODULE_LICENSE("GPL	v2");
