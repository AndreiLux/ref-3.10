/*******************************************************************
* (c) Copyright 2011-2012 Discretix Technologies Ltd.              *
* This software is protected by copyright, international           *
* treaties and patents, and distributed under multiple licenses.   *
* Any use of this Software as part of the Discretix CryptoCell or  *
* Packet Engine products requires a commercial license.            *
* Copies of this Software that are distributed with the Discretix  *
* CryptoCell or Packet Engine product drivers, may be used in      *
* accordance with a commercial license, or at the user's option,   *
* used and redistributed under the terms and conditions of the GNU *
* General Public License ("GPL") version 2, as published by the    *
* Free Software Foundation.                                        *
* This program is distributed in the hope that it will be useful,  *
* but WITHOUT ANY LIABILITY AND WARRANTY; without even the implied *
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. *
* See the GNU General Public License version 2 for more details.   *
* You should have received a copy of the GNU General Public        *
* License version 2 along with this Software; if not, please write *
* to the Free Software Foundation, Inc., 59 Temple Place - Suite   *
* 330, Boston, MA 02111-1307, USA.                                 *
* Any copy or reproduction of this Software, as permitted under    *
* the GNU General Public License version 2, must include this      *
* Copyright Notice as well as any other notices provided under     *
* the said license.                                                *
********************************************************************/
/** @file: dx_dev_defs.h
 * Device specific definitions for the PE device driver */


#ifndef _DX_DEV_DEFS_H_
#define _DX_DEV_DEFS_H_

#include <linux/platform_data/odin_tz.h>

#define DRIVER_NAME MODULE_NAME

/* OpenFirmware matches */
#define DX_DEV_OF_MATCH  {                        \
	{.compatible = "dx,pe" },				  \
	{.name = "dx_pe" },                       \
	{}                                        \
}

/* Firmware images file names (for request_firmware) */
#define RESIDENT_IMAGE_NAME "resident.bin"
#define VRL_IMAGE_NAME "Primary_VRL.bin"

/* 128 MSb of SHA256 of the key in the VRL - for verification of loaded VRL */
#define VRL_KEY_HASH {0x1EAF0EE3, 0xD5055DC6, 0xC4DB4AEE, 0x15C92CA7}

/* Default size for warm-boot backup buffer - 160 KB */
#define SEP_BACKUP_BUF_SIZE (160 * 1024)

#define EXPECTED_FW_VER 0x01000000

/* The known SeP clock frequency in MHz (30 MHz on Virtex-5 FPGA) */
#define SEP_FREQ_MHZ 200

/* Number of SEP descriptor queues */
#define SEP_MAX_NUM_OF_DESC_Q  1

/* Denote existence of IPsec queues */
#define PE_IPSEC_QUEUES 1

/* Maximum number of registered memory buffers per client context */
#define MAX_REG_MEMREF_PER_CLIENT_CTX 16

/* Maximum number of SeP Applets session per client context */
#define MAX_SEPAPP_SESSION_PER_CLIENT_CTX 0 /* Not supported for PE */

/* Customer should implement the following pair of macros to enable/disable
   CryptoCell core clock on his platform. */
#define DX_SHUTDOWN_CC_CORE_CLOCK(drvdata) do {} while (0)
/*
#define DX_SHUTDOWN_CC_CORE_CLOCK(drvdata) \
		tz_ses_pm(ODIN_SES_PE, 0x00000300);
*/

#define DX_RESTART_CC_CORE_CLOCK(drvdata) \
		WRITE_REGISTER((drvdata)->cc_base + \
				DX_HOST_CC_SW_RST_REG_OFFSET, 1)
//				DX_CC_REG_OFFSET(ENV, CC_RST_N), 1)

#endif /* _DX_DEV_DEFS_H_ */

