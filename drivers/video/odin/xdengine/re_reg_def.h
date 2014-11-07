/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef RE_BASE_ADDR
#define RE_BASE_ADDR 0


#define ADDR_RE_DP_CTRL                                ( RE_BASE_ADDR + 0x020 )
#define ADDR_RE_CORING_CTRL                            ( RE_BASE_ADDR + 0x024 )
#define ADDR_RE_SP_CTRL_00                             ( RE_BASE_ADDR + 0x030 )
#define ADDR_RE_SP_CTRL_01                             ( RE_BASE_ADDR + 0x034 )
#define ADDR_RE_SP_CTRL_02                             ( RE_BASE_ADDR + 0x038 )
#define ADDR_RE_SP_CTRL_04                             ( RE_BASE_ADDR + 0x040 )
#define ADDR_RE_SP_CTRL_06                             ( RE_BASE_ADDR + 0x048 )
#define ADDR_RE_SP_CTRL_07                             ( RE_BASE_ADDR + 0x04C )
#define ADDR_RE_SP_CTRL_08                             ( RE_BASE_ADDR + 0x050 )
#define ADDR_RE_SP_CTRL_09                             ( RE_BASE_ADDR + 0x054 )
#define ADDR_RE_SP_CTRL_0A                             ( RE_BASE_ADDR + 0x058 )
#define ADDR_RE_SP_CTRL_0B                             ( RE_BASE_ADDR + 0x05C )
#define ADDR_RE_SP_CTRL_0C                             ( RE_BASE_ADDR + 0x060 )

/* re_dp_ctrl */
#define XD_SP_GAIN_E_B_START                           (6)
#define XD_SP_GAIN_E_B_END                             (0)
#define XD_SP_GAIN_E_W_START                           (14)
#define XD_SP_GAIN_E_W_END                             (8)
#define XD_SP_GAIN_T_B_START                           (22)
#define XD_SP_GAIN_T_B_END                             (16)
#define XD_SP_GAIN_T_W_START                           (30)
#define XD_SP_GAIN_T_W_END                             (24)

/* re_coring_ctrl */
#define XD_CORING_MODE_START                           (1)
#define XD_CORING_MODE_END                             (0)
#define XD_REG_CORING_EN_START                         (2)
#define XD_REG_CORING_EN_END                           (2)
#define XD_WEIGHT_A_START                              (22)
#define XD_WEIGHT_A_END                                (16)
#define XD_WEIGHT_T_START                              (29)
#define XD_WEIGHT_T_END                                (23)

/* re_sp_ctrl_00 */
#define XD_EDGE_ENHANCE_ENABLE_START                   (0)
#define XD_EDGE_ENHANCE_ENABLE_END                     (0)
#define XD_EDGE_OPERATOR_SELECTION_START               (13)
#define XD_EDGE_OPERATOR_SELECTION_END                 (12)

/* re_sp_ctrl_01 */
#define XD_WHITE_GAIN_START                            (6)
#define XD_WHITE_GAIN_END                              (0)
#define XD_BLACK_GAIN_START                            (14)
#define XD_BLACK_GAIN_END                              (8)
#define XD_HORIZONTAL_GAIN_START                       (23)
#define XD_HORIZONTAL_GAIN_END                         (16)
#define XD_VERTICAL_GAIN_START                         (31)
#define XD_VERTICAL_GAIN_END                           (24)

/* re_sp_ctrl_02 */
#define XD_SOBEL_WEIGHT_START                          (7)
#define XD_SOBEL_WEIGHT_END                            (0)
#define XD_LAPLACIAN_WEIGHT_START                      (15)
#define XD_LAPLACIAN_WEIGHT_END                        (8)
#define XD_SOBEL_MANUAL_MODE_EN_START                  (16)
#define XD_SOBEL_MANUAL_MODE_EN_END                    (16)
#define XD_SOBEL_MANUAL_GAIN_START                     (31)
#define XD_SOBEL_MANUAL_GAIN_END                       (24)

/* re_sp_ctrl_04 */
#define XD_DISPLAY_MODE_START                          (11)
#define XD_DISPLAY_MODE_END                            (8)
#define XD_GX_WEIGHT_MANUAL_EN_START                   (12)
#define XD_GX_WEIGHT_MANUAL_EN_END                     (12)
#define XD_GX_WEIGHT_MANUAL_GAIN_START                 (23)
#define XD_GX_WEIGHT_MANUAL_GAIN_END                   (16)

/* re_sp_ctrl_06 */
#define XD_LAP_H_MODE_START                            (1)
#define XD_LAP_H_MODE_END                              (0)
#define XD_LAP_V_MODE_START                            (4)
#define XD_LAP_V_MODE_END                              (4)

/* re_sp_ctrl_07 */
#define XD_GB_EN_START                                 (0)
#define XD_GB_EN_END                                   (0)
#define XD_GB_MODE_START                               (4)
#define XD_GB_MODE_END                                 (4)
#define XD_GB_X1_START                                 (15)
#define XD_GB_X1_END                                   (8)
#define XD_GB_Y1_START                                 (23)
#define XD_GB_Y1_END                                   (16)

/* re_sp_ctrl_08 */
#define XD_GB_X2_START                                 (7)
#define XD_GB_X2_END                                   (0)
#define XD_GB_Y2_START                                 (15)
#define XD_GB_Y2_END                                   (8)
#define XD_GB_Y3_START                                 (23)
#define XD_GB_Y3_END                                   (16)

/* re_sp_ctrl_09 */
#define XD_LUM1_X_L0_START                             (7)
#define XD_LUM1_X_L0_END                               (0)
#define XD_LUM1_X_L1_START                             (15)
#define XD_LUM1_X_L1_END                               (8)
#define XD_LUM1_X_H0_START                             (23)
#define XD_LUM1_X_H0_END                               (16)
#define XD_LUM1_X_H1_START                             (31)
#define XD_LUM1_X_H1_END                               (24)

/* re_sp_ctrl_0a */
#define XD_LUM1_Y0_START                               (7)
#define XD_LUM1_Y0_END                                 (0)
#define XD_LUM1_Y1_START                               (15)
#define XD_LUM1_Y1_END                                 (8)
#define XD_LUM1_Y2_START                               (23)
#define XD_LUM1_Y2_END                                 (16)
#define XD_LUM2_X_L0_START                             (31)
#define XD_LUM2_X_L0_END                               (24)

/* re_sp_ctrl_0b */
#define XD_LUM2_X_L1_START                             (7)
#define XD_LUM2_X_L1_END                               (0)
#define XD_LUM2_X_H0_START                             (15)
#define XD_LUM2_X_H0_END                               (8)
#define XD_LUM2_X_H1_START                             (23)
#define XD_LUM2_X_H1_END                               (16)
#define XD_LUM2_Y0_START                               (31)
#define XD_LUM2_Y0_END                                 (24)

/* re_sp_ctrl_0c */
#define XD_LUM2_Y1_START                               (7)
#define XD_LUM2_Y1_END                                 (0)
#define XD_LUM2_Y2_START                               (15)
#define XD_LUM2_Y2_END                                 (8)


#endif

