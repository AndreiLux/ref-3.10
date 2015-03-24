#include <linux/slab.h>
#include <linux/aee.h>
#include <linux/atomic.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <asm/io.h>

#define RC_CPU_COUNT NR_CPUS
#define RAM_CONSOLE_HEADER_STR_LEN 1024

struct ram_buffer {
	char *start;	/* start of data memory (first valid byte) */
	char *end;	/* end of data memory (first invalid byte) */
	char *wptr;	/* write pointer: start <= wptr < end */
	size_t count;	/* valid data count: 0 <= count <= (end-start) */
};

struct ram_cpu_info {
	u64 jiffies_last_irq_enter;
	u64 jiffies_last_irq_exit;
	u64 jiffies_last_sched;
	u32 last_irq_enter;
	u32 last_irq_exit;

	char last_sched_comm[TASK_COMM_LEN];

	u8 hotplug_data1;
	u8 hotplug_data2;
	u8 __pad[6];
};

#define RAM_CONSOLE_SIG (('R' << 0) | ('A' << 8) | ('M' << 16) | ('C' << 24))
/* increment this every time 'struct ram_console_buffer' layout is changed */
#define RAM_CONSOLE_LAYOUT_VER (0x01000001)
/* keep in sync with preloader: preloader updates hw_status field */

struct ram_console_buffer {
	/* validating all these 3 fields to ensure integrity */
	u32 sig;		/* RAM_CONSOLE_SIG */
	u32 ver;		/* RAM_CONSOLE_LAYOUT_VER */
	u32 mem_size;	/* CONFIG_MTK_RAM_CONSOLE_DRAM_SIZE */

	u8 hw_status;
	u8 fiq_step;
	u8 reboot_mode;
	u8 __pad[1];

	struct ram_buffer con;
	void *kparams;

	struct ram_cpu_info cpu[RC_CPU_COUNT];
};

static int mtk_cpu_num;
static char *ram_console_old_log_init_buffer;

static struct ram_console_buffer ram_console_old_header;
static char *ram_console_old_log;
static size_t ram_console_old_log_size;

static struct ram_console_buffer *ram_console_buffer;

static DEFINE_SPINLOCK(ram_console_lock);

static atomic_t rc_in_fiq = ATOMIC_INIT(0);

void aee_rr_rec_reboot_mode(u8 mode)
{
	if (ram_console_buffer) {
		ram_console_buffer->reboot_mode = mode;
	}
}

void aee_rr_rec_kdump_params(void *params)
{
	if (ram_console_buffer) {
		ram_console_buffer->kparams = params;
	}
}

void aee_rr_rec_fiq_step(u8 i)
{
	if (ram_console_buffer) {
		ram_console_buffer->fiq_step = i;
	}
}

void aee_rr_rec_last_irq_enter(int cpu, int irq, u64 j)
{
	if ((ram_console_buffer != NULL) && (cpu >= 0) && (cpu < RC_CPU_COUNT)) {
		ram_console_buffer->cpu[cpu].last_irq_enter = irq;
		ram_console_buffer->cpu[cpu].jiffies_last_irq_enter = j;
	}
	mb();
}

void aee_rr_rec_last_irq_exit(int cpu, int irq, u64 j)
{
	if ((ram_console_buffer != NULL) && (cpu >= 0) && (cpu < RC_CPU_COUNT)) {
		ram_console_buffer->cpu[cpu].last_irq_exit = irq;
		ram_console_buffer->cpu[cpu].jiffies_last_irq_exit = j;
	}
	mb();
}

void aee_rr_rec_last_sched_jiffies(int cpu, u64 j, const char *comm)
{
	if ((ram_console_buffer != NULL) && (cpu >= 0) && (cpu < RC_CPU_COUNT)) {
		ram_console_buffer->cpu[cpu].jiffies_last_sched = j;
		strlcpy(ram_console_buffer->cpu[cpu].last_sched_comm, comm, TASK_COMM_LEN);
	}
	mb();
}

void aee_rr_rec_hoplug(int cpu, u8 data1, u8 data2)
{
	if ((ram_console_buffer != NULL) && (cpu >= 0) && (cpu < RC_CPU_COUNT)) {
		ram_console_buffer->cpu[cpu].hotplug_data1 = data1;
		ram_console_buffer->cpu[cpu].hotplug_data2 = data2;
	}
}

void sram_log_save(const char *msg, int count)
{
	struct ram_buffer *con;
	char *wptr;
	size_t con_buf_size;
	size_t total_count;

	if (!ram_console_buffer)
		return;

	con = &ram_console_buffer->con;

	con_buf_size = con->end - con->start;
	total_count = con->count + count;
	wptr = con->wptr;

	if (total_count > con_buf_size)
		total_count = con_buf_size;

	while (count) {
		int seg_len = con->end - wptr;
		if (seg_len > count)
			seg_len = count;
		memcpy(wptr, msg, seg_len);
		wptr += seg_len;
		msg += seg_len;
		if (wptr == con->end)
			wptr = con->start;
		count -= seg_len;
	}

	con->wptr = wptr;
	con->count = total_count;
}

static inline void aee_sram_fiq_lock(void)
{
	int delay = 100;
	atomic_cmpxchg(&rc_in_fiq, 0, 1);
	while (delay > 0 && spin_is_locked(&ram_console_lock)) {
		udelay(1);
		delay--;
	}
}

static inline void aee_sram_fiq_unlock(void)
{
	atomic_cmpxchg(&rc_in_fiq, 1, 0);
}

void aee_sram_fiq_save_bin(const char *msg, size_t len)
{
	int i;

	aee_sram_fiq_lock();
	sram_log_save("\n", 1);
	len >>= 2;
	for (i = 0; i < len; ++i, msg += 4) {
		char hex_buffer[10];
		int size = snprintf(hex_buffer, sizeof(hex_buffer), "%02X%02X%02X%02X%c",
			msg[0], msg[1], msg[2], msg[3],
			((i&0xF) == 0xF || i == len-1) ? '\n' : ' ');
		sram_log_save(hex_buffer, size);
	}
	aee_sram_fiq_unlock();
}

void aee_disable_ram_console_write(void)
{
	atomic_set(&rc_in_fiq, 1);
}

void aee_sram_fiq_log(const char *msg)
{
	size_t count = strlen(msg);

	aee_sram_fiq_lock();
	sram_log_save(msg, count);
	aee_sram_fiq_unlock();
}

void ram_console_write(struct console *console, const char *s, unsigned int count)
{
	unsigned long flags;

#ifndef CONFIG_MTK_RAM_CONSOLE_USING_DRAM
	/* When dumping on SRAM, don't dump all data */
	if (atomic_read(&rc_in_fiq))
		return;
#endif

	spin_lock_irqsave(&ram_console_lock, flags);
	sram_log_save(s, count);
	spin_unlock_irqrestore(&ram_console_lock, flags);
}

static struct console ram_console = {
	.name = "ram",
	.write = ram_console_write,
	.flags = CON_PRINTBUFFER | CON_ENABLED | CON_ANYTIME | CON_RAMCONSOLE,
	.index = -1,
};

void ram_console_enable_console(int enabled)
{
	if (enabled)
		ram_console.flags |= CON_ENABLED;
	else
		ram_console.flags &= ~CON_ENABLED;
}

static int __init ram_console_save_old(struct ram_buffer *con)
{
	char *rptr = con->wptr - con->count;
	int seg1_len = 0, seg2_len = 0, total_len = con->count;

	if (rptr < con->start)
		rptr += con->end - con->start;

	ram_console_old_log_init_buffer = kmalloc(total_len + 1, GFP_KERNEL);
	if (!ram_console_old_log_init_buffer) {
		pr_err("ram_console: failed to allocate old buffer: toal_len=0x%08X\n",
			total_len + 1);
		return -ENOMEM;
	}

	seg1_len = con->end - rptr;

	if (seg1_len > total_len)
		seg1_len = total_len;
	else
		seg2_len = total_len - seg1_len;

	memcpy(ram_console_old_log_init_buffer, rptr, seg1_len);
	if (seg2_len)
		memcpy(ram_console_old_log_init_buffer + seg1_len, con->start, seg2_len);

	ram_console_old_log_init_buffer[total_len] = 0;

	ram_console_old_log_size = total_len;
	ram_console_old_log = ram_console_old_log_init_buffer;
	return 0;
}

static int __init ram_console_init(struct ram_console_buffer *buffer, size_t buffer_size)
{
	if (buffer->sig == RAM_CONSOLE_SIG &&
		buffer->ver == RAM_CONSOLE_LAYOUT_VER &&
		buffer->mem_size == buffer_size) {
		struct ram_buffer con = buffer->con;
		pr_err("%s: found existing buffer at %p [%08X]; hw_status=%d\n",
			__func__, buffer, buffer->mem_size, buffer->hw_status);
		pr_err("%s: buffer: start=%p; end=%p; wptr=%p; count=0x%08X\n",
			__func__, con.start, con.end, con.wptr, con.count);
		if ((con.start != (char *)(buffer + 1)) ||
			(con.end != ((char *)buffer) + buffer_size)) {
			pr_err("%s: buffer limits damaged. Resetting\n", __func__);
			con.start = (char *)(buffer + 1);
			con.end = ((char *)buffer) + buffer_size;
		}
		if (con.count > (con.end - con.start)) {
			pr_err("%s: buffer data count damaged. Resetting\n", __func__);
			con.count = con.end - con.start;
		}
		if (con.wptr < con.start ||
			con.wptr >= con.end) {
			pr_err("%s: buffer data pointer damaged. Resetting\n", __func__);
			con.wptr = con.start;
			con.count = con.end - con.start;
		}
		memcpy(&ram_console_old_header, buffer, sizeof(struct ram_console_buffer));
		ram_console_save_old(&con);
	} else
		pr_err("%s: not found at %p\n", __func__, buffer);

	memset(buffer, 0, buffer_size);
	buffer->sig = RAM_CONSOLE_SIG;
	buffer->ver = RAM_CONSOLE_LAYOUT_VER;
	buffer->mem_size = buffer_size;

	buffer->con.start = (char *)(buffer + 1);
	buffer->con.end = ((char *)buffer) + buffer_size;
	buffer->con.wptr = buffer->con.start;
	ram_console_buffer = buffer;

	register_console(&ram_console);

	pr_notice("%s: sig=%08X; ver=%08X; mem_size=%08X\n",
		__func__, buffer->sig, buffer->ver, buffer->mem_size);

	return 0;
}

#if defined(CONFIG_MTK_RAM_CONSOLE_USING_DRAM)
static void *remap_lowmem(phys_addr_t start, phys_addr_t size)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

	prot = pgprot_noncached(PAGE_KERNEL);

	pages = kmalloc(sizeof(struct page *) * page_count, GFP_KERNEL);
	if (!pages) {
		pr_err("%s: Failed to allocate array for %u pages\n", __func__, page_count);
		return NULL;
	}

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);
	if (!vaddr) {
		pr_err("%s: Failed to map %u pages\n", __func__, page_count);
		return NULL;
	}

	return vaddr + offset_in_page(start);
}
#endif

static int __init ram_console_early_init(void)
{
	struct ram_console_buffer *bufp = NULL;
	size_t buffer_size = 0;
	pr_err("%s: enter\n", __func__);

#if defined(CONFIG_MTK_RAM_CONSOLE_USING_SRAM)
	bufp = (struct ram_console_buffer *)CONFIG_MTK_RAM_CONSOLE_ADDR;
	buffer_size = CONFIG_MTK_RAM_CONSOLE_SIZE;
#elif defined(CONFIG_MTK_RAM_CONSOLE_USING_DRAM)

	bufp = remap_lowmem(CONFIG_MTK_RAM_CONSOLE_DRAM_ADDR, CONFIG_MTK_RAM_CONSOLE_DRAM_SIZE);
	if (bufp == NULL) {
		pr_err("ioremap failed, no ram console available\n");
		return 0;
	}
	buffer_size = CONFIG_MTK_RAM_CONSOLE_DRAM_SIZE;
#else
	return 0;
#endif

	pr_err("%s: start: %p, size: %d\n", __func__, bufp, buffer_size);
	mtk_cpu_num = num_present_cpus();
	return ram_console_init(bufp, buffer_size);
}

static int ram_console_show(struct seq_file *m, void *v)
{
	if (ram_console_old_log)
		seq_write(m, ram_console_old_log, ram_console_old_log_size);
	return 0;
}

static int ram_console_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, ram_console_show, inode->i_private);
}

static const struct file_operations ram_console_file_ops = {
	.owner = THIS_MODULE,
	.open = ram_console_file_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init ram_console_late_init(void)
{
	struct proc_dir_entry *entry;
	struct last_reboot_reason lrr;
	char *ram_console_header_buffer;
	int str_real_len = 0;
	int i = 0;

	if (ram_console_old_log == NULL) {
		pr_err("ram console old log is null!\n");
		return 0;
	}

	memset(&lrr, 0, sizeof(struct last_reboot_reason));
	lrr.wdt_status = ram_console_old_header.hw_status;
	lrr.fiq_step = ram_console_old_header.fiq_step;
	lrr.reboot_mode = ram_console_old_header.reboot_mode;

	for (i = 0; i < RC_CPU_COUNT; i++) {
		lrr.last_irq_enter[i] = ram_console_old_header.cpu[i].last_irq_enter;
		lrr.jiffies_last_irq_enter[i] = ram_console_old_header.cpu[i].jiffies_last_irq_enter;

		lrr.last_irq_exit[i] = ram_console_old_header.cpu[i].last_irq_exit;
		lrr.jiffies_last_irq_exit[i] = ram_console_old_header.cpu[i].jiffies_last_irq_exit;

		lrr.jiffies_last_sched[i] = ram_console_old_header.cpu[i].jiffies_last_sched;
		strlcpy(lrr.last_sched_comm[i], ram_console_old_header.cpu[i].last_sched_comm,
			TASK_COMM_LEN);

		lrr.hotplug_data1[i] = ram_console_old_header.cpu[i].hotplug_data1;
		lrr.hotplug_data2[i] = ram_console_old_header.cpu[i].hotplug_data2;
	}

	aee_rr_last(&lrr);

	ram_console_header_buffer = kmalloc(RAM_CONSOLE_HEADER_STR_LEN, GFP_KERNEL);
	if (ram_console_header_buffer == NULL) {
		pr_err("ram_console: failed to allocate buffer for header buffer.\n");
		return 0;
	}

	str_real_len =
		snprintf(ram_console_header_buffer, RAM_CONSOLE_HEADER_STR_LEN,
			"ram console header, hw_status: %u, fiq step %u\n",
			ram_console_old_header.hw_status, ram_console_old_header.fiq_step);

	ram_console_old_log = kmalloc(ram_console_old_log_size + str_real_len, GFP_KERNEL);
	if (ram_console_old_log == NULL) {
		pr_err("ram_console: failed to allocate buffer for old log\n");
		ram_console_old_log_size = 0;
		kfree(ram_console_header_buffer);
		return 0;
	}
	memcpy(ram_console_old_log, ram_console_header_buffer, str_real_len);
	memcpy(ram_console_old_log + str_real_len,
	       ram_console_old_log_init_buffer, ram_console_old_log_size);

	kfree(ram_console_header_buffer);
	kfree(ram_console_old_log_init_buffer);

	entry = proc_create("last_kmsg", 0444, NULL, &ram_console_file_ops);
	if (!entry) {
		pr_err("ram_console: failed to create proc entry\n");
		kfree(ram_console_old_log);
		ram_console_old_log = NULL;
		return 0;
	}

	ram_console_old_log_size += str_real_len;
	return 0;
}


console_initcall(ram_console_early_init);
late_initcall(ram_console_late_init);
