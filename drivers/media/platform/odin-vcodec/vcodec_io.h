/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * Youngki Lyu <youngki.lyu@lge.com>
 * Jungmin Park <jungmin016.park@lge.com>
 * Younghyun Jo <younghyun.jo@lge.com>
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

#ifndef _VCODEC_IO_H_
#define _VCODEC_IO_H_

int vcodec_io_open(struct inode* inode, struct file* file);
int vcodec_io_close(struct inode* inode, struct file *file);
long vcodec_io_unlocked_ioctl(struct file* file,
				unsigned int cmd, unsigned long arg);

#endif /* #ifndef _VCODEC_IO_H_ */
