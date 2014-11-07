#ifndef _DSS_FB_IO_H_
#define _DSS_FB_IO_H_

#include "dss_types.h"

#define DSS_FB_IOCTL_MAGIC		'L'

#define DSS_FB_IOCTL_VEVENT_ID_SET	_IO(DSS_FB_IOCTL_MAGIC, 0)

#define DSS_NUM_CONFIGS				8

struct dss_fb_io_vevent_id_set
{
	void *vevent_id_vsync;
};

struct dss_fb_io_vsync_cb_data
{
	enum dss_display_port port;
	long long int timestamp;	//nsec

	long long int sync_tick;
};

struct dss_fb_io_hotplug_cb_data
{
	enum dss_display_port port;
	dss_bool connected;
};

#endif /* #ifndef _DSS_FB_IO_H_ */
