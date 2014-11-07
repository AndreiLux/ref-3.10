/*
 * videoCapabilityDataBlock.c
 *
 *  Created on: Jul 23, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "videoCapabilityDataBlock.h"
#include "../util/bitOperation.h"
#include "../util/log.h"

void videocapabilitydatablock_reset(videoCapabilityDataBlock_t * vcdb)
{
	vcdb->mquantizationrangeselectable = FALSE;
	vcdb->mpreferredtimingscaninfo = 0;
	vcdb->mitscaninfo = 0;
	vcdb->mcescaninfo = 0;
	vcdb->mvalid = FALSE;
}

int videocapabilitydatablock_parse(videoCapabilityDataBlock_t * vcdb, u8 * data)
{
	LOG_TRACE();
	videocapabilitydatablock_reset(vcdb);
	/* check tag code and extended tag */
	if (data != 0 && bitoperation_bitfield(data[0], 5, 3) == 0x7
			&& bitoperation_bitfield(data[1], 0, 8) == 0x0
			&& bitoperation_bitfield(data[0], 0, 5) == 0x2)
	{ /* so far VCDB is 2 bytes long */
		vcdb->mcescaninfo = bitoperation_bitfield(data[2], 0, 2);
		vcdb->mitscaninfo = bitoperation_bitfield(data[2], 2, 2);
		vcdb->mpreferredtimingscaninfo = bitoperation_bitfield(data[2], 4, 2);
		vcdb->mquantizationrangeselectable = (bitoperation_bitfield(data[2], 6,
				1) == 1) ? TRUE : FALSE;
		vcdb->mvalid = TRUE;
		return TRUE;
	}
	return FALSE;
}

u8 videocapabilitydatablock_getcescaninfo(videoCapabilityDataBlock_t * vcdb)
{
	return vcdb->mcescaninfo;
}

u8 videocapabilitydatablock_getitscaninfo(videoCapabilityDataBlock_t * vcdb)
{
	return vcdb->mitscaninfo;
}

u8 videocapabilitydatablock_getpreferredtimingscaninfo
	(videoCapabilityDataBlock_t * vcdb)
{
	return vcdb->mpreferredtimingscaninfo;
}

int videocapabilitydatablock_getquantizationrangeselectable
	(videoCapabilityDataBlock_t * vcdb)
{
	return vcdb->mquantizationrangeselectable;
}
