/*
 * Copyright(c) 2012, Analogix Semiconductor. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _SLIMPORT_H
#define _SLIMPORT_H
#include <linux/notifier.h>

#define SSC_EN

#if 0
#if defined CONFIG_SLIMPORT_ANX7808
#define SSC_1
#define EYE_TEST
#define EDID_DEBUG_PRINT
#endif
#endif

#define AUX_ERR  1
#define AUX_OK   0

#define CBL_910K 11

#if defined CONFIG_SLIMPORT_ANX7808
#define LOG_TAG "[anx7808]"
#elif defined CONFIG_SLIMPORT_ANX7816
#define LOG_TAG "[anx7816]"
#elif defined CONFIG_SLIMPORT_ANX7805
#define LOG_TAG "[anx7805]"
#elif defined CONFIG_SLIMPORT_ANX7812
#define LOG_TAG "[anx7812]"
#else
#define LOG_TAG "[anxNULL]"
#endif

#define SLIMPORT_DEBUG

#ifdef SLIMPORT_DEBUG
#define SLIMPORT_DBG_INFO(fmt, args...) \
	pr_info(LOG_TAG": %s: " fmt, __func__, ##args)
#define SLIMPORT_DBG(fmt, args...) \
	pr_debug(LOG_TAG": %s: " fmt, __func__, ##args)
#define debug_printf(fmt, arg...) printk(fmt,##arg)
#define debug_puts(fmt) printk(fmt)
#define SLIMPORT_ERR(fmt, args...) \
	pr_err(LOG_TAG": %s: " fmt, __func__, ##args)
#else
#define SLIMPORT_DBG_INFO(fmt, args...)  do { } while(0)
#define SLIMPORT_DBG(fmt, args...) do { } while(0)
#define debug_printf(fmt, arg...) do { } while(0)
#define debug_puts(fmt) do { } while(0)
#define SLIMPORT_ERR(fmt, args...) do { } while(0)
#endif

#ifdef CONFIG_SLIMPORT_ANX7808
enum {
 I2C_RESUME = 0,
 I2C_SUSPEND,
 I2C_SUSPEND_IRQ,
};

extern bool sp_tx_hw_lt_done;
extern bool  sp_tx_hw_lt_enable;
extern bool	sp_tx_link_config_done ;
extern enum RX_CBL_TYPE  sp_tx_rx_type_backup;
extern unchar sp_tx_hw_hdcp_en;
extern unchar bedid_break;
#endif

extern enum SP_TX_System_State sp_tx_system_state;
extern enum RX_CBL_TYPE sp_tx_rx_type;

extern bool is_slimport_vga(void);
extern bool is_slimport_dp(void);
extern struct atomic_notifier_head hdmi_hpd_notifier_list; 

extern unchar sp_tx_pd_mode;

int sp_read_reg(uint8_t slave_addr, uint8_t offset, uint8_t *buf);
int sp_write_reg(uint8_t slave_addr, uint8_t offset, uint8_t value);
void sp_tx_hardware_poweron(void);
void sp_tx_hardware_powerdown(void);
int slimport_read_edid_block(int block, uint8_t *edid_buf);
unchar sp_get_link_bw(void);
void sp_set_link_bw(unchar link_bw);

#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
void slimport_set_hdmi_hpd(int on);
#endif

#if defined (CONFIG_SLIMPORT_ANX7808) || defined (CONFIG_SLIMPORT_ANX7812)
bool slimport_is_connected(void);
#else
static inline bool slimport_is_connected(void)
{
	return false;
}
#endif
#endif
