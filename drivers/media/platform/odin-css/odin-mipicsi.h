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

#ifndef __ODIN_MIPICSI_H__
#define __ODIN_MIPICSI_H__


#define CSI2_CTRL			0x000
#define DPHY_CTRL			0x004
#define VBLANK_SIZE 		0x008
#define VSTART_SIZE 		0x00C
#define CSI2_ERROR			0x010
#define ESCAPE_MODE_03		0x014
#define ESCAPE_MODE_47		0x018
#define INTERRUPT_03		0x01C
#define PL_STATUS_03		0x020
#define PL_STATUS_46		0x024
#define PL_STATUS_711		0x028
#define EMB_DATA_TYPE_CTRL	0x030
#define EMB_DATA_CTRL01 	0x034
#define EMB_DATA_CTRL23 	0x038
#define TIF_CTRL			0x040
#define DEBUG_DLL_ON		0x068
#define DPHY_CTRL1			0x800
#define DPHY_CTRL_34H		0x834
#define DPHY_CTRL_3CH		0x83C
#define DPHY_CTRL11			0x840
#define DPHY_CTRL13 		0x84C
#define DPHY_CTRL14 		0x850
#define DPHY_CTRL15 		0x854
#define DPHY_CTRL16 		0x858
#define DPHY_CTRL20			0x880
#define DPHY_CTRL25 		0x894
#define DPHY_CTRL_HSRX_PDB	0x8D4
#define DPHY_CTRL_CU_CONT	0xB80
#define DPHY_CTRL_HSRX_CK	0xB84
#define DPHY_CTRL_CLEAR		0xC00


#define TIF_BPS_080_TO_100	0x0
#define TIF_BPS_100_TO_120	0x1
#define TIF_BPS_120_TO_160	0x2
#define TIF_BPS_160_TO_200	0x3
#define TIF_BPS_200_TO_240	0x4
#define TIF_BPS_240_TO_320	0x5
#define TIF_BPS_320_TO_390	0x6
#define TIF_BPS_390_TO_450	0x7
#define TIF_BPS_450_TO_510	0x8
#define TIF_BPS_510_TO_560	0x9
#define TIF_BPS_560_TO_640	0xA
#define TIF_BPS_640_TO_690	0xB
#define TIF_BPS_690_TO_770	0xC
#define TIF_BPS_770_TO_870	0xD
#define TIF_BPS_870_TO_950	0xE
#define TIF_BPS_950_TO_1000	0xF

#define MIPI_CSI_TIF_SEL_DLANE0			0x7
#define MIPI_CSI_TIF_SEL_DLANE1			0x8
#define MIPI_CSI_TIF_CONTROL_DEFAULT	0x0E
#define SHIFT_TIF_BAND_CONTROL			4
#define MASK_TIF_BAND_CONTROL			0xF0
#define MIPI_CSI_TIF_SPEED_ADDR			0x61

#define HSRX_BYTE_SP_CK					0x1
#define HSRX_DDR_SP_CK					0x2
#define HSRX_IN_SP_CK					0x10

#define INT_MASK_OFFSET					16
#define INT_CLEAR_OFFSET				8
#define INT_ESCAPE_MODE					0x1
#define INT_CSI_ERROR					0x2
#define INT_EMBEDDED_DATA				0x4
#define INT_FRAME_START					0x8
#define INT_FRAME_END					0x10

#define GATE_CLOCK_MODE					0x00
#define CONTINUOUS_MODE					0x3F /* 0x3C */

typedef union {
	struct {
		volatile unsigned int mipi_rx_enable			: 1;
		volatile unsigned int ecc_check_enable			: 1;
		volatile unsigned int crc_check_enable			: 1;
		volatile unsigned int flse_err_enable			: 1;
		volatile unsigned int data_type_err_enable		: 1;
		volatile unsigned int escape_mode_err_enable	: 1;
		volatile unsigned int line_data_size_err_enable	: 1;
		volatile unsigned int reserved0					: 1;
		volatile unsigned int line_byte_cnt_enable		: 1;
		volatile unsigned int emb_data_write_enable 	: 1;
		volatile unsigned int vsync_start_enable		: 1;
		volatile unsigned int line_cnt_enable			: 1;
		volatile unsigned int vsync_out_pol_enable		: 1;
		volatile unsigned int v_fix_en					: 1;
		volatile unsigned int reserved1					: 2;
		volatile unsigned int state_clear				: 1;
		volatile unsigned int reserved2					: 7;
		volatile unsigned int reg_up_en					: 1;
		volatile unsigned int reserved3					: 3;
		volatile unsigned int lane0_direction			: 1;
		volatile unsigned int lane1_direction			: 1;
		volatile unsigned int lane2_direction			: 1;
		volatile unsigned int lane3_direction			: 1;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_CSI2_CTRL;

typedef union {
	struct {
		volatile unsigned int slave_lane0_enable		: 1;
		volatile unsigned int slave_lane1_enable		: 1;
		volatile unsigned int slave_lane2_enable		: 1;
		volatile unsigned int slave_lane3_enable		: 1;
		volatile unsigned int slave_clock_enable		: 1;
		volatile unsigned int reserved0					: 3;
		volatile unsigned int d_phy_reset				: 1;
		volatile unsigned int reserved1					: 3;
		volatile unsigned int slave_lane0_force_rx_mode : 1;
		volatile unsigned int slave_lane1_force_rx_mode : 1;
		volatile unsigned int slave_lane2_force_rx_mode : 1;
		volatile unsigned int slave_lane3_force_rx_mode : 1;
		volatile unsigned int reserved2					: 6;
		volatile unsigned int d_phy_all_pdown			: 1;
		volatile unsigned int reserved3					: 1;
		volatile unsigned int slave_data_lane0_swap_en	: 1;
		volatile unsigned int slave_data_lane1_swap_en	: 1;
		volatile unsigned int slave_data_lane2_swap_en	: 1;
		volatile unsigned int slave_data_lane3_swap_en	: 1;
		volatile unsigned int slave_clock_lane_swap_en	: 1;
		volatile unsigned int reserved4					: 3;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_DPHY_CTRL;

typedef union {
	struct {
		volatile unsigned int v_blank_size				: 17;
		volatile unsigned int reserved0					: 15;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_VBLANK_SIZE;

typedef union {
	struct {
		volatile unsigned int v_start_size				: 17;
		volatile unsigned int reserved0					: 15;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_VSTART_SIZE;

typedef union {
	struct {
		volatile unsigned int ecc_error 					: 1;
		volatile unsigned int crc_error 					: 1;
		volatile unsigned int flse_error					: 3;
		volatile unsigned int data_type_error				: 1;
		volatile unsigned int slave_ctrl_error0 			: 1;
		volatile unsigned int slave_lp_trans_sync_error0	: 1;
		volatile unsigned int slave_escape_entry_error0		: 1;
		volatile unsigned int slave_sot_error0				: 1;
		volatile unsigned int slave_lp1_error0				: 1;
		volatile unsigned int slave_lp0_error0				: 1;
		volatile unsigned int slave_ctrl_error1 			: 1;
		volatile unsigned int slave_lp_trans_sync_error1	: 1;
		volatile unsigned int slave_escape_entry_error1		: 1;
		volatile unsigned int slave_sot_error1				: 1;
		volatile unsigned int slave_lp1_error1				: 1;
		volatile unsigned int slave_lp0_error1				: 1;
		volatile unsigned int line_byte_cnt_error			: 1;
		volatile unsigned int slave_ctrl_error2 			: 1;
		volatile unsigned int slave_lp_trans_sync_error2	: 1;
		volatile unsigned int slave_escape_entry_error2		: 1;
		volatile unsigned int slave_sot_error2				: 1;
		volatile unsigned int slave_lp1_error2				: 1;
		volatile unsigned int slave_lp0_error2				: 1;
		volatile unsigned int slave_ctrl_error3 			: 1;
		volatile unsigned int slave_lp_trans_sync_error3	: 1;
		volatile unsigned int slave_escape_entry_error3		: 1;
		volatile unsigned int slave_sot_error3				: 1;
		volatile unsigned int slave_lp1_error3				: 1;
		volatile unsigned int slave_lp0_error3				: 1;
		volatile unsigned int reserved0						: 1;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_ERROR_STATE;

typedef union {
	struct {
		volatile unsigned int escape_mode0				: 8;
		volatile unsigned int escape_mode1				: 8;
		volatile unsigned int reserved0					: 2;
		volatile unsigned int escape_mode2				: 6;
		volatile unsigned int escape_mode3				: 8;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_ESCAPE_MODE03;

typedef union {
	struct {
		volatile unsigned int reserved0					: 2;
		volatile unsigned int escape_mode4				: 6;
		volatile unsigned int escape_mode5				: 8;
		volatile unsigned int reserved1					: 2;
		volatile unsigned int escape_mode6				: 6;
		volatile unsigned int escape_mode7				: 6;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_ESCAPE_MODE47;

typedef union {
	struct {
		volatile unsigned int escape_mode_int			: 1;
		volatile unsigned int error_int 				: 1;
		volatile unsigned int emb_data_int				: 1;
		volatile unsigned int frame_start_int			: 1;
		volatile unsigned int frame_end_int 			: 1;
		volatile unsigned int reserved0					: 3;
		volatile unsigned int escape_mode_int_clr		: 1;
		volatile unsigned int error_int_clr 			: 1;
		volatile unsigned int emb_data_int_clr			: 1;
		volatile unsigned int frame_start_int_clr		: 1;
		volatile unsigned int frame_end_int_clr 		: 1;
		volatile unsigned int reserved1					: 3;
		volatile unsigned int escape_mode_int_mask		: 1;
		volatile unsigned int error_int_mask			: 1;
		volatile unsigned int emb_data_int_mask 		: 1;
		volatile unsigned int frame_start_int_mask		: 1;
		volatile unsigned int frame_end_int_mask		: 1;
		volatile unsigned int reserved2					: 11;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_INTERRUPT03;

typedef union {
	struct {
		volatile unsigned int csi2_pl_status0			: 8;
		volatile unsigned int csi2_pl_status1			: 8;
		volatile unsigned int csi2_pl_status2			: 8;
		volatile unsigned int csi2_pl_status3			: 8;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_PL_STATUS03;

typedef union {
	struct {
		volatile unsigned int csi2_pl_status4			: 6;
		volatile unsigned int reserved0					: 2;
		volatile unsigned int csi2_pl_status5			: 4;
		volatile unsigned int csi2_pl_status6			: 4;
		volatile unsigned int reserved1					: 16;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_PL_STATUS46;

typedef union {
	struct {
		volatile unsigned int csi2_pl_status6			: 4;
		volatile unsigned int csi2_pl_status7			: 4;
		volatile unsigned int csi2_pl_status8			: 4;
		volatile unsigned int csi2_pl_status9			: 4;
		volatile unsigned int csi2_pl_status10			: 4;
		volatile unsigned int csi2_pl_status11			: 4;
		volatile unsigned int reserved0					: 8;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_PL_STATUS711;

typedef union {
	struct {
		volatile unsigned int emb_data_type0			: 6;
		volatile unsigned int reserved0					: 2;
		volatile unsigned int emb_data_type1			: 6;
		volatile unsigned int reserved1					: 2;
		volatile unsigned int emb_line_cnt				: 4;
		volatile unsigned int reserved2					: 12;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_EMB_DTYPE_CTRL;

typedef union {
	struct {
		volatile unsigned int emb_data_16byte_en_0		: 9;
		volatile unsigned int reserved0					: 3;
		volatile unsigned int emb_data_line_sel_0		: 4;
		volatile unsigned int emb_data_16byte_en_1		: 9;
		volatile unsigned int reserved1 				: 3;
		volatile unsigned int emb_data_line_sel_1		: 4;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_EMB_DATA_CTRL01;

typedef union {
	struct {
		volatile unsigned int emb_data_16byte_en_2		: 9;
		volatile unsigned int reserved0					: 3;
		volatile unsigned int emb_data_line_sel_2		: 4;
		volatile unsigned int emb_data_16byte_en_3		: 9;
		volatile unsigned int reserved1 				: 3;
		volatile unsigned int emb_data_line_sel_3		: 4;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_EMB_DATA_CTRL23;

typedef union {
	struct {
		volatile unsigned int speed_idx 				: 8;
		volatile unsigned int reserved0 				: 24;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_TIF_CTRL;

typedef union {
	struct {
		volatile unsigned int state_tx_stop 			: 1;
		volatile unsigned int rx_hs 					: 2;
		volatile unsigned int contention_detection_en	: 1;
		volatile unsigned int band_crtl 				: 4;
		volatile unsigned int reserved0 				: 24;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_BAND_CTRL_DATA;

typedef union {
	struct {
		volatile unsigned int buf_mode					: 1;
		volatile unsigned int reserved0 				: 5;
		volatile unsigned int skip_t_clk_post			: 1;
		volatile unsigned int skip_t_clk_pre			: 1;
		volatile unsigned int reserved1 				: 24;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_TIF_CTRL_11;

typedef union {
	struct {
		volatile unsigned int test_data 				: 7;
		volatile unsigned int reserved0 				: 25;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_TIF_CTRL_13;

typedef union {
	struct {
		volatile unsigned int test_data 				: 7;
		volatile unsigned int reserved0 				: 25;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_TIF_CTRL_14;

typedef union {
	struct {
		volatile unsigned int hs_prepare_cnt			: 2;
		volatile unsigned int reserved0 				: 2;
		volatile unsigned int hs_tx_pdboverlap			: 1;
		volatile unsigned int reserved1 				: 27;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_TIF_CTRL_16;

typedef union {
	struct {
		volatile unsigned int rst_after_dt				: 1;
		volatile unsigned int rst_dur_sstate			: 1;
		volatile unsigned int reserved0 				: 30;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_TIF_0x25_CTRL;

typedef union {
	struct {
		volatile unsigned int tif_clr					: 1;
		volatile unsigned int reserved0 				: 31;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_TIF_CLR;

typedef union {
	struct {
		volatile unsigned int buf_mode					: 1;
		volatile unsigned int reserved0					: 5;
		volatile unsigned int skip_t_clk_post			: 1;
		volatile unsigned int skip_t_clk_pre			: 1;
		volatile unsigned int reserved1					: 24;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_DPHY_TIF_CTRL11;

typedef union {
	struct {
		volatile unsigned int speed						: 5;
		volatile unsigned int reserved0					: 27;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_DPHY_TIF_CTRL13;

typedef union {
	struct {
		volatile unsigned int testdat					: 7;
		volatile unsigned int reserved0					: 25;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_DPHY_TIF_CTRL14;

typedef union {
	struct {
		volatile unsigned int hsprepare_cnt				: 2;
		volatile unsigned int reserved0					: 2;
		volatile unsigned int hstx_pdboverlap			: 1;
		volatile unsigned int reserved1					: 27;
	} asbits;
	volatile unsigned int as32bits;
} CSS_MIPICSI_DPHY_TIF_CTRL16;

typedef struct {
	CSS_MIPICSI_CSI2_CTRL			csi2_ctrl;			/* 0x000 */
	CSS_MIPICSI_DPHY_CTRL			dphy_ctrl;			/* 0x004 */
	CSS_MIPICSI_VBLANK_SIZE 		vblank_size;		/* 0x008 */
	CSS_MIPICSI_VSTART_SIZE 		vstart_size;		/* 0x00C */
	CSS_MIPICSI_ERROR_STATE 		error_state;		/* 0x010 */
	CSS_MIPICSI_ESCAPE_MODE03		escape_mode03;		/* 0x014 */
	CSS_MIPICSI_ESCAPE_MODE47		escape_mode47;		/* 0x018 */
	CSS_MIPICSI_INTERRUPT03 		interrupt03;		/* 0x01C */
	CSS_MIPICSI_PL_STATUS03 		pl_status03;		/* 0x020 */
	CSS_MIPICSI_PL_STATUS46 		pl_status46;		/* 0x024 */
	CSS_MIPICSI_PL_STATUS711		pl_status711;		/* 0x028 */
	CSS_MIPICSI_EMB_DTYPE_CTRL		emb_data_type_ctrl;	/* 0x030 */
	CSS_MIPICSI_EMB_DATA_CTRL01 	emb_data_ctrl01;	/* 0x034 */
	CSS_MIPICSI_EMB_DATA_CTRL23 	emb_data_ctrl23;	/* 0x038 */
	CSS_MIPICSI_TIF_CTRL			tif_ctrl;			/* 0x040 */
	CSS_MIPICSI_TIF_CLR				tif_clear;			/* 0xC00 */
} CSS_MIPICSI_REG_DATA;

typedef struct {
	unsigned int lanecnt;
	unsigned int speed;
	unsigned int type;
	unsigned int syncpol;
	unsigned int delay;
} odin_mipicsi_config;

int odin_mipicsi_set_remote_control(int set_val);
void odin_mipicsi_reg_dump(void);
int odin_mipicsi_reg_update(css_mipi_csi_ch csi_ch);
unsigned int odin_mipicsi_get_error(css_mipi_csi_ch csi_ch);
int odin_mipicsi_status_clear(css_mipi_csi_ch csi_ch);
int odin_mipicsi_d_phy_reset_on(css_mipi_csi_ch csi_ch);
int odin_mipicsi_d_phy_reset_off(css_mipi_csi_ch csi_ch);
int odin_mipicsi_channel_rx_enable(css_mipi_csi_ch csi_ch, unsigned int lanecnt,
		unsigned int speed, unsigned int type, unsigned int syncpol,
		unsigned int delay, unsigned int hsrxmode);
int odin_mipicsi_channel_rx_disable(css_mipi_csi_ch csi_ch);
int odin_mipicsi_get_word_count(css_mipi_csi_ch csi_ch);
int odin_mipicsi_get_pixel_count(css_mipi_csi_ch csi_ch);
int odin_mipicsi_ctrl_continuous_mode(css_mipi_csi_ch csi_ch,
		unsigned int enable);
int odin_mipicsi_set_speed(css_mipi_csi_ch csi_ch, unsigned int speed);
int odin_mipicsi_wait_stable(css_mipi_csi_ch csi_ch, int in_width,
		int in_height, int trycnt);
int odin_mipicsi_interrupt_enable(css_mipi_csi_ch csi_ch, int int_mask);
int odin_mipicsi_interrupt_disable(css_mipi_csi_ch csi_ch, int int_mask);
int odin_mipicsi_get_hw_init_state(css_mipi_csi_ch csi_ch);
void odin_mipicsi_channel_check(void);

/* End of the Locally defined global data */
/***************************************************************************/

/* #define ODIN_MIPICSI_ERR_REPORT	1 */

#endif /* __ODIN_MIPICSI_H__ */
