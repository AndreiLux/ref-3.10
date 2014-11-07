/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Youngki Lyu <youngki.lyu@lge.com>
 * Jungmin Park <jungmin016.park@lge.com>
 * Younghyun Jo <younghyun.jo@lge.com>
 * Seokhoon.Kang <m4seokhoon.kang@lgepartner.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <asm/ioctl.h>
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/odin_iommu.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>

#include "../common/vcodec_dpb.h"
#include <media/odin/vcodec/vlog.h>
#include <media/odin/vcodec/vppu_drv.h>
#include <media/odin/vcodec/vcore/ppu.h>

#define VPPU_MAX_SCHEDULE_RETRY 		0x10000

struct vppu_buffer
{
	unsigned int paddr;
	unsigned long *vaddr;
	unsigned int size;
};

struct vppu_channel
{
	volatile enum
	{
		VPPU_RESETTING,
		VPPU_NORMAL,
	} reset_state;

	volatile enum
	{
		VPPU_OPEN,
		VPPU_CLOSING,
		VPPU_CLOSED,
	} close_state;

	volatile enum
	{
		VPPU_IDLE,
		VPPU_RUNNING,
	} vcore_state;

	struct workqueue_struct *vcore_report_wq;
	struct completion close_completion;

	void *vcore_id;
	struct vcore_ppu_ops vcore_ops;

	void *vcore_select_id;
	void *dpb_id;

	struct vppu_buffer work_buf;

	int irq_done;
	wait_queue_head_t irq_wq;

	void *cb_arg;
	void (*cb_isr)(void *cb_arg, struct vppu_report *report);

	struct vppu_report report;

	struct list_head list;
	struct mutex lock;
};

struct vppu_desc
{
	struct list_head channel_list;

	void *(*get_vcore)(struct vcore_ppu_ops *ops, bool without_fail,
					unsigned int width,
					unsigned int height,
					unsigned int fr_residual,
					unsigned int fr_divider);
	void (*put_vcore)(void *select_id);
}vppu_db;

struct vppu_vcore_report_work
{
	struct work_struct vcore_report_work;
	struct vcore_ppu_report vcore_report;
	struct vppu_channel *vppu_ch;
};

static void _vppu_wq_vcore_report(struct work_struct *work)
{
	struct vppu_vcore_report_work *wk
			= container_of(work, struct vppu_vcore_report_work,
							vcore_report_work);
	struct vppu_channel *ch = wk->vppu_ch;
	struct vcore_ppu_report *vcore_report = &wk->vcore_report;
	bool report=false;

	switch (ch->reset_state) {
	case VPPU_NORMAL:
		switch (ch->close_state) {
		case VPPU_OPEN:
			switch (ch->vcore_state) {
			case VPPU_IDLE:
				vlog_error("vcore state:%d\n", ch->vcore_state);
				break;
			case VPPU_RUNNING:
				mutex_lock( &ch->lock );
				switch (vcore_report->hdr) {
				case VCORE_PPU_RESET:
					switch (vcore_report->info.reset) {
					case VCORE_PPU_REPORT_RESET_START:
						ch->reset_state = VPPU_RESETTING;
						break;
					case VCORE_PPU_REPORT_RESET_END:
						ch->reset_state = VPPU_NORMAL;
						break;
					default :
						vlog_error("invalid vcore_report->info.reset:%d\n",
									vcore_report->info.reset);
						break;
					}
					break;
				case VCORE_PPU_DONE:
					switch (vcore_report->info.done) {
					case VCORE_PPU_OK :
						ch->report.hdr = VPPU_OK;
						report = true;
						break;
					case VCORE_PPU_NO :
						ch->report.hdr = VPPU_FAIL;
						report = true;
						break;
					default :
						vlog_error("invalid vcore_report->info.done:%d\n",
									vcore_report->info.done);
						break;
					}
					break;
				case VCORE_PPU_FEED:
					report = true;
					ch->vcore_state = VPPU_IDLE;
					break;

				default :
					vlog_error("invalid vcore_report->hdr:%d\n",
							vcore_report->hdr);
					break;
				}
				mutex_unlock( &ch->lock );
				break;
			default:
				vlog_error("invalid vcore_state:%d\n", ch->vcore_state);
				break;
			}
			break;
		case VPPU_CLOSING:
			mutex_lock( &ch->lock );
			ch->close_state = VPPU_CLOSED;
			mutex_unlock( &ch->lock );

			complete_all(&ch->close_completion);
			break;
		case VPPU_CLOSED:
			vlog_error("closed state\n");
			break;
		default:
			vlog_error("invalid close_state:%d\n", ch->close_state);
			break;
		}
		break;
	case VPPU_RESETTING:
		vlog_warning("vppu is resetting\n");
		break;
	default:
		vlog_error("invalid reset_state:%d\n", ch->reset_state);
		break;
	}

	if (report == true) {
		if (ch->cb_isr)
			ch->cb_isr(ch->cb_arg, &ch->report);
		else {
			ch->irq_done = 1;
			wake_up_interruptible(&ch->irq_wq);
		}
	}

	kfree(wk);
}

static void _vppu_cb_vcore_report(void *p_id, struct vcore_ppu_report * report)
{
	struct vppu_channel *ch = (struct vppu_channel *)p_id;
	struct vppu_vcore_report_work *wk;

	if (!ch) {
		vlog_error("ch is NULL\n");
		return;
	}
	else if (ch->close_state == VPPU_CLOSED) {
		vlog_error("closed state\n");
		return;
	}

	wk = kzalloc(sizeof(struct vppu_vcore_report_work), GFP_ATOMIC);
	if (!wk) {
		vlog_error("failed to alloc buf for callback data\n");
		return;
	}
	wk->vcore_report = *report;
	wk->vppu_ch = ch;

	INIT_WORK(&wk->vcore_report_work, _vppu_wq_vcore_report);

	queue_work(ch->vcore_report_wq, &wk->vcore_report_work);
}

static bool _vppu_wait_interrupt(struct vppu_channel *ch)
{
	unsigned long remain_timeout = 0;
	int timeout = 5000;

	ch->irq_done = 0;

	remain_timeout = wait_event_interruptible_timeout(ch->irq_wq,
			ch->irq_done, msecs_to_jiffies(timeout));

	if (remain_timeout <= 0) {
		vlog_error("PPU interrupt timeouted :: %lu \n", remain_timeout);
		return false;
	}

	return true;
}

enum vppu_report_info vppu_rotate(void *vppu_id,
									unsigned int src_paddr,
									unsigned int dst_paddr,
									int width, int height,
									int angle,
									enum vppu_image_format format)
{
	enum vppu_report_info ret = VPPU_FAIL;
	struct vppu_channel *ch= (struct vppu_channel *)vppu_id;
	enum vcore_ppu_image_format vcore_image_format;
	enum vcore_ppu_ret vcore_ret = VCORE_PPU_SUCCESS;
	unsigned int retry_count = 0;

	if (ch == NULL) {
		vlog_error("vppu_ch is NULL");
		return VPPU_FAIL;
	}

	if (src_paddr == 0 || dst_paddr == 0) {
		vlog_error("src_paddr(0x%08X) or dst_paddr(0x%08X) is error\n",
					src_paddr, dst_paddr);
		return VPPU_FAIL;
	}

	if (angle % 90) {
		vlog_error("invalid angle(%d)\n", angle);
		return VPPU_FAIL;
	}

	switch (format) {
	case VPPU_IMAGE_FORMAT_YUV420_2P:
		vcore_image_format = VCORE_PPU_IMAGE_FORMAT_YUV420_2P;
		break;
	case VPPU_IMAGE_FORMAT_YUV420_3P:
		vcore_image_format = VCORE_PPU_IMAGE_FORMAT_YUV420_3P;
		break;
	default:
		vlog_error("invald format(%d)\n", format);
		return VPPU_FAIL;
	}

	switch (ch->reset_state) {
	case VPPU_NORMAL:
		switch (ch->close_state) {
		case VPPU_OPEN:
			switch (ch->vcore_state) {
			case VPPU_IDLE:
			case VPPU_RUNNING:
				mutex_lock( &ch->lock );

				ch->vcore_state = VPPU_RUNNING;

				mutex_unlock( &ch->lock );

				do {
					vcore_ret = ch->vcore_ops.rotate(ch->vcore_id,
									src_paddr, dst_paddr,
									width, height,
									angle,
									vcore_image_format);
					if (vcore_ret != VCORE_PPU_RETRY)
						break;

					retry_count++;
					if ((retry_count % 1000) == 0)
						vlog_print( VLOG_MULTI_INSTANCE, "retry: %u\n", retry_count );

				} while (retry_count < VPPU_MAX_SCHEDULE_RETRY);

				if (retry_count)
					vlog_trace( "retry: %u\n", retry_count );

				if (vcore_ret != VCORE_PPU_SUCCESS)
					return VPPU_FAIL;

				if (ch->cb_isr == NULL) {
					if (_vppu_wait_interrupt(ch) == false) {
						vlog_error("wait_interrupt failed\n");
						break;
					}
				}
				ret = VPPU_OK;
				break;
		/*
			case VPPU_RUNNING:
				vlog_error("running state, cb_isr:0x%X\n",
								(unsigned int)(ch->cb_isr));
				break;
		*/
			default:
				vlog_error("invalid vcore_state:%d\n", ch->vcore_state);
				break;
			}
			break;
		case VPPU_CLOSING:
			vlog_error("closing state\n");
			break;
		case VPPU_CLOSED:
			vlog_error("closed state\n");
			break;
		default:
			vlog_error("invalid close_state:%d\n", ch->close_state);
			break;
		}
		break;
	case VPPU_RESETTING:
		vlog_warning("vppu is resetting\n");
		break;
	default:
		vlog_error("invalid reset_state:%d\n", ch->reset_state);
		break;
	}

	return ret;
}

void *vppu_open(bool without_fail,
					unsigned int width, unsigned int height,
					unsigned int fr_residual, unsigned int fr_divider,
					void *cb_arg,
					void (*cb_isr)(void *cb_arg, struct vppu_report* report))
{
	struct vppu_channel *ch = NULL;
	enum vcore_ppu_ret vcore_ret = VCORE_PPU_SUCCESS;
	unsigned int retry_count = 0;

	if (vppu_db.get_vcore == NULL) {
		vlog_error("vppu_get_vcore is NULL\n");
		return (void *)NULL;
	}

	ch = vzalloc(sizeof(struct vppu_channel));
	if (ch == NULL) {
		vlog_error("vzalloc failed size:%d\n", sizeof(struct vppu_channel));
		return (void*)NULL;
	}

	mutex_init( &ch->lock );
	mutex_lock( &ch->lock );

	ch->vcore_select_id = vppu_db.get_vcore(&ch->vcore_ops,
										without_fail,
										width, height,
										fr_residual, fr_divider);
	if (ch->vcore_select_id == NULL) {
		vlog_error( "no available vcore\n" );
		goto err_get_vcore;
	}

	ch->dpb_id = vcodec_dpb_open();
	if (IS_ERR_OR_NULL(ch->dpb_id)) {
		vlog_error("vcodec_dpb_open failed\n");
		goto err_dpb_open;
	}

	if (vcodec_dpb_add(ch->dpb_id, -1, &ch->work_buf.paddr, NULL,
							80*1024) == false) {
		vlog_error("work buffer alloc failed\n");
		goto err_work_buf_alloc;
	}

	do {
		vcore_ret = ch->vcore_ops.open(&ch->vcore_id,
									ch->work_buf.paddr, ch->work_buf.vaddr,
									ch->work_buf.size,
									width, height,
									fr_residual, fr_divider,
									(void *)ch, _vppu_cb_vcore_report);
		if (vcore_ret != VCORE_PPU_RETRY)
			break;

		retry_count++;
		if ((retry_count % 1000) == 0)
			vlog_print( VLOG_MULTI_INSTANCE, "retry: %u\n", retry_count );

	} while (retry_count < VPPU_MAX_SCHEDULE_RETRY);

	if (retry_count)
		vlog_trace( "retry: %u\n", retry_count );

	if (ch->vcore_id == NULL) {
		vlog_error( "error! failed to get vcore_id\n");
		goto err_vcore_open;
	}


	ch->vcore_report_wq = \
					  create_singlethread_workqueue( "vppu_vcore_report_wq" );

	ch->cb_arg = cb_arg;
	ch->cb_isr = cb_isr;

	ch->reset_state = VPPU_NORMAL;
	ch->close_state = VPPU_OPEN;
	ch->vcore_state = VPPU_IDLE;

	mutex_unlock(&ch->lock);

	init_completion(&ch->close_completion);
	init_waitqueue_head(&ch->irq_wq);

	vlog_trace("success - vppu:0x%X, vcore:0x%X\n",
				(unsigned int)ch, (unsigned int)ch->vcore_id);

	return (void*)ch;

err_vcore_open:
	vcodec_dpb_del(ch->dpb_id, -1, ch->work_buf.paddr);
	memset(&ch->work_buf, 0 , sizeof(struct vppu_buffer));

err_work_buf_alloc:
	if (ch->dpb_id) {
		vcodec_dpb_close(ch->dpb_id);
		ch->dpb_id = NULL;
	}

err_dpb_open:
	if (vppu_db.put_vcore && ch->vcore_select_id) {
		vppu_db.put_vcore(ch->vcore_select_id);
		ch->vcore_select_id = NULL;
	}

err_get_vcore:
	mutex_unlock(&ch->lock);
	mutex_destroy( &ch->lock );
	vfree(ch);
	return (void*)NULL;
}

void vppu_close(void *vppu_id)
{
	struct vppu_channel *ch = (struct vppu_channel*)vppu_id;
	enum vcore_ppu_ret vcore_ret = VCORE_PPU_SUCCESS;
	unsigned int retry_count = 0;

	if (ch == NULL) {
		vlog_error("vppu_ch is NULL\n");
		return;
	}

	mutex_lock( &ch->lock );

	switch (ch->reset_state) {
	case VPPU_NORMAL:
		switch (ch->close_state) {
		case VPPU_OPEN:
			switch (ch->vcore_state) {
			case VPPU_IDLE:
				ch->close_state = VPPU_CLOSED;
				break;
			case VPPU_RUNNING:
				ch->close_state = VPPU_CLOSING;
				break;
			default:
				vlog_error("invalid vcore_state:%d\n", ch->vcore_state);
				break;
			}
			break;
		case VPPU_CLOSING:
			vlog_error("closing state\n");
			break;
		case VPPU_CLOSED:
			vlog_error("closed state\n");
			break;
		default:
			vlog_error("invalid close_state:%d\n", ch->close_state);
			break;
		}
		break;
	case VPPU_RESETTING:
		vlog_error("vppu is resetting\n");
		break;
	default:
		vlog_error("invalid reset_state:%d\n", ch->reset_state);
		break;
	}

	if (ch->close_state == VPPU_CLOSING) {
		if (wait_for_completion_interruptible_timeout(&ch->close_completion, \
													HZ / 33) <= 0)
			vlog_error(
				"error waiting for reply to vcore done - close_state:%d\n",
				ch->close_state);

		ch->close_state = VPPU_CLOSED;
	}

	if (ch->vcore_id) {
		do {
			vcore_ret = ch->vcore_ops.close(ch->vcore_id);
			if (vcore_ret != VCORE_PPU_RETRY)
				break;

			retry_count++;
			if ((retry_count % 1000) == 0)
				vlog_print( VLOG_MULTI_INSTANCE, "retry: %u\n", retry_count );

		} while (retry_count < VPPU_MAX_SCHEDULE_RETRY);

		if (retry_count)
			vlog_trace( "retry: %u\n", retry_count );
	}

	if (ch->work_buf.paddr) {
		vcodec_dpb_del(ch->dpb_id, -1, ch->work_buf.paddr);
		memset(&ch->work_buf, 0 , sizeof(struct vppu_buffer));
	}

	if (ch->dpb_id) {
		vcodec_dpb_close(ch->dpb_id);
		ch->dpb_id = NULL;
	}

	if (vppu_db.put_vcore && ch->vcore_select_id)
		vppu_db.put_vcore(ch->vcore_select_id);

	flush_workqueue(ch->vcore_report_wq);
	destroy_workqueue(ch->vcore_report_wq);

	mutex_unlock( &ch->lock );
	mutex_destroy( &ch->lock );

	vfree(ch);
}

int vppu_init(void* (*get_vcore)(struct vcore_ppu_ops *ops,
								bool without_fail,
								unsigned int width, unsigned int height,
								unsigned int fr_residual,
								unsigned int fr_divider),
			void (*put_vcore)(void *select_id))
{

	vppu_db.get_vcore = get_vcore;
	vppu_db.put_vcore = put_vcore;

	return 0;
}

int vppu_cleanup(void)
{
	return 0;
}

