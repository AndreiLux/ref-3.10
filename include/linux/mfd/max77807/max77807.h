/*
 * MAX77807 Driver Core
 *
 * Copyright (C) 2014 Maxim Integrated
 * TaiEup Kim <clark.kim@maximintegrated.com>
 *
 * Copyright and License statement to be determined with Customer.
 * GNU Public License version 2 requires software code to be
 * publically open source if the code is to be statically linked with
 * the Linux kernel binary object.
 */

#ifndef __MAX77807_CORE_H__
#define __MAX77807_CORE_H__

#include <linux/regmap.h>
#ifdef CONFIG_USB_ODIN_DRD
#include <linux/usb/odin_usb3.h>
#endif

#define CONFIG_MAX77807_USBSW

/* MAX77807 Top Devices */
#define MAX77807_NAME                      "max77807"
#define MAX77807_PMIC_NAME                 MAX77807_NAME"-pmic"
#define MAX77807_PERIPH_NAME               MAX77807_NAME"-periph"
#ifdef CONFIG_MAX77807_USBSW
#define MAX77807_MUIC_NAME					MAX77807_NAME"-muic"
#endif
#define MAX77807_FUELGAUGE_NAME            MAX77807_NAME"-fuelgauge"

/* MAX77807 PMIC Devices */
#define MAX77807_CHARGER_NAME              MAX77807_NAME"-charger"
#define MAX77807_SFO_NAME                  MAX77807_NAME"-sfo"

/* MAX77807 Periph Devices */
#define MAX77807_MOTOR_NAME                MAX77807_NAME"-motor"

#ifdef CONFIG_MAX77807_USBSW
/* MAX77807 MUIC Devices */
#define MAX77807_USBSW_NAME                	MAX77807_NAME"-usbsw"
#endif
/* Chip Interrupts */
enum {
    MAX77807_IRQ_CHGR = 0,
    MAX77807_IRQ_TOP,
#ifdef CONFIG_MAX77807_USBSW
	MAX77807_IRQ_USBSW,
#endif
    /***/
    MAX77807_IRQ_NUM_OF_INTS,
};

/*******************************************************************************
 * Useful Macros
 ******************************************************************************/

#undef  __CONST_FFS
#define __CONST_FFS(_x) \
        ((_x) & 0x0F ? ((_x) & 0x03 ? ((_x) & 0x01 ? 0 : 1) :\
                                      ((_x) & 0x04 ? 2 : 3)) :\
                       ((_x) & 0x30 ? ((_x) & 0x10 ? 4 : 5) :\
                                      ((_x) & 0x40 ? 6 : 7)))

#undef  FFS
#if 0
#define FFS(_x) \
        ((_x) ? (__builtin_constant_p(_x) ? __CONST_FFS(_x) : __ffs(_x)) : 0)
#else
#define FFS(_x) \
        ((_x) ? __CONST_FFS(_x) : 0)
#endif

#undef  BIT_RSVD
#define BIT_RSVD  0

#undef  BITS
#define BITS(_end, _start) \
        ((BIT(_end) - BIT(_start)) + BIT(_end))

#undef  __BITS_GET
#define __BITS_GET(_word, _mask, _shift) \
        (((_word) & (_mask)) >> (_shift))

#undef  BITS_GET
#define BITS_GET(_word, _bit) \
        __BITS_GET(_word, _bit, FFS(_bit))

#undef  __BITS_SET
#define __BITS_SET(_word, _mask, _shift, _val) \
        (((_word) & ~(_mask)) | (((_val) << (_shift)) & (_mask)))

#undef  BITS_SET
#define BITS_SET(_word, _bit, _val) \
        __BITS_SET(_word, _bit, FFS(_bit), _val)

#undef  BITS_MATCH
#define BITS_MATCH(_word, _bit) \
        (((_word) & (_bit)) == (_bit))

/*******************************************************************************
 * Sub Modules Support
 ******************************************************************************/

struct max77807_io {
    struct regmap *regmap;
};

struct max77807_dev {
    struct max77807_core        *core;
    int                          dev_id;
    void                        *pdata;
    struct mutex                 lock;
    struct device               *dev;
    struct max77807_io           io;
    struct kobject              *kobj;
    struct attribute_group      *attr_grp;
    struct regmap_irq_chip_data *irq_data;
	struct regmap               *regmap;
    int                          irq;
    int                          irq_gpio;
    struct irq_domain           *irq_domain;
};



extern struct max77807_io *max77807_get_io (struct max77807_dev *chip);

/*******************************************************************************
 * Platform Data
 ******************************************************************************/

struct max77807_pmic_platform_data {
    int irq; /* system interrupt number for PMIC */
};

#if 0
struct max77807_periph_platform_data {
};
#endif


/*******************************************************************************
 * Chip IO
 ******************************************************************************/

static __always_inline int max77807_read (struct max77807_io *io,
    u8 addr, u8 *val)
{
    unsigned int buf = 0;
    int rc = regmap_read(io->regmap, (unsigned int)addr, &buf);

    if (likely(!IS_ERR_VALUE(rc))) {
        *val = (u8)buf;
    }
    return rc;
}

static __always_inline int max77807_write (struct max77807_io *io,
    u8 addr, u8 val)
{
    unsigned int buf = (unsigned int)val;
    return regmap_write(io->regmap, (unsigned int)addr, buf);
}

static __inline int max77807_masked_read (struct max77807_io *io,
    u8 addr, u8 mask, u8 shift, u8 *val)
{
    u8 buf = 0;
    int rc;

    if (unlikely(!mask)) {
        /* no actual access */
        *val = 0;
        rc   = 0;
        goto out;
    }

    rc = max77807_read(io, addr, &buf);
    if (likely(!IS_ERR_VALUE(rc))) {
        *val = __BITS_GET(buf, mask, shift);
    }

out:
    return rc;
}

static __inline int max77807_masked_write (struct max77807_io *io,
    u8 addr, u8 mask, u8 shift, u8 val)
{
    u8 buf = 0;
    int rc;

    if (unlikely(!mask)) {
        /* no actual access */
        rc = 0;
        goto out;
    }

    rc = max77807_read(io, addr, &buf);
    if (likely(!IS_ERR_VALUE(rc))) {
        rc = max77807_write(io, addr, __BITS_SET(buf, mask, shift, val));
    }

out:
    return rc;
}

static __always_inline int max77807_bulk_read (struct max77807_io *io,
    u8 addr, u8 *dst, u16 len)
{
    return regmap_bulk_read(io->regmap, (unsigned int)addr, dst, (size_t)len);
}

static __always_inline int max77807_bulk_write (struct max77807_io *io,
    u8 addr, const u8 *src, u16 len)
{
    return regmap_bulk_write(io->regmap, (unsigned int)addr, src, (size_t)len);
}

#ifdef CONFIG_MAX77807_USBSW
/* define USBSW register for VB interrupt */
#define REG_USBSW_ID					0x00
#define REG_USBSW_INT2					0x02
#define REG_USBSW_INTMASK2				0x08
#define BIT_INT2_VBVOLT					BIT (4)

#define REG_USBSW_STATUS2				0x05
#define BIT_STATUS2_VBVOLT				BIT (6)
#endif

extern int max77807_usbsw_read(int addr,u8 * val);
extern int max77807_usbsw_write(int addr,u8 val);

/*** Simplifying bitwise configurations for individual subdevices drivers ***/
#ifndef MAX77807_REG_ADDR_INVALID
#define MAX77807_REG_ADDR_INVALID  0x00
#endif

struct max77807_bitdesc {
    u8 reg, mask, shift;
};

#define MAX77807_BITDESC(_reg, _bit) \
        { .reg = _reg, .mask = _bit, .shift = (u8)FFS(_bit), }

#define MAX77807_BITDESC_INVALID \
        MAX77807_BITDESC(MAX77807_REG_ADDR_INVALID, BIT_RSVD)

#define MAX77807_BITDESC_PER_BIT(_reg, _index, _per, _bitsz) \
        MAX77807_BITDESC((_reg) + ((_index) / (_per)),\
            BITS((_bitsz) - 1, 0) << (((_index) % (_per)) * (_bitsz)))

#define MAX77807_BITDESC_PER_1BIT(_reg, _index) \
        MAX77807_BITDESC_PER_BIT(_reg, _index, 8, 1)
#define MAX77807_BITDESC_PER_2BIT(_reg, _index) \
        MAX77807_BITDESC_PER_BIT(_reg, _index, 4, 2)
#define MAX77807_BITDESC_PER_4BIT(_reg, _index) \
        MAX77807_BITDESC_PER_BIT(_reg, _index, 2, 4)

#define is_valid_max77807_bitdesc(_bitdesc) \
        ((_bitdesc)->mask != BIT_RSVD &&\
         (_bitdesc)->reg  != MAX77807_REG_ADDR_INVALID)

static __always_inline int max77807_read_bitdesc (struct max77807_io *io,
    const struct max77807_bitdesc *desc, u8 *val)
{
    return max77807_masked_read(io, desc->reg, desc->mask, desc->shift, val);
}

static __always_inline int max77807_write_bitdesc (struct max77807_io *io,
    const struct max77807_bitdesc *desc, u8 val)
{
    return max77807_masked_write(io, desc->reg, desc->mask, desc->shift, val);
}

#define max77807_read_reg_bit(_io, _reg, _bit, _val_ptr) \
        max77807_masked_read(_io, _reg, _reg##_##_bit,\
            (u8)FFS(_reg##_##_bit), _val_ptr)

#define max77807_write_reg_bit(_io, _reg, _bit, _val) \
        max77807_masked_write(_io, _reg, _reg##_##_bit,\
            (u8)FFS(_reg##_##_bit), _val)

/*******************************************************************************
 * Debugging Stuff
 ******************************************************************************/

#undef  log_fmt
#define log_fmt(format) \
        DRIVER_NAME ": " format
#undef  log_err
#define log_err(format, ...) \
        printk(KERN_ERR log_fmt(format), ##__VA_ARGS__)
#undef  log_warn
#define log_warn(format, ...) \
        printk(KERN_WARNING log_fmt(format), ##__VA_ARGS__)
#undef  log_info
#define log_info(format, ...) \
        if (likely(log_level >= 0)) {\
            printk(KERN_INFO log_fmt(format), ##__VA_ARGS__);\
        }
#undef  log_dbg
#define log_dbg(format, ...) \
        if (likely(log_level >= 1)) {\
            printk(KERN_DEFAULT log_fmt(format), ##__VA_ARGS__);\
        }
#undef  log_vdbg
#define log_vdbg(format, ...) \
        if (likely(log_level >= 2)) {\
            printk(KERN_DEFAULT log_fmt(format), ##__VA_ARGS__);\
        }

#ifdef CONFIG_USB_ODIN_DRD
void max77807_otg_vbus_onoff(struct odin_usb3_otg *odin_otg, int onoff);
bool max77807_drd_charger_init_detect(struct odin_usb3_otg *odin_otg);
#endif

#endif /* !__MAX77807_CORE_H__ */

