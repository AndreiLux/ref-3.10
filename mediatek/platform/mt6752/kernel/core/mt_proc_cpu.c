#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include "mach/sync_write.h"
#include "mach/mt_typedefs.h"
#include "mach/mt_spm.h"
#include "mach/mt_spm_mtcmos.h"

#ifdef CONFIG_PROC_FS

static char *_copy_from_user_for_proc(const char __user *buffer, size_t count)
{
	char *buf = (char *)__get_free_page(GFP_USER);

	if (!buf)
		return NULL;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	return buf;

out:
	free_page((unsigned long)buf);

	return NULL;
}

#define _read(addr)			__raw_readl(IOMEM(addr))
#define _write(addr, val) \
		do{ \
			mt_reg_sync_writel(val, addr); \
			if(1) printk(KERN_ERR "[%s]: [%x]=%x\n", __func__, (unsigned int)addr, (unsigned int)_read(addr)); \
		} while(0)

static int cpu_idle_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "CPU[1]=0x%x,", _read(SPM_CA7_CPU1_PWR_CON));
	seq_printf(m, "CPU[2]=0x%x,", _read(SPM_CA7_CPU2_PWR_CON));
	seq_printf(m, "CPU[3]=0x%x,", _read(SPM_CA7_CPU3_PWR_CON));
	seq_printf(m, "CPU[4]=0x%x,", _read(SPM_CA15_CPU0_PWR_CON));
	seq_printf(m, "CPU[5]=0x%x,", _read(SPM_CA15_CPU1_PWR_CON));
	seq_printf(m, "CPU[6]=0x%x,", _read(SPM_CA15_CPU2_PWR_CON));
	seq_printf(m, "CPU[7]=0x%x\n", _read(SPM_CA15_CPU3_PWR_CON));

	return 0;
}

static ssize_t cpu_idle_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
	unsigned int idx = 0;

	char *buf = _copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "CPU%d", &idx) == 1) {
		if ((idx > 0) && (idx < 8)) {

			if ( idx > 3 ) 
				spm_mtcmos_ctrl_cpusys1(STA_POWER_ON, 1);

			printk(KERN_ERR "[%s]: index=%x\n", __func__, idx);
			switch(idx) {
				case 1:
					_write(SPM_CA7_CPU1_PWR_CON, 0x36);
					udelay(1);
					_write(SPM_CA7_CPU1_PWR_CON, 0x3E);
					mdelay(1);
					_write(SPM_CA7_CPU1_PWR_CON, 0x3C);
					_write(SPM_CA7_CPU1_PWR_CON, 0x7C);
					_write(SPM_CA7_CPU1_PWR_CON, 0x5C);
				break;
				case 2:
					_write(SPM_CA7_CPU2_PWR_CON, 0x36);
					udelay(1);
					_write(SPM_CA7_CPU2_PWR_CON, 0x3E);
					mdelay(1);
					_write(SPM_CA7_CPU2_PWR_CON, 0x3C);
					_write(SPM_CA7_CPU2_PWR_CON, 0x7C);
					_write(SPM_CA7_CPU2_PWR_CON, 0x5C);
				break;
				case 3:
					_write(SPM_CA7_CPU3_PWR_CON, 0x36);
					udelay(1);
					_write(SPM_CA7_CPU3_PWR_CON, 0x3E);
					mdelay(1);
					_write(SPM_CA7_CPU3_PWR_CON, 0x3C);
					_write(SPM_CA7_CPU3_PWR_CON, 0x7C);
					_write(SPM_CA7_CPU3_PWR_CON, 0x5C);
				break;
				case 4:
					_write(SPM_CA15_CPU0_PWR_CON, 0x36);
					udelay(1);
					_write(SPM_CA15_CPU0_PWR_CON, 0x3E);
					mdelay(1);
					_write(SPM_CA15_CPU0_PWR_CON, 0x3C);
					_write(SPM_CA15_CPU0_PWR_CON, 0x7C);
					_write(SPM_CA15_CPU0_PWR_CON, 0x5C);
				break;
				case 5:
					_write(SPM_CA15_CPU1_PWR_CON, 0x36);
					udelay(1);
					_write(SPM_CA15_CPU1_PWR_CON, 0x3E);
					mdelay(1);
					_write(SPM_CA15_CPU1_PWR_CON, 0x3C);
					_write(SPM_CA15_CPU1_PWR_CON, 0x7C);
					_write(SPM_CA15_CPU1_PWR_CON, 0x5C);
				break;
				case 6:
					_write(SPM_CA15_CPU2_PWR_CON, 0x36);
					udelay(1);
					_write(SPM_CA15_CPU2_PWR_CON, 0x3E);
					mdelay(1);
					_write(SPM_CA15_CPU2_PWR_CON, 0x3C);
					_write(SPM_CA15_CPU2_PWR_CON, 0x7C);
					_write(SPM_CA15_CPU2_PWR_CON, 0x5C);
				break;
				case 7:
					_write(SPM_CA15_CPU3_PWR_CON, 0x36);
					udelay(1);
					_write(SPM_CA15_CPU3_PWR_CON, 0x3E);
					mdelay(1);
					_write(SPM_CA15_CPU3_PWR_CON, 0x3C);
					_write(SPM_CA15_CPU3_PWR_CON, 0x7C);
					_write(SPM_CA15_CPU3_PWR_CON, 0x5C);
				break;
				default:
					printk(KERN_ERR "[%s]: bad index!! should be CPU[1-7]\n", __func__);
				break;
			}
		}
	} else
		printk(KERN_ERR "[%s]: bad argument!! should be CPU[1-7]\n", __func__);

	return count;
}

#define PROC_FOPS_RW(name)							\
	static int name ## _proc_open(struct inode *inode, struct file *file)	\
	{									\
		return single_open(file, name ## _proc_show, NULL);		\
	}									\
	static const struct file_operations name ## _proc_fops = {		\
		.owner          = THIS_MODULE,					\
		.open           = name ## _proc_open,				\
		.read           = seq_read,					\
		.llseek         = seq_lseek,					\
		.release        = single_release,				\
		.write          = name ## _proc_write,				\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(cpu_idle);

static int _create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	const struct {
		const char *name;
		const struct file_operations *fops;
	} entries[] = {
		PROC_ENTRY(cpu_idle),
	};

	dir = proc_mkdir("cpu_idle", NULL);

	if (!dir) {
		printk(KERN_ERR "fail to create /proc/cpu_idle @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP, dir, entries[i].fops))
			printk(KERN_ERR "%s(), create /proc/cpu_idle/%s failed\n", __func__, entries[i].name);
	}

	return 0;
}
#endif /* CONFIG_PROC_FS */

/*********************************
* cpu speed stress initialization
**********************************/
static int __init mt_cpu_idle_init(void)
{
	return _create_procfs();
}
module_init(mt_cpu_idle_init);

MODULE_DESCRIPTION("MediaTek driver");
MODULE_LICENSE("GPL");
