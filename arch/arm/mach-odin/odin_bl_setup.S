/*
 * arch/arm/include/asm/odin_bl_setup.S
 *
 * Created by:  Dave Martin, 2012-06-22
 * Copyright:   (C) 2012  Linaro Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/linkage.h>

ENTRY(odin_bl_power_up_setup)

	cmp	r0, #0			@ check affinity level
	beq	2f

/*
 * Enable cluster-level coherency, in preparation for turning on the MMU.
 * The ACTLR SMP bit does not need to be set here, because cpu_resume()
 * already restores that.
 *
 * A15/A7 may not require explicit L2 invalidation on reset, dependent
 * on hardware integration decisions.
 * For now, this code assumes that L2 is either already invalidated,
 * or invalidation is not required.
 */

	b	cci_enable_port_for_self

2:	@ Implementation-specific local CPU setup operations should go here,
	@ if any.  In this case, there is nothing to do.

#ifdef CONFIG_ARM_LPAE
	mov r3, #0
	mcrr p15, 0, r3, r3, c2    @ Clear TTBR0_64
	mcrr p15, 1, r3, r3, c2    @ Clear TTBR1_64
#endif

	bx	lr

ENDPROC(odin_bl_power_up_setup)
