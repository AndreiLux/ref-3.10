/*
 * @file: error.c
 *
 *  Created on: Aug 10, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#include "error.h"

static errorType_t errorcode = NO_ERROR;

void error_set(errorType_t err)
{
	if ((err > NO_ERROR) && (err < ERR_UNDEFINED))
	{
		errorcode = err;
	}
}

errorType_t error_get(void)
{
	errorType_t tmperr = errorcode;
	errorcode = NO_ERROR;
	return tmperr;
}
