/*
 * hdcp.c
 *
 *  Created on: Jul 21, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */
#include "hdcp.h"
#include "hdcpVerify.h"
#include "halHdcp.h"
#include <linux/slab.h>
#include "../util/log.h"
#include "../util/error.h"
#include "../util/types.h"

#include <linux/board_lge.h>
#define HDCP_ENABLE 1
/*
 * HDCP module registers offset
 */
static const u16 hdcp_base_addr = 0x5000;
static const u16 hdcp_key1_base_addr = 0xA001;
static const u16 hdcp_key2_base_addr = 0xA009;

void hdmi_rxkeywrite(u16 baseAddr)
{
	u32 i;
	u8 aksv[5] = {0x01, 0x02, 0x03, 0x00, 0x00 };
	u8 aksv_key[281] =
	{
	0x1,};


	for (i = 0; i < 5; i++)
	{
		halhdcp_key(baseAddr+hdcp_key1_base_addr+i, aksv[i]);
	}
	for (i = 0; i < 281; i++)
	{
		halhdcp_key(baseAddr+hdcp_key2_base_addr+i, aksv_key[i]);
	}
}

int hdcp_initialize(u16 baseAddr, int dataEnablePolarity)
{
	LOG_TRACE();
	halhdcp_rxdetected(baseAddr + hdcp_base_addr, 0);
	halhdcp_dataenablepolarity(baseAddr + hdcp_base_addr, (dataEnablePolarity
			== TRUE) ? 1 : 0);
	halhdcp_disableencryption(baseAddr + hdcp_base_addr, 1);
	return TRUE;
}

#ifdef HW_WA_HDCP_DC0
#include "../edid/halEdid.h"
#include "../core/halInterrupt.h"
#include "../bsp/system.h"
#define HDCP_TIMEOUT_5S 	32
/* Aksv = 0xE6CB130E59 */
/* TODO: to be replaced by vendor code */
static const u8 aksv[] = {0xE6, 0xCB, 0x13, 0x0E, 0x59};
static const u8 an[] = {0x5A, 0x84, 0x5A, 0xD9, 0x1F, 0x3C, 0xC3, 0x88};

u8 readI2c(u16 baseAddr, u8 address)
{
	int interrupt = 0;
	haledid_requestaddr(baseAddr + 0x7E00, address);
	haledid_requestread(baseAddr + 0x7E00);
	do
	{
		interrupt = halinterrupt_i2cddcstate(baseAddr + 0x0100);
	}
	while (interrupt == 0);
	if (interrupt & BIT(0))
	{
		LOG_ERROR2("I2C error - READ", address);
	}
	halinterrupt_i2cddcclear(
		baseAddr + 0x0100, interrupt); /* clear i2c interrupt */
	return haledid_readdata(baseAddr + 0x7E00);
}

u8 writeI2c(u16 baseAddr, u8 address, u8 data)
{
	int interrupt = 0;
	haledid_requestaddr(baseAddr + 0x7E00, address);
	haledid_writedata(baseAddr + 0x7E00, data);
	haledid_requestwrite(baseAddr + 0x7E00);
	do
	{
		interrupt = halinterrupt_i2cddcstate(baseAddr + 0x0100);
	}
	while (interrupt == 0);
	if (interrupt & BIT(0))
	{
		LOG_ERROR2("I2C error- WRITE", address);
	}
	/* clear i2c interrupt */
	halinterrupt_i2cddcclear(baseAddr + 0x0100, interrupt);
	return TRUE;
}
#endif

int hdcp_configure(u16 baseAddr, hdcpParams_t *params,
		int hdmi, int hsPol, int vsPol)
{
#ifdef HW_WA_HDCP_DC0
	LOG_NOTICE("HW_WA_HDCP_DC0");
	u8 bCaps = 0;
	u16 bStatus = 0;
	int isRepeater = FALSE;
	long long i = 0;
	u8 temp = halinterrupt_mutei2cstate(baseAddr + 0x0100);
	halinterrupt_mutei2cclear(baseAddr + 0x0100, 0x3);
	haledid_maskinterrupts(baseAddr + 0x7E00, FALSE); /* enable interrupts */
	haledid_slaveaddress(baseAddr + 0x7E00, 0x3A); /* HW deals with LSB */
	/* video related */
	halhdcp_devicemode(baseAddr + hdcp_base_addr, (hdmi == TRUE) ? 1 : 0);
	do
	{
		/* read Bcaps */
		bCaps = readI2c(baseAddr, 0x40);
		i = 0;
		do
		{	/* poll Bstatus until video mode(DVI/HDMI) is equal */
			bStatus = (readI2c(baseAddr, 0x42) << 8);
			bStatus |= readI2c(baseAddr, 0x41);
			if (i >= HDCP_TIMEOUT_5S)
			{
				haledid_maskinterrupts(baseAddr + 0x7E00, TRUE); /* disable interrupts */
				/* return interrupts mute to original state */
				halinterrupt_mutei2cclear(baseAddr + 0x0100, temp);
				halinterrupt_i2cddcclear(baseAddr + 0x0100, 0xff); /* clear i2c interrupt */
				error_set(ERR_HDCP_FAIL);
				return FALSE;
			}
			i++;
		}
		while (hdmi != ((bStatus & BIT(12)) >> 12));
		bCaps = readI2c(baseAddr, 0x40);
		isRepeater = ((bCaps & BIT(6)) >> 6);
		if (!isRepeater)
		{	/* device is no repeater, controller takes over */
			LOG_NOTICE("Device NOT repeater");
			break;
		}
		/* write Ainfo - 0x00 */
		writeI2c(baseAddr, 0x15, 0x00);
		/* write An  */
		writeI2c(baseAddr, 0x18, an[7]);
		writeI2c(baseAddr, 0x19, an[6]);
		writeI2c(baseAddr, 0x1A, an[5]);
		writeI2c(baseAddr, 0x1B, an[4]);
		writeI2c(baseAddr, 0x1C, an[3]);
		writeI2c(baseAddr, 0x1D, an[2]);
		writeI2c(baseAddr, 0x1E, an[1]);
		writeI2c(baseAddr, 0x1F, an[0]);
		/* write Aksv */
		writeI2c(baseAddr, 0x10, aksv[4]);
		writeI2c(baseAddr, 0x11, aksv[3]);
		writeI2c(baseAddr, 0x12, aksv[2]);
		writeI2c(baseAddr, 0x13, aksv[1]);
		writeI2c(baseAddr, 0x14, aksv[0]);
		/* read Bksv */
		bCaps = readI2c(baseAddr, 0x00);
		bCaps = readI2c(baseAddr, 0x01);
		bCaps = readI2c(baseAddr, 0x02);
		bCaps = readI2c(baseAddr, 0x03);
		bCaps = readI2c(baseAddr, 0x04);
		/* wait 100ms min */
		system_sleepms(110);
		/* Read r0' */
		bCaps = readI2c(baseAddr, 0x09);
		bCaps = readI2c(baseAddr, 0x08);
		/* see if device is ready - 5 seconds limit */
		i = 0;
		do
		{	/* read Bcaps */
			bCaps = readI2c(baseAddr, 0x40);
			system_sleepms(100);
			i++;
		}
		while ((((bCaps & BIT(5)) >> 5) == 0) && (i < HDCP_TIMEOUT_5S));
		if (i >= HDCP_TIMEOUT_5S)
		{
			break;
		}
		bStatus = readI2c(baseAddr, 0x41);
		bStatus |= (readI2c(baseAddr, 0x42) << 8);
		if ((bStatus & 0x007f) != 0)
		{	/* device count is no zero, controller takes over */
			LOG_NOTICE("device count > 0");
			break;
		}
		/* cannot recover - get out */
		haledid_maskinterrupts(baseAddr + 0x7E00, TRUE); /* disable interrupts */
		/* return interrupts mute to original state */
		halinterrupt_mutei2cclear(baseAddr + 0x0100, temp);
		error_set(ERR_HDCP_FAIL);
		LOG_ERROR("Repeater with Device Count Zero NOT supported");
		halinterrupt_i2cddcclear(baseAddr + 0x0100, 0xff); /* clear i2c interrupt */
		return FALSE;
	}
	while (1);
	haledid_maskinterrupts(baseAddr + 0x7E00, TRUE); /* disable interrupts */
	/* return interrupts mute to original state */
	halinterrupt_mutei2cclear(baseAddr + 0x0100, temp);
#endif
	LOG_TRACE();
	/* video related */
	halhdcp_devicemode(baseAddr + hdcp_base_addr, (hdmi == TRUE) ? 1 : 0);
	halhdcp_hsyncpolarity(baseAddr + hdcp_base_addr, (hsPol == TRUE) ? 1 : 0);
	halhdcp_vsyncpolarity(baseAddr + hdcp_base_addr, (vsPol == TRUE) ? 1 : 0);

	/* HDCP only */
	halhdcp_enablefeature11(baseAddr + hdcp_base_addr,
		(hdcpparams_getenable11feature(params) == TRUE) ? 1 : 0);
/*	halHdcp_RiCheck(baseAddr + hdcp_base_addr,
	(hdcpParams_GetRiCheck(params)== TRUE) ? 1 : 0);*/
	halhdcp_richeck(baseAddr + hdcp_base_addr,  1 );
	halhdcp_enablei2cfastmode(baseAddr + hdcp_base_addr,
		(hdcpparams_geti2cfastmode(params) == TRUE) ? 1 : 0);
	halhdcp_enhancedlinkverification(baseAddr + hdcp_base_addr,
		(hdcpparams_getenhancedlinkverification(params) == TRUE) ? 1 : 0);

	/* fixed */
	halhdcp_enableavmute(baseAddr + hdcp_base_addr, 0);
#if HDCP_ENABLE	//default bypass
	if (lge_get_factory_boot()) {
		halhdcp_bypassencryption(baseAddr + hdcp_base_addr, 1);
	} else {
		halhdcp_bypassencryption(baseAddr + hdcp_base_addr, 0);
	}
#else
	halhdcp_bypassencryption(baseAddr + hdcp_base_addr, 1);
#endif
	halhdcp_disableencryption(baseAddr + hdcp_base_addr, 0);
	halhdcp_unencryptedvideocolor(baseAddr + hdcp_base_addr, 0x00);
	halhdcp_oesswindowsize(baseAddr + hdcp_base_addr, 64);
	halhdcp_encodingpacketheader(baseAddr + hdcp_base_addr, 1);

	halhdcp_swreset(baseAddr + hdcp_base_addr);
	/* enable KSV list SHA1 verification interrupt */
	halhdcp_interruptclear(baseAddr + hdcp_base_addr, ~0);
	halhdcp_interruptmask(baseAddr + hdcp_base_addr, ~BIT(int_ksv_sha1));
	return TRUE;
}

u8 hdcp_eventhandler(u16 baseAddr, int hpd, u8 state, handler_t ksvHandler)
{
	unsigned int list = 0;
	unsigned int size = 0;
	unsigned int attempt = 0;
	unsigned int i = 0;
	int valid = FALSE;
	u8 *data = NULL;
	buffer_t buf = {0};
	LOG_TRACE();
	if ((state & BIT(int_ksv_sha1)) != 0)
	{
		valid = FALSE;
		halhdcp_memoryaccessrequest(baseAddr + hdcp_base_addr, 1);
		for (attempt = 20; attempt > 0; attempt--)
		{
			if (halhdcp_memoryaccessgranted(baseAddr + hdcp_base_addr) == 1)
			{
				list = (halhdcp_ksvlistread(baseAddr + hdcp_base_addr, 0)
						& ksv_msk) * ksv_len;
				size = list + header_g + shamax;
				data = (u8*) kmalloc(sizeof(u8) * size, GFP_KERNEL);
				if (data != 0)
				{
					for (i = 0; i < size; i++)
					{
						if (i < header_g)
						{ /* BSTATUS & M0 */
							data[list + i] = halhdcp_ksvlistread(baseAddr
									+ hdcp_base_addr, i);
						}
						else if (i < (header_g + list))
						{ /* KSV list */
							data[i - header_g] = halhdcp_ksvlistread(baseAddr
									+ hdcp_base_addr, i);
						}
						else
						{ /* SHA */
							data[i] = halhdcp_ksvlistread(baseAddr
									+ hdcp_base_addr, i);
						}
					}
					valid = hdcpverify_ksv(data, size);
#ifdef HW_WA_HDCP_REP
					for (i = 0; !valid && i != (u8)(~0); i++)
					{
						data[14] = i; /* M0 - LSB */
						valid = hdcpverify_ksv(data, size);
					}
#endif
					if (ksvHandler != NULL)
					{
						buf.buffer = data;
						buf.size = size;
						ksvHandler((void*)(&buf));
					}
					kfree(data);
				}
				else
				{
					error_set(ERR_MEM_ALLOCATION);
					LOG_ERROR("cannot allocate memory");
					return FALSE;
				}
				break;
			}
		}
		halhdcp_memoryaccessrequest(baseAddr + hdcp_base_addr, 0);

		halhdcp_updateksvliststate(baseAddr + hdcp_base_addr,
				(valid == TRUE) ? 0 : 1);
	}
	return TRUE;
}

void hdcp_rxdetected(u16 baseAddr, int detected)
{
	LOG_TRACE1(detected);
	halhdcp_rxdetected(baseAddr + hdcp_base_addr, (detected == TRUE) ? 1 : 0);
}

void hdcp_avmute(u16 baseAddr, int enable)
{
	LOG_TRACE1(enable);
	halhdcp_enableavmute(baseAddr + hdcp_base_addr, (enable == TRUE) ? 1 : 0);
}

void hdcp_bypassencryption(u16 baseAddr, int bypass)
{
	LOG_TRACE1(bypass);
	halhdcp_bypassencryption(baseAddr + hdcp_base_addr, (bypass == TRUE) ? 1
			: 0);
}

void hdcp_disableencryption(u16 baseAddr, int disable)
{
	LOG_TRACE1(disable);
	halhdcp_disableencryption(baseAddr + hdcp_base_addr, (disable == TRUE) ? 1
			: 0);
}

int hdcp_engaged(u16 baseAddr)
{
	LOG_TRACE();
	return (halhdcp_hdcpengaged(baseAddr + hdcp_base_addr) != 0);
}

u8 hdcp_authenticationstate(u16 baseAddr)
{
	/* hardware recovers automatically from a lost authentication */
	LOG_TRACE();
	return halhdcp_authenticationstate(baseAddr + hdcp_base_addr);
}

u8 hdcp_cipherstate(u16 baseAddr)
{
	LOG_TRACE();
	return halhdcp_cipherstate(baseAddr + hdcp_base_addr);
}

u8 hdcp_revocationstate(u16 baseAddr)
{
	LOG_TRACE();
	return halhdcp_revocationstate(baseAddr + hdcp_base_addr);
}

int hdcp_srmupdate(u16 baseAddr, const u8 * data)
{
	unsigned int size = 0;
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int attempt = 10;
	int success = FALSE;
	LOG_TRACE();

	for (i = 0; i < vrl_number; i++)
	{
		size <<= i * 8;
		size |= data[vrl_length + i];
	}
	size += vrl_header;

	if (hdcpverify_srm(data, size) == TRUE)
	{
		halhdcp_memoryaccessrequest(baseAddr + hdcp_base_addr, 1);
		for (attempt = 20; attempt > 0; attempt--)
		{
			if (halhdcp_memoryaccessgranted(baseAddr + hdcp_base_addr) == 1)
			{
				/* TODO fix only first-generation is handled */
				size -= (vrl_header + vrl_number + 2 * dsamax);
				/* write number of KSVs */
				halhdcp_revoclistwrite(baseAddr + hdcp_base_addr, 0,
						(size == 0)? 0 : data[vrl_length + vrl_number]);
				halhdcp_revoclistwrite(baseAddr + hdcp_base_addr, 1, 0);
				/* write KSVs */
				for (i = 1; i < size; i += ksv_len)
				{
					for (j = 1; j <= ksv_len; j++)
					{
						halhdcp_revoclistwrite(baseAddr + hdcp_base_addr,
								i + j, data[vrl_length + vrl_number + i
										+ (ksv_len - j)]);
					}
				}
				success = TRUE;
				LOG_NOTICE("SRM successfully updated");
				break;
			}
		}
		halhdcp_memoryaccessrequest(baseAddr + hdcp_base_addr, 0);
		if (!success)
		{
			error_set(ERR_HDCP_MEM_ACCESS);
			LOG_ERROR("cannot access memory");
		}
	}
	else
	{
		error_set(ERR_SRM_INVALID);
		LOG_ERROR("SRM invalid");
	}
	return success;
}

u8 hdcp_interruptstatus(u16 baseAddr)
{
		return halhdcp_interruptstatus(baseAddr + hdcp_base_addr);
}

int hdcp_interruptclear(u16 baseAddr, u8 value)
{
	halhdcp_interruptclear(baseAddr + hdcp_base_addr, value);
	return TRUE;
}
