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

#ifndef __ODIN_CAMERA_UTILS_H__
#define __ODIN_CAMERA_UTILS_H__


/* #define CSS_TIME_MEASURE 1 */

extern u32 get_greatest_common_divisor(u32 img_w, u32 img_h);
extern unsigned long measure_time(struct timeval *start, struct timeval *stop);

#ifdef CSS_TIME_MEASURE
#define CSS_TIME_RESULT(log, ...) css_##log(__VA_ARGS__)
#define CSS_TIME_DEFINE(n) \
	struct timeval time_start_##n, time_stop_##n; \
	unsigned long log_time_##n = 0;

#define CSS_TIME_START(n) \
	do_gettimeofday(&time_start_##n);

#define CSS_TIME_END(n) \
	do_gettimeofday(&time_stop_##n); log_time_##n = \
		measure_time(&time_start_##n, &time_stop_##n);

#define CSS_TIME(n) \
	log_time_##n
#else
#define CSS_TIME_RESULT(...)
#define CSS_TIME_DEFINE(n)
#define CSS_TIME_START(n)
#define CSS_TIME_END(n)
#define CSS_TIME(n)
#endif

struct css_rsvd_memory *odin_css_create_mem_pool(unsigned int phys_pool_addr,
								unsigned int pool_size,
								struct platform_device *pdev);
int odin_css_destroy_mem_pool(struct css_rsvd_memory *mem);
int odin_css_physical_buffer_alloc(struct css_rsvd_memory *mem,
								struct css_buffer *buf, size_t size);
int odin_css_physical_buffer_free(struct css_rsvd_memory *mem,
								struct css_buffer *buf);

char *css_strtok(char *str_token, const char *str_delimit);
size_t css_atoi(const char *token);
void css_get_fd_info(int fd);

#define CSS_MAX(a,b)	(((a)>=(b))?(a):(b))


#endif /* __ODIN_CAMERA_UTILS_H__ */
