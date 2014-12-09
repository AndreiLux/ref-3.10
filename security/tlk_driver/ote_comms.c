/*
 * Copyright (c) 2012-2015 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/atomic.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/printk.h>
#include <linux/ioctl.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#ifdef CONFIG_TRUSTY
#include <linux/trusty/trusty.h>
#endif
#include <linux/completion.h>
#include <linux/se_access.h>
#include <asm/smp_plat.h>

#include "ote_protocol.h"

bool verbose_smc;
core_param(verbose_smc, verbose_smc, bool, 0644);

struct tlk_info {
	struct device *trusty_dev;
	atomic_t smc_count;
	struct completion smc_retry;
	bool smc_retry_timed_out;
	struct notifier_block smc_notifier;
};

static struct tlk_info *tlk_info;

#define SET_RESULT(req, r, ro)	{ req->result = r; req->result_origin = ro; }

static int te_pin_user_pages(void *buffer, size_t size,
			struct page **pages, unsigned long nrpages,
			uint32_t buf_type)
{
	int ret;
	int nrpages_pinned;
	int i;
	bool write;

	write = (buf_type == TE_PARAM_TYPE_MEM_RW ||
		buf_type == TE_PARAM_TYPE_PERSIST_MEM_RW);

	nrpages_pinned = get_user_pages_fast((unsigned long)buffer,
					     nrpages, write, pages);

	if (nrpages_pinned != nrpages)
		goto err_pin_pages;

	return OTE_SUCCESS;

err_pin_pages:
	for (i = 0; i < nrpages_pinned; i++)
		put_page(pages[i]);
	ret = OTE_ERROR_BAD_PARAMETERS;

	return ret;
}

#ifdef CONFIG_TRUSTY
static void te_unpin_user_pages(struct page **pages,
				unsigned long nrpages)
{
	unsigned long i;

	for (i = 0; i < nrpages; i++)
		put_page(pages[i]);
}

static int te_prep_page_list(unsigned long start,
			     unsigned long nrpages,
			     struct page **pages,
			     struct tlk_context *context)
{
	size_t cb;
	unsigned long i;
	struct te_oper_param_page_info *pg_inf;
	struct tlk_device *dev = context->dev;

	BUG_ON(!dev->param_pages);

	/* check if we have enough room to fit this buffer */
	cb = nrpages * sizeof(struct te_oper_param_page_info);
	if ((dev->param_pages_tail + cb) > dev->param_pages_size) {
		pr_err("%s: no space to build page list\n", __func__);
		return OTE_ERROR_EXCESS_DATA;
	}

	pg_inf = (struct te_oper_param_page_info *)
			((uintptr_t)dev->param_pages + dev->param_pages_tail);
	for (i = 0; i < nrpages; i++, pg_inf++, start += PAGE_SIZE) {
		if (te_fill_page_info(pg_inf, start, pages[i])) {
			pr_err("%s: failed to fill page info for addr 0x%p\n",
			       __func__, (void *)start);
			return OTE_ERROR_ACCESS_DENIED;
		}
	}

	dev->param_pages_tail += (uint32_t)cb;

	return OTE_SUCCESS;
}
#endif

static int te_prep_mem_buffer(uint32_t session_id,
		void *buffer, size_t size, uint32_t buf_type,
		struct tlk_context *context,
		bool need_page_list)
{
	struct te_shmem_desc *shmem_desc = NULL;
	int ret = 0;
	unsigned long nrpages;

	nrpages = (PAGE_ALIGN((uintptr_t)buffer + size) -
		round_down((uintptr_t)buffer, PAGE_SIZE)) /
		PAGE_SIZE;

	shmem_desc = kzalloc(sizeof(struct te_shmem_desc) +
			nrpages * sizeof(struct page *), GFP_KERNEL);
	if (!shmem_desc) {
		pr_err("%s: te_add_shmem_desc failed\n", __func__);
		ret = OTE_ERROR_OUT_OF_MEMORY;
		goto err_shmem;
	}

	/* pin pages */
	ret = te_pin_user_pages(buffer, size, shmem_desc->pages,
			      nrpages, buf_type);
	if (ret != OTE_SUCCESS) {
		pr_err("%s: te_pin_user_pages failed (%d)\n",
		       __func__, ret);
		goto err_pin_pages;
	}

#ifdef CONFIG_TRUSTY
	if (need_page_list) {
		/* build page list */
		ret = te_prep_page_list((unsigned long)buffer, nrpages,
					shmem_desc->pages,
					context);
		if (ret != OTE_SUCCESS) {
			pr_err("%s: te_prep_page_list failed (%d)\n",
			       __func__, ret);
			goto err_build_page_list;
		}
	}
#endif

	/* initialize rest of shmem descriptor */
	INIT_LIST_HEAD(&(shmem_desc->list));
	shmem_desc->buffer = buffer;
	shmem_desc->size = size;
	shmem_desc->nrpages = nrpages;

	/* add shmem descriptor to proper list */
	if ((buf_type == TE_PARAM_TYPE_MEM_RO) ||
	    (buf_type == TE_PARAM_TYPE_MEM_RW))
		list_add_tail(&shmem_desc->list, &context->temp_shmem_list);
	else {
		list_add_tail(&shmem_desc->list,
			      &context->temp_persist_shmem_list);
	}

	return OTE_SUCCESS;

#ifdef CONFIG_TRUSTY
err_build_page_list:
	te_unpin_user_pages(shmem_desc->pages, nrpages);
#endif
err_pin_pages:
	kfree(shmem_desc);
err_shmem:
	return ret;
}

static int te_prep_mem_buffers(struct te_request *request,
			struct tlk_context *context)
{
	uint32_t i;
	int ret = OTE_SUCCESS;
	struct te_oper_param *params = request->params;

	for (i = 0; i < request->params_size; i++) {
		switch (params[i].type) {
		case TE_PARAM_TYPE_NONE:
		case TE_PARAM_TYPE_INT_RO:
		case TE_PARAM_TYPE_INT_RW:
			break;
		case TE_PARAM_TYPE_MEM_RO:
		case TE_PARAM_TYPE_MEM_RW:
		case TE_PARAM_TYPE_PERSIST_MEM_RO:
		case TE_PARAM_TYPE_PERSIST_MEM_RW:
			ret = te_prep_mem_buffer(request->session_id,
				params[i].u.Mem.base,
				params[i].u.Mem.len,
				params[i].type,
				context,
				false);
			if (ret < 0) {
				pr_err("%s failed with err (%d)\n",
					__func__, ret);
				ret = OTE_ERROR_BAD_PARAMETERS;
				break;
			}
			break;
		default:
			pr_err("%s: OTE_ERROR_BAD_PARAMETERS\n", __func__);
			ret = OTE_ERROR_BAD_PARAMETERS;
			break;
		}
	}
	return ret;
}

static int te_prep_mem_buffers_compat(struct te_request_compat *request,
			struct tlk_context *context)
{
	uint32_t i;
	int ret = OTE_SUCCESS;
	struct te_oper_param_compat *params;
	struct te_oper_param_page_info *pages = context->dev->param_pages;

	/* reset page_info allocator */
	context->dev->param_pages_tail = 0;

	params = (struct te_oper_param_compat *)(uintptr_t)request->params;
	for (i = 0; i < request->params_size; i++) {
		switch (params[i].type) {
		case TE_PARAM_TYPE_NONE:
		case TE_PARAM_TYPE_INT_RO:
		case TE_PARAM_TYPE_INT_RW:
			break;
		case TE_PARAM_TYPE_MEM_RO:
		case TE_PARAM_TYPE_MEM_RW:
		case TE_PARAM_TYPE_PERSIST_MEM_RO:
		case TE_PARAM_TYPE_PERSIST_MEM_RW:
			ret = te_prep_mem_buffer(request->session_id,
				(void *)(uintptr_t)params[i].u.Mem.base,
				params[i].u.Mem.len,
				params[i].type,
				context,
				pages != NULL);
			if (ret < 0) {
				pr_err("%s failed with err (%d)\n",
					__func__, ret);
				ret = OTE_ERROR_BAD_PARAMETERS;
				break;
			}
			break;
		default:
			pr_err("%s: OTE_ERROR_BAD_PARAMETERS\n", __func__);
			ret = OTE_ERROR_BAD_PARAMETERS;
			break;
		}
	}
	return ret;
}

static void te_release_mem_buffer(struct te_shmem_desc *shmem_desc)
{
	uint32_t i;

	list_del(&shmem_desc->list);
	for (i = 0; i < shmem_desc->nrpages; i++) {
		BUG_ON(!shmem_desc->pages[i]);
		if ((shmem_desc->type == TE_PARAM_TYPE_MEM_RW) ||
		    (shmem_desc->type == TE_PARAM_TYPE_PERSIST_MEM_RW))
			set_page_dirty(shmem_desc->pages[i]);
		put_page(shmem_desc->pages[i]);
	}
	kfree(shmem_desc);
}

static void te_release_mem_buffers(struct list_head *list)
{
	struct te_shmem_desc *shmem_desc, *tmp_shmem_desc;

	list_for_each_entry_safe(shmem_desc, tmp_shmem_desc, list, list)
		te_release_mem_buffer(shmem_desc);
}

static void te_release_temp_mem_buffers(struct tlk_context *context)
{
	te_release_mem_buffers(&context->temp_persist_shmem_list);
	te_release_mem_buffers(&context->temp_shmem_list);
}

static void te_update_persist_mem_buffers(struct te_session *session,
					  struct tlk_context *context)
{
	/*
	 * Assumes any entries on temp_persist_shmem_list belong to the session
	 * that has been passed in.
	 */

	list_splice_tail_init(&context->temp_persist_shmem_list,
			      &session->persist_shmem_list);
}

#if defined(CONFIG_SMP) && !defined(CONFIG_TRUSTY)
cpumask_t saved_cpu_mask;
static void switch_cpumask_to_cpu0(void)
{
	long ret;
	cpumask_t local_cpu_mask = CPU_MASK_NONE;

	cpu_set(0, local_cpu_mask);
	cpumask_copy(&saved_cpu_mask, tsk_cpus_allowed(current));
	ret = sched_setaffinity(0, &local_cpu_mask);
	if (ret)
		pr_err("sched_setaffinity #1 -> 0x%lX", ret);
}

static void restore_cpumask(void)
{
	long ret = sched_setaffinity(0, &saved_cpu_mask);
	if (ret)
		pr_err("sched_setaffinity #2 -> 0x%lX", ret);
}
#else
static inline void switch_cpumask_to_cpu0(void) {};
static inline void restore_cpumask(void) {};
#endif

uint32_t tlk_generic_smc(struct tlk_info *info,
			 uint32_t arg0, uintptr_t arg1, uintptr_t arg2,
			 uintptr_t arg3)
{
	uint32_t retval;


#ifndef CONFIG_TRUSTY
	switch_cpumask_to_cpu0();
	retval = _tlk_generic_smc(arg0, arg1, arg2);
	while (retval == TE_ERROR_PREEMPT_BY_IRQ ||
	       retval == TE_ERROR_PREEMPT_BY_FS) {
		if (retval == TE_ERROR_PREEMPT_BY_IRQ) {
			retval = _tlk_generic_smc((60 << 24), 0, 0);
		} else {
			tlk_ss_op();
			retval = _tlk_generic_smc(TE_SMC_SS_REQ_COMPLETE, 0, 0);
		}
	}

	restore_cpumask();
#else
	retval = trusty_std_call32(tlk_info->trusty_dev,
					arg0, arg1, arg2, arg3);
#endif

	/* Print TLK logs if any */
	ote_print_logs();

	return retval;
}

uint32_t tlk_extended_smc(struct tlk_info *info, uintptr_t *regs)
{
#ifndef CONFIG_TRUSTY
	uint32_t retval;

	switch_cpumask_to_cpu0();

	retval = _tlk_extended_smc(regs);
	while (retval == 0xFFFFFFFD)
		retval = _tlk_generic_smc((60 << 24), 0, 0);

	restore_cpumask();

	/* Print TLK logs if any */
	ote_print_logs();

	return retval;
#else
	return OTE_ERROR_GENERIC;
#endif
}

/*
 * Do an SMC call
 */
static void do_smc(struct te_request *request, struct tlk_device *dev)
{
	uint32_t smc_args;
	uint32_t smc_params = 0;

	if (dev->req_param_buf) {
		smc_args = (char *)request - dev->req_param_buf;
		if (request->params)
			smc_params = (char *)request->params -
						dev->req_param_buf;
	} else {
		smc_args = (uint32_t)virt_to_phys(request);
		if (request->params)
			smc_params = (uint32_t)virt_to_phys(request->params);
	}

	tlk_generic_smc(tlk_info, request->type, smc_args, smc_params, 0);
}

#ifdef CONFIG_TRUSTY
static int tlk_smc_notify_old(struct notifier_block *nb,
			      unsigned long action, void *data)
{
	struct tlk_info *info = container_of(nb, struct tlk_info, smc_notifier);

	if (action != TRUSTY_CALL_RETURNED)
		return NOTIFY_DONE;

	/* smc_count keeps track of SMCs made to Trusty since the last OTE SMC
	 * When we have seen more than one SMC call, we need to retry the OTE command
	 * since the last SMC call may not have been from the OTE driver and the OTE
	 * command could have finished running in the trusted OS.
	 */
	if (!atomic_dec_if_positive(&info->smc_count))
		complete(&info->smc_retry);

	return NOTIFY_OK;
}

static int tlk_smc_notify(struct notifier_block *nb,
			  unsigned long action, void *data)
{
	int ret;
	struct tlk_info *info = container_of(nb, struct tlk_info, smc_notifier);

	if (action != TRUSTY_CALL_RETURNED)
		return NOTIFY_DONE;

	ret = trusty_fast_call32(info->trusty_dev,
				 TE_SMC_FC_HAS_NS_WORK, 0, 0, 0);
	pr_debug("%s: has response %d\n", __func__, ret);
	if (ret > 0)
		complete(&info->smc_retry);

	return NOTIFY_OK;
}
#endif

static uint32_t ote_ns_cb_retry(void)
{
	int ret;
	ret = wait_for_completion_timeout(&tlk_info->smc_retry, HZ * 10);
	if (!ret && !tlk_info->smc_retry_timed_out) {
		tlk_info->smc_retry_timed_out = true;
		pr_warn("%s: timed out waiting for event, retry anyway\n",
			__func__);
	}
	return OTE_SUCCESS;
}

#if defined(CONFIG_CRYPTO_DEV_TEGRA_SE)
static uint32_t errno_to_ote_err(int err)
{
	switch (err) {
	case 0:
		return OTE_SUCCESS;
	case -EINVAL:
		return OTE_ERROR_BAD_PARAMETERS;
	case -EBUSY:
		return OTE_ERROR_BUSY;
	default:
		return OTE_ERROR_BAD_STATE;
	}
}

static uint32_t ote_ns_cb_se_acquire(void)
{
	int err = tegra_se_acquire(SE_AES_OP_MODE_RNG_DRBG);
	return errno_to_ote_err(err);
}

static uint32_t ote_ns_cb_se_release(void)
{
	int err = tegra_se_release();
	return errno_to_ote_err(err);
}
#endif

typedef uint32_t (*ote_ns_cb_t)(void);

ote_ns_cb_t cb_table[] = {
	[OTE_NS_CB_RETRY] = ote_ns_cb_retry,
#if defined(CONFIG_CRYPTO_DEV_TEGRA_SE)
	[OTE_NS_CB_SE_ACQUIRE] = ote_ns_cb_se_acquire,
	[OTE_NS_CB_SE_RELEASE] = ote_ns_cb_se_release,
#endif
	[OTE_NS_CB_SS] = tlk_ss_op,
};

/*
 * Do an SMC call
 */
static void do_smc_compat(struct te_request_compat *request,
			  struct tlk_device *dev)
{
	uint32_t smc_args;
	uint32_t smc_params = 0;
	uint32_t smc_pages = 0;
	uint32_t smc_nr = request->type;
	uint32_t cb_code;
	uint32_t smc_result;

	BUG_ON(!mutex_is_locked(&smc_lock));

	smc_args = (char *)request - (char *)(dev->req_addr_compat);
	if (dev->req_addr_phys)
		smc_args += dev->req_addr_phys;

	if (request->params) {
		smc_params = ((char *)(request->params) -
				(char *)(dev->param_addr_compat));
		if (dev->param_addr_phys)
			smc_params += dev->param_addr_phys;
		else
			smc_params += PAGE_SIZE;
	}

#ifdef CONFIG_TRUSTY
	smc_pages = (uint32_t)dev->param_pages_phys;

	while (true) {
		INIT_COMPLETION(tlk_info->smc_retry);
		atomic_set(&tlk_info->smc_count, 2);
		smc_result = tlk_generic_smc(tlk_info, smc_nr,
					     smc_args, smc_params, smc_pages);
		if (smc_result) {
			pr_err("%s: SMC failed: 0x%x\n", __func__, smc_result);
			request->result = OTE_ERROR_COMMUNICATION;
			break;
		}
		if ((request->result & OTE_ERROR_NS_CB_MASK) != OTE_ERROR_NS_CB)
			break;

		cb_code = request->result & ~OTE_ERROR_NS_CB_MASK;
		if (cb_code < ARRAY_SIZE(cb_table) && cb_table[cb_code]) {
			request->result = cb_table[cb_code]();
		} else {
			pr_err("Unknown TEE callback: 0x%x\n", cb_code);
			request->result = OTE_ERROR_NOT_IMPLEMENTED;
		}
		request->type = cb_code;
		smc_nr = TE_SMC_NS_CB_COMPLETE;
	}

	if (tlk_info->smc_retry_timed_out) {
		tlk_info->smc_retry_timed_out = false;
		pr_warn("%s: return after timeout retry, result=%x\n",
			__func__, request->result);
	}

#else
	tlk_generic_smc(tlk_info, smc_nr, smc_args, smc_params, smc_pages);
#endif
}

struct tlk_smc_work_args {
	uint32_t arg0;
	uint32_t arg1;
	uint32_t arg2;
};

static long tlk_generic_smc_on_cpu0(void *args)
{
	struct tlk_smc_work_args *work;
	int cpu = cpu_logical_map(smp_processor_id());
	uint32_t retval;

	BUG_ON(cpu != 0);

	work = (struct tlk_smc_work_args *)args;
	retval = tlk_generic_smc(tlk_info,
				work->arg0, work->arg1, work->arg2, 0);
	while (retval == 0xFFFFFFFD)
		retval = tlk_generic_smc(tlk_info, (60 << 24), 0, 0, 0);
	return retval;
}

/*
 * VPR programming SMC
 *
 * This routine is called both from normal threads and worker threads.
 * The worker threads are per-cpu and have PF_NO_SETAFFINITY set, so
 * any calls to sched_setaffinity will fail.
 *
 * If it's a worker thread on CPU0, just invoke the SMC directly. If
 * it's running on a non-CPU0, use work_on_cpu() to schedule the SMC
 * on CPU0.
 */
int te_set_vpr_params(void *vpr_base, size_t vpr_size)
{
	uint32_t retval;

	/* Share the same lock used when request is send from user side */
	mutex_lock(&smc_lock);

	if (current->flags &
	    (PF_WQ_WORKER | PF_NO_SETAFFINITY | PF_KTHREAD)) {
		struct tlk_smc_work_args work_args;
		int cpu = cpu_logical_map(smp_processor_id());

		work_args.arg0 = TE_SMC_PROGRAM_VPR;
		work_args.arg1 = (uint32_t)vpr_base;
		work_args.arg2 = vpr_size;

		/* workers don't change CPU. depending on the CPU, execute
		 * directly or sched work */
		if (cpu == 0 && (current->flags & PF_WQ_WORKER))
			retval = tlk_generic_smc_on_cpu0(&work_args);
		else
			retval = work_on_cpu(0,
					tlk_generic_smc_on_cpu0, &work_args);
	} else {
		retval = tlk_generic_smc(tlk_info, TE_SMC_PROGRAM_VPR,
					(uintptr_t)vpr_base, vpr_size, 0);
	}

	mutex_unlock(&smc_lock);

	if (retval != OTE_SUCCESS) {
		pr_err("te_set_vpr_params failed err (0x%x)\n", retval);
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(te_set_vpr_params);

void te_restore_keyslots(void)
{
	uint32_t retval;

	mutex_lock(&smc_lock);

	if (current->flags &
			(PF_WQ_WORKER | PF_NO_SETAFFINITY | PF_KTHREAD)) {
		struct tlk_smc_work_args work_args;
		int cpu = cpu_logical_map(smp_processor_id());

		work_args.arg0 = TE_SMC_TA_EVENT;
		work_args.arg1 = TA_EVENT_RESTORE_KEYS;
		work_args.arg2 = 0;

		/* workers don't change CPU. depending on the CPU, execute
		 * directly or sched work */
		if (cpu == 0 && (current->flags & PF_WQ_WORKER)) {
			retval = tlk_generic_smc_on_cpu0(&work_args);
		} else {
			retval = work_on_cpu(0,
					tlk_generic_smc_on_cpu0, &work_args);
		}
	} else {
		retval = tlk_generic_smc(tlk_info, TE_SMC_TA_EVENT,
				TA_EVENT_RESTORE_KEYS, 0, 0);
	}

	mutex_unlock(&smc_lock);
}
EXPORT_SYMBOL(te_restore_keyslots);

static int te_allocate_session(struct tlk_context *context, uint32_t session_id,
			       struct te_session **sessionp)
{
	struct rb_node **p = &context->sessions.rb_node;
	struct rb_node *parent = NULL;
	struct te_session *session;

	pr_debug("%s: %d\n", __func__, session_id);

	while (*p) {
		parent = *p;
		session = rb_entry(parent, struct te_session, node);

		if (session_id < session->session_id)
			p = &parent->rb_left;
		else if (session_id > session->session_id)
			p = &parent->rb_right;
		else
			return -EBUSY;
	}

	session = kzalloc(sizeof(*session), GFP_KERNEL);
	if (!session)
		return -ENOMEM;

	session->session_id = session_id;
	INIT_LIST_HEAD(&session->persist_shmem_list);

	rb_link_node(&session->node, parent, p);
	rb_insert_color(&session->node, &context->sessions);
	*sessionp = session;

	return 0;
}

static struct te_session *te_session_lookup(struct tlk_context *context,
					    uint32_t session_id)
{
	struct rb_node *n = context->sessions.rb_node;
	struct te_session *session;

	pr_debug("%s: %d\n", __func__, session_id);
	while (n) {
		session = rb_entry(n, struct te_session, node);

		if (session_id < session->session_id)
			n = n->rb_left;
		else if (session_id > session->session_id)
			n = n->rb_right;
		else
			return session;
	}
	return NULL;
}

static void te_session_opened(struct tlk_context *context, uint32_t session_id)
{
	int ret;
	struct te_session *session;

	ret = te_allocate_session(context, session_id, &session);
	if (ret) {
		pr_err("%s: failed to allocate session, %d\n", __func__, ret);
		BUG_ON(!list_empty(&context->temp_persist_shmem_list));
		return;
	}

	/* mark active any persistent mem buffers */
	te_update_persist_mem_buffers(session, context);
}

static void te_session_closed(struct tlk_context *context,
			      struct te_session *session)
{
	pr_debug("%s: %d\n", __func__, session->session_id);

	/* release any peristent mem buffers */
	te_release_mem_buffers(&session->persist_shmem_list);

	rb_erase(&session->node, &context->sessions);
	kfree(session);
}

/*
 * Open session SMC (supporting client-based te_open_session() calls)
 */
void te_open_session(struct te_opensession *cmd,
		     struct te_request *request,
		     struct tlk_context *context)
{
	int ret;

	request->type = TE_SMC_OPEN_SESSION;

	ret = te_prep_mem_buffers(request, context);
	if (ret != OTE_SUCCESS) {
		pr_err("%s: te_prep_mem_buffers failed err (0x%x)\n",
		       __func__, ret);
		SET_RESULT(request, ret, OTE_RESULT_ORIGIN_API);
		return;
	}

	memcpy(&request->dest_uuid,
	       &cmd->dest_uuid,
	       sizeof(struct te_service_id));

	pr_info("OPEN_CLIENT_SESSION: 0x%x 0x%x 0x%x 0x%x\n",
		request->dest_uuid[0],
		request->dest_uuid[1],
		request->dest_uuid[2],
		request->dest_uuid[3]);

	request->type = TE_SMC_OPEN_SESSION;

	do_smc(request, context->dev);

	if (request->result == OTE_SUCCESS)
		te_session_opened(context, request->session_id);

	te_release_temp_mem_buffers(context);
}

/*
 * Close session SMC (supporting client-based te_close_session() calls)
 */
void te_close_session(struct te_closesession *cmd,
		      struct te_request *request,
		      struct tlk_context *context)
{
	struct te_session *session;

	session = te_session_lookup(context, cmd->session_id);
	if (!session) {
		pr_err("%s: error unknown session: %d\n",
		       __func__, cmd->session_id);
		request->result = OTE_ERROR_BAD_PARAMETERS;
		return;
	}

	request->session_id = cmd->session_id;
	request->type = TE_SMC_CLOSE_SESSION;

	do_smc(request, context->dev);
	if (request->result)
		pr_info("%s: error closing session: %08x\n",
			__func__, request->result);

	te_session_closed(context, session);
}

/*
 * Launch operation SMC (supporting client-based te_launch_operation() calls)
 */
void te_launch_operation(struct te_launchop *cmd,
			 struct te_request *request,
			 struct tlk_context *context)
{
	int ret;
	struct te_session *session;

	session = te_session_lookup(context, cmd->session_id);
	if (!session) {
		pr_err("%s: error unknown session: %d\n",
		       __func__, cmd->session_id);
		request->result = OTE_ERROR_BAD_PARAMETERS;
		return;
	}

	request->session_id = cmd->session_id;
	request->command_id = cmd->operation.command;
	request->type = TE_SMC_LAUNCH_OPERATION;

	ret = te_prep_mem_buffers(request, context);
	if (ret != OTE_SUCCESS) {
		pr_err("%s: te_prep_mem_buffers failed err (0x%x)\n",
		       __func__, ret);
		SET_RESULT(request, ret, OTE_RESULT_ORIGIN_API);
		return;
	}

	do_smc(request, context->dev);

	if (request->result == OTE_SUCCESS) {
		/* mark active any persistent mem buffers */
		te_update_persist_mem_buffers(session, context);
	}

	te_release_temp_mem_buffers(context);
}

/*
 * Open session SMC (supporting client-based te_open_session() calls)
 */
void te_open_session_compat(struct te_opensession_compat *cmd,
			    struct te_request_compat *request,
			    struct tlk_context *context)
{
	int ret;

	request->type = TE_SMC_OPEN_SESSION;

	ret = te_prep_mem_buffers_compat(request, context);
	if (ret != OTE_SUCCESS) {
		pr_err("%s: te_prep_mem_buffers failed err (0x%x)\n",
		       __func__, ret);
		SET_RESULT(request, ret, OTE_RESULT_ORIGIN_API);
		return;
	}

	memcpy(&request->dest_uuid,
	       &cmd->dest_uuid,
	       sizeof(struct te_service_id));

	pr_info("OPEN_CLIENT_SESSION_COMPAT: 0x%x 0x%x 0x%x 0x%x\n",
		request->dest_uuid[0],
		request->dest_uuid[1],
		request->dest_uuid[2],
		request->dest_uuid[3]);

	request->type = TE_SMC_OPEN_SESSION;

	do_smc_compat(request, context->dev);

	if (request->result == OTE_SUCCESS)
		te_session_opened(context, request->session_id);

	te_release_temp_mem_buffers(context);
}

/*
 * Close session SMC (supporting client-based te_close_session() calls)
 */
void te_close_session_compat(struct te_closesession_compat *cmd,
			     struct te_request_compat *request,
			     struct tlk_context *context)
{
	struct te_session *session;

	session = te_session_lookup(context, cmd->session_id);
	if (!session) {
		pr_err("%s: error unknown session: %d\n",
		       __func__, cmd->session_id);
		request->result = OTE_ERROR_BAD_PARAMETERS;
		return;
	}

	request->session_id = cmd->session_id;
	request->type = TE_SMC_CLOSE_SESSION;

	do_smc_compat(request, context->dev);
	if (request->result)
		pr_info("%s: error closing session: %08x\n",
			__func__, request->result);

	te_session_closed(context, session);
}

/*
 * Launch operation SMC (supporting client-based te_launch_operation() calls)
 */
void te_launch_operation_compat(struct te_launchop_compat *cmd,
				struct te_request_compat *request,
				struct tlk_context *context)
{
	int ret;
	struct te_session *session;

	session = te_session_lookup(context, cmd->session_id);
	if (!session) {
		pr_err("%s: error unknown session: %d\n",
		       __func__, cmd->session_id);
		request->result = OTE_ERROR_BAD_PARAMETERS;
		return;
	}

	request->session_id = cmd->session_id;
	request->command_id = cmd->operation.command;
	request->type = TE_SMC_LAUNCH_OPERATION;

	ret = te_prep_mem_buffers_compat(request, context);
	if (ret != OTE_SUCCESS) {
		pr_err("%s: te_prep_mem_buffers failed err (0x%x)\n",
		       __func__, ret);
		SET_RESULT(request, ret, OTE_RESULT_ORIGIN_API);
		return;
	}

	do_smc_compat(request, context->dev);

	if (request->result == OTE_SUCCESS) {
		/* mark active any persistent mem buffers */
		te_update_persist_mem_buffers(session, context);
	}

	te_release_temp_mem_buffers(context);
}

static void tlk_ote_init(struct tlk_info *info)
{
	tlk_ss_init_legacy(info);
	tlk_ss_init(info);
	ote_logger_init(info);
	tlk_device_init(info);
}

#ifndef CONFIG_TRUSTY
static int __init tlk_register_irq_handler(void)
{
	tlk_generic_smc(tlk_info, TE_SMC_REGISTER_IRQ_HANDLER,
		(uintptr_t)tlk_irq_handler, 0, 0);
	tlk_ote_init(NULL);
	return 0;
}

arch_initcall(tlk_register_irq_handler);
#else
static int trusty_ote_probe(struct platform_device *pdev)
{
	int ret;
	struct tlk_info *info;

	info = kzalloc(sizeof(struct tlk_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->trusty_dev = pdev->dev.parent;
	init_completion(&info->smc_retry);
	platform_set_drvdata(pdev, info);

	ret = trusty_fast_call32(info->trusty_dev,
				 TE_SMC_FC_HAS_NS_WORK, 0, 0, 0);
	if (ret >= 0) {
		info->smc_notifier.notifier_call = tlk_smc_notify;
	} else {
		dev_warn(&pdev->dev, "TE_SMC_FC_HAS_NS_WORK not supported\n");
		info->smc_notifier.notifier_call = tlk_smc_notify_old;
	}

	trusty_call_notifier_register(info->trusty_dev, &info->smc_notifier);

	tlk_info = info;
	tlk_ote_init(tlk_info);

	return 0;
}

static int trusty_ote_remove(struct platform_device *pdev)
{
	/* Note: We currently cannot remove this driver because
	 * of downstream dependencies like tlk_device, etc. Those need
	 * to be properly fixed to support dynamic unregistration. Until then,
	 * we cannot remove this device.
	 */
	return -EBUSY;
}

static const struct of_device_id trusty_of_match[] = {
	{
		.compatible	= "android,trusty-ote-v1",
	},
};

MODULE_DEVICE_TABLE(of, trusty_of_match);

static struct platform_driver trusty_ote_driver = {
	.probe = trusty_ote_probe,
	.remove = trusty_ote_remove,
	.driver = {
		.name = "trusty-ote",
		.owner = THIS_MODULE,
		.of_match_table = trusty_of_match,
	},
};

module_platform_driver(trusty_ote_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("OTE driver");
#endif
