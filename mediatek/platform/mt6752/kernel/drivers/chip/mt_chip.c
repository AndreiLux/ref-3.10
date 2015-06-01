#define pr_fmt(fmt) "["KBUILD_MODNAME"] " fmt
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/kfifo.h>

#include <linux/firmware.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/printk.h>

#include <mach/mt_chip.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_typedefs.h>
#include <asm/setup.h>
//#include "devinfo.h"
extern u32 get_devinfo_with_index(u32 index);

void __iomem *APHW_CODE = NULL;
void __iomem *APHW_SUBCODE = NULL;
void __iomem *APHW_VER = NULL;
void __iomem *APSW_VER = NULL;

/* return hardware version */
unsigned int get_chip_code(void)
{
    return readl(IOMEM(APHW_CODE));
}

unsigned int get_chip_hw_ver_code(void)
{
    return readl(IOMEM(APHW_VER));
}

unsigned int get_chip_sw_ver_code(void)
{
    return readl(IOMEM(APSW_VER));
}

unsigned int get_chip_hw_subcode(void)
{
    return readl(IOMEM(APHW_SUBCODE));
}

unsigned int mt_get_chip_id(void)
{
    unsigned int chip_id = get_chip_code();
    /*convert id if necessary*/
    return chip_id;
}

CHIP_SW_VER mt_get_chip_sw_ver(void)
{
    return (CHIP_SW_VER)get_chip_sw_ver_code();
}

struct chip_inf_entry
{
    const char*     name;
    CHIP_INFO       id;
    int             (*to_str)(char* buf, size_t len, int val);
};

unsigned int mt_get_chip_info(CHIP_INFO id)
{
    
    if (CHIP_INFO_HW_CODE == id)
        return get_chip_code();
    else if (CHIP_INFO_HW_SUBCODE == id)
        return get_chip_hw_subcode();
    else if (CHIP_INFO_HW_VER == id)
        return get_chip_hw_ver_code();
    else if (CHIP_INFO_SW_VER == id)
        return get_chip_sw_ver_code();
    else if (CHIP_INFO_FUNCTION_CODE == id) 
    {       
        unsigned int val = get_devinfo_with_index(24) & 0xFF000000; //[31:24]
        return (val >> 24);
    }
    else if (CHIP_INFO_PROJECT_CODE == id)
    {
        unsigned int val = get_devinfo_with_index(24) & 0x00003FFF; //[13:0]        
        return (val);
    }
    else if (CHIP_INFO_DATE_CODE == id)
    {
        unsigned int val = get_devinfo_with_index(24) & 0x00FFC000; //[23:14]
        return (val >> 14);
    }    
    else if (CHIP_INFO_FAB_CODE == id)
    {
        unsigned int val = get_devinfo_with_index(25) & 0x70000000; //[30:28]    
        return (val >> 28);
    }
    
    return 0x0000ffff;
}

static int hex2str(char* buf, size_t len, int val)
{
    return snprintf(buf, len, "%04X", val);
}

#if 0
static int dec2str(char* buf, size_t len, int val)
{
    return snprintf(buf, len, "%04d", val);    
}

static int date2str(char* buf, size_t len, int val)
{
    unsigned int year = ((val & 0x3C0) >> 6) + 2012;
    unsigned int week = (val & 0x03F);
    return snprintf(buf, len, "%04d%02d", year, week);
}
#endif

static struct proc_dir_entry *chip_proc = NULL;
static struct chip_inf_entry chip_ent[] = 
{
    {"hw_code",     CHIP_INFO_HW_CODE,      hex2str},
    {"hw_subcode",  CHIP_INFO_HW_SUBCODE,   hex2str},
    {"hw_ver",      CHIP_INFO_HW_VER,       hex2str},
    {"sw_ver",      CHIP_INFO_SW_VER,       hex2str},
    //{"code_proj",   CHIP_INFO_PROJECT_CODE, dec2str},
    //{"code_func",   CHIP_INFO_FUNCTION_CODE,hex2str},
    //{"code_fab",    CHIP_INFO_FAB_CODE,     hex2str},
    //{"code_date",   CHIP_INFO_DATE_CODE,    date2str},
    {"info",        CHIP_INFO_ALL,          NULL}
};

static int chip_proc_show(struct seq_file* s, void* v)
{
    struct chip_inf_entry *ent = s->private;
    if ((ent->id > CHIP_INFO_NONE) && (ent->id < CHIP_INFO_MAX))
    {
        seq_printf(s, "%04x\n", mt_get_chip_info(ent->id));
    }
    else
    {
        int idx = 0;
        char buf[16];
        for (idx = 0; idx < sizeof(chip_ent)/sizeof(chip_ent[0]); idx++)
        {    
            struct chip_inf_entry *ent = &chip_ent[idx];
            unsigned int val = mt_get_chip_info(ent->id);
        
            if (!ent->to_str)    
                continue;
            if (0 < ent->to_str(buf, sizeof(buf), val))
                seq_printf(s, "%-16s : %6s (%04x)\n", ent->name, buf, val);
            else
                seq_printf(s, "%-16s : %6s (%04x)\n", ent->name, "NULL", val); 
        }
    }      
    return 0;
}

static int chip_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, chip_proc_show, PDE_DATA(file_inode(file)));
}

static const struct file_operations chip_proc_fops = { 
    .open           = chip_proc_open,
    .read           = seq_read,
    .llseek         = seq_lseek,
    .release        = single_release,
};

extern struct proc_dir_entry proc_root;
static void __init create_procfs(void)
{
    int idx;
    
    chip_proc = proc_mkdir_data("chip", S_IRUGO, &proc_root, NULL);
    if (NULL == chip_proc)
    {
        pr_err("create /proc/chip fails\n");
        return;
    }            
    
    for (idx = 0; idx < sizeof(chip_ent)/sizeof(chip_ent[0]); idx++)
    {
        struct chip_inf_entry *ent = &chip_ent[idx];
        if (NULL == proc_create_data(ent->name, S_IRUGO, chip_proc, &chip_proc_fops, ent))
        {
            pr_err("create /proc/chip/%s fail\n", ent->name);
            return;
        }
    }
}

int __init chip_mod_init(void)
{
    struct device_node *node = NULL;
    
#ifdef CONFIG_OF
    node = of_find_compatible_node(NULL, NULL, "mediatek,CHIPID");
    if (node) {
        APHW_CODE = of_iomap(node,0);
        WARN(!APHW_CODE, "unable to map APHW_CODE registers\n");    
        APHW_SUBCODE = of_iomap(node,1);
        WARN(!APHW_SUBCODE, "unable to map APHW_SUBCODE registers\n");            
        APHW_VER = of_iomap(node,2);
        WARN(!APHW_VER, "unable to map APHW_VER registers\n");                    
        APSW_VER = of_iomap(node,3);
        WARN(!APSW_VER, "unable to map APSW_VER registers\n");            
    }
#endif
    create_procfs();
    //pr_alert("%p %p %p %p\n", APHW_CODE, APHW_SUBCODE, APHW_VER, APSW_VER);
    pr_alert("CODE = 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x\n", 
             get_chip_code(), get_chip_hw_subcode(), get_chip_hw_ver_code(), get_chip_sw_ver_code(), 
             mt_get_chip_info(CHIP_INFO_FUNCTION_CODE), mt_get_chip_info(CHIP_INFO_PROJECT_CODE), 
             mt_get_chip_info(CHIP_INFO_DATE_CODE), mt_get_chip_info(CHIP_INFO_FAB_CODE));
    pr_alert("CODE = 0x%04x", mt_get_chip_sw_ver());
    
    return 0;
}


core_initcall(chip_mod_init);
MODULE_DESCRIPTION("MTK Chip Information");
MODULE_LICENSE("GPL");
EXPORT_SYMBOL(mt_get_chip_id);
EXPORT_SYMBOL(mt_get_chip_sw_ver);
EXPORT_SYMBOL(mt_get_chip_info);

