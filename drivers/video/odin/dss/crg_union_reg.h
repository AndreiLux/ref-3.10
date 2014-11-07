#ifndef __CRG_UNION_REG_H
#define __CRG_UNION_REG_H


/*------------------------------------------------------------------
	0x31400000 crg_core_clk_selection
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 core_clk_sel                           : 2; /* 1:0 */
		u32 gfx_core_clk_sel                       : 2; /* 3:2 */
		u32 reserved2                              :28; /* 31:4 */
	} af;
	u32 a32;
} crg_core_clk_selection;

/*------------------------------------------------------------------
	0x31400004 crg_core_clock_enable
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 vscl0_clk_enable                       : 1; /* 0 */
		u32 vscl1_clk_enable                       : 1; /* 1 */
		u32 vdma_clk_enable                        : 1; /* 2 */
		u32 mxd_clk_enable                         : 1; /* 3 */
		u32 cabc_clk_enable                        : 1; /* 4 */
		u32 formatter_clk_enable                   : 1; /* 5 */
		u32 gdma0_clk_enable                       : 1; /* 6 */
		u32 gdma1_clk_enable                       : 1; /* 7 */
		u32 gdma2_clk_enable                       : 1; /* 8 */
		u32 overlay_clk_enable                     : 1; /* 9 */
		u32 gscl_clk_enable                        : 1; /* 10 */
		u32 rotator_clk_enable                     : 1; /* 11 */
		u32 sync_gen_clk_enable                    : 1; /* 12 */
		u32 dip0_clk_enable                        : 1; /* 13 */
		u32 mipi0_clk_enable                       : 1; /* 14 */
		u32 dip1_clk_enable                        : 1; /* 15 */
		u32 mipi1_clk_enable                       : 1; /* 16 */
		u32 gfx0_clk_enable                        : 1; /* 17 */
		u32 gfx1_clk_enable                        : 1; /* 18 */
		u32 dip2_clk_enable                        : 1; /* 19 */
		u32 hdmi_clk_enable                        : 1; /* 20 */
		u32 reserved21                             :11; /* 31:21 */
	} af;
	u32 a32;
} crg_core_clock_enable;

/*------------------------------------------------------------------
	0x31400008 crg_other_clock_control
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 disp0_clk_selection                    : 2; /* 1:0 */
		u32 disp1_clk_selection                    : 2; /* 3:2 */
		u32 hdmi_clk_selection                     : 2; /* 5:4 */
		u32 osc_clk_selection                      : 2; /* 7:6 */
		u32 cec_clk_selection                      : 2; /* 9:8 */
		u32 tx_clk_esc_selection                   : 2; /* 11:10 */
		u32 reserved6                              : 4; /* 15:12 */
		u32 disp0_clk_enable                       : 1; /* 16 */
		u32 disp1_clk_enable                       : 1; /* 17 */
		u32 hdmi_disp_clk_enable                   : 1; /* 18 */
		u32 dphy0_osc_clk_enable                   : 1; /* 19 */
		u32 dphy1_osc_clk_enable                   : 1; /* 20 */
		u32 sfr_clk_enable                         : 1; /* 21 */
		u32 cec_clk_enable                         : 1; /* 22 */
		u32 tx_clk_esc0_enable                     : 1; /* 23 */
		u32 tx_clk_esc1_enable                     : 1; /* 24 */
		u32 reserved16                             : 6; /* 30:25 */
		u32 parallel_lcd_test_mode                 : 1; /* 31 */
	} af;
	u32 a32;
} crg_other_clock_control;

/*------------------------------------------------------------------
	0x3140000c crg_clock_divide_value_0
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 tx_clk_esc_divide                      : 8; /* 7:0 */
		u32 disp0_clk_divide                       : 8; /* 15:8 */
		u32 disp1_clk_divide                       : 8; /* 23:16 */
		u32 hdmi_clk_divide                        : 8; /* 31:24 */
	} af;
	u32 a32;
} crg_clock_divide_value_0;

/*------------------------------------------------------------------
	0x31400010 crg_clock_divide_value_1
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 gfx_clk_esc_divide                     : 8; /* 7:0 */
		u32 reserved1                              :24; /* 31:8 */
	} af;
	u32 a32;
} crg_clock_divide_value_1;

/*------------------------------------------------------------------
	0x31400014 crg_clock_divide_value_update
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 core_clk_div_update                    : 1; /* 0 */
		u32 gfx_clk_div_update                     : 1; /* 1 */
		u32 disp0_clk_div_update                   : 1; /* 2 */
		u32 disp1_clk_div_update                   : 1; /* 3 */
		u32 hdmi_clk_div_update                    : 1; /* 4 */
		u32 dphy_osc_clk_div_update                : 1; /* 5 */
		u32 tx_clk_esc_div_update                  : 1; /* 6 */
		u32 cec_clk_div_update                     : 1; /* 7 */
		u32 reserved8                              :24; /* 31:8 */
	} af;
	u32 a32;
} crg_clock_divide_value_update;

/*------------------------------------------------------------------
	0x31401000 crg_soft_reset
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 vscl0_reset                            : 1; /* 0 */
		u32 vscl1_reset                            : 1; /* 1 */
		u32 vdma_reset                             : 1; /* 2 */
		u32 mxd_reset                              : 1; /* 3 */
		u32 cabc_reset                             : 1; /* 4 */
		u32 formtter_reset                         : 1; /* 5 */
		u32 gdma0_reset                            : 1; /* 6 */
		u32 gdma1_reset                            : 1; /* 7 */
		u32 gdma2_reset                            : 1; /* 8 */
		u32 overlay_reset                          : 1; /* 9 */
		u32 gscl_reset                             : 1; /* 10 */
		u32 rotator_reset                          : 1; /* 11 */
		u32 sync_gen_reset                         : 1; /* 12 */
		u32 dip0_reset                             : 1; /* 13 */
		u32 mipi0_reset                            : 1; /* 14 */
		u32 dip1_reset                             : 1; /* 15 */
		u32 mipi1_reset                            : 1; /* 16 */
		u32 gfx0_reset                             : 1; /* 17 */
		u32 gfx1_reset                             : 1; /* 18 */
		u32 dip2_reset                             : 1; /* 19 */
		u32 hdmi_reset                             : 1; /* 20 */
		u32 reserved21                             :11; /* 31:21 */
	} af;
	u32 a32;
} crg_soft_reset;

/*------------------------------------------------------------------
	0x31402000 crg_master_port_arbitration_control
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 bmi0_arbitration                       : 1; /* 0 */
		u32 bmi1_arbitration                       : 1; /* 1 */
		u32 bmi2_arbitration                       : 1; /* 2 */
		u32 bmi3_arbitration                       : 1; /* 3 */
		u32 bmi4_arbitration                       : 1; /* 4 */
		u32 bmi5_arbitration                       : 1; /* 5 */
		u32 reserved6                              :27; /* 31:5 */
	} af;
	u32 a32;
} crg_master_port_arbitration_control;

/*------------------------------------------------------------------
	0x31403000 crg_memory_deep_sleep_mode_control
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 deep_sleep_for_power_domain_1          : 1; /* 0 */
		u32 deep_sleep_for_power_domain_2          : 1; /* 1 */
		u32 deep_sleep_for_power_domain_3          : 1; /* 2 */
		u32 deep_sleep_for_power_domain_4          : 1; /* 3 */
		u32 deep_sleep_for_power_domain_5          : 1; /* 4 */
		u32 deep_sleep_for_power_domain_6          : 1; /* 5 */
		u32 reserved6                              : 2; /* 7:6 */
		u32 hdmi_mem_rm_ctrl                       : 4; /* 11:8 */
		u32 hdmi_mem_ls_ctrl                       : 1; /* 12 */
		u32 reserved9                              : 1; /* 13 */
		u32 reserved10                             : 1; /* 14 */
		u32 reserved11                             : 1; /* 15 */
		u32 hdmi_shut_down_mode                    : 1; /* 16 */
		u32 hdmi_sleep_mode                        : 1; /* 17 */
		u32 dip0_sleep_mode                        : 1; /* 18 */
		u32 dip1_sleep_mode                        : 1; /* 19 */
		u32 reserved16                             :12; /* 31:20 */
	} af;
	u32 a32;
} crg_memory_deep_sleep_mode_control;

/*------------------------------------------------------------------
	0x31403004 crg_test_mode_and_other_control
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 niu_wrapper_0_test_mode                : 1; /* 0 */
		u32 niu_wrapper_1_test_mode                : 1; /* 1 */
		u32 niu_wrapper_2_test_mode                : 1; /* 2 */
		u32 niu_wrapper_3_test_mode                : 1; /* 3 */
		u32 reserved4                              : 4; /* 7:4 */
		u32 dphy0_force_pll                        : 1; /* 8 */
		u32 dphy1_force_pll                        : 1; /* 9 */
		u32 reserved7                              :22; /* 31:10 */
	} af;
	u32 a32;
} crg_test_mode_and_other_control;

/*------------------------------------------------------------------
	0x31404000 crg_axi_prot_control_0
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 bmi0_axi_prot_control                  : 3; /* 2:0 */
		u32 reserved1                              : 1; /* 3 */
		u32 bmi1_axi_prot_control                  : 3; /* 6:4 */
		u32 reserved3                              : 1; /* 7 */
		u32 bmi2_axi_prot_control                  : 3; /* 10:8 */
		u32 reserved5                              : 1; /* 11 */
		u32 bmi3_axi_prot_control                  : 3; /* 14:12 */
		u32 reserved7                              : 1; /* 15 */
		u32 bmi4_axi_prot_control                  : 3; /* 18:16 */
		u32 reserved9                              : 1; /* 19 */
		u32 bmi5_axi_prot_control                  : 3; /* 22:20 */
		u32 reserved11                             : 1; /* 23 */
		u32 bmi6_axi_prot_control                  : 3; /* 26:24 */
		u32 reserved13                             : 1; /* 27 */
		u32 bmi7_axi_prot_control                  : 3; /* 30:28 */
		u32 reserved15                             : 1; /* 31 */
	} af;
	u32 a32;
} crg_axi_prot_control_0;

/*------------------------------------------------------------------
	0x31404004 crg_axi_prot_control_1
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 bmi8_axi_prot_control                  : 3; /* 2:0 */
		u32 reserved1                              : 1; /* 3 */
		u32 bmi9_axi_prot_control                  : 3; /* 6:4 */
		u32 reserved3                              : 1; /* 7 */
		u32 bmi10_axi_prot_control                 : 3; /* 10:8 */
		u32 reserved5                              :21; /* 31:11 */
	} af;
	u32 a32;
} crg_axi_prot_control_1;

/*------------------------------------------------------------------
	0x31404008 crg_secure_mode_control
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 vscl0_secure_mode                      : 1; /* 0 */
		u32 vscl1_secure_mode                      : 1; /* 1 */
		u32 vdma_secure_mode                       : 1; /* 2 */
		u32 xd_secure_mode                         : 1; /* 3 */
		u32 formtter_secure_mode                   : 1; /* 4 */
		u32 gdma0_secure_mode                      : 1; /* 5 */
		u32 gdma1_secure_mode                      : 1; /* 6 */
		u32 gdma2_secure_mode                      : 1; /* 7 */
		u32 overlay0_secure_mode                   : 1; /* 8 */
		u32 overlay1_secure_mode                   : 1; /* 9 */
		u32 overlay2_secure_mode                   : 1; /* 10 */
		u32 cabc_secure_mode                       : 1; /* 11 */
		u32 gscl_secure_mode                       : 1; /* 12 */
		u32 rotator_secure_mode                    : 1; /* 13 */
		u32 sync_gen_secure_mode                   : 1; /* 14 */
		u32 dip0_secure_mode                       : 1; /* 15 */
		u32 mipi0_secure_mode                      : 1; /* 16 */
		u32 dip1_secure_mode                       : 1; /* 17 */
		u32 mipi1_secure_mode                      : 1; /* 18 */
		u32 gfx0_secure_mode                       : 1; /* 19 */
		u32 gfx1_secure_mode                       : 1; /* 20 */
		u32 dip2_secure_mode                       : 1; /* 21 */
		u32 hdmi_secure_mode                       : 1; /* 22 */
		u32 reserved23                             : 9; /* 31:23 */
	} af;
	u32 a32;
} crg_secure_mode_control;

/*------------------------------------------------------------------
	0x31405000 crg_gfx_control_register
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 gfx0_flush_enable                      : 1; /* 0 */
		u32 gfx1_flush_enable                      : 1; /* 1 */
		u32 reserved2                              :30; /* 31:2 */
	} af;
	u32 a32;
} crg_gfx_control_register;

/*------------------------------------------------------------------
	0x31405004 crg_gfx_status_register
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 gfx0_flush_done                        : 1; /* 0 */
		u32 gfx1_flush_done                        : 1; /* 1 */
		u32 reserved2                              :30; /* 31:2 */
	} af;
	u32 a32;
} crg_gfx_status_register;

/*------------------------------------------------------------------
	0x31406000 crg_gpio_ppu_control
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 ppu_for_gpio_pad_0                     : 1; /* 0 */
		u32 ppu_for_gpio_pad_1                     : 1; /* 1 */
		u32 ppu_for_gpio_pad_2                     : 1; /* 2 */
		u32 ppu_for_gpio_pad_3                     : 1; /* 3 */
		u32 ppu_for_gpio_pad_4                     : 1; /* 4 */
		u32 ppu_for_gpio_pad_5                     : 1; /* 5 */
		u32 ppu_for_gpio_pad_6                     : 1; /* 6 */
		u32 ppu_for_gpio_pad_7                     : 1; /* 7 */
		u32 ppu_for_gpio_pad_8                     : 1; /* 8 */
		u32 ppu_for_gpio_pad_9                     : 1; /* 9 */
		u32 ppu_for_gpio_pad_10                    : 1; /* 10 */
		u32 ppu_for_gpio_pad_11                    : 1; /* 11 */
		u32 ppu_for_gpio_pad_12                    : 1; /* 12 */
		u32 ppu_for_gpio_pad_13                    : 1; /* 13 */
		u32 ppu_for_gpio_pad_14                    : 1; /* 14 */
		u32 ppu_for_gpio_pad_15                    : 1; /* 15 */
		u32 ppu_for_gpio_pad_16                    : 1; /* 16 */
		u32 ppu_for_gpio_pad_17                    : 1; /* 17 */
		u32 ppu_for_gpio_pad_18                    : 1; /* 18 */
		u32 ppu_for_gpio_pad_19                    : 1; /* 19 */
		u32 ppu_for_gpio_pad_20                    : 1; /* 20 */
		u32 ppu_for_gpio_pad_21                    : 1; /* 21 */
		u32 ppu_for_gpio_pad_22                    : 1; /* 22 */
		u32 ppu_for_gpio_pad_23                    : 1; /* 23 */
		u32 ppu_for_gpio_pad_24                    : 1; /* 24 */
		u32 ppu_for_gpio_pad_25                    : 1; /* 25 */
		u32 ppu_for_gpio_pad_26                    : 1; /* 26 */
		u32 ppu_for_gpio_pad_27                    : 1; /* 27 */
		u32 ppu_for_gpio_pad_28                    : 1; /* 28 */
		u32 ppu_for_gpio_pad_29                    : 1; /* 29 */
		u32 ppu_for_gpio_pad_30                    : 1; /* 30 */
		u32 ppu_for_gpio_pad_31                    : 1; /* 31 */
	} af;
	u32 a32;
} crg_gpio_ppu_control;

/*------------------------------------------------------------------
	0x31406004 crg_gpio_ppd_control
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 ppd_for_gpio_pad_0                     : 1; /* 0 */
		u32 ppd_for_gpio_pad_1                     : 1; /* 1 */
		u32 ppd_for_gpio_pad_2                     : 1; /* 2 */
		u32 ppd_for_gpio_pad_3                     : 1; /* 3 */
		u32 ppd_for_gpio_pad_4                     : 1; /* 4 */
		u32 ppd_for_gpio_pad_5                     : 1; /* 5 */
		u32 ppd_for_gpio_pad_6                     : 1; /* 6 */
		u32 ppd_for_gpio_pad_7                     : 1; /* 7 */
		u32 ppd_for_gpio_pad_8                     : 1; /* 8 */
		u32 ppd_for_gpio_pad_9                     : 1; /* 9 */
		u32 ppd_for_gpio_pad_10                    : 1; /* 10 */
		u32 ppd_for_gpio_pad_11                    : 1; /* 11 */
		u32 ppd_for_gpio_pad_12                    : 1; /* 12 */
		u32 ppd_for_gpio_pad_13                    : 1; /* 13 */
		u32 ppd_for_gpio_pad_14                    : 1; /* 14 */
		u32 ppd_for_gpio_pad_15                    : 1; /* 15 */
		u32 ppd_for_gpio_pad_16                    : 1; /* 16 */
		u32 ppd_for_gpio_pad_17                    : 1; /* 17 */
		u32 ppd_for_gpio_pad_18                    : 1; /* 18 */
		u32 ppd_for_gpio_pad_19                    : 1; /* 19 */
		u32 ppd_for_gpio_pad_20                    : 1; /* 20 */
		u32 ppd_for_gpio_pad_21                    : 1; /* 21 */
		u32 ppd_for_gpio_pad_22                    : 1; /* 22 */
		u32 ppd_for_gpio_pad_23                    : 1; /* 23 */
		u32 ppd_for_gpio_pad_24                    : 1; /* 24 */
		u32 ppd_for_gpio_pad_25                    : 1; /* 25 */
		u32 ppd_for_gpio_pad_26                    : 1; /* 26 */
		u32 ppd_for_gpio_pad_27                    : 1; /* 27 */
		u32 ppd_for_gpio_pad_28                    : 1; /* 28 */
		u32 ppd_for_gpio_pad_29                    : 1; /* 29 */
		u32 ppd_for_gpio_pad_30                    : 1; /* 30 */
		u32 ppd_for_gpio_pad_31                    : 1; /* 31 */
	} af;
	u32 a32;
} crg_gpio_ppd_control;

/*------------------------------------------------------------------
	0x3140f000 crg_secure_interrupt_pending_register
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 vsync_mipi0_interrupt                  : 1; /* 0 */
		u32 vsync_mipi1_interrupt                  : 1; /* 1 */
		u32 vsync_hdmi_interrupt                   : 1; /* 2 */
		u32 fmt_interrupt                          : 1; /* 3 */
		u32 xd_interrupt                           : 1; /* 4 */
		u32 rtt_interrupt                          : 1; /* 5 */
		u32 dip0_interrupt                         : 1; /* 6 */
		u32 mipi0_interrupt                        : 1; /* 7 */
		u32 dip1_interrupt                         : 1; /* 8 */
		u32 mipi1_interrupt                        : 1; /* 9 */
		u32 dip2_interrupt                         : 1; /* 10 */
		u32 hdmi_interrupt                         : 1; /* 11 */
		u32 hdmi_wakeup_interrupt                  : 1; /* 12 */
		u32 cabc0_interrupt                        : 1; /* 13 */
		u32 cabc1_interrupt                        : 1; /* 14 */
		u32 reserved15                             :17; /* 31:15 */
	} af;
	u32 a32;
} crg_secure_interrupt_pending_register;

/*------------------------------------------------------------------
	0x3140f004 crg_secure_interrupt_mask_register
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 vsync_mipi0_interrupt_mask             : 1; /* 0 */
		u32 vsync_mipi1_interrupt_mask             : 1; /* 1 */
		u32 vsync_hdmi_interrupt_mask              : 1; /* 2 */
		u32 fmt_interrupt_mask                     : 1; /* 3 */
		u32 xd_interrupt_mask                      : 1; /* 4 */
		u32 rtt_interrupt_mask                     : 1; /* 5 */
		u32 dip0_interrupt_mask                    : 1; /* 6 */
		u32 mipi0_interrupt_mask                   : 1; /* 7 */
		u32 dip1_interrupt_mask                    : 1; /* 8 */
		u32 mipi1_interrupt_mask                   : 1; /* 9 */
		u32 dip2_interrupt_mask                    : 1; /* 10 */
		u32 hdmi_interrupt_mask                    : 1; /* 11 */
		u32 hdmi_wakeup_interrupt_mask             : 1; /* 12 */
		u32 cabc0_interrupt_mask                   : 1; /* 13 */
		u32 cabc1_interrupt_mask                   : 1; /* 14 */
		u32 reserved15                             :17; /* 31:15 */
	} af;
	u32 a32;
} crg_secure_interrupt_mask_register;

/*------------------------------------------------------------------
	0x3140f008 crg_secure_interrupt_mode
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 vsync_mipi0_interrupt_mode             : 1; /* 0 */
		u32 vsync_mipi1_interrupt_mode             : 1; /* 1 */
		u32 vsync_hdmi_interrupt_mode              : 1; /* 2 */
		u32 fmt_interrupt_mode                     : 1; /* 3 */
		u32 xd_interrupt_mode                      : 1; /* 4 */
		u32 rtt_interrupt_mode                     : 1; /* 5 */
		u32 dip0_interrupt_mode                    : 1; /* 6 */
		u32 mipi0_interrupt_mode                   : 1; /* 7 */
		u32 dip1_interrupt_mode                    : 1; /* 8 */
		u32 mipi1_interrupt_mode                   : 1; /* 9 */
		u32 dip2_interrupt_mode                    : 1; /* 10 */
		u32 hdmi_interrupt_mode                    : 1; /* 11 */
		u32 hdmi_wakeup_interrupt_mode             : 1; /* 12 */
		u32 cabc0_interrupt_mode                   : 1; /* 13 */
		u32 cabc1_interrupt_mode                   : 1; /* 14 */
		u32 reserved15                             :17; /* 31:15 */
	} af;
	u32 a32;
} crg_secure_interrupt_mode;

/*------------------------------------------------------------------
	0x3140f800 crg_non_secure_interrupt_pending_register
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 vsync_mipi0_interrupt                  : 1; /* 0 */
		u32 vsync_mipi1_interrupt                  : 1; /* 1 */
		u32 vsync_hdmi_interrupt                   : 1; /* 2 */
		u32 fmt_interrupt                          : 1; /* 3 */
		u32 xd_interrupt                           : 1; /* 4 */
		u32 rtt_interrupt                          : 1; /* 5 */
		u32 dip0_interrupt                         : 1; /* 6 */
		u32 mipi0_interrupt                        : 1; /* 7 */
		u32 dip1_interrupt                         : 1; /* 8 */
		u32 mipi1_interrupt                        : 1; /* 9 */
		u32 dip2_interrupt                         : 1; /* 10 */
		u32 hdmi_interrupt                         : 1; /* 11 */
		u32 hdmi_wakeup_interrupt                  : 1; /* 12 */
		u32 cabc0_interrupt                        : 1; /* 13 */
		u32 cabc1_interrupt                        : 1; /* 14 */
		u32 reserved15                             :17; /* 31:15 */
	} af;
	u32 a32;
} crg_non_secure_interrupt_pending_register;

/*------------------------------------------------------------------
	0x3140f804 crg_non_secure_interrupt_mask_register
------------------------------------------------------------------*/
typedef union {
	struct {
		u32 vsync_mipi0_interrupt_mask             : 1; /* 0 */
		u32 vsync_mipi1_interrupt_mask             : 1; /* 1 */
		u32 vsync_hdmi_interrupt_mask              : 1; /* 2 */
		u32 fmt_interrupt_mask                     : 1; /* 3 */
		u32 xd_interrupt_mask                      : 1; /* 4 */
		u32 rtt_interrupt_mask                     : 1; /* 5 */
		u32 dip0_interrupt_mask                    : 1; /* 6 */
		u32 mipi0_interrupt_mask                   : 1; /* 7 */
		u32 dip1_interrupt_mask                    : 1; /* 8 */
		u32 mipi1_interrupt_mask                   : 1; /* 9 */
		u32 dip2_interrupt_mask                    : 1; /* 10 */
		u32 hdmi_interrupt_mask                    : 1; /* 11 */
		u32 hdmi_wakeup_interrupt_mask             : 1; /* 12 */
		u32 cabc0_interrupt_mask                   : 1; /* 13 */
		u32 cabc1_interrupt_mask                   : 1; /* 14 */
		u32 reserved15                             :17; /* 31:15 */
	} af;
	u32 a32;
} crg_non_secure_interrupt_mask_register;

typedef struct {
	crg_core_clk_selection        	core_clk_selection;
	crg_core_clock_enable         	core_clock_enable;
	crg_other_clock_control       	other_clock_control;
	crg_clock_divide_value_0      	clock_divide_value_0;
	crg_clock_divide_value_1      	clock_divide_value_1;
	crg_clock_divide_value_update 	clock_divide_value_update;
	crg_soft_reset                	soft_reset;
	crg_master_port_arbitration_control	master_port_arbitration_control;
	crg_memory_deep_sleep_mode_control	memory_deep_sleep_mode_control;
	crg_test_mode_and_other_control	test_mode_and_other_control;
	crg_axi_prot_control_0        	axi_prot_control_0;
	crg_axi_prot_control_1        	axi_prot_control_1;
	crg_secure_mode_control       	secure_mode_control;
	crg_gfx_control_register      	gfx_control_register;
	crg_gfx_status_register       	gfx_status_register;
	crg_gpio_ppu_control          	gpio_ppu_control;
	crg_gpio_ppd_control          	gpio_ppd_control;
	crg_secure_interrupt_pending_register	secure_interrupt_pending_register;
	crg_secure_interrupt_mask_register	secure_interrupt_mask_register;
	crg_secure_interrupt_mode     	secure_interrupt_mode;
	crg_non_secure_interrupt_pending_register
		non_secure_interrupt_pending_register;
	crg_non_secure_interrupt_mask_register	non_secure_interrupt_mask_register;
} DSS_CRG_CRG_REG_T;

#endif

