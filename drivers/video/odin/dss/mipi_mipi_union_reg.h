#ifndef _reg_mipi_mipi_h
#define _reg_mipi_mipi_h


/*-----------------------------------------------------------------
	0x312b0000 interrupt_source0
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 hs_cfg_complete                       : 1; /* 0 */
		u32 esc_complete                          : 1; /* 1 */
		u32 hs_data_complete                      : 1; /* 2 */
		u32 frame_complete                        : 1; /* 3 */
		u32 rx_complete                           : 1; /* 4 */
		u32 rxdata_complete                       : 1; /* 5 */
		u32 rxtrigger0                            : 1; /* 6 */
		u32 rxtrigger1                            : 1; /* 7 */
		u32 rxtrigger2                            : 1; /* 8 */
		u32 rxtrigger3                            : 1; /* 9 */
		u32 txcfg_fifo_ur_err                     : 1; /* 10 */
		u32 txdata_fifo_ur_err                    : 1; /* 11 */
		u32 rxdata_fifo_ur_err                    : 1; /* 12 */
		u32 rx_ecc_err1_l0                        : 1; /* 13 */
		u32 rx_ecc_err2_l0                        : 1; /* 14 */
		u32 rx_chksum_err_l0                      : 1; /* 15 */
		u32 phy_err_esc_l0                        : 1; /* 16 */
		u32 phy_syncerr_esc_l0                    : 1; /* 17 */
		u32 phy_err_control_l0                    : 1; /* 18 */
		u32 phy_err_cont_lp0_l0                   : 1; /* 19 */
		u32 phy_err_cont_lp1_l0                   : 1; /* 20 */
		u32 clk_lane_enter_hs                     : 1; /* 21 */
		u32 clk_lane_exit_hs                      : 1; /* 22 */
		u32 rx_ecc_err1_l1                        : 1; /* 23 */
		u32 rx_ecc_err2_l1                        : 1; /* 24 */
		u32 rx_chksum_err_l1                      : 1; /* 25 */
		u32 phy_err_esc_l1                        : 1; /* 26 */
		u32 phy_syncerr_esc_l1                    : 1; /* 27 */
		u32 phy_err_control_l1                    : 1; /* 28 */
		u32 phy_err_cont_lp0_l1                   : 1; /* 29 */
		u32 phy_err_cont_lp1_l1                   : 1; /* 30 */
		u32 reserved31                            : 1; /* 31 */
	} af;
	u32 a32;
} interrupt_source0;

/*-----------------------------------------------------------------
	0x312b0004 interrupt_mask0
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 mask0                                 :32; /* 31:0 */
	} af;
	u32 a32;
} interrupt_mask0;

/*-----------------------------------------------------------------
	0x312b0008 interrupt_clear0
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 clr0                                  :32; /* 31:0 */
	} af;
	u32 a32;
} interrupt_clear0;

/*-----------------------------------------------------------------
	0x312b000c configuration0
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 input_format                          : 2; /* 1:0 */
		u32 output_format                         : 3; /* 4:2 */
		u32 swap_input                            : 1; /* 5 */
		u32 long_packet_escape                    : 1; /* 6 */
		u32 datapath_mode                         : 1; /* 7 */
		u32 reserved5                             : 1; /* 8 */
		u32 phy_resetn                            : 1; /* 9 */
		u32 phy_clk_lane_ena                      : 1; /* 10 */
		u32 phy_clk_lane_swap                     : 1; /* 11 */
		u32 phy_data_lane_ena0                    : 1; /* 12 */
		u32 phy_data_lane_swap0                   : 1; /* 13 */
		u32 phy_data_lane_ena1                    : 1; /* 14 */
		u32 phy_data_lane_swap1                   : 1; /* 15 */
		u32 phy_data_lane_ena2                    : 1; /* 16 */
		u32 phy_data_lane_swap2                   : 1; /* 17 */
		u32 phy_data_lane_ena3                    : 1; /* 18 */
		u32 phy_data_lane_swap3                   : 1; /* 19 */
		u32 turndisable0                          : 1; /* 20 */
		u32 turndisable1                          : 1; /* 21 */
		u32 turndisable2                          : 1; /* 22 */
		u32 turndisable3                          : 1; /* 23 */
		u32 reserved21                            : 4; /* 27:24 */
		u32 rx_long_ena3                          : 1; /* 28 */
		u32 rx_long_ena2                          : 1; /* 29 */
		u32 rx_long_ena1                          : 1; /* 30 */
		u32 rx_long_ena0                          : 1; /* 31 */
	} af;
	u32 a32;
} configuration0;

/*-----------------------------------------------------------------
	0x312b0010 hs_packet
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 data_id                               : 8; /* 7:0 */
		u32 data                                  :16; /* 23:8 */
		u32 hs_enter_bta_skip                     : 1; /* 24 */
		u32 reserved3                             : 4; /* 28:25 */
		u32 enable_eotp                           : 1; /* 29 */
		u32 mode                                  : 1; /* 30 */
		u32 packet_type                           : 1; /* 31 */
	} af;
	u32 a32;
} hs_packet;

/*-----------------------------------------------------------------
	0x312b0014 escape_mode_packet
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 dataid                                : 8; /* 7:0 */
		u32 data0                                 : 8; /* 15:8 */
		u32 data1                                 : 8; /* 23:16 */
		u32 data                                  : 8; /* 31:24 */
	} af;
	u32 a32;
} escape_mode_packet;

/*-----------------------------------------------------------------
	0x312b0018 escape_mode_trigger
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 trigger                               : 4; /* 3:0 */
		u32 reserved1                             :28; /* 31:4 */
	} af;
	u32 a32;
} escape_mode_trigger;

/*-----------------------------------------------------------------
	0x312b001c action
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 clk_lane_enter_ulp                    : 1; /* 0 */
		u32 clk_lane_exit_ulp                     : 1; /* 1 */
		u32 clk_lane_enter_hs                     : 1; /* 2 */
		u32 clk_lane_exit_hs                      : 1; /* 3 */
		u32 data_lane_enter_ulp                   : 1; /* 4 */
		u32 data_lane_exit_ulp                    : 1; /* 5 */
		u32 force_data_lane_stop                  : 1; /* 6 */
		u32 turn_request                          : 1; /* 7 */
		u32 flush_rx_fifo0                        : 1; /* 8 */
		u32 flush_cfg_fifo                        : 1; /* 9 */
		u32 flush_data_fifo                       : 1; /* 10 */
		u32 stop_video_data                       : 1; /* 11 */
		u32 flush_rx_fifo1                        : 1; /* 12 */
		u32 flush_rx_fifo2                        : 1; /* 13 */
		u32 flush_rx_fifo3                        : 1; /* 14 */
		u32 reserved15                            :13; /* 27:15 */
		u32 packet_header_clr3                    : 1; /* 28 */
		u32 packet_header_clr2                    : 1; /* 29 */
		u32 packet_header_clr1                    : 1; /* 30 */
		u32 packet_header_clr0                    : 1; /* 31 */
	} af;
	u32 a32;
} action;

/*-----------------------------------------------------------------
	0x312b0020 command_config
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 num_bytes                             :16; /* 15:0 */
		u32 wait_fifo                             : 1; /* 16 */
		u32 reserved2                             : 3; /* 19:17 */
		u32 hs_cmd_force_data                     : 8; /* 27:20 */
		u32 hs_cmd_force_data_en                  : 1; /* 28 */
		u32 reserved5                             : 3; /* 31:29 */
	} af;
	u32 a32;
} command_config;

/*-----------------------------------------------------------------
	0x312b0024 video_timing1
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 hsa_count                             :16; /* 15:0 */
		u32 bllp_count                            :16; /* 31:16 */
	} af;
	u32 a32;
} video_timing1;

/*-----------------------------------------------------------------
	0x312b0028 video_timing2
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 hbp_count                             :16; /* 15:0 */
		u32 hfp_count                             :16; /* 31:16 */
	} af;
	u32 a32;
} video_timing2;

/*-----------------------------------------------------------------
	0x312b002c video_config1
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 num_vsa_lines                         : 8; /* 7:0 */
		u32 num_vbp_lines                         : 8; /* 15:8 */
		u32 num_vfp_lines                         : 8; /* 23:16 */
		u32 reserved3                             : 7; /* 30:24 */
		u32 full_sync                             : 1; /* 31 */
	} af;
	u32 a32;
} video_config1;

/*-----------------------------------------------------------------
	0x312b0030 video_config2
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 num_vact_lines                        :16; /* 15:0 */
		u32 num_bytes_line                        :16; /* 31:16 */
	} af;
	u32 a32;
} video_config2;

/*-----------------------------------------------------------------
	0x312b0034 rx_packet_header_lane0
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 dataid                                : 8; /* 7:0 */
		u32 data0                                 : 8; /* 15:8 */
		u32 data1                                 : 8; /* 23:16 */
		u32 reserved3                             : 8; /* 31:24 */
	} af;
	u32 a32;
} rx_packet_header_lane0;

/*-----------------------------------------------------------------
	0x312b0038 status
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 phy_direction0                        : 1; /* 0 */
		u32 rx_ulpesc0                            : 1; /* 1 */
		u32 escape_ready                          : 1; /* 2 */
		u32 hs_data_ready                         : 1; /* 3 */
		u32 hs_cfg_ready                          : 1; /* 4 */
		u32 phy_clk_status                        : 2; /* 6:5 */
		u32 phy_data_status                       : 2; /* 8:7 */
		u32 hs_data_sm                            : 5; /* 13:9 */
		u32 txstopstatedata0                      : 1; /* 14 */
		u32 local_rx_long_ena0                    : 1; /* 15 */
		u32 txstopstatedata1                      : 1; /* 16 */
		u32 phy_direction1                        : 1; /* 17 */
		u32 rx_ulpesc1                            : 1; /* 18 */
		u32 local_rx_long_ena1                    : 1; /* 19 */
		u32 reserved14                            : 2; /* 21:20 */
		u32 txstopstatedata2                      : 1; /* 22 */
		u32 phy_direction2                        : 1; /* 23 */
		u32 rx_ulpesc2                            : 1; /* 24 */
		u32 local_rx_long_ena2                    : 1; /* 25 */
		u32 phy_direction3                        : 1; /* 26 */
		u32 rx_ulpesc3                            : 1; /* 27 */
		u32 local_rx_long_ena3                    : 1; /* 28 */
		u32 reserved22                            : 1; /* 29 */
		u32 txstopstatedata3                      : 1; /* 30 */
		u32 mipi_pll_lock                         : 1; /* 31 */
	} af;
	u32 a32;
} status;

/*-----------------------------------------------------------------
	0x312b003c fifo_byte_count
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 rx_fifo                               : 6; /* 5:0 */
		u32 reserved1                             : 2; /* 7:6 */
		u32 tx_cfg_fifo                           : 9; /* 16:8 */
		u32 reserved3                             : 3; /* 19:17 */
		u32 tx_data_fifo                          :12; /* 31:20 */
	} af;
	u32 a32;
} fifo_byte_count;

/*-----------------------------------------------------------------
	0x312b0040 configuration1
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 lp_lane_sel                           : 4; /* 3:0 */
		u32 hs_lane_sel                           : 4; /* 7:4 */
		u32 txdataesc0_wdata                      : 8; /* 15:8 */
		u32 txdataesc0_wen                        : 1; /* 16 */
		u32 reserved4                             : 3; /* 19:17 */
		u32 txdataesc1_wdata                      : 8; /* 27:20 */
		u32 txdataesc1_wen                        : 1; /* 28 */
		u32 reserved7                             : 3; /* 31:29 */
	} af;
	u32 a32;
} configuration1;

/*-----------------------------------------------------------------
	0x312b0044 configuration2
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 pdn_tx_clk0                           : 1; /* 0 */
		u32 pdn_tx_data0                          : 1; /* 1 */
		u32 pdn_tx_data1                          : 1; /* 2 */
		u32 reserved3                             : 1; /* 3 */
		u32 pdn_tx_clk1                           : 1; /* 4 */
		u32 pdn_tx_data2                          : 1; /* 5 */
		u32 pdn_tx_data3                          : 1; /* 6 */
		u32 reserved7                             : 1; /* 7 */
		u32 txdataesc2_wdata                      : 8; /* 15:8 */
		u32 txdataesc2_wen                        : 1; /* 16 */
		u32 reserved10                            : 3; /* 19:17 */
		u32 txdataesc3_wdata                      : 8; /* 27:20 */
		u32 txdataesc3_wen                        : 1; /* 28 */
		u32 reserved13                            : 3; /* 31:29 */
	} af;
	u32 a32;
} configuration2;

/*-----------------------------------------------------------------
	0x312b0048 phy_pll_set_only_fpga
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 reserved0                             : 3; /* reserved */
	} af;
	u32 a32;
} phy_pll_set_only_fpga;

/*-----------------------------------------------------------------
	0x312b004c phy_register_set_only_fpga
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 reserved0                             : 3; /* reserved */
	} af;
	u32 a32;
} phy_register_set_only_fpga;

/*-----------------------------------------------------------------
	0x312b0050 interrupt_source1
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 rx_complete_l1                        : 1; /* 0 */
		u32 rxdata_complete_l1                    : 1; /* 1 */
		u32 rxtrigger0_l1                         : 1; /* 2 */
		u32 rxtrigger1_l1                         : 1; /* 3 */
		u32 rxtrigger2_l1                         : 1; /* 4 */
		u32 rxtrigger3_l1                         : 1; /* 5 */
		u32 rxdata_fifo_err_l1                    : 1; /* 6 */
		u32 rx_complete_l2                        : 1; /* 7 */
		u32 rxdata_complete_l2                    : 1; /* 8 */
		u32 rxtrigger0_l2                         : 1; /* 9 */
		u32 rxtrigger1_l2                         : 1; /* 10 */
		u32 rxtrigger2_l2                         : 1; /* 11 */
		u32 rxtrigger3_l2                         : 1; /* 12 */
		u32 rxdata_fifo_err_l2                    : 1; /* 13 */
		u32 rx_complete_l3                        : 1; /* 14 */
		u32 rxdata_complete_l3                    : 1; /* 15 */
		u32 rxtrigger0_l3                         : 1; /* 16 */
		u32 rxtrigger1_l3                         : 1; /* 17 */
		u32 rxtrigger2_l3                         : 1; /* 18 */
		u32 rxtrigger3_l3                         : 1; /* 19 */
		u32 rxdata_fifo_err_l3                    : 1; /* 20 */
		u32 txcfg_fifo_or_err                     : 1; /* 21 */
		u32 txdata_fifo_or_err                    : 1; /* 22 */
		u32 reserved23                            : 9; /* 31:23 */
	} af;
	u32 a32;
} interrupt_source1;

/*-----------------------------------------------------------------
	0x312b0054 interrupt_mask1
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 mask1                                 :32; /* 31:0 */
	} af;
	u32 a32;
} interrupt_mask1;

/*-----------------------------------------------------------------
	0x312b0058 interrupt_clear1
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 clr1                                  :32; /* 31:0 */
	} af;
	u32 a32;
} interrupt_clear1;

/*-----------------------------------------------------------------
	0x312b0060 interrupt_source2
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 rx_ecc_err1_l2                        : 1; /* 0 */
		u32 rx_ecc_err2_l2                        : 1; /* 1 */
		u32 rx_chksum_err_l2                      : 1; /* 2 */
		u32 phy_err_esc_l2                        : 1; /* 3 */
		u32 phy_syncerr_esc_l2                    : 1; /* 4 */
		u32 phy_err_control_l2                    : 1; /* 5 */
		u32 phy_err_cont_lp0_l2                   : 1; /* 6 */
		u32 phy_err_cont_lp1_l2                   : 1; /* 7 */
		u32 rx_ecc_err1_l3                        : 1; /* 8 */
		u32 rx_ecc_err2_l3                        : 1; /* 9 */
		u32 rx_chksum_err_l3                      : 1; /* 10 */
		u32 phy_err_esc_l3                        : 1; /* 11 */
		u32 phy_syncerr_esc_l3                    : 1; /* 12 */
		u32 phy_err_control_l3                    : 1; /* 13 */
		u32 phy_err_cont_lp0_l3                   : 1; /* 14 */
		u32 phy_err_cont_lp1_l3                   : 1; /* 15 */
		u32 reserved16                            :16; /* 31:16 */
	} af;
	u32 a32;
} interrupt_source2;

/*-----------------------------------------------------------------
	0x312b0064 interrupt_mask2
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 mask2                                 :32; /* 31:0 */
	} af;
	u32 a32;
} interrupt_mask2;

/*-----------------------------------------------------------------
	0x312b0068 interrupt_clear2
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 clr2                                  :32; /* 31:0 */
	} af;
	u32 a32;
} interrupt_clear2;

/*-----------------------------------------------------------------
	0x312b0070 test_clear
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 testclr                               : 1; /* 0 */
		u32 reserved1                             :31; /* 31:1 */
	} af;
	u32 a32;
} test_clear;

/*-----------------------------------------------------------------
	0x312b0074 test_clock
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 testclk                               : 1; /* 0 */
		u32 reserved1                             :31; /* 31:1 */
	} af;
	u32 a32;
} test_clock;

/*-----------------------------------------------------------------
	0x312b0078 test_data_in
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 testdin                               : 8; /* 7:0 */
		u32 reserved1                             :24; /* 31:8 */
	} af;
	u32 a32;
} test_data_in;

/*-----------------------------------------------------------------
	0x312b007c test_data_enable
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 testen                                : 1; /* 0 */
		u32 reserved1                             :31; /* 31:1 */
	} af;
	u32 a32;
} test_data_enable;

/*-----------------------------------------------------------------
	0x312b0080 test_data_out
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 testdout                              : 8; /* 7:0 */
		u32 reserved1                             :24; /* 31:8 */
	} af;
	u32 a32;
} test_data_out;

/*-----------------------------------------------------------------
	0x312b0090 rx_packet_header_lane1
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 dataid                                : 8; /* 7:0 */
		u32 data0                                 : 8; /* 15:8 */
		u32 data1                                 : 8; /* 23:16 */
		u32 reserved3                             : 8; /* 31:24 */
	} af;
	u32 a32;
} rx_packet_header_lane1;

/*-----------------------------------------------------------------
	0x312b0094 rx_packet_header_lane2
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 dataid                                : 8; /* 7:0 */
		u32 data0                                 : 8; /* 15:8 */
		u32 data1                                 : 8; /* 23:16 */
		u32 reserved3                             : 8; /* 31:24 */
	} af;
	u32 a32;
} rx_packet_header_lane2;

/*-----------------------------------------------------------------
	0x312b0098 rx_packet_header_lane3
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 dataid                                : 8; /* 7:0 */
		u32 data0                                 : 8; /* 15:8 */
		u32 data1                                 : 8; /* 23:16 */
		u32 reserved3                             : 8; /* 31:24 */
	} af;
	u32 a32;
} rx_packet_header_lane3;

/*-----------------------------------------------------------------
	0x312b1000 tx_cfg_fifo
-----------------------------------------------------------------*/
typedef union {
	struct {
		u32 reserved0                             : 8; /* reserved */
	} af;
	u32 a32;
} tx_cfg_fifo;

/*-----------------------------------------------------------------
	0x312b2000 rx_packet_payload_fifo
-----------------------------------------------------------------*/
typedef union {
	struct {
	} af;
	u32 a32;
} rx_packet_payload_fifo;

typedef struct {
	interrupt_source0             	interrupt_source0;
	interrupt_mask0               	interrupt_mask0;
	interrupt_clear0              	interrupt_clear0;
	configuration0                	configuration0;
	hs_packet                     	hs_packet;
	escape_mode_packet            	escape_mode_packet;
	escape_mode_trigger           	escape_mode_trigger;
	action                        	action;
	command_config                	command_config;
	video_timing1                 	video_timing1;
	video_timing2                 	video_timing2;
	video_config1                 	video_config1;
	video_config2                 	video_config2;
	rx_packet_header_lane0        	rx_packet_header_lane0;
	status                        	status;
	fifo_byte_count               	fifo_byte_count;
	configuration1                	configuration1;
	configuration2                	configuration2;
	phy_pll_set_only_fpga        	phy_pll_set__only_fpga;
	phy_register_set_only_fpga   	phy_register_set__only_fpga;
	interrupt_source1             	interrupt_source1;
	interrupt_mask1               	interrupt_mask1;
	interrupt_clear1              	interrupt_clear1;
	interrupt_source2             	interrupt_source2;
	interrupt_mask2               	interrupt_mask2;
	interrupt_clear2              	interrupt_clear2;
	test_clear                    	test_clear;
	test_clock                    	test_clock;
	test_data_in                  	test_data_in;
	test_data_enable              	test_data_enable;
	test_data_out                 	test_data_out;
	rx_packet_header_lane1        	rx_packet_header_lane1;
	rx_packet_header_lane2        	rx_packet_header_lane2;
	rx_packet_header_lane3        	rx_packet_header_lane3;
	tx_cfg_fifo                   	tx_cfg_fifo;
	rx_packet_payload_fifo        	rx_packet_payload_fifo;
} DSS_DIP_MIPI_MIPI_REG_T;
#endif

