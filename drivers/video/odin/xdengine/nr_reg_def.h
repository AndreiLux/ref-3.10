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


#ifndef NR_BASE_ADDR
#define NR_BASE_ADDR 0


#define ADDR_WIN_CTRL_00                               ( NR_BASE_ADDR + 0x000 )
#define ADDR_WIN_CTRL_01                               ( NR_BASE_ADDR + 0x004 )
#define ADDR_WIN_CTRL_02                               ( NR_BASE_ADDR + 0x008 )
#define ADDR_WIN_CTRL_03                               ( NR_BASE_ADDR + 0x00C )
#define ADDR_VFLT_CTRL_00                              ( NR_BASE_ADDR + 0x010 )
#define ADDR_MNR_CTRL_00                               ( NR_BASE_ADDR + 0x030 )
#define ADDR_MNR_CTRL_01                               ( NR_BASE_ADDR + 0x034 )
#define ADDR_MNR_CTRL_02                               ( NR_BASE_ADDR + 0x038 )
#define ADDR_MNR_CTRL_03                               ( NR_BASE_ADDR + 0x03C )
#define ADDR_MNR_CTRL_04                               ( NR_BASE_ADDR + 0x040 )
#define ADDR_MNR_CTRL_05                               ( NR_BASE_ADDR + 0x044 )
#define ADDR_MNR_CTRL_06                               ( NR_BASE_ADDR + 0x048 )
#define ADDR_BNR_DC_CTRL_00                            ( NR_BASE_ADDR + 0x050 )
#define ADDR_BNR_DC_CTRL_01                            ( NR_BASE_ADDR + 0x054 )
#define ADDR_BNR_DC_CTRL_02                            ( NR_BASE_ADDR + 0x058 )
#define ADDR_BNR_DC_CTRL_03                            ( NR_BASE_ADDR + 0x05C )
#define ADDR_BNR_AC_CTRL_00                            ( NR_BASE_ADDR + 0x060 )
#define ADDR_BNR_AC_CTRL_01                            ( NR_BASE_ADDR + 0x064 )
#define ADDR_BNR_AC_CTRL_02                            ( NR_BASE_ADDR + 0x068 )
#define ADDR_BNR_AC_CTRL_03                            ( NR_BASE_ADDR + 0x06C )
#define ADDR_BNR_AC_CTRL_04                            ( NR_BASE_ADDR + 0x070 )
#define ADDR_BNR_AC_CTRL_05                            ( NR_BASE_ADDR + 0x074 )
#define ADDR_BNR_AC_CTRL_06                            ( NR_BASE_ADDR + 0x078 )
#define ADDR_DNR_MAX_CTRL                              ( NR_BASE_ADDR + 0x080 )
#define ADDR_DER_CTRL_00                               ( NR_BASE_ADDR + 0x090 )
#define ADDR_DER_CTRL_01                               ( NR_BASE_ADDR + 0x094 )
#define ADDR_CNR_CTRL                                  ( NR_BASE_ADDR + 0x0A4 )
#define ADDR_CNR_CB_DIFF                               ( NR_BASE_ADDR + 0x0A8 )
#define ADDR_CNR_CB_CTRL                               ( NR_BASE_ADDR + 0x0AC )
#define ADDR_CNR_CB_DIFF_LOW                           ( NR_BASE_ADDR + 0x0B0 )
#define ADDR_CNR_CB_CTRL_LOW                           ( NR_BASE_ADDR + 0x0B4 )
#define ADDR_CNR_CR_DIFF                               ( NR_BASE_ADDR + 0x0B8 )
#define ADDR_CNR_CR_CTRL                               ( NR_BASE_ADDR + 0x0BC )
#define ADDR_CNR_CR_DIFF_LOW                           ( NR_BASE_ADDR + 0x0C0 )
#define ADDR_CNR_CR_CTRL_LOW                           ( NR_BASE_ADDR + 0x0C4 )

/* win_ctrl_00 */
#define XD_WIN0_X0_START                               (10)
#define XD_WIN0_X0_END                                 (0)
#define XD_WIN0_Y0_START                               (26)
#define XD_WIN0_Y0_END                                 (16)

/* win_ctrl_01 */
#define XD_WIN1_X1_START                               (10)
#define XD_WIN1_X1_END                                 (0)
#define XD_WIN1_Y1_START                               (26)
#define XD_WIN1_Y1_END                                 (16)

/* win_ctrl_02 */
#define XD_WIN2_X0_START                               (10)
#define XD_WIN2_X0_END                                 (0)
#define XD_WIN2_Y0_START                               (27)
#define XD_WIN2_Y0_END                                 (16)
#define XD_AC_BNR_FEATURE_CAL_MODE_START               (29)
#define XD_AC_BNR_FEATURE_CAL_MODE_END                 (29)

/* win_ctrl_03 */
#define XD_WIN3_X1_START                               (10)
#define XD_WIN3_X1_END                                 (0)
#define XD_WIN3_Y1_START                               (27)
#define XD_WIN3_Y1_END                                 (16)

/* vflt_ctrl_00 */
#define XD_VFILTERENABLE_START                         (0)
#define XD_VFILTERENABLE_END                           (0)

/* mnr_ctrl_00 */
#define XD_MNR_ENABLE_START                            (0)
#define XD_MNR_ENABLE_END                              (0)
#define XD_EDGE_GAIN_MAPPING_ENABLE_START              (1)
#define XD_EDGE_GAIN_MAPPING_ENABLE_END                (1)

/* mnr_ctrl_01 */
#define XD_HCOEF_00_START                              (19)
#define XD_HCOEF_00_END                                (16)
#define XD_HCOEF_01_START                              (23)
#define XD_HCOEF_01_END                                (20)
#define XD_HCOEF_02_START                              (27)
#define XD_HCOEF_02_END                                (24)
#define XD_HCOEF_03_START                              (31)
#define XD_HCOEF_03_END                                (28)

/* mnr_ctrl_02 */
#define XD_HCOEF_04_START                              (3)
#define XD_HCOEF_04_END                                (0)
#define XD_HCOEF_05_START                              (7)
#define XD_HCOEF_05_END                                (4)
#define XD_HCOEF_06_START                              (11)
#define XD_HCOEF_06_END                                (8)
#define XD_HCOEF_07_START                              (15)
#define XD_HCOEF_07_END                                (12)
#define XD_HCOEF_08_START                              (19)
#define XD_HCOEF_08_END                                (16)
#define XD_HCOEF_09_START                              (23)
#define XD_HCOEF_09_END                                (20)
#define XD_HCOEF_10_START                              (27)
#define XD_HCOEF_10_END                                (24)
#define XD_HCOEF_11_START                              (31)
#define XD_HCOEF_11_END                                (28)

/* mnr_ctrl_03 */
#define XD_HCOEF_12_START                              (3)
#define XD_HCOEF_12_END                                (0)
#define XD_HCOEF_13_START                              (7)
#define XD_HCOEF_13_END                                (4)
#define XD_HCOEF_14_START                              (11)
#define XD_HCOEF_14_END                                (8)
#define XD_HCOEF_15_START                              (15)
#define XD_HCOEF_15_END                                (12)
#define XD_HCOEF_16_START                              (19)
#define XD_HCOEF_16_END                                (16)
#define XD_X1_POSITION_START                           (31)
#define XD_X1_POSITION_END                             (24)

/* mnr_ctrl_04 */
#define XD_X2_POSITION_START                           (7)
#define XD_X2_POSITION_END                             (0)
#define XD_X3_POSITION_START                           (15)
#define XD_X3_POSITION_END                             (8)
#define XD_X4_POSITION_START                           (23)
#define XD_X4_POSITION_END                             (16)
#define XD_Y1_POSITION_START                           (31)
#define XD_Y1_POSITION_END                             (24)

/* mnr_ctrl_05 */
#define XD_Y2_POSITION_START                           (7)
#define XD_Y2_POSITION_END                             (0)
#define XD_Y3_POSITION_START                           (15)
#define XD_Y3_POSITION_END                             (8)
#define XD_Y4_POSITION_START                           (23)
#define XD_Y4_POSITION_END                             (16)
#define XD_FILTER_THRESHOLD_START                      (31)
#define XD_FILTER_THRESHOLD_END                        (24)

/* mnr_ctrl_06 */
#define XD_VCOEF0_START                                (3)
#define XD_VCOEF0_END                                  (0)
#define XD_VCOEF1_START                                (7)
#define XD_VCOEF1_END                                  (4)
#define XD_VCOEF2_START                                (11)
#define XD_VCOEF2_END                                  (8)
#define XD_VCOEF3_START                                (15)
#define XD_VCOEF3_END                                  (12)
#define XD_VCOEF4_START                                (19)
#define XD_VCOEF4_END                                  (16)
#define XD_VCOEF5_START                                (23)
#define XD_VCOEF5_END                                  (20)
#define XD_VCOEF6_START                                (27)
#define XD_VCOEF6_END                                  (24)

/* bnr_dc_ctrl_00 */
#define XD_DC_BNR_ENABLE_START                         (0)
#define XD_DC_BNR_ENABLE_END                           (0)
#define XD_ADAPTIVE_MODE_START                         (1)
#define XD_ADAPTIVE_MODE_END                           (1)
#define XD_OUTPUT_MUX_START                            (7)
#define XD_OUTPUT_MUX_END                              (5)
#define XD_DC_OFFSET_START                             (23)
#define XD_DC_OFFSET_END                               (16)
#define XD_DC_GAIN_START                               (31)
#define XD_DC_GAIN_END                                 (24)

/* bnr_dc_ctrl_01 */
#define XD_BLUR_ENABLE_START                           (1)
#define XD_BLUR_ENABLE_END                             (0)
#define XD_MUL2BLK_START                               (31)
#define XD_MUL2BLK_END                                 (28)

/* bnr_dc_ctrl_02 */
#define XD_DC_BNR_GAIN_CTRL_Y2_START                   (7)
#define XD_DC_BNR_GAIN_CTRL_Y2_END                     (0)
#define XD_DC_BNR_GAIN_CTRL_X2_START                   (15)
#define XD_DC_BNR_GAIN_CTRL_X2_END                     (8)
#define XD_DC_BNR_GAIN_CTRL_Y3_START                   (23)
#define XD_DC_BNR_GAIN_CTRL_Y3_END                     (16)
#define XD_DC_BNR_GAIN_CTRL_X3_START                   (31)
#define XD_DC_BNR_GAIN_CTRL_X3_END                     (24)

/* bnr_dc_ctrl_03 */
#define XD_DC_BNR_GAIN_CTRL_Y0_START                   (7)
#define XD_DC_BNR_GAIN_CTRL_Y0_END                     (0)
#define XD_DC_BNR_GAIN_CTRL_X0_START                   (15)
#define XD_DC_BNR_GAIN_CTRL_X0_END                     (8)
#define XD_DC_BNR_GAIN_CTRL_Y1_START                   (23)
#define XD_DC_BNR_GAIN_CTRL_Y1_END                     (16)
#define XD_DC_BNR_GAIN_CTRL_X1_START                   (31)
#define XD_DC_BNR_GAIN_CTRL_X1_END                     (24)

/* bnr_ac_ctrl_00 */
#define XD_BNR_H_EN_START                              (0)
#define XD_BNR_H_EN_END                                (0)
#define XD_BNR_V_EN_START                              (1)
#define XD_BNR_V_EN_END                                (1)
#define XD_STATUS_READ_TYPE_START                      (5)
#define XD_STATUS_READ_TYPE_END                        (5)
#define XD_STATUS_READ_MODE_START                      (7)
#define XD_STATUS_READ_MODE_END                        (6)
#define XD_HBMAX_GAIN_START                            (11)
#define XD_HBMAX_GAIN_END                              (8)
#define XD_VBMAX_GAIN_START                            (15)
#define XD_VBMAX_GAIN_END                              (12)
#define XD_STRENGTH_RESOLUTION_START                   (16)
#define XD_STRENGTH_RESOLUTION_END                     (16)
#define XD_FITER_TYPE_START                            (20)
#define XD_FITER_TYPE_END                              (20)
#define XD_BNR_OUTPUT_MUX_START                        (25)
#define XD_BNR_OUTPUT_MUX_END                          (24)

/* bnr_ac_ctrl_01 */
#define XD_STRENGTH_H_X0_START                         (7)
#define XD_STRENGTH_H_X0_END                           (0)
#define XD_STRENGTH_H_X1_START                         (15)
#define XD_STRENGTH_H_X1_END                           (8)
#define XD_STRENGTH_H_MAX_START                        (23)
#define XD_STRENGTH_H_MAX_END                          (16)
#define XD_AC_MIN_TH_START                         (31)
#define XD_AC_MIN_TH_END                           (24)

/* bnr_ac_ctrl_02 */
#define XD_STRENGTH_V_X0_START                         (7)
#define XD_STRENGTH_V_X0_END                           (0)
#define XD_STRENGTH_V_X1_START                         (15)
#define XD_STRENGTH_V_X1_END                           (8)
#define XD_STRENGTH_V_MAX_START                        (23)
#define XD_STRENGTH_V_MAX_END                          (16)

/* bnr_ac_ctrl_03 */
#define XD_H_OFFSET_MODE_START                         (0)
#define XD_H_OFFSET_MODE_END                           (0)
#define XD_MANUAL_OFFSET_H_VALUE_START                 (3)
#define XD_MANUAL_OFFSET_H_VALUE_END                   (1)
#define XD_V_OFFSET_MODE_START                         (4)
#define XD_V_OFFSET_MODE_END                           (4)
#define XD_MANUAL_OFFSET_V_VALUE_START                 (7)
#define XD_MANUAL_OFFSET_V_VALUE_END                   (5)
#define XD_OFFSET_USE_OF_HYSTERISIS_START              (11)
#define XD_OFFSET_USE_OF_HYSTERISIS_END                (8)
#define XD_T_FILTER_WEIGHT_START                       (23)
#define XD_T_FILTER_WEIGHT_END                         (16)

/* bnr_ac_ctrl_04 */
#define XD_MAX_DELTA_TH0_START                         (7)
#define XD_MAX_DELTA_TH0_END                           (0)
#define XD_MAX_DELTA_TH1_START                         (15)
#define XD_MAX_DELTA_TH1_END                           (8)
#define XD_H_BLOCKNESS_TH_START                        (23)
#define XD_H_BLOCKNESS_TH_END                          (16)
#define XD_H_WEIGHT_MAX_START                          (31)
#define XD_H_WEIGHT_MAX_END                            (24)

/* bnr_ac_ctrl_05 */
#define XD_SCALED_USE_OF_HYSTERISIS_START              (3)
#define XD_SCALED_USE_OF_HYSTERISIS_END                (0)

/* bnr_ac_ctrl_06 */
#define XD_AC_BNR_PROTECT_LVL_2_START                  (23)
#define XD_AC_BNR_PROTECT_LVL_2_END                    (16)
#define XD_AC_BNR_PROTECT_LVL_1_START                  (31)
#define XD_AC_BNR_PROTECT_LVL_1_END                    (24)

/* dnr_max_ctrl */
#define XD_ENABLE_MNR_START                            (1)
#define XD_ENABLE_MNR_END                              (1)
#define XD_ENABLE_DER_START                            (2)
#define XD_ENABLE_DER_END                              (2)
#define XD_ENABLE_DC_BNR_START                         (3)
#define XD_ENABLE_DC_BNR_END                           (3)
#define XD_ENABLE_AC_BNR_START                         (4)
#define XD_ENABLE_AC_BNR_END                           (4)
#define XD_DEBUG_ENABLE_START                          (8)
#define XD_DEBUG_ENABLE_END                            (8)
#define XD_DEBUG_MODE_START                            (9)
#define XD_DEBUG_MODE_END                              (9)
#define XD_DEBUG_MNR_ENABLE_START                      (11)
#define XD_DEBUG_MNR_ENABLE_END                        (11)
#define XD_DEBUG_DER_ENABLE_START                      (12)
#define XD_DEBUG_DER_ENABLE_END                        (12)
#define XD_DEBUG_DC_BNR_ENABLE_START                   (13)
#define XD_DEBUG_DC_BNR_ENABLE_END                     (13)
#define XD_DEBUG_AC_BNR_ENABLE_START                   (14)
#define XD_DEBUG_AC_BNR_ENABLE_END                     (14)
#define XD_WIN_CONTROL_ENABLE_START                    (16)
#define XD_WIN_CONTROL_ENABLE_END                      (16)
#define XD_BORDER_ENABLE_START                         (17)
#define XD_BORDER_ENABLE_END                           (17)
#define XD_WIN_INOUT_START                             (18)
#define XD_WIN_INOUT_END                               (18)

/* der_ctrl_00 */
#define XD_DER_EN_START                                (0)
#define XD_DER_EN_END                                  (0)
#define XD_DER_GAIN_MAPPING_START                      (2)
#define XD_DER_GAIN_MAPPING_END                        (2)
#define XD_DER_BIF_EN_START                            (3)
#define XD_DER_BIF_EN_END                              (3)
#define XD_BIF_TH_START                                (15)
#define XD_BIF_TH_END                                  (8)

/* der_ctrl_01 */
#define XD_GAIN_TH0_START                              (7)
#define XD_GAIN_TH0_END                                (0)
#define XD_GAIN_TH1_START                              (15)
#define XD_GAIN_TH1_END                                (8)

/* cnr_ctrl */
#define XD_CNR_CB_EN_START                             (0)
#define XD_CNR_CB_EN_END                               (0)
#define XD_CNR_CR_EN_START                             (1)
#define XD_CNR_CR_EN_END                               (1)
#define XD_LOW_LUMINACE_EN_START                       (2)
#define XD_LOW_LUMINACE_EN_END                         (2)

/* cnr_cb_diff */
#define XD_CB_DIFF_BOTTOM_START                        (10)
#define XD_CB_DIFF_BOTTOM_END                          (0)
#define XD_CB_DIFF_TOP_START                           (26)
#define XD_CB_DIFF_TOP_END                             (16)

/* cnr_cb_ctrl */
#define XD_CB_SLOPE_START                              (1)
#define XD_CB_SLOPE_END                                (0)
#define XD_CB_DIST_START                               (3)
#define XD_CB_DIST_END                                 (2)
#define XD_DELTA_CB_START                              (25)
#define XD_DELTA_CB_END                                (16)

/* cnr_cb_diff_low */
#define XD_CB_LOW_DIFF_BOTTOM_START                    (10)
#define XD_CB_LOW_DIFF_BOTTOM_END                      (0)
#define XD_CB_LOW_DIFF_TOP_START                       (26)
#define XD_CB_LOW_DIFF_TOP_END                         (16)

/* cnr_cb_ctrl_low */
#define XD_CB_LOW_SLOPE_START                          (1)
#define XD_CB_LOW_SLOPE_END                            (0)
#define XD_CB_LOW_DIST_START                           (3)
#define XD_CB_LOW_DIST_END                             (2)
#define XD_DELTA_CB_LOW_START                          (25)
#define XD_DELTA_CB_LOW_END                            (16)

/* cnr_cr_diff */
#define XD_CR_DIFF_BOTTOM_START                        (10)
#define XD_CR_DIFF_BOTTOM_END                          (0)
#define XD_CR_DIFF_TOP_START                           (26)
#define XD_CR_DIFF_TOP_END                             (16)

/* cnr_cr_ctrl */
#define XD_CR_SLOPE_START                              (1)
#define XD_CR_SLOPE_END                                (0)
#define XD_CR_DIST_START                               (3)
#define XD_CR_DIST_END                                 (2)
#define XD_DELTA_CR_START                              (25)
#define XD_DELTA_CR_END                                (16)

/* cnr_cr_diff_low */
#define XD_CR_LOW_DIFF_BOTTOM_START                    (10)
#define XD_CR_LOW_DIFF_BOTTOM_END                      (0)
#define XD_CR_LOW_DIFF_TOP_START                       (26)
#define XD_CR_LOW_DIFF_TOP_END                         (16)

/* cnr_cr_ctrl_low */
#define XD_CR_LOW_SLOPE_START                          (1)
#define XD_CR_LOW_SLOPE_END                            (0)
#define XD_CR_LOW_DIST_START                           (3)
#define XD_CR_LOW_DIST_END                             (2)
#define XD_DELTA_CR_LOW_START                          (25)
#define XD_DELTA_CR_LOW_END                            (16)


#endif

