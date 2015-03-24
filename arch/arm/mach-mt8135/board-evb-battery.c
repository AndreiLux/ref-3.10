
#include <linux/pm.h>
#include <linux/of_platform.h>
#include <asm/mach-types.h>
#include <mach/board.h>
#include <mach/battery_custom_data.h>
#include <mach/charging.h>
#include <linux/i2c.h>
#include <mach/mt_gpio.h>
#include <mt8135_evbp1_v2/cust_gpio_usage.h>

static struct i2c_board_info i2c_bq24196 __initdata = { I2C_BOARD_INFO("bq24196", (0xd6 >> 1)) };

#define BQ24196_BUSNUM 4

void __init mt_charger_init(void)
{
	i2c_register_board_info(BQ24196_BUSNUM, &i2c_bq24196, 1);
}

void __init mt_custom_battery_init(void)
{
	mt_bat_charging_data.charger_enable_pin = GPIO_CHR_CE_PIN;
}
