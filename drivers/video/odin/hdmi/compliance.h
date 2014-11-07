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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef COMPLIANCE_H_
#define COMPLIANCE_H_

#include "../hdmi_api_tx/edid/dtd.h"
int compliance_init(void);
int compliance_configure(u8 fixed_color_screen);
int compliance_standby(void);

int compliance_hdcp(int on);

void compliance_displayformat(void);
int compliance_modex(u8 x);
int compliance_next(void);

void compliance_audio_mute(int enable);
int compliance_audio_params_set(void *audparam);

int compliance_get_currentmode(void);
void compliance_set_currentmode(int i);

int get_dtd(dtd_t *tmp_dtd);
bool is_dtd(void);
int get_pclock(void);
int get_ceacode(void);
int get_sad_count(void);
int get_edid_sad(u8 *sadbuf, int cnt);
int get_edid_spk_alloc_data(u8 *spk_alloc);

/*  debug */
void compliance_show_sad(void);
int compliance_show_dtd_list(void);
int compliance_show_svd_list(int i);
int compliance_svd_select(void);
int compliance_total_count(void);

/* add for getting resolution info */
extern int aftmode_resolution;

#endif /*COMPLIANCE_H_*/

