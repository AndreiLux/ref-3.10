/******************************************************************************
* Copyright (C) 2013 Broadcom Corporation
*
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation version 2.
*
* This program is distributed "as is" WITHOUT ANY WARRANTY of any
* kind, whether express or implied; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
******************************************************************************/

#ifndef _BCM_GPS_HOSTWAKE_H_
#define _BCM_GPS_HOSTWAKE_H_

struct bcm_gps_hostwake_platform_data {
	/* HOST_WAKE : to indicate that ASIC has an geofence event. */
	unsigned int gpio_hostwake;
};

#endif /*_BCM_GPS_HOSTWAKE_H_*/
