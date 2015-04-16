#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/kobject.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/page.h>
#include <linux/slab.h>		// for kmalloc
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <asm/io.h>

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("jim");

unsigned long mmuroot = 0;
module_param(mmuroot, ulong, 0664);
MODULE_PARM_DESC(mmuroot, "mmuroot");
unsigned long mmureg = 0;
module_param(mmureg, ulong, 0664);
unsigned long memoffset = 0;
module_param(memoffset, ulong, 0664);

static char large_buffer[512*1024];
static int buffer_len = 0;


#define VALIDENTRY_MASK 1
#define DIR_SHIFT       22
#define PTE_SHIFT       12

static void * map_phys(unsigned long phys)
{
    if(memoffset)
        return ioremap(phys+memoffset, PAGE_SIZE);
    else
        return phys_to_virt(phys);
}

static void unmap_phys(void* v)
{
    if(memoffset) {
        iounmap(v);
    }
    else
    {
        // do nothing if unified memory
    }
}

static int pgtable_walk(unsigned long root, u32 * rootptr)
{
    int           i;
    char *        buf = large_buffer;

    // go through the valid directories,
    for(i = 0; i < PAGE_SIZE/sizeof(u32); i++) {
        // map a dir
        if(rootptr[i] & VALIDENTRY_MASK) {
            int           j;
            u32 *         dirptr;
            unsigned long dir;
            pr_info("dir %08lx %08x\n", root+sizeof(u32)*i, rootptr[i]);
            dir = rootptr[i];
            dirptr = map_phys(dir & PAGE_MASK);

            // map a set of page table entries
            for(j = 0; j < PAGE_SIZE/sizeof(u32); j++) {

                if(dirptr[j] & VALIDENTRY_MASK) {
		    unsigned long devvirt = i << DIR_SHIFT;
		    unsigned int  flags = dirptr[j] & ~PAGE_MASK;
		    devvirt += j << PTE_SHIFT;
		    buf += sprintf(buf, "PTE: v:%08lx p:%08lx flags:%04x\n", devvirt, dirptr[j]&PAGE_MASK, flags);
                }
            }
            unmap_phys(dirptr);
        }
    }
    return  buf-large_buffer;
}


static int jim_open(struct inode *inode, struct file *filep)
{
	int minor = iminor(inode);
	struct miscdevice * jimmisc = filep->private_data;	// recent kernels only!!
	dev_info(jimmisc->this_device, "jim_open: minor:%d misc:%p\n", minor, jimmisc);


    if(mmureg) {
        volatile u32 * mmuregptr = ioremap(mmureg & PAGE_MASK, PAGE_SIZE);
        volatile u32 * ptr = mmuregptr;
        ptr += (mmureg & ~PAGE_MASK)/sizeof(u32);
        mmuroot = *ptr;
        pr_info("%s mmureg:%08lx mmuroot:%08lx\n", __func__, mmureg, mmuroot);
        iounmap(mmuregptr);
    }

    if(mmuroot) {
        void * mmurootptr = map_phys(mmuroot);
        buffer_len = pgtable_walk(mmuroot, mmurootptr);
        unmap_phys(mmurootptr);
        pr_info("len:%d\n", buffer_len);
    }
    else {
        pr_err("invalid page table root: %lx\n", mmuroot);
        return -EINVAL;
    }
    return 0;
}

static loff_t jim_llseek(struct file *filp, loff_t off, int whence)
{
  loff_t newpos;

  switch(whence) {
   case 0: /* SEEK_SET */
    newpos = off;
    break;

   case 1: /* SEEK_CUR */
    newpos = filp->f_pos + off;
    break;

   case 2: /* SEEK_END */
    newpos = buffer_len + off;
    break;

   default: /* can't happen */
    return -EINVAL;
  }
  if (newpos<0) return -EINVAL;
  filp->f_pos = newpos;
  return newpos;
}

static int jim_release(struct inode *inode, struct file *filep)
{
	printk("jim_release\n");

	return 0;
}
static ssize_t jim_read(struct file *filep, char __user *buf, size_t count, loff_t *ppos)
{
	int ret = 0;
	printk("jim_read count:%zd ppos:%llu\n", count, *ppos);
	if(*ppos > buffer_len)
		return 0;

        if(count > buffer_len - *ppos)
            count = buffer_len - *ppos;

	ret = copy_to_user(buf, large_buffer+*ppos, count);
	if(ret)
		return ret;

	*ppos += count;
	return count;
}
static const struct file_operations jim_fops = {
	.owner   = THIS_MODULE,
	.open    = jim_open,
        .llseek  = jim_llseek,
	.release = jim_release,
	.read    = jim_read,
};

static struct miscdevice jimmisc = {
	.name = "jimptedump",
	.fops = &jim_fops,
	.minor = MISC_DYNAMIC_MINOR,
};

static int __init jim_init(void)
{
    int ret = 0;
    printk("jim_init\n");
    if(mmuroot==0 && mmureg==0) {
        pr_err("%s invalid mmuroot: cannot be zero\n", __func__);
        return -EINVAL;
    }

	ret = misc_register(&jimmisc);
	if(ret) {
		printk(KERN_ERR "device_register failed\n");
	}
	return ret;
}
static void __exit jim_exit(void)
{
	printk("jim exit\n");
	misc_deregister(&jimmisc);
}
module_init(jim_init);
module_exit(jim_exit);
