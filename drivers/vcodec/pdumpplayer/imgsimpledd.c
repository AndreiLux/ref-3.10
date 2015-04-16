/*!
******************************************************************************
 @file   : imgsimpledd.c

 @brief

 @Author Imagination Technologies

 @date   31/10/2011

         <b>Copyright 2011 Imagination Technologies Limited.</b>\n

         All rights reserved.  No part of this software, either
         material or conceptual may be copied or distributed,
         transmitted, transcribed, stored in a retrieval system
         or translated into any human or computer language in any
         form by any means, electronic, mechanical, manual or
         other-wise, or disclosed to the third parties without the
         express written permission of Imagination Technologies
         Limited, Home Park Estate, Kings Langley, Hertfordshire,
         WD4 8LZ, U.K.

 <b>Description:</b>\n
         Linux PDUMP device driver.

 <b>Platform:</b>\n
	     Linux

 @Version
	     1.0

******************************************************************************/
/*
******************************************************************************
*****************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/kobject.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <linux/version.h>
#include <linux/slab.h>

//#if defined(METAC_2_1)
//#include <asm/cacheflush.h>
//#include <asm/imgpcidd.h>
//#include <asm/soc-rocket/rocket_defs.h>
//#else
#include "imgpcidd.h"
//#endif // defined (METAC_2_1)

#if defined(__arm__)
#include <asm/cacheflush.h>
#include <linux/dma-mapping.h>
#endif // defined(__arm__)

MODULE_LICENSE("GPL v2");

#ifndef IMG_TRUE
#define IMG_TRUE  1
#define IMG_FALSE 0
#endif

#define IMG_DEVICE_MINOR 0	// only a single device is currently supported

// example values that could be assigned to module_reg_base and module_reg_size
#define SYS_OMAP4430_SGX_REGS_SYS_PHYS_BASE  0x56000000
#define SYS_OMAP4430_SGX_REGS_SIZE           0x10000

static int module_irq = -1;
module_param(module_irq, int, S_IRUGO);
MODULE_PARM_DESC(module_irq, "irq number");

static unsigned int module_reg_base = 0;
static unsigned int module_reg_size = 0;
module_param(module_reg_base, uint, S_IRUGO);
module_param(module_reg_size, uint, S_IRUGO);
MODULE_PARM_DESC(module_reg_base, "base address of hardware registers");
MODULE_PARM_DESC(module_reg_size, "size of hardware registers area");

struct img_mem
{
	struct kobject          kobj;
	unsigned long           addr;
	unsigned long           size;
	void __iomem            *internal_addr;
};

struct img_info
{
	struct img_device 		*img_dev;
	char					*name;
	char                    *version;
	struct img_mem	        mem;
	long                    irq;
	unsigned long           irq_flags;
	unsigned long			irq_enabled;
	void                    *priv;

	spinlock_t				ioctl_lock;
	spinlock_t				read_lock;

	irqreturn_t (*handler)(int irq, struct img_info *info);
	int			(*ioctl)(struct img_info *info,	unsigned int cmd, unsigned long arg);

	unsigned long			int_disabled;
	unsigned long			int_status;
	unsigned long			int_mask;
};


struct img_device
{
	struct module			*owner;
	struct device			*dev;
	int						minor;
	atomic_t				event;
	struct fasync_struct	*async_queue;
	wait_queue_head_t		wait;
	int						vma_count;
	struct img_info			*info;
	struct kset 			map_attr_kset;
	int                     using_dev_memory;	// false if using kernel pages. true if using kernel memory
};

static struct img_device img_device;			// only a single device is currently supported
static int img_major;
static struct file_operations img_fops;

/* used by vma fault handler. set by get_virt2phys, for allocating new pages
 * GFP_DMA is in the 1st 16M. GFP_KERNEL is in the first 900M.
 * GFP_DMA32 is in the first 4G. GFP_HIGHUSER is anywhere. */
static int alloc_page_flags = GFP_DMA32;

/* IMG class infrastructure */
static struct img_class
{
	struct kref kref;
	struct class *class;
} *img_class;

static int img_read32(struct img_info *info, unsigned long offset, unsigned long * value);
static int img_write32(struct img_info *info, unsigned long offset, unsigned long value);

#if defined(METAC_2_1)
static void meta_flush_L2(unsigned long ul32StartAddress, unsigned long ul32SizeInBytes);
#endif

#ifdef IMG_EXTRA_DEBUG
unsigned long g_int_count_total		= 0;
unsigned long g_int_count_acked		= 0;
unsigned long g_read_count =0, g_rw_error_count = 0, g_open_count = 0;
#endif

/*
 * attributes
 */

static struct attribute attr_addr =
{
	.name  = "addr",
	.mode  = S_IRUGO,
};

static struct attribute attr_size =
{
	.name  = "size",
	.mode  = S_IRUGO,
};

#ifdef IMGPCI_EXTRA_DEBUG
static struct attribute attr_intaddr =
{
	.name  = "intaddr",
	.mode  = S_IRUGO,
};
#endif

static struct attribute* map_attrs[] =
{
  #ifdef IMG_EXTRA_DEBUG
	&attr_addr, &attr_size, &attr_intaddr, NULL
  #else
	&attr_addr, &attr_size, NULL
  #endif
};


/*!
******************************************************************************

 @Function              map_attr_show

******************************************************************************/
static ssize_t map_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct img_mem *mem = container_of(kobj, struct img_mem, kobj);

	if (strncmp(attr->name,"addr",4) == 0)
	{
		return sprintf(buf, "0x%lx\n", mem->addr);
	}

	if (strncmp(attr->name,"size",4) == 0)
	{
		return sprintf(buf, "0x%lx\n", mem->size);
	}

  #ifdef IMG_EXTRA_DEBUG
	if (strncmp(attr->name,"intaddr",4) == 0)
	{
		return sprintf(buf, "0x%lx\n", mem->internal_addr);
	}
  #endif

	return -ENODEV;
}


/*!
******************************************************************************

 @Function              map_attr_release

******************************************************************************/
static void map_attr_release(struct kobject *kobj)
{
	/* needs doing */
}

static struct sysfs_ops map_attr_ops =
{
	.show  = map_attr_show,
};

static struct kobj_type map_attr_type =
{
	.release	= map_attr_release,
	.sysfs_ops	= &map_attr_ops,
	.default_attrs	= map_attrs,
};

/*!
******************************************************************************

 @Function              show_name

******************************************************************************/
static ssize_t show_name(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct img_device *idev = dev_get_drvdata(dev);
	if (idev)
	{
		return sprintf(buf, "%s\n", idev->info->name);
	}
	else
	{
		return -ENODEV;
	}
}
static DEVICE_ATTR(name, S_IRUGO, show_name, NULL);


/*!
******************************************************************************

 @Function              show_version

******************************************************************************/
static ssize_t show_version(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct img_device *idev = dev_get_drvdata(dev);
	if (idev)
	{
		return sprintf(buf, "%s\n", idev->info->version);
	}
	else
	{
		return -ENODEV;
	}
}
static DEVICE_ATTR(version, S_IRUGO, show_version, NULL);

/*!
******************************************************************************

 @Function              show_event

******************************************************************************/
static ssize_t show_event(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct img_device *idev = dev_get_drvdata(dev);
	if (idev)
	{
		return sprintf(buf, "%u\n", (unsigned int)atomic_read(&idev->event));
	}
	else
	{
		return -ENODEV;
	}
}
static DEVICE_ATTR(event, S_IRUGO, show_event, NULL);


static struct attribute *img_attrs[] =
{
	&dev_attr_name.attr,
	&dev_attr_version.attr,
	&dev_attr_event.attr,
	NULL,
};

static struct attribute_group img_attr_grp =
{
	.attrs = img_attrs,
};

/*!
******************************************************************************

 @Function              img_dev_add_attributes

******************************************************************************/
static int img_dev_add_attributes(struct img_device *idev)
{
	int ret;
	int map_found = 0;
	struct img_mem *mem;

	ret = sysfs_create_group(&idev->dev->kobj, &img_attr_grp);
	if (ret)
	{
		goto err_group;
	}

    mem = &idev->info->mem;

    if(!map_found)
    {
        map_found = 1;

        kobject_set_name(&idev->map_attr_kset.kobj,"maps");

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
        idev->map_attr_kset.ktype = &map_attr_type;
#else
        idev->map_attr_kset.kobj.ktype = &map_attr_type;
#endif
        idev->map_attr_kset.kobj.parent = &idev->dev->kobj;
        ret = kset_register(&idev->map_attr_kset);
        if (ret)
        {
            goto err_remove_group;
        }
    }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
    kobject_init(&mem->kobj);
    kobject_set_name(&mem->kobj,"map%d",0);
    mem->kobj.parent = &idev->map_attr_kset.kobj;
#else
    kobject_init(&mem->kobj, &map_attr_type);
#endif
    mem->kobj.kset = &idev->map_attr_kset;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
    ret = kobject_add(&mem->kobj);
#else
    ret = kobject_add(&mem->kobj, &idev->map_attr_kset.kobj, "map%d",0);
#endif
    if (ret)
    {
        goto err_remove_maps;
    }

	return 0;

err_remove_maps:
    mem = &idev->info->mem;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
    kobject_unregister(&mem->kobj);
#else
    kobject_put(&mem->kobj);
#endif

	kset_unregister(&idev->map_attr_kset); /* Needed ? */
err_remove_group:
	sysfs_remove_group(&idev->dev->kobj, &img_attr_grp);
err_group:
	dev_err(idev->dev, "error creating sysfs files (%d)\n", ret);
	return ret;
}


/*!
******************************************************************************

 @Function              img_dev_del_attributes

******************************************************************************/
static void img_dev_del_attributes(struct img_device *idev)
{
    struct img_info *info = idev->info;
	struct img_mem *mem = &info->mem;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
    kobject_unregister(&mem->kobj);
#else
    kobject_put(&mem->kobj);
#endif

	kset_unregister(&idev->map_attr_kset);
	sysfs_remove_group(&idev->dev->kobj, &img_attr_grp);
}



/*!
******************************************************************************

 @Function              img_event_notify

******************************************************************************/
void img_event_notify(struct img_info *info)
{
	struct img_device *idev = info->img_dev;

	atomic_inc(&idev->event);
	wake_up_interruptible(&idev->wait);
	kill_fasync(&idev->async_queue, SIGIO, POLL_IN);
}

/*!
******************************************************************************

 @Function              img_interrupt

******************************************************************************/
static irqreturn_t img_interrupt(int irq, void *dev_id)
{
	struct img_device *idev = (struct img_device *)dev_id;
	irqreturn_t ret = IRQ_NONE;

    if(idev->info->handler)
    {
        idev->info->handler(irq, idev->info);
        if (ret == IRQ_HANDLED)
        {
            img_event_notify(idev->info);
        }
    }

	return ret;
}


struct img_listener
{
	struct img_device *dev;
	s32 event_count;
};

/*!
******************************************************************************

 @Function              img_open

******************************************************************************/
static int img_open(struct inode *inode, struct file *filep)
{
	struct img_device *		idev;
	struct img_listener *	listener;
	struct img_info *		info;

    if(iminor(inode) != IMG_DEVICE_MINOR)
        return -EINVAL;

	idev = &img_device;		// only a single device is currently supported
	info = idev->info;

	listener = kmalloc(sizeof(*listener), GFP_KERNEL);
	if (!listener)
	{
		return -ENOMEM;
	}

	listener->dev = idev;
	listener->event_count = atomic_read(&idev->event);
	filep->private_data = listener;

	/* Return the ISR data to the ground state */
	spin_lock_irq(&info->read_lock);
	info->int_mask = 0;
	info->int_status = 0;
	info->int_disabled = 1;
	spin_unlock_irq(&info->read_lock);

  #ifdef IMGPCI_EXTRA_DEBUG
	g_open_count ++;
  #endif
	return 0;
}


/*!
******************************************************************************

 @Function              img_fasync

******************************************************************************/
static int img_fasync(int fd, struct file *filep, int on)
{
	struct img_listener *listener = filep->private_data;
	struct img_device *idev = listener->dev;

	return fasync_helper(fd, filep, on, &idev->async_queue);
}


/*!
******************************************************************************

 @Function              img_release

******************************************************************************/
static int img_release(struct inode *inode, struct file *filep)
{
	int							ret = 0;
	struct img_listener *	listener = filep->private_data;
	struct img_device *		idev = listener->dev;
	struct img_info *		info = idev->info;

	if (filep->f_flags & FASYNC)
	{
		ret = img_fasync(-1, filep, 0);
	}

	/* Return the ISR data to the ground state */
	spin_lock_irq(&info->read_lock);
	info->int_mask = 0;
	info->int_status = 0;
	info->int_disabled = 1;
	spin_unlock_irq(&info->read_lock);

	kfree(listener);
	return ret;
}


/*!
******************************************************************************

 @Function              img_poll

******************************************************************************/
static unsigned int img_poll(struct file *filep, poll_table *wait)
{
	struct img_listener *listener = filep->private_data;
	struct img_device *idev = listener->dev;

	poll_wait(filep, &idev->wait, wait);
	if (listener->event_count != atomic_read(&idev->event))
	{
		return POLLIN | POLLRDNORM;
	}
	return 0;
}


/*!
******************************************************************************

 @Function              img_read

******************************************************************************/
static ssize_t img_read(struct file *filep, char __user *buf, size_t count, loff_t *ppos)
{
	struct img_listener *	listener = filep->private_data;
	struct img_device *		idev = listener->dev;
	struct img_info *		info = idev->info;
	ssize_t					retval;
	struct imgpci_readdata *user_readdata = (struct imgpci_readdata *) buf;
	struct imgpci_readdata	local_readdata;
	DECLARE_WAITQUEUE(wait, current);


	if (count != sizeof(struct imgpci_readdata))
	{
		return -EINVAL;
	}

	/* Clear, then enable interrupts */
	spin_lock_irq(&info->read_lock);
//	info->int_disabled = 0;
	spin_unlock_irq(&info->read_lock);

	add_wait_queue(&idev->wait, &wait);

	do
	{
		set_current_state(TASK_INTERRUPTIBLE);

		local_readdata.event_count = atomic_read(&idev->event);

		if (local_readdata.event_count != listener->event_count)
		{
			// do we need a protected read around this variable?
			local_readdata.int_status = info->int_status;

			if (copy_to_user(user_readdata, &local_readdata, sizeof(struct imgpci_readdata)))
			{
				retval = -EFAULT;
			}
			else
			{
				listener->event_count = local_readdata.event_count;
				retval = count;
			}
			break;
		}

		if (filep->f_flags & O_NONBLOCK)
		{
			retval = -EAGAIN;
			break;
		}

		if (signal_pending(current))
		{
			retval = -ERESTARTSYS;
			break;
		}
		/*	We have set the task as TASK_INTERRUPTIBLE so the schedule call will put us to sleep
			until the next time there is an interrupt (http://www.faqs.org/docs/kernel_2_4/lki-2.html)
		*/
		schedule();
	} while (1);

	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&idev->wait, &wait);

	return retval;
}


/*!
******************************************************************************

 @Function              img_find_mem_index

******************************************************************************/
static int img_find_mem_index(struct vm_area_struct *vma)
{
    if (vma->vm_pgoff == 0)
    {
        return 0;
	}
	return -1;
}


/*!
******************************************************************************

 @Function              img_mmap_physical

******************************************************************************/
static int img_mmap_physical(struct vm_area_struct *vma)
{
	struct img_device *idev = vma->vm_private_data;
	int mi = img_find_mem_index(vma);
	if (mi < 0)
	{
		return -EIO;
	}

	vma->vm_flags |= VM_IO;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	//printk("IMG mmap physical %lx(%x) -> %lx\n", idev->info->mem.addr, vma->vm_end-vma->vm_start, vma->vm_start);
	return remap_pfn_range(vma,
			       vma->vm_start,
			       idev->info->mem.addr >> PAGE_SHIFT,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot);
}

/*!
******************************************************************************

 @Function              imgdd_flushcache

 On ARM9Cortex and above cahces need to be flushed after each allocation

******************************************************************************/
static void imgdd_flushcache(struct page* pg)
{
#if defined(__arm__)
	long unsigned int phys = page_to_phys(pg);
	void* virt = phys_to_virt(phys);

	/* To flush the the inner cache: must be called 1st: Note addresses here are (user) virtual */
	#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34))
		dmac_inv_range(virt, virt + PAGE_SIZE);
	#else
		dmac_map_area(virt, PAGE_SIZE, DMA_FROM_DEVICE);
	#endif

/* To flush the outer cache. Call 2nd */
outer_inv_range(phys, phys + PAGE_SIZE);

#elif defined (METAC_2_1)
	meta_flush_L2(page_to_phys(pg), 4096);
#endif // defined(METAC_2_1)
}

/*!
******************************************************************************

 @Function              img_fault

 Attempt to access a non existant page. Populate it from kernel memory.
 This is useful for systems without PCI memory (ie with unified memory).
 Use IOCTL_GET_VIRT2PHYS to lock the page in memory and get its physical address.

******************************************************************************/
static int img_fault(struct vm_area_struct * vma, struct vm_fault * vmf)
{
	int ret = 0;
	struct page * pg = NULL;
	struct img_device *idev = vma->vm_private_data;

	if(idev->using_dev_memory)
	{
		/* attempt to access a bad address */
		return VM_FAULT_SIGBUS;
	}

	pg = alloc_page(alloc_page_flags | __GFP_ZERO);

	if(pg)
	{
		vmf->page = pg;
		imgdd_flushcache(pg);
	}
	else
	{
		ret = VM_FAULT_SIGBUS;
	}
	//printk("IMG alloc_page:%lx\n", page_to_pfn(pg));
	return ret;
}


static struct vm_operations_struct img_vmops = {
	.fault = img_fault,
};

/*!
******************************************************************************

 @Function              img_mmap

 Either map a region (0,1,2,3),
 or, for devices with no physical memory, map to area, and allocate kernel pages to it.
 In this case, the size is not very important, as memory is only allocated when the page faults

******************************************************************************/
static int img_mmap(struct file *filep, struct vm_area_struct *vma)
{
	struct img_listener *listener = filep->private_data;
	struct img_device *idev = listener->dev;
	int mi;
	unsigned long requested_pages, actual_pages;

	vma->vm_private_data = idev;

	if(vma->vm_pgoff == IMGPCI_VIRTUAL_MAP)
	{
		/* allocate from kernel memory. Do no allocate at this time, but wait until the memory
		   is accessed, before allocating it. Then use fault() to populate the page */
		idev->using_dev_memory = IMG_FALSE;
		vma->vm_ops = &img_vmops;
		vma->vm_pgoff = 0;
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		//printk("IMG mmap virtual %lx(%lx) -> %lx\n", idev->info->mem.addr, vma->vm_end-vma->vm_start, vma->vm_start);
		return 0;
	}
	else
	{
		idev->using_dev_memory = IMG_TRUE;
		mi = img_find_mem_index(vma);

		requested_pages = (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;
		actual_pages = (idev->info->mem.size + PAGE_SIZE -1) >> PAGE_SHIFT;
		if (requested_pages > actual_pages)
		{
			return -EINVAL;
		}
	}

	return img_mmap_physical(vma);
}



/*!
******************************************************************************

 @Function              img_ioctl

******************************************************************************/
static long img_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	struct img_listener *listener = filep->private_data;
	struct img_device *idev = listener->dev;

	if (idev->info->ioctl)
	{
		return (idev->info->ioctl(idev->info, cmd, arg));
	}
	else
	{
		return -ENOTTY;
	}
}

static struct file_operations img_fops =
{
	.owner		= THIS_MODULE,
	.open		= img_open,
	.release	= img_release,
	.read		= img_read,
	.mmap		= img_mmap,
	.poll		= img_poll,
	.fasync		= img_fasync,
	.unlocked_ioctl	= img_ioctl,
};

/*!
******************************************************************************

 @Function              img_major_init

******************************************************************************/
static int img_major_init(void)
{
	img_major = register_chrdev(0, "img", &img_fops);
	if (img_major < 0)
	{
		return img_major;
	}
	return 0;
}


/*!
******************************************************************************

 @Function              img_major_cleanup

******************************************************************************/
static void img_major_cleanup(void)
{
	unregister_chrdev(img_major, "img");
}


/*!
******************************************************************************

 @Function              init_img_class

******************************************************************************/
static int init_img_class(void)
{
	int ret = 0;

	if (img_class != NULL)
	{
		kref_get(&img_class->kref);
		goto exit;
	}

	/* This is the first time in here, set everything up properly */
	ret = img_major_init();
	if (ret)
	{
		goto exit;
	}

	img_class = kzalloc(sizeof(*img_class), GFP_KERNEL);
	if (!img_class)
	{
		ret = -ENOMEM;
		goto err_kzalloc;
	}

	kref_init(&img_class->kref);
	img_class->class = class_create(THIS_MODULE, "img");
	if (IS_ERR(img_class->class))
	{
		ret = IS_ERR(img_class->class);
		printk(KERN_ERR "class_create failed for img\n");
		goto err_class_create;
	}
	return 0;

err_class_create:
	kfree(img_class);
	img_class = NULL;
err_kzalloc:
	img_major_cleanup();
exit:
	return ret;
}


/*!
******************************************************************************

 @Function              release_img_class

******************************************************************************/
static void release_img_class(struct kref *kref)
{
	/* Ok, we cheat as we know we only have one img_class */
	class_destroy(img_class->class);
	kfree(img_class);
	img_major_cleanup();
	img_class = NULL;
}


/*!
******************************************************************************

 @Function              img_class_destroy

******************************************************************************/
static void img_class_destroy(void)
{
	if (img_class)
	{
		kref_put(&img_class->kref, release_img_class);
	}
}


/*!
******************************************************************************

 @Function              img_request_irq

******************************************************************************/
int img_request_irq (struct img_info *info)
{
	struct img_device *idev = info->img_dev;
	int ret = 0;

	if (idev->info->irq_enabled)
	{
		return -1;
	}

	ret = request_irq(idev->info->irq, img_interrupt, idev->info->irq_flags, idev->info->name, idev);

	if (ret)
	{
		return -1;
	}

	idev->info->irq_enabled = 1;

	return 0;
}


/*!
******************************************************************************

 @Function              img_free_irq

******************************************************************************/
int img_free_irq (struct img_info *info)
{
	struct img_device *idev = info->img_dev;

	if (!idev->info->irq_enabled)
	{
		return -1;
	}

	free_irq(info->irq, idev);
	idev->info->irq_enabled = 0;

	return 0;
}



/*!
******************************************************************************

 @Function              img_register_device

******************************************************************************/
int img_register_device(struct device *parent, struct img_info *info)
{
	struct img_device *idev;
	int ret = 0;

	if (!info || !info->name || !info->version)
	{
		return -EINVAL;
	}

	info->img_dev = NULL;

	ret = init_img_class();
	if (ret)
	{
		return ret;
	}

	idev = &img_device;

	idev->owner = THIS_MODULE;
	idev->info = info;
	init_waitqueue_head(&idev->wait);
	atomic_set(&idev->event, 0);

	idev->minor = IMG_DEVICE_MINOR;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
	idev->dev = device_create(img_class->class, parent, MKDEV(img_major, idev->minor), "img%u", idev->minor);
#elif ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)))
	idev->dev = device_create_drvdata(img_class->class, parent, MKDEV(img_major, idev->minor), NULL, "img%u", idev->minor);
#else
	idev->dev = device_create(img_class->class, parent, MKDEV(img_major, idev->minor), NULL, "img%u", idev->minor);
#endif
	if (IS_ERR(idev->dev))
	{
		printk(KERN_ERR "IMG: device register failed\n");
		ret = PTR_ERR(idev->dev);
		goto err_device_create;
	}
	dev_set_drvdata(idev->dev, idev);

	ret = img_dev_add_attributes(idev);
	if (ret)
	{
		goto err_img_dev_add_attributes;
	}

	info->img_dev = idev;

    if(info->irq > 0)
    {
        /* request irq */
        ret = img_request_irq (info);
        if(ret)
        {
            goto err_img_dev_add_attributes;
        }
    }
    else
    {
        printk(KERN_DEBUG "imgsimpledd: IMG interrupts disabled\n");
    }
	return 0;

	img_dev_del_attributes(idev);
err_img_dev_add_attributes:
	device_destroy(img_class->class, MKDEV(img_major, idev->minor));
err_device_create:
	img_class_destroy();
	return ret;
}

/*!
******************************************************************************

 @Function              img_unregister_device

******************************************************************************/
void img_unregister_device(struct img_info *info)
{
	struct img_device *idev;

	if (!info || !info->img_dev)
	{
		return;
	}

	idev = info->img_dev;

	if ((info->irq >= 0) && info->irq_enabled)
	{
		free_irq(info->irq, idev);
	}

	img_dev_del_attributes(idev);

	dev_set_drvdata(idev->dev, NULL);
	device_destroy(img_class->class, MKDEV(img_major, idev->minor));
	img_class_destroy();

	return;
}



/************************************************************************************************
							Lower level driver functions
*************************************************************************************************/



/*!
******************************************************************************

 @Function              img_write32

******************************************************************************/
static int img_write32(struct img_info *info, unsigned long offset, unsigned long value)
{
	void __iomem *reg;

	reg = info->mem.internal_addr + offset;
	iowrite32(value, reg);

	return 0;
}



/*!
******************************************************************************

 @Function              img_read32

******************************************************************************/
static int img_read32(struct img_info *info, unsigned long offset, unsigned long * value)
{
	void __iomem *reg;

	reg = info->mem.internal_addr + offset;
	*value = ioread32(reg);

	return 0;
}

/*!
******************************************************************************

 @Function              img_handler

******************************************************************************/
static irqreturn_t img_handler(int irq, struct img_info *info)
{
	return IRQ_NONE;
}

/*!
******************************************************************************

 @Function              img_perform_ioctl

******************************************************************************/
static int img_perform_ioctl(struct img_info *info,	unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	if ((_IOC_TYPE(cmd) != IMGPCI_IOCTL_MAGIC) ||
		(_IOC_NR(cmd) > IMGPCI_IOCTL_MAXNR)
	)
	{
		return -ENOTTY;
	}

	switch (cmd)
	{
		/* Write32 */
		case IMGPCI_IOCTL_WRITE32:
		{
			struct imgpci_reg32	reg;

			if (copy_from_user(&reg, argp, sizeof(reg)))
			{
				return -EFAULT;
			}
            if(reg.address.bar != 0)
            {
                return -EINVAL;
            }

			spin_lock(&info->ioctl_lock);
			if (img_write32(info, reg.address.offset, reg.value))
			{
				spin_unlock(&info->ioctl_lock);
				return -EINVAL;
			}
			spin_unlock(&info->ioctl_lock);
			break;
		}
		/* Read32 */
    	case IMGPCI_IOCTL_READ32:
		{
			struct imgpci_reg32		reg;

			if (copy_from_user(&reg, argp, sizeof(reg)))
			{
				return -EFAULT;
			}

            if(reg.address.bar != 0)
            {
                return -EINVAL;
            }
			spin_lock(&info->ioctl_lock);
			if (img_read32(info, reg.address.offset, &reg.value))
			{
				spin_unlock(&info->ioctl_lock);
				return -EINVAL;
			}
			spin_unlock(&info->ioctl_lock);

			if (copy_to_user(argp, &reg, sizeof(reg)))
			{
				return -EFAULT;
			}
			break;
		}
		/* Interrupt enable/disable */
		case IMGPCI_IOCTL_INTEN:
		case IMGPCI_IOCTL_INTDIS:
		{
			/* The enable pattern is passed directly as the argument */
			break;
		}
		/* ISR parameter set command */
		case IMGPCI_IOCTL_SETISRPARAMS:
		{
			break;
		}

		/* get physical page, and put it into user logical address */
		case IMGPCI_IOCTL_GET_VIRT2PHYS:
		{
			struct imgpci_get_virt2phys virt2phys;
			int ret = 0;
			struct page * pages[1];

			spin_lock_irq(&info->ioctl_lock);
			if (copy_from_user(&virt2phys, argp, sizeof(virt2phys)))
			{
				spin_unlock_irq(&info->ioctl_lock);
				printk(KERN_ERR "copy from user failed\n");
				return -EFAULT;
			}
			spin_unlock_irq(&info->ioctl_lock);
			if(virt2phys.dma32)
			{
				alloc_page_flags = GFP_DMA32 | GFP_KERNEL;
			}
			else
			{
				alloc_page_flags = GFP_HIGHUSER;
			}
			/* get the physical page.
			   If the page does not exist, this will cause a page fault,
			   and img_fault will populate the page
			*/
			down_read(&current->mm->mmap_sem);
			ret = get_user_pages(current, current->mm, (unsigned long)virt2phys.virt & PAGE_MASK, 1, 1, 0, pages, NULL);
			up_read(&current->mm->mmap_sem);
			if(ret <= 0)
			{
				printk(KERN_ERR "virt2phys: failed to get user pages for %p: %d\n", virt2phys.virt, ret);
				virt2phys.phys = 0ULL;
			}
			else
			{
				virt2phys.phys = page_to_pfn(pages[0]) << PAGE_SHIFT;
				virt2phys.phys |= ((unsigned long)virt2phys.virt & (-1^PAGE_MASK));
				// no need to keep hold of the page, as it is owned by the
				// vma and is not swap-backed.
				put_page(pages[0]);
				if(copy_to_user(argp, &virt2phys, sizeof(virt2phys)))
					return -EFAULT;
			}

			break;
		}
#if defined(METAC_2_1)
		/* Flush the Meta L2 cache */
		case IMGPCI_IOCTL_META_L2_CACHE_FLUSH:
		{
			struct imgpci_cache_flush	sCacheFlushArea;

			spin_lock_irq(&info->ioctl_lock);
			if (copy_from_user(&sCacheFlushArea, argp, sizeof(sCacheFlushArea)))
			{
				spin_unlock_irq(&info->ioctl_lock);
				printk(KERN_ERR "copy from user failed\n");
				return -EFAULT;
			}
			spin_unlock_irq(&info->ioctl_lock);

			/* Flush the cache line, and issue a fence */
			meta_flush_L2(sCacheFlushArea.ui32PhysStartAddr, sCacheFlushArea.ui32SizeInBytes);

			break;
		}
#endif
		default:
		{
			return -ENOTTY;
		}
	}
	return 0;
}

/*!
******************************************************************************

 @Function              img_init_module

******************************************************************************/
static int __init img_init_module(void)
{
	/* Allocate a img_info structure */
	struct img_info * info = NULL;

	printk(KERN_INFO"imgsimpledd: Initialising Pdump player driver\n");

    if(module_reg_base == 0 || module_reg_size == 0)
    {
        printk(KERN_ERR "invalid value for module_reg_base or module_reg_size. "
               "Please provide values on the insmod command line\n"
               "EG: insmod imgsimpledd.ko module_reg_base=0x56000000 module_reg_size=0x10000\n");
        return -EINVAL;
    }
    info = kzalloc(sizeof(struct img_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

    img_device.info = info;

	/* Create a kernel space mapping for the registers */
	printk(KERN_INFO "imgsimpledd: Setting memory window to: 0x%08x, size 0x%08x", module_reg_base, module_reg_size);

    info->mem.addr = module_reg_base;
    info->mem.size = module_reg_size;
    info->mem.internal_addr = ioremap(module_reg_base, module_reg_size);

	info->name = "imgdev";
	info->version = "0.0.1";
	info->irq = module_irq;
	info->irq_flags = IRQF_DISABLED | IRQF_SHARED;
	info->handler = img_handler;
	info->int_mask = 0;
	info->int_status = 0;
	info->int_disabled = 1;

	info->ioctl = img_perform_ioctl;
	spin_lock_init(&info->ioctl_lock);
	spin_lock_init(&info->read_lock);

    return img_register_device(NULL, info);
}

/*!
******************************************************************************

 @Function              img_exit_module

******************************************************************************/
static void __exit img_exit_module(void)
{
    struct img_info *info = img_device.info;

    img_unregister_device(info);
    iounmap(info->mem.internal_addr);
    kfree(info);
}

module_init(img_init_module);
module_exit(img_exit_module);

