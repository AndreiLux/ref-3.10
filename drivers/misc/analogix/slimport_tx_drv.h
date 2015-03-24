// ---------------------------------------------------------------------------
// Analogix Confidential Strictly Private
//
//
// ---------------------------------------------------------------------------
// >>>>>>>>>>>>>>>>>>>>>>>>> COPYRIGHT NOTICE <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
// ---------------------------------------------------------------------------
// Copyright 2004-2012 (c) Analogix
//
//Analogix owns the sole copyright to this software. Under international
// copyright laws you (1) may not make a copy of this software except for
// the purposes of maintaining a single archive copy, (2) may not derive
// works herefrom, (3) may not distribute this work to others. These rights
// are provided for information clarification, other restrictions of rights
// may apply as well.
//
// This is an unpublished work.
// ---------------------------------------------------------------------------
// >>>>>>>>>>>>>>>>>>>>>>>>>>>> WARRANTEE <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
// ---------------------------------------------------------------------------
// Analogix  MAKES NO WARRANTY OF ANY KIND WITH REGARD TO THE USE OF
// THIS SOFTWARE, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR
// PURPOSE.
// ---------------------------------------------------------------------------

#ifndef _SP_TX_DRV_H
#define _SP_TX_DRV_H

#ifdef _SP_TX_DRV_C_
#define _SP_TX_DRV_EX_C_
#else
#define _SP_TX_DRV_EX_C_ extern
#endif
#include "sp_tx_reg.h"
#include "slimport.h"

#ifdef CEC_ENABLE
enum CEC_CTRL_STAS {
	CEC_IDLE,
	CEC_INIT,
	CEC_HDMI_RX_REC,
	CEC_DOWNSTREAM_HDMI_READ,
	CEC_STATUS_NUMS
};
#endif

#define FW_VERSION 0x12


#define _BIT0	0x01
#define _BIT1	0x02
#define _BIT2	0x04
#define _BIT3	0x08
#define _BIT4	0x10
#define _BIT5	0x20
#define _BIT6	0x40
#define _BIT7	0x80

#define _bit0_(val)  ((bit)(val & _BIT0))
#define _bit1_(val)  ((bit)(val & _BIT1))
#define _bit2_(val)  ((bit)(val & _BIT2))
#define _bit3_(val)  ((bit)(val & _BIT3))
#define _bit4_(val)  ((bit)(val & _BIT4))
#define _bit5_(val)  ((bit)(val & _BIT5))
#define _bit6_(val)  ((bit)(val & _BIT6))
#define _bit7_(val)  ((bit)(val & _BIT7))


enum CO3_CHIPID {
	ANX7818,
	ANX7816,
	ANX7812,
	ANX7810,
	ANX7806,
	ANX7802,
	CO3_NUMS,
};
#define DVI_MODE 0x00
#define HDMI_MODE 0x01

enum RX_CBL_TYPE {
	DWN_STRM_IS_NULL,
	DWN_STRM_IS_HDMI_7730,
	DWN_STRM_IS_DIGITAL,
	DWN_STRM_IS_ANALOG,
	DWN_STRM_IS_VGA_9832 = 0x04
};

enum CHARGING_STATUS {
	NO_CHARGING_CAPABLE = 0x00,
	NO_FAST_CHARGING = 0x01,
	FAST_CHARGING = 0x02,
	ERR_STATUS
};

enum SP_TX_System_State {
	STATE_INIT = 0,
	STATE_WAITTING_CABLE_PLUG = 1,
	STATE_SP_INITIALIZED = 2,
	STATE_SINK_CONNECTION = 3,
	STATE_PARSE_EDID = 4,
	STATE_LINK_TRAINING = 5,
	STATE_VIDEO_OUTPUT = 6,
	STATE_HDCP_AUTH = 7,
	STATE_AUDIO_OUTPUT = 8,
	STATE_PLAY_BACK = 9
};


enum SP_TX_POWER_BLOCK {
	SP_TX_PWR_REG = REGISTER_PD,
	SP_TX_PWR_HDCP = HDCP_PD,
	SP_TX_PWR_AUDIO = AUDIO_PD,
	SP_TX_PWR_VIDEO = VIDEO_PD,
	SP_TX_PWR_LINK = LINK_PD,
	SP_TX_PWR_TOTAL = TOTAL_PD,
	SP_TX_PWR_NUMS
};
enum HDMI_color_depth {
       Hdmi_legacy = 0x00,
	Hdmi_24bit = 0x04,
	Hdmi_30bit = 0x05,
	Hdmi_36bit = 0x06,
	Hdmi_48bit = 0x07,
};

enum SP_TX_SEND_MSG {
    MSG_OCM_EN,
    MSG_INPUT_HDMI,
    MSG_INPUT_DVI,
    MSG_CLEAR_IRQ,
};

enum SINK_CONNECTION_STATUS {
	SC_INIT,
	SC_CHECK_CABLE_TYPE,
	SC_WAITTING_CABLE_TYPE = SC_CHECK_CABLE_TYPE+5,
	SC_SINK_CONNECTED,
	SC_NOT_CABLE,
	SC_STATE_NUM
};
typedef enum
{
	CHECK_AUXCH,
	GETTED_CABLE_TYPE,
	CABLE_TYPE_STATE_NUM
} CABLE_TYPE_STATUS;

enum SP_TX_LT_STATUS {
	LT_INIT,
	LT_WAIT_PLL_LOCK,
	LT_CHECK_LINK_BW,
	LT_START,
	LT_WAITTING_FINISH,
	LT_ERROR,
	LT_FINISH,
	LT_END,
	LT_STATES_NUM
};

enum HDCP_STATUS {
	HDCP_CAPABLE_CHECK,
	HDCP_WAITTING_VID_STB,
	HDCP_HW_ENABLE,
	HDCP_WAITTING_FINISH,
	HDCP_FINISH,
	HDCP_FAILE,
	HDCP_NOT_SUPPORT,
	HDCP_PROCESS_STATE_NUM
};

enum VIDEO_OUTPUT_STATUS {
	VO_WAIT_VIDEO_STABLE,
	VO_WAIT_TX_VIDEO_STABLE,
	//VO_WAIT_PLL_LOCK,
	VO_CHECK_VIDEO_INFO,
	VO_FINISH,
	VO_STATE_NUM
};
enum AUDIO_OUTPUT_STATUS {
	AO_INIT,
	AO_CTS_RCV_INT,
	AO_AUDIO_RCV_INT,
	AO_RCV_INT_FINISH,
	AO_OUTPUT,
	AO_STATE_NUM
};
struct Packet_AVI{
	unchar AVI_data[13];
} ;


struct Packet_SPD{
	unchar SPD_data[25];
};


struct Packet_MPEG{
	unchar MPEG_data[13];
} ;


struct AudiInfoframe {
	unchar type;
	unchar version;
	unchar length;
	unchar pb_byte[11];
};


enum PACKETS_TYPE {
	AVI_PACKETS,
	SPD_PACKETS,
	MPEG_PACKETS,
	VSI_PACKETS,
	AUDIF_PACKETS
};
struct COMMON_INT {
	unchar common_int[5];
	unchar change_flag;
};
struct HDMI_RX_INT {
	unchar hdmi_rx_int[7];
	unchar change_flag;
};

enum xtal_enum {
	XTAL_19D2M,
	XTAL_24M,
	XTAL_25M,
	XTAL_26M,
	XTAL_27M,
	XTAL_38D4M,
	XTAL_52M,
	XTAL_NOT_SUPPORT,
	XTAL_CLK_NUM
};

//xjh add SSC settings
enum SP_SSC_DEP {
	SSC_DEP_DISABLE = 0x0,
	SSC_DEP_500PPM,
	SSC_DEP_1000PPM,
	SSC_DEP_1500PPM,
	SSC_DEP_2000PPM,
	SSC_DEP_2500PPM,
	SSC_DEP_3000PPM,
	SSC_DEP_3500PPM,
	SSC_DEP_4000PPM,
	SSC_DEP_4500PPM,
	SSC_DEP_5000PPM,
	SSC_DEP_5500PPM,
	SSC_DEP_6000PPM
};

typedef struct clock_data{
   unsigned char xtal_clk;
   unsigned int xtal_clk_m10;
} clock_Data;


#define AUX_ERR  1
#define AUX_OK   0


#define SP_POWER_ON 1
#define SP_POWER_DOWN 0

#define VID_DVI_MODE 0x00
#define VID_HDMI_MODE 0x01

#define anx7732_ID_H 0x77
#define anx7732_ID_L 0x32

_SP_TX_DRV_EX_C_ enum HDCP_STATUS HDCP_state;

_SP_TX_DRV_EX_C_ enum SP_TX_System_State sp_tx_system_state;
_SP_TX_DRV_EX_C_ enum SP_TX_System_State sp_tx_system_state_bak;
_SP_TX_DRV_EX_C_ enum RX_CBL_TYPE sp_tx_rx_type;
_SP_TX_DRV_EX_C_ enum SP_TX_LT_STATUS sp_tx_LT_state;
_SP_TX_DRV_EX_C_ enum SINK_CONNECTION_STATUS sp_tx_sc_state;
_SP_TX_DRV_EX_C_ enum VIDEO_OUTPUT_STATUS sp_tx_vo_state;
_SP_TX_DRV_EX_C_ enum AUDIO_OUTPUT_STATUS sp_tx_ao_state;
_SP_TX_DRV_EX_C_ struct COMMON_INT common_int_status;
_SP_TX_DRV_EX_C_ struct HDMI_RX_INT hdmi_rx_int_status;
_SP_TX_DRV_EX_C_  unsigned char g_need_clean_status;

_SP_TX_DRV_EX_C_ enum CHARGING_STATUS downstream_charging_status; //xjh add for charging

#define COMMON_INT1 common_int_status.common_int[0]
#define COMMON_INT2 common_int_status.common_int[1]
#define COMMON_INT3 common_int_status.common_int[2]
#define COMMON_INT4 common_int_status.common_int[3]
#define COMMON_INT5 common_int_status.common_int[4]
#define COMMON_INT_CHANGED common_int_status.change_flag
#define HDMI_RX_INT1 hdmi_rx_int_status.hdmi_rx_int[0]
#define HDMI_RX_INT2 hdmi_rx_int_status.hdmi_rx_int[1]
#define HDMI_RX_INT3 hdmi_rx_int_status.hdmi_rx_int[2]
#define HDMI_RX_INT4 hdmi_rx_int_status.hdmi_rx_int[3]
#define HDMI_RX_INT5 hdmi_rx_int_status.hdmi_rx_int[4]
#define HDMI_RX_INT6 hdmi_rx_int_status.hdmi_rx_int[5]
#define HDMI_RX_INT7 hdmi_rx_int_status.hdmi_rx_int[6]
#define HDMI_RX_INT_CHANGED hdmi_rx_int_status.change_flag

#define MAKE_WORD(ch,cl) ((uint)(((uint)ch<<8) | (uint)cl))
#define MAX_BUF_CNT 16

#define sp_tx_aux_polling_enable(ret) sp_write_reg_or(TX_P0, TX_DEBUG1, POLLING_EN, ret)
#define sp_tx_aux_polling_disable(ret) sp_write_reg_and(TX_P0, TX_DEBUG1, ~POLLING_EN, ret)

static inline void reg_bit_ctl(u8 addr, u8 offset, u8 data, bool enable)
{
	u8 c;
	sp_read_reg(addr, offset, &c);
	if (enable) {
		if ((c & data) != data) {
			c |= data;
			sp_write_reg(addr, offset, c);
		}
	} else {
		if ((c & data) == data) {
			c &= ~data;
			sp_write_reg(addr, offset, c);
		}
	}
}

#define sp_tx_video_mute(enable) \
	reg_bit_ctl(TX_P2, VID_CTRL1, VIDEO_MUTE, enable)
#define hdmi_rx_mute_audio(enable) \
	reg_bit_ctl(RX_P0, RX_MUTE_CTRL, AUD_MUTE, enable)
#define hdmi_rx_mute_video(enable) \
	reg_bit_ctl(RX_P0, RX_MUTE_CTRL, VID_MUTE, enable)
#define sp_tx_addronly_set(enable) \
	reg_bit_ctl(TX_P0, AUX_CTRL2, ADDR_ONLY_BIT, enable)

#define sp_tx_set_link_bw(bw) \
	sp_write_reg(TX_P0, SP_TX_LINK_BW_SET_REG, bw);
#define sp_tx_get_link_bw(buf) \
	__read_reg(TX_P0, SP_TX_LINK_BW_SET_REG, buf)

#define sp_tx_get_pll_lock_status(status, ret) do {\
	unchar tmp; \
	*ret = __read_reg(TX_P0, TX_DEBUG1, &tmp); \
	*status = ((tmp & DEBUG_PLL_LOCK) != 0 ? 1 : 0); \
	} while (0)

#define gen_M_clk_with_downspeading(ret) \
	sp_write_reg_or(TX_P0, SP_TX_M_CALCU_CTRL, M_GEN_CLK_SEL, ret)
#define gen_M_clk_without_downspeading(ret) \
	sp_write_reg_and(TX_P0, SP_TX_M_CALCU_CTRL, (~M_GEN_CLK_SEL), ret)

#define hdmi_rx_set_hpd(enable, ret) do { \
	if ((bool)enable) \
		sp_write_reg_or(TX_P2, SP_TX_VID_CTRL3_REG, HPD_OUT, ret); \
	else \
		sp_write_reg_and(TX_P2, SP_TX_VID_CTRL3_REG, ~HPD_OUT, ret); \
	} while (0)

#define hdmi_rx_set_termination(enable, ret) do { \
	if ((bool)enable) \
		sp_write_reg_and(RX_P0, HDMI_RX_TMDS_CTRL_REG7, ~TERM_PD, ret); \
	else \
		sp_write_reg_or(RX_P0, HDMI_RX_TMDS_CTRL_REG7, TERM_PD, ret); \
	} while (0)

#define sp_tx_get_rx_bw(pdata) \
	sp_tx_aux_dpcdread_bytes(0x00, 0x00, DPCD_MAX_LINK_RATE, 1, pdata)

#define sp_tx_clean_hdcp_status(ret) do { \
	sp_write_reg(TX_P0, TX_HDCP_CTRL0, 0x03); \
	sp_write_reg_or(TX_P0, TX_HDCP_CTRL0, RE_AUTH, ret); \
	delay_ms(2); \
	debug_puts("sp_tx_clean_hdcp_status\n"); \
	}while(0)



/*#define wait_aux_op_finish(err_flag, ret) \
	do{ \
		unchar idata cnt, c, tmp; \
		*ret = sp_read_reg(TX_P0, AUX_CTRL, &c);\
		if (((c&0x0c)>>2) == 0x01) {\
			cnt = 10; \
			*ret = __read_reg(TX_P0, AUX_CTRL2, &tmp); \
			while (tmp & AUX_OP_EN) { \
				msleep(2); \
				if ((cnt--) == 0) { \
					debug_puts("g_edid_break = 1\n"); \
					*err_flag = 1; \
					break; \
				} \
				*ret = __read_reg(TX_P0, AUX_CTRL2, &tmp); \
				if (*ret != 2) {\
					*err_flag = 1; \
					break; \
				} \
			} \
		}\
		else {\
				cnt = 100; \
				*ret = __read_reg(TX_P0, SP_TX_AUX_STATUS, &tmp); \
				while (tmp & AUX_BUSY) { \
					if ((cnt--) == 0) { \
						debug_puts("AUX Operaton does not finished, and time out.\n"); \
						*err_flag = 1; \
						break; \
					} \
					*ret = __read_reg(TX_P0, SP_TX_AUX_STATUS, &tmp); \
					if (*ret != 2) {\
						*err_flag = 1; \
						break; \
					} \
				} \
			}\
	}while(0)
*/
#define sp_tx_set_sys_state(ss) \
	do { \
		int ret; \
		debug_printf("set: clean_status: %x, ", (uint)g_need_clean_status); \
		if ((sp_tx_system_state >= STATE_LINK_TRAINING) && \
			(ss < STATE_LINK_TRAINING)) \
			sp_write_reg_or(TX_P0, SP_TX_ANALOG_PD_REG, CH0_PD, &ret); \
		sp_tx_system_state_bak = sp_tx_system_state; \
		sp_tx_system_state = (unchar)ss; \
		g_need_clean_status = 1; \
		print_sys_state(sp_tx_system_state); \
	}while(0)

#define goto_next_system_state() \
	do { \
		debug_printf("next: clean_status: %x, ", (uint)g_need_clean_status); \
		sp_tx_system_state_bak = sp_tx_system_state; \
		sp_tx_system_state++;\
		print_sys_state(sp_tx_system_state); \
	}while(0)

#define redo_cur_system_state() \
	do { \
		debug_printf("redo: clean_status: %x, ", (uint)g_need_clean_status); \
		g_need_clean_status = 1; \
		sp_tx_system_state_bak = sp_tx_system_state; \
		print_sys_state(sp_tx_system_state); \
	}while(0)



/*#define system_state_change(status) \
		do { \
			debug_printf("change: clean_status: %x, ", (uint)g_need_clean_status); \
			g_need_clean_status = 1; \
			sp_tx_system_state_bak = sp_tx_system_state; \
			sp_tx_system_state = (unchar)status; \
			print_sys_state(sp_tx_system_state); \
		}while(0)
*/

#define system_state_change_with_case(status) \
	do{ \
		int ret; \
		if(sp_tx_system_state >= status) { \
			debug_printf("change_case: clean_status: %xm, ", (uint)g_need_clean_status); \
			if ((sp_tx_system_state >= STATE_LINK_TRAINING) && \
				(status < STATE_LINK_TRAINING)) \
				sp_write_reg_or(TX_P0, SP_TX_ANALOG_PD_REG, CH0_PD, &ret); \
			g_need_clean_status = 1; \
			sp_tx_system_state_bak = sp_tx_system_state; \
			sp_tx_system_state = (unchar)status; \
			print_sys_state(sp_tx_system_state); \
		} \
	}while(0)

#define write_dpcd_addr(addrh, addrm, addrl, ret) \
	do{ \
		unchar temp; \
		unchar addr0, addr8; \
		__read_reg(TX_P0, AUX_ADDR_7_0, &addr0); \
		__read_reg(TX_P0, AUX_ADDR_15_8, &addr8); \
		if (addr0 != (unchar)addrl) { \
			*ret = sp_write_reg(TX_P0, AUX_ADDR_7_0, (unchar)addrl); \
		} \
		if (addr8 != (unchar)addrm) { \
			*ret = sp_write_reg(TX_P0, AUX_ADDR_15_8, (unchar)addrm); \
		} \
		sp_read_reg(TX_P0, AUX_ADDR_19_16, &temp); \
		if ((unchar)(temp & 0x0F)  != ((unchar)addrh & 0x0F)) { \
			*ret = sp_write_reg(TX_P0, AUX_ADDR_19_16, (temp  & 0xF0) | ((unchar)addrh)); \
		} \
	}while(0)



#define SP_BREAK(current_status, next_status)  if(next_status != (current_status) + 1) break

_SP_TX_DRV_EX_C_  unsigned char ext_int_index;
_SP_TX_DRV_EX_C_  unsigned char g_changed_bandwidth;
_SP_TX_DRV_EX_C_  unsigned char g_hdmi_dvi_status;
#ifdef ENABLE_READ_EDID
_SP_TX_DRV_EX_C_  unsigned char g_edid_break;
_SP_TX_DRV_EX_C_  unsigned char g_edid_checksum;
_SP_TX_DRV_EX_C_  unchar edid_blocks[256];
_SP_TX_DRV_EX_C_  unsigned char g_read_edid_flag;
#ifdef CO3_DEUBG_MSG
 _SP_TX_DRV_EX_C_ unsigned char bedid_print;//print EDID  xjh mod1 20130926
#endif
void sp_tx_edid_read_initial(void);
unchar sp_tx_get_edid_block(void);
void sp_tx_rst_aux(void);
void edid_read(unchar offset, unchar *pblock_buf);
bool sp_tx_edid_read(unchar *pBuf);
/* workaround */
static inline
int hdmi_common_read_edid_block(int block, unsigned char *edid_buf)
{
	int i;

	printk("mydp: ENABLE_READ_EDID is enabled...\n");
        if(block == 0) {
                for(i = 0; i < 128; i++) {
                        *edid_buf = edid_blocks[i];
                        edid_buf++;
                }
        } else if(block == 1) {
                for(i = 0; i < 128; i++) {
                        *edid_buf = edid_blocks[128 + i];
                        edid_buf++;
                }
        }
	return true;
}
#else
static inline
int hdmi_common_read_edid_block(int block, unsigned char *edid_buf)
{
	printk("mydp: ANALOGIX ENABLE_READ_EDID is not enable...\n");
	return false;
}
#endif
_SP_TX_DRV_EX_C_ struct AudiInfoframe sp_tx_audioinfoframe;
_SP_TX_DRV_EX_C_ struct Packet_AVI sp_tx_packet_avi;
_SP_TX_DRV_EX_C_ struct Packet_SPD sp_tx_packet_spd;
_SP_TX_DRV_EX_C_ struct Packet_MPEG sp_tx_packet_mpeg;


bool slimport_chip_detect(void);
void system_power_ctrl_0(void);
void slimport_main_process(void);
void slimport_set_xtal_freq(int clk);

unchar sp_tx_aux_dpcdread_bytes(unchar addrh, unchar addrm,
	unchar addrl, unchar cCount, unchar *pBuf);
unchar sp_tx_aux_dpcdwrite_bytes(unchar addrh, unchar addrm, unchar addrl, unchar cCount, unchar *pBuf);
unchar sp_tx_aux_dpcdwrite_byte(unchar addrh, unchar addrm, unchar addrl, unchar data1);
void sp_tx_show_infomation(void);
void hdmi_rx_show_video_info(void);
void slimport_block_power_ctrl(enum SP_TX_POWER_BLOCK sp_tx_pd_block, unchar power);
void vbus_power_ctrl(unsigned char ON);
void slimport_initialization(void);
unchar sp_tx_cur_states(void);
void print_sys_state(unchar ss) ;
void sp_tx_initialization(void);
extern bool i2c_master_read_reg(unchar Sink_device_sel, unchar offset, unchar * Buf);
extern bool i2c_master_write_reg(unchar Sink_device_sel,unchar offset, unchar value);
extern int notify_power_status_change(unsigned long status);
void sp_tx_variable_init(void);
unchar slimport_hdcp_cap_check(void);
#endif
