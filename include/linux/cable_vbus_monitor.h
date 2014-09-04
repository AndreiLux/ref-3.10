/*
 * cable_vbus_monitor.h
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

#ifndef __CABLE_VBUS_MONITOR_H
#define __CABLE_VBUS_MONITOR_H

/*
 * Provider to register the VBUS check callback.
 *
 * is_vbus_latched: callback function to check if vbus is latched once
 * data: provider data
 */
int cable_vbus_monitor_latch_cb_register(int (*is_vbus_latched)(void *data),
					void *data);
/*
 * Provider to unregister the VBUS check callback.
 *
 * data: provider data
 */
int cable_vbus_monitor_latch_cb_unregister(void *data);

/*
 * User to check if VBUS is latched once.
 * The result might be cleared if called once.
 */
int cable_vbus_monitor_is_vbus_latched(void);
#endif	/* __CABLE_VBUS_MONITOR_H */
