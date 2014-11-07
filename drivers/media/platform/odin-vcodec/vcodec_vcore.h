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

#ifndef _VCODEC_VCORE_H_
#define  _VCODEC_VCORE_H_

void* vcodec_vcore_get_dec(enum vcore_dec_codec codec_type,
								struct vcore_dec_ops *ops,
								bool rendering_dpb);
void vcodec_vcore_put_dec(void *id);

void* vcodec_vcore_get_enc(enum vcore_enc_codec codec_type,
							struct vcore_enc_ops *ops,
							unsigned int width, unsigned int height,
							unsigned int fr_residual, unsigned int fr_divider);
void vcodec_vcore_put_enc(void *id);

void* vcodec_vcore_get_ppu(struct vcore_ppu_ops *ops,
							bool without_fail,
							unsigned int width, unsigned int height,
							unsigned int fr_residual, unsigned int fr_divider);
void vcodec_vcore_put_ppu(void *id);
#endif /* #ifndef _VCODEC_VCORE_H_ */

