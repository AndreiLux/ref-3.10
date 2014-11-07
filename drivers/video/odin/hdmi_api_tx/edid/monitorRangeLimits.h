/*
 * monitorRangeLimits.h
 *
 *  Created on: Jul 22, 2010
 * 
 * Synopsys Inc.
 * SG DWC PT02
 */

#ifndef MONITORRANGELIMITS_H_
#define MONITORRANGELIMITS_H_

#include "../util/types.h"
/**
 * @file
 * Second Monitor Descriptor
 * Parse and hold Monitor Range Limits information read from EDID
 */

typedef struct
{
	u8 mminverticalrate;

	u8 mmaxverticalrate;

	u8 mminhorizontalrate;

	u8 mmaxhorizontalrate;

	u8 mmaxpixelclock;

	int mvalid;
} monitorRangeLimits_t;

void monitorrangelimits_reset(monitorRangeLimits_t * mrl);

int monitorrangelimits_parse(monitorRangeLimits_t * mrl, u8 * data);

/**
 * @return the maximum parameter for horizontal frequencies
 */
u8 monitorrangelimits_getmaxhorizontalrate(monitorRangeLimits_t * mrl);
/**
 * @return the maximum parameter for pixel clock rate
 */
u8 monitorrangelimits_getmaxpixelclock(monitorRangeLimits_t * mrl);
/**
 * @return the maximum parameter for vertical frequencies
 */
u8 monitorrangelimits_getmaxverticalrate(monitorRangeLimits_t * mrl);
/**
 * @return the minimum parameter for horizontal frequencies
 */
u8 monitorrangelimits_getminhorizontalrate(monitorRangeLimits_t * mrl);
/**
 * @return the minimum parameter for vertical frequencies
 */
u8 monitorrangelimits_getminverticalrate(monitorRangeLimits_t * mrl);

#endif /* MONITORRANGELIMITS_H_ */
