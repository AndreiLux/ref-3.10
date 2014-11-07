/*
 * Camera Bayer Control (Store/Load) driver
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

#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/dma-buf.h>

#include "odin-css.h"
#include "odin-css-utils.h"
#include "odin-css-platdrv.h"
#include "odin-s3dc.h"
#include "odin-framegrabber.h"
#include "odin-isp.h"
#include "odin-isp-wrap.h"
#include "odin-capture.h"
#include "odin-bayercontrol.h"
#include "odin-css-system.h"

/* #define ODIN_ZSL_STORE_TIME_ESTIMATE 1 */
#define ODIN_ZSL_LOAD_TYPE_ONESHOT_ONLY 1

unsigned int *bayer_ion_handle_data = NULL;
unsigned int *bayer_ion_client = NULL;
unsigned int bayer_ion_size = 0;
unsigned int bayer_ion_align_size = 0;


static void bayer_isp_read_path_enable(css_zsl_path rpath)
{
	switch (rpath) {
	case ZSL_READ_PATH1:
		odin_isp_wrap_set_read_path_enable(ZSL_READ_PATH1);
		break;
	case ZSL_READ_PATH_ALL:
		odin_isp_wrap_set_read_path_enable(ZSL_READ_PATH0);
		odin_isp_wrap_set_read_path_enable(ZSL_READ_PATH1);
		break;
	case ZSL_READ_PATH0:
	default:
		odin_isp_wrap_set_read_path_enable(ZSL_READ_PATH0);
		break;
	}
}

static void bayer_isp_read_path_disable(css_zsl_path rpath)
{
	switch (rpath) {
	case ZSL_READ_PATH1:
		odin_isp_wrap_set_read_path_disable(ZSL_READ_PATH1);
		break;
	case ZSL_READ_PATH_ALL:
		odin_isp_wrap_set_read_path_disable(ZSL_READ_PATH0);
		odin_isp_wrap_set_read_path_disable(ZSL_READ_PATH1);
		break;
	case ZSL_READ_PATH0:
	default:
		odin_isp_wrap_set_read_path_disable(ZSL_READ_PATH0);
		break;
	}
}

static int bayer_buffer_wait_queue_available(struct css_bufq *bufq, int msec)
{
	unsigned long flags;
	int empty = 0;
	int timeout = 0;

	timeout = msec;

	do {
		spin_lock_irqsave(&bufq->lock, flags);
		empty = list_empty(&bufq->head);
		spin_unlock_irqrestore(&bufq->lock, flags);

		if (!empty)
			return timeout;
		else
			usleep_range(1000, 1000);
	} while (timeout-- > 0);

	timeout = 0;

	return timeout;
}

static struct css_buffer *bayer_buffer_find_get_frame(struct css_bufq *bufq,
						unsigned int find_frame, int state, int waitTimeMs)
{
	struct css_buffer *tmp_buffer, *n;
	unsigned long flags;
	unsigned int need_to_wait = 0;
	int empty = 0;
	int timeout = 0;

	timeout = waitTimeMs;

	spin_lock_irqsave(&bufq->lock, flags);
	empty = list_empty(&bufq->head);
	spin_unlock_irqrestore(&bufq->lock, flags);

	if (empty) {
		css_info("zsl empty\n");
		return NULL;
	} else {

		spin_lock_irqsave(&bufq->lock, flags);

		tmp_buffer = list_first_entry(&bufq->head, struct css_buffer, list);
		if (tmp_buffer->frame_cnt < find_frame)
			need_to_wait = 1;

		spin_unlock_irqrestore(&bufq->lock, flags);

		if (need_to_wait && timeout > 0) {
			css_info("zsl wait frame %d\n", find_frame);

			do {
				usleep_range(1000, 1000);

				spin_lock_irqsave(&bufq->lock, flags);
				tmp_buffer = list_first_entry(&bufq->head,
											struct css_buffer, list);
				spin_unlock_irqrestore(&bufq->lock, flags);

				if (tmp_buffer->frame_cnt < find_frame)
					continue;

				spin_lock_irqsave(&bufq->lock, flags);
				list_del(&tmp_buffer->list);
				tmp_buffer->state = state;
				spin_unlock_irqrestore(&bufq->lock, flags);

				return tmp_buffer;

			} while (timeout-- > 0);

			css_info("zsl can't find within timeout %d(%d)\n", find_frame,
						tmp_buffer->frame_cnt);
			return NULL;
		}

		if (need_to_wait && (timeout <= 0)) {
			css_info("zsl not reached yet %d\n", find_frame);
			return NULL;
		}

		spin_lock_irqsave(&bufq->lock, flags);
		list_for_each_entry_safe_reverse(tmp_buffer, n, &bufq->head, list) {

			if (tmp_buffer->frame_cnt < find_frame)
				continue;

			list_del(&tmp_buffer->list);
			tmp_buffer->state = state;
			break;
		}
		spin_unlock_irqrestore(&bufq->lock, flags);
		return tmp_buffer;
	}
}

static
struct css_buffer *bayer_buffer_pop_from_top(struct css_bufq *bufq, int state)
{
	struct css_buffer *tmp_buffer;
	unsigned long flags;

	spin_lock_irqsave(&bufq->lock, flags);

	if (list_empty(&bufq->head)) {
		spin_unlock_irqrestore(&bufq->lock, flags);
		/* css_warn("frame is empty\n"); */
		return NULL;
	} else {
		tmp_buffer = list_first_entry(&bufq->head, struct css_buffer, list);
		list_del(&tmp_buffer->list);
	}

	tmp_buffer->state = state;
	spin_unlock_irqrestore(&bufq->lock, flags);

	return tmp_buffer;
}

static struct css_buffer *bayer_buffer_pop_from_bottom(struct css_bufq *bufq,
								int state)
{
	struct css_buffer *tmp_buffer, *n;
	struct list_head *count;
	unsigned long flags;

	spin_lock_irqsave(&bufq->lock, flags);

	if (list_empty(&bufq->head)) {
		spin_unlock_irqrestore(&bufq->lock, flags);
		/* css_warn("frame is empty\n"); */
		return NULL;
	} else {
		list_for_each_safe(count, n, &bufq->head) {
			tmp_buffer = list_entry(count, struct css_buffer, list);
		}
		list_del(&tmp_buffer->list);
	}

	tmp_buffer->state = state;

	spin_unlock_irqrestore(&bufq->lock, flags);

	return tmp_buffer;
}

static struct css_buffer *bayer_buffer_drop_frame(struct css_bufq *bufq, int i)
{
	struct css_buffer *tmp_buffer;
	struct list_head *count, *n;
	unsigned long flags;

	spin_lock_irqsave(&bufq->lock, flags);

	if (list_empty(&bufq->head)) {
		spin_unlock_irqrestore(&bufq->lock, flags);
		/* css_warn("frame is empty\n"); */
		return NULL;
	} else {
		list_for_each_safe(count, n, &bufq->head) {
			tmp_buffer = list_entry(count, struct css_buffer, list);
			if (tmp_buffer->id == i) {
				list_del(&tmp_buffer->list);
				tmp_buffer->state = CSS_BUF_UNUSED;
				break;
			}
		}
	}

	spin_unlock_irqrestore(&bufq->lock, flags);

	return tmp_buffer;
}

static inline int bayer_buffer_get_capture_index(struct css_bufq *bufq)
{
	int i = 0;
	int buf_index = 0;

	for (i = 1; i < bufq->count; i++) {
		if (bufq->bufs[i].state == CSS_BUF_CAPTURING) {
			buf_index = i;
			break;
		}
	}

	return buf_index;
}

static int bayer_buffer_get_next_index(struct css_zsl_device *zsl_dev,
			struct css_bufq *bufq, int current_index)
{
	int next_capture_index = 0;
	zsl_store_mode strmode = ZSL_STORE_STREAM_MODE;

	strmode = zsl_dev->storemode;
	zsl_dev->frame_cnt++;

	if (zsl_dev->frame_interval == 1) {
		int i = 0;
		int buf_full = 1;
		int buf_idx = 0;

		if (current_index) {
			zsl_dev->frame_pos = current_index;
			css_zsl_low("zsl[%d] push : index %d(%ld)\n", zsl_dev->path,
					current_index, bufq->bufs[current_index].frame_cnt);

			bayer_buffer_push(bufq, current_index);
		}

		for (i = zsl_dev->frame_pos; i < bufq->count; i++) {
			if (bufq->bufs[i].state == CSS_BUF_UNUSED &&
				i != zsl_dev->frame_pos) {
				buf_idx = i;
				buf_full = 0;
				break;
			}
		}

		if (buf_full) {
			for (i = 1; i < zsl_dev->frame_pos; i++) {
				if (bufq->bufs[i].state == CSS_BUF_UNUSED &&
					i != zsl_dev->frame_pos) {
					buf_idx = i;
					buf_full = 0;
					break;
				}
			}
		}

		if (zsl_dev->store_full.done > 0)
			buf_full = 1;

		if (buf_full && zsl_dev->storemode == ZSL_STORE_FILL_BUFFER_MODE) {
			next_capture_index = 0;
			zsl_dev->frame_cnt = 0;
			complete(&zsl_dev->store_full);
			return next_capture_index;
		}

		if (buf_full) {
			struct css_buffer *drop_buf = NULL;
			drop_buf = bayer_buffer_pop_from_bottom(bufq, CSS_BUF_UNUSED);
			if (drop_buf) {
				next_capture_index = drop_buf->id;
			} else {
				next_capture_index = (zsl_dev->frame_pos + 1);

				if (next_capture_index >= bufq->count) {
					zsl_dev->frame_pos = next_capture_index = 1;
				}

				buf_idx = next_capture_index;

				while (bufq->bufs[next_capture_index].state == CSS_BUF_QUEUED) {
					next_capture_index++;

					if (next_capture_index >= bufq->count) {
						zsl_dev->frame_pos = next_capture_index = 1;
					}

					if (buf_idx == next_capture_index)
						break;
				}
				bayer_buffer_drop_frame(bufq, next_capture_index);
			}
		} else {
			next_capture_index = buf_idx;
		}

		zsl_dev->frame_cnt = 0;

	} else {

		if ((zsl_dev->frame_cnt % zsl_dev->frame_interval) == 1) {
#ifdef ODIN_ZSL_STORE_TIME_ESTIMATE
			unsigned long long duration = 0;
			unsigned long expect_time = 0;
			zsl_dev->pts = zsl_dev->cts;
			zsl_dev->pfrm = zsl_dev->cfrm;
			zsl_dev->cts = ktime_get();
			zsl_dev->cfrm = bufq->bufs[current_index].frame_cnt;
			delta = ktime_sub(zsl_dev->cts, zsl_dev->pts);
			duration = (unsigned long long) ktime_to_ns(delta) >> 10;

			expect_time = (zsl_dev->frame_interval * FPS_TO_MS(25));

			if (zsl_dev->exp_frame == 0) {
				zsl_dev->exp_frame = zsl_dev->frame_interval;
			}

			if (zsl_dev->frame_interval) {
				int frmcnt = 0;

				if (zsl_dev->pfrm == 0)
					zsl_dev->pfrm = zsl_dev->cfrm;

				frmcnt = (zsl_dev->cfrm - zsl_dev->pfrm);

				if (frmcnt <= 0)
					frmcnt = zsl_dev->frame_interval;

				zsl_dev->frame_per_ms =
					((unsigned long)duration)/frmcnt;
				zsl_dev->frame_per_ms /= 1000;
			} else {
				zsl_dev->frame_per_ms = 0;
			}

			if (zsl_dev->frame_per_ms)
				zsl_dev->exp_frame = (expect_time/zsl_dev->frame_per_ms);
			else
				zsl_dev->exp_frame = 0;
#endif
			if (current_index) {
				zsl_dev->frame_pos = current_index;
				css_zsl_low("zsl[%d] push : index %d(%ld)\n", zsl_dev->path,
						current_index, bufq->bufs[current_index].frame_cnt);
				bayer_buffer_push(bufq, current_index);
			}
		}

#ifdef ODIN_ZSL_STORE_TIME_ESTIMATE
		if (zsl_dev->frame_interval >= 2) {
			if (zsl_dev->exp_frame) {
				if (zsl_dev->frame_interval > zsl_dev->exp_frame) {
					if (zsl_dev->frame_cnt == zsl_dev->exp_frame)
						zsl_dev->frame_cnt = zsl_dev->frame_interval;
				}
			}
		}
#endif

		if (((zsl_dev->frame_cnt+1) % zsl_dev->frame_interval) == 1) {
			int i = 0;
			int buf_full = 1;
			int buf_idx = 0;

			for (i = zsl_dev->frame_pos; i < bufq->count; i++) {
				if (bufq->bufs[i].state == CSS_BUF_UNUSED &&
					i != zsl_dev->frame_pos) {
					buf_idx = i;
					buf_full = 0;
					break;
				}
			}

			if (buf_full) {
				for (i = 1; i < zsl_dev->frame_pos; i++) {
					if (bufq->bufs[i].state == CSS_BUF_UNUSED &&
						i != zsl_dev->frame_pos) {
						buf_idx = i;
						buf_full = 0;
						break;
					}
				}
			}

			if (buf_full && zsl_dev->storemode == ZSL_STORE_FILL_BUFFER_MODE)
			{
				next_capture_index = 0;
				zsl_dev->frame_cnt = 0;
				complete(&zsl_dev->store_full);
				return next_capture_index;
			}

			if (buf_full) {
				struct css_buffer *drop_buf = NULL;
				drop_buf = bayer_buffer_pop_from_bottom(bufq, CSS_BUF_UNUSED);
				if (drop_buf) {
					next_capture_index = drop_buf->id;
				} else {
					next_capture_index = (zsl_dev->frame_pos + 1);

					if (next_capture_index >= bufq->count) {
						zsl_dev->frame_pos = next_capture_index = 1;
					}

					buf_idx = next_capture_index;

					while (bufq->bufs[next_capture_index].state
							== CSS_BUF_QUEUED) {
						next_capture_index++;

						if (next_capture_index >= bufq->count) {
							zsl_dev->frame_pos = next_capture_index = 1;
						}

						if (buf_idx == next_capture_index)
							break;
					}

					bayer_buffer_drop_frame(bufq, next_capture_index);
				}
			} else {
				next_capture_index = buf_idx;
			}
		} else {
			next_capture_index = 0;
		}

		if (zsl_dev->frame_cnt >= zsl_dev->frame_interval) {
			zsl_dev->frame_cnt = 0;
		}
	}

	return next_capture_index;
}

int bayer_buffer_alloc(struct capture_fh *capfh)
{
	int ret = CSS_SUCCESS;
	struct css_device *cssdev = NULL;
	struct css_zsl_device *zsl_dev	= NULL;
	struct css_bufq *bufq = NULL;
	int i;

	if (capfh->main_scaler > CSS_SCALER_1) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	cssdev = capfh->cssdev;
	zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);
	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}

	if (zsl_dev->buf_count <= 0) {
		css_err("zsl[%d] buffer count = %d!\n",
				zsl_dev->path, zsl_dev->buf_count);
		return CSS_ERROR_ZSL_BUF_COUNT;
	}

	if (zsl_dev->buf_size <= 0) {
		css_err("zsl[%d] buffer size = %d!\n",
				zsl_dev->path, zsl_dev->buf_size);
		return CSS_ERROR_ZSL_BUF_SIZE;
	}

	bufq = &zsl_dev->raw_buf;
	bufq->error = 0;

	if (bufq->ready == false) {
		css_info("zsl[%d] buffer alloc size = %d (%d)\n", capfh->main_scaler,
					zsl_dev->buf_size, zsl_dev->buf_count);
		if (zsl_dev->buf_size) {
			unsigned int buf_unit_size = 0;

			init_waitqueue_head(&bufq->complete.wait);
			spin_lock_init(&bufq->lock);
			INIT_LIST_HEAD(&bufq->head);

			bufq->bufs = kzalloc(sizeof(struct css_buffer) *
								zsl_dev->buf_count, GFP_KERNEL);

			if (bufq->bufs == NULL) {
				css_err("zsl[%d] buffer alloc fail!\n", zsl_dev->path)
				return CSS_FAILURE;
			}

			buf_unit_size = zsl_dev->buf_size/zsl_dev->buf_count;
			if (cssdev->rsvd_mem) {
				for (i = 0; i < zsl_dev->buf_count; i++) {
					ret = odin_css_physical_buffer_alloc(cssdev->rsvd_mem,
										&bufq->bufs[i], buf_unit_size);
					if (ret == CSS_SUCCESS) {
						bufq->bufs[i].id		= i;
						bufq->bufs[i].state		= CSS_BUF_UNUSED;
						bufq->bufs[i].byteused	= buf_unit_size;
						bufq->bufs[i].offset	= buf_unit_size >> 1;
						/*memset(bufq->bufs[i].cpu_addr, 0x0, buf_unit_size);*/
						css_zsl("zsl[%d] buffer[%d] 0x%08X-%d\n", zsl_dev->path,
								i,(unsigned int)bufq->bufs[i].dma_addr,
								buf_unit_size);
					} else {
						odin_css_physical_buffer_free(cssdev->rsvd_mem,
										&bufq->bufs[i]);
						css_err("zsl[%d] buffer alloc fail index is %d!\n",
								zsl_dev->path, i);
						return CSS_FAILURE;
					}
				}
			} else {
#ifdef CONFIG_ODIN_ION_SMMU
				unsigned long iova = 0;
				unsigned long aligned_size = 0;
				ktime_t delta, stime, etime;
				unsigned long long duration;

				stime = ktime_get();

				zsl_dev->ion_client = odin_ion_client_create("zsl_ion");
				if (zsl_dev->ion_client) {
					for (i=0; i < zsl_dev->buf_count; i++) {
						if (i == 0)
							aligned_size = ion_align_size(buf_unit_size >> 4);
						else {
							if (zsl_dev->buf_width <= 2104 &&
								zsl_dev->buf_height <= 1568) {
								aligned_size = ion_align_size(buf_unit_size) +
												SZ_2M;
							} else {
								aligned_size = ion_align_size(buf_unit_size);
							}
						}

						bufq->bufs[i].ion_hdl.handle
							= ion_alloc(zsl_dev->ion_client, aligned_size,
									0x1000, (1<<ODIN_ION_SYSTEM_HEAP1),
									0, ODIN_SUBSYS_CSS);
						if (IS_ERR_OR_NULL(bufq->bufs[i].ion_hdl.handle)) {
							for (i = 0; i < zsl_dev->buf_count; i++) {
								if (bufq->bufs[i].ion_hdl.handle)
									bufq->bufs[i].ion_hdl.handle = NULL;
							}
							odin_ion_client_destroy(zsl_dev->ion_client);
							zsl_dev->ion_client = NULL;
							if (bufq->bufs) {
								kzfree(bufq->bufs);
								bufq->bufs = NULL;
							}
							css_err("zsl ion get handle fail\n");
							return CSS_FAILURE;
						}

						iova = odin_ion_get_iova_of_buffer(
											bufq->bufs[i].ion_hdl.handle,
											ODIN_SUBSYS_CSS);
						if (!iova) {
							for (i = 0; i < zsl_dev->buf_count; i++) {
								if (bufq->bufs[i].ion_hdl.handle)
									bufq->bufs[i].ion_hdl.handle = NULL;
							}
							odin_ion_client_destroy(zsl_dev->ion_client);
							zsl_dev->ion_client = NULL;
							if (bufq->bufs) {
								kzfree(bufq->bufs);
								bufq->bufs = NULL;
							}
							css_err("zsl ion get iova fail\n");
							return CSS_FAILURE;
						}

						bufq->bufs[i].id = i;
						bufq->bufs[i].state = CSS_BUF_UNUSED;
						bufq->bufs[i].offset = buf_unit_size>>1;
						bufq->bufs[i].byteused = buf_unit_size;
						bufq->bufs[i].dma_addr = iova;
						if (zsl_dev->buf_width <= 2104 &&
							zsl_dev->buf_height <= 1568) {
							if (i == 0)
								bufq->bufs[i].cpu_addr = NULL;
							else {
								bufq->bufs[i].cpu_addr =
								(void *) odin_ion_map_kernel_with_condition(
										bufq->bufs[i].ion_hdl.handle,
										buf_unit_size,
										PAGE_ALIGN(CSS_SENSOR_STATISTIC_SIZE));
								css_zsl("cpu addr[%d]: 0x%x(0x%08x)\n", i,
										bufq->bufs[i].cpu_addr,
										PAGE_ALIGN(CSS_SENSOR_STATISTIC_SIZE));
							}
						} else {
							bufq->bufs[i].cpu_addr = NULL;
						}
						bufq->bufs[i].size = aligned_size;
					}
				} else {
					if (bufq->bufs) {
						kzfree(bufq->bufs);
						bufq->bufs = NULL;
					}
					css_err("zsl ion client create fail\n");
					return CSS_FAILURE;
				}

				etime = ktime_get();
				delta = ktime_sub(etime, stime);
				duration = (unsigned long long) ktime_to_ns(delta) >> 10;

				css_info("zsl buffer alloc duration: %lld usecs\n", duration);
#else
				css_err("not supported ion\n");
				return CSS_FAILURE;
#endif
			}
			bufq->count = zsl_dev->buf_count;
		}
		bufq->ready = true;
	}

	return ret;
}

int bayer_buffer_free(struct capture_fh *capfh)
{
	int ret = CSS_SUCCESS;
	struct css_device *cssdev = NULL;
	struct css_zsl_device *zsl_dev = NULL;
	struct css_bufq *bufq = NULL;
	int i;

	if (capfh->main_scaler > CSS_SCALER_1) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	cssdev = capfh->cssdev;
	zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);

	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}

	bufq = &zsl_dev->raw_buf;

	if (bufq->ready == true) {
		css_info("zsl[%d] buffer free\n", capfh->main_scaler);
		if (cssdev->rsvd_mem) {
			for (i = 0; i < zsl_dev->buf_count; i++) {
				odin_css_physical_buffer_free(cssdev->rsvd_mem,
								&bufq->bufs[i]);
			}
		} else {
#ifdef CONFIG_ODIN_ION_SMMU
			if (bufq->bufs) {
				for (i = 0; i < zsl_dev->buf_count; i++) {
					if (bufq->bufs[i].ion_hdl.handle) {
						bufq->bufs[i].id = -1;
						bufq->bufs[i].state = CSS_BUF_UNUSED;
						bufq->bufs[i].offset = 0;
						bufq->bufs[i].byteused = 0;
						bufq->bufs[i].dma_addr = 0;
						if (bufq->bufs[i].cpu_addr) {
							odin_ion_unmap_kernel_with_condition(
								bufq->bufs[i].ion_hdl.handle);
							css_zsl("cpu addr deinit[%d]: 0x%x\n", i,
									bufq->bufs[i].cpu_addr);
							bufq->bufs[i].cpu_addr = NULL;
						}
						bufq->bufs[i].size = 0;
						bufq->bufs[i].ion_hdl.handle = NULL;
					}
				}
			}
			if (zsl_dev->ion_client) {
				odin_ion_client_destroy(zsl_dev->ion_client);
				zsl_dev->ion_client = NULL;
			}
#else
			css_err("not supported ion\n");
			return CSS_FAILURE;
#endif
		}

		if (bufq->bufs) {
			kzfree(bufq->bufs);
			bufq->bufs = NULL;
		}

		bufq->count = 0;
		bufq->ready = false;
	}

	zsl_dev->buf_width = 0;
	zsl_dev->buf_height = 0;
	zsl_dev->buf_size = 0;
	zsl_dev->buf_count = 0;

	return ret;
}

int bayer_buffer_flush(struct capture_fh *capfh)
{
	int ret = CSS_SUCCESS;
	struct css_zsl_device *zsl_dev = NULL;
	struct css_bufq *bufq = NULL;
	struct css_buffer *cssbuf = NULL;
	int i, buf_cnt = 0;
	unsigned long flags;

	if (capfh->main_scaler > CSS_SCALER_1) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);
	bufq = &zsl_dev->raw_buf;

	if (!bufq->ready || bufq->bufs == NULL) {
		css_err("zsl buf not ready %d (%d:0x%x)!!\n", zsl_dev->path, bufq->ready,
				bufq->bufs);
		return CSS_ERROR_ZSL_BUF_READY;
	}

	buf_cnt = bufq->count;

	cssbuf = bayer_buffer_pop_frame(bufq, CSS_BUF_POP_TOP);
	buf_cnt--;

	if (cssbuf != NULL) {
		css_zsl("zsl[%d] flush buf[%d]-0x%08X!\n", zsl_dev->path, cssbuf->id,
			(unsigned int)cssbuf->dma_addr);
	}

	while (cssbuf != NULL && buf_cnt > 0) {
		cssbuf = bayer_buffer_pop_frame(bufq, CSS_BUF_POP_TOP);
		if (cssbuf != NULL) {
			css_zsl("zsl[%d] flush buf[%d]-0x%08X!\n", zsl_dev->path,
				cssbuf->id, (unsigned int)cssbuf->dma_addr);
		}
		buf_cnt--;
	}

	spin_lock_irqsave(&bufq->lock, flags);
	bufq->index = 0;
	bufq->q_pos = 0;
	for (i = 0; i < bufq->count; i++) {
		bufq->bufs[i].state = CSS_BUF_UNUSED;
	}
	spin_unlock_irqrestore(&bufq->lock, flags);

	return ret;
}

int bayer_buffer_push(struct css_bufq *bufq, int i)
{
	struct css_buffer *tmp_buffer;
	struct list_head *count, *n;
	unsigned long flags;

	spin_lock_irqsave(&bufq->lock, flags);
	list_for_each_safe(count, n, &bufq->head) {
		tmp_buffer = list_entry(count, struct css_buffer, list);
		if (tmp_buffer->id == i) {
			/* css_zsl("frame[%d] is already exist! deleted!!\n", i); */
			list_del(&tmp_buffer->list);
			break;
		}
	}

	bufq->bufs[i].id = i;
	bufq->bufs[i].state = CSS_BUF_CAPTURE_DONE;
	list_add(&bufq->bufs[i].list, &bufq->head);
	spin_unlock_irqrestore(&bufq->lock, flags);
	return 0;
}

struct css_buffer *bayer_buffer_get_frame(
			struct css_bufq *bufq, css_buffer_pop_types type)
{
	unsigned long flags;

	if (bufq)
		if (type == CSS_BUF_POP_TOP)
			return bayer_buffer_pop_from_top(bufq, CSS_BUF_QUEUED);
		else
			return bayer_buffer_pop_from_bottom(bufq, CSS_BUF_QUEUED);
	else
		return NULL;
}

void bayer_buffer_release_frame(struct css_bufq *bufq,
								struct css_buffer *cssbuf)
{
	unsigned long flags;

	if (bufq && cssbuf) {
		spin_lock_irqsave(&bufq->lock, flags);
		if (cssbuf->state == CSS_BUF_QUEUED)
			cssbuf->state = CSS_BUF_UNUSED;
		spin_unlock_irqrestore(&bufq->lock, flags);
	}

	return;
}

struct css_buffer *bayer_buffer_pop_frame(struct css_bufq *bufq,
							css_buffer_pop_types type)
{
	if (type == CSS_BUF_POP_TOP)
		return bayer_buffer_pop_from_top(bufq, CSS_BUF_UNUSED);
	else
		return bayer_buffer_pop_from_bottom(bufq, CSS_BUF_UNUSED);
}

int bayer_buffer_get_queued_count(struct css_bufq *bufq)
{
	struct css_buffer *tmp_buffer;
	struct list_head *count, *n;
	unsigned long flags;
	int queued_cnt = 0;

	spin_lock_irqsave(&bufq->lock, flags);
	if (!list_empty(&bufq->head)) {
		list_for_each_safe(count, n, &bufq->head) {
			tmp_buffer = list_entry(count, struct css_buffer, list);
			queued_cnt++;
		}
	}
	spin_unlock_irqrestore(&bufq->lock, flags);

	return queued_cnt;
}

#ifdef CONFIG_VIDEO_ODIN_S3DC
int bayer_stereo_store_intr_handler(struct css_device *cssdev,
			unsigned int int_status)
{
	int ret = CSS_SUCCESS;

#ifdef CONFIG_VIDEO_ODIN_S3DC
	struct css_bufq *bufq = NULL;
	struct css_zsl_device *zsl_dev = NULL;
	struct css_zsl_config *zsl_config = NULL;
	int capture_index = 0;
	int next_capture_index = 0;

	zsl_dev = &cssdev->zsl_device[0];
	zsl_config = &zsl_dev->config;

	if (int_status & INT_PATH_RAW0_STORE) {
		scaler_int_disable(INT_PATH_RAW0_STORE);
		scaler_int_clear(INT_PATH_RAW0_STORE);
		if (zsl_config->mem.base32st[0].filled) {
			css_zsl("zsl warn : already 0 filled\n");
		}
		zsl_config->mem.base32st[0].filled = 1;
	}

	if (int_status & INT_PATH_RAW1_STORE) {
		scaler_int_disable(INT_PATH_RAW1_STORE);
		scaler_int_clear(INT_PATH_RAW1_STORE);
		if (zsl_config->mem.base32st[1].filled) {
			css_zsl("zsl warn : already 1 filled\n");
		}
		zsl_config->mem.base32st[1].filled = 1;
	}

	if (zsl_config->mem.base32st[0].filled == 1 &&
		zsl_config->mem.base32st[1].filled == 1) {

		if (zsl_config->enable == 0) {
			complete(&zsl_dev->store_done);
		}

		bufq = &zsl_dev->raw_buf;

		capture_index = bayer_buffer_get_capture_index(bufq);

		bufq->bufs[capture_index].frame_cnt = scaler_get_frame_num(0);

		next_capture_index = bayer_buffer_get_next_index(zsl_dev, bufq,
														capture_index);

		css_zsl_low("zsl[%d] %d->%d\n",
					zsl_dev->path, capture_index, next_capture_index);

		/* start next zsl frame */
		bufq->bufs[next_capture_index].state = CSS_BUF_CAPTURING;
		zsl_config->mem.base32st[0].base32
							= bufq->bufs[next_capture_index].dma_addr;
		zsl_config->mem.base32st[1].base32
							= (u32)zsl_config->mem.base32st[0].base32
							+ bufq->bufs[next_capture_index].offset;

		zsl_config->mem.base32st[0].filled = 0;
		zsl_config->mem.base32st[1].filled = 0;

		ret = scaler_bayer_store_configure(zsl_dev->path, zsl_config);
	}

	if (ret == CSS_SUCCESS)
		ret = scaler_bayer_store_capture(zsl_dev->path, zsl_config);
#else
	ret = CSS_ERROR_UNSUPPORTED;
#endif
	return ret;
}

static int bayer_stereo_store_enable(struct capture_fh *capfh)
{
	int ret = CSS_SUCCESS;
	struct css_sensor_device *cam_dev = NULL;
	struct css_zsl_device *zsl_dev = NULL;
	struct css_bufq *bufq = NULL;
	struct css_zsl_config *zsl_config = NULL;
	unsigned int scl_path_reset = 0;
	unsigned int scl_clk_control = 0;
	struct v4l2_mbus_framefmt m_pix = {0,};

	if (capfh->main_scaler != CSS_SCALER_0) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	cam_dev = capfh->cam_dev;
	zsl_dev = capture_get_zsl_dev(capfh, CSS_SCALER_0);
	zsl_config = &zsl_dev->config;

	if (cam_dev->cur_sensor == NULL) {
		css_err("zsl[%d] input info error!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_CURRENT_INPUT;
	}

	zsl_dev->path	= ZSL_STORE_PATH_ALL;
	switch (zsl_dev->bitfmt) {
	case RAWDATA_BIT_MODE_12BIT:
		zsl_config->bit_mode = CSS_RAWDATA_BIT_MODE_12BIT;
		break;
	case RAWDATA_BIT_MODE_10BIT:
		zsl_config->bit_mode = CSS_RAWDATA_BIT_MODE_10BIT;
		break;
	case RAWDATA_BIT_MODE_8BIT:
		zsl_config->bit_mode = CSS_RAWDATA_BIT_MODE_8BIT;
		break;
	default:
		zsl_config->bit_mode = CSS_RAWDATA_BIT_MODE_12BIT;
		break;
	}

	scl_path_reset	= PATH_RAW0_ST_RESET | RAW0_ST_CTRL_RESET |
						PATH_RAW1_ST_RESET | RAW1_ST_CTRL_RESET;
	scl_clk_control = PATH_RAW0_ST_CLK | RAW0_ST_CTRL_CLK |
						PATH_RAW1_ST_CLK | RAW1_ST_CTRL_CLK;

	ret = v4l2_subdev_call(cam_dev->cur_sensor->sd, video, g_mbus_fmt, &m_pix);
	if (ret < 0) {
		css_err("get sensor size fail = %d\n", ret);
		return ret;
	}

	scaler_path_reset(scl_path_reset);
	scaler_path_clk_control(scl_clk_control, CLOCK_ON);

	bufq = &zsl_dev->raw_buf;
	bufq->index = 1;
	bufq->bufs[bufq->index].state = CSS_BUF_CAPTURING;

	zsl_config->img_size.width	= m_pix.width;
	zsl_config->img_size.height = m_pix.height;
	zsl_config->mem.base32st[0].filled = 0;
	zsl_config->mem.base32st[1].filled = 0;
	zsl_config->mem.base32st[0].base32 = bufq->bufs[bufq->index].dma_addr;
	zsl_config->mem.base32st[1].base32
							= (u32)zsl_config->mem.base32st[0].base32
							+ bufq->bufs[bufq->index].offset;

	ret = scaler_bayer_store_enable(zsl_dev->path, zsl_config);

	css_zsl("zsl[%d] enabled = %d!!\n", zsl_dev->path, ret);

	if (ret != CSS_SUCCESS)
		scaler_path_clk_control(scl_clk_control, CLOCK_OFF);

	return ret;
}

static int bayer_stereo_store_pause(struct capture_fh *capfh)
{
	int ret = CSS_SUCCESS;
	int timeout = 0;
	struct css_zsl_device *zsl_dev = NULL;
	struct css_zsl_config *zsl_config = NULL;

	if (capfh->main_scaler != CSS_SCALER_0) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	zsl_dev = capture_get_zsl_dev(capfh, CSS_SCALER_0);
	zsl_config = &zsl_dev->config;

	ret = scaler_bayer_store_disable(zsl_dev->path, zsl_config);
	if (ret == CSS_SUCCESS) {
		INIT_COMPLETION(zsl_dev->store_done);
		timeout = wait_for_completion_timeout(&zsl_dev->store_done,
											msecs_to_jiffies(100));

		if (timeout == 0) {
			css_info("zsl[%d] store done timeout!!\n", zsl_dev->path);
		}
	}

	scaler_int_disable(INT_PATH_RAW0_STORE | INT_PATH_RAW1_STORE);

	css_zsl("zsl[%d] paused = %d!!\n", zsl_dev->path, ret);

	return ret;
}

static int bayer_stereo_store_resume(struct capture_fh *capfh)
{
	int ret = CSS_SUCCESS;
	struct css_sensor_device *cam_dev = NULL;
	struct css_zsl_device *zsl_dev = NULL;
	struct css_bufq *bufq = NULL;
	struct css_zsl_config *zsl_config = NULL;
	struct v4l2_mbus_framefmt m_pix = {0,};

	if (capfh->main_scaler != CSS_SCALER_0) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	cam_dev = capfh->cam_dev;
	zsl_dev = capture_get_zsl_dev(capfh, CSS_SCALER_0);
	zsl_config = &zsl_dev->config;

	if (cam_dev->cur_sensor == NULL) {
		css_err("zsl[%d] input info error!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_CURRENT_INPUT;
	}

	ret = v4l2_subdev_call(cam_dev->cur_sensor->sd, video, g_mbus_fmt, &m_pix);
	if (ret < 0) {
		css_err("get sensor size fail = %d\n", ret);
		return ret;
	}

	switch (zsl_dev->bitfmt) {
	case RAWDATA_BIT_MODE_12BIT:
		zsl_config->bit_mode = CSS_RAWDATA_BIT_MODE_12BIT;
		break;
	case RAWDATA_BIT_MODE_10BIT:
		zsl_config->bit_mode = CSS_RAWDATA_BIT_MODE_10BIT;
		break;
	case RAWDATA_BIT_MODE_8BIT:
		zsl_config->bit_mode = CSS_RAWDATA_BIT_MODE_8BIT;
		break;
	default:
		zsl_config->bit_mode = CSS_RAWDATA_BIT_MODE_12BIT;
		break;
	}

	bufq = &zsl_dev->raw_buf;

	zsl_config->img_size.width	= m_pix.width;
	zsl_config->img_size.height = m_pix.height;

	zsl_config->mem.base32st[0].filled = 0;
	zsl_config->mem.base32st[1].filled = 0;
	zsl_config->mem.base32st[0].base32 = bufq->bufs[bufq->index].dma_addr;
	zsl_config->mem.base32st[1].base32
								= (u32)zsl_config->mem.base32st[0].base32
								+ bufq->bufs[bufq->index].offset;

	ret = scaler_bayer_store_enable(zsl_dev->path, zsl_config);

	css_zsl("zsl[%d] resumed = %d!!\n", zsl_dev->path, ret);

	return ret;
}

static int bayer_stereo_store_disable(struct capture_fh *capfh)
{
	int ret = CSS_SUCCESS;
	int timeout = 0;
	struct css_zsl_device *zsl_dev = NULL;
	struct css_zsl_config *zsl_config = NULL;
	unsigned int scl_clk_control = 0;
	unsigned int scl_int_control = 0;

	if (capfh->main_scaler != CSS_SCALER_0) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	zsl_dev = capture_get_zsl_dev(capfh, CSS_SCALER_0);
	zsl_config = &zsl_dev->config;

	ret = scaler_bayer_store_disable(zsl_dev->path, zsl_config);

	if (ret == CSS_SUCCESS) {
		INIT_COMPLETION(zsl_dev->store_done);
		timeout = wait_for_completion_timeout(&zsl_dev->store_done,
											msecs_to_jiffies(100));

		if (timeout == 0) {
			css_info("zsl[%d] store done timeout!!\n", zsl_dev->path);
		}
		bayer_buffer_flush(capfh);
	}

	scl_clk_control = PATH_RAW0_ST_CLK | RAW0_ST_CTRL_CLK |
						PATH_RAW1_ST_CLK | RAW1_ST_CTRL_CLK;
	scl_int_control = INT_PATH_RAW0_STORE | INT_PATH_RAW1_STORE;

	scaler_path_clk_control(scl_clk_control, CLOCK_OFF);

	scaler_int_disable(scl_int_control);
	scaler_int_clear(scl_int_control);

	css_info("zsl[%d] disabled = %d!!\n", zsl_dev->path, ret);

	return ret;
}

static int bayer_stereo_store_mode(struct capture_fh *capfh,
			zsl_store_mode mode)
{
	int ret = CSS_SUCCESS;

	struct css_zsl_device *zsl_dev = NULL;

	if (capfh->main_scaler != CSS_SCALER_0) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	zsl_dev = capture_get_zsl_dev(capfh, CSS_SCALER_0);
	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}

	if (zsl_dev->config.enable) {
		css_warn("zsl[%d] set store mode err: busy!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEV_BUSY;
	}

	zsl_dev->storemode = mode;

	css_info("zsl[%d] set store mode = %d!!\n", zsl_dev->path, mode);

	return ret;
}

static int bayer_stereo_store_frame_interval(struct capture_fh *capfh,
			unsigned int interval)
{
	int ret = CSS_SUCCESS;
	struct css_zsl_device *zsl_dev = NULL;

	if (capfh->main_scaler != CSS_SCALER_0) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	zsl_dev = capture_get_zsl_dev(capfh, CSS_SCALER_0);
	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}

	if (interval <= 0) interval = 1;
	if (interval > 30) interval = 30;
	zsl_dev->frame_interval = interval;

	css_zsl("zsl[%d] set frame interval = %d!!\n", zsl_dev->path, interval);

	return ret;
}

static int bayer_stereo_store_bit_format(struct capture_fh *capfh,
			zsl_store_raw_bitfmt fmt)
{
	int ret = CSS_SUCCESS;

	struct css_zsl_device *zsl_dev = NULL;

	if (capfh->main_scaler != CSS_SCALER_0) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	zsl_dev = capture_get_zsl_dev(capfh, CSS_SCALER_0);

	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}

	if (zsl_dev->config.enable) {
		css_warn("zsl[%d] set store bit format err: busy!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEV_BUSY;
	}

	zsl_dev->bitfmt = fmt;
	css_info("zsl[%d] set store bit format = %d!!\n", zsl_dev->path, fmt);

	return ret;
}

static int bayer_stereo_store_wait_buffer_full(struct capture_fh *capfh)
{
	int ret = CSS_SUCCESS;
	int timeout = 0;
	int waittime = 0;
	int fps = 0;
	int frmintv = 0;
	struct css_zsl_device *zsl_dev = NULL;

	if (capfh->main_scaler != CSS_SCALER_0) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	zsl_dev = capture_get_zsl_dev(capfh, CSS_SCALER_0);
	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}

	if (!zsl_dev->config.enable) {
		css_err("zsl[%d] not enabled\n", zsl_dev->path);
		return CSS_ERROR_ZSL_NOT_ENABLED;
	}

	if (zsl_dev->storemode == ZSL_STORE_STREAM_MODE) {
		css_err("zsl[%d] not fill buffer mode!!\n", zsl_dev->path);
		return CSS_ERROR_INVALID_ARG;
	}

	if (zsl_dev->frame_interval)
		frmintv = zsl_dev->frame_interval;
	else
		frmintv = 1;

	fps = 8 / frmintv;

	waittime = FPS_TO_MS(fps) * zsl_dev->buf_count;

	INIT_COMPLETION(zsl_dev->store_full);

	timeout = wait_for_completion_timeout(
			&zsl_dev->store_full,
			msecs_to_jiffies(waittime));

	if (timeout == 0) {
		ret = CSS_ERROR_ZSL_TIMEOUT;
		css_info("zsl[%d] store full timeout(%d)!!\n", zsl_dev->path, waittime);
	}

	return ret;
}

static int bayer_stereo_store_buf_index(struct capture_fh *capfh,
			unsigned int idx)
{
	int ret = CSS_SUCCESS;
	struct css_zsl_device *zsl_dev = NULL;

	zsl_dev = capture_get_zsl_dev(capfh, CSS_SCALER_0);
	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}

	if (idx > zsl_dev->buf_count)
		idx = 0;

	zsl_dev->loadbuf_idx = idx;

	css_zsl("zsl[%d] set load buf offset = %d!!\n", zsl_dev->path, idx);

	return ret;
}

static int bayer_stereo_store_buf_order(struct capture_fh *capfh,
			unsigned int order)
{
	int ret = CSS_SUCCESS;
	struct css_zsl_device *zsl_dev = NULL;

	zsl_dev = capture_get_zsl_dev(capfh, CSS_SCALER_0);
	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}

	if (order > ZSL_LOAD_ASCENDING)
		order = ZSL_LOAD_ASCENDING;

	zsl_dev->loadbuf_order = order;

	css_zsl("zsl[%d] set load buf offset = %d!!\n", zsl_dev->path, order);

	return ret;
}

static int bayer_stereo_load_frame_number(struct capture_fh *capfh, int num)
{
	int ret = CSS_SUCCESS;
	struct css_zsl_device *zsl_dev = NULL;

	if (capfh->zslstorefh) {
		css_err("not allowed to store fh!!\n");
		return CSS_ERROR_ZSL_NOT_ALLOWED_STORE_FH;
	}

	if (capfh->cssdev == NULL) {
		css_err("cssdev is null\n");
		return CSS_ERROR_DEV_HANDLE;
	}

	zsl_dev = capture_get_zsl_dev(capfh, CSS_SCALER_0);
	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}

	mutex_lock(&capfh->cssdev->bayer_mutex);

   zsl_dev->loadtype = ZSL_LOAD_ONESHOT_TYPE;

	if (num <= 0)
		zsl_dev->setloadframe = 0;
	else
		zsl_dev->setloadframe = num;

	css_info("zsl[%d] set load frame num: %d!!\n", zsl_dev->path, num);

	mutex_unlock(&capfh->cssdev->bayer_mutex);

	return CSS_SUCCESS;
}

static int bayer_stereo_load_mode(struct capture_fh *capfh, zsl_load_mode mode)
{
	int ret = CSS_SUCCESS;
	struct css_zsl_device *zsl_dev = NULL;

	if (capfh->zslstorefh) {
		css_err("not allowed to store fh!!\n");
		return CSS_ERROR_ZSL_NOT_ALLOWED_STORE_FH;
	}

	if (capfh->cssdev == NULL) {
		css_err("cssdev is null\n");
		return CSS_ERROR_DEV_HANDLE;
	}

	zsl_dev = capture_get_zsl_dev(capfh, CSS_SCALER_0);
	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}

	if (zsl_dev->load_running) {
		css_err("zsl[%d] is busy!!\n", zsl_dev->path);
		return -EBUSY;
	}

	mutex_lock(&capfh->cssdev->bayer_mutex);
	zsl_dev->loadmode = mode;
	css_info("zsl[%d] set load mode: %d!!\n", zsl_dev->path, mode);
	mutex_unlock(&capfh->cssdev->bayer_mutex);

	return CSS_SUCCESS;
}

static int bayer_stereo_load_schedule(struct capture_fh *load_fh)
{
	int ret = CSS_SUCCESS;
	s3dc_schedule_zsl_load_work();
	return ret;
}

int bayer_stereo_load_frame(struct capture_fh *load_fh)
{
	int ret = CSS_SUCCESS;
	struct css_scaler_device scl_dev;
	struct css_zsl_device *zsl_dev	= NULL;
	struct css_bufq *bayer_bufq = NULL;
	struct css_bufq *yuv_bufq = NULL;
	struct css_buffer *cssbuf = NULL;
	struct css_zsl_config zsl_config;
	int buf_index = 0;
	unsigned int phy_addr = 0;
	css_zsl_path r_path = ZSL_READ_PATH_ALL;
	css_buffer_pop_types ptype = 0;
	unsigned int src_w = 0;
	unsigned int src_h = 0;
	ktime_t delta, stime, etime;
	unsigned long long duration;

	if (load_fh == NULL) {
		css_zsl("load file already closed!!\n");
		return CSS_SUCCESS;
	}

	if (load_fh->cssdev == NULL) {
		css_err("cssdev is null\n");
		return CSS_ERROR_DEV_HANDLE;
	}

	if (capture_get_stream_mode(load_fh) == 0) {
		css_zsl("load file already streamoff!!\n");
		return CSS_SUCCESS;
	}

	mutex_lock(&load_fh->cssdev->bayer_mutex);

	if (load_fh->main_scaler != CSS_SCALER_1) {
		css_err("invalid zsl load scaler number!!\n");
		mutex_unlock(&load_fh->cssdev->bayer_mutex);
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	zsl_dev = capture_get_zsl_dev(load_fh, ZSL_READ_PATH0);

	zsl_dev->load_running = 1;

	if (zsl_dev->loadbuf_order == ZSL_LOAD_DECENDING)
		ptype = CSS_BUF_POP_TOP;
	else
		ptype = CSS_BUF_POP_BOTTOM;

	src_w = zsl_dev->config.img_size.width;
	src_h = zsl_dev->config.img_size.height;

	memset(&scl_dev, 0, sizeof(struct css_scaler_device));
	memset(&zsl_config, 0, sizeof(struct css_zsl_config));

	/* configure load : physical address, size, load format */
	bayer_bufq = &zsl_dev->raw_buf;

	if (zsl_dev->config.enable) {
		if (bayer_buffer_get_queued_count(bayer_bufq) == 0) {
			int timeout = 0;
			int waittime = 0;
			waittime = 2 * 130;
			timeout = bayer_buffer_wait_queue_available(bayer_bufq, waittime);
			css_info("zsl frame wait %dms\n", waittime - timeout);
		}
	}

	if (zsl_dev->loadbuf_idx > 0) {
		unsigned int queued_cnt = 0;
		unsigned int skip_buf	= zsl_dev->loadbuf_idx + 1;

		queued_cnt = bayer_buffer_get_queued_count(bayer_bufq);
		if (skip_buf > queued_cnt) {
			skip_buf = queued_cnt;
		}
		do {
			cssbuf = bayer_buffer_get_frame(bayer_bufq, ptype);
			if (cssbuf == NULL) {
				break;
			}
			if (skip_buf != 1) {
				bayer_buffer_release_frame(bayer_bufq, cssbuf);
			}
			css_info("zsl[%d] pop : index %d(%ld) [%d/%d]\n", zsl_dev->path,
					 cssbuf->id,
					 cssbuf->frame_cnt,
					 queued_cnt,
					 bayer_bufq->count-1);
		} while (--skip_buf > 0);
	} else {
		cssbuf = bayer_buffer_get_frame(bayer_bufq, ptype);
		if (cssbuf != NULL) {
			css_info("zsl[%d] pop : index %d(%ld) [%d/%d]\n", zsl_dev->path,
					 cssbuf->id,
					 cssbuf->frame_cnt,
					 bayer_buffer_get_queued_count(bayer_bufq),
					 bayer_bufq->count-1);
		}
	}

	if (cssbuf == NULL) {
		css_err("zsl[%d] load source empty!!\n", zsl_dev->path);
		zsl_dev->load_running = 0;
		complete(&zsl_dev->load_done);
		mutex_unlock(&load_fh->cssdev->bayer_mutex);
		return CSS_ERROR_ZSL_LOAD_EMPTY;
	}

	/* indicate read memory address */
	zsl_config.mem.base32st[0].base32 = (unsigned int)cssbuf->dma_addr;
	zsl_config.mem.base32st[1].base32 = (unsigned int)cssbuf->dma_addr
										+ cssbuf->offset;
	/* decide read pixel width and line count, read image format */
	zsl_config.img_size.width	= src_w;
	zsl_config.img_size.height	= src_h;
	zsl_config.bit_mode			= zsl_dev->config.bit_mode;
	zsl_config.ld_fmt			= CSS_LOAD_IMG_RAW;
	zsl_config.blank_count		= ZSL_LOAD_3D_BLKCNT_MULTIPLE(src_w);

	if (zsl_config.img_size.width == 0 || zsl_config.img_size.height == 0) {
		css_err("zsl[%d] width = %d height=%d\n",
				zsl_dev->path, zsl_config.img_size.width,
				zsl_config.img_size.height);
		zsl_dev->load_running = 0;
		complete(&zsl_dev->load_done);
		mutex_unlock(&load_fh->cssdev->bayer_mutex);
		return CSS_ERROR_INVALID_SIZE;
	}

	yuv_bufq = &load_fh->fb->capture_bufq;
	buf_index = yuv_bufq->index;

	if (buf_index < 0 || buf_index >= yuv_bufq->count) {
		css_err("buf index error %d\n", buf_index);
		zsl_dev->load_running = 0;
		complete(&zsl_dev->load_done);
		mutex_unlock(&load_fh->cssdev->bayer_mutex);
		return CSS_ERROR_BUFFER_INDEX;
	}

	phy_addr = (unsigned int)yuv_bufq->bufs[buf_index].dma_addr;

	yuv_bufq->bufs[buf_index].frame_cnt = cssbuf->frame_cnt;
	yuv_bufq->bufs[buf_index].state = CSS_BUF_CAPTURING;

	scaler_path_reset(PATH_RAW0_LD_RESET | PATH_RAW1_LD_RESET);
	scaler_path_clk_control(PATH_RAW0_LD_CLK | PATH_RAW1_LD_CLK, CLOCK_ON);

	bayer_isp_read_path_enable(ZSL_READ_PATH_ALL);

	scl_dev.config.src_width	= zsl_config.img_size.width;
	scl_dev.config.src_height	= zsl_config.img_size.height;
	scl_dev.config.dst_width	= load_fh->scl_dev->config.dst_width;
	scl_dev.config.dst_height	= load_fh->scl_dev->config.dst_height;
	scl_dev.config.action		= CSS_SCALER_ACT_CAPTURE_3D;
	scl_dev.config.pixel_fmt	= load_fh->scl_dev->config.pixel_fmt;
	scl_dev.config.crop.start_x	= load_fh->scl_dev->config.crop.start_x;
	scl_dev.config.crop.start_y	= load_fh->scl_dev->config.crop.start_y;
	scl_dev.config.crop.width	= load_fh->scl_dev->config.crop.width;
	scl_dev.config.crop.height	= load_fh->scl_dev->config.crop.height;

	scaler_configure(CSS_SCALER_0, &scl_dev.config, &scl_dev.data);
	scaler_configure(CSS_SCALER_1, &scl_dev.config, &scl_dev.data);

	s3dc_set_framebuffer_address(phy_addr);
	s3dc_int_clear(INT_S3DC_FRAME_END);
	s3dc_int_enable(INT_S3DC_FRAME_END);

	stime = ktime_get();

	ret = scaler_bayer_load_configure(r_path, &zsl_config);

	if (ret == CSS_SUCCESS)
		ret = scaler_bayer_load_frame_for_stereo(1);

	if (ret == CSS_SUCCESS)
		capture_wait_done(PATH_RAW0_LD_DONE | PATH_RAW1_LD_DONE, 500);

	if (ret == CSS_SUCCESS)
		ret = scaler_bayer_load_configure(r_path, &zsl_config);

	if (ret == CSS_SUCCESS)
		ret = scaler_bayer_load_frame_for_stereo(1);

	if (ret == CSS_SUCCESS) {
		int timeout;
		int retry = 20;

		do {
			timeout = s3dc_wait_capture_done(30);
			if (timeout != 0)
				break;
			--retry;
		} while (retry > 0 && capture_get_stream_mode(load_fh));

		if (retry > 0) {
			etime = ktime_get();
			delta = ktime_sub(etime, stime);
			duration = (unsigned long long) ktime_to_ns(delta) >> 10;
			css_info("zsl load_done [retry %d] %lld usecs\n", retry, duration);
			ret = CSS_SUCCESS;
		} else {
			css_err("zsl_load fail\n");
			ret = CSS_ERROR_ZSL_LOAD_FAIL;
		}
	}

	bayer_buffer_release_frame(bayer_bufq, cssbuf);

	bayer_isp_read_path_disable(ZSL_READ_PATH_ALL);

	capture_wait_scaler_vsync(load_fh->cssdev, CSS_SCALER_0,
								VSYNC_WAIT_TIME_MS);

	if (ret == CSS_SUCCESS)
		wake_up_interruptible(&yuv_bufq->complete.wait);

	scaler_bayer_load_frame_for_stereo(0);
	s3dc_int_disable(INT_S3DC_FRAME_END);
	scaler_path_clk_control(PATH_RAW0_LD_CLK | PATH_RAW1_LD_CLK, CLOCK_OFF);

	if (ret == CSS_SUCCESS && yuv_bufq->mode == STREAM_MODE)
		bayer_stereo_load_schedule(load_fh);

	complete(&zsl_dev->load_done);
	zsl_dev->load_running = 0;
	mutex_unlock(&load_fh->cssdev->bayer_mutex);

	return ret;
}
#endif

int bayer_store_get_status(struct capture_fh *capfh)
{
	struct css_zsl_device *zsl_dev = NULL;
	struct css_zsl_config *zsl_config = NULL;
	int enable = 0;

	if (!capfh)
		return 0;

	if (capfh->main_scaler > CSS_SCALER_1)
		return 0;

	zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);
	if (!zsl_dev)
		return 0;

	zsl_config = &zsl_dev->config;
	if (!zsl_config)
		return 0;

	enable = zsl_config->enable;

	return enable;
}

int bayer_store_enable(struct capture_fh *capfh)
{
	int ret = CSS_SUCCESS;
	struct css_zsl_device *zsl_dev = NULL;
	struct css_bufq *bufq = NULL;
	struct css_zsl_config *zsl_config = NULL;
	struct css_device *cssdev = capfh->cssdev;
	struct css_bayer_scaler *bayer_scl = &cssdev->afifo.bayer_scl[0];
	unsigned int scl_path_reset = 0;
	unsigned int scl_clk_control = 0;

#ifdef CONFIG_VIDEO_ODIN_S3DC
	/* stereo */
	if (capture_get_stereo_status(capfh->cssdev))
		return bayer_stereo_store_enable(capfh);
#endif

	if (capfh->main_scaler > CSS_SCALER_1) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);
	zsl_config = &zsl_dev->config;

	if (!zsl_dev->buf_width || !zsl_dev->buf_height) {
		css_err("zsl[%d] input info error [%d/%d]!!\n",
				zsl_dev->path, zsl_dev->buf_width, zsl_dev->buf_height);
		return CSS_ERROR_ZSL_CURRENT_INPUT;
	}

	mutex_lock(&cssdev->bayer_mutex);

	zsl_dev->path = (css_zsl_path)capfh->main_scaler;

	switch (zsl_dev->bitfmt) {
	case RAWDATA_BIT_MODE_12BIT:
		zsl_config->bit_mode = CSS_RAWDATA_BIT_MODE_12BIT;
		break;
	case RAWDATA_BIT_MODE_10BIT:
		zsl_config->bit_mode = CSS_RAWDATA_BIT_MODE_10BIT;
		break;
	case RAWDATA_BIT_MODE_8BIT:
		zsl_config->bit_mode = CSS_RAWDATA_BIT_MODE_8BIT;
		break;
	default:
		zsl_config->bit_mode = CSS_RAWDATA_BIT_MODE_12BIT;
		break;
	}

	if (zsl_dev->path == ZSL_STORE_PATH0) {
		scl_path_reset = PATH_RAW0_ST_RESET | RAW0_ST_CTRL_RESET;
		scl_clk_control = PATH_RAW0_ST_CLK | RAW0_ST_CTRL_CLK;
	} else if (zsl_dev->path == ZSL_STORE_PATH1) {
		scl_path_reset = PATH_RAW1_ST_RESET | RAW1_ST_CTRL_RESET;
		scl_clk_control = PATH_RAW1_ST_CLK | RAW1_ST_CTRL_CLK;
	} else {
		css_err("zsl path error %d!!\n", zsl_dev->path);
		mutex_unlock(&cssdev->bayer_mutex);
		return CSS_ERROR_INVALID_ARG;
	}

	bufq = &zsl_dev->raw_buf;

	if (!bufq->ready || bufq->bufs == NULL) {
		css_err("zsl buf not ready %d (%d:0x%x)!!\n",
				zsl_dev->path, bufq->ready, bufq->bufs);
		mutex_unlock(&cssdev->bayer_mutex);
		return CSS_ERROR_ZSL_BUF_READY;
	}

	zsl_config->next_capture_index = 1;
	bufq->index = 1;
	bufq->bufs[bufq->index].state = CSS_BUF_CAPTURING;

	zsl_config->img_size.width = zsl_dev->buf_width;
	if (zsl_dev->statistic.enable) {
		zsl_config->img_size.height = zsl_dev->buf_height +
					(zsl_dev->statistic.vline + BAYER_STORE_OFFSET);
		css_zsl("statistic vsize = %d + %d\n", zsl_dev->buf_height,
					zsl_dev->statistic.vline);
	} else {
		zsl_config->img_size.height = zsl_dev->buf_height;
	}
	zsl_config->mem.base32 = bufq->bufs[bufq->index].dma_addr;

	zsl_dev->cts = ktime_get();
	zsl_dev->cfrm = 0;

	init_completion(&zsl_dev->store_full);

	scaler_path_reset(scl_path_reset);
	scaler_path_clk_control(scl_clk_control, CLOCK_ON);

	ret = scaler_bayer_store_enable(zsl_dev->path, zsl_config);

	css_info("zsl[%d] bitmode[%d] enabled = %d!!\n", zsl_dev->path,
				zsl_config->bit_mode, ret);

	if (ret != CSS_SUCCESS)
		scaler_path_clk_control(scl_clk_control, CLOCK_OFF);
	else
		capfh->zslstorefh = 1;

	mutex_unlock(&cssdev->bayer_mutex);

	return ret;
}

int bayer_store_pause(struct capture_fh *capfh)
{
	int ret = CSS_SUCCESS;
	int timeout = 0;
	unsigned int scl_int_control = 0;
	struct css_zsl_device *zsl_dev = NULL;
	struct css_zsl_config *zsl_config = NULL;

#ifdef CONFIG_VIDEO_ODIN_S3DC
	if (capture_get_stereo_status(capfh->cssdev))
		return bayer_stereo_store_pause(capfh);
#endif

	if (capfh->main_scaler > CSS_SCALER_1) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	if (capfh->cssdev == NULL) {
		css_err("cssdev is null\n");
		return CSS_ERROR_DEV_HANDLE;
	}

	mutex_lock(&capfh->cssdev->bayer_mutex);

	zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);
	zsl_config = &zsl_dev->config;

	ret = scaler_bayer_store_disable(zsl_dev->path, zsl_config);
	if (ret == CSS_SUCCESS) {
		INIT_COMPLETION(zsl_dev->store_done);
		timeout = wait_for_completion_timeout(&zsl_dev->store_done,
									msecs_to_jiffies(150));

		if (timeout == 0) {
			css_info("zsl[%d] store done timeout!!\n", zsl_dev->path);
		}
	}

	if (zsl_dev->path == ZSL_STORE_PATH0)
		scl_int_control = INT_PATH_RAW0_STORE;
	else if (zsl_dev->path == ZSL_STORE_PATH1)
		scl_int_control = INT_PATH_RAW1_STORE;

	scaler_int_disable(scl_int_control);

	css_zsl("zsl[%d] paused = %d!!\n", zsl_dev->path, ret);

	mutex_unlock(&capfh->cssdev->bayer_mutex);

	return ret;
}

int bayer_store_resume(struct capture_fh *capfh)
{
	int ret = CSS_SUCCESS;
	struct css_sensor_device *cam_dev = NULL;
	struct css_zsl_device *zsl_dev = NULL;
	struct css_bufq *bufq = NULL;
	struct css_zsl_config *zsl_config = NULL;
	struct css_device *cssdev = capfh->cssdev;
	struct css_bayer_scaler *bayer_scl = &cssdev->afifo.bayer_scl[0];
	struct v4l2_mbus_framefmt m_pix = {0,};

#ifdef CONFIG_VIDEO_ODIN_S3DC
	if (capture_get_stereo_status(capfh->cssdev))
		return bayer_stereo_store_resume(capfh);
#endif

	if (capfh->main_scaler > CSS_SCALER_1) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	if (capfh->cssdev == NULL) {
		css_err("cssdev is null\n");
		return CSS_ERROR_DEV_HANDLE;
	}

	cam_dev = capfh->cam_dev;
	zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);
	zsl_config = &zsl_dev->config;

	if (!zsl_dev->buf_width || !zsl_dev->buf_height) {
		css_err("zsl[%d] input info error [%d/%d]!!\n",
				zsl_dev->path, zsl_dev->buf_width, zsl_dev->buf_height);
		return CSS_ERROR_ZSL_CURRENT_INPUT;
	}

	mutex_lock(&capfh->cssdev->bayer_mutex);

	bufq = &zsl_dev->raw_buf;

	if (!bufq->ready || bufq->bufs == NULL) {
		css_err("zsl buf not ready %d (%d:0x%x)!!\n",
				zsl_dev->path, bufq->ready, bufq->bufs);
		mutex_unlock(&cssdev->bayer_mutex);
		return CSS_ERROR_ZSL_BUF_READY;
	}

	switch (zsl_dev->bitfmt) {
	case RAWDATA_BIT_MODE_12BIT:
		zsl_config->bit_mode = CSS_RAWDATA_BIT_MODE_12BIT;
		break;
	case RAWDATA_BIT_MODE_10BIT:
		zsl_config->bit_mode = CSS_RAWDATA_BIT_MODE_10BIT;
		break;
	case RAWDATA_BIT_MODE_8BIT:
		zsl_config->bit_mode = CSS_RAWDATA_BIT_MODE_8BIT;
		break;
	default:
		zsl_config->bit_mode = CSS_RAWDATA_BIT_MODE_12BIT;
		break;
	}

	zsl_config->img_size.width = zsl_dev->buf_width;
	if (zsl_dev->statistic.enable) {
		zsl_config->img_size.height = zsl_dev->buf_height +
					(zsl_dev->statistic.vline + BAYER_STORE_OFFSET);
		css_zsl("statistic vsize = %d + %d\n", zsl_dev->buf_height,
					zsl_dev->statistic.vline);
	} else {
		zsl_config->img_size.height = zsl_dev->buf_height;
	}

	zsl_config->mem.base32 = bufq->bufs[bufq->index].dma_addr;

	zsl_dev->cts = ktime_get();
	zsl_dev->cfrm = 0;

	ret = scaler_bayer_store_enable(zsl_dev->path, zsl_config);

	css_zsl("zsl[%d] resumed = %d!!\n", zsl_dev->path, ret);

	mutex_unlock(&capfh->cssdev->bayer_mutex);

	return ret;
}

int bayer_store_disable(struct capture_fh *capfh)
{
	int ret = CSS_SUCCESS;
	int timeout = 0;
	struct css_zsl_device *zsl_dev = NULL;
	struct css_zsl_config *zsl_config = NULL;
	unsigned int scl_clk_control = 0;
	unsigned int scl_int_control = 0;

#ifdef CONFIG_VIDEO_ODIN_S3DC
	if (capture_get_stereo_status(capfh->cssdev))
		return bayer_stereo_store_disable(capfh);
#endif

	if (capfh->main_scaler > CSS_SCALER_1) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	if (capfh->cssdev == NULL) {
		css_err("cssdev is null\n");
		return CSS_ERROR_DEV_HANDLE;
	}

	zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);
	zsl_config = &zsl_dev->config;

	if (zsl_config->enable == 0)
		return CSS_SUCCESS;

	mutex_lock(&capfh->cssdev->bayer_mutex);

	ret = scaler_bayer_store_disable(zsl_dev->path, zsl_config);
	if (ret == CSS_SUCCESS) {
		INIT_COMPLETION(zsl_dev->store_done);
		timeout = wait_for_completion_timeout(&zsl_dev->store_done,
								msecs_to_jiffies(100));

		if (timeout == 0) {
			css_info("zsl[%d] store done timeout!!\n", zsl_dev->path);
		}
		bayer_buffer_flush(capfh);
	}

	if (zsl_dev->path == ZSL_STORE_PATH0) {
		scl_clk_control = PATH_RAW0_ST_CLK | RAW0_ST_CTRL_CLK;
		scl_int_control = INT_PATH_RAW0_STORE;
		scaler_path_reset(PATH_RAW0_LD_RESET);
	} else if (zsl_dev->path == ZSL_STORE_PATH1) {
		scl_clk_control = PATH_RAW1_ST_CLK | RAW1_ST_CTRL_CLK;
		scl_int_control = INT_PATH_RAW1_STORE;
		scaler_path_reset(PATH_RAW1_LD_RESET);
	}

	scaler_path_clk_control(scl_clk_control, CLOCK_OFF);

	scaler_int_disable(scl_int_control);

	capfh->zslstorefh = 0;

	css_info("zsl[%d] disabled = %d!!\n", zsl_dev->path, ret);

	mutex_unlock(&capfh->cssdev->bayer_mutex);

	return ret;
}


static int bayer_store_nonblock_pause(struct css_zsl_device *zsl_dev)
{
	unsigned int scl_int_control = 0;

	if (zsl_dev->path == ZSL_STORE_PATH0)
		scl_int_control = INT_PATH_RAW0_STORE;
	else if (zsl_dev->path == ZSL_STORE_PATH1)
		scl_int_control = INT_PATH_RAW1_STORE;

	zsl_dev->config.enable = 0;
	scaler_int_disable(scl_int_control);
	css_info("zsl nonblock_pause\n");
	return CSS_SUCCESS;
}

static int bayer_store_nonblock_resume(struct css_zsl_device *zsl_dev)
{
	struct css_zsl_config *zsl_config = NULL;

	if (zsl_dev == NULL)
		return CSS_FAILURE;

	zsl_config = &zsl_dev->config;

	zsl_config->enable = 1;

	scaler_bayer_store_reset(zsl_dev->path);
	scaler_bayer_store_configure(zsl_dev->path, zsl_config);
	scaler_bayer_store_capture(zsl_dev->path, zsl_config);
	css_info("zsl nonblock_resume\n");

	return CSS_SUCCESS;
}

int bayer_store_mode(struct capture_fh *capfh, zsl_store_mode mode)
{
	int ret = CSS_SUCCESS;
	struct css_zsl_device *zsl_dev = NULL;

#ifdef CONFIG_VIDEO_ODIN_S3DC
	if (capture_get_stereo_status(capfh->cssdev))
		return bayer_stereo_store_mode(capfh, mode);
#endif

	if (capfh->main_scaler > CSS_SCALER_1) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);
	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}

	if (zsl_dev->config.enable) {
		css_warn("zsl[%d] set store mode err: busy\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEV_BUSY;
	}

	zsl_dev->storemode = mode;
	css_info("zsl[%d] set store mode = %d!!\n", zsl_dev->path, mode);

	return ret;
}

int bayer_store_frame_interval(struct capture_fh *capfh, unsigned int interval)
{
	int ret = CSS_SUCCESS;
	struct css_zsl_device *zsl_dev = NULL;

#ifdef CONFIG_VIDEO_ODIN_S3DC
	if (capture_get_stereo_status(capfh->cssdev))
		return bayer_stereo_store_frame_interval(capfh, interval);
#endif

	if (capfh->main_scaler > CSS_SCALER_1) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);
	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}

	if (interval <= 0) interval = 1;
	if (interval > 30) interval = 30;
	zsl_dev->frame_interval = interval;

	css_info("zsl[%d] set frame interval = %d!!\n", zsl_dev->path, interval);

	return ret;
}

int bayer_store_bit_format(struct capture_fh *capfh, zsl_store_raw_bitfmt fmt)
{
	int ret = CSS_SUCCESS;
	struct css_zsl_device *zsl_dev = NULL;

#ifdef CONFIG_VIDEO_ODIN_S3DC
	if (capture_get_stereo_status(capfh->cssdev))
		ret = bayer_stereo_store_bit_format(capfh, fmt);
#endif

	if (capfh->main_scaler > CSS_SCALER_1) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);
	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}

	if (zsl_dev->config.enable) {
		css_warn("zsl[%d] set store bit format err: busy\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEV_BUSY;
	}

	zsl_dev->bitfmt = fmt;
	css_info("zsl[%d] set store bit format = %d!!\n", zsl_dev->path, fmt);

	return ret;
}

int bayer_store_wait_buffer_full(struct capture_fh *capfh)
{
	int ret = CSS_SUCCESS;
	int timeout = 0;
	int waittime = 0;
	int fps = 0;
	int frmintv = 0;
	struct css_zsl_device *zsl_dev = NULL;

#ifdef CONFIG_VIDEO_ODIN_S3DC
	if (capture_get_stereo_status(capfh->cssdev))
		return bayer_stereo_store_wait_buffer_full(capfh);
#endif

	if (capfh->main_scaler > CSS_SCALER_1) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);
	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}
#if 0
	if (!zsl_dev->config.enable) {
		css_err("zsl[%d] not enabled\n", zsl_dev->path);
		return CSS_ERROR_ZSL_NOT_ENABLED;
	}
#endif
	if (zsl_dev->storemode == ZSL_STORE_STREAM_MODE) {
		css_err("zsl[%d] not fill buffer mode!!\n", zsl_dev->path);
		return CSS_ERROR_INVALID_ARG;
	}

	if (zsl_dev->frame_interval)
		frmintv = zsl_dev->frame_interval;
	else
		frmintv = 1;

#ifdef CONFIG_VIDEO_ODIN_OIS_ONE_FPS_TEST
	fps = 1 / frmintv;

	if (fps == 0) {
		fps = 1;
	}
#else
	fps = 8 / frmintv;
#endif

	waittime = FPS_TO_MS(fps) * (zsl_dev->buf_count + 3);

	INIT_COMPLETION(zsl_dev->store_full);

	timeout = wait_for_completion_timeout(&zsl_dev->store_full,
							msecs_to_jiffies(waittime));

	if (timeout == 0) {
		ret = CSS_ERROR_ZSL_TIMEOUT;
		css_info("zsl[%d] en=%d store full timeout(%d) [%d/%d]!!\n",
			zsl_dev->path, zsl_dev->config.enable, waittime,
			bayer_buffer_get_queued_count(&zsl_dev->raw_buf),
			zsl_dev->buf_count);
		odin_css_reg_dump();
	} else {
		css_info("zsl[%d] wait buffer full Done [%d/%d]!!\n",
			zsl_dev->path, bayer_buffer_get_queued_count(&zsl_dev->raw_buf),
			zsl_dev->buf_count);
#if 0
		{
			struct css_bufq *cssbufq;
			struct css_buffer *tmp_buffer;
			struct list_head *count, *n;
			unsigned long flags;

			cssbufq = &zsl_dev->raw_buf;

			spin_lock_irqsave(&cssbufq->lock, flags);
			list_for_each_safe(count, n, &cssbufq->head) {
					tmp_buffer = list_entry(count, struct css_buffer, list);
				css_info("buf[%d]: %d\n", tmp_buffer->id, tmp_buffer->frame_cnt);
			}
			spin_unlock_irqrestore(&cssbufq->lock, flags);

		}
#endif
	}

	return ret;
}

int bayer_store_statistic(struct capture_fh *capfh,
			struct zsl_store_statistic *info)
{
	int ret = CSS_SUCCESS;
	struct css_zsl_device *zsl_dev = NULL;

	if (!info)
		return CSS_ERROR_INVALID_ARG;

	if (capture_get_stereo_status(capfh->cssdev))
		return CSS_ERROR_UNSUPPORTED;

	if (capfh->main_scaler > CSS_SCALER_1) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);
	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}
#if 0

	if (zsl_dev->config.enable) {
		css_warn("zsl[%d] set statistic err: busy\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEV_BUSY;
	}
#endif

	if (info->enable) {
		if (info->vline == 0) {
			css_err("zsl invalid v stat size!!\n");
			return CSS_ERROR_ZSL_STAT_VSIZE;
		}
		zsl_dev->statistic.hsize = info->hsize;
		zsl_dev->statistic.vline = info->vline;
	} else {
		zsl_dev->statistic.hsize = 0;
		zsl_dev->statistic.vline = 0;
	}

	zsl_dev->statistic.enable = info->enable;
	css_info("zsl[%d] statistic %d [%d %d] \n", zsl_dev->path, info->enable,
				info->hsize, info->vline);

	return ret;
}

int bayer_store_get_queued_count(struct capture_fh *capfh, unsigned int *count)
{
	int ret = CSS_SUCCESS;
	struct css_zsl_device *zsl_dev = NULL;
	struct css_bufq *bayer_bufq = NULL;

	if (count == NULL) {
		css_err("count is zero\n");
		return CSS_FAILURE;
	}

	if (capfh->main_scaler > CSS_SCALER_1) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);
	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}

	bayer_bufq = &zsl_dev->raw_buf;

	*count = bayer_buffer_get_queued_count(bayer_bufq);

	return ret;
}

int bayer_load_init_count(struct capture_fh *capfh)
{
	struct css_zsl_device *zsl_dev = NULL;
	css_zsl_path r_path = 0;

	if (capture_get_stereo_status(capfh->cssdev))
		return CSS_SUCCESS;

	if (capfh->main_scaler == 0)
		r_path = ZSL_READ_PATH1;
	else
		r_path = ZSL_READ_PATH0;

	zsl_dev = capture_get_zsl_dev(capfh, r_path);
	if (zsl_dev)
		zsl_dev->loadcnt = 0;

	return CSS_SUCCESS;
}

void bayer_load_set_type(struct capture_fh *capfh, zsl_load_type type)
{
	struct css_bufq *bufq = NULL;
	struct css_zsl_device *zsl_dev = NULL;

	css_zsl_path r_path = 0;
	if (capfh->main_scaler == 0)
		r_path = ZSL_READ_PATH1;
	else
		r_path = ZSL_READ_PATH0;

	zsl_dev = capture_get_zsl_dev(capfh, r_path);
	if (zsl_dev) {
		zsl_dev->loadtype = type;
	}
}

zsl_load_type bayer_load_get_type(struct capture_fh *capfh)
{
	struct css_zsl_device *zsl_dev = NULL;

	css_zsl_path r_path = 0;
	if (capfh->main_scaler == 0)
		r_path = ZSL_READ_PATH1;
	else
		r_path = ZSL_READ_PATH0;

	zsl_dev = capture_get_zsl_dev(capfh, r_path);
	if (zsl_dev) {
		if (zsl_dev->loadtype == ZSL_LOAD_ONESHOT_TYPE)
			return ZSL_LOAD_ONESHOT_TYPE;
	}

	return ZSL_LOAD_STREAM_TYPE;
}

int bayer_load_frame_number(struct capture_fh *capfh, int num)
{
	int ret = CSS_SUCCESS;
	struct css_zsl_device *zsl_dev = NULL;
	int store_path = 0;

#ifdef CONFIG_VIDEO_ODIN_S3DC
	if (capture_get_stereo_status(capfh->cssdev))
		return bayer_stereo_load_frame_number(capfh, num);
#endif

	if (capfh->main_scaler > CSS_SCALER_1) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	if (capfh->zslstorefh) {
		css_err("not allowed to store fh!!\n");
		return CSS_ERROR_ZSL_NOT_ALLOWED_STORE_FH;
	}

	if (capfh->cssdev == NULL) {
		css_err("cssdev is null\n");
		return CSS_ERROR_DEV_HANDLE;
	}

	if (capfh->main_scaler == CSS_SCALER_0)
		store_path = CSS_SCALER_1;
	else
		store_path = CSS_SCALER_0;

	zsl_dev = capture_get_zsl_dev(capfh, store_path);

	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}

	mutex_lock(&capfh->cssdev->bayer_mutex);

	zsl_dev->loadtype = ZSL_LOAD_ONESHOT_TYPE;

	if (num <= 0)
		zsl_dev->setloadframe = 0;
	else
		zsl_dev->setloadframe = num;

	css_info("zsl[%d] set load frame num: %d!!\n",
			zsl_dev->path, zsl_dev->setloadframe);

	mutex_unlock(&capfh->cssdev->bayer_mutex);

	return CSS_SUCCESS;
}

int bayer_load_mode(struct capture_fh *capfh, zsl_load_mode mode)
{
	int ret = CSS_SUCCESS;
	struct css_zsl_device *zsl_dev = NULL;
	int store_path = 0;

#ifdef CONFIG_VIDEO_ODIN_S3DC
	if (capture_get_stereo_status(capfh->cssdev))
		return bayer_stereo_load_mode(capfh, mode);
#endif

	if (capfh->main_scaler > CSS_SCALER_1) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	if (capfh->zslstorefh) {
		css_err("not allowed to store fh!!\n");
		return CSS_ERROR_ZSL_NOT_ALLOWED_STORE_FH;
	}

	if (capfh->cssdev == NULL) {
		css_err("cssdev is null\n");
		return CSS_ERROR_DEV_HANDLE;
	}

	if (capfh->main_scaler == CSS_SCALER_0)
		store_path = CSS_SCALER_1;
	else
		store_path = CSS_SCALER_0;

	zsl_dev = capture_get_zsl_dev(capfh, store_path);
	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}

	if (zsl_dev->load_running) {
		css_err("zsl[%d] is busy!!\n", zsl_dev->path);
		return -EBUSY;
	}

	mutex_lock(&capfh->cssdev->bayer_mutex);
	zsl_dev->loadmode = mode;
	css_info("zsl[%d] set load mode: %d!!\n", zsl_dev->path, mode);
	mutex_unlock(&capfh->cssdev->bayer_mutex);

	return CSS_SUCCESS;
}

static int bayer_load_capture_wait_done(struct css_zsl_device *zsl_dev,
			unsigned int mask, unsigned int msec)
{
	int timeout = 0;
	int done_st;

	mask &= 0x7FFF;

	if (zsl_dev->load_running) {
		if (mask != 0) {
			timeout = msec;
			do {
				done_st = scaler_get_done_status();
				if (zsl_dev->error) {
					zsl_dev->error = 0;
					return CSS_FAILURE;
				} else if (done_st & mask)	{
					return timeout;
				} else {
					usleep_range(1000, 1000);
				}

			} while (timeout-- > 0);

			timeout = 0;
		}
	} else {
		timeout = 0;
	}

	return timeout;
}

int bayer_load_wait_done(struct capture_fh *capfh)
{
	struct css_zsl_device *zsl_dev = NULL;
	int capture_scaler = 0;
	int r_path = 0;
	int timeout =0;

	if (capfh == NULL){
		css_err("capfh null\n");
		return CSS_ERROR_CAPTURE_HANDLE;
	}

	if (capfh->main_scaler > CSS_SCALER_1) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	if (capfh->zslstorefh) {
		css_err("not allowed to store fh!!\n");
		return CSS_ERROR_ZSL_NOT_ALLOWED_STORE_FH;
	}

	capture_scaler = capfh->main_scaler;

	/*
	 * current front zsl capture path is zsl1->isp1->scaler0.
	 * if we wanna use zsl0->isp0->scaler0, use below configure.
	 * r_path = ZSL_READ_PATH0;
	 */

	if (capture_scaler == 0)
		r_path = ZSL_READ_PATH1;
	else
		r_path = ZSL_READ_PATH0;

	zsl_dev = capture_get_zsl_dev(capfh, r_path);
	if (zsl_dev) {
		if (zsl_dev->load_running) {
			css_zsl("load is running..\n");
			INIT_COMPLETION(zsl_dev->load_done);
			timeout = wait_for_completion_timeout(&zsl_dev->load_done,
										msecs_to_jiffies(300));
			if (timeout == 0) {
				timeout = CSS_ERROR_ZSL_TIMEOUT;
				css_info("zsl[%d] load wait done timeout!!\n", zsl_dev->path);
			} else {
				css_zsl("load is finished..\n");
			}
		}
	}

	return timeout;
}

int bayer_load_buf_index(struct capture_fh *capfh, unsigned int idx)
{
	int ret = CSS_SUCCESS;
	struct css_zsl_device *zsl_dev = NULL;
	int store_path = 0;

#ifdef CONFIG_VIDEO_ODIN_S3DC
	if (capture_get_stereo_status(capfh->cssdev))
		return bayer_stereo_store_buf_index(capfh, idx);
#endif

	if (capfh->main_scaler > CSS_SCALER_1) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	if (capfh->zslstorefh) {
		css_err("not allowed to store fh!!\n");
		return CSS_ERROR_ZSL_NOT_ALLOWED_STORE_FH;
	}

	if (capfh->main_scaler == CSS_SCALER_0)
		store_path = CSS_SCALER_1;
	else
		store_path = CSS_SCALER_0;

	zsl_dev = capture_get_zsl_dev(capfh, store_path);
	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}

	if (idx > zsl_dev->buf_count)
		idx = 0;

	zsl_dev->loadbuf_idx = idx;

	css_zsl("zsl[%d] set load buf offset = %d!!\n", zsl_dev->path, idx);

	return ret;
}

int bayer_load_buf_order(struct capture_fh *capfh, unsigned int order)
{
	int ret = CSS_SUCCESS;
	int store_path = 0;
	struct css_zsl_device *zsl_dev = NULL;

#ifdef CONFIG_VIDEO_ODIN_S3DC
	if (capture_get_stereo_status(capfh->cssdev))
		return bayer_stereo_store_buf_order(capfh, order);
#endif

	if (capfh->main_scaler > CSS_SCALER_1) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	if (capfh->zslstorefh) {
		css_err("not allowed to store fh!!\n");
		return CSS_ERROR_ZSL_NOT_ALLOWED_STORE_FH;
	}

	if (capfh->main_scaler == CSS_SCALER_0)
		store_path = CSS_SCALER_1;
	else
		store_path = CSS_SCALER_0;

	zsl_dev = capture_get_zsl_dev(capfh, store_path);
	if (zsl_dev == NULL) {
		css_err("zsl[%d] device null!!\n", zsl_dev->path);
		return CSS_ERROR_ZSL_DEVICE;
	}

	if (order > ZSL_LOAD_ASCENDING)
		order = ZSL_LOAD_DECENDING;

	zsl_dev->loadbuf_order = order;

	css_zsl("zsl[%d] set load buf order = %d!!\n", zsl_dev->path, order);

	return ret;
}

int bayer_load_schedule(struct capture_fh *load_fh)
{
	int ret = CSS_SUCCESS;
	struct css_device *cssdev = NULL;

	cssdev = load_fh->cssdev;

#ifdef CONFIG_VIDEO_ODIN_S3DC
	if (capture_get_stereo_status(cssdev))
		return bayer_stereo_load_schedule(load_fh);
#endif

	/*
	 * current front zsl capture path is zsl1->isp1->scaler0.
	 * if we wanna use zsl0->isp0->scaler0, use below configure.
	 * schedule_work(&cssdev->work_bayer_loadpath0);
	 */

	if (load_fh->main_scaler == 0)
		schedule_work(&cssdev->work_bayer_loadpath0);
	else
		schedule_work(&cssdev->work_bayer_loadpath1);

	return ret;
}
#if 0
int bayer_load_test_only(struct capture_fh *load_fh)
{
#define CSS_ISP0_BASE		 0x33000000
#define CSS_ISP1_BASE		 0x33080000

	int ret = CSS_SUCCESS;

	struct css_scaler_device scl_dev;
	struct css_zsl_device *zsl_dev = NULL;
	struct css_bufq *bayer_bufq = NULL;
	struct css_bufq *yuv_bufq = NULL;
	struct css_buffer *cssbuf = NULL;
	struct css_postview *postview = NULL;

	void __iomem *css_isp0_reg = NULL;
	void __iomem *css_isp1_reg = NULL;

	struct css_zsl_config zsl_config;
	unsigned int buf_index = 0;
	unsigned int phy_addr = 0;

	css_zsl_path r_path = 0;
	int capture_scaler = 1;
	int postview_enable = 0;

	unsigned int src_w = 0;
	unsigned int src_h = 0;
	struct css_afifo_pre pre;
	struct css_afifo_post post;
	struct css_bayer_scaler bayer_scl;

	ktime_t delta, stime, etime;
	unsigned long long duration;

	if (load_fh == NULL) {
		css_zsl("load file already closed!!\n");
		return CSS_SUCCESS;
	}

	if (capture_get_stream_mode(load_fh) == 0) {
		css_zsl("load file already streamoff!!\n");
		return CSS_SUCCESS;
	}

	memset(&pre, 0, sizeof(struct css_afifo_pre));
	memset(&post, 0, sizeof(struct css_afifo_post));
	memset(&bayer_scl, 0, sizeof(struct css_bayer_scaler));


	pre.index = 0;
	pre.width = 4208;
	pre.height = 3120;
	pre.format = 1;
	odin_isp_wrap_pre_param(&pre);

	post.index = 0;
	post.width = 4208;
	post.height = 3120;
	odin_isp_wrap_post_param(&post);

	bayer_scl.index = 0;
	bayer_scl.format = 1;
	bayer_scl.src_w = 4208;
	bayer_scl.src_h = 3120;
	bayer_scl.dest_w = 4208;
	bayer_scl.dest_h = 3120;
	bayer_scl.margin_x = 0;
	bayer_scl.margin_y = 0;
	bayer_scl.offset_x = 0;
	bayer_scl.offset_y = 0;
	bayer_scl.crop_en = 0;
	bayer_scl.scl_en = 0;
	odin_isp_wrap_bayer_param(&bayer_scl);

	css_isp0_reg = ioremap(CSS_ISP0_BASE, SZ_256K+SZ_128K);
	css_isp1_reg = ioremap(CSS_ISP1_BASE, SZ_256K+SZ_128K);

	writel(0x00000011, css_isp0_reg + 0x00);
	writel(0x00000001, css_isp0_reg + 0x14);

	writel(3120, css_isp0_reg + 0x38);
	writel(3120, css_isp0_reg + 0x10340);
	writel(4208, css_isp0_reg + 0x10344);
	writel(0x000, css_isp0_reg + 0x10348);
	writel(0x001, css_isp0_reg + 0x1034C);
	writel(3120, css_isp0_reg + 0x200e0);
	writel(4208, css_isp0_reg + 0x200e4);
	writel(0x000, css_isp0_reg + 0x200e8);
	writel(0x001, css_isp0_reg + 0x200eC);
	writel(3120, css_isp0_reg + 0x500f0);
	writel(4208, css_isp0_reg + 0x500f4);
	writel(0x001, css_isp0_reg + 0x500f8);
	writel(0x001, css_isp0_reg + 0x500fC);

	odin_isp_wrap_set_live_path();
	odin_isp_wrap_set_path_sel(ISP0_LINKTO_SC1_ISP1_LINKTO_SC0);
	odin_isp_wrap_set_mem_ctrl(DUAL_SRAM_ENABLE);

	r_path = ZSL_READ_PATH0;

	zsl_dev = capture_get_zsl_dev(load_fh, r_path);

	zsl_dev->load_running = 1;

	src_w = zsl_dev->config.img_size.width;
	src_h = zsl_dev->config.img_size.height;

	memset(&scl_dev, 0, sizeof(struct css_scaler_device));
	memset(&zsl_config, 0, sizeof(struct css_zsl_config));

	/* configure load : physical address, size, load format */
	bayer_bufq = &zsl_dev->raw_buf;

	zsl_dev->loadbuf_idx = 0;

	if (zsl_dev->loadbuf_idx > 0) {
		unsigned int skip_buf = zsl_dev->loadbuf_idx + 1;
		do {
			cssbuf = bayer_buffer_get_frame(bayer_bufq, CSS_BUF_POP_TOP);
			if (cssbuf == NULL) {
				break;
			}
			if (skip_buf != 1) {
				bayer_buffer_release_frame(bayer_bufq, cssbuf);
			}
			css_info("zsl[%d] pop : index %d(%ld)\n", zsl_dev->path, cssbuf->id,
					 cssbuf->frame_cnt);
		} while (--skip_buf > 0);
	} else {
		cssbuf = bayer_buffer_get_frame(bayer_bufq, CSS_BUF_POP_TOP);
		if (cssbuf != NULL) {
			css_info("zsl[%d] pop : index %d(%ld)\n", zsl_dev->path, cssbuf->id,
					 cssbuf->frame_cnt);
		}
	}

	if (cssbuf == NULL) {
		css_err("zsl[%d] load source empty!!\n", zsl_dev->path);
		zsl_dev->load_running = 0;
		return CSS_ERROR_ZSL_LOAD_EMPTY;
	}

	/* indicate read memory address */
	zsl_config.mem.base32		 = (unsigned int)cssbuf->dma_addr;
	/* decide read pixel width and line count, read image format */
	zsl_config.img_size.width	 = src_w;
	zsl_config.img_size.height	 = src_h;
	zsl_config.bit_mode			 = zsl_dev->config.bit_mode;
	zsl_config.ld_fmt			 = CSS_LOAD_IMG_RAW;

	zsl_config.blank_count = ZSL_LOAD_1CH_BLKCNT_MULTIPLE(src_w);

	if (zsl_config.img_size.width == 0 || zsl_config.img_size.height == 0) {
		css_err("zsl[%d] width = %d height=%d\n",
			zsl_dev->path, zsl_config.img_size.width,
			zsl_config.img_size.height);
		zsl_dev->load_running = 0;
		return CSS_ERROR_INVALID_SIZE;
	}

	css_zsl("zsl blank size: %d\n", zsl_config.blank_count);

	/* current front zsl capture path is zsl1->isp1->scaler0.
	 * if we wanna use zsl0->isp0->scaler0, use below configure.
	 * change here: r_path = ZSL_READ_PATH0;
	*/

	if (r_path == ZSL_READ_PATH0) {
		scaler_path_reset(PATH_RAW0_LD_RESET);
		scaler_path_clk_control(PATH_RAW0_LD_CLK, CLOCK_ON);
	} else if (r_path == ZSL_READ_PATH1) {
		scaler_path_reset(PATH_RAW1_LD_RESET);
		scaler_path_clk_control(PATH_RAW1_LD_CLK, CLOCK_ON);
	}

	css_zsl("zsl read size: (%d, %d)\n", zsl_config.img_size.width,
					zsl_config.img_size.height);

	bayer_isp_read_path_enable(r_path);

	/* configure scaler : physical address, size */
	yuv_bufq = &load_fh->fb->drop_bufq;
	buf_index = 0;
	phy_addr = (unsigned int)yuv_bufq->bufs[buf_index].dma_addr;

	yuv_bufq->bufs[buf_index].frame_cnt = cssbuf->frame_cnt;
	yuv_bufq->bufs[buf_index].timestamp = cssbuf->timestamp;

	scl_dev.config.src_width	= zsl_config.img_size.width;
	scl_dev.config.src_height	= zsl_config.img_size.height;
	scl_dev.config.dst_width	= load_fh->scl_dev->config.dst_width;
	scl_dev.config.dst_height	= load_fh->scl_dev->config.dst_height;
	scl_dev.config.action		= CSS_SCALER_ACT_CAPTURE;
	scl_dev.config.pixel_fmt	= load_fh->scl_dev->config.pixel_fmt;
	scl_dev.config.crop.start_x	= load_fh->scl_dev->config.crop.start_x;
	scl_dev.config.crop.start_y	= load_fh->scl_dev->config.crop.start_y;
	scl_dev.config.crop.width	= load_fh->scl_dev->config.crop.width;
	scl_dev.config.crop.height	= load_fh->scl_dev->config.crop.height;

	ret = scaler_configure(capture_scaler, &scl_dev.config, &scl_dev.data);

	if (ret == CSS_SUCCESS) {
		if (phy_addr != 0) {

			yuv_bufq->bufs[buf_index].state = CSS_BUF_CAPTURING;

			css_zsl("zsl read base: 0x%x\n", phy_addr);

			scaler_set_buffers(capture_scaler,
						&scl_dev.config, phy_addr);
		} else {
			ret = CSS_FAILURE;
		}
	}

	ret = scaler_bayer_load_configure((css_scaler_select)r_path, &zsl_config);

	if (ret == CSS_SUCCESS) {
		ret = scaler_bayer_load_frame_for_1ch(1, r_path, CSS_SCALER_NONE);
	}

	if (ret == CSS_SUCCESS) {
		if (r_path == ZSL_READ_PATH0) {
			capture_wait_done(PATH_RAW0_LD_DONE, 500);
		} else {
			capture_wait_done(PATH_RAW1_LD_DONE, 500);
		}
	}

	scaler_bayer_load_frame_for_1ch(0, r_path, CSS_SCALER_NONE);

	if (ret == CSS_SUCCESS) {
		ret = scaler_bayer_load_configure((css_scaler_select)r_path,
				&zsl_config);
	}

	if (ret == CSS_SUCCESS) {
		ret = scaler_bayer_load_frame_for_1ch(1, r_path, capture_scaler);
	}

	if (ret == CSS_SUCCESS) {
		int retry = 5;

		do {
			int timeout = 0;
			timeout = capture_wait_done(PATH_1_BUSI_CAPTURE_DONE, 100);

			if (timeout > 0)
				break;

			--retry;
		} while (retry > 0);

		if (retry > 0) {
			css_info("zsl load_done\n");

			ret = CSS_SUCCESS;

		} else {
			css_info("zsl_load fail\n");
			ret = CSS_ERROR_ZSL_LOAD_FAIL;
		}
	}

	scaler_bayer_load_frame_for_1ch(0, r_path, capture_scaler);

	bayer_buffer_release_frame(bayer_bufq, cssbuf);

	bayer_isp_read_path_disable(r_path);

	zsl_dev->load_running = 0;

	iounmap(css_isp0_reg);
	iounmap(css_isp1_reg);

	return ret;

}
#endif

static struct css_buffer *bayer_load_source(struct css_zsl_device *zsl_dev,
									struct css_bufq *zsl_bufq,
									css_buffer_pop_types ptype,
									int find_and_get)
{
	struct css_buffer *cssbuf = NULL;
	struct css_buffer *lastbuf = NULL;

	if (!zsl_dev)
		return NULL;

	if (find_and_get) {
		cssbuf = bayer_buffer_find_get_frame(zsl_bufq, find_and_get,
											CSS_BUF_QUEUED, 500);
		if (cssbuf != NULL) {
			css_info("zsl[%d] pop : index %d(%ld:%ld) %p [%d/%d]\n",
					 zsl_dev->path,
					 cssbuf->id,
					 find_and_get,
					 cssbuf->frame_cnt,
					 (unsigned int)cssbuf->dma_addr,
					 bayer_buffer_get_queued_count(zsl_bufq),
					 zsl_bufq->count-1);
		}
	} else {
		if (zsl_dev->loadbuf_idx > 0) {
			unsigned int queued_cnt = 0;
			unsigned long flags;
			unsigned int skip_buf = zsl_dev->loadbuf_idx + 1;
			int last = 0;

			queued_cnt = bayer_buffer_get_queued_count(zsl_bufq);

			if (skip_buf > queued_cnt)
				skip_buf = queued_cnt;

			do {
				cssbuf = bayer_buffer_get_frame(zsl_bufq, ptype);
				if (cssbuf == NULL) {
					cssbuf = lastbuf;
					last = 1;
					if (cssbuf) {
						spin_lock_irqsave(&zsl_bufq->lock, flags);
						cssbuf->state = CSS_BUF_QUEUED;
						spin_unlock_irqrestore(&zsl_bufq->lock, flags);
					}
					break;
				}
				lastbuf = cssbuf;
				if (skip_buf != 1) {
					bayer_buffer_release_frame(zsl_bufq, cssbuf);
				}
			} while (--skip_buf > 0);
			if (cssbuf != NULL) {
				css_info("zsl[%d] pop : index %d(%ld) [%d/%d/%d/%d]\n",
						zsl_dev->path, cssbuf->id, cssbuf->frame_cnt,
						queued_cnt, zsl_bufq->count-1,
						zsl_dev->loadbuf_idx, last);
			}
			zsl_dev->loadbuf_idx = 0;
		} else {
			cssbuf = bayer_buffer_get_frame(zsl_bufq, ptype);
			if (cssbuf != NULL) {
				css_info("zsl[%d] pop : index %d(%ld) %p [%d/%d]\n",
						 zsl_dev->path, cssbuf->id, cssbuf->frame_cnt,
						 (unsigned int)cssbuf->dma_addr,
						 bayer_buffer_get_queued_count(zsl_bufq),
						 zsl_bufq->count-1);
			}
		}
	}

	return cssbuf;
}

int bayer_load_frame(struct capture_fh *load_fh)
{
	int ret = CSS_SUCCESS;
	struct css_scaler_device scl_dev;
	struct css_zsl_device *zsl_dev = NULL;
	struct css_bufq *bayer_bufq = NULL;
	struct css_bufq *yuv_bufq = NULL;
	struct css_buffer *cssbuf = NULL;
	struct css_postview *postview = NULL;
	struct css_zsl_config zsl_config;
	int buf_index = 0;
	unsigned int phy_addr = 0;
	css_zsl_path r_path = 0;
	css_buffer_pop_types ptype = 0;
	int capture_scaler = 0;
	int postview_enable = 0;
	int find_and_get = 0;
	int preload = 0;
	unsigned int src_w = 0;
	unsigned int src_h = 0;
	ktime_t delta, stime, etime;
	unsigned long long duration;
	int retry = 5;

	if (load_fh == NULL) {
		css_zsl("load file already closed!!\n");
		return CSS_SUCCESS;
	}

	if (load_fh->zslstorefh) {
		css_err("not allowed to store fh!!\n");
		return CSS_ERROR_ZSL_NOT_ALLOWED_STORE_FH;
	}

	if (load_fh->cssdev == NULL) {
		css_err("cssdev is null\n");
		return CSS_ERROR_DEV_HANDLE;
	}

	if (capture_get_stream_mode(load_fh) == 0) {
		css_zsl("load file already streamoff!!\n");
		return CSS_SUCCESS;
	}

	if (load_fh->main_scaler > CSS_SCALER_1) {
		css_err("zsl invalid scaler number!!\n");
		return CSS_ERROR_INVALID_SCALER_NUM;
	}

	capture_scaler = load_fh->main_scaler;

	if (capture_scaler == 0)
		r_path = ZSL_READ_PATH1;
	else
		r_path = ZSL_READ_PATH0;

	if (load_fh->buf_attrb.buf_type == CSS_BUF_CAPTURE_WITH_POSTVIEW)
		postview_enable = 1;

	zsl_dev = capture_get_zsl_dev(load_fh, r_path);

	src_w = zsl_dev->config.img_size.width;
	src_h = zsl_dev->config.img_size.height;
	if (src_w == 0 || src_h == 0) {
		css_err("zsl[%d] width = %d height=%d\n", zsl_dev->path, src_w, src_h);
		return CSS_ERROR_INVALID_SIZE;
	}

	if (zsl_dev->setloadframe > 0) {
		find_and_get = zsl_dev->setloadframe;
		zsl_dev->setloadframe = 0;
	} else {
		find_and_get = 0;
	}

	mutex_lock(&load_fh->cssdev->bayer_mutex);

	zsl_dev->load_running = 1;

	if (zsl_dev->loadbuf_order == ZSL_LOAD_DECENDING)
		ptype = CSS_BUF_POP_TOP;
	else
		ptype = CSS_BUF_POP_BOTTOM;

	memset(&scl_dev, 0, sizeof(struct css_scaler_device));
	memset(&zsl_config, 0, sizeof(struct css_zsl_config));

	postview = &load_fh->cssdev->postview;

	/* configure load : physical address, size, load format */
	bayer_bufq = &zsl_dev->raw_buf;
	yuv_bufq = &load_fh->fb->capture_bufq;
	if (!yuv_bufq->bufs || !bayer_bufq->bufs) {
		css_err("zsl[%d] buffer null!!\n", zsl_dev->path);
		ret = CSS_FAILURE;
		goto out;
	}

	if (zsl_dev->config.enable) {
		if (bayer_buffer_get_queued_count(bayer_bufq) == 0) {
			int timeout = 0;
			int waittime = 0;
			waittime = 2 * 130;
			bayer_store_nonblock_pause(zsl_dev);
			if (zsl_dev->store_full.done > 0 &&
				zsl_dev->storemode == ZSL_STORE_FILL_BUFFER_MODE) {
				css_info("check: store full done(%d)\n", zsl_dev->storemode);
				INIT_COMPLETION(zsl_dev->store_full);
			}
			bayer_store_nonblock_resume(zsl_dev);
			timeout = bayer_buffer_wait_queue_available(bayer_bufq, waittime);
			css_info("zsl[%d] frame wait %dms\n", zsl_dev->path, waittime - timeout);
		}
	}

	cssbuf = bayer_load_source(zsl_dev, bayer_bufq, ptype, find_and_get);
	if (cssbuf == NULL) {
		css_err("zsl[%d] load source empty!!\n", zsl_dev->path);
		ret = CSS_ERROR_ZSL_LOAD_EMPTY;
		goto out;
	}

	bayer_ion_handle_data	= (unsigned int *)&cssbuf->ion_hdl;
	bayer_ion_client		= (unsigned int *)zsl_dev->ion_client;
	bayer_ion_size			= (unsigned int)cssbuf->byteused;
	bayer_ion_align_size	= (unsigned int)cssbuf->size;

	/* indicate read memory address  */
	zsl_config.mem.base32		= (unsigned int)cssbuf->dma_addr;
	/* decide read pixel width and line count, read image format */
	zsl_config.img_size.width	= src_w;
	zsl_config.img_size.height	= src_h;
	zsl_config.bit_mode			= zsl_dev->config.bit_mode;
	zsl_config.ld_fmt			= CSS_LOAD_IMG_RAW;

	if (postview_enable)
		zsl_config.blank_count = ZSL_LOAD_2CH_BLKCNT_MULTIPLE(src_w);
	else
		zsl_config.blank_count = ZSL_LOAD_1CH_BLKCNT_MULTIPLE(src_w);

	css_zsl("zsl blank size: %d\n", zsl_config.blank_count);
	css_zsl("zsl read size: (%d, %d)\n", zsl_config.img_size.width,
					zsl_config.img_size.height);

	/* configure scaler : physical address, size */
	buf_index	= yuv_bufq->index;
	yuv_bufq->q_pos = buf_index;

	if (buf_index < 0 || buf_index >= yuv_bufq->count) {
		css_err("zsl[%d] buf index error %d\n", zsl_dev->path, buf_index);
		ret = CSS_ERROR_BUFFER_INDEX;
		goto out;
	}

	yuv_bufq->bufs[buf_index].frame_cnt = cssbuf->frame_cnt;
	yuv_bufq->bufs[buf_index].timestamp = cssbuf->timestamp;

	scl_dev.config.src_width	= zsl_config.img_size.width;
	scl_dev.config.src_height	= zsl_config.img_size.height;
	scl_dev.config.dst_width	= load_fh->scl_dev->config.dst_width;
	scl_dev.config.dst_height	= load_fh->scl_dev->config.dst_height;
	scl_dev.config.action		= CSS_SCALER_ACT_CAPTURE;
	scl_dev.config.pixel_fmt	= load_fh->scl_dev->config.pixel_fmt;
	scl_dev.config.crop.start_x = load_fh->scl_dev->config.crop.start_x;
	scl_dev.config.crop.start_y = load_fh->scl_dev->config.crop.start_y;
	scl_dev.config.crop.width = load_fh->scl_dev->config.crop.width;
	scl_dev.config.crop.height = load_fh->scl_dev->config.crop.height;

	if (capture_get_path_mode(load_fh->cssdev) == CSS_NORMAL_PATH_MODE)
		bayer_isp_read_path_enable(ZSL_READ_PATH0);

	/*
	 * current front zsl capture path is zsl1->isp1->scaler0.
	 * if we wanna use zsl0->isp0->scaler0, use below configure.
	 * change here: r_path = ZSL_READ_PATH0;
	 */

	if (r_path == ZSL_READ_PATH0)
		scaler_path_reset(PATH_1_RESET);
	else
		scaler_path_reset(PATH_0_RESET);

	scaler_path_reset(PATH_RAW0_LD_RESET);
	scaler_path_clk_control(PATH_RAW0_LD_CLK, CLOCK_ON);

	ret = scaler_configure(capture_scaler, &scl_dev.config, &scl_dev.data);
	if (ret < 0) {
		css_err("zsl_load scaler configure fail %x\n", ret);
		ret = CSS_ERROR_ZSL_LOAD_FAIL;
		goto out_fail;
	}

	phy_addr = (unsigned int)yuv_bufq->bufs[buf_index].dma_addr;
	if (phy_addr == 0) {
		css_err("zsl_load phy_addr err %x\n", phy_addr);
		ret = CSS_ERROR_ZSL_LOAD_FAIL;
		goto out_fail;
	}

	yuv_bufq->bufs[buf_index].state = CSS_BUF_CAPTURING;
	css_zsl("zsl read base: 0x%x\n", phy_addr);
	scaler_set_buffers(capture_scaler, &scl_dev.config, phy_addr);

	stime = ktime_get();

	if (zsl_dev->loadmode == ZSL_LOAD_DISCONTINUOUS_FRAME_MODE) {
		preload = 1;
	} else {
		if (zsl_dev->loadcnt == 0) {
			preload = 1;
		}
		zsl_dev->loadcnt++;
	}

	if (preload) {
		ret = scaler_bayer_load_configure((css_scaler_select)ZSL_READ_PATH0,
											&zsl_config);
		if (ret < 0) {
			css_err("zsl_load configure err %x\n", ret);
			ret = CSS_ERROR_ZSL_LOAD_FAIL;
			goto out_fail;
		}

		ret = scaler_bayer_load_frame_for_1ch(1, ZSL_READ_PATH0,
				CSS_SCALER_NONE);
		if (ret < 0) {
			css_err("zsl_load frame 1ch err none scale %x\n", ret);
			ret = CSS_ERROR_ZSL_LOAD_FAIL;
			goto out_fail;
		}

		capture_wait_done(PATH_RAW0_LD_DONE, 500);
	}

	ret = scaler_bayer_load_configure((css_scaler_select)ZSL_READ_PATH0,
				&zsl_config);
	if (ret < 0) {
		css_err("zsl_load configure err %x\n", ret);
		ret = CSS_ERROR_ZSL_LOAD_FAIL;
		goto out_fail;
	}

	ret = scaler_bayer_load_frame_for_1ch(1, ZSL_READ_PATH0, capture_scaler);
	if (ret < 0) {
		css_err("zsl_load frame 1ch err %x\n", ret);
		ret = CSS_ERROR_ZSL_LOAD_FAIL;
		goto out_fail;
	}

	do {
		int timeout = 0;

		timeout = bayer_load_capture_wait_done(zsl_dev,
							capture_get_done_mask(load_fh), 200);

		if (timeout > 0)
			break;

		css_info("load retry\n");

		if (r_path == ZSL_READ_PATH0)
			scaler_path_reset(PATH_1_RESET);
		else
			scaler_path_reset(PATH_0_RESET);

		scaler_path_reset(PATH_RAW0_LD_RESET);

		ret = scaler_configure(capture_scaler, &scl_dev.config, &scl_dev.data);
		if (ret < 0)
			css_warn("zsl_load scaler configure fail %x\n", ret);

		yuv_bufq->bufs[buf_index].state = CSS_BUF_CAPTURING;
		scaler_set_buffers(capture_scaler, &scl_dev.config, phy_addr);

		ret = scaler_bayer_load_configure((css_scaler_select)ZSL_READ_PATH0,
											&zsl_config);
		if (ret < 0)
			css_warn("zsl_load scaler configure fail %x\n", ret);

		ret = scaler_bayer_load_frame_for_1ch(1, ZSL_READ_PATH0,
					capture_scaler);
		if (ret < 0)
			css_warn("zsl_load frame 1ch err %x\n", ret);

		--retry;
	} while (retry > 0 && capture_get_stream_mode(load_fh));

	scaler_bayer_load_frame_for_1ch(0, ZSL_READ_PATH0, capture_scaler);

	if (retry > 0) {
		etime = ktime_get();
		delta = ktime_sub(etime, stime);
		duration = (unsigned long long) ktime_to_ns(delta) >> 10;

		css_info("zsl load_done[%d] [retry %d] %lld usecs\n",
				buf_index, retry, duration);

		yuv_bufq->bufs[buf_index].byteused
						= scaler_get_frame_size(capture_scaler);
		yuv_bufq->bufs[buf_index].width
						= scaler_get_frame_width(capture_scaler);
		yuv_bufq->bufs[buf_index].height
						= scaler_get_frame_height(capture_scaler);

		ret = CSS_SUCCESS;
	} else {
		css_info("zsl_load fail\n");
		ret = CSS_ERROR_ZSL_LOAD_FAIL;
		goto out_fail;
	}

	if (postview_enable) {

		zsl_dev->error = 0;
		phy_addr = (unsigned int)yuv_bufq->bufs[buf_index].dma_addr
								+ yuv_bufq->bufs[buf_index].offset;
		if (!phy_addr) {
			css_err("zsl_load post phy_addr err %x\n", phy_addr);
			ret = CSS_ERROR_ZSL_LOAD_FAIL;
			goto out_fail;
		}

		ret = capture_set_postview_config(postview, postview->scaler,
					&scl_dev.config.crop, phy_addr);
		if (ret < 0) {
			css_err("zsl_load set postview configure fail %x\n", ret);
			ret = CSS_ERROR_ZSL_LOAD_FAIL;
			goto out_fail;
		}

		ret = scaler_bayer_load_frame_for_1ch(1, ZSL_READ_PATH0,
					postview->scaler);
		if (ret < 0) {
			css_err("zsl_load postview frame 1ch err %x\n", ret);
			ret = CSS_ERROR_ZSL_LOAD_FAIL;
			goto out_fail;
		}

		retry = 5;
		do {
			int timeout = 0;
			timeout = bayer_load_capture_wait_done(zsl_dev,
							capture_get_done_mask(load_fh), 200);

			if (timeout > 0)
				break;

			if (postview->scaler == CSS_SCALER_0)
				scaler_path_reset(PATH_0_RESET);
			else
				scaler_path_reset(PATH_1_RESET);

			scaler_path_reset(PATH_RAW0_LD_RESET);

			ret = capture_set_postview_config(postview, postview->scaler,
						&scl_dev.config.crop, phy_addr);
			if (ret < 0)
				css_warn("zsl_load postview config err %x\n", ret);

			ret = scaler_bayer_load_frame_for_1ch(1, ZSL_READ_PATH0,
						postview->scaler);
			if (ret < 0)
				css_warn("zsl_load postview frame 1ch err %x\n", ret);

			--retry;
		} while (retry > 0 && capture_get_stream_mode(load_fh));

		scaler_bayer_load_frame_for_1ch(0, ZSL_READ_PATH0, postview->scaler);

		if (retry > 0) {
			css_info("post zsl load_done[%d] [retry %d]\n", buf_index, retry);
			ret = CSS_SUCCESS;

		} else {
			css_info("post zsl_load fail\n");
			ret = CSS_ERROR_ZSL_LOAD_FAIL;
			goto out_fail;
		}
	}

	bayer_buffer_release_frame(bayer_bufq, cssbuf);

	if (capture_get_path_mode(load_fh->cssdev) == CSS_NORMAL_PATH_MODE)
		bayer_isp_read_path_disable(ZSL_READ_PATH0);

	scaler_path_clk_control(PATH_RAW0_LD_CLK, CLOCK_OFF);

	if (yuv_bufq->bufs[buf_index].state == CSS_BUF_CAPTURING) {
		yuv_bufq->bufs[buf_index].state = CSS_BUF_CAPTURE_DONE;
		/* do_gettimeofday(&bufq->bufs[buf_index].timestamp); */
		capture_buffer_push(yuv_bufq, buf_index);

		wake_up_interruptible(&yuv_bufq->complete.wait);

		if (yuv_bufq->mode == STREAM_MODE) {
			buf_index = capture_buffer_get_next_index(yuv_bufq);
			if (buf_index >= 0) {
				unsigned long flags;
				spin_lock_irqsave(&yuv_bufq->lock, flags);
				yuv_bufq->index = buf_index;
				spin_unlock_irqrestore(&yuv_bufq->lock, flags);
			} else {
				int i = 0;
				css_info("zsl load queue full!!\n");
				for (i = 0; i < yuv_bufq->count; i++)
					css_zsl("buf[%d] state %d\n",
							i, yuv_bufq->bufs[i].state);
				ret = CSS_ERROR_ZSL_LOAD_FAIL;
				goto out;
			}
		}
	} else {
		css_err("zsl buf not capturing(%d)(%d)!!\n",
				buf_index, yuv_bufq->bufs[buf_index].state);
		ret = CSS_ERROR_ZSL_LOAD_FAIL;
		goto out;
	}

	complete(&zsl_dev->load_done);
	zsl_dev->load_running = 0;
	mutex_unlock(&load_fh->cssdev->bayer_mutex);

	return CSS_SUCCESS;;

out_fail:
	bayer_buffer_release_frame(bayer_bufq, cssbuf);
	if (capture_get_path_mode(load_fh->cssdev) == CSS_NORMAL_PATH_MODE)
		bayer_isp_read_path_disable(ZSL_READ_PATH0);

	scaler_path_clk_control(PATH_RAW0_LD_CLK, CLOCK_OFF);

out:
	zsl_dev->load_running = 0;
	zsl_dev->loadcnt = 0;
	complete(&zsl_dev->load_done);
	mutex_unlock(&load_fh->cssdev->bayer_mutex);

	return ret;
}

void bayer_fd_open(int *fd, unsigned int *size, unsigned int *align)
{
	struct ion_handle_data *hdl_data = NULL;
	struct ion_handle *handle = NULL;
	struct ion_client *client = NULL;
	int filediscriptor = -1;
	struct dma_buf *dmabuf;

	if (bayer_ion_handle_data && bayer_ion_client) {
		hdl_data = (struct ion_handle_data *)bayer_ion_handle_data;
		handle = hdl_data->handle;
		client = (struct ion_client *)bayer_ion_client;

		dmabuf = ion_share_dma_buf(client, handle);
		if (!IS_ERR(dmabuf)) {
			filediscriptor = dma_buf_fd(dmabuf, O_CLOEXEC);
			if (filediscriptor < 0)
				dma_buf_put(dmabuf);
		} else {
			filediscriptor = -1;
		}

		*fd = filediscriptor;
		*size = bayer_ion_size;
		*align = bayer_ion_align_size;

		css_zsl("open Bayer: fd=%d handle=0x%x client=0x%x\n",
				 filediscriptor, (unsigned int)handle, (unsigned int)client);
	}
}

void bayer_fd_release(void)
{
	bayer_ion_handle_data	= NULL;
	bayer_ion_client		= NULL;
	bayer_ion_size			= 0;
	bayer_ion_align_size	= 0;
	css_zsl("release Bayer\n");
}

extern struct capture_fh *g_capfh[];

void bayer_work_loadpath0(struct work_struct *work)
{
	int ret = CSS_SUCCESS;
	struct css_bufq *bufq = NULL;
	struct capture_fh *capfh = g_capfh[CSS_SCALER_0];

	if (capfh == NULL)
		return;

	if (capture_get_stream_mode(capfh)) {
		bufq = capture_buffer_get_pointer(capfh->fb, capfh->buf_attrb.buf_type);
		if (bufq == NULL) {
			css_err("zsl work 0: bufq is NULL\n");
			return;
		}

		if (bufq->ready == 0) {
			css_err("zsl work 0: bufq is not ready\n");
			return;
		}

		if (bufq->bufs == NULL) {
			css_err("zsl work 0: bufq->bufs is NULL\n");
			return;
		}

		css_zsl_low("zsl-0 load workq\n");

		mutex_lock(&capfh->v4l2_mutex);
		ret = bayer_load_frame(capfh);
		if (ret < 0) {
			bufq->error = 1;
			wake_up_interruptible(&bufq->complete.wait);
		}
		mutex_unlock(&capfh->v4l2_mutex);
	}
}

void bayer_work_loadpath1(struct work_struct *work)
{
	int ret = CSS_SUCCESS;
	struct capture_fh *capfh = g_capfh[CSS_SCALER_1];
	struct css_bufq *bufq = NULL;

	if (capfh == NULL)
		return;

	if (capture_get_stream_mode(capfh)) {
		bufq = capture_buffer_get_pointer(capfh->fb, capfh->buf_attrb.buf_type);
		if (bufq == NULL) {
			css_err("zsl work 1: bufq is NULL\n");
			return;
		}

		if (bufq->ready == 0) {
			css_err("zsl work 1: bufq is not ready\n");
			return;
		}

		if (bufq->bufs == NULL) {
			css_err("zsl work 1: bufq->bufs is NULL\n");
			return;
		}

		css_zsl_low("zsl-1 load workq\n");

		mutex_lock(&capfh->v4l2_mutex);
		ret = bayer_load_frame(capfh);
		if (ret < 0) {
			bufq->error = 1;
			wake_up_interruptible(&bufq->complete.wait);
		}
		mutex_unlock(&capfh->v4l2_mutex);
	}
}

int bayer_store_int_handler(struct capture_fh *capfh)
{
	int ret = CSS_SUCCESS;
	int capture_index = 0;
	int next_capture_index = 0;
	unsigned long flags = 0;
	struct css_zsl_device *zsl_dev = NULL;
	struct css_zsl_config *zsl_config = NULL;
	struct css_bufq *bufq = NULL;

	zsl_dev = capture_get_zsl_dev(capfh, capfh->main_scaler);
	zsl_config = &zsl_dev->config;

	/* if (zsl_config->enable == 0) { */
		complete(&zsl_dev->store_done);
	/* } */

	switch (capfh->main_scaler) {
	case CSS_SCALER_0:
		scaler_int_disable(INT_PATH_RAW0_STORE);
		scaler_int_clear(INT_PATH_RAW0_STORE);
		break;
	case CSS_SCALER_1:
		scaler_int_disable(INT_PATH_RAW1_STORE);
		scaler_int_clear(INT_PATH_RAW1_STORE);
		break;
	default:
		break;
	}

	if (zsl_config->enable) {
		/* struct timespec ts; */
		bufq = &zsl_dev->raw_buf;
		if (!bufq->bufs) {
			ret = CSS_ERROR_ZSL_BUF_READY;
			goto out;
		}

		capture_index = bayer_buffer_get_capture_index(bufq);

		if (capture_get_path_mode(capfh->cssdev) == CSS_LIVE_PATH_MODE)
			bufq->bufs[capture_index].frame_cnt
						= odin_isp_get_vsync_count(ISP1);
		else
			bufq->bufs[capture_index].frame_cnt =
						odin_isp_get_vsync_count(capfh->main_scaler);

		do_gettimeofday(&bufq->bufs[capture_index].timestamp);
		/*
		 * bufq->bufs[capture_index].timestamp.tv_sec = (long)ts.tv_sec;
		 * bufq->bufs[capture_index].timestamp.tv_usec = ts.tv_nsec;
		 */

		next_capture_index = bayer_buffer_get_next_index(zsl_dev, bufq,
										capture_index);

		css_zsl_low("zsl[%d] %d->%d\n", zsl_dev->path, capture_index,
										next_capture_index);

		spin_lock_irqsave(&bufq->lock, flags);
		bufq->capt_cnt++;
		spin_unlock_irqrestore(&bufq->lock, flags);

		/* start next zsl frame */
		bufq->bufs[next_capture_index].state = CSS_BUF_CAPTURING;
		zsl_config->mem.base32 = bufq->bufs[next_capture_index].dma_addr;
		zsl_config->next_capture_index = next_capture_index;
		scaler_bayer_store_reset(zsl_dev->path);
		if (next_capture_index == 0) {
			struct css_zsl_config zconfig;
			memcpy(&zconfig, zsl_config, sizeof(struct css_zsl_config));
			zconfig.img_size.height = ZSL_STORE_DROP_CAPTURE_LINE;
			ret = scaler_bayer_store_configure(zsl_dev->path, &zconfig);
		} else {
			ret = scaler_bayer_store_configure(zsl_dev->path, zsl_config);
		}

		if (ret == CSS_SUCCESS)
			ret = scaler_bayer_store_capture(zsl_dev->path, zsl_config);
	}

out:
	return ret;
}


