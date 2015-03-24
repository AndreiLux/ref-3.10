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

#include <mach/mtk_ccci_helper.h>


#define android_bring_up_prepare 1	/* when porting ccci drver for android bring up, enable the macro */

ccci_kern_func_info ccci_func_table[MAX_MD_SYS_SUPPORT_NUM][MAX_KERN_API];
ccci_sys_cb_func_info_t ccci_sys_cb_table_1000[MAX_MD_SYS_SUPPORT_NUM][MAX_KERN_API];
ccci_sys_cb_func_info_t ccci_sys_cb_table_100[MAX_MD_SYS_SUPPORT_NUM][MAX_KERN_API];

int (*ccci_sys_msg_notify_func[MAX_MD_SYS_SUPPORT_NUM]) (int, unsigned int, unsigned int);



/***************************************************************************
 * Make device node helper function section
 ***************************************************************************/
#if 0
static void *create_dev_class(struct module *owner, const char *name)
{
	int err = 0;
	struct class *dev_class = class_create(owner, name);
	if (IS_ERR(dev_class)) {
		err = PTR_ERR(dev_class);
		pr_info("create %s class fail, %d\n", name, err);
		return NULL;
	}

	return dev_class;
}

static void release_dev_class(void *dev_class)
{
	if (NULL != dev_class)
		class_destroy(dev_class);
}

static int registry_dev_node(void *dev_class, const char *name, int major_id, int minor_start_id,
			     int index)
{
	int ret = 0;
	dev_t dev;
	struct device *devices;

	if (index >= 0) {
		dev = MKDEV(major_id, minor_start_id) + index;
		devices =
		    device_create((struct class *)dev_class, NULL, dev, NULL, "%s%d", name, index);
	} else {
		dev = MKDEV(major_id, minor_start_id);
		devices = device_create((struct class *)dev_class, NULL, dev, NULL, "%s", name);
	}

	if (IS_ERR(devices)) {
		ret = PTR_ERR(devices);
		pr_info("create %s device fail, %d\n", name, ret);
	}
	return ret;
}


static void release_dev_node(void *dev_class, int major_id, int minor_start_id, int index)
{
	device_destroy(dev_class, MKDEV(major_id, minor_start_id) + index);
}

#endif

/* ------------------------------------------------------------------------- */
#define MAX_NR_MODEM 2
#define CONFIG_MTK_ENABLE_MD1
#define CONFIG_MTK_ENABLE_MD2
#define CURR_MD_ID		(0)


#define MD1_MEM_SIZE	(28*1024*1024)
#define MD2_MEM_SIZE	(28*1024*1024)
#define CONFIG_MD1_SMEM_SIZE	(2*1024*1024)
#define CONFIG_MD2_SMEM_SIZE	(2*1024*1024)

extern unsigned long *get_modem_start_addr_list(void);
#if defined(CONFIG_MTK_ENABLE_MD1) && defined(CONFIG_MTK_ENABLE_MD2)
unsigned int modem_size_list[MAX_NR_MODEM] =
    { MD1_MEM_SIZE + CONFIG_MD1_SMEM_SIZE + CONFIG_MD2_SMEM_SIZE, MD2_MEM_SIZE };
static const int memory_usage_case = 1;
#elif defined(CONFIG_MTK_ENABLE_MD1)
unsigned int modem_size_list[MAX_NR_MODEM] = { MD1_MEM_SIZE + CONFIG_MD1_SMEM_SIZE, 0 };

static const int memory_usage_case = 2;
#elif defined(CONFIG_MTK_ENABLE_MD2)
unsigned int modem_size_list[MAX_NR_MODEM] = { MD2_MEM_SIZE + CONFIG_MD2_SMEM_SIZE, 0 };

static const int memory_usage_case = 3;
#else
unsigned int modem_size_list[MAX_NR_MODEM] = { 0, 0, };

static const int memory_usage_case;
#endif

unsigned int get_nr_modem(void)
{
	/* 2 additional modems (rear end) */
	return MAX_NR_MODEM;
}

unsigned int *get_modem_size_list(void)
{
	return modem_size_list;
}

extern unsigned long *get_modem_start_addr_list(void);
unsigned int get_md_memory_start_addr(int md_sys_id)
{
	unsigned long *addr;
	unsigned int md_addr;
	addr = get_modem_start_addr_list();

	switch (md_sys_id) {
	case 0:		/* For MD1 */
		if (memory_usage_case == 2)	/* Only MD2 enabled */
			md_addr = 0;
		else
			md_addr = (unsigned int)addr[0];
		break;

	case 1:		/* For MD2 */
		if (memory_usage_case == 1)	/* Both 2 MD enabled */
			md_addr = (unsigned int)addr[1];
		else if (memory_usage_case == 2)	/* Only MD2 enabled, but MD2 using MD1 memory */
			md_addr = (unsigned int)addr[0];
		else
			md_addr = 0;
		break;
	default:
		md_addr = 0;
		break;
	}
	pr_info("[ccci/ctl] md%d memory addr %08x\n", md_sys_id + 1, md_addr);
	if ((md_addr & (32 * 1024 * 1024 - 1)) != 0)
		pr_info("[ccci/ctl] md%d memory addr is not 32M align!!!\n", md_sys_id + 1);

	return md_addr;
}
EXPORT_SYMBOL(get_md_memory_start_addr);

unsigned int get_md_smem_start_addr(int md_sys_id)
{
	unsigned long *addr;
	unsigned int md_smem_addr;
	addr = get_modem_start_addr_list();

	switch (md_sys_id) {
	case 0:		/* For MD1 */
		if (memory_usage_case == 2)	/* Only MD2 enabled */
			md_smem_addr = 0;
		else
			md_smem_addr = (unsigned int)(addr[0] + MD1_MEM_SIZE);
		break;

	case 1:		/* For MD2 */
		if (memory_usage_case == 1)	/* Both 2 MD enabled */
			md_smem_addr =
			    (unsigned int)(addr[0] + MD1_MEM_SIZE + CONFIG_MD1_SMEM_SIZE);
		else if (memory_usage_case == 2)	/* Only MD2 enabled, but MD2 using MD1 memory */
			md_smem_addr = (unsigned int)(addr[0] + MD2_MEM_SIZE);
		else
			md_smem_addr = 0;
		break;
	default:
		md_smem_addr = 0;
		break;
	}
	pr_info("[ccci/ctl] md%d share memory addr %08x\n", md_sys_id + 1, md_smem_addr);
	if ((md_smem_addr & (2 * 1024 * 1024 - 1)) != md_smem_addr)
		pr_info("[ccci/ctl] md%d share memory addr %08x\n", md_sys_id + 1, md_smem_addr);

	return md_smem_addr;
}
EXPORT_SYMBOL(get_md_smem_start_addr);


/***************************************************************************
 * provide API called by ccci module
 ***************************************************************************/

AP_IMG_TYPE get_ap_img_ver(void)
{
#if !defined(android_bring_up_prepare)

#if defined(MODEM_2G)
	return AP_IMG_2G;
#elif defined(MODEM_3G)
	return AP_IMG_3G;
#else
	return AP_IMG_INVALID;
#endif

#else
	return AP_IMG_3G;
#endif
}
EXPORT_SYMBOL(get_ap_img_ver);


int get_td_eint_info(int md_id, char *eint_name, unsigned int len)
{
#if !defined(android_bring_up_prepare)
	return get_td_eint_num(eint_name, len);

#else
	return -1;
#endif
}
EXPORT_SYMBOL(get_td_eint_info);


int get_md_gpio_info(int md_id, char *gpio_name, unsigned int len)
{
	/* #if !defined (android_bring_up_prepare) */
#if 1
	return mt_get_md_gpio(gpio_name, len);

#else
	return -1;
#endif
}
EXPORT_SYMBOL(get_md_gpio_info);


int get_md_adc_info(int md_id, char *adc_name, unsigned int len)
{
#if !defined(android_bring_up_prepare)
	return IMM_get_adc_channel_num(adc_name, len);

#else
	return -1;
#endif
}
EXPORT_SYMBOL(get_md_adc_info);

#if 0
static int get_eint_attribute(char *name, unsigned int name_len, unsigned int type, char *result,
			      unsigned int *len)
{
	return 0;
}
#endif

int get_eint_attr(char *name, unsigned int name_len, unsigned int type, char *result,
		  unsigned int *len)
{
	/* #if !defined (android_bring_up_prepare) */
#if 1
	return get_eint_attribute(name, name_len, type, result, len);

#else
	return -1;
#endif
}
EXPORT_SYMBOL(get_eint_attr);



int get_dram_type_clk(int *clk, int *type)
{
#if !defined(android_bring_up_prepare)
	return get_dram_info(clk, type);
#else
	return -1;
#endif
}
EXPORT_SYMBOL(get_dram_type_clk);

#include <mach/pmic_mt6320_sw.h>
#include <drivers/misc/mediatek/power/mt8135/upmu_common.h>
#include <mach/upmu_hw.h>

int ccci_set_md2_vast_en(unsigned int en)
{
	upmu_set_rg_vast_en(en);
	return 0;
}
EXPORT_SYMBOL(ccci_set_md2_vast_en);


/***************************************************************************
 * Make sysfs node helper function section
 ***************************************************************************/
ssize_t mtk_ccci_attr_show(struct kobject *kobj, struct attribute *attr, char *buffer);
ssize_t mtk_ccci_attr_store(struct kobject *kobj, struct attribute *attr, const char *buffer,
			    size_t size);
ssize_t mtk_ccci_filter_show(struct kobject *kobj, char *page);
ssize_t mtk_ccci_filter_store(struct kobject *kobj, const char *page, size_t size);

struct sysfs_ops mtk_ccci_sysfs_ops = {
	.show = mtk_ccci_attr_show,
	.store = mtk_ccci_attr_store,
};

struct mtk_ccci_sys_entry {
	struct attribute attr;
	 ssize_t(*show) (struct kobject *kobj, char *page);
	 ssize_t(*store) (struct kobject *kobj, const char *page, size_t size);
};

static struct mtk_ccci_sys_entry filter_setting_entry = {
	{.name = "filter", .mode = S_IRUGO | S_IWUSR},
	mtk_ccci_filter_show,
	mtk_ccci_filter_store,
};

struct attribute *mtk_ccci_attributes[] = {
	&filter_setting_entry.attr,
	NULL,
};

struct kobj_type mtk_ccci_ktype = {
	.sysfs_ops = &mtk_ccci_sysfs_ops,
	.default_attrs = mtk_ccci_attributes,
};

static struct mtk_ccci_sysobj {
	struct kobject kobj;
} ccci_sysobj;

static int mtk_ccci_sysfs(void)
{
	struct mtk_ccci_sysobj *obj = &ccci_sysobj;

	memset(&obj->kobj, 0x00, sizeof(obj->kobj));

	obj->kobj.parent = kernel_kobj;
	if (kobject_init_and_add(&obj->kobj, &mtk_ccci_ktype, NULL, "ccci")) {
		kobject_put(&obj->kobj);
		return -ENOMEM;
	}
	kobject_uevent(&obj->kobj, KOBJ_ADD);

	return 0;
}

ssize_t mtk_ccci_attr_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
	struct mtk_ccci_sys_entry *entry = container_of(attr, struct mtk_ccci_sys_entry, attr);
	return entry->show(kobj, buffer);
}

ssize_t mtk_ccci_attr_store(struct kobject *kobj, struct attribute *attr, const char *buffer,
			    size_t size)
{
	struct mtk_ccci_sys_entry *entry = container_of(attr, struct mtk_ccci_sys_entry, attr);
	return entry->store(kobj, buffer, size);
}

/*---------------------------------------------------------------------------*/
/* Filter table                                                              */
/*---------------------------------------------------------------------------*/
#define MAX_FILTER_MEMBER		(4)
typedef size_t(*ccci_sys_cb_func_t) (const char buf[], size_t len);
typedef struct _cmd_op_map {
	char cmd[8];
	int cmd_len;
	ccci_sys_cb_func_t store;
	ccci_sys_cb_func_t show;
} cmd_op_map_t;

cmd_op_map_t cmd_map_table[MAX_FILTER_MEMBER] = { {"", 0}, {"", 0}, {"", 0}, {"", 0} };

ssize_t mtk_ccci_filter_show(struct kobject *kobj, char *buffer)
{
	int i;
	int remain = PAGE_SIZE;
	unsigned long len;
	char *ptr = buffer;

	for (i = 0; i < MAX_FILTER_MEMBER; i++) {
		if (cmd_map_table[i].cmd_len != 0) {
			/* Find a mapped cmd */
			if (NULL != cmd_map_table[i].show) {
				len = cmd_map_table[i].show(ptr, remain);
				ptr += len;
				remain -= len;
			}
		}
	}

	return (PAGE_SIZE - remain);
}

ssize_t mtk_ccci_filter_store(struct kobject *kobj, const char *buffer, size_t size)
{
	int i;

	for (i = 0; i < MAX_FILTER_MEMBER; i++) {
		if (strncmp(buffer, cmd_map_table[i].cmd, cmd_map_table[i].cmd_len) == 0) {
			/* Find a mapped cmd */
			if (NULL != cmd_map_table[i].store) {
				return cmd_map_table[i].store(buffer, size);
			}
		}
	}
	pr_info("unsupport cmd\n");
	return size;
}

int register_filter_func(char cmd[], ccci_sys_cb_func_t store, ccci_sys_cb_func_t show)
{
	int i;
	int empty_slot = -1;
	int cmd_len;

	for (i = 0; i < MAX_FILTER_MEMBER; i++) {
		if (0 == cmd_map_table[i].cmd_len) {
			/* Find a empty slot */
			if (-1 == empty_slot)
				empty_slot = i;
		} else if (strcmp(cmd, cmd_map_table[i].cmd) == 0) {
			/* Find a duplicate cmd */
			return -1;
		}
	}
	if (-1 != empty_slot) {
		cmd_len = strlen(cmd);
		if (cmd_len > 7) {
			return -2;
		}

		cmd_map_table[empty_slot].cmd_len = cmd_len;
		for (i = 0; i < cmd_len; i++)
			cmd_map_table[empty_slot].cmd[i] = cmd[i];
		cmd_map_table[empty_slot].cmd[i] = 0;	/* termio char */
		cmd_map_table[empty_slot].store = store;
		cmd_map_table[empty_slot].show = show;
		return 0;
	}
	return -3;
}
EXPORT_SYMBOL(register_filter_func);



/***************************************************************************/
/* Register kernel API for ccci driver invoking
/ **************************************************************************/
int register_ccci_kern_func_by_md_id(int md_sys_id, unsigned int id, ccci_kern_cb_func_t func)
{
	int ret = 0;
	ccci_kern_func_info *info_ptr;

	if ((id >= MAX_KERN_API) || (func == NULL) || (md_sys_id >= MAX_MD_SYS_SUPPORT_NUM)) {
		pr_info("[ccci/ctl] invalid parameter for kern func(md_sys:%d; cmd:%d)!\n",
			md_sys_id + 1, id);
		return E_PARAM;
	}

	info_ptr = &(ccci_func_table[md_sys_id][id]);
	if (info_ptr->func == NULL) {
		info_ptr->id = id;
		info_ptr->func = func;
	} else
		pr_info("[ccci/ctl] md%d, kern func%d has registered!\n", md_sys_id + 1, id);

	return ret;
}
EXPORT_SYMBOL(register_ccci_kern_func_by_md_id);

int register_ccci_kern_func(unsigned int id, ccci_kern_cb_func_t func)
{
	return register_ccci_kern_func_by_md_id(CURR_MD_ID, id, func);
}
EXPORT_SYMBOL(register_ccci_kern_func);


int exec_ccci_kern_func_by_md_id(int md_sys_id, unsigned int id, char *buf, unsigned int len)
{
	ccci_kern_cb_func_t func;
	int ret = 0;

	if (md_sys_id >= MAX_MD_SYS_SUPPORT_NUM) {
		pr_info("[ccci/ctl] invalid md id(%d) for kern exec!\n", md_sys_id + 1);
		return E_PARAM;
	}
	if (id >= MAX_KERN_API) {
		pr_info("[ccci/ctl] invalid function id(%d) for kern exec!\n", id);
		return E_PARAM;
	}

	func = ccci_func_table[md_sys_id][id].func;
	if (func != NULL) {
		ret = func(md_sys_id, buf, len);
	} else {
		ret = E_NO_EXIST;
		pr_info("[ccci/ctl] md%d, kern func%d not register!\n", md_sys_id + 1, id);
	}

	return ret;
}
EXPORT_SYMBOL(exec_ccci_kern_func_by_md_id);

int exec_ccci_kern_func(unsigned int id, char *buf, unsigned int len)
{
	return exec_ccci_kern_func_by_md_id(CURR_MD_ID, id, buf, len);
}
EXPORT_SYMBOL(exec_ccci_kern_func);

int notifiy_md_by_sys_msg(int md_sys_id, unsigned int msg, unsigned int data)
{
	int (*func) (int, unsigned int, unsigned int);
	int ret = 0;

	if (md_sys_id >= MAX_MD_SYS_SUPPORT_NUM) {
		pr_info("[ccci/ctl] invalid md id(%d) for notifiy_md_by_sys_msg!\n", md_sys_id + 1);
		return E_PARAM;
	}

	func = ccci_sys_msg_notify_func[md_sys_id];
	if (func != NULL) {
		ret = func(md_sys_id, msg, data);
	} else {
		ret = E_NO_EXIST;
		pr_info("[ccci/ctl] md%d, notifiy_md_by_sys_msg func not register!\n",
			md_sys_id + 1);
	}

	return ret;
}
EXPORT_SYMBOL(notifiy_md_by_sys_msg);

int register_sys_msg_notify_func(int md_sys_id, int (*func) (int, unsigned int, unsigned int))
{
	int ret = 0;

	if (md_sys_id >= MAX_MD_SYS_SUPPORT_NUM) {
		pr_info("[ccci/ctl] invalid md id:%d for register_sys_msg_notify_func!\n",
			md_sys_id + 1);
		return E_PARAM;
	}

	if (ccci_sys_msg_notify_func[md_sys_id] == NULL) {
		ccci_sys_msg_notify_func[md_sys_id] = func;
	} else {
		pr_info("[ccci/ctl] md%d, ccci_sys_msg_notify_func has registered!\n",
			md_sys_id + 1);
	}

	return ret;
}
EXPORT_SYMBOL(register_sys_msg_notify_func);

int register_ccci_sys_call_back(int md_sys_id, unsigned int id, ccci_system_ch_cb_func_t func)
{
	int ret = 0;
	ccci_sys_cb_func_info_t *info_ptr;

	if (md_sys_id >= MAX_MD_SYS_SUPPORT_NUM) {
		pr_info("[ccci/ctl] invalid md id%d for sys ch cb!\n", md_sys_id + 1);
		return E_PARAM;
	}

	if ((id >= 0x100) && ((id - 0x100) < MAX_KERN_API)) {
		info_ptr = &(ccci_sys_cb_table_100[md_sys_id][id - 0x100]);
	} else if ((id >= 0x1000) && ((id - 0x1000) < MAX_KERN_API)) {
		info_ptr = &(ccci_sys_cb_table_1000[md_sys_id][id - 0x1000]);
	} else {
		pr_info("[ccci/ctl] invalid parameter for sys ch cb(md_sys:%d; cmd:%d)!\n",
			md_sys_id + 1, id);
		return E_PARAM;
	}

	if (info_ptr->func == NULL) {
		info_ptr->id = id;
		info_ptr->func = func;
	} else
		pr_info("[ccci/ctl] md%d, cb func%d has registered!\n", md_sys_id + 1, id);

	return ret;
}
EXPORT_SYMBOL(register_ccci_sys_call_back);


void dispatch_to_user(int md_sys_id, int cb_id, int data)
{
	ccci_system_ch_cb_func_t func;
	int id;
	ccci_sys_cb_func_info_t *curr_table;

	if (md_sys_id >= MAX_MD_SYS_SUPPORT_NUM) {
		pr_info("[ccci/ctl] invalid md id(%d) to dispatch!\n", md_sys_id + 1);
		return;
	}

	id = cb_id & 0xFF;
	if (id >= MAX_KERN_API) {
		pr_info("[ccci/ctl] invalid function id(0x%x) to dispatch!\n", cb_id);
		return;
	}

	if ((cb_id & (0x1000 | 0x100)) == 0x1000) {
		curr_table = ccci_sys_cb_table_1000[md_sys_id];
	} else if ((cb_id & (0x1000 | 0x100)) == 0x100) {
		curr_table = ccci_sys_cb_table_100[md_sys_id];
	} else {
		pr_info("[ccci/ctl] md%d get un-support cb_func_id: 0x%x!\n", md_sys_id + 1, cb_id);
		return;
	}
	func = curr_table[id].func;
	if (func != NULL) {
		func(data);
	} else {
		pr_info("[ccci/ctl] md%d, cb_func_id 0x%x not register!\n", md_sys_id + 1, cb_id);
	}
}
EXPORT_SYMBOL(dispatch_to_user);


/*-------------------------------------------------------------------------------------------------
 * Device interface for ccci helper
 *-------------------------------------------------------------------------------------------------
 */
typedef struct ccci_pm_cb_item {
	void (*cb_func) (int);
	int md_sys_id;
} ccci_pm_cb_item_t;


static ccci_pm_cb_item_t suspend_cb_table[MAX_MD_SYS_SUPPORT_NUM][MAX_SLEEP_API];
static ccci_pm_cb_item_t resume_cb_table[MAX_MD_SYS_SUPPORT_NUM][MAX_SLEEP_API];

void register_suspend_notify(int md_sys_id, unsigned int id, void (*func) (int))
{
	if ((id >= MAX_SLEEP_API) || (func == NULL) || (md_sys_id >= MAX_MD_SYS_SUPPORT_NUM)) {
		pr_info("[ccci/ctl] invalid suspend parameter(md:%d, cmd:%d)!\n", md_sys_id, id);
	}

	if (suspend_cb_table[md_sys_id][id].cb_func == NULL) {
		suspend_cb_table[md_sys_id][id].cb_func = func;
		suspend_cb_table[md_sys_id][id].md_sys_id = md_sys_id;
	}
}
EXPORT_SYMBOL(register_suspend_notify);

void register_resume_notify(int md_sys_id, unsigned int id, void (*func) (int))
{
	if ((id >= MAX_SLEEP_API) || (func == NULL) || (md_sys_id >= MAX_MD_SYS_SUPPORT_NUM)) {
		pr_info("[ccci/ctl] invalid resume parameter(md:%d, cmd:%d)!\n", md_sys_id, id);
	}

	if (resume_cb_table[md_sys_id][id].cb_func == NULL) {
		resume_cb_table[md_sys_id][id].cb_func = func;
		resume_cb_table[md_sys_id][id].md_sys_id = md_sys_id;
	}
}
EXPORT_SYMBOL(register_resume_notify);


static int ccci_helper_probe(struct platform_device *dev)
{

	pr_info("\nccci_helper_probe\n");
	return 0;
}

static int ccci_helper_remove(struct platform_device *dev)
{
	pr_info("\nccci_helper_remove\n");
	return 0;
}

static void ccci_helper_shutdown(struct platform_device *dev)
{
	pr_info("\nccci_helper_shutdown\n");
}

static int ccci_helper_suspend(struct platform_device *dev, pm_message_t state)
{
	int i, j;
	void (*func) (int);
	int md_sys_id;

	/* pr_info("\nccci_helper_suspend\n" ); */

	for (i = 0; i < MAX_MD_SYS_SUPPORT_NUM; i++) {
		for (j = 0; j < SLP_ID_MAX; j++) {
			func = suspend_cb_table[i][j].cb_func;
			md_sys_id = suspend_cb_table[i][j].md_sys_id;
			if (func != NULL)
				func(md_sys_id);
		}
	}

	return 0;
}

static int ccci_helper_resume(struct platform_device *dev)
{
	int i, j;
	void (*func) (int);
	int md_sys_id;

	/* pr_info("\nccci_helper_resume\n" ); */

	for (i = 0; i < MAX_MD_SYS_SUPPORT_NUM; i++) {
		for (j = 0; j < RSM_ID_MAX; j++) {
			func = resume_cb_table[i][j].cb_func;
			md_sys_id = resume_cb_table[i][j].md_sys_id;
			if (func != NULL)
				func(md_sys_id);
		}
	}

	return 0;
}

static struct platform_driver ccci_helper_driver = {
	.driver = {
		   .name = "ccci-helper",
		   },
	.probe = ccci_helper_probe,
	.remove = ccci_helper_remove,
	.shutdown = ccci_helper_shutdown,
	.suspend = ccci_helper_suspend,
	.resume = ccci_helper_resume,
};

struct platform_device ccci_helper_device = {
	.name = "ccci-helper",
	.id = 0,
	.dev = {}
};

static int __init ccci_helper_init(void)
{
	int ret;

	/* Init ccci helper sys fs */
	memset((void *)cmd_map_table, 0, sizeof(cmd_map_table));
	mtk_ccci_sysfs();

	/* init ccci kernel API register table */
	memset((void *)ccci_func_table, 0, sizeof(ccci_func_table));

	/* init ccci system channel call back function register table */
	memset((void *)ccci_sys_cb_table_100, 0, sizeof(ccci_sys_cb_table_100));
	memset((void *)ccci_sys_cb_table_1000, 0, sizeof(ccci_sys_cb_table_1000));

	ret = platform_device_register(&ccci_helper_device);
	if (ret) {
		pr_info("ccci_helper_device register fail(%d)\n", ret);
		return ret;
	}

	ret = platform_driver_register(&ccci_helper_driver);
	if (ret) {
		pr_info("ccci_helper_driver register fail(%d)\n", ret);
		return ret;
	}


	return 0;
}
module_init(ccci_helper_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MTK");
MODULE_DESCRIPTION("The ccci helper function");
