/*
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/stat.h>
#include <linux/timer.h>
#include "../staging/android/timed_output.h"
#include <linux/drv2603-vibrator.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/regulator/machine.h>
#include <linux/string.h>



#if IS_ENABLED(CONFIG_PWM_ODIN)
#include <linux/pwm.h>
#include <linux/odin_pwm.h>
#define PCLK_NS  20			/*50MHz*/
#define VIB_PERIOD	 0x400
#endif



#define DEVICE_NAME		 "vibrator"

static DEFINE_MUTEX(vib_lock);

struct timed_vibrator_data {
	struct timed_output_dev dev;
	struct hrtimer timer;
	spinlock_t lock;
	atomic_t ms_time; /* vibrator duration */
	atomic_t vib_status;			/* on/off */
	atomic_t vibe_pwm;
	int haptic_en_gpio;
	int motor_pwm_gpio;
	int vibe_warmup_delay; /* in ms */
	int initial_vibrate_ms;
	int max_timeout_ms;
	int pwm_value;
	char *regulator_name;
	struct regulator *vreg_l12;
	struct pwm_device *pd;
	struct work_struct work_vibrator_off;
	struct work_struct work_vibrator_on;
};


static int vibrator_ic_enable_set(int enable, struct timed_vibrator_data *vib_data)
{

	int gpio;

	DRV2603_DBG_INFO("ic_enable=%d\n", enable);

	if (enable)
		gpio_direction_output(vib_data->haptic_en_gpio, 1);
	else
		gpio_direction_output(vib_data->haptic_en_gpio, 0);

	gpio = gpio_get_value(vib_data->haptic_en_gpio);
	DRV2603_DBG_INFO("Haptic_EN_GPIO Value : %d\n", gpio);

	return 0;
}

static int vibrator_power_set(int enable, struct timed_vibrator_data *vib_data)
{
	int rc;

	DRV2603_DBG("pwr_enable=%d\n", enable);

	mutex_lock(&vib_lock);
	if (enable) {
		rc = regulator_enable(vib_data->vreg_l12);
		if (rc < 0)
			DRV2603_ERR("%s: regulator_enable failed\n", __func__);
	} else {
		if (regulator_is_enabled(vib_data->vreg_l12) > 0) {
			rc = regulator_disable(vib_data->vreg_l12);
			if (rc < 0)
				DRV2603_ERR("%s: regulator_disable failed\n", __func__);
		}
	}
	mutex_unlock(&vib_lock);
	return 0;
}




//#if IS_ENABLED(CONFIG_PWM_ODIN)
static void vibrator_pwm_set(struct timed_vibrator_data  *vib_data, int duty)
{
	struct pwm_device *pd = NULL;
    unsigned int duty_ns;
	unsigned int period_ns;
	int ret=0;

    pd=vib_data->pd;
	DRV2603_DBG("cal_value : %d\n",duty);
	if(duty == 0)
		duty_ns = 0;
	else
		duty_ns = (duty+1) * 4;


	duty_ns = duty_ns * PCLK_NS;
	period_ns = VIB_PERIOD * PCLK_NS;
	pwm_config(pd,duty_ns, period_ns);
    if (duty)
	    ret=pwm_enable(pd);
	else
		pwm_disable(pd);

}


static inline void vibrator_work_on(struct work_struct *work)
{
    schedule_work(work);
}

static inline void vibrator_work_off(struct work_struct *work)
{
	if (!work_pending(work))
		schedule_work(work);
}


static int drv2603_vibrator_force_set(struct timed_vibrator_data *vib, int enable, int pwm)
{

	/* Check the Force value with Max and Min force value */
	int vib_duration_ms = 0;

	if (vib->vibe_warmup_delay > 0) {
		if (atomic_read(&vib->vib_status))
			msleep(vib->vibe_warmup_delay);
	}

	/* TODO: control the gain of vibrator */
	if(enable == 0){
		vibrator_ic_enable_set(enable, vib);
		vibrator_pwm_set(vib, 0);

        if (vib->haptic_en_gpio!= 1)
			vibrator_power_set(0, vib);
		/* should be checked for vibrator response time */
        atomic_set(&vib->vib_status, false);
	}
    else{
		if (work_pending(&vib->work_vibrator_off))
			cancel_work_sync(&vib->work_vibrator_off);
	    hrtimer_cancel(&vib->timer);
        vib_duration_ms = atomic_read(&vib->ms_time);

		/* should be checked for vibrator response time */
		if (vib->haptic_en_gpio!= 1)
			vibrator_power_set(1, vib);

		vibrator_pwm_set(vib, pwm);
		vibrator_ic_enable_set(1, vib);
		atomic_set(&vib->vib_status, true);
	 	hrtimer_start(&vib->timer,
				ns_to_ktime((u64)vib_duration_ms * NSEC_PER_MSEC),
				HRTIMER_MODE_REL);
	}
	return 0;
}

static void drv2603_vibrator_on(struct work_struct *work)
{
	struct timed_vibrator_data *vib =
		container_of(work, struct timed_vibrator_data,
				work_vibrator_on);

	int pwm = atomic_read(&vib->vibe_pwm);
	/* suspend /resume logging test */
	DRV2603_DBG("%s: pwm = %d\n", __func__, pwm);
	drv2603_vibrator_force_set(vib,1,pwm);
}

static void drv2603_vibrator_off(struct work_struct *work)
{
	struct timed_vibrator_data *vib =
		container_of(work, struct timed_vibrator_data,
				work_vibrator_off);
	drv2603_vibrator_force_set(vib,0,0);
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct timed_vibrator_data *vib =
		container_of(timer, struct timed_vibrator_data, timer);
	vibrator_work_off(&vib->work_vibrator_off);
	return HRTIMER_NORESTART;
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	struct timed_vibrator_data *vib =
		container_of(dev, struct timed_vibrator_data, dev);
	if (hrtimer_active(&vib->timer)) {
		ktime_t r = hrtimer_get_remaining(&vib->timer);
		return ktime_to_ms(r);
	}
		return 0;
}


static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	struct timed_vibrator_data *vib =
		container_of(dev, struct timed_vibrator_data, dev);
	unsigned long flags;

	spin_lock_irqsave(&vib->lock, flags);

	if (value > 0) {
		if (value > vib->max_timeout_ms)
			value = vib->max_timeout_ms;

		atomic_set(&vib->ms_time, value);
		vibrator_work_on(&vib->work_vibrator_on);
	}
    else{
		vibrator_work_off(&vib->work_vibrator_off);
	}
	spin_unlock_irqrestore(&vib->lock, flags);
}


static int vibrator_gpio_init(struct timed_vibrator_data *vib_data)
{
	int rc;

	DRV2603_DBG_INFO("***vibrator_init\n");
	/* GPIO setting for Motor EN in pmic8921 */
	rc = gpio_request(vib_data->haptic_en_gpio, "lin_motor_en");
	if(rc){
		DRV2603_DBG("GPIO_LIN_MOTOR_EN %d request failed\n", vib_data->haptic_en_gpio);
		return 0;
	}

	/* gpio init *//*
	rc = gpio_request(vib_data->motor_pwm_gpio, "lin_motor_pwm");
	if (unlikely(rc < 0)) {
		printk("not able to get gpio %d\n", vib_data->motor_pwm_gpio);
		return 0;
	}*/

	return 0;
}

#ifdef CONFIG_OF
static void vibrator_parse_dt(struct device *dev,struct timed_vibrator_data *vib_data)
{
	int ret=0;
    struct device_node *np = dev->of_node;

	ret = of_property_read_u32(np, "haptic-en-gpio", &vib_data->haptic_en_gpio);
	ret = of_property_read_u32(np, "motor-pwm-gpio",& vib_data->motor_pwm_gpio);
	ret = of_property_read_u32(np, "initial-vibrate-ms", &vib_data->initial_vibrate_ms);
	ret = of_property_read_u32(np, "max-timeout-ms", &vib_data->max_timeout_ms);
	ret = of_property_read_u32(np, "pwm-value", &vib_data->pwm_value);
	ret = of_property_read_string(np, "regulator-name", &vib_data->regulator_name);
}
#endif

static ssize_t vibrator_pwm_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib = container_of(dev_, struct timed_vibrator_data, dev);

	int gain = atomic_read(&(vib->vibe_pwm));

	return sprintf(buf, "%d\n", gain);
}

static ssize_t vibrator_pwm_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib = container_of(dev_, struct timed_vibrator_data, dev);

	int gain;
	sscanf(buf, "%d", &gain);
	atomic_set(&vib->vibe_pwm, gain);
	vibrator_pwm_set(vib,gain);

	return size;
}


static ssize_t vibrator_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib = container_of(dev_, struct timed_vibrator_data, dev);

	int gain = atomic_read(&(vib->vibe_pwm));

	return sprintf(buf, "%d\n", gain);
}

static ssize_t vibrator_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib = container_of(dev_, struct timed_vibrator_data, dev);

	int s_pwm;
	int s_enable;

	char buff[256] = {0,};
	char *b = NULL;
	char *conf_str =NULL;


	strlcpy(buff, buf, sizeof(buff));
	b = strim(buff);

	conf_str = strsep(&b, " ");
	s_enable = simple_strtoul(conf_str, NULL, 0);
	conf_str = strsep(&b, " ");
	if(conf_str != NULL)
	s_pwm = simple_strtoul(conf_str, NULL, 0);
	atomic_set(&vib->vibe_pwm,s_pwm);
	drv2603_vibrator_force_set(vib,s_enable,s_pwm);

	return size;
}

static ssize_t vibrator_timeoutms_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib = container_of(dev_, struct timed_vibrator_data, dev);
	int gain = atomic_read(&(vib->ms_time));

	return sprintf(buf, "%d\n", gain);
}

static ssize_t vibrator_timeoutms_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib = container_of(dev_, struct timed_vibrator_data, dev);
	int gain;
	sscanf(buf, "%d", &gain);
	atomic_set(&vib->ms_time, gain);

	return size;
}


static struct device_attribute drv2603_device_attrs[] = {
	__ATTR(vib_enable, S_IRUGO | S_IWUSR, vibrator_enable_show, vibrator_enable_store),
	__ATTR(vib_pwm, S_IRUGO | S_IWUSR, vibrator_pwm_show, vibrator_pwm_store),
	__ATTR(vib_time, S_IRUGO | S_IWUSR, vibrator_timeoutms_show, vibrator_timeoutms_store),
};

struct timed_vibrator_data drv2603_vibrator_data = {
	.dev.name = "vibrator",
	.dev.enable = vibrator_enable,
	.dev.get_time = vibrator_get_time,
};

static int drv2603_vibrator_probe(struct platform_device *pdev)
{
	int i,ret = 0;
	struct timed_vibrator_data *vib;

	DRV2603_DBG_INFO("drv2603_probe!\n");

	platform_set_drvdata(pdev, &drv2603_vibrator_data);
	vib = (struct timed_vibrator_data *)platform_get_drvdata(pdev);

#ifdef CONFIG_OF
	if (pdev->dev.of_node)
	{
		DRV2603_DBG_INFO("pdev->dev.of_node\n");
	      vibrator_parse_dt(&pdev->dev,vib);
		vib->pd= pwm_get(&pdev->dev, "pwm-vib");
		DRV2603_DBG_INFO("finished parsing!!\n");
	}
#endif



			vib->vreg_l12 = regulator_get(&pdev->dev, vib->regulator_name);
			if (IS_ERR(vib->vreg_l12)) {
				pr_err("%s: regulator get of Dialog failed (%ld)\n",
						__func__, PTR_ERR(vib->vreg_l12));
				vib->vreg_l12 = NULL;
			}



	pdev->dev.init_name = vib->dev.name;
	DRV2603_DBG("dev->init_name : %s, dev->kobj : %s\n",	pdev->dev.init_name, pdev->dev.kobj.name);

	if (vibrator_gpio_init(vib) < 0) {
		DRV2603_ERR("Android Vreg, GPIO set failed\n");
		return -ENODEV;
	}

	atomic_set(&vib->vib_status, false);
	atomic_set(&vib->vibe_pwm, vib->pwm_value);
	atomic_set(&vib->ms_time,vib->initial_vibrate_ms);
	ret = timed_output_dev_register(&vib->dev);
	if (ret < 0) {
		timed_output_dev_unregister(&vib->dev);
		return -ENODEV;
	}

	INIT_WORK(&vib->work_vibrator_on, drv2603_vibrator_on);
	INIT_WORK(&vib->work_vibrator_off, drv2603_vibrator_off);
	hrtimer_init(&vib->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vib->timer.function = vibrator_timer_func;
	spin_lock_init(&vib->lock);

	for (i = 0; i < ARRAY_SIZE(drv2603_device_attrs); i++) {
		ret = device_create_file(vib->dev.dev, &drv2603_device_attrs[i]);
		if (ret < 0) {
			timed_output_dev_unregister(&vib->dev);
			device_remove_file(vib->dev.dev, &drv2603_device_attrs[i]);
			return -ENODEV;
		}
	}

	vibrator_enable(&vib->dev,vib->initial_vibrate_ms);
	//DRV2603_DBG_INFO("vibrator probe finished!!!\n");

	return 0;
}

static int drv2603_vibrator_remove(struct platform_device * pdev)
{
	struct timed_vibrator_data *vib =
		(struct timed_vibrator_data *)platform_get_drvdata(pdev);
	int i = 0;
	DRV2603_DBG_INFO("Android Vibrator Driver Shutdown\n");

    drv2603_vibrator_force_set(vib,0,vib->pwm_value);
	for (i = 0; i < ARRAY_SIZE(drv2603_device_attrs); i++) {
			timed_output_dev_unregister(&vib->dev);
			device_remove_file(vib->dev.dev, &drv2603_device_attrs[i]);
		}

	//gpio_free(vib->motor_pwm_gpio);
	gpio_free(vib->haptic_en_gpio);
	timed_output_dev_unregister(&vib->dev);

	return 0;
}

static int drv2603_vibrator_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct timed_vibrator_data *vib;

	DRV2603_DBG_INFO("Android Vibrator Driver Suspend\n");
    vib = (struct timed_vibrator_data *)platform_get_drvdata(pdev);
	drv2603_vibrator_force_set(vib,0,vib->pwm_value);

	return 0;
}

static int drv2603_vibrator_resume(struct platform_device *pdev)
{
	struct timed_vibrator_data *vib;

    DRV2603_DBG_INFO("Android Vibrator Driver Resume\n");
	vib = (struct timed_vibrator_data *)platform_get_drvdata(pdev);
    drv2603_vibrator_force_set(vib,0,vib->pwm_value);

	return 0;
}

static void drv2603_vibrator_shutdown(struct platform_device *pdev)
{
	struct timed_vibrator_data *vib;

	DRV2603_DBG_INFO("Android Vibrator Driver Shutdown\n");
	vib = (struct timed_vibrator_data *)platform_get_drvdata(pdev);
	drv2603_vibrator_force_set(vib,0,vib->pwm_value);

}

#ifdef CONFIG_OF
static struct of_device_id drv2603_match_table[] = {
    { .compatible = "ti,drv2603-vibrator" },
    { },
};
#endif

static struct platform_driver drv2603_vibrator_driver = {
	.probe = drv2603_vibrator_probe,
	.remove = drv2603_vibrator_remove,
	.shutdown = drv2603_vibrator_shutdown,
	.suspend = drv2603_vibrator_suspend,
	.resume = drv2603_vibrator_resume,
	.driver = {
		.name = DEVICE_NAME,
#ifdef CONFIG_OF
		.of_match_table = drv2603_match_table,
#endif
	},
};

static int __init drv2603_vibrator_init(void)
{
	DRV2603_DBG_INFO("Android Vibrator Driver Init\n");

	return platform_driver_register(&drv2603_vibrator_driver);
}

static void __exit drv2603_vibrator_exit(void)
{
	DRV2603_DBG_INFO("Android Vibrator Driver Exit\n");

	platform_driver_unregister(&drv2603_vibrator_driver);
}

/* to let init lately */
late_initcall_sync(drv2603_vibrator_init);
module_exit(drv2603_vibrator_exit);
/*
MODULE_AUTHOR("LG Electronics Inc.");
MODULE_DESCRIPTION("Android Common Vibrator Driver");
MODULE_LICENSE("GPL");
*/
