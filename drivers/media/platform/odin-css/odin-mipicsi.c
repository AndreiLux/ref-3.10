/*
 * MIPI-CSI driver
 *
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

#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#if CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#endif

#include "odin-css.h"
#include "odin-css-system.h"
#include "odin-css-clk.h"
#include "odin-mipicsi.h"
#include "odin-css-utils.h"
#include "odin-isp-wrap.h"

#define MIPICSI_INT_ENABLE 1

static volatile unsigned int mipi_remote_control = 0;
static struct csi_hardware *mipicsi_hardware = NULL;

typedef struct {
	char arg[32];
} mipi_args_list;

static int odin_mipicsi_interrupt_clear(css_mipi_csi_ch csi_ch, int int_mask);
static unsigned int odin_mipicsi_get_interrupt_status(css_mipi_csi_ch csi_ch);
static unsigned int odin_mipicsi_get_error_status(css_mipi_csi_ch csi_ch);
static int odin_mipicsi_int_handler(css_mipi_csi_ch csi_ch,
			unsigned int int_status);
static irqreturn_t odin_mipicsi_ch0_irq(int irq, void *dev_id);
static irqreturn_t odin_mipicsi_ch1_irq(int irq, void *dev_id);
static irqreturn_t odin_mipicsi_ch2_irq(int irq, void *dev_id);

static int odin_mipicsi_hw_init(css_mipi_csi_ch csi_ch);
static int odin_mipicsi_hw_deinit(css_mipi_csi_ch csi_ch);
static int odin_mipicsi_set_vsync_delay(css_mipi_csi_ch csi_ch, int pclkcnt);
static int odin_mipicsi_set_lane_enable(css_mipi_csi_ch csi_ch, int lanecnt);
static int odin_mipicsi_set_lane_disable(css_mipi_csi_ch csi_ch);
static int odin_mipicsi_set_lane_pdown_on(css_mipi_csi_ch csi_ch);
static int odin_mipicsi_set_lane_pdown_off(css_mipi_csi_ch csi_ch);
static int odin_mipicsi_set_emb_data_type(css_mipi_csi_ch csi_ch,
			int type0, int type1, int emb_linecnt);
static int odin_mipicsi_set_csi_control(css_mipi_csi_ch csi_ch, int rx_enable,
			int vsync_pol);
static int odin_mipicsi_get_csi_control(css_mipi_csi_ch csi_ch,
			CSS_MIPICSI_CSI2_CTRL *csi2_ctrl);
static int odin_mipicsi_set_check_error(css_mipi_csi_ch csi_ch, int check_err);

static int odin_mipicsi_set_tif_clear(css_mipi_csi_ch csi_ch);
static int odin_mipicsi_set_hsrx_clk_inversion(css_mipi_csi_ch csi_ch,
			unsigned int clks, int inversion);
static int odin_mipicsi_set_continuous_mode(css_mipi_csi_ch csi_ch,
			unsigned int hsrxctrl);
static void odin_mipicsi_channel_reset(css_mipi_csi_ch csi_ch);
static int odin_mipicsi_set_rx_bandwidth(css_mipi_csi_ch csi_ch, int level);


#ifdef ODIN_MIPICSI_ERR_REPORT
struct work_struct mipicsi_report[CSI_CH_MAX];
static unsigned int cur_error[CSI_CH_MAX];
static unsigned int cur_error_cnt[CSI_CH_MAX];

static void mipicsi_set_error(css_mipi_csi_ch csi_ch, unsigned int err)
{
	cur_error[csi_ch] = err;
}

static void mipicsi0_err_log_wq(struct work_struct *work)
{
	if (cur_error[CSI_CH0] != 0) {
		css_info("mipicsi0 err: 0x%08x(0x%04x):%ld\n", cur_error[CSI_CH0],
				odin_mipicsi_get_word_count(CSI_CH0), cur_error_cnt[CSI_CH0]);
	}
	cur_error[CSI_CH0] = 0;
	cur_error_cnt[CSI_CH0] = 1;
}

static void mipicsi1_err_log_wq(struct work_struct *work)
{
	if (cur_error[CSI_CH1] != 0) {
		css_info("mipicsi1 err: 0x%08x(0x%04x):%ld\n", cur_error[CSI_CH1],
				odin_mipicsi_get_word_count(CSI_CH1), cur_error_cnt[CSI_CH1]);
	}
	cur_error[CSI_CH1] = 0;
	cur_error_cnt[CSI_CH1] = 1;
}

static void mipicsi2_err_log_wq(struct work_struct *work)
{
	if (cur_error[CSI_CH2] != 0) {
		css_info("mipicsi2 err: 0x%08x(0x%04x):%ld\n", cur_error[CSI_CH2],
				odin_mipicsi_get_word_count(CSI_CH2), cur_error_cnt[CSI_CH2]);
	}
	cur_error[CSI_CH2] = 0;
	cur_error_cnt[CSI_CH2] = 1;
}


static void mipicsi_err_log(css_mipi_csi_ch csi_ch, unsigned int err)
{
	mipicsi_set_error(csi_ch, err);

	if (cur_error_cnt[csi_ch] == 0) {
		schedule_work(&mipicsi_report[csi_ch]);
	} else {
		if (cur_error_cnt[csi_ch] > 10)
			schedule_work(&mipicsi_report[csi_ch]);
	}
	cur_error_cnt[csi_ch]++;
}
#else
static void mipicsi_err_log(css_mipi_csi_ch csi_ch, unsigned int err)
{
	csi_ch = csi_ch;
	err = err;
	return;
}
#endif

static inline void set_csi_hw(struct csi_hardware *hw)
{
	mipicsi_hardware = hw;
}

static inline struct csi_hardware *get_csi_hw(void)
{
	BUG_ON(!mipicsi_hardware);
	return mipicsi_hardware;
}

#define CHECK_MIPI_AVALIABLE() {					\
	if (mipi_remote_control == 1) { 				\
		return CSS_ERROR_MIPI_CSI_ACCESS_DENIED;	\
	}												\
}

int odin_mipicsi_set_remote_control(int set_val)
{
	int ret = CSS_SUCCESS;

	if (set_val)
		mipi_remote_control = 1;
	else
		mipi_remote_control = 0;

	return ret;
}

int odin_mipicsi_get_remote_control(void)
{
	return (int)mipi_remote_control;
}

inline unsigned char mipi_get_args(mipi_args_list *arg_list, char *string,
						const char *delimiters)
{
	unsigned int i = 0;
	char *str_arg = NULL;

	str_arg = css_strtok(string, delimiters);
	while (str_arg != NULL)
	{
		int j = 0;
		sprintf(arg_list[i].arg ,"%s", str_arg);
		for (j = 0; j < 32; j++) {
			if (arg_list[i].arg[j] == 0xA) {
				arg_list[i].arg[j] = 0x0;
			}
		}
		i++;
		/* Get next args: */
		str_arg = css_strtok(NULL, delimiters);
	}
	return i;
}

static int mipicsi_store_ctrl(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t len)
{
	#define MIPI_CTRL_REG_WRITE 0
	#define MIPI_CTRL_REG_READ	1
	#define MIPI_CTRL_ENABLE	2
	#define MIPI_CTRL_DISABLE	3
	#define MIPI_CTRL_REMODE	4

	mipi_args_list arg_list[10];
	char str_cmd[256];
	const char delimiters[] = " .,;:!-";
	u32 n_args = 0;
	u32 mode = 0;
	u32 channel = 0;

	memcpy(str_cmd, buf, len);

	n_args = mipi_get_args(arg_list, str_cmd, delimiters);

	if (!strcmp(arg_list[0].arg, "regwrite"))
		mode = MIPI_CTRL_REG_WRITE;

	if (!strcmp(arg_list[0].arg, "regread"))
		mode = MIPI_CTRL_REG_READ;

	if (!strcmp(arg_list[0].arg, "enable"))
		mode = MIPI_CTRL_ENABLE;

	if (!strcmp(arg_list[0].arg, "disable"))
		mode = MIPI_CTRL_DISABLE;

	if (!strcmp(arg_list[0].arg, "remote"))
		mode = MIPI_CTRL_REMODE;

	if (mode != MIPI_CTRL_REMODE) {
		channel = simple_strtoul(arg_list[1].arg, NULL, 10);

		if (channel >= CSI_CH_MAX) {
			css_err("invalid channel number %d\n", channel);
			return len;
		}
	}

	switch (mode) {
		case MIPI_CTRL_REG_WRITE:
		{
			struct csi_hardware *csi_hw = get_csi_hw();
			unsigned long offset = 0;
			unsigned long data = 0;
			unsigned long reg_phys = 0;

			offset = simple_strtoul(arg_list[2].arg, NULL, 16);
			data = simple_strtoul(arg_list[3].arg, NULL, 16);

			reg_phys = (unsigned int)(csi_hw->iores[channel]->start) + offset;

			if (offset > 0xFFFF) {
				css_err("invalid offset range 0x%x\n", (u32)offset);
				return len;
			}

			css_info("[SYSFS]mipi-csi write 0x%x 0x%x\n",
						(u32)reg_phys, (u32)data);
			writel(data, csi_hw->iobase[channel] + offset);
		}
		break;
		case MIPI_CTRL_REG_READ:
		{
			struct csi_hardware *csi_hw = get_csi_hw();
			unsigned long offset = 0;
			unsigned long data = 0;
			unsigned long reg_phys = 0;

			offset = simple_strtoul(arg_list[2].arg, NULL, 16);

			reg_phys = (unsigned int)(csi_hw->iores[channel]->start) + offset;

			if (offset > 0xFFFF) {
				css_err("invalid offset range 0x%x\n", (u32)offset);
				return len;
			}

			data = readl(csi_hw->iobase[channel] + offset);

			css_info("[SYSFS]mipi-csi read 0x%x 0x%x\n",
						(u32)reg_phys, (u32)data);
		}
		break;
		case MIPI_CTRL_ENABLE:
		{
			unsigned long mipi_lane_cnt;
			unsigned long mipi_speed;
			unsigned long mipi_type;
			unsigned long mipi_syncpol;
			unsigned long delay;
			unsigned long hsrxmode;
			int remote_state;
			int ret = 0;

			mipi_lane_cnt	= simple_strtoul(arg_list[2].arg, NULL, 10);
			mipi_speed		= simple_strtoul(arg_list[3].arg, NULL, 10);
			mipi_type		= simple_strtoul(arg_list[4].arg, NULL, 10);
			mipi_syncpol	= simple_strtoul(arg_list[5].arg, NULL, 10);
			delay			= simple_strtoul(arg_list[6].arg, NULL, 10);
			hsrxmode		= simple_strtoul(arg_list[7].arg, NULL, 10);

			if (mipi_lane_cnt > 4) {
				css_err("invalid lane count %ld\n", mipi_lane_cnt);
				return len;
			}

			if (mipi_speed > 1000) {
				css_err("invalid mipi speed %ld\n", mipi_speed);
				return len;
			}

			if (mipi_syncpol > 1) {
				css_err("invalid mipi sync polarity %ld\n", mipi_syncpol);
				return len;
			}

			if (delay > 0x3FFFF) {
				css_err("invalid mipi vsync delay %ld\n", delay);
				return len;
			}

			if (hsrxmode > 1) {
				css_err("invalid mipi hsrx mode %ld\n", hsrxmode);
				return len;
			}

			remote_state = odin_mipicsi_get_remote_control();
			if (remote_state) {
				odin_mipicsi_set_remote_control(0);
			}

			ret = odin_mipicsi_channel_rx_enable(channel, mipi_lane_cnt,
						mipi_speed, mipi_type, mipi_syncpol, delay, hsrxmode);
			if (ret == CSS_SUCCESS) {
				css_info("[SYSFS] channel %d rx_enable\n", channel);
				css_info("        lanecnt : %ld\n", mipi_lane_cnt);
				css_info("        speed   : %ld\n", mipi_speed);
				css_info("        type    : %ld\n", mipi_type);
				css_info("        syncpol : %ld\n", mipi_syncpol);
				css_info("        delay   : %ld\n", delay);
				css_info("        hsrx    : %ld\n", hsrxmode);
			} else {
				css_info("[SYSFS] channel %d rx_enable fail %x\n",
						channel, ret);
			}

			if (remote_state) {
				odin_mipicsi_set_remote_control(1);
			}
		}
		break;
		case MIPI_CTRL_DISABLE:
		{
			int ret = 0;

			ret = odin_mipicsi_channel_rx_disable(channel);
			if (ret == CSS_SUCCESS) {
				css_info("[SYSFS] channel %d rx_disable\n", channel);
			} else {
				css_info("[SYSFS] channel %d rx_disable fail %x\n",
						channel, ret);
			}
		}
		break;
		case MIPI_CTRL_REMODE:
		{
			unsigned long remote_en = 0;

			remote_en = simple_strtoul(arg_list[1].arg, NULL, 10);
			if (remote_en > 1)
				css_err("invalid remote option %ld\n", remote_en);

			odin_mipicsi_set_remote_control(remote_en);
		}
		break;
	}

	return len;
}

static int mipicsi_show_ctrl(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	char msg[256] = {0, };

	sprintf(msg, "CSS_LOG_ERR enabled\n");
	strcat(buf, msg);
	return strlen(buf);
}

static DEVICE_ATTR(mipicsi, 0644, mipicsi_show_ctrl, mipicsi_store_ctrl);


void odin_mipicsi_reg_dump(void)
{
	int i = 0;
	unsigned int err_state = 0;
	struct csi_hardware *csi_hw = get_csi_hw();

	if (csi_hw == NULL)
		return;

	if (css_clock_available(CSS_CLK_MIPI0)) {
		css_info("mipicsi0 reg dump #1\n");
		for (i = 0; i < 5; i++) {
			printk("0x%08x ", readl(csi_hw->iobase[0] + (i * 0x10)));
			printk("0x%08x ", readl(csi_hw->iobase[0] + (i * 0x10) + 0x4));
			printk("0x%08x ", readl(csi_hw->iobase[0] + (i * 0x10) + 0x8));
			printk("0x%08x\n", readl(csi_hw->iobase[0] + (i * 0x10) + 0xC));
			schedule();
		}
		css_info("mipicsi0 check err\n");
		err_state = odin_mipicsi_get_error_status(0);
		printk("error[1]: 0x%08x\n", err_state);
		odin_mipicsi_interrupt_clear(0, INT_CSI_ERROR);
		msleep(30);
		err_state = odin_mipicsi_get_error_status(0);
		printk("error[2]: 0x%08x\n", err_state);
		odin_mipicsi_interrupt_clear(0, INT_CSI_ERROR);
		msleep(30);
		err_state = odin_mipicsi_get_error_status(0);
		printk("error[3]: 0x%08x\n", err_state);
		odin_mipicsi_interrupt_clear(0, INT_CSI_ERROR);

		odin_mipicsi_channel_reset(0);

		css_info("mipicsi0 reg dump #2\n");
		for (i = 0; i < 5; i++) {
			printk("0x%08x ", readl(csi_hw->iobase[0] + (i * 0x10)));
			printk("0x%08x ", readl(csi_hw->iobase[0] + (i * 0x10) + 0x4));
			printk("0x%08x ", readl(csi_hw->iobase[0] + (i * 0x10) + 0x8));
			printk("0x%08x\n", readl(csi_hw->iobase[0] + (i * 0x10) + 0xC));
			schedule();
		}
		css_info("mipicsi0 check err\n");
		err_state = odin_mipicsi_get_error_status(0);
		printk("error[1]: 0x%08x\n", err_state);
		odin_mipicsi_interrupt_clear(0, INT_CSI_ERROR);
		msleep(30);
		err_state = odin_mipicsi_get_error_status(0);
		printk("error[2]: 0x%08x\n", err_state);
		odin_mipicsi_interrupt_clear(0, INT_CSI_ERROR);
		msleep(30);
		err_state = odin_mipicsi_get_error_status(0);
		printk("error[3]: 0x%08x\n", err_state);
		odin_mipicsi_interrupt_clear(0, INT_CSI_ERROR);

	}

	if (css_clock_available(CSS_CLK_MIPI1)) {
		css_info("mipicsi1 reg dump #1\n");
		for (i = 0; i < 5; i++) {
			printk("0x%08x ", readl(csi_hw->iobase[1] + (i * 0x10)));
			printk("0x%08x ", readl(csi_hw->iobase[1] + (i * 0x10) + 0x4));
			printk("0x%08x ", readl(csi_hw->iobase[1] + (i * 0x10) + 0x8));
			printk("0x%08x\n", readl(csi_hw->iobase[1] + (i * 0x10) + 0xC));
			schedule();
		}
		css_info("mipicsi1 check err\n");
		err_state = odin_mipicsi_get_error_status(1);
		printk("error[1]: 0x%08x\n", err_state);
		odin_mipicsi_interrupt_clear(1, INT_CSI_ERROR);
		msleep(30);
		err_state = odin_mipicsi_get_error_status(1);
		printk("error[2]: 0x%08x\n", err_state);
		odin_mipicsi_interrupt_clear(1, INT_CSI_ERROR);
		msleep(30);
		err_state = odin_mipicsi_get_error_status(1);
		printk("error[3]: 0x%08x\n", err_state);
		odin_mipicsi_interrupt_clear(1, INT_CSI_ERROR);

		odin_mipicsi_channel_reset(1);

		css_info("mipicsi1 reg dump #2\n");
		for (i = 0; i < 5; i++) {
			printk("0x%08x ", readl(csi_hw->iobase[1] + (i * 0x10)));
			printk("0x%08x ", readl(csi_hw->iobase[1] + (i * 0x10) + 0x4));
			printk("0x%08x ", readl(csi_hw->iobase[1] + (i * 0x10) + 0x8));
			printk("0x%08x\n", readl(csi_hw->iobase[1] + (i * 0x10) + 0xC));
			schedule();
		}
		css_info("mipicsi1 check err\n");
		err_state = odin_mipicsi_get_error_status(1);
		printk("error[1]: 0x%08x\n", err_state);
		odin_mipicsi_interrupt_clear(1, INT_CSI_ERROR);
		msleep(30);
		err_state = odin_mipicsi_get_error_status(1);
		printk("error[2]: 0x%08x\n", err_state);
		odin_mipicsi_interrupt_clear(1, INT_CSI_ERROR);
		msleep(30);
		err_state = odin_mipicsi_get_error_status(1);
		printk("error[3]: 0x%08x\n", err_state);
		odin_mipicsi_interrupt_clear(1, INT_CSI_ERROR);
	}

	if (css_clock_available(CSS_CLK_MIPI2)) {
		css_info("mipicsi2 reg dump #1\n");
		for (i = 0; i < 5; i++) {
			printk("0x%08x ", readl(csi_hw->iobase[2] + (i * 0x10)));
			printk("0x%08x ", readl(csi_hw->iobase[2] + (i * 0x10) + 0x4));
			printk("0x%08x ", readl(csi_hw->iobase[2] + (i * 0x10) + 0x8));
			printk("0x%08x\n", readl(csi_hw->iobase[2] + (i * 0x10) + 0xC));
			schedule();
		}
		css_info("mipicsi2 check err\n");
		err_state = odin_mipicsi_get_error_status(2);
		printk("error[1]: 0x%08x\n", err_state);
		odin_mipicsi_interrupt_clear(2, INT_CSI_ERROR);
		msleep(30);
		err_state = odin_mipicsi_get_error_status(2);
		printk("error[2]: 0x%08x\n", err_state);
		odin_mipicsi_interrupt_clear(2, INT_CSI_ERROR);
		msleep(30);
		err_state = odin_mipicsi_get_error_status(2);
		printk("error[3]: 0x%08x\n", err_state);
		odin_mipicsi_interrupt_clear(2, INT_CSI_ERROR);

		odin_mipicsi_channel_reset(2);

		css_info("mipicsi2 reg dump #2\n");
		for (i = 0; i < 5; i++) {
			printk("0x%08x ", readl(csi_hw->iobase[2] + (i * 0x10)));
			printk("0x%08x ", readl(csi_hw->iobase[2] + (i * 0x10) + 0x4));
			printk("0x%08x ", readl(csi_hw->iobase[2] + (i * 0x10) + 0x8));
			printk("0x%08x\n", readl(csi_hw->iobase[2] + (i * 0x10) + 0xC));
			schedule();
		}
		css_info("mipicsi2 check err\n");
		err_state = odin_mipicsi_get_error_status(2);
		printk("error[1]: 0x%08x\n", err_state);
		odin_mipicsi_interrupt_clear(2, INT_CSI_ERROR);
		msleep(30);
		err_state = odin_mipicsi_get_error_status(2);
		printk("error[2]: 0x%08x\n", err_state);
		odin_mipicsi_interrupt_clear(2, INT_CSI_ERROR);
		msleep(30);
		err_state = odin_mipicsi_get_error_status(2);
		printk("error[3]: 0x%08x\n", err_state);
		odin_mipicsi_interrupt_clear(2, INT_CSI_ERROR);

	}
}

unsigned int odin_mipicsi_get_error(css_mipi_csi_ch csi_ch)
{
	unsigned int error = 0;
	if (css_clock_available(csi_ch)) {
		error = odin_mipicsi_get_error_status(csi_ch);
		odin_mipicsi_interrupt_clear(csi_ch, INT_CSI_ERROR);
		error = error & 0x10440003;
	}
	return error;
}

int odin_mipicsi_reg_update(css_mipi_csi_ch csi_ch)
{
	int ret = 0;
	struct csi_hardware *csi_hw = get_csi_hw();
	CSS_MIPICSI_CSI2_CTRL csi2_ctrl;

	csi2_ctrl.as32bits = readl(csi_hw->iobase[csi_ch] + CSI2_CTRL);
	csi2_ctrl.asbits.reg_up_en = 1;
	writel(csi2_ctrl.as32bits, csi_hw->iobase[csi_ch] + CSI2_CTRL);

	return ret;
}

int odin_mipicsi_status_clear(css_mipi_csi_ch csi_ch)
{
	int ret = 0;
	struct csi_hardware *csi_hw = get_csi_hw();
	CSS_MIPICSI_INTERRUPT03 int03;

	int03.as32bits = readl(csi_hw->iobase[csi_ch] + INTERRUPT_03);
	int03.as32bits = int03.as32bits & ~(0x00001F00);
	writel(int03.as32bits, csi_hw->iobase[csi_ch] + INTERRUPT_03);

	return ret;
}

int odin_mipicsi_d_phy_reset_on(css_mipi_csi_ch csi_ch)
{
	int ret = CSS_SUCCESS;
	struct csi_hardware *csi_hw = get_csi_hw();
	CSS_MIPICSI_DPHY_CTRL dphy_ctrl;

	dphy_ctrl.as32bits = readl(csi_hw->iobase[csi_ch] + DPHY_CTRL);
	dphy_ctrl.asbits.d_phy_reset = 0;
	writel(dphy_ctrl.as32bits, csi_hw->iobase[csi_ch] + DPHY_CTRL);

	mdelay(1);

	return ret;
}

int odin_mipicsi_d_phy_reset_off(css_mipi_csi_ch csi_ch)
{
	int ret = CSS_SUCCESS;
	struct csi_hardware *csi_hw = get_csi_hw();
	CSS_MIPICSI_DPHY_CTRL dphy_ctrl;

	dphy_ctrl.as32bits = readl(csi_hw->iobase[csi_ch] + DPHY_CTRL);
	dphy_ctrl.asbits.d_phy_reset = 1;
	writel(dphy_ctrl.as32bits, csi_hw->iobase[csi_ch] + DPHY_CTRL);

	return ret;
}

int odin_mipicsi_channel_rx_enable(css_mipi_csi_ch csi_ch, unsigned int lanecnt,
				unsigned int speed, unsigned int type, unsigned int syncpol,
				unsigned int delay, unsigned int hsrxmode)
{
	int ret = CSS_SUCCESS;

	CHECK_MIPI_AVALIABLE();

	if (csi_ch > CSI_CH2)
		return CSS_ERROR_MIPI_CSI_INVALID_LANE;

	if (csi_ch == CSI_CH2 && lanecnt > 2)
		return CSS_ERROR_MIPI_CSI_INVALID_LANE_CNT;

	css_info("CH%02d lane:%d spd:%d sync:%d hs:%d\n", csi_ch, lanecnt, speed,
				syncpol, hsrxmode);

	ret = odin_mipicsi_hw_init(csi_ch);
	if (ret != CSS_SUCCESS) {
		css_err("hw_init error!\n");
		goto mipicsi_set_fail;
	}

	odin_mipicsi_interrupt_disable(csi_ch, INT_CSI_ERROR);

	ret = odin_mipicsi_set_vsync_delay(csi_ch, delay);
	if (ret != CSS_SUCCESS) {
		css_err("vsync_delay error!\n");
		goto mipicsi_set_fail;
	}

	ret = odin_mipicsi_set_lane_pdown_on(csi_ch);
	if (ret != CSS_SUCCESS) {
		css_err("lane_pdown_on error!\n");
		goto mipicsi_set_fail;
	}

	ret = odin_mipicsi_d_phy_reset_on(csi_ch);
	if (ret != CSS_SUCCESS) {
		css_err("d_phy_reset_on error!\n");
		goto mipicsi_set_fail;
	}

	ret = odin_mipicsi_set_csi_control(csi_ch, 1, syncpol);
	if (ret != CSS_SUCCESS) {
		css_err("csi_control error!\n");
		goto mipicsi_set_fail;
	}

	ret = odin_mipicsi_set_check_error(csi_ch, 1);
	if (ret != CSS_SUCCESS) {
		css_err("check_error error!\n");
		goto mipicsi_set_fail;
	}

	ret = odin_mipicsi_set_tif_clear(csi_ch);
	if (ret != CSS_SUCCESS) {
		css_err("tif_clear error!\n");
		goto mipicsi_set_fail;
	}

	ret = odin_mipicsi_set_lane_enable(csi_ch, lanecnt);
	if (ret != CSS_SUCCESS) {
		css_err("lane_enable error!\n");
		goto mipicsi_set_fail;
	}

	ret = odin_mipicsi_set_speed(csi_ch, speed);
	if (ret != CSS_SUCCESS) {
		css_err("set_speed error!\n");
		goto mipicsi_set_fail;
	}

	ret = odin_mipicsi_set_continuous_mode(csi_ch, hsrxmode);
	if (ret != CSS_SUCCESS) {
		css_err("set_continuous_mode error!\n");
		goto mipicsi_set_fail;
	}

	ret = odin_mipicsi_set_lane_pdown_off(csi_ch);
	if (ret != CSS_SUCCESS) {
		css_err("lane_pdown_off error!\n");
		goto mipicsi_set_fail;
	}

	ret = odin_mipicsi_d_phy_reset_off(csi_ch);
	if (ret != CSS_SUCCESS) {
		css_err("d_phy_reset_off error!\n");
		goto mipicsi_set_fail;
	}

#if 1
	if (csi_ch == CSI_CH0) {
		ret = odin_mipicsi_set_hsrx_clk_inversion(csi_ch, HSRX_IN_SP_CK, 1);
		if (ret != CSS_SUCCESS) {
			goto mipicsi_set_fail;
		}
		ret = odin_mipicsi_set_rx_bandwidth(csi_ch, 3);
		if (ret != CSS_SUCCESS) {
			goto mipicsi_set_fail;
		}
	}
#endif

#if 0
	ret = odin_mipicsi_set_emb_data_type(csi_ch, 0x12, 0x13, 0);
	if (ret != CSS_SUCCESS) {
		goto mipicsi_set_fail;
	}
#endif

#ifdef ODIN_MIPICSI_ERR_REPORT
	odin_mipicsi_interrupt_enable(csi_ch, INT_CSI_ERROR);
#endif

	return CSS_SUCCESS;

mipicsi_set_fail:
	return ret;
}

int odin_mipicsi_channel_rx_disable(css_mipi_csi_ch csi_ch)
{
	int ret = CSS_SUCCESS;

	CHECK_MIPI_AVALIABLE();

	if (csi_ch > CSI_CH2)
		return CSS_ERROR_MIPI_CSI_INVALID_LANE;

	odin_mipicsi_interrupt_disable(csi_ch, INT_CSI_ERROR);

	ret = odin_mipicsi_set_lane_disable(csi_ch);
	if (ret != CSS_SUCCESS) {
		css_err("lane_disable error!\n");
		goto mipicsi_set_fail;
	}

	ret = odin_mipicsi_set_csi_control(csi_ch, 0, 0);
	if (ret != CSS_SUCCESS) {
		css_err("csi_control error!\n");
		goto mipicsi_set_fail;
	}

	ret = odin_mipicsi_set_check_error(csi_ch, 0);
	if (ret != CSS_SUCCESS) {
		css_err("set_check_error error!\n");
		goto mipicsi_set_fail;
	}

	ret = odin_mipicsi_set_lane_pdown_on(csi_ch);
	if (ret != CSS_SUCCESS) {
		css_err("lane_pdown_on error!\n");
		goto mipicsi_set_fail;
	}

	css_info("MIPI-CSI CH%02d rx disable\n", csi_ch);

mipicsi_set_fail:
	odin_mipicsi_hw_deinit(csi_ch);
	return ret;
}

int odin_mipicsi_interrupt_enable(css_mipi_csi_ch csi_ch, int int_mask)
{
#ifdef	MIPICSI_INT_ENABLE
	struct csi_hardware *csi_hw = get_csi_hw();
	CSS_MIPICSI_INTERRUPT03 interrupt03;
	unsigned long flags;

	interrupt03.as32bits = readl(csi_hw->iobase[csi_ch] + INTERRUPT_03);
	interrupt03.as32bits &= ~(int_mask << INT_MASK_OFFSET);

	spin_lock_irqsave(&csi_hw->slock[csi_ch], flags);
	writel(interrupt03.as32bits, csi_hw->iobase[csi_ch] + INTERRUPT_03);
	spin_unlock_irqrestore(&csi_hw->slock[csi_ch], flags);
#endif

	return 0;
}

int odin_mipicsi_interrupt_disable(css_mipi_csi_ch csi_ch, int int_mask)
{
#ifdef	MIPICSI_INT_ENABLE
	struct csi_hardware *csi_hw = get_csi_hw();
	CSS_MIPICSI_INTERRUPT03 interrupt03;
	unsigned long flags;

	interrupt03.as32bits = readl(csi_hw->iobase[csi_ch] + INTERRUPT_03);
	interrupt03.as32bits |= (int_mask << INT_MASK_OFFSET);

	spin_lock_irqsave(&csi_hw->slock[csi_ch], flags);
	writel(interrupt03.as32bits, csi_hw->iobase[csi_ch] + INTERRUPT_03);
	spin_unlock_irqrestore(&csi_hw->slock[csi_ch], flags);
#endif

	return 0;
}

static int odin_mipicsi_interrupt_clear(css_mipi_csi_ch csi_ch, int int_mask)
{
	struct csi_hardware *csi_hw = get_csi_hw();
	CSS_MIPICSI_INTERRUPT03 interrupt03;
	unsigned long flags;

	interrupt03.as32bits = readl(csi_hw->iobase[csi_ch] + INTERRUPT_03);
	interrupt03.as32bits |= (int_mask << INT_CLEAR_OFFSET);

	spin_lock_irqsave(&csi_hw->slock[csi_ch], flags);
	writel(interrupt03.as32bits, csi_hw->iobase[csi_ch] + INTERRUPT_03);
	spin_unlock_irqrestore(&csi_hw->slock[csi_ch], flags);

	return 0;
}

static unsigned int odin_mipicsi_get_interrupt_status(css_mipi_csi_ch csi_ch)
{
	struct csi_hardware *csi_hw = get_csi_hw();
	CSS_MIPICSI_INTERRUPT03 interrupt03;
	unsigned long flags;

	spin_lock_irqsave(&csi_hw->slock[csi_ch], flags);
	interrupt03.as32bits = readl(csi_hw->iobase[csi_ch] + INTERRUPT_03);
	spin_unlock_irqrestore(&csi_hw->slock[csi_ch], flags);

	return (interrupt03.as32bits & 0x1F);
}

static unsigned int odin_mipicsi_get_error_status(css_mipi_csi_ch csi_ch)
{
	struct csi_hardware *csi_hw = get_csi_hw();
	CSS_MIPICSI_ERROR_STATE error_state;
	unsigned long flags;

	spin_lock_irqsave(&csi_hw->slock[csi_ch], flags);
	error_state.as32bits = readl(csi_hw->iobase[csi_ch] + CSI2_ERROR);
	spin_unlock_irqrestore(&csi_hw->slock[csi_ch], flags);

	return error_state.as32bits;
}

int odin_mipicsi_get_hw_init_state(css_mipi_csi_ch csi_ch)
{
	struct csi_hardware *csi_hw = get_csi_hw();

	if (csi_hw && csi_hw->hw_init) {
		if ((csi_hw->hw_init & (1<<csi_ch)) != 0)
			return 1;
		else
			return 0;
	} else {
		return 0;
	}
}

static
int odin_mipicsi_int_handler(css_mipi_csi_ch csi_ch, unsigned int int_status)
{
	if (int_status & INT_ESCAPE_MODE) {
		odin_mipicsi_interrupt_disable(csi_ch, INT_ESCAPE_MODE);
		odin_mipicsi_interrupt_clear(csi_ch, INT_ESCAPE_MODE);
	}

	if (int_status & INT_CSI_ERROR) {
		unsigned int err_state = 0;
		odin_mipicsi_interrupt_disable(csi_ch, INT_CSI_ERROR);

		err_state = odin_mipicsi_get_error_status(csi_ch);
		odin_mipicsi_interrupt_clear(csi_ch, INT_CSI_ERROR);
		if (err_state & 0x10440003) {
			if (css_log & MIPICSI0 || css_log & MIPICSI1 || css_log & MIPICSI2){
				css_mipicsi("E(%d):0x%x:0x%x\n", csi_ch,
							err_state, odin_mipicsi_get_word_count(csi_ch));
			} else {
				mipicsi_err_log(csi_ch, err_state);
			}
		}
	}

	if (int_status & INT_EMBEDDED_DATA) {
		odin_mipicsi_interrupt_disable(csi_ch, INT_EMBEDDED_DATA);
		odin_mipicsi_interrupt_clear(csi_ch, INT_EMBEDDED_DATA);
	}

	if (int_status & INT_FRAME_START) {
		odin_mipicsi_interrupt_disable(csi_ch, INT_FRAME_START);
		odin_mipicsi_interrupt_clear(csi_ch, INT_FRAME_START);
	}

	if (int_status & INT_FRAME_END) {
		odin_mipicsi_interrupt_disable(csi_ch, INT_FRAME_END);
		odin_mipicsi_interrupt_clear(csi_ch, INT_FRAME_END);
	}

	if (int_status & INT_ESCAPE_MODE) {
		odin_mipicsi_interrupt_enable(csi_ch, INT_ESCAPE_MODE);
	}

	if (int_status & INT_CSI_ERROR) {
		odin_mipicsi_interrupt_enable(csi_ch, INT_CSI_ERROR);
	}

	if (int_status & INT_EMBEDDED_DATA) {
		odin_mipicsi_interrupt_enable(csi_ch, INT_EMBEDDED_DATA);
	}

	if (int_status & INT_FRAME_START) {
		odin_mipicsi_interrupt_enable(csi_ch, INT_FRAME_START);
	}

	if (int_status & INT_FRAME_END) {
		odin_mipicsi_interrupt_enable(csi_ch, INT_FRAME_END);
	}

	return 0;
}

static irqreturn_t odin_mipicsi_ch0_irq(int irq, void *dev_id)
{
	unsigned int int_status;

	int_status = odin_mipicsi_get_interrupt_status(CSI_CH0);
	odin_mipicsi_int_handler(CSI_CH0, int_status);

	return IRQ_HANDLED;
}

static irqreturn_t odin_mipicsi_ch1_irq(int irq, void *dev_id)
{
	unsigned int int_status;

	int_status = odin_mipicsi_get_interrupt_status(CSI_CH1);
	odin_mipicsi_int_handler(CSI_CH1, int_status);

	return IRQ_HANDLED;
}

static irqreturn_t odin_mipicsi_ch2_irq(int irq, void *dev_id)
{
	unsigned int int_status;

	int_status = odin_mipicsi_get_interrupt_status(CSI_CH2);
	odin_mipicsi_int_handler(CSI_CH2, int_status);

	return IRQ_HANDLED;
}

int odin_mipicsi_get_word_count(css_mipi_csi_ch csi_ch)
{
	struct csi_hardware *csi_hw = get_csi_hw();

	CSS_MIPICSI_PL_STATUS03 status03;
	int wordcount = 0;

	CHECK_MIPI_AVALIABLE();

	if (csi_ch > CSI_CH2)
		return CSS_ERROR_MIPI_CSI_INVALID_LANE;

	status03.as32bits = readl(csi_hw->iobase[csi_ch] + PL_STATUS_03);
	wordcount = (status03.as32bits & 0xFFFF0000L) >> 16;

	return wordcount;
}

int odin_mipicsi_get_pixel_count(css_mipi_csi_ch csi_ch)
{
	struct csi_hardware *csi_hw = get_csi_hw();

	CSS_MIPICSI_PL_STATUS03 status03;
	int pixcount = 0;

	CHECK_MIPI_AVALIABLE();

	if (csi_ch > CSI_CH2)
		return CSS_ERROR_MIPI_CSI_INVALID_LANE;

	status03.as32bits = readl(csi_hw->iobase[csi_ch] + PL_STATUS_03);
	pixcount = (status03.as32bits & 0x00001FFF);

	return pixcount;
}

int odin_mipicsi_ctrl_continuous_mode(css_mipi_csi_ch csi_ch,
			unsigned int enable)
{
	return odin_mipicsi_set_continuous_mode(csi_ch, enable);
}

static void odin_mipicsi_channel_reset(css_mipi_csi_ch csi_ch)
{
	switch (csi_ch) {
	case CSI_CH0:
		css_block_reset(BLK_RST_MIPI0);
		odin_mipicsi_reg_update(CSI_CH0);
		odin_mipicsi_status_clear(CSI_CH0);
		break;
	case CSI_CH1:
		css_block_reset(BLK_RST_MIPI1);
		odin_mipicsi_reg_update(CSI_CH1);
		odin_mipicsi_status_clear(CSI_CH1);
		break;
	case CSI_CH2:
		css_block_reset(BLK_RST_MIPI2);
		odin_mipicsi_reg_update(CSI_CH2);
		odin_mipicsi_status_clear(CSI_CH2);
		break;
	}
}

void odin_mipicsi_channel_check(void)
{
	if (css_clock_available(CSS_CLK_MIPI0)) {
		if (odin_mipicsi_get_error_status(0) != 0)
			odin_mipicsi_channel_reset(0);
	}

	if (css_clock_available(CSS_CLK_MIPI2)) {
		if (odin_mipicsi_get_error_status(2) != 0)
			odin_mipicsi_channel_reset(2);
	}
}

#if 0
int odin_mipicsi_wait_stable(css_mipi_csi_ch csi_ch, int in_width,
			int in_height, int trycnt)
{
	int retry = trycnt;
	int linecnt = 0;
	int pixcnt = 0;
	int sensorsize = 0;
	int i, j, be_ok, match;

	ktime_t delta, stime, etime;
	unsigned long long duration;
	u32 (*get_sensor_size)(void);
	CSS_MIPICSI_CSI2_CTRL csi2_ctrl;

	if (csi_ch >= CSI_CH_MAX)
		return CSS_FAILURE;

	stime = ktime_get();

	odin_mipicsi_set_check_error(csi_ch, 0);

	odin_mipicsi_get_csi_control(csi_ch, &csi2_ctrl);
	if (csi2_ctrl.asbits.vsync_out_pol_enable)
		odin_mipicsi_set_csi_control(csi_ch, 1, 0);

	if (csi_ch == CSI_CH0)
		get_sensor_size = odin_isp_wrap_get_primary_sensor_size;
	else
		get_sensor_size = odin_isp_wrap_get_secondary_sensor_size;

	sensorsize = get_sensor_size();

	pixcnt = (sensorsize & 0xffff0000) >> 16;
	if (pixcnt != 0 && pixcnt >= in_width/2) {
		etime = ktime_get();
		delta = ktime_sub(etime, stime);
		duration = (unsigned long long) ktime_to_ns(delta) >> 10;
		css_info("mipi%02d ok[0x%08x] %lld\n", csi_ch, get_sensor_size(),
					duration);
		goto mipi_ok;
	} else {
		css_info("pix cnt = %d\n", pixcnt);
	}

	odin_mipicsi_channel_reset(csi_ch);

wait_start:
	i = 0;
	j = 0;
	match = 0;

	for (i = 0; i < 10; i++) {
		sensorsize = get_sensor_size();
		pixcnt = (sensorsize & 0xffff0000) >> 16;
		if (pixcnt != 0 && pixcnt >= in_width/2) {
			match = 1;
			break;
		}
		odin_mipicsi_channel_reset(csi_ch);
		usleep_range(1000, 1000);
	}
	if (match) {
		match = 0;
		for (j = 0; j < 400; j++) {
			linecnt = get_sensor_size() & 0xffff;
			if (linecnt != 0 && linecnt >= in_height/2) {
				match = 1;
				break;
			}
			usleep_range(100, 100);
		}
	}
	if (match) {
		etime = ktime_get();
		delta = ktime_sub(etime, stime);
		duration = (unsigned long long) ktime_to_ns(delta) >> 10;

		css_info("mipi%02d stable ok[0x%08x] %lld\n", csi_ch,
					get_sensor_size(), duration);
		goto mipi_ok;
	} else {
		retry--;
		goto mipi_reset;
	}

mipi_reset:
	if (retry < 0) {
		etime = ktime_get();
		delta = ktime_sub(etime, stime);
		duration = (unsigned long long) ktime_to_ns(delta) >> 10;

		css_info("mipi%02d stable wait done 0x%08x %lld\n", csi_ch,
					get_sensor_size(), duration);

		if (csi2_ctrl.asbits.vsync_out_pol_enable)
			odin_mipicsi_set_csi_control(csi_ch, 1, 1);

		odin_mipicsi_set_check_error(csi_ch, 1);
		css_info("crg clks:0x%08x\n", css_crg_clk_info());

		return CSS_FAILURE;
	}

	odin_mipicsi_channel_reset(csi_ch);
	goto wait_start;

mipi_ok:
	if (csi2_ctrl.asbits.vsync_out_pol_enable)
		odin_mipicsi_set_csi_control(csi_ch, 1, 1);

	odin_mipicsi_set_check_error(csi_ch, 1);
	return CSS_SUCCESS;
}
#else
int odin_mipicsi_wait_stable(css_mipi_csi_ch csi_ch, int in_width,
			int in_height, int trycnt)
{
	int retry = trycnt;
	int wordsize = -1;
	int pixsize = -1;
	int i, j, be_ok, match;
	ktime_t delta, stime, etime;
	unsigned long long duration;

	stime = ktime_get();

	odin_mipicsi_set_check_error(csi_ch, 0);
	odin_mipicsi_channel_reset(csi_ch);

wait_start:

	i = 0;
	j = 0;
	be_ok = 0;
	match = 0;

	for (i = 0; i < 10; i++) {
		wordsize = odin_mipicsi_get_word_count(csi_ch);
		if (wordsize != 0x0 && wordsize != 0xFFFF) {
			be_ok++;
			if (be_ok > 3) {
				match = 1;
				break;
			}
		}
		odin_mipicsi_channel_reset(csi_ch);
		usleep_range(1000, 1000);
	}

	if (match) {
		match = 0;
		for (j = 0; j < 40; j++) {
			pixsize = odin_mipicsi_get_pixel_count(csi_ch);
			if (pixsize == in_width) {
				match = 1;
				break;
			}
			odin_mipicsi_channel_reset(csi_ch);
			usleep_range(1000, 1000);
		}

		if (match) {
			etime = ktime_get();
			delta = ktime_sub(etime, stime);
			duration = (unsigned long long) ktime_to_ns(delta) >> 10;

			css_info("mipi%02d stable ok[%d %d:%d] %lld\n",
					csi_ch, retry, i, j, duration);
			goto mipi_ok;
		} else {
			retry--;
			goto mipi_reset;
		}

	} else {
		retry--;
		goto mipi_reset;
	}

mipi_reset:
	if (retry < 0) {
		etime = ktime_get();
		delta = ktime_sub(etime, stime);
		duration = (unsigned long long) ktime_to_ns(delta) >> 10;

		css_info("mipi%02d stable wait done %lld\n", csi_ch, duration);

		odin_mipicsi_set_check_error(csi_ch, 1);
		css_info("crg clks:0x%08x\n", css_crg_clk_info());
		return CSS_FAILURE;
	}

	odin_mipicsi_channel_reset(csi_ch);
	goto wait_start;

mipi_ok:
	odin_mipicsi_set_check_error(csi_ch, 1);
	return CSS_SUCCESS;
}
#endif

static int odin_mipicsi_init_resources(struct platform_device *pdev)
{
	int ret = CSS_SUCCESS;
	struct csi_hardware *csi_hw = NULL;
	struct resource *iores = NULL;
	struct css_device *cssdev = NULL;
	unsigned int size_iores;
	int cnt;

	cssdev = get_css_plat();
	if (!cssdev) {
		css_err("can't get cssdev!\n");
		return CSS_FAILURE;
	}

	csi_hw = (struct csi_hardware *)&cssdev->csi_hw;
	if (!csi_hw) {
		css_err("can't get csi_hw!\n");
		return CSS_FAILURE;
	}

	csi_hw->pdev = pdev;

	platform_set_drvdata(pdev, csi_hw);

	for (cnt = 0; cnt < CSI_CH_MAX; cnt++) {
		/* get register physical base address to resource of platform device */
		iores = platform_get_resource(pdev, IORESOURCE_MEM, cnt);
		if (!iores) {
			css_err("failed to get csi-ch%d resource!\n", cnt);
			ret = CSS_ERROR_GET_RESOURCE;
			goto csi_error;
		}
		size_iores = iores->end - iores->start + 1;

		csi_hw->iores[cnt] = request_mem_region(iores->start,
					size_iores, pdev->name);
		if (!csi_hw->iores[cnt]) {
			css_err("failed to request csi-ch%d mem region!\n", cnt);
			ret = CSS_ERROR_GET_RESOURCE;
			goto csi_error;
		}

		csi_hw->iobase[cnt] = ioremap(iores->start, size_iores);
		if (!csi_hw->iobase[cnt]) {
			css_err("failed to ioremap!\n");
			ret = CSS_ERROR_IOREMAP;
			goto csi_error;
		}

#ifdef	MIPICSI_INT_ENABLE
		csi_hw->irq[cnt] = platform_get_irq(pdev, cnt);
		switch (cnt) {
		case 0:
			if (csi_hw->irq[cnt] > 0) {
				ret = request_irq(csi_hw->irq[cnt], odin_mipicsi_ch0_irq, 0,
								"mipicsi2-0", csi_hw);
				if (ret) {
					css_err("failed to request mipi ch0 irq %d\n", ret);
					goto csi_error;
				}
			}
			break;
		case 1:
			if (csi_hw->irq[cnt] > 0) {
				ret = request_irq(csi_hw->irq[cnt], odin_mipicsi_ch1_irq, 0,
								"mipicsi2-1", csi_hw);
				if (ret) {
					free_irq(csi_hw->irq[0], csi_hw);
					css_err("failed to request mipi ch1 irq %d\n", ret);
					goto csi_error;
				}
			}
			break;
		case 2:
			if (csi_hw->irq[cnt] > 0) {
				ret = request_irq(csi_hw->irq[cnt], odin_mipicsi_ch2_irq, 0,
								"mipicsi2-2", csi_hw);
				if (ret) {
					free_irq(csi_hw->irq[0], csi_hw);
					free_irq(csi_hw->irq[1], csi_hw);
					css_err("failed to request mipi ch2 irq %d\n", ret);
					goto csi_error;
				}
			}
			break;
		}
#endif
	}

	ret = device_create_file(&(pdev->dev), &dev_attr_mipicsi);
	if (ret < 0)
		css_warn("failed to add sysfs entries for mipi\n");

	set_csi_hw(csi_hw);

#ifdef ODIN_MIPICSI_ERR_REPORT
	INIT_WORK(&mipicsi_report[CSI_CH0], mipicsi0_err_log_wq);
	INIT_WORK(&mipicsi_report[CSI_CH1], mipicsi1_err_log_wq);
	INIT_WORK(&mipicsi_report[CSI_CH2], mipicsi2_err_log_wq);
#endif

	return CSS_SUCCESS;

csi_error:
	for (cnt = 0; cnt < CSI_CH_MAX; cnt++) {
#ifdef	MIPICSI_INT_ENABLE
		if (csi_hw->irq[cnt] > 0)
			free_irq(csi_hw->irq[cnt], csi_hw);
#endif
		if (csi_hw->iobase[cnt]) {
			iounmap(csi_hw->iobase[cnt]);
			csi_hw->iobase[cnt] = NULL;
		}
		if (csi_hw->iores[cnt]) {
			release_mem_region(csi_hw->iores[cnt]->start,
								csi_hw->iores[cnt]->end - \
								csi_hw->iores[cnt]->start + 1);
			csi_hw->iores[cnt] = NULL;
		}
	}

	return ret;
}

static int odin_mipicsi_deinit_resources(struct platform_device *pdev)
{
	int ret = CSS_FAILURE;
	struct csi_hardware *csi_hw = NULL;
	int cnt;

	csi_hw = platform_get_drvdata(pdev);

	if (csi_hw) {
		for (cnt = 0; cnt < CSI_CH_MAX; cnt++) {
#ifdef	MIPICSI_INT_ENABLE
			if (csi_hw->irq[cnt] > 0) {
				free_irq(csi_hw->irq[cnt], csi_hw);
			}
#endif

			if (csi_hw->iobase[cnt]) {
				iounmap(csi_hw->iobase[cnt]);
				csi_hw->iobase[cnt] = NULL;
			}
			if (csi_hw->iores[cnt])
				release_mem_region(csi_hw->iores[cnt]->start,
									csi_hw->iores[cnt]->end - \
									csi_hw->iores[cnt]->start + 1);
				csi_hw->iores[cnt] = NULL;
		}

		device_remove_file(&(pdev->dev), &dev_attr_mipicsi);
		ret = CSS_SUCCESS;
	}

	return ret;
}

static int odin_mipicsi_hw_init(css_mipi_csi_ch csi_ch)
{
	int ret = CSS_SUCCESS;
	struct csi_hardware *csi_hw = get_csi_hw();

	if ((csi_hw->hw_init & (1 << csi_ch)) == 0) {
		spin_lock_init(&csi_hw->slock[csi_ch]);
		csi_hw->hw_init |= (1 << csi_ch);
	}

	switch (csi_ch) {
	case CSI_CH0:
		css_clock_enable(CSS_CLK_MIPI0);
		css_block_reset(BLK_RST_MIPI0);
		break;
	case CSI_CH1:
		css_clock_enable(CSS_CLK_MIPI1);
		css_block_reset(BLK_RST_MIPI1);
		break;
	case CSI_CH2:
		css_clock_enable(CSS_CLK_MIPI2);
		css_block_reset(BLK_RST_MIPI2);
		break;
	default:
		ret = CSS_FAILURE;
		break;
	}

	if (ret == CSS_SUCCESS) {
		odin_mipicsi_status_clear(csi_ch);
	}

	return ret;
}

static int odin_mipicsi_hw_deinit(css_mipi_csi_ch csi_ch)
{
	int ret = CSS_SUCCESS;
	struct csi_hardware *csi_hw = get_csi_hw();

	csi_hw->hw_init &= ~(1 << csi_ch);

	switch (csi_ch) {
	case CSI_CH0:
		css_block_reset_off(BLK_RST_MIPI0);
		css_clock_disable(CSS_CLK_MIPI0);
		break;
	case CSI_CH1:
		css_block_reset_off(BLK_RST_MIPI1);
		css_clock_disable(CSS_CLK_MIPI1);
		break;
	case CSI_CH2:
		css_block_reset_off(BLK_RST_MIPI2);
		css_clock_disable(CSS_CLK_MIPI2);
		break;
	default:
		ret = CSS_FAILURE;
		break;
	}

	return ret;
}

static int odin_mipicsi_set_vsync_delay(css_mipi_csi_ch csi_ch, int pclkcnt)
{
	int ret = CSS_SUCCESS;
	struct csi_hardware *csi_hw = get_csi_hw();
	CSS_MIPICSI_VBLANK_SIZE vblank_size;
	unsigned long flags;

	vblank_size.as32bits = pclkcnt;

	spin_lock_irqsave(&csi_hw->slock[csi_ch], flags);
	writel(vblank_size.as32bits, csi_hw->iobase[csi_ch] + VBLANK_SIZE);
	spin_unlock_irqrestore(&csi_hw->slock[csi_ch], flags);

	return ret;
}

static int odin_mipicsi_set_lane_enable(css_mipi_csi_ch csi_ch, int lanecnt)
{
	int ret = CSS_SUCCESS;
	int cnt = 0;
	struct csi_hardware *csi_hw = get_csi_hw();
	CSS_MIPICSI_DPHY_CTRL dphy_ctrl;
	unsigned long flags;

	if (lanecnt > 4)
		lanecnt = 4;

	if (csi_ch == CSI_CH2 && lanecnt > 2)
		lanecnt = 2;

	cnt = lanecnt;

	dphy_ctrl.as32bits = readl(csi_hw->iobase[csi_ch] + DPHY_CTRL);

	dphy_ctrl.asbits.slave_lane0_enable = 0;
	dphy_ctrl.asbits.slave_lane1_enable = 0;
	if (csi_ch != CSI_CH2) {
		dphy_ctrl.asbits.slave_lane2_enable = 0;
		dphy_ctrl.asbits.slave_lane3_enable = 0;
	}
	dphy_ctrl.asbits.slave_clock_enable = 0;

	for (; cnt--;) {
		dphy_ctrl.as32bits |= 1 << cnt;
		if (cnt == 0) {
			dphy_ctrl.asbits.slave_clock_enable = 1;
		}
	}

	spin_lock_irqsave(&csi_hw->slock[csi_ch], flags);
	writel(dphy_ctrl.as32bits, csi_hw->iobase[csi_ch] + DPHY_CTRL);
	spin_unlock_irqrestore(&csi_hw->slock[csi_ch], flags);

	return ret;
}

static int odin_mipicsi_set_lane_disable(css_mipi_csi_ch csi_ch)
{
	int ret = CSS_SUCCESS;
	int cnt = 0;
	struct csi_hardware *csi_hw = get_csi_hw();
	CSS_MIPICSI_DPHY_CTRL dphy_ctrl;

	if (csi_ch == CSI_CH2)
		cnt = 2;
	else
		cnt = 4;

	dphy_ctrl.as32bits = readl(csi_hw->iobase[csi_ch] + DPHY_CTRL);

	for (; cnt--;) {
		dphy_ctrl.as32bits &= ~(1<<cnt);
		if (cnt == 0) {
			dphy_ctrl.asbits.slave_clock_enable = 0;
		}
	}

	writel(dphy_ctrl.as32bits, csi_hw->iobase[csi_ch] + DPHY_CTRL);

	return ret;
}

static int odin_mipicsi_set_lane_pdown_on(css_mipi_csi_ch csi_ch)
{
	int ret = CSS_SUCCESS;
	struct csi_hardware *csi_hw = get_csi_hw();
	CSS_MIPICSI_DPHY_CTRL dphy_ctrl;

	dphy_ctrl.as32bits = readl(csi_hw->iobase[csi_ch] + DPHY_CTRL);

	dphy_ctrl.asbits.d_phy_all_pdown = 1;

	writel(dphy_ctrl.as32bits, csi_hw->iobase[csi_ch] + DPHY_CTRL);

	return ret;
}

static int odin_mipicsi_set_lane_pdown_off(css_mipi_csi_ch csi_ch)
{
	int ret = CSS_SUCCESS;
	struct csi_hardware *csi_hw = get_csi_hw();
	CSS_MIPICSI_DPHY_CTRL dphy_ctrl;

	dphy_ctrl.as32bits = readl(csi_hw->iobase[csi_ch] + DPHY_CTRL);

	dphy_ctrl.asbits.d_phy_all_pdown = 0;

	writel(dphy_ctrl.as32bits, csi_hw->iobase[csi_ch] + DPHY_CTRL);

	return ret;
}

static int odin_mipicsi_set_tif_clear(css_mipi_csi_ch csi_ch)
{
	int ret = CSS_SUCCESS;
	struct csi_hardware *csi_hw = get_csi_hw();
	CSS_MIPICSI_TIF_CLR tif_clear;

	tif_clear.as32bits = 0;
	tif_clear.asbits.tif_clr = 1;

	writel(tif_clear.as32bits, csi_hw->iobase[csi_ch] + DPHY_CTRL_CLEAR);

	return ret;
}


static int odin_mipicsi_set_rx_bandwidth(css_mipi_csi_ch csi_ch, int level)
{
	int ret = CSS_SUCCESS;
	struct csi_hardware *csi_hw = get_csi_hw();

	writel(level, csi_hw->iobase[csi_ch] + DPHY_CTRL_CU_CONT);

	return ret;
}


static int odin_mipicsi_set_hsrx_clk_inversion(css_mipi_csi_ch csi_ch,
				unsigned int clks, int inversion)
{
	int ret = CSS_SUCCESS;
	unsigned int hsrx_ck;
	struct csi_hardware *csi_hw = get_csi_hw();

	hsrx_ck = readl(csi_hw->iobase[csi_ch] + DPHY_CTRL_HSRX_CK);

	if (inversion) {
		if (clks & HSRX_BYTE_SP_CK)
			hsrx_ck |= HSRX_BYTE_SP_CK;
		else if (clks & HSRX_DDR_SP_CK)
			hsrx_ck |= HSRX_DDR_SP_CK;
		else if (clks & HSRX_IN_SP_CK)
			hsrx_ck |= HSRX_IN_SP_CK;
	} else {
		if (clks & HSRX_BYTE_SP_CK)
			hsrx_ck &= ~HSRX_BYTE_SP_CK;
		else if (clks & HSRX_DDR_SP_CK)
			hsrx_ck &= ~HSRX_DDR_SP_CK;
		else if (clks & HSRX_IN_SP_CK)
			hsrx_ck &= ~HSRX_IN_SP_CK;
	}

	writel(hsrx_ck, csi_hw->iobase[csi_ch] + DPHY_CTRL_HSRX_CK);

	return ret;
}

int odin_mipicsi_set_speed(css_mipi_csi_ch csi_ch, unsigned int speed)
{
	int ret = CSS_SUCCESS;
	struct csi_hardware *csi_hw = get_csi_hw();

	CSS_MIPICSI_DPHY_TIF_CTRL11 dphy_ctrl11;
	CSS_MIPICSI_DPHY_TIF_CTRL13 dphy_ctrl13;
	CSS_MIPICSI_DPHY_TIF_CTRL16 dphy_ctrl16;

	dphy_ctrl11.as32bits = 0xC0;
	dphy_ctrl16.as32bits = 0x01;

	dphy_ctrl13.as32bits = readl(csi_hw->iobase[csi_ch] + DPHY_CTRL13);
	dphy_ctrl13.as32bits &= 0xffffffe0;

	if (speed <= 100)
		dphy_ctrl13.asbits.speed = 3;
	else if (speed <= 125)
		dphy_ctrl13.asbits.speed = 4;
	else if (speed <= 200)
		dphy_ctrl13.asbits.speed = 5;
	else if (speed <= 250)
		dphy_ctrl13.asbits.speed = 7;
	else if (speed <= 400)
		dphy_ctrl13.asbits.speed = 12;
	else if (speed <= 500)
		dphy_ctrl13.asbits.speed = 16;
	else if (speed <= 750)
		dphy_ctrl13.asbits.speed = 25;
	else
		dphy_ctrl13.asbits.speed = 25;

	writel(dphy_ctrl11.as32bits, csi_hw->iobase[csi_ch] + DPHY_CTRL11);
	writel(dphy_ctrl13.as32bits, csi_hw->iobase[csi_ch] + DPHY_CTRL13);
	writel(dphy_ctrl16.as32bits, csi_hw->iobase[csi_ch] + DPHY_CTRL16);

	writel(0x4, csi_hw->iobase[csi_ch] + DPHY_CTRL20);

#ifdef CONFIG_MACH_ODIN_FPGA
	writel(0x01, csi_hw->iobase[csi_ch] + DPHY_CTRL_34H);
	writel(0x01, csi_hw->iobase[csi_ch] + DPHY_CTRL_3CH);
	writel(0xc0, csi_hw->iobase[csi_ch] + DPHY_CTRL_HSRX_PDB);
	writel(0x01, csi_hw->iobase[csi_ch] + DEBUG_DLL_ON);
#endif

	return ret;
}

static int odin_mipicsi_set_continuous_mode(css_mipi_csi_ch csi_ch,
				unsigned int hsrxctrl)
{
	int ret = CSS_SUCCESS;
	int mode = 0;
	struct csi_hardware *csi_hw = get_csi_hw();

	if (hsrxctrl == 0) {
		css_info("continuous mode disabled\n");
		mode = GATE_CLOCK_MODE;
	} else {
		css_info("continuous mode enabled\n");
		mode = CONTINUOUS_MODE;
	}

	writel(mode, csi_hw->iobase[csi_ch] + DPHY_CTRL_HSRX_PDB);

	return CSS_SUCCESS;
}

static int odin_mipicsi_set_emb_data_type(css_mipi_csi_ch csi_ch,
				int type0, int type1, int emb_linecnt)
{
	int ret = CSS_SUCCESS;
	struct csi_hardware *csi_hw = get_csi_hw();
	CSS_MIPICSI_EMB_DTYPE_CTRL emb_data_type_ctrl;

	emb_data_type_ctrl.as32bits =
			readl(csi_hw->iobase[csi_ch] + EMB_DATA_TYPE_CTRL);

	emb_data_type_ctrl.asbits.emb_data_type0 = type0;
	emb_data_type_ctrl.asbits.emb_data_type1 = type1;
	emb_data_type_ctrl.asbits.emb_line_cnt = emb_linecnt;

	writel(emb_data_type_ctrl.as32bits,
			csi_hw->iobase[csi_ch] + EMB_DATA_TYPE_CTRL);

	return ret;
}

static int odin_mipicsi_set_csi_control(css_mipi_csi_ch csi_ch, int rx_enable,
				int vsync_pol)
{
	int ret = CSS_SUCCESS;
	struct csi_hardware *csi_hw = get_csi_hw();
	CSS_MIPICSI_CSI2_CTRL csi2_ctrl;

	csi2_ctrl.as32bits = readl(csi_hw->iobase[csi_ch] + CSI2_CTRL);

	if (vsync_pol)
		csi2_ctrl.asbits.vsync_out_pol_enable = 1;
	else
		csi2_ctrl.asbits.vsync_out_pol_enable = 0;

	if (rx_enable)
		csi2_ctrl.asbits.mipi_rx_enable = 1;
	else
		csi2_ctrl.asbits.mipi_rx_enable = 0;

	csi2_ctrl.asbits.state_clear = 1;
	csi2_ctrl.asbits.lane0_direction = 1;
	csi2_ctrl.asbits.lane1_direction = 1;

	if (csi_ch != CSI_CH2) {
		csi2_ctrl.asbits.lane2_direction = 1;
		csi2_ctrl.asbits.lane3_direction = 1;
	}

	writel(csi2_ctrl.as32bits, csi_hw->iobase[csi_ch] + CSI2_CTRL);

	return ret;
}


static int odin_mipicsi_get_csi_control(css_mipi_csi_ch csi_ch,
			CSS_MIPICSI_CSI2_CTRL *csi2_ctrl)
{
	struct csi_hardware *csi_hw = get_csi_hw();

	if (csi2_ctrl == NULL)
		return CSS_FAILURE;

	csi2_ctrl->as32bits = readl(csi_hw->iobase[csi_ch] + CSI2_CTRL);

	return CSS_SUCCESS;
}

static int odin_mipicsi_set_check_error(css_mipi_csi_ch csi_ch, int check_err)
{
	int ret = CSS_SUCCESS;
	struct csi_hardware *csi_hw = get_csi_hw();
	CSS_MIPICSI_CSI2_CTRL csi2_ctrl;

	csi2_ctrl.as32bits = readl(csi_hw->iobase[csi_ch] + CSI2_CTRL);

	if (check_err) {
		csi2_ctrl.asbits.ecc_check_enable = 1;
		csi2_ctrl.asbits.crc_check_enable = 1;
		csi2_ctrl.asbits.data_type_err_enable = 1;
		csi2_ctrl.asbits.escape_mode_err_enable = 0;
		csi2_ctrl.asbits.line_data_size_err_enable = 1;
	} else {
		csi2_ctrl.asbits.ecc_check_enable = 0;
		csi2_ctrl.asbits.crc_check_enable = 0;
		csi2_ctrl.asbits.data_type_err_enable = 0;
		csi2_ctrl.asbits.escape_mode_err_enable = 0;
		csi2_ctrl.asbits.line_data_size_err_enable = 0;
	}

	if (csi2_ctrl.asbits.line_data_size_err_enable)
		csi2_ctrl.asbits.line_byte_cnt_enable = 1;
	else
		csi2_ctrl.asbits.line_byte_cnt_enable = 0;

	writel(csi2_ctrl.as32bits, csi_hw->iobase[csi_ch] + CSI2_CTRL);

	return ret;
}

int odin_mipicsi_probe(struct platform_device *pdev)
{
	int ret = CSS_SUCCESS;

	ret = odin_mipicsi_init_resources(pdev);

	if (ret == CSS_SUCCESS) {
		css_info("mipicsi2 probe ok!!\n");
	} else {
		css_err("mipicsi2 probe err!!\n");
	}

	return ret;
}

int odin_mipicsi_remove(struct platform_device *pdev)
{
	int ret = CSS_SUCCESS;

	ret = odin_mipicsi_deinit_resources(pdev);

	if (ret == CSS_SUCCESS) {
		css_info("mipicsi2 remove ok!!\n");
	} else {
		css_err("mipicsi2 remove err!!\n");
	}

	return ret;
}

int odin_mipicsi_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	return ret;
}

int odin_mipicsi_resume(struct platform_device *pdev)
{
	int ret = 0;
	return ret;
}

#ifdef CONFIG_OF
static struct of_device_id odin_mipicsi_match[] = {
	{ .compatible = CSS_OF_CSI_NAME},
	{},
};
#endif /* CONFIG_OF */

struct platform_driver odin_mipicsi_driver =
{
	.probe		= odin_mipicsi_probe,
	.remove 	= odin_mipicsi_remove,
	.suspend	= odin_mipicsi_suspend,
	.resume 	= odin_mipicsi_resume,
	.driver 	= {
		.name	= CSS_PLATDRV_CSI,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odin_mipicsi_match),
#endif
	},
};

static int __init odin_mipicsi_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&odin_mipicsi_driver);

	return ret;
}

static void __exit odin_mipicsi_exit(void)
{
	platform_driver_unregister(&odin_mipicsi_driver);
	return;
}

MODULE_DESCRIPTION("odin mipicsi2 driver");
MODULE_AUTHOR("MTEKVISION<http://www.mtekvision.com>");

late_initcall(odin_mipicsi_init);
module_exit(odin_mipicsi_exit);

MODULE_LICENSE("GPL");

