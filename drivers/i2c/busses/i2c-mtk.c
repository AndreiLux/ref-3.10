/*
 *
 * (C) Copyright 2011
 * MediaTek <www.mediatek.com>
 * Infinity Chen <infinity.chen@mediatek.com>
 *
 * mt8320 I2C Bus Controller
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
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <asm/system.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_gpio_def.h>
#include <mach/mt_pmic_wrap.h>
#include <i2c-mtk.h>
#include <mach/i2c.h>
/* #define I2C_PRINT_ERROR_LOG */

#define MT_I2C_PM_TIMEOUT 10

static inline void i2c_writew(struct mt_i2c *i2c, u8 offset, u16 value)
{
	writew(value, (void *)((i2c->base) + (offset)));
}

static inline u16 i2c_readw(struct mt_i2c *i2c, u8 offset)
{
	return readw((void const *)((i2c->base) + (offset)));
}

#ifdef I2C_DEBUG

#define PORT_COUNT 7
#define MESSAGE_COUNT 16
#define I2C_T_DMA 1
#define I2C_T_TRANSFERFLOW 2
#define I2C_T_SPEED 3
/*7 ports,16 types of message*/
u8 i2c_port[PORT_COUNT][MESSAGE_COUNT];

#define I2C_INFO(i2c, type, format, arg...) do { \
	if (type < MESSAGE_COUNT && type >= 0) { \
		if (i2c_port[i2c->id][0] != 0 \
			&& (i2c_port[i2c->id][type] != 0 \
			|| i2c_port[i2c->id][MESSAGE_COUNT - 1] != 0)) { \
			dev_alert(i2c->dev, format, ## arg); \
		} \
	} \
} while (0)

static ssize_t show_config(
	struct device *dev, struct device_attribute *attr, char *buff)
{
	int i = 0;
	int j = 0;
	ssize_t rsl = 0;
	char *buf = buff;
	for (i = 0; i < PORT_COUNT; i++) {
		for (j = 0; j < MESSAGE_COUNT; j++)
			i2c_port[i][j] += '0';
		strncpy(buf, (char *)i2c_port[i], MESSAGE_COUNT);
		buf += MESSAGE_COUNT;
		*buf = '\n';
		buf++;
		for (j = 0; j < MESSAGE_COUNT; j++)
			i2c_port[i][j] -= '0';
	}
	rsl = buf - buff;
	return rsl;
}

static ssize_t set_config(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int port, type, status;
	int status = -1;
	status = sscanf(buf, "%d %d %d", &port, &type, &status);
	if (status > 0) {
		if (port >= PORT_COUNT || port < 0 ||
			type >= MESSAGE_COUNT || type < 0) {
			/*Invalid param */
			dev_err(dev,
				"i2c debug system: Parameter overflowed!\n");
		} else {
			if (status != 0)
				i2c_port[port][type] = 1;
			else
				i2c_port[port][type] = 0;

			dev_alert(dev,
				"port:%d type:%d status:%s\n"
				"i2c debug system: Parameter accepted!\n",
				port, type, status ? "on" : "off");
		}
	} else {
		/*parameter invalid */
		dev_err(dev, "i2c debug system: Parameter invalid!\n");
	}
	return count;
}

static DEVICE_ATTR(debug, S_IRUGO | S_IWUSR | S_IWGRP,
	show_config, set_config);

#else
#define I2C_INFO(mt_i2c, type, format, arg...) dev_dbg(mt_i2c->dev, format, ## arg)
#endif

static int mt_i2c_runtime_resume(struct device *dev)
{
	struct mt_i2c *i2c = dev_get_drvdata(dev);
	struct mt_i2c_data *pdata = dev_get_platdata(dev);
#if (!defined(CONFIG_MT_I2C_FPGA_ENABLE))
	I2C_INFO(i2c, I2C_T_TRANSFERFLOW, "Before dma clk enable\n");
	/*TODO: this must go away to platform DMA code */
	pdata->enable_dma_clk(pdata, true);
	I2C_INFO(i2c, I2C_T_TRANSFERFLOW, "Before i2c clk enable\n");
	pdata->enable_clk(pdata, true);
	I2C_INFO(i2c, I2C_T_TRANSFERFLOW, "clk enable done\n");
#endif
	return 0;
}

static int mt_i2c_runtime_suspend(struct device *dev)
{
	struct mt_i2c *i2c = dev_get_drvdata(dev);
	struct mt_i2c_data *pdata = dev_get_platdata(dev);
#if (!defined(CONFIG_MT_I2C_FPGA_ENABLE))
	I2C_INFO(i2c, I2C_T_TRANSFERFLOW, "Before dma clk disable\n");
	pdata->enable_dma_clk(pdata, false);
	I2C_INFO(i2c, I2C_T_TRANSFERFLOW, "Before i2c clk disable\n");
	pdata->enable_clk(pdata, false);
	I2C_INFO(i2c, I2C_T_TRANSFERFLOW, "clk disable done\n");
#endif
	return 0;
}

static void free_i2c_dma_bufs(struct mt_i2c *i2c)
{
	if (i2c->dma_buf.vaddr)
		dma_free_coherent(NULL, PAGE_SIZE, i2c->dma_buf.vaddr,
			i2c->dma_buf.paddr);
}

/*Set i2c port speed*/
static int mt_i2c_set_bus_speed(struct mt_i2c *i2c, u16 speed)
{
	int ret = 0;
	int mode = 0;
	unsigned long khz = 0;
	/* u32 base = i2c->base; */
	unsigned short step_cnt_div = 0;
	unsigned short sample_cnt_div = 0;
	unsigned long tmp, sclk, hclk = i2c->clk;
	unsigned short max_step_cnt_div = 0;
	unsigned long diff, min_diff = i2c->clk;
	unsigned short sample_div = MAX_SAMPLE_CNT_DIV;
	unsigned short step_div = max_step_cnt_div;
	struct mt_i2c_data *pdata = dev_get_platdata(i2c->adap.dev.parent);

	if (speed <= MAX_ST_MODE_SPEED) {
		mode = ST_MODE;
		khz = MAX_ST_MODE_SPEED;
	} else {
		if ((pdata->flags & MT_I2C_HS)) {
			mode = HS_MODE;
			khz = speed;
		} else {
			mode = FS_MODE;
			khz = speed;
		}
	}

	max_step_cnt_div = (mode == HS_MODE) ?
		MAX_HS_STEP_CNT_DIV : MAX_STEP_CNT_DIV;

	if ((mode == FS_MODE && khz > MAX_FS_MODE_SPEED)
	    || (mode == HS_MODE && khz > MAX_HS_MODE_SPEED)) {
		dev_err(i2c->dev,
			"the speed is too fast for this mode.\n");
		I2C_BUG_ON((mode == FS_MODE && khz > MAX_FS_MODE_SPEED)
			   || (mode == HS_MODE && khz > MAX_HS_MODE_SPEED));
		ret = -EINVAL;
		goto end;
	}
	if ((mode == i2c->mode) && (khz == i2c->sclk)) {
		I2C_INFO(i2c, I2C_T_SPEED,
			"set sclk to %ldkhz\n", i2c->sclk);
		return 0;
	}

	/*Find the best combination */
	for (sample_cnt_div = 1;
		sample_cnt_div <= MAX_SAMPLE_CNT_DIV; sample_cnt_div++) {
		for (step_cnt_div = 1;
			step_cnt_div <= max_step_cnt_div; step_cnt_div++) {
			sclk = (hclk >> 1) / (sample_cnt_div * step_cnt_div);
			if (sclk > khz)
				continue;
			diff = khz - sclk;
			if (diff < min_diff) {
				min_diff = diff;
				sample_div = sample_cnt_div;
				step_div = step_cnt_div;
			}
		}
	}
	sample_cnt_div = sample_div;
	step_cnt_div = step_div;

	sclk = hclk / (2 * sample_cnt_div * step_cnt_div);
	if (sclk > khz) {
		dev_err(i2c->dev, "%s mode: unsupported speed (%ldkhz)\n",
			(mode == HS_MODE) ? "HS" : "ST/FT", khz);
		I2C_BUG_ON(sclk > khz);
		ret = -ENOTSUPP;
		goto end;
	}

	step_cnt_div--;
	sample_cnt_div--;

	spin_lock(&i2c->lock);

	if (mode == HS_MODE) {

		/*Set the high-speed timing control register */
		tmp = i2c_readw(i2c, OFFSET_TIMING) & ~((0x7 << 8)
			| (0x3f << 0));
		tmp = (0 & 0x7) << 8 | (16 & 0x3f) << 0 | tmp;
		i2c_writew(i2c, OFFSET_TIMING, tmp);

		/*Set the high-speed mode register */
		tmp = i2c_readw(i2c, OFFSET_HS) & ~((0x7 << 12) | (0x7 << 8));
		tmp = (sample_cnt_div & 0x7) << 12
			| (step_cnt_div & 0x7) << 8 | tmp;
		/*Enable the hign speed transaction */
		tmp |= 0x0001;
		i2c_writew(i2c, OFFSET_HS, tmp);
	} else {
		/*Set non-highspeed timing */
		tmp = i2c_readw(i2c, OFFSET_TIMING) & ~((0x7 << 8)
			| (0x3f << 0));
		tmp = (sample_cnt_div & 0x7) << 8 |
			(step_cnt_div & 0x3f) << 0 | tmp;
		i2c_writew(i2c, OFFSET_TIMING, tmp);
		/*Disable the high-speed transaction */
		tmp = i2c_readw(i2c, OFFSET_HS) & ~(0x0001);
		i2c_writew(i2c, OFFSET_HS, tmp);
	}
	i2c->mode = mode;
	i2c->sclk = khz;
	spin_unlock(&i2c->lock);
	I2C_INFO(i2c, I2C_T_SPEED,
		"set sclk to %ldkhz(orig:%ldkhz), sample=%d,step=%d\n",
		sclk, khz, sample_cnt_div, step_cnt_div);
end:
	return ret;
}

static void mt_i2c_post_isr(struct mt_i2c *i2c, struct mt_i2c_msg *msg)
{
	/* u16 addr = msg->addr; */

	if (i2c->irq_stat & I2C_TRANSAC_COMP) {
		atomic_set(&i2c->trans_err, 0);
		atomic_set(&i2c->trans_comp, 1);
	}

	if (i2c->irq_stat & I2C_HS_NACKERR) {
		if (likely(!(msg->ext_flag & I2C_A_FILTER_MSG))) {
#ifdef I2C_PRINT_ERROR_LOG
			dev_err(i2c->dev, "I2C_HS_NACKERR\n");
#endif
		}
	}

	if (i2c->irq_stat & I2C_ACKERR) {
		if (likely(!(msg->ext_flag & I2C_A_FILTER_MSG))) {
#ifdef I2C_PRINT_ERROR_LOG
			dev_err(i2c->dev, "I2C_ACKERR\n");
#endif
		}
	}
	atomic_set(&i2c->trans_err, i2c->irq_stat &
		(I2C_HS_NACKERR | I2C_ACKERR));
}

static inline void mt_i2c_dump_info(struct mt_i2c *i2c)
{
#ifdef I2C_PRINT_ERROR_LOG
	dev_err(i2c->dev, "I2C structure:\nMode %x\nSt_rs %x\n", i2c->mode, i2c->st_rs);
	dev_err(i2c->dev,
		"I2C structure:\nDma_en %x\nOp %x\n",
		i2c->dma_en, i2c->op);
	dev_err(i2c->dev,
		"I2C structure:\nTrans_len %x\nTrans_num %x\nTrans_auxlen %x\n",
		i2c->trans_data.trans_len, i2c->trans_data.trans_num, i2c->trans_data.trans_auxlen);
	dev_err(i2c->dev,
		"I2C structure:\nData_size %x\nIrq_stat %x\nTrans_stop %u\n",
		i2c->trans_data.data_size, i2c->irq_stat, atomic_read(&i2c->trans_stop));
	dev_err(i2c->dev,
		"I2C structure:\nTrans_comp %u\nTrans_error %u\n",
		atomic_read(&i2c->trans_comp), atomic_read(&i2c->trans_err));

	dev_err(i2c->dev, "base address %x\n", i2c->base);
	dev_err(i2c->dev,
		"I2C register:\nSLAVE_ADDR %x\nINTR_MASK %x\n",
		(i2c_readw(i2c, OFFSET_SLAVE_ADDR)), (i2c_readw(i2c, OFFSET_INTR_MASK)));
	dev_err(i2c->dev,
		"I2C register:\nINTR_STAT %x\nCONTROL %x\n",
		(i2c_readw(i2c, OFFSET_INTR_STAT)), (i2c_readw(i2c, OFFSET_CONTROL)));
	dev_err(i2c->dev,
		"I2C register:\nTRANSFER_LEN %x\nTRANSAC_LEN %x\n",
		(i2c_readw(i2c, OFFSET_TRANSFER_LEN)), (i2c_readw(i2c, OFFSET_TRANSAC_LEN)));
	dev_err(i2c->dev,
		"I2C register:\nDELAY_LEN %x\nTIMING %x\n",
		(i2c_readw(i2c, OFFSET_DELAY_LEN)), (i2c_readw(i2c, OFFSET_TIMING)));
	dev_err(i2c->dev,
		"I2C register:\nSTART %x\nFIFO_STAT %x\n",
		(i2c_readw(i2c, OFFSET_START)), (i2c_readw(i2c, OFFSET_FIFO_STAT)));
	dev_err(i2c->dev,
		"I2C register:\nIO_CONFIG %x\nHS %x\n",
		(i2c_readw(i2c, OFFSET_IO_CONFIG)), (i2c_readw(i2c, OFFSET_HS)));
	dev_err(i2c->dev,
		"I2C register:\nDEBUGSTAT %x\nEXT_CONF %x\nPATH_DIR %x\n",
		(i2c_readw(i2c, OFFSET_DEBUGSTAT)), (i2c_readw(i2c, OFFSET_EXT_CONF)),
		(i2c_readw(i2c, OFFSET_PATH_DIR)));
	/* I2C*_PDN and AP_DMA_PDN bit must be 0 */
	dev_err(i2c->dev, "Peripheral power down0 register 0xF0003018 = 0x%x 0xF000301c = 0x%x\n",
		(__raw_readl((void const *)0xF0003018)), (__raw_readl((void const *)0xF000301c)));

	/*dev_err(i2c->dev, "DMA register:\nINT_FLAG %x\n
	   CON %x\nTX_MEM_ADDR %x\n
	   RX_MEM_ADDR %x\nTX_LEN %x\n
	   RX_LEN %x\nINT_EN %x\nEN %x\n",
	   (__raw_readl(i2c->pdmabase+OFFSET_INT_FLAG)),
	   (__raw_readl(i2c->pdmabase+OFFSET_CON)),
	   (__raw_readl(i2c->pdmabase+OFFSET_TX_MEM_ADDR)),
	   (__raw_readl(i2c->pdmabase+OFFSET_RX_MEM_ADDR)),
	   (__raw_readl(i2c->pdmabase+OFFSET_TX_LEN)),
	   (__raw_readl(i2c->pdmabase+OFFSET_RX_LEN)),
	   (__raw_readl(i2c->pdmabase+OFFSET_INT_EN)),
	   (__raw_readl(i2c->pdmabase+OFFSET_EN))); */

	/*8135 side and PMIC side clock */
	dev_err(i2c->dev, "Clock %s\n",
		(((__raw_readl((void const *)0xF0003018) >> 26) |
		  (__raw_readl((void const *)0xF000301c) & 0x1 << 6)) & (1 << i2c->id)) ? "disable"
		: "enable");
#endif
	return;
}

static int mt_deal_result(struct mt_i2c *i2c, struct mt_i2c_msg *msg)
{
	long tmo = i2c->adap.timeout;
	u16 data_size = 0;
	u8 *ptr = msg->buf;
	u16 addr = msg->addr;
	int ret = msg->len;
	u16 read = (msg->flags & I2C_M_RD);


	addr = read ? ((addr << 1) | 0x1) : ((addr << 1) & ~0x1);

	/*Interrupt mode,wait for interrupt wake up */
		tmo = wait_event_timeout(i2c->wait,
			atomic_read(&i2c->trans_stop), tmo);

	/*Save register status to i2c struct */
	mt_i2c_post_isr(i2c, msg);

	/*Check the transfer status */
	if (!(tmo == 0 || atomic_read(&i2c->trans_err))) {
		/* Transfer success ,we need to get data from fifo.
		 * only read mode or write_read mode and fifo mode
		 * need to get data */
		if (!i2c->dma_en && (i2c->op == I2C_MASTER_RD
			|| i2c->op == I2C_MASTER_WRRD)) {
			ptr = msg->buf;
			data_size =
				(i2c_readw(i2c, OFFSET_FIFO_STAT) >> 4) & 0xF;
			while (data_size--) {
				*ptr = i2c_readw(i2c, OFFSET_DATA_PORT);
				ptr++;
			}
		}
	} else {		/*Timeout or ACKERR */
		if (!tmo) {
#ifdef I2C_PRINT_ERROR_LOG
			dev_err(i2c->dev,
				"addr: %.2x, transfer timeout\n", addr);
#endif
			ret = -ETIMEDOUT;
		} else {
#ifdef I2C_PRINT_ERROR_LOG
			dev_err(i2c->dev,
				"addr: %.2x, transfer error\n", addr);
#endif
			ret = -EREMOTEIO;
		}

		if (likely(!(msg->ext_flag & I2C_A_FILTER_MSG))) {
			/*Dump i2c_struct & register */
			mt_i2c_dump_info(i2c);
		}

		spin_lock(&i2c->lock);
		/*Reset i2c port */
		i2c_writew(i2c, OFFSET_SOFTRESET, 0x0001);
		/*Set slave address */
		i2c_writew(i2c, OFFSET_SLAVE_ADDR, 0x0000);
		/*Clear interrupt status */
		i2c_writew(i2c, OFFSET_INTR_STAT,
			(I2C_HS_NACKERR | I2C_ACKERR | I2C_TRANSAC_COMP));
		/*Clear fifo address */
		i2c_writew(i2c, OFFSET_FIFO_ADDR_CLR, 0x0001);

		spin_unlock(&i2c->lock);
	}

	return ret;
}

static inline u32 mt_i2c_get_func_clk(struct mt_i2c *i2c)
{
	struct mt_i2c_data *pdata = dev_get_platdata(i2c->adap.dev.parent);
	return pdata->get_func_clk(pdata);
}

static int mt_i2c_start_xfer(struct mt_i2c *i2c, struct mt_i2c_msg *msg)
{
	struct mt_i2c_data *pdata = dev_get_platdata(i2c->adap.dev.parent);
	u16 control = 0;
	u16 trans_num = 0;
	u16 data_size = 0;
	u16 trans_len = 0;
	u8 *ptr = msg->buf;
	u16 len = msg->len;
	int ret = len;
	u16 trans_auxlen = 0;
	u16 addr = msg->addr;
	u16 flags = msg->flags;
	u16 read = (flags & I2C_M_RD);

	if ((msg->ext_flag & I2C_DMA_FLAG))
		i2c->dma_en = true;
	else
		i2c->dma_en = false;

	pm_runtime_get_sync(i2c->adap.dev.parent);

	/*Check param valid */
	if (addr == 0) {
		dev_err(i2c->dev, "addr is invalid.\n");
		I2C_BUG_ON(addr == NULL);
		ret = -EINVAL;
		goto err;
	}

	if (msg->buf == NULL) {
		dev_err(i2c->dev, "data buffer is NULL.\n");
		I2C_BUG_ON(msg->buf == NULL);
		ret = -EINVAL;
		goto err;
	}

	/* compatible with 77/75 driver */
	if (msg->addr & 0xFF00) {
		msg->ext_flag |= msg->addr & 0xFF00;
		/* I2C_BUG_ON(msg->addr & 0xFF00); */

		/*go on... */

		/* ret = -EINVAL; */
		/* goto err; */
	}

	if (g_i2c[0] == i2c || g_i2c[1] == i2c) {
		dev_err(i2c->dev,
			"mt-i2c%d: Current I2C Adapter is busy.\n", i2c->id);
		ret = -EINVAL;
		goto err;
	}

	I2C_INFO(i2c, I2C_T_TRANSFERFLOW, "Before i2c transfer\n");

	/*We must set wrapper bit before setting other register */
	if (pdata->flags & MT_WRAPPER_BUS)
		i2c_writew(i2c, OFFSET_PATH_DIR, I2C_CONTROL_WRAPPER);

	i2c->clk = mt_i2c_get_func_clk(i2c);

	ret = mt_i2c_set_bus_speed(i2c, pdata->speed);
	if (ret) {
		dev_err(i2c->dev,
			"failed to configure bus speed=%d; err=%d\n",
			pdata->speed, ret);
		goto err;
	}

	/*set start condition */
	if (pdata->speed <= 100)
		i2c_writew(i2c, OFFSET_EXT_CONF, 0x8001);

	/*prepare data */
	if ((msg->ext_flag & I2C_RS_FLAG))
		i2c->st_rs = I2C_TRANS_REPEATED_START;
	else
		i2c->st_rs = I2C_TRANS_STOP;

	if (i2c->dma_en) {
		I2C_INFO(i2c, I2C_T_DMA, "DMA Transfer mode!\n");
		if (i2c->pdmabase == 0) {
			dev_err(i2c->dev,
				"I2C%d does not support DMA mode!\n",
				i2c->id);
			I2C_BUG_ON(i2c->pdmabase == NULL);
			ret = -EINVAL;
			goto err;
		}
		if ((u32) ptr > DMA_ADDRESS_HIGH) {
			dev_err(i2c->dev,
				"DMA mode needs phys_addr: %d\n", (u32)ptr);
			I2C_BUG_ON((u32) ptr > DMA_ADDRESS_HIGH);
			ret = -EINVAL;
			goto err;
		}
	}

	i2c->delay_len = pdata->delay_len;

	if ((msg->ext_flag & I2C_WR_FLAG))
		i2c->op = I2C_MASTER_WRRD;
	else {
		if (msg->flags & I2C_M_RD)
			i2c->op = I2C_MASTER_RD;
		else
			i2c->op = I2C_MASTER_WR;
	}


	atomic_set(&i2c->trans_stop, 0);
	atomic_set(&i2c->trans_comp, 0);
	atomic_set(&i2c->trans_err, 0);
	i2c->irq_stat = 0;
	addr = read ? ((addr << 1) | 0x1) : ((addr << 1) & ~0x1);

	/*Get Transfer len and transaux len */
	if (false == i2c->dma_en) {	/*non-DMA mode */
		if (I2C_MASTER_WRRD != i2c->op) {

			trans_len = (msg->len) & 0xFF;
			trans_num = (msg->len >> 8) & 0xFF;
			if (0 == trans_num)
				trans_num = 1;
			trans_auxlen = 0;
			data_size = trans_len * trans_num;

			if (!trans_len || !trans_num
				|| trans_len * trans_num > 8) {
				dev_err(i2c->dev,
					"non-WRRD xfer len is wrong.\n"
					"trs_len=%x,ts_num=%x,trs_auxlen=%x\n",
					trans_len, trans_num, trans_auxlen);
				I2C_BUG_ON(!trans_len || !trans_num
					|| trans_len * trans_num > 8);
				ret = -EINVAL;
				goto err;
			}
		} else {

			trans_len = (msg->len) & 0xFF;
			trans_auxlen = (msg->len >> 8) & 0xFF;
			trans_num = 2;
			data_size = trans_len;

			if (!trans_len || !trans_auxlen
				|| trans_len > 8 || trans_auxlen > 8) {
				dev_err(i2c->dev,
					"WRRD xfer len is wrong.\n"
					"trs_len=%x,ts_num=%x,trs_auxlen=%x\n",
					trans_len, trans_num, trans_auxlen);
				I2C_BUG_ON(!trans_len
					|| !trans_auxlen || trans_len > 8
					|| trans_auxlen > 8);
				ret = -EINVAL;
				goto err;
			}
		}
	} else {		/*DMA mode */
		if (I2C_MASTER_WRRD != i2c->op) {
			trans_len = (msg->len) & 0xFF;
			trans_num = (msg->len >> 8) & 0xFF;
			if (0 == trans_num)
				trans_num = 1;
			trans_auxlen = 0;
			data_size = trans_len * trans_num;

			if (!trans_len || !trans_num
				|| trans_len > 255 || trans_num > 255) {
				dev_err(i2c->dev,
					"DMA non-WRRD xfer len is wrong.\n"
					"trs_len=%x,ts_num=%x,trs_auxlen=%x\n",
					trans_len, trans_num, trans_auxlen);
				I2C_BUG_ON(!trans_len
					|| !trans_num || trans_len > 255
					|| trans_num > 255);
				ret = -EINVAL;
				goto err;
			}
			I2C_INFO(i2c, I2C_T_DMA,
				"DMA non-WRRD xfer len is wrong.\n"
				"trs_len=%x,ts_num=%x,trs_auxlen=%x\n",
				trans_len, trans_num, trans_auxlen);
		} else {
			trans_len = (msg->len) & 0xFF;
			trans_auxlen = (msg->len >> 8) & 0xFF;
			trans_num = 2;
			data_size = trans_len;
			if (!trans_len || !trans_auxlen
				|| trans_len > 255 || trans_auxlen > 31) {
				dev_err(i2c->dev,
					"mt-i2c:DMA WRRD len wrong.\n"
					"trans_len=%x, tans_num=%x, trans_auxlen=%x\n",
					trans_len, trans_num, trans_auxlen);
				I2C_BUG_ON(!trans_len
					|| !trans_auxlen || trans_len > 255
					|| trans_auxlen > 31);
				ret = -EINVAL;
				goto err;
			}
			I2C_INFO(i2c, I2C_T_DMA,
				"DMA WRRD mode: trans_len=%x\n"
				"trans_num=%x, trans_auxlen=%x\n",
				 trans_len, trans_num, trans_auxlen);
		}
	}

	spin_lock(&i2c->lock);

	/*Set ioconfig */
	if ((msg->ext_flag & I2C_PUSHPULL_FLAG))
		i2c_writew(i2c, OFFSET_IO_CONFIG, 0x0000);
	else
		i2c_writew(i2c, OFFSET_IO_CONFIG, 0x0003);

	/*Set Control Register */
	control = I2C_CONTROL_ACKERR_DET_EN | I2C_CONTROL_CLK_EXT_EN;
	if (i2c->dma_en)
		control |= I2C_CONTROL_DMA_EN;

	if (I2C_MASTER_WRRD == i2c->op)
		control |= I2C_CONTROL_DIR_CHANGE;

	if (HS_MODE == i2c->mode || (trans_num > 1 &&
		I2C_TRANS_REPEATED_START == i2c->st_rs)) {
		control |= I2C_CONTROL_RS;
	}
	i2c_writew(i2c, OFFSET_CONTROL, control);
	if (~control & I2C_CONTROL_RS)
		/* bit is set to 1, i.e.,use repeated stop */
		i2c_writew(i2c, OFFSET_DELAY_LEN, i2c->delay_len);

	/*Setup Registers */

	/*Set slave address */
	i2c_writew(i2c, OFFSET_SLAVE_ADDR, addr);
	/*Clear interrupt status */
	i2c_writew(i2c, OFFSET_INTR_STAT,
		(I2C_HS_NACKERR | I2C_ACKERR | I2C_TRANSAC_COMP));
	/*Clear fifo address */
	i2c_writew(i2c, OFFSET_FIFO_ADDR_CLR, 0x0001);

	/*Enable interrupt */
	i2c_writew(i2c, OFFSET_INTR_MASK,
		i2c_readw(i2c,
			OFFSET_INTR_MASK) | (I2C_HS_NACKERR
			| I2C_ACKERR | I2C_TRANSAC_COMP));
	/*Set transfer len */
	i2c_writew(i2c, OFFSET_TRANSFER_LEN,
		((trans_auxlen & 0x1F) << 8) | (trans_len & 0xFF));
	/*Set transaction len */
	i2c_writew(i2c, OFFSET_TRANSAC_LEN, trans_num & 0xFF);

	/*Prepare buffer data to start transfer */

	if (i2c->dma_en) {
		if (I2C_MASTER_RD == i2c->op) {
			writel(0x0000,
				(void *)(i2c->pdmabase + OFFSET_INT_FLAG));
			writel(0x0001,
				(void *)(i2c->pdmabase + OFFSET_CON));
			writel((u32) msg->buf, (void *)(i2c->pdmabase
				+ OFFSET_RX_MEM_ADDR));
			writel(data_size,
				(void *)(i2c->pdmabase + OFFSET_RX_LEN));
		} else if (I2C_MASTER_WR == i2c->op) {
			writel(0x0000, (void *)(i2c->pdmabase
				+ OFFSET_INT_FLAG));
			writel(0x0000, (void *)(i2c->pdmabase
				+ OFFSET_CON));
			writel((u32) msg->buf, (void *)(i2c->pdmabase
				+ OFFSET_TX_MEM_ADDR));
			writel(data_size, (void *)(i2c->pdmabase
				+ OFFSET_TX_LEN));
		} else {
			writel(0x0000,
				(void *)(i2c->pdmabase + OFFSET_INT_FLAG));
			writel(0x0000,
				(void *)(i2c->pdmabase + OFFSET_CON));
			writel((u32) msg->buf,
				(void *)(i2c->pdmabase + OFFSET_TX_MEM_ADDR));
			writel((u32) msg->buf,
			       (void *)(i2c->pdmabase + OFFSET_RX_MEM_ADDR));
			writel(trans_len,
				(void *)(i2c->pdmabase + OFFSET_TX_LEN));
			writel(trans_auxlen,
				(void *)(i2c->pdmabase + OFFSET_RX_LEN));
		}
		/* flash config before sending start */
		mb();

		writel(0x0001, (void *)(i2c->pdmabase + OFFSET_EN));

		I2C_INFO(i2c, I2C_T_DMA,
			"addr %.2x dma %.2X byte\n", addr, data_size);
		I2C_INFO(i2c, I2C_T_DMA,
			"DMA Register:INT_FLAG:0x%x,CON:0x%x\n",
			readl((void *)(i2c->pdmabase + OFFSET_INT_FLAG)),
			readl((void *)(i2c->pdmabase + OFFSET_CON)));
		I2C_INFO(i2c, I2C_T_DMA, "DMA Register:RX_MEM_ADDR:0x%x\n",
			readl((void *)(i2c->pdmabase + OFFSET_RX_MEM_ADDR)));
		I2C_INFO(i2c, I2C_T_DMA, "DMA Register:TX_MEM_ADDR:0x%x,\n",
			readl((void *)(i2c->pdmabase + OFFSET_TX_MEM_ADDR)));
		I2C_INFO(i2c, I2C_T_DMA,
			"DMA Register:TX_LEN:0x%x,RX_LEN:0x%x,EN:0x%x\n",
			readl((void *)(i2c->pdmabase + OFFSET_TX_LEN)),
			readl((void *)(i2c->pdmabase + OFFSET_RX_LEN)),
			readl((void *)(i2c->pdmabase + OFFSET_EN)));

	} else {		/*Set fifo mode data */
		if (I2C_MASTER_RD == i2c->op) {
			/*do not need set fifo data */
		} else {
			/*both write && write_read mode */
			while (data_size--) {
				i2c_writew(i2c, OFFSET_DATA_PORT, *ptr);
				ptr++;
			}
		}
	}

	/*Set trans_data */
	i2c->trans_data.trans_num = trans_num;
	i2c->trans_data.data_size = data_size;
	i2c->trans_data.trans_len = trans_len;
	i2c->trans_data.trans_auxlen = trans_auxlen;


	/*All registers must be prepared before setting the start bit [SMP] */
	mb();

	/*This is only for 3D CAMERA */
	if (msg->ext_flag & I2C_3DCAMERA_FLAG) {

		spin_unlock(&i2c->lock);

		if (g_msg[0].buf == NULL)
			/*Save address infomation for 3d camera */
			memcpy((void *)&g_msg[0], msg,
				sizeof(struct mt_i2c_msg));
		else
			/*Save address infomation for 3d camera */
			memcpy((void *)&g_msg[1], msg,
				sizeof(struct mt_i2c_msg));

		if (g_i2c[0] == NULL)
			g_i2c[0] = i2c;
		else
			g_i2c[1] = i2c;

		goto end;
	}
	/*Start the transfer */
	i2c_writew(i2c, OFFSET_START, 0x0001);
	spin_unlock(&i2c->lock);

	ret = mt_deal_result(i2c, msg);

	I2C_INFO(i2c, I2C_T_TRANSFERFLOW, "After i2c transfer\n");
err:

end:
	if (!i2c->suspended) {
		pm_runtime_mark_last_busy(i2c->adap.dev.parent);
		pm_runtime_put_autosuspend(i2c->adap.dev.parent);
	} else
		pm_runtime_put_sync(i2c->adap.dev.parent);
	return ret;
}

/*Interrupt handler function*/
static irqreturn_t mt_i2c_irq(int irqno, void *dev_id)
{
	struct mt_i2c *i2c = (struct mt_i2c *)dev_id;
	/* u32 base = i2c->base; */

	I2C_INFO(i2c, I2C_T_TRANSFERFLOW, "i2c interrupt coming.....\n");

	/*Clear interrupt mask */
	i2c_writew(i2c, OFFSET_INTR_MASK,
		i2c_readw(i2c,
			OFFSET_INTR_MASK) &
			~(I2C_HS_NACKERR | I2C_ACKERR | I2C_TRANSAC_COMP));
	/*Save interrupt status */
	i2c->irq_stat = i2c_readw(i2c, OFFSET_INTR_STAT);
	/*Clear interrupt status,write 1 clear */
	i2c_writew(i2c, OFFSET_INTR_STAT,
		(I2C_HS_NACKERR | I2C_ACKERR | I2C_TRANSAC_COMP));

	/*Wake up process */
	atomic_set(&i2c->trans_stop, 1);
	wake_up(&i2c->wait);
	return IRQ_HANDLED;
}

static int mt_i2c_copy_to_dma(struct mt_i2c *i2c, struct mt_i2c_msg *msgs)
{
	if (((msgs->len) >> 8) & 0x00FF)
		memcpy(i2c->dma_buf.vaddr, (msgs->buf),
			(msgs->len & 0x00FF)*((msgs->len >> 8) & 0x00FF));
	else
		memcpy(i2c->dma_buf.vaddr, (msgs->buf), (msgs->len & 0x00FF));
	msgs->buf = (u8 *) i2c->dma_buf.paddr;
	return 0;
}

static int mt_i2c_copy_from_dma(
	struct mt_i2c *i2c, struct mt_i2c_msg *msg,
	unsigned char *temp_buf)
{
	if (msg->ext_flag & I2C_WR_FLAG)
		memcpy(temp_buf, i2c->dma_buf.vaddr, (msg->len >> 8) & 0xFF);
	else if (msg->flags & I2C_M_RD) {
		if (((msg->len) >> 8) & 0x00FF)
			memcpy(temp_buf, i2c->dma_buf.vaddr,
				(msg->len & 0xFF)*((msg->len>>8) & 0xFF));
		else
			memcpy(temp_buf, i2c->dma_buf.vaddr, (msg->len & 0xFF));
	}
	msg->buf = temp_buf;
	return 0;
}

static bool mt_i2c_should_combine(struct mt_i2c *i2c, struct mt_i2c_msg *msg)
{
	u16 *p_addr = i2c->pdata->need_wrrd;
	struct mt_i2c_msg *next_msg = msg + 1;
	if (p_addr
		&& (next_msg->len & 0xFF) < 32
		&& msg->addr == next_msg->addr
		&& !(msg->flags & I2C_M_RD)
		&& (next_msg->flags & I2C_M_RD) == I2C_M_RD) {
		u16 addr;
		while ((addr = *p_addr++)) {
			if (addr == msg->addr)
				return true;
		}
	}
	return false;
}

static int mt_i2c_do_transfer(
	struct mt_i2c *i2c, struct mt_i2c_msg *msgs, int num)
{
	int ret = 0;
	int left_num = num;
	struct mt_i2c_msg *msg = msgs;

	while (left_num--) {
		u8 *temp_for_dma = 0;
		u8 *temp_combine = 0;
		bool dma_need_copy_back = false;
		bool combined = false;
		bool restore = false;
		struct mt_i2c_msg *next_msg = msg + 1;

		if (left_num > 0 && mt_i2c_should_combine(i2c, msg)) {
			if (((next_msg->len) & 0xFF) >= ((msg->len) & 0xFF)) {
				memcpy(next_msg->buf, msg->buf, msg->len);
			} else {
				restore = true;
				temp_combine = next_msg->buf;
				next_msg->buf = msg->buf;
			}
			next_msg->flags &= ~I2C_M_RD;
			next_msg->ext_flag |= I2C_WR_FLAG | I2C_RS_FLAG;
			next_msg->len = (next_msg->len << 8) | msg->len;

			msg = next_msg;
			left_num--;
			combined = true;
		}
		if ((!(msg->ext_flag & I2C_DMA_FLAG))
			&& (((msg->len & 0xFF) > 8)
			|| (((msg->len >> 8) & 0xFF) > 8)
			|| combined)) {
			dma_need_copy_back = true;
			msg->ext_flag |= I2C_DMA_FLAG;
			temp_for_dma = msg->buf;
			mt_i2c_copy_to_dma(i2c, msg);
		}

		ret = mt_i2c_start_xfer(i2c, msg);
		if (ret < 0)
			return ret == -EINVAL ? ret : -EAGAIN;

		if (dma_need_copy_back) {
			mt_i2c_copy_from_dma(i2c, msg, temp_for_dma);
			msg->ext_flag &= ~I2C_DMA_FLAG;
		}
		if (restore) {
			memcpy(temp_combine, msgs->buf, (msg->len >> 8) & 0xFF);
			msg->buf = temp_combine;
		}

		msg++;
	}
	return num;
}

int mt_i2c_transfer(struct i2c_adapter *adap, struct mt_i2c_msg msgs[], int num)
{
	int ret = 0;
	int retry;
	struct mt_i2c *i2c = i2c_get_adapdata(adap);
	for (retry = 0; retry < adap->retries; retry++) {
		ret = mt_i2c_do_transfer(i2c, msgs, num);
		if (ret != -EAGAIN)
			break;
		if (retry < adap->retries - 1)
			udelay(100);
	}
	return (ret != -EAGAIN) ? ret : -EREMOTEIO;
}

static int mt_i2c_transfer_standard(
	struct i2c_adapter *adap, struct i2c_msg msgs[], int num)
{
	int i = 0, j = 0;
	int rc = 0;
	struct mt_i2c_msg msg_ext[num*2];

	for (i = 0, j = 0; i < num; i++) {
		/*expand the msg to mt_msg */
		int nseq = msgs[i].len / 0xFF;
		const int last_seq = msgs[i].len % 0xFF;

		if (nseq > 0 && nseq <= 252 && (msgs[i].len <= PAGE_SIZE)) {
			if (unlikely(j >= ARRAY_SIZE(msg_ext)))
				goto mt_i2c_transfer_standard_return;

			msg_ext[j].addr = msgs[i].addr;
			msg_ext[j].flags = msgs[i].flags;
			msg_ext[j].ext_flag = 0;

			msg_ext[j].len = ((nseq << 8) | 0xFF);
			msg_ext[j].buf = msgs[i].buf;
			j++;
		} else if (nseq > 252 || (msgs[i].len > PAGE_SIZE)) {
			goto mt_i2c_transfer_standard_return;
		}

		if (last_seq > 0) {
			if (unlikely(j >= ARRAY_SIZE(msg_ext)))
				goto mt_i2c_transfer_standard_return;

			msg_ext[j].addr = msgs[i].addr;
			msg_ext[j].flags = msgs[i].flags;
			msg_ext[j].ext_flag = 0;

			msg_ext[j].len = last_seq;
			msg_ext[j].buf = msgs[i].buf + (nseq * 0xFF);
			j++;
		}
	}

	rc = mt_i2c_transfer(adap, msg_ext, j);
	if (likely(rc > 0))
		rc = num;

mt_i2c_transfer_standard_return:
	return rc;
}

/**
 * mt_i2c_master_send - issue a single I2C message in master transmit mode
 * @client: Handle to slave device
 * @buf: Data that will be written to the slave
 * @count: How many bytes to write, must be less than 64k since msg.len is u16
 * @ext_flag: Controller special flags.
 *
 * Returns negative errno, or else the number of bytes written.
 */
int mt_i2c_master_send(const struct i2c_client *client,
	const char *buf, int count, u32 ext_flag)
{
	int ret;
	struct i2c_adapter *adap = client->adapter;
	struct mt_i2c_msg msg;

	msg.addr = client->addr;
	msg.flags = client->flags & I2C_M_TEN;
	msg.len = count;
	msg.buf = (char *)buf;
	msg.ext_flag = ext_flag;
	ret = mt_i2c_transfer(adap, &msg, 1);

	/*
	 * If everything went ok (i.e. 1 msg transmitted), return #bytes
	 * transmitted, else error code.
	 */
	return (ret == 1) ? count : ret;
}
EXPORT_SYMBOL(mt_i2c_master_send);

/**
 * i2c_master_recv - issue a single I2C message in master receive mode
 * @client: Handle to slave device
 * @buf: Where to store data read from slave
 * @count: How many bytes to read, must be less than 64k since msg.len is u16
 * @ext_flag: Controller special flags
 *
 * Returns negative errno, or else the number of bytes read.
 */
int mt_i2c_master_recv(const struct i2c_client *client,
	char *buf, int count, u32 ext_flag)
{
	struct i2c_adapter *adap = client->adapter;
	struct mt_i2c_msg msg;
	int ret;

	msg.addr = client->addr;
	msg.flags = client->flags & I2C_M_TEN;
	msg.flags |= I2C_M_RD;
	msg.len = count;
	msg.buf = buf;
	msg.ext_flag = ext_flag;
	ret = mt_i2c_transfer(adap, &msg, 1);

	/*
	 * If everything went ok (i.e. 1 msg received), return #bytes received,
	 * else error code.
	 */
	return (ret == 1) ? count : ret;
}
EXPORT_SYMBOL(mt_i2c_master_recv);


/*This function is only for 3d camera*/

int mt_wait4_i2c_complete(void)
{
	struct mt_i2c *i2c0 = g_i2c[0];
	struct mt_i2c *i2c1 = g_i2c[1];
	int result0, result1;
	int ret = 0;

	if ((i2c0 == NULL) || (i2c1 == NULL)) {
		/*What's wrong? */
		ret = -EINVAL;
		goto end;
	}

	result0 = mt_deal_result(i2c0, &g_msg[0]);
	result1 = mt_deal_result(i2c1, &g_msg[1]);

	if (result0 < 0 || result1 < 0)
		ret = -EINVAL;

end:
	g_i2c[0] = NULL;
	g_i2c[1] = NULL;

	g_msg[0].buf = NULL;
	g_msg[1].buf = NULL;

	return ret;
}
EXPORT_SYMBOL(mt_wait4_i2c_complete);

static u32 mt_i2c_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_10BIT_ADDR | I2C_FUNC_SMBUS_EMUL;
}

static struct i2c_algorithm mt_i2c_algorithm = {
	.master_xfer = mt_i2c_transfer_standard,
	.functionality = mt_i2c_functionality,
};

static inline void mt_i2c_init_hw(struct mt_i2c *i2c)
{
	struct mt_i2c_data *pdata = dev_get_platdata(i2c->adap.dev.parent);
	i2c_writew(i2c, OFFSET_SOFTRESET, 0x0001);
	pdata->enable_clk(pdata, false);
}

static void mt_i2c_free(struct mt_i2c *i2c)
{
	if (!i2c)
		return;
	free_i2c_dma_bufs(i2c);
	free_irq(i2c->irqnr, i2c);
	i2c_del_adapter(&i2c->adap);
	kfree(i2c);
}

static int mt_i2c_probe(struct platform_device *pdev)
{
	int ret, irq;
	struct mt_i2c *i2c = NULL;
	struct resource *res;
	struct mt_i2c_data *pdata = dev_get_platdata(&pdev->dev);

	/* Request platform_device IO resource */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (res == NULL || irq < 0)
		return -ENODEV;

	/* Request IO memory */
	if (!request_mem_region(res->start, resource_size(res), pdev->name))
		return -ENOMEM;
	i2c = kzalloc(sizeof(struct mt_i2c), GFP_KERNEL);

	if (NULL == (i2c))
		return -ENOMEM;

	/* initialize mt_i2c structure */
	i2c->id = pdev->id;
	i2c->base = res->start;
	/* i2c->base     = 0x11011000; */
	i2c->irqnr = irq;
#if (defined(CONFIG_MT_I2C_FPGA_ENABLE))
	i2c->clk = I2C_CLK_RATE;
#else
	i2c->clk = pdata->get_func_clk(pdata);
	/* FIX me if clock manager add this Macro */
	i2c->pdn = pdata->pdn;
#endif
	i2c->dev = &i2c->adap.dev;

	i2c->adap.dev.parent = &pdev->dev;
	i2c->adap.nr = i2c->id;
	i2c->adap.owner = THIS_MODULE;
	i2c->adap.algo = &mt_i2c_algorithm;
	i2c->adap.algo_data = NULL;
	i2c->adap.timeout = 2 * HZ;	/*2s */
	i2c->adap.retries = 1;	/*DO NOT TRY */
	i2c->pdata = pdata;
	i2c->dma_buf.vaddr =
		dma_alloc_coherent(NULL, PAGE_SIZE,
			  &i2c->dma_buf.paddr, GFP_KERNEL);
	snprintf(i2c->adap.name, sizeof(i2c->adap.name), I2C_DRV_NAME);

	i2c->pdmabase = AP_DMA_BASE + 0x300 + (0x80 * (i2c->id));
	i2c->speed = pdata->speed;
	platform_set_drvdata(pdev, i2c);

	spin_lock_init(&i2c->lock);
	init_waitqueue_head(&i2c->wait);

	pm_runtime_set_autosuspend_delay(&pdev->dev, MT_I2C_PM_TIMEOUT);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	ret = request_irq(irq, mt_i2c_irq, IRQF_TRIGGER_LOW, I2C_DRV_NAME, i2c);
	if (ret) {
		dev_err(&pdev->dev, "failed to request I2C IRQ %d; err=%d\n", irq, ret);
		goto free;
	}

	mt_i2c_init_hw(i2c);
	i2c_set_adapdata(&i2c->adap, i2c);
	ret = i2c_add_numbered_adapter(&i2c->adap);
	if (ret) {
		dev_err(&pdev->dev, "failed to add I2C bus to I2C core; err=%d\n", ret);
		goto free1;
	}

#ifdef I2C_DEBUG
	ret = device_create_file(i2c->dev, &dev_attr_debug);
#endif

	return ret;

free1:
	free_irq(irq, i2c);
free:
	pm_runtime_disable(&pdev->dev);
	mt_i2c_free(i2c);
	return ret;
}

static int mt_i2c_remove(struct platform_device *pdev)
{
	struct mt_i2c *i2c = platform_get_drvdata(pdev);
	if (i2c) {
		free_irq(i2c->irqnr, i2c);
		platform_set_drvdata(pdev, NULL);
		pm_runtime_disable(&pdev->dev);
		mt_i2c_free(i2c);
	}
	return 0;
}

static int mt_i2c_suspend(struct device *dev)
{
	struct mt_i2c *i2c = dev_get_drvdata(dev);
	i2c->suspended = true;
	pm_runtime_get_sync(dev);
	pm_runtime_put_sync(dev);
	return 0;
}

static int mt_i2c_resume(struct device *dev)
{
	struct mt_i2c *i2c = dev_get_drvdata(dev);
	pm_runtime_get_sync(dev);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);
	i2c->suspended = false;
	return 0;
}

static struct platform_driver mt_i2c_driver = {
	.probe = mt_i2c_probe,
	.remove = mt_i2c_remove,
	.driver = {
		.name = I2C_DRV_NAME,
		.owner = THIS_MODULE,
		.pm	= &(const struct dev_pm_ops){
			SET_RUNTIME_PM_OPS(mt_i2c_runtime_suspend, mt_i2c_runtime_resume, NULL)
			SET_SYSTEM_SLEEP_PM_OPS(mt_i2c_suspend, mt_i2c_resume)
		},
	},
};

module_platform_driver(mt_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek I2C Bus Driver");
MODULE_AUTHOR("Infinity Chen <infinity.chen@mediatek.com>");
