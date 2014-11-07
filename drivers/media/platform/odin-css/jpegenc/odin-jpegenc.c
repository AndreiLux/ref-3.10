/*
 * JPEG Encoder driver
 *
 * Copyright (C) 2013 Mtekvision
 * Author: Eonkyong Lee <tiny@mtekvision.com>
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

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <linux/poll.h>
#include <linux/slab.h>
#if CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif
#include <media/v4l2-ioctl.h>

#include "../odin-css.h"
#include "../odin-css-utils.h"

#include "odin-jpegenc-codec.h"
#include "odin-jpegenc.h"

#define ODIN_JPEGENC_MINOR 1
/* #define USE_PMEM */
#define USE_ION
#ifdef USE_PMEM
/* #define TEST_IMAGE_FROM_USER */
#endif

int odin_jpegenc_config(struct jpegenc_fh *jpeg_fh)
{
	int result = CSS_SUCCESS;
	struct jpegenc_device *jpeg_dev = NULL;

	if (!jpeg_fh) {
		css_err("jpeg_fh is null\n");
		return CSS_ERROR_JPEG_FILE_IS_NULL;
	}

	jpeg_dev = jpeg_fh->jpeg_dev;
	if (!jpeg_dev) {
		css_err("jpeg_dev is null\n");
		return CSS_ERROR_JPEG_DEV_IS_NULL;
	}

	result = jpegenc_configuration(jpeg_dev);
	if (result < 0)
		css_err("jpegenc_configuration fail = %d\n", result);

	return result;
}

int odin_jpegenc_clear(struct jpegenc_fh *jpeg_fh)
{
	struct jpegenc_platform *jpeg_plat = NULL;

	if (!jpeg_fh) {
		css_err("jpeg_fh is null\n");
		return CSS_ERROR_JPEG_FILE_IS_NULL;
	}

	jpeg_plat = jpeg_fh->jpeg_plat;
	if (!jpeg_plat) {
		css_err("jpeg_plat is null\n");
		return CSS_ERROR_JPEG_PLAT_IS_NULL;
	}

	spin_lock(&jpeg_plat->hwlock);	/* h/w controller lock */
	jpegenc_clear();
	spin_unlock(&jpeg_plat->hwlock);

	return CSS_SUCCESS;
}

int odin_jpegenc_start(struct jpegenc_fh *jpeg_fh)
{
	struct jpegenc_device *jpeg_dev = NULL;
	struct jpegenc_platform *jpeg_plat = NULL;
	int result = CSS_SUCCESS;

	if (!jpeg_fh) {
		css_err("jpeg_fh is null\n");
		return CSS_ERROR_JPEG_FILE_IS_NULL;
	}

	jpeg_dev = jpeg_fh->jpeg_dev;
	jpeg_plat = jpeg_fh->jpeg_plat;
	if (!jpeg_dev || !jpeg_plat) {
		css_err("jpeg enc does not ready\n");
		return CSS_ERROR_JPEG_DOES_NOT_READY;
	}

	spin_lock(&jpeg_plat->hwlock);	/* h/w controller lock */
	result = jpegenc_start(jpeg_dev);
	spin_unlock(&jpeg_plat->hwlock);
	if (result < 0)
		css_err("jpegenc_start fail = %d\n",result);

	return result;
}

int odin_jpegenc_v4l2_s_ctrl(struct file *file, void *fh,
				struct v4l2_control *c)
{
	struct jpegenc_fh *jpeg_fh = file->private_data;
	int result = CSS_SUCCESS;

	if (!jpeg_fh) {
		css_err("jpeg_fh is null\n");
		return CSS_ERROR_JPEG_FILE_IS_NULL;
	}

	if (!jpeg_fh->jpeg_dev) {
		css_err("jpeg_dev is null\n");
		return CSS_ERROR_JPEG_DEV_IS_NULL;
	}

	switch (c->id) {
	case V4L2_CID_CSS_JPEG_ENC_SET_READ_DELAY:
		jpeg_fh->jpeg_dev->config_data.read_delay = c->value;
		css_jpeg_enc("jpeg enc rd delay = %d\n",
						jpeg_fh->jpeg_dev->config_data.read_delay);
		break;
	default:
		css_err("invaild ctrl\n");
		result = CSS_ERROR_JPEG_INVALID_CTRL;
		break;
	}

	return result;
}

int odin_jpegenc_v4l2_g_ctrl(struct file *file, void *fh,
				struct v4l2_control *c)
{
	struct jpegenc_fh *jpeg_fh = file->private_data;
	unsigned long jpeg_size = 0;
	int result = CSS_SUCCESS;

	if (!jpeg_fh) {
		css_err("jpeg_fh is null\n");
		return CSS_ERROR_JPEG_FILE_IS_NULL;
	}

	if (!jpeg_fh->jpeg_dev) {
		css_err("jpeg_dev is null\n");
		return CSS_ERROR_JPEG_DEV_IS_NULL;
	}

	switch (c->id) {
	case V4L2_CID_CSS_JPEG_ENC_SIZE:
		jpeg_size = jpegenc_get_output_size();
		jpeg_fh->jpeg_dev->codec_hw_config.jpeg_size = jpeg_size;
		c->value = jpeg_size;
		css_jpeg_enc("jpeg enc size = %d\n",jpeg_size);
		break;
	case V4L2_CID_CSS_JPEG_ENC_ERROR:
		css_info("jpeg enc error = %d\n",jpeg_fh->jpeg_dev->error_num);
		c->value = jpeg_fh->jpeg_dev->error_num;
		/* jpeg_fh->jpeg_dev->error_num = 0; */
		break;
	default:
		css_err("invaild ctrl\n");
		result = CSS_ERROR_JPEG_INVALID_CTRL;
		break;
	}

	return result;
}

static int odin_jpegenc_v4l2_streamoff(struct file *file, void *fh,
				enum v4l2_buf_type type)
{
	struct jpegenc_fh *jpeg_fh = file->private_data;
	struct jpegenc_platform *jpeg_plat = NULL;
	int result = CSS_SUCCESS;

	css_info("odin_jpegenc_v4l2_streamoff\n");

	if (!jpeg_fh) {
		css_err("jpeg_fh is null\n");
		return CSS_ERROR_JPEG_FILE_IS_NULL;
	}

	jpeg_plat = jpeg_fh->jpeg_plat;
	if (!jpeg_plat) {
		css_err("jpeg_plat is null\n");
		return CSS_ERROR_JPEG_PLAT_IS_NULL;
	}

	spin_lock(&jpeg_plat->hwlock);	/* h/w controller lock */
	jpegenc_interrupt_disable_all();
	spin_unlock(&jpeg_plat->hwlock);
	css_info("jpegenc_interrupt_disable_all\n");

	result = odin_jpegenc_clear(jpeg_fh);
	if (result < 0) {
		css_err("odin_jpegenc_clear fail= %d\n",result);
		return result;
	}

	return CSS_SUCCESS;
}

static int odin_jpegenc_v4l2_streamon(struct file *file, void *fh,
				enum v4l2_buf_type type)
{
	struct jpegenc_fh *jpeg_fh = file->private_data;
	struct jpegenc_platform *jpeg_plat = NULL;
	int result = CSS_SUCCESS;

	css_info("odin_jpegenc_v4l2_streamon\n");

	if (!jpeg_fh) {
		css_err("jpeg_fh is null\n");
		return CSS_ERROR_JPEG_FILE_IS_NULL;
	}

	jpeg_plat = jpeg_fh->jpeg_plat;
	if (!jpeg_plat) {
		css_err("jpeg_plat is null\n");
		return CSS_ERROR_JPEG_PLAT_IS_NULL;
	}

	result = odin_jpegenc_config(jpeg_fh);
	if (result < 0) {
		css_err("odin_jpegenc_config fail=%d\n",result);
		return result;
	}

	result = odin_jpegenc_clear(jpeg_fh);
	if (result < 0) {
		css_err("odin_jpegenc_clear fail= %d\n",result);
		return result;
	}

	spin_lock(&jpeg_plat->hwlock);
	jpegenc_interrupt_enable_all();
	spin_unlock(&jpeg_plat->hwlock);

	result = odin_jpegenc_start(jpeg_fh);
	if (result < 0) {
		css_err("odin_jpegenc_start fail= %d\n",result);
		return result;
	}

	return CSS_SUCCESS;
}

static int odin_jpegenc_v4l2_reqbufs(struct file *file, void *fh,
				struct v4l2_requestbuffers *req)
{
	struct jpegenc_fh *jpeg_fh = file->private_data;
	struct jpegenc_device *jpeg_dev = NULL;
	struct jpegenc_platform *jpeg_plat = NULL;

	if (!jpeg_fh) {
		css_err("jpeg_fh is null\n");
		return CSS_ERROR_JPEG_FILE_IS_NULL;
	}

	jpeg_dev = jpeg_fh->jpeg_dev;
	jpeg_plat = jpeg_fh->jpeg_plat;
	if (!jpeg_dev || !jpeg_plat) {
		css_err("jpeg enc does not ready\n");
		return CSS_ERROR_JPEG_DOES_NOT_READY;
	}

	if (req->memory == V4L2_MEMORY_USERPTR) {
		if (req->count == 0) {
			if (jpeg_plat->input_buf.alloc == 1) {
				if (jpeg_plat->input_buf.ion_client) {
					if (jpeg_plat->input_buf.cpu_addr &&
						jpeg_plat->input_buf.ion_hdl.handle) {
						odin_ion_unmap_kernel_with_condition(
							jpeg_plat->input_buf.ion_hdl.handle);
						jpeg_plat->input_buf.cpu_addr = NULL;
					}
					css_sys("jpeg_in_client destroy\n");
					odin_ion_client_destroy(jpeg_plat->input_buf.ion_client);
					jpeg_plat->input_buf.ion_client = NULL;
				}
				jpeg_plat->input_buf.alloc = 0;
			}
			memset(&jpeg_plat->input_buf,0,sizeof(struct jpegenc_buffer));
		} else {
			if (jpeg_plat->input_buf.ion_client == NULL) {
				jpeg_plat->input_buf.ion_client =
										odin_ion_client_create("jpeg_in_ion");
				if (jpeg_plat->input_buf.ion_client == NULL) {
					css_err("jpeg_in_client create fail\n");
					return CSS_FAILURE;
				} else {
					css_sys("jpeg_in_client create success\n");
				}
			}
		}

		if (req->count == 0) {
			if (jpeg_plat->output_buf.alloc == 1) {
				if (jpeg_plat->output_buf.ion_client) {
					if (jpeg_plat->output_buf.cpu_addr &&
						jpeg_plat->output_buf.ion_hdl.handle) {
						ion_sync_for_device(jpeg_plat->output_buf.ion_client,
							jpeg_plat->output_buf.ion_fd);
						odin_ion_unmap_kernel_with_condition(
							jpeg_plat->output_buf.ion_hdl.handle);
						jpeg_plat->output_buf.cpu_addr = NULL;
					}
					css_sys("jpeg_out_client destroy\n");
					odin_ion_client_destroy(jpeg_plat->output_buf.ion_client);
					jpeg_plat->output_buf.ion_client = NULL;
				}
				jpeg_plat->output_buf.alloc = 0;
			}
			memset(&jpeg_plat->output_buf,0,sizeof(struct jpegenc_buffer));
		} else {
			if (jpeg_plat->output_buf.ion_client == NULL) {
				jpeg_plat->output_buf.ion_client =
										odin_ion_client_create("jpeg_out_ion");
				if (jpeg_plat->output_buf.ion_client == NULL) {
					css_err("jpeg_out_client create fail\n");
					return CSS_FAILURE;
				} else {
					css_sys("jpeg_out_client create success\n");
				}
			}
		}
	} else {
#ifdef USE_PMEM
		if (req->count == 1) {
			if (jpeg_plat->output_buf.alloc == 0) {
				jpeg_plat->output_buf.dma_addr = jpeg_plat->pmem_base;
				jpeg_plat->output_buf.size = jpeg_dev->config_data.outbufsize;
				jpeg_plat->output_buf.cpu_addr = (void *)ioremap_nocache(
								(unsigned long)(jpeg_plat->output_buf.dma_addr),
								(int)jpeg_plat->output_buf.size);
				jpeg_plat->output_buf.alloc = 1;
				jpeg_dev->config_data.output_buf =
								jpeg_plat->output_buf.dma_addr;
			}
#ifdef TEST_IMAGE_FROM_USER
			if (jpeg_plat->input_buf.alloc == 0) {
				jpeg_plat->input_buf.dma_addr =
					jpeg_plat->output_buf.dma_addr + jpeg_plat->output_buf.size;
				jpeg_plat->input_buf.size = JPEGENC_BUFFER_SIZE;
				/*jpeg_dev->config_data.width *jpeg_dev->config_data.height*2;*/
				jpeg_plat->input_buf.cpu_addr = (void *)ioremap_nocache(
								(unsigned long)(jpeg_plat->input_buf.dma_addr),
								(int)jpeg_plat->input_buf.size);

				jpeg_plat->input_buf.alloc = 1;
				jpeg_dev->config_data.input_buf = jpeg_plat->input_buf.dma_addr;

			}
#endif
		} else if (req->count == 0) {
#ifdef TEST_IMAGE_FROM_USER
			if (jpeg_plat->input_buf.alloc == 1) {
				iounmap(jpeg_plat->input_buf.cpu_addr);
				jpeg_plat->input_buf.alloc = 0;
			}
#endif
			if (jpeg_plat->output_buf.alloc == 1) {
				iounmap(jpeg_plat->output_buf.cpu_addr);
				jpeg_plat->output_buf.alloc = 0;
			}
		}
#endif /* #ifdef USE_PMEM */

	}

	return CSS_SUCCESS;
}

static int odin_jpegenc_v4l2_querybuf(struct file *file, void *fh,
				struct v4l2_buffer *buf)
{
	struct jpegenc_fh *jpeg_fh = file->private_data;
	struct jpegenc_platform *jpeg_plat = NULL;
	int result = CSS_SUCCESS;
	unsigned int buf_state = 0;

	if (!jpeg_fh) {
		css_err("jpeg_fh is null\n");
		return CSS_ERROR_JPEG_FILE_IS_NULL;
	}

	jpeg_plat = jpeg_fh->jpeg_plat;
	if (!jpeg_plat) {
		css_err("jpeg_plat is null\n");
		return CSS_ERROR_JPEG_PLAT_IS_NULL;
	}

	if (buf->memory == V4L2_MEMORY_USERPTR) {
		css_err("querybuf don't support v4l2 userptr!\n");
		return -EINVAL;
	}

	if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		buf->m.offset = (unsigned int)jpeg_plat->output_buf.dma_addr;
		buf->length = jpeg_plat->output_buf.size;
		buf_state = jpeg_plat->output_buf.state;
	}
#ifdef TEST_IMAGE_FROM_USER
	else if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		buf->m.offset = (unsigned int)jpeg_plat->input_buf.dma_addr;
		buf->length = jpeg_plat->input_buf.size;
		buf_state = jpeg_plat->input_buf.state;
	}
#endif
	else {
		result = -EINVAL;
	}

	if (buf_state == JPEGENC_BUF_USING)
		buf->flags = V4L2_BUF_FLAG_QUEUED;
	else if (buf_state == JPEGENC_BUF_DONE)
		buf->flags = V4L2_BUF_FLAG_DONE;

	buf->flags = V4L2_BUF_FLAG_MAPPED;
	/* buf->memory = V4L2_MEMORY_MMAP;*/

	return result;
}

static int odin_jpegenc_v4l2_qbuf(struct file *file, void *fh,
				struct v4l2_buffer *buf)
{
	struct jpegenc_fh *jpeg_fh = file->private_data;
	struct jpegenc_device *jpeg_dev = NULL;
	struct jpegenc_platform *jpeg_plat = NULL;
#if CONFIG_ODIN_ION_SMMU
	struct ion_handle *handle = NULL;
	int fd = -1;
#endif
	unsigned int offset = 0;
	unsigned int mapsize;
	unsigned int src, dst;
	unsigned int diff;
	unsigned int multipleof8;
	unsigned int proc_lines;
	unsigned int clr_lines;

	if (!jpeg_fh) {
		css_err("jpeg_fh is null\n");
		return CSS_ERROR_JPEG_FILE_IS_NULL;
	}

	jpeg_dev = jpeg_fh->jpeg_dev;
	jpeg_plat = jpeg_fh->jpeg_plat;
	if (!jpeg_dev || !jpeg_plat) {
		css_err("jpeg enc does not ready\n");
		return CSS_ERROR_JPEG_DOES_NOT_READY;
	}

	if (buf->type != V4L2_BUF_TYPE_VIDEO_CAPTURE &&
			buf->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		css_err("invalid buf type %d!\n", buf->type);
		return -EINVAL;
	}

	/*
	if (buf->memory != V4L2_MEMORY_MMAP) {
		printk("invalid memory type %d!\n", buf->memory);
		return -EINVAL;
	}
	*/

	if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		jpeg_plat->output_buf.state = JPEGENC_BUF_UNUSED;
		if (buf->memory == V4L2_MEMORY_USERPTR) {
			if (jpeg_plat->output_buf.alloc == 0) {
#if CONFIG_ODIN_ION_SMMU
				fd = (int)buf->m.fd;
				if (fd < 0) {
					css_err("jpeg_enc ion fd error\n");
					return -EINVAL;
				}

				if (jpeg_plat->output_buf.ion_client == NULL) {
					css_err("ion_client(out) NULL\n");
					return -EINVAL;
				}
				jpeg_plat->output_buf.ion_fd = fd;

				handle = ion_import_dma_buf(jpeg_plat->output_buf.ion_client,
									fd);

				if (IS_ERR_OR_NULL(handle)) {
					css_err("can't get import_dma_buf handle(%p) %p (%d)\n",
								handle, jpeg_plat->output_buf.ion_client, fd);
					css_get_fd_info(fd);
					return -EINVAL;
				}
				css_sys("import jepg out fd: %d\n", fd);

				jpeg_plat->output_buf.ion_hdl.handle = handle;
				jpeg_plat->output_buf.dma_addr =
						(dma_addr_t)odin_ion_get_iova_of_buffer(handle,
													ODIN_SUBSYS_CSS);
				jpeg_plat->output_buf.size = odin_ion_get_buffer_size(handle);
				jpeg_dev->config_data.outbufsize = jpeg_plat->output_buf.size;
				css_jpeg_enc("jpeg output %d \n",jpeg_plat->output_buf.size);

				jpeg_plat->output_buf.cpu_addr
					= (void *) odin_ion_map_kernel_with_condition(
							handle,
							0,
							PAGE_ALIGN(JPEGENC_FOR_HEADER_SIZE));
				if (jpeg_plat->output_buf.cpu_addr) {
					css_jpeg_enc("jpeg ion_map_kernel = 0x%lx\n",
							jpeg_plat->output_buf.cpu_addr);
				} else {
					css_sys("jpeg_out_client destroy\n");
					odin_ion_client_destroy(jpeg_plat->output_buf.ion_client);
					jpeg_plat->output_buf.ion_client = NULL;
					css_err("jpeg cpu_addr is NULL\n");
					return CSS_ERROR_JPEG_BUF_ALLOC_FAIL;
				}
#else
				jpeg_plat->output_buf.dma_addr = buf->m.userptr;
#endif
				jpeg_plat->output_buf.alloc = 1;
				jpeg_dev->config_data.output_buf =
								jpeg_plat->output_buf.dma_addr;
			}
		}
	} else if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
#ifdef TEST_IMAGE_FROM_USER
		jpeg_plat->input_buf.state = JPEGENC_BUF_UNUSED;
#endif
		if (buf->memory == V4L2_MEMORY_USERPTR) {
			if (jpeg_plat->input_buf.alloc == 0) {
#if CONFIG_ODIN_ION_SMMU
				fd = (int)buf->m.fd;
				if (fd < 0) {
					css_err("ion fd error\n");
					return -EINVAL;
				}
				if (jpeg_plat->input_buf.ion_client == NULL) {
					css_err("ion_client(in) NULL\n");
					return -EINVAL;
				}
				jpeg_plat->input_buf.ion_fd = fd;

				handle = ion_import_dma_buf(jpeg_plat->input_buf.ion_client,
											fd);
				if (IS_ERR_OR_NULL(handle)) {
					css_err("can't get import_dma_buf handle(%p) %p (%d)\n",
								handle, jpeg_plat->input_buf.ion_client, fd);
					css_get_fd_info(fd);
					return -EINVAL;
				}
				css_sys("import jpeg in fd: %d\n", fd);
				jpeg_plat->input_buf.ion_hdl.handle = handle;

				jpeg_plat->input_buf.dma_addr =
						(dma_addr_t)odin_ion_get_iova_of_buffer(handle,
													ODIN_SUBSYS_CSS);
				jpeg_plat->input_buf.size = odin_ion_get_buffer_size(handle);

				css_jpeg_enc("modify header %d\n", jpeg_dev->modify_header);

				if (jpeg_dev->modify_header) {
					diff = jpeg_dev->config_data.height
						- jpeg_dev->backup_height;

					if (diff % 8) {
						multipleof8 = 0;
						proc_lines = (diff % 8);
					} else {
						multipleof8 = 1;
						proc_lines = diff;
					}

					offset = (jpeg_dev->config_data.width * 2 *
								(jpeg_dev->backup_height - proc_lines));

					mapsize = jpeg_dev->config_data.width * 2 * (diff * 2);
					jpeg_plat->input_buf.cpu_addr
						= (void *) odin_ion_map_kernel_with_condition(
								handle,
								offset,
								PAGE_ALIGN(mapsize));
					if (jpeg_plat->input_buf.cpu_addr) {
						src = (unsigned int)jpeg_plat->input_buf.cpu_addr;
						dst = src +
							(jpeg_dev->config_data.width * 2 * proc_lines);
						clr_lines = (jpeg_dev->config_data.width * 2 * 8);
						if (multipleof8) {
							/* multiple of 8, makes clear to zero */
							memset(dst, 0x0, clr_lines);
							css_jpeg_enc("memset dst:0x%08x++%d\n",
								dst, clr_lines);
						} else {
							/* 8 lines clear to zero, other lines copy */
							memcpy(dst, src,
								jpeg_dev->config_data.width * 2 * proc_lines);
							css_jpeg_enc("cpy src: 0x%08x dst:0x%08x sz=%ld\n",
								src, dst,
								jpeg_dev->config_data.width * 2 * proc_lines);
							dst = src + clr_lines;
							memset(dst, 0x0, clr_lines);
							css_jpeg_enc("memset dst:0x%08x++%d\n",
									dst, clr_lines);
						}
						ion_sync_for_device(jpeg_plat->input_buf.ion_client,
							jpeg_plat->input_buf.ion_fd);
						css_jpeg_enc("backup height=%d multiple-8=%d "
							"lines=%d offset height=%d mapsize=%d diff=%d\n",
							jpeg_dev->backup_height, multipleof8, proc_lines,
							offset/(jpeg_dev->config_data.width * 2),
							mapsize, diff);
					} else {
						css_sys("jpeg_in_client destroy\n");
						odin_ion_client_destroy(jpeg_plat->input_buf.ion_client);
						jpeg_plat->input_buf.ion_client = NULL;
						css_err("jpeg cpu_addr is NULL\n");
						return CSS_ERROR_JPEG_BUF_ALLOC_FAIL;
					}
				}
				css_jpeg_enc("jpeg input %d \n\n",jpeg_plat->input_buf.size);
#else
				jpeg_plat->input_buf.dma_addr = buf->m.userptr;
#endif
				jpeg_plat->input_buf.alloc = 1;
				jpeg_dev->config_data.input_buf = jpeg_plat->input_buf.dma_addr;
			}
		}
	}
	buf->flags |= V4L2_BUF_FLAG_QUEUED;
	buf->flags &= ~V4L2_BUF_FLAG_DONE;

	return CSS_SUCCESS;
}

/* get new frame buffer from driver and user */
static int odin_jpegenc_v4l2_dqbuf(struct file *file, void *fh,
				struct v4l2_buffer *buf)
{
	struct jpegenc_fh *jpeg_fh = file->private_data;
	struct jpegenc_platform *jpeg_plat = NULL;
	unsigned int buf_state = 0;

	if (!jpeg_fh) {
		css_err("jpeg_fh is null\n");
		return CSS_ERROR_JPEG_FILE_IS_NULL;
	}

	jpeg_plat = jpeg_fh->jpeg_plat;
	if (!jpeg_plat) {
		css_err("jpeg_plat is null\n");
		return CSS_ERROR_JPEG_PLAT_IS_NULL;
	}

	if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		buf_state = jpeg_plat->output_buf.state;
	}
#ifdef TEST_IMAGE_FROM_USER
	else if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		buf_state = jpeg_plat->input_buf.state;
	}
#endif
	if (buf_state != JPEGENC_BUF_DONE) {
		css_err("invalid buf state %d!\n", buf_state);
		return -EINVAL;
	}

	buf->bytesused = jpeg_plat->output_buf.size;

#if defined USE_ION
#if CONFIG_ODIN_ION_SMMU
	buf->m.fd = (u32)jpeg_plat->output_buf.ion_fd;
#else
	buf->m.userptr = (u32)jpeg_plat->output_buf.dma_addr;
#endif
#elif defined USE_PMEM
	buf->memory = V4L2_MEMORY_MMAP;
	buf->m.offset = (u32)jpeg_plat->output_buf.dma_addr;
#endif
	buf->length = jpeg_plat->output_buf.size;

	return CSS_SUCCESS;
}

static int odin_jpegenc_v4l2_g_fmt_vid_out(struct file *file, void *priv,
					struct v4l2_format *fmt)
{	/*input_buf*/
	return CSS_SUCCESS;
}

static int odin_jpegenc_v4l2_g_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *fmt)

{	/*output_buf*/
	return CSS_SUCCESS;
}

static int odin_jpegenc_v4l2_s_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *fmt)
{	/*output_buf*/
	struct jpegenc_fh *jpeg_fh = file->private_data;
	struct jpegenc_device *jpeg_dev = NULL;
	struct v4l2_pix_format *pix;

	if (!jpeg_fh) {
		css_err("jpeg_fh is null\n");
		return CSS_ERROR_JPEG_FILE_IS_NULL;
	}

	jpeg_dev = jpeg_fh->jpeg_dev;
	if (!jpeg_dev) {
		css_err("jpeg_dev is null\n");
		return CSS_ERROR_JPEG_DEV_IS_NULL;
	}

	if (!fmt) {
		css_err("invalid config value");
		return CSS_ERROR_JPEG_INVALID_CONFIG;
	}
	pix = &fmt->fmt.pix;

	if ( pix->width == 0 || pix->height == 0 || pix->sizeimage == 0) {
		css_err("invalid size w=%d, h=%d, size=%d\n",
				pix->width, pix->height, pix->sizeimage);
		return CSS_ERROR_JPEG_INVALID_SIZE;
	}

	switch (pix->pixelformat) {
	case V4L2_PIX_FMT_RGB565:
		jpeg_dev->config_data.src_format = CSS_COLOR_RGB565;
		break;
	case V4L2_PIX_FMT_YUV420:
		jpeg_dev->config_data.src_format = CSS_COLOR_YUV420;
		break;
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YUV422P:
		jpeg_dev->config_data.src_format = CSS_COLOR_YUV422;
		break;
	default:
		css_err("unsupported jpegenc_format\n");
		return CSS_ERROR_JPEG_UNSUPPORTED_PIXELFORMAT;
	}

	if (pix->height % 8 != 0) {
		css_jpeg_enc("jpeg enc height = %d\n", pix->height);
		jpeg_dev->backup_height = pix->height;
		pix->height = pix->height + (pix->height % 8) + 8;
		jpeg_dev->modify_header = 1;
	} else {
		css_jpeg_enc("jpeg enc height = %d\n", pix->height);
		jpeg_dev->backup_height = pix->height;
		pix->height = pix->height + 8;
		jpeg_dev->modify_header = 1;
	}

	jpeg_dev->config_data.width = pix->width;
	jpeg_dev->config_data.height = pix->height;
#ifdef USE_PMEM
	jpeg_dev->config_data.outbufsize = pix->sizeimage;
#endif
	css_jpeg_enc("w = %d ,h = %d \n",
		jpeg_dev->config_data.width, jpeg_dev->config_data.height);

	return CSS_SUCCESS;
}

static int odin_jpegenc_v4l2_get_compression(struct file *file, void *priv,
					struct v4l2_jpegcompression *params)
{
	struct jpegenc_fh *jpeg_fh = file->private_data;
	struct jpegenc_device *jpeg_dev = NULL;

	if (!jpeg_fh) {
		css_err("jpeg_fh is null\n");
		return CSS_ERROR_JPEG_FILE_IS_NULL;
	}

	jpeg_dev = jpeg_fh->jpeg_dev;
	if (!jpeg_dev) {
		css_err("jpeg_dev is null\n");
		return CSS_ERROR_JPEG_DEV_IS_NULL;
	}

	params->quality = jpeg_dev->codec_hw_config.compress_level;

	return CSS_SUCCESS;
}

static int odin_jpegenc_v4l2_set_compression(struct file *file, void *priv,
					struct v4l2_jpegcompression *params)
{
	struct jpegenc_fh *jpeg_fh = file->private_data;
	struct jpegenc_device *jpeg_dev = NULL;

	if (!jpeg_fh) {
		css_err("jpeg_fh is null\n");
		return CSS_ERROR_JPEG_FILE_IS_NULL;
	}

	jpeg_dev = jpeg_fh->jpeg_dev;
	if (!jpeg_dev) {
		css_err("jpeg_dev is null\n");
		return CSS_ERROR_JPEG_DEV_IS_NULL;
	}

	if (params->quality < 0)
		jpeg_dev->config_data.compress_level = 0;
	else if (params->quality > 0xFF)
		jpeg_dev->config_data.compress_level = 0xFF;
	else
		jpeg_dev->config_data.compress_level = params->quality;

	return CSS_SUCCESS;
}

static void odin_jpegenc_modify_header(struct jpegenc_platform *jpeg_plat
										,unsigned short height)
{
	unsigned char *jpeg_height;

	jpeg_height = (unsigned char *)jpeg_plat->output_buf.cpu_addr
							+ JPEGENC_HEIGHT_POSITION;

	css_jpeg_enc("before h = %d\n", (*jpeg_height << 8) | *(jpeg_height + 1));

	*jpeg_height = height >> 8;
	*(jpeg_height + 1) = height & 0xFF;

	css_jpeg_enc("0x%x , 0x%x\n", *jpeg_height, *(jpeg_height + 1));
	css_jpeg_enc("h = %d\n", (*jpeg_height << 8) | *(jpeg_height + 1));
	css_jpeg_enc("w = %d\n", (*(jpeg_height + 2) << 8) | *(jpeg_height + 3));
}

irqreturn_t odin_jpegenc_irq(int irq, void *dev_id)
{
	struct jpegenc_fh *jpeg_fh = NULL;
	struct jpegenc_platform *jpeg_plat = NULL;
	unsigned int interrupt_status;

	jpeg_fh = g_jpeg_fh;
	if (!jpeg_fh) {
		css_err("jpeg_fh is null\n");
		jpegenc_interrupt_clear_status_all(); /* clear & disable */
		return IRQ_HANDLED;
	}

	jpeg_plat = jpeg_fh->jpeg_plat;
	if (!jpeg_fh->jpeg_plat) {
		css_err("jpeg_plat is null\n");
		jpegenc_interrupt_clear_status_all(); /* clear & disable */
		return IRQ_HANDLED;
	}

	interrupt_status = jpegenc_get_interrupt_status();
	/* jpegenc_interrupt_disable_all(); */

	if (interrupt_status) {
		if (interrupt_status & JPEGENC_WRITE_END) {
			complete(&jpeg_fh->jpeg_dev->enc_done);
			/*jpegenc_interrupt_clear_status(JPEGENC_WRITE_END_MASK);*/
			jpegenc_interrupt_clear_status_all();

			jpeg_plat->output_buf.state = JPEGENC_BUF_DONE;
#ifdef TEST_IMAGE_FROM_USER
			jpeg_plat->input_buf.state = JPEGENC_BUF_DONE;	/*need check*/
#endif
			/*printk("\n JPEG ENC DONE: %08X\n", interrupt_status);*/
		}
		else if (interrupt_status & JPEGENC_HEADER_END) {
			jpegenc_interrupt_clear_status(JPEGENC_HEADER_END_MASK);
		/*	printk("JPEGENC_HEADER_END %08X\n", interrupt_status);*/
		}
		else if (interrupt_status & JPEGENC_MEM_READ_END) {
			jpegenc_interrupt_clear_status(JPEGENC_MEM_READ_END_MASK);
		/*	printk("JPEGENC_MEM_READ_END %08X\n", interrupt_status);*/
		}
		else if (interrupt_status & JPEGENC_WRITE_OVERFLOW) {
			jpegenc_interrupt_clear_status(JPEGENC_WRITE_OVERFLOW_MASK);
			jpeg_fh->jpeg_dev->error_num = CSS_ERROR_JPEG_WRITE_OVERFLOW;
			goto error;
		}
		else if (interrupt_status & JPEGENC_WRAP_END) {
			jpegenc_interrupt_clear_status(JPEGENC_WRAP_END_MASK);
			jpeg_fh->jpeg_dev->error = 1;
			jpeg_fh->jpeg_dev->error_num = CSS_ERROR_JPEG_WRAP_END;
			/* goto error; */
		}
		else if (interrupt_status & JPEGENC_SCALADO_FIFO_OVERFLOW) {
			jpegenc_interrupt_clear_status(JPEGENC_SCALADO_FIFO_OVERFLOW_MASK);
			jpeg_fh->jpeg_dev->error_num = CSS_ERROR_JPEG_SCALADO_FIFO_OVERFLOW;
			goto error;
		}
		else if (interrupt_status & JPEGENC_BLOCK_FIFO_ERROR) {
			jpegenc_interrupt_clear_status(JPEGENC_BLOCK_FIFO_ERROR_MASK);
			jpeg_fh->jpeg_dev->error_num = CSS_ERROR_JPEG_BLOCK_FIFO_ERROR;
			goto error;
		}
		else if (interrupt_status & JPEGENC_BLOCK_FIFO_OVERFLOW) {
			jpegenc_interrupt_clear_status(JPEGENC_BLOCK_FIFO_OVERFLOW_MASK);
			jpeg_fh->jpeg_dev->error_num = CSS_ERROR_JPEG_BLOCK_FIFO_OVERFLOW;
			goto error;
		}
		else if (interrupt_status & JPEGENC_FLAT_FIFO_ERROR) {
			jpegenc_interrupt_clear_status(JPEGENC_FLAT_FIFO_ERROR_MASK);
			jpeg_fh->jpeg_dev->error_num = CSS_ERROR_JPEG_FLAT_FIFO_ERROR;
			goto error;
		}
		else if (interrupt_status & JPEGENC_FLAT_FIFO_OVERFLOW) {
			jpegenc_interrupt_clear_status(JPEGENC_FLAT_FIFO_OVERFLOW_MASK);
			jpeg_fh->jpeg_dev->error_num = CSS_ERROR_JPEG_FLAT_FIFO_OVERFLOW;
			goto error;
		}
		else {
			jpeg_fh->jpeg_dev->error_num = CSS_ERROR_JPEG_IRQ;
			goto error;
		}
	}

	jpegenc_interrupt_enable_all(); /* need spin lock? */

	return IRQ_HANDLED;

error:
	css_err("JPEG ENC ERROR Status %08X\n", interrupt_status);
	jpeg_fh->jpeg_dev->error = 1;
	complete(&jpeg_fh->jpeg_dev->enc_done);
	jpegenc_interrupt_clear_status_all(); /* clear & disable */
	/* jpegenc_interrupt_disable_all(); */
	jpegenc_clear();

	return IRQ_HANDLED;
}

int odin_jpegenc_open(struct file *file)
{
	struct jpegenc_fh *jpeg_fh;
	int result = CSS_SUCCESS;

	mutex_lock(&g_jpeg_plat->hwmutex);

	if (g_jpeg_fh) {
		css_warn("already opened jpeg file\n");
		mutex_unlock(&g_jpeg_plat->hwmutex);
		return -EBUSY;
	}

	jpeg_fh = kzalloc(sizeof(struct jpegenc_fh), GFP_KERNEL);
	if (!jpeg_fh) {
		css_err("jpeg_fh alloc fail\n");
		mutex_unlock(&g_jpeg_plat->hwmutex);
		return -ENOMEM;
	}

	g_jpeg_fh = jpeg_fh;

	jpeg_fh->jpeg_dev = kzalloc(sizeof(struct jpegenc_device), GFP_KERNEL);
	if (!jpeg_fh->jpeg_dev) {
		css_err("jpeg enc dev alloc fail\n");
		mutex_unlock(&g_jpeg_plat->hwmutex);
		return CSS_ERROR_JPEG_DEV_IS_NULL;
	}

	jpeg_fh->jpeg_plat = video_get_drvdata(video_devdata(file));
	if (!jpeg_fh->jpeg_plat) {
		css_err("jpeg_plat is null\n");
		mutex_unlock(&g_jpeg_plat->hwmutex);
		return CSS_ERROR_JPEG_PLAT_IS_NULL;
	}

	file->private_data = jpeg_fh;

	init_completion(&jpeg_fh->jpeg_dev->enc_done);

	/* jpegenc_block_power_on */
	jpegenc_hw_init(jpeg_fh->jpeg_plat);

	mutex_unlock(&g_jpeg_plat->hwmutex);

	css_info("jpeg-enc open!!!\n");

	return result;
}

int odin_jpegenc_release(struct file *file)
{
	struct jpegenc_fh *jpeg_fh = file->private_data;
	int result = CSS_SUCCESS;

	mutex_lock(&g_jpeg_plat->hwmutex);
	jpeg_fh = file->private_data;
	if (jpeg_fh) {
		/* jpegenc_block_power_off */
		if (jpeg_fh->jpeg_plat)
			jpegenc_hw_deinit(jpeg_fh->jpeg_plat);

		if (jpeg_fh->jpeg_dev) {
			kzfree(jpeg_fh->jpeg_dev);
			jpeg_fh->jpeg_dev = NULL;
		}

		kzfree(jpeg_fh);
		jpeg_fh = NULL;
		file->private_data = NULL;
		g_jpeg_fh = NULL;
	}
	mutex_unlock(&g_jpeg_plat->hwmutex);

	css_info("jpeg-enc release!!!\n");

	return result;
}

int odin_jpegenc_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long size	 = PAGE_ALIGN(vma->vm_end - vma->vm_start);
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long pfn	 = __phys_to_pfn(offset); /* same as vma->vm_pgoff */
	unsigned long prot	 = vma->vm_page_prot;
	int result = CSS_SUCCESS;

	prot = pgprot_noncached(prot);

	if (remap_pfn_range(vma, vma->vm_start, pfn, size, prot)) {
		css_err("remap_pfn_range fail\n");
		return CSS_FAILURE;
	}

	return result;
}

unsigned int odin_jpegenc_poll(struct file *file, poll_table *wait)
{
	struct jpegenc_fh *jpeg_fh = file->private_data;
	struct jpegenc_device *jpeg_dev = NULL;
	struct jpegenc_platform *jpeg_plat = NULL;
	int poll_mask = 0;

	if (!jpeg_fh) {
		css_err("jpeg_fh is null\n");
		return CSS_ERROR_JPEG_FILE_IS_NULL;
	}

	jpeg_dev = jpeg_fh->jpeg_dev;
	if (!jpeg_dev) {
		css_err("jpeg_dev is null\n");
		return CSS_ERROR_JPEG_DEV_IS_NULL;
	}

	jpeg_plat = jpeg_fh->jpeg_plat;
	if (!jpeg_plat) {
		css_err("jpeg_plat is null\n");
		return CSS_ERROR_JPEG_PLAT_IS_NULL;
	}

	if (jpeg_dev->enc_done.done) {
		css_jpeg_enc("jpeg enc done\n");

		if (jpeg_dev->modify_header == 1)
			odin_jpegenc_modify_header(jpeg_plat, jpeg_dev->backup_height);

		if (jpeg_dev->error) {
			css_jpeg_enc("jpeg enc err\n");
			jpeg_dev->error = 0;
			poll_mask |= POLLERR;
		} else {
			poll_mask |= POLLIN | POLLRDNORM;
		}
		INIT_COMPLETION(jpeg_dev->enc_done);
	} else {
		poll_wait(file, &jpeg_dev->enc_done.wait, wait);
	}

	return poll_mask;
}

const struct v4l2_ioctl_ops odin_jpegenc_v4l2_ioctl_ops =
{
	.vidioc_s_ctrl			= odin_jpegenc_v4l2_s_ctrl,
	.vidioc_g_ctrl			= odin_jpegenc_v4l2_g_ctrl,
	.vidioc_g_fmt_vid_cap	= odin_jpegenc_v4l2_g_fmt_vid_cap,
	.vidioc_g_fmt_vid_out	= odin_jpegenc_v4l2_g_fmt_vid_out,
	.vidioc_reqbufs			= odin_jpegenc_v4l2_reqbufs,
	.vidioc_querybuf		= odin_jpegenc_v4l2_querybuf,
	.vidioc_qbuf			= odin_jpegenc_v4l2_qbuf,
	.vidioc_dqbuf			= odin_jpegenc_v4l2_dqbuf,
	.vidioc_s_fmt_vid_cap	= odin_jpegenc_v4l2_s_fmt_vid_cap,
	/* .vidioc_s_fmt_vid_out	= odin_jpegenc_v4l2_s_fmt_vid_out,*/
	.vidioc_streamoff		= odin_jpegenc_v4l2_streamoff,
	.vidioc_streamon		= odin_jpegenc_v4l2_streamon,
	.vidioc_g_jpegcomp		= odin_jpegenc_v4l2_get_compression,
	.vidioc_s_jpegcomp		= odin_jpegenc_v4l2_set_compression,
};

const struct v4l2_file_operations odin_jpegenc_v4l2_fops =
{
	.owner		= THIS_MODULE,
	.open		= odin_jpegenc_open,
	.release	= odin_jpegenc_release,
	.unlocked_ioctl = video_ioctl2,
	.mmap		= odin_jpegenc_mmap,
	.poll		= odin_jpegenc_poll,

};

struct video_device odin_jpegenc_v4l2_dev =
{
	.name = ODIN_JPEGENC_V4L2_DEV_NAME,
	.fops = &odin_jpegenc_v4l2_fops,
	.ioctl_ops = &odin_jpegenc_v4l2_ioctl_ops,
	.release = video_device_release,
	.vfl_dir = VFL_DIR_M2M,
};

int odin_jpegenc_probe(struct platform_device *pdev)
{
	struct jpegenc_platform	*jpeg_plat;
	struct video_device *video_dev = NULL;
	struct resource *iores = NULL;
#ifdef USE_PMEM
	struct resource *iomem = NULL;
#endif
	int result = -EBUSY;

	/* allocate jpeg_plat */
	jpeg_plat = kzalloc(sizeof(struct jpegenc_platform), GFP_KERNEL);
	if (!jpeg_plat) {
		css_err("jpeg_plat alloc fail\n");
		return CSS_ERROR_JPEG_PLAT_IS_NULL;
	}

	mutex_init(&jpeg_plat->hwmutex);

	g_jpeg_plat = jpeg_plat;

	jpeg_plat->initialize = 0;

	/* jpeg_plat = g_jpeg_enc_plat;*/

	jpeg_plat->pdev = pdev;

	platform_set_drvdata(pdev, jpeg_plat);

	spin_lock_init(&jpeg_plat->hwlock);

	/* get register physical base address to resource of platform device */
	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!iores) {
		css_err("jpeg enc failed to get resource!\n");
		result = -ENOMEM;
		goto error_io;
	}

	jpeg_plat->iores = request_mem_region(iores->start,
								iores->end - iores->start + 1, pdev->name);
	if (!jpeg_plat->iores) {
		css_err("failed to request mem region : %s!\n", pdev->name);
		result = -ENOMEM;
		goto error_io;
	}

	jpeg_plat->io_base = ioremap(iores->start, iores->end - iores->start + 1);
	if (!jpeg_plat->io_base) {
		css_err("failed to ioremap!\n");
		result = -ENOMEM;
		goto error_res;
	}

#ifdef USE_PMEM
	iomem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (iomem == NULL) {
		dev_err(&pdev->dev, "failed to get memory region resource\n");
		result = -ENOENT;
		goto error_iomap;
	}

	jpeg_plat->pmem_size = (iomem->end - iomem->start) + 1;
	jpeg_plat->iomem = request_mem_region(iomem->start, jpeg_plat->pmem_size,
									pdev->name);

	if (jpeg_plat->iomem == NULL) {
		dev_err(&pdev->dev, "failed to get memory region resource\n");
		result = -ENOENT;
		goto error_iomap;
	}
	jpeg_plat->pmem_base = iomem->start;

	/*
	jpeg_plat->cpu_addr = (void*)ioremap_nocache((unsigned long)
				jpeg_plat->pmem_base, jpeg_plat->pmem_size);
	if (jpeg_plat->cpu_addr == NULL) {
		css_err("jpeg_plat cpu_addr pre-alloc fail!\n");
		return CSS_FAILURE;
	}
	*/

	css_info("memory(jpg): 0x%08x 0x%08x\n",
			(u32)iomem->start, (u32)resource_size(iomem));
#endif

	/* get IRQ number of jpeg-enc */
	jpeg_plat->irq = platform_get_irq(pdev, 0);

	/* requset IRQ number and register IRQ service routine */
	result = request_irq(jpeg_plat->irq, odin_jpegenc_irq , 0, pdev->name,
					jpeg_plat);
	if (result) {
		css_err("failed to request irq %d\n", result);
		goto error_irq;
	}

	disable_irq(jpeg_plat->irq);

	/* allocate v4l2 device */
	video_dev = video_device_alloc();
	if (!video_dev) {
		css_err("video_dev was failed to allocate!\n");
		goto error_video_alloc;
	}

	/* initialize v4l2 device */
	jpeg_plat->video_dev = video_dev;
	memcpy(video_dev, &odin_jpegenc_v4l2_dev, sizeof(odin_jpegenc_v4l2_dev));

	video_set_drvdata(video_dev,jpeg_plat);

	/* registers v4l2 device for odin-jpegenc to v4l2 layer of kernel */
	/* if videoDevNum == -1, first v4l2 device number is 0. */
	result = video_register_device(video_dev, VFL_TYPE_GRABBER,
							ODIN_JPEGENC_MINOR);
	if (result) {
		css_err("failed to register video device %d\n", result);
		goto error_video_reg;
	}
	css_info("jpeg-enc probe ok!!\n");
	return CSS_SUCCESS;

error_video_reg:
	video_device_release(video_dev);

error_video_alloc:
error_irq:
	free_irq(jpeg_plat->irq, odin_jpegenc_irq);

error_iomap:
	if (jpeg_plat->io_base)
		iounmap(jpeg_plat->io_base);

error_res:
	if (jpeg_plat->iores)
		release_mem_region(jpeg_plat->iores->start,
						jpeg_plat->iores->end - jpeg_plat->iores->start + 1);
#ifdef USE_PMEM
	if (jpeg_plat->iomem)
		release_mem_region(jpeg_plat->iomem->start,
						jpeg_plat->iomem->end - jpeg_plat->iomem->start + 1);
#endif

error_io:
	platform_set_drvdata(pdev, NULL);

	return result;
}

int odin_jpegenc_remove(struct platform_device *pdev)
{
	int result = CSS_SUCCESS;
	struct jpegenc_platform	*jpeg_plat = platform_get_drvdata(pdev);
	struct video_device *video_dev = NULL;

	if (jpeg_plat) {
		video_dev = jpeg_plat->video_dev;
		if (video_dev) {
			video_unregister_device(video_dev);
			video_device_release(video_dev);
			video_dev = NULL;
		}
		if (jpeg_plat->irq) {
			free_irq(jpeg_plat->irq, odin_jpegenc_irq);
			jpeg_plat->irq = 0;
		}
		if (jpeg_plat->io_base) {
			iounmap(jpeg_plat->io_base);
			jpeg_plat->io_base = NULL;
		}
		if (jpeg_plat->iores) {
			release_mem_region(jpeg_plat->iores->start,
				jpeg_plat->iores->end - jpeg_plat->iores->start + 1);
			jpeg_plat->iores = NULL;
		}
#ifdef USE_PMEM
		if (jpeg_plat->iomem) {
			release_mem_region(jpeg_plat->iomem->start,
				jpeg_plat->iomem->end - jpeg_plat->iomem->start + 1);
			jpeg_plat->iomem = NULL;
		}
#endif
		kzfree(jpeg_plat);
		jpeg_plat = NULL;
	}

	platform_set_drvdata(pdev, NULL);

	return result;
}

int odin_jpegenc_suspend(struct platform_device * pdev, pm_message_t state)
{
	int result = CSS_SUCCESS;
	return result;
}

int odin_jpegenc_resume(struct platform_device * pdev)
{
	int result = CSS_SUCCESS;
	return result;
}

#ifdef CONFIG_OF
static struct of_device_id odin_jpegenc_match[] = {
	{ .compatible = CSS_OF_JPEGENC_NAME},
	{},
};
#endif /* CONFIG_OF */

struct platform_driver odin_jpegenc_driver =
{
	.probe = odin_jpegenc_probe,
	.remove = odin_jpegenc_remove,
	.suspend = odin_jpegenc_suspend,
	.resume = odin_jpegenc_resume,
	.driver = {
		.name = ODIN_JPEGENC_DRV_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odin_jpegenc_match),
#endif
	},
};

int __init odin_jpegenc_init(void)
{
	int result = CSS_SUCCESS;

	result = platform_driver_register(&odin_jpegenc_driver);

	return result;
}

void __exit odin_jpegenc_exit(void)
{
	platform_driver_unregister(&odin_jpegenc_driver);
}

late_initcall(odin_jpegenc_init);
module_exit(odin_jpegenc_exit);
MODULE_DESCRIPTION("odin jpeg encode driver for V4L2");
MODULE_AUTHOR("MTEKVISION<http://www.mtekvision.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

