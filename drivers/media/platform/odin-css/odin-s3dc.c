/*
 * Stereoscopic 3D Creator driver
 *
 * Copyright (C) 2013 Mtekvision
 * Author: Daeheon Kim <kimdh@mtekvision.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/dma-mapping.h>

#if CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include "odin-css.h"
#include "odin-css-system.h"
#include "odin-css-clk.h"
#include "odin-css-utils.h"
#include "odin-framegrabber.h"
#include "odin-capture.h"
#include "odin-bayercontrol.h"
#include "odin-s3dc.h"

static struct css_s3dc_reg_data s3dc_reg_data;

#define S3DC_MODULE_NAME	"odin-s3dc"
#define CSS_OF_S3DC_NAME	"mtekvision,s3d-creator"

#define clear(x) memset(&(x), 0, sizeof(x))

struct s3dc_platform {
	struct platform_device	*pdev;
	struct resource *iores;
	struct resource *memres;
	void __iomem	*iobase;

#ifdef CONFIG_ODIN_ION_SMMU
	struct ion_client *ion_client;
#endif

	struct css_rsvd_memory *rsvd_mem;
	struct css_s3dc_buf_set blt_buf[4];

	int irq_s3dc;
	spinlock_t s3dc_lock;

	struct completion frame_end;
	struct work_struct work_zsl_load_to_s3dc;
};

NXSCContext *s3dc_ctxt = NULL;
struct s3dc_platform *g_s3dc_plat;
static unsigned int s3dc_frame_count = 0;

/*Nexus Stereo 3D Context Handler*/

unsigned int s3dc_frame_memory_base_addr= 0;


void s3dc_set_scaler_factor(int cam_sel, int src_w, int src_h,
							int dst_w, int dst_h, int deci_mode);
void s3dc_enable_ac_auto_start_quick(int enable);
void s3dc_set_reg_dma_Input0(NXSCContext *pctxt);
void s3dc_set_reg_dma_Input1(NXSCContext *pctxt);
void s3dc_set_reg_direct0(NXSCContext *pctxt);
void s3dc_set_reg_direct1(NXSCContext *pctxt);
void s3dc_fill_rect(void);
void s3dc_set_blt_in_s3d_mode(void);
void s3dc_enable_shadow_reg_up(void);
void s3dc_disable_shadow_reg_up(void);
void s3dc_change_framebuffer_address(int is_fullsize_mode);
NXSCContext *s3dc_get_context(void);


void s3dc_set_platform(struct s3dc_platform *s3dc_platform)
{
	if (!g_s3dc_plat)
		g_s3dc_plat = s3dc_platform;

	return;
}

inline void *s3dc_get_platform(void)
{
	return g_s3dc_plat;
}

int s3dc_hw_init(void)
{
	int ret = CSS_SUCCESS;

	css_clock_enable(CSS_CLK_S3DC);
	css_block_reset(BLK_RST_S3DC);

	return ret;
}

int s3dc_hw_deinit(void)
{
	int ret = CSS_SUCCESS;

	css_clock_disable(CSS_CLK_S3DC);

	return ret;
}

void *s3dc_malloc(int size)
{
	return kmalloc(size, GFP_KERNEL);
}

void s3dc_free(void *ptr)
{
	kfree(ptr);
}

void *s3dc_memcpy(void *dst, const void *src, int count)
{
	return memcpy(dst, src, count);
}

void *s3dc_memset(void *s, int c, int n)
{
	return memset(s,c,n);
}

/*
* 16bit register setting/getting function
*/
void s3dc_set_reg32(volatile void __iomem *addr, unsigned int data)
{
	writel(data, addr);

	return;
}

unsigned int s3dc_get_reg32(volatile void __iomem *addr)
{
	unsigned int val;

	val = readl(addr);

	return val;
}

void s3dc_poll_reg32(volatile void __iomem *addr, unsigned int bit,
			unsigned char cond)
{
	unsigned int val;

	do {
		val = readl(addr);
	} while ((((val)>>bit)&0x1)!=cond);
}

int s3dc_int_enable(unsigned int mask)
{
	unsigned int cur_mask = 0;
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	mask &= 0x7FF;

	cur_mask = s3dc_get_reg32(s3dc_plat->iobase + TOP_STATUS_MASK);

	cur_mask &= ~(mask);

	s3dc_set_reg32(s3dc_plat->iobase + TOP_STATUS_MASK, cur_mask);

	return 0;
}

int s3dc_int_disable(unsigned int mask)
{
	unsigned int cur_mask = 0;
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	mask &= 0x7FF;

	cur_mask = s3dc_get_reg32(s3dc_plat->iobase + TOP_STATUS_MASK);

	cur_mask |= mask;

	s3dc_set_reg32(s3dc_plat->iobase + TOP_STATUS_MASK, cur_mask);

	return 0;
}

unsigned int s3dc_get_interrupt_status(void)
{
	unsigned int cur_int = 0;
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	cur_int = s3dc_get_reg32(s3dc_plat->iobase + TOP_STATUS_INT);

	return cur_int;
}

int s3dc_int_clear(unsigned int int_source)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	s3dc_set_reg32(s3dc_plat->iobase + TOP_STATUS_CLR, int_source);

	return 0;

}

int s3dc_set_int_mode(top_int_mode mode)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	s3dc_set_reg32(s3dc_plat->iobase + TOP_INTERRUPT, mode);

	return 0;
}

int s3dc_wait_capture_done(unsigned int msec)
{
	int timeout = 0;
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	INIT_COMPLETION(s3dc_plat->frame_end);
	timeout = wait_for_completion_timeout(&s3dc_plat->frame_end,
					msecs_to_jiffies(msec));

	return timeout;
}

int s3dc_schedule_zsl_load_work(void)
{
	int ret = CSS_SUCCESS;
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	schedule_work(&s3dc_plat->work_zsl_load_to_s3dc);

	return ret;
}

int s3dc_set_v4l2_current_behavior(int behavior)
{
	int ret = CSS_SUCCESS;

	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();

	if (pctxt) {
		pctxt->v4l2_behavior = behavior;
	}

	return ret;
}

int s3dc_set_v4l2_stereo_buf_set(int buf_type, struct css_bufq *bufq)
{
	int ret = CSS_SUCCESS;
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();

	if (pctxt) {
		if (buf_type == CSS_BUF_PREVIEW_3D) {
			pctxt->v4l2_preview_buf_set = (void*)bufq;
		} else if (buf_type == CSS_BUF_CAPTURE_3D) {
			pctxt->v4l2_capture_buf_set = (void*)bufq;
		} else {
			return CSS_FAILURE;
		}
	} else {
		ret = CSS_ERROR_S3DC_INVALID_CTXT;
	}

	return ret;
}

int s3dc_set_v4l2_preview_cam_fh(void *capfh)
{
	int ret = CSS_SUCCESS;
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();

	if (pctxt) {
		pctxt->v4l2_preview_cam_fh = capfh;
	} else {
		ret = CSS_ERROR_S3DC_INVALID_CTXT;
	}

	return ret;
}

int s3dc_set_v4l2_capture_cam_fh(void *capfh)
{
	int ret = CSS_SUCCESS;
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();

	if (pctxt) {
		pctxt->v4l2_capture_cam_fh = capfh;
	} else {
		ret = CSS_ERROR_S3DC_INVALID_CTXT;
	}

	return ret;
}

void s3dc_set_frame_count(unsigned int frcnt)
{
	s3dc_frame_count = frcnt;
}

unsigned int s3dc_get_frame_count(void)
{
	return s3dc_frame_count;
}

static inline
int s3dc_get_v4l2_capture_index(struct css_bufq *bufq)
{
	int i			= 0;
	int buf_index	= -1;

	for (i = 0; i < bufq->count; i++) {
		if (bufq->bufs[i].state == CSS_BUF_CAPTURING) {
			buf_index = i;
			break;
		}
	}
	return buf_index;
}

static inline int s3dc_get_v4l2_next_index(struct css_bufq *bufq)
{
	int i			= 0;
	int buf_index	= 0;
	int buf_is_full = 1;

	if (bufq->count < 2) {
		return bufq->index;
	}

	/*search empty stream buf*/
	for (i = bufq->q_pos; i < bufq->count; i++) {
		if (bufq->bufs[i].state == CSS_BUF_QUEUED && i != bufq->q_pos) {
			buf_index = i;
			buf_is_full = 0;
			break;
		}
	}

	if (buf_is_full) {
		for (i = 0; i < bufq->q_pos; i++) {
			if (bufq->bufs[i].state == CSS_BUF_QUEUED && i != bufq->q_pos) {
				buf_index = i;
				buf_is_full = 0;
				break;
			}
		}
	}

	if (buf_is_full)
		return -1;
	else
		return buf_index;
}

int s3dc_get_v4l2_current_behavior(NXSCContext *pctxt)
{
	if (pctxt->v4l2_preview_cam_fh && pctxt->v4l2_capture_cam_fh == NULL) {
		struct capture_fh *capfh = NULL;
		capfh = (struct capture_fh *)pctxt->v4l2_preview_cam_fh;
		return (int)capfh->behavior;
	}

	if (pctxt->v4l2_preview_cam_fh == NULL && pctxt->v4l2_capture_cam_fh) {
		struct capture_fh *capfh = NULL;
		capfh = (struct capture_fh *)pctxt->v4l2_capture_cam_fh;
		return (int)capfh->behavior;
	}

	if (pctxt->v4l2_preview_cam_fh && pctxt->v4l2_capture_cam_fh) {
		struct capture_fh *preview_cam_fh = NULL;
		struct capture_fh *capture_cam_fh = NULL;

		preview_cam_fh = (struct capture_fh *)pctxt->v4l2_preview_cam_fh;
		capture_cam_fh = (struct capture_fh *)pctxt->v4l2_capture_cam_fh;

		if (preview_cam_fh->stream_on == 0 &&
			capture_cam_fh->stream_on == 0) {
			return (int)preview_cam_fh->behavior;
		}

		if (preview_cam_fh->stream_on == 0 &&
			capture_cam_fh->stream_on == 1) {
			return (int)capture_cam_fh->behavior;
		}

		if (preview_cam_fh->stream_on == 1 &&
			capture_cam_fh->stream_on == 0) {
			return (int)preview_cam_fh->behavior;
		}

		if (preview_cam_fh->stream_on == 1 &&
			capture_cam_fh->stream_on == 1) {
			return (int)preview_cam_fh->behavior;
		}
	}

	css_warn("unstable current behavior!!!\n");
	return CSS_BEHAVIOR_PREVIEW_3D;
}

irqreturn_t s3dc_irq(int irq, void *dev_id)
{
	unsigned int int_status = 0;
	struct s3dc_platform *s3dc_plat = NULL;
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	unsigned long flags;

	s3dc_plat = dev_id;

	int_status = s3dc_get_interrupt_status();

	if (int_status & INT_S3DC_FRAME_END) {
		s3dc_int_disable(INT_S3DC_FRAME_END);
		s3dc_int_clear(INT_S3DC_FRAME_END);
		css_s3dc("s3dc frame end int!!!!\n");

		complete(&s3dc_plat->frame_end);

		if (pctxt->config.out_path == OUTPUT_V4L2_MEM) {
			struct css_bufq *bufq = NULL;
			int capture_index = 0;
			int buf_index = 0;
			unsigned int phy_addr = 0;

			pctxt->v4l2_behavior = s3dc_get_v4l2_current_behavior(pctxt);

			if (pctxt->v4l2_behavior == CSS_BEHAVIOR_PREVIEW_3D) {
				bufq = (struct css_bufq*)pctxt->v4l2_preview_buf_set;
			} else {
				bufq = (struct css_bufq*)pctxt->v4l2_capture_buf_set;
			}

			capture_index = s3dc_get_v4l2_capture_index(bufq);
			if (capture_index >= 0) {
				spin_lock_irqsave(&bufq->lock, flags);
				bufq->q_pos = capture_index;
				spin_unlock_irqrestore(&bufq->lock, flags);

				if (bufq->bufs[capture_index].state == CSS_BUF_CAPTURING) {

					bufq->bufs[capture_index].state = CSS_BUF_CAPTURE_DONE;
					bufq->bufs[capture_index].frame_cnt
										= s3dc_get_frame_count();
					do_gettimeofday(&bufq->bufs[capture_index].timestamp);
					capture_buffer_push(bufq, capture_index);

					if (pctxt->v4l2_behavior == CSS_BEHAVIOR_PREVIEW_3D) {
						/* only 3d preview case, buf event will be generated
						 * in irq, zsl case, buf event will be generated in
						 * zsl stereo load function. buf event makes to work
						 * next procedure of application, therefore,
						 * next commands will be executed even if zsl load was
						 * not completed. It's not good.
						*/
						wake_up_interruptible(&bufq->complete.wait);
					}
				}
				buf_index = s3dc_get_v4l2_next_index(bufq);
				if (buf_index >= 0) {
					spin_lock_irqsave(&bufq->lock, flags);
					bufq->index = buf_index;
					spin_unlock_irqrestore(&bufq->lock, flags);
				} else {
					css_err("s3dc queue full : drop frame!!\n");
					s3dc_int_enable(INT_S3DC_FRAME_END);
					return IRQ_HANDLED;
				}
			} else {
				css_err("s3dc frame drop\n");
				s3dc_int_enable(INT_S3DC_FRAME_END);
				return IRQ_HANDLED;
			}

			if (pctxt->v4l2_behavior == CSS_BEHAVIOR_PREVIEW_3D) {
				struct capture_fh *capfh = NULL;
				capfh = (struct capture_fh *)pctxt->v4l2_preview_cam_fh;
				if (capture_get_stream_mode(capfh)) {
					if (bufq->mode == STREAM_MODE) {
						bufq->bufs[buf_index].state = CSS_BUF_CAPTURING;
						phy_addr = (unsigned int)bufq->bufs[buf_index].dma_addr;
						s3dc_set_framebuffer_address(phy_addr);
						scaler_set_capture_stereo_frame();
					}
				}
			}
		}
		s3dc_int_enable(INT_S3DC_FRAME_END);
	}

	return IRQ_HANDLED;
}

/*------------------------------------------------------------------------------
 * CF-eng Functions
 *----------------------------------------------------------------------------*/
void s3dc_set_2D_mode(void)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	pctxt->pre_mode = pctxt->curr_mode;
	pctxt->curr_mode = MODE_2D;
	pctxt->s3d_type = 0;
	pctxt->cf_enable = 1;
	pctxt->s3d_en = 0;
	pctxt->manual_en = 0;
	pctxt->deci_mode = 0;

	css_s3dc("S3D Mode : 2D mode\n");
	pctxt->cf_enable = 0;
	s3dc_disable_shadow_reg_up();

	if (pctxt->display_width > pctxt->cf_out_width) {/*side-bar*/
		udelay(100);
	}

	/*off address setting */
	s3dc_reg_data.cf_off0_address0_l.as32bits
				= (pctxt->frame_buffer_sm_addr[0] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address0_h.as32bits
				= (pctxt->frame_buffer_sm_addr[0] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address0_l.as32bits
				= (pctxt->frame_buffer_sm_addr[1] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address0_h.as32bits
				= (pctxt->frame_buffer_sm_addr[1] >> 2) >> 16;
	s3dc_reg_data.cf_off0_address1_l.as32bits
				= (pctxt->frame_buffer_sm_addr[0] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address1_h.as32bits
				= (pctxt->frame_buffer_sm_addr[0] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address1_l.as32bits
				= (pctxt->frame_buffer_sm_addr[1] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address1_h.as32bits
				= (pctxt->frame_buffer_sm_addr[1] >> 2) >> 16;

	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_L,
						s3dc_reg_data.cf_off0_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_H,
						s3dc_reg_data.cf_off0_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_L,
						s3dc_reg_data.cf_off1_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_H,
						s3dc_reg_data.cf_off1_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_L,
						s3dc_reg_data.cf_off0_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_H,
						s3dc_reg_data.cf_off0_address1_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_L,
						s3dc_reg_data.cf_off1_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_H,
						s3dc_reg_data.cf_off1_address1_h.as32bits);


	s3dc_reg_data.cf_s3d_fmt.asbits.s3d_type	= pctxt->s3d_type;
	s3dc_reg_data.cf_s3d_fmt.asbits.d2_sel		= pctxt->cam_select;
	s3dc_set_reg32(s3dc_plat->iobase + S3DTYPE_D2_SEL,
						s3dc_reg_data.cf_s3d_fmt.as32bits);

	s3dc_reg_data.cf_enable.asbits.s3d_en		= pctxt->s3d_en;
	s3dc_reg_data.cf_enable.asbits.blend_en		= pctxt->manual_en;
	s3dc_reg_data.cf_enable.asbits.deci_mode	= pctxt->deci_mode;
	s3dc_reg_data.cf_enable.asbits.deci_fil_en	= pctxt->deci_filter_en;
	s3dc_set_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN,
						s3dc_reg_data.cf_enable.as32bits);

	s3dc_set_scaler_factor(0, pctxt->cam0_crop_width, pctxt->cam0_crop_height,
				pctxt->cf_out_width, pctxt->cf_out_height, pctxt->deci_mode);
	s3dc_set_scaler_factor(1, pctxt->cam1_crop_width, pctxt->cam1_crop_height,
				pctxt->cf_out_width, pctxt->cf_out_height, pctxt->deci_mode);

	s3dc_reg_data.cf_wr_condition.as32bits =
						s3dc_get_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN);
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b0 = pctxt->cam0_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b1 = pctxt->cam1_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.cam0_pack_en = pctxt->blti_24bit_en;
	s3dc_reg_data.cf_wr_condition.asbits.cam1_pack_en = pctxt->blti_24bit_en;
	s3dc_set_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN,
						s3dc_reg_data.cf_wr_condition.as32bits);

	/*
	 * 이전 모드가 full size모드였으면
	 * 두 배로 할당했던 frame buffer 메모리를 다시 줄여서 할당한다.
	 */
	if ((pctxt->pre_mode == MODE_S3D_SIDE_BY_SIDE_FULL) ||
		(pctxt->pre_mode == MODE_S3D_TOP_BOTTOM_FULL)) {
		s3dc_change_framebuffer_address(0);
	}

	if (pctxt->display_width > pctxt->cf_out_width) {/*side-bar*/
		s3dc_enable_shadow_reg_up();
		/*
		 * display_width > cfOutWidth인 경우
		 * fill rect를 하여 side-bar가 생성되도록 한다.
		 */
		s3dc_fill_rect();
		s3dc_disable_shadow_reg_up();
	}

	if (pctxt->blt_fullsize_formatting_on == 1) {
		/* full size SBS or TB disable */
		pctxt->blt_fullsize_formatting_on = 0;
	}

	s3dc_set_blt_in_s3d_mode();
	pctxt->cf_enable = 1;

	s3dc_enable_shadow_reg_up();
}

void s3dc_set_3D_manual_mode(void)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	pctxt->pre_mode	= pctxt->curr_mode;
	pctxt->curr_mode = MODE_S3D_MANUAL;
	pctxt->s3d_type	= 0;
	pctxt->cf_enable = 1;
	pctxt->s3d_en = 1;
	pctxt->manual_en = 1;
	pctxt->deci_mode = 0;

	css_s3dc("S3D Mode : Manual mode\n");

	pctxt->cf_enable = 0;
	s3dc_disable_shadow_reg_up();

	if (pctxt->display_width > pctxt->cf_out_width) {
		udelay(100);
	}

	/*
	 * off address setting
	 */
	s3dc_reg_data.cf_off0_address0_l.as32bits
				= (pctxt->frame_buffer_sm_addr[0] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address0_h.as32bits
				= (pctxt->frame_buffer_sm_addr[0] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address0_l.as32bits
				= (pctxt->frame_buffer_sm_addr[1] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address0_h.as32bits
				= (pctxt->frame_buffer_sm_addr[1] >> 2) >> 16;
	s3dc_reg_data.cf_off0_address1_l.as32bits
				= (pctxt->frame_buffer_sm_addr[2] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address1_h.as32bits
				= (pctxt->frame_buffer_sm_addr[2] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address1_l.as32bits
				= (pctxt->frame_buffer_sm_addr[3] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address1_h.as32bits
				= (pctxt->frame_buffer_sm_addr[3] >> 2) >> 16;

	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_L,
					s3dc_reg_data.cf_off0_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_H,
					s3dc_reg_data.cf_off0_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_L,
					s3dc_reg_data.cf_off1_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_H,
					s3dc_reg_data.cf_off1_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_L,
					s3dc_reg_data.cf_off0_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_H,
					s3dc_reg_data.cf_off0_address1_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_L,
					s3dc_reg_data.cf_off1_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_H,
					s3dc_reg_data.cf_off1_address1_h.as32bits);

	s3dc_reg_data.cf_s3d_fmt.as32bits =
					s3dc_get_reg32(s3dc_plat->iobase + S3DTYPE_D2_SEL);
	/*
	 * init value = 0x0001
	 */
	s3dc_reg_data.cf_s3d_fmt.asbits.d2_sel	 = pctxt->cam_select;
	s3dc_reg_data.cf_s3d_fmt.asbits.s3d_type = pctxt->s3d_type;
	s3dc_set_reg32(s3dc_plat->iobase + S3DTYPE_D2_SEL,
					s3dc_reg_data.cf_s3d_fmt.as32bits);

	s3dc_reg_data.cf_enable.as32bits =
					s3dc_get_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN);
	s3dc_reg_data.cf_enable.asbits.s3d_en		= pctxt->s3d_en;
	s3dc_reg_data.cf_enable.asbits.blend_en		= pctxt->manual_en;
	s3dc_reg_data.cf_enable.asbits.deci_mode	= pctxt->deci_mode;
	s3dc_reg_data.cf_enable.asbits.deci_fil_en	= pctxt->deci_filter_en;
	s3dc_set_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN,
				s3dc_reg_data.cf_enable.as32bits);

	s3dc_set_scaler_factor(0, pctxt->cam0_crop_width, pctxt->cam0_crop_height,
				pctxt->cf_out_width, pctxt->cf_out_height, pctxt->deci_mode);
	s3dc_set_scaler_factor(1, pctxt->cam1_crop_width, pctxt->cam1_crop_height,
				pctxt->cf_out_width, pctxt->cf_out_height, pctxt->deci_mode);

	s3dc_reg_data.cf_wr_condition.as32bits =
					s3dc_get_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN);
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b0 = pctxt->cam0_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b1 = pctxt->cam1_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.cam0_pack_en = pctxt->blti_24bit_en;
	s3dc_reg_data.cf_wr_condition.asbits.cam1_pack_en = pctxt->blti_24bit_en;
	s3dc_set_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN,
					s3dc_reg_data.cf_wr_condition.as32bits);

	/*
	 * 이전 모드가 full size모드였으면
	 * 두 배로 할당했던 frame buffer 메모리를 다시 줄여서 할당한다.
	 */
	if ((pctxt->pre_mode == MODE_S3D_SIDE_BY_SIDE_FULL) ||
		(pctxt->pre_mode == MODE_S3D_TOP_BOTTOM_FULL)) {
		s3dc_change_framebuffer_address(0);
	}

	if (pctxt->display_width > pctxt->cf_out_width) {/*side-bar*/
		s3dc_enable_shadow_reg_up();
		/*
		 * display_width > cfOutWidth인 경우
		 * fill rect를 하여 side-bar가 생성되도록 한다.
		 */
		s3dc_fill_rect();
		s3dc_disable_shadow_reg_up();
	}

	if (pctxt->blt_fullsize_formatting_on == 1) {
		/* full size SBS or TB disable */
		pctxt->blt_fullsize_formatting_on = 0;
	}

	s3dc_set_blt_in_s3d_mode();
	pctxt->cf_enable = 1;

	s3dc_enable_shadow_reg_up();
}

void s3dc_set_3D_red_blue_mode(void)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	pctxt->pre_mode = pctxt->curr_mode;
	pctxt->curr_mode = MODE_S3D_RED_BLUE;
	pctxt->s3d_type = 2;
	pctxt->cf_enable = 1;
	pctxt->s3d_en = 1;
	pctxt->manual_en = 0;
	pctxt->deci_mode = 0;

	css_s3dc("S3D Mode : Red/Blue mode\n");

	pctxt->cf_enable = 0;
	s3dc_disable_shadow_reg_up();

	if (pctxt->display_width > pctxt->cf_out_width)
		udelay(100);

	/* off address setting */
	s3dc_reg_data.cf_off0_address0_l.as32bits
				= (pctxt->frame_buffer_sm_addr[0] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address0_h.as32bits
				= (pctxt->frame_buffer_sm_addr[0] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address0_l.as32bits
				= (pctxt->frame_buffer_sm_addr[1] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address0_h.as32bits
				= (pctxt->frame_buffer_sm_addr[1] >> 2) >> 16;
	s3dc_reg_data.cf_off0_address1_l.as32bits
				= (pctxt->frame_buffer_sm_addr[2] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address1_h.as32bits
				= (pctxt->frame_buffer_sm_addr[2] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address1_l.as32bits
				= (pctxt->frame_buffer_sm_addr[3] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address1_h.as32bits
				= (pctxt->frame_buffer_sm_addr[3] >> 2) >> 16;

	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_L,
					s3dc_reg_data.cf_off0_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_H,
					s3dc_reg_data.cf_off0_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_L,
					s3dc_reg_data.cf_off1_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_H,
					s3dc_reg_data.cf_off1_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_L,
					s3dc_reg_data.cf_off0_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_H,
					s3dc_reg_data.cf_off0_address1_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_L,
					s3dc_reg_data.cf_off1_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_H,
					s3dc_reg_data.cf_off1_address1_h.as32bits);

	s3dc_reg_data.cf_s3d_fmt.as32bits =
					s3dc_get_reg32(s3dc_plat->iobase + S3DTYPE_D2_SEL);
	s3dc_reg_data.cf_s3d_fmt.asbits.d2_sel	 = pctxt->cam_select;
	s3dc_reg_data.cf_s3d_fmt.asbits.s3d_type = pctxt->s3d_type;
	s3dc_set_reg32(s3dc_plat->iobase + S3DTYPE_D2_SEL,
					s3dc_reg_data.cf_s3d_fmt.as32bits);

	s3dc_reg_data.cf_enable.as32bits =
					s3dc_get_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN);
	s3dc_reg_data.cf_enable.asbits.s3d_en		= pctxt->s3d_en;
	s3dc_reg_data.cf_enable.asbits.blend_en		= pctxt->manual_en;
	s3dc_reg_data.cf_enable.asbits.deci_mode	= pctxt->deci_mode;
	s3dc_reg_data.cf_enable.asbits.deci_fil_en	= pctxt->deci_filter_en;
	s3dc_set_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN,
				s3dc_reg_data.cf_enable.as32bits);

	s3dc_set_scaler_factor(0, pctxt->cam0_crop_width, pctxt->cam0_crop_height,
				pctxt->cf_out_width, pctxt->cf_out_height, pctxt->deci_mode);
	s3dc_set_scaler_factor(1, pctxt->cam1_crop_width, pctxt->cam1_crop_height,
				pctxt->cf_out_width, pctxt->cf_out_height, pctxt->deci_mode);

	s3dc_reg_data.cf_wr_condition.as32bits =
						s3dc_get_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN);
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b0 = pctxt->cam0_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b1 = pctxt->cam1_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.cam0_pack_en = pctxt->blti_24bit_en;
	s3dc_reg_data.cf_wr_condition.asbits.cam1_pack_en = pctxt->blti_24bit_en;
	s3dc_set_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN,
						s3dc_reg_data.cf_wr_condition.as32bits);

	/*
	 * 이전 모드가 full size모드였으면
	 * 두 배로 할당했던 frame buffer 메모리를 다시 줄여서 할당한다.
	 */
	if ((pctxt->pre_mode == MODE_S3D_SIDE_BY_SIDE_FULL) ||
		(pctxt->pre_mode == MODE_S3D_TOP_BOTTOM_FULL)) {
		s3dc_change_framebuffer_address(0);
	}

	if (pctxt->display_width > pctxt->cf_out_width) {
		s3dc_enable_shadow_reg_up();
		/*
		 * display_width > cfOutWidth인 경우
		 * fill rect를 하여 side-bar가 생성되도록 한다.
		 */
		s3dc_fill_rect();
		s3dc_disable_shadow_reg_up();
	}

	if (pctxt->blt_fullsize_formatting_on == 1) {
		/* full size SBS or TB disable */
		pctxt->blt_fullsize_formatting_on = 0;
	}

	s3dc_set_blt_in_s3d_mode();
	pctxt->cf_enable = 1;
	s3dc_enable_shadow_reg_up();
}

void s3dc_set_3D_red_green_mode(void)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	pctxt->pre_mode = pctxt->curr_mode;
	pctxt->curr_mode = MODE_S3D_RED_GREEN;
	pctxt->s3d_type = 3;
	pctxt->cf_enable = 1;
	pctxt->s3d_en = 1;
	pctxt->manual_en = 0;
	pctxt->deci_mode = 0;

	css_s3dc("S3D Mode : Red/Green mode\n");

	pctxt->cf_enable = 0;
	s3dc_disable_shadow_reg_up();

	if (pctxt->display_width > pctxt->cf_out_width) {
		udelay(100);
	}

	/* off address setting */
	s3dc_reg_data.cf_off0_address0_l.as32bits
				= (pctxt->frame_buffer_sm_addr[0] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address0_h.as32bits
				= (pctxt->frame_buffer_sm_addr[0] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address0_l.as32bits
				= (pctxt->frame_buffer_sm_addr[1] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address0_h.as32bits
				= (pctxt->frame_buffer_sm_addr[1] >> 2) >> 16;
	s3dc_reg_data.cf_off0_address1_l.as32bits
				= (pctxt->frame_buffer_sm_addr[2] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address1_h.as32bits
				= (pctxt->frame_buffer_sm_addr[2] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address1_l.as32bits
				= (pctxt->frame_buffer_sm_addr[3] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address1_h.as32bits
				= (pctxt->frame_buffer_sm_addr[3] >> 2) >> 16;

	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_L,
					s3dc_reg_data.cf_off0_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_H,
					s3dc_reg_data.cf_off0_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_L,
					s3dc_reg_data.cf_off1_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_H,
					s3dc_reg_data.cf_off1_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_L,
					s3dc_reg_data.cf_off0_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_H,
					s3dc_reg_data.cf_off0_address1_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_L,
					s3dc_reg_data.cf_off1_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_H,
					s3dc_reg_data.cf_off1_address1_h.as32bits);

	s3dc_reg_data.cf_s3d_fmt.asbits.s3d_type	= pctxt->s3d_type;
	s3dc_reg_data.cf_s3d_fmt.asbits.d2_sel		= pctxt->cam_select;
	s3dc_set_reg32(s3dc_plat->iobase + S3DTYPE_D2_SEL,
						s3dc_reg_data.cf_s3d_fmt.as32bits);

	s3dc_reg_data.cf_enable.as32bits =
					s3dc_get_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN);
	s3dc_reg_data.cf_enable.asbits.s3d_en		= pctxt->s3d_en;
	s3dc_reg_data.cf_enable.asbits.blend_en		= pctxt->manual_en;
	s3dc_reg_data.cf_enable.asbits.deci_mode	= pctxt->deci_mode;
	s3dc_reg_data.cf_enable.asbits.deci_fil_en	= pctxt->deci_filter_en;
	s3dc_set_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN,
				s3dc_reg_data.cf_enable.as32bits);

	s3dc_set_scaler_factor(0, pctxt->cam0_crop_width, pctxt->cam0_crop_height,
				pctxt->cf_out_width, pctxt->cf_out_height, pctxt->deci_mode);
	s3dc_set_scaler_factor(1, pctxt->cam1_crop_width, pctxt->cam1_crop_height,
				pctxt->cf_out_width, pctxt->cf_out_height, pctxt->deci_mode);

	s3dc_reg_data.cf_wr_condition.as32bits =
						s3dc_get_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN);
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b0 = pctxt->cam0_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b1 = pctxt->cam1_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.cam0_pack_en = pctxt->blti_24bit_en;
	s3dc_reg_data.cf_wr_condition.asbits.cam1_pack_en = pctxt->blti_24bit_en;
	s3dc_set_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN,
						s3dc_reg_data.cf_wr_condition.as32bits);

	/*
	 * 이전 모드가 full size모드였으면
	 * 두 배로 할당했던 frame buffer 메모리를 다시 줄여서 할당한다.
	 */
	if ((pctxt->pre_mode == MODE_S3D_SIDE_BY_SIDE_FULL) ||
		(pctxt->pre_mode == MODE_S3D_TOP_BOTTOM_FULL)) {
		s3dc_change_framebuffer_address(0);
	}

	if (pctxt->display_width > pctxt->cf_out_width) {
		s3dc_enable_shadow_reg_up();
		/*
		 * display_width > cfOutWidth인 경우
		 * fill rect를 하여 side-bar가 생성되도록 한다.
		 */
		s3dc_fill_rect();
		s3dc_disable_shadow_reg_up();
	}

	if (pctxt->blt_fullsize_formatting_on == 1) {
		/* full size SBS or TB disable */
		pctxt->blt_fullsize_formatting_on = 0;
	}

	s3dc_set_blt_in_s3d_mode();
	pctxt->cf_enable = 1;

	s3dc_enable_shadow_reg_up();
}

void s3dc_set_3D_pixel_base_mode(void)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	pctxt->pre_mode	= pctxt->curr_mode;
	pctxt->curr_mode = MODE_S3D_PIXEL;
	pctxt->cf_enable = 1;
	pctxt->s3d_type	= 0;
	pctxt->s3d_en = 1;
	pctxt->manual_en = 0;
	pctxt->deci_mode = 1;

	css_s3dc("S3D Mode : PixelBase mode \n");

	pctxt->cf_enable = 0;
	s3dc_disable_shadow_reg_up();

	if (pctxt->display_width > pctxt->cf_out_width) {
		udelay(100);
	}

	/* off address setting */
	s3dc_reg_data.cf_off0_address0_l.as32bits
				= (pctxt->frame_buffer_sm_addr[0] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address0_h.as32bits
				= (pctxt->frame_buffer_sm_addr[0] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address0_l.as32bits
				= (pctxt->frame_buffer_sm_addr[1] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address0_h.as32bits
				= (pctxt->frame_buffer_sm_addr[1] >> 2) >> 16;
	s3dc_reg_data.cf_off0_address1_l.as32bits
				= (pctxt->frame_buffer_sm_addr[2] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address1_h.as32bits
				= (pctxt->frame_buffer_sm_addr[2] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address1_l.as32bits
				= (pctxt->frame_buffer_sm_addr[3] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address1_h.as32bits
				= (pctxt->frame_buffer_sm_addr[3] >> 2) >> 16;

	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_L,
					s3dc_reg_data.cf_off0_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_H,
					s3dc_reg_data.cf_off0_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_L,
					s3dc_reg_data.cf_off1_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_H,
					s3dc_reg_data.cf_off1_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_L,
					s3dc_reg_data.cf_off0_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_H,
					s3dc_reg_data.cf_off0_address1_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_L,
					s3dc_reg_data.cf_off1_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_H,
					s3dc_reg_data.cf_off1_address1_h.as32bits);

	s3dc_reg_data.cf_s3d_fmt.asbits.s3d_type	= pctxt->s3d_type;
	s3dc_reg_data.cf_s3d_fmt.asbits.d2_sel		= pctxt->cam_select;
	s3dc_set_reg32(s3dc_plat->iobase + S3DTYPE_D2_SEL,
						s3dc_reg_data.cf_s3d_fmt.as32bits);

	s3dc_reg_data.cf_enable.as32bits =
					s3dc_get_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN);
	s3dc_reg_data.cf_enable.asbits.s3d_en		= pctxt->s3d_en;
	s3dc_reg_data.cf_enable.asbits.blend_en		= pctxt->manual_en;
	s3dc_reg_data.cf_enable.asbits.deci_mode	= pctxt->deci_mode;
	s3dc_reg_data.cf_enable.asbits.deci_fil_en	= pctxt->deci_filter_en;
	s3dc_set_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN,
				s3dc_reg_data.cf_enable.as32bits);

	s3dc_set_scaler_factor(0, pctxt->cam0_crop_width, pctxt->cam0_crop_height,
				pctxt->cf_out_width, pctxt->cf_out_height, pctxt->deci_mode);
	s3dc_set_scaler_factor(1, pctxt->cam1_crop_width, pctxt->cam1_crop_height,
				pctxt->cf_out_width, pctxt->cf_out_height, pctxt->deci_mode);

	s3dc_reg_data.cf_wr_condition.as32bits =
						s3dc_get_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN);
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b0 = pctxt->cam0_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b1 = pctxt->cam1_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.cam0_pack_en = pctxt->blti_24bit_en;
	s3dc_reg_data.cf_wr_condition.asbits.cam1_pack_en = pctxt->blti_24bit_en;
	s3dc_set_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN,
						s3dc_reg_data.cf_wr_condition.as32bits);

	if ((pctxt->pre_mode == MODE_S3D_SIDE_BY_SIDE_FULL) ||
		(pctxt->pre_mode == MODE_S3D_TOP_BOTTOM_FULL)) {
		s3dc_change_framebuffer_address(0);
	}

	if (pctxt->display_width > pctxt->cf_out_width) {
		s3dc_enable_shadow_reg_up();
		s3dc_fill_rect();
		s3dc_disable_shadow_reg_up();
	}

	if (pctxt->blt_fullsize_formatting_on == 1) {
		pctxt->blt_fullsize_formatting_on = 0;
	}

	s3dc_set_blt_in_s3d_mode();
	pctxt->cf_enable = 1;

	s3dc_enable_shadow_reg_up();
}

void s3dc_set_3D_line_base_mode(void)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	pctxt->pre_mode	= pctxt->curr_mode;
	pctxt->curr_mode = MODE_S3D_LINE;
	pctxt->cf_enable = 1;
	pctxt->s3d_type	= 4;
	pctxt->s3d_en = 1;
	pctxt->manual_en = 0;
	pctxt->deci_mode = 2;

	css_s3dc("S3D Mode : LineBase mode \n");

	pctxt->cf_enable = 0;
	s3dc_disable_shadow_reg_up();

	if (pctxt->display_width > pctxt->cf_out_width) {
		udelay(100);
	}

	/* off address setting */
	s3dc_reg_data.cf_off0_address0_l.as32bits
					= (pctxt->frame_buffer_sm_addr[0] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address0_h.as32bits
					= (pctxt->frame_buffer_sm_addr[0] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address0_l.as32bits
					= (pctxt->frame_buffer_sm_addr[1] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address0_h.as32bits
					= (pctxt->frame_buffer_sm_addr[1] >> 2) >> 16;
	s3dc_reg_data.cf_off0_address1_l.as32bits
					= (pctxt->frame_buffer_sm_addr[2] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address1_h.as32bits
					= (pctxt->frame_buffer_sm_addr[2] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address1_l.as32bits
					= (pctxt->frame_buffer_sm_addr[3] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address1_h.as32bits
					= (pctxt->frame_buffer_sm_addr[3] >> 2) >> 16;

	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_L,
					s3dc_reg_data.cf_off0_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_H,
					s3dc_reg_data.cf_off0_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_L,
					s3dc_reg_data.cf_off1_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_H,
					s3dc_reg_data.cf_off1_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_L,
					s3dc_reg_data.cf_off0_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_H,
					s3dc_reg_data.cf_off0_address1_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_L,
					s3dc_reg_data.cf_off1_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_H,
					s3dc_reg_data.cf_off1_address1_h.as32bits);

	s3dc_reg_data.cf_s3d_fmt.asbits.s3d_type	= pctxt->s3d_type;
	s3dc_reg_data.cf_s3d_fmt.asbits.d2_sel		= pctxt->cam_select;
	s3dc_set_reg32(s3dc_plat->iobase + S3DTYPE_D2_SEL,
						s3dc_reg_data.cf_s3d_fmt.as32bits);

	s3dc_reg_data.cf_enable.as32bits =
					s3dc_get_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN);
	s3dc_reg_data.cf_enable.asbits.s3d_en		= pctxt->s3d_en;
	s3dc_reg_data.cf_enable.asbits.blend_en		= pctxt->manual_en;
	s3dc_reg_data.cf_enable.asbits.deci_mode	= pctxt->deci_mode;
	s3dc_reg_data.cf_enable.asbits.deci_fil_en	= pctxt->deci_filter_en;
	s3dc_set_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN,
				s3dc_reg_data.cf_enable.as32bits);

	s3dc_set_scaler_factor(0, pctxt->cam0_crop_width, pctxt->cam0_crop_height,
							pctxt->cf_out_width, pctxt->cf_out_height / 2,
							pctxt->deci_mode);
	s3dc_set_scaler_factor(1, pctxt->cam1_crop_width, pctxt->cam1_crop_height,
							pctxt->cf_out_width, pctxt->cf_out_height / 2,
							pctxt->deci_mode);

	s3dc_reg_data.cf_wr_condition.as32bits =
						s3dc_get_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN);
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b0 = pctxt->cam0_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b1 = pctxt->cam1_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.cam0_pack_en = pctxt->blti_24bit_en;
	s3dc_reg_data.cf_wr_condition.asbits.cam1_pack_en = pctxt->blti_24bit_en;
	s3dc_set_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN,
						s3dc_reg_data.cf_wr_condition.as32bits);

	if ((pctxt->pre_mode == MODE_S3D_SIDE_BY_SIDE_FULL) ||
		(pctxt->pre_mode == MODE_S3D_TOP_BOTTOM_FULL)) {
		s3dc_change_framebuffer_address(0);
	}

	if (pctxt->display_width > pctxt->cf_out_width) {
		s3dc_enable_shadow_reg_up();
		s3dc_fill_rect();
		s3dc_disable_shadow_reg_up();
	}

	if (pctxt->blt_fullsize_formatting_on == 1) {
		pctxt->blt_fullsize_formatting_on = 0;
	}

	s3dc_set_blt_in_s3d_mode();
	pctxt->cf_enable = 1;

	s3dc_enable_shadow_reg_up();
}

void s3dc_set_3D_side_by_side_mode(void)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	pctxt->pre_mode	= pctxt->curr_mode;
	pctxt->curr_mode = MODE_S3D_SIDE_BY_SIDE;
	pctxt->cf_enable = 1;
	pctxt->s3d_type	= 5;
	pctxt->s3d_en	= 1;
	pctxt->manual_en = 0;
	pctxt->deci_mode = 1;

	css_s3dc("S3D Mode : SideBySide mode \n");

	pctxt->cf_enable = 0;
	s3dc_disable_shadow_reg_up();

	if (pctxt->display_width > pctxt->cf_out_width) {
		udelay(100);
	}

	/* off address setting */
	s3dc_reg_data.cf_off0_address0_l.as32bits
					= (pctxt->frame_buffer_sm_addr[0] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address0_h.as32bits
					= (pctxt->frame_buffer_sm_addr[0] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address0_l.as32bits
					= (pctxt->frame_buffer_sm_addr[1] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address0_h.as32bits
					= (pctxt->frame_buffer_sm_addr[1] >> 2) >> 16;
	s3dc_reg_data.cf_off0_address1_l.as32bits
					= (pctxt->frame_buffer_sm_addr[2] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address1_h.as32bits
					= (pctxt->frame_buffer_sm_addr[2] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address1_l.as32bits
					= (pctxt->frame_buffer_sm_addr[3] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address1_h.as32bits
					= (pctxt->frame_buffer_sm_addr[3] >> 2) >> 16;

	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_L,
					s3dc_reg_data.cf_off0_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_H,
					s3dc_reg_data.cf_off0_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_L,
					s3dc_reg_data.cf_off1_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_H,
					s3dc_reg_data.cf_off1_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_L,
					s3dc_reg_data.cf_off0_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_H,
					s3dc_reg_data.cf_off0_address1_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_L,
					s3dc_reg_data.cf_off1_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_H,
					s3dc_reg_data.cf_off1_address1_h.as32bits);

	s3dc_reg_data.cf_s3d_fmt.asbits.s3d_type	= pctxt->s3d_type;
	s3dc_reg_data.cf_s3d_fmt.asbits.d2_sel		= pctxt->cam_select;
	s3dc_set_reg32(s3dc_plat->iobase + S3DTYPE_D2_SEL,
						s3dc_reg_data.cf_s3d_fmt.as32bits);

	s3dc_reg_data.cf_enable.as32bits =
					s3dc_get_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN);
	s3dc_reg_data.cf_enable.asbits.s3d_en		= pctxt->s3d_en;
	s3dc_reg_data.cf_enable.asbits.blend_en		= pctxt->manual_en;
	s3dc_reg_data.cf_enable.asbits.deci_mode	= pctxt->deci_mode;
	s3dc_reg_data.cf_enable.asbits.deci_fil_en	= pctxt->deci_filter_en;
	s3dc_set_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN,
				s3dc_reg_data.cf_enable.as32bits);

	s3dc_set_scaler_factor(0, pctxt->cam0_crop_width, pctxt->cam0_crop_height,
				pctxt->cf_out_width/2, pctxt->cf_out_height, pctxt->deci_mode);
	s3dc_set_scaler_factor(1, pctxt->cam1_crop_width, pctxt->cam1_crop_height,
				pctxt->cf_out_width/2, pctxt->cf_out_height, pctxt->deci_mode);

	s3dc_reg_data.cf_wr_condition.as32bits =
						s3dc_get_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN);
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b0 = pctxt->cam0_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b1 = pctxt->cam1_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.cam0_pack_en = pctxt->blti_24bit_en;
	s3dc_reg_data.cf_wr_condition.asbits.cam1_pack_en = pctxt->blti_24bit_en;
	s3dc_set_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN,
						s3dc_reg_data.cf_wr_condition.as32bits);

	if ((pctxt->pre_mode == MODE_S3D_SIDE_BY_SIDE_FULL) ||
		(pctxt->pre_mode == MODE_S3D_TOP_BOTTOM_FULL)) {
		s3dc_change_framebuffer_address(0);
	}

	if (pctxt->display_width > pctxt->cf_out_width) {
		s3dc_enable_shadow_reg_up();
		s3dc_fill_rect();
		s3dc_disable_shadow_reg_up();
	}

	if (pctxt->blt_fullsize_formatting_on == 1) {
		pctxt->blt_fullsize_formatting_on = 0;
	}

	s3dc_set_blt_in_s3d_mode();
	pctxt->cf_enable = 1;

	s3dc_enable_shadow_reg_up();
}

void s3dc_set_3D_sub_pixel_mode(void)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	pctxt->pre_mode	= pctxt->curr_mode;
	pctxt->curr_mode = MODE_S3D_SUB_PIXEL;
	pctxt->cf_enable = 1;
	pctxt->s3d_type	= 1;
	pctxt->s3d_en = 1;
	pctxt->manual_en = 0;
	pctxt->deci_mode = 0;

	css_s3dc("S3D Mode : SubPixel mode\n");

	pctxt->cf_enable = 0;
	s3dc_disable_shadow_reg_up();

	if (pctxt->display_width > pctxt->cf_out_width) {
		udelay(100);
	}

	/* off address setting */
	s3dc_reg_data.cf_off0_address0_l.as32bits
					= (pctxt->frame_buffer_sm_addr[0] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address0_h.as32bits
					= (pctxt->frame_buffer_sm_addr[0] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address0_l.as32bits
					= (pctxt->frame_buffer_sm_addr[1] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address0_h.as32bits
					= (pctxt->frame_buffer_sm_addr[1] >> 2) >> 16;
	s3dc_reg_data.cf_off0_address1_l.as32bits
					= (pctxt->frame_buffer_sm_addr[2] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address1_h.as32bits
					= (pctxt->frame_buffer_sm_addr[2] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address1_l.as32bits
					= (pctxt->frame_buffer_sm_addr[3] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address1_h.as32bits
					= (pctxt->frame_buffer_sm_addr[3] >> 2) >> 16;

	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_L,
					s3dc_reg_data.cf_off0_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_H,
					s3dc_reg_data.cf_off0_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_L,
					s3dc_reg_data.cf_off1_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_H,
					s3dc_reg_data.cf_off1_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_L,
					s3dc_reg_data.cf_off0_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_H,
					s3dc_reg_data.cf_off0_address1_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_L,
					s3dc_reg_data.cf_off1_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_H,
					s3dc_reg_data.cf_off1_address1_h.as32bits);

	s3dc_reg_data.cf_s3d_fmt.asbits.s3d_type	= pctxt->s3d_type;
	s3dc_reg_data.cf_s3d_fmt.asbits.d2_sel		= pctxt->cam_select;
	s3dc_set_reg32(s3dc_plat->iobase + S3DTYPE_D2_SEL,
						s3dc_reg_data.cf_s3d_fmt.as32bits);


	s3dc_reg_data.cf_enable.as32bits =
					s3dc_get_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN);
	s3dc_reg_data.cf_enable.asbits.s3d_en		= pctxt->s3d_en;
	s3dc_reg_data.cf_enable.asbits.blend_en		= pctxt->manual_en;
	s3dc_reg_data.cf_enable.asbits.deci_mode	= pctxt->deci_mode;
	s3dc_reg_data.cf_enable.asbits.deci_fil_en	= pctxt->deci_filter_en;
	s3dc_set_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN,
				s3dc_reg_data.cf_enable.as32bits);

	s3dc_set_scaler_factor(0, pctxt->cam0_crop_width, pctxt->cam0_crop_height,
				pctxt->cf_out_width, pctxt->cf_out_height, pctxt->deci_mode);
	s3dc_set_scaler_factor(1, pctxt->cam1_crop_width, pctxt->cam1_crop_height,
				pctxt->cf_out_width, pctxt->cf_out_height, pctxt->deci_mode);

	s3dc_reg_data.cf_wr_condition.as32bits =
						s3dc_get_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN);
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b0 = pctxt->cam0_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b1 = pctxt->cam1_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.cam0_pack_en = pctxt->blti_24bit_en;
	s3dc_reg_data.cf_wr_condition.asbits.cam1_pack_en = pctxt->blti_24bit_en;
	s3dc_set_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN,
						s3dc_reg_data.cf_wr_condition.as32bits);

	if ((pctxt->pre_mode == MODE_S3D_SIDE_BY_SIDE_FULL) ||
		(pctxt->pre_mode == MODE_S3D_TOP_BOTTOM_FULL)) {
		s3dc_change_framebuffer_address(0);
	}

	if (pctxt->display_width > pctxt->cf_out_width) {
		s3dc_enable_shadow_reg_up();
		s3dc_fill_rect();
		s3dc_disable_shadow_reg_up();
	}

	if (pctxt->blt_fullsize_formatting_on == 1) {
		pctxt->blt_fullsize_formatting_on = 0;
	}

	s3dc_set_blt_in_s3d_mode();
	pctxt->cf_enable = 1;

	s3dc_enable_shadow_reg_up();

}

void s3dc_set_3D_top_bottom_mode(void)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	pctxt->pre_mode	= pctxt->curr_mode;
	pctxt->curr_mode = MODE_S3D_TOP_BOTTOM;
	pctxt->cf_enable = 1;
	pctxt->s3d_type	= 6;
	pctxt->s3d_en = 1;
	pctxt->manual_en = 0;
	pctxt->deci_mode = 2;

	css_s3dc("S3D Mode : TopBottom mode \n");

	pctxt->cf_enable = 0;
	s3dc_disable_shadow_reg_up();

	if (pctxt->display_width > pctxt->cf_out_width) {
		udelay(100);
	}

	/* off address setting */
	s3dc_reg_data.cf_off0_address0_l.as32bits
					= (pctxt->frame_buffer_sm_addr[0] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address0_h.as32bits
					= (pctxt->frame_buffer_sm_addr[0] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address0_l.as32bits
					= (pctxt->frame_buffer_sm_addr[1] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address0_h.as32bits
					= (pctxt->frame_buffer_sm_addr[1] >> 2) >> 16;
	s3dc_reg_data.cf_off0_address1_l.as32bits
					= (pctxt->frame_buffer_sm_addr[2] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address1_h.as32bits
					= (pctxt->frame_buffer_sm_addr[2] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address1_l.as32bits
					= (pctxt->frame_buffer_sm_addr[3] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address1_h.as32bits
					= (pctxt->frame_buffer_sm_addr[3] >> 2) >> 16;

	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_L,
					s3dc_reg_data.cf_off0_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_H,
					s3dc_reg_data.cf_off0_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_L,
					s3dc_reg_data.cf_off1_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_H,
					s3dc_reg_data.cf_off1_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_L,
					s3dc_reg_data.cf_off0_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_H,
					s3dc_reg_data.cf_off0_address1_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_L,
					s3dc_reg_data.cf_off1_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_H,
					s3dc_reg_data.cf_off1_address1_h.as32bits);

	s3dc_reg_data.cf_s3d_fmt.asbits.s3d_type	= pctxt->s3d_type;
	s3dc_reg_data.cf_s3d_fmt.asbits.d2_sel		= pctxt->cam_select;
	s3dc_set_reg32(s3dc_plat->iobase + S3DTYPE_D2_SEL,
						s3dc_reg_data.cf_s3d_fmt.as32bits);

	s3dc_reg_data.cf_enable.as32bits =
					s3dc_get_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN);
	s3dc_reg_data.cf_enable.asbits.s3d_en		= pctxt->s3d_en;
	s3dc_reg_data.cf_enable.asbits.blend_en		= pctxt->manual_en;
	s3dc_reg_data.cf_enable.asbits.deci_mode	= pctxt->deci_mode;
	s3dc_reg_data.cf_enable.asbits.deci_fil_en	= pctxt->deci_filter_en;
	s3dc_set_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN,
				s3dc_reg_data.cf_enable.as32bits);

	s3dc_set_scaler_factor(0, pctxt->cam0_crop_width, pctxt->cam0_crop_height,
							pctxt->cf_out_width, pctxt->cf_out_height / 2,
							pctxt->deci_mode);
	s3dc_set_scaler_factor(1, pctxt->cam1_crop_width, pctxt->cam1_crop_height,
							pctxt->cf_out_width, pctxt->cf_out_height / 2,
							pctxt->deci_mode);

	s3dc_reg_data.cf_wr_condition.as32bits =
						s3dc_get_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN);
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b0 = pctxt->cam0_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b1 = pctxt->cam1_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.cam0_pack_en = pctxt->blti_24bit_en;
	s3dc_reg_data.cf_wr_condition.asbits.cam1_pack_en = pctxt->blti_24bit_en;
	s3dc_set_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN,
						s3dc_reg_data.cf_wr_condition.as32bits);

	if ((pctxt->pre_mode == MODE_S3D_SIDE_BY_SIDE_FULL) ||
		(pctxt->pre_mode == MODE_S3D_TOP_BOTTOM_FULL)) {
		s3dc_change_framebuffer_address(0);
	}

	if (pctxt->display_width > pctxt->cf_out_width) {
		s3dc_enable_shadow_reg_up();
		s3dc_fill_rect();
		s3dc_disable_shadow_reg_up();
	}

	if (pctxt->blt_fullsize_formatting_on == 1) {
		pctxt->blt_fullsize_formatting_on = 0;
	}

	s3dc_set_blt_in_s3d_mode();
	pctxt->cf_enable = 1;

	s3dc_enable_shadow_reg_up();
}

void s3dc_set_3D_top_bottom_mode_full(void)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	pctxt->curr_mode = MODE_S3D_TOP_BOTTOM_FULL;
	pctxt->cf_enable = 1;
	pctxt->s3d_type	= 6;
	pctxt->s3d_en = 1;
	pctxt->manual_en = 0;
	pctxt->deci_mode = 0;

	css_s3dc("S3D Mode : Full TopBottom mode \n");

	pctxt->cf_enable = 0;
	s3dc_disable_shadow_reg_up();

	if (pctxt->display_width > pctxt->cf_out_width) {
		udelay(100);
	}

	/* off address setting */
	s3dc_reg_data.cf_off0_address0_l.as32bits
					= (pctxt->frame_buffer_sm_addr[0] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address0_h.as32bits
					= (pctxt->frame_buffer_sm_addr[0] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address0_l.as32bits
					= (pctxt->frame_buffer_sm_addr[1] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address0_h.as32bits
					= (pctxt->frame_buffer_sm_addr[1] >> 2) >> 16;
	s3dc_reg_data.cf_off0_address1_l.as32bits
					= (pctxt->frame_buffer_sm_addr[2] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address1_h.as32bits
					= (pctxt->frame_buffer_sm_addr[2] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address1_l.as32bits
					= (pctxt->frame_buffer_sm_addr[3] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address1_h.as32bits
					= (pctxt->frame_buffer_sm_addr[3] >> 2) >> 16;

	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_L,
					s3dc_reg_data.cf_off0_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_H,
					s3dc_reg_data.cf_off0_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_L,
					s3dc_reg_data.cf_off1_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_H,
					s3dc_reg_data.cf_off1_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_L,
					s3dc_reg_data.cf_off0_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_H,
					s3dc_reg_data.cf_off0_address1_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_L,
					s3dc_reg_data.cf_off1_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_H,
					s3dc_reg_data.cf_off1_address1_h.as32bits);

	s3dc_reg_data.cf_s3d_fmt.asbits.s3d_type	= pctxt->s3d_type;
	s3dc_reg_data.cf_s3d_fmt.asbits.d2_sel		= pctxt->cam_select;
	s3dc_set_reg32(s3dc_plat->iobase + S3DTYPE_D2_SEL,
						s3dc_reg_data.cf_s3d_fmt.as32bits);

	s3dc_reg_data.cf_enable.as32bits =
					s3dc_get_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN);
	s3dc_reg_data.cf_enable.asbits.s3d_en		= pctxt->s3d_en;
	s3dc_reg_data.cf_enable.asbits.blend_en		= pctxt->manual_en;
	s3dc_reg_data.cf_enable.asbits.deci_mode	= pctxt->deci_mode;
	s3dc_reg_data.cf_enable.asbits.deci_fil_en	= pctxt->deci_filter_en;
	s3dc_set_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN,
				s3dc_reg_data.cf_enable.as32bits);

	s3dc_set_scaler_factor(0, pctxt->cam0_crop_width, pctxt->cam0_crop_height,
				pctxt->cf_out_width, pctxt->cf_out_height, pctxt->deci_mode);
	s3dc_set_scaler_factor(1, pctxt->cam1_crop_width, pctxt->cam1_crop_height,
				pctxt->cf_out_width, pctxt->cf_out_height, pctxt->deci_mode);

	s3dc_reg_data.cf_wr_condition.as32bits =
						s3dc_get_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN);
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b0 = pctxt->cam0_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b1 = pctxt->cam1_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.cam0_pack_en = pctxt->blti_24bit_en;
	s3dc_reg_data.cf_wr_condition.asbits.cam1_pack_en = pctxt->blti_24bit_en;
	s3dc_set_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN,
						s3dc_reg_data.cf_wr_condition.as32bits);

	s3dc_change_framebuffer_address(1);

	if (pctxt->display_width > pctxt->cf_out_width) {
		s3dc_enable_shadow_reg_up();
		s3dc_fill_rect();
		s3dc_disable_shadow_reg_up();
	}

	pctxt->blt_fullsize_formatting_on = 1;

	s3dc_set_blt_in_s3d_mode();

	pctxt->cf_enable = 1;
	s3dc_enable_shadow_reg_up();
}

void s3dc_set_3D_side_by_side_mode_full(void)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	pctxt->curr_mode = MODE_S3D_SIDE_BY_SIDE_FULL;
	pctxt->cf_enable = 1;
	pctxt->s3d_type	= 5;
	pctxt->s3d_en = 1;
	pctxt->manual_en = 0;
	pctxt->deci_mode = 0; /* must be 0 */

	css_s3dc("S3D Mode : Full SideBySide mode \n");

	pctxt->cf_enable = 0;
	s3dc_disable_shadow_reg_up();

	if (pctxt->display_width > (pctxt->cf_out_width * 2))
		udelay(100);

	/* off address setting */
	s3dc_reg_data.cf_off0_address0_l.as32bits
					= (pctxt->frame_buffer_sm_addr[0] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address0_h.as32bits
					= (pctxt->frame_buffer_sm_addr[0] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address0_l.as32bits
					= (pctxt->frame_buffer_sm_addr[1] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address0_h.as32bits
					= (pctxt->frame_buffer_sm_addr[1] >> 2) >> 16;
	s3dc_reg_data.cf_off0_address1_l.as32bits
					= (pctxt->frame_buffer_sm_addr[2] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off0_address1_h.as32bits
					= (pctxt->frame_buffer_sm_addr[2] >> 2) >> 16;
	s3dc_reg_data.cf_off1_address1_l.as32bits
					= (pctxt->frame_buffer_sm_addr[3] >> 2) & 0xFFFF;
	s3dc_reg_data.cf_off1_address1_h.as32bits
					= (pctxt->frame_buffer_sm_addr[3] >> 2) >> 16;

	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_L,
					s3dc_reg_data.cf_off0_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS0_H,
					s3dc_reg_data.cf_off0_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_L,
					s3dc_reg_data.cf_off1_address0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS0_H,
					s3dc_reg_data.cf_off1_address0_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_L,
					s3dc_reg_data.cf_off0_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF0_ADDRESS1_H,
					s3dc_reg_data.cf_off0_address1_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_L,
					s3dc_reg_data.cf_off1_address1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + OFF1_ADDRESS1_H,
					s3dc_reg_data.cf_off1_address1_h.as32bits);

	s3dc_reg_data.cf_s3d_fmt.asbits.s3d_type	= pctxt->s3d_type;
	s3dc_reg_data.cf_s3d_fmt.asbits.d2_sel		= pctxt->cam_select;
	s3dc_set_reg32(s3dc_plat->iobase + S3DTYPE_D2_SEL,
						s3dc_reg_data.cf_s3d_fmt.as32bits);

	s3dc_reg_data.cf_enable.as32bits =
					s3dc_get_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN);
	s3dc_reg_data.cf_enable.asbits.s3d_en		= pctxt->s3d_en;
	s3dc_reg_data.cf_enable.asbits.blend_en		= pctxt->manual_en;
	s3dc_reg_data.cf_enable.asbits.deci_mode	= pctxt->deci_mode;
	s3dc_reg_data.cf_enable.asbits.deci_fil_en	= pctxt->deci_filter_en;
	s3dc_set_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN,
				s3dc_reg_data.cf_enable.as32bits);

	s3dc_set_scaler_factor(0, pctxt->cam0_crop_width, pctxt->cam0_crop_height,
				pctxt->cf_out_width, pctxt->cf_out_height, pctxt->deci_mode);
	s3dc_set_scaler_factor(1, pctxt->cam1_crop_width, pctxt->cam1_crop_height,
				pctxt->cf_out_width, pctxt->cf_out_height, pctxt->deci_mode);

	s3dc_reg_data.cf_wr_condition.as32bits =
						s3dc_get_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN);
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b0 = pctxt->cam0_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.wr_16b1 = pctxt->cam1_wr_16bit;
	s3dc_reg_data.cf_wr_condition.asbits.cam0_pack_en = pctxt->blti_24bit_en;
	s3dc_reg_data.cf_wr_condition.asbits.cam1_pack_en = pctxt->blti_24bit_en;
	s3dc_set_reg32(s3dc_plat->iobase + PIXEL_FORMAT_CF_EN,
						s3dc_reg_data.cf_wr_condition.as32bits);

	s3dc_change_framebuffer_address(1);

	if (pctxt->display_width > (pctxt->cf_out_width * 2)) {
		s3dc_enable_shadow_reg_up();
		s3dc_fill_rect();
		s3dc_disable_shadow_reg_up();
	}

	pctxt->blt_fullsize_formatting_on = 1;

	s3dc_set_blt_in_s3d_mode();

	pctxt->cf_enable = 1;
	s3dc_enable_shadow_reg_up();
}

/* 2D mode에서 camera 변경 함수 (0 or 1) */
void s3dc_select_camera_2D_mode(int camera)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	css_s3dc("Camera Selection = %d\n", camera);

	pctxt->cam_select = camera;

	s3dc_disable_shadow_reg_up();

	s3dc_reg_data.cf_s3d_fmt.asbits.s3d_type = pctxt->s3d_type;
	s3dc_reg_data.cf_s3d_fmt.asbits.d2_sel = pctxt->cam_select;
	s3dc_set_reg32(s3dc_plat->iobase + S3DTYPE_D2_SEL,
						s3dc_reg_data.cf_s3d_fmt.as32bits);
	s3dc_enable_shadow_reg_up();
	css_s3dc("Camera Selection = %d (reg5001 = 0x%x)\n", camera,
					s3dc_get_reg32(s3dc_plat->iobase + S3DTYPE_D2_SEL));

}

void s3dc_set_pixel_start(int manual_select, int manual_p_start)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	s3dc_disable_shadow_reg_up();

	if (manual_select == 0) {
		s3dc_reg_data.cf_crop_pixel_start_0.as32bits = manual_p_start;
		s3dc_set_reg32(s3dc_plat->iobase + PIXEL_START_0, manual_p_start);
	} else {
		s3dc_reg_data.cf_crop_pixel_start_1.as32bits = manual_p_start;
		s3dc_set_reg32(s3dc_plat->iobase + PIXEL_START_1, manual_p_start);
	}

	s3dc_enable_shadow_reg_up();

	css_s3dc("Crop Start Point : Cam0-> (%d , %d) Cam1-> (%d , %d)\n",
			s3dc_get_reg32(s3dc_plat->iobase + PIXEL_START_0),
			s3dc_get_reg32(s3dc_plat->iobase + LINE_START_0),
			s3dc_get_reg32(s3dc_plat->iobase + PIXEL_START_1),
			s3dc_get_reg32(s3dc_plat->iobase + LINE_START_1));
	css_s3dc("Crop Size : Cam0-> %d x %d Cam1-> %d x %d \n",
			s3dc_get_reg32(s3dc_plat->iobase + PIXEL_COUNT_0),
			s3dc_get_reg32(s3dc_plat->iobase + LINE_COUNT_0),
			s3dc_get_reg32(s3dc_plat->iobase + PIXEL_COUNT_1),
			s3dc_get_reg32(s3dc_plat->iobase + LINE_COUNT_1));
}

int s3dc_get_pixel_start(int manual_select)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	if (manual_select == 0) {
		return s3dc_get_reg32(s3dc_plat->iobase + AC_P_START0_D);
	} else {
		return s3dc_get_reg32(s3dc_plat->iobase + AC_P_START1_D);
	}
}

void s3dc_set_pixel_start_offset(int manual_select, int offset)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	if (manual_select == 0) {
		s3dc_reg_data.cf_p_start_offset0.as32bits = offset & 0x3fff;
		s3dc_set_reg32(s3dc_plat->iobase + PSTART_OFFSET0,
			s3dc_reg_data.cf_p_start_offset0.as32bits); /*14bit(signed int)*/
		css_s3dc("Cam0 p_start offset : %d (reg = 0x%x)\n", offset,
						s3dc_get_reg32(s3dc_plat->iobase + PSTART_OFFSET0));
	} else {
		s3dc_reg_data.cf_p_start_offset1.as32bits = offset & 0x3fff;
		s3dc_set_reg32(s3dc_plat->iobase + PSTART_OFFSET1,
			s3dc_reg_data.cf_p_start_offset1.as32bits); /*14bit(signed int)*/
		css_s3dc("Cam1 p_start offset : %d (reg = 0x%x)\n", offset,
						s3dc_get_reg32(s3dc_plat->iobase + PSTART_OFFSET1));
	}
}

void s3dc_set_line_start(int manual_select, int manual_l_start)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	s3dc_disable_shadow_reg_up();

	if (manual_select == 0) {
		pctxt->cam0_line_start = manual_l_start;
		s3dc_reg_data.cf_crop_line_start_0.as32bits = manual_l_start;
		s3dc_set_reg32(s3dc_plat->iobase + LINE_START_0, manual_l_start);
	} else {
		pctxt->cam1_line_start = manual_l_start;
		s3dc_reg_data.cf_crop_line_start_1.as32bits = manual_l_start;
		s3dc_set_reg32(s3dc_plat->iobase + LINE_START_1, manual_l_start);
	}

	s3dc_enable_shadow_reg_up();

	css_s3dc("Crop Start Point : Cam0-> (%d , %d) Cam1-> (%d , %d)\n",
					s3dc_get_reg32(s3dc_plat->iobase + PIXEL_START_0),
					s3dc_get_reg32(s3dc_plat->iobase + LINE_START_0),
					s3dc_get_reg32(s3dc_plat->iobase + PIXEL_START_1),
					s3dc_get_reg32(s3dc_plat->iobase + LINE_START_1));
	css_s3dc("Crop Size : Cam0-> %d x %d Cam1-> %d x %d \n",
					s3dc_get_reg32(s3dc_plat->iobase + PIXEL_COUNT_0),
					s3dc_get_reg32(s3dc_plat->iobase + LINE_COUNT_0),
					s3dc_get_reg32(s3dc_plat->iobase + PIXEL_COUNT_1),
					s3dc_get_reg32(s3dc_plat->iobase + LINE_COUNT_1));
}

int s3dc_get_line_start(int manual_select)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	if (manual_select == 0) {
		return s3dc_get_reg32(s3dc_plat->iobase + LINE_START_0);
	} else {
		return s3dc_get_reg32(s3dc_plat->iobase + LINE_START_1);
	}
}

int s3dc_get_pixel_count(int manual_select)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	if (manual_select == 0) {
		return s3dc_get_reg32(s3dc_plat->iobase + PIXEL_COUNT_0);
	} else {
		return s3dc_get_reg32(s3dc_plat->iobase + PIXEL_COUNT_1);
	}
}

int s3dc_get_line_count(int manual_select)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	if (manual_select == 0) {
		return s3dc_get_reg32(s3dc_plat->iobase + LINE_COUNT_0);
	} else {
		return s3dc_get_reg32(s3dc_plat->iobase + LINE_COUNT_1);
	}
}

int s3dc_get_camera_width(int manual_select)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	if (manual_select == 0) {
		return pctxt->cam0_width;
	} else {
		return pctxt->cam1_width;
	}
}

int s3dc_get_camera_height(int manual_select)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	if (manual_select == 0) {
		return pctxt->cam0_height;
	} else {
		return pctxt->cam1_height;
	}
}

void s3dc_set_scaler_factor(int cam_sel, int src_w, int src_h, int dst_w,
							int dst_h, int deci_mode)
{
	int ratio_w, ratio_h;
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	if (cam_sel == 0)
		src_w -= pctxt->cam0_crop_w_correction;
	else
		src_w -= pctxt->cam1_crop_w_correction;

	if (deci_mode == 0) {
		s3dc_reg_data.cf_rot_deci_block.as32bits = 0;
		s3dc_set_reg32(s3dc_plat->iobase + ROT_DECI_BLOCK, 0);
	} else if (deci_mode == 1) {
		if ((pctxt->rotate_en) && (cam_sel == 0)) {
			src_w = src_w;
			s3dc_reg_data.cf_rot_deci_block.as32bits = 1;
			s3dc_set_reg32(s3dc_plat->iobase + ROT_DECI_BLOCK, 1);
		} else if ((pctxt->rotate_en) && (cam_sel == 1)) {
			src_w = src_w / 2;
			s3dc_reg_data.cf_rot_deci_block.as32bits = 1;
			s3dc_set_reg32(s3dc_plat->iobase + ROT_DECI_BLOCK, 1);
		} else {
			s3dc_reg_data.cf_rot_deci_block.as32bits = 0;
			s3dc_set_reg32(s3dc_plat->iobase + ROT_DECI_BLOCK, 0);
			src_w = src_w / 2;
		}
	} else if (deci_mode == 2) {
		if ((pctxt->rotate_en) && (cam_sel == 0)) {
			s3dc_reg_data.cf_rot_deci_block.as32bits = 1;
			s3dc_set_reg32(s3dc_plat->iobase + ROT_DECI_BLOCK, 1);
			src_h = src_h;
		} else if ((pctxt->rotate_en) && (cam_sel == 1)) {
			s3dc_reg_data.cf_rot_deci_block.as32bits = 1;
			s3dc_set_reg32(s3dc_plat->iobase + ROT_DECI_BLOCK, 1);
			src_h = src_h / 2;
		} else {
			s3dc_reg_data.cf_rot_deci_block.as32bits = 0;
			s3dc_set_reg32(s3dc_plat->iobase + ROT_DECI_BLOCK, 0);
			src_h = src_h / 2;
		}
	} else if (deci_mode == 3) {
		s3dc_reg_data.cf_rot_deci_block.as32bits = 0;
		s3dc_set_reg32(s3dc_plat->iobase + ROT_DECI_BLOCK, 0);
		src_w = src_w / 2;
		src_h = src_h / 2;
	}

	if ((src_w / dst_w) < 1) {
		 ratio_w = (int)((unsigned int)((src_w - 1) * 100000)
		 		/ dst_w * 256) / 100000;
	} else {
		ratio_w = (int)((((src_w * 100000) / dst_w) * 256 + 50000) / 100000);
	}

	if ((src_h / dst_h) < 1) {
		ratio_h = (int)((unsigned int)((src_h) * 100000)
				/ dst_h * 256) / 100000;
	} else {
		ratio_h = (int)((((src_h * 100000) / dst_h) * 256 + 50000) / 100000);
	}

	if (cam_sel == 0) {
		if (ratio_w == 256 && pctxt->cam0_crop_w_correction != 0)
			ratio_w = 0xFF;

		s3dc_reg_data.cf_scale_ws_wd0.asbits.ws_wd0 = ratio_w;
		s3dc_reg_data.cf_scale_hs_hd0.asbits.hs_hd0 = ratio_h;

		s3dc_set_reg32(s3dc_plat->iobase + WS_WD, ratio_w);
		s3dc_set_reg32(s3dc_plat->iobase + HS_HD, ratio_h);
	} else if (cam_sel == 1) {
		if (ratio_w == 256 && pctxt->cam1_crop_w_correction != 0)
			ratio_w = 0xFF;

		s3dc_reg_data.cf_scale_ws_wd1.asbits.ws_wd1 = ratio_w;
		s3dc_reg_data.cf_scale_hs_hd1.asbits.hs_hd1 = ratio_h;

		s3dc_set_reg32(s3dc_plat->iobase + WS_WD1, ratio_w);
		s3dc_set_reg32(s3dc_plat->iobase + HS_HD1, ratio_h);
	}

	css_s3dc("3d set scale src_w:%d src_h:%d dst_w:%d dst_h:%d\n",
				src_w, src_h, dst_w, dst_h);
	css_s3dc("3d scale factor rw:%d rh:%d\n", ratio_w, ratio_h);
}

void s3dc_change_lr(int change_lr)
{
	int get_val;
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	pctxt->change_lr = change_lr;

	s3dc_reg_data.cf_cam_lr_change.as32bits = change_lr;
	s3dc_set_reg32(s3dc_plat->iobase + LR_CHANGE, change_lr);

	get_val = s3dc_get_reg32(s3dc_plat->iobase + LR_CHANGE);

	if (get_val) {
		css_s3dc("Left-Right Changed (Reg : %d)\n", get_val);
	} else {
		css_s3dc("Left-Right Not Changed (Reg : %d)\n", get_val);
	}
}

void s3dc_enable_cf_filter(int filter_on)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	pctxt->deci_filter_en = filter_on;

	s3dc_disable_shadow_reg_up();

	s3dc_reg_data.cf_enable.as32bits =
					s3dc_get_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN);
	s3dc_reg_data.cf_enable.asbits.s3d_en		= pctxt->s3d_en;
	s3dc_reg_data.cf_enable.asbits.blend_en		= pctxt->manual_en;
	s3dc_reg_data.cf_enable.asbits.deci_mode	= pctxt->deci_mode;
	s3dc_reg_data.cf_enable.asbits.deci_fil_en	= pctxt->deci_filter_en;
	s3dc_set_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN,
				s3dc_reg_data.cf_enable.as32bits);

	s3dc_enable_shadow_reg_up();

	if (filter_on) {
		css_s3dc("CF filter Enabled (readVal = %d)\n",
			(s3dc_get_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN) >> 4)
					& 0x1);
	} else {
		css_s3dc("CF filter Disabled (readVal = %d)\n",
			(s3dc_get_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN) >> 4)
					& 0x1);
	}
}

void s3dc_set_tilt_value(unsigned int tiltbypass_enable, unsigned int tan_val)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	css_s3dc("[S3DC]TILT bypass %d tanval %d\n", tiltbypass_enable, tan_val);

	s3dc_reg_data.cf_tilt_tangent.as32bits = (tan_val & 0xffff);
	s3dc_set_reg32(s3dc_plat->iobase + TANGENT,
					s3dc_reg_data.cf_tilt_tangent.as32bits);

	/* tilt0.0이면 tiltBypass = 1으로 셋팅해야 한다. */
	if (tiltbypass_enable) {/* tilt 0.0 */
		pctxt->tilt_bypass_en = 1;
	} else {
		pctxt->tilt_bypass_en = 0;
	}

	s3dc_reg_data.cf_tilt_bypass.asbits.tilt_bypass = pctxt->tilt_bypass_en;
	s3dc_set_reg32(s3dc_plat->iobase + TILT_BYPASS,
					s3dc_reg_data.cf_tilt_bypass.as32bits);
}

void s3dc_change_deci_mode(int deci)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	pctxt->deci_mode = deci;

	s3dc_disable_shadow_reg_up();

	if ((pctxt->curr_mode == MODE_S3D_LINE) ||
		(pctxt->curr_mode == MODE_S3D_TOP_BOTTOM)) {
		s3dc_set_scaler_factor(0, pctxt->cam0_crop_width,
				pctxt->cam0_crop_height, pctxt->cf_out_width,
				pctxt->cf_out_height / 2, pctxt->deci_mode);
		s3dc_set_scaler_factor(1, pctxt->cam1_crop_width,
				pctxt->cam1_crop_height, pctxt->cf_out_width,
				pctxt->cf_out_height / 2, pctxt->deci_mode);
	} else if (pctxt->curr_mode == MODE_S3D_SIDE_BY_SIDE) {
		s3dc_set_scaler_factor(0, pctxt->cam0_crop_width,
				pctxt->cam0_crop_height, pctxt->cf_out_width / 2,
				pctxt->cf_out_height, pctxt->deci_mode);
		s3dc_set_scaler_factor(1, pctxt->cam1_crop_width,
				pctxt->cam1_crop_height, pctxt->cf_out_width / 2,
				pctxt->cf_out_height, pctxt->deci_mode);
	} else {
		s3dc_set_scaler_factor(0, pctxt->cam0_crop_width,
				pctxt->cam0_crop_height, pctxt->cf_out_width,
				pctxt->cf_out_height, pctxt->deci_mode);
		s3dc_set_scaler_factor(1, pctxt->cam1_crop_width,
				pctxt->cam1_crop_height, pctxt->cf_out_width,
				pctxt->cf_out_height, pctxt->deci_mode);
	}

	s3dc_reg_data.cf_enable.asbits.s3d_en		= pctxt->s3d_en;
	s3dc_reg_data.cf_enable.asbits.blend_en		= pctxt->manual_en;
	s3dc_reg_data.cf_enable.asbits.deci_mode	= pctxt->deci_mode;
	s3dc_reg_data.cf_enable.asbits.deci_fil_en	= pctxt->deci_filter_en;
	s3dc_set_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN,
						s3dc_reg_data.cf_enable.as32bits);

	s3dc_enable_shadow_reg_up();

	css_s3dc("Current Deci-Mode = 0x%x\n",
		(s3dc_get_reg32(s3dc_plat->iobase + DECI_MODE_MANUAL_S3D_EN) >> 2)
		& 0x3);
}

/*------------------------------------------------------------------------------
 * AC, AET Functions
 *----------------------------------------------------------------------------*/
void s3dc_set_auto_conv_area(int x_start, int y_start, int width, int height)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	s3dc_reg_data.ac_region_y.as32bits		= y_start;
	s3dc_reg_data.ac_region_x.as32bits		= x_start;
	s3dc_reg_data.ac_region_height.as32bits = height;
	s3dc_reg_data.ac_region_width.as32bits	= width;

	s3dc_set_reg32(s3dc_plat->iobase + AC_Y_START, y_start);
	s3dc_set_reg32(s3dc_plat->iobase + AC_X_START, x_start);
	s3dc_set_reg32(s3dc_plat->iobase + AC_HEIGHT, height);
	s3dc_set_reg32(s3dc_plat->iobase + AC_WIDTH, width);

	css_s3dc("Current Area -> X_start:%d  Y_start:%d  Width:%d  Height:%d\n",
		s3dc_get_reg32(s3dc_plat->iobase + AC_X_START),
		s3dc_get_reg32(s3dc_plat->iobase + AC_Y_START),
		s3dc_get_reg32(s3dc_plat->iobase + AC_WIDTH),
		s3dc_get_reg32(s3dc_plat->iobase + AC_HEIGHT));
}

/* auto convergence on/off */
void s3dc_enable_auto_convergence(int is_enable)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	css_s3dc("Auto-convergence setting\n");

	pctxt->ac_enable = is_enable;

	if (pctxt->ac_quick_enable)
		s3dc_enable_ac_auto_start_quick(0);

	s3dc_reg_data.ac_dp_th_step.as32bits = (32 << 2) | (1 << 1) | (0 << 0);
	s3dc_set_reg32(s3dc_plat->iobase + AC_DP_TH_STEP,
					s3dc_reg_data.ac_dp_th_step.as32bits);

	s3dc_reg_data.ac_auto_enable.as32bits = pctxt->ac_enable;
	s3dc_set_reg32(s3dc_plat->iobase + AC_AUTO_ENABLE, pctxt->ac_enable);

	s3dc_reg_data.ac_once_enable.as32bits = pctxt->ac_enable;
	s3dc_set_reg32(s3dc_plat->iobase + AC_EN_ONE, pctxt->ac_enable);

	if (is_enable) {
		css_s3dc("AutoConvergence Enabled\n");
	} else {
		css_s3dc("AutoConvergence Disabled\n");
	}
}

void s3dc_enable_ac_auto_start_quick(int enable)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	if (pctxt->ac_enable)
		s3dc_enable_auto_convergence(0);

	pctxt->ac_quick_enable = enable;
	if (enable) {
		s3dc_reg_data.cf_dp_step.as32bits = 0xff;
		s3dc_set_reg32(s3dc_plat->iobase + DP_STEP, 0xff);
	} else {
		s3dc_reg_data.cf_dp_step.as32bits = 0x00;
		s3dc_set_reg32(s3dc_plat->iobase + DP_STEP, 0x00);
	}

	s3dc_reg_data.ac_dp_th_step.as32bits = (32 << 2) | (1 << 1) | (0 << 0);
	s3dc_set_reg32(s3dc_plat->iobase + AC_DP_TH_STEP,
					s3dc_reg_data.ac_dp_th_step.as32bits);

	s3dc_reg_data.ac_auto_enable.as32bits = pctxt->ac_quick_enable;
	s3dc_set_reg32(s3dc_plat->iobase + AC_AUTO_ENABLE, pctxt->ac_quick_enable);

	css_s3dc("AC_AUTO_START enable : 0x%x\n",
					s3dc_get_reg32(s3dc_plat->iobase + AC_AUTO_ENABLE));
}

void s3dc_set_tolerance_val(int tol_val)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	pctxt->ac_edge_tol = tol_val;

	s3dc_reg_data.ac_edge_th.as32bits = tol_val;
	s3dc_set_reg32(s3dc_plat->iobase + AC_TAL, tol_val);

	css_s3dc("tolerance = %d \n", s3dc_get_reg32(s3dc_plat->iobase + AC_TAL));
}

void s3dc_set_acdp_redun(int data)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	/* AC_DP_REDUN [5:0]
	if ([5] == 0) dp = dp + [4:0]
	else if ([5] == 1) dp = dp - [4:0]
	*/
	s3dc_reg_data.ac_dp_offset.as32bits = data;
	s3dc_set_reg32(s3dc_plat->iobase + AC_DP_REDUN, data);

	css_s3dc("AC_DP_REDUN : 0x%x\n",
			s3dc_get_reg32(s3dc_plat->iobase + AC_DP_REDUN));
}

void s3dc_change_dp_step(int dp_step)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	/* DP_STEP 이 8bit로 확장됨. */
	s3dc_reg_data.cf_dp_step.as32bits = dp_step;
	s3dc_set_reg32(s3dc_plat->iobase + DP_STEP, dp_step);

	css_s3dc("Current DP_STEP : 0x%x\n",
			s3dc_get_reg32(s3dc_plat->iobase + DP_STEP));
}

/*------------------------------------------------------------------------------
 * Auto Luminance Balance Functions
 *----------------------------------------------------------------------------*/
void s3dc_enable_auto_luminance_balance(int alb_on)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	pctxt->wb_enable = alb_on;
	/* 안정적인 wb를 위해 wb enable시 hwb,autostopen 도 함께 enable한다. */
	if (alb_on) {
		pctxt->hwb_enable = 1;
		pctxt->wb_auto_stop_en = 1;
	} else {
		pctxt->hwb_enable = 0;
		pctxt->wb_auto_stop_en = 0;
		pctxt->wb_hold = 0;
	}

	s3dc_reg_data.wb_set_ctrl.asbits.wb_en			 = pctxt->wb_enable;
	s3dc_reg_data.wb_set_ctrl.asbits.hwb_en 		 = pctxt->hwb_enable;
	s3dc_reg_data.wb_set_ctrl.asbits.wb_hold		 = pctxt->wb_hold;
	s3dc_reg_data.wb_set_ctrl.asbits.wb_auto_stop_en = pctxt->wb_auto_stop_en;
	s3dc_set_reg32(s3dc_plat->iobase + WB_ENABLES,
					s3dc_reg_data.wb_set_ctrl.as32bits);

	if (alb_on) {
		css_s3dc("WB Enabled = 0x%x\n",
				s3dc_get_reg32(s3dc_plat->iobase + WB_ENABLES));
	} else {
		css_s3dc("WB Disabled = 0x%x\n",
				s3dc_get_reg32(s3dc_plat->iobase + WB_ENABLES));
	}
}

/*------------------------------------------------------------------------------
 * Color Control Functions
 *----------------------------------------------------------------------------*/
void s3dc_enable_color_pattern(int pattern_on)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	pctxt->color_pattern_en = pattern_on;

	s3dc_disable_shadow_reg_up();

	s3dc_reg_data.color_con_en.asbits.cam0_color_con_en
										= pctxt->color_ctrl_en[0];
	s3dc_reg_data.color_con_en.asbits.cam1_color_con_en
										= pctxt->color_ctrl_en[1];
	s3dc_reg_data.color_con_en.asbits.pattern_en
										= pctxt->color_pattern_en;
	s3dc_reg_data.color_con_en.asbits.no_color_en
										= pctxt->color_no_color_en;

	s3dc_set_reg32(s3dc_plat->iobase + COLOR_PATTERN_EN_CON_EN,
					s3dc_reg_data.color_con_en.as32bits);

	s3dc_enable_shadow_reg_up();
	css_s3dc("Color Test Pattern : %d (reg = 0x%x)\n", pattern_on,
			s3dc_get_reg32(s3dc_plat->iobase + COLOR_PATTERN_EN_CON_EN));
}

void s3dc_enable_color_control(int cam_sel, int colorConOn)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	pctxt->color_ctrl_en[cam_sel] = colorConOn;

	s3dc_disable_shadow_reg_up();

	s3dc_reg_data.color_con_en.asbits.cam0_color_con_en
										= pctxt->color_ctrl_en[0];
	s3dc_reg_data.color_con_en.asbits.cam1_color_con_en
										= pctxt->color_ctrl_en[1];
	s3dc_reg_data.color_con_en.asbits.pattern_en
										= pctxt->color_pattern_en;
	s3dc_reg_data.color_con_en.asbits.no_color_en
										= pctxt->color_no_color_en;

	s3dc_set_reg32(s3dc_plat->iobase + COLOR_PATTERN_EN_CON_EN,
					s3dc_reg_data.color_con_en.as32bits);


	s3dc_enable_shadow_reg_up();

	css_s3dc("Color Control[%d] on/off : %d (reg = 0x%x)\n",
				cam_sel, colorConOn,
				s3dc_get_reg32(s3dc_plat->iobase + COLOR_PATTERN_EN_CON_EN));
}

void s3dc_set_color_gains(int regSel, int cam_sel, int gain_0, int gain_1)
{
	int cGain, yGain, lcGain, hcGain, lyGain, hyGain, oGain, bright;
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	s3dc_disable_shadow_reg_up();

	switch (regSel) {
		case COLOR_Y_GAIN:
			cGain = gain_0;
			yGain = gain_1;
			if (!cam_sel) {
				s3dc_reg_data.cam0_gain.as32bits = (yGain << 8) | (cGain);
				s3dc_set_reg32(s3dc_plat->iobase + CGAIN_YGAIN_CAM0,
								s3dc_reg_data.cam0_gain.as32bits);
			} else {
				s3dc_reg_data.cam1_gain.as32bits = (yGain << 8) | (cGain);
				s3dc_set_reg32(s3dc_plat->iobase + CGAIN_YGAIN_CAM1,
								s3dc_reg_data.cam1_gain.as32bits);
			}
			break;
		case COLOR_C_GAIN:
			cGain = gain_0;
			yGain = gain_1;
			if (!cam_sel) {
				s3dc_reg_data.cam0_gain.as32bits = (yGain << 8) | (cGain);
				s3dc_set_reg32(s3dc_plat->iobase + CGAIN_YGAIN_CAM0,
								s3dc_reg_data.cam0_gain.as32bits);
			} else {
				s3dc_reg_data.cam1_gain.as32bits = (yGain << 8) | (cGain);
				s3dc_set_reg32(s3dc_plat->iobase + CGAIN_YGAIN_CAM1,
								s3dc_reg_data.cam1_gain.as32bits);
			}
			break;
		case COLOR_LC_GAIN:
			lcGain = gain_0;
			hcGain = gain_1;
			if (!cam_sel) {
				s3dc_reg_data.cam0_cgain.as32bits = (hcGain << 8) | (lcGain);
				s3dc_set_reg32(s3dc_plat->iobase + LCGAIN_HCGAIN_CAM0,
								s3dc_reg_data.cam0_cgain.as32bits);
			} else {
				s3dc_reg_data.cam1_cgain.as32bits = (hcGain << 8) | (lcGain);
				s3dc_set_reg32(s3dc_plat->iobase + LCGAIN_HCGAIN_CAM1,
								s3dc_reg_data.cam1_cgain.as32bits);
			}
			break;
		case COLOR_HC_GAIN:
			lcGain = gain_0;
			hcGain = gain_1;
			if (!cam_sel) {
				s3dc_reg_data.cam0_cgain.as32bits = (hcGain << 8) | (lcGain);
				s3dc_set_reg32(s3dc_plat->iobase + LCGAIN_HCGAIN_CAM0,
								s3dc_reg_data.cam0_cgain.as32bits);
			} else {
				s3dc_reg_data.cam1_cgain.as32bits = (hcGain << 8) | (lcGain);
				s3dc_set_reg32(s3dc_plat->iobase + LCGAIN_HCGAIN_CAM1,
								s3dc_reg_data.cam1_cgain.as32bits);
			}
			break;
		case COLOR_LY_GAIN:
			lyGain = gain_0;
			hyGain = gain_1;
			if (!cam_sel) {
				s3dc_reg_data.cam0_ygain.as32bits = (hyGain << 8) | (lyGain);
				s3dc_set_reg32(s3dc_plat->iobase + LYGAIN_HYGAIN_CAM0,
								s3dc_reg_data.cam0_ygain.as32bits);
			} else {
				s3dc_reg_data.cam1_ygain.as32bits = (hyGain << 8) | (lyGain);
				s3dc_set_reg32(s3dc_plat->iobase + LYGAIN_HYGAIN_CAM1,
								s3dc_reg_data.cam1_ygain.as32bits);
			}
			break;
		case COLOR_HY_GAIN:
			lyGain = gain_0;
			hyGain = gain_1;
			if (!cam_sel) {
				s3dc_reg_data.cam0_ygain.as32bits = (hyGain << 8) | (lyGain);
				s3dc_set_reg32(s3dc_plat->iobase + LYGAIN_HYGAIN_CAM0,
								s3dc_reg_data.cam0_ygain.as32bits);
			} else {
				s3dc_reg_data.cam1_ygain.as32bits = (hyGain << 8) | (lyGain);
				s3dc_set_reg32(s3dc_plat->iobase + LYGAIN_HYGAIN_CAM1,
								s3dc_reg_data.cam1_ygain.as32bits);
			}
			break;
		case COLOR_O_GAIN:
			oGain = gain_0;
			bright = gain_1;
			if (!cam_sel) {
				s3dc_reg_data.cam0_rgb_bright_ogain.as32bits
								= ((oGain << 8) | bright) & 0x7f;
				s3dc_set_reg32(s3dc_plat->iobase + OGAIN_BRIGHT_CAM0,
								s3dc_reg_data.cam0_rgb_bright_ogain.as32bits);
			} else {
				s3dc_reg_data.cam1_rgb_bright_ogain.as32bits
								= ((oGain << 8) | bright) & 0x7f;
				s3dc_set_reg32(s3dc_plat->iobase + OGAIN_BRIGHT_CAM1,
								s3dc_reg_data.cam1_rgb_bright_ogain.as32bits);
			}
			break;
	}
	s3dc_enable_shadow_reg_up();
}

void s3dc_set_color_bright(int regSel, int cam_sel, unsigned int value)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	s3dc_disable_shadow_reg_up();

	switch (regSel) {
	case L_COLOR_THRESHOLD:
		if (!cam_sel) {
			s3dc_reg_data.cam0_color_th.as32bits = value;
			s3dc_set_reg32(s3dc_plat->iobase + HCOLOR_LCOLOR_TH_CAM0,
							s3dc_reg_data.cam0_color_th.as32bits);
		} else {
			s3dc_reg_data.cam1_color_th.as32bits = value;
			s3dc_set_reg32(s3dc_plat->iobase + HCOLOR_LCOLOR_TH_CAM1,
							s3dc_reg_data.cam1_color_th.as32bits);
		}
		break;
	case H_COLOR_THRESHOLD:
		if (!cam_sel) {
			s3dc_reg_data.cam0_color_th.as32bits = value;
			s3dc_set_reg32(s3dc_plat->iobase + HCOLOR_LCOLOR_TH_CAM0,
							s3dc_reg_data.cam0_color_th.as32bits);
		} else {
			s3dc_reg_data.cam1_color_th.as32bits = value;
			s3dc_set_reg32(s3dc_plat->iobase + HCOLOR_LCOLOR_TH_CAM1,
							s3dc_reg_data.cam1_color_th.as32bits);
		}
		break;
	case L_BRIGHT_THRESHOLD:
		if (!cam_sel) {
			s3dc_reg_data.cam0_bright_th.as32bits = value;
			s3dc_set_reg32(s3dc_plat->iobase + LBRIGHT_HBRIGHT_TH_CAM0,
							s3dc_reg_data.cam0_bright_th.as32bits);
		} else {
			s3dc_reg_data.cam1_bright_th.as32bits = value;
			s3dc_set_reg32(s3dc_plat->iobase + LBRIGHT_HBRIGHT_TH_CAM1,
							s3dc_reg_data.cam1_bright_th.as32bits);
		}
		break;
	case H_BRIGHT_THRESHOLD:
		if (!cam_sel) {
			s3dc_reg_data.cam0_bright_th.as32bits = value;
			s3dc_set_reg32(s3dc_plat->iobase + LBRIGHT_HBRIGHT_TH_CAM0,
							s3dc_reg_data.cam0_bright_th.as32bits);
		} else {
			s3dc_reg_data.cam1_bright_th.as32bits = value;
			s3dc_set_reg32(s3dc_plat->iobase + LBRIGHT_HBRIGHT_TH_CAM1,
							s3dc_reg_data.cam1_bright_th.as32bits);
		}
		break;
	case COLOR_HUE:
		if (!cam_sel) {
			s3dc_reg_data.cam0_hue_degree.as32bits = value;
			s3dc_set_reg32(s3dc_plat->iobase + COLOR_HUE_DEGREE_CAM0,
							s3dc_reg_data.cam0_hue_degree.as32bits);
		} else {
			s3dc_reg_data.cam1_hue_degree.as32bits = value;
			s3dc_set_reg32(s3dc_plat->iobase + COLOR_HUE_DEGREE_CAM1,
							s3dc_reg_data.cam1_hue_degree.as32bits);
		}
		break;
	case COLOR_BRIGHT:
		if (!cam_sel) {
			s3dc_reg_data.cam0_rgb_bright_ogain.as32bits = value;
			s3dc_set_reg32(s3dc_plat->iobase + OGAIN_BRIGHT_CAM0,
							s3dc_reg_data.cam0_rgb_bright_ogain.as32bits);
		} else {
			s3dc_reg_data.cam1_rgb_bright_ogain.as32bits = value;
			s3dc_set_reg32(s3dc_plat->iobase + OGAIN_BRIGHT_CAM1,
							s3dc_reg_data.cam1_rgb_bright_ogain.as32bits);
		}
		break;
	}
	s3dc_enable_shadow_reg_up();
}

void s3dc_set_coeff(struct s3dc_coeff_set *coeff_set)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();
	int i;

	for (i = 0; i < 9; i++) {
		s3dc_reg_data.coeff_set_r11_coeff.as32bits
							= coeff_set->coeff[i] & 0x1fff;
		s3dc_set_reg32((s3dc_plat->iobase + COLOR_R11_COEFF) + i,
							s3dc_reg_data.coeff_set_r11_coeff.as32bits);
	}

	for (i = 0; i < 12; i++) {
		s3dc_reg_data.coeff_set_r11_offset.as32bits
							= coeff_set->offset[i] & 0x1fff;
		s3dc_set_reg32((s3dc_plat->iobase + COLOR_R11_OFFSET) + i,
							s3dc_reg_data.coeff_set_r11_offset.as32bits);
	}
}

void s3dc_init_creator_formatter(void)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	/* creator&formatter engine init */
	css_s3dc("Formatter init\n");

	/* input size setting */
	s3dc_reg_data.cf_crop_line_start_0.as32bits = pctxt->cam0_line_start;
	s3dc_reg_data.cf_crop_line_count_0.as32bits = pctxt->cam0_crop_height;
	s3dc_reg_data.cf_crop_pixel_start_0.as32bits = pctxt->cam0_pixel_start;
	s3dc_reg_data.cf_crop_pixel_count_0.as32bits = pctxt->cam0_crop_width;
	s3dc_reg_data.cf_crop_line_start_1.as32bits = pctxt->cam1_line_start;
	s3dc_reg_data.cf_crop_line_count_1.as32bits = pctxt->cam1_crop_height;
	s3dc_reg_data.cf_crop_pixel_start_1.as32bits = pctxt->cam1_pixel_start;
	s3dc_reg_data.cf_crop_pixel_count_1.as32bits = pctxt->cam1_crop_width;

	s3dc_set_reg32(s3dc_plat->iobase + LINE_START_0, pctxt->cam0_line_start);
	s3dc_set_reg32(s3dc_plat->iobase + LINE_COUNT_0, pctxt->cam0_crop_height);
	s3dc_set_reg32(s3dc_plat->iobase + PIXEL_START_0, pctxt->cam0_pixel_start);
	s3dc_set_reg32(s3dc_plat->iobase + PIXEL_COUNT_0, pctxt->cam0_crop_width);
	s3dc_set_reg32(s3dc_plat->iobase + LINE_START_1, pctxt->cam1_line_start);
	s3dc_set_reg32(s3dc_plat->iobase + LINE_COUNT_1, pctxt->cam1_crop_height);
	s3dc_set_reg32(s3dc_plat->iobase + PIXEL_START_1, pctxt->cam1_pixel_start);
	s3dc_set_reg32(s3dc_plat->iobase + PIXEL_COUNT_1, pctxt->cam1_crop_width);

	css_s3dc("Crop Start Point : Cam0-> (%d , %d)  Cam1-> (%d , %d)\n",
			s3dc_get_reg32(s3dc_plat->iobase + PIXEL_START_0),
			s3dc_get_reg32(s3dc_plat->iobase + LINE_START_0),
			s3dc_get_reg32(s3dc_plat->iobase + PIXEL_START_1),
			s3dc_get_reg32(s3dc_plat->iobase + LINE_START_1));
	css_s3dc("Crop Size : Cam0-> %d x %d   Cam1-> %d x %d \n\n",
			s3dc_get_reg32(s3dc_plat->iobase + PIXEL_COUNT_0),
			s3dc_get_reg32(s3dc_plat->iobase + LINE_COUNT_0),
			s3dc_get_reg32(s3dc_plat->iobase + PIXEL_COUNT_1),
			s3dc_get_reg32(s3dc_plat->iobase + LINE_COUNT_1));

	s3dc_reg_data.cf_ctw0_comp.as32bits = 0x08;
	s3dc_reg_data.cf_ctw1_comp.as32bits = 0x08;
	s3dc_set_reg32(s3dc_plat->iobase + CTW0_COMP, 0x08);
	s3dc_set_reg32(s3dc_plat->iobase + CTW1_COMP, 0x08);

	/* tilt setting */
	{
		int input_w = (int)((100000/pctxt->cam1_crop_width)*65536 + 50000)/100000;

		/* s3dc_set_reg32(TANGENT, 0);*/ /* default = 0 */
		s3dc_reg_data.cf_tilt_reci_pwidth.as32bits = input_w;
		s3dc_reg_data.cf_tilt_bypass.as32bits = pctxt->tilt_bypass_en << 2;
		s3dc_set_reg32(s3dc_plat->iobase + RECI_PWIDTH, input_w);
		s3dc_set_reg32(s3dc_plat->iobase + TILT_BYPASS,
						pctxt->tilt_bypass_en << 2);
	}

	/*
	 * AC Area setting :
	 * '최종 출력' width의 전체, height의 가운데 절반 만큼을 AC area로 잡는다.
	 */
	s3dc_reg_data.ac_region_y.as32bits		= pctxt->ac_area_y_start;
	s3dc_reg_data.ac_region_x.as32bits		= pctxt->ac_area_x_start;
	s3dc_reg_data.ac_region_height.as32bits = pctxt->ac_area_height;
	s3dc_reg_data.ac_region_width.as32bits	= pctxt->ac_area_width;
	s3dc_set_reg32(s3dc_plat->iobase + AC_Y_START, pctxt->ac_area_y_start);
	s3dc_set_reg32(s3dc_plat->iobase + AC_X_START, pctxt->ac_area_x_start);
	s3dc_set_reg32(s3dc_plat->iobase + AC_HEIGHT, pctxt->ac_area_height);
	s3dc_set_reg32(s3dc_plat->iobase + AC_WIDTH, pctxt->ac_area_width);

	/* edge tolerance */
	s3dc_reg_data.ac_edge_th.as32bits = pctxt->ac_edge_tol;
	s3dc_set_reg32(s3dc_plat->iobase + AC_TAL, pctxt->ac_edge_tol);

	/* BLT Out image size setting */
	s3dc_reg_data.ac_total_width.as32bits = pctxt->cf_out_width;
	s3dc_reg_data.ac_total_height.as32bits = pctxt->cf_out_height;
	s3dc_set_reg32(s3dc_plat->iobase + AC_TOTAL_WIDTH, pctxt->cf_out_width);
	s3dc_set_reg32(s3dc_plat->iobase + AC_TOTAL_HEIGHT, pctxt->cf_out_height);

	/* DP_STEP value setting */
	s3dc_reg_data.cf_dp_step.as32bits = 0x00;
	s3dc_set_reg32(s3dc_plat->iobase + DP_STEP, 0x00);

	/* cam0,cam1 width setting(for AC,rotate) */
	s3dc_reg_data.cf_cam0_total_pixel_count.as32bits = pctxt->cam0_width;
	s3dc_reg_data.cf_cam1_total_pixel_count.as32bits = pctxt->cam1_width;
	s3dc_reg_data.cf_cam0_total_line_count.as32bits = pctxt->cam0_height;
	s3dc_reg_data.cf_cam1_total_line_count.as32bits = pctxt->cam1_height;
	s3dc_set_reg32(s3dc_plat->iobase + CAM0_TOTAL_PIXEL_COUNT,
					pctxt->cam0_width);
	s3dc_set_reg32(s3dc_plat->iobase + CAM1_TOTAL_PIXEL_COUNT,
					pctxt->cam1_width);
	s3dc_set_reg32(s3dc_plat->iobase + CAM0_TOTAL_LINE_COUNT,
					pctxt->cam0_height);
	s3dc_set_reg32(s3dc_plat->iobase + CAM1_TOTAL_LINE_COUNT,
					pctxt->cam1_height);

	/* LR change register */
	s3dc_reg_data.cf_cam_lr_change.as32bits = pctxt->change_lr;
	s3dc_set_reg32(s3dc_plat->iobase + LR_CHANGE, pctxt->change_lr);

	/* auto luminance balance setting */
	s3dc_reg_data.wb_set_stablecnt.as32bits = (pctxt->wb_fstable_cnt);
	s3dc_set_reg32(s3dc_plat->iobase + WB_FSTABLE_CNT, (pctxt->wb_fstable_cnt));

	s3dc_reg_data.wb_set_param.as32bits
						= (pctxt->wb_avg_up_cnt << 2) | pctxt->hwb_value;
	s3dc_set_reg32(s3dc_plat->iobase + WB_CURR_AVG_UP_CNT_HWB_VAL,
						s3dc_reg_data.wb_set_param.as32bits);
}

void s3dc_set_zoom(int zoomIdx)
{
	int input_w = 0;
	int pre_c0_crop_w_correction, pre_c1_crop_w_correction;
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	int cf_out_w = pctxt->cf_out_width;
	int cf_out_h = pctxt->cf_out_height;

	int cam0_width = pctxt->cam0_width;
	int cam0_height = pctxt->cam0_height;
	int cam1_width = pctxt->cam1_width;
	int cam1_height = pctxt->cam1_height;

	int cam0_crop_w = pctxt->cam0_crop_width - pctxt->cam0_crop_w_correction;
	int cam0_crop_h = pctxt->cam0_crop_height;
	int cam1_crop_w = pctxt->cam1_crop_width - pctxt->cam1_crop_w_correction;
	int cam1_crop_h = pctxt->cam1_crop_height;

	int newCropW0 = cam0_crop_w - (32 * zoomIdx);
	int newCropH0 = cam0_crop_h - (18 * zoomIdx);
	int newCropW1 = cam1_crop_w - (32 * zoomIdx);
	int newCropH1 = cam1_crop_h - (18 * zoomIdx);
	int zStartX0 = (cam0_width - newCropW0) / 2;
	int zStartX1 = (cam1_width - newCropW1) / 2;
	int zStartY0 = (cam0_height - newCropH0) / 2;
	int zStartY1 = (cam1_height - newCropH1) / 2;
	int zWidth0 = newCropW0;
	int zHeight0 = newCropH0;
	int zWidth1 = newCropW1;
	int zHeight1 = newCropH1;

	pre_c0_crop_w_correction = pctxt->cam0_crop_w_correction;
	pre_c1_crop_w_correction = pctxt->cam0_crop_w_correction;

	if (zWidth0 % 8 != 0)
		zWidth0 = (zWidth0 - zWidth0 % 8) + 8;
	if (zWidth1 % 8 != 0)
		zWidth1 = (zWidth1 - zWidth1 % 8) + 8;

	pctxt->cam0_crop_w_correction = zWidth0 - newCropW0;
	pctxt->cam1_crop_w_correction = zWidth1 - newCropW1;

	css_s3dc("Zoom Setting----------------------------->\n");
	s3dc_disable_shadow_reg_up();

	s3dc_reg_data.cf_crop_line_start_0.as32bits = zStartY0;
	s3dc_reg_data.cf_crop_line_count_0.as32bits = zHeight0;
	s3dc_reg_data.cf_crop_pixel_start_0.as32bits = zStartX0;
	s3dc_reg_data.cf_crop_pixel_count_0.as32bits = zWidth0;
	s3dc_set_reg32(s3dc_plat->iobase + LINE_START_0, zStartY0);
	s3dc_set_reg32(s3dc_plat->iobase + LINE_COUNT_0, zHeight0);
	s3dc_set_reg32(s3dc_plat->iobase + PIXEL_START_0, zStartX0);
	s3dc_set_reg32(s3dc_plat->iobase + PIXEL_COUNT_0, zWidth0);

	s3dc_reg_data.cf_crop_line_start_1.as32bits = zStartY1;
	s3dc_reg_data.cf_crop_line_count_1.as32bits = zHeight1;
	s3dc_reg_data.cf_crop_pixel_start_1.as32bits = zStartX1;
	s3dc_reg_data.cf_crop_pixel_count_1.as32bits = zWidth1;
	s3dc_set_reg32(s3dc_plat->iobase + LINE_START_1, zStartY1);
	s3dc_set_reg32(s3dc_plat->iobase + LINE_COUNT_1, zHeight1);
	s3dc_set_reg32(s3dc_plat->iobase + PIXEL_START_1, zStartX1);
	s3dc_set_reg32(s3dc_plat->iobase + PIXEL_COUNT_1, zWidth1);

	if ((pctxt->curr_mode == MODE_S3D_LINE) ||
		(pctxt->curr_mode == MODE_S3D_TOP_BOTTOM)) {
		s3dc_set_scaler_factor(0, newCropW0, zHeight0, cf_out_w, cf_out_h / 2,
								pctxt->deci_mode);
		s3dc_set_scaler_factor(1, newCropW1, zHeight1, cf_out_w, cf_out_h / 2,
								pctxt->deci_mode);
	} else if (pctxt->curr_mode == MODE_S3D_SIDE_BY_SIDE) {
		s3dc_set_scaler_factor(0, newCropW0, zHeight0, cf_out_w / 2, cf_out_h,
								pctxt->deci_mode);
		s3dc_set_scaler_factor(1, newCropW1, zHeight1, cf_out_w / 2, cf_out_h,
								pctxt->deci_mode);
	} else {
		s3dc_set_scaler_factor(0, newCropW0, zHeight0, cf_out_w, cf_out_h,
								pctxt->deci_mode);
		s3dc_set_scaler_factor(1, newCropW1, zHeight1, cf_out_w, cf_out_h,
								pctxt->deci_mode);
	}

	input_w = (int)((100000 / zWidth1) * 65536 + 50000) / 100000;

	s3dc_reg_data.cf_tilt_reci_pwidth.as32bits = input_w;
	s3dc_set_reg32(s3dc_plat->iobase + RECI_PWIDTH, input_w);

	s3dc_enable_shadow_reg_up();

	pctxt->cam0_crop_w_correction = pre_c0_crop_w_correction;
	pctxt->cam1_crop_w_correction = pre_c1_crop_w_correction;

	css_s3dc("<------------------------ End of Zoom Setting\n");
}

void s3dc_set_oneshot_mode(int is_enable)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	if (is_enable) {
		s3dc_disable_shadow_reg_up();

		if (pctxt->page_flip_on) {
			/* page flipping off */
			s3dc_reg_data.blt_cmd_dst_sm_addr2_l.as32bits
						= (pctxt->frame_buffer_sm_addr[4] >> 1) & 0xffff;
			s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR2_LOW,
							s3dc_reg_data.blt_cmd_dst_sm_addr2_l.as32bits);
			s3dc_reg_data.blt_cmd_dst_sm_addr2_h.as32bits
							= (pctxt->frame_buffer_sm_addr[4] >> 1) >> 16;
			s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR2_HIGH,
							s3dc_reg_data.blt_cmd_dst_sm_addr2_h.as32bits);
		}
		s3dc_reg_data.one_shot_enable.as32bits = is_enable;
		s3dc_set_reg32(s3dc_plat->iobase + ONE_SHOT_ENABLE, is_enable);

		s3dc_enable_shadow_reg_up();
	} else { /*disable */
		if (pctxt->page_flip_on) {
			/* page flipping on */
			s3dc_reg_data.blt_cmd_dst_sm_addr2_l.as32bits
							= (pctxt->frame_buffer_sm_addr[5] >> 1) & 0xffff;
			s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR2_LOW,
							s3dc_reg_data.blt_cmd_dst_sm_addr2_l.as32bits);
			s3dc_reg_data.blt_cmd_dst_sm_addr2_h.as32bits
							= (pctxt->frame_buffer_sm_addr[5] >> 1) >> 16;
			s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR2_HIGH,
							s3dc_reg_data.blt_cmd_dst_sm_addr2_h.as32bits);
		}

		s3dc_reg_data.one_shot_enable.as32bits = is_enable;
		s3dc_set_reg32(s3dc_plat->iobase + ONE_SHOT_ENABLE, is_enable);

		s3dc_enable_shadow_reg_up();
	}

	css_s3dc("One Shot Enable : %d (reg5092 = 0x%x)\n", is_enable,
			s3dc_get_reg32(s3dc_plat->iobase + ONE_SHOT_ENABLE));
}

/*****
*
* s3dcCFInput.c
*
******/

/*
 * FUNC : s3dc_set_dma_start_mode
 * DESC : CF 엔진의 입력이 DMA 일 때 start 모드 설정.
 */
void s3dc_set_dma_start_mode(int dma0StartMode, int dma1StartMode)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();

	switch (dma0StartMode) {
	case DMA_START_HOST_CMD:
		pctxt->cfi_dma_start_mode[0] = 0x00;
		css_s3dc("Cam[0] DMA Start Mode : by host command\n");
		break;
	case DMA_START_DIR_IF_VSYNC:
		pctxt->cfi_dma_start_mode[0] = 0x01;
		css_s3dc("Cam[0] DMA Start Mode : by Cam1 (direct interface) VSYNC\n");
		break;
	case DMA_START_EMBEDDED_TIME:
		pctxt->cfi_dma_start_mode[0] = 0x02;
		css_s3dc("Cam[0] DMA Start Mode : by embedded timing generator.\n");
		break;
	default:
		pctxt->cfi_dma_start_mode[0] = 0x00;
		css_s3dc("Cam[0] DMA Start Mode : by host command\n");
	}

	switch (dma1StartMode) {
	case DMA_START_HOST_CMD:
		pctxt->cfi_dma_start_mode[1] = 0x00;
		css_s3dc("Cam[1] DMA Start Mode : by host command\n");
		break;
	case DMA_START_DIR_IF_VSYNC:
		pctxt->cfi_dma_start_mode[1] = 0x01;
		css_s3dc("Cam[1] DMA Start Mode : by Cam0 (direct interface) VSYNC\n");
		break;
	case DMA_START_EMBEDDED_TIME:
		pctxt->cfi_dma_start_mode[1] = 0x02;
		css_s3dc("Cam[1] DMA Start Mode : by embedded timing generator.\n");
		break;
	default:
		pctxt->cfi_dma_start_mode[1] = 0x00;
		css_s3dc("Cam[1] DMA Start Mode : by host command\n");
	}

	/*
	 * 레지스터 셋팅은 s3dc_set_cf_input_info() 에서 수행함
	if (cam_sel == 0) {
		s3dc_set_reg32(DMA_START_MODE0, pctxt->cfi_dma_start_mode[0]);
		css_s3dc("------> DMA_START_MODE0 : 0x%x\n",
				s3dc_get_reg32(DMA_START_MODE0));
	} else {
		s3dc_set_reg32(DMA_START_MODE1, pctxt->cfi_dma_start_mode[1]);
		css_s3dc("------> DMA_START_MODE1 : 0x%x\n",
				s3dc_get_reg32(DMA_START_MODE1));
	}
	*/
}

/*
 * FUNC : s3dc_set_cf_input_info
 * DESC : CF 엔진의 입력 모드 관련 설정.
 * y,cb,cr 의 스택메모리 주소는 이미 셋팅이 되어 있어야함.
 */
void s3dc_set_cf_input_info(CFInputFormatInfo cf_input_format, CFInputDMASizeInfo dma_size_info)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();

	if (cf_input_format.c0_input_mode != CF_INPUT_NONE) {
		pctxt->cfi_cam_if_sel[0] = cf_input_format.c0_input_mode;
		pctxt->cfi_dma_format[0] = cf_input_format.c0_format;
		pctxt->cfi_dma_data_order[0] = cf_input_format.c0_order;
		pctxt->cfi_dma_total_w[0] = dma_size_info.c0_total_w;
		pctxt->cfi_dma_total_h[0] = dma_size_info.c0_total_h;

		if (cf_input_format.c0_input_mode == CF_INPUT_DMA) {
			pctxt->cfi_dma_crop_x[0] = dma_size_info.c0_crop_x;
			pctxt->cfi_dma_crop_y[0] = dma_size_info.c0_crop_y;
			pctxt->cfi_dma_crop_w[0] = dma_size_info.c0_crop_w;
			pctxt->cfi_dma_crop_h[0] = dma_size_info.c0_crop_h;
			s3dc_set_reg_dma_Input0(pctxt);
		} else if (cf_input_format.c0_input_mode == CF_INPUT_DIRECT) {
			s3dc_set_reg_direct0(pctxt);
		}
	}

	if (cf_input_format.c1_input_mode != CF_INPUT_NONE) {
		pctxt->cfi_cam_if_sel[1] = cf_input_format.c1_input_mode;
		pctxt->cfi_dma_format[1] = cf_input_format.c1_format;
		pctxt->cfi_dma_data_order[1] = cf_input_format.c1_order;
		pctxt->cfi_dma_total_w[1] = dma_size_info.c1_total_w;
		pctxt->cfi_dma_total_h[1] = dma_size_info.c1_total_h;

		if (cf_input_format.c1_input_mode == CF_INPUT_DMA) {
			pctxt->cfi_dma_crop_x[1] = dma_size_info.c1_crop_x;
			pctxt->cfi_dma_crop_y[1] = dma_size_info.c1_crop_y;
			pctxt->cfi_dma_crop_w[1] = dma_size_info.c1_crop_w;
			pctxt->cfi_dma_crop_h[1] = dma_size_info.c1_crop_h;
			s3dc_set_reg_dma_Input1(pctxt);
		} else if (cf_input_format.c1_input_mode == CF_INPUT_DIRECT) {
			s3dc_set_reg_direct1(pctxt);
		}
	}
}

/*
 * FUNC : s3dc_set_cf_input_dma0_addr
 * DESC : CF 엔진의 입력이 DMA0 일 때 읽어오는 데이터 메모리 주소 셋팅
 */
void s3dc_set_cf_input_dma0_addr(int plane1Addr, int plane2Addr)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	pctxt->cfi_y_addr[0] = plane1Addr;
	pctxt->cfi_c0_addr[0] = plane2Addr;
}

/*
 * FUNC : s3dc_set_cf_input_dma1_addr
 * DESC : CF 엔진의 입력이 DMA1 일 때 읽어오는 데이터 메모리 주소 셋팅
 */
void s3dc_set_cf_input_dma1_addr(int plane1Addr, int plane2Addr)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	pctxt->cfi_y_addr[1] = plane1Addr;
	pctxt->cfi_c0_addr[1] = plane2Addr;
}

/*
 * FUNC : s3dc_start_dma_read
 * DESC : DMA Read enable
 */
void s3dc_start_dma_read(void)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	if ((pctxt->cfi_cam_if_sel[0] == CF_INPUT_DMA) &&
		(pctxt->cfi_dma_start_mode[0] == 0)) {
		css_s3dc("DMA0 Read Start.\n");
		s3dc_poll_reg32(s3dc_plat->iobase + DMA_BUSY0, 0, 0);

		s3dc_reg_data.dma_start0.as32bits = 0x0001;
		s3dc_set_reg32(s3dc_plat->iobase + DMA_START0, 0x0001);
	}

	if ((pctxt->cfi_cam_if_sel[1] == CF_INPUT_DMA) &&
		(pctxt->cfi_dma_start_mode[1] == 0)) {
		css_s3dc("DMA1 Read Start.\n");
		s3dc_poll_reg32(s3dc_plat->iobase + DMA_BUSY1, 0, 0);

		s3dc_reg_data.dma_start1.as32bits = 0x0001;
		s3dc_set_reg32(s3dc_plat->iobase + DMA_START1, 0x0001);
	}
}

/*
 * FUNC : s3dc_start_dma_read0
 * DESC : DMA0 Read enable
 */
void s3dc_start_dma_read0(void)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	css_s3dc("DMA0 Read Start.\n");

	s3dc_poll_reg32(s3dc_plat->iobase + DMA_BUSY0, 0, 0);

	s3dc_reg_data.dma_start0.as32bits = 0x0001;
	s3dc_set_reg32(s3dc_plat->iobase + DMA_START0, 0x0001);
}

/*
 * FUNC : s3dc_start_dma_read1
 * DESC : DMA1 Read enable
 */
void s3dc_start_dma_read1(void)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	css_s3dc("DMA1 Read Start.\n");

	s3dc_poll_reg32(s3dc_plat->iobase + DMA_BUSY1, 0, 0);

	s3dc_reg_data.dma_start1.as32bits = 0x0001;
	s3dc_set_reg32(s3dc_plat->iobase + DMA_START1, 0x0001);
}

/*
 * FUNC : s3dc_set_reg_dma_Input0
 * DESC : DMA0 Read Interface 관련 설정.
 */
void s3dc_set_reg_dma_Input0(NXSCContext *pctxt)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	css_s3dc("DMA0 input Setting.\n");

	s3dc_reg_data.cf_cam0_if_sel.as32bits = pctxt->cfi_cam_if_sel[0];
	s3dc_set_reg32(s3dc_plat->iobase + CAM0_IF_SEL, pctxt->cfi_cam_if_sel[0]);

	s3dc_reg_data.dma_y_base_addr0_l.as32bits = (pctxt->cfi_y_addr[0]) & 0xFFFF;
	s3dc_reg_data.dma_y_base_addr0_h.as32bits = (pctxt->cfi_y_addr[0]) >> 16;
	s3dc_set_reg32(s3dc_plat->iobase + Y_BASE_ADDR0_L,
					s3dc_reg_data.dma_y_base_addr0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + Y_BASE_ADDR0_H,
					s3dc_reg_data.dma_y_base_addr0_h.as32bits);

	s3dc_reg_data.dma_c0_base_addr0_l.as32bits = (pctxt->cfi_c0_addr[0])
												& 0xFFFF;
	s3dc_reg_data.dma_c0_base_addr0_h.as32bits = (pctxt->cfi_c0_addr[0]) >> 16;
	s3dc_set_reg32(s3dc_plat->iobase + C0_BASE_ADDR0_L,
					s3dc_reg_data.dma_c0_base_addr0_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + C0_BASE_ADDR0_H,
					s3dc_reg_data.dma_c0_base_addr0_h.as32bits);

	s3dc_reg_data.dma_vs_blank0.as32bits = 0x2710;
	s3dc_reg_data.dma_hs_blank0.as32bits = 0x0064;
	s3dc_reg_data.dma_vs2hs_del0.as32bits = 0x2710;
	s3dc_reg_data.dma_hs2vs_del0.as32bits = 0x2710;
	s3dc_set_reg32(s3dc_plat->iobase + VS_BLANK0, 0x2710);
	s3dc_set_reg32(s3dc_plat->iobase + HS_BLANK0, 0x0064);
	s3dc_set_reg32(s3dc_plat->iobase + VS2HS_DEL0, 0x2710);
	s3dc_set_reg32(s3dc_plat->iobase + HS2VS_DEL0, 0x2710);

	s3dc_reg_data.dma_t_width0.as32bits = pctxt->cfi_dma_total_w[0];
	s3dc_reg_data.dma_t_height0.as32bits = pctxt->cfi_dma_total_h[0];
	s3dc_reg_data.dma_x_offset0.as32bits = pctxt->cfi_dma_crop_x[0];
	s3dc_reg_data.dma_y_offset0.as32bits = pctxt->cfi_dma_crop_y[0];
	s3dc_reg_data.dma_p_width0.as32bits = pctxt->cfi_dma_crop_w[0];
	s3dc_reg_data.dma_p_height0.as32bits = pctxt->cfi_dma_crop_h[0];

	s3dc_set_reg32(s3dc_plat->iobase + DMA_T_WIDTH0, pctxt->cfi_dma_total_w[0]);
	s3dc_set_reg32(s3dc_plat->iobase + DMA_T_HEIGHT0,
													pctxt->cfi_dma_total_h[0]);
	s3dc_set_reg32(s3dc_plat->iobase + DMA_X_OFFSET0, pctxt->cfi_dma_crop_x[0]);
	s3dc_set_reg32(s3dc_plat->iobase + DMA_Y_OFFSET0, pctxt->cfi_dma_crop_y[0]);
	s3dc_set_reg32(s3dc_plat->iobase + DMA_P_WIDTH0, pctxt->cfi_dma_crop_w[0]);
	s3dc_set_reg32(s3dc_plat->iobase + DMA_P_HEIGHT0, pctxt->cfi_dma_crop_h[0]);

	s3dc_reg_data.dma_start_mode0.as32bits = pctxt->cfi_dma_start_mode[0];
	s3dc_reg_data.cf_d_fmt0.as32bits = pctxt->cfi_dma_format[0];
	s3dc_reg_data.cf_d_order0.as32bits = pctxt->cfi_dma_data_order[0];

	s3dc_set_reg32(s3dc_plat->iobase + DMA_START_MODE0,
					pctxt->cfi_dma_start_mode[0]);
	s3dc_set_reg32(s3dc_plat->iobase + D_FORMAT0, pctxt->cfi_dma_format[0]);
	s3dc_set_reg32(s3dc_plat->iobase + D_ORDER0, pctxt->cfi_dma_data_order[0]);

	css_s3dc("\n----- CF input Cam0 setting ------\n");
	css_s3dc("DMA_T_WIDTH0 : %d\n",
			s3dc_get_reg32(s3dc_plat->iobase + DMA_T_WIDTH0));
	css_s3dc("DMA_T_HEIGHT0 : %d\n",
			s3dc_get_reg32(s3dc_plat->iobase + DMA_T_HEIGHT0));
	css_s3dc("DMA_X_OFFSET0 : %d\n",
			s3dc_get_reg32(s3dc_plat->iobase + DMA_X_OFFSET0));
	css_s3dc("DMA_Y_OFFSET0 : %d\n",
			s3dc_get_reg32(s3dc_plat->iobase + DMA_Y_OFFSET0));
	css_s3dc("DMA_P_WIDTH0 : %d\n",
			s3dc_get_reg32(s3dc_plat->iobase + DMA_P_WIDTH0));
	css_s3dc("DMA_P_HEIGHT0 : %d\n",
			s3dc_get_reg32(s3dc_plat->iobase + DMA_P_HEIGHT0));
	css_s3dc("DMA_START_MODE0 : 0x%x\n",
			s3dc_get_reg32(s3dc_plat->iobase + DMA_START_MODE0));
	css_s3dc("D_FORMAT0 : 0x%x\n",
			s3dc_get_reg32(s3dc_plat->iobase + D_FORMAT0));
	css_s3dc("D_ORDER0 : 0x%x\n", s3dc_get_reg32(s3dc_plat->iobase + D_ORDER0));
}

/*
 * FUNC : s3dc_set_reg_dma_Input1
 * DESC : DMA1 Read Interface 관련 설정.
 */
void s3dc_set_reg_dma_Input1(NXSCContext *pctxt)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	css_s3dc("DMA1 input Setting.\n");

	s3dc_reg_data.cf_cam1_if_sel.as32bits = pctxt->cfi_cam_if_sel[1];
	s3dc_set_reg32(s3dc_plat->iobase + CAM1_IF_SEL, pctxt->cfi_cam_if_sel[1]);

	s3dc_reg_data.dma_y_base_addr1_l.as32bits = (pctxt->cfi_y_addr[1]) & 0xFFFF;
	s3dc_reg_data.dma_y_base_addr1_h.as32bits = (pctxt->cfi_y_addr[1]) >> 16;
	s3dc_set_reg32(s3dc_plat->iobase + Y_BASE_ADDR1_L,
					s3dc_reg_data.dma_y_base_addr1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + Y_BASE_ADDR1_H,
					s3dc_reg_data.dma_y_base_addr1_h.as32bits);

	s3dc_reg_data.dma_c0_base_addr1_l.as32bits = (pctxt->cfi_c0_addr[1])
												& 0xFFFF;
	s3dc_reg_data.dma_c0_base_addr1_h.as32bits = (pctxt->cfi_c0_addr[1]) >> 16;
	s3dc_set_reg32(s3dc_plat->iobase + C0_BASE_ADDR1_L,
					s3dc_reg_data.dma_c0_base_addr1_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + C0_BASE_ADDR1_H,
					s3dc_reg_data.dma_c0_base_addr1_h.as32bits);

	s3dc_reg_data.dma_vs_blank1.as32bits = 0x2710;
	s3dc_reg_data.dma_hs_blank1.as32bits = 0x0064;
	s3dc_reg_data.dma_vs2hs_del1.as32bits = 0x2710;
	s3dc_reg_data.dma_hs2vs_del1.as32bits = 0x2710;
	s3dc_set_reg32(s3dc_plat->iobase + VS_BLANK1, 0x2710);
	s3dc_set_reg32(s3dc_plat->iobase + HS_BLANK1, 0x0064);
	s3dc_set_reg32(s3dc_plat->iobase + VS2HS_DEL1, 0x2710);
	s3dc_set_reg32(s3dc_plat->iobase + HS2VS_DEL1, 0x2710);

	s3dc_reg_data.dma_t_width1.as32bits = pctxt->cfi_dma_total_w[1];
	s3dc_reg_data.dma_t_height1.as32bits = pctxt->cfi_dma_total_h[1];
	s3dc_reg_data.dma_x_offset1.as32bits = pctxt->cfi_dma_crop_x[1];
	s3dc_reg_data.dma_y_offset1.as32bits = pctxt->cfi_dma_crop_y[1];
	s3dc_reg_data.dma_p_width1.as32bits = pctxt->cfi_dma_crop_w[1];
	s3dc_reg_data.dma_p_height1.as32bits = pctxt->cfi_dma_crop_h[1];

	s3dc_set_reg32(s3dc_plat->iobase + DMA_T_WIDTH1, pctxt->cfi_dma_total_w[1]);
	s3dc_set_reg32(s3dc_plat->iobase + DMA_T_HEIGHT1,
													pctxt->cfi_dma_total_h[1]);
	s3dc_set_reg32(s3dc_plat->iobase + DMA_X_OFFSET1, pctxt->cfi_dma_crop_x[1]);
	s3dc_set_reg32(s3dc_plat->iobase + DMA_Y_OFFSET1, pctxt->cfi_dma_crop_y[1]);
	s3dc_set_reg32(s3dc_plat->iobase + DMA_P_WIDTH1, pctxt->cfi_dma_crop_w[1]);
	s3dc_set_reg32(s3dc_plat->iobase + DMA_P_HEIGHT1, pctxt->cfi_dma_crop_h[1]);

	s3dc_reg_data.dma_start_mode1.as32bits = pctxt->cfi_dma_start_mode[1];
	s3dc_reg_data.cf_d_fmt1.as32bits = pctxt->cfi_dma_format[1];
	s3dc_reg_data.cf_d_order1.as32bits = pctxt->cfi_dma_data_order[1];

	s3dc_set_reg32(s3dc_plat->iobase + DMA_START_MODE1,
					pctxt->cfi_dma_start_mode[1]);
	s3dc_set_reg32(s3dc_plat->iobase + D_FORMAT1, pctxt->cfi_dma_format[1]);
	s3dc_set_reg32(s3dc_plat->iobase + D_ORDER1, pctxt->cfi_dma_data_order[1]);

	css_s3dc("\n----- CF input Cam1 setting ------\n");
	css_s3dc("DMA_T_WIDTH1 : %d\n",
			s3dc_get_reg32(s3dc_plat->iobase + DMA_T_WIDTH1));
	css_s3dc("DMA_T_HEIGHT1 : %d\n",
			s3dc_get_reg32(s3dc_plat->iobase + DMA_T_HEIGHT1));
	css_s3dc("DMA_X_OFFSET1 : %d\n",
			s3dc_get_reg32(s3dc_plat->iobase + DMA_X_OFFSET1));
	css_s3dc("DMA_Y_OFFSET1 : %d\n",
			s3dc_get_reg32(s3dc_plat->iobase + DMA_Y_OFFSET1));
	css_s3dc("DMA_P_WIDTH1 : %d\n",
			s3dc_get_reg32(s3dc_plat->iobase + DMA_P_WIDTH1));
	css_s3dc("DMA_P_HEIGHT1 : %d\n",
			s3dc_get_reg32(s3dc_plat->iobase + DMA_P_HEIGHT1));
	css_s3dc("DMA_START_MODE1 : 0x%x\n",
			s3dc_get_reg32(s3dc_plat->iobase + DMA_START_MODE1));
	css_s3dc("D_FORMAT1 : 0x%x\n",
			s3dc_get_reg32(s3dc_plat->iobase + D_FORMAT1));
	css_s3dc("D_ORDER1 : 0x%x\n",
			s3dc_get_reg32(s3dc_plat->iobase + D_ORDER1));
}

/*
 * FUNC : s3dc_set_reg_direct0
 * DESC : Direct input Interface 0 관련 설정.
 */
void s3dc_set_reg_direct0(NXSCContext *pctxt)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	/* s3dc_disable_shadow_reg_up(); */
	/* Direct IF setting */
	s3dc_reg_data.cf_cam0_if_sel.as32bits	= pctxt->cfi_cam_if_sel[0];
	s3dc_reg_data.cf_d_fmt0.as32bits		= pctxt->cfi_dma_format[0];
	s3dc_reg_data.cf_d_order0.as32bits		= pctxt->cfi_dma_data_order[0];

	s3dc_set_reg32(s3dc_plat->iobase + CAM0_IF_SEL, pctxt->cfi_cam_if_sel[0]);
	s3dc_set_reg32(s3dc_plat->iobase + D_FORMAT0, pctxt->cfi_dma_format[0]);
	s3dc_set_reg32(s3dc_plat->iobase + D_ORDER0, pctxt->cfi_dma_data_order[0]);

	/* s3dc_enable_shadow_reg_up(); */
}

/*
 * FUNC : s3dc_set_reg_direct1
 * DESC : Direct input Interface 1 관련 설정.
 */
void s3dc_set_reg_direct1(NXSCContext *pctxt)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	/* s3dc_disable_shadow_reg_up(); */
	/* Direct IF setting */
	s3dc_reg_data.cf_cam1_if_sel.as32bits	= pctxt->cfi_cam_if_sel[1];
	s3dc_reg_data.cf_d_fmt1.as32bits		= pctxt->cfi_dma_format[1];
	s3dc_reg_data.cf_d_order1.as32bits		= pctxt->cfi_dma_data_order[1];

	s3dc_set_reg32(s3dc_plat->iobase + CAM1_IF_SEL, pctxt->cfi_cam_if_sel[1]);
	s3dc_set_reg32(s3dc_plat->iobase + D_FORMAT1, pctxt->cfi_dma_format[1]);
	s3dc_set_reg32(s3dc_plat->iobase + D_ORDER1, pctxt->cfi_dma_data_order[1]);

	/* s3dc_enable_shadow_reg_up(); */
}

/*****
*
* end of s3dcCFInput.c
*
******/

/*****
*
* s3dcBLT.c
*
******/

/*
 * FUNC : s3dc_set_blt_context
 * DESC : BLT input, output context setting
 */
int s3dc_set_blt_context(BLTInfo blt_info)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();

	int blt_out_order = blt_info.blto_order;
	int num_of_pixel;

	/*
	 * display_width > cfOutWidth일 때는 side-bar를 생성하므로
	 * display size기준으로 pixel갯수 및 메모리를 할당함.
	 */
	if (pctxt->display_width > pctxt->cf_out_width)
		num_of_pixel = pctxt->display_width * pctxt->display_height;
	else
		num_of_pixel = pctxt->cf_out_width * pctxt->cf_out_height;

	pctxt->fb_format = blt_info.blto_format;
	pctxt->fb_order = blt_info.blto_order;
	pctxt->blt_sbl_val = 0x20; /* default 0x20 */

	/* CF-eng write color format setting (= BLT input color format) */
	switch (blt_info.blti_format) {
		case BLT_INPUT_RGB888:
			pctxt->blti_format	= BLT_INPUT_RGB888;
			pctxt->blti_z_order	= 0;
			pctxt->blti_data_order	= 0;
			pctxt->blti_24bit_en	= 1;
			pctxt->cam0_wr_16bit	= 0;
			pctxt->cam1_wr_16bit	= 0;
			pctxt->blt_src_format	= BLT_COLOR_FMT_RGBA8888;
			break;
		case BLT_INPUT_RGB8888:
			pctxt->blti_format	= BLT_INPUT_RGB8888;
			pctxt->blti_z_order	= 0;
			pctxt->blti_data_order	= 0;
			pctxt->blti_24bit_en	= 0;
			pctxt->cam0_wr_16bit	= 0;
			pctxt->cam1_wr_16bit	= 0;
			pctxt->blt_src_format	= BLT_COLOR_FMT_RGBA8888;
			break;
	}

	/* BLT Out color format setting (= frame buffer color format) */
	switch (pctxt->fb_format) {
		case BLT_OUT_RGB565:
			pctxt->fb_data_size = num_of_pixel * 2;

			if (blt_out_order == BLT_OUT_ORDER_RGB) {
				pctxt->blto_data_order = 0; /* RGB order */
			} else if (blt_out_order == BLT_OUT_ORDER_BGR) {
				pctxt->blto_data_order = 1;
			}
			pctxt->blto_yuv_enable = 0;
			pctxt->blto_z_order = 0;
			pctxt->blto_24bit_en = 0;

			pctxt->blto_yuv_format = 0;
			pctxt->blto_yuv422_mode = 0; /* don't care */
			pctxt->blto_yuv420_mode = 0; /* don't care */
			pctxt->blt_dst_format = BLT_COLOR_FMT_RGB565;
			break;
		case BLT_OUT_RGB888:
			pctxt->fb_data_size = num_of_pixel * 3;

			if (blt_out_order == BLT_OUT_ORDER_RGB) {
				pctxt->blto_data_order = 0;/* RGB order */
			} else if (blt_out_order == BLT_OUT_ORDER_BGR) {
				pctxt->blto_data_order = 1;
			}
			pctxt->blto_yuv_enable = 0;
			pctxt->blto_z_order = 0;
			pctxt->blto_24bit_en = 1;

			pctxt->blto_yuv_format = 0;
			pctxt->blto_yuv422_mode = 0; /* don't care */
			pctxt->blto_yuv420_mode = 0; /* don't care */
			pctxt->blt_dst_format = BLT_COLOR_FMT_RGBA8888;
			break;
		case BLT_OUT_RGB8888:
			pctxt->fb_data_size = num_of_pixel * 4;
			if (blt_out_order == BLT_OUT_ORDER_RGBZ) {
				pctxt->blto_data_order = 0;/* RGB order */
				pctxt->blto_z_order	 = 0;
			} else if (blt_out_order == BLT_OUT_ORDER_ZRGB) {
				pctxt->blto_data_order = 0;/* RGB order */
				pctxt->blto_z_order	 = 1;
			} else if (blt_out_order == BLT_OUT_ORDER_BGRZ) {
				pctxt->blto_data_order = 1;/* RGB order */
				pctxt->blto_z_order	 = 0;
			} else if (blt_out_order == BLT_OUT_ORDER_ZBGR) {
				pctxt->blto_data_order = 1;/* RGB order */
				pctxt->blto_z_order	 = 1;
			}

			pctxt->blto_yuv_enable	= 0;
			pctxt->blto_24bit_en	= 0;
			pctxt->blto_yuv_format	= 0;
			pctxt->blto_yuv422_mode	= 0; /* don't care */
			pctxt->blto_yuv420_mode	= 0; /* don't care */
			pctxt->blt_dst_format	= BLT_COLOR_FMT_RGBA8888;
			break;
		case BLT_OUT_YUV444_1PLANE_PACKED:
			pctxt->fb_data_size = num_of_pixel * 3;
			if (blt_out_order == BLT_OUT_ORDER_YUV) {
				pctxt->blto_data_order = 0;
			} else if (blt_out_order == BLT_OUT_ORDER_YVU) {
				pctxt->blto_data_order = 1;
			} else {
				return 0;
			}
			pctxt->blto_yuv_enable	= 1;
			pctxt->blto_yuv_format	= 0; /* 0:YUV444 */
			pctxt->blto_yuv422_mode	= 0; /* don't care */
			pctxt->blto_yuv420_mode	= 0; /* don't care */
			pctxt->blto_z_order		= 0; /* don't care */
			pctxt->blto_24bit_en	= 1;
			pctxt->blt_dst_format	= BLT_COLOR_FMT_RGBA8888;

			break;
		case BLT_OUT_YUV444_1PLANE_UNPACKED:
			pctxt->fb_data_size = num_of_pixel * 4;
			if (blt_out_order == BLT_OUT_ORDER_ZYUV) {
				pctxt->blto_data_order = 0;
				pctxt->blto_z_order = 0;
			} else if (blt_out_order == BLT_OUT_ORDER_ZYVU) {
				pctxt->blto_data_order = 1;
				pctxt->blto_z_order = 0;
			} else if (blt_out_order == BLT_OUT_ORDER_YUVZ) {
				pctxt->blto_data_order = 0;
				pctxt->blto_z_order = 1;
			} else if (blt_out_order == BLT_OUT_ORDER_YVUZ) {
				pctxt->blto_data_order = 1;
				pctxt->blto_z_order = 1;
			} else {
				return 0;
			}
			pctxt->blto_yuv_enable	= 1;
			pctxt->blto_yuv_format	= 0; /* 0:YUV444 */
			pctxt->blto_yuv422_mode	= 0; /* don't care */
			pctxt->blto_yuv420_mode	= 0; /* don't care */
			pctxt->blto_24bit_en	= 0;
			pctxt->blt_dst_format	= BLT_COLOR_FMT_RGBA8888;
			break;
		case BLT_OUT_YUV422_1PLANE_PACKED:
			pctxt->fb_data_size = num_of_pixel * 2;
			if (blt_out_order == BLT_OUT_ORDER_YVYU) {
				pctxt->blto_yuv422_mode = 0;
				pctxt->blto_data_order = 0;
			} else if (blt_out_order == BLT_OUT_ORDER_YUYV) {
				pctxt->blto_yuv422_mode = 0;
				pctxt->blto_data_order = 1;
			} else if (blt_out_order == BLT_OUT_ORDER_VYUY) {
				pctxt->blto_yuv422_mode = 1;
				pctxt->blto_data_order = 0;
			} else if (blt_out_order == BLT_OUT_ORDER_UYVY) {
				pctxt->blto_yuv422_mode = 1;
				pctxt->blto_data_order = 1;
			} else {
				return 0;
			}
			pctxt->blto_z_order		= 0;	/* don't care */
			pctxt->blto_yuv_enable	= 1;
			pctxt->blto_yuv_format	= 0x01; /* 1:YUV422 packed */
			pctxt->blto_yuv420_mode	= 0; /* don't care */

			pctxt->blto_24bit_en	= 0;
			pctxt->blt_dst_format	= BLT_COLOR_FMT_RGB565;
			break;
		case BLT_OUT_YUV422_PLANAR:
			pctxt->fb_data_size = num_of_pixel * 2;
			if (blt_out_order == BLT_OUT_ORDER_VUVU) {
				pctxt->blto_data_order = 1;
			} else if (blt_out_order == BLT_OUT_ORDER_UVUV) {
				pctxt->blto_data_order = 0;
			} else {
				return 0;
			}
			pctxt->blto_z_order		= 0;	/* don't care */
			pctxt->blto_yuv_enable	= 1;
			pctxt->blto_yuv_format	= 0x02; /* 2:YUV422 planar */
			pctxt->blto_yuv420_mode	= 0; /* don't care */
			pctxt->blto_yuv422_mode	= 0; /* don't care */
			pctxt->blto_24bit_en	= 0;
			pctxt->blt_dst_format	= BLT_COLOR_FMT_RGB565;
			break;
		case BLT_OUT_N_V_12:
			pctxt->fb_data_size = num_of_pixel * 3 / 2;
			pctxt->blto_data_order	= 1;
			pctxt->blto_z_order		= 0;	/* don't care */
			pctxt->blto_yuv_enable	= 1;
			pctxt->blto_yuv_format	= 0x03; /* 3:YUV420 */
			pctxt->blto_yuv420_mode	= 0x03;
			pctxt->blto_yuv422_mode	= 0; /* don't care */
			pctxt->blto_24bit_en	= 0;
			pctxt->blt_dst_format	= BLT_COLOR_FMT_RGB565;
			break;
		case BLT_OUT_N_V_21:
			pctxt->fb_data_size = num_of_pixel * 3 / 2;
			pctxt->blto_data_order	= 0;
			pctxt->blto_z_order		= 0;	/* don't care */
			pctxt->blto_yuv_enable	= 1;
			pctxt->blto_yuv_format	= 0x03; /* 3:YUV420 */
			pctxt->blto_yuv420_mode	= 0x03;
			pctxt->blto_yuv422_mode	= 0; /* don't care */
			pctxt->blto_24bit_en	= 0;
			pctxt->blt_dst_format	= BLT_COLOR_FMT_RGB565;
			break;
		case BLT_OUT_Y_V_12:
			pctxt->fb_data_size = num_of_pixel * 3 / 2;
			pctxt->blto_data_order	= 1;
			pctxt->blto_z_order		= 0;	/* don't care */
			pctxt->blto_yuv_enable	= 1;
			pctxt->blto_yuv_format	= 0x03; /* 3:YUV420 */
			pctxt->blto_yuv420_mode	= 0x02;
			pctxt->blto_yuv422_mode	= 0; /* don't care */
			pctxt->blto_24bit_en	= 0;
			pctxt->blt_dst_format	= BLT_COLOR_FMT_RGB565;
			break;
		case BLT_OUT_Y_V_21:
			pctxt->fb_data_size = num_of_pixel * 3 / 2;
			pctxt->blto_data_order	= 0;
			pctxt->blto_z_order		= 0;	/* don't care */
			pctxt->blto_yuv_enable	= 1;
			pctxt->blto_yuv_format	= 0x03; /* 3:YUV420 */
			pctxt->blto_yuv420_mode	= 0x02;
			pctxt->blto_yuv422_mode	= 0; /* don't care */
			pctxt->blto_24bit_en	= 0;
			pctxt->blt_dst_format	= BLT_COLOR_FMT_RGB565;
			break;
		case BLT_OUT_IMC_1:
			pctxt->fb_data_size = num_of_pixel * 2;
			pctxt->blto_data_order	= 1;
			pctxt->blto_z_order		= 0;	/* don't care */
			pctxt->blto_yuv_enable	= 1;
			pctxt->blto_yuv_format	= 0x03; /* 3:YUV420 */
			pctxt->blto_yuv420_mode	= 0x00;
			pctxt->blto_yuv422_mode	= 0; /* don't care */
			pctxt->blto_24bit_en	= 0;
			pctxt->blt_dst_format	= BLT_COLOR_FMT_RGB565;
			break;
		case BLT_OUT_IMC_2:
			pctxt->fb_data_size = num_of_pixel * 3 / 2;
			pctxt->blto_data_order	= 1;
			pctxt->blto_z_order		= 0;	/* don't care */
			pctxt->blto_yuv_enable	= 1;
			pctxt->blto_yuv_format	= 0x03; /* 3:YUV420 */
			pctxt->blto_yuv420_mode	= 0x01;
			pctxt->blto_yuv422_mode	= 0; /* don't care */
			pctxt->blto_24bit_en	= 0;
			pctxt->blt_dst_format	= BLT_COLOR_FMT_RGB565;
			break;
		case BLT_OUT_IMC_3:
			pctxt->fb_data_size = num_of_pixel * 2;
			pctxt->blto_data_order	= 0;
			pctxt->blto_z_order		= 0;	/* don't care */
			pctxt->blto_yuv_enable	= 1;
			pctxt->blto_yuv_format	= 0x03; /* 3:YUV420 */
			pctxt->blto_yuv420_mode	= 0x00;
			pctxt->blto_yuv422_mode	= 0; /* don't care */
			pctxt->blto_24bit_en	= 0;
			pctxt->blt_dst_format	= BLT_COLOR_FMT_RGB565;
			break;
		case BLT_OUT_IMC_4:
			pctxt->fb_data_size = num_of_pixel * 3 / 2;
			pctxt->blto_data_order	= 0;
			pctxt->blto_z_order		= 0;	/* don't care */
			pctxt->blto_yuv_enable	= 1;
			pctxt->blto_yuv_format	= 0x03; /* 3:YUV420 */
			pctxt->blto_yuv420_mode	= 0x01;
			pctxt->blto_yuv422_mode	= 0; /* don't care */
			pctxt->blto_24bit_en	= 0;
			pctxt->blt_dst_format	= BLT_COLOR_FMT_RGB565;
			break;
		default:
			break;
	}

	return 1;
}

void s3dc_get_img_smaddr(int format, unsigned int baseAddr, int width,
			int height, unsigned int *plane2Addr, unsigned int *plane3Addr)
{
	int num_of_pixel = width * height;

	*plane2Addr = 0xFFFFFFFF;
	*plane3Addr = 0xFFFFFFFF;

	switch (format) {
	case BLT_OUT_N_V_12:
	case BLT_OUT_N_V_21:
		*plane2Addr = baseAddr + num_of_pixel;
		break;
	case BLT_OUT_Y_V_12:
	case BLT_OUT_Y_V_21:
		*plane2Addr = baseAddr + num_of_pixel;
		*plane3Addr = baseAddr + num_of_pixel + (num_of_pixel / 4);
		break;
	case BLT_OUT_IMC_1:
		*plane2Addr = baseAddr + num_of_pixel;
		*plane3Addr = baseAddr + num_of_pixel + (num_of_pixel / 2);
		break;
	case BLT_OUT_IMC_2:
		*plane2Addr = baseAddr + num_of_pixel;
		*plane3Addr = baseAddr + num_of_pixel + width / 2;
		break;
	case BLT_OUT_IMC_3:
		*plane2Addr = baseAddr + num_of_pixel;
		*plane3Addr = baseAddr + num_of_pixel + (num_of_pixel / 2);
		break;
	case BLT_OUT_IMC_4:
		*plane2Addr = baseAddr + num_of_pixel;
		*plane3Addr = baseAddr + num_of_pixel + width / 2;
		break;
	case BLT_OUT_YUV422_PLANAR:
		*plane2Addr = baseAddr + num_of_pixel;
		*plane3Addr = baseAddr + num_of_pixel + (num_of_pixel / 2);
		break;
	}
}

void s3dc_set_fill_rect(int color, char format, int x_start, int y_start,
						 int dp_width, int dp_height, unsigned int sm_addr,
						 unsigned int sm_addr2)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	s3dc_reg_data.blt_fillrect_color_l.as32bits = color & 0xffff;
	s3dc_reg_data.blt_fillrect_color_h.as32bits = color >> 16;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_FIXED_COLOR_FOR_FILLRECT_L,
					s3dc_reg_data.blt_fillrect_color_l.as32bits);

	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_FIXED_COLOR_FOR_FILLRECT_H,
					s3dc_reg_data.blt_fillrect_color_h.as32bits);

	/* src,dst format setting */
	s3dc_reg_data.blt_cmd_d_fmt.as32bits =
					s3dc_get_reg32(s3dc_plat->iobase + BLT_CMD_D_FORMAT);
	s3dc_reg_data.blt_cmd_d_fmt.asbits.src_d_fmt = 0x8;
	s3dc_reg_data.blt_cmd_d_fmt.asbits.dst_d_fmt = format;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_D_FORMAT,
					s3dc_reg_data.blt_cmd_d_fmt.as32bits);

	s3dc_reg_data.blt_cmd_width_l.as32bits = dp_width & 0xffff;
	s3dc_reg_data.blt_cmd_width_h.as32bits = dp_width >> 16;
	s3dc_reg_data.blt_cmd_height.as32bits = dp_height;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_WIDTH_L,
					s3dc_reg_data.blt_cmd_width_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_WIDTH_H,
					s3dc_reg_data.blt_cmd_width_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_HEIGHT,
					s3dc_reg_data.blt_cmd_height.as32bits);

	/* dst info setting */
	s3dc_reg_data.blt_cmd_dst_x_start.as32bits	 = x_start;
	s3dc_reg_data.blt_cmd_dst_y_start.as32bits	 = y_start;
	s3dc_reg_data.blt_cmd_dst_sm_addr_l.as32bits = (sm_addr >> 1) & 0xffff;
	s3dc_reg_data.blt_cmd_dst_sm_addr_h.as32bits = (sm_addr >> 1) >> 16;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_X_START,
					s3dc_reg_data.blt_cmd_dst_x_start.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_Y_START,
					s3dc_reg_data.blt_cmd_dst_y_start.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR_LOW,
					s3dc_reg_data.blt_cmd_dst_sm_addr_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR_HIGH,
					s3dc_reg_data.blt_cmd_dst_sm_addr_h.as32bits);

	if (pctxt->page_flip_on) {
		s3dc_reg_data.blt_cmd_dst_sm_addr2_l.as32bits = (sm_addr2 >> 1)
														& 0xffff;
		s3dc_reg_data.blt_cmd_dst_sm_addr2_h.as32bits = (sm_addr2 >> 1) >> 16;
		s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR2_LOW,
						s3dc_reg_data.blt_cmd_dst_sm_addr2_l.as32bits);
		s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR2_HIGH,
						s3dc_reg_data.blt_cmd_dst_sm_addr2_h.as32bits);
	} else {
		s3dc_reg_data.blt_cmd_dst_sm_addr2_l.as32bits = (sm_addr >> 1) & 0xffff;
		s3dc_reg_data.blt_cmd_dst_sm_addr2_h.as32bits = (sm_addr >> 1) >> 16;
		s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR2_LOW,
						s3dc_reg_data.blt_cmd_dst_sm_addr2_l.as32bits);
		s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR2_HIGH,
						s3dc_reg_data.blt_cmd_dst_sm_addr2_h.as32bits);
	}

	s3dc_reg_data.blt_cmd_dst_sm_width_l.as32bits = dp_width & 0xffff;
	s3dc_reg_data.blt_cmd_dst_sm_width_h.as32bits = dp_width >> 16;
	s3dc_reg_data.blt_cmd_dst_sm_height.as32bits = dp_height;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_WIDTH_LOW,
					s3dc_reg_data.blt_cmd_dst_sm_width_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_WIDTH_HIGH,
					s3dc_reg_data.blt_cmd_dst_sm_width_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_HEIGHT,
					s3dc_reg_data.blt_cmd_dst_sm_height.as32bits);

	s3dc_enable_shadow_reg_up();

	s3dc_reg_data.blt_test_mode0.as32bits = BLT_CMD_FILL_RECT_3000;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_TEST_MODE0, BLT_CMD_FILL_RECT_3000);
	s3dc_poll_reg32(s3dc_plat->iobase + BLT_TEST_MODE0, 0, 1);

	/* page flipping이면 page1에도 fillrect하기 위해 한 번더 커맨드를 전송함. */
	if (pctxt->page_flip_on) {
		s3dc_reg_data.blt_test_mode0.as32bits = BLT_CMD_FILL_RECT_3000;
		s3dc_set_reg32(s3dc_plat->iobase + BLT_TEST_MODE0,
							BLT_CMD_FILL_RECT_3000);
		s3dc_poll_reg32(s3dc_plat->iobase + BLT_TEST_MODE0, 0, 1);
	}
}

void s3dc_fill_rect(void)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	char format = BLT_COLOR_FMT_RGBA8888;
	int	dp_width = pctxt->display_width;
	int dp_height = pctxt->display_height;
	unsigned int sm_addr = pctxt->frame_buffer_sm_addr[4];
	unsigned int sm_addr2 = pctxt->frame_buffer_sm_addr[5];
	unsigned int plane2Addr0, plane3Addr0, plane2Addr1, plane3Addr1;
	unsigned short preInputType, preOutType, preOutYUVType;
	unsigned int color = 0x0;

	css_s3dc("Fill Rect---------------------->\n");

	preInputType	= s3dc_get_reg32(s3dc_plat->iobase + BLT_INPUT_TYPE);
	preOutType		= s3dc_get_reg32(s3dc_plat->iobase + BLT_OUT_TYPE);
	preOutYUVType	= s3dc_get_reg32(s3dc_plat->iobase + BLT_OUT_YUV_TYPE);

	s3dc_get_img_smaddr(pctxt->fb_format, sm_addr, dp_width, dp_height,
						&plane2Addr0, &plane3Addr0);
	s3dc_get_img_smaddr(pctxt->fb_format, sm_addr2, dp_width, dp_height,
						&plane2Addr1, &plane3Addr1);

	s3dc_reg_data.blt_mode.as32bits = 0x0000;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_MODE, 0x0000);
	s3dc_poll_reg32(s3dc_plat->iobase + BLT_MODE, 7, 0);

	s3dc_disable_shadow_reg_up();

	s3dc_reg_data.blt_input_type.as32bits =
		s3dc_get_reg32(s3dc_plat->iobase + BLT_INPUT_TYPE);
	s3dc_reg_data.blt_out_type.as32bits =
		s3dc_get_reg32(s3dc_plat->iobase + BLT_OUT_TYPE);
	s3dc_reg_data.blt_out_yuv_type.as32bits =
		s3dc_get_reg32(s3dc_plat->iobase + BLT_OUT_YUV_TYPE);

	switch (pctxt->fb_format) {
		case BLT_OUT_RGB565:
			format = BLT_COLOR_FMT_RGB565;
			color = 0x000000000;
			s3dc_set_fill_rect(color, format, 0, 0, dp_width, dp_height,
								sm_addr, sm_addr2);
			break;
		case BLT_OUT_RGB8888:
		case BLT_OUT_RGB888:
			format = BLT_COLOR_FMT_RGBA8888;
			color = 0x00000000;
			s3dc_set_fill_rect(color, format, 0, 0, dp_width, dp_height,
								sm_addr, sm_addr2);
			break;
		case BLT_OUT_YUV444_1PLANE_PACKED:
		case BLT_OUT_YUV444_1PLANE_UNPACKED:
			format = BLT_COLOR_FMT_RGBA8888;
			color = 0x80801000;	/* 80 80 10 */
			s3dc_set_fill_rect(color, format, 0, 0, dp_width, dp_height,
								sm_addr, sm_addr2);
			break;
		case BLT_OUT_YUV422_1PLANE_PACKED:
			format = BLT_COLOR_FMT_RGB565;
			s3dc_reg_data.blt_input_type.asbits.ld_order	= 0;
			s3dc_reg_data.blt_input_type.asbits.lz_position = 0;
			s3dc_reg_data.blt_input_type.asbits.lpack_en	= 0;
			s3dc_set_reg32(s3dc_plat->iobase + BLT_INPUT_TYPE,
							s3dc_reg_data.blt_input_type.as32bits);

			s3dc_reg_data.blt_out_type.asbits.od_order		= 1;
			s3dc_reg_data.blt_out_type.asbits.oz_position	= 0;
			s3dc_reg_data.blt_out_type.asbits.opack_en		= 0;
			s3dc_set_reg32(s3dc_plat->iobase + BLT_OUT_TYPE,
							s3dc_reg_data.blt_out_type.as32bits);

			s3dc_reg_data.blt_out_yuv_type.asbits.oy_en		= 0;
			s3dc_reg_data.blt_out_yuv_type.asbits.y_fmt		= 0;
			s3dc_reg_data.blt_out_yuv_type.asbits.y422_mode	= 0;
			s3dc_reg_data.blt_out_yuv_type.asbits.y420_mode	= 0;
			s3dc_set_reg32(s3dc_plat->iobase + BLT_OUT_YUV_TYPE,
							s3dc_reg_data.blt_out_yuv_type.as32bits);
			if (pctxt->fb_order == BLT_OUT_ORDER_YUYV ||
				pctxt->fb_order == BLT_OUT_ORDER_YVYU)
				color = 0x80008000;	/* 80 10 */
			else
				color = 0x10100000;	/* 10 80 */

			s3dc_set_fill_rect(color, format, 0, 0, dp_width, dp_height,
								sm_addr, sm_addr2);
			break;

		case BLT_OUT_YUV422_PLANAR:
			format = BLT_COLOR_FMT_RGB565;
			s3dc_reg_data.blt_input_type.asbits.ld_order	= 0;
			s3dc_reg_data.blt_input_type.asbits.lz_position = 0;
			s3dc_reg_data.blt_input_type.asbits.lpack_en	= 0;
			s3dc_set_reg32(s3dc_plat->iobase + BLT_INPUT_TYPE,
							s3dc_reg_data.blt_input_type.as32bits);

			s3dc_reg_data.blt_out_type.asbits.od_order		= 1;
			s3dc_reg_data.blt_out_type.asbits.oz_position	= 0;
			s3dc_reg_data.blt_out_type.asbits.opack_en		= 0;
			s3dc_set_reg32(s3dc_plat->iobase + BLT_OUT_TYPE,
							s3dc_reg_data.blt_out_type.as32bits);

			s3dc_reg_data.blt_out_yuv_type.asbits.oy_en		= 0;
			s3dc_reg_data.blt_out_yuv_type.asbits.y_fmt		= 0;
			s3dc_reg_data.blt_out_yuv_type.asbits.y422_mode	= 0;
			s3dc_reg_data.blt_out_yuv_type.asbits.y420_mode	= 0;
			s3dc_set_reg32(s3dc_plat->iobase + BLT_OUT_YUV_TYPE,
							s3dc_reg_data.blt_out_yuv_type.as32bits);

			color = 0x10008000;	/* 10 10 */
			s3dc_set_fill_rect(color, format, 0, 0, dp_width, dp_height / 2,
								sm_addr, sm_addr2);
			color = 0x80100000;	/* 80 80 */
			s3dc_set_fill_rect(color, format, 0, 0, dp_width / 2, dp_height,
								plane2Addr0, plane2Addr1);
			break;

		case BLT_OUT_Y_V_12:
		case BLT_OUT_Y_V_21:
			format = BLT_COLOR_FMT_RGB565;

			s3dc_reg_data.blt_input_type.asbits.ld_order	= 0;
			s3dc_reg_data.blt_input_type.asbits.lz_position = 0;
			s3dc_reg_data.blt_input_type.asbits.lpack_en	= 0;
			s3dc_set_reg32(s3dc_plat->iobase + BLT_INPUT_TYPE,
							s3dc_reg_data.blt_input_type.as32bits);

			s3dc_reg_data.blt_out_type.asbits.od_order		= 1;
			s3dc_reg_data.blt_out_type.asbits.oz_position	= 0;
			s3dc_reg_data.blt_out_type.asbits.opack_en	 	= 0;
			s3dc_set_reg32(s3dc_plat->iobase + BLT_OUT_TYPE,
							s3dc_reg_data.blt_out_type.as32bits);

			s3dc_reg_data.blt_out_yuv_type.asbits.oy_en		= 0;
			s3dc_reg_data.blt_out_yuv_type.asbits.y_fmt		= 0;
			s3dc_reg_data.blt_out_yuv_type.asbits.y422_mode	= 0;
			s3dc_reg_data.blt_out_yuv_type.asbits.y420_mode	= 0;
			s3dc_set_reg32(s3dc_plat->iobase + BLT_OUT_YUV_TYPE,
							s3dc_reg_data.blt_out_yuv_type.as32bits);

			color = 0x10008000;
			s3dc_set_fill_rect(color, format, 0, 0, dp_width, dp_height / 2,
							sm_addr, sm_addr2);
			color = 0x80100000;
			{

				int realHeight = (int)(dp_width / 2 * dp_height / 4 / 2);
				s3dc_set_fill_rect(color, format, 0, 0, 2, realHeight,
								plane2Addr0, plane2Addr1);
				s3dc_set_fill_rect(color, format, 0, 0, 2, realHeight,
								plane3Addr0, plane3Addr1);
			}
			break;

		case BLT_OUT_N_V_12:
		case BLT_OUT_N_V_21:
		case BLT_OUT_IMC_2:
		case BLT_OUT_IMC_4:
			format = BLT_COLOR_FMT_RGB565;

			s3dc_reg_data.blt_input_type.asbits.ld_order	= 0;
			s3dc_reg_data.blt_input_type.asbits.lz_position = 0;
			s3dc_reg_data.blt_input_type.asbits.lpack_en	= 0;
			s3dc_set_reg32(s3dc_plat->iobase + BLT_INPUT_TYPE,
							s3dc_reg_data.blt_input_type.as32bits);

			s3dc_reg_data.blt_out_type.asbits.od_order		= 1;
			s3dc_reg_data.blt_out_type.asbits.oz_position	= 0;
			s3dc_reg_data.blt_out_type.asbits.opack_en		= 0;
			s3dc_set_reg32(s3dc_plat->iobase + BLT_OUT_TYPE,
							s3dc_reg_data.blt_out_type.as32bits);

			s3dc_reg_data.blt_out_yuv_type.asbits.oy_en		= 0;
			s3dc_reg_data.blt_out_yuv_type.asbits.y_fmt		= 0;
			s3dc_reg_data.blt_out_yuv_type.asbits.y422_mode	= 0;
			s3dc_reg_data.blt_out_yuv_type.asbits.y420_mode	= 0;
			s3dc_set_reg32(s3dc_plat->iobase + BLT_OUT_YUV_TYPE,
							s3dc_reg_data.blt_out_yuv_type.as32bits);

			color = 0x10008000;
			s3dc_set_fill_rect(color, format, 0, 0, dp_width, dp_height / 2,
								sm_addr, sm_addr2);
			color = 0x80100000;
			s3dc_set_fill_rect(color, format, 0, 0, dp_width / 2, dp_height / 2,
								plane2Addr0, plane2Addr1);
			break;

		case BLT_OUT_IMC_1:
		case BLT_OUT_IMC_3:
			format = BLT_COLOR_FMT_RGB565;

			s3dc_reg_data.blt_input_type.asbits.ld_order	= 0;
			s3dc_reg_data.blt_input_type.asbits.lz_position = 0;
			s3dc_reg_data.blt_input_type.asbits.lpack_en	= 0;
			s3dc_set_reg32(s3dc_plat->iobase + BLT_INPUT_TYPE,
							s3dc_reg_data.blt_input_type.as32bits);

			s3dc_reg_data.blt_out_type.asbits.od_order		= 1;
			s3dc_reg_data.blt_out_type.asbits.oz_position	= 0;
			s3dc_reg_data.blt_out_type.asbits.opack_en		= 0;
			s3dc_set_reg32(s3dc_plat->iobase + BLT_OUT_TYPE,
							s3dc_reg_data.blt_out_type.as32bits);

			s3dc_reg_data.blt_out_yuv_type.asbits.oy_en		= 0;
			s3dc_reg_data.blt_out_yuv_type.asbits.y_fmt		= 0;
			s3dc_reg_data.blt_out_yuv_type.asbits.y422_mode	= 0;
			s3dc_reg_data.blt_out_yuv_type.asbits.y420_mode	= 0;
			s3dc_set_reg32(s3dc_plat->iobase + BLT_OUT_YUV_TYPE,
							s3dc_reg_data.blt_out_yuv_type.as32bits);

			color = 0x10008000;
			s3dc_set_fill_rect(color, format, 0, 0, dp_width, dp_height / 2,
								sm_addr, sm_addr2);
			color = 0x80100000;
			s3dc_set_fill_rect(color, format, 0, 0, dp_width, dp_height / 2,
								plane2Addr0, plane2Addr1);
			break;

		default:
			return;
	}

	s3dc_reg_data.blt_input_type.as32bits	= preInputType;
	s3dc_reg_data.blt_out_type.as32bits 	= preOutType;
	s3dc_reg_data.blt_out_yuv_type.as32bits = preOutYUVType;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_INPUT_TYPE, preInputType);
	s3dc_set_reg32(s3dc_plat->iobase + BLT_OUT_TYPE, preOutType);
	s3dc_set_reg32(s3dc_plat->iobase + BLT_OUT_YUV_TYPE, preOutYUVType);

	css_s3dc("<--------- End of Fill Rect\n");
}

/*
 * FUNC : s3dc_init_blt
 * DESC : BLT input, output register setting
 */
void s3dc_init_blt(void)
{
	int sbt_width;
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	css_s3dc("BLT setting\n");

	sbt_width = pctxt->cf_out_width;

#if 0 /* for warning */
	sbt_height = pctxt->cf_out_height;

	sbtPage0Addr = pctxt->frame_buffer_sm_addr[4];
	if (pctxt->page_flip_on)
		sbt_page1_addr = pctxt->frame_buffer_sm_addr[5];
	else
		sbt_page1_addr = pctxt->frame_buffer_sm_addr[4];
#endif

	/* src0 setting */
	s3dc_reg_data.blt_cmd_src_sm_width_l.as32bits = sbt_width & 0xffff;
	s3dc_reg_data.blt_cmd_src_sm_width_h.as32bits = sbt_width >> 16;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_SRC_SM_WIDTH_LOW,
					s3dc_reg_data.blt_cmd_src_sm_width_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_SRC_SM_WIDTH_HIGH,
					s3dc_reg_data.blt_cmd_src_sm_width_h.as32bits);

	/* src1 setting */
	s3dc_reg_data.blt_cmd_src2_sm_width_l.as32bits = sbt_width & 0xffff;
	s3dc_reg_data.blt_cmd_src2_sm_width_h.as32bits = sbt_width >> 16;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_SRC2_SM_WIDTH_LOW,
					s3dc_reg_data.blt_cmd_src2_sm_width_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_SRC2_SM_WIDTH_HIGH,
					s3dc_reg_data.blt_cmd_src2_sm_width_h.as32bits);

	/* BLT SBL value */
	s3dc_reg_data.blt_sbl.as32bits = pctxt->blt_sbl_val;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_SBL, pctxt->blt_sbl_val);
	/* scale filter */
	s3dc_reg_data.blt_filter_scaler_en.as32bits = 0x6;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_FILTER_SCALER_EN, 0x06);

	css_s3dc("BLT output format setting\n");

	/* BLT input / output format setting */

	s3dc_reg_data.blt_input_type.asbits.ld_order	= pctxt->blti_data_order;
	s3dc_reg_data.blt_input_type.asbits.lz_position = pctxt->blti_z_order;
	s3dc_reg_data.blt_input_type.asbits.lpack_en	= pctxt->blti_24bit_en;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_INPUT_TYPE,
					s3dc_reg_data.blt_input_type.as32bits);

	s3dc_reg_data.blt_out_type.asbits.od_order		= pctxt->blto_data_order;
	s3dc_reg_data.blt_out_type.asbits.oz_position	= pctxt->blto_z_order;
	s3dc_reg_data.blt_out_type.asbits.opack_en		= pctxt->blto_24bit_en;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_OUT_TYPE,
					s3dc_reg_data.blt_out_type.as32bits);

	s3dc_reg_data.blt_out_yuv_type.asbits.oy_en		= pctxt->blto_yuv_enable;
	s3dc_reg_data.blt_out_yuv_type.asbits.y_fmt		= pctxt->blto_yuv_format;
	s3dc_reg_data.blt_out_yuv_type.asbits.y422_mode	= pctxt->blto_yuv422_mode;
	s3dc_reg_data.blt_out_yuv_type.asbits.y420_mode	= pctxt->blto_yuv420_mode;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_OUT_YUV_TYPE,
					s3dc_reg_data.blt_out_yuv_type.as32bits);

	/*
	 * full size SBS or TB enable
	 * s3dc_set_reg32(BLT_FULL_EN, pctxt->blt_fullsize_formatting_on << 3);
	 * s3dc_set_blt_in_s3d_mode()에서 셋팅하므로 삭제
	 */
}

void s3dc_set_blt_in_s3d_mode(void)
{
	int sbt_fmt;
	int sbt_width, sbt_height;
	int dst_x_offset = 0, dst_y_offset = 0;
	unsigned int sbt_page1_addr;
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	sbt_width = pctxt->cf_out_width;
	sbt_height = pctxt->cf_out_height;

	s3dc_reg_data.blt_mode.as32bits = 0x0001;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_MODE, 0x0001);
	s3dc_poll_reg32(s3dc_plat->iobase + BLT_MODE, 7, 1);

	s3dc_reg_data.blt_cmd_width_l.as32bits = sbt_width & 0xffff;
	s3dc_reg_data.blt_cmd_width_h.as32bits = sbt_width >> 16;
	s3dc_reg_data.blt_cmd_height.as32bits = sbt_height;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_WIDTH_L,
					s3dc_reg_data.blt_cmd_width_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_WIDTH_H,
					s3dc_reg_data.blt_cmd_width_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_HEIGHT,
					s3dc_reg_data.blt_cmd_height.as32bits);

	if (pctxt->page_flip_on)
		sbt_page1_addr = pctxt->frame_buffer_sm_addr[5];
	else
		sbt_page1_addr = pctxt->frame_buffer_sm_addr[4];

	s3dc_reg_data.blt_cmd_dst_sm_addr_l.as32bits
				= (pctxt->frame_buffer_sm_addr[4] >> 1) & 0xffff; /*page 0*/
	s3dc_reg_data.blt_cmd_dst_sm_addr_h.as32bits
				= (pctxt->frame_buffer_sm_addr[4] >> 1) >> 16;
	s3dc_reg_data.blt_cmd_dst_sm_addr2_l.as32bits
				= (sbt_page1_addr >> 1) & 0xffff; /*page 1*/
	s3dc_reg_data.blt_cmd_dst_sm_addr2_h.as32bits
				= (sbt_page1_addr >> 1) >> 16;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR_LOW,
					s3dc_reg_data.blt_cmd_dst_sm_addr_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR_HIGH,
					s3dc_reg_data.blt_cmd_dst_sm_addr_h.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR2_LOW,
					s3dc_reg_data.blt_cmd_dst_sm_addr2_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR2_HIGH,
					s3dc_reg_data.blt_cmd_dst_sm_addr2_h.as32bits);

	/* Src0,1의 설정은 s3dc_init_blt에서 수행. */

	/* dst setting */
	dst_y_offset = 0;
	switch (pctxt->curr_mode) { /* 각 S3D모드 별로 dstXoffset계산 */
	case MODE_2D:
	case MODE_S3D_MANUAL:
	case MODE_S3D_PIXEL:
	case MODE_S3D_LINE:
	case MODE_S3D_RED_BLUE:
	case MODE_S3D_RED_GREEN:
	case MODE_S3D_SUB_PIXEL:
	case MODE_S3D_TOP_BOTTOM:
	case MODE_S3D_TOP_BOTTOM_FULL:
		dst_x_offset = (pctxt->display_width - pctxt->cf_out_width) / 2;
		break;

	case MODE_S3D_SIDE_BY_SIDE:
		dst_x_offset = (pctxt->display_width - pctxt->cf_out_width) / 4;
		break;

	case MODE_S3D_SIDE_BY_SIDE_FULL:
		dst_x_offset = (pctxt->display_width - (pctxt->cf_out_width*2)) / 4;
		break;
	}

	s3dc_reg_data.blt_cmd_dst_x_start.as32bits = dst_x_offset;
	s3dc_reg_data.blt_cmd_dst_y_start.as32bits = dst_y_offset;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_X_START, dst_x_offset);
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_Y_START, dst_y_offset);

	{ /* side-bar 와 fullSizeSBS 상관관계 때문에 추가된 코드임*/
		int dstSMWidth, dstSMHeight;

		if (pctxt->curr_mode == MODE_S3D_SIDE_BY_SIDE_FULL) {
			if (pctxt->display_width > (pctxt->cf_out_width * 2)) {
				/* fullSize SidebySide 모드에서 side-bar가 있는 경우 */
				dstSMWidth = pctxt->display_width/2;
				dstSMHeight = pctxt->display_height;
			} else {
				/* fullSize SidebySide 모드에서 side-bar가 없는 경우 */
				dstSMWidth = pctxt->cf_out_width;
				dstSMHeight = pctxt->cf_out_height;
			}
		} else if (pctxt->curr_mode == MODE_S3D_TOP_BOTTOM_FULL) {
			/* fullSize TopBottom Mode */
			if (pctxt->display_width > pctxt->cf_out_width) {
				/* sidebar있는 경우 */
				dstSMWidth = pctxt->display_width;
				dstSMHeight = pctxt->display_height/2;
			} else {
				/* sidebar없는 경우 */
				dstSMWidth = pctxt->cf_out_width;
				dstSMHeight = pctxt->cf_out_height;
			}
		} else {/* fullSize 모드가 아닌 일반 S3D모드인 경우 */
			/* 일반 S3D모드에서는 side-bar가 없는 경우에는
			 * display size == cfOutSize가 되므로 side-bar가 있는 경우나
			 * 없는 경우 모두 display size를 기준으로 dstSMW,H
			 * 를 셋팅하면 된다.
			 */
			dstSMWidth = pctxt->display_width;
			dstSMHeight = pctxt->display_height;
		}

		s3dc_reg_data.blt_cmd_dst_sm_width_l.as32bits = dstSMWidth & 0xffff;
		s3dc_reg_data.blt_cmd_dst_sm_width_h.as32bits = dstSMWidth >> 16;
		s3dc_reg_data.blt_cmd_dst_sm_height.as32bits = dstSMHeight;
		s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_WIDTH_LOW,
						s3dc_reg_data.blt_cmd_dst_sm_width_l.as32bits);
		s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_WIDTH_HIGH,
						s3dc_reg_data.blt_cmd_dst_sm_width_h.as32bits);
		s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_HEIGHT,
						s3dc_reg_data.blt_cmd_dst_sm_height.as32bits);
	}

	sbt_fmt = ((pctxt->blt_dst_format & 0xffff) << 4)
			| (pctxt->blt_src_format & 0xffff);

	s3dc_reg_data.blt_cmd_d_fmt.as32bits = sbt_fmt;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_D_FORMAT, sbt_fmt);


	/* full size SBS or TB enable */
	s3dc_reg_data.blt_full_en.as32bits = pctxt->blt_fullsize_formatting_on << 3;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_FULL_EN,
					s3dc_reg_data.blt_full_en.as32bits);
	s3dc_reg_data.blt_test_mode0.as32bits = BLT_CMD_MERGE_PIXEL_3000;
	s3dc_set_reg32(s3dc_plat->iobase + BLT_TEST_MODE0,
					BLT_CMD_MERGE_PIXEL_3000);
}


/******
*
*  end of s3dcBLT.c
*
********/


/******
*
*  s3dcContext.c
*
********/

/*
 * FUNC : s3dc_enable_shadow_reg_up
 * DESC : BLT, CF shadow register update enable
 */
void s3dc_enable_shadow_reg_up(void)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	css_s3dc("Enable Shadow Register\n");

	s3dc_reg_data.top_en_reset.as32bits =
						s3dc_get_reg32(s3dc_plat->iobase + TOP_EN_RESET);
	s3dc_reg_data.top_en_reset.asbits.nxs3dc_on = pctxt->cf_enable;
	s3dc_reg_data.top_en_reset.asbits.cf_reg_update = 1;
	s3dc_reg_data.top_en_reset.asbits.blt_reg_update = 1;
	s3dc_set_reg32(s3dc_plat->iobase + TOP_EN_RESET,
					s3dc_reg_data.top_en_reset.as32bits);
 }
/*
 * FUNC : s3dc_disable_shadow_reg_up
 * DESC : BLT, CF shadow register update disable
 */
void s3dc_disable_shadow_reg_up(void)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	css_s3dc("Disable Shadow Register\n");

	s3dc_reg_data.top_en_reset.as32bits =
						s3dc_get_reg32(s3dc_plat->iobase + TOP_EN_RESET);
	s3dc_reg_data.top_en_reset.asbits.nxs3dc_on = pctxt->cf_enable;
	s3dc_reg_data.top_en_reset.asbits.cf_reg_update = 0;
	s3dc_reg_data.top_en_reset.asbits.blt_reg_update = 0;
	s3dc_set_reg32(s3dc_plat->iobase + TOP_EN_RESET,
					s3dc_reg_data.top_en_reset.as32bits);
}

void s3dc_stop_trans(void)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	s3dc_reg_data.top_en_reset.as32bits =
						s3dc_get_reg32(s3dc_plat->iobase + TOP_EN_RESET);
	s3dc_reg_data.top_en_reset.asbits.trans_stop_req = 1;
	s3dc_set_reg32(s3dc_plat->iobase + TOP_EN_RESET,
					s3dc_reg_data.top_en_reset.as32bits);
}

/*
 * FUNC : s3dc_set_cf_context
 * DESC : CF-eng context setting
 */
void s3dc_set_cf_context(CFSizeInfo cf_size_info)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();

	/* input size 관련 셋팅 */

	pctxt->display_width = cf_size_info.cf_out_w;
	pctxt->display_height = cf_size_info.cf_out_h;

	pctxt->cam0_width = cf_size_info.cam0_width;
	pctxt->cam0_height = cf_size_info.cam0_height;
	pctxt->cam1_width = cf_size_info.cam1_width;
	pctxt->cam1_height = cf_size_info.cam1_height;

	pctxt->cam0_crop_width = cf_size_info.cam0_crop_w;
	pctxt->cam0_crop_height = cf_size_info.cam0_crop_h;
	pctxt->cam1_crop_width = cf_size_info.cam1_crop_w;
	pctxt->cam1_crop_height = cf_size_info.cam1_crop_h;

	if (pctxt->cam0_crop_width % 8 != 0)
		pctxt->cam0_crop_width = pctxt->cam0_crop_width
							- pctxt->cam0_crop_width % 8 + 8;
	if (cf_size_info.cam1_crop_w % 8 != 0)
		pctxt->cam1_crop_width = pctxt->cam1_crop_width
							- pctxt->cam1_crop_width % 8 + 8;

	pctxt->cam0_crop_w_correction = pctxt->cam0_crop_width
									- cf_size_info.cam0_crop_w;
	pctxt->cam1_crop_w_correction = pctxt->cam1_crop_width
									- cf_size_info.cam1_crop_w;

	pctxt->tilt_bypass_en = 1;
	pctxt->cf_out_width	= cf_size_info.cf_out_w;
	pctxt->cf_out_height	= cf_size_info.cf_out_h;

	css_s3dc("Camera0 Total Width : %d \n", pctxt->cam0_width);
	css_s3dc("Camera0 Total Height : %d \n", pctxt->cam0_height);
	css_s3dc("Camera1 Total Width : %d \n", pctxt->cam1_width);
	css_s3dc("Camera1 Total Height : %d \n", pctxt->cam1_height);

	pctxt->cam0_pixel_start
			= (pctxt->cam0_width - cf_size_info.cam0_crop_w) / 2;
	pctxt->cam1_pixel_start
			= (pctxt->cam1_width - cf_size_info.cam1_crop_w) / 2;
	pctxt->cam0_line_start
			= (pctxt->cam0_height - pctxt->cam0_crop_height) / 2;
	pctxt->cam1_line_start
			= (pctxt->cam1_height - pctxt->cam1_crop_height) / 2;

	pctxt->page_flip_on = 0;/* USE_PAGE_FLIPPING; */

	pctxt->cf_enable = 0;
	pctxt->blt_fullsize_formatting_on = 0;

	/* s3d type, camera select(2D), deci_mode, manual en, s3d en */
	pctxt->pre_mode		= 0;
	pctxt->curr_mode 	= 0;
	pctxt->deci_filter_en = 0;
	pctxt->s3d_type		= 0;
	pctxt->cam_select	= 0;
	pctxt->deci_mode 	= 0;
	pctxt->manual_en 	= 0;
	pctxt->s3d_en		= 0;
	pctxt->rotate_en 	= 0;

	/* auto-convergence setting */
	pctxt->ac_enable = 0;
	pctxt->ac_quick_enable = 0;
	{
		int input_w = pctxt->cf_out_width;
		int input_h = pctxt->cf_out_height;
		int width_div4 = input_w / 4;
		int height_div4 = input_h / 4;

		if (input_w <= 200) {
			css_s3dc("CF Out Size Width < 200. \n");
			pctxt->ac_area_x_start = 0;
			pctxt->ac_area_y_start = 0;
			pctxt->ac_area_width = input_w;
			pctxt->ac_area_height = input_h;
		} else {
			if ((width_div4*2) <= 200) {
				pctxt->ac_area_x_start = 0;
				pctxt->ac_area_y_start = 0;
				pctxt->ac_area_width = input_w;
				pctxt->ac_area_height = input_h;
			} else {
				pctxt->ac_area_x_start = width_div4;
				pctxt->ac_area_y_start = height_div4;
				pctxt->ac_area_width = width_div4 * 2;
				pctxt->ac_area_height = height_div4 * 2;
			}
		}
		pctxt->ac_edge_tol = 50;
	}

	/* LR change */
	pctxt->change_lr = 1;

	/* auto luminance balance setting */
	pctxt->wb_enable	= 0;
	pctxt->hwb_enable	= 0;
	pctxt->wb_hold		= 0;
	pctxt->wb_auto_stop_en	= 0;
	pctxt->hwb_value	= 1;
	pctxt->wb_avg_up_cnt	= 1;
	pctxt->wb_fstable_cnt	= 2;

	/* color contorl */
	pctxt->color_ctrl_en[0] = 0;
	pctxt->color_ctrl_en[1] = 0;
	pctxt->color_pattern_en = 0;
	pctxt->color_no_color_en = 1;
}

void s3dc_set_rotate_value(BrRotationInfo *rot_info)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	s3dc_disable_shadow_reg_up();

	pctxt->rotate_en = rot_info->rot_en;

	s3dc_reg_data.cf_br_rot_enable.as32bits = rot_info->rot_en;
	s3dc_set_reg32(s3dc_plat->iobase + ROT_ENABLE,
					s3dc_reg_data.cf_br_rot_enable.as32bits);

	s3dc_reg_data.cf_br_rot_degree_sign.as32bits = rot_info->degree_sign;
	s3dc_set_reg32(s3dc_plat->iobase + ROT_DEGREE_SIGN,
					s3dc_reg_data.cf_br_rot_degree_sign.as32bits);

	/* BLFR_SIN = rotSin_LLINT; */
	s3dc_reg_data.cf_br_rot_sin_l.asbits.br_sin_l = rot_info->rot_sin_l;
	s3dc_reg_data.cf_br_rot_sin_h.asbits.br_sin_h = rot_info->rot_sin_h;
	s3dc_set_reg32(s3dc_plat->iobase + ROT_SIN_L,
					s3dc_reg_data.cf_br_rot_sin_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + ROT_SIN_H,
					s3dc_reg_data.cf_br_rot_sin_h.as32bits);

	/* BR_COS = rotCos_LLINT; */
	s3dc_reg_data.cf_br_rot_cos_l.asbits.br_cos_l = rot_info->rot_cos_l;
	s3dc_reg_data.cf_br_rot_cos_h.asbits.br_cos_h = rot_info->rot_cos_h;
	s3dc_set_reg32(s3dc_plat->iobase + ROT_COS_L,
					s3dc_reg_data.cf_br_rot_cos_l.as32bits);
	s3dc_set_reg32(s3dc_plat->iobase + ROT_COS_H,
					s3dc_reg_data.cf_br_rot_cos_h.as32bits);

	/* BR_CSC = rotCsc_LLINT; */
	s3dc_reg_data.cf_br_rot_csc_l.asbits.br_csc_l = rot_info->rot_csc_l;
	s3dc_set_reg32(s3dc_plat->iobase + ROT_CSC_L,
					s3dc_reg_data.cf_br_rot_csc_l.as32bits);
	s3dc_reg_data.cf_br_rot_csc_m.asbits.br_csc_m = rot_info->rot_csc_m;
	s3dc_set_reg32(s3dc_plat->iobase + ROT_CSC_M,
					s3dc_reg_data.cf_br_rot_csc_m.as32bits);
	s3dc_reg_data.cf_br_rot_csc_h.asbits.br_csc_h = rot_info->rot_csc_h;
	s3dc_set_reg32(s3dc_plat->iobase + ROT_CSC_H,
					s3dc_reg_data.cf_br_rot_csc_h.as32bits);

	/* BR_COT = rotCot_LLINT; */
	s3dc_reg_data.cf_br_rot_cot_l.asbits.br_cot_l = rot_info->rot_cot_l;
	s3dc_set_reg32(s3dc_plat->iobase + ROT_COT_L,
					s3dc_reg_data.cf_br_rot_cot_l.as32bits);
	s3dc_reg_data.cf_br_rot_cot_m.asbits.br_cot_m = rot_info->rot_cot_m;
	s3dc_set_reg32(s3dc_plat->iobase + ROT_COT_M,
					s3dc_reg_data.cf_br_rot_cot_m.as32bits);
	s3dc_reg_data.cf_br_rot_cot_h.asbits.br_cot_h = rot_info->rot_cot_h;
	s3dc_set_reg32(s3dc_plat->iobase + ROT_COT_H,
					s3dc_reg_data.cf_br_rot_cot_h.as32bits);

	s3dc_enable_shadow_reg_up();
}

/*
 * FUNC : s3dc_alloc_stack_memory
 * DESC : stacked memory allocation
 */
#if 0 /* s3dcAllocationMemory ver 1.1 */
void s3dc_alloc_stack_memory(CFInputFormatInfo cf_input_format,
			CFInputDMASizeInfo dma_size_info)
{
	int colorSize1;
	int cam0_alloc_size, cam1_alloc_size, fb_alloc_size;
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	int temp = 32;
	int dataSize;

	if (pctxt->blti_format == BLT_INPUT_RGB8888)	/* 32bit */
		colorSize1 = 4;
	else if (pctxt->blti_format == BLT_INPUT_RGB888) /* 24bit */
		colorSize1 = 3;

	if (pctxt->deci_mode & 0x01) {
		cam0_alloc_size = (int)(pctxt->cam0_crop_width
							* pctxt->cam0_crop_height * colorSize1 * 14 / 20);
		cam1_alloc_size = (int)(pctxt->cam1_crop_width
							* pctxt->cam1_crop_height * colorSize1 / 2);
	} else {
		cam0_alloc_size = (int)(pctxt->cam0_crop_width
							* pctxt->cam0_crop_height * colorSize1 * 14 / 10);
		cam1_alloc_size = (int)(pctxt->cam1_crop_width
							* pctxt->cam1_crop_height * colorSize1);
	}

	fb_alloc_size = pctxt->fb_data_size;

	if (cf_input_format.c0_input_mode != CF_INPUT_NONE) {
		dataSize = dma_size_info.c0_total_w * dma_size_info.c0_total_h * 4;
		pctxt->frame_buffer_sm_addr[0] = (temp * (dataSize / temp)) + temp;
	}

	if (cf_input_format.c1_input_mode != CF_INPUT_NONE) {
		dataSize = dma_size_info.c1_total_w * dma_size_info.c1_total_h * 4;
		pctxt->frame_buffer_sm_addr[0] = pctxt->frame_buffer_sm_addr[0]
									+ (temp * (dataSize / temp)) + temp;
	}

	if ((cam0_alloc_size == 0) || (cam1_alloc_size == 0))
		pctxt->frame_buffer_sm_addr[1] = pctxt->frame_buffer_sm_addr[0];
	else
		pctxt->frame_buffer_sm_addr[1] = pctxt->frame_buffer_sm_addr[0]
									+ (temp * (cam0_alloc_size/temp)) +temp;

	if (cam0_alloc_size == 0)
		pctxt->frame_buffer_sm_addr[2] = pctxt->frame_buffer_sm_addr[1]
									+ (temp * (cam1_alloc_size/temp)) +temp;
	else
		pctxt->frame_buffer_sm_addr[2] = pctxt->frame_buffer_sm_addr[1]
									+ (temp * (cam0_alloc_size/temp)) +temp;

	if ((cam0_alloc_size == 0) || (cam1_alloc_size == 0))
		pctxt->frame_buffer_sm_addr[3] = pctxt->frame_buffer_sm_addr[2];
	else
		pctxt->frame_buffer_sm_addr[3] = pctxt->frame_buffer_sm_addr[2]
									+ (temp * (cam1_alloc_size/temp)) +temp;

	if (cam1_alloc_size == 0)
		pctxt->frame_buffer_sm_addr[4] = pctxt->frame_buffer_sm_addr[3]
									+ (temp * (cam0_alloc_size/temp)) +temp;
	else
		pctxt->frame_buffer_sm_addr[4] = pctxt->frame_buffer_sm_addr[3]
									+ (temp * (cam1_alloc_size/temp)) +temp;

	pctxt->frame_buffer_sm_addr[5] = pctxt->frame_buffer_sm_addr[4]
									+ (temp * (fb_alloc_size/temp)) +temp;

	css_s3dc("MemAlloc[0] = 0x%08x \n", pctxt->frame_buffer_sm_addr[0]);
	css_s3dc("MemAlloc[1] = 0x%08x \n", pctxt->frame_buffer_sm_addr[1]);
	css_s3dc("MemAlloc[2] = 0x%08x \n", pctxt->frame_buffer_sm_addr[2]);
	css_s3dc("MemAlloc[3] = 0x%08x \n", pctxt->frame_buffer_sm_addr[3]);
	css_s3dc("MemAlloc[4] = 0x%08x \n", pctxt->frame_buffer_sm_addr[4]);
	css_s3dc("MemAlloc[5] = 0x%08x \n", pctxt->frame_buffer_sm_addr[5]);
}
#else /* ver 1.0 */
void s3dc_alloc_stack_memory(unsigned int fmem)
{
	int colorSize1 = 0;
	int cam0_alloc_size, cam1_alloc_size, fb_alloc_size;
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	int temp = 32;
	/*
	 * 32의 배수로 주소를 지정하기 위해 필요한 변수
	 * (64bit 기준으로 0,1비트가 0이 되도록.)
	 */

	if (pctxt->blti_format == BLT_INPUT_RGB8888)
		colorSize1 = 4;
	else if (pctxt->blti_format == BLT_INPUT_RGB888)
		colorSize1 = 3;

	if (pctxt->deci_mode & 0x01) {
		cam0_alloc_size = (int)(pctxt->cam0_crop_width
							* pctxt->cam0_crop_height * colorSize1 * 14 / 20);
		cam1_alloc_size = (int)(pctxt->cam1_crop_width
							* pctxt->cam1_crop_height * colorSize1 / 2);
	} else {
		cam0_alloc_size = (int)(pctxt->cam0_crop_width
							* pctxt->cam0_crop_height * colorSize1 * 14 / 10);
		cam1_alloc_size = (int)(pctxt->cam1_crop_width
							* pctxt->cam1_crop_height * colorSize1);
	}

	fb_alloc_size = pctxt->fb_data_size;

	css_s3dc("s3dc_alloc_stack_memory\n");
	css_s3dc(" - cam0_alloc_size = %d\n", cam0_alloc_size);
	css_s3dc(" - cam1_alloc_size = %d\n", cam1_alloc_size);
	css_s3dc(" - colorSize1    = %d\n", colorSize1);
	css_s3dc(" - fb_alloc_size   = %d\n", fb_alloc_size);

	s3dc_frame_memory_base_addr = fmem;
	pctxt->frame_buffer_sm_addr[0] = s3dc_frame_memory_base_addr;

	if ((cam0_alloc_size == 0) || (cam1_alloc_size == 0))
		pctxt->frame_buffer_sm_addr[1] = s3dc_frame_memory_base_addr;
	else
		pctxt->frame_buffer_sm_addr[1] = pctxt->frame_buffer_sm_addr[0]
									+ (temp * (cam0_alloc_size/temp)) + temp;

	if (cam0_alloc_size == 0)
		pctxt->frame_buffer_sm_addr[2] = pctxt->frame_buffer_sm_addr[1]
									+ (temp * (cam1_alloc_size/temp)) + temp;
	else
		pctxt->frame_buffer_sm_addr[2] = pctxt->frame_buffer_sm_addr[1]
									+ (temp * (cam0_alloc_size/temp)) + temp;

	if ((cam0_alloc_size == 0) || (cam1_alloc_size == 0))
		pctxt->frame_buffer_sm_addr[3] = pctxt->frame_buffer_sm_addr[2];
	else
		pctxt->frame_buffer_sm_addr[3] = pctxt->frame_buffer_sm_addr[2]
									+ (temp * (cam1_alloc_size/temp)) + temp;

	/* dst가 16bit일때와 32bit일 때를 구분해서 할당해야함. */
	if (cam1_alloc_size == 0)
		pctxt->frame_buffer_sm_addr[4] = pctxt->frame_buffer_sm_addr[3]
									+ (temp * (cam0_alloc_size/temp)) + temp;
	else
		pctxt->frame_buffer_sm_addr[4] = pctxt->frame_buffer_sm_addr[3]
									+ (temp * (cam1_alloc_size/temp)) + temp;

	pctxt->frame_buffer_sm_addr[5] = pctxt->frame_buffer_sm_addr[4]
									+ (temp * (fb_alloc_size/temp)) +temp;

	css_s3dc(" - MemAlloc[0] = 0x%08x(%d bytes)\n",
				pctxt->frame_buffer_sm_addr[0],
				pctxt->frame_buffer_sm_addr[1]
				- pctxt->frame_buffer_sm_addr[0] + 1);
	css_s3dc(" - MemAlloc[1] = 0x%08x(%d bytes)\n",
				pctxt->frame_buffer_sm_addr[1],
				pctxt->frame_buffer_sm_addr[2]
				- pctxt->frame_buffer_sm_addr[1] + 1);
	css_s3dc(" - MemAlloc[2] = 0x%08x(%d bytes)\n",
				pctxt->frame_buffer_sm_addr[2],
				pctxt->frame_buffer_sm_addr[3]
				- pctxt->frame_buffer_sm_addr[2] + 1);
	css_s3dc(" - MemAlloc[3] = 0x%08x(%d bytes)\n",
				pctxt->frame_buffer_sm_addr[3],
				pctxt->frame_buffer_sm_addr[4]
				- pctxt->frame_buffer_sm_addr[3] + 1);
	css_s3dc(" - MemAlloc[4] = 0x%08x(%d bytes)\n",
				pctxt->frame_buffer_sm_addr[4],
				pctxt->frame_buffer_sm_addr[5]
				- pctxt->frame_buffer_sm_addr[4] + 1);
	css_s3dc(" - MemAlloc[5] = 0x%08x(%d bytes)\n\n",
				pctxt->frame_buffer_sm_addr[5],
				pctxt->frame_buffer_sm_addr[5]
				- pctxt->frame_buffer_sm_addr[4] + 1);
}

#endif

void s3dc_change_framebuffer_address(int is_fullsize_mode)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();
	int temp = 32;
	/*
	 * 32의 배수로 주소를 지정하기 위해 필요한 변수
	 * (64bit 기준으로 0,1비트가 0이 되도록.)
	 */

	if (pctxt->page_flip_on) {
		if (is_fullsize_mode) {
			pctxt->frame_buffer_sm_addr[5] = pctxt->frame_buffer_sm_addr[4]
						+ (((temp * (pctxt->fb_data_size/temp)) + temp) * 2);

			s3dc_reg_data.blt_cmd_dst_sm_addr2_l.as32bits
						= (pctxt->frame_buffer_sm_addr[5] >> 1) & 0xffff;
			s3dc_reg_data.blt_cmd_dst_sm_addr2_h.as32bits
						= (pctxt->frame_buffer_sm_addr[5] >> 1) >> 16;

			s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR2_LOW,
							s3dc_reg_data.blt_cmd_dst_sm_addr2_l.as32bits);
			s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR2_HIGH,
							s3dc_reg_data.blt_cmd_dst_sm_addr2_h.as32bits);
		} else {
			pctxt->frame_buffer_sm_addr[5] = pctxt->frame_buffer_sm_addr[4]
						+ (((temp * (pctxt->fb_data_size/temp)) +temp) * 1);

			s3dc_reg_data.blt_cmd_dst_sm_addr2_l.as32bits
						= (pctxt->frame_buffer_sm_addr[5] >> 1) & 0xffff;
			s3dc_reg_data.blt_cmd_dst_sm_addr2_h.as32bits
						= (pctxt->frame_buffer_sm_addr[5] >> 1) >> 16;

			s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR2_LOW,
							s3dc_reg_data.blt_cmd_dst_sm_addr2_l.as32bits);
			s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR2_HIGH,
							s3dc_reg_data.blt_cmd_dst_sm_addr2_h.as32bits);
		}
	}
}

void s3dc_set_framebuffer_address(unsigned int addr)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	if (pctxt) {
		pctxt->frame_buffer_sm_addr[4] = addr;

		s3dc_reg_data.blt_cmd_dst_sm_addr_l.as32bits
				= (pctxt->frame_buffer_sm_addr[4] >> 1) & 0xffff; /*page 0*/
		s3dc_reg_data.blt_cmd_dst_sm_addr_h.as32bits
				= (pctxt->frame_buffer_sm_addr[4] >> 1) >> 16;
		s3dc_reg_data.blt_cmd_dst_sm_addr2_l.as32bits
				= (pctxt->frame_buffer_sm_addr[4] >> 1) & 0xffff; /*page 1*/
		s3dc_reg_data.blt_cmd_dst_sm_addr2_h.as32bits
				= (pctxt->frame_buffer_sm_addr[4] >> 1) >> 16;
		s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR_LOW,
						s3dc_reg_data.blt_cmd_dst_sm_addr_l.as32bits);
		s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR_HIGH,
						s3dc_reg_data.blt_cmd_dst_sm_addr_h.as32bits);
		s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR2_LOW,
						s3dc_reg_data.blt_cmd_dst_sm_addr2_l.as32bits);
		s3dc_set_reg32(s3dc_plat->iobase + BLT_CMD_DST_SM_ADDR2_HIGH,
						s3dc_reg_data.blt_cmd_dst_sm_addr2_h.as32bits);
	}
}

unsigned int s3dc_get_framebuffer_address(void)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();

	return pctxt->frame_buffer_sm_addr[4];
}

/*
 * FUNC : s3dc_get_context
 * DESC : getting context
 */
NXSCContext *s3dc_get_context(void)
{
	return s3dc_ctxt;
}

/*
 * FUNC : s3dc_context_make_current
 * DESC : make current context
 */
void s3dc_context_make_current(void *pctxt)
{
	s3dc_ctxt = (NXSCContext*)pctxt;
}

/*
 * FUNC : s3dcCreateContext
 * DESC : context create
 */
int s3dc_create_context(void)
{
	int ret = CSS_SUCCESS;

	NXSCContext *pctxt = 0;

	/* create vgcontext */
	pctxt = (NXSCContext*)s3dc_malloc(sizeof(NXSCContext));

	if (pctxt != NULL) {
		memset(pctxt, 0x0, sizeof(NXSCContext));
		s3dc_context_make_current(pctxt);
	} else {
		ret = -ENOMEM;
	}
	return ret;
}

/*
 * FUNC : s3dcDestroyContext
 * DESC : context destroy
 */
void s3dc_destroy_context(void)
{
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();

	if (!pctxt) return;

	s3dc_free(pctxt);

	s3dc_context_make_current(NULL);
}

unsigned int s3dc_convert_input_format(unsigned int format)
{
	unsigned int fmt;

	switch (format) {
	case IN_FMT_YUV422:
		fmt = YUV422_1PLANE_PACKED;
		break;
	case IN_FMT_RGB565:
	case IN_FMT_RGB888:
	default:
		fmt = BLT_INPUT_RGB888;
		break;
	}

	return fmt;
}

unsigned int s3dc_convert_output_format(unsigned int format)
{
	unsigned int fmt;

	switch (format) {
	case OUT_FMT_RGB565:
		fmt = BLT_OUT_RGB565;
		break;
	case OUT_FMT_RGB888:
		fmt = BLT_OUT_RGB888;
		break;
	case OUT_FMT_YUV422:
		fmt = BLT_OUT_YUV422_1PLANE_PACKED;
		break;
	case OUT_FMT_YUV420:
		fmt = BLT_OUT_Y_V_12;
		break;
	case OUT_FMT_NV12:
		fmt = BLT_OUT_N_V_12;
		break;
	case OUT_FMT_NV21:
		fmt = BLT_OUT_N_V_21;
		break;
	default:
		fmt = BLT_OUT_YUV422_1PLANE_PACKED;
		break;
	}

	return fmt;
}


unsigned int s3dc_convert_input_mode(unsigned int in_mode)
{
	unsigned int path;

	switch (in_mode) {
	case INPUT_MEM:
		path = CF_INPUT_DMA;
		break;
	case INPUT_SENSOR:
	default:
		path = CF_INPUT_DIRECT;
		break;
	}

	return path;
}

void s3dc_set_input_format(BLTInfo *info, unsigned int format,
			unsigned int order)
{
	switch (format) {
	case IN_FMT_YUV422:
		info->blti_format = BLT_INPUT_RGB8888;
		break;
	case IN_FMT_RGB565:
	case IN_FMT_RGB888:
	default:
		info->blti_format = BLT_INPUT_RGB888;
		break;
	}
}

void s3dc_set_output_format(BLTInfo *info, unsigned int format,
			unsigned int order)
{
	order = order;

	switch (format) {
	case OUT_FMT_RGB565:
		info->blto_format = BLT_OUT_RGB565;
		info->blto_order = order;
		break;
	case OUT_FMT_RGB888:
		info->blto_format = BLT_OUT_RGB888;
		info->blto_order = order;
		break;
	case OUT_FMT_YUV420:
		info->blto_format = BLT_OUT_Y_V_12;
		info->blto_order = order;
		break;
	case OUT_FMT_NV12:
		info->blto_format = BLT_OUT_N_V_12;
		info->blto_order = order;
		break;
	case OUT_FMT_NV21:
		info->blto_format = BLT_OUT_N_V_21;
		info->blto_order = order;
		break;
	case OUT_FMT_YUV422:
	default:
		info->blto_format = BLT_OUT_YUV422_1PLANE_PACKED;
		info->blto_order = order;
		break;
	}
}

void s3dc_output_s3d_mode_control(unsigned int mode)
{
	switch (mode) {
	case MODE_S3D_PIXEL:
		s3dc_set_3D_pixel_base_mode();
		break;
	case MODE_S3D_SIDE_BY_SIDE:
		s3dc_set_3D_side_by_side_mode();
		break;
	case MODE_S3D_LINE:
		s3dc_set_3D_line_base_mode();
		break;
	case MODE_S3D_RED_BLUE:
		s3dc_set_3D_red_blue_mode();
		break;
	case MODE_S3D_RED_GREEN:
		s3dc_set_3D_red_green_mode();
		break;
	case MODE_S3D_SUB_PIXEL:
		s3dc_set_3D_sub_pixel_mode();
		break;
	case MODE_S3D_TOP_BOTTOM:
		s3dc_set_3D_top_bottom_mode();
		break;
	case MODE_S3D_SIDE_BY_SIDE_FULL:
		s3dc_set_3D_side_by_side_mode_full();
		break;
	case MODE_S3D_TOP_BOTTOM_FULL:
		s3dc_set_3D_top_bottom_mode_full();
		break;
	default:
		break;
	}
}

void s3dc_output_ext_mode_control(int cam_id, unsigned int mode)
{
	switch (mode) {
	case MODE_2D:
		s3dc_select_camera_2D_mode(cam_id);
		s3dc_set_2D_mode();
		break;
	case MODE_S3D_MANUAL:
		s3dc_set_3D_manual_mode();
		break;
	default:
		break;
	}
}

int s3dc_manual_convergence_move_left(int cam_id)
{
	int ret = CSS_SUCCESS;
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	int cam0_pixel_start = 0;
	int cam1_pixel_start = 0;
	int p_start_offset[2] = {0};

	if (!pctxt) {
		return CSS_ERROR_S3DC_INVALID_CTXT;
	}

	if (pctxt->config_done != 0x3) {
		return CSS_ERROR_S3DC_INVALID_CONFIG;
	}

	cam0_pixel_start = (pctxt->cam0_width - pctxt->cam0_crop_width) / 2;
	cam1_pixel_start = (pctxt->cam1_width - pctxt->cam1_crop_width) / 2;

	if (pctxt->curr_mode == MODE_S3D_MANUAL) {
		if (cam_id == CAMERA_0) {
			cam0_pixel_start--;
			if (cam0_pixel_start < 0) {
				cam0_pixel_start = 0;
			}
			s3dc_set_pixel_start(cam_id, cam0_pixel_start);
		} else {
			cam1_pixel_start--;
			if (cam1_pixel_start < 0) {
				cam1_pixel_start = 0;
			}
			s3dc_set_pixel_start(cam_id, cam1_pixel_start);
		}
	} else {
		p_start_offset[cam_id]--;
		s3dc_set_pixel_start_offset(cam_id, -1);
	}

	return ret;
}

int s3dc_manual_convergence_move_right(int cam_id)
{
	int ret = CSS_SUCCESS;
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	int cam0_pixel_start = 0;
	int cam1_pixel_start = 0;
	int p_start_offset[2] = {0};

	if (!pctxt) {
		return CSS_ERROR_S3DC_INVALID_CTXT;
	}

	if (pctxt->config_done != 0x3) {
		return CSS_ERROR_S3DC_INVALID_CONFIG;
	}

	cam0_pixel_start = (pctxt->cam0_width - pctxt->cam0_crop_width) / 2;
	cam1_pixel_start = (pctxt->cam1_width - pctxt->cam1_crop_width) / 2;

	if (pctxt->curr_mode == MODE_S3D_MANUAL) {
		if (cam_id == CAMERA_0) {
			cam0_pixel_start++;
			if ((cam0_pixel_start + pctxt->cam0_crop_width)
				> pctxt->cam0_width) {
				cam0_pixel_start = pctxt->cam0_width - pctxt->cam0_crop_width;
			}
			s3dc_set_pixel_start(cam_id, cam0_pixel_start);
		} else {
			cam1_pixel_start++;
			if ((cam1_pixel_start + pctxt->cam1_crop_width)
				> pctxt->cam1_width) {
				cam1_pixel_start = pctxt->cam1_width - pctxt->cam1_crop_width;
			}
			s3dc_set_pixel_start(cam_id, cam1_pixel_start);
		}
	} else {
		p_start_offset[cam_id]++;
		s3dc_set_pixel_start_offset(cam_id, 1);
	}

	return ret;
}

int s3dc_vertical_rectification_move_up(int cam_id)
{
	int ret = CSS_SUCCESS;
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	int cam0_line_start = 0;
	int cam1_line_start = 0;

	if (!pctxt) {
		return CSS_ERROR_S3DC_INVALID_CTXT;
	}

	if (pctxt->config_done != 0x3) {
		return CSS_ERROR_S3DC_INVALID_CONFIG;
	}

	cam0_line_start = (pctxt->cam0_height - pctxt->cam0_crop_height) / 2;
	cam1_line_start = (pctxt->cam1_height - pctxt->cam1_crop_height) / 2;

	if (cam_id == CAMERA_0) {
		cam0_line_start--;
		if (cam0_line_start < 0) {
			cam0_line_start = 0;
		}
		s3dc_set_line_start(cam_id, cam0_line_start);
	} else {
		cam1_line_start--;
		if (cam1_line_start < 0) {
			cam1_line_start = 0;
		}
		s3dc_set_line_start(cam_id, cam1_line_start);
	}

	return ret;
}

int s3dc_vertical_rectification_move_down(int cam_id)
{
	int ret = CSS_SUCCESS;
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	int cam0_line_start = 0;
	int cam1_line_start = 0;

	if (!pctxt) {
		return CSS_ERROR_S3DC_INVALID_CTXT;
	}

	if (pctxt->config_done != 0x3) {
		return CSS_ERROR_S3DC_INVALID_CONFIG;
	}

	cam0_line_start = (pctxt->cam0_height - pctxt->cam0_crop_height) / 2;
	cam1_line_start = (pctxt->cam1_height - pctxt->cam1_crop_height) / 2;

	if (cam_id == CAMERA_0) {
		cam0_line_start++;
		if ((cam0_line_start + pctxt->cam0_crop_height) > pctxt->cam0_height) {
			cam0_line_start = pctxt->cam0_height - pctxt->cam0_crop_height;
		}
		s3dc_set_line_start(cam_id, cam0_line_start);
	} else {
		cam1_line_start++;
		if ((cam1_line_start + pctxt->cam1_crop_height) > pctxt->cam1_height) {
			cam1_line_start = pctxt->cam1_height - pctxt->cam1_crop_height;
		}
		s3dc_set_line_start(cam_id, cam1_line_start);
	}

	return ret;
}

int s3dc_auto_convergence(int enable, int dynamic)
{
	int ret = CSS_SUCCESS;

	if (dynamic) {
		s3dc_enable_ac_auto_start_quick(enable);
	} else {
		s3dc_enable_auto_convergence(enable);
	}
	return ret;
}

/*
 * FUNC : s3dcConfigContext
 * DESC : context create
 */
int s3dc_config_context(struct css_s3dc_config *config, unsigned int mem_base)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	CFInputFormatInfo cf_input_format;
	CFInputDMASizeInfo dma_size_info;
	CFSizeInfo cf_size_info;
	BLTInfo blt_info;

	/* Config CF */
	cf_size_info.cam0_width = config->input0.rect.start_x
							+ config->input0.rect.width;
	cf_size_info.cam0_height= config->input0.rect.start_y
							+ config->input0.rect.height;
	cf_size_info.cam0_crop_w = config->output.rect.width;
	cf_size_info.cam0_crop_h = config->output.rect.height;
	cf_size_info.cam1_width = config->input1.rect.start_x
							+ config->input1.rect.width;
	cf_size_info.cam1_height= config->input1.rect.start_y
							+ config->input1.rect.height;
	cf_size_info.cam1_crop_w = config->output.rect.width;
	cf_size_info.cam1_crop_h = config->output.rect.height;
	cf_size_info.cf_out_w = config->output.rect.width;
	cf_size_info.cf_out_h = config->output.rect.height;

	s3dc_set_cf_context(cf_size_info);

	s3dc_set_tilt_value(pctxt->compensation_tilt_bypass,
						pctxt->compensation_tilt_tangent);

	/* Config BLT */
	s3dc_set_input_format(&blt_info, config->input0.fmt, config->input0.align);
	s3dc_set_output_format(&blt_info,config->output.fmt, config->output.align);
	s3dc_set_blt_context(blt_info);

	s3dc_reg_data.cf_sync_ctrl.asbits.sync_en = AUTO_SYNC;
	s3dc_reg_data.cf_sync_ctrl.asbits.fix_sync = FIX_SYNC;
	s3dc_reg_data.cf_sync_ctrl.asbits.blt_fast = BLT_FAST;
	s3dc_reg_data.cf_sync_ctrl.asbits.blt_slow = BLT_SLOW;

	s3dc_set_reg32(s3dc_plat->iobase + BLT_SLOW_FAST_FIX_AUTO_SYNC,
					s3dc_reg_data.cf_sync_ctrl.as32bits);

#if 0	/* v1.1 */
	s3dc_alloc_stack_memory(cf_input_format, dma_size_info);
#else	/* v1.0 */
	s3dc_alloc_stack_memory(mem_base);
#endif
	/* Config CF Input Info */
	cf_input_format.c0_input_mode = s3dc_convert_input_mode(config->in_path0);
	cf_input_format.c1_input_mode = s3dc_convert_input_mode(config->in_path1);

	cf_input_format.c0_format = s3dc_convert_input_format(config->input0.fmt);
	cf_input_format.c1_format = s3dc_convert_input_format(config->input1.fmt);
	cf_input_format.c0_order = config->input0.align;
	cf_input_format.c1_order = config->input1.align;

	dma_size_info.c0_crop_x = config->input0.rect.start_x;
	dma_size_info.c0_crop_y = config->input0.rect.start_y;
	dma_size_info.c0_crop_w = config->input0.rect.width;
	dma_size_info.c0_crop_h = config->input0.rect.height;
	dma_size_info.c0_total_w = config->input0.rect.start_x
							+ config->input0.rect.width;
	dma_size_info.c0_total_h = config->input0.rect.start_y
							+ config->input0.rect.height;

	dma_size_info.c1_crop_x = config->input1.rect.start_x;
	dma_size_info.c1_crop_y = config->input1.rect.start_y;
	dma_size_info.c1_crop_w = config->input1.rect.width;
	dma_size_info.c1_crop_h = config->input1.rect.height;
	dma_size_info.c1_total_w = config->input1.rect.start_x
							+ config->input1.rect.width;
	dma_size_info.c1_total_h = config->input1.rect.start_y
							+ config->input1.rect.height;

	if (config->in_path0 == INPUT_MEM) {
		s3dc_set_cf_input_dma0_addr(config->inaddr0, 0);
		s3dc_set_dma_start_mode(DMA_START_HOST_CMD, -1);
	}

	if (config->in_path1 == INPUT_MEM) {
		s3dc_set_cf_input_dma1_addr(config->inaddr1, 0);
		s3dc_set_dma_start_mode(-1, DMA_START_HOST_CMD);
	}

	s3dc_disable_shadow_reg_up();
	s3dc_set_cf_input_info(cf_input_format, dma_size_info);

	s3dc_init_blt();
	s3dc_init_creator_formatter();
	s3dc_enable_shadow_reg_up();

	s3dc_output_s3d_mode_control(config->outmode);

	if (config->in_path0 == INPUT_MEM || config->in_path1 == INPUT_MEM)
		s3dc_start_dma_read();

	return 0;
}

int s3dc_ioctl_get_version(int *dev_ver, int *hw_ver)
{
	int ret = CSS_SUCCESS;
	int ver_l, ver_h;
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	ver_l = s3dc_get_reg32(s3dc_plat->iobase + 0x1028);
	ver_h = s3dc_get_reg32(s3dc_plat->iobase + 0x102C);

	*dev_ver = NXSC_DRV_VER;
	*hw_ver = (ver_h << 16) | ver_l;

	return ret;
}

int s3dc_blt_buffer_size(struct css_s3dc_buf_set *buf)
{
	int ret = CSS_SUCCESS;
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct css_s3dc_config *config;
	int colorSize1 = 0;
	int cam0_alloc_size, cam1_alloc_size, fb_alloc_size;
	int temp = 32;
	int cam0_crop_width, cam0_crop_height;
	int cam1_crop_width, cam1_crop_height;
	int num_of_pixel;

	if (!pctxt)
		return CSS_ERROR_S3DC_INVALID_CTXT;

	if (pctxt->config_done == 0)
		return CSS_ERROR_S3DC_INVALID_CONFIG;

	config = &pctxt->config;

	colorSize1 = 4;

	cam0_crop_width = config->input0.rect.width;
	cam0_crop_height = config->input0.rect.height;
	cam1_crop_width = config->input1.rect.width;
	cam1_crop_height = config->input1.rect.height;
	num_of_pixel = config->output.rect.width * config->output.rect.height;

	switch (config->output.fmt) {
	case BLT_OUT_RGB565:
		fb_alloc_size = num_of_pixel * 2;
		break;
	case BLT_OUT_RGB888:
		fb_alloc_size = num_of_pixel * 3;
		break;
	case BLT_OUT_RGB8888:
		fb_alloc_size = num_of_pixel * 4;
		break;
	case BLT_OUT_YUV444_1PLANE_PACKED:
		fb_alloc_size = num_of_pixel * 3;
		break;
	case BLT_OUT_YUV444_1PLANE_UNPACKED:
		fb_alloc_size = num_of_pixel * 4;
		break;
	case BLT_OUT_YUV422_1PLANE_PACKED:
		fb_alloc_size = num_of_pixel * 2;
		break;
	case BLT_OUT_YUV422_PLANAR:
		fb_alloc_size = num_of_pixel * 2;
		break;
	case BLT_OUT_N_V_12:
		fb_alloc_size = num_of_pixel * 3 / 2;
		break;
	case BLT_OUT_N_V_21:
		fb_alloc_size = num_of_pixel * 3 / 2;
		break;
	case BLT_OUT_Y_V_12:
		fb_alloc_size = num_of_pixel * 3 / 2;
		break;
	case BLT_OUT_Y_V_21:
		fb_alloc_size = num_of_pixel * 3 / 2;
		break;
	case BLT_OUT_IMC_1:
		fb_alloc_size = num_of_pixel * 2;
		break;
	case BLT_OUT_IMC_2:
		fb_alloc_size = num_of_pixel * 3 / 2;
		break;
	case BLT_OUT_IMC_3:
		fb_alloc_size = num_of_pixel * 2;
		break;
	case BLT_OUT_IMC_4:
		fb_alloc_size = num_of_pixel * 3 / 2;
		break;
	default:
		fb_alloc_size = 0;
		break;

	}

	/* deci mode가 1인 경우엔 메모리 할당을 반으로 줄임 */
	if (pctxt->deci_mode & 0x01) {
		cam0_alloc_size
			= (int)(cam0_crop_width * cam0_crop_height * colorSize1 * 14 / 20);
		cam1_alloc_size
			= (int)(cam1_crop_width * cam1_crop_height * colorSize1 / 2);
	} else {
		cam0_alloc_size
			= (int)(cam0_crop_width * cam0_crop_height * colorSize1 * 14 / 10);
		cam1_alloc_size
			= (int)(cam1_crop_width * cam1_crop_height * colorSize1);
	}

	if ((cam0_alloc_size == 0) || (cam1_alloc_size == 0)) {
		buf[0].buf_size = pctxt->mem_size[0] = 0;
	} else {
		buf[0].buf_size = pctxt->mem_size[0] = (temp * (cam0_alloc_size / temp))
												+ temp;
	}

	if (cam0_alloc_size == 0) {
		buf[1].buf_size = pctxt->mem_size[1] = (temp * (cam1_alloc_size / temp))
												+ temp;
	} else {
		buf[1].buf_size = pctxt->mem_size[1] = (temp * (cam0_alloc_size / temp))
												+ temp;
	}

	if ((cam0_alloc_size == 0) || (cam1_alloc_size == 0)) {
		buf[2].buf_size = pctxt->mem_size[2] = 0;
	} else {
		buf[2].buf_size = pctxt->mem_size[2] = (temp * (cam1_alloc_size / temp))
												+ temp;
	}

	if (cam1_alloc_size == 0) {
		buf[3].buf_size = pctxt->mem_size[3] = (temp * (cam0_alloc_size / temp))
												+ temp;
	} else {
	   buf[3].buf_size = pctxt->mem_size[3] = (temp * (cam1_alloc_size / temp))
	   											+ temp;
	}

	return ret;
}

int s3dc_blt_buffer_alloc(void)
{
	struct s3dc_platform	*s3dc_plat = s3dc_get_platform();
	struct css_s3dc_buf_set *buf = NULL;
	int ret = CSS_SUCCESS;
	int i;

	buf = s3dc_plat->blt_buf;

	memset(buf, 0, sizeof(struct css_s3dc_buf_set) * 4);

	ret = s3dc_blt_buffer_size(buf);

	if (ret != CSS_SUCCESS)
		return ret;

	if (s3dc_plat->rsvd_mem) {
		for (i = 0; i < 4; i++) {
			struct css_buffer cssbuf;

			clear(cssbuf);

			ret = odin_css_physical_buffer_alloc(s3dc_plat->rsvd_mem,
							&cssbuf, buf[i].buf_size);
			if (ret == CSS_SUCCESS) {
				buf[i].index	= i;
				buf[i].dma_addr = (unsigned int)cssbuf.dma_addr;
				buf[i].cpu_addr = cssbuf.cpu_addr;
			} else {
				buf[i].dma_addr = 0;
				buf[i].cpu_addr = NULL;
			}
		}
	} else {
#ifdef CONFIG_ODIN_ION_SMMU
		unsigned long iova = 0;
		unsigned long aligned_size = 0;

		s3dc_plat->ion_client = odin_ion_client_create("s3dc_ion");
		if (s3dc_plat->ion_client) {
			for (i = 0; i < 4; i++) {
				aligned_size = ion_align_size(buf[i].buf_size);
				buf[i].ion_hdl.handle = ion_alloc(s3dc_plat->ion_client,
											aligned_size,
											0x1000,
											(1 << ODIN_ION_SYSTEM_HEAP1),
											0,
											ODIN_SUBSYS_CSS);

				if (IS_ERR_OR_NULL(buf[i].ion_hdl.handle)) {
					for (i = 0; i < 4; i++) {
						if (buf[i].ion_hdl.handle)
							buf[i].ion_hdl.handle = NULL;
					}
					odin_ion_client_destroy(s3dc_plat->ion_client);
					s3dc_plat->ion_client = NULL;
					css_err("s3dc ion get handle fail\n");
					return CSS_FAILURE;
				}
				iova = odin_ion_get_iova_of_buffer(buf[i].ion_hdl.handle,
												ODIN_SUBSYS_CSS);
				if (!iova) {
					for (i = 0; i < 4; i++) {
						if (buf[i].ion_hdl.handle)
							buf[i].ion_hdl.handle = NULL;
					}
					odin_ion_client_destroy(s3dc_plat->ion_client);
					s3dc_plat->ion_client = NULL;
					css_err("s3dc ion get iova fail\n");
					return CSS_FAILURE;
				}

				buf[i].index	= i;
				buf[i].dma_addr	= iova;
				buf[i].cpu_addr	= NULL;
			}

		} else {
			css_err("zsl ion client create fail\n");
			return CSS_FAILURE;
		}
#else
		css_err("not supported ion\n");
		return CSS_FAILURE;
#endif
	}

	for (i = 0; i < 4; i++) {
		if (buf[i].dma_addr) {
			NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();

			if (!pctxt->frame_buffer_sm_addr[i]) {
				pctxt->frame_buffer_sm_addr[i] = buf[i].dma_addr;
				css_s3dc("[S3DC]frame_buffer_sm_addr[%d]=%x(%dKB)\n",
						buf[i].index, buf[i].dma_addr, buf[i].buf_size / 1024);
			} else {
				return CSS_ERROR_S3DC_BUF_NEED_TO_FREE;
			}
		} else {
			ret = CSS_ERROR_S3DC_BUF_ALLOC_FAIL;
			break;
		}
	}

	return ret;
}

int s3dc_blt_buffer_free(void)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();
	struct css_s3dc_buf_set *buf = NULL;
	int ret = CSS_SUCCESS;
	int i;

	buf = s3dc_plat->blt_buf;

	if (s3dc_plat->rsvd_mem) {
		for (i = 0; i < 4; i++) {
			struct css_buffer cssbuf;
			clear(cssbuf);

			cssbuf.allocated = 1;
			cssbuf.dma_addr = buf[i].dma_addr;
			cssbuf.cpu_addr = buf[i].cpu_addr;
			cssbuf.size	= buf[i].buf_size;

			ret = odin_css_physical_buffer_free(s3dc_plat->rsvd_mem, &cssbuf);

			if (ret == CSS_SUCCESS) {
				buf[i].dma_addr = 0;
				buf[i].cpu_addr = NULL;
				buf[i].buf_size = 0;
			} else {
				ret = CSS_ERROR_S3DC_BUF_FREE_FAIL;
			}
		}
	} else {
#ifdef CONFIG_ODIN_ION_SMMU
		for (i = 0; i < 4; i++) {
			if (buf[i].ion_hdl.handle) {
				buf[i].dma_addr = 0;
				buf[i].cpu_addr = NULL;
				buf[i].buf_size = 0;
				buf[i].ion_hdl.handle = NULL;
			}
		}
		if (s3dc_plat->ion_client) {
			odin_ion_client_destroy(s3dc_plat->ion_client);
			s3dc_plat->ion_client = NULL;
		}
#else
		css_err("not supported ion\n");
		return CSS_FAILURE;
#endif
	}

	if (ret == CSS_SUCCESS) {
		NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
		for (i = 0; i < 4; i++) {
			pctxt->frame_buffer_sm_addr[buf[i].index] = 0;
		}
	}

	return ret;
}

int s3dc_ioctl_start(void)
{
	int ret = CSS_SUCCESS;
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();

	CFInputFormatInfo cf_input_format;
	CFInputDMASizeInfo dma_size_info;
	CFSizeInfo cf_size_info;
	BLTInfo blt_info;
	struct css_s3dc_config *config = NULL;

	if (!pctxt)
		return CSS_ERROR_S3DC_INVALID_CTXT;

	if (pctxt->config_done != 0x7)
		return CSS_ERROR_S3DC_INVALID_CONFIG;

	config = &pctxt->config;

	ret = s3dc_blt_buffer_alloc();
	if (ret)
		return ret;

	/* Config CF */
	cf_size_info.cam0_width = config->input0.rect.start_x
							+ config->input0.rect.width;
	cf_size_info.cam0_height= config->input0.rect.start_y
							+ config->input0.rect.height;
	cf_size_info.cam0_crop_w = config->output.rect.width;
	cf_size_info.cam0_crop_h = config->output.rect.height;

	cf_size_info.cam1_width = config->input1.rect.start_x
							+ config->input1.rect.width;
	cf_size_info.cam1_height= config->input1.rect.start_y
							+ config->input1.rect.height;
	cf_size_info.cam1_crop_w = config->output.rect.width;
	cf_size_info.cam1_crop_h = config->output.rect.height;

	cf_size_info.cf_out_w = config->output.rect.width;
	cf_size_info.cf_out_h = config->output.rect.height;

	s3dc_set_cf_context(cf_size_info);

	s3dc_set_tilt_value(pctxt->compensation_tilt_bypass,
						pctxt->compensation_tilt_tangent);

	s3dc_reg_data.cf_sync_ctrl.asbits.sync_en = AUTO_SYNC;
	s3dc_reg_data.cf_sync_ctrl.asbits.fix_sync = FIX_SYNC;
	s3dc_reg_data.cf_sync_ctrl.asbits.blt_fast = BLT_FAST;
	s3dc_reg_data.cf_sync_ctrl.asbits.blt_slow = BLT_SLOW;

	s3dc_set_reg32(s3dc_plat->iobase + BLT_SLOW_FAST_FIX_AUTO_SYNC,
					s3dc_reg_data.cf_sync_ctrl.as32bits);

	/* Config BLT */
	s3dc_set_input_format(&blt_info, config->input0.fmt, config->input0.align);
	s3dc_set_output_format(&blt_info,config->output.fmt, config->output.align);
	s3dc_set_blt_context(blt_info);

	/* Config CF Input Info */
	cf_input_format.c0_input_mode = s3dc_convert_input_mode(config->in_path0);
	cf_input_format.c1_input_mode = s3dc_convert_input_mode(config->in_path1);

	cf_input_format.c0_format = s3dc_convert_input_format(config->input0.fmt);
	cf_input_format.c1_format = s3dc_convert_input_format(config->input1.fmt);
	cf_input_format.c0_order = config->input0.align; /*ORDER_YVYU;*/
	cf_input_format.c1_order = config->input1.align; /*ORDER_YVYU; */

	dma_size_info.c0_crop_x = config->input0.rect.start_x;
	dma_size_info.c0_crop_y = config->input0.rect.start_y;
	dma_size_info.c0_crop_w = config->input0.rect.width;
	dma_size_info.c0_crop_h = config->input0.rect.height;
	dma_size_info.c0_total_w = config->input0.rect.start_x
							+ config->input0.rect.width;
	dma_size_info.c0_total_h = config->input0.rect.start_y
							+ config->input0.rect.height;

	dma_size_info.c1_crop_x = config->input1.rect.start_x;
	dma_size_info.c1_crop_y = config->input1.rect.start_y;
	dma_size_info.c1_crop_w = config->input1.rect.width;
	dma_size_info.c1_crop_h = config->input1.rect.height;
	dma_size_info.c1_total_w = config->input1.rect.start_x
							+ config->input1.rect.width;
	dma_size_info.c1_total_h = config->input1.rect.start_y
							+ config->input1.rect.height;

	if (config->in_path0 == INPUT_MEM) {
		s3dc_set_cf_input_dma0_addr(config->inaddr0, 0);
		s3dc_set_dma_start_mode(DMA_START_HOST_CMD, -1);
	}

	if (config->in_path1 == INPUT_MEM) {
		s3dc_set_cf_input_dma1_addr(config->inaddr1, 0);
		s3dc_set_dma_start_mode(-1, DMA_START_HOST_CMD);
	}

	if (config->in_path0 == INPUT_SENSOR || config->in_path1 == INPUT_SENSOR) {
		scaler_path_reset(PATH_3D_RESET);
		scaler_path_clk_control(PATH_3D_CLK, CLOCK_ON);
	}

	s3dc_disable_shadow_reg_up();
	s3dc_set_cf_input_info(cf_input_format, dma_size_info);

	s3dc_init_blt();
	s3dc_init_creator_formatter();
	s3dc_enable_shadow_reg_up();

	s3dc_output_s3d_mode_control(config->outmode);

	if (config->in_path0 == INPUT_MEM ||
			config->in_path1 == INPUT_MEM) {
		s3dc_start_dma_read();
	}

	s3dc_int_enable(INT_S3DC_FRAME_END);

	s3dc_set_frame_count(0);

	pctxt->s3dc_enable = 1;

	return ret;
}

int s3dc_ioctl_stop(void)
{
	int ret = CSS_SUCCESS;
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct css_s3dc_config *config = NULL;

	if (!pctxt)
		return CSS_ERROR_S3DC_INVALID_CTXT;

	config = &pctxt->config;

	s3dc_int_disable(INT_S3DC_FRAME_END);

	s3dc_stop_trans();

	if (pctxt) {
		pctxt->cf_enable = 0;
		s3dc_disable_shadow_reg_up();

		pctxt->config_done = 0;
		pctxt->s3dc_enable = 0;
	}

	if (config->in_path0 == INPUT_SENSOR || config->in_path1 == INPUT_SENSOR) {
		scaler_path_clk_control(PATH_3D_CLK, CLOCK_OFF);
	}

	s3dc_set_frame_count(0);

	ret = s3dc_blt_buffer_free();

	return ret;
}

int s3dc_ioctl_s3d_outmode(s3dc_output_mode outmode)
{
	int ret = CSS_SUCCESS;

	s3dc_output_s3d_mode_control(outmode);

	return ret;
}

int s3dc_ioctl_ext_outmode(int cam_id, s3dc_output_ext_mode outmode)
{
	int ret = CSS_SUCCESS;

	s3dc_output_ext_mode_control(cam_id, outmode);

	return ret;
}


int s3dc_ioctl_manual_convergence(int cam_id, s3dc_direction direction)
{
	int ret = CSS_SUCCESS;

	switch (direction) {
	case LEFT: /*left*/
		ret = s3dc_manual_convergence_move_left(cam_id);
		break;
	case RIGHT: /*right*/
		ret = s3dc_manual_convergence_move_right(cam_id);
		break;
	case UP:
		ret = s3dc_vertical_rectification_move_up(cam_id);
		break;
	case DOWN:
		ret = s3dc_vertical_rectification_move_down(cam_id);
		break;
	default:
		ret = CSS_ERROR_S3DC_INVALID_ARG;
		break;
	}

	return ret;
}

int s3dc_ioctl_auto_convergence(int enable, int dynamic)
{
	int ret = CSS_SUCCESS;

	s3dc_auto_convergence(dynamic, enable);

	return ret;
}

int s3dc_ioctl_auto_convergence_set_area(int sx, int sy, int width, int height)
{
	int ret = CSS_SUCCESS;

	s3dc_set_auto_conv_area(sx, sy, width, height);

	return ret;
}

int s3dc_ioctl_auto_convergence_set_edge_threshold(int value)
{
	int ret = CSS_SUCCESS;

	if (value >= 1 && value <= 254) {
		s3dc_set_tolerance_val(value);
	} else {
		ret = CSS_ERROR_S3DC_INVALID_ARG;
	}

	return ret;
}

int s3dc_ioctl_auto_convergence_set_disparity_offset(s3dc_sign sign, int offset)
{
	int ret = CSS_SUCCESS;
	int dp_redun = 0;

	if (offset < 0 || offset > 32) {
		ret = CSS_ERROR_S3DC_INVALID_ARG;
	} else {
		dp_redun = (sign << 5) | (offset & 0x1F);
		s3dc_set_acdp_redun(dp_redun);
	}
	return ret;
}

int s3dc_ioctl_convergence_set_disparity_step(int dpstep, int step_value)
{
	int ret = CSS_SUCCESS;

	if (dpstep <0 && dpstep > 1) {
		ret = CSS_ERROR_S3DC_INVALID_ARG;
	} else {
		int dpstep_val;

		dpstep_val = (dpstep << 7 | (step_value & 0x7F));
		s3dc_change_dp_step(dpstep_val);
	}
	return ret;
}

int s3dc_ioctl_rotate_set_degree(BrRotationInfo *rot_info)
{
	int ret = CSS_SUCCESS;

	s3dc_set_rotate_value(rot_info);

	return ret;
}

int s3dc_ioctl_tilt_set_value(unsigned int bypass_en,
							unsigned int tangent_value)
{
	int ret = CSS_SUCCESS;
	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();

	if (pctxt) {
		pctxt->compensation_tilt_bypass = bypass_en;
		pctxt->compensation_tilt_tangent = tangent_value;
	}
	return ret;
}


int s3dc_ioctl_auto_luminance_control(int enable)
{
	int ret = CSS_SUCCESS;

	s3dc_enable_auto_luminance_balance(enable);

	return ret;
}

int s3dc_ioctl_luminance_control(struct s3dc_luminance_set *l_set)
{
	int ret = CSS_SUCCESS;
	int cam = l_set->camera;

	/* color control enable */
	s3dc_enable_color_control(cam, 1);

	/* set YGAIN */
	s3dc_set_color_gains(COLOR_Y_GAIN, cam, l_set->y_gain_0, l_set->y_gain_1);
	/* set Low Bright threshold */
	s3dc_set_color_bright(L_BRIGHT_THRESHOLD, cam, l_set->l_bright_th);
	/* set High Bright threshold */
	s3dc_set_color_bright(H_BRIGHT_THRESHOLD, cam, l_set->h_bright_th);
	/* set Low Y-gain */
	s3dc_set_color_gains(COLOR_LY_GAIN,
							cam, l_set->ly_gain_0, l_set->ly_gain_1);
	/* set High Y-gain */
	s3dc_set_color_gains(COLOR_HY_GAIN,
						cam, l_set->hy_gain_0, l_set->hy_gain_1);

	return ret;
}

int s3dc_ioctl_saturation_control(struct s3dc_saturation_set *s_set)
{
	int ret = CSS_SUCCESS;
	int cam = s_set->camera;

	s3dc_enable_color_control(cam, 1);
	s3dc_set_color_gains(COLOR_C_GAIN, cam, s_set->c_gain_0, s_set->c_gain_1);
	s3dc_set_color_bright(L_COLOR_THRESHOLD, cam, s_set->l_color_th);
	s3dc_set_color_bright(H_COLOR_THRESHOLD, cam, s_set->h_color_th);
	s3dc_set_color_gains(COLOR_LC_GAIN,
						cam, s_set->lc_gain_0, s_set->lc_gain_1);
	s3dc_set_color_gains(COLOR_HC_GAIN,
						cam, s_set->hc_gain_0, s_set->hc_gain_1);

	return ret;
}

int s3dc_ioctl_get_crop_size_info(struct s3dc_crop_size_set *crop_set)
{
	int ret = CSS_SUCCESS;
	int cam = crop_set->camera;

	crop_set->pixel_start	= s3dc_get_pixel_start(cam);
	crop_set->line_start	= s3dc_get_line_start(cam);
	crop_set->pixel_count	= s3dc_get_pixel_count(cam);
	crop_set->line_count	= s3dc_get_line_count(cam);
	crop_set->camera_width	= s3dc_get_camera_width(cam);
	crop_set->camera_height = s3dc_get_camera_height(cam);

	return ret;
}

int s3dc_ioctl_set_color_coeff(struct s3dc_coeff_set *color_coeff)
{
	int ret = CSS_SUCCESS;

	return ret;
}

int s3dc_ioctl_brightness_control(int cam_id, unsigned int value)
{
	int ret = CSS_SUCCESS;

	s3dc_enable_color_control(cam_id, 1);

	s3dc_set_color_bright(COLOR_BRIGHT, cam_id, value);

	return ret;
}

int s3dc_ioctl_contrast_control(int cam_id, unsigned int gain_0,
						unsigned int gain_1)
{
	int ret = CSS_SUCCESS;

	s3dc_enable_color_control(cam_id, 1);

	s3dc_set_color_gains(COLOR_O_GAIN, cam_id, gain_0, gain_1);

	return ret;
}

int s3dc_ioctl_hue_control(int cam_id, unsigned int value)
{
	int ret = CSS_SUCCESS;

	s3dc_enable_color_control(cam_id, 1);

	s3dc_set_color_bright(COLOR_HUE, cam_id, value);

	return ret;
}

int s3dc_ioctl_color_pattern_control(int cam_id, unsigned int enable)
{
	int ret = CSS_SUCCESS;

	s3dc_enable_color_control(cam_id, 1);

	s3dc_enable_color_pattern(enable);

	return ret;
}

int s3dc_ioctl_zoom(unsigned int zoom_index)
{
	int ret = CSS_SUCCESS;

	s3dc_set_zoom(zoom_index);

	return ret;
}

int s3dc_ioctl_change_lr(unsigned int change_lr)
{
	int ret = CSS_SUCCESS;

	s3dc_change_lr(change_lr);

	return ret;
}

int s3dc_ioctl_init_input_buffer(struct s3dc_buf_param *buf_param)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();
	struct css_buffer cssbuf;

	int ret = CSS_SUCCESS;

	clear(cssbuf);

	if (!buf_param)
		return -EINVAL;

	if (s3dc_plat->rsvd_mem) {
		ret = odin_css_physical_buffer_alloc(s3dc_plat->rsvd_mem,
						&cssbuf, buf_param->buf_size);
		if (ret == CSS_SUCCESS) {
			buf_param->dma_addr = (unsigned int)cssbuf.dma_addr;
			buf_param->cpu_addr = cssbuf.cpu_addr;
		} else {
			buf_param->dma_addr = 0;
			buf_param->cpu_addr = NULL;
		}
	}

	return ret;
}

int s3dc_ioctl_deinit_input_buffer(struct s3dc_buf_param *buf_param)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();
	int ret = CSS_SUCCESS;

	if (!buf_param)
		return -EINVAL;

	if (s3dc_plat->rsvd_mem) {
		struct css_buffer cssbuf;

		clear(cssbuf);
		cssbuf.allocated = 1;
		cssbuf.dma_addr = buf_param->dma_addr;
		cssbuf.cpu_addr = buf_param->cpu_addr;

		ret = odin_css_physical_buffer_free(s3dc_plat->rsvd_mem, &cssbuf);
		if (ret == CSS_SUCCESS) {
			buf_param->dma_addr = 0;
		} else {
			ret = CSS_ERROR_S3DC_BUF_FREE_FAIL;
		}
	}
	return ret;
}

int s3dc_ioctl_init_out_buffer(struct s3dc_buf_param *buf_param)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();
	int ret = CSS_SUCCESS;

	if (!buf_param)
		return -EINVAL;

	if (s3dc_plat->rsvd_mem) {
		struct css_buffer cssbuf;

		clear(cssbuf);
		ret = odin_css_physical_buffer_alloc(s3dc_plat->rsvd_mem,
						&cssbuf, buf_param->buf_size);
		if (ret == CSS_SUCCESS) {
			buf_param->dma_addr = (unsigned int)cssbuf.dma_addr;
			buf_param->cpu_addr = cssbuf.cpu_addr;
		} else {
			buf_param->dma_addr = 0;
			buf_param->cpu_addr = NULL;
		}
	}

	return ret;
}

int s3dc_ioctl_deinit_out_buffer(struct s3dc_buf_param *buf_param)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();
	int ret = CSS_SUCCESS;

	if (!buf_param)
		return -EINVAL;

	if (s3dc_plat->rsvd_mem) {
		struct css_buffer cssbuf;

		clear(cssbuf);
		cssbuf.allocated = 1;
		cssbuf.dma_addr = buf_param->dma_addr;
		cssbuf.cpu_addr = buf_param->cpu_addr;

		ret = odin_css_physical_buffer_free(s3dc_plat->rsvd_mem, &cssbuf);
		if (ret == CSS_SUCCESS)
			buf_param->dma_addr = 0;
		else
			ret = CSS_ERROR_S3DC_BUF_FREE_FAIL;
	}

	return ret;
}

static int s3dc_ioctl_fill_param(int id, struct css_s3dc_config *config,
						struct s3dc_set_param *p)
{
	int ret = CSS_SUCCESS;

	switch (id) {
	case INPUT_0:
		config->in_path0			= p->param[1];
		config->input0.rect.start_x	= p->param[2];
		config->input0.rect.start_y	= p->param[3];
		config->input0.rect.width	= p->param[4];
		config->input0.rect.height	= p->param[5];
		config->input0.fmt			= p->param[6];
		config->input0.align		= p->param[7];
		break;
	case INPUT_1:
		config->in_path1			= p->param[1];
		config->input1.rect.start_x	= p->param[2];
		config->input1.rect.start_y	= p->param[3];
		config->input1.rect.width	= p->param[4];
		config->input1.rect.height	= p->param[5];
		config->input1.fmt			= p->param[6];
		config->input1.align		= p->param[7];
		break;
	case OUTPUT:
		config->out_path			= p->param[1];
		config->output.rect.start_x	= p->param[2];
		config->output.rect.start_y	= p->param[3];
		config->output.rect.width	= p->param[4];
		config->output.rect.height	= p->param[5];
		config->output.fmt			= p->param[6];
		config->output.align		= p->param[7];
		config->outmode				= p->param[8];
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int s3dc_ioctl_get_rsvd_memory(void)
{
	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	return (s3dc_plat->rsvd_mem == NULL) ? 0 : 1;
}

static void s3dc_work_for_zsl_load(struct work_struct *work)
{
	int ret = CSS_SUCCESS;

	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
	struct capture_fh *capfh = NULL;
	struct css_bufq *bufq = NULL;

	if (pctxt) {
		bufq = (struct css_bufq*)pctxt->v4l2_capture_buf_set;
		if (bufq && pctxt->v4l2_capture_cam_fh) {
			capfh = (struct capture_fh *)pctxt->v4l2_capture_cam_fh;
			css_zsl_low("s3dc zsl load workq\n");
			ret = bayer_stereo_load_frame(capfh);
		}
		if (ret < 0) {
			bufq->error = 1;
			wake_up_interruptible(&bufq->complete.wait);
		}
	}

	return;
}

static int s3dc_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();

	if (pctxt == NULL) {
		ret = s3dc_create_context();
		if (ret == CSS_SUCCESS) {
			file->private_data = (NXSCContext*)s3dc_get_context();
			s3dc_hw_init();
		}
	} else {
		ret = -EBUSY;
	}

	return ret;
}

static int s3dc_release(struct inode *ignored, struct file *file)
{
	int ret = 0;

	NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();

	s3dc_stop_trans();

	if (pctxt) {
		s3dc_destroy_context();
		file->private_data = 0;
		s3dc_hw_deinit();
	}

	return ret;
}

static long s3dc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	struct css_s3dc_control_args *ctrl_args
					= (struct css_s3dc_control_args *)arg;

	switch (cmd) {
		case CSS_IOC_S3DC_CONTROL:
		{
			switch (ctrl_args->cmode) {
				case CMD_GET_VERSION:
				{
					struct s3dc_set_param *s_param = NULL;
					s_param
						= (struct s3dc_set_param*)&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_get_version(&s_param->param[0],
												&s_param->param[1]);
					break;
				}
				case CMD_SET_INPUT:
				case CMD_SET_OUTPUT:
				{
					struct s3dc_set_param *s_param = NULL;

					NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();

					if (!pctxt) {
						return CSS_ERROR_S3DC_INVALID_CTXT;
					}

					s_param
						= (struct s3dc_set_param*)&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_fill_param(s_param->param[0],
												&pctxt->config, s_param);
					if (ret == CSS_SUCCESS) {
						pctxt->config_done |= (1 << s_param->param[0]);
					}
					break;
				}
				case CMD_GET_RSVD_BUFFER:
				{
					struct s3dc_set_param *s_param = NULL;
					s_param
						= (struct s3dc_set_param*)&ctrl_args->ctrl.mode.s_param;
					s_param->param[0] = s3dc_ioctl_get_rsvd_memory();
					break;
				}
				case CMD_START:
					ret = s3dc_ioctl_start();
					break;
				case CMD_STOP:
					ret = s3dc_ioctl_stop();
					break;
				case CMD_OUT_S3D_MODE:
				{
					struct s3dc_set_param *s_param = NULL;
					s_param
						= (struct s3dc_set_param*)&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_s3d_outmode(s_param->param[0]);
					break;
				}
				case CMD_OUT_EXT_MODE:
				{
					struct s3dc_set_param *s_param = NULL;
					s_param
						= (struct s3dc_set_param*)&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_ext_outmode(s_param->param[0],
												s_param->param[1]);
					break;
				}
				case CMD_MANUAL_CONVERGENCE:
				{
					struct s3dc_set_param *s_param = NULL;
					s_param
						= (struct s3dc_set_param*)&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_manual_convergence(s_param->param[0],
														s_param->param[1]);
					break;
				}
				case CMD_AUTO_CONVERGENCE:
				{
					struct s3dc_set_param *s_param = NULL;
					s_param
						= (struct s3dc_set_param*)&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_auto_convergence(s_param->param[0],
													s_param->param[1]);
					break;
				}
				case CMD_AUTO_CONVERGENCE_SET_AREA:
				{
					struct s3dc_set_param *s_param = NULL;
					int sx, sy, w, h;
					s_param
						= (struct s3dc_set_param*)&ctrl_args->ctrl.mode.s_param;
					sx = s_param->param[0];
					sy = s_param->param[1];
					w = s_param->param[2];
					h = s_param->param[3];
					ret = s3dc_ioctl_auto_convergence_set_area(sx, sy, w, h);
					break;
				}
				case CMD_AUTO_CONVERGENCE_SET_EDGE_THRESHOLD:
				{
					struct s3dc_set_param *s_param = NULL;
					s_param
						= (struct s3dc_set_param*)&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_auto_convergence_set_edge_threshold(
															s_param->param[0]);
					break;
				}
				case CMD_AUTO_CONVERGENCE_SET_DP_REDUN:
				{
					struct s3dc_set_param *s_param = NULL;
					s_param
						= (struct s3dc_set_param*)&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_auto_convergence_set_disparity_offset(
										s_param->param[0], s_param->param[1]);
					break;
				}
				case CMD_CONVERGENCE_DP_STEP:
				{
					struct s3dc_set_param *s_param = NULL;
					s_param
						= (struct s3dc_set_param*)&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_convergence_set_disparity_step(
										s_param->param[0], s_param->param[1]);
					break;
				}
				case CMD_SET_ROTATE_DEGREE:
				{
					struct s3dc_rotate_set *rot_set = NULL;
					rot_set	= (struct s3dc_rotate_set*)
								&ctrl_args->ctrl.mode.s_param;
					ret	= s3dc_ioctl_rotate_set_degree(
										(BrRotationInfo*)rot_set);
					break;
				}
				case CMD_SET_TILT_ANGLE:
				{
					struct s3dc_set_param *s_param = NULL;
					s_param
						= (struct s3dc_set_param*)&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_tilt_set_value(s_param->param[0],
													s_param->param[1]);
					break;
				}
				case CMD_AUTO_LUMINANCE_CONTROL:
				{
					struct s3dc_set_param *s_param = NULL;
					s_param
						= (struct s3dc_set_param*)&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_auto_luminance_control(s_param->param[0]);
					break;
				}
				case CMD_LUMINANCE_CONTROL:
				{
					struct s3dc_luminance_set *l_set = NULL;
					l_set = (struct s3dc_luminance_set*)
							&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_luminance_control(l_set);
					break;
				}
				case CMD_SATURATION_CONTROL:
				{
					struct s3dc_saturation_set *s_set = NULL;
					s_set = (struct s3dc_saturation_set*)
							&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_saturation_control(s_set);
					break;
				}
				case CMD_BRIGHTNESS_CONTROL:
				{
					struct s3dc_set_param *s_param = NULL;
					s_param = (struct s3dc_set_param*)
								&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_brightness_control(s_param->param[0],
										s_param->param[1]);
					break;
				}
				case CMD_CONTRAST_CONTROL:
				{
					struct s3dc_set_param *s_param = NULL;
					s_param = (struct s3dc_set_param*)
								&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_contrast_control(s_param->param[0],
										s_param->param[1], s_param->param[2]);
					break;
				}
				case CMD_HUE_CONTROL:
				{
					struct s3dc_set_param *s_param = NULL;
					s_param = (struct s3dc_set_param*)
								&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_hue_control(s_param->param[0],
										s_param->param[1]);
					break;
				}
				case CMD_COLOR_TEST_PATTERN_CONTROL:
				{
					struct s3dc_set_param *s_param = NULL;
					s_param = (struct s3dc_set_param*)
								&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_color_pattern_control(s_param->param[0],
															s_param->param[1]);
					break;
				}
				case CMD_SET_ZOOM:
				{
					struct s3dc_set_param *s_param = NULL;
					s_param = (struct s3dc_set_param*)
								&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_zoom(s_param->param[0]);
					break;
				}
				case CMD_CHANGE_LR:
				{
					struct s3dc_set_param *s_param = NULL;
					s_param = (struct s3dc_set_param*)
								&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_change_lr(s_param->param[0]);
					break;
				}
				case CMD_GET_CROP_SIZE_INFO:
				{
					struct s3dc_crop_size_set *crop_set = NULL;
					crop_set = (struct s3dc_crop_size_set*)
								&ctrl_args->ctrl.mode.s_param;
					ret = s3dc_ioctl_get_crop_size_info(crop_set);
					break;
				}
				case CMD_SET_COLOR_COEFF:
				{
					struct s3dc_coeff_set *coeff_set = NULL;
					coeff_set = (struct s3dc_coeff_set*)
								&ctrl_args->ctrl.mode.s_param;

					break;
				}
				case CMD_INIT_INPUT_MEM:
				{
					NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
					struct s3dc_buf_param *b_param = NULL;

					if (!pctxt) {
						return CSS_ERROR_S3DC_INVALID_CTXT;
					}

					b_param = (struct s3dc_buf_param*)
								&ctrl_args->ctrl.mode.s_buf;

					if (b_param->index == 0) {
						if (pctxt->config.in_path0 == INPUT_SENSOR) {
							return CSS_ERROR_S3DC_INVALID_PATH;
						}
					}

					if (b_param->index == 1) {
						if (pctxt->config.in_path1 == INPUT_SENSOR) {
							return CSS_ERROR_S3DC_INVALID_PATH;
						}
					}

					ret = s3dc_ioctl_init_input_buffer(b_param);

					if (ret == CSS_SUCCESS) {
						if (b_param->index == 0)
							pctxt->config.inaddr0 = b_param->dma_addr;
						else
							pctxt->config.inaddr1 = b_param->dma_addr;
					}
					break;
				}
				case CMD_DEINIT_INPUT_MEM:
				{
					NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
					struct s3dc_buf_param *b_param = NULL;

					if (!pctxt) {
						return CSS_ERROR_S3DC_INVALID_CTXT;
					}

					b_param = (struct s3dc_buf_param*)
								&ctrl_args->ctrl.mode.s_buf;

					if (b_param->index == 0) {
						if (pctxt->config.in_path0 == INPUT_SENSOR) {
							return CSS_ERROR_S3DC_INVALID_PATH;
						}
					}

					if (b_param->index == 1) {
						if (pctxt->config.in_path1 == INPUT_SENSOR) {
							return CSS_ERROR_S3DC_INVALID_PATH;
						}
					}

					ret = s3dc_ioctl_deinit_input_buffer(b_param);

					if (ret == CSS_SUCCESS) {
						if (b_param->index == 0)
							pctxt->config.inaddr0 = 0;
						else
							pctxt->config.inaddr1 = 0;
					}
					break;
				}
				case CMD_INIT_OUT_MEM:
				{
					NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
					struct s3dc_buf_param *b_param = NULL;

					if (!pctxt) {
						return CSS_ERROR_S3DC_INVALID_CTXT;
					}

					if (pctxt->config.out_path == OUTPUT_V4L2_MEM) {
						return CSS_ERROR_S3DC_INVALID_PATH;
					}

					b_param = (struct s3dc_buf_param*)
								&ctrl_args->ctrl.mode.s_buf;
					ret = s3dc_ioctl_init_out_buffer(b_param);

					if (ret == CSS_SUCCESS) {
						pctxt->frame_buffer_sm_addr[4] = b_param->dma_addr;
					}
					break;
				}
				case CMD_DEINIT_OUT_MEM:
				{
					NXSCContext *pctxt = (NXSCContext*)s3dc_get_context();
					struct s3dc_buf_param *b_param = NULL;

					if (!pctxt) {
						return CSS_ERROR_S3DC_INVALID_CTXT;
					}

					if (pctxt->config.out_path == OUTPUT_V4L2_MEM) {
						return CSS_ERROR_S3DC_INVALID_PATH;
					}

					b_param = (struct s3dc_buf_param*)
								&ctrl_args->ctrl.mode.s_buf;
					ret = s3dc_ioctl_deinit_input_buffer(b_param);

					if (ret == CSS_SUCCESS) {
						pctxt->frame_buffer_sm_addr[4] = 0;
					}
					break;
				}
				default:
					ret = -EINVAL;
				break;
			}
			break;
		}
		default:
			ret = -EINVAL;
			break;
	}

	return (long)ret;
}

int s3dc_mmap(struct file *file, struct vm_area_struct *vma)
{
	int result = 0;
	unsigned long size = vma->vm_end - vma->vm_start;

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size,
						vma->vm_page_prot))
	{
		css_err("mmap fail %x %x\n",
				(unsigned int)vma->vm_start, (unsigned int)vma->vm_pgoff);
		return -1;
	}

	return result;
}

unsigned int s3dc_poll(struct file *file, poll_table *wait)
{
	int poll_mask = 0;

	struct s3dc_platform *s3dc_plat = s3dc_get_platform();

	if (s3dc_plat->frame_end.done) {
		poll_mask |= POLLIN | POLLRDNORM;
		INIT_COMPLETION(s3dc_plat->frame_end);
	} else {
		poll_wait(file, &s3dc_plat->frame_end.wait, wait);
	}

	return poll_mask;
}

static struct file_operations s3dc_fops = {
	.owner			= THIS_MODULE,
	.open			= s3dc_open,
	.release		= s3dc_release,
	.unlocked_ioctl = s3dc_ioctl,
	.mmap			= s3dc_mmap,
	.poll			= s3dc_poll,
};

static struct miscdevice s3dc_miscdevice = {
	.minor	= S3DC_MINOR,
	.name	= S3DC_MODULE_NAME,
	.fops	= &s3dc_fops,
};

static void *s3dc_get_camera_pool(void)
{
	return get_css_plat()->rsvd_mem;
}

static int s3dc_probe(struct platform_device *pdev)
{
	int ret = CSS_SUCCESS;
	struct s3dc_platform *s3dc_plat = NULL;
	struct resource *res;
	unsigned long long res_size;

	css_info("s3dc_probe!!\n");

	s3dc_plat = kzalloc(sizeof(struct s3dc_platform), GFP_KERNEL);

	if (!s3dc_plat)
		return -ENOMEM;

	spin_lock_init(&s3dc_plat->s3dc_lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (NULL == res) {
		css_err("failed to get resource!\n");
		ret = -ENOENT;
		goto err_pos_1;
	}
	res_size = res->end - res->start + 1;

	s3dc_plat->iores = request_mem_region(res->start, res_size, pdev->name);
	if (NULL == s3dc_plat->iores) {
		css_err("failed to request mem region : %s!\n", pdev->name);
		ret = CSS_ERROR_GET_RESOURCE;
		goto err_pos_1;
	}

	s3dc_plat->iobase = ioremap_nocache(res->start, res_size);
	if (NULL == s3dc_plat->iobase) {
		css_err("failed to ioremap!\n");
		ret = CSS_ERROR_IOREMAP;
		goto err_pos_2;
	}

	platform_set_drvdata(pdev, s3dc_plat);

	s3dc_plat->irq_s3dc = platform_get_irq(pdev, 0);

	ret = request_irq(s3dc_plat->irq_s3dc, s3dc_irq, 0, pdev->name, s3dc_plat);
	if (ret < 0) {
		css_err("failed to request mdis irq %d\n", ret);
		ret = CSS_ERROR_REQUEST_IRQ;
		goto err_pos_3;
	}

	ret = misc_register(&s3dc_miscdevice);
	if (ret < 0) {
		css_err("failed to register misc %d\n", ret);
		goto err_pos_4;
	}

	s3dc_set_platform(s3dc_plat);

	init_completion(&s3dc_plat->frame_end);
	INIT_WORK(&s3dc_plat->work_zsl_load_to_s3dc, s3dc_work_for_zsl_load);

	s3dc_plat->rsvd_mem = (struct css_rsvd_memory*)s3dc_get_camera_pool();

	css_info("s3dc base addr : %x\n", (unsigned int)s3dc_plat->iobase);
	css_info("s3dc irq = %d registered\n", s3dc_plat->irq_s3dc);
	css_info("s3dc misc driver registered : minor %d\n", S3DC_MINOR);

	return CSS_SUCCESS;

err_pos_4:
	free_irq(s3dc_plat->irq_s3dc, s3dc_plat);

err_pos_3:
	iounmap(s3dc_plat->iobase);

err_pos_2:
	release_mem_region(res->start, res_size);

err_pos_1:
	kfree(s3dc_plat);

	return ret;
}

static int s3dc_remove(struct platform_device *pdev)
{
	struct s3dc_platform *s3dc_plat = platform_get_drvdata(pdev);
	int res_size = 0;

	if (!s3dc_plat)
		return CSS_FAILURE;

	res_size = s3dc_plat->iores->end - s3dc_plat->iores->start + 1;

	if (s3dc_plat->rsvd_mem)
		odin_css_destroy_mem_pool(s3dc_plat->rsvd_mem);

	if (s3dc_plat->irq_s3dc)
		free_irq(s3dc_plat->irq_s3dc, s3dc_plat);

	if (s3dc_plat->iobase)
		iounmap(s3dc_plat->iobase);

	if (s3dc_plat->iores)
		release_mem_region(s3dc_plat->iores->start, res_size);

	kfree(s3dc_plat);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id odin_s3dc_match[] = {
	{ .compatible = CSS_OF_S3DC_NAME},
	{},
};
#endif /* CONFIG_OF */

static struct platform_driver s3dc_driver = {
	.probe		= s3dc_probe,
	.remove 	= s3dc_remove,
	.driver = {
		.name	= S3DC_MODULE_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odin_s3dc_match),
#endif
	}
};

static int __init odin_s3dc_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&s3dc_driver);

	return ret;
}

static void __exit odin_s3dc_exit(void)
{
	platform_driver_unregister(&s3dc_driver);
	return;
}

late_initcall(odin_s3dc_init);
module_exit(odin_s3dc_exit);

MODULE_DESCRIPTION("s3dc driver for ODIN CSS");
MODULE_AUTHOR("Mtekvision <http://www.mtekvision.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
