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
	.file	"sec_uid.c"
	.text
.Ltext0:
	.align	2
	.global	masp_hal_get_uuid
	.type	masp_hal_get_uuid, %function
masp_hal_get_uuid:
.LFB150:
	.file 1 "alps/kernel-3.10/mediatek/platform/mt6752/kernel/drivers/masp/mach/sec_uid.c"
	.loc 1 43 0
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
.LVL0:
	mov	ip, sp
.LCFI0:
	stmfd	sp!, {fp, ip, lr, pc}
.LCFI1:
	sub	fp, ip, #4
.LCFI2:
.LVL1:
.LBB10:
.LBB11:
	.file 2 "alps/kernel-3.10/arch/arm/include/asm/io.h"
	.loc 2 107 0
	mov	r2, #24576
.LBE11:
.LBE10:
	.loc 1 43 0
	mov	r3, r0
.LBB13:
.LBB12:
	.loc 2 107 0
	movt	r2, 61472
#APP
@ 107 "alps/kernel-3.10/arch/arm/include/asm/io.h" 1
	ldr r1, [r2, #320]
@ 0 "" 2
.LVL2:
.LBE12:
.LBE13:
.LBB14:
.LBB15:
	mov	r2, #24576
.LBE15:
.LBE14:
	.loc 1 44 0
	str	r1, [r0]
.LVL3:
.LBB17:
.LBB16:
	.loc 2 107 0
	movt	r2, 61472
#APP
@ 107 "alps/kernel-3.10/arch/arm/include/asm/io.h" 1
	ldr r1, [r2, #324]
@ 0 "" 2
.LVL4:
.LBE16:
.LBE17:
.LBB18:
.LBB19:
	mov	r2, #24576
.LBE19:
.LBE18:
	.loc 1 45 0
	str	r1, [r0, #4]
.LVL5:
.LBB21:
.LBB20:
	.loc 2 107 0
	movt	r2, 61472
#APP
@ 107 "alps/kernel-3.10/arch/arm/include/asm/io.h" 1
	ldr r1, [r2, #320]
@ 0 "" 2
.LVL6:
.LBE20:
.LBE21:
.LBB22:
.LBB23:
	mov	r2, #24576
.LBE23:
.LBE22:
	.loc 1 46 0
	str	r1, [r0, #8]
.LVL7:
.LBB25:
.LBB24:
	.loc 2 107 0
	movt	r2, 61472
#APP
@ 107 "alps/kernel-3.10/arch/arm/include/asm/io.h" 1
	ldr r1, [r2, #324]
@ 0 "" 2
.LVL8:
.LBE24:
.LBE25:
	.loc 1 50 0
	mov	r0, #0
.LVL9:
	.loc 1 47 0
	str	r1, [r3, #12]
	.loc 1 50 0
	ldmfd	sp, {fp, sp, pc}
.LFE150:
	.size	masp_hal_get_uuid, .-masp_hal_get_uuid
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
	.4byte	.LFB150
	.4byte	.LFE150-.LFB150
	.byte	0x4
	.4byte	.LCFI0-.LFB150
	.byte	0xd
	.uleb128 0xc
	.byte	0x4
	.4byte	.LCFI1-.LCFI0
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
	.text
.Letext0:
	.file 3 "alps/kernel-3.10/arch/arm/include/asm/hwcap.h"
	.file 4 "alps/kernel-3.10/include/linux/printk.h"
	.file 5 "alps/kernel-3.10/include/linux/kernel.h"
	.file 6 "alps/kernel-3.10/include/asm-generic/int-ll64.h"
	.section	.debug_info,"",%progbits
.Ldebug_info0:
	.4byte	0x1e1
	.2byte	0x2
	.4byte	.Ldebug_abbrev0
	.byte	0x4
	.uleb128 0x1
	.4byte	.LASF15
	.byte	0x1
	.4byte	.LASF16
	.4byte	.LASF17
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
	.uleb128 0x4
	.ascii	"u32\000"
	.byte	0x6
	.byte	0x16
	.4byte	0x3a
	.uleb128 0x2
	.byte	0x4
	.byte	0x5
	.4byte	.LASF10
	.uleb128 0x2
	.byte	0x1
	.byte	0x2
	.4byte	.LASF11
	.uleb128 0x5
	.4byte	.LASF18
	.byte	0x2
	.byte	0x68
	.byte	0x1
	.4byte	0x72
	.byte	0x3
	.4byte	0xb3
	.uleb128 0x6
	.4byte	.LASF19
	.byte	0x2
	.byte	0x68
	.4byte	0xb3
	.uleb128 0x7
	.ascii	"val\000"
	.byte	0x2
	.byte	0x6a
	.4byte	0x72
	.byte	0
	.uleb128 0x8
	.byte	0x4
	.4byte	0xb9
	.uleb128 0x9
	.4byte	0xbe
	.uleb128 0xa
	.uleb128 0xb
	.byte	0x1
	.4byte	.LASF20
	.byte	0x1
	.byte	0x2a
	.byte	0x1
	.4byte	0x5d
	.4byte	.LFB150
	.4byte	.LFE150
	.4byte	.LLST0
	.byte	0x1
	.4byte	0x19b
	.uleb128 0xc
	.4byte	.LASF21
	.byte	0x1
	.byte	0x2a
	.4byte	0x19b
	.4byte	.LLST1
	.uleb128 0xd
	.4byte	0x8b
	.4byte	.LBB10
	.4byte	.Ldebug_ranges0+0
	.byte	0x1
	.byte	0x2c
	.4byte	0x119
	.uleb128 0xe
	.4byte	0x9c
	.sleb128 -266313408
	.uleb128 0xf
	.4byte	.Ldebug_ranges0+0
	.uleb128 0x10
	.4byte	0xa7
	.4byte	.LLST2
	.byte	0
	.byte	0
	.uleb128 0xd
	.4byte	0x8b
	.4byte	.LBB14
	.4byte	.Ldebug_ranges0+0x18
	.byte	0x1
	.byte	0x2d
	.4byte	0x146
	.uleb128 0xe
	.4byte	0x9c
	.sleb128 -266313404
	.uleb128 0xf
	.4byte	.Ldebug_ranges0+0x18
	.uleb128 0x10
	.4byte	0xa7
	.4byte	.LLST3
	.byte	0
	.byte	0
	.uleb128 0xd
	.4byte	0x8b
	.4byte	.LBB18
	.4byte	.Ldebug_ranges0+0x30
	.byte	0x1
	.byte	0x2e
	.4byte	0x173
	.uleb128 0xe
	.4byte	0x9c
	.sleb128 -266313408
	.uleb128 0xf
	.4byte	.Ldebug_ranges0+0x30
	.uleb128 0x10
	.4byte	0xa7
	.4byte	.LLST4
	.byte	0
	.byte	0
	.uleb128 0x11
	.4byte	0x8b
	.4byte	.LBB22
	.4byte	.Ldebug_ranges0+0x48
	.byte	0x1
	.byte	0x2f
	.uleb128 0xe
	.4byte	0x9c
	.sleb128 -266313404
	.uleb128 0xf
	.4byte	.Ldebug_ranges0+0x48
	.uleb128 0x12
	.4byte	0xa7
	.byte	0x1
	.byte	0x51
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x8
	.byte	0x4
	.4byte	0x3a
	.uleb128 0x13
	.4byte	.LASF12
	.byte	0x3
	.byte	0xc
	.4byte	0x3a
	.byte	0x1
	.byte	0x1
	.uleb128 0x14
	.4byte	0x5d
	.4byte	0x1b9
	.uleb128 0x15
	.byte	0
	.uleb128 0x13
	.4byte	.LASF13
	.byte	0x4
	.byte	0x24
	.4byte	0x1ae
	.byte	0x1
	.byte	0x1
	.uleb128 0x14
	.4byte	0x33
	.4byte	0x1d1
	.uleb128 0x15
	.byte	0
	.uleb128 0x16
	.4byte	.LASF14
	.byte	0x5
	.2byte	0x1b2
	.4byte	0x1df
	.byte	0x1
	.byte	0x1
	.uleb128 0x9
	.4byte	0x1c6
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
	.uleb128 0x8
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
	.byte	0
	.byte	0
	.uleb128 0x8
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x9
	.uleb128 0x26
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xa
	.uleb128 0x35
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0xb
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
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0xd
	.uleb128 0x1d
	.byte	0x1
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x52
	.uleb128 0x1
	.uleb128 0x55
	.uleb128 0x6
	.uleb128 0x58
	.uleb128 0xb
	.uleb128 0x59
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xe
	.uleb128 0x5
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x1c
	.uleb128 0xd
	.byte	0
	.byte	0
	.uleb128 0xf
	.uleb128 0xb
	.byte	0x1
	.uleb128 0x55
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x10
	.uleb128 0x34
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x11
	.uleb128 0x1d
	.byte	0x1
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x52
	.uleb128 0x1
	.uleb128 0x55
	.uleb128 0x6
	.uleb128 0x58
	.uleb128 0xb
	.uleb128 0x59
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x12
	.uleb128 0x34
	.byte	0
	.uleb128 0x31
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
	.uleb128 0x1
	.byte	0x1
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x15
	.uleb128 0x21
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x16
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
	.byte	0
	.section	.debug_loc,"",%progbits
.Ldebug_loc0:
.LLST0:
	.4byte	.LFB150-.Ltext0
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
	.4byte	.LFE150-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	0
	.4byte	0
.LLST1:
	.4byte	.LVL0-.Ltext0
	.4byte	.LVL9-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	.LVL9-.Ltext0
	.4byte	.LFE150-.Ltext0
	.2byte	0x1
	.byte	0x53
	.4byte	0
	.4byte	0
.LLST2:
	.4byte	.LVL2-.Ltext0
	.4byte	.LVL4-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	.LVL4-.Ltext0
	.4byte	.LVL9-.Ltext0
	.2byte	0x2
	.byte	0x70
	.sleb128 0
	.4byte	.LVL9-.Ltext0
	.4byte	.LFE150-.Ltext0
	.2byte	0x2
	.byte	0x73
	.sleb128 0
	.4byte	0
	.4byte	0
.LLST3:
	.4byte	.LVL4-.Ltext0
	.4byte	.LVL6-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	.LVL6-.Ltext0
	.4byte	.LVL9-.Ltext0
	.2byte	0x2
	.byte	0x70
	.sleb128 4
	.4byte	.LVL9-.Ltext0
	.4byte	.LFE150-.Ltext0
	.2byte	0x2
	.byte	0x73
	.sleb128 4
	.4byte	0
	.4byte	0
.LLST4:
	.4byte	.LVL6-.Ltext0
	.4byte	.LVL8-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	.LVL8-.Ltext0
	.4byte	.LVL9-.Ltext0
	.2byte	0x2
	.byte	0x70
	.sleb128 8
	.4byte	.LVL9-.Ltext0
	.4byte	.LFE150-.Ltext0
	.2byte	0x2
	.byte	0x73
	.sleb128 8
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
	.section	.debug_ranges,"",%progbits
.Ldebug_ranges0:
	.4byte	.LBB10-.Ltext0
	.4byte	.LBE10-.Ltext0
	.4byte	.LBB13-.Ltext0
	.4byte	.LBE13-.Ltext0
	.4byte	0
	.4byte	0
	.4byte	.LBB14-.Ltext0
	.4byte	.LBE14-.Ltext0
	.4byte	.LBB17-.Ltext0
	.4byte	.LBE17-.Ltext0
	.4byte	0
	.4byte	0
	.4byte	.LBB18-.Ltext0
	.4byte	.LBE18-.Ltext0
	.4byte	.LBB21-.Ltext0
	.4byte	.LBE21-.Ltext0
	.4byte	0
	.4byte	0
	.4byte	.LBB22-.Ltext0
	.4byte	.LBE22-.Ltext0
	.4byte	.LBB25-.Ltext0
	.4byte	.LBE25-.Ltext0
	.4byte	0
	.4byte	0
	.section	.debug_line,"",%progbits
.Ldebug_line0:
	.section	.debug_str,"MS",%progbits,1
.LASF8:
	.ascii	"long long int\000"
.LASF3:
	.ascii	"unsigned int\000"
.LASF17:
	.ascii	"/proj/mtk02464/Public/P4Android/ALPS_SW/TRUNK/KK/al"
	.ascii	"ps/out/target/product/mt6752_evb/obj/KERNEL_OBJ\000"
.LASF12:
	.ascii	"elf_hwcap\000"
.LASF0:
	.ascii	"long unsigned int\000"
.LASF9:
	.ascii	"long long unsigned int\000"
.LASF20:
	.ascii	"masp_hal_get_uuid\000"
.LASF19:
	.ascii	"addr\000"
.LASF16:
	.ascii	"/proj/mtk02464/Public/P4Android/ALPS_SW/TRUNK/KK/al"
	.ascii	"ps/kernel-3.10/mediatek/platform/mt6752/kernel/driv"
	.ascii	"ers/masp/mach/sec_uid.c\000"
.LASF14:
	.ascii	"hex_asc\000"
.LASF5:
	.ascii	"unsigned char\000"
.LASF2:
	.ascii	"char\000"
.LASF21:
	.ascii	"uuid\000"
.LASF10:
	.ascii	"long int\000"
.LASF13:
	.ascii	"console_printk\000"
.LASF11:
	.ascii	"_Bool\000"
.LASF7:
	.ascii	"short unsigned int\000"
.LASF4:
	.ascii	"signed char\000"
.LASF18:
	.ascii	"__raw_readl\000"
.LASF6:
	.ascii	"short int\000"
.LASF15:
	.ascii	"GNU C 4.8 -fpreprocessed -mbionic -mlittle-endian -"
	.ascii	"mapcs -mno-sched-prolog -mabi=aapcs-linux -mno-thum"
	.ascii	"b-interwork -marm -march=armv7-a -mfloat-abi=softfp"
	.ascii	" -mfpu=vfp -mtls-dialect=gnu -gdwarf-2 -O2 -fno-str"
	.ascii	"ict-aliasing -fno-common -fno-delete-null-pointer-c"
	.ascii	"hecks -fno-pic -fno-dwarf2-cfi-asm -fno-stack-prote"
	.ascii	"ctor -fno-omit-frame-pointer -fno-optimize-sibling-"
	.ascii	"calls -fno-strict-overflow -fconserve-stack\000"
.LASF1:
	.ascii	"sizetype\000"
	.ident	"GCC: (GNU) 4.8"
	.section	.note.GNU-stack,"",%progbits
