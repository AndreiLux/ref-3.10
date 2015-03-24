/******************************************************************************
 * mt_gpio_base.c - MTKLinux GPIO Device Driver
 *
 * Copyright 2008-2009 MediaTek Co.,Ltd.
 *
 * DESCRIPTION:
 *     This file provid the other drivers GPIO relative functions
 *
 ******************************************************************************/

#include <mach/mt_reg_base.h>
#include <mach/mt_gpio.h>
#include <mach/mt_gpio_core.h>
#include <mach/mt_typedefs.h>

#include <linux/gpio.h>
#include <mach/mt_gpio_def.h>

/******************************************************************************
Enumeration/Structure
******************************************************************************/
static GPIO_REGS *gpio_reg = (GPIO_REGS *) (GPIO_BASE);

static const char * const gpio_base_label[] = {
	"GPIO0", "GPIO1", "GPIO2", "GPIO3", "GPIO4", "GPIO5", "GPIO6", "GPIO7", "GPIO8", "GPIO9",
	"GPIO10", "GPIO11", "GPIO12", "GPIO13", "GPIO14", "GPIO15", "GPIO16", "GPIO17", "GPIO18", "GPIO19",
	"GPIO20", "GPIO21", "GPIO22", "GPIO23", "GPIO24", "GPIO25", "GPIO26", "GPIO27", "GPIO28", "GPIO29",
	"GPIO30", "GPIO31", "GPIO32", "GPIO33", "GPIO34", "GPIO35", "GPIO36", "GPIO37", "GPIO38", "GPIO39",
	"GPIO40", "GPIO41", "GPIO42", "GPIO43", "GPIO44", "GPIO45", "GPIO46", "GPIO47", "GPIO48", "GPIO49",
	"GPIO50", "GPIO51", "GPIO52", "GPIO53", "GPIO54", "GPIO55", "GPIO56", "GPIO57", "GPIO58", "GPIO59",
	"GPIO60", "GPIO61", "GPIO62", "GPIO63", "GPIO64", "GPIO65", "GPIO66", "GPIO67", "GPIO68", "GPIO69",
	"GPIO70", "GPIO71", "GPIO72", "GPIO73", "GPIO74", "GPIO75", "GPIO76", "GPIO77", "GPIO78", "GPIO79",
	"GPIO80", "GPIO81", "GPIO82", "GPIO83", "GPIO84", "GPIO85", "GPIO86", "GPIO87", "GPIO88", "GPIO89",
	"GPIO90", "GPIO91", "GPIO92", "GPIO93", "GPIO94", "GPIO95", "GPIO96", "GPIO97", "GPIO98", "GPIO99",
	"GPIO100", "GPIO101", "GPIO102", "GPIO103", "GPIO104", "GPIO105", "GPIO106", "GPIO107", "GPIO108", "GPIO109",
	"GPIO110", "GPIO111", "GPIO112", "GPIO113", "GPIO114", "GPIO115", "GPIO116", "GPIO117", "GPIO118", "GPIO119",
	"GPIO120", "GPIO121", "GPIO122", "GPIO123", "GPIO124", "GPIO125", "GPIO126", "GPIO127", "GPIO128", "GPIO129",
	"GPIO130", "GPIO131", "GPIO132", "GPIO133", "GPIO134", "GPIO135", "GPIO136", "GPIO137", "GPIO138", "GPIO139",
	"GPIO140", "GPIO141", "GPIO142", "GPIO143", "GPIO144", "GPIO145", "GPIO146", "GPIO147", "GPIO148", "GPIO149",
	"GPIO150", "GPIO151", "GPIO152", "GPIO153", "GPIO154", "GPIO155", "GPIO156", "GPIO157", "GPIO158", "GPIO159",
	"GPIO160", "GPIO161", "GPIO162", "GPIO163", "GPIO164", "GPIO165", "GPIO166", "GPIO167", "GPIO168", "GPIO169",
	"GPIO170", "GPIO171", "GPIO172", "GPIO173", "GPIO174", "GPIO175", "GPIO176", "GPIO177", "GPIO178", "GPIO179",
	"GPIO180", "GPIO181", "GPIO182", "GPIO183", "GPIO184", "GPIO185", "GPIO186", "GPIO187", "GPIO188", "GPIO189",
	"GPIO190", "GPIO191", "GPIO192", "GPIO193", "GPIO194", "GPIO195", "GPIO196", "GPIO197", "GPIO198", "GPIO199",
	"GPIO200", "GPIO201", "GPIO202"
};

/*---------------------------------------------------------------------------*/
int mt_set_gpio_dir_base(unsigned long pin, unsigned long dir)
{
	if (pin > MT_GPIO_BASE_MAX)
		return -ERINVAL;

	if (pin < 0 || pin > sizeof(gpio_base_label)/sizeof(gpio_base_label[0]))
		return -ERINVAL;

	gpio_request(pin, gpio_base_label[pin]);

	if (dir == GPIO_DIR_IN)
		return gpio_direction_input(pin);
	else if (dir == GPIO_DIR_OUT)
		return gpio_direction_output(pin, GPIO_OUT_DEFAULT);
	return RSUCCESS;
}

/*---------------------------------------------------------------------------*/
int mt_get_gpio_dir_base(unsigned long pin)
{
	return (mt_pin_get_dir(pin) == 0) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/
int mt_set_gpio_pull_enable_base(unsigned long pin, unsigned long enable)
{
	if (enable == GPIO_PULL_DISABLE)
		return mt_pin_set_pull(pin, MT_PIN_PULL_DISABLE);
	else if (enable == GPIO_PULL_ENABLE)
		return mt_pin_set_pull(pin, MT_PIN_PULL_ENABLE);
	return RSUCCESS;
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
int mt_get_gpio_pull_enable_base(unsigned long pin)
{
	bool pull_en = 0;
	mt_pin_get_pull(pin, &pull_en, NULL);
	return pull_en ? 1 : 0;
}

/*---------------------------------------------------------------------------*/
int mt_set_gpio_ies_base(unsigned long pin, unsigned long enable)
{
	unsigned long pos;
	unsigned long bit;
	GPIO_REGS *reg = gpio_reg;

	pos = pin / MAX_GPIO_REG_BITS;
	bit = pin % MAX_GPIO_REG_BITS;

	/* GPIO34~GPIO148 BASE_ADDRESS is different with others */
	if ((pin >= 34) && (pin <= 148)) {
		if (enable == GPIO_IES_DISABLE)
			GPIO_SET_BITS((1L << bit),
				      (unsigned long)(&reg->ies[pos].rst) + GPIO_OFFSET);
		else
			GPIO_SET_BITS((1L << bit),
				      (unsigned long)(&reg->ies[pos].set) + GPIO_OFFSET);
	} else {
		if (enable == GPIO_IES_DISABLE)
			GPIO_SET_BITS((1L << bit), &reg->ies[pos].rst);
		else
			GPIO_SET_BITS((1L << bit), &reg->ies[pos].set);
	}

	return RSUCCESS;
}

/*---------------------------------------------------------------------------*/
int mt_get_gpio_ies_base(unsigned long pin)
{
	unsigned long pos;
	unsigned long bit;
	unsigned long data;
	GPIO_REGS *reg = gpio_reg;

	pos = pin / MAX_GPIO_REG_BITS;
	bit = pin % MAX_GPIO_REG_BITS;

	/* GPIO34~GPIO148 BASE_ADDRESS is different with others */
	if ((pin >= 34) && (pin <= 148)) {
		data =
		    GPIO_RD32((unsigned short *)((unsigned long)(&reg->ies[pos].val) +
						 GPIO_OFFSET));
	} else {
		data = GPIO_RD32(&reg->ies[pos].val);
	}

	return ((data & (1L << bit)) != 0) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/
int mt_set_gpio_pull_select_base(unsigned long pin, unsigned long select)
{
	if (select == GPIO_PULL_DOWN)
		return mt_pin_set_pull(pin, MT_PIN_PULL_DOWN);
	else if (select == GPIO_PULL_UP)
		return mt_pin_set_pull(pin, MT_PIN_PULL_UP);
	return RSUCCESS;
}

/*---------------------------------------------------------------------------*/
int mt_get_gpio_pull_select_base(unsigned long pin)
{
	bool pull_up = 0;
	mt_pin_get_pull(pin, NULL, &pull_up);
	return pull_up ? 1 : 0;
}

/*---------------------------------------------------------------------------*/
int mt_set_gpio_inversion_base(unsigned long pin, unsigned long enable)
{
	unsigned long pos;
	unsigned long bit;

	GPIO_REGS *reg = gpio_reg;

	pos = pin / MAX_GPIO_REG_BITS;
	bit = pin % MAX_GPIO_REG_BITS;

	if (enable == GPIO_DATA_UNINV)
		GPIO_SET_BITS((1L << bit), &reg->dinv[pos].rst);
	else
		GPIO_SET_BITS((1L << bit), &reg->dinv[pos].set);
	return RSUCCESS;
}

/*---------------------------------------------------------------------------*/
int mt_get_gpio_inversion_base(unsigned long pin)
{
	unsigned long pos;
	unsigned long bit;
	unsigned long data;
	GPIO_REGS *reg = gpio_reg;


	pos = pin / MAX_GPIO_REG_BITS;
	bit = pin % MAX_GPIO_REG_BITS;

	data = GPIO_RD32(&reg->dinv[pos].val);

	return ((data & (1L << bit)) != 0) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/
int mt_set_gpio_out_base(unsigned long pin, unsigned long output)
{
	if (pin > MT_GPIO_BASE_MAX)
		return -ERINVAL;

	if (pin < 0 || pin > sizeof(gpio_base_label)/sizeof(gpio_base_label[0]))
		return -ERINVAL;

	gpio_request(pin, gpio_base_label[pin]);

	if (output == GPIO_OUT_ZERO)
		return gpio_direction_output(pin, GPIO_OUT_ZERO);
	else if (output == GPIO_OUT_ONE)
		return gpio_direction_output(pin, GPIO_OUT_ONE);
	return RSUCCESS;
}

/*---------------------------------------------------------------------------*/
int mt_get_gpio_out_base(unsigned long pin)
{
	return gpio_get_value(pin);
}

/*---------------------------------------------------------------------------*/
int mt_get_gpio_in_base(unsigned long pin)
{
	return gpio_get_value(pin);
}

/*---------------------------------------------------------------------------*/
int mt_set_gpio_mode_base(unsigned long pin, unsigned long mode)
{
	return mt_pin_set_mode(pin, mode);
}

/*---------------------------------------------------------------------------*/
int mt_get_gpio_mode_base(unsigned long pin)
{
	return mt_pin_get_mode(pin);
}

/*---------------------------------------------------------------------------*/
