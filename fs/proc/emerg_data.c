/* < DTS2013092906363 hanpeng 20131008 begin */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <asm/uaccess.h>
#include "internal.h"

#define EMERGDATA_NAME "emerg_data"

struct proc_dir_entry *emerg_data = NULL;

extern unsigned int get_datamount_flag(void);
extern void set_datamount_flag(int value);

int emergdata_read_proc(struct file *file, char *buffer, size_t size, loff_t *data) {
    /*
     * return the value of datamount_flag(definition in setup.c)
     */
    return snprintf(buffer, size, "%d\n", get_datamount_flag());
}

ssize_t emergdata_write_proc(struct file *file, const char *buffer, size_t count, loff_t *data) {
    long value = -1;
    int strtol_ret = -1;
    int ret = -EINVAL;
    char *tmp_buf = NULL;

    if ((tmp_buf = kzalloc(count, GFP_KERNEL)) == NULL)
        return -ENOMEM;
    if (copy_from_user(tmp_buf, buffer, count - 1)) { //should ignore character '\n'
        kfree(tmp_buf);
        return -EFAULT;
    }
    *(tmp_buf + count) = '\0';

    strtol_ret = strict_strtol(tmp_buf, 10, &value);

    /*
     * call function set_datamount_flag conditions as follow
     * 1. strict_strtol return 0, AND,
     * 2. value equal 0
     */
    if (strtol_ret == 0) {
        if (value == 0) {
            set_datamount_flag(value);
            ret = count;
        }
    }
    kfree(tmp_buf);
    return ret;
}

static int __init emergdata_proc_init(void) {
    static const struct file_operations emergdata_proc_fops = {
        .owner = THIS_MODULE,
        .read = emergdata_read_proc,
        .write = emergdata_write_proc,
    };
    emerg_data = proc_create(EMERGDATA_NAME, 0660, NULL, &emergdata_proc_fops);
    if (emerg_data == NULL) {
        return -1;
    }
    emerg_data->data = NULL;
    return 0;
}

static void __exit emergdata_proc_exit(void) {
    remove_proc_entry(EMERGDATA_NAME, emerg_data);
}

module_init(emergdata_proc_init);
module_exit(emergdata_proc_exit);

/* DTS2013092906363 hanpeng 20131008 end > */
