/*
 * @file:halIdentification.c
 *
 *  Created on: Jul 1, 2010
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "halIdentification.h"
#include "../bsp/access.h"
#include "../util/log.h"
/* register offsets */
static const u8 design_id = 0x00;
static const u8 revision_id = 0x01;
static const u8 product_id0 = 0x02;
static const u8 product_id1 = 0x03;

u8 halidentification_design(u16 baseAddr)
{
	LOG_TRACE();
	return access_corereadbyte((baseAddr + design_id));
}

u8 halidentification_revision(u16 baseAddr)
{
	LOG_TRACE();
	return access_corereadbyte((baseAddr + revision_id));
}

u8 halidentification_productline(u16 baseAddr)
{
	LOG_TRACE();
	return access_corereadbyte((baseAddr + product_id0));
}

u8 halidentification_producttype(u16 baseAddr)
{
	LOG_TRACE();
	return access_corereadbyte((baseAddr + product_id1));
}

