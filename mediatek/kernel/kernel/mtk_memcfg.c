#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <linux/aee.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <mach/mtk_memcfg.h>

#define MTK_MEMCFG_SIMPLE_BUFFER_LEN 16
#define MTK_MEMCFG_LARGE_BUFFER_LEN (1500)	/* 1200 bytes, it should not be larger than 1 page */

struct mtk_memcfg_info_buf {
	unsigned long max_len;
	unsigned long curr_pos;
	char buf[MTK_MEMCFG_LARGE_BUFFER_LEN];
};

static struct mtk_memcfg_info_buf mtk_memcfg_layout_buf = {
	.buf = {[0 ... (MTK_MEMCFG_LARGE_BUFFER_LEN - 1)] = 0,},
	.max_len = MTK_MEMCFG_LARGE_BUFFER_LEN,
	.curr_pos = 0,
};

static unsigned long mtk_memcfg_late_warning_flag = 0;

void mtk_memcfg_write_memory_layout_buf(char *fmt, ...)
{
	va_list ap;
	struct mtk_memcfg_info_buf *layout_buf = &mtk_memcfg_layout_buf;
	if (layout_buf->curr_pos <= layout_buf->max_len) {
		va_start(ap, fmt);
		layout_buf->curr_pos +=
		    vsnprintf((layout_buf->buf + layout_buf->curr_pos),
			      (layout_buf->max_len - layout_buf->curr_pos), fmt,
			      ap);
		va_end(ap);
	}
}

void mtk_memcfg_late_warning(unsigned long flag)
{
	mtk_memcfg_late_warning_flag |= flag;
}

/* kenerl memory information */

static int mtk_memcfg_memory_layout_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s", mtk_memcfg_layout_buf.buf);
	seq_printf(m, "buffer usage: %lu/%lu\n",
		   (mtk_memcfg_layout_buf.curr_pos <=
		    mtk_memcfg_layout_buf.max_len ? mtk_memcfg_layout_buf.
		    curr_pos : mtk_memcfg_layout_buf.max_len),
		   mtk_memcfg_layout_buf.max_len);

	return 0;
}

static int mtk_memcfg_memory_layout_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_memcfg_memory_layout_show, NULL);
}

/* end of kenerl memory information */

/* kenerl memory fragmentation trigger */

static LIST_HEAD(frag_page_list);
static DEFINE_SPINLOCK(frag_page_list_lock);
static unsigned long mtk_memcfg_frag_round = 0;
static struct kmem_cache *frag_page_cache;

struct frag_page {
	struct list_head list;
	struct page *page;
};

static int mtk_memcfg_frag_show(struct seq_file *m, void *v)
{
	int cnt = 0;
	struct frag_page *frag_page, *n_frag_page;
	spin_lock(&frag_page_list_lock);
	list_for_each_entry_safe(frag_page, n_frag_page, &frag_page_list, list) {
		cnt++;
	}
	spin_unlock(&frag_page_list_lock);
	seq_printf(m, "round: %lu, fragmentation-trigger held %d pages,"
		   " %d MB\n", mtk_memcfg_frag_round,
		   cnt, (cnt << PAGE_SHIFT) >> 20);

	return 0;
}

static int mtk_memcfg_frag_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_memcfg_frag_show, NULL);
}

void split_page(struct page *page, unsigned int order);

static int do_fragmentation(void *n)
{
	struct frag_page *frag_page, *n_frag_page;
	struct page *page;
	gfp_t gfp_mask = GFP_ATOMIC;
	unsigned int max_order = 2;
	int cnt = 0, i;

	/* trigger fragmentation */
	/* 
	 * Allocate an order-2-page, split it into 4 order-0-pages,
	 * and free 3 of them, repeatedly. 
	 * In this way, we split all high order pages to 
	 * order-0-pages and order-1-pages to create a 
	 * fragmentation scenario.
	 *
	 * In current stage, we only trigger fragmentation in 
	 * normal zone.
	 */
	while (1) {
#if 1
		if (cnt >= 10000) {
			/* 
			 * release all memory and restart the fragmentation 
			 * Allocating too much frag_page consumes too mush order-0 pages
			 */
			spin_lock(&frag_page_list_lock);
			list_for_each_entry_safe(frag_page, n_frag_page,
						 &frag_page_list, list) {
				list_del(&frag_page->list);
				__free_page(frag_page->page);
				kmem_cache_free(frag_page_cache, frag_page);
				cnt--;
			}
			spin_unlock(&frag_page_list_lock);
			pr_alert("round: %lu, fragmentation-trigger free pages"
				 "%d left\n", mtk_memcfg_frag_round, cnt);
		}
#endif
		while (1) {
			frag_page = kmem_cache_alloc(frag_page_cache, gfp_mask);
			if (!frag_page) {
				break;
			}
			page = alloc_pages(gfp_mask, max_order);
			if (!page) {
				kfree(frag_page);
				break;
			}
			split_page(page, 0);
			INIT_LIST_HEAD(&frag_page->list);
			frag_page->page = page;
			spin_lock(&frag_page_list_lock);
			list_add(&frag_page->list, &frag_page_list);
			spin_unlock(&frag_page_list_lock);
			for (i = 1; i < (1 << max_order); i++) {
				__free_page(page + i);
			}
			cnt++;
		}
		mtk_memcfg_frag_round++;
		pr_alert("round: %lu, fragmentation-trigger allocate %d pages"
			 " %d MB\n",
			 mtk_memcfg_frag_round, cnt, (cnt << PAGE_SHIFT) >> 20);
		msleep(500);
	}

	return 0;
}

static ssize_t
mtk_memcfg_frag_write(struct file *file, const char __user * buffer,
		      size_t count, loff_t * pos)
{
	static char state;
	static struct task_struct *p = 0;
	if (count > 0) {
		if (get_user(state, buffer)) {
			return -EFAULT;
		}
		state -= '0';
		pr_alert("%s state = %d\n", __func__, state);
		if (state) {
			pr_alert("activate do_fragmentation kthread\n");
			p = kthread_create(do_fragmentation, NULL,
					   "fragmentationd");
			if (!IS_ERR(p)) {
				wake_up_process(p);
			}

		}
	}
	return count;
}

/* end of kenerl memory fragmentation trigger */

static int __init mtk_memcfg_init(void)
{
	return 0;
}

static void __exit mtk_memcfg_exit(void)
{
}

static const struct file_operations mtk_memcfg_memory_layout_operations = {
	.open = mtk_memcfg_memory_layout_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations mtk_memcfg_frag_operations = {
	.open = mtk_memcfg_frag_open,
	.write = mtk_memcfg_frag_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#ifdef CONFIG_SLUB_DEBUG

extern int slabtrace_open(struct inode *inode, struct file *file);
static const struct file_operations proc_slabtrace_operations = {
	.open = slabtrace_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif
static int __init mtk_memcfg_late_init(void)
{
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtk_memcfg_dir = NULL;

	pr_info("[%s] start\n", __FUNCTION__);

	mtk_memcfg_dir = proc_mkdir("mtk_memcfg", NULL);

	if (!mtk_memcfg_dir) {

		pr_err("[%s]: mkdir /proc/mtk_memcfg failed\n", __FUNCTION__);

	} else {

		/* display kernel memory layout */
		entry = proc_create("memory_layout",
				    S_IRUGO | S_IWUSR, mtk_memcfg_dir,
				    &mtk_memcfg_memory_layout_operations);

		if (!entry) {
			pr_err("create memory_layout proc entry failed\n");
		}

		/* display kernel memory layout */
		entry = proc_create("frag-trigger",
				    S_IRUGO | S_IWUSR, mtk_memcfg_dir,
				    &mtk_memcfg_frag_operations);

		if (!entry) {
			pr_err("create memory_layout proc entry failed\n");
		}

		frag_page_cache = kmem_cache_create("frag_page_cache",
						    sizeof(struct frag_page),
						    0, SLAB_PANIC, NULL);

		if (!frag_page_cache) {
			pr_err("create frag_page_cache failed\n");
		}

#ifdef CONFIG_SLUB_DEBUG
		/* slabtrace - full slub object backtrace */
		entry = proc_create("slabtrace",
				    S_IRUSR, mtk_memcfg_dir,
				    &proc_slabtrace_operations);

		if (!entry) {
			pr_err("create slabtrace proc entry failed\n");
		}
#endif

	}

	return 0;
}

module_init(mtk_memcfg_init);
module_exit(mtk_memcfg_exit);

extern unsigned long totalhigh_pages;
static int __init mtk_memcfg_late_sanity_test(void)
{
	/* trigger kernel warning if warning flag is set */
	if (mtk_memcfg_late_warning_flag & WARN_MEMBLOCK_CONFLICT) {
        	aee_kernel_warning("[memory layout conflict]", 
				mtk_memcfg_layout_buf.buf);
	}

	if (mtk_memcfg_late_warning_flag & WARN_MEMSIZE_CONFLICT) {
        	aee_kernel_warning("[memory size conflict]", 
				mtk_memcfg_layout_buf.buf);
	}

	/* check highmem zone size */
	if (unlikely
	    (totalhigh_pages && (totalhigh_pages << PAGE_SHIFT) < SZ_8M)) {
		aee_kernel_warning("[high zone lt 8MB]", __FUNCTION__);
	}

	return 0;
}

late_initcall(mtk_memcfg_late_init);
late_initcall(mtk_memcfg_late_sanity_test);
