#include <mach/mt_typedefs.h>
#include <mach/mt_reg_base.h>
#include "sec_error.h"
#include "../hacc_mach.h"

/******************************************************************************
 * this file contains the hardware secure engine low-level operations
 * note that : all the functions in this file are ONLY for HACC internal usages.
 ******************************************************************************/

/******************************************************************************
 * CONSTANT DEFINITIONS
 ******************************************************************************/
#define MOD                         "HACC"
#define HACC_TEST                    (FALSE)

/******************************************************************************
 * DEBUG
 ******************************************************************************/
#define SEC_DEBUG                   (FALSE)
#define SMSG                        printk
#if SEC_DEBUG
#define DMSG                        printk
#else
#define DMSG
#endif

/******************************************************************************
 * EXTERNAL VARIABLE
 ******************************************************************************/
/*extern BOOL bHACC_HWWrapKeyInit;*/
/*extern BOOL bHACC_SWKeyInit;*/

/******************************************************************************
 * LOCAL VERIABLE
 ******************************************************************************/
/* static struct hacc_context hacc_ctx; */

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

#if HACC_TEST
static void hacc_test(void)
{
	U32 i, test_sz = HACC_AES_MAX_KEY_SZ * 24;
	U32 test_keysz = AES_KEY_256;
	U8 *test_src = (U8 *) HACC_AES_TEST_SRC;
	U8 *test_dst = (U8 *) HACC_AES_TEST_DST;
	U8 *test_tmp = (U8 *) HACC_AES_TEST_TMP;

	/* prepare data */
	for (i = 0; i < test_sz; i++) {
		test_src[i] = i + 1;
	}

	hacc_set_key(AES_HW_WRAP_KEY, test_keysz);
	hacc_do_aes(AES_ENC, test_src, test_tmp, test_sz);
	hacc_set_key(AES_HW_WRAP_KEY, test_keysz);
	hacc_do_aes(AES_DEC, test_tmp, test_dst, test_sz);

	for (i = 0; i < test_sz; i++) {
		if (test_src[i] != test_dst[i]) {
			DMSG("[%s] test_src[%d] = 0x%x != test_dst[%d] = 0x%x\n", MOD, i,
			     test_src[i], i, test_dst[i]);
			DMSG(0);
		}
	}
	DMSG("[%s] encrypt & descrypt unit test pass. (Key = %dbits)\n", MOD, test_keysz << 3);
}
#else
#define hacc_test()      do {} while (0)
#endif

/******************************************************************************
 * GLOBAL FUNCTIONS
 ******************************************************************************/
#if 0
static U32 hacc_set_cfg(AES_CFG *cfg)
{
	return SEC_OK;
}

static U32 hacc_set_mode(AES_MODE mode)
{
	return SEC_OK;
}
#endif

U32 hacc_set_key(AES_KEY_ID id, AES_KEY key)
{
	return SEC_OK;
}

U32 hacc_do_aes(AES_OPS ops, U8 *src, U8 *dst, U32 size)
{
	return SEC_OK;
}

U32 hacc_deinit(void)
{
	return SEC_OK;
}

U32 hacc_init(AES_KEY_SEED *keyseed)
{
	return SEC_OK;
}
