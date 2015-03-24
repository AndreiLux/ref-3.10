#ifndef _HDMI_CA_H_
#define _HDMI_CA_H_
#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT

bool fgCaHDMICreate(void);
bool fgCaHDMIClose(void);
void vCaHDMIWriteReg(unsigned int u4addr, unsigned int u4data);
bool fgCaHDMIInstallHdcpKey(unsigned char *pdata, unsigned int u4Len);
bool fgCaHDMIGetAKsv(unsigned char *pdata);
bool fgCaHDMILoadHDCPKey(void);
bool fgCaHDMIHDCPReset(bool fgen);
bool fgCaHDMIHDCPEncEn(bool fgen);
bool fgCaHDMIVideoUnMute(bool fgen);
bool fgCaHDMIAudioUnMute(bool fgen);
void vCaDPI1WriteReg(unsigned int u4addr, unsigned int u4data);

#endif
#endif
