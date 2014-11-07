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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
 * GNU General Public License for more details.
 */

#include "reg_types.h"

#ifndef GRA2_BASE_ADDR
#define GRA2_BASE_ADDR 0x31220000


#define ADDR_GRA2_RD_CFG0                              ( 0x0000 )
#define ADDR_GRA2_RD_CFG1                              ( 0x0004 )
#define ADDR_GRA2_RD_WIDTH_HEIGHT                      ( 0x0028 )
#define ADDR_GRA2_RD_STARTXY                           ( 0x002C )
#define ADDR_GRA2_RD_SIZEXY                            ( 0x0030 )
#define ADDR_GRA2_RD_DSS_BASE_Y_ADDR0                  ( 0x0040 )
#define ADDR_GRA2_RD_DSS_BASE_Y_ADDR1                  ( 0x004C )
#define ADDR_GRA2_RD_DSS_BASE_Y_ADDR2                  ( 0x0058 )
#define ADDR_GRA2_RD_STATUS                            ( 0x0064 )

#if 0 /* Re define */
/* gra2_rd_cfg0 */
#define DU_SRC_EN_START                                (0)
#define DU_SRC_EN_END                                  (0)
#define DU_SRC_DST_SEL_START                           (1)
#define DU_SRC_DST_SEL_END                             (1)
#define DU_SRC_BUF_SEL_START                           (5)
#define DU_SRC_BUF_SEL_END                             (4)
#define DU_SRC_TRANS_NUM_START                         (7)
#define DU_SRC_TRANS_NUM_END                           (6)
#define DU_SRC_SYNC_SEL_START                          (9)
#define DU_SRC_SYNC_SEL_END                            (8)
#define DU_DMA_INDEX_RD_START_START                    (10)
#define DU_DMA_INDEX_RD_START_END                      (10)
#define DU_SRC_REG_SW_UPDATE_START                     (12)
#define DU_SRC_REG_SW_UPDATE_END                       (12)

/* gra2_rd_cfg1 */
#define DU_SRC_FORMAT_START                            (2)
#define DU_SRC_FORMAT_END                              (0)
#define DU_SRC_BYTE_SWAP_START                         (5)
#define DU_SRC_BYTE_SWAP_END                           (4)
#define DU_SRC_WORD_SWAP_START                         (6)
#define DU_SRC_WORD_SWAP_END                           (6)
#define DU_SRC_ALPHA_SWAP_START                        (7)
#define DU_SRC_ALPHA_SWAP_END                          (7)
#define DU_SRC_RGB_SWAP_START                          (10)
#define DU_SRC_RGB_SWAP_END                            (8)
#define DU_SRC_LSB_SEL_START                           (14)
#define DU_SRC_LSB_SEL_END                             (14)
#define DU_SRC_FLIP_MODE_START                         (21)
#define DU_SRC_FLIP_MODE_END                           (20)

/* gra2_rd_width_height */
#define DU_SRC_WIDTH_START                             (12)
#define DU_SRC_WIDTH_END                               (0)
#define DU_SRC_HEIGHT_START                            (28)
#define DU_SRC_HEIGHT_END                              (16)

/* gra2_rd_startxy */
#define DU_SRC_SX_START                                (12)
#define DU_SRC_SX_END                                  (0)
#define DU_SRC_SY_START                                (28)
#define DU_SRC_SY_END                                  (16)

/* gra2_rd_sizexy */
#define DU_SRC_SIZEX_START                             (12)
#define DU_SRC_SIZEX_END                               (0)
#define DU_SRC_SIZEY_START                             (28)
#define DU_SRC_SIZEY_END                               (16)

/* gra2_rd_dss_base_y_addr0 */
#define DU_SRC_ST_ADDR_Y0_START                        (31)
#define DU_SRC_ST_ADDR_Y0_END                          (0)

/* gra2_rd_dss_base_y_addr1 */
#define DU_SRC_ST_ADDR_Y1_START                        (31)
#define DU_SRC_ST_ADDR_Y1_END                          (0)

/* gra2_rd_dss_base_y_addr2 */
#define DU_SRC_ST_ADDR_Y2_START                        (31)
#define DU_SRC_ST_ADDR_Y2_END                          (0)

/* gra2_rd_status */
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
#define DU_RDBIF_EMPTY_DATFIFO_Y_START                 (12)
#define DU_RDBIF_EMPTY_DATFIFO_Y_END                   (12)
#define DU_RDBIF_CON_STATE_START                       (16)
#define DU_RDBIF_CON_STATE_END                         (16)
#define DU_RDBIF_CON_SYNC_STATE_START                  (17)
#define DU_RDBIF_CON_SYNC_STATE_END                    (17)
#define DU_RDBIF_FMC_Y_STATE_START                     (20)
#define DU_RDBIF_FMC_Y_STATE_END                       (20)
#define DU_RDBIF_FMC_SYNC_STATE_START                  (22)
#define DU_RDBIF_FMC_SYNC_STATE_END                    (22)
#endif

#endif

