/*
* snfc_gpio.h
*
*/


#ifndef __SNFC_GPIO_H__
#define __SNFC_GPIO_H__


/*
 *  INCLUDE FILES FOR MODULE
 */
#include <linux/gpio.h>
#include "snfc_common.h"
/*DEFINE*/

enum{
  GPIO_DIRECTION_IN = 0,
  GPIO_DIRECTION_OUT,
};

enum{
  GPIO_LOW_VALUE = 0,
  GPIO_HIGH_VALUE,
};

enum{
  GPIO_CONFIG_ENABLE = 0,
  GPIO_CONFIG_DISABLE,
};


struct snfc_gp {
     unsigned int gpio_hsel;
     unsigned int gpio_pon;
     unsigned int gpio_hvdd;
     unsigned int gpio_intu;
     unsigned int gpio_rfs;
     unsigned int gpio_uicc_con;
};

/* snfc_pon */
#define GPIO_SNFC_PON       74      //IMA_PON

/* snfc_rfs */
#define GPIO_SNFC_RFS       82      //IMA_CDET

/* snfc_int */
#define GPIO_SNFC_INT       46      //IMA_INT

/* snfc_lockcont */
#define GPIO_SNFC_INTU      59      //IMA_INTU

/* snfc_hsel */
#define GPIO_SNFC_HSEL      94      //NFC_HSEL

/* snfc_hvdd */
#define GPIO_SNFC_HVDD      145     //NFC_HVDD

#define SNFC_GPIO_CFG(gpio, func, dir, pull, drvstr) \
    ((((gpio) & 0x3FF) << 4)        |   \
    ((func) & 0xf)                  |   \
    (((dir) & 0x1) << 14)           |   \
    (((pull) & 0x3) << 15)          |   \
    (((drvstr) & 0xF) << 17))

extern int gpio_rfs;

/*
 *  FUNCTION PROTOTYPE
 */
void snfc_gpio_write(int gpionum, int value);
int snfc_gpio_read(int gpionum);

#endif  //__SNFC_GPIO_H__
