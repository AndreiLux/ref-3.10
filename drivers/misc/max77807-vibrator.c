/*
 * Maxim MAX77807 Vibrator Driver
 *
 * Copyright (C) 2014 Maxim Integrated Product
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/regmap.h>
#include <linux/mfd/max77807/max77807.h>
#include <linux/mfd/max77807/max77807-vibrator.h>
#include "../staging/android/timed_output.h"
#include <linux/board_lge.h>
#include <linux/platform_data/gpio-odin.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/regulator/machine.h>
#include <linux/io.h>
#include <linux/stat.h>
#include <linux/mfd/max77807/max77807-charger.h>

#if IS_ENABLED(CONFIG_PWM_ODIN)
#include <linux/pwm.h>
#include <linux/odin_pwm.h>
#define PCLK_NS  30			/*50MHz*/
#define VIB_PERIOD	 0x400
#endif

#define DRIVER_NAME    MAX77807_MOTOR_NAME

#define VIB_ON 1
#define VIB_OFF 0

#define M2SH  __CONST_FFS

/* HAPTIC REGISTERS */
#define REG_STAUTS			0x00
#define REG_CONFIG1			0x01
#define REG_CONFIG2			0x02
#define REG_CONFIG_CHNL		0x03
#define REG_CONFIG_CYC1		0x04
#define REG_CONFIG_CYC2		0x05
#define REG_CONFIG_PER1		0x06
#define REG_CONFIG_PER2		0x07
#define REG_CONFIG_PER3		0x08
#define REG_CONFIG_PER4		0x09
#define REG_CONFIG_DUTY1		0x0A
#define REG_CONFIG_DUTY2		0X0B
#define REG_CONFIG_PWM1		0x0C
#define REG_CONFIG_PWM2		0x0D
#define REG_CONFIG_PWM3		0x0E
#define REG_CONFIG_PWM4		0x0F
#define REG_CONFIG_REV		0x10
/*Haptic driver on*/
#define REG_CONFIG_LSCNFG				0x2B
#define REG_CONFIG_HAPTIC_ENABLE		0x01
#define REG_CONFIG_HAPTIC_DISABLE		0x00



/* HAPTIC REGISTER BITS */
#define STATUS_HBUSY			BIT (0)

#define CONFIG1_INV			BIT (7)
#define CONFIG1_CONT			BIT (6)
#define CONFIG1_MSU			BITS(5,3)
#define CONFIG1_SCF			BITS(2,0)

#define CONFIG2_MODE			BIT (7)
#define CONFIG2_MEN			BIT (6)
#define CONFIG2_HTYP			BIT (5)
#define CONFIG2_PDIV			BITS(1,0)

#define CONFIG_CHNL_CYCA		BITS(7,6)
#define CONFIG_CHNL_SIGPA	BITS(5,4)
#define CONFIG_CHNL_SIGDCA	BITS(3,2)
#define CONFIG_CHNL_PWMDCA	BITS(1,0)

#define CONFIG_CYC1_CYC0		BITS(7,4)
#define CONFIG_CYC1_CYC1		BITS(3,0)
#define CONFIG_CYC2_CYC2		BITS(7,4)
#define CONFIG_CYC2_CYC3		BITS(3,0)

#define CONFIG_PER1_SIGP0	BITS(7,0)
#define CONFIG_PER2_SIGP1	BITS(7,0)
#define CONFIG_PER3_SIGP2	BITS(7,0)
#define CONFIG_PER4_SIGP3	BITS(7,0)

#define CONFIG_DUTY1_SIGDC0	BITS(7,4)
#define CONFIG_DUTY1_SIGDC1	BITS(3,0)
#define CONFIG_DUTY2_SIGDC2	BITS(7,4)
#define CONFIG_DUTY2_SIGDC3	BITS(3,0)

#define CONFIG_PWM1_PWMDC0	BITS(5,0)
#define CONFIG_PWM2_PWMDC1	BITS(5,0)
#define CONFIG_PWM3_PWMDC2	BITS(5,0)
#define CONFIG_PWM4_PWMDC3	BITS(5,0)

#define REV_MTR_REV			BITS(7,0)

/*Haptic driver on*/
#define LSCNFG_LSEN			BIT (7)
#define HAPTIC_ENABLE		BIT (6)


static DEFINE_MUTEX(vib_lock);

enum {
	VIB_STAT_STOP = 0,
	VIB_STAT_BRAKING,
	VIB_STAT_START,
	VIB_STAT_DRIVING,
	VIB_STAT_WARMUP,
	VIB_STAT_RUNNING,
};

extern struct max77807_charger *global_charger;

#define ANDROID_VIBRATOR_USE_WORKQUEUE

#ifdef ANDROID_VIBRATOR_USE_WORKQUEUE
static struct workqueue_struct *vibrator_workqueue;
#endif

struct max77807_vib
{
	struct timed_output_dev timed_dev;
	struct regmap *regmap;
	struct hrtimer timer;
	spinlock_t lock;
	atomic_t ms_time; /* vibrator duration */
	atomic_t vib_status;			/* on/off */
	atomic_t vib_pwm;
	ktime_t last_time;
	int status;
	int warmup_delay; /* in ms */
	int braking_ms;
	int driving_ms;
	int min_timeout_ms;
	int initial_vibrate_ms;
	int max_timeout_ms;
	int pwm_value;
	char *regulator_name;
	struct regulator *vreg_l12;
	struct pwm_device *pd;

#ifdef ANDROID_VIBRATOR_USE_WORKQUEUE
	struct delayed_work work_vibrator_off;
	struct delayed_work work_vibrator_on;
#else
	struct work_struct work_vibrator_off;
	struct work_struct work_vibrator_on;
#endif

};

void max77807_vib_run_set(struct max77807_vib *max77807_vib, int on){

	MAX77807_DBG("start  on : %d\n", on);

	//ret = regmap_read (max77807_vib->regmap, REG_CONFIG2, &val);
	if(on) {
		regmap_update_bits(max77807_vib->regmap, REG_CONFIG2,
						HAPTIC_ENABLE,  REG_CONFIG_HAPTIC_ENABLE << M2SH(HAPTIC_ENABLE));
	}else {
	       regmap_update_bits(max77807_vib->regmap, REG_CONFIG2,
							HAPTIC_ENABLE,	REG_CONFIG_HAPTIC_DISABLE << M2SH(HAPTIC_ENABLE));
	}
	//ret = regmap_read (max77807_vib->regmap, REG_CONFIG2, &val);

}


void max77807_vib_haptic_set(int on)
{
	MAX77807_DBG("start  on : %d\n",on);

	if(!global_charger) return;

	if(on) {
		/* Haptic drvier on */
		regmap_update_bits(global_charger->io->regmap, REG_CONFIG_LSCNFG,
						LSCNFG_LSEN, 1 << M2SH(LSCNFG_LSEN));

	}else {
		regmap_update_bits(global_charger->io->regmap, REG_CONFIG_LSCNFG,
						LSCNFG_LSEN, 0 << M2SH(LSCNFG_LSEN));
	}
}

static int max77807_vib_power_set(struct max77807_vib *max77807_vib, int enable)
{
	int ret=0;

	MAX77807_DBG("start  enable : %d\n",enable);

	mutex_lock(&vib_lock);
	if (enable) {
		ret = regulator_enable(max77807_vib->vreg_l12);
		if (ret < 0)
			MAX77807_ERR("regulator_enable failed\n");
	} else {
		if (regulator_is_enabled(max77807_vib->vreg_l12) > 0) {
			ret = regulator_disable(max77807_vib->vreg_l12);
			if (ret < 0)
				MAX77807_ERR("regulator_disable failed\n");
		}
	}
	mutex_unlock(&vib_lock);
	return ret;
}


static void max77807_vib_pwm_set(struct max77807_vib  *max77807_vib, int duty)
{
	struct pwm_device *pd = NULL;
	unsigned int duty_ns=0;
	unsigned int period_ns=0;
	int ret=0;


	//printk("vibrator start  duty : %d\n", duty);

	pd=max77807_vib->pd;

	if(pd == NULL) 	{
		MAX77807_ERR("pwm device is null\n");
		return;
	}

	if(duty == 0)
		duty_ns = 0;
	else
		duty_ns = (duty+1) * 4; // 175 ~ 255
	duty_ns = duty_ns * PCLK_NS;
	period_ns = VIB_PERIOD * PCLK_NS;

	pwm_config(pd,duty_ns, period_ns); //set duty_cycle

	if (duty)
	    ret=pwm_enable(pd);
	else
	    pwm_disable(pd);

	if( ret ){
		MAX77807_ERR("pwm_enable fail\n");
		return;
	}

}


static int max77807_vib_braking(struct max77807_vib *max77807_vib)
{
	if (max77807_vib->status <= VIB_STAT_BRAKING || !max77807_vib->braking_ms)
		return 0; /* don't need a braking */

	max77807_vib->status = VIB_STAT_BRAKING;
	//printk("vibrator braking\n");
	hrtimer_start(&max77807_vib->timer,
		ns_to_ktime((u64)max77807_vib->braking_ms * NSEC_PER_MSEC),
		HRTIMER_MODE_REL);

	return 1; /* braking */
}

static int max77807_vib_get_next(struct max77807_vib *max77807_vib)
{
	int next;

	switch(max77807_vib->status) {
		case VIB_STAT_STOP:
		case VIB_STAT_BRAKING:
			if (max77807_vib->driving_ms)
				next = VIB_STAT_DRIVING;
			else
				next = VIB_STAT_START;
			break;
		case VIB_STAT_DRIVING:
			next = VIB_STAT_RUNNING;
			break;
		case VIB_STAT_START:
			next = VIB_STAT_RUNNING;
			break;
		default:
			next = max77807_vib->status;
			break;
	};
	return next;
}



static inline int max77807_vib_set(struct max77807_vib *max77807_vib, int on)
{
	int pwm = atomic_read(&max77807_vib->vib_pwm);
	int vib_duration_ms = 0;

	MAX77807_DBG("start  pwm: %d ,  on : %d\n", pwm, on);

	if(!on || !pwm ){
		//printk("vibrator set off\n");
		if (max77807_vib_braking(max77807_vib))
			return 0;

		max77807_vib_haptic_set(VIB_OFF);
		max77807_vib_run_set(max77807_vib, VIB_OFF);
		max77807_vib_pwm_set(max77807_vib, VIB_OFF);
		max77807_vib_power_set(max77807_vib, VIB_OFF);

		max77807_vib->status = VIB_STAT_STOP;

		//printk("vibrator set off end\n");

		atomic_set(&max77807_vib->vib_status, false);
	} else{
		//printk("vibrator set on\n");
		int status = max77807_vib_get_next(max77807_vib);

		atomic_set(&max77807_vib->vib_status, true);

		if (delayed_work_pending(&max77807_vib->work_vibrator_off))
			cancel_delayed_work_sync(&max77807_vib->work_vibrator_off);

		if (max77807_vib->status == VIB_STAT_DRIVING &&
				hrtimer_active(&max77807_vib->timer))
				return 0;

		hrtimer_cancel(&max77807_vib->timer);

		vib_duration_ms = atomic_read(&max77807_vib->ms_time);// + max77807_vib->warmup_delay;
		//printk("vibrator set status B %d\n", status);

		if (status == VIB_STAT_WARMUP ) {
			cancel_delayed_work_sync(&max77807_vib->work_vibrator_off);
			//printk("vibrator set warmup\n");
		}

		//printk("vibrator set duration %d\n", vib_duration_ms);
		max77807_vib_haptic_set(on);
		max77807_vib_pwm_set(max77807_vib, pwm);
		max77807_vib_run_set(max77807_vib, on);
		max77807_vib_power_set(max77807_vib, on);

		max77807_vib->status = status;
		//printk("vibrator set status A %d\n", status);

		hrtimer_start(&max77807_vib->timer,
				ns_to_ktime((u64)vib_duration_ms * NSEC_PER_MSEC),
				HRTIMER_MODE_REL);
	}

	return 0;
}

#ifdef ANDROID_VIBRATOR_USE_WORKQUEUE
static inline void vibrator_work_on(struct delayed_work *work, unsigned long delay)
{
	queue_delayed_work(vibrator_workqueue, work, delay);
}

static inline void vibrator_work_off(struct delayed_work *work, unsigned long delay)
{
	if (!delayed_work_pending(work))
		queue_delayed_work(vibrator_workqueue, work, delay);
}

#else

static inline void vibrator_work_on(struct work_struct *work)
{
	schedule_work(work);
}

static inline void vibrator_work_off(struct work_struct *work)
{
	if (!work_pending(work))
		schedule_work(work);
}
#endif

static void max77807_vibrator_on(struct work_struct *work)
{
	struct max77807_vib *max77807_vib = container_of(work, struct max77807_vib,
				work_vibrator_on);
	max77807_vib_set(max77807_vib, VIB_ON);
}

static void max77807_vibrator_off(struct work_struct *work)
{
	struct max77807_vib *max77807_vib = container_of(work, struct max77807_vib,
				work_vibrator_off);
	max77807_vib_set(max77807_vib, VIB_OFF);
}


static void max77807_vib_enable(struct timed_output_dev *dev, int value)
{
	struct max77807_vib *max77807_vib = container_of(dev, struct max77807_vib, timed_dev);
	MAX77807_DBG("start value: %d\n",value);

	unsigned long flags;
	ktime_t now;

	spin_lock_irqsave(&max77807_vib->lock, flags);
	now = ktime_get_boottime();

	if (value > 0) //vibrator enable
	{
		printk("vibrator on %d\n", value);
		int delay = 0;

		if (value > max77807_vib->max_timeout_ms)
			value = max77807_vib->max_timeout_ms;

		atomic_set(&max77807_vib->ms_time, value);

		max77807_vib->last_time = now;

		if(max77807_vib->status == VIB_STAT_WARMUP )
			delay = max77807_vib->warmup_delay;
		else if (max77807_vib->status == VIB_STAT_BRAKING )  //off -- on
			delay = max77807_vib->braking_ms;

		//printk("vibrator on delay %d\n", delay);

		vibrator_work_on(&max77807_vib->work_vibrator_on, msecs_to_jiffies(delay));
	}else{
		printk("vibrator off %d\n", value);

		int diff_ms;
		bool force_stop = true;
		int ms_time = atomic_read(&max77807_vib->ms_time);

		diff_ms = ktime_to_ms(ktime_sub(now, max77807_vib->last_time));
		diff_ms = diff_ms - ms_time+ 1; //on -- on 

		//if (max77807_vib->min_timeout_ms && ms_time < max77807_vib->min_timeout_ms)
			//force_stop = false;

		if (force_stop && diff_ms < 0){
			//printk("vibrator warmup %d\n", max77807_vib->warmup_delay);
			max77807_vib->status = VIB_STAT_WARMUP;
			vibrator_work_off(&max77807_vib->work_vibrator_off,
				msecs_to_jiffies(max77807_vib->warmup_delay)); //success = 20ms
		}
	}

	spin_unlock_irqrestore(&max77807_vib->lock, flags);
}

static int max77807_vib_get_time(struct timed_output_dev *dev)
{
	struct max77807_vib *max77807_vib = container_of(dev, struct max77807_vib, timed_dev);

	if (hrtimer_active(&max77807_vib->timer))
	{
		ktime_t r = hrtimer_get_remaining(&max77807_vib->timer);
		return ktime_to_ms(r);
	}
	else
		return 0;
}

static enum hrtimer_restart max77807_vib_timer_func(struct hrtimer *timer)
{
	struct max77807_vib *max77807_vib = container_of(timer,
			struct max77807_vib, timer);
	MAX77807_DBG("start\n");
	vibrator_work_off(&max77807_vib->work_vibrator_off,0);
	return HRTIMER_NORESTART;
}

static int max77807_vib_hw_init(struct max77807_vib *max77807_vib, struct max77807_vib_platform_data *pdata)
{
	unsigned int value;

	value = pdata->mode << M2SH(CONFIG2_MODE) |
			pdata->htype << M2SH(CONFIG2_HTYP) |
			pdata->pdiv << M2SH(CONFIG2_PDIV);

	return regmap_write(max77807_vib->regmap, REG_CONFIG2, value);
}


static ssize_t max77807_vib_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct max77807_vib *max77807_vib = container_of(dev_, struct max77807_vib, timed_dev);

	int gain = atomic_read(&(max77807_vib->vib_pwm));

	return sprintf(buf, "%d\n", gain);
}

static ssize_t max77807_vib_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct max77807_vib *max77807_vib = container_of(dev_, struct max77807_vib, timed_dev);

	int s_pwm;
	int s_on;

	char buff[256] = {0,};
	char *b = NULL;
	char *conf_str =NULL;

	strlcpy(buff, buf, sizeof(buff));
	b = strim(buff);

	conf_str = strsep(&b, " ");
	s_on = simple_strtoul(conf_str, NULL, 0);
	conf_str = strsep(&b, " ");

	if(conf_str != NULL)
		s_pwm = simple_strtoul(conf_str, NULL, 0);

	MAX77807_DBG("%d, %d\n", s_on, s_pwm);

	atomic_set(&max77807_vib->vib_pwm, s_pwm);

	max77807_vib_set(max77807_vib, s_on);

	return size;
}

static ssize_t max77807_vib_pwm_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct max77807_vib *max77807_vib = container_of(dev_, struct max77807_vib, timed_dev);

	int gain = atomic_read(&(max77807_vib->vib_pwm));

	MAX77807_DBG("time: %d\n", gain);

	return sprintf(buf, "%d\n", gain);
}

static ssize_t max77807_vib_pwm_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct max77807_vib *max77807_vib = container_of(dev_, struct max77807_vib, timed_dev);

	int gain;
	sscanf(buf, "%d", &gain);
	atomic_set(&max77807_vib->vib_pwm, gain);

	max77807_vib_pwm_set(max77807_vib, gain);

	MAX77807_DBG("time: %d\n", gain);

	return size;
}


static ssize_t max77807_vib_timeoutms_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct max77807_vib *max77807_vib = container_of(dev_, struct max77807_vib, timed_dev);
	int gain = atomic_read(&(max77807_vib->ms_time));
	MAX77807_DBG("time: %d\n", gain);

	return sprintf(buf, "%d\n", gain);
}


static ssize_t max77807_vib_timeoutms_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ = (struct timed_output_dev *)dev_get_drvdata(dev);
	struct max77807_vib *max77807_vib = container_of(dev_, struct max77807_vib, timed_dev);
	int gain;
	sscanf(buf, "%d", &gain);
	atomic_set(&max77807_vib->ms_time, gain);
	MAX77807_DBG("time: %d\n", gain);
	MAX77807_DBG("ms time: %d\n", max77807_vib->ms_time);

	return size;
}



#ifdef CONFIG_OF
static struct max77807_vib_platform_data *max77807_vib_parse_dt(struct device *dev, struct max77807_vib * vib_data)
{
	struct device_node *nproot = dev->parent->of_node;
	struct device_node *np;
	struct max77807_vib_platform_data *pdata;
	int ret = 0;

	MAX77807_DBG("start\n");
	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);

	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

	np = of_find_node_by_name(nproot, "vibrator");

	if (unlikely(np == NULL))
	{
		dev_err(dev, "vibrator node not found\n");
		return ERR_PTR(-EINVAL);
	}

	ret = of_property_read_u32(np, "mode",&pdata->mode);
	ret = of_property_read_u32(np, "htype",&pdata->htype);
	ret = of_property_read_u32(np, "pdiv", &pdata->pdiv);
	ret = of_property_read_u32(np, "initial-vibrate-ms", &vib_data->initial_vibrate_ms);
	ret = of_property_read_u32(np, "max-timeout-ms", &vib_data->max_timeout_ms);
	ret = of_property_read_u32(np, "pwm-value", &vib_data->pwm_value);
	ret = of_property_read_string(np, "regulator-name", &vib_data->regulator_name);
	ret = of_property_read_u32(np, "warmup-delay", &vib_data->warmup_delay);
	ret = of_property_read_u32(np, "braking-ms", &vib_data->braking_ms);
	ret = of_property_read_u32(np, "driving-ms", &vib_data->driving_ms);

	return pdata;
}
#endif


static struct device_attribute max77807_device_attrs[] = {
	__ATTR(vib_enable, S_IRUGO | S_IWUSR, max77807_vib_enable_show, max77807_vib_enable_store),
	__ATTR(vib_pwm, S_IRUGO | S_IWUSR, max77807_vib_pwm_show,max77807_vib_pwm_store),
	__ATTR(vib_time, S_IRUGO | S_IWUSR, max77807_vib_timeoutms_show, max77807_vib_timeoutms_store),
};


struct max77807_vib max77807_vibrator_data = {
	.timed_dev.name = "vibrator",
	.timed_dev.enable = max77807_vib_enable,
	.timed_dev.get_time = max77807_vib_get_time,
};

static int max77807_vib_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct max77807_dev *chip = dev_get_drvdata(dev->parent);
        struct max77807_io *io = max77807_get_io(chip);
	struct max77807_vib_platform_data *pdata;
	struct max77807_vib *max77807_vib;
	int ret;
	int i=0;

	MAX77807_DBG("start\n");

	platform_set_drvdata(pdev, &max77807_vibrator_data);
	max77807_vib = (struct max77807_vib *)platform_get_drvdata(pdev);

#ifdef CONFIG_OF
	pdata = max77807_vib_parse_dt(dev, max77807_vib);
	max77807_vib->pd= pwm_get(dev, "pwm-vib");

	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
#else
	pdata = dev_get_platdata(dev);
#endif
	max77807_vib->vreg_l12 = regulator_get(&pdev->dev, max77807_vib->regulator_name);

	if (IS_ERR(max77807_vib->vreg_l12)) {
		MAX77807_ERR("%s: regulator get of Dialog failed (%ld)\n",
				__func__, PTR_ERR(max77807_vib->vreg_l12));
		max77807_vib->vreg_l12 = NULL;
	}

	pdev->dev.init_name = max77807_vib->timed_dev.name;

	atomic_set(&max77807_vib->vib_status, false);
	atomic_set(&max77807_vib->vib_pwm, max77807_vib->pwm_value);
	atomic_set(&max77807_vib->ms_time,max77807_vib->initial_vibrate_ms);
	max77807_vib->regmap = io->regmap;

	ret = max77807_vib_hw_init(max77807_vib, pdata);

	hrtimer_init(&max77807_vib->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	max77807_vib->timer.function = max77807_vib_timer_func;
	spin_lock_init(&max77807_vib->lock);

	ret = timed_output_dev_register(&max77807_vib->timed_dev);

	if (IS_ERR_VALUE(ret)){
		timed_output_dev_unregister(&max77807_vib->timed_dev);
		return -ENODEV;
	}

#ifdef ANDROID_VIBRATOR_USE_WORKQUEUE
	INIT_DELAYED_WORK(&max77807_vib->work_vibrator_on, max77807_vibrator_on);
	INIT_DELAYED_WORK(&max77807_vib->work_vibrator_off, max77807_vibrator_off);
#else
	INIT_WORK(&max77807_vib->work_vibrator_on, max77807_vibrator_on);
	INIT_WORK(&max77807_vib->work_vibrator_off, max77807_vibrator_off);
#endif

	for (i = 0; i < ARRAY_SIZE(max77807_device_attrs); i++) {
		ret = device_create_file(max77807_vib->timed_dev.dev, &max77807_device_attrs[i]);
		if (ret < 0) {
			timed_output_dev_unregister(&max77807_vib->timed_dev);
			device_remove_file(max77807_vib->timed_dev.dev, &max77807_device_attrs[i]);
			return -ENODEV;
		}
	}

	max77807_vib_enable(&max77807_vib->timed_dev, max77807_vib->initial_vibrate_ms);

	MAX77807_DBG("finished\n");

	return 0;
}

static int max77807_vib_remove(struct platform_device *pdev)
{
	struct max77807_vib *max77807_vib = platform_get_drvdata(pdev);

	MAX77807_DBG_INFO("Android Vibrator Driver Remove\n");

	max77807_vib_set(max77807_vib, VIB_OFF);

	timed_output_dev_unregister(&max77807_vib->timed_dev);

	return 0;
}



#ifdef CONFIG_PM
static int max77807_vib_suspend(struct platform_device *dev, pm_message_t state)
{
	struct max77807_vib *max77807_vib = platform_get_drvdata(dev);
	MAX77807_DBG_INFO("Android Vibrator Driver Suspend\n");

	if(atomic_read(&max77807_vib->vib_status) == true)
	{
		hrtimer_cancel(&max77807_vib->timer);
		return max77807_vib_set(max77807_vib, VIB_OFF);
	}
	return 0;
}


static int max77807_vib_resume(struct platform_device *pdev)
{
	struct max77807_vib *max77807_vib;

	MAX77807_DBG_INFO("Android Vibrator Driver Resume\n");
	max77807_vib = (struct max77807_vib *)platform_get_drvdata(pdev);

	if(atomic_read(&max77807_vib->vib_status) == true)
	{
		hrtimer_cancel(&max77807_vib->timer);
		return max77807_vib_set(max77807_vib, VIB_OFF);
	}

	return 0;
}
#endif

static void max77807_vib_shutdown(struct platform_device *pdev)
{
	struct max77807_vib *max77807_vib;

	MAX77807_DBG_INFO("Android Vibrator Driver Shutdown\n");
	max77807_vib = (struct max77807_vib *)platform_get_drvdata(pdev);

	max77807_vib_set(max77807_vib, VIB_OFF);
}


#ifdef CONFIG_OF
static struct of_device_id max77807_vibrator_match_table[] = {
    { .compatible = "maxim,"MAX77807_MOTOR_NAME },
    { },
};
#endif

static struct platform_driver max77807_vib_driver = {
	.driver	= {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = max77807_vibrator_match_table,
#endif
	},
	.probe = max77807_vib_probe,
	.remove = max77807_vib_remove,
	.shutdown = max77807_vib_shutdown,
#ifdef CONFIG_PM
	.resume = max77807_vib_resume,
	.suspend = max77807_vib_suspend,
#endif
};

static int __init max77807_vib_init(void)
{
#ifdef ANDROID_VIBRATOR_USE_WORKQUEUE
		vibrator_workqueue = create_workqueue("vibrator");
		if (!vibrator_workqueue) {
			pr_err("%s: out of memory\n", __func__);
			return -ENOMEM;
		}
#endif

	MAX77807_DBG_INFO("Android Vibrator Driver Init\n");
	return platform_driver_register(&max77807_vib_driver);
}

late_initcall_sync(max77807_vib_init);

static void __exit max77807_vib_exit(void)
{
#ifdef ANDROID_VIBRATOR_USE_WORKQUEUE
		if (vibrator_workqueue)
			destroy_workqueue(vibrator_workqueue);
		vibrator_workqueue = NULL;
#endif

	MAX77807_DBG_INFO("Android Vibrator Driver exit\n");
	platform_driver_unregister(&max77807_vib_driver);
}
module_exit(max77807_vib_exit);

MODULE_ALIAS("platform:max77807-vibrator");
MODULE_AUTHOR("TaiEup Kim<clark.kim@maximintegrated.com>");
MODULE_DESCRIPTION("MAX77807 vibrator driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
