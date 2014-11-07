
#ifndef __ODIN_ODINDSS_H__
#define __ODIN_ODINDSS_H__

#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/fb.h>
#include "linux/odinfb.h"
#include <linux/ion.h>

/* #define DSS_PD_CRG_DUMP_LOG */

#define MAX_OVERLAYS      7
#define MAX_WRITEBACKS    3
#define MAX_MANAGERS      3
#define MAX_DISPLAYS      3
#define MAX_SCALERS	      2

#define NUM_DIPC_BLOCK	3
#define NUM_MIPI_BLOCK	2

#define ODIN_PRIMARY_LCD	ODIN_DSS_CHANNEL_LCD0
#define ODIN_EXTERNAL_LCD	ODIN_DSS_CHANNEL_HDMI
#define ODIN_MEM2MEM_SYNC	ODIN_DSS_CHANNEL_LCD1

#define PRIMARY_DEFAULT_OVERLAY ODIN_DSS_GRA0
#define EXTERNAL_DEFAULT_OVERLAY ODIN_DSS_mSCALER

enum odin_channel {
	ODIN_DSS_CHANNEL_LCD0	= 0,
	ODIN_DSS_CHANNEL_LCD1	= 1,
	ODIN_DSS_CHANNEL_HDMI	= 2,
	ODIN_DSS_CHANNEL_MAX
};

/* This is Odin Plane for DMA */
enum odin_dma_plane {
	ODIN_DSS_VID0			= 0, //Frame Buffer Dedicated
	ODIN_DSS_VID1			= 1,
	ODIN_DSS_VID2_FMT		= 2,
	ODIN_DSS_GRA0			= 3,
	ODIN_DSS_GRA1			= 4,
	ODIN_DSS_GRA2			= 5,
	ODIN_DSS_mSCALER		= 6,
	ODIN_DSS_NONE			= 7,
};

/* interrupt mask num for Main MASK  0x31400804 (0 ~ 15) */
#define MAIN_VSYNC_MIPI0_POS		0
#define MAIN_VSYNC_MIPI1_POS		1
#define MAIN_VSYNC_HDMI_POS			2
#define MAIN_FMT_POS				3
#define MAIN_mXD_POS				4
#define MAIN_GFX_POS				5
#define MAIN_DIP0_POS				6  //Both Error and Framedone is used
#define MAIN_MIPI0_POS				7
#define MAIN_DIP1_POS				8 //Error is only used
#define MAIN_MIPI1_POS				9
#define MAIN_DIP2_POS			    10 //Both Error and Framedone is used
#define MAIN_HDMI_POS				11
#define MAIN_HDMI_WAKEUP_POS		12
#define MAIN_CABC0_POS				13
#define MAIN_CABC1_POS				14
#define MAIN_MAX_POS				15

/* interrupt sub mask num */
/*DIP*/
#define DIP_FIFO0_ACCESS_ERR_POS	 1 // RGB/TVout/MIPI
#define DIP_FIFO0_BECOMES_EMPTY_POS	 2 // FIFO0 Becomes Empty
#define DIP_FIFO1_ACCESS_ERR_POS	 5 // RGB/TVout
#define DIP_FIFO1_BECOMES_EMPTY_POS	 6 // FIFO0 Becomes Empty
#define DIP_COMBINATION_ERR_POS		 8 // COMBINATION ERROR
#define DIP_EOVF_POS				10 // End Of Valid Frame
#define DIP_EOF_POS					14 // End Of Frame

/*MIPI*/
#define MIPI_HS_DATA_COMPLETE_POS		 2
#define MIPI_FRAME_COMPLETE_POS		 3
#define MIPI_TXDATA_FIFO_URERR_POS		 11

/*MXD*/
#define MXD_NR_POS					8
#define MXD_RE_POS					9
#define MXD_CE_POS					10

/*GFX*/
#define GFX0_DONE_POS				0
#define GFX1_DONE_POS				1

/* interrupt main source for CRG  0x31400804 (1<<0 ~ 1<<15) */
#define CRG_IRQ_VSYNC_MIPI0			(1 << MAIN_VSYNC_MIPI0_POS)
#define CRG_IRQ_VSYNC_MIPI1			(1 << MAIN_VSYNC_MIPI1_POS)
#define CRG_IRQ_VSYNC_HDMI			(1 << MAIN_VSYNC_HDMI_POS)
#define CRG_IRQ_FMT					(1 << MAIN_FMT_POS)
#define CRG_IRQ_mXD					(1 << MAIN_mXD_POS)
#define CRG_IRQ_GFX					(1 << MAIN_GFX_POS)
#define CRG_IRQ_DIP0					(1 << MAIN_DIP0_POS)
#define CRG_IRQ_MIPI0				(1 << MAIN_MIPI0_POS)
#define CRG_IRQ_DIP1					(1 << MAIN_DIP1_POS)
#define CRG_IRQ_MIPI1				(1 << MAIN_MIPI1_POS)
#define CRG_IRQ_DIP2					(1 << MAIN_DIP2_POS)
#define CRG_IRQ_HDMI					(1 << MAIN_HDMI_POS)
#define CRG_IRQ_HDMI_WAKEUP			(1 << MAIN_HDMI_WAKEUP_POS)
#define CRG_IRQ_CABC0				(1 << MAIN_CABC0_POS)
#define CRG_IRQ_CABC1				(1 << MAIN_CABC1_POS)

#define CRG_IRQ_MAIN_ALL				(CRG_IRQ_VSYNC_MIPI0 | \
								CRG_IRQ_VSYNC_MIPI1 | \
								CRG_IRQ_VSYNC_HDMI | \
								CRG_IRQ_FMT | \
								CRG_IRQ_mXD | \
								CRG_IRQ_GFX | \
								CRG_IRQ_DIP0 | \
								CRG_IRQ_MIPI0 | \
								CRG_IRQ_DIP1 | \
								CRG_IRQ_MIPI1 | \
								CRG_IRQ_DIP2 | \
								CRG_IRQ_HDMI | \
								CRG_IRQ_HDMI_WAKEUP | \
								CRG_IRQ_CABC0 | \
								CRG_IRQ_CABC1)

#define CRG_IRQ_DIPS					(CRG_IRQ_DIP0 | \
								CRG_IRQ_DIP1 | \
								CRG_IRQ_DIP2)



#define CRG_IRQ_MIPIS					(CRG_IRQ_MIPI0 | \
								CRG_IRQ_MIPI1)

/* interrupt sub source for DIP*/
#define CRG_IRQ_DIP_EOVF						(1 << DIP_EOVF_POS)
#define CRG_IRQ_DIP_EOF							(1 << DIP_EOF_POS)
#define CRG_IRQ_DIP_FIFO0_ACCESS_ERR 			(1 << DIP_FIFO0_ACCESS_ERR_POS)
#define CRG_IRQ_DIP_FIFO0_BECOMES_EMPTY_ERR 	(1 << DIP_FIFO0_BECOMES_EMPTY_POS)
#define CRG_IRQ_DIP_FIFO1_ACCESS_ERR			(1 << DIP_FIFO1_ACCESS_ERR_POS)
#define CRG_IRQ_DIP_FIFO1_BECOMES_EMPTY_ERR 	(1 << DIP_FIFO1_BECOMES_EMPTY_POS)
#define CRG_IRQ_DIP_COMBINATAION_ERR			(1 << DIP_COMBINATION_ERR_POS)

#define CRG_IRQ_DIP_ERR_ALL					(CRG_IRQ_DIP_FIFO0_ACCESS_ERR | \
									CRG_IRQ_DIP_FIFO1_ACCESS_ERR | \
									CRG_IRQ_DIP_COMBINATAION_ERR)

#define CRG_IRQ_DIP_ALL					(CRG_IRQ_DIP_EOVF | \
									CRG_IRQ_DIP_EOF | \
									CRG_IRQ_DIP_ERR_ALL)

/* interrupt sub source for MIPI*/
#define CRG_IRQ_MIPI_HS_DATA_COMPLETE		(1 << MIPI_HS_DATA_COMPLETE_POS)
#define CRG_IRQ_MIPI_FRAME_COMPLETE			(1 << MIPI_FRAME_COMPLETE_POS)
#define CRG_IRQ_MIPI_TXDATA_FIFO_URERR		(1 << MIPI_TXDATA_FIFO_URERR_POS)

/* interrupt sub source for MXD */
#define CRG_IRQ_MXD_NR					(1 << MXD_NR_POS)
#define CRG_IRQ_MXD_RE 					(1 << MXD_RE_POS)
#define CRG_IRQ_MXD_CE					(1 << MXD_CE_POS)
#define CRG_IRQ_MXD_ALL					(CRG_IRQ_MXD_NR | \
									CRG_IRQ_MXD_RE | \
									CRG_IRQ_MXD_CE)

/* interrupt sub source for GFX For Virtual*/
#define CRG_IRQ_GFX0_DONE				(1 << GFX0_DONE_POS)
#define CRG_IRQ_GFX1_DONE 				(1 << GFX1_DONE_POS)

#define CRG_IRQ_MIPI_ALL					(CRG_IRQ_MIPI_HS_DATA_COMPLETE | \
											CRG_IRQ_MIPI_FRAME_COMPLETE | \
											CRG_IRQ_MIPI_TXDATA_FIFO_URERR)

enum odin_crg_clk_gate {
	CRG_CORE_CLK,
	CRG_OTHER_CLK
};

enum odin_crg_core_clk {
	VSCL0_CLK =		(1 << 0),
	VSCL1_CLK =		(1 << 1),
	VDMA_CLK =  	(1 << 2),
	MXD_CLK = 		(1 << 3),
	CABC_CLK =	 	(1 << 4),
	FMT_CLK =		(1 << 5),
	GDMA0_CLK = 	(1 << 6),
	GDMA1_CLK = 	(1 << 7),
	GDMA2_CLK =		(1 << 8),
	OVERLAY_CLK =	(1 << 9),
	GSCL_CLK =		(1 << 10),
	ROTATOR_CLK =	(1 << 11),
	SYNCGEN_CLK =	(1 << 12),
	DIP0_CLK =		(1 << 13),
	MIPI0_CLK =		(1 << 14),
	DIP1_CLK =		(1 << 15),
	MIPI1_CLK =		(1 << 16),
	GFX0_CLK =		(1 << 17),
	GFX1_CLK =		(1 << 18),
	DIP2_CLK =		(1 << 19),
	HDMI_CLK =		(1 << 20),
};

enum odin_crg_other_clk {
	DISP0_CLK =			(1 << 16),
	DISP1_CLK = 		(1 << 17),
	HDMI_DISP_CLK =		(1 << 18),
	DPHY0_OSC_CLK = 	(1 << 19),
	DPHY1_OSC_CLK = 	(1 << 20),
	SFR_CLK 	  = 	(1 << 21),
	CEC_CLK		  = 	(1 << 22),
	TX_CLK_ESC0   = 	(1 << 23),
	TX_CLK_ESC1   = 	(1 << 24),
};

enum odin_crg_mem_deep_sleep{
	PD1_DEEP_SLEEP =	(1 << 0),
	PD2_DEEP_SLEEP =	(1 << 1),
	PD3_DEEP_SLEEP =	(1 << 2),
	PD4_DEEP_SLEEP =	(1 << 3),
	PD5_DEEP_SLEEP =	(1 << 4),
	PD6_DEEP_SLEEP =	(1 << 5),

	HDMI_SHUT_DOWN =	(1 << 16),
	HDMI_SLEEP 	   =	(1 << 17),
	DIP0_SLEEP	   =	(1 << 18),
	DIP1_SLEEP 	   =	(1 << 19),
};

enum odin_crg_soft_reset{
	VSCL0_RESET	  	=	(1 << 0),
	VSCL1_RESET		=	(1 << 1),
	VDMA_RESET		=	(1 << 2),
	MXD_RESET		=	(1 << 3),
	CABC_RESET 		=	(1 << 4),
	FORMATTER_RESET =	(1 << 5),
	GDMA0_RESET 	=	(1 << 6),
	GDMA1_RESET 	=	(1 << 7),
	GDMA2_RESET	   	=	(1 << 8),
	OVERLAY_RESET 	=	(1 << 9),
	GSCL_RESET	    =	(1 << 10),
	ROTATOR_RESET	=	(1 << 11),
	SYNCGEN_RESET	=	(1 << 12),
	DIP0_RESET	  	=	(1 << 13),
	MIPI0_RESET	   	=	(1 << 14),
	DIP1_RESET	   	=	(1 << 15),
	MIPI1_RESET	   	=	(1 << 16),
	GFX0_RESET	   	=	(1 << 17),
	GFX1_RESET	   	=	(1 << 18),
	DIP2_RESET	   	=	(1 << 19),
	HDMI_RESET	   	=	(1 << 20),
};

#define MAX_DSS_MANAGERS	3
#define MAX_DSS_OVERLAYS	7
#define MAX_DSS_OVERLAY_COP	MAX_DSS_OVERLAYS - 1
#define MAX_DSS_LCD_MANAGERS	2
#define MAX_DSS_NUM_DSI		2

#define FLD_MASK(start, end)	(((1 << ((start) - (end) + 1)) - 1) << (end))
#define FLD_VAL(val, start, end) (((val) << (end)) & FLD_MASK(start, end))
#define FLD_GET(val, start, end) (((val) & FLD_MASK(start, end)) >> (end))
#define FLD_MOD(orig, val, start, end) \
	(((orig) & ~FLD_MASK(start, end)) | FLD_VAL(val, start, end))

#define ODIN_COLOR_MODE		0x000F
#define ODIN_COLOR_BYTE_SWAP 	0x0030
#define ODIN_COLOR_WORD_SWAP 	0x0040
#define ODIN_COLOR_ALPHA_SWAP	0x0080
#define ODIN_COLOR_RGB_SWAP	0x0700
#define ODIN_COLOR_YUV_SWAP	0x0800
#define ODIN_COLOR_PIXEL_ALPHA	0x1000
#define ODIN_COLOR_PLANE_UV_SWAP	0x2000

#define CHECK_COLOR_VALUE(val, mod)	(mod == ODIN_COLOR_MODE)?(val & mod):\
					(mod == ODIN_COLOR_BYTE_SWAP)?((val & mod)>>4):\
					(mod == ODIN_COLOR_WORD_SWAP)?((val & mod)>>6):\
					(mod == ODIN_COLOR_ALPHA_SWAP)?((val & mod)>>7):\
					(mod == ODIN_COLOR_RGB_SWAP)?((val & mod)>>8):\
					(mod == ODIN_COLOR_YUV_SWAP)?((val & mod)>>11):\
					(mod == ODIN_COLOR_PIXEL_ALPHA)?((val & mod)>>12):\
					(mod == ODIN_COLOR_PLANE_UV_SWAP)?((val & mod)>>13):0

struct odin_dss_device;
struct odin_overlay_manager;
extern struct odin_dss_device *board_odin_dss_devices[3];

enum odin_resource_index {
	ODIN_DSS_VID0_RES		= 0,
	ODIN_DSS_VID1_RES		= 1,
	ODIN_DSS_VID2_RES		= 2,
	ODIN_DSS_GRA0_RES		= 3,
	ODIN_DSS_GRA1_RES		= 4,
	ODIN_DSS_GRA2_RES		= 5,
	ODIN_DSS_GSCL_RES		= 6,
	ODIN_DSS_OVL_RES		= 7,
	ODIN_DSS_CABC_RES		= 8,
	ODIN_DSS_SYNC_GEN_RES	= 9,
#if 1
	ODIN_DSS_MAX_RES		= 10,
#endif
};

enum odin_display_format {
	ODIN_DISPLAY_2D		= 0,
	ODIN_DISPLAY_3D		= 1,
};

enum odin_display_type {
	ODIN_DISPLAY_TYPE_RGB		= 1 << 1,
	ODIN_DISPLAY_TYPE_DSI		= 1 << 2,
	ODIN_DISPLAY_TYPE_HDMI		= 1 << 3,
	ODIN_DISPLAY_TYPE_MEM2MEM	= 1 << 4,
	ODIN_DISPLAY_TYPE_HDMI_ON	= 1 << 5,
};

/* This is Odin WriteBack Index*/
enum odin_wb_idx {
	ODIN_WB_VID0			= 0, //Frame Buffer Dedicated
	ODIN_WB_VID1			= 1,
	ODIN_WB_mSCALER			= 2,
};

/* This is Odin Plane for Overlay*/
enum odin_overlay_src_num {
	ODIN_DSS_OVERLAY_SRC_NUM_0		= 0,
	ODIN_DSS_OVERLAY_SRC_NUM_1		= 1,
	ODIN_DSS_OVERLAY_SRC_NUM_2		= 2, //Frame Buffer Dedicated
	ODIN_DSS_OVERLAY_SRC_NUM_3		= 3,
	ODIN_DSS_OVERLAY_SRC_NUM_4		= 4,
	ODIN_DSS_OVERLAY_SRC_NUM_5		= 5,
	ODIN_DSS_OVERLAY_SRC_NUM_6		= 6
};

enum odin_vid2_overlay_plane {
	ODIN_DSS_VID2_CHANGE_FROM_VID0 = 0,
	ODIN_DSS_VID2_CHANGE_FROM_VID1 = 1
};

enum odin_color_mode {
	ODIN_DSS_COLOR_RGB565	 = 0x0,  /* RGB565 */
	ODIN_DSS_COLOR_RGB555	 = 0x1,  /* RGB555 - global alpha setting */
	ODIN_DSS_COLOR_RGB444	 = 0x2,  /* RGB444 - global alpha setting */
	ODIN_DSS_COLOR_RGB888	 = 0x3,  /* RGB888 - global alpha setting */
	ODIN_DSS_COLOR_RESERVED1 = 0x4,  /* RESERVED */
	ODIN_DSS_COLOR_YUV422S	 = 0x5,  /* YUV422 STREAM*/
	ODIN_DSS_COLOR_RESERVED2 = 0x6,  /* RESERVED */
	ODIN_DSS_COLOR_RESERVED3 = 0x7,  /* RESERVED */
	ODIN_DSS_COLOR_YUV422P_2 = 0x8,  /* YUV422 2PLANE*/
	ODIN_DSS_COLOR_YUV224P_2 = 0x9,  /* YUV224 2PLANE*/
	ODIN_DSS_COLOR_YUV420P_2 = 0xa,  /* YUV420 2PLANE*/
	ODIN_DSS_COLOR_YUV444P_2 = 0xb,  /* YUV444 2PLANE*/
	ODIN_DSS_COLOR_YUV422P_3 = 0xc,  /* YUV422 3PLANE*/
	ODIN_DSS_COLOR_YUV224P_3 = 0xd,  /* YUV224 3PLANE*/
	ODIN_DSS_COLOR_YUV420P_3 = 0xe,  /* YUV420 3PLANE*/
	ODIN_DSS_COLOR_YUV444P_3 = 0xf,  /* YUV444 3PLANE*/

	// Byte - swap
	ODIN_DSS_COLOR_BYTE_ABCD = 0x00,
	ODIN_DSS_COLOR_BYTE_BADC = 0x10,
	ODIN_DSS_COLOR_BYTE_CDAB = 0x20,
	ODIN_DSS_COLOR_BYTE_DCBA = 0x30,

	// Word - swap
	ODIN_DSS_COLOR_WORD_AB = 0x00,
	ODIN_DSS_COLOR_WORD_BA = 0x40,

	// Alpha - swap
	ODIN_DSS_COLOR_ALPHA_ARGB  = 0x00,
	ODIN_DSS_COLOR_ALPHA_RGBA  = 0x80,

	// RGB - swap
	ODIN_DSS_COLOR_RGB_RGB  = 0x000,
	ODIN_DSS_COLOR_RGB_RBG	= 0x100,
	ODIN_DSS_COLOR_RGB_BGR	= 0x200,
	ODIN_DSS_COLOR_RGB_BRG	= 0x300,
	ODIN_DSS_COLOR_RGB_GRB	= 0x400,
	ODIN_DSS_COLOR_RGB_GBR	= 0x500,

	// YUV - swap
	ODIN_DSS_COLOR_YUV_UV	  = 0x000,
	ODIN_DSS_COLOR_YUV_VU	  = 0x800,

	// global_alpha
	ODIN_DSS_COLOR_PIXEL_ALPHA= 0x1000,
	// UV_SWAP
	ODIN_DSS_COLOR_PLANE_UV_SWAP= 0x2000,
};

#define ODIN_DSS_COLOR_RGB_ALL  	ODIN_DSS_COLOR_RGB565 | ODIN_DSS_COLOR_RGB555 | ODIN_DSS_COLOR_RGB444 | \
					ODIN_DSS_COLOR_RGB888

#define ODIN_DSS_COLOR_YUV_2PLANE 	ODIN_DSS_COLOR_YUV422P_2 | ODIN_DSS_COLOR_YUV224P_2 | ODIN_DSS_COLOR_YUV420P_2 | \
					ODIN_DSS_COLOR_YUV444P_2

#define ODIN_DSS_COLOR_YUV_3PLANE	ODIN_DSS_COLOR_YUV422P_3 | ODIN_DSS_COLOR_YUV224P_3 | ODIN_DSS_COLOR_YUV420P_3 | \
					ODIN_DSS_COLOR_YUV444P_3

#define ODIN_DSS_COLOR_YUV_ALL 		ODIN_DSS_COLOR_YUV422S | ODIN_DSS_COLOR_YUV_2PLANE | ODIN_DSS_COLOR_YUV_3PLANE

enum odin_color_fmt {
	// TODO:
	ODIN_DSS_COLOR_YUV2,		// YUV422
	ODIN_DSS_COLOR_UYVY,
	ODIN_DSS_COLOR_ARGB32,
	ODIN_DSS_COLOR_RGBA32,
	ODIN_DSS_COLOR_RGBX32,
	ODIN_DSS_COLOR_NV12,		//YUV420
};

enum odin_dss_trans_key_type {
	ODIN_DSS_COLOR_KEY_GFX_DST = 0,
	ODIN_DSS_COLOR_KEY_VID_SRC = 1,
};


enum odin_dss_dsi_pixel_format {
	ODIN_DSS_DSI_FMT_RGB888,
	ODIN_DSS_DSI_FMT_RGB666,
	ODIN_DSS_DSI_FMT_RGB666_PACKED,
	ODIN_DSS_DSI_FMT_RGB565,
};

enum odin_dss_dsi_mode {
	ODIN_DSS_DSI_CMD_MODE = 0,
	ODIN_DSS_DSI_VIDEO_MODE,
};

enum odin_dss_flip_mode {
	ODIN_DSS_V_FLIP = 1 << 0,
	ODIN_DSS_H_FLIP = 1 << 1,
};

enum odin_display_caps {
	ODIN_DSS_DISPLAY_CAP_MANUAL_UPDATE	= 1 << 0,
	ODIN_DSS_DISPLAY_CAP_TEAR_ELIM		= 1 << 1,
};

enum odin_dss_update_mode {
	ODIN_DSS_UPDATE_DISABLED = 0,
	ODIN_DSS_UPDATE_AUTO,
	ODIN_DSS_UPDATE_MANUAL,
};


enum odin_dss_display_state {
	ODIN_DSS_DISPLAY_DISABLED = 0,
	ODIN_DSS_DISPLAY_ACTIVE,
	ODIN_DSS_DISPLAY_SUSPENDED,
	ODIN_DSS_DISPLAY_DISCONNECTED,
};

/* XXX perhaps this should be removed */
enum odin_dss_overlay_managers {
	ODIN_DSS_OVL_MGR_LCD0,
	ODIN_DSS_OVL_MGR_LCD1,
	ODIN_DSS_OVL_MGR_HDMI,
};

/* clockwise rotation angle */
enum odin_dss_rotation_angle {
	ODIN_DSS_ROT_0   = 0,
	ODIN_DSS_ROT_90  = 1,
	ODIN_DSS_ROT_180 = 2,
	ODIN_DSS_ROT_270 = 3,
};


enum odin_overlay_caps {
	ODIN_DSS_OVL_CAP_NONE 			= 0,
	ODIN_DSS_OVL_CAP_SCALE 			= 1 << 0,
	ODIN_DSS_OVL_CAP_GLOBAL_ALPHA 	= 1 << 1,
	ODIN_DSS_OVL_CAP_PRE_MULT_ALPHA = 1 << 2,
	ODIN_DSS_OVL_CAP_S3D		= 1 << 3,
	ODIN_DSS_OVL_CAP_XDENGINE	= 1 << 4,
};

enum odin_overlay_manager_caps {
	ODIN_DSS_OVL_MGR_CAP_DU = 1 << 0,
};

enum odin_overlay_zorder {
	ODIN_DSS_OVL_ZORDER_0   = 0,
	ODIN_DSS_OVL_ZORDER_1   = 1,
	ODIN_DSS_OVL_ZORDER_2   = 2,
	ODIN_DSS_OVL_ZORDER_3   = 3,
	ODIN_DSS_OVL_ZORDER_4	= 4,
	ODIN_DSS_OVL_ZORDER_5	= 5,
	ODIN_DSS_OVL_ZORDER_6	= 6
};

enum odin_dss_mXDalgorithm_level {
	ODIN_MXD_LEVEL_STRONG		= 0,
	ODIN_MXD_LEVEL_MEDIUM		= 1,
	ODIN_MXD_LEVEL_WEEK
};


typedef struct {
	enum odin_resource_index resource_index;
	enum odin_color_mode color_mode;
	int width;
	int height;
	int scl_en;
	int scenario;
}mxd_verify_data;

typedef struct {
	/* NR */
	bool nr_en;
	enum odin_dss_mXDalgorithm_level nr_level;

	/* RE */
	bool re_en;
	enum odin_dss_mXDalgorithm_level re_level;

	/* CE */
	bool ce_en;
	enum odin_dss_mXDalgorithm_level ce_level;

}mxd_algorithm_data;

typedef enum
{
	/* dss_input -> TPG -> NR -> Scaler -> RE -> CE -> CTI -> Display : 1 */
	MXDDU_SCENARIO_TEST1 = 0,
	/* dss_input -> TPG -> NR -> RE -> CE -> CTI -> Display : 2 */
	MXDDU_SCENARIO_TEST2,
	/* Scaler -> TPG -> NR -> RE -> CE -> CTI -> Display : 3 */
	MXDDU_SCENARIO_TEST3,
	/* Scaler -> RE -> CTI -> CE -> Display */
	MXDDU_SCENARIO_TEST4,
	MXDDU_SCENARIO_MAX
} mXdDu_SCENARIO_Test;

struct odin_dss_board_info {
	int num_devices;
	struct odin_dss_device **devices;
	struct odin_dss_device *default_device;
	int (*dsi_enable_pads)(int dsi_id, unsigned lane_mask);
	int (*dsi_disable_pads)(int dsi_id, unsigned lane_mask);
};

struct odin_display_platform_data {
	struct odin_dss_board_info *board_data;
};

enum odin_panel_config {
	ODIN_DSS_LCD_IVS		= 1<<0,
	ODIN_DSS_LCD_IHS		= 1<<1,
	ODIN_DSS_LCD_IPC		= 1<<2,
	ODIN_DSS_LCD_IEO		= 1<<3,
	ODIN_DSS_LCD_RF			= 1<<4,
	ODIN_DSS_LCD_ONOFF		= 1<<5,

	ODIN_DSS_LCD_TFT		= 1<<20,
};

enum dsscomp_setup_mode {
	DSSCOMP_SETUP_MODE_DISPLAY 			 = (1 << 0),	/* calls display update */
	DSSCOMP_SETUP_MODE_DISPLAY_CAPTURE   = (1 << 1),	/* display and capture in same sync*/
	DSSCOMP_SETUP_MODE_CAPTURE 			 = (1 << 2),	/* Indpendent display capture*/
	DSSCOMP_SETUP_MODE_DISPLAY_MEM2MEM	 = (1 << 3),	/* display and mem2mem in same sync*/
	DSSCOMP_SETUP_MODE_MEM2MEM			 = (1 << 4),	/* mem2mem*/
};

/* Init with the board info */
extern int odin_display_init(struct odin_dss_board_info *board_data);
extern int odin_hdmi_init(void);

struct odin_video_timings {
	/* RGB Interface*/
	/* Unit: pixels */
	__u16 x_res;
	/* Unit: pixels */
	__u16 y_res;
	/* Unit: mm (LCD height)*/
	__u32 height;
	/* Unit: mm (LCD width)*/
	__u32 width;
	/* Unit: KHz */
	__u32 pixel_clock;
	/* Unit: bit */
	__u16 data_width;
	/* Unit: pixel clocks */
	__u16 hsw;	/* Horizontal synchronization pulse width */
	/* Unit: pixel clocks */
	__u16 hfp;	/* Horizontal front porch */
	/* Unit: pixel clocks */
	__u16 hbp;	/* Horizontal back porch */
	/* Unit: line clocks */
	__u16 vsw;	/* Vertical synchronization pulse width */
	/* Unit: line clocks */
	__u16 vfp;	/* Vertical front porch */
	/* Unit: line clocks */
	__u16 vbp;	/* Vertical back porch */

	__u16 rgb_swap;

	/* HDMI Interface */
	__u16 mPixelRepetitionInput;

	__u16 mPixelClock;

	__u8 mInterlaced;

	__u16 mHActive;

	__u16 mHBlanking;

	__u16 mHBorder;

	__u16 mHImageSize;

	__u16 mHSyncOffset;

	__u16 mHSyncPulseWidth;

	__u8 mHSyncPolarity;

	__u16 mVActive;

	__u16 mVBlanking;

	__u16 mVBorder;

	__u16 mVImageSize;

	__u16 mVSyncOffset;

	__u16 mVSyncPulseWidth;

	__u8 mVSyncPolarity;

	/*
	 * LCD specific representation
	 *
	 * sync polatiry
	 *    1: active high
	 *    0: active low
	 *
	 * pclk polarity
	 *    0: latch data at the falling edge
	 *    1: latch data at the rising edge
	 */

	/* Comon Interface */
	__u16 vsync_pol;	/* vsync_pol */
	__u16 hsync_pol;	/* hsync_pol */
	__u16 blank_pol;	/* de_pol */
	__u16 pclk_pol;	/* pclk_pol */
};

enum odindss_completion_status {
	DSS_COMPLETION_PROGRAMMED	= (1 << 1),
	DSS_COMPLETION_DISPLAYING	= (1 << 2),
	DSS_COMPLETION_PROGRAM_ERROR	= (1 << 15),
	DSS_COMPLETION_MANUAL_DISPLAYED	= (1 << 15),
	DSS_COMPLETION_FENCE_RELEASE_N_REFRESH	= (1 << 16),
	DSS_COMPLETION_FENCE_RELEASE	= (1 << 17),
};

struct odindss_ovl_cb {
	/* optional callback method */
	u32 (*fn)(void *data, int id, int status);
	void *data;
	u32 mask;
};

struct odin_dss_cpr_coefs {
	s16 rr, rg, rb;
	s16 gr, gg, gb;
	s16 br, bg, bb;
};



struct odin_overlay_info {
	bool enabled;
	/* gscl enable */
	bool gscl_en;
	bool invisible;

	u32 p_y_addr;
	u32 p_u_addr;
	u32 p_v_addr;

	u64 debug_addr1; /* Physical Address For Debug */
/* Src Buffer Data*/
	u16 screen_width;
	u16 width;
	u16 height;
	enum odin_color_mode color_mode;

	u8 sync_sel;
	u8 buf_sel;
	u8 dst_sel;

	enum odin_color_fmt input_fmt;
	u8 rotation;
	bool mirror;
/* DMA Crop Data*/
	u16 crop_x;
	u16 crop_y;
	u16 crop_w;
	u16 crop_h;

/* Window Out Data*/
 	u16 out_x;
	u16 out_y;
	u16 out_w;	/* if 0, out_width == width */
	u16 out_h;	/* if 0, out_height == height */

	u8 blending;
	u8 global_alpha;
	u8 pre_mult_alpha;
	enum odin_overlay_zorder zorder;
	bool mxd_use;
	int mxd_scenario;
	mxd_algorithm_data level_data;
	bool formatter_use;

	u8 yuv_swap;
	u8 rgb_swap;
	u8 alpha_swap;
	u8 byte_swap;
	u8 word_swap;
};

struct odin_overlay {
	struct kobject kobj;
	struct list_head list;

	/* static fields */
	const char *name;
	enum odin_dma_plane id;

	enum odin_color_mode supported_modes;
	enum odin_overlay_caps caps;

	/* dynamic fields */
	struct odin_overlay_manager *manager;
	struct odin_overlay_info info;

	/* if true, info has been changed, but not applied() yet */
	bool info_dirty;

	/*
	 * The following functions do not block:
	 *
	 * is_enabled
	 * set_overlay_info
	 * get_overlay_info
	 *
	 * The rest of the functions may block and cannot be called from
	 * interrupt context
	 */

	int (*set_manager)(struct odin_overlay *ovl,
		struct odin_overlay_manager *mgr);
	int (*unset_manager)(struct odin_overlay *ovl);

	int (*set_overlay_info)(struct odin_overlay *ovl,
			struct odin_overlay_info *info);
	void (*get_overlay_info)(struct odin_overlay *ovl,
			struct odin_overlay_info *info);
};

enum odin_writeback_mode {
	ODIN_WB_CAPTURE_MODE = 0,
	ODIN_WB_DISPLAY_MEM2MEM_MODE = 1, /* DISPLAY and MEM2MEM Sync is same */
	ODIN_WB_MEM2MEM_MODE = 2, /* DISPLAY and MEM2MEM Sync is different */
};

struct odin_writeback_info {
	bool enabled;
	bool info_dirty;

	u32 p_y_addr;
	u32 p_uv_addr;

	u64 debug_addr1; /* Physical Address For Debug */

	u16 width;
	u16	height;

	/* Window Out Data*/
	u16 out_x;
	u16 out_y;
	u16 out_w;	/* if 0, out_width == width */
	u16 out_h; /* if 0, out_height == height */

	enum odin_color_mode			color_mode;
	/* capture or mem2mem mode */
	enum odin_writeback_mode		mode;
	enum odin_channel		channel;
};

struct odin_writeback {
	struct kobject			kobj;
	struct list_head		list;

	/* static fields */
	const char *name;
	enum odin_wb_idx id;
	enum odin_dma_plane plane;

	bool				info_dirty;
	int				width;
	int				height;
	/* mutex to control access to wb data */
	struct mutex			lock;
	struct odin_writeback_info	info;
	struct completion		wb_completion;
#if 0
#if  1
	struct timer_list wb_timer;
#else
	struct hrtimer wb_timer;
#endif
#endif

	bool (*check_wb)(struct odin_writeback *wb);
	int (*set_wb_info)(struct odin_writeback *wb,
			struct odin_writeback_info *info);
	void (*get_wb_info)(struct odin_writeback *wb,
			struct odin_writeback_info *info);
	int (*register_framedone)(struct odin_writeback *wb);
	int (*unregister_framedone)(struct odin_writeback *wb);
	int (*wait_framedone)(struct odin_writeback *wb);
	int (*initial_completion)(struct odin_writeback *wb);
};

struct odin_s3d_info {
	bool enable;
	bool enable_2dto3d;

	// TODO:

};

struct odin_s3d {
	struct odin_s3d_info info;
	int (*enable)(struct odin_overlay *ovl);
	void (*disable)(struct odin_overlay *ovl);

	// TODO:

};

struct odin_overlay_manager_info {
	u32 default_color;

	enum odin_dss_trans_key_type trans_key_type;
	u32 trans_key;
	bool trans_enabled;

	bool alpha_enabled;

	u16 size_x;
	u16 size_y;

	struct odindss_ovl_cb cb;
};

struct odin_overlay_manager {
	struct kobject kobj;
	struct list_head list;

	/* static fields */
	const char *name;
	enum odin_channel id;
	enum odin_dss_flip_mode mgr_flip;		// manager_flip

	int num_overlays;

	struct odin_overlay **overlays;
	enum odin_display_type supported_displays;		// LCD#0, LCD#1, HDMI

	/* dynamic fields */
	struct odin_dss_device *device;
	struct odin_overlay_manager_info info;
	enum dsscomp_setup_mode mode;

	bool vsync_ctrl;

	bool device_changed;
	bool info_dirty;
	bool frame_skip;
	bool cabc_en;

#if 1
	struct odinfb_device *fbdev;
#endif
	/*
	 * The following functions do not block:
	 *
	 * set_manager_info
	 * get_manager_info
	 * apply
	 *
	 * The rest of the functions may block and cannot be called from
	 * interrupt context
	 */

	int (*set_device)(struct odin_overlay_manager *mgr,
		struct odin_dss_device *dssdev);
	int (*unset_device)(struct odin_overlay_manager *mgr);

	int (*set_manager_info)(struct odin_overlay_manager *mgr,
			struct odin_overlay_manager_info *info);
	void (*get_manager_info)(struct odin_overlay_manager *mgr,
			struct odin_overlay_manager_info *info);
	int (*update_manager_cache)(struct odin_overlay_manager *mgr);

	int (*clk_enable)(u32 clk_mask, u32 clk_dmask, bool mem2mem_mode);
	int (*apply)(struct odin_overlay_manager *mgr,
		enum dsscomp_setup_mode mode);
	int (*wait_for_vsync)(struct odin_overlay_manager *mgr);
	int (*wait_for_framedone)(struct odin_overlay_manager *mgr,
		unsigned long timeout);

	int (*blank)(struct odin_overlay_manager *mgr, bool wait_for_vsync);
	void (*dump_cb)(struct odin_overlay_manager *mgr, struct seq_file *s);
	int (*cabc)(struct odin_overlay_manager *mgr, bool cabc_en);

	int (*enable)(struct odin_overlay_manager *mgr);
	int (*disable)(struct odin_overlay_manager *mgr);
};

enum odin_dss_dsi_input_format {
	ODIN_DSS_DSI_IN_FMT_RGB24,
	ODIN_DSS_DSI_IN_FMT_RGB565,
	ODIN_DSS_DSI_IN_FMT_MAX,
};

enum odin_mipi_dsi_output_format {
	ODIN_DSI_OUTFMT_RGB565_COMMAND_MODE	=	0,
	ODIN_DSI_OUTFMT_RGB565_VIDEO_MODE,
	ODIN_DSI_OUTFMT_RGB666_PACKED_VIDEO,
	ODIN_DSI_OUTFMT_RGB666,
	ODIN_DSI_OUTFMT_RGB24,
	ODIN_DSI_OUTFMT_RGB111_COMMAND_MODE,
	ODIN_DSI_OUTFMT_RGB332_COMMAND_MODE,
	ODNI_DSI_OUTFMT_RGB444_COMMAND_MODE,
};


struct odin_dss_device {
	struct device dev;

	bool steroscopic;			/* true: 3D LCD, false: 2D lcd  */
	enum odin_display_type type;		/* RGB, MIPI  or HDMI		*/
	enum odin_channel channel;		/* Hardware Display Device	*/
	bool first_vsync;
	bool boot_display;

	union {
		struct {
			u8 data_lines;
		} rgb;

		struct {
			u8 clk_lane;
			u8 clk_pol;
			u8 data1_lane;
			u8 data1_pol;
			u8 data2_lane;
			u8 data2_pol;
			u8 data3_lane;
			u8 data3_pol;
			u8 data4_lane;
			u8 data4_pol;

			int module;

			bool ext_te;
			u8 ext_te_gpio;
			u8 lane_num;

			u32 cmd_max_size;
			u32 frame_size;
		} dsi;

	} phy;

	struct {

		// TODO: I don't know clock source at ODIN Platform
		//struct {
		//	struct {
		//		u16 lck_div;
		//		u16 pck_div;
		//		enum odin_dss_clk_source lcd_clk_src;
		//	} channel;

		//	enum odin_dss_clk_source dispc_fclk_src;
		//} dispc;

		struct {
			/* regn is one greater than TRM's REGN value */
			u16 regn;
			u16 regm;
			u16 regm_dispc;
			u16 regm_dsi;
			u8 hs_clk;
			u8 lp_clk;
			//u16 lp_clk_div;
			//enum odin_dss_clk_source dsi_fclk_src;
		} dsi;

		struct {
			/* regn is one greater than TRM's REGN value */
			u16 regn;
			u16 regm2;
			struct regulator *hdmi_vp;
		} hdmi;
	} clocks;

	struct {
		enum odin_dss_dsi_mode dsi_mode;
		struct odin_video_timings timings;
		enum odin_panel_config config;
		enum odin_mipi_dsi_output_format dsi_pix_fmt;
		enum odin_dss_flip_mode flip;
		enum odin_dss_flip_mode flip_supported;

		u32 width_in_um;
		u32 height_in_um;

		struct fb_monspecs monspecs;
	} panel;

	u8 pixel_size;

	int reset_gpio;

	bool skip_init;

	int max_backlight_level;

	const char *name;

	/* used to match device to driver */
	const char *driver_name;

	void *data;

	struct odin_dss_driver *driver;

	/* helper variable for driver suspend/resume */
	bool activate_after_resume;

	enum odin_display_caps caps;

	struct odin_overlay_manager *manager;

	enum odin_dss_display_state state;

	/* platform specific  */
	int (*platform_enable)(struct odin_dss_driver *dssdev);
	void (*platform_disable)(struct odin_dss_driver *dssdev);
	int (*set_backlight)(struct odin_dss_driver *dssdev, int level);
	int (*get_backlight)(struct odin_dss_driver *dssdev);
};

struct odin_dss_hdmi_data
{
	int hpd_gpio;
};


// Panel driver
struct odin_dss_driver {
	struct device_driver driver;

	int (*probe)(struct odin_dss_device *);
	void (*remove)(struct odin_dss_device *);

	int (*enable)(struct odin_dss_device *display);
	void (*disable)(struct odin_dss_device *display);
	int (*suspend)(struct odin_dss_device *display);
	int (*resume)(struct odin_dss_device *display);

	int (*set_update_mode)(struct odin_dss_device *dssdev,
			enum odin_dss_update_mode);
	enum odin_dss_update_mode (*get_update_mode)(
			struct odin_dss_device *dssdev);

	int (*run_test)(struct odin_dss_device *display, int test);
	int (*update)(struct odin_dss_device *dssdev,
			       u16 x, u16 y, u16 w, u16 h);
	int (*sync)(struct odin_dss_device *dssdev);

	int (*enable_te)(struct odin_dss_device *dssdev, bool enable);
	int (*get_te)(struct odin_dss_device *dssdev);

	u8 (*get_rotate)(struct odin_dss_device *dssdev);
	int (*set_rotate)(struct odin_dss_device *dssdev, u8 rotate);

	bool (*get_mirror)(struct odin_dss_device *dssdev);
	int (*set_mirror)(struct odin_dss_device *dssdev, bool enable);

	int (*memory_read)(struct odin_dss_device *dssdev,
			void *buf, size_t size,
			u16 x, u16 y, u16 w, u16 h);

	void (*get_resolution)(struct odin_dss_device *dssdev,
			u16 *xres, u16 *yres);
	void (*get_dimensions)(struct odin_dss_device *dssdev,
			u32 *width, u32 *height);

	int (*get_recommended_bpp)(struct odin_dss_device *dssdev);

	int (*check_timings)(struct odin_dss_device *dssdev,
			struct odin_video_timings *timings);
	void (*set_timings)(struct odin_dss_device *dssdev,
			struct odin_video_timings *timings);
	void (*get_timings)(struct odin_dss_device *dssdev,
			struct odin_video_timings *timings);

	int (*get_modedb)(struct odin_dss_device *dssdev,
			  struct fb_videomode *modedb,
			  int modedb_len);
	int (*set_mode)(struct odin_dss_device *dssdev,
			struct fb_videomode *mode);

	int (*read_edid)(struct odin_dss_device *dssdev, u8 *buf, int len);
	bool (*detect)(struct odin_dss_device *dssdev);
	int (*set_fb_info)(struct odin_dss_device *dssdev, struct fb_info *info);
	int (*flush_pending)(struct odin_dss_device *dssdev);
};

/*TODO: Move this structure to manager.c*/
struct writeback_cache_data {
	/* If true, cache changed, but not written to shadow registers. Set
	 * in apply(), cleared when registers written. */
	bool dirty;
	/* If true, shadow registers contain changed values not yet in real
	 * registers. Set when writing to shadow registers, cleared at
	 * VSYNC/EVSYNC */
	bool du_dirty;

	bool enabled;

	u32 p_y_addr;
	u32 p_uv_addr;

	u64 debug_addr1; /* Physical Address For Debug */

	u16 width;
	u16 height;

	/* Window Out Data*/
	u16 out_x;
	u16 out_y;
	u16 out_w;	/* if 0, out_width == width */
	u16 out_h; /* if 0, out_height == height */

	enum odin_color_mode			color_mode;
	/* capture or mem2mem mode */
	enum odin_writeback_mode		mode;
	enum odin_channel channel;
	enum odin_dma_plane wb_plane;
};

struct coordinate {
	s32 x_left;
	s32 y_top;
	s32 x_right;
	s32 y_bottom;
};

bool dss_intersection_overlap_check(struct coordinate *layers, int layer_cnt);
int odin_dram_clk_change_before(void);
int odin_dram_clk_change_after(void);

void odin_dss_get_device(struct odin_dss_device *dssdev);
void odin_dss_put_device(struct odin_dss_device *dssdev);
#define for_each_dss_dev(d) while ((d = odin_dss_get_next_device(d)) != NULL)
struct odin_dss_device *odin_dss_get_next_device(struct odin_dss_device *from);
struct odin_dss_device *odin_dss_find_device(void *data,
		int (*match)(struct odin_dss_device *dssdev, void *data));

#define to_dss_driver(x) container_of((x), struct odin_dss_driver, driver)
#define to_dss_device(x) container_of((x), struct odin_dss_device, dev)

struct odin_overlay_manager *odin_mgr_dss_get_overlay_manager(int num);
struct odin_overlay *odin_ovl_dss_get_overlay(int num);
struct odin_writeback *odin_dss_get_writeback(int num);
int odin_mgr_dss_get_num_overlay_managers(void);
int odin_ovl_dss_get_num_overlays(void);

void odindss_display_get_dimensions(struct odin_dss_device *dssdev,
				u32 *width_in_um, u32 *height_in_um);

int odin_dsscomp_set_default_overlay(int ovl_ix, int mgr_ix);

static inline bool dssdev_manually_updated(struct odin_dss_device *dev)
{
	return dev->caps & ODIN_DSS_DISPLAY_CAP_MANUAL_UPDATE &&
		dev->driver->get_update_mode(dev) == ODIN_DSS_UPDATE_MANUAL;
}

/* generic callback handling */
static inline void dss_ovl_cb(struct odindss_ovl_cb *cb, int id, int status)
{
	if (cb->fn && (cb->mask & status))
		cb->mask &= cb->fn(cb->data, id, status);
	if (status & DSS_COMPLETION_DISPLAYING)
		cb->mask = 0;
	if (!cb->mask)
		cb->fn = NULL;
}

void odinfb_fb2dss_timings(struct fb_videomode *fb_timings,
                        struct odin_video_timings *dss_timings);

struct odin_dsi_panel_data {
	const char *name;

	int reset_gpio;

	bool use_ext_te;
	int ext_te_gpio;

	unsigned esd_interval;
	unsigned ulps_timeout;

	int max_backlight_level;
	int (*set_backlight)(struct odin_dss_device *dssdev, int level);
	int (*get_backlight)(struct odin_dss_device *dssdev);
};

struct odin_hdmi_panel_data {
	const char *name;

	int reset_gpio;

	bool use_ext_te;
	int ext_te_gpio;

	unsigned esd_interval;
	unsigned ulps_timeout;

	int max_backlight_level;
	int (*set_backlight)(struct odin_dss_device *dssdev, int level);
	int (*get_backlight)(struct odin_dss_device *dssdev);
};


struct odin_rgb_panel_data {
	const char *name;

	int reset_gpio;

	bool use_ext_te;
	int ext_te_gpio;

	unsigned esd_interval;
	unsigned ulps_timeout;

	int max_backlight_level;
	int (*set_backlight)(struct odin_dss_device *dssdev, int level);
	int (*get_backlight)(struct odin_dss_device *dssdev);
};


#endif	// __ODIN_ODINDSS_H__
