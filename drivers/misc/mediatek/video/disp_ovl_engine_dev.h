#ifndef __DISP_OVL_ENGINE_DEV_H__
#define __DISP_OVL_ENGINE_DEV_H__

#include <linux/ioctl.h>
#include "mtkfb.h"

struct DISP_OVL_ENGINE_REGION {
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
};



struct disp_ovl_engine_config_mem_out_struct {
	unsigned int enable;
	unsigned int dirty;
	unsigned int outFormat;
	unsigned int dstAddr;
	struct DISP_OVL_ENGINE_REGION srcROI;	/* ROI */
	unsigned int security;
};


struct disp_ovl_engine_setting {
	struct fb_overlay_layer overlay_layers[HW_OVERLAY_COUNT];
	struct disp_ovl_engine_config_mem_out_struct mem_out;
	int out_fence_fd;
};


enum {
	DISP_OVL_REQUEST_FORCE_DISABLE_CABC = 0
};

struct disp_ovl_engine_request_struct {
	unsigned int request;
	int value;
	int ret;
};



#define DISP_OVL_ENGINE_IOCTL_MAGIC        'l'

#define DISP_OVL_ENGINE_IOCTL_GET_LAYER_INFO         _IOWR(DISP_OVL_ENGINE_IOCTL_MAGIC, 1, DISP_LAYER_INFO)
#define DISP_OVL_ENGINE_IOCTL_SET_LAYER_INFO         _IOWR(DISP_OVL_ENGINE_IOCTL_MAGIC, 2, struct fb_overlay_layer)
#define DISP_OVL_ENGINE_IOCTL_SET_OVERLAYED_BUF      _IOWR(DISP_OVL_ENGINE_IOCTL_MAGIC, 3, struct disp_ovl_engine_config_mem_out_struct)
#define DISP_OVL_ENGINE_IOCTL_TRIGGER_OVERLAY        _IOWR(DISP_OVL_ENGINE_IOCTL_MAGIC, 4, int)
#define DISP_OVL_ENGINE_IOCTL_WAIT_OVERLAY_COMPLETE  _IOWR(DISP_OVL_ENGINE_IOCTL_MAGIC, 5, int)
#define DISP_OVL_ENGINE_IOCTL_SECURE_MVA_MAP         _IOWR(DISP_OVL_ENGINE_IOCTL_MAGIC, 6, struct disp_mva_map)
#define DISP_OVL_ENGINE_IOCTL_SECURE_MVA_UNMAP       _IOWR(DISP_OVL_ENGINE_IOCTL_MAGIC, 7, struct disp_mva_map)
#define DISP_OVL_ENGINE_IOCTL_GET_REQUEST            _IOWR(DISP_OVL_ENGINE_IOCTL_MAGIC, 8, struct disp_ovl_engine_request_struct)
#define DISP_OVL_ENGINE_IOCTL_ACK_REQUEST            _IOWR(DISP_OVL_ENGINE_IOCTL_MAGIC, 9, struct disp_ovl_engine_request_struct)
#define DISP_OVL_ENGINE_IOCTL_TRIGGER_OVERLAY_FENCE  _IOWR(DISP_OVL_ENGINE_IOCTL_MAGIC, 10, struct disp_ovl_engine_setting)
#define DISP_OVL_ENGINE_IOCTL_GET_FENCE_FD           _IOWR(DISP_OVL_ENGINE_IOCTL_MAGIC, 11, int)

#endif
