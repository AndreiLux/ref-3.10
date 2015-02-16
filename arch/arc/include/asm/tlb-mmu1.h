/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_TLB_MMU_V1_H__
#define __ASM_TLB_MMU_V1_H__

#if defined(__ASSEMBLY__) && defined(CONFIG_ARC_MMU_VER == 1)

#include <asm/tlb.h>

.macro TLB_WRITE_HEURISTICS

#define JH_HACK1
#undef JH_HACK2
#undef JH_HACK3

#ifdef JH_HACK3
; Calculate set index for 2-way MMU
; -avoiding use of GetIndex from MMU
;   and its unpleasant LFSR pseudo-random sequence
;
; r1 = TLBPD0 from TLB_RELOAD above
;
; -- jh_ex_way_set not cleared on startup
;    didn't want to change setup.c
;    hence extra instruction to clean
;
; -- should be in cache since in same line
;    as r0/r1 saves above
;
ld  r0,[jh_ex_way_sel]  ; victim pointer
and r0,r0,1         ; clean
xor.f   r0,r0,1         ; flip
st  r0,[jh_ex_way_sel]  ; store back
asr r0,r1,12        ; get set # <<1, note bit 12=R=0
or.nz   r0,r0,1         ; set way bit
and r0,r0,0xff      ; clean
sr  r0,[ARC_REG_TLBINDEX]
#endif

#ifdef JH_HACK2
; JH hack #2
;  Faster than hack #1 in non-thrash case, but hard-coded for 2-way MMU
;  Slower in thrash case (where it matters) because more code is executed
;  Inefficient due to two-register paradigm of this miss handler
;

lr      r0,[eret]               
xor     r0,r0,r1                
and.f   r0,r0,0x000fe000        
bne     88f                     

lr      r0,[eret]               
and     r0,r0,PAGE_MASK         
; lr  r1,[ARC_REG_TLBPD0]     
and     r1,r1,0xff              
or      r0,r0,r1                

lr      r1,[ARC_REG_TLBPD0]     
sr      r0,[ARC_REG_TLBPD0]     
sr      TLBProbe, [ARC_REG_TLBCOMMAND] 
lr      r0,[ARC_REG_TLBINDEX]   
sr      r1,[ARC_REG_TLBPD0]     

xor     r0,r0,1                 
b.d     89f
sr      r0,[ARC_REG_TLBINDEX]   
88:
sr  TLBGetIndex, [ARC_REG_TLBCOMMAND]
89:
#endif

#ifdef JH_HACK1
;
; Always checks whether instruction will be kicked out by dtlb miss
;
mov_s   r3, r1                  ; save PD0 prepared by TLB_RELOAD in r3
lr      r0,[eret]               
and     r0,r0,PAGE_MASK         
bmsk    r1,r3,7                 
or_s    r0,r0,r1                

sr      r0,[ARC_REG_TLBPD0]     
sr      TLBProbe, [ARC_REG_TLBCOMMAND] 
lr      r0,[ARC_REG_TLBINDEX]   
sr      r3,[ARC_REG_TLBPD0]     

sr      TLBGetIndex, [ARC_REG_TLBCOMMAND]
lr      r1,[ARC_REG_TLBINDEX]   
cmp     r0,r1                   
xor.eq  r1,r1,1   
sr      r1,[ARC_REG_TLBINDEX]
#endif

.endm

#endif

#endif
