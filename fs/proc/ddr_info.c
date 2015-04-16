#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/printk.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>

#include <linux/hw_log.h>
#define HWLOG_TAG	ddr_info
HWLOG_REGIST();
#define DDR_INFO_ADDR (0xFFF0A330)
//DDR_INFO_ADDR is the physical address of the register for storing DDR manufacturer id and memory size.the register vlaue is conveyed from xloader to Linux kernel,defined in "xloader/ddr/ddr_asic.h",and named  SYSCTRL_DDRINFO.

/****************************************************************************
FUNCTION:
    get_ddr_nfo
DESCRIPTION:
    getting the DDR manufacturer id and memory size from xloader.
RETURN VALUE:
    0 -- error.
    nonzero value -- the register value,DDR manufacturer id and memory size.
****************************************************************************/

static unsigned int get_ddr_info(void)
{
    unsigned int tmp_reg_value;
    unsigned int* virtual_addr =(unsigned int *)ioremap_nocache(DDR_INFO_ADDR&0xFFFFF000,0x800);
    if(0 == virtual_addr)
    {
        hwlog_err("%s  ioremap ERROR !!\n",__func__);
        return 0;
    }
    else
    {
        hwlog_info("%s  virtual_addr = 0x%x\n",__func__,(unsigned int)virtual_addr);
    }
    tmp_reg_value = *(unsigned int *)((unsigned int)virtual_addr + (DDR_INFO_ADDR&0x00000FFF));
    iounmap(virtual_addr);
    return tmp_reg_value;
}



static int ddr_info_show(struct seq_file *m, void *v)
{
    int len = 0;
    len = seq_printf(m,"ddr_info:\n0x%x\n",(unsigned int)get_ddr_info());
    return 0;
}


static int ddrinfo_open(struct inode *inode, struct file *file)
{
	return single_open(file, ddr_info_show, NULL);
}

static const struct file_operations proc_ddrinfo_operations = {
	.open		= ddrinfo_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_ddr_info_init(void)
{
    unsigned int ret;
    ret = (unsigned int)proc_create("ddr_info", 0, NULL, &proc_ddrinfo_operations);
    if(0 == ret)
    {
        hwlog_err("%s  //proc//ddr_info init ERROR !!\n",__func__);
    }
    return 0;
}

module_init(proc_ddr_info_init);
