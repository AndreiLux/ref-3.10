/******************************************************************************
 * mt_gpio_ext.c - MTKLinux GPIO Device Driver
 *
 * Copyright 2008-2009 MediaTek Co.,Ltd.
 *
 * DESCRIPTION:
 *     This file provid the other drivers GPIO debug functions
 *
 ******************************************************************************/

#include <mach/mt_reg_base.h>
#include <mach/mt_gpio.h>
#include <mach/mt_gpio_core.h>
#include <mach/mt_typedefs.h>

#include <linux/gpio.h>
#include <mach/mt_gpio_def.h>

#define MAX_GPIO_REG_BITS      16
#define GPIOEXT_BASE        (0xC000)	/* PMIC GPIO base. */

static GPIOEXT_REGS *gpioext_reg = (GPIOEXT_REGS *) (GPIOEXT_BASE);

static const char * const gpio_ext_label[] = {
	"GPIO203", "GPIO204", "GPIO205", "GPIO206", "GPIO207", "GPIO208", "GPIO209", "GPIO210",
	"GPIO211", "GPIO212", "GPIO213", "GPIO214", "GPIO215", "GPIO216", "GPIO217", "GPIO218",
	"GPIO219", "GPIO220", "GPIO221", "GPIO222", "GPIO223", "GPIO224", "GPIO225", "GPIO226",
	"GPIO227", "GPIO228", "GPIO229", "GPIO230", "GPIO231", "GPIO232", "GPIO233", "GPIO234",
	"GPIO235", "GPIO236", "GPIO237", "GPIO238", "GPIO239", "GPIO240", "GPIO241", "GPIO242",
	"GPIO243",
};

/*---------------------------------------------------------------------------*/
int mt_set_gpio_dir_ext(unsigned long pin, unsigned long dir)
{
	if (pin > MT_GPIO_EXT_MAX)
		return -ERINVAL;

	if (pin < 0 || pin > (sizeof(gpio_ext_label)/sizeof(gpio_ext_label[0]) + MT_GPIO_EXT_START))
		return -ERINVAL;

	gpio_request(pin, gpio_ext_label[pin - MT_GPIO_EXT_START]);

	if (dir == GPIO_DIR_IN)
		return gpio_direction_input(pin);
	else if (dir == GPIO_DIR_OUT)
		return gpio_direction_output(pin, GPIO_OUT_DEFAULT);
	return RSUCCESS;
}

/*---------------------------------------------------------------------------*/
int mt_get_gpio_dir_ext(unsigned long pin)
{
	return (mt_pin_get_dir(pin) == 0) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/
int mt_set_gpio_pull_enable_ext(unsigned long pin, unsigned long enable)
{
	if (enable == GPIO_PULL_DISABLE)
		return mt_pin_set_pull(pin, MT_PIN_PULL_DISABLE);
	else if (enable == GPIO_PULL_ENABLE)
		return mt_pin_set_pull(pin, MT_PIN_PULL_ENABLE);
	return RSUCCESS;
}

/*---------------------------------------------------------------------------*/
int mt_get_gpio_pull_enable_ext(unsigned long pin)
{
	bool pull_en = 0;
	mt_pin_get_pull(pin, &pull_en, NULL);
	return pull_en ? 1 : 0;
}

/*---------------------------------------------------------------------------*/
int mt_set_gpio_ies_ext(unsigned long pin, unsigned long enable)
{
	return RSUCCESS;
}

/*---------------------------------------------------------------------------*/
int mt_get_gpio_ies_ext(unsigned long pin)
{
	return RSUCCESS;
}

/*---------------------------------------------------------------------------*/
int mt_set_gpio_pull_select_ext(unsigned long pin, unsigned long select)
{
	if (select == GPIO_PULL_DOWN)
		return mt_pin_set_pull(pin, MT_PIN_PULL_DOWN);
	else if (select == GPIO_PULL_UP)
		return mt_pin_set_pull(pin, MT_PIN_PULL_UP);
	return RSUCCESS;
}

/*---------------------------------------------------------------------------*/
int mt_get_gpio_pull_select_ext(unsigned long pin)
{
	bool pull_up = 0;
	mt_pin_get_pull(pin, NULL, &pull_up);
	return pull_up ? 1 : 0;
}

/*---------------------------------------------------------------------------*/
int mt_set_gpio_inversion_ext(unsigned long pin, unsigned long enable)
{
	unsigned long pos;
	unsigned long bit;
	int ret = 0;
	GPIOEXT_REGS *reg = gpioext_reg;

	pin -= MT_GPIO_EXT_START;
	pos = pin / MAX_GPIO_REG_BITS;
	bit = pin % MAX_GPIO_REG_BITS;

	if (enable == GPIO_DATA_UNINV)
		ret = GPIOEXT_SET_BITS((1L << bit), &reg->dinv[pos].rst);
	else
		ret = GPIOEXT_SET_BITS((1L << bit), &reg->dinv[pos].set);
	if (ret != 0)
		return -ERWRAPPER;
	return RSUCCESS;
}

/*---------------------------------------------------------------------------*/
int mt_get_gpio_inversion_ext(unsigned long pin)
{
	unsigned long pos;
	unsigned long bit;
	signed long long data;
	GPIOEXT_REGS *reg = gpioext_reg;

	pin -= MT_GPIO_EXT_START;
	pos = pin / MAX_GPIO_REG_BITS;
	bit = pin % MAX_GPIO_REG_BITS;

	data = GPIOEXT_RD(&reg->dinv[pos].val);
	if (data < 0)
		return -ERWRAPPER;
	return ((data & (1L << bit)) != 0) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/
int mt_set_gpio_out_ext(unsigned long pin, unsigned long output)
{
	if (pin > MT_GPIO_EXT_MAX)
		return -ERINVAL;

	if (pin < 0 || pin > (sizeof(gpio_ext_label)/sizeof(gpio_ext_label[0]) + MT_GPIO_EXT_START))
		return -ERINVAL;

	gpio_request(pin, gpio_ext_label[pin - MT_GPIO_EXT_START]);

	if (output == GPIO_OUT_ZERO)
		return gpio_direction_output(pin, GPIO_OUT_ZERO);
	else if (output == GPIO_OUT_ONE)
		return gpio_direction_output(pin, GPIO_OUT_ONE);
	return RSUCCESS;
}

/*---------------------------------------------------------------------------*/
int mt_get_gpio_out_ext(unsigned long pin)
{
	return gpio_get_value(pin);
}

/*---------------------------------------------------------------------------*/
int mt_get_gpio_in_ext(unsigned long pin)
{
	return gpio_get_value(pin);
}

/*---------------------------------------------------------------------------*/
int mt_set_gpio_mode_ext(unsigned long pin, unsigned long mode)
{
	return mt_pin_set_mode(pin, mode);
}

/*---------------------------------------------------------------------------*/
int mt_get_gpio_mode_ext(unsigned long pin)
{
	return mt_pin_get_mode(pin);
}
