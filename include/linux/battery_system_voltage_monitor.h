/*
 * battery_system_voltage_monitor.h
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

#ifndef __BATTERY_SYSTEM_VOLTAGE_MONITOR_H
#define __BATTERY_SYSTEM_VOLTAGE_MONITOR_H

struct battery_system_voltage_monitor_worker_operations {
	int (*monitor_on_once)(unsigned int threshold, void *data);
	void (*monitor_off)(void *data);
	int (*listener_register)(int (*notification)(unsigned int threshold),
					void *data);
	void (*listener_unregister)(void *data);
};

struct battery_system_voltage_monitor_worker {
	struct battery_system_voltage_monitor_worker_operations *ops;
	void *data;
};

int battery_voltage_monitor_worker_register(
	struct battery_system_voltage_monitor_worker *worker);

int battery_voltage_monitor_on_once(unsigned int voltage);
int battery_voltage_monitor_off(void);
int battery_voltage_monitor_listener_register(
			int (*notification)(unsigned int voltage));
int battery_voltage_monitor_listener_unregister(void);

int system_voltage_monitor_worker_register(
	struct battery_system_voltage_monitor_worker *worker);

int system_voltage_monitor_on_once(unsigned int voltage);
int system_voltage_monitor_off(void);
int system_voltage_monitor_listener_register(
			int (*notification)(unsigned int voltage));
int system_voltage_monitor_listener_unregister(void);

#endif
