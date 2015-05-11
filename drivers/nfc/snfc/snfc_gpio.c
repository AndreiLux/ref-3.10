/*
* snfc_gpio.c
*
*/

/*
 *    INCLUDE FILES FOR MODULE
 */

#include "snfc_gpio.h"
#include <linux/gpio.h>

/*
* Description :
* Input :
* Output :
*/
void snfc_gpio_write(int gpionum, int value)
{
    gpio_set_value(gpionum, value);
}

/*
* Description :
* Input :
* Output :
*/
int snfc_gpio_read(int gpionum)
{
    return gpio_get_value(gpionum);
}
