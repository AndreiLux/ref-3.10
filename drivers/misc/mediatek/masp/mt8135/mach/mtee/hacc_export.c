#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include "tz_cross/ta_hacc.h"
#include <mach/sec_osal.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_sec_hal_tee.h>
#include "sec_osal_light.h"
#include "sec_boot_lib.h"
#include "sec_error.h"

#include "../hacc_mach.h"
#include "tz_cross/trustzone.h"
#include "kree/system.h"

/******************************************************************************
 * This file provide the HACC operation function to secure library
 * All the functions should be general ...
 ******************************************************************************/
#define MOD                         "TEE"
#define TEE_PARAM_MEM_LIMIT         (4096)

/******************************************************************************
 *  INTERNAL ENGINE
 ******************************************************************************/
static uchar *sp_hacc_internal(uchar *buf, uint32 size, BOOL bAC, HACC_USER user, BOOL bDoLock,
			       AES_OPS aes_type, BOOL bEn)
{
	TZ_RESULT ret;
	KREE_SESSION_HANDLE cryptoSession;
	MTEEC_PARAM param[4];
	uint32_t paramTypes;
	/* uint32_t result; */
	ta_crypto_data *cryptoData = 0;
	uchar *outBuf = 0;
	uchar *bufToTA = 0;

	pr_info("__FILE__: %s\n", __FILE__);
	pr_info("__func__: %s\n", __func__);
	pr_info("__LINE__: %d\n", __LINE__);

	/* Bind Crypto test section with TZ_CRYPTO_TA_UUID */
	ret = KREE_CreateSession(TZ_CRYPTO_TA_UUID, &cryptoSession);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("CreateSession error 0x%x\n", ret);
		goto _err;
	}

	/* TA service call */
	cryptoData = (ta_crypto_data *) kmalloc(sizeof(ta_crypto_data), GFP_KERNEL);
	if (!cryptoData) {
		pr_info("kmalloc cryptoData error\n");
		goto _err;
	}

	paramTypes = TZ_ParamTypes3(TZPT_MEM_INPUT, TZPT_MEM_INPUT, TZPT_MEM_OUTPUT);

	cryptoData->size = size;
	cryptoData->bAC = bAC;
	cryptoData->user = user;
	cryptoData->bDoLock = 1;	/* Forced to do Lock */
	cryptoData->aes_type = aes_type;
	cryptoData->bEn = bEn;

	/* In buffer */
	if (size > TEE_PARAM_MEM_LIMIT) {
		pr_info("Enc/Dec buffer size is over 4KB, we need to kmalloc buffer to use.\n");
		bufToTA = (uchar *) kmalloc(size, GFP_KERNEL);
		if (!bufToTA) {
			pr_info("kmalloc bufToTA error\n");
			goto _err;
		}
		memcpy(bufToTA, buf, size);
		param[0].mem.buffer = bufToTA;
	} else {
		param[0].mem.buffer = buf;
	}
	param[0].mem.size = size;
	param[1].mem.buffer = cryptoData;
	param[1].mem.size = sizeof(cryptoData);

	/* Out buffer */
	outBuf = (uchar *) kmalloc(size, GFP_KERNEL);
	if (!outBuf) {
		pr_info("kmalloc outBuf error\n");
		goto _err;
	}
	param[2].mem.buffer = outBuf;
	param[2].mem.size = size;

	pr_info("Start to do service call - TZCMD_HACC_INTERNAL\n");
	ret = KREE_TeeServiceCall(cryptoSession, TZCMD_HACC_INTERNAL, paramTypes, param);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("ServiceCall error %d\n", ret);
		goto _err;
	}
	/* buf = param[2].mem.buffer; */
	memcpy(buf, param[2].mem.buffer, size);

	pr_info("kfree outBuf\n");
	if (outBuf) {
		kfree(outBuf);
	}
	if (cryptoData) {
		kfree(cryptoData);
	}
	if (bufToTA) {
		kfree(bufToTA);
	}

	pr_info("Start to close session\n");
	/* Close session of TA_Crypto_TEST */
	ret = KREE_CloseSession(cryptoSession);

	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("CloseSession error %d\n", ret);
		goto _err;
	}

	return buf;

 _err:
	SMSG(true, "[%s] HACC Fail (0x%x)\n", MOD, ret);
	if (outBuf) {
		kfree(outBuf);
	}
	if (cryptoData) {
		kfree(cryptoData);
	}
	if (bufToTA) {
		kfree(bufToTA);
	}
	ASSERT(0);

	return buf;
}

/******************************************************************************
 *  ENCRYPTION
 ******************************************************************************/
unsigned char *masp_hal_sp_hacc_enc(unsigned char *buf, unsigned int size, unsigned char bAC,
				    HACC_USER user, unsigned char bDoLock)
{
	return sp_hacc_internal(buf, size, TRUE, user, bDoLock, AES_ENC, TRUE);
}


/******************************************************************************
 *  DECRYPTION
 ******************************************************************************/
unsigned char *masp_hal_sp_hacc_dec(unsigned char *buf, unsigned int size, unsigned char bAC,
				    HACC_USER user, unsigned char bDoLock)
{
	return sp_hacc_internal(buf, size, TRUE, user, bDoLock, AES_DEC, FALSE);
}

/******************************************************************************
 *  HACC BLK SIZE
 ******************************************************************************/
unsigned int masp_hal_sp_hacc_blk_sz(void)
{
	return AES_BLK_SZ;
}

/******************************************************************************
 *  HACC INITIALIZATION
 ******************************************************************************/
unsigned int masp_hal_sp_hacc_init(unsigned char *sec_seed, unsigned int size)
{
	TZ_RESULT ret;
	KREE_SESSION_HANDLE cryptoSession;
	MTEEC_PARAM param[4];
	uint32_t paramTypes;
	uint32_t result;

	pr_info("__FILE__: %s\n", __FILE__);
	pr_info("__func__: %s\n", __func__);
	pr_info("__LINE__: %d\n", __LINE__);

	/* Bind Crypto test section with TZ_CRYPTO_TA_UUID */
	ret = KREE_CreateSession(TZ_CRYPTO_TA_UUID, &cryptoSession);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("CreateSession error 0x%x\n", ret);
		return ret;
	}

	/* TA service call */
	paramTypes = TZ_ParamTypes3(TZPT_MEM_INPUT, TZPT_VALUE_INPUT, TZPT_VALUE_OUTPUT);

	param[0].mem.buffer = sec_seed;
	param[0].mem.size = _CRYPTO_SEED_LEN;
	param[1].value.a = size;

	pr_info("Start to do service call - TZCMD_HACC_INIT\n");
	ret = KREE_TeeServiceCall(cryptoSession, TZCMD_HACC_INIT, paramTypes, param);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("ServiceCall error %d\n", ret);
		return ret;
	}
	result = param[2].value.a;

	pr_info("Start to close session\n");
	/* Close session of TA_Crypto_TEST */
	ret = KREE_CloseSession(cryptoSession);

	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("CloseSession error %d\n", ret);
		return ret;
	}

	return result;
}
