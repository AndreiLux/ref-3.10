/*
 * Copyright (C) 2013 Mtekvision
 * Author: Daeheon Kim <kimdh@mtekvision.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __ODIN_CSS_SYSTEM_H__
#define __ODIN_CSS_SYSTEM_H__


#define BLK_RST_AXI			(0x1 << 0)
#define BLK_RST_AXI_R		(0x1 << 1)
#define BLK_RST_AHB			(0x1 << 2)
#define BLK_RST_AHB_R		(0x1 << 3)
#define BLK_RST_APB			(0x1 << 4)
#define BLK_RST_FRGB		(0x1 << 5)
#define BLK_RST_MDIS		(0x1 << 6)
#define BLK_RST_TUNE		(0x1 << 7)
#define BLK_RST_I2C0		(0x1 << 8)
#define BLK_RST_I2C1		(0x1 << 9)
#define BLK_RST_I2C2		(0x1 << 10)
#define BLK_RST_I2C3		(0x1 << 11)
#define BLK_RST_FD			(0x1 << 12)
#define BLK_RST_FPD			(0x1 << 13)
#define BLK_RST_JPEGENC		(0x1 << 14)
#define BLK_RST_S3DC		(0x1 << 15)
#define BLK_RST_MIPI0		(0x1 << 16)
#define BLK_RST_MIPI1		(0x1 << 17)
#define BLK_RST_MIPI2		(0x1 << 18)
#define BLK_RST_ISP0		(0x1 << 19)
#define BLK_RST_ISP1		(0x1 << 20)
#define BLK_RST_VNR			(0x1 << 21)
#define BLK_RST_PD_STILL	(0x1 << 22)
#define BLK_RST_PD_STEREO	(0x1 << 23)


#define CRG_CLK_MIPI0			(0x1 << 0)
#define CRG_CLK_MIPI1			(0x1 << 1)
#define CRG_CLK_MIPI2			(0x1 << 2)
#define CRG_CLK_FD				(0x1 << 10)
#define CRG_CLK_MDIS			(0x1 << 16)
#define CRG_CLK_ISP0			(0x1 << 17)
#define CRG_CLK_ISP1			(0x1 << 18)
#define CRG_CLK_FRGB			(0x1 << 21)
#define CRG_CLK_VNR				(0x1 << 23)


#define BLK_RESET_ALL		(0xffffff68)
#define BLK_RESET_NONE		(0x0)
#define BLK_RESET_I2C		(BLK_RST_I2C0 |		\
							 BLK_RST_I2C1 |		\
							 BLK_RST_I2C2 |		\
							 BLK_RST_I2C3)

#define BLK_RESET_PD0		(BLK_RESET_NONE)
#define BLK_RESET_PD1		(BLK_RST_FRGB |		\
							 BLK_RST_FD |		\
							 BLK_RST_FPD | 		\
							 BLK_RST_JPEGENC |	\
							 BLK_RST_MIPI0 | 	\
							 BLK_RST_MIPI2 |	\
							 BLK_RST_ISP0 |		\
							 BLK_RESET_I2C |	\
							 BLK_RST_PD_STILL)
#define BLK_RESET_PD2		(BLK_RST_MIPI1 |	\
							 BLK_RST_ISP1 |		\
							 BLK_RST_S3DC |		\
							 BLK_RST_TUNE |		\
							 BLK_RST_PD_STEREO)
#define BLK_RESET_PD3		(BLK_RST_MDIS |		\
							 BLK_RST_VNR)

#define	POWER_SCALER	0x0001
#define	POWER_MDIS		0x0002
#define	POWER_VNR		0x0004
#define	POWER_JPEG		0x0008
#define	POWER_ISP0		0x0010
#define	POWER_ISP1		0x0020
#define	POWER_S3DC		0x0040
#define	POWER_TUNER		0x0080
#define POWER_ALL		(POWER_SCALER | POWER_MDIS | POWER_VNR | POWER_JPEG | \
						 POWER_ISP0 | POWER_ISP1 | POWER_S3DC | POWER_TUNER)

/* #define CSS_VT_MEM_CTRL_SUPPORT			1 */
#define CSS_VT_MEM_BUS_FREQ_KHZ			400000
#define CSS_DEFAULT_MEM_BUS_FREQ_KHZ	800000
#define CSS_CAPTURE_MEM_BUS_FREQ_KHZ	800000
#define CSS_DEFAULT_CCI_BUS_FREQ_KHZ	100000

#define CSS_BUS_FREQ_CONTROL_OFF		0
#define CSS_BUS_TIME_SCHED_US			5000000
#define CSS_BUS_TIME_SCHED_NONE			0

#define CSS_CSI_DPHY_VOLTAGE			"+1.8V_VDD_CSS_MAIN_CAM_IO"

typedef enum {
	MEM_BUS,
	CCI_BUS
} bus_ctrl;

#ifdef CONFIG_ARCH_MVVICTORIA
void odin_css_init(void);
#endif

int	css_power_domain_init_on(int power);
int	css_power_domain_init_off(int power);
int	css_power_domain_on(int power);
int	css_power_domain_off(int power);
void css_power_domain_suspend(void);
void css_power_domain_resume(void);
int	css_block_reset(unsigned int mask);
int	css_block_reset_off(unsigned int mask);
int	css_block_reset_on(unsigned int mask);
void css_bus_control(bus_ctrl bus, unsigned int khz, unsigned int usec);
int css_get_chip_revision(void);
unsigned int css_crg_clk_info(void);
int css_set_isp_divider(int kHz);
unsigned int css_get_isp_divider(void);
void odin_css_reg_dump(void);
#endif /* __ODIN_CSS_SYSTEM_H__ */
