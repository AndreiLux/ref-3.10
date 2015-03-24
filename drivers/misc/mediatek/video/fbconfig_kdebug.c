#include <linux/string.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/wait.h>
#include <mach/mt_typedefs.h>
#include "disp_hal.h"
#include "mtkfb.h"
#include "debug.h"
#include "lcm_drv.h"
#include "ddp_path.h"
#include "disp_drv.h"
#include "lcd_drv.h"

/* **************************************************************************** */
/* This part is for customization parameters of D-IC and DSI . */
/* **************************************************************************** */
extern LCM_PARAMS *lcm_params;
extern LCM_DRIVER *lcm_drv;
extern LCM_UTIL_FUNCS fbconfig_lcm_utils;
extern BOOL is_early_suspended;
extern FBCONFIG_DISP_IF *fbconfig_if_drv;
BOOL fbconfig_start_LCM_config;
#define FBCONFIG_MDELAY(n)	(fbconfig_lcm_utils.mdelay((n)))
#define SET_RESET_PIN(v)	(fbconfig_lcm_utils.set_reset_pin((v)))
#define dsi_set_cmdq(pdata, queue_size, force_update)		fbconfig_lcm_utils.dsi_set_cmdq(pdata, queue_size, force_update)
#define read_reg_v2(cmd, buffer, buffer_size)			fbconfig_lcm_utils.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)
#define FBCONFIG_KEEP_NEW_SETTING 1
#define FBCONFIG_DEBUG 0
/* IOCTL commands. */

#define FBCONFIG_IOW(num, dtype)     _IOW('X', num, dtype)
#define FBCONFIG_IOR(num, dtype)     _IOR('X', num, dtype)
#define FBCONFIG_IOWR(num, dtype)    _IOWR('X', num, dtype)
#define FBCONFIG_IO(num)             _IO('X', num)

#define LCM_GET_ID     FBCONFIG_IOR(45, unsigned int)
#define LCM_GET_ESD    FBCONFIG_IOR(46, unsigned int)
#define DRIVER_IC_CONFIG    FBCONFIG_IOR(47, unsigned int)
#define DRIVER_IC_RESET    FBCONFIG_IOR(48, unsigned int)


#define MIPI_SET_CLK     FBCONFIG_IOW(51, unsigned int)
#define MIPI_SET_LANE    FBCONFIG_IOW(52, unsigned int)
#define MIPI_SET_TIMING  FBCONFIG_IOW(53, unsigned int)
#define MIPI_SET_VM      FBCONFIG_IOW(54, unsigned int)	/* mipi video mode timing setting */
#define MIPI_SET_CC	 FBCONFIG_IOW(55, unsigned int)	/* mipi non-continuous clock */

#define TE_SET_ENABLE  FBCONFIG_IOW(61, unsigned int)
#define FB_LAYER_DUMP  FBCONFIG_IOW(62, unsigned int)
#define FB_LAYER_GET_SIZE FBCONFIG_IOW(63, unsigned int)
#define FB_LAYER_GET_EN FBCONFIG_IOW(64, unsigned int)
#define LCM_GET_ESD_RET    FBCONFIG_IOR(65, unsigned int)

static int global_layer_id = -1;
struct dentry *ConfigPara_dbgfs = NULL;
static bool record_list_initialed;
CONFIG_RECORD *record_head = NULL;
CONFIG_RECORD *tmp_last = NULL;	/* always point to the last node */
CONFIG_RECORD *record_tmp = NULL;
#if FBCONFIG_KEEP_NEW_SETTING
CONFIG_RECORD *backup_head = NULL;
#endif
int esd_check_addr;
int esd_check_para_num;
char *esd_check_buffer = NULL;
extern struct semaphore sem_early_suspend;
extern void fbconfig_disp_set_mipi_timing(MIPI_TIMING timing);
extern unsigned int fbconfig_get_layer_info(FBCONFIG_LAYER_INFO *layers);
extern unsigned int fbconfig_get_layer_vaddr(int layer_id, int *layer_size, int *enable);
extern int m4u_mva_map_kernel(unsigned int mva, unsigned int size, int sec,
			      unsigned int *map_va, unsigned int *map_size);
extern int m4u_mva_unmap_kernel(unsigned int mva, unsigned int size, unsigned int va);


int fbconfig_get_esd_check_exec(void)
{
	if (lcm_params->type == LCM_TYPE_DSI) {
		int array[4];

		esd_check_buffer =
		    (char *)kmalloc(sizeof(char) * (6 + esd_check_para_num), GFP_KERNEL);
		if (esd_check_buffer == NULL) {
			pr_info("sxk=>esd_check :esd_check_buffer kmalloc fail!!\n");
			return -2;
		}
		array[0] = 0x00013700;
		array[0] = 0x3700 + (esd_check_para_num << 16);
		pr_info("sxk=>esd_check : arrar[0]=0x%x\n", array[0]);
		dsi_set_cmdq(array, 1, 1);
		read_reg_v2(esd_check_addr, esd_check_buffer, 6 + esd_check_para_num);
	}

	return 0;
}


static void print_record(CONFIG_RECORD *record_head)
{
	int z = 0;

	pr_info("sxk=>[PRINT_RECORD] type is %d\n", record_head->type);
	for (z = 0; z < record_head->ins_num; z++)
		pr_info("sxk=>[PRINT_RECORD]data is 0x%x\n", record_head->ins_array[z]);
}


static void record_list_init(void)
{
	pr_info("sxk=>call record_list_init!!\n");
	fbconfig_start_LCM_config = 1;
	record_head = (CONFIG_RECORD *) kmalloc(sizeof(CONFIG_RECORD), GFP_KERNEL);
	record_head->next = NULL;

	tmp_last = record_head;	/* tmp_last always point to the last node; */
	memset(record_head->ins_array, 0x00, sizeof(int) * MAX_INSTRUCTION);
	record_tmp = (CONFIG_RECORD *) kmalloc(sizeof(CONFIG_RECORD), GFP_KERNEL);
	record_tmp->next = NULL;
	memset(record_tmp->ins_array, 0x00, sizeof(int) * MAX_INSTRUCTION);
}


static void record_list_add(void)
{
	CONFIG_RECORD *record_add = (CONFIG_RECORD *) kmalloc(sizeof(CONFIG_RECORD), GFP_KERNEL);

	/* ecord_add = record_tmp ; */
	memcpy(record_add, record_tmp, sizeof(CONFIG_RECORD));
	if (tmp_last->next == NULL)
		tmp_last->next = record_add;

	tmp_last = record_add;
	tmp_last->next = NULL;
#if FBCONFIG_DEBUG
	print_record(tmp_last);
#endif
	/*clear the data content in record_tmp */
	memset(record_tmp->ins_array, 0x00, sizeof(int) * MAX_INSTRUCTION);
}


static void print_from_head_to_tail(void)
{
	int z = 0;
	CONFIG_RECORD *tmp = record_head;

	pr_info("sxk=>print_from_head_to_tail:START\n");
	while (tmp != NULL) {
		pr_info("sxk=>record type is %d;record ins_num is %d\n", tmp->type, tmp->ins_num);
		for (z = 0; z < tmp->ins_num; z++)
			pr_info("sxk=>record conten is :0x%x\n", tmp->ins_array[z]);
		tmp = tmp->next;
	}
	pr_info("sxk=>print_from_head_to_tail:END\n");
}


#if FBCONFIG_KEEP_NEW_SETTING
static void print_from_head_to_tail_backup(void)
{
	int z = 0;
	CONFIG_RECORD *tmp = backup_head;

	pr_info("sxk=>print_from_head_to_tail_BACKUP...........:START\n");
	while (tmp != NULL) {
		pr_info("sxk=>record type is %d;record ins_num is %d\n", tmp->type, tmp->ins_num);
		for (z = 0; z < tmp->ins_num; z++)
			pr_info("sxk=>record conten is :0x%x\n", tmp->ins_array[z]);
		tmp = tmp->next;
	}
	pr_info("sxk=>print_from_head_to_tail_BACKUP..................:END\n");
}


/* backup_lcm_setting will copy the linked list to a backup */
static void fbconfig_backup_lcm_setting(void)
{
	CONFIG_RECORD *new_node = NULL;
	CONFIG_RECORD *tmp = record_head;
	CONFIG_RECORD *backup_tmp = NULL;

	if (tmp != NULL) {
		backup_head = (CONFIG_RECORD *) kmalloc(sizeof(CONFIG_RECORD), GFP_KERNEL);
		memcpy(backup_head, tmp, sizeof(CONFIG_RECORD));
		backup_tmp = backup_head;
		backup_tmp->next = NULL;
		tmp = tmp->next;
	}
	while (tmp != NULL)	/* run over the record_head list ; */
	{
		new_node = (CONFIG_RECORD *) kmalloc(sizeof(CONFIG_RECORD), GFP_KERNEL);
		memcpy(new_node, tmp, sizeof(CONFIG_RECORD));
		backup_tmp->next = new_node;
		backup_tmp = backup_tmp->next;
		new_node = NULL;
		backup_tmp->next = NULL;
		tmp = tmp->next;
	}
}
#endif

/* RECORD_CMD = 0, */
/* RECORD_MS = 1, */
/* RECORD_PIN_SET        = 2, */

int fb_config_execute_cmd(void)
{
	CONFIG_RECORD *tmp = record_head;

	pr_info("sxk=>execute_cmd:START\n");
	while (tmp != NULL) {
		switch (tmp->type) {
		case RECORD_CMD:
			dsi_set_cmdq(tmp->ins_array, tmp->ins_num, 1);
			break;
		case RECORD_MS:
			FBCONFIG_MDELAY(tmp->ins_array[0]);
			/* msleep(tmp->ins_array[0]); */
			break;
		case RECORD_PIN_SET:
			SET_RESET_PIN(tmp->ins_array[0]);
			break;
		default:
			pr_info("sxk=>No such Type!!!!!\n");
		}
		tmp = tmp->next;
	}
	pr_info("sxk=>execute_cmd:END\n");

	return 0;
}


static void free_list_memory(void)
{
	CONFIG_RECORD *f1 = record_head;
	CONFIG_RECORD *f2 = f1;

	if (NULL == record_head)
		return;
	while (NULL != f1->next) {
		f2 = f1->next;
		kfree(f1);
		f1 = f2;
	}
	kfree(f1);
	record_head = NULL;

	return;
}


#if FBCONFIG_KEEP_NEW_SETTING
void fbconfig_apply_new_lcm_setting(void)
{
	CONFIG_RECORD *tmp = backup_head;

	pr_info("sxk=>fbconfig_apply_new_lcm_setting:START\n");
	while (tmp != NULL) {
		switch (tmp->type) {
		case RECORD_CMD:
			dsi_set_cmdq(tmp->ins_array, tmp->ins_num, 1);
			break;
		case RECORD_MS:
			FBCONFIG_MDELAY(tmp->ins_array[0]);
			/* msleep(tmp->ins_array[0]); */
			break;
		case RECORD_PIN_SET:
			SET_RESET_PIN(tmp->ins_array[0]);
			break;
		default:
			pr_info("sxk=>No such Type!!!!!\n");
		}
		tmp = tmp->next;
	}
	pr_info("sxk=>fbconfig_apply_new_lcm_setting:END\n");

	return 0;
}


static void fbconfig_reset_lcm_setting(void)
{
	fbconfig_start_LCM_config = 0;

	if (lcm_params->dsi.mode != CMD_MODE) {
		if (down_interruptible(&sem_early_suspend)) {
			pr_info("sxk=>can't get semaphore in fbconfig_reset_lcm_setting()\n");
			return 0;
		}
		fbconfig_rest_lcm_setting_prepare();	/* video mode */
		up(&sem_early_suspend);
	} else {		/* cmd mode */
		if (down_interruptible(&sem_early_suspend)) {
			pr_info("sxk=>can't get semaphore in execute_cmd()\n");
			return 0;
		}
		lcm_drv->init();
		up(&sem_early_suspend);
	}
}


static void fbconfig_free_backup_setting(void)
{
	CONFIG_RECORD *f1 = backup_head;
	CONFIG_RECORD *f2 = f1;

	if (NULL == backup_head)
		return;

	while (NULL != f1->next) {
		f2 = f1->next;
		kfree(f1);
		f1 = f2;
	}
	kfree(f1);
	backup_head = NULL;
	f1 = f2 = NULL;

	return;
}
#endif


BOOL get_fbconfig_start_lcm_config(void)
{
	return fbconfig_start_LCM_config;
}


static ssize_t fbconfig_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}


static char fbconfig_buffer[2048];

static ssize_t fbconfig_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(fbconfig_buffer) - 1;	/* 2047 */
	int n = 0;

	n += scnprintf(fbconfig_buffer + n, debug_bufmax - n, "sxkhome");
	fbconfig_buffer[n++] = 0;
	/* n = 5 ; */
	/* memcpy(fbconfig_buffer,"sxkhome",6); */

	return simple_read_from_buffer(ubuf, count, ppos, fbconfig_buffer, n);
}


static ssize_t fbconfig_write(struct file *file,
			      const char __user *ubuf, size_t count, loff_t *ppos)
{
	return 0;
}


static long fbconfig_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;

	pr_info("sxk=>run in fbconfig_ioctl**\n");

	switch (cmd) {
	case LCM_GET_ID:
		{
			LCM_DRIVER *lcm = lcm_drv;

			/* get_lcm_id() need implemented in lcm driver ... */
#if 0
			unsigned int lcm_id = lcm->get_lcm_id();
#else
			unsigned int lcm_id = 0;
#endif

			return copy_to_user(argp, &lcm_id, sizeof(lcm_id)) ? -EFAULT : 0;
		}
	case DRIVER_IC_CONFIG:
		{
			pr_info("sxk=>run in case:DRIVER_IC_CONFIG**\n");

			if (record_list_initialed == 0) {
				record_list_init();
				if (copy_from_user
				    (record_head, (void __user *)arg, sizeof(CONFIG_RECORD))) {
					pr_info("sxk=>copy_from_user failed! line:%d\n", __LINE__);
					return -EFAULT;
				}
				print_record(record_head);
				record_list_initialed = 1;
			} else {
				if (copy_from_user
				    (record_tmp, (void __user *)arg, sizeof(CONFIG_RECORD))) {
					pr_info
					    ("[DRIVER_IC_CONFIG]: copy_from_user failed! line:%d\n",
					     __LINE__);
					return -EFAULT;
				}
#if FBCONFIG_DEBUG
				pr_info("sxk=>will print before add to list\n");
				print_record(record_tmp);
#endif
				record_list_add();	/* add new node to list ; */
			}

			return 0;
		}
	case MIPI_SET_CLK:
		{
			unsigned int clk;

			pr_info("sxk=>debug.c call set mipi clk line:%d\n", __LINE__);
			if (copy_from_user(&clk, (void __user *)argp, sizeof(clk))) {
				pr_info("[MIPI_SET_CLK]: copy_from_user failed! line:%d\n",
					__LINE__);
				return -EFAULT;
			} else {
				pr_info("sxk=>debug.c call set mipi clk line:%d\n", __LINE__);
				fbconfig_disp_set_mipi_clk(clk);
			}

			return 0;
		}
	case MIPI_SET_LANE:
		{
			unsigned int lane_num;

			if (copy_from_user(&lane_num, (void __user *)argp, sizeof(lane_num))) {
				pr_info("[MIPI_SET_LANE]: copy_from_user failed! line:%d\n",
					__LINE__);
				return -EFAULT;
			} else {
				fbconfig_disp_set_mipi_lane_num(lane_num);
			}

			return 0;
		}
	case MIPI_SET_TIMING:
		{
			if (!is_early_suspended) {
				MIPI_TIMING timing;

				if (copy_from_user(&timing, (void __user *)argp, sizeof(timing))) {
					pr_info
					    ("[MIPI_SET_TIMING]: copy_from_user failed! line:%d\n",
					     __LINE__);
					return -EFAULT;
				} else {
					fbconfig_disp_set_mipi_timing(timing);
				}
				return 0;
			} else
				return -EFAULT;
		}
	case TE_SET_ENABLE:
		{
			char enable;

			if (copy_from_user(&enable, (void __user *)argp, sizeof(enable))) {
				pr_info("[TE_SET_ENABLE]: copy_from_user failed! line:%d\n",
					__LINE__);
				return -EFAULT;
			} else {
				if (fbconfig_if_drv->set_te_enable)
					fbconfig_if_drv->set_te_enable(enable);
			}

			return 0;
		}
	case FB_LAYER_GET_EN:
		{
			FBCONFIG_LAYER_INFO layers;

			fbconfig_get_layer_info(&layers);
			return copy_to_user(argp, &layers, sizeof(layers)) ? -EFAULT : 0;
		}
	case FB_LAYER_GET_SIZE:
		{
			if (copy_from_user(&global_layer_id, (void __user *)argp, sizeof(int))) {
				pr_info("[TE_SET_ENABLE]: copy_from_user failed! line:%d\n",
					__LINE__);
				return -EFAULT;
			}
			pr_info("sxk==>global_layer_id is %d\n", global_layer_id);
			int layer_size, enable;

			fbconfig_get_layer_vaddr(global_layer_id, &layer_size, &enable);
			if ((layer_size == 0) || (enable == 0))
				return -2;
			else
				return copy_to_user(argp, &layer_size,
						    sizeof(layer_size)) ? -EFAULT : 0;
		}
	case FB_LAYER_DUMP:
		{
			int layer_size, enable;
			int ret = 0;
			unsigned int kva = 0;
			unsigned int mapped_size = 0;
			unsigned int mva =
			    fbconfig_get_layer_vaddr(global_layer_id, &layer_size, &enable);

			if ((layer_size != 0) && (enable != 0)) {
				pr_info("sxk==>FB_LAYER_DUMP==>layer_size is %d   mva is 0x%x\n",
					layer_size, mva);

				m4u_mva_map_kernel(mva, layer_size, 0, &kva, &mapped_size);
				pr_info("sxk==> addr from user space is 0x%x\n", argp);
				pr_info("sxk==> kva is 0x%x   mmaped size is %d\n", kva,
					mapped_size);
				ret = copy_to_user(argp, (void *)kva, mapped_size) ? -EFAULT : 0;
				m4u_mva_unmap_kernel(mva, mapped_size, kva);

				return ret;
			} else
				return -2;
		}
	case MIPI_SET_CC:
		{
			int enable;

			if (copy_from_user(&enable, (void __user *)argp, sizeof(enable))) {
				pr_info("[MIPI_SET_CC]: copy_from_user failed! line:%d\n",
					__LINE__);
				return -EFAULT;
			} else {
				if (fbconfig_if_drv->set_continuous_clock)
					fbconfig_if_drv->set_continuous_clock(enable);
			}

			return 0;
		}
	case LCM_GET_ESD:
		{
			ESD_PARA esd_para;
			int i = 0;

			if (copy_from_user(&esd_para, (void __user *)arg, sizeof(esd_para))) {
				pr_info("[LCM_GET_ESD]: copy_from_user failed! line:%d\n",
					__LINE__);
				return -EFAULT;
			}
			esd_check_addr = esd_para.addr;
			esd_check_para_num = esd_para.para_num;

			ret = fbconfig_get_esd_check();
			if ((ret != 0) && (esd_check_buffer != NULL)) {
				kfree(esd_check_buffer);
				return -2;
			} else {
				for (i = 0; i < esd_check_para_num + 6; i++)
					pr_info("sxk=>%s, esd_check_buffer[%d]=0x%x\n", __func__, i,
						esd_check_buffer[i]);

				return 0;
			}
		}
	case LCM_GET_ESD_RET:
		{
			ret =
			    (copy_to_user
			     (argp, (void *)esd_check_buffer,
			      sizeof(char) * (esd_check_para_num + 6)) ? -EFAULT : 0);
			if (esd_check_buffer != NULL) {
				kfree(esd_check_buffer);
				esd_check_buffer = NULL;
			}

			return ret;
		}
	case DRIVER_IC_RESET:
		{
			fbconfig_reset_lcm_setting();
			fbconfig_free_backup_setting();

			return 0;
		}
	default:
		return ret;
	}
}


static int fbconfig_release(struct inode *inode, struct file *file)
{
	if (record_head == NULL)
		return 0;
	else {
		record_list_initialed = 0;
#if 1				/* FBCONFIG_DEBUG */
		print_from_head_to_tail();
		fbconfig_backup_lcm_setting();
		print_from_head_to_tail_backup();

#endif
#if 1
		if (lcm_params->dsi.mode != CMD_MODE) {
			pr_info("sxk=>01will exec cmd!! in fbconfig_release()\n");

			if (down_interruptible(&sem_early_suspend)) {
				pr_info("sxk=>can't get semaphore in execute_cmd()\n");
				return 0;
			}
			fbconfig_dsi_vdo_prepare();	/* video mode */
			pr_info("sxk=>FINISH!!\n");

			up(&sem_early_suspend);
		} else {	/* cmd mode */
			if (down_interruptible(&sem_early_suspend)) {
				pr_info("sxk=>can't get semaphore in execute_cmd()\n");
				return 0;
			}
			fb_config_execute_cmd();
			up(&sem_early_suspend);
		}
#endif
/*free the memory .....*/
		pr_info("sxk=>will free the memory\n");
		free_list_memory();
		return 0;
	}
}


static struct file_operations fbconfig_fops = {
	.read = fbconfig_read,
	.write = fbconfig_write,
	.open = fbconfig_open,
	.unlocked_ioctl = fbconfig_ioctl,
	.release = fbconfig_release,
};


void ConfigPara_Init(void)
{
	ConfigPara_dbgfs =
	    debugfs_create_file("fbconfig", S_IFREG | S_IRUGO, NULL, (void *)0, &fbconfig_fops);
}


void ConfigPara_Deinit(void)
{
	debugfs_remove(ConfigPara_dbgfs);
}
