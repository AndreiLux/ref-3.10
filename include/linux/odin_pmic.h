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

#ifndef __LINUX_ODIN_PMIC_H__
#define __LINUX_ODIN_PMIC_H__

#include <linux/odin_mailbox.h>

#define PMIC				4
#if defined(CONFIG_MACH_ODIN_LIGER)
#define PMIC_REGISTER		6
#endif
#define PMIC_OFFSET			28
#define SUB_CMD_OFFSET		24
#define SUB_CMD_MASK		0xF

#define PMIC_DISABLE		0x0
#define PMIC_ENABLE			0x1
#define PMIC_SET			0x2
#define PMIC_GET			0x3
#define PMIC_BOOT_REASON    0x6
#define PMIC_REV_GET		0xf
#if defined(CONFIG_MACH_ODIN_LIGER)
#define PMIC_REGISTER_GET	0x0
#define PMIC_REGISTER_SET	0x1
#endif

/* PMIC Index mappling */
#define _1V8_VDDIO_SMS_PM		0
#define _1V8_VDDIO_SMS			1
#define _1V8_VDDIO_AO			2
#define _1V8_VDDIO_NAND_OPT		3
#define _1V8_VDDIO_AV_OPT		4
#define _0V9_VDD_SMS			5
#define _1V8_AVDDPLL_SMS		6
#define _3V0_VDDF_NAND			7
#define _1V8_VDDIO_PERI			8
#define _1V8_VDDIO_ICS			9
#define _1V8_VDDIO_CPU			10
#define _1V8_VDDIO_DSS			11
#define _1V8_VDDIO_CSS			12
#define _1V8_VDDIO_AV			13
#define _1V8_VDDIO_NAND			14
#define _1V8_VDDIO_SYS			15
#define _0V9_VDD_NAND			16
#define _3V0_VDDIO_SDIO			17
#define _1V8_VDD_LPDDR_CORE		18
#define _1V2_VDD_LPDDRIO_AO		19
#define _1V2_VDD_LPDDRIO		20
#define _1V8_AVDDPLL_CORE_TS	21
#define _0V9_VDD_CORE			22
#define _0V9_VDD_CORE_M			23
#define _0V9_VDD_CA7			24
#define _0V9_VDD_CA7_CACHE		25
#define _3V3_AVDD_USB30PHYIO	26
#define _0V9_AVDD_USB30PHY		27
#define _0V9_VDD_CA15			28
#define _0V9_VDD_CA15_CACHE		29
#define _0V9_VDD_GPU			30
#define _0V9_VDD_AV				31
#define _0V9_VDD_CSS			32
#define _0V9_AVDD_MPHY_LLI		33
#define _1V8_AVDD_MPHY_LLI		34
#define _1V8_VCCA				35
#define _1V8_AVDD_DPHY_DSS		36
#define _3V3_VDD_RGB_LCD_PM		37
#define _1V8_LCD_IOVCC			38
#define _1V8_TOUCH_IOVCC		39
#define _1V8_MAIN_CAM_VCM		40
#define _3V3_TOUCH_AVCC			41
#define _1V2_CAM_DVDD			42
#define _2V8_CAM_AVDD			43
#define _1V8_AVDDDPHY_CSS		44
#define _1V8_AVDDDPHY_CSS2		45
#define _1V8_CAM_VDDIO			46
#define _3V0_VDD_USD			47
#define _1V2_VDDIO_USBHSIC_PM	48
#define _3V0_SENSOR_AVDD		49
#define _1V8_SENSOR_DVDD		50
#define _0V9_HDMI_VP			51
#define _3V0_VDD_MOTOR			52
#define _2V8_VTCAM_AVDD			53
#define _1V2_VTCAM_DVDD			54

/* board revision */
#define MADK_V1_0               0xA
#define MADK_V1_1_1             0xB
#define MADK_V1_1_2             0xC
#define MADK_V1_2               0xD
#define MADK_V1_3               0xE

/* RDK board revision */
#define RDK_REV_A                              0x0A
#define RDK_REV_B                              0x0B
#define RDK_REV_C                              0x0C
#define RDK_REV_I                              0x12
#define RDK_REV_J                              0x13

extern int board_rev;

extern int odin_pmic_source_disable(unsigned int pmic_source);
extern int odin_pmic_source_enable(unsigned int pmic_source);
extern int odin_pmic_source_set(unsigned int pmic_source, unsigned int milli_voltage);
extern int odin_pmic_source_get(unsigned int pmic_source);
extern int odin_pmic_revision_get(void);
extern int odin_pmic_bootreason_get(void);
#if defined(CONFIG_MACH_ODIN_LIGER)
extern unsigned int odin_pmic_getRegister(unsigned int nRegister);
extern int odin_pmic_setRegister(unsigned int nRegister, unsigned int nValue);
#endif

#endif /* __LINUX_ODIN_PMIC_H__ */
