/*
 * hdcpParams.c
 *
 *  Created on: Jul 21, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "hdcpParams.h"

void hdcpparams_reset(hdcpParams_t *params)
{
	params->menable11feature = 0;
	params->mricheck = 0;
	params->mi2cfastmode = 0;
	params->menhancedlinkverification = 0;
}

int hdcpparams_getenable11feature(hdcpParams_t *params)
{
	return params->menable11feature;
}

int hdcpparams_getricheck(hdcpParams_t *params)
{
	return params->mricheck;
}

int hdcpparams_geti2cfastmode(hdcpParams_t *params)
{
	return params->mi2cfastmode;
}

int hdcpparams_getenhancedlinkverification(hdcpParams_t *params)
{
	return params->menhancedlinkverification;
}

void hdcpparams_setenable11feature(hdcpParams_t *params, int value)
{
	params->menable11feature = value;
}

void hdcpparams_setenhancedlinkverification(hdcpParams_t *params, int value)
{
	params->menhancedlinkverification = value;
}

void hdcpparams_seti2cfastmode(hdcpParams_t *params, int value)
{
	params->mi2cfastmode = value;
}

void hdcpparams_setricheck(hdcpParams_t *params, int value)
{
	params->mricheck = value;
}
