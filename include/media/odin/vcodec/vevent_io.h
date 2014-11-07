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

#ifndef _VEVENT_IO_H_
#define _VEVENT_IO_H_

#define VEVENT_MAX_MSG_SIZE	1024

struct vevent
{
	enum
	{
		VEVENT_TYPE_EVENT,
		VEVENT_TYPE_END_OF_EVENT,
	} type;
	void *vevent_id;

	char msg[VEVENT_MAX_MSG_SIZE];
	int size;
};

#endif /* #ifndef _VEVENT_IO_H_ */
