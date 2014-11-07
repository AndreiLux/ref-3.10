/*
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
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/file.h>

#include "odin-css.h"
#include "odin-css-utils.h"

unsigned int css_log = LOG_ERR|LOG_WARN;


u32 get_greatest_common_divisor(u32 img_w, u32 img_h)
{
	s32 remain_num = 0;

	remain_num = img_w - img_h;
	if (remain_num == 0) {
		return img_w;
	} else {
		/* if result is minus, Height Length is big than Width. */
		if (remain_num < 0) {
			remain_num *= -1;
			img_h = img_w;
		}

		if (remain_num > img_h)
			return get_greatest_common_divisor(remain_num, img_h);
		else
			return get_greatest_common_divisor(img_h, remain_num);
	}
}

unsigned long measure_time(struct timeval *start, struct timeval *stop)
{
	unsigned long sec, usec, time;

	sec = stop->tv_sec - start->tv_sec;

	if (stop->tv_usec >= start->tv_usec) {
		usec = stop->tv_usec - start->tv_usec;
	} else {
		usec = stop->tv_usec + 1000000 - start->tv_usec;
		sec--;
	}

	time = (sec * 1000000) + usec;

	return time;
}

struct css_rsvd_memory *odin_css_create_mem_pool(unsigned int phys_pool_addr,
							unsigned int pool_size,
							struct platform_device *pdev)
{
	struct css_rsvd_memory *mem = NULL;

	if (phys_pool_addr == 0 || pool_size == 0) {
		return NULL;
	}

	mem = kmalloc(sizeof(struct css_rsvd_memory), GFP_KERNEL);
	if (mem == NULL) {
		return NULL;
	}

	mem->res = request_mem_region(phys_pool_addr, pool_size, pdev->name);
	if (mem->res == NULL) {
		kfree(mem);
		return NULL;
	}

	mem->base		= phys_pool_addr;
	mem->next_base	= phys_pool_addr;
	mem->alloc_size = 0;
	mem->size		= PAGE_ALIGN(pool_size);

	return mem;
}

int odin_css_destroy_mem_pool(struct css_rsvd_memory *mem)
{
	if (mem) {
		if (mem->res) {
			release_mem_region(mem->res->start,
								mem->res->end - mem->res->start + 1);
		}
		kfree(mem);
	}
	return CSS_SUCCESS;
}

int odin_css_physical_buffer_alloc(struct css_rsvd_memory *mem,
			struct css_buffer *buf, size_t size)
{
	int ret = CSS_SUCCESS;

	size = PAGE_ALIGN(size);

	if (buf->allocated == false) {
		buf->id = -1;

		if (mem->alloc_size + size > mem->size) {
			buf->cpu_addr = 0;
			buf->cpu_addr = 0;
			buf->dma_addr = 0;
			buf->allocated = false;
			css_err("rsvd buffer over! alloc:%d(KB) free:%d(KB) ",
					mem->alloc_size / 1024,
					(mem->size - mem->alloc_size) / 1024);
			css_err("req:%d(KB) total:%d(KB)\n",
					size / 1024, mem->size / 1024);
			return CSS_FAILURE;
		}

		buf->dma_addr = mem->next_base;
		buf->cpu_addr = (void*)ioremap_nocache((unsigned long)
					buf->dma_addr, size);
		if (buf->cpu_addr == NULL) {
			css_err("camera buffer pre-alloc fail!\n");
			return CSS_FAILURE;
		}

		buf->offset = 0;
		buf->byteused = 0;
		buf->size = size;
		buf->state = CSS_BUF_UNUSED;
		/* memset(buf->cpu_addr, 0x0, size); */
		buf->allocated = true;
		mem->next_base += size;
		mem->alloc_size += size;
	}

	return ret;
}

int odin_css_physical_buffer_free(struct css_rsvd_memory *mem,
			struct css_buffer *buf)
{
	int ret = CSS_SUCCESS;

	if (buf->cpu_addr != 0) {
		iounmap(buf->cpu_addr);
		buf->cpu_addr = 0x0;
	}

	if (mem->alloc_size == 0) {
		buf->dma_addr	= 0x0;
		buf->offset		= 0x0;
		buf->size		= 0x0;
		buf->byteused	= 0;
		buf->state		= 0x0;
		buf->allocated	= false;
		return ret;
	}

	mem->alloc_size -= buf->size;
	buf->dma_addr	= 0x0;
	buf->offset		= 0x0;
	buf->size		= 0x0;
	buf->byteused	= 0;
	buf->state		= 0x0;
	buf->allocated	= false;

	if (mem->alloc_size <= 0) {
		mem->alloc_size = 0;
		mem->next_base = mem->base;
	}

	return ret;
}

char *css_strtok(char *str_token, const char *str_delimit)
{
	static char *pcurr;
	char *pdelimit;

	if (str_token != NULL)
		pcurr = str_token;
	else
		str_token = pcurr;

	if (*pcurr == 0x0)
		return NULL;

	while (*pcurr) {
		pdelimit = (char*)str_delimit;

		while (*pdelimit) {
			if (*pcurr == *pdelimit) {
				*pcurr = (char)'\0';
				++pcurr;
				return str_token;
			}
			++pdelimit;
		}
		++pcurr;
	}

	return str_token;
}

size_t css_atoi(const char *token)
{
	int value = 0;

	for (;; token++) {
		switch (*token) {
		case '0' ... '9':
			value = (10 * value) + (*token - '0');
			break;
		default:
			return value;
		}
	}

	return 0;
}

void css_get_fd_info(int fd)
{
	if (fd > 0) {
		struct file *file;
		file = fget(fd);
		if (file) {
			printk("%pS fd(%d) name: %s\n", __builtin_return_address(0),
						fd, file->f_path.dentry->d_iname);
			fput(file);
		}
	}
}

