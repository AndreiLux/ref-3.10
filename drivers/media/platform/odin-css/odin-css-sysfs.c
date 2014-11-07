/*
 * Copyright (C) 2013 Mtekvision
 * Author: Daeheon Kim <kimdh@mtekvision.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include "odin-css.h"
#include "odin-capture.h"
#include "odin-css-utils.h"
#include "odin-css-clk.h"

#define MAX_TOKEN_STR_LENGTH	256
#define MAX_TOKEN				10

#define NUM_TO_MIN				0x00000000
#define NUM_TO_MAX				0xffffffff

/* Token List */
struct odin_token_list {
	char token[MAX_TOKEN_STR_LENGTH];
};

/* Test Scenario List */
struct odin_scenario {
	char *command;
	char *description;
	int (*execute)(struct odin_token_list *token_list, u32 n_token);
	int (*help)(void);
};

static int show_help(struct odin_token_list *token_list, u32 n_token);
static unsigned char odin_get_tokens(char *pString, char *delimiters);
static size_t odin_strcmp(const char *token, const char *string);
static ssize_t css_show(struct device *dev, struct device_attribute *attr,
				char *buf);
static ssize_t css_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t n);

static struct odin_token_list token_list[MAX_TOKEN];
static struct kobject *odin_css;

static char *odin_token;

const char src_img0[] = {
	0,
	/* #include "./test/yuv422_vga_image0.txt" */
};

const char src_img1[] = {
	0,
	/* #include "./test/yuv422_vga_image1.txt" */
};

/* ----------------- TEST FUNCTIONS IN HERE ------------------------- */
static int ex_func(struct odin_token_list *token_list, u32 n_token);


static int css_pll_ctrl(struct odin_token_list *token_list, u32 n_token);

static struct odin_scenario scenario[] = {
	{"show"	,"show commands"				,show_help		, NULL},
	{"test" ,"test function for example"	,ex_func		, NULL},
	{"cpll" ,"test function for css pll"	,css_pll_ctrl	, NULL},
};

static int show_help(struct odin_token_list *token_list, u32 n_token)
{
	u32 n_cnt = 0;
	printk("                                                 \n");
	printk("  =============================================  \n");
	printk("    CSS verification commands                    \n");
	printk("  =============================================  \n");
	printk("    You can make flexible test cases using this  \n");
	printk("    sysfs command.                               \n");
	printk("    If you want to know defined test commands,   \n");
	printk("    type below command.                          \n");
	printk("    # echo show > /sys/css/odin_css/css     \n");
	printk("    \n");
	printk("    If you want to know how to use each test     \n");
	printk("    commands, type below command for example     \n");
	printk("    # echo s3dc help > /sys/css/odin_css/css\n");
	printk("                                                 \n");
	printk("    [commands]          [description]            \n");
	printk("  =============================================  \n");

	for (n_cnt = 0; n_cnt < (sizeof(scenario) / sizeof(struct odin_scenario));
		n_cnt++) {
		if (scenario[n_cnt].description != NULL) {
			printk("     %s \t %s\n", scenario[n_cnt].command,
										scenario[n_cnt].description);
		}
	}

	return 0;
}

static int ex_func(struct odin_token_list *token_list, u32 n_token)
{
	int i = 0;

	printk("total token num is %d\n", n_token);

	for (i = 0; i < n_token; i++)
		printk("token[%d] = %s\n", i, token_list[i].token);

	for (i = 0; i < n_token; i++) {
		char *str_token = token_list[i].token;

		if ((str_token[0] >='0' && str_token[0] <= '9') &&
			(str_token[1] != 'x' && str_token[1] != 'X')) {
			printk("token[%d] is numeric = %d\n", i,
					css_atoi(token_list[i].token));
		}
	}

	for (i = 0; i < n_token; i++) {
		char *str_token = token_list[i].token;

		if (str_token[0] == '0' &&
			(str_token[1] == 'x' || str_token[1] == 'X')) {
			unsigned long hex_num = 0;
			hex_num = simple_strtoul(str_token, NULL, 16);
			printk("token[%d] is hex = 0x%08X\n", i, (unsigned int)hex_num);
		}
	}

	return 0;
}

static int css_pll_ctrl(struct odin_token_list *token_list, u32 n_token)
{
	struct css_device *cssdev = NULL;
	size_t isp_khz = 0;
	int cur_isp_khz = 0;

	cssdev = get_css_plat();

	isp_khz = css_atoi(token_list[1].token);

	capture_wait_scaler_vsync(cssdev, 0, 100);

	css_set_isp_clock_khz(isp_khz);

	return 0;
}

/* Passses buf to user level */
static ssize_t css_show(dev, attr, buf)
						struct device *dev;
						struct device_attribute *attr;
						char *buf;
{
	const char *msg_str = "type : \"echo show > /sys/css/odin_css/css\"";

	return sprintf(buf, "%s\n", msg_str);
}


/* Get data from user level */
static ssize_t css_store(dev, attr, buf, n)
						struct device *dev;
						struct device_attribute *attr;
						const char *buf;
						size_t n;
{
	char *str_cmd = (char*)buf;
	char delimiters[] = " .,;:!-";
	u32 n_cnt = 0;
	u32 n_token = 0;
	int ret = 0;

	n_token = odin_get_tokens(str_cmd, delimiters);

	/* TODO: */
	for (n_cnt = 0;	n_cnt < (sizeof(scenario) / sizeof(struct odin_scenario));
		n_cnt++) {
		if (odin_strcmp(token_list[0].token, scenario[n_cnt].command)) {

			if (odin_strcmp(token_list[1].token, "help")) {
				if (scenario[n_cnt].help != NULL)
					scenario[n_cnt].help();
				else
					printk(KERN_INFO "help : any information!!\n");

				return n;
			}

			if (scenario[n_cnt].execute != NULL) {
				ret = scenario[n_cnt].execute(token_list, n_token);
				if (ret < 0)
					printk(KERN_INFO "command [%s] : fail %d!!\n",
							scenario[n_cnt].command, ret);
			} else {
				printk(KERN_INFO "command [%s] : func is not registered!!\n",
					scenario[n_cnt].command);
			}
			break;
		}
	}

	return n;
}

static unsigned char odin_get_tokens(char *pString, char *delimiters)
{
	unsigned int i = 0;

	odin_token = css_strtok(pString, delimiters);
	while (odin_token != NULL) {
		int j = 0;
		/* While there are tokens in "string" */

		sprintf(token_list[i].token ,"%s", odin_token);
		for (j=0; j < MAX_TOKEN_STR_LENGTH; j++) {
			if (token_list[i].token[j] == 0xA) {
				token_list[i].token[j] = 0x0;
			}
		}
		i++;
		/* Get next token: */
		odin_token = css_strtok(NULL, delimiters);
	}

	return i;
}

static size_t odin_strcmp(const char *string1, const char *string2)
{
	const char *p_string1 = string1;
	const char *p_string2 = string2;

	for (; *p_string2 != '\0'; p_string2++, p_string1++) {
		if (*p_string1 != *p_string2)
			return 0;
	}

	return 1;
}

static DEVICE_ATTR(css, 0644, css_show, css_store);

static struct attribute *odin_css_attrs[] = {
	&dev_attr_css.attr,
	NULL,
};

static struct attribute_group odin_css_attr_group = {
	.name = "odin-css",
	.attrs = odin_css_attrs,
};

int __init odin_css_sysfs_init(void)
{
	int ret;

	odin_css = kobject_create_and_add("css", NULL);
	if (odin_css == NULL)
		return -ENOMEM;

	ret = sysfs_create_group(odin_css, &odin_css_attr_group);
	if (ret) {
		kobject_put(odin_css);

		return ret;
	}

	return 0;
}

void __exit odin_css_sysfs_exit(void)
{
	kobject_put(odin_css);
	sysfs_remove_group(odin_css, &odin_css_attr_group);
}

late_initcall(odin_css_sysfs_init);
module_exit(odin_css_sysfs_exit);
MODULE_DESCRIPTION("odin camera verification driver");
MODULE_AUTHOR("MTEKVISION<http://www.mtekvision.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

