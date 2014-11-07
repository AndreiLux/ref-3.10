static inline void odin_crash_setup_regs(struct pt_regs *newregs)
{
	__asm__ __volatile__ (
		"stmia	%[regs_base], {r0-r12}\n\t"
		"mov	%[_ARM_sp], sp\n\t"
		"str	lr, %[_ARM_lr]\n\t"
		"adr	%[_ARM_pc], 1f\n\t"
		"mrs	%[_ARM_cpsr], cpsr\n\t"
	"1:"
		: [_ARM_pc] "=r" (newregs->ARM_pc),
		  [_ARM_cpsr] "=r" (newregs->ARM_cpsr),
		  [_ARM_sp] "=r" (newregs->ARM_sp),
		  [_ARM_lr] "=o" (newregs->ARM_lr)
		: [regs_base] "r" (&newregs->ARM_r0)
		: "memory"
	);
}

extern void odin_mmu_ureg_save(int target_cpu, struct pt_regs *regs);
extern void odin_mmu_ureg_backup(void);
extern void odin_panic_save_core(void);

#ifdef CONFIG_KEXEC
extern void crash_kexec_odin(struct pt_regs *regs);
#endif
