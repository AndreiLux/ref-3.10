#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/kmod.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/sysctl.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/huawei/rdr.h>
#include "drv_reset.h"
#include "rdr_internal.h"

#ifndef HIFI_SEC_DDR_MAX_NUM
#define HIFI_SEC_DDR_MAX_NUM		(32)
#endif
#define HIFI_SEC_HEAD_ADDR			(0x37DF5000)
#define HIFI_SEC_HEAD_SIZE			(1024)
#define HIFI_BSS_SEC				(2)

struct drv_hifi_sec_info {
	unsigned int    type;
	unsigned int    src_addr;
	unsigned int    des_addr;
	unsigned int    size;
};

struct drv_hifi_sec_ddr_head {
	unsigned int                                    sections_num;
	struct drv_hifi_sec_info                 sections[HIFI_SEC_DDR_MAX_NUM];
};

int reset_hifi_sec(void)
{
	struct drv_hifi_sec_ddr_head	  *head;
	void *sec_head = NULL;
	void *sec_addr = NULL;
	int i;
	int ret = 0;

	sec_head = ioremap(HIFI_SEC_HEAD_ADDR, HIFI_SEC_HEAD_SIZE);
	if (NULL == sec_head) {
		ret = -1;
		goto error;
	}
	head = (struct drv_hifi_sec_ddr_head *)sec_head;

	pr_debug("sections_num = 0x%x\n", head->sections_num);

	for (i = 0; i < head->sections_num; i++) {
		if (head->sections[i].type == HIFI_BSS_SEC) {
			pr_debug("sec_id = %d, type = 0x%x, src_addr = 0x%x, des_addr = 0x%x, size = %d\n",
					i,
					head->sections[i].type,
					head->sections[i].src_addr,
					head->sections[i].des_addr,
					head->sections[i].size);
			sec_addr = ioremap(head->sections[i].des_addr,
							head->sections[i].size);
			if (NULL == sec_addr) {
				ret = -1;
				goto error1;
			}
			memset(sec_addr, 0x0, head->sections[i].size);
			iounmap(sec_addr);
		}
	}

error1:
	iounmap(sec_head);
error:
	return ret;
}

static char *shcmd = "/system/bin/sh";
static char *killcmd = "/system/bin/kill";
static char *cmd_envp[] = { "HOME=/data", "TERM=vt100", "USER=root",
	"PATH=/system/xbin:/system/bin", NULL };

#define MAX_PID_LEN			(10)
#define PID_FILE_MAXLEN		(512)
#define PID_FILE_NAME		"/data/mediaserver_pid.dat"
#define PID_SH_CMD			"/system/etc/mediaserver_pid.sh"
int get_mediaserver_pid(char *media_pid)
{
	int ret = -EINVAL;
	mm_segment_t old_fs;
	struct file *file;
	loff_t pos = 0;
	char statline[PID_FILE_MAXLEN];
	char *cur, *token;
	int pid_len = 0;

	file = filp_open(PID_FILE_NAME, O_RDONLY, 0600);
	if (IS_ERR(file)) {
		pr_err("filp_open(%s) failed\n", PID_FILE_NAME);
		ret = PTR_ERR(file);
		return ret;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret = vfs_read(file, statline, PID_FILE_MAXLEN-1, &pos);
	set_fs(old_fs);
	filp_close(file, NULL);
	if (ret < 0) {
		pr_err("read_file failed\n");
		return ret;
	}

	statline[ret] = 0;
	cur = statline;
	pr_info("read from .dat[%s]\n", statline);

	while (token = strsep(&cur, " \n")) {
		if (strncmp(token, "media", strlen("media")) == 0) {
			while (token = strsep(&cur, " ")) {
				if (token[0] != 0)
					break;
			}

			if (token != NULL) {
				pid_len = (strlen(token) > MAX_PID_LEN ?
						MAX_PID_LEN : strlen(token));
				strncpy(media_pid, token, pid_len);
				pr_info("media_pid is %s\n", media_pid);
				ret = 0;
			} else {
				pr_err("get mdeia pid fail pid is null\n");
				ret = -1;
			}
			break;
		}
	}
	if (token == NULL) {
		pr_err("get mdeia pid fail pid is null\n");
		ret = -1;
	}

	return ret;
}

int reset_mediaserver(void)
{
	char media_pid[MAX_PID_LEN] = {'0'};
	char *cmd_argv[] = {shcmd, NULL, NULL};
	int result = 0, num = 0;

	cmd_argv[0] = shcmd;
	cmd_argv[1] = PID_SH_CMD;
	do {
		result = call_usermodehelper(shcmd, cmd_argv, cmd_envp,
				UMH_WAIT_PROC);
		num++;
		if (num > 300)
			break;
	} while (result == -EBUSY);
	if (result < 0) {
		pr_err("%s : get media server pis fail %d\n",
				__func__ , result);
		return result;
	}

	result = get_mediaserver_pid(media_pid);
	if (result < 0) {
		pr_err("%s : analyze media server pid fail %d\n",
				__func__ , result);
		return -1;
	}

	num = 0;
	cmd_argv[0] = killcmd;
	cmd_argv[1] = media_pid;
	do {
		result = call_usermodehelper(killcmd, cmd_argv, cmd_envp,
				UMH_WAIT_PROC);
		num++;
		if (num > 300)
			break;
	} while (result == -EBUSY);
	if (result < 0)
		pr_err("%s : reset mediaserver fail %d\n",  __func__ , result);

	return 0;
}
