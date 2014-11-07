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


#ifndef XD_TOP_BASE_ADDR
#define XD_TOP_BASE_ADDR 0


#define ADDR_XD_TOP_CTRL_00            ( XD_TOP_BASE_ADDR + 0x000 )
#define ADDR_XD_TOP_CTRL_01            ( XD_TOP_BASE_ADDR + 0x008 )
#define ADDR_XD_TOP_CTRL_02            ( XD_TOP_BASE_ADDR + 0x00C )
#define ADDR_XD_IMAGE_SIZE0            ( XD_TOP_BASE_ADDR + 0x020 )
#define ADDR_XD_IMAGE_SIZE1            ( XD_TOP_BASE_ADDR + 0x024 )
#define ADDR_XD_FIFO_STATUS_00         ( XD_TOP_BASE_ADDR + 0x030 )
#define ADDR_XD_FIFO_STATUS_01         ( XD_TOP_BASE_ADDR + 0x034 )
#define ADDR_XD_FIFO_STATUS_02         ( XD_TOP_BASE_ADDR + 0x038 )
#define ADDR_XD_FIFO_STATUS_03         ( XD_TOP_BASE_ADDR + 0x03C )
#define ADDR_XD_VERSION                 ( XD_TOP_BASE_ADDR + 0x040 )
#define ADDR_XD_DMA_REG_CTRL           ( XD_TOP_BASE_ADDR + 0x044 )
#define ADDR_XD_HIST_LUT_BADDR         ( XD_TOP_BASE_ADDR + 0x048 )
#define ADDR_XD_GAIN_LUT_BADDR         ( XD_TOP_BASE_ADDR + 0x04C )
#define ADDR_XD_TP_CTRL_00             ( XD_TOP_BASE_ADDR + 0x050 )
#define ADDR_XD_TP_CTRL_01             ( XD_TOP_BASE_ADDR + 0x054 )
#define ADDR_XD_TP_CTRL_02             ( XD_TOP_BASE_ADDR + 0x058 )
#define ADDR_FIFO_FULL_ADRS0           ( XD_TOP_BASE_ADDR + 0x05C )
#define ADDR_FIFO_FULL_ADRS1           ( XD_TOP_BASE_ADDR + 0x060 )
#define ADDR_XD_FIFO_STATUS_04         ( XD_TOP_BASE_ADDR + 0x064 )
#define ADDR_XD_FIFO_STATUS_05         ( XD_TOP_BASE_ADDR + 0x068 )
#define ADDR_XD_FIFO_STATUS_06         ( XD_TOP_BASE_ADDR + 0x06C )
#define ADDR_XD_FIFO_STATUS_07         ( XD_TOP_BASE_ADDR + 0x070 )
#define ADDR_XD_FIFO_STATUS_08         ( XD_TOP_BASE_ADDR + 0x074 )
#define ADDR_XD_FIFO_STATUS_09         ( XD_TOP_BASE_ADDR + 0x078 )
#define ADDR_XD_FIFO_STATUS_10         ( XD_TOP_BASE_ADDR + 0x07C )
#define ADDR_XD_FIFO_STATUS_11         ( XD_TOP_BASE_ADDR + 0x080 )

/* xd_top_ctrl_00 */
#define XD_NR_INPUT_SEL_START                          (0)
#define XD_NR_INPUT_SEL_END                            (0)
#define XD_RE_INPUT_SEL_START                          (1)
#define XD_RE_INPUT_SEL_END                            (1)
#define XD_SCL_INPUT_SEL_START                         (2)
#define XD_SCL_INPUT_SEL_END                           (2)
#define XD_NR_INT_MASK_START                           (8)
#define XD_NR_INT_MASK_END                             (8)
#define XD_RE_INT_MASK_START                           (9)
#define XD_RE_INT_MASK_END                             (9)
#define XD_CE_INT_MASK_START                           (10)
#define XD_CE_INT_MASK_END                             (10)
#define XD_REG_UPDATE_MODE_START                       (16)
#define XD_REG_UPDATE_MODE_END                         (16)

/* xd_top_ctrl_01 */
#define XD_MEMORY_ALWAYS_ON_START                      (0)
#define XD_MEMORY_ALWAYS_ON_END                        (0)

/* xd_top_ctrl_02 */
#define XD_COLOR_FORMAT_START                          (1)
#define XD_COLOR_FORMAT_END                            (0)
#define XD_CBCR_ORDER_START                            (2)
#define XD_CBCR_ORDER_END                              (2)

/* xd_image_size0 */
#define XD_NR_PIC_WIDTH_START                          (10)
#define XD_NR_PIC_WIDTH_END                            (0)
#define XD_NR_PIC_HEIGHT_START                         (26)
#define XD_NR_PIC_HEIGHT_END                           (16)

/* xd_image_size1 */
#define XD_RE_PIC_WIDTH_START                          (10)
#define XD_RE_PIC_WIDTH_END                            (0)
#define XD_RE_PIC_HEIGHT_START                         (26)
#define XD_RE_PIC_HEIGHT_END                           (16)

/* xd_fifo_status_00 */
#define XD_PRE_FIFO1_PRE_NR_INPUT_PIXEL_TOTAL_COUNT_START	(20)
#define XD_PRE_FIFO1_PRE_NR_INPUT_PIXEL_TOTAL_COUNT_END	(0)

/* xd_fifo_status_01 */
#define XD_PRE_FIFO1_PRE_NR_OUTPUT_PIXEL_TOTAL_COUNT_START	(20)
#define XD_PRE_FIFO1_PRE_NR_OUTPUT_PIXEL_TOTAL_COUNT_END	(0)

/* xd_fifo_status_02 */
#define XD_POST_FIFO1_POST_NR_INPUT_PIXEL_TOTAL_COUNT_START	(20)
#define XD_POST_FIFO1_POST_NR_INPUT_PIXEL_TOTAL_COUNT_END	(0)

/* xd_fifo_status_03 */
#define XD_POST_FIFO1_POST_NR_OUTPUT_PIXEL_TOTAL_COUNT_START	(20)
#define XD_POST_FIFO1_POST_NR_OUTPUT_PIXEL_TOTAL_COUNT_END	(0)

/* xd_version */
#define XD_XD_VERSION_START                            (31)
#define XD_XD_VERSION_END                              (0)

/* xd_dma_reg_ctrl */
#define XD_HIST_LUT_DMA_START_START                    (0)
#define XD_HIST_LUT_DMA_START_END                      (0)
#define XD_HIST_LUT_OPERATION_MODE_START               (2)
#define XD_HIST_LUT_OPERATION_MODE_END                 (1)
#define XD_DCE_DSE_GAIN_LUT_DMA_START_START            (4)
#define XD_DCE_DSE_GAIN_LUT_DMA_START_END              (4)
#define XD_DCE_DSE_GAIN_LUT_OPERATION_MODE_START       (5)
#define XD_DCE_DSE_GAIN_LUT_OPERATION_MODE_END         (5)

/* xd_hist_lut_baddr */
#define XD_HIST_LUT_DRAM_BASE_ADDRESS_START            (31)
#define XD_HIST_LUT_DRAM_BASE_ADDRESS_END              (0)

/* xd_gain_lut_baddr */
#define XD_DCE_DSE_GAIN_LUT_DRAM_BASE_ADDRESS_START    (31)
#define XD_DCE_DSE_GAIN_LUT_DRAM_BASE_ADDRESS_END      (0)

/* xd_tp_ctrl_00 */
#define XD_TEST_PATTERN_ENABLE_START                   (0)
#define XD_TEST_PATTERN_ENABLE_END                     (0)
#define XD_TP_SYNC_GENERATION_ENABLE_START             (1)
#define XD_TP_SYNC_GENERATION_ENABLE_END               (1)
#define XD_TEST_PATTERN_SELECT_START                   (4)
#define XD_TEST_PATTERN_SELECT_END                     (2)
#define XD_TEST_PATTERN_SOLID_Y_DATA_START             (15)
#define XD_TEST_PATTERN_SOLID_Y_DATA_END               (8)
#define XD_TEST_PATTERN_SOLID_CB_DATA_START            (23)
#define XD_TEST_PATTERN_SOLID_CB_DATA_END              (16)
#define XD_TEST_PATTERN_SOLID_CR_DATA_START            (31)
#define XD_TEST_PATTERN_SOLID_CR_DATA_END              (24)

/* xd_tp_ctrl_01 */
#define XD_TEST_PATTERN_IMAGE_WIDTH_SIZE_START         (10)
#define XD_TEST_PATTERN_IMAGE_WIDTH_SIZE_END           (0)
#define XD_TEST_PATTERN_IMAGE_HEIGHT_SIZE_START        (26)
#define XD_TEST_PATTERN_IMAGE_HEIGHT_SIZE_END          (16)

/* xd_tp_ctrl_02 */
#define XD_TEST_PATTERN_Y_DATA_MASK_START              (7)
#define XD_TEST_PATTERN_Y_DATA_MASK_END                (0)
#define XD_TEST_PATTERN_CB_DATA_MASK_START             (15)
#define XD_TEST_PATTERN_CB_DATA_MASK_END               (8)
#define XD_TEST_PATTERN_CR_DATA_MASK_START             (23)
#define XD_TEST_PATTERN_CR_DATA_MASK_END               (16)

/* fifo_full_adrs0 */
#define XD_PRE_FIFO1_FULL_THRESHOLD_ADRS_START         (9)
#define XD_PRE_FIFO1_FULL_THRESHOLD_ADRS_END           (0)
#define XD_POST_FIFO1_FULL_THRESHOLD_ADRS_START        (25)
#define XD_POST_FIFO1_FULL_THRESHOLD_ADRS_END          (16)

/* fifo_full_adrs1 */
#define XD_PRE_FIFO2_FULL_THRESHOLD_ADRS_START         (9)
#define XD_PRE_FIFO2_FULL_THRESHOLD_ADRS_END           (0)
#define XD_POST_FIFO2_FULL_THRESHOLD_ADRS_START        (25)
#define XD_POST_FIFO2_FULL_THRESHOLD_ADRS_END          (16)

/* xd_fifo_status_04 */
#define XD_PRE_FIFO2_PRE_RE_INPUT_PIXEL_TOTAL_COUNT_START	(20)
#define XD_PRE_FIFO2_PRE_RE_INPUT_PIXEL_TOTAL_COUNT_END	(0)

/* xd_fifo_status_05 */
#define XD_PRE_FIFO2_PRE_RE_OUTPUT_PIXEL_TOTAL_COUNT_START	(20)
#define XD_PRE_FIFO2_PRE_RE_OUTPUT_PIXEL_TOTAL_COUNT_END	(0)

/* xd_fifo_status_06 */
#define XD_POST_FIFO2_POST_RE_INPUT_PIXEL_TOTAL_COUNT_START	(20)
#define XD_POST_FIFO2_POST_RE_INPUT_PIXEL_TOTAL_COUNT_END	(0)

/* xd_fifo_status_07 */
#define XD_POST_FIFO2_POST_RE_OUTPUT_PIXEL_TOTAL_COUNT_START	(20)
#define XD_POST_FIFO2_POST_RE_OUTPUT_PIXEL_TOTAL_COUNT_END	(0)

/* xd_fifo_status_08 */
#define XD_PRE_FIFO1_PRE_NR_WRITE_TOTAL_COUNT_START    (20)
#define XD_PRE_FIFO1_PRE_NR_WRITE_TOTAL_COUNT_END      (0)

/* xd_fifo_status_09 */
#define XD_POST_FIFO1_POST_NR_WRITE_TOTAL_COUNT_START  (20)
#define XD_POST_FIFO1_POST_NR_WRITE_TOTAL_COUNT_END    (0)

/* xd_fifo_status_10 */
#define XD_PRE_FIFO2_PRE_RE_WRITE_TOTAL_COUNT_START    (20)
#define XD_PRE_FIFO2_PRE_RE_WRITE_TOTAL_COUNT_END      (0)

/* xd_fifo_status_11 */
#define XD_POST_FIFO2_POST_RE_WRITE_TOTAL_COUNT_START  (20)
#define XD_POST_FIFO2_POST_RE_WRITE_TOTAL_COUNT_END    (0)


#endif

