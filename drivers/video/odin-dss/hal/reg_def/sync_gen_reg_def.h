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

#ifndef SYNC_GEN_BASE_ADDR
#define SYNC_GEN_BASE_ADDR 0x31270000


#define ADDR_SYNC_CFG0                                 ( 0x0000 )
#define ADDR_SYNC_CFG1                                 ( 0x0004 )
#define ADDR_SYNC_CFG2                                 ( 0x0008 )
#define ADDR_SYNC_RASTER_SIZE0                         ( 0x0028 )
#define ADDR_SYNC_RASTER_SIZE1                         ( 0x002C )
#define ADDR_SYNC_RASTER_SIZE2                         ( 0x0030 )
#define ADDR_SYNC_OVL0_ST_DLY                          ( 0x0038 )
#define ADDR_SYNC_OVL1_ST_DLY                          ( 0x003C )
#define ADDR_SYNC_OVL2_ST_DLY                          ( 0x0040 )
#define ADDR_SYNC_VID0_ST_DLY0                         ( 0x0044 )
#define ADDR_SYNC_VID1_ST_DLY0                         ( 0x0048 )
#define ADDR_SYNC_VID2_ST_DLY                          ( 0x004C )
#define ADDR_SYNC_GRA0_ST_DLY                          ( 0x0050 )
#define ADDR_SYNC_GRA1_ST_DLY                          ( 0x0054 )
#define ADDR_SYNC_GRA2_ST_DLY                          ( 0x0058 )
#define ADDR_SYNC_GSCL_ST_DLY0                         ( 0x005C )
#define ADDR_SYNC_VID0_ST_DLY1                         ( 0x0060 )
#define ADDR_SYNC_VID1_ST_DLY1                         ( 0x0064 )
#define ADDR_SYNC_GSCL_ST_DLY1                         ( 0x0068 )
#define ADDR_SYNC0_ACT_SIZE                            ( 0x006c )
#define ADDR_SYNC1_ACT_SIZE                            ( 0x0070 )
#define ADDR_SYNC2_ACT_SIZE                            ( 0x0074 )

/* sync_cfg0 */
#define DU_SYNC0_ENABLE_START                          (0)
#define DU_SYNC0_ENABLE_END                            (0)
#define DU_SYNC1_ENABLE_START                          (1)
#define DU_SYNC1_ENABLE_END                            (1)
#define DU_SYNC2_ENABLE_START                          (2)
#define DU_SYNC2_ENABLE_END                            (2)
#define DU_SYNC0_MODE_START                            (4)
#define DU_SYNC0_MODE_END                              (4)
#define DU_SYNC1_MODE_START                            (5)
#define DU_SYNC1_MODE_END                              (5)
#define DU_SYNC2_MODE_START                            (6)
#define DU_SYNC2_MODE_END                              (6)
#define DU_SYNC0_INT_ENA_START                         (8)
#define DU_SYNC0_INT_ENA_END                           (8)
#define DU_SYNC1_INT_ENA_START                         (9)
#define DU_SYNC1_INT_ENA_END                           (9)
#define DU_SYNC2_INT_ENA_START                         (10)
#define DU_SYNC2_INT_ENA_END                           (10)
#define DU_SYNC0_INT_SRC_SEL_START                     (12)
#define DU_SYNC0_INT_SRC_SEL_END                       (12)
#define DU_SYNC1_INT_SRC_SEL_START                     (13)
#define DU_SYNC1_INT_SRC_SEL_END                       (13)
#define DU_SYNC2_INT_SRC_SEL_START                     (14)
#define DU_SYNC2_INT_SRC_SEL_END                       (14)
#define DU_SYNC0_DISP_SYNC_SRC_SEL_START               (16)
#define DU_SYNC0_DISP_SYNC_SRC_SEL_END                 (16)
#define DU_SYNC1_DISP_SYNC_SRC_SEL_START               (17)
#define DU_SYNC1_DISP_SYNC_SRC_SEL_END                 (17)
#define DU_SYNC2_DISP_SYNC_SRC_SEL_START               (18)
#define DU_SYNC2_DISP_SYNC_SRC_SEL_END                 (18)

/* sync_cfg1 */
#define DU_OVL0_SYNC_EN_START                          (0)
#define DU_OVL0_SYNC_EN_END                            (0)
#define DU_OVL1_SYNC_EN_START                          (1)
#define DU_OVL1_SYNC_EN_END                            (1)
#define DU_OVL2_SYNC_EN_START                          (2)
#define DU_OVL2_SYNC_EN_END                            (2)
#define DU_VID0_SYNC_EN0_START                         (3)
#define DU_VID0_SYNC_EN0_END                           (3)
#define DU_VID1_SYNC_EN0_START                         (4)
#define DU_VID1_SYNC_EN0_END                           (4)
#define DU_VID2_SYNC_EN_START                          (5)
#define DU_VID2_SYNC_EN_END                            (5)
#define DU_GRA0_SYNC_EN_START                          (6)
#define DU_GRA0_SYNC_EN_END                            (6)
#define DU_GRA1_SYNC_EN_START                          (7)
#define DU_GRA1_SYNC_EN_END                            (7)
#define DU_GRA2_SYNC_EN_START                          (8)
#define DU_GRA2_SYNC_EN_END                            (8)
#define DU_GSCL_SYNC_EN0_START                         (9)
#define DU_GSCL_SYNC_EN0_END                           (9)

/* sync_cfg2 */
#define DU_VID0_SYNC_EN1_START                         (3)
#define DU_VID0_SYNC_EN1_END                           (3)
#define DU_VID1_SYNC_EN1_START                         (4)
#define DU_VID1_SYNC_EN1_END                           (4)
#define DU_GSCL_SYNC_EN1_START                         (9)
#define DU_GSCL_SYNC_EN1_END                           (9)

/* sync_raster_size0 */
#define DU_SYNC0_TOT_HSIZE_START                       (12)
#define DU_SYNC0_TOT_HSIZE_END                         (0)
#define DU_SYNC0_TOT_VSIZE_START                       (28)
#define DU_SYNC0_TOT_VSIZE_END                         (16)

/* sync_raster_size1 */
#define DU_SYNC1_TOT_HSIZE_START                       (12)
#define DU_SYNC1_TOT_HSIZE_END                         (0)
#define DU_SYNC1_TOT_VSIZE_START                       (28)
#define DU_SYNC1_TOT_VSIZE_END                         (16)

/* sync_raster_size2 */
#define DU_SYNC2_TOT_HSIZE_START                       (12)
#define DU_SYNC2_TOT_HSIZE_END                         (0)
#define DU_SYNC2_TOT_VSIZE_START                       (28)
#define DU_SYNC2_TOT_VSIZE_END                         (16)

/* sync_ovl0_st_dly */
#define DU_OVL0_H_ST_VAL_START                         (12)
#define DU_OVL0_H_ST_VAL_END                           (0)
#define DU_OVL0_V_ST_VAL_START                         (28)
#define DU_OVL0_V_ST_VAL_END                           (16)

/* sync_ovl1_st_dly */
#define DU_OVL1_H_ST_VAL_START                         (12)
#define DU_OVL1_H_ST_VAL_END                           (0)
#define DU_OVL1_V_ST_VAL_START                         (28)
#define DU_OVL1_V_ST_VAL_END                           (16)

/* sync_ovl2_st_dly */
#define DU_OVL2_H_ST_VAL_START                         (12)
#define DU_OVL2_H_ST_VAL_END                           (0)
#define DU_OVL2_V_ST_VAL_START                         (28)
#define DU_OVL2_V_ST_VAL_END                           (16)

/* sync_vid0_st_dly0 */
#define DU_VID0_H_ST_VAL0_START                        (12)
#define DU_VID0_H_ST_VAL0_END                          (0)
#define DU_VID0_V_ST_VAL0_START                        (28)
#define DU_VID0_V_ST_VAL0_END                          (16)

/* sync_vid1_st_dly0 */
#define DU_VID1_H_ST_VAL0_START                        (12)
#define DU_VID1_H_ST_VAL0_END                          (0)
#define DU_VID1_V_ST_VAL0_START                        (28)
#define DU_VID1_V_ST_VAL0_END                          (16)

/* sync_vid2_st_dly */
#define DU_VID2_H_ST_VAL_START                         (12)
#define DU_VID2_H_ST_VAL_END                           (0)
#define DU_VID2_V_ST_VAL_START                         (28)
#define DU_VID2_V_ST_VAL_END                           (16)

/* sync_gra0_st_dly */
#define DU_GRA0_H_ST_VAL_START                         (12)
#define DU_GRA0_H_ST_VAL_END                           (0)
#define DU_GRA0_V_ST_VAL_START                         (28)
#define DU_GRA0_V_ST_VAL_END                           (16)

/* sync_gra1_st_dly */
#define DU_GRA1_H_ST_VAL_START                         (12)
#define DU_GRA1_H_ST_VAL_END                           (0)
#define DU_GRA1_V_ST_VAL_START                         (28)
#define DU_GRA1_V_ST_VAL_END                           (16)

/* sync_gra2_st_dly */
#define DU_GRA2_H_ST_VAL_START                         (12)
#define DU_GRA2_H_ST_VAL_END                           (0)
#define DU_GRA2_V_ST_VAL_START                         (28)
#define DU_GRA2_V_ST_VAL_END                           (16)

/* sync_gscl_st_dly0 */
#define DU_GSCL_H_ST_VAL0_START                        (12)
#define DU_GSCL_H_ST_VAL0_END                          (0)
#define DU_GSCL_V_ST_VAL0_START                        (28)
#define DU_GSCL_V_ST_VAL0_END                          (16)

/* sync_vid0_st_dly1 */
#define DU_VID0_H_ST_VAL1_START                        (12)
#define DU_VID0_H_ST_VAL1_END                          (0)
#define DU_VID0_V_ST_VAL1_START                        (28)
#define DU_VID0_V_ST_VAL1_END                          (16)

/* sync_vid1_st_dly1 */
#define DU_VID1_H_ST_VAL1_START                        (12)
#define DU_VID1_H_ST_VAL1_END                          (0)
#define DU_VID1_V_ST_VAL1_START                        (28)
#define DU_VID1_V_ST_VAL1_END                          (16)

/* sync_gscl_st_dly1 */
#define DU_GSCL_H_ST_VAL1_START                        (12)
#define DU_GSCL_H_ST_VAL1_END                          (0)
#define DU_GSCL_V_ST_VAL1_START                        (28)
#define DU_GSCL_V_ST_VAL1_END                          (16)

/* sync0_act_size */
#define DU_SYNC0_ACT_HSIZE_START                       (12)
#define DU_SYNC0_ACT_HSIZE_END                         (0)
#define DU_SYNC0_ACT_VSIZE_START                       (28)
#define DU_SYNC0_ACT_VSIZE_END                         (16)

/* sync1_act_size */
#define DU_SYNC1_ACT_HSIZE_START                       (12)
#define DU_SYNC1_ACT_HSIZE_END                         (0)
#define DU_SYNC1_ACT_VSIZE_START                       (28)
#define DU_SYNC1_ACT_VSIZE_END                         (16)

/* sync2_act_size */
#define DU_SYNC2_ACT_HSIZE_START                       (12)
#define DU_SYNC2_ACT_HSIZE_END                         (0)
#define DU_SYNC2_ACT_VSIZE_START                       (28)
#define DU_SYNC2_ACT_VSIZE_END                         (16)


#endif

