/*
 * LCD Tuning Lowlevel Control Abstraction
 *
 */

#ifndef _LINUX_LCD_TUNING_H
#define _LINUX_LCD_TUNING_H

#include <linux/miscdevice.h>
#include <linux/mutex.h>

struct lcd_tuning_dev;

enum lcd_type
{
    OLED,
    TFT,
};

enum lcd_gamma
{
    GAMMA25,
    GAMMA22,
    GAMMA19,
};

/* BEGIN: Added by huohua, 2012/02/14   PN:DTS2012021403292 */
enum tft_cabc
{
    CABC_UI = 0,
    CABC_VID,
    CABC_OFF
};
/* END:   Added by huohua, 2012/02/14 */

struct lcd_properities {
    enum lcd_type type;
    enum lcd_gamma default_gamma;
};

struct lcd_tuning_ops {
    int (*set_gamma)(struct lcd_tuning_dev *ltd, enum lcd_gamma gamma);
    /* BEGIN: Added by huohua, 2012/02/14   PN:DTS2012021403292 */
    int (*set_cabc)(struct lcd_tuning_dev *ltc, enum tft_cabc cabc);
    /* END:   Added by huohua, 2012/02/14 */

    int (*set_color_temperature)(struct lcd_tuning_dev *ltc, unsigned int ctValue[]);
};

struct lcd_tuning_dev {
    struct lcd_properities props;
    const struct lcd_tuning_ops *ops;
    void *data;
};

struct lcd_mdevice {
    struct lcd_tuning_dev ltd;
    /* This protects the 'ops' field. If 'ops' is NULL, the driver that
       registered this device has been unloaded, and if class_get_devdata()
       points to something in the body of that driver, it is also invalid. */
    struct mutex ops_lock;
    struct miscdevice mdev;
};

//FIXME
#if 0
static inline void * lcd_tuning_get_data(struct lcd_tuning_device *ltd)
{
    return dev_get_drvdata(&ltd->dev);
}
#endif
struct lcd_tuning_dev *lcd_tuning_dev_register(struct lcd_properities *props, const struct lcd_tuning_ops *lcd_ops, void *devdata);

void lcd_tuning_dev_unregister(struct lcd_tuning_dev *ltd);

#endif
