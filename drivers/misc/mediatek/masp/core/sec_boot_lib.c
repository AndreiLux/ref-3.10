/******************************************************************************
 *  INCLUDE LIBRARY
 ******************************************************************************/
#include "sec_boot_lib.h"

/******************************************************************************
 * CONSTANT DEFINITIONS
 ******************************************************************************/
#define MOD                         "ASF"

/**************************************************************************
 *  INTERNAL UTILITES
 **************************************************************************/
void *mcpy(void *dest, const void *src, int cnt)
{
	char *tmp = dest;
	const char *s = src;

	while (cnt--)
		*tmp++ = *s++;
	return dest;
}

int mcmp(const void *cs, const void *ct, int cnt)
{
	const uchar *su1, *su2;
	int res = 0;

	for (su1 = cs, su2 = ct; 0 < cnt; ++su1, ++su2, cnt--) {
		res = *su1 - *su2;
		if (res != 0)
			break;
	}
	return res;
}

void dump_buf(uchar *buf, uint32 len)
{
	uint32 i = 1;

	for (i = 1; i < len + 1; i++) {
		if (0 != buf[i - 1]) {
			SMSG(true, "0x%x,", buf[i - 1]);

			if (0 == i % 8)
				SMSG(true, "\n");
		}
	}

	SMSG(true, "\n");
}

/******************************************************************************
 *  IMAGE HASH DUMP
 ******************************************************************************/
void img_hash_dump(uchar *buf, uint32 size)
{
	uint32 i = 0;

	for (i = 0; i < size; i++) {
		if (i % 4 == 0)
			SMSG(true, "\n");

		if (buf[i] < 0x10) {
			do {
				SMSG(true, "0x0%x, ", buf[i]);
			} while (0);
		} else {
			do {
				SMSG(true, "0x%x, ", buf[i]);
			} while (0);
		}
	}
}

/******************************************************************************
 *  IMAGE HASH CALCULATION
 ******************************************************************************/
void img_hash_compute(uchar *buf, uint32 size)
{
	SEC_ASSERT(0);
}

/******************************************************************************
 *  IMAGE HASH UPDATE
 ******************************************************************************/
uint32 img_hash_update(char *part_name)
{
	uint32 ret = SEC_OK;

	SEC_ASSERT(0);

	return ret;
}


/******************************************************************************
 *  IMAGE HASH CHECK
 ******************************************************************************/
uint32 img_hash_check(char *part_name)
{
	uint32 ret = SEC_OK;

	SEC_ASSERT(0);

	return ret;
}


/******************************************************************************
 *  GET BUILD INFORMATION
 ******************************************************************************/
char *asf_get_build_info(void)
{
	return BUILD_TIME "" BUILD_BRANCH;
}
