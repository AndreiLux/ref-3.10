/*
* linux/drivers/gpio/gpio-odin.c
*/
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/bitops.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pm.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/irqchip/chained_irq.h>

#include <linux/platform_data/gpio-odin.h>

static int __odin_gpio_pull_up(struct gpio_chip *gc,
		unsigned offset, int enable)
{
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);
	unsigned long flags;
	unsigned long conf_gpio;

	if (offset >= gc->ngpio)
		return -EINVAL;

	spin_lock_irqsave(&chip->lock, flags);

	conf_gpio = readl(chip->base + CONF_GPIO + (offset << 2));

	if (enable > 0)
		conf_gpio &= ~(0x1 << ODIN_GPIO_PULL_UP);
	else
		conf_gpio |= (0x1 << ODIN_GPIO_PULL_UP);

	writel(conf_gpio, chip->base + CONF_GPIO + (offset << 2));

	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

int odin_gpio_pull_up(unsigned gpio, int enable)
{
	int ret;
	struct gpio_chip *chip;

	chip = gpio_to_chip(gpio);
	gpio -= chip->base;
	ret = __odin_gpio_pull_up(chip, gpio, enable);

	return ret;
}

static int __odin_gpio_pull_down(struct gpio_chip *gc,
		unsigned offset, int enable)
{
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);
	unsigned long flags;
	unsigned long conf_gpio;

	if (offset >= gc->ngpio)
		return -EINVAL;

	spin_lock_irqsave(&chip->lock, flags);

	conf_gpio = readl(chip->base + CONF_GPIO + (offset << 2));

	if (enable > 0)
		conf_gpio &= ~(0x1 << ODIN_GPIO_PULL_DOWN);
	else
		conf_gpio |= (0x1 << ODIN_GPIO_PULL_DOWN);

	writel(conf_gpio, chip->base + CONF_GPIO + (offset << 2));

	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

int odin_gpio_pull_down(unsigned gpio, int enable)
{
	int ret;
	struct gpio_chip *chip;

	chip = gpio_to_chip(gpio);
	gpio -= chip->base;
	ret = __odin_gpio_pull_down(chip, gpio, enable);

	return ret;
}

static int __odin_gpio_get_pinmux(struct gpio_chip *gc, unsigned offset)
{
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);
	unsigned long flags;
	unsigned long conf_gpio;

	if (offset >= gc->ngpio)
		return -EINVAL;

	spin_lock_irqsave(&chip->lock, flags);

	conf_gpio = readl(chip->base + CONF_GPIO + (offset << 2));
	conf_gpio &= 0x3;

	spin_unlock_irqrestore(&chip->lock, flags);

	return (int)conf_gpio;
}

int odin_gpio_get_pinmux(unsigned gpio)
{
	int ret;
	struct gpio_chip *chip;

	chip = gpio_to_chip(gpio);
	gpio -= chip->base;
	ret = __odin_gpio_get_pinmux(chip, gpio);

	return ret;
}

static void odin_gpio_set_debounce_level(unsigned *conf_debounce,
		unsigned level)
{
	unsigned int deb_lev = 0x0;

	/* set dev level for 24MHz clk */
	if (level > 10)
		deb_lev = 0xff;
	else
		deb_lev = level * 0x18;

	*conf_debounce &= ~(0xff << 20);
	*conf_debounce |= (deb_lev << 20);
}

static int __odin_gpio_set_debounce(struct gpio_chip *gc, unsigned offset,
				unsigned debounce, unsigned type, unsigned clk)
{
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);
	unsigned long flags;
	unsigned long conf_debounce;

	conf_debounce = readl(chip->base + CONF_DEBOUNCE + (offset << 2));

	/* enable deb_en */
	conf_debounce |= (1 << 12);

	/* debounce type */
	switch (type) {
	case DEBOUNCE_TYPE_HOLDBUF:
		conf_debounce |= (1 << 16);
		break;
	case DEBOUNCE_TYPE_REGISTER:
		conf_debounce &= ~(1 << 16);
		odin_gpio_set_debounce_level(&conf_debounce, debounce);
		break;
	default:
		return -EINVAL;
	}

	/* debounce clk */
	conf_debounce &= ~(0x03 << 8);
	switch (clk) {
	case DEBOUNCE_CLK_ALIVE:
		break;
	case DEBOUNCE_CLK_EXT:
		conf_debounce |= (0x01 << 8);
		break;
	case DEBOUNCE_CLK_PCLK:
		conf_debounce |= (0x02 << 8);
		break;
	default:
		return -EINVAL;
	}

	writel(conf_debounce, chip->base + CONF_DEBOUNCE + (offset << 2));

	return 0;
}

static int __odin_gpio_set_pinmux(struct gpio_chip *gc, unsigned offset,
		unsigned type)
{
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);
	unsigned long flags;
	unsigned long conf_gpio;

	if (offset >= gc->ngpio)
		return -EINVAL;

	spin_lock_irqsave(&chip->lock, flags);

	conf_gpio = readl(chip->base + CONF_GPIO + (offset << 2));
	conf_gpio &= 0xfffffffc;
	conf_gpio |= type;
	writel(conf_gpio, chip->base + CONF_GPIO + (offset << 2));

	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

int odin_gpio_set_pinmux(unsigned gpio, unsigned type)
{
	int ret;
	struct gpio_chip *chip;

	chip = gpio_to_chip(gpio);
	gpio -= chip->base;
	ret = __odin_gpio_set_pinmux(chip, gpio, type);

	return ret;
}

static int __odin_gpio_set_retention(struct gpio_chip *gc, unsigned offset,
		unsigned enable)
{
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);
	unsigned long flags;
	unsigned long conf_gpio;

	if (offset >= gc->ngpio)
		return -EINVAL;

	spin_lock_irqsave(&chip->lock, flags);

	conf_gpio = readl(chip->base + CONF_GPIO + (offset << 2));
	conf_gpio |= (!!enable) << 20;
	writel(conf_gpio, chip->base + CONF_GPIO + (offset << 2));

	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

int odin_gpio_set_retention(unsigned gpio, unsigned enable)
{
	int ret;
	struct gpio_chip *chip;

	chip = gpio_to_chip(gpio);
	gpio -= chip->base;
	ret = __odin_gpio_set_retention(chip, gpio, enable);

	return ret;
}

static int __odin_gpio_save_regs(struct gpio_chip *gc)
{
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);
	int offset;

	chip->save_regs.set_pdata = readl(chip->base + SET_PDATA);

	for (offset = 0; offset < ODIN_GPIO_NR; offset++) {
		chip->save_regs.conf_gpio[offset] = readl(chip->base + CONF_GPIO + \
			(offset << 2));
		chip->save_regs.conf_debounce[offset] = readl(chip->base + CONF_DEBOUNCE + \
			(offset << 2));
	}

	return 0;
}

int odin_gpio_save_regs(unsigned gpio_base)
{
	int ret;
	struct gpio_chip *chip;

	chip = gpio_to_chip(gpio_base);
	ret = __odin_gpio_save_regs(chip);

	return ret;
}

static int __odin_gpio_restore_regs(struct gpio_chip *gc)
{
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);
	int offset;

	for (offset = 0; offset < ODIN_GPIO_NR; offset++) {
		writel(chip->save_regs.conf_gpio[offset], chip->base + CONF_GPIO + \
			(offset << 2));
		writel(chip->save_regs.conf_debounce[offset], chip->base + CONF_DEBOUNCE + \
			(offset << 2));
	}

	writel(chip->save_regs.set_pdata, chip->base + SET_PDATA);

	return 0;
}

int odin_gpio_restore_regs(unsigned gpio_base)
{
	int ret;
	struct gpio_chip *chip;

	chip = gpio_to_chip(gpio_base);
	ret = __odin_gpio_restore_regs(chip);

	return ret;
}

int woden_gpio_save_regs(unsigned gpio_base)
{
	return odin_gpio_save_regs(gpio_base);
}

int woden_gpio_restore_regs(unsigned gpio_base)
{
	return odin_gpio_restore_regs(gpio_base);
}

void odin_gpio_regs_suspend(int start, int end, int size)
{
	int base;

	for (base = start; base < end; base+=size) {
		odin_gpio_save_regs(base);
	}
}

void odin_gpio_regs_resume(int start, int end, int size)
{
	int base;

	for (base = start; base < end; base+=size) {
		odin_gpio_restore_regs(base);
	}
}

static int odin_gpio_set_debounce(struct gpio_chip *gc, unsigned offset,
		unsigned debounce)
{
	return __odin_gpio_set_debounce(gc, offset, debounce,
				DEBOUNCE_TYPE_REGISTER, DEBOUNCE_CLK_PCLK);
}

static int odin_gpio_direction_input(struct gpio_chip *gc, unsigned offset)
{
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);
	unsigned long flags;
	unsigned long conf_gpio;

	if (offset >= gc->ngpio)
		return -EINVAL;

	spin_lock_irqsave(&chip->lock, flags);

	conf_gpio = readl(chip->base + CONF_GPIO + (offset << 2));
#ifdef CONFIG_ARCH_ODIN
	conf_gpio &= 0xfffffffc;
#else
	conf_gpio |= 0x03;
#endif
	writel(conf_gpio, chip->base + CONF_GPIO + (offset << 2));

	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static int odin_gpio_direction_output(struct gpio_chip *gc, unsigned offset,
		int value)
{
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);
	unsigned long flags;
	unsigned long conf_gpio;

	if (offset >= gc->ngpio)
		return -EINVAL;

	spin_lock_irqsave(&chip->lock, flags);

	conf_gpio = readl(chip->base + CONF_GPIO + (offset << 2));

	conf_gpio &= 0x00fffffc;
	conf_gpio |= 0x01;
	conf_gpio |= !!value << 24;
	writel(conf_gpio, chip->base + CONF_GPIO + (offset << 2));

	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static int odin_gpio_get_value(struct gpio_chip *gc, unsigned offset)
{
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);
	unsigned long conf_gpio;

	if (offset >= gc->ngpio)
		return -EINVAL;

	conf_gpio = readb(chip->base + READ_PDATA);
	return (conf_gpio >> offset) & 0x01;
}

static void odin_gpio_set_value(struct gpio_chip *gc, unsigned offset,
		int value)
{
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);
	unsigned long conf_gpio;

	if (offset >= gc->ngpio)
		return;

	conf_gpio = readl(chip->base + CONF_GPIO + (offset << 2));
	conf_gpio &= 0x00ffffff;
	conf_gpio |= !!value << 24;

	writel(conf_gpio, chip->base + CONF_GPIO + (offset << 2));
}

static int odin_gpio_to_irq(struct gpio_chip *gc, unsigned offset)
{
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);

	if (chip->irq_base == (unsigned)-1)
		return -EINVAL;

	return chip->irq_base + offset;
}

static void odin_gpio_irq_disable(struct irq_data *data)
{
	struct odin_gpio *chip = irq_get_chip_data(data->irq);
	int offset = data->irq - chip->irq_base;
	unsigned long flags;
	unsigned long conf_debounce;

	spin_lock_irqsave(&chip->irq_lock, flags);

	conf_debounce = readl(chip->base + CONF_DEBOUNCE + (offset << 2));
	conf_debounce &= 0xfffffffe;
	writel(conf_debounce, chip->base + CONF_DEBOUNCE + (offset << 2));

	spin_unlock_irqrestore(&chip->irq_lock, flags);
}

static void odin_gpio_irq_enable(struct irq_data *data)
{
	struct odin_gpio *chip = irq_get_chip_data(data->irq);
	int offset = data->irq - chip->irq_base;
	unsigned long flags;
	unsigned long conf_debounce;

	spin_lock_irqsave(&chip->irq_lock, flags);

	conf_debounce = readl(chip->base + CONF_DEBOUNCE + (offset << 2));
	conf_debounce |= 0x01;
	writel(conf_debounce, chip->base + CONF_DEBOUNCE + (offset << 2));

	spin_unlock_irqrestore(&chip->irq_lock, flags);
}

static int odin_gpio_irq_type(struct irq_data *data, unsigned int trigger)
{
	struct odin_gpio *chip = irq_get_chip_data(data->irq);
	int offset = data->irq - chip->irq_base;
	unsigned long flags;
	unsigned long conf_debounce;
	unsigned long irq_type = 0;

	if (offset < 0 || offset >= ODIN_GPIO_NR)
		return -EINVAL;

	spin_lock_irqsave(&chip->irq_lock, flags);

	switch (trigger) {
	case IRQ_TYPE_LEVEL_HIGH:
		irq_type = 0x1;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		irq_type = 0x0;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		irq_type = 0x7;
		break;
	case IRQ_TYPE_EDGE_RISING:
		irq_type = 0x5;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		irq_type = 0x3;
		break;
	default:
		break;
	}

	conf_debounce = readl(chip->base + CONF_DEBOUNCE + (offset << 2));
	conf_debounce &= 0xffffff0f;
	conf_debounce |= irq_type << 4;

	writel(conf_debounce, chip->base + CONF_DEBOUNCE + (offset << 2));

	spin_unlock_irqrestore(&chip->irq_lock, flags);

	if (trigger & (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_LEVEL_HIGH))
		__irq_set_handler_locked(data->irq, handle_level_irq);
	else if (trigger & (IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING)) {
		__irq_set_handler_locked(data->irq, handle_edge_irq);
		__odin_gpio_set_debounce(&chip->gc, offset, 0,
				DEBOUNCE_TYPE_REGISTER, DEBOUNCE_CLK_PCLK);
	}

	return 0;
}

static void odin_gpio_irq_mask(struct irq_data *data)
{
	struct odin_gpio *chip = irq_get_chip_data(data->irq);
	int offset = data->irq - chip->irq_base;
	unsigned long eint_mask;

	eint_mask = readl(chip->base + EINT_MASK);
	eint_mask |= (1 << offset);
	writel(eint_mask, chip->base + EINT_MASK);
}

static void odin_gpio_irq_unmask(struct irq_data *data)
{
	struct odin_gpio *chip = irq_get_chip_data(data->irq);
	int offset = data->irq - chip->irq_base;
	unsigned long eint_mask;

	eint_mask = readl(chip->base + EINT_MASK);
	eint_mask &= ~(1 << offset);
	writel(eint_mask, chip->base + EINT_MASK);
}

static void odin_gpio_irq_ack(struct irq_data *data)
{
	unsigned int irq = data->irq;
	struct odin_gpio *chip = irq_get_chip_data(irq);
	int offset = irq - chip->irq_base;

	writeb((1 << offset), chip->base + EINT_PEND);
}

static int odin_gpio_irq_wake(struct irq_data *data, unsigned int on)
{
	struct irq_desc *desc = irq_to_desc(data->irq);

	if (!desc)
		return -EINVAL;

	if (on) {
		if (desc->action)
			desc->action->flags |= IRQF_NO_SUSPEND;
	} else {
		if (desc->action)
			desc->action->flags &= ~IRQF_NO_SUSPEND;
	}

	return 0;
}

static struct irq_chip odin_irqchip = {
	.name			= "GPIO",
	.irq_set_type	= odin_gpio_irq_type,
	.irq_mask	= odin_gpio_irq_disable,
	.irq_unmask	= odin_gpio_irq_enable,
	.irq_ack	= odin_gpio_irq_ack,
	.irq_set_wake	= odin_gpio_irq_wake,
};

static void odin_gpio_irq_handler(unsigned irq, struct irq_desc *desc)
{
	unsigned long pending;
	int offset;
	struct odin_gpio *chip = irq_desc_get_handler_data(desc);
	struct irq_chip *irqchip = irq_desc_get_chip(desc);

	chained_irq_enter(irqchip, desc);

	pending = readb(chip->base + EINT_PEND);
	writeb(pending, chip->base + EINT_PEND);

	if (pending) {
		for_each_set_bit(offset, &pending, ODIN_GPIO_NR)
			generic_handle_irq(odin_gpio_to_irq(&chip->gc, offset));
	}

	chained_irq_exit(irqchip, desc);
}

static int odin_gpio_chip_init(struct odin_gpio *chip)
{
	int ret;

	chip->gc.direction_input = odin_gpio_direction_input;
	chip->gc.direction_output = odin_gpio_direction_output;
	chip->gc.get = odin_gpio_get_value;
	chip->gc.set = odin_gpio_set_value;
	chip->gc.to_irq = odin_gpio_to_irq;
	chip->gc.set_debounce = odin_gpio_set_debounce;
	chip->gc.base = chip->gpio_base;
	chip->gc.ngpio = ODIN_GPIO_NR;
	chip->gc.label = dev_name(chip->dev);
	chip->gc.dev = chip->dev;
	chip->gc.owner = THIS_MODULE;

	ret = gpiochip_add(&chip->gc);

	return ret;
}

static int odin_gpio_irq_init(struct odin_gpio *chip)
{
	int offset;
	int ret = 0;

	if (chip->irq <= 0)
		return -ENODEV;

	/* disable irqs */
	writeb(0xff, chip->base + EINT_MASK);

	/* set irq chained handler */
	irq_set_chained_handler(chip->irq, odin_gpio_irq_handler);
	irq_set_handler_data(chip->irq, chip);

	for (offset=0; offset<ODIN_GPIO_NR; offset++) {
		irq_set_chip(chip->irq_base + offset, &odin_irqchip);
		irq_set_handler(chip->irq_base + offset, handle_simple_irq);
		set_irq_flags(chip->irq_base + offset, IRQF_VALID);
		irq_set_chip_data(chip->irq_base + offset, chip);
	}

	writeb(0x00, chip->base + EINT_MASK);

	return ret;
}

static int odin_gpio_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct odin_gpio_platform_data *pdata;
	struct odin_gpio *chip;
	struct list_head *chip_list;
	int ret = 0;
	static DECLARE_BITMAP(init_irq, NR_IRQS);

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (chip == NULL)
		return -ENOMEM;

	if (!request_mem_region(pdev->resource[0].start, \
		resource_size(&pdev->resource[0]), "odin_gpio")) {
		ret = -EBUSY;
		goto free_mem;
	}

	chip->base = ioremap(pdev->resource[0].start,
			resource_size(&pdev->resource[0]));
	if (chip->base == NULL) {
		ret = -ENOMEM;
		goto release_region;
	}

	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node, "bank_num", &chip->gpio_base);
		if (ret < 0)
			goto iounmap;
	}
	else {
		pdata = pdev->dev.platform_data;
		if (pdata == NULL)
			goto iounmap;

		chip->gpio_base = pdata->gpio_base;
		chip->irq_base = pdata->irq_base;
	}

	spin_lock_init(&chip->lock);
	spin_lock_init(&chip->irq_lock);
	INIT_LIST_HEAD(&chip->list);
	chip->dev = &pdev->dev;
	chip->irq = pdev->resource[1].start;
#ifndef CONFIG_ARCH_ODIN
	if (chip->gpio_base < 64)
		chip->pclk = clk_get(chip->dev, "gpio_0_7");
	else
		chip->pclk = clk_get(chip->dev, "gpio_8_11");

	clk_prepare_enable(chip->pclk);
#endif
	ret = odin_gpio_chip_init(chip);
	if (ret < 0)
		goto iounmap;

	/* irq_chip support */
	if (chip->irq <= 0) {
		platform_set_drvdata(pdev, chip);
		return 0;
	}

	chip->irq_base = irq_alloc_descs(-1, 0, ODIN_GPIO_NR, numa_node_id());
	if (chip->irq_base < 0) {
		ret = chip->irq_base;
		goto iounmap;
	}

	chip->domain = irq_domain_add_legacy(np, ODIN_GPIO_NR, chip->irq_base, 0, \
						&irq_domain_simple_ops, NULL);
	if (chip->domain == NULL) {
		ret = -ENODEV;
		goto irqdesc_free;
	}

	ret = odin_gpio_irq_init(chip);
	if (ret < 0)
		goto iounmap;

	return 0;

irqdesc_free:
	irq_free_descs(chip->irq_base, ODIN_GPIO_NR);
iounmap:
	iounmap(chip->base);
release_region:
	release_mem_region(pdev->resource[0].start, resource_size(&pdev->resource[0]));
free_mem:
	kfree(chip);

	return ret;
}

static int odin_gpio_remove(struct platform_device *pdev)
{
	struct odin_gpio *chip = platform_get_drvdata(pdev);
	int ret;

	ret = gpiochip_remove(&chip->gc);
	if (ret == 0) {
#ifndef CONFIG_ARCH_ODIN
		clk_disable_unprepare(chip->pclk);
#endif
		iounmap(chip->base);
		release_mem_region(pdev->resource[0].start,
				resource_size(&pdev->resource[0]));
		kfree(chip);
	}

	return ret;
}

#ifdef CONFIG_PM
static int odin_gpio_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct odin_gpio *chip = platform_get_drvdata(pdev);
	int offset;

	chip->save_regs.set_pdata = readl(chip->base + SET_PDATA);

	for (offset = 0; offset < ODIN_GPIO_NR; offset++) {
		chip->save_regs.conf_gpio[offset] = readl(chip->base + CONF_GPIO + \
			(offset << 2));
		chip->save_regs.conf_debounce[offset] = readl(chip->base + CONF_DEBOUNCE + \
			(offset << 2));
	}
#ifndef CONFIG_ARCH_ODIN
	clk_prepare_enable(chip->pclk);
#endif
	return 0;
}

static int odin_gpio_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct odin_gpio *chip = platform_get_drvdata(pdev);
	int offset;

#ifndef CONFIG_ARCH_ODIN
	clk_disable_unprepare(chip->pclk);
#endif

	for (offset = 0; offset < ODIN_GPIO_NR; offset++) {
		writel(chip->save_regs.conf_gpio[offset], chip->base + CONF_GPIO + \
			(offset << 2));
		writel(chip->save_regs.conf_debounce[offset], chip->base + CONF_DEBOUNCE + \
			(offset << 2));
	}

	writel(chip->save_regs.set_pdata, chip->base + SET_PDATA);

	return 0;
}

static const struct dev_pm_ops odin_gpio_pm_ops = {
	SET_RUNTIME_PM_OPS(odin_gpio_suspend, odin_gpio_resume, NULL)
};
#endif

#if CONFIG_OF
static const struct of_device_id odin_gpio_dt_match[] __initdata = {
	{.compatible = "LG,odin-gpio",},
	{}
};
#endif
static struct platform_driver odin_gpio_driver = {
	.driver.name		= "odin_gpio",
	.driver.owner		= THIS_MODULE,
	.driver.of_match_table	= of_match_ptr(odin_gpio_dt_match),
	.probe			= odin_gpio_probe,
	.remove			= odin_gpio_remove,
#ifdef CONFIG_PM
/*	.driver.pm		= &odin_gpio_pm_ops, */
#endif
};

static int __init odin_gpio_init(void)
{
	return platform_driver_register(&odin_gpio_driver);
}
arch_initcall(odin_gpio_init);

MODULE_AUTHOR("Hyogi Gim <hyogi.gim@lge.com>");
MODULE_DESCRIPTION("Odin GPIO driver");
MODULE_LICENSE("GPL");
