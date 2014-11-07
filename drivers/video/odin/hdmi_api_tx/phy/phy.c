/*
 * phy.c
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */
 #include <linux/delay.h>
#include "phy.h"
#include "halSourcePhy.h"
#include "halI2cMasterPhy.h"
#include "../core/halMainController.h"
#include "../util/log.h"
#include "../util/error.h"
#include <linux/io.h>

static const u16 phy_base_addr = 0x3000;
static const u16 mc_base_addr = 0x4000;
static const u16 phy_i2cm_base_addr = 0x3020;
/*static const u8 phy_i2c_slave_addr = 0x54;	//Synopsys phy*/
static const u8 phy_i2c_slave_addr = 0x69;	/* LG phy*/

int phy_initialize(u16 baseAddr, u8 dataEnablePolarity)
{
	LOG_TRACE1(dataEnablePolarity);
/*#ifndef PHY_THIRD_PARTY*/
#ifdef PHY_GEN2_TSMC_40LP_2_5V
	halsourcephy_gen2txpoweron(baseAddr + phy_base_addr, 0);
	halsourcephy_gen2pddq(baseAddr + phy_base_addr, 1);
	LOG_NOTICE("GEN 2 TSMC 40LP 2.5V build - TQL");
#endif
#ifdef PHY_GEN2_TSMC_40G_1_8V
	LOG_NOTICE("GEN 2 TSMC 40G 1.8V build - E102");
	halsourcephy_gen2txpoweron(baseAddr + phy_base_addr, 0);
	halsourcephy_gen2pddq(baseAddr + phy_base_addr, 1);
#endif
#ifdef PHY_GEN2_TSMC_65LP_2_5V
	LOG_NOTICE("GEN 2 TSMC 65LP 2.5V build - E104");
	halsourcephy_gen2txpoweron(baseAddr + phy_base_addr, 0);
	halsourcephy_gen2pddq(baseAddr + phy_base_addr, 1);
#endif
#ifdef PHY_TNP
	LOG_NOTICE("TNP build");
#endif
#ifdef PHY_CNP
	LOG_NOTICE("CNP build");
#endif
	/* mask phy interrupts */
	halsourcephy_interruptmask(baseAddr + phy_base_addr, ~0);
	halsourcephy_dataenablepolarity(baseAddr + phy_base_addr,
			dataEnablePolarity);
	halsourcephy_interfacecontrol(baseAddr + phy_base_addr, 0);
	halsourcephy_enabletmds(baseAddr + phy_base_addr, 1);
	halsourcephy_powerdown(baseAddr + phy_base_addr, 0); /* disable PHY */

	return TRUE;
}

int phy_configure (u16 baseAddr, u16 pClk, u8 cRes, u8 pRep)
{
/*#ifndef PHY_THIRD_PARTY
//#ifdef PHY_CNP
//	u16 clk = 0;
//	u16 rep = 0;
//#endif*/
	u16 i = 0;
	LOG_TRACE();
	/*  colour resolution 0 is 8 bit colour depth */
	if (cRes == 0)
		cRes = 8;

	if (pRep != 0)
	{
		error_set(ERR_PIXEL_REPETITION_NOT_SUPPORTED);
		LOG_ERROR2("pixel repetition not supported", pRep);
		return FALSE;
	}

	/* The following is only for PHY_GEN1_CNP, and 1v0 NOT 1v1 */
	/* values are found in document HDMISRC1UHTCcnp_IPCS_DS_0v3.doc
	 * for the HDMISRCGPHIOcnp
	 */
	/* in the cnp PHY interface, the 3 most significant bits are ctrl (which
	 * part block to write) and the last 5 bits are data */
	/* for example 0x6A4a is writing to block  3 (ie. [14:10]) (5-bit blocks)
	 * the bits 0x0A, and  block 2 (ie. [9:5]) the bits 0x0A */
	/* configure PLL after core pixel repetition */
#ifdef PHY_GEN2_TSMC_40LP_2_5V
	halsourcephy_gen2txpoweron(baseAddr + phy_base_addr, 0);
	halsourcephy_gen2pddq(baseAddr + phy_base_addr, 1);
	halmaincontroller_phyreset(baseAddr + mc_base_addr, 0);
	halmaincontroller_phyreset(baseAddr + mc_base_addr, 1);
	halmaincontroller_heacphyreset(baseAddr + mc_base_addr, 1);
	halsourcephy_testclear(baseAddr + phy_base_addr, 1);
	hali2cmasterphy_slaveaddress(
		baseAddr + phy_i2cm_base_addr, phy_i2c_slave_addr);
	halsourcephy_testclear(baseAddr + phy_base_addr, 0);

	phy_i2cwrite(baseAddr, 0x0000, 0x13); /* PLLPHBYCTRL */
	phy_i2cwrite(baseAddr, 0x0006, 0x17);
	/* RESISTANCE TERM 133Ohm Cfg  */
	phy_i2cwrite(baseAddr, 0x0005, 0x19); /* TXTERM */
	/* REMOVE CLK TERM */
	phy_i2cwrite(baseAddr, 0x8000, 0x05); /* CKCALCTRL */
	switch (pClk)
	{
		case 2520:
			switch (cRes)
			{
				case 8:
					/* PLL/MPLL Cfg */
					phy_i2cwrite(baseAddr, 0x01e0, 0x06);
					phy_i2cwrite(baseAddr, 0x091c, 0x10); /* CURRCTRL */
					phy_i2cwrite(baseAddr, 0x0000, 0x15); /* GMPCTRL */
					break;
				case 10:
					phy_i2cwrite(baseAddr, 0x21e1, 0x06);
					phy_i2cwrite(baseAddr, 0x091c, 0x10);
					phy_i2cwrite(baseAddr, 0x0000, 0x15);
					break;
				case 12:
					phy_i2cwrite(baseAddr, 0x41e2, 0x06);
					phy_i2cwrite(baseAddr, 0x06dc, 0x10);
					phy_i2cwrite(baseAddr, 0x0000, 0x15);
					break;
				default:
					error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
					LOG_ERROR2("color depth not supported", cRes);
					return FALSE;
			}
			/* PREEMP Cgf 0.00 */
			phy_i2cwrite(baseAddr, 0x8009, 0x09); /* CKSYMTXCTRL */
			/* TX/CK LVL 10 */
			phy_i2cwrite(baseAddr, 0x0210, 0x0E); /* VLEVCTRL */
			break;
		case 2700:
			switch (cRes)
			{
				case 8:
					phy_i2cwrite(baseAddr, 0x01e0, 0x06);
					phy_i2cwrite(baseAddr, 0x091c, 0x10);
					phy_i2cwrite(baseAddr, 0x0000, 0x15);
					break;
				case 10:
					phy_i2cwrite(baseAddr, 0x21e1, 0x06);
					phy_i2cwrite(baseAddr, 0x091c, 0x10);
					phy_i2cwrite(baseAddr, 0x0000, 0x15);
					break;
				case 12:
					phy_i2cwrite(baseAddr, 0x41e2, 0x06);
					phy_i2cwrite(baseAddr, 0x06dc, 0x10);
					phy_i2cwrite(baseAddr, 0x0000, 0x15);
					break;
				default:
					error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
					LOG_ERROR2("color depth not supported", cRes);
					return FALSE;
			}
			phy_i2cwrite(baseAddr, 0x8009, 0x09);
			phy_i2cwrite(baseAddr, 0x0210, 0x0E);
			break;
		case 5400:
			switch (cRes)
			{
				case 8:
					phy_i2cwrite(baseAddr, 0x0140, 0x06);
					phy_i2cwrite(baseAddr, 0x091c, 0x10);
					phy_i2cwrite(baseAddr, 0x0005, 0x15);
					break;
				case 10:
					phy_i2cwrite(baseAddr, 0x2141, 0x06);
					phy_i2cwrite(baseAddr, 0x091c, 0x10);
					phy_i2cwrite(baseAddr, 0x0005, 0x15);
					break;
				case 12:
					phy_i2cwrite(baseAddr, 0x4142, 0x06);
					phy_i2cwrite(baseAddr, 0x06dc, 0x10);
					phy_i2cwrite(baseAddr, 0x0005, 0x15);
					break;
				default:
					error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
					LOG_ERROR2("color depth not supported", cRes);
					return FALSE;
			}
			phy_i2cwrite(baseAddr, 0x8009, 0x09);
			phy_i2cwrite(baseAddr, 0x0210, 0x0E);
			break;
		case 7200:
			switch (cRes)
			{
				case 8:
					phy_i2cwrite(baseAddr, 0x0140, 0x06);
					phy_i2cwrite(baseAddr, 0x06dc, 0x10);
					phy_i2cwrite(baseAddr, 0x0005, 0x15);
					break;
				case 10:
					phy_i2cwrite(baseAddr, 0x2141, 0x06);
					phy_i2cwrite(baseAddr, 0x06dc, 0x10);
					phy_i2cwrite(baseAddr, 0x0005, 0x15);
					break;
				case 12:
					phy_i2cwrite(baseAddr, 0x40a2, 0x06);
					phy_i2cwrite(baseAddr, 0x091c, 0x10);
					phy_i2cwrite(baseAddr, 0x000a, 0x15);
					break;
				default:
					error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
					LOG_ERROR2("color depth not supported", cRes);
					return FALSE;
			}
			phy_i2cwrite(baseAddr, 0x8009, 0x09);
			phy_i2cwrite(baseAddr, 0x0210, 0x0E);
			break;
		case 7425:
			switch (cRes)
			{
				case 8:
					phy_i2cwrite(baseAddr, 0x0140, 0x06);
					phy_i2cwrite(baseAddr, 0x06dc, 0x10);
					phy_i2cwrite(baseAddr, 0x0005, 0x15);
					break;
				case 10:
					phy_i2cwrite(baseAddr, 0x20a1, 0x06);
					phy_i2cwrite(baseAddr, 0x0b5c, 0x10);
					phy_i2cwrite(baseAddr, 0x000a, 0x15);
					break;
				case 12:
					phy_i2cwrite(baseAddr, 0x40a2, 0x06);
					phy_i2cwrite(baseAddr, 0x091c, 0x10);
					phy_i2cwrite(baseAddr, 0x000a, 0x15);
					break;
				default:
					error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
					LOG_ERROR2("color depth not supported", cRes);
					return FALSE;
			}
			phy_i2cwrite(baseAddr, 0x8009, 0x09);
			phy_i2cwrite(baseAddr, 0x0210, 0x0E);
			break;
		case 10800:
			switch (cRes)
			{
				case 8:
					phy_i2cwrite(baseAddr, 0x00a0, 0x06);
					phy_i2cwrite(baseAddr, 0x091c, 0x10);
					phy_i2cwrite(baseAddr, 0x000a, 0x15);
					break;
				case 10:
					phy_i2cwrite(baseAddr, 0x20a1, 0x06);
					phy_i2cwrite(baseAddr, 0x091c, 0x10);
					phy_i2cwrite(baseAddr, 0x000a, 0x15);
					break;
				case 12:
					phy_i2cwrite(baseAddr, 0x40a2, 0x06);
					phy_i2cwrite(baseAddr, 0x06dc, 0x10);
					phy_i2cwrite(baseAddr, 0x000a, 0x15);
					break;
				default:
					error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
					LOG_ERROR2("color depth not supported", cRes);
					return FALSE;
			}
			phy_i2cwrite(baseAddr, 0x8009, 0x09);
			phy_i2cwrite(baseAddr, 0x0210, 0x0E);
			break;
		case 14850:
			switch (cRes)
			{
				case 8:
					phy_i2cwrite(baseAddr, 0x00a0, 0x06);
					phy_i2cwrite(baseAddr, 0x06dc, 0x10);
					phy_i2cwrite(baseAddr, 0x000a, 0x15);
					phy_i2cwrite(baseAddr, 0x8009, 0x09);
					phy_i2cwrite(baseAddr, 0x0210, 0x0E);
					break;
				case 10:
					phy_i2cwrite(baseAddr, 0x2001, 0x06);
					phy_i2cwrite(baseAddr, 0x0b5c, 0x10);
					phy_i2cwrite(baseAddr, 0x000f, 0x15);
					phy_i2cwrite(baseAddr, 0x8009, 0x09);
					phy_i2cwrite(baseAddr, 0x0210, 0x0E);
					break;
				case 12:
					phy_i2cwrite(baseAddr, 0x4002, 0x06);
					phy_i2cwrite(baseAddr, 0x091c, 0x10);
					phy_i2cwrite(baseAddr, 0x000f, 0x15);
					phy_i2cwrite(baseAddr, 0x800b, 0x09);
					phy_i2cwrite(baseAddr, 0x0129, 0x0E);
					break;
				default:
					error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
					LOG_ERROR2("color depth not supported", cRes);
					return FALSE;
			}
			break;
		default:
			error_set(ERR_PIXEL_CLOCK_NOT_SUPPORTED);
			LOG_ERROR2("pixel clock not supported", pClk);
			return FALSE;
	}
	halsourcephy_gen2enhpdrxsense(baseAddr + phy_base_addr, 1);
	halsourcephy_gen2txpoweron(baseAddr + phy_base_addr, 1);
	halsourcephy_gen2pddq(baseAddr + phy_base_addr, 0);
#else
#ifdef PHY_GEN2_TSMC_40G_1_8V
	halsourcephy_gen2txpoweron(baseAddr + phy_base_addr, 0);
	halsourcephy_gen2pddq(baseAddr + phy_base_addr, 1);
	halmaincontroller_phyreset(baseAddr + mc_base_addr, 0);
	halmaincontroller_phyreset(baseAddr + mc_base_addr, 1);
	halmaincontroller_heacphyreset(baseAddr + mc_base_addr, 1);
	halsourcephy_testclear(baseAddr + phy_base_addr, 1);
	hali2cmasterphy_slaveaddress(
		baseAddr + phy_i2cm_base_addr, phy_i2c_slave_addr);
	halsourcephy_testclear(baseAddr + phy_base_addr, 0);

	phy_i2cwrite(baseAddr, 0x0000, 0x13);
	phy_i2cwrite(baseAddr, 0x0002, 0x19);
	phy_i2cwrite(baseAddr, 0x0006, 0x17);
	phy_i2cwrite(baseAddr, 0x8000, 0x05);

	switch (pClk)
	{
		case 2520:
			switch (cRes)
			{
				case 8:
					phy_i2cwrite(baseAddr, 0x01e0, 0x06);
					phy_i2cwrite(baseAddr, 0x08da, 0x10);
					phy_i2cwrite(baseAddr, 0x0000, 0x15);
					break;
				case 10:
					phy_i2cwrite(baseAddr, 0x21e1, 0x06);
					phy_i2cwrite(baseAddr, 0x08da, 0x10);
					phy_i2cwrite(baseAddr, 0x0000, 0x15);
					break;
				case 12:
					phy_i2cwrite(baseAddr, 0x41e2, 0x06);
					phy_i2cwrite(baseAddr, 0x065a, 0x10);
					phy_i2cwrite(baseAddr, 0x0000, 0x15);
					break;
				default:
					error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
					LOG_ERROR2("color depth not supported", cRes);
					return FALSE;
			}
			phy_i2cwrite(baseAddr, 0x8009, 0x09);
			phy_i2cwrite(baseAddr, 0x0231, 0x0E);
			break;
		case 2700:
			switch (cRes)
			{
				case 8:
					phy_i2cwrite(baseAddr, 0x01e0, 0x06);
					phy_i2cwrite(baseAddr, 0x08da, 0x10);
					phy_i2cwrite(baseAddr, 0x0000, 0x15);
					break;
				case 10:
					phy_i2cwrite(baseAddr, 0x21e1, 0x06);
					phy_i2cwrite(baseAddr, 0x08da, 0x10);
					phy_i2cwrite(baseAddr, 0x0000, 0x15);
					break;
				case 12:
					phy_i2cwrite(baseAddr, 0x41e2, 0x06);
					phy_i2cwrite(baseAddr, 0x065a, 0x10);
					phy_i2cwrite(baseAddr, 0x0000, 0x15);
					break;
				default:
					error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
					LOG_ERROR2("color depth not supported", cRes);
					return FALSE;
			}
			phy_i2cwrite(baseAddr, 0x8009, 0x09);
			phy_i2cwrite(baseAddr, 0x0231, 0x0E);
			break;
		case 5400:
			switch (cRes)
			{
				case 8:
					phy_i2cwrite(baseAddr, 0x0140, 0x06);
					phy_i2cwrite(baseAddr, 0x09da, 0x10);
					phy_i2cwrite(baseAddr, 0x0005, 0x15);
					break;
				case 10:
					phy_i2cwrite(baseAddr, 0x2141, 0x06);
					phy_i2cwrite(baseAddr, 0x09da, 0x10);
					phy_i2cwrite(baseAddr, 0x0005, 0x15);
					break;
				case 12:
					phy_i2cwrite(baseAddr, 0x4142, 0x06);
					phy_i2cwrite(baseAddr, 0x079a, 0x10);
					phy_i2cwrite(baseAddr, 0x0005, 0x15);
					break;
				default:
					error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
					LOG_ERROR2("color depth not supported", cRes);
					return FALSE;
			}
			phy_i2cwrite(baseAddr, 0x8009, 0x09);
			phy_i2cwrite(baseAddr, 0x0231, 0x0E);
			break;
		case 6500:
		case 7200:
			switch (cRes)
			{
				case 8:
					phy_i2cwrite(baseAddr, 0x0140, 0x06);
					phy_i2cwrite(baseAddr, 0x079a, 0x10);
					phy_i2cwrite(baseAddr, 0x0005, 0x15);
					break;
				case 10:
					phy_i2cwrite(baseAddr, 0x2141, 0x06);
					phy_i2cwrite(baseAddr, 0x079a, 0x10);
					phy_i2cwrite(baseAddr, 0x0005, 0x15);
					break;
				case 12:
					phy_i2cwrite(baseAddr, 0x40a2, 0x06);
					phy_i2cwrite(baseAddr, 0x0a5a, 0x10);
					phy_i2cwrite(baseAddr, 0x000a, 0x15);
					break;
				default:
					error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
					LOG_ERROR2("color depth not supported", cRes);
					return FALSE;
			}
			phy_i2cwrite(baseAddr, 0x8009, 0x09);
			phy_i2cwrite(baseAddr, 0x0231, 0x0E);
			break;
		case 7425:
			switch (cRes)
			{
				case 8:
					phy_i2cwrite(baseAddr, 0x0140, 0x06);
					phy_i2cwrite(baseAddr, 0x079a, 0x10);
					phy_i2cwrite(baseAddr, 0x0005, 0x15);
					break;
				case 10:
					phy_i2cwrite(baseAddr, 0x20a1, 0x06);
					phy_i2cwrite(baseAddr, 0x0bda, 0x10);
					phy_i2cwrite(baseAddr, 0x000a, 0x15);
					break;
				case 12:
					phy_i2cwrite(baseAddr, 0x40a2, 0x06);
					phy_i2cwrite(baseAddr, 0x0a5a, 0x10);
					phy_i2cwrite(baseAddr, 0x000a, 0x15);
					break;
				default:
					error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
					LOG_ERROR2("color depth not supported", cRes);
					return FALSE;
			}
			phy_i2cwrite(baseAddr, 0x8009, 0x09);
			phy_i2cwrite(baseAddr, 0x0231, 0x0E);
			break;
		case 10800:
			switch (cRes)
			{
				case 8:
					phy_i2cwrite(baseAddr, 0x00a0, 0x06);
					phy_i2cwrite(baseAddr, 0x091c, 0x10);
					phy_i2cwrite(baseAddr, 0x000a, 0x15);
					break;
				case 10:
					phy_i2cwrite(baseAddr, 0x20a1, 0x06);
					phy_i2cwrite(baseAddr, 0x091c, 0x10);
					phy_i2cwrite(baseAddr, 0x000a, 0x15);
					break;
				case 12:
					phy_i2cwrite(baseAddr, 0x40a2, 0x06);
					phy_i2cwrite(baseAddr, 0x06dc, 0x10);
					phy_i2cwrite(baseAddr, 0x000a, 0x15);
					break;
				default:
					error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
					LOG_ERROR2("color depth not supported", cRes);
					return FALSE;
			}
			phy_i2cwrite(baseAddr, 0x8009, 0x09);
			phy_i2cwrite(baseAddr, 0x0231, 0x0E);
			break;
		case 13000:
		case 14850:
			switch (cRes)
			{
				case 8:
					phy_i2cwrite(baseAddr, 0x00a0, 0x06);
					phy_i2cwrite(baseAddr, 0x079a, 0x10);
					phy_i2cwrite(baseAddr, 0x000a, 0x15);
					phy_i2cwrite(baseAddr, 0x8009, 0x09);
					phy_i2cwrite(baseAddr, 0x0231, 0x0E);
					break;
				case 10:
					phy_i2cwrite(baseAddr, 0x2001, 0x06);
					phy_i2cwrite(baseAddr, 0x0bda, 0x10);
					phy_i2cwrite(baseAddr, 0x000f, 0x15);
					phy_i2cwrite(baseAddr, 0x800b, 0x09);
					phy_i2cwrite(baseAddr, 0x014a, 0x0E);
					break;
				case 12:
					phy_i2cwrite(baseAddr, 0x4002, 0x06);
					phy_i2cwrite(baseAddr, 0x0a5a, 0x10);
					phy_i2cwrite(baseAddr, 0x000f, 0x15);
					phy_i2cwrite(baseAddr, 0x800b, 0x09);
					phy_i2cwrite(baseAddr, 0x014a, 0x0E);
					break;
				case 16:
					phy_i2cwrite(baseAddr, 0x6003, 0x06);
					phy_i2cwrite(baseAddr, 0x07da, 0x10);
					phy_i2cwrite(baseAddr, 0x000f, 0x15);
					phy_i2cwrite(baseAddr, 0x800b, 0x09);
					phy_i2cwrite(baseAddr, 0x014a, 0x0E);
					break;
				default:
					error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
					LOG_ERROR2("color depth not supported", cRes);
					return FALSE;
			}
			break;
		case 34000:
			switch (cRes)
			{
				case 8:
					phy_i2cwrite(baseAddr, 0x0000, 0x06);
					phy_i2cwrite(baseAddr, 0x07da, 0x10);
					phy_i2cwrite(baseAddr, 0x000f, 0x15);
					phy_i2cwrite(baseAddr, 0x800b, 0x09);
					phy_i2cwrite(baseAddr, 0x0108, 0x0E);
					break;
				default:
					error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
					LOG_ERROR2("color depth not supported", cRes);
					return FALSE;
			}
			break;
		default:
			error_set(ERR_PIXEL_CLOCK_NOT_SUPPORTED);
			LOG_ERROR2("pixel clock not supported", pClk);
			return FALSE;
	}
	halsourcephy_gen2enhpdrxsense(baseAddr + phy_base_addr, 1);
	halsourcephy_gen2txpoweron(baseAddr + phy_base_addr, 1);
	halsourcephy_gen2pddq(baseAddr + phy_base_addr, 0);
#else
#ifdef PHY_GEN2_TSMC_65LP_2_5V
	halsourcephy_gen2txpoweron(baseAddr + phy_base_addr, 0);
	halsourcephy_gen2pddq(baseAddr + phy_base_addr, 1);
	halmaincontroller_phyreset(baseAddr + mc_base_addr, 0);
	halmaincontroller_phyreset(baseAddr + mc_base_addr, 1);
	halmaincontroller_heacphyreset(baseAddr + mc_base_addr, 1);
	halsourcephy_testclear(baseAddr + phy_base_addr, 1);
	hali2cmasterphy_slaveaddress(
		baseAddr + phy_i2cm_base_addr, phy_i2c_slave_addr);
	halsourcephy_testclear(baseAddr + phy_base_addr, 0);
	phy_i2cwrite(baseAddr, 0x8009, 0x09);
	phy_i2cwrite(baseAddr, 0x0004, 0x19);
	phy_i2cwrite(baseAddr, 0x0000, 0x13);
	phy_i2cwrite(baseAddr, 0x0006, 0x17);
	phy_i2cwrite(baseAddr, 0x8000, 0x05);
	switch (pClk)
		{
			case 2520:
				switch (cRes)
				{
					case 8:
						phy_i2cwrite(baseAddr, 0x01E0, 0x06);
						phy_i2cwrite(baseAddr, 0x08D9, 0x10);
						phy_i2cwrite(baseAddr, 0x0000, 0x15);
						break;
					case 10:
						phy_i2cwrite(baseAddr, 0x21E1, 0x06);
						phy_i2cwrite(baseAddr, 0x08D9, 0x10);
						phy_i2cwrite(baseAddr, 0x0000, 0x15);
						break;
					case 12:
						phy_i2cwrite(baseAddr, 0x41E2, 0x06);
						phy_i2cwrite(baseAddr, 0x0659, 0x10);
						phy_i2cwrite(baseAddr, 0x0000, 0x15);
						break;
					case 16:
						phy_i2cwrite(baseAddr, 0x6143, 0x06);
						phy_i2cwrite(baseAddr, 0x0999, 0x10);
						phy_i2cwrite(baseAddr, 0x0005, 0x15);
						break;
					default:
						error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
						LOG_ERROR2("color depth not supported", cRes);
						return FALSE;
				}
				phy_i2cwrite(baseAddr, 0x0231, 0x0E);
				break;
			case 2700:
				switch (cRes)
				{
					case 8:
						phy_i2cwrite(baseAddr, 0x01E0, 0x06);
						phy_i2cwrite(baseAddr, 0x08D9, 0x10);
						phy_i2cwrite(baseAddr, 0x0000, 0x15);
						break;
					case 10:
						phy_i2cwrite(baseAddr, 0x21E1, 0x06);
						phy_i2cwrite(baseAddr, 0x08D9, 0x10);
						phy_i2cwrite(baseAddr, 0x0000, 0x15);
						break;
					case 12:
						phy_i2cwrite(baseAddr, 0x41E2, 0x06);
						phy_i2cwrite(baseAddr, 0x0659, 0x10);
						phy_i2cwrite(baseAddr, 0x0000, 0x15);
						break;
					case 16:
						phy_i2cwrite(baseAddr, 0x6143, 0x06);
						phy_i2cwrite(baseAddr, 0x0999, 0x10);
						phy_i2cwrite(baseAddr, 0x0005, 0x15);
						break;
					default:
						error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
						LOG_ERROR2("color depth not supported", cRes);
						return FALSE;
				}
				phy_i2cwrite(baseAddr, 0x0231, 0x0E);
				break;
			case 5400:
				switch (cRes)
				{
					case 8:
						phy_i2cwrite(baseAddr, 0x0140, 0x06);
						phy_i2cwrite(baseAddr, 0x0999, 0x10);
						phy_i2cwrite(baseAddr, 0x0005, 0x15);
						break;
					case 10:
						phy_i2cwrite(baseAddr, 0x2141, 0x06);
						phy_i2cwrite(baseAddr, 0x0999, 0x10);
						phy_i2cwrite(baseAddr, 0x0005, 0x15);
						break;
					case 12:
						phy_i2cwrite(baseAddr, 0x4142, 0x06);
						phy_i2cwrite(baseAddr, 0x06D9, 0x10);
						phy_i2cwrite(baseAddr, 0x0005, 0x15);
						break;
					case 16:
						phy_i2cwrite(baseAddr, 0x60A3, 0x06);
						phy_i2cwrite(baseAddr, 0x09D9, 0x10);
						phy_i2cwrite(baseAddr, 0x000A, 0x15);
						break;
					default:
						error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
						LOG_ERROR2("color depth not supported", cRes);
						return FALSE;
				}
				phy_i2cwrite(baseAddr, 0x0231, 0x0E);
				break;
			case 7200:
				switch (cRes)
				{
					case 8:
						phy_i2cwrite(baseAddr, 0x0140, 0x06);
						phy_i2cwrite(baseAddr, 0x06D9, 0x10);
						phy_i2cwrite(baseAddr, 0x0005, 0x15);
						break;
					case 10:
						phy_i2cwrite(baseAddr, 0x2141, 0x06);
						phy_i2cwrite(baseAddr, 0x06D9, 0x10);
						phy_i2cwrite(baseAddr, 0x0005, 0x15);
						break;
					case 12:
						phy_i2cwrite(baseAddr, 0x40A2, 0x06);
						phy_i2cwrite(baseAddr, 0x09D9, 0x10);
						phy_i2cwrite(baseAddr, 0x000A, 0x15);
						break;
					case 16:
						phy_i2cwrite(baseAddr, 0x60A3, 0x06);
						phy_i2cwrite(baseAddr, 0x06D9, 0x10);
						phy_i2cwrite(baseAddr, 0x000A, 0x15);
						break;
					default:
						error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
						LOG_ERROR2("color depth not supported", cRes);
						return FALSE;
				}
				break;
				phy_i2cwrite(baseAddr, 0x0231, 0x0E);
			case 7425:
				switch (cRes)
				{
					case 8:
						phy_i2cwrite(baseAddr, 0x0140, 0x06);
						phy_i2cwrite(baseAddr, 0x06D9, 0x10);
						phy_i2cwrite(baseAddr, 0x0005, 0x15);
						break;
					case 10:
						phy_i2cwrite(baseAddr, 0x20A1, 0x06);
						phy_i2cwrite(baseAddr, 0x0BD9, 0x10);
						phy_i2cwrite(baseAddr, 0x000A, 0x15);
						break;
					case 12:
						phy_i2cwrite(baseAddr, 0x40A2, 0x06);
						phy_i2cwrite(baseAddr, 0x09D9, 0x10);
						phy_i2cwrite(baseAddr, 0x000A, 0x15);
						break;
					case 16:
						phy_i2cwrite(baseAddr, 0x60A3, 0x06);
						phy_i2cwrite(baseAddr, 0x06D9, 0x10);
						phy_i2cwrite(baseAddr, 0x000A, 0x15);
						break;
					default:
						error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
						LOG_ERROR2("color depth not supported", cRes);
						return FALSE;
				}
				phy_i2cwrite(baseAddr, 0x0231, 0x0E);
				break;
			case 10800:
				switch (cRes)
				{
					case 8:
						phy_i2cwrite(baseAddr, 0x00A0, 0x06);
						phy_i2cwrite(baseAddr, 0x09D9, 0x10);
						phy_i2cwrite(baseAddr, 0x000A, 0x15);
						phy_i2cwrite(baseAddr, 0x0231, 0x0E);
						break;
					case 10:
						phy_i2cwrite(baseAddr, 0x20A1, 0x06);
						phy_i2cwrite(baseAddr, 0x09D9, 0x10);
						phy_i2cwrite(baseAddr, 0x000A, 0x15);
						phy_i2cwrite(baseAddr, 0x0231, 0x0E);
						break;
					case 12:
						phy_i2cwrite(baseAddr, 0x40A2, 0x06);
						phy_i2cwrite(baseAddr, 0x06D9, 0x10);
						phy_i2cwrite(baseAddr, 0x000A, 0x15);
						phy_i2cwrite(baseAddr, 0x014A, 0x0E);
						break;
					case 16:
						phy_i2cwrite(baseAddr, 0x6003, 0x06);
						phy_i2cwrite(baseAddr, 0x09D9, 0x10);
						phy_i2cwrite(baseAddr, 0x000F, 0x15);
						phy_i2cwrite(baseAddr, 0x014A, 0x0E);
						break;
					default:
						error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
						LOG_ERROR2("color depth not supported", cRes);
						return FALSE;
				}
				break;
			case 14850:
				switch (cRes)
				{
					case 8:
						phy_i2cwrite(baseAddr, 0x00A0, 0x06);
						phy_i2cwrite(baseAddr, 0x06D9, 0x10);
						phy_i2cwrite(baseAddr, 0x000A, 0x15);
						phy_i2cwrite(baseAddr, 0x0231, 0x0E);
						break;
					case 10:
						phy_i2cwrite(baseAddr, 0x2001, 0x06);
						phy_i2cwrite(baseAddr, 0x0BD9, 0x10);
						phy_i2cwrite(baseAddr, 0x000F, 0x15);
						phy_i2cwrite(baseAddr, 0x014A, 0x0E);
						break;
					case 12:
						phy_i2cwrite(baseAddr, 0x4002, 0x06);
						phy_i2cwrite(baseAddr, 0x09D9, 0x10);
						phy_i2cwrite(baseAddr, 0x000F, 0x15);
						phy_i2cwrite(baseAddr, 0x014A, 0x0E);
						break;
					case 16:
						phy_i2cwrite(baseAddr, 0x6003, 0x06);
						phy_i2cwrite(baseAddr, 0x0719, 0x10);
						phy_i2cwrite(baseAddr, 0x000F, 0x15);
						phy_i2cwrite(baseAddr, 0x014A, 0x0E);
						break;
					default:
						error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
						LOG_ERROR2("color depth not supported", cRes);
						return FALSE;
				}
				break;
			case 29700:
				switch (cRes)
				{
					case 8:
						phy_i2cwrite(baseAddr, 0x0000, 0x06);
						phy_i2cwrite(baseAddr, 0x0719, 0x10);
						phy_i2cwrite(baseAddr, 0x000F, 0x15);
						phy_i2cwrite(baseAddr, 0x014A, 0x0E);
						break;
					default:
						error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
						LOG_ERROR2("color depth not supported", cRes);
						return FALSE;
				}
				break;

			case 34000:
				switch (cRes)
				{
					case 8:
						phy_i2cwrite(baseAddr, 0x0000, 0x06);
						phy_i2cwrite(baseAddr, 0x0719, 0x10);
						phy_i2cwrite(baseAddr, 0x000F, 0x15);
						phy_i2cwrite(baseAddr, 0x0129, 0x0E);
						break;
					default:
						error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
						LOG_ERROR2("color depth not supported", cRes);
						return FALSE;
				}
		}

	halsourcephy_gen2enhpdrxsense(baseAddr + phy_base_addr, 1);
	halsourcephy_gen2txpoweron(baseAddr + phy_base_addr, 1);
	halsourcephy_gen2pddq(baseAddr + phy_base_addr, 0);
#else
	if (cRes != 8 && cRes != 12)
	{
		error_set(ERR_COLOR_DEPTH_NOT_SUPPORTED);
		LOG_ERROR2("color depth not supported", cRes);
		return FALSE;
	}
	halsourcephy_testclear(baseAddr + phy_base_addr, 0);
	halsourcephy_testclear(baseAddr + phy_base_addr, 1);
	halsourcephy_testclear(baseAddr + phy_base_addr, 0);
#ifndef PHY_TNP
	switch (pClk)
	{
		case 2520:
			clk = 0x93C1;
			rep = (cRes == 8) ? 0x6A4A : 0x6653;
			break;
		case 2700:
			clk = 0x96C1;
			rep = (cRes == 8) ? 0x6A4A : 0x6653;
			break;
		case 5400:
			clk = 0x8CC3;
			rep = (cRes == 8) ? 0x6A4A : 0x6653;
			break;
		case 7200:
			clk = 0x90C4;
			rep = (cRes == 8) ? 0x6A4A : 0x6654;
			break;
		case 7425:
			clk = 0x95C8;
			rep = (cRes == 8) ? 0x6A4A : 0x6654;
			break;
		case 10800:
			clk = 0x98C6;
			rep = (cRes == 8) ? 0x6A4A : 0x6653;
			break;
		case 14850:
			clk = 0x89C9;
			rep = (cRes == 8) ? 0x6A4A : 0x6654;
			break;
		default:
			error_set(ERR_PIXEL_CLOCK_NOT_SUPPORTED);
			LOG_ERROR2("pixel clock not supported", pClk);
			return FALSE;
	}
	halsourcephy_testclock(baseAddr + phy_base_addr, 0);
	halsourcephy_testenable(baseAddr + phy_base_addr, 0);
	if (phy_testcontrol(baseAddr, 0x1B) != TRUE)
	{
		error_set(ERR_PHY_TEST_CONTROL);
		return FALSE;
	}
	phy_testdata(baseAddr, (u8)(clk >> 8));
	phy_testdata(baseAddr, (u8)(clk >> 0));
	if (phy_testcontrol(baseAddr, 0x1A) != TRUE)
	{
		error_set(ERR_PHY_TEST_CONTROL);
		return FALSE;
	}
	phy_testdata(baseAddr, (u8)(rep >> 8));
	phy_testdata(baseAddr, (u8)(rep >> 0));
#endif
	if (pClk == 14850 && cRes == 12)
	{
		LOG_NOTICE("Applying Pre-Emphasis");
		if (phy_testcontrol(baseAddr, 0x24) != TRUE)
		{
			error_set(ERR_PHY_TEST_CONTROL);
			return FALSE;
		}
		phy_testdata(baseAddr, 0x80);
		phy_testdata(baseAddr, 0x90);
		phy_testdata(baseAddr, 0xa0);
#ifndef PHY_TNP
		phy_testdata(baseAddr, 0xb0);
		if (phy_testcontrol(baseAddr, 0x20) != TRUE)
		{ /*  +11.1ma 3.3 pe */
			error_set(ERR_PHY_TEST_CONTROL);
			return FALSE;
		}
		phy_testdata(baseAddr, 0x04);
		if (phy_testcontrol(baseAddr, 0x21) != TRUE) /*  +11.1 +2ma 3.3 pe */
		{
			error_set(ERR_PHY_TEST_CONTROL);
			return FALSE;
		}
		phy_testdata(baseAddr, 0x2a);

		if (phy_testcontrol(baseAddr, 0x11) != TRUE)
		{
			error_set(ERR_PHY_TEST_CONTROL);
			return FALSE;
		}
		phy_testdata(baseAddr, 0xf3);
		phy_testdata(baseAddr, 0x93);
#else
		if (phy_testcontrol(baseAddr, 0x20) != TRUE)
		{
			error_Set(ERR_PHY_TEST_CONTROL);
			return FALSE;
		}
		phy_testdata(baseAddr, 0x00);
		if (phy_testcontrol(baseAddr, 0x21) != TRUE)
		{
			error_set(ERR_PHY_TEST_CONTROL);
			return FALSE;
		}
		phy_testdata(baseAddr, 0x00);
#endif
	}
	if (phy_testcontrol(baseAddr, 0x00) != TRUE)
	{
		error_set(ERR_PHY_TEST_CONTROL);
		return FALSE;
	}
	halsourcephy_testdatain(baseAddr + phy_base_addr, 0x00);
	halmaincontroller_phyreset(baseAddr + mc_base_addr, 1); /*  reset PHY */
	halmaincontroller_phyreset(baseAddr + mc_base_addr, 0);
	halsourcephy_powerdown(baseAddr + phy_base_addr, 1); /* enable PHY */
	halsourcephy_enabletmds(baseAddr + phy_base_addr, 0); /*  toggle TMDS */
	halsourcephy_enabletmds(baseAddr + phy_base_addr, 1);
#endif
#endif
#endif
	/* wait PHY_TIMEOUT no of cycles at most
	for the pll lock signal to raise ~around 20us max */
	for (i = 1; i < PHY_TIMEOUT; i++)
	{
		mdelay(10);
		if ((i % 10) == 0)
		{
			if (halsourcephy_phaselockloopstate(baseAddr + phy_base_addr) == TRUE)
			{
				mdelay(10);
				break;
			}
		}
	}
	if (halsourcephy_phaselockloopstate(baseAddr + phy_base_addr) != TRUE)
	{
		error_set(ERR_PHY_NOT_LOCKED);
		LOG_ERROR("PHY PLL not locked");
		return FALSE;
	}

	return TRUE;
}

void phy_poweron_standby(u16 baseAddr)
{
	printk("%s\n", __func__);

	halmaincontroller_phyreset(baseAddr + mc_base_addr, 0);
	halmaincontroller_phyreset(baseAddr + mc_base_addr, 1);

	phy_standby(baseAddr);
}
EXPORT_SYMBOL(phy_poweron_standby);


int phy_standby(u16 baseAddr)
{
#ifndef PHY_THIRD_PARTY
	/* mask phy interrupts */

	printk("%s\n", __func__);
	halsourcephy_interruptmask(baseAddr + phy_base_addr, ~0);
	halsourcephy_enabletmds(baseAddr + phy_base_addr, 0);
	halsourcephy_powerdown(baseAddr + phy_base_addr, 0); /*  disable PHY */
	halsourcephy_gen2txpoweron(baseAddr, 0);
	halsourcephy_gen2pddq(baseAddr + phy_base_addr, 1);
#endif
	return TRUE;
}

int phy_enablehpdsense(u16 baseAddr)
{
#ifndef PHY_THIRD_PARTY
	halsourcephy_gen2enhpdrxsense(baseAddr + phy_base_addr, 1);
#endif
	return TRUE;
}

int phy_disablehpdsense(u16 baseAddr)
{
#ifndef PHY_THIRD_PARTY
	halsourcephy_gen2enhpdrxsense(baseAddr + phy_base_addr, 0);
#endif
	return TRUE;
}

int phy_hotplugdetected(u16 baseAddr)
{
	/* MASK		STATUS		POLARITY	INTERRUPT		HPD
	 *   0			0			0			1			0
	 *   0			1			0			0			1
	 *   0			0			1			0			0
	 *   0			1   		1			1			1
	 *   1			x			x			0			x
	 */
	int polarity = 0;
	polarity = halsourcephy_interruptpolaritystatus(
		baseAddr + phy_base_addr, 0x02) >> 1;
	if (polarity == halsourcephy_hotplugstate(baseAddr + phy_base_addr))
	{
		halsourcephy_interruptpolarity(baseAddr + phy_base_addr, 1, !polarity);
		return polarity;
	}
	return !polarity;
	/* return halsourcephy_hotplugstate(baseAddr + phy_base_addr); */
}

int phy_interruptenable(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	halsourcephy_interruptmask(baseAddr + phy_base_addr, value);
	return TRUE;
}
#ifndef PHY_THIRD_PARTY
int phy_testcontrol(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	halsourcephy_testdatain(baseAddr + phy_base_addr, value);
	halsourcephy_testenable(baseAddr + phy_base_addr, 1);
	halsourcephy_testclock(baseAddr + phy_base_addr, 1);
	halsourcephy_testclock(baseAddr + phy_base_addr, 0);
	halsourcephy_testenable(baseAddr + phy_base_addr, 0);
	return TRUE;
}

int phy_testdata(u16 baseAddr, u8 value)
{
	LOG_TRACE1(value);
	halsourcephy_testdatain(baseAddr + phy_base_addr, value);
	halsourcephy_testenable(baseAddr + phy_base_addr, 0);
	halsourcephy_testclock(baseAddr + phy_base_addr, 1);
	halsourcephy_testclock(baseAddr + phy_base_addr, 0);
	return TRUE;
}

int phy_i2cwrite(u16 baseAddr, u16 data, u8 addr)
{
	LOG_TRACE2(data, addr);
	hali2cmasterphy_registeraddress(baseAddr + phy_i2cm_base_addr, addr);
	hali2cmasterphy_writedata(baseAddr + phy_i2cm_base_addr, data);
	hali2cmasterphy_writerequest(baseAddr + phy_i2cm_base_addr);
	mdelay(1);
	return TRUE;
}
u16 phy_i2cread(u32 baseAddr, u8 addr)
{
	hali2cmasterphy_registeraddress(baseAddr + phy_i2cm_base_addr, addr);
	mdelay(10);
	hali2cmasterphy_readrequest(baseAddr + phy_i2cm_base_addr);
	mdelay(10);
	return hali2cmasterphy_readdata(baseAddr + phy_i2cm_base_addr);
}
#endif
