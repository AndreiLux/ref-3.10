/*
 * Copyright (C) 2013 HTC Corporation.  All rights reserved.
 *
 *      @file   /kernel/drivers/htc_debug/stability/reboot_params.c
 *
 * This software is distributed under dual licensing. These include
 * the GNU General Public License version 2 and a commercial
 * license of HTC.  HTC reserves the right to change the license
 * of future releases.
 *
 * Unless you and HTC execute a separate written software license
 * agreement governing use of this software, this software is licensed
 * to you under the terms of the GNU General Public License version 2,
 * available at {link to GPL license term} (the "GPL").
 */

#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/reboot.h>
#include <linux/of_fdt.h>
#include <linux/sched.h>

#include <linux/kdebug.h>
#include <linux/notifier.h>
#include <linux/kallsyms.h>

#include <asm/io.h>

/* These constants are used in bootloader to decide actions. */
#define RESTART_REASON_BOOT_BASE	(0x77665500)
#define RESTART_REASON_BOOTLOADER	(RESTART_REASON_BOOT_BASE | 0x00)
#define RESTART_REASON_REBOOT		(RESTART_REASON_BOOT_BASE | 0x01)
#define RESTART_REASON_RECOVERY		(RESTART_REASON_BOOT_BASE | 0x02)
#define RESTART_REASON_HTCBL		(RESTART_REASON_BOOT_BASE | 0x03)
#define RESTART_REASON_OFFMODE		(RESTART_REASON_BOOT_BASE | 0x04)
#define RESTART_REASON_RAMDUMP		(RESTART_REASON_BOOT_BASE | 0xAA)
#define RESTART_REASON_HARDWARE		(RESTART_REASON_BOOT_BASE | 0xA0)
#define RESTART_REASON_POWEROFF		(RESTART_REASON_BOOT_BASE | 0xBB)

/*
 * The RESTART_REASON_OEM_BASE is used for oem commands.
 * The actual value is parsed from reboot commands.
 * RIL FATAL will use oem-99 to restart a device.
 */
#define RESTART_REASON_OEM_BASE		(0x6f656d00)
#define RESTART_REASON_RIL_FATAL	(RESTART_REASON_OEM_BASE | 0x99)

#define SZ_DIAG_ERR_MSG (128)

struct htc_reboot_params {
	u32 reboot_reason;
	u32 radio_flag;
	u32 battery_level;
	char msg[SZ_DIAG_ERR_MSG];
	char reserved[0];
};
static struct htc_reboot_params* reboot_params = NULL;

static void set_restart_reason(u32 reason)
{
	reboot_params->reboot_reason = reason;
}

static void set_restart_msg(const char *msg)
{
	char* buf;
	size_t buf_len;
	if (unlikely(!msg)) {
		WARN(1, "%s: argument msg is NULL\n", __func__);
		msg = "";
	}

	buf = reboot_params->msg;
	buf_len = sizeof(reboot_params->msg);

	pr_debug("copy buffer from %pK (%s) to %pK for %zu bytes\n",
			msg, msg, buf, min(strlen(msg), buf_len - 1));
	snprintf(buf, buf_len, "%s", msg);
}

static struct cmd_reason_map {
	char* cmd;
	u32 reason;
} cmd_reason_map[] = {
	{ .cmd = "",           .reason = RESTART_REASON_REBOOT },
	{ .cmd = "bootloader", .reason = RESTART_REASON_BOOTLOADER },
	{ .cmd = "recovery",   .reason = RESTART_REASON_RECOVERY },
	{ .cmd = "offmode",    .reason = RESTART_REASON_OFFMODE },
	{ .cmd = "poweroff",   .reason = RESTART_REASON_POWEROFF },
	{ .cmd = "force-hard", .reason = RESTART_REASON_RAMDUMP },
};

#define OEM_CMD_FMT "oem-%02x"

static void set_restart_command(const char* command)
{
	int code;
	int i;

	if (unlikely(!command)) {
		WARN(1, "%s: command is NULL\n", __func__);
		command = "";
	}

	/* standard reboot command */
	for (i = 0; i < ARRAY_SIZE(cmd_reason_map); i++)
		if (!strcmp(command, cmd_reason_map[i].cmd)) {
			set_restart_msg(cmd_reason_map[i].cmd);
			set_restart_reason(cmd_reason_map[i].reason);
			return;
		}

	/* oem reboot command */
	if (1 == sscanf(command, OEM_CMD_FMT, &code)) {
		/* oem-97, 98, 99 are RIL fatal */
		if ((code == 0x97) || (code == 0x98))
			code = 0x99;

		set_restart_msg(command);
		set_restart_reason(RESTART_REASON_OEM_BASE | code);
		return;
	}

	/* unknown reboot command */
	pr_warn("Unknown restart command: %s\n", command);
	set_restart_msg("");
	set_restart_reason(RESTART_REASON_REBOOT);
}

static int reboot_callback(struct notifier_block *nb,
		unsigned long event, void *data)
{
	/*
	 * NOTE: `data' is NULL when reboot w/o command or shutdown
	 */
	char* cmd;

	cmd = (char*) (data ? data : "");
	pr_debug("restart command: %s\n", data ? cmd : "<null>");

	switch (event) {
	case SYS_RESTART:
		set_restart_command(cmd);
		pr_info("syscall: reboot - current task: %s (%d:%d)\n",
				current->comm, current->tgid, current->pid);
		dump_stack();
		break;
	case SYS_HALT:
	case SYS_POWER_OFF:
	default:
		/*
		 * - Clear reboot_params to prevent unnessary RAM issue.
		 * - Set it to 'offmode' instead of 'poweroff' since
		 *   it is required to make device enter offmode charging
		 *   if cable attached
		 */
		set_restart_command("offmode");
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block reboot_notifier = {
	.notifier_call = reboot_callback,
};

static struct die_args *tombstone = NULL;

int die_notify(struct notifier_block *self,
				       unsigned long val, void *data)
{
	static struct die_args args;
	memcpy(&args, data, sizeof(args));
	tombstone = &args;
	pr_debug("saving oops: %p\n", (void*) tombstone);
	return NOTIFY_DONE;
}

static struct notifier_block die_nb = {
	.notifier_call = die_notify,
};

static int panic_event(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	char msg_buf[SZ_DIAG_ERR_MSG];

	if (tombstone) { // tamper the panic message for Oops
		char pc_symn[KSYM_NAME_LEN] = "<unknown>";
		char lr_symn[KSYM_NAME_LEN] = "<unknown>";

#if defined(CONFIG_ARM)
		sprint_symbol(pc_symn, tombstone->regs->ARM_pc);
		sprint_symbol(lr_symn, tombstone->regs->ARM_lr);
#elif defined(CONFIG_ARM64)
		sprint_symbol(pc_symn, tombstone->regs->pc);
		sprint_symbol(lr_symn, tombstone->regs->regs[30]);
#endif

		snprintf(msg_buf, sizeof(msg_buf),
				"KP: %s PC:%s LR:%s",
				current->comm,
				pc_symn, lr_symn);
	} else {
		snprintf(msg_buf, sizeof(msg_buf),
				"KP: %s", (const char*) ptr);
	}
	set_restart_msg((const char*) msg_buf);

	return NOTIFY_DONE;
}

static struct notifier_block panic_block = {
	.notifier_call	= panic_event,
};

static int htc_reboot_params_probe(struct platform_device *pdev)
{
	struct resource *param;

	param = platform_get_resource_byname(pdev, IORESOURCE_MEM, "reboot_params");
	if (param && resource_size(param) >= sizeof(struct htc_reboot_params)) {
		reboot_params = ioremap_nocache(param->start, resource_size(param));
		if (reboot_params) {
			dev_info(&pdev->dev, "got reboot_params buffer at %p\n", reboot_params);

			/*
			 * reboot_params is initialized by bootloader, so it's ready to
			 * register the reboot / die / panic handlers.
			 */
			register_reboot_notifier(&reboot_notifier);
			register_die_notifier(&die_nb);
			atomic_notifier_chain_register(&panic_notifier_list, &panic_block);

		} else
			dev_warn(&pdev->dev,
					"failed to map resource `reboot_params': %pR\n", param);
	}

	return 0;
}

#define MODULE_NAME "htc_reboot_params"
static struct of_device_id htc_reboot_params_dt_match_table[] = {
	{
		.compatible = MODULE_NAME
	},
	{},
};

static struct platform_driver htc_reboot_params_driver = {
	.driver		= {
		.name	= MODULE_NAME,
		.of_match_table = htc_reboot_params_dt_match_table,
	},
	.probe = htc_reboot_params_probe,
};

static int __init htc_reboot_params_init(void)
{
	return platform_driver_register(&htc_reboot_params_driver);
}
core_initcall(htc_reboot_params_init);
