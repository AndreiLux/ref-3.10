#include <linux/linkage.h>
#include <linux/init.h>

    __CPUINIT

CA15_CCI400_DVM_EN:
.long 0x10324000
CCI400_STATUS:
.long 0x1032000C
CA15_MISC_DBG:
.long 0x1020020C

ENTRY(mt_secondary_startup)
#ifndef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
    /* Enable CA15 cluster */
    /* We can not access in Non-secure world, otherwise system will crash */
    mrc     p15, 0, r0, c0, c0, 5   @ MPIDR
    ubfx    r0, r0, #8, #4          @ cluster
    cmp     r0, #0          @ A15 cluster?
    beq     cluster0
    ldr     r2, CA15_MISC_DBG
    ldr     r1, [r2]
    bic     r1, r1, #0x11
    str     r1, [r2]
    ldr     r2, CA15_CCI400_DVM_EN
    ldr     r1, [r2]
    orr     r1, r1, #3
    str     r1, [r2]
    ldr     r2, CCI400_STATUS
    b	    1f
0:  dsb
1:  ldr     r1, [r2]
    tst     r1, #1
    bne     0b
#endif /* !CONFIG_MTK_IN_HOUSE_TEE_SUPPORT */

cluster0:
    b secondary_startup
END(mt_secondary_startup)
