/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _AUDIO_DIGITAL_TYPE_H
#define _AUDIO_DIGITAL_TYPE_H

/*****************************************************************************
 *                ENUM DEFINITION
 *****************************************************************************/

enum Soc_Aud_Digital_Block {
	/* memmory interfrace */
	Soc_Aud_Digital_Block_MEM_DL1 = 0,
	Soc_Aud_Digital_Block_MEM_DL2,
	Soc_Aud_Digital_Block_MEM_VUL,
	Soc_Aud_Digital_Block_MEM_DAI,
	Soc_Aud_Digital_Block_MEM_I2S,	/* currently no use */
	Soc_Aud_Digital_Block_MEM_AWB,
	Soc_Aud_Digital_Block_MEM_MOD_DAI,
	/* connection to int main modem */
	Soc_Aud_Digital_Block_MODEM_PCM_1_O,
	/* connection to extrt/int modem */
	Soc_Aud_Digital_Block_MODEM_PCM_2_O,
	/* 1st I2S for DAC and ADC */
	Soc_Aud_Digital_Block_I2S_OUT_DAC,
	Soc_Aud_Digital_Block_I2S_IN_ADC,
	/* 2nd I2S */
	Soc_Aud_Digital_Block_I2S_OUT_2,
	Soc_Aud_Digital_Block_I2S_IN_2,
	/* HW gain contorl */
	Soc_Aud_Digital_Block_HW_GAIN1,
	Soc_Aud_Digital_Block_HW_GAIN2,
	/* megrge interface */
	Soc_Aud_Digital_Block_MRG_I2S_OUT,
	Soc_Aud_Digital_Block_MRG_I2S_IN,
	Soc_Aud_Digital_Block_DAI_BT,
	Soc_Aud_Digital_Block_HDMI,
	Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK,
	Soc_Aud_Digital_Block_NUM_OF_MEM_INTERFACE = Soc_Aud_Digital_Block_MEM_MOD_DAI + 1
};

enum Soc_Aud_MemIF_Direction {
	Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT,
	Soc_Aud_MemIF_Direction_DIRECTION_INPUT
};

enum Soc_Aud_InterConnectionInput {
	Soc_Aud_InterConnectionInput_I00,
	Soc_Aud_InterConnectionInput_I01,
	Soc_Aud_InterConnectionInput_I02,
	Soc_Aud_InterConnectionInput_I03,
	Soc_Aud_InterConnectionInput_I04,
	Soc_Aud_InterConnectionInput_I05,
	Soc_Aud_InterConnectionInput_I06,
	Soc_Aud_InterConnectionInput_I07,
	Soc_Aud_InterConnectionInput_I08,
	Soc_Aud_InterConnectionInput_I09,
	Soc_Aud_InterConnectionInput_I10,
	Soc_Aud_InterConnectionInput_I11,
	Soc_Aud_InterConnectionInput_I12,
	Soc_Aud_InterConnectionInput_I13,
	Soc_Aud_InterConnectionInput_I14,
	Soc_Aud_InterConnectionInput_I15,
	Soc_Aud_InterConnectionInput_I16,
	Soc_Aud_InterConnectionInput_Num_Input
};

enum Soc_Aud_InterConnectionOutput {
	Soc_Aud_InterConnectionOutput_O00,
	Soc_Aud_InterConnectionOutput_O01,
	Soc_Aud_InterConnectionOutput_O02,
	Soc_Aud_InterConnectionOutput_O03,
	Soc_Aud_InterConnectionOutput_O04,
	Soc_Aud_InterConnectionOutput_O05,
	Soc_Aud_InterConnectionOutput_O06,
	Soc_Aud_InterConnectionOutput_O07,
	Soc_Aud_InterConnectionOutput_O08,
	Soc_Aud_InterConnectionOutput_O09,
	Soc_Aud_InterConnectionOutput_O10,
	Soc_Aud_InterConnectionOutput_O11,
	Soc_Aud_InterConnectionOutput_O12,
	Soc_Aud_InterConnectionOutput_O13,
	Soc_Aud_InterConnectionOutput_O14,
	Soc_Aud_InterConnectionOutput_O15,
	Soc_Aud_InterConnectionOutput_O16,
	Soc_Aud_InterConnectionOutput_O17,
	Soc_Aud_InterConnectionOutput_O18,
	Soc_Aud_InterConnectionOutput_Num_Output
};

enum Soc_Aud_HDMI_InterConnectionInput {
	Soc_Aud_InterConnectionInput_I20 = 20,
	Soc_Aud_InterConnectionInput_I21,
	Soc_Aud_InterConnectionInput_I22,
	Soc_Aud_InterConnectionInput_I23,
	Soc_Aud_InterConnectionInput_I24,
	Soc_Aud_InterConnectionInput_I25,
	Soc_Aud_InterConnectionInput_I26,
	Soc_Aud_InterConnectionInput_I27,
	Soc_Aud_HDMI_CONN_INPUT_BASE = Soc_Aud_InterConnectionInput_I20,
	Soc_Aud_HDMI_CONN_INPUT_MAX = Soc_Aud_InterConnectionInput_I27,
	Soc_Aud_NUM_HDMI_INPUT = (Soc_Aud_HDMI_CONN_INPUT_MAX - Soc_Aud_HDMI_CONN_INPUT_BASE + 1)
};

enum Soc_Aud_HDMI_InterConnectionOutput {
	Soc_Aud_InterConnectionOutput_O20 = 20,
	Soc_Aud_InterConnectionOutput_O21,
	Soc_Aud_InterConnectionOutput_O22,
	Soc_Aud_InterConnectionOutput_O23,
	Soc_Aud_InterConnectionOutput_O24,
	Soc_Aud_InterConnectionOutput_O25,
	Soc_Aud_InterConnectionOutput_O26,
	Soc_Aud_InterConnectionOutput_O27,
	Soc_Aud_InterConnectionOutput_O28,
	Soc_Aud_InterConnectionOutput_O29,
	Soc_Aud_HDMI_CONN_OUTPUT_BASE = Soc_Aud_InterConnectionOutput_O20,
	Soc_Aud_HDMI_CONN_OUTPUT_MAX = Soc_Aud_InterConnectionOutput_O29,
	Soc_Aud_NUM_HDMI_OUTPUT = (Soc_Aud_HDMI_CONN_OUTPUT_MAX - Soc_Aud_HDMI_CONN_OUTPUT_BASE + 1)
};

enum Soc_Aud_InterConnectionState {
	Soc_Aud_InterCon_DisConnect = 0x0,
	Soc_Aud_InterCon_Connection = 0x1,
	Soc_Aud_InterCon_ConnectionShift = 0x2
};

enum STREAMSTATUS {
	STREAMSTATUS_STATE_FREE = -1,	/* memory is not allocate */
	STREAMSTATUS_STATE_STANDBY,	/* memory allocate and ready */
	STREAMSTATUS_STATE_EXECUTING,	/* stream is running */
};

enum Soc_Aud_TopClockType {
	Soc_Aud_TopClockType_APB_CLOCK = 1,
	Soc_Aud_TopClockType_APB_AFE_CLOCK = 2,
	Soc_Aud_TopClockType_APB_I2S_INPUT_CLOCK = 6,
	Soc_Aud_TopClockType_APB_AFE_CK_DIV_RRST = 16,
	Soc_Aud_TopClockType_APB_PDN_APLL_TUNER = 19,
	Soc_Aud_TopClockType_APB_PDN_HDMI_CK = 20,
	Soc_Aud_TopClockType_APB_PDN_SPDIF_CK = 21
};

enum Soc_Aud_AFEClockType {
	Soc_Aud_AFEClockType_AFE_ON = 0,
	Soc_Aud_AFEClockType_DL1_ON = 1,
	Soc_Aud_AFEClockType_DL2_ON = 2,
	Soc_Aud_AFEClockType_VUL_ON = 3,
	Soc_Aud_AFEClockType_DAI_ON = 4,
	Soc_Aud_AFEClockType_I2S_ON = 5,
	Soc_Aud_AFEClockType_AWB_ON = 6,
	Soc_Aud_AFEClockType_MOD_PCM_ON = 7
};

enum Soc_Aud_IRQ_MCU_MODE {
	Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE = 0,
	Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE,
	Soc_Aud_IRQ_MCU_MODE_IRQ3_MCU_MODE,
	Soc_Aud_IRQ_MCU_MODE_IRQ5_MCU_MODE,
	Soc_Aud_IRQ_MCU_MODE_NUM_OF_IRQ_MODE
};

enum Soc_Aud_Hw_Digital_Gain {
	Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1,
	Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN2
};

enum Soc_Aud_I2S_IN_PAD_SEL {
	Soc_Aud_I2S_IN_PAD_SEL_I2S_IN_FROM_CONNSYS = 0,
	Soc_Aud_I2S_IN_PAD_SEL_I2S_IN_FROM_IO_MUX = 1
};

enum Soc_Aud_LR_SWAP {
	Soc_Aud_LR_SWAP_NO_SWAP = 0,
	Soc_Aud_LR_SWAP_LR_DATASWAP = 1
};

enum Soc_Aud_INV_LRCK {
	Soc_Aud_INV_LRCK_NO_INVERSE = 0,
	Soc_Aud_INV_LRCK_INVESE_LRCK = 1
};

enum Soc_Aud_I2S_FORMAT {
	Soc_Aud_I2S_FORMAT_EIAJ = 0,
	Soc_Aud_I2S_FORMAT_I2S = 1
};

enum Soc_Aud_I2S_SRC {
	Soc_Aud_I2S_SRC_MASTER_MODE = 0,
	Soc_Aud_I2S_SRC_SLAVE_MODE = 1
};

enum Soc_Aud_I2S_WLEN {
	Soc_Aud_I2S_WLEN_WLEN_16BITS = 0,
	Soc_Aud_I2S_WLEN_WLEN_32BITS = 1
};

enum Soc_Aud_I2S_SAMPLERATE {
	Soc_Aud_I2S_SAMPLERATE_I2S_8K = 0,
	Soc_Aud_I2S_SAMPLERATE_I2S_11K = 1,
	Soc_Aud_I2S_SAMPLERATE_I2S_12K = 2,
	Soc_Aud_I2S_SAMPLERATE_I2S_16K = 4,
	Soc_Aud_I2S_SAMPLERATE_I2S_22K = 5,
	Soc_Aud_I2S_SAMPLERATE_I2S_24K = 6,
	Soc_Aud_I2S_SAMPLERATE_I2S_32K = 8,
	Soc_Aud_I2S_SAMPLERATE_I2S_44K = 9,
	Soc_Aud_I2S_SAMPLERATE_I2S_48K = 10
};

enum SOC_HDMI_INPUT_DATA_BIT_WIDTH {
	SOC_HDMI_INPUT_16BIT = 0,
	SOC_HDMI_INPUT_32BIT
};

enum SOC_HDMI_I2S_WLEN {
	SOC_HDMI_I2S_8BIT = 0,
	SOC_HDMI_I2S_16BIT,
	SOC_HDMI_I2S_24BIT,
	SOC_HDMI_I2S_32BIT
};

enum SOC_HDMI_I2S_DELAY_DATA {
	SOC_HDMI_I2S_NOT_DELAY = 0,
	SOC_HDMI_I2S_FIRST_BIT_1T_DELAY
};

enum SOC_HDMI_I2S_LRCK_INV {
	SOC_HDMI_I2S_LRCK_NOT_INVERSE = 0,
	SOC_HDMI_I2S_LRCK_INVERSE
};

enum SOC_HDMI_I2S_BCLK_INV {
	SOC_HDMI_I2S_BCLK_NOT_INVERSE = 0,
	SOC_HDMI_I2S_BCLK_INVERSE
};

struct AudioDigtalI2S {
	bool mLR_SWAP;
	bool mI2S_SLAVE;
	uint32_t mI2S_SAMPLERATE;
	bool mINV_LRCK;
	bool mI2S_FMT;
	bool mI2S_WLEN;
	bool mI2S_EN;
	bool mI2S_IN_PAD_SEL;

	/* her for ADC usage , DAC will not use this */
	int mBuffer_Update_word;
	bool mloopback;
	bool mFpga_bit;
	bool mFpga_bit_test;
};

enum Soc_Aud_TX_LCH_RPT {
	Soc_Aud_TX_LCH_RPT_TX_LCH_NO_REPEAT = 0,
	Soc_Aud_TX_LCH_RPT_TX_LCH_REPEAT = 1
};

enum Soc_Aud_VBT_16K_MODE {
	Soc_Aud_VBT_16K_MODE_VBT_16K_MODE_DISABLE = 0,
	Soc_Aud_VBT_16K_MODE_VBT_16K_MODE_ENABLE = 1
};

enum Soc_Aud_EXT_MODEM {
	Soc_Aud_EXT_MODEM_MODEM_2_USE_INTERNAL_MODEM = 0,
	Soc_Aud_EXT_MODEM_MODEM_2_USE_EXTERNAL_MODEM = 1
};

enum Soc_Aud_PCM_SYNC_TYPE {
	Soc_Aud_PCM_SYNC_TYPE_BCK_CYCLE_SYNC = 0,	/* bck sync length = 1 */
	Soc_Aud_PCM_SYNC_TYPE_EXTEND_BCK_CYCLE_SYNC = 1	/* bck sync length = PCM_INTF_CON[9:13] */
};

enum Soc_Aud_BT_MODE {
	Soc_Aud_BT_MODE_DUAL_MIC_ON_TX = 0,
	Soc_Aud_BT_MODE_SINGLE_MIC_ON_TX = 1
};

enum Soc_Aud_BYPASS_SRC {
	Soc_Aud_BYPASS_SRC_SLAVE_USE_ASRC = 0,	/* slave mode & external modem uses different crystal */
	Soc_Aud_BYPASS_SRC_SLAVE_USE_ASYNC_FIFO = 1	/* slave mode & external modem uses the same crystal */
};

enum Soc_Aud_PCM_CLOCK_SOURCE {
	Soc_Aud_PCM_CLOCK_SOURCE_MASTER_MODE = 0,
	Soc_Aud_PCM_CLOCK_SOURCE_SALVE_MODE = 1
};

enum Soc_Aud_PCM_WLEN_LEN {
	Soc_Aud_PCM_WLEN_LEN_PCM_16BIT = 0,
	Soc_Aud_PCM_WLEN_LEN_PCM_32BIT = 1
};

enum Soc_Aud_PCM_MODE {
	Soc_Aud_PCM_MODE_PCM_MODE_8K = 0,
	Soc_Aud_PCM_MODE_PCM_MODE_16K = 1
};

enum Soc_Aud_PCM_FMT {
	Soc_Aud_PCM_FMT_PCM_I2S = 0,
	Soc_Aud_PCM_FMT_PCM_EIAJ = 1,
	Soc_Aud_PCM_FMT_PCM_MODE_A = 2,
	Soc_Aud_PCM_FMT_PCM_MODE_B = 3
};

struct AudioDigitalPCM {
	uint32_t mTxLchRepeatSel;
	uint32_t mVbt16kModeSel;
	uint32_t mExtModemSel;
	uint8_t mExtendBckSyncLength;
	uint32_t mExtendBckSyncTypeSel;
	uint32_t mSingelMicSel;
	uint32_t mAsyncFifoSel;
	uint32_t mSlaveModeSel;
	uint32_t mPcmWordLength;
	uint32_t mPcmModeWidebandSel;
	uint32_t mPcmFormat;
	uint8_t mModemPcmOn;
};

enum Soc_Aud_BT_DAI_INPUT {
	Soc_Aud_BT_DAI_INPUT_FROM_BT,
	Soc_Aud_BT_DAI_INPUT_FROM_MGRIF
};

enum Soc_Aud_DATBT_MODE {
	Soc_Aud_DATBT_MODE_Mode8K,
	Soc_Aud_DATBT_MODE_Mode16K
};

enum Soc_Aud_DAI_DEL {
	Soc_Aud_DAI_DEL_HighWord,
	Soc_Aud_DAI_DEL_LowWord
};

enum Soc_Aud_BTSYNC {
	Soc_Aud_BTSYNC_Short_Sync,
	Soc_Aud_BTSYNC_Long_Sync
};

struct AudioDigitalDAIBT {
	bool mUSE_MRGIF_INPUT;
	bool mDAI_BT_MODE;
	bool mDAI_DEL;
	int mBT_LEN;
	bool mDATA_RDY;
	bool mBT_SYNC;
	bool mBT_ON;
	bool mDAIBT_ON;
};

enum Soc_Aud_MRFIF_I2S_SAMPLERATE {
	Soc_Aud_MRFIF_I2S_SAMPLERATE_MRFIF_I2S_8K = 0,
	Soc_Aud_MRFIF_I2S_SAMPLERATE_MRFIF_I2S_11K = 1,
	Soc_Aud_MRFIF_I2S_SAMPLERATE_MRFIF_I2S_12K = 2,
	Soc_Aud_MRFIF_I2S_SAMPLERATE_MRFIF_I2S_16K = 4,
	Soc_Aud_MRFIF_I2S_SAMPLERATE_MRFIF_I2S_22K = 5,
	Soc_Aud_MRFIF_I2S_SAMPLERATE_MRFIF_I2S_24K = 6,
	Soc_Aud_MRFIF_I2S_SAMPLERATE_MRFIF_I2S_32K = 8,
	Soc_Aud_MRFIF_I2S_SAMPLERATE_MRFIF_I2S_44K = 9,
	Soc_Aud_MRFIF_I2S_SAMPLERATE_MRFIF_I2S_48K = 10
};

struct AudioMrgIf {
	bool Mergeif_I2S_Enable;
	bool Merge_cnt_Clear;
	int Mrg_I2S_SampleRate;
	int Mrg_Sync_Dly;
	int Mrg_Clk_Edge_Dly;
	int Mrg_Clk_Dly;
	bool MrgIf_En;
};

/* class for irq mode and counter. */
struct AudioIrqMcuMode {
	unsigned int mStatus;	/* on,off */
	unsigned int mIrqMcuCounter;
	unsigned int mSampleRate;
};

struct AudioMemIFAttribute {
	int mFormat;
	int mDirection;
	unsigned int mSampleRate;
	unsigned int mChannels;
	unsigned int mBufferSize;
	unsigned int mInterruptSample;
	unsigned int mMemoryInterFaceType;
	unsigned int mClockInverse;
	unsigned int mMonoSel;
	unsigned int mdupwrite;
	unsigned int mState;
	void *privatedata;
};

struct Register_Control {
	unsigned int offset;
	unsigned int value;
	unsigned int mask;
};

struct SPH_Control {
	int bSpeechFlag;
	int bBgsFlag;
	int bRecordFlag;
	int bTtyFlag;
	int bVT;
	int bAudioPlay;
};

struct Hdmi_Clock_Setting {
	int SAMPLE_RATE;
	int CLK_APLL_SEL;
	int HDMI_BCK_DIV;
};

enum SPEAKER_CHANNEL {
	Channel_None = 0,
	Channel_Right,
	Channel_Left,
	Channel_Stereo
};

enum SOUND_PATH {
	DEFAULT_PATH = 0,
	IN1_PATH,
	IN2_PATH,
	IN3_PATH,
	IN1_IN2_MIX,
};

enum MIC_ANALOG_SWICTH {
	MIC_ANA_DEFAULT_PATH = 0,
	MIC_ANA_SWITCH1_HIGH
};
enum PolicyParameters {
	POLICY_LOAD_VOLUME = 0,
	POLICY_SET_FM_SPEAKER,
	POLICY_CHECK_FM_PRIMARY_KEY_ROUTING,
	POLICY_SET_FM_PRESTOP,
};

enum modem_index_t {
	MODEM_1 = 0,
	MODEM_2 = 1,
	MODEM_EXTERNAL = 2,
	NUM_MODEM
};

enum {
	AP_LOOPBACK_NONE = 0,
	AP_LOOPBACK_AMIC_TO_SPK,
	AP_LOOPBACK_AMIC_TO_HP,
	AP_LOOPBACK_DMIC_TO_SPK,
	AP_LOOPBACK_DMIC_TO_HP,
	AP_LOOPBACK_HEADSET_MIC_TO_SPK,
	AP_LOOPBACK_HEADSET_MIC_TO_HP,
};

#endif
