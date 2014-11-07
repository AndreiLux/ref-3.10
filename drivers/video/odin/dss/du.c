/*
 * linux/drivers/video/omap2/dss/dss.c
 *
 * Copyright (C) 2009 Nokia Corporation
 * Author: Tomi Valkeinen <tomi.valkeinen@nokia.com>
 *
 * Some code and ideas taken from drivers/video/omap/ driver
 * by Imre Deak.
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

#define DSS_SUBSYS_NAME "DU"
#define ODIN_FPGA_TESTBED

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
#include <linux/export.h>
#endif
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#include <video/odindss.h>
#include "dss.h"
#include "du.h"
#include "../xdengine/mxd.h"
#include "../display/panel_device.h"
#include "cabc/cabc.h"

#include "vid0_reg_def.h"
#include "vid0_union_reg.h"
#include "vid1_reg_def.h"
#include "vid1_union_reg.h"
#include "vid2_reg_def.h"
#include "vid2_union_reg.h"
#include "gra0_reg_def.h"
#include "gra0_union_reg.h"
#include "gra1_reg_def.h"
#include "gra1_union_reg.h"
#include "gra2_reg_def.h"
#include "gra2_union_reg.h"
#include "ovl_reg_def.h"
#include "ovl_union_reg.h"
#include "sync_gen_reg_def.h"
#include "sync_gen_union_reg.h"
#include "cabc/cabc_reg_def.h"
#include "cabc/cabc_union_reg.h"
#include "gscl_reg_def.h"
#include "gscl_union_reg.h"

#define DU_MANUAL_MODE_RASTER_X_SIZE 5000
#define DU_MANUAL_MODE_RASTER_Y_SIZE 2

#define DU_SZ_REGS			SZ_4K

#define REG_GET(idx, start, end) \
	FLD_GET(dispc_read_reg(idx), start, end)

#define REG_FLD_MOD(idx, val, start, end)				\
	dispc_write_reg(idx, FLD_MOD(dispc_read_reg(idx), val, start, end))

/* switch(diagram) - RDMA path sel : scaler or xd nr */
typedef enum{
    DU_VID_XDNR_PATH_DISABLE = 0, /* internal scaler path */
    DU_VID_XDNR_PATH_ENABLE,
}eDU_VID_XDNR_PATH;

/*  mux before scaler - scaler input path sel : RDMA or xd nr output */
typedef enum{
    DU_VID_SCALE_IN_PATH_RDMA = 0,
    DU_VID_SCALE_IN_PATH_NR,
}eDU_VID_SCALE_IN_PATH;

/*  mux after scaler - scaler output path sel : vid output or xd re input */
typedef enum{
    DU_VID_SCALE_OUT_PATH_VIDCH = 0,   /*  vid read ch output */
    DU_VID_SCALE_OUT_PATH_RE_IN,       /*   xd re */
}eDU_VID_SCALE_OUT_PATH;

/* mux before vid output - vid read ch output sel :
	scaler output or xd re output */
typedef enum{
    DU_VID_OUT_PATH_SCALER_OUT = 0,
    DU_VID_OUT_PATH_RE_OUT,
}eDU_VID_OUT_PATH;

/* mux before wb input - wb input source path sel : vid 0 or vid 2 */
typedef enum{
    DU_WB_IN_PATH_VID0 = 0,
    DU_WB_IN_PATH_VID2,
}eDU_WB_IN_PATH;

enum wb_color_mode {
	VID_WB_COLOR_RGB565		= 0x0,  /* RGB565 */
	VID_WB_COLOR_RGB555		= 0x1,  /* RGB555 - global alpha setting */
	VID_WB_COLOR_RGB444		= 0x2,  /* RGB444 - global alpha setting */
	VID_WB_COLOR_RGB888		= 0x3,  /* RGB888 - global alpha setting */
	VID_WB_YUV422S	 		= 0x4,  /* YUV422 STREAM*/
	VID_WB_NOT_SUPPORTED	= 0x5,  /* RESERVED */
	VID_WB_YUV420P_2 		= 0x6,  /* YUV420 2PLANE */
};

#if 0
static struct {
	struct platform_device *pdev;
	void __iomem    *base;

} du_dev;
#endif

static struct {
	struct platform_device *pdev;
	void __iomem    *base[ODIN_DSS_MAX_RES];

	int		ctx_loss_cnt;
	struct mutex	runtime_lock;
	int		runtime_count;

	struct clk *dss_clk;

	u32	fifo_size[MAX_DSS_OVERLAYS];

	u32	channel_irq[3]; /* Max channels hardcoded to 3*/

	bool		ctx_valid;
	u32		ctx[DU_SZ_REGS / sizeof(u32)];

	bool overlay_enabled_status[MAX_DSS_OVERLAYS];
	u32 enabled_plane_mask[MAX_DSS_MANAGERS];
	enum odin_overlay_src_num overlay_src_num[MAX_DSS_OVERLAYS]; /* zorder */
	u32 channel_offset[MAX_DSS_MANAGERS];
	u8 mxd_plane_mask;
	u8 mxd_enable;
	ktime_t vsync_time;
	spinlock_t sync_lock;
} du;

int mxd_use_plane;

/*******************************DU Register Control************************/
#if 0
#define DU_OVL_LCD0_OFFSET		0
#define DU_OVL_LCD1_OFFSET		0x100
#define DU_OVL_HDMI_OFFSET		0x200
#endif

static inline void dss_write_reg(enum odin_resource_index resource_index,
	enum odin_channel channel, u32 addr, u32 val)
{
	__raw_writel(val, du.base[resource_index] \
		+ du.channel_offset[channel] + addr);
}

static inline u32 dss_read_reg(enum odin_resource_index resource_index,
	enum odin_channel channel, u32 addr)
{
	return __raw_readl(du.base[resource_index] \
		+  du.channel_offset[channel] + addr);
}

static u32 dss_get_fb_num_offset(u8 fb_num)
{
	if (fb_num == 0)
		return 0x0;
	else if (fb_num == 1)
		return 0xc;
	else
		return 0x18;
}

#define DSS_REG_MASK(idx) \
	(((1 << ((DU_##idx##_START) - (DU_##idx##_END) + 1)) - 1) << \
	(DU_##idx##_END))
#define DSS_REG_VAL(val, idx) ((val) << (DU_##idx##_END))
#define DSS_REG_GET(val, idx) (((val) & DSS_REG_MASK(idx)) >> (DU_##idx##_END))
#define DSS_REG_MOD(orig, val, idx) \
    (((orig) & ~DSS_REG_MASK(idx)) | DSS_REG_VAL(val, idx))

enum vid_rd_cfg0 {
	VID_RD_SRC_REG_SW_UPDATE			= 0,
	VID_RD_SRC_SYNC_SEL				= 1,
	VID_RD_SRC_TRANS_NUM				= 2,
	VID_RD_SRC_BUF_SEL				= 3,
	VID_RD_SRC_DUAL_OP_EN				= 4,
	VID_RD_SRC_DST_SEL				= 5,
	VID_RD_SRC_EN					= 6
};

enum vid_rd_cfg1 {
	VID_RD_SRC_FLIP_MODE				= 0,
	VID_RD_SRC_SCL_BP_MODE				= 1,
	VID_RD_SRC_BND_FILL_MODE			= 2,
	VID_RD_SRC_LSB_SEL				= 3,
	VID_RD_SRC_YUV_SWAP				= 4,
	VID_RD_SRC_RGB_SWAP				= 5,
	VID_RD_SRC_ALPHA_SWAP				= 6,
	VID_RD_SRC_WORD_SWAP				= 7,
	VID_RD_SRC_BYTE_SWAP				= 8,
	VID_RD_SRC_FORMAT				= 9
};

enum vid_rd_cfg2 {
	VID_RD_VID_XD_PATH_EN				= 0,
	VID_RD_SRC_FR_XD_RE_PATH_SEL			= 1,
	VID_RD_SRC_TO_XD_RE_PATH_SEL			= 2,
	VID_RD_SRC_FR_XD_NR_PATH_SEL			= 3,
	VID_RD_SRC_TO_XD_NR_PATH_SEL			= 4
};

enum vid_rd_status {
	VID_RD_SCL_HSC_RD_STATE			= 0,
	VID_RD_SCL_HSC_WR_STATE			= 1,
	VID_RD_SCL_VSC_RD_STATE			= 2,
	VID_RD_SCL_VSC_WR_STATE			= 3,
	VID_RD_RDBIF_FMC_SYNC_STATE		= 4,
	VID_RD_RDBIF_FMC_UV_STATE		= 5,
	VID_RD_RDBIF_FMC_Y_STATE		= 6,
	VID_RD_RDBIF_CON_SYNC_STATE		= 7,
	VID_RD_RDBIF_CON_STATE_V		= 8,
	VID_RD_RDBIF_CON_STATE_U		= 9,
	VID_RD_RDBIF_CON_STATE_Y		= 10,
	VID_RD_RDBIF_EMPTY_DATFIFO_V		= 11,
	VID_RD_RDBIF_EMPTY_DATFIFO_U		= 12,
	VID_RD_RDBIF_EMPTY_DATFIFO_Y		= 13,
	VID_RD_RDBIF_FULL_DATFIFO_V		= 14,
	VID_RD_RDBIF_FULL_DATFIFO_U		= 15,
	VID_RD_RDBIF_FULL_DATFIFO_Y		= 16,
	VID_RD_RDMIF_EMPTY_DATFIFO		= 17,
	VID_RD_RDMIF_EMPTY_CMDFIFO		= 18,
	VID_RD_RDMIF_FULL_DATFIFO 		= 19,
	VID_RD_RDMIF_FULL_CMDFIFO		= 20
};

enum vid_wb_cfg0 {
	VID_WB_REG_SW_UPDATE			= 0,
	VID_WB_SYNC_SEL				= 1,
	VID_WB_BUF_SEL				= 2,
	VID_WB_ALPHA_REG_EN			= 3,
	VID_WB_EN				= 4
};

enum vid_wb_cfg1 {
	VID_WB_OVL_INFO_SRC_SEL		= 0,
	VID_WB_SRC_SEL				= 1,
	VID_WB_YUV_SWAP				= 2,
	VID_WB_RGB_SWAP				= 3,
	VID_WB_ALPHA_SWAP			= 4,
	VID_WB_WORD_SWAP			= 5,
	VID_WB_BYTE_SWAP			= 6,
	VID_WB_FORMAT				= 7
};

enum vid_wb_status {
	VID_WB_WRMIF_EMPTY_DATFIFO		= 0,
	VID_WB_WRMIF_EMPTY_CMDFIFO 		= 1,
	VID_WB_WRMIF_FULL_DATFIFO 		= 2,
	VID_WB_WRMIF_FULL_CMDFIFO 		= 3,
	VID_WB_WRCTRL_CON_SYNC_STATE 		= 4,
	VID_WB_WRCTRL_CON_V_STATE		= 5,
	VID_WB_WRCTRL_CON_U_STATE		= 6,
	VID_WB_WRCTRL_CON_Y_STATE	 	= 7,
	VID_WB_WRCTRL_EMPTY_INF_FIFOV		= 8,
	VID_WB_WRCTRL_EMPTY_INF_FIFOU		= 9,
	VID_WB_WRCTRL_EMPTY_INF_FIFOY		= 10,
	VID_WB_WRCTRL_FULL_INF_FIFOV		= 11,
	VID_WB_WRCTRL_FULL_INF_FIFOU		= 12,
	VID_WB_WRCTRL_FULL_INF_FIFOY		= 13
};

enum gra_rd_cfg0 {
	GRA_RD_SRC_REG_SW_UPDATE		= 0,
	GRA_RD_DMA_INDEX_RD_START		= 1,
	GRA_RD_SRC_SYNC_SEL			= 2,
	GRA_RD_SRC_TRANS_NUM			= 3,
	GRA_RD_SRC_BUF_SEL			= 4,
	GRA_RD_SRC_EN				= 5
};

enum gra_rd_cfg1 {
	GRA_RD_SRC_FLIP_MODE			= 0,
	GRA_RD_SRC_LSB_SEL			= 1,
	GRA_RD_SRC_RGB_SWAP			= 2,
	GRA_RD_SRC_ALPHA_SWAP			= 3,
	GRA_RD_SRC_WORD_SWAP			= 4,
	GRA_RD_SRC_BYTE_SWAP			= 5,
	GRA_RD_SRC_FORMAT			= 6
};

enum gra_rd_status {
	GRA_RD_RDBIF_FMC_SYNC_STATE		= 0,
	GRA_RD_RDBIF_FMC_Y_STATE		= 1,
	GRA_RD_RDBIF_CON_SYNC_STATE		= 2,
	GRA_RD_RDBIF_CON_STATE_Y		= 3,
	GRA_RD_RDBIF_EMPTY_DATFIFO_Y		= 4,
	GRA_RD_RDBIF_FULL_DATFIFO_Y		= 5,
	GRA_RD_RDMIF_EMPTY_DATFIFO		= 6,
	GRA_RD_RDMIF_EMPTY_CMDFIFO		= 7,
	GRA_RD_RDMIF_FULL_DATFIFO 		= 8,
	GRA_RD_RDMIF_FULL_CMDFIFO		= 9
};

enum gscl_rd_cfg0 {
	GSCL_RD_SRC_REG_SW_UPDATE		= 0,
	GSCL_RD_SRC_SYNC_SEL			= 1,
	GSCL_RD_SRC_TRANS_NUM			= 2,
	GSCL_RD_SRC_BUF_SEL			= 3,
	GSCL_RD_SRC_EN				= 4
};

enum gscl_rd_cfg1 {
	GSCL_RD_SRC_FLIP_OP			= 0,
	GSCL_RD_SRC_LSB_SEL			= 1,
	GSCL_RD_SRC_RGB_SWAP			= 2,
	GSCL_RD_SRC_ALPHA_SWAP			= 3,
	GSCL_RD_SRC_WORD_SWAP			= 4,
	GSCL_RD_SRC_BYTE_SWAP			= 5,
	GSCL_RD_SRC_FORMAT			= 6
};

enum gscl_rd_status {
	GSCL_RD_RDBIF_FMC_SYNC_STATE		= 0,
	GSCL_RD_RDBIF_FMC_Y_STATE		= 1,
	GSCL_RD_RDBIF_CON_SYNC_STATE		= 2,
	GSCL_RD_RDBIF_CON_STATE_Y		= 3,
	GSCL_RD_RDBIF_EMPTY_DATFIFO_Y		= 4,
	GSCL_RD_RDBIF_FULL_DATFIFO_Y		= 5,
	GSCL_RD_RDMIF_EMPTY_DATFIFO		= 6,
	GSCL_RD_RDMIF_EMPTY_CMDFIFO		= 7,
	GSCL_RD_RDMIF_FULL_DATFIFO 		= 8,
	GSCL_RD_RDMIF_FULL_CMDFIFO		= 9
};

enum gscl_wb_cfg0 {
	GSCL_WB_REG_SW_UPDATE			= 0,
	GSCL_WB_SYNC_SEL			= 1,
	GSCL_WB_BUF_SEL				= 2,
	GSCL_WB_ALPHA_REG_EN			= 3,
	GSCL_WB_EN				= 4
};

enum gscl_wb_cfg1 {
	GSCL_WB_OVL_INF_SRC_SEL			= 0,
	GSCL_WB_YUV_SWAP			= 1,
	GSCL_WB_RGB_SWAP			= 2,
	GSCL_WB_ALPHA_SWAP			= 3,
	GSCL_WB_WORD_SWAP			= 4,
	GSCL_WB_BYTE_SWAP			= 5,
	GSCL_WB_FORMAT				= 6
};

enum sync_gen_cfg0 {
	SYNC_SYNC2_DISP_SYNC_SRC_SEL		= 0,
	SYNC_SYNC1_DISP_SYNC_SRC_SEL		= 1,
	SYNC_SYNC0_DISP_SYNC_SRC_SEL		= 2,
	SYNC_SYNC2_INT_SRC_SEL			= 3,
	SYNC_SYNC1_INT_SRC_SEL			= 4,
	SYNC_SYNC0_INT_SRC_SEL			= 5,
	SYNC_SYNC2_INT_ENA			= 6,
	SYNC_SYNC1_INT_ENA			= 7,
	SYNC_SYNC0_INT_ENA			= 8,
	SYNC_SYNC2_MODE				= 9,
	SYNC_SYNC1_MODE				= 10,
	SYNC_SYNC0_MODE				= 11,
	SYNC_SYNC2_ENABLE			= 12,
	SYNC_SYNC1_ENABLE			= 13,
	SYNC_SYNC0_ENABLE			= 14
};


enum sync_gen_cfg1_2 {
	SYNC_GSCL_SYNC_EN0				= 0,
	SYNC_GRA2_SYNC_EN				= 1,
	SYNC_GRA1_SYNC_EN				= 2,
	SYNC_GRA0_SYNC_EN				= 3,
	SYNC_VID2_SYNC_EN				= 4,
	SYNC_VID1_SYNC_EN0				= 5,
	SYNC_VID0_SYNC_EN0				= 6,
	SYNC_OVL2_SYNC_EN				= 7,
	SYNC_OVL1_SYNC_EN				= 8,
	SYNC_OVL0_SYNC_EN				= 9,
	SYNC_GSCL_SYNC_EN1				= 10,
	SYNC_VID1_SYNC_EN1				= 11,
	SYNC_VID0_SYNC_EN1				= 12,
};

enum ovl_cfg0 {
	OVL_LAYER5_COP			= 0,
	OVL_LAYER4_COP			= 1,
	OVL_LAYER3_COP			= 2,
	OVL_LAYER2_COP			= 3,
	OVL_LAYER1_COP			= 4,
	OVL_LAYER0_COP			= 5,
	OVL_LAYER0_ROP			= 6,
	OVL_DUAL_OP_EN			= 7,
	OVL_DST_SEL			= 8,
	OVL_LAYER_OP			= 9,
	OVL_EN				= 10
};

enum ovl_cfg1 {
	OVL_LAYER5_SRC_PRE_MUL_EN		= 0,
	OVL_LAYER5_DST_PRE_MUL_EN		= 1,
	OVL_LAYER4_SRC_PRE_MUL_EN		= 2,
	OVL_LAYER4_DST_PRE_MUL_EN		= 3,
	OVL_LAYER3_SRC_PRE_MUL_EN		= 4,
	OVL_LAYER3_DST_PRE_MUL_EN		= 5,
	OVL_LAYER2_SRC_PRE_MUL_EN		= 6,
	OVL_LAYER2_DST_PRE_MUL_EN		= 7,
	OVL_LAYER1_SRC_PRE_MUL_EN		= 8,
	OVL_LAYER1_DST_PRE_MUL_EN		= 9,
	OVL_LAYER0_SRC_PRE_MUL_EN		= 10,
	OVL_LAYER0_DST_PRE_MUL_EN		= 11,
	OVL_LAYER5_CHROMAKEY_EN			= 12,
	OVL_LAYER4_CHROMAKEY_EN			= 13,
	OVL_LAYER3_CHROMAKEY_EN			= 14,
	OVL_LAYER2_CHROMAKEY_EN			= 15,
	OVL_LAYER1_CHROMAKEY_EN			= 16,
	OVL_LAYER0_CHROMAKEY_EN			= 17
};

enum ovl_cfg2 {
	OVL_SRC6_ALPHA_REG_EN			= 0,
	OVL_SRC5_ALPHA_REG_EN			= 1,
	OVL_SRC4_ALPHA_REG_EN			= 2,
	OVL_SRC3_ALPHA_REG_EN			= 3,
	OVL_SRC2_ALPHA_REG_EN			= 4,
	OVL_SRC1_ALPHA_REG_EN			= 5,
	OVL_SRC0_ALPHA_REG_EN			= 6,
	OVL_SRC6_SEL				= 7,
	OVL_SRC5_SEL				= 8,
	OVL_SRC4_SEL				= 9,
	OVL_SRC3_SEL				= 10,
	OVL_SRC2_SEL				= 11,
	OVL_SRC1_SEL				= 12,
	OVL_SRC0_SEL				= 13
};

enum ovl_cfg3 {
	OVL_SRC2_MUX_SEL			= 0
};

enum ovl_sw_update{
	OVL2_SAVE_PIP_POS			= 0,
	OVL1_SAVE_PIP_POS			= 1,
	OVL0_SAVE_PIP_POS			= 2,
	OVL2_REG_SW_UPDATE			= 3,
	OVL1_REG_SW_UPDATE			= 4,
	OVL0_REG_SW_UPDATE			= 5,
};

/*------------------------------------------------------------------------*/
/* Common of VID0, VID1, VID2*/
/* 0x000~0x0064		RD - VID0, VID1, VID2	   */
/* Exclude */
/* 0x0008	 		RD(mXD) - VID0, VID1 */
/* 0x0038, 0x003C 	RD(SCALE) - VID0, VID1 */
/*------------------------------------------------------------------------*/

static void _du_set_cabc_hist_lut_operation_mode
	(enum odin_resource_index resource_index, u8 reg_val)
{
	u32 val1,val2;

	val1 = dss_read_reg(resource_index, 0, ADDR_CABC1_DMA_CTRL);
	val1 = DSS_REG_MOD(val1, reg_val, HIST_LUT_OPERATION_MODE);
	dss_write_reg(resource_index, 0, ADDR_CABC1_DMA_CTRL, val1);


	val2 = dss_read_reg(resource_index, 0, ADDR_CABC2_DMA_CTRL);
	val2 = DSS_REG_MOD(val2, reg_val, HIST_LUT_OPERATION_MODE);
	dss_write_reg(resource_index, 0, ADDR_CABC2_DMA_CTRL, val2);
}

/* VID0,VID1,VID2 */
static void _du_vid_rd_cfg0(enum odin_resource_index resource_index,
	enum vid_rd_cfg0 id, u8 reg_val)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_CFG0);

	switch (id) {
	case VID_RD_SRC_REG_SW_UPDATE:
		/* SRC_REG_SW_UPDATE */
		val = DSS_REG_MOD(val, reg_val, SRC_REG_SW_UPDATE);
		break;
	case VID_RD_SRC_SYNC_SEL:
		val = DSS_REG_MOD(val, reg_val, SRC_SYNC_SEL);	/* SRC_SYNC_SEL */
		break;
	case VID_RD_SRC_TRANS_NUM:
		val = DSS_REG_MOD(val, reg_val, SRC_TRANS_NUM);	/* RC_TRANS_NUM */
		break;
	case VID_RD_SRC_BUF_SEL:
		val = DSS_REG_MOD(val, reg_val, SRC_BUF_SEL);	/* SRC_BUF_SEL */
		break;
	case VID_RD_SRC_DUAL_OP_EN:
		val = DSS_REG_MOD(val, reg_val, SRC_DUAL_OP_EN);/* SRC_DUAL_OP_EN */
		break;
	case VID_RD_SRC_DST_SEL:
		val = DSS_REG_MOD(val, reg_val, SRC_DST_SEL);	/* SRC_DST_SEL */
		break;
	case VID_RD_SRC_EN:
		val = DSS_REG_MOD(val, reg_val, SRC_EN);		/* SRC_EN */
		break;
	default:
		break;
	}

	dss_write_reg(resource_index, 0, ADDR_VID0_RD_CFG0, val);
}

#if 0
static void _du_vid_rd_src_reg_sw_update
	(enum odin_resource_index resource_index, u8 src_reg_sw_update)
{
	_du_vid_rd_cfg0(resource_index, VID_RD_SRC_REG_SW_UPDATE, src_reg_sw_update);
}
#endif

static void _du_vid_rd_src_sync_sel(enum odin_resource_index resource_index,
	u8 src_sync_sel)
{
	_du_vid_rd_cfg0(resource_index, VID_RD_SRC_SYNC_SEL, src_sync_sel);
}

static void _du_vid_rd_src_trans_num(enum odin_resource_index resource_index,
	u8 src_trans_num)
{
	_du_vid_rd_cfg0(resource_index, VID_RD_SRC_TRANS_NUM, src_trans_num);

}

static void _du_vid_rd_src_buf_sel(enum odin_resource_index resource_index,
	u8 src_buf_sel)
{
	_du_vid_rd_cfg0(resource_index, VID_RD_SRC_BUF_SEL, src_buf_sel);
}

static void _du_vid_rd_src_dual_op_en(enum odin_resource_index resource_index,
	u8 src_dual_op_en)
{
	_du_vid_rd_cfg0(resource_index, VID_RD_SRC_DUAL_OP_EN, src_dual_op_en);
}

static void _du_vid_rd_src_dst_sel(enum odin_resource_index resource_index,
	u8 src_dst_sel)
{
	_du_vid_rd_cfg0(resource_index, VID_RD_SRC_DST_SEL, src_dst_sel);
}

#if 0
static void _du_vid_rd_src_en(enum odin_resource_index resource_index,
	u8 src_en)
{
	_du_vid_rd_cfg0(resource_index, VID_RD_SRC_EN, src_en);
}
#endif

/* VID0_RD_CFG1 */
static void _du_vid_rd_cfg1(enum odin_resource_index resource_index,
	enum vid_rd_cfg1 id, u8 reg_val)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_CFG1);

	switch (id) {
	case VID_RD_SRC_FLIP_MODE:
		/* SRC_SCL_BP_MODE */
		val = DSS_REG_MOD(val, reg_val, SRC_FLIP_MODE);
		break;
	case VID_RD_SRC_SCL_BP_MODE:
		/* SRC_SCL_BP_MODE */
		val = DSS_REG_MOD(val, reg_val, SRC_SCL_BP_MODE);
		break;
	case VID_RD_SRC_BND_FILL_MODE:
		/* SRC_BND_FILL_MODE */
		val = DSS_REG_MOD(val, reg_val, SRC_BND_FILL_MODE);
		break;
	case VID_RD_SRC_LSB_SEL:
		/* SRC_LSB_SEL */
		val = DSS_REG_MOD(val, reg_val, SRC_LSB_SEL);
		break;
	case VID_RD_SRC_YUV_SWAP:
		/* SRC_YUV_SWAP */
		val = DSS_REG_MOD(val, reg_val, SRC_YUV_SWAP);
		break;
	case VID_RD_SRC_RGB_SWAP:
		/* SRC_RGB_SWAP */
		val = DSS_REG_MOD(val, reg_val, SRC_RGB_SWAP);
		break;
	case VID_RD_SRC_ALPHA_SWAP:
		/* SRC_ALPHA_SWAP */
		val = DSS_REG_MOD(val, reg_val, SRC_ALPHA_SWAP);
		break;
	case VID_RD_SRC_WORD_SWAP:
		/* SRC_WORD_SWAP */
		val = DSS_REG_MOD(val, reg_val, SRC_WORD_SWAP);
		break;
	case VID_RD_SRC_BYTE_SWAP:
		/* SRC_BYTE_SWAP */
		val = DSS_REG_MOD(val, reg_val, SRC_BYTE_SWAP);
		break;
	case VID_RD_SRC_FORMAT:
		/* SRC_FORMAT */
		val = DSS_REG_MOD(val, reg_val, SRC_FORMAT);
		break;
	default:
		break;
	}

	dss_write_reg(resource_index, 0, ADDR_VID0_RD_CFG1, val);
}

static void _du_vid_rd_src_flip_mode(enum odin_resource_index resource_index,
	u8 src_flip_mode)
{
	_du_vid_rd_cfg1(resource_index, VID_RD_SRC_FLIP_MODE, src_flip_mode);
}

static void _du_vid_rd_src_scl_bp_mode(enum odin_resource_index resource_index,
	u8 src_scl_bp_mode)
{
	_du_vid_rd_cfg1(resource_index, VID_RD_SRC_SCL_BP_MODE, src_scl_bp_mode);
}

static void _du_vid_rd_src_bnd_fill_mode
	(enum odin_resource_index resource_index, u8 src_bnd_fill_mode)
{
	_du_vid_rd_cfg1(resource_index, VID_RD_SRC_BND_FILL_MODE,
			src_bnd_fill_mode);
}

static void _du_vid_rd_src_lsb_sel(enum odin_resource_index resource_index,
	u8 src_lsb_sel)
{
	_du_vid_rd_cfg1(resource_index, VID_RD_SRC_LSB_SEL, src_lsb_sel);
}

static void _du_vid_rd_src_yuv_swap(enum odin_resource_index resource_index,
	u8 src_yuv_swap)
{
	_du_vid_rd_cfg1(resource_index, VID_RD_SRC_YUV_SWAP, src_yuv_swap);
}

#if 0
static void _du_vid_rd_src_rgb_swap(enum odin_resource_index resource_index,
	u8 src_rgb_swap)
{
	_du_vid_rd_cfg1(resource_index, VID_RD_SRC_RGB_SWAP, src_rgb_swap);
}

static void _du_vid_rd_src_alpha_swap(enum odin_resource_index resource_index,
	u8 src_alpha_swap)
{
	_du_vid_rd_cfg1(resource_index, VID_RD_SRC_ALPHA_SWAP, src_alpha_swap);
}
#endif

static void _du_vid_rd_src_word_swap(enum odin_resource_index resource_index,
	u8 src_word_swap)
{
	_du_vid_rd_cfg1(resource_index, VID_RD_SRC_WORD_SWAP, src_word_swap);
}

static void _du_vid_rd_src_byte_swap(enum odin_resource_index resource_index,
	u8 src_byte_swap)
{
	_du_vid_rd_cfg1(resource_index, VID_RD_SRC_BYTE_SWAP, src_byte_swap);
}

static void _du_vid_rd_src_format(enum odin_resource_index resource_index,
	u8 src_format)
{
	_du_vid_rd_cfg1(resource_index, VID_RD_SRC_FORMAT, src_format);
}


/* Case of VID0, VID1*/

/* VID0,1_RD_CFG2 */
static void _du_vid0_1_rd_cfg2(enum odin_resource_index resource_index,
	enum vid_rd_cfg2 id, u8 reg_val)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_CFG2);

	switch (id) {
	case VID_RD_VID_XD_PATH_EN:
		/* VID_XD_PATH_EN */
		val = DSS_REG_MOD(val, reg_val, VID_XD_PATH_EN);
		break;
	case VID_RD_SRC_FR_XD_RE_PATH_SEL:
		/* SRC_FR_XD_RE_PATH_SEL */
		val = DSS_REG_MOD(val, reg_val, SRC_FR_XD_RE_PATH_SEL);
		break;
	case VID_RD_SRC_TO_XD_RE_PATH_SEL:
		/* SRC_TO_XD_RE_PATH_SEL */
		val = DSS_REG_MOD(val, reg_val, SRC_TO_XD_RE_PATH_SEL);
		break;
	case VID_RD_SRC_FR_XD_NR_PATH_SEL:
		/* SRC_FR_XD_NR_PATH_SEL */
		val = DSS_REG_MOD(val, reg_val, SRC_FR_XD_NR_PATH_SEL);
		break;
	case VID_RD_SRC_TO_XD_NR_PATH_SEL:
		/* SRC_TO_XD_NR_PATH_SEL */
		val = DSS_REG_MOD(val, reg_val, SRC_TO_XD_NR_PATH_SEL);
		break;
	default:
		break;
	}

	dss_write_reg(resource_index, 0, ADDR_VID0_RD_CFG2, val);
}

static void _du_vid0_1_rd_vid_xd_path_en
	(enum odin_resource_index resource_index, u8 vid_xd_path_en)
{
	_du_vid0_1_rd_cfg2(resource_index,
			   VID_RD_VID_XD_PATH_EN, vid_xd_path_en);
}

static void _du_vid0_1_rd_src_fr_xd_re_path_sel
	(enum odin_resource_index resource_index, u8 src_fr_xd_re_path_sel)
{
	_du_vid0_1_rd_cfg2(resource_index,
		VID_RD_SRC_FR_XD_RE_PATH_SEL, src_fr_xd_re_path_sel);
}

static void _du_vid0_1_rd_src_to_xd_re_path_sel
	(enum odin_resource_index resource_index, u8 src_to_xd_re_path_sel)
{
	_du_vid0_1_rd_cfg2(resource_index,
		VID_RD_SRC_TO_XD_RE_PATH_SEL, src_to_xd_re_path_sel);
}

static void _du_vid0_1_rd_src_fr_xd_nr_path_sel
	(enum odin_resource_index resource_index, u8 src_fr_xd_nr_path_sel)
{
	_du_vid0_1_rd_cfg2(resource_index,
		VID_RD_SRC_FR_XD_NR_PATH_SEL, src_fr_xd_nr_path_sel);
}

static void _du_vid0_1_rd_src_to_xd_nr_path_sel
	(enum odin_resource_index resource_index, u8 src_to_xd_nr_path_sel)
{
	_du_vid0_1_rd_cfg2(resource_index,
		VID_RD_SRC_TO_XD_NR_PATH_SEL, src_to_xd_nr_path_sel);
}

/* VID0_RD_WIDTH_HEIGHT */
static void _du_vid_rd_width_hegiht(enum odin_resource_index resource_index,
	u16 width, u16 height)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_WIDTH_HEIGHT);

	val = DSS_REG_MOD(val, height, SRC_HEIGHT);	 /* SRC_HEIGHT */
	val = DSS_REG_MOD(val, width, SRC_WIDTH); 	/*  SRC_WIDTH */

	dss_write_reg(resource_index, 0, ADDR_VID0_RD_WIDTH_HEIGHT, val);
}

/* VID0_RD_STARTXY */
static void _du_vid_rd_startxy(enum odin_resource_index resource_index,
	u16 start_x, u16 start_y)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_STARTXY);

	val = DSS_REG_MOD(val, start_y, SRC_SY);	/* SRC_SY */
	val = DSS_REG_MOD(val, start_x, SRC_SX);	/* SRC_SX */

	dss_write_reg(resource_index, 0, ADDR_VID0_RD_STARTXY, val);
}

/* VID0_RD_SIZEXY */
static void _du_vid_rd_sizexy(enum odin_resource_index resource_index,
	u16 size_x, u32 size_y)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_SIZEXY);

	val = DSS_REG_MOD(val, size_y, SRC_SIZEY);	/* SRC_SIZEY */
	val = DSS_REG_MOD(val, size_x, SRC_SIZEX);	/* SRC_SIZEX */

	dss_write_reg(resource_index, 0, ADDR_VID0_RD_SIZEXY, val);
}

/* VID0_RD_PIP_SIZEXY */
static void _du_vid_rd_pip_sizexy(enum odin_resource_index resource_index,
	u16 size_x, u32 size_y)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_PIP_SIZEXY);

	val = DSS_REG_MOD(val, size_y, SRC_PIP_SIZEY);	/* SRC_PIP_SIZEY */
	val = DSS_REG_MOD(val, size_x, SRC_PIP_SIZEX);	/* SRC_PIP_SIZEX */

	dss_write_reg(resource_index, 0, ADDR_VID0_RD_PIP_SIZEXY, val);
}

/* VID0_RD_SCALE_RATIOXY */
static void _du_vid_rd_scale_ratioxy(enum odin_resource_index resource_index,
	u16 x_ratio, u32 y_ratio)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_SCALE_RATIOXY);

	val = DSS_REG_MOD(val, x_ratio, SRC_X_RATIO);	/* SRC_X_RATIO */
	val = DSS_REG_MOD(val, y_ratio, SRC_Y_RATIO);	/* SRC_Y_RATIO */

	dss_write_reg(resource_index, 0, ADDR_VID0_RD_SCALE_RATIOXY, val);
}

/* VID0_RD_DSS_BASE_Y_ADDR0,1,2 */
static void _du_vid_dss_base_y_addr(enum odin_resource_index resource_index,
	u8 fb_num, u32 paddr)
{
	u32 val;
	u32 fb_num_offset =  dss_get_fb_num_offset(fb_num);

	val = dss_read_reg(resource_index,
		0, ADDR_VID0_RD_DSS_BASE_Y_ADDR0 + fb_num_offset);

	val = DSS_REG_MOD(val, paddr, SRC_ST_ADDR_Y0);	/* SRC_ST_ADDR_Y0 */

	dss_write_reg(resource_index,
		0, ADDR_VID0_RD_DSS_BASE_Y_ADDR0  + fb_num_offset, val);
}

/* VID0_RD_DSS_BASE_U_ADDR0,1,2 */
static void _du_vid_dss_base_u_addr(enum odin_resource_index resource_index,
	u8 fb_num, u32 paddr)
{
	u32 val;
	u32 fb_num_offset =  dss_get_fb_num_offset(fb_num);

	val = dss_read_reg(resource_index,
		0, ADDR_VID0_RD_DSS_BASE_U_ADDR0 + fb_num_offset);

	val = DSS_REG_MOD(val, paddr, SRC_ST_ADDR_U0);	/* SRC_ST_ADDR_U0 */

	dss_write_reg(resource_index,
		0, ADDR_VID0_RD_DSS_BASE_U_ADDR0  + fb_num_offset, val);
}

/* VID0_RD_DSS_BASE_V_ADDR0,1,2 */
static void _du_vid_dss_base_v_addr(enum odin_resource_index resource_index,
	u8 fb_num, u32 paddr)
{
	u32 val;
	u32 fb_num_offset =  dss_get_fb_num_offset(fb_num);

	val = dss_read_reg(resource_index,
		0, ADDR_VID0_RD_DSS_BASE_V_ADDR0 + fb_num_offset);

	val = DSS_REG_MOD(val, paddr, SRC_ST_ADDR_V0);	/* SRC_ST_ADDR_V0 */

	dss_write_reg(resource_index,
		0, ADDR_VID0_RD_DSS_BASE_V_ADDR0  + fb_num_offset, val);
}

#if 0
/* VID0_RD_STATUS */
static u8 _du_vid_rd_staus(enum odin_resource_index resource_index,
	enum vid_rd_status id)
{
	u32 val;
	u8 ret;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_STATUS);

	switch (id) {
	case VID_RD_SCL_HSC_RD_STATE:
		ret = DSS_REG_GET(val, SCL_HSC_RD_STATE);
		break;
	case VID_RD_SCL_HSC_WR_STATE:
		ret = DSS_REG_GET(val, SCL_HSC_WR_STATE);
		break;
	case VID_RD_SCL_VSC_RD_STATE:
		ret = DSS_REG_GET(val, SCL_VSC_RD_STATE);
		break;
	case VID_RD_SCL_VSC_WR_STATE:
		ret = DSS_REG_GET(val, SCL_VSC_WR_STATE);
		break;
	case VID_RD_RDBIF_FMC_SYNC_STATE:
		ret = DSS_REG_GET(val, RDBIF_FMC_SYNC_STATE);
		break;
	case VID_RD_RDBIF_FMC_UV_STATE:
		ret = DSS_REG_GET(val, RDBIF_FMC_UV_STATE);
		break;
	case VID_RD_RDBIF_FMC_Y_STATE:
		ret = DSS_REG_GET(val, RDBIF_FMC_Y_STATE);
		break;
	case VID_RD_RDBIF_CON_SYNC_STATE:
		ret = DSS_REG_GET(val, RDBIF_CON_SYNC_STATE);
		break;
	case VID_RD_RDBIF_CON_STATE_V:
		ret = DSS_REG_GET(val, RDBIF_CON_STATE_V);
		break;
	case VID_RD_RDBIF_CON_STATE_U:
		ret = DSS_REG_GET(val, RDBIF_CON_STATE_U);
		break;
	case VID_RD_RDBIF_CON_STATE_Y:
		ret = DSS_REG_GET(val, RDBIF_CON_STATE_Y);
		break;
	case VID_RD_RDBIF_EMPTY_DATFIFO_V:
		ret = DSS_REG_GET(val, RDBIF_EMPTY_DATFIFO_V);
		break;
	case VID_RD_RDBIF_EMPTY_DATFIFO_U:
		ret = DSS_REG_GET(val, RDBIF_EMPTY_DATFIFO_U);
		break;
	case VID_RD_RDBIF_EMPTY_DATFIFO_Y:
		ret = DSS_REG_GET(val, RDBIF_EMPTY_DATFIFO_Y);
		break;
	case VID_RD_RDBIF_FULL_DATFIFO_V:
		ret = DSS_REG_GET(val, RDBIF_FULL_DATFIFO_V);
		break;
	case VID_RD_RDBIF_FULL_DATFIFO_U:
		ret = DSS_REG_GET(val, RDBIF_FULL_DATFIFO_U);
		break;
	case VID_RD_RDBIF_FULL_DATFIFO_Y:
		ret = DSS_REG_GET(val, RDBIF_FULL_DATFIFO_Y);
		break;
	case VID_RD_RDMIF_EMPTY_DATFIFO:
		ret = DSS_REG_GET(val, RDMIF_EMPTY_DATFIFO);
		break;
	case VID_RD_RDMIF_EMPTY_CMDFIFO:
		ret = DSS_REG_GET(val, RDMIF_EMPTY_CMDFIFO);
		break;
	case VID_RD_RDMIF_FULL_DATFIFO:
		ret = DSS_REG_GET(val, RDMIF_FULL_DATFIFO);
		break;
	case VID_RD_RDMIF_FULL_CMDFIFO:
		ret = DSS_REG_GET(val, RDMIF_FULL_CMDFIFO);
		break;
	default:
		ret = -EINVAL;
		DSSERR("_du_vid_rd_staus error \n");
		BUG_ON(1);
		break;
	}

	return ret;
}
#endif

/*------------------------------------------------------------------------*/
/* Common of VID0, VID1, GSCL  */
/* 0x190		_du_vid_wb_cfg0                */
/*------------------------------------------------------------------------*/

/* VID0_WB_CFG0 */
static void _du_vid_wb_cfg0(enum odin_resource_index resource_index,
	enum vid_wb_cfg0 id, u8 reg_val)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_WB_CFG0);

	switch (id) {
	case VID_WB_REG_SW_UPDATE:
		/* WB_REG_SW_UPDATE */
		val = DSS_REG_MOD(val, reg_val, WB_REG_SW_UPDATE);
		break;
	case VID_WB_SYNC_SEL:
		/* WB_SYNC_SEL  */
		val = DSS_REG_MOD(val, reg_val, WB_SYNC_SEL);
		break;
	case VID_WB_BUF_SEL:
		/* WB_BUF_SEL  */
		val = DSS_REG_MOD(val, reg_val, WB_BUF_SEL);
		break;
	case VID_WB_ALPHA_REG_EN:
		/* WB_ALPHA_REG_EN  */
		val = DSS_REG_MOD(val, reg_val, WB_ALPHA_REG_EN);
		break;
	case VID_WB_EN:
		/* WB_EN */
		val = DSS_REG_MOD(val, reg_val, WB_EN);
		break;
	default:
		break;
	}

	dss_write_reg(resource_index, 0, ADDR_VID0_WB_CFG0, val);
}

static void _du_vid_wb_reg_sw_update(enum odin_resource_index resource_index,
	u8 wb_reg_sw_update)
{
	_du_vid_wb_cfg0(resource_index, VID_WB_REG_SW_UPDATE, wb_reg_sw_update);
}

static void _du_vid_wb_sync_sel(enum odin_resource_index resource_index,
	u8 wb_sync_sel)
{
	_du_vid_wb_cfg0(resource_index, VID_WB_SYNC_SEL, wb_sync_sel);
}

static void _du_vid_wb_buf_sel(enum odin_resource_index resource_index,
	u8 wb_buf_sel)
{
	_du_vid_wb_cfg0(resource_index, VID_WB_BUF_SEL, wb_buf_sel);
}

#if 0
static void _du_vid_wb_alpha_reg_en(enum odin_resource_index resource_index,
	u8 wb_alpha_reg_en)
{
	_du_vid_wb_cfg0(resource_index, VID_WB_ALPHA_REG_EN, wb_alpha_reg_en);
}
#endif

static void _du_vid_wb_en(enum odin_resource_index resource_index, u8 wb_en)
{
	_du_vid_wb_cfg0(resource_index, VID_WB_EN, wb_en);
}


/*------------------------------------------------------------------------*/
/* Common of VID0, VID1, GSCL  */
/* 0x194~0x01E4		WB                      */
/*------------------------------------------------------------------------*/

/* VID0_WB_CFG1 */
static void _du_vid_wb_cfg1(enum odin_resource_index resource_index,
	enum vid_wb_cfg1 id, u8 reg_val)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_WB_CFG1);

	switch (id) {
/*GSCL Only Start*/
	case VID_WB_OVL_INFO_SRC_SEL:
		/* OVL_INF_SRC_SEL */
		val = DSS_REG_MOD(val, reg_val, OVL_INF_SRC_SEL);
		break;
/*GSCL Only End*/
/*VID0_1 Only Start*/
	case VID_WB_SRC_SEL:
		val = DSS_REG_MOD(val, reg_val, WB_SRC_SEL);	/* WB_SRC_SEL */
		break;
/*VID0_1 Only End*/
	case VID_WB_YUV_SWAP:
		val = DSS_REG_MOD(val, reg_val, WB_YUV_SWAP); 	/* WB_YUV_SWAP */
		break;
	case VID_WB_RGB_SWAP:
		val = DSS_REG_MOD(val, reg_val, WB_RGB_SWAP); 	/* WB_RGB_SWAP */
		break;
	case VID_WB_ALPHA_SWAP:
		val = DSS_REG_MOD(val, reg_val, WB_ALPHA_SWAP); /* WB_ALPHA_SWAP */
		break;
	case VID_WB_WORD_SWAP:
		val = DSS_REG_MOD(val, reg_val, WB_WORD_SWAP); 	/* WB_WORD_SWAP */
		break;
	case VID_WB_BYTE_SWAP:
		val = DSS_REG_MOD(val, reg_val, WB_BYTE_SWAP); 	/* WB_BYTE_SWAP */
		break;
	case VID_WB_FORMAT:
		val = DSS_REG_MOD(val, reg_val, WB_FORMAT); 	/* WB_FORMAT */
		break;
	default:
		break;
	}

	dss_write_reg(resource_index, 0, ADDR_VID0_WB_CFG1, val);
}

#if 0
static void _du_vid_wb_plane_src_sel(enum odin_resource_index resource_index,
	u8 wb_src_sel)
{
	_du_vid_wb_cfg1(resource_index, VID_WB_SRC_SEL, wb_src_sel);
}
#endif

static void _du_vid_wb_channel_src_sel(enum odin_resource_index resource_index,
	u8 wb_src_sel)
{
	_du_vid_wb_cfg1(resource_index, VID_WB_OVL_INFO_SRC_SEL, wb_src_sel);
}

static void _du_vid_wb_yuv_swap(enum odin_resource_index resource_index,
	u8 wb_yuv_swap)
{
	_du_vid_wb_cfg1(resource_index, VID_WB_YUV_SWAP, wb_yuv_swap);
}

static void _du_vid_wb_rgb_swap(enum odin_resource_index resource_index,
	u8 wb_rgb_swap)
{
	_du_vid_wb_cfg1(resource_index, VID_WB_RGB_SWAP, wb_rgb_swap);
}

static void _du_vid_wb_alpha_swap(enum odin_resource_index resource_index,
	u8 wb_alpha_swap)
{
	_du_vid_wb_cfg1(resource_index, VID_WB_ALPHA_SWAP, wb_alpha_swap);
}

static void _du_vid_wb_word_swap(enum odin_resource_index resource_index,
	u8 wb_word_swap)
{
	_du_vid_wb_cfg1(resource_index, VID_WB_WORD_SWAP, wb_word_swap);
}

static void _du_vid_wb_byte_swap(enum odin_resource_index resource_index,
	u8 wb_byte_swap)
{
	_du_vid_wb_cfg1(resource_index, VID_WB_BYTE_SWAP, wb_byte_swap);
}

static void _du_vid_wb_format(enum odin_resource_index resource_index,
	u8 wb_format)
{
	_du_vid_wb_cfg1(resource_index, VID_WB_FORMAT, wb_format);
}

#if 0
/* VID0_WB_ALPHA_VALUE */
static void _du_vid_wb_alpah_value(enum odin_resource_index resource_index,
	u8 global_alpha)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_WB_ALPHA_VALUE);

	val = DSS_REG_MOD(val, global_alpha, WB_ALPHA);		/*  WB_ALPHA */

	dss_write_reg(resource_index, 0, ADDR_VID0_WB_ALPHA_VALUE, val);
}
#endif

/* VID0_WB_WIDTH_HEIGHT */

static void _du_vid_wb_width_hegiht(enum odin_resource_index resource_index,
	u16 width, u16 height)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_WB_WIDTH_HEIGHT);

	val = DSS_REG_MOD(val, height, WB_HEIGHT);		/* WB_HEIGHT */
	val = DSS_REG_MOD(val, width, WB_WIDTH);		/* WB_WIDTH */

	dss_write_reg(resource_index, 0, ADDR_VID0_WB_WIDTH_HEIGHT, val);
}

/* VID0_WB_STARTXY */
static void _du_vid_wb_startxy(enum odin_resource_index resource_index,
	u16 start_x, u16 start_y)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_WB_STARTXY);

	val = DSS_REG_MOD(val, start_y, WB_SY);		/* WB_SY */
	val = DSS_REG_MOD(val, start_x, WB_SX);		/* WB_SX */

	dss_write_reg(resource_index, 0, ADDR_VID0_WB_STARTXY, val);
}

/* VID0_WB_SIZEXY */
static void _du_vid_wb_sizexy(enum odin_resource_index resource_index,
	u16 size_x, u32 size_y)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_WB_SIZEXY);

	val = DSS_REG_MOD(val, size_y, WB_SIZEY);		/* WB_SIZEY */
	val = DSS_REG_MOD(val, size_x, WB_SIZEX);		/* WB_SIZEX */

	dss_write_reg(resource_index, 0, ADDR_VID0_WB_SIZEXY, val);
}

/* VID0_WB_DSS_BASE_Y_ADDR0 */
static void _du_vid_wb_dss_base_y_addr
	(enum odin_resource_index resource_index, u8 fb_num, u32 paddr)
{
	u32 val;
	u32 fb_num_offset =  dss_get_fb_num_offset(fb_num);

	val = dss_read_reg(resource_index, 0, ADDR_VID0_WB_DSS_BASE_Y_ADDR0 +
			fb_num_offset);

	val = DSS_REG_MOD(val, paddr, WB_ST_ADDR_Y0); /* WB_ST_ADDR_Y0 */

	dss_write_reg(resource_index, 0, ADDR_VID0_WB_DSS_BASE_Y_ADDR0 +
		fb_num_offset, val);
}

/* VID0_WB_DSS_BASE_U_ADDR0 */
static void _du_vid_wb_dss_base_u_addr
	(enum odin_resource_index resource_index, u8 fb_num, u32 paddr)
{
	u32 val;
	u32 fb_num_offset =  dss_get_fb_num_offset(fb_num);

	val = dss_read_reg(resource_index, 0, ADDR_VID0_WB_DSS_BASE_U_ADDR0 +
			fb_num_offset);

	val = DSS_REG_MOD(val, paddr, WB_ST_ADDR_U0); /* WB_ST_ADDR_Y0 */

	dss_write_reg(resource_index, 0, ADDR_VID0_WB_DSS_BASE_U_ADDR0 +
		fb_num_offset, val);
}

#if 0
/* VID0_WB_STATUS */
static u8 _du_vid_wb_status(enum odin_resource_index resource_index,
	enum vid_wb_status id)
{
	u32 val;
	u8 ret;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_WB_STATUS);

	switch (id) {

	case VID_WB_WRMIF_EMPTY_DATFIFO:
		ret = DSS_REG_GET(val, WRMIF_EMPTY_DATFIFO);
		break;
	case VID_WB_WRMIF_EMPTY_CMDFIFO:
		ret = DSS_REG_GET(val, WRMIF_EMPTY_CMDFIFO);
		break;
	case VID_WB_WRMIF_FULL_DATFIFO:
		ret = DSS_REG_GET(val, WRMIF_FULL_DATFIFO);
		break;
	case VID_WB_WRMIF_FULL_CMDFIFO:
		ret = DSS_REG_GET(val, WRMIF_FULL_CMDFIFO);
		break;
	case VID_WB_WRCTRL_CON_SYNC_STATE:
		ret = DSS_REG_GET(val, WRCTRL_CON_SYNC_STATE);
		break;
	case VID_WB_WRCTRL_CON_V_STATE:
		ret = DSS_REG_GET(val, WRCTRL_CON_V_STATE);
		break;
	case VID_WB_WRCTRL_CON_U_STATE:
		ret = DSS_REG_GET(val, WRCTRL_CON_U_STATE);
		break;
	case VID_WB_WRCTRL_CON_Y_STATE:
		ret = DSS_REG_GET(val, WRCTRL_CON_Y_STATE);
		break;
	case VID_WB_WRCTRL_EMPTY_INF_FIFOV:
		ret = DSS_REG_GET(val, WRCTRL_EMPTY_INF_FIFOV);
		break;
	case VID_WB_WRCTRL_EMPTY_INF_FIFOU:
		ret = DSS_REG_GET(val, WRCTRL_EMPTY_INF_FIFOU);
		break;
	case VID_WB_WRCTRL_EMPTY_INF_FIFOY:
		ret = DSS_REG_GET(val, WRCTRL_EMPTY_INF_FIFOY);
		break;
	case VID_WB_WRCTRL_FULL_INF_FIFOV:
		ret = DSS_REG_GET(val, WRCTRL_FULL_INF_FIFOV);
		break;
	case VID_WB_WRCTRL_FULL_INF_FIFOU:
		ret = DSS_REG_GET(val, WRCTRL_FULL_INF_FIFOU);
		break;
	case VID_WB_WRCTRL_FULL_INF_FIFOY:
		ret = DSS_REG_GET(val, WRCTRL_FULL_INF_FIFOY);
		break;
	default:
		ret = -EINVAL;
		DSSERR("_du_vid_wb_status error \n");
		BUG_ON(1);
		break;
	}

	return ret;
}
#endif

/*------------------------------------------------------------------------*/
/* Common of VID0, VID1  */
/* 0x500~0x057C		HSC Coef	     */
/* 0x600~0x063C		VSC Coef	     */
/*------------------------------------------------------------------------*/

#if 0
/* VID0_POLY_HCOEF_12T_0P0 ~ VID0_POLY_HCOEF_12T_7P3 */

static void _du_vid_poly_hcoef_12t_np0(enum odin_resource_index resource_index,
	u8 phase, u16 m3_coef, u16 m4_coef, u16 m5_coef)
{
	u32 val;
	u32 reg_addr;
	switch (phase) {
	case 7:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_7P0;
		break;
	case 6:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_6P0;
		break;
	case 5:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_5P0;
		break;
	case 4:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_4P0;
		break;
	case 3:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_3P0;
		break;
	case 2:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_2P0;
		break;
	case 1:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_1P0;
		break;
	case 0:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_0P0;
		break;
	default:
		DSSERR("_du_vid_poly_hcoef_12t_np0 error \n");
		BUG_ON(1);
		return;
	}

	val = dss_read_reg(resource_index, 0, reg_addr);
	val = DSS_REG_MOD(val, m3_coef, PP_0P_M3_HCOEF); /* PP_0P_M3_HCOEF */
	val = DSS_REG_MOD(val, m4_coef, PP_0P_M4_HCOEF); /* PP_0P_M4_HCOEF */
	val = DSS_REG_MOD(val, m5_coef, PP_0P_M5_HCOEF); /* PP_0P_M5_HCOEF */
	dss_write_reg(resource_index, 0, reg_addr, val);

}

static void _du_vid_poly_hcoef_12t_np1(enum odin_resource_index resource_index,
	u8 phase,	u16 p0_coef, u16 m1_coef, u16 m2_coef)
{
	u32 val;
	u32 reg_addr;
	switch (phase) {
	case 7:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_7P1;
		break;
	case 6:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_6P1;
		break;
	case 5:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_5P1;
		break;
	case 4:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_4P1;
		break;
	case 3:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_3P1;
		break;
	case 2:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_2P1;
		break;
	case 1:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_1P1;
		break;
	case 0:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_0P1;
		break;
	default:
		DSSERR("_du_vid_poly_hcoef_12t_np1 error \n");
		BUG_ON(1);
		return;
	}

	val = dss_read_reg(resource_index, 0, reg_addr);
	val = DSS_REG_MOD(val, p0_coef, PP_0P_P0_HCOEF); /* PP_0P_P0_HCOEF */
	val = DSS_REG_MOD(val, m1_coef, PP_0P_M1_HCOEF); /* PP_0P_M1_HCOEF */
	val = DSS_REG_MOD(val, m2_coef, PP_0P_M2_HCOEF); /* PP_0P_M2_HCOEF */
	dss_write_reg(resource_index, 0, reg_addr, val);

}

static void _du_vid_poly_hcoef_12t_np2(enum odin_resource_index resource_index,
	u8 phase,	u16 p3_coef, u16 p2_coef, u16 p1_coef)
{
	u32 val;
	u32 reg_addr;
	switch (phase) {
	case 7:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_7P2;
		break;
	case 6:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_6P2;
		break;
	case 5:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_5P2;
		break;
	case 4:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_4P2;
		break;
	case 3:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_3P2;
		break;
	case 2:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_2P2;
		break;
	case 1:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_1P2;
		break;
	case 0:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_0P2;
		break;
	default:
		DSSERR("_du_vid_poly_hcoef_12t_np2 error \n");
		BUG_ON(1);
		return;
	}

	val = dss_read_reg(resource_index, 0, reg_addr);
	val = DSS_REG_MOD(val, p3_coef, PP_0P_P3_HCOEF); /* PP_0P_P3_HCOEF */
	val = DSS_REG_MOD(val, p2_coef, PP_0P_P2_HCOEF); /* PP_0P_P2_HCOEF */
	val = DSS_REG_MOD(val, p1_coef, PP_0P_P1_HCOEF); /* PP_0P_P1_HCOEF */
	dss_write_reg(resource_index, 0, reg_addr, val);

}

static void _du_vid_poly_hcoef_12t_np3(enum odin_resource_index resource_index,
	u8 phase, u16 p6_coef, u16 p5_coef, u16 p4_coef)
{
	u32 val;
	u32 reg_addr;
	switch (phase) {
	case 7:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_7P3;
		break;
	case 6:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_6P3;
		break;
	case 5:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_5P3;
		break;
	case 4:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_4P3;
		break;
	case 3:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_3P3;
		break;
	case 2:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_2P2;
		break;
	case 1:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_1P1;
		break;
	case 0:
		reg_addr = ADDR_VID0_POLY_HCOEF_12T_0P0;
		break;
	default:
		DSSERR("_du_vid_poly_hcoef_12t_np2 error \n");
		BUG_ON(1);
		return;
	}

	val = dss_read_reg(resource_index, 0, reg_addr);
	val = DSS_REG_MOD(val, p6_coef, PP_0P_P6_HCOEF); /* PP_0P_P6_HCOEF */
	val = DSS_REG_MOD(val, p5_coef, PP_0P_P5_HCOEF); /* PP_0P_P5_HCOEF */
	val = DSS_REG_MOD(val, p4_coef, PP_0P_P4_HCOEF); /* PP_0P_P4_HCOEF */
	dss_write_reg(resource_index, 0, reg_addr, val);

}

/* VID0_POLY_VCOEF_12T_0P0 ~ VID0_POLY_VCOEF_12T_7P3 */
static void _du_vid_poly_vcoef_4t_np0(enum odin_resource_index resource_index,
	u8 phase, u16 p0_coef, u16 p1_coef, u16 m1_coef)
{
	u32 val;
	u32 reg_addr;
	switch (phase) {
	case 7:
		reg_addr = ADDR_VID0_POLY_VCOEF_4T_7P0;
		break;
	case 6:
		reg_addr = ADDR_VID0_POLY_VCOEF_4T_6P0;
		break;
	case 5:
		reg_addr = ADDR_VID0_POLY_VCOEF_4T_5P0;
		break;
	case 4:
		reg_addr = ADDR_VID0_POLY_VCOEF_4T_4P0;
		break;
	case 3:
		reg_addr = ADDR_VID0_POLY_VCOEF_4T_3P0;
		break;
	case 2:
		reg_addr = ADDR_VID0_POLY_VCOEF_4T_2P0;
		break;
	case 1:
		reg_addr = ADDR_VID0_POLY_VCOEF_4T_1P0;
		break;
	case 0:
		reg_addr = ADDR_VID0_POLY_VCOEF_4T_0P0;
		break;
	default:
		DSSERR("_du_vid_poly_vcoef_4t_np0 error \n");
		BUG_ON(1);
		return;
	}

	val = dss_read_reg(resource_index, 0, reg_addr);
	val = DSS_REG_MOD(val, p1_coef, PP_0P_P1_VCOEF); /* PP_0P_P1_HCOEF */
	val = DSS_REG_MOD(val, p0_coef, PP_0P_P0_VCOEF); /* PP_0P_P0_HCOEF */
	val = DSS_REG_MOD(val, m1_coef, PP_0P_M1_VCOEF); /* PP_0P_M1_HCOEF */
	dss_write_reg(resource_index, 0, reg_addr, val);

}

static void _du_vid_poly_vcoef_4t_np1(enum odin_resource_index resource_index,
	u8 phase, u16 p2_coef)
{
	u32 val;
	u32 reg_addr;
	switch (phase) {
	case 7:
		reg_addr = ADDR_VID0_POLY_VCOEF_4T_7P1;
		break;
	case 6:
		reg_addr = ADDR_VID0_POLY_VCOEF_4T_6P1;
		break;
	case 5:
		reg_addr = ADDR_VID0_POLY_VCOEF_4T_5P1;
		break;
	case 4:
		reg_addr = ADDR_VID0_POLY_VCOEF_4T_4P1;
		break;
	case 3:
		reg_addr = ADDR_VID0_POLY_VCOEF_4T_3P1;
		break;
	case 2:
		reg_addr = ADDR_VID0_POLY_VCOEF_4T_2P1;
		break;
	case 1:
		reg_addr = ADDR_VID0_POLY_VCOEF_4T_1P1;
		break;
	case 0:
		reg_addr = ADDR_VID0_POLY_VCOEF_4T_0P1;
		break;
	default:
		DSSERR("_du_vid_poly_vcoef_4t_np1 error \n");
		BUG_ON(1);
		return;
	}

	val = dss_read_reg(resource_index, 0, reg_addr);
	val = DSS_REG_MOD(val, p2_coef, PP_0P_P2_VCOEF); /* PP_0P_P2_VCOEF */
	dss_write_reg(resource_index, 0, reg_addr, val);

}
#endif


/*------------------------------------------------------------------------*/
/* Common of GRA0, GRA1, GRA2, GSCL */
/* 0x000~0x0064          RD          */
/*------------------------------------------------------------------------*/

/* GRA0_RD_CFG0 */
static void _du_gra_rd_cfg0(enum odin_resource_index resource_index,
	enum gra_rd_cfg0 id, u8 reg_val)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_CFG0);

	switch (id) {
	case GRA_RD_SRC_REG_SW_UPDATE:
		val = DSS_REG_MOD(val, reg_val, SRC_REG_SW_UPDATE);
		break;
	case GRA_RD_DMA_INDEX_RD_START:
		val = DSS_REG_MOD(val, reg_val, DMA_INDEX_RD_START);
		break;
	case GRA_RD_SRC_SYNC_SEL:
		val = DSS_REG_MOD(val, reg_val, SRC_SYNC_SEL);
		break;
	case GRA_RD_SRC_TRANS_NUM:
		val = DSS_REG_MOD(val, reg_val, SRC_TRANS_NUM);
		break;
	case GRA_RD_SRC_BUF_SEL:
		val = DSS_REG_MOD(val, reg_val, SRC_BUF_SEL);
		break;
	case GRA_RD_SRC_EN:
		val = DSS_REG_MOD(val, reg_val, SRC_EN);
		break;
	default:
		break;
	}

	dss_write_reg(resource_index, 0, ADDR_VID0_RD_CFG0, val);
}

#if 0
static void _du_gra_rd_src_reg_sw_update
	(enum odin_resource_index resource_index, u8 src_reg_sw_update)
{
	_du_gra_rd_cfg0(resource_index,
			GRA_RD_SRC_REG_SW_UPDATE, src_reg_sw_update);
}

static void _du_gra_rd_dma_index_rd_start
	(enum odin_resource_index resource_index, u8 dma_index_rd_start)
{
	_du_gra_rd_cfg0(resource_index,
			GRA_RD_DMA_INDEX_RD_START, dma_index_rd_start);
}
#endif

static void _du_gra_rd_src_sync_sel(enum odin_resource_index resource_index,
	u8 src_sync_sel)
{
	_du_gra_rd_cfg0(resource_index, GRA_RD_SRC_SYNC_SEL, src_sync_sel);
}

static void _du_gra_rd_src_trans_num(enum odin_resource_index resource_index,
	u8 src_trans_num)
{
	_du_gra_rd_cfg0(resource_index, GRA_RD_SRC_TRANS_NUM, src_trans_num);
}

static void _du_gra_rd_src_buf_sel(enum odin_resource_index resource_index,
	u8 src_buf_sel)
{
	_du_gra_rd_cfg0(resource_index, GRA_RD_SRC_BUF_SEL, src_buf_sel);
}

#if 0
static void _du_gra_rd_src_en(enum odin_resource_index resource_index,
	u8 src_en)
{
	_du_gra_rd_cfg0(resource_index, GRA_RD_SRC_EN, src_en);
}
#endif

/* GRA0_RD_CFG1 */
static void _du_gra_rd_cfg1(enum odin_resource_index resource_index,
	enum gra_rd_cfg1 id, u8 reg_val)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_CFG1);

	switch (id) {
	case GRA_RD_SRC_FLIP_MODE:
		val = DSS_REG_MOD(val, reg_val, SRC_FLIP_MODE);
		break;
	case GRA_RD_SRC_LSB_SEL:
		val = DSS_REG_MOD(val, reg_val, SRC_LSB_SEL);
		break;
	case GRA_RD_SRC_RGB_SWAP:
		val = DSS_REG_MOD(val, reg_val, SRC_RGB_SWAP);
		break;
	case GRA_RD_SRC_ALPHA_SWAP:
		val = DSS_REG_MOD(val, reg_val, SRC_ALPHA_SWAP);
		break;
	case GRA_RD_SRC_WORD_SWAP:
		val = DSS_REG_MOD(val, reg_val, SRC_WORD_SWAP);
		break;
	case GRA_RD_SRC_BYTE_SWAP:
		val = DSS_REG_MOD(val, reg_val, SRC_BYTE_SWAP);
		break;
	case GRA_RD_SRC_FORMAT:
		val = DSS_REG_MOD(val, reg_val, SRC_FORMAT);
		break;
	default:
		break;
	}

	dss_write_reg(resource_index, 0, ADDR_VID0_RD_CFG1, val);
}

static void _du_gra_rd_src_flip_mode(enum odin_resource_index resource_index,
	u8 src_flip_mode)
{
	_du_gra_rd_cfg1(resource_index, GRA_RD_SRC_FLIP_MODE, src_flip_mode);
}

static void _du_gra_rd_src_lsb_sel(enum odin_resource_index resource_index,
	u8 src_lsb_sel)
{
	_du_gra_rd_cfg1(resource_index, GRA_RD_SRC_LSB_SEL, src_lsb_sel);
}

static void _du_gra_rd_src_rgb_swap(enum odin_resource_index resource_index,
	u8 src_rgb_swap)
{
	_du_gra_rd_cfg1(resource_index, GRA_RD_SRC_RGB_SWAP, src_rgb_swap);
}

static void _du_gra_rd_src_alpha_swap(enum odin_resource_index resource_index,
	u8 src_alpha_swap)
{
	_du_gra_rd_cfg1(resource_index, GRA_RD_SRC_ALPHA_SWAP, src_alpha_swap);
}

static void _du_gra_rd_src_word_swap(enum odin_resource_index resource_index,
	u8 src_word_swap)
{
	_du_gra_rd_cfg1(resource_index, GRA_RD_SRC_WORD_SWAP, src_word_swap);
}

static void _du_gra_rd_src_byte_swap(enum odin_resource_index resource_index,
	u8 src_byte_swap)
{
	_du_gra_rd_cfg1(resource_index, GRA_RD_SRC_BYTE_SWAP, src_byte_swap);
}

static void _du_gra_rd_src_format(enum odin_resource_index resource_index,
	u8 src_format)
{
	_du_gra_rd_cfg1(resource_index, GRA_RD_SRC_FORMAT, src_format);
}

/* GRA0_RD_WIDTH_HEIGHT */
static void _du_gra_rd_width_hegiht(enum odin_resource_index resource_index,
	u16 width, u16 height)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_WIDTH_HEIGHT);

	val = DSS_REG_MOD(val, height, SRC_HEIGHT);
	val = DSS_REG_MOD(val, width, SRC_WIDTH);

	dss_write_reg(resource_index, 0, ADDR_VID0_RD_WIDTH_HEIGHT, val);
}

/* GRA0_RD_STARTXY */
static void _du_gra_rd_startxy(enum odin_resource_index resource_index,
	u16 start_x, u16 start_y)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_STARTXY);

	val = DSS_REG_MOD(val, start_y, SRC_SY);
	val = DSS_REG_MOD(val, start_x, SRC_SX);

	dss_write_reg(resource_index, 0, ADDR_VID0_RD_STARTXY, val);
}

/* GRA0_RD_SIZEXY */
static void _du_gra_rd_sizexy(enum odin_resource_index resource_index,
	u16 size_x, u16 size_y)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_SIZEXY);

	val = DSS_REG_MOD(val, size_y, SRC_SIZEY);
	val = DSS_REG_MOD(val, size_x, SRC_SIZEX);

	dss_write_reg(resource_index, 0, ADDR_VID0_RD_SIZEXY, val);
}

/* GRA0_RD_DSS_BASE_Y_ADDR0,1,2 */
static void _du_gra_dss_base_y_addr(enum odin_resource_index resource_index,
	u8 fb_num, u32 paddr)
{
	u32 val;
	u32 fb_num_offset =  dss_get_fb_num_offset(fb_num);

	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_DSS_BASE_Y_ADDR0 +
			fb_num_offset);

	val = DSS_REG_MOD(val, paddr, SRC_ST_ADDR_Y0);

	dss_write_reg(resource_index, 0, ADDR_VID0_RD_DSS_BASE_Y_ADDR0 +
		fb_num_offset, val);
}

#if 0
/* GRA0_RD_STATUS */
static u8 _du_gra_rd_staus(enum odin_resource_index resource_index,
	enum gra_rd_status id)
{
	u32 val;
	u8 ret;

	val = dss_read_reg(resource_index, 0, ADDR_GRA0_RD_STATUS);

	switch (id) {

	case GRA_RD_RDBIF_FMC_SYNC_STATE:
		ret = DSS_REG_GET(val, RDBIF_FMC_SYNC_STATE);
		break;
	case GRA_RD_RDBIF_FMC_Y_STATE:
		ret = DSS_REG_GET(val, RDBIF_FMC_Y_STATE);
		break;
	case GRA_RD_RDBIF_CON_SYNC_STATE:
		ret = DSS_REG_GET(val, RDBIF_CON_SYNC_STATE);
		break;
	case GRA_RD_RDBIF_CON_STATE_Y:
		ret = DSS_REG_GET(val, RDBIF_CON_STATE_Y);
		break;
	case GRA_RD_RDBIF_EMPTY_DATFIFO_Y:
		ret = DSS_REG_GET(val, RDBIF_EMPTY_DATFIFO_Y);
		break;
	case GRA_RD_RDBIF_FULL_DATFIFO_Y:
		ret = DSS_REG_GET(val, RDBIF_FULL_DATFIFO_Y);
		break;
	case GRA_RD_RDMIF_EMPTY_DATFIFO:
		ret = DSS_REG_GET(val, RDMIF_EMPTY_DATFIFO);
		break;
	case GRA_RD_RDMIF_EMPTY_CMDFIFO:
		ret = DSS_REG_GET(val, RDMIF_EMPTY_CMDFIFO);
		break;
	case GRA_RD_RDMIF_FULL_DATFIFO:
		ret = DSS_REG_GET(val, RDMIF_FULL_DATFIFO);
		break;
	case GRA_RD_RDMIF_FULL_CMDFIFO:
		ret = DSS_REG_GET(val, RDMIF_FULL_CMDFIFO);
		break;
	default:
		ret = -EINVAL;
		DSSERR("_du_vid_rd_staus error \n");
		BUG_ON(1);
		break;
	}

	return ret;
}
#endif

static u8 _du_get_plane_rd_en(enum odin_resource_index resource_index)
{
	u32 val;
	u8 rd_enable;

	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_CFG0);
	rd_enable = DSS_REG_GET(val,  SRC_EN);

	return rd_enable;
}


u32 _du_get_plane_rd_y_rgb_addr(enum odin_resource_index resource_index)
{
	return dss_read_reg(resource_index, 0, ADDR_VID0_RD_DSS_BASE_Y_ADDR0);
}

/*------------------------------------------------------------------------*/
/* Common of OVL			             */
/* 0x000~0x034C		                          */
/*------------------------------------------------------------------------*/
/* OVL0_CFG0 */

static void _du_ovl_cfg0(enum odin_channel channel, enum ovl_cfg0 id,
	u8 reg_val)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_OVL_RES, channel, ADDR_OVL0_CFG0);

	switch (id) {
	case OVL_LAYER5_COP:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER5_COP);
		break;
	case OVL_LAYER4_COP:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER4_COP);
		break;
	case OVL_LAYER3_COP:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER3_COP);
		break;
	case OVL_LAYER2_COP:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER2_COP);
		break;
	case OVL_LAYER1_COP:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER1_COP);
		break;
	case OVL_LAYER0_COP:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER0_COP);
		break;
	case OVL_LAYER0_ROP:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER0_ROP);
		break;
	case OVL_DUAL_OP_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_DUAL_OP_EN);
		break;
	case OVL_DST_SEL:
		val = DSS_REG_MOD(val, reg_val, OVL0_DST_SEL);
		break;
	case OVL_LAYER_OP:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER_OP);
		break;
	case OVL_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_EN);
		break;
	default:
		break;
	}

	dss_write_reg(ODIN_DSS_OVL_RES, channel, ADDR_OVL0_CFG0, val);
}

static void _du_ovl_layer_cop(enum odin_channel channel, u8 layer_number,
	u8 cop)
{
	u8 id;
	switch (layer_number) {
	case 5:
		id = OVL_LAYER5_COP;
		break;
	case 4:
		id = OVL_LAYER4_COP;
		break;
	case 3:
		id = OVL_LAYER3_COP;
		break;
	case 2:
		id = OVL_LAYER2_COP;
		break;
	case 1:
		id = OVL_LAYER1_COP;
		break;
	case 0:
		id = OVL_LAYER0_COP;
		break;
	default:
		DSSERR("_du_ovl_layer_cop error \n");
		BUG_ON(1);
		return;
	}

	_du_ovl_cfg0(channel, id, cop);
}

static void _du_ovl_layer_cop_total(enum odin_channel channel, u32 ovl_cops)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_OVL_RES, channel, ADDR_OVL0_CFG0);

	val = ovl_cops | (val & 0xf);

	dss_write_reg(ODIN_DSS_OVL_RES, channel, ADDR_OVL0_CFG0, val);
}

#if 0
static void _du_ovl_layer_rop(enum odin_channel channel, u8 rop)
{
	_du_ovl_cfg0(channel, OVL_LAYER0_ROP, rop);
}
#endif


static void _du_ovl_dual_op_en(enum odin_channel channel, u8 ovl_dual_op_en)
{
	_du_ovl_cfg0(channel, OVL_DUAL_OP_EN, ovl_dual_op_en);
}

static void _du_ovl_dst_sel(enum odin_channel channel, u8 ovl_dst_sel)
{
	_du_ovl_cfg0(channel, OVL_DST_SEL, ovl_dst_sel);
}

static void _du_ovl_layer_op(enum odin_channel channel, u8 ovl_layer_op)
{
	_du_ovl_cfg0(channel, OVL_LAYER_OP, ovl_layer_op);
}

static void _du_ovl_en(enum odin_channel channel, u8 ovl_en)
{
	_du_ovl_cfg0(channel, OVL_EN, ovl_en);
}


/* OVL0_SIZEXY */
static void _du_ovl_sizexy(enum odin_channel channel, u16 size_x, u16 size_y)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_OVL_RES, channel, ADDR_OVL0_SIZEXY);

	val = DSS_REG_MOD(val, size_y, OVL0_SIZEY);
	val = DSS_REG_MOD(val, size_x, OVL0_SIZEX);

	dss_write_reg(ODIN_DSS_OVL_RES, channel, ADDR_OVL0_SIZEXY, val);
}

#if 0
/* OVL0_PATTERN_DATA */
static void _du_ovl_pattern_data(enum odin_channel channel,
	u8 bg_pat_rdata, u8 bg_pat_gdata, u8 bg_pat_bdata)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_OVL_RES, channel, ADDR_OVL0_PATTERN_DATA);

	val = DSS_REG_MOD(val, bg_pat_rdata, OVL0_BG_PAT_RDATA);
	val = DSS_REG_MOD(val, bg_pat_gdata, OVL0_BG_PAT_GDATA);
	val = DSS_REG_MOD(val, bg_pat_bdata, OVL0_BG_PAT_BDATA);

	dss_write_reg(ODIN_DSS_OVL_RES, channel, ADDR_OVL0_PATTERN_DATA, val);
}
#endif

/* OVL0_CFG1 */
static void _du_ovl_cfg1(enum odin_channel channel, u8 id, u8 reg_val)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_OVL_RES, channel, ADDR_OVL0_CFG1);

	switch (id) {
	case OVL_LAYER5_SRC_PRE_MUL_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER5_SRC_PRE_MUL_EN);
		break;
	case OVL_LAYER5_DST_PRE_MUL_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER5_DST_PRE_MUL_EN);
		break;
	case OVL_LAYER4_SRC_PRE_MUL_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER4_SRC_PRE_MUL_EN);
		break;
	case OVL_LAYER4_DST_PRE_MUL_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER4_DST_PRE_MUL_EN);
		break;
	case OVL_LAYER3_SRC_PRE_MUL_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER3_SRC_PRE_MUL_EN);
		break;
	case OVL_LAYER3_DST_PRE_MUL_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER3_DST_PRE_MUL_EN);
		break;
	case OVL_LAYER2_SRC_PRE_MUL_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER2_SRC_PRE_MUL_EN);
		break;
	case OVL_LAYER2_DST_PRE_MUL_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER2_DST_PRE_MUL_EN);
		break;
	case OVL_LAYER1_SRC_PRE_MUL_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER1_SRC_PRE_MUL_EN);
		break;
	case OVL_LAYER1_DST_PRE_MUL_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER1_DST_PRE_MUL_EN);
		break;
	case OVL_LAYER0_SRC_PRE_MUL_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER0_SRC_PRE_MUL_EN);
		break;
	case OVL_LAYER0_DST_PRE_MUL_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER0_DST_PRE_MUL_EN);
		break;
	case OVL_LAYER5_CHROMAKEY_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER5_CHROMAKEY_EN);
		break;
	case OVL_LAYER4_CHROMAKEY_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER4_CHROMAKEY_EN);
		break;
	case OVL_LAYER3_CHROMAKEY_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER3_CHROMAKEY_EN);
		break;
	case OVL_LAYER2_CHROMAKEY_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER2_CHROMAKEY_EN);
		break;
	case OVL_LAYER1_CHROMAKEY_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER1_CHROMAKEY_EN);
		break;
	case OVL_LAYER0_CHROMAKEY_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_LAYER0_CHROMAKEY_EN);
		break;

	default:
		break;
	}

	dss_write_reg(ODIN_DSS_OVL_RES, channel, ADDR_OVL0_CFG1, val);
}

#if 0
static void _du_ovl_src_pre_mul_en(enum odin_channel channel, u8 layer_number,
	u8 src_pre_mul_en)
{
	u8 id;
	switch (layer_number) {
	case 5:
		id = OVL_LAYER5_SRC_PRE_MUL_EN;
		break;
	case 4:
		id = OVL_LAYER4_SRC_PRE_MUL_EN;
		break;
	case 3:
		id = OVL_LAYER3_SRC_PRE_MUL_EN;
		break;
	case 2:
		id = OVL_LAYER2_SRC_PRE_MUL_EN;
		break;
	case 1:
		id = OVL_LAYER1_SRC_PRE_MUL_EN;
		break;
	case 0:
		id = OVL_LAYER0_SRC_PRE_MUL_EN;
		break;
	default:
		DSSERR("_du_ovl_src_pre_mul_en error \n");
		BUG_ON(1);
		return;
	}

	_du_ovl_cfg1(channel, id, src_pre_mul_en);

}
#endif

static void _du_ovl_src_pre_mul_en_total(enum odin_channel channel,
	u32 src_premul_zorder_mask)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_OVL_RES, channel, ADDR_OVL0_CFG1);
	/* Section   |     Src Premultiply |Remainder  	      */
	val = (src_premul_zorder_mask & 0xAAA00) | (val & ~0xAAA00);

	dss_write_reg(ODIN_DSS_OVL_RES, channel, ADDR_OVL0_CFG1, val);
}

static void _du_ovl_dst_pre_mul_en(enum odin_channel channel, u8 cop_num,
	u8 dst_pre_mul_en)
{
	u8 id;
	switch (cop_num) {
	case 5:
		id = OVL_LAYER5_DST_PRE_MUL_EN;
		break;
	case 4:
		id = OVL_LAYER4_DST_PRE_MUL_EN;
		break;
	case 3:
		id = OVL_LAYER3_DST_PRE_MUL_EN;
		break;
	case 2:
		id = OVL_LAYER2_DST_PRE_MUL_EN;
		break;
	case 1:
		id = OVL_LAYER1_DST_PRE_MUL_EN;
		break;
	case 0:
		id = OVL_LAYER0_DST_PRE_MUL_EN;
		break;
	default:
		DSSERR("_du_ovl_dst_pre_mul_en error \n");
		BUG_ON(1);
		return;
	}

	_du_ovl_cfg1(channel, id, dst_pre_mul_en);

}

static void _du_ovl_chromakey_en(enum odin_channel channel,
	u8 cop_num, u8 chromakey_en)
{
	u8 id;
	switch (cop_num) {
	case 5:
		id = OVL_LAYER5_CHROMAKEY_EN;
		break;
	case 4:
		id = OVL_LAYER4_CHROMAKEY_EN;
		break;
	case 3:
		id = OVL_LAYER3_CHROMAKEY_EN;
		break;
	case 2:
		id = OVL_LAYER2_CHROMAKEY_EN;
		break;
	case 1:
		id = OVL_LAYER1_CHROMAKEY_EN;
		break;
	case 0:
		id = OVL_LAYER0_CHROMAKEY_EN;
		break;
	default:
		DSSERR("_du_ovl_chromakey_en error \n");
		BUG_ON(1);
		return;
	}

	_du_ovl_cfg1(channel, id, chromakey_en);

}

/* OVL0_L0_CKEY_VALUE */
static void _du_ovl_layer_ckey_value(enum odin_channel channel,
		u8 cop_num, u8 chroma_rdata, u8 chroma_gdata, u8 chroma_bdata)
{
	u32 val;
	u32 reg_addr;
	switch (cop_num) {
	case 5:
		reg_addr = ADDR_OVL0_L5_CKEY_VALUE;
		break;
	case 4:
		reg_addr = ADDR_OVL0_L4_CKEY_VALUE;
		break;
	case 3:
		reg_addr = ADDR_OVL0_L3_CKEY_VALUE;
		break;
	case 2:
		reg_addr = ADDR_OVL0_L2_CKEY_VALUE;
		break;
	case 1:
		reg_addr = ADDR_OVL0_L1_CKEY_VALUE;
		break;
	case 0:
		reg_addr = ADDR_OVL0_L0_CKEY_VALUE;
		break;
	default:
		DSSERR("_du_ovl_layer_ckey_value error \n");
		BUG_ON(1);
		return;
	}

	val = dss_read_reg(ODIN_DSS_OVL_RES, channel, reg_addr);
	val = DSS_REG_MOD(val, chroma_rdata, OVL0_LAYER0_CHROMA_RDATA);
	val = DSS_REG_MOD(val, chroma_gdata, OVL0_LAYER0_CHROMA_GDATA);
	val = DSS_REG_MOD(val, chroma_bdata, OVL0_LAYER0_CHROMA_BDATA);
	dss_write_reg(ODIN_DSS_OVL_RES, channel, reg_addr, val);

}

/* OVL0_L0_CKEY_MASK_VALUE */
static void _du_ovl_layer_ckey_mask_value(enum odin_channel channel,
		u8 cop_num, u8 chroma_rmask, u8 chroma_gmask, u8 chroma_bmask)
{
	u32 val;
	u32 reg_addr;
	switch (cop_num) {
	case 5:
		reg_addr = ADDR_OVL0_L5_CKEY_MASK_VALUE;
		break;
	case 4:
		reg_addr = ADDR_OVL0_L4_CKEY_MASK_VALUE;
		break;
	case 3:
		reg_addr = ADDR_OVL0_L3_CKEY_MASK_VALUE;
		break;
	case 2:
		reg_addr = ADDR_OVL0_L2_CKEY_MASK_VALUE;
		break;
	case 1:
		reg_addr = ADDR_OVL0_L1_CKEY_MASK_VALUE;
		break;
	case 0:
		reg_addr = ADDR_OVL0_L0_CKEY_MASK_VALUE;
		break;
	default:
		DSSERR("_du_ovl_layer_ckey_value error \n");
		BUG_ON(1);
		return;
	}

	val = dss_read_reg(ODIN_DSS_OVL_RES, channel, reg_addr);
	val = DSS_REG_MOD(val, chroma_rmask, OVL0_LAYER0_CHROMA_RMASK);
	val = DSS_REG_MOD(val, chroma_gmask, OVL0_LAYER0_CHROMA_GMASK);
	val = DSS_REG_MOD(val, chroma_bmask, OVL0_LAYER0_CHROMA_BMASK);
	dss_write_reg(ODIN_DSS_OVL_RES, channel, reg_addr, val);

}

/* OVL0_CFG2 */
static void _du_ovl_cfg2(enum odin_channel channel, u8 id, u8 reg_val)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_OVL_RES, channel, ADDR_OVL0_CFG2);

	switch (id) {
	case OVL_SRC6_ALPHA_REG_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_SRC6_ALPHA_REG_EN);
		break;
	case OVL_SRC5_ALPHA_REG_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_SRC5_ALPHA_REG_EN);
		break;
	case OVL_SRC4_ALPHA_REG_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_SRC4_ALPHA_REG_EN);
		break;
	case OVL_SRC3_ALPHA_REG_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_SRC3_ALPHA_REG_EN);
		break;
	case OVL_SRC2_ALPHA_REG_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_SRC2_ALPHA_REG_EN);
		break;
	case OVL_SRC1_ALPHA_REG_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_SRC1_ALPHA_REG_EN);
		break;
	case OVL_SRC0_ALPHA_REG_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_SRC0_ALPHA_REG_EN);
		break;
	case OVL_SRC6_SEL:
		val = DSS_REG_MOD(val, reg_val, OVL0_SRC6_SEL);
		break;
	case OVL_SRC5_SEL:
		val = DSS_REG_MOD(val, reg_val, OVL0_SRC5_SEL);
		break;
	case OVL_SRC4_SEL:
		val = DSS_REG_MOD(val, reg_val, OVL0_SRC4_SEL);
		break;
	case OVL_SRC3_SEL:
		val = DSS_REG_MOD(val, reg_val, OVL0_SRC3_SEL);
		break;
	case OVL_SRC2_SEL:
		val = DSS_REG_MOD(val, reg_val, OVL0_SRC2_SEL);
		break;
	case OVL_SRC1_SEL:
		val = DSS_REG_MOD(val, reg_val, OVL0_SRC1_SEL);
		break;
	case OVL_SRC0_SEL:
		val = DSS_REG_MOD(val, reg_val, OVL0_SRC0_SEL);
		break;
	default:
		break;
	}

	dss_write_reg(ODIN_DSS_OVL_RES, channel, ADDR_OVL0_CFG2, val);
}

static void _du_ovl_src_alpha_reg_en(enum odin_channel channel,
	u8 layer_number, u8 ovl_src_alpha_reg_en)
{
	u8 id;
	switch (layer_number) {
	case 6:
		id = OVL_SRC6_ALPHA_REG_EN;
		break;
	case 5:
		id = OVL_SRC5_ALPHA_REG_EN;
		break;
	case 4:
		id = OVL_SRC4_ALPHA_REG_EN;
		break;
	case 3:
		id = OVL_SRC3_ALPHA_REG_EN;
		break;
	case 2:
		id = OVL_SRC2_ALPHA_REG_EN;
		break;
	case 1:
		id = OVL_SRC1_ALPHA_REG_EN;
		break;
	case 0:
		id = OVL_SRC0_ALPHA_REG_EN;
		break;
	default:
		DSSERR("_du_ovl_src_alpha_reg_en error \n");
		BUG_ON(1);
		return;
	}

	_du_ovl_cfg2(channel, id, ovl_src_alpha_reg_en);
}

static void _du_ovl_src_sel(enum odin_channel channel, u8 layer_number,
	u8 ovl_src_sel)
{
	u8 id;
	switch (layer_number) {
	case 6:
		id = OVL_SRC6_SEL;
		break;
	case 5:
		id = OVL_SRC5_SEL;
		break;
	case 4:
		id = OVL_SRC4_SEL;
		break;
	case 3:
		id = OVL_SRC3_SEL;
		break;
	case 2:
		id = OVL_SRC2_SEL;
		break;
	case 1:
		id = OVL_SRC1_SEL;
		break;
	case 0:
		id = OVL_SRC0_SEL;
		break;
	default:
		DSSERR("_du_ovl_src_sel error \n");
		BUG_ON(1);
		return;
	}

	_du_ovl_cfg2(channel, id, ovl_src_sel);
}

static void _du_ovl_src_sel_total(enum odin_channel channel, u32 ovl_sel)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_OVL_RES, channel, ADDR_OVL0_CFG2);

	val = ovl_sel | (val & 0xff000000);

	dss_write_reg(ODIN_DSS_OVL_RES, channel, ADDR_OVL0_CFG2, val);
}

static u32 _du_ovl_get_src_sel_total(enum odin_channel channel)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_OVL_RES, channel, ADDR_OVL0_CFG2);

	return val;
}

/* OVL0_ALPHA_REG0,1 */
static void _du_ovl_alpha_reg(enum odin_channel channel, u8 layer_number,
	u8 ovl_alpha_reg)
{
	u32 val;
	u32 reg_addr;

	if (layer_number >3)
		reg_addr = ADDR_OVL0_ALPHA_REG1;
	else
		reg_addr = ADDR_OVL0_ALPHA_REG0;

	val = dss_read_reg(ODIN_DSS_OVL_RES, channel, reg_addr);

	switch (layer_number) {
	case 6:
		val = DSS_REG_MOD(val, ovl_alpha_reg, OVL0_SRC6_ALPHA);
		break;
	case 5:
		val = DSS_REG_MOD(val, ovl_alpha_reg, OVL0_SRC5_ALPHA);
		break;
	case 4:
		val = DSS_REG_MOD(val, ovl_alpha_reg, OVL0_SRC4_ALPHA);
		break;
	case 3:
		val = DSS_REG_MOD(val, ovl_alpha_reg, OVL0_SRC3_ALPHA);
		break;
	case 2:
		val = DSS_REG_MOD(val, ovl_alpha_reg, OVL0_SRC2_ALPHA);
		break;
	case 1:
		val = DSS_REG_MOD(val, ovl_alpha_reg, OVL0_SRC1_ALPHA);
		break;
	case 0:
		val = DSS_REG_MOD(val, ovl_alpha_reg, OVL0_SRC0_ALPHA);
		break;
	default:
		DSSERR("_du_ovl_alpha_reg error \n");
		BUG_ON(1);
		return;
	}

	dss_write_reg(ODIN_DSS_OVL_RES, channel, reg_addr, val);
}


/* OVL0_CFG3 */
static void _du_ovl_cfg3(enum odin_channel channel,
	enum ovl_cfg3 id, u8 reg_val)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_OVL_RES, channel, ADDR_OVL0_CFG3);

	switch (id) {
	case OVL_SRC2_MUX_SEL:
		val = DSS_REG_MOD(val, reg_val, OVL0_SRC2_MUX_SEL);
		break;
	default:
		break;
	}

	dss_write_reg(ODIN_DSS_OVL_RES, channel, ADDR_OVL0_CFG3, val);
}
static void _du_ovl_src2_mux_sel(enum odin_channel channel, u8 src2_mux_sel)
{
	_du_ovl_cfg3(channel, OVL_SRC2_MUX_SEL, src2_mux_sel);
}

/* OVL_CFG_G0 */
static void _du_ovl_cfg_go_0(u8 ovl_src_num, u8 ovl_src_en)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_OVL_RES, 0, ADDR_OVL_CFG_G0);

	switch (ovl_src_num) {
	case 6:
		val = DSS_REG_MOD(val, ovl_src_en, OVL0_SRC6_EN);
		break;
	case 5:
		val = DSS_REG_MOD(val, ovl_src_en, OVL0_SRC5_EN);
		break;
	case 4:
		val = DSS_REG_MOD(val, ovl_src_en, OVL0_SRC4_EN);
		break;
	case 3:
		val = DSS_REG_MOD(val, ovl_src_en, OVL0_SRC3_EN);
		break;
	case 2:
		val = DSS_REG_MOD(val, ovl_src_en, OVL0_SRC2_EN);
		break;
	case 1:
		val = DSS_REG_MOD(val, ovl_src_en, OVL0_SRC1_EN);
		break;
	case 0:
		val = DSS_REG_MOD(val, ovl_src_en, OVL0_SRC0_EN);
		break;
	default:
		DSSERR("_du_ovl_cfg_go_0 error \n");
		BUG_ON(1);
		return;
	}

	dss_write_reg(ODIN_DSS_OVL_RES, 0, ADDR_OVL_CFG_G0, val);
}

static void _du_ovl_cfg_go_1(u8 ovl_src_num, u8 ovl_src_en)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_OVL_RES, 0, ADDR_OVL_CFG_G0);

	switch (ovl_src_num) {
	case 6:
		val = DSS_REG_MOD(val, ovl_src_en, OVL1_SRC6_EN);
		break;
	case 5:
		val = DSS_REG_MOD(val, ovl_src_en, OVL1_SRC5_EN);
		break;
	case 4:
		val = DSS_REG_MOD(val, ovl_src_en, OVL1_SRC4_EN);
		break;
	case 3:
		val = DSS_REG_MOD(val, ovl_src_en, OVL1_SRC3_EN);
		break;
	case 2:
		val = DSS_REG_MOD(val, ovl_src_en, OVL1_SRC2_EN);
		break;
	case 1:
		val = DSS_REG_MOD(val, ovl_src_en, OVL1_SRC1_EN);
		break;
	case 0:
		val = DSS_REG_MOD(val, ovl_src_en, OVL1_SRC0_EN);
		break;
	default:
		DSSERR("_du_ovl_cfg_go_1 error \n");
		BUG_ON(1);
		return;
	}

	dss_write_reg(ODIN_DSS_OVL_RES, 0, ADDR_OVL_CFG_G0, val);
}

static void _du_ovl_cfg_go_2(u8 ovl_src_num, u8 ovl_src_en)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_OVL_RES, 0, ADDR_OVL_CFG_G0);

	switch (ovl_src_num) {
	case 6:
		val = DSS_REG_MOD(val, ovl_src_en, OVL2_SRC6_EN);
		break;
	case 5:
		val = DSS_REG_MOD(val, ovl_src_en, OVL2_SRC5_EN);
		break;
	case 4:
		val = DSS_REG_MOD(val, ovl_src_en, OVL2_SRC4_EN);
		break;
	case 3:
		val = DSS_REG_MOD(val, ovl_src_en, OVL2_SRC3_EN);
		break;
	case 2:
		val = DSS_REG_MOD(val, ovl_src_en, OVL2_SRC2_EN);
		break;
	case 1:
		val = DSS_REG_MOD(val, ovl_src_en, OVL2_SRC1_EN);
		break;
	case 0:
		val = DSS_REG_MOD(val, ovl_src_en, OVL2_SRC0_EN);
		break;
	default:
		DSSERR("_du_ovl_cfg_go_2 error \n");
		BUG_ON(1);
		return;
	}

	dss_write_reg(ODIN_DSS_OVL_RES, 0, ADDR_OVL_CFG_G0, val);
}

static u8 _du_ovl_rd_cfg_go_0(enum odin_dma_plane plane)
{
	u32 val;
	u8 ret;

	val = dss_read_reg(ODIN_DSS_OVL_RES, 0, ADDR_OVL_CFG_G0);

	switch (plane) {
	case ODIN_DSS_VID0:
		ret = DSS_REG_GET(val, OVL0_SRC0_EN);
		break;
	case ODIN_DSS_VID1:
		ret = DSS_REG_GET(val, OVL0_SRC1_EN);
		break;
	case ODIN_DSS_VID2_FMT:
		ret = DSS_REG_GET(val, OVL0_SRC2_EN);
		break;
	case ODIN_DSS_GRA0:
		ret = DSS_REG_GET(val, OVL0_SRC3_EN);
		break;
	case ODIN_DSS_GRA1:
		ret = DSS_REG_GET(val, OVL0_SRC4_EN);
		break;
	case ODIN_DSS_GRA2:
		ret = DSS_REG_GET(val, OVL0_SRC5_EN);
		break;
	case ODIN_DSS_mSCALER:
		ret = DSS_REG_GET(val, OVL0_SRC6_EN);
		break;
	default:
		ret = -EINVAL;
		DSSERR("_du_ovl_rd_cfg_go_0 error\n");
		BUG_ON(1);
		break;
	}
	return ret;
}
static u8 _du_ovl_rd_cfg_go_1(enum odin_dma_plane plane)
{
	u32 val;
	u8 ret;

	val = dss_read_reg(ODIN_DSS_OVL_RES, 0, ADDR_OVL_CFG_G0);

	switch (plane) {
	case ODIN_DSS_VID0:
		ret = DSS_REG_GET(val, OVL1_SRC0_EN);
		break;
	case ODIN_DSS_VID1:
		ret = DSS_REG_GET(val, OVL1_SRC1_EN);
		break;
	case ODIN_DSS_VID2_FMT:
		ret = DSS_REG_GET(val, OVL1_SRC2_EN);
		break;
	case ODIN_DSS_GRA0:
		ret = DSS_REG_GET(val, OVL1_SRC3_EN);
		break;
	case ODIN_DSS_GRA1:
		ret = DSS_REG_GET(val, OVL1_SRC4_EN);
		break;
	case ODIN_DSS_GRA2:
		ret = DSS_REG_GET(val, OVL1_SRC5_EN);
		break;
	case ODIN_DSS_mSCALER:
		ret = DSS_REG_GET(val, OVL1_SRC6_EN);
		break;
	default:
		ret = -EINVAL;
		DSSERR("_du_ovl_rd_cfg_go_1 error\n");
		BUG_ON(1);
		break;
	}
	return ret;
}

static u8 _du_ovl_rd_cfg_go_2(enum odin_dma_plane plane)
{
	u32 val;
	u8 ret;

	val = dss_read_reg(ODIN_DSS_OVL_RES, 0, ADDR_OVL_CFG_G0);

	switch (plane) {
	case ODIN_DSS_VID0:
		ret = DSS_REG_GET(val, OVL2_SRC0_EN);
		break;
	case ODIN_DSS_VID1:
		ret = DSS_REG_GET(val, OVL2_SRC1_EN);
		break;
	case ODIN_DSS_VID2_FMT:
		ret = DSS_REG_GET(val, OVL2_SRC2_EN);
		break;
	case ODIN_DSS_GRA0:
		ret = DSS_REG_GET(val, OVL2_SRC3_EN);
		break;
	case ODIN_DSS_GRA1:
		ret = DSS_REG_GET(val, OVL2_SRC4_EN);
		break;
	case ODIN_DSS_GRA2:
		ret = DSS_REG_GET(val, OVL2_SRC5_EN);
		break;
	case ODIN_DSS_mSCALER:
		ret = DSS_REG_GET(val, OVL2_SRC6_EN);
		break;
	default:
		ret = -EINVAL;
		DSSERR("_du_ovl_rd_cfg_go_2 error\n");
		BUG_ON(1);
		break;
	}
	return ret;
}

static u32 _du_ovl_get_cfg_go(void)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_OVL_RES, 0, ADDR_OVL_CFG_G0);

	return val;
}

/* OVL_S0_PIP_STARTX */
static void _du_ovl_source_pip_startxy(u8 ovl_src_num,
	u16 start_x, u32 start_y)
{
	u32 val;
	u32 reg_addr;
	switch (ovl_src_num) {
	case 6:
		reg_addr= ADDR_OVL_S6_PIP_STARTXY;
		break;
	case 5:
		reg_addr= ADDR_OVL_S5_PIP_STARTXY;
		break;
	case 4:
		reg_addr= ADDR_OVL_S4_PIP_STARTXY;
		break;
	case 3:
		reg_addr= ADDR_OVL_S3_PIP_STARTXY;
		break;
	case 2:
		reg_addr= ADDR_OVL_S2_PIP_STARTXY;
		break;
	case 1:
		reg_addr= ADDR_OVL_S1_PIP_STARTXY;
		break;
	case 0:
		reg_addr= ADDR_OVL_S0_PIP_STARTXY;
		break;
	default:
		DSSERR("_du_ovl_source_pip_startxy error \n");
		BUG_ON(1);
		return;
	}

	val = dss_read_reg(ODIN_DSS_OVL_RES, 0, reg_addr);
	val = DSS_REG_MOD(val, start_y, OVL_SRC0_PIP_SY);
	val = DSS_REG_MOD(val, start_x, OVL_SRC0_PIP_SX);
	dss_write_reg(ODIN_DSS_OVL_RES, 0, reg_addr, val);
}

static u32 _du_ovl_get_source_pip_pip_startxy(u8 ovl_src_num)
{
	u32 val;
	u32 reg_addr;

	switch (ovl_src_num) {
	case 6:
		reg_addr= ADDR_OVL_S6_PIP_STARTXY;
		break;
	case 5:
		reg_addr= ADDR_OVL_S5_PIP_STARTXY;
		break;
	case 4:
		reg_addr= ADDR_OVL_S4_PIP_STARTXY;
		break;
	case 3:
		reg_addr= ADDR_OVL_S3_PIP_STARTXY;
		break;
	case 2:
		reg_addr= ADDR_OVL_S2_PIP_STARTXY;
		break;
	case 1:
		reg_addr= ADDR_OVL_S1_PIP_STARTXY;
		break;
	case 0:
		reg_addr= ADDR_OVL_S0_PIP_STARTXY;
		break;
	default:
		DSSERR("_du_ovl_source_pip_startxy error \n");
		BUG_ON(1);
		return 0;
	}

	val = dss_read_reg(ODIN_DSS_OVL_RES, 0, reg_addr);
	return val;
}

/* OVL_S0_PIP_SIZEXY */
static void _du_ovl_source_pip_sizexy(u8 ovl_src_num, u16 size_x, u32 size_y)
{
	u32 val;
	u32 reg_addr;

	switch (ovl_src_num) {
	case 6:
		reg_addr = ADDR_OVL_S6_PIP_SIZEXY;
		break;
	case 5:
		reg_addr = ADDR_OVL_S5_PIP_SIZEXY;
		break;
	case 4:
		reg_addr = ADDR_OVL_S4_PIP_SIZEXY;
		break;
	case 3:
		reg_addr = ADDR_OVL_S3_PIP_SIZEXY;
		break;
	case 2:
		reg_addr = ADDR_OVL_S2_PIP_SIZEXY;
		break;
	case 1:
		reg_addr = ADDR_OVL_S1_PIP_SIZEXY;
		break;
	case 0:
		reg_addr = ADDR_OVL_S0_PIP_SIZEXY;
		break;
	default:
		DSSERR("_du_ovl_source_pip_sizexy error \n");
		BUG_ON(1);
		return;
	}

	val = dss_read_reg(ODIN_DSS_OVL_RES, 0, reg_addr);
	val = DSS_REG_MOD(val, size_y, OVL_SRC0_PIP_SIZEY);
	val = DSS_REG_MOD(val, size_x, OVL_SRC0_PIP_SIZEX);
	dss_write_reg(ODIN_DSS_OVL_RES, 0, reg_addr, val);
}

static u32 _du_ovl_get_source_pip_sizexy(u8 ovl_src_num)
{
	u32 val;
	u32 reg_addr;

	switch (ovl_src_num) {
	case 6:
		reg_addr = ADDR_OVL_S6_PIP_SIZEXY;
		break;
	case 5:
		reg_addr = ADDR_OVL_S5_PIP_SIZEXY;
		break;
	case 4:
		reg_addr = ADDR_OVL_S4_PIP_SIZEXY;
		break;
	case 3:
		reg_addr = ADDR_OVL_S3_PIP_SIZEXY;
		break;
	case 2:
		reg_addr = ADDR_OVL_S2_PIP_SIZEXY;
		break;
	case 1:
		reg_addr = ADDR_OVL_S1_PIP_SIZEXY;
		break;
	case 0:
		reg_addr = ADDR_OVL_S0_PIP_SIZEXY;
		break;
	default:
		DSSERR("_du_ovl_source_pip_sizexy error \n");
		BUG_ON(1);
		return 0;
	}

	val = dss_read_reg(ODIN_DSS_OVL_RES, 0, reg_addr);
	return val;
}

#if 0
/* OVL_SW_UPDATE */
static void _du_ovl_sw_update(enum odin_resource_index resource_index,
	u8 id, u8 reg_val)
{
	u32 val;

	val = dss_read_reg(resource_index, 0, ADDR_OVL_SW_UPDATE);

	switch (id) {
	case OVL0_REG_SW_UPDATE:
		val = DSS_REG_MOD(val, reg_val, OVL0_REG_SW_UPDATE);
		break;
	case OVL1_REG_SW_UPDATE:
		val = DSS_REG_MOD(val, reg_val, OVL1_REG_SW_UPDATE);
		break;
	case OVL2_REG_SW_UPDATE:
		val = DSS_REG_MOD(val, reg_val, OVL2_REG_SW_UPDATE);
		break;
	case OVL0_SAVE_PIP_POS:
		val = DSS_REG_MOD(val, reg_val, OVL0_SAVE_PIP_SRR);
		break;
	case OVL1_SAVE_PIP_POS:
		val = DSS_REG_MOD(val, reg_val, OVL1_SAVE_PIP_SRR);
		break;
	case OVL2_SAVE_PIP_POS:
		val = DSS_REG_MOD(val, reg_val, OVL2_SAVE_PIP_SRR);
		break;
	default:
		break;
	}
	dss_write_reg(resource_index, 0, ADDR_OVL_SW_UPDATE, val);
}

/* OVL_STATUS */
static u32 _du_ovl_status(enum odin_resource_index resource_index,
	enum odin_channel channel)
{
	u32 val;

	switch (channel)
	{
	case ODIN_DSS_CHANNEL_LCD0:
		val = dss_read_reg(resource_index, 0, ADDR_OVL0_STATUS);
		break;
	case ODIN_DSS_CHANNEL_LCD1:
		val = dss_read_reg(resource_index, 0, ADDR_OVL1_STATUS);
		break;
	case ODIN_DSS_CHANNEL_HDMI:
		val = dss_read_reg(resource_index, 0, ADDR_OVL2_STATUS);
		break;
	default:
		DSSERR("_du_ovl_status chnnel error \n");
		BUG_ON(1);
		break;
	}

	return val;
}
#endif

/* SYNC_CFG0 */
static void _du_sync_cfg0(enum sync_gen_cfg0 id, u8 reg_val)
{
	u32 val;
	unsigned long flags;

	spin_lock_irqsave(&du.sync_lock, flags);

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_CFG0);

	switch (id) {
	case SYNC_SYNC0_ENABLE:
		val = DSS_REG_MOD(val, reg_val, SYNC0_ENABLE);
		break;
	case SYNC_SYNC1_ENABLE:
		val = DSS_REG_MOD(val, reg_val, SYNC1_ENABLE);
		break;
	case SYNC_SYNC2_ENABLE:
		val = DSS_REG_MOD(val, reg_val, SYNC2_ENABLE);
		break;
	case SYNC_SYNC0_MODE:
		val = DSS_REG_MOD(val, reg_val, SYNC0_MODE);
		break;
	case SYNC_SYNC1_MODE:
		val = DSS_REG_MOD(val, reg_val, SYNC1_MODE);
		break;
	case SYNC_SYNC2_MODE:
		val = DSS_REG_MOD(val, reg_val, SYNC2_MODE);
		break;
	case SYNC_SYNC0_INT_ENA:
		val = DSS_REG_MOD(val, reg_val, SYNC0_INT_ENA);
		break;
	case SYNC_SYNC1_INT_ENA:
		val = DSS_REG_MOD(val, reg_val, SYNC1_INT_ENA);
		break;
	case SYNC_SYNC2_INT_ENA:
		val = DSS_REG_MOD(val, reg_val, SYNC2_INT_ENA);
		break;
	case SYNC_SYNC0_INT_SRC_SEL:
		val = DSS_REG_MOD(val, reg_val, SYNC0_INT_SRC_SEL);
		break;
	case SYNC_SYNC1_INT_SRC_SEL:
		val = DSS_REG_MOD(val, reg_val, SYNC1_INT_SRC_SEL);
		break;
	case SYNC_SYNC2_INT_SRC_SEL:
		val = DSS_REG_MOD(val, reg_val, SYNC2_INT_SRC_SEL);
		break;
	case SYNC_SYNC0_DISP_SYNC_SRC_SEL:
		val = DSS_REG_MOD(val, reg_val, SYNC0_DISP_SYNC_SRC_SEL);
		break;
	case SYNC_SYNC1_DISP_SYNC_SRC_SEL:
		val = DSS_REG_MOD(val, reg_val, SYNC1_DISP_SYNC_SRC_SEL);
		break;
	case SYNC_SYNC2_DISP_SYNC_SRC_SEL:
		val = DSS_REG_MOD(val, reg_val, SYNC2_DISP_SYNC_SRC_SEL);
		break;
	default:
		break;
	}

	dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_CFG0, val);

	spin_unlock_irqrestore(&du.sync_lock, flags);
}

static void _du_set_channel_sync_enable(enum odin_channel channel, u8 enable)
{
	switch (channel) {
	case ODIN_DSS_CHANNEL_LCD0:
		_du_sync_cfg0(SYNC_SYNC0_ENABLE, enable);
		break;
	case ODIN_DSS_CHANNEL_LCD1:
		_du_sync_cfg0(SYNC_SYNC1_ENABLE, enable);
		break;
	case ODIN_DSS_CHANNEL_HDMI:
		_du_sync_cfg0(SYNC_SYNC2_ENABLE, enable);
		break;
	default:
		break;
	}
}

static void _du_set_channel_sync_mode(enum odin_channel channel,
	u8 is_continuous)
{
	switch (channel) {
	case ODIN_DSS_CHANNEL_LCD0:
		_du_sync_cfg0(SYNC_SYNC0_MODE, is_continuous);
		break;
	case ODIN_DSS_CHANNEL_LCD1:
		_du_sync_cfg0(SYNC_SYNC1_MODE, is_continuous);
		break;
	case ODIN_DSS_CHANNEL_HDMI:
		_du_sync_cfg0(SYNC_SYNC2_MODE, is_continuous);
		break;
	default:
		break;
	}
}

#if 0
static u8 _du_get_channel_sync_mode(enum odin_channel channel)
{
	u32 val;
	u8 ret = 0;

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_CFG0);

	switch (channel) {
	case ODIN_DSS_CHANNEL_LCD0:
		ret = DSS_REG_GET(val, SYNC0_MODE);	/* SYNC0_MODE */
		break;
	case ODIN_DSS_CHANNEL_LCD1:
		ret = DSS_REG_GET(val, SYNC1_MODE);	/* SYNC1_MODE */
		break;
	case ODIN_DSS_CHANNEL_HDMI:
		ret = DSS_REG_GET(val, SYNC2_MODE);	/* SYNC2_MODE */
		break;
	default:
		break;
	}

	return ret;
}
#endif

static void _du_set_channel_sync_int_ena(enum odin_channel channel, u8 int_en)
{
	switch (channel) {
	case ODIN_DSS_CHANNEL_LCD0:
		_du_sync_cfg0(SYNC_SYNC0_INT_ENA, int_en);
		break;
	case ODIN_DSS_CHANNEL_LCD1:
		_du_sync_cfg0(SYNC_SYNC1_INT_ENA, int_en);
		break;
	case ODIN_DSS_CHANNEL_HDMI:
		_du_sync_cfg0(SYNC_SYNC2_INT_ENA, int_en);
		break;
	default:
		break;
	}
}

static void _du_set_channel_sync_int_src_sel(enum odin_channel channel,
	u8 int_src_sel)
{
	switch (channel) {
	case ODIN_DSS_CHANNEL_LCD0:
		_du_sync_cfg0(SYNC_SYNC0_INT_SRC_SEL, int_src_sel);
		break;
	case ODIN_DSS_CHANNEL_LCD1:
		_du_sync_cfg0(SYNC_SYNC1_INT_SRC_SEL, int_src_sel);
		break;
	case ODIN_DSS_CHANNEL_HDMI:
		_du_sync_cfg0(SYNC_SYNC2_INT_SRC_SEL, int_src_sel);
		break;
	default:
		break;
	}
}

static void _du_set_channel_sync_disp_sync_src_sel(u8 sync_numeber,
	u8 disp_sync_src_sel)
{
	switch (sync_numeber) {
	case ODIN_DSS_CHANNEL_LCD0:
		_du_sync_cfg0(SYNC_SYNC0_DISP_SYNC_SRC_SEL, disp_sync_src_sel);
		break;
	case ODIN_DSS_CHANNEL_LCD1:
		_du_sync_cfg0(SYNC_SYNC1_DISP_SYNC_SRC_SEL, disp_sync_src_sel);
		break;
	case ODIN_DSS_CHANNEL_HDMI:
		_du_sync_cfg0(SYNC_SYNC2_DISP_SYNC_SRC_SEL, disp_sync_src_sel);
		break;
	default:
		break;
	}
}


/* SYNC_CFG1 */
static void _du_sync_cfg1(enum sync_gen_cfg1_2 id, u8 reg_val)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_CFG1);

	switch (id) {
	case SYNC_GSCL_SYNC_EN0:
		val = DSS_REG_MOD(val, reg_val, GSCL_SYNC_EN0);
		break;
	case SYNC_GRA2_SYNC_EN:
		val = DSS_REG_MOD(val, reg_val, GRA2_SYNC_EN);
		break;
	case SYNC_GRA1_SYNC_EN:
		val = DSS_REG_MOD(val, reg_val, GRA1_SYNC_EN);
		break;
	case SYNC_GRA0_SYNC_EN:
		val = DSS_REG_MOD(val, reg_val, GRA0_SYNC_EN);
		break;
	case SYNC_VID2_SYNC_EN:
		val = DSS_REG_MOD(val, reg_val, VID2_SYNC_EN);
		break;
	case SYNC_VID1_SYNC_EN0:
		val = DSS_REG_MOD(val, reg_val, VID1_SYNC_EN0);
		break;
	case SYNC_VID0_SYNC_EN0:
		val = DSS_REG_MOD(val, reg_val, VID0_SYNC_EN0);
		break;
	case SYNC_OVL2_SYNC_EN:
		val = DSS_REG_MOD(val, reg_val, OVL2_SYNC_EN);
		break;
	case SYNC_OVL1_SYNC_EN:
		val = DSS_REG_MOD(val, reg_val, OVL1_SYNC_EN);
		break;
	case SYNC_OVL0_SYNC_EN:
		val = DSS_REG_MOD(val, reg_val, OVL0_SYNC_EN);
		break;
	default:
		break;
	}

	dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_CFG1, val);
}

/* SYNC_CFG2 */
static void _du_sync_cfg2(enum sync_gen_cfg1_2 id, u8 reg_val)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_CFG2);

	switch (id) {
	case SYNC_GSCL_SYNC_EN1:
		val = DSS_REG_MOD(val, reg_val, GSCL_SYNC_EN1);
		break;
	case SYNC_VID1_SYNC_EN1:
		val = DSS_REG_MOD(val, reg_val, VID1_SYNC_EN1);
		break;
	case SYNC_VID0_SYNC_EN1:
		val = DSS_REG_MOD(val, reg_val, VID0_SYNC_EN1);
		break;
	default:
		break;
	}

	dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_CFG2, val);
}


/* SYNC_RASTER_SIZE0 */
static void _du_sync0_raster_size(u16 sync0_tot_hsize,  u16 sync0_tot_vsize)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_RASTER_SIZE0);

	val = DSS_REG_MOD(val, sync0_tot_hsize, SYNC0_TOT_HSIZE);
	val = DSS_REG_MOD(val, sync0_tot_vsize, SYNC0_TOT_VSIZE);

	dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_RASTER_SIZE0, val);
}

/* SYNC_RASTER_SIZE1 */
static void _du_sync1_raster_size(u16 sync1_tot_hsize,  u16 sync1_tot_vsize)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_RASTER_SIZE1);

	val = DSS_REG_MOD(val, sync1_tot_hsize, SYNC1_TOT_HSIZE);
	val = DSS_REG_MOD(val, sync1_tot_vsize, SYNC1_TOT_VSIZE);

	dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_RASTER_SIZE1, val);
}

/* SYNC_RASTER_SIZE2 */
static void _du_sync2_raster_size(u16 sync2_tot_hsize,  u16 sync2_tot_vsize)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_RASTER_SIZE2);

	val = DSS_REG_MOD(val, sync2_tot_hsize, SYNC2_TOT_HSIZE);
	val = DSS_REG_MOD(val, sync2_tot_vsize, SYNC2_TOT_VSIZE);

	dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_RASTER_SIZE2, val);
}

/* SYNC_OVL0_ST_DLY */
static void _du_sync_ovl0_st_dly(u16 ovl0_h_st_val,  u16 ovl0_v_st_val)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_OVL0_ST_DLY);

	val = DSS_REG_MOD(val, ovl0_h_st_val, OVL0_H_ST_VAL);
	val = DSS_REG_MOD(val, ovl0_v_st_val, OVL0_V_ST_VAL);

	dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_OVL0_ST_DLY, val);
}

/* SYNC_OVL1_ST_DLY */
static void _du_sync_ovl1_st_dly(u16 ovl1_h_st_val,  u16 ovl1_v_st_val)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_OVL1_ST_DLY);

	val = DSS_REG_MOD(val, ovl1_h_st_val, OVL1_H_ST_VAL);
	val = DSS_REG_MOD(val, ovl1_v_st_val, OVL1_V_ST_VAL);

	dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_OVL1_ST_DLY, val);
}

/* SYNC_OVL2_ST_DLY */
static void _du_sync_ovl2_st_dly(u16 ovl2_h_st_val,  u16 ovl2_v_st_val)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_OVL2_ST_DLY);

	val = DSS_REG_MOD(val, ovl2_h_st_val, OVL2_H_ST_VAL);
	val = DSS_REG_MOD(val, ovl2_v_st_val, OVL2_V_ST_VAL);

	dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_OVL2_ST_DLY, val);
}

/* ADDR_SYNC_VID0_ST_DLY0 */
static void _du_sync_vid0_st_dly0(u16 vid0_h_st_val0,  u16 vid0_v_st_val0)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_VID0_ST_DLY0);

	val = DSS_REG_MOD(val, vid0_h_st_val0, VID0_H_ST_VAL0);
	val = DSS_REG_MOD(val, vid0_v_st_val0, VID0_V_ST_VAL0);

	dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_VID0_ST_DLY0, val);
}

/* ADDR_SYNC_VID1_ST_DLY1 */
static void _du_sync_vid1_st_dly0(u16 vid1_h_st_val0,  u16 vid1_v_st_val0)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_VID1_ST_DLY0);

	val = DSS_REG_MOD(val, vid1_h_st_val0, VID1_H_ST_VAL0);
	val = DSS_REG_MOD(val, vid1_v_st_val0, VID1_V_ST_VAL0);

	dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_VID1_ST_DLY0, val);
}

/* ADDR_SYNC_VID2_ST_DLY */
static void _du_sync_vid2_st_dly(u16 vid2_h_st_val,  u16 vid2_v_st_val)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_VID2_ST_DLY);

	val = DSS_REG_MOD(val, vid2_h_st_val, VID2_H_ST_VAL);
	val = DSS_REG_MOD(val, vid2_v_st_val, VID2_V_ST_VAL);

	dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_VID2_ST_DLY, val);
}

/* ADDR_SYNC_GRA0_ST_DLY */
static void _du_sync_gra0_st_dly(u16 gra0_h_st_val,  u16 gra0_v_st_val)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_GRA0_ST_DLY);

	val = DSS_REG_MOD(val, gra0_h_st_val, GRA0_H_ST_VAL);
	val = DSS_REG_MOD(val, gra0_v_st_val, GRA0_V_ST_VAL);

	dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_GRA0_ST_DLY, val);
}

/* ADDR_SYNC_GRA1_ST_DLY */
static void _du_sync_gra1_st_dly(u16 gra1_h_st_val,  u16 gra1_v_st_val)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_GRA1_ST_DLY);

	val = DSS_REG_MOD(val, gra1_h_st_val, GRA1_H_ST_VAL);
	val = DSS_REG_MOD(val, gra1_v_st_val, GRA1_V_ST_VAL);

	dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_GRA1_ST_DLY, val);
}

/* ADDR_SYNC_GRA2_ST_DLY */
static void _du_sync_gra2_st_dly(u16 gra2_h_st_val,  u16 gra2_v_st_val)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_GRA2_ST_DLY);

	val = DSS_REG_MOD(val, gra2_h_st_val, GRA2_H_ST_VAL);
	val = DSS_REG_MOD(val, gra2_v_st_val, GRA2_V_ST_VAL);

	dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_GRA2_ST_DLY, val);
}

/* ADDR_SYNC_GSCL_ST_DLY0 */
static void _du_sync_gscl_st_dly0(u16 gscl_h_st_val0,  u16 gscl_v_st_val0)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_GSCL_ST_DLY0);

	val = DSS_REG_MOD(val, gscl_h_st_val0, GSCL_H_ST_VAL0);
	val = DSS_REG_MOD(val, gscl_v_st_val0, GSCL_V_ST_VAL0);

	dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_GSCL_ST_DLY0, val);
}

/* ADDR_SYNC_VID0_ST_DLY1 */
static void _du_sync_vid0_st_dly1(u16 vid0_h_st_val1,  u16 vid0_v_st_val1)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_VID0_ST_DLY1);

	val = DSS_REG_MOD(val, vid0_h_st_val1, VID0_H_ST_VAL1);
	val = DSS_REG_MOD(val, vid0_v_st_val1, VID0_V_ST_VAL1);

	dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_VID0_ST_DLY1, val);
}

/* ADDR_SYNC_VID1_ST_DLY1 */
static void _du_sync_vid1_st_dly1(u16 vid1_h_st_val1,  u16 vid1_v_st_val1)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_VID1_ST_DLY1);

	val = DSS_REG_MOD(val, vid1_h_st_val1, VID1_H_ST_VAL1);
	val = DSS_REG_MOD(val, vid1_v_st_val1, VID1_V_ST_VAL1);

	dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_VID1_ST_DLY1, val);
}

/* ADDR_SYNC_GSCL_ST_DLY1 */
static void _du_sync_gscl_st_dly1(u16 gscl_h_st_val1,  u16 gscl_v_st_val1)
{
	u32 val;

	val = dss_read_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_GSCL_ST_DLY1);

	val = DSS_REG_MOD(val, gscl_h_st_val1, GSCL_H_ST_VAL1);
	val = DSS_REG_MOD(val, gscl_v_st_val1, GSCL_V_ST_VAL1);

	dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC_GSCL_ST_DLY1, val);
}

/* ADDR_SYNC0_ACT_SIZE */
static u32 _du_sync_set_act_size(enum odin_channel channel, u16 act_vsize,
	u16 act_hsize)
{
	u32 val = 0;

	switch (channel)
	{
		case ODIN_DSS_CHANNEL_LCD0:
			val = DSS_REG_MOD(val, act_hsize, SYNC0_ACT_HSIZE);
			val = DSS_REG_MOD(val, act_vsize, SYNC0_ACT_VSIZE);
			dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC0_ACT_SIZE, val);
			break;
		case ODIN_DSS_CHANNEL_LCD1:
			val = DSS_REG_MOD(val, act_hsize, SYNC1_ACT_HSIZE);
			val = DSS_REG_MOD(val, act_vsize, SYNC1_ACT_VSIZE);
			dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC1_ACT_SIZE, val);
			break;
		case ODIN_DSS_CHANNEL_HDMI:
			val = DSS_REG_MOD(val, act_hsize, SYNC2_ACT_HSIZE);
			val = DSS_REG_MOD(val, act_vsize, SYNC2_ACT_VSIZE);
			dss_write_reg(ODIN_DSS_SYNC_GEN_RES, 0, ADDR_SYNC2_ACT_SIZE, val);
			break;
		default:
			DSSERR("_du_sync_get_act_size error \n");
			BUG_ON(1);
			break;
	}

	return val;
}


/***********************************************************************/


/****************************DU Common*********************************/
#if 0
static int du_get_clocks(void)
{
	return 0;
}

static void du_put_clocks(void)
{
}

int du_runtime_get(void)
{
	return 0;
}

void du_runtime_put(void)
{
	/* TODO: */
}
#endif

static enum odin_resource_index du_get_dma_resource_index
	(enum odin_dma_plane plane)
{
	switch (plane) {
	case ODIN_DSS_VID0:
		return ODIN_DSS_VID0_RES;

	case ODIN_DSS_VID1:
		return ODIN_DSS_VID1_RES;

	case ODIN_DSS_VID2_FMT:
		return ODIN_DSS_VID2_RES;

	case ODIN_DSS_GRA0:
		return ODIN_DSS_GRA0_RES;

	case ODIN_DSS_GRA1:
		return ODIN_DSS_GRA1_RES;

	case ODIN_DSS_GRA2:
		return ODIN_DSS_GRA2_RES;

	case ODIN_DSS_mSCALER:
		return ODIN_DSS_GSCL_RES;

 	default:
		DSSERR("du_get_dma_resource_index error \n");
		BUG_ON(1);
		return -EINVAL;
	}
}

static void du_set_cabc_hist_lut_operation_mode
	(enum odin_resource_index resource_index, u8 reg_val)
{
	if (resource_index == ODIN_DSS_CABC_RES)
		_du_set_cabc_hist_lut_operation_mode(resource_index, reg_val);

}

static void du_set_plane_rd_y_rgb_addr
	(enum odin_resource_index resource_index, u8 fb_num, u32 paddr)
{
	if ((resource_index == ODIN_DSS_GRA0_RES) ||
		(resource_index == ODIN_DSS_GRA1_RES) ||
	    (resource_index == ODIN_DSS_GRA2_RES))
		_du_gra_dss_base_y_addr(resource_index, fb_num, paddr);
	else
		_du_vid_dss_base_y_addr(resource_index, fb_num, paddr);
}

static void du_set_plane_rd_u_addr(enum odin_resource_index resource_index,
	u8 fb_num, u32 paddr)
{
	if ((resource_index == ODIN_DSS_GRA0_RES) ||
		(resource_index == ODIN_DSS_GRA1_RES) ||
	    (resource_index == ODIN_DSS_GRA2_RES))
	{
#if 0
		DSSERR("du_set_plane_rd_u_addr error \n");
		BUG_ON(1);
#endif
		return;
	}
	else
		_du_vid_dss_base_u_addr(resource_index, fb_num, paddr);
}

static void du_set_plane_rd_v_addr(enum odin_resource_index resource_index,
	u8 fb_num, u32 paddr)
{
	if ((resource_index == ODIN_DSS_GRA0_RES) ||
		(resource_index == ODIN_DSS_GRA1_RES) ||
	    (resource_index == ODIN_DSS_GRA2_RES))
	{
#if 0
		DSSERR("du_set_plane_rd_v_addr error \n");
		BUG_ON(1);
#endif
		return;
	}
	else
		_du_vid_dss_base_v_addr(resource_index, fb_num, paddr);
}

static void du_set_plane_rd_width_height
	(enum odin_resource_index resource_index, u16 width, u16 height)
{
	if ((resource_index == ODIN_DSS_GRA0_RES) ||
		(resource_index == ODIN_DSS_GRA1_RES) ||
	    (resource_index == ODIN_DSS_GRA2_RES))
	{
		_du_gra_rd_width_hegiht(resource_index, width, height);
	}
	else
		_du_vid_rd_width_hegiht(resource_index, width, height);
}

static void du_set_plane_rd_size(enum odin_resource_index resource_index,
	u16 size_x, u32 size_y)
{
	if ((resource_index == ODIN_DSS_GRA0_RES) ||
		(resource_index == ODIN_DSS_GRA1_RES) ||
	    (resource_index == ODIN_DSS_GRA2_RES))
	{
		_du_gra_rd_sizexy(resource_index, size_x, size_y);
	}
	else
		_du_vid_rd_sizexy(resource_index, size_x, size_y);
}

#if 0
static void du_get_plane_rd_size(enum odin_resource_index resource_index,
	u16 size_x, u32 size_y)
{
	if ((resource_index == ODIN_DSS_GRA0_RES) ||
		(resource_index == ODIN_DSS_GRA1_RES) ||
	    (resource_index == ODIN_DSS_GRA2_RES))
	{
		_du_gra_rd_sizexy(resource_index, size_x, size_y);
	}
	else
		_du_vid_rd_sizexy(resource_index, size_x, size_y);
}
#endif

static void du_set_plane_rd_pos(enum odin_resource_index resource_index,
	u16 pos_x, u16 pos_y)
{
	if ((resource_index == ODIN_DSS_GRA0_RES) ||
		(resource_index == ODIN_DSS_GRA1_RES) ||
	    (resource_index == ODIN_DSS_GRA2_RES))
	{
		_du_gra_rd_startxy(resource_index, pos_x, pos_y);
	}
	else
		_du_vid_rd_startxy(resource_index, pos_x, pos_y);
}

static void du_set_plane_pip_rd_size(enum odin_resource_index resource_index,
	u16 pip_size_x, u32 pip_size_y)
{
	if ((resource_index == ODIN_DSS_VID0_RES) ||
	    (resource_index == ODIN_DSS_VID1_RES))
	{
		_du_vid_rd_pip_sizexy(resource_index, pip_size_x, pip_size_y);
	}
}

static void du_set_plane_sync_start_dly
	(enum odin_resource_index resource_index, u16 h_st_val, u16 v_st_val)
{
	switch (resource_index) {
	case ODIN_DSS_VID0_RES:
		_du_sync_vid0_st_dly0(h_st_val, v_st_val);
		break;
	case ODIN_DSS_VID1_RES:
		_du_sync_vid1_st_dly0(h_st_val, v_st_val);
		break;
	case ODIN_DSS_VID2_RES:
		_du_sync_vid2_st_dly(h_st_val, v_st_val);
		break;
	case ODIN_DSS_GRA0_RES:
		_du_sync_gra0_st_dly(h_st_val, v_st_val);
		break;
	case ODIN_DSS_GRA1_RES:
		_du_sync_gra1_st_dly(h_st_val, v_st_val);
		break;
	case ODIN_DSS_GRA2_RES:
		_du_sync_gra2_st_dly(h_st_val, v_st_val);
		break;
	case ODIN_DSS_GSCL_RES:
		_du_sync_gscl_st_dly0(h_st_val, v_st_val);
		break;
	default:
		printk("sync dly is wrong index resource_index = %d", resource_index);
		break;
	}
}

u32 odin_du_get_src_size_width(enum odin_dma_plane plane)
{
	u32 val, input_width;
	enum odin_resource_index resource_index;

	resource_index = du_get_dma_resource_index(plane);
	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_SIZEXY);
	input_width = DSS_REG_GET(val,  SRC_SIZEX);		/* SRC_SIZEX */

	return input_width;
}

u32 odin_du_get_src_size_height(enum odin_dma_plane plane)
{
	u32 val, input_height;
	enum odin_resource_index resource_index;

	resource_index = du_get_dma_resource_index(plane);
	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_SIZEXY);
	input_height = DSS_REG_GET(val, SRC_SIZEY);		/* SRC_SIZEY */

	return input_height;
}

u32 odin_du_get_src_pipsize_width(enum odin_dma_plane plane)
{
	u32 val, output_width;
	enum odin_resource_index resource_index;

	if (plane > ODIN_DSS_VID1)
	{
		output_width = odin_du_get_src_size_width(plane);
		return output_width;
	}
	resource_index = du_get_dma_resource_index(plane);
	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_PIP_SIZEXY);
	output_width = DSS_REG_GET(val,  SRC_PIP_SIZEX);		/* SRC_SIZEX */

	return output_width;
}

u32 odin_du_get_src_pipsize_height(enum odin_dma_plane plane)
{
	u32 val, output_height;
	enum odin_resource_index resource_index;

	if (plane > ODIN_DSS_VID1)
	{
		output_height = odin_du_get_src_size_height(plane);
		return output_height;
	}
	resource_index = du_get_dma_resource_index(plane);
	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_PIP_SIZEXY);
	output_height = DSS_REG_GET(val, SRC_PIP_SIZEY);		/* SRC_SIZEY */

	return output_height;
}
static void du_set_plane_color_mode(enum odin_channel channel,
	enum odin_resource_index resource_index, enum odin_color_mode color_mode)
{
	u32 color_fmt=0, rgbswap=0, alphaswap=0, yuvswap=0, byteswap=0;
	u32 wordswap=0;

	color_fmt = CHECK_COLOR_VALUE(color_mode,ODIN_COLOR_MODE);

	if (color_fmt >= ODIN_DSS_COLOR_RGB565 &&
		color_fmt <= ODIN_DSS_COLOR_RGB888)
	{
		byteswap = CHECK_COLOR_VALUE(color_mode,ODIN_COLOR_BYTE_SWAP);
		wordswap = CHECK_COLOR_VALUE(color_mode,ODIN_COLOR_WORD_SWAP);
		rgbswap = CHECK_COLOR_VALUE(color_mode,ODIN_COLOR_RGB_SWAP);
		alphaswap = CHECK_COLOR_VALUE(color_mode,ODIN_COLOR_ALPHA_SWAP);

		_du_gra_rd_src_format(resource_index, color_fmt);
		_du_gra_rd_src_byte_swap(resource_index, byteswap);
		_du_gra_rd_src_word_swap(resource_index, wordswap);
		_du_gra_rd_src_rgb_swap(resource_index, rgbswap);
		_du_gra_rd_src_alpha_swap(resource_index, alphaswap);
	}
	else if (color_fmt == ODIN_DSS_COLOR_YUV422S ||\
		(color_fmt >= ODIN_DSS_COLOR_YUV422P_2 &&
		 color_fmt <= ODIN_DSS_COLOR_YUV444P_3))
	{
		byteswap = CHECK_COLOR_VALUE(color_mode,ODIN_COLOR_BYTE_SWAP);
		wordswap = CHECK_COLOR_VALUE(color_mode,ODIN_COLOR_WORD_SWAP);
		yuvswap = CHECK_COLOR_VALUE(color_mode,ODIN_COLOR_YUV_SWAP);

		if ((resource_index >= ODIN_DSS_VID0_RES) &&
			(resource_index <= ODIN_DSS_VID2_RES))
		{
			_du_vid_rd_src_format(resource_index, color_fmt);
			_du_vid_rd_src_byte_swap(resource_index, byteswap);
			_du_vid_rd_src_word_swap(resource_index, wordswap);

			if ((color_fmt >= ODIN_DSS_COLOR_YUV422P_2) &&
				(color_fmt <= ODIN_DSS_COLOR_YUV444P_2))
			{
				_du_vid_rd_src_yuv_swap(resource_index, yuvswap);
			}
		}
		else
		{
			printk("overlay%d can't support YUV format!\n", resource_index);
		}
	}
	else
	{
		printk("unsupported pixel format(%x) in overlay\n", color_mode);
		return;
	}

}

static void du_set_vid_scaling(enum odin_resource_index resource_index,
	u16 width, u16 height, u16 out_width, u16 out_height,
	enum odin_color_mode color_mode)
{
	u16 ratio_x , ratio_y;

	if (resource_index != ODIN_DSS_VID0_RES &&
		resource_index != ODIN_DSS_VID1_RES)
		return;

	if (width != out_width || height != out_height)
	{
		ratio_x = (width * 4096)/out_width;
		ratio_y = (height * 4096)/out_height;
		_du_vid_rd_src_scl_bp_mode(resource_index, 0);
	}
	else
	{
		ratio_x = 4096;
		ratio_y = 4096;
		_du_vid_rd_src_scl_bp_mode(resource_index, 1);
	}

	_du_vid_rd_src_bnd_fill_mode(resource_index, 0);
	_du_vid_rd_scale_ratioxy(resource_index, ratio_x, ratio_y);
}

static void du_rd_src_flip_mode(enum odin_resource_index resource_index,
	u8 flip_mode)
{
	switch (resource_index) {
	case ODIN_DSS_VID0_RES:
	case ODIN_DSS_VID1_RES:
	case ODIN_DSS_VID2_RES:
		_du_vid_rd_src_flip_mode(resource_index, flip_mode);
		break;
	case ODIN_DSS_GRA0_RES:
	case ODIN_DSS_GRA1_RES:
	case ODIN_DSS_GRA2_RES:
	case ODIN_DSS_GSCL_RES:
		_du_gra_rd_src_flip_mode(resource_index, flip_mode);
		break;
	default:
		printk("flip_mode is wrong index resource_index = %d", resource_index);
		break;
	}
}

#if 0
static void du_set_rotation(enum odin_resource_index resource_index,
	u8 rotation, bool mirror)
{
	/*  TODO: */
}
#endif

static void du_set_ovl_ckey_mask(enum odin_channel channel,
	u8 zorder, enum odin_color_mode color_mode)
{
	u8 cop_num;

	if (zorder == 0)
		cop_num = 0;
	else
		cop_num = zorder - 1;

	switch (color_mode) {
	case ODIN_DSS_COLOR_RGB565:
		_du_ovl_layer_ckey_mask_value(channel, cop_num,0xf8, 0xfc, 0xf8);
		break;
	case ODIN_DSS_COLOR_RGB555:
		_du_ovl_layer_ckey_mask_value(channel, cop_num,0xf8, 0xf8, 0xf8);
		break;
	case ODIN_DSS_COLOR_RGB444:
		_du_ovl_layer_ckey_mask_value(channel, cop_num,0xf0, 0xf0, 0xf0);
		break;
	default:
		_du_ovl_layer_ckey_mask_value(channel, cop_num,0xff, 0xff, 0xff);
		break;
	}
}


static void du_set_pre_mult_alpha(enum odin_channel channel,
	u8 zorder, u8 pre_mul_en)
{
	u8 cop_num;

	if (zorder == 0)
		cop_num = 0;
	else
		cop_num = zorder - 1;

	_du_ovl_dst_pre_mul_en(channel, cop_num, pre_mul_en);
}

static void du_setup_global_alpha_value(enum odin_channel channel,
	u8 layer_number, u8 ovl_alpha_reg)
{
	_du_ovl_alpha_reg(channel, layer_number, ovl_alpha_reg);
}

static void du_setup_global_alpha_ena(enum odin_channel channel,
	u8 layer_number, u8 ovl_alpha_en)
{
	_du_ovl_src_alpha_reg_en(channel, layer_number, ovl_alpha_en);
}


static void du_set_alpha_blend(enum odin_channel channel,
	u8 zorder, enum odin_color_mode color_mode,
	u8 blending, u8 global_alpha, bool mxd_use)
{
	if ((blending) && !(mxd_use))
	{
		if (global_alpha == 255) /* Pixel Alpha */
		{
			du_setup_global_alpha_value(channel, zorder, 255);
			du_setup_global_alpha_ena(channel, zorder, 0);
		}
		else /* global alpha */
		{
			du_setup_global_alpha_value(channel, zorder, global_alpha);
			du_setup_global_alpha_ena(channel, zorder, 1);
		}
 	}
	else /* Not Bleding */
	{
		du_setup_global_alpha_value(channel, zorder, 255);
		du_setup_global_alpha_ena(channel, zorder, 1);
	}
}

static void du_setpath_mxd(enum odin_resource_index resource_index,
	bool enable, mXdDu_SCENARIO_Test scenario)
{
	if (enable)
	{
		switch (scenario)
		{
			case MXDDU_SCENARIO_TEST1:
				_du_vid0_1_rd_src_fr_xd_re_path_sel(
					resource_index,
					DU_VID_OUT_PATH_RE_OUT);
				_du_vid0_1_rd_src_to_xd_re_path_sel(
					resource_index,
					DU_VID_SCALE_OUT_PATH_RE_IN);
				_du_vid0_1_rd_src_fr_xd_nr_path_sel(
					resource_index,
					DU_VID_SCALE_IN_PATH_NR);
				_du_vid0_1_rd_src_to_xd_nr_path_sel(
					resource_index,
					DU_VID_XDNR_PATH_ENABLE);
				break;
			case MXDDU_SCENARIO_TEST2:
				_du_vid0_1_rd_src_fr_xd_re_path_sel(
					resource_index,
					DU_VID_SCALE_OUT_PATH_VIDCH);
				_du_vid0_1_rd_src_to_xd_re_path_sel(
					resource_index,
					DU_VID_OUT_PATH_SCALER_OUT);
				_du_vid0_1_rd_src_fr_xd_nr_path_sel(
					resource_index,
					DU_VID_XDNR_PATH_DISABLE);
				_du_vid0_1_rd_src_to_xd_nr_path_sel(
					resource_index,
					DU_VID_SCALE_IN_PATH_RDMA);
				break;

			case MXDDU_SCENARIO_TEST3:
				_du_vid0_1_rd_src_fr_xd_re_path_sel(
					resource_index,
					DU_VID_OUT_PATH_RE_OUT);
				_du_vid0_1_rd_src_to_xd_re_path_sel(
					resource_index,
					DU_VID_SCALE_OUT_PATH_RE_IN);
				_du_vid0_1_rd_src_fr_xd_nr_path_sel(
					resource_index,
					DU_VID_SCALE_IN_PATH_RDMA);
				_du_vid0_1_rd_src_to_xd_nr_path_sel(
					resource_index,
					DU_VID_XDNR_PATH_DISABLE);
				break;

			case MXDDU_SCENARIO_TEST4:
				_du_vid0_1_rd_src_fr_xd_re_path_sel(
					resource_index,
					DU_VID_OUT_PATH_RE_OUT);
				_du_vid0_1_rd_src_to_xd_re_path_sel(
					resource_index,
					DU_VID_SCALE_OUT_PATH_RE_IN);
				_du_vid0_1_rd_src_fr_xd_nr_path_sel(
					resource_index,
					DU_VID_SCALE_IN_PATH_RDMA);
				_du_vid0_1_rd_src_to_xd_nr_path_sel(
					resource_index,
					DU_VID_XDNR_PATH_DISABLE);
				break;

			case MXDDU_SCENARIO_MAX:
				break;
		}
	}
	else
	{
		_du_vid0_1_rd_src_fr_xd_re_path_sel(
			resource_index,
			DU_VID_OUT_PATH_SCALER_OUT);
		_du_vid0_1_rd_src_to_xd_re_path_sel(
			resource_index,
			DU_VID_SCALE_OUT_PATH_VIDCH);
		_du_vid0_1_rd_src_fr_xd_nr_path_sel(
			resource_index,
			DU_VID_SCALE_IN_PATH_RDMA);
		_du_vid0_1_rd_src_to_xd_nr_path_sel(
			resource_index,
			DU_VID_XDNR_PATH_DISABLE);
	}

	_du_vid0_1_rd_vid_xd_path_en(resource_index, (u8)enable);
}

static void du_set_mxd_enable(enum odin_dma_plane plane, bool enable,
	int scenario)
{
	enum odin_resource_index resource_index;

	resource_index = du_get_dma_resource_index(plane);
	du.mxd_enable = enable;
	if (enable)
		du.mxd_plane_mask |= (1 << plane);
	else
		du.mxd_plane_mask &= ~(1 << plane);

	 du_setpath_mxd(resource_index, enable, scenario);
}

static void du_set_formater_enable(enum odin_channel channel, bool enable)
{
	_du_ovl_src2_mux_sel(channel, enable);
}

static void du_set_zorder(enum odin_channel channel,
	enum odin_dma_plane plane, u8 layer_number)
{
	_du_ovl_src_sel(channel, layer_number, plane);
	du.overlay_src_num[plane] = layer_number;
}

static void du_set_src_lsb_sel(enum odin_resource_index resource_index,
	u8 src_lsb_sel)
{
	if ((resource_index == ODIN_DSS_GRA0_RES) ||
		(resource_index == ODIN_DSS_GRA1_RES) ||
	    (resource_index == ODIN_DSS_GRA2_RES))
	{
		_du_gra_rd_src_lsb_sel(resource_index, src_lsb_sel);
	}
	else
	{
		_du_vid_rd_src_lsb_sel(resource_index, src_lsb_sel);
	}
}

static void du_set_ovl_src_size_pos(u8 zorder,
	u16 size_x, u16 size_y, u16 pos_x, u16 pos_y)
{
	_du_ovl_source_pip_sizexy(zorder, size_x, size_y);
	_du_ovl_source_pip_startxy(zorder, pos_x, pos_y);
}

static u32 du_get_ovl_src_size(u8 zorder)
{
	return _du_ovl_get_source_pip_sizexy(zorder);
}

static u32 du_get_ovl_src_pos(u8 zorder)
{
	return _du_ovl_get_source_pip_pip_startxy(zorder);
}


static void du_enable_overlay_src(enum odin_channel channel,
	enum odin_dma_plane plane, bool enable)
{
	switch (channel) {
	case ODIN_DSS_CHANNEL_LCD0:
		_du_ovl_cfg_go_0(plane, enable);
		break;
	case ODIN_DSS_CHANNEL_LCD1:
		_du_ovl_cfg_go_1(plane, enable);
		break;
	case ODIN_DSS_CHANNEL_HDMI:
		_du_ovl_cfg_go_2(plane, enable);
		break;
	default:
		break;
	}
}

static u8 du_rd_overlay_src(enum odin_channel channel,
		enum odin_dma_plane plane)
{
	u8 ret = 0;

	switch (channel) {
	case ODIN_DSS_CHANNEL_LCD0:
		ret = _du_ovl_rd_cfg_go_0(plane);
		break;
	case ODIN_DSS_CHANNEL_LCD1:
		ret = _du_ovl_rd_cfg_go_1(plane);
		break;
	case ODIN_DSS_CHANNEL_HDMI:
		ret = _du_ovl_rd_cfg_go_2(plane);
		break;
	default:
		break;
	}

	return ret;
}

static u32 du_get_ovl_src_go(void)
{
	return _du_ovl_get_cfg_go();
}

int odin_du_check_overlay_src_using_in_ch(enum odin_channel channel,
	enum odin_dma_plane plane)
{
	return du_rd_overlay_src(channel, plane);
}

int odin_du_check_overlay_src_using(enum odin_dma_plane plane)
{
	u8 ret;
	int i;

	/* check plane src for all channel */
	for (i = 0; i < ODIN_DSS_CHANNEL_MAX; i++)
	{
		ret = odin_du_check_overlay_src_using_in_ch(i, plane);
		if (ret)
		{
			return ret;
		}
	}
	return ret;
}

u32 odin_du_get_ovl_src_go(void)
{
	return du_get_ovl_src_go();
}

static void du_enable_channel(enum odin_channel channel, bool enable)
{
	_du_ovl_en(channel, enable);
}

/*
vid0 -> ovl0 path : 0 (sync0사용)
vid0 -> ovl1 path : 1 (sync1사용)
vid0 -> ovl2 path : 2 (sync2사용)
*/
static void du_set_plane_sync_sel(enum odin_resource_index resource_index,
	enum odin_channel channel)
{
	if ((resource_index == ODIN_DSS_GRA0_RES) ||
		(resource_index == ODIN_DSS_GRA1_RES) ||
	    (resource_index == ODIN_DSS_GRA2_RES))
		_du_gra_rd_src_sync_sel(resource_index, channel);

	else
		_du_vid_rd_src_sync_sel(resource_index, channel);
}

static void du_sync_enable(enum sync_gen_cfg1_2 sync_enable_block,
	bool sync_enable)
{
	if (sync_enable_block < SYNC_GSCL_SYNC_EN1)
		_du_sync_cfg1(sync_enable_block, sync_enable);
	else
		_du_sync_cfg2(sync_enable_block, sync_enable);
}

static void du_set_plane_sync_ena(enum odin_resource_index resource_index,
	bool sync_enable)
{
	switch (resource_index) {
	case ODIN_DSS_VID0_RES:
		du_sync_enable(SYNC_VID0_SYNC_EN0, sync_enable);
		break;
	case ODIN_DSS_VID1_RES:
		du_sync_enable(SYNC_VID1_SYNC_EN0, sync_enable);
		break;
	case ODIN_DSS_VID2_RES:
		du_sync_enable(SYNC_VID2_SYNC_EN, sync_enable);
		break;
	case ODIN_DSS_GRA0_RES:
		du_sync_enable(SYNC_GRA0_SYNC_EN, sync_enable);
		break;
	case ODIN_DSS_GRA1_RES:
		du_sync_enable(SYNC_GRA1_SYNC_EN, sync_enable);
		break;
	case ODIN_DSS_GRA2_RES:
		du_sync_enable(SYNC_GRA2_SYNC_EN, sync_enable);
		break;
	case ODIN_DSS_GSCL_RES:
		du_sync_enable(SYNC_GSCL_SYNC_EN0, sync_enable);
		break;
	default:
		break;
	}
}

static void du_set_plane_wb_sync_ena(enum odin_resource_index resource_index,
	bool sync_enable)
{
	switch (resource_index) {
	case ODIN_DSS_VID0_RES:
		du_sync_enable(SYNC_VID0_SYNC_EN1, sync_enable);
		break;
	case ODIN_DSS_VID1_RES:
		du_sync_enable(SYNC_VID1_SYNC_EN1, sync_enable);
		break;
	case ODIN_DSS_GSCL_RES:
		du_sync_enable(SYNC_GSCL_SYNC_EN1, sync_enable);
		break;
	default:
		break;
	}
}

static void du_set_ovl_sync_ena(enum odin_channel channel, bool sync_enable)
{
	switch (channel) {
	case ODIN_DSS_CHANNEL_LCD0:
		du_sync_enable(SYNC_OVL0_SYNC_EN, sync_enable);
		break;
	case ODIN_DSS_CHANNEL_LCD1:
		du_sync_enable(SYNC_OVL1_SYNC_EN, sync_enable);
		break;
	case ODIN_DSS_CHANNEL_HDMI:
		du_sync_enable(SYNC_OVL2_SYNC_EN, sync_enable);
		break;
	default:
		break;
	}
}

static void du_set_sync_size(enum odin_channel channel,
	u32 hsync_total, u32 vsync_total)
{
	switch (channel) {
		case ODIN_DSS_CHANNEL_LCD0:
			_du_sync0_raster_size(hsync_total, vsync_total);
			break;
		case ODIN_DSS_CHANNEL_LCD1:
			_du_sync1_raster_size(hsync_total, vsync_total);
			break;
		case ODIN_DSS_CHANNEL_HDMI:
			_du_sync2_raster_size(hsync_total, vsync_total);
			break;
		default:
			DSSERR("du_set_sync_size : wrong channel\n");
			break;
	}
}

static void du_set_trans_num(enum odin_resource_index resource_index,
	u8 src_trans_num)
{
	if ((resource_index == ODIN_DSS_GRA0_RES) ||
		(resource_index == ODIN_DSS_GRA1_RES) ||
	    (resource_index == ODIN_DSS_GRA2_RES))
		_du_gra_rd_src_trans_num(resource_index, src_trans_num);

	else
		_du_vid_rd_src_trans_num(resource_index, src_trans_num);

}

static void du_set_plane_src_buf_sel(enum odin_resource_index resource_index,
	u8 src_buf_sel)
{
	if ((resource_index == ODIN_DSS_GRA0_RES) ||
		(resource_index == ODIN_DSS_GRA1_RES) ||
	    (resource_index == ODIN_DSS_GRA2_RES))
		_du_gra_rd_src_buf_sel(resource_index, src_buf_sel);

	else
		_du_vid_rd_src_buf_sel(resource_index, src_buf_sel);
}

static void du_set_plane_rd_src_en(enum odin_resource_index resource_index,
	u8 src_en)
{
	if ((resource_index == ODIN_DSS_GRA0_RES) ||
		(resource_index == ODIN_DSS_GRA1_RES) ||
	    (resource_index == ODIN_DSS_GRA2_RES))
	   	_du_gra_rd_cfg0(resource_index, GRA_RD_SRC_EN, src_en);
	else
		_du_vid_rd_cfg0(resource_index, VID_RD_SRC_EN, src_en);
}


void du_set_ovl_cop(enum odin_channel channel, u8 layer_number, u8 cop)
{
	_du_ovl_layer_cop(channel, layer_number, cop);
}

void du_set_ovl_cop_or_rop(enum odin_channel channel, bool ovl_op)
{
	_du_ovl_layer_op(channel, ovl_op);
}

void du_ovl_src_sel(enum odin_channel channel,
	u8 layer_number, u8 ovl_src_sel)
{
	_du_ovl_src_sel(channel, layer_number, ovl_src_sel);
}

void du_set_pre_mult_alpha_default(enum odin_channel channel)
{
	_du_ovl_src_pre_mul_en_total(channel, 0xAAA00);
}

void du_set_channel_sync_int_src_sel(u8 sync_numeber, u8 int_src_sel)
{
	_du_set_channel_sync_int_src_sel(sync_numeber ,int_src_sel);
}

void du_set_channel_sync_mode(u8 sync_numeber, u8 is_continuos)
{
	_du_set_channel_sync_mode(sync_numeber, is_continuos);
}

/* WB */
static void du_set_plane_wb_sync_sel(enum odin_resource_index resource_index,
		enum odin_channel channel)
{
	if (resource_index == ODIN_DSS_GSCL_RES)
	{
		_du_vid_wb_sync_sel(resource_index, channel);
		/* rd, write sync is same */
		du_set_plane_sync_sel(resource_index ,channel);
	}
	else
		_du_vid_wb_sync_sel(resource_index, channel);
}

static void du_set_plane_wb_buf_sel(enum odin_resource_index resource_index,
	u8 wb_buf_sel)
{
	_du_vid_wb_buf_sel(resource_index, wb_buf_sel);
}

static void du_set_plane_wb_y_rgb_addr(enum odin_resource_index resource_index,
	u8 fb_num, u32 paddr)
{
	_du_vid_wb_dss_base_y_addr(resource_index, fb_num, paddr);
}

static void du_set_plane_wb_u_uv_addr(enum odin_resource_index resource_index,
	u8 fb_num, u32 paddr)
{
	_du_vid_wb_dss_base_u_addr(resource_index, fb_num, paddr);
}

static void du_set_plane_wb_width_height
	(enum odin_resource_index resource_index, u16 width, u16 height)
{
	_du_vid_wb_width_hegiht(resource_index, width, height);
}

static void du_set_plane_wb_pos(enum odin_resource_index resource_index,
	u16 pos_x, u16 pos_y)
{
	_du_vid_wb_startxy(resource_index, pos_x, pos_y);
}

static void du_set_plane_wb_size(enum odin_resource_index resource_index,
	u16 size_x, u16 size_y)
{
	_du_vid_wb_sizexy(resource_index, size_x, size_y);
}

static u8 du_get_wb_color_mode(enum odin_color_mode colormode)
{
	if (colormode >= ODIN_DSS_COLOR_RGB565 &&
		colormode <= ODIN_DSS_COLOR_RGB888)
		return colormode;
	else if (colormode == ODIN_DSS_COLOR_YUV422S)
		return VID_WB_YUV422S;
	else if (colormode == ODIN_DSS_COLOR_YUV420P_2)
		return VID_WB_YUV420P_2;
	else
		return VID_WB_NOT_SUPPORTED;
}

static void du_set_plane_wb_color_mode(enum odin_resource_index resource_index,
	enum odin_color_mode color_mode)
{
	u8 colorformat, rgbswap=0,  alphaswap=0,
	   yuvswap=0, 	wordswap=0, byteswap=0;
	u8 wb_colorformat;

	colorformat = CHECK_COLOR_VALUE(color_mode,ODIN_COLOR_MODE);
	wb_colorformat = du_get_wb_color_mode(colorformat);

	if (colorformat <= ODIN_DSS_COLOR_RGB888)
	{
		byteswap = CHECK_COLOR_VALUE(color_mode,ODIN_COLOR_BYTE_SWAP);
		wordswap = CHECK_COLOR_VALUE(color_mode,ODIN_COLOR_WORD_SWAP);
		rgbswap = CHECK_COLOR_VALUE(color_mode,ODIN_COLOR_RGB_SWAP);
		alphaswap = CHECK_COLOR_VALUE(color_mode,ODIN_COLOR_ALPHA_SWAP);

		_du_vid_wb_format(resource_index, wb_colorformat);
		_du_vid_wb_alpha_swap(resource_index, alphaswap);
		_du_vid_wb_byte_swap(resource_index, byteswap);
		_du_vid_wb_word_swap(resource_index, wordswap);
		_du_vid_wb_rgb_swap(resource_index, rgbswap);

	}
	else if ((colorformat == ODIN_DSS_COLOR_YUV422S) ||
		(colorformat == ODIN_DSS_COLOR_YUV420P_2))
	{

		byteswap = CHECK_COLOR_VALUE(color_mode,ODIN_COLOR_BYTE_SWAP);
		wordswap = CHECK_COLOR_VALUE(color_mode,ODIN_COLOR_WORD_SWAP);
		yuvswap = CHECK_COLOR_VALUE(color_mode,ODIN_COLOR_YUV_SWAP);
		_du_vid_wb_format(resource_index, wb_colorformat);
		_du_vid_wb_byte_swap(resource_index, byteswap);
		_du_vid_wb_word_swap(resource_index, wordswap);
		_du_vid_wb_yuv_swap(resource_index, yuvswap);
	}
	else
	{
		DSSDBG("unsupported pixel format(%x) in wb\n", color_mode);
		return;
	}

}

#if 0
static void du_set_wb_alpha_blend(enum odin_resource_index resource_index,
	u8 global_alpha)
{
	if ((global_alpha == 0) || (global_alpha == 255)) /* Pixel Alpha */
	{
		_du_vid_wb_alpha_reg_en(resource_index, 0);
		_du_vid_wb_alpah_value(resource_index, 0);
	}
	else /* global alpha */
	{
		_du_vid_wb_alpha_reg_en(resource_index, 1);
		_du_vid_wb_alpah_value(resource_index, global_alpha);
	}
}
#endif

static void du_set_wb_src_sel(enum odin_resource_index resource_index,
		enum odin_channel channel)
{
	if (resource_index == ODIN_DSS_GSCL_RES)
		_du_vid_wb_channel_src_sel(resource_index, channel);
}

static void du_set_wb_set_dst(enum odin_channel channel,
	enum odin_resource_index resource_index, enum odin_writeback_mode mode,
	bool enable)
{
	if (!enable)
	{
		switch (resource_index) {
			case ODIN_DSS_VID0_RES:
			case ODIN_DSS_VID1_RES:
				_du_vid_rd_src_dst_sel(resource_index, 0); /* 0:OVL, 1:WB */
				_du_vid_rd_src_dual_op_en(resource_index, 0);
				break;
			case ODIN_DSS_GSCL_RES:
				_du_ovl_dual_op_en(channel, 0);
				_du_ovl_dst_sel(channel, 0); /* 0:OVL, 1:WB */
				break;
			default:
				DSSDBG("wrong resource_index = %d \n", resource_index);
				break;
		}
		return;
	}

	switch (resource_index) {
		case ODIN_DSS_VID0_RES:
		case ODIN_DSS_VID1_RES:
			if (mode == ODIN_WB_CAPTURE_MODE)
				_du_vid_rd_src_dual_op_en(resource_index, 1);
			else
			{
				_du_vid_rd_src_dst_sel(resource_index, 1); /* 0:OVL, 1:WB */
				_du_vid_rd_src_dual_op_en(resource_index, 0);
			}
			break;
		case ODIN_DSS_GSCL_RES:
			if (mode == ODIN_WB_CAPTURE_MODE)
				_du_ovl_dual_op_en(channel, 1);
			else
			{
				_du_ovl_dst_sel(channel, 1);  /* 0:OVL, 1:WB */
				_du_ovl_dual_op_en(channel, 0);
			}
			break;
		default:
			DSSDBG("wrong resource_index = %d \n", resource_index);
			break;
	}
}

int delay_time = 10;
static void du_set_wb_sync_set(enum odin_channel channel,
	 enum odin_resource_index resource_index, bool enable)
{
	_du_vid_wb_reg_sw_update(resource_index, 1);

#if 0
	_du_set_channel_sync_int_ena(sync_src, enable);
	du_set_channel_sync_mode(channel, !enable); /* continuos or snapshot */

	du_set_channel_sync_int_src_sel(sync_src, 1);
#endif

	if (enable)
		_du_set_channel_sync_disp_sync_src_sel(channel, 0);
	du_set_sync_size(channel, DU_MANUAL_MODE_RASTER_X_SIZE,
			DU_MANUAL_MODE_RASTER_Y_SIZE);
			/* out width size가 커야 out_x가 가능 */

	_du_set_channel_sync_mode(channel, 0);

	if (enable)
	{
		_du_set_channel_sync_enable(channel, 0);
		udelay(100);
		_du_set_channel_sync_enable(channel, enable);
	}
	else
	{
		_du_set_channel_sync_enable(channel, enable);
	}
}


/****************************************************************************/

/****************************************************************************/
u32 odin_du_get_ovl_src_size(int zorder)
{
	return du_get_ovl_src_size(zorder);
}

u32 odin_du_get_ovl_src_pos(int zorder)
{
	return du_get_ovl_src_pos(zorder);
}

u32 odin_du_get_rd_staus(enum odin_dma_plane plane)
{
	enum odin_resource_index resource_index;
	u32 val;
	resource_index = du_get_dma_resource_index(plane);
	val = dss_read_reg(resource_index, 0, ADDR_GRA0_RD_STATUS);
	return val;
}

bool odin_du_iova_check(u32 iova_addr)
{
	int i;
	enum odin_resource_index resource_index;
	u32 plane_iova;
	for (i = 0; i< MAX_DSS_OVERLAYS ; i++)
	{
		resource_index = du_get_dma_resource_index(i);
		if (_du_get_plane_rd_en(resource_index))
		{
			plane_iova = _du_get_plane_rd_y_rgb_addr(resource_index);
			if (plane_iova == iova_addr)
				return true;
		}
	}
	return false;
}

/* Only Error routine function */
int odin_set_rd_enable_plane(enum odin_channel channel, bool enable)
{
	int i;
	enum odin_resource_index resource_index;

	for (i = 0; i< MAX_DSS_OVERLAYS ; i++)
		if (du.enabled_plane_mask[channel] & (1<<i))
		{
			resource_index = du_get_dma_resource_index(i);
			du_set_plane_rd_src_en(resource_index, enable);
		}

	return 0;
}

/* Only Error routine function */
int odin_set_rd_clk_gate_plane(enum odin_channel channel, bool enable)
{
	int i;
	u32 clk_mask =0;

	for (i = 0; i< MAX_DSS_OVERLAYS ; i++)
		if (du.enabled_plane_mask[channel] & (1<<i))
			clk_mask |= odin_crg_du_plane_to_clk(i, enable);

	odin_crg_dss_clk_ena(CRG_CORE_CLK, clk_mask, enable);

	return 0;
}

int odin_du_ch_all_disable_plane(enum odin_channel channel)
{
	int i;
	u32 *enabled_plane_mask;

	enabled_plane_mask = &du.enabled_plane_mask[channel];

	for (i = 0; i< MAX_DSS_OVERLAYS ; i++)
	{
		if (*(enabled_plane_mask) & (1<<i))
		{
			odin_du_enable_plane(i, channel, false,
					     DSSCOMP_SETUP_MODE_DISPLAY,
					     true, false);
			*(enabled_plane_mask) &= ~(1 << i);
		}
	}

	return 0;
}

int odin_du_enable_plane(enum odin_dma_plane plane, enum odin_channel channel,
			 bool enable, enum dsscomp_setup_mode mode,
			 bool invisible, bool frame_skip)
{
	bool *enabled_status;
	u32 *enabled_plane_mask;
	enum odin_resource_index resource_index;
	u8 layer_number;
	u32 du_addr;

	enabled_status = &du.overlay_enabled_status[plane];
	resource_index = du_get_dma_resource_index(plane);
	layer_number = du.overlay_src_num[plane];
	enabled_plane_mask = &du.enabled_plane_mask[channel];

	if ((enable == 0) && (enabled_status == 0))
	{
		DSSERR("odin_du_enable_plane error \n");
		BUG_ON(1);
		return -EINVAL;
	}

	if (mode & DSSCOMP_SETUP_MODE_MEM2MEM)
		goto skip_ovl_src_ena;

	if (((mode & DSSCOMP_SETUP_MODE_DISPLAY_MEM2MEM) &&
	     (invisible)) && (enable))
		goto skip_ovl_src_ena;

	du_enable_overlay_src(channel, plane, enable);

	if (resource_index < ODIN_DSS_VID2_RES)
		_du_vid_rd_src_dst_sel(resource_index, 0); /* 0:OVL, 1:WB */

skip_ovl_src_ena:
	if ((!frame_skip) || (!enable))
		du_set_plane_rd_src_en(resource_index, enable);
	du_addr = _du_get_plane_rd_y_rgb_addr(resource_index);

	*(enabled_status) = enable;
	if (enable)
		*(enabled_plane_mask) |= (1 << plane);
	else
		*(enabled_plane_mask) &= ~(1 << plane);

	DSSDBG("du_enable_plane plane:%d, layer:%d, enable:%d rd_addr:%x \n",
		plane, layer_number, enable, du_addr);

	return 0;
}

int odin_du_enable_wb_plane(enum odin_channel channel,
	enum odin_dma_plane wb_plane, enum odin_writeback_mode mode,
	bool enable)
{
	enum odin_resource_index resource_index;
	resource_index = du_get_dma_resource_index(wb_plane);

	DSSDBG("du_enable_wb_plane plane:%d, enable:%d\n", wb_plane, enable);

	du_set_wb_set_dst(channel, resource_index, mode, enable);
	_du_vid_wb_en(resource_index, enable);
	du_set_plane_wb_sync_ena(resource_index, enable);

	if (mode == ODIN_WB_MEM2MEM_MODE)
		du_set_wb_sync_set(channel, resource_index, enable);

	return 0;
}

void odin_du_set_ovl_cop_or_rop(enum odin_channel channel, bool ovl_op)
{
	du_set_ovl_cop_or_rop(channel, ovl_op);
}

bool odin_du_go_busy(enum odin_channel channel)
{
	/* TODO: */
	return 0;
}

void odin_du_go(enum odin_channel channel)
{
	/* TODO: */
}

void odin_du_set_channel_out(enum odin_dma_plane plane,
		enum odin_channel channel)
{
	/*  channel enable. */
	du_enable_channel(channel, 1);

	/* overlay enable. */
	/* odin_du_enable_plane(plane, channel, 1); */
}

int odin_du_setup_plane(enum odin_dma_plane plane,
		u16 width, u16 height,
		u16 crop_x, u16 crop_y,
		u16 crop_w, u16 crop_h,
		u16 out_x, u16 out_y,
		u16 out_w, u16 out_h,
		enum odin_color_mode color_mode,
		u8 rotation, bool mirror,
		enum odin_channel channel, u32 p_y_addr,
		u32 p_u_addr, u32 p_v_addr,
		enum odin_overlay_zorder zorder,
		u8 blending,
		u8 global_alpha,
		u8 pre_mult_alpha,
		u8 mgr_flip,
		bool mxd_use,
		bool invisible,
		u8 mode
)
{
 	u32 fb_num = 0;
	enum odin_resource_index resource_index;

	resource_index = du_get_dma_resource_index(plane);
	du_set_plane_sync_sel(resource_index ,channel);
	du_set_trans_num(resource_index , 2);
	du_set_plane_src_buf_sel(resource_index , fb_num);
	du_set_plane_rd_y_rgb_addr(resource_index, fb_num, p_y_addr);
	du_set_plane_rd_u_addr(resource_index, fb_num, p_u_addr);
	du_set_plane_rd_v_addr(resource_index, fb_num, p_v_addr);

	du_set_plane_rd_width_height(resource_index, width, height);
	du_set_plane_rd_pos(resource_index, crop_x, crop_y);
	du_set_plane_rd_size(resource_index, crop_w, crop_h);
	du_set_plane_pip_rd_size(resource_index, out_w, out_h);
	du_set_vid_scaling(resource_index,
		crop_w, crop_h, out_w, out_h, color_mode); /* video scaler 1 or 2 */

	du_set_plane_color_mode(channel, resource_index, color_mode);

	if (mode & DSSCOMP_SETUP_MODE_MEM2MEM)
		goto skip_mixer_setting;

	if ((mode & DSSCOMP_SETUP_MODE_DISPLAY_MEM2MEM) && (invisible))
	{
		du_rd_src_flip_mode(resource_index , 0);
		goto skip_mixer_setting;
	}

	du_set_zorder(channel, plane, zorder);
	if (zorder > 0)
		du_set_ovl_cop(channel, zorder - 1 , 0x4); /* dst_over */
	du_set_ovl_ckey_mask(channel, zorder, color_mode);

 	du_set_pre_mult_alpha(channel, zorder, pre_mult_alpha);

   	du_set_alpha_blend(channel,
		zorder, color_mode, blending, global_alpha, mxd_use);

	du_set_ovl_src_size_pos(zorder, out_w, out_h, out_x, out_y);

	du_rd_src_flip_mode(resource_index ,mgr_flip);

skip_mixer_setting:

	du_set_src_lsb_sel(resource_index ,1);

	/* du_set_plane_sync_start_dly(resource_index ,out_x+1, out_y+1); */

	return 0;
}

int odin_du_setup_wb_plane(enum odin_dma_plane wb_plane,
		u16 width, u16 height,
		u16 out_x, u16 out_y,
		u16 out_w, u16 out_h,
		enum odin_color_mode color_mode,
		u32 p_y_addr, u32 p_uv_addr,
		enum odin_channel sync_src,
		enum odin_writeback_mode mode
)
{
	u32 fb_num = 0;
	enum odin_resource_index resource_index;
	resource_index = du_get_dma_resource_index(wb_plane);

	du_set_plane_wb_sync_sel(resource_index , sync_src);
	du_set_plane_wb_buf_sel(resource_index , fb_num);
	du_set_plane_wb_y_rgb_addr(resource_index, fb_num, p_y_addr);
	du_set_plane_wb_u_uv_addr(resource_index, fb_num, p_uv_addr);

	du_set_plane_wb_width_height(resource_index, width, height);
	du_set_plane_wb_pos(resource_index, out_x, out_y);
	du_set_plane_wb_size(resource_index, out_w, out_h);

	du_set_plane_wb_color_mode(resource_index, color_mode);

	du_set_wb_src_sel(resource_index, sync_src);

	/* du_set_plane_sync_start_dly(resource_index ,out_x+1, out_y+1); */

	return 0;
}

void odin_du_set_ovl_width_height(enum odin_channel channel,
	u16 size_x, u16 size_y)
{
	_du_ovl_sizexy(channel, size_x, size_y);
}

void odin_du_set_ovl_cops(enum odin_channel channel, u32 ovl_cops)
{
	_du_ovl_layer_cop_total(channel, ovl_cops);
}

void odin_du_set_ovl_src_sel(enum odin_channel channel, u32 ovl_sel)
{
	_du_ovl_src_sel_total(channel, ovl_sel);
}

u32 odin_du_get_ovl_src_sel(enum odin_channel channel)
{
	return _du_ovl_get_src_sel_total(channel);
}

void odin_du_set_dst_select(enum odin_channel channel, u8 ovl_dst_sel)
{
	_du_ovl_dst_sel(channel, ovl_dst_sel);
}

void odin_du_set_ovl_mixer_default(enum odin_channel channel)
{
	_du_ovl_layer_cop_total(channel, 0x33333300); /* defult is src_over */
	_du_ovl_src_sel_total(channel, 0x001fffff); /* defualt is 0x111 */
}

void odin_du_set_ovl_sync_enable(enum odin_channel channel, bool sync_enable)
{
	du_set_ovl_sync_ena(channel, sync_enable);
}

void odin_du_set_channel_sync_enable(u8 sync_numeber, u8 enable)
{
	_du_set_channel_sync_enable(sync_numeber, enable);
}

void odin_du_set_channel_sync_int_ena(u8 sync_numeber, u8 int_en)
{
	_du_set_channel_sync_int_ena(sync_numeber ,int_en);
}

void odin_du_set_parallel_lcd_test_mode(u8 parallel_enable)
{
	/* mailbox 사용 */
}

void odin_du_enable_trans_key(enum odin_channel channel, bool enable)
{
	int i;
 	for (i = 0; i < 5; i++)
	_du_ovl_chromakey_en(channel, i, enable);

}

void odin_du_set_trans_key(enum odin_channel channel, u32 trans_key)
{
	int i;
 	for (i = 0; i < 5; i++)
	_du_ovl_layer_ckey_value(channel,
		i, (trans_key & 0xFF0000)>>16,
		(trans_key & 0xFF00)>>8, (trans_key & 0xFF));
}

void du_mxd_size_handler(void *data, u32 mask, u32 sub_mask)
{
	u8 *mxd_plane_mask = data;
	enum odin_dma_plane plane = ODIN_DSS_NONE;
	u32 val, input_width, input_height, output_width, output_height;
	enum odin_resource_index resource_index;

	if (*(mxd_plane_mask) & (1 << ODIN_DSS_VID0))
		plane = ODIN_DSS_VID0;
	else if (*(mxd_plane_mask) & (1 << ODIN_DSS_VID1))
		plane = ODIN_DSS_VID1;
	else
		goto err;

	resource_index = du_get_dma_resource_index(plane);

	du.mxd_plane_mask |= (1 << plane);
	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_SIZEXY);
	input_width = DSS_REG_GET(val,  SRC_SIZEX);
	input_height = DSS_REG_GET(val, SRC_SIZEY);

	val = dss_read_reg(resource_index, 0, ADDR_VID0_RD_PIP_SIZEXY);
	output_width = DSS_REG_GET(val, SRC_PIP_SIZEX);
	output_height = DSS_REG_GET(val, SRC_PIP_SIZEY);

	odin_mxd_set_image_size(input_width, input_height,
		output_width, output_height);

	du.mxd_plane_mask &= ~(1 << plane);
err:
	odin_crg_unregister_isr(du_mxd_size_handler, data, mask, sub_mask);
}

void odin_du_mxd_size_irq_set(enum odin_channel channel)
{
	if ((channel == ODIN_DSS_CHANNEL_LCD0) && (du.mxd_plane_mask))
		odin_crg_register_isr(du_mxd_size_handler, &du.mxd_plane_mask,
			CRG_IRQ_MIPI0, CRG_IRQ_MIPI_FRAME_COMPLETE);
}

void odin_du_set_mxd_mux_switch_control(enum odin_channel channel,
	enum odin_dma_plane plane, bool mXD_use, int scenario,
	int crop_w, int crop_h, int out_w, int out_h)
{
	u32 val;
	if ((plane != ODIN_DSS_VID0) && (channel != ODIN_DSS_CHANNEL_LCD0))
		return;

	if (mXD_use)
	{
		odin_mxd_set_image_size(crop_w, crop_h,
			out_w, out_h);
		du.mxd_plane_mask &= ~(1 << plane);
	}
	else
		du.mxd_plane_mask |= (1 << plane);

	du_set_mxd_enable(plane, mXD_use, scenario);
}

void odin_du_set_formatter_mux_switch_control(enum odin_dma_plane plane,
	bool formatter_use)
{
	enum odin_resource_index resource_index;

 	resource_index = du_get_dma_resource_index(plane);
	switch (resource_index) {

	case ODIN_DSS_VID0:

	case ODIN_DSS_VID1:
	break;

 	case ODIN_DSS_VID2_FMT:
 		if (formatter_use)
		{
			du_set_formater_enable(plane, 1);
		}
		else
		{
			du_set_formater_enable(plane, 0);
		}
	break;

 	default:
		break;
	}
}

int odin_du_set_sync_size(struct odin_dss_device *dss_dev, bool manual_mode)
{
	int ret = 0;
	struct odin_video_timings *ti = &dss_dev->panel.timings;
	u32 hsync_total, vsync_total;

	if (manual_mode)
	{
		hsync_total = DU_MANUAL_MODE_RASTER_X_SIZE;
		vsync_total = DU_MANUAL_MODE_RASTER_Y_SIZE;
	}
	else if (dss_dev->channel == ODIN_DSS_CHANNEL_HDMI)
	{
#if 1
		hsync_total = ti->mHBlanking + ti->mHActive - 1;
		vsync_total = ti->mVBlanking + ti->mVActive - 1;
#else
		hsync_total = 1649;
		vsync_total = 749;
#endif
	}
	else
	{
		hsync_total = ti->hbp + ti->x_res + ti->hfp;
		vsync_total = ti->vbp + ti->y_res + ti->vfp;
	}

	du_set_sync_size(dss_dev->channel, hsync_total ,vsync_total);

	return ret;
}

static void odin_du_vsync_timer_isr(void *data)
{
	/********mutex lock issue**************/
	struct odin_dss_device	*dssdev = (struct odin_dss_device *)data;
	du.vsync_time = ktime_get();
}

void odin_du_vsync_timer_enable(struct odin_dss_device *dssdev)
{
	int r = 0;
	u32 mask;

	switch (dssdev->channel) {
		case ODIN_DSS_CHANNEL_LCD0:
			mask = CRG_IRQ_VSYNC_MIPI0;
			break;
		case ODIN_DSS_CHANNEL_LCD1:
			mask = CRG_IRQ_VSYNC_MIPI1;
			break;
		case ODIN_DSS_CHANNEL_HDMI:
			mask = CRG_IRQ_VSYNC_HDMI;
			break;
		default:
			mask = MAIN_MAX_POS;
	}

		r = odin_crg_register_isr(odin_du_vsync_timer_isr,
			dssdev, mask, (u32)NULL);
}

void odin_du_vsync_timer_disable(struct odin_dss_device *dssdev)
{
	int r = 0;
	u32 mask;

	switch (dssdev->channel) {
		case ODIN_DSS_CHANNEL_LCD0:
			mask = CRG_IRQ_VSYNC_MIPI0;
			break;
		case ODIN_DSS_CHANNEL_LCD1:
			mask = CRG_IRQ_VSYNC_MIPI1;
			break;
		case ODIN_DSS_CHANNEL_HDMI:
			mask = CRG_IRQ_VSYNC_HDMI;
			break;
		default:
			mask = MAIN_MAX_POS;
	}

	r = odin_crg_unregister_isr(odin_du_vsync_timer_isr,
		dssdev, mask, (u32)NULL);
}

u32 odin_du_vsync_after_time(void)
{
	ktime_t end_time;
	s64 lead_time_us;
	u32 decimation;
	end_time = ktime_get();

	lead_time_us = ktime_to_us(ktime_sub(end_time, du.vsync_time));
	decimation = lead_time_us;

	return decimation;
}

int odin_du_power_on(struct odin_dss_device *dss_dev)
{
	enum odin_channel channel = dss_dev->channel;
	enum odin_dma_plane plane;
	u8 zorder;
	u16 size_x, size_y;
	u32 clk_mask;
	bool manual_mode = false;

	if (dssdev_manually_updated(dss_dev))
			manual_mode = true;

	dss_dev->driver->get_resolution(dss_dev, &size_x, &size_y);

	odin_du_set_ovl_width_height(channel, size_x, size_y);
    	odin_set_cabc_size(channel, size_x, size_y);
	odin_du_set_sync_size(dss_dev, manual_mode);

	if (channel == ODIN_DSS_CHANNEL_LCD0)
		du_set_ovl_cop_or_rop(channel, ODIN_DSS_OVL_COP); /* DATA Enable */
	else if (channel == ODIN_DSS_CHANNEL_HDMI)
		du_set_ovl_cop_or_rop(channel, ODIN_DSS_OVL_ROP);

	if (channel == ODIN_DSS_CHANNEL_LCD0)
	{
		plane = PRIMARY_DEFAULT_OVERLAY;
		zorder = 0;
	}
	else
	{
		plane = EXTERNAL_DEFAULT_OVERLAY;
		zorder = 4;
	}

	clk_mask = (1 << plane);

	dss_dev->manager->clk_enable(clk_mask, 0, false);

	if (dss_dev->type == ODIN_DISPLAY_TYPE_DSI) /* For Fifo Flush */
	{
		du_set_ovl_src_size_pos(zorder, size_x, size_y, 0, 0);
		du_set_zorder(channel, plane, zorder);
		du_enable_overlay_src(channel, plane, 1);
	}

	if (manual_mode)
	{
		odin_du_set_channel_sync_int_ena(channel , 0);
		du_set_channel_sync_mode(channel , 0); /*SNAPSHOT */
		_du_set_channel_sync_disp_sync_src_sel(channel, 0);
		odin_du_set_channel_sync_enable(channel, 0);
	}
	else
	{
		odin_du_set_channel_sync_int_ena(channel , 1);
		du_set_channel_sync_mode(channel , 1); /*CONTINUOS */
		_du_set_channel_sync_disp_sync_src_sel(channel, 1);
		odin_du_set_channel_sync_enable(channel , 1);
	}

	/* channel enable. */
	du_enable_channel(channel, true);
	if(channel != ODIN_DSS_CHANNEL_HDMI)
		odin_du_set_ovl_sync_enable(channel , true);


	dss_dev->manager->mgr_flip =
		(~(dss_dev->panel.flip_supported) & (dss_dev->panel.flip));
	dss_dev->manager->frame_skip = false;
	odin_crg_set_err_mask(dss_dev, true);
	if (channel == ODIN_DSS_CHANNEL_HDMI)
		odin_du_vsync_timer_enable(dss_dev);

	return 0;
}
EXPORT_SYMBOL(odin_du_power_on);

int odin_du_power_off(struct odin_dss_device *dss_dev)
{
	int ret = 0;
	enum odin_channel channel = dss_dev->channel;

	odin_set_rd_enable_plane(dss_dev->channel, 0);

	/* channel enable. */
	du_enable_channel(channel, 0);

	odin_du_set_channel_sync_int_ena(channel , 0);
	odin_du_set_ovl_sync_enable(channel , 0);
	odin_du_set_channel_sync_enable(channel , 0);

	dss_dev->manager->mgr_flip = 0;
	odin_crg_set_err_mask(dss_dev, false);
	if (channel == ODIN_DSS_CHANNEL_HDMI)
		odin_du_vsync_timer_disable(dss_dev);

	return ret;
}
EXPORT_SYMBOL(odin_du_power_off);


void odin_du_enable_channel(enum odin_channel channel,
		enum odin_display_type type, bool enable)
{
	/* TODO: */
}

int odin_du_set_sync_start_pos(void)
{
	/*Channel Setting*/
	_du_sync_ovl0_st_dly(1, 1);
	_du_sync_ovl1_st_dly(1, 1);
	_du_sync_ovl2_st_dly(1, 1);

	/*Plane Setting*/
	_du_sync_vid0_st_dly0(1, 1);
	_du_sync_vid1_st_dly0(1, 1);
	_du_sync_vid2_st_dly(1, 1);
	_du_sync_gra0_st_dly(1, 1);
	_du_sync_gra1_st_dly(1, 1);
	_du_sync_gra2_st_dly(1, 1);
	_du_sync_gscl_st_dly0(1, 1);
	_du_sync_vid0_st_dly1(1, 1);
	_du_sync_vid1_st_dly1(1, 1);
	_du_sync_gscl_st_dly1(1, 1);

	return 0;
}

int odin_du_set_sync_wb_disable(void)
{
#if 0
	enum odin_resource_index resource_index;

	resource_index = du_get_dma_resource_index(ODIN_DSS_VID0);
	_du_vid_wb_en(resource_index, 0);
	resource_index = du_get_dma_resource_index(ODIN_DSS_VID1);
	_du_vid_wb_en(resource_index, 0);
	resource_index = du_get_dma_resource_index(ODIN_DSS_mSCALER);
	_du_vid_wb_en(resource_index, 0);
#endif

	du_sync_enable(SYNC_GSCL_SYNC_EN1, 0);
	du_sync_enable(SYNC_VID1_SYNC_EN1, 0);
	du_sync_enable(SYNC_VID0_SYNC_EN1, 0);

	return 0;
}

int odin_du_query_sync_wb_done(enum odin_dma_plane wb_plane)
{
	enum odin_resource_index resource_index;
	unsigned int rd_status, wb_status;

	resource_index = du_get_dma_resource_index(wb_plane);
	wb_status = dss_read_reg(resource_index, 0, ADDR_VID0_WB_STATUS);
	rd_status = dss_read_reg(resource_index, 0, ADDR_VID0_RD_STATUS);
	if (rd_status == 0xf7f7030) /* default value */
	{
		return 1;
	}
	else
		return 0;
}

int odin_du_set_zorder_config(enum odin_channel channel,
	enum odin_dma_plane plane) /* pre-zorder value restore. */
{
	int zorder;
	zorder = du.overlay_src_num[plane];
	du_set_zorder(channel, plane, zorder);
	if (zorder > 0)
		du_set_ovl_cop(channel, zorder - 1 , 0x4); /* dst_over */

	return 0;
}


int odin_du_display_init(struct odin_dss_device *dss_dev)
{
	int i;
	enum odin_channel channel = dss_dev->channel;
	/* enum odin_display_type display_type = dss_dev->type; */

#if 0 /* 이후 mailbox로 변경 */
	if (display_type & ODIN_DISPLAY_TYPE_RGB)
		odin_du_set_parallel_lcd_test_mode(1);
#endif

	odin_du_set_dst_select(channel, 0); /* 0: Display 1:Write Back */
	/* Dual op 는 나중에.. */

	/*_du_ovl_layer_rop(channel, 0xc); */ /* Pattern Data */

	for (i = 0; i < MAX_DSS_OVERLAYS; i++)
		du_ovl_src_sel(channel, i, i);

	for (i = 0; i < MAX_DSS_OVERLAY_COP; i++)
	{
		du_set_ovl_cop(channel, i, 0x3); /* 0x3 : src_over */
		du_enable_overlay_src(channel, i, 0);
	}

#ifdef ODIN_FPGA_TESTBED
	du_set_pre_mult_alpha_default(channel);
#endif

	if (channel == ODIN_DSS_CHANNEL_HDMI)
	{
		du_set_channel_sync_int_src_sel(channel, 0); /* 0: Vsync 1:Vactive end */
		/* _du_sync_set_act_size(channel, 50, 11); */
	}
	else
	{
		du_set_channel_sync_int_src_sel(channel, 1); /* 0: Vsync 1:Vactive end */
		_du_sync_set_act_size(channel, 21, 11);
	}

	for (i = 0; i < MAX_DSS_OVERLAYS; i++)
		du_set_plane_sync_start_dly(i ,1, 1);

	return 0;
}
EXPORT_SYMBOL(odin_du_display_init);

void odin_du_sync_set_act_size(int ch, int vact, int hact)
{
	_du_sync_set_act_size(ch, vact, hact);
}

int odin_du_plane_init(void)
{
	int i = 0;
	odin_du_set_sync_start_pos();
	odin_du_set_sync_wb_disable();

	for (i = 0; i < MAX_DSS_OVERLAYS; i++)
		du_set_plane_sync_ena(i, true);

	du_set_cabc_hist_lut_operation_mode(ODIN_DSS_CABC_RES, 1);

	return 0;
}

/* DEBUGFS */
#if defined(CONFIG_DEBUG_FS) && defined(CONFIG_ODIN_DSS_DEBUG_SUPPORT)
void dss_debug_dump_clocks(struct seq_file *s)
{

}
#endif

#ifdef CONFIG_OF
extern struct device odin_device_parent;

static const struct of_device_id odin_duhw_match[] = {
	{
		.compatible	= "odindss-du",
	},
	{},
};
#endif
/* DSS HW IP initialisation */
static int odin_duhw_probe(struct platform_device *pdev)
{
	int r = 0;
	int i;

	DSSINFO("odin_duhw_probe\n");

	du.pdev = pdev;

#ifdef CONFIG_OF
	pdev->dev.parent = &odin_device_parent;

	for (i = 0; i < ODIN_DSS_MAX_RES; i++) {
		du.base[i] = of_iomap(pdev->dev.of_node, i);
		if (!du.base[i]) {
			DSSERR("can't ioremap DU\n");
			r = -ENOMEM;
			goto err_ioremap;
		}
	}
#else
	for (i = 0; i < ODIN_DSS_MAX_RES; i++) {
		du_mem = platform_get_resource(du.pdev, IORESOURCE_MEM, i);
		if (!du_mem) {
			DSSERR("can't get IORESOURCE_MEM DU\n");
			r = -EINVAL;
			goto err_ioremap;
		}
		du.base[i] = ioremap(du_mem->start, resource_size(du_mem));
		if (!du.base[i]) {
			DSSERR("can't ioremap DU\n");
			r = -ENOMEM;
			goto err_ioremap;
		}
	}
#endif
	for (i = 0; i < MAX_DSS_MANAGERS; i++) {
		du.channel_offset[i] = i*0x100;
		du.enabled_plane_mask[i] = 0;
	}

	du.mxd_plane_mask = 0;
	spin_lock_init(&du.sync_lock);

	odin_du_plane_init();

	err_ioremap:
	return r;
}

static int odin_duhw_remove(struct platform_device *pdev)
{
	int i;
	DSSINFO("odin_duhw_remove\n");

	for (i = 0; i < ODIN_DSS_MAX_RES; i++)
		iounmap(du.base[i]);
	return 0;
}
#if 0
static int duhw_runtime_suspend(struct device *dev)
{
	return 0;
}

static int duhw_runtime_resume(struct device *dev)
{
	return 0;
}


static const struct dev_pm_ops du_pm_ops = {
	.runtime_suspend = duhw_runtime_suspend,
	.runtime_resume = duhw_runtime_resume,
};
#endif

static int odin_duhw_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int odin_duhw_resume(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "resume\n");
#if 0
	odin_du_plane_init();
#endif
	return 0;
}
static struct platform_driver odin_duhw_driver = {
	.probe          = odin_duhw_probe,
	.remove         = odin_duhw_remove,
	.suspend	= odin_duhw_suspend,
	.resume		= odin_duhw_resume,
	.driver         = {
		.name   = "odindss-du",
		.owner  = THIS_MODULE,
	/*	.pm	= &du_pm_ops,	*/
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(odin_duhw_match),
#endif
	},
};

int odin_du_init_platform_driver(void)
{
	return platform_driver_register(&odin_duhw_driver);
}

void odin_du_uninit_platform_driver(void)
{
	return platform_driver_unregister(&odin_duhw_driver);
}

