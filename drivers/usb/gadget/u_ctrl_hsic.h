
#ifndef __U_CTRL_HSIC_H
#define __U_CTRL_HSIC_H

#include <linux/usb/composite.h>
#include <linux/usb/cdc.h>
#include <mach/usb_gadget_xport.h>

int ghsic_ctrl_setup(unsigned num_ports, enum gadget_type gtype);

#endif /* __U_CTRL_HSIC_H*/
