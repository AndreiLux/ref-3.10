/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Younghyun Jo <younghyun.jo@lge.com>
 * Youngki Lyu <youngki.lyu@lge.com>
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

#ifndef _DSS_BUF_H_
#define _DSS_BUF_H_

void dss_buf_list_print(void);

unsigned long dss_buf_rot_paddr_get(const unsigned long paddr);

unsigned long dss_buf_alloc(const size_t size);
int dss_buf_free(const void *fd_or_paddr); /* free buff immediatly */

unsigned long dss_buf_map(const int fd);
int dss_buf_unmap(const unsigned long paddr);

void dss_buf_init(void);
void dss_buf_cleanup(void);

#endif /* #ifndef _DSS_BUF_H_ */
