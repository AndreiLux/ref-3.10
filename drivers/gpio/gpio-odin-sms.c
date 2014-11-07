/*
 * linux/drivers/gpio/gpio-odin-sms.c
 */
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/list.h>
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
#include <linux/odin_mailbox.h>
#include <linux/notifier.h>

#include <linux/platform_data/gpio-odin.h>
#include <linux/platform_data/odin_tz.h>

u32 odin_gpio_read(void __iomem *addr)
{
#ifdef CONFIG_ODIN_TEE
	return tz_read(addr);
#else
	return readl(addr);
#endif
}

void odin_gpio_write(u32 b, void __iomem *addr)
{
#ifdef CONFIG_ODIN_TEE
	return tz_write(b, addr);
#else
	return writel(b, addr);
#endif
}

static void odin_gpio_sms_set_debounce_level(unsigned *conf_debounce,
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

static int __odin_gpio_sms_set_debounce(struct gpio_chip *gc, unsigned offset,
		unsigned debounce, unsigned type, unsigned clk)
{
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);
	unsigned long flags;
	unsigned long conf_debounce;

	conf_debounce = odin_gpio_read(chip->base + CONF_DEBOUNCE + (offset << 2));

	/* enable deb_en */
	conf_debounce |= (1 << 12);

	/* debounce type */
	switch (type) {
	case DEBOUNCE_TYPE_HOLDBUF:
		conf_debounce |= (1 << 16);
		break;
	case DEBOUNCE_TYPE_REGISTER:
		conf_debounce &= ~(1 << 16);
		odin_gpio_sms_set_debounce_level(&conf_debounce, debounce);
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

	odin_gpio_write(conf_debounce, chip->base + CONF_DEBOUNCE + (offset << 2));

	return 0;
}

static int odin_gpio_sms_set_debounce(struct gpio_chip *gc, unsigned offset,
		unsigned debounce)
{
	return __odin_gpio_sms_set_debounce(gc, offset, debounce,
			DEBOUNCE_TYPE_REGISTER, DEBOUNCE_CLK_PCLK);
}

static int odin_gpio_sms_direction_input(struct gpio_chip *gc, unsigned offset)
{
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);
	unsigned long flags;
	unsigned long conf_gpio;

	if (offset >= gc->ngpio)
		return -EINVAL;

	spin_lock_irqsave(&chip->lock, flags);

	conf_gpio = odin_gpio_read(chip->base + CONF_GPIO + (offset << 2));
#ifdef CONFIG_ARCH_ODIN
	conf_gpio &= 0xfffffffc;
#else
	conf_gpio |= 0x03;
#endif
	odin_gpio_write(conf_gpio, chip->base + CONF_GPIO + (offset << 2));

	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static int odin_gpio_sms_direction_output(struct gpio_chip *gc,
		unsigned offset, int value)
{
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);
	unsigned long flags;
	unsigned long conf_gpio;

	if (offset >= gc->ngpio)
		return -EINVAL;

	spin_lock_irqsave(&chip->lock, flags);

	conf_gpio = odin_gpio_read(chip->base + CONF_GPIO + (offset << 2));

	conf_gpio &= 0x00fffffc;
	conf_gpio |= 0x01;
	conf_gpio |= !!value << 24;
	odin_gpio_write(conf_gpio, chip->base + CONF_GPIO + (offset << 2));

	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static int odin_gpio_sms_get_value(struct gpio_chip *gc, unsigned offset)
{
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);
	unsigned long conf_gpio;

	if (offset >= gc->ngpio)
		return -EINVAL;

	conf_gpio = odin_gpio_read(chip->base + READ_PDATA);
	return (conf_gpio >> offset) & 0x01;
}

static void odin_gpio_sms_set_value(struct gpio_chip *gc, unsigned offset,
		int value)
{
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);
	unsigned long conf_gpio;

	if (offset >= gc->ngpio)
		return;

	conf_gpio = odin_gpio_read(chip->base + CONF_GPIO + (offset << 2));
	conf_gpio &= 0x00ffffff;
	conf_gpio |= !!value << 24;

	odin_gpio_write(conf_gpio, chip->base + CONF_GPIO + (offset << 2));
}

static void odin_gpio_sms_irq_mask(struct irq_data *data)
{
	struct odin_gpio *chip = irq_get_chip_data(data->irq);
	int offset = data->irq - chip->irq_base;
	unsigned long eint_mask;

	eint_mask = odin_gpio_read(chip->base + EINT_MASK);
	eint_mask |= (1 << offset);
	odin_gpio_write(eint_mask, chip->base + EINT_MASK);
}

static void odin_gpio_sms_irq_unmask(struct irq_data *data)
{
	struct odin_gpio *chip = irq_get_chip_data(data->irq);
	int offset = data->irq - chip->irq_base;
	unsigned long eint_mask;

	eint_mask = odin_gpio_read(chip->base + EINT_MASK);
	eint_mask &= ~(1 << offset);
	odin_gpio_write(eint_mask, chip->base + EINT_MASK);
}

static void odin_gpio_sms_irq_disable(struct irq_data *data)
{
	struct odin_gpio *chip = irq_get_chip_data(data->irq);
	int offset = data->irq - chip->irq_base;
	unsigned long flags;
	unsigned long conf_debounce;

	spin_lock_irqsave(&chip->irq_lock, flags);

	conf_debounce = odin_gpio_read(chip->base + CONF_DEBOUNCE + (offset << 2));
	conf_debounce &= 0xfffffffe;
	odin_gpio_write(conf_debounce, chip->base + CONF_DEBOUNCE + (offset << 2));

	spin_unlock_irqrestore(&chip->irq_lock, flags);
}

static void odin_gpio_sms_irq_enable(struct irq_data *data)
{
	struct odin_gpio *chip = irq_get_chip_data(data->irq);
	int offset = data->irq - chip->irq_base;
	unsigned long flags;
	unsigned long conf_debounce;

	odin_gpio_sms_irq_unmask(data);

	spin_lock_irqsave(&chip->irq_lock, flags);

	conf_debounce = odin_gpio_read(chip->base + CONF_DEBOUNCE + (offset << 2));
	conf_debounce |= 0x01;
	odin_gpio_write(conf_debounce, chip->base + CONF_DEBOUNCE + (offset << 2));

	spin_unlock_irqrestore(&chip->irq_lock, flags);
}

static int odin_gpio_sms_irq_type(struct irq_data *data, unsigned int trigger)
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

	conf_debounce = odin_gpio_read(chip->base + CONF_DEBOUNCE + (offset << 2));
	conf_debounce &= 0xffffff0f;
	conf_debounce |= irq_type << 4;

	odin_gpio_write(conf_debounce, chip->base + CONF_DEBOUNCE + (offset << 2));

	spin_unlock_irqrestore(&chip->irq_lock, flags);

	if (trigger & (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_LEVEL_HIGH))
		__irq_set_handler_locked(data->irq, handle_level_irq);
	else if (trigger & (IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING)) {
		__irq_set_handler_locked(data->irq, handle_edge_irq);
		__odin_gpio_sms_set_debounce(&chip->gc, offset, 0,
				DEBOUNCE_TYPE_REGISTER, DEBOUNCE_CLK_PCLK);
	}

	return 0;
}

static void odin_gpio_sms_irq_ack(struct irq_data *data)
{
	struct odin_gpio *chip = irq_get_chip_data(data->irq);
	int offset = data->irq - chip->irq_base;

	odin_gpio_write((1 << offset), chip->base + EINT_PEND);
}

static int odin_gpio_sms_irq_wake(struct irq_data *data, unsigned int on)
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
	.name		= "GPIO",
	.irq_enable	= odin_gpio_sms_irq_enable,
	.irq_disable	= odin_gpio_sms_irq_disable,
	.irq_set_type	= odin_gpio_sms_irq_type,
	.irq_mask	= odin_gpio_sms_irq_disable,
	.irq_unmask	= odin_gpio_sms_irq_enable,
	.irq_ack	= odin_gpio_sms_irq_ack,
	.irq_set_wake	= odin_gpio_sms_irq_wake,
};

static int odin_gpio_sms_to_irq(struct gpio_chip *gc, unsigned offset)
{
	int irq;
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);

	irq = chip->irq_base + offset;
	irq_set_chip(irq, &odin_irqchip);
	set_irq_flags(irq, IRQF_VALID);
	irq_set_chip_data(irq, chip);
	irq_set_nested_thread(irq, 0);

	return irq;
}

static int odin_gpio_sms_chip_init(struct odin_gpio *chip)
{
	int ret;

	chip->gc.direction_input = odin_gpio_sms_direction_input;
	chip->gc.direction_output = odin_gpio_sms_direction_output;
	chip->gc.get = odin_gpio_sms_get_value;
	chip->gc.set = odin_gpio_sms_set_value;
	chip->gc.to_irq = odin_gpio_sms_to_irq;
	chip->gc.set_debounce = odin_gpio_sms_set_debounce;
	chip->gc.base = chip->gpio_base;
	chip->gc.ngpio = ODIN_GPIO_NR;
	chip->gc.label = dev_name(chip->dev);
	chip->gc.dev = chip->dev;
	chip->gc.owner = THIS_MODULE;

	ret = gpiochip_add(&chip->gc);

	return ret;
}

static int odin_gpio_sms_probe(struct platform_device *pdev)
{
	struct odin_gpio_platform_data *pdata;
	struct odin_gpio *chip;
	struct list_head *chip_list;
	int ret;
	int irq_base_offset = -1;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (chip == NULL)
		return -ENOMEM;

#ifdef CONFIG_ODIN_TEE
	chip->base = pdev->resource[0].start;
#else
	if (!request_mem_region(pdev->resource[0].start, \
		resource_size(&pdev->resource[0]), "odin_gpio_sms")) {
		ret = -EBUSY;
		goto free_mem;
	}

	chip->base = ioremap(pdev->resource[0].start,
			resource_size(&pdev->resource[0]));
	if (chip->base == NULL) {
		ret = -ENOMEM;
		goto release_region;
	}
#endif

	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node, "bank_num",
				&chip->gpio_base);
		if (ret < 0)
			goto iounmap;
		ret = of_property_read_u32(pdev->dev.of_node, "irq_base_offset",
				&irq_base_offset);
		if (ret < 0)
			goto iounmap;
	}
	else {
		pdata = pdev->dev.platform_data;
		if (pdata == NULL) {
			ret = -ENODEV;
			goto iounmap;
		}

		chip->gpio_base = pdata->gpio_base;
	}

	chip->irq_base = pl320_gpio_irq_base(MB_SMS_GPIO_HANDLER_BASE,
			irq_base_offset);
	spin_lock_init(&chip->lock);
	spin_lock_init(&chip->irq_lock);
	INIT_LIST_HEAD(&chip->list);
	chip->dev = &pdev->dev;
	ret = odin_gpio_sms_chip_init(chip);
	if (ret < 0)
		goto iounmap;

	platform_set_drvdata(pdev, chip);

	return 0;

iounmap:
	iounmap(chip->base);
release_region:
	release_mem_region(pdev->resource[0].start, resource_size(&pdev->resource[0]));
free_mem:
	kfree(chip);

	return ret;
}

static int odin_gpio_sms_remove(struct platform_device *pdev)
{
	struct odin_gpio *chip = platform_get_drvdata(pdev);
	int ret;

	ret = gpiochip_remove(&chip->gc);
	if (ret == 0)
		kfree(chip);

	return ret;
}

#if CONFIG_OF
static const struct of_device_id odin_gpio_sms_dt_match[] __initdata = {
	{.compatible = "LG,odin-gpio-sms",},
	{}
};
#endif
static struct platform_driver odin_gpio_sms_driver = {
	.driver.name		= "odin_gpio_sms",
	.driver.owner		= THIS_MODULE,
	.driver.of_match_table	= of_match_ptr(odin_gpio_sms_dt_match),
	.probe			= odin_gpio_sms_probe,
	.remove			= odin_gpio_sms_remove,
};

static int __init odin_gpio_sms_init(void)
{
	return platform_driver_register(&odin_gpio_sms_driver);
}
arch_initcall(odin_gpio_sms_init);

MODULE_AUTHOR("Hyogi Gim <hyogi.gim@lge.com>");
MODULE_DESCRIPTION("Odin GPIO[SMS] controller driver");
MODULE_LICENSE("GPL");
