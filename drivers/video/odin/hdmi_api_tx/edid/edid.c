/*
 * edid.c
 *
 *  Created on: Jul 23, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include <linux/kernel.h>

#include "edid.h"
#include "../util/bitOperation.h"
#include "../util/log.h"
#include "../util/error.h"
#include "../../dss/dss.h"

const u16 i2cm_base_addr = 0x7E00;

const u16 i2cm_min_fs_scl_high_time = 600;
const u16 i2cm_min_fs_scl_low_time = 1300;
const u16 i2cm_min_ss_scl_high_time = 4500; /*4000;*/
const u16 i2cm_min_ss_scl_low_time = 5200; /*4700;*/
const u32 i2cm_div_factor = 100000;
u8 edid_mblocksno;
/**
 * Local variable hold the number of the currently read byte
 * in a certain block of the EDID structure
 */
u8 edid_mcurraddress;
/**
 * Local variable hold the number of the currently
 * read block in the EDID structure.
 */
u8 edid_mcurrblockno;
/**
 * The accumulator of the block check sum value.
 */
u8 edid_mblocksum;
/**
 * Local variable to hold the status when the reading
 * of the whole EDID structure
 */
edid_status_t edid_mstatus = EDID_IDLE;
/**
 * Local variable holding the buffer of 128 read bytes.
 */
u8 edid_mbuffer[0x80];
/**
 * Array to hold all the parsed Detailed Timing Descriptors.
 */
dtd_t edid_mdtd[32];

unsigned int edid_mdtdindex;

bool has_dtd480p;
/**
 * array to hold all the parsed Short Video Descriptors.
 */
shortVideoDesc_t edid_msvd[128];

unsigned int edid_msvdindex;
/**
 * array to hold all the parsed Short Audio Descriptors.
 */
shortAudioDesc_t edid_msad[128];

unsigned int edid_msadindex;
/**
 * A string to hold the Monitor Name parsed from EDID.
 */
char edid_mmonitorname[13];

int edid_mycc444support;

int edid_mycc422support;

int edid_mbasicaudiosupport;

int edid_munderscansupport;

hdmivsdb_t edid_mhdmivsdb;

monitorRangeLimits_t edid_mmonitorrangelimits;

videoCapabilityDataBlock_t edid_mvideocapabilitydatablock;

colorimetryDataBlock_t edid_mcolorimetrydatablock;

speakerAllocationDataBlock_t edid_mspeakerallocationdatablock;

/* calculate the fast speed high time counter - round up */
static u16 edid_i2c_count(u16 sfrClock, u16 sclMinTime)
{
	unsigned long tmp_scl_period = 0;
	if (((sfrClock * sclMinTime) % i2cm_div_factor) != 0)
	{
		tmp_scl_period = (unsigned long)((sfrClock * sclMinTime) +
			(i2cm_div_factor - ((sfrClock * sclMinTime) % i2cm_div_factor)))
			/ i2cm_div_factor;
	}
	else
	{
		tmp_scl_period = (unsigned long)(sfrClock * sclMinTime) / i2cm_div_factor;
	}
	return (u16)(tmp_scl_period);
}

void edid_sampledata(void)
{
	/*
	u8 edid_data[256] = {
		0x0 ,0xff,0xff,0xff,0xff,0xff,0xff,0x0 ,
		0x1e,0x6d,0x93,0x58,0x1 ,0x1 ,0x1 ,0x1 ,
		0x1 ,0x15,0x1 ,0x3 ,0x80,0x33,0x1d,0x78,
		0xa ,0xd9,0x45,0xa2,0x55,0x4d,0xa0,0x27,
		0x12,0x50,0x54,0xa5,0x6f,0x0 ,0x71,0x4f,
		0x81,0xc0,0x81,0x0 ,0x81,0x80,0x95,0x0 ,
		0x90,0x40,0xa9,0xc0,0xb3,0x0 ,0x2 ,0x3a,
		0x80,0x18,0x71,0x38,0x2d,0x40,0x58,0x2c,
		0x45,0x0 ,0xfd,0x1e,0x11,0x0 ,0x0 ,0x1a,
		0x21,0x39,0x90,0x30,0x62,0x1a,0x27,0x40,
		0x68,0xb0,0x36,0x0 ,0xfd,0x1e,0x11,0x0 ,
		0x0 ,0x1c,0x0 ,0x0 ,0x0 ,0xfd,0x0 ,0x38,
		0x4b,0x1e,0x53,0xf ,0x0 ,0xa ,0x20,0x20,
		0x20,0x20,0x20,0x20,0x0 ,0x0 ,0x0 ,0xfc,
		0x0 ,0x44,0x4d,0x32,0x33,0x35,0x30,0x44,
		0xa ,0x20,0x20,0x20,0x20,0x20,0x1 ,0x8d,
		0x2 ,0x3 ,0x33,0xf1,0x4e,0x84,0x5 ,0x3 ,
		0x2 ,0x20,0x22,0x10,0x11,0x13,0x12,0x14,
		0x1f,0x7 ,0x16,0x26,0x15,0x7 ,0x50,0x9 ,
		0x7 ,0x7 ,0x78,0x3 ,0x3 ,0xc ,0x0 ,0x10,
		0x0 ,0x80,0x2d,0x20,0xc0,0xe ,0x1 ,0x40,
		0xa ,0x3c,0x8 ,0x10,0x18,0x10,0x98,0x10,
		0x58,0x10,0x38,0x10,0x1 ,0x1d,0x0 ,0x72,
		0x51,0xd0,0x1e,0x20,0x38,0x88,0x15,0x0 ,
		0x56,0x50,0x21,0x0 ,0x0 ,0x1e,0x1 ,0x1d,
		0x80,0xd0,0x72,0x1c,0x16,0x20,0x10,0x2c,
		0x25,0x80,0xc4,0x8e,0x21,0x0 ,0x0 ,0x9e,
		0x2 ,0x3a,0x80,0xd0,0x72,0x38,0x2d,0x40,
		0x10,0x2c,0x45,0x20,0x6 ,0x44,0x21,0x0 ,
		0x0 ,0x1e,0x2 ,0x3a,0x80,0x18,0x71,0x38,
		0x2d,0x40,0x58,0x2c,0x45,0x0 ,0x56,0x50,
		0x21,0x0 ,0x0 ,0x1e,0x0 ,0x0 ,0x0 ,0x0
	};
	*/

}
u8 edid_slimporttohdmi(u32 baseAddr, u8* firstblock, u8* extblock)
{
	int ret;
	int i;
	dtd_t dtd_480p;
	LOG_NOTICE("Parsing edid data from SlimPort edid buffer\n");
	edid_reset(baseAddr);
	ret = edid_parseblock(baseAddr, firstblock);
	edid_mcurraddress = 0;
	edid_mcurrblockno++;
	edid_mblocksum = 0;

	if( edid_mcurrblockno <  edid_mblocksno)
	{

		ret = edid_parseblock(baseAddr, extblock);
		edid_mcurraddress = 0;
		edid_mcurrblockno++;
		edid_mblocksum = 0;
	}

	if (edid_mcurrblockno >= edid_mblocksno)
	{
		edid_mstatus = EDID_DONE;
	}

	dtd_fill(&dtd_480p, 1, 60000);
	for (i=0; i<edid_mdtdindex; i++){
		if (dtd_isequal(&edid_mdtd[i], &dtd_480p)){
			has_dtd480p = TRUE;
			break;
		}
	}
	if (!has_dtd480p && (edid_mdtdindex < (sizeof(edid_mdtd) / sizeof(dtd_t))) ){
		edid_mdtd[edid_mdtdindex] = dtd_480p;
		edid_mdtd[edid_mdtdindex++].mcode = -1; //due to dvi set later.
		has_dtd480p = TRUE;
	}

	return edid_mstatus;
}
int edid_initialize(u16 baseAddr, u16 sfrClock)
{
	LOG_TRACE2(baseAddr, sfrClock);
	/* mask interrupts */
	haledid_maskinterrupts(baseAddr + i2cm_base_addr, TRUE);
	edid_reset(baseAddr);
	/* set clock division */
	haledid_masterclockdivision(baseAddr + i2cm_base_addr, 0x05);
	haledid_fastspeedhighcounter(baseAddr + i2cm_base_addr,
		edid_i2c_count(sfrClock, i2cm_min_fs_scl_high_time ));
	haledid_fastspeedlowcounter(baseAddr + i2cm_base_addr,
		edid_i2c_count(sfrClock, i2cm_min_fs_scl_low_time));
	haledid_standardspeedlowcounter(baseAddr + i2cm_base_addr,
		edid_i2c_count(sfrClock, i2cm_min_ss_scl_low_time));
	haledid_standardspeedhighcounter(baseAddr + i2cm_base_addr,
		edid_i2c_count(sfrClock, i2cm_min_ss_scl_high_time));
	/* set address to EDID address -see spec */
	/* HW deals with LSB (alternating the address between 0xA0 & 0xA1) */
	haledid_slaveaddress(baseAddr + i2cm_base_addr, 0x50);
	/* HW deals with LSB (making the segment address go to 60) */
	haledid_segmentaddr(baseAddr + i2cm_base_addr, 0x30);
	/* start reading E-EDID */
	edid_mstatus = EDID_READING;
	LOG_NOTICE("reading EDID");
	/* enable interrupts */
	haledid_maskinterrupts(baseAddr + i2cm_base_addr, FALSE);
	edid_readrequest(baseAddr, 0, 0);
	return TRUE;
}

int edid_standby(u16 baseAddr)
{
	LOG_TRACE();
	/* disable intterupts */
	haledid_maskinterrupts(baseAddr + i2cm_base_addr, TRUE);
	edid_mstatus = EDID_IDLE;
	return TRUE;
}

u8 edid_eventhandler(u16 baseAddr, int hpd, u8 state)
{
	LOG_TRACE2(hpd, state);
	if (edid_mstatus != EDID_READING)
	{
		return EDID_IDLE;
	}
	else if (!hpd)
	{ /* hot plug detected without cable disconnection */
		error_set(ERR_HPD_LOST);
		LOG_WARNING("hpd");
		edid_mstatus = EDID_ERROR;
	}
	else if ((state & BIT(0)) != 0) /* error */
	{
		LOG_WARNING("error");
		edid_mstatus = EDID_ERROR;
	}
	else if ((state & BIT(1)) != 0) /* done */
	{
		if (edid_mcurraddress >= sizeof(edid_mbuffer))
		{
			error_set(ERR_OVERFLOW);
			LOG_WARNING("overflow");
			edid_mstatus = EDID_ERROR;
		}
		else
		{
			edid_mbuffer[edid_mcurraddress] = haledid_readdata(baseAddr
					+ i2cm_base_addr);
			edid_mblocksum += edid_mbuffer[edid_mcurraddress++];
			if (edid_mcurraddress >= sizeof(edid_mbuffer))
			{
				/*check if checksum is correct (CEA-861 D Spec p108) */
				if (edid_mblocksum % 0x100 == 0)
				{
					edid_parseblock(baseAddr, edid_mbuffer);
					edid_mcurraddress = 0;
					edid_mcurrblockno++;
					edid_mblocksum = 0;
					if (edid_mcurrblockno >= edid_mblocksno)
					{
						edid_mstatus = EDID_DONE;
					}
				}
				else
				{
					error_set(ERR_BLOCK_CHECKSUM_INVALID);
					LOG_WARNING("block checksum invalid");
					edid_mstatus = EDID_ERROR;
				}
			}
		}
	}
	if (edid_mstatus == EDID_READING)
	{
		edid_readrequest(baseAddr, edid_mcurraddress, edid_mcurrblockno);
	}
	else if (edid_mstatus == EDID_DONE)
	{
		edid_mblocksno = 1;
		edid_mcurrblockno = 0;
		edid_mcurraddress = 0;
		edid_mblocksum = 0;
	}
	return edid_mstatus;
}

unsigned int edid_getdtdcount(u16 baseAddr)
{
	LOG_TRACE();
	return edid_mdtdindex;
}

int edid_getdtd(u16 baseAddr, unsigned int n, dtd_t * dtd)
{
	LOG_TRACE1(n);
	if (n < edid_getdtdcount(baseAddr))
	{
		*dtd = edid_mdtd[n];
		return TRUE;
	}
	return FALSE;
}

int edid_gethdmivsdb(u16 baseAddr, hdmivsdb_t * vsdb)
{
	LOG_TRACE();
	if (edid_mhdmivsdb.mvalid)
	{
		*vsdb = edid_mhdmivsdb;
		return TRUE;
	}
	return FALSE;
}

int edid_getmonitorname(u16 baseAddr, char * name, unsigned int length)
{
	unsigned int i = 0;
	LOG_TRACE();
	for (i = 0; i < length && i < sizeof(edid_mmonitorname); i++)
	{
		name[i] = edid_mmonitorname[i];
	}
	return i;
}

int edid_getmonitorrangelimits(u16 baseAddr, monitorRangeLimits_t * limits)
{
	LOG_TRACE();
	if (edid_mmonitorrangelimits.mvalid)
	{
		*limits = edid_mmonitorrangelimits;
		return TRUE;
	}
	return FALSE;
}

unsigned int edid_getsvdcount(u16 baseAddr)
{
	LOG_TRACE();
	return edid_msvdindex;
}

int edid_getsvd(u16 baseAddr, unsigned int n, shortVideoDesc_t * svd)
{
	LOG_TRACE1(n);
	if (n < edid_getsvdcount(baseAddr))
	{
		*svd = edid_msvd[n];
		return TRUE;
	}
	return FALSE;
}

unsigned int edid_getsadcount(u16 baseAddr)
{
	LOG_TRACE();
	return edid_msadindex;
}

int edid_getsad(u16 baseAddr, unsigned int n, shortAudioDesc_t * sad)
{
	LOG_TRACE1(n);
	if (n < edid_getsadcount(baseAddr))
	{
		*sad = edid_msad[n];
		return TRUE;
	}
	return FALSE;
}

int edid_getvideocapabilitydatablock(u16 baseAddr,
		videoCapabilityDataBlock_t * capability)
{
	LOG_TRACE();
	if (edid_mvideocapabilitydatablock.mvalid)
	{
		*capability = edid_mvideocapabilitydatablock;
		return TRUE;
	}
	return FALSE;
}

int edid_getspeakerallocationdatablock(u16 baseAddr,
		speakerAllocationDataBlock_t * allocation)
{
	LOG_TRACE();
	if (edid_mspeakerallocationdatablock.mvalid)
	{
		*allocation = edid_mspeakerallocationdatablock;
		return TRUE;
	}
	return FALSE;
}

int edid_getcolorimetrydatablock(u16 baseAddr,
		colorimetryDataBlock_t * colorimetry)
{
	LOG_TRACE();
	if (edid_mcolorimetrydatablock.mvalid)
	{
		*colorimetry = edid_mcolorimetrydatablock;
		return TRUE;
	}
	return FALSE;
}

int edid_supportsbasicaudio(u16 baseAddr)
{
	return edid_mbasicaudiosupport;
}

int edid_supportsunderscan(u16 baseAddr)
{
	return edid_munderscansupport;
}

int edid_supportsycc422(u16 baseAddr)
{
	return edid_mycc422support;
}

int edid_supportsycc444(u16 baseAddr)
{
	return edid_mycc444support;
}

int edid_reset(u16 baseAddr)
{
	unsigned i = 0;
	LOG_TRACE();
	edid_mblocksno = 1;
	edid_mcurrblockno = 0;
	edid_mcurraddress = 0;
	edid_mblocksum = 0;
	edid_mstatus = EDID_READING;
	for (i = 0; i < sizeof(edid_mbuffer); i++)
	{
		edid_mbuffer[i] = 0;
	}
	for (i = 0; i < sizeof(edid_mmonitorname); i++)
	{
		edid_mmonitorname[i] = 0;
	}
	edid_mbasicaudiosupport = FALSE;
	edid_munderscansupport = FALSE;
	edid_mycc422support = FALSE;
	edid_mycc444support = FALSE;
	edid_mdtdindex = 0;
	edid_msadindex = 0;
	edid_msvdindex = 0;
	has_dtd480p = FALSE;
	hdmivsdb_reset(&edid_mhdmivsdb);
	monitorrangelimits_reset(&edid_mmonitorrangelimits);
	videocapabilitydatablock_reset(&edid_mvideocapabilitydatablock);
	colorimetrydatablock_reset(&edid_mcolorimetrydatablock);
	speakerallocationdatablock_reset(&edid_mspeakerallocationdatablock);
	return TRUE;
}

int edid_readrequest(u16 baseAddr, u8 address, u8 blockNo)
{
	/*to incorporate extensions we have to include the following
	   - see VESA E-DDC spec. P 11 */
	u8 spointer = blockNo / 2;
	u8 edidaddress = ((blockNo % 2) * 0x80) + address;

	LOG_TRACE2(spointer, edidaddress);
	haledid_requestaddr(baseAddr + i2cm_base_addr, edidaddress);
	haledid_segmentpointer(baseAddr + i2cm_base_addr, spointer);
	if (spointer == 0)
	{
		haledid_requestread(baseAddr + i2cm_base_addr);
	}
	else
	{
		haledid_requestextread(baseAddr + i2cm_base_addr);
	}
	return TRUE;
}

int edid_parseblock(u16 baseAddr, u8 * buffer)
{
	const unsigned dtd_size = 0x12;  /*18*/
	const unsigned timing_modes = 0x36; /*54*/
	dtd_t tmpdtd;
	unsigned c = 0;
	unsigned i = 0;
	LOG_TRACE();
	if (edid_mcurrblockno == 0)
	{
		if (buffer[0] == 0x00)
		{ /* parse block zero */
			edid_mblocksno = buffer[126] + 1;
			/* parse DTD's */
			for (i = timing_modes; i < (timing_modes + (dtd_size * 4)); i+= dtd_size)
			{
				if (bitoperation_bytes2word(buffer[i + 1], buffer[i + 0]) > 0)
				{
					if (dtd_parse(&tmpdtd, buffer + i) == TRUE)
					{
						if (edid_mdtdindex < (sizeof(edid_mdtd)/ sizeof(dtd_t)))
						{
							edid_mdtd[edid_mdtdindex++] = tmpdtd;
						}
						else
						{
							error_set(ERR_DTD_BUFFER_FULL);
							LOG_WARNING("buffer full - DTD ignored");
						}
					}
					else
					{
						LOG_WARNING("DTD corrupt");
					}
				}
				else if ((buffer[i + 2] == 0) && (buffer[i + 4] == 0))
				{
					/* it is a Display-monitor Descriptor */
					if (buffer[i + 3] == 0xFC)
					{ /* Monitor Name */
						for (c = 0; c < 13; c++)
						{
							edid_mmonitorname[c] = buffer[i + c + 5];
						}
					}
					else if (buffer[i + 3] == 0xFD)
					{ /* Monitor Range Limits */
						if (monitorrangelimits_parse(&edid_mmonitorrangelimits,
								buffer + i) != TRUE)
						{
							LOG_WARNING2("Monitor Range Limits corrupt", i);
						}
					}
				}
			}
			for ( i = 0; i < 0x80; i += 4) {
				printk("EDID1[%02x-%02x] %02x %02x %02x %02x\n",
					i, i +3, buffer[i], buffer[i+1], buffer[i+2], buffer[i+3]);
			}
		}
	}
	else if (buffer[0] == 0xF0)
	{ /* Block Map Extension */
		/* last is checksum */
		for (i = 1; i < (sizeof(edid_mbuffer) - 1); i++)
		{
			if (buffer[i] == 0x00)
				break;
		}
		if (edid_mblocksno < 128)
		{
			if (i > edid_mblocksno)
			{ /* N (no of extensions) does NOT include Block maps */
				edid_mblocksno += 2;
			}
			else if (i == edid_mblocksno)
			{
				edid_mblocksno += 1;
			}
		}
		else
		{
			i += 127;
			if (i > edid_mblocksno)
			{ /* N (no of extensions) does NOT include Block maps */
				edid_mblocksno += 3;
			}
			else if (i == edid_mblocksno)
			{
				edid_mblocksno += 1;
			}
		}
		for ( i = 0; i < 0x80; i += 4) {
			printk("EDID2[%02x-%02x] %02x %02x %02x %02x\n",
				i, i +3, buffer[i],buffer[i+1],buffer[i+2],buffer[i+3]);
		}
	}
	else if (buffer[0] == 0x02)
	{ /* CEA Extension block */
		if (buffer[1] == 0x03)
		{ /* revision number (only rev3 is allowed by HDMI spec) */
			u8 offset = buffer[2];
			edid_mycc422support = bitoperation_bitfield(buffer[3], 4, 1) == 1;
			edid_mycc444support = bitoperation_bitfield(buffer[3], 5, 1) == 1;
			edid_mbasicaudiosupport = bitoperation_bitfield(buffer[3], 6, 1)
					== 1;
			edid_munderscansupport = bitoperation_bitfield(buffer[3], 7, 1)
					== 1;
			if (offset != 4)
			{
				for (i = 4; i < offset; i += edid_parsedatablock(baseAddr, buffer + i))
					;
			}
			/* last is checksum */
			for (i = offset, c = 0; i < (sizeof(edid_mbuffer) - 1) && c < 6; i
					+= dtd_size, c++)
			{
				if (dtd_parse(&tmpdtd, buffer + i) == TRUE)
				{
					if (edid_mdtdindex < (sizeof(edid_mdtd) / sizeof(dtd_t)))
					{
						edid_mdtd[edid_mdtdindex++] = tmpdtd;
					}
					else
					{
						error_set(ERR_DTD_BUFFER_FULL);
						LOG_WARNING("buffer full - DTD ignored");
					}
				}
			}
			for ( i = 0; i < 0x80; i += 4) {
				printk("EDID[%02x-%02x] %02x %02x %02x %02x\n",
					i, i +3, buffer[i],buffer[i+1],buffer[i+2],buffer[i+3]);
			}
		}
	}

	return TRUE;
}

u8 edid_parsedatablock(u16 baseAddr, u8 * data)
{
	u8 tag = bitoperation_bitfield(data[0], 5, 3);
	u8 length = bitoperation_bitfield(data[0], 0, 5);
	shortAudioDesc_t tmpsad;
	shortVideoDesc_t tmpsvd;
	u32 ieeid;
	u8 extendedtag;
	u8 c = 0;

	LOG_TRACE3("TAG", tag, length);

	switch (tag)
	{
		case 0x1: /* Audio Data Block */
			for (c = 1; c < (length + 1); c += 3)
			{
				shortaudiodesc_parse(&tmpsad, data + c);
				if (edid_msadindex < (sizeof(edid_msad) / sizeof(shortAudioDesc_t)))
				{
					edid_msad[edid_msadindex++] = tmpsad;
				}
				else
				{
				error_set(ERR_SHORT_AUDIO_DESC_BUFFER_FULL);
					LOG_WARNING("buffer full - SAD ignored");
				}
			}
			break;
		case 0x2: /* Video Data Block */
			for (c = 1; c < (length + 1); c++)
			{
				shortvideodesc_parse(&tmpsvd, data[c]);
				if (edid_msvdindex < (sizeof(edid_msvd) / sizeof(shortVideoDesc_t)))
				{
					edid_msvd[edid_msvdindex++] = tmpsvd;
				}
				else
				{
				error_set(ERR_SHORT_VIDEO_DESC_BUFFER_FULL);
					LOG_WARNING("buffer full - SVD ignored");
				}
			}
			break;
		case 0x3: /* Vendor Specific Data Block */
			{
				ieeid =  bitoperation_bytes2dword(0x00, data[3], data[2], data[1]);
				if (ieeid == 0x000C03)
				{ /* HDMI */
					if (hdmivsdb_parse(&edid_mhdmivsdb, data) != TRUE)
					{
						LOG_WARNING("HDMI Vendor Specific Data Block corrupt");
					}
				}
				else
				{
					LOG_WARNING2("Vendor Specific Data Block not parsed", ieeid);
				}
				break;
			}
		case 0x4: /* Speaker Allocation Data Block */
			if (speakerallocationdatablock_parse(&edid_mspeakerallocationdatablock,
						data) != TRUE)
			{
				LOG_WARNING("Speaker Allocation Data Block corrupt");
			}
			break;
		case 0x7:
			{
				extendedtag = data[1];
				switch (extendedtag)
				{
					case 0x00: /* Video Capability Data Block */
						if (videocapabilitydatablock_parse(&edid_mvideocapabilitydatablock,
									data) != TRUE)
						{
							LOG_WARNING("Video Capability Data Block corrupt");
						}
						break;
					case 0x05: /* Colorimetry Data Block */
			if (colorimetrydatablock_parse(&edid_mcolorimetrydatablock, data)
								!= TRUE)
						{
							LOG_WARNING("Colorimetry Data Block corrupt");
						}
						break;
					case 0x04: /* HDMI Video Data Block */
					case 0x12: /* HDMI Audio Data Block */
						break;
					default:
						LOG_WARNING2("Extended Data Block not parsed", extendedtag);
						break;
				}
				break;
			}
		default:
			LOG_WARNING2("Data Block not parsed", tag);
			break;
	}
	return length + 1;
}
