/*
 * hdcpParams.h
 *
 *  Created on: Jul 20, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02 
 */

#ifndef HDCPPARAMS_H_
#define HDCPPARAMS_H_

typedef struct
{
	int menable11feature;
	int mricheck;
	int mi2cfastmode;
	int menhancedlinkverification;
} hdcpParams_t;

void hdcpparams_reset(hdcpParams_t *params);

int hdcpparams_getenable11feature(hdcpParams_t *params);

int hdcpparams_getricheck(hdcpParams_t *params);

int hdcpparams_geti2cfastmode(hdcpParams_t *params);

int hdcpparams_getenhancedlinkverification(hdcpParams_t *params);

void hdcpparams_setenable11feature(hdcpParams_t *params, int value);

void hdcpparams_setenhancedlinkverification(hdcpParams_t *params, int value);

void hdcpparams_seti2cfastmode(hdcpParams_t *params, int value);

void hdcpparams_setricheck(hdcpParams_t *params, int value);

#endif /* HDCPPARAMS_H_ */
