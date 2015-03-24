#include <asm/atomic.h>
#include <linux/file.h>
#include <sw_sync.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/ion.h>
#include <linux/ion_drv.h>
#include <mach/m4u_port.h>
#include "disp_ovl_engine_api.h"
#include "disp_ovl_engine_core.h"
#include "disp_ovl_engine_hw.h"
#include "ddp_ovl.h"


int Disp_Ovl_Engine_GetInstance(DISP_OVL_ENGINE_INSTANCE_HANDLE *pHandle, OVL_INSTANCE_MODE mode)
{
	unsigned int i = 0;
	unsigned int j = 0;
	static int need_trigger;

	DISP_OVL_ENGINE_INFO("Disp_Ovl_Engine_GetInstance\n");

	if (down_interruptible(&disp_ovl_engine_semaphore)) {
		return -1;
	}

	for (i = 0; i < OVL_ENGINE_INSTANCE_MAX_INDEX; i++) {
		if (!disp_ovl_engine.Instance[i].bUsed) {
			*pHandle = i;
			disp_ovl_engine.Instance[i].bUsed = TRUE;
			disp_ovl_engine.Instance[i].mode = mode;
			disp_ovl_engine.Instance[i].fgNeedConfigM4U = TRUE;
			disp_ovl_engine.Instance[i].outFence = -1;
			for (j = 0; j < DDP_OVL_LAYER_MUN; j++) {
				memset(&disp_ovl_engine.Instance[i].cached_layer_config[j], 0,
				       sizeof(OVL_CONFIG_STRUCT));
				disp_ovl_engine.Instance[i].cached_layer_config[j].layer = j;
			}

			if ((DECOUPLE_MODE == mode) && disp_ovl_engine.bCouple) {
				/* switch overlay direct link to de-couple mode */
				disp_ovl_engine.bModeSwitch = TRUE;
			} else
				disp_ovl_engine.bModeSwitch = FALSE;

			break;
		}
	}
	j = 0;
	while ((i < OVL_ENGINE_INSTANCE_MAX_INDEX) && !disp_ovl_engine.bCouple
	       && (j < OVL_ENGINE_INSTANCE_MAX_INDEX)) {
		/* find a valid de-couple instance */
		if ((disp_ovl_engine.Instance[j].bUsed)
		    && (DECOUPLE_MODE == disp_ovl_engine.Instance[j].mode))
			break;
		j++;
	}
	/* if no valid de-couple instance, it means only one couple instance , need to switch ovl direct link mode */
	if (j >= OVL_ENGINE_INSTANCE_MAX_INDEX)
		if (!disp_ovl_engine.bCouple)
			disp_ovl_engine.bModeSwitch = TRUE;

	up(&disp_ovl_engine_semaphore);

	if (OVL_ENGINE_INSTANCE_MAX_INDEX == i) {
		*pHandle = 0xFF;
		return OVL_ERROR;	/* invalid instance id */
	} else {
		/* disp_ovl_engine_hw_change_mode(mode); */
		DISP_OVL_ENGINE_INFO("Disp_Ovl_Engine_GetInstance success %d\n", *pHandle);

		/* For mode switch, MTKFB instance not need */
		if (need_trigger == 1)
			disp_ovl_engine_wake_up_ovl_engine_thread();

		if (need_trigger == 0)
			need_trigger = 1;

		return OVL_OK;
	}
}


int Disp_Ovl_Engine_ReleaseInstance(DISP_OVL_ENGINE_INSTANCE_HANDLE handle)
{
	int i = 0;
	DISP_OVL_ENGINE_INFO("Disp_Ovl_Engine_ReleaseInstance %d\n", handle);

	/* invalid handle */
	if (handle >= OVL_ENGINE_INSTANCE_MAX_INDEX)
		return OVL_ERROR;
	if (down_interruptible(&disp_ovl_engine_semaphore)) {
		return -1;
	}

	if (OVERLAY_STATUS_IDLE != disp_ovl_engine.Instance[handle].status) {
		up(&disp_ovl_engine_semaphore);

		DISP_OVL_ENGINE_ERR
		    ("Disp_Ovl_Engine_ReleaseInstance try to free a working instance\n");
		return OVL_ERROR;
	}

	memset(&disp_ovl_engine.Instance[handle], 0, sizeof(DISP_OVL_ENGINE_INSTANCE));
	disp_ovl_engine.Instance[handle].bUsed = FALSE;
	while (i < OVL_ENGINE_INSTANCE_MAX_INDEX) {
		if ((disp_ovl_engine.Instance[i].bUsed)
		    && (DECOUPLE_MODE == disp_ovl_engine.Instance[i].mode))
			break;
		i++;
	}

	if (i >= OVL_ENGINE_INSTANCE_MAX_INDEX) {
		/* no normal instance,switch to couple mode */
		if (!disp_ovl_engine.bCouple && !is_early_suspended)
			disp_ovl_engine.bModeSwitch = TRUE;
		else
			disp_ovl_engine.bModeSwitch = FALSE;

	}
	up(&disp_ovl_engine_semaphore);

	/* For mode switch */
	disp_ovl_engine_wake_up_ovl_engine_thread();

	DISP_OVL_ENGINE_INFO("Disp_Ovl_Engine_ReleaseInstance finish, bModeSwitch: %d\n",
			     disp_ovl_engine.bModeSwitch);

	return 0;
}


int Disp_Ovl_Engine_Get_layer_info(DISP_OVL_ENGINE_INSTANCE_HANDLE handle,
				   struct fb_overlay_layer *layerInfo)
{
	unsigned int layerpitch;
	if (handle >= OVL_ENGINE_INSTANCE_MAX_INDEX)
		return OVL_ERROR;
	DISP_OVL_ENGINE_INFO("Disp_Ovl_Engine_Get_layer_info\n");

	/* Todo, check layer id is valid. */

	if (down_interruptible(&disp_ovl_engine_semaphore)) {
		return -1;
	}

	layerInfo->layer_enable =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].layer_en;
	layerInfo->connected_type =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].
	    connected_type;
	layerInfo->identity =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].identity;
	layerInfo->isTdshp =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].isTdshp;
	layerInfo->layer_rotation = MTK_FB_ORIENTATION_0;
	layerInfo->layer_type = LAYER_2D;
	layerInfo->next_buff_idx =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].buff_idx;
	layerInfo->security =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].security;
	layerInfo->src_base_addr =
	    (void *)(disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].
		     vaddr);
	layerInfo->src_color_key =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].key;
	layerInfo->src_direct_link = 0;
	switch (disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].fmt) {
	case eYUY2:
		layerInfo->src_fmt = MTK_FB_FORMAT_YUV422;
		layerpitch = 2;
		break;
	case eRGB565:
		layerInfo->src_fmt = MTK_FB_FORMAT_RGB565;
		layerpitch = 2;
		break;
	case eRGB888:
		layerInfo->src_fmt = MTK_FB_FORMAT_RGB888;
		layerpitch = 3;
		break;
	case eBGR888:
		layerInfo->src_fmt = MTK_FB_FORMAT_BGR888;
		layerpitch = 3;
		break;
	case ePARGB8888:
		layerInfo->src_fmt = MTK_FB_FORMAT_ARGB8888;
		layerpitch = 4;
		break;
	case ePABGR8888:
		layerInfo->src_fmt = MTK_FB_FORMAT_ABGR8888;
		layerpitch = 4;
		break;
	case eARGB8888:
		layerInfo->src_fmt = MTK_FB_FORMAT_XRGB8888;
		layerpitch = 4;
		break;
	case eABGR8888:
		layerInfo->src_fmt = MTK_FB_FORMAT_XBGR8888;
		layerpitch = 4;
		break;
	default:
		layerInfo->src_fmt = MTK_FB_FORMAT_UNKNOWN;
		layerpitch = 1;
		break;
	}

	layerInfo->src_width =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].src_w;
	layerInfo->src_height =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].src_h;
	layerInfo->src_offset_x =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].src_x;
	layerInfo->src_offset_y =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].src_y;
	layerInfo->src_phy_addr =
	    (void *)(disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].
		     addr);
	layerInfo->src_pitch =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].src_pitch /
	    layerpitch;
	layerInfo->src_use_color_key =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].keyEn;
	layerInfo->tgt_offset_x =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].dst_x;
	layerInfo->tgt_offset_y =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].dst_y;
	layerInfo->tgt_width =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].dst_w;
	layerInfo->tgt_height =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].dst_h;
	layerInfo->video_rotation = MTK_FB_ORIENTATION_0;
	up(&disp_ovl_engine_semaphore);

	return 0;
}


int Disp_Ovl_Engine_Set_layer_info(DISP_OVL_ENGINE_INSTANCE_HANDLE handle,
				   struct fb_overlay_layer *layerInfo)
{
	/* Todo, check layer id is valid. */
	unsigned int fmt;
	unsigned int layerpitch;
	unsigned int layerbpp;

	if (handle >= OVL_ENGINE_INSTANCE_MAX_INDEX)
		return OVL_ERROR;
	DISP_OVL_ENGINE_INFO("Disp_Ovl_Engine_Set_layer_info %d\n", handle);

	if (down_interruptible(&disp_ovl_engine_semaphore)) {
		return OVL_ERROR;
	}

	if (layerInfo->layer_enable) {

		switch (layerInfo->src_fmt) {
		case MTK_FB_FORMAT_YUV422:
			fmt = eYUY2;
			layerpitch = 2;
			layerbpp = 24;
			break;

		case MTK_FB_FORMAT_RGB565:
			fmt = eRGB565;
			layerpitch = 2;
			layerbpp = 16;
			break;

		case MTK_FB_FORMAT_RGB888:
			fmt = eRGB888;
			layerpitch = 3;
			layerbpp = 24;
			break;
		case MTK_FB_FORMAT_BGR888:
			fmt = eBGR888;
			layerpitch = 3;
			layerbpp = 24;
			break;

		case MTK_FB_FORMAT_ARGB8888:
			fmt = ePARGB8888;
			layerpitch = 4;
			layerbpp = 32;
			break;
		case MTK_FB_FORMAT_ABGR8888:
			fmt = ePABGR8888;
			layerpitch = 4;
			layerbpp = 32;
			break;
		case MTK_FB_FORMAT_XRGB8888:
			fmt = eARGB8888;
			layerpitch = 4;
			layerbpp = 32;
			break;
		case MTK_FB_FORMAT_XBGR8888:
			fmt = eABGR8888;
			layerpitch = 4;
			layerbpp = 32;
			break;
		default:
			DISP_OVL_ENGINE_INFO
			    ("Disp_Ovl_Engine_Set_layer_info %d, src_fmt %d does not support\n",
			     handle, layerInfo->src_fmt);
			up(&disp_ovl_engine_semaphore);
			return -1;
		}

		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].fmt = fmt;

		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].vaddr =
		    (unsigned int)layerInfo->src_base_addr;
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].security =
		    layerInfo->security;
#ifdef DISP_OVL_ENGINE_FENCE
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].fence_fd =
		    layerInfo->fence_fd;
#endif
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].addr =
		    (unsigned int)layerInfo->src_phy_addr;
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].isTdshp =
		    layerInfo->isTdshp;
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].buff_idx =
		    layerInfo->next_buff_idx;
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].identity =
		    layerInfo->identity;
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].
		    connected_type = layerInfo->connected_type;

		/* set Alpha blending */
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].alpha =
		    0xFF;
		if (layerInfo->alpha_enable) {
			disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->
									     layer_id].aen = TRUE;
			disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].
			    alpha = layerInfo->alpha;
		} else {
			disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->
									     layer_id].aen = FALSE;
		}

		if (MTK_FB_FORMAT_ARGB8888 == layerInfo->src_fmt
		    || MTK_FB_FORMAT_ABGR8888 == layerInfo->src_fmt) {
			disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].
			    aen = TRUE;
		}


		/* set src width, src height */
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].src_x =
		    layerInfo->src_offset_x;
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].src_y =
		    layerInfo->src_offset_y;
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].src_w =
		    layerInfo->src_width;
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].src_h =
		    layerInfo->src_height;
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].dst_x =
		    layerInfo->tgt_offset_x;
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].dst_y =
		    layerInfo->tgt_offset_y;
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].dst_w =
		    layerInfo->tgt_width;
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].dst_h =
		    layerInfo->tgt_height;
		if (disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].
		    dst_w >
		    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].src_w)
			disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].
			    dst_w =
			    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->
										 layer_id].src_w;
		if (disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].
		    dst_h >
		    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].src_h)
			disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].
			    dst_h =
			    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->
										 layer_id].src_h;

		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].
		    src_pitch = layerInfo->src_pitch * layerpitch;

		/* set color key */
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].key =
		    layerInfo->src_color_key;
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].keyEn =
		    layerInfo->src_use_color_key;

	}

	disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].layer_en =
	    layerInfo->layer_enable;
	disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].isDirty = true;

	/* ion handle to mva */
	do {
		ion_mm_data_t data;
		size_t _unused;
		ion_phys_addr_t mva;
		struct ion_handle *ion_handles;

		if (!layerInfo->layer_enable)
			continue;

		/* If mva has already exist, do not need process ion handle to mva */
		if (disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].addr >
		    0)
			continue;

		/* Import ion handles so userspace (hwc) doesn't need to have a ref to them */
		if (layerInfo->ion_fd < 0) {
			ion_handles = NULL;
			continue;	/* worker will use mva from userspace */
		}
		ion_handles = ion_import_dma_buf(disp_ovl_engine.ion_client, layerInfo->ion_fd);
		if (IS_ERR(ion_handles)) {
			DISP_OVL_ENGINE_ERR("failed to import ion fd, disable ovl %d\n",
					    layerInfo->layer_id);
			ion_handles = NULL;
			continue;
		}

		if (!ion_handles)
			continue;	/* Use mva from userspace */

		/* configure buffer */
		memset(&data, 0, sizeof(ion_mm_data_t));
		data.mm_cmd = ION_MM_CONFIG_BUFFER;
		data.config_buffer_param.handle = ion_handles;
		data.config_buffer_param.eModuleID = M4U_CLNTMOD_OVL;
		ion_kernel_ioctl(disp_ovl_engine.ion_client, ION_CMD_MULTIMEDIA,
				 (unsigned long)&data);

		/* Get "physical" address (mva) */
		if (ion_phys(disp_ovl_engine.ion_client, ion_handles, &mva, &_unused)) {
			DISP_OVL_ENGINE_ERR("ion_phys failed, disable ovl %d\n",
					    layerInfo->layer_id);
			disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].
			    layer_en = 0;
			disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].
			    addr = 0;
			ion_free(disp_ovl_engine.ion_client, ion_handles);
			ion_handles = NULL;
			continue;
		}
		/* ion_free(disp_ovl_engine.ion_client, ion_handles); */
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].
		    fgIonHandleImport = true;
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].
		    ion_handles = ion_handles;
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].addr =
		    (unsigned int)mva;
	} while (0);

	if (layerInfo->layer_enable
	    && (disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].addr ==
		0)) {
		DISP_OVL_ENGINE_ERR
		    ("Invalid address for enable overlay, Instance %d, layer%d, addr%x, addr%x, ion_fd=0x%x\n",
		     handle, layerInfo->layer_id, (unsigned int)layerInfo->src_base_addr,
		     (unsigned int)layerInfo->src_phy_addr, layerInfo->ion_fd);
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].layer_en =
		    0;
	}
	atomic_set(&disp_ovl_engine.Instance[handle].OverlaySettingDirtyFlag, 1);
	atomic_set(&disp_ovl_engine.Instance[handle].OverlaySettingApplied, 0);
	disp_ovl_engine.Instance[handle].fgCompleted = 0;

	if (disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].
			layer_en == 0)
		disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->layer_id].
			security = LAYER_NORMAL_BUFFER;

	up(&disp_ovl_engine_semaphore);

	DISP_OVL_ENGINE_INFO
	    ("Instance%d, layer%d en%d,vaddr = 0x%x, addr = 0x%x, ion addr = 0x%x\n", handle,
	     layerInfo->layer_id, layerInfo->layer_enable, (unsigned int)layerInfo->src_base_addr,
	     (unsigned int)layerInfo->src_phy_addr,
	     (unsigned int)disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->
										layer_id].addr);
	return OVL_OK;
}

int Disp_Ovl_Engine_Get_Ovl_layer_info(DISP_OVL_ENGINE_INSTANCE_HANDLE handle,
				       DISP_LAYER_INFO *layerInfo)
{

	if (handle >= OVL_ENGINE_INSTANCE_MAX_INDEX)
		return OVL_ERROR;
	/* DISP_OVL_ENGINE_INFO("Disp_Ovl_Engine_Get_Ovl_layer_info\n"); */
	if (down_interruptible(&disp_ovl_engine_semaphore)) {
		return OVL_ERROR;
	}
	layerInfo->curr_en = disp_ovl_engine.captured_layer_config[layerInfo->id].layer_en;
	layerInfo->next_en =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->id].layer_en;
	layerInfo->hw_en = disp_ovl_engine.realtime_layer_config[layerInfo->id].layer_en;
	layerInfo->curr_idx = disp_ovl_engine.captured_layer_config[layerInfo->id].buff_idx;
	layerInfo->next_idx =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->id].buff_idx;
	layerInfo->hw_idx = disp_ovl_engine.realtime_layer_config[layerInfo->id].buff_idx;
	layerInfo->curr_identity = disp_ovl_engine.captured_layer_config[layerInfo->id].identity;
	layerInfo->next_identity =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->id].identity;
	layerInfo->hw_identity = disp_ovl_engine.realtime_layer_config[layerInfo->id].identity;
	layerInfo->curr_conn_type =
	    disp_ovl_engine.captured_layer_config[layerInfo->id].connected_type;
	layerInfo->next_conn_type =
	    disp_ovl_engine.Instance[handle].cached_layer_config[layerInfo->id].connected_type;
	layerInfo->hw_conn_type =
	    disp_ovl_engine.realtime_layer_config[layerInfo->id].connected_type;
	up(&disp_ovl_engine_semaphore);
	return OVL_OK;
}

int Disp_Ovl_Engine_Set_Overlayed_Buffer(DISP_OVL_ENGINE_INSTANCE_HANDLE handle,
						struct disp_ovl_engine_config_mem_out_struct
					 *overlayedBufInfo)
{
	DISP_OVL_ENGINE_INFO("Disp_Ovl_Engine_Set_Overlayed_Buffer %d\n", handle);

	if (down_interruptible(&disp_ovl_engine_semaphore)) {
		return OVL_ERROR;
	}

	memcpy(&(disp_ovl_engine.Instance[handle].MemOutConfig), overlayedBufInfo,
	       sizeof(struct disp_ovl_engine_config_mem_out_struct));

	switch (overlayedBufInfo->outFormat) {
	case MTK_FB_FORMAT_ARGB8888:
		disp_ovl_engine.Instance[handle].MemOutConfig.outFormat = eARGB8888;
		break;
	case MTK_FB_FORMAT_RGB888:
		disp_ovl_engine.Instance[handle].MemOutConfig.outFormat = eRGB888;
		break;
	case MTK_FB_FORMAT_YUV422:
		disp_ovl_engine.Instance[handle].MemOutConfig.outFormat = eUYVY;
		break;
	case MTK_FB_FORMAT_YUV420:
		disp_ovl_engine.Instance[handle].MemOutConfig.outFormat = eYUV_420_3P;
		break;
	}

	up(&disp_ovl_engine_semaphore);

	return OVL_OK;
}

extern int dump_all_info(void);
int Disp_Ovl_Engine_Trigger_Overlay(DISP_OVL_ENGINE_INSTANCE_HANDLE handle)
{
	if ((handle >= OVL_ENGINE_INSTANCE_MAX_INDEX) || !disp_ovl_engine.Instance[handle].bUsed)
		return OVL_ERROR;
	DISP_OVL_ENGINE_INFO("Disp_Ovl_Engine_Trigger_Overlay %d\n", handle);

	if (down_interruptible(&disp_ovl_engine_semaphore)) {
		return OVL_ERROR;
	}
	if (disp_ovl_engine.Instance[handle].status == OVERLAY_STATUS_IDLE) {
		disp_ovl_engine.Instance[handle].fgCompleted = 0;
		disp_ovl_engine.Instance[handle].status = OVERLAY_STATUS_TRIGGER;
		disp_ovl_engine.Instance[handle].fgCompleted = false;
		disp_ovl_engine_wake_up_ovl_engine_thread();

		DISP_OVL_ENGINE_INFO("disp_ovl_engine_wake_up_ovl_engine_thread %d\n", handle);
	} else {
		DISP_OVL_ENGINE_INFO("Disp_Ovl_Engine_Trigger_Overlay %d, not idle\n", handle);

		disp_ovl_engine_wake_up_ovl_engine_thread();

		up(&disp_ovl_engine_semaphore);
		return OVL_ERROR;
	}
	up(&disp_ovl_engine_semaphore);
	return OVL_OK;
}


int Disp_Ovl_Engine_Wait_Overlay_Complete(DISP_OVL_ENGINE_INSTANCE_HANDLE handle, int timeout)
{
	int wait_ret;
	int loopNum = 10; /*loop 10 times*/
	int subTimeout = timeout / loopNum;
	DISP_OVL_ENGINE_INFO("Disp_Ovl_Engine_Wait_Overlay_Complete %d, timeout = %d\n", handle,
			     timeout);

#if 0
	/*Wake-up Request Wait Queue for daemon */
	mb();			/*Add memory barrier before the other CPU (may) wakeup */
	wake_up_interruptible(&(disp_ovl_complete_wq));
#endif
	while (loopNum) {
		/*no need to wait when suspend state*/
		if (is_early_suspended)
			break;

		wait_ret =
			wait_event_interruptible_timeout(disp_ovl_complete_wq,
					     disp_ovl_engine.Instance[handle].fgCompleted, subTimeout);

		if (wait_ret != 0)
			break;

		loopNum--;
	}

	if (loopNum == 0 && wait_ret == 0) {
		DISP_OVL_ENGINE_ERR("Error: Disp_Ovl_Engine_Wait_Overlay_Complete timeout %d\n",
					    handle);

		return OVL_ERROR;
	}

	DISP_OVL_ENGINE_INFO("Disp_Ovl_Engine_Wait_Overlay_Complete success %d\n", handle);

	disp_ovl_engine.Instance[handle].fgCompleted = 0;

	return OVL_OK;
}


int Disp_Ovl_Engine_Get_Dirty_info(DISP_OVL_ENGINE_INSTANCE_HANDLE handle, int *OverlayDirty,
				   int *MemOutDirty)
{
	if (handle >= OVL_ENGINE_INSTANCE_MAX_INDEX)
		return OVL_ERROR;

	if (down_interruptible(&disp_ovl_engine_semaphore)) {
		return OVL_ERROR;
	}

	*OverlayDirty = atomic_read(&disp_ovl_engine.Instance[handle].OverlaySettingDirtyFlag);
	*MemOutDirty = disp_ovl_engine.Instance[handle].MemOutConfig.dirty;

	up(&disp_ovl_engine_semaphore);
	return OVL_OK;
}

int Disp_Ovl_Engine_Set_Path_Info(DISP_OVL_ENGINE_INSTANCE_HANDLE handle,
				  struct disp_path_config_struct *info)
{
	if ((handle >= OVL_ENGINE_INSTANCE_MAX_INDEX) || !disp_ovl_engine.Instance[handle].bUsed)
		return OVL_ERROR;
	DISP_OVL_ENGINE_INFO("Disp_Ovl_Engine_Set_Path_Info\n");
	if (down_interruptible(&disp_ovl_engine_semaphore)) {
		return OVL_ERROR;
	}
	memcpy(&disp_ovl_engine.Instance[handle].path_info, info,
	       sizeof(struct disp_path_config_struct));
	up(&disp_ovl_engine_semaphore);
	return OVL_OK;
}

int Disp_Ovl_Engine_Sync_Captured_layer_info(DISP_OVL_ENGINE_INSTANCE_HANDLE handle)
{
	if ((handle >= OVL_ENGINE_INSTANCE_MAX_INDEX) || !disp_ovl_engine.Instance[handle].bUsed)
		return OVL_ERROR;
	if (down_interruptible(&disp_ovl_engine_semaphore)) {
		return OVL_ERROR;
	}
	disp_ovl_engine.layer_config_index = 1 - disp_ovl_engine.layer_config_index;
	disp_ovl_engine.captured_layer_config =
	    disp_ovl_engine.layer_config[disp_ovl_engine.layer_config_index];
	/* DISP_OVL_ENGINE_INFO("========= cached --> captured ===========\n"); */
	memcpy(disp_ovl_engine.captured_layer_config,
	       disp_ovl_engine.Instance[handle].cached_layer_config,
	       sizeof(OVL_CONFIG_STRUCT) * DDP_OVL_LAYER_MUN);
	up(&disp_ovl_engine_semaphore);
	return OVL_OK;
}

int Disp_Ovl_Engine_Sync_Realtime_layer_info(DISP_OVL_ENGINE_INSTANCE_HANDLE handle)
{
	if ((handle >= OVL_ENGINE_INSTANCE_MAX_INDEX) || !disp_ovl_engine.Instance[handle].bUsed)
		return OVL_ERROR;
	disp_ovl_engine.realtime_layer_config = disp_ovl_engine.captured_layer_config;
	return OVL_OK;
}

int Disp_Ovl_Engine_Dump_layer_info(DISP_OVL_ENGINE_INSTANCE_HANDLE handle,
				    OVL_CONFIG_STRUCT *cached_layer,
				    OVL_CONFIG_STRUCT *captured_layer,
				    OVL_CONFIG_STRUCT *realtime_layer)
{
	if ((handle >= OVL_ENGINE_INSTANCE_MAX_INDEX) || !disp_ovl_engine.Instance[handle].bUsed)
		return OVL_ERROR;

	if (down_interruptible(&disp_ovl_engine_semaphore)) {
		return OVL_ERROR;
	}

	if (cached_layer != NULL) {
		memcpy(cached_layer, disp_ovl_engine.Instance[handle].cached_layer_config,
		       sizeof(OVL_CONFIG_STRUCT) * DDP_OVL_LAYER_MUN);
	}
	if (captured_layer != NULL) {
		memcpy(captured_layer, disp_ovl_engine.captured_layer_config,
		       sizeof(OVL_CONFIG_STRUCT) * DDP_OVL_LAYER_MUN);
	}
	if (realtime_layer != NULL) {
		memcpy(realtime_layer, disp_ovl_engine.realtime_layer_config,
		       sizeof(OVL_CONFIG_STRUCT) * DDP_OVL_LAYER_MUN);
	}
	up(&disp_ovl_engine_semaphore);
	return OVL_OK;
}


#ifdef DISP_OVL_ENGINE_FENCE

typedef struct {
	struct work_struct work;
	struct disp_ovl_engine_setting setting;
	struct sync_fence *fences[DDP_OVL_LAYER_MUN];
	DISP_OVL_ENGINE_INSTANCE *instance;
} ovls_work_t;

static void Disp_Ovl_Engine_Trigger_Overlay_Handler(struct work_struct *work_)
{
	int layer;
	ovls_work_t *work = (ovls_work_t *) work_;
	DISP_OVL_ENGINE_INSTANCE *instance = work->instance;
	int skip = 0;

	/* Wait input buffer fence */
	for (layer = 0; layer < DDP_OVL_LAYER_MUN; layer++) {
		if ((instance->cached_layer_config[layer].layer_en) && (work->fences[layer])) {
			DISP_OVL_ENGINE_INFO
			    ("Disp_Ovl_Engine_Trigger_Overlay_Handler %d layer %d wait fence\n",
			     instance->index, layer);
			if (sync_fence_wait(work->fences[layer], 1000) < 0)
				DISP_OVL_ENGINE_ERR
				    ("Disp_Ovl_Engine_Trigger_Overlay_Handler %d layer %d fence wait fail\n",
				     instance->index, layer);

			/* Unref the fence */
			sync_fence_put(work->fences[layer]);
		}
	}

	if (disp_ovl_engine.Instance[instance->index].status != OVERLAY_STATUS_IDLE) {
		int wait_ret;
		int err = 0;

		wait_ret =
		    wait_event_interruptible_timeout(disp_ovl_complete_wq,
						     disp_ovl_engine.Instance[instance->index].
						     fgCompleted, 1000);

		if (down_interruptible(&disp_ovl_engine_semaphore))
			err = 1;

		if (wait_ret == 0) {
			/* Timeout */
			disp_ovl_engine.Instance[instance->index].timeline_skip++;
			DISP_OVL_ENGINE_ERR
			    ("Disp_Ovl_Engine_Trigger_Overlay_Handler %d: skip buffer\n",
			     instance->index);
			skip = 1;
		}

		if (!err)
			up(&disp_ovl_engine_semaphore);

	}

	if (skip)
		return;

	/* Config input, output and trigger */
	for (layer = 0; layer < DDP_OVL_LAYER_MUN; layer++) {
		Disp_Ovl_Engine_Set_layer_info(instance->index,
					       &(work->setting.overlay_layers[layer]));
	}
	Disp_Ovl_Engine_Set_Overlayed_Buffer(instance->index, &(work->setting.mem_out));
	disp_ovl_engine.Instance[instance->index].fgCompleted = 0;
	Disp_Ovl_Engine_Trigger_Overlay(instance->index);

	kfree(work);
}
#endif


/* #define GET_FENCE_FD_SEPERATE */
int Disp_Ovl_Engine_Trigger_Overlay_Fence(DISP_OVL_ENGINE_INSTANCE_HANDLE handle,
					  struct disp_ovl_engine_setting *setting)
{
#ifdef DISP_OVL_ENGINE_FENCE
#ifndef GET_FENCE_FD_SEPERATE
	int fd = -1;
	struct sync_fence *fence;
	struct sync_pt *pt;
#endif
	ovls_work_t *work;

	DISP_OVL_ENGINE_INFO("Disp_Ovl_Engine_Trigger_Overlay_Fence %d\n", handle);

#ifndef GET_FENCE_FD_SEPERATE
	fd = get_unused_fd();
	if (fd < 0) {
		DISP_OVL_ENGINE_ERR("could not get a file descriptor\n");
		goto err;
	}
	/* Create output fence */
	disp_ovl_engine.Instance[handle].timeline_max++;
	pt = sw_sync_pt_create(disp_ovl_engine.Instance[handle].timeline,
			       disp_ovl_engine.Instance[handle].timeline_max);
	DISP_OVL_ENGINE_INFO("Disp_Ovl_Engine_Trigger_Overlay_Fence %d: fd = %d, time = %d\n",
			     handle, fd, disp_ovl_engine.Instance[handle].timeline_max);
	fence = sync_fence_create("overlay", pt);
	sync_fence_install(fence, fd);
	setting->out_fence_fd = fd;
#endif
	/* Init work */
	work = (ovls_work_t *) kzalloc(sizeof(ovls_work_t), GFP_KERNEL);
	if (!work) {
		DISP_OVL_ENGINE_ERR("could not allocate ovls_work_t\n");
		goto err;
	}
	INIT_WORK((struct work_struct *)work, Disp_Ovl_Engine_Trigger_Overlay_Handler);

	/* Import input fences so userspace can close (deref) them after ioctl */
	{
		int layer;

		for (layer = 0; layer < DDP_OVL_LAYER_MUN; layer++)
			if ((setting->overlay_layers[layer].layer_enable) &&
			    (setting->overlay_layers[layer].fence_fd >= 0))
				work->fences[layer] =
				    sync_fence_fdget(disp_ovl_engine.Instance[handle].
						     cached_layer_config[layer].fence_fd);
			else
				work->fences[layer] = NULL;
	}

	work->instance = &(disp_ovl_engine.Instance[handle]);

	/* Copy setting */
	memcpy(&work->setting, setting, sizeof(struct disp_ovl_engine_setting));


	/* Queue work */
	queue_work(disp_ovl_engine.Instance[handle].wq, (struct work_struct *)work);

	goto out;

 err:
#ifndef GET_FENCE_FD_SEPERATE
	DISP_OVL_ENGINE_ERR("Disp_Ovl_Engine_Trigger_Overlay_Fence fd=%d failed\n", fd);
	setting->out_fence_fd = -1;
	if (fd >= 0)
		put_unused_fd(fd);
#endif

	return OVL_ERROR;

 out:
	return OVL_OK;

#else
	return Disp_Ovl_Engine_Trigger_Overlay(handle);
#endif
}


int Disp_Ovl_Engine_Get_Fence_Fd(DISP_OVL_ENGINE_INSTANCE_HANDLE handle, int *fence_fd)
{
	int fd = -1;
	struct sync_fence *fence;
	struct sync_pt *pt;

	DISP_OVL_ENGINE_DBG("Disp_Ovl_Engine_Get_Fence_Fd %d\n", handle);

	if (down_interruptible(&disp_ovl_engine_semaphore)) {
		return OVL_ERROR;
	}

	fd = get_unused_fd();
	if (fd < 0) {
		DISP_OVL_ENGINE_DBG("could not get a file descriptor\n");
		goto err;
	}
	/* Create output fence */
	disp_ovl_engine.Instance[handle].timeline_max++;
	pt = sw_sync_pt_create(disp_ovl_engine.Instance[handle].timeline,
			       disp_ovl_engine.Instance[handle].timeline_max);
	DISP_OVL_ENGINE_DBG("Disp_Ovl_Engine_Get_Fence_Fd %d: fd = %d, time = %d\n", handle, fd,
			    disp_ovl_engine.Instance[handle].timeline_max);
	fence = sync_fence_create("overlay", pt);
	sync_fence_install(fence, fd);
	*fence_fd = fd;
	disp_ovl_engine.Instance[handle].outFence = 1;

	/* printk("Disp_Ovl_Engine_Get_Fence_Fd %d: fd = %d, time = %d, timeline->value = %d\n", */
	/* handle,fd,disp_ovl_engine.Instance[handle].timeline_max, */
	/* disp_ovl_engine.Instance[handle].timeline->value); */

	goto out;

 err:
	DISP_OVL_ENGINE_ERR("Disp_Ovl_Engine_Get_Fence_Fd fd=%d failed\n", fd);
	*fence_fd = -1;
	if (fd >= 0)
		put_unused_fd(fd);

	up(&disp_ovl_engine_semaphore);

	return OVL_ERROR;

 out:

	up(&disp_ovl_engine_semaphore);

	return OVL_OK;
}
