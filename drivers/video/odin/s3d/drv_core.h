/*
 * linux/drivers/video/odin/s3d/drv_core.h
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
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DRV_CORE_H__
#define __DRV_CORE_H__

#include "drv_customer.h"
#include "drv_mepsi.h"


/*-------------------------------------------------------------
  REGISTER MAP

   0x2000  S3D Formatter : S3D Formatter Register Map
---------------------------------------------------------------*/


#define OUTPUT_CON	__ADDR(FORMATTER_BASE,  0x004)
/*
[13] Test_EN1	R/W  1'b0  Color Bar output enable (for Test)    0/1
[12] Test_EN0	R/W  1'b0  Single color output enable (for Test)    0/1
[11] START_VPOL	R/W  1'b0  VSYNC Start polarity ( 0 : VSYNC rising edge,
							1 : VSYNC falling edge)    0/1
[10] START_MODE2	R/W  1'b0  VSYNC Start mode enable
	(0 : disable,  1 : enable)  It is valid
	only if FIFO_IF_EN = 1 and START_MODE = 1.    0/1
[9:8] DMA_OD_ORDER	R/W  2'b0  DMA Write Data Order  O  0 ~ 3
[7] DMA_OZ_POSITION	R/W  1'b0  DMA Write Data z(dummy byte) position
	( 0 : RGBz,  1 : zRGB)  O  0/1
[6:4] DMA_OD_FORMAT    R/W  3'h1  DMA Write Data Format  O  0 ~ 5
              000 : RGB888 Unpack
              001 : RGB888 Pack
              010 : RGB565
              011 : Reserved
              100 : YUV444 Unpack
              101 : YUV444 Pack
              Else : Reserved
[3]    FIFO_OD_FORMAT    R/W  1'b0  FIFO Interface data format
	( 0 : RGB24,  1 : YUV444)  O  0/1
[2]    START_MODE        R/W  1'b0  Frame start mode
	( 0 : single,  1 : periodic)    0/1
[1]    DMA_IF_EN         R/W  1'b0  DMA (write) Interface enable
	(0 : disable,  1 : enable)    0/1
[0]    FIFO_IF_EN        R/W  1'b0  FIFO Interface enable
	(0 : disable,  1 : enable)    0/1
*/
#define FRAME_ST	__ADDR(FORMATTER_BASE,  0x008)
/* [0]    frame_start       WO  1'b0  Frame start (auto cleared) 0/1 */
#define SHADOW_UP	__ADDR(FORMATTER_BASE,  0x010)
/* [0]    shadow_update     WO  1'h0  shadow regsiter update enable
	(auto cleared)    0/1 */
#define FB_RD_PAGE	__ADDR(FORMATTER_BASE,  0x014)
/* [3]    fb3_rpage	R/W  1'h0  designate frame buffer3 page to read 0/1
 [2]	fb2_rpage	R/W  1'h0  designate frame buffer2 page to read 0/1
 [1]	fb1_rpage	R/W  1'h0  designate frame buffer1 page to read 0/1
 [0]	fb0_rpage	R/W  1'h0  designate frame buffer0 page to read 0/1
*/
#define DE_FORMAT_CON   __ADDR(FORMATTER_BASE,  0x018)
/* [4:2] df_dst_type	R/W  3'b0  Deformatting destination type O  0 ~ 4
		0 : Side-by-Side
		1 : Top-bottom
		2 : pixel-base
		3 : line-base
		4 : Anaglyph
		Else : reserved
    [1]    df_src_type  R/W  1'b0  Deformatting source type  O  0/1
		0 : pixel base image
		1 : line base image
    [0]  deformat_en	R/W  1'b0  De-formatting enable
    	( 0 : disable, 1 : enable)  O 0/1
*/
#define ANAG_CMASK	__ADDR(FORMATTER_BASE,  0x01C)
/* [2] B_mask R/W  1'b0
	Blue color mask for Left image in anagyph output mode  O  0/1
  [1] G_mask R/W  1'b0
  	Green color mask for Left image in anaglyph output mode  O  0/1
  [0] R_mask R/W  1'b0
  	Red color mask for Left image in anagyph output mode  O  0/1
*/
#define S3D_CON	__ADDR(FORMATTER_BASE,  0x020)
/*	[4]  S3D_SRC_TYPE    R/W  1'b0 S3D Formatting source type O  0/1
		0 : Side-by-side image
		1 : Top-bottom image
	[3:1]  S3D_TYPE      R/W  3'h0  3'h0 : Pixel base O  0 ~ 4
		3'h1 : Sub-Pixel base
		3'h2 : Line base
		3'h3 : Frame switching (Time Division)
		3'h4 : Anaglyph
		Else : reserved
	[0] S3D_MODE_EN   R/W  1'h0  S3D mode enable
		( 0 : disable,  1 : enable)  O  0/1
*/
#define BG_COLOR_L	__ADDR(FORMATTER_BASE,  0x028)
/* [15:0] BG_Color_GB   R/W 16'h0000  Background color (Green/Blue)
	0 ~ 65535 */
#define BG_COLOR_H	__ADDR(FORMATTER_BASE,  0x02C)
/* [7:0]  BG_Color_R R/W  8'h00  Background color (Red) 0 ~ 255 */
#define BL_SEL	__ADDR(FORMATTER_BASE,  0x03C)
/*
	[9:8]  fb2_bl_sel    R/W  2'h2  Frame buffer2 read burst length  O  0 ~ 2
	[5:4]  fb1_bl_sel    R/W  2'h2  Frame buffer1 read burst length  O  0 ~ 2
	[1:0]  fb0_bl_sel    R/W  2'h2  Frame buffer0 read burst length  O  0 ~ 2
		2'h0 : 8 burst read
		2'h1 : 16 burst read
		2'h2 : 32 burst read
		2'h3 : Reserved
*/
#define FRAME_WIDTH     __ADDR(FORMATTER_BASE,  0x040)
/* [15:0] frame_width      R/W  13'h0000 frame width (total output size)
	O  32 ~ 4808	*/
#define FRAME_HEIGHT    __ADDR(FORMATTER_BASE,  0x044)
/* [15:0] frame_height     R/W  13'h0000 frame height (total output size)
	O  32 ~ 4808	*/
#define DMA_DST0_ADDR_L __ADDR(FORMATTER_BASE,  0x048)
/* [15:0] dma_dst0_addr_L  R/W  16'h0000 DMA write destination0 address[15:0]
	O  32'h0 ~ 32'hffff_ffff (should be multiple of 8) */
#define DMA_DST0_ADDR_H __ADDR(FORMATTER_BASE,  0x04C)
/* [15:0] dma_dst0_addr_H  R/W  16'h0000  DMA write destination0
		address[31:16]  O  */
#define DMA_DST1_ADDR_L __ADDR(FORMATTER_BASE,  0x050)
/* [15:0] dma_dst1_addr_L  R/W  16'h0000  DMA write destination1
		address[15:0]  O  32'h0 ~ 32'hffff_ffff (should be multiple of 8)*/
#define DMA_DST1_ADDR_H __ADDR(FORMATTER_BASE,  0x054)
/* [15:0] dma_dst1_addr_H  R/W  16'h0000 DMA write destination1
		address[31:16]  O  */
#define FRATE_CON_L     __ADDR(FORMATTER_BASE,  0x058)
/* [15:0] frate_con_L      R/W  16'h0000  Frame rate control[15:0]
	(1 frame period / op. clock period)  O  32'h0 ~ 32'hffff_ffff*/
#define FRATE_CON_H     __ADDR(FORMATTER_BASE,  0x05C)
/* [15:0] frate_con_H  R/W  16'h0000  Frame rate control[31:16]  O  */

#define FB0_PL0_ADDR0_L __ADDR(FORMATTER_BASE,  0x0C0)
/* [15:0]  FB0_PL0_ADDR0_L  R/W  16'h0000  Frame buffer0 plane0 base
	address [15:0] - page 0  O  32'h0 ~ 32'hffff_ffff (should be multiple of 8)*/
#define FB0_PL0_ADDR0_H __ADDR(FORMATTER_BASE,  0x0C4)
/* [15:0]  FB0_PL0_ADDR0_H  R/W  16'h0000  Frame buffer0 plane0 base
	address [31:16] - page 0  O  */
#define FB0_PL1_ADDR0_L __ADDR(FORMATTER_BASE,  0x0C8)
/*    [15:0]  FB0_PL1_ADDR0_L  R/W  16'h0000  Frame buffer0 plane1 base
	address [15:0] - page 0  O  32'h0 ~ 32'hffff_ffff (should be multiple of 8) */
#define FB0_PL1_ADDR0_H __ADDR(FORMATTER_BASE,  0x0CC)
/*    [15:0]  FB0_PL1_ADDR0_H  R/W  16'h0000  Frame buffer0 plane1 base
	address [31:16] - page 0  O   */
#define FB0_PL2_ADDR0_L __ADDR(FORMATTER_BASE,  0x0D0)
/*    [15:0]  FB0_PL2_ADDR0_L  R/W  16'h0000  Frame buffer0 plane2 base
	address [15:0] - page 0  O  32'h0 ~ 32'hffff_ffff (should be multiple of 8) */
#define FB0_PL2_ADDR0_H __ADDR(FORMATTER_BASE,  0x0D4)
/*    [15:0]  FB0_PL2_ADDR0_H  R/W  16'h0000  Frame buffer0 plane2 base
address [31:16] - page 0  O  */
#define FB0_PL0_ADDR1_L __ADDR(FORMATTER_BASE,  0x0D8)
/*    [15:0]  FB0_PL0_ADDR1_L  R/W  16'h0000  Frame buffer0 plane0 base
address [15:0] - page 1  O  32'h0 ~ 32'hffff_ffff (should be multiple of 8) */
#define FB0_PL0_ADDR1_H __ADDR(FORMATTER_BASE,  0x0DC)
/*    [15:0]  FB0_PL0_ADDR1_H  R/W  16'h0000  Frame buffer0 plane0 base
address [31:16] - page 1  O   */
#define FB0_PL1_ADDR1_L __ADDR(FORMATTER_BASE,  0x0E0)
/*    [15:0]  FB0_PL1_ADDR1_L  R/W  16'h0000  Frame buffer0 plane1 base
address [15:0] - page 1  O  32'h0 ~ 32'hffff_ffff (should be multiple of 8) */
#define FB0_PL1_ADDR1_H __ADDR(FORMATTER_BASE,  0x0E4)
/*    [15:0]  FB0_PL1_ADDR1_H  R/W  16'h0000  Frame buffer0 plane1 base
address [31:16] - page 1  O   */
#define FB0_PL2_ADDR1_L __ADDR(FORMATTER_BASE,  0x0E8)
/*    [15:0]  FB0_PL2_ADDR1_L  R/W  16'h0000  Frame buffer0 plane2 base
address [15:0] - page 1  O  32'h0 ~ 32'hffff_ffff (should be multiple of 8) */
#define FB0_PL2_ADDR1_H __ADDR(FORMATTER_BASE,  0x0EC)
/* [15:0]  FB0_PL2_ADDR1_H  R/W  16'h0000  Frame buffer0 plane2 base
address [31:16] - page 1  O   */
#define FB0_HSIZE       __ADDR(FORMATTER_BASE,  0x0F0)
/* [12:0]  FB0_HSIZE        R/W  13'h000  Frame buffer0 width (max. 4808)
	O  32 ~ 4808 */
#define FB0_VSIZE       __ADDR(FORMATTER_BASE,  0x0F4)
/* [12:0]  FB0_VSIZE        R/W  13'h000  Frame buffer0 height (max.4808)
	O  32 ~ 4808 */
#define FB0_DCNT_L      __ADDR(FORMATTER_BASE,  0x0F8)
/* [15:0]  FB0_DCNT_L       R/W  16'h0000  Frame buffer0 data count [15:0]
	O  0x400 ~ 0x160_bc40 */
#define FB0_DCNT_H      __ADDR(FORMATTER_BASE,  0x0FC)
/* [8:0]   FB0_DCNT_H       R/W  9'h000  Frame buffer0 data count [24:16] O*/
#define FB1_PL0_ADDR0_L __ADDR(FORMATTER_BASE,  0x100)
/*    [15:0]  FB1_PL0_ADDR0_L  R/W  16'h0000  Frame buffer1 plane0 base
	address [15:0] - page 0  O  32'h0 ~ 32'hffff_ffff (should be multiple of 8) */
#define FB1_PL0_ADDR0_H __ADDR(FORMATTER_BASE,  0x104)
/*    [15:0]  FB1_PL0_ADDR0_H  R/W  16'h0000  Frame buffer1 plane0 base
	address [31:16] - page 0  O   */
#define FB1_PL1_ADDR0_L __ADDR(FORMATTER_BASE,  0x108)
/*    [15:0]  FB1_PL1_ADDR0_L  R/W  16'h0000  Frame buffer1 plane1 base
	address [15:0] - page 0  O  32'h0 ~ 32'hffff_ffff (should be multiple of 8) */
#define FB1_PL1_ADDR0_H __ADDR(FORMATTER_BASE,  0x10C)
/*    [15:0]  FB1_PL1_ADDR0_H  R/W  16'h0000  Frame buffer1 plane1 base
	address [31:16] - page 0  O   */
#define FB1_PL2_ADDR0_L __ADDR(FORMATTER_BASE,  0x110)
/*    [15:0]  FB1_PL2_ADDR0_L  R/W  16'h0000  Frame buffer1 plane2 base
	address [15:0] - page 0  O  32'h0 ~ 32'hffff_ffff (should be multiple of 8) */
#define FB1_PL2_ADDR0_H __ADDR(FORMATTER_BASE,  0x114)
/*    [15:0]  FB1_PL2_ADDR0_H  R/W  16'h0000  Frame buffer1 plane2 base
	address [31:16] - page 0  O   */
#define FB1_PL0_ADDR1_L __ADDR(FORMATTER_BASE,  0x118)
/*    [15:0]  FB1_PL0_ADDR1_L  R/W  16'h0000  Frame buffer1 plane0 base
	address [15:0] - page 1  O  32'h0 ~ 32'hffff_ffff (should be multiple of 8) */
#define FB1_PL0_ADDR1_H __ADDR(FORMATTER_BASE,  0x11C)
/*    [15:0]  FB1_PL0_ADDR1_H  R/W  16'h0000  Frame buffer1 plane0 base
	address [31:16] - page 1  O   */
#define FB1_PL1_ADDR1_L __ADDR(FORMATTER_BASE,  0x120)
/*    [15:0]  FB1_PL1_ADDR1_L  R/W  16'h0000  Frame buffer1 plane1 base
	address [15:0] - page 1  O  32'h0 ~ 32'hffff_ffff (should be multiple of 8) */
#define FB1_PL1_ADDR1_H __ADDR(FORMATTER_BASE,  0x124)
/*    [15:0]  FB1_PL1_ADDR1_H  R/W  16'h0000  Frame buffer1 plane1 base
	address [31:16] - page 1  O   */
#define FB1_PL2_ADDR1_L __ADDR(FORMATTER_BASE,  0x128)
/*    [15:0]  FB1_PL2_ADDR1_L  R/W  16'h0000  Frame buffer1 plane2 base
	address [15:0] - page 1  O  32'h0 ~ 32'hffff_ffff (should be multiple of 8) */
#define FB1_PL2_ADDR1_H __ADDR(FORMATTER_BASE,  0x12C)
/*    [15:0]  FB1_PL2_ADDR1_H  R/W  16'h0000  Frame buffer1 plane2 base
	address [31:16] - page 1  O   */
#define FB1_HSIZE       __ADDR(FORMATTER_BASE,  0x130)
/*    [12:0]  FB1_HSIZE	R/W  13'h000  Frame buffer1 width (max.4808)
	O  32 ~ 4808 */
#define FB1_VSIZE       __ADDR(FORMATTER_BASE,  0x134)
/*    [12:0]  FB1_VSIZE 	R/W  13'h000  Frame buffer1 height (max.4808)
		O  32 ~ 4808 */
#define FB1_DCNT_L      __ADDR(FORMATTER_BASE,  0x138)
/*    [15:0]  FB1_DCNT_L	R/W  16'h0000  Frame buffer1 data count [15:0]
	O  0x400 ~ 0x160_bc40 */
#define FB1_DCNT_H      __ADDR(FORMATTER_BASE,  0x13C)
/*    [8:0]   FB1_DCNT_H	R/W  9'h000  Frame buffer1 data count [24:16]
	O   */
#define FB2_PL0_ADDR0_L __ADDR(FORMATTER_BASE,  0x140)
/*    [15:0]  FB2_PL0_ADDR0_L  R/W  16'h0000  Frame buffer2 plane0 base
	address [15:0] - page 0  O  32'h0 ~ 32'hffff_ffff (should be multiple of 8) */
#define FB2_PL0_ADDR0_H __ADDR(FORMATTER_BASE,  0x144)
/*    [15:0]  FB2_PL0_ADDR0_H  R/W  16'h0000  Frame buffer2 plane0 base
	address [31:16] - page 0  O   */
#define FB2_PL0_ADDR1_L __ADDR(FORMATTER_BASE,  0x148)
/*    [15:0]  FB2_PL0_ADDR1_L  R/W  16'h0000  Frame buffer2 plane0 base
	address [15:0] - page 1  O  32'h0 ~ 32'hffff_ffff (should be multiple of 8) */
#define FB2_PL0_ADDR1_H __ADDR(FORMATTER_BASE,  0x14C)
/*    [15:0]  FB2_PL0_ADDR1_H  R/W  16'h000  Frame buffer2 plane0 base
address [31:16] - page 1  O   */
#define FB2_HSIZE       __ADDR(FORMATTER_BASE,  0x150)
/*	[12:0]	FB2_HSIZE	R/W  13'h000  Frame buffer2 width	O 32 ~ 4808 */
#define FB2_VSIZE       __ADDR(FORMATTER_BASE,  0x154)
/*	[12:0]  FB2_VSIZE	R/W  13'h000  Frame buffer2 height  O  32 ~ 4808 */
#define FB2_DCNT_L      __ADDR(FORMATTER_BASE,  0x158)
/*	[15:0]  FB2_DCNT_L       R/W  16'h0000  Frame buffer2 data count [15:0]
	O  0x400 ~ 0x160_bc40 */
#define FB2_DCNT_H      __ADDR(FORMATTER_BASE,  0x15C)
/*	[8:0]	FB2_DCNT_H	R/W  9'h000  Frame buffer2 data count [24:16]	O   */
#define FB3_PL0_ADDR0_L __ADDR(FORMATTER_BASE,  0x160)
/*	[15:0]  FB3_PL0_ADDR0_L  R/W  16'h0000  Frame buffer3 plane0 base address
	[15:0] - page 0  O  32'h0 ~ 32'hffff_ffff (should be multiple of 8) */
#define FB3_PL0_ADDR0_H __ADDR(FORMATTER_BASE,  0x164)
/*	[15:0]  FB3_PL0_ADDR0_H  R/W  16'h000  Frame buffer3 plane0 base address
	[31:16] - page 0  O   */
#define FB3_PL0_ADDR1_L __ADDR(FORMATTER_BASE,  0x168)
/*	[15:0]  FB3_PL0_ADDR1_L  R/W  16'h0000  Frame buffer3 plane0 base address
	[15:0] - page 1  O  32'h0 ~ 32'hffff_ffff (should be multiple of 8) */
#define FB3_PL0_ADDR1_H __ADDR(FORMATTER_BASE,  0x16C)
/*	[15:0]  FB3_PL0_ADDR1_H  R/W  16'h000  Frame buffer3 plane0 base address
	[31:16] - page 1  O   */
#define FB3_HSIZE       __ADDR(FORMATTER_BASE,  0x170)
/*	[12:0]	FB3_HSIZE	R/W  13'h000  Frame buffer3 width	O	32 ~ 4808 */
#define FB3_VSIZE       __ADDR(FORMATTER_BASE,  0x174)
/*	[12:0]	FB3_VSIZE	R/W  13'h000  Frame buffer3 height	O  32 ~ 4808 */
#define FB3_DCNT_L      __ADDR(FORMATTER_BASE,  0x178)
/*	[15:0]  FB3_DCNT_L	R/W  16'h000  Frame buffer3 data count [15:0]
	O  0x400 ~ 0x160_bc40 */
#define FB3_DCNT_H      __ADDR(FORMATTER_BASE,  0x17C)
/*	[8:0]	FB3_DCNT_H	R/W  9'h000  Frame buffer3 data count [24:16]  O   */

#define WIN0_CON        __ADDR(FORMATTER_BASE,  0x180)
/* [13]    win0_packen      R/W  1'b0
		packing enable (1: 24b, 0 : 32b - include dummy)  O  0/1
	[12]    win0_z_position R/W  1'b0
		zero position (0 : xxxz, 1 : zxxx)  O  0/1
	[11]    win0_d_order	R/W  1'h0
		window0 input data order (0 : R or V first)  O  0/1
	[10:9]  win0Y420_mode	R/W  2'b0
		0/1/2/3 = IMC1,3 / IMC2,4 / YV / NV  O  0 ~ 3
	[8]     win0_Y422_mode	R/W  1'b0
		1 : Y first, 0 : U or V first along to win0_d_order  O  0/1
	[7:6]	win0_Y_FORMAT	R/W  2'b0
		0/1/2/3 = YUV444 / YUV422packed/YUV422planar/YUV420  O  0 ~ 3
	[5]	win0_RGB_16b	R/W  1'b0
		RGB565 enable (1:RGB565, 0 ; RGB888 or RGBz8888)  O  0/1
	[4]	win0_IY_EN	R/W  1'b0  YUV enable (0 : RGB 24 or 32b)  O  0/1
*/
#define WIN0_AB_CON     __ADDR(FORMATTER_BASE,  0x184)
/* [4]	win0_ab_sel      R/W  1'b0 window0 alpha blending
		source selection (valid only when win0_tp_en = 1)    0/1
	[3]	win0_bg_ab_en    R/W  1'h0 window0 alpha blending
		enable with BG color ( 0 : disable,  1 : enable )    0/1
	[2]	win0_tp_mode     R/W  1'h0
		window0 transparency mode    0/1
	[1]	win0_tp_en       R/W  1'h0
		window0 transparency enable ( 0 : disable,  1 : enable )  O  0/1
	[0]	win0_ab_en       R/W  1'h0
		window0 alpha blending enable  ( 0 : disable,  1 : enable )  O  0/1
*/
#define WIN0_XST        __ADDR(FORMATTER_BASE,  0x188)
/*	[12:0]  win0_xst         R/W  13'h0000  window0 x_start postion
	O  0 ~ 4776 */
#define WIN0_YST        __ADDR(FORMATTER_BASE,  0x18C)
/*	[12:0]  win0_yst         R/W  13'h0000  window0 y_start postion
	O  0 ~ 4776 */
#define WIN0_HSIZE      __ADDR(FORMATTER_BASE,  0x190)
/*	[12:0]  win0_hsize       R/W  13'h0000  window0 width  O  32 ~ 4808 */
#define WIN0_VSIZE      __ADDR(FORMATTER_BASE,  0x194)
/*	[12:0]  win0_vsize       R/W  13'h0000  window0 height  O  32 ~ 4808 */
#define WIN0_HCUT       __ADDR(FORMATTER_BASE,  0x198)
/*	[12:0]  win0_hcut        R/W  13'h0000  window0 crop x position
	in frame buffer  O  0 ~ 4776 */
#define WIN0_VCUT       __ADDR(FORMATTER_BASE,  0x19C)
/*	[12:0]  win0_vcut        R/W  13'h0000  window0 crop y position
	in frame buffer  O  0 ~ 4776 */
#define WIN1_CON        __ADDR(FORMATTER_BASE,  0x1A0)
/*    [13]    win1_packen      R/W  1'b0  packing enable
	(1: 24b, 0 : 32b - include dummy)  O  0/1
[12]    win1_z_position  R/W  1'b0  zero position (0 : xxxz, 1 : zxxx)  O 0/1
[11]    win1_d_order     R/W  1'h0  window1 input data order
		(0 : R or V first)  O  0/1
[10:9]  win1_Y420_mode   R/W  2'b0
		0/1/2/3 = IMC1,3 / IMC2,4 / YV / NV  O  0 ~ 3
[8]     win1_Y422_mode   R/W  1'b0
		1 : Y first, 0 : U or V first along to win0_d_order  O  0/1
[7:6]   win1_Y_FORMAT    R/W  2'b0
		0/1/2/3 = YUV444 / YUV422packed/YUV422planar/YUV420  O  0 ~ 3
[5]     win1_RGB_16b    R/W  1'b0  RGB565 enable
		(1:RGB565, 0 ; RGB888 or RGBz8888)  O  0/1
[4]     win1_IY_EN  R/W  1'b0  YUV enable (0 : RGB 24 or 32b)  O  0/1
*/
#define WIN1_AB_CON     __ADDR(FORMATTER_BASE,  0x1A4)
/*    [4]     win1_ab_sel  R/W  1'h0  window1 alpha blending source selection
	(valid only when win1_tp_en = 1)    0/1
	[3]	win1_bg_ab_en  R/W  1'h0  window1 alpha blending enable with BG color
		( 0 : disable,  1 : enable )    0/1
    [2]     win1_tp_mode  R/W  1'h0  window1 transparency mode    0/1
    [1]     win1_tp_en  R/W  1'h0  window1 transparency enable
    	( 0 : disable, 1 : enable )  O  0/1
    [0]     win1_ab_en  R/W  1'h0  window1 alpha blending enable
    	( 0 : disable,  1 : enable )  O  0/1
*/
#define WIN1_XST        __ADDR(FORMATTER_BASE,  0x1A8)
/*	[12:0]	win1_xst  R/W  13'h000  window1 x_start postion  O  0 ~ 4776 */
#define WIN1_YST        __ADDR(FORMATTER_BASE,  0x1AC)
/*    [12:0]  win1_yst  R/W  13'h000  window1 y_start postion  O  0 ~ 4776 */
#define WIN1_HSIZE      __ADDR(FORMATTER_BASE,  0x1B0)
/*    [12:0]  win1_hsize  R/W  13'h000  window1 width  O  32 ~ 4808 */
#define WIN1_VSIZE      __ADDR(FORMATTER_BASE,  0x1B4)
/*    [12:0]  win1_vsize  R/W  13'h000  window1 height  O  32 ~ 4808 */
#define WIN1_HCUT       __ADDR(FORMATTER_BASE,  0x1B8)
/*    [12:0]  win1_hcut  R/W  13'h000  window1 crop x position in frame buffer
	O  0 ~ 4776 */
#define WIN1_VCUT       __ADDR(FORMATTER_BASE,  0x1BC)
/*    [12:0]  win1_vcut  R/W  13'h000  window1 crop y position in frame buffer
	O  0 ~ 4776 */
#define WIN2_CON        __ADDR(FORMATTER_BASE,  0x1C0)
/*    [13]    win2_packen R/W  1'b0  packing enable
	(1: 24b, 0 : 32b - include dummy)  O  0/1
	[12] win2_z_position R/W  1'b0  zero position (0 : xxxz, 1 : zxxx) O  0/1
	[11] win2_d_order R/W  1'h0 window2 input data order (0 : R first) O  0/1
	[10:6]     Reserved
	[5]  win2_RGB_16b  R/W  1'b0
		RGB565 enable (1:RGB565, 0 ; RGB888 or RGBz8888)  O 0/1
	[4:0] Reserved     */
#define WIN2_AB_CON     __ADDR(FORMATTER_BASE,  0x1C4)
#define WIN2_XST        __ADDR(FORMATTER_BASE,  0x1C8)
#define WIN2_YST        __ADDR(FORMATTER_BASE,  0x1CC)
#define WIN2_HSIZE      __ADDR(FORMATTER_BASE,  0x1D0)
#define WIN2_VSIZE      __ADDR(FORMATTER_BASE,  0x1D4)
#define WIN2_HCUT       __ADDR(FORMATTER_BASE,  0x1D8)
#define WIN2_VCUT       __ADDR(FORMATTER_BASE,  0x1DC)
#define WIN3_CON        __ADDR(FORMATTER_BASE,  0x1E0)
#define WIN3_AB_CON     __ADDR(FORMATTER_BASE,  0x1E4)
#define WIN3_XST        __ADDR(FORMATTER_BASE,  0x1E8)
#define WIN3_YST        __ADDR(FORMATTER_BASE,  0x1EC)
#define WIN3_HSIZE      __ADDR(FORMATTER_BASE,  0x1F0)
#define WIN3_VSIZE      __ADDR(FORMATTER_BASE,  0x1F4)
#define WIN3_HCUT       __ADDR(FORMATTER_BASE,  0x1F8)
#define WIN3_VCUT       __ADDR(FORMATTER_BASE,  0x1FC)

#define WIN_EN          __ADDR(FORMATTER_BASE,  0x200)
#define WIN0_CKEY_L     __ADDR(FORMATTER_BASE,  0x204)
#define WIN0_CKEY_H     __ADDR(FORMATTER_BASE,  0x208)
#define WIN1_CKEY_L     __ADDR(FORMATTER_BASE,  0x20C)
#define WIN1_CKEY_H     __ADDR(FORMATTER_BASE,  0x210)
#define WIN2_CKEY_L     __ADDR(FORMATTER_BASE,  0x214)
#define WIN2_CKEY_H     __ADDR(FORMATTER_BASE,  0x218)
#define WIN3_CKEY_L     __ADDR(FORMATTER_BASE,  0x21C)
#define WIN3_CKEY_H     __ADDR(FORMATTER_BASE,  0x220)
#define WIN0_AB_VAL     __ADDR(FORMATTER_BASE,  0x228)
#define WIN1_AB_VAL     __ADDR(FORMATTER_BASE,  0x22C)
#define WIN2_AB_VAL     __ADDR(FORMATTER_BASE,  0x230)
#define WIN3_AB_VAL     __ADDR(FORMATTER_BASE,  0x234)
#define WIN_PRI         __ADDR(FORMATTER_BASE,  0x238)
#define WIN0_SCL_CON    __ADDR(FORMATTER_BASE,  0x240)
#define WIN0_TWS        __ADDR(FORMATTER_BASE,  0x244)
#define WIN0_THS        __ADDR(FORMATTER_BASE,  0x248)
#define WIN0_WS_WD      __ADDR(FORMATTER_BASE,  0x24C)
#define WIN0_HS_HD      __ADDR(FORMATTER_BASE,  0x250)
#define WIN0_X_OFF      __ADDR(FORMATTER_BASE,  0x254)
#define WIN0_Y_OFF      __ADDR(FORMATTER_BASE,  0x258)
#define WIN1_SCL_CON    __ADDR(FORMATTER_BASE,  0x260)
#define WIN1_TWS        __ADDR(FORMATTER_BASE,  0x264)
#define WIN1_THS        __ADDR(FORMATTER_BASE,  0x268)
#define WIN1_WS_WD      __ADDR(FORMATTER_BASE,  0x26C)
#define WIN1_HS_HD      __ADDR(FORMATTER_BASE,  0x270)
#define WIN1_X_OFF      __ADDR(FORMATTER_BASE,  0x274)
#define WIN1_Y_OFF      __ADDR(FORMATTER_BASE,  0x278)
#define WIN2_SCL_CON    __ADDR(FORMATTER_BASE,  0x280)
#define WIN2_TWS        __ADDR(FORMATTER_BASE,  0x284)
#define WIN2_THS        __ADDR(FORMATTER_BASE,  0x288)
#define WIN2_WS_WD      __ADDR(FORMATTER_BASE,  0x28C)
#define WIN2_HS_HD      __ADDR(FORMATTER_BASE,  0x290)
#define WIN2_X_OFF      __ADDR(FORMATTER_BASE,  0x294)
#define WIN2_Y_OFF      __ADDR(FORMATTER_BASE,  0x298)
#define WIN3_SCL_CON    __ADDR(FORMATTER_BASE,  0x2A0)
#define WIN3_TWS        __ADDR(FORMATTER_BASE,  0x2A4)
#define WIN3_THS        __ADDR(FORMATTER_BASE,  0x2A8)
#define WIN3_WS_WD      __ADDR(FORMATTER_BASE,  0x2AC)
#define WIN3_HS_HD      __ADDR(FORMATTER_BASE,  0x2B0)
#define WIN3_X_OFF      __ADDR(FORMATTER_BASE,  0x2B4)
#define WIN3_Y_OFF      __ADDR(FORMATTER_BASE,  0x2B8)
#define Y2R_R11_OFFSET  __ADDR(FORMATTER_BASE,  0x300)
#define Y2R_R12_OFFSET  __ADDR(FORMATTER_BASE,  0x304)
#define Y2R_R13_OFFSET  __ADDR(FORMATTER_BASE,  0x308)
#define Y2R_R14_OFFSET  __ADDR(FORMATTER_BASE,  0x30C)
#define Y2R_G12_OFFSET  __ADDR(FORMATTER_BASE,  0x314)
#define Y2R_G13_OFFSET  __ADDR(FORMATTER_BASE,  0x318)
#define Y2R_G14_OFFSET  __ADDR(FORMATTER_BASE,  0x31C)
#define Y2R_B12_OFFSET  __ADDR(FORMATTER_BASE,  0x324)
#define Y2R_B13_OFFSET  __ADDR(FORMATTER_BASE,  0x328)
#define Y2R_B14_OFFSET  __ADDR(FORMATTER_BASE,  0x32C)
#define Y2R_R11_COEF    __ADDR(FORMATTER_BASE,  0x330)
#define Y2R_R12_COEF    __ADDR(FORMATTER_BASE,  0x334)
#define Y2R_R13_COEF    __ADDR(FORMATTER_BASE,  0x338)
#define Y2R_G12_COEF    __ADDR(FORMATTER_BASE,  0x340)
#define Y2R_G13_COEF    __ADDR(FORMATTER_BASE,  0x344)
#define Y2R_B12_COEF    __ADDR(FORMATTER_BASE,  0x34C)
#define Y2R_B13_COEF    __ADDR(FORMATTER_BASE,  0x350)

#define R2Y_Y_RCOEF     __ADDR(FORMATTER_BASE,  0x354)
#define R2Y_Y_GCOEF     __ADDR(FORMATTER_BASE,  0x358)
#define R2Y_Y_BCOEF     __ADDR(FORMATTER_BASE,  0x35C)
#define R2Y_CB_RCOEF    __ADDR(FORMATTER_BASE,  0x360)
#define R2Y_CB_GCOEF    __ADDR(FORMATTER_BASE,  0x364)
#define R2Y_CB_BCOEF    __ADDR(FORMATTER_BASE,  0x368)
#define R2Y_CR_RCOEF    __ADDR(FORMATTER_BASE,  0x36C)
#define R2Y_CR_GCOEF    __ADDR(FORMATTER_BASE,  0x370)
#define R2Y_CR_BCOEF    __ADDR(FORMATTER_BASE,  0x374)
#define R2Y_Y_COEF_OFFSET __ADDR(FORMATTER_BASE,  0x378)
#define R2Y_C_COEF_OFFSET __ADDR(FORMATTER_BASE,  0x37C)


/*---------------------------------------------------------
  DATA TYPE & STRUCT
----------------------------------------------------------- */

struct drv_out_format_info {
	enum mepsi_img_format format;

	uint16 d_order;
	uint16 z_position;
	uint16 d_format;
};


struct drv_in_format_info {
	enum mepsi_img_format format;

	uint16 pack_en;      /* 1: 24b, 0 : 32b */
	uint16 z_position;   /* 0 : xxxz, 1 : zxxx */
	uint16 d_order;      /* 0 : r or v first */
	uint16 rgb16_en;     /* 1:rgb565, 0 ; rgb888 or rgbz8888 */
	uint16 y420_mode;    /* 0/1/2/3 = imc1,3 / imc2,4 / yv / nv */
	uint16 y422_mode;    /* 1 : y first, 0 : u or v first */
	uint16 y_format;     /* 0/1/2/3 = yuv444 /
						yuv422packed/yuv422planar/yuv420 */
	uint16 y_en;         /* 0 : rgb 24 or 32b */
};


struct drv_core_color_info {
	uint16 r11_offset;
	uint16 r12_offset;
	uint16 r13_offset;
	uint16 r14_offset;
	uint16 g12_offset;
	uint16 g13_offset;
	uint16 g14_offset;
	uint16 b12_offset;
	uint16 b13_offset;
	uint16 b14_offset;
	uint16 r11coef;
	uint16 r12coef;
	uint16 r13coef;
	uint16 g12coef;
	uint16 g13coef;
	uint16 b12coef;
	uint16 b13coef;
	uint16 y_rcoef;
	uint16 y_gcoef;
	uint16 y_bcoef;
	uint16 cb_rcoef;
	uint16 cb_gcoef;
	uint16 cb_bcoef;
	uint16 cr_rcoef;
	uint16 cr_gcoef;
	uint16 cr_bcoef;
	uint16 y_coef_offset;
	uint16 c_coef_offset;
};



struct drv_core_info {
  uint16 init;

  uint16 test_en1          ;
  uint16 test_en0          ;
  uint16 start_vpol        ;
  uint16 start_mode2       ;
  uint16 dma_od_order      ;
  uint16 dma_oz_position   ;
  uint16 dma_od_format     ;
  uint16 fifo_od_format    ;
  uint16 start_mode        ;
  uint16 dma_if_en         ;
  uint16 fifo_if_en        ;
  uint16 frame_start       ;
  uint16 shadow_update     ;
  uint16 fb3_rpage         ;
  uint16 fb2_rpage         ;
  uint16 fb1_rpage         ;
  uint16 fb0_rpage         ;
  uint16 df_dst_type       ;
  uint16 df_src_type       ;
  uint16 deformat_en       ;
  uint16 b_mask            ;
  uint16 g_mask            ;
  uint16 r_mask            ;
  uint16 s3d_src_type      ;
  uint16 s3d_type          ;
  uint16 s3d_mode_en       ;
  uint32 bg_color          ;
  uint16 fb2_bl_sel        ;
  uint16 fb1_bl_sel        ;
  uint16 fb0_bl_sel        ;
  uint16 frame_width       ;
  uint16 frame_height      ;
  uint32 dma_dst0_addr     ;
  uint32 dma_dst1_addr     ;
  uint32 frate_con         ;
  uint32 fb0_pl0_addr0     ;
  uint32 fb0_pl1_addr0     ;
  uint32 fb0_pl2_addr0     ;
  uint32 fb0_pl0_addr1     ;
  uint32 fb0_pl1_addr1     ;
  uint32 fb0_pl2_addr1     ;
  uint16 fb0_hsize         ;
  uint16 fb0_vsize         ;
  uint32 fb0_dcnt          ;
  uint32 fb1_pl0_addr0     ;
  uint32 fb1_pl1_addr0     ;
  uint32 fb1_pl2_addr0     ;
  uint32 fb1_pl0_addr1     ;
  uint32 fb1_pl1_addr1     ;
  uint32 fb1_pl2_addr1     ;
  uint16 fb1_hsize         ;
  uint16 fb1_vsize         ;
  uint32 fb1_dcnt          ;
  uint32 fb2_pl0_addr0     ;
  uint32 fb2_pl0_addr1     ;
  uint16 fb2_hsize         ;
  uint16 fb2_vsize         ;
  uint32 fb2_dcnt          ;
  uint32 fb3_pl0_addr0     ;
  uint32 fb3_pl0_addr1     ;
  uint16 fb3_hsize         ;
  uint16 fb3_vsize         ;
  uint32 fb3_dcnt          ;

  uint16 win0_packen       ;
  uint16 win0_z_position   ;
  uint16 win0_d_order      ;
  uint16 win0_y420_mode    ;
  uint16 win0_y422_mode    ;
  uint16 win0_y_format     ;
  uint16 win0_rgb_16b      ;
  uint16 win0_iy_en        ;
  uint16 win0_ab_sel       ;
  uint16 win0_bg_ab_en     ;
  uint16 win0_tp_mode      ;
  uint16 win0_tp_en        ;
  uint16 win0_ab_en        ;
  uint16 win0_xst          ;
  uint16 win0_yst          ;
  uint16 win0_hsize        ;
  uint16 win0_vsize        ;
  uint16 win0_hcut         ;
  uint16 win0_vcut         ;
  uint16 win1_packen       ;
  uint16 win1_z_position   ;
  uint16 win1_d_order      ;
  uint16 win1_y420_mode    ;
  uint16 win1_y422_mode    ;
  uint16 win1_y_format     ;
  uint16 win1_rgb_16b      ;
  uint16 win1_iy_en        ;
  uint16 win1_ab_sel       ;
  uint16 win1_bg_ab_en     ;
  uint16 win1_tp_mode      ;
  uint16 win1_tp_en        ;
  uint16 win1_ab_en        ;
  uint16 win1_xst          ;
  uint16 win1_yst          ;
  uint16 win1_hsize        ;
  uint16 win1_vsize        ;
  uint16 win1_hcut         ;
  uint16 win1_vcut         ;
  uint16 win2_packen       ;
  uint16 win2_z_position   ;
  uint16 win2_d_order      ;
  uint16 win2_rgb_16b      ;
  uint16 win2_ab_sel       ;
  uint16 win2_bg_ab_en     ;
  uint16 win2_tp_mode      ;
  uint16 win2_tp_en        ;
  uint16 win2_ab_en        ;
  uint16 win2_xst          ;
  uint16 win2_yst          ;
  uint16 win2_hsize        ;
  uint16 win2_vsize        ;
  uint16 win2_hcut         ;
  uint16 win2_vcut         ;
  uint16 win3_packen       ;
  uint16 win3_z_position   ;
  uint16 win3_d_order      ;
  uint16 win3_rgb_16b      ;
  uint16 win3_ab_sel       ;
  uint16 win3_bg_ab_en     ;
  uint16 win3_tp_mode      ;
  uint16 win3_tp_en        ;
  uint16 win3_ab_en        ;
  uint16 win3_xst          ;
  uint16 win3_yst          ;
  uint16 win3_hsize        ;
  uint16 win3_vsize        ;
  uint16 win3_hcut         ;
  uint16 win3_vcut         ;
  uint16 win3_en           ;
  uint16 win2_en           ;
  uint16 win1_en           ;
  uint16 win0_en           ;
  uint32 win0_color_key    ;
  uint32 win1_color_key    ;
  uint32 win2_color_key    ;
  uint32 win3_color_key    ;
  uint16 win0_ab_val       ;
  uint16 win1_ab_val       ;
  uint16 win2_ab_val       ;
  uint16 win3_ab_val       ;
  uint16 win3_pri          ;
  uint16 win2_pri          ;
  uint16 win1_pri          ;
  uint16 win0_pri          ;
  uint16 win0_fil_more     ;
  uint16 win0_fil_en_v     ;
  uint16 win0_fil_en_h ;
  uint16 win0_scl_en   ;
  uint16 win0_tws      ;
  uint16 win0_ths      ;
  uint16 win0_ws_wd    ;
  uint16 win0_hs_hd    ;
  uint16 win0_x_off    ;
  uint16 win0_y_off    ;
  uint16 win1_fil_more ;
  uint16 win1_fil_en_v ;
  uint16 win1_fil_en_h ;
  uint16 win1_scl_en   ;
  uint16 win1_tws      ;
  uint16 win1_ths      ;
  uint16 win1_ws_wd    ;
  uint16 win1_hs_hd    ;
  uint16 win1_x_off    ;
  uint16 win1_y_off    ;
  uint16 win2_fil_more;
  uint16 win2_fil_en_v ;
  uint16 win2_fil_en_h ;
  uint16 win2_scl_en   ;
  uint16 win2_tws      ;
  uint16 win2_ths      ;
  uint16 win2_ws_wd    ;
  uint16 win2_hs_hd    ;
  uint16 win2_x_off    ;
  uint16 win2_y_off    ;
  uint16 win3_fil_more ;
  uint16 win3_fil_en_v ;
  uint16 win3_fil_en_h ;
  uint16 win3_scl_en   ;
  uint16 win3_tws      ;
  uint16 win3_ths      ;
  uint16 win3_ws_wd    ;
  uint16 win3_hs_hd    ;
  uint16 win3_x_off    ;
  uint16 win3_y_off    ;

  struct drv_core_color_info color;
};


/*	EXTERNAL FUNCTION	*/
int32 drv_core_init_parm  (int32 parm, void* parm1, struct drv_core_info *info);
int32 drv_core_init (int32 parm, void* parm1, struct drv_core_info *info);


int32 drv_register_update   (int32 parm, void* parm1, void* parm2);
int32 drv_screen_update     (int32 parm, void* parm1, void* parm2);

#endif
