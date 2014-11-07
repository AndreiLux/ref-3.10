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

#ifndef __DX_INIT_CC_DEFS__H__
#define __DX_INIT_CC_DEFS__H__

/** @file dx_init_cc_defs.h
*  \brief definitions for the CC54 initialization API
*
*  \version
*  \author avis
*/

/* message token to sep */
/* CC_INIT definitions */
#define DX_CC_INIT_HEAD_MSG_TOKEN		0x673CFF77UL


/*The below enumerator includes the offsets inside the CC_Init message*/
/*The last enum is the length of the message */
enum dx_cc_init_msg_offset {
	DX_CC_INIT_MSG_TOKEN_OFFSET,
	DX_CC_INIT_MSG_LENGTH_OFFSET,
	DX_CC_INIT_MSG_OP_CODE_OFFSET,
	DX_CC_INIT_MSG_FLAGS_OFFSET,
	DX_CC_INIT_MSG_RESIDENT_IMAGE_OFFSET,
	DX_CC_INIT_MSG_HOST_BUFF_ADDR_OFFSET,
	DX_CC_INIT_MSG_HOST_BUFF_SIZE_OFFSET,
	DX_CC_INIT_MSG_CC_INIT_EXT_ADDR_OFFSET,
	DX_CC_INIT_MSG_VRL_ADDR_OFFSET,
	DX_CC_INIT_MSG_MAGIC_NUM_OFFSET,
	DX_CC_INIT_MSG_KEY_INDEX_OFFSET,
	DX_CC_INIT_MSG_KEY_HASH_0_OFFSET,
	DX_CC_INIT_MSG_KEY_HASH_1_OFFSET,
	DX_CC_INIT_MSG_KEY_HASH_2_OFFSET,
	DX_CC_INIT_MSG_KEY_HASH_3_OFFSET,
	DX_CC_INIT_MSG_CHECK_SUM_OFFSET,
	DX_CC_INIT_MSG_LENGTH
};

/* Set this value if key used in the VRL is to be verified against KEY_HASH
   fields in the CC_INIT message */
#define DX_CC_INIT_MSG_VRL_KEY_INDEX_INVALID 0xFFFFFFFF

/* Bit flags for the CC_Init flags word*/
/* The CC_Init resident address address is valid (it might be passed via VRL)*/
#define DX_CC_INIT_FLAGS_RESIDENT_ADDR_FLAG		0x00000001
/* The CC_Init extension address is valid and should be used */
#define DX_CC_INIT_FLAGS_INIT_EXT_FLAG			0x00000008
/*  use the magic number from the CC_Init to verify the VRL */
#define DX_CC_INIT_FLAGS_MAGIC_NUMBER_FLAG		0x00000080


/*-------------------------------
  STRUCTURES
---------------------------------*/
struct dx_cc_def_applet_msg {
	uint32_t	cc_flags;
	uint32_t	icache_image_addr;
	uint32_t	vrl_addr;
	uint32_t	magic_num;
	uint32_t	ver_key_index;
	uint32_t	hashed_key_val[4];
};

/**
 * struct dx_cc_init_msg - used for passing the parameters to the CC_Init API.
 * The structure is converted in to the CC_Init message
 * @cc_flags:		Bits flags for different fields in the message
 * @res_image_addr:	resident image address in the HOST memory
 * @host_buff_addr		-	HOST memory for SRAM backup
 * @host_buff_size		-	HOST memeory size
 * @init_ext_addr:	Address of the cc_Init extension message in the HOST
 * @vrl_addr:		The address of teh VRL in the HOST memory
 * @magic_num:		Requested VRL magic number
 * @ver_key_index:	The index of the verification key
 * @Hashed_key_val:	the trunked hash value of teh verification key in case
 */

struct dx_cc_init_msg {
	uint32_t	cc_flags;
	uint32_t	res_image_addr;
	uint32_t	host_buff_addr;
	uint32_t	host_buff_size;
	uint32_t	init_ext_addr;
	uint32_t	vrl_addr;
	uint32_t	magic_num;
	uint32_t	ver_key_index;
	uint32_t	hashed_key_val[4];
};

#endif /*__DX_INIT_CC_DEFS__H__*/
