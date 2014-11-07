#ifndef VID1_BASE_ADDR
#define VID1_BASE_ADDR 0x31010000


#define ADDR_VID1_RD_CFG0                              ( 0x0000 )
#define ADDR_VID1_RD_CFG1                              ( 0x0004 )
#define ADDR_VID1_RD_CFG2                              ( 0x0008 )
#define ADDR_VID1_RD_WIDTH_HEIGHT                      ( 0x0028 )
#define ADDR_VID1_RD_STARTXY                           ( 0x002C )
#define ADDR_VID1_RD_SIZEXY                            ( 0x0030 )
#define ADDR_VID1_RD_PIP_SIZEXY                        ( 0x0038 )
#define ADDR_VID1_RD_SCALE_RATIOXY                     ( 0x003C )
#define ADDR_VID1_RD_DSS_BASE_Y_ADDR0                  ( 0x0040 )
#define ADDR_VID1_RD_DSS_BASE_U_ADDR0                  ( 0x0044 )
#define ADDR_VID1_RD_DSS_BASE_V_ADDR0                  ( 0x0048 )
#define ADDR_VID1_RD_DSS_BASE_Y_ADDR1                  ( 0x004C )
#define ADDR_VID1_RD_DSS_BASE_U_ADDR1                  ( 0x0050 )
#define ADDR_VID1_RD_DSS_BASE_V_ADDR1                  ( 0x0054 )
#define ADDR_VID1_RD_DSS_BASE_Y_ADDR2                  ( 0x0058 )
#define ADDR_VID1_RD_DSS_BASE_U_ADDR2                  ( 0x005C )
#define ADDR_VID1_RD_DSS_BASE_V_ADDR2                  ( 0x0060 )
#define ADDR_VID1_RD_STATUS                            ( 0x0064 )
#define ADDR_VID1_WB_CFG0                              ( 0x0190 )
#define ADDR_VID1_WB_CFG1                              ( 0x0194 )
#define ADDR_VID1_WB_ALPHA_VALUE                       ( 0x01B8 )
#define ADDR_VID1_WB_WIDTH_HEIGHT                      ( 0x01BC )
#define ADDR_VID1_WB_STARTXY                           ( 0x01C0 )
#define ADDR_VID1_WB_SIZEXY                            ( 0x01C4 )
#define ADDR_VID1_WB_DSS_BASE_Y_ADDR0                  ( 0x01CC )
#define ADDR_VID1_WB_DSS_BASE_U_ADDR0                  ( 0x01D0 )
#define ADDR_VID1_WB_DSS_BASE_Y_ADDR1                  ( 0x01D8 )
#define ADDR_VID1_WB_DSS_BASE_U_ADDR1                  ( 0x01DC )
#define ADDR_VID1_WB_STATUS                            ( 0x01E4 )
#define ADDR_VID1_POLY_HCOEF_12T_0P0                   ( 0x0500 )
#define ADDR_VID1_POLY_HCOEF_12T_0P1                   ( 0x0504 )
#define ADDR_VID1_POLY_HCOEF_12T_0P2                   ( 0x0508 )
#define ADDR_VID1_POLY_HCOEF_12T_0P3                   ( 0x050C )
#define ADDR_VID1_POLY_HCOEF_12T_1P0                   ( 0x0510 )
#define ADDR_VID1_POLY_HCOEF_12T_1P1                   ( 0x0514 )
#define ADDR_VID1_POLY_HCOEF_12T_1P2                   ( 0x0518 )
#define ADDR_VID1_POLY_HCOEF_12T_1P3                   ( 0x051C )
#define ADDR_VID1_POLY_HCOEF_12T_2P0                   ( 0x0520 )
#define ADDR_VID1_POLY_HCOEF_12T_2P1                   ( 0x0524 )
#define ADDR_VID1_POLY_HCOEF_12T_2P2                   ( 0x0528 )
#define ADDR_VID1_POLY_HCOEF_12T_2P3                   ( 0x052C )
#define ADDR_VID1_POLY_HCOEF_12T_3P0                   ( 0x0530 )
#define ADDR_VID1_POLY_HCOEF_12T_3P1                   ( 0x0534 )
#define ADDR_VID1_POLY_HCOEF_12T_3P2                   ( 0x0538 )
#define ADDR_VID1_POLY_HCOEF_12T_3P3                   ( 0x053C )
#define ADDR_VID1_POLY_HCOEF_12T_4P0                   ( 0x0540 )
#define ADDR_VID1_POLY_HCOEF_12T_4P1                   ( 0x0544 )
#define ADDR_VID1_POLY_HCOEF_12T_4P2                   ( 0x0548 )
#define ADDR_VID1_POLY_HCOEF_12T_4P3                   ( 0x054C )
#define ADDR_VID1_POLY_HCOEF_12T_5P0                   ( 0x0550 )
#define ADDR_VID1_POLY_HCOEF_12T_5P1                   ( 0x0554 )
#define ADDR_VID1_POLY_HCOEF_12T_5P2                   ( 0x0558 )
#define ADDR_VID1_POLY_HCOEF_12T_5P3                   ( 0x055C )
#define ADDR_VID1_POLY_HCOEF_12T_6P0                   ( 0x0560 )
#define ADDR_VID1_POLY_HCOEF_12T_6P1                   ( 0x0564 )
#define ADDR_VID1_POLY_HCOEF_12T_6P2                   ( 0x0568 )
#define ADDR_VID1_POLY_HCOEF_12T_6P3                   ( 0x056C )
#define ADDR_VID1_POLY_HCOEF_12T_7P0                   ( 0x0570 )
#define ADDR_VID1_POLY_HCOEF_12T_7P1                   ( 0x0574 )
#define ADDR_VID1_POLY_HCOEF_12T_7P2                   ( 0x0578 )
#define ADDR_VID1_POLY_HCOEF_12T_7P3                   ( 0x057C )
#define ADDR_VID1_POLY_VCOEF_4T_0P0                    ( 0x0600 )
#define ADDR_VID1_POLY_VCOEF_4T_0P1                    ( 0x0604 )
#define ADDR_VID1_POLY_VCOEF_4T_1P0                    ( 0x0608 )
#define ADDR_VID1_POLY_VCOEF_4T_1P1                    ( 0x060C )
#define ADDR_VID1_POLY_VCOEF_4T_2P0                    ( 0x0610 )
#define ADDR_VID1_POLY_VCOEF_4T_2P1                    ( 0x0614 )
#define ADDR_VID1_POLY_VCOEF_4T_3P0                    ( 0x0618 )
#define ADDR_VID1_POLY_VCOEF_4T_3P1                    ( 0x061C )
#define ADDR_VID1_POLY_VCOEF_4T_4P0                    ( 0x0620 )
#define ADDR_VID1_POLY_VCOEF_4T_4P1                    ( 0x0624 )
#define ADDR_VID1_POLY_VCOEF_4T_5P0                    ( 0x0628 )
#define ADDR_VID1_POLY_VCOEF_4T_5P1                    ( 0x062C )
#define ADDR_VID1_POLY_VCOEF_4T_6P0                    ( 0x0630 )
#define ADDR_VID1_POLY_VCOEF_4T_6P1                    ( 0x0634 )
#define ADDR_VID1_POLY_VCOEF_4T_7P0                    ( 0x0638 )
#define ADDR_VID1_POLY_VCOEF_4T_7P1                    ( 0x063C )

#if 0 /* Re define */
/* vid1_rd_cfg0 */
#define DU_SRC_EN_START                                (0)
#define DU_SRC_EN_END                                  (0)
#define DU_SRC_DST_SEL_START                           (1)
#define DU_SRC_DST_SEL_END                             (1)
#define DU_SRC_DUAL_OP_EN_START                        (2)
#define DU_SRC_DUAL_OP_EN_END                          (2)
#define DU_SRC_BUF_SEL_START                           (5)
#define DU_SRC_BUF_SEL_END                             (4)
#define DU_SRC_TRANS_NUM_START                         (7)
#define DU_SRC_TRANS_NUM_END                           (6)
#define DU_SRC_SYNC_SEL_START                          (9)
#define DU_SRC_SYNC_SEL_END                            (8)
#define DU_SRC_REG_SW_UPDATE_START                     (12)
#define DU_SRC_REG_SW_UPDATE_END                       (12)

/* vid1_rd_cfg1 */
#define DU_SRC_FORMAT_START                            (3)
#define DU_SRC_FORMAT_END                              (0)
#define DU_SRC_BYTE_SWAP_START                         (5)
#define DU_SRC_BYTE_SWAP_END                           (4)
#define DU_SRC_WORD_SWAP_START                         (6)
#define DU_SRC_WORD_SWAP_END                           (6)
#define DU_SRC_ALPHA_SWAP_START                        (7)
#define DU_SRC_ALPHA_SWAP_END                          (7)
#define DU_SRC_RGB_SWAP_START                          (10)
#define DU_SRC_RGB_SWAP_END                            (8)
#define DU_SRC_YUV_SWAP_START                          (11)
#define DU_SRC_YUV_SWAP_END                            (11)
#define DU_SRC_LSB_SEL_START                           (14)
#define DU_SRC_LSB_SEL_END                             (14)
#define DU_SRC_BND_FILL_MODE_START                     (17)
#define DU_SRC_BND_FILL_MODE_END                       (17)
#define DU_SRC_VSCALE_MODE_START                       (18)
#define DU_SRC_VSCALE_MODE_END                         (18)
#define DU_SRC_SCL_BP_MODE_START                       (19)
#define DU_SRC_SCL_BP_MODE_END                         (19)
#define DU_SRC_FLIP_MODE_START                         (21)
#define DU_SRC_FLIP_MODE_END                           (20)

/* vid1_rd_cfg2 */
#define DU_SRC_FR_XD_RE_PATH_SEL_START                 (0)
#define DU_SRC_FR_XD_RE_PATH_SEL_END                   (0)
#define DU_SRC_TO_XD_RE_PATH_SEL_START                 (1)
#define DU_SRC_TO_XD_RE_PATH_SEL_END                   (1)
#define DU_SRC_FR_XD_NR_PATH_SEL_START                 (2)
#define DU_SRC_FR_XD_NR_PATH_SEL_END                   (2)
#define DU_SRC_TO_XD_NR_PATH_SEL_START                 (3)
#define DU_SRC_TO_XD_NR_PATH_SEL_END                   (3)
#define DU_VID_XD_PATH_EN_START                        (8)
#define DU_VID_XD_PATH_EN_END                          (8)

/* vid1_rd_width_height */
#define DU_SRC_WIDTH_START                             (12)
#define DU_SRC_WIDTH_END                               (0)
#define DU_SRC_HEIGHT_START                            (28)
#define DU_SRC_HEIGHT_END                              (16)

/* vid1_rd_startxy */
#define DU_SRC_SX_START                                (12)
#define DU_SRC_SX_END                                  (0)
#define DU_SRC_SY_START                                (28)
#define DU_SRC_SY_END                                  (16)

/* vid1_rd_sizexy */
#define DU_SRC_SIZEX_START                             (12)
#define DU_SRC_SIZEX_END                               (0)
#define DU_SRC_SIZEY_START                             (28)
#define DU_SRC_SIZEY_END                               (16)

/* vid1_rd_pip_sizexy */
#define DU_SRC_PIP_SIZEX_START                         (12)
#define DU_SRC_PIP_SIZEX_END                           (0)
#define DU_SRC_PIP_SIZEY_START                         (28)
#define DU_SRC_PIP_SIZEY_END                           (16)

/* vid1_rd_scale_ratioxy */
#define DU_SRC_X_RATIO_START                           (15)
#define DU_SRC_X_RATIO_END                             (0)
#define DU_SRC_Y_RATIO_START                           (31)
#define DU_SRC_Y_RATIO_END                             (16)

/* vid1_rd_dss_base_y_addr0 */
#define DU_SRC_ST_ADDR_Y0_START                        (31)
#define DU_SRC_ST_ADDR_Y0_END                          (0)

/* vid1_rd_dss_base_u_addr0 */
#define DU_SRC_ST_ADDR_U0_START                        (31)
#define DU_SRC_ST_ADDR_U0_END                          (0)

/* vid1_rd_dss_base_v_addr0 */
#define DU_SRC_ST_ADDR_V0_START                        (31)
#define DU_SRC_ST_ADDR_V0_END                          (0)

/* vid1_rd_dss_base_y_addr1 */
#define DU_SRC_ST_ADDR_Y1_START                        (31)
#define DU_SRC_ST_ADDR_Y1_END                          (0)

/* vid1_rd_dss_base_u_addr1 */
#define DU_SRC_ST_ADDR_U1_START                        (31)
#define DU_SRC_ST_ADDR_U1_END                          (0)

/* vid1_rd_dss_base_v_addr1 */
#define DU_SRC_ST_ADDR_V1_START                        (31)
#define DU_SRC_ST_ADDR_V1_END                          (0)

/* vid1_rd_dss_base_y_addr2 */
#define DU_SRC_ST_ADDR_Y2_START                        (31)
#define DU_SRC_ST_ADDR_Y2_END                          (0)

/* vid1_rd_dss_base_u_addr2 */
#define DU_SRC_ST_ADDR_U2_START                        (31)
#define DU_SRC_ST_ADDR_U2_END                          (0)

/* vid1_rd_dss_base_v_addr2 */
#define DU_SRC_ST_ADDR_V2_START                        (31)
#define DU_SRC_ST_ADDR_V2_END                          (0)

/* vid1_rd_status */
#define DU_RDMIF_FULL_CMDFIFO_START                    (0)
#define DU_RDMIF_FULL_CMDFIFO_END                      (0)
#define DU_RDMIF_FULL_DATFIFO_START                    (1)
#define DU_RDMIF_FULL_DATFIFO_END                      (1)
#define DU_RDMIF_EMPTY_CMDFIFO_START                   (4)
#define DU_RDMIF_EMPTY_CMDFIFO_END                     (4)
#define DU_RDMIF_EMPTY_DATFIFO_START                   (5)
#define DU_RDMIF_EMPTY_DATFIFO_END                     (5)
#define DU_RDBIF_FULL_DATFIFO_Y_START                  (8)
#define DU_RDBIF_FULL_DATFIFO_Y_END                    (8)
#define DU_RDBIF_FULL_DATFIFO_U_START                  (9)
#define DU_RDBIF_FULL_DATFIFO_U_END                    (9)
#define DU_RDBIF_FULL_DATFIFO_V_START                  (10)
#define DU_RDBIF_FULL_DATFIFO_V_END                    (10)
#define DU_RDBIF_EMPTY_DATFIFO_Y_START                 (12)
#define DU_RDBIF_EMPTY_DATFIFO_Y_END                   (12)
#define DU_RDBIF_EMPTY_DATFIFO_U_START                 (13)
#define DU_RDBIF_EMPTY_DATFIFO_U_END                   (13)
#define DU_RDBIF_EMPTY_DATFIFO_V_START                 (14)
#define DU_RDBIF_EMPTY_DATFIFO_V_END                   (14)
#define DU_RDBIF_CON_STATE_Y_START                     (16)
#define DU_RDBIF_CON_STATE_Y_END                       (16)
#define DU_RDBIF_CON_STATE_U_START                     (17)
#define DU_RDBIF_CON_STATE_U_END                       (17)
#define DU_RDBIF_CON_STATE_V_START                     (18)
#define DU_RDBIF_CON_STATE_V_END                       (18)
#define DU_RDBIF_CON_SYNC_STATE_START                  (19)
#define DU_RDBIF_CON_SYNC_STATE_END                    (19)
#define DU_RDBIF_FMC_Y_STATE_START                     (20)
#define DU_RDBIF_FMC_Y_STATE_END                       (20)
#define DU_RDBIF_FMC_UV_STATE_START                    (21)
#define DU_RDBIF_FMC_UV_STATE_END                      (21)
#define DU_RDBIF_FMC_SYNC_STATE_START                  (22)
#define DU_RDBIF_FMC_SYNC_STATE_END                    (22)
#define DU_SCL_VSC_WR_STATE_START                      (24)
#define DU_SCL_VSC_WR_STATE_END                        (24)
#define DU_SCL_VSC_RD_STATE_START                      (25)
#define DU_SCL_VSC_RD_STATE_END                        (25)
#define DU_SCL_HSC_WR_STATE_START                      (26)
#define DU_SCL_HSC_WR_STATE_END                        (26)
#define DU_SCL_HSC_RD_STATE_START                      (27)
#define DU_SCL_HSC_RD_STATE_END                        (27)

/* vid1_wb_cfg0 */
#define DU_WB_EN_START                                 (0)
#define DU_WB_EN_END                                   (0)
#define DU_WB_ALPHA_REG_EN_START                       (1)
#define DU_WB_ALPHA_REG_EN_END                         (1)
#define DU_WB_BUF_SEL_START                            (4)
#define DU_WB_BUF_SEL_END                              (4)
#define DU_WB_SYNC_SEL_START                           (9)
#define DU_WB_SYNC_SEL_END                             (8)
#define DU_WB_REG_SW_UPDATE_START                      (12)
#define DU_WB_REG_SW_UPDATE_END                        (12)

/* vid1_wb_cfg1 */
#define DU_WB_FORMAT_START                             (3)
#define DU_WB_FORMAT_END                               (0)
#define DU_WB_BYTE_SWAP_START                          (5)
#define DU_WB_BYTE_SWAP_END                            (4)
#define DU_WB_WORD_SWAP_START                          (6)
#define DU_WB_WORD_SWAP_END                            (6)
#define DU_WB_ALPHA_SWAP_START                         (7)
#define DU_WB_ALPHA_SWAP_END                           (7)
#define DU_WB_RGB_SWAP_START                           (10)
#define DU_WB_RGB_SWAP_END                             (8)
#define DU_WB_YUV_SWAP_START                           (11)
#define DU_WB_YUV_SWAP_END                             (11)
#define DU_WB_SRC_SEL_START                            (16)
#define DU_WB_SRC_SEL_END                              (16)

/* vid1_wb_alpha_value */
#define DU_WB_ALPHA_START                              (7)
#define DU_WB_ALPHA_END                                (0)

/* vid1_wb_width_height */
#define DU_WB_WIDTH_START                              (12)
#define DU_WB_WIDTH_END                                (0)
#define DU_WB_HEIGHT_START                             (28)
#define DU_WB_HEIGHT_END                               (16)

/* vid1_wb_startxy */
#define DU_WB_SX_START                                 (12)
#define DU_WB_SX_END                                   (0)
#define DU_WB_SY_START                                 (28)
#define DU_WB_SY_END                                   (16)

/* vid1_wb_sizexy */
#define DU_WB_SIZEX_START                              (12)
#define DU_WB_SIZEX_END                                (0)
#define DU_WB_SIZEY_START                              (28)
#define DU_WB_SIZEY_END                                (16)

/* vid1_wb_dss_base_y_addr0 */
#define DU_WB_ST_ADDR_Y0_START                         (31)
#define DU_WB_ST_ADDR_Y0_END                           (0)

/* vid1_wb_dss_base_u_addr0 */
#define DU_WB_ST_ADDR_U0_START                         (31)
#define DU_WB_ST_ADDR_U0_END                           (0)

/* vid1_wb_dss_base_y_addr1 */
#define DU_WB_ST_ADDR_Y1_START                         (31)
#define DU_WB_ST_ADDR_Y1_END                           (0)

/* vid1_wb_dss_base_u_addr1 */
#define DU_WB_ST_ADDR_U1_START                         (31)
#define DU_WB_ST_ADDR_U1_END                           (0)

/* vid1_wb_status */
#define DU_WRCTRL_FULL_INF_FIFOY_START                 (0)
#define DU_WRCTRL_FULL_INF_FIFOY_END                   (0)
#define DU_WRCTRL_FULL_INF_FIFOU_START                 (1)
#define DU_WRCTRL_FULL_INF_FIFOU_END                   (1)
#define DU_WRCTRL_FULL_INF_FIFOV_START                 (2)
#define DU_WRCTRL_FULL_INF_FIFOV_END                   (2)
#define DU_WRCTRL_EMPTY_INF_FIFOY_START                (4)
#define DU_WRCTRL_EMPTY_INF_FIFOY_END                  (4)
#define DU_WRCTRL_EMPTY_INF_FIFOU_START                (5)
#define DU_WRCTRL_EMPTY_INF_FIFOU_END                  (5)
#define DU_WRCTRL_EMPTY_INF_FIFOV_START                (6)
#define DU_WRCTRL_EMPTY_INF_FIFOV_END                  (6)
#define DU_WRCTRL_CON_Y_STATE_START                    (8)
#define DU_WRCTRL_CON_Y_STATE_END                      (8)
#define DU_WRCTRL_CON_U_STATE_START                    (9)
#define DU_WRCTRL_CON_U_STATE_END                      (9)
#define DU_WRCTRL_CON_V_STATE_START                    (10)
#define DU_WRCTRL_CON_V_STATE_END                      (10)
#define DU_WRCTRL_CON_SYNC_STATE_START                 (11)
#define DU_WRCTRL_CON_SYNC_STATE_END                   (11)
#define DU_WRMIF_FULL_CMDFIFO_START                    (12)
#define DU_WRMIF_FULL_CMDFIFO_END                      (12)
#define DU_WRMIF_FULL_DATFIFO_START                    (13)
#define DU_WRMIF_FULL_DATFIFO_END                      (13)
#define DU_WRMIF_EMPTY_CMDFIFO_START                   (16)
#define DU_WRMIF_EMPTY_CMDFIFO_END                     (16)
#define DU_WRMIF_EMPTY_DATFIFO_START                   (17)
#define DU_WRMIF_EMPTY_DATFIFO_END                     (17)

/* vid1_poly_hcoef_12t_0p0 */
#define DU_PP_0P_M5_HCOEF_START                        (8)
#define DU_PP_0P_M5_HCOEF_END                          (0)
#define DU_PP_0P_M4_HCOEF_START                        (17)
#define DU_PP_0P_M4_HCOEF_END                          (9)
#define DU_PP_0P_M3_HCOEF_START                        (26)
#define DU_PP_0P_M3_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_0p1 */
#define DU_PP_0P_M2_HCOEF_START                        (8)
#define DU_PP_0P_M2_HCOEF_END                          (0)
#define DU_PP_0P_M1_HCOEF_START                        (17)
#define DU_PP_0P_M1_HCOEF_END                          (9)
#define DU_PP_0P_P0_HCOEF_START                        (26)
#define DU_PP_0P_P0_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_0p2 */
#define DU_PP_0P_P1_HCOEF_START                        (8)
#define DU_PP_0P_P1_HCOEF_END                          (0)
#define DU_PP_0P_P2_HCOEF_START                        (17)
#define DU_PP_0P_P2_HCOEF_END                          (9)
#define DU_PP_0P_P3_HCOEF_START                        (26)
#define DU_PP_0P_P3_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_0p3 */
#define DU_PP_0P_P4_HCOEF_START                        (8)
#define DU_PP_0P_P4_HCOEF_END                          (0)
#define DU_PP_0P_P5_HCOEF_START                        (17)
#define DU_PP_0P_P5_HCOEF_END                          (9)
#define DU_PP_0P_P6_HCOEF_START                        (26)
#define DU_PP_0P_P6_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_1p0 */
#define DU_PP_1P_M5_HCOEF_START                        (8)
#define DU_PP_1P_M5_HCOEF_END                          (0)
#define DU_PP_1P_M4_HCOEF_START                        (17)
#define DU_PP_1P_M4_HCOEF_END                          (9)
#define DU_PP_1P_M3_HCOEF_START                        (26)
#define DU_PP_1P_M3_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_1p1 */
#define DU_PP_1P_M2_HCOEF_START                        (8)
#define DU_PP_1P_M2_HCOEF_END                          (0)
#define DU_PP_1P_M1_HCOEF_START                        (17)
#define DU_PP_1P_M1_HCOEF_END                          (9)
#define DU_PP_1P_P0_HCOEF_START                        (26)
#define DU_PP_1P_P0_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_1p2 */
#define DU_PP_1P_P1_HCOEF_START                        (8)
#define DU_PP_1P_P1_HCOEF_END                          (0)
#define DU_PP_1P_P2_HCOEF_START                        (17)
#define DU_PP_1P_P2_HCOEF_END                          (9)
#define DU_PP_1P_P3_HCOEF_START                        (26)
#define DU_PP_1P_P3_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_1p3 */
#define DU_PP_1P_P4_HCOEF_START                        (8)
#define DU_PP_1P_P4_HCOEF_END                          (0)
#define DU_PP_1P_P5_HCOEF_START                        (17)
#define DU_PP_1P_P5_HCOEF_END                          (9)
#define DU_PP_1P_P6_HCOEF_START                        (26)
#define DU_PP_1P_P6_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_2p0 */
#define DU_PP_2P_M5_HCOEF_START                        (8)
#define DU_PP_2P_M5_HCOEF_END                          (0)
#define DU_PP_2P_M4_HCOEF_START                        (17)
#define DU_PP_2P_M4_HCOEF_END                          (9)
#define DU_PP_2P_M3_HCOEF_START                        (26)
#define DU_PP_2P_M3_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_2p1 */
#define DU_PP_2P_M2_HCOEF_START                        (8)
#define DU_PP_2P_M2_HCOEF_END                          (0)
#define DU_PP_2P_M1_HCOEF_START                        (17)
#define DU_PP_2P_M1_HCOEF_END                          (9)
#define DU_PP_2P_P0_HCOEF_START                        (26)
#define DU_PP_2P_P0_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_2p2 */
#define DU_PP_2P_P1_HCOEF_START                        (8)
#define DU_PP_2P_P1_HCOEF_END                          (0)
#define DU_PP_2P_P2_HCOEF_START                        (17)
#define DU_PP_2P_P2_HCOEF_END                          (9)
#define DU_PP_2P_P3_HCOEF_START                        (26)
#define DU_PP_2P_P3_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_2p3 */
#define DU_PP_2P_P4_HCOEF_START                        (8)
#define DU_PP_2P_P4_HCOEF_END                          (0)
#define DU_PP_2P_P5_HCOEF_START                        (17)
#define DU_PP_2P_P5_HCOEF_END                          (9)
#define DU_PP_2P_P6_HCOEF_START                        (26)
#define DU_PP_2P_P6_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_3p0 */
#define DU_PP_3P_M5_HCOEF_START                        (8)
#define DU_PP_3P_M5_HCOEF_END                          (0)
#define DU_PP_3P_M4_HCOEF_START                        (17)
#define DU_PP_3P_M4_HCOEF_END                          (9)
#define DU_PP_3P_M3_HCOEF_START                        (26)
#define DU_PP_3P_M3_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_3p1 */
#define DU_PP_3P_M2_HCOEF_START                        (8)
#define DU_PP_3P_M2_HCOEF_END                          (0)
#define DU_PP_3P_M1_HCOEF_START                        (17)
#define DU_PP_3P_M1_HCOEF_END                          (9)
#define DU_PP_3P_P0_HCOEF_START                        (26)
#define DU_PP_3P_P0_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_3p2 */
#define DU_PP_3P_P1_HCOEF_START                        (8)
#define DU_PP_3P_P1_HCOEF_END                          (0)
#define DU_PP_3P_P2_HCOEF_START                        (17)
#define DU_PP_3P_P2_HCOEF_END                          (9)
#define DU_PP_3P_P3_HCOEF_START                        (26)
#define DU_PP_3P_P3_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_3p3 */
#define DU_PP_3P_P4_HCOEF_START                        (8)
#define DU_PP_3P_P4_HCOEF_END                          (0)
#define DU_PP_3P_P5_HCOEF_START                        (17)
#define DU_PP_3P_P5_HCOEF_END                          (9)
#define DU_PP_3P_P6_HCOEF_START                        (26)
#define DU_PP_3P_P6_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_4p0 */
#define DU_PP_4P_M5_HCOEF_START                        (8)
#define DU_PP_4P_M5_HCOEF_END                          (0)
#define DU_PP_4P_M4_HCOEF_START                        (17)
#define DU_PP_4P_M4_HCOEF_END                          (9)
#define DU_PP_4P_M3_HCOEF_START                        (26)
#define DU_PP_4P_M3_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_4p1 */
#define DU_PP_4P_M2_HCOEF_START                        (8)
#define DU_PP_4P_M2_HCOEF_END                          (0)
#define DU_PP_4P_M1_HCOEF_START                        (17)
#define DU_PP_4P_M1_HCOEF_END                          (9)
#define DU_PP_4P_P0_HCOEF_START                        (26)
#define DU_PP_4P_P0_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_4p2 */
#define DU_PP_4P_P1_HCOEF_START                        (8)
#define DU_PP_4P_P1_HCOEF_END                          (0)
#define DU_PP_4P_P2_HCOEF_START                        (17)
#define DU_PP_4P_P2_HCOEF_END                          (9)
#define DU_PP_4P_P3_HCOEF_START                        (26)
#define DU_PP_4P_P3_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_4p3 */
#define DU_PP_4P_P4_HCOEF_START                        (8)
#define DU_PP_4P_P4_HCOEF_END                          (0)
#define DU_PP_4P_P5_HCOEF_START                        (17)
#define DU_PP_4P_P5_HCOEF_END                          (9)
#define DU_PP_4P_P6_HCOEF_START                        (26)
#define DU_PP_4P_P6_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_5p0 */
#define DU_PP_5P_M5_HCOEF_START                        (8)
#define DU_PP_5P_M5_HCOEF_END                          (0)
#define DU_PP_5P_M4_HCOEF_START                        (17)
#define DU_PP_5P_M4_HCOEF_END                          (9)
#define DU_PP_5P_M3_HCOEF_START                        (26)
#define DU_PP_5P_M3_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_5p1 */
#define DU_PP_5P_M2_HCOEF_START                        (8)
#define DU_PP_5P_M2_HCOEF_END                          (0)
#define DU_PP_5P_M1_HCOEF_START                        (17)
#define DU_PP_5P_M1_HCOEF_END                          (9)
#define DU_PP_5P_P0_HCOEF_START                        (26)
#define DU_PP_5P_P0_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_5p2 */
#define DU_PP_5P_P1_HCOEF_START                        (8)
#define DU_PP_5P_P1_HCOEF_END                          (0)
#define DU_PP_5P_P2_HCOEF_START                        (17)
#define DU_PP_5P_P2_HCOEF_END                          (9)
#define DU_PP_5P_P3_HCOEF_START                        (26)
#define DU_PP_5P_P3_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_5p3 */
#define DU_PP_5P_P4_HCOEF_START                        (8)
#define DU_PP_5P_P4_HCOEF_END                          (0)
#define DU_PP_5P_P5_HCOEF_START                        (17)
#define DU_PP_5P_P5_HCOEF_END                          (9)
#define DU_PP_5P_P6_HCOEF_START                        (26)
#define DU_PP_5P_P6_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_6p0 */
#define DU_PP_6P_M5_HCOEF_START                        (8)
#define DU_PP_6P_M5_HCOEF_END                          (0)
#define DU_PP_6P_M4_HCOEF_START                        (17)
#define DU_PP_6P_M4_HCOEF_END                          (9)
#define DU_PP_6P_M3_HCOEF_START                        (26)
#define DU_PP_6P_M3_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_6p1 */
#define DU_PP_6P_M2_HCOEF_START                        (8)
#define DU_PP_6P_M2_HCOEF_END                          (0)
#define DU_PP_6P_M1_HCOEF_START                        (17)
#define DU_PP_6P_M1_HCOEF_END                          (9)
#define DU_PP_6P_P0_HCOEF_START                        (26)
#define DU_PP_6P_P0_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_6p2 */
#define DU_PP_6P_P1_HCOEF_START                        (8)
#define DU_PP_6P_P1_HCOEF_END                          (0)
#define DU_PP_6P_P2_HCOEF_START                        (17)
#define DU_PP_6P_P2_HCOEF_END                          (9)
#define DU_PP_6P_P3_HCOEF_START                        (26)
#define DU_PP_6P_P3_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_6p3 */
#define DU_PP_6P_P4_HCOEF_START                        (8)
#define DU_PP_6P_P4_HCOEF_END                          (0)
#define DU_PP_6P_P5_HCOEF_START                        (17)
#define DU_PP_6P_P5_HCOEF_END                          (9)
#define DU_PP_6P_P6_HCOEF_START                        (26)
#define DU_PP_6P_P6_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_7p0 */
#define DU_PP_7P_M5_HCOEF_START                        (8)
#define DU_PP_7P_M5_HCOEF_END                          (0)
#define DU_PP_7P_M4_HCOEF_START                        (17)
#define DU_PP_7P_M4_HCOEF_END                          (9)
#define DU_PP_7P_M3_HCOEF_START                        (26)
#define DU_PP_7P_M3_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_7p1 */
#define DU_PP_7P_M2_HCOEF_START                        (8)
#define DU_PP_7P_M2_HCOEF_END                          (0)
#define DU_PP_7P_M1_HCOEF_START                        (17)
#define DU_PP_7P_M1_HCOEF_END                          (9)
#define DU_PP_7P_P0_HCOEF_START                        (26)
#define DU_PP_7P_P0_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_7p2 */
#define DU_PP_7P_P1_HCOEF_START                        (8)
#define DU_PP_7P_P1_HCOEF_END                          (0)
#define DU_PP_7P_P2_HCOEF_START                        (17)
#define DU_PP_7P_P2_HCOEF_END                          (9)
#define DU_PP_7P_P3_HCOEF_START                        (26)
#define DU_PP_7P_P3_HCOEF_END                          (18)

/* vid1_poly_hcoef_12t_7p3 */
#define DU_PP_7P_P4_HCOEF_START                        (8)
#define DU_PP_7P_P4_HCOEF_END                          (0)
#define DU_PP_7P_P5_HCOEF_START                        (17)
#define DU_PP_7P_P5_HCOEF_END                          (9)
#define DU_PP_7P_P6_HCOEF_START                        (26)
#define DU_PP_7P_P6_HCOEF_END                          (18)

/* vid1_poly_vcoef_4t_0p0 */
#define DU_PP_0P_M1_VCOEF_START                        (8)
#define DU_PP_0P_M1_VCOEF_END                          (0)
#define DU_PP_0P_P0_VCOEF_START                        (17)
#define DU_PP_0P_P0_VCOEF_END                          (9)
#define DU_PP_0P_P1_VCOEF_START                        (26)
#define DU_PP_0P_P1_VCOEF_END                          (18)

/* vid1_poly_vcoef_4t_0p1 */
#define DU_PP_0P_P2_VCOEF_START                        (8)
#define DU_PP_0P_P2_VCOEF_END                          (0)

/* vid1_poly_vcoef_4t_1p0 */
#define DU_PP_1P_M1_VCOEF_START                        (8)
#define DU_PP_1P_M1_VCOEF_END                          (0)
#define DU_PP_1P_P0_VCOEF_START                        (17)
#define DU_PP_1P_P0_VCOEF_END                          (9)
#define DU_PP_1P_P1_VCOEF_START                        (26)
#define DU_PP_1P_P1_VCOEF_END                          (18)

/* vid1_poly_vcoef_4t_1p1 */
#define DU_PP_1P_P2_VCOEF_START                        (8)
#define DU_PP_1P_P2_VCOEF_END                          (0)

/* vid1_poly_vcoef_4t_2p0 */
#define DU_PP_2P_M1_VCOEF_START                        (8)
#define DU_PP_2P_M1_VCOEF_END                          (0)
#define DU_PP_2P_P0_VCOEF_START                        (17)
#define DU_PP_2P_P0_VCOEF_END                          (9)
#define DU_PP_2P_P1_VCOEF_START                        (26)
#define DU_PP_2P_P1_VCOEF_END                          (18)

/* vid1_poly_vcoef_4t_2p1 */
#define DU_PP_2P_P2_VCOEF_START                        (8)
#define DU_PP_2P_P2_VCOEF_END                          (0)

/* vid1_poly_vcoef_4t_3p0 */
#define DU_PP_3P_M1_VCOEF_START                        (8)
#define DU_PP_3P_M1_VCOEF_END                          (0)
#define DU_PP_3P_P0_VCOEF_START                        (17)
#define DU_PP_3P_P0_VCOEF_END                          (9)
#define DU_PP_3P_P1_VCOEF_START                        (26)
#define DU_PP_3P_P1_VCOEF_END                          (18)

/* vid1_poly_vcoef_4t_3p1 */
#define DU_PP_3P_P2_VCOEF_START                        (8)
#define DU_PP_3P_P2_VCOEF_END                          (0)

/* vid1_poly_vcoef_4t_4p0 */
#define DU_PP_4P_M1_VCOEF_START                        (8)
#define DU_PP_4P_M1_VCOEF_END                          (0)
#define DU_PP_4P_P0_VCOEF_START                        (17)
#define DU_PP_4P_P0_VCOEF_END                          (9)
#define DU_PP_4P_P1_VCOEF_START                        (26)
#define DU_PP_4P_P1_VCOEF_END                          (18)

/* vid1_poly_vcoef_4t_4p1 */
#define DU_PP_4P_P2_VCOEF_START                        (8)
#define DU_PP_4P_P2_VCOEF_END                          (0)

/* vid1_poly_vcoef_4t_5p0 */
#define DU_PP_5P_M1_VCOEF_START                        (8)
#define DU_PP_5P_M1_VCOEF_END                          (0)
#define DU_PP_5P_P0_VCOEF_START                        (17)
#define DU_PP_5P_P0_VCOEF_END                          (9)
#define DU_PP_5P_P1_VCOEF_START                        (26)
#define DU_PP_5P_P1_VCOEF_END                          (18)

/* vid1_poly_vcoef_4t_5p1 */
#define DU_PP_5P_P2_VCOEF_START                        (8)
#define DU_PP_5P_P2_VCOEF_END                          (0)

/* vid1_poly_vcoef_4t_6p0 */
#define DU_PP_6P_M1_VCOEF_START                        (8)
#define DU_PP_6P_M1_VCOEF_END                          (0)
#define DU_PP_6P_P0_VCOEF_START                        (17)
#define DU_PP_6P_P0_VCOEF_END                          (9)
#define DU_PP_6P_P1_VCOEF_START                        (26)
#define DU_PP_6P_P1_VCOEF_END                          (18)

/* vid1_poly_vcoef_4t_6p1 */
#define DU_PP_6P_P2_VCOEF_START                        (8)
#define DU_PP_6P_P2_VCOEF_END                          (0)

/* vid1_poly_vcoef_4t_7p0 */
#define DU_PP_7P_M1_VCOEF_START                        (8)
#define DU_PP_7P_M1_VCOEF_END                          (0)
#define DU_PP_7P_P0_VCOEF_START                        (17)
#define DU_PP_7P_P0_VCOEF_END                          (9)
#define DU_PP_7P_P1_VCOEF_START                        (26)
#define DU_PP_7P_P1_VCOEF_END                          (18)

/* vid1_poly_vcoef_4t_7p1 */
#define DU_PP_7P_P2_VCOEF_START                        (8)
#define DU_PP_7P_P2_VCOEF_END                          (0)

#endif

#endif

