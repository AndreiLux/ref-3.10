/*
 * Platform support for MediaTeK pin controls: GPIO, PINMUX, EINT
 *
 * Copyright (C) 2014, Alexey Polyudov <alexeyp@lab126.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/ctype.h>

/* platform definition shared with drivers */
#include <mach/mt_pmic_wrap.h>
#include <mach/devs.h>
#include <mach/eint.h>
#include <mach/mt_gpio_def.h>

/* generated data */
#include "mt_gpio_data.c"

/* private platform definition, may use generated data */
#include "mt_platform_gpio.h"

static int mt8135_read_reg(addr_t reg, u32 *d)
{
	rmb(); /* paired with mt8135_write_reg */
	*d = readl((void *)reg);
	pr_debug("DIRECT IO: [%08X] => %08X\n", reg, *d);
	return 0;
}

static int mt8135_write_reg(addr_t reg, u32 d)
{
	writel(d, (void *)(reg));
	wmb();  /* paired with mt8135_read_reg */
	pr_debug("DIRECT IO: [%08X] <= %08X\n", reg, d);
	return 0;
}

static int mt6397_read_reg(addr_t reg, u32 *d)
{
	int err;
	/* TODO: pwrap_* requires arbitration; need to re-implement */
	err = pwrap_read(reg, d);
	pr_debug("WRAP IO: [%08X] => %08X; err=%d\n", reg, *d, err);
	return err ? -EIO : 0;
}

static int mt6397_write_reg(addr_t reg, u32 d)
{
	/* TODO: pwrap_* requires arbitration; need to re-implement */
	int err = pwrap_write(reg, d);
	pr_debug("WRAP IO: [%08X] <= %08X; err=%d\n", reg, d, err);
	return err ? -EIO : 0;
}

static struct mt_gpio_chip_desc mt_gpio_chip_tbl[] = {
	{ .name = "MT8135", .flags = 0 },
	{ .name = "MT6397", .flags = MT_WRAPPER_BUS },
};

struct mt_gpio_platform_data_desc mt_gpio_platform_data_desc = {
	.gpio_pin_descs = mt_pin_desc_tbl,
	.gpio_chip_descs = mt_gpio_chip_tbl,
	.pin_drv_cls = mt_pin_drv_cls_tbl,
	.n_pin_drv_clss = ARRAY_SIZE(mt_pin_drv_cls_tbl),
	.pin_drv_grp_descs = mt_pin_drv_grp_tbl,
	.n_pin_drv_grps = ARRAY_SIZE(mt_pin_drv_grp_tbl),
	.gpio_chips = {
		[0] = {
			.n_gpio = MT8135_GPIO_PINS,
			.n_eint = MT8135_EINTS,
			.desc   = mt_gpio_chip_tbl,
			.base_pin = mt_gpio_platform_data_desc.gpio_pins,
			.base_eint = mt_gpio_platform_data_desc.eints,
			.parent = 0,
			.hw_base = { MT8135_GPIO_BASE, MT8135_GPIO_BASE2 },
			.dir_offset = 0x0000,
			.pullen_offset = 0x0200,
			.pullsel_offset = 0x0400,
			.drv_offset = 0x0500,
			.dsel_offset = 0x700,
			.dout_offset = 0x0800,
			.din_offset = 0x0A00,
			.pinmux_offset = 0x0C00,
			.type1_start = 34, /* first mode 1 pin# */
			.type1_end = 149, /* last mode 1 pin# +1 */
			.port_shf = 4,
			.port_mask = 0xF,
			.port_align = 4,
			.write_reg = mt8135_write_reg,
			.read_reg = mt8135_read_reg,
		},
		[1] = {
			.n_gpio = MT6397_GPIO_PINS,
			.n_eint = MT6397_EINTS,
			.desc   = mt_gpio_chip_tbl+1,
			.base_pin  = mt_gpio_platform_data_desc.gpio_pins
				+ MT8135_GPIO_PINS,
			.base_eint = mt_gpio_platform_data_desc.eints
				+ MT8135_EINTS,
			.parent = &mt_gpio_platform_data_desc.gpio_chips[0],
			.hw_base = { MT6397_GPIO_BASE },
			.dir_offset = 0x0000,
			.pullen_offset = 0x020,
			.pullsel_offset = 0x040,
			.dout_offset = 0x080,
			.din_offset = 0x0A0,
			.pinmux_offset = 0x0C0,
			.port_shf = 3,
			.port_mask = 0x3,
			.port_align = 2,
			.write_reg = mt6397_write_reg,
			.read_reg = mt6397_read_reg,
		},
	},
};

static struct mt_gpio_platform_data_desc *data;

struct mt_pin_def *mt_get_pin_def(u16 pin)
{
	if (pin < ARRAY_SIZE(data->gpio_pins))
		return &data->gpio_pins[pin];
	return NULL;
}

struct mt_eint_def *mt_get_eint_def(u16 eint)
{
	if (eint < ARRAY_SIZE(data->eints))
		return &data->eints[eint];
	return NULL;
}

/*
 * this function is meant to resolve multi-map
 * gpio->eint problems. It allows to declare gpio
 * pin that will be used in eint mode.
 */
int mt_pin_uses_eint(u16 gpio)
{
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	struct mt_eint_def *eint;

	if (!pin) {
		pr_err("Invalid platform pin: %d\n", gpio);
		return -EINVAL;
	}

	eint = pin->eint;
	if (!eint) {
		pr_err("GPIO->EINT bad translation for pin: %d\n", gpio);
		return -EINVAL;
	}

	if (eint->gpio != pin && eint->gpio) {
		pr_err("GPIO->EINT is already done by [%s]GPIO#%d [%s]\n",
			eint->gpio->mt_chip->desc->name,
			eint->gpio->id, eint->gpio->desc->pin);
		return -EBUSY;
	}

	eint->gpio = pin;
	return 0;
}

/**
 * Returns mode number (0..7), if found.
 * On error, returns MT_INVALID_MODE
 */
static int _find_pin_mode(const struct mt_pin_def *pin, const char *pfx, size_t pfx_len)
{
	int i;
	const struct mt_pin_desc *desc = pin->desc;
	for (i = 0; i < ARRAY_SIZE(desc->modes); ++i) {
		const char *mode = desc->modes[i];
		if (mode && !strnicmp(pfx, mode, pfx_len))
			return i;
	}
	return -1;
}

static struct mt_gpio_chip_def *_find_chip(const struct mt_pin_def *pin)
{
	u8 i;
	const char *chip_name = pin->desc->chip;
	for (i = 0; i < ARRAY_SIZE(data->gpio_chips); ++i) {
		if (strcasecmp(chip_name, data->gpio_chips[i].desc->name) == 0)
			return data->gpio_chips+i;
	}
	return NULL;
}

static struct mt_pin_drv_grp_def *_find_drv_grp(const char *grp_name)
{
	int i;
	struct mt_pin_drv_grp_def *grp;

	BUG_ON(!grp_name);

	for (i = 0, grp = data->pin_drv_grps;
		 i < data->n_pin_drv_grps; ++i, ++grp)
		if (!strcmp(grp->desc->name, grp_name))
			return grp;

	return NULL;
}

static const struct mt_pin_drv_cls *_find_drv_cls(const char *cls_name)
{
	int i;
	const struct mt_pin_drv_cls *cls;

	BUG_ON(!cls_name);

	for (i = 0, cls = data->pin_drv_cls;
		 i < data->n_pin_drv_clss; ++i, ++cls)
		if (!strcmp(cls->name, cls_name))
			return cls;

	return NULL;
}

int mt_pin_find_by_pin_name(int start_pin, const char *pin_name, const char *chip_name)
{
	int i;
	if (!pin_name)
		return -EINVAL;
	for (i = start_pin; i < ARRAY_SIZE(data->gpio_pins); ++i) {
		struct mt_pin_def *pin = &data->gpio_pins[i];
		if (!pin->desc || !pin->mt_chip)
			continue;
		if (chip_name &&
			strcasecmp(pin->mt_chip->desc->name, chip_name))
			continue;
		if (strnicmp(pin->desc->pin, pin_name, strlen(pin_name)))
			continue;
		return i;
	}
	return -ENOENT;
}
EXPORT_SYMBOL(mt_pin_find_by_pin_name);

static int __init mt_pinctrl_data_init(void)
{
	const struct mt_pin_desc *desc;
	const struct mt_pin_drv_grp_desc *grp_desc;
	int i, j, err;
	struct mt_pin_def *pin, *old_pin;
	struct mt_pin_drv_grp_def *grp;
	struct mt_gpio_chip_def *mt_chip;

	u8 gpio_mode;
	u8 eint_mode;
	u16 eint_num;
	struct mt_eint_def *eint;

	BUG_ON(data);

	data = &mt_gpio_platform_data_desc;

	pr_debug("%s: pin data parser start\n", __func__);

	for (i = 0; i < ARRAY_SIZE(data->gpio_chips); ++i)
		spin_lock_init(&data->gpio_chips[i].hw_lock);

	/* TODO: remove check once macros are generated from the same source */
	WARN(MT8135_GPIO_PINS + MT6397_GPIO_PINS !=
			ARRAY_SIZE(mt_pin_desc_tbl),
		"Generated pin data mismatch: %d <> %d\n",
		MT8135_GPIO_PINS + MT6397_GPIO_PINS,
		ARRAY_SIZE(mt_pin_desc_tbl));

	data->pin_drv_grps = kzalloc(
		data->n_pin_drv_grps * sizeof(struct mt_pin_drv_grp_def),
		GFP_KERNEL);
	if (!data->pin_drv_grps) {
		err = -ENOMEM;
		goto free_data;
	}

	for (i = 0; i < ARRAY_SIZE(data->eints); ++i)
		data->eints[i].id = i;

	/* find GPIO# and EINT# and mode for each pin */
	for (i = 0, pin = data->gpio_pins, desc = data->gpio_pin_descs;
			i < ARRAY_SIZE(data->gpio_pins); ++i, ++pin, ++desc) {
		pin->desc = desc;
		mt_chip = pin->mt_chip = _find_chip(pin);
		BUG_ON(!pin->mt_chip);

		/* estimate pin id; to be validated later */
		pin->id = i - (mt_chip->base_pin - data->gpio_pins);

		gpio_mode = _find_pin_mode(pin, "GPIO", 4);
		eint_mode = _find_pin_mode(pin, "EINT", 4);
		eint_num = MT_INVALID_EINT;
		eint = 0;
		pin->gpio_mode = gpio_mode;
		pin->eint_mode = eint_mode;

		/* pin pull and ies base is different (type 1 vs type 0) */
		if (gpio_mode != MT_INVALID_MODE) {
			u16 id;
			if (kstrtou16(
				desc->modes[gpio_mode] + 4, 10, &id)) {
				id = MT_INVALID_GPIO;
			}
			if (id == MT_INVALID_GPIO || pin->id != id) {
				pr_err("%s: %s:%s in record %d: GPIO# not found or out of order\n",
					__func__, mt_chip->desc->name,
					pin->desc->pin, i);
				continue;
			}
		}

		pin->type = (mt_chip->type1_start <= pin->id &&
			pin->id < mt_chip->type1_end) ? 1 : 0;

		pr_debug("%s: %s:GPIO%d[%s]: type = %d\n",
			__func__, pin->mt_chip->desc->name,
			pin->id, pin->desc->pin, pin->type);

		pin->pinmux_offset = mt_chip->pinmux_offset
			+ ((pin->id / 5) << mt_chip->port_shf);
		pin->pinmux_shf = (pin->id % 5) * 3;
		if (eint_mode != MT_INVALID_MODE) {
			unsigned long long tmp;
			int err = kstrtoull_chk(desc->modes[eint_mode] + 4, 10, &tmp, true);

			if (err || (u16)tmp != tmp) {
				eint_num = MT_INVALID_EINT;
				pr_err("%s: failed to decode EINT#: err=%d, eint_mode=%d, mode=%s\n",
					__func__, err, eint_mode, desc->modes[eint_mode]);
				continue;
			}


			/* convert chip-relative EINT number to global */
			eint_num = tmp + (mt_chip->base_eint - data->eints);

			eint = &data->eints[eint_num];
		}

		pin->eint = eint;
		old_pin = eint ? eint->gpio : NULL;
		if (eint && !eint->gpio) {
			if (!(eint->flags & FL_EINT_MULTIMAP))
				eint->gpio = pin;
		} else if (eint) {
			eint->gpio = NULL;
			eint->flags |= FL_EINT_MULTIMAP;
		}
		if (old_pin) {
			int eint_id = eint ? eint->id : -1;
			pr_err("%s: multi-map:GPIO%d -> EINT%d\n",
				__func__, old_pin->id, eint_id);
		}
		if (pin->eint && (pin->eint->flags & FL_EINT_MULTIMAP))
			pr_err("%s: multi-map:GPIO%d -> EINT%d\n",
				__func__, pin->id, pin->eint->id);
	}

	/* linking pin group descriptions to pin groups */
	for (i = 0, grp = data->pin_drv_grps,
		 grp_desc = data->pin_drv_grp_descs;
		 i < data->n_pin_drv_grps; ++i, ++grp, ++grp_desc) {
		grp->desc = grp_desc;
		grp->cls = _find_drv_cls(grp->desc->drv_cls);
		grp->n_pins = 0;
		grp->pins = NULL;
	}

	/* link pins to drive groups, count pins in each group */
	for (i = 0, pin = data->gpio_pins, desc = data->gpio_pin_descs;
			i < ARRAY_SIZE(data->gpio_pins); ++i, ++pin, ++desc) {
		const char *grp_name = desc->drv_grp;
		if (!pin->drv_grp && grp_name) {
			struct mt_pin_def *pin2 = pin + 1;
			grp = _find_drv_grp(grp_name);
			pin->drv_grp = grp;
			if (!grp) {
				pr_err("Group not found: pin='%s'; drv_grp='%s'\n",
					pin->desc->pin, grp_name);
				continue;
			}
			grp->n_pins = 1;
			for (j = i + 1; j < data->n_pin_drv_grps; ++j, ++pin2) {
				const char *grp_name2 = pin2->desc->drv_grp;
				if (!pin2->drv_grp && grp_name2 &&
					!strcmp(grp_name2, grp_name)) {
					pin2->drv_grp = grp;
					grp->n_pins++;
				}
			}
		}
	}

	/* parsing groups again, linking groups to pins */
	for (i = 0, grp = data->pin_drv_grps,
		 grp_desc = data->pin_drv_grp_descs;
		 i < data->n_pin_drv_grps; ++i, ++grp, ++grp_desc) {
		if (grp->n_pins) {
			u16 grp_idx = 0;
			grp->pins = kmalloc(
				grp->n_pins * (sizeof(struct mt_pin_def *)),
				GFP_KERNEL);
			if (!grp->pins) {
				err = -ENOMEM;
				goto free_groups;
			}
			for (j = 0, pin = data->gpio_pins;
				 j < ARRAY_SIZE(data->gpio_pins); ++j, ++pin) {
				if (pin->drv_grp == grp) {
					grp->pins[grp_idx++] = pin;
					if (grp_idx == grp->n_pins)
						break;
				}
			}
			BUG_ON(grp_idx != grp->n_pins);
		}
	}

	pr_debug("%s: MTK pin data init done\n", __func__);
	return 0;

free_groups:
	while (i > 0) {
		grp--;
		i--;
		kfree(grp->pins);
	}
free_data:
	kfree(data->pin_drv_grps);
	pr_err("%s: Failed to initialize pin data: err=%d\n",
		__func__, err);
	return err;
}

static int _mt_pin_set_mode(u16 gpio, u8 mode)
{
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	struct mt_gpio_chip_def *mt_chip;
	unsigned long flags;
	u8 shf;
	u32 val, old;
	addr_t reg_addr;
	if (!pin) {
		pr_err("%s: invalid gpio%d\n", __func__, gpio);
		return -EINVAL;
	}
	if (pin->id == MT_INVALID_GPIO ||
		mode > 7 || !pin->desc->modes[mode]) {
		pr_err("%s: can't set mode: pin=%s.%d [%s], mode=%d\n",
			__func__, pin->mt_chip->desc->name, pin->id, pin->desc->pin, mode);
		return -EINVAL;
	}
	mt_chip = pin->mt_chip;
	reg_addr = mt_chip->hw_base[0] + pin->pinmux_offset;
	shf = pin->pinmux_shf;
	spin_lock_irqsave(&mt_chip->hw_lock, flags);
	mt_chip->read_reg(reg_addr, &val);
	old = val;
	val &= ~(7 << shf);
	val |= (mode << shf);
	mt_chip->write_reg(reg_addr, val);
	spin_unlock_irqrestore(&mt_chip->hw_lock, flags);
	return 0;
}

int mt_pin_get_dir(u16 gpio)
{
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	struct mt_gpio_chip_def *mt_chip;
	u32 val;
	addr_t reg_addr;
	u32 bit = 1 << (gpio & 0xF);

	if (!pin) {
		pr_err("%s: invalid gpio%d\n", __func__, gpio);
		return -EINVAL;
	}
	if (pin->id == MT_INVALID_GPIO) {
		pr_err("%s: can't get mode: pin=%s.%d [%s]\n",
			__func__, pin->mt_chip->desc->name, pin->id, pin->desc->pin);
		return -EINVAL;
	}
	mt_chip = pin->mt_chip;

	reg_addr = mt_chip->hw_base[0] + mt_chip->dir_offset
		+ (((gpio >> 4) & mt_chip->port_mask) << mt_chip->port_shf);
	mt_chip->read_reg(reg_addr, &val);
	return (val & bit) > 0 ? GPIOF_DIR_OUT : GPIOF_DIR_IN;
}
EXPORT_SYMBOL(mt_pin_get_dir);

int mt_pin_get_mode(u16 gpio)
{
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	struct mt_gpio_chip_def *mt_chip;
	u8 shf;
	u32 val;
	addr_t reg_addr;
	if (!pin) {
		pr_err("%s: invalid gpio%d\n", __func__, gpio);
		return -EINVAL;
	}
	if (pin->id == MT_INVALID_GPIO) {
		pr_err("%s: can't get mode: pin=%s.%d [%s]\n",
			__func__, pin->mt_chip->desc->name, pin->id, pin->desc->pin);
		return -EINVAL;
	}
	mt_chip = pin->mt_chip;
	reg_addr = mt_chip->hw_base[0] + pin->pinmux_offset;
	shf = pin->pinmux_shf;
	mt_chip->read_reg(reg_addr, &val);
	val = (val >> shf) & 7;
	return val;
}
EXPORT_SYMBOL(mt_pin_get_mode);

/* returns name fragment, that is convenient for match with the mode list */
static int _name_len(const char *name)
{
	int len;
	const char *p = name;
	if (!name)
		return 0;
	for (len = 0, p = name; isalnum(*p); ++p, ++len)
		continue;
	return len;
}

static int mt_pin_find_mode_main(u16 gpio)
{
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	const struct mt_pin_desc *desc;
	int pin_name_len;
	int mode;
	if (!pin || !pin->desc)
		return -EINVAL;
	desc = pin->desc;
	pin_name_len = _name_len(desc->pin);
	if (!pin_name_len)
		return -EINVAL;

	mode = _find_pin_mode(pin, desc->pin, pin_name_len);
	return mode >= 0 ? mode : -ENOENT;
}

int mt_pin_set_mode_main(u16 gpio)
{
	int mode = mt_pin_find_mode_main(gpio);
	return mode < 0 ? mode : _mt_pin_set_mode(gpio, mode);
}
EXPORT_SYMBOL(mt_pin_set_mode_main);

static int mt_pin_find_mode_clk(u16 gpio)
{
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	const struct mt_pin_desc *desc;
	int i = 0;
	if (!pin || !pin->desc)
		return -EINVAL;
	desc = pin->desc;
	for (i = 0; i < ARRAY_SIZE(desc->modes); ++i) {
		const char *s_mode = desc->modes[i];
		if (!s_mode)
			continue;
		if (strstr(s_mode, "_CLK") ||
			!strnicmp(s_mode, "CLK", 3) ||
			!strnicmp(s_mode + strlen(s_mode) - 3, "CLK", 3))
			return i;
	}
	return -ENOENT;
}

int mt_pin_set_mode_clk(u16 gpio)
{
	int mode = mt_pin_find_mode_clk(gpio);
	return mode < 0 ? mode : _mt_pin_set_mode(gpio, mode);
}

static int mt_pin_find_load_index(struct mt_pin_def *pin, u16 load_ma)
{
	struct mt_pin_drv_grp_def *grp = pin->drv_grp;
	int idx;
	if (!load_ma || !grp || !grp->cls)
		/*
		 * load must be strictly positive value in ma.
		 * check is necessary, because invalid
		 * entries in the class tables are zero-initialized.
		 */
		return -1;
	for (idx = 0; idx < ARRAY_SIZE(grp->cls->vals_ma); ++idx)
		if (load_ma == grp->cls->vals_ma[idx])
			return idx;
	return -1;
}

static int mt_pin_set_grp_load(
		struct mt_pin_def *pin, u16 load_ma, bool force)
{
	struct mt_pin_drv_grp_def *grp = pin->drv_grp;
	struct mt_gpio_chip_def *mt_chip = pin->mt_chip;
	unsigned long flags;
	addr_t reg = 0;
	u32 val;
	int idx = 0;
	u32 raw = 0;
	int err;
	if (!grp || !grp->n_pins) {
		pr_err("%s: %s:gpio#%d: no drive strength group\n",
			__func__, mt_chip->desc->name, pin->id);
		return -EINVAL;
	}
	if (load_ma > 0)
		idx = mt_pin_find_load_index(pin, load_ma);
	if (idx < 0) {
		pr_err("%s: %s:gpio#%d: grp-%s: load not supported: %d ma\n",
			__func__, mt_chip->desc->name, pin->id,
			grp->desc->name, load_ma);
		return -EINVAL;
	}
	spin_lock_irqsave(&mt_chip->hw_lock, flags);
	if ((grp->flags & MT_PIN_GRP_LOAD_CONF) &&
		 grp->load_ma != load_ma && !force) {
		pr_err("%s:%s:gpio%d: grp-%s: already configured with %d ma\n",
			__func__, mt_chip->desc->name, pin->id,
			grp->desc->name, grp->load_ma);
		err = -EBUSY;
		goto done_unlock;
	}
	reg = mt_chip->hw_base[grp->desc->type] +
		mt_chip->drv_offset + (grp->desc->port << mt_chip->port_shf);
	raw = (load_ma ? 0x08 : 0x0) | (idx & 0x7);
	err = mt_chip->read_reg(reg, &val);
	if (!err) {
		val &= ~(0xF << grp->desc->offset);
		/* 0x08 - ON/OFF */
		val |= raw << grp->desc->offset;
		err = mt_chip->write_reg(reg, val);
	}
	if (!err) {
		grp->raw = raw;
		grp->enabled = load_ma ? true : false;
		grp->load_ma = load_ma;
		grp->flags |= MT_PIN_GRP_LOAD_CONF;
	}
done_unlock:
	spin_unlock_irqrestore(&mt_chip->hw_lock, flags);
	pr_debug("%s: %s:gpio#%d: set load to %d ma [raw=0x%02X]: err=%d\n",
		__func__, mt_chip->desc->name, pin->id, load_ma, raw, err);
	return err;
}

static int mt_pin_get_grp_load(struct mt_pin_def *pin)
{
	struct mt_pin_drv_grp_def *grp = pin->drv_grp;
	struct mt_gpio_chip_def *mt_chip = pin->mt_chip;
	addr_t reg;
	u32 val = 0xF;
	u16 load_ma;
	u8 raw = 0;
	int err;
	if (!grp || !grp->n_pins) {
		pr_err("%s: %s:gpio#%d: no drive strength group\n",
			__func__, mt_chip->desc->name, pin->id);
		return -EINVAL;
	}
	reg = mt_chip->hw_base[grp->desc->type] +
		mt_chip->drv_offset + (grp->desc->port << mt_chip->port_shf);
	err = mt_chip->read_reg(reg, &val);
	if (!err) {
		raw = grp->raw = (val >> grp->desc->offset) & 0xF;
		grp->load_ma = grp->cls->vals_ma[raw & 0x7];
		grp->enabled = raw & 0x8 ? true : false;
	}
	load_ma = grp->enabled ? grp->load_ma : 0;
	pr_debug("%s: %s:gpio#%d: load is %d ma [raw=0x%02X]: err=%d\n",
		__func__, mt_chip->desc->name, pin->id, load_ma, raw, err);
	return err ? err : grp->load_ma;
}

int mt_pin_set_load(u16 gpio, u16 load_ma, bool force)
{
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	if (!pin)
		return -EINVAL;
	return mt_pin_set_grp_load(pin, load_ma, force);
}
EXPORT_SYMBOL(mt_pin_set_load);

int mt_pin_get_load(u16 gpio)
{
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	if (!pin)
		return -EINVAL;
	return mt_pin_get_grp_load(pin);
}
EXPORT_SYMBOL(mt_pin_get_load);

int mt_pin_set_pull(u16 gpio, u8 pull_mode)
{
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	struct mt_gpio_chip_def *mt_chip;
	unsigned long flags;
	int ret = 0;
	addr_t reg_base;
	u16 bit;
	if (!pin || pin->id == MT_INVALID_GPIO)
		return -EINVAL;
	bit = 1 << (pin->id & 0xF);
	mt_chip = pin->mt_chip;
	reg_base = mt_chip->hw_base[pin->type] +
		((pin->id >> 4) << mt_chip->port_shf);
	spin_lock_irqsave(&mt_chip->hw_lock, flags);
	switch (pull_mode) {
	case MT_PIN_PULL_DEFAULT:
		break;
	case MT_PIN_PULL_ENABLE_DOWN:
		mt_chip->write_reg(reg_base +
			mt_chip->pullen_offset + SET_REG(mt_chip), bit);
		mt_chip->write_reg(reg_base +
			mt_chip->pullsel_offset + CLR_REG(mt_chip), bit);
		break;
	case MT_PIN_PULL_ENABLE_UP:
		mt_chip->write_reg(reg_base +
			mt_chip->pullen_offset + SET_REG(mt_chip), bit);
		mt_chip->write_reg(reg_base +
			mt_chip->pullsel_offset + SET_REG(mt_chip), bit);
		break;
	case MT_PIN_PULL_UP:
		mt_chip->write_reg(reg_base +
			mt_chip->pullsel_offset + SET_REG(mt_chip), bit);
		break;
	case MT_PIN_PULL_DOWN:
		mt_chip->write_reg(reg_base +
			mt_chip->pullsel_offset + CLR_REG(mt_chip), bit);
		break;
	case MT_PIN_PULL_ENABLE:
		mt_chip->write_reg(reg_base +
			mt_chip->pullen_offset + SET_REG(mt_chip), bit);
		break;
	case MT_PIN_PULL_DISABLE:
		mt_chip->write_reg(reg_base +
			mt_chip->pullen_offset + CLR_REG(mt_chip), bit);
		break;
	default:
		ret = -EINVAL;
	}
	spin_unlock_irqrestore(&mt_chip->hw_lock, flags);
	return ret;
}
EXPORT_SYMBOL(mt_pin_set_pull);

int mt_pin_get_pull(u16 gpio, bool *pull_en, bool *pull_up)
{
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	struct mt_gpio_chip_def *mt_chip;
	unsigned long flags;
	int ret = 0;
	addr_t reg_base;
	u16 bit;
	u32 pull_en_val;
	u32 pull_sel_val;
	if (!pin || pin->id == MT_INVALID_GPIO)
		return -EINVAL;
	bit = 1 << (pin->id & 0xF);
	mt_chip = pin->mt_chip;
	reg_base = mt_chip->hw_base[pin->type] +
		((pin->id >> 4) << mt_chip->port_shf);
	spin_lock_irqsave(&mt_chip->hw_lock, flags);
	mt_chip->read_reg(reg_base +
		mt_chip->pullen_offset, &pull_en_val);
	mt_chip->read_reg(reg_base +
		mt_chip->pullsel_offset, &pull_sel_val);
	spin_unlock_irqrestore(&mt_chip->hw_lock, flags);
	if (pull_en)
		*pull_en = (pull_en_val & bit) != 0;
	if (pull_up)
		*pull_up = (pull_sel_val & bit) != 0;
	return ret;
}
EXPORT_SYMBOL(mt_pin_get_pull);

int mt_pin_set_mode_gpio(u16 gpio)
{
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	if (!pin)
		return -EINVAL;
	return _mt_pin_set_mode(gpio, pin->gpio_mode);
}
EXPORT_SYMBOL(mt_pin_set_mode_gpio);

int mt_pin_set_mode_eint(u16 gpio)
{
	int err;
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	/* does not make sense to set EINT mode for pin without pad */
	if (!pin || !pin->desc->pad)
		return -EINVAL;
	err = mt_pin_uses_eint(gpio);
	return err ? err : _mt_pin_set_mode(gpio, pin->eint_mode);
}
EXPORT_SYMBOL(mt_pin_set_mode_eint);

int mt_pin_set_mode_disabled(u16 gpio)
{
	return 0;
}
EXPORT_SYMBOL(mt_pin_set_mode_disabled);

int mt_pin_set_mode_by_name(u16 gpio, const char *pfx)
{
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	if (!pin)
		return -EINVAL;
	return _mt_pin_set_mode(gpio, _find_pin_mode(pin, pfx, strlen(pfx)));
}
EXPORT_SYMBOL(mt_pin_set_mode_by_name);

int mt_pin_set_mode(u16 gpio, u8 mode)
{
	switch (mode) {
	case MT_MODE_DEFAULT:
		return 0;
	case MT_MODE_0...MT_MODE_7:
		return _mt_pin_set_mode(gpio, mode - MT_MODE_0);
	case MT_MODE_MAIN:
		return mt_pin_set_mode_main(gpio);
	case MT_MODE_GPIO:
		return mt_pin_set_mode_gpio(gpio);
	case MT_MODE_EINT:
		return mt_pin_set_mode_eint(gpio);
	case MT_MODE_CLK:
		return mt_pin_set_mode_clk(gpio);
	case MT_MODE_DISABLED:
		return mt_pin_set_mode_disabled(gpio);
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL(mt_pin_set_mode);

int mt_pin_find_mode(u16 gpio, u8 mode)
{
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	if (!pin)
		return -EINVAL;
	switch (mode) {
	case MT_MODE_0...MT_MODE_7:
		return mode - MT_MODE_0;
	case MT_MODE_GPIO:
		return pin->gpio_mode != MT_INVALID_MODE ? pin->gpio_mode : -ENODEV;
	case MT_MODE_EINT:
		return pin->eint_mode != MT_INVALID_MODE ? pin->eint_mode : -ENODEV;
	case MT_MODE_MAIN:
		return mt_pin_find_mode_main(gpio);
	case MT_MODE_CLK:
		return mt_pin_find_mode_clk(gpio);
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL(mt_pin_find_mode);

int mt_pin_get_din(u16 gpio)
{
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	struct mt_gpio_chip_def *mt_chip;
	u32 val;
	addr_t reg_addr;
	u32 bit = 1 << (gpio & 0xF);

	if (!pin) {
		pr_err("%s: invalid gpio%d\n", __func__, gpio);
		return -EINVAL;
	}
	if (pin->id == MT_INVALID_GPIO) {
		pr_err("%s: can't get din: pin=%s.%d [%s]\n",
			__func__, pin->mt_chip->desc->name, pin->id, pin->desc->pin);
		return -EINVAL;
	}

	mt_chip = pin->mt_chip;
	reg_addr = mt_chip->hw_base[0] +
		mt_chip->din_offset + (((gpio >> 4) & mt_chip->port_mask) << mt_chip->port_shf);
	mt_chip->read_reg(reg_addr, &val);
	return (val & bit) ? 1 : 0;
}
EXPORT_SYMBOL(mt_pin_get_din);

int mt_pin_get_dout(	u16 gpio)
{
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	struct mt_gpio_chip_def *mt_chip;
	u32 val;
	addr_t reg_addr;
	u32 bit = 1 << (gpio & 0xF);

	if (!pin) {
		pr_err("%s: invalid gpio%d\n", __func__, gpio);
		return -EINVAL;
	}
	if (pin->id == MT_INVALID_GPIO) {
		pr_err("%s: can't get dout: pin=%s.%d [%s]\n",
			__func__, pin->mt_chip->desc->name, pin->id, pin->desc->pin);
		return -EINVAL;
	}

	mt_chip = pin->mt_chip;
	reg_addr = mt_chip->hw_base[0] +
		mt_chip->dout_offset + (((gpio >> 4) & mt_chip->port_mask) << mt_chip->port_shf);
	mt_chip->read_reg(reg_addr, &val);
	return (val & bit) ? 1 : 0;
}
EXPORT_SYMBOL(mt_pin_get_dout);

static struct platform_device mtk_dev[] = {
	{
		.name = "mt8135-gpio",
		.dev  = {
			.platform_data =
				&mt_gpio_platform_data_desc.gpio_chips[0],
		},
	},
	{
		.name = "mt6397-gpio",
		.dev  = {
			.platform_data =
				&mt_gpio_platform_data_desc.gpio_chips[1],
		},
	},
	{
		.name = "mt-gpio",
	},
};

/*
 * Perform platform-specific setup to support pin control features.
 * Pin control features include: GPIO, GPIO IRQ, PINMUX, PHY features.
 * PHY features include: PU/PD, OS/OD, impedance, delays, etc.
 * This code is typically called from board_init(), arch(3) phase.
 * GPIO driver is already available and GPIO devices will be created
 * immediately upon registration.
 * Name suggests that this call is to eventually be integrated
 * with Linux pinctrl API.
 */
int __init mt_pinctrl_init(struct mt_pinctrl_cfg *cfg, struct eint_domain_config *eint_cfg)
{
	int i, err;

	err = mt_pinctrl_data_init();
	if (err) {
		pr_err(
			"%s: Failed to parse pin data: err=%d\n",
			__func__, err);
		goto fail;
	}

	/* TODO: replace with PWRAP platform device */
	err = mt_pwrap_init();
	if (err) {
		pr_err(
			"%s: Failed to init MTK wrapper bus: err=%d\n",
			__func__, err);
		goto fail;
	}

	for (i = 0; i < ARRAY_SIZE(mtk_dev); ++i) {
		err = platform_device_register(mtk_dev+i);
		if (err) {
			pr_err(
				"%s: Failed to register '%s': err=%d\n",
				__func__, mtk_dev[i].name, err);
			goto fail;
		}
	}

	err = mt_eint_init(eint_cfg);
	if (err) {
		pr_err(
			"%s: Failed to init EINT subsystem: err=%d\n",
			__func__, err);
		goto fail;
	}

	if (cfg) {
		bool valid;
		do {
			valid = false;
			if (cfg->gpio)
				valid = true;
			if (cfg->load_ma) {
				mt_pin_set_load(cfg->gpio, cfg->load_ma, false);
				valid = true;
			}
			if (cfg->pull != MT_PIN_PULL_DEFAULT) {
				mt_pin_set_pull(cfg->gpio, cfg->pull);
				valid = true;
			}
			if (cfg->mode_name) {
				mt_pin_set_mode_by_name(cfg->gpio, cfg->mode_name);
				valid = true;
			} else if (cfg->mode || valid) {
				if (cfg->mode != MT_MODE_DEFAULT)
					mt_pin_set_mode(cfg->gpio, cfg->mode);
				valid = true;
			}
			cfg++;
		} while (valid);
	}
	/* ARIEL-1257: set default drive strength for all pins to 4ma */
	for (i = 0; i < data->n_pin_drv_grps; ++i) {
		struct mt_pin_drv_grp_def *grp = &data->pin_drv_grps[i];
		mt_pin_set_grp_load(grp->pins[0], 4, false);
	}
fail:
	return err;
}

static int mt_pin_config_to_hw_config(
	u16 gpio, struct mt_pin_hw_config *hw_conf,
	const struct mt_pin_config *conf)
{
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	int hw_load_idx;
	int hw_mode_idx;
	struct mt_pin_hw_config hw;
	BUG();
	if (!pin || !conf || !hw_conf)
		return -EINVAL;
	hw_load_idx = mt_pin_find_load_index(pin, conf->load);
	hw_mode_idx = mt_pin_find_mode(gpio, conf->mode);
	hw.w = 0;
	if (hw_load_idx >= 0) {
		hw.hw_load_valid = true;
		hw.hw_load = 0x8 | hw_load_idx;
	}
	if (hw_mode_idx >= 0) {
		hw.hw_mode_valid = true;
		hw.hw_mode = hw_load_idx;
	}
	switch (conf->pull) {
	case MT_PIN_PULL_DISABLE:
		hw.pull_en_valid = true;
		hw.pull_en = false;
		break;
	case MT_PIN_PULL_ENABLE:
		hw.pull_en_valid = true;
		hw.pull_en = true;
		break;
	case MT_PIN_PULL_ENABLE_UP:
		hw.pull_en_valid = true;
		hw.pull_en = true;
		hw.pull_sel_valid = true;
		hw.pull_sel = true;
		break;
	case MT_PIN_PULL_ENABLE_DOWN:
		hw.pull_en_valid = true;
		hw.pull_en = true;
		hw.pull_sel_valid = true;
		hw.pull_sel = false;
		break;
	case MT_PIN_PULL_UP:
		hw.pull_sel_valid = true;
		hw.pull_sel = true;
		break;
	case MT_PIN_PULL_DOWN:
		hw.pull_sel_valid = true;
		hw.pull_sel = false;
		break;
	}
	hw_conf->w = hw.w;
	return 0;
}

int mt_gpio_to_eint(u16 gpio)
{
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	if (!pin || !pin->eint)
		return -EINVAL;
	return pin->eint->id;
}
EXPORT_SYMBOL(mt_gpio_to_eint);

int mt_eint_to_gpio(u16 eint)
{
	struct mt_eint_def *pin = mt_get_eint_def(eint);
	if (!pin || !pin->gpio)
		return -EINVAL;
	return pin->gpio - data->gpio_pins;
}
EXPORT_SYMBOL(mt_eint_to_gpio);

int mt_pin_config_off_mode(u16 gpio, struct mt_pin_config *conf)
{
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	if (!pin || !conf)
		return -EINVAL;
	mt_pin_config_to_hw_config(gpio, &pin->off_conf, conf);
	return 0;
}
EXPORT_SYMBOL(mt_pin_config_off_mode);

int mt_pin_config_mode(u16 gpio, struct mt_pin_config *conf)
{
	struct mt_pin_def *pin = mt_get_pin_def(gpio);
	if (!pin || !conf)
		return -EINVAL;
	mt_pin_config_to_hw_config(gpio, &pin->conf, conf);
	return 0;
}
EXPORT_SYMBOL(mt_pin_config_mode);
