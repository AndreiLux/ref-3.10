#include <linux/amba/bus.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/module.h>
//#include <linux/mux.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/system.h>
#include <asm/pmu.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/rfkill.h>
#include <linux/delay.h>

#define DTS_COMP_GPS_POWER_NAME "huawei,gps_power"
#define GPS_VDDIO_LDO_27V 2700000

typedef struct gps_bcm_info {
	struct gpio       gpioid_en;
	struct clk          *clk;
	struct gpio       gpioid_power;
	/* Begin: Added for E911 */
//	struct gpio       gpioid_refclk;
	/* End: Added for E911 */
} GPS_BCM_INFO;


static int k3_gps_bcm_probe(struct platform_device *pdev)
{
	GPS_BCM_INFO *gps_bcm;
	struct device *gps_power_dev = &pdev->dev;
	int ret = 0;
	struct device_node *np = gps_power_dev->of_node;;
	
printk(KERN_INFO "[GPS] start find gps_power\n");
	gps_bcm = kzalloc(sizeof(GPS_BCM_INFO), GFP_KERNEL);
	if (!gps_bcm) {
		dev_err(&pdev->dev, "[GPS] Alloc memory failed\n");
		return -ENOMEM;
	}


	if(of_property_read_u32(np, "huawei,gps_en", &gps_bcm->gpioid_en.gpio) < 0)		//gps_en
	{
		dev_err(&pdev->dev, "[GPS] of_property_read_u32 failed\n");
		goto err_free;
	}

	ret = gpio_request(gps_bcm->gpioid_en.gpio, "gps_enbale");
	if (ret < 0) {
		pr_err("[GPS] %s: gpio_direction_output %d  failed, ret:%d .\n", __func__, gps_bcm->gpioid_en.gpio ,ret);
		goto err_free;
	}
	
	gpio_export(gps_bcm->gpioid_en.gpio, false);
	ret	= gpio_direction_output( gps_bcm->gpioid_en.gpio , 0);
	printk(KERN_INFO "[GPS] OoO gpio_direction_output X \n");
	if (ret < 0) {
		pr_err("[GPS] %s: gpio_direction_output %d  failed, ret:%d .\n", __func__, gps_bcm->gpioid_en.gpio ,ret);
		goto err_free;
	}

printk(KERN_INFO "[GPS] finish gpio_direction_output gps_power\n");
	/* Get reset gpio */
	/*
	res = platform_get_resource(pdev, IORESOURCE_IO, 1);
	if (!res) {
		dev_err(&pdev->dev, "Get reset gpio resourse failed\n");
		ret = -ENXIO;
		goto err_free_gpio_en;
	}
	gps_bcm->gpioid_ret = res->start;

	ret = gpio_request(gps_bcm->gpioid_ret, "gps_reset");
	if (ret < 0) {
		dev_err(&pdev->dev,  "gpio_request failed, gpio=%lu, ret=%d\n", gps_bcm->gpioid_ret, ret);
		goto err_free_gpio_en;
	}
	gpio_export(gps_bcm->gpioid_ret, false);

	// Begin: Add for agps e911, get fref_clk gpio 
	res = platform_get_resource(pdev, IORESOURCE_IO, 2);
	if (!res) {
		dev_err(&pdev->dev, "Get fref_clk gpio resourse failed\n");
		ret = -ENXIO;
		goto err_free_gpio_en;
	}
	gps_bcm->gpioid_refclk = res->start;
	ret = gpio_request(gps_bcm->gpioid_refclk, "gps_refclk");
	if (ret < 0) {
		dev_err(&pdev->dev,  "request fref_clk gpio failed, gpio=%lu, ret=%d\n", gps_bcm->gpioid_refclk, ret);
		goto err_free_gpio_en;
	}
	dev_err(&pdev->dev,  "request fref_clk gpio ok, gpio=%lu", gps_bcm->gpioid_refclk);
	gpio_export(gps_bcm->gpioid_refclk, false);
	*/
	/* End: Add for agps e911 */

	/* Set 32KC clock */
	gps_bcm->clk = devm_clk_get(gps_power_dev, NULL);
	printk(KERN_INFO "[GPS] clk is 0x%x\n", (int)gps_bcm->clk);
	if (IS_ERR(gps_bcm->clk)) {
		printk(KERN_INFO "[GPS] clk is error 0x%x\n", (int)gps_bcm->clk);
		ret = PTR_ERR(gps_bcm->clk);
		goto err_free_clk;
	}
	ret = clk_prepare_enable(gps_bcm->clk);
	if(ret)
	{
		printk(KERN_INFO "[GPS] clk enable is failed\n");
		goto err_free_clk;
	}
	printk(KERN_INFO "[GPS] clk is finish\n");

	/* Begin: Add for E911 */
//	gpio_direction_output(gps_bcm->gpioid_refclk, 0);
//	gpio_set_value(gps_bcm->gpioid_refclk, 0);
//	dev_err(&pdev->dev,  "Low gpio_refclk \n");
	/* End: Add for E911 */
	kfree(gps_bcm);
	gps_bcm = NULL;
	return 0;

err_free_clk:
	clk_put(gps_bcm->clk);

//err_free_gpio_ret:
	//gpio_free(gps_bcm->gpioid_refclk);/* Add for E911 */
	//gpio_free(gps_bcm->gpioid_ret);

//err_free_gpio_en:
//	gpio_free(gps_bcm->gpioid_en.gpio);

err_free:
	kfree(gps_bcm);
	gps_bcm = NULL;
	return ret;
}


static int k3_gps_bcm_remove(struct platform_device *pdev)
{
	GPS_BCM_INFO *gps_bcm = platform_get_drvdata(pdev);
	int ret = 0;

	dev_dbg(&pdev->dev, "k3_gps_bcm_remove +\n");

	if (!gps_bcm) {
		dev_err(&pdev->dev, "gps_bcm is NULL\n");
		return 0;
	}

	clk_disable(gps_bcm->clk);
	clk_put(gps_bcm->clk);

	gpio_free(gps_bcm->gpioid_en.gpio);
	//gpio_free(gps_bcm->gpioid_ret);
	//gpio_free(gps_bcm->gpioid_refclk); /* Add for E911 */

	kfree(gps_bcm);
	gps_bcm = NULL;
	platform_set_drvdata(pdev, NULL);

	dev_dbg(&pdev->dev, "k3_gps_bcm_remove -\n");

	return ret;
}

static void K3_gps_bcm_shutdown(struct platform_device *pdev)
{
	GPS_BCM_INFO *gps_bcm = platform_get_drvdata(pdev);

	if (!gps_bcm) {
		dev_err(&pdev->dev, "gps_bcm is NULL\n");
		return;
	}

	printk("[%s] +\n", __func__);

	//gpio_set_value(gps_bcm->gpioid_en.gpio, 0);
	//gpio_set_value(gps_bcm->gpioid_ret, 0);
	//gpio_set_value(gps_bcm->gpioid_refclk, 0);/* Add for E911 */
	gpio_direction_output(gps_bcm->gpioid_en.gpio, 0);

	clk_disable(gps_bcm->clk);
	clk_put(gps_bcm->clk);

	printk("[%s] -\n", __func__);
}


#ifdef CONFIG_PM
static int  k3_gps_bcm_suspend(struct platform_device *pdev, pm_message_t state)
{
	GPS_BCM_INFO *gps_bcm = platform_get_drvdata(pdev);

	if (!gps_bcm) {
		dev_err(&pdev->dev, "gps_bcm is NULL\n");
		return 0;
	}

	printk("[%s] +\n", __func__);

	gpio_set_value(gps_bcm->gpioid_en.gpio, 0);

	printk("[%s] -\n", __func__);
	return 0;
}
#else

#define k3_gps_bcm_suspend	NULL

#endif /* CONFIG_PM */

static const struct of_device_id gps_power_match_table[] = {
	{
		.compatible = DTS_COMP_GPS_POWER_NAME,   // compatible must match with which defined in dts
		.data = NULL,
	},
	{
	},
};

MODULE_DEVICE_TABLE(of, gps_power_match_table);

static struct platform_driver k3_gps_bcm_driver = {
	.probe			= k3_gps_bcm_probe,
	.suspend		= k3_gps_bcm_suspend,
	.remove			= k3_gps_bcm_remove,
	.shutdown		= K3_gps_bcm_shutdown,
	.driver = {
		.name = "huawei,gps_power",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(gps_power_match_table), // dts required code
	},
};

static int __init k3_gps_bcm_init(void)
{
	return platform_driver_register(&k3_gps_bcm_driver);
}

static void __exit k3_gps_bcm_exit(void)
{
	platform_driver_unregister(&k3_gps_bcm_driver);
}


module_init(k3_gps_bcm_init);
module_exit(k3_gps_bcm_exit);

MODULE_AUTHOR("DRIVER_AUTHOR");
MODULE_DESCRIPTION("GPS Boardcom 47511 driver");
MODULE_LICENSE("GPL");
