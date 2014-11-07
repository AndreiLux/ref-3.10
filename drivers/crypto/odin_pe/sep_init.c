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


/* \file
   This file implements the SeP FW initialization sequence.
   This is part of the Discretix CC initalization specifications              */

#define SEP_LOG_CUR_COMPONENT SEP_LOG_MASK_SEP_INIT

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/time.h>
#include "dx_dev_defs.h" /* Must be included before internal headers */
/* Registers definitions from shared/hw/include */
#include "dx_reg_base_host.h"
#include "dx_host.h"
#define DX_CC_HOST_VIRT /* must be defined before including dx_cc_regs.h */
#include "dx_cc_regs.h"
#include "dx_bitops.h"
#include "sep_log.h"
#include "dx_driver.h"
#include "dx_init_cc_abi.h"
#include "dx_init_cc_defs.h"
#include "sep_sram_map.h"
#include "sep_init.h"
#include "sep_request_mgr.h"
#include "sep_power.h"
#ifdef PE_IPSEC_QUEUES
#include "ipsec_qs.h"
#endif

#ifdef DEBUG
#define FW_INIT_TIMEOUT_SEC     10
#define COLD_BOOT_TIMEOUT_SEC	10
#else
#define FW_INIT_TIMEOUT_SEC     3
#define COLD_BOOT_TIMEOUT_SEC	3
#endif
#define FW_INIT_TIMEOUT_MSEC	(FW_INIT_TIMEOUT_SEC * 1000)
#define COLD_BOOT_TIMEOUT_MSEC  (COLD_BOOT_TIMEOUT_SEC * 1000)

#define FW_INIT_PARAMS_BUF_LEN		1024

/*** CC_INIT handlers ***/

/**
 * struct cc_init_ctx - CC init. context
 * @drvdata:		Associated device driver
 * @resident_p:		Pointer to resident image buffer
 * @resident_dma_addr:	DMA address of resident image buffer
 * @resident_size:	Size in bytes of the resident image
 * @cache_p:	Pointer to (i)cache image buffer
 * @cache_dma_addr:	DMA address of the (i)cache image
 * @cache_size:		Size in bytes if the (i)cache image
 * @vrl_p:		Pointer to VRL image buffer
 * @vrl_dma_addr:	DMA address of the VRL
 * @vrl_size:		Size in bytes of the VRL
 * @msg_buf:		A buffer for building the CC-Init message
 */
struct cc_init_ctx {
	struct sep_drvdata *drvdata;
	void *resident_p;
	dma_addr_t resident_dma_addr;
	size_t resident_size;
	void *cache_p;
	dma_addr_t cache_dma_addr;
	size_t cache_size;
	void *vrl_p;
	dma_addr_t vrl_dma_addr;
	size_t vrl_size;
	uint32_t msg_buf[DX_CC_INIT_MSG_LENGTH];
};

static void destroy_cc_init_ctx(struct cc_init_ctx *ctx)
{
	struct device *mydev = ctx->drvdata->dev;

	if (ctx->vrl_p != NULL)
		dma_free_coherent(mydev, ctx->vrl_size,
			ctx->vrl_p, ctx->vrl_dma_addr);
	if (ctx->cache_p != NULL)
		dma_free_coherent(mydev, ctx->cache_size,
			ctx->cache_p, ctx->cache_dma_addr);
	if (ctx->resident_p != NULL)
		dma_free_coherent(mydev, ctx->resident_size,
			ctx->resident_p, ctx->resident_dma_addr);
	kfree(ctx);
}

/**
 * fetch_image() - Fetch CC image using request_firmware mechanism and
 *	locate it in a DMA coherent buffer.
 *
 * @mydev:
 * @image_name:		Image file name (from /lib/firmware/)
 * @image_pp:		Allocated image buffer
 * @image_dma_addr_p:	Allocated image DMA addr
 * @image_size_p:	Loaded image size
 */
static int fetch_image(struct device *mydev, const char *image_name,
	void **image_pp, dma_addr_t *image_dma_addr_p, size_t *image_size_p)
{
	const struct firmware *image;
	int rc;

	rc = request_firmware(&image, image_name, mydev);
	if (unlikely(rc != 0)) {
		SEP_LOG_ERR("Failed loading image %s (%d)\n", image_name, rc);
		return -ENODEV;
	}
	*image_pp = dma_alloc_coherent(mydev,
		image->size, image_dma_addr_p, GFP_KERNEL | GFP_DMA32);
	if (unlikely(*image_pp == NULL)) {
		SEP_LOG_ERR("Failed allocating DMA mem. for resident image\n");
		rc = -ENOMEM;
	} else {
		memcpy(*image_pp, image->data, image->size);
		*image_size_p = image->size;
	}
	/* Image copied into the DMA coherent buffer. No need for "firmware" */
	release_firmware(image);
	if (likely(rc == 0))
		SEP_LOG_DEBUG("%s: %d Bytes\n", image_name, *image_size_p);
	return rc;
}

#ifdef CACHE_IMAGE_NAME
static enum dx_cc_init_msg_icache_size icache_size_to_enum(
	uint8_t icache_size_log2)
{
	int i;
	const int icache_sizes_enum2log[] = DX_CC_ICACHE_SIZE_ENUM2LOG;

	for (i = 0; i < sizeof(icache_sizes_enum2log); i++)
		if ((icache_size_log2 == icache_sizes_enum2log[i]) &&
		    (icache_sizes_enum2log[i] >= 0))
			return (enum dx_cc_init_msg_icache_size)i;
	SEP_LOG_ERR("Requested Icache size (%uKB) is invalid\n",
			1 << (icache_size_log2 - 10));
	return DX_CC_INIT_MSG_ICACHE_SCR_INVALID_SIZE;
}
#endif

/**
 * get_cc_init_checksum() - Calculate CC_INIT message checksum
 *
 * @msg_p:	Pointer to the message buffer
 * @length:	Size of message in _bytes_
 */
static uint32_t get_cc_init_checksum(uint32_t *msg_p)
{
	int bytes_remain;
	uint32_t sum = 0;
	uint16_t *tdata = (uint16_t *)msg_p;

	for (bytes_remain = DX_CC_INIT_MSG_LENGTH * sizeof(uint32_t);
	      bytes_remain > 1; bytes_remain -= 2)
		sum += *tdata++;
	/*  Add left-over byte, if any */
	if (bytes_remain > 0)
		sum += *(uint8_t *) tdata;
	/*  Fold 32-bit sum to 16 bits */
	while ((sum >> 16) != 0)
		sum = (sum & 0xFFFF) + (sum >> 16);
	return ~sum & 0xFFFF;
}

static void build_cc_init_msg(struct cc_init_ctx *init_ctx)
{
	uint32_t * const msg_p = init_ctx->msg_buf;
	struct sep_drvdata *drvdata = init_ctx->drvdata;
#ifndef VRL_KEY_INDEX
	/* Verify VRL key against this truncated hash value */
	uint32_t const vrl_key_hash[] = VRL_KEY_HASH;
#endif
#ifdef CACHE_IMAGE_NAME
	enum dx_cc_init_msg_icache_size icache_size_code;
#endif

	memset(msg_p, 0, DX_CC_INIT_MSG_LENGTH * sizeof(uint32_t));
	msg_p[DX_CC_INIT_MSG_TOKEN_OFFSET] = DX_CC_INIT_HEAD_MSG_TOKEN;
	msg_p[DX_CC_INIT_MSG_LENGTH_OFFSET] = DX_CC_INIT_MSG_LENGTH;
	msg_p[DX_CC_INIT_MSG_OP_CODE_OFFSET] = DX_HOST_REQ_CC_INIT;
	msg_p[DX_CC_INIT_MSG_FLAGS_OFFSET] =
		DX_CC_INIT_FLAGS_RESIDENT_ADDR_FLAG;
	msg_p[DX_CC_INIT_MSG_RESIDENT_IMAGE_OFFSET] =
		init_ctx->resident_dma_addr;

#ifdef CACHE_IMAGE_NAME
	icache_size_code = icache_size_to_enum(drvdata->icache_size_log2);
	msg_p[DX_CC_INIT_MSG_FLAGS_OFFSET] |=
		DX_CC_INIT_FLAGS_I_CACHE_ADDR_FLAG |
		DX_CC_INIT_FLAGS_D_CACHE_EXIST_FLAG |
		DX_CC_INIT_FLAGS_CACHE_ENC_FLAG |
		DX_CC_INIT_FLAGS_CACHE_COPY_FLAG;
#ifdef DX_CC_INIT_FLAGS_CACHE_SCRAMBLE_FLAG
	/* Enable scrambling if available */
	msg_p[DX_CC_INIT_MSG_FLAGS_OFFSET] |=
		DX_CC_INIT_FLAGS_CACHE_SCRAMBLE_FLAG;
#endif
	msg_p[DX_CC_INIT_MSG_I_CACHE_IMAGE_OFFSET] = init_ctx->cache_dma_addr;
	msg_p[DX_CC_INIT_MSG_I_CACHE_DEST_OFFSET] =
		page_to_phys(drvdata->icache_pages);
	msg_p[DX_CC_INIT_MSG_I_CACHE_SIZE_OFFSET] = icache_size_code;
	msg_p[DX_CC_INIT_MSG_D_CACHE_ADDR_OFFSET] =
		page_to_phys(drvdata->dcache_pages);
	msg_p[DX_CC_INIT_MSG_D_CACHE_SIZE_OFFSET] =
		1 << drvdata->dcache_size_log2;
#elif defined(SEP_BACKUP_BUF_SIZE)
	/* Declare SEP backup buffer resources */
	msg_p[DX_CC_INIT_MSG_HOST_BUFF_ADDR_OFFSET] =
		__pa(drvdata->sep_backup_buf);
	msg_p[DX_CC_INIT_MSG_HOST_BUFF_SIZE_OFFSET] =
		drvdata->sep_backup_buf_size;
#endif /*CACHE_IMAGE_NAME*/

	msg_p[DX_CC_INIT_MSG_VRL_ADDR_OFFSET] = init_ctx->vrl_dma_addr;
	/* Handle VRL key hash */
#ifdef VRL_KEY_INDEX
	msg_p[DX_CC_INIT_MSG_KEY_INDEX_OFFSET] = VRL_KEY_INDEX;
#else /* Key should be validated against VRL_KEY_HASH */
	msg_p[DX_CC_INIT_MSG_KEY_INDEX_OFFSET] =
		DX_CC_INIT_MSG_VRL_KEY_INDEX_INVALID;
	msg_p[DX_CC_INIT_MSG_KEY_HASH_0_OFFSET] = vrl_key_hash[0];
	msg_p[DX_CC_INIT_MSG_KEY_HASH_1_OFFSET] = vrl_key_hash[1];
	msg_p[DX_CC_INIT_MSG_KEY_HASH_2_OFFSET] = vrl_key_hash[2];
	msg_p[DX_CC_INIT_MSG_KEY_HASH_3_OFFSET] = vrl_key_hash[3];
#endif

	msg_p[DX_CC_INIT_MSG_CHECK_SUM_OFFSET] = get_cc_init_checksum(msg_p);

	dump_word_array("CC_INIT", msg_p, DX_CC_INIT_MSG_LENGTH);
}

/**
 * create_cc_init_ctx() - Create CC-INIT message and allocate associated
 *	resources (load FW images, etc.)
 *
 * @drvdata:		Device context
 *
 * Returns the allocated message context or NULL on failure.
 */
struct cc_init_ctx *create_cc_init_ctx(struct sep_drvdata *drvdata)
{
	struct cc_init_ctx *init_ctx;
	struct device * const mydev = drvdata->dev;
	int rc;

	init_ctx = kzalloc(sizeof(struct cc_init_ctx), GFP_KERNEL);
	if (unlikely(init_ctx == NULL)) {
		SEP_LOG_ERR("Failed allocating CC-Init. context\n");
		rc = -ENOMEM;
		goto create_err;
	}
	init_ctx->drvdata = drvdata;
	rc = fetch_image(mydev, RESIDENT_IMAGE_NAME, &init_ctx->resident_p,
		&init_ctx->resident_dma_addr, &init_ctx->resident_size);
	if (unlikely(rc != 0))
		goto create_err;
#ifdef CACHE_IMAGE_NAME
	rc = fetch_image(mydev, CACHE_IMAGE_NAME, &init_ctx->cache_p,
		&init_ctx->cache_dma_addr, &init_ctx->cache_size);
	if (unlikely(rc != 0))
		goto create_err;
#endif /*CACHE_IMAGE_NAME*/
	rc = fetch_image(mydev, VRL_IMAGE_NAME, &init_ctx->vrl_p,
		&init_ctx->vrl_dma_addr, &init_ctx->vrl_size);
	if (unlikely(rc != 0))
		goto create_err;
	build_cc_init_msg(init_ctx);
	return init_ctx;

create_err:

	if (init_ctx != NULL)
		destroy_cc_init_ctx(init_ctx);
	return NULL;
}

/**
 * sepinit_wait_for_cold_boot_finish() - Wait for SeP to reach cold-boot-finish
 *					state (i.e., ready for driver-init)
 * @drvdata:
 *
 * Returns int 0 for success, !0 on timeout while waiting for cold-boot-finish
 */
static int sepinit_wait_for_cold_boot_finish(struct sep_drvdata *drvdata)
{
	enum dx_sep_state cur_state;
	uint32_t cur_status;
	int rc = 0;

	cur_state = dx_sep_wait_for_state(
		DX_SEP_STATE_DONE_COLD_BOOT | DX_SEP_STATE_FATAL_ERROR,
		COLD_BOOT_TIMEOUT_MSEC);
	if (cur_state != DX_SEP_STATE_DONE_COLD_BOOT) {
		rc = -EIO;
		cur_status =
			READ_REGISTER(drvdata->cc_base + SEP_STATUS_GPR_OFFSET);
		SEP_LOG_ERR("Failed waiting for DONE_COLD_BOOT from SeP "
			    "(state=0x%08X status=0x%08X)\n",
			cur_state, cur_status);
	}

	return rc;
}

/**
 * dispatch_cc_init_msg() - Push given CC_INIT message into SRAM and signal
 *	SeP to start cold boot sequence
 *
 * @drvdata:
 * @init_cc_msg_p:	A pointer to the message context
 */
static int dispatch_cc_init_msg(struct sep_drvdata *drvdata,
	struct cc_init_ctx *cc_init_ctx_p)
{
	int i;
	uint32_t is_data_ready;
	/* get the base address of the SRAM and add the offset for
	   the CC_Init message */
	const uint32_t msg_target_addr =
		READ_REGISTER(DX_CC_REG_ADDR(drvdata->cc_base,
			HOST, HOST_SEP_SRAM_THRESHOLD)) +
		DX_CC_INIT_MSG_OFFSET_IN_SRAM;

	/* Initialize SRAM access address register for message location */
	WRITE_REGISTER(DX_CC_REG_ADDR(drvdata->cc_base, SRAM, SRAM_ADDR),
		msg_target_addr);
	/* Write the message word by word to the SEP intenral offset */
	for (i = 0 ; i < sizeof(cc_init_ctx_p->msg_buf) ; i++) {
		/* write data to SRAM */
		WRITE_REGISTER(DX_CC_REG_ADDR(drvdata->cc_base,
			SRAM, SRAM_DATA), cc_init_ctx_p->msg_buf[i]);
		/* wait for write complete */
		do {
			is_data_ready = 1 &
				READ_REGISTER(DX_CC_REG_ADDR(drvdata->cc_base,
					SRAM, SRAM_DATA_READY));
		} while (!is_data_ready);
		/* TODO: Timeout in case something gets broken */
	}
	/* Signal SeP: Request CC_INIT */
	WRITE_REGISTER(drvdata->cc_base +
		HOST_SEP_GPR_REG_OFFSET(DX_HOST_REQ_GPR_IDX),
		DX_HOST_REQ_CC_INIT);
	return 0;
}

/**
 * sepinit_do_cc_init() - Initiate SeP cold boot sequence and wait for
 *	its completion.
 *
 * @drvdata:
 *
 * This function loads the CC firmware and dispatches a CC_INIT request message
 * Returns int 0 for success
 */
int sepinit_do_cc_init(struct sep_drvdata *drvdata)
{
	uint32_t cur_state;
	struct cc_init_ctx *cc_init_ctx_p;
	int rc;

	cur_state = dx_sep_wait_for_state(DX_SEP_STATE_START_SECURE_BOOT,
		COLD_BOOT_TIMEOUT_MSEC);
	if (cur_state != DX_SEP_STATE_START_SECURE_BOOT) {
		SEP_LOG_ERR("Bad SeP state = 0x%08X\n", cur_state);
		return -EIO;
	}
#ifdef __BIG_ENDIAN
	/* Enable byte swapping in DMA operations */
	WRITE_REGISTER(DX_CC_REG_ADDR(drvdata->cc_base, HOST, HOST_HOST_ENDIAN),
		0xCCUL);
	/* TODO: Define value in device specific header files? */
#endif
	cc_init_ctx_p = create_cc_init_ctx(drvdata);
	if (likely(cc_init_ctx_p != NULL))
		rc = dispatch_cc_init_msg(drvdata, cc_init_ctx_p);
	else
		rc = -ENOMEM;
	if (likely(rc == 0))
		rc = sepinit_wait_for_cold_boot_finish(drvdata);
	if (cc_init_ctx_p != NULL)
		destroy_cc_init_ctx(cc_init_ctx_p);
	return rc;
}


/*** FW_INIT handlers ***/

#ifdef DEBUG
#define ENUM_CASE_RETURN_STR(enum_name) case enum_name: return #enum_name

static const char *param2str(enum dx_fw_init_tlv_params param_type)
{
	switch (param_type) {
	ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_NULL);
	ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_FIRST);
	ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_LAST);
	ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_DISABLE_MODULES);
	ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_HOST_AXI_CONFIG);
	ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_HOST_DEF_APPLET_CONFIG);
	ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_NUM_OF_DESC_QS);
	ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_DESC_QS_ADDR);
	ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_DESC_QS_SIZE);
	ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_CTX_CACHE_PART);
	ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_SEP_REQUEST_PARAMS);
	ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_IPSEC_QS_BASE);
	ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_SEP_FREQ);
	ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_RANDOM_SEED);
	ENUM_CASE_RETURN_STR(DX_FW_INIT_PARAM_HOST_UPTIME);
	default: return "(unknown param.)";
	}
}

static void dump_fwinit_params(struct sep_drvdata *drvdata,
	uint32_t *fw_init_params_buf_p)
{
#define LINE_BUF_LEN 90 /* increased to hold values for 2 queues */
	const uint32_t last_tl_word = DX_TL_WORD(DX_FW_INIT_PARAM_LAST, 1);
	uint32_t tl_word;
	uint16_t type, len;
	uint32_t *cur_buf_p;
	unsigned int i = 0;
	char line_buf[LINE_BUF_LEN];
	unsigned int line_offset;

	printk(KERN_DEBUG "Dx SeP fw_init params dump:\n");
	cur_buf_p = fw_init_params_buf_p;
	do {
		tl_word = le32_to_cpu(*cur_buf_p);
		cur_buf_p++;
		type = DX_TL_GET_TYPE(tl_word);
		len = DX_TL_GET_LENGTH(tl_word);

		if ((cur_buf_p + len - fw_init_params_buf_p) >
		    (FW_INIT_PARAMS_BUF_LEN / sizeof(uint32_t))) {
			SEP_LOG_ERR(
				"LAST parameter not found up to buffer end\n");
			break;
		}

		line_offset = snprintf(line_buf, LINE_BUF_LEN,
			"Type=0x%04X (%s), Length=%u , Val={",
			type, param2str(type), len);
		for (i = 0; i < len ; i++) {
			/* 11 is length of printed value
			   (formatted with 0x%08X in the next call to snprintf*/
			if (line_offset + 11 >= LINE_BUF_LEN) {
				printk(KERN_DEBUG "%s\n", line_buf);
				line_offset = 0;
			}
			line_offset += snprintf(line_buf + line_offset,
				LINE_BUF_LEN - line_offset,
				"0x%08X,", le32_to_cpu(*cur_buf_p));
			cur_buf_p++;
		}
		printk(KERN_DEBUG "%s}\n", line_buf);
	} while (tl_word != last_tl_word);
}
#else
#define dump_fwinit_params(drvdata, fw_init_params_buf_p) do {} while (0)
#endif /*DEBUG*/

/**
 * add_fwinit_param() - Add TLV parameter for FW-init.
 * @tlv_buf:	 The base of the TLV parameters buffers
 * @idx_p:	 (in/out): Current tlv_buf word index
 * @checksum_p:	 (in/out): 32bit checksum for TLV array
 * @type:	 Parameter type
 * @length:	 Parameter length (size in words of "values")
 * @values:	 Values array ("length" values)
 *
 * Returns void
 */
static void add_fwinit_param(
	uint32_t *tlv_buf, uint32_t *idx_p, uint32_t *checksum_p,
	enum dx_fw_init_tlv_params type, uint16_t length,
	const uint32_t *values)
{
	const uint32_t tl_word = DX_TL_WORD(type, length);
	int i;

#ifdef DEBUG
	/* Verify that we have enough space for LAST param. after this param. */
	if ((*idx_p + 1 + length + 2) > (FW_INIT_PARAMS_BUF_LEN/4)) {
		SEP_LOG_ERR("tlv_buf size limit reached!\n");
		SEP_DRIVER_BUG();
	}
#endif

	/* Add type-length word */
	tlv_buf[(*idx_p)++] = cpu_to_le32(tl_word);
	*checksum_p += tl_word;

	/* Add values */
	for (i = 0; i < length; i++) {
		/* Add value words if any. TL-word is counted as first... */
		tlv_buf[(*idx_p)++] = cpu_to_le32(values[i]);
		*checksum_p += values[i];
	}
}

/**
 * init_fwinit_param_list() - Initialize TLV parameters list
 * @tlv_buf:	The pointer to the TLV list array buffer
 * @idx_p:	The pointer to the variable that would maintain current
 *		position in the tlv_array
 * @checksum_p:	The pointer to the variable that would accumulate the
 *		TLV array checksum
 *
 * Returns void
 */
static void init_fwinit_param_list(
	uint32_t *tlv_buf, uint32_t *idx_p, uint32_t *checksum_p)
{
	const uint32_t magic = DX_FW_INIT_PARAM_FIRST_MAGIC;
	/* Initialize index and checksum variables */
	*idx_p = 0;
	*checksum_p = 0;
	/* Start with FIRST_MAGIC parameter */
	add_fwinit_param(tlv_buf, idx_p, checksum_p,
		DX_FW_INIT_PARAM_FIRST, 1, &magic);
}

/**
 * terminate_fwinit_param_list() - Terminate the TLV parameters list with
 *				LAST/checksum parameter
 * @tlv_buf:	The pointer to the TLV list array buffer
 * @idx_p:	The pointer to the variable that would maintain current
 *		position in the tlv_array
 * @checksum_p:	The pointer to the variable that would accumulate the
 *		TLV array checksum
 *
 * Returns void
 */
static void terminate_fwinit_param_list(
	uint32_t *tlv_buf, uint32_t *idx_p, uint32_t *checksum_p)
{
	const uint32_t tl_word = DX_TL_WORD(DX_FW_INIT_PARAM_LAST, 1);

	tlv_buf[(*idx_p)++] = cpu_to_le32(tl_word);
	*checksum_p += tl_word; /* Last TL word is included in checksum */
	tlv_buf[(*idx_p)++] = cpu_to_le32(~(*checksum_p));
}

static int create_fwinit_command(struct sep_drvdata *drvdata,
	uint32_t **fw_init_params_buf_pp, dma_addr_t *fw_init_params_dma_p,
	bool is_cold_boot)
{
	uint32_t idx;
	uint32_t checksum = 0;
#ifdef SEP_FREQ_MHZ
	uint32_t sep_freq = SEP_FREQ_MHZ;
#endif
	dma_addr_t q_base_dma;
	unsigned long q_size;
	unsigned long long time_now_ms;
	struct timespec time_now;
	/* arrays for queue parameters values */
	uint32_t qs_base_dma[SEP_MAX_NUM_OF_DESC_Q];
	uint32_t qs_size[SEP_MAX_NUM_OF_DESC_Q];
	uint32_t qs_ctx_size[SEP_MAX_NUM_OF_DESC_Q];
	uint32_t qs_ctx_size_total;
	uint32_t qs_num = drvdata->num_of_desc_queues;
#ifdef PE_IPSEC_QUEUES
	uint32_t ipsec_qs_base;
#endif
	uint32_t sep_request_params[DX_SEP_REQUEST_PARAM_MSG_LEN];
	int i;
	int rc;

	/* allocate coherent working buffer */
	*fw_init_params_buf_pp = dma_alloc_coherent(drvdata->dev,
							FW_INIT_PARAMS_BUF_LEN,
							fw_init_params_dma_p,
							GFP_KERNEL);
	if (*fw_init_params_buf_pp == NULL) {
		SEP_LOG_ERR("Unable to allocate coherent workspace buffer\n");
		return -ENOMEM;
	}
	SEP_LOG_DEBUG("fw_init_params_dma=0x%08lX fw_init_params_va=0x%p\n",
		(unsigned long)*fw_init_params_dma_p, *fw_init_params_buf_pp);

	init_fwinit_param_list(*fw_init_params_buf_pp, &idx, &checksum);

	/* Get the kernel current time and convrt to millisecs */
	time_now = current_kernel_time();
	time_now_ms = (time_now.tv_sec * 1000) + (time_now.tv_nsec / 1000000);
#ifdef __BIG_ENDIAN
	/* Swap words to match SEP endianess for unsigned long long */
	/* Per word endianess is taken care of by add_fwinit_param() */
	time_now_ms = (time_now_ms >> 32) | ((time_now_ms & 0xFFFFFFFF) << 32);
#endif

	add_fwinit_param(*fw_init_params_buf_pp, &idx, &checksum,
			DX_FW_INIT_PARAM_HOST_UPTIME,
			sizeof(time_now_ms) / sizeof(uint32_t),
			(uint32_t *)&time_now_ms);

	/* No other params for warm-boot */
	if (!is_cold_boot)
		goto terminate_params_list;

#ifdef SEP_FREQ_MHZ
	add_fwinit_param(*fw_init_params_buf_pp, &idx, &checksum,
		  DX_FW_INIT_PARAM_SEP_FREQ, 1, &sep_freq);
#endif

	/* No need to validate number of queues - already validated in
	   sep_setup() */

	add_fwinit_param(*fw_init_params_buf_pp, &idx, &checksum,
		  DX_FW_INIT_PARAM_NUM_OF_DESC_QS, 1,
		  &qs_num);

	/* Fetch per-queue information */
	qs_ctx_size_total = 0;
	for (i = 0; i < qs_num; i++) {
		desc_q_get_info4sep(drvdata->queue[i].desc_queue,
				    &q_base_dma, &q_size);
		/* Data is first fetched into q_base_dma and q_size because
		   return value is of different type than uint32_t */
		qs_base_dma[i] = q_base_dma;
		qs_size[i] = q_size;
		qs_ctx_size[i] =
			ctxmgr_sep_cache_get_size(drvdata->queue[i].sep_cache);
		if ((qs_base_dma[i] == 0) || (qs_size[i] == 0) ||
		    (qs_ctx_size[i] == 0)) {
			SEP_LOG_ERR("Invalid queue %d resources: "
				    "base=0x%08X size=%u ctx_cache_size=%u\n",
				i, qs_base_dma[i], qs_size[i], qs_ctx_size[i]);
			rc = -EINVAL;
			goto fwinit_error;
		}
		qs_ctx_size_total += qs_ctx_size[i];
	}

	if (qs_ctx_size_total > drvdata->num_of_sep_cache_entries) {
		SEP_LOG_ERR("Too many context cache entries allocated "
			    "(%u>%u)\n",
			qs_ctx_size_total, drvdata->num_of_sep_cache_entries);
		rc = -EINVAL;
		goto fwinit_error;
	}

	/* SW descriptors queue params */
	add_fwinit_param(*fw_init_params_buf_pp, &idx, &checksum,
		DX_FW_INIT_PARAM_DESC_QS_ADDR, qs_num, qs_base_dma);
	add_fwinit_param(*fw_init_params_buf_pp, &idx, &checksum,
		DX_FW_INIT_PARAM_DESC_QS_SIZE, qs_num, qs_size);
	add_fwinit_param(*fw_init_params_buf_pp, &idx, &checksum,
		DX_FW_INIT_PARAM_CTX_CACHE_PART, qs_num, qs_ctx_size);

	/* Prepare sep request params */
	dx_sep_req_get_sep_init_params(sep_request_params);
	add_fwinit_param(*fw_init_params_buf_pp, &idx, &checksum,
		DX_FW_INIT_PARAM_SEP_REQUEST_PARAMS,
		DX_SEP_REQUEST_PARAM_MSG_LEN,
		sep_request_params);

#ifdef PE_IPSEC_QUEUES
	/* IPsec queues params */
	if (drvdata->ipsec_q_size_log2 > 0) {
		ipsec_qs_base = ipsec_qs_get_base(drvdata->ipsec_qs);
		add_fwinit_param(*fw_init_params_buf_pp, &idx, &checksum,
			DX_FW_INIT_PARAM_IPSEC_QS_BASE, 1, &ipsec_qs_base);
	}

	if (drvdata->rnd_seed_size > 0) {
		add_fwinit_param(*fw_init_params_buf_pp, &idx, &checksum,
			DX_FW_INIT_PARAM_RANDOM_SEED, drvdata->rnd_seed_size,
			drvdata->rnd_seed);
	}
#endif

terminate_params_list:
	terminate_fwinit_param_list(*fw_init_params_buf_pp, &idx, &checksum);

	return 0;

fwinit_error:
	dma_free_coherent(drvdata->dev, FW_INIT_PARAMS_BUF_LEN,
			  *fw_init_params_buf_pp, *fw_init_params_dma_p);
	return rc;
}

static void destroy_fwinit_command(struct sep_drvdata *drvdata,
	uint32_t *fw_init_params_buf_p, dma_addr_t fw_init_params_dma)
{
	/* release TLV parameters buffer */
	dma_free_coherent(drvdata->dev, FW_INIT_PARAMS_BUF_LEN,
			  fw_init_params_buf_p, fw_init_params_dma);
}

/**
 * sepinit_get_fw_props() - Get the FW properties (version, cache size, etc.)
 *				as given in the
 * @drvdata:	 Context where to fill retrieved data
 *
 * Get the FW properties (version, cache size, etc.) as given in the
 * respective GPRs.
 * This function should be called only after sepinit_wait_for_cold_boot_finish
 */
void sepinit_get_fw_props(struct sep_drvdata *drvdata)
{

	uint32_t init_fw_props;

	/* SeP ROM version */
	drvdata->rom_ver = READ_REGISTER(drvdata->cc_base +
		SEP_HOST_GPR_REG_OFFSET(DX_SEP_INIT_ROM_VER_GPR_IDX));
	/* SeP Resident (SRAM) image */
	drvdata->fw_ver = READ_REGISTER(drvdata->cc_base +
		SEP_HOST_GPR_REG_OFFSET(DX_SEP_INIT_FW_VER_GPR_IDX));

	init_fw_props = READ_REGISTER(drvdata->cc_base +
		SEP_HOST_GPR_REG_OFFSET(DX_SEP_INIT_FW_PROPS_GPR_IDX));

	drvdata->num_of_desc_queues =
		BITFIELD_GET(init_fw_props,
			DX_SEP_INIT_NUM_OF_QUEUES_BIT_OFFSET,
			DX_SEP_INIT_NUM_OF_QUEUES_BIT_SIZE);
	drvdata->num_of_sep_cache_entries =
		BITFIELD_GET(init_fw_props,
			DX_SEP_INIT_CACHE_CTX_SIZE_BIT_OFFSET,
			DX_SEP_INIT_CACHE_CTX_SIZE_BIT_SIZE);
	drvdata->mlli_table_size =
		BITFIELD_GET(init_fw_props,
			DX_SEP_INIT_MLLI_TBL_SIZE_BIT_OFFSET,
			DX_SEP_INIT_MLLI_TBL_SIZE_BIT_SIZE);

	SEP_LOG_INFO("ROM Ver.=0x%08X , FW Ver.=0x%08X\n"
		"SEP queues=%u, Ctx.Cache#ent.=%u , MLLIsize=%lu B ",
		drvdata->rom_ver, drvdata->fw_ver,
		drvdata->num_of_desc_queues,
		drvdata->num_of_sep_cache_entries,
		drvdata->mlli_table_size);

#ifdef PE_IPSEC_QUEUES
	drvdata->ipsec_q_size_log2 =
		BITFIELD_GET(init_fw_props,
			DX_SEP_INIT_IPSEC_Q_SIZE_BIT_OFFSET,
			DX_SEP_INIT_IPSEC_Q_SIZE_BIT_SIZE);

	SEP_LOG_INFO("ipsec_q_size_log2=%u\n", drvdata->ipsec_q_size_log2);

	drvdata->rnd_seed_size =
		BITFIELD_GET(init_fw_props,
			DX_SEP_INIT_IPSEC_RND_SEED_BIT_OFFSET,
			DX_SEP_INIT_IPSEC_RND_SEED_SBIT_SIZE);

	SEP_LOG_INFO("rnd_seed_size =%u\n", drvdata->rnd_seed_size);
#endif

}

/**
 * sepinit_wait_for_fw_init_done() - Wait for FW initialization to complete
 * @drvdata:
 *
 * Wait for FW initialization to complete
 * This function should be invoked after sepinit_set_fw_init_params
 * Returns int
 */
static int sepinit_wait_for_fw_init_done(struct sep_drvdata *drvdata)
{
	enum dx_sep_state cur_state;
	uint32_t cur_status;
	int rc = 0;

	cur_state = dx_sep_wait_for_state(
		DX_SEP_STATE_DONE_FW_INIT | DX_SEP_STATE_FATAL_ERROR,
		FW_INIT_TIMEOUT_MSEC);
	if (cur_state != DX_SEP_STATE_DONE_FW_INIT) {
		rc = -EIO;
		cur_status =
			READ_REGISTER(drvdata->cc_base + SEP_STATUS_GPR_OFFSET);
		SEP_LOG_INFO("Failed waiting for DONE_FW_INIT from SeP "
			    "(state=0x%08X status=0x%08X)\n",
			cur_state, cur_status);
	} else
		SEP_LOG_INFO("DONE_FW_INIT\n");

	return rc;
}

/**
 * sepinit_do_fw_init() - Initialize SeP FW
 * @drvdata:
 * @is_cold_boot:	If "true" process cold-boot sequence otherwise
 *			process warm-boot.
 * Provide SeP FW with initialization parameters and wait for DONE_FW_INIT.
 *
 * Returns int 0 on success
 */
int sepinit_do_fw_init(struct sep_drvdata *drvdata, bool is_cold_boot)
{
	int rc;
	uint32_t *fw_init_params_buf_p;
	dma_addr_t fw_init_params_dma;

	rc = create_fwinit_command(drvdata,
		&fw_init_params_buf_p, &fw_init_params_dma,
		is_cold_boot);
	if (rc != 0)
		return rc;
	dump_fwinit_params(drvdata, fw_init_params_buf_p);
	/* Write the physical address of the FW init parameters buffer */
	WRITE_REGISTER(drvdata->cc_base +
		HOST_SEP_GPR_REG_OFFSET(DX_HOST_REQ_PARAM_GPR_IDX),
		fw_init_params_dma);
	/* Initiate FW-init */
	WRITE_REGISTER(drvdata->cc_base +
		HOST_SEP_GPR_REG_OFFSET(DX_HOST_REQ_GPR_IDX),
		DX_HOST_REQ_FW_INIT);
	rc = sepinit_wait_for_fw_init_done(drvdata);
	printk(KERN_INFO "[sepinit_do_fw_init] rc after sepinit_wait_for_fw_init_done = %d\n", rc);
	destroy_fwinit_command(drvdata,
		fw_init_params_buf_p, fw_init_params_dma);
	return rc;
}

