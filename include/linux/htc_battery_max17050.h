/* htc_battery_max17050.h
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __HTC_BATTERY_MAX17050_H_
#define __HTC_BATTERY_MAX17050_H_

#include<linux/types.h>

struct htc_battery_max17050_ops {
	int (*get_vcell)(int *batt_volt);
	int (*get_battery_current)(int *batt_curr);
	int (*get_battery_avgcurrent)(int *batt_curr_avg);
	int (*get_temperature)(int *batt_temp);
	int (*get_soc)(int *batt_soc);
	int (*get_ocv)(int *batt_ocv);
	int (*get_battery_charge)(int *batt_charge);
	int (*get_battery_charge_ext)(int64_t *batt_charge_ext);
};

int htc_battery_max17050_gauge_register(struct htc_battery_max17050_ops *ops);
int htc_battery_max17050_gauge_unregister(void);

#endif /* __HTC_BATTERY_MAX17050_H_ */
