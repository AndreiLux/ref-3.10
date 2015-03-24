#ifdef CONFIG_MTK_INTERNAL_MHL_SUPPORT

#ifndef _MT6397_CBUS_REG_H_
#define _MT6397_CBUS_REG_H_

#define CBUS_BASE                                              (0)
#define CBUS_WBUF0                                            (CBUS_BASE + 0x0A00)
#define CBUS_WBUF1                                            (CBUS_BASE + 0x0A04)
#define CBUS_WBUF2                                            (CBUS_BASE + 0x0A08)
#define CBUS_WBUF3                                            (CBUS_BASE + 0x0A0C)
#define CBUS_WBUF4                                            (CBUS_BASE + 0x0A10)
#define CBUS_WBUF5                                            (CBUS_BASE + 0x0A14)
#define CBUS_WBUF6                                            (CBUS_BASE + 0x0A18)
#define CBUS_WBUF7                                            (CBUS_BASE + 0x0A1C)
#define CBUS_WBUF8                                            (CBUS_BASE + 0x0A20)
#define CBUS_WBUF9                                            (CBUS_BASE + 0x0A24)
#define CBUS_WBUF10                                           (CBUS_BASE + 0x0A28)
#define CBUS_WBUF11                                           (CBUS_BASE + 0x0A2C)
#define CBUS_RBUF                                             (CBUS_BASE + 0x0A30)
#define CBUS_LINK_00                                          (CBUS_BASE + 0x0A34)
#define CBUS_LINK_01                                          (CBUS_BASE + 0x0A38)
#define CBUS_LINK_02                                          (CBUS_BASE + 0x0A3C)
#define CBUS_LINK_03                                          (CBUS_BASE + 0x0A40)
#define CBUS_LINK_04                                          (CBUS_BASE + 0x0A44)
#define CBUS_LINK_05                                          (CBUS_BASE + 0x0A48)
#define CBUS_LINK_06                                          (CBUS_BASE + 0x0A4C)
#define CBUS_LINK_07                                          (CBUS_BASE + 0x0A50)
#define CBUS_LINK_08                                          (CBUS_BASE + 0x0A54)
#define CBUS_LINK_09                                          (CBUS_BASE + 0x0A58)
#define CBUS_LINK_0A                                          (CBUS_BASE + 0x0A5C)
#define CBUS_LINK_0B                                          (CBUS_BASE + 0x0A60)
#define CBUS_LINK_0C                                          (CBUS_BASE + 0x0A64)
#define CBUS_LINK_0D                                          (CBUS_BASE + 0x0A68)
#define CBUS_LINK_STA_00                                      (CBUS_BASE + 0x0A6C)
#define CBUS_LINK_STA_01                                      (CBUS_BASE + 0x0A70)
#define CBUS_LINK_BAK                                         (CBUS_BASE + 0x0A74)
#define CBUS_LINK_BAK1                                        (CBUS_BASE + 0x0A78)

#define WBUF0_CBUS_WBUF1                                   16
#define WBUF0_CBUS_WBUF0                                   0

#define WBUF1_CBUS_WBUF3                                   16
#define WBUF1_CBUS_WBUF2                                   0

#define WBUF2_CBUS_WBUF5                                   16
#define WBUF2_CBUS_WBUF4                                   0

#define WBUF3_CBUS_WBUF7                                   16
#define WBUF3_CBUS_WBUF6                                   0

#define WBUF4_CBUS_WBUF9                                   16
#define WBUF4_CBUS_WBUF8                                   0

#define WBUF5_CBUS_WBUF11                                  16
#define WBUF5_CBUS_WBUF10                                  0

#define WBUF6_CBUS_WBUF13                                  16
#define WBUF6_CBUS_WBUF12                                  0

#define WBUF7_CBUS_WBUF15                                  16
#define WBUF7_CBUS_WBUF14                                  0

#define WBUF8_CBUS_WBUF17                                  16
#define WBUF8_CBUS_WBUF16                                  0

#define WBUF9_CBUS_WBUF19                                  16
#define WBUF9_CBUS_WBUF18                                  0

#define WBUF10_CBUS_WBUF21                                 16
#define WBUF10_CBUS_WBUF20                                 0

#define WBUF11_CBUS_WBUF23                                 16
#define WBUF11_CBUS_WBUF22                                 0

#define RBUF_CBUS_RBUF                                     0

#define LINK_00_TX_ARB_BYPASS                              31
#define LINK_00_LINK_TXDECISION                            24
#define LINK_00_NRETRY                                     18
#define LINK_00_TRESP_HOLD                                 14
#define LINK_00_TX_TWAIT                                   10
#define LINK_00_TREQ_HOLD                                  6
#define LINK_00_TX_NUM                                     1
#define LINK_00_TX_TRIG                                    0

#define LINK_01_LINKTX_BITIME                              21
#define LINK_01_CBUS_ACK_0_MIN                             14
#define LINK_01_CBUS_ACK_0_MAX                             7
#define LINK_01_CBUS_ACK_FALL_MAX                          0

#define LINK_02_LINKRX_EN                                  31
#define LINK_02_ADP_BITIME_EN                              30
#define LINK_02_ADP_BITIME_RST                             29
#define LINK_02_ADP_CONFIG1                                28
#define LINK_02_ADP_CONFIG2                                27
#define LINK_02_ADP_CONFIG3                                26
#define LINK_02_ADP_CONFIG4                                25
#define LINK_02_ADP_CONFIG5                                24
#define LINK_02_ADP_CONFIG6                                23
#define LINK_02_LINK_ACK_MANU_EN                           22
#define LINK_02_LINKRX_DIS_TO_ARB23                        21
#define LINK_02_TREQ_CONT                                  14
#define LINK_02_LINK_HALFTRAN_MAX                          7
#define LINK_02_LINK_HALFTRAN_MIN                          0

#define LINK_03_LINKRX_BITIME_MAX                          21
#define LINK_03_LINKRX_BITIME_MIN                          14
#define LINK_03_LINK_SYNCDUTY_MAX                          7
#define LINK_03_LINK_SYNCDUTY_MIN                          0

#define LINK_04_RBUF_LVL_THR                               27
#define LINK_04_IGNORE_PAR                                 26
#define LINK_04_LINK_ACK_WIDTH                             14
#define LINK_04_LINK_RXDECISION                            7
#define LINK_04_LINKRX_BT_TIMEOUT                          0

#define LINK_05_DISCOVERY_EN                               31
#define LINK_05_WAKEUP_EN                                  30
#define LINK_05_WAKEUP_DONE                                29
#define LINK_05_DISCOVER_DET                               28
#define LINK_05_WAKEUP_OUT_LOW_EN                          27
#define LINK_05_WAKEUP_RETRY                               26
#define LINK_05_WAKEUP_SPEED                               24
#define LINK_05_WAKE_TO_DISC                               16
#define LINK_05_WAKE_PUL_WID2                              8
#define LINK_05_WAKE_PUL_WID1                              0

#define LINK_06_DISCOVER_CNT                               24
#define LINK_06_DISC_PUL_WID                               16
#define LINK_06_DISC_WID_CNT_TIMEOUT                       8
#define LINK_06_VBUS_CBUS_STABLE                           0

#define LINK_07_VBUS_DEGLITCH                              31
#define LINK_07_CBUS_DEGLITCH                              30
#define LINK_07_CBUS_OE                                    29
#define LINK_07_CBUS_DO                                    28
#define LINK_07_CBUS_DOE_SW                                27
#define LINK_07_TMDS_RX_SENSE                              26
#define LINK_07_ZCBUS_DET_EN_SW                            25
#define LINK_07_ZCBUS_DET_EN_HW                            24
#define LINK_07_DETECT_AGAIN                               23
#define LINK_07_CD_SENSE_DEGLITCH                          22
#define LINK_07_ZCBUS_SRC_ON_10K                           19
#define LINK_07_ZCBUS_SRC_ON_10K_SW                        18
#define LINK_07_MHL_MODE                                   17
#define LINK_07_MHL_MODE_SW                                16
#define LINK_07_VBUS_OE                                    15
#define LINK_07_VBUS_DO                                    14
#define LINK_07_VBUS_DOE_SW                                13
#define LINK_07_ZCBUS_SRC_ON_5K                            12
#define LINK_07_ZCBUS_SRC_ON_5K_SW                         11
#define LINK_07_ZCBUS_SRC_ON_100                           10
#define LINK_07_ZCBUS_SRC_ON_100_SW                        9
#define LINK_07_CBUS_SENSE_SMT_SW                          7
#define LINK_07_CBUS_SENSE_IE_SW                           6
#define LINK_07_CBUS_SENSE_SMT                             5
#define LINK_07_CBUS_SENSE_IE                              4
#define LINK_07_BYPASS_DET                                 2
#define LINK_07_CBUS_PULL_DOWN_SW                          1
#define LINK_07_CBUS_PULL_DOWN                             0

#define LINK_08_INT_MASK                                   16
#define LINK_08_VBUS_POS_INT_CLR                           15
#define LINK_08_LINKRX_TIMEOUT_INT_CLR                     14
#define LINK_08_WBUF_TRIG_INT_CLR                          13
#define LINK_08_RBUF_TRIG_INT_CLR                          12
#define LINK_08_ZCBUS_DET_DONE_INT_CLR                     11
#define LINK_08_CBUS_NEG_INT_CLR                           10
#define LINK_08_CBUS_POS_INT_CLR                           9
#define LINK_08_MONITOR_CMP_INT_CLR                        8
#define LINK_08_TMDS_RX_SENSE_DISCONN_INT_CLR              7
#define LINK_08_ZCBUS_DET_1K_INT_CLR                       6
#define LINK_08_TX_ARB_FAIL_INT_CLR                        5
#define LINK_08_TX_OK_INT_CLR                              4
#define LINK_08_TX_RETRY_TO_INT_CLR                        3
#define LINK_08_RBUF_LVL_THR_INT_CLR                       2
#define LINK_08_DISC_DET_FAIL_INT_CLR                      1
#define LINK_08_DISC_DET_INT_CLR                           0

#define LINK_09_SW_RESET_MISC                              31
#define LINK_09_SW_RESET_WAKE                              30
#define LINK_09_SW_RESET_RX                                29
#define LINK_09_SW_RESET_TX                                28
#define LINK_09_CD_SENSE_DET_TIME                          20
#define LINK_09_CBUS_OE_FAST                               17
#define LINK_09_DUPLEX                                     16
#define LINK_09_CBUS_DRV_H_PRD                             8
#define LINK_09_LINKTX_BITIME_MID                          0

#define LINK_0A_INT_STA_MASK                               16
#define LINK_0A_MON_CMP_SEL                                8
#define LINK_0A_MON_CMP_DATA                               0

#define LINK_0B_TREQ_OPP                                   16
#define LINK_0B_LINKRX_ACK1_SYNC                           8
#define LINK_0B_LINKRX_DIS_TO                              7
#define LINK_0B_RX_ARB_BYPASS                              6
#define LINK_0B_FORCE_NAK_EN                               5
#define LINK_0B_FORCE_NAK_CNT                              0

#define LINK_0C_RX_TWAIT                                   28
#define LINK_0C_LINK_ACK_WIDTH_UPPER                       21
#define LINK_0C_LINKRX_BITIME                              14
#define LINK_0C_ADP_BITIME_MIN                             7
#define LINK_0C_ADP_BITIME_MAX                             0

#define LINK_0D_REG_CBUS_LINK_0D                           0

#define LINK_STA_00_LINK_BITIME                            25
#define LINK_STA_00_RBUF_LVL_LAT                           20
#define LINK_STA_00_CD_SENSE_DGT                           19
#define LINK_STA_00_CBUS_DET_1K                            18
#define LINK_STA_00_CBUS_DET_10                            17
#define LINK_STA_00_CBUS_DET_100K                          16
#define LINK_STA_00_VBUS_POS_INT                           15
#define LINK_STA_00_LINKRX_TIMEOUT_INT                     14
#define LINK_STA_00_WBUF_TRIG_INT                          13
#define LINK_STA_00_RBUF_TRIG_INT                          12
#define LINK_STA_00_ZCBUS_DET_DONE_INT                     11
#define LINK_STA_00_CBUS_NEG_INT                           10
#define LINK_STA_00_CBUS_POS_INT                           9
#define LINK_STA_00_MONITOR_CMP_INT                        8
#define LINK_STA_00_TMDS_RX_SENSE_DISCONN_INT              7
#define LINK_STA_00_ZCBUS_DET_1K_INT                       6
#define LINK_STA_00_TX_ARB_FAIL_INT                        5
#define LINK_STA_00_TX_OK_INT                              4
#define LINK_STA_00_TX_RETRY_TO_INT                        3
#define LINK_STA_00_RBUF_LVL_THR_INT                       2
#define LINK_STA_00_DISC_DET_FAIL_INT                      1
#define LINK_STA_00_DISC_DET_INT                           0

#define LINK_STA_01_CBUS_IN_DGT                            31
#define LINK_STA_01_VBUS_IN_DGT                            30
#define LINK_STA_01_LINKRX_FSM                             12
#define LINK_STA_01_LINKTX_FSM                             7
#define LINK_STA_01_DISC_FSM                               4
#define LINK_STA_01_WAKEUP_FSM                             0

#define LINK_BAK_REG_CBUS_LINK_BAK                         0

#define LINK_BAK1_REG_CBUS_LINK_BAK1                       0

#define WBUF0_CBUS_WBUF1_MASK          (0X7FF << 16)
#define WBUF0_CBUS_WBUF0_MASK          (0X7FF << 0)

#define WBUF1_CBUS_WBUF3_MASK          (0X7FF << 16)
#define WBUF1_CBUS_WBUF2_MASK          (0X7FF << 0)

#define WBUF2_CBUS_WBUF5_MASK          (0X7FF << 16)
#define WBUF2_CBUS_WBUF4_MASK          (0X7FF << 0)

#define WBUF3_CBUS_WBUF7_MASK          (0X7FF << 16)
#define WBUF3_CBUS_WBUF6_MASK          (0X7FF << 0)

#define WBUF4_CBUS_WBUF9_MASK          (0X7FF << 16)
#define WBUF4_CBUS_WBUF8_MASK          (0X7FF << 0)

#define WBUF5_CBUS_WBUF11_MASK          (0X7FF << 16)
#define WBUF5_CBUS_WBUF10_MASK          (0X7FF << 0)

#define WBUF6_CBUS_WBUF13_MASK          (0X7FF << 16)
#define WBUF6_CBUS_WBUF12_MASK          (0X7FF << 0)

#define WBUF7_CBUS_WBUF15_MASK          (0X7FF << 16)
#define WBUF7_CBUS_WBUF14_MASK          (0X7FF << 0)

#define WBUF8_CBUS_WBUF17_MASK          (0X7FF << 16)
#define WBUF8_CBUS_WBUF16_MASK          (0X7FF << 0)

#define WBUF9_CBUS_WBUF19_MASK          (0X7FF << 16)
#define WBUF9_CBUS_WBUF18_MASK          (0X7FF << 0)

#define WBUF10_CBUS_WBUF21_MASK          (0X7FF << 16)
#define WBUF10_CBUS_WBUF20_MASK          (0X7FF << 0)

#define WBUF11_CBUS_WBUF23_MASK          (0X7FF << 16)
#define WBUF11_CBUS_WBUF22_MASK          (0X7FF << 0)

#define RBUF_CBUS_RBUF_MASK          (0X7FF << 0)

#define LINK_00_TX_ARB_BYPASS_MASK          (0X01 << 31)
#define LINK_00_LINK_TXDECISION_MASK          (0X7F << 24)
#define LINK_00_NRETRY_MASK          (0X3F << 18)
#define LINK_00_TRESP_HOLD_MASK          (0X0F << 14)
#define LINK_00_TX_TWAIT_MASK          (0X0F << 10)
#define LINK_00_TREQ_HOLD_MASK          (0X0F << 6)
#define LINK_00_TX_NUM_MASK          (0X1F << 1)
#define LINK_00_TX_TRIG_MASK          (0X01 << 0)

#define LINK_01_LINKTX_BITIME_MASK          (0X7F << 21)
#define LINK_01_CBUS_ACK_0_MIN_MASK          (0X7F << 14)
#define LINK_01_CBUS_ACK_0_MAX_MASK          (0X7F << 7)
#define LINK_01_CBUS_ACK_FALL_MAX_MASK          (0X7F << 0)

#define LINK_02_LINKRX_EN_MASK          (0X01 << 31)
#define LINK_02_ADP_BITIME_EN_MASK          (0X01 << 30)
#define LINK_02_ADP_BITIME_RST_MASK          (0X01 << 29)
#define LINK_02_ADP_CONFIG1_MASK          (0X01 << 28)
#define LINK_02_ADP_CONFIG2_MASK          (0X01 << 27)
#define LINK_02_ADP_CONFIG3_MASK          (0X01 << 26)
#define LINK_02_ADP_CONFIG4_MASK          (0X01 << 25)
#define LINK_02_ADP_CONFIG5_MASK          (0X01 << 24)
#define LINK_02_ADP_CONFIG6_MASK          (0X01 << 23)
#define LINK_02_LINK_ACK_MANU_EN_MASK          (0X01 << 22)
#define LINK_02_LINKRX_DIS_TO_ARB23_MASK          (0X01 << 21)
#define LINK_02_TREQ_CONT_MASK          (0X7F << 14)
#define LINK_02_LINK_HALFTRAN_MAX_MASK          (0X7F << 7)
#define LINK_02_LINK_HALFTRAN_MIN_MASK          (0X7F << 0)

#define LINK_03_LINKRX_BITIME_MAX_MASK          (0X7F << 21)
#define LINK_03_LINKRX_BITIME_MIN_MASK          (0X7F << 14)
#define LINK_03_LINK_SYNCDUTY_MAX_MASK          (0X7F << 7)
#define LINK_03_LINK_SYNCDUTY_MIN_MASK          (0X7F << 0)

#define LINK_04_RBUF_LVL_THR_MASK          (0X1F << 27)
#define LINK_04_IGNORE_PAR_MASK          (0X01 << 26)
#define LINK_04_LINK_ACK_WIDTH_MASK          (0X7F << 14)
#define LINK_04_LINK_RXDECISION_MASK          (0X7F << 7)
#define LINK_04_LINKRX_BT_TIMEOUT_MASK          (0X7F << 0)

#define LINK_05_DISCOVERY_EN_MASK          (0X01 << 31)
#define LINK_05_WAKEUP_EN_MASK          (0X01 << 30)
#define LINK_05_WAKEUP_DONE_MASK          (0X01 << 29)
#define LINK_05_DISCOVER_DET_MASK          (0X01 << 28)
#define LINK_05_WAKEUP_OUT_LOW_EN_MASK          (0X01 << 27)
#define LINK_05_WAKEUP_RETRY_MASK          (0X01 << 26)
#define LINK_05_WAKEUP_SPEED_MASK          (0X01 << 24)
#define LINK_05_WAKE_TO_DISC_MASK          (0XFF << 16)
#define LINK_05_WAKE_PUL_WID2_MASK          (0XFF << 8)
#define LINK_05_WAKE_PUL_WID1_MASK          (0XFF << 0)

#define LINK_06_DISCOVER_CNT_MASK          (0X1F << 24)
#define LINK_06_DISC_PUL_WID_MASK          (0XFF << 16)
#define LINK_06_DISC_WID_CNT_TIMEOUT_MASK          (0XFF << 8)
#define LINK_06_VBUS_CBUS_STABLE_MASK          (0XFF << 0)

#define LINK_07_VBUS_DEGLITCH_MASK          (0X01 << 31)
#define LINK_07_CBUS_DEGLITCH_MASK          (0X01 << 30)
#define LINK_07_CBUS_OE_MASK          (0X01 << 29)
#define LINK_07_CBUS_DO_MASK          (0X01 << 28)
#define LINK_07_CBUS_DOE_SW_MASK          (0X01 << 27)
#define LINK_07_TMDS_RX_SENSE_MASK          (0X01 << 26)
#define LINK_07_ZCBUS_DET_EN_SW_MASK          (0X01 << 25)
#define LINK_07_ZCBUS_DET_EN_HW_MASK          (0X01 << 24)
#define LINK_07_DETECT_AGAIN_MASK          (0X01 << 23)
#define LINK_07_CD_SENSE_DEGLITCH_MASK          (0X01 << 22)
#define LINK_07_ZCBUS_SRC_ON_10K_MASK          (0X07 << 19)
#define LINK_07_ZCBUS_SRC_ON_10K_SW_MASK          (0X01 << 18)
#define LINK_07_MHL_MODE_MASK          (0X01 << 17)
#define LINK_07_MHL_MODE_SW_MASK          (0X01 << 16)
#define LINK_07_VBUS_OE_MASK          (0X01 << 15)
#define LINK_07_VBUS_DO_MASK          (0X01 << 14)
#define LINK_07_VBUS_DOE_SW_MASK          (0X01 << 13)
#define LINK_07_ZCBUS_SRC_ON_5K_MASK          (0X01 << 12)
#define LINK_07_ZCBUS_SRC_ON_5K_SW_MASK          (0X01 << 11)
#define LINK_07_ZCBUS_SRC_ON_100_MASK          (0X01 << 10)
#define LINK_07_ZCBUS_SRC_ON_100_SW_MASK          (0X01 << 9)
#define LINK_07_CBUS_SENSE_SMT_SW_MASK          (0X01 << 7)
#define LINK_07_CBUS_SENSE_IE_SW_MASK          (0X01 << 6)
#define LINK_07_CBUS_SENSE_SMT_MASK          (0X01 << 5)
#define LINK_07_CBUS_SENSE_IE_MASK          (0X01 << 4)
#define LINK_07_BYPASS_DET_MASK          (0X01 << 2)
#define LINK_07_CBUS_PULL_DOWN_SW_MASK          (0X01 << 1)
#define LINK_07_CBUS_PULL_DOWN_MASK          (0X01 << 0)

#define LINK_08_INT_MASK_MASK          (0XFFFF << 16)
#define LINK_08_VBUS_POS_INT_CLR_MASK          (0X01 << 15)
#define LINK_08_LINKRX_TIMEOUT_INT_CLR_MASK          (0X01 << 14)
#define LINK_08_WBUF_TRIG_INT_CLR_MASK          (0X01 << 13)
#define LINK_08_RBUF_TRIG_INT_CLR_MASK          (0X01 << 12)
#define LINK_08_ZCBUS_DET_DONE_INT_CLR_MASK          (0X01 << 11)
#define LINK_08_CBUS_NEG_INT_CLR_MASK          (0X01 << 10)
#define LINK_08_CBUS_POS_INT_CLR_MASK          (0X01 << 9)
#define LINK_08_MONITOR_CMP_INT_CLR_MASK          (0X01 << 8)
#define LINK_08_TMDS_RX_SENSE_DISCONN_INT_CLR_MASK          (0X01 << 7)
#define LINK_08_ZCBUS_DET_1K_INT_CLR_MASK          (0X01 << 6)
#define LINK_08_TX_ARB_FAIL_INT_CLR_MASK          (0X01 << 5)
#define LINK_08_TX_OK_INT_CLR_MASK          (0X01 << 4)
#define LINK_08_TX_RETRY_TO_INT_CLR_MASK          (0X01 << 3)
#define LINK_08_RBUF_LVL_THR_INT_CLR_MASK          (0X01 << 2)
#define LINK_08_DISC_DET_FAIL_INT_CLR_MASK          (0X01 << 1)
#define LINK_08_DISC_DET_INT_CLR_MASK          (0X01 << 0)

#define LINK_09_SW_RESET_MISC_MASK          (0X01 << 31)
#define LINK_09_SW_RESET_WAKE_MASK          (0X01 << 30)
#define LINK_09_SW_RESET_RX_MASK          (0X01 << 29)
#define LINK_09_SW_RESET_TX_MASK          (0X01 << 28)
#define LINK_09_CD_SENSE_DET_TIME_MASK          (0XFF << 20)
#define LINK_09_CBUS_OE_FAST_MASK          (0X01 << 17)
#define LINK_09_DUPLEX_MASK          (0X01 << 16)
#define LINK_09_CBUS_DRV_H_PRD_MASK          (0X1F << 8)
#define LINK_09_LINKTX_BITIME_MID_MASK          (0X7F << 0)

#define LINK_0A_INT_STA_MASK_MASK          (0XFFFF << 16)
#define LINK_0A_MON_CMP_SEL_MASK          (0X07 << 8)
#define LINK_0A_MON_CMP_DATA_MASK          (0XFF << 0)

#define LINK_0B_TREQ_OPP_MASK          (0X7F << 16)
#define LINK_0B_LINKRX_ACK1_SYNC_MASK          (0X7F << 8)
#define LINK_0B_LINKRX_DIS_TO_MASK          (0X01 << 7)
#define LINK_0B_RX_ARB_BYPASS_MASK          (0X01 << 6)
#define LINK_0B_FORCE_NAK_EN_MASK          (0X01 << 5)
#define LINK_0B_FORCE_NAK_CNT_MASK          (0X1F << 0)

#define LINK_0C_RX_TWAIT_MASK          (0X0F << 28)
#define LINK_0C_LINK_ACK_WIDTH_UPPER_MASK          (0X7F << 21)
#define LINK_0C_LINKRX_BITIME_MASK          (0X7F << 14)
#define LINK_0C_ADP_BITIME_MIN_MASK          (0X7F << 7)
#define LINK_0C_ADP_BITIME_MAX_MASK          (0X7F << 0)

#define LINK_0D_REG_CBUS_LINK_0D_MASK          (0XFFFFFFFF << 0)

#define LINK_STA_00_LINK_BITIME_MASK          (0X7F << 25)
#define LINK_STA_00_RBUF_LVL_LAT_MASK          (0X1F << 20)
#define LINK_STA_00_CD_SENSE_DGT_MASK          (0X01 << 19)
#define LINK_STA_00_CBUS_DET_1K_MASK          (0X01 << 18)
#define LINK_STA_00_CBUS_DET_10_MASK          (0X01 << 17)
#define LINK_STA_00_CBUS_DET_100K_MASK          (0X01 << 16)
#define LINK_STA_00_VBUS_POS_INT_MASK          (0X01 << 15)
#define LINK_STA_00_LINKRX_TIMEOUT_INT_MASK          (0X01 << 14)
#define LINK_STA_00_WBUF_TRIG_INT_MASK          (0X01 << 13)
#define LINK_STA_00_RBUF_TRIG_INT_MASK          (0X01 << 12)
#define LINK_STA_00_ZCBUS_DET_DONE_INT_MASK          (0X01 << 11)
#define LINK_STA_00_CBUS_NEG_INT_MASK          (0X01 << 10)
#define LINK_STA_00_CBUS_POS_INT_MASK          (0X01 << 9)
#define LINK_STA_00_MONITOR_CMP_INT_MASK          (0X01 << 8)
#define LINK_STA_00_TMDS_RX_SENSE_DISCONN_INT_MASK          (0X01 << 7)
#define LINK_STA_00_ZCBUS_DET_1K_INT_MASK          (0X01 << 6)
#define LINK_STA_00_TX_ARB_FAIL_INT_MASK          (0X01 << 5)
#define LINK_STA_00_TX_OK_INT_MASK          (0X01 << 4)
#define LINK_STA_00_TX_RETRY_TO_INT_MASK          (0X01 << 3)
#define LINK_STA_00_RBUF_LVL_THR_INT_MASK          (0X01 << 2)
#define LINK_STA_00_DISC_DET_FAIL_INT_MASK          (0X01 << 1)
#define LINK_STA_00_DISC_DET_INT_MASK          (0X01 << 0)

#define LINK_STA_01_CBUS_IN_DGT_MASK          (0X01 << 31)
#define LINK_STA_01_VBUS_IN_DGT_MASK          (0X01 << 30)
#define LINK_STA_01_LINKRX_FSM_MASK          (0X1F << 12)
#define LINK_STA_01_LINKTX_FSM_MASK          (0X1F << 7)
#define LINK_STA_01_DISC_FSM_MASK          (0X07 << 4)
#define LINK_STA_01_WAKEUP_FSM_MASK          (0X0F << 0)

#define LINK_BAK_REG_CBUS_LINK_BAK_MASK          (0XFFFFFFFF << 0)

#define LINK_BAK1_REG_CBUS_LINK_BAK1_MASK          (0XFFFFFFFF << 0)

#define CBUS_LINK_BAK_INT_SEL_ECO_MASK			(0x01 << 0)
/* /////////////////////////////////////////// */
#define CBUS_LINK_00_SETTING	(0x17813200)
#define CBUS_LINK_00_SETTING_L	(0x3200)

#define CBUS_MT6397_VER_ECO1	0x197
#define CBUS_MT6397_VER_ECO2	0x297
/* /////////////////////////////////////////// */

extern void vWriteCbus(unsigned short dAddr, unsigned int dVal);
extern unsigned int u4ReadCbus(unsigned int dAddr);
extern unsigned int u4ReadCbusFld(unsigned int dAddr, unsigned char u1Shift, unsigned int u1Mask);
extern void vWriteCbusMsk(unsigned int dAddr, unsigned int dVal, unsigned int dMsk);
extern void vSetCbusBit(unsigned int dAddr, unsigned int dMsk);
extern void vClrCbusBit(unsigned int dAddr, unsigned int dMsk);

#endif
#endif
