/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _AUDIO_HDMI_CONTROL_H
#define _AUDIO_HDMI_CONTROL_H

void set_hdmi_out_control(unsigned int channels);

void set_hdmi_out_control_enable(bool enable);

void set_hdmi_i2s(void);

void set_hdmi_i2s_enable(bool enable);

void set_hdmi_clock_source(unsigned int sample_rate);

void set_hdmi_bck_div(unsigned int sample_rate);

#endif
