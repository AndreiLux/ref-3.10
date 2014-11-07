/*
 * @file:api.c
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include <linux/delay.h>
#include "api.h"
#include "../util/log.h"
#include "../bsp/mutex.h"
#include "../core/control.h"
#include "../core/video.h"
#include "../core/audio.h"
#include "../core/packets.h"
#include "../hdcp/hdcp.h"
#include "../edid/edid.h"
#include "../util/error.h"

#include <video/odindss.h>
#include "../../dss/dss.h"
#include "../../hdmi/hdmi.h"
#include "../../hdmi/hdmi_debug.h"
#include "../../display/hdmi-panel.h"

static u16 api_mbaseaddress = 0;
static state_t api_mcurrentstate = API_NOT_INIT;
static u8 api_mhpd = FALSE;
static handler_t api_meventregistry[DUMMY] = {NULL};
static u8 api_mdataenablepolarity = FALSE;
/* Flag to check if sending the Gamut Metadata
	Packet is not legal by the HDMI protocol */
static u8 api_msendgamutok = FALSE;
static u16 api_msfrclock = 0;
static u8 oldhpd=0;

#if defined (CONFIG_SLIMPORT_ANX7808)  || defined (CONFIG_SLIMPORT_ANX7812)
u8 edid_data_firstblock[128] = {0,};
u8 edid_data_extblock[128] = {0,};
/*
u8 edid_data_firstblock[128] = {
0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
0x1e, 0x6d, 0x93, 0x58, 0x01, 0x01, 0x01, 0x01,
0x01, 0x15, 0x01, 0x03, 0x80, 0x33, 0x1d, 0x78,
0x0a, 0xd9, 0x45, 0xa2, 0x55, 0x4d, 0xa0, 0x27,
0x12, 0x50, 0x54, 0xa5, 0x6f, 0x00, 0x71, 0x4f,
0x81, 0xc0, 0x81, 0x00, 0x81, 0x80, 0x95, 0x00,
0x90, 0x40, 0xa9, 0xc0, 0xb3, 0x00, 0x02, 0x3a,
0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
0x45, 0x00, 0xfd, 0x1e, 0x11, 0x00, 0x00, 0x1a,
0x21, 0x39, 0x90, 0x30, 0x62, 0x1a, 0x27, 0x40,
0x68, 0xb0, 0x36, 0x00, 0xfd, 0x1e, 0x11, 0x00,
0x00, 0x1c, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x38,
0x4b, 0x1e, 0x53, 0x0f, 0x00, 0x0a, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfc,
0x00, 0x44, 0x4d, 0x32, 0x33, 0x35, 0x30, 0x44,
0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x8d,
};
u8 edid_data_extblock[128] = {
0x02, 0x03, 0x33, 0xf1, 0x4e, 0x84, 0x05, 0x03,
0x02, 0x20, 0x22, 0x10, 0x11, 0x13, 0x12, 0x14,
0x1f, 0x07, 0x16, 0x26, 0x15, 0x07, 0x50, 0x09,
0x07, 0x07, 0x78, 0x03, 0x0c, 0x00, 0x10, 0x00,
0x80, 0x2d, 0x20, 0xc0, 0x0e, 0x01, 0x40, 0x0a,
0x3c, 0x08, 0x10, 0x18, 0x10, 0x98, 0x10, 0x58,
0x10, 0x38, 0x10, 0x01, 0x1d, 0x00, 0x72, 0x51,
0xd0, 0x1e, 0x20, 0x38, 0x88, 0x15, 0x00, 0x56,
0x50, 0x21, 0x00, 0x00, 0x1e, 0x01, 0x1d, 0x80,
0xd0, 0x72, 0x1c, 0x16, 0x20, 0x10, 0x2c, 0x25,
0x80, 0xc4, 0x8e, 0x21, 0x00, 0x00, 0x9e, 0x02,
0x3a, 0x80, 0xd0, 0x72, 0x38, 0x2d, 0x40, 0x10,
0x2c, 0x45, 0x20, 0x06, 0x44, 0x21, 0x00, 0x00,
0x1e, 0x02, 0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d,
0x40, 0x58, 0x2c, 0x45, 0x00, 0x56, 0x50, 0x21,
0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0xf7,
};*/
#endif

#ifdef QUEUE_USE_CASE
void api_do_apply(struct work_struct *work)
{
	struct api_cb_work *wk = container_of(work, typeof(*wk), work);

	hdmi_debug(LOG_DEBUG,"api_do_apply %d\n", wk->event);

	api_meventregistry[wk->event](&api_mhpd);
}
#endif


static int api_checkparamsvideo(videoParams_t * video);
static int api_checkparamsaudio(audioParams_t * audio);

int api_deinitialize(void)
{
	if (odin_crg_unregister_isr(
		(odin_crg_isr_t)api_eventhandler, NULL, CRG_IRQ_HDMI, 0) == 0)
		return true;
	return false;
}
int api_initialize(
	u16 address, u8 dataEnablePolarity, u16 sfrClock, u8 fixed_color_screen)
{
	dtd_t dtd;
	videoParams_t params;
	u16 pixelclock = 0;
	api_mdataenablepolarity = dataEnablePolarity;
	api_mhpd = FALSE;
	api_msendgamutok = FALSE;
	api_msfrclock = sfrClock;

	hdmi_debug(LOG_INFO,"dwc_hdmi_tx_software_api_2.12");
	hdmi_debug(LOG_INFO,__DATE__);
	LOG_TRACE1(dataEnablePolarity);
	/* reset error */
	error_set(NO_ERROR);


	api_mbaseaddress = address;
	control_interruptmute(api_mbaseaddress, ~0); /* disable interrupts */
	if ((api_mcurrentstate < API_HPD) || (api_mcurrentstate > API_EDID_READ))
	{
		/* if EDID read do not re-read, if HPDetected, keep in same state
		 * otherwise, NOT INIT */
		api_mcurrentstate = API_NOT_INIT;
	}
	/* VGA must be supported by all sinks
	 * so use it as default configuration
	 */
	dtd_fill(&dtd, 1, 60000); /* VGA */
	videoparams_reset(&params);
	videoparams_sethdmi(&params, FALSE); /* DVI */
	videoparams_setdtd(&params, &dtd);
	pixelclock = videoparams_getpixelclock(&params);

	hdmi_debug(LOG_INFO, "Product Line", control_productline(api_mbaseaddress));
	hdmi_debug(LOG_INFO, "Product Type", control_producttype(api_mbaseaddress));
	if (control_supportscore(api_mbaseaddress) != TRUE)
	{
		error_set(ERR_HW_NOT_SUPPORTED);
		hdmi_err("Unknown device: aborting...");
		return FALSE;
	}
	hdmi_debug(LOG_INFO,"HDMI TX Controller Design", control_design(api_mbaseaddress));
	hdmi_debug(LOG_INFO,"HDMI TX Controller Revision", control_revision(api_mbaseaddress));


	if (control_supportshdcp(api_mbaseaddress) == TRUE)
	{
		hdmi_debug(LOG_INFO,"HDMI TX controller supports HDCP");
	}


	if (video_initialize(api_mbaseaddress, &params, api_mdataenablepolarity)
			!= TRUE)
	{
		return FALSE;
	}


	if (audio_initialize(api_mbaseaddress) != TRUE)
	{
		return FALSE;
	}


	if (packets_initialize(api_mbaseaddress) != TRUE)
	{
		return FALSE;
	}


	if (hdcp_initialize(api_mbaseaddress, api_mdataenablepolarity)
			!= TRUE)
	{
		return FALSE;
	}

	if (control_initialize(api_mbaseaddress, api_mdataenablepolarity,
				pixelclock) != TRUE)
	{
		return FALSE;
	}


	if (phy_initialize(api_mbaseaddress, api_mdataenablepolarity) != TRUE)
	{
		return FALSE;
	}

	control_interruptclearall(api_mbaseaddress);


	if (video_configure(api_mbaseaddress, &params,
		api_mdataenablepolarity, FALSE, fixed_color_screen) != TRUE)
	{
		return FALSE;
	}
	if ((api_mcurrentstate < API_HPD) || (api_mcurrentstate > API_EDID_READ))
	{
		/* if EDID read do not re-read, if HPDetected, keep in same state
		 * otherwise, set to INIT */
		api_mcurrentstate = API_INIT;
	}
	/*
	 * By default no pixel repetition and color resolution is 8
	 */
	if (control_configure(api_mbaseaddress, pixelclock, 0, 8, 0, 0, 0, 0) != TRUE)
	{
		return FALSE;
	}
	/* send AVMUTE SET (optional) */
	if (phy_configure (api_mbaseaddress, pixelclock, 8, 0) != TRUE)
	{
		hdmi_err("opps --> phy configure error\n");
		return FALSE;
	}

	phy_enablehpdsense(api_mbaseaddress); /* enable HPD sending on phy */
	phy_interruptenable(api_mbaseaddress, ~0x02);
	if (0)
	{
		hdmi_debug(LOG_DEBUG,"init force");
		api_mcurrentstate = API_HPD;
	}
	else
	{
		hdmi_debug(LOG_DEBUG,"Waiting for hot plug detection...");
	}

	control_interruptaudiosamplermute(api_mbaseaddress, ~0);
	control_interruptvideopacketizermute(api_mbaseaddress, ~0);
	control_interrupti2cphymute(api_mbaseaddress, ~0);
	/*control_interruptphymute(api_mbaseaddress, ~0);*/
	/* enable interrupts */
	control_interruptmute(api_mbaseaddress, 0);

#if 0
	if (odin_crg_register_isr(
		(odin_crg_isr_t)api_eventhandler, NULL, CRG_IRQ_HDMI, 0) == 0)
		return true;
#endif

	return true;
}



int api_configure(videoParams_t * video, audioParams_t * audio,
		productParams_t * product, hdcpParams_t * hdcp, u8 fixed_color_screen)
{
	hdmivsdb_t vsdb;
	int audioon = 0;
	int success = TRUE;
	int hdcpon;
	hdmi_api_enter();

	hdcpon = (hdcp != 0 &&
		(control_supportshdcp(api_mbaseaddress) == TRUE)) ? TRUE : FALSE;
	if (video == NULL)
	{
		error_set(ERR_HPD_LOST);
		LOG_ERROR("video params invalid");
		return FALSE;
	}
	audioon = (audio != 0 && videoparams_gethdmi(video));

	if (api_mcurrentstate < API_HPD)
	{
		error_set(ERR_HPD_LOST);
		LOG_ERROR("cable not connected");
		/* return FALSE; */
	}
	else if (api_mcurrentstate == API_HPD)
	{
		hdmi_debug(LOG_DEBUG,"E-EDID not read. Media may not be supported by sink");
	}

	if (api_mcurrentstate == API_EDID_READ)
	{

		api_checkparamsvideo(video);
		if (api_edidhdmivsdb(&vsdb))
		{
			if (!hdmivsdb_getdeepcolor30(&vsdb) && !hdmivsdb_getdeepcolor36(&vsdb)
					&& !hdmivsdb_getdeepcolor48(&vsdb))
			{
				videoparams_setcolorresolution(video, 8);
			}
		}
	}
	control_interruptmute(api_mbaseaddress, ~0); /* disable interrupts */
	do
	{
		if (video_configure(api_mbaseaddress, video, api_mdataenablepolarity,
				hdcpon, fixed_color_screen) != TRUE)
		{
			success = FALSE;
			hdmi_debug(LOG_DEBUG,"video configure failed\n");
			break;
		}
		if (audioon)
		{
			if (api_mcurrentstate == API_EDID_READ)
			{
				api_checkparamsaudio(audio);
			}
			if (audio_configure(api_mbaseaddress, audio,
				videoparams_getpixelclock(video), videoparams_getratioclock(video)) != TRUE)
			{
				success = FALSE;
				hdmi_debug(LOG_DEBUG,"audio configure failed\n");
				break;
			}
			packets_audioinfoframe(api_mbaseaddress, audio);
		}
		else if (audio != 0 && videoparams_gethdmi(video) != TRUE)
		{
			hdmi_debug(LOG_DEBUG,"DVI mode selected: audio not configured");
		}
		else
		{
			hdmi_debug(LOG_DEBUG,"No audio parameters provided: not configured");
		}
		if (videoparams_gethdmi(video) == TRUE)
		{
			if (packets_configure(api_mbaseaddress, video, product) != TRUE)
			{
				success = FALSE;
				hdmi_debug(LOG_DEBUG,"packet configure failed\n");
				break;
			}
			api_msendgamutok = (videoparams_getencodingout(video) == YCC444)
					|| (videoparams_getencodingout(video) == YCC422);
			api_msendgamutok = api_msendgamutok && (videoparams_getcolorimetry(
					video) == EXTENDED_COLORIMETRY);
		}
		else
		{
			hdmi_debug(LOG_DEBUG,"DVI mode selected: packets not configured");
		}
		if (hdcpon == TRUE)
		{	/* HDCP is PHY independent */
//			hdmi_rxkeywrite(api_mbaseaddress);
			if (hdcp_configure(api_mbaseaddress, hdcp, videoparams_gethdmi(video),
					dtd_gethsyncpolarity(videoparams_getdtd(video)),
					dtd_getvsyncpolarity(videoparams_getdtd(video))) == FALSE)
			{
				hdmi_debug(LOG_DEBUG,"HDCP not configured");
				hdcpon = FALSE;
				success = FALSE;
				break;
			}
		}
		else if (hdcp != 0 && control_supportshdcp(api_mbaseaddress) != TRUE)
		{
			hdmi_debug(LOG_DEBUG,"HDCP is not supported by hardware");
		}
		else
		{
			hdmi_debug(LOG_DEBUG,"No HDCP parameters provided: not configured");
		}
/*		if (board_pixelclock(api_mbaseaddress, videoparams_getpixelclock(video),
				videoparams_getcolorresolution(video)) != TRUE)
		{
			success = FALSE;
			break;
		} */
		if (control_configure(api_mbaseaddress, videoparams_getpixelclock(video),
				videoparams_getpixelrepetitionfactor(video),
				videoparams_getcolorresolution(video),
				videoparams_iscolorspaceconversion(video), audioon, FALSE, hdcpon)
				!= TRUE)
		{
			hdmi_debug(LOG_DEBUG,"control configure break\n");
			success = FALSE;
			break;

		}
		if (phy_configure(api_mbaseaddress, videoparams_getpixelclock(video),
			videoparams_getcolorresolution(video),
			videoparams_getpixelrepetitionfactor(video)) != TRUE)
		{
			hdmi_debug(LOG_DEBUG,"phy configure break\n");
			success = FALSE;
			break;
		}
		/* disable blue screen transmission after
		turning on all necessary blocks (e.g. HDCP) */
		if (video_forceoutput(api_mbaseaddress, FALSE) != TRUE)
		{
			success = FALSE;
			break;
		}
		/* reports HPD state to HDCP */
		if (hdcpon)
		{
			hdcp_rxdetected(api_mbaseaddress, phy_hotplugdetected(
						api_mbaseaddress));
		}
		/* send AVMUTE CLEAR (optional) */
		api_mcurrentstate = API_CONFIGURED;
	}
	while (0);
	control_interruptaudiopacketsmute(api_mbaseaddress, ~0);
	control_interruptotherpacketsmute(api_mbaseaddress, ~0);
	control_interruptpacketsoverflowmute(api_mbaseaddress, 0x3);
	control_interruptmute(api_mbaseaddress, 0); /* enable interrupts */

	return success;
}

int api_standby()
{
	hdmi_api_enter();
	system_intrruptdisable(TX_INT);
	system_interrupthandlerunregister(TX_INT);
	control_standby(api_mbaseaddress);
	phy_standby(api_mbaseaddress);
	audio_dmainterruptenable(api_mbaseaddress, ~0);

	return TRUE;
}

u8 api_coreread(u16 addr)
{
	return access_corereadbyte(addr);
}

void api_corewrite(u8 data, u16 addr)
{
	access_corewritebyte(data, addr);
}

u8 hdmi_get_irq_status(void)
{
	u8 reg;
	reg = control_interruptphystate(api_mbaseaddress);

	return reg;
}

unsigned int hdmi_clear_irq(u8 reg_val)
{
	u8 state = 0;

	oldhpd = (api_mcurrentstate >= API_HPD);
	api_mhpd = (phy_hotplugdetected(api_mbaseaddress) > 0);


	/* if HPD always enter - all states */
	state = control_interruptphystate(api_mbaseaddress);
	control_interruptphyclear(api_mbaseaddress, state);
	if ((state != 0) || (api_mhpd != oldhpd))
	{
		if (api_mhpd && !oldhpd)
		{
			api_mcurrentstate = API_HPD;
			hdmi_debug(LOG_INFO,"[HDMI API] hot plug detected");
		}
		else if (!api_mhpd && oldhpd)
		{
			api_mcurrentstate = API_NOT_INIT;
			hdmi_debug(LOG_INFO,"[HDMI API] hot plug lost");
		}
		/* report HPD state to HDCP (after configuration) */

		if (api_mcurrentstate == API_CONFIGURED)
		{
			LOG_ERROR("rxdetected");
			hdcp_rxdetected(api_mbaseaddress, api_mhpd);
		}
	}
	return 0;
}

int api_read_edid(void)
{
	int ret;

	edid_standby(api_mbaseaddress);
	hdmi_read_edid_block(0, edid_data_firstblock);
	hdmi_read_edid_block(1, edid_data_extblock);

	if (edid_slimporttohdmi(api_mbaseaddress, edid_data_firstblock,
				edid_data_extblock) == EDID_DONE)
	{
		ret = 0;
	}
	else
		ret = -1;

	return ret;

}


void api_eventhandler(void * param)
{
	int ret = 0;
	u8 i = 0;
	u8 state = 0;
	u8 handled = TRUE;
	hdmi_api_enter();

	hdmi_debug(LOG_INFO,"api_eventhandler  call in");

	if(api_mcurrentstate == API_HPD)
	{
		hdmi_debug(LOG_INFO,"hpd ---> start main proc");


	}else if(api_mcurrentstate == API_NOT_INIT)
	{
		hdmi_debug(LOG_INFO,"unplug ---> stop proc");

	}
	else{

		hdmi_debug(LOG_DEBUG,"oops ---> %d\n", api_mcurrentstate);
	}
#if 0
	if(api_mcurrentstate != API_NOT_INIT)
	{
		if (api_mcurrentstate >= API_HPD)
		{

			/* only report if state is HPD, EDID_READ or CONFIGURED
			   for modules that can be enabled anytime there is HPD */
			/* EDID module */
			state = control_interruptedidstate(api_mbaseaddress);
			if ((state != 0) || (api_mhpd != oldhpd))
			{
				control_interruptedidclear(api_mbaseaddress, state);
#if defined (CONFIG_SLIMPORT_ANX7808)  || defined (CONFIG_SLIMPORT_ANX7812)

				hdmi_debug(LOG_INFO,"[HDMI API] EDID READ");
				edid_standby(api_mbaseaddress);
				hdmi_read_edid_block(0, edid_data_firstblock);
				hdmi_read_edid_block(1, edid_data_extblock);

				if (edid_slimporttohdmi(api_mbaseaddress, edid_data_firstblock,
							edid_data_extblock) == EDID_DONE)
				{
					api_mcurrentstate = API_EDID_READ;
					if (api_meventregistry[EDID_READ_EVENT] != NULL)
					{
						hdmi_debug(LOG_DEBUG,"api call edid read event\n");
						api_meventregistry[EDID_READ_EVENT](&api_mhpd);
					}
					else{
						hdmi_debug(LOG_DEBUG,"force call edid read event\n");
					/* hdmi_edid_done(); */
					}
				}
#else /* for direct hdmi-output test */
				if (edid_eventhandler(api_mbaseaddress, api_mhpd, state) == EDID_DONE)
				{
					edid_standby(api_mbaseaddress);
					api_mcurrentstate = API_EDID_READ;
					if (api_meventregistry[EDID_READ_EVENT] != NULL)
					{
						api_meventregistry[EDID_READ_EVENT](&api_mhpd);
					}
				}
#endif
				/* even if it is an error reading, it is has been handled!*/
				handled = TRUE;
			}
		}
	}
#endif
	state = control_interruptaudiodmastate(api_mbaseaddress);
	for (i = HPD_EVENT; i < DUMMY; i++)
	{
		/* call registered user callbacks */
		if (api_meventregistry[i] != NULL)
		{
			if ((i == HPD_EVENT) && (api_mhpd != oldhpd))
			{
				api_meventregistry[i](&api_mhpd);
			}
			else if ((i == DMA_DONE) && (state & (1 << 7)))
			{
				api_meventregistry[i](&api_mhpd);
			}
			else if ((i == DMA_ERROR) && (state & (1 << 4)))
			{
				api_meventregistry[i](&api_mhpd);
			}

		}
	}
	if (!handled)
	{
		/* hdmi_debug(LOG_DEBUG,"interrupt not handled"); */
	}
	system_interruptacknowledge();
}

int api_eventenable(event_t idEvent, handler_t handler, u8 oneshot)
{
	u8 retval;
	hdmi_api_enter();
	retval = idEvent < DUMMY;
	if (retval)
	{
		api_meventregistry[idEvent] = handler;
		/* enable hardware */
		switch (idEvent)
		{
			case DMA_DONE:
				audio_dmainterruptenable(api_mbaseaddress, (1 << 7) |
					audio_dmainterruptenablestatus(api_mbaseaddress)); /* DONE unmask */
				break;
			case DMA_ERROR:
				audio_dmainterruptenable(api_mbaseaddress, (1 << 4) |
					audio_dmainterruptenablestatus(api_mbaseaddress)); /* ERROR unmask */
				break;
			default:
				retval = FALSE;
				break;
		}
	}
	return retval;
}

int api_eventclear(event_t idEvent)
{
	u8 retval;
	hdmi_api_enter();
	retval = idEvent < DUMMY;
	if (retval)
	{	/* clear hardware */
		switch (idEvent)
		{
			case DMA_DONE:
				control_interruptaudiodmaclear(api_mbaseaddress, (1 << 7));
				break;
			case DMA_ERROR:
				control_interruptaudiodmaclear(api_mbaseaddress, (1 << 4));
				break;
			default:
				break;
		}
	}
	return retval;
}

int api_eventdisable(event_t idEvent)
{
	u8 retval;
	hdmi_api_enter();
	retval = idEvent < DUMMY;
	if (retval)
	{	/* disable hardware */
		switch (idEvent)
		{
			case DMA_DONE:
				audio_dmainterruptenable(api_mbaseaddress, ~(1 << 7) &
					audio_dmainterruptenablestatus(api_mbaseaddress)); /* DONE mask */
				break;
			case DMA_ERROR:
				audio_dmainterruptenable(api_mbaseaddress, ~(1 << 4) &
					audio_dmainterruptenablestatus(api_mbaseaddress)); /* ERROR mask */
				break;
			default:
				break;
		}
		api_meventregistry[idEvent] = NULL;
	}
	return retval;
}
int api_edidread(handler_t handler)
{
	hdmi_debug(LOG_DEBUG,"api_mcurrentstate = %d\n", api_mcurrentstate);
	if (api_mcurrentstate >= API_HPD)
	{
		hdmi_debug(LOG_DEBUG,"api edid read handler set = %d\n", api_mcurrentstate);
		api_meventregistry[EDID_READ_EVENT] = handler;

#if !(defined(CONFIG_SLIMPORT_ANX7808) || defined(CONFIG_SLIMPORT_ANX7812))
		if (edid_initialize(api_mbaseaddress, api_msfrclock) == TRUE)
		{
			return TRUE;
		}
#endif
	}
#if !(defined(CONFIG_SLIMPORT_ANX7808) || defined(CONFIG_SLIMPORT_ANX7812))
	LOG_ERROR("cannot read EDID");
	return FALSE;
#endif
	return TRUE;
}

int api_ediddtdcount()
{
	hdmi_api_enter();
	return edid_getdtdcount(api_mbaseaddress);
}

int api_ediddtd(unsigned int n, dtd_t * dtd)
{
	hdmi_api_enter();
	return edid_getdtd(api_mbaseaddress, n, dtd);
}

int api_edidhdmivsdb(hdmivsdb_t * vsdb)
{
	hdmi_api_enter();
	return edid_gethdmivsdb(api_mbaseaddress, vsdb);
}

int api_edidmonitorname(char * name, unsigned int length)
{
	hdmi_api_enter();
	return edid_getmonitorname(api_mbaseaddress, name, length);
}

int api_edidmonitorrangelimits(monitorRangeLimits_t * limits)
{
	hdmi_api_enter();
	return edid_getmonitorrangelimits(api_mbaseaddress, limits);
}

int api_edidsvdcount()
{
	hdmi_api_enter();
	return edid_getsvdcount(api_mbaseaddress);
}

int api_edidsvd(unsigned int n, shortVideoDesc_t * svd)
{
	hdmi_api_enter();
	return edid_getsvd(api_mbaseaddress, n, svd);
}

int api_edidsadcount()
{
	hdmi_api_enter();
	return edid_getsadcount(api_mbaseaddress);
}

int api_edidsad(unsigned int n, shortAudioDesc_t * sad)
{
	hdmi_api_enter();
	return edid_getsad(api_mbaseaddress, n, sad);
}

int api_edidvideocapabilitydatablock(videoCapabilityDataBlock_t * capability)
{
	hdmi_api_enter();
	return edid_getvideocapabilitydatablock(api_mbaseaddress, capability);
}

int api_edidspeakerallocationdatablock(
		speakerAllocationDataBlock_t * allocation)
{
	hdmi_api_enter();
	return edid_getspeakerallocationdatablock(api_mbaseaddress, allocation);
}

int api_edidcolorimetrydatablock(colorimetryDataBlock_t * colorimetry)
{
	hdmi_api_enter();
	return edid_getcolorimetrydatablock(api_mbaseaddress, colorimetry);
}

int api_edidsupportsbasicaudio()
{
	hdmi_api_enter();
	return edid_supportsbasicaudio(api_mbaseaddress);
}

int api_edidsupportsunderscan()
{
	hdmi_api_enter();
	return edid_supportsunderscan(api_mbaseaddress);
}

int api_edidsupportsycc422()
{
	hdmi_api_enter();
	return edid_supportsycc422(api_mbaseaddress);
}

int api_edidsupportsycc444()
{
	hdmi_api_enter();
	return edid_supportsycc444(api_mbaseaddress);
}

void api_packetaudiocontentprotection(u8 type, u8 * fields, u8 length,
		u8 autoSend)
{
	hdmivsdb_t vsdb;

	hdmi_api_enter();
	/* check if sink supports ACP packets */
	if (api_edidhdmivsdb(&vsdb))
	{
		if (!hdmivsdb_getsupportsai(&vsdb))
		{
			LOG_ERROR("sink does NOT support ACP");
		}
	}
	else
	{
		hdmi_debug(LOG_DEBUG,"sink is NOT HDMI compliant: ACP may not work");
	}
	packets_audiocontentprotection(api_mbaseaddress, type, fields, length,
			autoSend);
}

void api_packetsisrc(u8 initStatus, u8 * codes, u8 length, u8 autoSend)
{
	hdmivsdb_t vsdb;

	hdmi_api_enter();
	/* check if sink supports ISRC packets */
	if (api_edidhdmivsdb(&vsdb))
	{
		if (!hdmivsdb_getsupportsai(&vsdb))
		{
			LOG_ERROR("sink does NOT support ISRC");
		}
	}
	else
	{
		hdmi_debug(LOG_DEBUG,"sink is NOT HDMI compliant: ISRC may not work");
	}
	packets_isrcpackets(api_mbaseaddress, initStatus, codes, length, autoSend);
}

void api_packetsisrcstatus(u8 status)
{
	hdmi_api_enter();
	packets_isrcstatus(api_mbaseaddress, status);
}

void api_packetsgamutmetadata(u8 * gbdContent, u8 length)
{
	hdmi_api_enter();
	if (!api_msendgamutok)
	{
		hdmi_debug(LOG_DEBUG,"Gamut packets will be discarded by sink:\
			Video Out must be YCbCr with Extended Colorimetry");
	}
	packets_gamutmetadatapackets(api_mbaseaddress, gbdContent, length);
}

void api_packetsstopsendacp()
{
	hdmi_api_enter();
	packets_stopsendacp(api_mbaseaddress);
}

void api_packetsstopsendisrc()
{
	hdmi_api_enter();
	packets_stopsendisrc1(api_mbaseaddress);
}

void api_packetsstopsendspd()
{
	hdmi_api_enter();
	packets_stopsendspd(api_mbaseaddress);
}

void api_packetsstopsendvsd()
{
	hdmi_api_enter();
	packets_stopsendvsd(api_mbaseaddress);
}

void api_avmute(int enable)
{
	LOG_TRACE1(enable);
	if (enable == TRUE)
	{
		packets_avmute(api_mbaseaddress, enable);
		hdcp_avmute(api_mbaseaddress, enable);
	}
	else
	{
		hdcp_avmute(api_mbaseaddress, enable);
		packets_avmute(api_mbaseaddress, enable);
	}
}

void api_audiomute(int enable)
{
	LOG_TRACE1(enable);
	audio_mute(api_mbaseaddress, enable);
}

void api_audiodmarequestaddress(u32 startAddress, u32 stopAddress)
{
	if (startAddress != stopAddress)
	{
		audio_dmarequestaddress(api_mbaseaddress, startAddress, stopAddress);
	}
}
u16 api_audiodmagetcurrentburstlength()
{
	return audio_dmagetcurrentburstlength(api_mbaseaddress);
}
u32 api_audiodmagetcurrentoperationaddress()
{
	return audio_dmagetcurrentoperationaddress(api_mbaseaddress);
}
void api_audiodmastartread()
{
	audio_dmastartread(api_mbaseaddress);
}
void api_audiodmastopread()
{
	audio_dmastopread(api_mbaseaddress);
}

int api_hdcpengaged()
{
	hdmi_api_enter();
	return hdcp_engaged(api_mbaseaddress);
}

u8 api_hdcpauthenticationstate()
{
	hdmi_api_enter();
	return hdcp_authenticationstate(api_mbaseaddress);
}

u8 api_hdcpcipherstate()
{
	hdmi_api_enter();
	return hdcp_cipherstate(api_mbaseaddress);
}

u8 api_hdcprevocationstate()
{
	hdmi_api_enter();
	return hdcp_revocationstate(api_mbaseaddress);
}

int api_hdcpbypassencryption(int bypass)
{
	LOG_TRACE1(bypass);
	hdcp_bypassencryption(api_mbaseaddress, bypass);
	return TRUE;
}

int api_hdcpdisableencryption(int disable)
{
	LOG_TRACE1(disable);
	hdcp_disableencryption(api_mbaseaddress, disable);
	return TRUE;
}

int api_hdcpsrmupdate(const u8 * data)
{
	hdmi_api_enter();
	return hdcp_srmupdate(api_mbaseaddress, data);
}

int api_hotplugdetected()
{
	return phy_hotplugdetected(api_mbaseaddress);
}

u8 api_readconfig()
{
	u16 addr = 0;
	hdmi_api_enter();
	log_setverbosewrite(1);
	for (addr = 0; addr < 0x8000; addr++)
	{
		if ((addr == 0x01FF || addr <= 0x0003) || /* ID */
		(addr >= 0x0100 && addr <= 0x0107) || /* IH */
		(addr >= 0x0200 && addr <= 0x0207) || /* TX */
		(addr >= 0x0800 && addr <= 0x0808) || /* VP */
		(addr >= 0x1000 && addr <= 0x10E0) || /* FC */
		(addr >= 0x1100 && addr <= 0x1120) || /* FC */
		(addr >= 0x1200 && addr <= 0x121B) || /* FC */
		(addr >= 0x3000 && addr <= 0x3007) || /* PHY */
		(addr >= 0x3100 && addr <= 0x3102) || /* AUD */
		(addr >= 0x3200 && addr <= 0x3206) || /* AUD */
		(addr >= 0x3300 && addr <= 0x3302) || /* AUD */
		(addr >= 0x3400 && addr <= 0x3404) || /* AUD */
		(addr >= 0x4000 && addr <= 0x4006) || /* MC */
		(addr >= 0x4100 && addr <= 0x4119) || /* CSC */
		(addr >= 0x5000 && addr <= 0x501A) || /* A */
		(addr >= 0x7000 && addr <= 0x7026) || /* VG */
		(addr >= 0x7100 && addr <= 0x7116) || /* AG */
		(addr >= 0x7D00 && addr <= 0x7D31) || /* CEC */
		(addr >= 0x7E00 && addr <= 0x7E0A)) /* EDID */
		{
			/* SRM and I2C slave not required */
			LOG_WRITE(addr, access_corereadbyte(addr));
		}
	}
	log_setverbosewrite(0);
	return TRUE;
}

/**
 * Check audio parameters against read EDID structure to ensure
 * compatibility with sink.
 * @param audio audio parameters data structure
 */
static int api_checkparamsaudio(audioParams_t * audio)
{
	unsigned i = 0;
	int bitsupport = -1;
	shortAudioDesc_t sad;
	speakerAllocationDataBlock_t allocation;
	u8 errorcode = TRUE;
	int valid = FALSE;
	/* array to translate from AudioInfoFrame code to
		EDID Speaker Allocation data block bits */
	const u8 sadb[] =
	{ 1, 3, 5, 7, 17, 19, 21, 23, 9, 11, 13, 15, 25, 27, 29, 31, 73, 75, 77,
			79, 33, 35, 37, 39, 49, 51, 53, 55, 41, 43, 45, 47 };

	hdmi_api_enter();
	if (!api_edidsupportsbasicaudio())
	{
		error_set(ERR_SINK_DOES_NOT_SUPPORT_AUDIO);
		hdmi_debug(LOG_DEBUG,"Sink does NOT support audio");
		return FALSE;
	}
	/* check if audio type supported */
	for (i = 0; i < api_edidsadcount(); i++)
	{
		api_edidsad(i, &sad);
		if (audioparams_getcodingtype(audio) == shortaudiodesc_getformatcode(&sad))
		{
			bitsupport = i;
			break;
		}
	}
	if (bitsupport >= 0)
	{
		api_edidsad(bitsupport, &sad);
		/* 192 kHz| 176.4 kHz| 96 kHz| 88.2 kHz| 48 kHz| 44.1 kHz| 32 kHz */
		switch (audioparams_getsamplingfrequency(audio))
		{
		case 32000:
			valid = shortaudiodesc_support32k(&sad);
			break;
		case 44100:
			valid = shortaudiodesc_support44k1(&sad);
			break;
		case 48000:
			valid = shortaudiodesc_support48k(&sad);
			break;
		case 88200:
			valid = shortaudiodesc_support88k2(&sad);
			break;
		case 96000:
			valid = shortaudiodesc_support96k(&sad);
			break;
		case 176400:
			valid = shortaudiodesc_support176k4(&sad);
			break;
		case 192000:
			valid = shortaudiodesc_support192k(&sad);
			break;
		default:
			valid = FALSE;
			break;
		}
		if (!valid)
		{
			error_set(ERR_SINK_DOES_NOT_SUPPORT_AUDIO_SAMPLING_PREQ);
			hdmi_debug(LOG_DEBUG,"Sink does NOT support audio sampling frequency",
				audioparams_getsamplingfrequency(audio));
			errorcode = FALSE;
		}
		if (audioparams_getcodingtype(audio) == PCM)
		{
			/* 24 bit| 20 bit| 16 bit */
			switch (audioparams_getsamplesize(audio))
			{
			case 16:
				valid = shortaudiodesc_support16bit(&sad);
				break;
			case 20:
				valid = shortaudiodesc_support20bit(&sad);
				break;
			case 24:
				valid = shortaudiodesc_support24bit(&sad);
				break;
			default:
				valid = FALSE;
				break;
			}
			if (!valid)
			{
				error_set(ERR_SINK_DOES_NOT_SUPPORT_AUDIO_SAMPLE_SIZE);
				hdmi_debug(LOG_DEBUG,"Sink does NOT support audio sample size",
					audioparams_getsamplesize(audio));
				errorcode = FALSE;
			}
		}
		/* check Speaker Allocation */
		if (api_edidspeakerallocationdatablock(&allocation) == TRUE)
		{
			LOG_DEBUG2("Audio channel allocation sent",
				sadb[audioparams_getchannelallocation(audio)]);
			LOG_DEBUG2("Audio channel allocation accepted",
				speakerallocationdatablock_getspeakerallocationbyte(&allocation));
			valid = (sadb[audioparams_getchannelallocation(audio)]
					& speakerallocationdatablock_getspeakerallocationbyte(
							&allocation))
					== sadb[audioparams_getchannelallocation(audio)];
			if (!valid)
			{
				error_set(ERR_SINK_DOES_NOT_SUPPORT_ATTRIBUTED_CHANNELS);
				hdmi_debug(LOG_DEBUG,"Sink does NOT have attributed speakers");
				errorcode = FALSE;
			}
		}
	}
	else
	{
		error_set(ERR_SINK_DOES_NOT_SUPPORT_AUDIO_TYPE);
		hdmi_debug(LOG_DEBUG,"Sink does NOT support audio type");
		errorcode = FALSE;
	}
	return errorcode;
}

/**
 * Check Video parameters against allowed values and read EDID structure
 * to ensure compatibility with sink.
 * @param video video parameters
 */
static int api_checkparamsvideo(videoParams_t * video)
{
	u8 errorcode = TRUE;
	hdmivsdb_t vsdb;
	dtd_t dtd;
	unsigned i = 0;
	shortVideoDesc_t svd;
	videoCapabilityDataBlock_t capability;
	int valid = FALSE;
	colorimetryDataBlock_t colorimetry;

	hdmi_api_enter();
	colorimetrydatablock_reset(&colorimetry);
	shortvideodesc_reset(&svd);
	videocapabilitydatablock_reset(&capability);

	if (videoparams_gethdmi(video))
	{ /* HDMI mode */
		if (api_edidhdmivsdb(&vsdb) == TRUE)
		{
			if (videoparams_getcolorresolution(video) == 10
					&& !hdmivsdb_getdeepcolor30(&vsdb))
			{
				error_set(ERR_SINK_DOES_NOT_SUPPORT_30BIT_DC);
				hdmi_debug(LOG_DEBUG,"Sink does NOT support 30bits/pixel deep colour mode");
				errorcode = FALSE;
			}
			else if (videoparams_getcolorresolution(video) == 12
					&& !hdmivsdb_getdeepcolor36(&vsdb))
			{
				error_set(ERR_SINK_DOES_NOT_SUPPORT_36BIT_DC);
				hdmi_debug(LOG_DEBUG,"Sink does NOT support 36bits/pixel deep colour mode");
				errorcode = FALSE;
			}
			else if (videoparams_getcolorresolution(video) == 16
					&& !hdmivsdb_getdeepcolor48(&vsdb))
			{
				error_set(ERR_SINK_DOES_NOT_SUPPORT_48BIT_DC);
				hdmi_debug(LOG_DEBUG,"Sink does NOT support 48bits/pixel deep colour mode");
				errorcode = FALSE;
			}
			if (videoparams_getencodingout(video) == YCC444
					&& !hdmivsdb_getdeepcolory444(&vsdb))
			{
				error_set(ERR_SINK_DOES_NOT_SUPPORT_YCC444_DC);
				hdmi_debug(LOG_DEBUG,"Sink does NOT support YCC444 in deep colour mode");
				errorcode = FALSE;
			}
			if (videoparams_gettmdsclock(video) > 16500)
			{
				/*
				 * Sink must specify MaxTMDSClk when supports frequencies over 165MHz
				 * GetMaxTMDSClk() is TMDS clock / 5MHz and GetTmdsClock() returns [0.01MHz]
				 * so GetMaxTMDSClk() value must be multiplied by 500
				 * in order to be compared
				 */
				if (videoparams_gettmdsclock(video) > (hdmivsdb_getmaxtmdsclk(
						&vsdb) * 500))
				{
					error_set(ERR_SINK_DOES_NOT_SUPPORT_TMDS_CLOCK);
					hdmi_debug(LOG_DEBUG,"Sink does not support TMDS clock",
						videoparams_gettmdsclock(video));
					hdmi_debug(LOG_DEBUG,"Maximum TMDS clock supported is",
						hdmivsdb_getmaxtmdsclk(&vsdb) * 500);
					errorcode = FALSE;
				}
			}
		}
		else
		{
			error_set(ERR_SINK_DOES_NOT_SUPPORT_HDMI);
			hdmi_debug(LOG_DEBUG,"Sink does not support HDMI");
			errorcode = FALSE;
		}
		/* video out encoding (YCC/RGB) */
		if (videoparams_getencodingout(video) == YCC444
				&& !api_edidsupportsycc444())
		{
			error_set(ERR_SINK_DOES_NOT_SUPPORT_YCC444_ENCODING);
			hdmi_debug(LOG_DEBUG,"Sink does NOT support YCC444 encoding");
			errorcode = FALSE;
		}
		else if (videoparams_getencodingout(video) == YCC422
				&& !api_edidsupportsycc422())
		{
			error_set(ERR_SINK_DOES_NOT_SUPPORT_YCC422_ENCODING);
			hdmi_debug(LOG_DEBUG,"Sink does NOT support YCC422 encoding");
			errorcode = FALSE;
		}
		/* check extended colorimetry data and if supported by sink */
		if (videoparams_getcolorimetry(video) == EXTENDED_COLORIMETRY)
		{
			if (api_edidcolorimetrydatablock(&colorimetry) == TRUE)
			{
				if (!colorimetrydatablock_supportsxvycc601(&colorimetry)
						&& videoparams_getextcolorimetry(video) == 0)
				{
					error_set(ERR_SINK_DOES_NOT_SUPPORT_XVYCC601_COLORIMETRY);
					hdmi_debug(LOG_DEBUG,"Sink does NOT support xvYcc601 extended colorimetry");
					errorcode = FALSE;
				}
				else if (!colorimetrydatablock_supportsxvycc709(&colorimetry)
						&& videoparams_getextcolorimetry(video) == 1)
				{
					error_set(ERR_SINK_DOES_NOT_SUPPORT_XVYCC709_COLORIMETRYY);
					hdmi_debug(LOG_DEBUG,"Sink does NOT support xvYcc709 extended colorimetry");
					errorcode = FALSE;
				}
				else
				{
					error_set(ERR_EXTENDED_COLORIMETRY_PARAMS_INVALID);
					hdmi_debug(LOG_DEBUG,"Invalid extended colorimetry parameters");
				}
			}
			else
			{
				error_set(ERR_SINK_DOES_NOT_SUPPORT_EXTENDED_COLORIMETRY);
				errorcode = FALSE;
			}
		}
	}
	else
	{ /* DVI mode */

		if (videoparams_getcolorresolution(video) > 8)
		{
			error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
			hdmi_err("DVI - deep color not supported");
			errorcode = FALSE;
		}
		if (videoparams_getencodingout(video) != RGB)
		{
			error_set(ERR_DVI_MODE_WITH_YCC_ENCODING);
			hdmi_err("DVI mode does not support video encoding other than RGB");
			errorcode = FALSE;
		}
		if (videoparams_ispixelrepetition(video))
		{
			error_set(ERR_DVI_MODE_WITH_PIXEL_REPETITION);
			hdmi_err("DVI mode does not support pixel repetition");
			errorcode = FALSE;
		}
		if (videoparams_gettmdsclock(video) > 16500)
		{
			hdmi_debug(LOG_DEBUG,"Sink must support DVI dual-link");
		}
		/* check colorimetry data */
		if ((videoparams_getcolorimetry(video) == EXTENDED_COLORIMETRY))
		{
			error_set(ERR_DVI_MODE_WITH_EXTENDED_COLORIMETRY);
			hdmi_err("DVI does not support extended colorimetry");
			errorcode = FALSE;
		}
	}
	/*
	 * DTD check
	 * always checking VALID to minimise code execution (following probability)
	 * this video code is to be supported by all dtv's
	 */
	valid = (dtd_getcode(videoparams_getdtd(video)) == 1) ? TRUE : FALSE;
	if (!valid)
	{
		for (i = 0; i < api_ediddtdcount(); i++)
		{
			api_ediddtd(i, &dtd);
			if (dtd_getcode(videoparams_getdtd(video)) != (u8) (-1))
			{
				if (dtd_getcode(videoparams_getdtd(video)) == dtd_getcode(&dtd))
				{
					valid = TRUE;
					break;
				}
			}
			else if (dtd_isequal(videoparams_getdtd(video), &dtd))
			{
				valid = TRUE;
				break;
			}
		}
	}
	if (!valid)
	{
		if (dtd_getcode(videoparams_getdtd(video)) != (u8) (-1))
		{
			for (i = 0; i < api_edidsvdcount(); i++)
			{
				api_edidsvd(i, &svd);
				if ((unsigned) (dtd_getcode(videoparams_getdtd(video)))
						== shortvideodesc_getcode(&svd))
				{
					valid = TRUE;
					break;
				}
			}
		}
	}
	if (!valid)
	{
		error_set(ERR_SINK_DOES_NOT_SUPPORT_SELECTED_DTD);
		hdmi_err("Sink does NOT indicate support for selected DTD");
		errorcode = FALSE;
	}
	/* check quantization range */
	if (api_edidvideocapabilitydatablock(&capability) == TRUE)
	{
		if (!videocapabilitydatablock_getquantizationrangeselectable(
				&capability) && videoparams_getrgbquantizationrange(video) > 1)
		{
			hdmi_err("Sink does NOT indicate support for\
				selected quantization range");
		}
	}
	return errorcode;
}

int only_audio_configure(videoParams_t *video, audioParams_t *audio)
{

	int ret;

	if (api_mcurrentstate == API_EDID_READ) {
		ret = api_checkparamsaudio(audio);
		if (ret == FALSE) {
			hdmi_err("api_checkparamsaudio failed\n");
			return -1;
		}
	}

	ret = audio_configure(api_mbaseaddress, audio,
			videoparams_getpixelclock(video),
			videoparams_getratioclock(video));

	if (ret == FALSE) {
		hdmi_debug(LOG_DEBUG,"audio_configure failed\n");
		return -1;
	}

	packets_audioinfoframe(api_mbaseaddress, audio);

	return 0;
}
