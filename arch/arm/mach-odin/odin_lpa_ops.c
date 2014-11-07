/* linux/arch/arm/mach-odin/
 *
 * Copyright (c) 2013 LG Electronics
 *
 * Odin Power Domain Control support.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/odin_mailbox.h>
#include <linux/odin_lpa.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/odin_dfs.h>
#include <linux/devfreq.h>
#include <linux/spinlock.h>
#include <linux/cpuidle.h>

static DEFINE_SPINLOCK(events_lock);

static int lpa_state = 0;

static long int memc_clk_buf = 0;
static long int cclk_buf = 0;
static long int scfg_buf = 0;
static long int gclk_buf = 0;
static long int cluster1_buf = 0;

static BLOCKING_NOTIFIER_HEAD(lpa_notifier_list);

int lpa_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&lpa_notifier_list, nb);

}
EXPORT_SYMBOL(lpa_register_client);

int lpa_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&lpa_notifier_list, nb);
}
EXPORT_SYMBOL(lpa_unregister_client);

static int lpa_notifier_call_chain(unsigned long val)
{
	return blocking_notifier_call_chain(&lpa_notifier_list, val, NULL);
}



int odin_lpa_start(unsigned int sub_block, unsigned int id)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (LPA << LPA_OFFSET) |
		( (sub_block & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) | (LPA_START);
	mb_data[2] = id;
	ret = ipc_call_fast(mb_data);
	/*	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_LPA, mb_data, 1000); */
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}

	return ret;
}

int odin_lpa_stop(unsigned int sub_block, unsigned int id)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (LPA << LPA_OFFSET) |
		( (sub_block & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) | (LPA_STOP);
	mb_data[2] = id;
	ret = ipc_call_fast(mb_data);
	/*ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_LPA, mb_data, 1000);*/
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}

	return ret;
}

int odin_lpa_write_done(unsigned int sub_block, unsigned int id)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (LPA << LPA_OFFSET) |
		( (sub_block & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) | (LPA_WRITE_DONE);
	mb_data[2] = id;
	ret = ipc_call_fast(mb_data);
	/*ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_LPA, mb_data, 1000);*/
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}

	return ret;
}

/* SMS_LPA control */
int odin_sms_lpa_write(unsigned int sub_block, unsigned int address,
		unsigned int size)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (LPA << LPA_OFFSET) |
		( (sub_block & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) | (SMS_LPA_WRITE);
	mb_data[2] = address;
	mb_data[3] = size;
	ret = ipc_call_fast(mb_data);
	/*	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_LPA, mb_data, 1000); */
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}
	return ret;
}

int odin_sms_lpa_start(unsigned int sub_block)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (LPA << LPA_OFFSET) |
		( (sub_block & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) | (SMS_LPA_START);
	ret = ipc_call_fast(mb_data);
	/*	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_LPA, mb_data, 1000); */
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}
	return ret;
}

int odin_sms_lpa_stop(unsigned int sub_block)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (LPA << LPA_OFFSET) |
		( (sub_block & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) | (SMS_LPA_STOP);
	ret = ipc_call_fast(mb_data);
	/*	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_LPA, mb_data, 1000);*/
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}
	return ret;
}

int odin_sms_lpa_resume(unsigned int sub_block)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (LPA << LPA_OFFSET) |
		( (sub_block & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) | (SMS_LPA_RESUME);
	ret = ipc_call_fast(mb_data);
	/*	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_LPA, mb_data, 1000);*/
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}
	return ret;
}

int odin_sms_lpa_pause(unsigned int sub_block)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (LPA << LPA_OFFSET) |
		( (sub_block & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) | (SMS_LPA_PAUSE);
	ret = ipc_call_fast(mb_data);
	/*	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_LPA, mb_data, 1000);*/
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}
	return ret;
}

int lpa_enable(int lpa_ipc)
{
	struct clk *clk;
	struct cpuidle_device *idle_cpu;
	int i;

#ifdef CONFIG_PM_DEVFREQ
	set_devfreq_governor("userspace");
	printk("odin-dfs-disable\n");
#endif
	cpuidle_pause();
	/*idle_cpu = odin_get_idle_device(0);*/
	/*cpuidle_disable_device(idle_cpu);*/

	cpu_hotplug_driver_lock();
	for (i=1; i<8; i++){
		cpu_down(i);
	}
	cpu_hotplug_driver_unlock();

	mdelay(100);
	store_set_governor(0,"userspace");
	mdelay(100);

	clk = clk_get(NULL, "memc_clk");
	clk_set_rate(clk, 200000000);
	clk = clk_get(NULL, "cclk");
	clk_set_rate(clk, 100000000);
	clk = clk_get(NULL, "gclk");
	clk_set_rate(clk, 50000000);
	clk = clk_get(NULL, "scfg");
	clk_set_rate(clk, 50000000);
	clk = clk_get(NULL, "cluster1");
	clk_set_rate(clk, 800000000);

	cpuidle_resume();

	if (lpa_ipc == 1)
	{
		odin_lpa_start(3,0);
	}

	/*cpuidle_enable_device(idle_cpu);*/

	lpa_state = 1;
	return 0;
}

int lpa_disable()
{
	struct clk *clk;
	struct cpuidle_device *idle_cpu;
	unsigned long flags;
	int i;

	cpuidle_pause();
	odin_lpa_stop(3,0);

	cpu_hotplug_driver_lock();
	for (i=1; i<8; i++){
		cpu_up(i);
	}
	cpu_hotplug_driver_unlock();

	clk = clk_get(NULL, "memc_clk");
	clk_set_rate(clk, 800000000);
	clk = clk_get(NULL, "cclk");
	clk_set_rate(clk, 400000000);
	clk = clk_get(NULL, "gclk");
	clk_set_rate(clk, 200000000);
	clk = clk_get(NULL, "scfg");
	clk_set_rate(clk, 200000000);

	/*cpuidle_resume();*/
#ifdef CONFIG_PM_DEVFREQ
	set_devfreq_governor("simple_ondemand");
#endif
	store_set_governor(0,"interactive");
	lpa_state = 0;
	return 0;
}


int get_lpa_state()
{
	return lpa_state;
}

int odin_sms_lpa_dma_start(unsigned int sub_block)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (LPA << LPA_OFFSET) |
		( (sub_block & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) | (SMS_LPA_DMA_START);
	ret = ipc_call_fast(mb_data);
	/*	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_LPA, mb_data, 1000);*/
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}
	return ret;
}

int odin_sms_lpa_pcm_decoding_start(unsigned int sub_block)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (LPA << LPA_OFFSET) |
		( (sub_block & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) |
		(SMS_LPA_PCM_DECODING_START);
	ret = ipc_call_fast(mb_data);
	/*	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_LPA, mb_data, 1000);*/
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}
	return ret;
}

int odin_sms_lpa_pcm_decoding_end(unsigned int sub_block)
{
	int ret;
	int mb_data[7] = {0, };

	mb_data[0] = PM_CMD_TYPE;
	mb_data[1] = (LPA << LPA_OFFSET) |
		( (sub_block & SUB_BLOCK_MASK) << SUB_BLOCK_OFFSET ) |
		(SMS_LPA_PCM_DECODING_END);
	ret = ipc_call_fast(mb_data);
	/*	ret = mailbox_call(FAST_IPC_CALL, MB_COMPLETION_LPA, mb_data, 1000);*/
	if (ret < 0) {
		pr_err("%s: failed to send mail (%d)\n", __func__, ret);
	}
	return ret;
}
