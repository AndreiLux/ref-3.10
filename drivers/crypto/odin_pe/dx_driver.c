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



#define SEP_LOG_CUR_COMPONENT SEP_LOG_MASK_MAIN

#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/random.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/sysctl.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/list.h>
#include <linux/slab.h>
/* cache.h required for L1_CACHE_ALIGN() and cache_line_size() */
#include <linux/cache.h>
#include <asm/byteorder.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <asm/system.h>
#include <asm/cacheflush.h>
#include <linux/pagemap.h>
#include <linux/sched.h>
#include <linux/odin_pd.h>

#if 0 //def CONFIG_PM_RUNTIME 
#include <linux/runtime.pm>
#endif

#ifdef CONFIG_OF
/* For open firmware. */
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <generated/autoconf.h>
#if defined(DEBUG) && defined(CONFIG_KGDB)
/* For setup_break option */
#include <linux/kgdb.h>
#endif

#include "sep_log.h"
#include "sep_init.h"
#include "desc_mgr.h"
#include "lli_mgr.h"
#include "sep_sysfs.h"
#include "dx_driver_abi.h"
#include "crypto_ctx_mgr.h"
#include "crypto_api.h"
#include "sep_request.h"
#include "dx_sep_kapi.h"
#if MAX_SEPAPP_SESSION_PER_CLIENT_CTX > 0
#include "sepapp.h"
#endif
#include "sep_power.h"
#include "sep_request_mgr.h"
#ifdef PE_IPSEC_QUEUES
#include "ipsec_qs.h"
#include "ike_evt_agent.h"
#endif

/* Registers definitions from shared/hw/include */
#include "dx_reg_base_host.h"
#include "dx_host.h"
#ifdef DX_BASE_ENV_REGS
#include "dx_env.h"
#endif
#define DX_CC_HOST_VIRT /* must be defined before including dx_cc_regs.h */
#include "dx_cc_regs.h"
#include "dx_init_cc_abi.h"
#include "dx_driver.h"

#if SEPAPP_UUID_SIZE != DXDI_SEPAPP_UUID_SIZE
#error Size mismatch of SEPAPP_UUID_SIZE and DXDI_SEPAPP_UUID_SIZE
#endif

#define SEP_DEVICES SEP_MAX_NUM_OF_DESC_Q

#ifdef SEP_PRINTF
#define SEP_PRINTF_H2S_GPR_OFFSET \
	HOST_SEP_GPR_REG_OFFSET(DX_SEP_HOST_PRINTF_GPR_IDX)
#define SEP_PRINTF_S2H_GPR_OFFSET \
	SEP_HOST_GPR_REG_OFFSET(DX_SEP_HOST_PRINTF_GPR_IDX)
/* Ack is allocated only upper 24 bits */
#define SEP_PRINTF_ACK_MAX 0xFFFFFF
/* Sync. host-SeP value */
#define SEP_PRINTF_ACK_SYNC_VAL SEP_PRINTF_ACK_MAX
#endif

static struct class *sep_class;

int q_num; /* Initialized to 0 */
module_param(q_num, int, 0444);
MODULE_PARM_DESC(q_num, "Num. of active queues 1-2");

int sep_log_level = SEP_BASE_LOG_LEVEL;
module_param(sep_log_level, int, 0644);
MODULE_PARM_DESC(sep_log_level, "Log level: min ERR = 0, max TRACE = 4");

int sep_log_mask = SEP_LOG_MASK_ALL;
module_param(sep_log_mask, int, 0644);
MODULE_PARM_DESC(sep_log_mask, "Log components mask");

int disable_linux_crypto;
module_param(disable_linux_crypto, int, 0444);
MODULE_PARM_DESC(disable_linux_crypto,
	"Set to !0 to disable registration with Linux CryptoAPI");

/* Parameters to override default sep cache memories reserved pages */
#ifdef ICACHE_SIZE_LOG2_DEFAULT
#include "dx_init_cc_defs.h"
int icache_size_log2 = ICACHE_SIZE_LOG2_DEFAULT;
module_param(icache_size_log2, int, 0444);
MODULE_PARM_DESC(icache_size_log2, "Size of Icache memory in log2(bytes)");

int dcache_size_log2 = DCACHE_SIZE_LOG2_DEFAULT;
module_param(dcache_size_log2, int, 0444);
MODULE_PARM_DESC(icache_size_log2, "Size of Dcache memory in log2(bytes)");
#endif

#ifdef SEP_BACKUP_BUF_SIZE
int sep_backup_buf_size = SEP_BACKUP_BUF_SIZE;
module_param(sep_backup_buf_size, int, 0444);
MODULE_PARM_DESC(sep_backup_buf_size, "Size of backup buffer of SeP context "
				  "(for warm-boot)");
#endif

#ifdef DEBUG
void dump_byte_array(const char *name, const uint8_t *the_array,
		     unsigned long size)
{
	int i , line_offset = 0;
	const uint8_t *cur_byte;
	char line_buf[80];

	line_offset = snprintf(line_buf, sizeof(line_buf), "%s[%lu]: ",
		name, size);

	for (i = 0 , cur_byte = the_array;
	     (i < size) && (line_offset < sizeof(line_buf)); i++, cur_byte++) {
		line_offset += snprintf(line_buf + line_offset,
					sizeof(line_buf) - line_offset,
					"%02X ", *cur_byte);
		if (line_offset > 75) { /* Cut before line end */
			SEP_LOG_DEBUG("%s\n", line_buf);
			line_offset = 0;
		}
	}

	if (line_offset > 0) /* Dump remainding line */
		SEP_LOG_DEBUG("%s\n", line_buf);

}

void dump_word_array(const char *name, const uint32_t *the_array,
		     unsigned long size_in_words)
{
	int i , line_offset = 0;
	const uint32_t *cur_word;
	char line_buf[80];

	line_offset = snprintf(line_buf, sizeof(line_buf), "%s[%lu]: ",
		name, size_in_words);

	for (i = 0 , cur_word = the_array;
	     (i < size_in_words) && (line_offset < sizeof(line_buf));
	     i++, cur_word++) {
		line_offset += snprintf(line_buf + line_offset,
					sizeof(line_buf) - line_offset,
					"%08X ", *cur_word);
		if (line_offset > 70) { /* Cut before line end */
			SEP_LOG_DEBUG("%s\n", line_buf);
			line_offset = 0;
		}
	}

	if (line_offset > 0) /* Dump remainding line */
		SEP_LOG_DEBUG("%s\n", line_buf);
}

#endif /*DEBUG*/


/**** SeP descriptor operations implementation functions *****/
/* (send descriptor, wait for completion, process result)    */

/**
 * send_crypto_op_desc() - Pack crypto op. descriptor and send
 * @op_ctx:
 * @sep_ctx_load_req:	Flag if context loading is required
 * @sep_ctx_init_req:	Flag if context init is required
 * @proc_mode:		Processing mode
 *
 * On failure desc_type is retained so process_desc_completion cleans up
 * resources anyway (error_info denotes failure to send/complete)
 * Returns int 0 on success
 */
static int send_crypto_op_desc(struct sep_op_ctx *op_ctx,
		    int sep_ctx_load_req, int sep_ctx_init_req,
		    enum sep_proc_mode proc_mode)
{
	const struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	struct sep_sw_desc desc;
	int rc;

	desc_q_pack_crypto_op_desc(&desc, op_ctx,
				   sep_ctx_load_req, sep_ctx_init_req,
				   proc_mode);
	/* op_state must be updated before dispatching descriptor */
	rc = desc_q_enqueue(drvdata->desc_queue, &desc, true);
	if (unlikely(IS_DESCQ_ENQUEUE_ERR(rc))) {
		/* Desc. sending failed - "signal" process_desc_completion */
		op_ctx->error_info = DXDI_ERROR_INTERNAL;
	} else
		rc = 0;

	return rc;
}

/**
 * send_combined_op_desc() - Pack combined or/and load operation descriptor(s)
 * @op_ctx:
 * @sep_ctx_load_req: Flag if context loading is required
 * @sep_ctx_init_req: Flag if context init is required
 * @proc_mode: Processing mode
 * @cfg_scheme: The SEP format configuration scheme claimed by the user
 *
 * On failure desc_type is retained so process_desc_completion cleans up
 * resources anyway (error_info denotes failure to send/complete)
 * Returns int 0 on success
 */
static int send_combined_op_desc(
	struct sep_op_ctx *op_ctx,
	int *sep_ctx_load_req, int sep_ctx_init_req,
	enum sep_proc_mode proc_mode,
	uint32_t cfg_scheme)
{
	const struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	struct sep_sw_desc desc;
	int rc;

	/* transaction of two descriptors */
	op_ctx->pending_descs_cntr = 2;

	/* prepare load descriptor of combined associated contexts */
	desc_q_pack_load_op_desc(&desc, op_ctx, sep_ctx_load_req);

	/* op_state must be updated before dispatching descriptor */
	rc = desc_q_enqueue(drvdata->desc_queue, &desc, true);
	if (unlikely(IS_DESCQ_ENQUEUE_ERR(rc))) {
		/* Desc. sending failed - "signal" process_desc_completion */
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
		goto send_combined_op_exit;
	}

	/* prepare crypto descriptor for the combined scheme operation */
	desc_q_pack_combined_op_desc(&desc, op_ctx,
				0 /* contexts already loaded
				in prior descriptor */,
				sep_ctx_init_req,
				proc_mode, cfg_scheme);
	rc = desc_q_enqueue(drvdata->desc_queue, &desc, true);
	if (unlikely(IS_DESCQ_ENQUEUE_ERR(rc))) {
		/*invalidate first descriptor (if still pending) */
		desc_q_mark_invalid_cookie(drvdata->desc_queue,
					(uint32_t)op_ctx);
		/* Desc. sending failed - "signal" process_desc_completion */
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
	} else
		rc = 0;

send_combined_op_exit:
	return rc;
}

/**
 * register_client_memref() - Register given client memory buffer reference
 * @client_ctx:		User context data
 * @user_buf_ptr:	Buffer address in user space. NULL if sgl!=NULL.
 * @sgl:		Scatter/gather list (for kernel buffers)
 *			NULL if user_buf_ptr!=NULL.
 * @buf_size:		Buffer size in bytes
 * @dma_direction:	DMA direction
 *
 * Returns int >= is the registered memory reference ID, <0 for error
 */
int register_client_memref(
	struct sep_client_ctx *client_ctx,
	uint8_t __user *user_buf_ptr,
	struct scatterlist *sgl,
	const unsigned long buf_size,
	const enum dma_data_direction dma_direction)
{
	int free_memref_idx, rc;
	struct registered_memref *regmem_p;

	if (unlikely((user_buf_ptr != NULL) && (sgl != NULL))) {
		SEP_LOG_ERR("Both user_buf_ptr and sgl are given!\n");
		return -EINVAL;
	}

	/* Find free entry in user_memref */
	for (free_memref_idx = 0, regmem_p = &client_ctx->reg_memrefs[0];
	      free_memref_idx < MAX_REG_MEMREF_PER_CLIENT_CTX;
	      free_memref_idx++, regmem_p++) {
		mutex_lock(&regmem_p->buf_lock);
		if (regmem_p->ref_cnt == 0)
			break; /* found free entry */
		mutex_unlock(&regmem_p->buf_lock);
	}
	if (unlikely(free_memref_idx == MAX_REG_MEMREF_PER_CLIENT_CTX)) {
		SEP_LOG_WARN("No free entry for user memory registration\n");
		free_memref_idx = -ENOMEM; /* Negative error code as index */
	} else {
		SEP_LOG_DEBUG("Allocated memref_idx=%d (regmem_p=%p)\n",
			free_memref_idx, regmem_p);
		regmem_p->ref_cnt = 1; /* Capture entry */
		/* Lock user pages for DMA and save pages info.
		   (prepare for MLLI) */
		rc = llimgr_register_client_dma_buf(
			client_ctx->drv_data->sep_data->llimgr,
			user_buf_ptr, sgl, buf_size, 0, dma_direction,
			&regmem_p->dma_obj);
		if (unlikely(rc < 0)) {
			/* Release entry */
			regmem_p->ref_cnt = 0;
			free_memref_idx = rc;
		}
		mutex_unlock(&regmem_p->buf_lock);
	}

	/* Return user_memref[] entry index as the memory reference ID */
	return free_memref_idx;
}

/**
 * free_user_memref() - Free resources associated with a user memory reference
 * @client_ctx:	 User context data
 * @memref_idx:	 Index of the user memory reference
 *
 * Free resources associated with a user memory reference
 * (The referenced memory may be locked user pages or allocated DMA-coherent
 *  memory mmap'ed to the user space)
 * Returns int !0 on failure (memref still in use or unknown)
 */
int free_client_memref(
	struct sep_client_ctx *client_ctx,
	dxdi_memref_id_t memref_idx)
{
	struct registered_memref *regmem_p =
		&client_ctx->reg_memrefs[memref_idx];
	int rc = 0;

	if (!IS_VALID_MEMREF_IDX(memref_idx)) {
		SEP_LOG_ERR("Invalid memref ID %d\n", memref_idx);
		return -EINVAL;
	}

	mutex_lock(&regmem_p->buf_lock);

	if (likely(regmem_p->ref_cnt == 1)) {
		/* TODO: support case of allocated DMA-coherent buffer*/
		llimgr_deregister_client_dma_buf(
			client_ctx->drv_data->sep_data->llimgr,
			&regmem_p->dma_obj);
		regmem_p->ref_cnt = 0;
	} else if (unlikely(regmem_p->ref_cnt == 0)) {
		SEP_LOG_ERR("Invoked for free memref ID=%d\n", memref_idx);
		rc = -EINVAL;
	} else { /* ref_cnt > 1 */
		SEP_LOG_ERR("BUSY/Invalid memref to release: ref_cnt=%d , "
			    "user_buf_ptr=%p\n",
			regmem_p->ref_cnt, regmem_p->dma_obj.user_buf_ptr);
		rc = -EBUSY;
	}

	mutex_unlock(&regmem_p->buf_lock);

	return rc;
}

/**
 * acquire_dma_obj() - Get the memref object of given memref_idx and increment
 *			reference count of it
 * @client_ctx:	Associated client context
 * @memref_idx:	Required registered memory reference ID (index)
 *
 * Get the memref object of given memref_idx and increment reference count of it
 * The returned object must be released by invoking release_dma_obj() before
 * the object (memref) may be freed.
 * Returns struct user_dma_buffer The memref object or NULL for invalid
 */
struct client_dma_buffer *acquire_dma_obj(
	struct sep_client_ctx *client_ctx,
	dxdi_memref_id_t memref_idx)
{
	struct registered_memref *regmem_p =
		&client_ctx->reg_memrefs[memref_idx];
	struct client_dma_buffer *rc;

	if (!IS_VALID_MEMREF_IDX(memref_idx)) {
		SEP_LOG_ERR("Invalid memref ID %d\n", memref_idx);
		return NULL;
	}

	mutex_lock(&regmem_p->buf_lock);
	if (regmem_p->ref_cnt < 1) {
		SEP_LOG_ERR("Invalid memref (ID=%d, ref_cnt=%d)\n",
			memref_idx, regmem_p->ref_cnt);
		rc = NULL;
	} else {
		regmem_p->ref_cnt++;
		rc = &regmem_p->dma_obj;
	}
	mutex_unlock(&regmem_p->buf_lock);

	return rc;
}

/**
 * release_dma_obj() - Release memref object taken with get_memref_obj
 *			(Does not free!)
 * @client_ctx:	Associated client context
 * @dma_obj:	The DMA object returned from acquire_dma_obj()
 *
 * Returns void
 */
void release_dma_obj(struct sep_client_ctx *client_ctx,
	struct client_dma_buffer *dma_obj)
{
	struct registered_memref *regmem_p;
	dxdi_memref_id_t memref_idx;

	if (dma_obj == NULL) /* Probably failed on acquire_dma_obj */
		return;
	/* Verify valid container */
	memref_idx = DMA_OBJ_TO_MEMREF_IDX(client_ctx, dma_obj);
	if (!IS_VALID_MEMREF_IDX(memref_idx)) {
		SEP_LOG_ERR("Given DMA object is not registered\n");
		return;
	}
	/* Get container */
	regmem_p = &client_ctx->reg_memrefs[memref_idx];
	mutex_lock(&regmem_p->buf_lock);
	if (regmem_p->ref_cnt < 2) {
		SEP_LOG_ERR("Invalid memref (ref_cnt=%d, user_buf_ptr=%p)\n",
			regmem_p->ref_cnt, regmem_p->dma_obj.user_buf_ptr);
	} else
		regmem_p->ref_cnt--;
	mutex_unlock(&regmem_p->buf_lock);
}

/**
 * crypto_op_completion_cleanup() - Cleanup CRYPTO_OP descriptor operation
 *					resources after completion
 * @op_ctx:
 *
 * Returns int
 */
int crypto_op_completion_cleanup(struct sep_op_ctx *op_ctx)
{
	struct sep_client_ctx *client_ctx = op_ctx->client_ctx;
	struct queue_drvdata *drvdata = client_ctx->drv_data;
	llimgr_h llimgr = drvdata->sep_data->llimgr;
	struct client_crypto_ctx_info *ctx_info_p = &(op_ctx->ctx_info);
	enum sep_op_type op_type = op_ctx->op_type;
	uint32_t error_info = op_ctx->error_info;
	uint32_t ctx_info_idx;
	bool data_in_place;
	const enum crypto_alg_class alg_class =
		ctxmgr_get_alg_class(&op_ctx->ctx_info);

	if (op_type & (SEP_OP_CRYPTO_PROC | SEP_OP_CRYPTO_FINI)) {
		/* Resources cleanup on data processing operations (PROC/FINI)*/
		if ((alg_class == ALG_CLASS_HASH) ||
		     (ctxmgr_get_mac_type(ctx_info_p) == DXDI_MAC_HMAC)) {
			/* Unmap what was mapped in prepare_data_for_sep() */
			/* If there was no tail mapped this would do nothing */
			ctxmgr_unmap2dev_hash_tail(ctx_info_p,
				drvdata->sep_data->dev);
			/* Save last data block tail (remainder of crypto-block)
			   or clear that buffer after it was used if there is
			   no new block remainder data */
			ctxmgr_save_hash_blk_remainder(ctx_info_p,
				&op_ctx->din_dma_obj, false);
		}
		data_in_place = llimgr_is_same_mlli_tables(llimgr,
			&op_ctx->ift, &op_ctx->oft);
		/* First free IFT resources */
		llimgr_destroy_mlli(llimgr, &op_ctx->ift);
		llimgr_deregister_client_dma_buf(llimgr, &op_ctx->din_dma_obj);
		/* Free OFT resources */
		if (data_in_place) {
			/* OFT already destroyed as IFT. Just clean it. */
			MLLI_TABLES_LIST_INIT(&op_ctx->oft);
			CLEAN_DMA_BUFFER_INFO(&op_ctx->din_dma_obj);
		} else { /* OFT resources cleanup */
			llimgr_destroy_mlli(llimgr, &op_ctx->oft);
			llimgr_deregister_client_dma_buf(llimgr,
				&op_ctx->dout_dma_obj);
		}
	}

	for (ctx_info_idx = 0;
	     ctx_info_idx < op_ctx->ctx_info_num;
	     ctx_info_idx++) {
		if ((op_type & SEP_OP_CRYPTO_FINI) || (error_info != 0)) {
			/* If this was a finalizing descriptor, or any error,
			   invalidate from cache */
			ctxmgr_sep_cache_invalidate(drvdata->sep_cache,
				ctxmgr_get_ctx_id(&ctx_info_p[ctx_info_idx]),
				CRYPTO_CTX_ID_SINGLE_MASK);
		}
		/* Update context state */
		if ((op_type & SEP_OP_CRYPTO_FINI) ||
		    ((op_type & SEP_OP_CRYPTO_INIT) && (error_info != 0))) {
			/* If this was a finalizing descriptor,
			   or a failing initializing descriptor: */
			ctxmgr_set_ctx_state(&ctx_info_p[ctx_info_idx],
				      CTX_STATE_UNINITIALIZED);
		} else if (op_type & SEP_OP_CRYPTO_INIT)
			ctxmgr_set_ctx_state(&ctx_info_p[ctx_info_idx],
				      CTX_STATE_INITIALIZED);
	}

	return 0;
}


/**
 * wait_for_sep_op_result() - Wait for outstanding SeP operation to complete and
 *				fetch SeP ret-code
 * @op_ctx:
 *
 * Wait for outstanding SeP operation to complete and fetch SeP ret-code
 * into op_ctx->sep_ret_code
 * Returns int
 */
int wait_for_sep_op_result(struct sep_op_ctx *op_ctx)
{
#ifdef DEBUG
	if (unlikely(op_ctx->op_state == USER_OP_NOP)) {
		SEP_LOG_ERR("Operation context is inactive!\n");
		op_ctx->error_info = DXDI_ERROR_FATAL;
		return -EBUG;
	}
#endif

	/* wait until crypto operation is completed.
	   We cannot timeout this operation because hardware operations may
	   be still pending on associated data buffers.
	   Only system reboot can take us out of this abnormal state in a safe
	   manner (avoiding data corruption) */
	wait_for_completion(&(op_ctx->ioctl_op_compl));
#ifdef DEBUG
	if (unlikely(op_ctx->op_state != USER_OP_COMPLETED)) {
		SEP_LOG_ERR("Op. state is not COMPLETED "
			"after getting completion event "
			"(op_ctx=0x%p, op_state=%d)\n",
			op_ctx, op_ctx->op_state);
		dump_stack(); /*SEP_DRIVER_BUG();*/
		op_ctx->error_info = DXDI_ERROR_FATAL;
		return -EBUG;
	}
#endif

	return 0;
}

/**
 * get_num_of_ctx_info() - Count the number of valid contexts assigned for the
 *				combined operation.
 * @config:	 The user configuration scheme array
 *
 * Returns int
 */
static int get_num_of_ctx_info(struct dxdi_combined_props *config)
{
	int valid_ctx_n;

	for (valid_ctx_n = 0;
	      (config->node_props[valid_ctx_n].context != NULL) &&
	      (valid_ctx_n < DXDI_COMBINED_NODES_MAX);
	      valid_ctx_n++);

	return valid_ctx_n;
}

/***** Driver Interface implementation functions *****/

/**
 * format_sep_combined_cfg_scheme() - Encode the user configuration scheme to
 *					SeP format.
 * @config:	 The user configuration scheme array
 * @op_ctx:	 Operation context
 *
 * Returns uint32_t
 */
static uint32_t format_sep_combined_cfg_scheme(
	struct dxdi_combined_props *config,
	struct sep_op_ctx *op_ctx
)
{
	enum dxdi_input_engine_type engine_src = DXDI_INPUT_NULL;
	enum sep_engine_type engine_type = SEP_ENGINE_NULL;
	enum crypto_alg_class alg_class;
	uint32_t sep_cfg_scheme = 0; /* the encoded config scheme */
	struct client_crypto_ctx_info *ctx_info_p = &op_ctx->ctx_info;
	enum dxdi_sym_cipher_type symc_type;
	int eng_idx, done = 0;
	int prev_direction = -1;

	/* encode engines connections into SEP format */
	for (eng_idx = 0;
	     (eng_idx < DXDI_COMBINED_NODES_MAX) && (!done);
	     eng_idx++, ctx_info_p++) {

		/* set engine source */
		engine_src = config->node_props[eng_idx].eng_input;

		/* set engine type */
		if (config->node_props[eng_idx].context != NULL) {
			int dir;
			alg_class = ctxmgr_get_alg_class(ctx_info_p);
			switch (alg_class) {
			case ALG_CLASS_HASH:
				engine_type = SEP_ENGINE_HASH;
				break;
			case ALG_CLASS_SYM_CIPHER:
				symc_type = ctxmgr_get_sym_cipher_type(
						ctx_info_p);
				if ((symc_type == DXDI_SYMCIPHER_AES_ECB) ||
				    (symc_type == DXDI_SYMCIPHER_AES_CBC) ||
				    (symc_type == DXDI_SYMCIPHER_AES_CTR) ||
				    (symc_type == DXDI_SYMCIPHER_AES_OFB))
					engine_type = SEP_ENGINE_AES;
				else
					engine_type = SEP_ENGINE_NULL;

				dir = (int)ctxmgr_get_symcipher_direction(ctx_info_p);
				if (prev_direction == -1) {
					prev_direction = dir;
				} else {
					/* Allow only decrypt->encrypt operation */
					if (!(prev_direction == SEP_CRYPTO_DIRECTION_DECRYPT &&
					    dir == SEP_CRYPTO_DIRECTION_ENCRYPT))
					{
						SEP_LOG_ERR("Invalid direction combination %s->%s\n",
							prev_direction ==  SEP_CRYPTO_DIRECTION_DECRYPT? "DEC" : "ENC",
							dir == SEP_CRYPTO_DIRECTION_DECRYPT? "DEC"  : "ENC");
						op_ctx->error_info = DXDI_ERROR_INVAL_DIRECTION;
					}
				}
				break;
			default:
				engine_type = SEP_ENGINE_NULL;
				break; /*unsupported alg class*/
			}
		} else if (engine_src != DXDI_INPUT_NULL) {
			/* incase engine source is not NULL and NULL sub-context
			is passed then DOUT is -DOUT type*/
			engine_type = SEP_ENGINE_DOUT;
			/* exit after props set */
			done = 1;
		} else {
			/* both context pointer & input type are
			NULL -we're done */
			break;
		}

		SepCombinedEnginePropsSet(&sep_cfg_scheme, eng_idx,
					engine_src, engine_type);
	}

	return sep_cfg_scheme;
}

/**
 * init_crypto_context() - Initialize host crypto context
 * @op_ctx:
 * @context_buf:
 * @alg_class:
 * @props:	Pointer to configuration properties which match given alg_class:
 *		ALG_CLASS_SYM_CIPHER: struct dxdi_sym_cipher_props
 *		ALG_CLASS_AUTH_ENC: struct dxdi_auth_enc_props
 *		ALG_CLASS_MAC: struct dxdi_mac_props
 *		ALG_CLASS_HASH: enum dxdi_hash_type
 *
 * Returns int 0 if operation executed in SeP.
 * See error_info for actual results.
 */
static int init_crypto_context(
	struct sep_op_ctx *op_ctx,
	uint32_t __user *context_buf,
	enum crypto_alg_class alg_class,
	void *props)
{
	int rc;
	int sep_cache_load_required;
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	bool postpone_init = false;

	rc = ctxmgr_map_user_ctx(
		&op_ctx->ctx_info, drvdata->sep_data->dev, alg_class,
		context_buf);
	if (rc != 0) {
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		return rc;
	}
	op_ctx->op_type = SEP_OP_CRYPTO_INIT;
	ctxmgr_set_ctx_state(&op_ctx->ctx_info, CTX_STATE_UNINITIALIZED);
	/* Allocate a new Crypto context ID */
	ctxmgr_set_ctx_id(&op_ctx->ctx_info,
		alloc_crypto_ctx_id(op_ctx->client_ctx));

	switch (alg_class) {
	case ALG_CLASS_SYM_CIPHER:
		rc = ctxmgr_init_symcipher_ctx(&op_ctx->ctx_info,
			(struct dxdi_sym_cipher_props *)props,
			&postpone_init, &op_ctx->error_info);
		break;
	case ALG_CLASS_AUTH_ENC:
		rc = ctxmgr_init_auth_enc_ctx(&op_ctx->ctx_info,
			(struct dxdi_auth_enc_props *)props,
			&op_ctx->error_info);
		break;
	case ALG_CLASS_MAC:
		rc = ctxmgr_init_mac_ctx(&op_ctx->ctx_info,
			(struct dxdi_mac_props *)props,
			&op_ctx->error_info);
		break;
	case ALG_CLASS_HASH:
		rc = ctxmgr_init_hash_ctx(&op_ctx->ctx_info,
			*((enum dxdi_hash_type *)props),
			&op_ctx->error_info);
		break;
	default:
		SEP_LOG_ERR("Invalid algorithm class %d\n", alg_class);
		op_ctx->error_info = DXDI_ERROR_UNSUP;
		rc = -EINVAL;
	}
	if (rc != 0)
		goto ctx_init_exit;

	/* After inialization above we are partially init. (missing SeP init.)*/
	ctxmgr_set_ctx_state(&op_ctx->ctx_info, CTX_STATE_PARTIAL_INIT);
#ifdef DEBUG
	ctxmgr_dump_sep_ctx(&op_ctx->ctx_info);
#endif

	/* If not all the init. information is available at this time
	   we postpone INIT in SeP to processing phase */
	if (postpone_init) {
		SEP_LOG_DEBUG("Init. postponed to processing phase\n");
		ctxmgr_unmap_user_ctx(&op_ctx->ctx_info);
		/* must be valid on "success" */
		op_ctx->error_info = DXDI_ERROR_NULL;
		return 0;
	}

	ctxmgr_sync_sep_ctx(&op_ctx->ctx_info,
			 drvdata->sep_data->dev); /* Flush out of host cache */
	/* Start critical section -
	   cache allocation must be coupled to descriptor enqueue */
	rc = mutex_lock_interruptible(
		&drvdata->desc_queue_sequencer);
	if (rc != 0) {
		SEP_LOG_ERR("Failed locking descQ sequencer[%u]\n",
			op_ctx->client_ctx->qid);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
		goto ctx_init_exit;
	}
	ctxmgr_set_sep_cache_idx(&op_ctx->ctx_info,
			      ctxmgr_sep_cache_alloc(
				      drvdata->sep_cache,
				      ctxmgr_get_ctx_id(&op_ctx->ctx_info),
				      &sep_cache_load_required));
	if (!sep_cache_load_required)
		SEP_LOG_ERR("New context already in SeP cache?!");
	rc = send_crypto_op_desc(op_ctx,
				 1/*always load on init*/, 1/*INIT*/,
				 SEP_PROC_MODE_NOP);
	mutex_unlock(&drvdata->desc_queue_sequencer);
	if (likely(rc == 0))
		rc = wait_for_sep_op_result(op_ctx);

ctx_init_exit:
	/* Cleanup resources and update context state */
	crypto_op_completion_cleanup(op_ctx);
	ctxmgr_unmap_user_ctx(&op_ctx->ctx_info);

	return rc;
}

/**
 * map_ctx_for_proc() - Map previously initialized crypto context before data
 *			processing
 * @op_ctx:
 * @context_buf:
 * @ctx_state:	 Returned current context state
 *
 * Returns int
 */
static int map_ctx_for_proc(
	struct sep_client_ctx *client_ctx,
	struct client_crypto_ctx_info *ctx_info,
	uint32_t __user *context_buf,
	enum host_ctx_state *ctx_state_p)
{
	int rc;
	struct queue_drvdata *drvdata = client_ctx->drv_data;

	/* default state in case of error */
	*ctx_state_p = CTX_STATE_UNINITIALIZED;

	rc = ctxmgr_map_user_ctx(ctx_info, drvdata->sep_data->dev,
		ALG_CLASS_NONE, context_buf);
	if (rc != 0) {
		SEP_LOG_ERR("Failed mapping context\n");
		return rc;
	}
	if (ctxmgr_get_session_id(ctx_info) != (uint32_t)client_ctx) {
		SEP_LOG_ERR("Context ID is not associated with "
			    "this session!\n");
		rc = -EINVAL;
	}
	if (rc == 0)
		*ctx_state_p = ctx_info->ctx_kptr->state;

#ifdef DEBUG
	ctxmgr_dump_sep_ctx(ctx_info);
	ctxmgr_sync_sep_ctx(ctx_info,
			 drvdata->sep_data->dev); /* Flush out of host cache */
#endif
	if (rc != 0)
		ctxmgr_unmap_user_ctx(ctx_info);

	return rc;
}

/**
 * init_combined_context() - Initialize Combined context
 * @op_ctx:
 * @config: Pointer to configuration scheme to be validate by the SeP
 *
 * Returns int 0 if operation executed in SeP.
 * See error_info for actual results.
 */
static int init_combined_context(
	struct sep_op_ctx *op_ctx,
	struct dxdi_combined_props *config)
{
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	struct client_crypto_ctx_info *ctx_info_p = &op_ctx->ctx_info;
	int sep_ctx_load_req[DXDI_COMBINED_NODES_MAX];
	enum host_ctx_state ctx_state;
	int rc, ctx_idx, ctx_mapped_n = 0;

	op_ctx->op_type = SEP_OP_CRYPTO_INIT;

	/* no context to load -clear buffer */
	memset(sep_ctx_load_req, 0, sizeof(sep_ctx_load_req));

	/* set the number of active contexts */
	op_ctx->ctx_info_num = get_num_of_ctx_info(config);

	/* map each context in the configuration scheme */
	for (ctx_idx = 0; ctx_idx < op_ctx->ctx_info_num; ctx_idx++) {
		/* context already initialized by the user */
		rc = map_ctx_for_proc(
			op_ctx->client_ctx,
			&ctx_info_p[ctx_idx],
			config->node_props[ctx_idx].context, /*user ctx*/
			&ctx_state);
		if (rc != 0) {
			op_ctx->error_info = DXDI_ERROR_BAD_CTX;
			goto ctx_init_exit;
		}

		ctx_mapped_n++; /*ctx mapped successfully*/

		/* context must be initialzed */
		if (ctx_state != CTX_STATE_INITIALIZED) {
			SEP_LOG_ERR("Given context [%d] in invalid state for"
				    " processing -%d\n", ctx_idx, ctx_state);
			op_ctx->error_info = DXDI_ERROR_BAD_CTX;
			rc = -EINVAL;
			goto ctx_init_exit;
		}
	}

	/* Start critical section -
	   cache allocation must be coupled to descriptor enqueue */
	rc = mutex_lock_interruptible(
		&drvdata->desc_queue_sequencer);
	if (rc == 0) { /* Coupled sequence */
		for (ctx_idx = 0; ctx_idx < op_ctx->ctx_info_num; ctx_idx++) {
			ctxmgr_set_sep_cache_idx(
				&ctx_info_p[ctx_idx],
				ctxmgr_sep_cache_alloc(
					drvdata->sep_cache,
					ctxmgr_get_ctx_id(&ctx_info_p[ctx_idx]),
					&sep_ctx_load_req[ctx_idx]));
		}

		rc = send_combined_op_desc(op_ctx,
				sep_ctx_load_req/*nothing to load*/,
				1/*INIT*/,
				SEP_PROC_MODE_NOP,
				0/*no scheme in init*/);

		mutex_unlock(&drvdata->desc_queue_sequencer);
	} else {
		SEP_LOG_ERR("Failed locking descQ sequencer[%u]\n",
			op_ctx->client_ctx->qid);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
	}
	if (rc == 0)
		rc = wait_for_sep_op_result(op_ctx);

	crypto_op_completion_cleanup(op_ctx);

ctx_init_exit:
	for (ctx_idx = 0; ctx_idx < ctx_mapped_n; ctx_idx++)
		ctxmgr_unmap_user_ctx(&ctx_info_p[ctx_idx]);

	return rc;
}

/**
 * prepare_adata_for_sep() - Generate MLLI tables for additional/associated data
 *				(input only)
 * @op_ctx:	 Operation context
 * @adata_in:
 * @adata_in_size:
 *
 * Returns int
 */
static inline int prepare_adata_for_sep(
	struct sep_op_ctx *op_ctx,
	uint8_t __user *adata_in,
	uint32_t adata_in_size
)
{
	struct sep_client_ctx *client_ctx = op_ctx->client_ctx;
	llimgr_h llimgr = client_ctx->drv_data->sep_data->llimgr;
	struct dma_pool *spad_buf_pool =
		client_ctx->drv_data->sep_data->spad_buf_pool;
	struct mlli_tables_list *ift_p = &op_ctx->ift;
	unsigned long a0_buf_size;
	u8 *a0_buf_p;
	int rc = 0;

	if (adata_in == NULL) {	/*data_out required for this alg_class*/
		SEP_LOG_ERR("adata_in==NULL for authentication\n");
		op_ctx->error_info = DXDI_ERROR_INVAL_DIN_PTR;
		return -EINVAL;
	}

	op_ctx->spad_buf_p = dma_pool_alloc(spad_buf_pool,
		GFP_KERNEL, &op_ctx->spad_buf_dma_addr);
	if (unlikely(op_ctx->spad_buf_p == NULL)) {
		SEP_LOG_ERR("Failed allocating from spad_buf_pool for A0\n");
		return -ENOMEM;
	}
	a0_buf_p = op_ctx->spad_buf_p;

	/* format A0 (the first 2 words in the first block) */
	if (adata_in_size < ((1UL << 16) - (1UL << 8))) {
		a0_buf_size = 2;

		a0_buf_p[0] = (adata_in_size >> 8) & 0xFF;
		a0_buf_p[1] = adata_in_size & 0xFF;
	} else {
		a0_buf_size = 6;

		a0_buf_p[0] = 0xFF;
		a0_buf_p[1] = 0xFE;
		a0_buf_p[2] = (adata_in_size >> 24) & 0xFF;
		a0_buf_p[3] = (adata_in_size >> 16) & 0xFF;
		a0_buf_p[4] = (adata_in_size >> 8) & 0xFF;
		a0_buf_p[5] = adata_in_size & 0xFF;
	}

	/* Create IFT (MLLI table) */
	rc = llimgr_register_client_dma_buf(llimgr,
		adata_in, NULL, adata_in_size, 0, DMA_TO_DEVICE,
		&op_ctx->din_dma_obj);
	if (likely(rc == 0)) {
		rc = llimgr_create_mlli(llimgr, ift_p, DMA_TO_DEVICE,
			&op_ctx->din_dma_obj,
			op_ctx->spad_buf_dma_addr, a0_buf_size);
		if (unlikely(rc != 0)) {
			llimgr_deregister_client_dma_buf(llimgr,
				&op_ctx->din_dma_obj);
		}
	}

	return rc;
}

/**
 * prepare_cipher_data_for_sep() - Generate MLLI tables for cipher algorithms
 *				(input + output)
 * @op_ctx:	 Operation context
 * @data_in:	User space pointer for input data (NULL for kernel data)
 * @sgl_in:	Kernel buffers s/g list for input data (NULL for user data)
 * @data_out:	User space pointer for output data (NULL for kernel data)
 * @sgl_out:	Kernel buffers s/g list for output data (NULL for user data)
 * @data_in_size:
 *
 * Returns int
 */
static inline int prepare_cipher_data_for_sep(
	struct sep_op_ctx *op_ctx,
	uint8_t __user *data_in,
	struct scatterlist *sgl_in,
	uint8_t __user *data_out,
	struct scatterlist *sgl_out,
	uint32_t data_in_size
)
{
	struct sep_client_ctx *client_ctx = op_ctx->client_ctx;
	llimgr_h llimgr = client_ctx->drv_data->sep_data->llimgr;
	const bool is_data_inplace =
		data_in != NULL ? (data_in == data_out) : (sgl_in == sgl_out);
	const enum dma_data_direction din_dma_direction =
		is_data_inplace ? DMA_BIDIRECTIONAL : DMA_TO_DEVICE;
	int rc;

	/* Check parameters */
	if (data_in_size == 0) { /* No data to prepare */
		return 0;
	}
	if ((data_out == NULL) && (sgl_out == NULL)) {
		/* data_out required for this alg_class */
		SEP_LOG_ERR("data_out/sgl_out==NULL for enc/decryption\n");
		op_ctx->error_info = DXDI_ERROR_INVAL_DOUT_PTR;
		return -EINVAL;
	}

	/* Avoid partial overlapping of data_in with data_out */
	if (!is_data_inplace)
		if (data_in != NULL) { /* User space buffer */
			if (((data_in < data_out) &&
			    ((data_in + data_in_size) > data_out)) ||
			    ((data_out < data_in) &&
			    ((data_out + data_in_size) > data_in))) {
				SEP_LOG_ERR("Buffers partially overlap!\n");
				op_ctx->error_info =
					DXDI_ERROR_DIN_DOUT_OVERLAP;
				return -EINVAL;
			}
		}
		/* else: TODO - scan s/g lists for overlapping */

	/* Create IFT + OFT (MLLI tables) */
	rc = llimgr_register_client_dma_buf(llimgr,
		data_in, sgl_in, data_in_size, 0, din_dma_direction,
		&op_ctx->din_dma_obj);
	if (likely(rc == 0)) {
		rc = llimgr_create_mlli(llimgr, &op_ctx->ift, din_dma_direction,
			&op_ctx->din_dma_obj, 0, 0);
	}
	if (likely(rc == 0)) {
		if (is_data_inplace) {
			/* Mirror IFT data in OFT */
			op_ctx->dout_dma_obj = op_ctx->din_dma_obj;
			op_ctx->oft = op_ctx->ift;
		} else { /* Create OFT */
			rc = llimgr_register_client_dma_buf(llimgr,
				data_out, sgl_out, data_in_size, 0,
				DMA_FROM_DEVICE, &op_ctx->dout_dma_obj);
			if (likely(rc == 0)) {
				rc = llimgr_create_mlli(llimgr, &op_ctx->oft,
					DMA_FROM_DEVICE, &op_ctx->dout_dma_obj,
					0, 0);
			}
		}
	}

	if (unlikely(rc != 0)) {/* Failure cleanup */
		/* No output MLLI to free in error case */
		if (!is_data_inplace) {
			llimgr_deregister_client_dma_buf(llimgr,
				&op_ctx->dout_dma_obj);
		}
		llimgr_destroy_mlli(llimgr, &op_ctx->ift);
		llimgr_deregister_client_dma_buf(llimgr, &op_ctx->din_dma_obj);
	}

	return rc;
}


/**
 * prepare_hash_data_for_sep() - Prepare data for hash operation
 * @op_ctx:		Operation context
 * @is_finalize:	Is hash finialize operation (last)
 * @data_in:		Pointer to user buffer OR...
 * @sgl_in:		Gather list for kernel buffers
 * @data_in_size:	Data size in bytes
 *
 * Returns 0 on success
 */
static int prepare_hash_data_for_sep(
	struct sep_op_ctx *op_ctx,
	bool is_finalize,
	uint8_t __user *data_in,
	struct scatterlist *sgl_in,
	uint32_t data_in_size)
{
	struct sep_client_ctx *client_ctx = op_ctx->client_ctx;
	struct queue_drvdata *drvdata = client_ctx->drv_data;
	llimgr_h llimgr = drvdata->sep_data->llimgr;
	uint32_t crypto_blk_size;
	/* data size for processing this op. (incl. prev. block remainder) */
	uint32_t data_size4hash;
	/* amount of data_in to process in this op. */
	uint32_t data_in_save4next;
	uint16_t last_hash_blk_tail_size;
	dma_addr_t last_hash_blk_tail_dma;
	int rc;

	if ((data_in != NULL) && (sgl_in != NULL)) {
		SEP_LOG_ERR("Given valid data_in+sgl_in!\n");
		return -EINVAL;
	}

	/*Hash block size required in order to buffer block remainders*/
	crypto_blk_size =
		ctxmgr_get_crypto_blk_size(&op_ctx->ctx_info);
	if (crypto_blk_size == 0) { /* Unsupported algorithm?...*/
		op_ctx->error_info = DXDI_ERROR_UNSUP;
		return -ENOSYS;
	}

	/* Map for DMA the last block tail, if any */
	rc = ctxmgr_map2dev_hash_tail(&op_ctx->ctx_info,
		drvdata->sep_data->dev);
	if (rc != 0) {
		SEP_LOG_ERR("Failed mapping hash data tail buffer\n");
		return rc;
	}
	last_hash_blk_tail_size = ctxmgr_get_hash_blk_remainder_buf(
		&op_ctx->ctx_info, &last_hash_blk_tail_dma);
	data_size4hash = data_in_size + last_hash_blk_tail_size;
	if (!is_finalize) {
		/* Not last. Round to hash block size. */
		data_size4hash =
			(data_size4hash &
			 ~(crypto_blk_size - 1));
		/* On the last hash op. take all that's left */
	}
	data_in_save4next = (data_size4hash > 0) ?
		data_in_size - (data_size4hash - last_hash_blk_tail_size) :
		data_in_size;
	rc = llimgr_register_client_dma_buf(llimgr,
		data_in, sgl_in, data_in_size, data_in_save4next,
		DMA_TO_DEVICE, &op_ctx->din_dma_obj);
	if (unlikely(rc != 0)) {
		SEP_LOG_ERR("Failed registering client buffer (rc=%d)\n", rc);
		/* Unmap hash block tail buffer */
		ctxmgr_unmap2dev_hash_tail(&op_ctx->ctx_info,
			drvdata->sep_data->dev);
	} else {
		if ((!is_finalize) && (data_size4hash == 0)) {
			/* Not enough for even one hash block */
			/* Append to existing tail buffer */
			rc = ctxmgr_save_hash_blk_remainder(
				&op_ctx->ctx_info, &op_ctx->din_dma_obj,
				true/*append*/);
			if (rc == 0) /* signal: not even one block */
				rc = -ENOTBLK;
			/* Unmap memory resources - no processing this time */
			llimgr_deregister_client_dma_buf(llimgr,
				&op_ctx->din_dma_obj);
			ctxmgr_unmap2dev_hash_tail(&op_ctx->ctx_info,
				drvdata->sep_data->dev);
		} else {
			rc = llimgr_create_mlli(llimgr, &op_ctx->ift,
				DMA_TO_DEVICE, &op_ctx->din_dma_obj,
				last_hash_blk_tail_dma,
				last_hash_blk_tail_size);
			if (unlikely(rc != 0)) { /* Failed - unmap resources */
				llimgr_deregister_client_dma_buf(llimgr,
					&op_ctx->din_dma_obj);
				/* Unmap hash block tail buffer */
				ctxmgr_unmap2dev_hash_tail(&op_ctx->ctx_info,
					drvdata->sep_data->dev);
			}
		}
	}

	/* Hash block remainder would be copied into tail buffer only after
	   operation completion, because this buffer is still in use for
	   current operation */

	return rc;
}


/**
 * prepare_mac_data_for_sep() - Prepare data memory objects for (AES) MAC
 *				operations
 * @op_ctx:
 * @data_in:		Pointer to user buffer OR...
 * @sgl_in:		Gather list for kernel buffers
 * @data_in_size:
 *
 * Returns int
 */
static inline int prepare_mac_data_for_sep(
	struct sep_op_ctx *op_ctx,
	uint8_t __user *data_in,
	struct scatterlist *sgl_in,
	uint32_t data_in_size)
{
	struct sep_client_ctx *client_ctx = op_ctx->client_ctx;
	llimgr_h llimgr = client_ctx->drv_data->sep_data->llimgr;
	int rc;

	rc = llimgr_register_client_dma_buf(llimgr,
		data_in, sgl_in, data_in_size, 0, DMA_TO_DEVICE,
		&op_ctx->din_dma_obj);
	if (likely(rc == 0)) {
		rc = llimgr_create_mlli(llimgr, &op_ctx->ift,
			DMA_TO_DEVICE, &op_ctx->din_dma_obj, 0, 0);
		if (rc != 0) {
			llimgr_deregister_client_dma_buf(llimgr,
				&op_ctx->din_dma_obj);
		}
	}

	return rc;
}

/**
 * prepare_data_for_sep() - Prepare data for processing by SeP
 * @op_ctx:
 * @data_in:	User space pointer for input data (NULL for kernel data)
 * @sgl_in:	Kernel buffers s/g list for input data (NULL for user data)
 * @data_out:	User space pointer for output data (NULL for kernel data)
 * @sgl_out:	Kernel buffers s/g list for output data (NULL for user data)
 * @data_in_size:	 data_in buffer size (and data_out's if not NULL)
 * @data_intent:	 the purpose of the given data
 *
 * Prepare data for processing by SeP
 * (common flow for sep_proc_dblk and sep_fin_proc) .
 * Returns int
 */
int prepare_data_for_sep(
	struct sep_op_ctx *op_ctx,
	uint8_t __user *data_in,
	struct scatterlist *sgl_in,
	uint8_t __user *data_out,
	struct scatterlist *sgl_out,
	uint32_t data_in_size,
	enum crypto_data_intent data_intent)
{
	int rc;
	enum crypto_alg_class alg_class;

	if (data_intent == CRYPTO_DATA_ADATA) {
		/* additional/associated data */
		if (!ctxmgr_is_valid_adata_size(&op_ctx->ctx_info,
			data_in_size)) {
			op_ctx->error_info = DXDI_ERROR_INVAL_DATA_SIZE;
			return -EINVAL;
		}
	} else {
		/* cipher/text data */
		if (!ctxmgr_is_valid_data_unit_size(&op_ctx->ctx_info,
			data_in_size,
			(data_intent == CRYPTO_DATA_TEXT_FINALIZE))) {
			op_ctx->error_info = DXDI_ERROR_INVAL_DATA_SIZE;
			return -EINVAL;
		}
	}

	SEP_LOG_DEBUG("data_in=0x%p/0x%p data_out=0x%p/0x%p data_in_size=%uB\n",
		data_in, sgl_in, data_out, sgl_out, data_in_size);

	alg_class = ctxmgr_get_alg_class(&op_ctx->ctx_info);
	SEP_LOG_DEBUG("alg_class = %d\n", alg_class);
	switch (alg_class) {
	case ALG_CLASS_SYM_CIPHER:
		rc = prepare_cipher_data_for_sep(op_ctx,
			data_in, sgl_in, data_out, sgl_out, data_in_size);
		break;
	case ALG_CLASS_AUTH_ENC:
		if (data_intent == CRYPTO_DATA_ADATA) {
			struct host_crypto_ctx_auth_enc *aead_ctx_p =
				(struct host_crypto_ctx_auth_enc *)op_ctx->
					ctx_info.ctx_kptr;

			if (!aead_ctx_p->is_adata_processed) {
				rc = prepare_adata_for_sep(op_ctx,
					data_in, data_in_size);
				/* no more invocation to adata process
				is allowed */
				aead_ctx_p->is_adata_processed = 1;
			} else {
				/* additional data may be processed
				only once */
				return -EPERM;
			}
		} else {
			rc = prepare_cipher_data_for_sep(op_ctx,
				data_in, sgl_in, data_out, sgl_out,
				data_in_size);
		}
		break;
	case ALG_CLASS_MAC:
	case ALG_CLASS_HASH:
		if ((alg_class == ALG_CLASS_MAC) &&
		    (ctxmgr_get_mac_type(&op_ctx->ctx_info) != DXDI_MAC_HMAC)) {
			/* Handle all MACs but HMAC */
			if (data_in_size == 0) { /* No data to prepare */
				rc = 0;
				break;
			}
#if 0
			/* ignore checking the user out pointer due to crys api
			limitation */
			if (data_out != NULL) {
				SEP_LOG_ERR("data_out!=NULL for MAC\n");
				return -EINVAL;
			}
#endif
			rc = prepare_mac_data_for_sep(
				op_ctx, data_in, sgl_in, data_in_size);
			break;
		}

		/* else: HASH or HMAC require the same handling */
		rc = prepare_hash_data_for_sep(op_ctx,
			(data_intent == CRYPTO_DATA_TEXT_FINALIZE),
			data_in, sgl_in, data_in_size);
		break;

	default:
		SEP_LOG_ERR("Invalid algorithm class %d in context\n",
			alg_class);
		/* probably context was corrupted since init. phase */
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		rc = -EBUG;
	}

	return rc;
}

/**
 * prepare_combined_data_for_sep() - Prepare combined data for processing by SeP
 * @op_ctx:
 * @data_in:
 * @data_out:
 * @data_in_size:	 data_in buffer size (and data_out's if not NULL)
 * @data_intent:	 the purpose of the given data
 *
 * Prepare combined data for processing by SeP
 * (common flow for sep_proc_dblk and sep_fin_proc) .
 * Returns int
 */
static int prepare_combined_data_for_sep(
	struct sep_op_ctx *op_ctx,
	uint8_t __user *data_in,
	uint8_t __user *data_out,
	uint32_t data_in_size,
	enum crypto_data_intent data_intent
)
{
	int rc;

	if (data_intent == CRYPTO_DATA_TEXT) {
		/* restrict data unit size to the max block size multiple */
		if (!IS_MULT_OF(data_in_size, SEP_HASH_BLOCK_SIZE_MAX)) {
			SEP_LOG_ERR("Data unit size (%u) is "
				    "not HASH block multiple\n",
					data_in_size);
			op_ctx->error_info = DXDI_ERROR_INVAL_DATA_SIZE;
			return -EINVAL;
		}
	} else if (data_intent == CRYPTO_DATA_TEXT_FINALIZE) {
		/* user may finalize with zero or AES block size multiple */
		if (!IS_MULT_OF(data_in_size, SEP_AES_BLOCK_SIZE)) {
			SEP_LOG_ERR("Data unit size (%u) is "
				    "not AES block multiple\n",
				data_in_size);
			op_ctx->error_info = DXDI_ERROR_INVAL_DATA_SIZE;
			return -EINVAL;
		}
	}

	SEP_LOG_DEBUG("data_in=0x%08lX data_out=0x%08lX data_in_size=%uB\n",
		(unsigned long)data_in, (unsigned long)data_out, data_in_size);

	SEP_LOG_DEBUG("alg_class = COMBINED\n");
	if (data_out == NULL)
		rc = prepare_mac_data_for_sep(op_ctx,
			data_in, NULL, data_in_size);
	else
		rc = prepare_cipher_data_for_sep(op_ctx,
			data_in, NULL, data_out, NULL, data_in_size);

	return rc;
}

/**
 * sep_proc_dblk() - Process data block
 * @op_ctx:
 * @context_buf:
 * @data_block_type:
 * @data_in:
 * @data_out:
 * @data_in_size:
 *
 * Returns int
 */
static int sep_proc_dblk(
	struct sep_op_ctx *op_ctx,
	uint32_t __user *context_buf,
	enum dxdi_data_block_type data_block_type,
	uint8_t __user *data_in,
	uint8_t __user *data_out,
	uint32_t data_in_size)
{
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	int rc;
	int sep_cache_load_required;
	int sep_ctx_init_required = 0;
	enum crypto_alg_class alg_class;
	enum host_ctx_state ctx_state;

	if (data_in_size == 0) {
		SEP_LOG_ERR("Got empty data_in\n");
		op_ctx->error_info = DXDI_ERROR_INVAL_DATA_SIZE;
		return -EINVAL;
	}

	rc = map_ctx_for_proc(op_ctx->client_ctx, &op_ctx->ctx_info,
				context_buf, &ctx_state);
	if (rc != 0) {
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		return rc;
	}
	op_ctx->op_type = SEP_OP_CRYPTO_PROC;
	if (ctx_state == CTX_STATE_PARTIAL_INIT) {
		/* case of postponed sep context init. */
		sep_ctx_init_required = 1;
		op_ctx->op_type |= SEP_OP_CRYPTO_INIT;
	} else if (ctx_state != CTX_STATE_INITIALIZED) {
		SEP_LOG_ERR("Given context in invalid state for processing -"
			    " %d\n", ctx_state);
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		rc = -EINVAL;
		goto unmap_ctx_and_return;
	}
	alg_class = ctxmgr_get_alg_class(&op_ctx->ctx_info);

	rc = prepare_data_for_sep(op_ctx, data_in, NULL, data_out, NULL,
		data_in_size, (data_block_type == DXDI_DATA_TYPE_TEXT ?
			       CRYPTO_DATA_TEXT : CRYPTO_DATA_ADATA));
	if (rc != 0) {
		if (rc == -ENOTBLK) {
			/* Did not accumulate even a single hash block */
			/* The data_in already copied to context, in addition
			   to existing data. Report as success with no op. */
			op_ctx->error_info = DXDI_ERROR_NULL;
			rc = 0;
		}
		goto unmap_ctx_and_return;
	}

	if (sep_ctx_init_required) {
		/* If this flag is set it implies that we have updated
		   parts of the sep_ctx structure during data preparation -
		   need to sync. context to memory (from host cache...) */
#ifdef DEBUG
		ctxmgr_dump_sep_ctx(&op_ctx->ctx_info);
#endif
		ctxmgr_sync_sep_ctx(&op_ctx->ctx_info, drvdata->sep_data->dev);
	}

	/* Start critical section -
	   cache allocation must be coupled to descriptor enqueue */
	rc = mutex_lock_interruptible(
		&drvdata->desc_queue_sequencer);
	if (rc == 0) { /* coupled sequence */
		ctxmgr_set_sep_cache_idx(
			&op_ctx->ctx_info,
			ctxmgr_sep_cache_alloc(
				drvdata->sep_cache,
				ctxmgr_get_ctx_id(&op_ctx->ctx_info),
				&sep_cache_load_required));
		rc = send_crypto_op_desc(op_ctx,
			sep_cache_load_required, sep_ctx_init_required,
			(data_block_type == DXDI_DATA_TYPE_TEXT ?
				SEP_PROC_MODE_PROC_T : SEP_PROC_MODE_PROC_A));
		mutex_unlock(&drvdata->desc_queue_sequencer);
	} else {
		SEP_LOG_ERR("Failed locking descQ sequencer[%u]\n",
			op_ctx->client_ctx->qid);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
	}
	if (rc == 0)
		rc = wait_for_sep_op_result(op_ctx);

	crypto_op_completion_cleanup(op_ctx);

unmap_ctx_and_return:
	ctxmgr_unmap_user_ctx(&op_ctx->ctx_info);

	return rc;
}

/**
 * sep_fin_proc() - Finalize processing of given context with given (optional)
 *			data
 * @op_ctx:
 * @context_buf:
 * @data_in:
 * @data_out:
 * @data_in_size:
 * @digest_or_mac_p:
 * @digest_or_mac_size_p:
 *
 * Returns int
 */
static int sep_fin_proc(
	struct sep_op_ctx *op_ctx,
	uint32_t __user *context_buf,
	uint8_t __user *data_in,
	uint8_t __user *data_out,
	uint32_t data_in_size,
	uint8_t *digest_or_mac_p,
	uint8_t *digest_or_mac_size_p)
{
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	int rc;
	int sep_cache_load_required;
	int sep_ctx_init_required = 0;
	enum crypto_alg_class alg_class;
	enum host_ctx_state ctx_state;

	rc = map_ctx_for_proc(op_ctx->client_ctx, &op_ctx->ctx_info,
				context_buf, &ctx_state);
	if (rc != 0) {
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		return rc;
	}
	if (ctx_state == CTX_STATE_PARTIAL_INIT) {
		/* case of postponed sep context init. */
		sep_ctx_init_required = 1;
	} else if (ctx_state != CTX_STATE_INITIALIZED) {
		SEP_LOG_ERR("Given context in invalid state for finalizing -"
			    " %d\n", ctx_state);
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		rc = -EINVAL;
		goto data_prepare_err;
	}

	op_ctx->op_type = SEP_OP_CRYPTO_FINI;
	alg_class = ctxmgr_get_alg_class(&op_ctx->ctx_info);

	rc = prepare_data_for_sep(op_ctx, data_in, NULL, data_out, NULL,
		data_in_size, CRYPTO_DATA_TEXT_FINALIZE);
	if (rc != 0)
		goto data_prepare_err;

	if (sep_ctx_init_required) {
		/* If this flag is set it implies that we have updated
		   parts of the sep_ctx structure during data preparation -
		   need to sync. context to memory (from host cache...) */
#ifdef DEBUG
		ctxmgr_dump_sep_ctx(&op_ctx->ctx_info);
#endif
		ctxmgr_sync_sep_ctx(&op_ctx->ctx_info, drvdata->sep_data->dev);
	}

	/* Start critical section -
	   cache allocation must be coupled to descriptor enqueue */
	rc = mutex_lock_interruptible(
		&drvdata->desc_queue_sequencer);
	if (rc == 0) { /* Coupled sequence */
		ctxmgr_set_sep_cache_idx(
			&op_ctx->ctx_info,
			ctxmgr_sep_cache_alloc(
				drvdata->sep_cache,
				ctxmgr_get_ctx_id(&op_ctx->ctx_info),
				&sep_cache_load_required));
		rc = send_crypto_op_desc(op_ctx,
			sep_cache_load_required, sep_ctx_init_required,
			SEP_PROC_MODE_FIN);
		mutex_unlock(&drvdata->desc_queue_sequencer);
	} else {
		SEP_LOG_ERR("Failed locking descQ sequencer[%u]\n",
			op_ctx->client_ctx->qid);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
	}
	if (rc == 0)
		rc = wait_for_sep_op_result(op_ctx);

	if ((rc == 0) && (op_ctx->error_info == 0)) {
		/* Digest or MAC are embedded in the SeP/FW context */
		*digest_or_mac_size_p = ctxmgr_get_digest_or_mac(
			&op_ctx->ctx_info, digest_or_mac_p);
		/* If above is not applicable for given algorithm,
		   *digest_or_mac_size_p would be set to 0          */
	} else
		*digest_or_mac_size_p = 0; /* Nothing valid in digest_or_mac_p*/

	crypto_op_completion_cleanup(op_ctx);

data_prepare_err:
	ctxmgr_unmap_user_ctx(&op_ctx->ctx_info);

	return rc;
}

/**
 * sep_combined_proc_dblk() - Process Combined operation block
 * @op_ctx:
 * @config:
 * @data_in:
 * @data_out:
 * @data_in_size:
 *
 * Returns int
 */
static int sep_combined_proc_dblk(
	struct sep_op_ctx *op_ctx,
	struct dxdi_combined_props *config,
	uint8_t __user *data_in,
	uint8_t __user *data_out,
	uint32_t data_in_size)
{
	int rc;
	int ctx_idx, ctx_mapped_n = 0;
	int sep_cache_load_required[DXDI_COMBINED_NODES_MAX];
	enum host_ctx_state ctx_state;
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	struct client_crypto_ctx_info *ctx_info_p = &(op_ctx->ctx_info);
	uint32_t cfg_scheme;

	if (data_in_size == 0) {
		SEP_LOG_ERR("Got empty data_in\n");
		op_ctx->error_info = DXDI_ERROR_INVAL_DATA_SIZE;
		return -EINVAL;
	}

	/* assume nothing to load */
	memset(sep_cache_load_required, 0, sizeof(sep_cache_load_required));

	/* set the number of active contexts */
	op_ctx->ctx_info_num = get_num_of_ctx_info(config);

	/* map each context in the configuration scheme */
	for (ctx_idx = 0; ctx_idx < op_ctx->ctx_info_num; ctx_idx++) {
		/* context already initialized by the user */
		rc = map_ctx_for_proc(
			op_ctx->client_ctx,
			&ctx_info_p[ctx_idx],
			config->node_props[ctx_idx].context, /*user ctx*/
			&ctx_state);
		if (rc != 0) {
			op_ctx->error_info = DXDI_ERROR_BAD_CTX;
			goto unmap_ctx_and_return;
		}

		ctx_mapped_n++; /*ctx mapped successfully*/

		/* context must be initialzed */
		if (ctx_state != CTX_STATE_INITIALIZED) {
			SEP_LOG_ERR("Given context [%d] in invalid state for"
				    " processing -%d\n", ctx_idx, ctx_state);
			op_ctx->error_info = DXDI_ERROR_BAD_CTX;
			rc = -EINVAL;
			goto unmap_ctx_and_return;
		}
	}


	op_ctx->op_type = SEP_OP_CRYPTO_PROC;

	/* Construct SeP combined scheme */
	cfg_scheme = format_sep_combined_cfg_scheme(config, op_ctx);
	SEP_LOG_DEBUG("SeP Config. Scheme: 0x%08X\n", cfg_scheme);

	rc = prepare_combined_data_for_sep(op_ctx, data_in, data_out,
				data_in_size, CRYPTO_DATA_TEXT);

	/* Start critical section -
	   cache allocation must be coupled to descriptor enqueue */
	rc = mutex_lock_interruptible(
		&drvdata->desc_queue_sequencer);
	if (rc == 0) { /* coupled sequence */
		for (ctx_idx = 0; ctx_idx < op_ctx->ctx_info_num; ctx_idx++) {
			ctxmgr_set_sep_cache_idx(
				&ctx_info_p[ctx_idx],
				ctxmgr_sep_cache_alloc(
					drvdata->sep_cache,
					ctxmgr_get_ctx_id(
					&ctx_info_p[ctx_idx]),
					&sep_cache_load_required[ctx_idx]));
		}

		rc = send_combined_op_desc(op_ctx,
			sep_cache_load_required, 0/*INIT*/,
			SEP_PROC_MODE_PROC_T, cfg_scheme);
		mutex_unlock(&drvdata->desc_queue_sequencer);
	} else {
		SEP_LOG_ERR("Failed locking descQ sequencer[%u]\n",
			op_ctx->client_ctx->qid);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
	}
	if (rc == 0)
		rc = wait_for_sep_op_result(op_ctx);

	crypto_op_completion_cleanup(op_ctx);

unmap_ctx_and_return:
	for (ctx_idx = 0; ctx_idx < ctx_mapped_n; ctx_idx++)
		ctxmgr_unmap_user_ctx(&ctx_info_p[ctx_idx]);

	return rc;
}

/**
 * sep_combined_fin_proc() - Finalize Combined processing with given (optional) data
 * @op_ctx:
 * @config:
 * @data_in:
 * @data_out:
 * @data_in_size:
 * @auth_data_p:
 * @auth_data_size_p:
 *
 * Returns int
 */
static int sep_combined_fin_proc(
	struct sep_op_ctx *op_ctx,
	struct dxdi_combined_props *config,
	uint8_t __user *data_in,
	uint8_t __user *data_out,
	uint32_t data_in_size,
	uint8_t *auth_data_p,
	uint8_t *auth_data_size_p)
{
	int rc;
	int ctx_idx, ctx_mapped_n = 0;
	int sep_cache_load_required[DXDI_COMBINED_NODES_MAX];
	enum host_ctx_state ctx_state;
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	struct client_crypto_ctx_info *ctx_info_p = &(op_ctx->ctx_info);
	uint32_t cfg_scheme;

	/* assume nothing to load */
	memset(sep_cache_load_required, 0, sizeof(sep_cache_load_required));

	/* set the number of active contexts */
	op_ctx->ctx_info_num = get_num_of_ctx_info(config);

	/* map each context in the configuration scheme */
	for (ctx_idx = 0; ctx_idx < op_ctx->ctx_info_num; ctx_idx++) {
		/* context already initialized by the user */
		rc = map_ctx_for_proc(
			op_ctx->client_ctx,
			&ctx_info_p[ctx_idx],
			config->node_props[ctx_idx].context, /*user ctx*/
			&ctx_state);
		if (rc != 0) {
			op_ctx->error_info = DXDI_ERROR_BAD_CTX;
			goto unmap_ctx_and_return;
		}

		ctx_mapped_n++; /*ctx mapped successfully*/

		/* context must be initialzed */
		if (ctx_state != CTX_STATE_INITIALIZED) {
			SEP_LOG_ERR("Given context [%d] in invalid state for"
				    " processing -%d\n", ctx_idx, ctx_state);
			rc = -EINVAL;
			op_ctx->error_info = DXDI_ERROR_BAD_CTX;
			goto unmap_ctx_and_return;
		}
	}

	op_ctx->op_type = SEP_OP_CRYPTO_FINI;

	/* Construct SeP combined scheme */
	cfg_scheme = format_sep_combined_cfg_scheme(config, op_ctx);
	SEP_LOG_DEBUG("SeP Config. Scheme: 0x%08X\n", cfg_scheme);

	rc = prepare_combined_data_for_sep(op_ctx, data_in, data_out,
		data_in_size, CRYPTO_DATA_TEXT_FINALIZE);
	if (rc != 0)
		goto unmap_ctx_and_return;

	/* Start critical section -
	   cache allocation must be coupled to descriptor enqueue */
	rc = mutex_lock_interruptible(
		&drvdata->desc_queue_sequencer);
	if (rc == 0) { /* Coupled sequence */
		for (ctx_idx = 0; ctx_idx < op_ctx->ctx_info_num; ctx_idx++) {
			ctxmgr_set_sep_cache_idx(
				&ctx_info_p[ctx_idx],
				ctxmgr_sep_cache_alloc(
					drvdata->sep_cache,
					ctxmgr_get_ctx_id(
					&ctx_info_p[ctx_idx]),
					&sep_cache_load_required[ctx_idx]));
		}
		rc = send_combined_op_desc(op_ctx,
			sep_cache_load_required, 0 /*INIT*/,
			SEP_PROC_MODE_FIN, cfg_scheme);

		mutex_unlock(&drvdata->desc_queue_sequencer);
	} else {
		SEP_LOG_ERR("Failed locking descQ sequencer[%u]\n",
			op_ctx->client_ctx->qid);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
	}
	if (rc == 0)
		rc = wait_for_sep_op_result(op_ctx);

	if (auth_data_size_p != NULL) {
		if ((rc == 0) && (op_ctx->error_info == 0)) {
			/* Auth data embedded in the last SeP/FW context */
			*auth_data_size_p = ctxmgr_get_digest_or_mac(
				&ctx_info_p[op_ctx->ctx_info_num - 1],
				auth_data_p);
		} else { /* Failure */
			*auth_data_size_p = 0; /* Nothing valid */
		}
	}

	crypto_op_completion_cleanup(op_ctx);

unmap_ctx_and_return:
	for (ctx_idx = 0; ctx_idx < ctx_mapped_n; ctx_idx++)
		ctxmgr_unmap_user_ctx(&ctx_info_p[ctx_idx]);

	return rc;
}

/**
 * process_combined_integrated() - Integrated processing of Combined data (init+proc+fin)
 * @op_ctx:
 * @props:	 Initialization properties (see init_context)
 * @data_in:
 * @data_out:
 * @data_in_size:
 * @auth_data_p:
 * @auth_data_size_p:
 *
 * Returns int
 */
static int process_combined_integrated(
	struct sep_op_ctx *op_ctx,
	struct dxdi_combined_props *config,
	uint8_t __user *data_in,
	uint8_t __user *data_out,
	uint32_t data_in_size,
	uint8_t *auth_data_p,
	uint8_t *auth_data_size_p)
{
	int rc;
	int ctx_idx, ctx_mapped_n = 0;
	int sep_cache_load_required[DXDI_COMBINED_NODES_MAX];
	enum host_ctx_state ctx_state;
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;
	struct client_crypto_ctx_info *ctx_info_p = &(op_ctx->ctx_info);
	uint32_t cfg_scheme;

	/* assume nothing to load */
	memset(sep_cache_load_required, 0, sizeof(sep_cache_load_required));

	/* set the number of active contexts */
	op_ctx->ctx_info_num = get_num_of_ctx_info(config);

	/* map each context in the configuration scheme */
	for (ctx_idx = 0; ctx_idx < op_ctx->ctx_info_num; ctx_idx++) {
		/* context already initialized by the user */
		rc = map_ctx_for_proc(op_ctx->client_ctx,
				&ctx_info_p[ctx_idx],
				config->node_props[ctx_idx].context,
				&ctx_state);
		if (rc != 0) {
			op_ctx->error_info = DXDI_ERROR_BAD_CTX;
			goto unmap_ctx_and_return;
		}

		ctx_mapped_n++; /*ctx mapped successfully*/

		if (ctx_state != CTX_STATE_INITIALIZED) {
			SEP_LOG_ERR("Given context [%d] in invalid state for"
				    " processing 0x%08X\n", ctx_idx, ctx_state);
			op_ctx->error_info = DXDI_ERROR_BAD_CTX;
			rc = -EINVAL;
			goto unmap_ctx_and_return;
		}
	}

	op_ctx->op_type = SEP_OP_CRYPTO_FINI;
	/* reconstruct combined scheme */
	cfg_scheme = format_sep_combined_cfg_scheme(config, op_ctx);
	SEP_LOG_DEBUG("SeP Config. Scheme: 0x%08X\n", cfg_scheme);

	rc = prepare_combined_data_for_sep(op_ctx, data_in, data_out,
			data_in_size, CRYPTO_DATA_TEXT_FINALIZE/*last*/);
	if (rc != 0)
		goto unmap_ctx_and_return;

	/* Start critical section -
	   cache allocation must be coupled to descriptor enqueue */
	rc = mutex_lock_interruptible(
		&drvdata->desc_queue_sequencer);
	if (rc == 0) { /* Coupled sequence */
		for (ctx_idx = 0; ctx_idx < op_ctx->ctx_info_num; ctx_idx++) {
			ctxmgr_set_sep_cache_idx(
				&ctx_info_p[ctx_idx],
				ctxmgr_sep_cache_alloc(
					drvdata->sep_cache,
					ctxmgr_get_ctx_id(&ctx_info_p[ctx_idx]),
					&sep_cache_load_required[ctx_idx]));
		}
		rc = send_combined_op_desc(op_ctx,
			sep_cache_load_required, 1/*INIT*/,
			SEP_PROC_MODE_FIN, cfg_scheme);

		mutex_unlock(&drvdata->desc_queue_sequencer);
	} else {
		SEP_LOG_ERR("Failed locking descQ sequencer[%u]\n",
			op_ctx->client_ctx->qid);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
	}
	if (rc == 0)
		rc = wait_for_sep_op_result(op_ctx);


	if (auth_data_size_p != NULL) {
		if ((rc == 0) && (op_ctx->error_info == 0)) {
			/* Auth data embedded in the last SeP/FW context */
			*auth_data_size_p = ctxmgr_get_digest_or_mac(
				&ctx_info_p[op_ctx->ctx_info_num - 1],
				auth_data_p);
		} else { /* Failure */
			*auth_data_size_p = 0; /* Nothing valid */
		}
	}

	crypto_op_completion_cleanup(op_ctx);

unmap_ctx_and_return:
	for (ctx_idx = 0; ctx_idx < ctx_mapped_n; ctx_idx++)
		ctxmgr_unmap_user_ctx(&ctx_info_p[ctx_idx]);

	return rc;
}

/**
 * process_integrated() - Integrated processing of data (init+proc+fin)
 * @op_ctx:
 * @context_buf:
 * @alg_class:
 * @props:	 Initialization properties (see init_context)
 * @data_in:
 * @data_out:
 * @data_in_size:
 * @digest_or_mac_p:
 * @digest_or_mac_size_p:
 *
 * Returns int
 */
static int process_integrated(
	struct sep_op_ctx *op_ctx,
	uint32_t __user *context_buf,
	enum crypto_alg_class alg_class,
	void *props,
	uint8_t __user *data_in,
	uint8_t __user *data_out,
	uint32_t data_in_size,
	uint8_t *digest_or_mac_p,
	uint8_t *digest_or_mac_size_p)
{
	int rc;
	int sep_cache_load_required;
	bool postpone_init;
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;

	rc = ctxmgr_map_user_ctx(
		&op_ctx->ctx_info, drvdata->sep_data->dev, alg_class,
		context_buf);
	if (rc != 0) {
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		return rc;
	}
	ctxmgr_set_ctx_state(&op_ctx->ctx_info, CTX_STATE_UNINITIALIZED);
	/* Allocate a new Crypto context ID */
	ctxmgr_set_ctx_id(&op_ctx->ctx_info,
		alloc_crypto_ctx_id(op_ctx->client_ctx));

	/* Algorithm class specific initialization */
	switch (alg_class) {
	case ALG_CLASS_SYM_CIPHER:
		rc = ctxmgr_init_symcipher_ctx(&op_ctx->ctx_info,
			(struct dxdi_sym_cipher_props *)props,
			&postpone_init, &op_ctx->error_info);
		/* postpone_init would be ignored because this is an integrated
		   operation - all required data would be updated in the
		   context before the descriptor is sent */
		break;
	case ALG_CLASS_AUTH_ENC:
		rc = ctxmgr_init_auth_enc_ctx(&op_ctx->ctx_info,
			(struct dxdi_auth_enc_props *)props,
			&op_ctx->error_info);
		break;
	case ALG_CLASS_MAC:
		rc = ctxmgr_init_mac_ctx(&op_ctx->ctx_info,
			(struct dxdi_mac_props *)props,
			&op_ctx->error_info);
		break;
	case ALG_CLASS_HASH:
		rc = ctxmgr_init_hash_ctx(&op_ctx->ctx_info,
			*((enum dxdi_hash_type *)props),
			&op_ctx->error_info);
		break;
	default:
		SEP_LOG_ERR("Invalid algorithm class %d\n", alg_class);
		op_ctx->error_info = DXDI_ERROR_UNSUP;
		rc = -EINVAL;
	}
	if (rc != 0)
		goto unmap_ctx_and_exit;

	ctxmgr_set_ctx_state(&op_ctx->ctx_info, CTX_STATE_PARTIAL_INIT);
	op_ctx->op_type = SEP_OP_CRYPTO_FINI; /* Integrated is also fin. */
	rc = prepare_data_for_sep(op_ctx, data_in, NULL, data_out, NULL,
		data_in_size, CRYPTO_DATA_TEXT_FINALIZE/*last*/);
	if (rc != 0)
		goto unmap_ctx_and_exit;

#ifdef DEBUG
	ctxmgr_dump_sep_ctx(&op_ctx->ctx_info);
#endif
	/* Flush sep_ctx out of host cache */
	ctxmgr_sync_sep_ctx(&op_ctx->ctx_info, drvdata->sep_data->dev);

	/* Start critical section -
	   cache allocation must be coupled to descriptor enqueue */
	rc = mutex_lock_interruptible(
		&drvdata->desc_queue_sequencer);
	if (rc == 0) {
		/* Allocate SeP context cache entry */
		ctxmgr_set_sep_cache_idx(&op_ctx->ctx_info,
			ctxmgr_sep_cache_alloc(
				drvdata->sep_cache,
				ctxmgr_get_ctx_id(&op_ctx->ctx_info),
				&sep_cache_load_required));
		if (!sep_cache_load_required)
			SEP_LOG_ERR("New context already in SeP cache?!");
		/* Send descriptor with combined load+init+fin */
		rc = send_crypto_op_desc(op_ctx,
					 1/*load*/, 1/*INIT*/,
					 SEP_PROC_MODE_FIN);
		mutex_unlock(&drvdata->desc_queue_sequencer);

	} else { /* failed acquiring mutex */
		SEP_LOG_ERR("Failed locking descQ sequencer[%u]\n",
			op_ctx->client_ctx->qid);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
	}

	if (rc == 0)
		rc = wait_for_sep_op_result(op_ctx);

	if (digest_or_mac_size_p != NULL) {
		if ((rc == 0) && (op_ctx->error_info == 0)) {
			/* Digest or MAC are embedded in the SeP/FW context */
			*digest_or_mac_size_p = ctxmgr_get_digest_or_mac(
				&op_ctx->ctx_info, digest_or_mac_p);
		} else { /* Failure */
			*digest_or_mac_size_p = 0; /* Nothing valid */
		}
	}

	/* Hash tail buffer is never used/mapped in integrated op -->
	   no need to unmap*/

	crypto_op_completion_cleanup(op_ctx);

unmap_ctx_and_exit:
	ctxmgr_unmap_user_ctx(&op_ctx->ctx_info);

	return rc;
}

/**
 * process_integrated_auth_enc() - Integrated processing of aead
 * @op_ctx:
 * @context_buf:
 * @alg_class:
 * @props:
 * @data_header:
 * @data_in:
 * @data_out:
 * @data_header_size:
 * @data_in_size:
 * @mac_p:
 * @mac_size_p:
 *
 * Integrated processing of authenticate & encryption of data
 * (init+proc_a+proc_t+fin)
 * Returns int
 */
static int process_integrated_auth_enc(
	struct sep_op_ctx *op_ctx,
	uint32_t __user *context_buf,
	enum crypto_alg_class alg_class,
	void *props,
	uint8_t __user *data_header,
	uint8_t __user *data_in,
	uint8_t __user *data_out,
	uint32_t data_header_size,
	uint32_t data_in_size,
	uint8_t *mac_p,
	uint8_t *mac_size_p)
{
	int rc;
	int sep_cache_load_required;
	struct queue_drvdata *drvdata = op_ctx->client_ctx->drv_data;

	rc = ctxmgr_map_user_ctx(
		&op_ctx->ctx_info, drvdata->sep_data->dev, alg_class,
		context_buf);
	if (rc != 0) {
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		goto integ_ae_exit;
	}

	ctxmgr_set_ctx_state(&op_ctx->ctx_info, CTX_STATE_UNINITIALIZED);
	/* Allocate a new Crypto context ID */
	ctxmgr_set_ctx_id(&op_ctx->ctx_info,
		alloc_crypto_ctx_id(op_ctx->client_ctx));

	/* initialization */
	rc = ctxmgr_init_auth_enc_ctx(&op_ctx->ctx_info,
		(struct dxdi_auth_enc_props *)props, &op_ctx->error_info);
	if (rc != 0) {
		ctxmgr_unmap_user_ctx(&op_ctx->ctx_info);
		op_ctx->error_info = DXDI_ERROR_BAD_CTX;
		goto integ_ae_exit;
	}

	ctxmgr_set_ctx_state(&op_ctx->ctx_info, CTX_STATE_PARTIAL_INIT);
	/* Op. type is to init. the context and process Adata */
	op_ctx->op_type = SEP_OP_CRYPTO_INIT | SEP_OP_CRYPTO_PROC;
	/* prepare additional/assoc data */
	rc = prepare_data_for_sep(op_ctx, data_header, NULL, NULL, NULL,
		data_header_size, CRYPTO_DATA_ADATA);
	if (rc != 0) {
		ctxmgr_unmap_user_ctx(&op_ctx->ctx_info);
		goto integ_ae_exit;
	}

#ifdef DEBUG
	ctxmgr_dump_sep_ctx(&op_ctx->ctx_info);
#endif
	/* Flush sep_ctx out of host cache */
	ctxmgr_sync_sep_ctx(&op_ctx->ctx_info, drvdata->sep_data->dev);

	/* Start critical section -
	   cache allocation must be coupled to descriptor enqueue */
	rc = mutex_lock_interruptible(
		&drvdata->desc_queue_sequencer);
	if (rc == 0) {
		/* Allocate SeP context cache entry */
		ctxmgr_set_sep_cache_idx(&op_ctx->ctx_info,
			ctxmgr_sep_cache_alloc(
				drvdata->sep_cache,
				ctxmgr_get_ctx_id(&op_ctx->ctx_info),
				&sep_cache_load_required));
		if (!sep_cache_load_required)
			SEP_LOG_ERR("New context already in SeP cache?!");
		/* Send descriptor with combined load+init+fin */
		rc = send_crypto_op_desc(op_ctx,
					 1/*load*/, 1/*INIT*/,
					 SEP_PROC_MODE_PROC_A);
		mutex_unlock(&drvdata->desc_queue_sequencer);

	} else { /* failed acquiring mutex */
		SEP_LOG_ERR("Failed locking descQ sequencer[%u]\n",
			op_ctx->client_ctx->qid);
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;
	}

	/* set status and cleanup last descriptor */
	if (rc == 0)
		rc = wait_for_sep_op_result(op_ctx);
	crypto_op_completion_cleanup(op_ctx);
	ctxmgr_unmap_user_ctx(&op_ctx->ctx_info);

	/* process text data only on adata success */
	if ((rc == 0) && (op_ctx->error_info == 0)) { /* Init+Adata succeeded */
		/* reset pending descriptor and preserve operation
		context for the finalize phase */
		op_ctx->pending_descs_cntr = 1;
		/* process & finalize operation with entire user data */
		rc = sep_fin_proc(op_ctx, context_buf, data_in,
			data_out, data_in_size,
			mac_p, mac_size_p);
	}

integ_ae_exit:
	return rc;
}


/**
 * dxdi_data_dir_to_dma_data_dir() - Convert from DxDI DMA direction type to
 *					Linux kernel DMA direction type
 * @dxdi_dir:	 DMA direction in DxDI encoding
 *
 * Returns enum dma_data_direction
 */
enum dma_data_direction dxdi_data_dir_to_dma_data_dir(
	enum dxdi_data_direction dxdi_dir)
{
	switch (dxdi_dir) {
	case DXDI_DATA_BIDIR: return DMA_BIDIRECTIONAL;
	case DXDI_DATA_TO_DEVICE: return DMA_TO_DEVICE;
	case DXDI_DATA_FROM_DEVICE: return DMA_FROM_DEVICE;
	default: return DMA_NONE;
	}
}

/**
 * dispatch_sep_rpc() - Dispatch a SeP RPC descriptor and process results
 * @op_ctx:
 * @agent_id:
 * @func_id:
 * @mem_refs:
 * @rpc_params_size:
 * @rpc_params:
 *
 * Returns int
 */
static int dispatch_sep_rpc(
	struct sep_op_ctx *op_ctx,
	seprpc_agent_id_t agent_id,
	seprpc_func_id_t func_id,
	struct dxdi_memref mem_refs[],
	unsigned long rpc_params_size,
	struct seprpc_params __user *rpc_params)
{
	int i, rc = 0;
	struct sep_client_ctx *client_ctx = op_ctx->client_ctx;
	struct queue_drvdata *drvdata = client_ctx->drv_data;
	enum dma_data_direction dma_dir;
	unsigned int num_of_mem_refs;
	dxdi_memref_id_t memref_idx;
	struct client_dma_buffer *local_dma_objs[SEP_RPC_MAX_MEMREF_PER_FUNC];
	struct mlli_tables_list mlli_tables[SEP_RPC_MAX_MEMREF_PER_FUNC];
	struct sep_sw_desc desc;
	struct seprpc_params *rpc_msg_p;

	/* Verify RPC message size */
	if (unlikely(SEP_RPC_MAX_MSG_SIZE < rpc_params_size)) {
		SEP_LOG_ERR("Given rpc_params is too big (%lu B)\n",
			rpc_params_size);
		return -EINVAL;
	}

	op_ctx->spad_buf_p = dma_pool_alloc(drvdata->sep_data->spad_buf_pool,
		GFP_KERNEL, &op_ctx->spad_buf_dma_addr);
	if (unlikely(op_ctx->spad_buf_p == NULL)) {
		SEP_LOG_ERR("Failed allocating from spad_buf_pool for "
			    "RPC message\n");
		return -ENOMEM;
	}
	rpc_msg_p = (struct seprpc_params *)op_ctx->spad_buf_p;

	/* Copy params to DMA buffer of message */
	rc = copy_from_user(rpc_msg_p, rpc_params, rpc_params_size);
	if (rc) {
		SEP_LOG_ERR("Failed copying RPC parameters/message from user "
			    "at 0x%p (rc=%d)\n", rpc_params, rc);
		return -EFAULT;
	}
	/* Get num. of memory references in host endianess */
	num_of_mem_refs = le32_to_cpu(rpc_msg_p->num_of_memrefs);

	/* Handle user memory references - prepare DMA buffers */
	if (unlikely(num_of_mem_refs > SEP_RPC_MAX_MEMREF_PER_FUNC)) {
		SEP_LOG_ERR("agent_id=%d func_id=%d: Invalid number of "
			    "memref %u\n", agent_id, func_id, num_of_mem_refs);
		return -EINVAL;
	}
	for (i = 0; i < num_of_mem_refs; i++) {
		/* Init. tables lists for proper cleanup */
		MLLI_TABLES_LIST_INIT(mlli_tables + i);
		local_dma_objs[i] = NULL;
	}
	for (i = 0; i < num_of_mem_refs; i++) {
		SEP_LOG_DEBUG("memref[%d]: id=%d dma_dir=%d "
			      "start/offset 0x%08lX size %lu\n",
			i, mem_refs[i].ref_id, mem_refs[i].dma_direction,
			mem_refs[i].start_or_offset, mem_refs[i].size);

		/* convert DMA direction to enum dma_data_direction */
		dma_dir = dxdi_data_dir_to_dma_data_dir(
			mem_refs[i].dma_direction);
		if (unlikely(dma_dir == DMA_NONE)) {
			SEP_LOG_ERR("agent_id=%d func_id=%d: Invalid DMA "
				    "direction (%d) for memref %d\n",
				agent_id, func_id,
				mem_refs[i].dma_direction, i);
			rc = -EINVAL;
			break;
		}
		/* Temporary memory registration if needed */
		if (IS_VALID_MEMREF_IDX(mem_refs[i].ref_id)) {
			memref_idx = mem_refs[i].ref_id;
			if (unlikely(mem_refs[i].start_or_offset != 0)) {
				SEP_LOG_ERR("Offset in memref is not supported "
					    "for RPC.\n");
				rc = -EINVAL;
				break;
			}
		} else {
			memref_idx = register_client_memref(client_ctx,
				(uint8_t __user *)mem_refs[i].start_or_offset,
				NULL, mem_refs[i].size, dma_dir);
			if (unlikely(!IS_VALID_MEMREF_IDX(memref_idx))) {
				SEP_LOG_ERR("Failed temp. memory "
					    "registration\n");
				rc = -ENOMEM;
				break;
			}
		}
		/* MLLI table creation */
		local_dma_objs[i] =
			acquire_dma_obj(client_ctx, memref_idx);
		if (unlikely(local_dma_objs[i] == NULL)) {
			SEP_LOG_ERR("Failed acquiring DMA objects.\n");
			rc = -ENOMEM;
			break;
		}
		if (unlikely(local_dma_objs[i]->buf_size != mem_refs[i].size)) {
			SEP_LOG_ERR("Partial memory reference is not supported "
				"for RPC.\n");
			rc = -EINVAL;
			break;
		}
		rc = llimgr_create_mlli(drvdata->sep_data->llimgr,
			mlli_tables + i, dma_dir, local_dma_objs[i], 0, 0);
		if (unlikely(rc != 0))
			break;
		llimgr_mlli_to_seprpc_memref(
			&(mlli_tables[i]), &(rpc_msg_p->memref[i]));
	}

	op_ctx->op_type = SEP_OP_RPC;
	if (rc == 0) {
		/* Pack SW descriptor */
		desc_q_pack_rpc_desc(&desc, op_ctx, agent_id, func_id,
			rpc_params_size, op_ctx->spad_buf_dma_addr);
		op_ctx->op_state = USER_OP_INPROC;
		/* Enqueue descriptor */
		rc = desc_q_enqueue(drvdata->desc_queue, &desc, true);
		if (likely(!IS_DESCQ_ENQUEUE_ERR(rc)))
			rc = 0;
	}

	if (likely(rc == 0))
		rc = wait_for_sep_op_result(op_ctx);
	else
		op_ctx->error_info = DXDI_ERROR_NO_RESOURCE;

	/* Process descriptor completion */
	if ((rc == 0) && (op_ctx->error_info == 0)) {
		/* Copy back RPC message buffer */
		rc = copy_to_user(rpc_params, rpc_msg_p, rpc_params_size);
		if (rc) {
			SEP_LOG_ERR("Failed copying back RPC parameters/message"
				    " to user at 0x%p (rc=%d)\n",
				rpc_params, rc);
			rc = -EFAULT;
		}
	}
	op_ctx->op_state = USER_OP_NOP;
	for (i = 0; i < num_of_mem_refs; i++) {
		/* Can call for all - unitialized mlli tables would have
		   table_count == 0 */
		llimgr_destroy_mlli(drvdata->sep_data->llimgr, mlli_tables + i);
		if (local_dma_objs[i] != NULL) {
			release_dma_obj(client_ctx, local_dma_objs[i]);
			memref_idx = DMA_OBJ_TO_MEMREF_IDX(
				client_ctx, local_dma_objs[i]);
			if (memref_idx != mem_refs[i].ref_id) {
				/* Memory reference was temp. registered */
				(void)free_client_memref(client_ctx,
					memref_idx);
			}
		}
	}

	return rc;
}


#if defined(SEP_PRINTF)
/* Replace component mask */
#undef SEP_LOG_CUR_COMPONENT
#define SEP_LOG_CUR_COMPONENT SEP_LOG_MASK_SEP_PRINTF
void sep_printf_handler(struct sep_drvdata *drvdata,
	int cause_id, void *priv_context)
{
#ifdef DEBUG
	int cur_ack_cntr;
	u32 gpr_val;
	int i;

	/* Reduce interrupts by polling until no more characters */
	/* Loop for at most a line - to avoid inifite looping in wq ctx */
	for (i = 0; i < SEP_PRINTF_LINE_SIZE; i++) {

		gpr_val = READ_REGISTER(drvdata->cc_base +
			SEP_PRINTF_S2H_GPR_OFFSET);
		cur_ack_cntr = gpr_val >> 8;
		/* ack as soon as possible (data is already in local variable)*/
		/* let SeP push one more character until we finish processing */
		WRITE_REGISTER(drvdata->cc_base + SEP_PRINTF_H2S_GPR_OFFSET,
			cur_ack_cntr);
#if 0
		SEP_LOG_DEBUG("%d. GPR=0x%08X (cur_ack=0x%08X , last=0x%08X)\n",
			i, gpr_val, cur_ack_cntr, drvdata->last_ack_cntr);
#endif
		if (cur_ack_cntr == drvdata->last_ack_cntr)
			break;

		/* Identify lost characters case */
		if (cur_ack_cntr >
		    ((drvdata->last_ack_cntr + 1) & SEP_PRINTF_ACK_MAX)) {
			/* NULL terminate */
			drvdata->line_buf[drvdata->cur_line_buf_offset] = 0;
			if (sep_log_mask & SEP_LOG_CUR_COMPONENT)
				printk(KERN_INFO "SeP(lost %d): %s",
				       cur_ack_cntr - drvdata->last_ack_cntr
				       - 1, drvdata->line_buf);
			drvdata->cur_line_buf_offset = 0;
		}
		drvdata->last_ack_cntr = cur_ack_cntr;

		drvdata->line_buf[drvdata->cur_line_buf_offset] =
			gpr_val & 0xFF;

		/* Is end of line? */
		if ((drvdata->line_buf[drvdata->cur_line_buf_offset] == '\n') ||
		    (drvdata->line_buf[drvdata->cur_line_buf_offset] == 0) ||
		    (drvdata->cur_line_buf_offset == (SEP_PRINTF_LINE_SIZE - 1))
			) {
			/* NULL terminate */
			drvdata->line_buf[drvdata->cur_line_buf_offset + 1] = 0;
			if (sep_log_mask & SEP_LOG_CUR_COMPONENT)
				printk(KERN_INFO "SeP: %s", drvdata->line_buf);
			drvdata->cur_line_buf_offset = 0;
		} else {
			drvdata->cur_line_buf_offset++;
		}

	}

#else /* Just ack to SeP so it does not stall */
	WRITE_REGISTER(drvdata->cc_base + SEP_PRINTF_H2S_GPR_OFFSET,
		READ_REGISTER(drvdata->cc_base +
			SEP_PRINTF_S2H_GPR_OFFSET) >> 8);
#endif /*DEBUG*/
}
/* Restore component mask */
#undef SEP_LOG_CUR_COMPONENT
#define SEP_LOG_CUR_COMPONENT SEP_LOG_MASK_MAIN
#endif /*SEP_PRINTF*/


/*** Interrupts handling infrastructure ***/

/**
 * cc_irr_process() - Process interrupt requests from CC::IRR
 *
 * @drvdata:	Driver context
 *
 * This function is the hook to the ISR or timer handlers.
 * It checks IRR for interrupt causes and invokes the registered handlers
 */
static int cc_irr_process(struct sep_drvdata *drvdata)
{
	uint32_t cause_reg;
	uint32_t cur_cause_mask;
	int cause_id;
	struct cc_interrupt_dispatch_entry *irqe;

	/* read the interrupt status */
	cause_reg = READ_REGISTER(drvdata->cc_base +
		DX_CC_REG_OFFSET(HOST, IRR));
	/* Clear masked interrupts - these causes should be ignored
	   (they did not generate this interrupt and have no handler) */
	cause_reg &= ~drvdata->irq_mask;
	if (cause_reg == 0) { /* Probably shared interrupt line */
		SEP_LOG_DEBUG("Got interrupt with no cause\n");
		return IRQ_NONE;
	}
	/* clear interrupt - must be before calling handlers */
	WRITE_REGISTER(drvdata->cc_base + DX_CC_REG_OFFSET(HOST, ICR),
		cause_reg);
	/* Scan IRR and invoke registered handlers */
	while (cause_reg != 0) {
		cause_id = ffs(cause_reg) - 1; /* ffs() return 1 for first bit*/
#if defined(DEBUG) && (CC_CAUSE_ID_MAX < 31)
		/* Verify that we did not unmask IRR bit beyond size of
		   irq_handlers lookup table */
		if (cause_id > CC_CAUSE_ID_MAX) {
			/* Protect irq_handlers table */
			SEP_LOG_ERR("Invalid IRR bit %d set\n", cause_id);
			break;
		}
#endif
		cur_cause_mask = 1 << cause_id;
		irqe = drvdata->irq_handlers + cause_id;
		if (irqe->handler != NULL)
			irqe->handler(drvdata, cause_id,
				irqe->priv_context);
		else
			SEP_LOG_ERR("NULL handler for cause_id=%d)\n",
				cause_id);
		cause_reg &= ~cur_cause_mask; /* Clear handled cause */
	}

	return IRQ_HANDLED;
}

#ifdef CC_INTERRUPT_BY_TIMER
static void cc_timer(unsigned long arg)
{
	struct sep_drvdata *drvdata = (struct sep_drvdata *)arg;

	(void)cc_irr_process(drvdata);

	mod_timer(&drvdata->delegate, jiffies + msecs_to_jiffies(10));
}
#else
irqreturn_t cc_isr(int irq, void *dev_id)
{
	struct sep_drvdata *drvdata =
	    (struct sep_drvdata *)dev_get_drvdata((struct device *)dev_id);

	return cc_irr_process(drvdata);
}
#endif

/**
 * init_cc_interrupts() - Initialize CC interrupts resources
 *
 * @drvdata:	Device driver context
 * @r_irq:	IRQ resource
 */
int init_cc_interrupts(struct sep_drvdata *drvdata, struct resource *r_irq)
{
	int rc;

	/* Initialize handlers dispatch table */
	memset(drvdata->irq_handlers, 0, sizeof(drvdata->irq_handlers));

	/* Interrupt handler setup */
	/* Mask all interrupts before registering IRQ handler */
	drvdata->irq_mask = 0xFFFFFFFFUL;
	WRITE_REGISTER(drvdata->cc_base + DX_CC_REG_OFFSET(HOST, IMR),
		0xFFFFFFFFUL);
#ifdef CC_INTERRUPT_BY_TIMER
	init_timer(&drvdata->delegate);
	drvdata->delegate.function = sep_timer;
	drvdata->delegate.data = (unsigned long)drvdata;
	mod_timer(&drvdata->delegate, jiffies);

#else /* IRQ handler setup */
	drvdata->irq = r_irq->start;
	rc = request_irq(drvdata->irq, cc_isr,
			 IRQF_SHARED, DRIVER_NAME, drvdata->dev);
	if (unlikely(rc != 0)) {
		SEP_LOG_ERR("Could not allocate interrupt %d\n",
			drvdata->irq);
		return rc;
	}
	SEP_LOG_INFO("%s at 0x%08X mapped to interrupt %d\n",
		 DRIVER_NAME, (unsigned int)drvdata->cc_base,
		 drvdata->irq);
//	SEP_LOG_INFO("irq address: 0x%08X \n", (unsigned int)drvdata->mem_start + SES_DXPE_IRQ_ADDR);
#endif /*CC_INTERRUPT_BY_TIMER?*/

	return 0;
}

/**
 * detach_cc_interrupts() - Cleanup CC interrupts resources
 *
 * @drvdata:	Driver context
 */
void detach_cc_interrupts(struct sep_drvdata *drvdata)
{
#ifdef CC_INTERRUPT_BY_TIMER
	del_timer_sync(&drvdata->delegate);
#else
	free_irq(drvdata->irq, drvdata->dev);
#endif
	drvdata->irq_mask = 0xFFFFFFFFUL;
	memset(drvdata->irq_handlers, 0, sizeof(drvdata->irq_handlers));
}

/**
 * register_cc_interrupt_handler() - Register handler for specific interrupt
 *	cause.
 *
 * @drvdata:	Driver context
 * @cause_id:	CC cause ID (index of cause bit)
 * @handler:	Function pointer to handler function
 * @priv_context:	Private context info. for interrupt processor
 */
int register_cc_interrupt_handler(
	struct sep_drvdata *drvdata, int cause_id,
	cc_interrupt_handler handler, void *priv_context)
{
	struct cc_interrupt_dispatch_entry *irqe;

	if (cause_id > CC_CAUSE_ID_MAX) {
		SEP_LOG_ERR("Cause ID out of range %d > %d\n",
			cause_id, CC_CAUSE_ID_MAX);
		return -EINVAL;
	}
	if (handler == NULL) {
		SEP_LOG_ERR("Invalid NULL handler\n");
		return -EINVAL;
	}
	irqe = drvdata->irq_handlers + cause_id;
	/* Add handler to dispatch table */
	if (irqe->handler != NULL) {
		SEP_LOG_ERR("Cause ID %d is already occupied\n", cause_id);
		return -EBUSY;
	}
	/* Add to dispatch table */
	irqe->handler = handler;
	irqe->priv_context = priv_context;
	/* Clear any pending interrupt (old event) */
	WRITE_REGISTER(drvdata->cc_base + DX_CC_REG_OFFSET(HOST, ICR),
		(1 << cause_id));
	/* Unmask interrupt cause */
	drvdata->irq_mask &= ~(1 << cause_id);
	WRITE_REGISTER(drvdata->cc_base + DX_CC_REG_OFFSET(HOST, IMR),
		drvdata->irq_mask);
	return 0;
}

/**
 * unregister_cc_interrupt_handler() - Revert register_cc_interrupt_handler()
 *
 * @drvdata:	Driver context
 * @cause_id:	CC cause ID (index of cause bit)
 */
void unregister_cc_interrupt_handler(struct sep_drvdata *drvdata, int cause_id)
{
	struct cc_interrupt_dispatch_entry *irqe;

	if (cause_id > CC_CAUSE_ID_MAX) {
		SEP_LOG_ERR("Cause ID out of range %d > %d\n",
			cause_id, CC_CAUSE_ID_MAX);
		return;
	}
	irqe = drvdata->irq_handlers + cause_id;
	/* Add handler to dispatch table */
	if (irqe->handler == NULL) {
		SEP_LOG_WARN("Cause ID %d has no handler\n", cause_id);
		return;
	}
	/* Mask interrupt cause */
	drvdata->irq_mask |= (1 << cause_id);
	WRITE_REGISTER(drvdata->cc_base + DX_CC_REG_OFFSET(HOST, IMR),
		drvdata->irq_mask);
	/* Clear from dispatch table */
	irqe->handler = NULL;
	irqe->priv_context = NULL;
}

/***** IOCTL commands handlers *****/

static int sep_ioctl_get_ver_major(unsigned long arg)
{
	uint32_t __user *ver_p = (uint32_t __user *)arg;
	const uint32_t ver_major = DXDI_VER_MAJOR;

	return __put_user(ver_major, ver_p);
}

static int sep_ioctl_get_ver_minor(unsigned long arg)
{
	uint32_t __user *ver_p = (uint32_t __user *)arg;
	const uint32_t ver_minor = DXDI_VER_MINOR;

	return __put_user(ver_minor, ver_p);
}

static int sep_ioctl_get_sym_cipher_ctx_size(unsigned long arg)
{
	struct dxdi_get_sym_cipher_ctx_size_params __user *user_params =
		(struct dxdi_get_sym_cipher_ctx_size_params __user *)arg;
	enum dxdi_sym_cipher_type sym_cipher_type;
	const uint32_t ctx_size = ctxmgr_get_ctx_size(ALG_CLASS_SYM_CIPHER);
	int err;

	err = __get_user(sym_cipher_type, &(user_params->sym_cipher_type));
	if (err)
		return err;

	if (
	    ((sym_cipher_type >= _DXDI_SYMCIPHER_AES_FIRST) &&
	     (sym_cipher_type <= _DXDI_SYMCIPHER_AES_LAST))   ||
	    ((sym_cipher_type >= _DXDI_SYMCIPHER_DES_FIRST) &&
	     (sym_cipher_type <= _DXDI_SYMCIPHER_DES_LAST))   ||
	    ((sym_cipher_type >= _DXDI_SYMCIPHER_C2_FIRST) &&
	     (sym_cipher_type <= _DXDI_SYMCIPHER_C2_LAST))
	   ) {
		SEP_LOG_DEBUG("sym_cipher_type=%u\n", sym_cipher_type);
		return __put_user(ctx_size, &(user_params->ctx_size));
	} else {
		SEP_LOG_ERR("Invalid cipher type=%u\n", sym_cipher_type);
		return -EINVAL;
	}
}

static int sep_ioctl_get_auth_enc_ctx_size(unsigned long arg)
{
	struct dxdi_get_auth_enc_ctx_size_params __user *user_params =
		(struct dxdi_get_auth_enc_ctx_size_params __user *)arg;
	enum dxdi_auth_enc_type ae_type;
	const uint32_t ctx_size = ctxmgr_get_ctx_size(ALG_CLASS_AUTH_ENC);
	int err;

	err = __get_user(ae_type, &(user_params->ae_type));
	if (err)
		return err;

	if ((ae_type == DXDI_AUTHENC_NONE) || (ae_type > DXDI_AUTHENC_MAX)) {
		SEP_LOG_ERR("Invalid auth-enc. type=%u\n", ae_type);
		return -EINVAL;
	}

	SEP_LOG_DEBUG("A.E. type=%u\n", ae_type);
	return __put_user(ctx_size, &(user_params->ctx_size));
}

static int sep_ioctl_get_mac_ctx_size(unsigned long arg)
{
	struct dxdi_get_mac_ctx_size_params __user *user_params =
		(struct dxdi_get_mac_ctx_size_params __user *)arg;
	enum dxdi_mac_type mac_type;
	const uint32_t ctx_size = ctxmgr_get_ctx_size(ALG_CLASS_MAC);
	int err;

	err = __get_user(mac_type, &(user_params->mac_type));
	if (err)
		return err;

	if ((mac_type == DXDI_MAC_NONE) || (mac_type > DXDI_MAC_MAX)) {
		SEP_LOG_ERR("Invalid MAC type=%u\n", mac_type);
		return -EINVAL;
	}

	SEP_LOG_DEBUG("MAC type=%u\n", mac_type);
	return __put_user(ctx_size, &(user_params->ctx_size));
}

static int sep_ioctl_get_hash_ctx_size(unsigned long arg)
{
	struct dxdi_get_hash_ctx_size_params __user *user_params =
		(struct dxdi_get_hash_ctx_size_params __user *)arg;
	enum dxdi_hash_type hash_type;
	const uint32_t ctx_size = ctxmgr_get_ctx_size(ALG_CLASS_HASH);
	int err;

	err = __get_user(hash_type, &(user_params->hash_type));
	if (err)
		return err;

	if ((hash_type == DXDI_HASH_NONE) || (hash_type > DXDI_HASH_MAX)) {
		SEP_LOG_ERR("Invalid hash type=%u\n", hash_type);
		return -EINVAL;
	}

	SEP_LOG_DEBUG("hash type=%u\n", hash_type);
	return __put_user(ctx_size, &(user_params->ctx_size));
}

static int sep_ioctl_sym_cipher_init(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{
	struct dxdi_sym_cipher_init_params __user *user_init_params =
		(struct dxdi_sym_cipher_init_params __user *)arg;
	struct dxdi_sym_cipher_init_params init_params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
		offsetof(struct dxdi_sym_cipher_init_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&init_params, user_init_params, input_size)) {
		SEP_LOG_ERR("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = init_crypto_context(&op_ctx, init_params.context_buf,
		ALG_CLASS_SYM_CIPHER, &(init_params.props));
	/* Even on SeP error the function above
	   returns 0 (operation completed with no host side errors) */
	__put_user(op_ctx.error_info, &(user_init_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_auth_enc_init(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{
	struct dxdi_auth_enc_init_params __user *user_init_params =
		(struct dxdi_auth_enc_init_params __user *)arg;
	struct dxdi_auth_enc_init_params init_params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
		offsetof(struct dxdi_sym_cipher_init_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&init_params, user_init_params, input_size)) {
		SEP_LOG_ERR("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = init_crypto_context(&op_ctx, init_params.context_buf,
		ALG_CLASS_AUTH_ENC, &(init_params.props));
	/* Even on SeP error the function above
	   returns 0 (operation completed with no host side errors) */
	__put_user(op_ctx.error_info, &(user_init_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_mac_init(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{
	struct dxdi_mac_init_params __user *user_init_params =
		(struct dxdi_mac_init_params __user *)arg;
	struct dxdi_mac_init_params init_params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
		offsetof(struct dxdi_mac_init_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&init_params, user_init_params, input_size)) {
		SEP_LOG_ERR("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = init_crypto_context(&op_ctx, init_params.context_buf,
		ALG_CLASS_MAC, &(init_params.props));
	/* Even on SeP error the function above
	   returns 0 (operation completed with no host side errors) */
	__put_user(op_ctx.error_info, &(user_init_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_hash_init(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{
	struct dxdi_hash_init_params __user *user_init_params =
		(struct dxdi_hash_init_params __user *)arg;
	struct dxdi_hash_init_params init_params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
		offsetof(struct dxdi_hash_init_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&init_params, user_init_params, input_size)) {
		SEP_LOG_ERR("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = init_crypto_context(&op_ctx, init_params.context_buf,
		ALG_CLASS_HASH, &(init_params.hash_type));
	/* Even on SeP error the function above
	   returns 0 (operation completed with no host side errors) */
	__put_user(op_ctx.error_info, &(user_init_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_proc_dblk(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{
	struct dxdi_process_dblk_params __user *user_dblk_params =
		(struct dxdi_process_dblk_params __user *)arg;
	struct dxdi_process_dblk_params dblk_params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
		offsetof(struct dxdi_process_dblk_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&dblk_params, user_dblk_params, input_size)) {
		SEP_LOG_ERR("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = sep_proc_dblk(&op_ctx, dblk_params.context_buf,
		dblk_params.data_block_type,
		dblk_params.data_in, dblk_params.data_out,
		dblk_params.data_in_size);
	/* Even on SeP error the function above
	   returns 0 (operation completed with no host side errors) */
	__put_user(op_ctx.error_info, &(user_dblk_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_fin_proc(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{
	struct dxdi_fin_process_params __user *user_fin_params =
		(struct dxdi_fin_process_params __user *)arg;
	struct dxdi_fin_process_params fin_params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
		offsetof(struct dxdi_fin_process_params, digest_or_mac);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&fin_params, user_fin_params, input_size)) {
		SEP_LOG_ERR("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = sep_fin_proc(&op_ctx, fin_params.context_buf,
		fin_params.data_in, fin_params.data_out,
		fin_params.data_in_size,
		fin_params.digest_or_mac, &(fin_params.digest_or_mac_size));
	/* Even on SeP error the function above
	   returns 0 (operation completed with no host side errors) */
	if (rc == 0) {
		/* Always copy back digest/mac size + error_info */
		/* (that's the reason for keeping them together)  */
		rc = __copy_to_user(&(user_fin_params->digest_or_mac_size),
			&(fin_params.digest_or_mac_size),
			sizeof(struct dxdi_fin_process_params) -
			offsetof(struct dxdi_fin_process_params,
					digest_or_mac_size));
		/* We always need to copy back the digest/mac size (even if 0)
		   in order to indicate validity of digest_or_mac buffer */
	}
	if ((rc == 0) && (op_ctx.error_info == 0) &&
	    (fin_params.digest_or_mac_size > 0)) {
		if (likely(fin_params.digest_or_mac_size <=
			DXDI_DIGEST_SIZE_MAX)) {
			/* Copy back digest/mac if valid */
			rc = __copy_to_user(&(user_fin_params->digest_or_mac),
				fin_params.digest_or_mac,
				fin_params.digest_or_mac_size);
		} else { /* Invalid digest/mac size! */
			SEP_LOG_ERR("Got invalid digest/MAC size = %u",
				fin_params.digest_or_mac_size);
			op_ctx.error_info = DXDI_ERROR_INVAL_DATA_SIZE;
			rc = -EBUG;
		}
	}

	/* Even on SeP error the function above
	   returns 0 (operation completed with no host side errors) */
	__put_user(op_ctx.error_info, &(user_fin_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_combined_init(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{
	struct dxdi_combined_init_params __user *user_init_params =
		(struct dxdi_combined_init_params __user *)arg;
	struct dxdi_combined_init_params init_params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
		offsetof(struct dxdi_combined_init_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&init_params, user_init_params, input_size)) {
		SEP_LOG_ERR("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = init_combined_context(&op_ctx,
			&(init_params.props));
	/* Even on SeP error the function above
	   returns 0 (operation completed with no host side errors) */
	__put_user(op_ctx.error_info, &(user_init_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_combined_proc_dblk(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{
	struct dxdi_combined_proc_dblk_params __user *user_dblk_params =
		(struct dxdi_combined_proc_dblk_params __user *)arg;
	struct dxdi_combined_proc_dblk_params dblk_params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
		offsetof(struct dxdi_combined_proc_dblk_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&dblk_params, user_dblk_params, input_size)) {
		SEP_LOG_ERR("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = sep_combined_proc_dblk(&op_ctx, &dblk_params.props,
		dblk_params.data_in, dblk_params.data_out,
		dblk_params.data_in_size);
	/* Even on SeP error the function above
	   returns 0 (operation completed with no host side errors) */
	__put_user(op_ctx.error_info, &(user_dblk_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_combined_fin_proc(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{
	struct dxdi_combined_proc_params __user *user_fin_params =
		(struct dxdi_combined_proc_params __user *)arg;
	struct dxdi_combined_proc_params fin_params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
		offsetof(struct dxdi_combined_proc_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&fin_params, user_fin_params, input_size)) {
		SEP_LOG_ERR("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = sep_combined_fin_proc(&op_ctx, &fin_params.props,
		fin_params.data_in, fin_params.data_out,
		fin_params.data_in_size,
		fin_params.auth_data, &(fin_params.auth_data_size));

	if (rc == 0) {
		/* Always copy back digest size + error_info */
		/* (that's the reason for keeping them together)  */
		rc = __copy_to_user(&(user_fin_params->auth_data_size),
			&(fin_params.auth_data_size),
			sizeof(struct dxdi_combined_proc_params) -
			offsetof(struct dxdi_combined_proc_params,
					auth_data_size));
		/* We always need to copy back the digest size (even if 0)
		   in order to indicate validity of digest buffer */
	}

	if ((rc == 0) && (op_ctx.error_info == 0)) {
		if (likely((fin_params.auth_data_size > 0) &&
			(fin_params.auth_data_size <= DXDI_DIGEST_SIZE_MAX))) {
			/* Copy back auth if valid */
			rc = __copy_to_user(&(user_fin_params->auth_data),
				fin_params.auth_data,
				fin_params.auth_data_size);
		}
	}

	/* Even on SeP error the function above
	   returns 0 (operation completed with no host side errors) */
	__put_user(op_ctx.error_info, &(user_fin_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_combined_proc(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{
	struct dxdi_combined_proc_params __user *user_params =
		(struct dxdi_combined_proc_params __user *)arg;
	struct dxdi_combined_proc_params params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
		offsetof(struct dxdi_combined_proc_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&params, user_params, input_size)) {
		SEP_LOG_ERR("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = process_combined_integrated(&op_ctx, &(params.props),
		params.data_in, params.data_out, params.data_in_size,
		params.auth_data, &(params.auth_data_size));

	if (rc == 0) {
		/* Always copy back digest size + error_info */
		/* (that's the reason for keeping them together)  */
		rc = __copy_to_user(&(user_params->auth_data_size),
			&(params.auth_data_size),
			sizeof(struct dxdi_combined_proc_params) -
			offsetof(struct dxdi_combined_proc_params,
					auth_data_size));
		/* We always need to copy back the digest size (even if 0)
		   in order to indicate validity of digest buffer */
	}

	if ((rc == 0) && (op_ctx.error_info == 0)) {
		if (likely((params.auth_data_size > 0) &&
			(params.auth_data_size <= DXDI_DIGEST_SIZE_MAX))) {
			/* Copy back auth if valid */
			rc = __copy_to_user(&(user_params->auth_data),
				params.auth_data, params.auth_data_size);
		}
	}

	/* Even on SeP error the function above
	   returns 0 (operation completed with no host side errors) */
	__put_user(op_ctx.error_info, &(user_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_sym_cipher_proc(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{
	struct dxdi_sym_cipher_proc_params __user *user_params =
		(struct dxdi_sym_cipher_proc_params __user *)arg;
	struct dxdi_sym_cipher_proc_params params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
		offsetof(struct dxdi_sym_cipher_proc_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&params, user_params, input_size)) {
		SEP_LOG_ERR("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = process_integrated(&op_ctx, params.context_buf,
		ALG_CLASS_SYM_CIPHER, &(params.props),
		params.data_in, params.data_out, params.data_in_size,
		NULL, NULL);

	/* Even on SeP error the function above
	   returns 0 (operation completed with no host side errors) */
	__put_user(op_ctx.error_info, &(user_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}


static int sep_ioctl_auth_enc_proc(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{
	struct dxdi_auth_enc_proc_params __user *user_params =
		(struct dxdi_auth_enc_proc_params __user *)arg;
	struct dxdi_auth_enc_proc_params params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
		offsetof(struct dxdi_auth_enc_proc_params, tag);
	int rc;
	uint8_t tag_size;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&params, user_params, input_size)) {
		SEP_LOG_ERR("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);

	if (params.props.adata_size == 0) {
		/* without assoc data we can optimize for one descriptor
		sequence */
		rc = process_integrated(&op_ctx, params.context_buf,
			ALG_CLASS_AUTH_ENC, &(params.props),
			params.text_data, params.data_out,
			params.props.text_size, params.tag, &tag_size);
	} else {
		/* Integrated processing with auth. enc. algorithms with
		   Additional-Data requires special two-descriptors flow */
		rc = process_integrated_auth_enc(&op_ctx, params.context_buf,
			ALG_CLASS_AUTH_ENC, &(params.props),
			params.adata, params.text_data, params.data_out,
			params.props.adata_size, params.props.text_size,
			params.tag, &tag_size);

	}

	if ((rc == 0) && (tag_size != params.props.tag_size)) {
		SEP_LOG_WARN("Got tag result size different than "
			    "requested (%u != %u)\n",
			tag_size, params.props.tag_size);
	}

	if ((rc == 0) && (op_ctx.error_info == 0) &&
	    (tag_size > 0)) {
		if (likely(tag_size <= DXDI_DIGEST_SIZE_MAX)) {
			/* Copy back digest/mac if valid */
			rc = __copy_to_user(&(user_params->tag), params.tag,
				tag_size);
		} else { /* Invalid digest/mac size! */
			SEP_LOG_ERR("Got invalid tag size = %u",
				tag_size);
			op_ctx.error_info = DXDI_ERROR_INVAL_DATA_SIZE;
			rc = -EBUG;
		}
	}

	/* Even on SeP error the function above
	   returns 0 (operation completed with no host side errors) */
	__put_user(op_ctx.error_info, &(user_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_mac_proc(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{
	struct dxdi_mac_proc_params __user *user_params =
		(struct dxdi_mac_proc_params __user *)arg;
	struct dxdi_mac_proc_params params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
		offsetof(struct dxdi_mac_proc_params, mac);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&params, user_params, input_size)) {
		SEP_LOG_ERR("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = process_integrated(&op_ctx, params.context_buf,
		ALG_CLASS_MAC, &(params.props),
		params.data_in, NULL, params.data_in_size,
		params.mac, &(params.mac_size));

	if (rc == 0) {
		/* Always copy back mac size + error_info */
		/* (that's the reason for keeping them together)  */
		rc = __copy_to_user(&(user_params->mac_size),
			&(params.mac_size),
			sizeof(struct dxdi_mac_proc_params) -
			offsetof(struct dxdi_mac_proc_params, mac_size));
		/* We always need to copy back the mac size (even if 0)
		   in order to indicate validity of mac buffer */
	}

	if ((rc == 0) && (op_ctx.error_info == 0)) {
		if (likely((params.mac_size > 0) &&
			(params.mac_size <= DXDI_DIGEST_SIZE_MAX))) {
			/* Copy back mac if valid */
			rc = __copy_to_user(&(user_params->mac), params.mac,
				params.mac_size);
		} else { /* Invalid mac size! */
			SEP_LOG_ERR("Got invalid MAC size = %u",
				params.mac_size);
			op_ctx.error_info = DXDI_ERROR_INVAL_DATA_SIZE;
			rc = -EBUG;
		}
	}

	/* Even on SeP error the function above
	   returns 0 (operation completed with no host side errors) */
	__put_user(op_ctx.error_info, &(user_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}


static int sep_ioctl_hash_proc(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{
	struct dxdi_hash_proc_params __user *user_params =
		(struct dxdi_hash_proc_params __user *)arg;
	struct dxdi_hash_proc_params params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
		offsetof(struct dxdi_hash_proc_params, digest);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&params, user_params, input_size)) {
		SEP_LOG_ERR("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = process_integrated(&op_ctx, params.context_buf,
		ALG_CLASS_HASH, &(params.hash_type),
		params.data_in, NULL, params.data_in_size,
		params.digest, &(params.digest_size));

	if (rc == 0) {
		/* Always copy back digest size + error_info */
		/* (that's the reason for keeping them together)  */
		rc = __copy_to_user(&(user_params->digest_size),
			&(params.digest_size),
			sizeof(struct dxdi_hash_proc_params) -
			offsetof(struct dxdi_hash_proc_params, digest_size));
		/* We always need to copy back the digest size (even if 0)
		   in order to indicate validity of digest buffer */
	}

	if ((rc == 0) && (op_ctx.error_info == 0)) {
		if (likely((params.digest_size > 0) &&
			(params.digest_size <= DXDI_DIGEST_SIZE_MAX))) {
			/* Copy back mac if valid */
			rc = __copy_to_user(&(user_params->digest),
				params.digest, params.digest_size);
		} else { /* Invalid digest size! */
			SEP_LOG_ERR("Got invalid digest size = %u",
				params.digest_size);
			op_ctx.error_info = DXDI_ERROR_INVAL_DATA_SIZE;
			rc = -EBUG;
		}
	}

	/* Even on SeP error the function above
	   returns 0 (operation completed with no host side errors) */
	__put_user(op_ctx.error_info, &(user_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

static int sep_ioctl_sep_rpc(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{

	struct dxdi_sep_rpc_params __user *user_params =
		(struct dxdi_sep_rpc_params __user *)arg;
	struct dxdi_sep_rpc_params params;
	struct sep_op_ctx op_ctx;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
		offsetof(struct dxdi_sep_rpc_params, error_info);
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&params, user_params, input_size)) {
		SEP_LOG_ERR("Failed reading input parameters");
		return -EFAULT;
	}

	op_ctx_init(&op_ctx, client_ctx);
	rc = dispatch_sep_rpc(&op_ctx, params.agent_id, params.func_id,
		params.mem_refs, params.rpc_params_size, params.rpc_params);

	__put_user(op_ctx.error_info, &(user_params->error_info));

	op_ctx_fini(&op_ctx);

	return rc;
}

#if MAX_SEPAPP_SESSION_PER_CLIENT_CTX > 0
static int sep_ioctl_register_mem4dma(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{

	struct dxdi_register_mem4dma_params __user *user_params =
		(struct dxdi_register_mem4dma_params __user *)arg;
	struct dxdi_register_mem4dma_params params;
	/* Calculate size of input parameters part */
	const unsigned long input_size =
		offsetof(struct dxdi_register_mem4dma_params, memref_id);
	enum dma_data_direction dma_dir;
	int rc;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__copy_from_user(&params, user_params, input_size)) {
		SEP_LOG_ERR("Failed reading input parameters");
		return -EFAULT;
	}

	/* convert DMA direction to enum dma_data_direction */
	dma_dir = dxdi_data_dir_to_dma_data_dir(params.memref.dma_direction);
	if (unlikely(dma_dir == DMA_NONE)) {
		SEP_LOG_ERR("Invalid DMA direction (%d)\n",
			params.memref.dma_direction);
		rc = -EINVAL;
	} else {
		params.memref_id = register_client_memref(client_ctx,
			(uint8_t __user *)params.memref.start_or_offset, NULL,
			params.memref.size, dma_dir);
		if (unlikely(!IS_VALID_MEMREF_IDX(params.memref_id)))
			rc = -ENOMEM;
		else {
			rc = __put_user(params.memref_id,
				&(user_params->memref_id));
			if (rc != 0) /* revert if failed __put_user */
				(void)free_client_memref(client_ctx,
					params.memref_id);
		}
	}

	return rc;
}


static int sep_ioctl_free_mem4dma(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{
	struct dxdi_free_mem4dma_params __user *user_params =
		(struct dxdi_free_mem4dma_params __user *)arg;
	dxdi_memref_id_t memref_id;

	/* access permissions to arg was already checked in sep_ioctl */
	if (__get_user(memref_id, &user_params->memref_id)) {
		SEP_LOG_ERR("Failed reading input parameter\n");
		return -EFAULT;
	}

	return free_client_memref(client_ctx, memref_id);
}
#endif

static int sep_ioctl_set_iv(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{
	struct dxdi_aes_iv_params *user_params =
		(struct dxdi_aes_iv_params __user *)arg;
	struct dxdi_aes_iv_params params;
	struct host_crypto_ctx_sym_cipher *host_context =
		(struct host_crypto_ctx_sym_cipher *)user_params->context_buf;
	host_crypto_ctx_id_t uid;
	int err;

	/* Copy ctx uid from user context*/
	if (copy_from_user(&uid, &host_context->uid,
			   sizeof(host_crypto_ctx_id_t))) {
		SEP_LOG_ERR("Failed reading input parameters");
		return -EFAULT;
	}

	/* Copy IV from user context*/
	if (__copy_from_user(&params, user_params,
			     sizeof(struct dxdi_aes_iv_params))) {
		SEP_LOG_ERR("Failed reading input parameters");
		return -EFAULT;
	}
	err = ctxmgr_set_symcipher_iv_user(
		user_params->context_buf, params.iv_ptr);
	if (err != 0)
		return err;

	ctxmgr_sep_cache_invalidate(client_ctx->drv_data->sep_cache,
				    uid, CRYPTO_CTX_ID_SINGLE_MASK);

	return 0;
}

static int sep_ioctl_get_iv(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{
	struct dxdi_aes_iv_params *user_params =
		(struct dxdi_aes_iv_params __user *)arg;
	struct dxdi_aes_iv_params params;
	int err;

	/* copy context ptr from user */
	if (__copy_from_user(&params, user_params, sizeof(uint32_t))) {
		printk("Failed reading input parameters");
		return -EFAULT;
	}

	err = ctxmgr_get_symcipher_iv_user(
		user_params->context_buf, params.iv_ptr);
	if (err != 0)
		return err;

	if (__copy_to_user(user_params, &params,
		sizeof(struct dxdi_aes_iv_params)) != 0)
		return -EFAULT;

	return 0;
}

#ifdef PE_IPSEC_QUEUES
static int sep_ioctl_get_next_ike_event(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{
	struct dxdi_get_next_ike_event_params *user_params =
		(struct dxdi_get_next_ike_event_params __user *)arg;
	struct dxdi_get_next_ike_event_params params;
	int err;

	err = __get_user(params.timeout, &(user_params->timeout));
	if (err)
		return err;
	err = ipsec_ike_event_get_next(
		params.timeout, &params.event_msg_size, params.event_msg);
	if (err != 0)
		return err;
	err = __put_user(params.event_msg_size, &(user_params->event_msg_size));
	if (err != 0)
		return err;
	err = __copy_to_user(user_params->event_msg, &params.event_msg,
			params.event_msg_size);
	return err;
}

/**
 * sep_ioctl_ipsec_ctl() - Control IPsec stack states
 *
 * @client_ctx:
 * @arg:
 */
static int sep_ioctl_ipsec_ctl(
	struct sep_client_ctx *client_ctx, unsigned long arg)
{
	struct dxdi_ipsec_ctl_params *user_params =
		(struct dxdi_ipsec_ctl_params __user *)arg;
	enum dxdi_ipsec_ctl_op op;
	ipsec_qs_h qs = client_ctx->drv_data->sep_data->ipsec_qs;
	int err;

	if (unlikely(qs == IPSEC_QS_INVALID_HANDLE)) {
		SEP_LOG_ERR("Invoked for unavailable resource\n");
		return -ENODEV; /* Not supposed to happen... */
	}
	err = __get_user(op, &(user_params->op));
	if (unlikely(err != 0))
		return err;
	switch (op) {
	case DXDI_IPSEC_ACTIVATE:
		err = ipsec_qs_activate(qs);
		break;
	case DXDI_IPSEC_DEACTIVATE:
		err = ipsec_qs_set_state(qs, DESC_Q_INACTIVE);
		break;
	default:
		SEP_LOG_ERR("Invalid request: %d\n", op);
		err = -EINVAL;
	}
	return err;
}
#endif

/****** Driver entry points (open, release, ioctl, etc.) ******/

/**
 * init_client_ctx() - Initialize a client context object given
 * @drvdata:	Queue driver context
 * @client_ctx:
 *
 * Returns void
 */
void init_client_ctx(struct queue_drvdata *drvdata,
	struct sep_client_ctx *client_ctx)
{
	int i;
	const unsigned int qid = drvdata - (drvdata->sep_data->queue);

	/* Initialize user data structure */
	client_ctx->qid = qid;
	client_ctx->drv_data = drvdata;
	atomic_set(&client_ctx->uid_cntr, 0);
#if MAX_SEPAPP_SESSION_PER_CLIENT_CTX > 0
	/* Initialize sessions */
	for (i = 0; i < MAX_SEPAPP_SESSION_PER_CLIENT_CTX; i++) {
		mutex_init(&client_ctx->sepapp_sessions[i].session_lock);
		client_ctx->sepapp_sessions[i].sep_session_id =
			SEP_SESSION_ID_INVALID;
		/* The rest of the fields are 0/NULL from kzalloc*/
	}
#endif
	/* Initialize memrefs */
	for (i = 0; i < MAX_REG_MEMREF_PER_CLIENT_CTX; i++) {
		mutex_init(&client_ctx->reg_memrefs[i].buf_lock);
		/* The rest of the fields are 0/NULL from kzalloc */
	}
}

/**
 * sep_open() - "open" device file entry point.
 * @inode:
 * @file:
 *
 * Returns int
 */
static int sep_open(struct inode *inode, struct file *file)
{
	struct queue_drvdata *drvdata;
	struct sep_client_ctx *client_ctx;
	unsigned int qid;

	drvdata = container_of(inode->i_cdev, struct queue_drvdata, cdev);

	if (imajor(inode) != MAJOR(drvdata->sep_data->devt_base)) {
		SEP_LOG_ERR("Invalid major device num=%d\n", imajor(inode));
		return -ENOENT;
	}
	qid = iminor(inode) - MINOR(drvdata->sep_data->devt_base);
	if (qid >= drvdata->sep_data->num_of_desc_queues) {
		SEP_LOG_ERR("Invalid minor device num=%d\n", iminor(inode));
		return -ENOENT;
	}
#ifdef DEBUG
	/* The qid based on the minor device number must match the offset
	   of given drvdata in the queues array of the sep_data context */
	if (qid != (drvdata - (drvdata->sep_data->queue))) {
		SEP_LOG_ERR("qid=%d but drvdata index is %d\n",
			qid, (drvdata - (drvdata->sep_data->queue)));
		return -EBUG;
	}
#endif
	SEP_LOG_DEBUG("qid=%d\n", qid);

	client_ctx = kzalloc(sizeof(struct sep_client_ctx), GFP_KERNEL);
	if (client_ctx == NULL)
		return -ENOMEM;

	init_client_ctx(drvdata, client_ctx);

	file->private_data = client_ctx;

	return 0;
}


void cleanup_client_ctx(struct queue_drvdata *drvdata,
	struct sep_client_ctx *client_ctx)
{
	dxdi_memref_id_t memref_id;
#if MAX_SEPAPP_SESSION_PER_CLIENT_CTX > 0
	struct sep_op_ctx op_ctx;
	dxdi_sepapp_session_id_t session_id;

	/* Free any Applet session left open */
	for (session_id = 0; session_id < MAX_SEPAPP_SESSION_PER_CLIENT_CTX;
	      session_id++) {
		if (IS_VALID_SESSION_CTX(
			&client_ctx->sepapp_sessions[session_id])) {
			SEP_LOG_DEBUG("Closing session ID=%d\n", session_id);
			op_ctx_init(&op_ctx, client_ctx);
			sepapp_session_close(&op_ctx, session_id);
			/* Note: There is never a problem with the session's
			   ref_cnt because when "release" is invoked there
			   are no pending IOCTLs, so ref_cnt is at most 1 */
			op_ctx_fini(&op_ctx);
		}
		mutex_destroy(
			&client_ctx->sepapp_sessions[session_id].session_lock);
	}
#endif /*MAX_SEPAPP_SESSION_PER_CLIENT_CTX > 0*/

	/* Free registered user memory references */
	for (memref_id = 0; memref_id < MAX_REG_MEMREF_PER_CLIENT_CTX;
	      memref_id++) {
		if (client_ctx->reg_memrefs[memref_id].ref_cnt > 0) {
			SEP_LOG_DEBUG("Freeing user memref ID=%d\n", memref_id);
			(void)free_client_memref(client_ctx, memref_id);
		}
		/* There is no problem with memref ref_cnt because when
		   "release" is invoked there are no pending IOCTLs,
		   so ref_cnt is at most 1                             */
		mutex_destroy(&client_ctx->reg_memrefs[memref_id].buf_lock);
	}

	/* Invalidate any outstanding descriptors associated with this
	   client_ctx*/
	desc_q_mark_invalid_cookie(drvdata->desc_queue, (uint32_t)client_ctx);

	/* Invalidate any crypto context cache entry associated with this client
	   context before freeing context data object that may be reused.
	   This assures retaining of UIDs uniqueness (and makes sense since all
	   associated contexts does not exist anymore) */
	ctxmgr_sep_cache_invalidate(drvdata->sep_cache,
		((uint64_t)(unsigned long)client_ctx) <<
			CRYPTO_CTX_ID_CLIENT_SHIFT,
		CRYPTO_CTX_ID_CLIENT_MASK);
}

static int sep_release(struct inode *inode, struct file *file)
{
	struct sep_client_ctx *client_ctx = file->private_data;
	struct queue_drvdata *drvdata = client_ctx->drv_data;

	cleanup_client_ctx(drvdata, client_ctx);

	kfree(client_ctx);

	return 0;
}

static ssize_t sep_read(
	struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
	SEP_LOG_DEBUG("Invoked for %u bytes", count);
	return -ENOSYS; /* nothing to read... IOCTL only */
}

/*!
 * The SeP device does not support read/write
 * We use it for debug purposes. Currently loopback descriptor is sent given
 * number of times. Usage example: echo 10 > /dev/dx_sep_q0
 * TODO: Move this functionality to sysfs?
 *
 * \param filp
 * \param buf
 * \param count
 * \param ppos
 *
 * \return ssize_t
 */
static ssize_t
sep_write(struct file *filp, const char __user *buf,
	  size_t count, loff_t *ppos)
{
	struct sep_sw_desc desc;
	struct sep_client_ctx *client_ctx = filp->private_data;
	struct sep_op_ctx op_ctx;
	unsigned int loop_times = 1;
	unsigned int i;
	int rc = 0;
	char tmp_buf[80];

	if (count > 79)
		return -ENOMEM; /* Avoid buffer overflow */
	memcpy(tmp_buf, buf, count); /* Copy to NULL terminate */
	tmp_buf[count] = 0; /* NULL terminate */

	/*SEP_LOG_DEBUG("Invoked for %u bytes", count);*/
	sscanf(buf, "%u", &loop_times);
	SEP_LOG_DEBUG("Loopback X %u...\n", loop_times);

	op_ctx_init(&op_ctx, client_ctx);
	/* prepare loopback descriptor */
	desq_q_pack_debug_desc(&desc, &op_ctx);

	/* Perform loopback for given times */
	for (i = 0; i < loop_times; i++) {
		op_ctx.op_state = USER_OP_INPROC;
		rc = desc_q_enqueue(client_ctx->drv_data->desc_queue, &desc,
			true);
		if (unlikely(IS_DESCQ_ENQUEUE_ERR(rc))) {
			SEP_LOG_ERR("Failed sending desc. %u\n", i);
			break;
		}
		rc = wait_for_sep_op_result(&op_ctx);
		if (rc != 0) {
			SEP_LOG_ERR("Failed completion of desc. %u\n", i);
			break;
		}
		op_ctx.op_state = USER_OP_NOP;
	}

	op_ctx_fini(&op_ctx);

	SEP_LOG_DEBUG("Completed loopback of %u desc.\n", i);

	return count; /* Noting to write for this device... */
}

#define IOCTL_CMD_CASE_ARG_ONLY(_ioc_nr, _ioc_func)   \
	case _ioc_nr:                                 \
		SEP_LOG_DEBUG(#_ioc_nr "\n");         \
		err = _ioc_func(arg);                 \
		break

#define IOCTL_CMD_CASE(_ioc_nr, _ioc_func)        \
	case _ioc_nr:                             \
		SEP_LOG_DEBUG(#_ioc_nr "\n");     \
		err = _ioc_func(client_ctx, arg); \
		break

/*!
 * IOCTL entry point
 *
 * \param filp
 * \param cmd
 * \param arg
 *
 * \return int
 * \retval 0 Operation succeeded (but SeP return code may indicate an error)
 * \retval -ENOTTY  : Unknown IOCTL command
 * \retval -ENOSYS  : Unsupported/not-implemented (known) operation
 * \retval -EINVAL  : Invalid parameters
 * \retval -EFAULT  : Bad pointers for given user memory space
 * \retval -EPERM   : Not enough permissions for given command
 * \retval -ENOMEM,-EAGAIN: when not enough resources available for given op.
 * \retval -EIO     : SeP HW error or another internal error
 *                    (probably operation timed out or unexpected behavior)
 */
static long
sep_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct sep_client_ctx *client_ctx = filp->private_data;
	unsigned long long ioctl_start;
	int err = 0;


	ioctl_start = sched_clock();

	/* Verify IOCTL command: magic + number */
	if (_IOC_TYPE(cmd) != DXDI_IOC_MAGIC) {
		SEP_LOG_ERR("Invalid IOCTL type=%u", _IOC_TYPE(cmd));
		return -ENOTTY;
	}
	if (_IOC_NR(cmd) > DXDI_IOC_NR_MAX) {
		SEP_LOG_ERR("IOCTL NR=%u out of range for ABI ver.=%u.%u",
			_IOC_NR(cmd), DXDI_VER_MAJOR, DXDI_VER_MINOR);
		return -ENOTTY;
	}

	/* Verify permissions on parameters pointer (arg) */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(ACCESS_WRITE,
				(void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(ACCESS_READ,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	switch (_IOC_NR(cmd)) {
	/* Version info. commands */
	IOCTL_CMD_CASE_ARG_ONLY(DXDI_IOC_NR_GET_VER_MAJOR,
		sep_ioctl_get_ver_major);
	IOCTL_CMD_CASE_ARG_ONLY(DXDI_IOC_NR_GET_VER_MINOR,
		sep_ioctl_get_ver_minor);
	/* Context size queries */
	IOCTL_CMD_CASE_ARG_ONLY(DXDI_IOC_NR_GET_SYMCIPHER_CTX_SIZE,
		sep_ioctl_get_sym_cipher_ctx_size);
	IOCTL_CMD_CASE_ARG_ONLY(DXDI_IOC_NR_GET_AUTH_ENC_CTX_SIZE,
		sep_ioctl_get_auth_enc_ctx_size);
	IOCTL_CMD_CASE_ARG_ONLY(DXDI_IOC_NR_GET_MAC_CTX_SIZE,
		sep_ioctl_get_mac_ctx_size);
	IOCTL_CMD_CASE_ARG_ONLY(DXDI_IOC_NR_GET_HASH_CTX_SIZE,
		sep_ioctl_get_hash_ctx_size);
	/* Init context commands */
	IOCTL_CMD_CASE(DXDI_IOC_NR_SYMCIPHER_INIT, sep_ioctl_sym_cipher_init);
	IOCTL_CMD_CASE(DXDI_IOC_NR_AUTH_ENC_INIT, sep_ioctl_auth_enc_init);
	IOCTL_CMD_CASE(DXDI_IOC_NR_MAC_INIT, sep_ioctl_mac_init);
	IOCTL_CMD_CASE(DXDI_IOC_NR_HASH_INIT, sep_ioctl_hash_init);
	IOCTL_CMD_CASE(DXDI_IOC_NR_COMBINED_INIT, sep_ioctl_combined_init);
	/* Processing commands */
	IOCTL_CMD_CASE(DXDI_IOC_NR_PROC_DBLK, sep_ioctl_proc_dblk);
	IOCTL_CMD_CASE(DXDI_IOC_NR_COMBINED_PROC_DBLK,
		sep_ioctl_combined_proc_dblk);
	IOCTL_CMD_CASE(DXDI_IOC_NR_FIN_PROC, sep_ioctl_fin_proc);
	IOCTL_CMD_CASE(DXDI_IOC_NR_COMBINED_PROC_FIN,
		sep_ioctl_combined_fin_proc);
	/* "Integrated" processing operations */
	IOCTL_CMD_CASE(DXDI_IOC_NR_SYMCIPHER_PROC, sep_ioctl_sym_cipher_proc);
	IOCTL_CMD_CASE(DXDI_IOC_NR_AUTH_ENC_PROC, sep_ioctl_auth_enc_proc);
	IOCTL_CMD_CASE(DXDI_IOC_NR_MAC_PROC, sep_ioctl_mac_proc);
	IOCTL_CMD_CASE(DXDI_IOC_NR_HASH_PROC, sep_ioctl_hash_proc);
	IOCTL_CMD_CASE(DXDI_IOC_NR_COMBINED_PROC, sep_ioctl_combined_proc);
	/* SeP RPC */
	IOCTL_CMD_CASE(DXDI_IOC_NR_SEP_RPC, sep_ioctl_sep_rpc);
#if MAX_SEPAPP_SESSION_PER_CLIENT_CTX > 0
	/* Memory registation */
	IOCTL_CMD_CASE(DXDI_IOC_NR_REGISTER_MEM4DMA,
		sep_ioctl_register_mem4dma);
	case DXDI_IOC_NR_ALLOC_MEM4DMA:
		SEP_LOG_ERR("DXDI_IOC_NR_ALLOC_MEM4DMA: Not supported, yet");
		err = -ENOTTY;
		break;
	IOCTL_CMD_CASE(DXDI_IOC_NR_FREE_MEM4DMA, sep_ioctl_free_mem4dma);
	IOCTL_CMD_CASE(DXDI_IOC_NR_SEPAPP_SESSION_OPEN,
		sep_ioctl_sepapp_session_open);
	IOCTL_CMD_CASE(DXDI_IOC_NR_SEPAPP_SESSION_CLOSE,
		sep_ioctl_sepapp_session_close);
	IOCTL_CMD_CASE(DXDI_IOC_NR_SEPAPP_COMMAND_INVOKE,
		sep_ioctl_sepapp_command_invoke);
#endif
	IOCTL_CMD_CASE(DXDI_IOC_NR_SET_IV, sep_ioctl_set_iv);
	IOCTL_CMD_CASE(DXDI_IOC_NR_GET_IV, sep_ioctl_get_iv);
#ifdef PE_IPSEC_QUEUES
	IOCTL_CMD_CASE(DXDI_IOC_NR_GET_NEXT_IKE_EVENT,
		sep_ioctl_get_next_ike_event);
	IOCTL_CMD_CASE(DXDI_IOC_NR_IPSEC_CTL, sep_ioctl_ipsec_ctl);
#endif
	default: /* Not supposed to happen - we already tested for NR range */
		SEP_LOG_ERR("bad IOCTL cmd 0x%08X\n", cmd);
		err = -ENOTTY;
	} /*switch(_IOC_NR(cmd))*/

	/* Update stats per IOCTL command */
	if (err == 0)
		sysfs_update_drv_stats(client_ctx->qid, _IOC_NR(cmd),
			ioctl_start, sched_clock());

	return err;
}

static const struct file_operations sep_fops = {
	.owner = THIS_MODULE,
	.open = sep_open,
	.release = sep_release,
	.read = sep_read,
	.write = sep_write,
	.unlocked_ioctl = sep_ioctl,
};


/**
 * get_q_cache_size() - Get the number of entries to allocate for the SeP/FW
 *			cache of given queue
 * @drvdata:	 Driver context
 * @qid:	 The queue to allocate for
 *
 * Get the number of entries to allocate for the SeP/FW cache of given queue
 * The function assumes that num_of_desc_queues and num_of_sep_cache_entries
 * are already initialized in drvdata.
 * Returns Number of cache entries to allocate
 */
static int get_q_cache_size(struct sep_drvdata *drvdata, int qid)
{
	/* Simple allocation - divide evenly among queues */
	/* consider prefering higher priority queues...   */
	return drvdata->num_of_sep_cache_entries / drvdata->num_of_desc_queues;
}


/**
 * alloc_host_mem_for_sep() - Allocate memory pages for sep icache/dcache
 *	or for SEP backup memory in case there is not SEP cache memory.
 *
 * @drvdata:
 *
 * Currently using alloc_pages to allocate the pages.
 * Consider using CMA feature for the memory allocation
 */
//static int __devinit alloc_host_mem_for_sep(struct sep_drvdata *drvdata)
static int alloc_host_mem_for_sep(struct sep_drvdata *drvdata)
{
#ifdef CACHE_IMAGE_NAME
	int i;
	const int icache_sizes_enum2log[] = DX_CC_ICACHE_SIZE_ENUM2LOG;

	SEP_LOG_DEBUG("icache_size=%uKB dcache_size=%uKB\n",
		1 << (icache_size_log2 - 10), 1 << (dcache_size_log2 - 10));

	/* Verify validity of chosen cache memory sizes */
	if ((dcache_size_log2 > DX_CC_INIT_D_CACHE_MAX_SIZE_LOG2) ||
	    (dcache_size_log2 < DX_CC_INIT_D_CACHE_MIN_SIZE_LOG2)) {
		SEP_LOG_ERR("Requested Dcache size (%uKB) is invalid\n",
			1 << (dcache_size_log2 - 10));
		return -EINVAL;
	}
	/* Icache size must be one of values defined for this device */
	for (i = 0; i < sizeof(icache_sizes_enum2log); i++)
		if ((icache_size_log2 == icache_sizes_enum2log[i]) &&
		    (icache_sizes_enum2log[i] >= 0)) /* Found valid value */
			break;
	if (unlikely(i == sizeof(icache_sizes_enum2log))) {
		SEP_LOG_ERR("Requested Icache size (%uKB) is invalid\n",
			1 << (icache_size_log2 - 10));
	}
	drvdata->icache_size_log2 = icache_size_log2;
	/* Allocate pages suitable for 32bit DMA and out of cache (cold) */
	drvdata->icache_pages = alloc_pages(
		GFP_KERNEL | GFP_DMA32 |  __GFP_COLD,
		icache_size_log2 - PAGE_SHIFT);
	if (drvdata->icache_pages == NULL) {
		SEP_LOG_ERR("Failed allocating %uKB for Icache\n",
			1 << (icache_size_log2 - 10));
		return -ENOMEM;
	}
	drvdata->dcache_size_log2 = dcache_size_log2;
	/* same as for icache */
	drvdata->dcache_pages = alloc_pages(
		GFP_KERNEL | GFP_DMA32 |  __GFP_COLD,
		dcache_size_log2 - PAGE_SHIFT);
	if (drvdata->dcache_pages == NULL) {
		SEP_LOG_ERR("Failed allocating %uKB for Dcache\n",
			1 << (dcache_size_log2 - 10));
		__free_pages(drvdata->icache_pages,
			drvdata->icache_size_log2 - PAGE_SHIFT);
		return -ENOMEM;
	}
#elif defined(SEP_BACKUP_BUF_SIZE)
	/* This size is not enforced by power of two, so we use
	   alloc_pages_exact() */
	drvdata->sep_backup_buf = alloc_pages_exact(
		SEP_BACKUP_BUF_SIZE,
		GFP_KERNEL | GFP_DMA32 |  __GFP_COLD);
	if (unlikely(drvdata->sep_backup_buf == NULL)) {
		SEP_LOG_ERR("Failed allocating %d B for SEP backup "
			    "buffer\n", SEP_BACKUP_BUF_SIZE);
		return -ENOMEM;
	}
	drvdata->sep_backup_buf_size = SEP_BACKUP_BUF_SIZE;
#endif
	return 0;
}

/**
 * free_host_mem_for_sep() - Free the memory resources allocated by
 *	alloc_host_mem_for_sep()
 *
 * @drvdata:
 */
static void free_host_mem_for_sep(struct sep_drvdata *drvdata)
{
#ifdef CACHE_IMAGE_NAME
	if (drvdata->dcache_pages != NULL) {
		__free_pages(drvdata->dcache_pages,
			drvdata->dcache_size_log2 - PAGE_SHIFT);
		drvdata->dcache_pages = NULL;
	}
	if (drvdata->icache_pages != NULL) {
		__free_pages(drvdata->icache_pages,
			drvdata->icache_size_log2 - PAGE_SHIFT);
		drvdata->icache_pages = NULL;
	}
#elif defined(SEP_BACKUP_BUF_SIZE)
	if (drvdata->sep_backup_buf != NULL) {
		free_pages_exact(drvdata->sep_backup_buf,
			drvdata->sep_backup_buf_size);
		drvdata->sep_backup_buf_size = 0;
		drvdata->sep_backup_buf = NULL;
	}
#endif
}
#ifdef PE_IPSEC_QUEUES
static int random_seed_create(unsigned int *seed_buff, unsigned int seed_size)
{

	if (seed_size > DXDI_RND_SEED_SIZE_IN_WORD) {
		SEP_LOG_ERR("Couldn't create Sep Rrandom Seed\n");
		return -EINVAL;
	}
	get_random_bytes(seed_buff, seed_size * sizeof(int));

	return 0;

}
#endif
//static int __devinit sep_setup(struct device *dev,
static int sep_setup(struct device *dev,
			       const struct resource *regs_res,
			       struct resource *r_irq)
{
	dev_t devt;
	struct sep_drvdata *drvdata = NULL;
	enum dx_sep_state sep_state;
	int rc = 0;
	int i;

	SEP_LOG_INFO("Discretix %s Driver initializing...\n", DRIVER_NAME);

	drvdata = kzalloc(sizeof(struct sep_drvdata), GFP_KERNEL);
	if (unlikely(drvdata == NULL)) {
		SEP_LOG_ERR("Unable to allocate device private record\n");
		rc = -ENOMEM;
		goto failed0;
	}
	dev_set_drvdata(dev, (void *)drvdata);

	if (!regs_res) {
		SEP_LOG_ERR("Couldn't get registers resource\n");
		rc = -EFAULT;
		goto failed1;
	}

	if (q_num > SEP_MAX_NUM_OF_DESC_Q) {
		SEP_LOG_ERR("Requested number of queues (%u) is out of range - "
			    "must be no more than %u\n",
			    q_num,
			    SEP_MAX_NUM_OF_DESC_Q);
		rc = -EINVAL;
		goto failed1;
	}

	/* TODO: Verify number of queues also with SeP capabilities */
	/* Initialize objects arrays for proper cleanup in case of error */
	for (i = 0; i < SEP_MAX_NUM_OF_DESC_Q; i++) {
		drvdata->queue[i].desc_queue = DESC_Q_INVALID_HANDLE;
		drvdata->queue[i].sep_cache = SEP_CTX_CACHE_NULL_HANDLE;
	}

	drvdata->mem_start = regs_res->start;
	drvdata->mem_end = regs_res->end;
	drvdata->mem_size = regs_res->end - regs_res->start + 1;

	if (!request_mem_region(drvdata->mem_start,
				drvdata->mem_size, DRIVER_NAME)) {
		SEP_LOG_ERR("Couldn't lock memory region at %Lx\n",
			(unsigned long long)regs_res->start);
		rc = -EBUSY;
		goto failed1;
	}

	drvdata->dev = dev;
	drvdata->cc_base = ioremap(drvdata->mem_start, drvdata->mem_size);
	if (drvdata->cc_base == NULL) {
		SEP_LOG_ERR("ioremap() failed\n");
		goto failed2;
	}

	SEP_LOG_INFO("regbase_phys=0x%08X..0x%08X\n",
		 (u32)drvdata->mem_start, (u32)drvdata->mem_end);
	SEP_LOG_INFO("regbase_virt=0x%08X\n",
		 (u32)drvdata->cc_base);

#ifdef DX_BASE_ENV_REGS
	SEP_LOG_INFO("FPGA ver. = 0x%08X\n",
		READ_REGISTER(DX_ENV_REG_ADDR(drvdata->cc_base, VERSION)));
	/* TODO: verify FPGA version against expected version */
#endif

	rc = init_cc_interrupts(drvdata, r_irq);
	if (rc != 0)
		goto failed3;

#ifdef SEP_PRINTF
	/* Sync. host to SeP initialization counter */
	/* After seting the interrupt mask, the interrupt from the GPR would
	   trigger host_printf_handler to ack this value */
	drvdata->last_ack_cntr = SEP_PRINTF_ACK_SYNC_VAL;
	(void)register_cc_interrupt_handler(drvdata,
		SEP_HOST_GPR_IRQ_SHIFT(DX_SEP_HOST_PRINTF_GPR_IDX),
		sep_printf_handler, NULL);
#endif

	dx_sep_power_init(drvdata);

	/* SeP FW initialization sequence */
	/* Cold boot before creating descQ objects */
	sep_state = GET_SEP_STATE(drvdata);
	if (sep_state != DX_SEP_STATE_DONE_COLD_BOOT) {
		SEP_LOG_DEBUG("sep_state=0x%08X\n", sep_state);
		/* If INIT_CC was not done externally, take care of it here */
		rc = alloc_host_mem_for_sep(drvdata);
		if (unlikely(rc != 0))
			goto failed4;
		rc = sepinit_do_cc_init(drvdata);
		if (unlikely(rc != 0))
			goto failed5;
	}
	sepinit_get_fw_props(drvdata);
	if (drvdata->fw_ver != EXPECTED_FW_VER) {
		SEP_LOG_WARN("Expected FW version %u.%u.%u but got %u.%u.%u\n",
			VER_MAJOR(EXPECTED_FW_VER),
			VER_MINOR(EXPECTED_FW_VER),
			VER_PATCH(EXPECTED_FW_VER),
			VER_MAJOR(drvdata->fw_ver),
			VER_MINOR(drvdata->fw_ver),
			VER_PATCH(drvdata->fw_ver));
	}

	if (q_num > drvdata->num_of_desc_queues) {
		SEP_LOG_ERR("Requested number of queues (%u) is greater  "
			    "than SEP could support (%u)\n",
			    q_num,
			    drvdata->num_of_desc_queues);
		rc = -EINVAL;
		goto failed5;
	}

	if (q_num == 0)	{
		if (drvdata->num_of_desc_queues > SEP_MAX_NUM_OF_DESC_Q) {
			SEP_LOG_INFO("The SEP number of queues (%u) is greater "
			    "than the driver could support (%u)\n",
			    drvdata->num_of_desc_queues, SEP_MAX_NUM_OF_DESC_Q);
			q_num = SEP_MAX_NUM_OF_DESC_Q;
		} else {
			q_num = drvdata->num_of_desc_queues;
		}
	}
	drvdata->num_of_desc_queues = q_num;

	SEP_LOG_INFO("q_num=%d\n", drvdata->num_of_desc_queues);

	rc = dx_sep_req_init(drvdata);
	if (unlikely(rc != 0))
		goto failed6;

	/* Create descriptor queues objects - must be before
	*  sepinit_set_fw_init_params to assure GPRs from host are reset */
	for (i = 0; i < drvdata->num_of_desc_queues; i++) {
		drvdata->queue[i].sep_data = drvdata;
		mutex_init(&drvdata->queue[i].desc_queue_sequencer);
		drvdata->queue[i].desc_queue =
			desc_q_create(i, &drvdata->queue[i]);
		if (drvdata->queue[i].desc_queue == DESC_Q_INVALID_HANDLE) {
			SEP_LOG_ERR("Unable to allocate desc_q object (%d)\n",
				i);
			rc = -ENOMEM;
			goto failed7;
		}
	}

	/* Create context cache management objects */
	for (i = 0; i < drvdata->num_of_desc_queues; i++) {
		const int num_of_cache_entries = get_q_cache_size(drvdata, i);
		if (num_of_cache_entries < 1) {
			SEP_LOG_ERR(
				"No SeP cache entries were assigned for qid=%d",
				i);
			rc = -ENOMEM;
			goto failed7;
		}
		drvdata->queue[i].sep_cache =
			ctxmgr_sep_cache_create(num_of_cache_entries);
		if (drvdata->queue[i].sep_cache == SEP_CTX_CACHE_NULL_HANDLE) {
			SEP_LOG_ERR(
				"Unable to allocate SeP cache object (%d)\n",
				i);
			rc = -ENOMEM;
			goto failed7;
		}
	}

#ifdef PE_IPSEC_QUEUES
	if (drvdata->ipsec_q_size_log2 > 0) { /* 0 means it does not exist */
		drvdata->ipsec_qs = ipsec_qs_create(
			drvdata, drvdata->ipsec_q_size_log2);
		if (drvdata->ipsec_qs == IPSEC_QS_INVALID_HANDLE) {
			SEP_LOG_ERR("Failed creating IPsec queues.\n");
			goto failed7;
		}
	}

	if (ipsec_ike_event_listener_init() != 0)
		goto failed7;

	if (drvdata->rnd_seed_size > 0) { /* 0 means it does not exist */
		rc = random_seed_create(drvdata->rnd_seed,
			drvdata->rnd_seed_size);

		if (rc != 0) {
			SEP_LOG_ERR("Failed creating IPsec Random Seed .\n");
			goto failed7;
		}
	}
#endif

	rc = sepinit_do_fw_init(drvdata, true /* cold boot */);
	if (unlikely(rc != 0))
		goto failed7;

	drvdata->llimgr = llimgr_create(drvdata->dev, drvdata->mlli_table_size);
	if (drvdata->llimgr == LLIMGR_NULL_HANDLE) {
		SEP_LOG_ERR("Failed creating LLI-manager object\n");
		rc = -ENOMEM;
		goto failed7;
	}

	drvdata->spad_buf_pool = dma_pool_create(
				"dx_sep_rpc_msg", drvdata->dev,
				USER_SPAD_SIZE,
				L1_CACHE_BYTES, 0);
	if (drvdata->spad_buf_pool == NULL) {
		SEP_LOG_ERR("Failed allocating DMA pool for RPC messages\n");
		rc = -ENOMEM;
		goto failed8;
	}

	/* Add character device nodes */
	rc = alloc_chrdev_region(&drvdata->devt_base, 0, SEP_DEVICES,
		DRIVER_NAME);
	if (unlikely(rc != 0))
		goto failed9;
	SEP_LOG_DEBUG("Allocated %u chrdevs at %u:%u\n", SEP_DEVICES,
		MAJOR(drvdata->devt_base), MINOR(drvdata->devt_base));
	for (i = 0; i < drvdata->num_of_desc_queues; i++) {
		devt = MKDEV(MAJOR(drvdata->devt_base),
			MINOR(drvdata->devt_base) + i);
		cdev_init(&drvdata->queue[i].cdev, &sep_fops);
		drvdata->queue[i].cdev.owner = THIS_MODULE;
		rc = cdev_add(&drvdata->queue[i].cdev, devt, 1);
		if (unlikely(rc != 0)) {
			SEP_LOG_ERR("cdev_add() failed for q%d\n", i);
			goto failed9;
		}
		drvdata->queue[i].dev =	device_create(sep_class, dev, devt,
			&drvdata->queue[i], "%s%d", DX_DEVICE_NAME_PREFIX, i);
		drvdata->queue[i].devt = devt;
	}

	rc = sep_setup_sysfs(&(dev->kobj), drvdata);
	if (unlikely(rc != 0))
		goto failed9;

	/* Everything is ready - activate desc-Qs */
	for (i = 0; i < drvdata->num_of_desc_queues; i++)
		desc_q_activate(drvdata->queue[i].desc_queue);

	/* Enable sep request interrupt handling */
	dx_sep_req_enable(drvdata);

#if 0 //def CONFIG_PM_RUNTIME
	pm_runtime_put_noidle(&sep->pdev->dev);
	pm_runtime_mark_last_busy
	
#endif



	/* Init DX Linux crypto module */
	if (!disable_linux_crypto) {
		rc = dx_crypto_api_init(drvdata);
		if (unlikely(rc != 0))
			goto failed10;
	}
#if MAX_SEPAPP_SESSION_PER_CLIENT_CTX > 0
	dx_sepapp_init(drvdata);
#endif
	return 0;		/*success */

/* Error cases cleanup */
#if 0 /* Left for future use (additional initializations) */
failed11:
	dx_crypto_api_fini();
#endif
failed10:
	/* Disable interrupts */
	WRITE_REGISTER(drvdata->cc_base + DX_CC_REG_OFFSET(HOST, IMR), ~0);
	sep_free_sysfs();
failed9:
	/* Cleanup device nodes */
	for (i = 0; i < drvdata->num_of_desc_queues; i++) {
		if (drvdata->queue[i].devt != 0) { /* Cleanup required */
			device_destroy(sep_class, drvdata->queue[i].devt);
			cdev_del(&drvdata->queue[i].cdev);
		}
	}
	if (drvdata->devt_base != 0)
		unregister_chrdev_region(drvdata->devt_base, SEP_DEVICES);
	dma_pool_destroy(drvdata->spad_buf_pool);
failed8:
	llimgr_destroy(drvdata->llimgr);
failed7:
#ifdef PE_IPSEC_QUEUES
	ipsec_ike_event_listener_terminate();
	if (drvdata->ipsec_qs != IPSEC_QS_INVALID_HANDLE)
		ipsec_qs_destroy(drvdata->ipsec_qs);
#endif
	for (i = 0; i < drvdata->num_of_desc_queues; i++) {
		if (drvdata->queue[i].sep_cache != SEP_CTX_CACHE_NULL_HANDLE) {
			ctxmgr_sep_cache_destroy(drvdata->queue[i].sep_cache);
			drvdata->queue[i].sep_cache = SEP_CTX_CACHE_NULL_HANDLE;
		}
		if (drvdata->queue[i].desc_queue != DESC_Q_INVALID_HANDLE) {
			desc_q_destroy(drvdata->queue[i].desc_queue);
			drvdata->queue[i].desc_queue = DESC_Q_INVALID_HANDLE;
			mutex_destroy(&drvdata->queue[i].desc_queue_sequencer);
		}
	}

failed6:
	dx_sep_req_fini(drvdata);
failed5:
	free_host_mem_for_sep(drvdata);
failed4:
	dx_sep_power_exit(drvdata);
	detach_cc_interrupts(drvdata);
failed3:
	iounmap(drvdata->cc_base);
failed2:
	release_mem_region(regs_res->start, drvdata->mem_size);
failed1:
	kfree(drvdata);
failed0:

	return rc;
}

//static int __devexit sep_remove(struct platform_device *plat_dev)
static int sep_remove(struct platform_device *plat_dev)
{
	struct device *dev = &plat_dev->dev;
	struct sep_drvdata *drvdata =
		(struct sep_drvdata *)dev_get_drvdata(dev);
	int i;

	if (!drvdata)
		return 0;
	if (!disable_linux_crypto)
		dx_crypto_api_fini();
	/* Disable interrupts */
	WRITE_REGISTER(drvdata->cc_base + DX_CC_REG_OFFSET(HOST, IMR), ~0);

	for (i = 0; i < drvdata->num_of_desc_queues; i++) {
		if (drvdata->queue[i].desc_queue != DESC_Q_INVALID_HANDLE) {
			desc_q_destroy(drvdata->queue[i].desc_queue);
			mutex_destroy(&drvdata->queue[i].desc_queue_sequencer);
		}
		if (drvdata->queue[i].sep_cache != NULL)
			ctxmgr_sep_cache_destroy(drvdata->queue[i].sep_cache);
		device_destroy(sep_class, drvdata->queue[i].devt);
		cdev_del(&drvdata->queue[i].cdev);
	}
#ifdef PE_IPSEC_QUEUES
	ipsec_ike_event_listener_terminate();
	if (drvdata->ipsec_qs != IPSEC_QS_INVALID_HANDLE)
		ipsec_qs_destroy(drvdata->ipsec_qs);
#endif
	dx_sep_req_fini(drvdata); /* Must be done after
				     ipsec_ike_event_listener_terminate() */
	unregister_chrdev_region(drvdata->devt_base, SEP_DEVICES);
	dma_pool_destroy(drvdata->spad_buf_pool);
	drvdata->spad_buf_pool = NULL;
	llimgr_destroy(drvdata->llimgr);
	drvdata->llimgr = LLIMGR_NULL_HANDLE;
	free_host_mem_for_sep(drvdata);
	dx_sep_power_exit(drvdata);
	detach_cc_interrupts(drvdata);
	iounmap(drvdata->cc_base);
	release_mem_region(drvdata->mem_start, drvdata->mem_size);
	sep_free_sysfs();
	kfree(drvdata);
	dev_set_drvdata(dev, NULL);
	return 0;		/* success */
}

#if defined(CONFIG_OF)

//static int __devinit sep_probe(struct platform_device *plat_dev)
static int sep_probe(struct platform_device *plat_dev)
{
	int rc, irq;
	struct resource res_mem;
	struct resource res_irq;

	SEP_LOG_DEBUG("sep_probe(%p)\n", plat_dev);

	rc = of_address_to_resource(plat_dev->dev.of_node, 0, &res_mem);
	if (unlikely(rc != 0)) {
		SEP_LOG_ERR("(OF) Failed getting IO memory resource\n");
		goto probe_error;
	}

	irq = of_irq_to_resource(plat_dev->dev.of_node, 0, &res_irq);
	if (unlikely(irq == NO_IRQ)) {
		SEP_LOG_ERR("(OF) Failed getting IRQ resource\n");
		goto probe_error;
	}
	/* FIXME exchange to DTB */
	plat_dev->dev.dma_mask = &plat_dev->dev.coherent_dma_mask;
	plat_dev->dev.coherent_dma_mask = DMA_BIT_MASK(sizeof(dma_addr_t) * 8);

	rc = sep_setup(&plat_dev->dev, &res_mem, &res_irq);

probe_error:
	return rc;
}

/* Match table for OF binding */
static const struct of_device_id dx_dev_of_match[]  __initconst =
	DX_DEV_OF_MATCH;
MODULE_DEVICE_TABLE(of, dx_dev_of_match);

#else /* !CONFIG_OF */

//static int __devinit sep_probe(struct platform_device *plat_dev)
static int sep_probe(struct platform_device *plat_dev)
{
	int rc = 0;
	struct resource *res_mem;
	struct resource *res_irq;

	SEP_LOG_DEBUG("sep_probe(%p)\n", plat_dev);

	res_mem = platform_get_resource(plat_dev, IORESOURCE_MEM, 0);
	if (unlikely(res_mem == NULL)) {
		SEP_LOG_ERR("Failed getting IO memory resource\n");
		rc = -ENODEV;
		goto probe_error;
	}
	res_irq = platform_get_resource(plat_dev, IORESOURCE_IRQ, 0);
	if (unlikely(res_irq == NULL)) {
		SEP_LOG_ERR("Failed getting IRQ resource\n");
		rc = -ENODEV;
		goto probe_error;
	}

	rc = sep_setup(&plat_dev->dev, res_mem, res_irq);

probe_error:
	return rc;
}

#endif /* CONFIG_OF */

#ifdef CONFIG_PM

static int sep_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct sep_drvdata *drvdata;
	int rc;

	SEP_LOG_DEBUG("sep_suspend(%p)\n", pdev);

	rc = dx_sep_power_state_set(DX_SEP_POWER_HIBERNATED);
	if (rc != 0) {
		SEP_LOG_ERR("Sep hibernation failed (%d)\n", rc);
		return rc;
	}

	drvdata = platform_get_drvdata(pdev);
	DX_SHUTDOWN_CC_CORE_CLOCK(drvdata);
	return 0;
}

static int sep_resume(struct platform_device *pdev)
{
	struct sep_drvdata *drvdata;

	//tz_ses_pm(ODIN_SES_PE, 0xc0000007);
	SEP_LOG_DEBUG("sep_resume(%p)\n", pdev);

	drvdata = platform_get_drvdata(pdev);
	DX_RESTART_CC_CORE_CLOCK(drvdata);
	return dx_sep_power_state_set(DX_SEP_POWER_ACTIVE);
}

#else

#define sep_suspend   NULL
#define sep_resume    NULL

#endif /* CONFIG_PM */

static struct platform_driver sep_platform_driver = {
	.probe = sep_probe,
	.remove = sep_remove,
	.suspend = sep_suspend,
	.resume  = sep_resume,
	.driver = {
		   .name = DRIVER_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = dx_dev_of_match,
#endif

	}
};

static int __init sep_module_init(void)
{
	int rc;

	sep_class = class_create(THIS_MODULE, "sep_ctl");
	rc = platform_driver_register(&sep_platform_driver);
	if (unlikely(rc != 0))
		goto failed;
	return 0;		/*success */

failed:
	class_destroy(sep_class);
	return rc;
}

static void __exit sep_module_cleanup(void)
{
	platform_driver_unregister(&sep_platform_driver);
	class_destroy(sep_class);
}

/* Entry points  */
#ifdef CONFIG_ARCH_ODIN
device_initcall(sep_module_init);
#else
module_init(sep_module_cleanup);
module_exit(sep_module_cleanup);
/* Module description */
MODULE_DESCRIPTION("Discretix " DRIVER_NAME " Driver");
MODULE_VERSION("0.7");
MODULE_AUTHOR("Discretix");
MODULE_LICENSE("GPL v2");
#endif
