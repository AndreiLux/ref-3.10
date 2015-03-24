#include <linux/slab.h>
#include "tz_cross/ta_hacc.h"
#include <mach/sec_osal.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_sec_hal_tee.h>

/* #include "../hacc_mach.h" */
#include "kree/system.h"
#include "sec_log.h"
#include "sec_error.h"
#include "sec_typedef.h"

/******************************************************************************
 * Crypto Engine Test Driver Debug Control
 ******************************************************************************/
#define MOD                         "CE"

/******************************************************************************
 * Seed Definition
 ******************************************************************************/
#define _CRYPTO_SEED_LEN            (16)

/******************************************************************************
 * GLOBAL FUNCTIONS
 ******************************************************************************/
/* return the result of hwEnableClock ( )
   - TRUE  (1) means crypto engine init success
   - FALSE (0) means crypto engine init fail    */
unsigned char masp_hal_secure_algo_init(void)
{
	bool ret = TRUE;

	return ret;
}

/* return the result of hwDisableClock ( )
   - TRUE  (1) means crypto engine de-init success
   - FALSE (0) means crypto engine de-init fail    */
unsigned char masp_hal_secure_algo_deinit(void)
{
	bool ret = TRUE;

	return ret;
}

/******************************************************************************
 * CRYPTO ENGINE EXPORTED APIs
 ******************************************************************************/
/* perform crypto operation
   @ Direction   : TRUE  (1) means encrypt
		       FALSE (0) means decrypt
   @ ContentAddr : input source address
   @ ContentLen  : input source length
   @ CustomSeed  : customization seed for crypto engine
   @ ResText     : output destination address */
void masp_hal_secure_algo(unsigned char Direction, unsigned int ContentAddr,
			  unsigned int ContentLen, unsigned char *CustomSeed,
			  unsigned char *ResText)
{
	/* uint32 err; */
	/* uchar *src, *dst, *seed; */
	/* uint32 i = 0; */

	TZ_RESULT ret;
	KREE_SESSION_HANDLE cryptoSession;
	MTEEC_PARAM param[4];
	uint32_t paramTypes;
	/* uint32_t result; */
	ta_secure_algo_data *secureAlgoData;

	/* Bind Crypto test section with TZ_CRYPTO_TA_UUID */
	ret = KREE_CreateSession(TZ_CRYPTO_TA_UUID, &cryptoSession);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("CreateSession error 0x%x\n", ret);
		goto _error;
	}

	/* TA service call */
	secureAlgoData = kmalloc(sizeof(secureAlgoData), GFP_KERNEL);
	if (!secureAlgoData) {
		pr_info("Malloc secureAlgoData error\n");
		goto _error;
	}

	paramTypes = TZ_ParamTypes2(TZPT_MEM_INPUT, TZPT_VALUE_OUTPUT);

	secureAlgoData->direction = Direction;
	secureAlgoData->contentAddr = ContentAddr;
	secureAlgoData->contentLen = ContentLen;
	secureAlgoData->customSeed = CustomSeed;
	secureAlgoData->resText = ResText;

	param[0].mem.buffer = secureAlgoData;
	param[0].mem.size = sizeof(secureAlgoData);

	ret = KREE_TeeServiceCall(cryptoSession, TZCMD_SECURE_ALGO, paramTypes, param);
	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("ServiceCall error %d\n", ret);
		goto _error;
	}

	/* Close session of TA_Crypto_TEST */
	ret = KREE_CloseSession(cryptoSession);

	if (ret != TZ_RESULT_SUCCESS) {
		pr_info("CloseSession error %d\n", ret);
		goto _error;
	}

/* Get lock in TEE */
#if 0
	/* try to get hacc lock */
	do {
		/* If the semaphore is successfully acquired, this function returns 0. */
		err = osal_hacc_lock();
	} while (0 != err);
#endif

#if 0
	/* initialize hacc crypto configuration */
	seed = (uchar *) CustomSeed;
	err = masp_hal_sp_hacc_init(seed, _CRYPTO_SEED_LEN);

	if (SEC_OK != err) {
		goto _error;
	}

	/* initialize source and destination address */
	src = (uchar *) ContentAddr;
	dst = (uchar *) ResText;

	/* according to input parameter to encrypt or decrypt */
	switch (Direction) {
	case TRUE:
		/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
		/* ! CCCI driver already got HACC lock ! */
		/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
		dst = masp_hal_sp_hacc_enc((uchar *) src, ContentLen, TRUE, HACC_USER3, FALSE);
		break;

	case FALSE:
		/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
		/* ! CCCI driver already got HACC lock ! */
		/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
		dst = masp_hal_sp_hacc_dec((uchar *) src, ContentLen, TRUE, HACC_USER3, FALSE);
		break;

	default:
		err = ERR_KER_CRYPTO_INVALID_MODE;
		goto _error;
	}


	/* copy result */
	for (i = 0; i < ContentLen; i++) {
		*(ResText + i) = *(dst + i);
	}
#endif

/*Release lock in TEE*/
#if 0
	/* try to release hacc lock */
	osal_hacc_unlock();
#endif

	return;

 _error:

/*Release lock in TEE*/
#if 0
	/* try to release hacc lock */
	osal_hacc_unlock();
#endif

	SMSG(TRUE, "[%s] masp_hal_secure_algo error (0x%x)\n", MOD, ret);
	ASSERT(0);
}
