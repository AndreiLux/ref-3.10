/**********************************************************************/
/***************           MTK CONFIDENTIAL            ****************/
/***************                                       ****************/
/***************   Description : MT8118 MTKPrintf      ****************/
/***************                 Procedure             ****************/
/***************                                       ****************/
/***************       Company : MediaTek Inc.         ****************/
/***************    Programmer : Ted Hu                ****************/
/**********************************************************************/
/* #define _IRQ_FIQ_PROC_ */
#include "vdec_verify_irq_fiq_proc.h"
#include "vdec_verify_mpv_prov.h"
#include <linux/interrupt.h>
#include <mach/irqs.h>


/* ********************************************************************* */
/* Function    : void vVldIrq0(void) */
/* Description : Clear picture info in frame buffer */
/* Parameter   : None */
/* Return      : None */
/* ********************************************************************* */
void vVldIrq0(void)
{
	_fgVDecComplete[0] = TRUE;
}

/* ********************************************************************* */
/* Function    : void vVldIrq1(void) */
/* Description : Clear picture info in frame buffer */
/* Parameter   : None */
/* Return      : None */
/* ********************************************************************* */
void vVldIrq1(void)
{
	_fgVDecComplete[1] = TRUE;
}

#if 0
/* ********************************************************************* */
/* Function    : void vVDec0IrqProc(UINT16 u2Vector) */
/* Description : Irq Service routine. */
/* Parameter   : None */
/* Return      : None */
/* ********************************************************************* */
void vVDec0IrqProc(UINT16 u2Vector)
{
#ifndef IRQ_DISABLE
/* BIM_ClearIrq(VECTOR_VDFUL); */
	vVldIrq0();
#endif
}

/* ********************************************************************* */
/* Function    : void vVDec1IrqProc(void) */
/* Description : Irq Service routine. */
/* Parameter   : None */
/* Return      : None */
/* ********************************************************************* */
void vVDec1IrqProc(UINT16 u2Vector)
{
#ifndef IRQ_DISABLE
/* BIM_ClearIrq(VECTOR_VDLIT); */
	vVldIrq1();
#endif
}
#else

/* ********************************************************************* */
/* Function    : void vVDec0IrqProc(UINT16 u2Vector) */
/* Description : Irq Service routine. */
/* Parameter   : None */
/* Return      : None */
/* ********************************************************************* */
irqreturn_t vVDec0IrqProc(int irq, void *dev_id)
{
#ifndef IRQ_DISABLE
/* BIM_ClearIrq(VECTOR_VDFUL); */
	vVldIrq0();
	return IRQ_HANDLED;
#endif
}

/* ********************************************************************* */
/* Function    : void vVDec1IrqProc(void) */
/* Description : Irq Service routine. */
/* Parameter   : None */
/* Return      : None */
/* ********************************************************************* */
irqreturn_t vVDec1IrqProc(int irq, void *dev_id)
{
#ifndef IRQ_DISABLE
/* BIM_ClearIrq(VECTOR_VDLIT); */
	vVldIrq1();
	return IRQ_HANDLED;
#endif
}



#endif
