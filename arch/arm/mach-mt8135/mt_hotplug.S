       .text
       .global __switch_to_smp
       .global __switch_to_amp
       .global __disable_l2_prefetch
       .global __lock_debug
.equ ACTLR_SMP_BIT ,0x00000040
.equ L2PFR_DISABLE_BIT ,0x00000400
.equ DBGOSDLR_DLK_BIT ,0x00000001

__switch_to_smp:
    MRC p15,0,r0,c1,c0,1
    ORR r0,r0,#ACTLR_SMP_BIT
    MCR p15,0,r0,c1,c0,1
    BX lr

__switch_to_amp:
    MRC p15,0,r0,c1,c0,1
    BIC r0,r0,#ACTLR_SMP_BIT
    MCR p15,0,r0,c1,c0,1
    BX lr

__disable_l2_prefetch:
    MOV r0,#L2PFR_DISABLE_BIT
    MCR p15,1,r0,c15,c0,3

__lock_debug:
    MRC p14,0,r0,c1,c3,4
    ORR r0,r0,#DBGOSDLR_DLK_BIT
    MCR p14,0,r0,c1,c3,4