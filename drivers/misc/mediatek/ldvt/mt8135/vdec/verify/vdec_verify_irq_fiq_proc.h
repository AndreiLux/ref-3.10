#ifndef _VDEC_VERIFY_IRQ_FIQ_PROC_H_
#define _VDEC_VERIFY_IRQ_FIQ_PROC_H_

#include <mach/mt_typedefs.h>
#include <linux/interrupt.h>


#if 0
void vVDec0IrqProc(UINT16 u2Vector);
void vVDec1IrqProc(UINT16 u2Vector);
#else
irqreturn_t vVDec0IrqProc(int irq, void *dev_id);
irqreturn_t vVDec1IrqProc(int irq, void *dev_id);


#endif

#endif
