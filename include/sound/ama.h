/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
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

#ifndef _AMA_H_
#define _AMA_H_


void *ama_virt_to_phys(void *addr);

void* ama_alloc(size_t size);
void ama_free(void *addr);

bool ama_init(unsigned long base, size_t size);
bool ama_cleanup(void);

#endif /* #ifndef _AMA_H_ */
