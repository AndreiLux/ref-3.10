
#include <linux/pm.h>
#include <linux/of_platform.h>
#include <asm/mach-types.h>
#include <mach/board.h>
#include <mach/battery_custom_data.h>
#include <mach/charging.h>
#include <linux/i2c.h>
#include <mt8135_ariel/cust_gpio_usage.h>
#include <mach/battery_data.h>

#include <mach/mt_gpio_def.h>
#include <mach/mt_gpio.h>
#include <mach/eint.h>

static struct i2c_board_info i2c_bq24297 __initdata = { I2C_BOARD_INFO("bq24297", (0xd6 >> 1)) };

#define BQ24297_BUSNUM 4

void __init mt_charger_init(void)
{
	mt_pin_set_mode(GPIOEXT22, 2);
	mt_pin_set_pull(GPIOEXT22, MT_PIN_PULL_ENABLE_UP);
	gpio_request(GPIOEXT22, "bq24297");
	gpio_direction_input(GPIOEXT22);
	i2c_bq24297.irq = EINT_IRQ(192+20);
	i2c_register_board_info(BQ24297_BUSNUM, &i2c_bq24297, 1);
}

#ifdef CONFIG_IDME
extern unsigned int idme_get_battery_info(int, size_t);
#endif

void __init mt_custom_battery_init(void)
{
	unsigned int battery_module;
	unsigned int battery_project;
#ifdef CONFIG_IDME
	battery_module = idme_get_battery_info(28, 1);
	printk(KERN_ERR "battery_module = %u\n", battery_module);
	battery_project = idme_get_battery_info(31, 2);
	printk(KERN_ERR "project = %u\n", battery_project);
#else
	battery_module = 1;  /* default module - ATL */
	battery_project = 2; /* default project - Ariel */
#endif

	mt_bat_meter_data.q_max_pos_50 = 3483;
	mt_bat_meter_data.q_max_pos_25 = 3502;
	mt_bat_meter_data.q_max_pos_0 = 3156;
	mt_bat_meter_data.q_max_neg_10 = 2805;

	mt_bat_meter_data.q_max_pos_50_h_current = 3458;
	mt_bat_meter_data.q_max_pos_25_h_current = 3457;
	mt_bat_meter_data.q_max_pos_0_h_current = 2967;
	mt_bat_meter_data.q_max_neg_10_h_current = 1400;

	mt_bat_meter_data.cust_tracking_point = 0;
	mt_bat_meter_data.car_tune_value = 102;

	/* NTC 47K */
	mt_bat_meter_data.rbat_pull_up_r = 24000;
	mt_bat_meter_data.rbat_pull_down_r = 100000000;
	mt_bat_meter_data.cust_r_fg_offset = 0;


	mt_bat_meter_data.battery_ntc_table_saddles =
		sizeof(custom_Batt_Temperature_Table) / sizeof(BATT_TEMPERATURE);

	switch (battery_module) {
	case 2: /* SDI */
		mt_bat_meter_data.battery_aging_table_saddles =
			sizeof(sdi_custom_battery_aging_table) / sizeof(BATTERY_CYCLE_STRUC);
		mt_bat_meter_data.p_battery_aging_table = sdi_custom_battery_aging_table;
		if (battery_project == 2) {  /* ariel */
			mt_bat_meter_data.q_max_pos_50 = 3528;
			mt_bat_meter_data.q_max_pos_50_h_current = 3497;
			mt_bat_meter_data.q_max_pos_25 = 3437;
			mt_bat_meter_data.q_max_pos_25_h_current = 3401;
			mt_bat_meter_data.q_max_pos_0 = 3030;
			mt_bat_meter_data.q_max_pos_0_h_current = 2750;
			mt_bat_meter_data.q_max_neg_10 = 2191;
			mt_bat_meter_data.q_max_neg_10_h_current = 1100;

			pr_notice("Load ARIEL SDI table!\n");
			mt_bat_meter_data.battery_profile_saddles =
				sizeof(ariel_sdi_custom_battery_profile_temperature) / sizeof(BATTERY_PROFILE_STRUC);
			mt_bat_meter_data.battery_r_profile_saddles =
				sizeof(ariel_sdi_custom_r_profile_temperature) / sizeof(R_PROFILE_STRUC);
			mt_bat_meter_data.p_battery_profile_t0 = ariel_sdi_custom_battery_profile_t0;
			mt_bat_meter_data.p_battery_profile_t1 = ariel_sdi_custom_battery_profile_t1;
			mt_bat_meter_data.p_battery_profile_t2 = ariel_sdi_custom_battery_profile_t2;
			mt_bat_meter_data.p_battery_profile_t3 = ariel_sdi_custom_battery_profile_t3;
			mt_bat_meter_data.p_r_profile_t0 = ariel_sdi_custom_r_profile_t0;
			mt_bat_meter_data.p_r_profile_t1 = ariel_sdi_custom_r_profile_t1;
			mt_bat_meter_data.p_r_profile_t2 = ariel_sdi_custom_r_profile_t2;
			mt_bat_meter_data.p_r_profile_t3 = ariel_sdi_custom_r_profile_t3;

			mt_bat_meter_data.p_battery_profile_temperature = ariel_sdi_custom_battery_profile_temperature;
			mt_bat_meter_data.p_r_profile_temperature = ariel_sdi_custom_r_profile_temperature;
		} else { /* aston */

			mt_bat_meter_data.q_max_pos_50 = 3532;
			mt_bat_meter_data.q_max_pos_50_h_current = 3487;
			mt_bat_meter_data.q_max_pos_25 = 3600;
			mt_bat_meter_data.q_max_pos_25_h_current = 3550;
			mt_bat_meter_data.q_max_pos_10 = 3401;
			mt_bat_meter_data.q_max_pos_10_h_current = 3148;
			mt_bat_meter_data.q_max_pos_0 = 2861;
			mt_bat_meter_data.q_max_pos_0_h_current = 2213;
			mt_bat_meter_data.q_max_neg_10 = 1565;
			mt_bat_meter_data.q_max_neg_10_h_current = 54;

			pr_notice("Load ASTON SDI table!\n");
			mt_bat_meter_data.battery_profile_saddles =
				sizeof(aston_sdi_custom_battery_profile_temperature) / sizeof(BATTERY_PROFILE_STRUC);
			mt_bat_meter_data.battery_r_profile_saddles =
				sizeof(aston_sdi_custom_r_profile_temperature) / sizeof(R_PROFILE_STRUC);
			mt_bat_meter_data.p_battery_profile_t0 = aston_sdi_custom_battery_profile_t0;
			mt_bat_meter_data.p_battery_profile_t1 = aston_sdi_custom_battery_profile_t1;
			mt_bat_meter_data.p_battery_profile_t1_5 = aston_sdi_custom_battery_profile_t1_5;
			mt_bat_meter_data.p_battery_profile_t2 = aston_sdi_custom_battery_profile_t2;
			mt_bat_meter_data.p_battery_profile_t3 = aston_sdi_custom_battery_profile_t3;
			mt_bat_meter_data.p_r_profile_t0 = aston_sdi_custom_r_profile_t0;
			mt_bat_meter_data.p_r_profile_t1 = aston_sdi_custom_r_profile_t1;
			mt_bat_meter_data.p_r_profile_t1_5 = aston_sdi_custom_r_profile_t1_5;
			mt_bat_meter_data.p_r_profile_t2 = aston_sdi_custom_r_profile_t2;
			mt_bat_meter_data.p_r_profile_t3 = aston_sdi_custom_r_profile_t3;

			mt_bat_meter_data.p_battery_profile_temperature = aston_sdi_custom_battery_profile_temperature;
			mt_bat_meter_data.p_r_profile_temperature = aston_sdi_custom_r_profile_temperature;
		}
		break;
	case 1: /* ATL */
	default:
		if (battery_module != 1)
			pr_notice("Load default table, battery_module = %d\n", battery_module);
		mt_bat_meter_data.battery_aging_table_saddles =
			sizeof(atl_custom_battery_aging_table) / sizeof(BATTERY_CYCLE_STRUC);
		mt_bat_meter_data.p_battery_aging_table = atl_custom_battery_aging_table;

		if (battery_project == 2) {  /* ariel */
			pr_notice("Load ARIEL ATL table!\n");
			mt_bat_meter_data.q_max_pos_50 = 3535;
			mt_bat_meter_data.q_max_pos_50_h_current = 3502;
			mt_bat_meter_data.q_max_pos_25 = 3452;
			mt_bat_meter_data.q_max_pos_25_h_current = 3409;
			mt_bat_meter_data.q_max_pos_0 = 2819;
			mt_bat_meter_data.q_max_pos_0_h_current = 2370;
			mt_bat_meter_data.q_max_neg_10 = 2183;
			mt_bat_meter_data.q_max_neg_10_h_current = 876;

			mt_bat_meter_data.battery_profile_saddles =
				sizeof(ariel_atl_custom_battery_profile_temperature) / sizeof(BATTERY_PROFILE_STRUC);
			mt_bat_meter_data.battery_r_profile_saddles =
				sizeof(ariel_atl_custom_r_profile_temperature) / sizeof(R_PROFILE_STRUC);

			mt_bat_meter_data.p_battery_profile_t0 = ariel_atl_custom_battery_profile_t0;
			mt_bat_meter_data.p_battery_profile_t1 = ariel_atl_custom_battery_profile_t1;
			mt_bat_meter_data.p_battery_profile_t2 = ariel_atl_custom_battery_profile_t2;
			mt_bat_meter_data.p_battery_profile_t3 = ariel_atl_custom_battery_profile_t3;
			mt_bat_meter_data.p_r_profile_t0 = ariel_atl_custom_r_profile_t0;
			mt_bat_meter_data.p_r_profile_t1 = ariel_atl_custom_r_profile_t1;
			mt_bat_meter_data.p_r_profile_t2 = ariel_atl_custom_r_profile_t2;
			mt_bat_meter_data.p_r_profile_t3 = ariel_atl_custom_r_profile_t3;

			mt_bat_meter_data.p_battery_profile_temperature = ariel_atl_custom_battery_profile_temperature;
			mt_bat_meter_data.p_r_profile_temperature = ariel_atl_custom_r_profile_temperature;
		} else {  /* aston */
			pr_notice("Load ASTON ATL table!\n");
			mt_bat_meter_data.q_max_pos_50 = 3576;
			mt_bat_meter_data.q_max_pos_50_h_current = 3526;
			mt_bat_meter_data.q_max_pos_25 = 3700;
			mt_bat_meter_data.q_max_pos_25_h_current = 3650;
			mt_bat_meter_data.q_max_pos_0 = 2564;
			mt_bat_meter_data.q_max_pos_0_h_current = 2402;
			mt_bat_meter_data.q_max_neg_10 = 2392;
			mt_bat_meter_data.q_max_neg_10_h_current = 2171;

			mt_bat_meter_data.battery_profile_saddles =
				sizeof(aston_atl_custom_battery_profile_temperature) / sizeof(BATTERY_PROFILE_STRUC);
			mt_bat_meter_data.battery_r_profile_saddles =
				sizeof(aston_atl_custom_r_profile_temperature) / sizeof(R_PROFILE_STRUC);

			mt_bat_meter_data.p_battery_profile_t0 = aston_atl_custom_battery_profile_t0;
			mt_bat_meter_data.p_battery_profile_t1 = aston_atl_custom_battery_profile_t1;
			mt_bat_meter_data.p_battery_profile_t2 = aston_atl_custom_battery_profile_t2;
			mt_bat_meter_data.p_battery_profile_t3 = aston_atl_custom_battery_profile_t3;
			mt_bat_meter_data.p_r_profile_t0 = aston_atl_custom_r_profile_t0;
			mt_bat_meter_data.p_r_profile_t1 = aston_atl_custom_r_profile_t1;
			mt_bat_meter_data.p_r_profile_t2 = aston_atl_custom_r_profile_t2;
			mt_bat_meter_data.p_r_profile_t3 = aston_atl_custom_r_profile_t3;

			mt_bat_meter_data.p_battery_profile_temperature = aston_atl_custom_battery_profile_temperature;
			mt_bat_meter_data.p_r_profile_temperature = aston_atl_custom_r_profile_temperature;
		}
	}

	mt_bat_meter_data.p_batt_temperature_table = custom_Batt_Temperature_Table;
	mt_bat_charging_data.charger_enable_pin = GPIO_CHR_CE_PIN;

	mt_bat_charging_data.jeita_temp_above_pos_60_cv_voltage = BATTERY_VOLT_04_000000_V;
	mt_bat_charging_data.jeita_temp_pos_45_to_pos_60_cv_voltage = BATTERY_VOLT_04_000000_V;
	mt_bat_charging_data.jeita_temp_pos_10_to_pos_45_cv_voltage = BATTERY_VOLT_04_200000_V;
	mt_bat_charging_data.jeita_temp_pos_0_to_pos_10_cv_voltage = BATTERY_VOLT_04_000000_V;
	mt_bat_charging_data.jeita_temp_neg_10_to_pos_0_cv_voltage = BATTERY_VOLT_04_000000_V;
	mt_bat_charging_data.jeita_temp_below_neg_10_cv_voltage = BATTERY_VOLT_04_000000_V;

	mt_bat_charging_data.sync_to_real_tracking_time = 30;

	mt_bat_charging_data.use_avg_temperature = 0;
	mt_bat_charging_data.max_charge_temperature = 60;
	mt_bat_charging_data.min_charge_temperature = 0;
	mt_bat_charging_data.temp_pos_60_threshold = 60;
	mt_bat_charging_data.temp_pos_60_thres_minus_x_degree = 57;
	mt_bat_charging_data.temp_pos_45_threshold = 45;
	mt_bat_charging_data.temp_pos_45_thres_minus_x_degree = 44;
	mt_bat_charging_data.temp_pos_10_threshold = 15;
	mt_bat_charging_data.temp_pos_10_thres_plus_x_degree = 16;
	mt_bat_charging_data.temp_pos_0_threshold = 1;
	mt_bat_charging_data.temp_pos_0_thres_plus_x_degree = 2;
	mt_bat_charging_data.temp_neg_10_threshold = -10;
	mt_bat_charging_data.temp_neg_10_thres_plus_x_degree = -10;
}
