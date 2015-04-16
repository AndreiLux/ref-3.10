/*!
 *****************************************************************************
 *
 * @file	   default.c
 *
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

#include <asm/io.h>
#include <linux/mm.h>
#include <asm/page.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kobject.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <system.h>
#include <sysmem_utils.h>
#include <sysdev_utils.h>

static int module_irq = -1;
static void *reg_base_addr;
static unsigned int module_reg_base = 0;
static unsigned int module_reg_size = 0;

/* Device information...*/
static SYSDEVU_sInfo *gpsInfo = IMG_NULL;
static bool device_detected = IMG_FALSE;

#define DEVICE_NAME "topaz_hw"

int driver_probe(struct platform_device *device);

static struct platform_driver local_driver = {
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
	},
	.probe = driver_probe
};


int driver_probe(struct platform_device *device) {
	printk("probing platform device : %s\n", device->name);

	// registers
	IMG_ASSERT(device->resource[0].flags == IORESOURCE_MEM);
	module_reg_base = device->resource[0].start;
	module_reg_size = device->resource[0].end - device->resource[0].start + 1;

	// interrupt line
	IMG_ASSERT(device->resource[1].flags == IORESOURCE_IRQ);
	module_irq = device->resource[1].start;

	device_detected = IMG_TRUE;
	return 0;
}



static irqreturn_t handle_interrupt(int irq, void *dev_id)
{
	bool handled;
	if ( (gpsInfo != IMG_NULL) && (gpsInfo->pfnDevKmLisr != IMG_NULL) )
	{
		SYSOSKM_DisableInt();
		handled = gpsInfo->pfnDevKmLisr(gpsInfo->pvParam);
		SYSOSKM_EnableInt();

		if (handled)
		{
			/* Signal this...*/
			return IRQ_HANDLED;
		}
	}
	return IRQ_NONE;
}

static void handle_suspend(SYSDEVU_sInfo *dev, IMG_BOOL forAPM)
{
	/* customer specific code for handling device suspend ( disabling clocks ) */
}

static void handle_resume(SYSDEVU_sInfo *dev, IMG_BOOL forAPM)
{
	/* customer specific code for handling device resume ( enabling clocks ) */
}


/*!
******************************************************************************

 @Function				SYSDEVU1_Deinitialise

******************************************************************************/
static void free_device(SYSDEVU_sInfo *dev)
{
	if(!device_detected)
		return;
	iounmap(reg_base_addr);
	free_irq(module_irq, gpsInfo);
}

IMG_RESULT SYSDEVU_RegisterDriver(SYSDEVU_sInfo *sysdev) {
	int ret = 0;
	gpsInfo = sysdev;

	ret = platform_driver_register(&local_driver);
	if (ret != 0) {
		ret = IMG_ERROR_DEVICE_NOT_FOUND;
		goto failure_register;
	}

	if (device_detected != IMG_TRUE) {
		ret = IMG_ERROR_DEVICE_NOT_FOUND;
		goto failure_detect;
	}

	reg_base_addr = ioremap(module_reg_base, module_reg_size);
	if (!reg_base_addr) {
		printk(KERN_ERR "Failed to remap registers\n");
		ret = IMG_ERROR_GENERIC_FAILURE;
		goto failure_map_reg;
	}

	if (request_irq(module_irq, handle_interrupt, 0, DEVICE_NAME, gpsInfo)) {
		printk(KERN_ERR "Failed to get IRQ\n");
		ret = IMG_ERROR_GENERIC_FAILURE;
		goto failure_request_irq;
	}

	SYSDEVU_SetDevMap(sysdev, ((IMG_UINT32 *)module_reg_base), ((IMG_UINT32 *)reg_base_addr), module_reg_size, IMG_NULL, IMG_NULL, 0, 0);
	SYSDEVU_SetDevFuncs(sysdev, free_device, handle_resume, handle_suspend);

	ret = SYSMEMKM_AddSystemMemory(&sysdev->sMemPool);
	if(IMG_SUCCESS != ret)
	{
		goto failure_add_sys_mem;
	}

	/* Return success...*/
	return IMG_SUCCESS;

failure_add_sys_mem:
failure_request_irq:
	iounmap(reg_base_addr);
failure_map_reg:
failure_detect:
	platform_driver_unregister(&local_driver);
failure_register:
	gpsInfo = IMG_NULL;
	return ret;
}

IMG_RESULT SYSDEVU_UnRegisterDriver(SYSDEVU_sInfo *sysdev) {
	SYSMEMU_RemoveMemoryHeap(sysdev->sMemPool);
	sysdev->pfnFreeDevice(sysdev);
	return IMG_SUCCESS;
}
