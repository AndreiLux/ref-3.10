#include "disp_ovl_engine_sw.h"

#ifdef DISP_OVL_ENGINE_SW_SUPPORT

#include <mach/m4u.h>
#include "disp_ovl_engine_core.h"


/* Parameter */
static DISP_OVL_ENGINE_INSTANCE disp_ovl_engine_params;


/* Ovl_engine thread */
static struct task_struct *disp_ovl_engine_sw_task;
static int disp_ovl_engine_sw_kthread(void *data);
static wait_queue_head_t disp_ovl_engine_sw_wq;
static unsigned int gWakeupOvlEngineSwThread;


/* Irq callback */
void (*disp_ovl_engine_sw_irq_callback) (unsigned int param) = NULL;


/* SW Overlay Processor */
void disp_ovl_engine_sw_processor(void);


void disp_ovl_engine_sw_init(void)
{
	memset(&disp_ovl_engine_params, 0, sizeof(DISP_OVL_ENGINE_INSTANCE));

	/* Init ovl_engine sw thread */
	init_waitqueue_head(&disp_ovl_engine_sw_wq);
	disp_ovl_engine_sw_task =
	    kthread_create(disp_ovl_engine_sw_kthread, NULL, "ovl_engine_sw_kthread");

	wake_up_process(disp_ovl_engine_sw_task);
}


static int disp_ovl_engine_sw_kthread(void *data)
{
	int wait_ret = 0;

	while (1) {
		wait_ret =
		    wait_event_interruptible(disp_ovl_engine_sw_wq, gWakeupOvlEngineSwThread);
		gWakeupOvlEngineSwThread = 0;

		DISP_OVL_ENGINE_DBG("disp_ovl_engine_sw_kthread wake_up\n");

		/* Start SW overlay */
		disp_ovl_engine_sw_processor();

		DISP_OVL_ENGINE_DBG("disp_ovl_engine_sw_processor done\n");

		/* Complete overlay, notify */
		if (disp_ovl_engine_sw_irq_callback != NULL)
			disp_ovl_engine_sw_irq_callback(0);
	}

	return 0;
}


void disp_ovl_engine_sw_set_params(DISP_OVL_ENGINE_INSTANCE *params)
{
	memcpy(&disp_ovl_engine_params, params, sizeof(DISP_OVL_ENGINE_INSTANCE));
}


void disp_ovl_engine_trigger_sw_overlay(void)
{
	gWakeupOvlEngineSwThread = 1;
	wake_up(&disp_ovl_engine_sw_wq);
}


void disp_ovl_engine_sw_register_irq(void (*irq_callback) (unsigned int param))
{
	disp_ovl_engine_sw_irq_callback = irq_callback;
}


void disp_ovl_engine_sw_bitblit(unsigned int in_format, unsigned int in_addr, int in_width,
				int in_height, int in_pitch, unsigned int out_format,
				unsigned int out_addr, int out_x, int out_y, int out_width,
				int out_height)
{
	int in_x, in_y;
	int x, y;
	unsigned int in_addr_curr;
	unsigned int out_addr_curr;

	if (in_format == eYUY2) {
		for (in_y = 0; in_y < in_height; in_y++) {
			y = (out_y + in_y);
			in_addr_curr = in_addr + in_y * in_pitch;
			out_addr_curr = out_addr + (y * out_width + out_x) * 4;

			for (in_x = 0; in_x < in_width; in_x++) {
				x = (out_x + in_x);

				if ((x < out_width) && (y < out_height)) {
					*(unsigned int *)out_addr_curr =
					    (*(unsigned short *)in_addr_curr << 8) |
					    (*(unsigned short *)in_addr_curr);

					in_addr_curr += 2;
					out_addr_curr += 4;
				}
			}
		}
	} else {
		for (in_y = 0; in_y < in_height; in_y++) {
			y = (out_y + in_y);
			in_addr_curr = in_addr + in_y * in_pitch;
			out_addr_curr = out_addr + (y * out_width + out_x) * 4;

			for (in_x = 0; in_x < in_width; in_x++) {
				x = (out_x + in_x);

				if ((x < out_width) && (y < out_height)) {
					*(unsigned int *)out_addr_curr =
					    *(unsigned int *)in_addr_curr;

					in_addr_curr += 4;
					out_addr_curr += 4;
				}
			}
		}

	}
}


void disp_ovl_engine_sw_processor(void)
{
	int i;

	unsigned int in_format;
	unsigned int in_addr;
	int in_width, in_height;
	int in_pitch;
	int in_size;

	unsigned int out_format;
	unsigned int out_addr;
	int out_x, out_y;
	int out_width, out_height;
	int out_size;

	out_format = disp_ovl_engine_params.MemOutConfig.outFormat;
	out_width = disp_ovl_engine_params.MemOutConfig.srcROI.width;
	out_height = disp_ovl_engine_params.MemOutConfig.srcROI.height;
	out_size = out_width * out_height * 4;	/* ARGB8888 */

	DISP_OVL_ENGINE_DBG("disp_ovl_engine_params.MemOutConfig.dstAddr = 0x%08x\n",
			    disp_ovl_engine_params.MemOutConfig.dstAddr);

	m4u_mva_map_kernel(disp_ovl_engine_params.MemOutConfig.dstAddr, out_size, 0, &out_addr,
			   &out_size);

	DISP_OVL_ENGINE_DBG("out_addr = 0x%08x\n", out_addr);

	for (i = 0; i < DDP_OVL_LAYER_MUN; i++) {
		if (disp_ovl_engine_params.cached_layer_config[i].layer_en) {
			/* Do bit blit */
			in_format = disp_ovl_engine_params.cached_layer_config[i].fmt;
			in_width = disp_ovl_engine_params.cached_layer_config[i].src_w;
			in_height = disp_ovl_engine_params.cached_layer_config[i].src_h;
			in_pitch = disp_ovl_engine_params.cached_layer_config[i].src_pitch;
			if (in_format == eYUY2)
				in_size = in_width * in_height * 2;	/* YUY2 422 */
			else
				in_size = in_width * in_height * 4;	/* ARGB8888 */

			DISP_OVL_ENGINE_DBG
			    ("disp_ovl_engine_params.cached_layer_config[i].addr = 0x%08x\n",
			     disp_ovl_engine_params.cached_layer_config[i].addr);

			m4u_mva_map_kernel(disp_ovl_engine_params.cached_layer_config[i].addr,
					   in_size, 0, &in_addr, &in_size);

			DISP_OVL_ENGINE_DBG("in_addr = 0x%08x\n", in_addr);

			out_x = disp_ovl_engine_params.cached_layer_config[i].dst_x;
			out_y = disp_ovl_engine_params.cached_layer_config[i].dst_y;

			disp_ovl_engine_sw_bitblit(in_format, in_addr, in_width, in_height,
						   in_pitch, out_format, out_addr, out_x, out_y,
						   out_width, out_height);

			m4u_mva_unmap_kernel(disp_ovl_engine_params.cached_layer_config[i].addr,
					     in_size, in_addr);
		}
	}

	DISP_OVL_ENGINE_DBG("out_addr = 0x%08x, value = 0x%08x\n", out_addr,
			    *(unsigned int *)out_addr);

	m4u_mva_unmap_kernel(disp_ovl_engine_params.MemOutConfig.dstAddr, out_size, out_addr);
}

#endif
