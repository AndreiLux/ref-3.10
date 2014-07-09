/*
 * cable_vbus_monitor.c
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
#include <linux/cable_vbus_monitor.h>

static DEFINE_MUTEX(mutex);

struct cable_vbus_monitor_data {
	int (*is_vbus_latch_cb)(void *data);
	void *is_vbus_latch_cb_data;
} cable_vbus_monitor_data;


int cable_vbus_monitor_latch_cb_register(int (*is_vbus_latched)(void *data),
					void *data)
{
	int ret = 0;

	if (!is_vbus_latched)
		return -EINVAL;

	mutex_lock(&mutex);
	if (cable_vbus_monitor_data.is_vbus_latch_cb) {
		ret = -EBUSY;
		goto done;
	}
	cable_vbus_monitor_data.is_vbus_latch_cb = is_vbus_latched;
	cable_vbus_monitor_data.is_vbus_latch_cb_data = data;
done:
	mutex_unlock(&mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(cable_vbus_monitor_latch_cb_register);

int cable_vbus_monitor_latch_cb_unregister(void *data)
{
	mutex_lock(&mutex);
	if (data == cable_vbus_monitor_data.is_vbus_latch_cb_data) {
		cable_vbus_monitor_data.is_vbus_latch_cb = NULL;
		cable_vbus_monitor_data.is_vbus_latch_cb_data = NULL;
	}
	mutex_unlock(&mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(cable_vbus_monitor_latch_cb_unregister);

int cable_vbus_monitor_is_vbus_latched(void)
{
	int ret = 0;

	mutex_lock(&mutex);
	if (cable_vbus_monitor_data.is_vbus_latch_cb) {
		ret = cable_vbus_monitor_data.is_vbus_latch_cb(
			cable_vbus_monitor_data.is_vbus_latch_cb_data);
		if (ret < 0) {
			pr_warn("unknown cable vbus latch status\n");
			ret = 0;
		}
	}
	mutex_unlock(&mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(cable_vbus_monitor_is_vbus_latched);
