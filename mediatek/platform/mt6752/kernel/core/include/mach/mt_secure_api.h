
#ifndef _MT_SECURE_API_H_
#define _MT_SECURE_API_H_

#include <asm/opcodes-sec.h>
#include <asm/opcodes-virt.h>
#include <mach/sync_write.h>

#if defined(CONFIG_ARM_PSCI)
/* Error Code */
#define SIP_SVC_E_SUCCESS               0
#define SIP_SVC_E_NOT_SUPPORTED         -1
#define SIP_SVC_E_INVALID_PARAMS        -2
#define SIP_SVC_E_INVALID_Range         -3
#define SIP_SVC_E_PERMISSION_DENY       -4

/* SIP SMC Call */
#define MTK_SIP_KERNEL_MCUSYS_WRITE_AARCH32         0x82000201
#define MTK_SIP_KERNEL_MCUSYS_ACCESS_COUNT_AARCH32  0x82000202

static noinline int mt_secure_call(u32 function_id, u32 arg0, u32 arg1, u32 arg2)
{
	register u32 reg0 __asm__("r0") = function_id;
	register u32 reg1 __asm__("r1") = arg0;
	register u32 reg2 __asm__("r2") = arg1;
	register u32 reg3 __asm__("r3") = arg2;
    int ret = 0;
    
    asm volatile(
            __SMC(0)
            : "+r" (reg0), "+r" (reg1), "+r" (reg2), "+r" (reg3));
    
	ret = reg0;
	return ret;
}
#endif

#define CONFIG_MCUSYS_WRITE_PROTECT

#if defined(CONFIG_MCUSYS_WRITE_PROTECT) && defined(CONFIG_ARM_PSCI)
#define SMC_IO_VIRT_TO_PHY(addr) (addr-0xF0000000+0x10000000)
#define mcusys_smc_write(addr, val) \
    mt_secure_call(MTK_SIP_KERNEL_MCUSYS_WRITE_AARCH32, SMC_IO_VIRT_TO_PHY(addr), val, 0)

#define mcusys_access_count() \
    mt_secure_call(MTK_SIP_KERNEL_MCUSYS_ACCESS_COUNT_AARCH32, 0, 0, 0)
#else
#define mcusys_smc_write(addr, val)      mt_reg_sync_writel(val, addr)
#define mcusys_access_count()            (0)
#endif

#endif /* _MT_SECURE_API_H_ */

