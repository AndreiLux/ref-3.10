/* arch/arm/mach-odin/include/mach/odin_pd.h
 *
 * ODIN PM domain IPC driver
 *
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


#ifndef __MACH_ODIN_PD_H
#define __MACH_ODIN_PD_H

/* FIXME: move to mach-odin/include/mach/ */

#include <linux/odin_mailbox.h>

#define PD					2
#define PD_OFFSET			28
#define SUB_BLOCK_OFFSET	24
#define SUB_BLOCK_MASK		0xF
#define PD_OFF				0x0
#define PD_ON				0x1

#define PD_FRAME_WORK	1

#define PD_VSP			0
#define PD_CSS			1
#define PD_DSS			2
#define PD_AUD			3
#define PD_ICS			4
#define PD_GPU			5
#define PD_CPU			6
#define PD_PERI			7
#define PD_SES			8

#define PM_SUB_BLOCK        8
#define PM_SUB_BLOCK_LPA    3
#define PM_SUB_BLOCK_MEMORY 4
#define PM_SUB_BLOCK_SES    5
#define PM_SUB_BLOCK_EFUSE  6

#define LPA_DSP_PLL_CTL     3

#define SES_HDMI_KEY_REQUEST  0
#define SES_HDMI_KEY_FAIL     1

#define EFUSE_READ_DEVICE     0
#define EFUSE_READ_OEM        1
#define EFUSE_READ_LIFE_CYCLE 2

extern int odin_pd_off(unsigned int sub_block, unsigned int id);
extern int odin_pd_on(unsigned int sub_block, unsigned int id);
extern int odin_power_domain_off(unsigned int sub_block, unsigned int id);
extern int odin_power_domain_on(unsigned int sub_block, unsigned int id);

extern int odin_mem_thermal_ctl(void);

extern int odin_ses_hdmi_key_request(void);
extern int odin_ses_hdmi_key_fail(void);

extern int odin_lpa_dsp_pll_ctl(unsigned int dsp_pll_ctl);

extern int odin_efuse_read_device(void);
extern int odin_efuse_read_oem(void);
extern int odin_efuse_read_life_cycle(void);

#endif /* __MACH_ODIN_PD_H */
