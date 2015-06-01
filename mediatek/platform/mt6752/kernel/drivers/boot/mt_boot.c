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

#include <mach/mt_boot.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_typedefs.h>
#include <asm/setup.h>
//#include "devinfo.h"
extern u32 get_devinfo_with_index(u32 index);

/* this vairable will be set by mt_fixup.c */
META_COM_TYPE g_meta_com_type = META_UNKNOWN_COM;
unsigned int g_meta_com_id = 0;
unsigned int g_meta_uart_port = 0;

struct meta_driver {
    struct device_driver driver;
    const struct platform_device_id *id_table;
};

static struct meta_driver meta_com_type_info =
{
    .driver  = {
        .name = "meta_com_type_info",
        .bus = &platform_bus_type,
        .owner = THIS_MODULE,
    },
    .id_table = NULL,
};

static struct meta_driver meta_com_id_info =
{
    .driver = {
        .name = "meta_com_id_info",
        .bus = &platform_bus_type,
        .owner = THIS_MODULE,
    },
    .id_table = NULL,
};

static struct meta_driver meta_uart_port_info =
{
    .driver = {
        .name = "meta_uart_port_info",
        .bus = &platform_bus_type,
        .owner = THIS_MODULE,
    },
    .id_table = NULL,
};



#ifdef CONFIG_OF
struct boot_tag_meta_com {
	u32 size;
	u32 tag;
	u32 meta_com_type; /* identify meta via uart or usb */
	u32 meta_com_id;  /* multiple meta need to know com port id */
	u32 meta_uart_port; /* identify meta uart port number */
};
#endif

static ssize_t boot_show(struct kobject *kobj, struct attribute *a, char *buf)
{
    if (!strncmp(a->name, INFO_SYSFS_ATTR, strlen(INFO_SYSFS_ATTR)))
    {
        /*FIXME: wait for porting*/
        //return sprintf(buf, "%04X%04X%04X%04X %04X %04X\n", get_chip_code(), get_chip_hw_subcode(),
          //                  get_chip_hw_ver_code(), get_chip_sw_ver_code(),
            //                mt_get_chip_sw_ver(), mt_get_chip_id());
        return sprintf(buf, "FIXME: Not porting yet...");
    }
    else
    {
        return sprintf(buf, "%d\n", get_boot_mode());
    }
}

static ssize_t boot_store(struct kobject *kobj, struct attribute *a, const char *buf, size_t count)
{
    return count;
}


/* boot object */
static struct kobject boot_kobj;
static struct sysfs_ops boot_sysfs_ops = {
    .show = boot_show,
    .store = boot_store
};

/* boot attribute */
struct attribute boot_attr = {BOOT_SYSFS_ATTR, 0644};
struct attribute info_attr = {INFO_SYSFS_ATTR, 0644};
static struct attribute *boot_attrs[] = {
    &boot_attr,
    &info_attr,
    NULL
};

/* boot type */
static struct kobj_type boot_ktype = {
    .sysfs_ops = &boot_sysfs_ops,
    .default_attrs = boot_attrs
};

/* boot device node */
static dev_t boot_dev_num;
static struct cdev boot_cdev;
static struct file_operations boot_fops = {
    .owner = THIS_MODULE,
    .open = NULL,
    .release = NULL,
    .write = NULL,
    .read = NULL,
    .unlocked_ioctl = NULL
};

/* boot device class */
static struct class *boot_class;
static struct device *boot_device;


bool com_is_enable(void)  // usb android will check whether is com port enabled default. in normal boot it is default enabled.
{
    if(get_boot_mode() == NORMAL_BOOT)
	{
        return false;
	}
	else
	{
        return true;
	}
}

void set_meta_com(META_COM_TYPE type, unsigned int id)
{
    g_meta_com_type = type;
    g_meta_com_id = id;
}

META_COM_TYPE get_meta_com_type(void)
{
    return g_meta_com_type;
}

unsigned int get_meta_com_id(void)
{
    return g_meta_com_id;
}

unsigned int get_meta_uart_port(void)
{
    return g_meta_uart_port;
}


static ssize_t meta_com_type_show(struct device_driver *driver, char *buf)
{
  return sprintf(buf, "%d\n", g_meta_com_type);
}

static ssize_t meta_com_type_store(struct device_driver *driver, const char *buf, size_t count)
{
  /*Do nothing*/
  return count;
}

DRIVER_ATTR(meta_com_type_info, 0644, meta_com_type_show, meta_com_type_store);


static ssize_t meta_com_id_show(struct device_driver *driver, char *buf)
{
  return sprintf(buf, "%d\n", g_meta_com_id);
}

static ssize_t meta_com_id_store(struct device_driver *driver, const char *buf, size_t count)
{
  /*Do nothing*/
  return count;
}

DRIVER_ATTR(meta_com_id_info, 0644, meta_com_id_show, meta_com_id_store);
static ssize_t meta_uart_port_show(struct device_driver *driver, char *buf)
{
  return sprintf(buf, "%d\n", g_meta_uart_port);
}

static ssize_t meta_uart_port_store(struct device_driver *driver, const char *buf, size_t count)
{
  /*Do nothing*/
  return count;
}

DRIVER_ATTR(meta_uart_port_info, 0644, meta_uart_port_show, meta_uart_port_store);


static int __init create_sysfs(void)
{
    int ret;
    BOOTMODE bm = get_boot_mode();

    /* allocate device major number */
    if (alloc_chrdev_region(&boot_dev_num, 0, 1, BOOT_DEV_NAME) < 0) {
        pr_warn("fail to register chrdev\n");
        return -1;
    }

    /* add character driver */
    cdev_init(&boot_cdev, &boot_fops);
    ret = cdev_add(&boot_cdev, boot_dev_num, 1);
    if (ret < 0) {
        pr_warn("fail to add cdev\n");
        return ret;
    }

    /* create class (device model) */
    boot_class = class_create(THIS_MODULE, BOOT_DEV_NAME);
    if (IS_ERR(boot_class)) {
        pr_warn("fail to create class\n");
        return (int)boot_class;
    }

    boot_device = device_create(boot_class, NULL, boot_dev_num, NULL, BOOT_DEV_NAME);
    if (IS_ERR(boot_device)) {
        pr_warn("fail to create device\n");
        return (int)boot_device;
    }

    /* add kobject */
    ret = kobject_init_and_add(&boot_kobj, &boot_ktype, &(boot_device->kobj), BOOT_SYSFS);
    if (ret < 0) {
        pr_warn("fail to add kobject\n");
        return ret;
    }

    /* FIXME: wait porting */
    //pr_notice("CHIP = 0x%04x 0x%04x\n", get_chip_code(), get_chip_hw_subcode());
    //pr_notice("CHIP = 0x%04x 0x%04x 0x%04x 0x%04x\n", mt_get_chip_info(CHIP_INFO_FUNCTION_CODE),
    //           mt_get_chip_info(CHIP_INFO_PROJECT_CODE), mt_get_chip_info(CHIP_INFO_DATE_CODE),
    //           mt_get_chip_info(CHIP_INFO_FAB_CODE));

#ifdef CONFIG_OF
    if (of_chosen) {
        struct boot_tag_meta_com *tags;
        tags = (struct boot_tag_meta_com *)of_get_property(of_chosen, "atag,meta", NULL);
        if (tags) {
            g_meta_com_type = tags->meta_com_type;
            g_meta_com_id = tags->meta_com_id;
			g_meta_uart_port = tags->meta_uart_port;
            pr_notice("[%s] g_meta_com_type = %d, g_meta_com_id = %d, g_meta_uart_port=%d. \n", __func__, g_meta_com_type, g_meta_com_id,g_meta_uart_port);
        }
        else
            pr_notice("[%s] No atag,meta found !\n", __func__);
    }
    else
        pr_notice("[%s] of_chosen is NULL !\n", __func__);
#endif

    if(bm == META_BOOT || bm == ADVMETA_BOOT || bm == ATE_FACTORY_BOOT || bm == FACTORY_BOOT)
    {
        /* register driver and create sysfs files */
        ret = driver_register(&meta_com_type_info.driver);
        if (ret)
        {
            pr_warn("fail to register META COM TYPE driver\n");
        }
        ret = driver_create_file(&meta_com_type_info.driver, &driver_attr_meta_com_type_info);
        if (ret)
        {
            pr_warn("fail to create META COM TPYE sysfs file\n");
        }

        ret = driver_register(&meta_com_id_info.driver);
        if (ret)
        {
            pr_warn("fail to register META COM ID driver\n");
        }
        ret = driver_create_file(&meta_com_id_info.driver, &driver_attr_meta_com_id_info);
        if (ret)
        {
            pr_warn("fail to create META COM ID sysfs file\n");
        }
		ret = driver_register(&meta_uart_port_info.driver);
        if (ret)
        {
            pr_warn("fail to register META UART PORT driver\n");
        }
        ret = driver_create_file(&meta_uart_port_info.driver, &driver_attr_meta_uart_port_info);
        if (ret)
        {
            pr_warn("fail to create META UART PORT sysfs file\n");
        }
    }

    return 0;
}

static void __exit destroy_sysfs(void)
{
    cdev_del(&boot_cdev);
}

static int __init boot_mod_init(void)
{
    create_sysfs();
    return 0;
}

static void __exit boot_mod_exit(void)
{
    destroy_sysfs();
}

module_init(boot_mod_init);
module_exit(boot_mod_exit);
MODULE_DESCRIPTION("MTK Boot Information Querying Driver");
MODULE_LICENSE("GPL");
