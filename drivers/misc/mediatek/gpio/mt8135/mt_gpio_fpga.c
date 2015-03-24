/******************************************************************************
 * mt_gpio_fpga.c - MTKLinux GPIO Device Driver
 *
 * Copyright 2008-2009 MediaTek Co.,Ltd.
 *
 * DESCRIPTION:
 *     This file provid the other drivers GPIO debug functions
 *
 ******************************************************************************/

#include <mach/mt_gpio.h>
#include <mach/mt_gpio_core.h>
#include <mach/mt_typedefs.h>

/* FPGA support code */
/* #if (defined(CONFIG_MT6589_FPGA)) */
#define MT_GPIO_DIR_REG 0xF100C008
#define MT_GPIO_OUT_REG 0xF100C000
#define MT_GPIO_IN_REG  0xF100C010

static void mt_gpio_set_bit(unsigned long nr, unsigned long reg)
{
	unsigned long value;
	value = readl(reg);
	value |= 1L << nr;
	writel(value, reg);
}

static void mt_gpio_clear_bit(unsigned long nr, unsigned long reg)
{
	unsigned long value;
	value = readl(reg);
	value &= ~(1L << nr);
	writel(value, reg);
}

static unsigned long mt_gpio_get_bit(unsigned long nr, unsigned long reg)
{
	unsigned long value;
	value = readl(reg);
	value &= (1L << nr);
	return value ? 1 : 0;
}

int mt_set_gpio_dir_base(unsigned long pin, unsigned long dir)
{
	int ret = RSUCCESS;
	unsigned long flags = 0;
	if (dir == GPIO_DIR_IN || dir == GPIO_DIR_DEFAULT)
		mt_gpio_clear_bit(pin, MT_GPIO_DIR_REG);
	else if (dir == GPIO_DIR_OUT)
		mt_gpio_set_bit(pin, MT_GPIO_DIR_REG);
	else
		ret = GPIO_DIR_UNSUPPORTED;
	return ret;
}

int mt_get_gpio_dir_base(unsigned long pin)
{
	int value;
	unsigned long flags = 0;
	value = mt_gpio_get_bit(pin, MT_GPIO_DIR_REG);
	return value;
}

int mt_set_gpio_pull_enable_base(unsigned long pin, unsigned long enable)
{
	return RSUCCESS;
}

int mt_get_gpio_pull_enable_base(unsigned long pin)
{
	return GPIO_PULL_EN_UNSUPPORTED;
}

int mt_set_gpio_pull_select_base(unsigned long pin, unsigned long select)
{
	return RSUCCESS;
}

int mt_get_gpio_pull_select_base(unsigned long pin)
{
	return GPIO_PULL_UNSUPPORTED;
}

int mt_set_gpio_inversion_base(unsigned long pin, unsigned long enable)
{
	return RSUCCESS;
}

int mt_get_gpio_inversion_base(unsigned long pin)
{
	return GPIO_DATA_INV_UNSUPPORTED;
}

int mt_set_gpio_out_base(unsigned long pin, unsigned long output)
{
	int ret = RSUCCESS;
	unsigned long flags = 0;
	if (output == GPIO_OUT_ZERO || output == GPIO_OUT_DEFAULT
	    || output == GPIO_DATA_OUT_DEFAULT) {
		mt_gpio_clear_bit(pin, MT_GPIO_OUT_REG);
	} else if (output == GPIO_OUT_ONE) {
		mt_gpio_set_bit(pin, MT_GPIO_OUT_REG);
	} else {
		ret = GPIO_OUT_UNSUPPORTED;
	}
	return ret;
}

int mt_get_gpio_out_base(unsigned long pin)
{
	int value;
	unsigned long flags = 0;
	value = mt_gpio_get_bit(pin, MT_GPIO_OUT_REG);

	return value;
}

int mt_get_gpio_in_base(unsigned long pin)
{
	int value;
	unsigned long flags = 0;
	value = mt_gpio_get_bit(pin, MT_GPIO_IN_REG);
	return value;
}

int mt_set_gpio_mode_base(unsigned long pin, unsigned long mode)
{
	return RSUCCESS;
}

int mt_get_gpio_mode_base(unsigned long pin)
{
	return GPIO_MODE_UNSUPPORTED;
}

int mt_set_clock_output_base(unsigned long num, unsigned long src, unsigned long div)
{
	return RSUCCESS;
}

int mt_get_clock_output_base(unsigned long num, unsigned long *src, unsigned long *div)
{
	return CLK_SRC_UNSUPPORTED;
}


#if 0
static ssize_t mt_gpio_dump_regs(char *buf, ssize_t bufLen)
{
	int idx = 0, len = 0;

	GPIOMSG("PIN: [DIR] [DOUT] [DIN]\n");
	for (idx = 0; idx < 8; idx++) {
		len += snprintf(buf + len, bufLen - len, "%d: %d %d %d\n",
				idx, mt_get_gpio_dir(idx), mt_get_gpio_out(idx),
				mt_get_gpio_in(idx));
	}
	GPIOMSG("PIN: [MODE] [PULL_SEL] [DIN] [DOUT] [PULL EN] [DIR] [INV]\n");
	for (idx = GPIO_EXTEND_START; idx < MAX_GPIO_PIN; idx++) {
		len += snprintf(buf + len, bufLen - len, "%d: %d %d %d %d %d %d %d\n",
				idx, mt_get_gpio_mode(idx), mt_get_gpio_pull_select(idx),
				mt_get_gpio_in(idx), mt_get_gpio_out(idx),
				mt_get_gpio_pull_enable(idx), mt_get_gpio_dir(idx),
				mt_get_gpio_inversion(idx));
	}

	return len;
}

/*---------------------------------------------------------------------------*/
static ssize_t mt_gpio_show_pin(struct device *dev, struct device_attribute *attr, char *buf)
{
/* GPIOEXT_WR(0x502,0x4000); */
/* GPIOMSG("AGPIO Setting:%x\n",GPIOEXT_RD(0x502)); */
	return mt_gpio_dump_regs(buf, PAGE_SIZE);
}

/*---------------------------------------------------------------------------*/
static ssize_t mt_gpio_store_pin(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	int pin;
	int mode, pullsel, dout, pullen, dir, dinv, din;

	if (!strncmp(buf, "-h", 2)) {
		GPIOMSG("PIN: [MODE] [DIR] [DOUT] [DIN] [PULL EN] [PULL SEL] [INV]\n");
	} else if (!strncmp(buf, "-w", 2)) {
		buf += 2;
		if (!strncmp(buf, "mode", 4) && (2 == sscanf(buf + 4, "%d %d", &pin, &mode)))
			GPIOMSG("set mode(%3d, %d)=%d\n", pin, mode, mt_set_gpio_mode(pin, mode));
		else if (!strncmp(buf, "psel", 4)
			 && (2 == sscanf(buf + 4, "%d %d", &pin, &pullsel)))
			GPIOMSG("set psel(%3d, %d)=%d\n", pin, pullsel,
				mt_set_gpio_pull_select(pin, pullsel));
		else if (!strncmp(buf, "dout", 4) && (2 == sscanf(buf + 4, "%d %d", &pin, &dout)))
			GPIOMSG("set dout(%3d, %d)=%d\n", pin, dout, mt_set_gpio_out(pin, dout));
		else if (!strncmp(buf, "pen", 3) && (2 == sscanf(buf + 3, "%d %d", &pin, &pullen)))
			GPIOMSG("set pen (%3d, %d)=%d\n", pin, pullen,
				mt_set_gpio_pull_enable(pin, pullen));
		else if (!strncmp(buf, "dir", 3) && (2 == sscanf(buf + 3, "%d %d", &pin, &dir)))
			GPIOMSG("set dir (%3d, %d)=%d\n", pin, dir, mt_set_gpio_dir(pin, dir));
		else if (!strncmp(buf, "dinv", 4) && (2 == sscanf(buf + 4, "%d %d", &pin, &dinv)))
			GPIOMSG("set dinv(%3d, %d)=%d\n", pin, dinv,
				mt_set_gpio_inversion(pin, dinv));
		else if (8 == sscanf(buf, "=%d:%d %d %d %d %d %d %d", &pin,
				&mode, &pullsel, &din, &dout, &pullen, &dir, &dinv)) {
			GPIOMSG("set mode(%3d, %d)=%d\n", pin, mode, mt_set_gpio_mode(pin, mode));
			GPIOMSG("set psel(%3d, %d)=%d\n", pin, pullsel,
				mt_set_gpio_pull_select(pin, pullsel));
			GPIOMSG("set dout(%3d, %d)=%d\n", pin, dout, mt_set_gpio_out(pin, dout));
			GPIOMSG("set pen (%3d, %d)=%d\n", pin, pullen,
				mt_set_gpio_pull_enable(pin, pullen));
			GPIOMSG("set dir (%3d, %d)=%d\n", pin, dir, mt_set_gpio_dir(pin, dir));
			GPIOMSG("set dinv(%3d, %d)=%d\n", pin, dinv,
				mt_set_gpio_inversion(pin, dinv));
		} else
			GPIOMSG("invalid format: '%s'", buf);
	}
	return count;
}
#endif
