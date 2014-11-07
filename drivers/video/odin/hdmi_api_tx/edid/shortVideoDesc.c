/*
 * shortVideoDesc.c
 *
 *  Created on: Jul 22, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#define DSS_SUBSYS_NAME "HDMI_SVD"

#include <linux/kernel.h>

#include "shortVideoDesc.h"
#include "../util/bitOperation.h"
#include "../util/log.h"

#include "../../dss/dss.h"

void shortvideodesc_reset(shortVideoDesc_t *svd)
{
	svd->mnative = FALSE;
	svd->mcode = 0;
}

int shortvideodesc_parse(shortVideoDesc_t *svd, u8 data)
{
	shortvideodesc_reset(svd);
	svd->mnative = (bitoperation_bitfield(data, 7, 1) == 1) ? TRUE : FALSE;
	svd->mcode = bitoperation_bitfield(data, 0, 7);
//	printk("svd mcode = %x\n", svd->mcode);
	return TRUE;
}

unsigned shortvideodesc_getcode(shortVideoDesc_t *svd)
{
//	DSSINFO("svd get mcode = %x\n", svd->mcode);
	return svd->mcode;
}

int shortvideodesc_getnative(shortVideoDesc_t *svd)
{
	return svd->mnative;
}
