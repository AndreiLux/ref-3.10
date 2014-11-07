/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See th
 * GNU General Public License for more details.
 */


#ifndef __LINUX_ODIN_REGISTER_H__
#define __LINUX_ODIN_REGISTER_H__

#include <linux/odin_mailbox.h>

#define REGISTER			6
#define REGISTER_OFFSET		28
#define SUB_CMD_OFFSET		0
#define SUB_CMD_MASK		0xF

#define REGISTER_READ		0x0
#define REGISTER_WRITE		0x1


extern int odin_register_write(unsigned int address, unsigned int write_value);
extern unsigned int odin_register_read(unsigned int address);

#endif /* __LINUX_ODIN_REGISTER_H__ */
