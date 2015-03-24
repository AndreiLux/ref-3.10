/* drivers/misc/lowmemorykiller.c
 *
 * The lowmemorykiller driver lets user-space specify a set of memory thresholds
 * where processes with a range of oom_score_adj values will get killed. Specify
 * the minimum oom_score_adj values in
 * /sys/module/lowmemorykiller/parameters/adj and the number of free pages in
 * /sys/module/lowmemorykiller/parameters/minfree. Both files take a comma
 * separated list of numbers in ascending order.
 *
 * For example, write "0,8" to /sys/module/lowmemorykiller/parameters/adj and
 * "1024,4096" to /sys/module/lowmemorykiller/parameters/minfree to kill
 * processes with a oom_score_adj value of 8 or higher when the free memory
 * drops below 4096 pages and kill processes with a oom_score_adj value of 0 or
 * higher when the free memory drops below 1024 pages.
 *
 * The driver considers memory used for caches to be free, but if a large
 * percentage of the cached memory is locked this can be very inaccurate
 * and processes may not get killed until the normal oom killer is triggered.
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/swap.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/notifier.h>
#include <linux/delay.h>

#if defined(CONFIG_MTK_AEE_FEATURE) && defined(CONFIG_MT_ENG_BUILD)
#include <linux/aee.h>
#include <linux/disp_assert_layer.h>
uint32_t in_lowmem = 0;
#endif

#ifdef CONFIG_ZRAM
extern void mlog(int type);
#endif

extern void show_free_areas_minimum(void);

#if defined(CONFIG_MT_ENG_BUILD) && defined(CONFIG_MT_LMK_RAM_DUMP)
static short lowmem_debug_adj = 1;
static short lowmem_debug_score_adj;

static short lowmem_kernel_warn_adj;
static short lowmem_kernel_warn_score_adj;
#endif

static DEFINE_MUTEX(lowmem_shrink_mutex);
static uint32_t lowmem_debug_level = 1;

#ifdef CONFIG_ZRAM
static short lowmem_adj[9] = {
	0,
	1,
	2,
	4,
	6,
	8,
	9,
	12,
	15,
};
static int lowmem_adj_size = 9;
static int lowmem_minfree[9] = {
	4 * 256,	/* 4MB */
	12 * 256,	/* 12MB */
	16 * 256,	/* 16MB */
	24 * 256,	/* 24MB */
	28 * 256,	/* 28MB */
	32 * 256,	/* 32MB */
	36 * 256,	/* 36MB */
	40 * 256,	/* 40MB */
	48 * 256,	/* 48MB */
};
static int lowmem_minfree_size = 9;
#else // CONFIG_ZRAM
static short lowmem_adj[6] = {
	0,
	1,
	6,
	12,
};
static int lowmem_adj_size = 4;
static int lowmem_minfree[6] = {
	3 * 512,	/* 6MB */
	2 * 1024,	/* 8MB */
	4 * 1024,	/* 16MB */
	16 * 1024,	/* 64MB */
};
static int lowmem_minfree_size = 4;
#endif // CONFIG_ZRAM

static unsigned long lowmem_deathpending_timeout;

#define lowmem_print(level, x...)			\
	do {						\
		if (lowmem_debug_level >= (level))	\
			pr_info(x);			\
	} while (0)

static int test_task_flag(struct task_struct *p, int flag)
{
	struct task_struct *t = p;

	do {
		task_lock(t);
		if (test_tsk_thread_flag(t, flag)) {
			task_unlock(t);
			return 1;
		}
		task_unlock(t);
	} while_each_thread(p, t);

	return 0;
}

static short lowmem_oom_score_adj_to_oom_adj(short oom_score_adj)
{
    if (oom_score_adj == OOM_SCORE_ADJ_MAX)
        return OOM_ADJUST_MAX;
    else
        return ((oom_score_adj * -OOM_DISABLE *10)/OOM_SCORE_ADJ_MAX + 5)/10; //round
}

static int lowmem_shrink(struct shrinker *s, struct shrink_control *sc)
{
	struct task_struct *tsk;
	struct task_struct *selected = NULL;
	int rem = 0;
	int tasksize;
	int i;
	short min_score_adj = OOM_SCORE_ADJ_MAX + 1;
	int minfree = 0;
	int selected_tasksize = 0;
	short selected_oom_score_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);
	int other_free;
	int other_file;

	unsigned long nr_to_scan = sc->nr_to_scan;

#ifdef CONFIG_MT_ENG_BUILD
	int print_extra_info = 0;
#ifdef CONFIG_MT_LMK_RAM_DUMP
	static unsigned long lowmem_print_extra_info_timeout;

	/*dump memory info when framework low memory*/
	int pid_dump = -1; /* process which need to be dump */
	int pid_sec_mem = -1;
	int max_mem = 0;
#endif

	/* add_kmem_status_lmk_counter(); */
#endif /* CONFIG_MT_ENG_BUILD */

	if (nr_to_scan > 0) {
		if (mutex_lock_interruptible(&lowmem_shrink_mutex) < 0) {
			lowmem_print(4, "lowmem_shrink lock failed\n");
			return 0;
		}
	}

	/* Do not subtract totalreserve_pages here - the name of this
	 * parameter is very misleading - it is does not represent the
	 * sum of pages_min - it is the sum of pages_high, and hence will
	 * almost always be greater than the number of free pages when the
	 * system is under pressure!
	 *
	 * There is no need to account for reserves at all here since
	 * this value is only used to index into the lowmem_minfree
	 * parameters, which are intended to be tunable at runtime.
	 * If these thresholds don't work well for the device, they
	 * should be retuned.
	 */
	other_free = global_page_state(NR_FREE_PAGES);
	other_file = global_page_state(NR_FILE_PAGES) -
						global_page_state(NR_SHMEM);

#ifdef CONFIG_ZRAM
	other_file -= total_swapcache_pages();
	if (other_file < 0)
		other_file = 0;
#endif

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;
	if (lowmem_minfree_size < array_size)
		array_size = lowmem_minfree_size;
	for (i = 0; i < array_size; i++) {
		minfree = lowmem_minfree[i];
		if (other_free < minfree && other_file < minfree) {
			min_score_adj = lowmem_adj[i];
			break;
		}
	}
	if (nr_to_scan > 0)
		lowmem_print(3, "lowmem_shrink %lu, %x, ofree %d %d, ma %hd\n",
				nr_to_scan, sc->gfp_mask, other_free,
				other_file, min_score_adj);
	rem = global_page_state(NR_ACTIVE_ANON) +
		global_page_state(NR_ACTIVE_FILE) +
		global_page_state(NR_INACTIVE_ANON) +
		global_page_state(NR_INACTIVE_FILE);
	if (nr_to_scan <= 0 || min_score_adj == OOM_SCORE_ADJ_MAX + 1) {
		lowmem_print(5, "lowmem_shrink %lu, %x, return %d\n",
			     nr_to_scan, sc->gfp_mask, rem);
    /*
     * disable indication if low memory
     */
#if defined(CONFIG_MTK_AEE_FEATURE) && defined(CONFIG_MT_ENG_BUILD) && defined(CONFIG_MT_LMK_RAM_DUMP)
		if (in_lowmem) {
			in_lowmem = 0;
			/* DAL_LowMemoryOff(); */
			lowmem_print(1, "LowMemoryOff\n");
		}
#endif
		if (nr_to_scan > 0)
			mutex_unlock(&lowmem_shrink_mutex);
		return rem;
	}
	selected_oom_score_adj = min_score_adj;

	/* add debug log */
#ifdef CONFIG_MT_ENG_BUILD
#ifdef CONFIG_MT_LMK_RAM_DUMP
	if (min_score_adj <= lowmem_debug_score_adj) {

		if (lowmem_print_extra_info_timeout == 0)
			lowmem_print_extra_info_timeout = jiffies;

		if (time_after_eq(jiffies, lowmem_print_extra_info_timeout)) {
			lowmem_print_extra_info_timeout = jiffies + HZ;
			print_extra_info = 1;
		}
	}
#else
	print_extra_info = 0;
#endif
	if (print_extra_info) {
		lowmem_print(2, "======low memory killer=====\n");
		lowmem_print(2, "Free memory other_free: %d, other_file:%d pages\n", other_free, other_file);
	}
#endif

	rcu_read_lock();
	for_each_process(tsk) {
		struct task_struct *p;
		short oom_score_adj;

		if (tsk->flags & PF_KTHREAD)
			continue;

		/* if task no longer has any memory ignore it */
		if (test_task_flag(tsk, TIF_MM_RELEASED))
			continue;

		if (time_before_eq(jiffies, lowmem_deathpending_timeout)) {
			if (test_task_flag(tsk, TIF_MEMDIE)) {
				rcu_read_unlock();
				/* give the system time to free up the memory
				 * to avoid too many lowmem_shrinks eating cpu
				 */
				msleep_interruptible(20);
				mutex_unlock(&lowmem_shrink_mutex);
				return 0;
			}
		}

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		oom_score_adj = p->signal->oom_score_adj;

#if defined(CONFIG_MT_ENG_BUILD) && defined(CONFIG_MT_LMK_RAM_DUMP)
		if (print_extra_info) {
#ifdef CONFIG_ZRAM
			lowmem_print(2, "Candidate %d (%s), adj %d, score_adj %d, rss %lu, rswap %lu, to kill\n",
					p->pid, p->comm, lowmem_oom_score_adj_to_oom_adj(oom_score_adj),
					oom_score_adj, get_mm_rss(p->mm), get_mm_counter(p->mm, MM_SWAPENTS));
#else /* CONFIG_ZRAM */
			lowmem_print(2, "Candidate %d (%s), adj %d, score_adj %d, rss %lu, to kill\n",
					p->pid, p->comm, lowmem_oom_score_adj_to_oom_adj(oom_score_adj),
					oom_score_adj, get_mm_rss(p->mm));
#endif /* CONFIG_ZRAM */
	}
#endif /* CONFIG_MT_ENG_BUILD */

		if (oom_score_adj < min_score_adj) {
			task_unlock(p);
			continue;
		}
#ifdef CONFIG_ZRAM
		tasksize = get_mm_rss(p->mm) + get_mm_counter(p->mm, MM_SWAPENTS);
#else
		tasksize = get_mm_rss(p->mm);
#endif
		task_unlock(p);
		if (tasksize <= 0)
			continue;

#if defined(CONFIG_MT_ENG_BUILD) && defined(CONFIG_MT_LMK_RAM_DUMP)
		/*
			* dump memory info when framework low memory:
			* record the first two pid which consumed most memory.
		*/
		if (tasksize > max_mem) {
			max_mem = tasksize;
			pid_sec_mem = pid_dump;
			pid_dump = p->pid;

		}
#endif

		if (selected) {
			if (oom_score_adj < selected_oom_score_adj)
				continue;
			if (oom_score_adj == selected_oom_score_adj &&
			    tasksize <= selected_tasksize)
				continue;
		}
		selected = p;
		selected_tasksize = tasksize;
		selected_oom_score_adj = oom_score_adj;
		lowmem_print(2, "select '%s' (%d), adj %d, score_adj %hd, size %d, to kill\n",
			     p->comm, p->pid, lowmem_oom_score_adj_to_oom_adj(oom_score_adj), oom_score_adj, tasksize);
	}
	if (selected) {
		lowmem_print(1, "Killing '%s' (%d), adj %d, score_adj %hd,\n" \
				"   to free %ldkB on behalf of '%s' (%d) because\n" \
				"   cache %ldkB is below limit %ldkB for oom_score_adj %hd\n" \
				"   Free memory is %ldkB above reserved\n",
			     selected->comm, selected->pid,
				lowmem_oom_score_adj_to_oom_adj(selected_oom_score_adj),
			     selected_oom_score_adj,
			     selected_tasksize * (long)(PAGE_SIZE / 1024),
			     current->comm, current->pid,
			     other_file * (long)(PAGE_SIZE / 1024),
			     minfree * (long)(PAGE_SIZE / 1024),
			     min_score_adj,
			     other_free * (long)(PAGE_SIZE / 1024));
		lowmem_deathpending_timeout = jiffies + HZ;
		if (lowmem_debug_level >= 2) {
			lowmem_print(2, "low memory info:\n");
			show_free_areas_minimum();
		}

		/*
		 * when kill adj=0 process trigger kernel warning, only in MTK internal eng load
		 */
#if defined(CONFIG_MTK_AEE_FEATURE) && defined(CONFIG_MT_ENG_BUILD) && \
	defined(MTK_INTERNAL_BUILD) && defined(CONFIG_MT_LMK_RAM_DUMP)
		if (selected_oom_score_adj <= lowmem_kernel_warn_score_adj) {
			/* can set lowmem_kernel_warn_adj=16 for test */
#define MSG_SIZE_TO_AEE 70
			char msg_to_aee[MSG_SIZE_TO_AEE];
			lowmem_print(1, "low memory trigger kernel warning\n");

			if (pid_dump == selected->pid)
				pid_dump = pid_sec_mem;
			snprintf(msg_to_aee, MSG_SIZE_TO_AEE,
					"please contact AP/AF memory module owner[pid:%d]\n", pid_dump);

			aee_kernel_warning_api("LMK", 0,
				DB_OPT_DEFAULT|DB_OPT_DUMPSYS_ACTIVITY|DB_OPT_LOW_MEMORY_KILLER
				| DB_OPT_PID_MEMORY_INFO /*for smaps and hprof*/
				| DB_OPT_PROCESS_COREDUMP
				| DB_OPT_DUMPSYS_SURFACEFLINGER,
				"Framework low memory", msg_to_aee);
		}
#endif
		/*
		 * show an indication if low memory
		 */
#if defined(CONFIG_MTK_AEE_FEATURE) && defined(CONFIG_MT_ENG_BUILD) && defined(CONFIG_MT_LMK_RAM_DUMP)
		if (!in_lowmem && selected_oom_score_adj <= lowmem_debug_score_adj) {
			in_lowmem = 1;
			/*DAL_LowMemoryOn();*/
			lowmem_print(1, "LowMemoryOn\n");
			/*aee_kernel_warning(module_name, lowmem_warning);*/
		}
#endif

#ifdef CONFIG_ZRAM
	    mlog(1);
#endif
		send_sig(SIGKILL, selected, 0);
		set_tsk_thread_flag(selected, TIF_MEMDIE);
		rem -= selected_tasksize;
		rcu_read_unlock();
		msleep_interruptible(20);
	} else
		rcu_read_unlock();
	mutex_unlock(&lowmem_shrink_mutex);
	lowmem_print(4, "lowmem_shrink %lu, %x, return %d\n",
		     nr_to_scan, sc->gfp_mask, rem);
	return rem;
}

static struct shrinker lowmem_shrinker = {
	.shrink = lowmem_shrink,
	.seeks = DEFAULT_SEEKS * 16
};

static int __init lowmem_init(void)
{
#ifdef CONFIG_ZRAM
	vm_swappiness = 100;
#endif
	register_shrinker(&lowmem_shrinker);
	return 0;
}

static void __exit lowmem_exit(void)
{
	unregister_shrinker(&lowmem_shrinker);
}

#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
static short lowmem_oom_adj_to_oom_score_adj(short oom_adj)
{
	if (oom_adj == OOM_ADJUST_MAX)
		return OOM_SCORE_ADJ_MAX;
	else
		return ((oom_adj * OOM_SCORE_ADJ_MAX * 10) / -OOM_DISABLE + 5)/10;//round
}

static void lowmem_autodetect_oom_adj_values(void)
{
	int i;
	short oom_adj;
	short oom_score_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;

	if (array_size <= 0)
		return;

	oom_adj = lowmem_adj[array_size - 1];
	if (oom_adj > OOM_ADJUST_MAX)
		return;

	oom_score_adj = lowmem_oom_adj_to_oom_score_adj(oom_adj);
	if (oom_score_adj <= OOM_ADJUST_MAX)
		return;

	/*lowmem_print(1, "lowmem_shrink: convert oom_adj to oom_score_adj:\n");*/
	for (i = 0; i < array_size; i++) {
		oom_adj = lowmem_adj[i];
		oom_score_adj = lowmem_oom_adj_to_oom_score_adj(oom_adj);
		lowmem_adj[i] = oom_score_adj;
		/*lowmem_print(1, "oom_adj %d => oom_score_adj %d\n",
			     oom_adj, oom_score_adj); */
	}
#if defined(CONFIG_MT_ENG_BUILD) && defined(CONFIG_MT_LMK_RAM_DUMP)
    lowmem_debug_score_adj = lowmem_oom_adj_to_oom_score_adj(lowmem_debug_adj);
    /*lowmem_print(1, "lowmem_debug_score_adj %d\n", lowmem_debug_score_adj);*/
    lowmem_kernel_warn_score_adj = lowmem_oom_adj_to_oom_score_adj(lowmem_kernel_warn_adj);
    /*lowmem_print(1, "lowmem_kernel_warn_score_adj %d\n", lowmem_kernel_warn_score_adj);*/
#endif
}

static int lowmem_adj_array_set(const char *val, const struct kernel_param *kp)
{
	int ret;

	ret = param_array_ops.set(val, kp);

	/* HACK: Autodetect oom_adj values in lowmem_adj array */
	lowmem_autodetect_oom_adj_values();

	return ret;
}

static int lowmem_adj_array_get(char *buffer, const struct kernel_param *kp)
{
	return param_array_ops.get(buffer, kp);
}

static void lowmem_adj_array_free(void *arg)
{
	param_array_ops.free(arg);
}

static struct kernel_param_ops lowmem_adj_array_ops = {
	.set = lowmem_adj_array_set,
	.get = lowmem_adj_array_get,
	.free = lowmem_adj_array_free,
};

static const struct kparam_array __param_arr_adj = {
	.max = ARRAY_SIZE(lowmem_adj),
	.num = &lowmem_adj_size,
	.ops = &param_ops_short,
	.elemsize = sizeof(lowmem_adj[0]),
	.elem = lowmem_adj,
};
#endif

/*
 * get_min_free_pages
 * returns the low memory killer watermark of the given pid,
 * When the system free memory is lower than the watermark, the LMK (low memory
 * killer) may try to kill processes.
 */
int get_min_free_pages(pid_t pid)
{
    struct task_struct *p = 0;
    int target_oom_adj = 0;
    int i = 0;
    int array_size = ARRAY_SIZE(lowmem_adj);

    if (lowmem_adj_size < array_size)
            array_size = lowmem_adj_size;
    if (lowmem_minfree_size < array_size)
            array_size = lowmem_minfree_size;

    for_each_process(p) {
        /* search pid */
        if (p->pid == pid) {
            task_lock(p);
            target_oom_adj = p->signal->oom_score_adj;
            task_unlock(p);
            /* get min_free value of the pid */
            for (i = array_size - 1; i >= 0; i--) {
                if (target_oom_adj >= lowmem_adj[i]) {
                    lowmem_print(3, KERN_INFO"pid: %d, target_oom_adj = %d, "
                            "lowmem_adj[%d] = %d, lowmem_minfree[%d] = %d\n",
                            pid, target_oom_adj, i, lowmem_adj[i], i,
                            lowmem_minfree[i]);
                    return lowmem_minfree[i];
                }
            }
            goto out; 
        }
    }

out:
    lowmem_print(3, KERN_ALERT"[%s]pid: %d, adj: %d, lowmem_minfree = 0\n", 
            __FUNCTION__, pid, p->signal->oom_score_adj);
    return 0;
}
EXPORT_SYMBOL(get_min_free_pages);

/* Query LMK minfree settings */
/* To query default value, you can input index with value -1. */
size_t query_lmk_minfree(int index)
{
	int which;

	/* Invalid input index, return default value */
	if (index < 0) {
		return lowmem_minfree[2];
	}

	/* Find a corresponding output */
	which = 5;
	do {
		if (lowmem_adj[which] <= index) {
			break;
		}
	} while (--which >= 0);

	/* Fix underflow bug */
	which = (which < 0)? 0 : which;

	return lowmem_minfree[which];
}
EXPORT_SYMBOL(query_lmk_minfree);

module_param_named(cost, lowmem_shrinker.seeks, int, S_IRUGO | S_IWUSR);
#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
__module_param_call(MODULE_PARAM_PREFIX, adj,
		    &lowmem_adj_array_ops,
		    .arr = &__param_arr_adj,
		    S_IRUGO | S_IWUSR, -1);
__MODULE_PARM_TYPE(adj, "array of short");
#else
module_param_array_named(adj, lowmem_adj, short, &lowmem_adj_size,
			 S_IRUGO | S_IWUSR);
#endif
module_param_array_named(minfree, lowmem_minfree, uint, &lowmem_minfree_size,
			 S_IRUGO | S_IWUSR);
module_param_named(debug_level, lowmem_debug_level, uint, S_IRUGO | S_IWUSR);
#if defined(CONFIG_MT_ENG_BUILD) && defined(CONFIG_MT_LMK_RAM_DUMP)
module_param_named(debug_adj, lowmem_debug_adj, short, S_IRUGO | S_IWUSR);
#endif

late_initcall(lowmem_init);
/*module_init(lowmem_init);*/
module_exit(lowmem_exit);

MODULE_LICENSE("GPL");

