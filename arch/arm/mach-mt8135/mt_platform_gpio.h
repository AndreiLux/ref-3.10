#ifndef __MT_PLATFORM_GPIO_H_
#define __MT_PLATFORM_GPIO_H_

/*
 * Private definitions for mt_platform_gpio.c
 */
#include <mach/mt_reg_base.h> /* defines GPIO_BASE, GPIO1_BASE */
#include <mach/mt_gpio_def.h>

/* defines below could be generated, but we have no data */

#define MT8135_GPIO_BASE			(GPIO_BASE)
#define MT8135_GPIO_BASE2			(GPIO1_BASE)

#define MT6397_GPIO_BASE			0xC000

#define MT8135_GPIO_PINS			203
#define MT6397_GPIO_PINS			41

#define MT8135_EINTS			192
#define MT6397_EINTS			25

#define	MT8135_GPIO_DIR_OFFSET		0x0000
#define	MT8135_GPIO_EIS_OFFSET		0x0100
#define	MT8135_GPIO_PULLEN_OFFSET	0x0200
#define	MT8135_GPIO_PULLSEL_OFFSET	0x0400
#define	MT8135_GPIO_DINV_OFFSET		0x0600
#define	MT8135_GPIO_DOUT_OFFSET		0x0800
#define	MT8135_GPIO_DOIN_OFFSET		0x0A00

#define	MT6397_GPIO_DIR_OFFSET		0x000
#define	MT6397_GPIO_PULLEN_OFFSET	0x020
#define	MT6397_GPIO_PULLSEL_OFFSET	0x040
#define	MT6397_GPIO_DINV_OFFSET		0x060
#define	MT6397_GPIO_DOUT_OFFSET		0x080
#define	MT6397_GPIO_DOIN_OFFSET		0x0A0

#define MT8135_PIN_DATA(_port, _offset, _pins, _base)	\
{	\
	.base = (MT8135_GPIO_##_base) + ((_port) << 4), \
	.pins	= (_pins), \
	.offset	= (_offset), \
}

#define MT6397_PIN_DATA(_port, _pins)	\
{	\
	.base = (MT6397_GPIO_BASE) + ((_port) << 3), \
	.pins	= (_pins), \
	.offset	= 0, \
}

#define MT8135_GPIO_DATA(_start, _pins)	\
{	\
	.base   = MT8135_GPIO_BASE, \
	.start  = (_start), \
	.pins	= (_pins), \
}

#define MT6397_GPIO_DATA(_port, _pins)	\
{	\
	.base = MT6397_GPIO_BASE + ((_port) << 3), \
	.pins	= (_pins), \
	.flags  = MT6397_GPIO, \
}

struct mt_pin_desc;
struct mt_gpio_chip_desc;
struct mt_pin_drv_grp_desc;
struct mt_pin_drv_cls;
struct mt_pin_def;
struct mt_gpio_chip_def;
struct mt_eint_def;
struct mt_pin_drv_grp_def;

struct mt_gpio_platform_data_desc {
	const struct mt_pin_desc *gpio_pin_descs;
	const struct mt_gpio_chip_desc *gpio_chip_descs;
	const struct mt_pin_drv_grp_desc *pin_drv_grp_descs;
	const struct mt_pin_drv_cls *pin_drv_cls;
	struct mt_pin_def gpio_pins[MT8135_GPIO_PINS+MT6397_GPIO_PINS];
	struct mt_eint_def eints[MT8135_EINTS+MT6397_EINTS];
	struct mt_gpio_chip_def gpio_chips[2];
	struct mt_pin_drv_grp_def *pin_drv_grps;
	size_t n_pin_drv_clss;
	size_t n_pin_drv_grps;
};

#endif /* __MT_PLATFORM_GPIO_H_ */
