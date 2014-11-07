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

#ifndef CE_BASE_ADDR
#define CE_BASE_ADDR 0


#define ADDR_CE_APL_CTRL_00                            ( CE_BASE_ADDR + 0x020 )
#define ADDR_CE_APL_CTRL_01                            ( CE_BASE_ADDR + 0x024 )
#define ADDR_CE_APL_STAT_00                            ( CE_BASE_ADDR + 0x030 )
#define ADDR_CE_APL_STAT_01                            ( CE_BASE_ADDR + 0x034 )
#define ADDR_CE_APL_STAT_02                            ( CE_BASE_ADDR + 0x038 )
#define ADDR_CE_APL_STAT_03                            ( CE_BASE_ADDR + 0x03C )
#define ADDR_CE_FILM_CTRL_00                           ( CE_BASE_ADDR + 0x040 )
#define ADDR_CE_FILM_CTRL_02                           ( CE_BASE_ADDR + 0x048 )
#define ADDR_CE_VSPRGB_CTRL_00                         ( CE_BASE_ADDR + 0x050 )
#define ADDR_CE_CEN_CTRL_00                            ( CE_BASE_ADDR + 0x060 )
#define ADDR_CE_CEN_CTRL_01                            ( CE_BASE_ADDR + 0x064 )
#define ADDR_CE_CEN_CTRL_02                            ( CE_BASE_ADDR + 0x068 )
#define ADDR_CE_CEN_CTRL_03                            ( CE_BASE_ADDR + 0x06C )
#define ADDR_CE_CEN_CTRL_04                            ( CE_BASE_ADDR + 0x070 )
#define ADDR_CE_CEN_IA_CTRL                            ( CE_BASE_ADDR + 0x078 )
#define ADDR_CE_CEN_IA_DATA                            ( CE_BASE_ADDR + 0x07C )
#define ADDR_CE_DCE_CTRL_00                            ( CE_BASE_ADDR + 0x080 )
#define ADDR_CE_DCE_CTRL_01                            ( CE_BASE_ADDR + 0x084 )
#define ADDR_CE_DCE_CTRL_02                            ( CE_BASE_ADDR + 0x088 )
#define ADDR_CE_DCE_CTRL_03                            ( CE_BASE_ADDR + 0x08C )
#define ADDR_CE_DCE_CTRL_04                            ( CE_BASE_ADDR + 0x090 )
#define ADDR_CE_DCE_CTRL_05                            ( CE_BASE_ADDR + 0x094 )
#define ADDR_CE_DCE_CTRL_08                            ( CE_BASE_ADDR + 0x0A0 )
#define ADDR_CE_DCE_IA_CTRL                            ( CE_BASE_ADDR + 0x0B0 )
#define ADDR_CE_DCE_IA_DATA                            ( CE_BASE_ADDR + 0x0B4 )
#define ADDR_CE_DSE_CTRL_00                            ( CE_BASE_ADDR + 0x0D0 )
#define ADDR_CE_DSE_CTRL_01                            ( CE_BASE_ADDR + 0x0D4 )
#define ADDR_CE_DSE_CTRL_02                            ( CE_BASE_ADDR + 0x0D8 )
#define ADDR_CE_DSE_IA_CTRL                            ( CE_BASE_ADDR + 0x0E0 )
#define ADDR_CE_DSE_IA_DATA                            ( CE_BASE_ADDR + 0x0E4 )
#define ADDR_CE_WB_CTRL_00                             ( CE_BASE_ADDR + 0x0F0 )
#define ADDR_CE_WB_CTRL_01                             ( CE_BASE_ADDR + 0x0F4 )
#define ADDR_CE_WB_CTRL_02                             ( CE_BASE_ADDR + 0x0F8 )
#define ADDR_CE_GMC_CTRL_00                            ( CE_BASE_ADDR + 0x100 )
#define ADDR_CE_GMC_CTRL_01                            ( CE_BASE_ADDR + 0x104 )
#define ADDR_CE_GMC_CTRL_02                            ( CE_BASE_ADDR + 0x108 )
#define ADDR_CE_GMC_CTRL_03                            ( CE_BASE_ADDR + 0x10C )
#define ADDR_CE_GMC_IA_CTRL                            ( CE_BASE_ADDR + 0x110 )
#define ADDR_CE_GMC_IA_DATA                            ( CE_BASE_ADDR + 0x114 )
#define ADDR_YUV2RGB_CLIP                              ( CE_BASE_ADDR + 0x120 )
#define ADDR_YUV2RGB_OFFSET                            ( CE_BASE_ADDR + 0x124 )
#define ADDR_YUV2RGB_COEF_00                           ( CE_BASE_ADDR + 0x128 )
#define ADDR_YUV2RGB_COEF_01                           ( CE_BASE_ADDR + 0x12C )
#define ADDR_YUV2RGB_GAIN                              ( CE_BASE_ADDR + 0x130 )
#define ADDR_CTI_CTRL_00                               ( CE_BASE_ADDR + 0x140 )
#define ADDR_CTI_CTRL_01                               ( CE_BASE_ADDR + 0x144 )
#define ADDR_HIST_CTRL_00                              ( CE_BASE_ADDR + 0x148 )
#define ADDR_HIST_CTRL_01                              ( CE_BASE_ADDR + 0x14c )
#define ADDR_HIST_CTRL_02                              ( CE_BASE_ADDR + 0x150 )
#define ADDR_HIST_MIN_MAX                              ( CE_BASE_ADDR + 0x154 )
#define ADDR_HIST_TOP5                                 ( CE_BASE_ADDR + 0x158 )
#define ADDR_HIST_TOTAL_CNT                            ( CE_BASE_ADDR + 0x15c )
#define ADDR_HIST_LUT_CTRL                             ( CE_BASE_ADDR + 0x160 )
#define ADDR_HIST_LUT_RDATA                            ( CE_BASE_ADDR + 0x164 )

/* ce_apl_ctrl_00 */
#define XD_APL_WIN_CTRL_X0_START                       (10)
#define XD_APL_WIN_CTRL_X0_END                         (0)
#define XD_APL_WIN_CTRL_Y0_START                       (26)
#define XD_APL_WIN_CTRL_Y0_END                         (16)

/* ce_apl_ctrl_01 */
#define XD_APL_WIN_CTRL_X1_START                       (10)
#define XD_APL_WIN_CTRL_X1_END                         (0)
#define XD_APL_WIN_CTRL_Y1_START                       (26)
#define XD_APL_WIN_CTRL_Y1_END                         (16)

/* ce_apl_stat_00 */
#define XD_APL_Y_START                                 (9)
#define XD_APL_Y_END                                   (0)
#define XD_APL_RGB_START                               (25)
#define XD_APL_RGB_END                                 (16)

/* ce_apl_stat_01 */
#define XD_APL_B_START                                 (9)
#define XD_APL_B_END                                   (0)
#define XD_APL_G_START                                 (19)
#define XD_APL_G_END                                   (10)
#define XD_APL_R_START                                 (29)
#define XD_APL_R_END                                   (20)

/* ce_apl_stat_02 */
#define XD_APL_Y_OTHER_SIDE_START                      (9)
#define XD_APL_Y_OTHER_SIDE_END                        (0)

/* ce_apl_stat_03 */
#define XD_APL_B_OTHER_SIDE_START                      (9)
#define XD_APL_B_OTHER_SIDE_END                        (0)
#define XD_APL_G_OTHER_SIDE_START                      (19)
#define XD_APL_G_OTHER_SIDE_END                        (10)
#define XD_APL_R_OTHER_SIDE_START                      (29)
#define XD_APL_R_OTHER_SIDE_END                        (20)

/* ce_film_ctrl_00 */
#define XD_VIGNETTING_ENABLE_START                     (0)
#define XD_VIGNETTING_ENABLE_END                       (0)
#define XD_FILM_CONTRAST_ENABLE_START                  (1)
#define XD_FILM_CONTRAST_ENABLE_END                    (1)
#define XD_CONTRAST_ALPHA_START                        (15)
#define XD_CONTRAST_ALPHA_END                          (8)
#define XD_CONTRAST_DELTA_MAX_START                    (23)
#define XD_CONTRAST_DELTA_MAX_END                      (16)
#define XD_VIGNETTING_GAIN_START                       (31)
#define XD_VIGNETTING_GAIN_END                         (24)

/* ce_film_ctrl_02 */
#define XD_TONE_GAIN_START                             (6)
#define XD_TONE_GAIN_END                               (0)
#define XD_TONE_OFFSET_START                           (13)
#define XD_TONE_OFFSET_END                             (8)

/* ce_vsprgb_ctrl_00 */
#define XD_CONTRAST_CTRL_START                         (9)
#define XD_CONTRAST_CTRL_END                           (0)
#define XD_CENTER_POSITION_START                       (19)
#define XD_CENTER_POSITION_END                         (12)
#define XD_BRIGHTNESS_START                            (29)
#define XD_BRIGHTNESS_END                              (20)
#define XD_CONTRAST_ENABLE_START                       (31)
#define XD_CONTRAST_ENABLE_END                         (31)

/* ce_cen_ctrl_00 */
#define XD_SELECT_HSV_START                            (2)
#define XD_SELECT_HSV_END                              (2)
#define XD_SELECT_RGB_START                            (3)
#define XD_SELECT_RGB_END                              (3)
#define XD_VSP_SEL_START                               (5)
#define XD_VSP_SEL_END                                 (5)
#define XD_GLOBAL_APL_SEL_START                        (6)
#define XD_GLOBAL_APL_SEL_END                          (6)
#define XD_REG_CEN_BYPASS_START                        (7)
#define XD_REG_CEN_BYPASS_END                          (7)
#define XD_REG_CEN_DEBUG_MODE_START                    (8)
#define XD_REG_CEN_DEBUG_MODE_END                      (8)
#define XD_1ST_CORE_GAIN_DISABLE_START                 (10)
#define XD_1ST_CORE_GAIN_DISABLE_END                   (10)
#define XD_2ND_CORE_GAIN_DISABLE_START                 (11)
#define XD_2ND_CORE_GAIN_DISABLE_END                   (11)
#define XD_V_SCALER_EN_START                           (12)
#define XD_V_SCALER_EN_END                             (12)
#define XD_DEBUG_MODE_SEL_START                        (14)
#define XD_DEBUG_MODE_SEL_END                          (13)
#define XD_DEMO_MODE_START                             (23)
#define XD_DEMO_MODE_END                               (16)

/* ce_cen_ctrl_01 */
#define XD_SHOW_COLOR_REGION0_START                    (7)
#define XD_SHOW_COLOR_REGION0_END                      (0)
#define XD_SHOW_COLOR_REGION1_START                    (15)
#define XD_SHOW_COLOR_REGION1_END                      (8)
#define XD_COLOR_REGION_EN0_START                      (23)
#define XD_COLOR_REGION_EN0_END                        (16)
#define XD_COLOR_REGION_EN1_START                      (31)
#define XD_COLOR_REGION_EN1_END                        (24)

/* ce_cen_ctrl_02 */
#define XD_IHSV_SGAIN_START                            (7)
#define XD_IHSV_SGAIN_END                              (0)
#define XD_IHSV_VGAIN_START                            (15)
#define XD_IHSV_VGAIN_END                              (8)

/* ce_cen_ctrl_03 */
#define XD_IHSV_HOFFSET_START                          (7)
#define XD_IHSV_HOFFSET_END                            (0)
#define XD_IHSV_SOFFSET_START                          (15)
#define XD_IHSV_SOFFSET_END                            (8)
#define XD_IHSV_VOFFSET_START                          (23)
#define XD_IHSV_VOFFSET_END                            (16)

/* ce_cen_ctrl_04 */
#define XD_DEN_CTRL0_START                             (7)
#define XD_DEN_CTRL0_END                               (0)
#define XD_DEN_APL_LIMIT_HIGH_START                    (15)
#define XD_DEN_APL_LIMIT_HIGH_END                      (8)
#define XD_DEN_GAIN_START                              (23)
#define XD_DEN_GAIN_END                                (16)
#define XD_DEN_CORING_START                            (31)
#define XD_DEN_CORING_END                              (24)

/* ce_cen_ia_ctrl */
#define XD_CEN_LUT_EN_START                            (0)
#define XD_CEN_LUT_EN_END                              (0)
#define XD_CEN_LUT_CTRL_MODE_START                     (6)
#define XD_CEN_LUT_CTRL_MODE_END                       (4)
#define XD_CEN_LUT_ADRS_START                          (15)
#define XD_CEN_LUT_ADRS_END                            (8)
#define XD_CEN_LUT_WEN_START                           (16)
#define XD_CEN_LUT_WEN_END                             (16)

/* ce_cen_ia_data */
#define XD_CEN_LUT_DATA_START                          (31)
#define XD_CEN_LUT_DATA_END                            (0)

/* ce_dce_ctrl_00 */
#define XD_DYNAMIC_CONTRAST_EN_START                   (0)
#define XD_DYNAMIC_CONTRAST_EN_END                     (0)
#define XD_COLOR_REGION0_SEL_START                     (8)
#define XD_COLOR_REGION0_SEL_END                       (8)
#define XD_COLOR_REGION1_SEL_START                     (9)
#define XD_COLOR_REGION1_SEL_END                       (9)
#define XD_COLOR_REGION2_SEL_START                     (10)
#define XD_COLOR_REGION2_SEL_END                       (10)
#define XD_COLOR_REGION3_SEL_START                     (11)
#define XD_COLOR_REGION3_SEL_END                       (11)
#define XD_COLOR_REGION4_SEL_START                     (12)
#define XD_COLOR_REGION4_SEL_END                       (12)
#define XD_COLOR_REGION5_SEL_START                     (13)
#define XD_COLOR_REGION5_SEL_END                       (13)
#define XD_COLOR_REGION6_SEL_START                     (14)
#define XD_COLOR_REGION6_SEL_END                       (14)
#define XD_COLOR_REGION7_SEL_START                     (15)
#define XD_COLOR_REGION7_SEL_END                       (15)
#define XD_COLOR_REGION8_SEL_START                     (16)
#define XD_COLOR_REGION8_SEL_END                       (16)
#define XD_COLOR_REGION9_SEL_START                     (17)
#define XD_COLOR_REGION9_SEL_END                       (17)
#define XD_COLOR_REGION10_SEL_START                    (18)
#define XD_COLOR_REGION10_SEL_END                      (18)
#define XD_COLOR_REGION11_SEL_START                    (19)
#define XD_COLOR_REGION11_SEL_END                      (19)
#define XD_COLOR_REGION12_SEL_START                    (20)
#define XD_COLOR_REGION12_SEL_END                      (20)
#define XD_COLOR_REGION13_SEL_START                    (21)
#define XD_COLOR_REGION13_SEL_END                      (21)
#define XD_COLOR_REGION14_SEL_START                    (22)
#define XD_COLOR_REGION14_SEL_END                      (22)
#define XD_COLOR_REGION15_SEL_START                    (23)
#define XD_COLOR_REGION15_SEL_END                      (23)
#define XD_DCE_DOMAIN_SEL_START                        (24)
#define XD_DCE_DOMAIN_SEL_END                          (24)
#define XD_HISTOGRAM_MODE_START                        (29)
#define XD_HISTOGRAM_MODE_END                          (28)
#define XD_DCE_Y_SELECTION_START                       (30)
#define XD_DCE_Y_SELECTION_END                         (30)

/* ce_dce_ctrl_01 */
#define XD_COLOR_REGION_GAIN_START                     (23)
#define XD_COLOR_REGION_GAIN_END                       (16)

/* ce_dce_ctrl_02 */
#define XD_COLOR_REGION_EN_START                       (0)
#define XD_COLOR_REGION_EN_END                         (0)
#define XD_COLOR_DEBUG_EN_START                        (1)
#define XD_COLOR_DEBUG_EN_END                          (1)
#define XD_Y_GRAD_GAIN_START                           (5)
#define XD_Y_GRAD_GAIN_END                             (4)
#define XD_CB_GRAD_GAIN_START                          (9)
#define XD_CB_GRAD_GAIN_END                            (8)
#define XD_CR_GRAD_GAIN_START                          (13)
#define XD_CR_GRAD_GAIN_END                            (12)

/* ce_dce_ctrl_03 */
#define XD_Y_RANGE_MIN_START                           (9)
#define XD_Y_RANGE_MIN_END                             (0)
#define XD_Y_RANGE_MAX_START                           (25)
#define XD_Y_RANGE_MAX_END                             (16)

/* ce_dce_ctrl_04 */
#define XD_CB_RANGE_MIN_START                          (9)
#define XD_CB_RANGE_MIN_END                            (0)
#define XD_CB_RANGE_MAX_START                          (25)
#define XD_CB_RANGE_MAX_END                            (16)

/* ce_dce_ctrl_05 */
#define XD_CR_RANGE_MIN_START                          (9)
#define XD_CR_RANGE_MIN_END                            (0)
#define XD_CR_RANGE_MAX_START                          (25)
#define XD_CR_RANGE_MAX_END                            (16)

/* ce_dce_ctrl_08 */
#define XD_HIF_DYC_WDATA_Y_32ND_START                  (9)
#define XD_HIF_DYC_WDATA_Y_32ND_END                    (0)
#define XD_HIF_DYC_WDATA_X_32ND_START                  (25)
#define XD_HIF_DYC_WDATA_X_32ND_END                    (16)

/* ce_dce_ia_ctrl */
#define XD_DCE_LUT_EN_START                            (0)
#define XD_DCE_LUT_EN_END                              (0)
#define XD_DCE_LUT_ADRS_START                          (12)
#define XD_DCE_LUT_ADRS_END                            (8)
#define XD_DCE_LUT_WEN_START                           (16)
#define XD_DCE_LUT_WEN_END                             (16)

/* ce_dce_ia_data */
#define XD_DCE_LUT_DATA_START                          (31)
#define XD_DCE_LUT_DATA_END                            (0)

/* ce_dse_ctrl_00 */
#define XD_DYNAMIC_SATURATION_EN_START                 (0)
#define XD_DYNAMIC_SATURATION_EN_END                   (0)
#define XD_WIN_SELECTION_START                         (3)
#define XD_WIN_SELECTION_END                           (3)
#define XD_SATURATION_REGION0_SEL_START                (8)
#define XD_SATURATION_REGION0_SEL_END                  (8)
#define XD_SATURATION_REGION1_SEL_START                (9)
#define XD_SATURATION_REGION1_SEL_END                  (9)
#define XD_SATURATION_REGION2_SEL_START                (10)
#define XD_SATURATION_REGION2_SEL_END                  (10)
#define XD_SATURATION_REGION3_SEL_START                (11)
#define XD_SATURATION_REGION3_SEL_END                  (11)
#define XD_SATURATION_REGION4_SEL_START                (12)
#define XD_SATURATION_REGION4_SEL_END                  (12)
#define XD_SATURATION_REGION5_SEL_START                (13)
#define XD_SATURATION_REGION5_SEL_END                  (13)
#define XD_SATURATION_REGION6_SEL_START                (14)
#define XD_SATURATION_REGION6_SEL_END                  (14)
#define XD_SATURATION_REGION7_SEL_START                (15)
#define XD_SATURATION_REGION7_SEL_END                  (15)
#define XD_SATURATION_REGION8_SEL_START                (16)
#define XD_SATURATION_REGION8_SEL_END                  (16)
#define XD_SATURATION_REGION9_SEL_START                (17)
#define XD_SATURATION_REGION9_SEL_END                  (17)
#define XD_SATURATION_REGION10_SEL_START               (18)
#define XD_SATURATION_REGION10_SEL_END                 (18)
#define XD_SATURATION_REGION11_SEL_START               (19)
#define XD_SATURATION_REGION11_SEL_END                 (19)
#define XD_SATURATION_REGION12_SEL_START               (20)
#define XD_SATURATION_REGION12_SEL_END                 (20)
#define XD_SATURATION_REGION13_SEL_START               (21)
#define XD_SATURATION_REGION13_SEL_END                 (21)
#define XD_SATURATION_REGION14_SEL_START               (22)
#define XD_SATURATION_REGION14_SEL_END                 (22)
#define XD_SATURATION_REGION15_SEL_START               (23)
#define XD_SATURATION_REGION15_SEL_END                 (23)

/* ce_dse_ctrl_01 */
#define XD_SATURATION_REGION_GAIN_START                (31)
#define XD_SATURATION_REGION_GAIN_END                  (24)

/* ce_dse_ctrl_02 */
#define XD_HIF_DSE_WDATA_Y_32ND_START                  (9)
#define XD_HIF_DSE_WDATA_Y_32ND_END                    (0)
#define XD_HIF_DSE_WDATA_X_32ND_START                  (25)
#define XD_HIF_DSE_WDATA_X_32ND_END                    (16)

/* ce_dse_ia_ctrl */
#define XD_DSE_LUT_EN_START                            (0)
#define XD_DSE_LUT_EN_END                              (0)
#define XD_DSE_LUT_ADRS_START                          (12)
#define XD_DSE_LUT_ADRS_END                            (8)
#define XD_DSE_LUT_WEN_START                           (16)
#define XD_DSE_LUT_WEN_END                             (16)

/* ce_dse_ia_data */
#define XD_DSE_LUT_DATA_START                          (31)
#define XD_DSE_LUT_DATA_END                            (0)

/* ce_wb_ctrl_00 */
#define XD_WB_EN_START                                 (0)
#define XD_WB_EN_END                                   (0)
#define XD_GAMMA_EN_START                              (30)
#define XD_GAMMA_EN_END                                (30)
#define XD_DEGAMMA_EN_START                            (31)
#define XD_DEGAMMA_EN_END                              (31)

/* ce_wb_ctrl_01 */
#define XD_USER_CTRL_G_GAIN_START                      (7)
#define XD_USER_CTRL_G_GAIN_END                        (0)
#define XD_USER_CTRL_B_GAIN_START                      (15)
#define XD_USER_CTRL_B_GAIN_END                        (8)
#define XD_USER_CTRL_R_GAIN_START                      (23)
#define XD_USER_CTRL_R_GAIN_END                        (16)

/* ce_wb_ctrl_02 */
#define XD_USER_CTRL_G_OFFSET_START                    (7)
#define XD_USER_CTRL_G_OFFSET_END                      (0)
#define XD_USER_CTRL_B_OFFSET_START                    (15)
#define XD_USER_CTRL_B_OFFSET_END                      (8)
#define XD_USER_CTRL_R_OFFSET_START                    (23)
#define XD_USER_CTRL_R_OFFSET_END                      (16)

/* ce_gmc_ctrl_00 */
#define XD_PXL_REP_XPOS_START                          (10)
#define XD_PXL_REP_XPOS_END                            (0)
#define XD_LUT_WMASK_G_START                           (12)
#define XD_LUT_WMASK_G_END                             (12)
#define XD_LUT_WMASK_B_START                           (13)
#define XD_LUT_WMASK_B_END                             (13)
#define XD_LUT_WMASK_R_START                           (14)
#define XD_LUT_WMASK_R_END                             (14)
#define XD_PXL_REP_AREA_START                          (15)
#define XD_PXL_REP_AREA_END                            (15)
#define XD_PXL_REP_YPOS_START                          (27)
#define XD_PXL_REP_YPOS_END                            (16)
#define XD_PXL_REP_DISABLE_G_START                     (28)
#define XD_PXL_REP_DISABLE_G_END                       (28)
#define XD_PXL_REP_DISABLE_B_START                     (29)
#define XD_PXL_REP_DISABLE_B_END                       (29)
#define XD_PXL_REP_DISABLE_R_START                     (30)
#define XD_PXL_REP_DISABLE_R_END                       (30)
#define XD_PXL_REP_ENABLE_START                        (31)
#define XD_PXL_REP_ENABLE_END                          (31)

/* ce_gmc_ctrl_01 */
#define XD_PXL_REP_WIDTH_START                         (10)
#define XD_PXL_REP_WIDTH_END                           (0)
#define XD_PXL_REP_HEIGHT_START                        (26)
#define XD_PXL_REP_HEIGHT_END                          (16)

/* ce_gmc_ctrl_02 */
#define XD_PXL_REP_VALUE_G_START                       (9)
#define XD_PXL_REP_VALUE_G_END                         (0)
#define XD_PXL_REP_VALUE_B_START                       (19)
#define XD_PXL_REP_VALUE_B_END                         (10)
#define XD_PXL_REP_VALUE_R_START                       (29)
#define XD_PXL_REP_VALUE_R_END                         (20)
#define XD_GMC_MODE_START                              (31)
#define XD_GMC_MODE_END                                (30)

/* ce_gmc_ctrl_03 */
#define XD_DITHER_EN_START                             (0)
#define XD_DITHER_EN_END                               (0)
#define XD_DECONTOUR_EN_START                          (1)
#define XD_DECONTOUR_EN_END                            (1)
#define XD_DITHER_RANDOM_FREEZE_EN_START               (2)
#define XD_DITHER_RANDOM_FREEZE_EN_END                 (2)
#define XD_DEMO_PATTERN_ENABLE_START                   (3)
#define XD_DEMO_PATTERN_ENABLE_END                     (3)
#define XD_BIT_MODE_START                              (5)
#define XD_BIT_MODE_END                                (4)
#define XD_DECONTOUR_GAIN_R_START                      (15)
#define XD_DECONTOUR_GAIN_R_END                        (8)
#define XD_DECONTOUR_GAIN_G_START                      (23)
#define XD_DECONTOUR_GAIN_G_END                        (16)
#define XD_DECONTOUR_GAIN_B_START                      (31)
#define XD_DECONTOUR_GAIN_B_END                        (24)

/* ce_gmc_ia_ctrl */
#define XD_GMC_LUT_MAX_VALUE_START                     (7)
#define XD_GMC_LUT_MAX_VALUE_END                       (0)
#define XD_GMC_LUT_ADRS_START                          (15)
#define XD_GMC_LUT_ADRS_END                            (8)
#define XD_GMC_LUT_WEN_START                           (16)
#define XD_GMC_LUT_WEN_END                             (16)

/* ce_gmc_ia_data */
#define XD_LUT_DATA_G_START                            (9)
#define XD_LUT_DATA_G_END                              (0)
#define XD_LUT_DATA_B_START                            (19)
#define XD_LUT_DATA_B_END                              (10)
#define XD_LUT_DATA_R_START                            (29)
#define XD_LUT_DATA_R_END                              (20)

/* yuv2rgb_clip */
#define XD_RGB_MAX_START                               (7)
#define XD_RGB_MAX_END                                 (0)
#define XD_RGB_MIN_START                               (23)
#define XD_RGB_MIN_END                                 (16)

/* yuv2rgb_offset */
#define XD_Y_OFFSET_START                              (7)
#define XD_Y_OFFSET_END                                (0)
#define XD_C_OFFSET_START                              (23)
#define XD_C_OFFSET_END                                (16)

/* yuv2rgb_coef_00 */
#define XD_R_CR_COEF_START                             (11)
#define XD_R_CR_COEF_END                               (0)
#define XD_B_CB_COEF_START                             (27)
#define XD_B_CB_COEF_END                               (16)

/* yuv2rgb_coef_01 */
#define XD_G_CR_COEF_START                             (11)
#define XD_G_CR_COEF_END                               (0)
#define XD_G_CB_COEF_START                             (27)
#define XD_G_CB_COEF_END                               (16)

/* yuv2rgb_gain */
#define XD_Y_GAIN_START                                (11)
#define XD_Y_GAIN_END                                  (0)

/* cti_ctrl_00 */
#define XD_CTI_EN_START                                (0)
#define XD_CTI_EN_END                                  (0)
#define XD_TAP_SIZE_START                              (6)
#define XD_TAP_SIZE_END                                (4)
#define XD_COLOR_CTI_GAIN_START                        (15)
#define XD_COLOR_CTI_GAIN_END                          (8)

/* cti_ctrl_01 */
#define XD_YCM_EN_START                                (0)
#define XD_YCM_EN_END                                  (0)
#define XD_YCM_BAND_SEL_START                          (6)
#define XD_YCM_BAND_SEL_END                            (4)
#define XD_YCM_DIFF_TH_START                           (15)
#define XD_YCM_DIFF_TH_END                             (8)
#define XD_YCM_Y_GAIN_START                            (19)
#define XD_YCM_Y_GAIN_END                              (16)
#define XD_YCM_C_GAIN_START                            (23)
#define XD_YCM_C_GAIN_END                              (20)

/* hist_ctrl_00 */
#define XD_HIST_ZONE_DISPLAY_START                     (0)
#define XD_HIST_ZONE_DISPLAY_END                       (0)
#define XD_HIST_LUT_STEP_MODE_START                    (1)
#define XD_HIST_LUT_STEP_MODE_END                      (1)
#define XD_HIST_LUT_X_POS_RATE_START                   (3)
#define XD_HIST_LUT_X_POS_RATE_END                     (2)
#define XD_HIST_LUT_Y_POS_RATE_START                   (5)
#define XD_HIST_LUT_Y_POS_RATE_END                     (4)

/* hist_ctrl_01 */
#define XD_HIST_WINDOW_START_X_START                   (10)
#define XD_HIST_WINDOW_START_X_END                     (0)
#define XD_HIST_WINDOW_START_Y_START                   (26)
#define XD_HIST_WINDOW_START_Y_END                     (16)

/* hist_ctrl_02 */
#define XD_HIST_WINDOW_X_SIZE_START                    (10)
#define XD_HIST_WINDOW_X_SIZE_END                      (0)
#define XD_HIST_WINDOW_Y_SIZE_START                    (26)
#define XD_HIST_WINDOW_Y_SIZE_END                      (16)

/* hist_min_max */
#define XD_HIST_Y_MIN_VALUE_START                      (7)
#define XD_HIST_Y_MIN_VALUE_END                        (0)
#define XD_HIST_Y_MAX_VALUE_START                      (23)
#define XD_HIST_Y_MAX_VALUE_END                        (16)

/* hist_top5 */
#define XD_HIST_BIN_HIGH_LEVEL_TOP1_START              (5)
#define XD_HIST_BIN_HIGH_LEVEL_TOP1_END                (0)
#define XD_HIST_BIN_HIGH_LEVEL_TOP2_START              (13)
#define XD_HIST_BIN_HIGH_LEVEL_TOP2_END                (8)
#define XD_HIST_BIN_HIGH_LEVEL_TOP3_START              (21)
#define XD_HIST_BIN_HIGH_LEVEL_TOP3_END                (16)
#define XD_HIST_BIN_HIGH_LEVEL_TOP4_START              (29)
#define XD_HIST_BIN_HIGH_LEVEL_TOP4_END                (24)

/* hist_total_cnt */
#define XD_HIST_BIN_HIGH_LEVEL_TOP5_START              (5)
#define XD_HIST_BIN_HIGH_LEVEL_TOP5_END                (0)
#define XD_HIST_WINDOW_PIXEL_TOTAL_CNT_START           (28)
#define XD_HIST_WINDOW_PIXEL_TOTAL_CNT_END             (8)

/* hist_lut_ctrl */
#define XD_HIST_LUT_ADRS_START                         (5)
#define XD_HIST_LUT_ADRS_END                           (0)

/* hist_lut_rdata */
#define XD_DSE_LUT_RDATA_START                         (31)
#define XD_DSE_LUT_RDATA_END                           (0)


#endif

