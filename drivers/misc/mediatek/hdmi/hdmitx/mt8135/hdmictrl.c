#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT

#include "hdmictrl.h"
#include "hdmi_ctrl.h"
#include "hdmihdcp.h"
#include "hdmiedid.h"
#include "hdmicec.h"

#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
#include "hdmi_ca.h"
#endif

HDMI_AV_INFO_T _stAvdAVInfo = { 0 };

static unsigned char _bAudInfoFm[5];
static unsigned char _bAviInfoFm[5];
static unsigned char _bSpdInf[25] = { 0 };

static unsigned char _bAcpType;
static unsigned char _bAcpData[16] = { 0 };
static unsigned char _bIsrc1Data[16] = { 0 };

static unsigned int _u4NValue;
extern unsigned char _bflagvideomute;
extern unsigned char _bflagaudiomute;
extern unsigned char _bsvpvideomute;
extern unsigned char _bsvpaudiomute;

extern unsigned char _bHdcpOff;

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
	"RES_720p3d60Hz",
	"RES_720p3d50Hz",
	"RES_1080i3d60Hz",
	"RES_1080i3d50Hz",
	"RES_1080P3d24Hz",
	"RES_1080P3d23Hz",
	"VGA_VIDEO_1680x1050_60Hz",
	"VGA_VIDEO_1440x1050_60Hz",
	"VGA_VIDEO_1440x900_60Hz",
	"VGA_VIDEO_1360x768_60Hz",
	"VGA_VIDEO_1280x1024_60Hz",
	"VGA_VIDEO_1280x960_60Hz",
	"VGA_VIDEO_1280x800_60Hz",
	"VGA_VIDEO_1280x768_60Hz",
	"VGA_VIDEO_1024x768_60Hz",
	"VGA_VIDEO_848x480_60Hz",
	"VGA_VIDEO_640x480_60Hz",
	"VGA_VIDEO_800x600_60Hz",
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

#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
void ta_internal_hdmi_write(unsigned int u4Reg, unsigned int u4data)
{
	vCaHDMIWriteReg(u4Reg, u4data);
}
#endif
void internal_hdmi_read(unsigned int u4Reg, unsigned int *p4Data)
{
	*p4Data = (*(volatile unsigned int *)(u4Reg));
}

void internal_hdmi_write(unsigned int u4Reg, unsigned int u4data)
{
	*(volatile unsigned int *)(u4Reg) = (u4data);
}

/* //////////////////////////////////////////// */
unsigned int hdmi_drv_read(unsigned short u2Reg)
{
	unsigned int u4Data;
	internal_hdmi_read(HDMIDRV_BASE + u2Reg, &u4Data);
	HDMI_PLL_LOG("[R]addr = 0x%04x, data = 0x%08x\n", u2Reg, u4Data);
	return u4Data;
}

void hdmi_drv_write(unsigned short u2Reg, unsigned int u4Data)
{
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
	HDMI_PLL_LOG("[W]addr = 0x%04x, data = 0x%08x\n", u2Reg, u4Data);
	ta_internal_hdmi_write(HDMIDRV_BASE + u2Reg, u4Data);
#else
	HDMI_PLL_LOG("[W]addr = 0x%04x, data = 0x%08x\n", u2Reg, u4Data);
	internal_hdmi_write(HDMIDRV_BASE + u2Reg, u4Data);
#endif
}

/* /////////////////////////////////////////////// */
unsigned int hdmi_sys_read(unsigned short u2Reg)
{
	unsigned int u4Data;
	internal_hdmi_read(HDMISYS_BASE + u2Reg, &u4Data);
	HDMI_PLL_LOG("[R]addr = 0x%04x, data = 0x%08x\n", u2Reg, u4Data);
	return u4Data;
}

void hdmi_sys_write(unsigned short u2Reg, unsigned int u4Data)
{
	HDMI_PLL_LOG("[W]addr= 0x%04x, data = 0x%08x\n", u2Reg, u4Data);
	internal_hdmi_write(HDMISYS_BASE + u2Reg, u4Data);
}

/* /////////////////////////////////////////////////// */
unsigned int hdmi_hdmitopck_read(unsigned short u2Reg)
{
	unsigned int u4Data;
	internal_hdmi_read(HDMICKGEN_BASE + u2Reg, &u4Data);
	HDMI_PLL_LOG("[R]addr = 0x%04x, data = 0x%08x\n", u2Reg, u4Data);
	return u4Data;
}

void hdmi_hdmitopck_write(unsigned short u2Reg, unsigned int u4Data)
{
	HDMI_PLL_LOG("[W]addr= 0x%04x, data = 0x%08x\n", u2Reg, u4Data);
	internal_hdmi_write(HDMICKGEN_BASE + u2Reg, u4Data);

}

/* ///////////////////////////////////////////////////// */
unsigned int hdmi_pll_read(unsigned short u2Reg)
{
	unsigned int u4Data;
	internal_hdmi_read(HDMIPLL_BASE + u2Reg, &u4Data);
	HDMI_PLL_LOG("[R]addr = 0x%04x, data = 0x%08x\n", u2Reg, u4Data);
	return u4Data;
}

void hdmi_pll_write(unsigned short u2Reg, unsigned int u4Data)
{
	HDMI_PLL_LOG("[W]addr= 0x%04x, data = 0x%08x\n", u2Reg, u4Data);
	internal_hdmi_write(HDMIPLL_BASE + u2Reg, u4Data);
}

/* ///////////////////////////////////////////////////// */

void vHotPlugPinInit(void)
{
	/* vWriteIoPadMsk(IO_PAD_PD, IO_PAD_HOT_PLUG_PD, IO_PAD_HOT_PLUG_PD); */
}

unsigned char fgIsAcpEnable(void)
{
	if (bReadByteHdmiGRL(GRL_ACP_ISRC_CTRL) & ACP_EN)
		return TRUE;
	else
		return FALSE;
}

unsigned char fgIsVSEnable(void)
{
	if (bReadByteHdmiGRL(GRL_ACP_ISRC_CTRL) & VS_EN)
		return TRUE;
	else
		return FALSE;
}

unsigned char fgIsISRC1Enable(void)
{
	if (bReadByteHdmiGRL(GRL_ACP_ISRC_CTRL) & ISRC1_EN)
		return TRUE;
	else
		return FALSE;
}


unsigned char fgIsISRC2Enable(void)
{
	if (bReadByteHdmiGRL(GRL_ACP_ISRC_CTRL) & ISRC2_EN)
		return TRUE;
	else
		return FALSE;
}

unsigned char vIsDviMode(void)
{
	unsigned char bData;

	bData = bReadByteHdmiGRL(GRL_CFG1);
	if (bData & CFG1_DVI)
		return TRUE;
	else
		return FALSE;
}

unsigned char bCheckStatus(unsigned char bMode)
{
	unsigned char bStatus = 0;

	bStatus = bReadByteHdmiGRL(GRL_STATUS);

	if ((STATUS_HTPLG & bMode) == STATUS_HTPLG) {
		bStatus = IS_HDMI_HTPLG() ? (bStatus | STATUS_HTPLG) : (bStatus & (~STATUS_HTPLG));
	}

	HDMI_PLL_LOG("IS_HDMI_HTPLG=0x%x, bStatus=0x%x\n", IS_HDMI_HTPLG(), bStatus);

	if ((bStatus & bMode) == bMode) {
		return TRUE;
	} else {
		return FALSE;
	}
}

unsigned char bCheckPordHotPlug(unsigned char bMode)
{
	unsigned char bStatus = FALSE;
	if (bMode == (PORD_MODE | HOTPLUG_MODE)) {
		bStatus = bCheckStatus(STATUS_PORD | STATUS_HTPLG);
	} else if (bMode == HOTPLUG_MODE) {
		bStatus = bCheckStatus(STATUS_HTPLG);
	} else if (bMode == PORD_MODE) {
		bStatus = bCheckStatus(STATUS_PORD);
	}

	return bStatus;
}

void MuteHDMIAudio(void)
{

#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
	HDMI_AUDIO_FUNC();

	fgCaHDMIAudioUnMute(FALSE);
#else
	unsigned char bData;
	HDMI_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
	bData |= AUDIO_ZERO;
	vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
#endif

}

void vBlackHDMIOnly(void)
{
	HDMI_DRV_FUNC();
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
	fgCaHDMIVideoUnMute(FALSE);
#else
	*(unsigned int *)(0xf400f0b4) = 0x51;
#endif
}

void vHDMIAVMute(void)
{
	HDMI_AUDIO_FUNC();

	vBlackHDMIOnly();
	MuteHDMIAudio();
}

void vTxSignalOnOff(unsigned char bOn)
{
	HDMI_PLL_FUNC();

	if (bOn) {
		vWriteIoPllMsk(HDMI_CON7, RG_HTPLL_AUTOK_EN, RG_HTPLL_AUTOK_EN);
		vWriteIoPllMsk(HDMI_CON6, 0, RG_HTPLL_RLH_EN);
		vWriteIoPllMsk(HDMI_CON6, (0x3 << RG_HTPLL_POSDIV), RG_HTPLL_POSDIV_MASK);
		vWriteIoPllMsk(HDMI_CON2, RG_HDMITX_EN_MBIAS, RG_HDMITX_EN_MBIAS);
		udelay(3);
		vWriteIoPllMsk(HDMI_CON6, RG_HTPLL_EN, RG_HTPLL_EN);
		vWriteIoPllMsk(HDMI_CON2, RG_HDMITX_EN_TX_CKLDO, RG_HDMITX_EN_TX_CKLDO);
		vWriteIoPllMsk(HDMI_CON0, (0xf << RG_HDMITX_EN_SLDO), RG_HDMITX_EN_SLDO_MASK);
		udelay(200);
		vWriteIoPllMsk(HDMI_CON2, RG_HDMITX_MBIAS_LPF_EN, RG_HDMITX_MBIAS_LPF_EN);
		vWriteIoPllMsk(HDMI_CON2, RG_HDMITX_EN_TX_POSDIV, RG_HDMITX_EN_TX_POSDIV);
		vWriteIoPllMsk(HDMI_CON0, (0xf << RG_HDMITX_EN_SER), RG_HDMITX_EN_SER_MASK);
		vWriteIoPllMsk(HDMI_CON0, (0xf << RG_HDMITX_EN_PRED), RG_HDMITX_EN_PRED_MASK);
		vWriteIoPllMsk(HDMI_CON0, (0xf << RG_HDMITX_EN_DRV), RG_HDMITX_EN_DRV_MASK);
		udelay(3);
	} else {
		vWriteIoPllMsk(HDMI_CON0, 0, RG_HDMITX_EN_DRV_MASK);
		vWriteIoPllMsk(HDMI_CON0, 0, RG_HDMITX_EN_PRED_MASK);
		vWriteIoPllMsk(HDMI_CON0, 0, RG_HDMITX_EN_SER_MASK);
		vWriteIoPllMsk(HDMI_CON2, 0, RG_HDMITX_EN_TX_POSDIV);
		vWriteIoPllMsk(HDMI_CON2, 0, RG_HDMITX_MBIAS_LPF_EN);
		udelay(200);
		vWriteIoPllMsk(HDMI_CON0, 0, RG_HDMITX_EN_SLDO_MASK);
		vWriteIoPllMsk(HDMI_CON2, 0, RG_HDMITX_EN_TX_CKLDO);
		vWriteIoPllMsk(HDMI_CON6, 0, RG_HTPLL_EN);
		udelay(3);
		vWriteIoPllMsk(HDMI_CON2, 0, RG_HDMITX_EN_MBIAS);
		vWriteIoPllMsk(HDMI_CON6, 0, RG_HTPLL_POSDIV_MASK);
		vWriteIoPllMsk(HDMI_CON6, 0, RG_HTPLL_RLH_EN);
		vWriteIoPllMsk(HDMI_CON7, 0, RG_HTPLL_AUTOK_EN);
		udelay(3);
	}
}

void vSetCTL0BeZero(unsigned char fgBeZero)
{
	unsigned char bTemp;

	HDMI_VIDEO_FUNC();

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

void vWriteHdmiIntMask(unsigned char bMask)
{
	HDMI_DRV_FUNC();

	vWriteByteHdmiGRL(GRL_INT_MASK, bMask);	/* INT mask */

}

void vUnBlackHDMIOnly(void)
{
	HDMI_DRV_FUNC();
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
	fgCaHDMIVideoUnMute(TRUE);
#else
	*(unsigned int *)(0xf400f0b4) = 0x0;
#endif
}

void UnMuteHDMIAudio(void)
{
#if (defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) && defined(CONFIG_MTK_HDMI_HDCP_SUPPORT))
	HDMI_AUDIO_FUNC();

	fgCaHDMIAudioUnMute(TRUE);
#else
	BYTE bData;
	HDMI_AUDIO_FUNC();

	bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
	bData &= ~AUDIO_ZERO;
	vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);
#endif
}

void vTmdsOnOffAndResetHdcp(unsigned char fgHdmiTmdsEnable)
{
	HDMI_DRV_FUNC();

	if (fgHdmiTmdsEnable == 1) {
		mdelay(1);
		vTxSignalOnOff(SV_ON);
	} else {
		vHDMIAVMute();
		mdelay(1);
		vTxSignalOnOff(SV_OFF);
		vHDCPReset();
		mdelay(1);
	}
}

void vSetHDMITxPLL(unsigned char bResIndex, unsigned char bdeepmode)
{
	unsigned char u4Feq = 0;
	unsigned int v4value1 = 0;
	unsigned int v4value2 = 0;

	HDMI_PLL_FUNC();

	if ((bResIndex == HDMI_VIDEO_720x480p_60Hz)
		|| (bResIndex == HDMI_VIDEO_720x576p_50Hz)
		|| (bResIndex == VGA_VIDEO_640x480_60Hz)
		|| (bResIndex == VGA_VIDEO_848x480_60Hz)
		|| (bResIndex == VGA_VIDEO_800x600_60Hz))
		u4Feq = 0;	/* 27M */
	else if ((bResIndex == HDMI_VIDEO_1920x1080p_60Hz)
		 || (bResIndex == HDMI_VIDEO_1920x1080p_50Hz)
		 || (bResIndex == HDMI_VIDEO_1280x720p3d_60Hz)
		 || (bResIndex == HDMI_VIDEO_1280x720p3d_50Hz)
		 || (bResIndex == HDMI_VIDEO_1920x1080i3d_60Hz)
		 || (bResIndex == HDMI_VIDEO_1920x1080i3d_50Hz)
		 || (bResIndex == HDMI_VIDEO_1920x1080p3d_24Hz)
		 || (bResIndex == HDMI_VIDEO_1920x1080p3d_23Hz)
		 || (bResIndex == VGA_VIDEO_1680x1050_60Hz)
		 || (bResIndex == VGA_VIDEO_1440x1050_60Hz)
		 || (bResIndex == VGA_VIDEO_1440x900_60Hz)
		 || (bResIndex == VGA_VIDEO_1280x1024_60Hz)
		 || (bResIndex == VGA_VIDEO_1280x960_60Hz))
		u4Feq = 2;	/* 148M */
	else
		u4Feq = 1;	/* 74M */

	vWriteIoPllMsk(HDMI_CON6, (0x3 << RG_HTPLL_PREDIV), RG_HTPLL_PREDIV_MASK);
	vWriteIoPllMsk(HDMI_CON6, (0x3 << RG_HTPLL_POSDIV), RG_HTPLL_POSDIV_MASK);
	vWriteIoPllMsk(HDMI_CON6, (0x1 << RG_HTPLL_IC), RG_HTPLL_IC_MASK);
	vWriteIoPllMsk(HDMI_CON6, (0x1 << RG_HTPLL_IR), RG_HTPLL_IR_MASK);
	vWriteIoPllMsk(HDMI_CON2, ((POSDIV[u4Feq][bdeepmode - 1]) << RG_HDMITX_TX_POSDIV),
		       RG_HDMITX_TX_POSDIV_MASK);
	vWriteIoPllMsk(HDMI_CON6, ((FBKSEL[u4Feq][bdeepmode - 1]) << RG_HTPLL_FBKSEL),
		       RG_HTPLL_FBKSEL_MASK);
	vWriteIoPllMsk(HDMI_CON6, ((FBKDIV[u4Feq][bdeepmode - 1]) << RG_HTPLL_FBKDIV),
		       RG_HTPLL_FBKDIV_MASK);
	vWriteIoPllMsk(HDMI_CON7, ((DIVEN[u4Feq][bdeepmode - 1]) << RG_HTPLL_DIVEN),
		       RG_HTPLL_DIVEN_MASK);
	vWriteIoPllMsk(HDMI_CON6, ((HTPLLBP[u4Feq][bdeepmode - 1]) << RG_HTPLL_BP),
		       RG_HTPLL_BP_MASK);
	vWriteIoPllMsk(HDMI_CON6, ((HTPLLBC[u4Feq][bdeepmode - 1]) << RG_HTPLL_BC),
		       RG_HTPLL_BC_MASK);
	vWriteIoPllMsk(HDMI_CON6, ((HTPLLBR[u4Feq][bdeepmode - 1]) << RG_HTPLL_BR),
		       RG_HTPLL_BR_MASK);

	v4value1 = (dReadHdmiTOPCK(0x9170) & 0xf8000000) >> 26;
	v4value2 = dReadHdmiTOPCK(0x91c0) & 0x003fffff;
	if (v4value1 == 0)
		v4value1 = 0x1c;
	if (v4value2 == 0)
		v4value2 = 0x1c1c1c;

	if ((u4Feq == 2) && (bdeepmode != HDMI_NO_DEEP_COLOR)) {
		vWriteIoPllMsk(HDMI_CON1, RG_HDMITX_PRED_IMP, RG_HDMITX_PRED_IMP);
		vWriteIoPllMsk(HDMI_CON1, (0x6 << RG_HDMITX_PRED_IBIAS), RG_HDMITX_PRED_IBIAS_MASK);
		vWriteIoPllMsk(HDMI_CON0, (0xf << RG_HDMITX_EN_IMP), RG_HDMITX_EN_IMP_MASK);
		vWriteIoPllMsk(HDMI_CON1, (v4value1 << RG_HDMITX_DRV_IMP), RG_HDMITX_DRV_IMP_MASK);
		vWriteIoPllMsk(HDMI_CON4, v4value2, RG_HDMITX_RESERVE_MASK);
		vWriteIoPllMsk(HDMI_CON0, (0x1b << RG_HDMITX_DRV_IBIAS), RG_HDMITX_DRV_IBIAS_MASK);
	} else {
		vWriteIoPllMsk(HDMI_CON1, 0, RG_HDMITX_PRED_IMP);
		vWriteIoPllMsk(HDMI_CON1, (0x3 << RG_HDMITX_PRED_IBIAS), RG_HDMITX_PRED_IBIAS_MASK);
		vWriteIoPllMsk(HDMI_CON0, (0x0 << RG_HDMITX_EN_IMP), RG_HDMITX_EN_IMP_MASK);
		vWriteIoPllMsk(HDMI_CON1, (v4value1 << RG_HDMITX_DRV_IMP), RG_HDMITX_DRV_IMP_MASK);
		vWriteIoPllMsk(HDMI_CON4, v4value2, RG_HDMITX_RESERVE_MASK);
		vWriteIoPllMsk(HDMI_CON0, (0xa << RG_HDMITX_DRV_IBIAS), RG_HDMITX_DRV_IBIAS_MASK);
	}

	/* power on sequence of hdmi */

}


void vEnableDeepColor(unsigned char ui1Mode)
{
	unsigned int u4Data;
	HDMI_DRV_FUNC();
	if (ui1Mode == HDMI_DEEP_COLOR_10_BIT) {
		u4Data = COLOR_10BIT_MODE;
	} else if (ui1Mode == HDMI_DEEP_COLOR_12_BIT) {
		u4Data = COLOR_12BIT_MODE;
	} else if (ui1Mode == HDMI_DEEP_COLOR_16_BIT) {
		u4Data = COLOR_16BIT_MODE;
	} else {
		u4Data = COLOR_8BIT_MODE;
	}

	if (u4Data == COLOR_8BIT_MODE) {
		vWriteHdmiSYSMsk(HDMI_SYS_CFG20, u4Data, DEEP_COLOR_MODE_MASK | DEEP_COLOR_EN);
	} else {
		vWriteHdmiSYSMsk(HDMI_SYS_CFG20, u4Data | DEEP_COLOR_EN,
				 DEEP_COLOR_MODE_MASK | DEEP_COLOR_EN);
	}

}

void vConfigHdmiSYS(unsigned char bResIndex)
{
	unsigned char u4Feq = 0;
	HDMI_PLL_FUNC();

	if ((bResIndex == HDMI_VIDEO_720x480p_60Hz)
		|| (bResIndex == HDMI_VIDEO_720x576p_50Hz)
		|| (bResIndex == VGA_VIDEO_640x480_60Hz)
		|| (bResIndex == VGA_VIDEO_848x480_60Hz)
		|| (bResIndex == VGA_VIDEO_800x600_60Hz))
		u4Feq = 0;	/* 27M */
	else if ((bResIndex == HDMI_VIDEO_1920x1080p_60Hz)
		 || (bResIndex == HDMI_VIDEO_1920x1080p_50Hz)
		 || (bResIndex == HDMI_VIDEO_1280x720p3d_60Hz)
		 || (bResIndex == HDMI_VIDEO_1280x720p3d_50Hz)
		 || (bResIndex == HDMI_VIDEO_1920x1080i3d_60Hz)
		 || (bResIndex == HDMI_VIDEO_1920x1080i3d_50Hz)
		 || (bResIndex == HDMI_VIDEO_1920x1080p3d_24Hz)
		 || (bResIndex == HDMI_VIDEO_1920x1080p3d_23Hz)
		 || (bResIndex == VGA_VIDEO_1680x1050_60Hz)
		 || (bResIndex == VGA_VIDEO_1440x1050_60Hz)
		 || (bResIndex == VGA_VIDEO_1440x900_60Hz)
		 || (bResIndex == VGA_VIDEO_1280x1024_60Hz)
		 || (bResIndex == VGA_VIDEO_1280x960_60Hz))
		u4Feq = 2;	/* 148M */
	else
		u4Feq = 1;	/* 74M */

	vWriteHdmiTOPCKMsk(HDMICLK_CFG_9, 0, CLK_DPI1_SEL | CLK_HDMIPLL_SEL);

	vWriteHdmiSYSMsk(HDMI_SYS_CFG110, ENABLE_DPICLK | ENABLE_IDCLK | ENABLE_PLLCLK,
			 DISABLE_DPICLK | DISABLE_IDCLK | DISABLE_PLLCLK);

	if (_stAvdAVInfo.e_hdmi_aud_in == SV_SPDIF)
		vWriteHdmiSYSMsk(HDMI_SYS_CFG110, ENABLE_SPDIFCLK, DISABLE_SPDIFCLK | DISABLE_BCLK);
	else
		vWriteHdmiSYSMsk(HDMI_SYS_CFG110, ENABLE_SPDIFCLK | ENABLE_BCLK,
				 DISABLE_SPDIFCLK | DISABLE_BCLK);

	vWriteHdmiSYSMsk(HDMI_SYS_CFG20, 0, HDMI_OUT_FIFO_EN | MHL_MODE_ON);
	udelay(100);
	vWriteHdmiSYSMsk(HDMI_SYS_CFG20, HDMI_OUT_FIFO_EN, HDMI_OUT_FIFO_EN | MHL_MODE_ON);


	if (u4Feq == 2)
		vWriteHdmiTOPCKMsk(HDMICLK_CFG_9, DPI1_H_CK | HDMIPLL_CTS,
				   CLK_DPI1_SEL | CLK_HDMIPLL_SEL);
	else if (u4Feq == 1)
		vWriteHdmiTOPCKMsk(HDMICLK_CFG_9, DPI1_D2 | HDMIPLL_CTS,
				   CLK_DPI1_SEL | CLK_HDMIPLL_SEL);
	else if (u4Feq == 0)
		vWriteHdmiTOPCKMsk(HDMICLK_CFG_9, DPI1_D4 | HDMIPLL_CTS,
				   CLK_DPI1_SEL | CLK_HDMIPLL_SEL);

	/* vWriteHdmiTOPCKMsk(HDMICLK_CFG_9, AD_APLL_CK, CLK_APLL_SEL); */
}

void vChangeVpll(unsigned char bRes, unsigned char bdeepmode)
{
	HDMI_PLL_FUNC();
	vSetHDMITxPLL(bRes, bdeepmode);	/* set PLL */
	vTxSignalOnOff(SV_ON);
	vConfigHdmiSYS(bRes);
	vEnableDeepColor(bdeepmode);
}

void vResetHDMI(BYTE bRst)
{
	HDMI_DRV_FUNC();
	if (bRst) {
		vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, HDMI_RST, HDMI_RST);
	} else {
		vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, 0, HDMI_RST);
		vWriteHdmiGRLMsk(GRL_CFG3, 0, CFG3_CONTROL_PACKET_DELAY);	/* Designer suggest adjust Control packet deliver time */
		vWriteHdmiSYSMsk(HDMI_SYS_CFG1C, ANLG_ON, ANLG_ON);
	}
}

void vHDMIAVUnMute(void)
{
	HDMI_AUDIO_FUNC();
	if ((_bflagvideomute == FALSE) && (_bsvpvideomute == FALSE))
		vUnBlackHDMIOnly();
	if ((_bflagaudiomute == FALSE) && (_bsvpaudiomute == FALSE))
		UnMuteHDMIAudio();
}


void vEnableNotice(unsigned char bOn)
{
	unsigned char bData;
	HDMI_VIDEO_FUNC();
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

void vEnableHdmiMode(unsigned char bOn)
{
	unsigned char bData;
	HDMI_DRV_FUNC();
	if (bOn == TRUE) {
		bData = bReadByteHdmiGRL(GRL_CFG1);
		bData &= ~CFG1_DVI;	/* enable HDMI mode */
		vWriteByteHdmiGRL(GRL_CFG1, bData);
	} else {
		bData = bReadByteHdmiGRL(GRL_CFG1);
		bData |= CFG1_DVI;	/* disable HDMI mode */
		vWriteByteHdmiGRL(GRL_CFG1, bData);
	}

}

void vEnableNCTSAutoWrite(void)
{
	unsigned char bData;
	HDMI_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_DIVN);
	bData |= NCTS_WRI_ANYTIME;	/* enabel N-CTS can be written in any time */
	vWriteByteHdmiGRL(GRL_DIVN, bData);

}


void vHDMIResetGenReg(unsigned char ui1resindex, unsigned char ui1colorspace)
{
	HDMI_DRV_FUNC();
	vResetHDMI(1);
	vResetHDMI(0);
	vEnableNotice(TRUE);
	vWriteHdmiIntMask(0xff);

	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == TRUE)
		vEnableHdmiMode(TRUE);
	else
		vEnableHdmiMode(FALSE);

	vEnableNCTSAutoWrite();

	vWriteHdmiGRLMsk(GRL_CFG4, 0, CFG_MHL_MODE);

	if ((ui1resindex == HDMI_VIDEO_1920x1080i_50Hz)
	    || (ui1resindex == HDMI_VIDEO_1920x1080i_60Hz))
		vWriteHdmiGRLMsk(GRL_CFG2, 0, MHL_DE_SEL);
	else
		vWriteHdmiGRLMsk(GRL_CFG2, MHL_DE_SEL, MHL_DE_SEL);
}

void vAudioPacketOff(unsigned char bOn)
{
	unsigned char bData;
	HDMI_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_SHIFT_R2);
	if (bOn)
		bData |= 0x40;
	else
		bData &= ~0x40;

	vWriteByteHdmiGRL(GRL_SHIFT_R2, bData);
}

void vSetChannelSwap(unsigned char u1SwapBit)
{
	HDMI_AUDIO_FUNC();
	vWriteHdmiGRLMsk(GRL_CH_SWAP, u1SwapBit, 0xff);
}

void vEnableIecTxRaw(void)
{
	unsigned char bData;
	HDMI_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_MIX_CTRL);
	bData |= MIX_CTRL_FLAT;
	vWriteByteHdmiGRL(GRL_MIX_CTRL, bData);
}

void vSetHdmiI2SDataFmt(unsigned char bFmt)
{
	unsigned char bData;
	HDMI_AUDIO_FUNC();
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
	HDMI_AUDIO_FUNC();
	vWriteByteHdmiGRL(GRL_AOUT_BNUM_SEL, bBitNum);

}

void vSetHdmiHighBitrate(unsigned char fgHighBitRate)
{
	unsigned char bData;
	HDMI_AUDIO_FUNC();
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

void vDSTNormalDouble(unsigned char fgEnable)
{
	unsigned char bData;
	HDMI_AUDIO_FUNC();
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

void vEnableDSTConfig(unsigned char fgEnable)
{
	unsigned char bData;
	HDMI_AUDIO_FUNC();
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
	unsigned char bData;
	HDMI_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
	bData &= ~SACD_SEL;
	vWriteByteHdmiGRL(GRL_AUDIO_CFG, bData);

}

void vSetHdmiI2SChNum(unsigned char bChNum, unsigned char bChMapping)
{
	unsigned char bData, bData1, bData2, bData3;
	HDMI_AUDIO_FUNC();
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

void vSetHdmiIecI2s(unsigned char bIn)
{
	unsigned char bData;
	HDMI_AUDIO_FUNC();
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

unsigned char vIsDsdBitEnable(void)
{
	unsigned char bData;

	bData = bReadByteHdmiGRL(GRL_AUDIO_CFG);
	if (bData & SACD_SEL)
		return TRUE;
	else
		return FALSE;

}

void vUpdateAudSrcChType(unsigned char u1SrcChType)
{
	_stAvdAVInfo.ui2_aud_out_ch.word = 0;

	switch (u1SrcChType) {
	case AUD_INPUT_1_0:	/* TYPE_MONO */
	case AUD_INPUT_2_0:	// STEREO /* 2/0 */
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 0;
		_stAvdAVInfo.ui1_aud_out_ch_number = 2;
		break;

	case AUD_INPUT_1_1:
	case AUD_INPUT_2_1:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 3;
		break;

	case AUD_INPUT_3_0:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 0;
		_stAvdAVInfo.ui1_aud_out_ch_number = 3;
		break;

	case AUD_INPUT_3_0_LRS:	/* L,R, RC -> L,R, RR, RL */
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 0;
		_stAvdAVInfo.ui1_aud_out_ch_number = 4;
		break;

	case AUD_INPUT_3_1_LRS:	/* L, R, RC, LFE -> expand S to RR,RL to be  L, R, RR, RL, LFE */
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 0;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 5;
		break;

	case AUD_INPUT_4_0_CLRS:	/* L, R, FC, RC -> expand S to RR,RL to be  L, R, FC, RR, RL */
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 0;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 5;
		break;

	case AUD_INPUT_4_1_CLRS:	/* L, R, FC, RC, LFE -> expand S to RR,RL to be  L, R, FC, RR, RL, LFE */
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 6;
		break;

	case AUD_INPUT_3_1:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 4;
		break;

	case AUD_INPUT_4_0:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 0;
		_stAvdAVInfo.ui1_aud_out_ch_number = 4;
		break;

	case AUD_INPUT_4_1:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 5;
		break;

	case AUD_INPUT_5_0:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 0;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 5;
		break;

	case AUD_INPUT_5_1:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 6;
		break;

	case AUD_INPUT_6_0:	/* 6/0 */
	case AUD_INPUT_6_0_Cs:
	case AUD_INPUT_6_0_Ch:
	case AUD_INPUT_6_0_Oh:
	case AUD_INPUT_6_0_Chr:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 0;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RC = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 6;
		break;

	case AUD_INPUT_6_1:	/* 6/1 */
	case AUD_INPUT_6_1_Cs:	/* 6/1 */
	case AUD_INPUT_6_1_Ch:
	case AUD_INPUT_6_1_Oh:
	case AUD_INPUT_6_1_Chr:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RC = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 7;
		break;

	case AUD_INPUT_7_0:	/* 5/2 */
	case AUD_INPUT_7_0_Lh_Rh:
	case AUD_INPUT_7_0_Lsr_Rsr:
	case AUD_INPUT_7_0_Lc_Rc:
	case AUD_INPUT_7_0_Lw_Rw:
	case AUD_INPUT_7_0_Lsd_Rsd:	/* Dolby new type. -- Water 20091102 */
	case AUD_INPUT_7_0_Lss_Rss:
	case AUD_INPUT_7_0_Lhs_Rhs:
	case AUD_INPUT_7_0_Cs_Ch:
	case AUD_INPUT_7_0_Cs_Oh:
	case AUD_INPUT_7_0_Cs_Chr:
	case AUD_INPUT_7_0_Ch_Oh:
	case AUD_INPUT_7_0_Ch_Chr:
	case AUD_INPUT_7_0_Oh_Chr:
	case AUD_INPUT_7_0_Lss_Rss_Lsr_Rsr:
	case AUD_INPUT_8_0_Lh_Rh_Cs:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 0;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RRC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RLC = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 7;
		break;

	case AUD_INPUT_7_1:	/* 5/2 */
	case AUD_INPUT_7_1_Lh_Rh:
	case AUD_INPUT_7_1_Lsr_Rsr:
	case AUD_INPUT_7_1_Lc_Rc:
	case AUD_INPUT_7_1_Lw_Rw:
	case AUD_INPUT_7_1_Lsd_Rsd:	/* Dolby new type. -- Water 20091102 */
	case AUD_INPUT_7_1_Lss_Rss:
	case AUD_INPUT_7_1_Lhs_Rhs:
	case AUD_INPUT_7_1_Cs_Ch:
	case AUD_INPUT_7_1_Cs_Oh:
	case AUD_INPUT_7_1_Cs_Chr:
	case AUD_INPUT_7_1_Ch_Oh:
	case AUD_INPUT_7_1_Ch_Chr:
	case AUD_INPUT_7_1_Oh_Chr:
	case AUD_INPUT_7_1_Lss_Rss_Lsr_Rsr:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.LFE = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RL = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RRC = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.RLC = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 8;
		break;

	default:
		_stAvdAVInfo.ui2_aud_out_ch.bit.FR = 1;
		_stAvdAVInfo.ui2_aud_out_ch.bit.FL = 1;
		_stAvdAVInfo.ui1_aud_out_ch_number = 2;
		break;
	}

}

unsigned char bGetChannelMapping(void)
{
	unsigned char bChannelMap = 0x00;

	vUpdateAudSrcChType(_stAvdAVInfo.u1Aud_Input_Chan_Cnt);

	switch (_stAvdAVInfo.ui1_aud_out_ch_number) {
	case 8:

		break;

	case 7:

		break;

	case 6:
		if ((_stAvdAVInfo.ui2_aud_out_ch.bit.FR == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.FL == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.FC == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.RR == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.RL == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.RC == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.LFE == 0)
		    ) {
			bChannelMap = 0x0E;	/* 6.0 */
		} else if ((_stAvdAVInfo.ui2_aud_out_ch.bit.FR == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.FL == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.FC == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.RR == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.RL == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.RC == 0) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.LFE == 1)
		    ) {
			bChannelMap = 0x0B;	/* 5.1 */

		}
		break;

	case 5:
		break;

	case 4:
		if ((_stAvdAVInfo.ui2_aud_out_ch.bit.FR == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.FL == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.RR == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.RL == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.LFE == 0)) {
			bChannelMap = 0x08;
		} else if ((_stAvdAVInfo.ui2_aud_out_ch.bit.FR == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.FL == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.FC == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.LFE == 1)) {
			bChannelMap = 0x03;
		}
		break;

	case 3:
		if ((_stAvdAVInfo.ui2_aud_out_ch.bit.FR == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.FL == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.FC == 1)) {
			bChannelMap = 0x02;
		} else if ((_stAvdAVInfo.ui2_aud_out_ch.bit.FR == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.FL == 1) &&
			   (_stAvdAVInfo.ui2_aud_out_ch.bit.LFE == 1)
		    ) {
			bChannelMap = 0x01;
		}

		break;

	case 2:
		if ((_stAvdAVInfo.ui2_aud_out_ch.bit.FR == 1) &&
		    (_stAvdAVInfo.ui2_aud_out_ch.bit.FL == 1)) {
			bChannelMap = 0x00;
		}
		break;

	default:
		break;
	}


	return bChannelMap;
}


void vSetHDMIAudioIn(void)
{
	unsigned char bI2sFmt, bData2, bChMapping = 0;

	vSetChannelSwap(LFE_CC_SWAP);
	vEnableIecTxRaw();

	bData2 = vCheckPcmBitSize(0);

	bI2sFmt = _stAvdAVInfo.e_I2sFmt;

	if ((_stAvdAVInfo.e_hdmi_aud_in == SV_SPDIF) && (_stAvdAVInfo.e_aud_code == AVD_DST)) {
		vAOUT_BNUM_SEL(AOUT_24BIT);
	} else if (_stAvdAVInfo.e_I2sFmt == HDMI_LJT_24BIT) {
		if (!(bData2 == PCM_24BIT))
			bI2sFmt = HDMI_LJT_16BIT;
	}
	vSetHdmiI2SDataFmt(bI2sFmt);
	if (bData2 == PCM_24BIT)
		vAOUT_BNUM_SEL(AOUT_24BIT);
	else
		vAOUT_BNUM_SEL(AOUT_16BIT);

	vSetHdmiHighBitrate(FALSE);
	vDSTNormalDouble(FALSE);
	vEnableDSTConfig(FALSE);

	if (_stAvdAVInfo.e_hdmi_aud_in == SV_SPDIF) {
		vDisableDsdConfig();

		if ((_stAvdAVInfo.e_aud_code == AVD_DST)) {
			vEnableDSTConfig(TRUE);
			vDSTNormalDouble(TRUE);
		}

		vSetHdmiI2SChNum(2, bChMapping);
		vSetHdmiIecI2s(SV_SPDIF);
	} else			/* I2S input */
	{
		bChMapping = bGetChannelMapping();

		vDisableDsdConfig();

		vSetHdmiI2SChNum(_stAvdAVInfo.ui1_aud_out_ch_number, bChMapping);
		vSetHdmiIecI2s(SV_I2S);
	}
}

void vHwNCTSOnOff(unsigned char bHwNctsOn)
{
	unsigned char bData;
	HDMI_AUDIO_FUNC();
	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;
	bData = bReadByteHdmiGRL(GRL_CTS_CTRL);

	if (bHwNctsOn == TRUE)
		bData &= ~CTS_CTRL_SOFT;
	else
		bData |= CTS_CTRL_SOFT;

	vWriteByteHdmiGRL(GRL_CTS_CTRL, bData);

}

void vHalHDMI_NCTS(unsigned char bAudioFreq, unsigned char bPix, unsigned char bDeepMode)
{
	unsigned char bTemp, bData, bData1[NCTS_BYTES];
	unsigned int u4Temp, u4NTemp = 0;

	HDMI_AUDIO_FUNC();
	HDMI_AUDIO_LOG("bAudioFreq=%d,  bPix=%d, bDeepMode=%d\n", bAudioFreq, bPix, bDeepMode);

	bData = 0;
	vWriteByteHdmiGRL(GRL_NCTS, bData);	/* YT suggest 3 dummy N-CTS */
	vWriteByteHdmiGRL(GRL_NCTS, bData);
	vWriteByteHdmiGRL(GRL_NCTS, bData);

	for (bTemp = 0; bTemp < NCTS_BYTES; bTemp++) {
		bData1[bTemp] = 0;
	}

	if (bDeepMode == HDMI_NO_DEEP_COLOR) {
		for (bTemp = 0; bTemp < NCTS_BYTES; bTemp++) {

			if ((bAudioFreq < 7) && (bPix < 7))

				bData1[bTemp] = HDMI_NCTS[bAudioFreq][bPix][bTemp];
		}

		u4NTemp = (bData1[4] << 16) | (bData1[5] << 8) | (bData1[6]);	/* N */
		u4Temp = (bData1[0] << 24) | (bData1[1] << 16) | (bData1[2] << 8) | (bData1[3]);	/* CTS */

	} else {
		for (bTemp = 0; bTemp < NCTS_BYTES; bTemp++) {
			if ((bAudioFreq < 7) && (bPix < 7))

				bData1[bTemp] = HDMI_NCTS[bAudioFreq][bPix][bTemp];
		}

		u4NTemp = (bData1[4] << 16) | (bData1[5] << 8) | (bData1[6]);	/* N */
		u4Temp = (bData1[0] << 24) | (bData1[1] << 16) | (bData1[2] << 8) | (bData1[3]);

		if (bDeepMode == HDMI_DEEP_COLOR_10_BIT) {
			u4Temp = (u4Temp >> 2) * 5;	/* (*5/4) */
		} else if (bDeepMode == HDMI_DEEP_COLOR_12_BIT) {
			u4Temp = (u4Temp >> 1) * 3;	/* (*3/2) */
		} else if (bDeepMode == HDMI_DEEP_COLOR_16_BIT) {
			u4Temp = (u4Temp << 1);	/* (*2) */
		}

		bData1[0] = (u4Temp >> 24) & 0xff;
		bData1[1] = (u4Temp >> 16) & 0xff;
		bData1[2] = (u4Temp >> 8) & 0xff;
		bData1[3] = (u4Temp) & 0xff;

	}
	for (bTemp = 0; bTemp < NCTS_BYTES; bTemp++) {
		bData = bData1[bTemp];
		vWriteByteHdmiGRL(GRL_NCTS, bData);
	}

	_u4NValue = u4NTemp;
}

void vHDMI_NCTS(unsigned char bHDMIFsFreq, unsigned char bResolution, unsigned char bdeepmode)
{
	unsigned char bPix;

	HDMI_AUDIO_FUNC();

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
	case HDMI_VIDEO_1920x1080p_60Hz:	/* 148.35M pixel clock */
	case HDMI_VIDEO_1280x720p3d_60Hz:
	case HDMI_VIDEO_1920x1080i3d_60Hz:
	case HDMI_VIDEO_1920x1080p3d_23Hz:
		bPix = 4;
		break;
	case HDMI_VIDEO_1920x1080p_50Hz:	/* 148.50M pixel clock */
	case HDMI_VIDEO_1280x720p3d_50Hz:
	case HDMI_VIDEO_1920x1080i3d_50Hz:
	case HDMI_VIDEO_1920x1080p3d_24Hz:
		bPix = 5;
		break;
	}

	vHalHDMI_NCTS(bHDMIFsFreq, bPix, bdeepmode);

}

void vSetHdmiClkRefIec2(unsigned char fgSyncIec2Clock)
{
	HDMI_AUDIO_FUNC();
}

void vSetHDMISRCOff(void)
{
	unsigned char bData;
	HDMI_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_MIX_CTRL);
	bData &= ~MIX_CTRL_SRC_EN;
	vWriteByteHdmiGRL(GRL_MIX_CTRL, bData);
	bData = 0x00;
	vWriteByteHdmiGRL(GRL_SHIFT_L1, bData);
}

void vHalSetHDMIFS(unsigned char bFs)
{
	unsigned char bData;
	HDMI_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_CFG5);
	bData &= CFG5_CD_RATIO_MASK;
	bData |= bFs;
	vWriteByteHdmiGRL(GRL_CFG5, bData);

}

void vHdmiAclkInv(unsigned char bInv)
{
	unsigned char bData;
	HDMI_AUDIO_FUNC();
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

void vSetHDMIFS(unsigned char bFs, unsigned char fgAclInv)
{
	HDMI_AUDIO_FUNC();
	vHalSetHDMIFS(bFs);

	if (fgAclInv) {
		vHdmiAclkInv(TRUE);	/* //fix 192kHz, SPDIF downsample noise issue, ACL iNV */
	} else {
		vHdmiAclkInv(FALSE);	/* fix 192kHz, SPDIF downsample noise issue */
	}

}

void vReEnableSRC(void)
{
	unsigned char bData;
	HDMI_AUDIO_FUNC();
	bData = bReadByteHdmiGRL(GRL_MIX_CTRL);
	if (bData & MIX_CTRL_SRC_EN) {
		bData &= ~MIX_CTRL_SRC_EN;
		vWriteByteHdmiGRL(GRL_MIX_CTRL, bData);
		udelay(255);
		bData |= MIX_CTRL_SRC_EN;
		vWriteByteHdmiGRL(GRL_MIX_CTRL, bData);
	}

}

void vHDMIAudioSRC(unsigned char ui1hdmifs, unsigned char ui1resindex, unsigned char bdeepmode)
{

	HDMI_AUDIO_FUNC();

	vHwNCTSOnOff(FALSE);

	if (_stAvdAVInfo.e_hdmi_aud_in == SV_SPDIF)
		vSetHdmiClkRefIec2(TRUE);
	else
		vSetHdmiClkRefIec2(FALSE);

	if (_stAvdAVInfo.e_hdmi_aud_in == SV_I2S) {
		switch (ui1hdmifs) {
		case HDMI_FS_32K:
		case HDMI_FS_96K:
		case HDMI_FS_44K:
		case HDMI_FS_88K:
		case HDMI_FS_48K:
			vSetHDMISRCOff();
			if (_stAvdAVInfo.u1HdmiI2sMclk == MCLK_128FS) {
				vSetHDMIFS(CFG5_FS128, FALSE);
			} else if (_stAvdAVInfo.u1HdmiI2sMclk == MCLK_256FS) {
				vSetHDMIFS(CFG5_FS256, FALSE);
			} else if (_stAvdAVInfo.u1HdmiI2sMclk == MCLK_384FS) {
				vSetHDMIFS(CFG5_FS384, FALSE);
			} else if (_stAvdAVInfo.u1HdmiI2sMclk == MCLK_512FS) {
				vSetHDMIFS(CFG5_FS512, FALSE);
			} else if (_stAvdAVInfo.u1HdmiI2sMclk == MCLK_768FS) {
				vSetHDMIFS(CFG5_FS768, FALSE);
			} else {
				vSetHDMIFS(CFG5_FS256, FALSE);
			}
			break;

		default:
			break;
		}
	} else {
		switch (_stAvdAVInfo.e_iec_frame) {
		case IEC_32K:
			ui1hdmifs = HDMI_FS_32K;
			vSetHDMISRCOff();
			vSetHDMIFS(CFG5_FS128, FALSE);
			break;

		case IEC_48K:
			ui1hdmifs = HDMI_FS_48K;
			vSetHDMISRCOff();
			vSetHDMIFS(CFG5_FS128, FALSE);
			break;

		case IEC_44K:
			ui1hdmifs = HDMI_FS_44K;
			vSetHDMISRCOff();
			vSetHDMIFS(CFG5_FS128, FALSE);
			break;

		default:
			break;
		}

	}
	vHDMI_NCTS(ui1hdmifs, ui1resindex, bdeepmode);
	_stAvdAVInfo.e_hdmi_fs = ui1hdmifs;
	vReEnableSRC();

}

void vHwSet_Hdmi_I2S_C_Status(unsigned char *prLChData, unsigned char *prRChData)
{
	unsigned char bData;
	HDMI_AUDIO_FUNC();
	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;
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
	unsigned char bData = 0;
	unsigned char bhdmi_RCh_status[5];
	unsigned char bhdmi_LCh_status[5];

	HDMI_AUDIO_FUNC();

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

	case HDMI_FS_88K:
		bData |= 0x08;
		break;

	case HDMI_FS_96K:
		bData |= 0x0A;
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

void vHalSendAudioInfoFrame(unsigned char bData1, unsigned char bData2, unsigned char bData4,
			    unsigned char bData5)
{
	unsigned char bAUDIO_CHSUM;
	unsigned char bData = 0;
	HDMI_AUDIO_FUNC();
	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;
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
	HDMI_AUDIO_FUNC();
	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;

	if (_stAvdAVInfo.e_hdmi_aud_in == SV_SPDIF) {
		_bAudInfoFm[0] = 0x00;	/* CC as 0, */
		_bAudInfoFm[3] = 0x00;	/* CA 2ch */
	} else			/* pcm */
	{
		switch (_stAvdAVInfo.ui2_aud_out_ch.word & 0x7fb) {
		case 0x03:	/* FL/FR */
			_bAudInfoFm[0] = 0x01;
			_bAudInfoFm[3] = 0x00;
			break;

		case 0x0b:	/* FL/FR/FC */
			_bAudInfoFm[0] = 0x02;
			_bAudInfoFm[3] = 0x02;
			break;

		case 0x13:	/* FL/FR/RC */
			_bAudInfoFm[0] = 0x02;
			_bAudInfoFm[3] = 0x04;
			break;

		case 0x1b:	/* FL/FR/FC/RC */
			_bAudInfoFm[0] = 0x03;
			_bAudInfoFm[3] = 0x06;
			break;

		case 0x33:	/* FL/FR/RL/RR */
			_bAudInfoFm[0] = 0x03;
			_bAudInfoFm[3] = 0x08;
			break;

		case 0x3b:	/* FL/FR/FC/RL/RR */
			_bAudInfoFm[0] = 0x04;
			_bAudInfoFm[3] = 0x0A;
			break;

		case 0x73:	/* FL/FR/RL/RR/RC */
			_bAudInfoFm[0] = 0x04;
			_bAudInfoFm[3] = 0x0C;
			break;

		case 0x7B:	/* FL/FR/FC/RL/RR/RC */
			_bAudInfoFm[0] = 0x05;
			_bAudInfoFm[3] = 0x0E;
			break;

		case 0x633:	/* FL/FR/RL/RR/RLC/RRC */
			_bAudInfoFm[0] = 0x05;
			_bAudInfoFm[3] = 0x10;
			break;

		case 0x63B:	/* FL/FR/FC/RL/RR/RLC/RRC */
			_bAudInfoFm[0] = 0x06;
			_bAudInfoFm[3] = 0x12;
			break;

		case 0x183:	/* FL/FR/FLC/FRC */
			_bAudInfoFm[0] = 0x03;
			_bAudInfoFm[3] = 0x14;
			break;

		case 0x18B:	/* FL/FR/FC/FLC/FRC */
			_bAudInfoFm[0] = 0x04;
			_bAudInfoFm[3] = 0x16;
			break;

		case 0x1C3:	/* FL/FR/RC/FLC/FRC */
			_bAudInfoFm[0] = 0x04;
			_bAudInfoFm[3] = 0x18;
			break;

		case 0x1CB:	/* FL/FR/FC/RC/FLC/FRC */
			_bAudInfoFm[0] = 0x05;
			_bAudInfoFm[3] = 0x1A;
			break;

		default:
			_bAudInfoFm[0] = 0x01;
			_bAudInfoFm[3] = 0x00;
			break;
		}

		if (_stAvdAVInfo.ui2_aud_out_ch.word & 0x04)	/* LFE */
		{
			_bAudInfoFm[0]++;
			_bAudInfoFm[3]++;
		}
	}

	_bAudInfoFm[1] = 0;
	_bAudInfoFm[4] = 0x0;

	vHalSendAudioInfoFrame(_bAudInfoFm[0], _bAudInfoFm[1], _bAudInfoFm[3], _bAudInfoFm[4]);

}

void vChgHDMIAudioOutput(unsigned char ui1hdmifs, unsigned char ui1resindex,
			 unsigned char bdeepmode)
{
	unsigned int ui4Index;

	HDMI_AUDIO_FUNC();
	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;
	MuteHDMIAudio();
	vAudioPacketOff(TRUE);
	vSetHDMIAudioIn();
	vHDMIAudioSRC(ui1hdmifs, ui1resindex, bdeepmode);

	vHDMI_I2S_C_Status();
	vSendAudioInfoFrame();

	for (ui4Index = 0; ui4Index < 5; ui4Index++) {
		udelay(5);
	}
	vHwNCTSOnOff(TRUE);

	vAudioPacketOff(FALSE);
	if (_bflagaudiomute == FALSE)
		UnMuteHDMIAudio();
}

void vDisableGamut(void)
{
	HDMI_AUDIO_FUNC();
	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;
	vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, GAMUT_EN);
}

void vHalSendAVIInfoFrame(unsigned char *pr_bData)
{
	unsigned char bAVI_CHSUM;
	unsigned char bData1 = 0, bData2 = 0, bData3 = 0, bData4 = 0, bData5 = 0;
	unsigned char bData;
	HDMI_VIDEO_FUNC();
	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;
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

void vSendAVIInfoFrame(unsigned char ui1resindex, unsigned char ui1colorspace)
{
	HDMI_VIDEO_FUNC();
	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;
	if (ui1colorspace == HDMI_YCBCR_444) {
		_bAviInfoFm[0] = 0x40;
	} else if (ui1colorspace == HDMI_YCBCR_422) {
		_bAviInfoFm[0] = 0x20;
	} else {
		_bAviInfoFm[0] = 0x00;
	}

	_bAviInfoFm[0] |= 0x10;	/* A0=1, Active format (R0~R3) inf valid */

	_bAviInfoFm[1] = 0x0;	/* bData2 */

	if (ui1resindex == HDMI_VIDEO_1920x1080p3d_23Hz)
		ui1resindex = HDMI_VIDEO_1920x1080p_23Hz;
	else if (ui1resindex == HDMI_VIDEO_1920x1080p3d_24Hz)
		ui1resindex = HDMI_VIDEO_1920x1080p_24Hz;
	else if (ui1resindex == HDMI_VIDEO_1920x1080i3d_60Hz)
		ui1resindex = HDMI_VIDEO_1920x1080i_60Hz;
	else if (ui1resindex == HDMI_VIDEO_1920x1080i3d_50Hz)
		ui1resindex = HDMI_VIDEO_1920x1080i_50Hz;
	else if (ui1resindex == HDMI_VIDEO_1280x720p3d_60Hz)
		ui1resindex = HDMI_VIDEO_1280x720p_60Hz;
	else if (ui1resindex == HDMI_VIDEO_1280x720p3d_50Hz)
		ui1resindex = HDMI_VIDEO_1280x720p_50Hz;


	if ((ui1resindex == HDMI_VIDEO_720x480p_60Hz) || (ui1resindex == HDMI_VIDEO_720x576p_50Hz)) {
		_bAviInfoFm[1] |= AV_INFO_SD_ITU601;
	} else {
		_bAviInfoFm[1] |= AV_INFO_HD_ITU709;
	}

	_bAviInfoFm[1] |= 0x20;
	_bAviInfoFm[1] |= 0x08;
	_bAviInfoFm[2] = 0;	/* bData3 */
	_bAviInfoFm[2] |= 0x04;	/* limit Range */

	if (ui1resindex < HDMI_VIDEO_RESOLUTION_NUM)
		_bAviInfoFm[3] = HDMI_VIDEO_ID_CODE[ui1resindex];	/* bData4 */

	if ((_bAviInfoFm[1] & AV_INFO_16_9_OUTPUT)
	    && ((ui1resindex == HDMI_VIDEO_720x480p_60Hz)
		|| (ui1resindex == HDMI_VIDEO_720x576p_50Hz))) {
		_bAviInfoFm[3] = _bAviInfoFm[3] + 1;
	}

	_bAviInfoFm[4] = 0x00;

	vHalSendAVIInfoFrame(&_bAviInfoFm[0]);

}

void vHalSendSPDInfoFrame(unsigned char *pr_bData)
{
	unsigned char bSPD_CHSUM, bData;
	unsigned char i = 0;

	HDMI_VIDEO_FUNC();

	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;
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
	unsigned char bData;
	HDMI_VIDEO_FUNC();
	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;

	bData = bReadByteHdmiGRL(GRL_CFG4);
	bData |= CFG4_AV_UNMUTE_EN;	/* disable original mute */
	bData &= ~CFG4_AV_UNMUTE_SET;	/* disable */

	vWriteByteHdmiGRL(GRL_CFG4, bData);
	udelay(30);

	bData &= ~CFG4_AV_UNMUTE_EN;	/* disable original mute */
	bData |= CFG4_AV_UNMUTE_SET;	/* disable */

	vWriteByteHdmiGRL(GRL_CFG4, bData);

}

void vHalSendVendorSpecificInfoFrame(unsigned char fg3DRes, unsigned char bVIC,
				     unsigned char b3dstruct)
{
	unsigned char bVS_CHSUM;
	unsigned char bPB1, bPB2, bPB3, bPB4, bPB5;

	HDMI_VIDEO_FUNC();

	vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, 0, VS_EN);
	vWriteByteHdmiGRL(GRL_INFOFRM_VER, VS_VERS);	/* HB1 */
	vWriteByteHdmiGRL(GRL_INFOFRM_TYPE, VS_TYPE);	/* HB0 */
	vWriteByteHdmiGRL(GRL_INFOFRM_LNG, VS_LEN);	/* HB2 */

	bPB1 = 0x03;
	bPB2 = 0x0C;
	bPB3 = 0x00;

	if (fg3DRes == TRUE)
		bPB4 = 0x40;

	bPB5 = 0x00;
	bPB5 |= b3dstruct << 4;

	bVS_CHSUM = VS_VERS + VS_TYPE + VS_LEN + bPB1 + bPB2 + bPB3 + bPB4 + bPB5;
	bVS_CHSUM = 0x100 - bVS_CHSUM;

	vWriteByteHdmiGRL(GRL_IFM_PORT, bVS_CHSUM);	/* check sum */
	vWriteByteHdmiGRL(GRL_IFM_PORT, bPB1);	/* 24bit IEEE Registration Identifier */
	vWriteByteHdmiGRL(GRL_IFM_PORT, bPB2);	/* 24bit IEEE Registration Identifier */
	vWriteByteHdmiGRL(GRL_IFM_PORT, bPB3);	/* 24bit IEEE Registration Identifier */
	vWriteByteHdmiGRL(GRL_IFM_PORT, bPB4);	/* HDMI_Video_Format */
	vWriteByteHdmiGRL(GRL_IFM_PORT, bPB5);	/* 3D_Structure/4k2k */

	vWriteHdmiGRLMsk(GRL_ACP_ISRC_CTRL, VS_EN, VS_EN);
	HDMI_VIDEO_LOG
	    (" vendor: bVS_CHSUM = %d, bPB1 = %d, bPB2 = %d, bPB3 = %d, bPB4 = %d, bPB5 = %d\n",
	     bVS_CHSUM, bPB1, bPB2, bPB3, bPB4, bPB5);
}

void vSendVendorSpecificInfoFrame(unsigned char ui1resindex)
{
	unsigned char bResTableIndex, b3DStruct;
	unsigned char fg3DRes;

	if (i4SharedInfo(SI_EDID_VSDB_EXIST) == FALSE)
		return;

	fg3DRes = TRUE;

	if (ui1resindex == HDMI_VIDEO_1920x1080p3d_23Hz)
		ui1resindex = HDMI_VIDEO_1920x1080p_23Hz;
	else if (ui1resindex == HDMI_VIDEO_1920x1080p3d_24Hz)
		ui1resindex = HDMI_VIDEO_1920x1080p_24Hz;
	else if (ui1resindex == HDMI_VIDEO_1920x1080i3d_60Hz)
		ui1resindex = HDMI_VIDEO_1920x1080i_60Hz;
	else if (ui1resindex == HDMI_VIDEO_1920x1080i3d_50Hz)
		ui1resindex = HDMI_VIDEO_1920x1080i_50Hz;
	else if (ui1resindex == HDMI_VIDEO_1280x720p3d_60Hz)
		ui1resindex = HDMI_VIDEO_1280x720p_60Hz;
	else if (ui1resindex == HDMI_VIDEO_1280x720p3d_50Hz)
		ui1resindex = HDMI_VIDEO_1280x720p_50Hz;
	else
		fg3DRes = FALSE;

	b3DStruct = 0;

	if (ui1resindex < HDMI_VIDEO_RESOLUTION_NUM)
		bResTableIndex = HDMI_VIDEO_ID_CODE[ui1resindex];	/* bData4 */

	if (fg3DRes == TRUE)
		vHalSendVendorSpecificInfoFrame(fg3DRes, bResTableIndex, b3DStruct);

}

void vChgHDMIVideoResolution(unsigned char ui1resindex, unsigned char ui1colorspace,
			     unsigned char ui1hdmifs, unsigned char bdeepmode)
{
	unsigned int u4Index;
	HDMI_VIDEO_FUNC();

	vHDMIAVMute();
	vHDMIResetGenReg(ui1resindex, ui1colorspace);
	vChgHDMIAudioOutput(ui1hdmifs, ui1resindex, bdeepmode);

	for (u4Index = 0; u4Index < 5; u4Index++) {
		udelay(10);
	}

	vDisableGamut();
	vSendAVIInfoFrame(ui1resindex, ui1colorspace);
	vSendVendorSpecificInfoFrame(ui1resindex);
	/* vHalSendSPDInfoFrame(&_bSpdInf[0]); */
	vSend_AVUNMUTE();
}

void vChgtoSoftNCTS(unsigned char ui1resindex, unsigned char ui1audiosoft, unsigned char ui1hdmifs,
		    unsigned char bdeepmode)
{
	HDMI_AUDIO_FUNC();

	vHwNCTSOnOff(ui1audiosoft);	/* change to software NCTS; */
	vHDMI_NCTS(ui1hdmifs, ui1resindex, bdeepmode);

}

void vShowHpdRsenStatus(void)
{
	if (bCheckPordHotPlug(HOTPLUG_MODE) == TRUE)
		pr_info("[HDMI]HPD ON\n");
	else
		pr_info("[HDMI]HPD OFF\n");

	if (bCheckPordHotPlug(PORD_MODE) == TRUE)
		pr_info("[HDMI]RSEN ON\n");
	else
		pr_info("[HDMI]RSEN OFF\n");

	if (i4SharedInfo(SI_HDMI_RECEIVER_STATUS) == HDMI_PLUG_IN_ONLY)
		pr_info("[HDMI]SI_HDMI_RECEIVER_STATUS = HDMI_PLUG_IN_ONLY\n");
	else if (i4SharedInfo(SI_HDMI_RECEIVER_STATUS) == HDMI_PLUG_IN_AND_SINK_POWER_ON)
		pr_info("[HDMI]SI_HDMI_RECEIVER_STATUS = HDMI_PLUG_IN_AND_SINK_POWER_ON\n");
	else if (i4SharedInfo(SI_HDMI_RECEIVER_STATUS) == HDMI_PLUG_OUT)
		pr_info("[HDMI]SI_HDMI_RECEIVER_STATUS = HDMI_PLUG_OUT\n");
	else
		pr_info("[HDMI]SI_HDMI_RECEIVER_STATUS error\n");



}

void vShowOutputVideoResolution(void)
{
	pr_info("[HDMI]HDMI output resolution = %s\n", szHdmiResStr[_stAvdAVInfo.e_resolution]);	/*  */

}

void vShowDviOrHdmiMode(void)
{
	if (vIsDviMode())
		pr_info("[HDMI]DVI Mode\n");
	else
		pr_info("[HDMI]HDMI Mode\n");

}

void vShowDeepColor(void)
{

	if (_stAvdAVInfo.e_deep_color_bit == HDMI_NO_DEEP_COLOR)
		pr_info("[HDMI]HDMI output deepcolor = HDMI_NO_DEEP_COLOR\n");
	else if (_stAvdAVInfo.e_deep_color_bit == HDMI_DEEP_COLOR_10_BIT)
		pr_info("[HDMI]HDMI output deepcolor = HDMI_DEEP_COLOR_10_BIT\n");
	else if (_stAvdAVInfo.e_deep_color_bit == HDMI_DEEP_COLOR_12_BIT)
		pr_info("[HDMI]HDMI output deepcolor = HDMI_DEEP_COLOR_12_BIT\n");
	else if (_stAvdAVInfo.e_deep_color_bit == HDMI_DEEP_COLOR_16_BIT)
		pr_info("[HDMI]HDMI output deepcolor = HDMI_DEEP_COLOR_16_BIT\n");
	else
		pr_info("[HDMI]HDMI output deepcolor error\n");

}

void vShowColorSpace(void)
{
	if (_stAvdAVInfo.e_video_color_space == HDMI_RGB)
		pr_info("[HDMI]HDMI output colorspace = HDMI_RGB\n");
	else if (_stAvdAVInfo.e_video_color_space == HDMI_RGB_FULL)
		pr_info("[HDMI]HDMI output colorspace = HDMI_RGB_FULL\n");
	else if (_stAvdAVInfo.e_video_color_space == HDMI_YCBCR_444)
		pr_info("[HDMI]HDMI output colorspace = HDMI_YCBCR_444\n");
	else if (_stAvdAVInfo.e_video_color_space == HDMI_YCBCR_422)
		pr_info("[HDMI]HDMI output colorspace = HDMI_YCBCR_422\n");
	else if (_stAvdAVInfo.e_video_color_space == HDMI_XV_YCC)
		pr_info("[HDMI]HDMI output colorspace = HDMI_XV_YCC\n");
	else
		pr_info("[HDMI]HDMI output colorspace error\n");

}

void vShowInforFrame(void)
{
	pr_info
	    ("====================Audio inforFrame Start ====================================\n");
	pr_info("Data Byte (1~5) = 0x%x  0x%x  0x%x  0x%x  0x%x\n", _bAudInfoFm[0], _bAudInfoFm[1],
		_bAudInfoFm[2], _bAudInfoFm[3], _bAudInfoFm[4]);
	pr_info("CC2~ CC0: 0x%x, %s\n", _bAudInfoFm[0] & 0x07,
		cAudChCountStr[_bAudInfoFm[0] & 0x07]);
	pr_info("CT3~ CT0: 0x%x, %s\n", (_bAudInfoFm[0] >> 4) & 0x0f,
		cAudCodingTypeStr[(_bAudInfoFm[0] >> 4) & 0x0f]);
	pr_info("SS1, SS0: 0x%x, %s\n", _bAudInfoFm[1] & 0x03,
		cAudSampleSizeStr[_bAudInfoFm[1] & 0x03]);
	pr_info("SF2~ SF0: 0x%x, %s\n", (_bAudInfoFm[1] >> 2) & 0x07,
		cAudFsStr[(_bAudInfoFm[1] >> 2) & 0x07]);
	pr_info("CA7~ CA0: 0x%x, %s\n", _bAudInfoFm[3] & 0xff,
		cAudChMapStr[_bAudInfoFm[3] & 0xff]);
	pr_info("LSV3~LSV0: %d db\n", (_bAudInfoFm[4] >> 3) & 0x0f);
	pr_info("DM_INH: 0x%x ,%s\n", (_bAudInfoFm[4] >> 7) & 0x01,
		cAudDMINHStr[(_bAudInfoFm[4] >> 7) & 0x01]);
	pr_info
	    ("====================Audio inforFrame End ======================================\n");

	pr_info("====================AVI inforFrame Start ====================================\n");
	pr_info("Data Byte (1~5) = 0x%x  0x%x  0x%x  0x%x  0x%x\n", _bAviInfoFm[0], _bAviInfoFm[1],
		_bAviInfoFm[2], _bAviInfoFm[3], _bAviInfoFm[4]);
	pr_info("S1,S0: 0x%x, %s\n", _bAviInfoFm[0] & 0x03, cAviScanStr[_bAviInfoFm[0] & 0x03]);
	pr_info("B1,S0: 0x%x, %s\n", (_bAviInfoFm[0] >> 2) & 0x03,
		cAviBarStr[(_bAviInfoFm[0] >> 2) & 0x03]);
	pr_info("A0: 0x%x, %s\n", (_bAviInfoFm[0] >> 4) & 0x01,
		cAviActivePresentStr[(_bAviInfoFm[0] >> 4) & 0x01]);
	pr_info("Y1,Y0: 0x%x, %s\n", (_bAviInfoFm[0] >> 5) & 0x03,
		cAviRgbYcbcrStr[(_bAviInfoFm[0] >> 5) & 0x03]);
	pr_info("R3~R0: 0x%x, %s\n", (_bAviInfoFm[1]) & 0x0f,
		cAviActiveStr[(_bAviInfoFm[1]) & 0x0f]);
	pr_info("M1,M0: 0x%x, %s\n", (_bAviInfoFm[1] >> 4) & 0x03,
		cAviAspectStr[(_bAviInfoFm[1] >> 4) & 0x03]);
	pr_info("C1,C0: 0x%x, %s\n", (_bAviInfoFm[1] >> 6) & 0x03,
		cAviColorimetryStr[(_bAviInfoFm[1] >> 6) & 0x03]);
	pr_info("SC1,SC0: 0x%x, %s\n", (_bAviInfoFm[2]) & 0x03,
		cAviScaleStr[(_bAviInfoFm[2]) & 0x03]);
	pr_info("Q1,Q0: 0x%x, %s\n", (_bAviInfoFm[2] >> 2) & 0x03,
		cAviRGBRangeStr[(_bAviInfoFm[2] >> 2) & 0x03]);
	if (((_bAviInfoFm[2] >> 4) & 0x07) <= 1)
		pr_info("EC2~EC0: 0x%x, %s\n", (_bAviInfoFm[2] >> 4) & 0x07,
			cAviExtColorimetryStr[(_bAviInfoFm[2] >> 4) & 0x07]);
	else
		pr_info("EC2~EC0: resevered\n");
	pr_info("ITC: 0x%x, %s\n", (_bAviInfoFm[2] >> 7) & 0x01,
		cAviItContentStr[(_bAviInfoFm[2] >> 7) & 0x01]);
	pr_info("====================AVI inforFrame End ======================================\n");

	pr_info("====================SPD inforFrame Start ====================================\n");
	pr_info("Data Byte (1~8)  = 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n", _bSpdInf[0],
		_bSpdInf[1], _bSpdInf[2], _bSpdInf[3], _bSpdInf[4], _bSpdInf[5], _bSpdInf[6],
		_bSpdInf[7]);
	pr_info("Data Byte (9~16) = 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n", _bSpdInf[8],
		_bSpdInf[9], _bSpdInf[10], _bSpdInf[11], _bSpdInf[12], _bSpdInf[13], _bSpdInf[14],
		_bSpdInf[15]);
	pr_info("Data Byte (17~24)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n", _bSpdInf[16],
		_bSpdInf[17], _bSpdInf[18], _bSpdInf[19], _bSpdInf[20], _bSpdInf[21], _bSpdInf[22],
		_bSpdInf[23]);
	pr_info("Data Byte  25    = 0x%x\n", _bSpdInf[24]);
	pr_info("Source Device information is %s\n", cSPDDeviceStr[_bSpdInf[24]]);
	pr_info("====================SPD inforFrame End ======================================\n");

	if (fgIsAcpEnable()) {
		pr_info
		    ("====================ACP inforFrame Start ====================================\n");
		pr_info("Acp type =0x%x\n", _bAcpType);

		if (_bAcpType == 0) {
			pr_info("Generic Audio\n");
			pr_info("Data Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
				_bAcpData[0], _bAcpData[1], _bAcpData[2], _bAcpData[3],
				_bAcpData[4], _bAcpData[5], _bAcpData[6], _bAcpData[7]);
			pr_info
			    ("Data Byte (9~16)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			     _bAcpData[8], _bAcpData[9], _bAcpData[10], _bAcpData[11],
			     _bAcpData[12], _bAcpData[13], _bAcpData[14], _bAcpData[15]);
		} else if (_bAcpType == 1) {
			pr_info("IEC 60958-Identified Audio\n");
			pr_info("Data Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
				_bAcpData[0], _bAcpData[1], _bAcpData[2], _bAcpData[3],
				_bAcpData[4], _bAcpData[5], _bAcpData[6], _bAcpData[7]);
			pr_info
			    ("Data Byte (9~16)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			     _bAcpData[8], _bAcpData[9], _bAcpData[10], _bAcpData[11],
			     _bAcpData[12], _bAcpData[13], _bAcpData[14], _bAcpData[15]);
		} else if (_bAcpType == 2) {
			pr_info("DVD Audio\n");
			pr_info("DVD-AUdio_TYPE_Dependent Generation = 0x%x\n", _bAcpData[0]);
			pr_info("Copy Permission = 0x%x\n", (_bAcpData[1] >> 6) & 0x03);
			pr_info("Copy Number = 0x%x\n", (_bAcpData[1] >> 3) & 0x07);
			pr_info("Quality = 0x%x\n", (_bAcpData[1] >> 1) & 0x03);
			pr_info("Transaction = 0x%x\n", _bAcpData[1] & 0x01);

		} else if (_bAcpType == 3) {
			pr_info("SuperAudio CD\n");
			pr_info
			    ("CCI_1 Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			     _bAcpData[0], _bAcpData[1], _bAcpData[2], _bAcpData[3], _bAcpData[4],
			     _bAcpData[5], _bAcpData[6], _bAcpData[7]);
			pr_info
			    ("CCI_1 Byte (9~16)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			     _bAcpData[8], _bAcpData[9], _bAcpData[10], _bAcpData[11],
			     _bAcpData[12], _bAcpData[13], _bAcpData[14], _bAcpData[15]);

		}
		pr_info
		    ("====================ACP inforFrame End ======================================\n");
	}

	if (fgIsISRC1Enable()) {
		pr_info
		    ("====================ISRC1 inforFrame Start ====================================\n");
		pr_info("Data Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			_bIsrc1Data[0], _bIsrc1Data[1], _bIsrc1Data[2], _bIsrc1Data[3],
			_bIsrc1Data[4], _bIsrc1Data[5], _bIsrc1Data[6], _bIsrc1Data[7]);
		pr_info
		    ("====================ISRC1 inforFrame End ======================================\n");
	}

	if (fgIsISRC2Enable()) {
		pr_info
		    ("====================ISRC2 inforFrame Start ====================================\n");
		pr_info("Data Byte (1~8)= 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
			_bIsrc1Data[8], _bIsrc1Data[9], _bIsrc1Data[10], _bIsrc1Data[11],
			_bIsrc1Data[12], _bIsrc1Data[13], _bIsrc1Data[14], _bIsrc1Data[15]);
		pr_info
		    ("====================ISRC2 inforFrame End ======================================\n");
	}
}

unsigned int u4ReadNValue(void)
{
	return _u4NValue;
}

unsigned int u4ReadCtsValue(void)
{
	unsigned int u4Data;

	u4Data = bReadByteHdmiGRL(GRL_CTS0) & 0xff;
	u4Data |= ((bReadByteHdmiGRL(GRL_CTS1) & 0xff) << 8);
	u4Data |= ((bReadByteHdmiGRL(GRL_CTS2) & 0x0f) << 16);

	return u4Data;
}

void vShowHdmiAudioStatus(void)
{
	pr_info("[HDMI]HDMI output audio Channel Number =%d\n",
		_stAvdAVInfo.ui1_aud_out_ch_number);
	pr_info("[HDMI]HDMI output Audio Fs = %s\n", cHdmiAudFsStr[_stAvdAVInfo.e_hdmi_fs]);
	pr_info("[HDMI]HDMI MCLK =%d\n", _stAvdAVInfo.u1HdmiI2sMclk);
	pr_info("[HDMI]HDMI output ACR N= %d, CTS = %d\n", u4ReadNValue(), u4ReadCtsValue());
}

void vShowHdmiDrmHdcpStatus(void)
{
	pr_info("[HDMI]DrmHdcp _bflagvideomute =%d\n", _bflagvideomute);
	pr_info("[HDMI]DrmHdcp _bflagaudiomute =%d\n", _bflagaudiomute);
	pr_info("[HDMI]DrmHdcp _bsvpvideomute =%d\n", _bsvpvideomute);
	pr_info("[HDMI]DrmHdcp _bsvpaudiomute =%d\n", _bsvpaudiomute);
	pr_info("[HDMI]DrmHdcp _bNeedSwHdcp =%d\n", _bHdcpOff);
	pr_info("[HDMI]DrmHdcp _bHdcpOff =%d\n", _bHdcpOff);
}

void hdmi_hdmistatus(void)
{
	vShowHpdRsenStatus();
	vShowOutputVideoResolution();
	vShowDviOrHdmiMode();
	vShowInforFrame();
	vShowHdmiAudioStatus();
}

#endif
