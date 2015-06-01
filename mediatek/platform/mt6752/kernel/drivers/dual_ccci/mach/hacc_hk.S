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
	.file	"hacc_hk.c"
	.text
.Ltext0:
	.align	2
	.global	HACC_V3_Init
	.type	HACC_V3_Init, %function
HACC_V3_Init:
.LFB186:
	.file 1 "alps/kernel-3.10/mediatek/platform/mt6752/kernel/drivers/masp/mach/hacc_hk.c"
	.loc 1 63 0
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
.LVL0:
	mov	ip, sp
.LCFI0:
	stmfd	sp!, {r4, r5, r6, r7, fp, ip, lr, pc}
.LCFI1:
	sub	fp, ip, #4
.LCFI2:
	.loc 1 63 0
	mov	r5, r1
	mov	r4, r0
	.loc 1 73 0
	bl	masp_hal_secure_algo_init
.LVL1:
	.loc 1 91 0
	cmp	r4, #0
	.loc 1 95 0
	mov	r3, #0
	movw	r0, #:lower16:hacc_base
	movt	r0, #:upper16:hacc_base
	.loc 1 91 0
	moveq	r1, #2
	movne	r1, #3
.LVL2:
	.loc 1 105 0
	mov	lr, r3
.LVL3:
.L4:
	.loc 1 105 0 is_stmt 0 discriminator 2
	ldr	ip, [r0]
	movw	r2, #:lower16:hacc_base
	movt	r2, #:upper16:hacc_base
	add	ip, ip, #32
	str	lr, [ip, r3]
	add	r3, r3, #4
	.loc 1 103 0 is_stmt 1 discriminator 2
	cmp	r3, #32
	bne	.L4
	.loc 1 111 0
	ldr	lr, [r2]
	mov	ip, #2
	.loc 1 115 0
	mov	r4, #16
	.loc 1 120 0
	mov	r3, #0
	.loc 1 111 0
	str	ip, [lr, #4]
	.loc 1 115 0
	ldr	lr, [r2]
	str	r4, [lr, #12]
	.loc 1 117 0
	ldr	lr, [r2]
	ldr	r4, [lr, #12]
	orr	r4, r4, #256
	str	r4, [lr, #12]
	.loc 1 120 0
	ldr	r2, [r2]
	str	ip, [r2, #8]
.LVL4:
.L6:
	.loc 1 126 0 discriminator 2
	ldr	ip, [r0]
	movw	r2, #:lower16:hacc_base
	ldr	lr, [r5, r3]
	movt	r2, #:upper16:hacc_base
	add	ip, ip, #64
	str	lr, [ip, r3]
	add	r3, r3, #4
	.loc 1 124 0 discriminator 2
	cmp	r3, #16
	bne	.L6
	movw	lr, #:lower16:.LANCHOR0
	ldr	ip, [r2]
	movt	lr, #:upper16:.LANCHOR0
	.loc 1 139 0
	mov	r7, #1
	add	r6, lr, #48
.LVL5:
.L12:
	.loc 1 133 0
	mov	r3, #0
.LVL6:
.L8:
	.loc 1 136 0 discriminator 2
	add	r2, r3, #16
	ldr	r4, [lr, r3]
	add	r3, r3, #4
	.loc 1 134 0 discriminator 2
	cmp	r3, #16
	.loc 1 136 0 discriminator 2
	str	r4, [r2, ip]
	.loc 1 134 0 discriminator 2
	ldrne	ip, [r0]
	bne	.L8
.L7:
	.loc 1 139 0
	ldr	r3, [r0]
	str	r7, [r3, #8]
	ldr	ip, [r0]
	add	r2, ip, #8
.L10:
	.loc 1 141 0 discriminator 1
	ldr	r3, [r2]
	tst	r3, #32768
	beq	.L10
	add	lr, lr, #16
.LVL7:
	.loc 1 130 0
	cmp	lr, r6
	bne	.L12
	.loc 1 150 0
	mov	r3, #0
	mov	ip, #2
	str	ip, [r2]
.LVL8:
.L14:
	.loc 1 156 0 discriminator 2
	ldr	ip, [r0]
	movw	r2, #:lower16:hacc_base
	ldr	lr, [r5, r3]
	movt	r2, #:upper16:hacc_base
	add	ip, ip, #64
	str	lr, [ip, r3]
	add	r3, r3, #4
	.loc 1 154 0 discriminator 2
	cmp	r3, #16
	bne	.L14
	.loc 1 160 0
	ldr	r3, [r2]
	.loc 1 161 0
	mov	r0, #0
	.loc 1 160 0
	str	r1, [r3, #4]
	.loc 1 161 0
	ldr	r3, [r2]
	str	r0, [r3, #12]
	ldmfd	sp, {r4, r5, r6, r7, fp, sp, pc}
.LFE186:
	.size	HACC_V3_Init, .-HACC_V3_Init
	.align	2
	.global	HACC_V3_Run
	.type	HACC_V3_Run, %function
HACC_V3_Run:
.LFB187:
	.loc 1 165 0
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
.LVL9:
	mov	ip, sp
.LCFI3:
	stmfd	sp!, {r4, r5, r6, r7, r8, fp, ip, lr, pc}
.LCFI4:
	sub	fp, ip, #4
.LCFI5:
.LVL10:
	.loc 1 172 0
	cmp	r1, #0
	ldmeqfd	sp, {r4, r5, r6, r7, r8, fp, sp, pc}
	movw	r4, #:lower16:hacc_base
	movt	r4, #:upper16:hacc_base
	mov	r5, #0
	.loc 1 180 0
	mov	r7, #1
	.loc 1 177 0
	mov	r6, r4
.LVL11:
.L29:
	add	r8, r0, r5
.LVL12:
	.loc 1 165 0
	mov	r3, #0
.LVL13:
.L30:
	.loc 1 177 0 discriminator 2
	ldr	ip, [r4]
	ldr	lr, [r8, r3]
	add	ip, ip, #16
	str	lr, [ip, r3]
	add	r3, r3, #4
	.loc 1 175 0 discriminator 2
	cmp	r3, #16
	bne	.L30
	.loc 1 180 0
	ldr	r3, [r6]
	str	r7, [r3, #8]
	ldr	lr, [r6]
	add	ip, lr, #8
.L25:
	.loc 1 182 0 discriminator 1
	ldr	r3, [ip]
	tst	r3, #32768
	beq	.L25
	add	r8, r2, r5
.LVL14:
	.loc 1 182 0 is_stmt 0
	mov	r3, #0
.L27:
	.loc 1 186 0 is_stmt 1 discriminator 2
	add	ip, r3, #80
	ldr	ip, [ip, lr]
	str	ip, [r3, r8]
	add	r3, r3, #4
	.loc 1 184 0 discriminator 2
	cmp	r3, #16
	ldrne	lr, [r4]
	bne	.L27
.L26:
	.loc 1 172 0
	add	r5, r5, #16
.LVL15:
	cmp	r1, r5
	bhi	.L29
	ldmfd	sp, {r4, r5, r6, r7, r8, fp, sp, pc}
.LFE187:
	.size	HACC_V3_Run, .-HACC_V3_Run
	.align	2
	.global	HACC_V3_Terminate
	.type	HACC_V3_Terminate, %function
HACC_V3_Terminate:
.LFB188:
	.loc 1 192 0
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
.LCFI6:
	stmfd	sp!, {fp, ip, lr, pc}
.LCFI7:
	sub	fp, ip, #4
.LCFI8:
	.loc 1 196 0
	movw	r1, #:lower16:hacc_base
	movt	r1, #:upper16:hacc_base
	mov	r3, #0
	mov	ip, #2
	ldr	r2, [r1]
	.loc 1 204 0
	mov	r0, r3
	.loc 1 196 0
	str	ip, [r2, #8]
.LVL16:
.L34:
	.loc 1 204 0 discriminator 2
	ldr	r2, [r1]
	add	r2, r2, #32
	str	r0, [r2, r3]
	add	r3, r3, #4
	.loc 1 202 0 discriminator 2
	cmp	r3, #32
	bne	.L34
	.loc 1 207 0
	bl	masp_hal_secure_algo_deinit
.LVL17:
	ldmfd	sp, {fp, sp, pc}
.LFE188:
	.size	HACC_V3_Terminate, .-HACC_V3_Terminate
	.section	.rodata
	.align	2
.LANCHOR0 = . + 0
	.type	g_CFG_RANDOM_PATTERN, %object
	.size	g_CFG_RANDOM_PATTERN, 48
g_CFG_RANDOM_PATTERN:
	.word	759479152
	.word	-1488661977
	.word	-794196405
	.word	-2080390588
	.word	2127046251
	.word	1139281842
	.word	27669642
	.word	543685024
	.word	1414751237
	.word	-716790095
	.word	482602042
	.word	-566789501
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
	.4byte	.LFB186
	.4byte	.LFE186-.LFB186
	.byte	0x4
	.4byte	.LCFI0-.LFB186
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
	.align	2
.LEFDE0:
.LSFDE2:
	.4byte	.LEFDE2-.LASFDE2
.LASFDE2:
	.4byte	.Lframe0
	.4byte	.LFB187
	.4byte	.LFE187-.LFB187
	.byte	0x4
	.4byte	.LCFI3-.LFB187
	.byte	0xd
	.uleb128 0xc
	.byte	0x4
	.4byte	.LCFI4-.LCFI3
	.byte	0x84
	.uleb128 0x9
	.byte	0x85
	.uleb128 0x8
	.byte	0x86
	.uleb128 0x7
	.byte	0x87
	.uleb128 0x6
	.byte	0x88
	.uleb128 0x5
	.byte	0x8b
	.uleb128 0x4
	.byte	0x8d
	.uleb128 0x3
	.byte	0x8e
	.uleb128 0x2
	.byte	0x4
	.4byte	.LCFI5-.LCFI4
	.byte	0xc
	.uleb128 0xb
	.uleb128 0x4
	.align	2
.LEFDE2:
.LSFDE4:
	.4byte	.LEFDE4-.LASFDE4
.LASFDE4:
	.4byte	.Lframe0
	.4byte	.LFB188
	.4byte	.LFE188-.LFB188
	.byte	0x4
	.4byte	.LCFI6-.LFB188
	.byte	0xd
	.uleb128 0xc
	.byte	0x4
	.4byte	.LCFI7-.LCFI6
	.byte	0x8b
	.uleb128 0x4
	.byte	0x8d
	.uleb128 0x3
	.byte	0x8e
	.uleb128 0x2
	.byte	0x4
	.4byte	.LCFI8-.LCFI7
	.byte	0xc
	.uleb128 0xb
	.uleb128 0x4
	.align	2
.LEFDE4:
	.text
.Letext0:
	.file 2 "alps/kernel-3.10/arch/arm/include/asm/hwcap.h"
	.file 3 "alps/kernel-3.10/include/linux/printk.h"
	.file 4 "alps/kernel-3.10/include/linux/kernel.h"
	.file 5 "alps/kernel-3.10/mediatek/platform/mt6752/kernel/drivers/masp/mach/hacc_mach.h"
	.file 6 "alps/kernel-3.10/../mediatek/kernel/include/mach/mt_sec_hal.h"
	.file 7 "alps/kernel-3.10/mediatek/platform/mt6752/kernel/drivers/masp/asf/asf_inc/sec_typedef.h"
	.section	.debug_info,"",%progbits
.Ldebug_info0:
	.4byte	0x250
	.2byte	0x2
	.4byte	.Ldebug_abbrev0
	.byte	0x4
	.uleb128 0x1
	.4byte	.LASF29
	.byte	0x1
	.4byte	.LASF30
	.4byte	.LASF31
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
	.uleb128 0x3
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
	.uleb128 0x4
	.4byte	.LASF32
	.byte	0x7
	.byte	0xe
	.4byte	0x3a
	.uleb128 0x2
	.byte	0x4
	.byte	0x4
	.4byte	.LASF12
	.uleb128 0x5
	.byte	0x1
	.4byte	.LASF16
	.byte	0x1
	.byte	0x3e
	.byte	0x1
	.4byte	.LFB186
	.4byte	.LFE186
	.4byte	.LLST0
	.byte	0x1
	.4byte	0x10b
	.uleb128 0x6
	.4byte	.LASF13
	.byte	0x1
	.byte	0x3e
	.4byte	0x79
	.4byte	.LLST1
	.uleb128 0x6
	.4byte	.LASF14
	.byte	0x1
	.byte	0x3e
	.4byte	0x10b
	.4byte	.LLST2
	.uleb128 0x7
	.ascii	"i\000"
	.byte	0x1
	.byte	0x41
	.4byte	0x80
	.4byte	.LLST3
	.uleb128 0x7
	.ascii	"j\000"
	.byte	0x1
	.byte	0x41
	.4byte	0x80
	.4byte	.LLST4
	.uleb128 0x7
	.ascii	"p1\000"
	.byte	0x1
	.byte	0x42
	.4byte	0x10b
	.4byte	.LLST5
	.uleb128 0x8
	.4byte	.LASF15
	.byte	0x1
	.byte	0x44
	.4byte	0x80
	.4byte	.LLST6
	.uleb128 0x9
	.4byte	.LVL1
	.4byte	0x237
	.byte	0
	.uleb128 0xa
	.byte	0x4
	.4byte	0x111
	.uleb128 0xb
	.4byte	0x80
	.uleb128 0x5
	.byte	0x1
	.4byte	.LASF17
	.byte	0x1
	.byte	0xa4
	.byte	0x1
	.4byte	.LFB187
	.4byte	.LFE187
	.4byte	.LLST7
	.byte	0x1
	.4byte	0x183
	.uleb128 0x6
	.4byte	.LASF18
	.byte	0x1
	.byte	0xa4
	.4byte	0x183
	.4byte	.LLST8
	.uleb128 0xc
	.4byte	.LASF19
	.byte	0x1
	.byte	0xa4
	.4byte	0x80
	.byte	0x1
	.byte	0x51
	.uleb128 0x6
	.4byte	.LASF20
	.byte	0x1
	.byte	0xa4
	.4byte	0x183
	.4byte	.LLST9
	.uleb128 0x7
	.ascii	"i\000"
	.byte	0x1
	.byte	0xa6
	.4byte	0x80
	.4byte	.LLST10
	.uleb128 0x7
	.ascii	"j\000"
	.byte	0x1
	.byte	0xa6
	.4byte	0x80
	.4byte	.LLST11
	.uleb128 0xd
	.ascii	"len\000"
	.byte	0x1
	.byte	0xa9
	.4byte	0x80
	.byte	0x1
	.byte	0x51
	.byte	0
	.uleb128 0xa
	.byte	0x4
	.4byte	0x189
	.uleb128 0xe
	.4byte	0x80
	.uleb128 0x5
	.byte	0x1
	.4byte	.LASF21
	.byte	0x1
	.byte	0xbf
	.byte	0x1
	.4byte	.LFB188
	.4byte	.LFE188
	.4byte	.LLST12
	.byte	0x1
	.4byte	0x1bb
	.uleb128 0xf
	.ascii	"i\000"
	.byte	0x1
	.byte	0xc1
	.4byte	0x80
	.uleb128 0x9
	.4byte	.LVL17
	.4byte	0x245
	.byte	0
	.uleb128 0x10
	.4byte	0x80
	.4byte	0x1d1
	.uleb128 0x11
	.4byte	0x2c
	.byte	0x2
	.uleb128 0x11
	.4byte	0x2c
	.byte	0x3
	.byte	0
	.uleb128 0x12
	.4byte	.LASF22
	.byte	0x1
	.byte	0x34
	.4byte	0x1e2
	.byte	0x5
	.byte	0x3
	.4byte	g_CFG_RANDOM_PATTERN
	.uleb128 0xb
	.4byte	0x1bb
	.uleb128 0x13
	.4byte	.LASF23
	.byte	0x2
	.byte	0xc
	.4byte	0x3a
	.byte	0x1
	.byte	0x1
	.uleb128 0x10
	.4byte	0x5d
	.4byte	0x1ff
	.uleb128 0x14
	.byte	0
	.uleb128 0x13
	.4byte	.LASF24
	.byte	0x3
	.byte	0x24
	.4byte	0x1f4
	.byte	0x1
	.byte	0x1
	.uleb128 0x10
	.4byte	0x33
	.4byte	0x217
	.uleb128 0x14
	.byte	0
	.uleb128 0x15
	.4byte	.LASF25
	.byte	0x4
	.2byte	0x1b2
	.4byte	0x225
	.byte	0x1
	.byte	0x1
	.uleb128 0xb
	.4byte	0x20c
	.uleb128 0x13
	.4byte	.LASF26
	.byte	0x5
	.byte	0x15
	.4byte	0x3a
	.byte	0x1
	.byte	0x1
	.uleb128 0x16
	.byte	0x1
	.4byte	.LASF27
	.byte	0x6
	.byte	0x1c
	.byte	0x1
	.4byte	0x48
	.byte	0x1
	.uleb128 0x16
	.byte	0x1
	.4byte	.LASF28
	.byte	0x6
	.byte	0x1d
	.byte	0x1
	.4byte	0x48
	.byte	0x1
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
	.uleb128 0x4
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
	.uleb128 0x5
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
	.uleb128 0x6
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
	.uleb128 0x7
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
	.uleb128 0x8
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
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x9
	.uleb128 0x4109
	.byte	0
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x31
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xa
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xb
	.uleb128 0x26
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xc
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
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0xd
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
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0xe
	.uleb128 0x35
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xf
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
	.uleb128 0x10
	.uleb128 0x1
	.byte	0x1
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x11
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2f
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x12
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
	.uleb128 0x13
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
	.uleb128 0x14
	.uleb128 0x21
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x15
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
	.uleb128 0x16
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
	.4byte	.LFB186-.Ltext0
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
	.4byte	.LFE186-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	0
	.4byte	0
.LLST1:
	.4byte	.LVL0-.Ltext0
	.4byte	.LVL1-1-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	.LVL1-1-.Ltext0
	.4byte	.LFE186-.Ltext0
	.2byte	0x4
	.byte	0xf3
	.uleb128 0x1
	.byte	0x50
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST2:
	.4byte	.LVL0-.Ltext0
	.4byte	.LVL1-1-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	.LVL1-1-.Ltext0
	.4byte	.LFE186-.Ltext0
	.2byte	0x1
	.byte	0x55
	.4byte	0
	.4byte	0
.LLST3:
	.4byte	.LVL2-.Ltext0
	.4byte	.LVL3-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST4:
	.4byte	.LVL5-.Ltext0
	.4byte	.LVL6-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST5:
	.4byte	.LVL4-.Ltext0
	.4byte	.LVL5-.Ltext0
	.2byte	0x1
	.byte	0x55
	.4byte	.LVL5-.Ltext0
	.4byte	.LVL7-.Ltext0
	.2byte	0x1
	.byte	0x5e
	.4byte	.LVL7-.Ltext0
	.4byte	.LVL8-.Ltext0
	.2byte	0x3
	.byte	0x7e
	.sleb128 -16
	.byte	0x9f
	.4byte	.LVL8-.Ltext0
	.4byte	.LFE186-.Ltext0
	.2byte	0x1
	.byte	0x55
	.4byte	0
	.4byte	0
.LLST6:
	.4byte	.LVL1-.Ltext0
	.4byte	.LVL2-.Ltext0
	.2byte	0x2
	.byte	0x32
	.byte	0x9f
	.4byte	.LVL2-.Ltext0
	.4byte	.LFE186-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	0
	.4byte	0
.LLST7:
	.4byte	.LFB187-.Ltext0
	.4byte	.LCFI3-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 0
	.4byte	.LCFI3-.Ltext0
	.4byte	.LCFI5-.Ltext0
	.2byte	0x2
	.byte	0x7c
	.sleb128 0
	.4byte	.LCFI5-.Ltext0
	.4byte	.LFE187-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	0
	.4byte	0
.LLST8:
	.4byte	.LVL9-.Ltext0
	.4byte	.LVL11-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	.LVL11-.Ltext0
	.4byte	.LVL12-.Ltext0
	.2byte	0x6
	.byte	0x70
	.sleb128 0
	.byte	0x75
	.sleb128 0
	.byte	0x22
	.byte	0x9f
	.4byte	.LVL12-.Ltext0
	.4byte	.LVL14-.Ltext0
	.2byte	0x1
	.byte	0x58
	.4byte	.LVL14-.Ltext0
	.4byte	.LVL15-.Ltext0
	.2byte	0x6
	.byte	0x70
	.sleb128 0
	.byte	0x75
	.sleb128 0
	.byte	0x22
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST9:
	.4byte	.LVL9-.Ltext0
	.4byte	.LVL11-.Ltext0
	.2byte	0x1
	.byte	0x52
	.4byte	0
	.4byte	0
.LLST10:
	.4byte	.LVL10-.Ltext0
	.4byte	.LVL11-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	.LVL11-.Ltext0
	.4byte	.LFE187-.Ltext0
	.2byte	0x1
	.byte	0x55
	.4byte	0
	.4byte	0
.LLST11:
	.4byte	.LVL11-.Ltext0
	.4byte	.LVL13-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	0
	.4byte	0
.LLST12:
	.4byte	.LFB188-.Ltext0
	.4byte	.LCFI6-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 0
	.4byte	.LCFI6-.Ltext0
	.4byte	.LCFI8-.Ltext0
	.2byte	0x2
	.byte	0x7c
	.sleb128 0
	.4byte	.LCFI8-.Ltext0
	.4byte	.LFE188-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
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
.LASF8:
	.ascii	"long long int\000"
.LASF24:
	.ascii	"console_printk\000"
.LASF15:
	.ascii	"acon_setting\000"
.LASF28:
	.ascii	"masp_hal_secure_algo_deinit\000"
.LASF18:
	.ascii	"p_src\000"
.LASF19:
	.ascii	"src_len\000"
.LASF12:
	.ascii	"float\000"
.LASF14:
	.ascii	"g_AC_CFG\000"
.LASF5:
	.ascii	"unsigned char\000"
.LASF17:
	.ascii	"HACC_V3_Run\000"
.LASF0:
	.ascii	"long unsigned int\000"
.LASF7:
	.ascii	"short unsigned int\000"
.LASF31:
	.ascii	"/proj/mtk02464/Public/P4Android/ALPS_SW/TRUNK/KK/al"
	.ascii	"ps/out/target/product/mt6752_evb/obj/KERNEL_OBJ\000"
.LASF21:
	.ascii	"HACC_V3_Terminate\000"
.LASF23:
	.ascii	"elf_hwcap\000"
.LASF30:
	.ascii	"/proj/mtk02464/Public/P4Android/ALPS_SW/TRUNK/KK/al"
	.ascii	"ps/kernel-3.10/mediatek/platform/mt6752/kernel/driv"
	.ascii	"ers/masp/mach/hacc_hk.c\000"
.LASF32:
	.ascii	"uint32\000"
.LASF27:
	.ascii	"masp_hal_secure_algo_init\000"
.LASF3:
	.ascii	"unsigned int\000"
.LASF9:
	.ascii	"long long unsigned int\000"
.LASF16:
	.ascii	"HACC_V3_Init\000"
.LASF20:
	.ascii	"p_dst\000"
.LASF1:
	.ascii	"sizetype\000"
.LASF29:
	.ascii	"GNU C 4.8 -fpreprocessed -mbionic -mlittle-endian -"
	.ascii	"mapcs -mno-sched-prolog -mabi=aapcs-linux -mno-thum"
	.ascii	"b-interwork -marm -march=armv7-a -mfloat-abi=softfp"
	.ascii	" -mfpu=vfp -mtls-dialect=gnu -gdwarf-2 -O2 -fno-str"
	.ascii	"ict-aliasing -fno-common -fno-delete-null-pointer-c"
	.ascii	"hecks -fno-pic -fno-dwarf2-cfi-asm -fno-stack-prote"
	.ascii	"ctor -fno-omit-frame-pointer -fno-optimize-sibling-"
	.ascii	"calls -fno-strict-overflow -fconserve-stack\000"
.LASF2:
	.ascii	"char\000"
.LASF22:
	.ascii	"g_CFG_RANDOM_PATTERN\000"
.LASF6:
	.ascii	"short int\000"
.LASF25:
	.ascii	"hex_asc\000"
.LASF26:
	.ascii	"hacc_base\000"
.LASF10:
	.ascii	"long int\000"
.LASF13:
	.ascii	"encode\000"
.LASF4:
	.ascii	"signed char\000"
.LASF11:
	.ascii	"_Bool\000"
	.ident	"GCC: (GNU) 4.8"
	.section	.note.GNU-stack,"",%progbits
