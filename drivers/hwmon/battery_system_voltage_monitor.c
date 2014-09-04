/*
 * battery_system_voltage_monitor.c
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

#include <linux/module.h>
#include <linux/err.h>
#include <linux/battery_system_voltage_monitor.h>

static DEFINE_MUTEX(worker_mutex);
static struct battery_system_voltage_monitor_worker *vbat_worker;
static struct battery_system_voltage_monitor_worker *vsys_worker;

static inline int __voltage_monitor_worker_register(bool is_vbat,
	struct battery_system_voltage_monitor_worker *worker)
{
	int ret = 0;

	mutex_lock(&worker_mutex);
	if (!worker || !worker->ops) {
		ret = -EINVAL;
		goto error;
	}

	if (is_vbat)
		vbat_worker = worker;
	else
		vsys_worker = worker;

error:
	mutex_unlock(&worker_mutex);
	return ret;
}

static inline int __voltage_monitor_on_once(bool is_vbat, unsigned int voltage)
{
	int ret = 0;
	struct battery_system_voltage_monitor_worker *worker;

	mutex_lock(&worker_mutex);
	if (is_vbat)
		worker = vbat_worker;
	else
		worker = vsys_worker;

	if (!worker || !worker->ops || !worker->ops->monitor_on_once) {
		ret = -ENODEV;
		goto error;
	}

	ret = worker->ops->monitor_on_once(voltage, worker->data);

error:
	mutex_unlock(&worker_mutex);
	return ret;
}

static inline int __voltage_monitor_off(bool is_vbat)
{
	int ret = 0;
	struct battery_system_voltage_monitor_worker *worker;

	mutex_lock(&worker_mutex);
	if (is_vbat)
		worker = vbat_worker;
	else
		worker = vsys_worker;

	if (!worker || !worker->ops || !worker->ops->monitor_off) {
		ret = -ENODEV;
		goto error;
	}

	worker->ops->monitor_off(worker->data);

error:
	mutex_unlock(&worker_mutex);
	return ret;
}

static inline int __voltage_monitor_listener_register(bool is_vbat,
				int (*notification)(unsigned int voltage))
{
	int ret = 0;
	struct battery_system_voltage_monitor_worker *worker;

	if (!notification)
		return -EINVAL;

	mutex_lock(&worker_mutex);
	if (is_vbat)
		worker = vbat_worker;
	else
		worker = vsys_worker;

	if (!worker || !worker->ops || !worker->ops->listener_register) {
		ret = -ENODEV;
		goto error;
	}

	ret = worker->ops->listener_register(notification, worker->data);

error:
	mutex_unlock(&worker_mutex);
	return ret;
}

static inline int __voltage_monitor_listener_unregister(bool is_vbat)
{
	int ret = 0;
	struct battery_system_voltage_monitor_worker *worker;

	mutex_lock(&worker_mutex);
	if (is_vbat)
		worker = vbat_worker;
	else
		worker = vsys_worker;

	if (!worker || !worker->ops || !worker->ops->listener_unregister) {
		ret = -ENODEV;
		goto error;
	}

	worker->ops->listener_unregister(worker->data);

error:
	mutex_unlock(&worker_mutex);
	return ret;
}

int battery_voltage_monitor_worker_register(
	struct battery_system_voltage_monitor_worker *worker)
{
	return __voltage_monitor_worker_register(true, worker);
}
EXPORT_SYMBOL_GPL(battery_voltage_monitor_worker_register);

int battery_voltage_monitor_on_once(unsigned int voltage)
{
	return __voltage_monitor_on_once(true, voltage);
}
EXPORT_SYMBOL_GPL(battery_voltage_monitor_on);

int battery_voltage_monitor_off(void)
{
	return __voltage_monitor_off(true);
}
EXPORT_SYMBOL_GPL(battery_voltage_monitor_off);

int battery_voltage_monitor_listener_register(
			int (*notification)(unsigned int voltage))
{
	return __voltage_monitor_listener_register(true, notification);
}
EXPORT_SYMBOL_GPL(battery_voltage_monitor_listener_register);

int battery_voltage_monitor_listener_unregister(void)
{
	return __voltage_monitor_listener_unregister(true);
}
EXPORT_SYMBOL_GPL(battery_voltage_monitor_listener_unregister);

int system_voltage_monitor_worker_register(
	struct battery_system_voltage_monitor_worker *worker)
{
	return __voltage_monitor_worker_register(false, worker);
}
EXPORT_SYMBOL_GPL(system_voltage_monitor_worker_register);

int system_voltage_monitor_on_once(unsigned int voltage)
{
	return __voltage_monitor_on_once(false, voltage);
}
EXPORT_SYMBOL_GPL(system_voltage_monitor_on);

int system_voltage_monitor_off(void)
{
	return __voltage_monitor_off(false);
}
EXPORT_SYMBOL_GPL(system_voltage_monitor_off);

int system_voltage_monitor_listener_register(
			int (*notification)(unsigned int voltage))
{
	return __voltage_monitor_listener_register(false, notification);
}
EXPORT_SYMBOL_GPL(system_voltage_monitor_listener_register);

int system_voltage_monitor_listener_unregister(void)
{
	return __voltage_monitor_listener_unregister(false);
}
EXPORT_SYMBOL_GPL(system_voltage_monitor_listener_unregister);
