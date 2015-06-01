	.arch armv7-a
	.eabi_attribute 27, 3
	.fpu vfp
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 2
	.eabi_attribute 34, 1
	.eabi_attribute 18, 4
	.file	"hacc_export.c"
	.text
.Ltext0:
	.align	2
	.type	sp_hacc_internal.isra.0, %function
sp_hacc_internal.isra.0:
.LFB191:
	.file 1 "alps/kernel-3.10/mediatek/platform/mt6752/kernel/drivers/masp/mach/hacc_export.c"
	.loc 1 89 0
	@ args = 8, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
.LVL0:
	mov	ip, sp
.LCFI0:
.LVL1:
	stmfd	sp!, {r4, r5, r6, r7, fp, ip, lr, pc}
.LCFI1:
	sub	fp, ip, #4
.LCFI2:
	sub	sp, sp, #8
.LVL2:
	.loc 1 96 0
	cmp	r3, #1
	.loc 1 89 0
	mov	r5, r3
	mov	r4, r0
	mov	r6, r1
	ldrb	r7, [fp, #8]	@ zero_extendqisi2
	.loc 1 96 0
	beq	.L2
.LVL3:
.L10:
	.loc 1 111 0
	cmp	r2, #3
	ldrls	pc, [pc, r2, asl #2]
	b	.L14
.L5:
	.word	.L4
	.word	.L6
	.word	.L7
	.word	.L8
.L8:
	.loc 1 161 0
	adds	r0, r7, #0
	ldr	r1, .L21
	movne	r0, #1
.L18:
	bl	HACC_V3_Init
.LVL4:
	.loc 1 163 0
	mov	r0, r4
	mov	r1, r6
	mov	r2, r4
	bl	HACC_V3_Run
.LVL5:
	.loc 1 165 0
	bl	HACC_V3_Terminate
.LVL6:
.L11:
	.loc 1 176 0
	cmp	r5, #1
	beq	.L20
	.loc 1 194 0
	mov	r0, r4
	sub	sp, fp, #28
.LCFI3:
	ldmfd	sp, {r4, r5, r6, r7, fp, sp, pc}
.LVL7:
.L7:
.LCFI4:
	.loc 1 139 0
	movw	r3, #:lower16:.LANCHOR1
	movt	r3, #:upper16:.LANCHOR1
	ldrb	r3, [r3]	@ zero_extendqisi2
	cmp	r3, #0
	beq	.L16
	.loc 1 146 0
	mov	r0, #2
	mov	r1, #32
	bl	hacc_set_key
.LVL8:
	.loc 1 148 0
	subs	r2, r0, #0
	bne	.L3
	.loc 1 151 0
	mov	r2, r4
	bic	r3, r6, #127
	ldr	r0, [fp, #4]
.LVL9:
	mov	r1, r4
	bl	hacc_do_aes
.LVL10:
	.loc 1 153 0
	subs	r2, r0, #0
	beq	.L11
.LVL11:
.L3:
	.loc 1 184 0
	cmp	r5, #1
	beq	.L9
.LVL12:
.L13:
	.loc 1 189 0
	movw	r0, #:lower16:.LC0
	movw	r1, #:lower16:.LC1
	movt	r0, #:upper16:.LC0
	movt	r1, #:upper16:.LC1
	bl	printk
.LVL13:
	.loc 1 191 0
#APP
@ 191   "alps/kernel-3.10/mediatek/platform/mt6752/kernel/drivers/masp/mach/hacc_export.c" 1
	.word 0xe7f001f2
@ 0 "" 2
.L6:
	.loc 1 128 0
	adds	r0, r7, #0
	ldr	r1, .L21+4
	movne	r0, #1
	b	.L18
.L4:
	.loc 1 117 0
	adds	r0, r7, #0
	movw	r1, #:lower16:.LANCHOR0
	movt	r1, #:upper16:.LANCHOR0
	movne	r0, #1
	b	.L18
.L20:
	.loc 1 178 0
	bl	osal_hacc_unlock
.LVL14:
	.loc 1 194 0
	mov	r0, r4
	sub	sp, fp, #28
.LCFI5:
	ldmfd	sp, {r4, r5, r6, r7, fp, sp, pc}
.LVL15:
.L2:
.LCFI6:
	.loc 1 99 0
	str	r2, [fp, #-32]
	bl	osal_hacc_lock
.LVL16:
	.loc 1 101 0
	ldr	r2, [fp, #-32]
	cmp	r0, #0
	beq	.L10
	.loc 1 103 0
	movw	r2, #24586
.LVL17:
.L9:
	.loc 1 186 0
	str	r2, [fp, #-32]
	bl	osal_hacc_unlock
.LVL18:
	ldr	r2, [fp, #-32]
	b	.L13
.L16:
	.loc 1 141 0
	movw	r2, #24583
	b	.L3
.L14:
	.loc 1 169 0
	movw	r2, #24588
	b	.L3
.L22:
	.align	2
.L21:
	.word	.LANCHOR0+64
	.word	.LANCHOR0+32
.LFE191:
	.size	sp_hacc_internal.isra.0, .-sp_hacc_internal.isra.0
	.align	2
	.global	masp_hal_sp_hacc_enc
	.type	masp_hal_sp_hacc_enc, %function
masp_hal_sp_hacc_enc:
.LFB187:
	.loc 1 200 0
	@ args = 4, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
.LVL19:
	mov	ip, sp
.LCFI7:
.LVL20:
	stmfd	sp!, {fp, ip, lr, pc}
.LCFI8:
	sub	fp, ip, #4
.LCFI9:
	sub	sp, sp, #8
	.loc 1 200 0
	ldrb	lr, [fp, #4]	@ zero_extendqisi2
	.loc 1 201 0
	mov	ip, #1
.LVL21:
	mov	r2, r3
.LVL22:
	str	ip, [sp]
	str	ip, [sp, #4]
	mov	r3, lr
.LVL23:
	bl	sp_hacc_internal.isra.0
.LVL24:
	.loc 1 202 0
	sub	sp, fp, #12
.LCFI10:
	ldmfd	sp, {fp, sp, pc}
.LFE187:
	.size	masp_hal_sp_hacc_enc, .-masp_hal_sp_hacc_enc
	.align	2
	.global	masp_hal_sp_hacc_dec
	.type	masp_hal_sp_hacc_dec, %function
masp_hal_sp_hacc_dec:
.LFB188:
	.loc 1 209 0
	@ args = 4, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
.LVL25:
	mov	ip, sp
.LCFI11:
.LVL26:
	stmfd	sp!, {fp, ip, lr, pc}
.LCFI12:
	sub	fp, ip, #4
.LCFI13:
	sub	sp, sp, #8
	.loc 1 209 0
	ldrb	lr, [fp, #4]	@ zero_extendqisi2
	.loc 1 210 0
	mov	ip, #0
.LVL27:
	mov	r2, r3
.LVL28:
	str	ip, [sp]
	str	ip, [sp, #4]
	mov	r3, lr
.LVL29:
	bl	sp_hacc_internal.isra.0
.LVL30:
	.loc 1 211 0
	sub	sp, fp, #12
.LCFI14:
	ldmfd	sp, {fp, sp, pc}
.LFE188:
	.size	masp_hal_sp_hacc_dec, .-masp_hal_sp_hacc_dec
	.align	2
	.global	masp_hal_sp_hacc_blk_sz
	.type	masp_hal_sp_hacc_blk_sz, %function
masp_hal_sp_hacc_blk_sz:
.LFB189:
	.loc 1 217 0
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
.LCFI15:
	stmfd	sp!, {fp, ip, lr, pc}
.LCFI16:
	sub	fp, ip, #4
.LCFI17:
	.loc 1 219 0
	mov	r0, #16
	ldmfd	sp, {fp, sp, pc}
.LFE189:
	.size	masp_hal_sp_hacc_blk_sz, .-masp_hal_sp_hacc_blk_sz
	.align	2
	.global	masp_hal_sp_hacc_init
	.type	masp_hal_sp_hacc_init, %function
masp_hal_sp_hacc_init:
.LFB190:
	.loc 1 225 0
	@ args = 0, pretend = 0, frame = 40
	@ frame_needed = 1, uses_anonymous_args = 0
.LVL31:
	mov	ip, sp
.LCFI18:
	stmfd	sp!, {fp, ip, lr, pc}
.LCFI19:
	sub	fp, ip, #4
.LCFI20:
	sub	sp, sp, #48
.LVL32:
	.loc 1 229 0
	cmp	r1, #16
	.loc 1 231 0
	movwne	r0, #24580
.LVL33:
	.loc 1 229 0
	beq	.L32
.LVL34:
.L27:
	.loc 1 244 0
	sub	sp, fp, #12
.LCFI21:
	ldmfd	sp, {fp, sp, pc}
.LVL35:
.L32:
.LCFI22:
	sub	r1, fp, #12
.LVL36:
	.loc 1 235 0
	mov	r3, #0
	.loc 1 234 0
	mov	r2, #32
	str	r2, [fp, #-48]
.LVL37:
.L29:
	sub	ip, fp, #48
	.loc 1 237 0 discriminator 2
	ldrb	r2, [r0, r3]	@ zero_extendqisi2
	add	lr, ip, r3
	.loc 1 235 0 discriminator 2
	add	r3, r3, #1
.LVL38:
	cmp	r3, #16
	.loc 1 238 0 discriminator 2
	add	ip, r2, #1
	.loc 1 237 0 discriminator 2
	strb	r2, [lr, #4]
	.loc 1 238 0 discriminator 2
	strb	ip, [r1, #-1]!
	.loc 1 235 0 discriminator 2
	bne	.L29
	.loc 1 241 0 discriminator 1
	ldrb	ip, [fp, #-41]	@ zero_extendqisi2
	movw	r0, #:lower16:.LC2
	ldrb	r1, [fp, #-44]	@ zero_extendqisi2
	movt	r0, #:upper16:.LC2
	ldrb	r2, [fp, #-43]	@ zero_extendqisi2
	ldrb	r3, [fp, #-42]	@ zero_extendqisi2
.LVL39:
	str	ip, [sp]
	bl	printk
.LVL40:
	.loc 1 243 0 discriminator 1
	sub	r0, fp, #48
	bl	hacc_init
.LVL41:
	b	.L27
.LFE190:
	.size	masp_hal_sp_hacc_init, .-masp_hal_sp_hacc_init
	.global	bHACC_SWKeyInit
	.global	bHACC_HWWrapKeyInit
	.section	.rodata
	.align	2
.LANCHOR0 = . + 0
	.type	g_HACC_CFG_1, %object
	.size	g_HACC_CFG_1, 32
g_HACC_CFG_1:
	.word	-1630272512
	.word	15238305
	.word	-470776899
	.word	793669002
	.word	-8155556
	.word	-381640477
	.word	-1924272954
	.word	1174145417
	.type	g_HACC_CFG_2, %object
	.size	g_HACC_CFG_2, 32
g_HACC_CFG_2:
	.word	-1437324070
	.word	1431445780
	.word	-470776899
	.word	1431445780
	.word	-1437324070
	.word	-1437324070
	.word	1431445780
	.word	-1437324070
	.type	g_HACC_CFG_3, %object
	.size	g_HACC_CFG_3, 32
g_HACC_CFG_3:
	.word	646231696
	.word	-345528130
	.word	-1592585140
	.word	393941525
	.word	378269286
	.word	310687364
	.word	-549728710
	.word	561999778
	.section	.rodata.str1.4,"aMS",%progbits,1
	.align	2
.LC0:
	.ascii	"[%s] HACC Fail (0x%x)\012\000"
	.space	1
.LC1:
	.ascii	"ASF\000"
.LC2:
	.ascii	"0x%x,0x%x,0x%x,0x%x\012\000"
	.bss
.LANCHOR1 = . + 0
	.type	bHACC_HWWrapKeyInit, %object
	.size	bHACC_HWWrapKeyInit, 1
bHACC_HWWrapKeyInit:
	.space	1
	.type	bHACC_SWKeyInit, %object
	.size	bHACC_SWKeyInit, 1
bHACC_SWKeyInit:
	.space	1
	.section	.debug_frame,"",%progbits
.Lframe0:
	.4byte	.LECIE0-.LSCIE0
.LSCIE0:
	.4byte	0xffffffff
	.byte	0x1
	.ascii	"\000"
	.uleb128 0x1
	.sleb128 -4
	.byte	0xe
	.byte	0xc
	.uleb128 0xd
	.uleb128 0
	.align	2
.LECIE0:
.LSFDE0:
	.4byte	.LEFDE0-.LASFDE0
.LASFDE0:
	.4byte	.Lframe0
	.4byte	.LFB191
	.4byte	.LFE191-.LFB191
	.byte	0x4
	.4byte	.LCFI0-.LFB191
	.byte	0xd
	.uleb128 0xc
	.byte	0x4
	.4byte	.LCFI1-.LCFI0
	.byte	0x84
	.uleb128 0x8
	.byte	0x85
	.uleb128 0x7
	.byte	0x86
	.uleb128 0x6
	.byte	0x87
	.uleb128 0x5
	.byte	0x8b
	.uleb128 0x4
	.byte	0x8d
	.uleb128 0x3
	.byte	0x8e
	.uleb128 0x2
	.byte	0x4
	.4byte	.LCFI2-.LCFI1
	.byte	0xc
	.uleb128 0xb
	.uleb128 0x4
	.byte	0x4
	.4byte	.LCFI3-.LCFI2
	.byte	0xa
	.byte	0xc
	.uleb128 0xd
	.uleb128 0x20
	.byte	0x4
	.4byte	.LCFI4-.LCFI3
	.byte	0xb
	.byte	0x4
	.4byte	.LCFI5-.LCFI4
	.byte	0xa
	.byte	0xc
	.uleb128 0xd
	.uleb128 0x20
	.byte	0x4
	.4byte	.LCFI6-.LCFI5
	.byte	0xb
	.align	2
.LEFDE0:
.LSFDE2:
	.4byte	.LEFDE2-.LASFDE2
.LASFDE2:
	.4byte	.Lframe0
	.4byte	.LFB187
	.4byte	.LFE187-.LFB187
	.byte	0x4
	.4byte	.LCFI7-.LFB187
	.byte	0xd
	.uleb128 0xc
	.byte	0x4
	.4byte	.LCFI8-.LCFI7
	.byte	0x8b
	.uleb128 0x4
	.byte	0x8d
	.uleb128 0x3
	.byte	0x8e
	.uleb128 0x2
	.byte	0x4
	.4byte	.LCFI9-.LCFI8
	.byte	0xc
	.uleb128 0xb
	.uleb128 0x4
	.byte	0x4
	.4byte	.LCFI10-.LCFI9
	.byte	0xc
	.uleb128 0xd
	.uleb128 0x10
	.align	2
.LEFDE2:
.LSFDE4:
	.4byte	.LEFDE4-.LASFDE4
.LASFDE4:
	.4byte	.Lframe0
	.4byte	.LFB188
	.4byte	.LFE188-.LFB188
	.byte	0x4
	.4byte	.LCFI11-.LFB188
	.byte	0xd
	.uleb128 0xc
	.byte	0x4
	.4byte	.LCFI12-.LCFI11
	.byte	0x8b
	.uleb128 0x4
	.byte	0x8d
	.uleb128 0x3
	.byte	0x8e
	.uleb128 0x2
	.byte	0x4
	.4byte	.LCFI13-.LCFI12
	.byte	0xc
	.uleb128 0xb
	.uleb128 0x4
	.byte	0x4
	.4byte	.LCFI14-.LCFI13
	.byte	0xc
	.uleb128 0xd
	.uleb128 0x10
	.align	2
.LEFDE4:
.LSFDE6:
	.4byte	.LEFDE6-.LASFDE6
.LASFDE6:
	.4byte	.Lframe0
	.4byte	.LFB189
	.4byte	.LFE189-.LFB189
	.byte	0x4
	.4byte	.LCFI15-.LFB189
	.byte	0xd
	.uleb128 0xc
	.byte	0x4
	.4byte	.LCFI16-.LCFI15
	.byte	0x8b
	.uleb128 0x4
	.byte	0x8d
	.uleb128 0x3
	.byte	0x8e
	.uleb128 0x2
	.byte	0x4
	.4byte	.LCFI17-.LCFI16
	.byte	0xc
	.uleb128 0xb
	.uleb128 0x4
	.align	2
.LEFDE6:
.LSFDE8:
	.4byte	.LEFDE8-.LASFDE8
.LASFDE8:
	.4byte	.Lframe0
	.4byte	.LFB190
	.4byte	.LFE190-.LFB190
	.byte	0x4
	.4byte	.LCFI18-.LFB190
	.byte	0xd
	.uleb128 0xc
	.byte	0x4
	.4byte	.LCFI19-.LCFI18
	.byte	0x8b
	.uleb128 0x4
	.byte	0x8d
	.uleb128 0x3
	.byte	0x8e
	.uleb128 0x2
	.byte	0x4
	.4byte	.LCFI20-.LCFI19
	.byte	0xc
	.uleb128 0xb
	.uleb128 0x4
	.byte	0x4
	.4byte	.LCFI21-.LCFI20
	.byte	0xa
	.byte	0xc
	.uleb128 0xd
	.uleb128 0x10
	.byte	0x4
	.4byte	.LCFI22-.LCFI21
	.byte	0xb
	.align	2
.LEFDE8:
	.text
.Letext0:
	.file 2 "alps/kernel-3.10/../mediatek/platform/mt6752/kernel/core/include/mach/mt_typedefs.h"
	.file 3 "alps/kernel-3.10/../mediatek/kernel/include/mach/mt_sec_hal.h"
	.file 4 "alps/kernel-3.10/mediatek/platform/mt6752/kernel/drivers/masp/asf/asf_inc/sec_typedef.h"
	.file 5 "alps/kernel-3.10/mediatek/platform/mt6752/kernel/drivers/masp/mach/hacc_mach.h"
	.file 6 "alps/kernel-3.10/arch/arm/include/asm/hwcap.h"
	.file 7 "alps/kernel-3.10/include/linux/printk.h"
	.file 8 "alps/kernel-3.10/include/linux/kernel.h"
	.file 9 "alps/kernel-3.10/../mediatek/kernel/include/mach/sec_osal.h"
	.section	.debug_info,"",%progbits
.Ldebug_info0:
	.4byte	0x6ac
	.2byte	0x2
	.4byte	.Ldebug_abbrev0
	.byte	0x4
	.uleb128 0x1
	.4byte	.LASF59
	.byte	0x1
	.4byte	.LASF60
	.4byte	.LASF61
	.4byte	.Ltext0
	.4byte	.Letext0
	.4byte	.Ldebug_line0
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.4byte	.LASF0
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.4byte	.LASF1
	.uleb128 0x3
	.byte	0x4
	.4byte	0x39
	.uleb128 0x4
	.4byte	0x3e
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.4byte	.LASF2
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.4byte	.LASF3
	.uleb128 0x2
	.byte	0x1
	.byte	0x6
	.4byte	.LASF4
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.4byte	.LASF5
	.uleb128 0x2
	.byte	0x2
	.byte	0x5
	.4byte	.LASF6
	.uleb128 0x2
	.byte	0x2
	.byte	0x7
	.4byte	.LASF7
	.uleb128 0x5
	.byte	0x4
	.byte	0x5
	.ascii	"int\000"
	.uleb128 0x2
	.byte	0x8
	.byte	0x5
	.4byte	.LASF8
	.uleb128 0x2
	.byte	0x8
	.byte	0x7
	.4byte	.LASF9
	.uleb128 0x2
	.byte	0x4
	.byte	0x5
	.4byte	.LASF10
	.uleb128 0x2
	.byte	0x1
	.byte	0x2
	.4byte	.LASF11
	.uleb128 0x3
	.byte	0x4
	.4byte	0x53
	.uleb128 0x6
	.ascii	"U8\000"
	.byte	0x2
	.byte	0x2a
	.4byte	0x53
	.uleb128 0x6
	.ascii	"U32\000"
	.byte	0x2
	.byte	0x2e
	.4byte	0x45
	.uleb128 0x2
	.byte	0x4
	.byte	0x4
	.4byte	.LASF12
	.uleb128 0x7
	.4byte	.LASF13
	.byte	0x2
	.byte	0x68
	.4byte	0x53
	.uleb128 0x8
	.byte	0x4
	.byte	0x3
	.byte	0x12
	.4byte	0xd9
	.uleb128 0x9
	.4byte	.LASF14
	.sleb128 0
	.uleb128 0x9
	.4byte	.LASF15
	.sleb128 1
	.uleb128 0x9
	.4byte	.LASF16
	.sleb128 2
	.uleb128 0x9
	.4byte	.LASF17
	.sleb128 3
	.byte	0
	.uleb128 0x7
	.4byte	.LASF18
	.byte	0x3
	.byte	0x17
	.4byte	0xb8
	.uleb128 0x7
	.4byte	.LASF19
	.byte	0x4
	.byte	0xe
	.4byte	0x45
	.uleb128 0x7
	.4byte	.LASF20
	.byte	0x4
	.byte	0xf
	.4byte	0x53
	.uleb128 0x8
	.byte	0x4
	.byte	0x5
	.byte	0x7a
	.4byte	0x10f
	.uleb128 0x9
	.4byte	.LASF21
	.sleb128 0
	.uleb128 0x9
	.4byte	.LASF22
	.sleb128 1
	.byte	0
	.uleb128 0x7
	.4byte	.LASF23
	.byte	0x5
	.byte	0x7d
	.4byte	0xfa
	.uleb128 0x8
	.byte	0x4
	.byte	0x5
	.byte	0x80
	.4byte	0x135
	.uleb128 0x9
	.4byte	.LASF24
	.sleb128 16
	.uleb128 0x9
	.4byte	.LASF25
	.sleb128 24
	.uleb128 0x9
	.4byte	.LASF26
	.sleb128 32
	.byte	0
	.uleb128 0x7
	.4byte	.LASF27
	.byte	0x5
	.byte	0x84
	.4byte	0x11a
	.uleb128 0x8
	.byte	0x4
	.byte	0x5
	.byte	0x87
	.4byte	0x15b
	.uleb128 0x9
	.4byte	.LASF28
	.sleb128 0
	.uleb128 0x9
	.4byte	.LASF29
	.sleb128 1
	.uleb128 0x9
	.4byte	.LASF30
	.sleb128 2
	.byte	0
	.uleb128 0x7
	.4byte	.LASF31
	.byte	0x5
	.byte	0x8b
	.4byte	0x140
	.uleb128 0xa
	.byte	0x24
	.byte	0x5
	.byte	0x91
	.4byte	0x18b
	.uleb128 0xb
	.4byte	.LASF32
	.byte	0x5
	.byte	0x92
	.4byte	0x9b
	.byte	0x2
	.byte	0x23
	.uleb128 0
	.uleb128 0xb
	.4byte	.LASF33
	.byte	0x5
	.byte	0x93
	.4byte	0x18b
	.byte	0x2
	.byte	0x23
	.uleb128 0x4
	.byte	0
	.uleb128 0xc
	.4byte	0x91
	.4byte	0x19b
	.uleb128 0xd
	.4byte	0x2c
	.byte	0x1f
	.byte	0
	.uleb128 0x7
	.4byte	.LASF34
	.byte	0x5
	.byte	0x94
	.4byte	0x166
	.uleb128 0xe
	.4byte	.LASF62
	.byte	0x1
	.byte	0x59
	.byte	0x1
	.4byte	0x217
	.byte	0x1
	.4byte	0x217
	.uleb128 0xf
	.ascii	"buf\000"
	.byte	0x1
	.byte	0x59
	.4byte	0x217
	.uleb128 0x10
	.4byte	.LASF32
	.byte	0x1
	.byte	0x59
	.4byte	0xe4
	.uleb128 0xf
	.ascii	"bAC\000"
	.byte	0x1
	.byte	0x59
	.4byte	0xad
	.uleb128 0x10
	.4byte	.LASF35
	.byte	0x1
	.byte	0x59
	.4byte	0xd9
	.uleb128 0x10
	.4byte	.LASF36
	.byte	0x1
	.byte	0x59
	.4byte	0xad
	.uleb128 0x10
	.4byte	.LASF37
	.byte	0x1
	.byte	0x59
	.4byte	0x10f
	.uleb128 0xf
	.ascii	"bEn\000"
	.byte	0x1
	.byte	0x59
	.4byte	0xad
	.uleb128 0x11
	.ascii	"err\000"
	.byte	0x1
	.byte	0x5b
	.4byte	0xe4
	.uleb128 0x12
	.4byte	.LASF63
	.byte	0x1
	.byte	0xb7
	.byte	0
	.uleb128 0x3
	.byte	0x4
	.4byte	0xef
	.uleb128 0x13
	.4byte	0x1a6
	.4byte	.LFB191
	.4byte	.LFE191
	.4byte	.LLST0
	.byte	0x1
	.4byte	0x337
	.uleb128 0x14
	.4byte	0x1b7
	.4byte	.LLST1
	.uleb128 0x14
	.4byte	0x1c2
	.4byte	.LLST2
	.uleb128 0x14
	.4byte	0x1d8
	.4byte	.LLST3
	.uleb128 0x14
	.4byte	0x1e3
	.4byte	.LLST4
	.uleb128 0x14
	.4byte	0x1ee
	.4byte	.LLST5
	.uleb128 0x15
	.4byte	0x1f9
	.byte	0x2
	.byte	0x91
	.sleb128 4
	.uleb128 0x15
	.4byte	0x1cd
	.byte	0x6
	.byte	0xfa
	.4byte	0x1cd
	.byte	0x9f
	.uleb128 0x16
	.4byte	0x204
	.4byte	.LLST6
	.uleb128 0x17
	.4byte	0x20f
	.4byte	.L3
	.uleb128 0x18
	.4byte	.LVL4
	.4byte	0x5c5
	.uleb128 0x19
	.4byte	.LVL5
	.4byte	0x5e9
	.4byte	0x2af
	.uleb128 0x1a
	.byte	0x1
	.byte	0x52
	.byte	0x2
	.byte	0x74
	.sleb128 0
	.uleb128 0x1a
	.byte	0x1
	.byte	0x51
	.byte	0x2
	.byte	0x76
	.sleb128 0
	.uleb128 0x1a
	.byte	0x1
	.byte	0x50
	.byte	0x2
	.byte	0x74
	.sleb128 0
	.byte	0
	.uleb128 0x18
	.4byte	.LVL6
	.4byte	0x612
	.uleb128 0x19
	.4byte	.LVL8
	.4byte	0x61c
	.4byte	0x2d1
	.uleb128 0x1a
	.byte	0x1
	.byte	0x51
	.byte	0x2
	.byte	0x8
	.byte	0x20
	.uleb128 0x1a
	.byte	0x1
	.byte	0x50
	.byte	0x1
	.byte	0x32
	.byte	0
	.uleb128 0x19
	.4byte	.LVL10
	.4byte	0x639
	.4byte	0x2fb
	.uleb128 0x1a
	.byte	0x1
	.byte	0x53
	.byte	0x5
	.byte	0x76
	.sleb128 0
	.byte	0x9
	.byte	0x80
	.byte	0x1a
	.uleb128 0x1a
	.byte	0x1
	.byte	0x52
	.byte	0x2
	.byte	0x74
	.sleb128 0
	.uleb128 0x1a
	.byte	0x1
	.byte	0x51
	.byte	0x2
	.byte	0x74
	.sleb128 0
	.uleb128 0x1a
	.byte	0x1
	.byte	0x50
	.byte	0x3
	.byte	0x91
	.sleb128 0
	.byte	0x6
	.byte	0
	.uleb128 0x19
	.4byte	.LVL13
	.4byte	0x660
	.4byte	0x31b
	.uleb128 0x1a
	.byte	0x1
	.byte	0x51
	.byte	0x5
	.byte	0x3
	.4byte	.LC1
	.uleb128 0x1a
	.byte	0x1
	.byte	0x50
	.byte	0x5
	.byte	0x3
	.4byte	.LC0
	.byte	0
	.uleb128 0x18
	.4byte	.LVL14
	.4byte	0x679
	.uleb128 0x18
	.4byte	.LVL16
	.4byte	0x683
	.uleb128 0x18
	.4byte	.LVL18
	.4byte	0x679
	.byte	0
	.uleb128 0x1b
	.byte	0x1
	.4byte	.LASF38
	.byte	0x1
	.byte	0xc7
	.byte	0x1
	.4byte	0x8b
	.4byte	.LFB187
	.4byte	.LFE187
	.4byte	.LLST7
	.byte	0x1
	.4byte	0x3d5
	.uleb128 0x1c
	.ascii	"buf\000"
	.byte	0x1
	.byte	0xc7
	.4byte	0x8b
	.4byte	.LLST8
	.uleb128 0x1d
	.4byte	.LASF32
	.byte	0x1
	.byte	0xc7
	.4byte	0x45
	.4byte	.LLST9
	.uleb128 0x1c
	.ascii	"bAC\000"
	.byte	0x1
	.byte	0xc7
	.4byte	0x53
	.4byte	.LLST10
	.uleb128 0x1d
	.4byte	.LASF35
	.byte	0x1
	.byte	0xc7
	.4byte	0xd9
	.4byte	.LLST11
	.uleb128 0x1d
	.4byte	.LASF36
	.byte	0x1
	.byte	0xc7
	.4byte	0x53
	.4byte	.LLST12
	.uleb128 0x1e
	.4byte	.LVL24
	.4byte	0x21d
	.uleb128 0x1a
	.byte	0x1
	.byte	0x53
	.byte	0x4
	.byte	0x91
	.sleb128 0
	.byte	0x94
	.byte	0x1
	.uleb128 0x1a
	.byte	0x1
	.byte	0x52
	.byte	0x3
	.byte	0xf3
	.uleb128 0x1
	.byte	0x53
	.uleb128 0x1a
	.byte	0x1
	.byte	0x51
	.byte	0x3
	.byte	0xf3
	.uleb128 0x1
	.byte	0x51
	.uleb128 0x1a
	.byte	0x1
	.byte	0x50
	.byte	0x3
	.byte	0xf3
	.uleb128 0x1
	.byte	0x50
	.uleb128 0x1a
	.byte	0x2
	.byte	0x7d
	.sleb128 4
	.byte	0x1
	.byte	0x31
	.uleb128 0x1f
	.4byte	0x1cd
	.byte	0x1
	.byte	0x31
	.byte	0
	.byte	0
	.uleb128 0x1b
	.byte	0x1
	.4byte	.LASF39
	.byte	0x1
	.byte	0xd0
	.byte	0x1
	.4byte	0x8b
	.4byte	.LFB188
	.4byte	.LFE188
	.4byte	.LLST13
	.byte	0x1
	.4byte	0x473
	.uleb128 0x1c
	.ascii	"buf\000"
	.byte	0x1
	.byte	0xd0
	.4byte	0x8b
	.4byte	.LLST14
	.uleb128 0x1d
	.4byte	.LASF32
	.byte	0x1
	.byte	0xd0
	.4byte	0x45
	.4byte	.LLST15
	.uleb128 0x1c
	.ascii	"bAC\000"
	.byte	0x1
	.byte	0xd0
	.4byte	0x53
	.4byte	.LLST16
	.uleb128 0x1d
	.4byte	.LASF35
	.byte	0x1
	.byte	0xd0
	.4byte	0xd9
	.4byte	.LLST17
	.uleb128 0x1d
	.4byte	.LASF36
	.byte	0x1
	.byte	0xd0
	.4byte	0x53
	.4byte	.LLST18
	.uleb128 0x1e
	.4byte	.LVL30
	.4byte	0x21d
	.uleb128 0x1a
	.byte	0x1
	.byte	0x53
	.byte	0x4
	.byte	0x91
	.sleb128 0
	.byte	0x94
	.byte	0x1
	.uleb128 0x1a
	.byte	0x1
	.byte	0x52
	.byte	0x3
	.byte	0xf3
	.uleb128 0x1
	.byte	0x53
	.uleb128 0x1a
	.byte	0x1
	.byte	0x51
	.byte	0x3
	.byte	0xf3
	.uleb128 0x1
	.byte	0x51
	.uleb128 0x1a
	.byte	0x1
	.byte	0x50
	.byte	0x3
	.byte	0xf3
	.uleb128 0x1
	.byte	0x50
	.uleb128 0x1a
	.byte	0x2
	.byte	0x7d
	.sleb128 4
	.byte	0x1
	.byte	0x30
	.uleb128 0x1f
	.4byte	0x1cd
	.byte	0x1
	.byte	0x31
	.byte	0
	.byte	0
	.uleb128 0x20
	.byte	0x1
	.4byte	.LASF64
	.byte	0x1
	.byte	0xd8
	.byte	0x1
	.4byte	0x45
	.4byte	.LFB189
	.4byte	.LFE189
	.4byte	.LLST19
	.byte	0x1
	.uleb128 0x1b
	.byte	0x1
	.4byte	.LASF40
	.byte	0x1
	.byte	0xe0
	.byte	0x1
	.4byte	0x45
	.4byte	.LFB190
	.4byte	.LFE190
	.4byte	.LLST20
	.byte	0x1
	.4byte	0x50c
	.uleb128 0x1d
	.4byte	.LASF41
	.byte	0x1
	.byte	0xe0
	.4byte	0x8b
	.4byte	.LLST21
	.uleb128 0x1d
	.4byte	.LASF32
	.byte	0x1
	.byte	0xe0
	.4byte	0x45
	.4byte	.LLST22
	.uleb128 0x21
	.4byte	.LASF42
	.byte	0x1
	.byte	0xe2
	.4byte	0x19b
	.byte	0x2
	.byte	0x91
	.sleb128 -52
	.uleb128 0x22
	.ascii	"i\000"
	.byte	0x1
	.byte	0xe3
	.4byte	0xe4
	.4byte	.LLST23
	.uleb128 0x19
	.4byte	.LVL40
	.4byte	0x660
	.4byte	0x4fb
	.uleb128 0x1a
	.byte	0x1
	.byte	0x50
	.byte	0x5
	.byte	0x3
	.4byte	.LC2
	.byte	0
	.uleb128 0x1e
	.4byte	.LVL41
	.4byte	0x691
	.uleb128 0x1a
	.byte	0x1
	.byte	0x50
	.byte	0x2
	.byte	0x7b
	.sleb128 -48
	.byte	0
	.byte	0
	.uleb128 0xc
	.4byte	0xe4
	.4byte	0x51c
	.uleb128 0xd
	.4byte	0x2c
	.byte	0x7
	.byte	0
	.uleb128 0x21
	.4byte	.LASF43
	.byte	0x1
	.byte	0x46
	.4byte	0x52d
	.byte	0x5
	.byte	0x3
	.4byte	g_HACC_CFG_1
	.uleb128 0x4
	.4byte	0x50c
	.uleb128 0x21
	.4byte	.LASF44
	.byte	0x1
	.byte	0x4b
	.4byte	0x543
	.byte	0x5
	.byte	0x3
	.4byte	g_HACC_CFG_2
	.uleb128 0x4
	.4byte	0x50c
	.uleb128 0x21
	.4byte	.LASF45
	.byte	0x1
	.byte	0x50
	.4byte	0x559
	.byte	0x5
	.byte	0x3
	.4byte	g_HACC_CFG_3
	.uleb128 0x4
	.4byte	0x50c
	.uleb128 0x23
	.4byte	.LASF46
	.byte	0x6
	.byte	0xc
	.4byte	0x45
	.byte	0x1
	.byte	0x1
	.uleb128 0xc
	.4byte	0x68
	.4byte	0x576
	.uleb128 0x24
	.byte	0
	.uleb128 0x23
	.4byte	.LASF47
	.byte	0x7
	.byte	0x24
	.4byte	0x56b
	.byte	0x1
	.byte	0x1
	.uleb128 0xc
	.4byte	0x3e
	.4byte	0x58e
	.uleb128 0x24
	.byte	0
	.uleb128 0x25
	.4byte	.LASF48
	.byte	0x8
	.2byte	0x1b2
	.4byte	0x59c
	.byte	0x1
	.byte	0x1
	.uleb128 0x4
	.4byte	0x583
	.uleb128 0x26
	.4byte	.LASF49
	.byte	0x1
	.byte	0x40
	.4byte	0xad
	.byte	0x1
	.byte	0x5
	.byte	0x3
	.4byte	bHACC_HWWrapKeyInit
	.uleb128 0x26
	.4byte	.LASF50
	.byte	0x1
	.byte	0x41
	.4byte	0xad
	.byte	0x1
	.byte	0x5
	.byte	0x3
	.4byte	bHACC_SWKeyInit
	.uleb128 0x27
	.byte	0x1
	.4byte	.LASF51
	.byte	0x1
	.byte	0x33
	.byte	0x1
	.byte	0x1
	.4byte	0x5de
	.uleb128 0x28
	.4byte	0x84
	.uleb128 0x28
	.4byte	0x5de
	.byte	0
	.uleb128 0x3
	.byte	0x4
	.4byte	0x5e4
	.uleb128 0x4
	.4byte	0x45
	.uleb128 0x27
	.byte	0x1
	.4byte	.LASF52
	.byte	0x1
	.byte	0x34
	.byte	0x1
	.byte	0x1
	.4byte	0x607
	.uleb128 0x28
	.4byte	0x607
	.uleb128 0x28
	.4byte	0x9b
	.uleb128 0x28
	.4byte	0x607
	.byte	0
	.uleb128 0x3
	.byte	0x4
	.4byte	0x60d
	.uleb128 0x29
	.4byte	0x9b
	.uleb128 0x2a
	.byte	0x1
	.4byte	.LASF56
	.byte	0x1
	.byte	0x35
	.byte	0x1
	.byte	0x1
	.uleb128 0x2b
	.byte	0x1
	.4byte	.LASF53
	.byte	0x1
	.byte	0x2f
	.byte	0x1
	.4byte	0x45
	.byte	0x1
	.4byte	0x639
	.uleb128 0x28
	.4byte	0x15b
	.uleb128 0x28
	.4byte	0x135
	.byte	0
	.uleb128 0x2b
	.byte	0x1
	.4byte	.LASF54
	.byte	0x1
	.byte	0x30
	.byte	0x1
	.4byte	0x45
	.byte	0x1
	.4byte	0x660
	.uleb128 0x28
	.4byte	0x10f
	.uleb128 0x28
	.4byte	0x8b
	.uleb128 0x28
	.4byte	0x8b
	.uleb128 0x28
	.4byte	0x45
	.byte	0
	.uleb128 0x2b
	.byte	0x1
	.4byte	.LASF55
	.byte	0x7
	.byte	0x7e
	.byte	0x1
	.4byte	0x68
	.byte	0x1
	.4byte	0x679
	.uleb128 0x28
	.4byte	0x33
	.uleb128 0x2c
	.byte	0
	.uleb128 0x2a
	.byte	0x1
	.4byte	.LASF57
	.byte	0x9
	.byte	0x17
	.byte	0x1
	.byte	0x1
	.uleb128 0x2d
	.byte	0x1
	.4byte	.LASF65
	.byte	0x9
	.byte	0x16
	.byte	0x1
	.4byte	0x68
	.byte	0x1
	.uleb128 0x2b
	.byte	0x1
	.4byte	.LASF58
	.byte	0x1
	.byte	0x31
	.byte	0x1
	.4byte	0x45
	.byte	0x1
	.4byte	0x6a9
	.uleb128 0x28
	.4byte	0x6a9
	.byte	0
	.uleb128 0x3
	.byte	0x4
	.4byte	0x19b
	.byte	0
	.section	.debug_abbrev,"",%progbits
.Ldebug_abbrev0:
	.uleb128 0x1
	.uleb128 0x11
	.byte	0x1
	.uleb128 0x25
	.uleb128 0xe
	.uleb128 0x13
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1b
	.uleb128 0xe
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x10
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x2
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.byte	0
	.byte	0
	.uleb128 0x3
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x4
	.uleb128 0x26
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x5
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0x8
	.byte	0
	.byte	0
	.uleb128 0x6
	.uleb128 0x16
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x7
	.uleb128 0x16
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x8
	.uleb128 0x4
	.byte	0x1
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x9
	.uleb128 0x28
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1c
	.uleb128 0xd
	.byte	0
	.byte	0
	.uleb128 0xa
	.uleb128 0x13
	.byte	0x1
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xb
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0xc
	.uleb128 0x1
	.byte	0x1
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xd
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2f
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0xe
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x20
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xf
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x10
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x11
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x12
	.uleb128 0xa
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x13
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x40
	.uleb128 0x6
	.uleb128 0x2117
	.uleb128 0xc
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x14
	.uleb128 0x5
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x15
	.uleb128 0x5
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0x16
	.uleb128 0x34
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x17
	.uleb128 0xa
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.byte	0
	.byte	0
	.uleb128 0x18
	.uleb128 0x4109
	.byte	0
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x31
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x19
	.uleb128 0x4109
	.byte	0x1
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x1a
	.uleb128 0x410a
	.byte	0
	.uleb128 0x2
	.uleb128 0xa
	.uleb128 0x2111
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0x1b
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x40
	.uleb128 0x6
	.uleb128 0x2117
	.uleb128 0xc
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x1c
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x1d
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x1e
	.uleb128 0x4109
	.byte	0x1
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x31
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x1f
	.uleb128 0x410a
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x2111
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0x20
	.uleb128 0x2e
	.byte	0
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x40
	.uleb128 0x6
	.uleb128 0x2117
	.uleb128 0xc
	.byte	0
	.byte	0
	.uleb128 0x21
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0x22
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x23
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3c
	.uleb128 0xc
	.byte	0
	.byte	0
	.uleb128 0x24
	.uleb128 0x21
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x25
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3c
	.uleb128 0xc
	.byte	0
	.byte	0
	.uleb128 0x26
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x2
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0x27
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x3c
	.uleb128 0xc
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x28
	.uleb128 0x5
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x29
	.uleb128 0x35
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x2a
	.uleb128 0x2e
	.byte	0
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x3c
	.uleb128 0xc
	.byte	0
	.byte	0
	.uleb128 0x2b
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3c
	.uleb128 0xc
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x2c
	.uleb128 0x18
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x2d
	.uleb128 0x2e
	.byte	0
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3c
	.uleb128 0xc
	.byte	0
	.byte	0
	.byte	0
	.section	.debug_loc,"",%progbits
.Ldebug_loc0:
.LLST0:
	.4byte	.LFB191-.Ltext0
	.4byte	.LCFI0-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 0
	.4byte	.LCFI0-.Ltext0
	.4byte	.LCFI2-.Ltext0
	.2byte	0x2
	.byte	0x7c
	.sleb128 0
	.4byte	.LCFI2-.Ltext0
	.4byte	.LCFI3-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	.LCFI3-.Ltext0
	.4byte	.LCFI4-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 32
	.4byte	.LCFI4-.Ltext0
	.4byte	.LCFI5-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	.LCFI5-.Ltext0
	.4byte	.LCFI6-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 32
	.4byte	.LCFI6-.Ltext0
	.4byte	.LFE191-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	0
	.4byte	0
.LLST1:
	.4byte	.LVL0-.Ltext0
	.4byte	.LVL3-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	.LVL3-.Ltext0
	.4byte	.LVL15-.Ltext0
	.2byte	0x1
	.byte	0x54
	.4byte	.LVL15-.Ltext0
	.4byte	.LVL16-1-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	.LVL16-1-.Ltext0
	.4byte	.LFE191-.Ltext0
	.2byte	0x1
	.byte	0x54
	.4byte	0
	.4byte	0
.LLST2:
	.4byte	.LVL0-.Ltext0
	.4byte	.LVL3-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	.LVL3-.Ltext0
	.4byte	.LVL15-.Ltext0
	.2byte	0x1
	.byte	0x56
	.4byte	.LVL15-.Ltext0
	.4byte	.LVL16-1-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	.LVL16-1-.Ltext0
	.4byte	.LFE191-.Ltext0
	.2byte	0x1
	.byte	0x56
	.4byte	0
	.4byte	0
.LLST3:
	.4byte	.LVL0-.Ltext0
	.4byte	.LVL3-.Ltext0
	.2byte	0x1
	.byte	0x52
	.4byte	.LVL3-.Ltext0
	.4byte	.LVL15-.Ltext0
	.2byte	0x4
	.byte	0xf3
	.uleb128 0x1
	.byte	0x52
	.byte	0x9f
	.4byte	.LVL15-.Ltext0
	.4byte	.LVL16-1-.Ltext0
	.2byte	0x1
	.byte	0x52
	.4byte	.LVL16-1-.Ltext0
	.4byte	.LFE191-.Ltext0
	.2byte	0x4
	.byte	0xf3
	.uleb128 0x1
	.byte	0x52
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST4:
	.4byte	.LVL0-.Ltext0
	.4byte	.LVL3-.Ltext0
	.2byte	0x1
	.byte	0x53
	.4byte	.LVL3-.Ltext0
	.4byte	.LVL15-.Ltext0
	.2byte	0x4
	.byte	0xf3
	.uleb128 0x1
	.byte	0x53
	.byte	0x9f
	.4byte	.LVL15-.Ltext0
	.4byte	.LVL16-1-.Ltext0
	.2byte	0x1
	.byte	0x53
	.4byte	.LVL16-1-.Ltext0
	.4byte	.LFE191-.Ltext0
	.2byte	0x4
	.byte	0xf3
	.uleb128 0x1
	.byte	0x53
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST5:
	.4byte	.LVL0-.Ltext0
	.4byte	.LVL1-.Ltext0
	.2byte	0x2
	.byte	0x91
	.sleb128 0
	.4byte	.LVL1-.Ltext0
	.4byte	.LVL3-.Ltext0
	.2byte	0x2
	.byte	0x7c
	.sleb128 0
	.4byte	.LVL3-.Ltext0
	.4byte	.LVL15-.Ltext0
	.2byte	0x2
	.byte	0x91
	.sleb128 0
	.4byte	.LVL15-.Ltext0
	.4byte	.LVL16-1-.Ltext0
	.2byte	0x2
	.byte	0x7c
	.sleb128 0
	.4byte	.LVL16-1-.Ltext0
	.4byte	.LFE191-.Ltext0
	.2byte	0x2
	.byte	0x91
	.sleb128 0
	.4byte	0
	.4byte	0
.LLST6:
	.4byte	.LVL2-.Ltext0
	.4byte	.LVL3-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	.LVL8-.Ltext0
	.4byte	.LVL9-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	.LVL10-.Ltext0
	.4byte	.LVL11-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	.LVL11-.Ltext0
	.4byte	.LVL12-.Ltext0
	.2byte	0x1
	.byte	0x52
	.4byte	.LVL15-.Ltext0
	.4byte	.LVL16-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	.LVL16-.Ltext0
	.4byte	.LVL17-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	.LVL17-.Ltext0
	.4byte	.LVL18-1-.Ltext0
	.2byte	0x1
	.byte	0x52
	.4byte	0
	.4byte	0
.LLST7:
	.4byte	.LFB187-.Ltext0
	.4byte	.LCFI7-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 0
	.4byte	.LCFI7-.Ltext0
	.4byte	.LCFI9-.Ltext0
	.2byte	0x2
	.byte	0x7c
	.sleb128 0
	.4byte	.LCFI9-.Ltext0
	.4byte	.LCFI10-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	.LCFI10-.Ltext0
	.4byte	.LFE187-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 16
	.4byte	0
	.4byte	0
.LLST8:
	.4byte	.LVL19-.Ltext0
	.4byte	.LVL24-1-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	.LVL24-1-.Ltext0
	.4byte	.LFE187-.Ltext0
	.2byte	0x4
	.byte	0xf3
	.uleb128 0x1
	.byte	0x50
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST9:
	.4byte	.LVL19-.Ltext0
	.4byte	.LVL24-1-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	.LVL24-1-.Ltext0
	.4byte	.LFE187-.Ltext0
	.2byte	0x4
	.byte	0xf3
	.uleb128 0x1
	.byte	0x51
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST10:
	.4byte	.LVL19-.Ltext0
	.4byte	.LVL22-.Ltext0
	.2byte	0x1
	.byte	0x52
	.4byte	.LVL22-.Ltext0
	.4byte	.LFE187-.Ltext0
	.2byte	0x4
	.byte	0xf3
	.uleb128 0x1
	.byte	0x52
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST11:
	.4byte	.LVL19-.Ltext0
	.4byte	.LVL23-.Ltext0
	.2byte	0x1
	.byte	0x53
	.4byte	.LVL23-.Ltext0
	.4byte	.LVL24-1-.Ltext0
	.2byte	0x1
	.byte	0x52
	.4byte	.LVL24-1-.Ltext0
	.4byte	.LFE187-.Ltext0
	.2byte	0x4
	.byte	0xf3
	.uleb128 0x1
	.byte	0x53
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST12:
	.4byte	.LVL19-.Ltext0
	.4byte	.LVL20-.Ltext0
	.2byte	0x2
	.byte	0x91
	.sleb128 0
	.4byte	.LVL20-.Ltext0
	.4byte	.LVL21-.Ltext0
	.2byte	0x2
	.byte	0x7c
	.sleb128 0
	.4byte	.LVL21-.Ltext0
	.4byte	.LFE187-.Ltext0
	.2byte	0x2
	.byte	0x91
	.sleb128 0
	.4byte	0
	.4byte	0
.LLST13:
	.4byte	.LFB188-.Ltext0
	.4byte	.LCFI11-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 0
	.4byte	.LCFI11-.Ltext0
	.4byte	.LCFI13-.Ltext0
	.2byte	0x2
	.byte	0x7c
	.sleb128 0
	.4byte	.LCFI13-.Ltext0
	.4byte	.LCFI14-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	.LCFI14-.Ltext0
	.4byte	.LFE188-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 16
	.4byte	0
	.4byte	0
.LLST14:
	.4byte	.LVL25-.Ltext0
	.4byte	.LVL30-1-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	.LVL30-1-.Ltext0
	.4byte	.LFE188-.Ltext0
	.2byte	0x4
	.byte	0xf3
	.uleb128 0x1
	.byte	0x50
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST15:
	.4byte	.LVL25-.Ltext0
	.4byte	.LVL30-1-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	.LVL30-1-.Ltext0
	.4byte	.LFE188-.Ltext0
	.2byte	0x4
	.byte	0xf3
	.uleb128 0x1
	.byte	0x51
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST16:
	.4byte	.LVL25-.Ltext0
	.4byte	.LVL28-.Ltext0
	.2byte	0x1
	.byte	0x52
	.4byte	.LVL28-.Ltext0
	.4byte	.LFE188-.Ltext0
	.2byte	0x4
	.byte	0xf3
	.uleb128 0x1
	.byte	0x52
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST17:
	.4byte	.LVL25-.Ltext0
	.4byte	.LVL29-.Ltext0
	.2byte	0x1
	.byte	0x53
	.4byte	.LVL29-.Ltext0
	.4byte	.LVL30-1-.Ltext0
	.2byte	0x1
	.byte	0x52
	.4byte	.LVL30-1-.Ltext0
	.4byte	.LFE188-.Ltext0
	.2byte	0x4
	.byte	0xf3
	.uleb128 0x1
	.byte	0x53
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST18:
	.4byte	.LVL25-.Ltext0
	.4byte	.LVL26-.Ltext0
	.2byte	0x2
	.byte	0x91
	.sleb128 0
	.4byte	.LVL26-.Ltext0
	.4byte	.LVL27-.Ltext0
	.2byte	0x2
	.byte	0x7c
	.sleb128 0
	.4byte	.LVL27-.Ltext0
	.4byte	.LFE188-.Ltext0
	.2byte	0x2
	.byte	0x91
	.sleb128 0
	.4byte	0
	.4byte	0
.LLST19:
	.4byte	.LFB189-.Ltext0
	.4byte	.LCFI15-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 0
	.4byte	.LCFI15-.Ltext0
	.4byte	.LCFI17-.Ltext0
	.2byte	0x2
	.byte	0x7c
	.sleb128 0
	.4byte	.LCFI17-.Ltext0
	.4byte	.LFE189-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	0
	.4byte	0
.LLST20:
	.4byte	.LFB190-.Ltext0
	.4byte	.LCFI18-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 0
	.4byte	.LCFI18-.Ltext0
	.4byte	.LCFI20-.Ltext0
	.2byte	0x2
	.byte	0x7c
	.sleb128 0
	.4byte	.LCFI20-.Ltext0
	.4byte	.LCFI21-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	.LCFI21-.Ltext0
	.4byte	.LCFI22-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 16
	.4byte	.LCFI22-.Ltext0
	.4byte	.LFE190-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	0
	.4byte	0
.LLST21:
	.4byte	.LVL31-.Ltext0
	.4byte	.LVL33-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	.LVL33-.Ltext0
	.4byte	.LFE190-.Ltext0
	.2byte	0x4
	.byte	0xf3
	.uleb128 0x1
	.byte	0x50
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST22:
	.4byte	.LVL31-.Ltext0
	.4byte	.LVL34-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	.LVL34-.Ltext0
	.4byte	.LVL35-.Ltext0
	.2byte	0x4
	.byte	0xf3
	.uleb128 0x1
	.byte	0x51
	.byte	0x9f
	.4byte	.LVL35-.Ltext0
	.4byte	.LVL36-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	.LVL36-.Ltext0
	.4byte	.LFE190-.Ltext0
	.2byte	0x4
	.byte	0xf3
	.uleb128 0x1
	.byte	0x51
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST23:
	.4byte	.LVL32-.Ltext0
	.4byte	.LVL34-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	.LVL35-.Ltext0
	.4byte	.LVL37-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	.LVL37-.Ltext0
	.4byte	.LVL39-.Ltext0
	.2byte	0x1
	.byte	0x53
	.4byte	0
	.4byte	0
	.section	.debug_aranges,"",%progbits
	.4byte	0x1c
	.2byte	0x2
	.4byte	.Ldebug_info0
	.byte	0x4
	.byte	0
	.2byte	0
	.2byte	0
	.4byte	.Ltext0
	.4byte	.Letext0-.Ltext0
	.4byte	0
	.4byte	0
	.section	.debug_line,"",%progbits
.Ldebug_line0:
	.section	.debug_str,"MS",%progbits,1
.LASF53:
	.ascii	"hacc_set_key\000"
.LASF13:
	.ascii	"BOOL\000"
.LASF59:
	.ascii	"GNU C 4.8 -fpreprocessed -mbionic -mlittle-endian -"
	.ascii	"mapcs -mno-sched-prolog -mabi=aapcs-linux -mno-thum"
	.ascii	"b-interwork -marm -march=armv7-a -mfloat-abi=softfp"
	.ascii	" -mfpu=vfp -mtls-dialect=gnu -gdwarf-2 -O2 -fno-str"
	.ascii	"ict-aliasing -fno-common -fno-delete-null-pointer-c"
	.ascii	"hecks -fno-pic -fno-dwarf2-cfi-asm -fno-stack-prote"
	.ascii	"ctor -fno-omit-frame-pointer -fno-optimize-sibling-"
	.ascii	"calls -fno-strict-overflow -fconserve-stack\000"
.LASF6:
	.ascii	"short int\000"
.LASF1:
	.ascii	"sizetype\000"
.LASF50:
	.ascii	"bHACC_SWKeyInit\000"
.LASF27:
	.ascii	"AES_KEY\000"
.LASF22:
	.ascii	"AES_ENC\000"
.LASF57:
	.ascii	"osal_hacc_unlock\000"
.LASF47:
	.ascii	"console_printk\000"
.LASF46:
	.ascii	"elf_hwcap\000"
.LASF51:
	.ascii	"HACC_V3_Init\000"
.LASF56:
	.ascii	"HACC_V3_Terminate\000"
.LASF15:
	.ascii	"HACC_USER2\000"
.LASF12:
	.ascii	"float\000"
.LASF8:
	.ascii	"long long int\000"
.LASF26:
	.ascii	"AES_KEY_256\000"
.LASF40:
	.ascii	"masp_hal_sp_hacc_init\000"
.LASF49:
	.ascii	"bHACC_HWWrapKeyInit\000"
.LASF10:
	.ascii	"long int\000"
.LASF52:
	.ascii	"HACC_V3_Run\000"
.LASF38:
	.ascii	"masp_hal_sp_hacc_enc\000"
.LASF55:
	.ascii	"printk\000"
.LASF31:
	.ascii	"AES_KEY_ID\000"
.LASF20:
	.ascii	"uchar\000"
.LASF18:
	.ascii	"HACC_USER\000"
.LASF5:
	.ascii	"unsigned char\000"
.LASF63:
	.ascii	"_err\000"
.LASF61:
	.ascii	"/proj/mtk02464/Public/P4Android/ALPS_SW/TRUNK/KK/al"
	.ascii	"ps/out/target/product/mt6752_evb/obj/KERNEL_OBJ\000"
.LASF58:
	.ascii	"hacc_init\000"
.LASF4:
	.ascii	"signed char\000"
.LASF9:
	.ascii	"long long unsigned int\000"
.LASF28:
	.ascii	"AES_SW_KEY\000"
.LASF3:
	.ascii	"unsigned int\000"
.LASF65:
	.ascii	"osal_hacc_lock\000"
.LASF23:
	.ascii	"AES_OPS\000"
.LASF43:
	.ascii	"g_HACC_CFG_1\000"
.LASF44:
	.ascii	"g_HACC_CFG_2\000"
.LASF45:
	.ascii	"g_HACC_CFG_3\000"
.LASF35:
	.ascii	"user\000"
.LASF39:
	.ascii	"masp_hal_sp_hacc_dec\000"
.LASF29:
	.ascii	"AES_HW_KEY\000"
.LASF14:
	.ascii	"HACC_USER1\000"
.LASF2:
	.ascii	"char\000"
.LASF16:
	.ascii	"HACC_USER3\000"
.LASF17:
	.ascii	"HACC_USER4\000"
.LASF33:
	.ascii	"seed\000"
.LASF24:
	.ascii	"AES_KEY_128\000"
.LASF7:
	.ascii	"short unsigned int\000"
.LASF60:
	.ascii	"al"
	.ascii	"ps/kernel-3.10/mediatek/platform/mt6752/kernel/driv"
	.ascii	"ers/masp/mach/hacc_export.c\000"
.LASF11:
	.ascii	"_Bool\000"
.LASF37:
	.ascii	"aes_type\000"
.LASF64:
	.ascii	"masp_hal_sp_hacc_blk_sz\000"
.LASF34:
	.ascii	"AES_KEY_SEED\000"
.LASF54:
	.ascii	"hacc_do_aes\000"
.LASF19:
	.ascii	"uint32\000"
.LASF0:
	.ascii	"long unsigned int\000"
.LASF42:
	.ascii	"keyseed\000"
.LASF32:
	.ascii	"size\000"
.LASF30:
	.ascii	"AES_HW_WRAP_KEY\000"
.LASF48:
	.ascii	"hex_asc\000"
.LASF25:
	.ascii	"AES_KEY_192\000"
.LASF36:
	.ascii	"bDoLock\000"
.LASF21:
	.ascii	"AES_DEC\000"
.LASF62:
	.ascii	"sp_hacc_internal\000"
.LASF41:
	.ascii	"sec_seed\000"
	.ident	"GCC: (GNU) 4.8"
	.section	.note.GNU-stack,"",%progbits
