#ifndef __ARCH_MT_GPIO_DEF_H
#define __ARCH_MT_GPIO_DEF_H

#include <linux/gpio.h>
#include <mach/mt_platform.h>
#include <mach/eint.h>

struct mt_pinctrl_cfg;
struct mt_eint_chip;
struct mt_pin_def;

#define SET_REG(chip) ((chip)->port_align)
#define CLR_REG(chip) (((chip)->port_align)<<1)

enum {
	MT_PIN_PULL_DEFAULT,
	MT_PIN_PULL_DISABLE,
	MT_PIN_PULL_DOWN,
	MT_PIN_PULL_UP,
	MT_PIN_PULL_ENABLE,
	MT_PIN_PULL_ENABLE_DOWN,
	MT_PIN_PULL_ENABLE_UP,
};

enum mt_pinctrl_mode {
	MT_MODE_0 = 0,
	MT_MODE_1 = 1,
	MT_MODE_2 = 2,
	MT_MODE_3 = 3,
	MT_MODE_4 = 4,
	MT_MODE_5 = 5,
	MT_MODE_6 = 6,
	MT_MODE_7 = 7,
	MT_MODE_GPIO = 8,
	MT_MODE_EINT = 9,
	MT_MODE_MAIN = 10,
	MT_MODE_CLK = 11,
	MT_MODE_USER = 12,
	MT_MODE_OFF = 13,
	MT_MODE_DISABLED = 14,
	MT_MODE_DEFAULT = 15,
};

/*
 *  GPIO: pin groups (DRV - drive strength, DSEL - Tx/Rx delays)
 */
struct mt_pingrp_data {
	/* drive strength control */
	addr_t drv_base;
	u32	drv_mask;
	/* signal delays control */
	addr_t dsel_base;
	u32	tdsel_mask;
	u32	rdsel_mask;
};

/*
 *  GPIO: basic IO
 */
struct mt_gpio_data {
	addr_t	base;	/* virtual addr of 1-st reg */
	u16	start;		/* GPIO# of the first pin on device */
	u16	pins;		/* total GPIO pins supported by device */

	u32 flags;		/* mt_platform_flags */
};

struct mt_eint_chip {
	addr_t	base_addr;
	u16	base_irq;	/* irq_desc[] index of 1-st supported irq */
	u16 stat;
	u16 ack;
	u16 mask;
	u16 mask_set;
	u16 mask_clr;
	u16 sens;
	u16 sens_set;
	u16 sens_clr;
	u16 pol;
	u16 pol_set;
	u16 pol_clr;
	u8	base_eint;
	u8	nirqs;
	u8  port_mask;
	u8  ports;
	char *name;
	unsigned long *wake_mask;
	unsigned long *cur_mask;
	struct irq_domain *domain;
	bool suspended:1;
};

struct mt_pin_desc {
	char *pin;
	char *pad;
	char *drv_grp;
	char *chip;
	char *modes[8];
/*
  char *clk_domain;
  char *pwr_domain;
*/
};

struct mt_pin_drv_cls {
	char *name;
	u8   vals_ma[8];
};

struct mt_pin_drv_grp_desc {
	char *name;
	char *drv_cls;
	u8	port;
	u8	offset;
	u8	type;
	u8	reserved;
};

/**
 * struct mt_gpio_chip_desc - platform-specific data that describes MTK gpio chip properties
 * @name  : chip name in upper case
 * @flags : bit combination from mt_chip_flags enum
 */
struct mt_gpio_chip_desc {
	char *name;
	u32 flags;
};

/**
 * struct mt_gpio_chip_def - platform-specific data that describes MTK gpio chip properties
 * @desc        : pointer to chip descriptor
 * @base_pin    : pointer to first pin definition for this chip
 * @base_eint   : pointer to first eint definition for this chip
 * @n_gpio      : number of pin definitions for this chip
 * @n_eint      : number of eint definitions for this chip
 * @parent      : pointer to chip that is logically "before" this chip
 */
struct mt_gpio_chip_def {
	spinlock_t hw_lock; /* to protect pinmux, dsel, drv reg accesses */
	struct mt_gpio_chip_desc *desc;
	struct mt_pin_def *base_pin;
	struct mt_eint_def *base_eint;
	struct mt_gpio_chip_def *parent;
	addr_t hw_base[2]; /* selected by pin type */

	u16 n_gpio;
	u16 n_eint;

	u16 type1_start; /* first pin */
	u16 type1_end; /* last pin + 1 */

	/* all offsets are from chip base addr (either base or base2) */
	u16 pullen_offset;	/* get/set/clr */
	u16 pullsel_offset;	/* get/set/clr */

	u16 dir_offset;		/* get/set/clr */
	u16 din_offset;		/* get/set/clr */

	u16 dout_offset;	/* get/set/clr */
	u16 pinmux_offset;	/* read/modify/write */

	u16 drv_offset;		/* read/modify/write */
	u16 dsel_offset;	/* read/modify/write */

	u8	port_shf;		/* 3 for MT6397, 4 for MT8135 */
	u8  port_align;		/* 2 for MT6397, 4 for MT8135 */
	u8	port_mask;		/* 0x3 for MT6397, 0xF for MT8135 */
	u8	reserved;

	int (*write_reg)(addr_t, u32);
	int (*read_reg)(addr_t, u32 *);
};

struct mt_pin_drv_grp_def;

#define MT_INVALID_GPIO ((u16)-1)
#define MT_INVALID_EINT ((u16)-1)
#define MT_INVALID_MODE ((u8)-1)

enum EINT_FLAGS {
	FL_EINT_NONE	 = (0 << 0),
	FL_EINT_MULTIMAP = (1 << 0),
};

struct mt_eint_def;

struct mt_pin_hw_config {
	union {
		struct {
		u16 hw_mode:3;
		u16 hw_mode_valid:1;
		u16 pull_en:1;
		u16 pull_en_valid:1;
		u16 pull_sel:1;
		u16 pull_sel_valid:1;
		u16 hw_load:4;
		u16 hw_load_valid:1;
		u16 _reserved:3;
		};
		u16 w;
	};
};

struct mt_pin_config {
	union {
		struct {
		u16 mode:4;
		u16 pull:3;
		u16 disabled:1;
		u16 load:8;
		};
		u16 w;
	};
};

struct mt_pin_def {
	const struct mt_pin_desc	*desc;
	struct mt_gpio_chip_def		*mt_chip;
	struct mt_pin_drv_grp_def	*drv_grp;

	struct mt_pin_hw_config conf;
	struct mt_pin_hw_config off_conf;

	u32 flags;

	u16 pinmux_offset; /* pre-calced to avoid /5 operation */
	/* gpio id (unnecessary, but this is additional integrity check) */
	u16 id;

	u8  type; /* selects mt_chip->hw_base[] addr for pull/ies */
	u8	eint_mode; /* cached, to avoid array search at run time */
	u8	gpio_mode; /* cached, to avoid array search at run time */
	u8  pinmux_shf;	/* pre-calced shift, to avoid %5 operation */

	struct mt_eint_def   *eint;
};

struct mt_pin_drv_grp_def {
	const struct mt_pin_drv_grp_desc *desc;
	const struct mt_pin_drv_cls *cls;
	struct mt_pin_def **pins;
	u16	n_pins;
	u8 load_ma;
	u8 raw;
	bool enabled;

	#define MT_PIN_GRP_LOAD_CONF	(MT_DRIVER_FIRST)

	u32 flags;
};

struct mt_eint_def {
	int id;
	struct mt_eint_chip *chip;
	struct mt_pin_def *gpio;
	u32 reg;
	u32 flags;
};

struct mt_pinctrl_cfg {
	u16 gpio;
	u8  load_ma;
	u8  pull:3;
	u8  mode:4;
	u8  disabled:1;
	u8  *mode_name;
};

struct mt_drv_grp_conf {
	struct mt_drv_grp *grp;
	u32 attrs;
	u32 val;
};

struct mt_pin_info {
	union {
		struct {
			s16 pin;
			u8 flags;
			u8 mode1:3;
			u8 mode2:3;
			u8 valid:1;
			u8 custom:1;
		};
		u32 w;
	};
};

struct mtk_uart_data {
	struct mt_pin_info rxd;
	struct mt_pin_info txd;
};

#define MTK_GPIO(name) \
{ \
	.pin = ((GPIO_##name) & (~0x80000000)), \
	.valid = 1, \
}

#define MTK_GPIO_PIN(name) MTK_GPIO(name##_PIN)

#define MTK_EINT_PIN(name, type) \
{ \
	.pin = ((GPIO_##name##_PIN) & (~0x80000000)), \
	.flags = (type), \
	.valid = 1, \
}

int mt_gpio_to_eint(u16 gpio);
int mt_eint_to_gpio(u16 eint);

int mt_pinctrl_init(struct mt_pinctrl_cfg *cfg, struct eint_domain_config *eint_cfg);
struct mt_pin_def *mt_get_pin_def(u16 pin);
struct mt_eint_def *mt_get_eint_def(u16 eint);
int mt_pwrap_init(void);

int mt_pin_find_mode(u16 gpio, u8 mode);
int mt_pin_set_mode(u16 gpio, u8 mode);
int mt_pin_set_mode_by_name(u16 gpio, const char *pfx);
int mt_pin_set_mode_gpio(u16 gpio);
int mt_pin_set_mode_eint(u16 gpio);
int mt_pin_set_mode_main(u16 gpio);
int mt_pin_set_mode_clk(u16 gpio);
int mt_pin_get_dir(u16 gpio);
int mt_pin_get_mode(u16 gpio);
int mt_pin_set_pull(u16 gpio, u8 pull_mode);
int mt_pin_get_pull(u16 gpio, bool *pull_en, bool *pull_up);
int mt_pin_uses_eint(u16 gpio);
int mt_pin_set_load(u16 gpio, u16 load_ma, bool force);
int mt_pin_get_load(u16 gpio);
int mt_pin_get_din(u16 gpio);
int mt_pin_get_dout(u16 gpio);

int mt_pin_find_by_pin_name(int start_pin, const char *pin_name, const char *chip_name);
int mt_pin_config_mode(u16 gpio, struct mt_pin_config *conf);
int mt_pin_config_off_mode(u16 gpio, struct mt_pin_config *conf);

#endif /* __ARCH_MT_GPIO_DEF_H */
