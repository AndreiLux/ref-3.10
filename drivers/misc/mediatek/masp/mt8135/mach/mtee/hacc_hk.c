/* #include <mach/mt_sec_hal.h> */
#include "sec_error.h"
#include "sec_typedef.h"
/* #include "../hacc_mach.h" */

/******************************************************************************
 * DEBUG
 ******************************************************************************/
#define SEC_DEBUG                   (FALSE)
#define SMSG                        DBG_MSG
#if SEC_DEBUG
#define DMSG                        DBG_MSG
#else
#define DMSG
#endif

/******************************************************************************
 * HACC HW internal function
 ******************************************************************************/
void HACC_V3_Init(bool encode, const uint32 g_AC_CFG[])
{
	/*  */
}

void HACC_V3_Run(volatile uint32 *p_src, uint32 src_len, volatile uint32 *p_dst)
{
	/*  */
}

void HACC_V3_Terminate(void)
{
	/*  */
}
