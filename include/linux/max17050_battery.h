/* max17050_battery.h
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

#ifndef __MAX17050_BATTERY_H_
#define __MAX17050_BATTERY_H_

struct max17050_platform_data {
	unsigned int fake;
};
#ifdef CONFIG_BATTERY_MAX17050
extern void max17050_battery_status(int status);
#else
static inline void max17050_battery_status(int status) {}
#endif
#endif /* __MAX17050_BATTERY_H_ */
