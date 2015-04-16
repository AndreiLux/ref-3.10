/*
 *
 * Copyright (c) 2011-2013 Hisilicon Technologies CO., Ltd.
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
 *
 * Discription:using gpio_203 realizing volume-up-key and gpio_204
 * realizing volumn-down-key instead of KPC in kernel, only support simple
 * key-press at currunt version, not support combo-keys.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <asm/irq.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/wakelock.h>

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif

#define TRUE				(1)
#define FALSE				(0)
#define GPIO_KEY_RELEASE    (0)
#define GPIO_KEY_PRESS      (1)

#define GPIO_HIGH_VOLTAGE   (1)
#define GPIO_LOW_VOLTAGE    (0)
#define GPIO_GET_VALUE_MAXTIME (3)
#define TIMER_DEBOUNCE (15)

static struct wake_lock volume_up_key_lock;
static struct wake_lock volume_down_key_lock;

struct k3v3_gpio_key {
	struct input_dev *input_dev;
	struct delayed_work gpio_keyup_work;
	struct delayed_work gpio_keydown_work;
	struct timer_list key_up_timer;
	struct timer_list key_down_timer;
	int    			gpio_up;
	int    			gpio_down;
	int    			volume_up_irq;      /*volumn up key irq.*/
	int    		 	volume_down_irq;    /*volumn down key irq.*/
	struct pinctrl *pctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_idle;
};

static int k3v3_gpio_key_open(struct input_dev *dev)
{
	return 0;
}

static void k3v3_gpio_key_close(struct input_dev *dev)
{
	return;
}

static void k3v3_gpio_keyup_work(struct work_struct *work)
{
	struct k3v3_gpio_key *gpio_key = container_of(work,
		struct k3v3_gpio_key, gpio_keyup_work.work);

	unsigned int keyup_value = 0;
	unsigned int report_action = GPIO_KEY_RELEASE;

	keyup_value = gpio_get_value(gpio_key->gpio_up);
	/*judge key is pressed or released.*/
	if (keyup_value == GPIO_LOW_VOLTAGE) {
		report_action = GPIO_KEY_PRESS;
	} else if (keyup_value == GPIO_HIGH_VOLTAGE) {
		report_action = GPIO_KEY_RELEASE;
	} else {
		printk(KERN_ERR "[gpiokey][%s]invalid gpio key_value.\n", __FUNCTION__);
		return;
	}

	printk(KERN_DEBUG"[gpiokey][%s] volumn key %u action %u\n", __FUNCTION__
		, KEY_VOLUMEUP, report_action);
	input_report_key(gpio_key->input_dev, KEY_VOLUMEUP, report_action);
	input_sync(gpio_key->input_dev);

	if (keyup_value == GPIO_HIGH_VOLTAGE)
		wake_unlock(&volume_up_key_lock);

	return;
}

static void k3v3_gpio_keydown_work(struct work_struct *work)
{
	struct k3v3_gpio_key *gpio_key = container_of(work,
		struct k3v3_gpio_key, gpio_keydown_work.work);

	unsigned int keydown_value = 0;
	unsigned int report_action = GPIO_KEY_RELEASE;

	keydown_value = gpio_get_value(gpio_key->gpio_down);
	/*judge key is pressed or released.*/
	if (keydown_value == GPIO_LOW_VOLTAGE) {
		report_action = GPIO_KEY_PRESS;
	} else if (keydown_value == GPIO_HIGH_VOLTAGE) {
		report_action = GPIO_KEY_RELEASE;
	} else {
		printk(KERN_ERR "[gpiokey][%s]invalid gpio key_value.\n", __FUNCTION__);
		return;
	}

	printk(KERN_DEBUG"[gpiokey][%s]volumn key %u action %u\n", __FUNCTION__
		, KEY_VOLUMEDOWN, report_action);
	input_report_key(gpio_key->input_dev, KEY_VOLUMEDOWN, report_action);
	input_sync(gpio_key->input_dev);

	if (keydown_value == GPIO_HIGH_VOLTAGE)
		wake_unlock(&volume_down_key_lock);
	return;
}

static void gpio_keyup_timer(unsigned long data)
{
	int keydown_value;
	struct k3v3_gpio_key *gpio_key = (struct k3v3_gpio_key *)data;

	keydown_value = gpio_get_value(gpio_key->gpio_up);
        /*judge key is pressed or released.*/
        if (keydown_value == GPIO_LOW_VOLTAGE)
                wake_lock(&volume_up_key_lock);
        schedule_delayed_work(&(gpio_key->gpio_keyup_work), 0);
	return;
}

static void gpio_keydown_timer(unsigned long data)
{
	int keydown_value;
	struct k3v3_gpio_key *gpio_key = (struct k3v3_gpio_key *)data;

	keydown_value = gpio_get_value(gpio_key->gpio_down);
        /*judge key is pressed or released.*/
        if (keydown_value == GPIO_LOW_VOLTAGE)
                wake_lock(&volume_down_key_lock);
	schedule_delayed_work(&(gpio_key->gpio_keydown_work), 0);
	return;
}

static irqreturn_t k3v3_gpio_key_irq_handler(int irq, void *dev_id)
{
	struct k3v3_gpio_key *gpio_key = (struct k3v3_gpio_key *)dev_id;
	/* handle gpio key volume up & gpio key volume down event at here */

	if (irq == gpio_key->volume_up_irq) {
		mod_timer(&(gpio_key->key_up_timer), jiffies + msecs_to_jiffies(TIMER_DEBOUNCE));
		wake_lock_timeout(&volume_up_key_lock, 50);
	} else if (irq == gpio_key->volume_down_irq) {
		wake_lock_timeout(&volume_down_key_lock, 50);
		mod_timer(&(gpio_key->key_down_timer), jiffies + msecs_to_jiffies(TIMER_DEBOUNCE));
	} else {
		printk(KERN_ERR "[gpiokey] [%s]invalid irq %d!\n", __FUNCTION__, irq);
	}
	return IRQ_HANDLED;
}

#ifdef CONFIG_OF
static const struct of_device_id hs_gpio_key_match[] = {
	{ .compatible = "hisilicon,gpio-key" },
	{},
};
MODULE_DEVICE_TABLE(of, hs_gpio_key_match);
#endif

static int k3v3_gpio_key_probe(struct platform_device* pdev)
{
	struct k3v3_gpio_key *gpio_key = NULL;
	struct input_dev *input_dev = NULL;
	int err =0;

	if (NULL == pdev) {
		printk(KERN_ERR "[gpiokey]parameter error!\n");
		return -EINVAL;
	}

	dev_info(&pdev->dev, "k3v3 gpio key driver probes start!\n");
#ifdef CONFIG_OF
	if (!of_match_node(hs_gpio_key_match, pdev->dev.of_node)) {
		dev_err(&pdev->dev, "dev node is not match. exiting.\n");
		return -ENODEV;
	}
#endif

	gpio_key = devm_kzalloc(&pdev->dev, sizeof(struct k3v3_gpio_key), GFP_KERNEL);
	if (!gpio_key) {
		dev_err(&pdev->dev, "Failed to allocate struct k3v3_gpio_key!\n");
		return -ENOMEM;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&pdev->dev, "Failed to allocate struct input_dev!\n");
		return -ENOMEM;
	}

	input_dev->name = pdev->name;
	input_dev->id.bustype = BUS_HOST;
	input_dev->dev.parent = &pdev->dev;
	input_set_drvdata(input_dev, gpio_key);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_SYN, input_dev->evbit);
	set_bit(KEY_VOLUMEUP, input_dev->keybit);
	set_bit(KEY_VOLUMEDOWN, input_dev->keybit);
	input_dev->open = k3v3_gpio_key_open;
	input_dev->close = k3v3_gpio_key_close;

	gpio_key->input_dev = input_dev;

	/*initial work before we use it.*/
	INIT_DELAYED_WORK(&(gpio_key->gpio_keyup_work), k3v3_gpio_keyup_work);
	INIT_DELAYED_WORK(&(gpio_key->gpio_keydown_work), k3v3_gpio_keydown_work);
	wake_lock_init(&volume_down_key_lock,WAKE_LOCK_SUSPEND,"key_down_wake_lock");
        wake_lock_init(&volume_up_key_lock,WAKE_LOCK_SUSPEND,"key_up_wake_lock");

	gpio_key->gpio_up = of_get_named_gpio(pdev->dev.of_node, "gpio-keyup,gpio-irq", 0);
	if (!gpio_is_valid(gpio_key->gpio_up)) {
		dev_err(&pdev->dev, "gpio keyup is not valid!\n");
		err = -EINVAL;
		goto err_get_gpio;
	}
	gpio_key->gpio_down = of_get_named_gpio(pdev->dev.of_node, "gpio-keydown,gpio-irq", 0);
	if (!gpio_is_valid(gpio_key->gpio_down)) {
		dev_err(&pdev->dev, "gpio keydown is not valid!\n");
		err = -EINVAL;
		goto err_get_gpio;
	}
	err = gpio_request(gpio_key->gpio_up, "gpio_up");
	if (err < 0) {
		dev_err(&pdev->dev, "Fail request gpio:%d\n", gpio_key->gpio_up);
		goto err_get_gpio;
	}
	err = gpio_request(gpio_key->gpio_down, "gpio_down");
	if (err) {
		dev_err(&pdev->dev, "Fail request gpio:%d\n", gpio_key->gpio_down);
		goto err_gpio_down_req;
	}

	gpio_key->pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(gpio_key->pctrl)) {
		dev_err(&pdev->dev, "failed to devm pinctrl get\n");
		err = -EINVAL;
		goto err_pinctrl;
	}
	gpio_key->pins_default = pinctrl_lookup_state(gpio_key->pctrl, PINCTRL_STATE_DEFAULT);
	if (IS_ERR(gpio_key->pins_default)) {
		dev_err(&pdev->dev, "failed to pinctrl lookup state default\n");
		err = -EINVAL;
		goto err_pinctrl_put;
	}
	gpio_key->pins_idle = pinctrl_lookup_state(gpio_key->pctrl, PINCTRL_STATE_IDLE);
	if (IS_ERR(gpio_key->pins_idle)) {
		dev_err(&pdev->dev, "failed to pinctrl lookup state idle\n");
		err = -EINVAL;
		goto err_pinctrl_put;
	}
	err = pinctrl_select_state(gpio_key->pctrl, gpio_key->pins_default);
	if (err < 0) {
		dev_err(&pdev->dev, "set iomux normal error, %d\n", err);
		goto err_pinctrl_put;
	}

	gpio_direction_input(gpio_key->gpio_up);
	gpio_direction_input(gpio_key->gpio_down);

	gpio_key->volume_up_irq = gpio_to_irq(gpio_key->gpio_up);
	if (gpio_key->volume_up_irq < 0) {
		dev_err(&pdev->dev, "Failed to get gpio key press irq!\n");
		err = gpio_key->volume_up_irq;
		goto err_gpio_to_irq;
	}

	gpio_key->volume_down_irq = gpio_to_irq(gpio_key->gpio_down);
	if (gpio_key->volume_down_irq < 0) {
		dev_err(&pdev->dev, "Failed to get gpio key release irq!\n");
		err = gpio_key->volume_down_irq;
		goto err_gpio_to_irq;
	}

	setup_timer(&(gpio_key->key_up_timer), gpio_keyup_timer, (unsigned long )gpio_key);
        setup_timer(&(gpio_key->key_down_timer), gpio_keydown_timer, (unsigned long )gpio_key);
	/*
	 * support failing irq that means volume-up-key is pressed,
	 * and rising irq which means volume-up-key is released.
	 */
	err = request_irq(gpio_key->volume_up_irq, k3v3_gpio_key_irq_handler, IRQF_NO_SUSPEND | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, pdev->name, gpio_key);
	if (err) {
		dev_err(&pdev->dev, "Failed to request press interupt handler!\n");
		goto err_up_irq_req;
	}

	/*
	 * support failing irq that means volume-down-key is pressed,
	 * and rising irq which means volume-down-key is released.
	 */
	err = request_irq(gpio_key->volume_down_irq, k3v3_gpio_key_irq_handler, IRQF_NO_SUSPEND | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, pdev->name, gpio_key);
	if (err) {
		dev_err(&pdev->dev, "Failed to request release interupt handler!\n");
		goto err_down_irq_req;
	}

	err = input_register_device(gpio_key->input_dev);
	if (err) {
		dev_err(&pdev->dev, "Failed to register input device!\n");
		goto err_register_dev;
	}

	device_init_wakeup(&pdev->dev, TRUE);
	platform_set_drvdata(pdev, gpio_key);

	dev_info(&pdev->dev, "k3v3 gpio key driver probes successfully!\n");
	return 0;

err_register_dev:
	free_irq(gpio_key->volume_down_irq, gpio_key);
err_down_irq_req:
	free_irq(gpio_key->volume_up_irq, gpio_key);
err_up_irq_req:
err_gpio_to_irq:
err_pinctrl_put:
	devm_pinctrl_put(gpio_key->pctrl);
err_pinctrl:
	gpio_free(gpio_key->gpio_down);
err_gpio_down_req:
	gpio_free(gpio_key->gpio_up);
err_get_gpio:
	input_free_device(input_dev);
	wake_lock_destroy(&volume_down_key_lock);
        wake_lock_destroy(&volume_up_key_lock);
	pr_info(KERN_ERR "[gpiokey]K3v3 gpio key probe failed! ret = %d.\n", err);
	return err;
}

static int k3v3_gpio_key_remove(struct platform_device* pdev)
{
	struct k3v3_gpio_key *gpio_key = platform_get_drvdata(pdev);

	if (gpio_key == NULL) {
		printk(KERN_ERR "get invalid gpio_key pointer\n");
		return -EINVAL;
	}

	free_irq(gpio_key->volume_up_irq, gpio_key);
	free_irq(gpio_key->volume_down_irq, gpio_key);
	gpio_free(gpio_key->gpio_down);
	gpio_free(gpio_key->gpio_up);

	cancel_delayed_work(&(gpio_key->gpio_keyup_work));
	cancel_delayed_work(&(gpio_key->gpio_keydown_work));

        wake_lock_destroy(&volume_down_key_lock);
        wake_lock_destroy(&volume_up_key_lock);

	input_unregister_device(gpio_key->input_dev);
	platform_set_drvdata(pdev, NULL);
	kfree(gpio_key);
	gpio_key = NULL;
	return 0;
}

#ifdef CONFIG_PM
static int k3v3_gpio_key_suspend(struct platform_device *pdev, pm_message_t state)
{
	int err;
	struct k3v3_gpio_key *gpio_key = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "%s: suspend +\n", __func__);

	err = pinctrl_select_state(gpio_key->pctrl, gpio_key->pins_idle);
	if (err < 0) {
		dev_err(&pdev->dev, "set iomux normal error, %d\n", err);
	}

	dev_info(&pdev->dev, "%s: suspend -\n", __func__);
	return 0;
}

static int k3v3_gpio_key_resume(struct platform_device *pdev)
{
	int err;
	struct k3v3_gpio_key *gpio_key = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "%s: resume +\n", __func__);

	err = pinctrl_select_state(gpio_key->pctrl, gpio_key->pins_default);
	if (err < 0) {
		dev_err(&pdev->dev, "set iomux idle error, %d\n", err);
	}

	dev_info(&pdev->dev, "%s: resume -\n", __func__);
	return 0;
}
#endif

struct platform_driver k3v3_gpio_key_driver = {
	.probe = k3v3_gpio_key_probe,
	.remove = k3v3_gpio_key_remove,
	.driver = {
		.name = "k3v3_gpio_key",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(hs_gpio_key_match),
	},
#ifdef CONFIG_PM
	.suspend = k3v3_gpio_key_suspend,
	.resume = k3v3_gpio_key_resume,
#endif
};

module_platform_driver(k3v3_gpio_key_driver);

MODULE_AUTHOR("Hisilicon K3 Driver Group");
MODULE_DESCRIPTION("K3v3 keypad platform driver");
MODULE_LICENSE("GPL");
