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

#ifndef _VCORE_DEVICE_H_
#define _VCORE_DEVICE_H_

#include <linux/string.h>
#include <media/odin/vcodec/vcore/decoder.h>
#include <media/odin/vcodec/vcore/encoder.h>
#include <media/odin/vcodec/vcore/ppu.h>

struct vcore_entry
{
	char *vcore_name;
	char *gpd_name;
	int (*runtime_resume)(void);
	int (*runtime_suspend)(void);
	void (*resume)(void);
	void (*suspend)(void);
	unsigned long (*get_utilization)(void);
	unsigned long (*get_util_estimation)(
					unsigned int width, unsigned int height,
					unsigned int fr_residual, unsigned int fr_divider);
	struct {
		char **name;
		int num;
	} clk;

	void (*vcore_isr)(void);

	void (*vcore_remove)(void);

	int (*vcore_register_done)(unsigned int reg_base,
					void (*clock_on)(char* vcore_name),
					void (*clock_off)(char* vcore_name));

	struct
	{
		unsigned int capability;
		void (*init)(struct vcore_dec_ops *ops);
	} dec;

	struct
	{
		unsigned int capability;
		void (*init)(struct vcore_enc_ops *ops);
	} enc;

	struct
	{
		void (*init)(struct vcore_ppu_ops *ops);
	} ppu;
};

int vcore_register_request(struct vcore_entry *core_ops);
int vcore_unregister_request(const char *vcore_name);

#endif /* #ifndef _VCORE_DEVICE_H_ */
