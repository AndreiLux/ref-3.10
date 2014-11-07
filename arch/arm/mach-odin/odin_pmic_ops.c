/* linux/arch/arm/mach-odin/odin_dvs.c
 *
 * Copyright (c) 2013 LG Electronics
 *
 * Odin PMIC Control support.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/odin_mailbox.h>
#include <linux/odin_pmic.h>

int board_rev 	= 0;
int boot_reason = 0;

int odin_pmic_source_disable(unsigned int pmic_source)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (PMIC << PMIC_OFFSET) | (PMIC_DISABLE << SUB_CMD_OFFSET);
	mb_data[2] = pmic_source;
	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_PMIC, mb_data, 1000);
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}

	return ret;
}

int odin_pmic_source_enable(unsigned int pmic_source)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (PMIC << PMIC_OFFSET) | (PMIC_ENABLE << SUB_CMD_OFFSET);
	mb_data[2] = pmic_source;
	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_PMIC, mb_data, 1000);
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}

	return ret;
}

int odin_pmic_source_set(unsigned int pmic_source, unsigned int milli_voltage)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (PMIC << PMIC_OFFSET) | (PMIC_SET << SUB_CMD_OFFSET);
	mb_data[2] = pmic_source;
	mb_data[3] = milli_voltage * 1000;
	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_PMIC, mb_data, 1000);
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}

	return ret;
}

int odin_pmic_source_get(unsigned int pmic_source)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (PMIC << PMIC_OFFSET) | (PMIC_GET << SUB_CMD_OFFSET);
	mb_data[2] = pmic_source;
	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_PMIC, mb_data, 1000);
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
		goto out;
	}
	ret = mb_data[2]/1000;
out:
	return ret;
}

int odin_pmic_revision_get(void)
{
	int ret;
	int mb_data[7] = {0, };

	if (0 == board_rev){
		mb_data[0] = PM_CMD_TYPE;
		mb_data[1] = (PMIC << PMIC_OFFSET) | (PMIC_REV_GET << SUB_CMD_OFFSET);
		ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_PMIC, mb_data, 1000);
		if (ret < 0) {
			pr_err("%s: failed to send mail (%d)\n", __func__, ret);
			goto out;
		}

		board_rev = mb_data[2];
	}
out:
	return board_rev;
}

#if defined(CONFIG_MACH_ODIN_LIGER)
unsigned int odin_pmic_getRegister(unsigned int nRegister)
{
	int ret;
	unsigned int value = 0x00;
	unsigned int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (PMIC_REGISTER << PMIC_OFFSET) | (PMIC_REGISTER_GET << SUB_CMD_OFFSET);
	mb_data[2] = nRegister;
	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_CLK, mb_data, 1000);
	if (ret < 0) {
		printk("%s: failed to send mail (%d)\n", __func__, ret);
		goto out;
	}

	value = mb_data[2];
out:
	return value;
}

int odin_pmic_setRegister(unsigned int nRegister, unsigned int nValue)
{
	int ret;
	unsigned int value;
	unsigned int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (PMIC_REGISTER << PMIC_OFFSET) | (PMIC_REGISTER_SET << SUB_CMD_OFFSET);
	mb_data[2] = nRegister;
	mb_data[3] = nValue;
	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_CLK, mb_data, 1000);
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
		goto out;
	}

	value = mb_data[2];
out:
	return ret;
}
#endif

int odin_pmic_bootreason_get(void)
{
	int ret;
	int mb_data[7] = {0, };

	if (0 == boot_reason){
		mb_data[0] = PM_CMD_TYPE;
		mb_data[1] = (PMIC << PMIC_OFFSET) | (PMIC_BOOT_REASON << SUB_CMD_OFFSET);
		ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_PMIC, mb_data, 1000);
		if (ret < 0) {
			pr_err("%s: failed to send mail (%d)\n", __func__, ret);
			goto out;
		}

		boot_reason = mb_data[2];
	}
out:
	return boot_reason;
}
