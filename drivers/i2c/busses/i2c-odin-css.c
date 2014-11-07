/*
 *  drivers/i2c/busses/i2c-odin-css.c
 *
 *  Copyright (C) 2010 MtekVision Co., Ltd.
 *	Jinyoung Park <kimska@mtekvision.com>
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/slab.h>

#include "../../media/platform/odin-css/odin-css-system.h"
#include "i2c-odin-css-gpio.h"

#define DRV_NAME	"css-i2c"

//#define POLL_MODE

/*
 * Debug Level
 * 	2 : Print all debug messages
 * 	1 : print only dbg() messages
 * 	0 : No debug messages
 */
//#define MVCSS_I2C_DEBUG_LEVEL	0
#define MVCSS_I2C_DEBUG_LEVEL	1
#if (MVCSS_I2C_DEBUG_LEVEL == 2)
#define dbg(format, arg...)	\
//	printk(KERN_ALERT DRV_NAME": Debug: %s(): " format, __func__, ## arg)
	printk(KERN_DEBUG DRV_NAME": Debug: %s(): " format, __func__, ## arg)
#define enter()			\
//	printk(KERN_ALERT DRV_NAME": Enter: %s()\n", __func__)
	printk(KERN_DEBUG DRV_NAME": Enter: %s()\n", __func__)
#define leave()			\
//	printk(KERN_ALERT DRV_NAME": Leave: %s()\n", __func__)
	printk(KERN_DEBUG DRV_NAME": Leave: %s()\n", __func__)
#define	DATA_DUMP
#elif (MVCSS_I2C_DEBUG_LEVEL == 1)
#define dbg(format, arg...)	\
//	printk(KERN_ALERT DRV_NAME": Debug: %s(): " format, __func__, ## arg)
//	printk(KERN_DEBUG DRV_NAME": Debug: %s(): " format, __func__, ## arg)
#define enter()
#define leave()
#else
#define dbg(format, arg...)
#define enter()
#define leave()
#endif

/*
 * Register offset
 */
#define CSS_I2C_TRANS_START_REG			0x00000000
#define CSS_I2C_TRANS_NUMBER_REG		0x00000004
#define CSS_I2C_STATUS_REG				0x00000008
#define CSS_I2C_CONTROL_REG				0x0000000C
#define CSS_I2C_DEVICE_ID_REG			0x00000010
#define CSS_I2C_INDEX_REG				0x00000014
#define CSS_I2C_TXBYTE_1_REG			0x00000018
#define CSS_I2C_TXBYTE_2_REG			0x0000001C
#define CSS_I2C_TXBYTE_3_REG			0x00000020
#define CSS_I2C_TXBYTE_4_REG			0x00000024
#define CSS_I2C_TXBYTE_5_REG			0x00000028
#define CSS_I2C_READ_DATA_1_REG			0x0000002C
#define CSS_I2C_SPEEDCTRL_REG			0x00000034
#define CSS_I2C_CLOCK_CFG_REG			0x00000038
#define CSS_I2C_INT_EN_REG				0x00000040
#define CSS_I2C_INT_STAT_1_REG			0x00000044
#define CSS_I2C_INT_CLEAR_REG			0x00000048
#define CSS_I2C_INT_STAT_2_REG			0x0000004C

/*
 * Field definitions
 */

/*
 *	0x00 : I2C start
 */
#define CSS_I2C_START_ON	0x1

/*
 *	0x08 : I2C status
 */
#define CSS_I2C_STAT_IDLE	0x00
#define CSS_I2C_STAT_ERROR	0xBB
#define CSS_I2C_STAT_BUSY	0xCC
#define CSS_I2C_STAT_OK		0xAA

/*
 *	0x0C : I2C CTRL Mode
 */
#define CSS_I2C_CTRL_FAST_MODE		0x8
#define CSS_I2C_CTRL_READ_RESTART	0x4
#define CSS_I2C_CTRL_DUMMY_WRITE	0x2
#define CSS_I2C_CTRL_SEQ_MODE		0x1
#define CSS_I2C_CTRL_NORMAL_MODE	0x0

/*
 *	0x38 : I2C Baud rate
 */
#define CSS_I2C_CLK_SHIFT			3
#define CSS_I2C_CLK_DIV_4		(0x00 << CSS_I2C_CLK_SHIFT)
#define CSS_I2C_CLK_DIV_2		(0x01 << CSS_I2C_CLK_SHIFT)
#define CSS_I2C_CLK_BYPASS		(0x10 << CSS_I2C_CLK_SHIFT)

/*
 *	0x40 : I2C Interrupt Enable
 */
#define CSS_I2C_INT_EN_ERROR			0x8
#define CSS_I2C_INT_EN_RX_DATA_AVAIL	0x4
#define CSS_I2C_INT_EN_TX_DATA_EMPTY	0x2
#define CSS_I2C_INT_EN_TRANS_DONE		0x1
#define CSS_I2C_INT_EN_ALL			0xF

/*
 *	0x44 : I2C Interrupt Status
 */
#define CSS_I2C_INT_STAT_ERROR			0x8
#define CSS_I2C_INT_STAT_RX_DATA_AVAIL	0x4
#define CSS_I2C_INT_STAT_TX_DATA_EMPTY	0x2
#define CSS_I2C_INT_STAT_TRANS_DONE		0x1

/*
 *	0x48 : I2C Interrupt Clear
 */
#define CSS_I2C_INT_CLEAR_ERROR			0x8
#define CSS_I2C_INT_CLEAR_RX_DATA_AVAIL	0x4
#define CSS_I2C_INT_CLEAR_TX_DATA_EMPTY	0x2
#define CSS_I2C_INT_CLEAR_TRANS_DONE	0x1
#define CSS_I2C_INT_CLEAR_ALL			0xF

/*
 *	0x4C : I2C Interrupt Raw status
 */
#define CSS_I2C_RAW_STAT_ERROR			0x3
#define CSS_I2C_RAW_STAT_RX_DATA_AVAIL	0x2
#define CSS_I2C_RAW_STAT_TX_EMPTY		0x1
#define CSS_I2C_RAW_STAT_TRANS_DONE		0x0



#define I2C_STATE_IDLE		0x0
#define I2C_STATE_BUSY		0xcc
#define I2C_STATE_DONE		0xaa
#define I2C_STATE_ERROR		0xbb


static DEFINE_MUTEX(odin_i2c_css_mutex);
static unsigned long msecs1000_to_jiffies;

#define ODIN_I2C_TIMEOUT (msecs_to_jiffies(30))
#define INIT_MODE	0x0
#define READ_MODE	0x1
#define WRITE_MODE	0x2
#define RESTART_MODE	0x4

struct mv_css_i2c_platform_data {
	int clk_hz;
	int clk_div;
	int seq_mod;
};

struct mv_css_i2c_drvdata {
	struct platform_device *pdev;
	struct i2c_adapter adap;

	/* H/W resource */
	struct resource *mem;
	void __iomem *base;
	int irq;

	int hw_init;

	struct completion completion;
	spinlock_t spinlock;

	u8	*buf;
	size_t	buf_len;

	int clk_hz;
	int clk_div;
	int seq_mod;
	int rw_mod;

	int state;
	int cond;
	u8 recv_ack;
};
static void mv_css_i2c_set_clk(struct mv_css_i2c_drvdata *drvdata)
{
#if 0
	struct i2c_adapter *adap = &drvdata->adap;
	u32 apb_clk_hz = 0;
	u32 clk_div = 0;
	u32 val;
	unsigned long flags;

	apb_clk_hz = mv_sys_get_apb_clk_hz();


	dbg("Requested clock hz=%dhz, clock div=%dhz\n", drvdata->clk_hz, drvdata->clk_div);
	for(i=0; i< ARRAY_SIZE(css_clk_tlb); i++)
	{
		if(css_clk_tlb[i].clk_hz == drvdata->clk_hz)
		{
			writel(css_clk_tlb[i].reg_val | drvdata->clk_div, drvdata->base + CSS_I2C_CLOCK_CFG_REG);
			break;
		}
	}
#endif
	writel(0x20, drvdata->base + CSS_I2C_CLOCK_CFG_REG);  /* 0x20 bypass and 1(clock/1000) ==> 400kbps */

}


static void mv_css_i2c_hw_init(struct mv_css_i2c_drvdata *drvdata)
{
	u32 val;

	enter();

	/*
	 *  i2c reset todo
	 */
	switch (drvdata->pdev->id) {
		case 10:
			printk("css i2c-0 hw init call\n");
			css_block_reset(BLK_RST_I2C0);
			break;
		case 11:
			printk("css i2c-1 hw init call\n");
			css_block_reset(BLK_RST_I2C1);
			break;
		case 12:
			printk("css i2c-2 hw init call\n");
			css_block_reset(BLK_RST_I2C2);
			break;
	}

	if(drvdata->seq_mod == 0)
		writel(CSS_I2C_CTRL_NORMAL_MODE, drvdata->base + CSS_I2C_CONTROL_REG);
	else
		writel(CSS_I2C_CTRL_SEQ_MODE, drvdata->base + CSS_I2C_CONTROL_REG);

	mv_css_i2c_set_clk(drvdata);

	udelay(10);

	writel(CSS_I2C_INT_EN_ALL, drvdata->base + CSS_I2C_INT_EN_REG);
	val = readl(drvdata->base + CSS_I2C_INT_EN_REG);
	writel(CSS_I2C_INT_CLEAR_ALL, drvdata->base + CSS_I2C_INT_CLEAR_REG);

	 /*  i2c baud rate setting / byte interlval setting */

	val = readl(drvdata->base + CSS_I2C_STATUS_REG);

	if(val == CSS_I2C_STAT_IDLE)
		drvdata->state = I2C_STATE_IDLE;

	drvdata->hw_init = 1;

	leave();
}

static void mv_css_i2c_hw_uninit(struct mv_css_i2c_drvdata *drvdata)
{
	/* struct i2c_adapter *adap = &drvdata->adap; */

	enter();

	drvdata->hw_init = 0;

//	dbg(DRV_NAME ": %s(): adap->nr=%d\n", __func__, adap->nr);

	leave();
}
static inline int mv_css_i2c_wait_for_busy(struct mv_css_i2c_drvdata *drvdata)
{
	struct i2c_adapter *adap = &drvdata->adap;
	unsigned long timeout;
	int ret = 0;

	timeout = jiffies + msecs1000_to_jiffies;
	drvdata->state = readl(drvdata->base + CSS_I2C_STATUS_REG);
	while (drvdata->state == I2C_STATE_BUSY) {
		schedule();
		if (time_after(jiffies, timeout)) {
			dev_err(&adap->dev,
					"Timeout waiting for I2C idle\n");
			ret = -ETIMEDOUT;
			break;
		}
		drvdata->state = readl(drvdata->base + CSS_I2C_STATUS_REG);
	}

	return ret;
}


#ifdef POLL_MODE
static inline int mv_css_i2c_wait_for_poll(struct mv_css_i2c_drvdata *drvdata)
{
	struct i2c_adapter *adap = &drvdata->adap;
	unsigned long timeout;
	u32 val;
	int ret = 0;
	dbg("polling wait\n");
	timeout = jiffies + msecs1000_to_jiffies;
	drvdata->state = readl(drvdata->base + CSS_I2C_STATUS_REG);
	while (drvdata->state != I2C_STATE_DONE) {
		if (time_after(jiffies, timeout)) {
			dev_err(&adap->dev,
				"Timeout waiting for I2C idle\n");
			ret = -ETIMEDOUT;
			break;
		}
		msleep(1);
		drvdata->state = readl(drvdata->base + CSS_I2C_STATUS_REG);
	}
	val = readl(drvdata->base + CSS_I2C_INT_STAT_1_REG);

	if((val & CSS_I2C_INT_STAT_ERROR) != 0)
	{
		writel(CSS_I2C_INT_CLEAR_ERROR, drvdata->base + CSS_I2C_INT_CLEAR_REG);
	}
	if((val & CSS_I2C_INT_STAT_TRANS_DONE) != 0)
	{
		writel(CSS_I2C_INT_CLEAR_TRANS_DONE, drvdata->base + CSS_I2C_INT_CLEAR_REG);

		 val = readl(drvdata->base + CSS_I2C_STATUS_REG);
//#if 0
//		 if(val == 0xAA)
//			 printk("success done\n");
//		 else
//			 printk("error done \n");
//#endif
	}

	val = readl(drvdata->base + CSS_I2C_INT_STAT_1_REG);
	switch(val) {
		case CSS_I2C_INT_STAT_ERROR:
			writel(CSS_I2C_INT_CLEAR_ERROR, drvdata->base + CSS_I2C_INT_CLEAR_REG);
			break;

		case CSS_I2C_INT_STAT_RX_DATA_AVAIL:
				 *drvdata->buf++ = readl(drvdata->base + CSS_I2C_READ_DATA_1_REG);
				writel(CSS_I2C_INT_CLEAR_RX_DATA_AVAIL, drvdata->base + CSS_I2C_INT_CLEAR_REG);


			break;
		case CSS_I2C_INT_STAT_TX_DATA_EMPTY:
			writel(CSS_I2C_INT_CLEAR_TX_DATA_EMPTY, drvdata->base + CSS_I2C_INT_CLEAR_REG);
			break;

	}

	dbg("done\n");
	return ret;
}
#endif

static inline int mv_css_i2c_wait_for_done(struct mv_css_i2c_drvdata *drvdata)
{
	int ret = 0;
	unsigned long timeout;
	int val;

#ifndef  POLL_MODE
	timeout = wait_for_completion_timeout(&drvdata->completion,
			ODIN_I2C_TIMEOUT);
#if 0
	if(timeout == 0) {
		pr_err( "controller timed out\n");
			/*	 odin_i2c_reset(drvdata);  todo */
		mv_css_i2c_hw_init(drvdata);
		ret =  -ETIMEDOUT;
	}
#endif
	val = readl(drvdata->base + CSS_I2C_STATUS_REG);
	switch(val)
	{
		case I2C_STATE_DONE:
			drvdata->state = I2C_STATE_DONE;
			break;

		case I2C_STATE_IDLE:
			drvdata->state = I2C_STATE_IDLE;
			break;

		case I2C_STATE_BUSY:
			drvdata->state = I2C_STATE_BUSY;
			printk("i2c busy state = %x\n", val);
			ret = -EBUSY;
			break;

		case I2C_STATE_ERROR:
			drvdata->state = I2C_STATE_ERROR;
			printk("i2c error = %x\n", val);
			ret = -ENODATA;
			break;
		default:
			pr_err("oops : i2c no status : %x\n", val);
			ret = -ENOMSG;
			break;

	}
	if(timeout == 0) {
		writel(CSS_I2C_INT_CLEAR_ALL, drvdata->base + CSS_I2C_INT_CLEAR_REG);
		pr_err("i2c timeout\n");
		if (drvdata->state != I2C_STATE_DONE) {
			pr_err("err %x\n",val);
			mv_css_i2c_hw_init(drvdata);
			ret =  -ETIMEDOUT;
		}
	}
#else
	mv_css_i2c_wait_for_poll(drvdata);
#endif

	return ret;
}




static irqreturn_t mv_css_i2c_irq(int irq, void *dev_id)
{
	struct mv_css_i2c_drvdata *drvdata = (struct mv_css_i2c_drvdata *)dev_id;
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&drvdata->spinlock, flags);

	val = readl(drvdata->base + CSS_I2C_INT_STAT_1_REG);

	if(unlikely((val & CSS_I2C_INT_STAT_ERROR) != 0))
	{
		writel(CSS_I2C_INT_CLEAR_ERROR, drvdata->base + CSS_I2C_INT_CLEAR_REG);
	}
	if((val & CSS_I2C_INT_STAT_RX_DATA_AVAIL) != 0)
	{
		*drvdata->buf++ = readl(drvdata->base + CSS_I2C_READ_DATA_1_REG);
		writel(CSS_I2C_INT_CLEAR_RX_DATA_AVAIL, drvdata->base + CSS_I2C_INT_CLEAR_REG);
	}

	if((val & CSS_I2C_INT_STAT_TX_DATA_EMPTY) != 0)
	{
		writel(CSS_I2C_INT_CLEAR_TX_DATA_EMPTY, drvdata->base + CSS_I2C_INT_CLEAR_REG);
	}

	if((val & CSS_I2C_INT_STAT_TRANS_DONE) != 0)
	{
		complete(&drvdata->completion);
		writel(CSS_I2C_INT_CLEAR_TRANS_DONE, drvdata->base + CSS_I2C_INT_CLEAR_REG);
	}

	spin_unlock_irqrestore(&drvdata->spinlock, flags);

	return IRQ_HANDLED;
}
static int mv_css_i2c_write_1byte(struct mv_css_i2c_drvdata *drvdata, u16 addr)
{
	int ret = 0 ;
	int i;

/*	dbg("i2c seq write  : address : %x, buf : %x,%x size : %d\n",addr, drvdata->buf[0], drvdata->buf[1],
			drvdata->buf_len);*/
dbg("i2c write  : address = %x, buf_len = %d\n",addr, drvdata->buf_len);
#ifdef DATA_DUMP
	for(i=0; i< drvdata->buf_len; i++)
	{
		dbg("buf[%d] = %x\n", i, drvdata->buf[i]);
	}
#endif


	if(drvdata->buf_len > 6)
		return -EINVAL;

	writel(CSS_I2C_CTRL_NORMAL_MODE, drvdata->base + CSS_I2C_CONTROL_REG);

	writel((addr &0xFE), drvdata->base + CSS_I2C_DEVICE_ID_REG);
	writel(drvdata->buf_len, drvdata->base + CSS_I2C_TRANS_NUMBER_REG);
	for(i=0; i<drvdata->buf_len; i++)
	{
		writel(drvdata->buf[i], drvdata->base + CSS_I2C_INDEX_REG+(4*i));
	}

	writel(CSS_I2C_INT_CLEAR_ERROR, drvdata->base + CSS_I2C_INT_CLEAR_REG);
	writel(CSS_I2C_INT_CLEAR_TRANS_DONE, drvdata->base + CSS_I2C_INT_CLEAR_REG);
	writel(CSS_I2C_INT_CLEAR_TX_DATA_EMPTY, drvdata->base + CSS_I2C_INT_CLEAR_REG);
	writel(CSS_I2C_INT_CLEAR_RX_DATA_AVAIL, drvdata->base + CSS_I2C_INT_CLEAR_REG);

	writel(CSS_I2C_START_ON, drvdata->base + CSS_I2C_TRANS_START_REG);

	return ret;
}

static int mv_css_i2c_read_1byte(struct mv_css_i2c_drvdata *drvdata, u16 addr)
{
	int ret = 0;

//	dbg("i2c normal read  : address :0x%x,  0x%x%x\n", addr, drvdata->buf[0], drvdata->buf[1]);
dbg("i2c  read  : address :0x%x,  buf_len =%d\n", addr, drvdata->buf_len);
	if(drvdata->buf_len > 2)
		return -EINVAL;

	/*seq mode next byte write */
	writel(CSS_I2C_CTRL_NORMAL_MODE, drvdata->base + CSS_I2C_CONTROL_REG);
	writel((addr &0xFE) | 0x1, drvdata->base + CSS_I2C_DEVICE_ID_REG);
	writel(drvdata->buf_len, drvdata->base + CSS_I2C_TRANS_NUMBER_REG);

	writel(CSS_I2C_INT_CLEAR_ERROR, drvdata->base + CSS_I2C_INT_CLEAR_REG);
	writel(CSS_I2C_INT_CLEAR_TRANS_DONE, drvdata->base + CSS_I2C_INT_CLEAR_REG);
	writel(CSS_I2C_INT_CLEAR_TX_DATA_EMPTY, drvdata->base + CSS_I2C_INT_CLEAR_REG);
	writel(CSS_I2C_INT_CLEAR_RX_DATA_AVAIL, drvdata->base + CSS_I2C_INT_CLEAR_REG);

	writel(CSS_I2C_START_ON, drvdata->base + CSS_I2C_TRANS_START_REG);

	return ret;
}

static int mv_css_i2c_master_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
			int num)
{
	struct mv_css_i2c_drvdata *drvdata = i2c_get_adapdata(adap);
	int i;
	int ret = 0;
	int rw_buf_len=0;

	enter();

	dbg("i2c master xfer addr: 0x%04x, len: %d, flags: 0x%x, num: %d\n",
			msgs->addr, msgs->len, msgs->flags, num);
	 mutex_lock(&odin_i2c_css_mutex);
	 /*                                                                                            */
//	 if (num >= 2 && msgs[1].len >= 2 && msgs[1].flags & I2C_M_RD)
if (num >= 2 && (msgs[1].flags & (I2C_M_RD | I2C_M_STOP)))
	{
		/* burst read */
		rw_buf_len = i2c_gpio_xfer(adap, msgs, num);

	} else {

		INIT_COMPLETION(drvdata->completion);

		enable_irq(drvdata->irq);

		if(unlikely(readl(drvdata->base + CSS_I2C_CLOCK_CFG_REG) == 0x4))  /* reset value 0x04 check */
			mv_css_i2c_hw_init(drvdata);

		if (mv_css_i2c_wait_for_busy(drvdata))
			mv_css_i2c_hw_init(drvdata);

		drvdata->state = I2C_STATE_BUSY;

		for (i = 0; i < num; i++) {
			drvdata->buf = msgs[i].buf;
			drvdata->buf_len = msgs[i].len;
			rw_buf_len = msgs[i].len;

			if (msgs[i].flags & I2C_M_RD){
				drvdata->rw_mod = READ_MODE;
				ret = mv_css_i2c_read_1byte(drvdata, msgs->addr);
			}else{

				drvdata->rw_mod = WRITE_MODE;
				ret = mv_css_i2c_write_1byte(drvdata, msgs->addr);
			}
			if (ret < 0)
			{
				dbg("ret = %d\n", ret);
				goto out_err;
			}
			ret = mv_css_i2c_wait_for_done(drvdata);
			if (ret < 0)
			{
				dbg("ret = %d\n", ret);
				goto out_err;
			}
		}

		disable_irq(drvdata->irq);
	}

	mutex_unlock(&odin_i2c_css_mutex);
	return rw_buf_len;

out_err:
	disable_irq(drvdata->irq);
	dev_err(&adap->dev, "Failed to transfer: address=0x%04x, data=0x%04x\n",
			msgs[i].addr, *msgs[i].buf);
	mutex_unlock(&odin_i2c_css_mutex);

	drvdata->state = I2C_STATE_IDLE;

	leave();

	return ret;
}

static u32 mv_css_i2c_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | (I2C_FUNC_SMBUS_EMUL & ~I2C_FUNC_SMBUS_QUICK);
}

static struct i2c_algorithm mv_css_i2c_algorithm = {
	.master_xfer = mv_css_i2c_master_xfer,
	.functionality = mv_css_i2c_functionality,
};

static int mv_css_i2c_probe(struct platform_device *pdev)
{
	struct mv_css_i2c_drvdata *drvdata;
	struct i2c_adapter *adap;
	struct resource *res;
#if CONFIG_OF
	u32 val;
	u32 bank_num;
	struct device_node *pnode = NULL;
#endif

	int size;
	int ret;
	printk("odin css i2c probe called\n");

#if CONFIG_OF
    of_property_read_u32(pdev->dev.of_node, "id",&val);
    pdev->id = val;

#if 0
	/* CSS I2C0 */
	if (pdev->id == 10) {
		pnode = of_find_compatible_node(NULL, NULL, "LG,odin-css-gpio3");
		if (pnode) {
			ret = of_property_read_u32(pnode, "bank_num", &bank_num);
			if (ret == 0) {

				/* set GIPO number */
				i2c_gpio_set(bank_num + 2, bank_num + 3);
			} else {
				goto out;
			}

		}
	}

#endif

#if 0 // CONFIG_VIDEO_ODIN_CSS_TEMP_CODES
    #define CSS_SYS_ISPPLL_BASE     0x200F0000
    #define CSS_SYS_ISPPLL_CON0     0x20E0
    #define CSS_SYS_ISPPLL_CON1     0x20E4

    void __iomem *css_reg = NULL;

    css_reg = ioremap(CSS_SYS_ISPPLL_BASE, SZ_4K);

    writel(0x12313011, css_reg + CSS_SYS_ISPPLL_CON0);
    writel(0x11820000, css_reg + CSS_SYS_ISPPLL_CON1);

    if (css_reg)
        	iounmap((void *)css_reg);
#endif

#if 0//CONFIG_VIDEO_ODIN_CSS_TEMP_CODES
	if(pdev->id == 10)
    {
        /* below crg(odin-css-system reg) control codes must be
         * reworked. hope that main crg control driver will take
         * this job like as audio crg control.
        */
        #define CSS_SYS_CRG_BASE        0x330F0000
        #define CSS_SYS_RESET_CTRL      0x00
        #define CSS_SYS_CLK_CTRL        0x04
        #define CSS_SYS_MIPI_CLK_CTRL   0x08
        #define CSS_SYS_AXI_CLK_CTRL    0x0C
        #define CSS_SYS_AHB_CLK_CTRL    0x10
        #define CSS_SYS_ISP_CLK_CTRL    0x14
        #define CSS_SYS_VNR_CLK_CTRL    0x18
        #define CSS_SYS_FPD_CLK_CTRL    0x20
        #define CSS_SYS_FD_CLK_CTRL     0x24
        #define CSS_SYS_PIX_CLK_CTRL    0x28
        #define CSS_SYS_MEM_DS_CTRL     0x2C


        void __iomem *css_crg_reg = NULL;

        css_crg_reg = ioremap(CSS_SYS_CRG_BASE, 0x900);

        writel(0xffffff28, css_crg_reg + CSS_SYS_RESET_CTRL);
        udelay(10);
        writel(0x00000000, css_crg_reg + CSS_SYS_RESET_CTRL);
        writel(0xffffffff, css_crg_reg + CSS_SYS_CLK_CTRL);
        writel(0x00040000, css_crg_reg + CSS_SYS_MIPI_CLK_CTRL);
        writel(0x00000007, css_crg_reg + CSS_SYS_AXI_CLK_CTRL);
        writel(0x00000005, css_crg_reg + CSS_SYS_ISP_CLK_CTRL);
        writel(0x00000005, css_crg_reg + CSS_SYS_VNR_CLK_CTRL);
        writel(0x00000005, css_crg_reg + CSS_SYS_FPD_CLK_CTRL);
        writel(0x00000005, css_crg_reg + CSS_SYS_FD_CLK_CTRL);
        writel(0x00020002, css_crg_reg + CSS_SYS_PIX_CLK_CTRL);
		writel(0x00000000, css_crg_reg + CSS_SYS_MEM_DS_CTRL);


        if(css_crg_reg)
        	iounmap((void *)css_crg_reg);
    }
#endif
#endif

    enter();

	drvdata = kzalloc(sizeof(struct mv_css_i2c_drvdata), GFP_KERNEL);
	if (!drvdata) {
		dev_err(&pdev->dev, "Can't allocate memory for driver data\n");
		ret = -ENOMEM;
		goto out;
	}

	platform_set_drvdata(pdev, drvdata);
	drvdata->pdev = pdev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Can't get memory resource\n");
		ret = -ENXIO;
		goto out_kfree_drvdata;
	}

	size = res->end - res->start + 1;
	drvdata->mem = request_mem_region(res->start, size, pdev->name);
	if (!drvdata->mem) {
		dev_err(&pdev->dev, "Can't get memory region\n");
		ret = -ENOENT;
		goto out_kfree_drvdata;
	}

	drvdata->base = ioremap(res->start, size);
	if (!drvdata->base) {
		dev_err(&pdev->dev, "Can't remap memory\n");
		ret = -ENXIO;
		goto out_release_mem;
	}

	drvdata->irq = platform_get_irq(pdev, 0);
	if (drvdata->irq < 0) {
		dev_err(&pdev->dev, "Can't get IRQ resource\n");
		ret = drvdata->irq;
		goto out_iounmap;
	}

#ifndef POLL_MODE
	ret = request_irq(drvdata->irq, mv_css_i2c_irq, 0, pdev->name, drvdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't request IRQ for I2C(%d)\n",
			drvdata->irq);
		goto out_iounmap;
	}

	disable_irq(drvdata->irq);
#endif

	spin_lock_init(&drvdata->spinlock);
	init_completion(&drvdata->completion);
	msecs1000_to_jiffies = msecs_to_jiffies(1000);

	drvdata->cond = 0;
	adap = &drvdata->adap;
	i2c_set_adapdata(adap, drvdata);
	adap->owner = THIS_MODULE;
	strncpy(adap->name, DRV_NAME, sizeof(adap->name));
	adap->algo = &mv_css_i2c_algorithm;
	adap->dev.parent = &pdev->dev;
	adap->nr = pdev->id;

	ret = i2c_add_numbered_adapter(adap);
	if (ret) {
		dev_err(&pdev->dev, "Can't add I2C adapter\n");
		goto out_free_irq;
	}

	goto out;

out_free_irq:
	free_irq(drvdata->irq, drvdata);
out_iounmap:
	iounmap(drvdata->base);
out_release_mem:
	release_mem_region(drvdata->mem->start, size);
out_kfree_drvdata:
	kfree(drvdata);
out:
	leave();

	return ret;
}

static int mv_css_i2c_remove(struct platform_device *pdev)
{
	struct mv_css_i2c_drvdata *drvdata = platform_get_drvdata(pdev);
	struct i2c_adapter *adap = &drvdata->adap;
	int size = drvdata->mem->end - drvdata->mem->start + 1;

	printk("%s called\n", __func__);
	i2c_del_adapter(adap);

	if (drvdata->hw_init)
		mv_css_i2c_hw_uninit(drvdata);

	free_irq(drvdata->irq, drvdata);

	iounmap(drvdata->base);
	drvdata->base = NULL;

	release_mem_region(drvdata->mem->start, size);
	drvdata->mem = NULL;

	kfree(drvdata);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int mv_css_i2c_suspend(struct platform_device *pdev, pm_message_t pm)
{
	return 0;
}

static int mv_css_i2c_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define mv_css_i2c_suspend		NULL
#define mv_css_i2c_resume		NULL
#endif /* CONFIG_PM */

#ifdef CONFIG_OF
static const struct of_device_id css_i2c_of_match[] = {
	{ .compatible = "mtekvision,css-i2c"},
	{},
};
#endif

static struct platform_driver mv_css_i2c_driver = {
	.probe = mv_css_i2c_probe,
	.remove = mv_css_i2c_remove,
	.suspend = mv_css_i2c_suspend,
	.resume = mv_css_i2c_resume,
	.driver = {
		   .name = DRV_NAME,
#ifdef CONFIG_OF
    .of_match_table = of_match_ptr(css_i2c_of_match),
#endif
	},
};

static int __init mv_css_i2c_init(void)
{
	printk("%s called \n", __func__);
	return platform_driver_register(&mv_css_i2c_driver);
}

static void __exit mv_css_i2c_exit(void)
{
	platform_driver_unregister(&mv_css_i2c_driver);
}

/* subsys_initcall(mv_css_i2c_init); */
late_initcall(mv_css_i2c_init);
module_exit(mv_css_i2c_exit);

MODULE_DESCRIPTION("MtekVision ODIN CSS I2C Controller driver");
MODULE_AUTHOR("Kimsunki, <kimska@mtekvision.com>");
MODULE_LICENSE("GPL");


