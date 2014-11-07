/*
 * @file:control.c
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */
 #include <linux/kernel.h>

#include "control.h"
#include "halMainController.h"
#include "halInterrupt.h"
#include "halIdentification.h"
#include "../util/log.h"
#include "../util/error.h"

/* block offsets */
const u16 id_base_addr = 0x0000;
const u16 ih_base_addr = 0x0100;
static const u16 mc_base_addr = 0x4000;

int control_initialize(u16 baseAddr, u8 dataEnablePolarity, u16 pixelClock)
{
	/*  clock gate == 1 => turn off modules */
	halmaincontroller_pixelclockgate(baseAddr + mc_base_addr, 1);
	halmaincontroller_tmdsclockgate(baseAddr + mc_base_addr, 1);
	halmaincontroller_pixelrepetitionclockgate(baseAddr + mc_base_addr, 1);
	halmaincontroller_colorspaceconverterclockgate(baseAddr + mc_base_addr, 1);
	halmaincontroller_audiosamplerclockgate(baseAddr + mc_base_addr, 1);
	halmaincontroller_cecclockgate(baseAddr + mc_base_addr, 1);
	halmaincontroller_hdcpclockgate(baseAddr + mc_base_addr, 1);
	return TRUE;
}

int control_configure(u16 baseAddr, u16 pClk, u8 pRep, u8 cRes,
	int cscOn, int audioOn, int cecOn, int hdcpOn)
{
	halmaincontroller_videofeedthroughoff(baseAddr + mc_base_addr, cscOn ? 0 : 1);
	/*  clock gate == 0 => turn on modules */
	halmaincontroller_pixelclockgate(baseAddr + mc_base_addr, 0);
	halmaincontroller_tmdsclockgate(baseAddr + mc_base_addr, 0);
	halmaincontroller_pixelrepetitionclockgate(
				baseAddr+ mc_base_addr, (pRep > 0) ? 0 : 1);
	/*if this bit =1, then it doesn't display anything.*/
	halmaincontroller_colorspaceconverterclockgate(
		baseAddr+ mc_base_addr, 0); /*cscOn ? 0 : 1); */
	halmaincontroller_audiosamplerclockgate(
		baseAddr + mc_base_addr, audioOn ? 0 : 1);
	halmaincontroller_cecclockgate(baseAddr + mc_base_addr, cecOn ? 0 : 1);
	halmaincontroller_hdcpclockgate(baseAddr + mc_base_addr, hdcpOn ? 0 : 1);
	return TRUE;
}

int control_standby(u16 baseAddr)
{
	LOG_TRACE();
	/*  clock gate == 1 => turn off modules */
	halmaincontroller_hdcpclockgate(baseAddr + mc_base_addr, 1);
	/*CEC is not turned off because it has to answer PINGs even in standby mode*/
	halmaincontroller_audiosamplerclockgate(baseAddr + mc_base_addr,
			1);
	halmaincontroller_colorspaceconverterclockgate(baseAddr
			+ mc_base_addr, 1);
	halmaincontroller_pixelrepetitionclockgate(baseAddr
			+ mc_base_addr, 1);
	halmaincontroller_pixelclockgate(baseAddr + mc_base_addr, 1);
	halmaincontroller_tmdsclockgate(baseAddr + mc_base_addr, 1);
	return TRUE;
}

u8 control_design(u16 baseAddr)
{
	LOG_TRACE();
	return halidentification_design(baseAddr + id_base_addr);
}

u8 control_revision(u16 baseAddr)
{
	LOG_TRACE();
	return halidentification_revision(baseAddr + id_base_addr);
}

u8 control_productline(u16 baseAddr)
{
	LOG_TRACE();
	return halidentification_productline(baseAddr + id_base_addr);
}

u8 control_producttype(u16 baseAddr)
{
	LOG_TRACE();
	return halidentification_producttype(baseAddr + id_base_addr);
}

int control_supportscore(u16 baseAddr)
{
	/*  Line 0xA0 - HDMICTRL */
	/*  Type 0x01 - TX */
	/*  Type 0xC1 - TX with HDCP */
	return control_productline(baseAddr) == 0xA0 &&
			(control_producttype(baseAddr) == 0x01
			|| control_producttype(baseAddr) == 0xC1);
}

int control_supportshdcp(u16 baseAddr)
{
	/*  Line 0xA0 - HDMICTRL */
	/*  Type 0xC1 - TX with HDCP */
	return control_productline(baseAddr) == 0xA0 &&
				control_producttype(baseAddr) == 0xC1;
}

int control_interruptaudiopacketsmute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	halinterrupt_audiopacketsmute(baseAddr + ih_base_addr, value);
	return TRUE;
}

int control_interruptotherpacketsmute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	halinterrupt_otherpacketsmute(baseAddr + ih_base_addr, value);
	return TRUE;
}

int control_interruptpacketsoverflowmute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	halinterrupt_packetsoverflowmute(baseAddr + ih_base_addr, value);
	return TRUE;
}

int control_interruptaudiosamplermute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	halinterrupt_audiosamplemute(baseAddr + ih_base_addr, value);
	return TRUE;
}

int control_interruptphymute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	halinterrupt_phymute(baseAddr + ih_base_addr, value);
	return TRUE;
}

int control_interruptedidmute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	halinterrupt_i2cddcmute(baseAddr + ih_base_addr, value);
	return TRUE;
}

int control_interruptcecmute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	halinterrupt_cecmute(baseAddr + ih_base_addr, value);
	return TRUE;
}

int control_interruptvideopacketizermute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	halinterrupt_videopacketizermute(baseAddr + ih_base_addr, value);
	return TRUE;
}

int control_interrupti2cphymute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	halinterrupt_i2cphymute(baseAddr + ih_base_addr, value);
	return TRUE;
}

int control_interruptaudiodmamute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	halinterrupt_audiodmamute(baseAddr + ih_base_addr, value);
	return TRUE;
}
int control_interruptmute(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	halinterrupt_mute(baseAddr + ih_base_addr, value);
	return TRUE;
}
int control_interruptcecclear(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	halinterrupt_cecclear(baseAddr + ih_base_addr, value);
	return TRUE;
}

u8 control_interruptcecstate(u16 baseAddr)
{
	LOG_TRACE();
	return halinterrupt_cecstate(baseAddr + ih_base_addr);
}

int control_interruptedidclear(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	halinterrupt_i2cddcclear(baseAddr + ih_base_addr, value);
	return TRUE;
}

u8 control_interruptedidstate(u16 baseAddr)
{
	LOG_TRACE();
	return halinterrupt_i2cddcstate(baseAddr + ih_base_addr);
}

int control_interruptphyclear(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	halinterrupt_phyclear(baseAddr + ih_base_addr, value);
	return TRUE;
}

u8 control_interruptphystate(u16 baseAddr)
{
	LOG_TRACE();
	return halinterrupt_phystate(baseAddr + ih_base_addr);
}

u8 control_interruptaudiodmastate(u16 baseAddr)
{
	LOG_TRACE();
	return halinterrupt_audiodmastate(baseAddr + ih_base_addr);
}

int control_interruptaudiodmaclear(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	halinterrupt_audiodmaclear(baseAddr + ih_base_addr, value);
	return TRUE;
}

int control_interruptstateall(u16 baseAddr)
{
	LOG_NOTICE("---------");
	LOG_NOTICE2("audio packets   ",\
		halinterrupt_audiopacketsstate(baseAddr + ih_base_addr));
	LOG_NOTICE2("other packets   ",\
		halinterrupt_audiopacketsstate(baseAddr + ih_base_addr));
	LOG_NOTICE2("packets overflow",\
		halinterrupt_packetsoverflowstate(baseAddr + ih_base_addr));
	LOG_NOTICE2("audio sampler   ",\
		halinterrupt_audiosamplestate(baseAddr + ih_base_addr));
	LOG_NOTICE2("phy state       ",\
		halinterrupt_phystate(baseAddr + ih_base_addr));
	LOG_NOTICE2("i2c ddc state   ",\
		halinterrupt_i2cddcstate(baseAddr + ih_base_addr));
	LOG_NOTICE2("cec state       ",\
		halinterrupt_cecstate(baseAddr + ih_base_addr));
	LOG_NOTICE2("video packetizer",\
		halinterrupt_videopacketizerstate(baseAddr + ih_base_addr));
	LOG_NOTICE2("i2c phy state   ",\
		halinterrupt_i2cphystate(baseAddr + ih_base_addr));
	return TRUE;
}

int control_interruptclearall(u16 baseAddr)
{
	LOG_TRACE();
	halinterrupt_audiopacketsclear(baseAddr + ih_base_addr,	(u8)(-1));
	halinterrupt_otherpacketsclear(baseAddr + ih_base_addr,	(u8)(-1));
	halinterrupt_packetsoverflowclear(baseAddr + ih_base_addr, (u8)(-1));
	halinterrupt_audiosampleclear(baseAddr + ih_base_addr,	(u8)(-1));
	halinterrupt_phyclear(baseAddr + ih_base_addr, (u8)(-1));
	halinterrupt_i2cddcclear(baseAddr + ih_base_addr, (u8)(-1));
	halinterrupt_cecclear(baseAddr + ih_base_addr, (u8)(-1));
	halinterrupt_videopacketizerclear(baseAddr + ih_base_addr, (u8)(-1));
	halinterrupt_i2cphyclear(baseAddr + ih_base_addr, (u8)(-1));
	return TRUE;
}
