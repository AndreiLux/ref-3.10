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

/*                          
                                  
                                                 
  
                                                       
                                                  
                   
                          
                                  
  
                            
      
 */

#ifndef __ASM_ARM_ARCH_ODIN_SDHCI_H
#define __ASM_ARM_ARCH_ODIN_SDHCI_H

/* Include For UHS-I mode */
#include <linux/odin_mailbox.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/module.h>

/* Declare completion For UHS-I mode */
static DECLARE_COMPLETION(pmic_sd_completion);

/* Declare completion For UHS-I mode */
int sdhci_odin_switch_1_8v(void);
int sdhci_odin_switch_3_3v(void);
irqreturn_t sdhci_pmic_handler(int irq, void *data);
void sdhci_odin_iopad_ctrl(unsigned int iopad_set);
void sdhci_set_xinclk(unsigned int xinclk);

#define SDHCI_ODIN_SDC_CONF0		0x20020000
#define SDHCI_ODIN_DEB7				0x2003085C
#define SDHCI_IOPAD_1_8V			0x1
#define SDHCI_IOPAD_3_3V			0x2

struct sdhci_odin_platform_data {
	unsigned int is_8bit;
	unsigned int dev_id;
	unsigned long caps;
	unsigned int pm_caps;
	unsigned int cmd_timeout;
#ifndef CONFIG_COMMON_CLK
	unsigned int xin_clk;
#endif
#ifdef CONFIG_OF
	int			sdio_clk;
	uint8_t		sdio_port;
	bool		ext_cd_gpio_invert;
	uint8_t		ext_cd_gpio;
#endif /* CONFIG_OF */
};

#ifdef CONFIG_WIFI_CONTROL_FUNC
int sdhci_odin_wifi_carddetect(void);
#endif

#if defined(CONFIG_PM_RUNTIME)
#define ODIN_SDHCI_PM_TIMEOUT		1000		/* ms */
#endif /* CONFIG_PM_RUNTIME */
#endif
