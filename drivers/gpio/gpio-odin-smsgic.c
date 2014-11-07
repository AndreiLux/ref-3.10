/*
 * linux/drivers/gpio/gpio-odin-smssic.c
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
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/odin_mailbox.h>
#include <linux/notifier.h>

#include <linux/platform_data/gpio-odin.h>

static int odin_gpio_smsgic_eint_index(int irq)
{
	return irq - ODIN_SMS_GID_START;
}

static void odin_gpio_smsgic_set_debounce_level(unsigned *conf_debounce,
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

static int __odin_gpio_smsgic_set_debounce(struct gpio_chip *gc,
		unsigned offset, unsigned debounce, unsigned type, unsigned clk)
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
		odin_gpio_smsgic_set_debounce_level(&conf_debounce, debounce);
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

static int odin_gpio_smsgic_set_debounce(struct gpio_chip *gc, unsigned offset,
		unsigned debounce)
{
	return __odin_gpio_smsgic_set_debounce(gc, offset, debounce,
			DEBOUNCE_TYPE_REGISTER, DEBOUNCE_CLK_PCLK);
}

static int odin_gpio_smsgic_direction_input(struct gpio_chip *gc,
		unsigned offset)
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

static int odin_gpio_smsgic_direction_output(struct gpio_chip *gc,
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

static int odin_gpio_smsgic_get_value(struct gpio_chip *gc, unsigned offset)
{
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);
	unsigned long conf_gpio;

	if (offset >= gc->ngpio)
		return -EINVAL;

	conf_gpio = odin_gpio_read(chip->base + READ_PDATA);
	return (conf_gpio >> offset) & 0x01;
}

static void odin_gpio_smsgic_set_value(struct gpio_chip *gc, unsigned offset,
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

static int odin_gpio_smsgic_irq_to_offset(struct odin_gpio *chip,
		unsigned int irq)
{
	if (irq >= ODIN_SMS_GID_START && irq < ODIN_SMS_GID_MID)
		return irq - ODIN_SMS_GID_START;

	if (irq >= ODIN_SMS_GID_MID && irq <= ODIN_SMS_GID_END)
		return irq - ODIN_SMS_GID_MID;

	return -EINVAL;
}

static void __odin_gpio_smsgic_irq_disable(struct irq_data *data)
{
	int index = odin_gpio_smsgic_eint_index(data->irq);
	struct odin_gpio *chip = odin_gpio_chip[index];
	int offset = odin_gpio_smsgic_irq_to_offset(chip, data->irq);
	unsigned long flags;
	unsigned long conf_debounce;

	spin_lock_irqsave(&chip->irq_lock, flags);

	conf_debounce = odin_gpio_read(chip->base + CONF_DEBOUNCE + (offset << 2));
	conf_debounce &= 0xfffffffe;
	odin_gpio_write(conf_debounce, chip->base + CONF_DEBOUNCE + (offset << 2));

	spin_unlock_irqrestore(&chip->irq_lock, flags);
}

static void __odin_gpio_smsgic_irq_enable(struct irq_data *data)
{
	int index = odin_gpio_smsgic_eint_index(data->irq);
	struct odin_gpio *chip = odin_gpio_chip[index];
	int offset = odin_gpio_smsgic_irq_to_offset(chip, data->irq);
	unsigned long flags;
	unsigned long conf_debounce;

	spin_lock_irqsave(&chip->irq_lock, flags);

	conf_debounce = odin_gpio_read(chip->base + CONF_DEBOUNCE + (offset << 2));
	conf_debounce |= 0x01;
	odin_gpio_write(conf_debounce, chip->base + CONF_DEBOUNCE + (offset << 2));

	spin_unlock_irqrestore(&chip->irq_lock, flags);
}

int odin_gpio_smsgic_irq_disable(unsigned int irq)
{
	int ret = 0;

	ret = is_gic_direct_irq(irq);
	if (ret < 0) {
		return ret;
	}

	__odin_gpio_smsgic_irq_disable(irq_get_irq_data(irq));

	return 0;
}

int odin_gpio_smsgic_irq_enable(unsigned int irq)
{
	int ret = 0;

	ret = is_gic_direct_irq(irq);
	if (ret < 0) {
		return ret;
	}

	__odin_gpio_smsgic_irq_enable(irq_get_irq_data(irq));

	return 0;
}

static void __odin_gpio_smsgic_irq_mask(struct irq_data *data)
{
	int index = odin_gpio_smsgic_eint_index(data->irq);
	struct odin_gpio *chip = odin_gpio_chip[index];
	int offset = odin_gpio_smsgic_irq_to_offset(chip, data->irq);
	unsigned long eint_mask;

	eint_mask = odin_gpio_read(chip->base + EINT_MASK);
	eint_mask |= (1 << offset);
	odin_gpio_write(eint_mask, chip->base + EINT_MASK);
}

static void __odin_gpio_smsgic_irq_unmask(struct irq_data *data)
{
	int index = odin_gpio_smsgic_eint_index(data->irq);
	struct odin_gpio *chip = odin_gpio_chip[index];
	int offset = odin_gpio_smsgic_irq_to_offset(chip, data->irq);
	unsigned long eint_mask;

	eint_mask = odin_gpio_read(chip->base + EINT_MASK);
	eint_mask &= ~(1 << offset);
	odin_gpio_write(eint_mask, chip->base + EINT_MASK);
}

int odin_gpio_smsgic_irq_mask(unsigned int irq)
{
	int ret = 0;

	ret = is_gic_direct_irq(irq);
	if (ret < 0) {
		return ret;
	}

	__odin_gpio_smsgic_irq_mask(irq_get_irq_data(irq));

	return 0;
}

int odin_gpio_smsgic_irq_unmask(unsigned int irq)
{
	int ret = 0;

	ret = is_gic_direct_irq(irq);
	if (ret < 0) {
		return ret;
	}

	__odin_gpio_smsgic_irq_unmask(irq_get_irq_data(irq));

	return 0;
}

static int odin_gpio_smsgic_irq_type(struct irq_data *data,
		unsigned int trigger)
{
	int index = odin_gpio_smsgic_eint_index(data->irq);
	struct odin_gpio *chip = odin_gpio_chip[index];
	int offset = odin_gpio_smsgic_irq_to_offset(chip, data->irq);

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

	if (trigger & (IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING)) {
		__odin_gpio_smsgic_set_debounce(&chip->gc, offset, 0,
				DEBOUNCE_TYPE_REGISTER, DEBOUNCE_CLK_PCLK);
	}
	return 0;
}

static void __odin_gpio_smsgic_irq_ack(struct irq_data *data)
{
	int index = odin_gpio_smsgic_eint_index(data->irq);
	struct odin_gpio *chip = odin_gpio_chip[index];
	int offset = odin_gpio_smsgic_irq_to_offset(chip, data->irq);

	odin_gpio_write((1 << offset), chip->base + EINT_PEND);
}

int odin_gpio_smsgic_irq_ack(unsigned int irq)
{
	int ret = 0;

	ret = is_gic_direct_irq(irq);
	if (ret < 0) {
		return ret;
	}

	__odin_gpio_smsgic_irq_ack(irq_get_irq_data(irq));

	return 0;
}

static irqreturn_t odin_gpio_smsgic_irq_handler(int irq, void *dev_id)
{
	int ret;
	struct irq_data *data = irq_get_irq_data(irq);
	int index = odin_gpio_smsgic_eint_index(irq);

	__odin_gpio_smsgic_irq_disable(data);

	ret = odin_gpio_sms_handler[index](irq, dev_id);

	__odin_gpio_smsgic_irq_ack(data);
	__odin_gpio_smsgic_irq_enable(data);

	return ret;
}

static irqreturn_t odin_gpio_smsgic_irq_handler_modem(int irq, void *dev_id)
{
	int ret;
	struct irq_data *data = irq_get_irq_data(irq);
	int index = odin_gpio_smsgic_eint_index(irq);

	__odin_gpio_smsgic_irq_mask(data);
	__odin_gpio_smsgic_irq_ack(data);

	ret = odin_gpio_sms_handler[index](irq, dev_id);

	__odin_gpio_smsgic_irq_unmask(data);

	return ret;
}

int odin_gpio_sms_request_irq(unsigned int irq, irq_handler_t handler,
			unsigned long flags, const char *name, void *dev)
{
	struct irq_data *data = irq_get_irq_data(irq);
	int index = odin_gpio_smsgic_eint_index(irq);
	irq_handler_t smsgic_irq_handler;

	odin_gpio_smsgic_irq_type(data, flags & IRQF_TRIGGER_MASK);
	flags &= ~(IRQF_TRIGGER_MASK);
	__odin_gpio_smsgic_irq_enable(data);
	__odin_gpio_smsgic_irq_unmask(data);
	odin_gpio_sms_handler[index] = handler;

	if (irq == 204)	/* modem */
		smsgic_irq_handler = odin_gpio_smsgic_irq_handler_modem;
	else
		smsgic_irq_handler = odin_gpio_smsgic_irq_handler;

	return request_irq(irq, smsgic_irq_handler, flags, name, dev);
}
EXPORT_SYMBOL(odin_gpio_sms_request_irq);

int odin_gpio_sms_request_threaded_irq(unsigned int irq, irq_handler_t handler,
			irq_handler_t thread_fn, unsigned long irqflags,
			const char *devname, void *dev_id)
{
	struct irq_data *data = irq_get_irq_data(irq);
	int index = odin_gpio_smsgic_eint_index(irq);
	irq_handler_t smsgic_irq_handler;

	odin_gpio_smsgic_irq_type(data, irqflags & IRQF_TRIGGER_MASK);
	irqflags &= ~(IRQF_TRIGGER_MASK);
	__odin_gpio_smsgic_irq_enable(data);
	__odin_gpio_smsgic_irq_unmask(data);
	odin_gpio_sms_handler[index] = thread_fn;

	if (irq == 204)	/* modem */
		smsgic_irq_handler = odin_gpio_smsgic_irq_handler_modem;
	else
		smsgic_irq_handler = odin_gpio_smsgic_irq_handler;

	return request_threaded_irq(irq, handler, smsgic_irq_handler,
				irqflags, devname, dev_id);
}
EXPORT_SYMBOL(odin_gpio_sms_request_threaded_irq);

int odin_gpio_sms_request_any_context_irq(unsigned int irq,
			irq_handler_t handler, unsigned long flags,
			const char *name, void *dev_id)
{
	struct irq_desc *desc = irq_to_desc(irq);
	int ret;

	if (!desc)
		return -EINVAL;

	if (desc->status_use_accessors & IRQ_NESTED_THREAD) {
		ret = odin_gpio_sms_request_threaded_irq(irq, NULL, handler,
					   flags, name, dev_id);
		return !ret ? IRQC_IS_NESTED : ret;
	}

	ret = odin_gpio_sms_request_irq(irq, handler, flags, name, dev_id);
	return !ret ? IRQC_IS_HARDIRQ : ret;
}
EXPORT_SYMBOL(odin_gpio_sms_request_any_context_irq);

int is_gic_direct_irq(unsigned int irq)
{
	if (irq >= ODIN_SMS_GID_START && irq <= ODIN_SMS_GID_END) {
		return 1;
	}

	return -EINVAL;
}

static int odin_gpio_smsgic_to_irq(struct gpio_chip *gc, unsigned offset)
{
	int irq;
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);

	irq = chip->irq_base + offset;

	return irq;
}

static int odin_gpio_smsgic_request(struct gpio_chip *gc, unsigned offset)
{
	int irq;
	int index;
	struct odin_gpio *chip = container_of(gc, struct odin_gpio, gc);

	irq = chip->irq_base + offset;
	index = odin_gpio_smsgic_eint_index(irq);
	odin_gpio_chip[index] = chip;

	return 0;
}

static int odin_gpio_smsgic_chip_init(struct odin_gpio *chip)
{
	int ret;

	chip->gc.request = odin_gpio_smsgic_request;
	chip->gc.direction_input = odin_gpio_smsgic_direction_input;
	chip->gc.direction_output = odin_gpio_smsgic_direction_output;
	chip->gc.get = odin_gpio_smsgic_get_value;
	chip->gc.set = odin_gpio_smsgic_set_value;
	chip->gc.to_irq = odin_gpio_smsgic_to_irq;
	chip->gc.set_debounce = odin_gpio_smsgic_set_debounce;
	chip->gc.base = chip->gpio_base;
	chip->gc.ngpio = ODIN_GPIO_NR;
	chip->gc.label = dev_name(chip->dev);
	chip->gc.dev = chip->dev;
	chip->gc.owner = THIS_MODULE;

	ret = gpiochip_add(&chip->gc);

	return ret;
}

static int odin_gpio_smsgic_suspend(struct platform_device *dev,
	pm_message_t state)
{
	struct irq_desc *desc;
	int irq;

	for (irq = ODIN_SMS_GID_START; irq <= ODIN_SMS_GID_END; irq++) {
		desc = irq_to_desc(irq);

		if (!desc->action || (desc->action->flags & IRQF_NO_SUSPEND))
			continue;

		__odin_gpio_smsgic_irq_disable(&desc->irq_data);
	}

	return 0;
}

static int odin_gpio_smsgic_resume(struct platform_device *dev)
{
	struct irq_desc *desc;
	int irq;

	for (irq = ODIN_SMS_GID_START; irq <= ODIN_SMS_GID_END; irq++) {
		desc = irq_to_desc(irq);

		if (!desc->action || (desc->action->flags & IRQF_NO_SUSPEND))
			continue;

		__odin_gpio_smsgic_irq_enable(&desc->irq_data);
	}

	return 0;
}

static int odin_gpio_smsgic_probe(struct platform_device *pdev)
{
	struct odin_gpio_platform_data *pdata;
	struct odin_gpio *chip;
	struct list_head *chip_list;
	int ret;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (chip == NULL)
		return -ENOMEM;

#ifdef CONFIG_ODIN_TEE
	chip->base = pdev->resource[0].start;
#else
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
#endif

	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node, "bank_num", &chip->gpio_base);
		if (ret < 0)
			goto iounmap;
		ret = of_property_read_u32(pdev->dev.of_node, "irq_base", &chip->irq_base);
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

	spin_lock_init(&chip->lock);
	spin_lock_init(&chip->irq_lock);
	INIT_LIST_HEAD(&chip->list);
	chip->dev = &pdev->dev;
	ret = odin_gpio_smsgic_chip_init(chip);
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

static int odin_gpio_smsgic_remove(struct platform_device *pdev)
{
	struct odin_gpio *chip = platform_get_drvdata(pdev);
	int ret;

	ret = gpiochip_remove(&chip->gc);
	if (ret == 0)
		kfree(chip);

	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id odin_gpio_smsgic_dt_match[] __initdata = {
	{.compatible = "LG,odin-gpio-smsgic",},
	{}
};
#endif
static struct platform_driver odin_gpio_smsgic_driver = {
	.driver.name	= "odin_gpio_smsgic",
	.driver.owner	= THIS_MODULE,
	.driver.of_match_table = of_match_ptr(odin_gpio_smsgic_dt_match),
	.probe			= odin_gpio_smsgic_probe,
	.remove			= odin_gpio_smsgic_remove,
	.suspend		= odin_gpio_smsgic_suspend,
	.resume			= odin_gpio_smsgic_resume,
};

static int __init odin_gpio_smsgic_init(void)
{
	return platform_driver_register(&odin_gpio_smsgic_driver);
}
arch_initcall(odin_gpio_smsgic_init);

MODULE_AUTHOR("Hyogi Gim <hyogi.gim@lge.com>");
MODULE_DESCRIPTION("Odin GPIO[SMS] controller driver");
MODULE_LICENSE("GPL");
