#include "hdmi_drv.h"
#include "mhl_ctrl.h"
#include "mhl_edid.h"
#include "mhl_hdcp.h"
#include "mhl_table.h"
#include "mhl_cbus.h"
#include "mhl_cbus_ctrl.h"
#include "mhl_avd.h"
#include "mhl_dbg.h"
#include "mt8135_mhl_reg.h"
#include "mt6397_cbus_reg.h"


HDMI_AV_INFO_T _stAvdAVInfo = { 0 };

static u8 _bAudInfoFm[5];
static u8 _bAviInfoFm[5];
static u8 _bSpdInf[25] = { 0 };

static u8 _bAcpType;
static u8 _bAcpData[16] = { 0 };
static u8 _bIsrc1Data[16] = { 0 };

static u32 _u4NValue;

static const char *szHdmiResStr[HDMI_VIDEO_RESOLUTION_NUM] = {
	"RES_480P",
	"RES_576P",
	"RES_720P60HZ",
	"RES_720P50HZ",
	"RES_1080I60HZ",
	"RES_1080I50HZ",
	"RES_1080P30HZ",
	"RES_1080P25HZ",
	"RES_1080P24HZ",
	"RES_1080P23_976HZ",
	"RES_1080P29_97HZ",
	"RES_1080P60HZ",
	"RES_1080P50HZ",
	"RES_720P60HZ_3D",
	"RES_720P50HZ_3D",
	"RES_1080P24HZ_3D",
	"RES_1080P23_976HZ_3D"
};

static const char *cHdmiAudFsStr[7] = {
	"HDMI_FS_32K",
	"HDMI_FS_44K",
	"HDMI_FS_48K",
	"HDMI_FS_88K",
	"HDMI_FS_96K",
	"HDMI_FS_176K",
	"HDMI_FS_192K"
};


static const char *cAudCodingTypeStr[16] = {
	"Refer to Stream Header",
	"PCM",
	"AC3",
	"MPEG1",
	"MP3",
	"MPEG2",
	"AAC",
	"DTS",
	"ATRAC",
	"ONE Bit Audio",
	"Dolby Digital+",
	"DTS-HD",
	"MAT(MLP)",
	"DST",
	"WMA Pro",
	"Reserved",
};

static const char *cAudChCountStr[8] = {
	"Refer to Stream Header",
	"2ch",
	"3ch",
	"4ch",
	"5ch",
	"6ch",
	"7ch",
	"8ch",

};

static const char *cAudFsStr[8] = {
	"Refer to Stream Header",
	"32 khz",
	"44.1 khz",
	"48 khz",
	"88.2 khz",
	"96 khz",
	"176.4 khz",
	"192 khz"
};


static const char *cAudChMapStr[32] = {
	"FR,FL",
	"LFE,FR,FL",
	"FC,FR,FL",
	"FC,LFE,FR,FL",
	"RC,FR,FL",
	"RC,LFE,FR,FL",
	"RC,FC,FR,FL",
	"RC,FC,LFE,FR,FL",
	"RR,RL,FR,FL",
	"RR,RL,LFE,FR,FL",
	"RR,RL,FC,FR,FL",
	"RR,RL,FC,LFE,FR,FL",
	"RC,RR,RL,FR,FL",
	"RC,RR,RL,LFE,FR,FL",
	"RC,RR,RL,FC,FR,FL",
	"RC,RR,RL,FC,LFE,FR,FL",
	"RRC,RLC,RR,RL,FR,FL",
	"RRC,RLC,RR,RL,LFE,FR,FL",
	"RRC,RLC,RR,RL,FC,FR,FL",
	"RRC,RLC,RR,RL,FC,LFE,FR,FL",
	"FRC,FLC,FR,FL",
	"FRC,FLC,LFE,FR,FL",
	"FRC,FLC,FC,FR,FL",
	"FRC,FLC,FC,LFE,FR,FL",
	"FRC,FLC,RC,FR,FL",
	"FRC,FLC,RC,LFE,FR,FL",
	"FRC,FLC,RC,FC,FR,FL",
	"FRC,FLC,RC,FC,LFE,FR,FL",
	"FRC,FLC,RR,RL,FR,FL",
	"FRC,FLC,RR,RL,LFE,FR,FL",
	"FRC,FLC,RR,RL,FC,FR,FL",
	"FRC,FLC,RR,RL,FC,LFE,FR,FL",
};

static const char *cAudDMINHStr[2] = {
	"Permiited down mixed stereo or no information",
	"Prohibited down mixed stereo"
};

static const char *cAudSampleSizeStr[4] = {
	"Refer to Stream Header",
	"16 bit",
	"20 bit",
	"24 bit"
};

static const char *cAviRgbYcbcrStr[4] = {
	"RGB",
	"YCbCr 4:2:2",
	"YCbCr 4:4:4",
	"Future"
};

static const char *cAviActivePresentStr[2] = {
	"No data",
	"Actuve Format(R0..R3) Valid",

};

static const char *cAviBarStr[4] = {
	"Bar data not valid",
	"Vert. Bar info valid",
	"Horiz. Bar info valid",
	"Vert. and Horiz Bar info valid",
};

static const char *cAviScanStr[4] = {
	"No data",
	"Overscanned display",
	"underscanned display",
	"Future",
};

static const char *cAviColorimetryStr[4] = {
	"no data",
	"ITU601",
	"ITU709",
	"Extended Colorimetry infor valid",
};

static const char *cAviAspectStr[4] = {
	"No data",
	"4:3",
	"16:9",
	"Future",
};


static const char *cAviActiveStr[16] = {
	"reserved",
	"reserved",
	"box 16:9(top)",
	"box 14:9(top)",
	"box > 16:9(center)",
	"reserved",
	"reserved",
	"reserved",
	"Same as picture aspect ratio",
	"4:3(Center)",
	"16:9(Center)",
	"14:9(Center)",
	"reserved",
	"4:3(with shoot & protect 14:9 center)",
	"16:9(with shoot & protect 14:9 center)",
	"16:3(with shoot & protect 4:3 center)"
};

static const char *cAviItContentStr[2] = {
	"no data",
	"IT Content"
};

static const char *cAviExtColorimetryStr[2] = {
	"xvYCC601",
	"xvYCC709",
};

static const char *cAviRGBRangeStr[4] = {
	"depends on video format",
	"Limit range",
	"FULL range",
	"Reserved",
};

static const char *cAviScaleStr[4] = {
	"Unkown non-uniform scaling",
	"Picture has been scaled horizontally",
	"Picture has been scaled vertically",
	"Picture has been scaled horizontally and vertically",
};



static const char *cSPDDeviceStr[16] = {
	"unknown",
	"Digital STB",
	"DVD Player",
	"D-VHS",
	"HDD Videorecorder",
	"DVC",
	"DSC",
	"Video CD",
	"Game",
	"PC General",
	"Blu-Ray Disc",
	"Super Audio CD",
	"reserved",
	"reserved",
	"reserved",
	"reserved",
};

/* ///////////////////////////////////////////////////// */

u8 fgIsAcpEnable(void)
{
	if (bReadByteHdmiGRL(GRL_ACP_ISRC_CTRL) & ACP_EN)
		return TRUE;
	else
		return FALSE;
}

u8 fgIsVSEnable(void)
{
	if (bReadByteHdmiGRL(GRL_ACP_ISRC_CTRL) & VS_EN)
		return TRUE;
	else
		return FALSE;
}

u8 fgIsISRC1Enable(void)
{
	if (bReadByteHdmiGRL(GRL_ACP_ISRC_CTRL) & ISRC1_EN)
		return TRUE;
	else
		return FALSE;
}


u8 fgIsISRC2Enable(void)
{
	if (bReadByteHdmiGRL(GRL_ACP_ISRC_CTRL) & ISRC2_EN)
		return TRUE;
	else
		return FALSE;
}

u8 bCheckStatus(void)
{
	u8 bStatus = 0;
	/* MHL_PLUG_FUNC(); */
	bStatus = bReadByteHdmiGRL(GRL_STATUS);

	if (bStatus & STATUS_PORD) {
		return TRUE;
	} else {
		return FALSE;
	}
}

u8 bCheckPordHotPlug(void)
{

	return bCheckStatus();

}

void MuteHDMIAudio(void)
{
	u8 bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
	bData |= AUDIO_ZERO;
	vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
}

void vBlackHDMIOnly(void)
{
	MHL_DRV_FUNC();

}

void vHDMIAVMute(void)
{
	MHL_AUDIO_FUNC();

	vBlackHDMIOnly();
	MuteHDMIAudio();
}

void vSetCTL0BeZero(u8 fgBeZero)
{
	u8 bTemp;

	MHL_VIDEO_FUNC();

	if (fgBeZero == TRUE) {
		bTemp = bReadByteHdmiGRL(GRL_CFG1);
		bTemp |= (1 << 4);
		vWriteByteHdmiGRL(GRL_CFG1, bTemp);
	} else {
		bTemp = bReadByteHdmiGRL(GRL_CFG1);
		bTemp &= ~(1 << 4);
		vWriteByteHdmiGRL(GRL_CFG1, bTemp);
	}

}

void vUnBlackHDMIOnly(void)
{
	MHL_DRV_FUNC();

}

void UnMuteHDMIAudio(void)
{
	u8 bData;
	MHL_AUDIO_FUNC();

	bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
	bData &= ~AUDIO_ZERO;
	vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
}


void vMhlAnalogInit(void)
{
	vWriteHdmiANA(HDMI_CON0, 0x0000000A);
	vWriteHdmiANA(HDMI_CON1, 0x002C0000);
	vWriteHdmiANA(HDMI_CON2, 0x0007C000);
	vWriteHdmiANA(HDMI_CON3, 0x00000000);
	vWriteHdmiANA(HDMI_CON4, 0x00000000);
	vWriteHdmiANA(HDMI_CON5, 0x00000000);
	vWriteHdmiANA(HDMI_CON6, 0x09040000);
	vWriteHdmiANA(HDMI_CON7, 0x8080000F);
	vWriteHdmiANA(HDMI_CON8, 0x00000000);


	vWriteHdmiANAMsk(HDMI_CON1, (0x0C << RG_HDMITX_DRV_IMP), RG_HDMITX_DRV_IMP_MASK);
	vWriteHdmiANAMsk(HDMI_CON0, (0x08 << RG_HDMITX_EN_IMP), RG_HDMITX_EN_IMP_MASK);
	vWriteHdmiANAMsk(HDMI_CON0, (0x0E << RG_HDMITX_DRV_IBIAS), RG_HDMITX_DRV_IBIAS_MASK);
}

void vMhlAnalogPD(void)
{
	vWriteHdmiANAMsk(HDMI_CON2, 0, RG_HDMITX_EN_MBIAS | RG_HDMITX_EN_TX_CKLDO);
	vWriteHdmiANAMsk(HDMI_CON6, 0, RG_HTPLL_EN);
	vWriteHdmiANAMsk(HDMI_CON0, 0, RG_HDMITX_EN_SLDO_MASK);

	vWriteHdmiANAMsk(HDMI_CON2, 0, RG_HDMITX_MBIAS_LPF_EN | RG_HDMITX_EN_TX_POSDIV);
	vWriteHdmiANAMsk(HDMI_CON0, 0,
			 RG_HDMITX_EN_SER_MASK | RG_HDMITX_EN_PRED_MASK | RG_HDMITX_EN_DRV_MASK);

}

void vMhlTriggerPLL(void)
{
	vWriteHdmiANAMsk(HDMI_CON7, RG_HTPLL_AUTOK_EN, RG_HTPLL_AUTOK_EN);
	vWriteHdmiANAMsk(HDMI_CON6, RG_HTPLL_RLH_EN, RG_HTPLL_RLH_EN);
	vWriteHdmiANAMsk(HDMI_CON6, (0 << RG_HTPLL_POSDIV), RG_HTPLL_POSDIV_MASK);

	vMhlAnalogPD();

	/* step1 */
	vWriteHdmiANAMsk(HDMI_CON2, RG_HDMITX_EN_MBIAS, RG_HDMITX_EN_MBIAS);
	udelay(5);
	/* step2 */
	vWriteHdmiANAMsk(HDMI_CON2, RG_HDMITX_EN_TX_CKLDO, RG_HDMITX_EN_TX_CKLDO);
	vWriteHdmiANAMsk(HDMI_CON6, RG_HTPLL_EN, RG_HTPLL_EN);
	vWriteHdmiANAMsk(HDMI_CON0, RG_HDMITX_EN_SLDO_MASK, RG_HDMITX_EN_SLDO_MASK);
	udelay(100);
	/* step3 */
	vWriteHdmiANAMsk(HDMI_CON2, RG_HDMITX_MBIAS_LPF_EN | RG_HDMITX_EN_TX_POSDIV,
			 RG_HDMITX_MBIAS_LPF_EN | RG_HDMITX_EN_TX_POSDIV);
	vWriteHdmiANAMsk(HDMI_CON0, (0x08 << RG_HDMITX_EN_SER) | (0x08 << RG_HDMITX_EN_PRED),
			 RG_HDMITX_EN_SER_MASK | RG_HDMITX_EN_PRED_MASK);
}

void vMhlSignalOff(bool fgEn)
{
	if (fgEn)
		vWriteHdmiANAMsk(HDMI_CON0, 0, RG_HDMITX_EN_DRV_MASK);
	else
		vWriteHdmiANAMsk(HDMI_CON0, (0x08 << RG_HDMITX_EN_DRV), RG_HDMITX_EN_DRV_MASK);
}

void vMhlSetPLL(MHL_RES_PLL res)
{
	vMhlAnalogPD();

	vWriteHdmiANAMsk(MHL_TVDPLL_PWR, RG_TVDPLL_PWR_ON, RG_TVDPLL_PWR_ON);
	udelay(5);

	switch (res) {
	case MHL_PLL_27:
		/* exel pll cal */
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, 0, RG_TVDPLL_EN);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, (0x04 << RG_TVDPLL_POSDIV),
				 RG_TVDPLL_POSDIV_MASK);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON1, (1115039586 << RG_TVDPLL_SDM_PCW),
				 RG_TVDPLL_SDM_PCW_MASK);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, RG_TVDPLL_EN, RG_TVDPLL_EN);
		udelay(20);
		vWriteHdmiANAMsk(HDMI_CON6, (3 << RG_HTPLL_PREDIV), RG_HTPLL_PREDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON2, (2 << RG_HDMITX_TX_POSDIV), RG_HDMITX_TX_POSDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (1 << RG_HTPLL_FBKSEL), RG_HTPLL_FBKSEL_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (29 << RG_HTPLL_FBKDIV), RG_HTPLL_FBKDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (0 << RG_HTPLL_POSDIV), RG_HTPLL_POSDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON2, 0, RG_HDMITX_TX_POSDIV_SEL);
		vWriteHdmiANAMsk(HDMI_CON6, (1 << RG_HTPLL_IC), RG_HTPLL_IC_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (1 << RG_HTPLL_IR), RG_HTPLL_IR_MASK);
		vWriteHdmiANAMsk(HDMI_CON7, (1 << RG_HTPLL_DIVEN), RG_HTPLL_DIVEN_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (14 << RG_HTPLL_BP), RG_HTPLL_BP_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (3 << RG_HTPLL_BC), RG_HTPLL_BC_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (1 << RG_HTPLL_BR), RG_HTPLL_BR_MASK);
		/*  */
		vWriteHdmiANAMsk(HDMI_CON1, 0, RG_HDMITX_PRED_IMP);
		vWriteHdmiANAMsk(HDMI_CON1, (3 << RG_HDMITX_PRED_IBIAS), RG_HDMITX_PRED_IBIAS_MASK);
		/* top clk */
		break;
	case MHL_PLL_74175:
		/* exel pll cal */
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, 0, RG_TVDPLL_EN);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, (0x04 << RG_TVDPLL_POSDIV),
				 RG_TVDPLL_POSDIV_MASK);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON1, (1531630765 << RG_TVDPLL_SDM_PCW),
				 RG_TVDPLL_SDM_PCW_MASK);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, RG_TVDPLL_EN, RG_TVDPLL_EN);
		udelay(20);
		vWriteHdmiANAMsk(HDMI_CON6, (3 << RG_HTPLL_PREDIV), RG_HTPLL_PREDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON2, (0 << RG_HDMITX_TX_POSDIV), RG_HDMITX_TX_POSDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (1 << RG_HTPLL_FBKSEL), RG_HTPLL_FBKSEL_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (14 << RG_HTPLL_FBKDIV), RG_HTPLL_FBKDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (0 << RG_HTPLL_POSDIV), RG_HTPLL_POSDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON2, 0, RG_HDMITX_TX_POSDIV_SEL);
		vWriteHdmiANAMsk(HDMI_CON6, (1 << RG_HTPLL_IC), RG_HTPLL_IC_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (1 << RG_HTPLL_IR), RG_HTPLL_IR_MASK);
		vWriteHdmiANAMsk(HDMI_CON7, (2 << RG_HTPLL_DIVEN), RG_HTPLL_DIVEN_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (15 << RG_HTPLL_BP), RG_HTPLL_BP_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (3 << RG_HTPLL_BC), RG_HTPLL_BC_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (2 << RG_HTPLL_BR), RG_HTPLL_BR_MASK);
		/*  */
		vWriteHdmiANAMsk(HDMI_CON1, 1, RG_HDMITX_PRED_IMP);
		vWriteHdmiANAMsk(HDMI_CON1, (6 << RG_HDMITX_PRED_IBIAS), RG_HDMITX_PRED_IBIAS_MASK);
		/* top clk */
		break;
	case MHL_PLL_7425:
		/* exel pll cal */
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, 0, RG_TVDPLL_EN);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, (0x04 << RG_TVDPLL_POSDIV),
				 RG_TVDPLL_POSDIV_MASK);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON1, (1533179431 << RG_TVDPLL_SDM_PCW),
				 RG_TVDPLL_SDM_PCW_MASK);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, RG_TVDPLL_EN, RG_TVDPLL_EN);
		udelay(20);
		vWriteHdmiANAMsk(HDMI_CON6, (3 << RG_HTPLL_PREDIV), RG_HTPLL_PREDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON2, (0 << RG_HDMITX_TX_POSDIV), RG_HDMITX_TX_POSDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (1 << RG_HTPLL_FBKSEL), RG_HTPLL_FBKSEL_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (14 << RG_HTPLL_FBKDIV), RG_HTPLL_FBKDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (0 << RG_HTPLL_POSDIV), RG_HTPLL_POSDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON2, 0, RG_HDMITX_TX_POSDIV_SEL);
		vWriteHdmiANAMsk(HDMI_CON6, (1 << RG_HTPLL_IC), RG_HTPLL_IC_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (1 << RG_HTPLL_IR), RG_HTPLL_IR_MASK);
		vWriteHdmiANAMsk(HDMI_CON7, (2 << RG_HTPLL_DIVEN), RG_HTPLL_DIVEN_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (15 << RG_HTPLL_BP), RG_HTPLL_BP_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (3 << RG_HTPLL_BC), RG_HTPLL_BC_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (2 << RG_HTPLL_BR), RG_HTPLL_BR_MASK);
		/*  */
		vWriteHdmiANAMsk(HDMI_CON1, 1, RG_HDMITX_PRED_IMP);
		vWriteHdmiANAMsk(HDMI_CON1, (6 << RG_HDMITX_PRED_IBIAS), RG_HDMITX_PRED_IBIAS_MASK);
		/* top clk */
		break;
	case MHL_PLL_74175PP:
		/* exel pll cal */
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, 0, RG_TVDPLL_EN);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, (0x04 << RG_TVDPLL_POSDIV),
				 RG_TVDPLL_POSDIV_MASK);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON1, (1531630765 << RG_TVDPLL_SDM_PCW),
				 RG_TVDPLL_SDM_PCW_MASK);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, RG_TVDPLL_EN, RG_TVDPLL_EN);
		udelay(20);
		vWriteHdmiANAMsk(HDMI_CON6, (3 << RG_HTPLL_PREDIV), RG_HTPLL_PREDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON2, (0 << RG_HDMITX_TX_POSDIV), RG_HDMITX_TX_POSDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (1 << RG_HTPLL_FBKSEL), RG_HTPLL_FBKSEL_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (19 << RG_HTPLL_FBKDIV), RG_HTPLL_FBKDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (0 << RG_HTPLL_POSDIV), RG_HTPLL_POSDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON2, RG_HDMITX_TX_POSDIV_SEL, RG_HDMITX_TX_POSDIV_SEL);
		vWriteHdmiANAMsk(HDMI_CON6, (1 << RG_HTPLL_IC), RG_HTPLL_IC_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (1 << RG_HTPLL_IR), RG_HTPLL_IR_MASK);
		vWriteHdmiANAMsk(HDMI_CON7, (2 << RG_HTPLL_DIVEN), RG_HTPLL_DIVEN_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (12 << RG_HTPLL_BP), RG_HTPLL_BP_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (2 << RG_HTPLL_BC), RG_HTPLL_BC_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (1 << RG_HTPLL_BR), RG_HTPLL_BR_MASK);
		/*  */
		vWriteHdmiANAMsk(HDMI_CON1, 1, RG_HDMITX_PRED_IMP);
		vWriteHdmiANAMsk(HDMI_CON1, (6 << RG_HDMITX_PRED_IBIAS), RG_HDMITX_PRED_IBIAS_MASK);
		break;
	case MHL_PLL_7425PP:
		/* exel pll cal */
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, 0, RG_TVDPLL_EN);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, (0x04 << RG_TVDPLL_POSDIV),
				 RG_TVDPLL_POSDIV_MASK);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON1, (1533179431 << RG_TVDPLL_SDM_PCW),
				 RG_TVDPLL_SDM_PCW_MASK);
		vWriteHdmiANAMsk(MHL_TVDPLL_CON0, RG_TVDPLL_EN, RG_TVDPLL_EN);
		udelay(20);
		vWriteHdmiANAMsk(HDMI_CON6, (3 << RG_HTPLL_PREDIV), RG_HTPLL_PREDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON2, (0 << RG_HDMITX_TX_POSDIV), RG_HDMITX_TX_POSDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (1 << RG_HTPLL_FBKSEL), RG_HTPLL_FBKSEL_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (19 << RG_HTPLL_FBKDIV), RG_HTPLL_FBKDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (0 << RG_HTPLL_POSDIV), RG_HTPLL_POSDIV_MASK);
		vWriteHdmiANAMsk(HDMI_CON2, RG_HDMITX_TX_POSDIV_SEL, RG_HDMITX_TX_POSDIV_SEL);
		vWriteHdmiANAMsk(HDMI_CON6, (1 << RG_HTPLL_IC), RG_HTPLL_IC_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (1 << RG_HTPLL_IR), RG_HTPLL_IR_MASK);
		vWriteHdmiANAMsk(HDMI_CON7, (2 << RG_HTPLL_DIVEN), RG_HTPLL_DIVEN_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (12 << RG_HTPLL_BP), RG_HTPLL_BP_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (2 << RG_HTPLL_BC), RG_HTPLL_BC_MASK);
		vWriteHdmiANAMsk(HDMI_CON6, (1 << RG_HTPLL_BR), RG_HTPLL_BR_MASK);
		/*  */
		vWriteHdmiANAMsk(HDMI_CON1, 1, RG_HDMITX_PRED_IMP);
		vWriteHdmiANAMsk(HDMI_CON1, (6 << RG_HDMITX_PRED_IBIAS), RG_HDMITX_PRED_IBIAS_MASK);
		/* top clk */
		break;
	default:
		break;
	}

	vMhlTriggerPLL();
}

void vMhlSetDigital(MHL_RES_PLL res)
{
	MHL_ANA_CKGEN_PORT = (MHL_ANA_CKGEN_PORT & (~(0x03 << 0)));
	MHL_ANA_CKGEN_PORT = (MHL_ANA_CKGEN_PORT & (~(0x03 << 24)));

	vWriteHdmiSYSMsk(DISP_CG_CON1, 0, (((1 << 7)) | (1 << 12) | (1 << 13)));

	vWriteHdmiSYSMsk(HDMI_SYS_CFG1, 0, MHL_MODE_ON);
	vWriteHdmiSYSMsk(HDMI_SYS_CFG1, 0, HDMI_OUT_FIFO_EN);
	udelay(100);
	vWriteHdmiSYSMsk(HDMI_SYS_CFG1, MHL_MODE_ON, MHL_MODE_ON);
	vWriteHdmiSYSMsk(HDMI_SYS_CFG1, HDMI_OUT_FIFO_EN, HDMI_OUT_FIFO_EN);
	vWriteHdmiGRLMsk(GRL_CFG4, 0, CFG4_MHL_MODE);

	switch (res) {
	case MHL_PLL_27:
		/* top clk */
		vWriteHdmiSYSMsk(HDMI_SYS_CFG1, 0, MHL_PP_MODE);
		MHL_ANA_CKGEN_PORT = (MHL_ANA_CKGEN_PORT & (~(0x03 << 0))) | 0x03;
		MHL_ANA_CKGEN_PORT = (MHL_ANA_CKGEN_PORT & (~(0x03 << 24))) | (0x03 << 24);
		break;
	case MHL_PLL_74175:
		/* top clk */
		vWriteHdmiSYSMsk(HDMI_SYS_CFG1, 0, MHL_PP_MODE);
		MHL_ANA_CKGEN_PORT = (MHL_ANA_CKGEN_PORT & (~(0x03 << 0))) | 0x02;
		MHL_ANA_CKGEN_PORT = (MHL_ANA_CKGEN_PORT & (~(0x03 << 24))) | (0x03 << 24);
		break;
	case MHL_PLL_7425:
		/* top clk */
		vWriteHdmiSYSMsk(HDMI_SYS_CFG1, 0, MHL_PP_MODE);
		MHL_ANA_CKGEN_PORT = (MHL_ANA_CKGEN_PORT & (~(0x03 << 0))) | 0x02;
		MHL_ANA_CKGEN_PORT = (MHL_ANA_CKGEN_PORT & (~(0x03 << 24))) | (0x03 << 24);
		break;
	case MHL_PLL_74175PP:
		vWriteHdmiSYSMsk(HDMI_SYS_CFG1, MHL_PP_MODE, MHL_PP_MODE);
		MHL_ANA_CKGEN_PORT = (MHL_ANA_CKGEN_PORT & (~(0x03 << 0))) | 0x01;
		MHL_ANA_CKGEN_PORT = (MHL_ANA_CKGEN_PORT & (~(0x03 << 24))) | (0x02 << 24);
		vWriteHdmiGRLMsk(GRL_CFG4, CFG4_MHL_MODE, CFG4_MHL_MODE);
		break;
	case MHL_PLL_7425PP:
		vWriteHdmiSYSMsk(HDMI_SYS_CFG1, MHL_PP_MODE, MHL_PP_MODE);
		MHL_ANA_CKGEN_PORT = (MHL_ANA_CKGEN_PORT & (~(0x03 << 0))) | 0x01;
		MHL_ANA_CKGEN_PORT = (MHL_ANA_CKGEN_PORT & (~(0x03 << 24))) | (0x02 << 24);
		vWriteHdmiGRLMsk(GRL_CFG4, CFG4_MHL_MODE, CFG4_MHL_MODE);
		break;
	default:
		break;
	}

}

void vTmdsOnOffAndResetHdcp(u8 fgHdmiTmdsEnable)
{
	MHL_DRV_FUNC();

	if (fgHdmiTmdsEnable == 1) {
		vMhlSignalOff(0);
	} else {
		vHDMIAVMute();
		mdelay(1);
		vHDCPReset();
		vMhlSignalOff(1);
		mdelay(100);
	}
}

void vVideoPLLInit(void)
{
	MHL_DRV_FUNC();
	/* init analog part */
	vMhlAnalogInit();
}

void vSetHDMITxPLL(u8 bResIndex)
{
	MHL_PLL_FUNC();

	switch (bResIndex) {
	case HDMI_VIDEO_720x480p_60Hz:
	case HDMI_VIDEO_720x576p_50Hz:
		vMhlSetPLL(MHL_PLL_27);
		break;
	case HDMI_VIDEO_1280x720p_60Hz:
	case HDMI_VIDEO_1920x1080i_60Hz:
	case HDMI_VIDEO_1920x1080p_23Hz:
	case HDMI_VIDEO_1920x1080p_29Hz:
		vMhlSetPLL(MHL_PLL_74175);
		break;
	case HDMI_VIDEO_1280x720p_50Hz:
	case HDMI_VIDEO_1920x1080i_50Hz:
	case HDMI_VIDEO_1920x1080p_24Hz:
	case HDMI_VIDEO_1920x1080p_25Hz:
	case HDMI_VIDEO_1920x1080p_30Hz:
		vMhlSetPLL(MHL_PLL_7425);
		break;
	case HDMI_VIDEO_1920x1080p_60Hz:
	case HDMI_VIDEO_1280x720p_60Hz_3D:
	case HDMI_VIDEO_1920x1080p_23Hz_3D:
		vMhlSetPLL(MHL_PLL_74175PP);
		break;
	case HDMI_VIDEO_1920x1080p_50Hz:
	case HDMI_VIDEO_1280x720p_50Hz_3D:
	case HDMI_VIDEO_1920x1080p_24Hz_3D:
		vMhlSetPLL(MHL_PLL_7425PP);
		break;
	default:
		printk("[MHL]can not support resolution\n");
		break;

	}

}

#ifdef MHL_INTER_PATTERN_FOR_DBG
void vDPI1_480p(void)
{
	*(unsigned int *)0xf400f000 = 0x00000001;
	*(unsigned int *)0xf400f004 = 0x00000000;
	*(unsigned int *)0xf400f008 = 0x00000001;
	*(unsigned int *)0xf400f00c = 0x00000000;
	*(unsigned int *)0xf400f010 = 0x00000000;
	*(unsigned int *)0xf400f014 = 0x82000200;
	*(unsigned int *)0xf400f018 = 0x01e002d0;
	*(unsigned int *)0xf400f01c = 0x0000003e;
	*(unsigned int *)0xf400f020 = 0x0010003c;
	*(unsigned int *)0xf400f024 = 0x0000000c;
	*(unsigned int *)0xf400f028 = 0x0003001e;
	*(unsigned int *)0xf400f02c = 0x00000000;
	*(unsigned int *)0xf400f030 = 0x00000000;
	*(unsigned int *)0xf400f034 = 0x00000000;
	*(unsigned int *)0xf400f038 = 0x00000000;
	*(unsigned int *)0xf400f03c = 0x00000000;
	*(unsigned int *)0xf400f040 = 0x00000000;

	*(unsigned int *)0xf400f05c = 0x00000000;

	*(unsigned int *)0xf400f0b4 = 0x00000041;

	*(unsigned int *)0xf400f080 = 0x00000000;
	*(unsigned int *)0xf400f084 = 0x00000000;
	*(unsigned int *)0xf400f094 = 0x00000000;
}

void vDPI1_720p60(void)
{
	*(unsigned int *)0xf400f000 = 0x00000001;
	*(unsigned int *)0xf400f004 = 0x00000000;
	*(unsigned int *)0xf400f008 = 0x00000001;
	*(unsigned int *)0xf400f00c = 0x00000000;
	*(unsigned int *)0xf400f010 = 0x00000000;
	*(unsigned int *)0xf400f014 = 0x82000200;
	*(unsigned int *)0xf400f018 = 0x02d00500;
	*(unsigned int *)0xf400f01c = 0x00000028;
	*(unsigned int *)0xf400f020 = 0x006e00dc;
	*(unsigned int *)0xf400f024 = 0x00000005;
	*(unsigned int *)0xf400f028 = 0x00050014;
	*(unsigned int *)0xf400f02c = 0x00000000;
	*(unsigned int *)0xf400f030 = 0x00000000;
	*(unsigned int *)0xf400f034 = 0x00000000;
	*(unsigned int *)0xf400f038 = 0x00000000;
	*(unsigned int *)0xf400f03c = 0x00000000;
	*(unsigned int *)0xf400f040 = 0x00000000;
	*(unsigned int *)0xf400f05c = 0x00000003;
	*(unsigned int *)0xf400f0b4 = 0x00000041;

	*(unsigned int *)0xf400f080 = 0x00000000;
	*(unsigned int *)0xf400f084 = 0x00000000;
	*(unsigned int *)0xf400f094 = 0x00000000;
}

void vDPI1_1080i60(void)
{
	*(unsigned int *)0xf400f000 = 0x00000001;
	*(unsigned int *)0xf400f004 = 0x00000000;
	*(unsigned int *)0xf400f008 = 0x00000001;
	*(unsigned int *)0xf400f00c = 0x00000000;
	*(unsigned int *)0xf400f010 = 0x00000004;
	*(unsigned int *)0xf400f014 = 0x82000200;
	*(unsigned int *)0xf400f018 = 0x021c0780;
	*(unsigned int *)0xf400f01c = 0x0000002c;
	*(unsigned int *)0xf400f020 = 0x00580094;
	*(unsigned int *)0xf400f024 = 0x00000005;
	*(unsigned int *)0xf400f028 = 0x0002000f;
	*(unsigned int *)0xf400f02c = 0x00000005;
	*(unsigned int *)0xf400f030 = 0x0102010f;
	*(unsigned int *)0xf400f034 = 0x00000000;
	*(unsigned int *)0xf400f038 = 0x00000000;
	*(unsigned int *)0xf400f03c = 0x00000000;
	*(unsigned int *)0xf400f040 = 0x00000000;
	*(unsigned int *)0xf400f05c = 0x00000003;
	*(unsigned int *)0xf400f0b4 = 0x00000041;

	*(unsigned int *)0xf400f080 = 0x00000000;
	*(unsigned int *)0xf400f084 = 0x00000000;
	*(unsigned int *)0xf400f094 = 0x00000000;

}

void vDPI1_1080p60(void)
{
	*(unsigned int *)0xf400f000 = 0x00000001;
	*(unsigned int *)0xf400f004 = 0x00000000;
	*(unsigned int *)0xf400f008 = 0x00000001;
	*(unsigned int *)0xf400f00c = 0x00000000;
	*(unsigned int *)0xf400f010 = 0x000000a0;
	*(unsigned int *)0xf400f014 = 0x82000200;
	*(unsigned int *)0xf400f018 = 0x04380780;
	*(unsigned int *)0xf400f01c = 0x0000002c;
	*(unsigned int *)0xf400f020 = 0x00580094;
	*(unsigned int *)0xf400f024 = 0x00000005;
	*(unsigned int *)0xf400f028 = 0x00040024;
	*(unsigned int *)0xf400f02c = 0x00000000;
	*(unsigned int *)0xf400f030 = 0x00000000;
	*(unsigned int *)0xf400f034 = 0x00000000;
	*(unsigned int *)0xf400f038 = 0x00000000;
	*(unsigned int *)0xf400f03c = 0x00000000;
	*(unsigned int *)0xf400f040 = 0x00000000;
	*(unsigned int *)0xf400f05c = 0x00000003;

	*(unsigned int *)0xf400f064 = 0x1e751f8b;
	*(unsigned int *)0xf400f068 = 0x00da0200;
	*(unsigned int *)0xf400f06c = 0x004a02dc;
	*(unsigned int *)0xf400f070 = 0x1e2f0200;
	*(unsigned int *)0xf400f074 = 0x00001fd1;
	*(unsigned int *)0xf400f080 = 0x01000800;
	*(unsigned int *)0xf400f084 = 0x00000800;
	*(unsigned int *)0xf400f094 = 0x00000001;
	*(unsigned int *)0xf400f0a8 = 0x00000020;

	*(unsigned int *)0xf400f0b4 = 0x00000041;

}

void vDPI1_1080p24(void)
{
	*(unsigned int *)0xf400f000 = 0x00000001;
	*(unsigned int *)0xf400f004 = 0x00000000;
	*(unsigned int *)0xf400f008 = 0x00000001;
	*(unsigned int *)0xf400f00c = 0x00000000;
	*(unsigned int *)0xf400f010 = 0x00000000;
	*(unsigned int *)0xf400f014 = 0x82000200;
	*(unsigned int *)0xf400f018 = 0x04380780;
	*(unsigned int *)0xf400f01c = 0x0000002c;
	*(unsigned int *)0xf400f020 = 0x027e0094;
	*(unsigned int *)0xf400f024 = 0x00000005;
	*(unsigned int *)0xf400f028 = 0x00040024;
	*(unsigned int *)0xf400f02c = 0x00000000;
	*(unsigned int *)0xf400f030 = 0x00000000;
	*(unsigned int *)0xf400f034 = 0x00000000;
	*(unsigned int *)0xf400f038 = 0x00000000;
	*(unsigned int *)0xf400f03c = 0x00000000;
	*(unsigned int *)0xf400f040 = 0x00000000;
	*(unsigned int *)0xf400f05c = 0x00000003;
	*(unsigned int *)0xf400f0b4 = 0x00000041;

	*(unsigned int *)0xf400f080 = 0x00000000;
	*(unsigned int *)0xf400f084 = 0x00000000;
	*(unsigned int *)0xf400f094 = 0x00000000;
}

void vDPI1_720p60_3d(void)
{
	*(unsigned int *)0xf400f000 = 0x00000001;
	*(unsigned int *)0xf400f004 = 0x00000000;
	*(unsigned int *)0xf400f008 = 0x00000001;
	*(unsigned int *)0xf400f00c = 0x00000000;
	*(unsigned int *)0xf400f010 = 0x000000a8;
	*(unsigned int *)0xf400f014 = 0x82000200;
	*(unsigned int *)0xf400f018 = 0x02d00500;
	*(unsigned int *)0xf400f01c = 0x00000028;
	*(unsigned int *)0xf400f020 = 0x006e00dc;
	*(unsigned int *)0xf400f024 = 0x00000005;
	*(unsigned int *)0xf400f028 = 0x00050014;
	*(unsigned int *)0xf400f02c = 0x00000000;
	*(unsigned int *)0xf400f030 = 0x00000000;
	*(unsigned int *)0xf400f034 = 0x00000005;
	*(unsigned int *)0xf400f038 = 0x00050014;
	*(unsigned int *)0xf400f03c = 0x00000000;
	*(unsigned int *)0xf400f040 = 0x00000000;
	*(unsigned int *)0xf400f05c = 0x00000003;
	*(unsigned int *)0xf400f0b4 = 0x00000041;

	*(unsigned int *)0xf400f064 = 0x1e751f8b;
	*(unsigned int *)0xf400f068 = 0x00da0200;
	*(unsigned int *)0xf400f06c = 0x004a02dc;
	*(unsigned int *)0xf400f070 = 0x1e2f0200;
	*(unsigned int *)0xf400f074 = 0x00001fd1;
	*(unsigned int *)0xf400f080 = 0x01000800;
	*(unsigned int *)0xf400f084 = 0x00000800;
	*(unsigned int *)0xf400f094 = 0x00000001;
	*(unsigned int *)0xf400f0a8 = 0x00000020;

}

void vDPI1_1080p24_3d(void)
{
	*(unsigned int *)0xf400f000 = 0x00000001;
	*(unsigned int *)0xf400f004 = 0x00000000;
	*(unsigned int *)0xf400f008 = 0x00000001;
	*(unsigned int *)0xf400f00c = 0x00000000;
	*(unsigned int *)0xf400f010 = 0x000000a8;
	*(unsigned int *)0xf400f014 = 0x82000200;
	*(unsigned int *)0xf400f018 = 0x04380780;
	*(unsigned int *)0xf400f01c = 0x0000002c;
	*(unsigned int *)0xf400f020 = 0x027e0094;
	*(unsigned int *)0xf400f024 = 0x00000005;
	*(unsigned int *)0xf400f028 = 0x00040024;
	*(unsigned int *)0xf400f02c = 0x00000000;
	*(unsigned int *)0xf400f030 = 0x00000000;
	*(unsigned int *)0xf400f034 = 0x00000005;
	*(unsigned int *)0xf400f038 = 0x00040024;
	*(unsigned int *)0xf400f03c = 0x00000000;
	*(unsigned int *)0xf400f040 = 0x00000000;
	*(unsigned int *)0xf400f05c = 0x00000003;
	*(unsigned int *)0xf400f0b4 = 0x00000041;

	*(unsigned int *)0xf400f064 = 0x1e751f8b;
	*(unsigned int *)0xf400f068 = 0x00da0200;
	*(unsigned int *)0xf400f06c = 0x004a02dc;
	*(unsigned int *)0xf400f070 = 0x1e2f0200;
	*(unsigned int *)0xf400f074 = 0x00001fd1;
	*(unsigned int *)0xf400f080 = 0x01000800;
	*(unsigned int *)0xf400f084 = 0x00000800;
	*(unsigned int *)0xf400f094 = 0x00000001;
	*(unsigned int *)0xf400f0a8 = 0x00000020;

}
#endif
void vSetHDMITxDigital(u8 bResIndex)
{
	MHL_PLL_FUNC();

	switch (bResIndex) {
	case HDMI_VIDEO_720x480p_60Hz:
	case HDMI_VIDEO_720x576p_50Hz:
		vMhlSetDigital(MHL_PLL_27);
		break;
	case HDMI_VIDEO_1280x720p_60Hz:
	case HDMI_VIDEO_1920x1080i_60Hz:
	case HDMI_VIDEO_1920x1080p_23Hz:
	case HDMI_VIDEO_1920x1080p_29Hz:
		vMhlSetDigital(MHL_PLL_74175);
		break;
	case HDMI_VIDEO_1280x720p_50Hz:
	case HDMI_VIDEO_1920x1080i_50Hz:
	case HDMI_VIDEO_1920x1080p_24Hz:
	case HDMI_VIDEO_1920x1080p_25Hz:
	case HDMI_VIDEO_1920x1080p_30Hz:
		vMhlSetDigital(MHL_PLL_7425);
		break;
	case HDMI_VIDEO_1920x1080p_60Hz:
	case HDMI_VIDEO_1280x720p_60Hz_3D:
	case HDMI_VIDEO_1920x1080p_23Hz_3D:
		vMhlSetDigital(MHL_PLL_74175PP);
		break;
	case HDMI_VIDEO_1920x1080p_50Hz:
	case HDMI_VIDEO_1280x720p_50Hz_3D:
	case HDMI_VIDEO_1920x1080p_24Hz_3D:
		vMhlSetDigital(MHL_PLL_7425PP);
		break;
	default:
		printk("can not support resolution\n");
		break;
	}

#ifdef MHL_INTER_PATTERN_FOR_DBG
	/* vWriteByteHdmiGRL(GRL_ABIST_CTL1,0x80); */
	vWriteByteHdmiGRL(GRL_ABIST_CTL1, 0x00);
	switch (bResIndex) {
	case HDMI_VIDEO_720x480p_60Hz:
		vWriteByteHdmiGRL(GRL_ABIST_CTL0, ((0 << 7) | (0 << 6) | (0x02)));	/* 480p */
		vDPI1_480p();
		break;
	case HDMI_VIDEO_1280x720p_60Hz:
		vWriteByteHdmiGRL(GRL_ABIST_CTL0, ((1 << 7) | (1 << 6) | (0x03)));	/* 720p */
		vDPI1_720p60();
		break;
	case HDMI_VIDEO_1920x1080i_60Hz:
		vWriteByteHdmiGRL(GRL_ABIST_CTL0, ((1 << 7) | (1 << 6) | (0x04)));	/* 1080i */
		vDPI1_1080i60();
		break;
	case HDMI_VIDEO_1920x1080p_60Hz:
		vWriteByteHdmiGRL(GRL_ABIST_CTL0, ((1 << 7) | (1 << 6) | (0x0A)));	/* 1080p */
		vDPI1_1080p60();
		break;
	case HDMI_VIDEO_720x576p_50Hz:
		vWriteByteHdmiGRL(GRL_ABIST_CTL0, ((0 << 7) | (0 << 6) | (0x0b)));	/* 576p */
		break;
	case HDMI_VIDEO_1280x720p_50Hz:
		vWriteByteHdmiGRL(GRL_ABIST_CTL0, ((1 << 7) | (1 << 6) | (0x0c)));	/* 720p50 */
		break;
	case HDMI_VIDEO_1920x1080i_50Hz:
		vWriteByteHdmiGRL(GRL_ABIST_CTL0, ((1 << 7) | (1 << 6) | (0x0d)));	/* 1080i50 */
		break;
	case HDMI_VIDEO_1920x1080p_50Hz:
		vWriteByteHdmiGRL(GRL_ABIST_CTL0, ((1 << 7) | (1 << 6) | (0x13)));	/* 1080p50 */
		break;
	case HDMI_VIDEO_1920x1080p_24Hz:
		vWriteByteHdmiGRL(GRL_ABIST_CTL0, ((1 << 7) | (1 << 6) | (0x0A)));	/* 1080p */
		vDPI1_1080p24();
		break;
	case HDMI_VIDEO_1280x720p_60Hz_3D:
		vDPI1_720p60_3d();
		break;
	case HDMI_VIDEO_1920x1080p_24Hz_3D:
		vDPI1_1080p24_3d();
		break;
	default:
		printk("abist can not support resolution\n");
		break;
	}
#endif

}

void vResetHDMI(BYTE bRst)
{
	MHL_DRV_FUNC();
	if (bRst) {
		vWriteHdmiSYSMsk(HDMI_SYS_CFG0, HDMI_RST, HDMI_RST);
	} else {
		vWriteHdmiSYSMsk(HDMI_SYS_CFG0, 0, HDMI_RST);
		vWriteHdmiGRLMsk(GRL_CFG3, 0, CFG3_CONTROL_PACKET_DELAY);	/* Designer suggest adjust Control packet deliver time */
		vWriteHdmiSYSMsk(HDMI_SYS_CFG0, ANLG_ON, ANLG_ON);
	}
}

void vChangeVpll(u8 bRes)
{
	MHL_PLL_FUNC();
	vSetHDMITxPLL(bRes);	/* set PLL */
	vResetHDMI(1);
	vResetHDMI(0);
	vSetHDMITxDigital(bRes);
}



void vHDMIAVUnMute(void)
{
	MHL_AUDIO_FUNC();
	vUnBlackHDMIOnly();
	UnMuteHDMIAudio();
}

void vEnableNotice(u8 bOn)
{
	u8 bData;
	MHL_VIDEO_FUNC();
	if (bOn == TRUE) {
		bData = bReadByteHdmiGRL(GRL_CFG2);
		bData |= 0x40;	/* temp. solve 720p issue. to avoid audio packet jitter problem */
		vWriteByteHdmiGRL(GRL_CFG2, bData);
	} else {
		bData = bReadByteHdmiGRL(GRL_CFG2);
		bData &= ~0x40;
		vWriteByteHdmiGRL(GRL_CFG2, bData);
	}
}

/* xubo */
void vEnableNCTSAutoWrite(void)
{
	u8 bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_DIVN);
	bData |= NCTS_WRI_ANYTIME;	/* enabel N-CTS can be written in any time */
	vWriteByteHdmiGRL(GRL_DIVN, bData);

}

void vHDMIResetGenReg(u8 ui1resindex, u8 ui1colorspace)
{
	MHL_DRV_FUNC();
	/* vResetHDMI(1); */
	/* vResetHDMI(0); */
	vEnableNotice(TRUE);
	vEnableNCTSAutoWrite();
}

void vAudioPacketOff(u8 bOn)
{
	u8 bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_SHIFT_R2);
	if (bOn)
		bData |= 0x40;
	else
		bData &= ~0x40;

	vWriteByteHdmiGRL(GRL_SHIFT_R2, bData);
}

void vSetChannelSwap(u8 u1SwapBit)
{
	MHL_AUDIO_FUNC();
	vWriteHdmiGRLMsk(GRL_CH_SWAP, u1SwapBit, 0xff);
}

void vEnableIecTxRaw(void)
{
	u8 bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_MIX_CTRL);
	bData |= MIX_CTRL_FLAT;
	vWriteByteHdmiGRL(GRL_MIX_CTRL, bData);
}

void vSetHdmiI2SDataFmt(u8 bFmt)
{
	u8 bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_CFG0);
	bData &= ~0x33;
	switch (bFmt) {
	case RJT_24BIT:
		bData |= (CFG0_I2S_MODE_RTJ | CFG0_I2S_MODE_24Bit);
		break;

	case RJT_16BIT:
		bData |= (CFG0_I2S_MODE_RTJ | CFG0_I2S_MODE_16Bit);
		break;

	case LJT_24BIT:
		bData |= (CFG0_I2S_MODE_LTJ | CFG0_I2S_MODE_24Bit);
		break;

	case LJT_16BIT:
		bData |= (CFG0_I2S_MODE_LTJ | CFG0_I2S_MODE_16Bit);
		break;

	case I2S_24BIT:
		bData |= (CFG0_I2S_MODE_I2S | CFG0_I2S_MODE_24Bit);
		break;

	case I2S_16BIT:
		bData |= (CFG0_I2S_MODE_I2S | CFG0_I2S_MODE_16Bit);
		break;

	}


	vWriteByteHdmiGRL(GRL_CFG0, bData);
}

void vAOUT_BNUM_SEL(BYTE bBitNum)
{
	MHL_AUDIO_FUNC();
	vWriteByteHdmiGRL(GRL_AOUT_BNUM_SEL, bBitNum);

}

void vSetHdmiHighBitrate(u8 fgHighBitRate)
{
	u8 bData;
	MHL_AUDIO_FUNC();
	if (fgHighBitRate == TRUE) {
		bData = bReadByteHdmiGRL(GRL_AOUT_BNUM_SEL);
		bData |= HIGH_BIT_RATE_PACKET_ALIGN;
		vWriteByteHdmiGRL(GRL_AOUT_BNUM_SEL, bData);
		udelay(100);	/* 1ms */
		bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
		bData |= HIGH_BIT_RATE;
		vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
	} else {
		bData = bReadByteHdmiGRL(GRL_AOUT_BNUM_SEL);
		bData &= ~HIGH_BIT_RATE_PACKET_ALIGN;
		vWriteByteHdmiGRL(GRL_AOUT_BNUM_SEL, bData);

		bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
		bData &= ~HIGH_BIT_RATE;
		vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
	}


}

void vDSTNormalDouble(u8 fgEnable)
{
	u8 bData;
	MHL_AUDIO_FUNC();
	if (fgEnable) {
		bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
		bData |= DST_NORMAL_DOUBLE;
		vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
	} else {
		bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
		bData &= ~DST_NORMAL_DOUBLE;
		vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
	}

}

void vEnableDSTConfig(u8 fgEnable)
{
	u8 bData;
	MHL_AUDIO_FUNC();
	if (fgEnable) {
		bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
		bData |= SACD_DST;
		vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
	} else {
		bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
		bData &= ~SACD_DST;
		vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
	}

}

void vDisableDsdConfig(void)
{
	u8 bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
	bData &= ~SACD_SEL;
	vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);

}

void vSetHdmiI2SChNum(u8 bChNum, u8 bChMapping)
{
	u8 bData, bData1, bData2, bData3;
	MHL_AUDIO_FUNC();
	if (bChNum == 2)	/* I2S 2ch */
	{
		bData = 0x04;	/* 2ch data */
		bData1 = 0x50;	/* data0 */


	} else if ((bChNum == 3) || (bChNum == 4))	/* I2S 2ch */
	{
		if ((bChNum == 4) && (bChMapping == 0x08)) {
			bData = 0x14;	/* 4ch data */

		} else {
			bData = 0x0c;	/* 4ch data */
		}
		bData1 = 0x50;	/* data0 */


	} else if ((bChNum == 6) || (bChNum == 5))	/* I2S 5.1ch */
	{
		if ((bChNum == 6) && (bChMapping == 0x0E)) {
			bData = 0x3C;	/* 6.0 ch data */
			bData1 = 0x50;	/* data0 */
		} else {
			bData = 0x1C;	/* 5.1ch data, 5/0ch */
			bData1 = 0x50;	/* data0 */
		}


	} else if (bChNum == 8)	/* I2S 5.1ch */
	{
		bData = 0x3C;	/* 7.1ch data */
		bData1 = 0x50;	/* data0 */
	} else if (bChNum == 7)	/* I2S 6.1ch */
	{
		bData = 0x3C;	/* 6.1ch data */
		bData1 = 0x50;	/* data0 */
	} else {
		bData = 0x04;	/* 2ch data */
		bData1 = 0x50;	/* data0 */
	}

	bData2 = 0xc6;
	bData3 = 0xfa;

	vWriteByteHdmiGRL(GRL_CH_SW0, bData1);
	vWriteByteHdmiGRL(GRL_CH_SW1, bData2);
	vWriteByteHdmiGRL(GRL_CH_SW2, bData3);
	vWriteByteHdmiGRL(GRL_I2S_UV, bData);

	/* vDisableDsdConfig(); */

}

void vSetHdmiIecI2s(u8 bIn)
{
	u8 bData;
	MHL_AUDIO_FUNC();
	if (bIn == SV_SPDIF) {

		bData = bReadByteHdmiGRL(GRL_CFG1);

		if ((bData & CFG1_SPDIF) == 0) {
			bData |= CFG1_SPDIF;
			vWriteByteHdmiGRL(GRL_CFG1, bData);
		}
	} else {
		bData = bReadByteHdmiGRL(GRL_CFG1);
		if (bData & CFG1_SPDIF) {
			bData &= ~CFG1_SPDIF;
			vWriteByteHdmiGRL(GRL_CFG1, bData);
		}
		bData = bReadByteHdmiGRL(GRL_CFG1);
	}
}

void vSetHDMIAudioIn(void)
{
	u8 bData2;

	MHL_AUDIO_FUNC();

	bData2 = vCheckPcmBitSize(0);

	vSetChannelSwap(LFE_CC_SWAP);
	vEnableIecTxRaw();

	vSetHdmiI2SDataFmt(I2S_16BIT);

	if (bData2 == PCM_24BIT)
		vAOUT_BNUM_SEL(AOUT_24BIT);
	else
		vAOUT_BNUM_SEL(AOUT_16BIT);

	vSetHdmiHighBitrate(FALSE);
	vDSTNormalDouble(FALSE);
	vEnableDSTConfig(FALSE);

	vDisableDsdConfig();
	vSetHdmiI2SChNum(2, 0);
	vSetHdmiIecI2s(SV_I2S);

}

void vHwNCTSOnOff(u8 bHwNctsOn)
{
	u8 bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_CTS_CTRL);

	if (bHwNctsOn == TRUE)
		bData &= ~CTS_CTRL_SOFT;
	else
		bData |= CTS_CTRL_SOFT;

	vWriteByteHdmiGRL(GRL_CTS_CTRL, bData);

}

void vHalHDMI_NCTS(u8 bAudioFreq, u8 bPix)
{
	u8 bTemp, bData, bData1[NCTS_BYTES];
	u32 u4Temp, u4NTemp = 0;

	MHL_AUDIO_FUNC();
	MHL_AUDIO_LOG("bAudioFreq=%d,  bPix=%d\n", bAudioFreq, bPix);

	bData = 0;
	vWriteByteHdmiGRL(GRL_NCTS, bData);	/* YT suggest 3 dummy N-CTS */
	vWriteByteHdmiGRL(GRL_NCTS, bData);
	vWriteByteHdmiGRL(GRL_NCTS, bData);

	for (bTemp = 0; bTemp < NCTS_BYTES; bTemp++) {
		bData1[bTemp] = 0;
	}

	for (bTemp = 0; bTemp < NCTS_BYTES; bTemp++) {

		if ((bAudioFreq < 7) && (bPix < 9))

			bData1[bTemp] = HDMI_NCTS[bAudioFreq][bPix][bTemp];
	}

	u4NTemp = (bData1[4] << 16) | (bData1[5] << 8) | (bData1[6]);	/* N */
	u4Temp = (bData1[0] << 24) | (bData1[1] << 16) | (bData1[2] << 8) | (bData1[3]);	/* CTS */

	for (bTemp = 0; bTemp < NCTS_BYTES; bTemp++) {
		bData = bData1[bTemp];
		vWriteByteHdmiGRL(GRL_NCTS, bData);
	}

	_u4NValue = u4NTemp;
}

void vHDMI_NCTS(u8 bHDMIFsFreq, u8 bResolution)
{
	u8 bPix;

	MHL_AUDIO_FUNC();

	vWriteHdmiGRLMsk(DUMMY_304, AUDIO_I2S_NCTS_SEL_64, AUDIO_I2S_NCTS_SEL);

	switch (bResolution) {
	case HDMI_VIDEO_720x480p_60Hz:
	case HDMI_VIDEO_720x576p_50Hz:
	default:
		bPix = 0;
		break;

	case HDMI_VIDEO_1280x720p_60Hz:	/* 74.175M pixel clock */
	case HDMI_VIDEO_1920x1080i_60Hz:
	case HDMI_VIDEO_1920x1080p_23Hz:
		bPix = 2;
		break;

	case HDMI_VIDEO_1280x720p_50Hz:	/* 74.25M pixel clock */
	case HDMI_VIDEO_1920x1080i_50Hz:
	case HDMI_VIDEO_1920x1080p_24Hz:
		bPix = 3;
		break;
	}

	vHalHDMI_NCTS(bHDMIFsFreq, bPix);

}

void vSetHdmiClkRefIec2(u8 fgSyncIec2Clock)
{
	MHL_AUDIO_FUNC();
}

void vSetHDMISRCOff(void)
{
	u8 bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_MIX_CTRL);
	bData &= ~MIX_CTRL_SRC_EN;
	vWriteByteHdmiGRL(GRL_MIX_CTRL, bData);
	bData = 0x00;
	vWriteByteHdmiGRL(GRL_SHIFT_L1, bData);
}

void vHalSetHDMIFS(u8 bFs)
{
	u8 bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_CFG5);
	bData &= CFG5_CD_RATIO_MASK;
	bData |= bFs;
	vWriteByteHdmiGRL(GRL_CFG5, bData);

}

void vHdmiAclkInv(u8 bInv)
{
	u8 bData;
	MHL_AUDIO_FUNC();
	if (bInv == TRUE) {
		bData = bReadByteHdmiGRL(GRL_CFG2);
		bData |= 0x80;
		vWriteByteHdmiGRL(GRL_CFG2, bData);
	} else {
		bData = bReadByteHdmiGRL(GRL_CFG2);
		bData &= ~0x80;
		vWriteByteHdmiGRL(GRL_CFG2, bData);
	}
}

void vSetHDMIFS(u8 bFs, u8 fgAclInv)
{
	MHL_AUDIO_FUNC();
	vHalSetHDMIFS(bFs);

	if (fgAclInv) {
		vHdmiAclkInv(TRUE);	/* //fix 192kHz, SPDIF downsample noise issue, ACL iNV */
	} else {
		vHdmiAclkInv(FALSE);	/* fix 192kHz, SPDIF downsample noise issue */
	}

	_stAvdAVInfo.u1HdmiI2sMclk = bFs;
}

void vReEnableSRC(void)
{
	u8 bData;
	MHL_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_MIX_CTRL);
	if (bData & MIX_CTRL_SRC_EN) {
		bData &= ~MIX_CTRL_SRC_EN;
		vWriteByteHdmiGRL(GRL_MIX_CTRL, bData);
		udelay(255);
		bData |= MIX_CTRL_SRC_EN;
		vWriteByteHdmiGRL(GRL_MIX_CTRL, bData);
	}

}

void vHDMIAudioSRC(u8 ui1hdmifs, u8 ui1resindex)
{

	MHL_AUDIO_FUNC();

	vHwNCTSOnOff(FALSE);

	vSetHdmiClkRefIec2(FALSE);

	switch (ui1hdmifs) {
	case HDMI_FS_44K:
		vSetHDMISRCOff();
		vSetHDMIFS(CFG5_FS128, FALSE);
		break;

	case HDMI_FS_48K:
		vSetHDMISRCOff();
		vSetHDMIFS(CFG5_FS128, FALSE);
		break;

	default:
		break;
	}

	vHDMI_NCTS(ui1hdmifs, ui1resindex);
	vReEnableSRC();

}

void vHwSet_Hdmi_I2S_C_Status(u8 *prLChData, u8 *prRChData)
{
	u8 bData;
	MHL_AUDIO_FUNC();
	bData = prLChData[0];

	vWriteByteHdmiGRL(GRL_I2S_C_STA0, bData);
	vWriteByteHdmiGRL(GRL_L_STATUS_0, bData);

	bData = prRChData[0];

	vWriteByteHdmiGRL(GRL_R_STATUS_0, bData);

	bData = prLChData[1];
	vWriteByteHdmiGRL(GRL_I2S_C_STA1, bData);
	vWriteByteHdmiGRL(GRL_L_STATUS_1, bData);
	bData = prRChData[1];
	vWriteByteHdmiGRL(GRL_R_STATUS_1, bData);

	bData = prLChData[2];
	vWriteByteHdmiGRL(GRL_I2S_C_STA2, bData);
	vWriteByteHdmiGRL(GRL_L_STATUS_2, bData);
	bData = prRChData[2];
	vWriteByteHdmiGRL(GRL_R_STATUS_2, bData);

	bData = prLChData[3];
	vWriteByteHdmiGRL(GRL_I2S_C_STA3, bData);
	vWriteByteHdmiGRL(GRL_L_STATUS_3, bData);
	bData = prRChData[3];
	vWriteByteHdmiGRL(GRL_R_STATUS_3, bData);

	bData = prLChData[4];
	vWriteByteHdmiGRL(GRL_I2S_C_STA4, bData);
	vWriteByteHdmiGRL(GRL_L_STATUS_4, bData);
	bData = prRChData[4];
	vWriteByteHdmiGRL(GRL_R_STATUS_4, bData);

	for (bData = 0; bData < 19; bData++) {
		vWriteByteHdmiGRL(GRL_L_STATUS_5 + bData * 4, 0);
		vWriteByteHdmiGRL(GRL_R_STATUS_5 + bData * 4, 0);

	}
}

void vHDMI_I2S_C_Status(void)
{
	u8 bData = 0;
	u8 bhdmi_RCh_status[5];
	u8 bhdmi_LCh_status[5];

	MHL_AUDIO_FUNC();

	bhdmi_LCh_status[0] = _stAvdAVInfo.bhdmiLChstatus[0];
	bhdmi_LCh_status[1] = _stAvdAVInfo.bhdmiLChstatus[1];
	bhdmi_LCh_status[2] = _stAvdAVInfo.bhdmiLChstatus[2];
	bhdmi_RCh_status[0] = _stAvdAVInfo.bhdmiRChstatus[0];
	bhdmi_RCh_status[1] = _stAvdAVInfo.bhdmiRChstatus[1];
	bhdmi_RCh_status[2] = _stAvdAVInfo.bhdmiRChstatus[2];


	bhdmi_LCh_status[0] &= ~0x02;
	bhdmi_RCh_status[0] &= ~0x02;

	bData = _stAvdAVInfo.bhdmiLChstatus[3] & 0xf0;

	switch (_stAvdAVInfo.e_hdmi_fs) {
	case HDMI_FS_32K:
		bData |= 0x03;
		break;
	case HDMI_FS_44K:
		break;

	case HDMI_FS_48K:
	default:
		bData |= 0x02;
		break;
	}


	bhdmi_LCh_status[3] = bData;
	bhdmi_RCh_status[3] = bData;

	bData = _stAvdAVInfo.bhdmiLChstatus[4];

	bData |= ((~(bhdmi_LCh_status[3] & 0x0f)) << 4);

	bhdmi_LCh_status[4] = bData;
	bhdmi_RCh_status[4] = bData;

	vHwSet_Hdmi_I2S_C_Status(&bhdmi_LCh_status[0], &bhdmi_RCh_status[0]);

}

void vHalSendAudioInfoFrame(u8 bData1, u8 bData2, u8 bData4, u8 bData5)
{
	u8 bAUDIO_CHSUM;
	u8 bData = 0;
	MHL_AUDIO_FUNC();
	vWriteHdmiGRLMsk(GRL_CTRL, 0, CTRL_AUDIO_EN);
	vWriteByteHdmiGRL(GRL_INFOFRM_VER, AUDIO_VERS);
	vWriteByteHdmiGRL(GRL_INFOFRM_TYPE, AUDIO_TYPE);
	vWriteByteHdmiGRL(GRL_INFOFRM_LNG, AUDIO_LEN);

	bAUDIO_CHSUM = AUDIO_TYPE + AUDIO_VERS + AUDIO_LEN;

	bAUDIO_CHSUM += bData1;
	bAUDIO_CHSUM += bData2;
	bAUDIO_CHSUM += bData4;
	bAUDIO_CHSUM += bData5;

	bAUDIO_CHSUM = 0x100 - bAUDIO_CHSUM;
	vWriteByteHdmiGRL(GRL_IFM_PORT, bAUDIO_CHSUM);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData1);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData2);	/* bData2 */
	vWriteByteHdmiGRL(GRL_IFM_PORT, 0);	/* bData3 */
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData4);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData5);

	for (bData = 0; bData < 5; bData++) {
		vWriteByteHdmiGRL(GRL_IFM_PORT, 0);
	}
	bData = bReadByteHdmiGRL(GRL_CTRL);
	bData |= CTRL_AUDIO_EN;
	vWriteByteHdmiGRL(GRL_CTRL, bData);

}

void vSendAudioInfoFrame(void)
{
	MHL_AUDIO_FUNC();
	_bAudInfoFm[0] = 0x01;
	_bAudInfoFm[2] = 0x00;
	_bAudInfoFm[1] = 0;
	_bAudInfoFm[3] = 0x0;
	_bAudInfoFm[4] = 0x0;
	vHalSendAudioInfoFrame(_bAudInfoFm[0], _bAudInfoFm[1], _bAudInfoFm[2], _bAudInfoFm[3]);
}

void vChgHDMIAudioOutput(u8 ui1hdmifs, u8 ui1resindex)
{
	u32 ui4Index;

	MHL_AUDIO_FUNC();
	vWriteHdmiSYSMsk(HDMI_SYS_CFG0, HDMI_SPDIF_ON, HDMI_SPDIF_ON);
	MuteHDMIAudio();
	vAudioPacketOff(TRUE);
	vSetHDMIAudioIn();
	vHDMIAudioSRC(ui1hdmifs, ui1resindex);

	vHDMI_I2S_C_Status();
	vSendAudioInfoFrame();

	for (ui4Index = 0; ui4Index < 5; ui4Index++) {
		udelay(5);
	}
	vHwNCTSOnOff(TRUE);

	vAudioPacketOff(FALSE);
}

void vDisableGamut(void)
{
	MHL_AUDIO_FUNC();
	vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, GAMUT_EN);
}

void vHalSendAVIInfoFrame(u8 *pr_bData)
{
	u8 bAVI_CHSUM;
	u8 bData1 = 0, bData2 = 0, bData3 = 0, bData4 = 0, bData5 = 0;
	u8 bData;
	MHL_VIDEO_FUNC();
	bData1 = *pr_bData;
	bData2 = *(pr_bData + 1);
	bData3 = *(pr_bData + 2);
	bData4 = *(pr_bData + 3);
	bData5 = *(pr_bData + 4);

	vWriteHdmiGRLMsk(GRL_CTRL, 0, CTRL_AVI_EN);
	vWriteByteHdmiGRL(GRL_INFOFRM_VER, AVI_VERS);
	vWriteByteHdmiGRL(GRL_INFOFRM_TYPE, AVI_TYPE);
	vWriteByteHdmiGRL(GRL_INFOFRM_LNG, AVI_LEN);

	bAVI_CHSUM = AVI_TYPE + AVI_VERS + AVI_LEN;

	bAVI_CHSUM += bData1;
	bAVI_CHSUM += bData2;
	bAVI_CHSUM += bData3;
	bAVI_CHSUM += bData4;
	bAVI_CHSUM += bData5;
	bAVI_CHSUM = 0x100 - bAVI_CHSUM;
	vWriteByteHdmiGRL(GRL_IFM_PORT, bAVI_CHSUM);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData1);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData2);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData3);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData4);
	vWriteByteHdmiGRL(GRL_IFM_PORT, bData5);

	for (bData2 = 0; bData2 < 8; bData2++) {
		vWriteByteHdmiGRL(GRL_IFM_PORT, 0);
	}
	bData = bReadByteHdmiGRL(GRL_CTRL);
	bData |= CTRL_AVI_EN;
	vWriteByteHdmiGRL(GRL_CTRL, bData);
}

void vSendAVIInfoFrame(u8 ui1resindex, u8 ui1colorspace)
{
	MHL_VIDEO_FUNC();

	if (ui1colorspace == HDMI_YCBCR_444) {
		_bAviInfoFm[0] = 0x40;
	} else if (ui1colorspace == HDMI_YCBCR_422) {
		_bAviInfoFm[0] = 0x20;
	} else {
		_bAviInfoFm[0] = 0x00;
	}

	_bAviInfoFm[0] |= 0x10;	/* A0=1, Active format (R0~R3) inf valid */

	_bAviInfoFm[1] = 0x0;	/* bData2 */

	if (ui1resindex == HDMI_VIDEO_1280x720p_60Hz_3D)
		ui1resindex = HDMI_VIDEO_1280x720p_60Hz;
	else if (ui1resindex == HDMI_VIDEO_1280x720p_50Hz_3D)
		ui1resindex = HDMI_VIDEO_1280x720p_50Hz;
	else if (ui1resindex == HDMI_VIDEO_1920x1080p_24Hz_3D)
		ui1resindex = HDMI_VIDEO_1920x1080p_24Hz;
	else if (ui1resindex == HDMI_VIDEO_1920x1080p_23Hz_3D)
		ui1resindex = HDMI_VIDEO_1920x1080p_23Hz;

	if ((ui1resindex == HDMI_VIDEO_720x480p_60Hz) || (ui1resindex == HDMI_VIDEO_720x576p_50Hz)) {
		_bAviInfoFm[1] |= AV_INFO_SD_ITU601;
	} else {
		_bAviInfoFm[1] |= AV_INFO_HD_ITU709;
	}

	_bAviInfoFm[1] |= 0x20;
	_bAviInfoFm[1] |= 0x08;
	_bAviInfoFm[2] = 0;	/* bData3 */
	_bAviInfoFm[2] |= 0x04;	/* limit Range */
	_bAviInfoFm[3] = HDMI_VIDEO_ID_CODE[ui1resindex];	/* bData4 */

	if ((_bAviInfoFm[1] & AV_INFO_16_9_OUTPUT)
	    && ((ui1resindex == HDMI_VIDEO_720x480p_60Hz)
		|| (ui1resindex == HDMI_VIDEO_720x576p_50Hz))) {
		_bAviInfoFm[3] = _bAviInfoFm[3] + 1;
	}

	_bAviInfoFm[4] = 0x00;

	vHalSendAVIInfoFrame(&_bAviInfoFm[0]);

}

/************************************************************************
     Function :void vHalSendSPDInfoFrame(BYTE *pr_bData)
  Description : Setting SPD infoframe according to EIA/CEA-861-B
  Parameter   : None
  Return      : None
************************************************************************/
void vHalSendVendorSpecificInfoFrame(bool fg3DRes, u8 b3dstruct)
{
	unsigned char bVS_CHSUM;
	unsigned char bPB1, bPB2, bPB3, bPB4, bPB5;
	/* bPB5 */
	vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, VS_EN);

	if (fg3DRes == FALSE)
		return;

	vWriteByteHdmiGRL(GRL_INFOFRM_VER, VS_VERS);	/* HB1 */

	vWriteByteHdmiGRL(GRL_INFOFRM_TYPE, VS_TYPE);	/* HB0 */

	vWriteByteHdmiGRL(GRL_INFOFRM_LNG, VS_LEN);	/* HB2 */
	bPB1 = 0x1D;
	bPB2 = 0xA6;
	bPB3 = 0x7C;

	bPB4 = 0x01;

	bVS_CHSUM = VS_VERS + VS_TYPE + VS_LEN + bPB1 + bPB2 + bPB3 + bPB4;

	bVS_CHSUM = 0x100 - bVS_CHSUM;

	vWriteByteHdmiGRL(GRL_IFM_PORT, bVS_CHSUM);	/* check sum */
	vWriteByteHdmiGRL(GRL_IFM_PORT, bPB1);	/* 24bit IEEE Registration Identifier */
	vWriteByteHdmiGRL(GRL_IFM_PORT, bPB2);	/* 24bit IEEE Registration Identifier */
	vWriteByteHdmiGRL(GRL_IFM_PORT, bPB3);	/* 24bit IEEE Registration Identifier */
	vWriteByteHdmiGRL(GRL_IFM_PORT, bPB4);	/* HDMI_Video_Format */

	vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, VS_EN, VS_EN);
	printk(" vendor: bVS_CHSUM = %d, bPB1 = %d, bPB2 = %d, bPB3 = %d, bPB4 = %d\n", bVS_CHSUM,
	       bPB1, bPB2, bPB3, bPB4);
}

void vSendVendorSpecificInfoFrame(void)
{
	unsigned char b3DStruct;
	bool fg3DRes;

	if ((_stAvdAVInfo.e_resolution == HDMI_VIDEO_1280x720p_60Hz_3D)
	    || (_stAvdAVInfo.e_resolution == HDMI_VIDEO_1280x720p_50Hz_3D)
	    || (_stAvdAVInfo.e_resolution == HDMI_VIDEO_1920x1080p_24Hz_3D)
	    || (_stAvdAVInfo.e_resolution == HDMI_VIDEO_1920x1080p_23Hz_3D))
		fg3DRes = TRUE;
	else
		fg3DRes = FALSE;

	b3DStruct = 0;

	if (fg3DRes == TRUE)
		vHalSendVendorSpecificInfoFrame(TRUE, b3DStruct);
	else
		vHalSendVendorSpecificInfoFrame(0, 0);
}

void vHalSendSPDInfoFrame(u8 *pr_bData)
{
	u8 bSPD_CHSUM, bData;
	u8 i = 0;

	MHL_VIDEO_FUNC();
	vWriteHdmiGRLMsk(GRL_CTRL, 0, CTRL_SPD_EN);
	vWriteByteHdmiGRL(GRL_INFOFRM_VER, SPD_VERS);
	vWriteByteHdmiGRL(GRL_INFOFRM_TYPE, SPD_TYPE);

	vWriteByteHdmiGRL(GRL_INFOFRM_LNG, SPD_LEN);
	bSPD_CHSUM = SPD_TYPE + SPD_VERS + SPD_LEN;

	for (i = 0; i < SPD_LEN; i++) {
		bSPD_CHSUM += (*(pr_bData + i));
	}

	bSPD_CHSUM = 0x100 - bSPD_CHSUM;
	vWriteByteHdmiGRL(GRL_IFM_PORT, bSPD_CHSUM);
	for (i = 0; i < SPD_LEN; i++)
		vWriteByteHdmiGRL(GRL_IFM_PORT, *(pr_bData + i));

	bData = bReadByteHdmiGRL(GRL_CTRL);
	bData |= CTRL_SPD_EN;
	vWriteByteHdmiGRL(GRL_CTRL, bData);

}

void vSend_AVUNMUTE(void)
{
	u8 bData;
	MHL_VIDEO_FUNC();

	bData = bReadByteHdmiGRL(GRL_CFG4);
	bData |= CFG4_AV_UNMUTE_EN;	/* disable original mute */
	bData &= ~CFG4_AV_UNMUTE_SET;	/* disable */

	vWriteByteHdmiGRL(GRL_CFG4, bData);
	udelay(30);

	bData &= ~CFG4_AV_UNMUTE_EN;	/* disable original mute */
	bData |= CFG4_AV_UNMUTE_SET;	/* disable */

	vWriteByteHdmiGRL(GRL_CFG4, bData);

}

void vChgHDMIVideoResolution(u8 ui1resindex, u8 ui1colorspace, u8 ui1hdmifs)
{
	u32 u4Index;
	MHL_VIDEO_FUNC();

	vHDMIAVMute();
	vHDMIResetGenReg(ui1resindex, ui1colorspace);

	vChgHDMIAudioOutput(ui1hdmifs, ui1resindex);
	for (u4Index = 0; u4Index < 5; u4Index++) {
		udelay(10);
	}

	vDisableGamut();
	vSendAVIInfoFrame(ui1resindex, ui1colorspace);
	vHalSendSPDInfoFrame(&_bSpdInf[0]);
	vSendVendorSpecificInfoFrame();
	vSend_AVUNMUTE();
}

void vChgtoSoftNCTS(u8 ui1resindex, u8 ui1audiosoft, u8 ui1hdmifs)
{
	MHL_AUDIO_FUNC();

	vHwNCTSOnOff(ui1audiosoft);	/* change to software NCTS; */
	vHDMI_NCTS(ui1hdmifs, ui1resindex);

}

void vShowHpdRsenStatus(void)
{
	if (bCheckPordHotPlug() == TRUE)
		printk("[HDMI]RSEN ON\n");
	else
		printk("[HDMI]RSEN OFF\n");

}

void vShowOutputVideoResolution(void)
{
	printk("[HDMI]HDMI output resolution = %s\n", szHdmiResStr[_stAvdAVInfo.e_resolution]);	/*  */

}

void vShowColorSpace(void)
{
	if (_stAvdAVInfo.e_video_color_space == HDMI_RGB)
		printk("[HDMI]HDMI output colorspace = HDMI_RGB\n");
	else if (_stAvdAVInfo.e_video_color_space == HDMI_RGB_FULL)
		printk("[HDMI]HDMI output colorspace = HDMI_RGB_FULL\n");
	else if (_stAvdAVInfo.e_video_color_space == HDMI_YCBCR_444)
		printk("[HDMI]HDMI output colorspace = HDMI_YCBCR_444\n");
	else if (_stAvdAVInfo.e_video_color_space == HDMI_YCBCR_422)
		printk("[HDMI]HDMI output colorspace = HDMI_YCBCR_422\n");
	else if (_stAvdAVInfo.e_video_color_space == HDMI_XV_YCC)
		printk("[HDMI]HDMI output colorspace = HDMI_XV_YCC\n");
	else
		printk("[HDMI]HDMI output colorspace error\n");

}

void vShowInforFrame(void)
{
	printk("====================Audio inforFrame Start ====================================\n");
	printk("Data Byte (1~5) = 0x%x  0x%x  0x%x  0x%x  0x%x\n", _bAudInfoFm[0], _bAudInfoFm[1],
	       _bAudInfoFm[2], _bAudInfoFm[3], _bAudInfoFm[4]);
	printk("CC2~ CC0: 0x%x, %s\n", _bAudInfoFm[0] & 0x07,
	       cAudChCountStr[_bAudInfoFm[0] & 0x07]);
	printk("CT3~ CT0: 0x%x, %s\n", (_bAudInfoFm[0] >> 4) & 0x0f,
	       cAudCodingTypeStr[(_bAudInfoFm[0] >> 4) & 0x0f]);
	printk("SS1, SS0: 0x%x, %s\n", _bAudInfoFm[1] & 0x03,
	       cAudSampleSizeStr[_bAudInfoFm[1] & 0x03]);
	printk("SF2~ SF0: 0x%x, %s\n", (_bAudInfoFm[1] >> 2) & 0x07,
	       cAudFsStr[(_bAudInfoFm[1] >> 2) & 0x07]);
	printk("CA7~ CA0: 0x%x, %s\n", _bAudInfoFm[3] & 0xff, cAudChMapStr[_bAudInfoFm[3] & 0xff]);
	printk("LSV3~LSV0: %d db\n", (_bAudInfoFm[4] >> 3) & 0x0f);
	printk("DM_INH: 0x%x ,%s\n", (_bAudInfoFm[4] >> 7) & 0x01,
	       cAudDMINHStr[(_bAudInfoFm[4] >> 7) & 0x01]);
	printk("====================Audio inforFrame End ======================================\n");

	printk("====================AVI inforFrame Start ====================================\n");
	printk("Data Byte (1~5) = 0x%x  0x%x  0x%x  0x%x  0x%x\n", _bAviInfoFm[0], _bAviInfoFm[1],
	       _bAviInfoFm[2], _bAviInfoFm[3], _bAviInfoFm[4]);
	printk("S1,S0: 0x%x, %s\n", _bAviInfoFm[0] & 0x03, cAviScanStr[_bAviInfoFm[0] & 0x03]);
	printk("B1,S0: 0x%x, %s\n", (_bAviInfoFm[0] >> 2) & 0x03,
	       cAviBarStr[(_bAviInfoFm[0] >> 2) & 0x03]);
	printk("A0: 0x%x, %s\n", (_bAviInfoFm[0] >> 4) & 0x01,
	       cAviActivePresentStr[(_bAviInfoFm[0] >> 4) & 0x01]);
	printk("Y1,Y0: 0x%x, %s\n", (_bAviInfoFm[0] >> 5) & 0x03,
	       cAviRgbYcbcrStr[(_bAviInfoFm[0] >> 5) & 0x03]);
	printk("R3~R0: 0x%x, %s\n", (_bAviInfoFm[1]) & 0x0f,
	       cAviActiveStr[(_bAviInfoFm[1]) & 0x0f]);
	printk("M1,M0: 0x%x, %s\n", (_bAviInfoFm[1] >> 4) & 0x03,
	       cAviAspectStr[(_bAviInfoFm[1] >> 4) & 0x03]);
	printk("C1,C0: 0x%x, %s\n", (_bAviInfoFm[1] >> 6) & 0x03,
	       cAviColorimetryStr[(_bAviInfoFm[1] >> 6) & 0x03]);
	printk("SC1,SC0: 0x%x, %s\n", (_bAviInfoFm[2]) & 0x03,
	       cAviScaleStr[(_bAviInfoFm[2]) & 0x03]);
	printk("Q1,Q0: 0x%x, %s\n", (_bAviInfoFm[2] >> 2) & 0x03,
	       cAviRGBRangeStr[(_bAviInfoFm[2] >> 2) & 0x03]);
	if (((_bAviInfoFm[2] >> 4) & 0x07) <= 1)
		printk("EC2~EC0: 0x%x, %s\n", (_bAviInfoFm[2] >> 4) & 0x07,
		       cAviExtColorimetryStr[(_bAviInfoFm[2] >> 4) & 0x07]);
	else
		printk("EC2~EC0: resevered\n");
	printk("ITC: 0x%x, %s\n", (_bAviInfoFm[2] >> 7) & 0x01,
	       cAviItContentStr[(_bAviInfoFm[2] >> 7) & 0x01]);
	printk("====================AVI inforFrame End ======================================\n");

	printk("====================SPD inforFrame Start ====================================\n");
	printk("Data Byte (1~8)  = 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n", _bSpdInf[0],
	       _bSpdInf[1], _bSpdInf[2], _bSpdInf[3], _bSpdInf[4], _bSpdInf[5], _bSpdInf[6],
	       _bSpdInf[7]);
	printk("Data Byte (9~16) = 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n", _bSpdInf[8],
	       _bSpdInf[9], _bSpdInf[10], _bSpdInf[11], _bSpdInf[12], _bSpdInf[13], _bSpdInf[14],
	       _bSpdInf[15]);
	printk("Data Byte (17~24)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n", _bSpdInf[16],
	       _bSpdInf[17], _bSpdInf[18], _bSpdInf[19], _bSpdInf[20], _bSpdInf[21], _bSpdInf[22],
	       _bSpdInf[23]);
	printk("Data Byte  25    = 0x%x\n", _bSpdInf[24]);
	printk("Source Device information is %s\n", cSPDDeviceStr[_bSpdInf[24]]);
	printk("====================SPD inforFrame End ======================================\n");

	if (fgIsAcpEnable()) {
		printk
		    ("====================ACP inforFrame Start ====================================\n");
		printk("Acp type =0x%x\n", _bAcpType);

		if (_bAcpType == 0) {
			printk("Generic Audio\n");
			printk("Data Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			       _bAcpData[0], _bAcpData[1], _bAcpData[2], _bAcpData[3], _bAcpData[4],
			       _bAcpData[5], _bAcpData[6], _bAcpData[7]);
			printk("Data Byte (9~16)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			       _bAcpData[8], _bAcpData[9], _bAcpData[10], _bAcpData[11],
			       _bAcpData[12], _bAcpData[13], _bAcpData[14], _bAcpData[15]);
		} else if (_bAcpType == 1) {
			printk("IEC 60958-Identified Audio\n");
			printk("Data Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			       _bAcpData[0], _bAcpData[1], _bAcpData[2], _bAcpData[3], _bAcpData[4],
			       _bAcpData[5], _bAcpData[6], _bAcpData[7]);
			printk("Data Byte (9~16)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			       _bAcpData[8], _bAcpData[9], _bAcpData[10], _bAcpData[11],
			       _bAcpData[12], _bAcpData[13], _bAcpData[14], _bAcpData[15]);
		} else if (_bAcpType == 2) {
			printk("DVD Audio\n");
			printk("DVD-AUdio_TYPE_Dependent Generation = 0x%x\n", _bAcpData[0]);
			printk("Copy Permission = 0x%x\n", (_bAcpData[1] >> 6) & 0x03);
			printk("Copy Number = 0x%x\n", (_bAcpData[1] >> 3) & 0x07);
			printk("Quality = 0x%x\n", (_bAcpData[1] >> 1) & 0x03);
			printk("Transaction = 0x%x\n", _bAcpData[1] & 0x01);

		} else if (_bAcpType == 3) {
			printk("SuperAudio CD\n");
			printk("CCI_1 Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			       _bAcpData[0], _bAcpData[1], _bAcpData[2], _bAcpData[3], _bAcpData[4],
			       _bAcpData[5], _bAcpData[6], _bAcpData[7]);
			printk
			    ("CCI_1 Byte (9~16)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			     _bAcpData[8], _bAcpData[9], _bAcpData[10], _bAcpData[11],
			     _bAcpData[12], _bAcpData[13], _bAcpData[14], _bAcpData[15]);

		}
		printk
		    ("====================ACP inforFrame End ======================================\n");
	}

	if (fgIsISRC1Enable()) {
		printk
		    ("====================ISRC1 inforFrame Start ====================================\n");
		printk("Data Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
		       _bIsrc1Data[0], _bIsrc1Data[1], _bIsrc1Data[2], _bIsrc1Data[3],
		       _bIsrc1Data[4], _bIsrc1Data[5], _bIsrc1Data[6], _bIsrc1Data[7]);
		printk
		    ("====================ISRC1 inforFrame End ======================================\n");
	}

	if (fgIsISRC2Enable()) {
		printk
		    ("====================ISRC2 inforFrame Start ====================================\n");
		printk("Data Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
		       _bIsrc1Data[8], _bIsrc1Data[9], _bIsrc1Data[10], _bIsrc1Data[11],
		       _bIsrc1Data[12], _bIsrc1Data[13], _bIsrc1Data[14], _bIsrc1Data[15]);
		printk
		    ("====================ISRC2 inforFrame End ======================================\n");
	}
}

u32 u4ReadNValue(void)
{
	return _u4NValue;
}

u32 u4ReadCtsValue(void)
{
	u32 u4Data;

	u4Data = bReadByteHdmiGRL(GRL_CTS0) & 0xff;
	u4Data |= ((bReadByteHdmiGRL(GRL_CTS1) & 0xff) << 8);
	u4Data |= ((bReadByteHdmiGRL(GRL_CTS2) & 0x0f) << 16);

	return u4Data;
}

void vShowHdmiAudioStatus(void)
{
	printk("[HDMI]HDMI output audio Channel Number =%d\n", _stAvdAVInfo.ui1_aud_out_ch_number);
	printk("[HDMI]HDMI output Audio Fs = %s\n", cHdmiAudFsStr[_stAvdAVInfo.e_hdmi_fs]);
	printk("[HDMI]HDMI MCLK =%d\n", _stAvdAVInfo.u1HdmiI2sMclk);
	printk("[HDMI]HDMI output ACR N= %d, CTS = %d\n", u4ReadNValue(), u4ReadCtsValue());
}

void vCheckHDMICRC(void)
{
	u16 u4Data = 0xffff, i;

	for (i = 0; i < 10; i++) {
		vWriteHdmiGRLMsk(CRC_CTRL, clr_crc_result, clr_crc_result | init_crc);
		vWriteHdmiGRLMsk(CRC_CTRL, init_crc, clr_crc_result | init_crc);

		mdelay(40);

		if (i == 0)
			u4Data =
			    (bReadByteHdmiGRL(CRC_RESULT_L) & 0xff) +
			    ((bReadByteHdmiGRL(CRC_RESULT_H) & 0xff) << 8);
		else {
			if ((u4Data !=
			     ((bReadByteHdmiGRL(CRC_RESULT_L) & 0xff) +
			      ((bReadByteHdmiGRL(CRC_RESULT_H) & 0xff) << 8))) || (u4Data == 0)) {
				printk("[HDMI]number = %d, u4Data = 0x%x\n", i, u4Data);
				printk("[HDMI]hdmi crc error\n");
				return;
			}
		}
	}
	printk("[HDMI]hdmi crc pass\n");
}

void vCheckHDMICLKPIN(void)
{
/*
  u32 u4Data, i;

  for(i=0; i<5; i++)
  {
  vWriteHdmiSYSMsk(HDMI_SYS_FMETER,TRI_CAL|CALSEL,TRI_CAL|CALSEL);
  vWriteHdmiSYSMsk(HDMI_SYS_FMETER,CLK_EXC,CLK_EXC);

  while(!(dReadHdmiSYS(HDMI_SYS_FMETER)&&CAL_OK));

  u4Data = ((dReadHdmiSYS(HDMI_SYS_FMETER)&(0xffff0000))>>16)*26000/1024;

  printk("[HDMI]hdmi pin clk = %d.%dM\n", (u4Data/1000), (u4Data-((u4Data/1000)*1000)));
  }
  */
}

void mhl_status(void)
{
	vShowHpdRsenStatus();
	vShowOutputVideoResolution();
	vShowColorSpace();
	vShowInforFrame();
	vShowHdmiAudioStatus();
	vShowEdidRawData();
	vShowEdidInformation();
	vShowHdcpRawData();

	vCheckHDMICRC();
	vCheckHDMICLKPIN();
}

/* ////////////////////////////////////////////////// */
/* / */
/* /             mhl infoframe debug */
/* / */
/* ////////////////////////////////////////////////// */
static u32 pdMpegInfoReg[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000085,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x000000dE,	/* check sum */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000
};

static u32 pdMpegInfoReg2[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000085,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x000000d4,	/* check sum */
	0x188, 0x00000041,
	0x188, 0x00000052,
	0x188, 0x00000003,
	0x188, 0x00000002,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000004
};


static u32 pdSpdInfoReg[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000083,	/* type */
	0x1A4, 0x00000019,	/* length, 25 bytes */
	0x188, 0x000000d1,	/* check sum */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000
};

static u32 pdSpdInfoReg2[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000083,	/* type */
	0x1A4, 0x00000019,	/* length, 25 bytes */
	0x188, 0x000000cf,	/* check sum */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000002
};



static u32 pdAudioInfoReg[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000084,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x0000006a,	/* check sum */
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000007,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000
};

static u32 pdAudioInfoReg2[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000084,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x00000055,	/* check sum */
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000007,
	0x188, 0x00000001,
	0x188, 0x00000002,
	0x188, 0x00000003,
	0x188, 0x00000004,
	0x188, 0x00000005,
	0x188, 0x00000006
};


static u32 pdVendorSpecInfoReg[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000081,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x000000E2,	/* check sum */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000
};

static u32 pdVendorSpecInfoReg2[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000081,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x000000d2,	/* check sum */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000001,
	0x188, 0x00000002,
	0x188, 0x00000003,
	0x188, 0x00000004,
	0x188, 0x00000006,
	0x188, 0x00000000
};


static u32 pdGenericInfoReg[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000087,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x000000DC,	/* check sum */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000,
	0x188, 0x00000000
};

static u32 pdGenericInfoReg2[] = {
	0x19c, 0x00000001,	/* version */
	0x1A0, 0x00000087,	/* type */
	0x1A4, 0x0000000a,	/* length */
	0x188, 0x000000BA,	/* check sum */
	0x188, 0x00000041,
	0x188, 0x00000051,
	0x188, 0x00000000,
	0x188, 0x00000002,
	0x188, 0x00000010,
	0x188, 0x00000001,
	0x188, 0x00000002,
	0x188, 0x00000003,
	0x188, 0x00000004,
	0x188, 0x00000006
};


static u32 pdACPInfoReg[] = {
	0x19c, 0x00000002,	/* version, acp type */
	0x1A0, 0x00000004,	/* type */
	0x1A4, 0x00000000,	/* length */
	0x188, 0x000000dE,	/* check sum, PB0 */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002
};

static u32 pdACPInfoReg2[] = {
	0x19c, 0x00000002,	/* version, acp type */
	0x1A0, 0x00000004,	/* type */
	0x1A4, 0x00000000,	/* length */
	0x188, 0x000000dE,	/* check sum, PB0 */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000001,
	0x188, 0x00000001
};


static u32 pdISRC1InfoReg[] = {
	0x19c, 0x00000002,	/* version, ISRC status */
	0x1A0, 0x00000005,	/* type */
	0x1A4, 0x00000000,	/* length */
	0x188, 0x000000dE,	/* check sum, PB0 */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002
};

static u32 pdISRC1InfoReg2[] = {
	0x19c, 0x00000002,	/* version, ISRC status */
	0x1A0, 0x00000005,	/* type */
	0x1A4, 0x00000000,	/* length */
	0x188, 0x000000d0,	/* check sum, PB0 */
	0x188, 0x00000041,
	0x188, 0x00000051,
	0x188, 0x00000003,
	0x188, 0x00000001
};


static u32 pdISRC2InfoReg[] = {
	0x19c, 0x00000000,	/* version, ISRC status */
	0x1A0, 0x00000006,	/* type */
	0x1A4, 0x00000000,	/* length */
	0x188, 0x000000dE,	/* check sum, PB0 */
	0x188, 0x00000040,
	0x188, 0x00000050,
	0x188, 0x00000000,
	0x188, 0x00000002
};

static u32 pdISRC2InfoReg2[] = {
	0x19c, 0x00000000,	/* version, ISRC status */
	0x1A0, 0x00000006,	/* type */
	0x1A4, 0x00000000,	/* length */
	0x188, 0x000000d6,	/* check sum, PB0 */
	0x188, 0x00000042,
	0x188, 0x00000052,
	0x188, 0x00000002,
	0x188, 0x00000004
};


static u32 pdGamutInfoReg[] = {
	0x19c, 0x00000080,	/* HB1 version */
	0x1A0, 0x0000000a,	/* HB0 type */
	0x1A4, 0x00000030,	/* HB2 */
	0x188, 0x00000000	/* PB0 */
};

static u32 pdGamutInfoReg2[] = {
	0x19c, 0x00000080,	/* HB1 version */
	0x1A0, 0x0000000a,	/* HB0 type */
	0x1A4, 0x00000030,	/* HB2 */
	0x188, 0x00000001	/* PB0 */
};

void mhl_InfoframeSetting(u8 i1typemode, u8 i1typeselect)
{
	u32 u4Ind;
	u8 bData;

	if ((i1typemode == 0xff) && (i1typeselect == 0xff)) {
		printk("Arg1: Infoframe output type\n");
		printk("1: AVi, 2: Mpeg, 3: SPD\n");
		printk("4: Vendor, 5: Audio, 6: ACP\n");
		printk("7: ISRC1, 8: ISRC2,  9:GENERIC\n");
		printk("10:GAMUT\n");

		printk("Arg2: Infoframe data select\n");
		printk("0: old(default), 1: new;\n");
		return;
	}

	if (i1typeselect == 0) {
		switch (i1typemode) {
		case 1:	/* Avi */

			vSendAVIInfoFrame(HDMI_VIDEO_1280x720p_50Hz, 1);
			mdelay(50);
			/* break; */

		case 2:	/* Mpeg */
			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 4));
			for (u4Ind = 0; u4Ind < (sizeof(pdMpegInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdMpegInfoReg[u4Ind], pdMpegInfoReg[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 4), (1 << 4));
			mdelay(50);
			/* break; */

		case 3:
			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 3));
			for (u4Ind = 0; u4Ind < (sizeof(pdSpdInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdSpdInfoReg[u4Ind], pdSpdInfoReg[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 3), (1 << 3));
			/* break; */
			mdelay(50);

		case 4:
			/* Vendor spec */
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 0));
			for (u4Ind = 0; u4Ind < (sizeof(pdVendorSpecInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdVendorSpecInfoReg[u4Ind],
						  pdVendorSpecInfoReg[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 0), (1 << 0));
			/* break; */
			mdelay(50);

		case 5:
			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 5));
			for (u4Ind = 0; u4Ind < (sizeof(pdAudioInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdAudioInfoReg[u4Ind], pdAudioInfoReg[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 5), (1 << 5));
			mdelay(50);
			/* break; */

			/* ACP */
		case 6:
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 1));
			for (u4Ind = 0; u4Ind < (sizeof(pdACPInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdACPInfoReg[u4Ind], pdACPInfoReg[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 23; u4Ind++)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 1), (1 << 1));
			/* break; */
			mdelay(50);
			/* ISCR1 */
		case 7:
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 2));
			for (u4Ind = 0; u4Ind < (sizeof(pdISRC1InfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdISRC1InfoReg[u4Ind], pdISRC1InfoReg[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 23; u4Ind++)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 2), (1 << 2));
			/* break; */
			mdelay(50);
		case 8:
			/* ISCR2 */
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 3));
			for (u4Ind = 0; u4Ind < (sizeof(pdISRC2InfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdISRC2InfoReg[u4Ind], pdISRC2InfoReg[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 23; u4Ind++)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 3), (1 << 3));
			/* break; */
			mdelay(50);
		case 9:
			/* Generic spec */

			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 2));
			for (u4Ind = 0; u4Ind < (sizeof(pdGenericInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdGenericInfoReg[u4Ind],
						  pdGenericInfoReg[u4Ind + 1]);

			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 2), (1 << 2));
			/* break; */
			mdelay(50);
		case 10:

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 4));
			for (u4Ind = 0; u4Ind < (sizeof(pdGamutInfoReg) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdGamutInfoReg[u4Ind], pdGamutInfoReg[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 4), (1 << 4));

			/* vSendAVIInfoFrame(HDMI_VIDEO_720x480p_60Hz,  HDMI_XV_YCC); */

			break;


		case 11:
			bData = bReadByteHdmiGRL(GRL_CTRL);
			bData &= ~(0x1 << 7);
			vWriteByteHdmiGRL(GRL_CTRL, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData |= (0x1 << 7);

			vWriteByteHdmiGRL(GRL_CTRL, bData);
			break;
			/* c : unmute */
		case 12:
			bData = bReadByteHdmiGRL(GRL_CFG4);
			bData |= (0x1 << 5);	/* disable original mute */
			bData &= ~(0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData &= ~(0x1 << 5);	/* disable original mute */
			bData |= (0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);
			break;
			/* d mute */
		case 13:

			bData = bReadByteHdmiGRL(GRL_CFG4);
			bData &= ~(0x1 << 5);	/* enable original mute */
			bData &= ~(0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);

			bData = bReadByteHdmiGRL(GRL_CTRL);
			bData &= ~(0x1 << 7);
			vWriteByteHdmiGRL(GRL_CTRL, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData |= (0x1 << 7);

			vWriteByteHdmiGRL(GRL_CTRL, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData &= ~(0x1 << 7);
			vWriteByteHdmiGRL(GRL_CTRL, bData);

			break;
		case 14:
			bData = bReadByteHdmiGRL(GRL_CFG4);
			bData |= (0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);
			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);
			bData &= ~(0x1 << 6);
			vWriteByteHdmiGRL(GRL_CFG4, bData);

			break;
		case 15:
			/* vCMDHwNCTSOnOff(FALSE);// change to software NCTS; */
			/* vCMDHDMI_NCTS(0x03, 0x12); */

			break;
		default:
			break;

		}
	} else {
		switch (i1typemode) {
		case 1:	/* Avi */

			vSendAVIInfoFrame(HDMI_VIDEO_1280x720p_50Hz, 2);

			break;

		case 2:	/* Mpeg */
			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 4));
			for (u4Ind = 0; u4Ind < (sizeof(pdMpegInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdMpegInfoReg2[u4Ind], pdMpegInfoReg2[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 4), (1 << 4));

			break;

		case 3:
			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 3));
			for (u4Ind = 0; u4Ind < (sizeof(pdSpdInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdSpdInfoReg2[u4Ind], pdSpdInfoReg2[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 3), (1 << 3));
			break;


		case 4:
			/* Vendor spec */
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 0));
			for (u4Ind = 0; u4Ind < (sizeof(pdVendorSpecInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdVendorSpecInfoReg2[u4Ind],
						  pdVendorSpecInfoReg2[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 0), (1 << 0));
			break;


		case 5:
			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 5));
			for (u4Ind = 0; u4Ind < (sizeof(pdAudioInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdAudioInfoReg2[u4Ind],
						  pdAudioInfoReg2[u4Ind + 1]);
			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 5), (1 << 5));

			break;


			/* ACP */
		case 6:
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 1));
			for (u4Ind = 0; u4Ind < (sizeof(pdACPInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdACPInfoReg2[u4Ind], pdACPInfoReg2[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 23; u4Ind++)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 1), (1 << 1));
			break;

			/* ISCR1 */
		case 7:
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 2));
			for (u4Ind = 0; u4Ind < (sizeof(pdISRC1InfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdISRC1InfoReg2[u4Ind],
						  pdISRC1InfoReg2[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 23; u4Ind++)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 2), (1 << 2));
			break;

		case 8:
			/* ISCR2 */
			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 3));
			for (u4Ind = 0; u4Ind < (sizeof(pdISRC2InfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdISRC2InfoReg2[u4Ind],
						  pdISRC2InfoReg2[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 23; u4Ind++)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 3), (1 << 3));
			break;

		case 9:
			/* Generic spec */

			vWriteHdmiGRLMsk(GRL_CTRL, 0, (1 << 2));
			for (u4Ind = 0; u4Ind < (sizeof(pdGenericInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdGenericInfoReg2[u4Ind],
						  pdGenericInfoReg2[u4Ind + 1]);

			vWriteHdmiGRLMsk(GRL_CTRL, (1 << 2), (1 << 2));
			break;

		case 10:

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, (1 << 4));
			for (u4Ind = 0; u4Ind < (sizeof(pdGamutInfoReg2) / 4); u4Ind += 2)
				vWriteByteHdmiGRL(pdGamutInfoReg2[u4Ind],
						  pdGamutInfoReg2[u4Ind + 1]);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1)
				vWriteByteHdmiGRL(GRL_IFM_PORT, 0);

			vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, (1 << 4), (1 << 4));

			vSendAVIInfoFrame(HDMI_VIDEO_720x480p_60Hz, HDMI_XV_YCC);

			break;


		case 11:
			bData = bReadByteHdmiGRL(GRL_CTRL);
			bData &= ~(0x1 << 7);
			vWriteByteHdmiGRL(GRL_CTRL, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData |= (0x1 << 7);

			vWriteByteHdmiGRL(GRL_CTRL, bData);
			break;

		case 12:
			bData = bReadByteHdmiGRL(GRL_CFG4);
			bData |= (0x1 << 5);	/* disable original mute */
			bData &= ~(0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData &= ~(0x1 << 5);	/* disable original mute */
			bData |= (0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);
			break;

		case 13:

			bData = bReadByteHdmiGRL(GRL_CFG4);
			bData &= ~(0x1 << 5);	/* enable original mute */
			bData &= ~(0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);

			bData = bReadByteHdmiGRL(GRL_CTRL);
			bData &= ~(0x1 << 7);
			vWriteByteHdmiGRL(GRL_CTRL, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData |= (0x1 << 7);

			vWriteByteHdmiGRL(GRL_CTRL, bData);

			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);

			bData &= ~(0x1 << 7);
			vWriteByteHdmiGRL(GRL_CTRL, bData);
			break;
		case 14:
			bData = bReadByteHdmiGRL(GRL_CFG4);

			bData |= (0x1 << 6);	/* disable */
			vWriteByteHdmiGRL(GRL_CFG4, bData);
			for (u4Ind = 0; u4Ind < 27; u4Ind += 1);
			bData &= ~(0x1 << 6);
			vWriteByteHdmiGRL(GRL_CFG4, bData);

			break;
		case 15:
			/* vCMDHwNCTSOnOff(FALSE);// change to software NCTS; */
			/* vCMDHDMI_NCTS(0x03, 0x12); */

			break;
		default:
			break;

		}

	}
}

void vMhlInit(void)
{

	vWriteHdmiSYSMsk(HDMI_SYS_CFG1, MHL_RISC_CLK_EN, MHL_RISC_CLK_EN);
	vMhlAnalogPD();
	vVideoPLLInit();
}
