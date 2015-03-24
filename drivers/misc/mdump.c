
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/reboot.h>
#include <linux/mdump.h>
#include <linux/atomic.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/syscore_ops.h>
#include <linux/sysfs.h>
#include <linux/mm.h>
#include <asm/cacheflush.h>

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
#include <mach/mt_boot_common.h>
#endif

struct mdbinattr {
	struct bin_attribute binattr;
	int readonly;
	char *address;
};
struct mdtxtattr {
	struct attribute attr;
	void *udata;
	ssize_t (*show)(void*, char *buffer);
	ssize_t (*store)(void*, const char *buffer, size_t size);
};

static struct mdump_buffer *s_mdump_buffer;
static struct kobject s_mdump_kobj;

static ssize_t mdump_binary_read(struct file *filp,
		struct kobject *kobj,
		struct bin_attribute *attr,
		char *buf, loff_t off, size_t count)
{
	struct mdbinattr *mdattr = (struct mdbinattr *) attr;

	if (off > mdattr->binattr.size)
		return -ERANGE;
	if (off + count > mdattr->binattr.size)
		count = mdattr->binattr.size - off;
	if (count != 0)
		memcpy(buf, mdattr->address + off, count);
	return count;
}

static ssize_t mdump_binary_write(struct file *filp,
		struct kobject *kobj,
		struct bin_attribute *attr,
		char *data, loff_t off, size_t count)
{
	struct mdbinattr *mdattr = (struct mdbinattr *) attr;

	if (mdattr->readonly == 0) {
		if (off > mdattr->binattr.size)
			return -ERANGE;
		if (off + count > mdattr->binattr.size)
			count = mdattr->binattr.size - off;
		if (count != 0)
			memcpy(mdattr->address + off, data, count);
	}
	return count;
}

static ssize_t mdump_text_show(struct kobject *kobj,
		struct attribute *attr,
		char *buffer)
{
	struct mdtxtattr *mdattr = (struct mdtxtattr *) attr;
	if (mdattr->show == NULL) {
		buffer[0] = 0;
		return 0;
	}
	return mdattr->show(mdattr->udata, buffer);
}

static ssize_t mdump_text_store(struct kobject *kobj,
		struct attribute *attr,
		const char *buffer, size_t size)
{
	struct mdtxtattr *mdattr = (struct mdtxtattr *) attr;
	if (mdattr->store == NULL)
		return size;
	return mdattr->store(mdattr->udata, buffer, size);
}

static ssize_t mdump_enable_show(void *ud, char *buffer)
{
	if (!s_mdump_buffer->enable_flags) {
		strcpy(buffer, "OFF");
		return 3;
	} else {
		strcpy(buffer, "ON");
		return 2;
	}
}

static ssize_t mdump_enable_store(void *ud, const char *buffer, size_t size)
{
	u16 flags = s_mdump_buffer->enable_flags;

	if ((size >= 3) && !strncmp(buffer, "OFF", 3))
		flags = 0;
	else if ((size >= 3) && !strncmp(buffer, "off", 3))
		flags = 0;
	else if ((size >= 2) && !strncmp(buffer, "ON", 2))
		flags = 1;
	else if ((size >= 2) && !strncmp(buffer, "on", 2))
		flags = 1;
	else
		pr_err("Unknown store value: %s\n", buffer);
	if (flags != s_mdump_buffer->enable_flags) {
		s_mdump_buffer->enable_flags = flags;
		flush_cache_all();
	}
	return size;
}

static ssize_t mdump_reason_show(void *ud, char *buffer)
{
	const char *mesg = 0;

	switch (s_mdump_buffer->backup_reason) {
	case MDUMP_COLD_RESET:
		mesg = "Cold reset";
		break;
	case MDUMP_REBOOT_WATCHDOG:
		mesg = "Watchdog";
		break;
	case MDUMP_REBOOT_PANIC:
		mesg = "Kernel Panic";
		break;
	case MDUMP_REBOOT_NORMAL:
		mesg = "Warm Reboot";
		break;
	case MDUMP_REBOOT_HARDWARE:
		mesg = "Hardware reset";
		break;
	default:
		mesg = "Unknown reason";
		break;
	}
	strcpy(buffer, mesg);
	return strlen(buffer);
}

static ssize_t mdump_list_show(void *ud, char *buffer)
{
	buffer[0] = 0;
	return 0;
}

static ssize_t mdump_list_store(void *ud, const char *buffer, size_t size)
{
	return size;
}

/*---------------------------------------------------------------------------*/

void mdump_mark_reboot_reason(int reason)
{
	if ((s_mdump_buffer != NULL)
			&& (reason < s_mdump_buffer->reboot_reason)) {
		s_mdump_buffer->reboot_reason = (u8) reason;
		flush_cache_all();
	}
}

EXPORT_SYMBOL(mdump_mark_reboot_reason);

/*---------------------------------------------------------------------------*/

static struct mdbinattr s_mdump_pblmsg = {
	{
		{ .name = "pblmsg", .mode = S_IRUGO },
		.size = 0,
		.read = mdump_binary_read,
		.write = mdump_binary_write
	},
	.readonly = 1,
	.address = NULL
};
static struct mdbinattr s_mdump_lkmsg = {
	{
		{ .name = "lkmsg", .mode = S_IRUGO },
		.size = 0,
		.read = mdump_binary_read,
		.write = mdump_binary_write
	},
	1,
	NULL
};

static struct mdtxtattr s_mdump_enable = {
	{ .name = "enable", .mode = S_IRUGO | S_IWUSR },
	NULL,
	mdump_enable_show,
	mdump_enable_store
};
static struct mdtxtattr s_mdump_reason = {
	{ .name = "reason", .mode = S_IRUGO },
	NULL,
	mdump_reason_show,
	NULL
};
static struct mdtxtattr s_mdump_list = {
	{ .name = "list", .mode = S_IRUGO | S_IWUSR },
	NULL,
	mdump_list_show,
	mdump_list_store
};
static const struct sysfs_ops s_mdump_sysfs_ops = {
	.show   = mdump_text_show,
	.store  = mdump_text_store,
};
static struct attribute *s_mdump_attributes[] = {
	&s_mdump_enable.attr,
	&s_mdump_reason.attr,
	&s_mdump_list.attr,
	NULL,
};

static struct kobj_type s_mdump_type = {
	.sysfs_ops = &s_mdump_sysfs_ops,
	.default_attrs = s_mdump_attributes,
};

static int __init mdump_init(void)
{
	pr_info("Check mdump buffer on address: 0x%x\n",
			CONFIG_MDUMP_BUFFER_ADDRESS);
	s_mdump_buffer = (struct mdump_buffer *)
			phys_to_virt(CONFIG_MDUMP_BUFFER_ADDRESS);
	if (s_mdump_buffer->signature != MDUMP_BUFFER_SIG) {
		memset(s_mdump_buffer, 0, sizeof(struct mdump_buffer));
		s_mdump_buffer->signature = MDUMP_BUFFER_SIG;
		s_mdump_buffer->backup_reason = MDUMP_COLD_RESET;
		pr_err("MDump buffer not found. initial.\n");
	} else {
		pr_info("MDump: found existing mdump_buffer\n");
		pr_info("MDump Boot Reason: 0x%x\n",
				s_mdump_buffer->backup_reason);
	}
	s_mdump_buffer->reboot_reason = MDUMP_REBOOT_HARDWARE;
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	if (g_boot_mode != KERNEL_POWER_OFF_CHARGING_BOOT && g_boot_mode != LOW_POWER_OFF_CHARGING_BOOT)
		s_mdump_buffer->enable_flags = 1;
	else
		s_mdump_buffer->enable_flags = 0;
#else
	s_mdump_buffer->enable_flags = 1;
#endif

	if (kobject_init_and_add(&s_mdump_kobj,
			&s_mdump_type, kernel_kobj, "mdump")) {
		kobject_put(&s_mdump_kobj);
		return -ENOMEM;
	}

	s_mdump_pblmsg.binattr.size = strlen(s_mdump_buffer->stage1_messages);
	s_mdump_pblmsg.address = s_mdump_buffer->stage1_messages;
	if (sysfs_create_bin_file(&s_mdump_kobj, &s_mdump_pblmsg.binattr) != 0)
		pr_err("Create Preloader message file failed.\n");

	s_mdump_lkmsg.binattr.size = strlen(s_mdump_buffer->stage2_messages);
	s_mdump_lkmsg.address = s_mdump_buffer->stage2_messages;
	if (sysfs_create_bin_file(&s_mdump_kobj, &s_mdump_lkmsg.binattr) != 0)
		pr_err("Create LK message file failed.\n");

	return 0;
}

arch_initcall(mdump_init);
/*---------------------------------------------------------------------------*/
MODULE_AUTHOR("NEUSOFT");
MODULE_DESCRIPTION("NEUSOFT MDUMP MODULE");
MODULE_LICENSE("GPL");

