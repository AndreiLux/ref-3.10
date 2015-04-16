/*!
 *****************************************************************************
 *
 * @file       pci-linux-cdrv.c
 *
 * This file contains kernel module device interface implementation
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

#include "sysdev_utils1.h"
#include "sysmem_utils.h"
#include <linux/pci.h>
#include <linux/kobject.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <asm/page.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/module.h>
#include "img_defs.h"
#include "target.h"
#include "sysenv_api_km.h"
#ifdef PDUMP_INTERFACE
#include "sysbrg_pdump.h"
#endif


#if defined(ANDROID_ION_BUFFERS) && !defined(ANDROID_ION_SGX)
#include <linux/ion.h>

extern struct ion_device *ion_device_create(long (*custom_ioctl)
                                            (struct ion_client *client,
                                            unsigned int cmd,
                                            unsigned long arg));
extern struct ion_heap *ion_heap_create(struct ion_platform_heap *heap_data);
extern void ion_device_add_heap(struct ion_device *dev, struct ion_heap *heap);
#endif

#ifndef IMG_MEMALLOC_UNIFIED_VMALLOC
//module parameters used if using reserved video memory, rather than vmalloc
static unsigned int module_ram_base = 0;
static unsigned int module_ram_size = 0;
module_param(module_ram_base, uint, S_IRUGO);
module_param(module_ram_size, uint, S_IRUGO);
static void* ram_base_vaddr;
#endif
#define PCI_MAX_REGIONS                (1)            // !< Maximum allowed memory regions mappable through the driver

#define PCI_CDV_VENDOR_ID               0x8086
#define PCI_CDV_DEVICE_ID0              0x0be0
#define PCI_CDV_DEVICE_ID1              0x0be1
#define PCI_CDV_DEVICE_ID2              0x0be2
#define PCI_CDV_DEVICE_ID3              0x0be3
#define PCI_CDV_DEVICE_ID4              0x0be4
#define PCI_CDV_DEVICE_ID5              0x0be5
#define PCI_CDV_DEVICE_ID6              0x0be6
#define PCI_CDV_DEVICE_ID7              0x0be7
#define PCI_CDV_BAR                     0
#define PCI_CDV_MSVDX_OFFSET            0x00090000    // offset into the BAR of the MSVDX component
#define PCI_CDV_TOPAZ_OFFSET            0x000C0000    // offset into the BAR of the TOPAZSX component
#define PCI_CDV_MSVDX_INT_MASK          0x00080000    // CDV_SGX_INT_MASK=0x40000
#define PCI_CDV_TOPAZ_INT_MASK          0x00100000    // guesswork: this is the value on older cores
#define PCI_CDV_TOPAZ_MSVDX_INT_MASK    (PCI_CDV_MSVDX_INT_MASK | PCI_CDV_TOPAZ_INT_MASK)
#define PCI_CDV_INT_ENABLE_REG          0x20a0
#define PCI_CDV_IDENTITY_REG            0x20a4
#define PCI_CDV_INT_MASK_REG            0x20a8
#define PCI_CDV_INT_STATUS_REG          0x20ac
#define PCI_CDV_MSVDX_SIZE              0x10000

/* Some of local function prototypes */
static int pci_PciProbeCb (struct pci_dev *dev, const struct pci_device_id *id);
static void pci_PciRemoveCb (struct pci_dev *dev);
static IMG_VOID pci_DisableInterrupts(struct pci_dev *dev);

static IMG_BOOL gbDevDetected = 0;

static struct pci_device_id pci_pci_ids[] =
{
    {.vendor = PCI_CDV_VENDOR_ID, .device = PCI_CDV_DEVICE_ID0, .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID, 0, 0, 0,},
    {.vendor = PCI_CDV_VENDOR_ID, .device = PCI_CDV_DEVICE_ID1, .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID, 0, 0, 0,},
    {.vendor = PCI_CDV_VENDOR_ID, .device = PCI_CDV_DEVICE_ID2, .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID, 0, 0, 0,},
    {.vendor = PCI_CDV_VENDOR_ID, .device = PCI_CDV_DEVICE_ID3, .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID, 0, 0, 0,},
    {.vendor = PCI_CDV_VENDOR_ID, .device = PCI_CDV_DEVICE_ID4, .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID, 0, 0, 0,},
    {.vendor = PCI_CDV_VENDOR_ID, .device = PCI_CDV_DEVICE_ID5, .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID, 0, 0, 0,},
    {.vendor = PCI_CDV_VENDOR_ID, .device = PCI_CDV_DEVICE_ID6, .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID, 0, 0, 0,},
    {.vendor = PCI_CDV_VENDOR_ID, .device = PCI_CDV_DEVICE_ID7, .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID, 0, 0, 0,},
    { 0,0,0,0,0,0,0, }
};

struct imgpci_prvdata {
    int irq;
    struct {
        unsigned long addr;
        unsigned long size;
        void __iomem *km_addr;
    } memmap[2];
    SYSDEVU_sInfo *dev;
    struct pci_dev *pci_dev;
};

struct imgpci_prvdata imgpci_prvdata;

static struct img_pci_driver {
    struct pci_dev *pci_dev;
    struct pci_driver pci_driver;
} img_pci_driver =
{
    .pci_driver = {
        .name     = "imgpciif",    /* can change this name as necessary */
        .id_table = pci_pci_ids,
        .probe    = pci_PciProbeCb,
        .remove   = pci_PciRemoveCb,
    }
};

static unsigned int readreg32(struct pci_dev *dev, int bar, unsigned long offset)
{
    void __iomem *reg;
    struct imgpci_prvdata *data = &imgpci_prvdata;

    reg = (void __iomem *)(data->memmap[bar].km_addr + offset);
    return ioread32(reg);
}

static void    writereg32(struct pci_dev *dev, int bar, unsigned long offset, int val)
{
    void __iomem *reg;
    struct imgpci_prvdata *data = &imgpci_prvdata;

    reg = (void __iomem *)(data->memmap[bar].km_addr + offset);
    iowrite32(val, reg);
}

/*!
******************************************************************************

 @Function              pci_IrqHandler

******************************************************************************/
static irqreturn_t pci_IrqHandler(
    int     irq,
    void *  dev_info
)
{
    IMG_UINT32  ui32Identity;
    IMG_UINT32  ui32Mask;
    IMG_BOOL    bHandled;

    struct pci_dev *dev = (struct pci_dev *)dev_info;
    struct imgpci_prvdata *data = &imgpci_prvdata;

    if(dev_info == NULL)
    {
        // spurious interrupt: not yet initialised.
        return IRQ_NONE;
    }

    ui32Identity = readreg32(dev, PCI_CDV_BAR, PCI_CDV_IDENTITY_REG);
    ui32Mask     = readreg32(dev, PCI_CDV_BAR, PCI_CDV_INT_MASK_REG);

    /* If there is a LISR registered...*/
    if ( data && (data->dev) && (data->dev->pfnDevKmLisr != IMG_NULL)
         && ((ui32Identity & PCI_CDV_TOPAZ_MSVDX_INT_MASK) != 0)
         && ((ui32Mask     & PCI_CDV_TOPAZ_MSVDX_INT_MASK) == 0))
    {
        /* Call it...*/
        SYSOSKM_DisableInt();
        bHandled = data->dev->pfnDevKmLisr(data->dev->pvParam);
        SYSOSKM_EnableInt();

        /* If the LISR handled the interrupt...*/
        if (bHandled)
        {
            /* clear the interrupt */
            writereg32(dev, PCI_CDV_BAR, PCI_CDV_IDENTITY_REG, ui32Identity & PCI_CDV_TOPAZ_MSVDX_INT_MASK);

            /* Signal this...*/
            return IRQ_HANDLED;
        }
    }
    return IRQ_NONE;
}


/*!
******************************************************************************

 @Function              pci_PciManualProbe

******************************************************************************/
static int pci_PciManualProbe (
    struct pci_dev *  dev
)
{
    IMG_UINT32  ui32Result;
    int res;
    struct imgpci_prvdata *data = &imgpci_prvdata;

    printk(KERN_INFO "pci_PciManualProbe trying 0x%X:0x%X",
           (unsigned) dev->vendor, (unsigned) dev->device);

    /* Enable the device */
    if ((res = pci_enable_device(dev)) != 0)
    {
        printk(KERN_ERR "pci_enable_device failed: %d", res);
        return -ENODEV;
    }

    memset(data, 0, sizeof(*data));

    /* PCI I/O and memory resources already reserved by SGX driver*/

    /* Get the IRQ...*/
    data->irq = dev->irq;
    data->pci_dev = dev;
    img_pci_driver.pci_dev = dev;

    /* Create a kernel space mapping for each of the bars */

    data->memmap[0].addr    = pci_resource_start(dev, 0);
    data->memmap[0].size    = pci_resource_len(dev, 0);
    data->memmap[0].km_addr = ioremap(pci_resource_start(dev, 0), pci_resource_len(dev, 0));

    /* Install the ISR callback...*/
    ui32Result = request_irq(data->irq, &pci_IrqHandler, IRQF_SHARED, "VDEC", (void *)dev);
    IMG_ASSERT(ui32Result == 0);
    if (ui32Result != 0)
    {
        pci_disable_device(dev);
        return -ENODEV;
    }

    /* Device detected */
    gbDevDetected = IMG_TRUE;

    return 0;
}


/*!
******************************************************************************
 @Function              pci_PciProbeCb

******************************************************************************/
static int pci_PciProbeCb (
    struct pci_dev *              dev,
    const struct pci_device_id *  id
)
{
    IMG_UINT32  ui32Result;
    int res;
    struct imgpci_prvdata *data = &imgpci_prvdata;


    printk(KERN_INFO "pci_PciProbeCb trying 0x%X:0x%X",
           (unsigned) dev->vendor, (unsigned) dev->device);

    /* Enable the device */
    if ((res = pci_enable_device(dev)) != 0)
    {
        printk(KERN_ERR "pci_enable_device failed: %d", res);
        goto out_free;
    }

    /* Reserve PCI I/O and memory resources */
    if ((res = pci_request_regions(dev, "imgpci")) != 0)
    {
        printk(KERN_INFO "pci_request_regions failed: %d", res);
        goto out_disable;
    }
    /* Create a kernel space mapping for each of the bars */
    memset(data, 0, sizeof(*data));

    /* Get the IRQ...*/
#if 0
    ui32Result = pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq);
    IMG_ASSERT(ui32Result == 0);
    if (ui32Result != 0)
    {
        return -ENODEV;
    }
    IMG_ASSERT(dev->irq == gui8Irq);
#else
    data->irq = dev->irq;
#endif

    /* Create a kernel space mapping for each of the bars */

    data->memmap[0].addr    = pci_resource_start(dev, 0);
    data->memmap[0].size    = pci_resource_len(dev, 0);
    data->memmap[0].km_addr = ioremap(pci_resource_start(dev, 0), pci_resource_len(dev, 0));

    printk("%s bar %u address 0x%lx size 0x%lx km addr 0x%p\n", __func__,
            0, data->memmap[0].addr, data->memmap[0].size, data->memmap[0].km_addr);


#ifndef IMG_MEMALLOC_UNIFIED_VMALLOC
    ram_base_vaddr = ioremap(module_ram_base, module_ram_size);
    if (!ram_base_vaddr) {
      printk(KERN_ERR "%s: Failed to remap RAM\n", __func__);
        return IMG_ERROR_GENERIC_FAILURE;
    }
#endif
    data->pci_dev = dev;
    img_pci_driver.pci_dev = dev;

    /* Install the ISR callback...*/
    ui32Result = request_irq(data->irq, &pci_IrqHandler, IRQF_SHARED, "VDEC", (void *)dev);
    IMG_ASSERT(ui32Result == 0);
    if (ui32Result != 0)
    {
        return -ENODEV;
    }

    gbDevDetected = IMG_TRUE;

    return 0;

out_disable:
    pci_disable_device(dev);

out_free:
    return -ENODEV;
}


/*!
******************************************************************************

 @Function              pci_PciRemoveCb

******************************************************************************/
static void pci_PciRemoveCb(
    struct pci_dev *  dev
)
{
    pci_DisableInterrupts(dev);
    pci_release_regions(dev);
    pci_disable_device(dev);

#ifndef IMG_MEMALLOC_UNIFIED_VMALLOC
    iounmap(ram_base_vaddr);
#endif
}



/*!
******************************************************************************

 @Function                pci_EnableInterrupts

******************************************************************************/
static IMG_RESULT pci_EnableInterrupts(
    struct pci_dev *  dev
)
{
    IMG_UINT32 ui32Bits = PCI_CDV_TOPAZ_MSVDX_INT_MASK;
    IMG_UINT32 ui32Mask;
    IMG_UINT32 ui32Enable;

    // disable interrupts during read-modify-write of interrupt registers
    // in case they are being modified by (eg) SGX driver
    SYSOSKM_DisableInt();
    // clear any spurious MSVDX interrupts
    writereg32(dev, PCI_CDV_BAR, PCI_CDV_IDENTITY_REG, ui32Bits);
    // enable MSVDX interrupts
    ui32Mask = readreg32(dev, PCI_CDV_BAR, PCI_CDV_INT_MASK_REG);
    writereg32(dev, PCI_CDV_BAR, PCI_CDV_INT_MASK_REG, ui32Mask & (~ui32Bits));
    ui32Enable = readreg32(dev, PCI_CDV_BAR, PCI_CDV_INT_ENABLE_REG);
    writereg32(dev, PCI_CDV_BAR, PCI_CDV_INT_ENABLE_REG, ui32Enable | ui32Bits);

    SYSOSKM_EnableInt();

    return IMG_SUCCESS;
}
/*!
******************************************************************************

 @Function                pci_DisableInterrupts

******************************************************************************/
static IMG_VOID pci_DisableInterrupts(
    struct pci_dev *  dev
)
{
    IMG_UINT32 ui32Bits = PCI_CDV_TOPAZ_MSVDX_INT_MASK;
    IMG_UINT32 ui32Mask;
    IMG_UINT32 ui32Enable;

    // disable interrupts during read-modify-write of interrupt registers
    SYSOSKM_DisableInt();

    // disable MSVDX interrupts
    ui32Enable = readreg32(dev, PCI_CDV_BAR, PCI_CDV_INT_ENABLE_REG);
    writereg32(dev,PCI_CDV_BAR, PCI_CDV_INT_ENABLE_REG, ui32Enable & (~ui32Bits));
    ui32Mask = readreg32(dev,PCI_CDV_BAR, PCI_CDV_INT_MASK_REG);
    writereg32(dev,PCI_CDV_BAR, PCI_CDV_INT_MASK_REG, ui32Mask | ui32Bits);

    SYSOSKM_EnableInt();
}

/*!
******************************************************************************

@Function handle_resume

******************************************************************************/
static IMG_VOID handle_resume(
    SYSDEVU_sInfo *  psInfo,
    IMG_BOOL forAPM
)
{
	if(!forAPM)
		pci_EnableInterrupts(img_pci_driver.pci_dev);
}


/*!
******************************************************************************

@Function handle_suspend

******************************************************************************/
static IMG_VOID handle_suspend(
    SYSDEVU_sInfo *  psInfo,
    IMG_BOOL forAPM
)
{
	if(!forAPM)
		pci_DisableInterrupts(img_pci_driver.pci_dev);
}

/*!
******************************************************************************

 @Function                  SYSDEVU1_FreeDevice

******************************************************************************/
static IMG_VOID free_device(
    SYSDEVU_sInfo *  dev
)
{
    unsigned bar = 0;
    struct imgpci_prvdata *data = &imgpci_prvdata;

    free_irq(data->irq, (void *)data->pci_dev);

    /* Unregister the driver from the OS */
    pci_unregister_driver(&(img_pci_driver.pci_driver));

    iounmap(data->memmap[0].km_addr);
    printk("%s bar %u address 0x%lx size 0x%lx km addr 0x%p\n", __func__,
            bar, data->memmap[0].addr, data->memmap[0].size, data->memmap[0].km_addr);

    data->memmap[bar].km_addr = NULL;
}


/*!
******************************************************************************

 @Function                SYSDEVU_VDECRegisterDriver

******************************************************************************/
IMG_RESULT SYSDEVU_VDECRegisterDriver(
    SYSDEVU_sInfo *  psInfo
)
{
    IMG_UINT32 ui32Result = IMG_SUCCESS;
    struct pci_dev *dev;
    struct imgpci_prvdata *data;

    /* Register pci driver with the OS */
    ui32Result = pci_register_driver(&img_pci_driver.pci_driver);
    IMG_ASSERT(ui32Result == 0);
    if (ui32Result != 0)
    {
        printk("pci_register_driver failed \r\n");
        /* Unregister the driver from the OS */
        pci_unregister_driver(&img_pci_driver.pci_driver);
        return IMG_ERROR_DEVICE_NOT_FOUND;
    }

    /* Check if pci_PciProbeCb() was called...*/
    if (!gbDevDetected)
    {
        /* Not called, but maybe it was because SGX driver had already allocated it.
           Try manually. */
        size_t dev_i;
        for (dev_i = 0; pci_pci_ids[dev_i].vendor != 0; ++dev_i)
        {
            dev = pci_get_device(pci_pci_ids[dev_i].vendor,
                                 pci_pci_ids[dev_i].device, NULL);
            if(dev)
            {
                pci_PciManualProbe(dev);
                pci_dev_put(dev);
                break;
            }
        }
    }

    /* Check if pci device detected */
    if (!gbDevDetected)
    {
        /* Unregister the driver from the OS */
        pci_unregister_driver(&(img_pci_driver.pci_driver));
        printk(KERN_INFO "imgsysbrg: PCI card not found\n");
        return IMG_ERROR_DEVICE_NOT_FOUND;
    }

    dev = img_pci_driver.pci_dev;
    data = &imgpci_prvdata;
    data->dev = psInfo;

#if defined(ANDROID_ION_BUFFERS) && !defined(ANDROID_ION_SGX)
    /* Create system heap for ION allocation */
    {
        struct ion_device *psIonDev;
        psIonDev = ion_device_create(NULL);
        if (psIonDev)
        {
            struct ion_heap *psSysHeap;
            struct ion_platform_heap sIonSysHeapConf =
            {
                .type = ION_HEAP_TYPE_SYSTEM,
                .id   = ION_HEAP_TYPE_SYSTEM,
                .name = "System",
            };
            psSysHeap = ion_heap_create(&sIonSysHeapConf);
            if (psSysHeap)
            {
                ion_device_add_heap(psIonDev, psSysHeap);
                printk(KERN_INFO "System heap for ion created");
            }
            else
            {
                printk(KERN_ERR "Can't add system heap to ion device");
            }
        }
        else
        {
            printk(KERN_ERR "Can't create ion device");
        }
    }
#endif

    /* Save register pointer etc....*/
    SYSDEVU_SetDevMap(psInfo, NULL, NULL, 0, NULL, NULL, 0, 0);

    SYSDEVU_SetDevFuncs(psInfo, free_device, handle_resume, handle_suspend);
    ui32Result = SYSMEMKM_AddSystemMemory(&psInfo->sMemPool);
    IMG_ASSERT(IMG_SUCCESS == ui32Result);
    if (IMG_SUCCESS != ui32Result)
    {
        /* Unregister the driver from the OS */
        pci_unregister_driver(&img_pci_driver.pci_driver);
        return ui32Result;
    }
    psInfo->pPrivate = data;

    pci_EnableInterrupts(dev);
	// release pci reference
	pci_dev_put(dev);

    /* Card found...*/
    printk(KERN_INFO "imgsysbrg: PCI card found\n");

    return ui32Result;
}


/*!
******************************************************************************

@Function    SYSDEVU_VDECUnRegisterDriver

******************************************************************************/
IMG_RESULT SYSDEVU_VDECUnRegisterDriver(
    SYSDEVU_sInfo *  psInfo
)
{
    SYSMEMU_RemoveMemoryHeap(psInfo->sMemPool);
    psInfo->pfnFreeDevice(psInfo);
    return IMG_SUCCESS;
}

