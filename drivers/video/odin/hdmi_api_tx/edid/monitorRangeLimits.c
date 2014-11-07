/*
 * monitorRangeLimits.c
 *
 *  Created on: Jul 22, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#include "monitorRangeLimits.h"
#include "../util/bitOperation.h"
#include "../util/log.h"

void monitorrangelimits_reset(monitorRangeLimits_t * mrl)
{
	mrl->mminverticalrate = 0;
	mrl->mmaxverticalrate = 0;
	mrl->mminhorizontalrate = 0;
	mrl->mmaxhorizontalrate = 0;
	mrl->mmaxpixelclock = 0;
	mrl->mvalid = FALSE;
}
int monitorrangelimits_parse(monitorRangeLimits_t * mrl, u8 * data)
{
	LOG_TRACE();
	monitorrangelimits_reset(mrl);
	if (data != 0 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00
			&& data[3] == 0xFD && data[4] == 0x00)
	{ /* block tag */
		mrl->mminverticalrate = data[5];
		mrl->mmaxverticalrate = data[6];
		mrl->mminhorizontalrate = data[7];
		mrl->mmaxhorizontalrate = data[8];
		mrl->mmaxpixelclock = data[9];
		mrl->mvalid = TRUE;
		return TRUE;
	}
	return FALSE;
}

u8 monitorrangelimits_getmaxhorizontalrate(monitorRangeLimits_t * mrl)
{
	return mrl->mmaxhorizontalrate;
}

u8 monitorrangelimits_getmaxpixelclock(monitorRangeLimits_t * mrl)
{
	return mrl->mmaxpixelclock;
}

u8 monitorrangelimits_getmaxverticalrate(monitorRangeLimits_t * mrl)
{
	return mrl->mmaxverticalrate;
}

u8 monitorrangelimits_getminhorizontalrate(monitorRangeLimits_t * mrl)
{
	return mrl->mminhorizontalrate;
}

u8 monitorrangelimits_getminverticalrate(monitorRangeLimits_t * mrl)
{
	return mrl->mminverticalrate;
}
