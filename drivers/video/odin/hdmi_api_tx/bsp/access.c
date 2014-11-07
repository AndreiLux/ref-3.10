/*
 * access.c
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include <linux/kernel.h>
#include "access.h"
#include "../util/log.h"

#include "../util/error.h"
#include "mutex.h"

static u8 *access_mbaseaddr = 0;

int access_initialize(u8 * baseAddr)
{
	access_mbaseaddr = baseAddr;

	return TRUE;
}

int access_standby()
{
	return TRUE;
}

u8 access_corereadbyte(u16 addr)
{
	u8 data = 0;
	data = access_read(addr);
	return data;
}

u8 access_coreread(u16 addr, u8 shift, u8 width)
{
	if (width <= 0)
	{
		error_set(ERR_DATA_WIDTH_INVALID);
		LOG_ERROR("Invalid parameter: width == 0");
		return 0;
	}
	return (access_corereadbyte(addr) >> shift) & (BIT(width) - 1);
}

void access_corewritebyte(u8 data, u16 addr)
{
	access_write(data, addr);
}

u8 simualtion_readbyte(u32 addr)
{
	u8 data = 0;
	data = access_read(addr);
	return data;
}
void simualtion_writebyte(u32 addr, u8 data)
{
	access_write(data, addr);
}
void access_corewrite(u8 data, u16 addr, u8 shift, u8 width)
{
	u8 temp = 0;
	u8 mask = 0;


	if (width <= 0)
	{
		error_set(ERR_DATA_WIDTH_INVALID);
		LOG_ERROR("Invalid parameter: width == 0");
		return;
	}
	mask = BIT(width) - 1;
	/*if (data > mask)
	{
		error_set(ERR_DATA_GREATER_THAN_WIDTH);
		LOG_ERROR("Invalid parameter: data > width");
		return;
	}*/
	temp = access_read(addr);
	temp &= ~(mask << shift);
	temp |= (data & mask) << shift;
	access_write(temp, addr);
}

u8 access_read(u16 addr)
{
	return access_mbaseaddr[addr];
}

void access_write(u8 data, u16 addr)
{
	access_mbaseaddr[addr] = data;
}
