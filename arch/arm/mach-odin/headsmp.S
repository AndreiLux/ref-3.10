/*
 *  linux/arch/arm/plat-versatile/headsmp.S
 *
 *  Copyright (c) 2003 ARM Limited
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/linkage.h>
#include <linux/init.h>

	__INIT

/*
 * odin specific entry point for secondary CPUs.
 * This provides a "holding pen" into which all secondary cores are held
 * until we're ready for them to initialise.
 */
ENTRY(odin_secondary_startup)
/*
	movw	r1, #0x1800
	movt	r1, #0x3980
	ldr	r2, [r1]			@ GICD_ITARGETSR0
	and	r2, r2, #0xff
	clz	r2, r2
	mov	r3, #31
	sub	r0, r3, r2
*/
	mrc	p15, 0, r0, c0, c0, 5
	and	r0, r0, #0xffffff

	adr	r1, 1f
	ldmia	r1, {r2, r3}
	sub	r1, r1, r2
	add	r3, r3, r1
pen:
	ldr	r1, [r3]
	cmp	r1, r0
	bne	pen

	/*
	 * we've been released from the holding pen: secondary_stack
	 * should now contain the SVC stack for this core
	 */
	b	secondary_startup

	.align
1:	.long	.
	.long	pen_release
ENDPROC(odin_secondary_startup)
