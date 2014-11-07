/* drivers/pwm/pwm-odin.c
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License.
*
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/pwm.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/odin_pwm.h>

#include <linux/pm_runtime.h>

static int odin_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			  int duty_ns, int period_ns)
{
	struct odin_chip *odin = to_odin_chip(chip);
	unsigned long tcon;
	unsigned long tcnt;
	unsigned long reg_tcnt;
	unsigned long reg_tcmp;
	unsigned long clk_rate;
	unsigned long clk_ns = 0;
	long tcmp;
	int offset = 0;
	/* We currently avoid using 64bit arithmetic by using the
	 * fact that anything faster than 1Hz is easily representable
	 * by 32bits. */

	if (duty_ns > NS_IN_HZ || period_ns > NS_IN_HZ)
		return -ERANGE;

	if (period_ns == chip->pwms->period && duty_ns == odin->duty_ns[pwm->hwpwm] )
		return 0;

	if (pwm->hwpwm == 0){
		reg_tcmp = TCMPB0;
		reg_tcnt = TCNTB0;
	} else {
		reg_tcmp = TCMPB1;
		reg_tcnt = TCNTB1;
		offset = 5;
	}

	/* The TCMP and TCNT can be read without a lock, they're not
	 * shared between the timers. */
	tcmp = __raw_readl(odin->base + reg_tcmp);
	tcnt = __raw_readl(odin->base + reg_tcnt);

	clk_rate = clk_get_rate(odin->clk);
	clk_ns = NS_IN_HZ / clk_rate;
	/* Check to see if we are changing the clock rate of the PWM */
	if (tcnt != period_ns) {
		pwm->period = period_ns;
		tcnt = period_ns / clk_ns;
	}

	odin->duty_ns[pwm->hwpwm] = duty_ns;
	tcmp = duty_ns / clk_ns;

	if (tcmp == tcnt)
		tcmp--;
	
	if (tcmp < 0)
		tcmp = 0;

	/* Update the PWM register block. */
	spin_lock_irq(&odin->lock);

	__raw_writel(tcmp, odin->base + reg_tcmp);
	__raw_writel(tcnt, odin->base + reg_tcnt);

	tcon = __raw_readl(odin->base + CONTROL);
	/* pwm update */
	tcon |= 1 << (1+offset);

	/* pwm auto_reload */
	tcon |= 1 << (3+offset);
	__raw_writel(tcon, odin->base + CONTROL);

	/* pwm update clear */
	tcon &= ~(1 << (1+offset));
	__raw_writel(tcon, odin->base + CONTROL);

	spin_unlock_irq(&odin->lock);

	return 0;
}

static int odin_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct odin_chip *odin = (struct odin_chip *)chip;
	unsigned long tcon;
	int offset = 0;

	offset = pwm->hwpwm == 0 ? 0 : 5;

	spin_lock_irq(&odin->lock);
	tcon = __raw_readl(odin->base + CONTROL);
	/* pwm timer control register start bit set */
	tcon |= 1 << offset;
	__raw_writel(tcon, odin->base + CONTROL);
	spin_unlock_irq(&odin->lock);

	return 0;
}

static void odin_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct odin_chip *odin = to_odin_chip(chip);
	unsigned long tcon;
	int offset;

	offset = pwm->hwpwm == 0 ? 0 : 5;

	spin_lock_irq(&odin->lock);
	tcon = __raw_readl(odin->base + CONTROL);
	/* pwm timer control register start bit clear */
	tcon &= ~(1 << offset);
	__raw_writel(tcon, odin->base + CONTROL);
	spin_unlock_irq(&odin->lock);
}
static int odin_pwm_set_polarity(struct pwm_chip *chip, struct pwm_device *pwm,
								  enum pwm_polarity polarity)
{
	struct odin_chip *odin = to_odin_chip(chip);
	unsigned long tcon;
	int offset = 0;

	offset = pwm->hwpwm == 0 ? 0 : 5;

	spin_lock_irq(&odin->lock);
	tcon = __raw_readl(odin->base + CONTROL);

	if (polarity == PWM_POLARITY_INVERSED)
		/* inverter on for pwm timer */
		tcon |= 1 << (2+offset);
	else
		/* inverter off for pwm timer */
		tcon &= ~(1 << (2+offset));

	__raw_writel(tcon, odin->base + CONTROL);
	spin_unlock_irq(&odin->lock);
	return 0;
}

static struct pwm_ops odin_pwm_ops = {
	.enable		= odin_pwm_enable,
	.disable	= odin_pwm_disable,
	.config		= odin_pwm_config,
	.set_polarity = odin_pwm_set_polarity,
	.owner		= THIS_MODULE,
};

static const struct of_device_id odin_pwm_dt_ids[] = {
	{ .compatible = "LG,odin-pwm", },
};

static int odin_pwm_probe(struct platform_device *pdev)
{
	struct odin_chip *chip;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;
	int ret;

	if (!np)
		return -EINVAL;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (chip == NULL) {
		return -ENOMEM;
	}

	chip->chip.dev = &pdev->dev;
	chip->chip.ops = &odin_pwm_ops;
	chip->chip.base = -1;			/* determine base address */
	chip->chip.npwm = ODIN_NR_PWMS;
	chip->chip.of_xlate = of_pwm_xlate_with_flags;
	chip->chip.of_pwm_n_cells = 3;
	/* period : 0x400 pclk : 50MHz PWM : 48.828KHz */
	chip->duty_ns[0] = 0;
	chip->duty_ns[1] = 0;

	chip->clk = clk_get(&pdev->dev, "pwm_pclk");

	if (IS_ERR(chip->clk)) {
		ret = PTR_ERR(chip->clk);
		goto out;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		ret = -ENODEV;
		goto out;
	}
	chip->base = of_iomap(pdev->dev.of_node, 0);
	if (!chip->base) {
		ret = -EADDRNOTAVAIL;
		goto out;
	}
	ret = clk_prepare(chip->clk);
	if (ret < 0) {
		printk("failed to prepare clock\n");
		goto out;
	}

	ret = pwmchip_add(&chip->chip);
	if (ret < 0) {
		printk("failed to add PWM chip\n");
		goto out;
	}
	spin_lock_init(&chip->lock);
	platform_set_drvdata(pdev, chip);
	printk("%s 20024000 ",__func__);
	return 0;
out:
	clk_disable(chip->clk);
	return ret;
}
static int odin_pwm_remove(struct platform_device *pdev)
{
	struct odin_chip *chip;
	int ret;

	chip = platform_get_drvdata(pdev);
	if (chip == NULL)
		return -ENODEV;

	ret = pwmchip_remove(&chip->chip);
	if (ret < 0)
		return ret;

	clk_unprepare(chip->clk);
	return 0;
}

static int odin_pwm_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct odin_chip *chip = platform_get_drvdata(pdev);

	chip->duty_ns[chip->chip.pwms->hwpwm] = 0;
	return 0;
}

static int odin_pwm_resume(struct platform_device *pdev)
{
	return 0;
}

#if defined(CONFIG_PM_RUNTIME)
static int odin_pwm_runtime_suspend(struct device *dev)
{
	struct odin_chip *chip = dev_get_drvdata(dev);

	clk_disable_unprepare(chip->clk);
	return 0;
}

static int odin_pwm_runtime_resume(struct device *dev)
{
	struct odin_chip *chip = dev_get_drvdata(dev);
	int ret = 0;
	ret = clk_prepare_enable(chip->clk);
	return ret;
}
#else
#define odin_pwm_runtime_suspend	NULL
#define odin_pwm_runtime_resume		NULL
#endif

static struct dev_pm_ops odin_pwm_dev_pm_ops = {
	SET_RUNTIME_PM_OPS(odin_pwm_runtime_suspend, odin_pwm_runtime_resume, NULL)
};
static struct platform_driver odin_pwm_driver = {
	.probe		= odin_pwm_probe,
	.remove		= odin_pwm_remove,
	.suspend	= odin_pwm_suspend,
	.resume		= odin_pwm_resume,
	.driver		= {
		.name 	= "odin-pwm",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(odin_pwm_dt_ids),
		#if 0
		.pm = &odin_pwm_dev_pm_ops,
		#endif
	},
};

int __init pwm_init(void)
{
	return platform_driver_register(&odin_pwm_driver);
}
void __exit pwm_exit(void)
{
	platform_driver_unregister(&odin_pwm_driver);
}
module_init(pwm_init);
module_exit(pwm_exit);

MODULE_LICENSE("GPL v2");
