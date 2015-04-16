/*!
 *****************************************************************************
 *
 * @file	   sysbrg_pdump.h
 *
 * The System Bridge pdump interface.
 * ---------------------------------------------------------------------------
 *
 * Copyright (c) Imagination Technologies Ltd.
 * 
 * The contents of this file are subject to the MIT license as set out below.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN 
 * THE SOFTWARE.
 * 
 * Alternatively, the contents of this file may be used under the terms of the 
 * GNU General Public License Version 2 ("GPL")in which case the provisions of
 * GPL are applicable instead of those above. 
 * 
 * If you wish to allow use of your version of this file only under the terms 
 * of GPL, and not to allow others to use your version of this file under the 
 * terms of the MIT license, indicate your decision by deleting the provisions 
 * above and replace them with the notice and other provisions required by GPL 
 * as set out in the file called “GPLHEADER” included in this distribution. If 
 * you do not delete the provisions above, a recipient may use your version of 
 * this file under the terms of either the MIT license or GPL.
 * 
 * This License is also included in this distribution in the file called 
 * "MIT_COPYING".
 *
 *****************************************************************************/

#ifndef __SYSBRG_PDUMP_H__
#define __SYSBRG_PDUMP_H__

#include <img_defs.h>
#include <linux/interrupt.h>
#include <sysdev_utils1.h>

/*!
******************************************************************************
 Supported Vendor ID (IMG)
******************************************************************************/
#ifdef IMG_PENWELL
#define IMGPCI_VENDOR_ID		0x8086
#define IMGPCI_DEVICE_ID                0x0130
#else
#define IMGPCI_VENDOR_ID                0x1010
#define IMGPCI_DEVICE_ID                PCI_ANY_ID
#endif

/*!
******************************************************************************
 Maximum number of mapped regions available through this driver
******************************************************************************/
#define MAX_IMGPCI_MAPS			(3)

/*!
******************************************************************************
 A PCI location reference
******************************************************************************/
struct imgpci_memref
{
	unsigned long	bar;
	unsigned long	offset;
};

/*!
******************************************************************************
 32bit register structure
******************************************************************************/
struct imgpci_reg32
{
	struct imgpci_memref	address;	
	unsigned long			value;
};

/*!
******************************************************************************
 ISR parameters
******************************************************************************/
struct imgpci_isrparams
{
	struct imgpci_memref	addr_status;
	struct imgpci_memref	addr_mask;
	struct imgpci_memref	addr_clear;
};

/*!
******************************************************************************
 Readdata struct used in read() calls
******************************************************************************/
struct imgpci_readdata
{
	unsigned long		event_count;
	unsigned long		int_status;
};

struct imgpci_mem 
{
	struct kobject          kobj;
	unsigned long           addr;
	unsigned long           size;
	IMG_VOID *              internal_addr;
};

struct imgpci_device;

struct imgpci_info 
{
	struct imgpci_device	*imgpci_dev;
	IMG_CHAR *				name;
	IMG_CHAR *              version;
	struct imgpci_mem       mem[MAX_IMGPCI_MAPS];
	long                    irq;
	unsigned long           irq_flags;
	unsigned long			irq_enabled;
	IMG_VOID *              priv;
	
	spinlock_t				ioctl_lock;
	spinlock_t				read_lock;

	irqreturn_t (*handler)(int irq, struct imgpci_info *info);
	IMG_INT32               (*ioctl)(struct imgpci_info *info,	IMG_UINT32 cmd, unsigned long arg);	
	
	struct imgpci_isrparams	isrparams;
	unsigned long			int_disabled;
	unsigned long			int_status;
	unsigned long			int_mask;
};

struct imgpci_device 
{
	struct module			*owner;
	struct device			*dev;
	IMG_INT32    			minor;
	atomic_t				event;
	struct fasync_struct	*async_queue;
	wait_queue_head_t		wait;
	IMG_INT32   			vma_count;
	struct imgpci_info		*info;
	struct kset 			map_attr_kset;
};

/*!
******************************************************************************
 The ioctl number for this device - may need to be registered (check ioctl-number.txt for more info)
******************************************************************************/
#define IMGPCI_IOCTL_MAGIC	'p'

/*!
******************************************************************************
 Write a 32bit word to the specified PCI location
******************************************************************************/
#define IMGPCI_IOCTL_WRITE32		_IOW (IMGPCI_IOCTL_MAGIC, 0, struct imgpci_reg32 *)

/*!
******************************************************************************
 Read a 32bit word from the specified PCI location
******************************************************************************/
#define IMGPCI_IOCTL_READ32			_IOR (IMGPCI_IOCTL_MAGIC, 1, struct imgpci_reg32 *)

/*!
******************************************************************************
 Interrupt enable (The supplied 32bit mask is OR-ed with the internally held mask)
******************************************************************************/
#define IMGPCI_IOCTL_INTEN			_IO (IMGPCI_IOCTL_MAGIC, 2)

/*!
******************************************************************************
 Interrupt disable (The supplied 32bit mask is inverted and AND-ed with the internally held mask)
******************************************************************************/
#define IMGPCI_IOCTL_INTDIS			_IO (IMGPCI_IOCTL_MAGIC, 3)

/*!
******************************************************************************
 Change the default ISR parameters - normally not used!
******************************************************************************/
#define IMGPCI_IOCTL_SETISRPARAMS	_IOW (IMGPCI_IOCTL_MAGIC, 4, struct imgpci_isrparams *)

/*!
******************************************************************************
 Max command number
******************************************************************************/
#define IMGPCI_IOCTL_MAXNR			4

/*!
******************************************************************************

 @Function              imgpci_register_pdump

 @Description
 
 This functions registers the pdump interface within the imgsysbrg driver

 @Input   None
 
 @Output  None
 
 @Return IMG_RESULT :	This function returns either IMG_SUCCESS or an
						error code.

******************************************************************************/
extern IMG_INT32 imgpci_register_pdump(
	IMG_VOID
	);

/*!
******************************************************************************

 @Function              imgpci_unregister_pdump
 
 @Description
 
 This functions unregisters the pdump interface within the imgsysbrg driver

 @Input   None
 
 @Output  None
 
 @Return None
 
******************************************************************************/
extern IMG_VOID imgpci_unregister_pdump(
	IMG_VOID
	);

#endif  /* __SYSBRG_PDUMP_H__  */
