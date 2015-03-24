#ifndef _SLIMPORT_H
#define _SLIMPORT_H

#include <linux/gpio.h>
#include <linux/delay.h>

/*
 * Below five GPIOs are example  for AP to control the Slimport chip.
 * Different AP needs to configure these control pins to corresponding GPIOs of AP.
 */

/*******************Slimport Control************************/

#define SP_TX_PWR_V10_CTRL            (82)//AP IO Control - Power+V12
#define SP_TX_HW_RESET                (83)//AP IO Control - Reset
#define SLIMPORT_CABLE_DETECT         (84)//AP IO Input - Cable detect
#define SP_TX_CHIP_PD_CTRL            (85)//AP IO Control - CHIP_PW_HV

#define SP_I2C_DATA (47)
#define SP_I2C_SCL (48)
#define CONFIG_I2C_GPIO
#define SSC_EN
//#define SSC_1
//#define HDCP_EN


/* 7730 audio related registers */
#define REG_7730_72     	5
#define AUD_INFOFRM_TYPE_IDX    0x83

#define AUX_ERR  1
#define AUX_OK   0

bool slimport_goes_powerdown_mode(void);
bool slimport_is_cable_connected(void);

void Colorado2_work_func(struct work_struct * work);
int sp_write_reg(unsigned char dev, unsigned char dev_offset, unsigned char d);
int sp_read_reg(unsigned char dev, unsigned char dev_offset, unsigned char *d);
int __i2c_read_byte(unsigned char dev, unsigned char dev_offset, unsigned char *buf);

#define ENABLE_READ_EDID

#define idata
#define code
#define TX_P0 0x70
#define TX_P1 0x7A
#define TX_P2 0x72

#define RX_P0 0x7e
#define RX_P1 0x80

#define debug_puts(fmt) pr_err("%s:%d: " fmt, __func__, __LINE__)
#define debug_printf(fmt, arg...) \
	pr_err("%s:%d: " fmt, __func__, __LINE__, ##arg)

#define delay_ms(msec) mdelay(msec)

#define __read_reg(dev, dev_offset, buf) __i2c_read_byte(dev, dev_offset, buf)

#define sp_write_reg_or(address, offset, mask, ret) do { \
	unchar tmp; \
	__read_reg(address, offset, &tmp); \
	*ret = sp_write_reg(address, offset, (tmp | (mask))); \
	} while (0)

#define sp_write_reg_and(address, offset, mask, ret) do { \
	unchar tmp; \
	__read_reg(address, offset, &tmp); \
	*ret = sp_write_reg(address, offset, (tmp & (mask))); \
	} while (0)

#define sp_write_reg_and_or(address, offset, and_mask, or_mask, ret) do {\
	unchar tmp; \
	__read_reg(address, offset, &tmp); \
	*ret = sp_write_reg(address, offset, (tmp & and_mask) | (or_mask)); \
	} while (0)

#define sp_write_reg_or_and(address, offset, or_mask, and_mask, ret) do {\
	unchar tmp; \
	__read_reg(address, offset, &tmp); \
	*ret = sp_write_reg(address, offset, (tmp | or_mask) & (and_mask)); \
	} while (0)

extern void Acquire_wakelock(void);
extern void Release_wakelock(void);
extern bool get_cableproc_status(void);

int hardware_power_ctl(int stat);
unsigned char check_cable_det_pin(void);

int slimport_read_edid_block(int block, uint8_t *edid_buf);
unsigned char slimport_get_link_bw(void);
unsigned char sp_get_ds_cable_type(void);
#ifdef CONFIG_SLIMPORT_FAST_CHARGE
enum CHARGING_STATUS sp_get_ds_charge_type(void);
#endif
bool slimport_is_connected(void);
int get_slimport_hdcp_status(void);
#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT
extern int check_sink_mode(void);
#endif

#endif

