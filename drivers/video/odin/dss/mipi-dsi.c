/*
 * linux/drivers/video/odin/dss/dsi.c
 *
 * Copyright (C) 2012 LG Electronics
 * Author:
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define DSS_SUBSYS_NAME "DSI"

#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
#include <linux/export.h>
#endif
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/hardirq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/odin_dfs.h>

#include <video/odindss.h>

#include "du.h"
#include "dipc.h"
#include "dss.h"

#include "../display/panel_device.h"

#include "mipi-dsi-regs.h"
#include "mipi-dsi.h"
#include "linux/ktime.h"
#include "mipi-dsi-set.h"

#ifdef MIPI_PKT_SIZE_CONTROL
#define MIPI_924MHZ_MODE
#else
#define MIPI_888MHZ_MODE
#endif

#define MAX_DSI_DEVICE	2
#define DEBUG 1
#define LCD_MIPI_VSYNC 54

static DEFINE_MUTEX(mtx);
static spinlock_t	vsync_ctrl_lock;

typedef void (*mipi_dsi_vsync_isr_t) (void *arg);
struct mipi_dsi_isr_data {
	mipi_dsi_vsync_isr_t 	isr;
	void			*arg;
	bool			complete_valid;
	spinlock_t		completion_lock;
};

#define VSYNC_MAX_NR_ISRS		4

static struct {
	struct platform_device *pdev;
	struct resource *iomem[MAX_DSI_DEVICE];
	void *base[MAX_DSI_DEVICE];
	int vsync_gpio;
	const char *vsync_gpio_name;
	struct mipi_dsi_isr_data registered_isr[VSYNC_MAX_NR_ISRS];
	int irq;
	unsigned long irq_flags;
	spinlock_t irq_lock;
	spinlock_t vsync_irq_lock;
	bool irq_enabled;
	struct completion framedone_completion;
	bool updating;
	ktime_t vsync_time;
#ifdef DSS_INTERRUPT_COMMAND_SEND
	int pkt_state;
	bool first_booting;
#endif
	ktime_t update_start;
	enum mipi_dsi_state dsi_state;
	bool first_pkt_timeout;
} dsi_dev;

static inline void dsi_write_reg(
	enum odin_mipi_dsi_index resource_index, u32 addr, u32 val)
{
	__raw_writel(val, dsi_dev.base[resource_index] + addr);
}

static inline u32 dsi_read_reg(
	enum odin_mipi_dsi_index resource_index, u32 addr)
{
	return __raw_readl(dsi_dev.base[resource_index] + addr);
}

static inline u32 dsi_hs_txfifo_addr(enum odin_mipi_dsi_index resource_index)
{
	return ((u32)(dsi_dev.base[resource_index] + 0x1000));
}

static inline u32 dsi_hs_rxfifo_addr(enum odin_mipi_dsi_index resource_index)
{
	return ((u32)(dsi_dev.base[resource_index] + 0x2000));
}

#define DSI_REG_MASK(idx) \
	(((1 << ((DSI_##idx##_START) - (DSI_##idx##_END) + 1)) - 1)\
	<< (DSI_##idx##_END))
#define DSI_REG_VAL(val, idx) ((val) << (DSI_##idx##_END))
#define DSI_REG_GET(val, idx) (((val) & DSI_REG_MASK(idx)) >> (DSI_##idx##_END))
#define DSI_REG_MOD(orig, val, idx) \
	(((orig) & ~DSI_REG_MASK(idx)) | DSI_REG_VAL(val, idx))

enum dsi_cfg0_id{
	ID_DSI_CON0_RX_LONG_ENA0		= (31),
	ID_DSI_CON0_RX_LONG_ENA1		= (30),
	ID_DSI_CON0_RX_LONG_ENA2		= (29),
	ID_DSI_CON0_RX_LONG_ENA3		= (28),
	ID_DSI_CON0_TURNDISABLE3		= (23),
	ID_DSI_CON0_TURNDISABLE2		= (22),
	ID_DSI_CON0_TURNDISABLE1		= (21),
	ID_DSI_CON0_TURNDISABLE0		= (20),
	ID_DSI_CON0_PHY_DATA_LANE_SWAP3		= (19),
	ID_DSI_CON0_PHY_DATA_LANE_ENA3		= (18),
	ID_DSI_CON0_PHY_DATA_LANE_SWAP2		= (17),
	ID_DSI_CON0_PHY_DATA_LANE_ENA2		= (16),
	ID_DSI_CON0_PHY_DATA_LANE_SWAP1		= (15),
	ID_DSI_CON0_PHY_DATA_LANE_ENA1		= (14),
	ID_DSI_CON0_PHY_DATA_LANE_SWAP0		= (13),
	ID_DSI_CON0_PHY_DATA_LANE_ENA0		= (12),
	ID_DSI_CON0_PHY_CLK_LANE_SWAP		= (11),
	ID_DSI_CON0_PHY_CLK_LANE_ENA		= (10),
	ID_DSI_CON0_PHY_RESETN			= (9),
	ID_DSI_CON0_DATAPATH_MODE		= (7),
	ID_DSI_CON0_LONG_PACKET_ESCAPE		= (6),
	ID_DSI_CON0_SWAP_INPUT			= (5),
	ID_DSI_CON0_OUTPUT_FORMAT		= (2),
	ID_DSI_CON0_INPUT_FORMAT		= (0),
};

enum dsi_cfg1_id{
	ID_DSI_CON1_TX_DATA_ESC1_WEN		= (28),
	ID_DSI_CON1_TX_DATA_ESC1_WDATA		= (20),
	ID_DSI_CON1_TX_DATA_ESC0_WEN		= (16),
	ID_DSI_CON1_TX_DATA_ESC0_WDATA		= (8),
	ID_DSI_CON1_HS_LANE_SEL			= (4),
	ID_DSI_CON1_LP_LANE_SEL			= (0),
};

enum dsi_cfg2_id{
	ID_DSI_CON2_TX_DATA_ESC3_WEN		= (28),
	ID_DSI_CON2_TX_DATA_ESC3_WDATA		= (20),
	ID_DSI_CON2_TX_DATA_ESC2_WEN		= (16),
	ID_DSI_CON2_TX_DATA_ESC2_WDATA		= (8),
	ID_DSI_CON2_PDN_TX_DATA3		= (6),
	ID_DSI_CON2_PDN_TX_DATA2		= (5),
	ID_DSI_CON2_PDN_TX_CLK1			= (4),
	ID_DSI_CON2_PDN_TX_DATA1		= (2),
	ID_DSI_CON2_PDN_TX_DATA0		= (1),
	ID_DSI_CON2_PDN_TX_CLK0			= (0),
};

enum dsi_hs_packet_id{
	ID_DSI_HSPACKET_PACKET_TYPE		= (31),
	ID_DSI_HSPACKET_MODE			= (30),
	ID_DSI_HSPACKET_ENABLE_EOTP		= (29),
	ID_DSI_HSPACKET_HS_ENTER_BTA_SKIP	= (24),
	ID_DSI_HSPACKET_DATA			= (8),
	ID_DSI_HSPACKET_DATA_ID			= (0),
};

enum dsi_action_id{
	ID_DSI_ACTION_PACKET_HEADER_CLR0	= (31),
	ID_DSI_ACTION_PACKET_HEADER_CLR1	= (30),
	ID_DSI_ACTION_PACKET_HEADER_CLR2	= (29),
	ID_DSI_ACTION_PACKET_HEADER_CLR3	= (28),
	ID_DSI_ACTION_FLUSH_RX_FIFO3		= (14),
	ID_DSI_ACTION_FLUSH_RX_FIFO2		= (13),
	ID_DSI_ACTION_FLUSH_RX_FIFO1		= (12),
	ID_DSI_ACTION_STOP_VIDEO_DATA		= (11),
	ID_DSI_ACTION_FLUSH_DATA_FIFO		= (10),
	ID_DSI_ACTION_FLUSH_CFG_FIFO		= (9),
	ID_DSI_ACTION_FLUSH_RX_FIFO0		= (8),
	ID_DSI_ACTION_TURN_REQUEST		= (7),
	ID_DSI_ACTION_FORCE_DATA_LANE_STOP	= (6),
	ID_DSI_ACTION_DATA_LANE_EXIT_ULP	= (5),
	ID_DSI_ACTION_DATA_LANE_ENTER_ULP	= (4),
	ID_DSI_ACTION_CLK_LANE_EXIT_HS		= (3),
	ID_DSI_ACTION_CLK_LANE_ENTER_HS		= (2),
	ID_DSI_ACTION_CLK_LANE_EXIT_ULP		= (1),
	ID_DSI_ACTION_CLK_LANE_ENTER_ULP	= (0),
};

enum dsi_command_cfg_id{
	ID_DSI_CMD_CFG_HS_CMD_FORCE_DATA_EN	= (28),
	ID_DSI_CMD_CFG_HS_CMD_FORCE_DATA	= (20),
	ID_DSI_CMD_CFG_WAIT_FIFO		= (16),
	ID_DSI_CMD_CFG_NUM_BYTES		= (0),

};

enum dsi_video_cfg1_id{
	ID_DSI_VIDEO_CONFIG1_FULL_SYNC		= (31),
	ID_DSI_VIDEO_CONFIG1_NUM_VFP_LINES	= (16),
	ID_DSI_VIDEO_CONFIG1_NUM_VBP_LINES	= (8),
	ID_DSI_VIDEO_CONFIG1_NUM_VSA_LINES	= (0),
};

enum dsi_phy_reset {
	MIPI_DSI_RESET_ENA = 0,
	MIPI_DSI_RESET_DIS,
};

#define	ID_DSI_PHY_DIRECTION0		(0x01 << 0)
#define	ID_DSI_RX_ULP_ESC0		(0x01 << 1)
#define	ID_DSI_ESC_READY		(0x01 << 2)
#define	ID_DSI_HS_DATA_READY		(0x01 << 3)
#define	ID_DSI_CFG_READY		(0x01 << 4)
#define	ID_DSI_PHY_CLK_STATUS		(0x03 << 5)
#define	ID_DSI_PHY_DATA_STATUS		(0x03 << 7)
#define	ID_DSI_HS_DATA_STATE_MACHINE	(0x1F << 9)
#define	ID_DSI_TX_STOP_DATA0		(0x01 << 14)
#define	ID_DSI_LOCAL_RX_LONG_ENA0	(0x01 << 15)
#define	ID_DSI_TX_STOP_DATA1		(0x01 << 16)
#define	ID_DSI_PHY_DIRECTION1		(0x01 << 17)
#define	ID_DSI_RX_ULP_ESC1		(0x01 << 18)
#define	ID_DSI_LOCAL_RX_LONG_ENA1	(0x01 << 19)
#define	ID_DSI_TX_STOP_DATA2		(0x01 << 22)
#define	ID_DSI_PHY_DIRECTION2		(0x01 << 23)
#define	ID_DSI_RX_ULP_ESC2		(0x01 << 24)
#define	ID_DSI_RX_LONG_ENA2		(0x01 << 25)
#define	ID_DSI_PHY_DIRECTION3		(0x01 << 26)
#define	ID_DSI_RX_ULP_ESC3		(0x01 << 27)
#define	ID_DSI_RX_LONG_ENA3		(0x01 << 28)
#define	ID_DSI_TX_STOP_DATA3		(0x01 << 30)
#define	ID_DSI_PLL_LOCK			(0x01 << 31)

u32 find_ls_bit(u32 value)
{
	int i = 0;
	while (!(value&(1<<i)))
		i++;
	return i;
}

#if 0
static struct {

	enum odin_mipi_dsi_lane_status lane_status[2];
	enum odin_mipi_dsi_lane_num	  lane_num;
}lane_cfg;
#endif

static void _mipi_dsi_int0_clear_all_irqs(
	enum odin_mipi_dsi_index resource_index)
{
	dsi_write_reg(resource_index, ADDR_DSI_INTERRUPT_CLEAR0, 0xFFFFFFFF);
}

static void _mipi_dsi_int0_mask_all_irqs(
	enum odin_mipi_dsi_index resource_index)
{
	dsi_write_reg(resource_index, ADDR_DSI_INTERRUPT_MASK0, 0xFFFFFFFF);
}

static void _mipi_dsi_int0_clear_irq(
	enum odin_mipi_dsi_index resource_index,
	enum dsi_int_src0_index src0_ix)
{
	dsi_write_reg(resource_index, ADDR_DSI_INTERRUPT_CLEAR0, src0_ix);
}

static void _mipi_dsi_int0_mask_irq(
	enum odin_mipi_dsi_index resource_index,
	enum dsi_int_src0_index src0_ix)
{
	u32 reg;
	reg = dsi_read_reg(resource_index, ADDR_DSI_INTERRUPT_MASK0);
	reg |= src0_ix;
	dsi_write_reg(resource_index, ADDR_DSI_INTERRUPT_MASK0, reg);
}

static u32 _mipi_dsi_int0_get_mask_irq(
	enum odin_mipi_dsi_index resource_index)
{
	u32 reg;
	reg = dsi_read_reg(resource_index, ADDR_DSI_INTERRUPT_MASK0);
	return reg;
}

void _mipi_dsi_int0_unmask_irq(
	enum odin_mipi_dsi_index resource_index, u32 src0_ix)
{
	u32 reg;
	reg = dsi_read_reg(resource_index, ADDR_DSI_INTERRUPT_MASK0);
	reg &= ~(src0_ix);
	dsi_write_reg(resource_index, ADDR_DSI_INTERRUPT_MASK0, reg);
}

static u32 _mipi_dsi_int0_check_irq(
	enum odin_mipi_dsi_index resource_index,
	enum dsi_int_src0_index src0_index)
{
	u32 reg;
	reg = dsi_read_reg(resource_index, ADDR_DSI_INTERRUPT_SOURCE0);
	return (reg & src0_index);
}

static u32 _mipi_dsi_int0_get_irq(
	enum odin_mipi_dsi_index resource_index)
{
	u32 reg;
	reg = dsi_read_reg(resource_index, ADDR_DSI_INTERRUPT_SOURCE0);
	return reg;
}

static void _mipi_dsi_int1_clear_all_irqs(
	enum odin_mipi_dsi_index resource_index)
{
	dsi_write_reg(resource_index, ADDR_DSI_INTERRUPT_CLEAR1, 0x7FFFFF);
}

static void _mipi_dsi_int1_mask_all_irqs(
	enum odin_mipi_dsi_index resource_index)
{
	dsi_write_reg(resource_index, ADDR_DSI_INTERRUPT_MASK1, 0x7FFFFF);
}

#if 0
static void _mipi_dsi_int1_clear_irq(
	enum odin_mipi_dsi_index resource_index,
	enum dsi_int_src1_index src1_ix)
{
	u32 reg = 0;
	reg = src1_ix >> find_ls_bit(src1_ix);
	dsi_write_reg(resource_index, ADDR_DSI_INTERRUPT_CLEAR1, reg);
}

static void _mipi_dsi_int1_mask_irq(
	enum odin_mipi_dsi_index resource_index,
	enum dsi_int_src1_index src1_ix)
{
	u32 reg;
	reg = dsi_read_reg(resource_index, ADDR_DSI_INTERRUPT_MASK1);
	reg = src1_ix >> find_ls_bit(src1_ix);
	dsi_write_reg(resource_index, ADDR_DSI_INTERRUPT_MASK1, reg);
}

static void _mipi_dsi_int1_unmask_irq(
	enum odin_mipi_dsi_index resource_index,
	enum dsi_int_src1_index src1_ix)
{
	u32 reg;
	reg = dsi_read_reg(resource_index, ADDR_DSI_INTERRUPT_MASK1);
	reg &= ~src1_ix;
	dsi_write_reg(resource_index, ADDR_DSI_INTERRUPT_MASK1, reg);
}

static u32 _mipi_dsi_int1_check_irq(
	enum odin_mipi_dsi_index resource_index,
	enum dsi_int_src1_index src1_num)
{
	u32 reg;
	reg = dsi_read_reg(resource_index, ADDR_DSI_INTERRUPT_SOURCE1);
	return !!(reg & src1_num);
}
#endif

static void _mipi_dsi_int2_clear_all_irqs(
	enum odin_mipi_dsi_index resource_index)
{
	dsi_write_reg(resource_index, ADDR_DSI_INTERRUPT_CLEAR2, 0xFFFF);
}

static void _mipi_dsi_int2_mask_all_irqs(
	enum odin_mipi_dsi_index resource_index)
{
	dsi_write_reg(resource_index, ADDR_DSI_INTERRUPT_MASK2, 0xFFFF);
}

#if 0
static void _mipi_dsi_int2_clear_irq(
	enum odin_mipi_dsi_index resource_index,
	enum dsi_int_src2_num src2_num)
{
	u32 reg = 0;
	reg |= src2_num;
	dsi_write_reg(resource_index, ADDR_DSI_INTERRUPT_CLEAR2, reg);
}

static void _mipi_dsi_int2_mask_irq(
	enum odin_mipi_dsi_index resource_index,
	enum dsi_int_src2_num src2_num)
{
	u32 reg;
	reg = dsi_read_reg(resource_index, ADDR_DSI_INTERRUPT_MASK2);
	reg |= src2_num;
	dsi_write_reg(resource_index, ADDR_DSI_INTERRUPT_MASK2, reg);
}

static void _mipi_dsi_int2_unmask_irq(
	enum odin_mipi_dsi_index resource_index,
	enum dsi_int_src2_num src2_num)
{
	u32 reg;
	reg = dsi_read_reg(resource_index, ADDR_DSI_INTERRUPT_MASK2);
	reg &= ~src2_num;
	dsi_write_reg(resource_index, ADDR_DSI_INTERRUPT_MASK2, reg);
}

static u32 _mipi_dsi_int2_check_irq(
	enum odin_mipi_dsi_index resource_index,
	enum dsi_int_src2_num src2_num)
{
	u32 reg;
	reg = dsi_read_reg(resource_index, ADDR_DSI_INTERRUPT_SOURCE2);
	return (reg & src2_num);
}
#endif

static void _mipi_dsi_configuration0(
	enum odin_mipi_dsi_index resource_index,
	enum dsi_cfg0_id id, u8 reg_val)
{
	u32 val;

	val = dsi_read_reg(resource_index, ADDR_DSI_CONFIGURATION0);

	switch (id) {
	case ID_DSI_CON0_RX_LONG_ENA0:
		val = DSI_REG_MOD(val, reg_val, CON0_RX_LONG_ENA0);
		break;
	case ID_DSI_CON0_RX_LONG_ENA1:
		val = DSI_REG_MOD(val, reg_val, CON0_RX_LONG_ENA1);
		break;
	case ID_DSI_CON0_RX_LONG_ENA2:
		val = DSI_REG_MOD(val, reg_val, CON0_RX_LONG_ENA2);
		break;
	case ID_DSI_CON0_RX_LONG_ENA3:
		val = DSI_REG_MOD(val, reg_val, CON0_RX_LONG_ENA3);
		break;
	case ID_DSI_CON0_TURNDISABLE3:
		val = DSI_REG_MOD(val, reg_val, CON0_TURNDISABLE3);
		break;
	case ID_DSI_CON0_TURNDISABLE2:
		val = DSI_REG_MOD(val, reg_val, CON0_TURNDISABLE2);
		break;
	case ID_DSI_CON0_TURNDISABLE1:
		val = DSI_REG_MOD(val, reg_val, CON0_TURNDISABLE1);
		break;
	case ID_DSI_CON0_TURNDISABLE0:
		val = DSI_REG_MOD(val, reg_val, CON0_TURNDISABLE0);
		break;
	case ID_DSI_CON0_PHY_DATA_LANE_SWAP3:
		val = DSI_REG_MOD(val, reg_val, CON0_PHY_DATA_LANE_SWAP3);
		break;
	case ID_DSI_CON0_PHY_DATA_LANE_ENA3:
		val = DSI_REG_MOD(val, reg_val, CON0_PHY_DATA_LANE_ENA3);
		break;
	case ID_DSI_CON0_PHY_DATA_LANE_SWAP2:
		val = DSI_REG_MOD(val, reg_val, CON0_PHY_DATA_LANE_SWAP2);
		break;
	case ID_DSI_CON0_PHY_DATA_LANE_ENA2:
		val = DSI_REG_MOD(val, reg_val, CON0_PHY_DATA_LANE_ENA2);
		break;
	case ID_DSI_CON0_PHY_DATA_LANE_SWAP1:
		val = DSI_REG_MOD(val, reg_val, CON0_PHY_DATA_LANE_SWAP1);
		break;
	case ID_DSI_CON0_PHY_DATA_LANE_ENA1:
		val = DSI_REG_MOD(val, reg_val, CON0_PHY_DATA_LANE_ENA1);
		break;
	case ID_DSI_CON0_PHY_DATA_LANE_SWAP0:
		val = DSI_REG_MOD(val, reg_val, CON0_PHY_DATA_LANE_SWAP0);
		break;
	case ID_DSI_CON0_PHY_DATA_LANE_ENA0:
		val = DSI_REG_MOD(val, reg_val, CON0_PHY_DATA_LANE_ENA0);
		break;
	case ID_DSI_CON0_PHY_CLK_LANE_SWAP:
		val = DSI_REG_MOD(val, reg_val, CON0_PHY_CLK_LANE_SWAP);
		break;
	case ID_DSI_CON0_PHY_CLK_LANE_ENA:
		val = DSI_REG_MOD(val, reg_val, CON0_PHY_CLK_LANE_ENA);
		break;
	case ID_DSI_CON0_PHY_RESETN:
		val = DSI_REG_MOD(val, reg_val, CON0_PHY_RESETN);
		break;
	case ID_DSI_CON0_DATAPATH_MODE:
		val = DSI_REG_MOD(val, reg_val, CON0_DATAPATH_MODE);
		break;
	case ID_DSI_CON0_LONG_PACKET_ESCAPE:
		val = DSI_REG_MOD(val, reg_val, CON0_LONG_PACKET_ESCAPE);
		break;
	case ID_DSI_CON0_SWAP_INPUT:
		val = DSI_REG_MOD(val, reg_val, CON0_SWAP_INPUT);
		break;
	case ID_DSI_CON0_OUTPUT_FORMAT:
		val = DSI_REG_MOD(val, reg_val, CON0_OUTPUT_FORMAT);
		break;
	case ID_DSI_CON0_INPUT_FORMAT:
		val = DSI_REG_MOD(val, reg_val, CON0_INPUT_FORMAT);
		break;
	default:
		break;
	}

	dsi_write_reg(resource_index, ADDR_DSI_CONFIGURATION0, val);
}

static void mipi_dsi_cfg0_rx_long_ena(
	enum odin_mipi_dsi_index resource_index,
	enum mipi_dsi_lane_index lane,
	enum mipi_dsi_packet_type type)
{
	switch (lane)
	{
		case MIPI_DSI_INDEX_LANE0:
			_mipi_dsi_configuration0(resource_index,
						ID_DSI_CON0_RX_LONG_ENA0, type);
			break;

		case MIPI_DSI_INDEX_LANE1:
			_mipi_dsi_configuration0(resource_index,
						ID_DSI_CON0_RX_LONG_ENA1, type);
			break;

		case MIPI_DSI_INDEX_LANE2:
			_mipi_dsi_configuration0(resource_index,
						ID_DSI_CON0_RX_LONG_ENA2, type);
			break;

		case MIPI_DSI_INDEX_LANE3:
			_mipi_dsi_configuration0(resource_index,
						ID_DSI_CON0_RX_LONG_ENA3, type);
			break;

		case MIPI_DSI_INDEX_CLK:
		case MIPI_DSI_INDEX_CLK1:
		default:
			break;
	}
}

#if 0
static void mipi_dsi_cfg0_turn_disable(
			enum odin_mipi_dsi_index resource_index,
			enum mipi_dsi_lane_index lane, u8 dis)
{
	switch (lane)
	{
		case MIPI_DSI_INDEX_LANE0:
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_TURNDISABLE0, dis);
			break;

		case MIPI_DSI_INDEX_LANE1:
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_TURNDISABLE1, dis);
			break;

		case MIPI_DSI_INDEX_LANE2:
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_TURNDISABLE2, dis);
			break;

		case MIPI_DSI_INDEX_LANE3:
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_TURNDISABLE3, dis);
			break;

		case MIPI_DSI_INDEX_CLK:
		case MIPI_DSI_INDEX_CLK1:
		default:
			break;
	}
}
#endif

void mipi_dsi_cfg0_phy_reset(
	enum odin_mipi_dsi_index resource_index, u8 reset)
{
	_mipi_dsi_configuration0(resource_index, ID_DSI_CON0_PHY_RESETN, reset);
}

static void mipi_dsi_cfg0_datapath_mode(
	enum odin_mipi_dsi_index resource_index,
	enum odin_mipi_dsi_datapath_mode mode)
{
	_mipi_dsi_configuration0(resource_index, ID_DSI_CON0_DATAPATH_MODE, mode);
}

#if 0
static void mipi_dsi_cfg0_long_packet_escape(
	enum odin_mipi_dsi_index resource_index,
	enum odin_mipi_dsi_datapath_mode type)
{
	_mipi_dsi_configuration0(resource_index,
				 ID_DSI_CON0_LONG_PACKET_ESCAPE, type);
}

static void mipi_dsi_cfg0_rgb_swap_input(
	enum odin_mipi_dsi_index resource_index, u8 ena)
{
	_mipi_dsi_configuration0(resource_index, ID_DSI_CON0_SWAP_INPUT, ena);
}

static void mipi_dsi_cfg0_output_format(
	enum odin_mipi_dsi_index resource_index,
	enum odin_mipi_dsi_output_format fmt)
{
	_mipi_dsi_configuration0(resource_index, ID_DSI_CON0_OUTPUT_FORMAT, fmt);
}

static void mipi_dsi_cfg0_input_format(
	enum odin_mipi_dsi_index resource_index, enum odin_mipi_dsi_input_format fmt)
{
	_mipi_dsi_configuration0(resource_index, ID_DSI_CON0_INPUT_FORMAT, fmt);
}
#endif

static void mipi_dsi_cfg0_phy_lane_sel(
	enum odin_mipi_dsi_index resource_index, enum odin_mipi_dsi_lane_num lane_num)
{
	switch (lane_num)
	{
		case ODIN_DSI_1LANE:
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_PHY_DATA_LANE_ENA0, 1);
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_PHY_DATA_LANE_ENA1, 0);
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_PHY_DATA_LANE_ENA2, 0);
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_PHY_DATA_LANE_ENA3, 0);
			break;

		case ODIN_DSI_2LANE:
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_PHY_DATA_LANE_ENA0, 1);
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_PHY_DATA_LANE_ENA1, 1);
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_PHY_DATA_LANE_ENA2, 0);
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_PHY_DATA_LANE_ENA3, 0);
			break;

		case ODIN_DSI_4LANE:
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_PHY_DATA_LANE_ENA0, 1);
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_PHY_DATA_LANE_ENA1, 1);
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_PHY_DATA_LANE_ENA2, 1);
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_PHY_DATA_LANE_ENA3, 1);
			break;

		default:
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_PHY_DATA_LANE_ENA0, 0);
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_PHY_DATA_LANE_ENA1, 0);
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_PHY_DATA_LANE_ENA2, 0);
			_mipi_dsi_configuration0(resource_index,
					ID_DSI_CON0_PHY_DATA_LANE_ENA3, 0);

	}
}

static enum odin_mipi_dsi_lane_num mipi_dsi_get_lane_num(
	enum odin_mipi_dsi_index resource_index)
{
	u32 val;

	val = dsi_read_reg(resource_index, ADDR_DSI_CONFIGURATION0);
	if (val & (1 << ID_DSI_CON0_PHY_DATA_LANE_ENA3))
		return ODIN_DSI_4LANE;
	else if (val & (1 << ID_DSI_CON0_PHY_DATA_LANE_ENA2))
		return ODIN_DSI_3LANE;
	else if (val & (1 << ID_DSI_CON0_PHY_DATA_LANE_ENA1))
		return ODIN_DSI_2LANE;
	else if (val & (1 << ID_DSI_CON0_PHY_DATA_LANE_ENA0))
		return ODIN_DSI_1LANE;
	else
		return 0;
}

#if 0
static u32 _mipi_dsi_get_cfg0(enum odin_mipi_dsi_index resource_index)
{
	return dsi_read_reg(resource_index, ADDR_DSI_CONFIGURATION0);
}
#endif

static void _mipi_dsi_configuration1(
		enum odin_mipi_dsi_index resource_index,
		enum dsi_cfg1_id id, u8 reg_val)
{
	u32 val;

	val = dsi_read_reg(resource_index, ADDR_DSI_CONFIGURATION1);

	switch (id) {
	case ID_DSI_CON1_TX_DATA_ESC1_WEN:
		val = DSI_REG_MOD(val, reg_val, CON1_TX_DATA_ESC1_WEN);
		break;
	case ID_DSI_CON1_TX_DATA_ESC1_WDATA:
		val = DSI_REG_MOD(val, reg_val, CON1_TX_DATA_ESC1_WDATA);
		break;
	case ID_DSI_CON1_TX_DATA_ESC0_WEN:
		val = DSI_REG_MOD(val, reg_val, CON1_TX_DATA_ESC0_WEN);
		break;
	case ID_DSI_CON1_TX_DATA_ESC0_WDATA:
		val = DSI_REG_MOD(val, reg_val, CON1_TX_DATA_ESC0_WDATA);
		break;
	case ID_DSI_CON1_HS_LANE_SEL:
		val = DSI_REG_MOD(val, reg_val, CON1_HS_LANE_SEL);
		break;
	case ID_DSI_CON1_LP_LANE_SEL:
		val = DSI_REG_MOD(val, reg_val, CON1_LP_LANE_SEL);
		break;
	default:
		break;
	}

	dsi_write_reg(resource_index, ADDR_DSI_CONFIGURATION1, val);
}

static void _mipi_dsi_configuration2(
		enum odin_mipi_dsi_index resource_index,
		enum dsi_cfg2_id id, u8 reg_val)
{
	u32 val;

	val = dsi_read_reg(resource_index, ADDR_DSI_CONFIGURATION2);

	switch (id) {
	case ID_DSI_CON2_TX_DATA_ESC3_WEN:
		val = DSI_REG_MOD(val, reg_val, CON2_TX_DATA_ESC3_WEN);
		break;
	case ID_DSI_CON2_TX_DATA_ESC3_WDATA:
		val = DSI_REG_MOD(val, reg_val, CON2_TX_DATA_ESC3_WDATA);
		break;
	case ID_DSI_CON2_TX_DATA_ESC2_WEN:
		val = DSI_REG_MOD(val, reg_val, CON2_TX_DATA_ESC2_WEN);
		break;
	case ID_DSI_CON2_TX_DATA_ESC2_WDATA:
		val = DSI_REG_MOD(val, reg_val, CON2_TX_DATA_ESC2_WDATA);
		break;
	case ID_DSI_CON2_PDN_TX_DATA3:
		val = DSI_REG_MOD(val, reg_val, CON2_PDN_TX_DATA3);
		break;
	case ID_DSI_CON2_PDN_TX_DATA2:
		val = DSI_REG_MOD(val, reg_val, CON2_PDN_TX_DATA2);
		break;
	case ID_DSI_CON2_PDN_TX_CLK1:
		val = DSI_REG_MOD(val, reg_val, CON2_PDN_TX_CLK1);
		break;
	case ID_DSI_CON2_PDN_TX_DATA1:
		val = DSI_REG_MOD(val, reg_val, CON2_PDN_TX_DATA1);
		break;
	case ID_DSI_CON2_PDN_TX_DATA0:
		val = DSI_REG_MOD(val, reg_val, CON2_PDN_TX_DATA0);
		break;
	case ID_DSI_CON2_PDN_TX_CLK0:
		val = DSI_REG_MOD(val, reg_val, CON2_PDN_TX_CLK0);
		break;
	default:
		break;
	}

	dsi_write_reg(resource_index, ADDR_DSI_CONFIGURATION2, val);
}


#if 0
static void mipi_dsi_cfg_txdata_esc_wdata_ena(
		enum odin_mipi_dsi_index resource_index,
		enum mipi_dsi_lane_index lane, u8 ena)
{
	switch (lane)
	{
		case MIPI_DSI_INDEX_LANE0:
			_mipi_dsi_configuration1(
				resource_index,
				ID_DSI_CON1_TX_DATA_ESC0_WEN, ena);
			break;
		case MIPI_DSI_INDEX_LANE1:
			_mipi_dsi_configuration1(
				resource_index,
				ID_DSI_CON1_TX_DATA_ESC1_WEN, ena);
			break;
		case MIPI_DSI_INDEX_LANE2:
			_mipi_dsi_configuration2(
				resource_index,
				ID_DSI_CON2_TX_DATA_ESC2_WEN, ena);
			break;
		case MIPI_DSI_INDEX_LANE3:
			_mipi_dsi_configuration2(
				resource_index,
				ID_DSI_CON2_TX_DATA_ESC3_WEN, ena);
			break;

		case MIPI_DSI_INDEX_CLK:
		case MIPI_DSI_INDEX_CLK1:
		default:
			break;
	}
}

static void mipi_dsi_cfg_txdata_esc_wdata(
	enum odin_mipi_dsi_index resource_index,
	enum mipi_dsi_lane_index lane, u8 data)
{
	switch (lane)
	{
		case MIPI_DSI_INDEX_LANE0:
			_mipi_dsi_configuration1(resource_index,
				ID_DSI_CON1_TX_DATA_ESC0_WDATA, data);
			break;
		case MIPI_DSI_INDEX_LANE1:
			_mipi_dsi_configuration1(resource_index,
				ID_DSI_CON1_TX_DATA_ESC1_WDATA, data);
			break;
		case MIPI_DSI_INDEX_LANE2:
			_mipi_dsi_configuration2(resource_index,
				ID_DSI_CON2_TX_DATA_ESC2_WDATA, data);
			break;
		case MIPI_DSI_INDEX_LANE3:
			_mipi_dsi_configuration2(resource_index,
				ID_DSI_CON2_TX_DATA_ESC3_WDATA, data);
			break;

		case MIPI_DSI_INDEX_CLK:
		case MIPI_DSI_INDEX_CLK1:
		default:
			break;
	}
}
#endif

static void _mipi_dsi_esc_packet(
		enum odin_mipi_dsi_index resource_index,
	u8 data_id, u8 data0, u8 data1, u8 data)
{
	u32 val = 0;

	/*val = dsi_read_reg(resource_index, ADDR_DSI_ESCAPE_MODE_PACKET);*/

	val = DSI_REG_MOD(val, data_id, ESC_MODE_PACKET_DATAID);
	val = DSI_REG_MOD(val, data0, ESC_MODE_PACKET_DATA0);
	val = DSI_REG_MOD(val, data1, ESC_MODE_PACKET_DATA1);
	val = DSI_REG_MOD(val, data, ESC_MODE_PACKET_DATA);

	dsi_write_reg(resource_index, ADDR_DSI_ESCAPE_MODE_PACKET, val);
}

#if 0
static void _mipi_dsi_esc_mode_trigger(
		enum odin_mipi_dsi_index resource_index, u8 trigger)
{
	dsi_write_reg(resource_index, ADDR_DSI_ESCAPE_MODE_TRIGGER, trigger);
}
#endif

static void _mipi_dsi_action(
		enum odin_mipi_dsi_index resource_index,
		enum dsi_action_id id, u8 reg_val)
{
	u32 val;

	//val = dsi_read_reg(resource_index, ADDR_DSI_ACTION);
	val = 0;

	switch (id) {
	case ID_DSI_ACTION_PACKET_HEADER_CLR0:
		val = DSI_REG_MOD(val, reg_val, ACTION_PACKET_HEADER_CLR0);
		break;
	case ID_DSI_ACTION_PACKET_HEADER_CLR1:
		val = DSI_REG_MOD(val, reg_val, ACTION_PACKET_HEADER_CLR1);
		break;
	case ID_DSI_ACTION_PACKET_HEADER_CLR2:
		val = DSI_REG_MOD(val, reg_val, ACTION_PACKET_HEADER_CLR2);
		break;
	case ID_DSI_ACTION_PACKET_HEADER_CLR3:
		val = DSI_REG_MOD(val, reg_val, ACTION_PACKET_HEADER_CLR3);
		break;
	case ID_DSI_ACTION_FLUSH_RX_FIFO3:
		val = DSI_REG_MOD(val, reg_val, ACTION_FLUSH_RX_FIFO3);
		break;
	case ID_DSI_ACTION_FLUSH_RX_FIFO2:
		val = DSI_REG_MOD(val, reg_val, ACTION_FLUSH_RX_FIFO2);
		break;
	case ID_DSI_ACTION_FLUSH_RX_FIFO1:
		val = DSI_REG_MOD(val, reg_val, ACTION_FLUSH_RX_FIFO1);
		break;
	case ID_DSI_ACTION_STOP_VIDEO_DATA:
		val = DSI_REG_MOD(val, reg_val, ACTION_STOP_VIDEO_DATA);
		break;
	case ID_DSI_ACTION_FLUSH_DATA_FIFO:
		val = DSI_REG_MOD(val, reg_val, ACTION_FLUSH_DATA_FIFO);
		break;
	case ID_DSI_ACTION_FLUSH_CFG_FIFO:
		val = DSI_REG_MOD(val, reg_val, ACTION_FLUSH_CFG_FIFO);
		break;
	case ID_DSI_ACTION_FLUSH_RX_FIFO0:
		val = DSI_REG_MOD(val, reg_val, ACTION_FLUSH_RX_FIFO0);
		break;
	case ID_DSI_ACTION_TURN_REQUEST:
		val = DSI_REG_MOD(val, reg_val, ACTION_TURN_REQUEST);
		break;
	case ID_DSI_ACTION_FORCE_DATA_LANE_STOP:
		val = DSI_REG_MOD(val, reg_val, ACTION_FORCE_DATA_LANE_STOP);
		break;
	case ID_DSI_ACTION_DATA_LANE_EXIT_ULP:
		val = DSI_REG_MOD(val, reg_val, ACTION_DATA_LANE_EXIT_ULP);
		break;
	case ID_DSI_ACTION_DATA_LANE_ENTER_ULP:
		val = DSI_REG_MOD(val, reg_val, ACTION_DATA_LANE_ENTER_ULP);
		break;
	case ID_DSI_ACTION_CLK_LANE_EXIT_HS:
		val = DSI_REG_MOD(val, reg_val, ACTION_CLK_LANE_EXIT_HS);
		break;
	case ID_DSI_ACTION_CLK_LANE_ENTER_HS:
		val = DSI_REG_MOD(val, reg_val, ACTION_CLK_LANE_ENTER_HS);
		break;
	case ID_DSI_ACTION_CLK_LANE_EXIT_ULP:
		val = DSI_REG_MOD(val, reg_val, ACTION_CLK_LANE_EXIT_ULP);
		break;
	case ID_DSI_ACTION_CLK_LANE_ENTER_ULP:
		val = DSI_REG_MOD(val, reg_val, ACTION_CLK_LANE_ENTER_ULP);
		break;
	default:
		break;
	}

	dsi_write_reg(resource_index, ADDR_DSI_ACTION, val);
}

#if 0
static void mipi_dsi_action_rx_packet_header_clear(
	enum odin_mipi_dsi_index resource_index, enum mipi_dsi_lane_index lane)
{
	switch (lane)
	{
		case MIPI_DSI_INDEX_LANE0:
			_mipi_dsi_action(resource_index,
					 ID_DSI_ACTION_PACKET_HEADER_CLR0, 1);
			break;

		case MIPI_DSI_INDEX_LANE1:
			_mipi_dsi_action(resource_index,
					 ID_DSI_ACTION_PACKET_HEADER_CLR1, 1);
			break;

		case MIPI_DSI_INDEX_LANE2:
			_mipi_dsi_action(resource_index,
					 ID_DSI_ACTION_PACKET_HEADER_CLR2, 1);
			break;

		case MIPI_DSI_INDEX_LANE3:
			_mipi_dsi_action(resource_index,
					 ID_DSI_ACTION_PACKET_HEADER_CLR3, 1);
			break;

		case MIPI_DSI_INDEX_CLK:
		case MIPI_DSI_INDEX_CLK1:
		default:
			break;
	}
}
#endif

static void mipi_dsi_action_fifo_flush(
	enum odin_mipi_dsi_index resource_index, enum mipi_dsi_fifo_flush fifo)
{
	switch (fifo)
	{
		case MIPI_DSI_RX_FIFO_FLUSH0:
			_mipi_dsi_action(resource_index,
					 ID_DSI_ACTION_FLUSH_RX_FIFO0, 1);
			break;

		case MIPI_DSI_RX_FIFO_FLUSH1:
			_mipi_dsi_action(resource_index,
					 ID_DSI_ACTION_FLUSH_RX_FIFO1, 1);
			break;

		case MIPI_DSI_RX_FIFO_FLUSH2:
			_mipi_dsi_action(resource_index,
					 ID_DSI_ACTION_FLUSH_RX_FIFO2, 1);
			break;

		case MIPI_DSI_RX_FIFO_FLUSH3:
			_mipi_dsi_action(resource_index,
					 ID_DSI_ACTION_FLUSH_RX_FIFO3, 1);
			break;

		case MIPI_DSI_CFG_FIFO_FLUSH:
			_mipi_dsi_action(resource_index,
					 ID_DSI_ACTION_FLUSH_CFG_FIFO, 1);
			break;

		case MIPI_DSI_DATA_FIFO_FLUSH:
			_mipi_dsi_action(resource_index,
					 ID_DSI_ACTION_FLUSH_DATA_FIFO, 1);
			break;
	}
}

#if 0
static void mipi_dsi_action_stop_video_data(
	enum odin_mipi_dsi_index resource_index)
{
	_mipi_dsi_action(resource_index, ID_DSI_ACTION_STOP_VIDEO_DATA, 1);
}

static void mipi_dsi_action_force_data_lane_stop(
	enum odin_mipi_dsi_index resource_index)
{
	_mipi_dsi_action(resource_index, ID_DSI_ACTION_FORCE_DATA_LANE_STOP, 1);
}
#endif
static void mipi_action_turn_request(enum odin_mipi_dsi_index resource_index)
{
	_mipi_dsi_action(resource_index, ID_DSI_ACTION_TURN_REQUEST, 1);
}


static void mipi_dsi_action_lane_enter(
	enum odin_mipi_dsi_index resource_index, enum mipi_dsi_lane_state lane)
{
	switch (lane)
	{
		case MIPI_DSI_CLK_LANE_ULP:
			_mipi_dsi_action(resource_index,
					 ID_DSI_ACTION_CLK_LANE_ENTER_ULP, 1);
			break;

		case MIPI_DSI_CLK_LANE_HS:
			_mipi_dsi_action(resource_index,
					 ID_DSI_ACTION_CLK_LANE_ENTER_HS, 1);
			break;

		case MIPI_DSI_DATA_LANE_ULP:
			_mipi_dsi_action(resource_index,
					 ID_DSI_ACTION_DATA_LANE_ENTER_ULP, 1);
			break;

		case MIPI_DSI_DATA_LANE_LP:
		case MIPI_DSI_DATA_LANE_HS:
		default:
			break;
	}
}

static void mipi_dsi_action_lane_exit(
	enum odin_mipi_dsi_index resource_index, enum mipi_dsi_lane_state lane)
{
	switch (lane)
	{
		case MIPI_DSI_CLK_LANE_ULP:
			_mipi_dsi_action(resource_index,
					 ID_DSI_ACTION_CLK_LANE_EXIT_ULP, 1);
			break;

		case MIPI_DSI_CLK_LANE_HS:
			_mipi_dsi_action(resource_index,
					 ID_DSI_ACTION_CLK_LANE_EXIT_HS, 1);
			break;

		case MIPI_DSI_DATA_LANE_ULP:
			_mipi_dsi_action(resource_index,
					 ID_DSI_ACTION_DATA_LANE_EXIT_ULP, 1);
			break;

		case MIPI_DSI_DATA_LANE_LP:
		case MIPI_DSI_DATA_LANE_HS:
		default:
			break;

	}
}

static void _mipi_dsi_command_cfg(
	enum odin_mipi_dsi_index resource_index,
	enum dsi_command_cfg_id id, u16 reg_val)
{
	u32 val;

	val = dsi_read_reg(resource_index, ADDR_DSI_COMMAND_CONFIG);

	switch (id) {
	case ID_DSI_CMD_CFG_NUM_BYTES:
		val = DSI_REG_MOD(val, reg_val, CMD_CFG_NUM_BYTES);
		break;
	case ID_DSI_CMD_CFG_WAIT_FIFO:
		val = DSI_REG_MOD(val, reg_val, CMD_CFG_WAIT_FIFO);
		break;
	case ID_DSI_CMD_CFG_HS_CMD_FORCE_DATA:
		val = DSI_REG_MOD(val, reg_val, CMD_CFG_HS_CMD_FORCE_DATA);
		break;
	case ID_DSI_CMD_CFG_HS_CMD_FORCE_DATA_EN:
		val = DSI_REG_MOD(val, reg_val, CMD_CFG_HS_CMD_FORCE_DATA_EN);
		break;

	default:
		break;
	}

	dsi_write_reg(resource_index, ADDR_DSI_COMMAND_CONFIG, val);
}

static void _mipi_dsi_command_mode_loop_setting(
	enum odin_mipi_dsi_index resource_index, u16 enable, u16 cnt)
{
	u32 val;

	val = dsi_read_reg(resource_index, ADDR_DSI_FRAME_COMMAND);

	val = DSI_REG_MOD(val, cnt, FRAME_COMMAND_LOOP_CNT);
	val = DSI_REG_MOD(val, enable, FRAME_COMMAND_LOOP_ENABLE);

	dsi_write_reg(resource_index, ADDR_DSI_FRAME_COMMAND, val);
}

#if 0
static void _mipi_dsi_command_mode_loop_ena(
	enum odin_mipi_dsi_index resource_index, u16 enable)
{
	u32 val;

	val = dsi_read_reg(resource_index, ADDR_DSI_FRAME_COMMAND);

	val = DSI_REG_MOD(val, enable, FRAME_COMMAND_LOOP_ENABLE);

	dsi_write_reg(resource_index, ADDR_DSI_FRAME_COMMAND, val);
}
#endif

static bool _mipi_dsi_command_mode_loop_get(
	enum odin_mipi_dsi_index resource_index)
{
	u32 val;

	val = dsi_read_reg(resource_index, ADDR_DSI_FRAME_COMMAND);
	val = DSI_REG_GET(val, FRAME_COMMAND_LOOP_ENABLE);
	return (bool) val;
}

static void mipi_dsi_command_cfg_hs_num_bytes(
	enum odin_mipi_dsi_index resource_index, u16 num)
{
	_mipi_dsi_command_cfg(resource_index, ID_DSI_CMD_CFG_NUM_BYTES, num);
}

static void mipi_dsi_command_cfg_hs_wait_fifo(
	enum odin_mipi_dsi_index resource_index, u16 en)
{
	_mipi_dsi_command_cfg(resource_index, ID_DSI_CMD_CFG_WAIT_FIFO, en);
}

static void mipi_dsi_command_cfg_hs_force_data(
	enum odin_mipi_dsi_index resource_index, u16 data)
{
	_mipi_dsi_command_cfg(resource_index, ID_DSI_CMD_CFG_HS_CMD_FORCE_DATA, data);
}

static void mipi_dsi_command_cfg_hs_force_data_en(
	enum odin_mipi_dsi_index resource_index, u16 en)
{
	if (en == 1)
		_mipi_dsi_command_cfg(resource_index,
				ID_DSI_CMD_CFG_HS_CMD_FORCE_DATA_EN, 1);
	else
		_mipi_dsi_command_cfg(resource_index,
				ID_DSI_CMD_CFG_HS_CMD_FORCE_DATA_EN, 0);
}

static void _mipi_dsi_video_timming1(
	enum odin_mipi_dsi_index resource_index, u16 hsa, u16 bllp)
{
	u32 val = 0;

	val = DSI_REG_MOD(val, bllp, VIDEO_TIMMING1_BLLP_COUNT);
	val = DSI_REG_MOD(val, hsa, VIDEO_TIMMING1_HSA_COUNT);

	dsi_write_reg(resource_index, ADDR_DSI_VIDEO_TIMING1, val);
}

static void _mipi_dsi_video_timming2(
	enum odin_mipi_dsi_index resource_index, u16 hbp, u16 hfp)
{
	u32 val = 0;

	val = DSI_REG_MOD(val, hfp, VIDEO_TIMMING2_HFP_COUNT);
	val = DSI_REG_MOD(val, hbp, VIDEO_TIMMING2_HBP_COUNT);

	dsi_write_reg(resource_index, ADDR_DSI_VIDEO_TIMING2, val);
}

static void _mipi_dsi_video_config1(
	enum odin_mipi_dsi_index resource_index,
	enum dsi_video_cfg1_id id,u8 reg_val)
{
	u32 val;

	val = dsi_read_reg(resource_index, ADDR_DSI_VIDEO_CONFIG1);

	switch (id) {
	case ID_DSI_VIDEO_CONFIG1_FULL_SYNC:
		val = DSI_REG_MOD(val, reg_val, VIDEO_CONFIG1_FULL_SYNC);
		break;
	case ID_DSI_VIDEO_CONFIG1_NUM_VFP_LINES:
		val = DSI_REG_MOD(val, reg_val, VIDEO_CONFIG1_NUM_VFP_LINES);
		break;
	case ID_DSI_VIDEO_CONFIG1_NUM_VBP_LINES:
		val = DSI_REG_MOD(val, reg_val, VIDEO_CONFIG1_NUM_VBP_LINES);
		break;
	case ID_DSI_VIDEO_CONFIG1_NUM_VSA_LINES:
		val = DSI_REG_MOD(val, reg_val, VIDEO_CONFIG1_NUM_VSA_LINES);
		break;

	default:
		break;
	}

	dsi_write_reg(resource_index, ADDR_DSI_VIDEO_CONFIG1, val);
}

#if 0
static void mipi_dsi_video_config1_full_sync_ena(
	enum odin_mipi_dsi_index resource_index, u8 ena)
{
	_mipi_dsi_video_config1(resource_index,
		ID_DSI_VIDEO_CONFIG1_FULL_SYNC, ena);
}

static void mipi_dsi_video_config1_num_vfp_lines(
	enum odin_mipi_dsi_index resource_index, u8 num)
{
	_mipi_dsi_video_config1(resource_index,
		ID_DSI_VIDEO_CONFIG1_NUM_VFP_LINES, num);
}

static void mipi_dsi_video_config1_num_vbp_lines(
	enum odin_mipi_dsi_index resource_index, u8 num)
{
	_mipi_dsi_video_config1(resource_index,
		ID_DSI_VIDEO_CONFIG1_NUM_VBP_LINES, num);
}

static void mipi_dsi_video_config1_num_vsa_lines(
	enum odin_mipi_dsi_index resource_index, u8 num)
{
	_mipi_dsi_video_config1(resource_index,
		ID_DSI_VIDEO_CONFIG1_NUM_VSA_LINES, num);
}
#endif

static void _mipi_dsi_video_config2(
	enum odin_mipi_dsi_index resource_index,
	u16 num_vact_lines,u16 num_bytes_line)
{
	u32 val = 0;

	val = DSI_REG_MOD(val, num_vact_lines, VIDEO_CONFIG2_NUM_VACT_LINES);
	val = DSI_REG_MOD(val, num_bytes_line, VIDEO_CONFIG2_NUM_BYTES_LINES);

	dsi_write_reg(resource_index, ADDR_DSI_VIDEO_CONFIG2, val);
}

#if 0
static u32 _mipi_dsi_get_rx_packet_header(
	enum odin_mipi_dsi_index resource_index, enum mipi_dsi_lane_index lane)
{
	u32 val = 0;

	switch (lane) {
	case MIPI_DSI_INDEX_LANE0:
		val = dsi_read_reg(resource_index, ADDR_DSI_RX_PACKET_HEADER0);
		break;
	case MIPI_DSI_INDEX_LANE1:
		val = dsi_read_reg(resource_index, ADDR_DSI_RX_PACKET_HEADER1);
		break;
	case MIPI_DSI_INDEX_LANE2:
		val = dsi_read_reg(resource_index, ADDR_DSI_RX_PACKET_HEADER2);
		break;
	case MIPI_DSI_INDEX_LANE3:
		val = dsi_read_reg(resource_index, ADDR_DSI_RX_PACKET_HEADER3);
		break;

	case MIPI_DSI_INDEX_CLK:
	case MIPI_DSI_INDEX_CLK1:
	default:
		break;
	}

	return val;
}
#endif

static u32 _mipi_dsi_get_status(enum odin_mipi_dsi_index resource_index)
{
	u32 val;
	val = dsi_read_reg(resource_index, ADDR_DSI_STATUS);
	return val;
}

static u32 _mipi_dsi_get_fifo_byte_count(
	enum odin_mipi_dsi_index resource_index)
{
	u32 val;
	val = dsi_read_reg(resource_index, ADDR_DSI_FIFO_BYTE_COUNT);
	return val;
}

#if 0
static void _mipi_dsi_packet_payload(
	enum odin_mipi_dsi_index resource_index, u32 packet)
{
#ifdef CONFIG_ODIN_DSS_FPGA
	dsi_write_reg(resource_index, ADDR_DSI_PACKET_PAYLOAD, packet);
#endif
}
#endif

void _mipi_dsi_test_clear(enum odin_mipi_dsi_index resource_index, u32 tstclr)
{
	dsi_write_reg(resource_index, ADDR_DSI_TEST_CLEAR, tstclr); /* 0x70*/
}

static void _mipi_dsi_test_clock(
	enum odin_mipi_dsi_index resource_index, u32 tstclk)
{
	dsi_write_reg(resource_index, ADDR_DSI_TEST_CLOCK, tstclk); /* 0x74*/
}

static void _mipi_dsi_test_datain(
	enum odin_mipi_dsi_index resource_index, u32 tstDin)
{
	dsi_write_reg(resource_index, ADDR_DSI_TEST_DATA_IN, tstDin); /* 0x78*/
}

static void _mipi_dsi_test_dataen(
	enum odin_mipi_dsi_index resource_index, u32 tstDen)
{
	dsi_write_reg(resource_index, ADDR_DSI_TEST_DATA_ENABLE, tstDen); /* 0x7C*/
}

static u32 _mipi_dsi_test_dataout_get(enum odin_mipi_dsi_index resource_index)
{
	return dsi_read_reg(resource_index, ADDR_DSI_TEST_DATA_OUT); /* 0x80*/
}

void mipi_dsi_fifo_flush_all(enum odin_mipi_dsi_index resource_index)
{
	mipi_dsi_action_fifo_flush(resource_index, MIPI_DSI_RX_FIFO_FLUSH0);
	mipi_dsi_action_fifo_flush(resource_index, MIPI_DSI_CFG_FIFO_FLUSH);
	mipi_dsi_action_fifo_flush(resource_index, MIPI_DSI_DATA_FIFO_FLUSH);
}

void mipi_dsi_mask_all_irqs(enum odin_mipi_dsi_index resource_index)
{
	_mipi_dsi_int0_mask_all_irqs(resource_index);
	_mipi_dsi_int1_mask_all_irqs(resource_index);
	_mipi_dsi_int2_mask_all_irqs(resource_index);
}

void mipi_dsi_clear_all_irqs(enum odin_mipi_dsi_index resource_index)
{
	_mipi_dsi_int0_clear_all_irqs(resource_index);
	_mipi_dsi_int1_clear_all_irqs(resource_index);
	_mipi_dsi_int2_clear_all_irqs(resource_index);
}


void mipi_dsi_cfg_phy_tx_lane_power(
	enum odin_mipi_dsi_index resource_index, bool enable)
{
	_mipi_dsi_configuration2(resource_index,
				 ID_DSI_CON2_PDN_TX_CLK0, enable);
}

void mipi_dsi_sel_lanes(
	enum odin_mipi_dsi_index resource_index,
	enum odin_mipi_dsi_lane_num lane_num)
{
	switch (lane_num)
	{
		case ODIN_DSI_4LANE:
			_mipi_dsi_configuration1(resource_index,
						 ID_DSI_CON1_LP_LANE_SEL, 0xF);
			_mipi_dsi_configuration1(resource_index,
						 ID_DSI_CON1_HS_LANE_SEL, 0xF);
			break;

		case ODIN_DSI_3LANE:
			_mipi_dsi_configuration1(resource_index,
						 ID_DSI_CON1_LP_LANE_SEL, 0x7);
			_mipi_dsi_configuration1(resource_index,
						 ID_DSI_CON1_HS_LANE_SEL, 0x7);
			break;

		case ODIN_DSI_2LANE:
			_mipi_dsi_configuration1(resource_index,
						 ID_DSI_CON1_LP_LANE_SEL, 0x3);
			_mipi_dsi_configuration1(resource_index,
						 ID_DSI_CON1_HS_LANE_SEL, 0x3);
			break;

		case ODIN_DSI_1LANE:
			_mipi_dsi_configuration1(resource_index,
						 ID_DSI_CON1_LP_LANE_SEL, 0x1);
			_mipi_dsi_configuration1(resource_index,
						 ID_DSI_CON1_HS_LANE_SEL, 0x1);
			break;
	}
}

void mipi_dsi_enable_clk_data_lane(
	enum odin_mipi_dsi_index resource_index,
	enum odin_mipi_dsi_lane_num lane_num)
{
	_mipi_dsi_configuration0(resource_index,
				 ID_DSI_CON0_PHY_CLK_LANE_ENA, true);

	switch (lane_num)
	{
		case ODIN_DSI_4LANE:
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA3, true);
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA2, true);
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA1, true);
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA0, true);
			break;

		case ODIN_DSI_3LANE:
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA3, false);
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA2, true);
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA1, true);
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA0, true);
			break;

		case ODIN_DSI_2LANE:
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA3, false);
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA2, false);
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA1, true);
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA0, true);
			break;

		case ODIN_DSI_1LANE:
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA3, false);
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA2, false);
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA1, false);
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA0, true);
			break;

		default:
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA3, false);
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA2, false);
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA1, false);
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_ENA0, false);
			break;
	}
}


void mipi_dsi_lane_config(
	enum odin_mipi_dsi_index resource_index,
	enum odin_mipi_dsi_lane_num lane_num)
{
	mipi_dsi_sel_lanes(resource_index, lane_num);

#if 0
	if (lane_num == ODIN_DSI_1LANE)  /* 1lane  ==  esc mode*/
		_mipi_dsi_configuration0(resource_index,
					 ID_DSI_CON0_PHY_CLK_LANE_ENA, 0);
	else
		_mipi_dsi_configuration0(resource_index,
					 ID_DSI_CON0_PHY_CLK_LANE_ENA, 1);
#endif

	mipi_dsi_enable_clk_data_lane(resource_index, lane_num);
}

void mipi_dsi_change_lane_status(
	enum odin_mipi_dsi_index resource_index, enum odin_mipi_dsi_index status)
{
	switch (status)
	{
		case ODIN_DSI_LANE_ULP:
			mipi_dsi_action_lane_exit(resource_index,
						  MIPI_DSI_CLK_LANE_HS);
			mipi_dsi_action_lane_enter(resource_index,
						   MIPI_DSI_CLK_LANE_ULP);
			mipi_dsi_action_lane_enter(resource_index,
						   MIPI_DSI_DATA_LANE_ULP);
			break;
		case ODIN_DSI_LANE_LP:
			mipi_dsi_action_lane_exit(resource_index,
						  MIPI_DSI_CLK_LANE_HS);
			mipi_dsi_action_lane_exit(resource_index,
						  MIPI_DSI_CLK_LANE_ULP);
			mipi_dsi_action_lane_exit(resource_index,
						  MIPI_DSI_DATA_LANE_ULP);
			break;
		case ODIN_DSI_LANE_HS:
			mipi_dsi_action_lane_exit(resource_index,
						  MIPI_DSI_CLK_LANE_ULP);
			mipi_dsi_action_lane_exit(resource_index,
						  MIPI_DSI_DATA_LANE_ULP);
			mipi_dsi_action_lane_enter(resource_index,
						   MIPI_DSI_CLK_LANE_HS);
			break;
	}
}

void mipi_dsi_cfg0_phy_lane_swap(
	enum odin_mipi_dsi_index resource_index,
	enum mipi_dsi_lane_index lane, u8 swap)
{
	switch (lane)
	{
		case MIPI_DSI_INDEX_LANE0:
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_SWAP0, swap);
			break;

		case MIPI_DSI_INDEX_LANE1:
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_SWAP1, swap);
			break;

		case MIPI_DSI_INDEX_LANE2:
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_SWAP2, swap);
			break;

		case MIPI_DSI_INDEX_LANE3:
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_DATA_LANE_SWAP3, swap);
			break;

		case MIPI_DSI_INDEX_CLK:
			_mipi_dsi_configuration0(
				resource_index,
				ID_DSI_CON0_PHY_CLK_LANE_SWAP, swap);
			break;

		case MIPI_DSI_INDEX_CLK1:
		default:
			break;
	}
}


bool mipi_dsi_check_status(
	enum odin_mipi_dsi_index resource_index, enum dsi_status_index status)
{
	u32 val;
	val = _mipi_dsi_get_status(resource_index);
	return (val & status);
}

void mipi_dsi_video_mode_timming(
	enum odin_mipi_dsi_index resource_index,
	struct odin_mipi_dsi_video_timing_info *pinfo)
{
	_mipi_dsi_video_timming1(resource_index,
		pinfo->hsa_count, pinfo->bllp_count);
	_mipi_dsi_video_timming2(
		resource_index, pinfo->hbp_count, pinfo->hfp_count);

	_mipi_dsi_video_config1(
		resource_index, ID_DSI_VIDEO_CONFIG1_NUM_VSA_LINES,
		pinfo->num_vsa_lines);
	_mipi_dsi_video_config1(
		resource_index, ID_DSI_VIDEO_CONFIG1_NUM_VBP_LINES,
		pinfo->num_vbp_lines);
	_mipi_dsi_video_config1(
		resource_index, ID_DSI_VIDEO_CONFIG1_NUM_VFP_LINES,
		pinfo->num_vfp_lines);
	_mipi_dsi_video_config1(
		resource_index, ID_DSI_VIDEO_CONFIG1_FULL_SYNC,
		pinfo->full_sync);
	_mipi_dsi_video_config2(
		resource_index, pinfo->num_vact_lines,
		pinfo->num_bytes_lines);
}

void mipi_dsi_phy_lane_clk_swap(
	enum odin_mipi_dsi_index resource_index, u16 pclk_pol)
{
	u8 lane_pol;
	u8 clk_pol;

	if (pclk_pol == 1)
	{
		lane_pol = 0;
		clk_pol = 1;
	}
	else
	{
		lane_pol = 0;
		clk_pol = 0;
	}

	mipi_dsi_cfg0_phy_lane_swap(resource_index,
				    MIPI_DSI_INDEX_LANE0, lane_pol);
	mipi_dsi_cfg0_phy_lane_swap(resource_index,
				    MIPI_DSI_INDEX_LANE1, lane_pol);
	mipi_dsi_cfg0_phy_lane_swap(resource_index,
				    MIPI_DSI_INDEX_LANE2, lane_pol);
	mipi_dsi_cfg0_phy_lane_swap(resource_index,
				    MIPI_DSI_INDEX_LANE3, lane_pol);
	mipi_dsi_cfg0_phy_lane_swap(resource_index,
				    MIPI_DSI_INDEX_CLK, clk_pol);

}

void mipi_dsi_set_in_fmt(
	enum odin_mipi_dsi_index resource_index,
	enum odin_mipi_dsi_input_format in_fmt)
{
	_mipi_dsi_configuration0(resource_index,
				 ID_DSI_CON0_INPUT_FORMAT, in_fmt);
}

void mipi_dsi_set_out_fmt(
	enum odin_mipi_dsi_index resource_index,
	enum odin_mipi_dsi_output_format out_fmt)
{
	_mipi_dsi_configuration0(resource_index,
				 ID_DSI_CON0_OUTPUT_FORMAT, out_fmt);
}

void mipi_dsi_set_fmt(
	enum odin_mipi_dsi_index resource_index,
	enum odin_mipi_dsi_input_format in_fmt,
	enum odin_mipi_dsi_output_format out_fmt)
{
	_mipi_dsi_configuration0(resource_index,
				 ID_DSI_CON0_INPUT_FORMAT, in_fmt);
	_mipi_dsi_configuration0(resource_index,
				 ID_DSI_CON0_OUTPUT_FORMAT, out_fmt);
}

u32 mipi_dsi_tif_read(
	enum odin_mipi_dsi_index resource_index, u32 addr)
{
	u32 ret_val;
	/* set Test code*/
	_mipi_dsi_test_clock(resource_index, 1);
	_mipi_dsi_test_datain(resource_index, addr);
	_mipi_dsi_test_dataen(resource_index, 1);
	_mipi_dsi_test_clock(resource_index, 0);
	_mipi_dsi_test_clock(resource_index, 1);
	_mipi_dsi_test_dataen(resource_index, 0);

	ret_val = _mipi_dsi_test_dataout_get(resource_index);
	MIPI_INFO("ret_val in the read : %d \n", ret_val);

	return ret_val;

}


u32  mipi_dsi_tif_write(
	enum odin_mipi_dsi_index resource_index, u32 addr, u8 data)
{
	u8 ret_val;
	/* set Test code*/
	_mipi_dsi_test_clock(resource_index, 1);
	_mipi_dsi_test_datain(resource_index, addr);
	_mipi_dsi_test_dataen(resource_index, 1);
	_mipi_dsi_test_clock(resource_index, 0);
	_mipi_dsi_test_clock(resource_index, 1);
	_mipi_dsi_test_dataen(resource_index, 0);

	/* set Test data*/
	_mipi_dsi_test_clock(resource_index, 0);
	_mipi_dsi_test_datain(resource_index, data);
	_mipi_dsi_test_clock(resource_index, 0);
	_mipi_dsi_test_clock(resource_index, 1);

	/* set Test code(read data)*/
	_mipi_dsi_test_clock(resource_index, 1);
	_mipi_dsi_test_datain(resource_index, addr);
	_mipi_dsi_test_dataen(resource_index, 1);
	_mipi_dsi_test_clock(resource_index, 0);
	_mipi_dsi_test_clock(resource_index, 1);
	_mipi_dsi_test_dataen(resource_index, 0);

	ret_val = _mipi_dsi_test_dataout_get(resource_index);

	if (data == _mipi_dsi_test_dataout_get(resource_index))
		return 0;
	else
	{
		printk("\t-->> MIPI DSI D-PHY test I/F fail\n");
		return -1;
	}
}

void mipi_dsi_send_packet(
	enum odin_mipi_dsi_index resource_index,
	u8 data0, u8 data1, u8 data2, u8 data3)
{
	u32 cntretry = 0;

	_mipi_dsi_esc_packet(resource_index, data0, data1, data2, data3);

	while (mipi_dsi_check_status(resource_index,
				     DSI_STATUS_ESCAPE_READY) == 0)
	{
		if (cntretry >= 1000)
		{
			_mipi_dsi_int0_clear_irq(resource_index,
						 DSI_INT0_RX_ECC_ERR1);
			_mipi_dsi_int0_clear_irq(resource_index,
						 DSI_INT0_RX_ECC_ERR2);
			DSSERR(">> Error Time out wating for ESCAPE_READY-before(TimeOut:%d[us])\n",
								cntretry);
			break;
		}
		udelay(1);
		cntretry++;
	}

}

u32 mipi_dsi_send_short_packet(
	enum odin_mipi_dsi_index resource_index,
	u8 dataId, u8 data0, u8 data1)
{
	u32 result = 0;
	u32 cntretry = 0;
	enum odin_mipi_dsi_lane_num saveddatalane =
		mipi_dsi_get_lane_num(resource_index);

	mipi_dsi_sel_lanes(resource_index, ODIN_DSI_1LANE);
	mipi_dsi_cfg0_phy_lane_sel(resource_index, ODIN_DSI_1LANE);

	/* esc short packet*/
	_mipi_dsi_configuration0(resource_index,
				 ID_DSI_CON0_LONG_PACKET_ESCAPE, 0);

	mipi_dsi_send_packet(resource_index, dataId, data0, data1, 0);

	while (_mipi_dsi_int0_check_irq
		(resource_index, DSI_INT0_ESC_COMPLETE) == 0)
	{
		if (cntretry >= 1000)
		{
			DSSINFO("Short Packet, Time out for ESC_COMPLETE %d[us])\n",
				cntretry);
			result = -1;
			break;
		}
		cntretry++;
		udelay(1);
	}
	_mipi_dsi_int0_clear_irq(resource_index, DSI_INT0_ESC_COMPLETE);

	mipi_dsi_sel_lanes(resource_index, saveddatalane);
	mipi_dsi_cfg0_phy_lane_sel(resource_index, saveddatalane);

	return result;
}

u32 mipi_dsi_send_long_packet(
	enum odin_mipi_dsi_index resource_index,
	u32 dataId, u32 cntWord, char *pData)
{
	u32 result = 0;
	u32 cntretry = 0;

	/* Save phyDataLaneEn1~3*/
	enum odin_mipi_dsi_lane_num saveddatalane =
		(enum odin_mipi_dsi_lane_num)mipi_dsi_get_lane_num
					     (resource_index);

	u32 cntpacketbuild = 0;
	u32 i, data[4];

	if (cntWord <= 1)
		return -1;

	/* for esc mode*/
	mipi_dsi_sel_lanes(resource_index, ODIN_DSI_1LANE);
	mipi_dsi_cfg0_phy_lane_sel(resource_index, ODIN_DSI_1LANE);

	/* esc long packet*/
	_mipi_dsi_configuration0(resource_index,
				 ID_DSI_CON0_LONG_PACKET_ESCAPE, 1);

	mipi_dsi_send_packet(resource_index, dataId, cntWord, 0, 0);

	for (i = 0; i < cntWord; i++)
	{
		data[cntpacketbuild++] = (*pData++);

		if (cntpacketbuild%4 == 0)
		{
			mipi_dsi_send_packet(resource_index,
				(u8)data[0], (u8)data[1],
				(u8)data[2], (u8)data[3]);
			data[0] = data[1] = data[2] = data[3] = 0;
			cntpacketbuild = 0;
		}
	}

	if (cntpacketbuild)
	{
		mipi_dsi_send_packet(resource_index, data[0],
				     data[1], data[2], data[3]);
	}

	while (_mipi_dsi_int0_check_irq(
				resource_index, DSI_INT0_ESC_COMPLETE) == 0)
	{
		if (cntretry >= 1000)
		{
			DSSINFO("Long Packet, Time out for ESC_COMPLETE %d[us])\n",
				cntretry);
			result = -1;
			break;
		}
		cntretry++;
		udelay(1);
	}

	_mipi_dsi_int0_clear_irq(resource_index, DSI_INT0_ESC_COMPLETE);

	/* restore*/
	mipi_dsi_cfg0_phy_lane_sel(resource_index, saveddatalane);
	mipi_dsi_sel_lanes(resource_index, saveddatalane);

	return result;
}

void  _mipi_build_hs_package(
	enum odin_mipi_dsi_index resource_index,
	enum mipi_dsi_src_data_id dataId, u32 dataByte,
	enum odin_mipi_dsi_hs_eotp eotpEable, enum odin_mipi_dsi_hs_modes mode,
	enum mipi_dsi_packet_type packageType, bool skipBtaRequest)
{
	u32 val = 0;

	val = DSI_REG_MOD(val, dataId, HSPACKET_DATA_ID);
	val = DSI_REG_MOD(val, dataByte, HSPACKET_DATA);
	val = DSI_REG_MOD(val, eotpEable, HSPACKET_ENABLE_EOTP);
	val = DSI_REG_MOD(val, mode, HSPACKET_MODE);
	val = DSI_REG_MOD(val, packageType, HSPACKET_PACKET_TYPE);
	val = DSI_REG_MOD(val, skipBtaRequest, HSPACKET_HS_ENTER_BTA_SKIP);

	dsi_write_reg(resource_index, ADDR_DSI_HS_PACKET, val);
}

void  mipi_build_hs_package(
	enum odin_mipi_dsi_index resource_index,
	enum mipi_dsi_src_data_id dataId, u32 dataByte,
	enum odin_mipi_dsi_hs_eotp eotpEable, enum odin_mipi_dsi_hs_modes mode,
	enum mipi_dsi_packet_type packageType, bool skipBtaRequest)
{

	_mipi_build_hs_package(resource_index, dataId, dataByte,
		eotpEable, mode, packageType, skipBtaRequest);
}

void mipi_dsi_d_phy_set(struct odin_dss_device *dssdev)
{

	enum odin_mipi_dsi_index resource_index = dssdev->channel;

	mipi_dsi_cfg0_phy_reset(resource_index, MIPI_DSI_RESET_ENA); /* Enable Reset */
	_mipi_dsi_configuration2(
		resource_index, ID_DSI_CON2_PDN_TX_CLK0, false);  /* Phy Shutdown */
	mdelay(10);

    mipi_dsi_sel_lanes(resource_index, ODIN_DSI_1LANE); /* Data lane select */
	mdelay(1);

	mipi_dsi_cfg0_phy_lane_sel(resource_index, ODIN_DSI_1LANE);
	mdelay(1);

	_mipi_dsi_configuration2(
		resource_index, ID_DSI_CON2_PDN_TX_CLK0, true);  /* Release Shutdown */
	mdelay(1);

	mipi_dsi_cfg0_phy_reset(resource_index, MIPI_DSI_RESET_DIS);

	_mipi_dsi_test_clear(resource_index, 1);
	_mipi_dsi_test_clear(resource_index, 0);

	mipi_dsi_tif_write(resource_index, 0x10, 0x00);

	mipi_dsi_tif_write(resource_index, 0xF0, 0x00);
	mipi_dsi_tif_write(resource_index, 0xF1, 0x10);

#ifdef MIPI_300MHZ_MODE
	DSSINFO("MIPI Phy Clk 864 MHz \n");
	mipi_dsi_tif_write(resource_index, 0xF2, 0x02);
	mipi_dsi_tif_write(resource_index, 0xF3, 0x12);
	mipi_dsi_tif_write(resource_index, 0xF4, 0x03);
	mipi_dsi_tif_write(resource_index, 0xF5, 0x00);
	mipi_dsi_tif_write(resource_index, 0xF6, 0x21);
#endif

#ifdef MIPI_864MHZ_MODE
	DSSINFO("MIPI Phy Clk 864 MHz \n");
	mipi_dsi_tif_write(resource_index, 0xF2, 0x00);
	mipi_dsi_tif_write(resource_index, 0xF3, 0x08);
	mipi_dsi_tif_write(resource_index, 0xF4, 0x04);
	mipi_dsi_tif_write(resource_index, 0xF5, 0x00);
	mipi_dsi_tif_write(resource_index, 0xF6, 0x00);
#endif

#ifdef MIPI_876MHZ_MODE
	DSSINFO("MIPI Phy Clk 876 MHz \n");
	mipi_dsi_tif_write(resource_index, 0xF2, 0x01);
	mipi_dsi_tif_write(resource_index, 0xF3, 0x12);
	mipi_dsi_tif_write(resource_index, 0xF4, 0x01);
	mipi_dsi_tif_write(resource_index, 0xF5, 0x00);
	mipi_dsi_tif_write(resource_index, 0xF6, 0x00);
#endif

#ifdef MIPI_888MHZ_MODE
	DSSINFO("MIPI Phy Clk 888 MHz \n");
	mipi_dsi_tif_write(resource_index, 0xF2, 0x00);
	mipi_dsi_tif_write(resource_index, 0xF3, 0x08);
	mipi_dsi_tif_write(resource_index, 0xF4, 0x05);
	mipi_dsi_tif_write(resource_index, 0xF5, 0x00);
	mipi_dsi_tif_write(resource_index, 0xF6, 0x00);
#endif

#ifdef MIPI_900MHZ_MODE
	DSSINFO("MIPI Phy Clk 900 MHz \n");
	mipi_dsi_tif_write(resource_index, 0xF2, 0x01);
	mipi_dsi_tif_write(resource_index, 0xF3, 0x12);
	mipi_dsi_tif_write(resource_index, 0xF4, 0x03);
	mipi_dsi_tif_write(resource_index, 0xF5, 0x00);
	mipi_dsi_tif_write(resource_index, 0xF6, 0x00);
#endif

#ifdef MIPI_912MHZ_MODE
	DSSINFO("MIPI Phy Clk 912 MHz \n");
	mipi_dsi_tif_write(resource_index, 0xF2, 0x00);
	mipi_dsi_tif_write(resource_index, 0xF3, 0x09);
	mipi_dsi_tif_write(resource_index, 0xF4, 0x02);
	mipi_dsi_tif_write(resource_index, 0xF5, 0x00);
	mipi_dsi_tif_write(resource_index, 0xF6, 0x00);
#endif

#ifdef MIPI_924MHZ_MODE
   DSSINFO("MIPI Phy Clk 924 MHz \n");
   mipi_dsi_tif_write(resource_index, 0xF2, 0x01);
   mipi_dsi_tif_write(resource_index, 0xF3, 0x12);
   mipi_dsi_tif_write(resource_index, 0xF4, 0x07);
   mipi_dsi_tif_write(resource_index, 0xF5, 0x00);
   mipi_dsi_tif_write(resource_index, 0xF6, 0x00);
#endif

#ifdef MIPI_936MHZ_MODE
   DSSINFO("MIPI Phy Clk 936 MHz \n");
   mipi_dsi_tif_write(resource_index, 0xF2, 0x01);
   mipi_dsi_tif_write(resource_index, 0xF3, 0x12);
   mipi_dsi_tif_write(resource_index, 0xF4, 0x08);
   mipi_dsi_tif_write(resource_index, 0xF5, 0x00);
   mipi_dsi_tif_write(resource_index, 0xF6, 0x00);
#endif

#ifdef MIPI_948MHZ_MODE
   DSSINFO("MIPI Phy Clk 948 MHz \n");
   mipi_dsi_tif_write(resource_index, 0xF2, 0x01);
   mipi_dsi_tif_write(resource_index, 0xF3, 0x12);
   mipi_dsi_tif_write(resource_index, 0xF4, 0x09);
   mipi_dsi_tif_write(resource_index, 0xF5, 0x00);
   mipi_dsi_tif_write(resource_index, 0xF6, 0x00);
#endif

#ifdef MIPI_960MHZ_MODE
	DSSINFO("MIPI Phy Clk 960 MHz \n");
	mipi_dsi_tif_write(resource_index, 0xF2, 0x00);
	mipi_dsi_tif_write(resource_index, 0xF3, 0x09);
	mipi_dsi_tif_write(resource_index, 0xF4, 0x04);
	mipi_dsi_tif_write(resource_index, 0xF5, 0x00);
	mipi_dsi_tif_write(resource_index, 0xF6, 0x00);
#endif

#ifdef MIPI_880MHZ_MODE
	DSSINFO("MIPI Phy Clk 880 MHz \n");
	mipi_dsi_tif_write(resource_index, 0xF2, 0x02);
	mipi_dsi_tif_write(resource_index, 0xF3, 0x1b);
	mipi_dsi_tif_write(resource_index, 0xF4, 0x02);
	mipi_dsi_tif_write(resource_index, 0xF5, 0x00);
	mipi_dsi_tif_write(resource_index, 0xF6, 0x00);
#endif

	mipi_dsi_tif_write(resource_index, 0x34, 0x03);
	mipi_dsi_tif_write(resource_index, 0x01, 0x02);
	mipi_dsi_tif_write(resource_index, 0x34, 0x03);
	mipi_dsi_tif_write(resource_index, 0x01, 0x03);
	mipi_dsi_tif_write(resource_index, 0xF7, 0x01); /* pll cal */
	mipi_dsi_tif_write(resource_index, 0x14, 0x0f); /* THS-ZERO */
	mipi_dsi_tif_write(resource_index, 0x16, 0x10); /* THS-PREPARE */
	mipi_dsi_tif_write(resource_index, 0x20, 0x01);
	mipi_dsi_tif_write(resource_index, 0x35, 0x2a); /* clk */
	mipi_dsi_tif_write(resource_index, 0x45, 0x2a); /* ch0 */
	mipi_dsi_tif_write(resource_index, 0x55, 0x2a); /* ch1 */
	mipi_dsi_tif_write(resource_index, 0x65, 0x2a); /* ch2 */
	mipi_dsi_tif_write(resource_index, 0x75, 0x2a); /* ch3 */

	while (mipi_dsi_check_status(
			resource_index, DSI_STATUS_MIPI_PLL_LOCK) == 0 )
		mdelay(1);

	_mipi_dsi_int0_clear_all_irqs(resource_index);
    /* Now it's D-PHY LP11 state.*/
}

void mipi_dsi_d_phy_read(void)
{

	u32 i_reg_PLL_NPC = 0 , i_reg_PLL_NSC = 0;
	int i_fout = 0;
	u32 i_reg_PLL_M = 0;

	odin_crg_dss_clk_ena(CRG_CORE_CLK, (DIP0_CLK | MIPI0_CLK), true);

	i_reg_PLL_M = mipi_dsi_tif_read(0, 0xF2);
	i_reg_PLL_NPC = mipi_dsi_tif_read(0, 0xF3);
	i_reg_PLL_NSC = mipi_dsi_tif_read(0, 0xF4);

	MIPI_INFO("MIPI Phy i_reg_PLL_M is %x \n", i_reg_PLL_M);
	MIPI_INFO("MIPI Phy i_reg_PLL_NPC is %x \n", i_reg_PLL_NPC);
	MIPI_INFO("MIPI Phy i_reg_PLL_NSC is %x \n", i_reg_PLL_NSC);

	if ( i_reg_PLL_M == 0 ) {
		i_fout = ((i_reg_PLL_NPC * 4) + i_reg_PLL_NSC) * 24;
	} else if (i_reg_PLL_M == 2) {
		i_fout = ((i_reg_PLL_NPC * 4) + i_reg_PLL_NSC) * 8;
	} else {
		MIPI_INFO("MIPI Phy i_reg_PLL_M is %d \n", i_reg_PLL_M);
		i_fout = ((i_reg_PLL_NPC * 4) + i_reg_PLL_NSC) * 8;
	}

	MIPI_INFO("MIPI Phy Clk is %d MHZ \n", i_fout);

}

void mipi_dsi_d_phy_reset(struct odin_dss_device *dssdev)
{

	enum odin_mipi_dsi_index resource_index = dssdev->channel;

	mipi_dsi_cfg0_phy_reset(resource_index, MIPI_DSI_RESET_ENA); /* Enable Reset */
	_mipi_dsi_configuration2(
		resource_index, ID_DSI_CON2_PDN_TX_CLK0, false);  /* Phy Shutdown */
	mdelay(10);

    mipi_dsi_sel_lanes(resource_index, ODIN_DSI_1LANE); /* Data lane select */
	mdelay(1);

	mipi_dsi_cfg0_phy_lane_sel(resource_index, ODIN_DSI_1LANE);
	mdelay(1);

	_mipi_dsi_configuration2(
		resource_index, ID_DSI_CON2_PDN_TX_CLK0, true);  /* Release Shutdown */
	mdelay(1);

	mipi_dsi_cfg0_phy_reset(resource_index, MIPI_DSI_RESET_DIS);

	_mipi_dsi_test_clear(resource_index, 1);
	_mipi_dsi_test_clear(resource_index, 0);

	mipi_dsi_tif_write(resource_index, 0x10, 0x00);

	mipi_dsi_tif_write(resource_index, 0xF0, 0x00);
	mipi_dsi_tif_write(resource_index, 0xF1, 0x10);

	MIPI_INFO("MIPI Phy Clk Setting \n");
	mipi_dsi_tif_write(resource_index, 0xF2, 0x02);
	mipi_dsi_tif_write(resource_index, 0xF3, g_reg_PLL_NPC);
	mipi_dsi_tif_write(resource_index, 0xF4, g_reg_PLL_NSC);
	mipi_dsi_tif_write(resource_index, 0xF5, 0x00);
	mipi_dsi_tif_write(resource_index, 0xF6, 0x00);
	MIPI_INFO("MIPI Phy Clock : %d, g_reg_PLL_M : %x, g_reg_PLL_NPC : %x, g_reg_PLL_NSC : %x",g_fout, g_reg_PLL_M, g_reg_PLL_NPC, g_reg_PLL_NSC );

	mipi_dsi_tif_write(resource_index, 0x34, 0x03);
	mipi_dsi_tif_write(resource_index, 0x01, 0x02);
	mipi_dsi_tif_write(resource_index, 0x34, 0x03);
	mipi_dsi_tif_write(resource_index, 0x01, 0x03);
	mipi_dsi_tif_write(resource_index, 0xF7, 0x01); /* pll cal */
	mipi_dsi_tif_write(resource_index, 0x14, 0x0f); /* THS-ZERO */
	mipi_dsi_tif_write(resource_index, 0x16, 0x10); /* THS-PREPARE */
	mipi_dsi_tif_write(resource_index, 0x20, 0x01);
	mipi_dsi_tif_write(resource_index, 0x35, 0x2a); /* clk */
	mipi_dsi_tif_write(resource_index, 0x45, 0x2a); /* ch0 */
	mipi_dsi_tif_write(resource_index, 0x55, 0x2a); /* ch1 */
	mipi_dsi_tif_write(resource_index, 0x65, 0x2a); /* ch2 */
	mipi_dsi_tif_write(resource_index, 0x75, 0x2a); /* ch3 */

	while (mipi_dsi_check_status(
			resource_index, DSI_STATUS_MIPI_PLL_LOCK) == 0 )
		mdelay(1);

	_mipi_dsi_int0_clear_all_irqs(resource_index);
	/* Now it's D-PHY LP11 state.*/
}



int mipi_dsi_vsync_register_isr(mipi_dsi_vsync_isr_t isr, void *arg)
{
	/* the return value is negative (ret < 0) if it has an error,
	   or the registered isr_num in registered_isr[n] ("ret >= 0")
	   if it completed with no error.*/
	int i;
	int ret;
	unsigned long flags;
	struct mipi_dsi_isr_data *isr_data;

	if (isr == NULL)
	{
		DSSERR("isr is NULL \n");
		return -EINVAL;
	}

	spin_lock_irqsave(&dsi_dev.vsync_irq_lock, flags);

	/* check for dsi duplicate entry */
	for (i = 0; i < VSYNC_MAX_NR_ISRS; i++) {
		isr_data = &dsi_dev.registered_isr[i];
		if (isr_data->isr == isr && isr_data->arg == arg) {
			ret = -EINVAL;
			DSSDBG("vsync register_isr is existed\n");
			goto err;
		}
	}

	isr_data = NULL;
	ret = -EBUSY;

	for (i = 0; i < VSYNC_MAX_NR_ISRS; i++) {
		isr_data = &dsi_dev.registered_isr[i];

		if (isr_data->isr != NULL)
			continue;

		isr_data->isr = isr;
		isr_data->arg = arg;
		ret = i;
		break;
	}

	if (ret < 0)
		goto err;

#if 0
	mipi_dsi_vsync_enable();
#endif

	spin_unlock_irqrestore(&dsi_dev.vsync_irq_lock, flags);

	return ret;
err:
	spin_unlock_irqrestore(&dsi_dev.vsync_irq_lock, flags);

	return ret;
}

int mipi_dsi_vsync_unregister_isr(odin_crg_isr_t isr, void *arg)
{
	int i;
	unsigned long flags;
	int ret = -EINVAL;
	struct mipi_dsi_isr_data *isr_data;

	spin_lock_irqsave(&dsi_dev.vsync_irq_lock, flags);

	for (i = 0; i < VSYNC_MAX_NR_ISRS; i++) {
		isr_data = &dsi_dev.registered_isr[i];
		if (isr_data->isr != isr || isr_data->arg != arg)
			continue;

		isr_data->isr = NULL;
		isr_data->arg = NULL;

		ret = 0;
		break;
	}

#if 0
	if (ret == 0)
		mipi_dsi_vsync_disable();
#endif

	spin_unlock_irqrestore(&dsi_dev.vsync_irq_lock, flags);

	return ret;
}

int mipi_dsi_set_state(enum mipi_dsi_state dsi_state)
{
	return (dsi_dev.dsi_state = dsi_state);
}

int odin_mipi_dsi_get_state(void)
{
	return (int) dsi_dev.dsi_state;
}

bool odin_mipi_cmd_loop_get(enum odin_mipi_dsi_index resource_index)
{
	return _mipi_dsi_command_mode_loop_get(resource_index);
}

struct complete_data {
	struct completion 	*p_completion;
	int			isr_num;
};

void mipi_dsi_irq_wait_handler(void *data)
{
	struct mipi_dsi_isr_data *isr_data;
	int isr_num = ((struct complete_data *)data)->isr_num;

	if (isr_num < 0 || isr_num >= VSYNC_MAX_NR_ISRS)
		return;

	isr_data = &dsi_dev.registered_isr[isr_num];

	spin_lock(&isr_data->completion_lock);
	if (isr_data->complete_valid)
		complete(((struct complete_data *)data)->p_completion);
	spin_unlock(&isr_data->completion_lock);
}

int odin_mipi_dsi_vsync_wait_for_irq_interruptible_timeout
		(unsigned long timeout)
{

	int r, isr_num;
	unsigned long flags;
	struct mipi_dsi_isr_data *isr_data;
	struct complete_data complete_data;
	DECLARE_COMPLETION_ONSTACK(completion);

	complete_data.p_completion = &completion;

	r = mipi_dsi_vsync_register_isr(mipi_dsi_irq_wait_handler, &complete_data);

	if (r < 0)
		goto skip;

	isr_num = r;
	isr_data = &dsi_dev.registered_isr[isr_num];

	/* valid section for vsync complete() available */
	spin_lock_irqsave(&isr_data->completion_lock, flags);
	complete_data.isr_num = isr_num;
	isr_data->complete_valid = true;
	spin_unlock_irqrestore(&isr_data->completion_lock, flags);

	timeout = wait_for_completion_interruptible_timeout(&completion,
			timeout);

	spin_lock_irqsave(&isr_data->completion_lock, flags);
	isr_data->complete_valid = false;
	spin_unlock_irqrestore(&isr_data->completion_lock, flags);

	/* invalid section for vsync complete() not available */
	mipi_dsi_vsync_unregister_isr((odin_crg_isr_t)mipi_dsi_irq_wait_handler, (void *)&complete_data);

	if (timeout == 0)
		r = -ETIMEDOUT;
	else  if (timeout == -ERESTARTSYS)
		r = timeout;

	return r;

skip:
	return 0;
}

void odin_mipi_dsi_set_lane_config(
	enum odin_mipi_dsi_index resource_index, struct odin_dss_device *dssdev,
	enum odin_mipi_dsi_lane_status status)
{
	enum odin_mipi_dsi_lane_num lane_num;

	if (status == ODIN_DSI_LANE_LP)
		lane_num = ODIN_DSI_1LANE;
	else
		lane_num = (enum odin_mipi_dsi_lane_num)dssdev->phy.dsi.lane_num;

	mipi_dsi_cfg_phy_tx_lane_power(resource_index, true); /* PHY Power ON */
	mdelay(1);

	mipi_dsi_lane_config(resource_index, lane_num); /* Lane Select */
	mdelay(1);

	if (status == ODIN_DSI_LANE_LP)
		mipi_dsi_cfg0_datapath_mode(resource_index,
			ODIN_DSI_DATAPATH_COMMAND_MODE);/*command*/
	else if (status == ODIN_DSI_LANE_HS)
	{
		if ((dssdev->driver->get_update_mode(dssdev)) ==
							ODIN_DSS_UPDATE_AUTO)
			mipi_dsi_cfg0_datapath_mode(
			resource_index, ODIN_DSI_DATAPATH_VIDEO_MODE);  /*video*/
		else
			mipi_dsi_cfg0_datapath_mode(
			resource_index, ODIN_DSI_DATAPATH_COMMAND_MODE);
	}

	odin_mipi_dsi_fifo_flush_all(resource_index);
}

u32 odin_mipi_hs_video_mode(
	enum odin_mipi_dsi_index resource_index, struct odin_dss_device *dssdev)
{
	u32 cntretry = 0;
	u32 dip_cntretry = 0;
	u32 mipi_cntretry = 0;

	odin_mipi_dsi_set_lane_config(resource_index, dssdev, ODIN_DSI_LANE_HS);

	/* dip flush*/

	if (_mipi_dsi_int0_check_irq(
			resource_index, DSI_INT0_CLK_LANE_ENTER_HS) == 0)
	{
		mipi_dsi_action_lane_enter(resource_index, MIPI_DSI_CLK_LANE_HS);
		while (_mipi_dsi_int0_check_irq(resource_index,
				DSI_INT0_CLK_LANE_ENTER_HS) == 0)
		{
			cntretry++;
			mdelay(1);
		}
		DSSDBG("\t-->> Retry counter to enter CLK HS mode: %d\n", cntretry);
		_mipi_dsi_int0_clear_irq(resource_index, DSI_INT0_CLK_LANE_ENTER_HS);
	}

	/* Due to clks are not synch, better to set the jetter buffer*/
	mipi_dsi_command_cfg_hs_wait_fifo(resource_index, 1);	/* need???*/
	mipi_dsi_command_cfg_hs_num_bytes(resource_index, 512);	/* need???*/

	do {
		odin_dipc_fifo_flush(resource_index);
		mdelay(1);
		dip_cntretry++;
		if (dip_cntretry > 100)
		{
			DSSERR("DIP fifo flush cnt fail \n");
			break;
		}
	} while (odin_dipc_get_lcd_cnt(resource_index) != 0x0);

	do {
		odin_mipi_dsi_fifo_flush_all(resource_index);
		mdelay(1);
		mipi_cntretry++;
		if (mipi_cntretry > 100)
		{
			DSSERR("MIPI fifo flush cnt fail \n");
			break;
		}
	} while (odin_mipi_dsi_check_tx_fifo(resource_index) != 0x0);

	mipi_build_hs_package(
		resource_index, DSI_SRC_DATA_ID_DCS_LONG_WRITE, 1,
		ODIN_DSI_HS_EOTP_ENABLE,
		ODIN_DSI_HS_MODE_1, MIPI_DSI_LONG_PACKET, true);

	odin_du_set_ovl_cop_or_rop(
		resource_index, ODIN_DSS_OVL_ROP); /* For Pattern Data */

	return 0;
}

void odin_mipi_hs_video_mode_stop(enum odin_mipi_dsi_index resource_index,
	struct odin_dss_device *dssdev)
{
	u32 cntretry = 0;
	do
	{
		_mipi_dsi_action(resource_index,
				 ID_DSI_ACTION_STOP_VIDEO_DATA, 1);
		mdelay(17);
		if (cntretry >= 10)
		{
			DSSERR("\t++>> Error Time out wating for STOP_VIDEO_DATA(TimeOut:%d[ms])\n",
						cntretry);
			break;
		}
		cntretry++;
		DSSDBG("high-speed video stop\n");
	}while (mipi_dsi_check_status(resource_index, DSI_STATUS_HS_DATA_SM) != 0);

	cntretry = 0;
	while (_mipi_dsi_int0_check_irq(resource_index,
			DSI_INT0_HS_DATA_COMPLETE) == 0)
	{
		if (cntretry >= 100)
		{
			DSSERR("\t++>> Error Time out wating for HS_DATA_COMPLETE(TimeOut:%d[ms])\n",
							cntretry);
			break;
		}
		cntretry++;
		mdelay(1);
	}

	_mipi_dsi_int0_clear_irq(resource_index, DSI_INT0_HS_DATA_COMPLETE);

	/* stop DU * flush*/

	/* clean up last frame interrupt.*/
	_mipi_dsi_int0_clear_irq(resource_index, DSI_INT0_FRAME_COMPLETE);

	_mipi_dsi_action(resource_index, ID_DSI_ACTION_FLUSH_DATA_FIFO, 1);

	/* exit hs mode*/
	mipi_dsi_change_lane_status(resource_index, ODIN_DSI_LANE_LP);

	cntretry = 0;
	while (_mipi_dsi_int0_check_irq(resource_index,
			DSI_INT0_CLK_LANE_EXIT_HS) == 0)
	{
		if (cntretry >= 10)
		{
			DSSERR("\t++>> Error Time out wating for CLK_LANE_EXIT_HS(TimeOut:%d[ms])\n",
									cntretry);
			break;
		}
		cntretry++;
		mdelay(1);
	}

	odin_mipi_dsi_set_lane_config(resource_index, dssdev, ODIN_DSI_LANE_LP);
}

void odin_mipi_hs_video_start_stop(enum odin_mipi_dsi_index resource_index,
	struct odin_overlay_manager *mgr, bool start_stop)
{
	u32 mipi_cntretry = 0;
	u32 cntretry = 0;

	unsigned long timeout = msecs_to_jiffies(100);
	int r;

	if (start_stop)
	{
		if (mipi_dsi_check_status(resource_index,
					  DSI_STATUS_HS_DATA_SM) != 0)
		{
			odin_mipi_hs_video_mode_stop(resource_index,
						     mgr->device);
			odin_mipi_hs_video_mode(resource_index, mgr->device);
			odin_set_rd_enable_plane(ODIN_DSS_CHANNEL_LCD0, 1);
			odin_du_set_ovl_cop_or_rop(
				resource_index, ODIN_DSS_OVL_COP);
			odin_crg_set_err_mask(mgr->device, true);
			mgr->frame_skip = false;
			return;
		}

		while ((odin_dipc_get_lcd_cnt(resource_index) != 0x0) ||
			(odin_mipi_dsi_check_tx_fifo(resource_index) != 0x0))
		{
			odin_mipi_dsi_fifo_flush_all(resource_index);
			odin_dipc_fifo_flush(resource_index);
			msleep(1);
			cntretry++;
			if (cntretry > 20)
				break;
		}

		if (cntretry > 20)
			DSSERR("fifo flush retry cntretry = %d \n", cntretry);

		odin_set_rd_enable_plane(ODIN_DSS_CHANNEL_LCD0, 1);
		mgr->frame_skip = false;
		mipi_build_hs_package(
			resource_index, DSI_SRC_DATA_ID_DCS_LONG_WRITE, 1,
			ODIN_DSI_HS_EOTP_ENABLE,
			ODIN_DSI_HS_MODE_1, MIPI_DSI_LONG_PACKET, true);
	}
	else
	{
		_mipi_dsi_action(resource_index,
				 ID_DSI_ACTION_STOP_VIDEO_DATA, 1);
		r = odin_crg_wait_for_irq_interruptible_timeout(
			CRG_IRQ_MIPI0, CRG_IRQ_MIPI_FRAME_COMPLETE, timeout);
		if (r!=0)
			DSSERR("video_stop frame complete timeout \n");

		odin_set_rd_enable_plane(ODIN_DSS_CHANNEL_LCD0, 0);

		while (mipi_dsi_check_status(resource_index,
					     DSI_STATUS_HS_DATA_SM) != 0)
		{
			if (mipi_cntretry >= 34) /*2 frame is over */
			{
				DSSERR("++ 2 frame wait, but MIPI SM is not stop state ++\n");
				break;
			}
			mipi_cntretry++;
			_mipi_dsi_action(resource_index,
					 ID_DSI_ACTION_STOP_VIDEO_DATA, 1);
			msleep(1);
		}

		mgr->frame_skip = true;
		odin_set_rd_enable_plane(ODIN_DSS_CHANNEL_LCD0, 0);
	}
}

int odin_mipi_dsi_check_tx_fifo(enum odin_mipi_dsi_index resource_index)
{
	u32 val;

	val = _mipi_dsi_get_fifo_byte_count(resource_index);
	if ((val & 0xfff00000) == 0x80000000)
		return 0;
	else
		return 1;
}

u32 odin_mipi_dsi_get_tx_fifo(enum odin_mipi_dsi_index resource_index)
{
	u32 val;

	val = _mipi_dsi_get_fifo_byte_count(resource_index);
	return val;
}

static void  _mipi_dsi_get_rxpacketheader(
                enum odin_mipi_dsi_index resource_index,
                unsigned char *data0,  unsigned char *data1,
                unsigned char *dataID)
{
    u32 reg_val = dsi_read_reg(resource_index, ADDR_DSI_RX_PACKET_HEADER0);

    if(data0 != NULL)
    {
        *data0 = (unsigned char) (reg_val >> 8) & 0xff ;
    }
    if(data1 != NULL)
    {
        *data1 = (unsigned char) (reg_val >> 16) & 0xff ;
    }
    if(dataID != NULL)
    {
        *dataID = (unsigned char) reg_val & 0xff;
    }
}


int odin_mipi_dsi_check_AwER(enum odin_mipi_dsi_index resource_index)
{
    unsigned char dt, data0, data1;
    int ret = 0;

    enum odin_mipi_dsi_lane_num saveddatalane =
            mipi_dsi_get_lane_num(resource_index);

//	mipi_dsi_sel_lanes(resource_index, ODIN_DSI_1LANE);
//	mipi_dsi_cfg0_phy_lane_sel(resource_index, ODIN_DSI_1LANE);

    _mipi_dsi_int0_mask_irq(resource_index, DSI_INT0_RX_COMPLETE);
    _mipi_dsi_int0_mask_irq(resource_index, DSI_INT0_RX_ECC_ERR1);
    _mipi_dsi_int0_mask_irq(resource_index, DSI_INT0_RX_ECC_ERR2);
    _mipi_dsi_int0_mask_irq(resource_index, DSI_INT0_RXTRIGGER3);
    _mipi_dsi_int0_mask_irq(resource_index, DSI_INT0_RXTRIGGER2);
    _mipi_dsi_int0_mask_irq(resource_index, DSI_INT0_RXTRIGGER1);
    _mipi_dsi_int0_mask_irq(resource_index, DSI_INT0_RXTRIGGER0);

// BTA(Bus Turnaround) Action
    mipi_action_turn_request(resource_index);
//    udelay(10); /* wait 1usec */
#if 1
    if(mipi_dsi_check_status(resource_index, DSI_STATUS_PHY_DIRECTION0)) {
        DSSINFO("The D-PHY went to the Rx state!\n");
        // Wait until the direction returns back to Tx.
        while(mipi_dsi_check_status(resource_index, DSI_STATUS_PHY_DIRECTION0));
    } else {
		udelay(1); /* wait 1usec */
        // Wait until the direction returns back to Tx.
        while(mipi_dsi_check_status(resource_index, DSI_STATUS_PHY_DIRECTION0));
    }
#endif
	if (_mipi_dsi_int0_check_irq(
			resource_index, DSI_INT0_RX_COMPLETE))
	{
        DSSERR("MIPI RX COMPLETE\n");
    	_mipi_dsi_int0_clear_irq(resource_index, DSI_INT0_RX_COMPLETE);
        if (_mipi_dsi_int0_check_irq(
                resource_index, DSI_INT0_RX_ECC_ERR1))
        {
            DSSERR("MIPI RX ECC_ERR1\n");
            _mipi_dsi_int0_clear_irq(resource_index, DSI_INT0_RX_ECC_ERR1);
            goto error;
        }
        if (_mipi_dsi_int0_check_irq(
                resource_index, DSI_INT0_RX_ECC_ERR2))
        {
            DSSERR("MIPI RX ECC_ERR2\n");
            _mipi_dsi_int0_clear_irq(resource_index, DSI_INT0_RX_ECC_ERR2);
            goto error;
        }
        // Get Rx Packet Header
        _mipi_dsi_get_rxpacketheader(resource_index, &data0, &data1, &dt);
        ret = data1 << 8 | data0;
        // Clear Rx Packet Header for debugging...
       //   mipi_dsi_action_rx_packet_header_clear(resource_index, MIPI_DSI_INDEX_LANE0);
       //   while(mipiDsiRegisters->rxPacketHeader.as32Bits != 0) ;
    } else {
        DSSERR("MIPI RX not COMPLETE\n");
    }

    if(_mipi_dsi_int0_check_irq(
               resource_index, DSI_INT0_RXTRIGGER0)) {
                DSSERR("MIPI RX DSI_INT0_RXTRIGGER0\n");
    }
    if(_mipi_dsi_int0_check_irq(
               resource_index, DSI_INT0_RXTRIGGER1)) {
                DSSERR("MIPI RX DSI_INT0_RXTRIGGER1\n");
    }
    if(_mipi_dsi_int0_check_irq(
               resource_index, DSI_INT0_RXTRIGGER2)) {
                DSSERR("MIPI RX DSI_INT0_RXTRIGGER2\n");
    }
    if(_mipi_dsi_int0_check_irq(
                   resource_index, DSI_INT0_RXTRIGGER3)) {
           DSSERR("MIPI RX DSI_INT0_RXTRIGGER3\n");

    }

    _mipi_dsi_int0_unmask_irq(resource_index, DSI_INT0_RX_COMPLETE);
//    _mipi_dsi_int0_unmask_irq(resource_index, DSI_INT0_RX_ECC_ERR1);
//    _mipi_dsi_int0_unmask_irq(resource_index, DSI_INT0_RX_ECC_ERR2);

//	mipi_dsi_sel_lanes(resource_index, saveddatalane);
//	mipi_dsi_cfg0_phy_lane_sel(resource_index, saveddatalane);
    DSSERR("MIPI END\n");

error:
    return ret;
}


u32 mipi_hs_cmd_init_data(
	enum odin_mipi_dsi_index resource_index, u16 inital_data)
{
	mipi_dsi_command_cfg_hs_force_data(resource_index, inital_data);
	mipi_dsi_command_cfg_hs_force_data_en(resource_index, true);

	return 0;
}

u32 odin_mipi_cmd_loop_set(enum odin_mipi_dsi_index resource_index,
	bool enable, unsigned int loop_cnt)
{
	_mipi_dsi_command_mode_loop_setting(resource_index, enable, loop_cnt);
	return 0;
}

u32 odin_mipi_hs_command_update_window(
	struct odin_dss_device *dssdev, u16 x, u16 y, u16 w, u16 h)
{
	u16 x1 = x;
	u16 x2 = x + w - 1;
	u16 y1 = y;
	u16 y2 = y + h - 1;
	u32 retry_cnt;
	u32 *word0;
	u32 *word1;


	u8 buf1[4] = {0, };
	u8 buf2[4] = {0, };

	word0 = (u32 *) buf1;
	word1 = (u32 *) buf2;

	buf1[0] = MIPI_DSI_DCS_CMD_SET_COLUMN_ADDRESS;
	buf1[1] = (x1 >> 8) & 0xff;
	buf1[2] = (x1 >> 0) & 0xff;
	buf1[3] = (x2 >> 8) & 0xff;

	buf2[0] = (x2 >> 0) & 0xff;

	printk(KERN_INFO "column : word0 = %x, word1 = %x \n", *(word0), *(word1));

	_mipi_dsi_int0_clear_irq(dssdev->channel, DSI_INT0_HS_CFG_COMPLETE);
	mipi_build_hs_package(dssdev->channel,
			      DSI_SRC_DATA_ID_DCS_LONG_WRITE, 5,
			      MIPI_HS_EOTP_DISABLE, ODIN_DSI_HS_MODE_0,
			      MIPI_DSI_LONG_PACKET, false);

	dsi_write_reg(dssdev->channel, 0x1000, *(word0));
	dsi_write_reg(dssdev->channel, 0x1000 + 0x4,*(word1));

	retry_cnt = 0;
	while (_mipi_dsi_int0_check_irq(dssdev->channel,
			DSI_INT0_HS_CFG_COMPLETE) == 0)
	{
		retry_cnt++;
			/*msleep(1);*/
	}
	_mipi_dsi_int0_clear_irq(dssdev->channel,
				 DSI_INT0_HS_CFG_COMPLETE);
	mipi_dsi_action_fifo_flush(dssdev->channel,
				   ID_DSI_ACTION_FLUSH_CFG_FIFO);

	*(word0) = 0;
	*(word1) = 0;

	buf1[0] = MIPI_DSI_DCS_CMD_SET_PAGE_ADDRESS;
	buf1[1] = (y1 >> 8) & 0xff;
	buf1[2] = (y1 >> 0) & 0xff;
	buf1[3] = (y2 >> 8) & 0xff;

	buf2[0] = (y2 >> 0) & 0xff;

	printk(KERN_INFO "page : word0 = %x, word1 = %x \n", *(word0), *(word1));

	dsi_write_reg(dssdev->channel, 0x1000, *(word0));
	dsi_write_reg(dssdev->channel, 0x1000 + 0x4,*(word1));

	mipi_build_hs_package(dssdev->channel,
			      DSI_SRC_DATA_ID_DCS_LONG_WRITE, 5,
			      MIPI_HS_EOTP_DISABLE, ODIN_DSI_HS_MODE_0,
			      MIPI_DSI_LONG_PACKET, false);

	retry_cnt = 0;
	while (_mipi_dsi_int0_check_irq(dssdev->channel,
			DSI_INT0_HS_CFG_COMPLETE) == 0)
	{
		retry_cnt++;
			/*msleep(1);*/
	}
	_mipi_dsi_int0_clear_irq(dssdev->channel, DSI_INT0_HS_CFG_COMPLETE);
	mipi_dsi_action_fifo_flush(
				dssdev->channel, ID_DSI_ACTION_FLUSH_CFG_FIFO);

	return 0;

}

u32 odin_mipi_hs_cmd_mode(
	enum odin_mipi_dsi_index resource_index, struct odin_dss_device *dssdev)
{
	u32 cntretry = 0;

	odin_mipi_dsi_set_lane_config(resource_index, dssdev, ODIN_DSI_LANE_HS);

	do {
		odin_dipc_fifo_flush(resource_index);
		mdelay(1);
	} while (odin_dipc_get_lcd_cnt(resource_index) != 0x0);

	do {
		odin_mipi_dsi_fifo_flush_all(resource_index);
		mdelay(1);
	} while (odin_mipi_dsi_check_tx_fifo(resource_index) != 0x0);

	mipi_dsi_action_lane_enter(resource_index, MIPI_DSI_CLK_LANE_HS);
	if (_mipi_dsi_int0_check_irq(
			resource_index, DSI_INT0_CLK_LANE_ENTER_HS) == 0)
	{
		mipi_dsi_action_lane_enter(resource_index, MIPI_DSI_CLK_LANE_HS);
		while (_mipi_dsi_int0_check_irq(resource_index,
				DSI_INT0_CLK_LANE_ENTER_HS) == 0)
		{
			cntretry++;
			mdelay(1);
		}
		DSSDBG("\t-->> Retry counter to enter CLK HS mode: %d\n", cntretry);
		_mipi_dsi_int0_clear_irq(resource_index, DSI_INT0_CLK_LANE_ENTER_HS);
	}

	/*Due to clks are not synch, better to set the jetter buffer*/
	mipi_dsi_command_cfg_hs_wait_fifo(resource_index, 1);
#ifdef MIPI_PKT_SIZE_CONTROL
	mipi_dsi_command_cfg_hs_num_bytes(resource_index, 1944);
#else
	mipi_dsi_command_cfg_hs_num_bytes(resource_index, 2016);
#endif
	/* mipi_dsi_command_cfg_hs_num_bytes(resource_index, 1024); */

#if 0
	odin_mipi_hs_command_update_window(
		dssdev, 0, 0, dssdev->panel.timings.x_res, dssdev->panel.timings.y_res);
#endif

	return 0;
}

u32 odin_mipi_hs_cmd_mode_stop(enum odin_mipi_dsi_index resource_index,
	struct odin_dss_device *dssdev)
{
	u32 cntretry = 0;

	odin_mipi_cmd_loop_set(resource_index, false, 0);
	mipi_dsi_command_cfg_hs_force_data_en(resource_index, false);

	_mipi_dsi_int0_clear_irq(resource_index, DSI_INT0_HS_DATA_COMPLETE);

	/* stop DU * flush*/

	/* clean up last frame interrupt.*/
	_mipi_dsi_int0_clear_irq(resource_index, DSI_INT0_FRAME_COMPLETE);

	mipi_dsi_fifo_flush_all(resource_index);

	/* exit hs mode*/
	mipi_dsi_change_lane_status(resource_index, ODIN_DSI_LANE_LP);

	if (_mipi_dsi_int0_check_irq(
			resource_index, DSI_INT0_CLK_LANE_EXIT_HS) == 0)
	{
		cntretry++;
		mipi_dsi_action_lane_exit(resource_index,
					  MIPI_DSI_CLK_LANE_HS);
		mdelay(1);
	}
	_mipi_dsi_int0_clear_irq(resource_index, DSI_INT0_CLK_LANE_EXIT_HS);

	odin_mipi_dsi_set_lane_config(resource_index, dssdev, ODIN_DSI_LANE_LP);

	return 0;
}

int odin_mipi_cmd_wait_for_framedone(u32 irqmask, u32 sub_irqmask,
		unsigned long timeout)
{
#if 0
	void mipi_irq_wait_handler(void *data, u32 mask, u32 sub_mask)
	{
		enum odin_channel channel;
#ifdef DSS_INTERRUPT_COMMAND_SEND
#else
		bool mipi_state = odin_mipi_cmd_loop_get(channel);
#endif

		if (mask & CRG_IRQ_MIPI1)
			channel = ODIN_DSS_CHANNEL_LCD1;
		else
			channel = ODIN_DSS_CHANNEL_LCD0;

#ifdef DSS_INTERRUPT_COMMAND_SEND
		if ((dsi_dev.updating == false)
			|| (odin_crg_get_error_handling_status()))
#else
		if (mipi_state)
			|| (odin_crg_get_error_handling_status()))
#endif
			complete((struct completion *)data);
	}

	int r;

	DECLARE_COMPLETION_ONSTACK(completion);

	if ((dsi_dev.updating == false) ||
		(odin_crg_get_error_handling_status()))
		return 0;

	if (odin_crg_get_error_handling_status())
		return 0;

	/* DSSINFO("vsync is over"); */
	r = odin_crg_register_isr(mipi_irq_wait_handler, &completion,
			irqmask, sub_irqmask);

	if (r)
		goto done;

	/* re -check */
	if ((dsi_dev.updating == false) ||
		(odin_crg_get_error_handling_status()))
	{
		odin_crg_unregister_isr(mipi_irq_wait_handler, &completion,
			irqmask, sub_irqmask);
		return 0;
	}

	timeout = wait_for_completion_interruptible_timeout(&completion,
			timeout);

	odin_crg_unregister_isr(mipi_irq_wait_handler, &completion,
		irqmask, sub_irqmask);

	if (timeout == 0)
		r = -ETIMEDOUT;
	else  if (timeout == -ERESTARTSYS)
		r = timeout;

done:

	return r;
#else
	int r = 0;

#if 0
	if (odin_crg_get_error_handling_status())
		return 0;
#endif

	mipi_dsi_set_state(MIPI_FRAMEDONE_WAITING);

	timeout = wait_for_completion_interruptible_timeout(
		&dsi_dev.framedone_completion, timeout);

	if (timeout == 0)
		r = -ETIMEDOUT;
	else  if (timeout == -ERESTARTSYS)
		r = timeout;

	return r;
#endif
}

#if 1
extern atomic_t vsync_ready;
#endif
static void mipi_dsi_vsync_update_isr(void *data)
{
	/********mutex lock issue**************/
	struct odin_dss_device	*dssdev = (struct odin_dss_device *)data;
	dsi_dev.vsync_time = ktime_get();
	if (atomic_read(&vsync_ready))
	{
		dssdev->driver->update(dssdev, 0, 0, 0, 0);
		atomic_set(&vsync_ready, 0);
	}
}

bool mipi_dsi_get_vsync_irq_status(void)
{
	return dsi_dev.irq_enabled;
}

void mipi_dsi_vsync_enable(struct odin_dss_device *dssdev)
{
	struct odin_overlay_manager *mgr;

	spin_lock(&vsync_ctrl_lock);

	mgr = dssdev->manager;

	mipi_dsi_vsync_register_isr(mipi_dsi_vsync_update_isr, dssdev);

	if (!dsi_dev.irq_enabled)
	{
		enable_irq(dsi_dev.irq);
		dsi_dev.irq_enabled = true;
	}

	mgr->vsync_ctrl = true;

	spin_unlock(&vsync_ctrl_lock);
}

void mipi_dsi_vsync_disable(struct odin_dss_device *dssdev)
{
	int i;
	struct mipi_dsi_isr_data *isr_data;
	bool other_isr_exist = false;
	struct odin_overlay_manager *mgr;

	spin_lock(&vsync_ctrl_lock);
	mgr = dssdev->manager;

	/* if wq is ready to update in vsync,
	   don't vsync disabled */
	if (atomic_read(&vsync_ready))
		goto vsync_disable_done;

	mipi_dsi_vsync_unregister_isr((odin_crg_isr_t)mipi_dsi_vsync_update_isr,
				      (void *)dssdev);

	for (i = 0; i < VSYNC_MAX_NR_ISRS; i++) {
		isr_data = &dsi_dev.registered_isr[i];

		if (isr_data->isr == NULL)
			continue;

		other_isr_exist = true;
	}

	if ((dsi_dev.irq_enabled) && (!other_isr_exist))
	{
		disable_irq(dsi_dev.irq);
		dsi_dev.irq_enabled = false;
	}

	mgr->vsync_ctrl = false;

vsync_disable_done:
	spin_unlock(&vsync_ctrl_lock);
}

int mipi_dsi_vsync_ctrl_status(struct odin_dss_device *dssdev)
{
	struct odin_overlay_manager *mgr = dssdev->manager;
	bool vsync_ctrl_status;

	spin_lock(&vsync_ctrl_lock);
	vsync_ctrl_status = mgr->vsync_ctrl;
	spin_unlock(&vsync_ctrl_lock);

	return vsync_ctrl_status;
}

u32 odin_mipi_dsi_vsync_after_time(void)
{
	ktime_t end_time;
	s64 lead_time_us;
	u32 decimation;
	end_time = ktime_get();

	lead_time_us = ktime_to_us(ktime_sub(end_time, dsi_dev.vsync_time));
	decimation = lead_time_us;

	return decimation;
}


s64 odin_mip_dsi_time_after_update(void)
{
	ktime_t end_time;
	s64 lead_time_ns;

	end_time = ktime_get();
	lead_time_ns = ktime_to_ns(ktime_sub(end_time, dsi_dev.update_start));
	return lead_time_ns;
}

int odin_mipi_dsi_get_update_status(void)
{
	return	dsi_dev.updating;
}

void odin_mipi_dsi_set_update_status(bool value)
{
	spin_lock(&dsi_dev.irq_lock);
	dsi_dev.updating = value;
	spin_unlock(&dsi_dev.irq_lock);
}

static void mipi_dsi_disable_isr(void *data, u32 mask, u32 sub_mask)
{
	int r;
	enum odin_channel channel;
	/* DSSINFO("update end \n"); */

	spin_lock(&dsi_dev.irq_lock);
	dsi_dev.updating = false;
	spin_unlock(&dsi_dev.irq_lock);

	if (mask & CRG_IRQ_MIPI1)
		channel = ODIN_DSS_CHANNEL_LCD1;
	else
		channel = ODIN_DSS_CHANNEL_LCD0;

#if RUNTIME_CLK_GATING
	/*disable_runtime_clk (channel);*/
#endif

	r = odin_crg_unregister_isr(mipi_dsi_disable_isr,
			data, CRG_IRQ_MIPI0, DSI_INT0_HS_DATA_COMPLETE);

	if (r < 0)
		DSSERR("unregister fail mask:%d, submask:%d \n",
			CRG_IRQ_MIPI0, DSI_INT0_HS_DATA_COMPLETE);
#if 0
#ifdef DSS_DFS_MUTEX_LOCK
	mutex_unlock(&dfs_mtx);
#elif defined(DSS_DFS_SPINLOCK)
	put_mem_dfs();
#endif
#endif

#ifdef DSS_TIME_CHECK_LOG
	if (check_time == ODIN_CMD_START_END_TIME)
		DSSINFO("check_time:%d, elapsed time = %lld \n",
			check_time, odin_mgr_dss_time_end(ODIN_CMD_START_END_TIME));
#endif
}

#ifdef DSS_INTERRUPT_COMMAND_SEND
static void mipi_dsi_sending_isr(void *data, u32 mask, u32 sub_mask)
{
	enum odin_channel channel;

	if (mask & CRG_IRQ_MIPI1)
		channel = ODIN_DSS_CHANNEL_LCD1;
	else
		channel = ODIN_DSS_CHANNEL_LCD0;

	if ((odin_mipi_dsi_check_tx_fifo(channel) != 0x0) ||
		(odin_dipc_get_lcd_cnt(channel) != 0x0))
	{
		DSSINFO("fifo is not flushed, but hs_completed \n");
		return;
	}
#if 0
	if (odin_mipi_dsi_get_update_status())
	{
		/* if dsi_dev.updating is already false,*/
		/*it means dss_irq_handler executed lately...*/
		/* so, we have to disable clk only if dsi_dev.updating has true. */
#if RUNTIME_CLK_GATING
		disable_runtime_clk (channel);
#endif
#ifdef DSS_DFS_MUTEX_LOCK
		mutex_unlock(&dfs_mtx);
#elif defined(DSS_DFS_SPINLOCK)
		put_mem_dfs();
#endif
	}
#endif
	/* DSSINFO("update last end \n"); */
#if 1
	spin_lock(&dsi_dev.irq_lock);
	dsi_dev.updating = false;
	spin_unlock(&dsi_dev.irq_lock);
#endif

#ifdef DSS_TIME_CHECK_LOG
	if (check_time == ODIN_CMD_START_END_TIME)
		DSSINFO("check_time:%d, elapsed time = %lld \n",
			check_time, odin_mgr_dss_time_end(ODIN_CMD_START_END_TIME));
#endif
	mipi_dsi_set_state(MIPI_COMPLETE);

	dsscomp_release_fence_display(channel, false);

	complete_all(&dsi_dev.framedone_completion);
}

u32 mipi_dsi_clear_hs_complete(enum odin_mipi_dsi_index resource_index)
{
	u32 retry_cnt = 0;

	while (_mipi_dsi_int0_check_irq(
				resource_index, DSI_INT0_HS_DATA_COMPLETE)
		!= 0)
	{
		retry_cnt++;
		_mipi_dsi_int0_clear_irq(
				resource_index, DSI_INT0_HS_DATA_COMPLETE);
		udelay(1); /* wait 1usec */
		if (retry_cnt > 10)
		{
			DSSERR("hs_data complete clear is timeout error \n");
			break;
		}
	}
	return 0;
}

u32 odin_mipi_hs_command_update_loop(enum odin_mipi_dsi_index resource_index,
	struct odin_dss_device *dssdev)
{
	u32 max_pkt_size = dssdev->phy.dsi.cmd_max_size;
	u32 data_size = dssdev->phy.dsi.frame_size;
	u32 byte_cnt;
	u32 retry_cnt = 0;
	unsigned long flags;
	int loop_cnt;

	loop_cnt = (data_size / (max_pkt_size-1)) - 1;
	byte_cnt = max_pkt_size;

	dsi_dev.update_start = ktime_get();

	spin_lock_irqsave(&dsi_dev.irq_lock, flags);
	dsi_dev.updating = true;
	spin_unlock_irqrestore(&dsi_dev.irq_lock, flags);

#ifdef DSS_TIME_CHECK_LOG
	if (check_time == ODIN_CMD_START_END_TIME)
		odin_mgr_dss_time_start(ODIN_CMD_START_END_TIME);
#endif

	mipi_dsi_set_state(MIPI_FIRST_START);
	mipi_dsi_clear_hs_complete(resource_index);
	odin_crg_set_interrupt_mask(CRG_IRQ_MIPI0, false);
	_mipi_dsi_int0_mask_irq(resource_index, DSI_INT0_HS_DATA_COMPLETE);

	odin_mipi_cmd_loop_set(resource_index, false, loop_cnt);
	mipi_hs_cmd_init_data(resource_index, MIPI_DSI_DCS_CMD_MEMORY_WRITE);

	mipi_build_hs_package(
		resource_index ,DSI_SRC_DATA_ID_DCS_LONG_WRITE, byte_cnt,
		MIPI_HS_EOTP_ENABLE, ODIN_DSI_HS_MODE_1,
		MIPI_DSI_LONG_PACKET, false);

	retry_cnt = 0;
	while (_mipi_dsi_int0_check_irq(
			resource_index, DSI_INT0_HS_DATA_COMPLETE)
			== 0)
	{
		retry_cnt++;
		udelay(1); /* wait 1usec */
		if (retry_cnt > 2000)
		{
			DSSERR("first packet is timeout error \n");

			DSSERR("core_clk:0x%x, other_clk:0x%x\n",
					odin_crg_get_dss_clk_ena(CRG_CORE_CLK),
					odin_crg_get_dss_clk_ena(CRG_OTHER_CLK));

			/* restore clk status & retry update first packet */
			if (check_clk_gating_status())
				release_clk_status(0);

			enable_runtime_clk(dssdev->manager, false);

			_mipi_dsi_int0_unmask_irq(resource_index,
						  DSI_INT0_HS_DATA_COMPLETE);
			odin_crg_set_interrupt_mask(CRG_IRQ_MIPI0, true);
			dsi_dev.first_pkt_timeout = true;
			return 0;
#if 0
			mipi_build_hs_package(
				resource_index ,DSI_SRC_DATA_ID_DCS_LONG_WRITE, byte_cnt,
				MIPI_HS_EOTP_DISABLE, ODIN_DSI_HS_MODE_1, MIPI_DSI_LONG_PACKET, false);

			retry_cnt = 0;
			while (_mipi_dsi_int0_check_irq(resource_index, DSI_INT0_HS_DATA_COMPLETE) == 0)
			{
				retry_cnt++;
				udelay(1);
				if (retry_cnt > 2000) {
					DSSERR("retrying update first packet has timeout error, too\n");
					DSSERR("core_clk:0x%x, other_clk:0x%x\n",
						odin_crg_get_dss_clk_ena(CRG_CORE_CLK),
						odin_crg_get_dss_clk_ena(CRG_OTHER_CLK));
					check_clk_gating_status();
					return 0;
				}
			}
#endif
		}
	}

	dsi_dev.first_pkt_timeout = false;
	mipi_dsi_set_state(MIPI_SECOND_START);
	mipi_dsi_clear_hs_complete(resource_index);
	_mipi_dsi_int0_unmask_irq(resource_index, DSI_INT0_HS_DATA_COMPLETE);
	odin_crg_set_interrupt_mask(CRG_IRQ_MIPI0, true);

	odin_mipi_cmd_loop_set(resource_index, true, loop_cnt);
	mipi_hs_cmd_init_data(
		resource_index, MIPI_DSI_DCS_CMD_MEMORY_WRITE_CONTINUE);

	mipi_build_hs_package(
		resource_index ,DSI_SRC_DATA_ID_DCS_LONG_WRITE, byte_cnt,
		MIPI_HS_EOTP_ENABLE, ODIN_DSI_HS_MODE_1,
		MIPI_DSI_LONG_PACKET, false);

	return 0;
}

u32 odin_mipi_hs_command_sync(enum odin_mipi_dsi_index resource_index)
{
	odin_du_set_channel_sync_enable(resource_index, 0);
	odin_du_set_channel_sync_enable(resource_index, 1);

	INIT_COMPLETION(dsi_dev.framedone_completion);

	return 0;
}

#else /* DSS_THREAD_COMMAND_SEND */

u32 odin_mipi_hs_command_update_loop(enum odin_mipi_dsi_index resource_index,
	struct odin_dss_device *dssdev)
{
	unsigned long timeout;

	void mipi_irq_wait_handler(void *data, u32 mask, u32 sub_mask)
	{
		enum odin_channel channel;
		bool mipi_state;

		if (mask & CRG_IRQ_MIPI1)
			channel = ODIN_DSS_CHANNEL_LCD1;
		else
			channel = ODIN_DSS_CHANNEL_LCD0;

		mipi_state = odin_mipi_cmd_loop_get(channel);

		if ((!mipi_state))
			complete((struct completion *)data);
	}

	u32 max_pkt_size = dssdev->phy.dsi.cmd_max_size;
	u32 data_size = dssdev->phy.dsi.frame_size;
	u32 byte_cnt;
	u32 retry_cnt = 0;
	u32 cnt = 0;
	unsigned long flags;
	int r;
	int loop_cnt;

	loop_cnt = (data_size / (max_pkt_size-1)) - 1;
	byte_cnt = max_pkt_size;

	spin_lock_irqsave(&dsi_dev.irq_lock, flags);
	dsi_dev.updating = true;
	spin_unlock_irqrestore(&dsi_dev.irq_lock, flags);

	mutex_lock(&mtx);

	DECLARE_COMPLETION_ONSTACK(completion);

	odin_du_set_channel_sync_enable(resource_index, 0);

	odin_mipi_cmd_loop_set(resource_index, false, loop_cnt);
	mipi_hs_cmd_init_data(resource_index, MIPI_DSI_DCS_CMD_MEMORY_WRITE);
	odin_du_set_channel_sync_enable(resource_index, 1);

	r = odin_crg_register_isr(mipi_irq_wait_handler, &completion,
			CRG_IRQ_MIPI0, CRG_IRQ_MIPI_HS_DATA_COMPLETE);

	mipi_build_hs_package(
		resource_index ,DSI_SRC_DATA_ID_DCS_LONG_WRITE, byte_cnt,
		MIPI_HS_EOTP_DISABLE, ODIN_DSI_HS_MODE_1,
		MIPI_DSI_LONG_PACKET, true);

	timeout = wait_for_completion_interruptible_timeout(&completion,
			msecs_to_jiffies(100));

	if (timeout <= 0)
	{
		DSSERR("command first packet is timeout = %d \n", timeout);
		complete((struct completion *)&completion);
	}

	r = odin_crg_unregister_isr(
		mipi_irq_wait_handler, &completion,
				CRG_IRQ_MIPI0, DSI_INT0_HS_DATA_COMPLETE);

#if 0 /*polling */
	while (_mipi_dsi_int0_check_irq(resource_index, DSI_INT0_HS_DATA_COMPLETE)
			== 0)
	{
		retry_cnt++;
		udelay(1); /* wait 1usec */
	}
#endif

	r = odin_crg_register_isr(
		mipi_dsi_disable_isr, &dsi_dev.dsi_state, /*updating,*/
				CRG_IRQ_MIPI0, DSI_INT0_HS_DATA_COMPLETE);

	odin_mipi_cmd_loop_set(resource_index, true, loop_cnt);
	mipi_hs_cmd_init_data(
		resource_index, MIPI_DSI_DCS_CMD_MEMORY_WRITE_CONTINUE);

	mipi_build_hs_package(
		resource_index ,DSI_SRC_DATA_ID_DCS_LONG_WRITE, byte_cnt,
		MIPI_HS_EOTP_DISABLE, ODIN_DSI_HS_MODE_1,
		MIPI_DSI_LONG_PACKET, true);

	mutex_unlock(&mtx);
#if 0
	if (!wait_for_completion_timeout(&data_completion,
				msecs_to_jiffies(100)))
		DSSERR("timeout waiting for DATA DONE\n");

	r = odin_crg_unregister_isr(mipi_dsi_disable_isr,
			&data_completion, CRG_IRQ_MIPI0, DSI_INT0_HS_DATA_COMPLETE);

#if 1
	retry_cnt = 0;
	while (odin_dipc_get_lcd_cnt(resource_index) != 0x0)
	{
		odin_dipc_fifo_flush(resource_index);
		if (retry_cnt++ > 100)
		{
			DSSERR("timeout waiting for dipc fifo flush\n");
			break;
		}
		udelay(1);
	}

	retry_cnt = 0;
	while (odin_mipi_dsi_check_tx_fifo(resource_index) != 0x0)
	{
		odin_mipi_dsi_fifo_flush_all(resource_index);
		if (retry_cnt++ > 100)
		{
			DSSERR("timeout waiting for mipi fifo flush\n");
			break;
		}
		udelay(1);
	}
#endif

	odin_du_set_channel_sync_enable(resource_index, 0);
#endif

	return 0;
}

#endif

u32 odin_mipi_hs_command_update(enum odin_mipi_dsi_index resource_index,
	struct odin_dss_device *dssdev)
{
	u32 max_pkt_size = dssdev->phy.dsi.cmd_max_size;
	u32 data_size = dssdev->phy.dsi.frame_size;
	u32 byte_cnt;
	u32 cnt = 0;
	int r;
	struct completion data_completion;

	odin_du_set_channel_sync_enable(resource_index, 1);

	mipi_hs_cmd_init_data(resource_index, MIPI_DSI_DCS_CMD_MEMORY_WRITE);

	while (data_size > 0)
	{
		cnt++;

		if (data_size < max_pkt_size)
		{
			byte_cnt = data_size + 1;
			data_size  = 0;
		}
		else
		{
			byte_cnt = max_pkt_size;
			data_size -= max_pkt_size - 1;
		}

		init_completion(&data_completion);
		r = odin_crg_register_isr(
			mipi_dsi_disable_isr, &data_completion,
			CRG_IRQ_MIPI0, DSI_INT0_HS_DATA_COMPLETE);
		mipi_build_hs_package(
			resource_index ,DSI_SRC_DATA_ID_DCS_LONG_WRITE, byte_cnt,
			MIPI_HS_EOTP_DISABLE, ODIN_DSI_HS_MODE_1,
			MIPI_DSI_LONG_PACKET, true);


		if (!wait_for_completion_timeout(&data_completion,
					msecs_to_jiffies(100)))
			DSSERR("timeout waiting for DATA DONE\n");

		r = odin_crg_unregister_isr(mipi_dsi_disable_isr,
				&data_completion, CRG_IRQ_MIPI0,
				DSI_INT0_HS_DATA_COMPLETE);



		if (r < 0)
			DSSERR("mipi is timeout = %d \n", cnt);

		/*DSSINFO("Retry counter to wait HS Data Complete: %d\n", retry_cnt);*/
		if (data_size > 0)
		{
			mipi_hs_cmd_init_data(
				resource_index,
				MIPI_DSI_DCS_CMD_MEMORY_WRITE_CONTINUE);
		}
	}

	DSSDBG("Retry counter to wait HS Data Complete: %d\n", cnt);

	do {
		odin_dipc_fifo_flush(resource_index);
		mdelay(1);
	} while (odin_dipc_get_lcd_cnt(resource_index) != 0x0);

	do {
		odin_mipi_dsi_fifo_flush_all(resource_index);
		mdelay(1);
	} while (odin_mipi_dsi_check_tx_fifo(resource_index) != 0x0);

	odin_du_set_channel_sync_enable(resource_index, 0);

	return 0;
}

int mipi_dip_fifo_flush(struct odin_dss_device *dssdev)
{
	int ret_dip = 0;
	int ret_mipi = 0;
	u32 retry_cnt = 0;
	enum odin_mipi_dsi_index resource_index =
		(enum odin_mipi_dsi_index) dssdev->channel;

	while (odin_dipc_get_lcd_cnt(resource_index) != 0x0)
	{
		odin_dipc_fifo_flush(resource_index);
		if (retry_cnt++ > 1000)
		{
			DSSERR("timeout waiting for dipc fifo flush\n");
			ret_dip = -EPIPE;
			break;
		}
		udelay(1);
	}

	retry_cnt = 0;

	while (odin_mipi_dsi_check_tx_fifo(resource_index) != 0x0)
	{
		odin_mipi_dsi_fifo_flush_all(resource_index);
		if (retry_cnt++ > 1000)
		{
			DSSERR("timeout waiting for mipi fifo flush\n");
			ret_mipi = -EPIPE;
			break;
		}
		udelay(1);
	}

	if ((ret_dip != 0) && (ret_mipi != 0))
		DSSINFO("No Panel \n");

	if ((ret_dip != 0) || (ret_mipi != 0))
		return -EPIPE;
	else
		return 0;
}

int mipi_hs_command_pending(struct odin_dss_device *dssdev)
{
	int r = 0;
	bool updating_error = false;
	unsigned long flags;
	/* bool reset_needed = false; */
	u32 reset_mask = 0, ovl_mask = 0;
	int i;

	mipi_dsi_set_state(MIPI_PENDING_PROCESSING);

	spin_lock_irqsave(&dsi_dev.irq_lock, flags);
	if ((dsi_dev.updating) || (atomic_read(&vsync_ready)))
	{
		dsi_dev.updating = false;
		spin_unlock_irqrestore(&dsi_dev.irq_lock, flags);
		dsi_dev.first_pkt_timeout = false;

#ifdef DSS_INTERRUPT_COMMAND_SEND
		spin_lock_irqsave(&dsi_dev.irq_lock, flags);
		dsi_dev.pkt_state = 3;
		spin_unlock_irqrestore(&dsi_dev.irq_lock, flags);

		atomic_set(&vsync_ready, 0);
#else
		r = odin_crg_unregister_isr(
				mipi_dsi_disable_isr, &dsi_dev.dsi_state,
				CRG_IRQ_MIPI0, DSI_INT0_HS_DATA_COMPLETE);
#endif
#if 0
#ifdef DSS_DFS_MUTEX_LOCK
		mutex_unlock(&dfs_mtx);
#elif defined(DSS_DFS_SPINLOCK)
		put_mem_dfs();
#endif
#endif
		complete(&dsi_dev.framedone_completion);
		/*updating_error = true;*/

		odin_crg_mipi_set_err_crg_mask(dssdev->channel);
		odin_crg_underrun_process_thread(dssdev->channel, true);
	}
	else
		spin_unlock_irqrestore(&dsi_dev.irq_lock, flags);

	if (odin_crg_get_error_handling_status(dssdev->channel))
		r = -EPIPE;
	else
		r = mipi_dip_fifo_flush(dssdev);

#if 0 /*RUNTIME_CLK_GATING*/
	if (updating_error)
		disable_runtime_clk(dssdev->manager->id);
#endif

	return r;

}

unsigned int odin_mipi_dsi_get_irq_mask(enum odin_mipi_dsi_index resource_index)
{
	return _mipi_dsi_int0_get_mask_irq(resource_index);
}

unsigned int odin_mipi_dsi_irq_clear(
	enum odin_mipi_dsi_index resource_index, u32 src0_ix)
{
	_mipi_dsi_int0_clear_irq(resource_index, src0_ix);
	return 0;
}

unsigned int odin_mipi_dsi_irq_mask(
	enum odin_mipi_dsi_index resource_index, u32 reg)
{
	dsi_write_reg(resource_index, ADDR_DSI_INTERRUPT_MASK0, reg);
	return 0;
}

unsigned int odin_mipi_dsi_get_irq_status(
	enum odin_mipi_dsi_index resource_index)
{
	u32 val;
	val = _mipi_dsi_int0_get_irq(resource_index);
	return val;
}

#ifdef CONFIG_OF
extern struct device odin_device_parent;

static struct of_device_id odindss_mipidsi_match[] = {
	{
		.compatible = "odindss-dsi",
	},
	{},
};
#endif


u32 odin_mipi_dsi_cmd(
	enum odin_mipi_dsi_index resource_index,
	struct odin_dsi_cmd *dsi_cmd, u32 num_of_cmd)
{
	int i;
	u16 delay;

	for (i = 0; i < num_of_cmd; i++)
	{
		if (dsi_cmd->cmd_type == DSI_DELAY)
		{
			delay = dsi_cmd->sp_len_dly.delay_ms;
			mdelay(delay);
			DSSDBG("DSI_DELAY : %d ms \n", delay);
		}
		else if (dsi_cmd->cmd_type == LONG_CMD_MIPI)
		{
			mipi_dsi_send_long_packet(
				resource_index, dsi_cmd->data_id,
				dsi_cmd->sp_len_dly.data_len, dsi_cmd->pdata);

			DSSDBG("LONG_CMD_MIPI : id:0x%x data_len:%x data:%x \n",
				dsi_cmd->data_id, dsi_cmd->sp_len_dly.data_len,
				dsi_cmd->pdata);
		}
		else if (dsi_cmd->cmd_type == SHORT_CMD_MIPI)
		{
			mipi_dsi_send_short_packet(
				resource_index, dsi_cmd->data_id,
				dsi_cmd->sp_len_dly.sp.data0,
				dsi_cmd->sp_len_dly.sp.data1);

			DSSDBG("SHORT_CMD_MIPI : id:0x%x data0:0x%x data1:%x \n",
				dsi_cmd->data_id, dsi_cmd->sp_len_dly.sp.data0,
				dsi_cmd->sp_len_dly.sp.data1);
		}
		dsi_cmd++;
	}

	return 0;

}

void mipi_dsi_calculate_time_info(
	struct odin_mipi_dsi_video_timing_info *dsi_time,
	struct odin_video_timings *panel_time)
{
	u32 psize;
	u32 cnthsa, cnthfp, cnthbp, cntbllp;

	switch (panel_time->data_width)
	{
		case 24:
		case 18:
			psize = 3;
			break;
		case 16:
			psize = 2;
			break;
		default:
			psize = 3;
			break;
	}

	cnthsa = panel_time->hsw * psize;
	cnthfp = panel_time->hfp * psize;
	cnthbp = panel_time->hbp * psize;
	cntbllp = (cnthbp + 6) + (panel_time->x_res * psize + 6) + (cnthfp + 6);

	dsi_time->hsa_count = cnthsa - 6;
	dsi_time->hfp_count = cnthfp - 6;
	dsi_time->hbp_count = cnthbp - 6;
	dsi_time->bllp_count = cntbllp - 6;

	dsi_time->num_vsa_lines = panel_time->vsw;
	dsi_time->num_vfp_lines = panel_time->vfp;
	dsi_time->num_vbp_lines = panel_time->vbp;

	dsi_time->num_bytes_lines = panel_time->x_res * psize;
	dsi_time->num_vact_lines = panel_time->y_res;
	dsi_time->full_sync = 1;
}

void mipi_dsi_set_hs_cmd_size(struct odin_dss_device *dssdev)
{
	u32 max_pkt_size;
	struct odin_video_timings *timings = &dssdev->panel.timings;
	u32 line_cnt = timings->x_res * (timings->data_width >> 3);
#ifdef MIPI_PKT_SIZE_CONTROL
	max_pkt_size = 1944;
#else
	/* Trim it based on size of a line from max packet size.*/
	max_pkt_size = 0xffff - (0xffff % line_cnt);
#endif

	/* Add a size of initial data byte*/
	if (max_pkt_size < (0xffff - 1) )
		max_pkt_size += 1;
	else
		max_pkt_size = max_pkt_size - line_cnt + 1;

	dssdev->phy.dsi.cmd_max_size = max_pkt_size;
	dssdev->phy.dsi.frame_size =  line_cnt * timings->y_res;
}

void odin_mipi_dsi_panel_config(
	enum odin_mipi_dsi_index resource_index, struct odin_dss_device *dssdev)
{
	struct odin_mipi_dsi_video_timing_info dsi_time_info;

	mipi_dsi_calculate_time_info(&dsi_time_info, &dssdev->panel.timings);
	mipi_dsi_video_mode_timming(resource_index, &dsi_time_info);

	_mipi_dsi_configuration0(resource_index,
		ID_DSI_CON0_OUTPUT_FORMAT, dssdev->panel.dsi_pix_fmt);
	mipi_dsi_phy_lane_clk_swap(resource_index, dssdev->panel.timings.pclk_pol);
}

void odin_mipi_dsi_set_hs_cmd_size(struct odin_dss_device *dssdev)
{
	mipi_dsi_set_hs_cmd_size(dssdev);

	/* move to another */
#ifdef DSS_INTERRUPT_COMMAND_SEND
	if (dssdev->panel.dsi_mode == ODIN_DSS_DSI_CMD_MODE)
	{
		if (dsi_dev.first_booting == false) /* booting first */
		{
			odin_crg_register_isr(mipi_dsi_sending_isr,
					      &dsi_dev.dsi_state,
					      CRG_IRQ_MIPI0,
					      CRG_IRQ_MIPI_HS_DATA_COMPLETE);
			init_completion(&dsi_dev.framedone_completion);

			mipi_dsi_vsync_register_isr(mipi_dsi_vsync_update_isr,
						    dssdev);

			dsi_dev.first_booting = true;
		}
		else	/* suspend/resume */
			odin_crg_register_isr(mipi_dsi_sending_isr,
					      &dsi_dev.dsi_state,
					      CRG_IRQ_MIPI0,
					      CRG_IRQ_MIPI_HS_DATA_COMPLETE);
	}
#endif
}

void odin_mipi_dsi_unregister_irq(struct odin_dss_device *dssdev)
{
#ifdef DSS_INTERRUPT_COMMAND_SEND
	if (dsi_dev.first_booting == true)
	{
		odin_crg_unregister_isr(mipi_dsi_sending_isr,
			&dsi_dev.dsi_state,
			CRG_IRQ_MIPI0, CRG_IRQ_MIPI_HS_DATA_COMPLETE);
	}
#endif
}

void odin_mipi_dsi_init(struct odin_dss_device *dssdev)
{
	enum odin_mipi_dsi_index resource_index = dssdev->channel;

	mipi_dsi_mask_all_irqs(resource_index);
	mipi_dsi_clear_all_irqs(resource_index);

//	mipi_dsi_d_phy_set(dssdev);
	MIPI_INFO("g_mipi_dsi_active : %d \n", g_mipi_dsi_active);
	if (g_mipi_dsi_active == 1){
		mipi_dsi_d_phy_reset(dssdev);
	}
	else if (g_mipi_dsi_active == 2){
		MIPI_INFO("MIPI DSI Clock Show Active \n");
	}
	else
		mipi_dsi_d_phy_set(dssdev);

	mipi_dsi_cfg0_rx_long_ena(
				resource_index, ID_DSI_CON0_RX_LONG_ENA3,
				MIPI_DSI_SHORT_PACKET);
	mipi_dsi_cfg0_rx_long_ena(
				resource_index, ID_DSI_CON0_RX_LONG_ENA2,
				MIPI_DSI_SHORT_PACKET);
	mipi_dsi_cfg0_rx_long_ena(
				resource_index, ID_DSI_CON0_RX_LONG_ENA1,
				MIPI_DSI_SHORT_PACKET);
	mipi_dsi_cfg0_rx_long_ena(
				resource_index, ID_DSI_CON0_RX_LONG_ENA0,
				MIPI_DSI_SHORT_PACKET);

	_mipi_dsi_configuration0(
				resource_index, ID_DSI_CON0_SWAP_INPUT, 0);
	_mipi_dsi_configuration0(
				resource_index, ID_DSI_CON0_INPUT_FORMAT,
				ODIN_DSS_DSI_IN_FMT_RGB24);
}

void odin_mipi_dsi_fifo_flush_all(enum odin_mipi_dsi_index resource_index)
{
	mipi_dsi_fifo_flush_all(resource_index);
}

void odin_mipi_dsi_phy_power(
	enum odin_mipi_dsi_index resource_index, bool enable)
{
	mipi_dsi_cfg_phy_tx_lane_power(resource_index, enable);
}

static irqreturn_t mipi_dsi_vsync_irq(int irq, void *data)
{
	int i;
	struct mipi_dsi_isr_data registered_isr[VSYNC_MAX_NR_ISRS];
	struct mipi_dsi_isr_data *isr_data;

	/* make a copy and unlock, so that isrs can unregister
	 * themselves */

	spin_lock(&dsi_dev.vsync_irq_lock);

	memcpy(registered_isr, dsi_dev.registered_isr,
			sizeof(registered_isr));

	spin_unlock(&dsi_dev.vsync_irq_lock);

	for (i = 0; i < VSYNC_MAX_NR_ISRS; i++) {
		isr_data = &registered_isr[i];

		if (!isr_data->isr)
			continue;
		isr_data->isr(isr_data->arg);
	}

	return IRQ_HANDLED;
}

static ssize_t mipi_dsi_phy_show(struct device *dev, struct
	device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", 0);
}

static ssize_t  mipi_dsi_phy_store(struct device *dev, struct
	device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	char op, i;
	char r[3];
	char v[3];
	unchar tmp;
	int id, reg, val = 0 ;
	struct odin_overlay_manager *mgr;

	if(count != 7 && count != 5){
		DSSINFO("cnt:%d, invalid input!\n", count-1);
		DSSINFO("ex) 05df   -> op:0(read)  dsi#:0 reg:0xdf \n");
		DSSINFO("ex) 15df5f -> op:1(wirte) dsi#:0 reg:0xdf val:0x5f\n");
		return -EINVAL;
	}

	ret = snprintf(&op,2,buf);
	ret = snprintf(&i,2,buf+1);
	ret = snprintf(r,3,buf+2);

	id = simple_strtoul(&i,NULL,10);
	reg = simple_strtoul(r,NULL,16);

	if ((id != 0 && id != 1)){
		DSSINFO("invalid addr id! (id:0,1)\n");
		return -EINVAL;
	}

	mgr = odin_mgr_dss_get_overlay_manager(id);

	switch(op){
		case 0x30: /* "0" -> read */
			enable_runtime_clk(mgr, false);
			tmp = mipi_dsi_tif_read(id, reg);
			disable_runtime_clk(id);
			DSSINFO("mipi phy read(0x%x,0x%x)= 0x%x \n", id, reg, tmp);
			break;

		case 0x31: /* "1" -> write */
			ret = snprintf(v,3,buf+4);
			val = simple_strtoul(v,NULL,16);

			enable_runtime_clk(mgr, false);
			mipi_dsi_tif_write(id, reg, val);
			tmp = mipi_dsi_tif_read(id, reg);
			disable_runtime_clk(id);
			DSSINFO("mipi phy write(0x%x,0x%x,0x%x)\n", id, reg, tmp);
			break;

		default:
			DSSINFO("invalid operation code! (0:read, 1:write)\n");
			return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR( mipi_phy_reg, 0664,
		    mipi_dsi_phy_show,
		    mipi_dsi_phy_store );

/* MIPI DSI HW IP initialisation */
static int mipi_dsi_probe(struct platform_device *pdev)
{
#ifndef CONFIG_OF
	struct resource *res;
#endif
	int ret;
	int size, i;
	int irq;

	unsigned long irq_flags;

	DSSINFO("dsi_probe\n");

#ifdef CONFIG_OF
	/*const struct of_device_id *of_id;*/
	/*of_id = of_match_device(odindss_mipidsi_match, &pdev->dev);*/
	pdev->dev.parent = &odin_device_parent;
#endif

	memset(&dsi_dev, 0x0, sizeof(dsi_dev));

	spin_lock_init(&dsi_dev.irq_lock);
	spin_lock_init(&dsi_dev.vsync_irq_lock);
	spin_lock_init(&vsync_ctrl_lock);

	for (i = 0; i < VSYNC_MAX_NR_ISRS; i++)
		spin_lock_init(&dsi_dev.registered_isr[i].completion_lock);

	for (i = 0; i < MAX_DSI_DEVICE; i++)
	{
#ifdef CONFIG_OF
		dsi_dev.base[i] = of_iomap(pdev->dev.of_node, i);
		if (dsi_dev.base[i] == 0)
		{
			DSSERR("can't ioremap dsi\n");
			ret = -ENOMEM;
			size = 0x3000;
			goto free_memory_region;
		}
		DSSDBG("dsi-base:%x\n", dsi_dev.base[i]);
#else
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (res == NULL) {
			dev_err(&pdev->dev, "no resource %d for device\n", i);
			ret = -ENXIO;
			goto out;
		}

		/* request memmory region*/
		size = res->end - res->start + 1;
		dsi_dev.iomem[i] = request_mem_region(
					res->start, size, pdev->name);
		if (dsi_dev.iomem[i] == NULL) {
			dev_err(&pdev->dev, "failed to request memory region\n");
			ret = -ENOENT;
			goto out;
		}

		dsi_dev.base[i] = ioremap(res->start, size);
		DSSDBG("remap dip-%d base to ox%08x\n", i, (unsigned int)dsi_dev.base[i]);

		if (dsi_dev.base[i] == NULL) {
			dev_err(&pdev->dev, "failed to remap to memory\n");
			ret = -ENXIO;
			goto free_memory_region;
		}
#endif
	}

#ifdef CONFIG_PANEL_LGLH600WF2 /*VYSNC_GPIO */

	dsi_dev.vsync_gpio = LCD_MIPI_VSYNC;
	dsi_dev.vsync_gpio_name = "vsync_gpio";

	ret = gpio_request(dsi_dev.vsync_gpio, dsi_dev.vsync_gpio_name);

	if (ret < 0)
	{
		DSSERR("can't gpio request\n");
		return -ENODEV;
	}

	ret = gpio_direction_input(dsi_dev.vsync_gpio);
	if (ret < 0)
	{
		DSSERR("can't gpio setting error\n");
		goto fail_gpio;
	}

	irq = gpio_to_irq(dsi_dev.vsync_gpio);
	if (irq < 0) {
		ret = irq;
		DSSERR("gpio irq error\n");
		goto fail_gpio;
	}

	irq_flags = IRQF_TRIGGER_RISING | IRQF_ONESHOT;

	dsi_dev.irq = irq;
	dsi_dev.irq_flags = irq_flags;
	dsi_dev.irq_enabled = true;

	ret = request_irq(irq,
					mipi_dsi_vsync_irq,
					irq_flags,
					dsi_dev.vsync_gpio_name,
					NULL);

	if (ret < 0)
	{
		DSSERR("resquest_thread_irq is failed \n");
		goto fail_gpio;
	}

#endif

	ret = device_create_file(&pdev->dev, &dev_attr_mipi_phy_reg);
	if (ret) {
		pr_err("%s: failed to create sysfs files\n", __func__);
		goto fail_gpio;
	}

	platform_set_drvdata(pdev, &dsi_dev);
	dsi_dev.pdev = pdev;

	return 0;

free_memory_region:
	for (i = 0; i < MAX_DSI_DEVICE-1; i++)
		release_mem_region(dsi_dev.iomem[i], size);

fail_gpio:
	gpio_free(dsi_dev.vsync_gpio);

#ifndef CONFIG_OF
out:
#endif
	return ret;
}

static int mipi_dsi_remove(struct platform_device *pdev)
{
	int size;
	int i;

	dev_dbg(KERN_INFO, "dsi_remove: free_memory_region\n");
	for (i=0; i<NUM_DIPC_BLOCK-1; i++) {
		if (dsi_dev.base[i]) {
			iounmap(dsi_dev.base[i]);
			dsi_dev.base[i] = NULL;
		}

		if (dsi_dev.iomem[i]) {
			size =  dsi_dev.iomem[i]->end -
				dsi_dev.iomem[i]->start + 1;
			release_mem_region(dsi_dev.iomem[i]->start, size);
			dsi_dev.iomem[i] = NULL;
		}
	}
	return 0;
}

static int mipi_dsi_runtime_suspend(struct device *dev)
{
	return 0;
}

static int mipi_dsi_runtime_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops dispc_pm_ops = {
	.runtime_suspend = mipi_dsi_runtime_suspend,
	.runtime_resume = mipi_dsi_runtime_resume,
};

static struct platform_driver odin_mipi_dsi_driver = {
	.probe          = mipi_dsi_probe,
	.remove         = mipi_dsi_remove,
	.driver         = {
		.name   = "odindss-dsi",
		.owner  = THIS_MODULE,
		/*.pm	= &dispc_pm_ops,*/
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odindss_mipidsi_match),
#endif
	},
};

int odin_mipi_dsi_init_platform_driver(void)
{
	return platform_driver_register(&odin_mipi_dsi_driver);
}

void odin_mipi_dsi_uninit_platform_driver(void)
{
	platform_driver_unregister(&odin_mipi_dsi_driver);
}


