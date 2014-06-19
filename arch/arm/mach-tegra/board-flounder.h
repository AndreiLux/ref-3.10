/*
 * arch/arm/mach-tegra/board-flounder.h
 *
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _MACH_TEGRA_BOARD_FLOUNDER_H
#define _MACH_TEGRA_BOARD_FLOUNDER_H

#include <mach/gpio-tegra.h>
#include <mach/irqs.h>
#include "gpio-names.h"
#include <mach/board_htc.h>

int flounder_pinmux_init(void);
int flounder_emc_init(void);
int flounder_display_init(void);
int flounder_panel_init(void);
int flounder_kbc_init(void);
int flounder_sdhci_init(void);
int flounder_sensors_init(void);
int flounder_regulator_init(void);
int flounder_suspend_init(void);
int flounder_rail_alignment_init(void);
int flounder_soctherm_init(void);
int flounder_edp_init(void);
void flounder_camera_auxdata(void *);
int flounder_mdm_9k_init(void);
void flounder_new_sysedp_init(void);
void flounder_sysedp_dynamic_capping_init(void);

/* generated soc_therm OC interrupts */
#define TEGRA_SOC_OC_IRQ_BASE	TEGRA_NR_IRQS
#define TEGRA_SOC_OC_NUM_IRQ	TEGRA_SOC_OC_IRQ_MAX

#define PALMAS_TEGRA_GPIO_BASE	TEGRA_NR_GPIOS
#define PALMAS_TEGRA_IRQ_BASE	TEGRA_NR_IRQS

#define CAM_RSTN TEGRA_GPIO_PBB3
#define CAM_FLASH_STROBE TEGRA_GPIO_PBB4
#define CAM2_PWDN TEGRA_GPIO_PBB6
#define CAM1_PWDN TEGRA_GPIO_PBB5
#define CAM_AF_PWDN TEGRA_GPIO_PBB7
#define CAM_BOARD_E1806

#define CAM_VCM_PWDN TEGRA_GPIO_PBB7
#define CAM_PWDN TEGRA_GPIO_PO7
#define CAM_1V2_EN TEGRA_GPIO_PH6
#define CAM_A2V85_EN TEGRA_GPIO_PH2
#define CAM_VCM2V85_EN TEGRA_GPIO_PH3
#define CAM_1V8_EN TEGRA_GPIO_PH7
#define CAM2_RST TEGRA_GPIO_PBB3

#define UTMI1_PORT_OWNER_XUSB   0x1
#define UTMI2_PORT_OWNER_XUSB   0x2
#define HSIC1_PORT_OWNER_XUSB   0x4
#define HSIC2_PORT_OWNER_XUSB   0x8

/* Touchscreen definitions */
#define TOUCH_GPIO_IRQ_RAYDIUM_SPI	TEGRA_GPIO_PK2
#define TOUCH_GPIO_RST_RAYDIUM_SPI	TEGRA_GPIO_PK4
#define TOUCH_SPI_ID			2	/*SPI 3 on flounder_interposer*/
#define TOUCH_SPI_CS			0	/*CS  0 on flounder_interposer*/

#define TOUCH_GPIO_IRQ_MAXIM_STI_SPI	TEGRA_GPIO_PK2
#define TOUCH_GPIO_RST_MAXIM_STI_SPI	TEGRA_GPIO_PX6

/* Audio-related GPIOs */
#define TEGRA_GPIO_CDC_IRQ	TEGRA_GPIO_PH4
#define TEGRA_GPIO_HP_DET		TEGRA_GPIO_PR7
#define TEGRA_GPIO_LDO_EN	TEGRA_GPIO_PR2

/*GPIOs used by board panel file */
#define DSI_PANEL_RST_GPIO_EVT_1_1 TEGRA_GPIO_PB4
#define DSI_PANEL_RST_GPIO      TEGRA_GPIO_PH5
#define DSI_PANEL_BL_PWM_GPIO   TEGRA_GPIO_PH1

/* HDMI Hotplug detection pin */
#define flounder_hdmi_hpd	TEGRA_GPIO_PN7

/* I2C related GPIOs */
/* Same for interposer and t124 */
#define TEGRA_GPIO_I2C1_SCL	TEGRA_GPIO_PC4
#define TEGRA_GPIO_I2C1_SDA	TEGRA_GPIO_PC5
#define TEGRA_GPIO_I2C2_SCL	TEGRA_GPIO_PT5
#define TEGRA_GPIO_I2C2_SDA	TEGRA_GPIO_PT6
#define TEGRA_GPIO_I2C3_SCL	TEGRA_GPIO_PBB1
#define TEGRA_GPIO_I2C3_SDA	TEGRA_GPIO_PBB2
#define TEGRA_GPIO_I2C4_SCL	TEGRA_GPIO_PV4
#define TEGRA_GPIO_I2C4_SDA	TEGRA_GPIO_PV5
#define TEGRA_GPIO_I2C5_SCL	TEGRA_GPIO_PZ6
#define TEGRA_GPIO_I2C5_SDA	TEGRA_GPIO_PZ7

/* AUO Display related GPIO */
#define en_vdd_bl       TEGRA_GPIO_PP2 /* DAP3_DOUT */
#define lvds_en         TEGRA_GPIO_PI0 /* GMI_WR_N */
#define refclk_en       TEGRA_GPIO_PG4 /* GMI_AD4 */

/* HID keyboard and trackpad irq same for interposer and t124 */
#define I2C_KB_IRQ	TEGRA_GPIO_PC7
#define I2C_TP_IRQ	TEGRA_GPIO_PW3

/* Qualcomm mdm9k modem related GPIO */
#define AP2MDM_VDD_MIN		TEGRA_GPIO_PO6
#define AP2MDM_PMIC_RESET_N	TEGRA_GPIO_PBB5
#define AP2MDM_WAKEUP		TEGRA_GPIO_PO1
#define AP2MDM_STATUS		TEGRA_GPIO_PO2
#define AP2MDM_ERRFATAL		TEGRA_GPIO_PO3
#define AP2MDM_IPC1		TEGRA_GPIO_PO4
#define AP2MDM_IPC2		TEGRA_GPIO_PO5
#define MDM2AP_ERRFATAL		TEGRA_GPIO_PV0
#define MDM2AP_HSIC_READY	TEGRA_GPIO_PV1
#define MDM2AP_VDD_MIN		TEGRA_GPIO_PCC2
#define MDM2AP_WAKEUP		TEGRA_GPIO_PS5
#define MDM2AP_STATUS		TEGRA_GPIO_PS6
#define MDM2AP_IPC3		TEGRA_GPIO_PS0
#define AP2MDM_CHNL_RDY_CPU	TEGRA_GPIO_PQ7

#define FLOUNDER_REV_EVT1_1 1
#define FLOUNDER_REV_EVT1_2 2
int flounder_get_hw_revision(void);
int flounder_get_eng_id(void);

#endif
