#include <linux/module.h>
#include <linux/io.h>
#include <linux/moduleparam.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <asm/cacheflush.h>
#include <linux/kdebug.h>
#include <linux/odin_panic.h>
#include <linux/kexec.h>

#define PER_CORE_REPO 512
#define REPOSITORY_BASE 0xDCFFE000

static int panic_status = 0;
static struct kobject *odin_panic_kobj;
static int odin_mmu_ureg_saved[NR_CPUS] = {0,};
static void *repository;
static void *gic_dist;

#pragma pack(1)

struct odin_reg_dump {
	uint32_t r0;
	uint32_t r1;
	uint32_t r2;
	uint32_t r3;
	uint32_t r4;
	uint32_t r5;
	uint32_t r6;
	uint32_t r7;
	uint32_t r8;
	uint32_t r9;
	uint32_t r10;
	uint32_t fp;
	uint32_t ip;
	uint32_t sp;
	uint32_t cpsr;
	uint32_t lr;
	uint32_t pc;
};

struct odin_mmu_dump {
	uint32_t sctlr;
	uint64_t ttbr0;
	uint64_t ttbr1;
	uint32_t ttbcr;
	uint32_t dacr;
	uint32_t dfsr;
	uint32_t dfar;
	uint32_t ifsr;
	uint32_t ifar;
	uint32_t prrr;
	uint32_t nmrr;
};

struct odin_backup {
	uint32_t cpsr;
	uint32_t lr;
	uint32_t pc;
};

struct odin_timer_gic {
	uint64_t counter;
	uint32_t tval;
	uint32_t tcon;
	uint32_t gic_check;
	uint32_t gic_pending;
	uint32_t gic_active;
};

static uint32_t read_var(void *addr)
{
	uint32_t var;
	var = *((uint32_t *)addr);

	return var;
}

static void write_var(void *addr, uint32_t var)
{
	*(uint32_t *)addr = var;
}

static void odin_core_reg_save(struct odin_reg_dump *reg, struct pt_regs *regs)
{
	reg->r0 = regs->ARM_r0;
	reg->r1 = regs->ARM_r1;
	reg->r2 = regs->ARM_r2;
	reg->r3 = regs->ARM_r3;
	reg->r4 = regs->ARM_r4;
	reg->r5 = regs->ARM_r5;
	reg->r6 = regs->ARM_r6;
	reg->r7 = regs->ARM_r7;
	reg->r8 = regs->ARM_r8;
	reg->r9 = regs->ARM_r9;
	reg->r10 = regs->ARM_r10;
	reg->fp = regs->ARM_fp;
	reg->ip = regs->ARM_ip;
	reg->sp = regs->ARM_sp;
	reg->cpsr = regs->ARM_cpsr;
	reg->lr = regs->ARM_lr;
	reg->pc = regs->ARM_pc;
}

static void odin_mmu_reg_save(struct odin_mmu_dump *mmu) {
	asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (mmu->sctlr));
	asm volatile("mrrc p15, 0, %Q0, %R0, c2" : "=r" (mmu->ttbr0));
	asm volatile("mrrc p15, 1, %Q0, %R0, c2" : "=r" (mmu->ttbr1));
	asm volatile("mrc p15, 0, %0, c2, c0, 2" : "=r" (mmu->ttbcr));
	asm volatile("mrc p15, 0, %0, c3, c0, 0" : "=r" (mmu->dacr));
	asm volatile("mrc p15, 0, %0, c5, c0, 0" : "=r" (mmu->dfsr));
	asm volatile("mrc p15, 0, %0, c6, c0, 0" : "=r" (mmu->dfar));
	asm volatile("mrc p15, 0, %0, c5, c0, 1" : "=r" (mmu->ifsr));
	asm volatile("mrc p15, 0, %0, c6, c0, 2" : "=r" (mmu->ifar));
	asm volatile("mrc p15, 0, %0, c10, c2, 0" : "=r" (mmu->prrr));
	asm volatile("mrc p15, 0, %0, c10, c2, 1" : "=r" (mmu->nmrr));
}

struct odin_reg_dump reg_dump[NR_CPUS];
struct odin_mmu_dump mmu_dump[NR_CPUS];

struct odin_reg_dump reg_backup[NR_CPUS];
struct odin_mmu_dump mmu_backup[NR_CPUS];

void odin_mmu_ureg_backup(void)
{
	struct odin_reg_dump *reg;
	struct odin_mmu_dump *mmu;
	struct odin_backup *backup;
	int cpu;

	if (!repository)
		return;

	for (cpu=0 ; cpu<NR_CPUS ; cpu++) {
		reg = (struct odin_reg_dump *)(repository + PER_CORE_REPO * cpu);
		mmu = (struct odin_mmu_dump *)(repository + PER_CORE_REPO * cpu
				+ sizeof(struct odin_reg_dump));

		memcpy(&reg_backup[cpu], reg, sizeof(reg_backup[cpu]));
		memcpy(&mmu_backup[cpu], mmu, sizeof(mmu_backup[cpu]));

		backup =(struct odin_backup *)(repository + PER_CORE_REPO * cpu
				+ sizeof(struct odin_reg_dump) + sizeof(struct odin_mmu_dump)
				+ sizeof(struct odin_timer_gic));
		backup->cpsr = reg_backup[cpu].cpsr;
		backup->lr = reg_backup[cpu].lr;
		backup->pc = reg_backup[cpu].pc;
	}
}

void odin_mmu_ureg_save(int target_cpu, struct pt_regs *regs)
{
	int this_cpu;
	struct odin_reg_dump *reg;
	struct odin_mmu_dump *mmu;
	struct odin_timer_gic *info;
	uint32_t *s_flag;

	this_cpu = smp_processor_id();	/* in irq_disabled */
	if (target_cpu != this_cpu)
		printk(KERN_CRIT "%s failed: Trying to dump mmu information   \
			for WRONG CPU(%d),not CPU%d!!!\n", __func__, this_cpu, target_cpu);

	if (!odin_mmu_ureg_saved[target_cpu] && repository) {
		/* Backup core general purpose register */
		reg = (struct odin_reg_dump *)(repository + PER_CORE_REPO * target_cpu);
		odin_core_reg_save(reg, regs);

		/* Backup core MMU register */
		mmu = (struct odin_mmu_dump *)(repository + PER_CORE_REPO * target_cpu
			+ sizeof(struct odin_reg_dump));
		odin_mmu_reg_save(mmu);

		s_flag = (uint32_t *)(repository + PER_CORE_REPO * (target_cpu + 1) - 4);
		write_var(s_flag, *s_flag & 0x0000FFFF);
		write_var(s_flag, *s_flag | 0x5A7E0000);
		odin_mmu_ureg_saved[target_cpu] = 1;
		flush_cache_all();

		info = (struct odin_timer_gic *)(repository + PER_CORE_REPO * target_cpu
			+ sizeof(struct odin_reg_dump) + sizeof(struct odin_mmu_dump));

		asm volatile("mrrc p15, 0, %Q0, %R0, c14" : "=r" (info->counter));
		asm volatile("mrc p15, 0, %0, c14, c2, 0" : "=r" (info->tval));
		asm volatile("mrc p15, 0, %0, c14, c2, 1" : "=r" (info->tcon));

		info->gic_check = 0x91c012f0;
		info->gic_pending = read_var(gic_dist + 0x200);
		info->gic_active = read_var(gic_dist + 0x300);
	}
#ifdef CONFIG_KEXEC
	crash_save_cpu(regs, this_cpu);
	flush_cache_all();
#endif
}

void odin_panic_save_core(void)
{
	struct pt_regs odin_regs;
	int cpu = smp_processor_id();
	odin_crash_setup_regs(&odin_regs);
	odin_mmu_ureg_save(cpu, &odin_regs);
}

#ifdef CONFIG_KEXEC
static DEFINE_MUTEX(odin_kexec_mutex);
void crash_kexec_odin(struct pt_regs *regs)
{
	struct pt_regs fixed_regs;

	if (mutex_trylock(&odin_kexec_mutex)) {
		crash_setup_regs(&fixed_regs, NULL);
		crash_save_vmcoreinfo();
		/* machine_crash_shutdown(&fixed_regs); */
		flush_cache_all();
		mutex_unlock(&odin_kexec_mutex);
	}
}
#endif

int odin_check_panic(void)
{
	return panic_status;
}
EXPORT_SYMBOL(odin_check_panic);

//#ifdef CONFIG_PSTORE_FTRACE
void odin_push_panic(void)
{
	panic_status = 1;
	flush_cache_all();
}
EXPORT_SYMBOL(odin_push_panic);
//#endif

static ssize_t odin_panic_gen_panic_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	panic("Generate kernel panic");
	return count;
}

static struct kobj_attribute odin_panic_gen_panic_attr =
	__ATTR(gen_panic, 0200, NULL, odin_panic_gen_panic_store);

static struct attribute *odin_panic_attrs[] = {
	&odin_panic_gen_panic_attr.attr,
	NULL,
};

static struct attribute_group odin_panic_attr_group = {
	.attrs = odin_panic_attrs,
};

static int odin_die_handler(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	flush_cache_all();
	return NOTIFY_DONE;
}

static struct notifier_block die_handler = {
	.notifier_call = odin_die_handler,
	.priority = INT_MAX,
};

static int odin_panic_handler(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	panic_status = 1;
	flush_cache_all();
	return NOTIFY_DONE;
}

static struct notifier_block panic_handler = {
	.notifier_call = odin_panic_handler,
	.priority = INT_MAX,
};

static int __init odin_panic_init(void)
{
    int ret = -1;

	if (!of_find_compatible_node(NULL, NULL, "lge,odin")) {
		pr_info("%s: No compatible node found\n", __func__);
		return -ENODEV;
	}

	repository = ioremap(REPOSITORY_BASE, SZ_4K);
	memset(repository, 0, SZ_4K);
	gic_dist = ioremap(0x39801000, SZ_4K);

	odin_panic_kobj = kobject_create_and_add("panic", kernel_kobj);
	if (!odin_panic_kobj)
		return -ENOMEM;

	ret = sysfs_create_group(odin_panic_kobj, &odin_panic_attr_group);
	if (ret)
		kobject_put(odin_panic_kobj);

	register_die_notifier(&die_handler);
	atomic_notifier_chain_register(&panic_notifier_list,
					&panic_handler);

    return ret;
}

early_initcall(odin_panic_init);
