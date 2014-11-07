#ifndef CABC_BASE_ADDR
#define CABC_BASE_ADDR 0


#define ADDR_CABC1_SIZE_CTRL                 ( CABC_BASE_ADDR + 0x000 )
#define ADDR_CABC1_CSC_CTRL                  ( CABC_BASE_ADDR + 0x004 )
#define ADDR_CABC1_TOP_CTRL                  ( CABC_BASE_ADDR + 0x008 )
#define ADDR_CABC1_GAIN_LUT_CTRL             ( CABC_BASE_ADDR + 0x00C )
#define ADDR_CABC1_GAIN_LUT_DATA             ( CABC_BASE_ADDR + 0x010 )
#define ADDR_CABC1_HIST_LUT_ADRS             ( CABC_BASE_ADDR + 0x014 )
#define ADDR_CABC1_HIST_LUT_RDATA            ( CABC_BASE_ADDR + 0x018 )
#define ADDR_CABC1_APL                       ( CABC_BASE_ADDR + 0x01C )
#define ADDR_CABC1_MAX_MIN                   ( CABC_BASE_ADDR + 0x020 )
#define ADDR_CABC1_TOTAL_PIXEL_CNT           ( CABC_BASE_ADDR + 0x024 )
#define ADDR_CABC1_HIST_TOP5                 ( CABC_BASE_ADDR + 0x028 )
#define ADDR_CABC1_DMA_CTRL                  ( CABC_BASE_ADDR + 0x02C )
#define ADDR_CABC1_HIST_LUT_BADDR            ( CABC_BASE_ADDR + 0x030 )
#define ADDR_CABC1_GAIN_LUT_BADDR            ( CABC_BASE_ADDR + 0x034 )
#define ADDR_CABC2_SIZE_CTRL                 ( CABC_BASE_ADDR + 0x038 )
#define ADDR_CABC2_CSC_CTRL                  ( CABC_BASE_ADDR + 0x03C )
#define ADDR_CABC2_TOP_CTRL                  ( CABC_BASE_ADDR + 0x040 )
#define ADDR_CABC2_GAIN_LUT_CTRL             ( CABC_BASE_ADDR + 0x044 )
#define ADDR_CABC2_GAIN_LUT_RDATA            ( CABC_BASE_ADDR + 0x048 )
#define ADDR_CABC2_HIST_LUT_ADRS             ( CABC_BASE_ADDR + 0x04C )
#define ADDR_CABC2_HIST_LUT_DATA             ( CABC_BASE_ADDR + 0x050 )
#define ADDR_CABC2_APL                       ( CABC_BASE_ADDR + 0x054 )
#define ADDR_CABC2_MAX_MIN                   ( CABC_BASE_ADDR + 0x058 )
#define ADDR_CABC2_TOTAL_PIXEL_CNT           ( CABC_BASE_ADDR + 0x05C )
#define ADDR_CABC2_HIST_TOP5                 ( CABC_BASE_ADDR + 0x060 )
#define ADDR_CABC2_DMA_CTRL                  ( CABC_BASE_ADDR + 0x064 )
#define ADDR_CABC2_HIST_LUT_BADDR            ( CABC_BASE_ADDR + 0x068 )
#define ADDR_CABC2_GAIN_LUT_BADDR            ( CABC_BASE_ADDR + 0x06C )

/* cabc1_size_ctrl */
#define DU_IMAGE_WIDTH_SIZE_START            (11)
#define DU_IMAGE_WIDTH_SIZE_END              (0)
#define DU_IMAGE_HEIGHT_SIZE_START           (26)
#define DU_IMAGE_HEIGHT_SIZE_END             (16)
#define DU_REG_UPDATE_MODE_START             (31)
#define DU_REG_UPDATE_MODE_END               (31)

/* cabc1_csc_ctrl */
#define DU_R_COEF_START                      (9)
#define DU_R_COEF_END                        (0)
#define DU_G_COEF_START                      (19)
#define DU_G_COEF_END                        (10)
#define DU_B_COEF_START                      (29)
#define DU_B_COEF_END                        (20)

/* cabc1_top_ctrl */
#define DU_CABC_EN_START                     (0)
#define DU_CABC_EN_END                       (0)
#define DU_HIST_STEP_MODE_START              (1)
#define DU_HIST_STEP_MODE_END                (1)
#define DU_CABC_GAIN_RANGE_START             (3)
#define DU_CABC_GAIN_RANGE_END               (2)
#define DU_CABC_GLOBAL_GAIN_START            (27)
#define DU_CABC_GLOBAL_GAIN_END              (16)
#define DU_INTERRUPT_MASK_START              (31)
#define DU_INTERRUPT_MASK_END                (31)

/* cabc1_gain_lut_ctrl */
#define DU_CABC_LUT_WEN_START                (0)
#define DU_CABC_LUT_WEN_END                  (0)
#define DU_CABC_LUT_ADRS_START               (8)
#define DU_CABC_LUT_ADRS_END                 (1)
#define DU_CABC_LUT_WDATA_START              (27)
#define DU_CABC_LUT_WDATA_END                (16)

/* cabc1_gain_lut_rdata */
#define DU_CABC_LUT_RDATA_START              (11)
#define DU_CABC_LUT_RDATA_END                (0)

/* cabc1_hist_lut_adrs */
#define DU_HIST_LUT_ADRS_START               (4)
#define DU_HIST_LUT_ADRS_END                 (0)

/* cabc1_hist_lut_rdata */
#define DU_HIST_LUT_RDATA_START              (22)
#define DU_HIST_LUT_RDATA_END                (0)

/* cabc1_apl */
#define DU_APL_Y_START                       (7)
#define DU_APL_Y_END                         (0)
#define DU_APL_R_START                       (15)
#define DU_APL_R_END                         (8)
#define DU_APL_G_START                       (23)
#define DU_APL_G_END                         (16)
#define DU_APL_B_START                       (31)
#define DU_APL_B_END                         (24)

/* cabc1_max_min */
#define DU_MAX_Y_START                       (7)
#define DU_MAX_Y_END                         (0)
#define DU_MIN_Y_START                       (16)
#define DU_MIN_Y_END                         (8)

/* cabc1_total_pixel_cnt */
#define DU_TOTAL_PIXEL_CNT_START             (22)
#define DU_TOTAL_PIXEL_CNT_END               (0)

/* cabc1_hist_top5 */
#define DU_HIST_TOP1_START                   (5)
#define DU_HIST_TOP1_END                     (0)
#define DU_HIST_TOP2_START                   (11)
#define DU_HIST_TOP2_END                     (6)
#define DU_HIST_TOP3_START                   (17)
#define DU_HIST_TOP3_END                     (12)
#define DU_HIST_TOP4_START                   (23)
#define DU_HIST_TOP4_END                     (18)
#define DU_HIST_TOP5_START                   (29)
#define DU_HIST_TOP5_END                     (24)

/* cabc1_dma_ctrl */
#define DU_HIST_LUT_DMA_START_START          (0)
#define DU_HIST_LUT_DMA_START_END            (0)
#define DU_HIST_LUT_OPERATION_MODE_START     (2)
#define DU_HIST_LUT_OPERATION_MODE_END       (1)
#define DU_GAIN_LUT_DAM_START_START          (4)
#define DU_GAIN_LUT_DAM_START_END            (4)
#define DU_GAIN_LUT_OPERATION_MODE_START     (5)
#define DU_GAIN_LUT_OPERATION_MODE_END       (5)

/* cabc1_hist_lut_baddr */
#define DU_HIST_LUT_DRAM_BASE_ADDRESS_START  (31)
#define DU_HIST_LUT_DRAM_BASE_ADDRESS_END    (0)

/* cabc1_gain_lut_baddr */
#define DU_GAIN_LUT_DRAM_BASE_ADDRESS_START  (31)
#define DU_GAIN_LUT_DRAM_BASE_ADDRESS_END    (0)

/* cabc2_size_ctrl */
#define DU_IMAGE_WIDTH_SIZE_START            (11)
#define DU_IMAGE_WIDTH_SIZE_END              (0)
#define DU_IMAGE_HEIGHT_SIZE_START           (26)
#define DU_IMAGE_HEIGHT_SIZE_END             (16)

/* cabc2_csc_ctrl */
#define DU_R_COEF_START                      (9)
#define DU_R_COEF_END                        (0)
#define DU_G_COEF_START                      (19)
#define DU_G_COEF_END                        (10)
#define DU_B_COEF_START                      (29)
#define DU_B_COEF_END                        (20)

/* cabc2_top_ctrl */
#define DU_CABC_EN_START                     (0)
#define DU_CABC_EN_END                       (0)
#define DU_HIST_STEP_MODE_START              (1)
#define DU_HIST_STEP_MODE_END                (1)
#define DU_CABC_GAIN_RANGE_START             (3)
#define DU_CABC_GAIN_RANGE_END               (2)
#define DU_CABC_GLOBAL_GAIN_START            (27)
#define DU_CABC_GLOBAL_GAIN_END              (16)
#define DU_INTERRUPT_MASK_START              (31)
#define DU_INTERRUPT_MASK_END                (31)

/* cabc2_gain_lut_ctrl */
#define DU_CABC_LUT_WEN_START                (0)
#define DU_CABC_LUT_WEN_END                  (0)
#define DU_CABC_LUT_ADRS_START               (8)
#define DU_CABC_LUT_ADRS_END                 (1)
#define DU_CABC_LUT_WDATA_START              (27)
#define DU_CABC_LUT_WDATA_END                (16)

/* cabc2_gain_lut_rdata */
#define DU_CABC_LUT_RDATA_START              (11)
#define DU_CABC_LUT_RDATA_END                (0)

/* cabc2_hist_lut_adrs */
#define DU_HIST_LUT_ADRS_START               (4)
#define DU_HIST_LUT_ADRS_END                 (0)

/* cabc2_hist_lut_rdata */
#define DU_HIST_LUT_RDATA_START              (22)
#define DU_HIST_LUT_RDATA_END                (0)

/* cabc2_apl */
#define DU_APL_Y_START                       (7)
#define DU_APL_Y_END                         (0)
#define DU_APL_R_START                       (15)
#define DU_APL_R_END                         (8)
#define DU_APL_G_START                       (23)
#define DU_APL_G_END                         (16)
#define DU_APL_B_START                       (31)
#define DU_APL_B_END                         (24)

/* cabc2_max_min */
#define DU_MAX_Y_START                       (7)
#define DU_MAX_Y_END                         (0)
#define DU_MIN_Y_START                       (16)
#define DU_MIN_Y_END                         (8)

/* cabc2_total_pixel_cnt */
#define DU_TOTAL_PIXEL_CNT_START             (22)
#define DU_TOTAL_PIXEL_CNT_END               (0)

/* cabc2_hist_top5 */
#define DU_HIST_TOP1_START                   (5)
#define DU_HIST_TOP1_END                     (0)
#define DU_HIST_TOP2_START                   (11)
#define DU_HIST_TOP2_END                     (6)
#define DU_HIST_TOP3_START                   (17)
#define DU_HIST_TOP3_END                     (12)
#define DU_HIST_TOP4_START                   (23)
#define DU_HIST_TOP4_END                     (18)
#define DU_HIST_TOP5_START                   (29)
#define DU_HIST_TOP5_END                     (24)

/* cabc2_dma_ctrl */
#define DU_HIST_LUT_DMA_START_START          (0)
#define DU_HIST_LUT_DMA_START_END            (0)
#define DU_HIST_LUT_OPERATION_MODE_START     (2)
#define DU_HIST_LUT_OPERATION_MODE_END       (1)
#define DU_GAIN_LUT_DAM_START_START          (4)
#define DU_GAIN_LUT_DAM_START_END            (4)
#define DU_GAIN_LUT_OPERATION_MODE_START     (5)
#define DU_GAIN_LUT_OPERATION_MODE_END       (5)

/* cabc2_hist_lut_baddr */
#define DU_HIST_LUT_DRAM_BASE_ADDRESS_START  (31)
#define DU_HIST_LUT_DRAM_BASE_ADDRESS_END    (0)

/* cabc2_gain_lut_baddr */
#define DU_GAIN_LUT_DRAM_BASE_ADDRESS_START  (31)
#define DU_GAIN_LUT_DRAM_BASE_ADDRESS_END    (0)


#endif

